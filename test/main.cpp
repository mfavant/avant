#include <iostream>
#include <string>
using namespace std;

int main(int argc, char **argv)
{
    std::string str;
    char buffer[10] = {0, '1', '2', '\0', 'a', 'b'};
    str.append(buffer, 10);
    std::cout << str.size() << std::endl;
    str.erase(0, 1);
    std::cout << str[0] << std::endl;
    std::cout << *str.c_str() << std::endl;
    return 0;
}