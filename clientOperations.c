#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <dirent.h>
#include <sys/select.h>
#include <signal.h>

#include "clientOperations.h"
#include "sharedFunctions.h"
#include "avl.h"

int performRollback(int ver, char** argv, int sockfd){
    char* projName = messageHandler(argv[2]);
    write(sockfd, projName, strlen(projName));
    free(projName);
    char succ[5]; succ[4] = '\0';
    read(sockfd, succ, 4);
    if(!strcmp(succ, "Fail")){
        printf("Project not found\n");
        return -1;
    }
    char* verNumString = messageHandler(argv[3]);
    write(sockfd, verNumString, strlen(verNumString));
    free(verNumString);
    read(sockfd, succ, 4);
    if(!strcmp(succ, "Fail")){
        printf("Version number not found\n");
        return -1;
    }
    read(sockfd, succ, 4);
    if(!strcmp(succ, "fail")){
        printf("Could not rollback to previous version.\n");
        return -1;
    }
    else{
        printf("Successfully rolled back to version %d\n", ver);
        return 0;
    }

}

int performAdd(char** argv){
    DIR* d = opendir(argv[2]);
    
    if(!d){
        printf("Project does not exist locally. Cannot execute command.\n");
        return -1;
    }
    closedir(d);
    
    int len1 = strlen(argv[2]);
    char* manPath = (char*) malloc(len1 + 11); bzero(manPath, len1+11);
    sprintf(manPath, "%s/%s", argv[2], ".Manifest");
    int len2 = strlen(argv[3]);
    
    printf("Path: %s\n\n", manPath);
    int len3 = len1+5+len2;
    char* writefile = (char*) malloc(len3); 
    sprintf(writefile, "./%s/%s ", argv[2], argv[3]);

    //Check if we can open/has the file
    writefile[len3-2] = '\0';
    printf("Size: %d, writeFile: %s", len3-1, writefile);
    char* hashcode = hash(writefile);
    if(hashcode == NULL){
        printf("Fatal Error: Cannot open/hash file. Make sure it exists with write permissions\n");
        free(writefile);
        free(manPath);
        return -1;
    }
    writefile[len3-2] = ' ';

    //check if if file already exists in manifest:
    /*int manfd = open(manPath, O_RDONLY);
    int size = (int) lseek(manfd, 0, SEEK_END);
    printf("Size: %d\nFD: %d\n", size, manfd);
    lseek(manfd, 0, SEEK_SET);
    char manifest[size+1];
    int bytesRead=0, status=0;
    do{
        status = read(manfd, manifest + bytesRead, size-bytesRead);
        if(status < 0){
            close(manfd);
            error("Fatal Error: Unable to read .Manifest file\n");
        }
        bytesRead += status;
    }while(status != 0);*/
    char* manifest = stringFromFile(manPath);
    int size = strlen(manifest);
    printf("%s", manifest);
    char* filename = (char*) malloc(len3);
    int i = 0;
    for(i = 0; i <= size; i++){
        if(manifest[i] == '.'){
            strncpy(filename, &manifest[i], len3);
            filename[len3-1] = '\0';
            printf("%s\n", filename);
            printf("%s\n", writefile);
            if(!strcmp(filename, writefile)){
                //two cases: R already and can be added or would be adding duplicate
                //Removed alraedy
                if(manifest[i - 2] == 'R'){
                    printf("Previously removed and to be added now.\n");
                    char newmani[size-1]; bzero(newmani, size-1);
                    strncpy(newmani, manifest, i-2);
                    strcat(newmani, &manifest[i-1]);
                    printf("newmani: %s\n", newmani);
                    remove(manPath);
                    int newfd = open(manPath, O_CREAT | O_WRONLY, 00600);
                    writeString(newfd, newmani);
                    free(filename);
                    free(manPath);
                    free(writefile);
                    close(newfd);
                    printf("added file to manifest after removing it");
                    return 0;
                }
                //found file so cannot add duplicate
                printf("Cannot add filename because it already exists in manifest\n");
                free(filename);
                free(manPath);
                free(writefile);
                return -1;
            }
            else while(manifest[i] != '\n' && manifest[i] != '\0') i++;
        }
    }
    //add it
    printf("%s\n", writefile);
    printf("%d\n", strlen(writefile));
    int manfd = open(manPath, O_WRONLY | O_APPEND);
    //lseek(manfd, 0, SEEK_END);
    if(manfd < 0) error("Could not open manifest");
    //sprintf(writefile, "./%s/%s ", argv[1], argv[2]);
    writeString(manfd, "0A ");
    writeString(manfd, writefile);
    writeString(manfd, hashcode);
    writeString(manfd, "\n");
    free(hashcode);
    free(filename);
    free(manPath);
    free(writefile);
    close(manfd);
    printf("Successfully added file to .Manifest\n");
    return 0;
    
}

