/*
 *  File:       MacString.cc
 *  Summary:    Wrapper around an immutable CFString.
 *  Written by: Jesse Jones (jesjones@mindspring.com)
 *
 *  Change History (most recent first):
 *
 *      <1>     6/04/02     JDJ     Created
 */

#include "AppHdr.h"
#include "MacString.h"

#if macintosh

#include <CoreFoundation/CFString.h>

#include "debug.h"


// ========================================================================
//	Internal Functions
// ========================================================================

//---------------------------------------------------------------
//
// ThrowIf
//
//---------------------------------------------------------------
static void ThrowIf(bool predicate, const std::string& text)
{
	if (predicate)
		throw std::runtime_error(text);
}

#if __MWERKS__
#pragma mark -
#endif

// ============================================================================
//	class MacString
// ============================================================================	

//---------------------------------------------------------------
//
// MacString::~MacString
//
//---------------------------------------------------------------
MacString::~MacString()
{
	CFRelease(mString);
}


//---------------------------------------------------------------
//
// MacString::MacString ()
//
//---------------------------------------------------------------
MacString::MacString()
{	
	mString = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, NULL, 0);
	ThrowIf(mString == NULL, "Couldn't create the CFString");
}


//---------------------------------------------------------------
//
// MacString::MacString (unsigned char*)
//
//---------------------------------------------------------------
MacString::MacString(const unsigned char* str)
{
	ASSERT(str != NULL);
	
	CFStringEncoding encoding = CFStringGetSystemEncoding();
	mString = CFStringCreateWithPascalString(kCFAllocatorSystemDefault, str, encoding);
	ThrowIf(mString == NULL, "Couldn't create the CFString");
}


//---------------------------------------------------------------
//
// MacString::MacString (char*)
//
//---------------------------------------------------------------
MacString::MacString(const char* str)
{
	ASSERT(str != NULL);
	
	CFStringEncoding encoding = CFStringGetSystemEncoding();
	mString = CFStringCreateWithCString(kCFAllocatorSystemDefault, str, encoding);
	ThrowIf(mString == NULL, "Couldn't create the CFString");
}


//---------------------------------------------------------------
//
// MacString::MacString (CFStringRef)
//
//---------------------------------------------------------------
MacString::MacString(CFStringRef str)
{
	ASSERT(str != NULL);
	
	mString = str;
	CFRetain(mString);
}


//---------------------------------------------------------------
//
// MacString::MacString (CFMutableStringRef)
//
//---------------------------------------------------------------
MacString::MacString(CFMutableStringRef str)
{
	ASSERT(str != NULL);
	
	mString = CFStringCreateCopy(kCFAllocatorSystemDefault, str);	
	ThrowIf(mString == NULL, "Couldn't create the CFString");
}


//---------------------------------------------------------------
//
// MacString::MacString (int)
//
//---------------------------------------------------------------
MacString::MacString(int value)
{
	char buffer[32];
	sprintf(buffer, "%d", value);
	
	CFStringEncoding encoding = CFStringGetSystemEncoding();
	mString = CFStringCreateWithCString(kCFAllocatorSystemDefault, buffer, encoding);
	ThrowIf(mString == NULL, "Couldn't create the CFString");
}


//---------------------------------------------------------------
//
// MacString::MacString (MacString)
//
//---------------------------------------------------------------
MacString::MacString(const MacString& str)
{	
	mString = str.mString;			// immutable so we can refcount
	CFRetain(mString);
}
				

//---------------------------------------------------------------
//
// MacString::operator= (MacString)
//
//---------------------------------------------------------------
MacString& MacString::operator=(const MacString& rhs)
{
	if (this != &rhs) 
	{
		CFRelease(mString);

		mString = rhs.mString;		// immutable so we can refcount
		CFRetain(mString);
	}
	
	return *this;
}


//---------------------------------------------------------------
//
// MacString::length
//
//---------------------------------------------------------------
size_t MacString::length() const
{
	size_t len = (size_t) CFStringGetLength(mString);
	
	return len;
}


//---------------------------------------------------------------
//
// MacString::CopyTo
//
//---------------------------------------------------------------
void MacString::CopyTo(unsigned char* buffer, CFIndex bytes)
{
	ASSERT(buffer != NULL || bytes == 0);
	
	bool converted = CFStringGetPascalString(mString, buffer, bytes, CFStringGetSystemEncoding());
	ThrowIf(!converted, "Couldn't convert the CFString into a Pascal string");	
}


#endif // macintosh
