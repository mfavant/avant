#include "app/http_app.h"
#include "server/server.h"
#include "workers/worker.h"
#include "utility/url.h"
#include "utility/mime_type.h"
#include "utility/singleton.h"
#include "app/lua_plugin.h"
#include <string>
#include <filesystem>
#include <avant-log/logger.h>
#include <vector>
#include <sys/stat.h>
#include "global/tunnel_id.h"
#include "proto/proto_util.h"
#include "zlib/zlib.h"

using namespace avant::app;
using std::string;
using std::vector;
namespace fs = std::filesystem;
namespace utility = avant::utility;

class avant_html_loader
{
public:
    static string load(string body)
    {
        static string frame1 = "<!DOCTYPE html>\
                        <html>\
                        <head>\
                        <title></title>\
                        </head>\
                        <body>";
        static string frame2 = "</body>\
                                </html>";
        return frame1 + body + frame2;
    }
    static string a_tag(string url, string text)
    {
        string frame = "<a href=\"" + url + "\">" + text + "</a></br>";
        return frame;
    }
};

struct avant_http_app_reponse
{
    enum type
    {
        DIR = 0,
        FD = 1,
        NONE = 2,
    };

    void *ptr{nullptr};
    type ptr_type{NONE};
    z_stream strm{};
    bool use_gzip{false};
    bool gzip_initialized{false};

    typedef std::tuple<std::string, size_t> DIR_TYPE;
    typedef FILE FD_TYPE;

    inline void destory()
    {
        if (gzip_initialized)
        {
            deflateEnd(&strm);
            gzip_initialized = false;
        }

        if (ptr && ptr_type == FD)
        {
            FD_TYPE *free_ptr = (FD_TYPE *)ptr;
            ::fclose(free_ptr);
            ptr = nullptr;
            return;
        }
        else if (ptr && ptr_type == DIR)
        {
            DIR_TYPE *free_ptr = (DIR_TYPE *)ptr;
            delete free_ptr;
            ptr = nullptr;
            return;
        }
        else if (ptr)
        {
            ::free(ptr);
            ptr = nullptr;
        }
    }
};

struct avant_http_range
{
    int64_t start;
    int64_t end;
};

bool avant_parse_range_header(const std::string &range_header,
                              std::vector<avant_http_range> &ranges)
{
    if (range_header.empty() || range_header.find("bytes=") != 0)
    {
        return false;
    }

    // skip "bytes="
    std::string range_str = range_header.substr(6);
    std::stringstream ss(range_str);
    std::string range_item;

    while (std::getline(ss, range_item, ','))
    {
        size_t dash_pos = range_item.find('-');
        if (dash_pos == std::string::npos)
        {
            return false;
        }
        std::string start_str = range_item.substr(0, dash_pos);
        std::string end_str = range_item.substr(dash_pos + 1);

        int64_t start, end;
        try
        {
            start = start_str.empty() ? 0 : std::stoll(start_str);
            end = end_str.empty() ? -1 : std::stoll(end_str);
        }
        catch (...)
        {
            return false;
        }

        ranges.push_back({start, end});
    }

    return !ranges.empty();
}

void http_app::on_new_connection(avant::connection::http_ctx &ctx, bool is_keep_alive_call)
{
    // send new_connection protocol to other thread
    if constexpr (false)
    {
        ProtoPackage package;
        ProtoTunnelWorker2OtherEventNewClientConnection protoNewConn;
        protoNewConn.set_gid(ctx.get_conn_gid());
        ctx.tunnel_forward(std::vector<int>{avant::global::tunnel_id::get().get_other_tunnel_id()},
                           avant::proto::pack_package(package, protoNewConn, ProtoCmd::PROTO_CMD_TUNNEL_WORKER2OTHER_EVENT_NEW_CLIENT_CONNECTION));
    }
    // LOG_DEBUG("http_app new socket gid {}", ctx.get_conn_gid());
}

