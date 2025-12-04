// g++ -o client_ssl.exe client_ssl.cpp ../../protocol/proto_res/*.pb.cc -lprotobuf -lssl -lcrypto
#include <iostream>
#include <string>
#include <sstream>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/message.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <endian.h>
#include <thread>
#include <chrono>

#include "../../protocol/proto_res/proto_cmd.pb.h"
#include "../../protocol/proto_res/proto_example.pb.h"
#include "../../protocol/proto_res/proto_message_head.pb.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

//             std::string body_str;
//             body_str.resize(exampleReq.ByteSizeLong());
//             google::protobuf::io::ArrayOutputStream output_stream_1(&body_str[0], body_str.size());
//             google::protobuf::io::CodedOutputStream coded_output_1(&output_stream_1);
//             exampleReq.SerializeToCodedStream(&coded_output_1);
//             message.set_protocol(body_str);
//             std::string data = message.SerializeAsString();

//             uint64_t packageLen = data.size();

//             for (int i = 0; i < 5; i++)
//             {
//                 if (SSL_write(ssl, data.c_str(), data.size()) != data.size())
//                 {
//                     perror("Send Head failed");
//                     SSL_shutdown(ssl);
//                     SSL_free(ssl);
//                     close(client_socket);
//                     return 1;
//                 }
//                 else
//                 {
//                     printf("Send Head succ\n");
//                 }

//                 printf("Send all bytes package %ld body %ld all %ld\n", data.size(), body_str.size(), data.size() + body_str.size());
//             }

//             // sleep(4);

//             SSL_shutdown(ssl);
//             SSL_free(ssl);
//             close(client_socket);
//         }
//         sleep(4);
//     }

//     SSL_CTX_free(ctx);

//     return 0;
// }

