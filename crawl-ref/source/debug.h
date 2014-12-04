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
#if _MSC_VER >= 1100 && defined(DEBUG) != defined(_DEBUG)
#error DEBUG and _DEBUG are out of sync!
#endif
#endif

#if defined(DEBUG) && !defined(ASSERTS)
#define ASSERTS
#endif

#ifdef ASSERTS

NORETURN void AssertFailed(const char *expr, const char *file, int line,
                           const char *text = nullptr, ...);

#ifdef __clang__
# define WARN_PUSH _Pragma("GCC diagnostic push")
# define WARN_POP  _Pragma("GCC diagnostic pop")
# define IGNORE_ASSERT_WARNINGS _Pragma("GCC diagnostic ignored \"-Wtautological-constant-out-of-range-compare\"")
#elif __GNUC__ * 100 + __GNUC_MINOR__ >= 406
# define WARN_PUSH _Pragma("GCC diagnostic push")
# define WARN_POP  _Pragma("GCC diagnostic pop")
# define IGNORE_ASSERT_WARNINGS _Pragma("GCC diagnostic ignored \"-Wtype-limits\"")
#else
// gcc-4.2 has a worse variant, but I don't care enough
# define WARN_PUSH
# define WARN_POP
# define IGNORE_ASSERT_WARNINGS
#endif

#define ASSERT(p)                                       \
    do {                                                \
        WARN_PUSH                                       \
        IGNORE_ASSERT_WARNINGS                          \
        if (!(p)) AssertFailed(#p, __FILE__, __LINE__); \
        WARN_POP                                        \
    } while (false)

#define ASSERTM(p,text,...)                             \
    do {                                                \
        WARN_PUSH                                       \
        IGNORE_ASSERT_WARNINGS                          \
        if (!(p)) AssertFailed(#p, __FILE__, __LINE__, text, __VA_ARGS__); \
        WARN_POP                                        \
    } while (false)

#define ASSERT_RANGE(x, xmin, xmax)                                           \
    do {                                                                      \
        WARN_PUSH                                                             \
        IGNORE_ASSERT_WARNINGS                                                \
        if ((x) < (xmin) || (x) >= (xmax))                                    \
        {                                                                     \
            die("ASSERT failed: " #x " of %" PRIdMAX " out of range " \
                #xmin " (%" PRIdMAX ") .. " \
                #xmax " (%" PRIdMAX ")",                                      \
                (intmax_t)(x), (intmax_t)(xmin), (intmax_t)(xmax));           \
        }                                                                     \
        WARN_POP                                                              \
    } while (false)

#else

#define ASSERT(p)                         ((void) 0)
#define ASSERTM(p, text,...)              ((void) 0)
#define ASSERT_RANGE(x, a, b)             ((void) 0)

#endif

NORETURN void die(const char *file, int line, PRINTF(2, ));
#define die(...) die(__FILE__, __LINE__, __VA_ARGS__)

NORETURN void die_noline(PRINTF(0, ));

#ifdef DEBUG
void debuglog(PRINTF(0, ));
#endif
#endif
