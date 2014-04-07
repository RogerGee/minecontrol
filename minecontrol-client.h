// minecontrol-client.h
#ifndef MINECONTROL_CLIENT_H
#define MINECONTROL_CLIENT_H
#include "rlibrary/rdynarray.h"
#include "domain-socket.h"
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
        // object; the memory should be freed automatically once the
        // client has disconnected
        static controller_client* accept_client(domain_socket&);

        static void shutdown_clients();
    private:
        controller_client();

        static mutex clientsMutex; // protects 'clients'
        static rtypes::dynamic_array<void*> clients; // rlibrary limitation: reduces coat-bloat
        static void* client_thread(void*);

        // command data
        typedef bool (controller_client::* command_call)(rtypes::rstream&);
        static rtypes::size_type CMD_COUNT_WITHOUT_LOGIN;
        static rtypes::size_type CMD_COUNT_WITH_LOGIN;
        static rtypes::size_type CMD_COUNT_WITH_PRIVALEGED_LOGIN;
        static char const *const CMDNAME_WITHOUT_LOGIN[];
        static command_call const CMDFUNC_WITHOUT_LOGIN[];
        static char const *const CMDNAME_WITH_LOGIN[];
        static command_call const CMDFUNC_WITH_LOGIN[];
        static char const *const CMDNAME_WITH_PRIVALEGED_LOGIN[]; // root commands
        static command_call const CMDFUNC_WITH_PRIVALEGED_LOGIN[];

        bool message_loop();
        bool command_login(rtypes::rstream&);
        bool command_logout(rtypes::rstream&);
        bool command_start(rtypes::rstream&);
        bool command_status(rtypes::rstream&);
        bool command_stop(rtypes::rstream&);
        bool command_console(rtypes::rstream&);
        bool command_shutdown(rtypes::rstream&);

        inline rtypes::rstream& client_log(rtypes::rstream&);
        inline rtypes::rstream& send_message(const char* command);

        domain_socket_stream connection;
        pthread_t threadID;
        volatile bool threadCondition;
        int uid, guid;
        rtypes::str homedir;
        rtypes::size_type referenceIndex;
    };
}

#endif
