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
#include <sys/select.h>
#include <openssl/md5.h>
#include "../sharedFunctions.h"
#include "../avl.h"

/*  
    --SINCE WE CLOSE THE socket on SIGINT, I think that the clients won't be able to write back to server
    --MOVE SENDFILE to shared functiosns
    --We can make a readFindProject function since it is basically the same for all performs
    --performCurVer && performUpdate are exact same code since all the server needs to do is send the Manifest
    --Do stuff mentioned in client todo list
*/
pthread_mutex_t headLock; 
void* performCreate(int, void*);
void* switchCase(void* arg);
int readCommand(int socket, char** buffer);
int createProject(int socket, char* name);
//char* readNClient(int socket, int size);
void* performDestroy(int, void*);
void* performCurVer(int, void*);
void performHistory(int, void*);
void performUpdate(int, void*);
void* performUpgradeServer(int, void*);
void* performPushServer(int, void*);
void* performCommit(int, void*, char*);
//char* messageHandler(char* msg);
//int sendFile(int sockfd, char* pathName);
char* hash(char*);
int killProgram = 0;
int sockfd;

void interruptHandler(int sig){
    killProgram = 1;
    close(sockfd);
}

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

int main(int argc, char* argv[]){

    if(argc != 2) error("Fatal Error: Please enter 1 number as the argument (port)\n");

    signal(SIGINT, interruptHandler);
    setbuf(stdout, NULL);
    int newsockfd;
    int clilen;
    int servlen;
    int bytes;
    char buffer[256];
    node* head = NULL;              //MAKE A C/H FILE for mutex stuff 
    threadList* threadHead = NULL;
    struct sockaddr_in servaddr;
    struct sockaddr_in cliaddr;

    pthread_mutex_init(&headLock, NULL);
    //destroy atexit()?

    //socket creation
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        printf("socket could not be made \n");
        exit(0);
    }
    else printf("Socket created \n");

    //zero out servaddr
    bzero(&servaddr, sizeof(servaddr));

    //asign IP and port
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    int port = atoi(argv[1]);
    servaddr.sin_port = htons(port);

    servlen = sizeof(servaddr);
    //bind socket and ip
    if(bind(sockfd, (struct sockaddr *) &servaddr, servlen) != 0){
        printf("socket bind failed\n");
        exit(0);
    }
    else printf("Socket binded\n");

    //Creating list of projects
    printf("Creating list of projects...");
    head = fillLL(head);
    printLL(head);
    printf("Success!\n");

    //listening 
    if (listen(sockfd, 5) != 0){
        printf("listen failed\n");
        exit(0);
    }
    else printf("server listening\n");

    clilen = sizeof(cliaddr);
    data* info = (data*) malloc(sizeof(data));
    info->head = head;
    while(1){
        //accept packets from clients
        
        newsockfd = accept(sockfd, (struct sockaddr*) &cliaddr, &clilen);
        if(killProgram){
            break;
        }

        if(newsockfd < 0){
            printf("server could not accept a client\n");
            continue;
        }

        //Get client IP
        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&cliaddr;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, clientIP, INET_ADDRSTRLEN);

        printf("Server accepted client with IP %s\n", clientIP);
        
        //thread part  //chatting between client and server
        pthread_t thread_id;
        info->socketfd = newsockfd;
        
        //Handle sending in IP. This will get freed by the thread that takes it
        info->clientIP = (char*) malloc((strlen(clientIP)+1) * sizeof(char));
        strcpy(info->clientIP, clientIP);
        //printf("%s\n", info->clientIP);
        if(pthread_create(&thread_id, NULL, switchCase, info) != 0){
            error("thread creation error");
        } else {
            threadHead = addThreadNode(threadHead, thread_id);
        }
    }
    
    //Code below prints out # of thread Nodes.
    /*threadList* ptr = threadHead;
    int j = 0;
    for(j = 0; ptr != NULL; j++){
        printf("%d -> ", j);
        ptr = ptr->next;
    }*/

    //Join all the threads
    joinAll(threadHead);
    printf("Successfully closed all sockets and threads!\n");
    //socket is already closed
    return 0;
}

