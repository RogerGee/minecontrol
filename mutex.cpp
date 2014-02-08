#include "mutex.h"
using namespace minecraft_controller;

mutex::mutex()
    : _mutexID(PTHREAD_MUTEX_INITIALIZER)
{
}
void mutex::lock()
{
    if (::pthread_mutex_lock(&_mutexID) != 0)
        throw mutex_error();
}
void mutex::unlock()
{
    if (::pthread_mutex_unlock(&_mutexID) != 0)
        throw mutex_error();
}
