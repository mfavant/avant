#include "task/task_type.h"

using namespace avant;

task::task_type task::str2task_type(std::string str)
{
    if (str == "HTTP_TASK")
    {
        return task::task_type::HTTP_TASK;
    }
    else if (str == "STREAM_TASK")
    {
        return task::task_type::STREAM_TASK;
    }
    else if (str == "WEBSOCKET_TASK")
    {
        return task::task_type::WEBSOCKET_TASK;
    }
    return task::task_type::NONE;
}