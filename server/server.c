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

pthread_mutex_t alock; 
void* chatFunc(void*);
void error(char*);


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

    char cmd[15];
    clilen = sizeof(cliaddr);
    while(1){
        //accept packets from clients
        newsockfd = accept(sockfd, (struct sockaddr*) &cliaddr, &clilen);
        if(newsockfd < 0){
            printf("server could not accept a client\n");
            continue;
        }
        printf("server accepted client\n");

        bytes = write(newsockfd, "You are connected to the server", 32);
        if(bytes < 0) printf("Could not write to client");
        bytes = read(newsockfd, cmd, sizeof(cmd));
        printf("Chosen Command: %s\n", cmd);
        //thread part  //chatting between client and server
        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, chatFunc, &newsockfd) != 0){
            error("thread creation error");
        }
    }
    // close(sockfd) with a signal handler. can only be killed with sigkill
    return 0;
}


void *chatFunc(void* arg){
    pthread_mutex_init(&alock, NULL);
    //int newsockfd = *(int*)arg;
    //int bytes;
    
    //pthread_mutex_lock(&alock);
    //pthread_mutex_unlock(&alock);
    //close(newsockfd);
}

void error(char* msg){
    perror(msg);
    exit(1);
}