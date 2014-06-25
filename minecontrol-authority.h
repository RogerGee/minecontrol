// minecontrol-authority.h
#ifndef MINECONTROL_AUTHORITY_H
#define MINECONTROL_AUTHORITY_H
#include "minecontrol-misc-types.h"
#include "pipe.h"
#include "socket.h"
#include "mutex.h"
#include <rlibrary/rdynarray.h>
#include <rlibrary/rstream.h>

namespace minecraft_controller
{
    /* error type */
    class minecontrol_authority_error { };

    /* represents the server message type as flagged by
       the second bracketed field in each line message */
    enum minecraft_server_message_type
    {
        type_unkn, // type is unknown as the message is irregular
        main_info, // server flagged Server thread/INFO
        main_warn, // server flagged Server thread/WARN
        auth_info, // server flagged User Authenticator ##/INFO
        auth_warn, // server flagged User Authenticator ##/WARN
        shdw_info, // server flagged Server Shutdown Thread/INFO
        shdw_warn // server flagged Server Shutdown Thread/WARN
    };

    /* each of these enumerators represents the gist of what
       a server message describes */
    enum minecraft_server_message_gist
    {
        gist_server_generic,
        gist_server_start,
        gist_server_start_bind,
        gist_server_chat,
        gist_server_secret_chat,
        gist_server_shutdown,
        gist_player_id,
        gist_player_login,
        gist_player_join,
        gist_player_losecon_logout,
        gist_player_losecon_error,
        gist_player_leave,
        gist_player_chat,
        gist_player_achievement
    };

    /* represents a message printed on the minecraft server process's standard output channel */
    class minecraft_server_message
    {
    public:
        /* turns the specified line of server text into a minecraft_server_message object; returns
           a pointer to the dynamically allocated object; it will need to be deleted later by the callee */
        static minecraft_server_message* generate_message(const rtypes::str& serverLineText);

        bool good() const
        { return _good; }
        minecraft_server_message_type get_type() const
        { return _type; }
        minecraft_server_message_gist get_gist() const
        { return _gist; }
        rtypes::str get_type_string() const;
        rtypes::str get_gist_string() const;
        rtypes::uint32 get_hour() const
        { return _hour; }
        rtypes::uint32 get_minute() const
        { return _minute; }
        rtypes::uint32 get_second() const
        { return _second; }
        rtypes::str get_payload() const
        { return _payload; }
        rtypes::str get_original_payload() const
        { return _originalPayload; }
        const char* get_format_string() const
        { return _formatString; }
        rtypes::size_type get_token_count() const
        { return _tokens.size(); }
        const rtypes::str& get_token(rtypes::size_type at) const
        { return _tokens[at]; }
        const rtypes::str& operator[](rtypes::size_type i) const
        { return _tokens[i]; }
    protected:
        minecraft_server_message(const rtypes::str& originalPayload,
            const rtypes::str& payload,
            const rtypes::dynamic_array<rtypes::str>& tokens,
            const char* formatString,
            const rtypes::str& timeMessage,
            const rtypes::str& typeMessage,
            minecraft_server_message_gist messageGist);

        static bool _payloadParse(const char* format,const char* source,rtypes::dynamic_array<rtypes::str>& tokens);
    private:
        bool _good;
        minecraft_server_message_type _type;
        minecraft_server_message_gist _gist;
        rtypes::uint32 _hour, _minute, _second;
        rtypes::str _originalPayload, _payload;
        rtypes::dynamic_array<rtypes::str> _tokens;
        const char* _formatString;
    };

    /* represents an object that authoritatively manages
     * the server (either directly or indirectly) via
     * messages received from/sent to the minecraft server
     * process's standard io channels; this class provides
     * the base functionality for any kind of minecontrol
     * authority
     */
    class minecontrol_authority
    {
    public:
        enum console_result
        {
            console_communication_finished, // the client successfully negotiated with the authority control and closed the console mode
            console_communication_terminated, // the client successfully negotiated with the authority control but was shutdown by the authority
            console_no_channel, // the authority was not ready to enter console mode
            console_channel_busy
        };
        enum execute_result
        {
            authority_exec_okay, // the executable program was successfully executed
            authority_exec_okay_exited, // the executable program was successfully executed
            authority_exec_cannot_run, // the process couldn't fork or exec failed in some way other than not finding the program
            authority_exec_access_denied, // the user didn't have execute permission for the executable program
            authority_exec_attr_fail, // the correct environment for the child process couldn't be applied
            authority_exec_not_program, // the specified file wasn't in an executable format
            authority_exec_program_not_found, // the executable program was not found in the proper directories
            authority_exec_too_many_arguments, // too many arguments supplied to program
            authority_exec_too_many_running, // too many executable programs are already running under the authority
            authority_exec_not_ready, // the authority object is not ready to run executable programs at this time (most likely the Minecraft server shutdown)
            authority_exec_unspecified // the failure reason is undocumented
        };

        minecontrol_authority(const pipe& ioChannel,const rtypes::str& serverDirectory,const user_info& login);
        ~minecontrol_authority();

        console_result client_console_operation(socket& clientChannel); // blocks
        void issue_command(const rtypes::str& commandLine);
        execute_result run_executable(rtypes::str commandLine,int* ppid = NULL);

        bool is_responsive() const; // determine if server process is still responsive

        static const char* const AUTHORITY_EXEC_FILE;
    private:
        static const char* const AUTHORITY_EXE_PATH;
        static const int ALLOWED_CHILDREN = 10;
        static void* processing(void*); // thread handler for message processing/message output to logged-in client or child processes

        pipe _iochannel; // IO channel to Minecraft server Java process (read/write enabled)
        rtypes::str _serverDirectory; // directory of server files that the authority manages; becomes the current working directory for the child authority process
        user_info _login; // login information for user running minecraft server
        socket* _clientchannel; // socket used for client communications using minecontrol protocol; NULL if client is not registered
        pipe _childStdIn[ALLOWED_CHILDREN]; // write-only pipe (in parent) to child stdin
        rtypes::int32 _childID[ALLOWED_CHILDREN]; // parallel array of child process ids
        int _childCnt; // maintain a count of child processes (for convenience)
        mutex _childMtx, _clientMtx; // control cross-thread access
        mutable rtypes::ulong _threadID; // processing threadID
        volatile bool _threadCondition;
        volatile bool _consoleEnabled;

        static bool _prepareArgs(char* commandLine,const char** outProgram,const char** outArgv,int size);
    };

    rtypes::rstream& operator <<(rtypes::rstream&,minecontrol_authority::execute_result);
}

#endif
