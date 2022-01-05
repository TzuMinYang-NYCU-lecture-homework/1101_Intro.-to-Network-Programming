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
#include <queue>
#include <algorithm>

using namespace std;

#define MAXLINE 4096
#define UDP_PORT 9991
#define MAX_X 11
#define MAX_Y 7

char udp_input_buf[MAXLINE];
string str_udp_input_buf, str_udp_output_buf;
int udp_sockfd;
sockaddr_in udp_serv_addr, udp_cli_addr;
socklen_t udp_serv_len;
int end_x, end_y, beg_x, beg_y;
int map[MAX_Y + 10][MAX_X + 10] = {0};

//__attribute__((packed))

struct pos
{
    int x, y;
    string path = "";
};


void err_quit(string msg)
{
    perror(msg.c_str());
    exit(-1);
}

void load_maze()
{
    string oneline = "";
    for(int j = 0; j < MAX_Y; j++)
    {
        if(str_udp_output_buf.find("\n") == std::string::npos) oneline = str_udp_output_buf.substr(0, str_udp_output_buf.length() - 1);
        else oneline = str_udp_output_buf.substr(0, str_udp_output_buf.find("\n"));

        if(oneline == "") return;

        if(str_udp_output_buf.find("\n") == str_udp_output_buf.length() - 1)
            str_udp_output_buf = "";
        else
            str_udp_output_buf = str_udp_output_buf.substr(str_udp_output_buf.find("\n") + 1);

        for(int i = 0; i < MAX_X; i++)
        {
            if(oneline.at(i) == '#') map[j][i] = 0;
            else map[j][i] = 1;

            if(oneline.at(i) == '*')
            {
                beg_x = j;
                beg_y = i;
            }
            else if(oneline.at(i) == 'E')
            {
                end_x = j;
                end_y = i;
            }
        }
    }
}

bool test(int x, int y)
{
    if(x >= MAX_Y || x < 0 || y >= MAX_X || y < 0) return false;
    return true;
}

void bfs()
{
    queue<pos> q;
    q.push({beg_x, beg_y, ""});
    int is_visited[MAX_Y + 10][MAX_X + 10] = {0};
    is_visited[beg_x][beg_y] = 1;
    
    while(!q.empty())
    {
        pos cur = q.front(), next;
        q.pop();

        if(cur.x == end_x && cur.y == end_y)
        {
            str_udp_output_buf = cur.path + "\n";
            return;
        }
        
        next = cur;
        next.x--;
        if(is_visited[next.x][next.y] == 0 && test(next.x, next.y))
        {
            next.path += "W";
            q.push(next);
            is_visited[next.x][next.y] = 1;
        }

        next = cur;
        next.x++;
        if(is_visited[next.x][next.y] == 0 && test(next.x, next.y))
        {
            next.path += "S";
            q.push(next);
            is_visited[next.x][next.y] = 1;
        }
        
        next = cur;
        next.y--;
        if(is_visited[next.x][next.y] == 0 && test(next.x, next.y))
        {
            next.path += "A";
            q.push(next);
            is_visited[next.x][next.y] = 1;
        }

        next = cur;
        next.y++;
        if(is_visited[next.x][next.y] == 0 && test(next.x, next.y))
        {
            next.path += "D";
            q.push(next);
            is_visited[next.x][next.y] = 1;
        }
    }
}

void udp_bind()
{
    bzero(&udp_cli_addr, sizeof(udp_cli_addr));
    udp_cli_addr.sin_family = AF_INET;
    udp_cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
    if(argc < 3) err_quit("Usage: ./level3 cmaze1 <UDP-port-received-from-telnet-command>");

    udp_lookup(argv[1], argv[2]);
    udp_bind();

    str_udp_output_buf = "test";
    udp_send_and_stdout();
    udp_recv_and_stdout();

    str_udp_output_buf = "M";
    udp_send_and_stdout();
    udp_recv_and_stdout();
    str_udp_output_buf = str_udp_input_buf;

    
    load_maze();
    bfs();

    cout << str_udp_output_buf;

    udp_send_and_stdout();


    return 0;
}