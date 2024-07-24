#pragma once

#include <string>
#include <mysql/mysql.h>

namespace avant
{
    namespace sql
    {
        class connection
        {
        public:
            connection();
            ~connection();
            bool connect(const std::string &ip,
                         const std::string &user,
                         const std::string &password,
                         const std::string &db,
                         const unsigned int port = 3306);
            void close();
            MYSQL *get();
            bool ping();

        private:
            MYSQL *conn;
        };
    }
}