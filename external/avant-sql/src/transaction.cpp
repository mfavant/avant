#include "transaction.h"
#include <stdexcept>

using avant::sql::connection;
using avant::sql::transaction;

transaction::transaction(connection *conn) : conn(conn),
                                             commited(false)
{
    if (mysql_query(this->conn->get(), "START TRANSACTION"))
    {
        throw std::runtime_error("failed to start transaction: " + std::string(mysql_error(this->conn->get())));
    }
}

transaction::~transaction()
{
    if (!commited)
    {
        rollback();
    }
}

int transaction::commit()
{
    if (mysql_query(this->conn->get(), "COMMIT"))
    {
        throw std::runtime_error("failed to commit transaction: " + std::string(mysql_error(this->conn->get())));
    }
    this->commited = true;
    return 0;
}

int transaction::rollback()
{
    if (mysql_query(this->conn->get(), "ROLLBACK"))
    {
        throw std::runtime_error("failed to rollback transaction: " + std::string(mysql_error(this->conn->get())));
    }
    this->commited = true;
    return 0;
}