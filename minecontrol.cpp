// minecontrol.cpp - minecraft-controller client application
#include "minecontrol-protocol.h"
#include "domain-socket.h"
#include "net-socket.h"
#include "mutex.h"
#include <rlibrary/rlist.h>
#include <rlibrary/rstdio.h> // gets io_device
#include <rlibrary/rstringstream.h>
#include <rlibrary/rutility.h>
#include <functional>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <ncurses.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
using namespace rtypes;
using namespace minecraft_controller;

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define PACKAGE_VERSION "(unknown version)"
#endif

// constants (well, some are mutable but they function like constants)
static const char* DOMAIN_NAME = "@minecontrol";
static const char* SERVICE_PORT = "44446";
static const char* PROGRAM_NAME;
static const char* const CLIENT_NAME = "minecontrol-standard";
static const char* const PROGRAM_VERSION = PACKAGE_VERSION;
static const char PROMPT_MAIN = '#';
static const char PROMPT_CONSOLE = '$';

// session_state structure: stores connection information
struct session_state
{
    session_state();
    ~session_state();

    // id strings pertaining to connection
    str username;
    str serverName, serverVersion;

    // socket/address information
    socket* psocket;
    socket_address* paddress;

    // io enabled streams for connection and terminal
    socket_stream connectStream;
    stringstream inputStream;

    // minecontrol protocol messages
    minecontrol_message response;
    minecontrol_message_buffer request;

    // session alive control
    bool sessionControl;

    // mutex for cross-thread safety and control
    // variable for cross-thread sync
    mutex mtx;
    volatile bool control;
};
session_state::session_state()
{
    psocket = NULL;
    paddress = NULL;
    sessionControl = true;
    control = true;
}
session_state::~session_state()
{
    if (psocket != NULL)
        delete psocket;
    if (paddress != NULL)
        delete paddress;
}

// signal handlers
static void sigpipe_handler(int); // handle SIGPIPE

// helpers
static bool check_long_option(const char* option,int& exitCode);
static bool check_short_option(const char* option,const char* argument,int& exitCode);
static void term_echo(bool onState);
static bool check_status(io_device& device);
static bool request_response_sequence(session_state& session);
static bool print_response(minecontrol_message& response);
static void insert_field_expression(minecontrol_message_buffer& msgbuf,const str& expression);
static bool read_next_response_field(minecontrol_message& response,str& key,str& value);

// commands
static bool hello_exchange(session_state& session);
static void login(session_state& session); // these commands provide an interactive mode for input
static void logout(session_state& session);
static void start(session_state& session);
static void extend(session_state& session);
static void exec(session_state& session);
static void auth_ls(session_state& session);
static void console(session_state& session);
static void stop(session_state& session);
static void any_command(const generic_string& command,session_state& session); // these commands do not provide an interactive mode

static void* console_output_thread(void* param)
{
    std::function<void()>* functor = reinterpret_cast<std::function<void()>*>(param);
    (*functor)();
    return nullptr;
}

static std::function<void(char*)> consoleRlLineFunctor;
static void console_rl_line_callback(char* line)
{
    consoleRlLineFunctor(line);
}

static std::function<void(void)> consoleRlRedisplayFunctor;
static void console_rl_redisplay_callback(void)
{
    consoleRlRedisplayFunctor();
}

