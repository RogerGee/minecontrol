// minecraft-controller.cpp
#include "rlibrary/rstdio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
using namespace rtypes;
//using namespace minecraft_controller;

// globals
static const char* programName;
static standard_stream errConsole;

// functions
static void daemonize();

int main(int,const char* argv[])
{
    // perform global setup
    programName = argv[0];
    errConsole = stdConsole;
    errConsole << err;
    // become a daemon
    daemonize();
}

void daemonize()
{
    // let's become a daemon
    pid_t mypid = getpid();
    pid_t pid = fork();
    if (pid == -1)
    {
        errConsole << programName << '<' << mypid << ">: cannot fork a child process" << endline;
        _exit(1);
    }
    if (pid == 0) // child process stays alive
    {
        mypid = getpid(); // reset PID since new process was spawned
        // ensure that the process will have no controlling terminal
        if (setsid() == -1)
        {
            errConsole << programName << '<' << mypid << ">: cannot become leader of new session" << endline;
            _exit(1);
        }
        // reset umask
        umask(0);
        // set working directory to root
        chdir("/");
        // try to close all file descriptors
        int maxfd, fd;
        maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd == -1)
            maxfd = 8192;
        for (fd = 0;fd<maxfd;fd++)
            close(fd);
        // duplicate '/dev/null' to standard IO descriptors
        close(STDIN_FILENO);
        fd = open("/dev/null",O_RDWR);
        if (fd!=STDIN_FILENO || dup2(STDIN_FILENO,STDOUT_FILENO)!=STDOUT_FILENO || dup2(STDIN_FILENO,STDERR_FILENO)!=STDERR_FILENO)
        {
            errConsole << programName << '<' << mypid << ">: cannot duplicate null device" << endline;
            _exit(1);
        }
    }
    else
        _exit(0);
}
