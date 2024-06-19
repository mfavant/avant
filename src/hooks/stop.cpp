#include "hooks/stop.h"
#include "app/http_app.h"
#include "app/stream_app.h"
#include "app/websocket_app.h"
#include "app/other_app.h"
#include <stdexcept>
#include <avant-log/logger.h>

using namespace avant::hooks;

void stop::on_main_stop(avant::server::server &server_obj)
{
    try
    {
        switch (server_obj.get_task_type())
        {
        case task::task_type::HTTP_TASK:
            app::http_app::on_main_stop(server_obj);
            break;
        case task::task_type::STREAM_TASK:
            app::stream_app::on_main_stop(server_obj);
            break;
        case task::task_type::WEBSOCKET_TASK:
            app::websocket_app::on_main_stop(server_obj);
            break;
        default:
            break;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(e.what());
    }
}

void stop::on_worker_stop(avant::workers::worker &worker_obj)
{
    try
    {
        switch (worker_obj.type)
        {
        case task::task_type::HTTP_TASK:
            app::http_app::on_worker_stop(worker_obj);
            break;
        case task::task_type::STREAM_TASK:
            app::stream_app::on_worker_stop(worker_obj);
            break;
        case task::task_type::WEBSOCKET_TASK:
            app::websocket_app::on_worker_stop(worker_obj);
            break;
        default:
            break;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(e.what());
    }
}

void stop::on_other_stop(avant::workers::other &other_obj)
{
    try
    {
        app::other_app::on_other_stop(other_obj);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(e.what());
    }
}