int main(int argc,const char* argv[])
{
    // initialize openssl
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    PROGRAM_NAME = argv[0];
    // attempt to setup SIGPIPE handler
    if (::signal(SIGPIPE,&sigpipe_handler) == SIG_ERR) {
        errConsole << PROGRAM_NAME << ": couldn't set up signal handler for SIGPIPE\n";
        return 1;
    }

    // process options; give them precedence over non-option command-line arguments
    int top = 0;
    const char* mainArgs[20];
    for (int i = 1;i < argc;++i) {
        if (argv[i][0] == '-') {
            int exitCode;
            if (argv[i][1] == '-') {
                // this is a long option: pass the entire argument minus the "--"
                if ( !check_long_option(argv[i]+2,exitCode) )
                    return exitCode;
            }
            else if (i+1<argc && argv[i][1]!=0) {
                if ( !check_short_option(argv[i]+1,argv[i+1],exitCode) )
                    return exitCode;
                ++i;
            }
            else {
                errConsole << PROGRAM_NAME << ": error: expected argument after '" << argv[i] << "' option\n";
                return 1;
            }
        }
        else
            mainArgs[top++] = argv[i];
    }

    session_state session;
    // setup session with the correct stream and io device
    if (top > 0) { // use network socket
        session.psocket = new network_socket;
        session.paddress = new network_socket_address(mainArgs[0],SERVICE_PORT);
        session.paddress->setEncrypted(true);
    }
    else { // use domain socket
        session.psocket = new domain_socket;
        session.paddress = new domain_socket_address(DOMAIN_NAME);
    }

    // create socket and attempt to establish connection with minecontrol server
    session.psocket->open();
    if ( !session.psocket->connect(*session.paddress) ) {
        errConsole << PROGRAM_NAME << ": couldn't establish connection with " << session.paddress->get_address_string() << endline;
        return 1;
    }
    session.connectStream.assign(*session.psocket);

    // perform the hello exchange
    if ( !hello_exchange(session) ) {
        errConsole << PROGRAM_NAME << ": the server did not respond properly; are you sure it's a minecontrold server?\n";
        return 1;
    }

    // greetings were received; connection is up
    stdConsole << "Connection established: client '"
               << PROGRAM_NAME << '-' << PROGRAM_VERSION << "' ---> server '"
               << session.serverName << '-' << session.serverVersion << "'\n";
    stdConsole.flush_output();

    // main message loop
    while (session.sessionControl) {
        str command;

        // stringstream prompt;
        // if (session.username.length() > 0)
        //     prompt << session.username << '@';
        // prompt << session.serverName << '-' << session.serverVersion << PROMPT_MAIN << ' ';

        // char* line = readline(prompt.get_device().c_str());
        // if (line == nullptr) {
        //     break;
        // }
        // if (!*line) {
        //     continue;
        // }
        // add_history(line);

        if (session.username.length() > 0)
            stdConsole << session.username << '@';
        stdConsole << session.serverName << '-' << session.serverVersion << PROMPT_MAIN << ' ';
        session.inputStream.clear();
        stdConsole.getline( session.inputStream.get_device() );
        //session.inputStream.get_device() = line;
        session.inputStream >> command;
        rutil_to_lower_ref(command);

        if (command == "login")
            login(session);
        else if (command == "logout")
            logout(session);
        else if (command == "start")
            start(session);
        else if (command == "extend")
            extend(session);
        else if (command == "exec")
            exec(session);
        else if (command == "auth-ls")
            auth_ls(session);
        else if (command == "console") {
            // list<str> history;
            // for (HIST_ENTRY** ent = history_list();*ent != nullptr;++ent) {
            //     history.push_back((*ent)->line);
            // }
            // clear_history();

            auto save_rl_catch_signals = rl_catch_signals;
            auto save_rl_catch_sigwinch = rl_catch_sigwinch;
            auto save_rl_deprep_term_function = rl_deprep_term_function;
            auto save_rl_prep_term_function = rl_prep_term_function;
            auto save_rl_change_environment = rl_change_environment;

            // rl_replace_line("",0);
            // rl_reset_line_state();
            // rl_cleanup_after_signal();

            rl_catch_signals = false;
            rl_catch_sigwinch = false;
            rl_deprep_term_function = nullptr;
            rl_prep_term_function = nullptr;
            rl_change_environment = false;

            console(session);
            clear_history();

            rl_catch_signals = save_rl_catch_signals;
            rl_catch_sigwinch = save_rl_catch_sigwinch;
            rl_deprep_term_function = save_rl_deprep_term_function;
            rl_prep_term_function = save_rl_prep_term_function;
            rl_change_environment = save_rl_change_environment;

            rl_replace_line("",0);
            rl_reset_line_state();

            // for (list<str>::iterator it = history.begin();it != history.end();++it) {
            //     add_history(it->c_str());
            // }
        }
        else if (command == "stop")
            stop(session);
        else if (command == "quit")
            break;
        else
            any_command(command,session);
    };

    return 0;
}

