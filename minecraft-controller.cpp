// minecraft-controller.cpp
#include "minecraft-server.h"
#include "minecontrol-client.h"
#include "minecraft-controller.h"
#include "domain-socket.h"
#include "net-socket.h"
#include <rlibrary/rstdio.h>
#include <rlibrary/rstringstream.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
using namespace rtypes;
using namespace minecraft_controller;

// constants
static const char* const INIT_DIR = "/etc/minecontrold";
static const char* const LOG_FILE = "minecontrol.log"; // relative to current working directory of the server process
static const char* const DOMAIN_NAME = "@minecontrol";
static const char* const SERVICE_PORT = "44446";

// globals
static domain_socket local;
static network_socket remote;

// types
class minecraft_controller_error { };

// functions
static void daemonize(); // turns this process into a daemon
static void shutdown_handler(int); // recieves signals from system for server shutdown
static void sigpipe_handler(int); // recieves signals from system in the event that output fails on a network connection
static void create_server_sockets(); // creates server sockets
static void local_operation(); // accepts local connections; manages the thread that runs remote_operation
static void* remote_operation(void*); // started on a new thread by local_operation; accepts remote connections
static void fatal_error(const char* message); // exits the calling process after showing error message on STDERR

int main(int,const char*[])
{
    // set up signal handler for TERM and INT events
    if (::signal(SIGTERM,&shutdown_handler) == SIG_ERR)
        fatal_error("cannot create signal handler for SIGTERM");
    if (::signal(SIGINT,&shutdown_handler) == SIG_ERR)
        fatal_error("cannot create signal handler for SIGINT");
    if (::signal(SIGPIPE,&sigpipe_handler) == SIG_ERR)
        fatal_error("cannot create signal handler for SIGPIPE");
    // attempt to bind server sockets
    create_server_sockets();
    // become a daemon
    daemonize();
    // log process start
    minecontrold::standardLog << "process started" << endline;
    // perform startup operations
    minecraft_server_manager::startup_server_manager();
    // begin local server operation
    local_operation();
    // perform shutdown operations
    controller_client::shutdown_clients();
    minecraft_server_manager::shutdown_server_manager();
    // log process completion
    minecontrold::standardLog << "process complete" << endline;
    return 0;
}

void daemonize()
{
    // let's become a daemon
    pid_t pid = ::fork();
    if (pid == -1) {
        minecontrold::standardLog << "fatal: cannot fork a child process" << endline;
        ::_exit(1);
    }
    if (pid == 0) { // child process stays alive
        // become the leader of a new session and process group; this 
        // removes the controlling terminal from the process group
        if (::setsid() == -1)
            fatal_error("cannot become leader of new session");
        // reset umask
        ::umask(0);
        // attempt to create minecontrold init-dir if it does not already exist
        struct stat stbuf;
        if (stat(INIT_DIR,&stbuf) == -1) {
            if (errno != ENOENT)
                fatal_error("cannot access file information for initial directory");
            if (mkdir(INIT_DIR,S_IRWXU) == -1) {
                if (errno == EACCES)
                    fatal_error("cannot create initial directory: access denied");
                fatal_error("cannot create initial directory");
            }
        }
        else if ( !S_ISDIR(stbuf.st_mode) )
            fatal_error("initial directory exists as something other than a directory");
        // set working directory to minecontrold init-dir
        if (::chdir(INIT_DIR) == -1) {
            if (errno == EACCES)
                fatal_error("cannot change current directory to needed initial directory: access denied");
            fatal_error("cannot change current directory to needed initial directory");
        }
        int fd, fdNull;
        // duplicate:
        //  log file to STDOUT and STDERR
        //  null to STDIN
        fd = ::open(LOG_FILE,O_RDWR|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
        fdNull = ::open("/dev/null",O_RDWR);
        if (fd == -1) {
            if (errno == EACCES)
                fatal_error("cannot open log file: permission denied: this process must be privileged");
            fatal_error("cannot open log file");
        }
        if (fdNull == -1)
            fatal_error("cannot open null device");
        if (::dup2(fdNull,STDIN_FILENO)!=STDIN_FILENO || ::dup2(fd,STDOUT_FILENO)!=STDOUT_FILENO || ::dup2(fd,STDERR_FILENO)!=STDERR_FILENO)
            fatal_error("cannot redirect standard IO to log file");
        ::close(fd);
        ::close(fdNull);
    }
    else {
        stdConsole << '[' << pid << "] " << minecontrold::get_server_name() << " started" << endline;
        ::_exit(0);
    }
}

void shutdown_handler(int sig)
{
    minecontrold::standardLog << "the server is going down; received " << ::strsignal(sig) << " signal" << endline;
    local.shutdown();
    remote.shutdown();
}

void sigpipe_handler(int)
{
    // ignore this signal
}

void create_server_sockets()
{
    domain_socket_address domainAddress(DOMAIN_NAME);
    network_socket_address networkAddress(NULL,SERVICE_PORT);
    // create domain socket
    if ( !local.open() )
        fatal_error("cannot create domain socket server");
    // create network socket
    if ( !remote.open() )
        fatal_error("cannot create network socket server");
    // attempt to bind the domain socket server
    if ( !local.bind(domainAddress) )
        fatal_error("cannot bind domain socket server to path");
    // attempt to bind the network socket server
    if ( !remote.bind(networkAddress) )
        fatal_error("cannot bind network socket server to address");
}

void local_operation()
{
    // start up another thread to accept remote connections
    void* retval;
    pthread_t threadID;
    if (pthread_create(&threadID,NULL,&remote_operation,NULL) != 0)
        fatal_error("cannot create thread to accept remote connections");
    // accept local connections to the domain socket server
    while (true)
        // accept a connection
        if (controller_client::accept_client(local) == NULL) // assume 'local' was shutdown
            break;
    // join back with the remote operation thread
    if (pthread_join(threadID,&retval) != 0)
        throw minecraft_controller_error();
}

void* remote_operation(void*)
{
    // accept remote conenctions to the network socket server
    while (true)
        // accept a connection
        if (controller_client::accept_client(remote) == NULL) // assume 'remote' was shutdown
            break;
    return NULL;
}

void fatal_error(const char* message)
{
    errConsole << minecontrold::get_server_name() << ": fatal: " << message << endline;
    _exit(1);
}

// minecraft_controller::minecraft_controller_log_stream
minecraft_controller_log_stream::minecraft_controller_log_stream()
{
}
void minecraft_controller_log_stream::_outDevice()
{
    char sbuffer[40];
    time_t tp;
    ::time(&tp);
    ::strftime(sbuffer,40,"%a %b %d %H:%M:%S",localtime(&tp));
    // send our local buffer to the standard console
    // add name and process id
    stdConsole << '[' << sbuffer << "] " << minecontrold::get_server_name()
               << '[' << ::getpid() << "]: ";
    stdConsole.place(*this);
    stdConsole.flush_output();
    _bufOut.clear();
}

// minecraft-controller::minecontrold
/*static*/ minecraft_controller_log_stream minecontrold::standardLog;
/*static*/ const char* minecontrold::SERVER_NAME = "minecontrold";
/*static*/ const char* minecontrold::SERVER_VERSION = "0.7 (beta test)";
void minecontrold::shutdown_minecontrold()
{
    minecontrold::standardLog << "the server is going down; an internal request was issued" << endline;
    local.shutdown();
    remote.shutdown();
}
void minecontrold::close_global_fds()
{
    // close the domain socket @minecontrol
    local.close();
    // close domain socket connections
    controller_client::close_client_sockets();
}
