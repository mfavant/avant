#include <stdexcept>
#include <iostream>
#include <string>
#include <cstring>
#include "redis.h"
#include "hiredis.h"

using namespace avant::redis;

typedef struct
{
    int id;
    char name[50];
    double value;
} CustomData;

// 存储二进制结构体数据到Redis
static void StoreStructToRedis(redisContext *conn, const char *key, const CustomData *data)
{
    redisReply *reply = (redisReply *)redisCommand(conn, "SET %s %b", key, data, sizeof(CustomData));
    if (reply == NULL)
    {
        throw std::runtime_error("redis set command error");
    }
    freeReplyObject(reply);
}

static void FetchStructFromRedis(redisContext *conn, const char *key, CustomData *data)
{
    redisReply *reply = (redisReply *)redisCommand(conn, "GET %s", key);
    if (reply != NULL && reply->type == REDIS_REPLY_STRING)
    {
        memcpy(data, reply->str, sizeof(CustomData));
    }
    else
    {
        throw std::runtime_error("redis get command error or key not found");
    }
    freeReplyObject(reply);
}

redis::redis(const std::string &host, const std::string &pwd, const int &port) : host(host), port(port), pwd(pwd)
{
}

redis::~redis()
{
}

void redis::test()
{
    redisContext *context = redisConnect(host.c_str(), port);
    if (context == nullptr || context->err)
    {
        if (context)
        {
            std::string msg("connect error: ");
            std::string err(context->errstr);
            redisFree(context);
            throw std::runtime_error(msg + err);
        }
        else
        {
            throw std::runtime_error("error: can not allocate redisContext");
        }
    }
    // AUTH
    std::string command = "AUTH " + this->pwd;
    redisReply *reply = nullptr;
    reply = (redisReply *)redisCommand(context, command.c_str());
    if (reply == nullptr)
    {
        redisFree(context);
        throw std::runtime_error("error: auth error");
    }
    freeReplyObject(reply);
    // SELECT
    command = "SELECT 0";
    reply = (redisReply *)redisCommand(context, command.c_str());
    if (reply == nullptr)
    {
        redisFree(context);
        throw std::runtime_error("error: select 0 error");
    }
    freeReplyObject(reply);
    // SET example
    std::string key = "mkey";
    std::string value = "mValue";
    command = "SET " + key + " " + value;
    reply = (redisReply *)redisCommand(context, command.c_str());
    if (reply == nullptr)
    {
        redisFree(context);
        throw std::runtime_error("error: set key-value error");
    }
    freeReplyObject(reply);
    // get example
    command = "GET " + key;
    reply = (redisReply *)redisCommand(context, command.c_str());
    if (reply == nullptr)
    {
        redisFree(context);
        throw std::runtime_error("error: get key-value error");
    }

    std::cout << reply->str << std::endl;

    freeReplyObject(reply);

    try
    {
        CustomData obj;
        bzero(&obj, sizeof(obj));
        obj.id = 1;
        obj.value = 102.2;
        strcpy(obj.name, "hello");
        StoreStructToRedis(context, "obj", &obj);
        bzero(&obj, sizeof(obj));
        FetchStructFromRedis(context, "obj", &obj);
        std::cout << obj.id << " " << obj.name << " " << obj.value << std::endl;
    }
    catch (std::runtime_error &e)
    {
        std::cout << e.what() << std::endl;
    }

    // disconnect
    redisFree(context);
}