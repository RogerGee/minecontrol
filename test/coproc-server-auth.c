#include <unistd.h>
#include <stdio.h>

static const char* const SERVER_CMD_LINE[] = {
    "java",
    "-jar",
    "/mnt/tmpdisk/minecraft_server.jar",
    "nogui",
    NULL
};

int main(int argc,const char* argv[])
{
    pid_t id;
    int pipeA[2], pipeB[2];

    if (argc < 2) {
        fprintf(stderr,"Usage: %s authority-script\n",argv[0]);
        return 1;
    }
    
    /* create pipes */
    if (pipe(pipeA)==-1 || pipe(pipeB)==-1) {
        fprintf(stderr,"can't create pipe\n");
        return 1;
    }

    id = fork();
    if (id == -1) {
        fprintf(stderr,"fail fork()\n");
        return 1;
    }
    else if (id == 0) {
        /* the child becomes the authority script 
            it reads from pipeA and writes to pipeB */
        if (dup2(pipeA[0],0) != 0) {
            fprintf(stderr,"fail dup on STDIN\n");
            return 1;
        }
        if (dup2(pipeB[1],1) != 1) {
            fprintf(stderr,"fail dup on STDOUT\n");
            return 1;
        }
        close(pipeA[0]);
        close(pipeA[1]);
        close(pipeB[0]);
        close(pipeB[1]);

        if (execl(argv[1],argv[1],NULL) == -1) {
            fprintf(stderr,"could not exec %s\n",argv[1]);
            return 1;
        }
    }

    /* parent becomes minecraft 
        it reads from pipeB and writes to pipeA */
    if (dup2(pipeB[0],0) != 0) {
        fprintf(stderr,"fail dup on STDIN\n");
        return 1;
    }
    if (dup2(pipeA[1],1) != 1) {
        fprintf(stderr,"fail dup on STDOUT\n");
        return 1;
    }
    close(pipeA[0]);
    close(pipeA[1]);
    close(pipeB[0]);
    close(pipeB[1]);

    if (execv("/usr/bin/java",SERVER_CMD_LINE) == -1) {
        fprintf(stderr,"could not exec minecraft server\n");
        return 1;
    }

    /* no control here */
}
