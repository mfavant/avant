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

// 为了返回目录内容用代码拼接出 HTML
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

// 为 http_ctx 存储对话信息 以便每次 callback 能访问到交互状态
struct avant_http_app_reponse
{
    enum type
    {
        DIR = 0,  // 响应文件夹内容
        FD = 1,   // 响应普通文件内容
        NONE = 2, // 未知
    };

    void *ptr{nullptr};
    type ptr_type{NONE};

    // for compressor
    z_stream strm{};
    bool use_gzip{false};
    bool gzip_initialized{false};

    // for http range
    int64_t range_left{-1};

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

    // true when the start part is absent, i.e. the suffix form "-N"
    bool start_empty{false};
};

// 解析出 HTTP 请求头里的 Range 内容
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

        ranges.push_back({start, end, start_str.empty()});
    }

    return !ranges.empty();
}

// HTTP 当 socket 建立连接后，HTTPS 则是 TLS 握手就绪后 调用 on_new_connection
void http_app::on_new_connection(avant::connection::http_ctx &ctx, bool is_keep_alive_call)
{
    // send new_connection protocol to other thread
    if constexpr (false)
    {
        ProtoPackage package;
        ProtoTunnelWorker2OtherEventNewClientConnection protoNewConn;
        protoNewConn.set_gid(ctx.get_conn_gid());
        const auto ip_port = ctx.get_ip_port();
        if (!ip_port.first.empty())
        {
            protoNewConn.set_ip(ip_port.first);
            protoNewConn.set_port(ip_port.second);
        }
        ctx.tunnel_forward(std::vector<int>{avant::global::tunnel_id::get().get_other_tunnel_id()},
                           avant::proto::pack_package(package, protoNewConn, ProtoCmd::PROTO_CMD_TUNNEL_WORKER2OTHER_EVENT_NEW_CLIENT_CONNECTION));
    }
    // LOG_DEBUG("http_app new socket gid {}", ctx.get_conn_gid());
}

