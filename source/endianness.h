#include <sys/types.h>
#include <sys/param.h>

#ifdef __ANDROID__
  #include <sys/endian.h>
#endif

#ifdef __sun
# include <sys/isa_defs.h>
# define LITTLE_ENDIAN 1234
# define BIG_ENDIAN    4321
# ifdef _LITTLE_ENDIAN
#  define BYTE_ORDER LITTLE_ENDIAN
# else
#  define BYTE_ORDER BIG_ENDIAN
# endif
#endif

#ifndef htole32
 #if BYTE_ORDER == LITTLE_ENDIAN
  #define htole32(x) (x)
 #else
  #if BYTE_ORDER == BIG_ENDIAN
   #define htole32(x) \
       ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
        (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))
  #else
   #error No endianness given.
  #endif
 #endif
#endif

#ifndef htole64
 #if BYTE_ORDER == LITTLE_ENDIAN
  #define htole64(x) (x)
 #else
  #if BYTE_ORDER == BIG_ENDIAN
   #define htole64(x) \
     ( (((x) & 0xff00000000000000ull) >> 56)                                 \
     | (((x) & 0x00ff000000000000ull) >> 40)                                 \
     | (((x) & 0x0000ff0000000000ull) >> 24)                                 \
     | (((x) & 0x000000ff00000000ull) >> 8)                                  \
     | (((x) & 0x00000000ff000000ull) << 8)                                  \
     | (((x) & 0x0000000000ff0000ull) << 24)                                 \
     | (((x) & 0x000000000000ff00ull) << 40)                                 \
     | (((x) & 0x00000000000000ffull) << 56))
  #else
   #error No endianness given.
  #endif
 #endif
#endif
