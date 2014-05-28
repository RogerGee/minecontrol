#include "net-socket.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace rtypes;
using namespace minecraft_controller;

network_socket_address::network_socket_address()
    : _addrInfo(NULL), _pWalker(NULL)
{
}
network_socket_address::network_socket_address(const char* address,const char* portNo)
    : _addrInfo(NULL), _pWalker(NULL)
{
    assign(address,portNo);
}
network_socket_address::~network_socket_address()
{
    if (_addrInfo != NULL)
        freeaddrinfo(reinterpret_cast<addrinfo*>(_addrInfo));
}
void network_socket_address::_assignAddress(const char* address,const char* service)
{
    if (_addrInfo != NULL) {
        freeaddrinfo(reinterpret_cast<addrinfo*>(_addrInfo));
        _addrInfo = NULL;
    }
    addrinfo hints;
    memset(&hints,0,sizeof(addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    // try to get address information
    if (getaddrinfo(address,service,&hints,(addrinfo**)&_addrInfo) == 0)
        _pWalker = _addrInfo;
}
bool network_socket_address::_getNextAddress(const void*& address,rtypes::size_type& sz) const
{
    if (_addrInfo!=NULL && _pWalker!=NULL) {
        addrinfo* tmp = reinterpret_cast<addrinfo*>(_pWalker);
        _pWalker = tmp->ai_next;
        if (_pWalker == NULL)
            _pWalker = _addrInfo;
        address = tmp->ai_addr;
        sz = tmp->ai_addrlen;
        return tmp->ai_next != NULL;
    }
    address = NULL;
    sz = 0;
    return false;
}
void network_socket_address::_prepareAddressString(char* buffer,rtypes::size_type length) const
{
    // use the most current address pointed to by _pWalker
    if (_pWalker != NULL) {
        const addrinfo* pitem = reinterpret_cast<addrinfo*>(_pWalker);
        getnameinfo(pitem->ai_addr,pitem->ai_addrlen,buffer,length,NULL,0,0);
    }
    else
        buffer[0] = 0;
}

network_socket::network_socket()
{
}
void network_socket::_openSocket(int& fd)
{
    fd = ::socket(AF_INET,SOCK_STREAM,0);
    if (fd == -1)
        throw network_socket_error();
}
void network_socket::_createClientSocket(socket*& socknew)
{
    socknew = new network_socket;
}

network_socket_stream::network_socket_stream()
{
}
network_socket_stream::network_socket_stream(network_socket& device)
    : stream_device<network_socket>(device)
{
}
network_socket_stream::~network_socket_stream()
{
    if ( !_bufOut.is_empty() )
        _outDevice();
}
void network_socket_stream::_clearDevice()
{
    // no implementation
}
bool network_socket_stream::_openDevice(const char*)
{
    _device->open();
    return _device->get_last_operation_status() == no_operation;
}
void network_socket_stream::_closeDevice()
{
    _device->close();
}
bool network_socket_stream::_inDevice() const
{
    char buffer[4096];
    _device->read(buffer,4096);
    if (_device->get_last_operation_status() == success_read) {
        _bufIn.push_range(buffer,_device->get_last_byte_count());
        return true;
    }
    return false;
}
void network_socket_stream::_outDevice()
{
    // write the entire buffer
    _device->write(&_bufOut.peek(),_bufOut.size());
    _bufOut.clear();
}
