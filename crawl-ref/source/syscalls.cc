/*
 *  File:       syscalls.cc
 *  Summary:    Wrappers for sys/libc calls, mostly for charset purposes.
 *  Written by: Adam Borowski
 */

#include "AppHdr.h"
#include "syscalls.h"

#ifdef TARGET_OS_WINDOWS

// FIXME!  This file doesn't yet use rename() (but we'll redefine it anyway once
// charset handling comes).
#undef rename

#include <io.h>
#endif

bool lock_file(int fd)
{
#ifdef TARGET_OS_WINDOWS
    return !!LockFile((HANDLE)_get_osfhandle(fd), 0, 0, -1, -1);
#else
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    return !fcntl(fd, F_SETLK, &fl);
#endif
}
