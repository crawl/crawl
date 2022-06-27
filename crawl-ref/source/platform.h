/*
 *   CrissCross
 *   A multi-purpose cross-platform library.
 *
 *   A product of Uplink Laboratories.
 *
 *   (c) 2006-2009 Steven Noonan.
 *   Licensed under the New BSD License.
 *
 */

/*
 *
 * Preprocessor Definitions
 * ------------------------
 *
 * TARGET_CPU_ARM
 *  ARM processor
 * TARGET_CPU_ALPHA
 *  DEC Alpha processor
 * TARGET_CPU_SPARC
 *  Sun Microsystems SPARC processor
 * TARGET_CPU_X86
 *  Intel x86 processor
 * TARGET_CPU_IA64
 *  Intel Itanic processor
 * TARGET_CPU_X64
 *  Intel 64-bit processor
 * TARGET_CPU_PPC
 *  IBM PowerPC processor
 *
 * TARGET_OS_WINDOWS
 *  Windows
 * TARGET_OS_LINUX
 *  Linux
 * TARGET_OS_CYGWIN
 *  Cygwin
 * TARGET_OS_MACOSX
 *  Mac OS X
 * TARGET_OS_FREEBSD
 *  FreeBSD
 * TARGET_OS_NETBSD
 *  NetBSD
 * TARGET_OS_OPENBSD
 *  OpenBSD
 * TARGET_OS_NDSFIRMWARE
 *  Nintendo DS firmware
 *
 * TARGET_COMPILER_GCC
 *  GNU C++ Compiler
 * TARGET_COMPILER_MINGW
 *  GCC for MinGW
 * TARGET_COMPILER_VC
 *  Visual C++
 * TARGET_COMPILER_ICC
 *  Intel C++ Compiler
 *
 */

#ifdef __APPLE__
// OS X provides handy headers for most of the stuff here that have
// unfortunate collisions with this file as of XCode 13.1 / Monterey. The
// problem is that they use #define X 0 to indicate that X is false, but we
// leave X undefined for that case. There's not really a great solution to
// this beyond either selective #undefs, or just renaming the prefix of the
// ones we control. For now I have chosen the simpler of those two.
// As of XCode 13.1 we have 10 direct collisions. (There's also many confusing
// ones where e.g. Apple uses TARGET_OS_OSX and we use TARGET_OS_MACOSX, leave
// those alone for now.)
//
// Another option might be to not include this at all. However, it can get
// indirectly included by SDL headers, so it's better to handle it up front.
#include <TargetConditionals.h>
#if defined(USE_SDL)
// this is to prevent breaking SDL because we had undefined TARGET_CPU_ARM
#include <SDL.h>
#endif
#if defined(TARGET_OS_WINDOWS) && !TARGET_OS_WINDOWS
#  undef TARGET_OS_WINDOWS
#endif
#if defined(TARGET_OS_LINUX) && !TARGET_OS_LINUX
#  undef TARGET_OS_LINUX
#endif
#if defined(TARGET_CPU_ARM) && !TARGET_CPU_ARM
#  undef TARGET_CPU_ARM
#endif
#if defined(TARGET_CPU_X86) && !TARGET_CPU_X86
#  undef TARGET_CPU_X86
#endif
#if defined(TARGET_CPU_X86) && !TARGET_CPU_X86_64
#  undef TARGET_CPU_X86_64
#endif
#if defined(TARGET_CPU_PPC) && !TARGET_CPU_PPC
#  undef TARGET_CPU_PPC
#endif
#if defined(TARGET_CPU_PPC64) && !TARGET_CPU_PPC64
#  undef TARGET_CPU_PPC64
#endif
#if defined(TARGET_CPU_MIPS) && !TARGET_CPU_MIPS
#  undef TARGET_CPU_MIPS
#endif
#if defined(TARGET_CPU_SPARC) && !TARGET_CPU_SPARC
#  undef TARGET_CPU_SPARC
#endif
#if defined(TARGET_CPU_ALPHA) && !TARGET_CPU_ALPHA
#  undef TARGET_CPU_ALPHA
#endif
#endif

#ifndef __included_cc_platform_detect_h
#define __included_cc_platform_detect_h

#undef PROCESSOR_DETECTED
#undef COMPILER_DETECTED
#undef OS_DETECTED

/* ------------------- *
* PROCESSOR DETECTION *
* ------------------- */

// Some of these macros (Example: TARGET_CPU_ARM) might be #defined by
// catch2-tests/catch.hpp. This shouldn't affect compilation for crawl
// proper, but this might be the culprit if you're getting weird
// platform.h errors when trying to compile catch2-tests

// from TargetConditionals.h, no further modification needed
#if defined(TARGET_CPU_MIPS) || defined(TARGET_CPU_SPARC) || defined(TARGET_CPU_ALPHA)
#define PROCESSOR_DETECTED
#endif

/* ARM */
#if !defined (PROCESSOR_DETECTED)
#if defined (__arm__) || defined(TARGET_CPU_ARM) || defined(TARGET_CPU_ARM64)
#define PROCESSOR_DETECTED
#define TARGET_CPU_ARM
#endif
#endif

/* DEC Alpha */
#if !defined (PROCESSOR_DETECTED)
#if defined (__alpha) || defined (__alpha__)
#define PROCESSOR_DETECTED
#define TARGET_CPU_ALPHA
#endif
#endif

/* Sun SPARC */
#if !defined (PROCESSOR_DETECTED)
#if defined (__sparc) || defined (__sparc__)
#define PROCESSOR_DETECTED
#define TARGET_CPU_SPARC
#endif
#endif

