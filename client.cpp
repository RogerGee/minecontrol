// client.cpp
#define _XOPEN_SOURCE // get `crypt'
#include "client.h"
#include <pwd.h>
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
    if (ds.accept(pnew->connection) == domain_socket::domain_socket_accepted)
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
        standardLog << '{' << pnew->connection.get_accept_id() << "} accepted client connection" << endline;
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
    // authenticate the client
    if ( !client->authenticate() )
    {
        client->connection.write("minecraft authority: authentication failure\n");
        standardLog << '{' << client->connection.get_accept_id() << "} client failed authentication" << endline;
        return NULL;
    }
    // enter into communication with the client;
    // if message_loop() returns false, then the
    // client should be disconnected from this end
    if ( !client->message_loop() )
    {
        client->connection.shutdown();
        standardLog << '{' << client->connection.get_accept_id() << "} client connection shutdown by server" << endline;
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
    // I let them be all powerful for now...
    // ....
    uid = 0;
    guid = 0;
    return true;
}
bool controller_client::message_loop()
{
    domain_socket_stream stream;
    stream.open(connection);
    while (threadCondition)
    {
        rstringstream lineStream;
        stream.getline( lineStream.get_device() );
        if (connection.get_last_operation_status() == no_input) // client disconnected
        {
            standardLog << '{' << connection.get_accept_id() << "} client disconnect" << endline;
            return true;
        }
        else if (connection.get_last_operation_status() == bad_read) // input error; assume disconnect
        {
            standardLog << '{' << connection.get_accept_id() 
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
            stream << "minecraft authority: bad command '" << word << '\'' << endline;
    }
    return false;
}
void controller_client::command_start(rstream&)
{
}
void controller_client::command_status(rstream&)
{
}
void controller_client::command_stop(rstream&)
{
}
