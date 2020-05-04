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

/* TODO LIST:::
    REMEMBER THAT WHEN WE SUBMIT, SERVER AND CLIENT HAVE TO BE IN THE SAME DIRECTORY
        This might change how things compile. Makefile needs to be changed to reflect new client/server.c paths
        We also may need to change the #include headers

    COMMENT AND CLEAN UP PRINT STATEMENTS
    testplan.txt
    testcases.txt
    Finish up readme.pdf
    WTFTest
*/

//Set global variables so that signal handler can access them
int sockfd;
int killProgram = 0;
void interruptHandler(int sig){
    killProgram = 1;
    close(sockfd);  //Prevents other connections from being accepted
}


int main(int argc, char* argv[]){
    //Get rid of buffer in stdout
    setbuf(stdout, NULL);
    signal(SIGINT, interruptHandler);
    int port;
    char* ipAddr;
    int bytes;

    char buffer[256];
    

    if(strcmp(argv[1], "configure") == 0){
        if(argc < 4) error("Not enough arguments for configure. Need IP & Port\n");
        writeConfigureFile(argv[2], argv[3]);
        printf("Successfully created configure file with hostname: %s and port: %s\n", argv[2], argv[3]);
        return 0;
    }

    //figure out what command to operate
    command mode = argCheck(argc, argv[1]);

    //Exit on error. Don't try to connect to server if its add/remove
    if(mode == ERROR){
        printf("Please enter a valid command with the valid arguments\n");
        return -1;
    }
    if(mode == add){
        performAdd(argv);
        return 0;
    }
    if(mode == rmv){
        performRemove(argv);
        return 0;
    }

    //Read IP and port from .configure file. Exit if no .configure
    int configureFile = open(".configure", O_RDONLY);
    if(configureFile < 0) error("Fatal Error: There is no configure file. Please use ./WTF configure <IP/host> <Port>\n");
    ipAddr = getConfigInfo(configureFile, &port);
    close(configureFile);

    //Set up connection, quit if something goes wrong besides not connecting to server 
    struct sockaddr_in servaddr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        error("error opening socket");
    }
    else printf("successfully opened socket\n");

    server = gethostbyname(ipAddr);
    free(ipAddr); //ipAddr is no longer needed

    if (server == NULL){
        printf("Fatal Error: Cannot get host. Is the given IP/host correct?\n");
        exit(0);
    }
    else printf("successfully found host\n");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    servaddr.sin_port = htons(port);

    //Keep trying to connect to server every 3 seconds until SIGINT or connect
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
    
    //Read that we are successfully connected. Then write to server the command
    char cmd[3]; bzero(cmd, 3);
    read(sockfd, buffer, 32);
    printf("%s\n", buffer);
    sprintf(cmd, "%d", mode);
    write(sockfd, cmd, 3);
    
    //Perform the right operation based on what command user gave
    switch(mode){
        case checkout:
            printf("Performing Checkout\n");
            performCheckout(sockfd, argv);
            break;
        case update:          
            printf("Performing Update\n");
            performUpdate(sockfd, argv);
            write(sockfd, "4:Done", 6);
            break; 
        case upgrade:{ 
            int len = strlen(argv[2]);
            char dotfilepath[len + 11]; dotfilepath[0] = '\0';
            sprintf(dotfilepath, "%s/.Conflict", argv[2]);
            int conflictFile = open(dotfilepath, O_RDONLY);
            if(conflictFile> 0){
                close(conflictFile);
                printf("Conflicts exist. Please resolve all conflicts and update\n");
                write(sockfd, "8:Conflict", 10);
                return -1;
            }
            bzero(dotfilepath, len+9);
            sprintf(dotfilepath, "%s/.Update", argv[2]);
            printf("%s\n", dotfilepath);
            int updateFile = open(dotfilepath, O_RDONLY);
            if(updateFile < 0) {
                printf("No openable update file available. First run an update before upgrading\n");
                write(sockfd, "6:Update", 8);
                return -1;
            }
            close(updateFile);
            write(sockfd, "4:Succ", 6);
            performUpgrade(sockfd, argv, dotfilepath);
            break;}
        case commit: 
            printf("Performing Commit\n");
            performCommit(sockfd, argv);
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
            char succ[5]; succ[4] = '\0';
            read(sockfd, succ, 4);
            printf("Succ: %s\n", succ);
            if(!strcmp(succ, "fail")){
                printf("Project does not exist.\n");
                return -1;
            }
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
            //Send project Name over
            char sendFile[12+strlen(argv[2])];
            sprintf(sendFile, "%d:%s", strlen(argv[2]), argv[2]);
            write(sockfd, sendFile, strlen(sendFile));
            
            char* test = readNClient(sockfd, readSizeClient(sockfd)); //Read/Throw away the filePath but make sure its not an error message
            if(!strncmp(test, "Could", 5)){
                printf("Error: %s\n", test);
                free(test);
                break;
            }
            free(test);
            
            char* manifest = readNClient(sockfd, readSizeClient(sockfd)); //Read the actual data (.manifest)
            printf("Success! Current Version received:\n");
            write(sockfd, "4:Done", 6); //Tell the server that we are done. Connection can be terminated
            printCurVer(manifest); //Process the .manifest to print currentversion
            free(manifest);
            break;}
        case history:{
            //Send project Name over
            char sendFile[12+strlen(argv[2])];
            sprintf(sendFile, "%d:%s", strlen(argv[2]), argv[2]);
            write(sockfd, sendFile, strlen(sendFile));
            
            char* confirm = readNClient(sockfd, readSizeClient(sockfd)); //Read/Throw away the filePath, make sure its not an error
            if(!strncmp(confirm, "Could", 5)){
                printf("Error: %s\n", confirm);
                free(confirm);
                break;
            }
            free(confirm);
            
            //Read .History and print out
            char* history = readNClient(sockfd, readSizeClient(sockfd));
            printf("Success! History received:\n%s", history);
            free(history);
            break;}
        case rollback:{
            int ver = atoi(argv[3]);
            performRollback(ver, argv, sockfd);
            break;}
        default:
            break;
    }
    printf("Disconnected from server\n");
    return 0;
}

