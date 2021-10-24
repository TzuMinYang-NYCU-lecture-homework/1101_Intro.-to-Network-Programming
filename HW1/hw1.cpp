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
    string current_user = "";
    bool is_loggin = false;
};

class BBS_Server
{
public:

    int SERV_PORT = 0;

    BBS_Server(int SERV_PORT) { this->SERV_PORT = SERV_PORT; }

    void single_user_server()
    {
        listenfd = socket(AF_INET, SOCK_STREAM, 0);

        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(SERV_PORT);
        
        bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr));

        listen(listenfd, LISTENQ);  //LISTENQ是backlog的大小

        char input_buffer[100000];
        bzero(input_buffer, sizeof(input_buffer));

        string str_input_buffer = "";

        while(1)
        {
            cli_len = sizeof(cliaddr);
            connectfd = accept(listenfd, (sockaddr *) &cliaddr, &cli_len);
            client[connectfd] = {"", false};

            output_buffer = "********************************\n** Welcome to the BBS server. **\n********************************\n% ";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());

            str_input_buffer = "";
            while(read(connectfd, input_buffer, MAXLINE) > 0 )
            {
                str_input_buffer += input_buffer;
                bzero(input_buffer, sizeof(input_buffer));

                // 發現如果在comman line用輸入的,單行最多只能讀4096(含換行)個字元,疑似和OS的IO buffer有關
                // 用file IO就不會有這個問題,但會有連線太快關掉而收不到server訊息的問題
                if(str_input_buffer.find('\n') != std::string::npos)    //若出現換行,i.e.,讀完一次輸入
                {
                    action(str_input_buffer);

                    output_buffer = "% ";
                    writen(connectfd, output_buffer.c_str(), output_buffer.length());

                    str_input_buffer = "";
                }
            }
            
        }
    }

private:

    vector<string> register_user;
    map<string, string> user_password;
    map<string, map<string, message> > receive_message;
    map<int, client_state> client;

    int listenfd, connectfd;
    socklen_t cli_len;
    struct sockaddr_in servaddr, cliaddr;

    string output_buffer;

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
            if(arg2.length() >= 3) arg2 = arg2.substr(1, arg2.length() - 3);
            Send(arg1, arg2);//send 123 ""不知道要不要算錯
        }
        else if (instr == "list-msg") List_msg();
        else if (instr == "receive") Receive(arg1);
    }

    void Register(string username, string password)
    {
        if(username == "" || password == "")    //參數過少
        {
            output_buffer = "Usage: register <username> <password>\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }
        
        else if(find(register_user.begin(), register_user.end(), username) != register_user.end())  //已註冊的使用者
        {
            output_buffer = "Username is already used.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            register_user.push_back(username);
            user_password[username] = password;

            output_buffer = "Register successfully\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }   
        
    }

    void Login(string username, string password)
    {
        if(username == "" || password == "")    //參數過少
        {
            output_buffer = "Usage: login <username> <password>\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }
        
        else if(client.size() != 0 && client[connectfd].is_loggin)  //已經登入卻又要登入
        {
            output_buffer = "Please logout first.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else if(find(register_user.begin(), register_user.end(), username) == register_user.end() || user_password[username] != password)   //使用者不存在或密碼錯誤
        {
            output_buffer = "Login failed.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            client[connectfd] = {username, true};
            output_buffer = "Welcome, ";
            output_buffer += username + ".\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

    }

    void Logout()
    {
        if(!client[connectfd].is_loggin)    //還沒登入就想登出
        {
            output_buffer = "Please login first.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = "Bye, ";
            output_buffer += client[connectfd].current_user + ".\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());

            client[connectfd] = {"", false};
        }
    }

    void Whoami()
    {
        if(!client[connectfd].is_loggin)    //還沒登入就問自己是誰
        {
            output_buffer = "Please login first.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = client[connectfd].current_user + "\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }
    }

    void List_user()
    {
        output_buffer = "";

        sort(register_user.begin(), register_user.end());   //把user照字母順序排序
        for(auto iter = register_user.begin(); iter != register_user.end(); iter++)
            output_buffer += *iter + "\n";
        writen(connectfd, output_buffer.c_str(), output_buffer.length());
    }
    void Exit()
    {
        if(client[connectfd].is_loggin)     //若已登入要exit就先跟他說Bye
        {
            output_buffer = "Bye, ";
            output_buffer += client[connectfd].current_user + ".\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        close(connectfd);//這樣client要再輸入一次才會發現,不知道這樣能不能
    }

    void Send(string receiver_username, string content)
    {
        if(receiver_username == "" || content == "")    //參數過少
        {
            output_buffer = "Usage: send <username> <message>\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }
        
        else if(!client[connectfd].is_loggin)   //還沒登入就想send
        {
            output_buffer = "Please login first.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else if(find(register_user.begin(), register_user.end(), receiver_username) == register_user.end())     //send的目標不存在
        {
            output_buffer = "User not existed.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            receive_message[receiver_username][client[connectfd].current_user].content.push_back(content);
        }
    }

    void List_msg()     //是list出還沒讀的訊息
    {
        if(!client[connectfd].is_loggin)    //還沒登入就想list-msg
        {
            output_buffer = "Please login first.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else if(receive_message[client[connectfd].current_user].size() == 0)     //沒有未讀的訊息
        {
            output_buffer = "Your message box is empty.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = "";
            for(auto iter = receive_message[client[connectfd].current_user].begin(); iter != receive_message[client[connectfd].current_user].end(); iter++)
                output_buffer += to_string(iter->second.content.size() - iter->second.first_msg) + " message from " + iter->first + ".\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }
    }
    
    void Receive(string sender_username)    //receive後的訊息會被刪掉
    {
        if(sender_username == "")  //參數過少
        {
            output_buffer = "Usage: receive <username>\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else if(!client[connectfd].is_loggin)   //還沒登入就想receive
        {
            output_buffer = "Please login first.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }
        
        else if(find(register_user.begin(), register_user.end(), sender_username) == register_user.end())  //receive的對象不存在
        {
            output_buffer = "User not existed.\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            if(receive_message[client[connectfd].current_user].count(sender_username) == 0) return;    //若沒有來自他的訊息直接結束

            output_buffer = receive_message[client[connectfd].current_user][sender_username].content[receive_message[client[connectfd].current_user][sender_username].first_msg++] + "\n";
            writen(connectfd, output_buffer.c_str(), output_buffer.length());

            if(receive_message[client[connectfd].current_user][sender_username].content.size() == receive_message[client[connectfd].current_user][sender_username].first_msg)
                receive_message[client[connectfd].current_user].erase(sender_username);
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
    my_server.single_user_server();
    
    return 0;
}