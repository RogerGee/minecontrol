// minecraft-server.cpp
#include "minecraft-server.h"
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
using namespace rtypes;
using namespace minecraft_controller;

extern char **environ;

minecraft_server_info::minecraft_server_info()
{
    isNew = false;
}
void minecraft_server_info::read_props(rstream& stream)
{
}
void minecraft_server_info::put_props(rstream& stream) const
{
}

/*static*/ short minecraft_server::_handlerRef = 0;
/*static*/ qword minecraft_server::_alarmTick = 0;
minecraft_server::minecraft_server()
{
    // set up shared handler and real timer if not already set up
    if (_handlerRef <= 0)
    {
        if (::signal(SIGALRM,&_alarmHandler) == SIG_ERR)
            throw minecraft_server_error();
        itimerval tval;
        tval.it_interval.tv_sec = 1;
        tval.it_interval.tv_usec = 0;
        tval.it_value.tv_sec = 1;
        tval.it_value.tv_usec = 0;
        if (::setitimer(ITIMER_REAL,&tval,NULL) == -1)
            throw minecraft_server_error();
        ++_handlerRef;
    }
    // assign default per process attributes
    _threadID = -1;
    _processID = -1;
    _threadCondition = false;
    _threadExit = mcraft_noexit;
    _elapsed = 0;
    // load global attributes from file
    _program = "/home/roger/code/projects/minecraft-controller/test"; // (test)
    _argumentsBuffer += "one"; // (test)
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "two";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer.push_back(0);
    _shutdownCountdown = 30; // (test) @ 30 seconds
    _maxSeconds = 120; // (test) @ 2 minutes
}
minecraft_server::~minecraft_server()
{
    this->end();
    // unset the alarm system if the last server
    // object is being destroyed
    if (--_handlerRef <= 0)
    {
        // shutdown the signal handler
        if (::signal(SIGALRM,SIG_DFL) == SIG_ERR)
            throw minecraft_server_error();
        // shutdown the real time timer
        itimerval tval;
        ::memset(&tval,0,sizeof(itimerval));
        if (::setitimer(ITIMER_REAL,&tval,NULL) == -1)
            throw minecraft_server_error();
    }
}
minecraft_server::minecraft_server_start_condition minecraft_server::begin()
{
    pid_t pid = ::fork();
    _iochannel.open(); // create new pipe for communication
    if (pid == 0) // child
    {
        // prepare execve arguments
        int top = 0;
        char* pstr = &_argumentsBuffer[0];
        char* args[25];
        args[top++] = &_program[0];
        while (true)
        {
            args[top++] = pstr;
            while (*pstr)
                ++pstr;
            if (*++pstr == 0)
                break;
        }
        args[top++] = NULL;
        // duplicate pipe as standard IO
        _iochannel.standard_duplicate();
        // execute program
        if (::execve(_program.c_str(),args,environ) == -1)
            return mcraft_start_fail;
        // (control does not exist in this program anymore)
    }
    else if (pid != -1)
    {
        _iochannel.close_open(); // close open ends of pipe
        _processID = pid;
        _elapsed = 0;
        // create io thread
        if (::pthread_create(&_threadID,NULL,&minecraft_server::_ioThread,this) != 0)
        {
            ::kill(pid,SIGKILL); // just in case
            throw minecraft_server_error();
        }
    }
    else
        throw minecraft_server_error();
    return mcraft_start_success;
}
minecraft_server::minecraft_server_exit_condition minecraft_server::end()
{
    minecraft_server_exit_condition status;
    status = mcraft_server_not_running;
    if (_processID != -1)
    {
        void* retVal;
        // stop the io thread and wait for it to complete;
        // the IO thread should deliver the "stop" message
        _threadCondition = true;
        if (::pthread_join(_threadID,&retVal) != 0)
            throw minecraft_server_error();
        status = *reinterpret_cast<minecraft_server_exit_condition*>(retVal);
        int countdown = 30; // give 30 seconds for server shutdown
        while (true)
        {
            int wstatus;
            if (::waitpid(_processID,&wstatus,WNOHANG) == -1)
                throw minecraft_server_error();
            if ( WIFEXITED(wstatus) ) // process exited on its own
            {
                if (status == mcraft_noexit)
                    status = mcraft_exit_request; // due to the final request of the io thread
                break;
            }
            else if ( WIFSIGNALED(wstatus) ) // we killed the process
            {
                if (status == mcraft_noexit)
                    status = mcraft_exit_killed; // in this function
                break;
            }
            // the client is still alive
            if (--countdown == 0) // it ran out of time to shut down
                if (::kill(_processID,SIGKILL)==-1 && errno!=ESRCH) // send sure kill signal
                    throw minecraft_server_error();
            ::sleep(1);
        }
        _iochannel.close();
    }
    return status;
}
minecraft_server::minecraft_server_poll minecraft_server::poll(int times,int timeout) const
{
    minecraft_server_poll result = mcraft_poll_not_started;
    if (_processID != -1)
    {
        while (times > 0)
        {
            // send a null-signal to poll the child process
            if (::kill(_processID,0) == -1)
            {
                if (errno == ESRCH)
                    return mcraft_poll_terminated;
                throw minecraft_server_error();
            }
            ::sleep(timeout);
            --times;
        }
        return mcraft_poll_running;
    }
    return mcraft_poll_not_started;
}
/*static*/ void minecraft_server::_alarmHandler(int)
{
    ++_alarmTick;
}
/*static*/ void* minecraft_server::_ioThread(void* pobj)
{
    minecraft_server* pserver = reinterpret_cast<minecraft_server*>(pobj);
    pipe_stream pstream(pserver->_iochannel); // open up pipe stream
    qword lastTick = _alarmTick;
    pserver->_threadExit = mcraft_noexit;
    while (pserver->_threadCondition)
    {
        if (pserver->_elapsed >= pserver->_maxSeconds)
        {
            // time's up! request shutdown from mcraft process
            pstream << "say The time has expired. The server will soon shutdown." << endline;
            ::sleep(5); // let the message sink in
            pstream << "stop" << endline;
            // give it the default time to shutdown; if it's still running, then kill it
            if (pserver->poll(int(pserver->_shutdownCountdown),1)==mcraft_poll_running)
            {
                if (::kill(pserver->_processID,SIGKILL)==-1 && errno!=ESRCH)
                    throw minecraft_server_error();
                pserver->_threadExit = mcraft_exit_timeout_killed;
            }
            else
                pserver->_threadExit = mcraft_exit_timeout_request;
            break;
        }
        // check to see if the process is still alive
        if (pserver->poll() != mcraft_poll_running)
        {
            // assume the process was quit by an in-game operator
            pserver->_threadExit = mcraft_exit_ingame;
            break;
        }
        // display time messages at regular intervals

        // sleep thread and update elapsed information
        ::sleep(1);
        pserver->_elapsed += _alarmTick - lastTick;
        lastTick = _alarmTick;
    }
    // send the stop command if there's reason to 
    // believe that the server is still running
    if (pserver->_threadExit == mcraft_noexit)
        pstream << "stop" << endline;
    // let another context manage waiting on the process
    return &pserver->_threadExit;
}
