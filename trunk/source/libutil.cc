/*
 *  File:       libutil.cc
 *  Summary:    Functions that may be missing from some systems
 *
 *  Change History (most recent first):
 *
 *      <1> 2001/Nov/01        BWR     Created
 *
 */

#include "AppHdr.h"
#include <stdio.h>
#include <ctype.h>

void get_input_line( char *const buff, int len )
{
    buff[0] = '\0';         // just in case

#if defined(LINUX)
    get_input_line_from_curses( buff, len ); // inplemented in liblinux.cc
#elif defined(MAC) || defined(WIN32CONSOLE)
    getstr( buff, len );        // implemented in libmac.cc
#else
    fgets( buff, len, stdin );  // much safer than gets()
#endif

    buff[ len - 1 ] = '\0';  // just in case 

    // Removing white space from the end in order to get rid of any
    // newlines or carriage returns that any of the above might have 
    // left there (ie fgets especially).  -- bwr
    const int end = strlen( buff ); 
    int i; 

    for (i = end - 1; i >= 0; i++) 
    {
        if (isspace( buff[i] ))
            buff[i] = '\0';
        else
            break;
    }
}

// The old school way of doing short delays via low level I/O sync.  
// Good for systems like old versions of Solaris that don't have usleep.
#ifdef NEED_USLEEP

#include <sys/time.h>
#include <sys/types.h>
#include <sys/unistd.h>

void usleep(unsigned long time)
{
    struct timeval timer;

    timer.tv_sec = (time / 1000000L);
    timer.tv_usec = (time % 1000000L);

    select(0, NULL, NULL, NULL, &timer);
}
#endif

// Not the greatest version of snprintf, but a functional one that's 
// a bit safer than raw sprintf().  Note that this doesn't do the
// special behaviour for size == 0, largely because the return value 
// in that case varies depending on which standard is being used (SUSv2 
// returns an unspecified value < 1, whereas C99 allows str == NULL
// and returns the number of characters that would have been written). -- bwr
#ifdef NEED_SNPRINTF

#include <stdarg.h>
#include <string.h>

int snprintf( char *str, size_t size, const char *format, ... )
{
    va_list argp;
    va_start( argp, format );

    char buff[ 10 * size ];  // hopefully enough 

    vsprintf( buff, format, argp );
    strncpy( str, buff, size );
    str[ size - 1 ] = '\0';

    int ret = strlen( str );  
    if ((unsigned int) ret == size - 1 && strlen( buff ) >= size)
        ret = -1;

    va_end( argp );

    return (ret);
}
#endif
