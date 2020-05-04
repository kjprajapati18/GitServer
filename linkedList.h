#ifndef _LINKEDLIST_H
#define _LINKEDLIST_H

//Node for the list of mutexes (for locking repos)
typedef struct _node{
    char* proj;
    pthread_mutex_t mutex;
    struct _node* next;
}node;

//Node for list of thread ids (for joining and cleaning threads)
typedef struct _node2{
    pthread_t thread;
    struct _node2* next;
}threadList;

//Data structure for passing in info to thread
typedef struct _data{
    node* head;
    threadList* threadHead;
    char* clientIP;
    int socketfd;
} data;

//Standard Add/Remove/Search LL functions
node* addNode(node* head, char* name);
node* removeNode(node* head, char* name);
node* findNode(node* head, char* name);

//Fill linked list of project mutexes based on what folders are already in the server
node* fillLL(node* head);

//Standard Add/Remove LL functions
threadList* addThreadNode(threadList* head, pthread_t thread);
int removeThreadNode(threadList** head, pthread_t thread);

//Joins all threads and frees the list
void joinAll(threadList* head);
//Free the list of mutexes
void freeMutexList(node* head);
//Print Linked list in order
void printLL(node* head);

#endif