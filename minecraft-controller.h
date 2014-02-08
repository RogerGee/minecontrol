// minecraft-controller.h
#ifndef MINECRAFT_CONTROLLER_H
#define MINECRAFT_CONTROLLER_H
#include "rlibrary/rstream.h"

namespace minecraft_controller
{
    extern class minecraft_controller_log_stream : public rtypes::rstream
    {
    public:
        minecraft_controller_log_stream();
    private:
        virtual bool _inDevice() const
        { return false; } // this stream doesn't do input
        virtual void _outDevice();
    } standardLog;
}

#endif
