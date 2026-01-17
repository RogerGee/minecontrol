// minecontrol-client.h
#ifndef MINECONTROL_CLIENT_H
#define MINECONTROL_CLIENT_H
#include <rlibrary/rdynarray.h>
#include "minecontrol-protocol.h"
#include "minecontrol-misc-types.h"
#include "socket.h" // gets io_device
#include "mutex.h" // gets pthread

namespace minecraft_controller
{
    class controller_client_error { };

    class controller_client
    {
    public:
        // accepts a client connection on the specified socket;
        // if the accept failed, then NULL is returned, else
        // a pointer to a dynamically allocated `controller-client'
        // object; this memory will be freed automatically once the
        // client has disconnected
        static controller_client* accept_client(socket&);

        static void shutdown_clients();
	static void close_client_sockets();
    private:
        controller_client(socket* acceptedSocket);
        ~controller_client();

        static mutex clientsMutex; // protects 'clients'
        static rtypes::dynamic_array<void*> clients; // rlibrary limitation: reduces coat-bloat
        static void* client_thread(void*);

        // command data
        typedef bool (controller_client::* command_call)(rtypes::rstream&,rtypes::rstream&);
        static rtypes::size_type CMD_COUNT_WITHOUT_LOGIN;
        static rtypes::size_type CMD_COUNT_WITH_LOGIN;
        static rtypes::size_type CMD_COUNT_WITH_PRIVILEGED_LOGIN;
        static char const *const CMDNAME_WITHOUT_LOGIN[];
        static command_call const CMDFUNC_WITHOUT_LOGIN[];
        static char const *const CMDNAME_WITH_LOGIN[];
        static command_call const CMDFUNC_WITH_LOGIN[];
        static char const *const CMDNAME_WITH_PRIVILEGED_LOGIN[]; // root commands
        static command_call const CMDFUNC_WITH_PRIVILEGED_LOGIN[];

        // message handlers
        bool message_loop();
        bool command_login(rtypes::rstream&,rtypes::rstream&);
        bool command_logout(rtypes::rstream&,rtypes::rstream&);
        bool command_start(rtypes::rstream&,rtypes::rstream&);
        bool command_status(rtypes::rstream&,rtypes::rstream&);
        bool command_extend(rtypes::rstream&,rtypes::rstream&);
        bool command_exec(rtypes::rstream&,rtypes::rstream&);
        bool command_auth_ls(rtypes::rstream&,rtypes::rstream&);
        bool command_server_ls(rtypes::rstream&,rtypes::rstream&);
        bool command_profile_ls(rtypes::rstream&,rtypes::rstream&);
        bool command_stop(rtypes::rstream&,rtypes::rstream&);
        bool command_console(rtypes::rstream&,rtypes::rstream&);
        bool command_shutdown(rtypes::rstream&,rtypes::rstream&);

        // helpers
        rtypes::rstream& client_log(rtypes::rstream& stream)
        {
            return stream << '{' << sock->get_accept_id() << "} ";
        }

        rtypes::rstream& prepare_message()
        {
            msgbuf.begin("MESSAGE");
            msgbuf.enqueue_field_name("Payload");
            return msgbuf;
        }

        rtypes::rstream& prepare_error()
        {
            msgbuf.begin("ERROR");
            msgbuf.enqueue_field_name("Payload");
            return msgbuf;
        }

        rtypes::rstream& prepare_list_message()
        {
            msgbuf.begin("LIST-MESSAGE");
            msgbuf.repeat_field("Item");
            return msgbuf;
        }

        rtypes::rstream& prepare_list_error()
        {
            msgbuf.begin("LIST-ERROR");
            msgbuf.repeat_field("Item");
            return msgbuf;
        }

        socket* sock;
        socket_stream connection;
        minecontrol_message_buffer msgbuf;
        pthread_t threadID;
        volatile bool threadCondition;
        user_info userInfo;
        rtypes::size_type referenceIndex;
    };
}

#endif

/*
 * Local Variables:
 * mode:c++
 * indent-tabs-mode:nil
 * tab-width:4
 * End:
 */
