#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h> 
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/select.h>
#include <openssl/md5.h>

#include "../linkedList.h"
#include "../avl.h"
#include "../serverOperations.h"
#include "../sharedFunctions.h"


/*  
    --SINCE WE CLOSE THE socket on SIGINT, I think that the clients won't be able to write back to server
    --We can make a readFindProject function since it is basically the same for all performs
    --performCurVer && performUpdate are exact same code since all the server needs to do is send the Manifest
    --Let threads remove themselves from LINKED LIST OF JOINS
    --Do stuff mentioned in client todo list
*/

void* switchCase(void* arg);
int killProgram = 0;
int sockfd;

void interruptHandler(int sig){
    killProgram = 1;
    close(sockfd);
}

int main(int argc, char* argv[]){

    if(argc != 2) error("Fatal Error: Please enter 1 number as the argument (port)\n");

    signal(SIGINT, interruptHandler);
    setbuf(stdout, NULL);
    int newsockfd;
    int clilen;
    int servlen;
    int bytes;
    char buffer[256];
    node* head = NULL;
    threadList* threadHead = NULL;
    struct sockaddr_in servaddr;
    struct sockaddr_in cliaddr;

    pthread_mutex_init(&headLock, NULL);
    //destroy atexit()?

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

    //Creating list of projects
    printf("Creating list of projects...");
    head = fillLL(head);
    printLL(head);
    printf("Success!\n");

    //listening 
    if (listen(sockfd, 5) != 0){
        printf("listen failed\n");
        exit(0);
    }
    else printf("server listening\n");

    clilen = sizeof(cliaddr);
    data* info = (data*) malloc(sizeof(data));
    info->head = head;
    while(1){
        //accept packets from clients
        
        newsockfd = accept(sockfd, (struct sockaddr*) &cliaddr, &clilen);
        if(killProgram){
            break;
        }

        if(newsockfd < 0){
            printf("server could not accept a client\n");
            continue;
        }

        //Get client IP
        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&cliaddr;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, clientIP, INET_ADDRSTRLEN);

        printf("Server accepted client with IP %s\n", clientIP);
        
        //thread part  //chatting between client and server
        pthread_t thread_id;
        info->socketfd = newsockfd;
        
        //Handle sending in IP. This will get freed by the thread that takes it
        info->clientIP = (char*) malloc((strlen(clientIP)+1) * sizeof(char));
        strcpy(info->clientIP, clientIP);
        //printf("%s\n", info->clientIP);
        if(pthread_create(&thread_id, NULL, switchCase, info) != 0){
            error("thread creation error");
        } else {
            threadHead = addThreadNode(threadHead, thread_id);
        }
    }
    
    //Code below prints out # of thread Nodes.
    /*threadList* ptr = threadHead;
    int j = 0;
    for(j = 0; ptr != NULL; j++){
        printf("%d -> ", j);
        ptr = ptr->next;
    }*/

    //Join all the threads and clean up
    joinAll(threadHead);
    free(info);
    //Free the list of thread_id's and the list of mutexes!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    printf("Successfully closed all sockets and threads!\n");
    
    //socket is already closed from sig handler
    return 0;
}

void* switchCase(void* arg){
    //RECREATE ALL PERFORM FUNCTIONS TO TAKE IN ARGS CUZ SOCKET CHANGES
    //AND WE PASS SOCKET AFTER CHECKIGN THROUGH THE SWITCH CASE
    int newsockfd = ((data*) arg)->socketfd; //PASS THIS IN
    char* clientIP = ((data*) arg)->clientIP;
    int bytes;
    char cmd[3];
    bzero(cmd, 3);
    bytes = write(newsockfd, "You are connected to the server", 32);
    if(bytes < 0) error("Could not write to client");

    printf("Waiting for command..\n");
    bytes = read(newsockfd, cmd, 3);
    
    if (bytes < 0) error("Could not read from client");
    int mode = atoi(cmd);
    printf("Chosen Command: %d\n", mode);

    switch(mode){
        case create:
            performCreate(newsockfd, arg);
            printLL(((data*)arg)->head);
            break;
        case destroy:
            performDestroy(newsockfd, arg);
            printLL(((data*)arg)->head);
            break;
        case currentversion:
        case update:
            performCurVer(newsockfd, arg);
            break;
        case upgrade:
            performUpgradeServer(newsockfd, arg);
            break;
        case history:
            performHistory(newsockfd, arg);
            break;
        case push:
            performPushServer(newsockfd, arg);
            break;
        case commit:
            performCommit(newsockfd, arg, clientIP);
            break;
        case checkout:
            performCheckout(newsockfd, arg);
            break;
        case rollback:
            performRollback(newsockfd, arg);
            break;
    }
    close(newsockfd);
    free(clientIP);
    printf("Disconnected from client\n");
}


char* hash(char* path){
    unsigned char c[MD5_DIGEST_LENGTH];
    int fd = open(path, O_RDONLY);
    MD5_CTX mdContext;
    int bytes;
    unsigned char buffer[256];
    if(fd < 0){
        //printf("cannot open file");
        return NULL;
    }
    MD5_Init(&mdContext);
    do{
        bzero(buffer, 256);
        bytes = read(fd, buffer, 256);
        MD5_Update (&mdContext, buffer, bytes);
    }while(bytes > 0);
    MD5_Final(c, &mdContext);
    close(fd);
    int i;
    char* hash = (char*) malloc(33); bzero(hash, 33);
    char buf[3];
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(buf, "%02x", c[i]);
        strcat(hash, buf);
    }
    return hash;

}
