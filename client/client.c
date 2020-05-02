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

#include "../sharedFunctions.h"
#include "../avl.h"

/* TODO LIST:::
    Create .h and .c files
    Move the argc checks to argCheck instead of the switch cases.
    Create a function for generating filePath given proj name && file
    Create a function that returns 2 .Manifest files (SHARED FUNCTIONS)
        Use in update (clientside) && commit(serverside) 
    
    REMEMBER THAT WHEN WE SUBMIT, SERVER AND CLIENT HAVE TO BE IN THE SAME DIRECTORY
        This might change how things compile. Makefile needs to be changed to reflect new client/server.c paths
        We also may need to change the #include headers
*/

int sockfd;
int killProgram = 0;
void interruptHandler(int sig){
    killProgram = 1;
    close(sockfd);
}

void writeConfigureFile(char* IP, char* port);
void sendServerCommand(int socket, char* command, int comLen);
char* getConfigInfo(int config, int* port);
int writeString(int fd, char* string);
char* hash(char* path);
int performCreate(int socket, char** argv);
void printCurVer(char* manifest);
int performUpdate(int sockfd, char** argv);
command argCheck(int argc, char* arg);
void advanceToken(char** ptr, char delimiter);
int manDifferencesCDM(int, int, avlNode*, avlNode*);
int manDifferencesA(int, int, avlNode*, avlNode*);
int performPush(int, char**, char*);
int performUpgrade(int, char**, char*);

