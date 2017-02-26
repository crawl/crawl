#pragma once

#ifndef TARGET_OS_WINDOWS

#include <pthread.h>

#ifndef PTHREAD_CREATE_JOINABLE
// AIX
# define PTHREAD_CREATE_UNDETACHED
#endif
//#ifndef PTHREAD_MUTEX_RECURSIVE
//// LinuxThreads, probably gone from any semi-modern system
//# define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
//#endif

#define mutex_t pthread_mutex_t
#define mutex_lock(x) pthread_mutex_lock(&x)
#define mutex_unlock(x) pthread_mutex_unlock(&x)
static inline void mutex_init(pthread_mutex_t &x)
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&x, &attr);
    pthread_mutexattr_destroy(&attr);
}
#define mutex_destroy(x) pthread_mutex_destroy(&x);

#define thread_t pthread_t
#define thread_create_joinable(th, start, arg)  \
    unix_pthread_create(th, PTHREAD_CREATE_JOINABLE, (start), (void*)(arg))
#define thread_join(th) pthread_join(th, 0)
#define thread_create_detached(th, start, arg)  \
    unix_pthread_create(th, PTHREAD_CREATE_DETACHED, (start, (void*)(arg))

static inline int unix_pthread_create(pthread_t *th, int det,
                                      void *(*start)(void *), void *arg)
{
    pthread_attr_t attr;
    int ret;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, det);
    ret = pthread_create(th, &attr, start, arg);
    pthread_attr_destroy(&attr);

    return ret;
}

#define cond_t pthread_cond_t
#define cond_init(x) pthread_cond_init(&x, 0)
#define cond_destroy(x) pthread_cond_destroy(&x)
#define cond_wait(x,m) pthread_cond_wait(&x, &m)
#define cond_wake(x) pthread_cond_signal(&x)


#else


#include <windows.h>
#include <errno.h>

#define mutex_t CRITICAL_SECTION
#define mutex_lock(x) EnterCriticalSection(&x)
#define mutex_unlock(x) LeaveCriticalSection(&x)
#define mutex_init(x) InitializeCriticalSection(&x);
#define mutex_destroy(x) DeleteCriticalSection(&x);

#define thread_t HANDLE
#define thread_create_joinable(th, start, arg)  \
    (!(*th=CreateThread(0, 0, (LPTHREAD_START_ROUTINE)(start), (void*)(arg), \
                        0, (LPDWORD)(th))))
#define thread_join(th)                         \
    {                                           \
        WaitForSingleObject(th, INFINITE);      \
        CloseHandle(th);                        \
    }
#define thread_create_detached(th, start, arg)  \
    (win32_thread_create_detached(th, (LPTHREAD_START_ROUTINE)(start), (void*)(arg)))

static inline int win32_thread_create_detached(thread_t *th,
                                               LPTHREAD_START_ROUTINE start,
                                               void *arg)
{
    DWORD dummy;

    if (!(*th = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)start, arg, 0, &dummy)))
        return EAGAIN;
    CloseHandle(*th);
    return !*th;
}

#define cond_t HANDLE
#define cond_init(x) {(x)=CreateEvent(0, 0, 0, 0);}
#define cond_destroy(x) CloseHandle(x);
#define cond_wait(x,m) {mutex_unlock(m);WaitForSingleObject(x, INFINITE);mutex_lock(m);}
#define cond_wake(x) PulseEvent(x)

#endif