int performRemove(char** argv){
    DIR* d = opendir(argv[2]);
    if(!d){
        printf("Project does not exist locally. Cannot remove file from this project manifest");
        closedir(d);
        return -1;
    }
    closedir(d);
    int len1 = strlen(argv[2]);
    char* manPath = (char*) malloc(len1 + 11); bzero(manPath, len1+11);
    sprintf(manPath, "%s/%s", argv[2], ".Manifest");
    int len2 = strlen(argv[3]);
    int len3 = len1+5+len2;
    char* writefile = (char*) malloc(len3); 
    bzero(writefile, len3);
    sprintf(writefile, "./%s/%s ", argv[2], argv[3]);
    /*int manfd = open(manPath, O_RDONLY);
    int size = (int) lseek(manfd, 0, SEEK_END);
    printf("Size: %d\n", size);
    char manifest[size+1];
    lseek(manfd, 0, SEEK_SET);
    int bytesRead=0, status=0;
    do{
        status = read(manfd, manifest + bytesRead, size-bytesRead);
        if(status < 0){
            close(manfd);
            error("Fatal Error: Unable to read .Manifest file\n");
        }
        bytesRead += status;
    }while(status != 0);

    manifest[size] = '\0';*/
    //char newmani[size+3]; bzero(newmani, size+3);
    char* manifest = stringFromFile(manPath);
    int size = strlen(manifest);
    char* filename = (char*) malloc(len3);
    int i;
    for(i = 0; i < size + 1; i++){
        if(manifest[i] == '.'){
            strncpy(filename, &manifest[i], len3);
            filename[len3 - 1] = '\0';
            printf("length: %d. filename: %s\n", strlen(filename), filename);
            printf("lenght: %d, writefile: %s\n", strlen(writefile), writefile);
            //found file to remove
            if(!strcmp(filename, writefile)){
                printf("files ar ethe same name\n");
                //already removed this file
                if(manifest[i -2] == 'R') {
                    printf("Already removed this file from manifest\n"); 
                    free(filename);
                    free(manPath);
                    free(writefile);
                    return -1;
                }
                else if(manifest[i-2] == 'A'){
                    printf("A key before\n");
                    printf("%d\n", size - len1 -len2 - 39);
                    char newmani[size - len1 - len2 - 39]; bzero(newmani, size - len1 - len2 -39);
                    strncpy(newmani, manifest, i-3);
                    printf("newmani after first strncpy: %s\n", newmani);
                    strcat(newmani, &manifest[i+len1+len2+ 37]);
                    printf("newmani after second strcpy: %s\n", newmani);
                    remove(manPath);
                    int newfd = open(manPath, O_CREAT | O_WRONLY, 00600);
                    printf("newfd: %d\n", newfd);
                    writeString(newfd, newmani);
                    free(filename);
                    free(manPath);
                    free(writefile);
                    close(newfd);
                    printf("Removed file from manifest\n");
                    return 0;
                }
                //did not remove file so need to add R
                char newmani[size+3]; bzero(newmani, size+3);
                //did not remove file so need to add R
                strncpy(newmani, manifest, i-1); //-1 is to remove the space
                strcat(newmani, "R ");
                strcat(newmani, &manifest[i]);
                //new mani now has full new manifest string
                remove(manPath);
                int newfd = open(manPath, O_CREAT | O_WRONLY, 00600);
                writeString(newfd, newmani);
                free(filename);
                free(manPath);
                free(writefile);
                close(newfd);
                printf("Successfully removed file from manifest.\n");
                return 0;
            }
            else while(manifest[i] != '\n' && manifest[i] != '\0') i++;
        }
    }
    free(manPath);
    free(filename);
    free(writefile);
    printf("Fatal Error: Could not find filename to remove in .Manifest\n");
    return -1;
}

void sendServerCommand(int socket, char* command, int comLen){

    command[comLen] = ':';
    write(socket, command, comLen+1);
    command[comLen] = '\0';
}

void writeConfigureFile(char* IP, char* port){
    remove(".configure");
    int configureFile = open(".configure", O_CREAT | O_WRONLY, 00600);
    if(configureFile < 0) error("Could not create a .configure file\n");
    
    int writeCheck = 0;
    writeCheck += writeString(configureFile, IP);
    writeCheck += writeString(configureFile, "\n");
    writeCheck += writeString(configureFile, port);
    close(configureFile);
    if(writeCheck != 0) error("Could not write to .configure file\n");
}

char* getConfigInfo(int config, int* port){
    char buffer[256]; //The config file should not be more than a couple of bytes
    int status = 0, bytesRead = 0;
    char* ipRead = NULL;
    *port = 0;

    do{
        status = read(config, buffer + bytesRead, 256-bytesRead);
        if(status < 0){
            close(config);
            error("Fatal Error: Unable to read .configure file\n");
        }
        bytesRead += status;
    } while(status != 0);

    int i;
    for(i = 0; i<bytesRead; i++){
        // '\n' is the delimiter in .configure. Therefore, copy everything before it into the IP address, and everything after will be the port
        if(buffer[i] == '\n'){
            buffer[i] = '\0';

            ipRead = (char*) malloc(sizeof(char) * (i+1));
            *ipRead = '\0';
            strcpy(ipRead, buffer);

            *port = atoi(buffer+i+1);
            break;
        }
    }

    if(*port == 0 || ipRead == NULL){
        close(config);
        error("Fatal Error: .configure file has been corrupted or changed. Please use the configure command to fix file\n");
    }    

    return ipRead;
}

