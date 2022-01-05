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
#define MAX_X 9
#define MAX_Y 9
#define BOOM -1
#define UNKNOWN -2
#define PACKET_LEN 5
#define MAX_BOOM 5

char udp_input_buf[MAXLINE];
string str_udp_input_buf, str_udp_output_buf;
int udp_sockfd;
sockaddr_in udp_serv_addr, udp_cli_addr;
socklen_t udp_serv_len;

char udp_packet_buf[PACKET_LEN];
int map[MAX_X + 5][MAX_Y + 5] = {0}, known_boom = 0;

struct udp_packet
{
    short pos_x, pos_y;
    char action;
}__attribute__((packed));

void err_quit(string msg)
{
    perror(msg.c_str());
    exit(-1);
}

// origin
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

// level5

void udp_send_packet(short pos_y, short pos_x, char action) //注意x y位置交換了
{
    udp_packet *ptr = (udp_packet *)udp_packet_buf;

    // -1是因為外面有加一圈空的
    ptr->pos_x = htons(pos_x - 1);
    ptr->pos_y = htons(pos_y - 1);

    ptr->action = action;

    if(sendto(udp_sockfd, udp_packet_buf, PACKET_LEN, 0, (sockaddr *)&udp_serv_addr, sizeof(udp_serv_addr)) < 0) err_quit("udp sendto error");
}

void dump_map()
{
    for(int i = 0; i < MAX_Y + 2; i++)
    {
        for(int j = 0; j < MAX_X + 2; j++)
        {
            if(map[i][j] == BOOM) cout << "X";
            else if(map[i][j] == UNKNOWN) cout << "#";
            else if(map[i][j] == 0) cout << ".";
            else cout << map[i][j];
        }
        cout << "\n";
    }
}

void load_map()
{
    //取得地圖
    udp_send_packet(1, 1, 'G'); //座標隨便傳一個在1~9都可以裡面的
    udp_recv_and_stdout();
    str_udp_output_buf = str_udp_input_buf;

    //最外面是一圈空的
    string oneline = "";
    for(int i = 0; i < MAX_Y; i++)
    {
        if(str_udp_output_buf.find("\n") == std::string::npos) oneline = str_udp_output_buf.substr(0, str_udp_output_buf.length() - 1);
        else oneline = str_udp_output_buf.substr(0, str_udp_output_buf.find("\n"));

        if(oneline == "") return;

        if(str_udp_output_buf.find("\n") == str_udp_output_buf.length() - 1)
            str_udp_output_buf = "";
        else
            str_udp_output_buf = str_udp_output_buf.substr(str_udp_output_buf.find("\n") + 1);

        for(int j = 0; j < MAX_X; j++)
        {
            if(oneline.at(j) == '.') map[i + 1][j + 1] = 0;
            else if(oneline.at(j) == '#') map[i + 1][j + 1] = UNKNOWN;
            else if(oneline.at(j) == 'M') map[i + 1][j + 1] = BOOM;
            else if(oneline.at(j) == 'X') exit(0);  //真的踩到炸彈就結束
            else map[i + 1][j + 1] = oneline.at(j) - 48;
        }
    }

    //dump_map();
}

bool test_solve()
{
    for(int i = 1; i < MAX_Y + 1; i++)
        for(int j = 1; j < MAX_X + 1; j++)
            if(map[i][j] == UNKNOWN) return false;
    return true;
}

void solve_boom()
{
    pair<int, int> dir[8] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};
    bool nothing_can_do = true;
    //最外面有一圈空的
    for(int i = 1; i < MAX_Y + 1; i++)
    {
        for(int j = 1; j < MAX_X + 1; j++)
        {
            if(map[i][j] == 0 || map[i][j] == UNKNOWN || map[i][j] == BOOM) continue;

            int boom_count = 0, unknown_count = 0;
            for(int k = 0; k < 8; k++)
            {
                if(map[i + dir[k].first][j + dir[k].second] == BOOM) boom_count++;
                else if(map[i + dir[k].first][j + dir[k].second] == UNKNOWN) unknown_count++;
            }
                

            if(boom_count == map[i][j]) //附近的炸彈都找到了，剩下未開的都點開
            {
                for(int k = 0; k < 8; k++)
                    if(map[i + dir[k].first][j + dir[k].second] == UNKNOWN)
                    {
                        udp_send_packet(i + dir[k].first, j + dir[k].second, 'O');
                        map[i + dir[k].first][j + dir[k].second] = 0;
                        nothing_can_do = false;
                    }
            }

            else if(unknown_count == map[i][j] - boom_count) //未開的=剩下的炸彈數，剩下未開的都標成炸彈
            {
                for(int k = 0; k < 8; k++)
                    if(map[i + dir[k].first][j + dir[k].second] == UNKNOWN) 
                    {
                        udp_send_packet(i + dir[k].first, j + dir[k].second, 'M');
                        map[i + dir[k].first][j + dir[k].second] = BOOM;
                        nothing_can_do = false;
                    }
            }
        }
    }

    if(nothing_can_do) //如果目前資訊不足以確定任何行為，就把第一個UNKNOWN打開
    {
        for(int i = 1; i < MAX_Y + 1; i++)
            for(int j = 1; j < MAX_X + 1; j++)
                if(map[i][j] == UNKNOWN)
                {
                    udp_send_packet(i, j, 'O'); 
                    map[i][j] = 0;
                    return;
                }
    }
}

int main(int argc, char** argv)
{
    if(argc < 3) err_quit("Usage: ./level5 minesweeper <UDP-port-received-from-telnet-command>");

    udp_lookup(argv[1], argv[2]);
    udp_bind();

    str_udp_output_buf = "test";
    udp_send_and_stdout();
    udp_recv_and_stdout();

    udp_send_packet(1, 1, 'O'); //開最左上角那個
    load_map();

    while(!test_solve())
    {
        load_map();
        solve_boom();
    }

    return 0;
}