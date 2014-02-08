#include "rlibrary/rstdio.h"
#include "domain-socket.h"
using namespace rtypes;
using namespace minecraft_controller;

// constants
static const char* const DOMAIN_NAME = "minecraft-control";

int main(int,const char* argv[])
{
    domain_socket sock;
    domain_socket_stream stream;

    sock.open(); // create new domain socket
    if ( !sock.connect(DOMAIN_NAME) )
    {
        stdConsole << err << argv[0] << ": couldn't establish connection with minecraft-controller server\n";
        return 1;
    }
    stream.open(sock);

    stdConsole << "Connection is established.\n";
    while (true)
    {
        str line;
        stdConsole << ">> ";
        stdConsole.getline(line);

        if (line == "<quit>")
            break;

        stream << line << endline;
        stream.getline(line);
        stdConsole << "The server says: " << line << endline;
    }
}
