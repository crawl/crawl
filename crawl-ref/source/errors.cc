/**
 * @file
 * @brief Handling of error conditions that are not program bugs.
**/

#include "AppHdr.h"

#include "errors.h"

#include <cerrno>
#include <cstdarg>
#include <cstring>

#include "stringutil.h"

NORETURN void fail(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    string buf = vmake_stringf(msg, args);
    va_end(args);

    // Do we want to call end() right on when there's no one trying catching,
    // or should we risk exception code mess things up?
    throw ext_fail_exception(buf);
}

NORETURN void sysfail(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    string buf = vmake_stringf(msg, args);
    va_end(args);

    buf += ": ";
    buf += strerror(errno);

    throw ext_fail_exception(buf);
}

NORETURN void corrupted(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    string buf = vmake_stringf(msg, args);
    va_end(args);

    throw corrupted_save(buf);
}
