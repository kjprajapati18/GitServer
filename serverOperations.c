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


#include "linkedList.h"
#include "avl.h"
#include "serverOperations.h"
#include "sharedFunctions.h"


void* performCreate(int socket, void* arg){

    printf("Attempting to read project name...\n");
    node** head = &(((data*) arg)->head);
    char* projectName = readNClient(socket, readSizeClient(socket));
    //find node check to see if it already exists in linked list
    //treat LL as list of projects for all purposes
    pthread_mutex_lock(&headLock);
    *head = addNode(*head, projectName);

    node* found = findNode(*head, projectName);
    printf("Test found name: %s\n", found->proj);
    pthread_mutex_lock(&(found->mutex));
    int creation = createProject(socket, projectName);
    pthread_mutex_unlock(&(found->mutex));

        
    if(creation < 0){
        write(socket, "fail:", 5);
        *head = removeNode(*head, projectName);

    }

    pthread_mutex_unlock(&headLock);
    free(projectName);
}


void* performDestroy(int socket, void* arg){
    //fully lock this one prob
    node** head = &(((data*) arg)->head);
    char* projName = readNClient(socket, readSizeClient(socket));
    //now projName has the string name of the file to destroy
    
    pthread_mutex_lock(&headLock);
    node* found = findNode(*head, projName);
    if(found == NULL) {
        
        printf("Could not find project with that name to destroy (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to destroy");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }
    else{
        *(found->proj) = '\0';
        
        pthread_mutex_lock(&(found->mutex));
        recDest(projName);
        pthread_mutex_unlock(&(found->mutex));

        *head = removeNode(*head, "");

        char* returnMsg = messageHandler("Successfully destroyed project");
        printf("Notifying client\n");
        write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
        printf("Successfully destroyed %s\n", projName);
    }
    pthread_mutex_unlock(&headLock);
    free(projName);
}


void* performCurVer(int socket, void* arg){
    //fully lock this one prob
    node** head = &(((data*) arg)->head);
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);
    
    node* found = findNode(*head, projName);
    int check = 0;
    if(found == NULL) {
        printf("Could not find project with that name. Cannot find current version (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to perform current verison");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }else{
        pthread_mutex_lock(&(found->mutex));
        char manPath[projNameLen + 12];
        sprintf(manPath, "%s/.Manifest", projName);
        check = sendFile(socket, manPath);
        if(check == 0) printf("Successfully sent current version to client\n");
        else printf("Something went wrong with sendManifest (%d)\n", check);
        char* confirm = readNClient(socket, readSizeClient(socket));
        free(confirm);
        pthread_mutex_unlock(&(found->mutex));
    }
    free(projName);
}


void performHistory(int socket, void* arg){
    node** head = &(((data*) arg)->head);
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);
    
    node* found = findNode(*head, projName);
    int check = 0;
    if(found == NULL) {
        printf("Could not find project with that name. Cannot find history (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to perform History");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }else{
        pthread_mutex_lock(&(found->mutex));
        char manPath[projNameLen + 12];
        sprintf(manPath, "%s/.History", projName);
        check = sendFile(socket, manPath);

        //Since we checked that the project exists, and couldn't open .History
        //That means we haven't created a .History. So history must be 0
        if(check != 0) write(socket, "2:na2:0\n", 8);
        printf("Successfully sent history to client\n"); 
        pthread_mutex_unlock(&(found->mutex));
    }
    free(projName);
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
        write(socket, "fail", 4);
        return;
    }
    pthread_mutex_lock(&(found->mutex));
    char manifestFile[strlen(projName) + 11];
    sprintf(manifestFile, "%s/.Manifest", projName);
    printf("%s\n", manifestFile);
    /*int manfd = open(manifestFile, O_RDONLY);
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
    manifest[size] = '\0';*/
    char* manifest = stringFromFile(manifestFile);
    printf("%s\n", manifest);
    //close(manfd);
    char* manifestmsg = messageHandler(manifest);
    write(socket, manifestmsg, strlen(manifestmsg));
    free(manifestmsg);
    free(manifest);
    //char bigboi[11];
    int numFiles = readSizeClient(socket);
    printf("# of files: %d", numFiles);
    int i;
    for(i = 0; i < numFiles; i++){
        char* filepath = readNClient(socket, readSizeClient(socket));
        /*int fd = open(filepath, O_RDONLY);
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
        file[fileSize] = '\0';*/
        //printf("%d to %d\n", fileSize, bytesRead);
        //close(fd);
        char* file = stringFromFile(filepath);
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
        free(file);
    }
    char succ[5]; succ[4] = '\0';
    read(socket, succ, 4);
    printf("succ msg: %s\n", succ); 
    pthread_mutex_unlock(&(found->mutex));
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
    if(found == NULL){
        free(projName);
        free(confirmation);
        printf("Project does not exist\n");
        write(socket, "fail", 4);
        return;
    }
    pthread_mutex_lock(&(found->mutex));
    char commitPath[strlen(projName) + 9];
    char manpath[strlen(projName) + 11];
    char hispath[strlen(projName) + 10];
    sprintf(hispath, "%s/.History", projName);
    sprintf(manpath, "%s/.Manifest", projName);
    sprintf(commitPath, "%s/.Commit", projName);
    char* commit = stringFromFile(commitPath);
    char* servercommithash = hash(commit);
    char* clientcommit = readNClient(socket, readSizeClient(socket));
    char* clientcommithash = hash(clientcommit);
    if(strcmp(clientcommithash, servercommithash)){
        printf("Commits do not match, terminating\n");
        write(socket, "Fail", 4);
        pthread_mutex_unlock(&(found->mutex));
        free(commit);
        return;
    }
    else{
        printf("Commits match up\n");
        write(socket, "Succ", 4);
    }
    //write the commit to .history
    int histfd = open(hispath, O_CREAT| O_WRONLY|O_APPEND, 00600);
    writeString(histfd, commit);
    writeString(histfd, "\n");
    close(histfd);
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
            free(path);
            close(addFilefd);
            return;
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
    free(commit);
    free(manifest);
    freeAvl(commitHead);
    freeAvl(manHead);
}


