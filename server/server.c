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
#include <dirent.h>

pthread_mutex_t alock; 
void* chatFunc(void*);
void error(char*);
int readCommand(int socket, char** buffer);
int createProject(int socket, char* name);
char* readNClient(int socket, int size);
void* destroy(void*);

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
    while(1){
        //accept packets from clients
        newsockfd = accept(sockfd, (struct sockaddr*) &cliaddr, &clilen);
        if(newsockfd < 0){
            printf("server could not accept a client\n");
            continue;
        }
        printf("server accepted client\n");
        char cmd[3];
        bzero(cmd, 3);
        bytes = write(newsockfd, "You are connected to the server", 32);
        if(bytes < 0) error("Could not write to client");
        bytes = read(newsockfd, cmd, 3);
        if (bytes < 0) error("Coult not read from client");
        printf("Chosen Command: %d\n", cmd);
        int mode = atoi(cmd);
        //switch case on mode corresponding to the enum in client.c. make thread for each function.
        //thread part  //chatting between client and server
        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, chatFunc, &newsockfd) != 0){
            error("thread creation error");
        }
    }
    // close(sockfd) with a signal handler. can only be killed with sigkill
    return 0;
}

void* destroy(void* arg){
    pthread_mutex_init(&alock, NULL);
    int socket = *((int*) arg);
    int bytes = readSizeClient(socket);
    char projName[bytes + 1];
    projName[bytes] = '\0';
    read(socket, projName, bytes);
    //now projName has the string name of the file to destroy
    DIR* dir = opendir(projName);
    if(dir < 0) {
        write(socket, "Fatal Error: Project does not exist", 37);
        return NULL;
    }
    else{
        //destroy files and send success msg
        write(socket, "Successfully destroyed project\0\0\0\0\0\0", 37);
    }

    
}

void *chatFunc(void* arg){
    pthread_mutex_init(&alock, NULL);
    int socket = *((int*) arg);
    char* cmd = malloc(16*sizeof(char));
    printf("yea\n");
    readCommand(socket, &cmd);

    //bytes = read(newsockfd, cmd, sizeof(cmd));
    printf("Chosen Command: %s\n", cmd);
    if(strcmp("create", cmd) == 0){

        printf("Succesful create message received\n");
        //write(socket, "completed", 10);
        char* projectName = readNClient(socket, readSizeClient(socket));
        int creation = createProject(socket, projectName);
        free(projectName);
    }
    //int newsockfd = *(int*)arg;
    //int bytes;
    
    //pthread_mutex_lock(&alock);
    //pthread_mutex_unlock(&alock);
    close(socket);
    free(cmd);
}

int readCommand(int socket, char** buffer){
    int status = 0, bytesRead = 0;
    do{
        status = read(socket, *buffer+bytesRead, 1);
        bytesRead += status;
    }while(status > 0 && (*buffer)[bytesRead-1] != ':');
    (*buffer)[bytesRead-1] = '\0';
    return 0;
}

int readSizeClient(int socket){
    int status = 0, bytesRead = 0;
    char buffer[11];
    do{
        status = read(socket, buffer+bytesRead, 1);
        bytesRead += status;
    }while(status > 0 && buffer[bytesRead-1] != ':');
    buffer[bytesRead-1] = '\0';
    return atoi(buffer);
}

char* readNClient(int socket, int size){
    char* buffer = malloc(sizeof(char) * (size+1));
    read(socket, buffer, size);
    buffer[size] = '\0';
    return buffer;
}

int createProject(int socket, char* name){
    printf("%s\n", name);
    return 0;
}

void error(char* msg){
    perror(msg);
    exit(1);
}