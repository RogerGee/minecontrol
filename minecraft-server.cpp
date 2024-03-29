// minecraft-server.cpp
#include "minecraft-server.h"
#include "minecraft-controller.h"
#include <cstring>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <rlibrary/rutility.h>
#include <rlibrary/rfile.h>
using namespace rtypes;
using namespace minecraft_controller;

// Globals

extern char **environ;

// Files relative to current working directory (global files):
#define SERVER_INIT_FILE "minecontrol.init"

// Files relative to minecraft directory:
#define MINECONTROL_ERRORS_FILE "errors"
#define MINECONTROL_PROPERTIES_FILE "minecontrol.properties"

#define MINECONTROL_PROPERTIES_HEADER           \
    "# minecontrol.properties\n"                \
    "#\n"                                               \
    "# This file stores per-server minecontrol properties.\n"

#define MINECONTROL_PROPERTIES_PROFILE_HEADER                           \
    "# profile\n"                                                       \
    "#\n"                                                               \
    "# Stores the server profile used to execute the Minecraft server. This maps to a\n" \
    "# profile defined in the global \"minecontrol.init\" file. Warning - changing this\n" \
    "# property may result in undefined behavior that may break your server!\n"

// minecraft_controller::minecraft_server_info

/* static */ const char* const minecraft_server_info::MINECRAFT_USER_DIRECTORY = "minecraft";

minecraft_server_info::minecraft_server_info(bool createNew,const char* serverName,const user_info& userInformation)
    : isNew(createNew), internalName(serverName), userInfo(userInformation)
{
    // set extended property defaults
    serverTime = uint64(-1);
    // read in server props if server is pre-existing; we must read the minecontrol init file to see if an alternate home exists
    minecraft_server_init_manager initInfo;
    initInfo.read_from_file();
    path serverDir;
    if (initInfo.alternate_home().length() > 0) {
        serverDir = initInfo.alternate_home();
        serverDir += userInfo.userName;
    }
    else
        serverDir = userInformation.homeDirectory;
    serverDir += MINECRAFT_USER_DIRECTORY;
    serverDir += serverName;
    if (!isNew && serverDir.exists()) {
        file_stream fstream( filename(serverDir,"server.properties").get_full_name().c_str() );
        if ( fstream.get_device().is_valid_context() )
            _properties.read(fstream);
    }
}
void minecraft_server_info::read_props(str& propertyList,rstream& errorStream)
{
    auto sanitize_values = [](str& key,str& value)
    {
        // replace any commas with spaces
        for (size_type i = 0;i<key.size();i++) {
            if (key[i] == ',') {
                key[i] = ' ';
            }
        }
        for (size_type i = 0;i<value.size();i++) {
            if (value[i] == ',') {
                value[i] = ' ';
            }
        }
    };

    stringstream inputStream(propertyList);
    inputStream.delimit_whitespace(false);
    inputStream.add_extra_delimiter("=\n");
    while ( inputStream.has_input() ) {
        str key, value;
        inputStream >> key >> value;

        // Check to see if the property is an extended property; we want to give
        // these preference. Otherwise see if it is a Minecraft property.
        _prop_process_flag flag = _process_ex_prop(key,value);
        if (flag==_prop_process_bad_key || flag==_prop_process_undefined) {
            flag = _process_prop(key,value);

            // error messages are destined for the client; use a newline to delimit
            if (flag == _prop_process_bad_key) {
                sanitize_values(key,value);
                errorStream << "Note: the specified option '" << key
                            << "' does not exist or cannot be used and was ignored."
                            << newline;
            }
        }
        if (flag == _prop_process_bad_value) {
            sanitize_values(key,value);
            errorStream << "Error: the specified option '" << key
                        << "' cannot be assigned the value '" << value << "'."
                        << newline;
        }
    }
}

bool minecraft_server_info::set_prop(const str& key,const str& value,bool applyIfDefault)
{
    // try the normal server.properties file property list then the extended list
    if (_process_ex_prop(key,value)!=_prop_process_success && _process_prop(key,value,applyIfDefault)!=_prop_process_success)
        return false;
    return true;
}

