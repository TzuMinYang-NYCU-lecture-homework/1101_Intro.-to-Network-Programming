#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <unistd.h>

using namespace std;

#define MAXLINE 4000

char tcp_input_buffer[MAXLINE], udp_input_buffer[MAXLINE];
string str_tcp_input_buffer, str_udp_input_buffer;
int tcp_sockfd, udp_sockfd;

void err_quit(string msg)
{
    cout << msg << "\n";
    exit(-1);
}

void lookup(const char* host, const char* service, string tcp_or_udp, bool is_bind, sockaddr &addr)
{
    addrinfo hint, *res = nullptr;
    bzero(&hint, sizeof(hint));
    hint.ai_flags = is_bind ? AI_PASSIVE : AI_CANONNAME;
    hint.ai_family = AF_INET;
    hint.ai_socktype = tcp_or_udp == "tcp" ? SOCK_STREAM : SOCK_DGRAM;

    if(getaddrinfo(host, service, &hint, &res) != 0) err_quit("getaddrinfo");

    if(res == nullptr) err_quit("nothing");
    addr = res->ai_addr[0];
}

void read_until(string desire_str)
{
    bzero(tcp_input_buffer, MAXLINE);
    str_tcp_input_buffer = "";
    while(str_tcp_input_buffer.find(desire_str) == std::string::npos)
    {
        bzero(tcp_input_buffer, MAXLINE);
        read(tcp_sockfd, tcp_input_buffer, MAXLINE);
        str_tcp_input_buffer += tcp_input_buffer;
    }
    cout << str_tcp_input_buffer;
}

int main(int argc, char** argv)
{
    if(argc < 3) err_quit("Usage: ");

    sockaddr tcp_serv_addr, udp_serv_addr;
    sockaddr_in udp_cli_addr;
    lookup(argv[1], argv[2], "tcp", false, tcp_serv_addr);
    lookup(argv[1], argv[2], "udp", false, udp_serv_addr);
    //lookup(NULL, argv[2], "udp", true, udp_cli_addr);

    if((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) err_quit("tcp socket");
    if((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) err_quit("udp socket");

    if(connect(tcp_sockfd, (sockaddr*)&tcp_serv_addr, sizeof(tcp_serv_addr)) < 0) err_quit("connect");

    bzero(&udp_cli_addr, sizeof(udp_cli_addr));
    udp_cli_addr.sin_family = AF_INET;
    udp_cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_cli_addr.sin_port = htons(9999);

    if(bind(udp_sockfd, (sockaddr*)&udp_cli_addr, sizeof(udp_cli_addr)) < 0) err_quit("udp bind");

    //read_until("\n");

    char *test = "test";
    sendto(udp_sockfd, test, strlen(test), 0, (sockaddr *)&udp_serv_addr, sizeof(udp_serv_addr));
    write(tcp_sockfd, test, strlen(test));

    sleep(10);

    return 0;
}