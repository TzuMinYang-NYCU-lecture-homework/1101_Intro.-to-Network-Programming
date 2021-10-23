#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

#define SERV_PORT 9877
#define LISTENQ 1024
#define MAXLINE 4096

using namespace std;

struct message
{
    string sender = "";
    vector<string> content;
    int msg_num = 0;
};

class BBS_Server
{
public:
    vector<string> register_user, current_user;
    map<string, string> user_password;
    map<string, message>  receive_message;

    void single_user_server()
    {
        int listenfd, connectfd;
        socklen_t cli_len;
        struct sockaddr_in servaddr, cliaddr;

        listenfd = socket(AF_INET, SOCK_STREAM, 0);

        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(SERV_PORT); // 要再改
        
        bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr));

        listen(listenfd, LISTENQ); //LISTENQ是backlog的大小

        int input_len = 0;
        //char buf[MAXLINE];
        string input_buffer, output_buffer = "% ";

        while(1)
        {
            cli_len = sizeof(cliaddr);
            connectfd = accept(listenfd, (sockaddr *) &cliaddr, &cli_len);
            
            write(connectfd, output_buffer.c_str(), 2);
            while((input_len = read(connectfd, const_cast<char*>(input_buffer.c_str()), MAXLINE)) > 0 )
            {
                output_buffer = input_buffer.data();
                cout << input_buffer << "!!!\n data " << input_buffer.data() << "\n output " << output_buffer << "\n";
                write(connectfd, output_buffer.c_str(), input_len);
                output_buffer = "% ";
                write(connectfd, output_buffer.c_str(), 2);
            }
            
        }
    }

    void action(string input)
    {
        stringstream ss(input);
        string instr = "", arg1 = "", arg2 = "", arg3 = "";
        ss >> instr >> arg1 >> arg2 >> arg3;
        
        if(instr == "register") Register(arg1, arg2, arg3);
        else if (instr == "login") Login(arg1, arg2, arg3);
        else if (instr == "logout") Logout(arg1);
        else if (instr == "whoami") Whoami(arg1);
        else if (instr == "list-user") List_user(arg1);
        else if (instr == "exit") Exit(arg1);
        else if (instr == "send") Send(arg1, arg2, arg3);
        else if (instr == "list-msg") List_msg(arg1);
        else if (instr == "receive") Receive(arg1, arg2);
    }

    void Register(string username, string password, string useless);
    void Login(string username, string password, string useless);
    void Logout(string useless);
    void Whoami(string useless);
    void List_user(string useless);
    void Exit(string useless);

    void Send(string username, string message, string useless);
    void List_msg(string useless);
    void Receive(string username, string useless);
};

int main(int argc, char **argv)
{
    BBS_Server my_server;
    my_server.single_user_server();

    return 0;
}