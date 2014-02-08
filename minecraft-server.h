// minecraft-server.h
#ifndef MINECRAFT_SERVER_H
#define MINECRAFT_SERVER_H
#include "minecraft-server-properties.h"
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
        rtypes::str homeDirectory; // home directory of user currently logged in
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

    class minecraft_server
    {
    public:
        // represents the start condition of the
        // server process as determined by either
        // the configuration or the Java process
        enum minecraft_server_start_condition
        {
            mcraft_start_success = 0, // the server process was started successfully
            mcraft_start_server_filesystem_error, // the server couldn't set up the filesystem for the minecraft server
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

    // static class that manages minecraft servers
    // spawned by this server
    class minecraft_server_manager
    {
    public:
        // allocates a new minecraft_server object
        static minecraft_server* allocate_server();

        static void deallocate_server(minecraft_server*);

        // attempts to cleanly shutdown any
        // running server processes
        static void shutdown_servers();
    private:
        static mutex _mutex;
        static rtypes::dynamic_array<void*> _servers; // rlibrary limitation: reduces code-bloat
    };
}

#endif
