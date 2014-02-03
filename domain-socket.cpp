// domain-socket.cpp
#include "domain-socket.h"
#include "rlibrary/rstdio.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
using namespace rtypes;
using namespace minecraft_controller;

domain_socket::domain_socket()
    : _path(NULL)
{
}
void domain_socket::accept(domain_socket& dsnew)
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
            return;
        }
    }
    dsnew._lastOp = no_device;
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
}

domain_socket_stream::domain_socket_stream()
{
}
domain_socket_stream::domain_socket_stream(domain_socket& domain)
{
    open(domain);
}
domain_socket_stream::~domain_socket_stream()
{
    if ( !_bufOut.is_empty() )
        flush_output();
}
void domain_socket_stream::_clearDevice()
{
}
bool domain_socket_stream::_openDevice(const char* DeviceID)
{
    _device->open(DeviceID);
    return _device->get_last_operation_status() == no_operation;
}
void domain_socket_stream::_closeDevice()
{
    _device->close();
}
bool domain_socket_stream::_inDevice() const
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
void domain_socket_stream::_outDevice()
{
    // write the entire buffer
    _device->write(&_bufOut.peek(),_bufOut.size());
    _bufOut.clear();
}
