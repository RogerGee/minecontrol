// minecraft-server.cpp
#include "minecraft-server.h"
#include <cstring>
#include <unistd.h>
#include <fcntl.h> // for now
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pwd.h>
#include <errno.h>
#include "rlibrary/rfilename.h"
#include "minecraft-controller.h"
using namespace rtypes;
using namespace minecraft_controller;

extern char **environ;

// minecraft_controller::minecraft_server_info

minecraft_server_info::minecraft_server_info()
{
    isNew = false;
    homeDirectory = "";
    uid = ::getuid();
    guid = ::getgid();
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
        _properties.booleanProps[i]->put(stream), stream << newline;
    for (size_type i = 0;i<_properties.numericProps.size();i++)
        _properties.numericProps[i]->put(stream), stream << newline;
    for (size_type i = 0;i<_properties.stringProps.size();i++)
        _properties.stringProps[i]->put(stream), stream << newline;
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
    _program = "/home/roger/code/projects/minecraft-controller/test/test"/*"/usr/bin/java"*/; // (test) // java program default
    //-Xmx1024M -Xms1024M -jar /usr/jar/minecraft_server.jar nogui
    _argumentsBuffer += "-Xmx1024M"; // (test) default for minecraft
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "-Xms1024M";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "-jar";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "/usr/jar/minecraft_server.jar";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "nogui";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer.push_back(0);
    _shutdownCountdown = 30; // (test) @ 30 seconds
    _maxSeconds = 3600; // (test) 1 hour
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
        // change process privilages
        standardLog << ::geteuid() << endline;
        if (::setresuid(info.uid,info.uid,info.uid)==-1 /*|| ::setgid(info.guid)==-1*/)
            _exit((int)mcraft_start_server_permissions_fail);
        // change working directory to directory for minecraft server
        path p(info.homeDirectory);
        p += "minecraft";
        if ( !p.exists() && !p.make() )
            _exit((int)mcraft_start_server_filesystem_error);
        p += info.internalName;
        if ( !p.exists() )
        {
            if (!info.isNew)
                _exit((int)mcraft_start_server_does_not_exist);
            if ( !p.make() )
                _exit((int)mcraft_start_server_filesystem_error);
        }
        else if (info.isNew)
            _exit((int)mcraft_start_server_already_exists);
        if (::chdir( p.get_full_name().c_str() ) != 0)
            _exit((int)mcraft_start_server_filesystem_error);
        // if creating a new server, attempt to create the server.properties
        // file
        if (info.isNew)
        {
            rstringstream stream;
            int fd = ::open("server.properties",O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
            if (fd == -1)
                _exit((int)mcraft_start_server_filesystem_error);
            info.put_props(stream);
            ::write(fd,stream.get_device().c_str(),stream.get_device().size());
            ::close(fd);
        }
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
    pipe_stream serverIO(pserver->_iochannel); // open up pipe stream
    qword lastTick = _alarmTick, timeVar;
    pserver->_threadExit = mcraft_noexit;
    while (pserver->_threadCondition)
    {
        if (pserver->_elapsed >= pserver->_maxSeconds)
        {
            // time's up! request shutdown from mcraft process
            serverIO << "say The time has expired. The server will soon shutdown.\nsay Going down in...5" << endline;
            for (int c = 4;c>=1;--c)
            {
                ::sleep(1);
                serverIO << "say " << c << endline;
            }
            ::sleep(1);
            serverIO << "say 0\nstop" << endline;
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
        if (pserver->_elapsed%3600 == 0)
        {
            timeVar = pserver->_elapsed / 3600;
            serverIO << "say Time status: " << timeVar << " hour" << (timeVar>1 ? "s " : " ")
                     << "remaining" << endline;
        }
        timeVar = (pserver->_maxSeconds - pserver->_elapsed);
        if (timeVar<3600 && timeVar%600==0)
        {
            timeVar /= 60;
            serverIO << "say Time status: " << timeVar << " minute" << (timeVar>1 ? "s " : " ")
                     << "remaining" << endline;
        }
        // process mcraft output

        // sleep thread and update elapsed information
        ::sleep(1);
        pserver->_elapsed += _alarmTick - lastTick;
        lastTick = _alarmTick;
    }
    // send the stop command if there's reason to 
    // believe that the server is still running
    if (pserver->_threadExit == mcraft_noexit)
    {
        serverIO << "say Attention: a request has been made to close the server.\nsay Going down in...5" << endline;
        for (int c = 4;c>=1;--c)
        {
            ::sleep(1);
            serverIO << "say " << c << endline;
        }
        ::sleep(1);
        serverIO << "say 0\nstop" << endline;
    }
    // let another context manage waiting on the process
    return &pserver->_threadExit;
}

rstream& minecraft_controller::operator <<(rstream& stream,minecraft_server::minecraft_server_start_condition condition)
{
    if (condition == minecraft_server::mcraft_start_success)
        return stream << "mcraft server started successfully";
    stream << "mcraft server start fail: ";
    if (condition == minecraft_server::mcraft_start_server_filesystem_error)
        stream << "the filesystem could not be set up correctly";
    else if (condition == minecraft_server::mcraft_start_server_permissions_fail)
        stream << "the minecraft server process could not take on the needed credentials";
    else if (condition == minecraft_server::mcraft_start_server_does_not_exist)
        stream << "the specified server internal name did not exist";
    else if (condition == minecraft_server::mcraft_start_server_already_exists)
        stream << "a new server could not be started because another server exists with the same name";
    else if (condition == minecraft_server::mcraft_start_server_process_fail)
        stream << "the child minecraft server process could not be executed";
    else
        stream << "an unknown error occurred"; 
    return stream;
}
rstream& minecraft_controller::operator <<(rstream& stream,minecraft_server::minecraft_server_exit_condition condition)
{
    if (condition == minecraft_server::mcraft_exit_ingame)
        stream << "mcraft server exit: an in-game event is supposed to have caused the exit";
    else if (condition == minecraft_server::mcraft_exit_authority_request)
        stream << "mcraft server exit: by authority: by request";
    else if (condition == minecraft_server::mcraft_exit_authority_killed)
        stream << "mcraft server exit: by authority: killed";
    else if (condition == minecraft_server::mcraft_exit_timeout_request)
        stream << "mcraft server exit: by timeout: by request";
    else if (condition == minecraft_server::mcraft_exit_timeout_killed)
        stream << "mcraft server exit: by timeout: killed";
    else if (condition == minecraft_server::mcraft_exit_request)
        stream << "mcraft server exit: by request";
    else if (condition == minecraft_server::mcraft_exit_killed)
        stream << "mcraft server exit: killed";
    else
        stream << "mcraft server exit: unknown condition";
    return stream;
}

// minecraft_controller::server_handle

server_handle::server_handle()
{
    pserver = NULL;
    _issued = true;
    _clientid = 0;
}

// minecraft_controller::minecraft_server_manager
/*static*/ mutex minecraft_server_manager::_mutex;
/*static*/ dynamic_array<server_handle*> minecraft_server_manager::_handles;
/*static*/ pthread_t minecraft_server_manager::_threadID = -1;
/*static*/ volatile bool minecraft_server_manager::_threadCondition = false;
/*static*/ server_handle* minecraft_server_manager::allocate_server()
{
    size_type index = 0;
    _mutex.lock();
    while (index<_handles.size() && _handles[index]->pserver!=NULL)
        ++index;
    if (index >= _handles.size())
        _handles.push_back(new server_handle);
    server_handle* handle = _handles[index];
    handle->pserver = new minecraft_server;
    handle->_issued = true;
    _mutex.unlock();
    return handle;
}
/*static*/ void minecraft_server_manager::attach_server(server_handle* handle)
{
    _mutex.lock();
    handle->_issued = false;
    _mutex.unlock();
}
/*static*/ void minecraft_server_manager::attach_server(server_handle** handles,size_type count)
{
    _mutex.lock();
    for (size_type i = 0;i<count;i++)
        handles[i]->_issued = false;
    _mutex.unlock();
}
/*static*/ bool minecraft_server_manager::lookup_auth_servers(int uid,int guid,dynamic_array<server_handle*>& outList)
{
    _mutex.lock();
    for (size_type i = 0;i<_handles.size();i++)
    {
        if (_handles[i]->pserver!=NULL && (_handles[i]->pserver->_uid==uid || _handles[i]->pserver->_guid==guid))
        {
            _handles[i]->_issued = true;
            outList.push_back(_handles[i]);
        }
    }       
    _mutex.unlock();
    return outList.size() > 0;
}
/*static*/ bool minecraft_server_manager::print_servers(rstream& stream)
{
    _mutex.lock();
    for (size_type i = 0;i<_handles.size();i++)
    {
        if (_handles[i]->pserver != NULL)
        {
            int hoursLeft, hoursTotal,
                minutesLeft, minutesTotal,
                secondsLeft, secondsTotal;
            passwd* userInfo = ::getpwuid(_handles[i]->pserver->_uid);
            if ( _handles[i]->pserver->is_running() )
            {
                // calculate time running and time total
                int var = int(_handles[i]->pserver->_maxSeconds);
                hoursTotal = var / 3600;
                var = var % 3600;
                minutesTotal = var / 60;
                secondsTotal = var % 60;
                var = _handles[i]->pserver->_elapsed;
                hoursLeft = var / 3600;
                var = var % 3600;
                minutesLeft = var / 60;
                secondsLeft = var % 60;                
            }
            else
                hoursLeft = -1;
            stream << "minecraft-server: pid=" << _handles[i]->pserver->_processID 
                   << ": user=" << (userInfo==NULL ? "unknown" : userInfo->pw_name) 
                   << ": time=";
            if (hoursLeft == -1)
                stream << "not-running";
            else
            {
                stream.fill('0');
                stream << setw(2) << hoursLeft << ':' << minutesLeft << ':' << secondsLeft
                       << '/' << hoursTotal << ':' << minutesTotal << ':' << secondsTotal << setw(0);
                stream.fill(' ');
            }
            stream << newline;
        }
    }
    _mutex.unlock();
    return _handles.size() > 0;
}
/*static*/ void minecraft_server_manager::startup_server_manager()
{
    // set up manager thread
    _threadCondition = true;
    if (::pthread_create(&_threadID,NULL,&minecraft_server_manager::_manager_thread,NULL) != 0)
        throw minecraft_server_manager_error();
}
/*static*/ void minecraft_server_manager::shutdown_server_manager()
{
    // wait for the thread to quit
    _threadCondition = false;
    if (::pthread_join(_threadID,NULL) != 0)
        throw minecraft_server_manager_error();
    _mutex.lock();
    for (size_type i = 0;i<_handles.size();i++)
    {
        if (_handles[i]->pserver != NULL)
        {
            if ( _handles[i]->pserver->was_started() )
                standardLog << _handles[i]->pserver->end() << endline;
            delete _handles[i]->pserver;
            _handles[i]->pserver = NULL;
        }
        delete _handles[i];
    }
    _handles.clear();
    _mutex.unlock();
}
/*static*/ void* minecraft_server_manager::_manager_thread(void*)
{
    // check the run condition of each server; if the server hasn't
    // been issued and has stopped running, destroy it
    while (_threadCondition)
    {
        _mutex.lock();
        for (size_type i = 0;i<_handles.size();i++)
        {
            if (_handles[i]->pserver!=NULL && !_handles[i]->_issued && !_handles[i]->pserver->is_running())
            {
                // the server quit (most likely from a timeout or in-game event)
                // call its destructor (cleanly ends the server) and free the memory
                if ( _handles[i]->pserver->was_started() )
                    standardLog << _handles[i]->pserver->end() << endline;
                delete _handles[i]->pserver;
                _handles[i]->pserver = NULL;
            }
        }
        _mutex.unlock();
        ::sleep(1);
    }
    return NULL;
}
