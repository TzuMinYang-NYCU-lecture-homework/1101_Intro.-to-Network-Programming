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

#define MAXLINE 4095

using namespace std;

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

int main(int argc, char *argv[]) {
    if(argc < 3)
    {
        printf("Usage: ./a.out <challenge_name> <port_number>");
        return 0;
    }
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    /*struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);*/

    struct sockaddr servaddr = lookup_udp(argv[1], argv[2]);
    connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    char from_serv[MAXLINE + 100];
    string str_from_serv = "";

    while(str_from_serv.find("=====") == std::string::npos)
    {
        bzero(from_serv, MAXLINE);
        read(sockfd, from_serv, MAXLINE);
        str_from_serv += from_serv;
    }
    

    cout << str_from_serv;

    string temp = str_from_serv.substr(str_from_serv.find("total") + 5, str_from_serv.find("round") - (str_from_serv.find("total") + 5));
    int round = atoi(temp.c_str()) + 1;
    //cout << str_from_serv.find("total") + 5 << "\n" << str_from_serv.find("round") << "\n" << temp << " " << round << "\n";


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


	return 0;
}