#include "pipe.h"
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
using namespace rtypes;
using namespace minecraft_controller;

pipe::pipe()
{
    _fdOpenRead = -1;
    _fdOpenWrite = -1;
}
minecraft_controller::pipe& pipe::operator =(const pipe& obj)
{
    if (this != &obj) {
        _assign(obj);
        _fdOpenRead = obj._fdOpenRead;
        _fdOpenWrite = obj._fdOpenWrite;
    }
    return *this;
}
void pipe::standard_duplicate(bool includeError)
{
    // this member function should run on a pipe object that exists in
    // a forked child process
    if ( is_valid_context() ) {
        // duplicate open ends of created pipes as standard input/output
        // close current IO context and load the open end as the current
        // context
        if (_fdOpenRead != -1) {
            if (_fdOpenRead != STDIN_FILENO)
                if (::dup2(_fdOpenRead,STDIN_FILENO) == -1)
                    throw pipe_error();
            // make input in this pipe object not exist
            close_input();
        }
        if (_fdOpenWrite != -1) {
            if (_fdOpenWrite != STDOUT_FILENO)
                if (::dup2(_fdOpenWrite,STDOUT_FILENO) == -1)
                    throw pipe_error();
            if (includeError && _fdOpenWrite!=STDERR_FILENO)
                if (::dup2(_fdOpenWrite,STDERR_FILENO) == -1)
                    throw pipe_error();
            // make output in this pipe object not exist
            close_output();
        }
    }
}
void pipe::close_open()
{
    // this member function should be run on a pipe
    // object that exists as a parent process that has
    // just forked a child
    if (_fdOpenWrite != -1) {
        ::close(_fdOpenWrite);
        _fdOpenWrite = -1;
    }
    if (_fdOpenRead != -1) {
        ::close(_fdOpenRead);
        _fdOpenRead = -1;
    }
}
void pipe::duplicate(int fdInput,int fdOutput)
{
    duplicate_input(fdInput);
    duplicate_output(fdOutput);
}
void pipe::duplicate_input(int fd)
{
    io_resource* pcontext = _getInputContext();
    if (pcontext!=NULL && dup2(pcontext->interpret_as<int>(),fd)!=fd)
        throw pipe_error();
}
void pipe::duplicate_output(int fd)
{
    io_resource* pcontext = _getOutputContext();
    if (pcontext!=NULL && dup2(pcontext->interpret_as<int>(),fd)!=fd)
        throw pipe_error();
}

/*static*/ size_type pipe::pipe_atomic_limit()
{
    return PIPE_BUF;
}
void pipe::_openEvent(const char*,io_access_flag access,io_resource** pinput,io_resource** poutput,void**,uint32)
{
    // simulate full duplex mode by creating two pipes (that is, if the access is read/write); set
    // O_NONBLOCK for the write end of the pipe (the one set for this io object)
    int pipe1[2]/*read*/, pipe2[2]/*write*/;
    if ((access&read_access)!=0 && ::pipe(pipe1)!=0)
        throw pipe_error();
    if ((access&write_access)!=0 && ::pipe(pipe2)!=0) {
        ::close(pipe1[0]);
        ::close(pipe1[1]);
        throw pipe_error();
    }
    if (access & read_access) {
        *pinput = new io_resource;
        (*pinput)->assign(pipe1[0]);
        _fdOpenWrite = pipe1[1];
    }
    if (access & write_access) {
        if (fcntl(pipe2[1],F_SETFL,O_NONBLOCK) == -1)
            throw pipe_error();
        *poutput = new io_resource;
        (*poutput)->assign(pipe2[1]);
        _fdOpenRead = pipe2[0];
    }
}
void pipe::_readAll(generic_string& buffer) const
{
    // read 
    buffer.resize(4096);
    read(buffer);
}
void pipe::_closeEvent(io_access_flag access)
{
    if ((access&read_access)!=0 && _fdOpenRead!=-1) {
        ::close(_fdOpenRead);
        _fdOpenRead = -1;
    }
    if ((access&write_access)!=0 && _fdOpenWrite != -1) {
        ::close(_fdOpenWrite);
        _fdOpenWrite = -1;
    }
}

pipe_stream::pipe_stream()
{
}
pipe_stream::pipe_stream(pipe& device)
    : stream_device<pipe>(device)
{
}
pipe_stream::~pipe_stream()
{
    if ( !_bufOut.is_empty() )
        flush_output();
}
bool pipe_stream::_inDevice() const
{
    char buffer[4960];
    _device->read(buffer,4096);
    if (_device->get_last_operation_status() == success_read) {
        _bufIn.push_range(buffer,_device->get_last_byte_count());
        return true;
    }
    return false;
}
void pipe_stream::_outDevice()
{
    if ( !_bufOut.is_empty() ) {
        _device->write(&_bufOut.peek(),_bufOut.size());
        _bufOut.clear();
    }
}
void pipe_stream::_clearDevice()
{
}
bool pipe_stream::_openDevice(const char*)
{
    return _device->open();
}
void pipe_stream::_closeDevice()
{
    _device->close();
}
