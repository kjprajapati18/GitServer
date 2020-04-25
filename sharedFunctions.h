#ifndef _SHAREDFUNCTIONS_H
#define _SHAREDFUNCTIONS_H

typedef enum command{
    ERROR, checkout, update, upgrade, commit, push, create, destroy, add, rmv, currentversion, history, rollback
} command;

void error(char*);
char* readNClient(int socket, int size);
int readSizeClient(int socket);


#endif