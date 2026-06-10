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

class AArg
{
public:
    static int count;
    int num;
    AArg(int n)
    {
        count += n;
        num = count;
        cout << "AArg() " << num << endl;
    }
    ~AArg()
    {
        cout << "~AArg() " << num << endl;
    }
};

int AArg::count = 0;

int main(int argc, char **argv)
{
    {
        object_pool<A> pool;
        pool.init(100, false);
        list<A *> m_list;
        for (size_t i = 0; i < 100; i++)
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

    {
        object_pool<AArg> pool;
        pool.init(100, true, 10);
        list<AArg *> m_list;
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

// g++ object_pool.test.cpp -o object_pool.test.exe -lpthread
// ./object_pool.test.exe
