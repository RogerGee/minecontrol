// minecontrol-authority.cpp
#include "minecontrol-authority.h"
#include <rlibrary/rutility.h>
#include <cstdio>
#include <pthread.h>
#include <unistd.h>
#include "minecontrol-protocol.h"
using namespace rtypes;
using namespace minecraft_controller;

/*static*/ minecraft_server_message* minecraft_server_message::generate_message(const str& serverLineText)
{
    /* the parallel arrays MESSAGE_GISTS and GIST_FORMATS define the search
       order for message patters (formats); they are arranged (hopefully) by
       most frequent message gist kinds first; if you need to add a gist kind,
       make sure to update both arrays */
    static const char* const GENERAL_FORMAT = "[%S] [%S]: %S";
    static const uint32 MESSAGE_GIST_COUNT = 13;
    static const minecraft_server_message_gist MESSAGE_GISTS[] = {
        gist_player_chat,
        gist_server_chat,
        gist_player_login,
        gist_player_id,
        gist_player_join,
        gist_player_leave,
        gist_player_losecon_logout,
        gist_player_achievement,
        gist_server_start,
        gist_server_start_bind,
        gist_server_shutdown,
        gist_player_losecon_error,
        gist_server_generic // base case; let this be the last element in this array
    };
    static const char* const GIST_FORMATS[] = {
        "<%S> %S", // gist_player_chat
        "[%S] %S", // gist_server_chat
        "%S[/%S] logged in with entity id %S at (%S, %S, %S)", // gist_player_login
        "UUID of player %S is %S", // gist_player_id
        "%S joined the game", // gist_player_join
        "%S left the game", // gist_player_leave
        "%S lost connection: TextComponent{%S, %S, style=Style{%S, %S, %S, %S, %S, %S, %S, %S}}", // gist_player_losecon_logout
        "%S has just earned the achievement [%S]", // gist_player_achievement
        "Starting minecraft server version %S", // gist_server_start
        "Starting Minecraft server on %S", // gist_server_start_bind
        "Stopping server", // gist_server_shutdown
        "com.mojang.authlib.GameProfile@%S[id=%S,name=%S,properties=%S,legacy=%S] (/%S) lost connection: Disconnected", // gist_player_losecon_error
        "%S", // gist_server_generic (this is the base case; let this be the last element in this array)
    };
    dynamic_array<str> tokens;
    if (!_payloadParse(GENERAL_FORMAT,serverLineText.c_str(),tokens) || tokens.size()<3)
        return NULL;
    str time(tokens[0]), type(tokens[1]), payload(tokens[2]);
    uint32 i = 0;
    while (i < MESSAGE_GIST_COUNT) {
        tokens.clear();
        if ( _payloadParse(GIST_FORMATS[i],payload.c_str(),tokens) )
            return new minecraft_server_message(serverLineText,payload,tokens,GIST_FORMATS[i],time,type,MESSAGE_GISTS[i]);
        ++i;
    }
    return NULL;
}
minecraft_server_message::minecraft_server_message(const str& originalPayload,const str& payload,const dynamic_array<str>& tokens,const char* formatString,const str& timeMessage,const str& typeMessage,minecraft_server_message_gist messageGist)
    : _gist(messageGist), _originalPayload(originalPayload), _payload(payload), _tokens(tokens), _formatString(formatString)
{
    _good = true;
    if (sscanf(timeMessage.c_str(),"%u:%u:%u",&_hour,&_minute,&_second) != 3)
        _good = false;
    else if (typeMessage == "Server thread/INFO")
        _type = main_info;
    else if (typeMessage == "Server thread/WARN")
        _type = main_warn;
    else if ( rutil_strncmp(typeMessage.c_str(),"User Authenticator",18) ) {
        const char* p = typeMessage.c_str();
        while (*p && *p!='/')
            ++p;
        if (*p == '/') {
            ++p;
            if ( rutil_strcmp(p,"WARN") )
                _type = auth_warn;
            else if ( rutil_strcmp(p,"INFO") )
                _type = auth_info;
        }
    }
    else if (typeMessage == "Server Shutdown Thread/INFO")
        _type = shdw_info;
    else if (typeMessage == "Server Shutdown Thread/WARN")
        _type = shdw_warn;
    else
        _type = type_unkn;
}
str minecraft_server_message::get_type_string() const
{
    static const char* const DEFAULT = "type-unknown";
    switch (_type) {
    case main_info:
        return "main-info";
    case main_warn:
        return "main-warning";
    case auth_info:
        return "auth-info";
    case auth_warn:
        return "auth-warning";
    case shdw_info:
        return "shutdown-info";
    case shdw_warn:
        return "shutdown-warning";
    default:
        return DEFAULT;
    }
    return DEFAULT;
}
str minecraft_server_message::get_gist_string() const
{
    static const char* const DEFAULT = "type-unknown";
    switch (_gist) {
    case gist_server_start:
        return "start";
    case gist_server_start_bind:
        return "bind";
    case gist_server_chat:
        return "server-chat";
    case gist_server_shutdown:
        return "shutdown";
    case gist_player_id:
        return "player-id";
    case gist_player_login:
        return "login";
    case gist_player_join:
        return "join";
    case gist_player_losecon_logout:
        return "logout-connection";
    case gist_player_losecon_error:
        return "lost-connection";
    case gist_player_leave:
        return "leave";
    case gist_player_chat:
        return "chat";
    case gist_player_achievement:
        return "achievement";
    default:
        return DEFAULT;
    }
    return DEFAULT;
}
/*static*/ bool minecraft_server_message::_payloadParse(const char* format,const char* source,dynamic_array<str>& tokens)
{
    str token;
    while (*format && *source) {
        if (*format == '%') {
            ++format;
            if (*format == 0)
                return false; // bad format string
            if (*format == '%') {
                // escaped percent sign character
                if (*source != *format)
                    return false;
            }
            else {
                token.clear();
                if (*format == 'S') {
                    // find token until delimiter character
                    char delim;
                    ++format;
                    delim = *format;
                    while (*source && *source!=delim) {
                        token.push_back(*source);
                        ++source;
                    }
                    if (*source != delim)
                        return false;
                }
                else if (*format == 's') {
                    // find token until whitespace
                    while (*source && *source!=' ' && *source!='\t' && *source!='\n') {
                        token.push_back(*source);
                        ++source;
                    }
                }
                else
                    throw minecontrol_authority_error(); // bad format character
                tokens.push_back(token);
            }
            // seek to next character if possible
            if (*format)
                ++format;
            else if (!*source)
                return true; // string matched
            if (*source)
                ++source;
            continue;
        }
        if (*format != *source)
            return false;
        ++format;
        ++source;
    }
    return *format == *source;
}

minecontrol_authority::minecontrol_authority(const pipe& ioChannel)
    : _iochannel(ioChannel)
{
    _threadCond = false;

}
minecontrol_authority::~minecontrol_authority()
{
    // end processing just in case
    end_processing();
}
void minecontrol_authority::begin_processing()
{
}
void minecontrol_authority::end_processing()
{
}