// 被调用的时机是 服务器接收完HTTP请求发送 Header Bod 后只调用 绑定毁掉函数
void http_app::process_connection(avant::connection::http_ctx &ctx)
{
    // 在处理完一个 HTTP 请求后调用
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

    // 接收完 HTTP Request 的 Header 与 Body 后 响应客户端前 调用 处理请求内容与响应内容逻辑
    ctx.process_callback = [](avant::connection::http_ctx &ctx) -> void
    {
        static auto return_404 = [](avant::connection::http_ctx &ctx) -> void
        {
            std::string response = "HTTP/1.1 404 Not Found\r\nServer: avant\r\n";
            if (ctx.keep_alive)
            {
                response += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
            }
            else
            {
                response += "Connection: close\r\n";
            }
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
            if (ctx.keep_alive)
            {
                response += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
            }
            else
            {
                response += "Connection: close\r\n";
            }
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

        if constexpr (false) // 这里是纯粹了HTTP压力测试写的
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

        // 非 GET 与 HEAD 请求 直接强制断开连接即使客户端
        if (ctx.method != "GET" && ctx.method != "HEAD")
        {
            std::string response = "HTTP/1.1 405 Method Not Allowed\r\nServer: avant\r\n";
            response += "Allow: GET, HEAD\r\n";
            if (ctx.keep_alive)
            {
                response += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
            }
            else
            {
                response += "Connection: close\r\n";
            }
            response += "Content-Type: text/plain; charset=UTF-8\r\n";
            response += "Content-Length: 3\r\n";
            response += "\r\n";
            response += "405";
            ctx.send_buffer_append(response.c_str(), response.size());
            ctx.set_response_end(true);
            return;
        }

        const bool is_http_head_method = (ctx.method == "HEAD");

        std::string url;
        if (!utility::url::unescape_path(ctx.url, url))
        {
            LOG_ERROR("url::unescape_path false {}", ctx.url.c_str());
            return_500(ctx);
            ctx.set_response_end(true);
            return;
        }

        if constexpr (false)
        {
            const auto ip_port = ctx.get_ip_port();
            LOG_DEBUG("HttpUrl {} ClientIPPort {} : {}", url.c_str(), ip_port.first.c_str(), ip_port.second);
        }

        // 防止有人攻击在 URL 里添加 ..
        auto find_res = url.find("..");
        if (std::string::npos != find_res)
        {
            LOG_ERROR("exist .. in url");
            return_404(ctx);
            ctx.set_response_end(true);
            return;
        }

        // URL 解析出 路径部分
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

        // 根默认访问 index.html
        if (url == "" || url == "/")
        {
            url = "/index.html";
        }

        const string &prefix = ctx.get_worker_ptr()->get_server()->get_config().get_http_static_dir();

        fs::path t_path = prefix + url;

        if (fs::exists(t_path) && fs::is_regular_file(t_path)) // 是普通文件且存在
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

                    ss << std::put_time(
                        std::gmtime(&st.st_mtime),
                        "%a, %d %b %Y %H:%M:%S GMT");

                    out_http_date = ss.str();
                }

                return true;
            };

            std::string now_etag, now_last_modify_date;
            if (!generate_etag_for_regular_file(t_path, now_etag, now_last_modify_date))
            {
                LOG_ERROR("generate_etag_for_regular_file failed {}", t_path.c_str());
            }

            struct stat st;
            if (stat(t_path.c_str(), &st) != 0)
            {
                LOG_ERROR("stat failed {}", t_path.c_str());
                return_500(ctx);
                ctx.set_response_end(true);
                return;
            }
            const int64_t file_size = static_cast<int64_t>(st.st_size);

            auto return_416 = [&ctx, &now_etag, file_size]() -> void
            {
                std::string response = "HTTP/1.1 416 Range Not Satisfiable\r\nServer: avant\r\n";
                if (ctx.keep_alive)
                {
                    response += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
                }
                else
                {
                    response += "Connection: close\r\n";
                }
                response += "Content-Range: bytes */" + std::to_string(file_size) + "\r\n";
                if (now_etag.size() > 0)
                {
                    response += "ETag: " + now_etag + "\r\n";
                }
                response += "Content-Length: 0\r\n";
                response += "\r\n";
                ctx.send_buffer_append(response.c_str(), response.size());
                ctx.set_response_end(true);
            };

            // Resolve the byte range requested by the client (H5 video player seeks via Range).
            // range_start/range_end hold the *effective* range only when a partial (206)
            // response is served: the suffix form "-N" resolves to [file_size-N, file_size-1]
            // (a suffix that covers the whole file falls through to a 200 full file), and an
            // explicit end beyond EOF clamps to file_size-1. partial_response stays false for
            // a 200 so the head uses chunked encoding with no Content-Range.
            bool partial_response = false;
            int64_t range_start = 0;
            int64_t range_end = file_size - 1;
            {
                bool range_valid = false;

                if (!header_ranges.empty())
                {
                    if (header_if_range.size() > 0 && now_etag.size() > 0 &&
                        header_if_range != now_etag)
                    {
                        LOG_DEBUG("If-Range[{}] != ETag[{}], ignoring Range", header_if_range.c_str(), now_etag.c_str());
                    }
                    else if (header_ranges.size() > 1)
                    {
                        // Multiple ranges would require a multipart/byteranges body;
                        // degrade to the full 200 instead (RFC 7233 allows ignoring Range).
                        LOG_DEBUG("multi-range request, ignoring Range and serving the full file");
                    }
                    else
                    {
                        const avant_http_range &r = header_ranges.front();
                        if (file_size == 0)
                        {
                            return_416();
                            return;
                        }
                        else if (r.start_empty)
                        {
                            // Suffix form "-N": the last N bytes.
                            if (r.end <= 0) // "bytes=-0" / "bytes=-" is unsatisfiable
                            {
                                return_416();
                                return;
                            }
                            if (r.end >= file_size)
                            {
                                // The suffix covers the whole (or more than) the representation:
                                // answer with the entire file as 200, NOT a 206 (RFC 7233 3.1).
                            }
                            else
                            {
                                range_start = file_size - r.end;
                                range_end = file_size - 1;
                                range_valid = true;
                            }
                        }
                        else
                        {
                            // Explicit form: "N-M" or "N-".
                            if (r.start < 0 || r.start >= file_size ||
                                (r.end != -1 && r.end < r.start))
                            {
                                // Range start out of bounds, or start > end (unsatisfiable).
                                return_416();
                                return;
                            }
                            range_valid = true;
                            range_start = r.start;
                            range_end = (r.end == -1) ? (file_size - 1) : r.end;
                            if (range_end > file_size - 1)
                            {
                                range_end = file_size - 1;
                            }
                        }
                    }
                }

                if (range_valid)
                {
                    partial_response = true;
                }
            }

            // 命中缓存 (only when no range request is being served)
            if (!partial_response &&
                header_cache_control != "no-cache" &&
                ((now_etag.size() > 0 && header_if_none_match.size() > 0 && now_etag == header_if_none_match) ||
                 (now_last_modify_date.size() > 0 && header_if_modified_since.size() > 0 && now_last_modify_date == header_if_modified_since)))
            {
                std::string response = "HTTP/1.1 304 Not Modified\r\nServer: avant\r\n";
                if (ctx.keep_alive)
                {
                    response += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
                }
                else
                {
                    response += "Connection: close\r\n";
                }
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

            // 请求处理结束后 资源 response_ptr 由 ctx.destory_callback 调用 进行释放 无需手动处理
            ctx.ptr = response_ptr;
            response_ptr->ptr_type = avant_http_app_reponse::FD;

            std::string mime_type;
            mime_type = utility::mime_type::get_type(t_path.string(), "application/octet-stream");

            // checking mine_type for use_gzip, default using use_gzip.
            // ranged responses must not be compressed (Content-Range refers to raw bytes,
            // and video/audio bytes are already compressed and won't benefit).
            {
                response_ptr->use_gzip = !partial_response && client_support_gzip &&
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
                        return_500(ctx);
                        ctx.set_response_end(true);
                        return;
                    }
                    // gzip_initialized 设置为 true 是 avant_http_app_reponse 调用 deflateEnd 的关键
                    response_ptr->gzip_initialized = true;
                }
            }

            // "rb": raw bytes; the ranged path streams them verbatim into Content-Range
            response_ptr->ptr = ::fopen(t_path.c_str(), "rb");
            if (response_ptr->ptr == NULL)
            {
                LOG_ERROR("fopen({}, rb) failed", t_path.c_str());
                return_500(ctx);
                ctx.set_response_end(true);
                return;
            }

            // range_left must be set before seeking: the write callback early-returns for
            // HEAD without touching it, and seeks must not happen for a bodyless response.
            response_ptr->range_left = (partial_response && !is_http_head_method)
                                           ? (range_end - range_start + 1)
                                           : -1;
            if (partial_response && !is_http_head_method)
            {
                if (::fseek((FILE *)response_ptr->ptr, (long)range_start, SEEK_SET) != 0)
                {
                    LOG_ERROR("fseek to range_start[{}] failed", (long long)range_start);
                    return_500(ctx);
                    ctx.set_response_end(true);
                    return;
                }
            }

            std::string response_head = "HTTP/1.1 " +
                                        std::string(partial_response ? "206 Partial Content" : "200 OK") + "\r\n" +
                                        "Server: avant\r\n";
            if (ctx.keep_alive)
            {
                response_head += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
            }
            else
            {
                response_head += "Connection: close\r\n";
            }
            response_head += "Accept-Ranges: bytes\r\n";
            if (now_etag.size() > 0)
            {
                response_head += std::string("ETag: ") + now_etag + "\r\n";
            }
            if (now_last_modify_date.size() > 0)
            {
                response_head += std::string("Last-Modified: ") + now_last_modify_date + "\r\n";
            }
            response_head += "Content-Type: " + mime_type + "\r\n";
            if (response_ptr->use_gzip && !partial_response)
            {
                response_head += "Content-Encoding: gzip\r\n";
            }
            if (partial_response)
            {
                response_head += "Content-Range: bytes " + std::to_string(range_start) + "-" +
                                 std::to_string(range_end) + "/" + std::to_string(file_size) + "\r\n";
                response_head += "Content-Length: " + std::to_string(range_end - range_start + 1) + "\r\n";
            }
            else
            {
                response_head += "Transfer-Encoding: chunked\r\n";
            }
            response_head += "\r\n";

            ctx.send_buffer_append(response_head.c_str(), response_head.size());

            // The event loop (http_ctx) invokes write_end_callback every time the send buffer
            // drains. Ranged responses stream raw bytes (Content-Length) and stop when
            // range_left hits 0. Full responses stream chunked until EOF, and gzip ones also
            // flush the deflate trailer on EOF.
            ctx.write_end_callback = [is_http_head_method, partial_response](connection::http_ctx &ctx) -> void
            {
                constexpr int buffer_size = 1024000;                  // 1000KB
                constexpr int compress_buffer_size = 2 * buffer_size; // 2000KB

                if (is_http_head_method) // HEAD 请求只响应 HTTP Response Header
                {
                    ctx.set_response_end(true);
                    return;
                }

                std::vector<char> buf(buffer_size);
                std::vector<char> compress_buf(compress_buffer_size);

                avant_http_app_reponse *resp = (avant_http_app_reponse *)ctx.ptr;

                int want = buffer_size;
                if (partial_response && resp->range_left < want)
                {
                    want = (int)resp->range_left;
                }

                int len = ::fread(buf.data(),
                                  sizeof(char),
                                  want,
                                  (FILE *)resp->ptr);

                if (len > 0)
                {
                    if (partial_response) // Range 范围请求 用的 Content-Length
                    {
                        if (resp->range_left < len)
                        {
                            len = (int)resp->range_left;
                        }
                        ctx.send_buffer_append(buf.data(), len);
                        resp->range_left -= len;
                        if (resp->range_left <= 0)
                        {
                            ctx.set_response_end(true);
                        }
                    }
                    else if (resp->use_gzip)
                    {
                        // using gzip
                        resp->strm.avail_in = len;
                        resp->strm.next_in = (Bytef *)buf.data();

                        resp->strm.avail_out = compress_buffer_size;
                        resp->strm.next_out = (Bytef *)compress_buf.data();

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
                            ctx.send_buffer_append(compress_buf.data(), exit_compressed_data_len);
                            ctx.send_buffer_append("\r\n", 2);
                        }
                    }
                    else
                    {
                        std::stringstream ss;
                        ss << std::hex << len; // convert to hex
                        std::string chunk_size = ss.str() + "\r\n";

                        ctx.send_buffer_append(chunk_size.c_str(), chunk_size.size());
                        ctx.send_buffer_append(buf.data(), len);
                        ctx.send_buffer_append("\r\n", 2);
                    }
                }
                else
                {
                    if (partial_response) // Range 范围请求 用的 Content-Length
                    {
                        ctx.set_response_end(true);
                    }
                    else if (resp->use_gzip) // 用的 Transfer-Encoding: chunked + gzip
                    {
                        resp->strm.avail_in = 0;
                        resp->strm.next_in = Z_NULL;
                        resp->strm.avail_out = compress_buffer_size;
                        resp->strm.next_out = (Bytef *)compress_buf.data();

                        int ret = deflate(&resp->strm, Z_FINISH);
                        if (ret == Z_STREAM_END)
                        {
                            int finish_compressed_data_len = compress_buffer_size - resp->strm.avail_out;
                            if (finish_compressed_data_len > 0)
                            {
                                std::stringstream ss;
                                ss << std::hex << finish_compressed_data_len;
                                std::string chunk_size = ss.str() + "\r\n";

                                ctx.send_buffer_append(chunk_size.c_str(), chunk_size.size());
                                ctx.send_buffer_append(compress_buf.data(), finish_compressed_data_len);
                                ctx.send_buffer_append("\r\n", 2);
                            }
                            ctx.send_buffer_append("0\r\n\r\n", 5);
                        }
                        else
                        {
                            LOG_ERROR("deflate Z_FINISH failed with {}", ret);
                        }
                        ctx.set_response_end(true);
                    }
                    else
                    {
                        // 用的 Transfer-Encoding: chunked
                        ctx.send_buffer_append("0\r\n\r\n", 5);
                        ctx.set_response_end(true);
                    }
                }
            };
            ctx.write_end_callback(ctx);

            return;
        }
        else if (fs::exists(t_path) && fs::is_directory(t_path)) // 是目录且存在
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
            if (ctx.keep_alive)
            {
                response_head += "Connection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\n";
            }
            else
            {
                response_head += "Connection: close\r\n";
            }
            response_head += "Content-Type: text/html; charset=UTF-8\r\n";
            response_head += "Content-Length: " + std::to_string(std::get<0>(*dir_type_ptr).size());
            response_head += "\r\n\r\n";

            ctx.send_buffer_append(response_head.c_str(), response_head.size());

            ctx.write_end_callback = [](connection::http_ctx &ctx) -> void
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
        else // 请求想访问的内容不存在
        {
            return_404(ctx);
            ctx.set_response_end(true);
            return;
        }
    };
}

