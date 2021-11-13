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

struct board
{
    int index = -1;
    string name = "", moderator = "";
};

struct comment
{
    string author = "", content = "";
};

struct post
{
    int sn = -1;
    string title = "", content = "", author = "", date = "", belong_board = "";
    vector<comment> comments;
};

struct client_state
{
    string current_user = "", input_buffer = "";
    bool is_loggin = false;
};

class BBS_Server
{
public:

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

                FILE* new_cli_fp = fdopen(new_cli_fd, "r+");
                client[new_cli_fd] = {"", "", false};

                output_buffer = "********************************\n** Welcome to the BBS server. **\n********************************\n% ";
                writen(new_cli_fd, output_buffer.c_str(), output_buffer.length());
                //fprintf(new_cli_fp, "********************************\n** Welcome to the BBS server. **\n********************************\n% ");

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

private:

    //網路的
    int SERV_PORT = 0;
    fd_set rset, all_set;
    int current_client_fd = -1;
    FILE* current_client_fp = NULL;

    //BBS的
    vector<string> register_user;
    map<string, string> user_password;
    map<int, client_state> client;

    int post_sn = 0;
    int board_index = 0;
    vector<string> created_board;
    map<string, board> board_satus;
    map<int, post> post_status;

    string output_buffer, input_buffer;

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
        char char_input_buffer[MAXLINE + 100];
        bzero(char_input_buffer, sizeof(char_input_buffer));
        
        //current_client_fp = fdopen(current_client_fd, "r+");////////////////// 要改
        
        //if(read(current_client_fd, char_input_buffer, MAXLINE) <= 0)
        //if(fgets(input_buffer, MAXLINE, current_client_fp) != EOF)

        if(read(current_client_fd, char_input_buffer, MAXLINE) <= 0)
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

