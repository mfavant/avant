#include "hooks/reload.h"
#include "app/http_app.h"
#include "app/stream_app.h"
#include "app/websocket_app.h"
#include "app/other_app.h"
#include <stdexcept>
#include <avant-log/logger.h>

using namespace avant::hooks;

void reload::on_cmd_reload(avant::server::server &server_obj)
{
    try
    {
        switch (server_obj.get_task_type())
        {
        case task::task_type::HTTP_TASK:
        {

            LOG_ERROR("signal on_cmd_reload http_task");
            app::http_app::on_cmd_reload(server_obj);
            break;
        }
        case task::task_type::STREAM_TASK:
        {
            LOG_ERROR("signal on_cmd_reload stream_task");
            app::stream_app::on_cmd_reload(server_obj);
            break;
        }
        case task::task_type::WEBSOCKET_TASK:
        {
            LOG_ERROR("signal on_cmd_reload websocket_task");
            app::websocket_app::on_cmd_reload(server_obj);
            break;
        }
        default:
            break;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("{}", e.what());
    }
}
