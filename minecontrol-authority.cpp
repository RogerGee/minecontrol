// minecontrol-authority.cpp
#include "minecontrol-authority.h"
using namespace minecraft_controller;

minecontrol_authority::minecontrol_authority(const pipe& ioChannel)
    : _iochannel(ioChannel)
{
    _threadCond = false;

}
