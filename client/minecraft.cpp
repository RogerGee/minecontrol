#include "rlibrary/rstdio.h"
#include "domain-socket.h"
using namespace rtypes;
using namespace minecraft_controller;

// constants
static const char* const DOMAIN_NAME = "minecraft-control";

int main(int,const char* argv[])
{
    // this is a beta test
    str message;
    domain_socket sock;
    domain_socket_stream stream;

    sock.open(); // create new domain socket
    if ( !sock.connect(DOMAIN_NAME) )
    {
        stdConsole << err << argv[0] << ": couldn't establish connection with minecraft-control!\n";
        return 1;
    }
    stream.open(sock);

    stdConsole << "Connection established. Type `quit' to exit.\n";

    str username, password;
    stdConsole << "Login: ";
    stdConsole >> username;
    stdConsole << "Password: ";
    stdConsole >> password;
    stream << username << ' ' << password << endline;

    stream.getline(message);
    stdConsole << message << endline;

    while (true)
    {
        stream.getline(message);

        auto status = sock.get_last_operation_status();
        if (status==no_input || status==bad_read)
        {
            stdConsole << "The server closed the connection.\n";
            break;
        }

        stdConsole << message << endline;

        stdConsole << ">> ";
        stdConsole.getline(message);

        if (message == "quit")
            break;

        stream << message << endline;
    }
}
