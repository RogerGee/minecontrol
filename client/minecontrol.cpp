// minecontrol.cpp - minecraft-controller client application
#include "rlibrary/rstdio.h"
#include "rlibrary/rstringstream.h"
#include "rlibrary/rutility.h"
#include "domain-socket.h"
// these will be replaced later
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
//
using namespace rtypes;
using namespace minecraft_controller;

// constants
static const char* const DOMAIN_NAME = "minecraft-control";
static const char* programName;

// helpers
static void echo(bool onState); // will become obsolete in future versions
static bool check_status(io_device& device);
static bool server_message(rstream&);

/* commands - a command function takes arguments from the
   first stream passed to it; it then issues a command 
   using the proper syntax on the second stream passed to
   it; if no input is available, an interactive mode should
   be provided to obtain minimal arguments
 */
static void login(rstream&,rstream&,str&); // these commands provide an interactive mode for input
static void start(rstream&,rstream&);
static void status(rstream&,rstream&);
static void stop(rstream&,rstream&);
static void any_command(const str& command,rstream&,rstream&); // this command does not provide an interactive mode

int main(int,const char* argv[])
{
    str uname;
    str serverName;
    // for now, just use a local domain
    // socket connection to talk to the server
    domain_socket sock;
    domain_socket_stream sockstream;
    programName = argv[0];

    // attempt to connect to domain server
    sock.open();
    if ( !sock.connect(DOMAIN_NAME) )
    {
        stdConsole << err << programName << ": couldn't establish connection with minecraft-control\n";
        return 1;
    }
    sockstream.open(sock);
    sockstream.delimit_whitespace(false);
    sockstream.add_extra_delimiter("|,");

    // receive server name as first communication
    sockstream.getline(serverName);
    if ( !check_status(sock) )
        return 0;
    stdConsole << "Connection established -> " << serverName << endline;

    while (true)
    {
        str command;
        rstringstream ss;
        if (uname.length() > 0)
            stdConsole << uname << '@';
        stdConsole << serverName << ">> ";
        stdConsole.getline( ss.get_device() );
        ss >> command; // get first token
        rutil_to_lower_ref(command);

        // handle user command line
        if (command == "quit")
            break;
        if (command == "login")
            login(ss,sockstream,uname);
        else if (command == "start")
            start(ss,sockstream);
        else if (command == "status")
            status(ss,sockstream);
        else if (command == "stop")
            stop(ss,sockstream);
        else // send any other command to default command handler
            any_command(command,ss,sockstream);

        // handle response from server
        bool serverSuccess; // did the server encounter an error or not?
        ss.clear();
        sockstream.getline( ss.get_device() );
        serverSuccess = server_message(ss);

        // handle post-response actions
        if ((command=="login" && !serverSuccess) || command=="logout")
            uname.clear();

        if ( !check_status(sock) )
            break; // server most likely closed connection
    }
}

// this function will become obsolete in future versions
void echo(bool on)
{
    // turn input echoing on or off
    int fd = ::open("/dev/tty",O_RDWR);
    if (fd != -1)
    {
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
    if (condition == no_input)
    {
        stdConsole << programName << ": The server closed the connection." << endline;
        return false;
    }
    if (condition == bad_read)
    {
        stdConsole << programName << ": An error occurred and the connection was lost." << endline;
        return false;
    }
    return true;
}

bool server_message(rstream& inputStream)
{
    str command;
    inputStream.add_extra_delimiter("|,");
    inputStream.delimit_whitespace(false);
    inputStream >> command;
    if (command == "message")
    {
        inputStream >> command;
        stdConsole << command << newline;
    }
    else if (command == "error")
    {
        inputStream >> command;
        stdConsole << command << newline;
        return false;
    }
    else if (command == "list")
    {
        while ( inputStream.has_input() )
        {
            str item;
            inputStream >> item;
            stdConsole << item << newline;
        }
    }
    return true;
}

void login(rstream& inputStream,rstream& comStream,str& username)
{
    // this function will effectively output the username; it's
    // up to the calling context to determine if the login attempt
    // was successful
    str password;
    // try to read username from original command line
    inputStream >> username;
    if (username.length() == 0)
    {
        stdConsole << "login: ";
        stdConsole >> username;
    }
    stdConsole << "password: ";
    echo(false);
    stdConsole.getline(password);
    echo(true);
    stdConsole << endline; // move cursor down a row
    comStream << "login|" << username << ',' << password << endline;
}

void start(rstream& inputStream,rstream& comStream)
{
    str modifier, name, props;
    if ( (inputStream>>modifier) )
    {
        if (modifier.length()>0 && modifier!="create")
        {
            name = modifier;
            modifier.clear();
        }
        else
            inputStream >> name;
        inputStream.getline(props);
    }
    else
    {
        stdConsole << "Is this a new server? ";
        stdConsole >> modifier;
        rutil_to_lower_ref(modifier);
        if (modifier=="y" || modifier=="yes")
            modifier = "create";
        else
            modifier.clear();
        stdConsole << "Enter server name: ";
        stdConsole >> name;
        stdConsole << "Enter properties, one on each line; type 'stop' to finish - \n";
        while (true)
        {
            str line;
            size_type i;
            stdConsole << "\t> ";
            stdConsole.getline(line);
            if (rutil_to_lower(line) == "stop")
                break;
            i = 0;
            while (i < line.length())
            {
                if (line[i] == '=')
                    break;
                ++i;
            }
            if (i==0 || i>=line.length())
            {
                stdConsole << "Bad syntax; use 'property key=property value'\n";
                continue;
            }
            i = 0;
            // don't allow commas
            while (i < line.length())
            {
                if (line[i] == ',')
                    line[i] = ';';
                ++i;
            }
            if (props.length() > 0)
                props.push_back(',');
            props += line;
        }
    }
    comStream << "start|";
    if (modifier.length() > 0)
        comStream << modifier << ',';
    comStream << name;
    if (props.length() > 0)
        comStream << ',' << props;
    comStream << endline;
}

void status(rstream&,rstream& comStream)
{
    comStream << "status" << endline;
}

void stop(rstream& inputStream,rstream& comStream)
{
    str tok;
    inputStream >> tok;
    if (tok.length() == 0)
    {
        stdConsole << "Enter ID of running server: ";
        stdConsole >> tok;
    }
    comStream << "stop|" << tok << endline;
}

void any_command(const str& command,rstream& inputStream,rstream& comStream)
{
    str tok;
    comStream << command << '|';
    inputStream >> tok;
    if (tok.length() > 0)
        inputStream << tok;
    while ( inputStream.has_input() )
    {
        inputStream >> tok;
        if (tok.length() > 0)
            comStream << ',' << tok;
    }
    comStream << endline;
}
