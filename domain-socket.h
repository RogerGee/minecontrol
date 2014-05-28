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
    private:
        // implement virtual socket interface
        virtual void _openSocket(int& fd);
        virtual void _createClientSocket(socket*& socknew);
        virtual socket_family _getFamily() const
        { return socket_family_unix; }
    };

    class domain_socket_stream_device : public rtypes::stream_device<domain_socket>
    {
    protected:
        domain_socket_stream_device();
        domain_socket_stream_device(domain_socket&);
        ~domain_socket_stream_device();
    private:
        virtual void _clearDevice();
        virtual bool _openDevice(const char*);
        virtual void _closeDevice();

        virtual bool _inDevice() const;
        virtual void _outDevice();
    };

    class domain_socket_stream : public rtypes::rstream,
                                 public domain_socket_stream_device
    {
    public:
        domain_socket_stream();
        domain_socket_stream(domain_socket& device);
    };

    class domain_socket_binary_stream : public rtypes::rbinstream,
                                        public domain_socket_stream_device
    {
    public:
        domain_socket_binary_stream();
        domain_socket_binary_stream(domain_socket& device);
    };
}

#endif
