#ifndef _LINKEDLIST_H
#define _LINKEDLIST_H

typedef struct _node{
    char* proj;
    pthread_mutex_t mutex;
    struct _node* next;
}node;

typedef struct _node2{
    pthread_t thread;
    struct _node2* next;
}threadList;

typedef struct _data{
    node* head;
    int socketfd;
    char* clientIP;
} data;

node* addNode(node* head, char* name);
node* removeNode(node* head, char* name);
node* findNode(node* head, char* name);
node* fillLL(node* head);
threadList* addThreadNode(threadList* head, pthread_t thread);
void joinAll(threadList* head);
void printLL(node* head);

#endif