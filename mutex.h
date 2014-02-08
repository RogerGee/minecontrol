// mutex.h
#ifndef MUTEX_H
#define MUTEX_H
#include <pthread.h>

namespace minecraft_controller
{
    class mutex_error { };

    // represents a very, very simple 
    // statically allocated mutex...
    class mutex
    {
    public:
        mutex();

        // these both throw if anything unusual happens
        void lock();
        void unlock();
    private:
        pthread_mutex_t _mutexID;
    };
}

#endif
