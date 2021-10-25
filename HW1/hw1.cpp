#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

#define LISTENQ 1024
#define MAXLINE 4095

using namespace std;

struct message
{
    vector<string> content;
    int first_msg = 0;
};

struct client_state
{
    string current_user = "", input_buffer = "";
    bool is_loggin = false;
};

class BBS_Server
{
public:

    int SERV_PORT = 0;

    BBS_Server(int SERV_PORT) { this->SERV_PORT = SERV_PORT; }

    void multiple_user_server()
    {
        // ordinary
        int listenfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in servaddr, cliaddr;

        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(SERV_PORT);
        
        bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr));

        listen(listenfd, LISTENQ);  //LISTENQ是backlog的大小

        // select
        int max_fd = listenfd, ready_sock_num = 0;

        FD_ZERO(&all_set);
        FD_SET(listenfd, &all_set);

        int new_cli_fd = 0;
        while(1)
        {
            rset = all_set;
            ready_sock_num = select(max_fd + 1, &rset, NULL, NULL, NULL);

            if(FD_ISSET(listenfd, &rset))
            {
                ready_sock_num--;

                socklen_t cli_len = sizeof(cliaddr);
                new_cli_fd = accept(listenfd, (sockaddr *) &cliaddr, &cli_len);
                client[new_cli_fd] = {"", "", false};

                output_buffer = "********************************\n** Welcome to the BBS server. **\n********************************\n% ";
                writen(new_cli_fd, output_buffer.c_str(), output_buffer.length());

                FD_SET(new_cli_fd, &all_set);
                if(new_cli_fd > max_fd) max_fd = new_cli_fd;
            }

            for(int i = 0; i <= max_fd; i++)
            {
                
                if(FD_ISSET(i, &rset) && client.count(i) != 0)
                {
                    ready_sock_num--;
                    current_client_fd = i;
                    client_reaction();
                }
            }                    
        }

    }

    void client_reaction()
    {
        char char_input_buffer[MAXLINE + 100];
        bzero(char_input_buffer, sizeof(char_input_buffer));
        
        if(read(current_client_fd, char_input_buffer, MAXLINE) < 0)
        {
            close(current_client_fd);
            client.erase(current_client_fd);
            FD_CLR(current_client_fd, &all_set);
            return;
        }

        client[current_client_fd].input_buffer += char_input_buffer;
        
        // 發現如果在comman line用輸入的,單行最多只能讀4096(含換行)個字元,疑似和OS的IO buffer有關
        // 用file IO就不會有這個問題,但會有連線太快關掉而收不到server訊息的問題
        int endline_position = client[current_client_fd].input_buffer.find('\n');
        while(endline_position != std::string::npos)     //若出現換行,i.e.,讀完一次輸入,用while是因為讀檔的話read會一次讀很多進來
        {
            //執行換行前的指令
            action(client[current_client_fd].input_buffer.substr(0, endline_position + 1));

            if(client.size() == 0 || client.count(current_client_fd) == 0) return;    // 代表已經exit
            output_buffer = "% ";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
            //留下換行後的
            
            if(endline_position != client[current_client_fd].input_buffer.length() - 1)
                client[current_client_fd].input_buffer = client[current_client_fd].input_buffer.substr(endline_position + 1);
                
            else
                client[current_client_fd].input_buffer = "";
                
            endline_position = client[current_client_fd].input_buffer.find('\n');
        }
    
    }

