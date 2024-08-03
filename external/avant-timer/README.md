# avant-timer

## example

```cpp
#include <iostream>
#include <thread>
#include <memory>
#include <unistd.h>
#include <ctime>

#include "../external/avant-timer/timer.h"
#include "../external/avant-timer/timer_manager.h"

using namespace std;
using namespace avant::timer;

int main(int argc, char **argv)
{
    std::shared_ptr<timer_manager> manager_instance = std::make_shared<timer_manager>();

    uint64_t timer_id_gen = 0;
    uint64_t now_time_stamp = ::time(nullptr);

    {
        std::shared_ptr<timer> new_timer = std::make_shared<timer>(timer_id_gen++, now_time_stamp, -1, 1, [](timer &self) -> void
                                                                   { std::cout << self.get_id() << ">>" << "-1 1" << std::endl; });
        manager_instance->add(new_timer);
    }

    {
        std::shared_ptr<timer> new_timer = std::make_shared<timer>(timer_id_gen++, now_time_stamp, 3, 3, [](timer &self) -> void
                                                                   { std::cout << self.get_id() << ">>" << "3 3" << std::endl; });
        manager_instance->add(new_timer);
    }

    {
        std::shared_ptr<timer> new_timer = std::make_shared<timer>(timer_id_gen++, now_time_stamp, 1, 5, [](timer &self) -> void
                                                                   { std::cout << self.get_id() << ">>" << "1 5" << std::endl; });
        manager_instance->add(new_timer);
    }

    thread m_thread(
        [&]() -> void
        {
            for (int i = 0; i < 10; i++)
            {
                std::shared_ptr<timer> new_timer = std::make_shared<timer>(timer_id_gen++, now_time_stamp, 1, i, [i](timer &self) -> void
                                                                           { std::cout << self.get_id() << ">>" << "1 " << i << std::endl; });
                manager_instance->add(new_timer);
            }
            while (1)
            {
                sleep(1); // can use select epoll nanosleep etc...
                uint64_t now_time_stamp = ::time(nullptr);
                manager_instance->check_and_handle(now_time_stamp);
            }
        });

    m_thread.detach();

    sleep(100);
    return 0;
}

//  g++ timer.test.cpp ../external/avant-timer/timer.cpp ../external/avant-timer/timer_manager.cpp -o timer.test.exe -I"../src/" -lpthread
```