command argCheck(int argc, char* arg){
    command mode = ERROR;

    if(strcmp(arg, "checkout") == 0){
        if(argc == 3) mode = checkout;
        else printf("Fatal Error: checkout requires only 1 additional argument (project name)\n");

    }
    else if(strcmp(arg, "update") == 0){
        if(argc == 3) mode = update;
        else printf("Fatal Error: update requires only 1 additional argument (project name)\n");

    }
    else if(strcmp(arg, "upgrade") == 0) {
        if(argc == 3) mode = upgrade;
        else printf("Fatal Error: upgrade requires 1 argument (project name)\n");

    }
    else if(strcmp(arg, "commit") == 0){
        if(argc == 3) mode = commit;
        else printf("Fatal Error: commit requires 1 argument (project name)\n");
    }
    else if(strcmp(arg, "create") == 0){
        if(argc == 3) mode = create;
        else printf("Fatal Error: create requires only 1 additional argument (project name)\n");

    }
    else if(strcmp(arg, "destroy") == 0){
        if(argc == 3) mode = destroy;
        else printf("Fatal Error: destroy requires only 1 additional argument (project name)\n");

    }
    else if(strcmp(arg, "add") == 0){
        if(argc == 4) mode = add;
        else printf("Fatal Error: Not enough arguments for this command. Proper usage is: ./WTF add <projectName> <filename>");
    
    }
    else if(strcmp(arg, "remove") == 0){
        if(argc == 4) mode = rmv;
        else printf("Fatal Error: Not enough arguments for this command. Proper usage is: ./WTF remove <projectName> <fileName>");

    }
    else if(strcmp(arg, "currentversion") == 0){
        if(argc == 3) mode = currentversion;
        else printf("Fatal Error: currentversion requires only 1 additional argument (project name)\n");

    }
    else if(strcmp(arg, "history") == 0){
        if(argc == 3) mode = history;
        else printf("Fatal Error: history requires only 1 additional argument (project name)\n");

    }
    else if(strcmp(arg, "rollback") == 0){ 
        if(argc ==4) mode = rollback;
        else printf("Fatal Error: rollback requires 2 additional arguments (projectname and version number)\n");
    
    }
    else if(strcmp(arg, "push") == 0){
        if(argc == 3) mode = push;
        else printf("Fatal Error: push requires only 1 additional argument (projectname)\n");
    }

    return mode;
}

//Will read the config file and return output in ipAddr and port
//On error, it will close the config file and quit

int performCreate(int sockfd, char** argv){
    int nameSize = strlen(argv[2]);
    char sendFile[12+nameSize];
    sprintf(sendFile, "%d:%s", nameSize, argv[2]);
    write(sockfd, sendFile, strlen(sendFile)); 
    read(sockfd, sendFile, 5); //Waiting for either fail: or succ:
    sendFile[5] = '\0';       //Make it a string
    //printf("%s\n", buffer);
            
    if(strcmp(sendFile, "succ:") == 0){
        printf("%s was created on server!\n", argv[2]);
        mkdir(argv[2], 00700);
        sprintf(sendFile, "%s/%s", argv[2], ".Manifest"); //sendFile now has path since it has enough space
        remove(sendFile); //There shouldn't be one anyways
        int output = open(sendFile, O_CREAT | O_WRONLY, 00600);
        if(output < 0){
            printf("Fatal Error: Cannot create local .Manifest file. Server still retains copy\n");
        }else{
            write(output, "0\n", 2);
            printf("Project successfully created locally!\n");
        }
    } else {
        printf("Fatal Error: Server was unable to create this project. The project may already exist\n");
    }
    return 0;
}

