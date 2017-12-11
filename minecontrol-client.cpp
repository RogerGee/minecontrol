// minecontrol-client.cpp
#define _XOPEN_SOURCE // get 'crypt'
#include "minecontrol-client.h"
#include "minecraft-controller.h"
#include "minecraft-server.h" // gets minecontrol-authority.h
#include <unistd.h>
#include <pwd.h>
#ifndef __APPLE__
#include <shadow.h>
#endif
#include <errno.h>
#include <rlibrary/rutility.h>
using namespace rtypes;
using namespace minecraft_controller;

/*static*/ mutex controller_client::clientsMutex;
/*static*/ dynamic_array<void*> controller_client::clients;
/*static*/ size_type controller_client::CMD_COUNT_WITHOUT_LOGIN = 2;
/*static*/ size_type controller_client::CMD_COUNT_WITH_LOGIN = 7;
/*static*/ size_type controller_client::CMD_COUNT_WITH_PRIVILEGED_LOGIN = 1;
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
    "extend", "exec",
    "auth-ls"
};
/*static*/ const controller_client::command_call controller_client::CMDFUNC_WITH_LOGIN[] =
{
    &controller_client::command_start, &controller_client::command_stop,
    &controller_client::command_logout, &controller_client::command_console,
    &controller_client::command_extend, &controller_client::command_exec,
    &controller_client::command_auth_ls
};
/*static*/ const char* const controller_client::CMDNAME_WITH_PRIVILEGED_LOGIN[] =
{
    "shutdown"
};
/*static*/ const controller_client::command_call controller_client::CMDFUNC_WITH_PRIVILEGED_LOGIN[] =
{
    &controller_client::command_shutdown
};