bool check_long_option(const char* option,int& exitCode)
{
    exitCode = 0;
    if ( rutil_strcmp(option,"help") ) {
        stdConsole << "usage: " << PROGRAM_NAME <<
" [remote-host] [-p port|path] [--version] [--help]\n\
\n\
The following commands can be run interactively:\n\
 login - authenticate with minecontrol server\n\
 logout - deauthenticate with minecontrol server\n\
 start - run remote Minecraft server\n\
 stop - terminate remote Minecraft server\n\
 extend - extend time limit for Minecraft server\n\
 exec - run authority program\n\
 console - enter Minecraft server console mode\n\
 shutdown - terminate remote minecontrol server\n\
 quit - exit this program\n\
\n\
Run 'man minecontrol(1)' for more information.\n\
Report bugs to Roger Gee <rpg11a@acu.edu>" << endline;
        return false;
    }
    else if ( rutil_strcmp(option,"version") ) {
        stdConsole << PROGRAM_NAME << " version " << PROGRAM_VERSION << newline;
        return false;
    }
    else {
        errConsole << PROGRAM_NAME << ": error: unrecognized option '" << option << "'\n";
        exitCode = 1;
        return false;
    }
    return true;
}

bool check_short_option(const char* option,const char* argument,int& exitCode)
{
    if (*option == 'p') {
        // change port and path (only one will be used)
        DOMAIN_NAME = argument;
        SERVICE_PORT = argument;
    }
    else {
        errConsole << PROGRAM_NAME << ": error: unrecognized option '" << option << "'\n";
        exitCode = 1;
        return false;
    }
    return true;
}

void sigpipe_handler(int signal)
{
    if (signal == SIGPIPE)
        stdConsole << PROGRAM_NAME << ": the connection was lost; the server most likely closed the connection or was shutdown" << endline;
    _exit(SIGPIPE);
}

void term_echo(bool on)
{
    // turn input echoing on or off
    int fd = ::open("/dev/tty",O_RDWR);
    if (fd != -1) {
        termios attributes;
        if (::tcgetattr(fd,&attributes) != 0)
            return;
        if (on)
            attributes.c_lflag |= ECHO;
        else
            attributes.c_lflag &= ~ECHO;
        if (::tcsetattr(fd,TCSAFLUSH,&attributes) != 0)
            return;
        ::close(fd);
    }
}

bool check_status(io_device& device)
{
    auto condition = device.get_last_operation_status();
    if (condition == no_input) {
        stdConsole << PROGRAM_NAME << ": the server closed the connection" << endline;
        return false;
    }
    if (condition == bad_read) {
        stdConsole << PROGRAM_NAME << ": an error occurred and the connection was closed" << endline;
        return false;
    }
    return true;
}

bool request_response_sequence(session_state& session)
{
    // make the request
    session.connectStream << session.request.get_message();
    // read the response
    session.connectStream >> session.response;
    // see if the connection is still good
    if ( !check_status(session.connectStream.get_device()) ) {
        session.sessionControl = false;
        return false;
    }
    return print_response(session.response);
}

bool print_response(minecontrol_message& response)
{
    str key;
    if ( rutil_strcmp(response.get_command(),"message") ) {
        while (response.get_field_key_stream() >> key) {
            if (key == "payload") {
                response.get_field_value_stream() >> key;
                stdConsole << key << newline;
                break;
            }
        }
        stdConsole << '[' << PROGRAM_NAME << ": server responded with SUCCESS message]" << endline;
        return true;
    }
    else if ( rutil_strcmp(response.get_command(),"error") ) {
        while (response.get_field_key_stream() >> key) {
            if (key == "payload") {
                response.get_field_value_stream() >> key;
                errConsole << key << newline;
                break;
            }
        }
        errConsole << '[' << PROGRAM_NAME << ": server responded with ERROR message]" << endline;
        return false;
    }
    else if ( rutil_strcmp(response.get_command(),"list-message") ) {
        while (response.get_field_key_stream() >> key) {
            if (key == "item") {
                response.get_field_value_stream() >> key;
                stdConsole << key << newline;
            }
        }
        stdConsole << '[' << PROGRAM_NAME << ": server responded with SUCCESS message]" << endline;
        return true;
    }
    else if ( rutil_strcmp(response.get_command(),"list-error") ) {
        while (response.get_field_key_stream() >> key) {
            if (key == "item") {
                response.get_field_value_stream() >> key;
                errConsole << key << newline;
            }
        }
        errConsole << '[' << PROGRAM_NAME << ": server responded with ERROR message]" << endline;
        return true;
    }
    errConsole << '[' << PROGRAM_NAME << ": server responded with UNRECOGNIZED message]" << endline;
    return false;
}

