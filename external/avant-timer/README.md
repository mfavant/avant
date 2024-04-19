# avant-timer

## example

```cpp
#include <iostream>
#include <thread>
#include <memory>
#include <unistd.h>

#include "timer/timer.h"
#include "timer/timer_manager.h"

using namespace std;
using namespace avant::timer;

int main(int argc, char **argv)
{
    auto m_manager = make_shared<timer_manager>();
    m_manager->add(-1, 1,
                   []() -> void
                   {
                       cout << "-1 1" << endl;
                   });
    m_manager->add(3, 3,
                   []() -> void
                   {
                       cout << "3 3" << endl;
                   });
    int loop_timer_id = m_manager->add(1, 5,
                                       []() -> void
                                       {
                                           cout << "1 5" << endl;
                                       });
    thread m_thread(
        [&]() -> void
        {
            while (1)
            {
                sleep(1); // can use select epoll nanosleep etc...
                m_manager->check_and_handle();
            }
        });

    m_thread.detach();

    for (int i = 0; i < 10; i++)
    {
        m_manager->add(1, i,
                       [i]() -> void
                       {
                           cout << "1 " << i << endl;
                       });
    }

    sleep(100);
    return 0;
}

// g++ timer.test.cpp ../src/timer/timer.cpp ../src/timer/timer_manager.cpp -o timer.test.exe -I"../src/" -lpthread
```