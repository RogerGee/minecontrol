// minecraft-server.h
#ifndef MINECRAFT_SERVER_H
#define MINECRAFT_SERVER_H
#include "minecraft-server-properties.h"
#include <sys/types.h>
#include <pthread.h>
#include "pipe.h"

namespace minecraft_controller
{
    class minecraft_server_error { }; // generic error (should be left unhandled; left for debugging)
    class minecraft_server_error_name_exists : public minecraft_server_error { }; // server cannot be created because a server with the same name already exists

    struct minecraft_server_info
    {
        minecraft_server_info();

        rtypes::str internalName; // corresponds to the directory that contains server files
        bool isNew; // determines if a 'minecraft_server' object attempts to create a new server or not

        // attempts to read server properties until end-of-stream
        void read_props(rtypes::rstream&);

        // writes properties in the correct format for the `server.properties'
        // file used by the minecraft server process; some fields are constant
        // and do not appear in this struture, however they are inserted as well
        void put_props(rtypes::rstream&) const;
    private:

    };

    class minecraft_server
    {
    public:
        // represents the start condit
        enum minecraft_server_start_condition
        {
            mcraft_start_fail, // the server could not be started
            mcraft_start_success // the server started successfully
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
            mcraft_server_not_running // the server didn't exit because it wasn't running
        };

        // represents the status of a poll operation
        enum minecraft_server_poll
        {
            mcraft_poll_not_started, // the server process was never started to begin with
            mcraft_poll_terminated, // the server process was started and terminated before the poll
            mcraft_poll_running // the server process is still running
        };

        minecraft_server(const minecraft_server_info&);
        ~minecraft_server();

        // spawns a child process that executes the minecraft
        // server Java binary
        minecraft_server_start_condition begin();

        // requests that the server shutdown by issuing "stop"
        // to the server's standard input
        minecraft_server_exit_condition end();

        // sends a null-signal to the server process to see
        // if it is still running; the server will be polled
        // 'times' times with a timeout of 'timeout' only while
        // a result of 'running' is being generated
        minecraft_server_poll poll(int times = 1,int timeout = 0) const;

        bool is_running() const
        { return _processID != -1; }
    private:
        static short _handlerRef;
        static rtypes::qword _alarmTick;
        static void _alarmHandler(int);
        static void* _ioThread(void*);

        // per server attributes
        pthread_t _threadID;
        pid_t _processID;
        pipe _iochannel;
        volatile bool _threadCondition;
        minecraft_server_exit_condition _threadExit;
        rtypes::qword _elapsed;

        // global server attributes (read from init file)
        rtypes::str _program; // path to executable
        rtypes::str _argumentsBuffer; // arguments to executable separated by null-characters and terminated by a final null-character
        rtypes::byte _shutdownCountdown; // the number of seconds to wait for the server to shutdown before killing it
        rtypes::qword _maxSeconds; // the number of seconds to allow the server to run before auto-shutdown
    };
}

#endif
