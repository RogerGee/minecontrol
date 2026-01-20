#include "socket.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
using namespace rtypes;
using namespace minecraft_controller;

// minecraft_controller::socket_address

socket_address::socket_address(bool doEncryption)
    : _doEncryption(doEncryption)
{
}
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
    : _id(0), _sslCtx(nullptr), _ssl(nullptr)
{
}
socket::~socket()
{ // allow for virtual destruction
    if (_ssl) {
        SSL_free(_ssl);
    }
    if (_sslCtx) {
        SSL_CTX_free(_sslCtx);
    }
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

    SSL_CTX* ctx = nullptr;
    if (address.isEncrypted()) {
        const SSL_METHOD* method = TLS_server_method();
        ctx = SSL_CTX_new(method);
        if (ctx == nullptr) {
            ERR_print_errors_fp(stderr);
            return false;
        }

        const char* val;
        const char* keyFile = "key.pem";
        const char* certFile = "cert.pem";
        if ((val = getenv("MINECONTROL_KEY_FILE"))) {
            certFile = val;
        }
        if ((val = getenv("MINECONTROL_CERTIFICATE_FILE"))) {
            certFile = val;
        }
        if (SSL_CTX_use_certificate_file(ctx,certFile,SSL_FILETYPE_PEM) <= 0
            || SSL_CTX_use_PrivateKey_file(ctx,keyFile,SSL_FILETYPE_PEM) <= 0)
        {
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            return false;
        }
    }

    if (pres != NULL) {
        int fd = pres->interpret_as<int>();
        // attempt bind
        while (true) {
            const sockaddr* addr;
            size_type sz;
            bool cont = address.get_next_address(reinterpret_cast<const void*&>(addr),sz);
            if (addr!=NULL && ::bind(fd,addr,sz) == 0)
                break;
            if (!cont) {
                if (ctx != nullptr) {
                    SSL_CTX_free(ctx);
                }
                return false;
            }
        }
        // attempt to make the socket passive
        if (::listen(fd,SOMAXCONN) == -1) {
            if (ctx != nullptr) {
                SSL_CTX_free(ctx);
            }
            return false;
        }

        if (address.isEncrypted()) {
            _sslCtx = ctx;
        }

        return true;
    }

    if (ctx != nullptr) {
        SSL_CTX_free(ctx);
    }

    return false;
}
socket_accept_condition socket::accept(socket*& snew,str& fromAddress)
{
    io_resource* pres;
    snew = NULL;
    pres = _getValidContext();
    if (pres != NULL) {
        int fd;
        size_type prelen;
        socklen_t len;
        sockaddr* paddrbuf = reinterpret_cast<sockaddr*>(_getAddressBuffer(prelen));
        len = prelen;
        fd = ::accept(pres->interpret_as<int>(),paddrbuf,&len);
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
            // get address of remote peer
            _addressBufferToString(fromAddress);

            // Wrap connection in openssl ctx if we have one.
            if (_sslCtx != nullptr) {
                SSL* ssl = SSL_new(_sslCtx);
                if (SSL_set_fd(ssl,fd) == 0) {
                    ERR_print_errors_fp(stderr);
                    SSL_free(ssl);
                    throw socket_error();
                }
                if (SSL_accept(ssl) <= 0) {
                    ERR_print_errors_fp(stderr);
                    SSL_free(ssl);
                    throw socket_error();
                }
                snew->_ssl = ssl;
            }

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

    SSL_CTX* ctx = nullptr;
    if (address.isEncrypted()) {
        const SSL_METHOD* method = TLS_client_method();
        ctx = SSL_CTX_new(method);
        if (ctx == nullptr) {
            ERR_print_errors_fp(stderr);
            return false;
        }
    }

    // attempt connect on all available interfaces
    while (pres != NULL) {
        size_type sz;
        const sockaddr* addr;
        int fd = pres->interpret_as<int>();
        bool cont = address.get_next_address(reinterpret_cast<const void*&>(addr),sz);
        if (addr!=NULL && ::connect(fd,addr,sz) == 0) {
            if (address.isEncrypted()) {
                int ret;
                SSL* ssl = SSL_new(ctx);
                if (SSL_set_fd(ssl,fd) == 0) {
                    ERR_print_errors_fp(stderr);
                    SSL_free(ssl);
                    SSL_CTX_free(ctx);
                    return false;
                }
                if ((ret = SSL_connect(ssl)) <= 0) {
                    ERR_print_errors_fp(stderr);
                    SSL_free(ssl);
                    SSL_CTX_free(ctx);
                    return false;
                }
                _sslCtx = ctx;
                _ssl = ssl;
            }

            return true;
        }
        if (!cont)
            break;
    }

    if (ctx != nullptr) {
        SSL_CTX_free(ctx);
    }

    return false;
}
bool socket::select(uint32 timeout)
{
    int sock;
    int result;
    timeval tm;
    io_resource* pres;
    fd_set rset;
    tm.tv_sec = timeout;
    tm.tv_usec = 0;
    pres = _getValidContext();
    sock = pres->interpret_as<int>();
    FD_ZERO(&rset);
    FD_SET(sock,&rset);
    result = ::select(sock+1,&rset,NULL,NULL,&tm);
    if (result == -1)
        throw socket_error();
    return result != 0;
}
bool socket::shutdown()
{
    io_resource* pres;
    pres = _getValidContext();
    if (pres != NULL) {
        if (_ssl) {
            SSL_shutdown(_ssl);
        }

        // shutdown both reading and writing
        if (::shutdown(pres->interpret_as<int>(),SHUT_RDWR) == -1)
            return false;
        return true;
    }
    return false;
}
void socket::_readBuffer(void* buffer,size_type bytesToRead) const
{
    if (_ssl) {
        _sslRead(buffer,bytesToRead);
    }
    else {
        io_device::_readBuffer(buffer,bytesToRead);
    }
}
void socket::_readBuffer(const io_resource* context,void* buffer,size_type bytesToRead) const
{
    if (_ssl) {
        _sslRead(buffer,bytesToRead);
    }
    else {
        io_device::_readBuffer(buffer,bytesToRead);
    }
}
void socket::_writeBuffer(const void* buffer,size_type length)
{
    if (_ssl) {
        _sslWrite(buffer,length);
    }
    else {
        io_device::_writeBuffer(buffer,length);
    }
}
void socket::_writeBuffer(const io_resource* context,const void* buffer,size_type length)
{
    if (_ssl) {
        _sslWrite(buffer,length);
    }
    else {
        io_device::_writeBuffer(context,buffer,length);
    }
}
void socket::_sslRead(void* buffer,size_type bytesToRead) const
{
    int ret;
    ret = SSL_read(_ssl,buffer,static_cast<int>(bytesToRead));
    if (ret <= 0) {
        int err = SSL_get_error(_ssl,ret);
        if (err == SSL_ERROR_ZERO_RETURN) {
            _lastOp = no_input;
        }
        else {
            _lastOp = bad_read;
        }
        _byteCount = 0;
    }
    else {
        _lastOp = success_read;
        _byteCount = static_cast<size_type>(ret);
    }
}
void socket::_sslWrite(const void* buffer,size_type length)
{
    int ret = SSL_write(_ssl,buffer,static_cast<int>(length));
    if (ret <= 0) {
        int err = SSL_get_error(_ssl,ret);
        if (err == SSL_ERROR_ZERO_RETURN) {
            _lastOp = no_input;
        }
        else {
            _lastOp = bad_write;
        }
        _byteCount = 0;
    }
    else {
        _lastOp = success_write;
        _byteCount = static_cast<size_type>(ret);
    }
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
