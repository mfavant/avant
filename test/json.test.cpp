#include <iostream>
#include <string>
#include "../external/avant-json/json.h"
#include "../external/avant-json/parser.h"

using namespace std;
using avant::json::json;
using avant::json::parser;

int main(int argc, char **argv)
{
    json json_obj;
    const std::string str = "{\"int\": 232, \"double\":-434.2, \"arr\":[23,43,1,43.3,{\"n\":{\"arr\":[12,32],\"obj\":{\"in_obj\":{}}}}]}";
    json_obj.parse(str);
    std::cout << json_obj.to_string() << std::endl;
    json json_bool_obj = false;
    std::cout << (json_bool_obj == false) << std::endl; // 1
    json json_int = 12ll;
    std::cout << (json_bool_obj == json_int) << std::endl; // 0
    return 0;
}
// g++ ../external/avant-json/json.cpp ../external/avant-json/parser.cpp json.test.cpp -o json.test.exe --std=c++17