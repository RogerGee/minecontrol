// domain-socket.h
#ifndef DOMAIN_SOCKET_H
#define DOMAIN_SOCKET_H
#include "rlibrary/riodevice.h"

namespace minecraft_controller
{
    class domain_socket : public rtypes::io_device
    {
    public:
        domain_socket();

        void accept(domain_socket& dsnew); // server
        bool connect(const char* path); // client

        const char* get_path() const
        { return _path; }
    private:
        const char* _path;

        // implement virtual io_device interface
        virtual void _openEvent(const char*,rtypes::io_access_flag,void**,rtypes::dword);
        virtual void _readAll(rtypes::generic_string&) const;
        virtual void _closeEvent(rtypes::io_access_flag);
    };

    class domain_socket_stream : public rtypes::rstream,
                                 public rtypes::stream_device<domain_socket>
    {
    public:
        domain_socket_stream();
        domain_socket_stream(domain_socket& device);
        ~domain_socket_stream();
    private:
        virtual void _clearDevice();
        virtual bool _openDevice(const char*);
        virtual void _closeDevice();

        virtual bool _inDevice() const;
        virtual void _outDevice();
    };
}

#endif
