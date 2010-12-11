/*
 *  File:       syscalls.h
 *  Summary:    Wrappers for sys/libc calls, mostly for charset purposes.
 *  Written by: Adam Borowski
 */

#ifndef SYSCALLS_H
#define SYSCALLS_H

#ifdef TARGET_OS_WINDOWS
# ifdef TARGET_COMPILER_VC
#  include <direct.h>
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# define rename(foo,bar) !MoveFileEx(foo, bar, MOVEFILE_REPLACE_EXISTING)
#endif

bool lock_file(int fd, bool write, bool wait = false);
bool unlock_file(int fd);

#ifdef TARGET_OS_WINDOWS
# ifndef UNIX
int fdatasync(int fd);
# endif
#endif

// This check is way underinclusive.
#if !defined(TARGET_OS_LINUX) && !defined(TARGET_OS_WINDOWS) && !defined(TARGET_OS_NETBSD) && !defined(TARGET_OS_SOLARIS)
# define NEED_FAKE_FDATASYNC
int fdatasync(int fd);
#endif

#endif
