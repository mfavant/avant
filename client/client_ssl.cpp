// g++ -o client_ssl.exe client_ssl.cpp ../protocol/proto_res/*.pb.cc -lprotobuf -lssl -lcrypto
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
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../protocol/proto_res/proto_cmd.pb.h"
#include "../protocol/proto_res/proto_example.pb.h"
#include "../protocol/proto_res/proto_message_head.pb.h"

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

    ProtoPackage message;
    ProtoCSReqExample exampleReq;
    std::string send_str(argv[1]);
    exampleReq.set_testcontext(send_str);
    message.set_cmd(ProtoCmd::CS_REQ_EXAMPLE);

    // const char *server_ip = "61.171.51.135";
    const char *server_ip = "172.29.94.203";
    int server_port = 20023;

    while (true)
    {
        for (int loop = 0; loop < 1000; loop++)
        {
            int client_socket = -1;

            {
                int INET = AF_INET;
                if (is_ipv6(server_ip))
                {
                    INET = AF_INET6;
                }

                client_socket = socket(INET, SOCK_STREAM, 0);
                if (client_socket == -1)
                {
                    perror("Socket creation failed");
                    return -1;
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
                        return -1;
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
                        return -1;
                    }

                    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
                    {
                        perror("Connection failed");
                        close(client_socket);
                        return -1;
                    }
                }
            }

            SSL *ssl = SSL_new(ctx);
            SSL_set_fd(ssl, client_socket);
            SSL_connect(ssl);

            std::string body_str;
            body_str.resize(exampleReq.ByteSizeLong());
            google::protobuf::io::ArrayOutputStream output_stream_1(&body_str[0], body_str.size());
            google::protobuf::io::CodedOutputStream coded_output_1(&output_stream_1);
            exampleReq.SerializeToCodedStream(&coded_output_1);
            message.set_body(body_str);
            std::string data = message.SerializePartialAsString();

            uint64_t packageLen = data.size();

            for (int i = 0; i < 5; i++)
            {
                if (SSL_write(ssl, data.c_str(), data.size()) != data.size())
                {
                    perror("Send Head failed");
                    SSL_shutdown(ssl);
                    SSL_free(ssl);
                    close(client_socket);
                    return 1;
                }
                else
                {
                    printf("Send Head succ\n");
                }

                printf("Send all bytes package %ld body %ld all %ld\n", data.size(), body_str.size(), data.size() + body_str.size());
            }

            // sleep(4);

            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(client_socket);
        }
        sleep(4);
    }

    SSL_CTX_free(ctx);

    return 0;
}
