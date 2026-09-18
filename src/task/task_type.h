#pragma once
#include <string_view>

namespace avant::task
{
    enum class task_type
    {
        HTTP_TASK = 0,
        STREAM_TASK,
        WEBSOCKET_TASK,
        NONE
    };
    task_type str2task_type(std::string_view str);
}