#pragma once
#include <string>

namespace avant::task
{
    enum class task_type
    {
        HTTP_TASK = 0,
        STREAM_TASK,
        WEBSOCKET_TASK,
        NONE
    };
    task_type str2task_type(std::string str);
}