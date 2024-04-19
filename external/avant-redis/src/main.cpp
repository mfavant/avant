#include <iostream>
#include <string>
#include "redis.h"
using namespace std;

using avant::redis::redis;

constexpr const char *ip = "";
constexpr const char *pwd = "";
constexpr int port = 6379;

int main(int argc, char **argv)
{
    redis m_redis(ip, pwd, port);
    try
    {
        m_redis.test();
    }
    catch (const std::runtime_error &err)
    {
        cout << err.what() << endl;
        return -1;
    }
    cout << "success" << endl;
    return 0;
}