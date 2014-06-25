// domain-socket.cpp
#include "domain-socket.h"
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
using namespace rtypes;
using namespace minecraft_controller;

// minecraft_controller::domain_socket_address

domain_socket_address::domain_socket_address()
{
    _domainSockAddr = new sockaddr_un;
}
domain_socket_address::domain_socket_address(const char* path)
{
    _domainSockAddr = new sockaddr_un;
    assign(path);
}
domain_socket_address::~domain_socket_address()
{ // allow virtual destructors
    delete reinterpret_cast<sockaddr_un*>(_domainSockAddr);
}
void domain_socket_address::_assignAddress(const char* address,const char*/*ignore service*/)
{
    size_type i, j;
    sockaddr_un* buffer = reinterpret_cast<sockaddr_un*>(_domainSockAddr);
    memset(buffer,0,sizeof(sockaddr_un));
    buffer->sun_family = AF_UNIX;
    i = 0;
    if (address[0] == '@') {
        // use abstract domain-socket namespace
        // (buffer->sun_path is already zero-filled)
        j = 1;
        ++address;
    }
    else
        j = 0;
    while (address[i] && j<sizeof(buffer->sun_path)) {
        buffer->sun_path[j] = address[i];
        ++i;
        ++j;
    }
}
bool domain_socket_address::_getNextAddress(const void*& address,rtypes::size_type& sz) const
{
    address = _domainSockAddr;
    sz = sizeof(sockaddr_un);
    return false; // no more addresses available after this one
}
void domain_socket_address::_prepareAddressString(char* buffer,size_type length) const
{
    size_type i = 0, j = 0;
    const sockaddr_un* addr = reinterpret_cast<const sockaddr_un*>(_domainSockAddr);
    if (addr->sun_path[0] == '\0') {
        *buffer++ = '@';
        j = 1;
    }
    while (addr->sun_path[j] && i<length) {
        buffer[i] = addr->sun_path[j];
        ++i;
        ++j;
    }
}

// minecraft_controller::domain_socket

domain_socket::domain_socket()
{
    _addrbuf = new sockaddr_un;
}
domain_socket::~domain_socket()
{
    delete reinterpret_cast<sockaddr_un*>(_addrbuf);
}
void domain_socket::_openSocket(int& fd)
{
    // create socket
    fd = ::socket(AF_UNIX,SOCK_STREAM,0);
    if (fd == -1)
        throw domain_socket_error();
}
void domain_socket::_createClientSocket(socket*& socknew)
{
    // we just need this function to get the correct socket type
    socknew = new domain_socket;
}
void* domain_socket::_getAddressBuffer(size_type& length)
{
    length = sizeof(sockaddr_un);
    return _addrbuf;
}
void domain_socket::_addressBufferToString(str& s) const
{
    // 'accept' doesn't set information for 'sockaddr_un'
    s = "";
}
