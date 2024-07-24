#include "connection.h"

#include <iostream>
#include <mysql/mysql.h>
#include <cstring>
#include <stdexcept>

using namespace avant::sql;
using std::string;

connection::connection() : conn(nullptr)
{
    this->conn = mysql_init(NULL);
    if (this->conn == nullptr)
    {
        throw std::runtime_error("this->conn == nullptr");
    }
}

bool connection::connect(const std::string &ip,
                         const std::string &user,
                         const std::string &password,
                         const std::string &db,
                         const unsigned int port /*= 3306*/)
{
    if (!mysql_real_connect(this->conn, ip.c_str(), user.c_str(), password.c_str(), db.c_str(), port, nullptr, 0))
    {
        std::cout << __FILE__ << __LINE__ << mysql_error(this->conn) << std::endl;
        return false;
    }
    return true;
}

MYSQL *connection::get()
{
    return this->conn;
}

connection::~connection()
{
    close();
}

void connection::close()
{
    if (this->conn)
    {
        mysql_close(this->conn);
        this->conn = nullptr;
    }
}

bool connection::ping()
{
    if (mysql_ping(this->conn) != 0)
    {
        return false;
    }
    return true;
}