//What do when you add file locally, but someone else pushes that same filename??? Same hash?
//For now, its just gonna be a .Conflict
int performUpdate(int sockfd, char** argv){
    //Send project name to the server (TURN TO ONE FUNCTION)
    int nameSize = strlen(argv[2]);
    char sendFile[12+nameSize];
    sprintf(sendFile, "%d:%s", nameSize, argv[2]);
    write(sockfd, sendFile, strlen(sendFile)); 

    //Check that the project exists locally (TURN TO ONE FUNCTION)
    char clientManPath[nameSize+12];
    sprintf(clientManPath, "%s/.Manifest", argv[2]);
    int clientManfd = open(clientManPath, O_RDONLY);
    if(clientManfd < 0){
        printf("Fatal Error: The project does not exist locally\n");
        return 1;
    }

    //Read Manifest from the server (TURN THIS AND NEXT INTO 1 FUNCTION);
    //TURN THIS INTO A CHECK FOR FAIL
    free(readNClient(sockfd, readSizeClient(sockfd))); //Throw away the file path
    int serverManSize = readSizeClient(sockfd);
    char* serverMan = readNClient(sockfd, serverManSize);
    
    //Read our own Manifest (<- This should be own function too but also next with above comment)
    int clientManSize = lseek(clientManfd, 0, SEEK_END);
    char* clientMan = (char*) malloc((clientManSize+1)*sizeof(char));
    lseek(clientManfd, 0, SEEK_SET);
    int bytesRead = 0, status = 0;
    do{
        status = read(clientManfd, clientMan+bytesRead, clientManSize-bytesRead);
        bytesRead += status;
    }while(status > 0);
    if(status < 0){
        printf("Fatal Error: Found local .Manifest file but could not read it\n");
        return 2;
    }
    clientMan[bytesRead] = '\0';
    close(clientManfd);                 //We're done with the manifest files.

    //Get the manifest version numbers of each
    char *serverPtr = serverMan, *clientPtr = clientMan;
    
    while(*serverPtr != '\n') serverPtr++;
    *serverPtr = '\0'; serverPtr++;
    while(*clientPtr != '\n') clientPtr++;
    *clientPtr = '\0'; clientPtr++;

    int serverManVerNum = atoi(serverMan), clientManVerNum = atoi(clientMan);
    
    char updatePath[nameSize+10];
    sprintf(updatePath, "%s/.Update", argv[2]);
    remove(updatePath);
    int updatefd = open(updatePath, O_WRONLY | O_CREAT, 00600);
    if(updatefd < 0){
        free(serverMan);
        free(clientMan);
        close(updatefd);
        printf("Error: Could not create a .Update file\n");
        return -1;
    }

    char updateVersion[12];
    sprintf(updateVersion, "%s\n", serverMan);
    writeString(updatefd, updateVersion);

    if(serverManVerNum == clientManVerNum){
        printf("Up To Date\n");
        free(serverMan);
        free(clientMan);
        close(updatefd);
        //Now make the .Update file blank
        remove(updatePath);
        updatefd = open(updatePath, O_WRONLY | O_CREAT, 00600);
        close(updatefd);
        return 0;
    }

    //We need to make an update since manifest versions mismatch
    char conflictPath[nameSize+12];
    sprintf(conflictPath, "%s/.Conflict", argv[2]);
    remove(conflictPath);
    int conflictfd = open(conflictPath, O_WRONLY|O_CREAT, 00600);

    //int serverFileVerNum = 0, clientFileVerNum=0;
    avlNode *serverHead = NULL, *clientHead = NULL;
    char *liveHash; 

    //Tokenize version #, filepath, and hash code. Put it into an avl tree organized by file path
    serverHead = fillAvl(&serverPtr);
    clientHead = fillAvl(&clientPtr);

    // printAVLList(serverHead);
    // printAVLList(clientHead);
    //Will compare the 2 files and write to the proper string to stdout and the proper files
    printf("Pog1\n");
    manDifferencesCDM(updatefd, conflictfd, clientHead, serverHead);
    printf("Pog2\n");
    manDifferencesA(updatefd, conflictfd, clientHead, serverHead);
    

    if(serverHead != NULL)freeAvl(serverHead);
    if(clientHead != NULL)freeAvl(clientHead);
    free(serverMan);
    free(clientMan);
    close(updatefd);
    close(conflictfd);

    //Check if anything was written to the conflict file. Delete .Update and print to let user know
    conflictfd = open(conflictPath, O_RDONLY);
    int conflictSize = lseek(conflictfd, 0, SEEK_END);
    close(conflictfd);
    if(conflictSize == 0){
        remove(conflictPath);
    } else {
        remove(updatePath);
        printf("Conflicts were found and must be resolved before the project can be updated\n");
    }
    return 0;
}

