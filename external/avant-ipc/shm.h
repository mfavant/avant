#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <list>
#include <functional>

#define DEBUG cout << "DEBUG " << __LINE__ << endl

using namespace std;

namespace avant
{
    namespace ipc
    {
        template <typename T>
        class shm_pool
        {
        public:
            shm_pool(const std::string &name, const size_t &count);
            ~shm_pool();
            bool init();
            bool close();
            bool unlink();
            T *alloc();
            bool back(T *t);
            void foreach (std::function<bool(T *t, bool used)> callback, bool used = true);

        private:
            T *get_by_index(size_t index);

        private:
            size_t count{0};
            size_t mem_size{0};
            std::string name{};
            T *list{nullptr};
            int mem_fd{-1};
            void *mem_ptr{nullptr};
        };

        template <typename T>
        shm_pool<T>::shm_pool(const std::string &name, const size_t &count) : name(name), count(count)
        {
            mem_size = sizeof(char) * count + sizeof(T) * count;
        }

        template <typename T>
        shm_pool<T>::~shm_pool()
        {
            if (mem_fd != -1)
            {
                close();
            }
        }

        template <typename T>
        bool shm_pool<T>::init()
        {
            int shm_fd = shm_open(name.c_str(), O_RDWR, 0);
            bool recreate = false;
            if (shm_fd == -1) // create
            {
                mem_fd = shm_open(name.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                if (mem_fd == -1)
                {
                    return false;
                }
                // shm space size
                if (-1 == ftruncate(mem_fd, mem_size))
                {
                    return false;
                }
                recreate = true;
            }
            else
            {
                mem_fd = shm_fd;
            }
            // mapping
            mem_ptr = mmap(nullptr, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
            if (mem_ptr == MAP_FAILED)
            {
                return false;
            }
            char *use_list = (char *)mem_ptr;
            if (recreate)
            {
                for (size_t i = 0; i < count; ++i)
                {
                    use_list[i] = 0;
                }
            }
            return true;
        }

        template <typename T>
        bool shm_pool<T>::close()
        {
            mem_ptr = nullptr;
            int close_res = -1;
            if (mem_fd != -1)
            {
                close_res = ::close(mem_fd);
            }
            return (close_res != -1);
        }

        template <typename T>
        bool shm_pool<T>::unlink()
        {
            close();
            mem_ptr = nullptr;
            return (shm_unlink(name.c_str()) != -1);
        }

        template <typename T>
        T *shm_pool<T>::alloc()
        {
            T *object_ptr = (T *)((char *)mem_ptr + sizeof(char) * count);
            char *use_list = (char *)mem_ptr;
            for (size_t i = 0; i < count; ++i)
            {
                if (use_list[i] == 0) // unused
                {
                    use_list[i] = 1;
                    return &object_ptr[i];
                }
            }
            return nullptr;
        }

        template <typename T>
        bool shm_pool<T>::back(T *t)
        {
            T *start_addr = (T *)((char *)mem_ptr + sizeof(char) * count);
            T *end_addr = start_addr + count;
            if (!(t <= end_addr && t >= start_addr))
            {
                DEBUG;
                return false;
            }
            size_t gap = (char *)t - (char *)start_addr;
            if (gap % sizeof(T) != 0)
            {
                DEBUG;
                return false;
            }
            size_t index = gap / sizeof(T);
            {
                char *use_list = (char *)mem_ptr;
                if (use_list[index] != 1)
                {
                    DEBUG;
                    return false;
                }
                use_list[index] = 0;
            }
            return true;
        }

        template <typename T>
        T *shm_pool<T>::get_by_index(size_t index)
        {
            if (index > count)
            {
                return nullptr;
            }
            T *start_addr = (T *)((char *)mem_ptr + sizeof(char) * count);
            return start_addr + index;
        }

        template <typename T>
        void shm_pool<T>::foreach (std::function<bool(T *t, bool used)> callback, bool used)
        {
            char *use_list = (char *)mem_ptr;
            for (size_t i = 0; i < count; ++i)
            {
                if (used)
                {
                    if (use_list[i])
                    {
                        bool continue_able = callback(get_by_index(i), use_list[i]);
                        if (!continue_able)
                        {
                            break;
                        }
                    }
                }
                else
                {
                    bool continue_able = callback(get_by_index(i), use_list[i]);
                    if (!continue_able)
                    {
                        break;
                    }
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    avant::ipc::shm_pool<int> pool("/m_pool", 10);
    pool.init();
    pool.foreach (
        [](int *obj_ptr, bool used) -> bool
        {
            cout << *obj_ptr << " ";
            return true;
        },
        false);
    cout << endl;

    cout << "alloc start" << endl;

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

    cout << "DEBUG " << __LINE__ << endl;

    pool.foreach (
        [](int *obj_ptr, bool used) -> bool
        {
            cout << *obj_ptr << " ";
            return true;
        });
    cout << endl;

    cout << "back res=" << pool.back(obj6) << endl;

    pool.foreach (
        [](int *obj_ptr, bool used) -> bool
        {
            cout << *obj_ptr << " ";
            return true;
        });
    cout << endl;

    pool.unlink();
    return 0;
}

/*
output

0 0 0 0 0 0 0 0 0 0
alloc start
DEBUG 244
1 2 3 4 5 6 7 8
back res=1
1 2 3 4 5 7 8

*/