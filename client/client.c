#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void error(char* msg){
    perror(msg);
    exit(1);
}

typedef enum command{
    ERROR, checkout, update, upgrade, commit, push, create, destroy, add, rmv, currentversion, history, rollback
} command;

void writeConfigureFile(char* IP, char* port);
void sendServerCommand(int socket, char* command, int comLen);
char* getConfigInfo(int config, int* port);
int writeString(int fd, char* string);
//int connectToServer(char* ipAddr, int port);
command argCheck(int argc, char* arg);

int main(int argc, char* argv[]){
    //int sockfd;
    int port;
    char* ipAddr;
    int bytes;

    char buffer[256];
    //Should we check that IP/Host && Port are valid
    if(strcmp(argv[1], "configure") == 0){
        if(argc < 4) error("Not enough arguments for configure. Need IP & Port\n");
        writeConfigureFile(argv[2], argv[3]);
        return 0;
    }

    int configureFile = open(".configure", O_RDONLY);
    if(configureFile < 0) error("Fatal Error: There is no configure file. Please use ./WTF configure <IP/host> <Port>\n");
    ipAddr = getConfigInfo(configureFile, &port);
    close(configureFile);

    //connectToServer(ipAddr, port);
    struct sockaddr_in servaddr;
    struct hostent *server;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        error("error opening socket");
    }
    else printf("successfully opened socket\n");

    server = gethostbyname(ipAddr);
    free(ipAddr); //No longer needed

    if (server == NULL){
        printf("Fatal Error: Cannot get host. Is the given IP/host correct?\n");
        exit(0);
    }
    else printf("successfully found host\n");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    //servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(port);
    while(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) != 0){
        printf("Error: Could not connect to server. Retrying in 3 seconds...\n");
        sleep(3);
    }
    printf("successfully connected to host.\n");


    //figure out what command to operate
    command mode = argCheck(argc, argv[1]);
    if(mode == ERROR){
        close(sockfd);
        return -1;
    }
    
    sendServerCommand(sockfd, argv[1], strlen(argv[1]));
    sendServerCommand(sockfd, argv[2], strlen(argv[2]));
    //write(sockfd, argv[1], strlen(argv[1]));
    switch(mode){
        case checkout:
            bytes = read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("checkout\n");
            break;
        case update: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);            
            printf("update\n");
            break; 
        case upgrade: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("upgrade\n");
            break;
        case commit: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("commit\n");
            break;
        case push: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("push\n");
            break;
        case create: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("create\n");
            break;
        case destroy: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            write(sockfd, argv[1], sizeof(argv[1]));
            break;
        case add: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("add\n");
            break;
        case rmv: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("remove\n");
            break;
        case currentversion: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("currentversion\n");
            break;
        case history: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("history\n");
            break;
        case rollback: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("rollback\n");
            break;
        default:
            break;
    }
    return 0;


}

void sendServerCommand(int socket, char* command, int comLen){

    int newLen = comLen+2;
    char sending[newLen];
    sending[0] = '\0';
    strcpy(sending, command);
    sending[newLen-2] = ':';
    sending[newLen-1] = '\0';
    write(socket, sending, newLen);
}

void writeConfigureFile(char* IP, char* port){
    remove(".configure");
    int configureFile = open(".configure", O_CREAT | O_WRONLY, 00600);
    if(configureFile < 0) error("Could not create a .configure file\n");
    
    int writeCheck = 0;
    writeCheck += writeString(configureFile, IP);
    writeCheck += writeString(configureFile, "\n");
    writeCheck += writeString(configureFile, port);
    close(configureFile);
    if(writeCheck != 0) error("Could not write to .configure file\n");
}

//Will read the config file and return output in ipAddr and port
//On error, it will close the config file and quit
char* getConfigInfo(int config, int* port){
    char buffer[256]; //The config file should not be more than a couple of bytes
    int status = 0, bytesRead = 0;
    char* ipRead = NULL;
    *port = 0;

    do{
        status = read(config, buffer + bytesRead, 256-bytesRead);
        if(status < 0){
            close(config);
            error("Fatal Error: Unable to read .configure file\n");
        }
        bytesRead += status;
    } while(status != 0);

    int i;
    for(i = 0; i<bytesRead; i++){
        // '\n' is the delimiter in .configure. Therefore, copy everything before it into the IP address, and everything after will be the port
        if(buffer[i] == '\n'){
            buffer[i] = '\0';

            ipRead = (char*) malloc(sizeof(char) * (i+1));
            *ipRead = '\0';
            strcpy(ipRead, buffer);

            *port = atoi(buffer+i+1);
            break;
        }
    }

    if(*port == 0 || ipRead == NULL){
        close(config);
        error("Fatal Error: .configure file has been corrupted or changed. Please use the configure command to fix file\n");
    }    

    return ipRead;
}

//Write to fd. Return 0 on success but 1 if there was a write error
int writeString(int fd, char* string){

    int status = 0, bytesWritten = 0, strLength = strlen(string);
    
    do{
        status = write(fd, string + bytesWritten, strLength - bytesWritten);
        bytesWritten += status;
    }while(status > 0 && bytesWritten < strLength);
    
    if(status < 0) return 1;
    return 0;
}

/*int connectToServer(char* ipAddr, int port){
    struct sockaddr_in servaddr;
    struct hostent *server;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        error("error opening socket");
    }
    else printf("successfully opened socket\n");

    server = gethostbyname(ipAddr);
    
    if (server == NULL){
        printf("Fatal Error: Cannot get host. Is the given IP/host correct?\n");
        exit(0);
    }
    else printf("successfully found host\n");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    //servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(port);
    while(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) != 0){
        printf("Error: Could not connect to server. Retrying in 3 seconds...\n");
        sleep(3);
    }
    printf("successfully connected to host\n");
}*/

command argCheck(int argc, char* arg){
    command mode;
    if(strcmp(arg, "checkout") == 0) mode = checkout;
    else if(strcmp(arg, "update") == 0) mode = update;
    else if(strcmp(arg, "upgrade") == 0) mode = upgrade;
    else if(strcmp(arg, "commit") == 0) mode = commit;
    else if(strcmp(arg, "create") == 0){
        if(argc == 3) mode = create;
        else printf("Fatal Error: create requires only 1 argument (project name)\n");
    }
    else if(strcmp(arg, "destroy") == 0) mode = destroy;
    else if(strcmp(arg, "add") == 0) mode = add;
    else if(strcmp(arg, "remove") == 0) mode = rmv;
    else if(strcmp(arg, "currentversion") == 0) mode = currentversion;
    else if(strcmp(arg, "history") == 0) mode = history;
    else if(strcmp(arg, "rollback") == 0) mode = rollback;
    else mode = -1;
    
    return mode;
}