int performCommit(int sockfd, char** argv){

    int projNameLen = strlen(argv[2]);
    char projNameProto[12+projNameLen];
    sprintf(projNameProto, "%d:%s", projNameLen, argv[2]);
    write(sockfd, projNameProto, strlen(projNameProto));
    
    //Check that the project exists locally (TURN TO ONE FUNCTION)
    char clientManPath[projNameLen+12];
    sprintf(clientManPath, "%s/.Manifest", argv[2]);
    int clientManfd = open(clientManPath, O_RDONLY);
    if(clientManfd < 0){
        printf("Fatal Error: The project does not exist locally\n");
        sendFail(sockfd);
        return 1;
    }

    //Check to make sure that there is no conflict or update file
    char clientUpdatePath[projNameLen+10];
    char clientConflictPath[projNameLen+10];
    sprintf(clientUpdatePath, "%s/.Update", argv[2]);
    sprintf(clientConflictPath, "%s/.Conflict", argv[2]);
    int updatefd = open(clientUpdatePath, O_RDONLY);
    int conflictfd = open(clientConflictPath, O_RDONLY);
    int updateSize = 0;
    if(updatefd > 0){
        updateSize = lseek(updatefd, 0, SEEK_END);
    }
    close(updatefd);
    close(conflictfd);

    if(updateSize > 0){
        printf("Fatal Error: A non-empty Update file exists. Cannot perform commit\n");
        sendFail(sockfd);
        return 1;
    }
    if(conflictfd > 0){
        printf("Fatal Error: A Conflict file exists. Cannot perform commit\n");
        sendFail(sockfd);
        return 1;
    }

    //Read Manifest from the server (TURN THIS AND NEXT INTO 1 FUNCTION);
    char* serverManVer = readNClient(sockfd, readSizeClient(sockfd)); 
    //printf("ServerManVer:\n%s\n", serverManVer);
    if(!strcmp("fail", serverManVer)){
        printf("Server could not read manifest\n");
        free(serverManVer);
        close(clientManfd);
        return -1;
    }
    

    // int serverManSize = readSizeClient(sockfd);
    // char* serverMan = readNClient(sockfd, serverManSize);
    
    //Read our own Manifest (<- This should be own function too but also next with above comment)
    int clientManSize = lseek(clientManfd, 0, SEEK_END);
    char* clientMan = (char*) malloc((clientManSize+1)*sizeof(char));
    lseek(clientManfd, 0, SEEK_SET);
    int bytesRead = 0, status = 0;
    do{
        status = read(clientManfd, clientMan+bytesRead, clientManSize-bytesRead);
        bytesRead += status;
    }while(status > 0);

    if(status < 0){
        printf("Fatal Error: Found local .Manifest file but could not read it\n");
        close(clientManfd);
        free(clientMan);
        free(serverManVer);
        sendFail(sockfd);
        return 2;
    }
    clientMan[bytesRead] = '\0';
    close(clientManfd);                 //We're done with the manifest files.

    //Get the manifest version numbers of each
    char *clientPtr = clientMan;
    
    while(*clientPtr != '\n') clientPtr++;
    *clientPtr = '\0'; clientPtr++;

    int serverManVerNum = atoi(serverManVer), clientManVerNum = atoi(clientMan);
    if(serverManVerNum != clientManVerNum){
        printf("Please update your project to the latest version before commit\n");
        free(serverManVer);
        free(clientMan);
        sendFail(sockfd);
        return 0;
    }

    //Commit because client might have changes
    char commitPath[projNameLen+12];
    sprintf(commitPath, "%s/.Commit", argv[2]);
    remove(commitPath);
    int commitfd = open(commitPath, O_WRONLY|O_CREAT, 00600);

    //int serverFileVerNum = 0, clientFileVerNum=0;
    avlNode *clientHead = NULL;

    //Tokenize version #, filepath, and hash code. Put it into an avl tree organized by file path
    //serverHead = fillAvl(&serverPtr);
    
    clientHead = fillAvl(&clientPtr);

    // printAVLList(serverHead);
    // printAVLList(clientHead);
    //Will compare the 2 files and write to the proper string to stdout and the proper files
    writeString(commitfd, serverManVer);
    writeString(commitfd, "\n");
    int commitWriteStatus = commitManDiff(commitfd, clientHead, 0);
    close(commitfd);
    //printf("%d\n", commitWriteStatus);
    if(commitWriteStatus <= 0){
        remove(commitPath);
        sendFail(sockfd);
        if(commitWriteStatus == 0) printf("Cannot Commit. There are no local changes!\n");
    } else {
        sendFile(sockfd, commitPath);
        printf("Sucessfully sent and created .Commit files on client and server\n");
    }
    /*printf("Pog1\n");
    manDifferencesCDM(updatefd, conflictfd, clientHead, serverHead);
    printf("Pog2\n");
    manDifferencesA(updatefd, conflictfd, clientHead, serverHead);*/
    

    //if(serverHead != NULL)freeAvl(serverHead);
    if(clientHead != NULL)freeAvl(clientHead);
    free(serverManVer);
    free(clientMan);
    read(sockfd, commitPath, 12);   //Wait until the server is done before closing socket
    return 0;
}

int performUpgrade(int sockfd, char** argv, char* updatePath){
    char* projName = messageHandler(argv[2]);
    write(sockfd, projName, strlen(projName));
    char succ[5]; succ[4] = '\0';
    read(sockfd, succ, 4);
    if(!strcmp(succ, "fail")){
        printf("Could not find project on server\n");
        free(projName);
        return -1;
    }
    /*int updatefd = open(updatePath, O_RDONLY);
    int size = lseek(updatefd, 0, SEEK_END);*/
    char* update = stringFromFile(updatePath);
    if(update[0]=='\0'){
        printf("Project is up to date\n");
        remove(updatePath);
        return 1;
    }
    //send version number to sever to check if update num matches manifest num
    int k = 0; 
    while(update[k] != '\n') k++;
    update[k] = '\0';
    char* verNum = messageHandler(update);
    write(sockfd, verNum, strlen(verNum));
    free(verNum);
    update[k] = '\n';
    read(sockfd, succ, 4);
    if(!strcmp(succ, "fail")){
        printf("version numbers did not match up. update again");
        free(update);
        return -1;
    }
    //remake manifest if accepted
    char manifestFile[(strlen(argv[2]) + 11)];
    sprintf(manifestFile, "%s/.Manifest", argv[2]);
    remove(manifestFile);
    char* manifest = readNClient(sockfd, readSizeClient(sockfd));
    printf("%s\n", manifest);
    int manfd = open(manifestFile, O_WRONLY | O_CREAT, 00600);
    writeString(manfd, manifest);
    close(manfd);
    free(manifest);
    /*lseek(updatefd, 0, SEEK_SET);
    char update[size+1];
    int bytesRead=0, status=0;
    do{
        status = read(updatefd, update + bytesRead, size-bytesRead);
        if(status < 0){
            close(updatefd);
            error("Fatal Error: Unable to read .Update file\n");
        }
        bytesRead += status;
    }while(status != 0);
    update[size] = '\0';
    close(updatefd);*/
    int i =0, j = 0;
    int numFiles = 0;
    char* addFile = (char*) malloc(1); addFile[0] = '\0';
    int track = 0;
    for(i = 0; i < strlen(update); i++){
        switch(update[i]){
            case 'A':
            case 'M':{
                i = i+2;
                int end = i;
                while(update[end] != ' ') end++;
                char filename[end-i+1]; bzero(filename, end-i+1);
                strncpy(filename, &update[i], end-i);
                filename[end-i] = '\0';
                remove(filename);
                numFiles++; 
                char* fileAndSize = messageHandler(filename);
                track+= strlen(fileAndSize);
                char* temp = (char*) malloc(track + 2);
                sprintf(temp, "%s%s", addFile, fileAndSize);
                printf("%s\n", temp);
                free(addFile);
                free(fileAndSize);
                addFile = temp;
                break;}
            default: break;
        }
        while(update[i] != '\n') i++;
    }
    //shuld have a string of all files to download and replace on client side separated by colons here. 
    char* temp = malloc(track+2+11);
    sprintf(temp, "%d:%s", numFiles, addFile);
    free(addFile);
    addFile = temp;
    write(sockfd, addFile, strlen(addFile));
    free(addFile);
    i = 0;
    for(i = 0; i< numFiles; i++){
        char* filePath = readNClient(sockfd, readSizeClient(sockfd));
        int fd = fileCreator(filePath);
        char* fileCont = readNClient(sockfd, readSizeClient(sockfd));
        printf("Received from socket %d:\n%s\n%s\n", sockfd, filePath, fileCont);
        writeString(fd, fileCont);
        close(fd);
        free(filePath);
        free(fileCont);
    }
    write(sockfd, "Succ", 4);
    printf("Wrote\n");
    remove(updatePath);
    return 0;
}

