#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

void makeClientResult();
void makeServerResult();
void makeResultDirectory();
int writeString(int fd, char* string);

int main(int argc, char* argv[]){
    /*makeClientResult();
    makeServerResult();
    makeResultDirectory();*/
    
    int pid = fork();
    if(pid == 0){
        int fd = open("serverStdOut.txt", O_CREAT|O_WRONLY, 00600);
        dup2(fd ,1);
        close(fd);
        char* argv[3] = {"sadf", "9998", NULL};
        execv("./server/WTFserver", argv);
    }
    else if(pid >0){
        system("cd client; ./WTF configure localhost 9998 > ../clientStdOut.txt");
        system("cd client; ./WTF create testProject >> ../clientStdOut.txt");
        system("cd client; ./WTF create destroyThis >> ../clientStdOut.txt");
        system("cd client; ./WTF destroy destroyThis >> ../clientStdOut.txt");
        system("cd client; touch testProject/test.txt; echo \"abcde\" > testProject/test.txt");
        system("cd client; touch testProject/removeThis.txt");
        system("cd client; ./WTF add testProject test.txt >> ../clientStdOut.txt");
        system("cd client; ./WTF add testProject removeThis.txt >> ../clientStdOut.txt");
        system("cd client; ./WTF commit testProject >> ../clientStdOut.txt");
        system("cd client; ./WTF push testProject >> ../clientStdOut.txt");
        system("cd client; ./WTF remove testProject removeThis.txt >> ../clientStdOut.txt");
        system("cd client; ./WTF commit testProject >> ../clientStdOut.txt");
        system("cd client; ./WTF push testProject >> ../clientStdOut.txt");
        system("cd client; ./WTF rollback testProject 1 >> ../clientStdOut.txt");
        system("cd client; ./WTF update testProject >> ../clientStdOut.txt");
        system("cd client; ./WTF upgrade testProject >> ../clientStdOut.txt");
        system("cd client; ./WTF currentversion testProject >> ../clientStdOut.txt");
        system("cd client; ./WTF history testProject >> ../clientStdOut.txt");
        system("cd client; rm -r testProject");
        system("cd client; ./WTF checkout testProject >> ../clientStdOut.txt");
        kill(pid, SIGINT);
        wait();
    }
}

void makeClientResult(){
    char* result = "Successfully created configure file with hostname: localhost and port: 9998\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\ntestProject was created on server!\nProject successfully created locally!\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\ndestroyThis was created on server!\nProject successfully created locally!\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\nSuccessfully destroyed project\nDisconnected from server\nSuccessfully added file to .Manifest\nSuccessfully added file to .Manifest\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\nPerforming Commit\nA ./testProject/test.txt\nA ./testProject/removeThis.txt\nSucessfully sent and created .Commit files on client and server\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\n./testProject/test.txt\n./testProject/removeThis.txt\nSuccessfully pushed to server!\nDisconnected from server\nSuccessfully removed file from manifest.\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\nPerforming Commit\nD ./testProject/removeThis.txt\nSucessfully sent and created .Commit files on client and server\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\nSuccessfully pushed to server!\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\nSuccessfully rolled back to version 1\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\nPerforming Update\nA ./testProject/removeThis.txt\nUpdate was successful!\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\ntestProject/.Update\ntestProject/.Manifest\n./testProject/removeThis.txt\nUpgraded local copy of project successfully\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\nSuccess! Current Version received:\n1\n0 ./testProject/removeThis.txt\n0 ./testProject/test.txt\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\nSuccess! History received:\n0\n0A ./testProject/test.txt 9b9af6945c95f1aa302a61acf75c9bd6\n0A ./testProject/removeThis.txt d41d8cd98f00b204e9800998ecf8427e\n\nDisconnected from server\nsuccessfully opened socket\nsuccessfully found host\nsuccessfully connected to host.\nYou are connected to the server\nPerforming Checkout\ntestProject/\ntestProject/.Manifest\ntestProject/test.txt\ntestProject/removeThis.txt\nCheckout was successful!\nDisconnected from server\n";
    int fd = open(".ClientResult.txt", O_CREAT | O_WRONLY, 00600);
    writeString(fd, result);
    close(fd);
}

void makeServerResult(){
    char* result = "Socket created \nSocket binded\nCreating list of projects...NULL\nSuccess!\nserver listening\nServer is ready to accept clients!\nServer accepted client with IP 127.0.0.1\nSuccesful server-side project creation. Notifying Client\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nSuccesful server-side project creation. Notifying Client\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nSuccessfully destroyed destroyThis\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nSuccessfully sent manifest to client\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nCommits match up\ntestProject/.v0/\ntestProject/.v0/.Manifest\n./testProject/test.txt\n./testProject/removeThis.txt\nSuccessfully received client's push\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nSuccessfully sent manifest to client\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nCommits match up\ntestProject/.v1/\ntestProject/.v1/.Manifest\ntestProject/.v1/.History\ntestProject/.v1/.v0.tar.gz\ntestProject/.v1/test.txt\ntestProject/.v1/removeThis.txt\nSuccessfully received client's push\nDisconnected from client\nServer accepted client with IP 127.0.0.1\ntestProject/.v1/\ntestProject/.v1/.Manifest\ntestProject/.v1/.History\ntestProject/.v1/.v0.tar.gz\ntestProject/.v1/test.txt\ntestProject/.v1/removeThis.txt\nSuccessful rollback\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nSuccessfully sent current version to client\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nVersion numbers match\ntestProject/.Manifest\n./testProject/removeThis.txt\nUpgrade was successful!\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nSuccessfully sent current version to client\nDisconnected from client\nServer accepted client with IP 127.0.0.1\nSuccessfully sent history to client\nDisconnected from client\nServer accepted client with IP 127.0.0.1\ntestProject/\ntestProject/.Manifest\ntestProject/test.txt\ntestProject/removeThis.txt\nClient successfully received project: testProject\nDisconnected from client\nSuccessfully closed all sockets and threads!\n";

    int fd = open(".ServerResult.txt", O_CREAT | O_WRONLY, 00600);
    writeString(fd, result);
    close(fd);
}

void makeResultDirectory(){
    mkdir(".clientResult");
    mkdir(".clientResult/destroyThisResult");

}

int writeString(int fd, char* string){

    int status = 0, bytesWritten = 0, strLength = strlen(string);
    
    do{
        status = write(fd, string + bytesWritten, strLength - bytesWritten);
        bytesWritten += status;
    }while(status > 0 && bytesWritten < strLength);
    
    if(status < 0) return 1;
    return 0;
}