// minecraft-server.cpp
#include "minecraft-server.h"
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
#include "rlibrary/rfilename.h"
using namespace rtypes;
using namespace minecraft_controller;

extern char **environ;

// minecraft_controller::minecraft_server_info

minecraft_server_info::minecraft_server_info()
{
    isNew = false;
    uid = -1;
    guid = -1;
}
void minecraft_server_info::read_props(rstream&,rstream&)
{
    // do nothing
}
void minecraft_server_info::put_props(rstream&) const
{
    // do nothing
}

// minecraft_controller::minecraft_server_info_ex

void minecraft_server_info_ex::read_props(rstream& stream,rstream& errorStream)
{
    str key;
    str value;
    stream.add_extra_delimiter("=-");
    stream.delimit_whitespace(false);
    while ( stream.has_input() )
    {
        stream >> key >> value;
        // strip leading/trailing whitespace off of strings
        _strip(key);
        _strip(value);
        // make key lower-case so that it can match
        // the keys of the defined properties; they 
        // are guaranteed to be lower-case
        for (size_type i = 0;i<key.length();i++)
            if (key[i]>='A' && key[i]<='Z')
                key[i] -= 'A', key[i] += 'a';
        // check the three different types of properties
        {
            boolean_prop* pprop = _properties.booleanProps.lookup(key.c_str());
            if (pprop != NULL)
            {
                if ( !pprop->set_value(value) )
                    errorStream << "error The property '" << key << "' cannot be assigned the value '" << value << "'.\n";
            }
            else
                errorStream << "error The specified server property '" << key << "' does not exist and was ignored.\n";
        }
        {
            numeric_prop* pprop = _properties.numericProps.lookup(key.c_str());
            if (pprop != NULL)
            {
                if ( !pprop->set_value(value) )
                    errorStream << "error The property '" << key << "' cannot be assigned the value '" << value << "'.\n";
            }
            else
                errorStream << "error The specified server property '" << key << "' does not exist and was ignored.\n";
        }
        {
            string_prop* pprop = _properties.stringProps.lookup(key.c_str());
            if (pprop != NULL)
            {
                if ( !pprop->set_value(value) )
                    errorStream << "error The property '" << key << "' cannot be assigned the value '" << value << "'.\n";
            }
            else
                errorStream << "error The specified server property '" << key << "' does not exist and was ignored.\n";
        }
    }
    stream.remove_extra_delimiter("-=");
    stream.delimit_whitespace(true);
}
void minecraft_server_info_ex::put_props(rstream& stream) const
{
    for (size_type i = 0;i<_properties.booleanProps.size();i++)
        _properties.booleanProps[i]->put(stream);
    for (size_type i = 0;i<_properties.numericProps.size();i++)
        _properties.numericProps[i]->put(stream);
    for (size_type i = 0;i<_properties.stringProps.size();i++)
        _properties.stringProps[i]->put(stream);
}
/*static*/ void minecraft_server_info_ex::_strip(str& item)
{
    str tmp;
    size_type i, j;
    if (item.length() > 0)
    {
        i = 0;
        while (i<item.length() && item[i]==' ')
            ++i;
        j = item.length();
        while (j>0 && item[j-1]==' ')
            --j;
        if (i > 0)
        {
            tmp.clear();
            for (;i<j;i++)
                tmp.push_back(item[i]);
            item = tmp;
        }
        else
            item.resize(j);
    }
}

// minecraft_controller::minecraft_server

