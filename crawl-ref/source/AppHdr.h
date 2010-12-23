#ifndef APPHDR_H
#define APPHDR_H

#include <stdio.h>

#ifdef __cplusplus

template < class T >
inline void UNUSED(const volatile T &)
{
}

#endif // __cplusplus

#include "externs.h"

#endif // APPHDR_H
