#include <iostream>
#include <list>
#include "../src/utility/object_pool.h"
using namespace std;
using namespace avant::utility;

class A
{
public:
    static int count;
    int num;
    A()
    {
        num = count++;
        cout << "A() " << num << endl;
    }
    ~A()
    {
        cout << "~A() " << num << endl;
    }
};

int A::count = 0;

int main(int argc, char **argv)
{
    {
        object_pool<A> pool;
        pool.init(100);
        list<A *> m_list;
        for (size_t i = 0; i < 120; i++)
        {
            auto p = pool.allocate();
            if (p)
            {
                m_list.push_back(p);
            }
        }
        for (auto p : m_list)
        {
            pool.release(p);
        }
    }
    return 0;
}

// g++ object_pool.test.cpp ../src/thread/auto_lock.cpp ../src/thread/mutex.cpp -o object_pool.test.exe -lpthread
// ./object_pool.test.exe
