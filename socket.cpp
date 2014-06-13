#include "socket.h"
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
using namespace rtypes;
using namespace minecraft_controller;

// minecraft_controller::socket_address

socket_address::~socket_address()
{ // allow virtual destructors
}
const char* socket_address::get_address_string() const
{
    static const size_type bufsz = 512;
    static char buf[bufsz];
    _prepareAddressString(buf,bufsz);
    return buf;
}

// minecraft_controller::socket

/*static*/ uint64 socket::_idTop = 1;
socket::socket()
    : _id(0)
{
}
minecraft_controller::socket& socket::operator =(const socket& obj)
{
    if (this != &obj)
        _assign(obj);
    return *this;
}
bool socket::bind(const socket_address& address)
{
    io_resource* pres = _getValidContext();
    if (pres != NULL) {
        int fd = pres->interpret_as<int>();
        // attempt bind
        while (true) {
            const sockaddr* addr;
            size_type sz;
            bool cont = address.get_next_address(reinterpret_cast<const void*&>(addr),sz);
            if (addr!=NULL && ::bind(fd,addr,sz) == 0)
                break;
            if (!cont)
                return false;
        }
        // attempt to make the socket passive
        if (::listen(fd,SOMAXCONN) == -1)
            return false;
        return true;
    }
    return false;
}
socket_accept_condition socket::accept(socket*& snew)
{
    io_resource* pres;
    snew = NULL;
    pres = _getValidContext();
    if (pres != NULL) {
        int fd = ::accept(pres->interpret_as<int>(),NULL,NULL);
        if (fd != -1) {
            io_resource* input, *output;
            input = new io_resource;
            output = new io_resource(false);
            input->assign(fd);
            output->assign(fd);
            // assign io contexts to device
            _createClientSocket(snew);
            snew->close(); 
            snew->_assign(input,output);
            // release our reference to the handles
            --_ResourceRef(input);
            --_ResourceRef(output);
            // assign a unique id to represent the connection
            snew->_id = _idTop++;
            return socket_accepted;
        }
        else if (errno==EINTR || errno==ECONNABORTED || errno==EBADF || errno==EINVAL)
            return socket_interrupted;
        else
            throw socket_error();
    }
    return socket_nodevice;
}
bool socket::connect(const socket_address& address)
{
    io_resource* pres = _getValidContext();
    // attempt connect on all available interfaces
    while (true) {
        size_type sz;
        const sockaddr* addr;
        bool cont = address.get_next_address(reinterpret_cast<const void*&>(addr),sz);
        if (addr!=NULL && ::connect(pres->interpret_as<int>(),addr,sz) == 0)
            return true;
        if (!cont)
            break;
    }
    return false;
}
bool socket::shutdown()
{
    io_resource* pres;
    pres = _getValidContext();
    if (pres != NULL) {
        // shutdown both reading and writing
        if (::shutdown(pres->interpret_as<int>(),SHUT_RDWR) == -1)
            return false;
        return true;
    }
    return false;
}
void socket::_openEvent(const char*,rtypes::io_access_flag mode,rtypes::io_resource** pinput,rtypes::io_resource** poutput,void**,rtypes::uint32)
{
    int fd;
    // let derived implementation create the socket
    _openSocket(fd);
    // create device resources
    if (mode & read_access) {
        *pinput = new io_resource;
        (*pinput)->assign(fd);
    }
    if (mode & write_access) {
        *poutput = new io_resource(*pinput == NULL);
        (*poutput)->assign(fd);
    }
}
void socket::_readAll(generic_string& sbuf) const
{
    if ( is_valid_input() ) {
        // I don't anticipate huge amounts of data being processed
        // but you never know...
        char buffer[10000];
        read(buffer,10000);
        for (size_type i = 0;i<get_last_byte_count();i++)
            sbuf.push_back(buffer[i]);
    }
}

// minecraft_controller::socket_stream

bool socket_stream::_openDevice(const char* DeviceID)
{
    return _device->open(DeviceID);
}
void socket_stream::_closeDevice()
{
    _device->close();
}
bool socket_stream::_inDevice() const
{
    if (_device != NULL) {
        char buffer[4096];
        _device->read(buffer,4096);
        if (_device->get_last_operation_status() == success_read) {
            _bufIn.push_range(buffer,_device->get_last_byte_count());
            return true;
        }
    }
    return false;
}
void socket_stream::_outDevice()
{
    if (_device != NULL) {
        _device->write(&_bufOut.peek(),_bufOut.size());
        _bufOut.clear();
    }
}
