#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "sharedFunctions.h"


int readSizeClient(int socket){
    int status = 0;
    int bytesRead = 0;
    char buffer[11];
    printf("\tAttempting to read size of next message...\n");
    do{
        status = recv(socket, buffer+bytesRead, 1, MSG_WAITALL);
        bytesRead += status;
    }while(status > 0 && buffer[bytesRead-1] != ':');
    
    buffer[bytesRead-1] = '\0';
    return atoi(buffer);
}

char* readNClient(int socket, int size){
    char* buffer = malloc(sizeof(char) * (size+1));
    printf("\tAttempting to read %d bytes...\n", size);
    recv(socket, buffer, size, MSG_WAITALL);
    buffer[size] = '\0';
    return buffer;
}

void error(char* msg){
    perror(msg);
    exit(1);
}

char* messageHandler(char* msg){
    int size = strlen(msg);
    char* returnMsg = (char*) malloc(12+size); bzero(returnMsg, 12+size);
    sprintf(returnMsg, "%d:%s", size, msg);
    return returnMsg;
}