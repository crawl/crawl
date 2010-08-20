/*
 *  File:       errors.cc
 *  Summary:    Handling of error conditions that are not program bugs.
 *  Written by: Adam Borowski
 */

#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "AppHdr.h"
#include "errors.h"
#include "libutil.h"
#include "stuff.h"

void fail(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    std::string buf = vmake_stringf(msg, args);
    va_end(args);

    // Do we want to call end() right on when there's no one trying catching,
    // or should we risk exception code mess things up?
    throw ext_fail_exception(buf);
}

void sysfail(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    std::string buf = vmake_stringf(msg, args);
    va_end(args);

    buf += ": ";
    buf += strerror(errno);

    throw ext_fail_exception(buf);
}