minecraft_server_info::_prop_process_flag
minecraft_server_info::_process_ex_prop(const str& key,const str& value)
{
    const_stringstream ss(value);

    // "servertime" OR "server-time" [uint64]: how long the mcraft server stays
    // alive in seconds
    if (key == "servertime" || key == "server-time") {
        if ( !(ss >> serverTime) )
            return _prop_process_bad_value;
        // assume server time was in hours; compute seconds
        serverTime *= 3600;
        return _prop_process_success;
    }

    // "serverexec" [string]: Program names separated by colons (:) to pass to
    // the minecontrol authority (by creating minecontrol.exec file)
    if (key == "serverexec") {
        serverExec = value;
        return _prop_process_success;
    }

    // "profile" [string]: The profile to use for the server.
    if (key == "profile") {
        profileName = value;
        return _prop_process_success;
    }

    return _prop_process_bad_key;
}

minecraft_server_info::_prop_process_flag
minecraft_server_info::_process_prop(const str& key,const str& value,bool applyIfDefault)
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

/*static*/ void minecraft_server_init_manager::list_profiles(rtypes::dynamic_array<rtypes::str>& out)
{
    minecraft_server_init_manager initInfo;

    initInfo.read_from_file();
    for (auto it = initInfo.profiles.begin();it != initInfo.profiles.end();++it) {
        out.push_back(it->first.c_str());
    }
}

minecraft_server_init_manager::minecraft_server_init_manager()
{
    _exec = "/usr/bin/java"; // java program default
    _shutdownCountdown = 30; // 30 seconds
    _maxSeconds = 3600*4; // 4 hours
    _maxServers = 0xffff; // allow unlimited (virtually)
}

char* minecraft_server_init_manager::arguments(const char* profileName)
{
    minecraft_server_profile* profile;

    try {
        profile = &profiles.at(profileName);
    } catch (std::out_of_range) {
        return nullptr;
    }

    return &profile->cmdline[0];
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
            if (key == "exec") {
                ssValue.getline(_exec);
            }
            else if (key == "default-profile") {
                ssValue.getline(defaultProfile);
            }
            else if (key == "profile") {
                str line;
                int state;
                size_t iter;
                minecraft_server_profile newProfile;

                // Parse profile name,
                ssValue.getline(line);
                iter = 0;
                while (iter < line.length() && line[iter] != ':') {
                    newProfile.profileName.push_back(line[iter]);
                    iter += 1;
                }

                // Parse command-line.
                iter += 1;
                state = 0;
                while (iter < line.length()) {
                    int ch = line[iter++];

                    if (state == 0) {
                        if (ch == '\\') {
                            continue;
                        }

                        if (ch == '"') {
                            state = 1;
                            continue;
                        }
                        if (ch == '\'') {
                            state = 2;
                            continue;
                        }

                        if (isspace(ch)) {
                            if (newProfile.cmdline.length() > 0) {
                                newProfile.cmdline.push_back(0);
                            }

                            while (iter < line.length() && line[iter] == ' ') {
                                iter += 1;
                            }

                            continue;
                        }
                    }
                    else if (state == 1) {
                        if (ch == '"') {
                            state = 0;
                            continue;
                        }
                    }
                    else if (state == 2) {
                        if (ch == '\'') {
                            state = 0;
                            continue;
                        }
                    }

                    newProfile.cmdline.push_back(ch);
                }
                newProfile.cmdline.push_back(0);
                newProfile.cmdline.push_back(0);

                profiles[newProfile.profileName.c_str()] = newProfile;
            }
            else if (key == "server-time") {
                ssValue >> _maxSeconds;
                _maxSeconds *= 3600; // assume value was in hours; make units in seconds
            }
            else if (key == "max-servers") {
                ssValue >> _maxServers;
            }
            else if (key == "shutdown-countdown") {
                ssValue >> _shutdownCountdown;
            }
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

/*static*/ void minecraft_server::list_servers(rtypes::dynamic_array<rtypes::str>& out,
    const user_info& userInfo)
{
    str mcraftdir;
    stringstream formatter;
    minecraft_server_init_manager initInfo;

    // Deduce the user's minecraft directory.
    initInfo.read_from_file();
    if (initInfo.alternate_home().length() > 0) {
        formatter << initInfo.alternate_home() << '/' << userInfo.userName;
    }
    else {
        formatter << userInfo.homeDirectory;
    }
    formatter << '/' << minecraft_server_info::MINECRAFT_USER_DIRECTORY;
    mcraftdir = static_cast<str&>(formatter.get_device());

    // Walk the user's minecraft directory to discover minecraft servers.
    DIR* dir = opendir(mcraftdir.c_str());

    if (dir != nullptr) {
        while (true) {
            struct stat st;
            struct dirent* result;
            result = readdir(dir);

            if (result == nullptr) {
                break;
            }

            if (result->d_type != DT_DIR) {
                continue;
            }

            // Verify that the folder contains a "world" subdirectory.
            stringstream dirPath(mcraftdir.c_str());
            dirPath << '/' << result->d_name << "/world";
            if (stat(dirPath.get_device().c_str(),&st) == -1) {
                continue;
            }
            if (S_ISDIR(st.st_mode)) {
                out.push_back(result->d_name);
            }
        }

        closedir(dir);
    }
}

/*static*/ set<uint32> minecraft_server::_idSet;
/*static*/ mutex minecraft_server::_idSetProtect;
/*static*/ short minecraft_server::_handlerRef = 0;
/*static*/ uint64 minecraft_server::_alarmTick = 0;
/*static*/ minecraft_server_init_manager minecraft_server::_initManager;

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
    //_threadID = 0;
    _processID = -1;
    _authority = NULL;
    _threadCondition = false;
    _threadExit = mcraft_noexit;
    _maxTime = 0;
    _elapsed = 0;
    _uid = -1;
    _gid = -1;
    _fderr = -1;
    _propsFileIsDirty = false;
    // reload global settings in case they have changed
    _initManager.read_from_file();
}

