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
#include <cstdlib>


using namespace std;

#define MAXLINE 4096
#define UDP_PORT 9991

char tcp_input_buf[MAXLINE], udp_input_buf[MAXLINE];
string str_tcp_input_buf, str_udp_input_buf, str_tcp_output_buf, str_udp_output_buf;
int tcp_sockfd, udp_sockfd;
sockaddr_in udp_serv_addr, udp_cli_addr;
socklen_t udp_serv_len;
//int udp_port;

//__attribute__((packed))

void err_quit(string msg)
{
    perror(msg.c_str());
    exit(-1);
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

    if((tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) err_quit("tcp socket error");
    if((connect(tcp_sockfd, res->ai_addr, res->ai_addrlen)) < 0) err_quit("tcp connect error");
}

void tcp_read_until_and_stdout(string desire_str)
{
    bzero(tcp_input_buf, MAXLINE);
    str_tcp_input_buf = "";
    while(str_tcp_input_buf.find(desire_str) == std::string::npos)
    {
        bzero(tcp_input_buf, MAXLINE);
        if(read(tcp_sockfd, tcp_input_buf, MAXLINE) <= 0) err_quit("tcp read error");
        str_tcp_input_buf += tcp_input_buf;
    }
    cout << str_tcp_input_buf << "\n";
}

void tcp_write_and_stdout()
{
    str_tcp_output_buf += "\n";
    if(write(tcp_sockfd, str_tcp_output_buf.c_str(), str_tcp_output_buf.length()) < 0) err_quit("tcp write error");
    cout << str_tcp_output_buf << "\n";
    str_tcp_output_buf = "";
}

void udp_bind()
{
    bzero(&udp_cli_addr, sizeof(udp_cli_addr));
    udp_cli_addr.sin_family = AF_INET;
    udp_cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //udp_cli_addr.sin_port = htons(udp_port);
    udp_cli_addr.sin_port = htons(UDP_PORT);

    if((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) err_quit("udp socket error");
    if(bind(udp_sockfd, (sockaddr *)&udp_cli_addr, sizeof(udp_cli_addr)) < 0) err_quit("udp bind error");
}

void udp_lookup(char *host, char *service)
{
    addrinfo hints, *res;
    bzero(&hints, sizeof(hints));
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if(getaddrinfo(host, service, &hints, &res) != 0)  err_quit("getaddrinfo error");

    if(res == NULL) err_quit("res is NULL");

    memcpy(&udp_serv_addr, res->ai_addr, res->ai_addrlen);
}

void udp_recv_and_stdout()
{
    bzero(&udp_input_buf, MAXLINE);
    str_udp_input_buf = "";
    if(recvfrom(udp_sockfd, udp_input_buf, MAXLINE, 0, (sockaddr *)&udp_serv_addr, &udp_serv_len) <= 0) err_quit("udp recvfrom error");
    str_udp_input_buf += udp_input_buf;
    cout << str_udp_input_buf << "\n";
}

void udp_send_and_stdout()
{
    str_udp_output_buf += "\n";
    if(sendto(udp_sockfd, str_udp_output_buf.c_str(), str_udp_output_buf.length(), 0, (sockaddr *)&udp_serv_addr, sizeof(udp_serv_addr)) < 0) err_quit("udp sendto error");
    cout << str_udp_output_buf << "\n";
    str_udp_output_buf = "";
}

int main(int argc, char** argv)
{
    if(argc < 3) err_quit("Usage: ./level2 token2 <UDP-port-received-from-telnet-command>");

    /*
    tcp_connect(argv[1], argv[2]);
    tcp_read_until_and_stdout("Send me a UDP packet to start!");

    str_tcp_output_buf = str_tcp_input_buf;
    if(str_tcp_output_buf.find("nc -u token2 ") == std::string::npos) err_quit("can't find nc -u token2");
    str_tcp_output_buf = str_tcp_output_buf.substr(str_tcp_output_buf.find("nc -u token2 ") + 13);
    if(str_tcp_output_buf.find("\n") == std::string::npos) err_quit("\\n");
    str_tcp_output_buf = str_tcp_output_buf.substr(0, str_tcp_output_buf.find("\n"));
    udp_port = atoi(str_tcp_output_buf.c_str());
    */

    udp_lookup(argv[1], argv[2]);
    //udp_port = atoi(argv[2]);
    udp_bind();

    str_udp_output_buf = "test";
    udp_send_and_stdout();
    
    while(1)
    {
        udp_recv_and_stdout();
        str_udp_output_buf = str_udp_input_buf.substr(0, str_udp_input_buf.length() - 1);
        udp_send_and_stdout();
    }


    return 0;
}