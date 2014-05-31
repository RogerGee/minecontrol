// minecontrol-client.cpp
#define _XOPEN_SOURCE // get 'crypt'
#include "minecontrol-client.h"
#include "minecraft-controller.h"
#include "minecraft-server.h"
#include <unistd.h>
#include <pwd.h>
#include <shadow.h>
#include <errno.h>
#include <rlibrary/rutility.h>
using namespace rtypes;
using namespace minecraft_controller;

/*static*/ mutex controller_client::clientsMutex;
/*static*/ dynamic_array<void*> controller_client::clients;
/*static*/ size_type controller_client::CMD_COUNT_WITHOUT_LOGIN = 2;
/*static*/ size_type controller_client::CMD_COUNT_WITH_LOGIN = 5;
/*static*/ size_type controller_client::CMD_COUNT_WITH_PRIVALEGED_LOGIN = 1;
/*static*/ const char* const controller_client::CMDNAME_WITHOUT_LOGIN[] =
{
    "login", "status"
};
/*static*/ const controller_client::command_call controller_client::CMDFUNC_WITHOUT_LOGIN[] =
{
    &controller_client::command_login, &controller_client::command_status
};
/*static*/ const char* const controller_client::CMDNAME_WITH_LOGIN[] =
{
    "start", "stop",
    "logout", "console",
    "extend"
};
/*static*/ const controller_client::command_call controller_client::CMDFUNC_WITH_LOGIN[] =
{
    &controller_client::command_start, &controller_client::command_stop,
    &controller_client::command_logout, &controller_client::command_console,
    &controller_client::command_extend
};
/*static*/ const char* const controller_client::CMDNAME_WITH_PRIVALEGED_LOGIN[] =
{
    "shutdown"
};
/*static*/ const controller_client::command_call controller_client::CMDFUNC_WITH_PRIVALEGED_LOGIN[] =
{
    &controller_client::command_shutdown
};
/*static*/ controller_client* controller_client::accept_client(socket& ds)
{
    socket* pclientsock;
    // accept a client; stores a dynamically allocated socket object in pnew->sock; this will
    // be passed to the controller_client object which will later free it
    if (ds.accept(pclientsock) == socket_accepted) {
        controller_client* pnew = new controller_client(pclientsock);
        // send the client to a new thread for processing
        if (::pthread_create(&pnew->threadID,NULL,&controller_client::client_thread,pnew) != 0)
            throw controller_client_error();
        // detach the thread
        if (::pthread_detach(pnew->threadID) != 0)
            throw controller_client_error();
        // add the client reference to the list of maintained clients
        clientsMutex.lock();
        while (pnew->referenceIndex<clients.size() && clients[pnew->referenceIndex]!=NULL)
            ++pnew->referenceIndex;
        if (pnew->referenceIndex < clients.size())
            clients[pnew->referenceIndex] = pnew;
        else
            clients.push_back(pnew);
        clientsMutex.unlock();
        if (pclientsock->get_family() == socket_family_unix)
            pnew->client_log(minecontrold::standardLog) << "accepted local client connection" << endline;
        else if (pclientsock->get_family() == socket_family_inet)
            pnew->client_log(minecontrold::standardLog) << "accepted remote client connection" << endline;
        else
            throw controller_client_error();
        return pnew;
    }
    // the accept failed
    return NULL;
}
/*static*/ void controller_client::shutdown_clients()
{
    clientsMutex.lock();
    for (size_type i = 0;i<clients.size();i++) {
        controller_client* cl = reinterpret_cast<controller_client*>(clients[i]);
        if (cl!=NULL && cl->sock!=NULL) {
            if (cl->sock != NULL) {
                cl->sock->shutdown();
                cl->sock->close();
            }
            cl->threadCondition = false;
        }
    }
    clientsMutex.unlock();
}
/*static*/ void controller_client::close_client_sockets()
{
    // I assume that this is called from a single-threaded forked context
    // thus I don't attempt to aquire the clientsMutex lock
    for (size_type i = 0;i<clients.size();++i)
	if (clients[i] != NULL)
	    reinterpret_cast<controller_client*>(clients[i])->sock->close();
}
controller_client::controller_client(socket* acceptedSocket)
{
    if (acceptedSocket == NULL)
        throw controller_client_error();
    sock = acceptedSocket;
    threadID = -1;
    threadCondition = true;
    uid = -1; // means "not logged in"
    guid = -1;
    referenceIndex = 0;
}
controller_client::~controller_client()
{
    // this object's socket was dynamically allocated
    // in another context
    if (sock != NULL)
        delete sock;
}
/*static*/ void* controller_client::client_thread(void* pclient)
{
    controller_client* client = reinterpret_cast<controller_client*>(pclient);
    // enter into communication with the client;
    // if message_loop() returns false, then the
    // client should be disconnected from this end
    client->connection.assign(*client->sock);
    if ( !client->message_loop() ) {
        client->client_log(minecontrold::standardLog) << "client connection shutdown by server" << endline;
        client->sock->shutdown();
    }
    // remove client reference from the list of maintained clients
    clientsMutex.lock();
    clients[client->referenceIndex] = NULL;
    clientsMutex.unlock();
    // this thread is responsible for the dynamically allocated
    // memory used to create the controller client object
    delete client;
    return NULL;
}
bool controller_client::message_loop()
{
    io_device& device = connection.get_device(); // should be a reference to this->sock
    minecontrol_message inMessage, outMessage;
    // perform greeting negotiation: the client must send HELLO as it's first message;
    // anything else (including a malformed message) results in a shutdown
    connection >> inMessage;
    if ( !inMessage.is_command("hello") ) {
        client_log(minecontrold::standardLog) << "client didn't say HELLO" << endline;
        prepare_error() << "Protocol error: expected HELLO command to server" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    client_log(minecontrold::standardLog) << "client said HELLO" << endline;
    outMessage.assign_command("GREETINGS");
    outMessage.add_field("Name",minecontrold::get_server_name());
    outMessage.add_field("Version",minecontrold::get_server_version());
    outMessage.add_field("EncryptKey",crypto.get_public_key().c_str());
    connection << outMessage;
    while (threadCondition) {
        // get next message from client
        connection >> inMessage;
        if (device.get_last_operation_status() == no_input) { // client disconnected
            if ( !device.is_valid_context() )
                return false;
            client_log(minecontrold::standardLog) << "client disconnect" << endline;
            return true;
        }
        else if (device.get_last_operation_status() == bad_read) { // input error; assume disconnect
            if ( !device.is_valid_context() )
                return false;
            client_log(minecontrold::standardLog) << "read error: client disconnect" << endline;
            return true;
        }
        // process message
        bool executed = false;
        // attempt to process commands that can be executed without login
        for (size_type i = 0;i<CMD_COUNT_WITHOUT_LOGIN;i++) {
            if ( inMessage.is_command(CMDNAME_WITHOUT_LOGIN[i]) ) {
                (this->*CMDFUNC_WITHOUT_LOGIN[i])(inMessage.get_field_key_stream(),inMessage.get_field_value_stream());
                executed = true;
                break;
            }
        }
        if (!executed) {
            // attempt to process commands that can only be executed with login status
            for (size_type i = 0;i<CMD_COUNT_WITH_LOGIN;i++) {
                if ( inMessage.is_command(CMDNAME_WITH_LOGIN[i]) ) {
                    if (uid < 0) {
                        prepare_error() << "Permission denied: '" << inMessage.get_command() << "' command requires authentication" << flush;
                        connection << msgbuf.get_message();
                    }
                    else
                        (this->*CMDFUNC_WITH_LOGIN[i])(inMessage.get_field_key_stream(),inMessage.get_field_value_stream());
                    executed = true;
                    break;
                }
            }
            if (!executed) {
                // attempt to process commands that can only be executed with privaleged login (root) status
                for (size_type i = 0;i<CMD_COUNT_WITH_PRIVALEGED_LOGIN;i++) {
                    if ( inMessage.is_command(CMDNAME_WITH_PRIVALEGED_LOGIN[i]) ) {
                        if (uid != 0) {
                            prepare_error() << "Permission denied: '" << inMessage.get_command() << "' command requires privaleged (root) authentication" << flush;
                            connection << msgbuf.get_message();
                        }
                        else
                            (this->*CMDFUNC_WITH_PRIVALEGED_LOGIN[i])(inMessage.get_field_key_stream(),inMessage.get_field_value_stream());
                        executed = true;
                        break;
                    }
                }
                if (!executed) {
                    prepare_error() << "Command '" << inMessage.get_command() << "' is not recognized" << flush;
                    connection << msgbuf.get_message();
                }
            }
        }
    }
    return false;
}
bool controller_client::command_login(rstream& kstream,rstream& vstream) // handles 'login' requests
{
    if (uid != -1) {
        prepare_error() << "Log in status is already confirmed; please log out to log in again" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    str key;
    str username, password;
    // read fields from message stream
    while (kstream >> key) {
        if (key == "username")
            vstream >> username;
        else if (key == "password")
            vstream >> password;
        // ignore anything else
    }
    // check fields
    if (username.length() == 0) {
        prepare_error() << "Required parameter 'username' missing" << flush;
        connection << msgbuf.get_message();
        client_log(minecontrold::standardLog) << "authentication failure: client didn't supply username" << endline;
        return false;
    }
    if (password.length() == 0) {
        prepare_error() << "Required parameter 'password' missing" << flush;
        connection << msgbuf.get_message();
        client_log(minecontrold::standardLog) << "authentication failure: client didn't supply password" << endline;
        return false;
    }
    // decrypt the password
    str decryptedPassword;
    if ( !crypto.decrypt(password.c_str(),decryptedPassword) ) {
        prepare_error() << "Password decryption failed" << flush;
        connection << msgbuf.get_message();
        client_log(minecontrold::standardLog) << "authentication failure: client password could not be decrypted" << endline;
        return false;
    }
    // obtain user information
    passwd* pwd;
    spwd* shadowPwd;
    const char* pencrypt;
    pwd = getpwnam( username.c_str() );
    if (pwd == NULL) {
        prepare_error() << "Authentication failure: bad username" << flush;
        connection << msgbuf.get_message();
        client_log(minecontrold::standardLog) << "authentication failure: bad username '" << username << '\'' << endline;
        return false;
    }
    shadowPwd = getspnam( username.c_str() );
    if (shadowPwd==NULL && errno==EACCES) {
        prepare_error() << "This server cannot log you in because it is not priviledged; this is most likely a mistake; contact your SA" << flush;
        connection << msgbuf.get_message();
        minecontrold::standardLog << "!!warning!!: server doesn't have permission to authenticate user; is this process priviledged?" << endline;
        return false;
    }
    pwd->pw_passwd = shadowPwd->sp_pwdp;
    if ((pencrypt = ::crypt(decryptedPassword.c_str(),pwd->pw_passwd)) == NULL)
        throw controller_client_error();
    if ( !rutil_strcmp(pencrypt,pwd->pw_passwd) ) {
        prepare_error() << "Authentication failure: bad password for user '" << username << '\'' << flush;
        connection << msgbuf.get_message();
        client_log(minecontrold::standardLog) << "authentication failure: bad password for user '" << username << '\'' << endline;
        return false;
    }
    uid = pwd->pw_uid;
    guid = pwd->pw_gid;
    homedir = pwd->pw_dir;
    prepare_message() << "Authentication complete: logged in as '" << username << '\'' << flush;
    connection << msgbuf.get_message();
    client_log(minecontrold::standardLog) << "client authenticated successfully as '" << username << '\'' << endline;
    return true;
}
bool controller_client::command_logout(rstream&,rstream&)
{
    uid = -1;
    guid = -1;
    homedir.clear();
    prepare_message() << "Successful logout" << flush;
    connection << msgbuf.get_message();
    client_log(minecontrold::standardLog) << "client was logged-out of authentication mode" << endline;
    return true;
}
bool controller_client::command_start(rstream& kstream,rstream& vstream)
{
    str key;
    bool isNew = false; str serverName; str props;
    stringstream errors;
    // attempt to read field list from message
    while (kstream >> key) {
        if (key == "isnew")
            vstream >> isNew;
        else if (key == "servername")
            vstream >> serverName;
        else {
            // add assumed property of format: name=value\n
            props += key;
            vstream >> key;
            props.push_back('=');
            props += key;
            props.push_back('\n');
        }
    }
    if (serverName.length() == 0) {
        prepare_error() << "No server name specified" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    minecraft_server_info info(isNew,serverName.c_str(),homedir.c_str(),uid,guid);
    info.read_props(props,errors); // read and parse options
    // attempt to start the minecraft server
    server_handle* handle;
    handle = minecraft_server_manager::allocate_server();
    handle->set_clientid( sock->get_accept_id() );
    auto cond = handle->pserver->begin(info);
    client_log(minecontrold::standardLog) << cond << endline;
    if ( errors.has_input() )
        (cond==minecraft_server::mcraft_start_success ? prepare_list_message() : prepare_list_error()) << cond << newline << errors << flush;
    else
        (cond==minecraft_server::mcraft_start_success ? prepare_message() : prepare_error()) << cond << flush;
    connection << msgbuf.get_message();
    minecraft_server_manager::attach_server(handle);
    return true;
}
bool controller_client::command_status(rstream&,rstream&)
{
    rstream& msg = prepare_list_message();
    minecraft_server_manager::print_servers(msg,uid,guid);
    msg.flush_output();
    connection << msgbuf.get_message();
    return true;
}
bool controller_client::command_extend(rstream& kstream,rstream& vstream)
{
    str key;
    uint32 id = -1;
    uint32 hours = 0;
    dynamic_array<server_handle*> servers;
    minecraft_server_manager::auth_lookup_result result;
    // read off needed properties
    while (kstream >> key) {
        if (key == "serverid") {
            vstream >> id;
            if (!vstream.get_input_success() || id==0) {
                prepare_error() << "Bad id value specified; expected a positive non-zero integer" << flush;
                connection << msgbuf.get_message();
                return false;
            }
        }
        else if (key == "amount") {
            vstream >> hours;
            if (!vstream.get_input_success() || hours==0) {
                prepare_error() << "Bad extend amount specified; expected a positive integer" << flush;
                connection << msgbuf.get_message();
                return false;
            }
        }
    }
    // handle missing fields
    if (id == uint32(-1)) {
        prepare_error() << "No running server id was specified" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    if (hours == 0) {
        prepare_error() << "No extend time amount was specified" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    // ask the server manager for handles to running servers that the user can access; we need to make sure no blocking calls happen here (shouldn't be a problem)
    if ((result = minecraft_server_manager::lookup_auth_servers(uid,guid,servers)) == minecraft_server_manager::auth_lookup_none) {
        prepare_error() << "There are no active servers running to stop" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    else if (result == minecraft_server_manager::auth_lookup_no_owned) {
        prepare_error() << "User does not have permission to access any of the running servers" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    // search through accessible servers; find the one with the specified id
    size_type i = 0;
    while (i<servers.size() && servers[i]->pserver->get_internal_id()!=id)
        ++i;
    if (i >= servers.size()) {
        // return regulation of minecraft server(s) to the manager
        minecraft_server_manager::attach_server(&servers[0],servers.size());
        prepare_error() << "No owned server was found with id=" << id << "; user may not have permission to modify it" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    servers[i]->pserver->extend_time_limit(hours);
    prepare_message() << "Time limit for server '" << servers[i]->pserver->get_internal_name() << "' was extended" << flush;
    connection << msgbuf.get_message();
    // give control of minecraft server(s) back to the manager
    minecraft_server_manager::attach_server(&servers[0],servers.size());
    return true;
}
bool controller_client::command_stop(rstream& kstream,rstream& vstream)
{
    str key;
    uint32 id = -1;
    dynamic_array<server_handle*> servers;
    minecraft_server_manager::auth_lookup_result result;
    // read off needed property
    while (kstream >> key) {
        if (key == "serverid") {
            vstream >> id;
            if (!vstream.get_input_success() || id==0) {
                prepare_error() << "Bad id value specified; expected a positive non-zero integer" << flush;
                connection << msgbuf.get_message();
                return false;
            }
        }
    }
    if (id == uint32(-1)) {
        prepare_error() << "No running server id specified" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    // ask the server manager for handles to running servers that the user can access; we need to make sure no blocking calls happen here (shouldn't be a problem)
    if ((result = minecraft_server_manager::lookup_auth_servers(uid,guid,servers)) == minecraft_server_manager::auth_lookup_none) {
        prepare_error() << "There are no active servers running to stop" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    else if (result == minecraft_server_manager::auth_lookup_no_owned) {
        prepare_error() << "User does not have permission to access any of the running servers" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    // search through accessible servers; find the one with the specified id
    size_type i = 0;
    while (i<servers.size() && servers[i]->pserver->get_internal_id()!=id)
        ++i;
    if (i >= servers.size()) {
        // return regulation of minecraft server(s) to the manager
        minecraft_server_manager::attach_server(&servers[0],servers.size());
        prepare_error() << "No owned server was found with id=" << id << "; user may not have permission to modify it" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    auto status = servers[i]->pserver->end();
    prepare_message() << status << flush;
    connection << msgbuf.get_message();
    // use the original id in log message to map closing server
    // to the client session that started it
    minecontrold::standardLog << '{' << servers[i]->get_clientid() << "} " << status << endline;
    // return regulation of minecraft server(s) to the manager
    minecraft_server_manager::attach_server(&servers[0],servers.size());
    return true;
}
bool controller_client::command_console(rstream& kstream,rstream& vstream)
{
    str key;
    uint32 id = -1;
    dynamic_array<server_handle*> servers;
    minecraft_server_manager::auth_lookup_result result;
    // read off needed property
    while (kstream >> key)
        if (key == "serverid")
            vstream >> id;
    if (id == uint32(-1)) {
        prepare_message() << "No running server id specified" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    // ask the server manager for handles to running servers that the user can access; we need to make sure no blocking calls happen here (shouldn't be a problem)
    if ((result = minecraft_server_manager::lookup_auth_servers(uid,guid,servers)) == minecraft_server_manager::auth_lookup_none) {
        prepare_message() << "There are no active servers running to stop" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    else if (result == minecraft_server_manager::auth_lookup_no_owned) {
        prepare_error() << "User does not have permission to access any of the running servers" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    // search through accessible servers; find the one with the specified id
    size_type i = 0;
    while (i<servers.size() && servers[i]->pserver->get_internal_id()!=id)
        ++i;
    if (i >= servers.size()) {
        // return regulation of minecraft server(s) to the manager
        minecraft_server_manager::attach_server(&servers[0],servers.size());
        prepare_error() << "No owned server was found with id=" << id << "; user may not have permission to modify it" << flush;
        connection << msgbuf.get_message();
        return false;
    }

    return true;
}
bool controller_client::command_shutdown(rstream&,rstream&)
{
    prepare_message() << "Server '" << minecontrold::get_server_name() << "' is shutting down" << flush;
    connection << msgbuf.get_message();
    minecontrold::shutdown_minecontrold();
    return true;
}
inline
rstream& controller_client::client_log(rstream& stream)
{
    return stream << '{' << sock->get_accept_id() << "} ";
}
inline
rstream& controller_client::prepare_message()
{
    msgbuf.begin("MESSAGE");
    msgbuf.enqueue_field_name("Payload");
    return msgbuf;
}
inline
rstream& controller_client::prepare_error()
{
    msgbuf.begin("ERROR");
    msgbuf.enqueue_field_name("Payload");
    return msgbuf;
}
inline
rstream& controller_client::prepare_list_message()
{
    msgbuf.begin("LIST-MESSAGE");
    msgbuf.repeat_field("Item");
    return msgbuf;
}
inline
rstream& controller_client::prepare_list_error()
{
    msgbuf.begin("LIST-ERROR");
    msgbuf.repeat_field("Item");
    return msgbuf;
}
