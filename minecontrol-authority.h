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
        result_ban
    };

    struct minecontrol_authority_info
    {
        rule cussing; // rule is executed if profanity is detected
        rule jury; // rule is executed if all players vote against player

    };

    /* represents an object that authoritatively manages
     * the server (either directly or indirectly) via
     * messages received from/sent to the minecraft server
     * process's standard io channels
     */
    class minecontrol_authority
    {
    public:
        minecontrol_authority();

        // initialize an authority for use
        void initialize(pipe* ioChannel,
            const rtypes::str& serverName,
            int userID,
            int groupID);

        void begin_processing();
        void end_processing();
    private:
        static void* _processing_thread(void*);

        pipe* _iochannel; // ptr to io channel in use by server object
        volatile bool _threadCond;
        rtypes::ulong _threadID;
        rtypes::int32 _processID;
    };
}

#endif
