// domain-socket.h
#ifndef DOMAIN_SOCKET_H
#define DOMAIN_SOCKET_H
#include "socket.h"

namespace minecraft_controller
{
    class domain_socket_error { };

    class domain_socket_address : public socket_address
    {
    public:
        domain_socket_address();
        domain_socket_address(const char* path);
        virtual ~domain_socket_address();
    private:
        void* _domainSockAddr;

        virtual void _assignAddress(const char* address,const char* service);
        virtual bool _getNextAddress(const void*& address,rtypes::size_type& sz) const;
        virtual void _prepareAddressString(char* buffer,rtypes::size_type length) const;
    };

    class domain_socket : public socket
    {
    public:
        domain_socket();
        ~domain_socket();
    private:
        void* _addrbuf;

        // implement virtual socket interface
        virtual void _openSocket(int& fd);
        virtual void _createClientSocket(socket*& socknew);
        virtual socket_family _getFamily() const
        { return socket_family_unix; }
        virtual void* _getAddressBuffer(rtypes::size_type& length);
        virtual void _addressBufferToString(rtypes::str& s) const;
    };
}

#endif
