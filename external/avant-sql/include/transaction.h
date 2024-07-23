#pragma once

#include <string>
#include <mysql/mysql.h>
#include <vector>
#include <string>
#include "connection.h"
#include <memory>

namespace avant
{
    namespace sql
    {
        class transaction
        {
        public:
            transaction(std::shared_ptr<avant::sql::connection> conn);
            ~transaction();

            int commit();
            int rollback();
            int save_point();
            int rollback_once();

        private:
            std::shared_ptr<avant::sql::connection> conn{nullptr};
            bool commited{false};
            std::vector<std::string> save_points;
            int save_point_seq{0};
        };
    }
}