//make sure to remove file before creating it;
int fileCreator(char* path){
    int fd = open(path, O_CREAT | O_WRONLY, 00600);
    if(fd > 0) return fd;

    int i = strlen(path);
    while(fd < 0 && i >= 0){
        while(path[i] != '/' && i >=0){
            i--;
        }
        path[i] = '\0';
        mkdir(path, 0777);
        path[i] = '/';
        i--;
        fd = open(path, O_CREAT | O_WRONLY, 00600);
    }
    return fd;
}

int performPush(int sockfd, char**argv, char* commitPath){
    //send the .commit
    
    //close(commitfd);
    printf("commitpath: %s\n", commitPath);
    char* commit = stringFromFile(commitPath);
    char* commitmsg = messageHandler(commit);
    printf("commitmsg: %s\n", commitmsg);
    write(sockfd, commitmsg, strlen(commitmsg));
    free(commitmsg);

    //check success of commit compare
    char succ[5]; succ[4] = '\0';
    read(sockfd, succ, 4);
    if(!strcmp(succ, "Fail")){
        printf("commits did not match up\n");
        return -1;
    }

    int i = 0, numAddFiles = 0, numDelFiles = 0, projNameLen = strlen(argv[2]);
    
    //Setup for tarring
    int tarList = open("tarList.txt", O_CREAT | O_WRONLY, 00600);
    char* tarCommand = (char*) malloc(38+projNameLen);
    char tarPath[projNameLen+12];
    sprintf(tarPath, "%sPush.tar.gz", argv[2]);
    sprintf(tarCommand, "tar -czvf %s -T tarList.txt", tarPath);

    //Setup for list of to-be-deleted files
    char* delFile = (char*) malloc(1); delFile[0] = '\0';
    int addTrack = strlen(tarCommand), delTrack = 0;
    for(i = 0; i < strlen(commit); i++){
        while(commit[i] >= '0' && commit[i] <= '9') i++;
        
        switch(commit[i]){
            case 'D':{
                i = i+2;
                int end = i;
                while(commit[end] != ' ') end++;
                char filename[end-i+1]; bzero(filename, end-i+1);
                strncpy(filename, &commit[i], end-i);
                filename[end-i] = '\0';
                numDelFiles++;
                char* fileAndSize = messageHandler(filename);
                delTrack+= strlen(fileAndSize);
                char* temp = (char*) malloc(delTrack+1);
                sprintf(temp, "%s%s", delFile, fileAndSize);
                free(delFile);
                free(fileAndSize);
                delFile = temp;
                break;}
            case 'A':
            case 'M':{
                i = i+2;
                int end = i;
                while(commit[end] != ' ') end++;
                char filename[end-i+1]; bzero(filename, end-i+1);
                strncpy(filename, &commit[i], end-i);
                filename[end-i] = '\0';
                numDelFiles++;
                char* fileAndSize = messageHandler(filename);
                delTrack+= strlen(fileAndSize);
                char* temp = (char*) malloc(delTrack+1);
                sprintf(temp, "%s%s", delFile, fileAndSize);
                free(delFile);
                free(fileAndSize);
                delFile = temp;

                numAddFiles++;
                writeString(tarList, filename);
                writeString(tarList, "\n");
                /*
                addTrack+= strlen(filename)+1;
                temp = (char*) malloc(addTrack+1);
                sprintf(temp, "%s %s", tarCommand, filename);
                free(tarCommand);
                tarCommand = temp;
                /*
                char* file = stringFromFile(filename);
                char* fileContandSize = messageHandler(file);
                addTrack+= strlen(fileContandSize);
                temp = (char*) malloc(addTrack +1);
                sprintf(temp, "%s%s", addFile, fileContandSize);
                free(addFile);
                free(fileContandSize);
                addFile = temp;*/
                break;
            }
            default: break;
        }

        while(commit[i] != '\n') i++;
    }

    //delfile is a list of all files to delete
    char* temp = malloc(delTrack+2+11);
    sprintf(temp, "%d:%s", numDelFiles, delFile);
    free(delFile);
    delFile = temp;
    char number[12]; sprintf(number, "%d", numDelFiles);
    write(sockfd, delFile, delTrack+1+strlen(number));
    free(delFile);

    read(sockfd, succ, 4);
    printf("Del Succ?: %s\n", succ);

    //Tar the add and modify files and send it over
    //We already initialized the command, and we've been adding to the end of it as we read
    close(tarList);

    if(numAddFiles > 0){
        system(tarCommand);
        write(sockfd, "succ", 4);
        sendTarFile(sockfd, tarPath);
        remove(tarPath);
    } else {
        write(sockfd, "fail", 4);
    }
    remove("tarList.txt");


    /*
    //addfile is a list of all files and their contents
    char numTemp[12];
    sprintf(numTemp, "%d", numAddFiles);
    int numLen = strlen(numTemp);
    temp = malloc(addTrack + numLen+2);
    sprintf(temp, "%d:%s", numAddFiles, addFile);
    free(addFile);
    addFile = temp;
    write(sockfd, addFile, addTrack+numLen+1);
    free(addFile);
    read(sockfd, succ, 4);
    printf("Add Succ?: %s\n", succ);*/

    read(sockfd, succ, 4);
    perror("Test");
    printf("Final manifest success?: %s\n", succ);
    remove(commitPath);


    char* manPath = readNClient(sockfd, readSizeClient(sockfd));
    remove(manPath);
    char* manifest = readNClient(sockfd, readSizeClient(sockfd));
    int manfd = open(manPath, O_CREAT|O_WRONLY, 00600);
    writeString(manfd, manifest);
    write(sockfd, "succ", 4);
    
    free(tarCommand);
    free(manPath);
    free(manifest);
    printf("Successfully pushed to server!\n");
    return 0;
}

