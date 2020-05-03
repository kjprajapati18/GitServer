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
    char* buffer = malloc(sizeof(char) * (size+1));
    printf("\tAttempting to read %d bytes...\n", size);
    read(socket, buffer, size);
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

//Writes #:Data for manifest 
//# is the size of the Manifest while Data is the actual content
//REPLACE A BUNCH OF THESE LINES WITH readFile(path)
int sendFile(int sockfd, char* pathName){

    int manifest = open(pathName, O_RDONLY);
    if(manifest < 0) return 2;
    int fileSize = (int) lseek(manifest, 0, SEEK_END);
    lseek(manifest, 0, SEEK_SET);
    
    int pathLen = strlen(pathName);

    int sendSize = pathLen + 26 + fileSize; //26 accounts for : and digits in string and \0
    char* fileData = (char*) malloc(sizeof(char) * (sendSize)); bzero(fileData, sendSize);
    
    sprintf(fileData, "%d:%s%d:", pathLen, pathName, fileSize);

    int status = 0, bytesRead = 0, start = strlen(fileData);
    
    do{
        status = read(manifest, fileData + bytesRead+start, fileSize - bytesRead);
        bytesRead += status;
    }while(status > 0 && bytesRead < fileSize);
    
    close(manifest);
    if(status < 0){
        free(fileData);
        return 1;
    }

    write(sockfd, fileData, start+bytesRead);    
    free(fileData);
    return 0;
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

int writeNString(int fd, char* string, int len){
    int status = 0, bytesWritten = 0, strLength = len;
    
    do{
        status = write(fd, string + bytesWritten, strLength - bytesWritten);
        bytesWritten += status;
    }while(status > 0 && bytesWritten < strLength);
    
    if(status < 0) return 1;
    return 0;
}

void sendFail(int socket){
    write(socket, "4:fail", 6);
}

char* stringFromFile(char* path){
    int fd = open(path, O_RDONLY);
    if(fd < 0) return NULL;

    int status = 0, bytesRead = 0;
    int fileSize = (int) lseek(fd, 0, SEEK_END);
    char* fileData = (char*) malloc((fileSize+1)*sizeof(char));
    fileData[fileSize] = '\0';
    lseek(fd, 0, SEEK_SET);
    do{
        status = read(fd, fileData + bytesRead, fileSize - bytesRead);
        bytesRead += status;
    }while(status > 0 && bytesRead < fileSize);
    
    close(fd);
    if(status < 0){
        free(fileData);
        return NULL;
    }
    return fileData;
}