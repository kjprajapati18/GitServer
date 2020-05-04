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
    //open the directory to see if it exists locally
    DIR* d = opendir(argv[2]);
    //if it doesn't exist, return and print to client the error.
    if(!d){
        printf("Project does not exist locally. Cannot execute command.\n");
        return -1;
    }
    closedir(d);
    
    int len1 = strlen(argv[2]);
    char* manPath = (char*) malloc(len1 + 11); bzero(manPath, len1+11);
    sprintf(manPath, "%s/%s", argv[2], ".Manifest");
    int len2 = strlen(argv[3]);
    int test = open(manPath, O_RDONLY);
    if(test < 0){
        printf("Manifest does not exist in project. Terminating\n");
        return -1;
    }
    int len3 = len1+5+len2;
    char* writefile = (char*) malloc(len3); 
    sprintf(writefile, "./%s/%s ", argv[2], argv[3]);

    //Check if we can open the file that we want to add and if we can hash it
    writefile[len3-2] = '\0';
    char* hashcode = hash(writefile);
    if(hashcode == NULL){
        printf("Fatal Error: Cannot open/hash file. Make sure it exists with write permissions\n");
        free(writefile);
        free(manPath);
        return -1;
    }
    writefile[len3-2] = ' ';
    //save the manifest file as a string
    char* manifest = stringFromFile(manPath);
    int size = strlen(manifest);
    char* filename = (char*) malloc(len3);
    int i = 0;
    //loop through the manifest string
    for(i = 0; i <= size; i++){
        if(manifest[i] == '.'){
            //if we run into a filename , compare it to the filename we were told to add
            strncpy(filename, &manifest[i], len3);
            filename[len3-1] = '\0';
            if(!strcmp(filename, writefile)){
                //two cases: R already and can be added or would be adding duplicate
                //Removed already so add it without the local change
                if(manifest[i - 2] == 'R'){
                    printf("Previously removed and to be added now.\n");
                    char newmani[size-1]; bzero(newmani, size-1);
                    strncpy(newmani, manifest, i-2);
                    strcat(newmani, &manifest[i-1]);
                    remove(manPath);
                    int newfd = open(manPath, O_CREAT | O_WRONLY, 00600);
                    writeString(newfd, newmani);
                    free(filename);
                    free(manPath);
                    free(writefile);
                    close(newfd);
                    printf("Added file to manifest after removing it prior");
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
    //add file to the manifest
    int manfd = open(manPath, O_WRONLY | O_APPEND);
    if(manfd < 0) error("Could not open manifest");
    //append an A after version number to indicate addition locally
    writeString(manfd, "0A ");
    writeString(manfd, writefile);
    writeString(manfd, hashcode);
    writeString(manfd, "\n");
    //close free and print success message
    free(hashcode);
    free(filename);
    free(manPath);
    free(writefile);
    close(manfd);
    printf("Successfully added file to .Manifest\n");
    return 0;
    
}

int performRemove(char** argv){
    //open directory to see if local copy exists.
    DIR* d = opendir(argv[2]);
    if(!d){
        //without a local copy remove fails and notifies client
        printf("Project does not exist locally. Cannot remove file from this project manifest");
        closedir(d);
        return -1;
    }
    closedir(d);
    //create paths for manifest and file to remove
    int len1 = strlen(argv[2]);
    char* manPath = (char*) malloc(len1 + 11); bzero(manPath, len1+11);
    sprintf(manPath, "%s/%s", argv[2], ".Manifest");
    int test = open(manPath, O_RDONLY);
    if(test < 0){
        printf("Manifest does not exist in project. Terminating\n");
        return -1;
    }
    int len2 = strlen(argv[3]);
    int len3 = len1+5+len2;
    char* writefile = (char*) malloc(len3); 
    bzero(writefile, len3);
    sprintf(writefile, "./%s/%s ", argv[2], argv[3]);
    //get manifest as a string to analyze
    char* manifest = stringFromFile(manPath);
    int size = strlen(manifest);
    char* filename = (char*) malloc(len3);
    int i;
    //loop through manifest string to find file to remove
    for(i = 0; i < size + 1; i++){
        if(manifest[i] == '.'){
            strncpy(filename, &manifest[i], len3);
            filename[len3 - 1] = '\0';
            //found file to remove
            if(!strcmp(filename, writefile)){
                //already removed this file
                if(manifest[i -2] == 'R') {
                    printf("Already removed this file from manifest\n"); 
                    free(filename);
                    free(manPath);
                    free(writefile);
                    return -1;
                }
                //file was only added locally so can be removed completely from manifest
                else if(manifest[i-2] == 'A'){
                    char newmani[size - len1 - len2 - 39]; bzero(newmani, size - len1 - len2 -39);
                    strncpy(newmani, manifest, i-3);
                    strcat(newmani, &manifest[i+len1+len2+ 37]);
                    remove(manPath);
                    int newfd = open(manPath, O_CREAT | O_WRONLY, 00600);
                    writeString(newfd, newmani);
                    free(filename);
                    free(manPath);
                    free(writefile);
                    close(newfd);
                    printf("Removed locally added file from manifest\n");
                    return 0;
                }
                //did not remove file yet so need to add R
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
    //if not found, let user know they cannot remove a file that does not exist in the project
    free(manPath);
    free(filename);
    free(writefile);
    printf("Fatal Error: Could not find filename to remove in .Manifest\n");
    return -1;
}

void sendServerCommand(int socket, char* command, int comLen){
    //Send command using our protocol
    command[comLen] = ':';
    write(socket, command, comLen+1);
    command[comLen] = '\0';
}

void writeConfigureFile(char* IP, char* port){
    //Remove current .configure & Try to create a new one
    remove(".configure");
    int configureFile = open(".configure", O_CREAT | O_WRONLY, 00600);
    if(configureFile < 0) error("Could not create a .configure file\n");
    
    //Write the IP and port given by user. Then close
    int writeCheck = 0;
    writeCheck += writeString(configureFile, IP);
    writeCheck += writeString(configureFile, "\n");
    writeCheck += writeString(configureFile, port);
    close(configureFile);

    //Clean up on error
    if(writeCheck != 0){
        remove(".configure");
        error("Could not write to .configure file\n");
    }
}

char* getConfigInfo(int config, int* port){
    char buffer[256]; //The config file should not be more than 256 bytes since linux does not accept commands that long
    int status = 0, bytesRead = 0;
    char* ipRead = NULL;
    *port = 0;

    //Read the whole file, quit on error
    do{
        status = read(config, buffer + bytesRead, 256-bytesRead);
        if(status < 0){
            close(config);
            error("Fatal Error: Unable to read .configure file\n");
        }
        bytesRead += status;
    } while(status != 0);
    close(config);

    //Find the newline. Split the buffer there. Set IP to first part, port is second part
    //atoi the port
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

    //Check to see if the output makes sense
    if(*port == 0 || ipRead == NULL){
        error("Fatal Error: .configure file has been corrupted or changed. Please use the configure command to fix file\n");
    }    

    return ipRead;
}

command argCheck(int argc, char* arg){
    //Set default mode to be error
    command mode = ERROR;

    //If we get a valid command with the valid number of args, then change mode
    //Print error and correct usage on fails
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

    //Will be ERROR unless a proper command is given (name & args)
    return mode;
}

int performCreate(int sockfd, char** argv){
    //Send project Name
    int nameSize = strlen(argv[2]);
    char sendFile[12+nameSize];
    sprintf(sendFile, "%d:%s", nameSize, argv[2]);
    write(sockfd, sendFile, strlen(sendFile)); 

    read(sockfd, sendFile, 5); //Waiting for either fail: or succ: 
    sendFile[5] = '\0';       //Make it a string by replacing : with \0
    
    //If we were successful, create the project locally.
    //Either case, let user know what's going on
    if(strcmp(sendFile, "succ:") == 0){
        printf("%s was created on server!\n", argv[2]);
        mkdir(argv[2], 00700);
        sprintf(sendFile, "%s/%s", argv[2], ".Manifest");   //sendFile now has path since it has enough space
        remove(sendFile);                                   //There shouldn't be one anyways
        int output = open(sendFile, O_CREAT | O_WRONLY, 00600);
        if(output < 0){
            printf("Error: Cannot create local .Manifest file. Server still retains copy\n");
        }else{
            write(output, "0\n", 2);
            printf("Project successfully created locally!\n");
        }
    } else {
        printf("Fatal Error: Server was unable to create this project. The project may already exist\n");
    }
    return 0;
}


int performUpdate(int sockfd, char** argv){
    //Send project name to the server
    int nameSize = strlen(argv[2]);
    char sendFile[12+nameSize];
    sprintf(sendFile, "%d:%s", nameSize, argv[2]);
    write(sockfd, sendFile, strlen(sendFile)); 

    //Check that the project exists locally
    char clientManPath[nameSize+12];
    sprintf(clientManPath, "%s/.Manifest", argv[2]);
    int clientManfd = open(clientManPath, O_RDONLY);
    if(clientManfd < 0){
        printf("Fatal Error: The project does not exist locally\n");
        return 1;
    }

    //Check for fail. On success, this will just be a filepath to throw away.
    char* confirm = readNClient(sockfd, readSizeClient(sockfd));
    if(!strncmp(confirm, "Could", 5)){
        printf("Fatal Error: Project does not exist on server\n");
        free(confirm);
        close(clientManfd);
        return 1;
    }
    free(confirm);
    
    //Next message will be server manifest data. Read Manifest from the server.
    int serverManSize = readSizeClient(sockfd);
    char* serverMan = readNClient(sockfd, serverManSize);
    
    //Read our own Manifest
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
    
    close(clientManfd); //We're done with the manifest files.

    //Get the manifest version numbers of each by tokenizing them
    char *serverPtr = serverMan, *clientPtr = clientMan;
    
    while(*serverPtr != '\n') serverPtr++;
    *serverPtr = '\0'; serverPtr++;
    while(*clientPtr != '\n') clientPtr++;
    *clientPtr = '\0'; clientPtr++;

    //Get numbers now that we've tokenized
    int serverManVerNum = atoi(serverMan), clientManVerNum = atoi(clientMan);
    
    //Open up a .Update file
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

    //Write server version number to .update
    char updateVersion[12];
    sprintf(updateVersion, "%s\n", serverMan);
    writeString(updatefd, updateVersion);

    //Full success case. No updates required. Clean up everything
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
    //Create a conflict in case we need to add to it
    char conflictPath[nameSize+12];
    sprintf(conflictPath, "%s/.Conflict", argv[2]);
    remove(conflictPath);
    int conflictfd = open(conflictPath, O_WRONLY|O_CREAT, 00600);

    //initialize AVL heads since we will store manifests there for comparisons
    avlNode *serverHead = NULL, *clientHead = NULL;
    char *liveHash;

    //Tokenize version #, filepath, and hash code. Put it into an avl tree organized by file path
    serverHead = fillAvl(&serverPtr);
    clientHead = fillAvl(&clientPtr);

    //Will compare the 2 trees and write to the proper string to stdout and the proper files
    manDifferencesCDM(updatefd, conflictfd, clientHead, serverHead);
    manDifferencesA(updatefd, conflictfd, clientHead, serverHead);
    //All changes have been written to stdout, .update, and possibly .conflict
    
    //Free everything and close all files since we only have to check file lengths
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
        printf("Update was successful!\n");
    } else {
        remove(updatePath);
        printf("Conflicts were found and must be resolved before the project can be upgraded\n");
    }

    return 0;
}

int performCommit(int sockfd, char** argv){
    //Send Project name to server
    int projNameLen = strlen(argv[2]);
    char projNameProto[12+projNameLen];
    sprintf(projNameProto, "%d:%s", projNameLen, argv[2]);
    write(sockfd, projNameProto, strlen(projNameProto));
    
    //Check that the project exists locally
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
        close(updatefd);
    }

    //Update can exist but it should be blank
    if(updateSize > 0){
        printf("Fatal Error: A non-empty Update file exists. Cannot perform commit\n");
        sendFail(sockfd);
        return 1;
    }
    //No conflict file can exist
    if(conflictfd > 0){
        close(conflictfd);
        printf("Fatal Error: A Conflict file exists. Cannot perform commit\n");
        sendFail(sockfd);
        return 1;
    }

    //Server will write 1 of 3 things:
    //      fail-> Cannot read version number from .Manifest    (fail)
    //      Could...-> Cannot find project on server            (fail)
    //      ### -> Server Manifest's version number             (success)
    char* serverManVer = readNClient(sockfd, readSizeClient(sockfd)); 
    if(!strcmp("fail", serverManVer)){
        printf("Fatal Error: Server could not read manifest\n");
        free(serverManVer);
        close(clientManfd);
        return -1;
    }
    if(!strncmp(serverManVer, "Could", 5)){
        printf("Fatal error: Project does not exist on server\n");
        free(serverManVer);
        close(clientManfd);
        return -1;
    }
    
    //Read our own Manifest
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
    close(clientManfd);
    //We're done with the manifest files.

    //Get the manifest version number from ours
    char *clientPtr = clientMan;
    
    while(*clientPtr != '\n') clientPtr++;
    *clientPtr = '\0'; clientPtr++;

    int serverManVerNum = atoi(serverManVer), clientManVerNum = atoi(clientMan);
    //Cannot perform commit if versions don't match
    if(serverManVerNum != clientManVerNum){
        printf("Please update your project to the latest version before commit\n");
        free(serverManVer);
        free(clientMan);
        sendFail(sockfd);
        return 0;
    }

    //Otherwise, keep going with commit because client might have changes
    char commitPath[projNameLen+12];
    sprintf(commitPath, "%s/.Commit", argv[2]);
    remove(commitPath);
    int commitfd = open(commitPath, O_WRONLY|O_CREAT, 00600);

    //AVL head to store client manifest data
    avlNode *clientHead = NULL;

    //Tokenize version #, filepath, and hash code. Put it into an avl tree organized by file path
    clientHead = fillAvl(&clientPtr);

    //First write version number for commit for performPush's convinience
    //Will compare the live hashes and client manifest
    //And write to the proper string to stdout and to .Commit
    writeString(commitfd, serverManVer);
    writeString(commitfd, "\n");
    int commitWriteStatus = commitManDiff(commitfd, clientHead, 0);
    close(commitfd);
    
    //Helper function for comparisons keeps track of number of files written
    //Will be negative if there is an error, and will print out error in that function
    //0 means that nothing was written. Which is also handled
    if(commitWriteStatus <= 0){
        remove(commitPath);
        sendFail(sockfd);
        if(commitWriteStatus == 0) printf("Cannot Commit. There are no local changes!\n");
    } else {
        //Otherwise, send the commit file over because it was successful
        sendFile(sockfd, commitPath);
        printf("Sucessfully sent and created .Commit files on client and server\n");
    }

    //Free and clean up everything
    if(clientHead != NULL)freeAvl(clientHead);
    free(serverManVer);
    free(clientMan);

    read(sockfd, commitPath, 12);   //Wait until the server is done before closing socket
    return 0;
}

int performUpgrade(int sockfd, char** argv, char* updatePath){
    //send project name to server to see if it exists
    char* projName = messageHandler(argv[2]);
    write(sockfd, projName, strlen(projName));
    char succ[5]; succ[4] = '\0';
    //receive confirmation on whether the project exists on the server
    read(sockfd, succ, 4);
    if(!strcmp(succ, "fail")){
        printf("Could not find project on server\n");
        free(projName);
        return -1;
    }
    //write update file as a string
    char* update = stringFromFile(updatePath);
    //if update file is empty, we're done
    if(update[0]=='\0'){
        printf("Project is up to date\n");
        write(sockfd, "fail", 4);
        remove(updatePath);
        return 1;
    }
    write(sockfd, "succ", 4);

    //send version number to server to check if update num matches manifest num
    int k = 0; 
    while(update[k] != '\n') k++;
    update[k] = '\0';
    char* verNum = messageHandler(update);
    write(sockfd, verNum, strlen(verNum));
    free(verNum);
    update[k] = '\n';

    //read if server version number matches update version
    read(sockfd, succ, 4);
    if(!strcmp(succ, "fail")){
        //they dont match so fails 
        printf("Version numbers did not match up. Please update again\n");
        free(update);
        return -1;
    }

    //remake manifest if accepted
    
    //write manifest file path and remove manifest file
    char manifestFile[(strlen(argv[2]) + 11)];
    sprintf(manifestFile, "%s/.Manifest", argv[2]);
    remove(manifestFile);
    
    //Set up for sending a list of files to the server. Start with manifest
    int i =0, j = 0;
    int numFiles = 1;
    char* manifestPathProto = messageHandler(manifestFile); //Converts manifest file to protocol
    int manifestPathProtoLen = strlen(manifestPathProto);
    char* addFile = (char*) malloc(manifestPathProtoLen+1);
    int track = manifestPathProtoLen;
    sprintf(addFile, "%s", manifestPathProto);

    //read update file as string
    for(i = 0; i < strlen(update); i++){
        switch(update[i]){
            //if its an add or a modify write the file path in protocol as a list
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
    char numFilesStr[12];
    sprintf(numFilesStr, "%d", numFiles);
    //send list of files needed in protocol to server
    write(sockfd, addFile, track+1+strlen(numFilesStr));
    free(addFile);

    // receive tarred file from server with all files to add/modify
    char* tarFilePath = readWriteTarFile(sockfd);
    //untar the file to create all new needed files from the server
    char untarCommand[strlen(tarFilePath)+12];
    sprintf(untarCommand, "tar -xzvf %s", tarFilePath);
    system(untarCommand);
    remove(tarFilePath);
    free(tarFilePath);
    //write success to the server
    write(sockfd, "Succ", 4);
    printf("Wrote Files\n");
    //remove update file
    remove(updatePath);
    free(projName);
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
    //write the commit file to the server
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
    //loop through commit file based on each filename
    for(i = 0; i < strlen(commit); i++){
        while(commit[i] >= '0' && commit[i] <= '9') i++;
        
        switch(commit[i]){
            //if it needs to be deleted just gets added to a list of files to delete
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
            //if it needs to be added or modified, it gets added to the delete list and an add list
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
    //send list of files to delete to server
    write(sockfd, delFile, delTrack+1+strlen(number));
    free(delFile);
    //read success of deletes
    read(sockfd, succ, 4);

    //Tar the add and modify files and send it over
    //We already initialized the command, and we've been adding to the end of it as we read
    close(tarList);

    //if number of files to add is > 0, tar file of these files is created and sent to server. Otherwise the server is told no tar file is necessary
    if(numAddFiles > 0){
        system(tarCommand);
        write(sockfd, "succ", 4);
        sendTarFile(sockfd, tarPath);
        remove(tarPath);
    } else {
        write(sockfd, "fail", 4);
    }
    remove("tarList.txt");

    read(sockfd, succ, 4);
    remove(commitPath);

    //write the manifest from the server now
    char* manPath = readNClient(sockfd, readSizeClient(sockfd));
    remove(manPath);
    char* manifest = readNClient(sockfd, readSizeClient(sockfd));
    int manfd = open(manPath, O_CREAT|O_WRONLY, 00600);
    writeString(manfd, manifest);
    //write success of manifest creation to server
    write(sockfd, "succ", 4);
    
    //free and send user a success message
    free(tarCommand);
    free(manPath);
    free(manifest);
    printf("Successfully pushed to server!\n");
    return 0;
}

void printCurVer(char* manifest){
    //The idea is that we just want to skip over hash codes. Besides for the beginning manifest version, every other line looks similar
    //We will set every space before the hashcode to be \0 to tokenize, and print (starting after hashes)
    
    int skip = 1; //We always skip over the first space (between fileversion and path)
    char *ptr = manifest, *startWord = manifest;
    
    //Start of word is beginning of the file. Skip to first entry
    while(*ptr != '\n') ptr++;
    //ptr is now either at \n. Which is the end of manifest version
    ptr++;
    //We are at the spot after the newline
    while(*ptr != '\0'){
        //Find the space character
        while(*ptr != ' ' && *ptr != '\0') ptr++;
        
        //If skip == 1, then we are between versionNum & path. Skip this one, but not next space
        if(skip){
            skip = 0;
            ptr++;
            continue;
        }
        
        skip = 1;                   //The next space will be between ver num and path, so we should reset skip
        *ptr = '\0';                //Right now, we are between path and hash, so set null terminator before hash
        printf("%s", startWord);    //Print out the string up until now
        ptr += 34;                  //skip over the hash code(+33). Go PAST the newline[+1] (we are now at either # or \0)
        startWord = ptr-1;          //start of next print is AT the newline character;
    }

    //Print out the last newline that we missed
    printf("%s", startWord);
}

//Handles conflicts, deletes, and modifies for Update
int manDifferencesCDM(int hostupdate, int hostconflict, avlNode* hostHead, avlNode* senderHead){
    //Base case for recursion
    if(hostHead == NULL) return 0;
    
    //Find client entry in the server manifest. Take live hash of the file as well and keep track of last char of verNum
    avlNode* ptr = NULL;
    int nullCheck = findAVLNode(&ptr, senderHead, hostHead->path);
    char insert = '\0';
    char* liveHash = hash(hostHead->path);
    char lastVerChar = (hostHead->ver)[strlen(hostHead->ver)-1];    //A or R if locally added/removed
    
    
    if(liveHash == NULL || strcmp(liveHash, hostHead->code) || lastVerChar == 'A' || lastVerChar == 'R'){
        //The file was locally modified iff any of these conditions are true. This is a conflict.
        insert = 'C';
    }else if(nullCheck == -1){
        //Did not find host's entry in the server. Since its not a conflict, it was deleted on server
        insert = 'D';
    } else if (nullCheck == -2){
        //For debugging purposes and safety purposes only
        printf("A null terminator was somehow passed in\n");
    } else {
        //We found the entry in both manifests. Ignore if they are the same entry
        if(strcmp(hostHead->ver, ptr->ver) || strcmp(hostHead->code, ptr->code)){
            //Files do not match because either the version or hash are different.
            //Therefore, since it's not a conflict, it was modified on servers.
            insert = 'M';
        }
    }
    
    char write[strlen(hostHead->path) + 37]; //The pathname plus hash plus "(char)(spacex2)\0\n"
    
    //Write the proper string to stdOut and either .Update or .Conflict
    switch(insert){
        case 'D':
        case 'M': 
            if(ptr != NULL)sprintf(write, "%c %s %s\n", insert, hostHead->path, ptr->code);
            else sprintf(write, "%c %s %s\n", insert, hostHead->path, liveHash);
            writeString(hostupdate, write);
            printf("%c %s\n", insert, hostHead->path);
            break;
        case 'C':
            if(liveHash != NULL) sprintf(write, "C %s %s\n", hostHead->path, liveHash);
            else sprintf(write, "C %s 00000000000000000000000000000000\n", hostHead->path);
            writeString(hostconflict, write);
            printf("C %s\n", hostHead->path);
            break;
        default:
            break;
    }
    
    if(liveHash != NULL) free(liveHash);
    //Recurse on left and right trees
    int left = manDifferencesCDM(hostupdate, hostconflict, hostHead->left, senderHead);
    int right = manDifferencesCDM(hostupdate, hostconflict, hostHead->right, senderHead);
    return left + right;
}

//Only handles Adds for Update
int manDifferencesA(int hostupdate, int hostconflict, avlNode* hostHead, avlNode* senderHead){
    //Base case for recursion
    if(senderHead == NULL) return 0;
    
    //Try to find server's entry in the client
    avlNode* ptr = NULL;
    int nullCheck = findAVLNode(&ptr, hostHead, senderHead->path);
    
    if(nullCheck == -1){
        //Did not find server's entry in the host. They need to add this file
        //Cannot be a conflict since that is taken care of in manDifferencesCDM
        char write[strlen(senderHead->path) + 37]; //The pathname + hash + "A(2spaces)\0\n"
        sprintf(write, "A %s %s\n", senderHead->path, senderHead->code);
        writeString(hostupdate, write);
        printf("A %s\n", senderHead->path);
    }

    //recurse on left and right server trees
    int left = manDifferencesA(hostupdate, hostconflict, hostHead, senderHead->left);
    int right = manDifferencesA(hostupdate, hostconflict, hostHead, senderHead->right);
    return left + right;
}

//Finds all differences between client .manifest and livehashes for commit
int commitManDiff(int commitfd, avlNode* clientHead, int status){
    
    //Base case 0 for commit error
    if(status < 0) return status;
    //Base case 1 for normal operation
    if(clientHead == NULL) return status;

    //Grab the last char of the client man's version. Also live hash the file
    int verNumStrLen = strlen(clientHead->ver);
    char last = (clientHead->ver)[verNumStrLen-1];
    char* liveHash = hash(clientHead->path);

    //If there is no live Hash(file is unopenable) and we're not removing it, then we have an error
    //Let the user know
    if(liveHash == NULL && last != 'R'){
        printf("Error: Could not open file %s. Please use the remove command to untrack this file\n", clientHead->path);
        free(liveHash);

        //Return -1 so that all the recursion stops due to base case 0
        return -1;
    }

    //Space for the written string, numbers represent each part of the string
    //"<verNum><A/M/D> <path> <hash><newLine>"
    //Incremented version number might need 1 extra byte. i.e. 9 -> 10
    char write[(verNumStrLen+1) + 1 + 1+ strlen(clientHead->path) + 1 + 32 + 2];
    int boolWrite = 0, inc = 0;
    //If the last char is A or R, this is the local code for add/remove.
    //Otherwise check the hashes, and write M
    if(last == 'A'){
        //Set changes to true
        boolWrite = 1;
    }else if(last == 'R'){
        //Set char to D to match assignment description. Set changes to true
        //Use all 0's for hash because file is being removed
        last = 'D';
        boolWrite = 1;
        if(liveHash != NULL) free(liveHash);
        liveHash = (char*) malloc(33*sizeof(char));
        strcpy(liveHash, "00000000000000000000000000000000");
    }else if(strcmp(liveHash, clientHead->code)){
        //Set char to M to match assignment. Set changes to true.
        //Modification is the only time we increment the file version
        last = 'M';
        boolWrite = 1; inc = 1;
    }

    //Write the string as mentioned above to .Commit
    //Write simplified string to stdout
    if(boolWrite){
        sprintf(write, "%d%c %s %s\n", clientHead->verNum + inc, last, clientHead->path, liveHash);
        printf("%c %s\n", last, clientHead->path);
        writeString(commitfd, write);
        status++;
    }

    free(liveHash);
    //Recurse on left and right sub-trees, keeping track of status
    status = commitManDiff(commitfd, clientHead->left, status);
    status = commitManDiff(commitfd, clientHead->right, status);
    //Return status, which keeps track of number of writes to .Commit
    return status;
}


int performCheckout(int sockfd, char** argv){
    //Write project name to server
    char* projName = messageHandler(argv[2]);
    write(sockfd, projName, strlen(projName));


    //Check to see if project exists on server
    char* confirm = readNClient(sockfd, readSizeClient(sockfd));
    if(!strcmp("fail", confirm)){
        printf("Error: The project does not exist on the server side\n");
        free(confirm);
        return -1;
    }
    free(confirm);

    //Check to see if the project already exists locally. Let the server know.
    DIR* project = opendir(argv[2]);
    if(project != NULL){
        closedir(project);
        printf("The project folder already exists on the client. Please delete it before checkout \n");
        sendFail(sockfd);
        return -1;
    }
    write(sockfd, "4:succ", 6);
    
    //Otherwise, everything is all good for checkout. Wait for server to send tarFile
    char* tarFilePath = readWriteTarFile(sockfd);
    if(tarFilePath == NULL){
        //Safety check. This means that readWriteTarFile failed, so tar file could not be created
        free(projName);
        sendFail(sockfd);
        return -1;
    }

    //Set up for untar command. Then remove the tar file when done
    int tarFilePathLen = strlen(tarFilePath);
    char tarCommand[12+tarFilePathLen];
    sprintf(tarCommand, "tar -xzvf %s", tarFilePath);
    system(tarCommand);
    remove(tarFilePath);

    //Write that it was successful. Free everything
    write(sockfd, "4:succ", 6);
    free(projName);
    free(tarFilePath);
    return 0;
}