    void action(string input)
    {
        stringstream ss(input);
        string instr = "", arg1 = "", arg2 = "";
        ss >> instr >> arg1 >> arg2;
        
        if(instr == "register") Register(arg1, arg2);
        else if(instr == "login") Login(arg1, arg2);
        else if(instr == "logout") Logout();
        else if(instr == "exit") Exit();
        else if(instr == "create-board") Creat_board(arg1);
        else if(instr == "create-post")
        {
            string title = "", content = "";
            int title_pos = input.find("--title"), content_pos = input.find("--content");
            if(title_pos == std::string::npos || content_pos == std::string::npos)
                Create_post(arg1, title, content);

            else if(arg2 == "--title")
            {
                title = input.substr(title_pos + 7 + 1, content_pos - (title_pos + 7 + 1));
                content = input.substr(content_pos + 9 + 1);
                while(content.find("<br>") != std::string::npos) 
                    content.replace(content.find("<br>"), 4, "\n");
                content += "\n";    //最後補一個\n給它

                Create_post(arg1, title, content);
            }

            else if(arg2 == "--content")
            {
                content = input.substr(content_pos + 9 + 1, title_pos - (content_pos + 9 + 1));
                while(content.find("<br>") != std::string::npos) 
                    content.replace(content.find("<br>"), 4, "\n");
                title = input.substr(title_pos + 7 + 1, input.length() - (title_pos + 7 + 1) - 1); //不要連最後的\n都放進去

                Create_post(arg1, title, content);
            }
            //可能有問題
        }
        else if(instr == "list-board") List_board();
        else if(instr == "list-post") List_post(arg1);
        else if(instr == "read") Read(strtol(arg1.c_str(), NULL, 10));  //不確定
        else if(instr == "delete-post") Delete_post(strtol(arg1.c_str(), NULL, 10)); //不確定
        else if(instr == "update-post")
        {
            string title = "", content = "";
            if(arg2 == "") Update_post(strtol(arg1.c_str(), NULL, 10), arg2, "");

            else if(arg2 == "--title")
            {
                title = input.substr(input.find("--title") + 7 + 1);
                Update_post(strtol(arg1.c_str(), NULL, 10), arg2, title);
            }

            else if(arg2 == "--content")
            {
                content = input.substr(input.find("--content") + 9 + 1);
                while(content.find("<br>") != std::string::npos) 
                    content.replace(content.find("<br>"), 4, "\n");

                Update_post(strtol(arg1.c_str(), NULL, 10), arg2, content);
            }
        }
        else if(instr == "comment") Comment(strtol(arg1.c_str(), NULL, 10), input.substr(input.find(arg2))) ;
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

    void Creat_board(string boardname)
    {
        if(boardname == "") //參數過少
        ;

        else if(client.size() != 0 && !client[current_client_fd].is_loggin)  //沒登入就想建
        ;

        else if(created_board.size() != 0 && find(created_board.begin(), created_board.end(), boardname) != created_board.end())   //若看板已存在
        ;

        else
        {
            created_board.push_back(boardname);
            //board_satus[boardname] = {++board_index, boardname, client[current_client_fd].current_user};
            //輸出
        }
    }

    void Create_post(string boardname, string title, string content)
    {
        if(content == "") //參數過少
        ;

        else if(client.size() != 0 && !client[current_client_fd].is_loggin)  //沒登入就想po
        ;

        else if(created_board.size() == 0 || find(created_board.begin(), created_board.end(), boardname) == created_board.end())   //若看板不存在
        ;

        else
        {
            //post_status[post_sn] = {++post_sn, title, content, client[current_client_fd].current_user, "", boardname}; //date先亂給, comment沒給
            //輸出
        }
    }

    void List_board()
    {
        for(int i = 0; i < board_index; i++)
        ;
    }

    void List_post(string boardname)
    {
        if(boardname == "") //參數過少
        ;

        else if(created_board.size() == 0 || find(created_board.begin(), created_board.end(), boardname) == created_board.end())   //若看板不存在
        ;

        else
        {
            for(int i = 0; i < post_status.size(); i++)
            {
                if(post_status[i].belong_board == boardname)
                ;
            }
            
        }
    }

    void Read(int sn)
    {
        if(sn == 0) //參數過少 要注意這邊是0
        ;

        else
        {
            
        }
    }

    void Delete_post(int sn)
    {
        if(sn == 0) //參數過少 要注意這邊是0
        ;

        else if(client.size() != 0 && !client[current_client_fd].is_loggin)  //沒登入就想刪
        {
            
        }

        else if(post_status.size() == 0 || post_status.find(sn) == post_status.end())   //若post不存在
        {

        }

        else if(post_status[sn].author != client[current_client_fd].current_user) //若不是post的作者
        {

        }

        else
        {
            post_status.erase(sn);
        }
    }

    void Update_post(int sn, string update_type, string new_tilte_or_content)
    {
        if(new_tilte_or_content == "") //參數過少
        ;

        else if(client.size() != 0 && !client[current_client_fd].is_loggin)  //沒登入就想更新
        {
            
        }

        else if(post_status.find(sn) == post_status.end()) //若此post不存在
        {

        }

        else if(post_status[sn].author != client[current_client_fd].current_user) //若不是post的作者
        {

        }

        else
        {
            if(update_type == "--title") post_status[sn].title = new_tilte_or_content;
            else if(update_type == "--content") post_status[sn].content = new_tilte_or_content;
            //輸出
        }
    }

    void Comment(int sn, string new_comment_content)
    {
        if(new_comment_content == "") //參數過少
        ;

        else if(client.size() != 0 && !client[current_client_fd].is_loggin)  //沒登入就想comment
        {
            
        }

        else if(post_status.find(sn) == post_status.end()) //若此post不存在
        {

        }

        else
        {
            post_status[sn].comments.push_back({client[current_client_fd].current_user, new_comment_content});
            //輸出
        }
    }
};

int main(int argc, char **argv)
{
    if(argc <= 1)
    {
        cout << "Usage: ./hw2 <serverport>\n";
        return 0;
    }

    BBS_Server my_server(strtol(argv[1], NULL, 10));
    my_server.multiple_user_server();
    
    return 0;
}
