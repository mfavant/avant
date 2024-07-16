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
#include "global/tunnel_id.h"
#include "proto/proto_util.h"

using namespace avant::app;
using std::string;
using std::vector;
namespace fs = std::filesystem;
namespace utility = avant::utility;

class html_loader
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

struct http_app_reponse
{
    enum type
    {
        DIR = 0,
        FD = 1,
        NONE = 2,
    };

    void *ptr{nullptr};
    type ptr_type{NONE};

    typedef std::tuple<std::string, size_t> DIR_TYPE;
    typedef FILE FD_TYPE;

    inline void destory()
    {
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

// ctx created
void http_app::on_ctx_create(avant::connection::http_ctx &ctx)
{
    // LOG_ERROR("http_app::on_ctx_create %llu", ctx.get_conn_gid());
}

void http_app::on_new_connection(avant::connection::http_ctx &ctx)
{
    // send new_connection protocol to other thread
    {
        ProtoPackage package;
        ProtoTunnelWorker2OtherEventNewClientConnection protoNewConn;
        protoNewConn.set_gid(ctx.get_conn_gid());
        ctx.tunnel_forward(std::vector<int>{avant::global::tunnel_id::get().get_other_tunnel_id()},
                           avant::proto::pack_package(package, protoNewConn, ProtoCmd::PROTO_CMD_TUNNEL_WORKER2OTHER_EVENT_NEW_CLIENT_CONNECTION));
    }
    // LOG_ERROR("http_app new socket gid %llu", ctx.get_conn_gid());
}

void http_app::process_connection(avant::connection::http_ctx &ctx)
{
    // load callback
    ctx.destory_callback = [](avant::connection::http_ctx &ctx) -> void
    {
        if (ctx.ptr)
        {
            http_app_reponse *reponse_ptr = (http_app_reponse *)ctx.ptr;
            reponse_ptr->destory();
            delete reponse_ptr;
            ctx.ptr = nullptr;
        }
    };

    ctx.process_callback = [](avant::connection::http_ctx &ctx) -> void
    {
        bool exist_keep_live = false;
        if (ctx.headers.find("Connection") != ctx.headers.end())
        {
            for (const std::string &str : ctx.headers.find("Connection")->second)
            {
                if (str == "keep-alive")
                {
                    exist_keep_live = true;
                    break;
                }
            }
        }

#if 0
        if (exist_keep_live)
        {
            ctx.keep_alive = true;
        }
        {
            const char *response = "HTTP/1.1 200 OK\r\nServer: avant\r\nConnection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Length: 5\r\n\r\nHELLO";
            ctx.send_buffer_append(response, strlen(response));
            ctx.set_process_end(true);
            return;
        }
#else
        if (exist_keep_live)
        {
            ctx.keep_alive = false; // app not use keep_alive
        }
#endif

        string url = utility::url::decode(ctx.url);
        auto find_res = url.find("..");
        if (std::string::npos != find_res)
        {
            ctx.set_response_end(true);
            return;
        }

        const string &prefix = workers::worker::http_static_dir;

        fs::path t_path = prefix + url;
        // if (url.empty() || url[0] != '/')
        // {
        //     t_path = prefix + url;
        // }
        // else
        // {
        //     t_path = url;
        // }

        if (fs::exists(t_path) && fs::is_regular_file(t_path))
        {
            auto response_ptr = new (std::nothrow) http_app_reponse;
            if (!response_ptr)
            {
                ctx.set_response_end(true);
                return;
            }
            ctx.ptr = response_ptr;
            response_ptr->ptr_type = http_app_reponse::FD;

            std::string mime_type;
            try
            {
                mime_type = utility::mime_type::get_type(t_path.string());
            }
            catch (...)
            {
                mime_type = "application/octet-stream";
            }
            std::string response = "HTTP/1.1 200 OK\r\nServer: avant\r\nConnection: close\r\n";
            response += "Content-Type: ";
            response += mime_type + "\r\n\r\n";

            ctx.send_buffer_append(response.c_str(), response.size());

            response_ptr->ptr = ::fopen(t_path.c_str(), "r");
            if (response_ptr->ptr == nullptr)
            {
                ctx.set_response_end(true);
                return;
            }
            // Write when the contents of the buffer have been sent write_end_callback will be executed,
            // and the response must be set response_end to true, then write after write_end_callback will be continuously recalled
            ctx.write_end_callback = [](connection::http_ctx &ctx) -> void
            {
                constexpr int buffer_size = 1024000;
                char buf[buffer_size] = {0};
                int len = 0;
                len = ::fread(buf, sizeof(char), buffer_size, (http_app_reponse::FD_TYPE *)((http_app_reponse *)ctx.ptr)->ptr);
                if (len > 0)
                {
                    ctx.send_buffer_append(buf, len);
                }
                else
                {
                    ctx.set_response_end(true);
                }
            };
            ctx.write_end_callback(ctx);
            return;
        }
        else if (fs::exists(t_path) && fs::is_directory(t_path))
        {
            auto response_ptr = new (std::nothrow) http_app_reponse;
            if (!response_ptr)
            {
                ctx.set_response_end(true);
                return;
            }
            ctx.ptr = response_ptr;
            response_ptr->ptr_type = http_app_reponse::DIR;
            response_ptr->ptr = new (std::nothrow) http_app_reponse::DIR_TYPE;
            if (!response_ptr->ptr)
            {
                ctx.set_response_end(true);
                return;
            }
            auto dir_type_ptr = (http_app_reponse::DIR_TYPE *)response_ptr->ptr;

            const char *response_head = "HTTP/1.1 200 OK\r\nServer: avant\r\nConnection: close\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n";
            ctx.send_buffer_append(response_head, strlen(response_head));

            //  generate dir list
            vector<string> a_tags;
            try
            {
                for (const auto &dir_entry : fs::directory_iterator(t_path))
                {
                    std::string sub_path = dir_entry.path().string().substr(prefix.size());
                    a_tags.push_back(html_loader::a_tag(utility::url::encode(sub_path), sub_path));
                }
            }
            catch (const std::filesystem::filesystem_error &ex)
            {
                LOG_ERROR("%s", ex.what());
            }

            string body;
            for (const auto &a_tag : a_tags)
            {
                body += a_tag;
            }
            std::get<0>(*dir_type_ptr) = html_loader::load(body);
            std::get<1>(*dir_type_ptr) = 0;

            ctx.write_end_callback = [](connection::http_ctx &ctx) -> void
            {
                http_app_reponse *response_ptr = (http_app_reponse *)ctx.ptr;
                auto dir_type_ptr = (http_app_reponse::DIR_TYPE *)response_ptr->ptr;
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
            const char *response = "HTTP/1.1 404 Not Found\r\nServer: avant\r\nConnection: close\r\nContent-Type: text/text; charset=UTF-8\r\n\r\n";
            ctx.send_buffer_append(response, strlen(response));
            ctx.set_response_end(true);
        }
    };
}

int http_app::on_body(avant::connection::http_ctx &ctx, size_t length)
{
    size_t recv_buffer_size = ctx.get_recv_buffer_size();
    size_t body_size = ctx.get_recv_body_size();

    // processing http request body data
    {
        ctx.clear_recv_buffer();
    }

    constexpr size_t max_body_size = 2048000;

    if (body_size > max_body_size)
    {
        LOG_ERROR("recv_buffer_size %llu length %llu body_size %llu", recv_buffer_size, length, body_size);
        LOG_ERROR("http_app::on_body body_size > %llu", max_body_size);
        return -1;
    }

    return 0;
}

void http_app::on_main_init(avant::server::server &server_obj)
{
    LOG_ERROR("http_app::on_main_init");
    utility::singleton<lua_plugin>::instance()->on_main_init(server_obj.get_lua_dir(), server_obj.get_worker_cnt());
}

void http_app::on_worker_init(avant::workers::worker &worker_obj)
{
    LOG_ERROR("http_app::on_worker_init %d", worker_obj.worker_id);
    utility::singleton<lua_plugin>::instance()->on_worker_init(worker_obj.worker_id);
}

void http_app::on_main_stop(avant::server::server &server_obj)
{
    LOG_ERROR("http_app::on_main_stop");
    utility::singleton<lua_plugin>::instance()->on_main_stop();
}

void http_app::on_worker_stop(avant::workers::worker &worker_obj)
{
    LOG_ERROR("http_app::on_worker_stop %d", worker_obj.worker_id);
    utility::singleton<lua_plugin>::instance()->on_worker_stop(worker_obj.worker_id);
}

void http_app::on_main_tick(avant::server::server &server_obj)
{
    utility::singleton<lua_plugin>::instance()->on_main_tick();
}

void http_app::on_worker_tick(avant::workers::worker &worker_obj)
{
    utility::singleton<lua_plugin>::instance()->on_worker_tick(worker_obj.worker_id);
}

void http_app::on_worker_tunnel(avant::workers::worker &worker_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package)
{
    LOG_ERROR("http_app on_worker_tunnel cmd %d", package.cmd());
}
