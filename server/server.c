#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

void* chatFunc(void* arg){

    write(newsockfd, "You are connected to the server", 32);
    bytes= read(newsockfd, buffer, 255);
    if(bytes < 0) printf("error reading from socket");
    printf("Received filename: %s", buffer);
    buffer[12] = '\0';
    printf("String Lenght: %d\n", strlen(buffer));
    int filefd = open(buffer, O_RDWR);
    if(filefd < 0){
        printf("FD: %d\n", filefd);
        error("couldn't open file");
        close(filefd);
    }
    bzero(buffer, 256);
    bytes = read(filefd, buffer, 255);
    printf("Sending string: %s\n", buffer);
    bytes = write(newsockfd, buffer, bytes);
    if (bytes <  0) printf("error writing to socket\n");
    //close socket
    close(sockfd);
}

void error(char* msg){
    perror(msg);
    exit(1);
}

int main(int argc, char* argv[]){
    int sockfd;
    int newsockfd;
    int clilen;
    int servlen;
    int bytes;
    char buffer[256];
    struct sockaddr_in servaddr;
    struct sockaddr_in cliaddr;

    //socket creation
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        printf("socket could not be made \n");
        exit(0);
    }
    else printf("Socket created \n");

    //zero out servaddr
    bzero(&servaddr, sizeof(servaddr));

    //asign IP and port
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    int port = atoi(argv[1]);
    servaddr.sin_port = htons(port);

    servlen = sizeof(servaddr);
    //bind socket and ip
    if(bind(sockfd, (struct sockaddr *) &servaddr, servlen) != 0){
        printf("socket bind failed\n");
        exit(0);
    }
    else printf("Socket binded\n");


    //listening 
    if (listen(sockfd, 5) != 0){
        printf("listen failed\n");
        exit(0);
    }
    else printf("server listening\n");


    clilen = sizeof(cliaddr);

    bzero(buffer, 256); 

    //accept packets from client
    newsockfd = accept(sockfd, (struct sockaddr*) &cliaddr, &clilen);
    if(newsockfd < 0){
        printf("server could not accept\n");
        exit(0);
    }
    else printf("server accepted client\n");
    //thread part
    pthread_t thread_id;
    if(pthread_create(&thread_id, NULL, chatFunc, arg))

    //chatting between client and server
    
    return 0;
}
