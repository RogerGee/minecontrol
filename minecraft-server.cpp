// minecraft-server.cpp
#include "minecraft-server.h"
#include "minecraft-controller.h"
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pwd.h>
#include <errno.h>
#include <rlibrary/rfilename.h>
#include <rlibrary/rutility.h>
#include <rlibrary/rfile.h>
using namespace rtypes;
using namespace minecraft_controller;

// globals
extern char **environ;
static const char* const SERVER_INIT_FILE = "minecontrol.init"; // relative to current working directory (this is a global server init file)
static const char* const MINECRAFT_USER_DIRECTORY = "minecraft";

// minecraft_controller::minecraft_server_info

minecraft_server_info::minecraft_server_info(bool createNew,const char* serverName,const user_info& userInformation)
    : isNew(createNew), internalName(serverName), userInfo(userInformation)
{
    // set extended property defaults
    serverTime = uint64(-1);
    // read in server props if server is pre-existing
    path p(userInformation.homeDirectory);
    p += MINECRAFT_USER_DIRECTORY;
    p += serverName;
    if (!isNew && p.exists()) {
        file_stream fstream( filename(p,"server.properties").get_full_name().c_str() );
        if ( fstream.get_device().is_valid_context() )
            _properties.read(fstream);
    }
}
void minecraft_server_info::read_props(str& propertyList,rstream& errorStream)
{
    stringstream inputStream(propertyList);
    inputStream.delimit_whitespace(false);
    inputStream.add_extra_delimiter("=\n");
    while ( inputStream.has_input() ) {
        str key, value;
        inputStream >> key >> value;
        // check to see if the property belongs to
        // the server.properties file list; if not,
        // attempt the extended properties list
        _prop_process_flag flag = _process_prop(key,value);
        if (flag==_prop_process_bad_key || flag==_prop_process_undefined) {
            flag = _process_ex_prop(key,value);
            // replace any commas with spaces
            for (size_type i = 0;i<key.size();i++)
                if (key[i] == ',')
                    key[i] = ' ';
            for (size_type i = 0;i<value.size();i++)
                if (key[i] == ',')
                    key[i] = ' ';
            // error messages are destined for the client; use a newline to delimit
            if (flag == _prop_process_bad_key)
                errorStream << "Note: the specified option '" << key << "' does not exist or cannot be used and was ignored." << newline;
        }
        if (flag == _prop_process_bad_value)
            errorStream << "Error: the specified option '" << key << "' cannot be assigned the value '" << value << "'." << newline;
    }
}
bool minecraft_server_info::set_prop(const str& key,const str& value,bool applyIfDefault)
{
    // try the normal server.properties file property list then the extended list
    if (_process_ex_prop(key,value)!=_prop_process_success && _process_prop(key,value,applyIfDefault)!=_prop_process_success)
        return false;
    return true;
}
minecraft_server_info::_prop_process_flag minecraft_server_info::_process_ex_prop(const str& key,const str& value)
{
    const_stringstream ss(value);
    if (key == "servertime") { // uint64: how long the mcraft server stays alive in seconds
        if ( !(ss >> serverTime) )
            return _prop_process_bad_value;
        // assume server time was in hours; compute seconds
        serverTime *= 3600;
        return _prop_process_success;
    }
    else if (key == "serverexec") { // string: program names separated by colons (:) to pass to the minecontrol authority (by creating minecontrol.exec file)
        serverExec = value;
        return _prop_process_success;
    }
    return _prop_process_bad_key;
}
minecraft_server_info::_prop_process_flag minecraft_server_info::_process_prop(const str& key,const str& value,bool applyIfDefault)
{
    // check the three different types of properties
    {
        boolean_prop* pprop = _properties.booleanProps.lookup(key.c_str());
        if (pprop!=NULL && (!applyIfDefault || !pprop->is_user_supplied())) {
            if ( !pprop->set_value(value) )
                return _prop_process_bad_value;
            return _prop_process_success;
        }
    }
    {
        numeric_prop* pprop = _properties.numericProps.lookup(key.c_str());
        if (pprop!=NULL && (!applyIfDefault || !pprop->is_user_supplied())) {
            if ( !pprop->set_value(value) )
                return _prop_process_bad_value;
            return _prop_process_success;
        }
    }
    {
        string_prop* pprop = _properties.stringProps.lookup(key.c_str());
        if (pprop!=NULL && (!applyIfDefault || !pprop->is_user_supplied())) {
            if ( !pprop->set_value(value) )
                return _prop_process_bad_value;
            return _prop_process_success;
        }
    }
    return _prop_process_bad_key;
}

