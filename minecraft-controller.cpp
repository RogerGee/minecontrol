// minecraft-controller.cpp
#define _BSD_SOURCE // get `getpass'
#define _XOPEN_SOURCE // get `crypt'
#include "rlibrary/rstdio.h"
#include "rlibrary/rstringstream.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include "minecraft-server.h" // gets pthread.h
#include "domain-socket.h"
using namespace rtypes;
using namespace minecraft_controller;

// constants
static const char* const DOMAIN_NAME = "minecraft-control";
static const size_type MAX_CLIENTS = 512;

// globals
static const char* programName;
static bool globalShutdown = false;
static domain_socket local;
static domain_socket* clients[MAX_CLIENTS];

// types
class minecraft_controller_error { };

// functions
static inline void operation_log(const char*);
static inline void error_log(const char*);
static void daemonize(); // turns this process into a daemon
static void terminate_handler(int); // recieves SIGTERM event
static void* client_thread(void*); // accepts domain_socket*
//static void* server_thread(void*); // accepts minecraft_server_info*

int main(int,const char* argv[])
{
    // perform global setup
    programName = argv[0];
    // become a daemon
    daemonize();
    // set up signal handler for TERM event
    if (::signal(SIGTERM,&terminate_handler) == SIG_ERR)
    {
        error_log("cannot create signal handler for SIGTERM");
        _exit(1);
    }
    // create domain socket for client CLI application communication
    if ( !local.open(DOMAIN_NAME) )
    {
        error_log("cannot open domain socket for minecraft control");
        return 1;
    }
    // accept incoming connections
    for (size_type i = 0;i<MAX_CLIENTS;i++)
        clients[i] = NULL;
    while (true)
    {
        // accept a connection
        domain_socket* connection = new domain_socket;
        domain_socket::domain_socket_accept_condition cond = local.accept(*connection);
        if (cond == domain_socket::domain_socket_accepted)
        {
            operation_log("client connected to domain server");
            // see if there is room for the client and add it
            // to the list of maintained clients
            size_type i = 0;
            while (i<MAX_CLIENTS && clients[i]!=NULL)
                ++i;
            if (i >= MAX_CLIENTS)
            {
                connection->write("error Too many clients are connected to the server; try again later.");
                operation_log("could not add new client: too many clients are currently connected");
                delete connection;
                continue;
            }
            clients[i] = connection;
            // send the connection to a thread for processing; this 
            // thread should detach itself since I'm not going to join
            // with it later
            pthread_t threadID;
            if (::pthread_create(&threadID,NULL,&client_thread,connection) != 0)
            {
                connection->write("error The server encountered an error and couldn't process the request.");
                operation_log("cannot create thread for client processing");
                delete connection;
                continue;
            }
        }
        else
        {
            delete connection;
            break;
        }
    }
    operation_log("process complete");
}

void operation_log(const char* pmessage)
{
    stdConsole << programName 
               << '[' << ::getpid() << "]: " 
               << pmessage << endline;
}

void error_log(const char* pmessage)
{
    stdConsole << err;
    operation_log(pmessage);
    stdConsole << out;
}

void daemonize()
{
    // let's become a daemon
    pid_t pid = fork();
    if (pid == -1)
    {
        error_log("cannot fork a child process");
        _exit(1);
    }
    if (pid == 0) // child process stays alive
    {
        // ensure that the process will have no controlling terminal
        if (setsid() == -1)
        {
            error_log("cannot become leader of new session");
            _exit(1);
        }
        // reset umask
        umask(0);
        // set working directory to root
        //chdir("/");
        // try to close all file descriptors
        int fd, fdNull;
        // duplicate:
        //  log file to STDOUT and STDERR
        //  null to STDIN
        fd = open("log",O_RDWR|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
        fdNull = open("/dev/null",O_RDWR);
        if (fd == -1)
        {
            error_log("cannot open log file");
            _exit(1);
        }
        if (fdNull == -1)
        {
            error_log("cannot open null device");
            _exit(1);
        }
        if (dup2(fdNull,STDIN_FILENO)!=STDIN_FILENO || dup2(fd,STDOUT_FILENO)!=STDOUT_FILENO || dup2(fd,STDERR_FILENO)!=STDERR_FILENO)
        {
            error_log("cannot replace standard IO");
            _exit(1);
        }
        close(fd);
        close(fdNull);
    }
    else
        _exit(0);
}

void terminate_handler(int)
{
    operation_log("the server is going down; received TERM signal");
    globalShutdown = true;
    for (size_type i = 0;i<MAX_CLIENTS;i++)
    {
        if (clients[i] != NULL)
        {
            clients[i]->shutdown();
            clients[i]->close();
        }
    }
    local.shutdown();
    local.close();
}

void* client_thread(void* psocket)
{
    // detach this thread
    if (::pthread_detach( ::pthread_self() ) == -1)
        throw minecraft_controller_error();
    domain_socket* pclient = reinterpret_cast<domain_socket*>(psocket);
    domain_socket_stream stream(*pclient);
    while (!globalShutdown)
    {
        str line;
        stream.getline(line);
        if ( !stream.get_input_success() )
        {
            if (pclient->get_last_operation_status() == no_input) // client disconnect
            {
                operation_log("client disconnected");
                break;
            }
            else if (pclient->get_last_operation_status() == bad_read)
            {
                error_log("bad read from client socket; assuming disconnect");
                break;
            }
        }
        str s;
        rstringstream ss(line);
        ss >> s;
        for (size_type i = 0;i<s.length();i++) // make lowercase
            if (s[i]>='A' && s[i]<='Z')
                s[i] -= 'A', s[i] += 'a';
        if (s == "start")
        {
            
        }
        else if (s == "status")
        {
        }
    }
    // remove the client connection from the list of connected clients
    size_type i = 0;
    while (i<MAX_CLIENTS && clients[i]!=pclient)
        ++i;
    if (i < MAX_CLIENTS)
        clients[i] = NULL;
    // delete the memory used to hold psocket (do it on pclient so that the destructor is called)
    delete pclient;
    return NULL;
}

// void* server_thread(void* /*pinfo*/)
// {
//     return NULL;
// }
