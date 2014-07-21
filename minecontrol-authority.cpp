// minecontrol-authority.cpp
#include "minecontrol-authority.h"
#include "minecontrol-protocol.h"
#include "minecraft-controller.h"
#include <rlibrary/rfile.h>
#include <rlibrary/rstringstream.h>
#include <rlibrary/rutility.h>
#include <cstdio>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <limits.h>
#include <errno.h>
using namespace rtypes;
using namespace minecraft_controller;

/*static*/ minecraft_server_message* minecraft_server_message::generate_message(const str& serverLineText)
{
    /* the parallel arrays MESSAGE_GISTS and GIST_FORMATS define the search
       order for message patters (formats); they are arranged (hopefully) by
       most frequent message gist kinds first; if you need to add a gist kind,
       make sure to update both arrays */
    static const char* const GENERAL_FORMAT = "[%S] [%S]: %S";
    static const uint32 MESSAGE_GIST_COUNT = 13;
    static const minecraft_server_message_gist MESSAGE_GISTS[] = {
        gist_player_chat,
        gist_server_chat,
        gist_server_secret_chat,
        gist_player_teleported,
        gist_player_login,
        gist_player_id,
        gist_player_join,
        gist_player_leave,
        gist_player_losecon_logout,
        gist_player_achievement,
        gist_server_start,
        gist_server_start_bind,
        gist_server_shutdown,
        gist_player_losecon_error,
        gist_server_generic // base case; let this be the last element in this array
    };
    static const char* const GIST_FORMATS[] = {
        "<%S> %S", // gist_player_chat
        "[%S] %S", // gist_server_chat
        "You whisper to %S: %S", // gist_server_secret_chat
        "Teleported %S to %S,%S,%S", // gist_player_teleported
        "%S[/%S] logged in with entity id %S at (%S, %S, %S)", // gist_player_login
        "UUID of player %S is %S", // gist_player_id
        "%S joined the game", // gist_player_join
        "%S left the game", // gist_player_leave
        "%S lost connection: TextComponent{%S, %S, style=Style{%S, %S, %S, %S, %S, %S, %S, %S}}", // gist_player_losecon_logout
        "%S has just earned the achievement [%S]", // gist_player_achievement
        "Starting minecraft server version %S", // gist_server_start
        "Starting Minecraft server on %S", // gist_server_start_bind
        "Stopping server", // gist_server_shutdown
        "com.mojang.authlib.GameProfile@%S[id=%S,name=%S,properties=%S,legacy=%S] (/%S) lost connection: Disconnected", // gist_player_losecon_error
        "%S", // gist_server_generic (this is the base case; let this be the last element in this array)
    };
    dynamic_array<str> tokens;
    if (!_payloadParse(GENERAL_FORMAT,serverLineText.c_str(),tokens) || tokens.size()<3)
        return NULL;
    str time(tokens[0]), type(tokens[1]), payload(tokens[2]);
    uint32 i = 0;
    while (i < MESSAGE_GIST_COUNT) {
        tokens.clear();
        if ( _payloadParse(GIST_FORMATS[i],payload.c_str(),tokens) )
            return new minecraft_server_message(serverLineText,payload,tokens,GIST_FORMATS[i],time,type,MESSAGE_GISTS[i]);
        ++i;
    }
    return NULL;
}
minecraft_server_message::minecraft_server_message(const str& originalPayload,const str& payload,const dynamic_array<str>& tokens,const char* formatString,const str& timeMessage,const str& typeMessage,minecraft_server_message_gist messageGist)
    : _gist(messageGist), _originalPayload(originalPayload), _payload(payload), _tokens(tokens), _formatString(formatString)
{
    _good = true;
    if (sscanf(timeMessage.c_str(),"%u:%u:%u",&_hour,&_minute,&_second) != 3)
        _good = false;
    else if (typeMessage == "Server thread/INFO")
        _type = main_info;
    else if (typeMessage == "Server thread/WARN")
        _type = main_warn;
    else if ( rutil_strncmp(typeMessage.c_str(),"User Authenticator",18) ) {
        const char* p = typeMessage.c_str();
        while (*p && *p!='/')
            ++p;
        if (*p == '/') {
            ++p;
            if ( rutil_strcmp(p,"WARN") )
                _type = auth_warn;
            else if ( rutil_strcmp(p,"INFO") )
                _type = auth_info;
        }
    }
    else if (typeMessage == "Server Shutdown Thread/INFO")
        _type = shdw_info;
    else if (typeMessage == "Server Shutdown Thread/WARN")
        _type = shdw_warn;
    else
        _type = type_unkn;
}
str minecraft_server_message::get_type_string() const
{
    static const char* const DEFAULT = "unknown-msg";
    switch (_type) {
    case main_info:
        return "main-info";
    case main_warn:
        return "main-warning";
    case auth_info:
        return "auth-info";
    case auth_warn:
        return "auth-warning";
    case shdw_info:
        return "shutdown-info";
    case shdw_warn:
        return "shutdown-warning";
    default:
        return DEFAULT;
    }
    return DEFAULT;
}
str minecraft_server_message::get_gist_string() const
{
    // these strings determine what an authority program
    // sees as its first token on an input line
    static const char* const DEFAULT = "unknown";
    switch (_gist) {
    case gist_server_start:
        return "start";
    case gist_server_start_bind:
        return "bind";
    case gist_server_chat:
        return "server-chat";
    case gist_server_secret_chat:
        return "server-secret-chat";
    case gist_server_shutdown:
        return "shutdown";
    case gist_player_id:
        return "player-id";
    case gist_player_login:
        return "login";
    case gist_player_join:
        return "join";
    case gist_player_losecon_logout:
        return "logout-connection";
    case gist_player_losecon_error:
        return "lost-connection";
    case gist_player_leave:
        return "leave";
    case gist_player_chat:
        return "chat";
    case gist_player_achievement:
        return "achievement";
    case gist_player_teleported:
        return "player-teleported";
    default:
        return DEFAULT;
    }
    return DEFAULT;
}
/*static*/ bool minecraft_server_message::_payloadParse(const char* format,const char* source,dynamic_array<str>& tokens)
{
    str token;
    while (*format && *source) {
        if (*format == '%') {
            ++format;
            if (*format == 0)
                return false; // bad format string
            if (*format == '%') {
                // escaped percent sign character
                if (*source != *format)
                    return false;
            }
            else {
                token.clear();
                if (*format == 'S') {
                    // find token until delimiter character
                    char delim;
                    ++format;
                    delim = *format;
                    while (*source && *source!=delim) {
                        token.push_back(*source);
                        ++source;
                    }
                    if (*source != delim)
                        return false;
                }
                else if (*format == 's') {
                    // find token until whitespace
                    while (*source && *source!=' ' && *source!='\t' && *source!='\n') {
                        token.push_back(*source);
                        ++source;
                    }
                }
                else
                    throw minecontrol_authority_error(); // bad format character
                tokens.push_back(token);
            }
            // seek to next character if possible
            if (*format)
                ++format;
            else if (!*source)
                return true; // string matched
            if (*source)
                ++source;
            continue;
        }
        if (*format != *source)
            return false;
        ++format;
        ++source;
    }
    return *format == *source;
}

