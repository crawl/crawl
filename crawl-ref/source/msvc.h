/**
 * @file
 * @brief Header file for MSVC compiles
**/

#pragma once

#if defined(TARGET_COMPILER_VC)

#include <io.h>

#define fileno _fileno
#define strcasecmp _stricmp
#define ftruncate _chsize

// These are not defined in MSVC version of stat.h
#define        S_IWUSR        S_IWRITE
#define        S_IRUSR        S_IREAD

// macro redefinitions. This happens with PURE which gets defined in combaseapi.h
#pragma warning(disable : 4005)

// unary minus operator applied to unsigned type and truncation of constant
// value: these are valid things for the compiler to worry about but we know
// what we're doing.
#pragma warning(disable : 4146)
#pragma warning(disable : 4309)

// truncating conversions
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)

// POSIX/GetVersionExA deprecation warnings
#pragma warning(disable : 4996)

// _int64 is used as a struct member name in store.h, but MSVC considers it a
// synonym for __int64 which is a synonym for long long!
#define _int64 var_int64

//missing in sys/types.h
#define mode_t unsigned short
typedef ptrdiff_t ssize_t;

#endif /* defined(TARGET_COMPILER_VC) */
