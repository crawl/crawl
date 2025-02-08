/**
 * @file
 * @brief Header file for MSVC compiles
**/

#ifndef __msvc_h
#define __msvc_h

#if defined(TARGET_COMPILER_VC)

#include <io.h>
#include <math.h>

#define fileno _fileno
#define strdup _strdup
#define strcasecmp _stricmp
#define strnicmp _strnicmp
#define ftruncate _chsize
#define putenv _putenv

// These are not defined in MSVC version of stat.h
#define        S_IWUSR        S_IWRITE
#define        S_IRUSR        S_IREAD

/* Disable warning about:
   4290: the way VC handles the throw() specifier
   4267: "possible loss of data" when switching data types without a cast
 */
#pragma warning(disable : 4290)
#pragma warning (disable: 4267)
#pragma warning(disable : 4351)
// bool -> int
#pragma warning(disable : 4800)

// struct vs class XXX: fix these some day!
#pragma warning(disable : 4099)

// truncating conversions XXX: fix these too!
#pragma warning(disable : 4244)

// POSIX deprecation warnings
#pragma warning(disable : 4996)

//this is targeting for struct member name in store.h, nothing else gets affected as of 0.9.0
#define _int64 var_int64

//missing in sys/types.h
#define mode_t unsigned short
typedef ptrdiff_t ssize_t;

#endif /* defined(TARGET_COMPILER_VC) */

#endif