void http_app::process_connection(avant::connection::http_ctx &ctx)
{
    // load callback
    ctx.destory_callback = [](avant::connection::http_ctx &ctx) -> void
    {
        if (ctx.ptr)
        {
            avant_http_app_reponse *reponse_ptr = (avant_http_app_reponse *)ctx.ptr;
            reponse_ptr->destory();

            // LOG_DEBUG("reponse_ptr->destory() conngid {}", ctx.get_conn_gid());
            delete reponse_ptr;
            ctx.ptr = nullptr;
        }
    };

    ctx.process_callback = [](avant::connection::http_ctx &ctx) -> void
    {
        static auto return_404 = [](avant::connection::http_ctx &ctx) -> void
        {
            std::string response = "HTTP/1.1 404 Not Found\r\nServer: avant\r\n";
            response += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
            response += "Content-Type: text/plain; charset=UTF-8\r\n";
            response += "Content-Length: 3\r\n";
            response += "\r\n";
            response += "404";
            ctx.send_buffer_append(response.c_str(), response.size());
            ctx.set_response_end(true);
        };

        static auto return_500 = [](avant::connection::http_ctx &ctx) -> void
        {
            std::string response = "HTTP/1.1 500 Internal Server Error\r\nServer: avant\r\n";
            response += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
            response += "Content-Type: text/plain; charset=UTF-8\r\n";
            response += "Content-Length: 3\r\n";
            response += "\r\n";
            response += "500";
            ctx.send_buffer_append(response.c_str(), response.size());
            ctx.set_response_end(true);
        };

        // Header: Connection
        bool header_exist_keep_live = false;
        {
            if (ctx.headers.find("Connection") != ctx.headers.end())
            {
                for (const std::string &str : ctx.headers.find("Connection")->second)
                {
                    if (str == "keep-alive")
                    {
                        header_exist_keep_live = true;
                        break;
                    }
                }
            }
        }
        // Header: If-None-Match
        std::string header_if_none_match;
        {
            auto if_none_match = ctx.headers.find("If-None-Match");
            if (if_none_match != ctx.headers.end())
            {
                if (if_none_match->second.size() == 1)
                {
                    header_if_none_match = if_none_match->second.at(0);
                }
            }
        }
        // Header: If-Modified-Since
        std::string header_if_modified_since;
        {
            auto if_modified_since = ctx.headers.find("If-Modified-Since");
            if (if_modified_since != ctx.headers.end())
            {
                if (if_modified_since->second.size() == 1)
                {
                    header_if_modified_since = if_modified_since->second.at(0);
                }
            }
        }
        // Header: Cache-Control
        std::string header_cache_control;
        {
            auto cache_control = ctx.headers.find("Cache-Control");
            if (cache_control != ctx.headers.end())
            {
                if (cache_control->second.size() == 1)
                {
                    header_cache_control = cache_control->second.at(0);
                }
            }
        }
        // Header: Range
        std::string header_range;
        std::vector<avant_http_range> header_ranges;
        {
            auto iter_header_range = ctx.headers.find("Range");

            if (iter_header_range != ctx.headers.end() &&
                iter_header_range->second.size() == 1)
            {
                header_range = iter_header_range->second.at(0);
                // std::cout << "header_range: " << header_range << std::endl;

                if (!avant_parse_range_header(header_range,
                                              header_ranges))
                {
                    LOG_ERROR("!avant_parse_range_header failed");
                }
            }
        }
        // Header: If-Range
        std::string header_if_range;
        {
            auto iter_header_if_range = ctx.headers.find("If-Range");
            if (iter_header_if_range != ctx.headers.end() &&
                iter_header_if_range->second.size() == 1)
            {
                header_if_range = iter_header_if_range->second.at(0);
            }
        }
        // Header: Accept-Encoding
        bool client_support_gzip = false;
        {
            auto accept_encoding = ctx.headers.find("Accept-Encoding");
            if (accept_encoding != ctx.headers.end())
            {
                for (const std::string &encoding : accept_encoding->second)
                {
                    if (encoding.find("gzip") != std::string::npos)
                    {
                        client_support_gzip = true;
                        break;
                    }
                }
            }
        }

        if constexpr (false)
        {
            ctx.keep_alive = header_exist_keep_live;
            const char *response = "HTTP/1.1 200 OK\r\nServer: avant\r\nConnection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Length: 5\r\n\r\nHELLO";
            ctx.send_buffer_append(response, strlen(response));
            ctx.set_response_end(true);
            return;
        }
        else
        {
            ctx.keep_alive = header_exist_keep_live; // app not use keep_alive
        }

        std::string url;
        if (!utility::url::unescape_path(ctx.url, url))
        {
            LOG_ERROR("url::unescape_path false {}", ctx.url.c_str());
            return_500(ctx);
            ctx.set_response_end(true);
            return;
        }

        // LOG_DEBUG("HttpUrl {}", url.c_str());

        auto find_res = url.find("..");
        if (std::string::npos != find_res)
        {
            LOG_ERROR("exist .. in url");
            return_404(ctx);
            ctx.set_response_end(true);
            return;
        }

        try
        {
            utility::url url_obj(url);
            url = url_obj.get_path();
        }
        catch (std::runtime_error &e)
        {
            LOG_ERROR("utility::url url_obj(url) error {}", e.what());
            return_500(ctx);
            ctx.set_response_end(true);
            return;
        }

        if (url == "" || url == "/")
        {
            url = "/index.html";
        }

        const string &prefix = ctx.get_worker_ptr()->get_server()->get_config().get_http_static_dir();

        fs::path t_path = prefix + url;

        if (fs::exists(t_path) && fs::is_regular_file(t_path))
        {
            auto generate_etag_for_regular_file = [](const fs::path &t_path, std::string &out_etag, std::string &out_http_date) -> bool
            {
                struct stat st;
                if (stat(t_path.c_str(), &st) != 0)
                {
                    return false;
                }
                {
                    std::ostringstream oss;
                    oss << "\"" << std::hex << st.st_mtime << "-" << st.st_size << "\"";
                    out_etag = oss.str();
                }
                {
                    std::stringstream ss;
                    ss << std::put_time(std::gmtime(&st.st_mtim.tv_sec), "%a, %d %b %Y %H:%M:%S GMT");
                    out_http_date = ss.str();
                }

                return true;
            };

            std::string now_etag, now_last_modify_date;
            if (!generate_etag_for_regular_file(t_path, now_etag, now_last_modify_date))
            {
                LOG_ERROR("generate_etag_for_regular_file failed {}", t_path.c_str());
            }

            // std::cout << "now_etag: " << now_etag << std::endl;
            // std::cout << "now_last_modify_date: " << now_last_modify_date << std::endl;

            // std::cout << "header_if_range: " << header_if_range << std::endl;
            for (const auto &range_item : header_ranges)
            {
                // std::cout << "range_item: " << range_item.start << "-" << range_item.end << std::endl;
            }

            // 命中缓存
            if (header_cache_control != "no-cache" && ((now_etag.size() > 0 &&
                                                        header_if_none_match.size() > 0 &&
                                                        now_etag == header_if_none_match) ||
                                                       (now_last_modify_date.size() > 0 &&
                                                        header_if_modified_since.size() > 0 &&
                                                        now_last_modify_date == header_if_modified_since)))
            {
                std::string response = "HTTP/1.1 304 Not Modified\r\nServer: avant\r\n";
                response += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
                response += "Content-Length: 0\r\n";
                response += "\r\n";
                ctx.send_buffer_append(response.c_str(), response.size());
                ctx.set_response_end(true);
                return;
            }

            auto response_ptr = new (std::nothrow) avant_http_app_reponse;
            if (!response_ptr)
            {
                LOG_ERROR("new (std::nothrow) avant_http_app_reponse failed");
                return_500(ctx);
                ctx.set_response_end(true);
                return;
            }

            ctx.ptr = response_ptr;
            response_ptr->ptr_type = avant_http_app_reponse::FD;

            std::string mime_type;
            try
            {
                mime_type = utility::mime_type::get_type(t_path.string());
            }
            catch (...)
            {
                mime_type = "application/octet-stream";
            }

            // checking mine_type for use_gzip, default using use_gzip
            {
                response_ptr->use_gzip = client_support_gzip &&
                                         (mime_type.find("text/") == 0 ||
                                          mime_type.find("application/javascript") == 0 ||
                                          mime_type.find("application/json") == 0 ||
                                          mime_type.find("application/xml") == 0 ||
                                          mime_type.find("image/") == 0);

                if (response_ptr->use_gzip && !response_ptr->gzip_initialized)
                {
                    response_ptr->strm = {};
                    response_ptr->strm.zalloc = Z_NULL;
                    response_ptr->strm.zfree = Z_NULL;
                    response_ptr->strm.opaque = Z_NULL;

                    // using gzip format (windowBits = 15 + 16)
                    int ret = deflateInit2(&response_ptr->strm,
                                           Z_DEFAULT_COMPRESSION,
                                           Z_DEFLATED,
                                           15 + 16, // gzip 格式
                                           8,
                                           Z_DEFAULT_STRATEGY);

                    if (ret != Z_OK)
                    {
                        LOG_ERROR("deflateInit2 failed: {}", ret);
                        delete response_ptr;
                        return_500(ctx);
                        ctx.set_response_end(true);
                        return;
                    }
                    response_ptr->gzip_initialized = true;
                }
            }

            response_ptr->ptr = ::fopen(t_path.c_str(), "r");
            if (response_ptr->ptr == NULL)
            {
                LOG_ERROR("fopen({}, r) failed", t_path.c_str());
                return_500(ctx);
                ctx.set_response_end(true);
                return;
            }

            ::fseek((FILE *)response_ptr->ptr, 0, SEEK_SET);

            std::string response_head = "HTTP/1.1 200 OK\r\nServer: avant\r\n";
            response_head += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
            if (now_etag.size() > 0)
            {
                response_head += std::string("ETag: ") + now_etag + "\r\n";
            }
            if (now_last_modify_date.size() > 0)
            {
                response_head += std::string("Last-Modified: ") + now_last_modify_date + "\r\n";
            }
            response_head += "Content-Type: " + mime_type + "\r\n";
            if (response_ptr->use_gzip)
            {
                response_head += "Content-Encoding: gzip\r\n";
            }
            response_head += "Transfer-Encoding: chunked\r\n\r\n";

            ctx.send_buffer_append(response_head.c_str(), response_head.size());

            // Write when the contents of the buffer have been sent write_end_callback will be executed,
            // and the response must be set response_end to true, then write after write_end_callback will be continuously recalled
            ctx.write_end_callback = [](connection::http_ctx &ctx) -> void
            {
                constexpr int buffer_size = 1024000; // 1000KB
                char buf[buffer_size] = {0};
                constexpr int compress_buffer_size = 2 * buffer_size; // 2000KB
                char compress_buf[compress_buffer_size] = {0};

                avant_http_app_reponse *resp = (avant_http_app_reponse *)ctx.ptr;

                int len = ::fread(buf,
                                  sizeof(char),
                                  buffer_size,
                                  (avant_http_app_reponse::FD_TYPE *)((avant_http_app_reponse *)ctx.ptr)->ptr);

                if (len > 0)
                {
                    if (resp->use_gzip)
                    {
                        // using gzip
                        resp->strm.avail_in = len;
                        resp->strm.next_in = (Bytef *)buf;

                        resp->strm.avail_out = compress_buffer_size;
                        resp->strm.next_out = (Bytef *)compress_buf;

                        int ret = deflate(&resp->strm, Z_SYNC_FLUSH);
                        if (ret == Z_STREAM_ERROR)
                        {
                            LOG_ERROR("deflate failed with Z_STREAM_ERROR");
                            ctx.set_response_end(true);
                            return;
                        }
                        int exit_compressed_data_len = compress_buffer_size - resp->strm.avail_out;
                        if (exit_compressed_data_len > 0)
                        {
                            std::stringstream ss;
                            ss << std::hex << exit_compressed_data_len;
                            std::string chunk_size = ss.str() + "\r\n";

                            ctx.send_buffer_append(chunk_size.c_str(), chunk_size.size());
                            ctx.send_buffer_append(compress_buf, exit_compressed_data_len);
                            ctx.send_buffer_append("\r\n", 2);
                        }
                    }
                    else
                    {
                        std::stringstream ss;
                        ss << std::hex << len; // convert to hex
                        std::string chunk_size = ss.str() + "\r\n";

                        ctx.send_buffer_append(chunk_size.c_str(), chunk_size.size());
                        ctx.send_buffer_append(buf, len);
                        ctx.send_buffer_append("\r\n", 2);
                    }
                }
                else
                {
                    ctx.send_buffer_append("0\r\n\r\n", 5);
                    ctx.set_response_end(true);
                }
            };
            ctx.write_end_callback(ctx);

            return;
        }
        else if (fs::exists(t_path) && fs::is_directory(t_path))
        {
            auto response_ptr = new (std::nothrow) avant_http_app_reponse;
            if (!response_ptr)
            {
                LOG_ERROR("new (std::nothrow) avant_http_app_reponse failed");
                return_404(ctx);
                ctx.set_response_end(true);
                return;
            }
            ctx.ptr = response_ptr;
            response_ptr->ptr_type = avant_http_app_reponse::DIR;
            response_ptr->ptr = new (std::nothrow) avant_http_app_reponse::DIR_TYPE;
            if (!response_ptr->ptr)
            {
                delete response_ptr;
                return_404(ctx);
                ctx.set_response_end(true);
                return;
            }
            auto dir_type_ptr = (avant_http_app_reponse::DIR_TYPE *)response_ptr->ptr;

            //  generate dir list
            vector<string> a_tags;
            try
            {
                for (const auto &dir_entry : fs::directory_iterator(t_path))
                {
                    std::string sub_path = dir_entry.path().string().substr(prefix.size());
                    a_tags.push_back(avant_html_loader::a_tag(sub_path, sub_path));
                }
            }
            catch (const std::filesystem::filesystem_error &ex)
            {
                LOG_ERROR("{}", ex.what());
            }

            string body;
            for (const auto &a_tag : a_tags)
            {
                body += a_tag;
            }
            std::get<0>(*dir_type_ptr) = avant_html_loader::load(body);
            std::get<1>(*dir_type_ptr) = 0;

            std::string response_head = "HTTP/1.1 200 OK\r\nServer: avant\r\n";
            response_head += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
            response_head += "Content-Type: text/html; charset=UTF-8\r\n";
            response_head += "Content-Length: " + std::to_string(std::get<0>(*dir_type_ptr).size());
            response_head += "\r\n\r\n";

            ctx.send_buffer_append(response_head.c_str(), response_head.size());

            ctx.write_end_callback = [header_exist_keep_live](connection::http_ctx &ctx) -> void
            {
                avant_http_app_reponse *response_ptr = (avant_http_app_reponse *)ctx.ptr;
                auto dir_type_ptr = (avant_http_app_reponse::DIR_TYPE *)response_ptr->ptr;
                const char *buffer_ptr = std::get<0>(*dir_type_ptr).c_str();
                const size_t buffer_size = std::get<0>(*dir_type_ptr).size();
                size_t &already_size = std::get<1>(*dir_type_ptr);

                if (buffer_size > already_size)
                {
                    size_t need_send = buffer_size - already_size;
                    need_send = need_send > 1024000 ? 1024000 : need_send;
                    ctx.send_buffer_append(buffer_ptr, need_send);
                    already_size += need_send;
                }
                else
                {
                    ctx.set_response_end(true);
                }
            };

            ctx.write_end_callback(ctx);
            return;
        }
        else
        {
            return_404(ctx);
            ctx.set_response_end(true);
            return;
        }
    };
}

