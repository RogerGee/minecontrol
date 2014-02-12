#include "rlibrary/rstdio.h"
#include "rlibrary/rutility.h"
#include "domain-socket.h"
using namespace rtypes;
using namespace minecraft_controller;

// constants
static const char* const DOMAIN_NAME = "minecraft-control";
static const char* programName;

// functions
static void echo(bool onState);
static bool check_status(io_device& device);

int main(int,const char* argv[])
{
    str message;
    str serverName;
    domain_socket sock;
    domain_socket_stream stream;
    programName = argv[0];

    // attempt to connect to domain server
    sock.open();
    if ( !sock.connect(DOMAIN_NAME) )
    {
        stdConsole << err << programName << ": couldn't establish connection with minecraft-control\n";
        return 1;
    }
    stream.open(sock);
    stream.delimit_whitespace(false);
    stream.add_extra_delimiter("|,");

    // receive server name as first communication
    stream.getline(serverName);
    if ( !check_status(sock) )
        return 0;
    stdConsole << "Connection established -> " << serverName << endline;

    // perform login sequence by default
    str username, password;
    stdConsole << "login: ";
    stdConsole >> username;
    stdConsole << "password: ";
    echo(false); stdConsole >> password; echo(true);
    stream << "login|" << username << ',' << password << endline;
    stream.getline(message);
    if ( !check_status(sock) )
        return 0;
    stdConsole << message << endline;

    while (true)
    {
        str commandLine;
        stdConsole << serverName << ">> ";
        stdConsole.getline(commandLine);

        if (commandLine == "quit")
            break;

        stream << commandLine << endline;
        stream.getline(message);
        // check connection status
        if ( !check_status(sock) )
            break;
        stdConsole << message << newline;
    }
}

void echo(bool)
{
    // turn input echoing on or off
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
