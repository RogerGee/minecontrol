#include <rlibrary/rstdio.h>
#include "../minecontrol-authority.h"
using namespace rtypes;
using namespace minecraft_controller;

int main()
{
    str line;
    while (true) {
        minecraft_server_message* pmsg;
        stdConsole << "> ";
        stdConsole.getline(line);
        if (stdConsole.get_device().get_last_operation_status() != success_read)
            break;
        stdConsole << "\nAttempting to parse \"" << line << '"' << endline;
        pmsg = minecraft_server_message::generate_message(line);
        if (pmsg == NULL)
            stdConsole << "no luck, buddy!\n";
        else {
            size_type tcount = pmsg->get_token_count();
            stdConsole << "Gist: " << pmsg->get_gist_string() << newline
                       << "Type: " << pmsg->get_type_string() << newline
                       << "Hour: " << pmsg->get_hour() << newline
                       << "Minute: " << pmsg->get_minute() << newline
                       << "Second: " << pmsg->get_second() << newline
                       << "Payload: " << pmsg->get_payload() << newline
                       << "Original Payload: " << pmsg->get_original_payload() << newline;
            if (tcount >= 1) {
                stdConsole << "Tokens: {" << pmsg->get_token(0);
                if (tcount > 1) {
                    for (size_type i = 1;i < pmsg->get_token_count();++i)
                        stdConsole << ", " << pmsg->get_token(i);
                }
                stdConsole << '}' << newline;
            }
            stdConsole << "Format String: " << pmsg->get_format_string() << newline << newline;
            delete pmsg;
        }
    }
}