void insert_field_expression(minecontrol_message_buffer& msgbuf,const str& expression)
{
    size_type i = 0;
    str fieldName, fieldValue;
    while (i < expression.length()) {
        bool found = false;
        while (i < expression.length()) {
            if (expression[i] == '=')
                found = true;
            else if (expression[i] == '\n')
                break;
            else if (!found)
                fieldName.push_back( expression[i] );
            else
                fieldValue.push_back( expression[i] );
            ++i;
        }
        ++i;
        if (fieldName.length() == 0)
            errConsole << PROGRAM_NAME << ": empty property name; use form property-name=property-value" << endline;
        else if (!found)
            errConsole << PROGRAM_NAME << ": property '" << fieldName << "' requires value" << endline;
        else if (fieldValue.length() == 0)
            errConsole << PROGRAM_NAME << ": property '" << fieldName << "' requires value; value was empty" << endline;
        else {
            msgbuf.enqueue_field_name( fieldName.c_str() );
            msgbuf << fieldValue << newline;
        }
        fieldName.clear();
        fieldValue.clear();
    }
}

bool read_next_response_field(minecontrol_message& response,str& key,str& value)
{
    response.get_field_key_stream() >> key;
    if ( response.get_field_key_stream().get_input_success() ) {
        response.get_field_value_stream() >> value;
        return response.get_field_value_stream().get_input_success();
    }
    return false;
}

bool hello_exchange(session_state& session)
{
    str key;
    minecontrol_message res;
    minecontrol_message req("HELLO");
    req.add_field("Name",CLIENT_NAME);
    req.add_field("Version",PROGRAM_VERSION);
    session.connectStream << req;
    session.connectStream >> res;
    if (!res.good() || !rutil_strcmp(res.get_command(),"greetings"))
        return false;
    while (res.get_field_key_stream() >> key) {
        if (key == "name")
            res.get_field_value_stream() >> session.serverName;
        else if (key == "version")
            res.get_field_value_stream() >> session.serverVersion;
    }
    return session.serverName.length()>0 && session.serverVersion.length()>0;
}

void login(session_state& session)
{
    str username, password;
    minecontrol_message response;
    // try to read username from original command line
    session.inputStream >> username;
    if (username.length() == 0) {
        stdConsole << "login: ";
        stdConsole >> username;
    }
    stdConsole << "password: ";
    term_echo(false);
    stdConsole.getline(password);
    term_echo(true);
    stdConsole << endline; // move cursor down a row
    // create request message
    session.request.begin("LOGIN");
    session.request.enqueue_field_name("Username");
    session.request.enqueue_field_name("Password");
    session.request << username << newline << password << flush;
    if ( request_response_sequence(session) )
        session.username = username;
}

void logout(session_state& session)
{
    session.request.begin("LOGOUT");
    if ( request_response_sequence(session) )
        session.username.clear();
}

void start(session_state& session)
{
    str s;
    str name, props;
    bool modifier = false;
    if (session.inputStream >> s) {
        if (s == "create") {
            modifier = true;
            session.inputStream >> name;
        }
        else
            name = s;
        session.inputStream.getline(props);
        for (size_type i = 0;i<props.length();++i)
            if (props[i] == '\\')
                props[i] = '\n';
    }
    else {
        stdConsole << "Enter server name: ";
        stdConsole >> name;
        stdConsole << "Is this a new server? ";
        stdConsole >> s;
        rutil_to_lower_ref(s);
        modifier = (s=="y" || s=="yes");
        stdConsole << "Enter server properties:\n";
        while (true) {
            str line;
            size_type i;
            stdConsole << "\t> ";
            stdConsole.getline(line);
            if (!stdConsole.get_input_success() || line.length() == 0)
                break;
            props += line;
            props.push_back('\n');
        }
    }
    // create request message
    session.request.begin("START");
    session.request.enqueue_field_name("ServerName");
    session.request.enqueue_field_name("IsNew");
    session.request << name << newline << modifier << newline;
    insert_field_expression(session.request,props);
    session.request << flush;
    request_response_sequence(session);
}

