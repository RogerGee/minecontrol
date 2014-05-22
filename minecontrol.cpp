// minecontrol.cpp - minecraft-controller client application
#include "rlibrary/rstdio.h"
#include "rlibrary/rstringstream.h"
#include "rlibrary/rutility.h"
#include "minecontrol-protocol.h"
#include "domain-socket.h"
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
using namespace rtypes;
using namespace minecraft_controller;

// constants
static const char* const DOMAIN_NAME = "minecraft-control";
static const char* PROGRAM_NAME;
static const char* const PROGRAM_VERSION = "0.6 (BETA)";

// types
struct session_state
{
    session_state(io_device& dev,rstream& cs)
        : connectDevice(dev), connectStream(cs), sessionControl(true) {}

    str username;
    str serverName, serverVersion;

    io_device& connectDevice;
    rstream& connectStream;
    rstringstream inputStream;

    minecontrol_message response;
    minecontrol_message_buffer request;

    bool sessionControl;
};

// signal handlers
static void sigpipe_handler(int); // handle SIGPIPE

// helpers
static void echo(bool onState);
static bool check_status(io_device& device);
static bool request_response_sequence(session_state& session);
static void insert_field_expression(minecontrol_message_buffer& msgbuf,const str& expression);

// commands
static bool hello_exchange(session_state& session);
static void login(session_state& session); // these commands provide an interactive mode for input
static void logout(session_state& session);
static void start(session_state& session);
static void stop(session_state& session);
static void any_command(const generic_string& command,session_state& session); // these commands do not provide an interactive mode

int main(int,const char* argv[])
{
    domain_socket_stream sockstream;
    domain_socket& sock = sockstream.get_device();
    session_state session(sock,sockstream);
    PROGRAM_NAME = argv[0];

    // attempt to setup SIGPIPE handler
    if (::signal(SIGPIPE,&sigpipe_handler) == SIG_ERR) {
        errConsole << PROGRAM_NAME << ": couldn't set up signal handler for SIGPIPE\n";
        return 1;
    }

    // attempt to connect to domain server
    sock.open();
    if ( !sock.connect(DOMAIN_NAME) ) {
        errConsole << PROGRAM_NAME << ": couldn't establish connection with minecraft-control\n";
        return 1;
    }

    // perform the hello exchange
    if ( !hello_exchange(session) ) {
        errConsole << PROGRAM_NAME << ": the server did not respond properly; are you sure it's a minecontrold server?\n";
        return 1;
    }

    stdConsole << "Connection established: client '" 
               << PROGRAM_NAME << '-' << PROGRAM_VERSION << " ---> " 
               << session.serverName << '-' << session.serverVersion << newline;

    // main message loop
    while (session.sessionControl) {
        str command;

        if (session.username.length() > 0)
            stdConsole << session.username << '@';
        stdConsole << session.serverName << '-' << session.serverVersion << "# ";
        session.inputStream.close(); // close any last session
        stdConsole.getline( session.inputStream.get_device() );

        session.inputStream >> command;
        rutil_to_lower_ref(command);
        if (command == "login")
            login(session);
        else if (command == "logout")
            logout(session);
        else if (command == "start")
            start(session);
        else if (command == "stop")
            stop(session);
        else if (command == "quit")
            break;
        else
            any_command(command,session);
    };
}

void sigpipe_handler(int signal)
{
    if (signal == SIGPIPE)
        stdConsole << PROGRAM_NAME << ": the connection was lost; the server most likely closed the connection or was shutdown" << endline;
    _exit(SIGPIPE);
}

void echo(bool on)
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
    if ( !check_status(session.connectDevice) ) {
        session.sessionControl = false;
        return false;
    }
    str key;
    if ( rutil_strcmp(session.response.get_command(),"message") ) {
        while (session.response.get_field_key_stream() >> key) {
            if (key == "payload") {
                session.response.get_field_value_stream() >> key;
                stdConsole << key << newline;
                break;
            }
        }
        stdConsole << '[' << PROGRAM_NAME << ": server responded with SUCCESS message]" << endline;
        return true;
    }
    else if ( rutil_strcmp(session.response.get_command(),"error") ) {
        while (session.response.get_field_key_stream() >> key) {
            if (key == "payload") {
                session.response.get_field_value_stream() >> key;
                errConsole << key << newline;
                break;
            }
        }
        errConsole << '[' << PROGRAM_NAME << ": server responded with ERROR message]" << endline;
        return false;
    }
    else if ( rutil_strcmp(session.response.get_command(),"list-message") ) {
        while (session.response.get_field_key_stream() >> key) {
            if (key == "item") {
                session.response.get_field_value_stream() >> key;
                stdConsole << key << newline;
            }
        }
        stdConsole << '[' << PROGRAM_NAME << ": server responded with SUCCESS message]" << endline;
        return true;
    }
    else if ( rutil_strcmp(session.response.get_command(),"list-error") ) {
        while (session.response.get_field_key_stream() >> key) {
            if (key == "item") {
                session.response.get_field_value_stream() >> key;
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

bool hello_exchange(session_state& session)
{
    str key;
    minecontrol_message res;
    minecontrol_message req("HELLO");
    req.add_field("Name",PROGRAM_NAME);
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
    echo(false);
    stdConsole.getline(password);
    echo(true);
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

void stop(session_state& session)
{
    str tok;
    session.inputStream >> tok;
    if (tok.length() == 0) {
        stdConsole << "Enter ID of running server: ";
        stdConsole >> tok;
    }
    session.request.begin("STOP");
    session.request.enqueue_field_name("ServerID");
    session.request << tok << flush;
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
