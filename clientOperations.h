#ifndef _CLIENTOPERATIONS_H
#define _CLIENTOPERATIONS_H
#include "avl.h"
#include "sharedFunctions.h"

int performRollback(int, char**, int);
int performAdd(char**);
int performRemove(char**);
void sendServerCommand(int socket, char* command, int comLen);
void writeConfigureFile(char* IP, char* port);
char* getConfigInfo(int config, int* port);                     //returns IP and Port from .configure
command argCheck(int argc, char* arg);
int performCreate(int socket, char** argv);
int performUpdate(int sockfd, char** argv);
int performCommit(int, char**);
int performUpgrade(int, char**, char*);
int performCheckout(int, char**);
int fileCreator(char*);
int performPush(int, char**, char*);
void printCurVer(char* manifest);

//Finds conflicts, deletes, and modifies for update
int manDifferencesCDM(int, int, avlNode*, avlNode*);
//Finds adds for update
int manDifferencesA(int, int, avlNode*, avlNode*);
//Finds all differences between manifests/files for commit
int commitManDiff(int, avlNode*, int);

#endif