void* performCommit(int socket, void* arg, char* clientIP){
    //WRITE BTTER BY CHECKING IF IT FAILED. MAKE SURE TO ADD FAIL CHECKS ON BOTH SIDES
    //Try to get rid of these god damn nested ifs
    //2 blocks are not freed!
    node** head = &(((data*) arg)->head);
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);   //REMEMBER TO FREE THIS!!!
    
    node* found = findNode(*head, projName);
    int check = 0;
    if(found == NULL) {
        printf("Could not find project with that name. Cannot find current version (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to perform current verison");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }else{
        pthread_mutex_lock(&(found->mutex));
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


void* performCheckout(int socket, void* arg){
    //1 memory block lost!!
    node** head = &(((data*) arg)->head);
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);

    
    node* found = findNode(*head, projName);
    int check = 0;
    if(found == NULL) {
        printf("Could not find project with that name. Cannot find current version (%s)\n", projName);
        sendFail(socket);
        free(projName);
        return;    
    }

    write(socket, "4:succ", 6);
    char* confirm = readNClient(socket, readSizeClient(socket));
    if(!strcmp("fail", confirm)){
        free(confirm);
        free(projName);
        printf("Client failed checkout. Client already has a project folder\n");
        return NULL;
    }
    free(confirm);

    pthread_mutex_lock(&(found->mutex));
    
    int compressLength = projNameLen*4 + 75;
    char* compressCommand = (char*) malloc(compressLength *sizeof(char));
    sprintf(compressCommand, "tar -czvf %s.tar.gz %s --exclude=%s/.Commi* --exclude=%s/.History --exclude=%s/.*/", projName, projName, projName, projName, projName);
    system(compressCommand);
    free(compressCommand);

    char tarPath[projNameLen+9];
    sprintf(tarPath, "%s.tar.gz", projName);

    int tarFd = open(tarPath, O_RDONLY);
    int bytesRead = 0, status = 0;
    int tarSize = lseek(tarFd, 0, SEEK_END);
    lseek(tarFd, 0, SEEK_SET);
    char* tarData = (char*) malloc(tarSize +1);
    do{
        status = read(tarFd, tarData + bytesRead, tarSize - bytesRead);
        bytesRead+=status;
    }while(status > 0);
    close(tarFd);
    remove(tarPath);

    int tarPathLen = strlen(tarPath);
    
    char sendBuffer[tarPathLen+tarSize+27];
    sprintf(sendBuffer, "%d:%s%d:", tarPathLen, tarPath, tarSize);
    write(socket, sendBuffer, strlen(sendBuffer));
    write(socket, tarData, tarSize);


    confirm = readNClient(socket, readSizeClient(socket));
    if(!strcmp(confirm, "succ")){
        printf("Client successfully received project: %s\n", projName);
    } else {
        printf("Client failed to received project: %s\n", projName);
    }
    free(confirm);
    free(projName);
    pthread_mutex_unlock(&(found->mutex));
    return;
}


void* performRollback(int socket, void* arg){
    node** head = &(((data*) arg)->head);
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);

    node* found = findNode(*head, projName);
    if(found == NULL){
        printf("Cannot find project with given name");
        free(projName);
        write(socket, "fail", 4);
        return;
    }
    else write(socket, "succ", 4);

    int verNumStringLen = readSizeClient(socket);
    char* verNumString = readNClient(socket, verNumStringLen);
    char projVerPath[projNameLen + verNumStringLen + 3];
    sprintf(projVerPath, "%s/.%d", projName, atoi(verNumString));

    DIR* dir = opendir(projVerPath);
    if(dir != NULL) write(socket, "Succ", 4);
    else{
        write(socket, "fail", 4);
        free(projName);
        free(verNumString);
        return;
    }
    closedir(dir);

    char syscmd[10+projNameLen*2 + verNumStringLen];
    sprintf(syscmd, "mv %s .", projVerPath);
    system(syscmd);
    sprintf(syscmd, "rm -r %s", projName);
    system(syscmd); 
    sprintf(syscmd, "mv %s %s", projVerPath, projName);
    system(syscmd);
    
    write(socket, "succ", 4);
    printf("Succesful rollback\n");
    return;
}


int createProject(int socket, char* name){
    printf("%s\n", name);
    char manifestPath[11+strlen(name)];
    sprintf(manifestPath, "%s/%s", name, ".Manifest");
    
    int manifest = open(manifestPath, O_WRONLY);    //This first open is a test to see if the project already exists
    if(manifest > 0){
        close(manifest);
        printf("Error: Client attempted to create an already existing project. Nothing changed\n");
        return -1;
    }
    printf("%s\n", manifestPath);
    int folder = mkdir(name, 0777);
    if(folder < 0){
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