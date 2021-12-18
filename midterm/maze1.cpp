#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <sstream>
#include <queue>

#define MAXLINE 4095
#define MAX_X 6
#define MAX_Y 15

using namespace std;

struct pos
{
    int x, y;
    string path = "";
};

int maze[MAX_X + 10][MAX_Y + 10] = {0};
pos beg_, end_;

struct sockaddr lookup_udp(const char *hostname, const char *service) {
    struct addrinfo hint, *res = NULL;

    bzero(&hint, sizeof(hint));
    hint.ai_flags = AI_CANONNAME;
    hint.ai_socktype = SOCK_DGRAM;

    getaddrinfo(hostname, service, &hint, &res);
    return res -> ai_addr[0];
}

bool test(pos &next, char dir)
{
    if(dir == 'w') next.x--;
    else if(dir == 's') next.x++;
    else if(dir == 'a') next.y--;
    else if(dir == 'd') next.y++;

    if(next.x < 0 || next.x > MAX_X || next.y < 0 || next.y > MAX_Y || maze[next.x][next.y] == -1) return false;
    return true;
}


void bfs()
{
    queue<pos> q;
    q.push(beg_);

    bool invai[MAX_X + 10][MAX_Y + 10] = {0};
    while(!q.empty())
    {
        pos cur = q.front(), next = cur;
        q.pop();
        
        if(invai[cur.x][cur.y]) continue;
        invai[cur.x][cur.y] = true;

        if(cur.x == end_.x && cur.y == end_.y)
        {
            end_ = cur;
            return;
        }
        
        if(test(next, 'w'))
        {
            next = cur;
            next.path += "w";
            next.x = cur.x - 1;
            q.push(next);
        }
        if(test(next, 's'))
        {
            next = cur;
            next.path += "s";
            next.x = cur.x + 1;
            q.push(next);
        }
        if(test(next, 'a'))
        {
            next = cur;
            next.path += "a";
            next.y = cur.y - 1;
            q.push(next);
        }
        if(test(next, 'd'))
        {
            next = cur;
            next.path += "d";
            next.y = cur.y + 1;
            q.push(next);
        }
    }
}


int main(int argc, char *argv[]) {
    if(argc < 3)
    {
        printf("Usage: ./a.out <challenge_name> <port_number>");
        return 0;
    }
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr servaddr = lookup_udp(argv[1], argv[2]);
    connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    char from_serv[MAXLINE + 100];
    string str_from_serv = "";

    while(str_from_serv.find("[16 steps left]") == std::string::npos)
    {
        bzero(from_serv, MAXLINE);
        read(sockfd, from_serv, MAXLINE);
        str_from_serv += from_serv;
    }

    cout << str_from_serv;

    str_from_serv = str_from_serv.substr(str_from_serv.find("steps.") + 8, (str_from_serv.find("[16 steps left]") - 1) - (str_from_serv.find("steps.") + 8));

    string one_line = "";
    for(int i = 0; i < 7; i++)
    {
        one_line = str_from_serv.substr(0, str_from_serv.find("\n") + 1);
        str_from_serv = str_from_serv.substr(str_from_serv.find("\n") + 1);
        for(int j = 0; j < (int)one_line.length(); j++)
        {
            if(one_line.at(j) == '#') maze[i][j] = -1;
            else
            {
                maze[i][j] = 1;
                if(one_line.at(j) == '*') beg_ = {i, j};
                else if(one_line.at(j) == 'E') end_ = {i, j};
            } 
        }
    }

    bfs();
    end_.path += "\n";
    write(sockfd, end_.path.c_str(), end_.path.length());

    bzero(from_serv, MAXLINE);
    while(read(sockfd, from_serv, MAXLINE) != 0)
    {
        cout << from_serv;
        bzero(from_serv, MAXLINE);
    }

	return 0;
}