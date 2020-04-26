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

#include <signal.h>

#include "../sharedFunctions.h"
/* TODO LIST:::
    Create .h and .c files
    Move the argc checks to argCheck instead of the switch cases.
    Create a function for generating filePath given proj name && file
    Create a function that returns 2 .Manifest files (SHARED FUNCTIONS)
        Use in update (clientside) && commit(serverside) 
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
//int connectToServer(char* ipAddr, int port);
command argCheck(int argc, char* arg);

int main(int argc, char* argv[]){
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
            break; 
        case upgrade: 
            printf("upgrade\n");
            break;
        case commit: 
            printf("commit\n");
            break;
        case push: 
            printf("push\n");
            break;
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
            readNClient(sockfd, readSizeClient(sockfd)); //Throw away the filePath
            char* manifest = readNClient(sockfd, readSizeClient(sockfd));
            printf("Success! Current Version received:\n");
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
    else if(strcmp(arg, "upgrade") == 0) mode = upgrade;
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

/*int readSizeServer(int socket){
    int status = 0, bytesRead = 0;
    char buffer[11];
    do{
        status = read(socket, buffer+bytesRead, 1);
        bytesRead += status;
    }while(status > 0 && buffer[bytesRead-1] != ':');
    buffer[bytesRead-1] = '\0';
    return atoi(buffer);
}*/

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
    readNClient(sockfd, readSizeClient(sockfd)); //Throw away the file path
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

    //Get the manifest version numbers of each
    int serverManVerNum;
    if(!strcmp(serverMan, clientMan)){
        printf("Server and Client Manifests are the same!\n");
    } else {
        printf("There are differences between manifests\n");
        printf("Client:\n%s", clientMan);
        printf("Server:\n%s", serverMan);
    }

    free(serverMan);
    free(clientMan);
    return 0;
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