private:

    vector<string> register_user;
    map<string, string> user_password;
    map<string, map<string, message> > receive_message;
    map<int, client_state> client;

    int current_client_fd = -1;
    string output_buffer;

    fd_set rset, all_set;

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

    void action(string input)
    {
        stringstream ss(input);
        string instr = "", arg1 = "", arg2 = "";
        ss >> instr >> arg1 >> arg2;
        
        if(instr == "register") Register(arg1, arg2);
        else if (instr == "login") Login(arg1, arg2);
        else if (instr == "logout") Logout();
        else if (instr == "whoami") Whoami();
        else if (instr == "list-user") List_user();
        else if (instr == "exit") Exit();
        else if (instr == "send")
        {
            if(arg2 != "") arg2 = input.substr(input.find(arg2));
            Send(arg1, arg2);
        }
        else if (instr == "list-msg") List_msg();
        else if (instr == "receive") Receive(arg1);
    }

    void Register(string username, string password)
    {
        if(username == "" || password == "")    //參數過少
        {
            output_buffer = "Usage: register <username> <password>\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }
        
        else if(find(register_user.begin(), register_user.end(), username) != register_user.end())  //使用者已註冊又要註冊
        {
            output_buffer = "Username is already used.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            register_user.push_back(username);
            user_password[username] = password;

            output_buffer = "Register successfully\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }   
        
    }

    void Login(string username, string password)
    {
        if(username == "" || password == "")    //參數過少
        {
            output_buffer = "Usage: login <username> <password>\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }
        
        else if(client.size() != 0 && client[current_client_fd].is_loggin)  //已經登入卻又要登入
        {
            output_buffer = "Please logout first.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else if(find(register_user.begin(), register_user.end(), username) == register_user.end() || user_password[username] != password)   //使用者不存在或密碼錯誤
        {
            output_buffer = "Login failed.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            client[current_client_fd].current_user = username;
            client[current_client_fd].is_loggin = true;
            output_buffer = "Welcome, ";
            output_buffer += username + ".\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

    }

    void Logout()
    {
        if(!client[current_client_fd].is_loggin)    //還沒登入就想登出
        {
            output_buffer = "Please login first.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = "Bye, ";
            output_buffer += client[current_client_fd].current_user + ".\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());

            client[current_client_fd].current_user = "";
            client[current_client_fd].is_loggin = false;
        }
    }

    void Whoami()
    {
        if(!client[current_client_fd].is_loggin)    //還沒登入就問自己是誰
        {
            output_buffer = "Please login first.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = client[current_client_fd].current_user + "\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }
    }

    void List_user()
    {
        output_buffer = "";

        sort(register_user.begin(), register_user.end());   //把user照字母順序排序
        for(auto iter = register_user.begin(); iter != register_user.end(); iter++)
            output_buffer += *iter + "\n";
        writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
    }
    void Exit()
    {
        if(client[current_client_fd].is_loggin)     //若已登入要exit就先跟他說Bye
        {
            output_buffer = "Bye, ";
            output_buffer += client[current_client_fd].current_user + ".\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        close(current_client_fd);   //用nc連線的話,client要再輸入一次才會發現斷線,telnet則會exit後就馬上發現斷線
        client.erase(current_client_fd);
        FD_CLR(current_client_fd, &all_set);
    }

    void Send(string receiver_username, string content)     //允許寄空的訊息,e.g.send 123 ""
    {
        if(receiver_username == "" || content == "")    //參數過少
        {
            output_buffer = "Usage: send <username> <message>\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());

            return;
        }

        if(content.length() >= 3)   // 去除""
            content = content.substr(1, content.length() - 3);    
        
        if(!client[current_client_fd].is_loggin)   //還沒登入就想send
        {
            output_buffer = "Please login first.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else if(find(register_user.begin(), register_user.end(), receiver_username) == register_user.end())     //send的目標不存在
        {
            output_buffer = "User not existed.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            receive_message[receiver_username][client[current_client_fd].current_user].content.push_back(content);
        }
    }

    void List_msg()     //是list出還沒讀的訊息
    {
        if(!client[current_client_fd].is_loggin)    //還沒登入就想list-msg
        {
            output_buffer = "Please login first.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else if(receive_message[client[current_client_fd].current_user].size() == 0)     //沒有未讀的訊息
        {
            output_buffer = "Your message box is empty.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = "";
            for(auto iter = receive_message[client[current_client_fd].current_user].begin(); iter != receive_message[client[current_client_fd].current_user].end(); iter++)
                output_buffer += to_string(iter->second.content.size() - iter->second.first_msg) + " message from " + iter->first + ".\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }
    }
    
    void Receive(string sender_username)    //receive後的訊息會被刪掉
    {
        if(sender_username == "")  //參數過少
        {
            output_buffer = "Usage: receive <username>\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else if(!client[current_client_fd].is_loggin)   //還沒登入就想receive
        {
            output_buffer = "Please login first.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }
        
        else if(find(register_user.begin(), register_user.end(), sender_username) == register_user.end())  //receive的對象不存在
        {
            output_buffer = "User not existed.\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            if(receive_message[client[current_client_fd].current_user].count(sender_username) == 0) return;    //若沒有來自他的訊息直接結束

            output_buffer = receive_message[client[current_client_fd].current_user][sender_username].content[receive_message[client[current_client_fd].current_user][sender_username].first_msg++] + "\n";
            writen(current_client_fd, output_buffer.c_str(), output_buffer.length());

            if(receive_message[client[current_client_fd].current_user][sender_username].content.size() == receive_message[client[current_client_fd].current_user][sender_username].first_msg)
                receive_message[client[current_client_fd].current_user].erase(sender_username);
        }
    }
};

int main(int argc, char **argv)
{
    if(argc <= 1)
    {
        cout << "Usage: ./hw1 <serverport>\n";
        return 0;
    }

    BBS_Server my_server(strtol(argv[1], NULL, 10));
    my_server.multiple_user_server();
    
    return 0;
}