// minecraft_controller::minecraft_server_init_manager

minecraft_server_init_manager::minecraft_server_init_manager()
{
    _exec = "/usr/bin/java"; // java program default
    _argumentsBuffer += "-Xmx1024M"; // defaults for minecraft: -Xmx1024M -Xms1024M -jar minecraftserver.jar nogui
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "-Xms1024M";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "-jar";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "minecraft_server.jar";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer += "nogui";
    _argumentsBuffer.push_back(0);
    _argumentsBuffer.push_back(0);
    _shutdownCountdown = 30; // 30 seconds
    _maxSeconds = 3600*4; // 4 hours
    _maxServers = 0xffff; // allow unlimited (virtually)
}
void minecraft_server_init_manager::read_from_file()
{
    _mtx.lock();
    // load global attributes from file
    file initFile;
    if ( initFile.open_input(SERVER_INIT_FILE,file_open_existing) ) {
        file_stream fstream(initFile);
        stringstream ssValue;
        ssValue.add_extra_delimiter('=');
        while (fstream.has_input()) {
            str key;
            ssValue.reset_input_iter();
            fstream.getline( ssValue.get_device() );
            ssValue >> key;
            rutil_to_lower_ref(key);
            // test for each key
            if (key.length()==0 || key[0]=='#') // comment or blank line
                continue;
            if (key == "exec")
	        ssValue.getline(_exec);
            else if (key == "cmd-line") {
	        _argumentsBuffer.clear();
                while (ssValue >> key) {
                    _argumentsBuffer += key;
                    _argumentsBuffer.push_back(0);
                }
                _argumentsBuffer.push_back(0);
            }
            else if (key == "server-time") {
                ssValue >> _maxSeconds;
                _maxSeconds *= 3600; // assume value was in hours; make units in seconds
            }
            else if (key == "max-servers")
                ssValue >> _maxServers;
            else if (key == "shutdown-countdown")
                ssValue >> _shutdownCountdown;
            else if (key == "alt-home") {
                ssValue >> _altHome;
                // remove trailing slashes
                size_type i = _altHome.length();
                while (i>1 && _altHome[i-1]=='/')
                    --i;
                _altHome.truncate(i);
            }
            else {
                // handle default and override properties of the
                // form: override-<property-key> OR default-<property-key>
                str head;
                stringstream ss(key);
                // get "override" or "default" part; if it's anything else
                // just continue
                ss.delimit_whitespace(false);
                ss.add_extra_delimiter('-');
                ss >> head;
                ss.remove_extra_delimiter('-');
                if (head == "override") {
                    minecraft_server_input_property& prop = ++_overrideProperties;
                    ss >> head;
                    prop.key = head;
                    ssValue >> prop.value;
                }
                else if (head == "default") {
                    minecraft_server_input_property& prop = ++_defaultProperties;
                    ss >> head;
                    prop.key = head;
                    ssValue >> prop.value;
                }
            }
        }
    }
    else
        minecontrold::standardLog << "!!warning!!: no init file found or can't open it: using internal defaults" << endline;
    _mtx.unlock();
}
void minecraft_server_init_manager::apply_properties(minecraft_server_info& info)
{
    for (uint32 i = 0;i<_overrideProperties.size();i++)
        info.set_prop(_overrideProperties[i].key,_overrideProperties[i].value);
    for (uint32 i = 0;i<_defaultProperties.size();i++)
        info.set_prop(_defaultProperties[i].key,_defaultProperties[i].value,true);
}

// minecraft_controller::minecraft_server

