#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/md5.h>

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


//Separate function for tarFile since treating it like a string causes undefined behavior
//Sends in form <pathLen>:<path><dataLen>:<data>
int sendTarFile(int socket, char* tarPath){
    int tarFd = open(tarPath, O_RDONLY);
    if(tarFd < 0){
        printf("Could not open tar file for sending\n");
        return -1;
    }
    int bytesRead = 0, status = 0;
    int tarSize = lseek(tarFd, 0, SEEK_END);
    lseek(tarFd, 0, SEEK_SET);
    char* tarData = (char*) malloc(tarSize +1);
    do{
        status = read(tarFd, tarData + bytesRead, tarSize - bytesRead);
        bytesRead+=status;
    }while(status > 0);
    close(tarFd);
    if(status <0){
        printf("Could open tar but could not read for sending\n");
        free(tarData);
        return -1;
    }
    //remove(tarPath);

    int tarPathLen = strlen(tarPath);
    
    char sendBuffer[tarPathLen+tarSize+27];
    sprintf(sendBuffer, "%d:%s%d:", tarPathLen, tarPath, tarSize);
    write(socket, sendBuffer, strlen(sendBuffer));
    write(socket, tarData, tarSize);
    free(tarData);
}

char* readWriteTarFile(int sockfd){

    int tarFilePathLen = readSizeClient(sockfd);
    char* tarFilePath = readNClient(sockfd, tarFilePathLen);
    int tarFileDataLen = readSizeClient(sockfd);
    char* tarFile = (char*) malloc(sizeof(char) * tarFileDataLen); 
    int checkTar = read(sockfd, tarFile, tarFileDataLen);

    if(checkTar < 0){
        printf("Could not read tar file from server\n");
        free(tarFilePath); free(tarFile);
        return NULL;
    }

    int tarFd = open(tarFilePath, O_WRONLY | O_CREAT, 00700);
    if(tarFd < 0){
        printf("Could read tar but could not create tar file\n");
        free(tarFilePath); free(tarFile);
        //sendFail(sockfd);*/
        return NULL;
    }

    writeNString(tarFd, tarFile, tarFileDataLen);
    close(tarFd);
    free(tarFile);

    return tarFilePath;
}