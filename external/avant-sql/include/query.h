#pragma once
#include <memory>

#include "connection.h"

namespace avant
{
    namespace sql
    {
        class query
        {
        public:
            query(std::shared_ptr<connection> conn);
            ~query();
            void execute(const std::string &sql);
            uint64_t insert(const std::string &sql);
            uint64_t del(const std::string &sql);
            void select(const std::string &sql);
            uint64_t update(const std::string &sql);

        private:
            std::shared_ptr<connection> conn;
        };
    }
}