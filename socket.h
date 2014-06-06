// socket.h
#ifndef SOCKET_H
#define SOCKET_H
#include <rlibrary/riodevice.h>
#include <rlibrary/rstream.h>

namespace minecraft_controller
{
    /* represents the result of the accept
       operation on a domain socket */
    enum socket_accept_condition
    {
        socket_nodevice, // the socket does not exist yet
        socket_accepted, // a connection was accepted by the bound socket
        socket_interrupted // accept was interrupted; the socket may have been shutdown
    };

    class socket_error { };

    class socket_address
    {
    public:
        virtual ~socket_address();

        void assign(const char* address,const char* service = NULL)
        { _assignAddress(address,service); }

        bool get_next_address(const void*& address,rtypes::size_type& structureSize) const
        { return _getNextAddress(address,structureSize); }
        const char* get_address_string() const;
    private:
        // virtual socket_address interface
        virtual void _assignAddress(const char* address,const char* service) = 0;
        virtual bool _getNextAddress(const void*& address,rtypes::size_type& sz) const = 0;
        virtual void _prepareAddressString(char* buffer,rtypes::size_type length) const = 0;
    };

    enum socket_family
    {
        socket_family_unix,
        socket_family_inet
    };

    class socket : public rtypes::io_device
    {
    public:
        socket();

        bool bind(const socket_address& address); // server
        socket_accept_condition accept(socket*& socknew); // server
        bool connect(const socket_address& address); // client
        bool shutdown(); // client and server

        // gets unique id for accepted connection socket; a
        // value of zero represents an invalid id number
        rtypes::uint64 get_accept_id() const
        { return _id; }
        socket_family get_family() const
        { return _getFamily(); }
    private:
        static rtypes::uint64 _idTop; // maintain count of connected clients
        rtypes::uint64 _id;

        // implement virtual io_device interface
        virtual void _openEvent(const char*,rtypes::io_access_flag,rtypes::io_resource**,rtypes::io_resource**,void**,rtypes::uint32);
        virtual void _readAll(rtypes::generic_string&) const;
        virtual void _closeEvent(rtypes::io_access_flag) {}

        // virtual socket interface to be implemented
        virtual void _openSocket(int& fd) = 0;
        virtual void _createClientSocket(socket*& socknew) = 0;
        virtual socket_family _getFamily() const = 0;
    };

    /* this behaves almost like an rtypes::io_stream except it handles endline
       encoding in a cross-platform manner (no alterations) */
    class socket_stream : public rtypes::rstream, public rtypes::generic_stream_device<socket>
    {
    private:
        // rtypes::generic_stream_device interface
        virtual void _clearDevice() {};
        virtual bool _openDevice(const char* DeviceID);
        virtual void _closeDevice();

        // rtypes::rstream interface
        virtual bool _inDevice() const;
        virtual void _outDevice();
    };
}

#endif
