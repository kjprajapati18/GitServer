#ifndef _SERVEROPERATIONS_H
#define _SERVEROPERATIONS_H

void* performCreate(int, void*);
void* performDestroy(int, void*);
void* performCurVer(int, void*);
void performHistory(int, void*);
//void performUpdate(int, void*);
void* performUpgradeServer(int, void*);
void* performPushServer(int, void*);
void* performCommit(int, void*, char*);
void* performCheckout(int, void*);
void* performRollback(int, void*);

int createProject(int, char*);
int recDest(char*);
//char* hash(char*);

pthread_mutex_t headLock;

#endif