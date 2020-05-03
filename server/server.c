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
    --Do stuff mentioned in client todo list
*/

void* switchCase(void* arg);    //Thread creation and operation handler
void interruptHandler(int sig);

int killProgram = 0;
int sockfd; int ret;
pthread_mutex_t threadListLock; 



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
    struct sockaddr_in servaddr;
    struct sockaddr_in cliaddr;

    pthread_mutex_init(&headLock, NULL);
    pthread_mutex_init(&threadListLock, NULL);
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
    info->threadHead = NULL;
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
            pthread_mutex_lock(&threadListLock);
            info->threadHead = addThreadNode(info->threadHead, thread_id);
            pthread_mutex_unlock(&threadListLock);
        }
    }
    
    //Code below prints out # of thread Nodes.
    /*threadList* ptr = threadHead;
    int j = 0;
    for(j = 0; ptr != NULL; j++){
        printf("%d -> ", j);
        ptr = ptr->next;
    }
    printf("NULL\n");*/

    //Join all the threads and frees the threadID list
    joinAll(info->threadHead);
    //Free list of mutexes!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    pthread_mutex_destroy(&headLock);
    pthread_mutex_destroy(&threadListLock);
    freeMutexList(info->head);
    
    free(info);
    printf("Successfully closed all sockets and threads!\n");
    
    //socket is already closed from sig handler
    return 0;
}

void* switchCase(void* arg){
    //RECREATE ALL PERFORM FUNCTIONS TO TAKE IN ARGS CUZ SOCKET CHANGES
    //AND WE PASS SOCKET AFTER CHECKIGN THROUGH THE SWITCH CASE
    data* info = (data*) arg;
    int newsockfd = info->socketfd; //PASS THIS IN
    char* clientIP = info->clientIP;

    int bytes;
    char cmd[3];
    bzero(cmd, 3);
    bytes = write(newsockfd, "You are connected to the server", 32);
    if(bytes < 0){
        printf("Could not write to client\n");
        close(newsockfd);
        free(clientIP);
        return NULL;
    }

    printf("Waiting for command..\n");
    bytes = read(newsockfd, cmd, 3);
    
    if (bytes < 0){
        printf("Could not read from client\n");
        close(newsockfd);
        free(clientIP);
        return NULL;
    }
    
    int mode = atoi(cmd);
    printf("Chosen Command: %d\n", mode);

    switch(mode){
        case create:
            performCreate(newsockfd, arg);
            printLL(info->head);
            break;
        case destroy:
            performDestroy(newsockfd, arg);
            printLL(info->head);
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
            performPushServer(newsockfd, arg, clientIP);
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
    //If the server is shutting down, don't perform these lines because we have to join the threads
    if(!killProgram){
        pthread_mutex_lock(&threadListLock);
        removeThreadNode(&(info->threadHead), pthread_self());
        pthread_mutex_unlock(&threadListLock);
        pthread_detach(pthread_self());
    }
    printf("Disconnected from client\n");
}

void interruptHandler(int sig){
    killProgram = 1;
    close(sockfd);
}