/*static*/ controller_client* controller_client::accept_client(socket& ds)
{
    str addr;
    socket* pclientsock;
    // accept a client; stores a dynamically allocated socket object in pnew->sock; this will
    // be passed to the controller_client object which will later free it
    if (ds.accept(pclientsock,addr) == socket_accepted) {
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
            // there is no address information for incoming unix domain clients
            pnew->client_log(minecontrold::standardLog) << "accepted local client connection"/*from" << addr */<< endline;
        else if (pclientsock->get_family() == socket_family_inet)
            pnew->client_log(minecontrold::standardLog) << "accepted remote client connection from " << addr << endline;
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
    //threadID = -1;
    threadCondition = true;
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
    str clientName, clientVersion;
    io_device& device = connection.get_device(); // should be a reference to this->sock
    minecontrol_message inMessage, outMessage;
    // perform greeting negotiation: the client must send HELLO (within 10 seconds) as its first message;
    // anything else (including a malformed message) results in a shutdown
    if ( !sock->select(10) ) { // wait 10 seconds for data to be sent from client
        client_log(minecontrold::standardLog) << "client didn't send anything in timeout period" << endline;
        return false;
    }
    connection >> inMessage;
    if ( !inMessage.is_command("hello") ) {
        client_log(minecontrold::standardLog) << "client didn't say HELLO" << endline;
        return false;
    }
    // get name and version info if available
    while ( inMessage.get_field_key_stream().has_input() ) {
        str k, v;
        inMessage.get_field_key_stream() >> k;
        inMessage.get_field_value_stream() >> v;
        if (k=="name" && clientName.length()==0)
            clientName = v;
        else if (k=="version" && clientVersion.length()==0)
            clientVersion = v;
    }
    if (clientName.length() == 0) {
        clientName = "unknown-client";
        clientVersion = "n/a";
    }
    client_log(minecontrold::standardLog) << "hello received from client '" << clientName << " version " << clientVersion << '\'' << endline;
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
        if ( !inMessage.good() ) {
            prepare_error() << "Bad message syntax" << flush;
            connection << msgbuf.get_message();
            continue;
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
                    if (userInfo.uid < 0) {
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
                // attempt to process commands that can only be executed with privileged login (root) status
                for (size_type i = 0;i<CMD_COUNT_WITH_PRIVILEGED_LOGIN;i++) {
                    if ( inMessage.is_command(CMDNAME_WITH_PRIVILEGED_LOGIN[i]) ) {
                        if (userInfo.uid != 0) {
                            prepare_error() << "Permission denied: '" << inMessage.get_command() << "' command requires privileged (root) authentication" << flush;
                            connection << msgbuf.get_message();
                        }
                        else
                            (this->*CMDFUNC_WITH_PRIVILEGED_LOGIN[i])(inMessage.get_field_key_stream(),inMessage.get_field_value_stream());
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
    if (userInfo.uid >= 0) {
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
    pwd = getpwnam( username.c_str() );
    if (pwd == NULL) {
        prepare_error() << "Authentication failure: bad username" << flush;
        connection << msgbuf.get_message();
        client_log(minecontrold::standardLog) << "authentication failure: bad username '" << username << '\'' << endline;
        return false;
    }
#ifndef __APPLE__
    spwd* shadowPwd;
    const char* pencrypt;
    shadowPwd = getspnam( username.c_str() );
    if (shadowPwd==NULL && errno==EACCES) {
        prepare_error() << "This server cannot log you in because it is not privileged; this is most likely a mistake; contact your SA" << flush;
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
#endif
    userInfo.uid = pwd->pw_uid;
    userInfo.gid = pwd->pw_gid;
    userInfo.homeDirectory = pwd->pw_dir;
    userInfo.userName = username;
    prepare_message() << "Authentication complete: logged in as '" << username << '\'' << flush;
    connection << msgbuf.get_message();
    client_log(minecontrold::standardLog) << "client authenticated successfully as '" << username << '\'' << endline;
    return true;
}

bool controller_client::command_logout(rstream&,rstream&)
{
    userInfo.uid = -1;
    userInfo.gid = -1;
    userInfo.homeDirectory.clear();
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
    minecraft_server_info info(isNew,serverName.c_str(),userInfo);
    info.read_props(props,errors); // read and parse options
    // attempt to start the minecraft server
    server_handle* handle;
    handle = minecraft_server_manager::allocate_server();
    handle->set_clientid( sock->get_accept_id() );
    auto cond = handle->pserver->begin(info);
    client_log(minecontrold::standardLog) << cond << " {" << handle->pserver->get_internal_id() << '}' << endline;
    rstream& response = cond==minecraft_server::mcraft_start_success ? prepare_list_message() : prepare_list_error();
    response << cond;
    // place error output into the message to client
    if ( errors.has_input() ) {
        response << newline;
        response.place(errors);
    }
    response.flush_output();
    connection << msgbuf.get_message();
    minecraft_server_manager::attach_server(handle);
    return true;
}

bool controller_client::command_status(rstream&,rstream&)
{
    rstream& msg = prepare_list_message();
    minecraft_server_manager::print_servers(msg,userInfo.uid>-1 ? &userInfo : NULL);
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
    if ((result = minecraft_server_manager::lookup_auth_servers(userInfo,servers)) == minecraft_server_manager::auth_lookup_none) {
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

bool controller_client::command_exec(rstream& kstream,rstream& vstream)
{
    str key;
    uint32 id = -1;
    str command;
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
        else if (key == "command") {
            vstream >> command;
            if (!vstream.get_input_success() || command.length()==0) {
                prepare_error() << "Bad or empty command value specified; expected a non-empty string" << flush;
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
    if (command.length() == 0) {
        prepare_error() << "No command specified" << flush;
        connection << msgbuf.get_message();
        return false;
    }
    // ask the server manager for handles to running servers that the user can access; we need to make sure no blocking calls happen here (shouldn't be a problem)
    if ((result = minecraft_server_manager::lookup_auth_servers(userInfo,servers)) == minecraft_server_manager::auth_lookup_none) {
        prepare_error() << "There are no server processes running at this time" << flush;
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
    minecontrol_authority* pauth;
    minecontrol_authority::execute_result res = minecontrol_authority::authority_exec_unspecified;
    pauth = servers[i]->pserver->get_authority();
    if (pauth != NULL) {
        int pid;
        res = pauth->run_auth_process(command,&pid);
        prepare_message() << res << flush;
        connection << msgbuf.get_message();
        minecontrold::standardLog << res;
        if (pid != -1)
            minecontrold::standardLog << " (client-exec: " << "process=" << pid << ", cmd=" << command << ')';
        minecontrold::standardLog << endline;
    }
    else {
        prepare_error() << "The server's authority management has shutdown; this may be a bug in minecontrold" << flush;
        connection << msgbuf.get_message();
    }
    // return regulation of minecraft server(s) to the manager
    minecraft_server_manager::attach_server(&servers[0],servers.size());
    return res==minecontrol_authority::authority_exec_okay || res==minecontrol_authority::authority_exec_okay_exited;
}

bool controller_client::command_auth_ls(rstream& kstream,rstream& vstream)
{
    str key;
    str filter;
    dynamic_array<str> programs;
    minecontrol_authority::path_type pathType;

    // Read optional filter field from protocol message.
    while (kstream >> key) {
        if (key == "filter") {
            vstream >> filter;
            if (!vstream.get_input_success()) {
                prepare_error() << "Could not read filter value from input stream" << flush;
                connection << msgbuf.get_message();
                return false;
            }
            if (filter.length() == 0 || (filter != "user" && filter != "system")) {
                prepare_error() << "The specified auth-list filter is incorrect" << flush;
                connection << msgbuf.get_message();
                return false;
            }
        }
    }

    if (filter == "user") {
        pathType = minecontrol_authority::authority_user_path;
    }
    else if (filter == "system") {
        pathType = minecontrol_authority::authority_system_path;
    }
    else {
        pathType = minecontrol_authority::authority_any_path;
    }

    minecontrol_authority::list_authority_programs(programs,userInfo,pathType);

    if (programs.size() > 0) {
        rstream& msg = prepare_list_message();
        for (size_type i = 0;i < programs.size();++i) {
            msg << programs[i] << newline;
        }
        msg.flush_output();
    }
    else {
        rstream& msg = prepare_message();
        if (filter.length() > 0) {
            msg << "There are no such authority programs available on the system." << flush;
        }
        else {
            msg << "There are no authority programs available on the system." << flush;
        }
    }
    connection << msgbuf.get_message();

    return true;
}

bool controller_client::command_stop(rstream& kstream,rstream& vstream)
{
    str key;
    uint32 id = -1;
    int32 authPID = -1;
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
        else if (key == "authpid") {
            vstream >> authPID;
            if (!vstream.get_input_success() || authPID<=0) {
                prepare_error() << "Bad authority PID value specified; expected a positive non-zero integer" << flush;
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
    if ((result = minecraft_server_manager::lookup_auth_servers(userInfo,servers)) == minecraft_server_manager::auth_lookup_none) {
        prepare_error() << "There are no server processes running at this time" << flush;
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
    if (authPID == -1) {
        uint32 svrid = servers[i]->pserver->get_internal_id();
        auto status = servers[i]->pserver->end();
        prepare_message() << status << flush;
        connection << msgbuf.get_message();
        // use the original id in log message to map closing server
        // to the client session that started it
        minecontrold::standardLog << '{' << servers[i]->get_clientid() << "} " << status << " {" << svrid << '}' << endline;
    }
    else {
        minecontrol_authority* auth = servers[i]->pserver->get_authority();
        if (auth==NULL || !auth->stop_auth_process(authPID))
            prepare_error() << "No authority process existed with PID=" << authPID << flush;
        else {
            prepare_message() << "Authority process with PID=" << authPID << " was terminated" << flush;
            client_log(minecontrold::standardLog) << "client terminated authority program with PID=" << authPID << endline;
        }
        connection << msgbuf.get_message();
    }
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
    if ((result = minecraft_server_manager::lookup_auth_servers(userInfo,servers)) == minecraft_server_manager::auth_lookup_none) {
        prepare_message() << "There are no server processes running at this time" << flush;
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
    // get authority object and server information (for log messages)
    minecontrol_authority* pauth;
    minecontrol_authority::console_result res = minecontrol_authority::console_no_channel;
    str serverName = servers[i]->pserver->get_internal_name();
    uint32 serverID = servers[i]->pserver->get_internal_id();
    pauth = servers[i]->pserver->get_authority();
    // return regulation of server(s) to the manager before we begin console negotiation; this way the servers may
    // be shared with other clients who are logged into this minecontrol server
    minecraft_server_manager::attach_server(&servers[0],servers.size());
    if (pauth != NULL) {
        client_log(minecontrold::standardLog) << "client entered console mode on server '" << serverName << "' with id=" << serverID << endline;
        res = pauth->client_console_operation(*sock);
        client_log(minecontrold::standardLog) << "client exited console mode on server '" << serverName << "' with id=" << serverID << endline;
    }
    else {
        prepare_error() << "The server's authority management has shutdown; this may be a bug in minecontrold" << flush;
        connection << msgbuf.get_message();
    }
    return res != minecontrol_authority::console_no_channel;
}

bool controller_client::command_shutdown(rstream&,rstream&)
{
    prepare_message() << "Server '" << minecontrold::get_server_name() << "' is shutting down" << flush;
    connection << msgbuf.get_message();
    minecontrold::shutdown_minecontrold();
    return true;
}
