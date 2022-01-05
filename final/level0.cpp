#include <iostream>
#include <cstdio>
#include <error.h>
#include <cstring>
#include <string>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>


using namespace std;

#define MAXLINE 4096

char tcp_input_buf[MAXLINE], udp_input_buf[MAXLINE];
string str_tcp_input_buf, str_udp_input_buf, str_tcp_output_buf, str_udp_output_buf;
int tcp_sockfd, udp_sockfd;

//__attribute__((packed))

void err_quit(string msg)
{
    perror(msg.c_str());
    exit(-1);
}

void tcp_read_until_and_stdout(string desire_str)
{
    bzero(tcp_input_buf, MAXLINE);
    str_tcp_input_buf = "";
    while(str_tcp_input_buf.find(desire_str) == std::string::npos)
    {
        bzero(tcp_input_buf, MAXLINE);
        if(read(tcp_sockfd, tcp_input_buf, MAXLINE) <= 0) err_quit("read error");
        str_tcp_input_buf += tcp_input_buf;
    }
    cout << str_tcp_input_buf;
}

void tcp_connect(char *host, char *service)
{
    addrinfo hints, *res;
    bzero(&hints, sizeof(hints));
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(host, service, &hints, &res) != 0)  err_quit("getaddrinfo error");

    if(res == NULL) err_quit("res is NULL");

    if((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) err_quit("socket error");
    if((connect(tcp_sockfd, res->ai_addr, res->ai_addrlen)) < 0) err_quit("connect error");
}

int main(int argc, char** argv)
{
    if(argc < 3) err_quit("Usage: ./level0 hello 10100");

    tcp_connect(argv[1], argv[2]);
    tcp_read_until_and_stdout("to me");

    str_tcp_output_buf = str_tcp_input_buf;
    if(str_tcp_output_buf.find("message `") != std::string::npos)
        str_tcp_output_buf = str_tcp_output_buf.substr(str_tcp_output_buf.find("message `") + 9);
    
    if(str_tcp_output_buf.find("'") != std::string::npos)
        str_tcp_output_buf = str_tcp_output_buf.substr(0, str_tcp_output_buf.find("'"));
    str_tcp_output_buf += "\n";

    write(tcp_sockfd, str_tcp_output_buf.c_str(), str_tcp_output_buf.length());

    while(1) tcp_read_until_and_stdout("\n");

    return 0;
}