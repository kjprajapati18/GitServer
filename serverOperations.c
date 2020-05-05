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

    //Read Project name
    node** head = &(((data*) arg)->head);
    char* projectName = readNClient(socket, readSizeClient(socket));

    //Lock the list of projects so that no one else can accidnetly delete/change things
    pthread_mutex_lock(&headLock);
    //Add the node and find it
    *head = addNode(*head, projectName);
    node* found = findNode(*head, projectName);
    
    //lock the project so no one else can edit. Then physically create the project
    pthread_mutex_lock(&(found->mutex));
    int creation = createProject(socket, projectName);
    pthread_mutex_unlock(&(found->mutex));

    //If creation failed, let client know and remove the Node we added
    if(creation < 0){
        write(socket, "fail:", 5);
        *head = removeNode(*head, projectName);
    }

    pthread_mutex_unlock(&headLock);
    free(projectName);
}


void* performDestroy(int socket, void* arg){
    
    node** head = &(((data*) arg)->head);
    char* projName = readNClient(socket, readSizeClient(socket));
    //now projName has the string name of the file to destroy
    
    //Lock the list of projects to prevent concurrent edits from messing up list. Find actual project
    pthread_mutex_lock(&headLock);
    node* found = findNode(*head, projName);
    if(found == NULL) {
        //If we cant find the project let the client know.
        printf("Could not find project with that name to destroy (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to destroy");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }
    else{
        //Otherwise set the first char to '\0' so no one else can find it 
        *(found->proj) = '\0';
        
        //lock the project repository in case someone found it before we changed the name
        pthread_mutex_lock(&(found->mutex));
        recDest(projName); //Destroy
        pthread_mutex_unlock(&(found->mutex));

        //Remove the node now that project is deleted
        *head = removeNode(*head, "");
        
        //Let client know it was successful
        char* returnMsg = messageHandler("Successfully destroyed project");
        write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
        printf("Successfully destroyed %s\n", projName);
    }
    //Unlock and free
    pthread_mutex_unlock(&headLock);
    free(projName);
}


