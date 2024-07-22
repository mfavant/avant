#pragma once

#include <string>
#include <mysql/mysql.h>
#include "connection.h"

namespace avant
{
    namespace sql
    {
        class transaction
        {
        public:
            transaction(avant::sql::connection *conn);
            ~transaction();

            int commit();
            int rollback();

        private:
            avant::sql::connection *conn{nullptr};
            bool commited{false};
        };
    }
}