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
#include <cstring>
#include <string.h>

#define LISTENQ 1024
#define MAXLINE 4095
#define UDP_MAX 2000

using namespace std;

struct user_state
{
    string password = "";
    int violation_num = 0;
    bool is_login = false;
};

struct client_state
{
    string username = "", input_buffer = "";
    bool is_login = false;
    // HW3新增
    int udp_port = -1; // -1代表還沒進入聊天室
    int version = -1; // -1代表還沒進入聊天室
};

struct chat_info
{
    string sender = "", message = "";
};

struct udp_flag_and_ver {   //助教給的範例
    unsigned char flag;
    unsigned char version;
    unsigned char payload[0];
} __attribute__((packed));

struct udp_version1 {   //助教給的範例
    unsigned short len;
    unsigned char data[0];
} __attribute__((packed));

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

                clilen = sizeof(tcp_cliaddr);
                if ((new_cli_fd = accept(tcplistenfd, (sockaddr *) &tcp_cliaddr, &clilen)) < 0)
                    perror("accept failed");
                
                // !!! 有改
                //client_fd_to_port.insert({new_cli_fd, cliaddr.sin_port});
                //client.insert({cliaddr.sin_port, {"", "", false, -1, -1}});
                client[new_cli_fd] = {"", "", false, -1, -1};

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

                if(FD_ISSET(i, &rset) && client.count(i) != 0)
                {
                    ready_sock_num--;
                    current_client_tcp_fd = i;

                    client_tcp_reaction();
                }
            }                    
        }

    }

