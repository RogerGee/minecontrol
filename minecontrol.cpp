// minecontrol.cpp - minecraft-controller client application
#include "minecontrol-protocol.h"
#include "domain-socket.h"
#include "net-socket.h"
#include <rlibrary/rstdio.h> // gets io_device
#include <rlibrary/rstringstream.h>
#include <rlibrary/rutility.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
using namespace rtypes;
using namespace minecraft_controller;

// constants
static const char* const DOMAIN_NAME = "@minecontrol";
static const char* const SERVICE_PORT = "44446";
static const char* PROGRAM_NAME;
static const char* const PROGRAM_VERSION = "0.7 (BETA)";

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
    io_stream connectStream;
    stringstream inputStream;

    // minecontrol protocol messages
    crypt_session* pcrypto;
    minecontrol_message response;
    minecontrol_message_buffer request;

    // session alive control
    bool sessionControl;
};
session_state::session_state()
{
    psocket = NULL;
    paddress = NULL;
    pcrypto = NULL;
    sessionControl = true;
}
session_state::~session_state()
{
    if (psocket != NULL)
        delete psocket;
    if (paddress != NULL)
        delete paddress;
    if (pcrypto != NULL)
        delete pcrypto;
}

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
static void extend(session_state& session);
static void stop(session_state& session);
static void any_command(const generic_string& command,session_state& session); // these commands do not provide an interactive mode

int main(int argc,const char* argv[])
{
    PROGRAM_NAME = argv[0];
    // attempt to setup SIGPIPE handler
    if (::signal(SIGPIPE,&sigpipe_handler) == SIG_ERR) {
        errConsole << PROGRAM_NAME << ": couldn't set up signal handler for SIGPIPE\n";
        return 1;
    }

    session_state session;
    // setup session with the correct stream and io device
    if (argc > 1) { // use network socket        
        session.psocket = new network_socket;
        session.paddress = new network_socket_address(argv[1],SERVICE_PORT);
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
        else if (command == "extend")
            extend(session);
        else if (command == "stop")
            stop(session);
        else if (command == "quit")
            break;
        else
            any_command(command,session);
    };

    return 0;
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
    if ( !check_status(session.connectStream.get_device()) ) {
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
        else if (key == "encryptkey") {
            // the server sent the public key (modulus and exponent)
            str encryptKey;
            res.get_field_value_stream() >> encryptKey;
            try {
                session.pcrypto = new crypt_session(encryptKey);
            } catch (minecontrol_encrypt_error) {
                return false;
            }
        }
    }
    return session.serverName.length()>0 && session.serverVersion.length()>0 && session.pcrypto!=NULL;
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
    session.request.enqueue_field_name("Password",session.pcrypto);
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
    session.request << tokA << tokB << flush;
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
