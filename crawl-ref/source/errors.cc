/*
 *  File:       errors.cc
 *  Summary:    Handling of error conditions that are not program bugs.
 *  Written by: Adam Borowski
 */

#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "AppHdr.h"
#include "libutil.h"
#include "stuff.h"

void fail(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    std::string buf = vmake_stringf(msg, args);
    va_end(args);

    // TODO: throw an exception or sumthing here when expected
    end(1, true, buf.c_str());
}

void sysfail(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    std::string buf = vmake_stringf(msg, args);
    va_end(args);

    buf += ": ";
    buf += strerror(errno);

    // TODO: throw an exception or sumthing here when expected
    end(1, true, buf.c_str());
}