void extend(session_state& session)
{
    str tokA, tokB;
    session.inputStream >> tokA >> tokB;
    if (tokA.length() == 0) {
        stdConsole << "Enter ID of running server: ";
        stdConsole >> tokA;
    }
    if (tokB.length() == 0) {
        stdConsole << "Enter time to extend (hours): ";
        stdConsole >> tokB;
    }
    session.request.begin("EXTEND");
    session.request.enqueue_field_name("ServerID");
    session.request.enqueue_field_name("Amount");
    session.request << tokA << newline << tokB << flush;
    request_response_sequence(session);
}

void exec(session_state& session)
{
    str tokA, tokB;
    session.inputStream >> tokA;
    if (tokA.length() == 0) {
        stdConsole << "Enter ID of running server: ";
        stdConsole >> tokA;
    }
    while ( session.inputStream.has_input() ) {
        str item;
        session.inputStream >> item;
        if (tokB.length() > 0) {
            tokB.push_back(' ');
            tokB += item;
        }
        else
            tokB += item;
    }
    if (tokB.length() == 0) {
        stdConsole << "Enter command: ";
        stdConsole.getline(tokB);
    }
    session.request.begin("EXEC");
    session.request.enqueue_field_name("ServerID");
    session.request.enqueue_field_name("Command");
    session.request << tokA << newline << tokB << flush;
    request_response_sequence(session);
}

void auth_ls(session_state& session)
{
    str filter;

    session.inputStream >> filter;
    session.request.begin("AUTH-LS");
    if (filter.length() != 0) {
        session.request.enqueue_field_name("Filter");
        session.request << filter << newline << flush;
    }

    request_response_sequence(session);
}