void* switchCase(void* arg){
    //RECREATE ALL PERFORM FUNCTIONS TO TAKE IN ARGS CUZ SOCKET CHANGES
    //AND WE PASS SOCKET AFTER CHECKIGN THROUGH THE SWITCH CASE
    int newsockfd = ((data*) arg)->socketfd; //PASS THIS IN
    char* clientIP = ((data*) arg)->clientIP;
    int bytes;
    char cmd[3];
    bzero(cmd, 3);
    bytes = write(newsockfd, "You are connected to the server", 32);
    if(bytes < 0) error("Could not write to client");

    printf("Waiting for command..\n");
    bytes = read(newsockfd, cmd, 3);
    
    if (bytes < 0) error("Could not read from client");
    int mode = atoi(cmd);
    printf("Chosen Command: %d\n", mode);

    switch(mode){
        case create:
            performCreate(newsockfd, arg);
            printLL(((data*)arg)->head);
            break;
        case destroy:
            performDestroy(newsockfd, arg);
            printLL(((data*)arg)->head);
            break;
        case currentversion:
        case update:
            performCurVer(newsockfd, arg);
            break;
        case upgrade:
            performUpgradeServer(newsockfd, arg);
            break;
        case history:
            performHistory(newsockfd, arg);
            break;
        case push:
            performPushServer(newsockfd, arg);
            break;
        case commit:
            performCommit(newsockfd, arg, clientIP);
            break;
    }
    close(newsockfd);
    free(clientIP);
    printf("Disconnected from client\n");
}

void* performPushServer(int socket, void* arg){
    char* confirmation = readNClient(socket, readSizeClient(socket));
    if(!strcmp(confirmation, "Commit")){
        printf("There is no commit file. \n");
        free(confirmation);
        return;
    }
    char* projName = readNClient(socket, readSizeClient(socket));
    printf("%s\n", projName);
    node* found = findNode(((data*) arg)->head, projName);
    //if found is null do something
    pthread_mutex_lock(&(found->mutex));
    char commitPath[strlen(projName) + 9];
    char manpath[strlen(projName) + 11];
    sprintf(manpath, "%s/.Manifest", projName);
    sprintf(commitPath, "%s/.Commit", projName);
    int commitfd = open(commitPath, O_RDONLY);
    int size = lseek(commitfd, 0, SEEK_END);
    char commit[size+1];
    int bytesRead = 0, status = 0;
    do{
        status = read(commitfd, commit+bytesRead, size-bytesRead);
        if(status < 0){
            close(commitfd);
            error("Fatal error: unable to read .Commit\n");
        }
        bytesRead+= status;
    }while(status!=0);
    commit[size] = '\0';
    close(commitfd);
    char* servercommithash = hash(commit);
    char* clientcommit = readNClient(socket, readSizeClient(socket));
    char* clientcommithash = hash(clientcommit);
    if(strcmp(clientcommithash, servercommithash)){
        printf("Commits do not match, terminating\n");
        write(socket, "Fail", 4);
        pthread_mutex_unlock(&(found->mutex));
        return;
    }
    else{
        printf("Commits match up\n");
        write(socket, "Succ", 4);
    }
    //dupe directory and rename old one to version number 
    int index = 0; 
    while(commit[index] != '\n') index++;
    commit[index] = '\0';
    int verNum = atoi(commit);
    commit[index] = '\n';
    char syscmd[23+2*strlen(projName)];
    sprintf(syscmd, "cp -r %s old%s", projName, projName);
    system(syscmd);
    sprintf(syscmd, "mv old%s %s/.%d", projName, projName, verNum); 
    system(syscmd);
    int numFiles = readSizeClient(socket);
    int i = 0;
    for(i = 0; i< numFiles; i++){
        char* delPath = readNClient(socket, readSizeClient(socket));
        remove(delPath);
        free(delPath);
    }
    write(socket, "Succ", 4);
    numFiles = readSizeClient(socket);
    for(i = 0; i < numFiles; i++){
        char* path = readNClient(socket, readSizeClient(socket));
        remove(path);
        int addFilefd = open(path, O_CREAT | O_WRONLY, 00600);
        if(addFilefd < 0){
            printf("cannot open path\n");
            write(socket, "FAIL", 4);
        }
        char* fileCont = readNClient(socket, readSizeClient(socket));
        writeString(addFilefd, fileCont);
        free(path); free(fileCont);
        close(addFilefd);
    }
    write(socket, "Succ", 4);
    //manifest creation
    char* manifest = stringFromFile(manpath);
    char* manptr = manifest;
    char* commitptr = commit;
    avlNode* commitHead = fillAvl(&commitptr);
    avlNode* manHead = fillAvl(&manptr);
    manHead = commitChanges(commitHead, manHead); 
    remove(manpath);
    int fd = open(manpath, O_CREAT | O_WRONLY | O_APPEND, 00600);
    char verString[13];
    sprintf(verString, "%d\n", ++verNum);
    writeString(fd, verString);
    writeTree(manHead, fd);
    //success message
    write(socket, "Succ", 4);
}

