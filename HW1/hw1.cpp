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
    vector<string> content;
    int first_msg = 0;
};

struct client_state
{
    string current_user = "";
    bool is_loggin;
};

class BBS_Server
{
public://晚點再改
    vector<string> register_user;
    vector<client_state> client;
    map<string, string> user_password;
    map<string, map<string, message> > receive_message;

    int listenfd, connectfd;
    socklen_t cli_len;
    struct sockaddr_in servaddr, cliaddr;

    string output_buffer;

    void single_user_server()
    {
        listenfd = socket(AF_INET, SOCK_STREAM, 0);

        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(SERV_PORT); // 要再改
        
        bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr));

        listen(listenfd, LISTENQ); //LISTENQ是backlog的大小

        int input_len = 0;
        char input_buffer[MAXLINE];
        //string input_buffer;
        output_buffer = "% ";

        while(1)
        {
            cli_len = sizeof(cliaddr);
            connectfd = accept(listenfd, (sockaddr *) &cliaddr, &cli_len);
            
            write(connectfd, output_buffer.c_str(), output_buffer.length());
            while((input_len = read(connectfd, input_buffer, MAXLINE)) > 0 )
            {
                action(input_buffer);
                bzero(input_buffer, sizeof(input_buffer));
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

    void Register(string username, string password, string useless)
    {
        cout << "R: u:" << username << " p:" << password << " u:" << useless << "\n";
        if(find(register_user.begin(), register_user.end(), username) != register_user.end())
        {
            output_buffer = "Username is already used.\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            register_user.push_back(username);
            user_password[username] = password;

            output_buffer = "Register successfully\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }   
        
    }

    void Login(string username, string password, string useless)
    {
        if(client[0].is_loggin)
        {
            output_buffer = "Please logout first.\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else if(find(register_user.begin(), register_user.end(), username) == register_user.end() || user_password[username] != password)
        {
            output_buffer = "Login failed.\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            client[0] = {username, true};
            output_buffer = "Welcome, ";
            output_buffer += username + ".\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }

    }

    void Logout(string useless)
    {
        if(!client[0].is_loggin)
        {
            output_buffer = "Please login first.\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = "Bye, ";
            output_buffer += client[0].current_user + ".\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());

            client[0] = {"", false};
        }
    }

    void Whoami(string useless)
    {
        if(!client[0].is_loggin)
        {
            output_buffer = "Please login first.\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = client[0].current_user + "\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }
    }

    void List_user(string useless)
    {
        output_buffer = "";

        sort(register_user.begin(), register_user.end());
        for(auto iter = register_user.begin(); iter != register_user.end(); iter++)
            output_buffer += *iter + "\n";
        write(connectfd, output_buffer.c_str(), output_buffer.length());
    }
    void Exit(string useless)
    {
        if(client[0].is_loggin)
        {
            output_buffer = "Bye, ";
            output_buffer += client[0].current_user + ".\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }
        //離開
    }

    void Send(string receiver_username, string content, string useless)
    {
        if(find(register_user.begin(), register_user.end(), receiver_username) == register_user.end())
        {
            output_buffer = "User not existed.\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            receive_message[receiver_username][client[0].current_user].content.push_back(content);
        }
    }
    void List_msg(string useless)
    {
        if(receive_message[client[0].current_user].size() == 0)
        {
            output_buffer = "Your message box is empty.\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = "";
            for(auto iter = receive_message[client[0].current_user].begin(); iter != receive_message[client[0].current_user].end(); iter++)
                output_buffer += to_string(iter -> second.content.size()) + " message from " + iter -> first + ".\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }
    }
    void Receive(string username, string useless)
    {
        if(find(register_user.begin(), register_user.end(), username) == register_user.end())
        {
            output_buffer = "User not existed.\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());
        }

        else
        {
            output_buffer = receive_message[client[0].current_user][username].content[receive_message[client[0].current_user][username].first_msg++] + "\n";
            write(connectfd, output_buffer.c_str(), output_buffer.length());

            if(receive_message[client[0].current_user][username].content.size() == receive_message[client[0].current_user][username].first_msg)
                receive_message[client[0].current_user].erase(username);
        }
    }
};

int main(int argc, char **argv)
{
    int					listenfd, connfd;
	pid_t				childpid;
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;


	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(SERV_PORT);

	bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr));

	listen(listenfd, LISTENQ);

    int n;
    char buf[MAXLINE];
    string buffer;

	for ( ; ; ) {
		clilen = sizeof(cliaddr);
		connfd = accept(listenfd, (sockaddr *) &cliaddr, &clilen);

		if ( (childpid = fork()) == 0) {	/* child process */
			close(listenfd);	/* close listening socket */
			while ( (n = read(connfd, const_cast<char*>(buffer.c_str()), MAXLINE)) > 0)
            {
                
                string test = buffer.data();
                for(int i = 0; i < n; i++) cout << (int)test.at(i) << " ";
                cout << "\n";
                test.at(n - 1) = '\0';
                test.at(0) = '@';
                cout << n << " test " << test << " test.data " << test.data() << " test.cstr " << test.c_str() << " buffer.data() " << buffer.data() << "\n";
                write(connfd, test.c_str(), n);	/* process the request */
            }

		        
			exit(0);
		}
		close(connfd);			/* parent closes connected socket */
	}

    return 0;
}