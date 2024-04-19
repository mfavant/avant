#include "query.h"

#include <iostream>

using namespace avant::sql;
using std::shared_ptr;

query::query(std::shared_ptr<connection> conn) : conn(conn)
{
}

query::~query()
{
}

void query::execute(const std::string &sql)
{
    MYSQL_RES *res = nullptr;
    if (mysql_query(conn->get(), sql.c_str()))
    {
        std::cerr << mysql_error(conn->get()) << std::endl;
    }
    else
    {
        res = mysql_use_result(conn->get());
    }
    if (res)
    {
        mysql_free_result(res);
    }
}

uint64_t query::insert(const std::string &sql)
{
    if (mysql_query(conn->get(), sql.c_str()))
    {
        std::cerr << mysql_error(conn->get()) << std::endl;
        return 0;
    }
    else
    {
        return mysql_affected_rows(conn->get());
    }
}

uint64_t query::del(const std::string &sql)
{
    if (mysql_query(conn->get(), sql.c_str()))
    {
        std::cerr << mysql_error(conn->get()) << std::endl;
        return 0;
    }
    else
    {
        return mysql_affected_rows(conn->get());
    }
}

void query::select(const std::string &sql)
{
    MYSQL_RES *res = nullptr;
    MYSQL_ROW row;
    if (mysql_query(conn->get(), sql.c_str()))
    {
        std::cerr << mysql_error(conn->get()) << std::endl;
    }
    else
    {
        res = mysql_use_result(conn->get());
        const unsigned fields_num = mysql_num_fields(res);
        {
            MYSQL_FIELD *fields;
            fields = mysql_fetch_field(res);
            for (unsigned i = 0; i < res->field_count; ++i)
            {
                if (fields[i].name)
                    std::cout << fields[i].name << " ";
                else
                    std::cout << "   ";
            }
            std::cout << std::endl;
        }
        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            const unsigned long *const lengths = mysql_fetch_lengths(res);
            for (unsigned i = 0; i < fields_num; i++)
            {
                if (row[i])
                    std::cout << row[i] << "(" << lengths[i] << ") ";
                else
                    std::cout << "null ";
            }
            std::cout << std::endl;
        }
    }
    if (res)
    {
        mysql_free_result(res);
    }
}

uint64_t query::update(const std::string &sql)
{
    if (mysql_query(conn->get(), sql.c_str()))
    {
        std::cerr << mysql_error(conn->get()) << std::endl;
        return 0;
    }
    else
    {
        return mysql_affected_rows(conn->get());
    }
}
