// domain-socket.cpp
#include "rlibrary/rstdio.h"
#include "domain-socket.h"
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
using namespace rtypes;
using namespace minecraft_controller;

/*static*/ qword domain_socket::_idTop = 1;
domain_socket::domain_socket()
    : _path(NULL), _id(0)
{
}
domain_socket::domain_socket_accept_condition domain_socket::accept(domain_socket& dsnew)
{
    io_resource* pres;
    dsnew.close();
    pres = _getValidContext();
    if (pres != NULL)
    {
        int fd = ::accept(pres->interpret_as<int>(),NULL,NULL);
        if (fd != -1)
        {
            dsnew._input = new io_resource;
            dsnew._output = new io_resource(false);
            dsnew._input->assign(fd);
            dsnew._output->assign(fd);
            dsnew._lastOp = no_operation;
            // assign a unique id to represent the connection
            dsnew._id = _idTop++;
            return domain_socket_accepted;
        }
        else if (errno==EINTR || errno==ECONNABORTED || errno==EBADF)
            return domain_socket_interrupted;
        else
            throw domain_socket_error();
    }
    dsnew._lastOp = no_device;
    return domain_socket_nodevice;
}
bool domain_socket::connect(const char* path)
{
    size_type t, u;
    sockaddr_un addr;
    io_resource* pres = _getValidContext();
    // prepare address
    ::memset(&addr,0,sizeof(sockaddr_un));
    t = 0;
    u = 1;
    while (u<sizeof(addr.sun_path) && path[t])
    {
        addr.sun_path[u] = path[t];
        ++t;
        ++u;
    }
    addr.sun_family = AF_UNIX;
    // attempt connect
    if (::connect(pres->interpret_as<int>(),(sockaddr*)&addr,sizeof(sockaddr_un)) == -1)
        return false;
    return true;
}
bool domain_socket::shutdown()
{
    io_resource* pres;
    pres = _getValidContext();
    if (pres != NULL)
    {
        if (::shutdown(pres->interpret_as<int>(),SHUT_RDWR) == -1)
            return false;
        return true;
    }
    return false;
}
void domain_socket::_openEvent(const char* deviceID,io_access_flag mode,void**,dword)
{
    int fd;
    // create socket
    fd = ::socket(AF_UNIX,SOCK_STREAM,0);
    if (fd != -1)
    {
        if (deviceID != NULL)
        {
            size_type t, u;
            sockaddr_un addr;
            // create address
            ::memset(&addr,0,sizeof(sockaddr_un));
            addr.sun_family = AF_UNIX;
            t = 0;
            u = 1;
            while (u<sizeof(addr.sun_path) && deviceID[t])
            {
                addr.sun_path[u] = deviceID[t];
                ++t;
                ++u;
            }
            // attempt bind
            if (::bind(fd,(sockaddr*)&addr,sizeof(sockaddr_un)) == -1)
            {
                ::close(fd);
                return;
            }
            // attempt to make the socket passive
            if (::listen(fd,SOMAXCONN) == -1)
            {
                ::close(fd);
                return;
            }
            _path = deviceID;
        }
        // create device resources
        if (mode & read_access)
        {
            _input = new io_resource;
            _input->assign(fd);
        }
        if (mode & write_access)
        {
            _output = new io_resource(_input == NULL);
            _output->assign(fd);
        }
    }
    else
        throw domain_socket_error();
}
void domain_socket::_readAll(generic_string& sbuf) const
{
    if ( is_valid_input() )
    {
        char buffer[10000];
        read(buffer,10000);
        for (size_type i = 0;i<_byteCount;i++)
            sbuf.push_back(buffer[i]);
    }
}
void domain_socket::_closeEvent(io_access_flag)
{
    _path = NULL;
    _id = 0;
}

domain_socket_stream_device::domain_socket_stream_device()
{
}
domain_socket_stream_device::~domain_socket_stream_device()
{
    if ( !_bufOut.is_empty() )
        _outDevice();
}
void domain_socket_stream_device::_clearDevice()
{
}
bool domain_socket_stream_device::_openDevice(const char* DeviceID)
{
    _device->open(DeviceID);
    return _device->get_last_operation_status() == no_operation;
}
void domain_socket_stream_device::_closeDevice()
{
    _device->close();
}
bool domain_socket_stream_device::_inDevice() const
{
    char buffer[4096];
    _device->read(buffer,4096);
    if (_device->get_last_operation_status() == success_read)
    {
        _bufIn.push_range(buffer,_device->get_last_byte_count());
        return true;
    }
    return false;
}
void domain_socket_stream_device::_outDevice()
{
    // write the entire buffer
    _device->write(&_bufOut.peek(),_bufOut.size());
    _bufOut.clear();
}

domain_socket_stream::domain_socket_stream()
{
}
domain_socket_stream::domain_socket_stream(domain_socket& domain)
{
    open(domain);
}

domain_socket_binary_stream::domain_socket_binary_stream()
{
}
domain_socket_binary_stream::domain_socket_binary_stream(domain_socket& domain)
{
    open(domain);
}
