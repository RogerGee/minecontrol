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
        standardLog << '{' << pnew->connection.get_device().get_accept_id() << "} accepted client connection" << endline;
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
    uid = -1;
    guid = -1;
    referenceIndex = 0;
}
/*static*/ void* controller_client::client_thread(void* pclient)
{
    controller_client* client = reinterpret_cast<controller_client*>(pclient);
    domain_socket& clientSock = client->connection.get_device();
    // authenticate the client
    if ( !client->authenticate() )
    {
        if (clientSock.get_last_operation_status() != success_read)
            standardLog << '{' << clientSock.get_accept_id() << "} client disconnected before authentication" << endline;
        else
        {
            client->connection << PROGRAM_NAME << ": authentication failure" << endline;
            standardLog << '{' << clientSock.get_accept_id() << "} client failed authentication" << endline;
        }
    }
    else
    {
        client->connection << PROGRAM_NAME << ": successful login\n" << PROGRAM_NAME << '/' << PROGRAM_VERSION << endline;
        standardLog << '{' << clientSock.get_accept_id() << "} client authenticated successfully" << endline;
        // enter into communication with the client;
        // if message_loop() returns false, then the
        // client should be disconnected from this end
        if ( !client->message_loop() )
        {
            clientSock.shutdown();
            standardLog << '{' << clientSock.get_accept_id() << "} client connection shutdown by server" << endline;
        }
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
bool controller_client::authenticate()
{
    // ask the client to authenticate themselves
    str username, password;
    connection >> username >> password;
    if (connection.get_device().get_last_operation_status() != success_read)
        return false;
    // obtain user information
    passwd* pwd;
    spwd* shadowPwd;
    const char* pencrypt;
    pwd = getpwnam( username.c_str() );
    if (pwd == NULL)
    {
        standardLog << "bad username on authentication: " << username << endline;
        return false;
    }
    shadowPwd = getspnam( username.c_str() );
    if (shadowPwd==NULL && errno==EACCES)
    {
        standardLog << "warning: server doesn't have permission to authenticate user" << endline;
        return false;
    }
    pwd->pw_passwd = shadowPwd->sp_pwdp;
    if ((pencrypt = ::crypt(password.c_str(),pwd->pw_passwd)) == NULL)
        throw controller_client_error();
    if ( !rutil_strcmp(pencrypt,pwd->pw_passwd) )
    {
        standardLog << "bad password on username: " << username << endline;
        return false;
    }
    uid = pwd->pw_uid;
    guid = pwd->pw_gid;
    homedir = pwd->pw_dir;
    return true;
}
bool controller_client::message_loop()
{
    domain_socket& sock = connection.get_device();
    while (threadCondition)
    {
        rstringstream lineStream;
        connection.getline( lineStream.get_device() );
        if (sock.get_last_operation_status() == no_input) // client disconnected
        {
            standardLog << '{' << sock.get_accept_id() << "} client disconnect" << endline;
            return true;
        }
        else if (sock.get_last_operation_status() == bad_read) // input error; assume disconnect
        {
            standardLog << '{' << sock.get_accept_id() 
                        << "} read error: client disconnect" << endline;
            return true;
        }
        str word;
        lineStream >> word;
        rutil_to_lower_ref(word);
        if (word == "start")
            command_start(lineStream);
        else if (word == "status")
            command_status(lineStream);
        else if (word == "stop")
            command_stop(lineStream);
        else
            connection << PROGRAM_NAME << ": command `" << word << "' is not understood" << endline;
    }
    return false;
}
void controller_client::command_start(rstream& stream)
{
    str s;
    server_handle* handle;
    minecraft_server_info* pinfo;
    minecraft_server::minecraft_server_start_condition cond;
    // attempt to read arguments
    stream >> s;
    if (rutil_to_lower(s) == "create")
    {
        pinfo = new minecraft_server_info_ex;
        stream >> pinfo->internalName;
        pinfo->isNew = true;
        pinfo->read_props(stream,connection); // read and parse options
    }
    else
    {
        pinfo = new minecraft_server_info;
        pinfo->internalName = s;
    }
    if (pinfo->internalName.length() == 0)
    {
        connection << PROGRAM_NAME << ": bad command syntax: `start [create] server-name [options]'" << endline;
        delete pinfo;
        return;
    }
    pinfo->uid = uid;
    pinfo->guid = guid;
    pinfo->homeDirectory = homedir.c_str();
    // attempt to start the minecraft server
    handle = minecraft_server_manager::allocate_server();
    handle->set_clientid( connection.get_device().get_accept_id() );
    cond = handle->pserver->begin(*pinfo);
    standardLog << '{' << connection.get_device().get_accept_id() << "} "
                << cond << endline;
    connection << PROGRAM_NAME << ": " << cond << endline;
    minecraft_server_manager::attach_server(handle);
    delete pinfo;
}
void controller_client::command_status(rstream&)
{
    if ( !minecraft_server_manager::print_servers(connection) )
        connection << "No servers currently running under this manager";
    connection << endline;
}
void controller_client::command_stop(rstream&)
{
}
