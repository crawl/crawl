/**
 * @file
 * @brief Assertions and such.
**/

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
#define COMPILE_CHECK(expr) typedef char compile_check_ ## __LINE__[(expr) ? 1 : -1]
#else
#define COMPILE_CHECK(expr)
#endif

#if defined(DEBUG) && !defined(ASSERTS)
#define ASSERTS
#endif

#ifdef ASSERTS

NORETURN void AssertFailed(const char *expr, const char *file, int line, bool save_game);

#define ASSERT_SAVE(p)                                                  \
    do {                                                                \
        if (!(p)) AssertFailed(#p, __FILE__, __LINE__, true);           \
    } while (false)

#define ASSERT(p)                                                       \
    do {                                                                \
        if (!(p)) AssertFailed(#p, __FILE__, __LINE__, false);          \
    } while (false)

#define VERIFY(p)       ASSERT(p)

#else

#define ASSERT_SAVE(p)  ((void) 0)
#define ASSERT(p)       ((void) 0)
#define VERIFY(p)       do {if (p) ;} while (false)

inline void __DUMMY_TRACE__(...)
{
}

#endif

NORETURN void die(const char *file, int line, const char *format, ...);
#define die(...) die(__FILE__, __LINE__, __VA_ARGS__)

NORETURN void die_noline(const char *format, ...);

#ifdef DEBUG
void debuglog(const char *format, ...);
#endif
#endif