void* performUpgradeServer(int socket, void* arg){
    char* confirmation = readNClient(socket, readSizeClient(socket));
    if(!strcmp(confirmation, "Conflict")){
        printf("There is a Conflict.\n");
        free(confirmation);
        return;
    } 
    else if(!strcmp(confirmation, "Update")){
        printf("No update file.\n");
        free(confirmation);
        return;
    }
    char *projName = readNClient(socket, readSizeClient(socket));
    printf("%s\n", projName);
    node* found = findNode(((data*) arg)->head, projName);
    //gotta do something here to inform client. give it an ok
    if(found == NULL){
        printf("Cannot find project with given name\n");
        free(projName);
        return;
    }
    //check if found is null i dont do it but we should
    pthread_mutex_lock(&(found->mutex));
    char manifestFile[strlen(projName) + 11];
    sprintf(manifestFile, "%s/.Manifest", projName);
    printf("%s\n", manifestFile);
    int manfd = open(manifestFile, O_RDONLY);
    if(manfd < 0) printf("dumbbutc\n");
    int size = lseek(manfd, 0, SEEK_END);
    lseek(manfd, 0, SEEK_SET);
    char manifest[size+1];
    int bytesRead = 0, status = 0;
    do{
        status = read(manfd, manifest+bytesRead, size-bytesRead);
        if(status < 0){
            close(manfd);
            error("Fatal Error: Unable to read .Manifest\n");
        }
        bytesRead+= status;
    }while(status!= 0);
    manifest[size] = '\0';
    printf("%s\n", manifest);
    close(manfd);
    char* manifestmsg = messageHandler(manifest);
    write(socket, manifestmsg, strlen(manifestmsg));
    free(manifestmsg);
    //char bigboi[11];
    int numFiles = readSizeClient(socket);
    printf("# of files: %d", numFiles);
    int i;
    for(i = 0; i < numFiles; i++){
        char* filepath = readNClient(socket, readSizeClient(socket));
        int fd = open(filepath, O_RDONLY);
        int fileSize = (int) lseek(fd, 0, SEEK_END);
        char file[fileSize+1];
        lseek(fd, 0, SEEK_SET);
        bytesRead = 0; status =0;
        //printf("fi%sle: %s\n", "\0", filepath);
        do{
            status = read(fd, file + bytesRead, fileSize-bytesRead);
            if(status < 0){
                close(fd);
                error("Fatal Error: Unable to read file\n");
            }
            bytesRead+= status;
        }while(status!=0);
        file[fileSize] = '\0';
        //printf("%d to %d\n", fileSize, bytesRead);
        close(fd);
        char* fileToSend = messageHandler(file);
        char* fileNameToSend = messageHandler(filepath);
        char* thing = malloc(strlen(fileToSend) + strlen(fileNameToSend)+1);
        sprintf(thing, "%s%s", fileNameToSend, fileToSend);
        //printf("File: %s\tContent: %s\n", fileNameToSend, fileToSend);
        // write(socket, fileNameToSend, strlen(fileNameToSend));
        // write(socket, fileToSend, strlen(fileToSend));
        write(socket, thing, strlen(thing));
        printf("Sent:\t%s\n", thing);
        free(fileToSend);
        free(filepath);
        free(thing);
    }
    char succ[5]; succ[4] = '\0';
    read(socket, succ, 4);
    printf("succ msg: %s\n", succ); 
    pthread_mutex_unlock(&(found->mutex));
}

void* performDestroy(int socket, void* arg){
    //fully lock this one prob
    
    int bytes = readSizeClient(socket);
    char projName[bytes + 1];
    read(socket, projName, bytes);
    projName[bytes] = '\0';
    //now projName has the string name of the file to destroy
    
    pthread_mutex_lock(&headLock);
    node* found = findNode(((data*) arg)->head, projName);
    if(found == NULL) {
        
        printf("Could not find project with that name to destroy (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to destroy");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }
    else{
        *(found->proj) = '\0';
        //destroy files and send success msg
        //printLL(((data*) arg)->head);
        //if((((data*) arg)->head) == NULL) printf("IT'S NULL\n");
        
        pthread_mutex_lock(&(found->mutex));
        recDest(projName);
        pthread_mutex_unlock(&(found->mutex));

        ((data*) arg)->head = removeNode(((data*) arg)->head, "");

        char* returnMsg = messageHandler("Successfully destroyed project");
        printf("Notifying client\n");
        write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
        printf("Successfully destroyed %s\n", projName);
    }
    
    pthread_mutex_unlock(&headLock);
}

