#ifndef _SHAREDFUNCTIONS_H
#define _SHAREDFUNCTIONS_H

typedef enum command{
    ERROR, checkout, update, upgrade, commit, push, create, destroy, add, rmv, currentversion, history, rollback
} command;

void error(char*);
//Read the next size bytes
char* readNClient(int socket, int size);
//Read size of next message
int readSizeClient(int socket);
//Convert string to our protocol
char* messageHandler(char* msg);
//Read a file path and create a string of file data
char* stringFromFile(char*);
//Send a file over socket
int sendFile(int sockfd, char* pathName);
//Write a string to a fileDescriptor
int writeString(int fd, char* string);
//Write N bytes of a string to a file descriptor
int writeNString(int fd, char* string, int len);
//Send fail to other side
void sendFail(int socket);
//Hash a file given a file path
char* hash(char*);
//Send a tar file over the socket given path
int sendTarFile(int socket, char* tarPath);
//Read a tar file from server and create the tar file in ./ directory
char* readWriteTarFile(int sockfd);

#endif