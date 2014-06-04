// minecontrol-authority.h
#ifndef MINECONTROL_AUTHORITY_H
#define MINECONTROL_AUTHORITY_H
#include "pipe.h"
#include <rlibrary/rdynarray.h>

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
        minecontrol_authority(const pipe& ioChannel);
        ~minecontrol_authority();

        void begin_processing();
        void end_processing();

        // client login functions:
        void login(const rtypes::io_device& clientChannel); // begin asyncronous coms with client under authority management; this function blocks until logout
        void force_logout(); // force the client out of the authority management protocol

        bool is_client_logged_in() const // determines if authority is under client management
        { return _clientchannel != NULL; }
        bool is_authority_managed() const; // determine if authority is under client or script management
        bool is_responsive() const; // determine if server process is still responsive
    private:
        static const rtypes::uint16 _idleTimeout; // (in seconds)
        static void* _processing(void*); // thread handler for message processing/message output to logged-in client
        static void* _clientinput(void*); // thread handler for client input; started upon client login

        /* the usage of these IO devices assumes atomic operation on normal read/write
           operations; thus they are used in multiple thread contexts without mutexes */
        pipe _iochannel; // IO channel to Minecraft server Java process (read/write enabled)
        pipe _childStdIn; // standard input channel to child authority process; optionally used
        rtypes::io_device* _clientchannel; // IO channel to logged-in client; data sent written/read must follow the MINECONTROL-PROTOCOL; NULL if no logged in client

        volatile bool _threadCond; // condition variable for _processing thread
        rtypes::ulong _threadID; // threadID for _processing thread
        rtypes::int32 _processID; // processID for child authority process; optionally used
    };
}

#endif
