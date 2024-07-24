#include "pool.h"
#include <iostream>

using pool = avant::sql::pool;
using connection = avant::sql::connection;

pool::pool()
{
}

pool::~pool()
{
}

void pool::init(int size,
                const std::string &ip,
                const std::string &user,
                const std::string &password,
                const std::string &db,
                const unsigned int timeout_seconds /*= 10*/,
                const unsigned int port /*= 3306*/)
{
    this->size = size;
    this->ip = ip;
    this->user = user;
    this->password = password;
    this->db = db;
    this->timeout_seconds = timeout_seconds;
    this->port = port;

    std::lock_guard<std::mutex> lock(this->mutex);
    for (int i = 0; i < size; i++)
    {
        std::shared_ptr<connection> conn = std::make_shared<connection>();

        mysql_options(conn->get(), MYSQL_OPT_CONNECT_TIMEOUT, &this->timeout_seconds);

        bool result = conn->connect(this->ip, this->user, this->password, this->db, this->port);
        if (result)
        {
            this->list.push_back(conn);
        }
    }
    this->cv.notify_all();
}

std::shared_ptr<connection> pool::get()
{
    std::shared_ptr<connection> ptr = nullptr;
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->cv.wait(lock,
                      [&]
                      {
                          return !this->list.empty();
                      });
        ptr = *this->list.begin();
        this->list.pop_front();
    }

    if (ptr->ping())
    {
        return ptr;
    }

    std::cout << __FILE__ << __LINE__ << "pool::get() ping failed" << std::endl;

    {
        std::lock_guard<std::mutex> lock(this->mutex);
        std::shared_ptr<connection> conn = std::make_shared<connection>();
        mysql_options(conn->get(), MYSQL_OPT_CONNECT_TIMEOUT, &this->timeout_seconds);
        bool result = conn->connect(this->ip, this->user, this->password, this->db, this->port);
        if (result)
        {
            std::cout << __FILE__ << __LINE__ << "poll::get() reconnect succ" << std::endl;
            ptr->close();
            return conn;
        }
        std::cout << __FILE__ << __LINE__ << "poll::get() reconnect failed" << std::endl;
    }

    return ptr;
}

void pool::back(std::shared_ptr<connection> ptr)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    this->list.push_back(ptr);
    this->cv.notify_all();
}

int pool::get_size()
{
    std::lock_guard<std::mutex> lock(this->mutex);
    return this->list.size();
}