void* performCurVer(int socket, void* arg){
    //Read project name
    node** head = &(((data*) arg)->head);
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);
    
    //Find project
    node* found = findNode(*head, projName);
    int check = 0;
    if(found == NULL) {
        //Let client know it doesn't exist
        printf("Could not find project with that name. Cannot find current version (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to perform current verison");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }else{
        //lock project. Send manifest
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
    //Read project name
    node** head = &(((data*) arg)->head);
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);
    
    node* found = findNode(*head, projName);
    int check = 0;
    if(found == NULL) {
        //If you cant find project, send mesage to client
        printf("Could not find project with that name. Cannot find history (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to perform History");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }else{
        pthread_mutex_lock(&(found->mutex));
        //lock repository and send the .History file
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
    //Read confirmation from the client that the client has a .Update and no .Conflict
    char* confirmation = readNClient(socket, readSizeClient(socket));
    //This will be what the client sends if there is a .Conflict file so the server does nothing
    if(!strcmp(confirmation, "Conflict")){
        printf("There is a Conflict.\n");
        free(confirmation);
        return;
    } 
    //If the client has no update file, it will send this message and the serer does nothing
    else if(!strcmp(confirmation, "Update")){
        printf("No update file.\n");
        free(confirmation);
        return;
    }
    free(confirmation);
    //read the project name from the client
    int projNameLen = readSizeClient(socket);
    char *projName = readNClient(socket, projNameLen);
    //check if project with that name exists in the list of mutexes
    node* found = findNode(((data*) arg)->head, projName);
    //if not found, print failure to server and send failure notice to client.
    if(found == NULL){
        printf("Cannot find project with given name\n");
        free(projName);
        write(socket, "fail", 4);
        return;
    }
    //lock mutex of this project so it cannot be changed while upgrade runs
    pthread_mutex_lock(&(found->mutex));
    //write success of finding project to client
    write(socket, "succ", 4);
    //read if client has non empty update
    char succ[5]; succ[4] = '\0';
    read(socket, succ, 4);
    if(!strcmp(succ, "fail")){
        printf("Client has empty .Update file\n");
        pthread_mutex_unlock(&(found->mutex));
        free(projName);
        return;
    }

    //file path for manifest file
    char manifestFile[projNameLen + 11];
    sprintf(manifestFile, "%s/.Manifest", projName);
    //get manifest file into a string
    char* manifest = stringFromFile(manifestFile);

    //check manifest version number with client's update vernum
    //read client's update file version number 
    char* updateVerNum = readNClient(socket, readSizeClient(socket));
    int k = 0; while(manifest[k] != '\n') k++;
    manifest[k] = '\0';
    //version numbers do not match so send failure message to client, unlock mutex and terminate
    if(strcmp(manifest, updateVerNum)){
        printf("Version numbers do not match between manifest and \n");
        write(socket, "fail", 4);
        pthread_mutex_unlock(&(found->mutex));
        free(manifest);
        free(updateVerNum);
        free(projName);
        return;
    }
    //version numbers do match so send success message to client and continue
    else{
        printf("Version numbers match\n");
        manifest[k] = '\n';
        write(socket, "Succ", 4);
        free(updateVerNum);
    }
    free(manifest);

    //Setup for tarring
    char tarListPath[projNameLen + 12];
    sprintf(tarListPath, "%sTarList.txt", projName);
    int tarListPathLen = strlen(tarListPath);

    int tarList = open(tarListPath, O_CREAT | O_WRONLY, 00600);
    char* tarCommand = (char*) malloc(32+projNameLen+tarListPathLen);
    char tarPath[projNameLen+12];
    sprintf(tarPath, "%sSend.tar.gz", projName);
    sprintf(tarCommand, "tar -czvf %s -T %s", tarPath, tarListPath);
    
    //read the number of files the client needs from the server
    int numFiles = readSizeClient(socket);
    int i;
    //read every file name that the client needs and write them to a file
    for(i = 0; i < numFiles; i++){
        char* filepath = readNClient(socket, readSizeClient(socket));
        writeString(tarList, filepath);
        writeString(tarList, "\n");
        free(filepath);
    }
    close(tarList);
    //tar the list of files to send
    system(tarCommand);
    //send the tar file
    sendTarFile(socket, tarPath);
    remove(tarPath);
    remove(tarListPath);

    //wait for success message from client before terminating thread
    read(socket, succ, 4);
    printf("Upgrade was successful!\n");
    pthread_mutex_unlock(&(found->mutex));
}