int http_app::on_body_before(avant::connection::http_ctx &ctx, const char *at, size_t length)
{
    constexpr size_t max_body_size = 2048000;
    size_t body_size = ctx.get_recv_body_size();

    if (body_size + length > max_body_size)
    {
        LOG_ERROR("body_size[{}] + length[{}] > max_body_size[{}]", body_size, length, max_body_size);
        return -1;
    }

    return 0;
}

int http_app::on_body(avant::connection::http_ctx &ctx, size_t length)
{
    constexpr size_t max_body_size = 2048000;

    size_t recv_buffer_size = ctx.get_recv_buffer_size();
    size_t body_size = ctx.get_recv_body_size();

    // processing http request body data
    {
        ctx.clear_recv_buffer();
    }

    if (body_size > max_body_size)
    {
        LOG_ERROR("recv_buffer_size {} length {} body_size {}", recv_buffer_size, length, body_size);
        LOG_ERROR("http_app::on_body body_size > {}", max_body_size);
        return -1;
    }

    return 0;
}

void http_app::on_main_init(avant::server::server &server_obj)
{
    LOG_ERROR("http_app::on_main_init");
    utility::singleton<lua_plugin>::instance()->on_main_init(server_obj.get_config().get_lua_dir(),
                                                             server_obj.get_config().get_worker_cnt());
}

