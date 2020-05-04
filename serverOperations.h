#ifndef _SERVEROPERATIONS_H
#define _SERVEROPERATIONS_H

void* performCreate(int, void*);
void* performDestroy(int, void*);
void* performCurVer(int, void*); //Perform Current Version && perform Update are the same
void performHistory(int, void*);
void* performUpgradeServer(int, void*);
void* performPushServer(int, void*, char*);
void* performCommit(int, void*, char*);
void* performCheckout(int, void*);
void* performRollback(int, void*);

int createProject(int, char*);
int recDest(char*);

pthread_mutex_t headLock;

#endif