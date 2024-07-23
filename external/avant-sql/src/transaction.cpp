#include "transaction.h"
#include <stdexcept>

using avant::sql::connection;
using avant::sql::transaction;

transaction::transaction(std::shared_ptr<connection> conn) : conn(conn),
                                                             commited(false),
                                                             save_point_seq(0)
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

int transaction::save_point()
{
    if (this->commited)
    {
        return -1;
    }
    std::string new_save_point = std::string("sp") + std::to_string(this->save_point_seq++);
    std::string sql("SAVEPOINT ");
    sql += new_save_point;
    if (mysql_query(this->conn->get(), sql.c_str()))
    {
        throw std::runtime_error("failed to save_point: " + std::string(mysql_error(this->conn->get())));
    }
    this->save_points.push_back(new_save_point);
    return 0;
}

int transaction::rollback_once()
{
    if (this->commited)
    {
        return -1;
    }
    if (this->save_points.empty())
    {
        return -2;
    }
    std::string save_point = *this->save_points.rbegin();
    std::string sql("ROLLBACK TO ");
    sql += save_point;
    if (mysql_query(this->conn->get(), sql.c_str()))
    {
        throw std::runtime_error("failed to rollback_once: " + std::string(mysql_error(this->conn->get())));
    }
    this->save_points.pop_back();
    return 0;
}