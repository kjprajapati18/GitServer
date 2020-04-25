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
        status = read(socket, buffer+bytesRead, 1);
        bytesRead += status;
    }while(status > 0 && buffer[bytesRead-1] != ':');
    
    buffer[bytesRead-1] = '\0';
    return atoi(buffer);
}

char* readNClient(int socket, int size){
    char* buffer = malloc(sizeof(char) * (size));
    printf("\tAttempting to read %d bytes...\n", size);
    read(socket, buffer, size);
    buffer[size-1] = '\0';
    return buffer;
}

void error(char* msg){
    perror(msg);
    exit(1);
}