void console(session_state& session)
{
    bool good;
    str key, value;
    pthread_t tid;
    SCREEN* screen = set_term(nullptr);
    WINDOW* winCommand;
    WINDOW* winSeparator;
    WINDOW* winMessages;
    set_term(screen);

    // get server id from user; first see if they specified it on the cmdline
    session.inputStream >> key;
    if (key.length() == 0) {
        stdConsole << "Enter ID of running server: ";
        stdConsole >> key;
    }

    // negotiate with the server for console mode
    session.request.begin("CONSOLE");
    session.request.enqueue_field_name("ServerID");
    session.request << key << flush;
    session.connectStream << session.request.get_message();
    session.connectStream >> session.response;
    if ( !rutil_strcmp(session.response.get_command(),"console-message") ) {
        print_response(session.response);
        return;
    }
    good = false;
    while ( read_next_response_field(session.response,key,value) ) {
        if (key == "status") {
            if (value == "established") {
                good = true;
                break;
            }
            if (value == "failed") {
                errConsole << PROGRAM_NAME << ": minecontrol server is not accepting console clients at this time" << endline;
                return;
            }
        }
    }
    if (!good) {
        errConsole << PROGRAM_NAME << ": server refused console mode" << endline;
        return;
    }

    // set up ncurses screen
    initscr();
    if (has_colors()) {
        start_color();
        use_default_colors();
    }
    winCommand = newwin(1,COLS,0,0);
    winMessages = newwin(LINES - 1,COLS,1,0);
    scrollok(winMessages,TRUE);
    cbreak();
    noecho();
    nonl();
    intrflush(nullptr,false);

    // Set up console messages thread.
    std::function<void()> console_thread_functor = [&session,&winMessages]() {
        static const int BUFFER_MAX = 1024;

        bool done = false;
        str key, value;
        minecontrol_message serverMessage;
        socket_stream connectStream(session.connectStream); // create a new stream to prevent race conditions

        while (!done) {
            connectStream >> serverMessage;
            if ( !serverMessage.good() ) {
                // this shouldn't happen under normal circumstances (and if it
                // does a SIGPIPE probably will be sent)
                done = true;
                break;
            }

            if ( serverMessage.is_command("console-message") ) {
                while ( read_next_response_field(serverMessage,key,value) ) {
                    if (key=="status" && value=="shutdown") {
                        done = true;
                        break;
                    }

                    if (key=="payload" && value.length()>0) {
                        session.mtx.lock();
                        wscrl(winMessages,-1);
                        wmove(winMessages,0,0);
                        waddstr(winMessages,value.c_str());
                        wrefresh(winMessages);
                        session.mtx.unlock();
                        console_rl_redisplay_callback();
                    }
                }
            }
        }

        session.control = false;
    };
    pthread_create(
        &tid,
        nullptr,
        &console_output_thread,
        &console_thread_functor);

    // get user command input; quit with 'quit'/'exit' command; use the session.mtx
    // mutex to synchronize access to the connect stream and the ncurses library calls
    str cache;
    bool doCache = false; // cache commands to send as a batch later on
    minecontrol_message request("CONSOLE-COMMAND");
    static const int BUFFER_MAX = 1024;
    char buffer[BUFFER_MAX];

    // Create display function for readline to write to the command window.
    consoleRlRedisplayFunctor = [&winCommand,&session](void) {
        session.mtx.lock();
        werase(winCommand);
        mvwprintw(winCommand,0,0,"%s%s",rl_display_prompt,rl_line_buffer);
        wmove(winCommand,0,rutil_strlen(rl_display_prompt) + rl_point);
        wrefresh(winCommand);
        session.mtx.unlock();
    };
    rl_redisplay_function = &console_rl_redisplay_callback;

    // Create a handler for when readline sends us a line.
    consoleRlLineFunctor = [&request,&session,&doCache,&cache,&winMessages](char* line) {
        if (line == nullptr) {
            return;
        }

        if (*line) {
            add_history(line);
        }

        if ( rutil_strcmp(line,"cache") ) {
            doCache = true;
            session.mtx.lock();
            wscrl(winMessages,-1);
            wmove(winMessages,0,0);
            waddstr(winMessages,"<<== (minecontrol client) Command cache is enabled; type 'release' to flush and disable");
            wrefresh(winMessages);
            session.mtx.unlock();
        }
        else if ( rutil_strcmp(line,"release") ) {
            size_type i;
            doCache = false;
            i = 0;
            while (cache[i]) {
                request.add_field("ServerCommand",cache.c_str()+i);
                while (cache[i]) {
                    ++i;
                }
                ++i;
            }
            session.connectStream << request;
            request.reset_fields();
            cache.clear();
            session.mtx.lock();
            wscrl(winMessages,-1);
            wmove(winMessages,0,0);
            waddstr(winMessages,"<<== (minecontrol client) Command cache has been flushed and disabled");
            wrefresh(winMessages);
            session.mtx.unlock();
        }
        else if (rutil_strcmp(line,"quit") || rutil_strcmp(line,"exit")) {
            session.control = false;
        }
        else if (doCache) {
            cache += line;
            cache.push_back(0);
        }
        else {
            request.add_field("ServerCommand",line);
            session.connectStream << request;
            request.reset_fields();
        }
    };
    rl_callback_handler_install("ServerConsole> ",&console_rl_line_callback);

    console_rl_redisplay_callback();
    session.control = true;
    while (session.control) {
        chtype ch;
        ch = wgetch(winCommand);
        if (ch == KEY_F(1)) {
            session.control = false;
        }
        else {
            rl_stuff_char(ch);
            rl_callback_read_char();
        }
    }

    session.request.begin("CONSOLE-QUIT");
    session.connectStream << session.request.get_message();
    pthread_join(tid,NULL);
    rl_redisplay_function = rl_redisplay;
    rl_callback_handler_remove();

    // shutdown ncurses
    delwin(winMessages);
    delwin(winSeparator);
    delwin(winCommand);
    endwin();
    delscreen(screen);
    signal(SIGTSTP,SIG_DFL);
    signal(SIGCONT,SIG_DFL);
}

void stop(session_state& session)
{
    size_type i, j;
    str tok, authTok;
    session.inputStream >> tok;
    if (tok.length() == 0) {
        stdConsole << "Enter ID of running server: ";
        stdConsole >> tok;
    }
    i = 0;
    while (i<tok.length() && tok[i]!=':')
        ++i;
    for (j = i+1;j < tok.length();++j)
        authTok.push_back(tok[j]);
    tok.truncate(i);
    session.request.begin("STOP");
    session.request.enqueue_field_name("ServerID");
    if (authTok.length() > 0)
        session.request.enqueue_field_name("AuthPID");
    session.request << tok;
    if (authTok.length() > 0)
        session.request << newline << authTok;
    session.request << flush;
    request_response_sequence(session);
}

void any_command(const generic_string& command,session_state& session)
{
    str expr;
    session.request.begin( command.c_str() );
    while ( session.inputStream.has_input() ) {
        session.inputStream.getline(expr);
        insert_field_expression(session.request,expr);
    }
    session.request << flush;
    request_response_sequence(session);
}
