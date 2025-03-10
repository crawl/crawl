/**
 * @file
 * @brief Various preprocessor macros commonly used throughout Crawl.
 *
 * This does not include tweakable constants; see defines.h for that.
 */

#pragma once

namespace std {}
using namespace std;

#ifndef SIZE_MAX
    #include <limits>
    #define SIZE_MAX (std::numeric_limits<std::size_t>::max())
#endif


#ifndef _lint
    #define COMPILE_CHECK(expr) static_assert((expr), #expr)
#else
    #define COMPILE_CHECK(expr)
#endif


#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                     \
    TypeName(const TypeName&) = delete;                                        \
    void operator=(const TypeName&) = delete


// See the GCC __attribute__ documentation for what these mean.
// Note: clang does masquerade as GNUC.

#if defined(__GNUC__)
# define NORETURN __attribute__ ((noreturn))
#elif defined(TARGET_COMPILER_VC)
# define NORETURN __declspec(noreturn)
#else
# define NORETURN
#endif

#if defined(__GNUC__)
# define PURE __attribute__ ((pure))
# define IMMUTABLE __attribute__ ((const))
#else
# define PURE
# define IMMUTABLE
#endif


#ifdef __GNUC__
// show warnings about the format string
# ifdef TARGET_COMPILER_MINGW
// Configure mingw32 / mingw-w64 printf format
//
// If __USE_MINGW_ANSI_STDIO is defined, mingw-w64 will try to use a C99 printf
//  However, stdio.h must be included to have a C99 printf for use with
//  __attribute__((format(...)).
#  ifdef __cplusplus
#   include <cstdio>
#  else
#   include <stdio.h>
#  endif
// If mingw defined a special format function to use, use this over 'printf'.
#  ifdef __MINGW_PRINTF_FORMAT
#   define CRAWL_PRINTF_FORMAT __MINGW_PRINTF_FORMAT
#  else
#   define CRAWL_PRINTF_FORMAT printf
#  endif
# else
// standard GNU-compatible compiler (i.e., not mingw32/mingw-w64)
#  define CRAWL_PRINTF_FORMAT printf
# endif
# define PRINTF(x, dfmt) const char *format dfmt, ...) \
                   __attribute__((format (CRAWL_PRINTF_FORMAT, x+1, x+2))
#else
# define PRINTF(x, dfmt) const char *format dfmt, ...
#endif


#ifdef __cplusplus

template < class T >
static inline void UNUSED(const volatile T &)
{
}

template <class... T>
static inline void UNUSED(const volatile T &...)
{
}

#endif // __cplusplus


#ifdef TARGET_COMPILER_VC
/* Tell Windows.h not to define min and max as macros, define them via STL */
#define NOMINMAX
#endif
