// minecontrol-authority.h
#ifndef MINECONTROL_AUTHORITY_H
#define MINECONTROL_AUTHORITY_H
#include "pipe.h"

namespace minecraft_controller
{
    class minecraft_authority_error { };

    enum rule
    {
        no_result,
        result_kill,
        result_ban,
        result_jail
    };

    // these are the base rules for any minecontrol authority
    struct minecontrol_authority_rules
    {
        minecontrol_authority_rules();

        rule cussing; // rule is executed if profanity is detected
        rule jury; // rule is executed if all players vote against player

    };

    /* represents an object that authoritatively manages
     * the server (either directly or indirectly) via
     * messages received from/sent to the minecraft server
     * process's standard io channels; this class provides
     * the base functionality for any kind of minecontrol
     * authority
     */
    class minecontrol_authority
    {
    public:
        minecontrol_authority(const pipe& ioChannel);

        void begin_processing();
        void end_processing();
    private:
        static void* _processing(void*);

        void _createJail(); // creates the default jail for the world

        pipe _iochannel; // IO channel to Minecraft server Java process (read/write enabled)
        volatile bool _threadCond;
        rtypes::ulong _threadID;
        rtypes::int32 _processID;
    };
}

#endif
