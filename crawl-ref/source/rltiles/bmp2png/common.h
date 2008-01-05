/*
**  bmp2png --- conversion from (Windows or OS/2 style) BMP to PNG
**  png2bmp --- conversion from PNG to (Windows style) BMP
**
**  Copyright (C) 1999-2005 MIYASAKA Masaru <alkaid@coral.ocn.ne.jp>
**
**  Permission to use, copy, modify, and distribute this software and
**  its documentation for any purpose and without fee is hereby granted,
**  provided that the above copyright notice appear in all copies and
**  that both that copyright notice and this permission notice appear
**  in supporting documentation. This software is provided "as is"
**  without express or implied warranty.
**
**  NOTE: Comments are partly written in Japanese. Sorry.
*/

#ifndef COMMON_H
#define COMMON_H

#if defined(__RSXNT__) && defined(__CRTRSXNT__)
# include <crtrsxnt.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

	/* for stat() */
#include <sys/types.h>
#include <sys/stat.h>

	/* for utime() */
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__LCC__)
# include <sys/utime.h>
# if defined(__LCC__)
   int utime(const char *, struct _utimbuf *);
# endif
#else
# include <utime.h>
#endif
	/* for isatty() */
#if defined(_MSC_VER) || defined(__BORLANDC__) || defined(__MINGW32__) || \
    defined(__LCC__)
# include <io.h>
#else
# include <unistd.h>
#endif
	/* for mkdir() */
#if defined(_MSC_VER) || defined(__BORLANDC__) || defined(__MINGW32__) || \
    defined(__LCC__)
# include <direct.h>
# if defined(__MINGW32__)
#  define MKDIR(d,m) _mkdir(d)
# else
#  define MKDIR(d,m) mkdir(d)
# endif
#else
# if defined(__GO32__) && !defined(__DJGPP__)	/* DJGPP v.1 */
#  include <osfcn.h>
# else
#  include <sys/stat.h>
# endif
# define MKDIR(d,m) mkdir(d,m)
#endif

#if !defined(BINSTDIO_FDOPEN) && !defined(BINSTDIO_SETMODE)
# if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__EMX__) || \
     defined(_MSC_VER) || defined(__BORLANDC__) || defined(__LCC__) || \
     defined(__DJGPP__) || defined(__GO32__)
#  define BINSTDIO_SETMODE
# endif
# if 0 /* defined(__YOUR_COMPLIER_MACRO__) */
#  define BINSTDIO_FDOPEN
# endif
#endif
	/* for setmode() */
#ifdef BINSTDIO_SETMODE
# include <io.h>
# include <fcntl.h>
#endif

#include "png.h"

#if (PNG_LIBPNG_VER < 10004)
# error libpng version 1.0.4 or later is required.
#endif

#if (PNG_LIBPNG_VER == 10207) || (PNG_LIBPNG_VER == 10206) || \
    (PNG_LIBPNG_VER == 10017) || (PNG_LIBPNG_VER == 10016)
# error Libpng versions 1.2.7, 1.2.6, 1.0.17, and 1.0.16
# error have a bug that will cause png2bmp to crash.
# error Update your libpng to latest version.
# error "http://www.libpng.org/pub/png/libpng.html"
#endif

#if !defined(PNG_READ_tRNS_SUPPORTED) || !defined(PNG_WRITE_tRNS_SUPPORTED)
# error This software requires tRNS chunk support.
#endif

#ifndef png_jmpbuf					/* pngconf.h (libpng 1.0.6 or later) */
# define png_jmpbuf(png_ptr) ((png_ptr)->jmpbuf)
#endif

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32)
# define WIN32
#endif
#if defined(__MSDOS__) && !defined(MSDOS)
# define MSDOS
#endif
#if defined(__CYGWIN__)
# undef WIN32
# undef MSDOS
#endif

#if defined(WIN32) || defined(__DJGPP__)
# define WIN32_LFN		/* Win32-style long filename */
#endif

#if defined(WIN32) || defined(MSDOS)
# define PATHDELIM		'\\'
# define DRIVESUFFIX	':'
# define IsPathDelim(c)	((c)==PATHDELIM || (c)=='/')
# define IsOptChar(c)	((c)=='-' || (c)=='/')
# ifdef JAPANESE
#  define IsDBCSLead(c)	((0x81<=(c) && (c)<=0x9F) || (0xE0<=(c) && (c)<=0xFC))
# else
#  define IsDBCSLead(c)	(0)
# endif
#else	/* UNIX */
# define PATHDELIM		'/'
# define IsPathDelim(c)	((c)==PATHDELIM)
# define IsOptChar(c)	((c)=='-')
# define IsDBCSLead(c)	(0)
#endif

typedef char           CHAR;
typedef unsigned char  BYTE;
typedef short          SHORT;
typedef unsigned short WORD;
typedef int            INT;
typedef unsigned int   UINT;
typedef long           LONG;
typedef unsigned long  DWORD;
typedef enum { FALSE = 0, TRUE = 1 } BOOL;

typedef png_color PALETTE;
typedef struct tagIMAGE {
	LONG    width;
	LONG    height;
	UINT    pixdepth;
	UINT    palnum;
	BOOL    topdown;
	BOOL    alpha;
	/* ----------- */
	DWORD   rowbytes;
	DWORD   imgbytes;
	PALETTE *palette;
	BYTE    **rowptr;
	BYTE    *bmpbits;
	/* ----------- */
	png_color_8 sigbit;
} IMAGE;

extern int quietmode;
extern int errorlog;
extern const char errlogfile[];

void xxprintf(const char *, ...);
void set_status(const char *, ...);
void feed_line(void);
void init_progress_meter(png_structp, png_uint_32, png_uint_32);
void row_callback(png_structp, png_uint_32, int);
void png_my_error(png_structp, png_const_charp);
void png_my_warning(png_structp, png_const_charp);
BOOL imgbuf_alloc(IMAGE *);
void imgbuf_free(IMAGE *);
void imgbuf_init(IMAGE *);
int parsearg(int *, char **, int, char **, char *);
char **envargv(int *, char ***, const char *);
int tokenize(char *, const char *);
int makedir(const char *);
int renbak(const char *);
int cpyftime(const char *, const char *);
FILE *binary_stdio(int);
char *suffix(const char *);
char *basname(const char *);
char *addslash(char *);
char *delslash(char *);
char *path_skiproot(const char *);
char *path_nextslash(const char *);
#ifdef WIN32_LFN
int is_dos_filename(const char *);
#endif

#endif /* COMMON_H */
