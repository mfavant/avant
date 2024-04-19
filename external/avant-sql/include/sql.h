#pragma once

#include <cstring>
#include <memory>
#include <mysql/mysql.h>
#include "connection.h"

namespace avant
{
    namespace sql
    {
        enum class value_type
        {
            DECIMAL = 0,
            TINY,
            SHORT,
            LONG,
            FLOAT,
            DOUBLE,
            NULL_TYPE,
            TIMESTAMP,
            LONGLONG,
            INT24,
            DATE,
            TIME,
            DATETIME,
            YEAR,
            NEWDATE,
            VARCHAR,
            BIT,
            TIMESTAMP2,
            DATETIME2,
            TIME2,
            TYPED_ARRAY,
            INVALID,
            BOOL,
            JSON,
            NEWDECIMAL,
            ENUM,
            SET,
            TINY_BLOB,
            MEDIUM_BLOB,
            LONG_BLOB,
            BLOB,
            VAR_STRING,
            STRING,
            GEOMETRY,
        };

        /**
         * @brief query result template
         *
         * @tparam bind_size
         */
        template <size_t bind_size>
        class result
        {
        public:
            result();
            ~result();
            void set_bind(size_t index, void *buffer, unsigned long size, value_type type);
            MYSQL_BIND *get_bind();
            size_t get_size();

        public:
            bool is_null[bind_size + 1]{0};
            unsigned long out_length[bind_size + 1]{0};

        private:
            MYSQL_BIND bind[bind_size + 1];
            size_t size{bind_size + 1};
        };

        template <size_t bind_size>
        result<bind_size>::result()
        {
            ::memset(bind, 0, sizeof(bind));
        }

        template <size_t bind_size>
        result<bind_size>::~result()
        {
        }

        template <size_t bind_size>
        size_t result<bind_size>::get_size()
        {
            return size;
        }

        template <size_t bind_size>
        MYSQL_BIND *result<bind_size>::get_bind()
        {
            return this->bind;
        }

        template <size_t bind_size>
        void result<bind_size>::set_bind(size_t index, void *buffer, unsigned long size, value_type type)
        {
            switch (type)
            {
            case value_type::DECIMAL:
                bind[index].buffer_type = MYSQL_TYPE_DECIMAL;
                break;
            case value_type::TINY:
                bind[index].buffer_type = MYSQL_TYPE_TINY;
                break;
            case value_type::SHORT:
                bind[index].buffer_type = MYSQL_TYPE_SHORT;
                break;
            case value_type::LONG:
                bind[index].buffer_type = MYSQL_TYPE_LONG;
                break;
            case value_type::FLOAT:
                bind[index].buffer_type = MYSQL_TYPE_FLOAT;
                break;
            case value_type::DOUBLE:
                bind[index].buffer_type = MYSQL_TYPE_DOUBLE;
                break;
            case value_type::NULL_TYPE:
                bind[index].buffer_type = MYSQL_TYPE_NULL;
                break;
            case value_type::TIMESTAMP:
                bind[index].buffer_type = MYSQL_TYPE_TIMESTAMP;
                break;
            case value_type::LONGLONG:
                bind[index].buffer_type = MYSQL_TYPE_LONGLONG;
                break;
            case value_type::INT24:
                bind[index].buffer_type = MYSQL_TYPE_INT24;
                break;
            case value_type::DATE:
                bind[index].buffer_type = MYSQL_TYPE_DATE;
                break;
            case value_type::TIME:
                bind[index].buffer_type = MYSQL_TYPE_TIME;
                break;
            case value_type::DATETIME:
                bind[index].buffer_type = MYSQL_TYPE_DATETIME;
                break;
            case value_type::YEAR:
                bind[index].buffer_type = MYSQL_TYPE_YEAR;
                break;
            case value_type::NEWDATE:
                bind[index].buffer_type = MYSQL_TYPE_NEWDATE;
                break;
            case value_type::VARCHAR:
                bind[index].buffer_type = MYSQL_TYPE_VARCHAR;
                break;
            case value_type::BIT:
                bind[index].buffer_type = MYSQL_TYPE_BIT;
                break;
            case value_type::TIMESTAMP2:
                bind[index].buffer_type = MYSQL_TYPE_TIMESTAMP2;
                break;
            case value_type::DATETIME2:
                bind[index].buffer_type = MYSQL_TYPE_DATETIME2;
                break;
            case value_type::TIME2:
                bind[index].buffer_type = MYSQL_TYPE_TIME2;
                break;
            case value_type::TYPED_ARRAY:
                bind[index].buffer_type = MYSQL_TYPE_TYPED_ARRAY;
                break;
            case value_type::INVALID:
                bind[index].buffer_type = MYSQL_TYPE_INVALID;
                break;
            case value_type::BOOL:
                bind[index].buffer_type = MYSQL_TYPE_BOOL;
                break;
            case value_type::JSON:
                bind[index].buffer_type = MYSQL_TYPE_JSON;
                break;
            case value_type::NEWDECIMAL:
                bind[index].buffer_type = MYSQL_TYPE_NEWDECIMAL;
                break;
            case value_type::ENUM:
                bind[index].buffer_type = MYSQL_TYPE_ENUM;
                break;
            case value_type::SET:
                bind[index].buffer_type = MYSQL_TYPE_SET;
                break;
            case value_type::TINY_BLOB:
                bind[index].buffer_type = MYSQL_TYPE_TINY_BLOB;
                break;
            case value_type::MEDIUM_BLOB:
                bind[index].buffer_type = MYSQL_TYPE_MEDIUM_BLOB;
                break;
            case value_type::LONG_BLOB:
                bind[index].buffer_type = MYSQL_TYPE_LONG_BLOB;
                break;
            case value_type::BLOB:
                bind[index].buffer_type = MYSQL_TYPE_BLOB;
                break;
            case value_type::VAR_STRING:
                bind[index].buffer_type = MYSQL_TYPE_VAR_STRING;
                break;
            case value_type::STRING:
                bind[index].buffer_type = MYSQL_TYPE_STRING;
                break;
            case value_type::GEOMETRY:
                bind[index].buffer_type = MYSQL_TYPE_GEOMETRY;
                break;
            default:
                bind[index].buffer_type = MYSQL_TYPE_BLOB;
                break;
            }
            bind[index].buffer = buffer;
            bind[index].buffer_length = size;
            bind[index].is_null = is_null + index;
            bind[index].length = out_length + index;
        }

