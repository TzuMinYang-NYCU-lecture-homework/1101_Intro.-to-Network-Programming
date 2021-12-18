#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>

using namespace std;

int main(int argc, char** argv)
{
    if(argc < 3) 
    {
        printf("Usage: ./client <server ip> <server port>\n");
        return 0;
    }
    
    struct sockaddr_in servaddr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(strtol(argv[2], NULL, 10));

    /*
    cout << "原本: " << strtol(argv[2], NULL, 10) << "\n";
    cout << "short: " << htons(strtol(argv[2], NULL, 10)) << "\n";
    cout << "long: " << htonl(strtol(argv[2], NULL, 10)) << "\n"; 
    */
    
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    
    string s;
    char input_from_serv[1000], input_from_cli[1000];
    bzero(input_from_serv, sizeof(input_from_serv));
    bzero(input_from_cli, sizeof(input_from_cli));

    FILE *sockfp = fdopen(sockfd, "r+");
    setvbuf(sockfp, NULL, _IONBF, 0);

    read(sockfd, input_from_serv, sizeof(input_from_serv));
    fputs(input_from_serv, stdout);
    bzero(input_from_serv, sizeof(input_from_serv));
    
    while(fgets(input_from_cli, sizeof(input_from_cli), stdin))
    {
        write(sockfd, input_from_cli, sizeof(input_from_cli));
        read(sockfd, input_from_serv, sizeof(input_from_serv));
        //fputs(input_from_cli, sockfp);
        //fgets(input_from_serv, sizeof(input_from_serv), sockfp);
        fputs(input_from_serv, stdout);
        bzero(input_from_serv, sizeof(input_from_serv));
        bzero(input_from_cli, sizeof(input_from_cli));
    }

    return 0;
}