#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
#include <error.h>

#define LISTENQ 1024
#define MAXLINE 4095

using namespace std;

struct client_state
{
    string current_user = "", input_buffer = "";
    bool is_loggin = false;
    // HW3新增
    int udp_port = -1; // -1代表還沒進入聊天室
    int version = -1; // -1代表還沒進入聊天室
};

struct chat_info
{
    string sender = "", message = "";
};

class BBS_Server
{
public:

    BBS_Server(int SERV_PORT) { this->SERV_PORT = SERV_PORT; }

    void multiple_user_server()
    {
        // error的部分是HW3時我自己想新增的
        if ((tcplistenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            perror("tcp socket failed");

        // !!!設定TCP SO_REUSEADDR HW3新增
        int enable = 1;
        if (setsockopt(tcplistenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
            perror("setsockopt(SO_REUSEADDR) failed");

        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(SERV_PORT);
        
       
        if (bind(tcplistenfd, (sockaddr *) &servaddr, sizeof(servaddr)) < 0)
            perror("tcp bind failed");

        if (listen(tcplistenfd, LISTENQ) < 0)  //LISTENQ是backlog的大小
            perror("tcp listen failed");

        // UDP
        if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
            perror("udp socket failed");

        if (bind(udpfd, (sockaddr *) &servaddr, sizeof(servaddr)) < 0)
            perror("udp bind failed");

        // select
        int max_fd = (tcplistenfd > udpfd ? tcplistenfd : udpfd), ready_sock_num = 0;

        FD_ZERO(&all_set);
        FD_SET(tcplistenfd, &all_set);
        FD_SET(udpfd, &all_set);

        int new_cli_fd = 0;
        while(1)
        {
            rset = all_set;
            if ((ready_sock_num = select(max_fd + 1, &rset, NULL, NULL, NULL)) < 0)
                perror("select failed");

            if(FD_ISSET(tcplistenfd, &rset))
            {
                ready_sock_num--;

                cli_len = sizeof(cliaddr);
                if ((new_cli_fd = accept(tcplistenfd, (sockaddr *) &cliaddr, &cli_len)) < 0)
                    perror("accept failed");
                
                // !!! 有改
                //client_fd_to_port.insert({new_cli_fd, cliaddr.sin_port});
                //client.insert({cliaddr.sin_port, {"", "", false, -1, -1}});
                client_fd_to_port[new_cli_fd] = cliaddr.sin_port;
                client[cliaddr.sin_port] = {"", "", false, -1, -1};

                output_buffer = "********************************\n** Welcome to the BBS server. **\n********************************\n% ";
                writen(new_cli_fd, output_buffer.c_str(), output_buffer.length());

                FD_SET(new_cli_fd, &all_set);
                if(new_cli_fd > max_fd) max_fd = new_cli_fd;
            }

            if(FD_ISSET(udpfd, &rset))
            {
                ready_sock_num--;
                current_client_tcp_fd = -1;

                Chat();
            }

            for(int i = 0; i <= max_fd; i++)
            {
                if(ready_sock_num <= 0) break;

                if(FD_ISSET(i, &rset) && client_fd_to_port.count(i) != 0)
                {
                    ready_sock_num--;
                    current_client_tcp_fd = i;
                    current_client_tcp_port = client_fd_to_port[current_client_tcp_fd];

                    client_reaction();
                }
            }                    
        }

    }

private:

    //網路的
    int SERV_PORT = 0;
    fd_set rset, all_set;
    int tcplistenfd, udpfd, current_client_tcp_fd = -1, current_client_tcp_port;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cli_len;

    string output_buffer, input_buffer;
    char char_input_buffer[MAXLINE + 100];

    map<int, int> client_fd_to_port;

    //BBS的
    vector<string> register_user;
    map<string, string> user_password;
    map<int, client_state> client;

    //聊天室的
    vector<chat_info> chat_history;
    vector<string> filtering_list = {"how", "you", "or", "pek0", "tea", "ha", "kon", "pain", "Starburst Stream"};
    vector<string> black_list;

    ssize_t	writen(int fd, const char *vptr, size_t n)  /* Write "n" bytes to a descriptor. */
    {
        size_t		nleft;
        ssize_t		nwritten;
        const char	*ptr;

        ptr = vptr;
        nleft = n;
        while (nleft > 0) {
            if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
                if (nwritten < 0 && errno == EINTR)
                    nwritten = 0;		/* and call write() again */
                else
                    return(-1);			/* error */
            }

            nleft -= nwritten;
            ptr   += nwritten;
        }
        return(n);
    }

    void client_reaction()
    {
        bzero(char_input_buffer, sizeof(char_input_buffer));

        if(read(current_client_tcp_fd, char_input_buffer, MAXLINE) <= 0)
        {
            close(current_client_tcp_fd);
            client_fd_to_port.erase(current_client_tcp_fd);
            client.erase(current_client_tcp_port);
            
            FD_CLR(current_client_tcp_fd, &all_set);
            return;
        }

        client[current_client_tcp_port].input_buffer += char_input_buffer;
        
        // 發現如果在comman line用輸入的,單行最多只能讀4096(含換行)個字元,疑似和OS的IO buffer有關
        // 用file IO就不會有這個問題,但會有連線太快關掉而收不到server訊息的問題
        int endline_position = client[current_client_tcp_port].input_buffer.find('\n');
        while(endline_position != std::string::npos)     //若出現換行,i.e.,讀完一次輸入,用while是因為讀檔的話read會一次讀很多進來
        {
            //執行換行前的指令
            action(client[current_client_tcp_port].input_buffer.substr(0, endline_position + 1));

            if(client.size() == 0 || client_fd_to_port.count(current_client_tcp_fd) == 0) return;    // 代表已經exit

            output_buffer = "% ";
            writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());

            //留下換行後的
            if(endline_position != client[current_client_tcp_port].input_buffer.length() - 1)
                client[current_client_tcp_port].input_buffer = client[current_client_tcp_port].input_buffer.substr(endline_position + 1);
                
            else
                client[current_client_tcp_port].input_buffer = "";
                
            endline_position = client[current_client_tcp_port].input_buffer.find('\n');
        }
    
    }

    void action(string input)
    {
        stringstream ss(input);
        string instr = "", arg1 = "", arg2 = "", arg3 = "";
        ss >> instr >> arg1 >> arg2 >> arg3;
        
        if(instr == "register") Register(arg1, arg2, arg3);
        else if(instr == "login") Login(arg1, arg2, arg3);
        else if(instr == "logout") Logout(arg1);
        else if(instr == "exit") Exit(arg1);
        else if(instr == "enter-chat-room") Enter_chat_room(arg1, arg2, arg3);
    }

    //hw1就有的指令

    void Register(string username, string password, string garbage)
    {
        if(username == "" || password == "" || garbage != "")    //參數過少或過多
            output_buffer = "Usage: register <username> <password>\n";
        
        else if(find(register_user.begin(), register_user.end(), username) != register_user.end())  //使用者已註冊又要註冊
            output_buffer = "Username is already used.\n";

        else
        {
            register_user.push_back(username);
            user_password[username] = password;

            output_buffer = "Register successfully.\n";
        }   

        writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
    }

    bool the_user_has_login(string username) //看這個user有沒有在其他client登入過了
    {
        for(auto iter = client.begin(); iter != client.end(); iter++)
            if(username == iter -> second.current_user) return true;
        return false;
    }

    
    void Login(string username, string password, string garbage)
    {
        if(username == "" || password == "" || garbage != "")    //參數過少或過多
            output_buffer = "Usage: login <username> <password>\n";
        
        else if(client.size() != 0 && (client[current_client_tcp_port].is_loggin || the_user_has_login(username)))  //當前client已經登入 or 此username在別的client登入過了
            output_buffer = "Please logout first.\n";

        else if(find(register_user.begin(), register_user.end(), username) == register_user.end() )   //使用者不存在
            output_buffer = "Login failed.\n";

        else if(find(black_list.begin(), black_list.end(), username) != black_list.end() )   // !!!使用者在blacklist中
            output_buffer = "We don't welcome " + username + "!\n";

        else if(user_password[username] != password) //密碼錯誤
            output_buffer = "Login failed.\n";

        else
        {
            client[current_client_tcp_port].current_user = username;
            client[current_client_tcp_port].is_loggin = true;
            output_buffer = "Welcome, " + username + ".\n";
        }

        writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
    }

    void Logout(string garbage)
    {
        if(garbage != "")    //參數過多
            output_buffer = "Usage: logout\n";

        else if(!client[current_client_tcp_port].is_loggin)    //還沒登入就想登出
            output_buffer = "Please login first.\n";

        else
        {
            output_buffer = "Bye, ";
            output_buffer += client[current_client_tcp_port].current_user + ".\n";
            
            client[current_client_tcp_port].current_user = "";
            client[current_client_tcp_port].is_loggin = false;
        }

        writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());

    }

    void Exit(string garbage)//!!!要改
    {
        if(garbage != "")    //參數過多
        {
            output_buffer = "Usage: exit\n";
            writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            if(client[current_client_tcp_port].is_loggin)     //若已登入要exit就先跟他說Bye
            {
                output_buffer = "Bye, ";
                output_buffer += client[current_client_tcp_port].current_user + ".\n";
                writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
            }
            close(current_client_tcp_fd);   //用nc連線的話,client要再輸入一次才會發現斷線,telnet則會exit後就馬上發現斷線
            client_fd_to_port.erase(current_client_tcp_fd);
            client.erase(current_client_tcp_port);
            FD_CLR(current_client_tcp_fd, &all_set);
        }
        
    }

    //hw3新增

    void Enter_chat_room(string str_port, string str_version, string garbage)
    {
        int port = strtol(str_port.c_str(), NULL, 10), version = strtol(str_version.c_str(), NULL, 10);

        if(str_port == "" || str_version == "" || garbage != "")    //參數過少或過多
            output_buffer = "Usage: enter-chat-room <port> <version>\n";
        
        else if(port < 1 || port > 65535)  //!!!port不在[1,65535]，或port不是數字
            output_buffer = "Port " + str_port + " is not valid.\n";

        else if(version != 1 && version != 2)   //!!!version不是1或2，或version不是數字
            output_buffer = "Version " + str_version + " is not supported.\n";

        else if(!client[current_client_tcp_port].is_loggin)   //還沒登入就想進聊天室
            output_buffer = "Please login first.\n";

        else
        {
            output_buffer = "Welcome to public chat room.\nPort:" + str_port + "\nVersion:" + str_version + "\n";
            for(auto &msg: chat_history)
                output_buffer += msg.sender + ":" + msg.message + "\n";
            client[current_client_tcp_port].udp_port = port;
            client[current_client_tcp_port].version = version;
        }

        writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
    }

    void Chat()
    {
        bzero(char_input_buffer, sizeof(char_input_buffer));
        if(recvfrom(udpfd, char_input_buffer, MAXLINE, 0, (sockaddr *)&cliaddr, &cli_len) < 0)
            perror("recvfrom failed");
        
        current_client_tcp_port = cliaddr.sin_port;

    }
};

int main(int argc, char **argv)
{
    if(argc <= 1)
    {
        cout << "Usage: ./hw3 <serverport>\n";
        return 0;
    }

    BBS_Server my_server(strtol(argv[1], NULL, 10));
    my_server.multiple_user_server();
    
    return 0;
}