int recDest(char* path){
    DIR* dir = opendir(path);
    int len = strlen(path);
    if(dir){
        struct dirent* entry;
        while((entry =readdir(dir))){
            char* newPath; 
            int newLen;
            if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            newLen = len + strlen(entry->d_name) + 2;
            newPath = malloc(newLen); bzero(newPath, sizeof(newLen));
            strcpy(newPath, path);
            strcat(newPath, "/");
            strcat(newPath, entry->d_name);
            if(entry->d_type == DT_DIR){
                printf("Deleting folder: %s\n", newPath);
                recDest(newPath);
            }
            else if(entry->d_type == DT_REG){
                printf("Deleting file: %s\n", newPath);
                remove(newPath);
            }
            free(newPath);
        }
    }
    closedir(dir);
    int check = rmdir(path);
    printf("rmDir checl: %d\n", check);
    return 0;
}


void* performCreate(int socket, void* arg){

    printf("Attempting to read project name...\n");
    char* projectName = readNClient(socket, readSizeClient(socket));
    //find node check to see if it already exists in linked list
    //treat LL as list of projects for all purposes
    pthread_mutex_lock(&headLock);
    ((data*) arg)->head = addNode(((data*) arg)->head, projectName);

    node* found = findNode(((data*) arg)->head, projectName);
    printf("Test found name: %s\n", found->proj);
    pthread_mutex_lock(&(found->mutex));
    int creation = createProject(socket, projectName);
    pthread_mutex_unlock(&(found->mutex));

        
    if(creation < 0){
        write(socket, "fail:", 5);
        ((data*) arg)->head = removeNode(((data*) arg)->head, projectName);

    }
    pthread_mutex_unlock(&headLock);
    free(projectName);
}

int readCommand(int socket, char** buffer){
    int status = 0, bytesRead = 0;
    do{
        status = read(socket, *buffer+bytesRead, 1);
        bytesRead += status;
    }while(status > 0 && (*buffer)[bytesRead-1] != ':');
    (*buffer)[bytesRead-1] = '\0';
    return 0;
}

int createProject(int socket, char* name){
    printf("%s\n", name);
    char manifestPath[11+strlen(name)];
    sprintf(manifestPath, "%s/%s", name, ".Manifest");
    /*manifestPath[0] = '\0';
    strcpy(manifestPath, name);
    strcat(manifestPath, "/.Manifest");*/
    int manifest = open(manifestPath, O_WRONLY);    //This first open is a test to see if the project already exists
    if(manifest > 0){
        close(manifest);
        printf("Error: Client attempted to create an already existing project. Nothing changed\n");
        return -1;
    }
    printf("%s\n", manifestPath);
    int folder = mkdir(name, 0777);
    if(folder < 0){
        //I don't know what we sohuld do in this situation. For now just error
        printf("Error: Could not create project folder.. Nothing created\n");
        return -2;
    }
    manifest = open(manifestPath, O_WRONLY | O_CREAT, 00600);
    if(manifest < 0){
        printf("Error: Could not create .Manifest file. Nothing created\n");
        return -2;
    }
    write(manifest, "0\n", 2);
    printf("Succesful server-side project creation. Notifying Client\n");
    write(socket, "succ:", 5);
    close(manifest);
    return 0;
}

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

