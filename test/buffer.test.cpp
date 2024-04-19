#include <iostream>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include "../src/buffer/buffer.h"
using namespace std;
using avant::buffer::buffer;

int main(int argc, char **argv)
{
    const char *response = "HTTP/1.1 200 OK\r\nServer: avant\r\nContent-Type: text/json;\r\n\r\n{\"server\":\"avant\"}";
    buffer m_buffer(1024);
    std::cout << m_buffer.write(response, strlen(response)) << std::endl;
    char charbuf[1024];
    int len = m_buffer.read(charbuf, 1023);
    charbuf[len] = 0;
    while (len > 0)
    {
        std::cout << charbuf << std::endl;
        len = m_buffer.read(charbuf, 1023);
        charbuf[len] = 0;
    }

    // char mybuffer[1024] = {0};
    // buffer m_buffer(19);
    // sleep(2);
    // cout << m_buffer.get_last_read_gap() << endl;  // 2
    // cout << m_buffer.get_last_write_gap() << endl; // 2

    // try
    // {
    //     cout << m_buffer.write("1234567890", 10) << endl; // 10
    // }
    // catch (const std::runtime_error &e)
    // {
    //     cout << e.what() << endl;
    // }

    // cout << m_buffer.get_last_write_gap() << endl; // 0

    // try
    // {
    //     cout << m_buffer.write("1234567890", 10) << endl; // none
    // }
    // catch (const std::runtime_error &e)
    // {
    //     cout << e.what() << endl; // should_add_size + m_size > m_limit_max
    // }

    // try
    // {
    //     cout << m_buffer.write("123456789", 9) << endl; // 9
    // }
    // catch (const std::runtime_error &e)
    // {
    //     cout << e.what() << endl;
    // }

    // try
    // {
    //     cout << m_buffer.read(mybuffer, 0) << endl; // none
    // }
    // catch (const std::runtime_error &e)
    // {
    //     cout << e.what() << endl; // dest == nullptr || size == 0
    // }

    // try
    // {
    //     cout << m_buffer.read(mybuffer, 10) << endl; // 10
    //     printf("%s\n", mybuffer);                    // 1234567890
    // }
    // catch (const std::runtime_error &e)
    // {
    //     cout << e.what() << endl;
    // }

    // try
    // {
    //     mybuffer[m_buffer.read(mybuffer, 10)] = 0;
    //     printf("%s\n", mybuffer); // 123456789
    // }
    // catch (const std::runtime_error &e)
    // {
    //     cout << e.what() << endl;
    // }

    // sleep(3);
    // cout << m_buffer.get_last_read_gap() << endl; // 3
    // const char *str1 = "sudygvuygvuyergfakshjdkhskfhaldsssssdfhsjkdahwueiyeuuuuuuuuuuuuuuuuuuuuuuuuuuuuuufjhsdkajfgjhdsvbgahjsdv";

    // try
    // {
    //     cout << m_buffer.write(str1, strlen(str1)) << endl;
    // }
    // catch (const std::runtime_error &e)
    // {
    //     cout << e.what() << endl; // should_add_size + m_size > m_limit_max
    // }

    // const char *str2 = "abcdefghij";

    // try
    // {
    //     cout << m_buffer.write(str2, strlen(str2)) << endl; // 10
    // }
    // catch (const std::runtime_error &e)
    // {
    //     cout << e.what() << endl;
    // }

    // try
    // {
    //     mybuffer[m_buffer.read(mybuffer, 10)] = 0;
    //     printf("%s\n", mybuffer); // abcdefghij
    // }
    // catch (const std::runtime_error &e)
    // {
    //     cout << e.what() << endl;
    // }

    return 0;
}

/*
2
2
10
0
should_add_size + m_size > m_limit_max
9
dest == nullptr || size == 0
10
1234567890
123456789
3
should_add_size + m_size > m_limit_max
10
abcdefghij
*/

// g++ buffer.test.cpp ../src/buffer/buffer.cpp -o buffer.test.exe -I"../src/" && ./buffer.test.exe
