// minecraft-controller.h
#ifndef MINECRAFT_CONTROLLER_H
#define MINECRAFT_CONTROLLER_H
#include "rlibrary/rstream.h"

namespace minecraft_controller
{
    class minecraft_controller_log_stream : public rtypes::rstream
    {
    public:
        minecraft_controller_log_stream();
    private:
        virtual bool _inDevice() const
        { return false; } // this stream doesn't do input
        virtual void _outDevice();
    };

    class minecontrold
    {
    public:
        static const char* get_server_name()
        { return SERVER_NAME; }
        static const char* get_server_version()
        { return SERVER_VERSION; }

        static void shutdown_minecontrold();

        static minecraft_controller_log_stream standardLog;
    private:
        static const char* SERVER_NAME;
        static const char* SERVER_VERSION;
    };
}

#endif
