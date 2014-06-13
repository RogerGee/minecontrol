// minecontrol-misc-types.h
#ifndef MINECONTROL_MISC_TYPES_H
#define MINECONTROL_MISC_TYPES_H
#include <rlibrary/rstring.h>

namespace minecraft_controller
{
    struct user_info
    {
        user_info() : uid(-1), gid(-1) {}

        rtypes::str userName;
        rtypes::str homeDirectory;
        int uid, gid;
    };
}

#endif
