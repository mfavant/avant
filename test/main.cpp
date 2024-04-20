#include <iostream>
#include <memory>
using namespace std;

class A
{
public:
    A()
    {
        std::cout << "A" << std::endl;
    }
    ~A()
    {
        std::cout << "~A" << std::endl;
    }
};

int main(int argc, char **argv)
{
    {
        std::cout << "test1" << std::endl;
        std::shared_ptr<A[]> ptr2 = nullptr;
        std::cout << "test2" << std::endl;
        {
            std::shared_ptr<A[]> ptr1 = nullptr;
            auto addr = new A[10];
            std::cout << "testk" << std::endl;
            ptr1.reset(addr);
            ptr2 = ptr1;
            std::cout << "test3" << std::endl;
        }
        std::cout << "test4" << std::endl;
    }
    std::cout << "test5" << std::endl;

    std::shared_ptr<std::shared_ptr<A>[]> ptr4;
    ptr4.reset(new std::shared_ptr<A>[4]);
    std::shared_ptr<A> ptr5;
    {
        for (int i = 0; i < 4; i++)
        {
            ptr4[i].reset(new A);
        }
        ptr5 = ptr4[0];
    }
    std::cout << "P" << std::endl;

    return 0;
}