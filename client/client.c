#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

void error(char* msg){
    perror(msg);
    exit(1);
}

int main(int argc, char* argv[]){
    int sockfd;
    int port;
    int bytes;
    struct sockaddr_in servaddr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3){
        printf("need hostname and port\n");
        exit(0);
    }

    port = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        error("error opening socket");
    }
    else printf("successfully opened socket\n");

    server = gethostbyname(argv[1]);
    if (server == NULL){
        printf("error getting host\n");
        exit(0);
    }
    else printf("successfully found host\n");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    //servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(port);
    if(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) != 0){
        error("error connecting to host\n");
    }
    else printf("successfully connected to host\n");

    printf("Please enter the filename to send: \n");
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);
    bytes = write(sockfd, buffer, strlen(buffer));
    if (bytes < 0){
        printf("error writing to socket\n");
        exit(0);
    }

    bzero(buffer, 256);
    bytes = read(sockfd, buffer, 255);
    if(bytes < 0){
        printf("error reading from socket\n");
        exit(0);
    }
    printf("Received string: %s\n", buffer);
    return 0;


}
