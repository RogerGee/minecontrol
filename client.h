// client.h
#ifndef CLIENT_H
#define CLIENT_H
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

        bool authenticate();
        bool message_loop();
        void command_start(rtypes::rstream&);
        void command_status(rtypes::rstream&);
        void command_stop(rtypes::rstream&);

        domain_socket_stream connection;
        pthread_t threadID;
        volatile bool threadCondition;
        int uid, guid;
        rtypes::str homedir;
        rtypes::size_type referenceIndex;
    };
}

#endif