void printCurVer(char* manifest){
    int skip = 1;
    char *ptr = manifest, *startWord = manifest;
    while(*ptr != '\n') ptr++;
    //ptr is now either at \n. Which is the end of manifest version
    ptr++;
    //We are at the spot after the newline
    while(*ptr != '\0'){
        while(*ptr != ' ' && *ptr != '\0') ptr++;
        if(skip){
            skip = 0;
            ptr++;
            continue;
        }
        skip = 1;
        *ptr = '\0';
        printf("%s", startWord);
        ptr += 34; //skip over the hash code(32). Go PAST the newline (either # or \0)
        startWord = ptr-1; //start of word is AT the newline;
    }
    printf("%s", startWord);
}

int manDifferencesCDM(int hostupdate, int hostconflict, avlNode* hostHead, avlNode* senderHead){
    
    if(hostHead == NULL) return 0;
    printf("File: %s\n", hostHead->path);
    avlNode* ptr = NULL;
    int nullCheck = findAVLNode(&ptr, senderHead, hostHead->path);
    char insert = '\0';
    char* liveHash = hash(hostHead->path);
    char lastVerChar = (hostHead->ver)[strlen(hostHead->ver)-1];
    if(liveHash == NULL) printf("NULL HASH  ");
    printf("%d\n", nullCheck);
    if(liveHash == NULL || strcmp(liveHash, hostHead->code) || lastVerChar == 'A' || lastVerChar == 'R'){
        //The file was locally modified iff any of these conditions are true. This is a conflict.
        insert = 'C';
    }else if(nullCheck == -1){
        //Did not find host's entry in the server. Since its not a conflict, it was deleted on server
        insert = 'D';
    } else if (nullCheck == -2){
        printf("A null terminator was somehow passed in\n");
    } else {
        //We found the entry in both manifests. Ignore if they are the same entry
        if(strcmp(hostHead->ver, ptr->ver) || strcmp(hostHead->code, ptr->code)){
            //Files do not match because either the version or hash are different.
            //Therefore, it was modified on servers.
            insert = 'M';
        }
    }
    
    char write[strlen(hostHead->path) + 37]; //The pathname plus hash plus "(char)(spacex2)\0\n"
    printf("Here\n");
    switch(insert){
        case 'D':
        case 'M': 
            if(ptr != NULL)sprintf(write, "%c %s %s\n", insert, hostHead->path, ptr->code);
            else sprintf(write, "%c %s %s\n", insert, hostHead->path, liveHash);
            printf("OtherHere\n");
            writeString(hostupdate, write);
            printf("%c%s\n", insert, hostHead->path);
            break;
        case 'C':
            if(liveHash != NULL) sprintf(write, "C %s %s\n", hostHead->path, liveHash);
            else sprintf(write, "C %s 00000000000000000000000000000000\n", hostHead->path);
            writeString(hostconflict, write);
            printf("C%s\n", hostHead->path);
            break;
        default:
            break;
    }
    
    if(liveHash != NULL) free(liveHash);
    int left = manDifferencesCDM(hostupdate, hostconflict, hostHead->left, senderHead);
    int right = manDifferencesCDM(hostupdate, hostconflict, hostHead->right, senderHead);
    return left + right;
}


