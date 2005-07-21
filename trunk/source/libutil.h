/*
 *  File:       libutil.h
 *  Summary:    System indepentant functions 
 *
 *  Change History (most recent first):
 *
 *      <1>   2001/Nov/01        BWR     Created
 *
 */

#ifndef LIBUTIL_H
#define LIBUTIL_H

void get_input_line( char *const buff, int len );

#ifdef NEED_USLEEP
void usleep( unsigned long time );
#endif

#ifdef NEED_SNPRINTF
int snprintf( char *str, size_t size, const char *format, ... );
#endif

#endif