minecraft_server::~minecraft_server() noexcept(false)
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
    char* javaArgs;
    path mcraftdir;
    const str& altHome = _initManager.alternate_home();

    // Figure out the path to the minecraft server.
    if (altHome.length() > 0) {
        mcraftdir = altHome;
        mcraftdir += info.userInfo.userName;
    }
    else {
        mcraftdir = info.userInfo.homeDirectory;
    }

    // create needed directories; make sure to change ownership and permissions
    if (!mcraftdir.exists() && (!mcraftdir.make(false) || chown(mcraftdir.get_full_name().c_str(),info.userInfo.uid,info.userInfo.gid)==-1
            || chmod(mcraftdir.get_full_name().c_str(),S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP)==-1))
        return mcraft_start_server_filesystem_error;

    // compute the server working directory (as a subdirectory of the home
    // directory and the minecraft user directory)
    mcraftdir += minecraft_server_info::MINECRAFT_USER_DIRECTORY;
    if ( !mcraftdir.exists() ) {
        if (!mcraftdir.make(false) || chown(mcraftdir.get_full_name().c_str(),info.userInfo.uid,info.userInfo.gid)==-1
            || chmod(mcraftdir.get_full_name().c_str(),S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP)==-1)
            return mcraft_start_server_filesystem_error;
    }
    mcraftdir += info.internalName;

    // create the directory (and its parent directories where able) and change
    // its owner to the authenticated user
    if ( !mcraftdir.exists() ) {
        if (!info.isNew)
            return mcraft_start_server_does_not_exist;
        if (!mcraftdir.make(false) || chown(mcraftdir.get_full_name().c_str(),info.userInfo.uid,info.userInfo.gid)==-1
            || chmod(mcraftdir.get_full_name().c_str(),S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP)==-1)
            return mcraft_start_server_filesystem_error;
    }
    else if (info.isNew) {
        return mcraft_start_server_already_exists;
    }

    // Set up per-process attributes. We should set values here that are
    // required by helper functions or that serve as defaults before extended
    // options are applied.
    _serverDir = mcraftdir;
    _internalName = info.internalName;
    _maxTime = _initManager.server_time();

    // create or open existing error file; we'll send child process error output
    // here; we need the descriptor in the parent process so we'll have to
    // change its ownership to the authenticated user
    _fderr = open((mcraftdir.get_full_name() + "/" MINECONTROL_ERRORS_FILE).c_str(),O_WRONLY|O_CREAT|O_APPEND,0660);
    if (_fderr == -1 || fchown(_fderr,info.userInfo.uid,info.userInfo.gid) == -1) {
        return mcraft_start_server_filesystem_error;
    }
    _setup_error_file();

    // Read minecontrol.properties file.
    _read_minecontrol_properties_file();

    // Assign any extended properties in minecraft_info instance. These will
    // override any existing properties.
    _check_extended_options(info);

    // Select a profile. We choose the loaded profile over the default if it is
    // available. The loaded profile can either be specified in the extended
    // options or in the minecontrol properties file.
    if (_profileName.length() == 0) {
        _profileName = _initManager.default_profile();
        _propsFileIsDirty = true;
    }

    // Lookup server arguments from profile.
    javaArgs = _initManager.arguments(_profileName.c_str());
    if (javaArgs == nullptr) {
        if (_profileName.length() == 0) {
            return mcraft_start_server_no_default_profile;
        }

        return mcraft_start_server_bad_profile;
    }

    // create new pipe for communication; this needs to be done before the fork
    _iochannel.open();
    pid = ::fork();
    if (pid == 0) { // child
        // check server limit before proceeding
        if (_idSet.size()+1 > size_type(_initManager.max_servers()))
            _exit((int)mcraft_start_server_too_many_servers);

        // setup the environment for the minecraft server:
        // change process privilages
#ifdef __APPLE__
        if (::setgid(info.userInfo.gid) == -1 || ::setegid(info.userInfo.gid) == -1
            || ::setuid(info.userInfo.uid) == -1 || ::seteuid(info.userInfo.uid) == -1)
        {
            _exit((int)mcraft_start_server_permissions_fail);
        }
#else
        if (::setresgid(info.userInfo.gid,info.userInfo.gid,info.userInfo.gid)==-1 || ::setresuid(info.userInfo.uid,info.userInfo.uid,info.userInfo.uid)==-1)
            _exit((int)mcraft_start_server_permissions_fail);
#endif

        // change the process umask
        ::umask(S_IWGRP | S_IWOTH);

        // change working directory to the directory for minecraft server
        if (::chdir( mcraftdir.get_full_name().c_str() ) != 0)
            _exit((int)mcraft_start_server_filesystem_error);

        // create the server.properties file using the properties in info
        if ( !_create_server_properties_file(info) )
            _exit((int)mcraft_start_server_filesystem_error);

        // create the auth.txt file needed by a new server
        if (info.isNew && !_create_eula_txt_file())
            _exit((int)mcraft_start_server_filesystem_error);

        // check any extended server info options that depend on this process's state
        _check_extended_options_child(info);

        // Create the minecontrol properties file.
        _create_minecontrol_properties_file();

        // prepare execve arguments
        int top = 0;
        char* pstr = javaArgs;
        char* args[128];
        args[top++] = _initManager.exec();
        while (top+1 < 128) {
            args[top++] = pstr;
            while (*pstr) {
                ++pstr;
            }
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
        if (::execve(_initManager.exec(),args,environ) == -1)
            _exit((int)mcraft_start_server_process_fail);

        // (control does not exist in this program anymore)
    }
    else if (pid != -1) {
        _iochannel.close_open(); // close open ends of pipe

        // initialize the authority which will manage the minecraft server
        _authority = new minecontrol_authority(_iochannel,_fderr,mcraftdir.get_full_name(),info.userInfo);

        // close the input side for our copy of the io channel; the authority
        // will maintain the read end of the pipe
        _iochannel.close_input();

        // set per-process attributes
        _internalID = 1;
        _idSetProtect.lock();
        while (_idSet.contains(_internalID)) {
            ++_internalID;
        }
        _idSet.insert(_internalID);
        _idSetProtect.unlock();
        _processID = pid;
        _uid = info.userInfo.uid;
        _gid = info.userInfo.gid;
        _elapsed = 0;
        _threadCondition = true;

        // create io thread that will manage the basic status of the server process
        if (::pthread_create(&_threadID,NULL,&minecraft_server::_io_thread,this) != 0) {
            ::kill(pid,SIGKILL); // just in case
            throw minecraft_server_error();
        }
    }
    else {
        throw minecraft_server_error();
    }

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
        _close_error_file();
        //_threadID = -1;
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
    _initManager.apply_properties(info);
    // write props to properties file
    info.get_props().write(fstream);
    return true;
}

