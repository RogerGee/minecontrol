// minecraft-server.h
#ifndef MINECRAFT_SERVER_H
#define MINECRAFT_SERVER_H
#include "minecraft-server-properties.h" // gets rstringstream
#include "minecontrol-authority.h"
#include "minecontrol-misc-types.h"
#include "pipe.h"
#include "mutex.h"
#include <rlibrary/rdynarray.h>
#include <rlibrary/rset.h>

namespace minecraft_controller
{
    class minecraft_server_error { }; // generic error (should be left unhandled; left for debugging)

    struct minecraft_server_info
    {
        minecraft_server_info(bool createNew,const char* serverName,const user_info& userInformation);

        /* attributes for server startup: these determine the context in which a minecraft
           server runs or is created */
        bool isNew; // request (if possible) that a new server be made with the specified name
        rtypes::str internalName; // corresponds to the directory that contains the Minecraft server files
        user_info userInfo; // information for user that is running the server
        
        /* extended properties: these properties extend those found in server.properties, often
           implementing a feature provided by this network server program; some properties may
           override default settings for a 'minecraft_server' read from the minecontrol.init file */
        rtypes::uint64 serverTime; // number of seconds of allowed server run-time; (uint64)-1 means "not set"

        /* attempts to read server properties from specified string buffer; the properties
           should be formatted like so: key=value\n...; any errors are written in a human
           readable fashion to 'errorStream'; this operation processes both normal properties
           used in the 'server.properties' file and extended properties used by minecontrol */
        void read_props(rtypes::str& propertyList,rtypes::rstream& errorStream);

        /* looks up the property with the specified name and assigns it the specified
           value; false is returned if either the property didn't exist or if value
           was incorrect; this operation processes both normal property names (server.properties file)
           and extended property names; if 'applyIfDefault' is true, the property will only be applied
           if the current property value was not user-supplied */
        bool set_prop(const rtypes::str& key,const rtypes::str& value,bool applyIfDefault = false);

        const minecraft_server_property_list& get_props() const
        { return _properties; }
    private:
        enum _prop_process_flag
        {
            _prop_process_success,
            _prop_process_bad_key,
            _prop_process_bad_value,
            _prop_process_undefined
        };

        minecraft_server_property_list _properties;

        _prop_process_flag _process_ex_prop(const rtypes::str& key,const rtypes::str& value);
        _prop_process_flag _process_prop(const rtypes::str& key,const rtypes::str& value,bool applyIfDefault = false);
    };

    class minecraft_server_init_manager
    {
    public:
        minecraft_server_init_manager();

        void read_from_file(); // reload settings from master file
        void apply_properties(minecraft_server_info&); // apply override and default properties

        char* exec()
        { return &_exec[0]; }
        char* arguments()
        { return &_argumentsBuffer[0]; }
        rtypes::byte shutdown_countdown() const
        { return _shutdownCountdown; }
        rtypes::uint64 server_time() const // get time in seconds
        { return _maxSeconds; }
        rtypes::uint16 max_servers() const
        { return _maxServers; }
    private:
        mutex _mtx;

        rtypes::str _exec; // path to executable
        rtypes::str _argumentsBuffer; // arguments to executable separated by null-characters and terminated by a final null-character
        rtypes::byte _shutdownCountdown; // the number of seconds to wait for the server to shutdown before killing it
        rtypes::uint64 _maxSeconds; // the number of seconds to allow the server to run before auto-shutdown
        rtypes::uint16 _maxServers; // the maximum number of servers that minecontrol will allow
        rtypes::dynamic_array<minecraft_server_input_property> _overrideProperties; // properties that are always applied
        rtypes::dynamic_array<minecraft_server_input_property> _defaultProperties; // properties that are only applied when the user doesn't specify them
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
            mcraft_start_server_too_many_servers = 50, // the server process was not allowed to start; too many servers are running on the machine
            mcraft_start_server_filesystem_error, // the server couldn't set up the filesystem for the minecraft server
            mcraft_start_server_permissions_fail, // the server process couldn't set correct permissions for minecraft server process
            mcraft_start_server_does_not_exist, // server identified by specified internalName (directory) was not found
            mcraft_start_server_already_exists, // a new server could not be started because another one already exists
            mcraft_start_server_process_fail = 200, // the server process (Java) couldn't be executed (should be a larg(er) value)
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
        minecraft_server_start_condition begin(minecraft_server_info&);

