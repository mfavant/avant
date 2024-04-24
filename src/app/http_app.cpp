#include "app/http_app.h"
#include "server/server.h"
#include "worker/worker.h"
#include "utility/url.h"
#include "utility/mime_type.h"
#include <string>
#include <filesystem>
#include <avant-log/logger.h>

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

void http_app::on_new_connection(avant::connection::http_ctx &ctx)
{
    //  LOG_ERROR("http_app new socket %d", ctx.conn_ptr->fd);
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
        {
            const char *response = "HTTP/1.1 200 OK\r\nServer: tubekit\r\nConnection: keep-alive\r\nKeep-Alive: timeout=60, max=10000\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Length: 5\r\n\r\nHELLO";
            ctx.conn_ptr->send_buffer.append(response, strlen(response));
            ctx.set_process_end(true);
            return;
        }
        string url = utility::url::decode(ctx.url);
        auto find_res = url.find("..");
        if (std::string::npos != find_res)
        {
            ctx.set_response_end(true);
            return;
        }

        const string &prefix = worker::worker::http_static_dir;

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
            std::string response = "HTTP/1.1 200 OK\r\nServer: tubekit\r\n";
            response += "Content-Type: ";
            response += mime_type + "\r\n\r\n";

            ctx.conn_ptr->send_buffer.append(response.c_str(), response.size());

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
                    ctx.conn_ptr->send_buffer.append(buf, len);
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

            const char *response_head = "HTTP/1.1 200 OK\r\nServer: tubekit\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n";
            ctx.conn_ptr->send_buffer.append(response_head, strlen(response_head));

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
                    ctx.conn_ptr->send_buffer.append(buffer_ptr, need_send);
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
            const char *response = "HTTP/1.1 404 Not Found\r\nServer: tubekit\r\nContent-Type: text/text; charset=UTF-8\r\n\r\n";
            ctx.conn_ptr->send_buffer.append(response, strlen(response));
            ctx.set_response_end(true);
        }
    };
}

int http_app::on_body(avant::connection::http_ctx &ctx)
{
    if (ctx.conn_ptr->recv_buffer.size() > 1024)
    {
        return -1;
    }
    return 0;
}

void http_app::on_main_init(avant::server::server &server_obj)
{
    LOG_ERROR("http_app::on_main_init");
}

void http_app::on_worker_init(avant::worker::worker &worker_obj)
{
    LOG_ERROR("http_app::on_worker_init %d", worker_obj.worker_id);
}

void http_app::on_main_stop(avant::server::server &server_obj)
{
    LOG_ERROR("http_app::on_main_stop");
}

void http_app::on_worker_stop(avant::worker::worker &worker_obj)
{
    LOG_ERROR("http_app::on_worker_stop %d", worker_obj.worker_id);
}

void http_app::on_main_tick(avant::server::server &server_obj)
{
}

void http_app::on_worker_tick(avant::worker::worker &worker_obj)
{
}

void http_app::on_worker_tunnel(avant::worker::worker &worker_obj, const ProtoPackage &package)
{
    LOG_ERROR("http_app on_worker_tunnel cmd %d", package.cmd());
}