bool minecraft_server::_create_eula_txt_file()
{
    // we need to create the obnoxious eula.txt file so that the server
    // process will operate correctly
    file eulatxt("eula.txt",file_create_exclusively);
    if ( !eulatxt.is_valid_context() ) // couldn't open file
        return false;
    eulatxt.write("eula=true\n");
    return true;
}

void minecraft_server::_check_extended_options(const minecraft_server_info& info)
{
    if (info.serverTime != uint64(-1)) {
        _maxTime = info.serverTime;
    }

    if (info.profileName.length() > 0) {
        _profileName = info.profileName;
        _propsFileIsDirty = true;
    }
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

void minecraft_server::_setup_error_file()
{
    time_t t;
    char buf[40];
    tm tmval;
    stringstream ss("BEGIN error log ");
    time(&t);
    strftime(buf,40,"%a %b %d %Y %H:%M:%S",localtime_r(&t,&tmval));
    ss << '[' << buf << "] ";
    ss << _internalName;
    for (size_type i = ss.get_device().length();i<80;++i)
        ss.put('-');
    ss << newline;
    ssize_t r = write(_fderr,ss.get_device().c_str(),ss.get_device().length());
    if (r == -1) {
        minecontrold::standardLog << "error writing to server log file (fd="
                                  << _fderr << "): " << strerror(errno) << endline;
    }
}

void minecraft_server::_close_error_file()
{
    if (_fderr != -1) {
        str s;
        str part = _internalName;
        char buf[40];
        time_t t;
        tm tmval;
        time(&t);
        strftime(buf,40,"%a %b %d %Y %H:%M:%S",localtime_r(&t,&tmval));
        part += " [";
        part += buf;
        part += "] error log END";
        if (part.length() < 80) {
            size_type len = 80 - part.length();
            for (size_type i = 0;i<len;++i)
                s.push_back('-');
        }
        s += part;
        s.push_back('\n');
        ssize_t r = write(_fderr,s.c_str(),s.length());
        if (r == -1) {
            minecontrold::standardLog << "error writing to server log file (fd="
                                      << _fderr << "): " << strerror(errno) << endline;
        }
        close(_fderr);
        _fderr = -1;
    }
}

bool minecraft_server::_read_minecontrol_properties_file()
{
    file propsFile;
    str fileName = _serverDir.get_full_name() + "/" MINECONTROL_PROPERTIES_FILE;

    if (propsFile.open_input(fileName.c_str(),file_open_existing)) {
        stringstream ss;
        file_stream stream(propsFile);

        ss.add_extra_delimiter('=');

        while (stream.has_input()) {
            str key;

            ss.reset_input_iter();
            stream.getline(ss.get_device());

            ss >> key;
            rutil_to_lower_ref(key);
            if (key.length() == 0 || key[0] == '#') {
                continue;
            }

            if (key == "profile") {
                ss.getline(_profileName);
            }
        }
    }

    return true;
}

bool minecraft_server::_create_minecontrol_properties_file()
{
    // NOTE: This member function assumes the PWD is set to the minecraft server
    // directory.

    if (!_propsFileIsDirty) {
        return true;
    }

    file propsFile(MINECONTROL_PROPERTIES_FILE,file_create_always);
    if (!propsFile.is_valid_context()) {
        return false;
    }

    file_stream fstream(propsFile);
    fstream << MINECONTROL_PROPERTIES_HEADER << newline;
    fstream << MINECONTROL_PROPERTIES_PROFILE_HEADER
            << "profile=" << _profileName << newline;

    return true;
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
    else if (condition == minecraft_server::mcraft_start_server_bad_profile) {
        stream << "the specified profile does not exist";
    }
    else if (condition == minecraft_server::mcraft_start_server_no_default_profile) {
        stream << "no profile found: the default profile is not configured";
    }
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
/*static*/ pthread_t minecraft_server_manager::_threadID;
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
            dynamic_array<int32> authProcessPIDs;
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
            if (_handles[i]->pserver->_authority != NULL)
                _handles[i]->pserver->_authority->get_auth_processes(authProcessPIDs);
            stream << _handles[i]->pserver->_internalName << ": id=" << _handles[i]->pserver->_internalID
                   << " pid=" << _handles[i]->pserver->_processID;
            if (authProcessPIDs.size() > 0) {
                stream << '[';
                stream << authProcessPIDs[0];
                for (size_type ind = 1;ind < authProcessPIDs.size();++ind)
                    stream << ',' << authProcessPIDs[ind];
                stream << ']';
            }
            stream << " user=" << (userInfo==NULL ? "unknown" : userInfo->pw_name)
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
            if ( _handles[i]->pserver->was_started() ) {
                uint32 id = _handles[i]->pserver->get_internal_id();
                minecontrold::standardLog << '{' << _handles[i]->_clientid << "} " << _handles[i]->pserver->end()
                                          << " {" << id << '}' << endline;
            }
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
                if ( _handles[i]->pserver->was_started() ) {
                    uint32 id = _handles[i]->pserver->get_internal_id();
                    minecontrold::standardLog << '{' << _handles[i]->_clientid << "} " << _handles[i]->pserver->end() << " {" << id
                                              << '}' << endline;
                }
                delete _handles[i]->pserver;
                _handles[i]->pserver = NULL;
            }
        }
        _mutex.unlock();
        ::sleep(1);
    }
    return NULL;
}