        // requests more time for the server (in hours); the server will
        // display a message to players informing them of the time limit
        // change
        void extend_time_limit(rtypes::uint32 hoursMore);

        // get a reference to the server's authority object
        minecontrol_authority* get_authority()
        { return _authority; }
        const minecontrol_authority* get_authority() const
        { return _authority; }

        // requests that the server shutdown by issuing "stop"
        // to the server's standard input; "end" should be called
        // on every server after a begin; "end" will be called by the
        // destructor if you do not call it yourself
        minecraft_server_exit_condition end();

        // "was_started" and "is_running" can be used in conjunction
        // to determine whether "end()" has been called on the object
        bool was_started() const
        { return _processID != -1; }
        bool is_running() volatile
        { return _threadCondition; }

        rtypes::str get_internal_name() const
        { return _internalName; }
        rtypes::uint32 get_internal_id() const
        { return _internalID; }
    private:
        static rtypes::set<rtypes::uint32> _idSet;
        static mutex _idSetProtect;
        static short _handlerRef;
        static rtypes::uint64 _alarmTick;
        static void _alarm_handler(int);
        static void* _io_thread(void*);

        // global server settings (read from initialization file)
        static minecraft_server_init_manager _globals; // global settings

        // per server attributes
        rtypes::str _internalName;
        rtypes::uint32 _internalID;
        rtypes::ulong _threadID;
        rtypes::int32 _processID;
        pipe _iochannel;
        minecontrol_authority* _authority;
        volatile bool _threadCondition;
        minecraft_server_exit_condition _threadExit; // (this just serves as a memory location that outlives _io_thread)
        rtypes::uint64 _maxTime;
        rtypes::uint64 _elapsed;
        int _uid, _gid;

        // helpers
        bool _create_server_properties_file(minecraft_server_info&);
        void _check_extended_options(const minecraft_server_info&);
    };

    rtypes::rstream& operator <<(rtypes::rstream&,minecraft_server::minecraft_server_start_condition);
    rtypes::rstream& operator <<(rtypes::rstream&,minecraft_server::minecraft_server_exit_condition);

    class minecraft_server_manager_error { };

    struct server_handle
    {
        friend class minecraft_server_manager;
        server_handle();

        rtypes::uint64 get_clientid() const
        { return _clientid; }
        void set_clientid(const rtypes::uint64& id)
        { _clientid = id; }

        minecraft_server* pserver;
    private:
        bool _issued;
        rtypes::uint64 _clientid;
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

        enum auth_lookup_result
        {
            auth_lookup_found, // servers were found to which the specified user has access
            auth_lookup_no_owned, // servers were found, but the specified user doesn't have access to them
            auth_lookup_none // no servers are running
        };

        // looks up all current servers that are still running
        // that can be accessed based on an authenticated user;
        // the server handles are checked out (detached from the system)
        static auth_lookup_result lookup_auth_servers(const user_info& login,rtypes::dynamic_array<server_handle*>& outList);

        // prints out server all information on the specified rstream; if login != NULL then
        // additional information that is accessible to an authenticated user might be included
        static void print_servers(rtypes::rstream&,const user_info* login = NULL);

        // starts up the server manager system
        static void startup_server_manager();

        // attempts to cleanly shutdown any
        // running server processes
        static void shutdown_server_manager();
    private:
        static mutex _mutex;
        static rtypes::dynamic_array<server_handle*> _handles;
        static rtypes::ulong _threadID;
        static volatile bool _threadCondition;

        static void* _manager_thread(void*);
    };
}

#endif
