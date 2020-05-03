#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h> 
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>

#include "linkedList.h"

node* addNode(node* head, char* name){
    node* newNode = malloc(sizeof(node));
    newNode->proj = malloc(sizeof(char) * (strlen(name)+1));
    *(newNode->proj) = '\0';
    strcpy(newNode->proj, name);
    pthread_mutex_init(&(newNode->mutex), NULL);
    newNode->next = NULL;

    node* ptr = head;
    if(ptr == NULL){
        return newNode;
    }

    while(ptr->next != NULL){
        ptr = ptr->next;
    }
    ptr->next = newNode;
    return head;
}

node* removeNode(node* head, char* name){
    node *ptr = head, *prev = NULL;
    
    while(ptr != NULL && strcmp(ptr->proj, name)){
        prev = ptr;
        ptr = ptr->next;
    }

    if(ptr == NULL){
        printf("Could not find Node\n");
        return NULL;
    }
    
    if(prev != NULL){
        prev->next = ptr->next;
    } else {
        head = head->next;
    }

    free(ptr->proj);
    pthread_mutex_destroy(&(ptr->mutex));
    free(ptr);

    return head;
}

node* findNode(node* head, char* name){
    node* ptr = head;
    while(ptr != NULL && strcmp(ptr->proj, name)){
        ptr = ptr->next;
    }
    return ptr;
}

node* fillLL(node* head){
    char path[3] = "./";
    DIR* dir = opendir(path);
    if(dir){
        struct dirent* entry;
        readdir(dir);
        readdir(dir);
        while((entry =readdir(dir))){
            if(entry->d_type != DT_DIR) continue;
            char* newPath; 
            int newLen;
            newLen = strlen(entry->d_name) + 10;
            newPath = malloc(newLen); bzero(newPath, sizeof(newLen));
            strcpy(newPath, entry->d_name);
            strcat(newPath, "/.Manifest");
            int file = open(newPath, O_RDWR);
            if(file > 0){
                head = addNode(head, entry->d_name);
                close(file);
            }
            free(newPath);
        }
    }
    
    closedir(dir);
    return head;
}

threadList* addThreadNode(threadList* head, pthread_t thread){
    threadList* newNode = malloc(sizeof(threadList));
    newNode->thread = thread;
    newNode->next = NULL;

    if(head == NULL){
        return newNode;
    }
    
    threadList* ptr = head;

    while(ptr->next != NULL){
        ptr = ptr->next;
    }
    ptr->next = newNode;
    return head;
}

int removeThreadNode(threadList** head, pthread_t thread){

    threadList *ptr = *head, *prev = NULL;
    
    while(ptr != NULL && !pthread_equal(ptr->thread, thread)){
        prev = ptr;
        ptr = ptr->next;
    }
    if(ptr == NULL){
        printf("Could not find thread in the threadList\n");
        return -1;
    }

    if(prev != NULL){
        prev->next = ptr->next;
    } else {
        *head = ptr->next;
    }
    free(ptr);
    return 0;

}


void printLL(node* head){
    node* ptr = head;
    while(ptr!=NULL){
        printf("%s -> ", ptr->proj);
        ptr= ptr->next;
    }
    printf("NULL\n");
}


void joinAll(threadList* head){
    
    while(head != NULL){
        threadList* temp = head->next;
        pthread_join(head->thread, NULL);
        free(head);
        head = temp;
    }
}

void freeMutexList(node* head){
    while(head != NULL){
        node* temp = head->next;
        free(head->proj);
        pthread_mutex_destroy(&(head->mutex));
        free(head);
        head = temp;
    }
}