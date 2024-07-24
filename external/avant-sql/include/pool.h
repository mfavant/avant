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
                      const std::string &db,
                      const unsigned int timeout_seconds = 10,
                      const unsigned int port = 3306);
            std::shared_ptr<connection> get();
            void back(std::shared_ptr<connection> ptr);
            int get_size();

        private:
            int size{0};
            std::string ip;
            std::string user;
            std::string password;
            std::string db;
            unsigned int timeout_seconds{0};
            unsigned int port{0};

        private:
            std::mutex mutex;
            std::condition_variable cv;
            std::list<std::shared_ptr<connection>> list;
        };
    }
}