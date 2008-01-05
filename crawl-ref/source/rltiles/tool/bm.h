#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int bmread(char *fn, int *x, int *y, unsigned char *buf3[3]);
extern void fixalloc(char *buf[3], int size);
extern void bmwrite(char *fn, int x, int y, unsigned char *buf3[3]);
extern void bmwrite24(char *fn, int x, int y, unsigned char *buf3[3]);
extern void bmwrite_dither(char *fn, int x, int y, unsigned char *buf3[3], 
                           unsigned char *flag);
extern void stdpal();
extern void myfget(char *ss, FILE *fp);
extern int getval(char *buf, char *tag, int *val);
extern int getname(char *buf, char *tag, char *name);
extern void process_cpath(char *path);
extern void newgold();


/*** PATH to this program ***/
extern char cpath[1024];
#if defined(_WIN32)|| defined(WINDOWS)
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

