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

#include "linkedList.h"
#include "avl.h"
#include "serverOperations.h"
#include "sharedFunctions.h"


void* switchCase(void* arg);    //Thread creation and operation handler
void interruptHandler(int sig);

//Global variables that interrupt handler needs
int killProgram = 0;
int sockfd;

//Globals for proper thread closing and freeing
int ret;
pthread_mutex_t threadListLock; 



int main(int argc, char* argv[]){
    //Should only open with port #
    if(argc != 2) error("Fatal Error: Please enter 1 number as the argument (port)\n");

    //Set up for server creation && signal handlers && remove buffer from stdout
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

    //Init the mutex of mutexlist and mutex of threadList
    pthread_mutex_init(&headLock, NULL);
    pthread_mutex_init(&threadListLock, NULL);

    //socket creation
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        printf("Socket could not be made \n");
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
        printf("Socket bind failed. Please try again or choose a different port\n");
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

    //Initialize for structs for accept
    clilen = sizeof(cliaddr);
    //info struct will hold all data we want to pass into thread
    data* info = (data*) malloc(sizeof(data));
    info->head = head;
    info->threadHead = NULL;
    printf("Server is ready to accept clients!\n");

    while(1){
        //accept the client    
        newsockfd = accept(sockfd, (struct sockaddr*) &cliaddr, &clilen);
        if(killProgram){
            //If we sig INT, then we should stop accepting people.
            //Signal handler closes socket so it doesn't get stuck on accept
            break;
        }

        //Couldn't accept
        if(newsockfd < 0){
            printf("Server could not accept a client\n");
            continue;
        }

        //Get client IP and print that we accepted
        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&cliaddr;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, clientIP, INET_ADDRSTRLEN);
        printf("Server accepted client with IP %s\n", clientIP);
        
        //Intialize thread_id and set info to have socket (to pass into thread)
        pthread_t thread_id;
        info->socketfd = newsockfd;
        
        //Handle sending in IP. This will get freed by the thread that takes it
        info->clientIP = (char*) malloc((strlen(clientIP)+1) * sizeof(char));
        strcpy(info->clientIP, clientIP);
        
        //Attempt to create the thread
        if(pthread_create(&thread_id, NULL, switchCase, info) != 0){
            printf("Could not create thread for client. Disconnecting from client...\n");
        } else {
            //If we can create, add to threadList so we can properly free on signals
            pthread_mutex_lock(&threadListLock);
            info->threadHead = addThreadNode(info->threadHead, thread_id);
            pthread_mutex_unlock(&threadListLock);
        }
    }

    //Join all the threads and frees the threadID list
    joinAll(info->threadHead);
    
    //Free all mutexes
    pthread_mutex_destroy(&headLock);
    pthread_mutex_destroy(&threadListLock);
    freeMutexList(info->head);
    
    //Free the info struct and print that we've successfully clean everything
    free(info);
    printf("Successfully closed all sockets and threads!\n");
    
    //socket is already closed from sig handler
    return 0;
}

//Function that will be performed on a thread
void* switchCase(void* arg){
    //First type-cast. Then store all needed info on the stack
    data* info = (data*) arg;
    int newsockfd = info->socketfd;
    char* clientIP = info->clientIP;

    //Send to client that they are connected to the server
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

    //Read command from the client
    bytes = read(newsockfd, cmd, 3);
    if (bytes < 0){
        printf("Could not read from client\n");
        close(newsockfd);
        free(clientIP);
        return NULL;
    }
    
    int mode = atoi(cmd);
    //Perform a different function based on this command
    switch(mode){
        case create:
            performCreate(newsockfd, arg);
            break;
        case destroy:
            performDestroy(newsockfd, arg);
            break;
        case currentversion:
        case update:
            //Both update and currentVersion require only a manifest
            //Everything else is server-sided
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
    //Close the fd to the client and free their IP
    close(newsockfd);
    free(clientIP);

    //If the server is shutting down, don't perform these lines because we are already trying to join these threads
    if(!killProgram){
        pthread_mutex_lock(&threadListLock);
        removeThreadNode(&(info->threadHead), pthread_self());
        pthread_mutex_unlock(&threadListLock);
        pthread_detach(pthread_self());
    }
    //Print that we are disconnected from the client
    printf("Disconnected from client\n");
}

//If SIGINT signal is received, then set killProgram to true, and close the accept socket
void interruptHandler(int sig){
    killProgram = 1;
    close(sockfd);
}