const char* const minecontrol_authority::AUTHORITY_EXE_PATH = "/usr/lib/minecontrol:/usr/local/lib/minecontrol"; // standard authority program location
const char* const minecontrol_authority::AUTHORITY_EXEC_FILE = "minecontrol.exec";
minecontrol_authority::minecontrol_authority(const pipe& ioChannel,int fderr,const str& serverDirectory,const user_info& userInfo)
    : _iochannel(ioChannel), _fderr(fderr), _serverDirectory(serverDirectory), _login(userInfo)
{
    _childCnt = 0;
    for (int i = 0;i < ALLOWED_CHILDREN;++i)
        _childID[i] = -1;
   _threadID = -1;
    _consoleEnabled = true;
    _threadCondition = true;
    // start default programs from _serverDirectory/AUTHORITY_EXEC_FILE
    file execFile;
    str execFileName = _serverDirectory;
    execFileName.push_back('/');
    execFileName += AUTHORITY_EXEC_FILE;
    if ( execFile.open_input(execFileName.c_str(),file_open_existing) ) {
        str line;
        file_stream fstream(execFile);
        while ( fstream.has_input() ) {
            fstream.getline(line);
            rutil_strip_whitespace_ref(line);
            // check for comments and blank lines
            if (line[0]=='#' || line.length()==0)
                continue;
            // assume 'line' is a command line
            int pid;
            auto result = run_auth_process(line,&pid);
            // log result message
            minecontrold::standardLog << result;
            if (pid != -1)
                minecontrold::standardLog << " (" << AUTHORITY_EXEC_FILE << ": " << "process=" << pid << ", cmd=" << line << ')';
            minecontrold::standardLog << endline;
        }
    }
    // start processing thread; this thread is joinable and will be joined with at destruction-time
    if (pthread_create(&_threadID,NULL,&minecontrol_authority::processing,this) != 0)
        throw minecontrol_authority_error();
}
minecontrol_authority::~minecontrol_authority()
{
    // join back up with the processing thread
    _threadCondition = false;
    if (int(_threadID)!=-1 && pthread_join(_threadID,NULL) != 0)
        throw minecontrol_authority_error();
}
minecontrol_authority::console_result minecontrol_authority::client_console_operation(socket& clientChannel)
{
    if (_iochannel.is_valid_context() && _consoleEnabled) {
        _clientMtx.lock();
        // register the client
        size_type i = 0;
        while (i<_clientchannels.size() && _clientchannels[i]!=NULL)
            ++i;
        if (i >= _clientchannels.size())
            _clientchannels.push_back(&clientChannel);
        else
            _clientchannels[i] = &clientChannel;
        // handle input from client using the minecontrol protocol; another thread will send messages;
        // we do not respond directly to these messages; the communication to the client is asynchronous
        minecontrol_message to, from;
        // send established status to client
        to.assign_command("CONSOLE-MESSAGE");
        to.add_field("Status","established");
        to.write_protocol_message(clientChannel);
        to.reset_fields();
        _clientMtx.unlock();
        while (_consoleEnabled) {
            from.read_protocol_message(clientChannel);
            if (!_consoleEnabled || !from.good() || from.is_command("console-quit"))
                break;
            if ( !from.is_command("console-command") ) {
                to.add_field("Status","error");
                to.add_field("Payload","Bad command sent to server in console mode");
                to.write_protocol_message(clientChannel);
                to.reset_fields();
            }
            else {
                // handle CONSOLE-COMMAND message
                str k, v;
                while (true) {
                    if (!(from.get_field_key_stream() >> k) || !(from.get_field_value_stream() >> v))
                        break;
                    if (k == "servercommand")
                        issue_command(v);
                }
            }
        }
        _clientMtx.lock();
        if (_clientchannels[i] != NULL) {
            // if a message was never received or if the last message received was good, then
            // we exit console mode gracefully by sending a shutdown console message
            if (from.good() || from.is_blank()/*just in case*/) {
                // send a console message with status shutdown
                to.assign_command("CONSOLE-MESSAGE");
                to.add_field("Status","shutdown");
                to.write_protocol_message(clientChannel);
            }
            // unregister the client
            _clientchannels[i] = NULL;
        }
        _clientMtx.unlock();
        return _consoleEnabled ? console_communication_finished : console_communication_terminated;
    }
    // use the minecontrol protocol to alert the client of the failed attempt
    minecontrol_message msg("CONSOLE-MESSAGE");
    msg.add_field("Status","failed");
    msg.write_protocol_message(clientChannel);
    return console_no_channel;
}
void minecontrol_authority::issue_command(const str& commandLine)
{
    /* ensure that the write operation is atomic by limiting the
       number of bytes written in any one call; subtract 1 to account
       for the endline needed by the minecraft server to parse the command line */
    char buffer[PIPE_BUF+1];
    size_type len = rutil_strncpy(buffer,commandLine.c_str(),PIPE_BUF-1);
    buffer[len] = '\n';
    buffer[len+1] = 0;
    _iochannel.write(buffer);
}
minecontrol_authority::execute_result minecontrol_authority::run_auth_process(str commandLine,int* ppid)
{
    static const int ARGV_BUF_SIZE = 512;
    pid_t pid;
    int index;
    if (ppid != NULL)
        *ppid = -1;
    // the entire operation needs to be atomic; if the lock is aquired after the 
    // processing thread shuts down, the _consoleEnabled flag should be false
    _childMtx.lock();
    // if this flag is false then the processing thread most certainly is winding down
    if (!_consoleEnabled)
        return authority_exec_not_ready;
    // see if there is a slot for a new child process
    index = 0;
    while (index<ALLOWED_CHILDREN && _childID[index]!=-1)
        ++index;
    if (index >= ALLOWED_CHILDREN) {
        _childMtx.unlock();
        return authority_exec_too_many_running;
    }
    // create pipe for child's stdin
    if ( !_childStdIn[index].open_output() ) {
        _childMtx.unlock();
        return authority_exec_cannot_run;
    }
    // fork process
    pid = fork();
    if (pid == -1) {
        _childMtx.unlock();
        return authority_exec_cannot_run;
    }
    if (pid == 0) {
        const char* program;
        const char* argv[ARGV_BUF_SIZE];
        // prepare arguments
        if ( !_prepareArgs(&commandLine[0],&program,argv,ARGV_BUF_SIZE) )
            _exit((int)authority_exec_too_many_arguments);
        // change permissions for this process
        if (setresgid(_login.gid,_login.gid,_login.gid)==-1 || setresuid(_login.uid,_login.uid,_login.uid)==-1)
            _exit((int)authority_exec_attr_fail);
        // duplicate open pipe end as stdin
        _childStdIn[index].standard_duplicate();
        // duplicate Minecraft server process's stdin as stdout, then
        // close all open file descriptors except standard fds
        _iochannel.duplicate_output(STDOUT_FILENO);
        // duplicate _fderr as stderr for the process; this was provided by the caller
        // and should point to an log/error file for the authority program that it shares
        // with the Minecraft process
        if (dup2(_fderr,STDERR_FILENO) != STDERR_FILENO)
            _exit((int)authority_exec_cannot_run);
        int maxfd;
        maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd == -1)
            maxfd = 1000;
        for (int fd = 3;fd < maxfd;++fd)
            ::close(fd);
        // modify path to point to minecontrol authority exe locations
        str path = AUTHORITY_EXE_PATH;
        path.push_back(':');
        path += _serverDirectory;
        // add the previous directory as well (this should be the 'minecraft' directory where
        // minecontrol stores all the servers it creates); I include this so that a local user-created
        // authority program can be shared by different server directory structures
        path.push_back(':');
        path += _serverDirectory;
        path += "/..";
        if (setenv("PATH",path.c_str(),1) == -1)
            _exit((int)authority_exec_attr_fail);
        // set current directory for process to server directory
        if (chdir(_serverDirectory.c_str()) == -1)
            _exit((int)authority_exec_cannot_run);
        // attempt to execute program
        if (execvpe(program,(char* const*)argv,environ) == -1) {
            if (errno == ENOENT)
                _exit((int)authority_exec_program_not_found);
            if (errno == ENOEXEC)
                _exit((int)authority_exec_not_program);
            if (errno == EACCES)
                _exit((int)authority_exec_access_denied);
            _exit((int)authority_exec_unspecified);
        }
        // control no longer in this program
    }
    // close open end of pipe
    _childStdIn[index].close_open();
    /* wait a second for child process; if it exited see if the exit
       status was non-zero and report the appropriate error; else, 
       cache its process id and increment the child count; if the program
       is nice and short it may quit within the next second */
    sleep(1);
    int status;
    pid_t result = waitpid(pid,&status,WNOHANG);
    if (result == pid) {
        execute_result code = (execute_result)WEXITSTATUS(status);
        _childMtx.unlock();
        // close the pipe
        _childStdIn[index].close();
        if (code != authority_exec_okay)
            return code;
    }
    else {
        // we are good to go (the child process survived long enough to enter processing mode)
        ++_childCnt;
        _childID[index] = pid;
        _childMtx.unlock();
    }
    if (ppid != NULL)
        *ppid = pid;
    return result==pid ? authority_exec_okay_exited : authority_exec_okay;
}
bool minecontrol_authority::stop_auth_process(int32 pid)
{
    // this routine seeks to stop a service-oriented authority 
    // process by convention (sending EOF to the child process)
    // first; we give a timeout period for the child to quit before
    // sending the sure kill signal
    _childMtx.lock();
    for (int32 i = 0;i < ALLOWED_CHILDREN;++i) {
        if (_childID[i] == pid) {
            // cache the PID, close the pipe, and mark the child as non-existing;
            // then return control to other threads that may need to send messages
            _childStdIn[i].close();
            _childID[i] = -1;
            _childMtx.unlock();
            // let's give 10 seconds for the child to terminate
            int result;
            for (int sec = 1;sec <= 10;++sec) {
                int status;
                result = waitpid(pid,&status,WNOHANG);
                if (result == pid)
                    break;
                sleep(1);
            }
            // if the child didn't terminate, send the sure kill signal
            if (result != pid) {
                kill(pid,SIGKILL);
                waitpid(pid,NULL,0);
            }
            return true;
        }
    }
    _childMtx.unlock();
    return false;
}
void minecontrol_authority::get_auth_processes(dynamic_array<int32>& pidlist) const
{
    _childMtx.lock();
    for (int32 i = 0;i < ALLOWED_CHILDREN;++i) {
        if (_childID[i] != -1)
            pidlist.push_back(_childID[i]);
    }
    _childMtx.unlock();
}
bool minecontrol_authority::is_responsive() const
{
    // the processing thread is joinable, therefore try
    // to join with it; it would have terminated if the Minecontrol
    // process had become unresponsive
    if (int(_threadID) != -1) {
        void* junk;
        if (pthread_tryjoin_np(_threadID,&junk) == 0) {
            _threadID = -1;
            return false;
        }
        return true;
    }
    return false;
}
void* minecontrol_authority::processing(void* pparam)
{
    minecontrol_authority* object = reinterpret_cast<minecontrol_authority*>(pparam);
    static const size_type BUF_SIZE = 4096;
    pthread_t tid;
    char msgbuf[BUF_SIZE+1]; // add 1 for the null terminator
    char* pbuf = msgbuf;
    // handle message IO for authority
    while (object->_threadCondition) {
        size_type msglen = 0;
        size_type len = 0;
        // read off a message written by the Minecraft server process to its standard output
        while (len < BUF_SIZE) {
            object->_iochannel.read(pbuf,BUF_SIZE-len); // blocking call
            if (object->_iochannel.get_last_operation_status() != success_read)
                break;
            size_type last = object->_iochannel.get_last_byte_count(), i = 0;
            len += last;
            while (i < last) {
                ++msglen; // include endline in msglen count
                if (pbuf[i] == '\n')
                    break;
                ++i;
            }
            if (i < last) {
                pbuf[i] = 0;
                break;
            }
            if (len >= BUF_SIZE) { // if true, len == BUF_SIZE
                pbuf[i] = 0; // pbuf[i] is msgbuf[BUF_SIZE]
                break;
            }
            pbuf += last;
        }
        if (len==0 && object->_iochannel.get_last_operation_status()!=success_read) {
            /* the pipe is not responding; the server probably shutdown; if clients are 
               connected, send a console-message with status shutdown; the mutex lock 
               unsures that we don't conflict with another thread that might try to send
               the shutdown status */
            object->_clientMtx.lock();
            for (size_type i = 0;i < object->_clientchannels.size();++i) {
                if (object->_clientchannels[i] != NULL) {
                    minecontrol_message msg("CONSOLE-MESSAGE");
                    msg.add_field("Status","shutdown");
                    msg.write_protocol_message(*object->_clientchannels[i]);
                    object->_clientchannels[i] = NULL;
                }
            }
            object->_clientMtx.unlock();
            object->_threadCondition = false;
            break;
        }
        // if clients are registered with the authority, send the message as is
        object->_clientMtx.lock();
        for (size_type i = 0;i < object->_clientchannels.size();++i) {
            if (object->_clientchannels[i] != NULL) {
                // use the minecontrol protocol to send the server message
                minecontrol_message msg("CONSOLE-MESSAGE");
                msg.add_field("Status","message");
                msg.add_field("Payload",msgbuf);
                msg.write_protocol_message(*object->_clientchannels[i]);
            }
        }
        object->_clientMtx.unlock();
        // if there are any child programs running, send a parsed version of the
        // message to them on their stdin; also check the status of the running
        // process for termination
        object->_childMtx.lock();
        if (object->_childCnt > 0) {
            int status;
            minecraft_server_message* pmessage;
            pmessage = minecraft_server_message::generate_message(msgbuf);
            if (pmessage!=NULL && pmessage->good()) {
                /* prepare a simple message to send to any child programs; this message
                   is already parsed so that the client doesn't have to deal with too much
                   complexity with the Minecraft server output; each token in the message is
                   separated by whitespace */
                stringstream ss;
                ss << pmessage->get_gist_string();
                for (size_type i = 0;i < pmessage->get_token_count();++i)
                    ss << ' ' << pmessage->get_token(i);
                ss << newline;
                for (int i = 0;i < ALLOWED_CHILDREN;++i) {
                    if (object->_childID[i] != -1) {
                        if ( object->_childStdIn[i].is_valid_output() )
                            // send parsed message to child process (this pipe write may fail if the child isn't reading our messages)
                            object->_childStdIn[i].write(ss.get_device());
                        // wait poll any child process so that they can be reaped as soon as possible; I don't
                        // like the idea of a zombie apocalypse happening (at least at the moment)
                        if (waitpid(object->_childID[i],&status,WNOHANG) == object->_childID[i]) {
                            object->_childStdIn[i].close();
                            object->_childID[i] = -1;
                            --object->_childCnt;
                        }
                    }
                }
            }
            if (pmessage != NULL)
                delete pmessage;
        }
        object->_childMtx.unlock();
        // transfer any extra bytes to the beginning of the buffer (for simplicity and to allow the full capacity)
        pbuf = msgbuf;
        for (;msglen < len;++msglen) {
            *pbuf = msgbuf[msglen];
            ++pbuf;
        }
    }
    // disable clients from plugging in and also disable new executable programs
    // from being started by remote clients
    object->_consoleEnabled = false;
    object->_childMtx.lock();
    // wait for children to shutdown; give 10 seconds for termination else send kill signal
    for (int i = 0;i < ALLOWED_CHILDREN;++i) {
        if (object->_childID[i] != -1) {
            pid_t result;
            // close pipe to signal end of input
            object->_childStdIn[i].close();
            // give the process time to quit
            for (int sec = 1;sec <= 10;++sec) {
                int status;
                result = waitpid(object->_childID[i],&status,WNOHANG);
                if (result == object->_childID[i])
                    break;
                sleep(1);
            }
            // if the child didn't close in a reasonable amount of time, send sure kill
            if (result != object->_childID[i]) {
                kill(object->_childID[i],SIGKILL);
                waitpid(object->_childID[i],NULL,0);
            }
            object->_childID[i] = -1;
        }
    }
    object->_childMtx.unlock();
    return NULL;
}
/*static*/ bool minecontrol_authority::_prepareArgs(char* commandLine,const char** outProgram,const char** outArgv,int size)
{
    int top = 0;
    *outProgram = commandLine;
    while (*commandLine && top<size) {
        while (*commandLine && isspace(*commandLine))
            ++commandLine;
        outArgv[top++] = commandLine;
        while (*commandLine && !isspace(*commandLine))
            ++commandLine;
        if (*commandLine) {
            *commandLine = 0;
            ++commandLine;
        }
    }
    if (top < size) {
        outArgv[top] = NULL;
        return true;
    }
    return false;
}