/* MIPS */
#if !defined (PROCESSOR_DETECTED)
#if defined (__MIPSEL__) || defined (__mips__)
#define PROCESSOR_DETECTED
#define TARGET_CPU_MIPS
#endif
#endif

/* PowerPC */
#if !defined (PROCESSOR_DETECTED)
#if defined(TARGET_CPU_PPC) || defined(TARGET_CPU_PPC64) || defined (_ARCH_PPC) || defined (__ppc__) || defined (__ppc64__) || defined (__PPC) || defined (powerpc) || defined (__PPC__) || defined (__powerpc64__) || defined (__powerpc64)
#define PROCESSOR_DETECTED
#define TARGET_CPU_PPC
#define TARGET_BIG_ENDIAN
#endif
#endif

/* x86_64 or AMD64 or x64 */
#if !defined (PROCESSOR_DETECTED)
#if defined(TARGET_CPU_X86_64) || defined (__x86_64__) || defined (__x86_64) || defined (__amd64) || defined (__amd64__) || defined (_AMD64_) || defined (_M_X64)
#define PROCESSOR_DETECTED
#define TARGET_CPU_X64
#define TARGET_CPU_X86_64
#endif
#endif

/* Intel x86 */
#if !defined (PROCESSOR_DETECTED)
#if defined(TARGET_CPU_X86) || defined (__i386__) || defined (__i386) || defined (i386) || defined (_X86_) || defined (_M_IX86)
#define PROCESSOR_DETECTED
#define TARGET_CPU_X86
#endif
#endif

/* IA64 */
#if !defined (PROCESSOR_DETECTED)
#if defined (__ia64__) || defined (_IA64) || defined (__ia64) || defined (_M_IA64)
#define PROCESSOR_DETECTED
#define TARGET_CPU_IA64
#define TARGET_LITTLE_ENDIAN
#endif
#endif

/* ------------------- *
* COMPILER DETECTION  *
* ------------------- */

#if !defined (COMPILER_DETECTED)
#if defined (__GNUC__)
#define COMPILER_DETECTED
#define TARGET_COMPILER_GCC
#endif
#if defined (__CYGWIN__) || defined (__CYGWIN32__)
#define TARGET_COMPILER_CYGWIN
#endif
#if defined (__MINGW32__)
#define TARGET_COMPILER_MINGW
#endif
#if defined (__DJGPP__)
#define TARGET_COMPILER_DJGPP
#endif
#endif

#if !defined (COMPILER_DETECTED)
#if defined (__INTEL_COMPILER) || defined (__ICL)
#define COMPILER_DETECTED
#define TARGET_COMPILER_ICC
#endif
#endif

#if !defined (COMPILER_DETECTED)
#if defined (_MSC_VER)
#define COMPILER_DETECTED
#define TARGET_COMPILER_VC
#endif
#endif

#if !defined (COMPILER_DETECTED)
#if defined (__BORLANDC__)
/* Earlier Borland compilers break terribly */
#if __BORLANDC__ >= 0x0600
#define COMPILER_DETECTED
#define TARGET_COMPILER_BORLAND
#endif
#endif
#endif

/* ------------ *
* OS DETECTION *
* ------------ */

#if !defined (OS_DETECTED)
#if defined (TARGET_COMPILER_VC) || defined (_WIN32) || defined (_WIN64)
#define OS_DETECTED
#define TARGET_OS_WINDOWS
#endif
#endif

#if !defined (OS_DETECTED)
#if defined (__CYGWIN__) || defined (__CYGWIN32__)
#define OS_DETECTED
#define TARGET_OS_CYGWIN
#endif
#endif

#if !defined (OS_DETECTED)
#if defined (__linux__) || defined (linux) || defined (__linux) || defined (__gnu_linux__)
#define OS_DETECTED
#define TARGET_OS_LINUX
#endif
#endif

#if !defined (OS_DETECTED)
#if defined (MSDOS) || defined (__DOS__) || defined (__DJGPP__)
#define OS_DETECTED
#define TARGET_OS_DOS
#endif
#endif

#if !defined (OS_DETECTED)
#if defined (__sun__)
#define OS_DETECTED
#define TARGET_OS_SOLARIS
#endif
#endif

#if !defined (OS_DETECTED)
#if defined (__FreeBSD__)
#define OS_DETECTED
#define TARGET_OS_FREEBSD
#endif
#endif

#if !defined (OS_DETECTED)
#if defined (__NetBSD__)
#define OS_DETECTED
#define TARGET_OS_NETBSD
#endif
#endif

#if !defined (OS_DETECTED)
#if defined (__OpenBSD__)
#define OS_DETECTED
#define TARGET_OS_OPENBSD
#endif
#endif

#if !defined (OS_DETECTED)
// possible TODO: instead use os x TARGET_ defines to set this
#if defined(__APPLE__)
#define OS_DETECTED
#define TARGET_OS_MACOSX
#endif
#endif

#if !defined (OS_DETECTED)
#if defined (__hurd__)
#define OS_DETECTED
#define TARGET_OS_HURD
#endif
#endif

#if !defined (OS_DETECTED)
#if defined (TARGET_CPU_ARM)
#define OS_DETECTED
#define TARGET_OS_NDSFIRMWARE
#endif
#endif

//#if !defined (PROCESSOR_DETECTED)
//#error "Could not detect target CPU."
//#endif

//#if !defined (COMPILER_DETECTED)
//#error "Could not detect target compiler."
//#endif

#if !defined (OS_DETECTED)
#define TARGET_OS_UNKNOWN
//#error "Could not detect target OS."
#endif

/* ICC on Windows uses VC includes, etc */
#ifdef TARGET_OS_WINDOWS
#ifdef TARGET_COMPILER_ICC
#define TARGET_COMPILER_VC
#endif
#endif

#endif