void* performPushServer(int socket, void* arg, char* ip){
    //read confirmation from the client that the server can run the push command
    char* confirmation = readNClient(socket, readSizeClient(socket));
    //if confirmation is "Commit" the client has no commit file to send so terminate thread
    if(!strcmp(confirmation, "Commit")){
        printf("There is no commit file. \n");
        free(confirmation);
        return;
    }
    //read the project name and length of the project name
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);
    //check if project name exists on server
    node* found = findNode(((data*) arg)->head, projName);
    //if not found, then terminate command and let client know
    if(found == NULL){
        free(projName);
        free(confirmation);
        printf("Project does not exist\n");
        write(socket, "fail", 4);
        return;
    }
    //lock mutex for this project
    pthread_mutex_lock(&(found->mutex));
    //write success message to client to let it know to continue
    write(socket, "Succ", 4);
    //generate paths for the commit file, manifest file and history file
    char commitPath[projNameLen + strlen(ip)+ 10];
    char manpath[projNameLen + 13];
    char hispath[projNameLen + 10];
    sprintf(hispath, "%s/.History", projName);
    sprintf(manpath, "%s/.Manifest", projName);
    //commit file path should have the client ip address appended on the server side
    sprintf(commitPath, "%s/.Commit-%s", projName, ip);
    //save commit file as a string
    char* commit = stringFromFile(commitPath);
    //read the client's commit version
    char* clientcommit = readNClient(socket, readSizeClient(socket));
    //compare the two commit files
    //if they are different, then terminate and send client a failure message
    if(strcmp(clientcommit, commit)){
        printf("Commits do not match, terminating\n");
        write(socket, "Fail", 4);
        pthread_mutex_unlock(&(found->mutex));
        free(commit);
        return;
    }
    //if they are not different, send success message to client, remove all other commits and continue
    else{
        printf("Commits match up\n");
        char command[13+projNameLen];
        sprintf(command, "rm %s/.Commit*", projName);
        system(command);
        write(socket, "Succ", 4);
    }
    
    //duplicate the directory and compress old version to a tar file
    int index = 0; 
    while(commit[index] != '\n') index++;
    commit[index] = '\0';
    int verNum = atoi(commit);
    commit[index] = '\n';
    char syscmd[64+2*projNameLen]; //Using 1 buffer for all commands to save space
    sprintf(syscmd, "cp -r %s old%s", projName, projName);
    system(syscmd);
    sprintf(syscmd, "mv old%s %s/.v%d", projName, projName, verNum); 
    system(syscmd);
    sprintf(syscmd, "tar -czvf %s/.v%d.tar.gz %s/.v%d --remove-files", projName, verNum, projName, verNum);
    system(syscmd);

    //write the commit to .history
    int histfd = open(hispath, O_CREAT| O_WRONLY|O_APPEND, 00600);
    writeString(histfd, commit);
    writeString(histfd, "\n");
    close(histfd);

    //start deleting files that are specified to delete from the commit. given as a list from the client
    //deletes files that will be added and modified as well since their new versions will be added from a tarfile soon
    int numFiles = readSizeClient(socket);
    int i = 0;
    for(i = 0; i< numFiles; i++){
        char* delPath = readNClient(socket, readSizeClient(socket));
        remove(delPath);
        free(delPath);
    }
    //write success notification to client
    write(socket, "Succ", 4);

    char succ[5]; succ[4] = '\0';
    char* tarFilePath = NULL;
    read(socket, succ, 4); //We are expecting either succ or fail, depending on tar file
    //if success, tar file is created and sent over to server with files that need to be added
    if(!strcmp("succ", succ)){
        //untar file to get updated files
        tarFilePath = readWriteTarFile(socket);
        char untarCommand[strlen(tarFilePath)+12];
        sprintf(untarCommand, "tar -xzvf %s", tarFilePath);
        system(untarCommand);
        remove(tarFilePath);
        free(tarFilePath);
    }
    //manifest creation
    char* manifest = stringFromFile(manpath);
    char* manptr = manifest;
    char* commitptr = commit;
    advanceToken(&commitptr, '\n');
    advanceToken(&manptr, '\n');
    //create AVL trees out of the manifest file and the commit file for fast traversal and updating of information
    avlNode* commitHead = fillAvl(&commitptr);
    avlNode* manHead = fillAvl(&manptr);
    //make the commit file's changes to the manifest
    manHead = commitChanges(commitHead, manHead); 
    //rewrite manifest
    remove(manpath);
    int fd = open(manpath, O_CREAT | O_WRONLY | O_APPEND, 00600);
    char verString[13];
    sprintf(verString, "%d\n", ++verNum);
    writeString(fd, verString);
    //write the updated manifest from the AVL tree
    writeTree(manHead, fd);
    close(fd);

    //free some pointers that we are done with   
    free(commit);
    free(manifest);
    freeAvl(commitHead);
    freeAvl(manHead);

    //write success of manifest to client
    write(socket, "SucD", 4);
    //send manifest to client
    sendFile(socket, manpath);
    
    //wait for successful reception of manifest message
    read(socket, succ, 4);
    //destroy mutex and inform server stdout of the push and terminate
    pthread_mutex_unlock(&(found->mutex));
    printf("Successfully received client's push\n");
    return;
}


