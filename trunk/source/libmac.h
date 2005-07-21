/*
 *  File:       libmac.h
 *  Summary:    Mac specific routines used by Crawl.
 *  Written by: Jesse Jones
 *
 *  Change History (most recent first): 
 *
 *    <2>     5/25/02     JDJ       Updated for Mach-O targets
 *    <1>     3/23/99     JDJ       Created
 */
 
#ifndef LIBMAC_H
#define LIBMAC_H

#if macintosh

#ifdef _BSD_SIZE_T_			// $$$ is there a better way to test for OS X?
	#define OSX	1
#else
	#define OS9	1
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#if OSX
	#include <unistd.h>
#endif

#define MAC_NUMBER_OF_LINES     30

// constants
const int _NORMALCURSOR = 1;
const int _NOCURSOR = 0;


// non-ANSI functions
int stricmp(const char* lhs, const char* rhs);
char* strlwr(char* str);
void itoa(int n, char* buffer, int radix);

#if !OSX
	inline int random()
	{
	    return rand();
	}

	inline void srandom(unsigned int seed)
	{
	    srand(seed);
	}

	int open(const char* path, int openFlags, int permissions);
	int open(const char* path, int openFlags, int permissions, int mysteryFlags);
	int close(int desc);
	int read(int desc, void *buffer, unsigned int bytes);
	int write(int desc, const void *buffer, unsigned int bytes);
	int unlink(const char* path);
#endif


// curses(?) functions
void clrscr();
void gotoxy(int x, int y);
void textcolor(int c);
void cprintf(const char* format,...);

void window(int x, int y, int lx, int ly);
int wherex();
int wherey();
void putch(char c);
int kbhit();

char getche();
int getch();
void getstr(char* buffer, int bufferSize);

void textbackground(int c);
void _setcursortype(int curstype);


// misc functions
void delay(int ms);

void init_mac();
void deinit_mac();


#endif // macintosh
#endif // LIBMAC_H