int manDifferencesA(int hostupdate, int hostconflict, avlNode* hostHead, avlNode* senderHead){

    if(senderHead == NULL) return 0;

    printf("File: %s\n", senderHead->path);
    avlNode* ptr = NULL;
    int nullCheck = findAVLNode(&ptr, hostHead, senderHead->path);
    printf("Pog\n");
    if(nullCheck == -1){
        //Did not find server's entry in the host. They need to add this file
        //Cannot be a conflict since that is taken care of in manDifferencesCDM
        char write[strlen(senderHead->path) + 37]; //The pathname + hash + "A(2spaces)\0\n"
        sprintf(write, "A %s %s\n", senderHead->path, senderHead->code);
        writeString(hostupdate, write);
        printf("A%s\n", senderHead->path);
    } else if (nullCheck == -2){
        printf("A null terminator was somehow passed in\n");
    }

    int left = manDifferencesA(hostupdate, hostconflict, hostHead, senderHead->left);
    int right = manDifferencesA(hostupdate, hostconflict, hostHead, senderHead->right);
    return left + right;
}


int commitManDiff(int commitfd, avlNode* clientHead, int status){
    
    if(status < 0) return status;

    if(clientHead == NULL) return status;
    int verNumStrLen = strlen(clientHead->ver);
    char last = (clientHead->ver)[verNumStrLen-1];
    char* liveHash = hash(clientHead->path);

    if(liveHash == NULL && last != 'R'){
        printf("Error: Could not open file %s. Please use the remove command to untrack this file\n", clientHead->path);
        free(liveHash);
        return -1;
    }

    //Space for the written string, numbers represent each part of the string
    //"<verNum><A/M/D> <path> <hash><newLine>"
    //Incremented version number might need 1 extra byte. i.e. 9 -> 10
    char write[(verNumStrLen+1) + 1 + 1+ strlen(clientHead->path) + 1 + 32 + 2];
    int boolWrite = 0, inc = 0;
    if(last == 'A'){
        boolWrite = 1;
    }else if(last == 'R'){
        last = 'D';
        boolWrite = 1;
        if(liveHash != NULL) free(liveHash);
        liveHash = (char*) malloc(33*sizeof(char));
        strcpy(liveHash, "00000000000000000000000000000000");
    }else if(strcmp(liveHash, clientHead->code)){
        last = 'M';
        boolWrite = 1; inc = 1;
    }

    if(boolWrite){
        sprintf(write, "%d%c %s %s\n", clientHead->verNum + inc, last, clientHead->path, liveHash);
        printf("%c %s\n", last, clientHead->path);
        writeString(commitfd, write);
        status++;
    }
    free(liveHash);
    status = commitManDiff(commitfd, clientHead->left, status);
    status = commitManDiff(commitfd, clientHead->right, status);
    return status;
}


int performCheckout(int sockfd, char** argv){
    //Write project name to server
    char* projName = messageHandler(argv[2]);
    write(sockfd, projName, strlen(projName));


    //Read Manifest from the server (TURN THIS AND NEXT INTO 1 FUNCTION);
    char* confirm = readNClient(sockfd, readSizeClient(sockfd)); 
    //printf("ServerManVer:\n%s\n", serverManVer);
    if(!strcmp("fail", confirm)){
        printf("The project does not exist on the server side\n");
        free(confirm);
        return -1;
    }
    free(confirm);

    DIR* project = opendir(argv[2]);
    if(project != NULL){
        closedir(project);
        printf("The project folder already exists on the client. Please delete it before checkout \n");
        sendFail(sockfd);
        return -1;
    }
    write(sockfd, "4:succ", 6);
    
    char* tarFilePath = readWriteTarFile(sockfd);
    if(tarFilePath == NULL){
        free(projName);
        sendFail(sockfd);
        return -1;
    }
    int tarFilePathLen = strlen(tarFilePath);
    // int tarFilePathLen = readSizeClient(sockfd);
    // char* tarFilePath = readNClient(sockfd, tarFilePathLen);
    // int tarFileDataLen = readSizeClient(sockfd);
    // char* tarFile = (char*) malloc(sizeof(char) * tarFileDataLen); 
    // int checkTar = read(sockfd, tarFile, tarFileDataLen);
    // if(checkTar < 0){
    //     printf("Could not read files from server\n");
    //     free(tarFilePath); free(tarFile);
    //     sendFail(sockfd);
    //     return -1;
    // }

    // int tarFd = open(tarFilePath, O_WRONLY | O_CREAT, 00700);
    // if(tarFd < 0){
    //     printf("Could not create files\n");
    //     free(tarFilePath); free(tarFile);
    //     sendFail(sockfd);
    //     return -1;
    // }
    // writeNString(tarFd, tarFile, tarFileDataLen);
    // close(tarFd);
    
    char tarCommand[12+tarFilePathLen];
    sprintf(tarCommand, "tar -xzvf %s", tarFilePath);
    system(tarCommand);
    remove(tarFilePath);

    write(sockfd, "4:succ", 6);
    free(projName);
    free(tarFilePath);
    return 0;
}