rstream& minecraft_controller::operator <<(rstream& stream,minecontrol_authority::execute_result result)
{
    if (result == minecontrol_authority::authority_exec_okay)
        return stream << "Authority program started successfully";
    else if (result == minecontrol_authority::authority_exec_okay_exited)
        return stream << "Authority program started and completed successfully";
    stream << "Authority program execution failed: ";
    if (result == minecontrol_authority::authority_exec_cannot_run)
        stream << "the program couldn't be executed";
    else if (result == minecontrol_authority::authority_exec_access_denied)
        stream << "access is denied for the specified user";
    else if (result == minecontrol_authority::authority_exec_attr_fail)
        stream << "the process's attributes could not be correctly set";
    else if (result == minecontrol_authority::authority_exec_not_program)
        stream << "specified file wasn't an executable program";
    else if (result == minecontrol_authority::authority_exec_program_not_found)
        stream << "specified program was not found";
    else if (result == minecontrol_authority::authority_exec_too_many_arguments)
        stream << "too many arguments were supplied on the program's command-line";
    else if (result == minecontrol_authority::authority_exec_too_many_running)
        stream << "too many programs are running under the authority for this server";
    else if (result == minecontrol_authority::authority_exec_not_ready)
        stream << "the authority is not ready to execute programs at this time";
    else
        stream << "the reason was unspecified";
    return stream;
}