int main(int argc, char* argv[]){
    setbuf(stdout, NULL);
    //int sockfd;
    signal(SIGINT, interruptHandler);
    int port;
    char* ipAddr;
    int bytes;

    char buffer[256];
    //Should we check that IP/Host && Port are valid
    if(strcmp(argv[1], "configure") == 0){
        if(argc < 4) error("Not enough arguments for configure. Need IP & Port\n");
        writeConfigureFile(argv[2], argv[3]);
        return 0;
    }
    //figure out what command to operate
    command mode = argCheck(argc, argv[1]);
    if(mode == ERROR){
        printf("Please enter a valid command with the valid arguments\n");
        return -1;
    }
    if(mode == add){
        if(argc != 4){
            printf("Not enough arguments for this command. Proper usage is: ./WTF add <projectName> <filename>");
            return -1;
        }
        performAdd(argv);
        return 0;
    }
    else if(mode == rmv){
        if(argc != 4){
            printf("Not enough arguments for this command. Proper useage is: ./WTF remove <projectName> <fileName>");
            return -1;
        }
        performRemove(argv);
        return 0;
    }

    int configureFile = open(".configure", O_RDONLY);
    if(configureFile < 0) error("Fatal Error: There is no configure file. Please use ./WTF configure <IP/host> <Port>\n");
    ipAddr = getConfigInfo(configureFile, &port);
    close(configureFile);


    

    //connectToServer(ipAddr, port);
    struct sockaddr_in servaddr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        error("error opening socket");
    }
    else printf("successfully opened socket\n");

    server = gethostbyname(ipAddr);
    free(ipAddr); //No longer needed

    if (server == NULL){
        printf("Fatal Error: Cannot get host. Is the given IP/host correct?\n");
        exit(0);
    }
    else printf("successfully found host\n");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    //servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(port);
    while(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) != 0){
        printf("Error: Could not connect to server. Retrying in 3 seconds...\n");
        sleep(3);
        if(killProgram) break;
    }
    if(killProgram){
        printf("\nClosing sockets and freeing\n");
        return 0;
    }
    printf("successfully connected to host.\n");
    
    char cmd[3]; bzero(cmd, 3);
    read(sockfd, buffer, 32);
    printf("%s\n", buffer);
    sprintf(cmd, "%d", mode);
    write(sockfd, cmd, 3);
    //write(sockfd, argv[1], strlen(argv[1]));
    switch(mode){
        case checkout:
            printf("checkout\n");
            break;
        case update:          
            printf("update\n");
            performUpdate(sockfd, argv);
            write(sockfd, "4:Done", 6);
            break; 
        case upgrade:{ 
            int len = strlen(argv[2]);
            char dotfilepath[len + 11]; dotfilepath[0] = '\0';
            sprintf(dotfilepath, "%s/.Conflict", argv[2]);
            if(open(dotfilepath, O_RDONLY) > 0){
                printf("Conflicts exist. Please resolve all conflicts and update\n");
                write(sockfd, "8:Conflict", 10);
                return -1;
            }
            bzero(dotfilepath, len+9);
            sprintf(dotfilepath, "%s/.Update", argv[2]);
            printf("%s\n", dotfilepath);
            if(open(dotfilepath, O_RDONLY)< 0) {
                printf("No openable update file available. First run an update before upgrading\n");
                write(sockfd, "6:Update", 8);
                return -1;
            }
            write(sockfd, "4:Succ", 6);
            performUpgrade(sockfd, argv, dotfilepath);
            break;}
        case commit: 
            printf("commit\n");
            performCommit(sockfd, argv);
            write(sockfd, "4:Done", 6);
            break;
        case push:{ 
            int len = strlen(argv[2]);
            char dotfilepath[len+11]; dotfilepath[0] = '\0';
            sprintf(dotfilepath, "%s/.Commit", argv[2]);
            if(open(dotfilepath, O_RDONLY) < 0){
                printf("No .Commit file detected. Please run commit first");
                write(sockfd, "6:Commit", 8);
                return -1;
            }
            write(sockfd, "4:Succ", 6);
            char* projName = messageHandler(argv[2]);
            write(sockfd, projName, strlen(projName));
            performPush(sockfd, argv, dotfilepath);
            break;}
        case create:{
            performCreate(sockfd, argv);
            break;}
        case destroy:{
            char sendFile[12+strlen(argv[2])];
            sprintf(sendFile, "%d:%s", strlen(argv[2]), argv[2]);
            write(sockfd, sendFile, strlen(sendFile));
            bytes = readSizeClient(sockfd);
            char returnMsg[bytes + 1]; bzero(returnMsg, bytes+1);
            read(sockfd, returnMsg, bytes);
            printf("%s\n", returnMsg);
            //printf("%s\n", hash("hashtest.txt"));
            break;}
        case currentversion:{
            char sendFile[12+strlen(argv[2])];
            sprintf(sendFile, "%d:%s", strlen(argv[2]), argv[2]);
            write(sockfd, sendFile, strlen(sendFile));
            char* test = readNClient(sockfd, readSizeClient(sockfd)); //Throw away the filePath but make sure its not an error message
            if(!strncmp(test, "Could", 5)){
                printf("%s\n", test);
                free(test);
                break;
            }
            char* manifest = readNClient(sockfd, readSizeClient(sockfd));
            printf("Success! Current Version received:\n");
            write(sockfd, "4:Done", 6);
            printCurVer(manifest);
            free(manifest);
            break;}
        case history:{
            char sendFile[12+strlen(argv[2])];
            sprintf(sendFile, "%d:%s", strlen(argv[2]), argv[2]);
            write(sockfd, sendFile, strlen(sendFile));
            readNClient(sockfd, readSizeClient(sockfd)); //Throw away the filePath
            char* history = readNClient(sockfd, readSizeClient(sockfd));
            printf("Success! History received:\n%s", history);
            free(history);
            break;}
        case rollback: 
            read(sockfd, buffer, 32);
            printf("%s\n", buffer);
            printf("rollback\n");
            break;
        default:
            break;
    }
    return 0;
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
    //printf("Size: %d, writeFile: %s", len3-1, writefile);
    char* hashcode = hash(writefile);
    if(hashcode == NULL){
        printf("Fatal Error: Cannot open/hash file. Make sure it exists with write permissions\n");
        free(writefile);
        free(manPath);
        return 1;
    }
    writefile[len3-2] = ' ';

    //check if if file already exists in manifest:
    int manfd = open(manPath, O_RDONLY);
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
    }while(status != 0);
    manifest[size] = '\0';
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
                    close(manfd);
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
                close(manfd);
                return -1;
            }
            else while(manifest[i] != '\n' && manifest[i] != '\0') i++;
        }
    }
    close(manfd);
    //add it
    printf("%s\n", writefile);
    printf("%d\n", strlen(writefile));
    manfd = open(manPath, O_WRONLY | O_APPEND);
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
    int manfd = open(manPath, O_RDONLY);
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

    manifest[size] = '\0';
    //char newmani[size+3]; bzero(newmani, size+3);
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
                    close(manfd);
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
                    close(manfd);
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
                close(manfd);
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
    close(manfd);
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

//Will read the config file and return output in ipAddr and port
//On error, it will close the config file and quit
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

//Write to fd. Return 0 on success but 1 if there was a write error
int writeString(int fd, char* string){

    int status = 0, bytesWritten = 0, strLength = strlen(string);
    
    do{
        status = write(fd, string + bytesWritten, strLength - bytesWritten);
        bytesWritten += status;
    }while(status > 0 && bytesWritten < strLength);
    
    if(status < 0) return 1;
    return 0;
}


