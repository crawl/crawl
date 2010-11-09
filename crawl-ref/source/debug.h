/*
 *  File:       debug.h
 *  Summary:    Assertions and such.
 *  Written by: Linley Henzell and Jesse Jones
 */

#ifndef DEBUG_H
#define DEBUG_H

// Synch with ANSI definitions.
#if !(defined(DEBUG) ^ defined(NDEBUG))
#error DEBUG and NDEBUG are out of sync!
#endif

// Synch with MSL definitions.
#ifdef __MSL__
#if __MSL__ && DEBUG != defined(MSIPL_DEBUG_MODE)
#error DEBUG and MSIPL_DEBUG_MODE are out of sync!
#endif
#endif

// Synch with MSVC.
#ifdef TARGET_COMPILER_VC
#if _MSC_VER >= 1100 && DEBUG != defined(_DEBUG)
#error DEBUG and _DEBUG are out of sync!
#endif
#endif


#ifndef _lint
#define COMPILE_CHECK(expr, tag) typedef char compile_check_ ## tag[(expr) ? 1 : -1]
#else
#define COMPILE_CHECK(expr, tag)
#endif

#if defined(DEBUG) && !defined(ASSERTS)
#define ASSERTS
#endif

#ifdef ASSERTS

void AssertFailed(const char *expr, const char *file, int line, bool save_game);

#define ASSERT_SAVE(p)                                                  \
    do {                                                                \
        if (!(p)) AssertFailed(#p, __FILE__, __LINE__, true);           \
    } while (false)

#define ASSERT(p)                                                       \
    do {                                                                \
        if (!(p)) AssertFailed(#p, __FILE__, __LINE__, false);          \
    } while (false)

#define VERIFY(p)       ASSERT(p)

void DEBUGSTR(const char *format,...);
void TRACE(const char *format,...);

#else

#define ASSERT_SAVE(p)  ((void) 0)
#define ASSERT(p)       ((void) 0)
#define VERIFY(p)       do {if (p) ;} while (false)

inline void __DUMMY_TRACE__(...)
{
}

#define DEBUGSTR 1 ? ((void) 0) : __DUMMY_TRACE__
#define TRACE    1 ? ((void) 0) : __DUMMY_TRACE__

#endif

#ifdef DEBUG
void debuglog(const char *format, ...);
#endif
#endif