static bool is_ipv6(std::string ip)
{
    if (std::string::npos != ip.find_first_of('.'))
    {
        return false;
    }
    return true;
}

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        std::cout << "arg<2" << std::endl;
        return 1;
    }

    // const char *server_ip = "61.171.51.135";
    const char *server_ip = "127.0.0.1";
    int server_port = ::atoi(argv[1]);

    int n;
    bool stop_flag = false;

    constexpr int thread_count = 4;

    constexpr int client_cnt = 1;

    constexpr int pingpong_cnt = 1;
    // 350KB/s

    {
        SSL_library_init();
        SSL_load_error_strings();
    }
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx)
    {
        std::cout << "ctx null" << std::endl;
        exit(-1);
    }

    // 1024KB
    for (int loop = 0; loop < thread_count; loop++)
    {
        std::thread m_thread(
            [server_port, server_ip, argv, loop, &stop_flag, &client_cnt, &pingpong_cnt, ctx]()
            {
                ProtoPackage message;
                ProtoCSReqExample exampleReq;
                constexpr uint send_all_string_bytes = 8;
                std::string send_str(send_all_string_bytes, 'a');
                // send_str = argv[1];
                exampleReq.set_testcontext(send_str);
                message.set_cmd(ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE);
                // 30MB * 2000 = 60 000MB = 60GB
                uint64_t send_size = 0;
                uint64_t recv_size = 0;
                uint64_t last_print_size = 0;
                uint64_t last_time = std::time(nullptr);

                int client_socket_arr[client_cnt]{0};
                std::vector<SSL *> ssl;

                for (int client_idx = 0; client_idx < client_cnt; client_idx++)
                {
                    ssl.push_back(SSL_new(ctx));
                    int &client_socket = client_socket_arr[client_idx];

                    int INET = AF_INET;
                    if (is_ipv6(server_ip))
                    {
                        INET = AF_INET6;
                    }

                    client_socket = socket(INET, SOCK_STREAM, 0);
                    if (client_socket == -1)
                    {
                        perror("Socket creation failed");
                        return;
                    }

                    if (INET == AF_INET)
                    {
                        sockaddr_in server_address;
                        server_address.sin_family = AF_INET;
                        server_address.sin_port = htons(server_port);
                        server_address.sin_addr.s_addr = inet_addr(server_ip);

                        if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
                        {
                            perror("Connection failed");
                            close(client_socket);
                            return;
                        }
                    }
                    else
                    {
                        sockaddr_in6 server_address;
                        server_address.sin6_family = AF_INET6;
                        server_address.sin6_port = htons(server_port);
                        if (inet_pton(AF_INET6, server_ip, &server_address.sin6_addr) <= 0)
                        {
                            perror("Invalid IPV6 address");
                            close(client_socket);
                            return;
                        }

                        if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
                        {
                            perror("Connection failed");
                            close(client_socket);
                            return;
                        }
                    }

                    SSL_set_fd(ssl[client_idx], client_socket);
                    SSL_connect(ssl[client_idx]);
                }

                while (true)
                {
                    if (stop_flag)
                    {
                        for (int i = 0; i < client_cnt; i++)
                        {
                            if (client_socket_arr[i] > 0)
                            {
                                close(client_socket_arr[i]);
                            }
                        }
                        return;
                    }
                    for (int client_idx = 0; client_idx < client_cnt; client_idx++)
                    {
                        int &client_socket = client_socket_arr[client_idx];
                        if (client_socket <= 0)
                        {
                            continue;
                        }
                        std::string str;
                        exampleReq.SerializeToString(&str);
                        message.set_protocol(str);
                        std::string data;
                        message.SerializeToString(&data);

                        uint64_t packageLen = data.size();
                        packageLen = ::htobe64(packageLen);
                        data.insert(0, (char *)&packageLen, sizeof(packageLen));

                        // every client pingpong 1
                        for (int i = 0; i < pingpong_cnt; i++)
                        {
                            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                            std::chrono::milliseconds send_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
                            // std::cout << "ping" << std::endl;
                            uint64_t sended = 0;
                            while (data.size() != sended)
                            {
                                // std::cout<<"send.. data.size() - sended = "<< ( data.size() - sended) <<std::endl;
                                int len = SSL_write(ssl[client_idx], data.c_str() + sended, data.size() - sended);
                                if (len <= 0)
                                {
                                    perror("Send Head failed");
                                    close(client_socket);
                                    client_socket = 0;
                                    return;
                                }
                                else
                                {
                                    sended += len;
                                }
                                std::this_thread::sleep_for(std::chrono::microseconds(1));
                            }
                            send_size++;
                            // printf("Send all bytes package %ld body %ld all %ld\n", data.size(), body_str.size(), data.size() + body_str.size());
                            // std::cout << "ping OK" << std::endl;

                            // block read res package
                            char buffer[5024000]{0};
                            uint64_t data_len = 0;

                            // std::cout << "pong" << std::endl;
                            // std::cout <<"recv.."<<std::endl;
                            // std::cout << "reading=======================================================================>" << std::endl;

                            std::this_thread::sleep_for(std::chrono::microseconds(1));
                            while (true)
                            {
                                int recv_len = SSL_read(ssl[client_idx], buffer + data_len, 5024000);
                                if (recv_len == 0)
                                {
                                    close(client_socket);
                                    client_socket = 0;
                                    break;
                                }
                                if (recv_len == -1)
                                {
                                    std::cout << "recv err" << std::endl;
                                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                    continue;
                                }
                                // 6000 * 20 * 2MB = 6 000 * 2 0 * 2=240 000MB = 240GB
                                // printf("data_len[%u] += recv_len[%d]\n", data_len, recv_len);
                                data_len += recv_len;

                                if (data_len < sizeof(uint64_t))
                                {
                                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                                    continue;
                                }

                                uint64_t packSize = ::be64toh(*((uint64_t *)buffer));

                                if (data_len < packSize + sizeof(uint64_t))
                                {
                                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                                    continue;
                                }

                                ProtoPackage protoPackage;
                                // std::cout << "ParseFromArray recv_len[" << recv_len << "]"
                                //           << "data_len=" << data_len << std::endl;
                                if (!protoPackage.ParseFromArray(buffer + sizeof(uint64_t), packSize))
                                {
                                    // std::cout << "Failed" << std::endl;
                                    // std::cout << "ParseFromArray recv_len[" << recv_len << "]"
                                    //           << "data_len=" << data_len << std::endl;
                                    // std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                    std::cout << "ParseFromArrayErr" << std::endl;
                                    return;
                                }
                                else
                                {
                                    if (protoPackage.cmd() == ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE)
                                    {
                                        ProtoCSResExample exampleRes;
                                        if (exampleRes.ParseFromString(protoPackage.protocol()))
                                        {
                                            recv_size++;

                                            uint64_t now_time = std::time(nullptr);
                                            if ((recv_size - last_print_size) >= 100)
                                            {
                                                // printf("RES(thread[%d] seconds[%lu] send_size[%lu],recv_size[%lu])\n", loop, now_time - last_time, send_size, recv_size);
                                                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                                                std::chrono::milliseconds recv_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
                                                std::cout << "pingpong " << recv_ms.count() - send_ms.count() << " ms cnt " << recv_size << "  " << send_all_string_bytes << " bytes " << std::endl;
                                                std::cout << exampleRes.testcontext() << std::endl;
                                                last_print_size = recv_size;
                                            }
                                            last_time = now_time;
                                            break;
                                        }
                                        else
                                        {
                                            std::cout << "exampleRes.ParseFromString(protoPackage.body()) failed" << std::endl;
                                            close(client_socket);
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        std::cout << "res package cmd is not CS_RES_EXAMPLE" << std::endl;
                                        close(client_socket);
                                        client_socket = 0;
                                        break;
                                    }
                                }
                            }

                            // std::cout << "pong OK" << std::endl;
                            // std::cout << "pingpong OK" << std::endl;
                        }

                        std::this_thread::sleep_for(std::chrono::microseconds(5));
                    }
                }
            });
        m_thread.detach();
    }

    std::cin >> n;
    stop_flag = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    return 0;
}