        /**
         * @brief sql template
         *
         * @tparam bind_size
         */
        template <size_t bind_size>
        class sql
        {
        public:
            sql(std::weak_ptr<connection> conn, const std::string &sql);
            ~sql();
            template <size_t result_size>
            bool execute(result<result_size> &m_result);
            bool fetch();
            void set_bind(size_t index, void *buffer, size_t size, value_type type, bool is_null = false);

        public:
            unsigned long get_affected_rows();

        private:
            MYSQL_STMT *stmt{nullptr};
            MYSQL_BIND bind[bind_size + 1]{0};
            std::weak_ptr<connection> m_conn{nullptr};
            unsigned long affected_rows{0};
        };

        template <size_t bind_size>
        sql<bind_size>::sql(std::weak_ptr<connection> conn, const std::string &sql) : m_conn(conn)
        {
            auto m_conn_ptr = m_conn.lock();
            if ((stmt = mysql_stmt_init(m_conn_ptr->get())) == nullptr)
            {
                std::cout << "stmt == nullptr" << std::endl;
            }
            if (0 != mysql_stmt_prepare(stmt, sql.c_str(), sql.size()))
            {
                std::cout << mysql_stmt_error(stmt) << std::endl;
            }
        }

        template <size_t bind_size>
        sql<bind_size>::~sql()
        {
            mysql_stmt_close(stmt);
        }

        template <size_t bind_size>
        template <size_t result_size>
        bool sql<bind_size>::execute(result<result_size> &m_result)
        {
            if (m_conn.expired())
            {
                std::cerr << "sql m_conn. is expired" << std::endl;
                return false;
            }
            else
            {
                std::cout << " m_conn.lock()" << std::endl;
                auto m_conn_ptr = m_conn.lock();
                std::cout << "mysql_stmt_bind_param(stmt, bind)" << std::endl;
                if (bind_size >= 1)
                {
                    if (0 != mysql_stmt_bind_param(stmt, bind))
                    {
                        std::cout << mysql_stmt_error(stmt) << std::endl;
                        return false;
                    }
                }
                std::cout << "mysql_stmt_execute(stmt)" << std::endl;
                if (0 != mysql_stmt_execute(stmt))
                {
                    std::cout << mysql_stmt_error(stmt) << std::endl;
                    return false;
                }
                std::cout << "mysql_stmt_bind_result" << std::endl;
                if (result_size >= 1)
                {
                    if (0 != mysql_stmt_bind_result(stmt, m_result.get_bind()))
                    {
                        std::cout << mysql_stmt_error(stmt) << std::endl;
                        return false;
                    }
                }
                affected_rows = mysql_affected_rows(m_conn_ptr->get());
            }
            return true;
        }

        template <size_t bind_size>
        bool sql<bind_size>::fetch()
        {
            int res = mysql_stmt_fetch(stmt);
            if (res == 0)
            {
                return true;
            }
            else if (res == 1)
            {
                std::cout << "fetch end" << std::endl;
                return false;
            }
            else if (res == MYSQL_NO_DATA)
            {
                std::cout << "fetch result is no data" << std::endl;
                return false;
            }
            else if (res == MYSQL_DATA_TRUNCATED)
            {
                std::cout << "fetch result is MYSQL_DATA_TRUNCATED" << std::endl;
                return false;
            }
            return false;
        }

