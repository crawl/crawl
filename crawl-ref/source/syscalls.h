/*
 *  File:       syscalls.h
 *  Summary:    Wrappers for sys/libc calls, mostly for charset purposes.
 *  Written by: Adam Borowski
 */

#ifndef SYSCALLS_H
#define SYSCALLS_H

bool lock_file(int fd, bool write, bool wait = false);
bool unlock_file(int fd);

int rename_u(const char *oldpath, const char *newpath);
int unlink_u(const char *pathname);
int chmod_u(const char *path, mode_t mode);
FILE *fopen_u(const char *path, const char *mode);
int mkdir_u(const char *pathname, mode_t mode);
int open_u(const char *pathname, int flags, mode_t mode);

#endif
