#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

int main(int argc, char* argv[]){
    int pid = fork();
    if(pid == 0){
        int fd = open("serverStdOut.txt", O_CREAT|O_WRONLY, 00600);
        dup2(fd ,1);
        close(fd);
        char* argv[2] = {"9998", NULL};
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