        template <size_t bind_size>
        void sql<bind_size>::set_bind(size_t index, void *buffer, unsigned long size, value_type type, bool is_null)
        {
            switch (type)
            {
            case value_type::DECIMAL:
                bind[index].buffer_type = MYSQL_TYPE_DECIMAL;
                break;
            case value_type::TINY:
                bind[index].buffer_type = MYSQL_TYPE_TINY;
                break;
            case value_type::SHORT:
                bind[index].buffer_type = MYSQL_TYPE_SHORT;
                break;
            case value_type::LONG:
                bind[index].buffer_type = MYSQL_TYPE_LONG;
                break;
            case value_type::FLOAT:
                bind[index].buffer_type = MYSQL_TYPE_FLOAT;
                break;
            case value_type::DOUBLE:
                bind[index].buffer_type = MYSQL_TYPE_DOUBLE;
                break;
            case value_type::NULL_TYPE:
                bind[index].buffer_type = MYSQL_TYPE_NULL;
                break;
            case value_type::TIMESTAMP:
                bind[index].buffer_type = MYSQL_TYPE_TIMESTAMP;
                break;
            case value_type::LONGLONG:
                bind[index].buffer_type = MYSQL_TYPE_LONGLONG;
                break;
            case value_type::INT24:
                bind[index].buffer_type = MYSQL_TYPE_INT24;
                break;
            case value_type::DATE:
                bind[index].buffer_type = MYSQL_TYPE_DATE;
                break;
            case value_type::TIME:
                bind[index].buffer_type = MYSQL_TYPE_TIME;
                break;
            case value_type::DATETIME:
                bind[index].buffer_type = MYSQL_TYPE_DATETIME;
                break;
            case value_type::YEAR:
                bind[index].buffer_type = MYSQL_TYPE_YEAR;
                break;
            case value_type::NEWDATE:
                bind[index].buffer_type = MYSQL_TYPE_NEWDATE;
                break;
            case value_type::VARCHAR:
                bind[index].buffer_type = MYSQL_TYPE_VARCHAR;
                break;
            case value_type::BIT:
                bind[index].buffer_type = MYSQL_TYPE_BIT;
                break;
            case value_type::TIMESTAMP2:
                bind[index].buffer_type = MYSQL_TYPE_TIMESTAMP2;
                break;
            case value_type::DATETIME2:
                bind[index].buffer_type = MYSQL_TYPE_DATETIME2;
                break;
            case value_type::TIME2:
                bind[index].buffer_type = MYSQL_TYPE_TIME2;
                break;
            case value_type::TYPED_ARRAY:
                bind[index].buffer_type = MYSQL_TYPE_TYPED_ARRAY;
                break;
            case value_type::INVALID:
                bind[index].buffer_type = MYSQL_TYPE_INVALID;
                break;
            case value_type::BOOL:
                bind[index].buffer_type = MYSQL_TYPE_BOOL;
                break;
            case value_type::JSON:
                bind[index].buffer_type = MYSQL_TYPE_JSON;
                break;
            case value_type::NEWDECIMAL:
                bind[index].buffer_type = MYSQL_TYPE_NEWDECIMAL;
                break;
            case value_type::ENUM:
                bind[index].buffer_type = MYSQL_TYPE_ENUM;
                break;
            case value_type::SET:
                bind[index].buffer_type = MYSQL_TYPE_SET;
                break;
            case value_type::TINY_BLOB:
                bind[index].buffer_type = MYSQL_TYPE_TINY_BLOB;
                break;
            case value_type::MEDIUM_BLOB:
                bind[index].buffer_type = MYSQL_TYPE_MEDIUM_BLOB;
                break;
            case value_type::LONG_BLOB:
                bind[index].buffer_type = MYSQL_TYPE_LONG_BLOB;
                break;
            case value_type::BLOB:
                bind[index].buffer_type = MYSQL_TYPE_BLOB;
                break;
            case value_type::VAR_STRING:
                bind[index].buffer_type = MYSQL_TYPE_VAR_STRING;
                break;
            case value_type::STRING:
                bind[index].buffer_type = MYSQL_TYPE_STRING;
                break;
            case value_type::GEOMETRY:
                bind[index].buffer_type = MYSQL_TYPE_GEOMETRY;
                break;
            default:
                bind[index].buffer_type = MYSQL_TYPE_BLOB;
                break;
            }
            bind[index].buffer = buffer;
            static bool static_is_null = true;
            if (is_null)
                bind[index].is_null = &static_is_null;
            else
                bind[index].is_null = nullptr;
            bind[index].buffer_length = size;
        }

        template <size_t bind_size>
        unsigned long sql<bind_size>::get_affected_rows()
        {
            return affected_rows;
        }
    }
}
