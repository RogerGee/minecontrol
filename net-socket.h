// net-socket.h
#ifndef NET_SOCKET_H
#define NET_SOCKET_H
#include "socket.h"
#include <rlibrary/rtypestypes.h>

namespace minecraft_controller
{
    class network_socket_error { };

    class network_socket_address : public socket_address
    {
    public:
        network_socket_address();
        network_socket_address(const char* address,const char* portNumber);
        virtual ~network_socket_address();
    private:
        void* _addrInfo; // pointer to address information from getaddrinfo
        mutable void* _pWalker; // pointer to current sockaddr structure

        virtual void _assignAddress(const char* address,const char* service);
        virtual bool _getNextAddress(const void*& address,rtypes::size_type& sz) const;
        virtual void _prepareAddressString(char* buffer,rtypes::size_type length) const;
    };

    class network_socket : public socket
    {
    public:
        network_socket();
        ~network_socket();
    private:
        void* _addrbuf;

        // implement virtual socket interface
        virtual void _openSocket(int& fd);
        virtual void _createClientSocket(socket*& socknew);
        virtual socket_family _getFamily() const
        { return socket_family_inet; }
        virtual void* _getAddressBuffer(rtypes::size_type& length);
        virtual void _addressBufferToString(rtypes::str& s) const;
    };

    class network_socket_stream : public rtypes::rstream,
                                  public rtypes::stream_device<network_socket>
    {
    public:
        network_socket_stream();
        network_socket_stream(network_socket&);
        ~network_socket_stream();
    private:
        virtual void _clearDevice();
        virtual bool _openDevice(const char*);
        virtual void _closeDevice();

        virtual bool _inDevice() const;
        virtual void _outDevice();
    };
}

#endif
