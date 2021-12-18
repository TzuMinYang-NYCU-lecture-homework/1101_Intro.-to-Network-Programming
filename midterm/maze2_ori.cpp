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

using namespace std;

struct pos
{
    int x, y;
    string path = "";
};

int maze[150][100] = {0};
pos beg_, end_;

struct sockaddr lookup_udp(const char *hostname, const char *service) {
    int i, err;
    struct addrinfo hint, *r, *res = NULL;
    bzero(&hint, sizeof(hint));
    hint.ai_flags = AI_CANONNAME;
    hint.ai_socktype = SOCK_DGRAM;
    if((err = getaddrinfo(hostname, service, &hint, &res)) != 0)
        printf("Error [%s]: %s\n", hostname, gai_strerror(err));

    for(i = 0, r = res; r != NULL; i++, r = r->ai_next)
        return r -> ai_addr[0];
}

int test(pos cur, char dir)
{
    if(dir == 'w') cur.x--;
    else if(dir == 's') cur.x++;
    else if(dir == 'a') cur.y--;
    else if(dir == 'd') cur.y++;

    if(cur.x < 0 || cur.x > 20 || cur.y < 0 || cur.y > 78 || maze[cur.x][cur.y] == -1) return 0;
    return 1;
}


void bfs()
{
    queue<pos> q;
    q.push(beg_);

    bool invai[150][100] = {0};
    while(!q.empty())
    {
        pos cur = q.front(), next_cur;
        q.pop();
        
        if(invai[cur.x][cur.y]) continue;

        invai[cur.x][cur.y] = 1;

        //cout << cur.x << " " << cur.y << "\n";

        if(cur.x == end_.x && cur.y == end_.y)
        {
            end_ = cur;
            return;
        }
        
        if(test(cur, 'w'))
        {
            next_cur = cur;
            next_cur.path += "w";
            next_cur.x = cur.x - 1;
            q.push(next_cur);
        }
        if(test(cur, 's'))
        {
            next_cur = cur;
            next_cur.path += "s";
            next_cur.x = cur.x + 1;
            q.push(next_cur);
        }
        if(test(cur, 'a'))
        {
            next_cur = cur;
            next_cur.path += "a";
            next_cur.y = cur.y - 1;
            q.push(next_cur);
        }
        if(test(cur, 'd'))
        {
            next_cur = cur;
            next_cur.path += "d";
            next_cur.y = cur.y + 1;
            q.push(next_cur);
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

    while(str_from_serv.find("Enter") == std::string::npos)
    {
        bzero(from_serv, MAXLINE);
        read(sockfd, from_serv, MAXLINE);
        str_from_serv += from_serv;
    }

    cout << str_from_serv;

    str_from_serv = str_from_serv.substr(str_from_serv.find("#"), (str_from_serv.find("Enter") - 1) - (str_from_serv.find("#")));
    //cout << str_from_serv;

    string one_line = "";
    for(int i = 0; i < 21; i++)
    {
        one_line = str_from_serv.substr(0, str_from_serv.find("\n") + 1);
        str_from_serv = str_from_serv.substr(str_from_serv.find("\n") + 1);
        for(int j = 0; j < one_line.length(); j++)
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

/*
    cout << beg_.x << " " << beg_.y << " " << end_.x << " " << end_.y << "\n";
    

    for(int i = 0; i < 21; i++)
    {
        for(int j = 0; j < 79; j++)
            cout << maze[i][j] << " ";
        cout << "\n";
    }*/

    bfs();
    //cout << "!!";
    end_.path += "\n";
    //cout << end_.path;
    write(sockfd, end_.path.c_str(), end_.path.length());

    bzero(from_serv, MAXLINE);
    while(read(sockfd, from_serv, MAXLINE) != 0)
    {
        cout << from_serv;
        bzero(from_serv, MAXLINE);
    }

    /*
    string useless, repeat_str = "";

    if(str_from_serv.find("#") != std::string::npos)
    {
        str_from_serv = str_from_serv.substr(str_from_serv.find("#"));
        repeat_str = str_from_serv.substr(str_from_serv.find(":") + 2);
        write(sockfd, repeat_str.c_str(), repeat_str.length());
        round--;
    }

    
    while(round--)
    {
        str_from_serv = "";
        bzero(from_serv, MAXLINE);
        read(sockfd, from_serv, MAXLINE);
        str_from_serv = from_serv;
        cout << from_serv;

        while(str_from_serv.find(":") == std::string::npos)
        {
            bzero(from_serv, MAXLINE);
            read(sockfd, from_serv, MAXLINE);
            str_from_serv = from_serv;
            cout << from_serv;
        }

        
        repeat_str = str_from_serv.substr(str_from_serv.find(":") + 2);

        if(repeat_str.at(repeat_str.length() - 1) != '\n') repeat_str += "\n";
        write(sockfd, repeat_str.c_str(), repeat_str.length());
    }

    bzero(from_serv, MAXLINE);
    while(read(sockfd, from_serv, MAXLINE) != 0)
    {
        cout << from_serv;
        bzero(from_serv, MAXLINE);
    }
    */

	return 0;
}