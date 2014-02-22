// domain-socket.h
#ifndef DOMAIN_SOCKET_H
#define DOMAIN_SOCKET_H
#include "rlibrary/riodevice.h"

namespace minecraft_controller
{
    class domain_socket_error { };

    class domain_socket : public rtypes::io_device
    {
    public:
        // represents the result of the accept
        // operation on a domain socket
        enum domain_socket_accept_condition
        {
            domain_socket_nodevice, // the domain socket does not exist yet
            domain_socket_accepted, // a connection was accepted by the domain socket
            domain_socket_interrupted // accept was interrupted; the domain socket may have been shutdown
        };

        domain_socket();

        domain_socket_accept_condition accept(domain_socket& dsnew); // server
        bool connect(const char* path); // client
        bool shutdown();

        const char* get_path() const
        { return _path; }
        rtypes::qword get_accept_id() const // gets unique id for accepted connection socket
        { return _id; }
    private:
        static rtypes::qword _idTop; // maintain count of connected clients
        const char* _path;
        rtypes::qword _id;

        // implement virtual io_device interface
        virtual void _openEvent(const char*,rtypes::io_access_flag,rtypes::io_resource**,rtypes::io_resource**,void**,rtypes::dword);
        virtual void _readAll(rtypes::generic_string&) const;
        virtual void _closeEvent(rtypes::io_access_flag);
    };

    class domain_socket_stream_device : public rtypes::stream_device<domain_socket>
    {
    protected:
        domain_socket_stream_device();
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
