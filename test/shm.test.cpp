// g++ -std=c++20 shm.test.cpp -o shm_test.exe   (Linux: add -lrt)
#include <iostream>
#include <cstdint>
#include "../external/avant-ipc/shm.h"

using namespace avant::ipc;

int main()
{
    // testing No.1: trivially copyable type
    {
        shm_pool<int> pool("/avant_ipc_pool_test", 10);
        if (!pool.init())
        {
            std::cerr << "init failed" << std::endl;
            return 1;
        }
        pool.foreach (
            [](int *obj_ptr, bool used) -> bool
            {
                std::cout << *obj_ptr << " ";
                return true;
            },
            false);
        std::cout << std::endl;

        std::cout << "alloc start" << std::endl;

        int *obj1 = pool.alloc();
        if (obj1)
            *obj1 = 1;
        int *obj2 = pool.alloc();
        if (obj2)
            *obj2 = 2;
        int *obj3 = pool.alloc();
        if (obj3)
            *obj3 = 3;
        int *obj4 = pool.alloc();
        if (obj4)
            *obj4 = 4;
        int *obj5 = pool.alloc();
        if (obj5)
            *obj5 = 5;
        int *obj6 = pool.alloc();
        if (obj6)
            *obj6 = 6;
        int *obj7 = pool.alloc();
        if (obj7)
            *obj7 = 7;
        int *obj8 = pool.alloc();
        if (obj8)
            *obj8 = 8;

        pool.foreach (
            [](int *obj_ptr, bool used) -> bool
            {
                std::cout << *obj_ptr << " ";
                return true;
            });
        std::cout << std::endl;

        std::cout << "back res=" << pool.back(obj6) << std::endl;

        pool.foreach (
            [](int *obj_ptr, bool used) -> bool
            {
                std::cout << *obj_ptr << " ";
                return true;
            });
        std::cout << std::endl;

        // stale in-use slots survive close(); reset() frees them
        pool.close();
        if (!pool.init())
        {
            std::cerr << "re-init failed" << std::endl;
            return 1;
        }
        pool.reset();
        pool.foreach (
            [](int *obj_ptr, bool used) -> bool
            {
                std::cout << "after reset used=" << used << " ";
                return true;
            },
            false);
        std::cout << std::endl;

        pool.unlink();
    }

    // testing No.2: struct with mixed alignment
    {
        struct Foo
        {
            char a;
            uint64_t b;
            uint32_t c;
            uint8_t d;
        };
        shm_pool<Foo> pool("/avant_ipc_pool_test_foo", 10);
        if (!pool.init())
        {
            std::cerr << "init failed" << std::endl;
            return 1;
        }
        pool.foreach (
            [](Foo *obj_ptr, bool used) -> bool
            {
                std::cout << obj_ptr->a << " " << obj_ptr->b << " " << obj_ptr->c << " " << (int)obj_ptr->d << std::endl;
                return true;
            },
            false);
        std::cout << std::endl;

        Foo *foo1 = pool.alloc();
        if (foo1)
        {
            foo1->a = 'a';
            foo1->b = 123456789;
            foo1->c = 123456;
            foo1->d = 123;
        }
        Foo *foo2 = pool.alloc();
        if (foo2)
        {
            foo2->a = 'b';
            foo2->b = 987654321;
            foo2->c = 654321;
            foo2->d = 3;
        }
        pool.foreach (
            [](Foo *obj_ptr, bool used) -> bool
            {
                std::cout << obj_ptr->a << " " << obj_ptr->b << " " << obj_ptr->c << " " << (int)obj_ptr->d << std::endl;
                return true;
            });
        std::cout << std::endl;

        pool.unlink();
    }

    return 0;
}
