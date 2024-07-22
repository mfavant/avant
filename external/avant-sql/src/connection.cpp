#include "connection.h"

#include <iostream>
#include <mysql/mysql.h>
#include <cstring>

using namespace avant::sql;
using std::string;

connection::connection() : conn(nullptr)
{
}

bool connection::connect(const std::string &ip,
                         const std::string &user,
                         const std::string &password,
                         const std::string &db,
                         const unsigned int port /*= 3306*/)
{
    if (conn == nullptr)
    {
        conn = mysql_init(NULL);
        if (!mysql_real_connect(conn, ip.c_str(), user.c_str(), password.c_str(), db.c_str(), port, nullptr, 0))
        {
            std::cerr << mysql_error(conn) << std::endl;
            return false;
        }
        return true;
    }
    return true;
}

MYSQL *connection::get()
{
    return conn;
}

connection::~connection()
{
    close();
}

void connection::close()
{
    if (conn)
    {
        mysql_close(conn);
        conn = nullptr;
    }
}