void* performCommit(int socket, void* arg, char* clientIP){
    //Read project name
    node** head = &(((data*) arg)->head);
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);
    
    node* found = findNode(*head, projName);
    int check = 0;
    if(found == NULL) {
        //Didn't find project, let client know
        printf("Could not find project with that name. Cannot find current version (%s)\n", projName);
        char* returnMsg = messageHandler("Could not find project with that name to perform current verison");
        int bytecheck = write(socket, returnMsg, strlen(returnMsg));
        free(returnMsg);
    }else{
        //Lock repository
        pthread_mutex_lock(&(found->mutex));
        char manPath[projNameLen + 12];
        sprintf(manPath, "%s/.Manifest", projName);
        int manfd = open(manPath, O_RDONLY);
        
        //Read project version from manifest
        char verNum[12];
        int bytesRead = 0, status = 0;
        do{
            status = read(manfd, verNum + bytesRead, 1);
            bytesRead += status;
        }while(status > 0 && *(verNum + bytesRead-status) != '\n');

        close(manfd);
        verNum[bytesRead-1] = '\0';
        
        //If that failed, let client know
        if(status < 0 || bytesRead ==0 || *verNum == '\n'){
            printf("Failed reading manifest\n");
            sendFail(socket);
            return NULL;
        }

        //Client doesnt need the whole manifest, send only the version number
        char* sendVerNum = messageHandler(verNum);
        int sendVerNumLen = strlen(sendVerNum);
        int check = write(socket, sendVerNum, sendVerNumLen);

        if(check == sendVerNumLen){
            //Manifest was sent successfully
            printf("Successfully sent manifest to client\n");
            int pathLen = readSizeClient(socket);
            char* commitPath = readNClient(socket, pathLen);

            //Check to make sure that client wants to continue with commit
            if(!strcmp("fail", commitPath)){
                printf("Client could not complete commit request. Terminating...\n");
                free(commitPath);
                pthread_mutex_unlock(&(found->mutex));
                return NULL;
            }
            
            //Read Commit data since client wants to continue and sent the commit
            char* commitData = readNClient(socket, readSizeClient(socket));
            //Write the .Commit-<IP> file
            int clientIPLen = strlen(clientIP);
            char commitIP[pathLen + clientIPLen + 2];
            sprintf(commitIP, "%s-%s", commitPath, clientIP);
            remove(commitIP);
            int commitfd = open(commitIP, O_WRONLY | O_CREAT, 00600);
            writeString(commitfd, commitData);

            //Free everything and send a message to client so they know we are done
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
    //Read projName
    node** head = &(((data*) arg)->head);
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);

    //Find project and make sure it exists
    node* found = findNode(*head, projName);
    int check = 0;
    if(found == NULL) {
        printf("Could not find project with that name. Cannot find current version (%s)\n", projName);
        sendFail(socket);
        free(projName);
        return;    
    }
    //Send success cuz we found the project
    write(socket, "4:succ", 6);
    //Check if client wants to continue
    char* confirm = readNClient(socket, readSizeClient(socket));
    if(!strcmp("fail", confirm)){
        free(confirm);
        free(projName);
        printf("Client failed checkout. Client already has a project folder\n");
        return NULL;
    }
    free(confirm);

    //Lock repository cuz we're grabbing the latest version
    pthread_mutex_lock(&(found->mutex));
    
    //Tar command that excludes the server-only files
    int compressLength = projNameLen*4 + 75;
    char* compressCommand = (char*) malloc(compressLength *sizeof(char));
    sprintf(compressCommand, "tar -czvf %s.tar.gz %s --exclude=%s/.Commi* --exclude=%s/.History --exclude=%s/.v*", projName, projName, projName, projName, projName);
    system(compressCommand);
    free(compressCommand);

    //Get tar file path and send it over to client. THen remove
    char tarPath[projNameLen+9];
    sprintf(tarPath, "%s.tar.gz", projName);
    check = sendTarFile(socket, tarPath);
    remove(tarPath);

    //Confirm that the client received it, print out result
    confirm = readNClient(socket, readSizeClient(socket));
    if(!strcmp(confirm, "succ")){
        printf("Client successfully received project: %s\n", projName);
    } else {
        printf("Client failed to received project: %s\n", projName);
    }
    //Unlock and free
    free(confirm);
    free(projName);
    pthread_mutex_unlock(&(found->mutex));
    return;
}


