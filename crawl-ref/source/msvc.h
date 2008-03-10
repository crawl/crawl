#define snprintf _snprintf
#define itoa _itoa
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define unlink _unlink

// No va_copy in MSVC
#if defined(_MSC_VER) || !defined(va_copy)
#define va_copy(dst, src) \
   ((void) memcpy(&(dst), &(src), sizeof(va_list)))
#endif

#pragma warning( disable : 4290 )
#pragma warning( disable : 4351 )
// bool -> int
#pragma warning( disable : 4800 )