command argCheck(int argc, char* arg){
    command mode = ERROR;

    if(strcmp(arg, "checkout") == 0) mode = checkout;
    else if(strcmp(arg, "update") == 0){
        if(argc == 3) mode = update;
        else printf("Fatal Error: update requires only 1 additional argument (project name)\n");
    }
    else if(strcmp(arg, "upgrade") == 0) {
        if(argc == 3) mode = upgrade;
        else printf("Fatal Error: upgrade requires 1 argument (project name). Proper usage is ./WTF upgrade <projectName>\n");
    }
    else if(strcmp(arg, "commit") == 0) mode = commit;
    else if(strcmp(arg, "create") == 0){
        if(argc == 3) mode = create;
        else printf("Fatal Error: create requires only 1 additional argument (project name)\n");
    }
    else if(strcmp(arg, "destroy") == 0){
        if(argc == 3) mode = destroy;
        else printf("Fatal Error: destroy requires only 1 additional argument (project name)\n");
    }
    else if(strcmp(arg, "add") == 0) mode = add;
    else if(strcmp(arg, "remove") == 0) mode = rmv;
    else if(strcmp(arg, "currentversion") == 0){
        if(argc == 3) mode = currentversion;
        else printf("Fatal Error: currentversion requires only 1 additional argument (project name)\n");
    }
    else if(strcmp(arg, "history") == 0){
        if(argc == 3) mode = history;
        else printf("Fatal Error: history requires only 1 additional argument (project name)\n");
    }
    else if(strcmp(arg, "rollback") == 0) mode = rollback;
    
    return mode;
}

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

int performCommit(int socket, char** argv){
/*
    int projNameLen = strlen(argv[2]);
    char sendFile[12+projNameLen)];
    sprintf(sendFile, "%d:%s", projNameLen, argv[2]);
    write(sockfd, sendFile, strlen(sendFile));

    char manPath[projNameLen + 12];
    sprintf(manPath, "%s/.Manifest", projName);
    int check = sendFile(socket, manPath);
    if(check == 0)printf("Successfully sent current version to server\n");
    else printf("Something went wrong with sendManifest (%d)\n", check);

    char* confirm = readNClient(socket, readSizeClient(socket));
    if(!strcmp(confirm, "Success")){
        printf("Successful Commit!\n");
    } else {
        printf("Error: %s\n", confirm);
    }
    free(confirm);*/

    /* FOR CLIENT

        Copy update function basically but remove the .conflict part
        Make sure to add check to make sure that manifest are matched
        Create new functiosn that check through the AVL for the .commits
        close the .commit
        send the file to server
        end on confirmation

    */


}

int performUpgrade(int sockfd, char** argv, char* updatePath){
    char* projName = messageHandler(argv[2]);
    write(sockfd, projName, strlen(projName));
    int updatefd = open(updatePath, O_RDONLY);
    int size = lseek(updatefd, 0, SEEK_END);
    if(size==0){
        printf("Project is up to date");
        close(updatefd);
        remove(updatePath);
        return 1;
    }
    char manifestFile[(strlen(argv[2]) + 11)];
    sprintf(manifestFile, "%s/.Manifest", argv[2]);
    remove(manifestFile);
    char* manifest = readNClient(sockfd, readSizeClient(sockfd));
    printf("%s\n", manifest);
    int manfd = open(manifestFile, O_WRONLY | O_CREAT, 00600);
    writeString(manfd, manifest);
    close(manfd);
    free(manifest);
    lseek(updatefd, 0, SEEK_SET);
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
    close(updatefd);
    int i =0, j = 0;
    int numFiles = 0;
    char* addFile = (char*) malloc(1); addFile[0] = '\0';
    int track = 0;
    for(i = 0; i < size +1; i++){
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
    int i = strlen(path) -1;
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
    char* commitmsg = messageHandler(commit);
    write(sockfd, commitmsg, strlen(commitmsg));
    free(commitmsg);
    //check success of commit compare
    char succ[5]; succ[4] = '\0';
    read(sockfd, succ, 4);
    if(!strcmp(succ, "Fail")){
        printf("commits did not match up\n");
        return -1;
    }
    int i = 0, numAddFiles = 0, numDelFiles = 0;
    char* addFile = (char*) malloc(1); addFile[0] = '\0';
    char* delFile = (char*) malloc(1); delFile[0] = '\0';
    int addTrack = 0, delTrack = 0;
    for(i = 0; i < size+1; i++){
        switch(commit[i]){
            case 'D':{
                int i = i+1;
                int end = i;
                while(commit[end] != ' ') end++;
                char filename[end-i+1]; bzero(filename, end-i+1);
                strncpy(filename, &commit[i], end-i);
                filename[end-i] = '\0';
                numDelFiles++;
                char* fileAndSize = messageHandler(filename);
                delTrack+= strlen(fileAndSize);
                char* temp = (char*) malloc(delTrack+2);
                sprintf(temp, "%s%s", delFile, fileAndSize);
                free(delFile);
                free(fileAndSize);
                delFile = temp;
                break;
            }
            case 'A':
            case 'M':{
                i = i+2;
                int end = i;
                while(commit[end] != ' ') end++;
                char filename[end-i+1]; bzero(filename, end-i+1);
                strncpy(filename, &commit[i], end-i);
                filename[end-i] = '\0';
                numAddFiles++;
                char* fileAndSize = messageHandler(filename);
            }
        }
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
