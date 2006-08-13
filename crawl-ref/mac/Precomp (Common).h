/*
 *  File:       Precomp (Common).h
 *  Summary:   	The header included by the CodeWarrior precompiled header files.
 *  Written by: Jesse Jones
 *
 *  Change History (most recent first):	
 *
 *    <1>     5/25/02     JDJ     Created
 */
 
// In order for precompiled headers to work on MSVC this header must be included
// before *anything* else.
#if _MSC_VER >= 1100
	#pragma message("Compiling Precomp (Common).h (this message should only appear once per project)")
#endif

#ifndef	PRECOMP_COMMON_H
#define	PRECOMP_COMMON_H


// ===================================================================================
//	Debug Macros
// ===================================================================================
#ifdef _DEBUG
	#define	DEBUG					1
	#define RELEASE					0	
	
	#if __MWERKS__
		#define MSIPL_DEBUG_MODE
	#endif
#else		
	#define	DEBUG					0
	
	#if __profile__
		#define RELEASE				0
	#else
		#define RELEASE				0	
	#endif
	
	#if	!defined(NDEBUG)
		#define	NDEBUG							// used by <assert.h>
	#endif
#endif


// ===================================================================================
//	Misc Macros
// ===================================================================================
#if MAC
	#define TARGET_API_MAC_CARBON	1
	#define __CF_USE_FRAMEWORK_INCLUDES__
#endif

#if MAC && !defined(macintosh)	// macintosh isn't defined for MACH-O
	#define macintosh	1
#endif

#include <mslconfig>


// ===================================================================================
//	C++ Includes
// ===================================================================================
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <limits>
#include <list>						
#include <map>
#include <set>
#include <string>
#include <vector>


#endif	// PRECOMP_COMMON_H