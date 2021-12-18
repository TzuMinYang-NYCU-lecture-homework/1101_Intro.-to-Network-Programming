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
#define MAX_X 100
#define MAX_Y 100

using namespace std;

struct pos
{
    int x, y;
    string path = "";
};

int maze[MAX_X + 100][MAX_Y + 100] = {0}, sockfd;
pos beg_, end_;
char input_buffer[MAXLINE + 100];
string str_input_buffer = "";

struct sockaddr lookup_udp(const char *hostname, const char *service) {
    struct addrinfo hint, *res = NULL;

    bzero(&hint, sizeof(hint));
    hint.ai_flags = AI_CANONNAME;
    hint.ai_socktype = SOCK_DGRAM;

    getaddrinfo(hostname, service, &hint, &res);
    return res -> ai_addr[0];
}

int test(pos cur, char dir)
{
    if(dir == 'w') cur.x--;
    else if(dir == 's') cur.x++;
    else if(dir == 'a') cur.y--;
    else if(dir == 'd') cur.y++;

    if(cur.x < 0 || cur.x > MAX_X || cur.y < 0 || cur.y > MAX_Y || maze[cur.x][cur.y] == -1) return 0;
    return 1;
}

void bfs()
{
    queue<pos> q;
    q.push(beg_);

    bool inval[MAX_X + 100][MAX_Y + 100] = {0};
    while(!q.empty())
    {
        pos cur = q.front(), next;
        q.pop();
        
        if(inval[cur.x][cur.y]) continue;
        inval[cur.x][cur.y] = true;

        if(cur.x == end_.x && cur.y == end_.y)
        {
            end_ = cur;
            end_.path += "\n";
            return;
        }
        
        if(test(cur, 'w'))
        {
            next = cur;
            next.path += "w";
            next.x = cur.x - 1;
            q.push(next);
        }
        if(test(cur, 's'))
        {
            next = cur;
            next.path += "s";
            next.x = cur.x + 1;
            q.push(next);
        }
        if(test(cur, 'a'))
        {
            next = cur;
            next.path += "a";
            next.y = cur.y - 1;
            q.push(next);
        }
        if(test(cur, 'd'))
        {
            next = cur;
            next.path += "d";
            next.y = cur.y + 1;
            q.push(next);
        }
    }
}

void read_until_and_output(string wanted_str)
{
    str_input_buffer = "";
    while(str_input_buffer.find(wanted_str) == std::string::npos)
    {
        bzero(input_buffer, MAXLINE);
        read(sockfd, input_buffer, MAXLINE);
        str_input_buffer += input_buffer;
    }
    cout << str_input_buffer;
}

void load_into_maze(int start_x, int start_y)
{
    str_input_buffer = str_input_buffer.substr(str_input_buffer.find(":"));
    string one_line = "";
    for(int i = 0; i < 7; i++)
    {
        one_line = str_input_buffer.substr(0, str_input_buffer.find("\n"));
        one_line = one_line.substr(one_line.find(":") + 2);
        str_input_buffer = str_input_buffer.substr(str_input_buffer.find("\n") + 1);
        
        for(int j = 0; j < (int)one_line.length(); j++)
        {
            if(one_line.at(j) == '*' || one_line.at(j) == 'E' || one_line.at(j) == '.') maze[start_x + i][start_y + j] = 1;
            else if(one_line.at(j) == '#') maze[start_x + i][start_y + j] = -1;
                
            if(one_line.at(j) == '*') beg_ = {start_x + i, start_y + j};
            else if(one_line.at(j) == 'E') end_ = {start_x + i, start_y + j};
        }
    }
}

int main(int argc, char *argv[])
{

    if(argc < 3)
    {
        printf("Usage: ./a.out <challenge_name> <port_number>");
        return 0;
    }

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr servaddr = lookup_udp(argv[1], argv[2]);
    connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));


    string direction_instr;
    
    read_until_and_output("Enter");

    str_input_buffer = str_input_buffer.substr(str_input_buffer.find("x 7") + 5, (str_input_buffer.find("Enter") - 1) - (str_input_buffer.find("x 7") + 5));
    
    // 避免一開始迷宮位置太偏
    direction_instr = "";
    if(str_input_buffer.at(7) == ' ') //view太左邊, 要往右
        direction_instr = "lllllllllllll\n";

    else if(str_input_buffer.at(17) == ' ') //view太右邊, 要往左
        direction_instr = "jjjjjjjjjjjjj\n";

    else if(str_input_buffer.at(3) == '-') //view太上面, 要往下
        direction_instr = "kkkkkkkkk\n";
    
    else if(str_input_buffer.at(3) == '9') //view太下面, 要往上
        direction_instr = "iiiiiiiii\n";

    if(direction_instr != "")
    {
        write(sockfd, direction_instr.c_str(), direction_instr.length());
        read_until_and_output(" move(s)>");
    }

    //移到最上面
    while(str_input_buffer.find("-1:") == std::string::npos) 
    {   
        direction_instr = "i\n";
        write(sockfd, direction_instr.c_str(), direction_instr.length());
        read_until_and_output(" move(s)>");
    }
    

    direction_instr = "k\n";
    write(sockfd, direction_instr.c_str(), direction_instr.length());
    read_until_and_output(" move(s)>");

    //移到最左邊
    while(str_input_buffer.find("0:  ") == std::string::npos) 
    {   
        direction_instr = "j\n";
        write(sockfd, direction_instr.c_str(), direction_instr.length());
        read_until_and_output(" move(s)>");
    }
    
    
    direction_instr = "l\n";
    write(sockfd, direction_instr.c_str(), direction_instr.length());
    read_until_and_output(" move(s)>");

    string view_right = "lllllllllll\n", view_down = "kkkkkkk\n", view_back_to_top = "";
    for(int i = 0; i < 105; i++) view_back_to_top += "i";
    view_back_to_top += "\n";

    // 先往下, 再回到最上面往右一點點, 重複
    for(int j = 0; j < MAX_Y / 11 + 1; j++)
    {
        for(int i = 0; i < MAX_X / 7 + 1; i++)
        {
            load_into_maze(i * 7, j * 11);
            write(sockfd, view_down.c_str(), view_down.length());
            read_until_and_output(" move(s)>");
        }
        
        write(sockfd, view_back_to_top.c_str(), view_back_to_top.length());
        read_until_and_output(" move(s)>");

        write(sockfd, view_right.c_str(), view_right.length());
        read_until_and_output(" move(s)>");
        
    }

    /*  
    cout << "    0         1         2         3         4         5         6         7         8         9         0\n" 
         << "    012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901\n";
    for(int i = 0; i < MAX_X + 1; i++)
    {
        printf("%3d ", i);
        for(int j = 0; j < MAX_Y + 1; j++)
        {
            if(maze[i][j] == 0) cout << "#";
            else cout << ".";
        }
        cout << "\n";
    }*/
    
    bfs();
    write(sockfd, end_.path.c_str(), end_.path.length());

    bzero(input_buffer, MAXLINE);
    while(read(sockfd, input_buffer, MAXLINE) != 0)
    {
        cout << input_buffer;
        bzero(input_buffer, MAXLINE);
    }

	return 0;
}