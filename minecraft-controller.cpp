// minecraft-controller.cpp
#include "rlibrary/rstdio.h"
#include "rlibrary/rstringstream.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "minecraft-server.h"
#include "client.h"
#include "minecraft-controller.h"
using namespace rtypes;
using namespace minecraft_controller;

// constants
static const char* const LOG_FILE = "/etc/minecontrol.log";
static const char* const DOMAIN_NAME = "minecraft-control";

// globals
const char* minecraft_controller::PROGRAM_NAME;
const char* const minecraft_controller::PROGRAM_VERSION = "0.3 (Beta Test)";
minecraft_controller_log_stream minecraft_controller::standardLog;
static domain_socket local;

// types
class minecraft_controller_error { };

// functions
static void daemonize(); // turns this process into a daemon
static void terminate_handler(int); // recieves SIGTERM event
static bool local_operation(); // sends control to the local server operation

int main(int,const char* argv[])
{
    // perform global setup
    PROGRAM_NAME = argv[0];

    // become a daemon
    daemonize();

    // log process start
    standardLog << "process started" << endline;

    // set up signal handler for TERM and INT events
    if (::signal(SIGTERM,&terminate_handler) == SIG_ERR)
    {
        standardLog << "cannot create signal handler for SIGTERM" << endline;
        _exit(1);
    }
    if (::signal(SIGINT,&terminate_handler) == SIG_ERR)
    {
        standardLog << "cannot create signal handler for SIGINT" << endline;
        _exit(1);
    }

    // perform startup operations
    minecraft_server_manager::startup_server_manager();

    // begin local server operation
    if ( !local_operation() )
        return 1;

    // perform shutdown operations
    controller_client::shutdown_clients();
    minecraft_server_manager::shutdown_server_manager();

    // log process completion
    standardLog << "process complete" << endline;
    return 0;
}

void daemonize()
{
    // let's become a daemon
    pid_t pid = ::fork();
    if (pid == -1)
    {
        standardLog << "cannot fork a child process" << endline;
        ::_exit(1);
    }
    if (pid == 0) // child process stays alive
    {
        // ensure that the process will have no controlling terminal
        if (::setsid() == -1)
        {
            standardLog << "cannot become leader of new session" << endline;
            ::_exit(1);
        }
        // reset umask
        ::umask(0);
        // set working directory to root
        ::chdir("/");
        int fd, fdNull;
        // duplicate:
        //  log file to STDOUT and STDERR
        //  null to STDIN
        fd = ::open(LOG_FILE,O_RDWR|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
        fdNull = ::open("/dev/null",O_RDWR);
        if (fd == -1)
        {
            standardLog << "cannot open log file";
            _exit(1);
        }
        if (fdNull == -1)
        {
            standardLog << "cannot open null device" << endline;
            ::_exit(1);
        }
        if (::dup2(fdNull,STDIN_FILENO)!=STDIN_FILENO || ::dup2(fd,STDOUT_FILENO)!=STDOUT_FILENO || ::dup2(fd,STDERR_FILENO)!=STDERR_FILENO)
        {
            standardLog << "cannot replace standard IO" << endline;
            ::_exit(1);
        }
        ::close(fd);
        ::close(fdNull);
    }
    else
        ::_exit(0);
}

void terminate_handler(int sig)
{
    standardLog << "the server is going down; received " << ::strsignal(sig) << " signal" << endline;
    local.shutdown();
    local.close();
}

bool local_operation()
{
    // create domain socket for client CLI application communication
    if ( !local.open(DOMAIN_NAME) )
    {
        standardLog << "cannot open domain socket for minecraft control" << endline;
        return false;
    }
    // accept incoming connections
    while (true)
        // accept a connection
        if (controller_client::accept_client(local) == NULL) // assume `local' was shutdown
            break;
    return true;
}

// minecraft_controller::minecraft_controller_log_stream
minecraft_controller_log_stream::minecraft_controller_log_stream()
{
    // enable automatic buffering
    _doesBuffer = true;
}
void minecraft_controller_log_stream::_outDevice()
{
    // send our local buffer to the standard console
    // add name and process id
    stdConsole << PROGRAM_NAME 
               << '[' << ::getpid() << "]: ";
    stdConsole.place(*this);
    stdConsole.flush_output();
    _bufOut.clear();
}
