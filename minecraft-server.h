// minecraft-server.h
#ifndef MINECRAFT_SERVER_H
#define MINECRAFT_SERVER_H
#include "minecraft-server-properties.h" // gets rstringstream
#include <sys/types.h>
#include "pipe.h"
#include "mutex.h"
#include "rlibrary/rdynarray.h"

namespace minecraft_controller
{
    class minecraft_server_error { }; // generic error (should be left unhandled; left for debugging)

    struct minecraft_server_info
    {
        minecraft_server_info();

        bool isNew; // request (if possible) that a new server be made with the specified name
        rtypes::str internalName; // corresponds to the directory that contains the Minecraft server files
        const char* homeDirectory; // home directory of user currently logged in
        int uid, guid; // associated user and group id of currently logged-in user

        // attempts to read server properties until end-of-stream; the properties
        // should be formatted like so: --key=value; any errors are written in a human
        // readable fashion to 'errorStream'
        virtual void read_props(rtypes::rstream&,rtypes::rstream& errorStream);

        // writes properties in the correct format for the `server.properties'
        // file used by the minecraft server process
        virtual void put_props(rtypes::rstream&) const;
    };

    struct minecraft_server_info_ex : minecraft_server_info
    {
        virtual void read_props(rtypes::rstream&,rtypes::rstream& errorStream);
        virtual void put_props(rtypes::rstream&) const;
    private:
        minecraft_server_property_list _properties;

        static void _strip(rtypes::str&);
    };

    class minecraft_server_manager;

    class minecraft_server
    {
        friend class minecraft_server_manager;
    public:
        // represents the start condition of the
        // server process as determined by either
        // the configuration or the Java process
        enum minecraft_server_start_condition
        {
            mcraft_start_success = 0, // the server process was started successfully
            mcraft_start_server_filesystem_error, // the server couldn't set up the filesystem for the minecraft server
            mcraft_start_server_permissions_fail, // the server process couldn't set correct permissions for minecraft server process
            mcraft_start_server_does_not_exist, // server identified by specified internalName (directory) was not found
            mcraft_start_server_already_exists, // a new server could not be started because another one already exists
            mcraft_start_server_process_fail = 100, // the server process (Java) couldn't be executed (should be a larg(er) value)
            mcraft_start_failure_unknown
        };

        // represents the exit condition of the
        // server process as determined by the
        // different managing contexts
        enum minecraft_server_exit_condition
        {
            mcraft_noexit, // no request was issued and no exit occurred (used by the implementation)
            mcraft_exit_ingame, // the server process exited due to an unknown in-game event (on its own)
            mcraft_exit_authority_request, // the server process exited due to an authoritative event in this program
            mcraft_exit_authority_killed, // the server process was killed due to an authoritative event in this program
            mcraft_exit_timeout_request, // the server process exited due to the expiration of the allocated time
            mcraft_exit_timeout_killed, // the server process was killed due to the expiration of the allocated time
            mcraft_exit_request, // the server process exited due to a direct request from this program
            mcraft_exit_killed, // the server process was killed from this program
            mcraft_exit_unknown, // the server process was terminated but in an unknown or unspecified manner
            mcraft_server_not_running // the server didn't exit because it wasn't running
        };

        minecraft_server();
        ~minecraft_server();

        // spawns a child process that executes the minecraft
        // server Java binary
        minecraft_server_start_condition begin(const minecraft_server_info&);

        // requests that the server shutdown by issuing "stop"
        // to the server's standard input
        minecraft_server_exit_condition end();

        bool was_started() const
        { return _processID != -1; }
        bool is_running() volatile
        { return _threadCondition; }
    private:
        static short _handlerRef;
        static rtypes::qword _alarmTick;
        static void _alarm_handler(int);
        static void* _io_thread(void*);

        // per server attributes
        pthread_t _threadID;
        pid_t _processID;
        pipe _iochannel;
        volatile bool _threadCondition;
        minecraft_server_exit_condition _threadExit;
        rtypes::qword _elapsed;
        int _uid, _guid;

        // global server attributes (read from init file)
        rtypes::str _program; // path to executable
        rtypes::str _argumentsBuffer; // arguments to executable separated by null-characters and terminated by a final null-character
        rtypes::byte _shutdownCountdown; // the number of seconds to wait for the server to shutdown before killing it
        rtypes::qword _maxSeconds; // the number of seconds to allow the server to run before auto-shutdown
    };

    rtypes::rstream& operator <<(rtypes::rstream&,minecraft_server::minecraft_server_start_condition);
    rtypes::rstream& operator <<(rtypes::rstream&,minecraft_server::minecraft_server_exit_condition);

    class minecraft_server_manager_error { };

    struct server_handle
    {
        friend class minecraft_server_manager;
        server_handle();

        minecraft_server* pserver;
    private:
        bool _issued;
        rtypes::size_type _index;
    };

    // static class that manages minecraft servers
    // spawned by this process
    class minecraft_server_manager
    {
    public:
        // allocates a new minecraft_server object; the server object
        // (and any memory used to allocate it) is managed by this system;
        // however, the server handle is considered to be in a detached state,
        // meaning this system will keep the object available for as long as 
        // possible until either `shutdown_servers' is called; when finished,
        // the pointer should be passed to `attach_server'
        static server_handle* allocate_server();

        // released the server handle to this system; it may become invalid at
        // any time; to obtain a detached handle, use `lookup_auth_servers'
        static void attach_server(server_handle*);
        static void attach_server(server_handle**,rtypes::size_type count);

        // looks up all current servers that are still running
        // that can be accessed based on an authenticated user;
        // the server handles are checked out (detached from the system)
        static bool lookup_auth_servers(int uid,int guid,rtypes::dynamic_array<server_handle*>& outList);

        // prints out server all information on the specified rstream; this
        // includes servers that are not owned by a certain user
        static bool print_servers(rtypes::rstream&);

        // starts up the server manager system
        static void startup_server_manager();

        // attempts to cleanly shutdown any
        // running server processes
        static void shutdown_server_manager();
    private:
        static mutex _mutex;
        static rtypes::dynamic_array<server_handle*> _handles;
        static pthread_t _threadID;
        static volatile bool _threadCondition;

        static void* _manager_thread(void*);
    };
}

#endif