// 调用 on_body_before 时，这个HTTP请求还未被 http_app::process_connection 处理过
// 在每次 http parser 解析到 HTTP 请求 Body 内容来临时都会调用 on_body_before
// 返回非 0 调用此请求 Body 不符合规定 放弃处理 连接会断开
int http_app::on_body_before(avant::connection::http_ctx &ctx, const char *at, size_t length)
{
    constexpr size_t max_body_size = 2048000;
    size_t body_size = ctx.get_recv_body_size();

    // 防止客户端请求过大的 HTTP Body
    if (body_size + length > max_body_size)
    {
        LOG_ERROR("body_size[{}] + length[{}] > max_body_size[{}]", body_size, length, max_body_size);
        return -1;
    }

    return 0;
}

// 在每次 http parser 解析到 HTTP 请求 Body 内容来临 调用 http_app::on_body_before 后就会调用 http_app::on_body
// 肯定是 在 ctx 的 conn 里 append 了 length 字节 然后再调用 http_app::on_body
// 返回非 0 调用此请求 Body 不符合规定 放弃处理 连接会断开
int http_app::on_body(avant::connection::http_ctx &ctx, size_t length)
{
    constexpr size_t max_body_size = 2048000;

    // 已经接受了多少 Http Request Body 内容（clear 前的累计值）
    const size_t body_size = ctx.get_recv_body_size();
    // recv buffer 中目前存了多少内容（clear 前）
    const size_t recv_buffer_size = ctx.get_recv_buffer_size();

    // processing http request body data
    {
        // 暂时 Body 内容不做处理 直接清空
        ctx.clear_recv_buffer();
    }

    if (body_size + length > max_body_size)
    {
        LOG_ERROR("http_app::on_body body_size[{}] + length[{}] > max_body_size[{}], recv_buffer_size[{}] (pre-clear)",
                  body_size, length, max_body_size, recv_buffer_size);
        return -1;
    }

    return 0;
}

void http_app::on_main_init(avant::server::server &server_obj)
{
    LOG_ERROR("http_app::on_main_init");
    utility::singleton<lua_plugin>::instance()->on_main_init(server_obj.get_config().get_lua_dir(),
                                                             server_obj.get_config().get_app_id(),
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
        LOG_DEBUG("worker_id {} PROTO_CMD_TUNNEL_OTHER2WORKER_TEST time {}", worker_obj.get_worker_idx(), other2worker_test.time());
        return;
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
