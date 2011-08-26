#ifndef APPHDR_H
#define APPHDR_H

#include <stdio.h>

#define USE_MORE_SECURE_SEED

#ifdef __cplusplus

template < class T >
inline void UNUSED(const volatile T &)
{
}

#endif // __cplusplus

#include "externs.h"

#endif // APPHDR_H