void http_app::on_worker_init(avant::workers::worker &worker_obj)
{
    LOG_ERROR("http_app::on_worker_init {}", worker_obj.get_worker_idx());
    utility::singleton<lua_plugin>::instance()->on_worker_init(worker_obj.get_worker_idx());
}

void http_app::on_main_stop(avant::server::server &server_obj)
{
    LOG_ERROR("http_app::on_main_stop");
    utility::singleton<lua_plugin>::instance()->on_main_stop();
}

void http_app::on_worker_stop(avant::workers::worker &worker_obj)
{
    LOG_ERROR("http_app::on_worker_stop {}", worker_obj.get_worker_idx());
    utility::singleton<lua_plugin>::instance()->on_worker_stop(worker_obj.get_worker_idx());
}

void http_app::on_main_tick(avant::server::server &server_obj)
{
    utility::singleton<lua_plugin>::instance()->on_main_tick();
}

void http_app::on_worker_tick(avant::workers::worker &worker_obj)
{
    utility::singleton<lua_plugin>::instance()->on_worker_tick(worker_obj.get_worker_idx());
}

void http_app::on_worker_tunnel(avant::workers::worker &worker_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package)
{
    int cmd = package.cmd();
    if (cmd == ProtoCmd::PROTO_CMD_TUNNEL_OTHER2WORKER_TEST)
    {
        ProtoTunnelOther2WorkerTest other2worker_test;
        if (!proto::parse(other2worker_test, package))
        {
            LOG_ERROR("proto::parse(other2worker_test, package) failed");
            return;
        }
        // LOG_ERROR("worker_id {} PROTO_CMD_TUNNEL_OTHER2WORKER_TEST time {}", worker_obj.get_worker_idx(), other2worker_test.time());
    }
    else
    {
        LOG_ERROR("not exist handler {}", cmd);
    }
}

void http_app::on_cmd_reload(avant::server::server &server_obj)
{
    LOG_ERROR("http_app on_cmd_reload execute lua_plugin reload");
    utility::singleton<lua_plugin>::instance()->reload();
}
