#ifndef _SHAREDFUNCTIONS_H
#define _SHAREDFUNCTIONS_H

typedef enum command{
    ERROR, checkout, update, upgrade, commit, push, create, destroy, add, rmv, currentversion, history, rollback
} command;

void error(char*);
char* readNClient(int socket, int size);
int readSizeClient(int socket);
char* messageHandler(char* msg);
char* stringFromFile(char*);
int sendFile(int sockfd, char* pathName);
int writeString(int fd, char* string);
int writeNString(int fd, char* string, int len);
void sendFail(int socket);
char* stringFromFile(char* path);

#endif