/*static*/ set<uint32> minecraft_server::_idSet;
/*static*/ mutex minecraft_server::_idSetProtect;
/*static*/ short minecraft_server::_handlerRef = 0;
/*static*/ uint64 minecraft_server::_alarmTick = 0;
/*static*/ minecraft_server_init_manager minecraft_server::_globals;
minecraft_server::minecraft_server()
{
    // set up shared handler and real timer if not already set up
    if (++_handlerRef == 1) {
        if (::signal(SIGALRM,&_alarm_handler) == SIG_ERR)
            throw minecraft_server_error();
        itimerval tval;
        tval.it_interval.tv_sec = 1;
        tval.it_interval.tv_usec = 0;
        tval.it_value.tv_sec = 1;
        tval.it_value.tv_usec = 0;
        if (::setitimer(ITIMER_REAL,&tval,NULL) == -1)
            throw minecraft_server_error();
    }
    // initialize per process attributes
    _internalID = 0;
    _threadID = -1;
    _processID = -1;
    _authority = NULL;
    _threadCondition = false;
    _threadExit = mcraft_noexit;
    _maxTime = 0;
    _elapsed = 0;
    _uid = -1;
    _gid = -1;
    _fderr = -1;
    // reload global settings in case they have changed
    _globals.read_from_file();
}
minecraft_server::~minecraft_server()
{
    this->end();
    // unset the alarm system if the last server
    // object is being destroyed
    if (--_handlerRef <= 0) {
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
minecraft_server::minecraft_server_start_condition minecraft_server::begin(minecraft_server_info& info)
{
    pid_t pid;
    const str& altHome = _globals.alternate_home();
    path mcraftdir;
    if (altHome.length() > 0) {
        mcraftdir = altHome;
        mcraftdir += info.userInfo.userName;
    }
    else
        mcraftdir = info.userInfo.homeDirectory;
    // compute the server working directory (as a subdirectory of the home directory and the minecraft
    // user directory); create the directory (and its parent directories where able) and change its owner
    // to the authenticated user
    mcraftdir += MINECRAFT_USER_DIRECTORY;
    mcraftdir += info.internalName;
    if ( !mcraftdir.exists() ) {
        if (!info.isNew)
            return mcraft_start_server_does_not_exist;
        if (!mcraftdir.make(true) || chown(mcraftdir.get_full_name().c_str(),info.userInfo.uid,info.userInfo.gid)==-1) // create sub-directories if need be
            return mcraft_start_server_filesystem_error;
    }
    else if (info.isNew)
        return mcraft_start_server_already_exists;
    // create or open existing error file; we'll send child process error output here; we need the descriptor in
    // the parent process so we'll have to change its ownership to the authenticated user
    _fderr = open((mcraftdir.get_full_name()+"/errors").c_str(),O_WRONLY|O_CREAT|O_APPEND,0660);
    if (_fderr==-1 || fchown(_fderr,info.userInfo.uid,info.userInfo.gid)==-1)
        return mcraft_start_server_filesystem_error;
    _setup_error_file(info.internalName);
    // create new pipe for communication; this needs to be done before the fork
    _iochannel.open();
    pid = ::fork();
    if (pid == 0) { // child
        // check server limit before proceeding
        if (_idSet.size()+1 > size_type(_globals.max_servers()))
            _exit((int)mcraft_start_server_too_many_servers);
        // setup the environment for the minecraft server:
        // change process privilages
        if (::setresgid(info.userInfo.gid,info.userInfo.gid,info.userInfo.gid)==-1 || ::setresuid(info.userInfo.uid,info.userInfo.uid,info.userInfo.uid)==-1)
            _exit((int)mcraft_start_server_permissions_fail);
        // change the process umask
        ::umask(S_IWGRP | S_IWOTH);
        // change working directory to directory for minecraft server
        if (::chdir( mcraftdir.get_full_name().c_str() ) != 0)
            _exit((int)mcraft_start_server_filesystem_error);
        // create the server.properties file using the properties in info
        if ( !_create_server_properties_file(info) )
            _exit((int)mcraft_start_server_filesystem_error);
        // check any extended server info options that depend on this process's state
        _check_extended_options_child(info);
        // prepare execve arguments
        int top = 0;
        char* pstr = _globals.arguments();
        char* args[25];
        args[top++] = _globals.exec();
        while (true) {
            args[top++] = pstr;
            while (*pstr)
                ++pstr;
            if (*++pstr == 0)
                break;
        }
        args[top++] = NULL;
        // duplicate open ends of io channel pipe as standard IO
        // for this process; (this will close the descriptors)
        _iochannel.standard_duplicate(); // setup stdout and stdin
        duplicate_error_file(); // setup stderr
        // close all file descriptors not needed by the
        // child process; note that this is safe since
	// the fork closed any other threads running
	// servers off these file descriptors
        int maxfd;
        maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd == -1)
            maxfd = 1000;
        for (int fd = 3;fd < maxfd;++fd)
            ::close(fd);
        // execute program
        if (::execve(_globals.exec(),args,environ) == -1)
            _exit((int)mcraft_start_server_process_fail);
        // (control does not exist in this program anymore)
    }
    else if (pid != -1) {
        _iochannel.close_open(); // close open ends of pipe
        // wait for the child process to put data on the pipe;
        // this will be our signal that the process started up properly
        char message[5];
        _iochannel.read(message,1); // just read one byte so as to not interfere too much with our future snooping
        if (_iochannel.get_last_operation_status()==no_input // child wasn't able to start properly
            || _iochannel.get_last_operation_status()==bad_read) {
            // close the pipe, close the error file, and reap the child process
            int wstatus;
            _iochannel.close();
            _close_error_file(info.internalName);
            if (::waitpid(pid,&wstatus,0) != pid) // this should happen sooner rather than never...
                throw minecraft_server_error();
            if ( WIFEXITED(wstatus) )
                return (minecraft_server_start_condition)WEXITSTATUS(wstatus);
            return mcraft_start_failure_unknown;
        }
        // initialize the authority which will manage the minecraft server
        _authority = new minecontrol_authority(_iochannel,_fderr,mcraftdir.get_full_name(),info.userInfo);
        // close the input side for our copy of the io channel; the authority
        // will maintain the read end of the pipe
        _iochannel.close_input();
        // set per-process attributes
        _internalName = info.internalName;
        _internalID = 1;
        _idSetProtect.lock();
        while ( _idSet.contains(_internalID) )
            ++_internalID;
        _idSet.insert(_internalID);
        _idSetProtect.unlock();
        _processID = pid;
        _uid = info.userInfo.uid;
        _gid = info.userInfo.gid;
        _maxTime = _globals.server_time();
        _elapsed = 0;
        _threadCondition = true;
        // assign any extended properties in minecraft_info instance
        _check_extended_options(info);
        // create io thread that will manage the basic status of the server process
        if (::pthread_create(&_threadID,NULL,&minecraft_server::_io_thread,this) != 0) {
            ::kill(pid,SIGKILL); // just in case
            throw minecraft_server_error();
        }
    }
    else
        throw minecraft_server_error();
    return mcraft_start_success;
}
void minecraft_server::extend_time_limit(uint32 hoursMore)
{
    if (_processID != -1) {
        stringstream msg;
        uint64 secondsMore = hoursMore * 3600;
        uint64 remaining;
        _maxTime += secondsMore;
        // compile a message to inform players of the time limit change
        msg << "say An administrator has extended the time limit by " << hoursMore
            << (hoursMore>1 ? " hours" : " hour") << ". \nsay ";
        remaining = _maxTime - _elapsed;
        msg << "Time remaining: " << remaining/3600 << ':';
        remaining %= 3600;
        msg << remaining/60 << ':';
        remaining %= 60;
        msg << remaining << ".\n";
        _iochannel.write(msg.get_device());
    }
}
bool minecraft_server::duplicate_error_file() const
{
    return _fderr!=-1 && dup2(_fderr,STDERR_FILENO)==STDERR_FILENO;
}
minecraft_server::minecraft_server_exit_condition minecraft_server::end()
{
    minecraft_server_exit_condition status;
    status = mcraft_server_not_running;
    if (_processID != -1) {
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
        while (true) {
            int wret;
            int wstatus;
            wret = ::waitpid(_processID,&wstatus,WNOHANG);
            if (wret == -1) {
                if (errno != ECHILD)
                    throw minecraft_server_error();
                else // process didn't exist; assume it already terminated
                    break;
            }
            if (wret > 0) {
                if ( WIFEXITED(wstatus) ) { // process exited on its own
                    if (status == mcraft_noexit)
                        status = mcraft_exit_request; // due to the final request of the io thread
                    break;
                }
                else if ( WIFSIGNALED(wstatus) ) { // we killed the process
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
        // put every "per-server" attribute back in an invalid state
        _iochannel.close();
        _internalName.clear();
        _idSetProtect.lock();
        _idSet.remove(_internalID);
        _idSetProtect.unlock();
        _internalID = 0;
        _processID = -1;
        if (_authority != NULL) {
            delete _authority;
            _authority = NULL;
        }
        /* we must wait until the authority has finished using the error file descriptor */
        _close_error_file(_internalName);
        _threadID = -1;
        _threadExit = mcraft_noexit;
        _maxTime = 0;
        _elapsed = 0;
        _uid = -1;
        _gid = -1;
    }
    return status;
}
/*static*/ void minecraft_server::_alarm_handler(int)
{
    ++_alarmTick;
}
/*static*/ void* minecraft_server::_io_thread(void* pobj)
{
    // this thread is responsible for the basic input/output (including time management)
    // functionality of managing the server process; occasionally, this 
    // thread will write a message to the standard input of the server process;
    // I consider this operation to always be atomic since I hopefully will
    // never write more than the pipe buffer's size number of bytes (this way,
    // I don't interfere with the authority object's control of the io channel)
    minecraft_server* pserver = reinterpret_cast<minecraft_server*>(pobj);
    // open up a pipe stream for this thread's use; we'll use it to interrupt players with
    // annoying time-status alerts; this pipe is a shared resource which is also used by
    // the minecontrol authority running on a separate thread; pserver->_iochannel only
    // has a valid output resource handle
    pipe_stream serverIO(pserver->_iochannel);
    uint64 lastTick = _alarmTick, timeVar;
    // use pserver->_threadExit to store exit result
    pserver->_threadExit = mcraft_noexit;
    while (pserver->_threadCondition) {
        // check to see if the process is still alive
        int status;
        if (::waitpid(pserver->_processID,&status,WNOHANG) > 0) {
            if ( WIFEXITED(status) ) // assume the process was quit by an in-game operator
                pserver->_threadExit = mcraft_exit_ingame;
            else
                pserver->_threadExit = mcraft_exit_unknown; // this will flag some sort of error
            pserver->_threadCondition = false;
            break;
        }
        // perform elapsed time-dependent operations if this
        // server is to countdown time for the minecraft server
        // process; if maxTime is zero then time is unlimited
        if (pserver->_maxTime != 0) {
            if (pserver->_elapsed >= pserver->_maxTime) {
                // time's up! request shutdown from mcraft process
                serverIO << "say The time limit has expired. The server will soon shutdown.\nsay Going down in...5" << endline;
                for (int c = 4;c>=1;--c) {
                    ::sleep(1);
                    serverIO << "say " << c << endline;
                }
                ::sleep(1);
                serverIO << "say 0\nstop" << endline;
                pserver->_threadExit = mcraft_exit_timeout_request;
                pserver->_threadCondition = false;
                break;
            }
            // display time messages at regular intervals
            timeVar = (pserver->_maxTime - pserver->_elapsed);
            if (timeVar%3600 == 0) {
                // show hours remaining
                timeVar /= 3600;
                serverIO << "say Time status: " << timeVar << " hour" << (timeVar>1 ? "s " : " ")
                         << "remaining" << endline;
            }
            else if ((timeVar<3600 && timeVar%600==0) || (timeVar<600 && timeVar%60==0)) {
                // show minutes remaining on ten-minute intervals (< 1 hour remaining) OR minute intervals (< 10 minutes remaining)
                timeVar /= 60;
                serverIO << "say Time status: " << timeVar << " minute" << (timeVar>1 ? "s " : " ")
                         << "remaining" << endline;
            }
        }
        // sleep thread and update elapsed information
        ::sleep(1);
        pserver->_elapsed += _alarmTick - lastTick;
        lastTick = _alarmTick;
    }
    // send the stop command if there's reason to believe that the server is still running;
    // do not try to put output on the pipe if status is exit_unknown since an error may
    // have occurred and the server may not respond to data put on the pipe causing this thread to hang
    if (pserver->_threadExit==mcraft_noexit && pserver->_threadExit!=mcraft_exit_unknown) {
        serverIO << "say Attention: a request has been made to close the server.\nsay Going down in...5" << endline;
        for (int c = 4;c>=1;--c) {
            ::sleep(1);
            serverIO << "say " << c << endline;
        }
        ::sleep(1);
        serverIO << "say 0\nstop" << endline;
    }
    // let another context manage waiting on the process
    return &pserver->_threadExit;
}
bool minecraft_server::_create_server_properties_file(minecraft_server_info& info)
{
    file props("server.properties",file_create_always);
    if ( !props.is_valid_context() ) // couldn't open file
        return false;
    file_stream fstream(props);
    // apply default and/or override properties
    _globals.apply_properties(info);
    // write props to properties file
    info.get_props().write(fstream);
    return true;
}
void minecraft_server::_check_extended_options(const minecraft_server_info& info)
{
    if (info.serverTime != uint64(-1))
        _maxTime = info.serverTime;
}
void minecraft_server::_check_extended_options_child(const minecraft_server_info& info)
{
    if (info.serverExec.length() > 0) {
        file minecontrolExec(minecontrol_authority::AUTHORITY_EXEC_FILE,file_open_append);
        if ( minecontrolExec.is_valid_context() ) {
            str program;
            size_type i = 0;
            while (true) {
                if (i>=info.serverExec.length() || info.serverExec[i]==':') {
                    if (program.length() > 0) {
                        program.push_back('\n');
                        minecontrolExec.write(program);
                        program.clear();
                    }
                    ++i;
                    if (i >= info.serverExec.length())
                        break;
                    continue;
                }
                program.push_back(info.serverExec[i++]);
            }
        }
    }
}
void minecraft_server::_setup_error_file(const str& name)
{
    stringstream ss("BEGIN error log: ");
    ss << name;
    for (size_type i = ss.get_device().length();i<80;++i)
        ss.put('-');
    ss << newline;
    write(_fderr,ss.get_device().c_str(),ss.get_device().length());
}
void minecraft_server::_close_error_file(const str& name)
{
    if (_fderr != -1) {
        str s;
        str part = name;
        part += ": error log END";
        if (part.length() < 80) {
            size_type len = 80 - part.length();
            for (size_type i = 0;i<len;++i)
                s.push_back('-');
        }
        s += part;
        s.push_back('\n');
        write(_fderr,s.c_str(),s.length());
        close(_fderr);
        _fderr = -1;
    }
}

rstream& minecraft_controller::operator <<(rstream& stream,minecraft_server::minecraft_server_start_condition condition)
{
    // these messages could be sent to the user and therefore follow
    // the typical conventions for user messages
    if (condition == minecraft_server::mcraft_start_success)
        return stream << "Minecraft server started successfully";
    stream << "Minecraft server start fail: ";
    if (condition == minecraft_server::mcraft_start_server_filesystem_error)
        stream << "the filesystem could not be set up correctly; check the user home directory and its permissions";
    else if (condition == minecraft_server::mcraft_start_server_too_many_servers)
        stream << "too many servers are running: no more are allowed to be started at this time";
    else if (condition == minecraft_server::mcraft_start_server_permissions_fail)
        stream << "the minecraft server process could not take on the needed credentials";
    else if (condition == minecraft_server::mcraft_start_server_does_not_exist)
        stream << "the specified server did not exist in the expected location on disk";
    else if (condition == minecraft_server::mcraft_start_server_already_exists)
        stream << "a new server could not be started because another server exists with the same name";
    else if (condition == minecraft_server::mcraft_start_server_process_fail)
        stream << "the minecraft server process could not be executed";
    else if (condition == minecraft_server::mcraft_start_java_process_fail)
        stream << "the java process exited with an error code; check to make sure its command-line is correct";
    else
        stream << "an unspecified error occurred";
    return stream;
}
rstream& minecraft_controller::operator <<(rstream& stream,minecraft_server::minecraft_server_exit_condition condition)
{
    // these messages could be sent to the user and therefore follow
    // the typical conventions for user messages
    if (condition == minecraft_server::mcraft_exit_ingame)
        stream << "Minecraft server exit: an in-game event is supposed to have caused the shutdown";
    else if (condition == minecraft_server::mcraft_exit_authority_request)
        stream << "Minecraft server exit: an authoritative event caused the server to cleanly shutdown";
    else if (condition == minecraft_server::mcraft_exit_authority_killed)
        stream << "Minecraft server exit: an authoritative event caused the server process to be killed";
    else if (condition == minecraft_server::mcraft_exit_timeout_request)
        stream << "Minecraft server exit: the server timed-out and was cleanly shutdown";
    else if (condition == minecraft_server::mcraft_exit_timeout_killed)
        stream << "Minecraft server exit: the server timed-out and the server process was killed";
    else if (condition == minecraft_server::mcraft_exit_request)
        stream << "Minecraft server exit: the server was cleanly shutdown";
    else if (condition == minecraft_server::mcraft_exit_killed)
        stream << "Minecraft server exit: the server process was killed";
    else
        stream << "Minecraft server exit: the server process exited for an unspecified reason";
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
/*static*/ minecraft_server_manager::auth_lookup_result minecraft_server_manager::lookup_auth_servers(const user_info& login,dynamic_array<server_handle*>& outList)
{
    auth_lookup_result result = auth_lookup_none;
    _mutex.lock();
    for (size_type i = 0;i<_handles.size();i++) {
        if (_handles[i]->pserver!=NULL && !_handles[i]->_issued) {
            if (_handles[i]->pserver->_uid==login.uid || _handles[i]->pserver->_gid==login.gid || login.uid==0/*is root*/ || login.gid==0) {
                _handles[i]->_issued = true;
                outList.push_back(_handles[i]);
                result = auth_lookup_found;
            }
            else if (result == auth_lookup_none)
                result = auth_lookup_no_owned;
        }
    }
    _mutex.unlock();
    return result;
}
/*static*/ void minecraft_server_manager::print_servers(rstream& stream,const user_info* login)
{
    bool noneFound = true;
    _mutex.lock();
    for (size_type i = 0;i<_handles.size();i++) {
        if (_handles[i]->pserver!=NULL && _handles[i]->pserver->is_running()) {
            uint64 var;
            uint64 hoursElapsed, hoursTotal,
                minutesElapsed, minutesTotal,
                secondsElapsed, secondsTotal;
            passwd* userInfo = ::getpwuid(_handles[i]->pserver->_uid);
            // calculate time running and time total
            var = _handles[i]->pserver->_maxTime;
            hoursTotal = var / 3600;
            var %= 3600;
            minutesTotal = var / 60;
            secondsTotal = var % 60;
            var = _handles[i]->pserver->_elapsed;
            hoursElapsed = var / 3600;
            var %= 3600;
            minutesElapsed = var / 60;
            secondsElapsed = var % 60;
            stream << _handles[i]->pserver->_internalName << ": id=" << _handles[i]->pserver->_internalID
                   << " pid=" << _handles[i]->pserver->_processID 
                   << " user=" << (userInfo==NULL ? "unknown" : userInfo->pw_name)
                   << " time=";
            stream.fill('0');
            stream << setw(2) << hoursElapsed << ':' << minutesElapsed << ':' << secondsElapsed
                   << '/';
            if (_handles[i]->pserver->_maxTime > 0)
                stream << hoursTotal << ':' << minutesTotal << ':' << secondsTotal;
            else
                stream << "unlimited";
            stream << setw(0);
            stream.fill(' ');
            stream << newline;
            noneFound = false;
        }
    }
    // print out information for an authenticated user
    if (login != NULL) {
    }
    if (noneFound)
        stream << "No active servers running at this time\n";
    _mutex.unlock();
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
    for (size_type i = 0;i<_handles.size();i++) {
        if (_handles[i]->pserver != NULL) {
            if ( _handles[i]->pserver->was_started() )
                minecontrold::standardLog << '{' << _handles[i]->_clientid << "} " << _handles[i]->pserver->end() << endline;
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
    while (_threadCondition) {
        _mutex.lock();
        for (size_type i = 0;i<_handles.size();i++) {
            if (_handles[i]->pserver!=NULL && !_handles[i]->_issued && !_handles[i]->pserver->is_running()) {
                // the server quit (most likely from a timeout or in-game event)
                // call its destructor (cleanly ends the server) and free the memory
                // if the server was already started, call end such that we can record its
                // exit condition
                if ( _handles[i]->pserver->was_started() )
                    minecontrold::standardLog << '{' << _handles[i]->_clientid << "} " << _handles[i]->pserver->end() << endline;
                delete _handles[i]->pserver;
                _handles[i]->pserver = NULL;
            }
        }
        _mutex.unlock();
        ::sleep(1);
    }
    return NULL;
}
