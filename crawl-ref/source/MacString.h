/*
 *  File:       MacString.h
 *  Summary:    Wrapper around an immutable CFString.
 *  Written by: Jesse Jones (jesjones@mindspring.com)
 *
 *  Change History (most recent first):
 *
 *      <1>     6/04/02     JDJ     Created
 */

#ifndef MAC_STRING_H
#define MAC_STRING_H

#if macintosh

#include <CoreFoundation/CFBase.h>


// ============================================================================
//	class MacString
//!		Wrapper around an immutable CFString.
// ============================================================================
class MacString {

//-----------------------------------
//	Initialization/Destruction
//
public:
						~MacString();

						MacString();
						
						MacString(const char* str);				
						MacString(const unsigned char* str);				
						/**< Uses default system encoding. */

						MacString(CFStringRef str);
						/**< Bumps the ref count. */
						
						MacString(CFMutableStringRef str);
						/**< Makes a copy. */

	explicit			MacString(int value);

						MacString(const MacString& str);				
			MacString& 	operator=(const MacString& rhs);

//-----------------------------------
//	API
//
public:
	// ----- Size -----
			size_t 		length() const;
			size_t		size() const						{return this->length();}
			bool		empty() const						{return this->length() == 0;}

	// ----- Access -----
			void 		CopyTo(unsigned char* buffer, CFIndex bytes);
						
						operator CFStringRef() const		{return mString;}

//-----------------------------------
//	Member Data
//
private:
	CFStringRef	mString;
};


#endif // macintosh
#endif // MAC_STRING_H
