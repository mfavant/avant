#pragma once
#include <string>

namespace avant
{
    namespace redis
    {
        class redis
        {
        public:
            redis(const std::string &host, const std::string &pwd, const int &port = 6379);
            ~redis();
            void test();

        private:
            std::string host{};
            int port{0};
            std::string pwd{};
        };
    }
}