void printLL(node* head){
    node* ptr = head;
    while(ptr!=NULL){
        printf("%s -> ", ptr->proj);
        ptr= ptr->next;
    }
    printf("NULL\n");
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

void joinAll(threadList* head){
    
    while(head != NULL){
        threadList* temp = head->next;
        pthread_join(head->thread, NULL);
        free(head);
        head = temp;
    }
    
}

void performHistory(int socket, void* arg){
    node* head = ((data*) arg)->head;
    int bytes = readSizeClient(socket);
    char projName[bytes + 1];
    read(socket, projName, bytes);
    projName[bytes] = '\0';
    
    node* found = findNode(head, projName);
    int check = 0;
    if(found == NULL) {
        printf("Could not find project with that name. Cannot find history (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to perform History");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }else{
        pthread_mutex_lock(&(found->mutex));
        int projNameLen = strlen(projName);
        char manPath[projNameLen + 12];
        sprintf(manPath, "%s/.History", projName);
        check = sendFile(socket, manPath);

        //Since we checked that the project exists, and couldn't open .History
        //That means we haven't created a .History. So history must be 0
        if(check != 0) write(socket, "2:na2:0\n", 8);
        printf("Successfully sent history to client\n"); 
        pthread_mutex_unlock(&(found->mutex));
    }
}

void* performCurVer(int socket, void* arg){
    //fully lock this one prob
    int bytes = readSizeClient(socket);
    char projName[bytes + 1];
    read(socket, projName, bytes);
    projName[bytes] = '\0';
    
    node* found = findNode(((data*) arg)->head, projName);
    int check = 0;
    if(found == NULL) {
        printf("Could not find project with that name. Cannot find current version (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to perform current verison");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }else{
        pthread_mutex_lock(&(found->mutex));
        int projNameLen = strlen(projName);
        char manPath[projNameLen + 12];
        sprintf(manPath, "%s/.Manifest", projName);
        check = sendFile(socket, manPath);
        if(check == 0)printf("Successfully sent current version to client\n");
        else printf("Something went wrong with sendManifest (%d)\n", check);
        char* confirm = readNClient(socket, readSizeClient(socket));
        free(confirm);
        pthread_mutex_unlock(&(found->mutex));
    }
}

void performUpdate(int socket, void* arg){
    return;
}

//Writes #:Data for manifest 
//# is the size of the Manifest while Data is the actual content
//int sendFile(int sockfd, char* pathName){

void* performCommit(int socket, void* arg, char* clientIP){
    //WRITE BTTER BY CHECKING IF IT FAILED. MAKE SURE TO ADD FAIL CHECKS ON BOTH SIDES
    //Try to get rid of these god damn nested ifs
    int bytes = readSizeClient(socket);
    char projName[bytes + 1];
    read(socket, projName, bytes);
    projName[bytes] = '\0';
    
    node* found = findNode(((data*) arg)->head, projName);
    int check = 0;
    if(found == NULL) {
        printf("Could not find project with that name. Cannot find current version (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to perform current verison");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }else{
        pthread_mutex_lock(&(found->mutex));
        int projNameLen = strlen(projName);
        char manPath[projNameLen + 12];
        sprintf(manPath, "%s/.Manifest", projName);
        int manfd = open(manPath, O_RDONLY);
        
        char verNum[12];
        int bytesRead = 0, status = 0;
        do{
            status = read(manfd, verNum + bytesRead, 1);
            bytesRead += status;
        }while(status > 0 && *(verNum + bytesRead-status) != '\n');

        close(manfd);
        verNum[bytesRead-1] = '\0';
        
        if(status < 0 || bytesRead ==0 || *verNum == '\n'){
            printf("Failed reading manifest\n");
            sendFail(socket);
            return NULL;
        }

        //Client doesnt need the whole manifest, only the version number
        char* sendVerNum = messageHandler(verNum);
        int sendVerNumLen = strlen(sendVerNum);
        int check = write(socket, sendVerNum, sendVerNumLen);

        if(check == sendVerNumLen){
            printf("Successfully sent manifest to client\n");
            int pathLen = readSizeClient(socket);
            char* commitPath = readNClient(socket, pathLen);
            if(!strcmp("fail", commitPath)){
                printf("Client could not complete commit request. Terminating...\n");
                free(commitPath);
                pthread_mutex_unlock(&(found->mutex));
                return NULL;
            }
            char* commitData = readNClient(socket, readSizeClient(socket));
            
            int clientIPLen = strlen(clientIP);
            //printf("%s\n%d\n", clientIP, clientIPLen);
            char commitIP[pathLen + clientIPLen + 2];
            sprintf(commitIP, "%s-%s", commitPath, clientIP);
            remove(commitIP);
            int commitfd = open(commitIP, O_WRONLY | O_CREAT, 00600);
            writeString(commitfd, commitData);

            free(commitPath);
            free(commitData);
            close(commitfd);
            write(socket, "yerrrrrrrrrr", 12);
        }else{
            printf("Something went wrong with sendFile (%d)\n", check);
        }
        pthread_mutex_unlock(&(found->mutex));
    }
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
    char* hash = (char*) malloc(32); bzero(hash, 32);
    char buf[3];
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(buf, "%02x", c[i]);
        strcat(hash, buf);
    }
    return hash;

}
