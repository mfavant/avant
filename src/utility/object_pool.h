#pragma once
#include <unordered_set>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace avant
{
    namespace utility
    {
        template <typename T>
        class object_pool
        {
        public:
            object_pool() = default;
            ~object_pool();

            /**
             * @brief init object to storage in list
             *
             * @param max_size number of object
             */
            template <typename... ARGS>
            int init(size_t max_size, bool block, ARGS &&...args);

            /**
             * @brief get a object from list
             *
             * @return T* can null
             */
            [[nodiscard]] T *allocate();

            /**
             * @brief return object to list
             *
             */
            void release(T *t);

            uint space();

        private:
            std::unordered_set<T *> m_set{};

            /**
             * @brief ensure thread safety for m_list operations
             *
             */
            std::mutex m_mutex;
            std::condition_variable m_condition;
            T *m_objects{nullptr};
            bool m_block{true};
            size_t m_max_size{0};
        };

        template <typename T>
        object_pool<T>::~object_pool()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_objects)
            {
                for (size_t i = 0; i < m_max_size; i++)
                {
                    m_objects[i].~T();
                }
            }
            free(m_objects);
            m_objects = nullptr;
            m_set.clear();
        }

        template <typename T>
        template <typename... ARGS>
        int object_pool<T>::init(size_t max_size, bool block, ARGS &&...args)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_objects = (T *)::malloc(sizeof(T) * max_size);
            if (!m_objects)
            {
                return -1;
            }
            m_max_size = 0;
            for (size_t i = 0; i < max_size; ++i)
            {
                auto ptr = new (&m_objects[i]) T(std::forward<ARGS>(args)...);
                if (ptr)
                {
                    m_max_size++;
                    m_set.insert(ptr);
                }
                else
                {
                    return -2;
                }
            }
            m_block = block;
            return 0;
        }

        template <typename T>
        T *object_pool<T>::allocate()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_block)
            {
                while (m_set.empty())
                {
                    m_condition.wait(lock);
                }
            }
            else
            {
                if (m_set.empty())
                {
                    return nullptr;
                }
            }
            T *p = *m_set.begin();
            m_set.erase(m_set.begin());
            return p;
        }

        template <typename T>
        void object_pool<T>::release(T *t)
        {
            if (t)
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_set.insert(t);
                }
                m_condition.notify_all();
            }
        }

        template <typename T>
        uint object_pool<T>::space()
        {
            uint space = 0;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                space = m_set.size();
            }
            return space;
        }
    }
}
