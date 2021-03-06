// pipe.h - simple pipe device/stream types
#ifndef PIPE_H
#define PIPE_H
#include <rlibrary/riodevice.h>

namespace minecraft_controller
{
    class pipe_error { };

    class pipe : public rtypes::io_device
    {
    public:
        pipe();

        pipe& operator =(const pipe&);

        /* these functions operate on the open read/write ends
           that this io_device does not use

           close and duplicate this pipe as the standard
           IO channel for the current process; optionally
           include standard error                   [child] */
        void standard_duplicate(bool includeError = false);
        /* closes open side of pipe for the process; this should
           be invoked in a parent process that spawns a child that
           invokes 'standard_duplicate'             [parent] */
        void close_open();

        /* these functions operate on the read/write ends that
           this io_device uses */
        void duplicate(int fdInput,int fdOutput); // duplicate input and output as specified file descriptors
        void duplicate_input(int fd); // duplicate just input as specified file descriptor
        void duplicate_output(int fd); // duplicate just output as specified file descriptor

        static rtypes::size_type pipe_atomic_limit();
    private:
        // implement io_device interface
        virtual void _openEvent(const char*,rtypes::io_access_flag,rtypes::io_resource**,rtypes::io_resource**,void**,rtypes::uint32);
        virtual void _readAll(rtypes::generic_string&) const;
        virtual void _closeEvent(rtypes::io_access_flag);

        int _fdOpenRead, _fdOpenWrite;
    };

    // this stream type performs local buffering until either:
    //  a) 'endline' is inserted into the stream
    //  b) 'flush' is inserted into the stream
    //  c) .flush_output() is called on the stream
    class pipe_stream : public rtypes::rstream,
                        public rtypes::stream_device<pipe>
    {
    public:
        pipe_stream();
        pipe_stream(pipe& device);
        ~pipe_stream();
    private:
        // implement stream_buffer interface
        virtual bool _inDevice() const;
        virtual void _outDevice();

        // implement stream_device<pipe> interface
        virtual void _clearDevice();
        virtual bool _openDevice(const char* deviceID);
        virtual void _closeDevice();
    };
}

#endif
