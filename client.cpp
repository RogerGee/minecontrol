// client.cpp
#define _XOPEN_SOURCE // get `crypt'
#include "client.h"
#include <unistd.h>
#include <pwd.h>
#include <shadow.h>
#include <errno.h>
#include "minecraft-controller.h"
#include "minecraft-server.h"
#include "rlibrary/rutility.h"
using namespace rtypes;
using namespace minecraft_controller;

/*static*/ mutex controller_client::clientsMutex;
/*static*/ dynamic_array<void*> controller_client::clients;
/*static*/ controller_client* controller_client::accept_client(domain_socket& ds)
{
    controller_client* pnew = new controller_client;
    if (ds.accept(pnew->connection.get_device()) == domain_socket::domain_socket_accepted)
    {
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
        pnew->client_log(standardLog) << "accepted client connection" << endline;
        return pnew;
    }
    // the accept failed
    delete pnew;
    return NULL;
}
/*static*/ void controller_client::shutdown_clients()
{
    clientsMutex.lock();
    for (size_type i = 0;i<clients.size();i++)
        if (clients[i] != NULL)
            reinterpret_cast<controller_client*>(clients[i])->threadCondition = false;
    clientsMutex.unlock();
}
controller_client::controller_client()
{
    threadID = -1;
    threadCondition = true;
    uid = -1; // means "not logged in"
    guid = -1;
    referenceIndex = 0;
}
/*static*/ void* controller_client::client_thread(void* pclient)
{
    controller_client* client = reinterpret_cast<controller_client*>(pclient);
    domain_socket& clientSock = client->connection.get_device();
    // enter into communication with the client;
    // if message_loop() returns false, then the
    // client should be disconnected from this end
    if ( !client->message_loop() )
    {
        clientSock.shutdown();
        client->client_log(standardLog) << "client connection shutdown by server" << endline;
    }
    // remove client reference from the list of maintained clients
    clientsMutex.lock();
    clients[client->referenceIndex] = NULL;
    clientsMutex.unlock();
    // this thread is responsible for the 
    // memory that the client occupies
    delete client;
    return NULL;
}
bool controller_client::message_loop()
{
    domain_socket& sock = connection.get_device();
    // send the client the name of this server
    connection << PROGRAM_NAME << '/' << PROGRAM_VERSION << endline;
    while (threadCondition)
    {
        // get message as line of text separated by newline
        rstringstream lineStream;
        connection.getline( lineStream.get_device() );
        if (sock.get_last_operation_status() == no_input) // client disconnected
        {
            client_log(standardLog) << "client disconnect" << endline;
            return true;
        }
        else if (sock.get_last_operation_status() == bad_read) // input error; assume disconnect
        {
            client_log(standardLog) << "read error: client disconnect" << endline;
            return true;
        }
        lineStream.delimit_whitespace(false);
        lineStream.add_extra_delimiter("|,");
        // process message
        str command;
        lineStream >> command;
        rutil_to_lower_ref(command);
        if (uid == -1) // user is not logged in
        {
            if (command == "login")
                command_login(lineStream);
            else
                send_message("error") << "Permission denied: please log in" << endline;
        }
        else // user is logged in
        {
            if (command == "start")
                command_start(lineStream);
            else if (command == "status")
                command_status(lineStream);
            else if (command == "stop")
                command_stop(lineStream);
            else
                send_message("error") << "Command `" << command << "' is not understood" << endline;
        }
    }
    return false;
}
bool controller_client::command_login(rtypes::rstream& stream) // handles `login' requests
{
    // read fields from message stream
    str username, password;
    stream >> username >> password;
    // check fields
    if (username.length() == 0)
    {
        send_message("error") << "Required parameter for username missing" << endline;
        client_log(standardLog) << "authentication failure: client didn't supply username" << endline;
        return false;
    }
    if (password.length() == 0)
    {
        send_message("error") << "Required parameter for password missing" << endline;
        client_log(standardLog) << "authentication failure: client didn't supply password" << endline;
        return false;
    }
    // obtain user information
    passwd* pwd;
    spwd* shadowPwd;
    const char* pencrypt;
    pwd = getpwnam( username.c_str() );
    if (pwd == NULL)
    {
        send_message("error") << "Authentication failure: bad username" << endline;
        client_log(standardLog) << "authentication failure: bad username `" << username << '\'' << endline;
        return false;
    }
    shadowPwd = getspnam( username.c_str() );
    if (shadowPwd==NULL && errno==EACCES)
    {
        send_message("error") << "Server error: the server cannot log you in" << endline;
        standardLog << "!!warning!!: server doesn't have permission to authenticate user; is this process priviledged?" << endline;
        return false;
    }
    pwd->pw_passwd = shadowPwd->sp_pwdp;
    if ((pencrypt = ::crypt(password.c_str(),pwd->pw_passwd)) == NULL)
        throw controller_client_error();
    if ( !rutil_strcmp(pencrypt,pwd->pw_passwd) )
    {
        send_message("error") << "Authentication failure: bad password for user `" << username << '\'' << endline;
        client_log(standardLog) << "authentication failure: bad password for user `" << username << '\'' << endline;
        return false;
    }
    uid = pwd->pw_uid;
    guid = pwd->pw_gid;
    homedir = pwd->pw_dir;
    send_message("message") << "Authentication complete: logged in as `" << username << '\'' << endline;
    client_log(standardLog) << "client authenticated successfully as `" << username << '\'' << endline;
    return true;
}

bool controller_client::command_start(rstream& stream)
{
    str s;
    minecraft_server_info* pinfo;
    // attempt to read arguments
    stream >> s;
    if (rutil_to_lower(s) == "create")
    {
        rstringstream errors;
        pinfo = new minecraft_server_info_ex;
        stream >> pinfo->internalName;
        pinfo->isNew = true;
        pinfo->read_props(stream,errors); // read and parse options
    }
    else
    {
        pinfo = new minecraft_server_info;
        pinfo->internalName = s;
    }
    if (pinfo->internalName.length() == 0)
    {
        send_message("error") << "Bad command syntax: usage: `start [create] server-name [options]'" << endline;
        delete pinfo;
        return false;
    }
    pinfo->uid = uid;
    pinfo->guid = guid;
    pinfo->homeDirectory = homedir.c_str();
    // attempt to start the minecraft server
    server_handle* handle;
    handle = minecraft_server_manager::allocate_server();
    handle->set_clientid( connection.get_device().get_accept_id() );
    auto cond = handle->pserver->begin(*pinfo);
    client_log(standardLog) << cond << endline;
    send_message("message") << cond << endline;
    minecraft_server_manager::attach_server(handle);
    delete pinfo;
    return true;
}
bool controller_client::command_status(rstream&)
{
    minecraft_server_manager::print_servers( send_message("message") );
    return true;
}
bool controller_client::command_stop(rstream&)
{
    return false;
}
inline
rstream& controller_client::client_log(rstream& stream)
{
    return stream << '{' << connection.get_device().get_accept_id() << "} ";
}
inline
rstream& controller_client::send_message(const char* command)
{
    return connection << command << '|';
}
