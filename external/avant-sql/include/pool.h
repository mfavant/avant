#pragma once

#include <mutex>
#include <list>
#include <memory>
#include <condition_variable>
#include "connection.h"

namespace avant
{
    namespace sql
    {
        class pool
        {
        public:
            pool();
            ~pool();
            void init(int size,
                      const std::string &ip,
                      const std::string &user,
                      const std::string &password,
                      const std::string &db);
            std::shared_ptr<connection> get();
            void back(std::shared_ptr<connection> ptr);
            int get_size();

        private:
            std::mutex m_mutex;
            std::condition_variable cv;
            std::list<std::shared_ptr<connection>> m_list;
        };
    }
}