/*static*/ short minecraft_server::_handlerRef = 0;
/*static*/ qword minecraft_server::_alarmTick = 0;
minecraft_server::minecraft_server()
{
    // set up shared handler and real timer if not already set up
    if (_handlerRef <= 0)
    {
        if (::signal(SIGALRM,&_alarm_handler) == SIG_ERR)
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
    // assign server object properties defaults
    _uid = -1;
    _guid = -1;
    // assign default per process attributes
    _threadID = -1;
    _processID = -1;
    _threadCondition = false;
    _threadExit = mcraft_noexit;
    _elapsed = 0;
    // load global attributes from file
    _program = "/home/roger/code/projects/minecraft-controller/test/test"; // (test)
    _argumentsBuffer += "one"; // (test)
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "two";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer.push_back(0);
    _shutdownCountdown = 30; // (test) @ 30 seconds
    _maxSeconds = 5; // (test)
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
minecraft_server::minecraft_server_start_condition minecraft_server::begin(const minecraft_server_info& info)
{
    _iochannel.open(); // create new pipe for communication
    pid_t pid = ::fork();
    if (pid == 0) // child
    {
        // setup the environment for the minecraft server:
        // change working directory to directory for minecraft server
        path p(info.homeDirectory);
        p += "minecraft";
        if ( !p.exists() )
            if ( !p.make() )
                _exit((int)mcraft_start_server_filesystem_error);

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
            _exit((int)mcraft_start_server_process_fail);
        // (control does not exist in this program anymore)
    }
    else if (pid != -1)
    {
        _iochannel.close_open(); // close open ends of pipe
        // wait for the child process to put data on the pipe;
        // this will be our signal that the process started up properly
        char message[5];
        _iochannel.read(message,1); // just read one byte so as to not interfere too much with our future snooping
        if (_iochannel.get_last_operation_status()==no_input // child wasn't able to start properly
            || _iochannel.get_last_operation_status()==bad_read)
        {
            // close the pipe and read the child process
            int wstatus;
            _iochannel.close();
            if (::waitpid(pid,&wstatus,0) != pid) // this should happen sooner rather than never...
                throw minecraft_server_error();
            if ( WIFEXITED(wstatus) )
                return (minecraft_server_start_condition)WEXITSTATUS(wstatus);
            return mcraft_start_failure_unknown;
        }
        _processID = pid;
        _uid = info.uid;
        _guid = info.guid;
        _elapsed = 0;
        _threadCondition = true;
        // create io thread
        if (::pthread_create(&_threadID,NULL,&minecraft_server::_io_thread,this) != 0)
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
        _threadCondition = false;
        if (::pthread_join(_threadID,&retVal) != 0)
            throw minecraft_server_error();
        status = *reinterpret_cast<minecraft_server_exit_condition*>(retVal);
        // wait on the child process; it may have already been waited on
        // by another context in which case `waitpid' should fail with ECHILD
        int countdown = 30; // give 30 seconds for server shutdown
        while (true)
        {
            int wret;
            int wstatus;
            wret = ::waitpid(_processID,&wstatus,WNOHANG);
            if (wret == -1)
            {
                if (errno != ECHILD)
                    throw minecraft_server_error();
                else // process didn't exist; assume it already terminated
                    break;
            }
            if (wret > 0)
            {
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
                    else if (status == mcraft_exit_timeout_request)
                        status = mcraft_exit_timeout_killed; // provide clarification
                    break;
                }
            }
            // the client is still alive
            if (--countdown == 0) // it ran out of time to shut down
                if (::kill(_processID,SIGKILL)==-1 && errno!=ESRCH) // send sure kill signal
                    throw minecraft_server_error();
            ::sleep(1);
        }
        _iochannel.close();
        _processID = -1;
        _threadID = -1;
        _threadExit = mcraft_noexit;
    }
    return status;
}
/*static*/ void minecraft_server::_alarm_handler(int)
{
    ++_alarmTick;
}
/*static*/ void* minecraft_server::_io_thread(void* pobj)
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
            pserver->_threadExit = mcraft_exit_timeout_request;
            pserver->_threadCondition = false;
            break;
        }
        // check to see if the process is still alive
        int status;
        if (::waitpid(pserver->_processID,&status,WNOHANG) > 0)
        {
            if ( WIFEXITED(status) ) // assume the process was quit by an in-game operator
                pserver->_threadExit = mcraft_exit_ingame;
            else
                pserver->_threadExit = mcraft_exit_unknown;
            pserver->_threadCondition = false;
            break;
        }
        // display time messages at regular intervals

        // process mcraft output

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

// minecraft_controller::minecraft_server_manager

/*static*/ minecraft_server* minecraft_server_manager::allocate_server()
{
    return NULL;
}
/*static*/ void minecraft_server_manager::deallocate_server(minecraft_server*)
{
}
/*static*/ void minecraft_server_manager::shutdown_servers()
{
}
