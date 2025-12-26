#include "hooks/init.h"
#include "app/http_app.h"
#include "app/stream_app.h"
#include "app/websocket_app.h"
#include "app/other_app.h"
#include <stdexcept>
#include <avant-log/logger.h>

using namespace avant::hooks;

void init::on_main_init(avant::server::server &server_obj)
{
    try
    {
        switch (server_obj.get_task_type())
        {
        case task::task_type::HTTP_TASK:
            app::http_app::on_main_init(server_obj);
            break;
        case task::task_type::STREAM_TASK:
            app::stream_app::on_main_init(server_obj);
            break;
        case task::task_type::WEBSOCKET_TASK:
            app::websocket_app::on_main_init(server_obj);
            break;
        default:
            break;
        }
    }
    catch (const std::exception &e)
    {
        exit(EXIT_FAILURE);
        LOG_ERROR("{}", e.what());
    }
}

void init::on_worker_init(avant::workers::worker &worker_obj)
{
    try
    {
        switch (worker_obj.type)
        {
        case task::task_type::HTTP_TASK:
            app::http_app::on_worker_init(worker_obj);
            break;
        case task::task_type::STREAM_TASK:
            app::stream_app::on_worker_init(worker_obj);
            break;
        case task::task_type::WEBSOCKET_TASK:
            app::websocket_app::on_worker_init(worker_obj);
            break;
        default:
            break;
        }
    }
    catch (const std::exception &e)
    {
        exit(EXIT_FAILURE);
        LOG_ERROR("{}", e.what());
    }
}

void init::on_other_init(avant::workers::other &other_obj)
{
    try
    {
        app::other_app::on_other_init(other_obj);
    }
    catch (const std::exception &e)
    {
        exit(EXIT_FAILURE);
        LOG_ERROR("{}", e.what());
    }
}