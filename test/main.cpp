#include <iostream>
#include <string>
using namespace std;

int main(int argc, char **argv)
{
    size_t bytes = 0;
    for (int loop = 0; loop < 1000000000; loop++)
    {
        std::string str;
        char buffer[1048576] = {1, 0, 1, 2, 3, 4, 5, 6, 7, 8};
        // 1MB
        str.append(buffer, 1048576);
        // for (int i = 0; i < 20; i++)
        // {
        //     str += std::string(buffer, 1000000);
        // }
        bytes += str.size();
        // for (char &ch : str)
        // {
        //     // std::cout << (int)ch << std::endl;
        // }
        if (loop % 100)
        {
            std::cout << "bytes " << bytes / 1048576 << "MB" << std::endl;
        }
    }
    return 0;
}