void* performRollback(int socket, void* arg){
    node** head = &(((data*) arg)->head);
    //read project name and length
    int projNameLen = readSizeClient(socket);
    char* projName = readNClient(socket, projNameLen);
    //check if project exists
    node* found = findNode(*head, projName);
    if(found == NULL){
        //if it doenst exist, terminate
        printf("Cannot find project with given name");
        free(projName);
        write(socket, "fail", 4);
        return;
    }
    //send success message to client and lock mutex
    else write(socket, "succ", 4);
    pthread_mutex_lock(&(found->mutex));

    //get version number to rollback to from the client
    int verNumStringLen = readSizeClient(socket);
    char* verNumString = readNClient(socket, verNumStringLen);
    char projVerPath[projNameLen + verNumStringLen + 11];
    sprintf(projVerPath, "%s/.v%s.tar.gz", projName, verNumString);
    //see if version number exists on the server side
    int tarFile = open(projVerPath, O_RDONLY);
    //if it does inform client that we can continue
    if(tarFile > 0) write(socket, "Succ", 4);
    else{
        //if it does exist we tell the client that verison number is not acceptable and we terminate
        write(socket, "fail", 4);
        pthread_mutex_unlock(&(found->mutex));
        free(projName);
        free(verNumString);
        printf("Client sent an invalid previous version. Rollback was not performed\n");
        return;
    }
    close(tarFile);
    //we move that tarfile out of the project path into the server's current working directory
    int projVerPathLen = strlen(projVerPath); 
    char syscmd[16+projNameLen + projVerPathLen];
    sprintf(syscmd, "mv %s .", projVerPath);
    system(syscmd);
    //we remove the current project directory
    sprintf(syscmd, "rm -r %s", projName);
    system(syscmd);
    //we untar the version of the project
    sprintf(syscmd, "tar -xzvf %s", projVerPath+projNameLen+1);
    system(syscmd);
    remove(projVerPath+projNameLen+1);
    //we move the project to a projectName directory so that the project directory exists with the old version inside
    sprintf(syscmd, "mv %s/.v%s .", projName, verNumString);
    system(syscmd);
    sprintf(syscmd, "rm -r %s", projName);
    system(syscmd);
    sprintf(syscmd, "mv .v%s %s", verNumString, projName);
    system(syscmd);
    
    //write a success message to server stdout and client
    write(socket, "succ", 4);
    printf("Successful rollback\n");
    return;
}


int createProject(int socket, char* name){
    
    //Create Manifest Path
    char manifestPath[11+strlen(name)];
    sprintf(manifestPath, "%s/%s", name, ".Manifest");
    
    int manifest = open(manifestPath, O_WRONLY);    //This first open is a test to see if the project already exists
    if(manifest > 0){
        close(manifest);
        printf("Error: Client attempted to create an already existing project. Nothing changed\n");
        return -1;
    }

    //Make folder with project name
    int folder = mkdir(name, 0777);
    if(folder < 0){
        printf("Error: Could not create project folder.. Nothing created\n");
        return -2;
    }
    
    //Create manifest file and write the manifest there
    manifest = open(manifestPath, O_WRONLY | O_CREAT, 00600);
    if(manifest < 0){
        printf("Error: Could not create .Manifest file. Nothing created\n");
        return -2;
    }
    writeString(manifest, "0\n");
    printf("Succesful server-side project creation. Notifying Client\n");

    //Let client know we succeeded
    write(socket, "succ:", 5);
    close(manifest);
    return 0;
}


//recursive destroy directory function to avoid system call
int recDest(char* path){
    //open the directory and start deleting recursively if it can be opened
    DIR* dir = opendir(path);
    int len = strlen(path);
    if(dir){
        //read through the directory entries
        struct dirent* entry;
        while((entry =readdir(dir))){
            char* newPath; 
            int newLen;
            //skip the . and .. directories
            if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            newLen = len + strlen(entry->d_name) + 2;
            newPath = malloc(newLen); bzero(newPath, sizeof(newLen));
            strcpy(newPath, path);
            strcat(newPath, "/");
            strcat(newPath, entry->d_name);
            //if the entry is a directory, recurse on it
            if(entry->d_type == DT_DIR){
                printf("Deleting folder: %s\n", newPath);
                recDest(newPath);
            }
            //if the entry is a file delete it
            else if(entry->d_type == DT_REG){
                printf("Deleting file: %s\n", newPath);
                remove(newPath);
            }
            free(newPath);
        }
    }
    //close the directory and remove the now empty directory
    closedir(dir);
    int check = rmdir(path);
    printf("rmDir checl: %d\n", check);
    return 0;
}