private:

    //網路的
    int SERV_PORT = 0;
    fd_set rset, all_set;
    int tcplistenfd, udpfd, current_client_tcp_fd = -1;
    struct sockaddr_in servaddr, tcp_cliaddr, udp_cliaddr;
    socklen_t clilen;

    string output_buffer;
    char char_input_buffer[MAXLINE + 100];

    //BBS的
    map<string, user_state> user;   //index:username，因為不用像HW1一樣根據註冊時間列出順序所以可以這樣
    map<int, client_state> client;  //index:tcp fd

    //聊天室的
    vector<chat_info> chat_history; //其實可以用一個很長的字串就好
    map<string, string> filtering_list = {{"how", "***"}, {"you", "***"}, {"or", "**"}, {"pek0", "****"}, {"tea", "***"}, {"ha", "**"}, {"kon", "***"}, 
                                          {"pain", "****"}, {"Starburst Stream", "****************"}};
    string base64_dic = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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

    void client_tcp_reaction()
    {
        bzero(char_input_buffer, sizeof(char_input_buffer));

        if(read(current_client_tcp_fd, char_input_buffer, MAXLINE) <= 0)   // <=0是連線已斷
        {
            close(current_client_tcp_fd);
            client.erase(current_client_tcp_fd);
            
            FD_CLR(current_client_tcp_fd, &all_set);
            return;
        }

        client[current_client_tcp_fd].input_buffer += char_input_buffer;
        
        // 助教的測資其實可以不用這樣處理，但懶得改
        // 發現如果在comman line用輸入的,單行最多只能讀4096(含換行)個字元,疑似和OS的IO buffer有關
        // 用file IO就不會有這個問題,但會有連線太快關掉而收不到server訊息的問題
        int endline_position = client[current_client_tcp_fd].input_buffer.find('\n');
        while(endline_position != std::string::npos)     //若出現換行,i.e.,讀完一次輸入,用while是因為讀檔的話read會一次讀很多進來
        {
            //執行換行前的指令
            tcp_action(client[current_client_tcp_fd].input_buffer.substr(0, endline_position + 1));

            if(client.size() == 0 || client.count(current_client_tcp_fd) == 0) return;    // 代表已經exit

            output_buffer = "% ";
            writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());

            //留下換行後的
            if(endline_position != client[current_client_tcp_fd].input_buffer.length() - 1)
                client[current_client_tcp_fd].input_buffer = client[current_client_tcp_fd].input_buffer.substr(endline_position + 1);
                
            else
                client[current_client_tcp_fd].input_buffer = "";
                
            endline_position = client[current_client_tcp_fd].input_buffer.find('\n');
        }
    
    }

    void tcp_action(string input)
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
        
        else if(user.count(username) != 0)  //使用者已註冊又要註冊
            output_buffer = "Username is already used.\n";
            
        else
        {
            user[username] = {password, 0, false};
            output_buffer = "Register successfully.\n";
        }   

        writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
    }
    
    void Login(string username, string password, string garbage)
    {
        if(username == "" || password == "" || garbage != "")    //參數過少或過多
            output_buffer = "Usage: login <username> <password>\n";
        
        else if(client[current_client_tcp_fd].is_login || (user.count(username) != 0 && user[username].is_login))  //當前client已經登入 or 此username在別的client登入過了
            output_buffer = "Please logout first.\n";

        else if(user.count(username) == 0)   //使用者不存在
            output_buffer = "Login failed.\n";

        else if(user[username].violation_num >= 3)   // !!!使用者在blacklist中
            output_buffer = "We don't welcome " + username + "!\n";

        else if(user[username].password != password) //密碼錯誤
            output_buffer = "Login failed.\n";

        else
        {
            user[username].is_login = true;
            client[current_client_tcp_fd].username = username;
            client[current_client_tcp_fd].is_login = true;
            output_buffer = "Welcome, " + username + ".\n";
        }

        writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
    }

    void Logout(string garbage)
    {
        if(garbage != "")    //參數過多
            output_buffer = "Usage: logout\n";

        else if(!client[current_client_tcp_fd].is_login)    //還沒登入就想登出
            output_buffer = "Please login first.\n";

        else
        {
            output_buffer = "Bye, ";
            output_buffer += client[current_client_tcp_fd].username + ".\n";
            
            user[client[current_client_tcp_fd].username].is_login = false;
            client[current_client_tcp_fd].username = "";
            client[current_client_tcp_fd].is_login = false;
            client[current_client_tcp_fd].udp_port = -1;
            client[current_client_tcp_fd].version = -1;
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
            if(client[current_client_tcp_fd].is_login)     //若已登入要exit就先跟他說Bye
            {
                user[client[current_client_tcp_fd].username].is_login = false;

                output_buffer = "Bye, " + client[current_client_tcp_fd].username + ".\n";
                writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
            }
            close(current_client_tcp_fd);   //用nc連線的話,client要再輸入一次才會發現斷線,telnet則會exit後就馬上發現斷線
            FD_CLR(current_client_tcp_fd, &all_set);

            client.erase(current_client_tcp_fd);
        }
    }

    //hw3新增

    void Enter_chat_room(string str_port, string str_version, string garbage)//!!!同時有數字和非數字的情況
    {
        int port = strtol(str_port.c_str(), NULL, 10), version = strtol(str_version.c_str(), NULL, 10);  //後面要用比較方便

        if(str_port == "" || str_version == "" || garbage != "")    //參數過少或過多
            output_buffer = "Usage: enter-chat-room <port> <version>\n";
        
        else if(!is_num(str_port) || port < 1 || port > 65535)  //!!!port不是數字，或port不在[1,65535]
            output_buffer = "Port " + str_port + " is not valid.\n";

        else if(!is_num(str_version) || (version != 1 && version != 2))   //!!!version不是數字，或version不是1也不是2
            output_buffer = "Version " + str_version + " is not supported.\n";

        else if(!client[current_client_tcp_fd].is_login)   //還沒登入就想進聊天室
            output_buffer = "Please login first.\n";

        else
        {
            output_buffer = "Welcome to public chat room.\nPort:" + str_port + "\nVersion:" + str_version + "\n";
            for(auto &msg: chat_history)    //把history串起來
                output_buffer += msg.sender + ":" + msg.message + "\n";
            client[current_client_tcp_fd].udp_port = port;
            client[current_client_tcp_fd].version = version;
        }

        writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
    }

    bool is_num(string str)
    {
        for(int i = 0; i < str.length(); i++)
            if(!(str.at(i) >= '0' && str.at(i) <= '9'))
                return false;
        return true;
    }

    void Chat()
    {
        bzero(char_input_buffer, sizeof(char_input_buffer));
        if(recvfrom(udpfd, char_input_buffer, MAXLINE, 0, (sockaddr *)&udp_cliaddr, &clilen) < 0)
            perror("recvfrom failed");

        string username = "", message = "";
        parse_chat_packet(&username, &message);
        
        for(auto iter = client.begin(); iter != client.end(); iter++)   //找此udp port對應client的tcp fd
        {
            if(iter->second.udp_port == ntohs(udp_cliaddr.sin_port))    //記得要轉換成host的endian
            {
                current_client_tcp_fd = iter->first;
                break;
            }
        }

        bool is_violated = false;

        for(auto iter = filtering_list.begin(); iter != filtering_list.end(); iter++)   //replace全部的出現的違規的字串
        {
            while(message.find(iter->first) != std::string::npos)   
            {
                message.replace(message.find(iter->first), iter->first.length(), iter->second);
                is_violated = true;
            }
        }

        if(is_violated && ++user[username].violation_num >= 3)  //若違規超過三次就把他登出
        {
            user[username].is_login = false;
            client[current_client_tcp_fd].is_login = false;
            client[current_client_tcp_fd].udp_port = -1;
            client[current_client_tcp_fd].version = -1;

            output_buffer = "Bye, " + username + ".\n% ";   //要記得自己補送%，這個情況比較特別
            writen(current_client_tcp_fd, output_buffer.c_str(), output_buffer.length());
        }

        chat_history.push_back({username, message});    // 記錄歷史紀錄

        for(auto iter = client.begin(); iter != client.end(); iter++)   //把訊息送給所有在聊天室中的人
            if(iter->second.udp_port != -1)
                send_chat_packet(iter->second.udp_port, username, message);

    }

    void parse_chat_packet(string *name, string *message)   // 解析UDP封包
    {
        struct udp_flag_and_ver *ptr_flag_and_ver = (struct udp_flag_and_ver*) char_input_buffer;
        char flag = *((char *)ptr_flag_and_ver), version = *((char *)ptr_flag_and_ver + 1);

        if(flag != '\1') return;

        if(version == '\1')
        {
            int name_len = -1, msg_len = -1;
            char chat_name[UDP_MAX], chat_msg[UDP_MAX];
            bzero(chat_name, UDP_MAX);
            bzero(chat_msg, UDP_MAX);

            struct udp_version1 *ptr_name = (struct udp_version1*) (char_input_buffer + sizeof(struct udp_flag_and_ver));
            name_len = ntohs(ptr_name->len);
            memcpy(chat_name, ptr_name->data, name_len);
            *name = chat_name;

            struct udp_version1 *ptr_msg = (struct udp_version1*) (char_input_buffer + sizeof(struct udp_flag_and_ver) + sizeof(struct udp_version1) + name_len);
            msg_len = ntohs(ptr_msg->len);
            memcpy(chat_msg, ptr_msg->data, msg_len);
            *message = chat_msg;
        }

        else if(version == '\2')
        {
            cout << "I'm in\n";
            char *name_beg = char_input_buffer + 2, *name_end = strstr(name_beg, "\n");
            *name_end = '\0';
            *name = base64_decode(name_beg);

            char *msg_beg = name_end + 1, *msg_end = strstr(msg_beg, "\n");
            *msg_end = '\0';
            *message = base64_decode(msg_beg);
        }
    }

    void send_chat_packet(int recever_udp_port, string sender, string message)  //送出UDP封包
    {
        char buf[4096];
        bzero(buf, 4096);

        bzero(&udp_cliaddr, sizeof(udp_cliaddr));
        udp_cliaddr.sin_family = AF_INET;
        inet_pton(AF_INET, "127.0.0.1", &udp_cliaddr.sin_addr);
        udp_cliaddr.sin_port = htons(recever_udp_port);

        if(client[current_client_tcp_fd].version == 1)
        {
            uint16_t name_len = (uint16_t)sender.length();
            uint16_t msg_len = (uint16_t)message.length();
            
            struct udp_flag_and_ver *ptr_flag_and_ver = (struct udp_flag_and_ver*) buf;
            struct udp_version1 *ptr_name = (struct udp_version1*) (buf + sizeof(struct udp_flag_and_ver));
            struct udp_version1 *ptr_msg = (struct udp_version1*) (buf + sizeof(struct udp_flag_and_ver) + sizeof(struct udp_version1) + name_len);
            ptr_flag_and_ver->flag = 0x01;
            ptr_flag_and_ver->version = 0x01;
            ptr_name->len = htons(name_len);
            memcpy(ptr_name->data, sender.c_str(), name_len);
            ptr_msg->len = htons(msg_len);
            memcpy(ptr_msg->data, message.c_str(), msg_len);

            sendto(udpfd, buf, sizeof(struct udp_flag_and_ver) + sizeof(struct udp_version1) + name_len + sizeof(struct udp_version1) + msg_len, 0, (sockaddr*)&udp_cliaddr, clilen);
        }

        else if(client[current_client_tcp_fd].version == 2)
        {
            sprintf(buf, "\x01\x02%s\n%s\n", base64_encode(sender).c_str(), base64_encode(message).c_str());
            sendto(udpfd, buf, strlen(buf), 0, (sockaddr*)&udp_cliaddr, clilen);
        }
    }

    string base64_encode(string encode_str)
    {
        string result_str = "";
        char encode_char[4], decode_char[5];
        int encode_len = encode_str.length();

        for(int i = 0; i < encode_len; i++)
        {
            encode_char[i % 3] = encode_str.at(i);
            if((i + 1) % 3 == 0)
            {
                decode_char[0] = base64_dic.at((encode_char[0] & 0b11111100) >> 2);
                decode_char[1] = base64_dic.at(((encode_char[0] & 0b00000011) << 4) + ((encode_char[1] & 0b11110000) >> 4));
                decode_char[2] = base64_dic.at(((encode_char[1] & 0b00001111) << 2) + ((encode_char[2] & 0b11000000) >> 6));
                decode_char[3] = base64_dic.at((encode_char[2] & 0b00111111));
                decode_char[4] = '\0';
                result_str += decode_char;
            }
        }

        if(encode_len % 3 != 0) //後面要補'='
        {
            for(int i = encode_len; i < 3 - encode_len % 3 + encode_len; i++)
                encode_char[i % 3] = '\0';
            
            if(encode_len % 3 == 1)
            {
                decode_char[0] = base64_dic.at((encode_char[0] & 0b11111100) >> 2);
                decode_char[1] = base64_dic.at(((encode_char[0] & 0b00000011) << 4) + ((encode_char[1] & 0b11110000) >> 4));
                decode_char[2] = '=';
                decode_char[3] = '=';
                decode_char[4] = '\0';
                result_str += decode_char;
            }

            else if(encode_len % 3 == 2)
            {
                decode_char[0] = base64_dic.at((encode_char[0] & 0b11111100) >> 2);
                decode_char[1] = base64_dic.at(((encode_char[0] & 0b00000011) << 4) + ((encode_char[1] & 0b11110000) >> 4));
                decode_char[2] = base64_dic.at(((encode_char[1] & 0b00001111) << 2) + ((encode_char[2] & 0b11000000) >> 6));
                decode_char[3] ='=';
                decode_char[4] = '\0';
                result_str += decode_char;
            }
        }

        return result_str;
    }

    string base64_decode(char *beg)
    {
        string decode_str = beg;
        string result_str = "";
        char encode_char[4], decode_char[5];
        int decode_len = decode_str.length(), equal_pos = -1;

        for(int i = 0; i < decode_len; i++)
        {
            if(decode_str.at(i) == '=')
            {
                equal_pos = i;
                break;
            }

            decode_char[i % 4] = decode_str.at(i);
            if((i + 1) % 4 == 0)
            {
                encode_char[0] = (base64_dic.find(decode_char[0]) << 2) + ((base64_dic.find(decode_char[1]) & 0b110000) >> 4);
                encode_char[1] = ((base64_dic.find(decode_char[1]) & 0b001111) << 4) + ((base64_dic.find(decode_char[2]) & 0b111100) >> 2);
                encode_char[2] = ((base64_dic.find(decode_char[2]) & 0b000011) << 6) + base64_dic.find(decode_char[3]);
                encode_char[3] = '\0';
                result_str += encode_char;
            }
        }

        if(equal_pos != -1)
        {
            if(equal_pos == decode_str.length() - 1 - 1) //有兩個'='
            {
                encode_char[0] = (base64_dic.find(decode_char[0]) << 2) + ((base64_dic.find(decode_char[1]) & 0b110000) >> 4);
                encode_char[1] = '\0';
                result_str += encode_char;
            }

            else if(equal_pos == decode_str.length() - 1) //有一個'='
            {
                encode_char[0] = (base64_dic.find(decode_char[0]) << 2) + ((base64_dic.find(decode_char[1]) & 0b110000) >> 4);
                encode_char[1] = ((base64_dic.find(decode_char[1]) & 0b001111) << 4) + ((base64_dic.find(decode_char[2]) & 0b111100) >> 2);
                encode_char[2] = '\0';
                result_str += encode_char;
            }
        }

        return result_str;
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
