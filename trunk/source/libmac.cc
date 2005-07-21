/*
 *  File:       libmac.cc
 *  Summary:    Mac specific routines used by Crawl.
 *  Written by: Jesse Jones (jesjones@mindspring.com)
 *
 *  Change History (most recent first):
 *
 *      <5>     5/25/02     JDJ     Rewrote to use Carbon Events and Mach-O.
 *      <4>     9/25/99     CDL     linuxlib -> liblinux
 *
 *      <3>     5/30/99     JDJ     Quit only pops up save changes dialog if game_has_started is true.
 *      <2>     4/24/99     JDJ     HandleMenu calls save_game instead of returning a 'S'
 *                                  character ('S' now prompts). kTermHeight is now initialized
 *                                  to NUMBER_OF_LINES.
 *      <2>     4/24/99     JDJ     getstr only adds printable characters to the buffer.
 *      <1>     3/23/99     JDJ     Created
 */

#include "AppHdr.h"
#include "libmac.h"

#if macintosh

#include <cstdarg>
#include <ctype.h>
#include <string.h>
#include <vector.h>

#include <CarbonCore/StringCompare.h>
#include <HIToolbox/CarbonEvents.h>
#include <HIToolbox/Dialogs.h>

#include "debug.h"
#include "defines.h"
#include "files.h"
#include "MacString.h"
#include "version.h"


//-----------------------------------
//      Forward References
//
class CApplication;


//-----------------------------------
//      Constants
//
const int kTermWidth = 80;
const int kTermHeight = MAC_NUMBER_OF_LINES;

const int kLeftArrowKey = 0x7B;
const int kUpArrowKey = 0x7E;
const int kRightArrowKey = 0x7C;
const int kDownArrowKey = 0x7D;

const int kNumPad0Key = 0x52;
const int kNumPad1Key = 0x53;
const int kNumPad2Key = 0x54;
const int kNumPad3Key = 0x55;
const int kNumPad4Key = 0x56;
const int kNumPad5Key = 0x57;
const int kNumPad6Key = 0x58;
const int kNumPad7Key = 0x59;
const int kNumPad8Key = 0x5B;
const int kNumPad9Key = 0x5C;

const int kNumPadMultKey = 0x43;
const int kNumPadAddKey = 0x45;
const int kNumPadSubKey = 0x4E;
const int kNumPadDecimalKey = 0x41;
const int kNumPadDivideKey = 0x4B;

const char kEnterChar = 0x03;
const short kEscapeKey = 0x35;
const char kCheckMarkChar = 0x12;
const char kNoMarkChar 	  = 0x00;				

const short kSaveBtn = 1;
const short kCancelBtn = 2;
const short kDontSaveBtn = 3;

const Rect kBigRect = {0, 0, 32000, 32000};

const RGBColor kBlack = {0, 0, 0};
const RGBColor kWhite = {65535, 65535, 65535};

enum EAskSaveResult
{
    kSaveChanges = 1,
    kCancelSave = 2,
    kDontSave = 3
};


//-----------------------------------
//      Variables
//
static CApplication* sApp = NULL;
static CTabHandle sColors = NULL;

static bool sInDialog = false;

extern bool game_has_started;


// ========================================================================
//	Internal Functions
// ========================================================================

//---------------------------------------------------------------
//
// CreateSpec
//
//---------------------------------------------------------------
static EventTypeSpec CreateSpec(UInt32 c, UInt32 k)
{
	EventTypeSpec spec; 
	
	spec.eventClass = c;
	spec.eventKind = k;
	
	return spec;
}


//---------------------------------------------------------------
//
// DrawChar
//
//---------------------------------------------------------------
inline void DrawChar(short x, short y, char ch)
{
    MoveTo(x, y);
    DrawText(&ch, 0, 1);
}


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


//---------------------------------------------------------------
//
// ThrowIfOSErr
//
//---------------------------------------------------------------
static void ThrowIfOSErr(OSErr err, const std::string& text)
{
	if (err != noErr)
	{
		char buffer[256];
		sprintf(buffer, "%s (%d)", text.c_str(), err);
		
		throw std::runtime_error(buffer);
	}
}


//---------------------------------------------------------------
//
// FlashButton
//
//---------------------------------------------------------------
static void FlashButton(DialogPtr dialog, short item)
{
	ASSERT(dialog != nil);
	ASSERT(item >= 1 && item <= CountDITL(dialog));

	ControlRef control;
	(void) GetDialogItemAsControl(dialog, item, &control);

	HiliteControl(control, 1);
//	QDAddRectToDirtyRegion(GetDialogPort(dialog), &kBigRect);
	QDFlushPortBuffer(GetDialogPort(dialog), NULL);
	
	unsigned long ticks;
	Delay(8, &ticks);
	
	HiliteControl(control, 0);
//	QDAddRectToDirtyRegion(GetDialogPort(dialog), &kBigRect);
	QDFlushPortBuffer(GetDialogPort(dialog), NULL);
}


//---------------------------------------------------------------
//
// WarnUser
//
// Pops up an error dialog.
//
//---------------------------------------------------------------
static void WarnUser(const char* message)
{
	int len = strlen(message);
	if (len > 250)
		len = 250;

	Str255 text;
	for (int i = 0; i < len; i++)
		text[i + 1] = message[i];
	text[0] = len;

	AlertStdAlertParamRec params;
	params.movable       = true;
	params.helpButton    = false;
	params.filterProc    = NULL;
	params.defaultText   = (StringPtr) -1L;  // use default (ie "OK")
	params.cancelText    = NULL;
	params.otherText     = NULL;
	params.defaultButton = 1;
	params.cancelButton  = 0;
	params.position      = kWindowAlertPositionParentWindowScreen;

	short item;
	sInDialog = true;
	OSErr err = StandardAlert(kAlertCautionAlert, text, "\p", &params, &item);
	ASSERT(err == noErr);   // seems kind of pointless to throw
	sInDialog = false;
}


//---------------------------------------------------------------
//
// SaveChangesFilter
//
//---------------------------------------------------------------
static pascal Boolean SaveChangesFilter(DialogPtr dptr, EventRecord* event, short* item)
{
	bool handled = false;

	if (event->what == keyDown)
	{
		char ch = (char) toupper((char) (event->message & charCodeMask));
		short key = (short) ((event->message & keyCodeMask) >> 8);

		if (ch == 'S')
		{
			*item = 1;
			handled = true;
		}
		else if (ch == 'D')
		{
			*item = 3;
			handled = true;
		}
		else if (ch == '\r' || ch == kEnterChar)
		{
			*item = 1;
			handled = true;
		}
		else if (key == kEscapeKey)
		{
			*item = 2;
			handled = true;
		}

		if (handled)
			FlashButton(dptr, *item);
	}

	return handled;
}


//---------------------------------------------------------------
//
// AskSaveChanges
//
//---------------------------------------------------------------
static EAskSaveResult AskSaveChanges()
{
#if 1
	static ModalFilterUPP filterProc = NewModalFilterUPP(SaveChangesFilter);
	
	AlertStdAlertParamRec params;
	params.movable       = true;
	params.helpButton    = false;
	params.filterProc    = filterProc;
	params.defaultText   = "\pSave";
	params.cancelText    = "\pCancel";
	params.otherText     = "\pDon't Save";
	params.defaultButton = kSaveBtn;
	params.cancelButton  = kCancelBtn;
	params.position      = kWindowAlertPositionParentWindowScreen;

	short item = kSaveBtn;
	sInDialog = true;
	OSErr err = StandardAlert(kAlertStopAlert, "\pDo you want to save your game before quitting?", "\p", &params, &item);
	ASSERT(err == noErr);   // seems kind of pointless to throw
	sInDialog = false;

	EAskSaveResult result;
	if (item == kSaveBtn)
		result = kSaveChanges;
	else if (item == kDontSaveBtn)
		result = kDontSave;
	else
		result = kCancelSave;

	return result;

#else
	NavDialogOptions options;
	OSStatus err = NavGetDefaultDialogOptions(&options);
	ASSERT(err == noErr);						// seems kind of pointless to throw

	NavAskSaveChangesResult reply = kSaveChanges;
	if (err == noErr)
	{
		std::string name = "foobar";
		UInt32 length = std::min(name.size(), sizeof(options.savedFileName) - 1);
		BlockMoveData(name.c_str(), options.savedFileName + 1, length);
		options.savedFileName[0] = length;

		err = NavAskSaveChanges(&options,	
								action,
								&reply,
								NULL,
								0UL);
		ASSERT(err == noErr);					// seems kind of pointless to throw
	}
	
	return (EAskSaveResult) reply;
#endif
}


//---------------------------------------------------------------
//
// ConvertColor
//
//---------------------------------------------------------------
static RGBColor ConvertColor(int c)
{
    ASSERT(c >= 0);
    ASSERT(c < 16);

    RGBColor color;

    if (c == BLACK)
        color = (**sColors).ctTable[15].rgb;    // QD likes black and white swapped from DOS indices

    else if (c == WHITE)
        color = (**sColors).ctTable[0].rgb;
    else
        color = (**sColors).ctTable[c].rgb;

    return color;
}

#if __MWERKS__
#pragma mark -
#endif

// ============================================================================
//	class CApplication
// ============================================================================
class CApplication {

//-----------------------------------
//	Initialization/Destruction
//
public:
				~CApplication();														
				CApplication();

//-----------------------------------
//	API
//
public:			
	void		Quit();
	
	char 		GetChar();
				// Block until a key is pressed.

	bool 		PeekChar();
				// Return true if a key event is on the event queue.

	void 		Clear();
	void 		SetCursor(int x, int y);
	Point 		GetCursor() const						{return mCursor;}
	void 		SetChar(unsigned char ch);
	void 		Print(const char* buffer);
	void 		ShowCursor(bool show)					{mShowCursor = show;}

	void 		SetForeColor(const RGBColor& color);
	void 		SetBackColor(const RGBColor& color);
	
    void 		SetFont(const unsigned char *name)		{this->DoSetFont(name, mPointSize);}
    void 		SetFontSize(int size)					{this->DoSetFont(mFontName, size);}

//-----------------------------------
//	Internal Types
//
private:
    struct SCell {
		unsigned char ch;
		RGBColor color;

		SCell()
		{
			ch = ' ';
			color.red = color.green = color.blue = 65535;
		}
    };

    typedef vector<SCell> Line;

//-----------------------------------
//	Internal API
//
private:
	void 		DoAbout();
    void 		DoClearToEOL();
    void 		DoDrawCell(const Rect& area, const SCell& cell);
	OSStatus 	DoEnableCommand(MenuRef menuH, MenuCommand command, MenuItemIndex index);
	OSStatus 	DoHandleCommand(MenuCommand command);
    void 		DoInitMenus();
    void 		DoInitWindows();
	char 		DoMungeChar(char inChar, UInt32 inKey, UInt32 modifiers) const;
	OSStatus 	DoOpenMenu(MenuRef menuH);
	void 		DoReadPrefs();
	void 		DoRender();
    void 		DoScroll();
    void 		DoSetChar(unsigned char ch);
	void 		DoSetFont(const unsigned char* name, int size);
	void 		DoWritePrefs();
			
	static pascal OSStatus 	DoKeyDown(EventHandlerCallRef handler, EventRef event, void* refCon);
	static pascal OSStatus 	DoMenuEvent(EventHandlerCallRef handler, EventRef event, void* refCon);
	static pascal OSErr 	DoQuit(const AppleEvent* event, AppleEvent* reply, SInt32 refCon);
	static pascal OSStatus 	DoWindowEvent(EventHandlerCallRef handler, EventRef event, void* refCon);

//-----------------------------------
//	Member Data
//
private:
	WindowRef		mWindow;
    vector<Line>	mLines;
    char			mChar;
	
    Point 			mCursor;
    bool 			mShowCursor;

    RGBColor 		mForeColor;
    RGBColor 		mBackColor;

    Str255 			mFontName;
    int 			mPointSize;
    UInt32			mNumFonts;
    MenuRef 		mFontMenu;
    
    short 			mFontNum;
	int				mAscent;
	int				mCellHeight;
	int				mCellWidth;		
};
	
//---------------------------------------------------------------
//
// CApplication::~CApplication
//
//---------------------------------------------------------------
CApplication::~CApplication()
{
    this->DoWritePrefs();

	DisposeWindow(mWindow);
}


//---------------------------------------------------------------
//
// CApplication::CApplication
//
//---------------------------------------------------------------
CApplication::CApplication() 
{
	InitCursor();
	
    mCursor.h = 0;
    mCursor.v = 0;
    mShowCursor = false;
    mChar = 0;

	mNumFonts = 0;
    mFontName[0] = '\0';
    mFontNum = -2;
    mPointSize = 0;
	mAscent = 0;
	mCellHeight = 0;
	mCellWidth = 0;
	mFontMenu = NULL;

    mForeColor = kWhite;
    mBackColor = kBlack;

	try
	{
		// install a handler for the quit apple event
		OSStatus err = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, NewAEEventHandlerUPP(DoQuit), 0, false);
		ThrowIfOSErr(err, "Couldn't install the quit handler!");
		
		// install a custom key handler
		std::vector<EventTypeSpec> specs;
		specs.push_back(CreateSpec(kEventClassKeyboard, kEventRawKeyDown));
		specs.push_back(CreateSpec(kEventClassKeyboard, kEventRawKeyRepeat));
		
		err = InstallApplicationEventHandler(NewEventHandlerUPP(DoKeyDown), specs.size(), &specs[0], this, NULL);
		ThrowIfOSErr(err, "Couldn't install the key handler!");

		// create the window
		this->DoInitWindows();	
		
		// init the menus
		this->DoInitMenus();
		
		// update some state
    	this->DoReadPrefs();

	    this->Clear();
	
		// show the window
		ShowWindow(mWindow);
	} 
	catch (const std::exception& e)
	{
        WarnUser(e.what());
        ExitToShell();
	}
	catch (...)
	{
        WarnUser("Couldn't initialize the application.");
        ExitToShell();
	}
}


//---------------------------------------------------------------
//
// CApplication::Quit
//
//---------------------------------------------------------------
void CApplication::Quit()
{
    if (game_has_started)
    {
        EAskSaveResult answer = AskSaveChanges();

        if (answer == kSaveChanges)
        {
            save_game(true);
        }
        else if (answer == kDontSave)
        {
            deinit_mac();
            ExitToShell();
        }
    }
    else
    {
        deinit_mac();
        ExitToShell();
    }
}


//---------------------------------------------------------------
//
// CApplication::Clear
//
//---------------------------------------------------------------
void CApplication::Clear()
{
    mLines.resize(kTermHeight);

    for (int y = 0; y < mLines.size(); ++y)
    {
        Line& line = mLines[y];
        line.resize(kTermWidth);

        for (int x = 0; x < line.size(); ++x)
        {
            SCell& cell = line[x];

            cell.ch = ' ';
            cell.color = kWhite;
        }
    }

    mCursor.h = 0;
    mCursor.v = 0;

	(void) InvalWindowRect(mWindow, &kBigRect);
}


//---------------------------------------------------------------
//
// CApplication::SetCursor
//
//---------------------------------------------------------------
void CApplication::SetCursor(int x, int y)
{
    ASSERT(x >= 0);
    ASSERT(x < kTermWidth);
    ASSERT(y >= 0);
    ASSERT(y < kTermHeight);

    if (x != mCursor.h || y != mCursor.v)
    {
        if (mShowCursor)
        {
            Rect area;
            area.top    = mCursor.v * mCellHeight;
            area.bottom = area.top + mCellHeight;
            area.left   = mCursor.h * mCellWidth;
            area.right  = area.left + mCellWidth;

            (void) InvalWindowRect(mWindow, &area);
        }

        mCursor.h = x;
        mCursor.v = y;

        if (mShowCursor)
        {
            Rect area;
            area.top    = mCursor.v * mCellHeight;
            area.bottom = area.top + mCellHeight;
            area.left   = mCursor.h * mCellWidth;
            area.right  = area.left + mCellWidth;

            (void) InvalWindowRect(mWindow, &area);
        }
    }
}


//---------------------------------------------------------------
//
// CApplication::SetChar
//
//---------------------------------------------------------------
void CApplication::SetChar(unsigned char ch)
{
    ASSERT(ch != '\t');
    ASSERT(mLines.size() == kTermHeight);

    const int TABSIZE = 8;

    int x = mCursor.h;          // this is from the ncurses source
    int y = mCursor.v;

    switch (ch) {
	    case '\t':
	        x += (TABSIZE - (x % TABSIZE));

	        // Space-fill the tab on the bottom line so that we'll get the
	        // "correct" cursor position.
	        if (x < kTermWidth)
	        {
	            char blank = ' ';

	            while (mCursor.h < x)
	                this->DoSetChar(blank);
	            break;

	        }
	        else
	        {
	            this->DoClearToEOL();
	            if (++y >= kTermHeight)
	            {
	                x = kTermWidth - 1;
	                y--;
	                this->DoScroll();
	                x = 0;
	            }
	            else
	                x = 0;
	        }
	        break;

#if 1
	    case '\n':
	    case '\r':
	        this->DoClearToEOL();
	        if (++y >= kTermHeight)
	        {
	            y--;
	            this->DoScroll();
	        }
	        x = 0;
	        break;

#else
	    case '\n':
	        this->DoClearToEOL();
	        if (++y >= kTermHeight)
	        {
	            y--;
	            this->DoScroll();
	        }
	        /* FALLTHRU */

	    case '\r':
	        x = 0;
	        break;
#endif

	    case '\b':
	        if (x == 0)
	            return;
	        mCursor.h--;
	        this->DoSetChar(' ');
	        x--;
	        break;

	    case 159:
	    case 176:
	    case 177:
	    case 220:
	    case 239:
	    case 240:
	    case 247:
	    case 249:
	    case 250:
	    case 206:
	    case 254:
	        this->DoSetChar(ch);
	        return;
	        break;

	    default:
	        if (ch == '\0')
	            ch = ' ';

//     	 	ASSERT(ch >= ' ');
//     		ASSERT(ch <= '~');

	        if (ch >= ' ' && ch <= '~')
	            this->DoSetChar(ch);
	        else
	            this->DoSetChar('À');
	        return;
    }

    mCursor.h = x;
    mCursor.v = y;
}


//---------------------------------------------------------------
//
// CApplication::Print
//
//---------------------------------------------------------------
void CApplication::Print(const char* buffer)
{
    ASSERT(buffer != NULL);

    const char* p = buffer;

    while (*p != '\0')
    {
        char ch = *p++;

        this->SetChar(ch);
    }
}


//---------------------------------------------------------------
//
// CApplication::SetForeColor
//
//---------------------------------------------------------------
void CApplication::SetForeColor(const RGBColor & color)
{
    if (color.red != mForeColor.red || color.green != mForeColor.green || color.blue != mForeColor.blue)
    {
        mForeColor = color;

		(void) InvalWindowRect(mWindow, &kBigRect);
    }
}


//---------------------------------------------------------------
//
// CApplication::SetBackColor
//
//---------------------------------------------------------------
void CApplication::SetBackColor(const RGBColor & color)
{
    if (color.red != mBackColor.red || color.green != mBackColor.green || color.blue != mBackColor.blue)
    {
        mBackColor = color;

		(void) InvalWindowRect(mWindow, &kBigRect);
    }
}


//---------------------------------------------------------------
//
// CApplication::GetChar
//
//---------------------------------------------------------------
char CApplication::GetChar()
{	
	mChar = 0;
	RunApplicationEventLoop();
		
	(void) InvalWindowRect(mWindow, &kBigRect);
				
	return mChar;
}


//---------------------------------------------------------------
//
// CApplication::PeekChar
//
//---------------------------------------------------------------
bool CApplication::PeekChar()
{	
	EventTypeSpec specs[2];
	specs[0] = CreateSpec(kEventClassKeyboard, kEventRawKeyDown);
	specs[1] = CreateSpec(kEventClassKeyboard, kEventRawKeyRepeat);
	
	EventRef event = NULL;
	OSStatus err = ReceiveNextEvent(2, specs, kEventDurationNoWait, false, &event);
	
	return err == noErr;
}

#if __MWERKS__
#pragma mark ~
#endif

//---------------------------------------------------------------
//
// CApplication::DoAbout
//
//---------------------------------------------------------------
void CApplication::DoAbout()
{
	AlertStdAlertParamRec params;
	params.movable       = true;
	params.helpButton    = false;
	params.filterProc    = NULL;
	params.defaultText   = (StringPtr) -1L;  // use default (ie "OK")	VERSION
	params.cancelText    = NULL;
	params.otherText     = NULL;
	params.defaultButton = 1;
	params.cancelButton  = 0;
	params.position      = kWindowAlertPositionParentWindowScreen;
	
	short item;
	sInDialog = true;
	OSErr err = StandardAlert(kAlertNoteAlert, "\p Crawl " VERSION, "\p© 1997-2002 by Linley Henzell\nMac Port by Jesse Jones", &params, &item);
	ASSERT(err == noErr);   // seems kind of pointless to throw
	sInDialog = false;
}


//---------------------------------------------------------------
//
// CApplication::DoClearToEOL
//
//---------------------------------------------------------------
void CApplication::DoClearToEOL()
{
    ASSERT(mCursor.h < kTermWidth);
    ASSERT(mCursor.v < kTermHeight);

    Line& line = mLines[mCursor.v];
    for (int x = mCursor.h; x < kTermWidth; ++x)
    {
        SCell& cell = line[x];
        cell.ch = ' ';
    }

    Rect area;
    area.top    = mCursor.v * mCellHeight;
    area.bottom = area.top + mCellHeight;
    area.left   = mCursor.h * mCellWidth;
    area.right  = 16000;

	(void) InvalWindowRect(mWindow, &kBigRect);
}


//---------------------------------------------------------------
//
// CApplication::DoDrawCell
//
//---------------------------------------------------------------
void CApplication::DoDrawCell(const Rect& area, const SCell& cell)
{
    RGBForeColor(&cell.color);
    
    switch (cell.ch) {
	    case 159:                   // fountain
            DrawChar(area.left, area.top + mAscent, '´');
	        break;

	    case 177:                   // wall
	    case 176:
	        PaintRect(&area);
	        break;

	    case 247:                   // water/lava
	        PaintRect(&area);
	        break;

	    case 249:                   // floor
	    case 250:                   // undiscovered trap?
//     		FillRect(&area, &qd.gray);
            DrawChar(area.left, area.top + mAscent, '.');
	        break;

	    case 206:
	    case 254:                   // door
	    	{
	    	Rect temp = area;
	        InsetRect(&temp, 2, 2);
	        PaintRect(&temp);
	        }
	        break;

	    case 220:                   // altar
            DrawChar(area.left, area.top + mAscent, 'Æ');
	        break;

	    case 239:                   // staircase to hell
	    case 240:                   // branch staircase
            DrawChar(area.left, area.top + mAscent, '²');
	        break;

	    default:
            DrawChar(area.left, area.top + mAscent, cell.ch);
    }
}


//---------------------------------------------------------------
//
// CApplication::DoEnableCommand
//
//---------------------------------------------------------------
OSStatus CApplication::DoEnableCommand(MenuRef menuH, MenuCommand command, MenuItemIndex index)
{
	OSStatus err = noErr;	
	
	if (command == 'Abut')
	{
		EnableMenuItem(menuH, index);
	}
	else if (command == 'Save')
	{
		if (game_has_started)
			EnableMenuItem(menuH, index);
		else
			DisableMenuItem(menuH, index);
	}
	else if (command >= 'Size' && command <= 'Size'+128)
	{
		if (mPointSize == command - 'Size')
			SetItemMark(menuH, index, kCheckMarkChar);
		else
			SetItemMark(menuH, index, kNoMarkChar);
	}
	else if (command >= 'Font' && command <= 'Font'+128)
	{
		Str255 name;
		GetMenuItemText(menuH, index, name);

		if (EqualString(mFontName, name, true, true))
			SetItemMark(menuH, index, kCheckMarkChar);
		else
			SetItemMark(menuH, index, noMark);
	}
	else
		err = eventNotHandledErr;			

	return err;
}


//---------------------------------------------------------------
//
// CApplication::DoHandleCommand
//
//---------------------------------------------------------------
OSStatus CApplication::DoHandleCommand(MenuCommand command)
{
	OSStatus err = noErr;
		
	if (command == 'Abut')
	{
		this->DoAbout();
	}
	else if (command == 'Save')
	{
		save_game(false);
	}
	else if (command >= 'Size' && command <= 'Size'+128)
	{
		int size = command - 'Size';

		this->SetFontSize(size);
	}
	else if (command >= 'Font' && command <= 'Font'+128)
	{
		int index = command - 'Font';

		Str255 name;
		GetMenuItemText(mFontMenu, index, name);

		this->SetFont(name);
	}
	else if (command == kHICommandQuit)
	{
		this->Quit();
	}
	else
		err = eventNotHandledErr;			

	return err;
}


//---------------------------------------------------------------
//
// CApplication::DoInitMenus
//
//---------------------------------------------------------------
void CApplication::DoInitMenus()
{
	const int kFileMenu = 257;
	const int kFontMenu = 258;
	const int kSizeMenu = 259;

	// add the About menu item
	MenuRef menuH = NewMenu(0, "\pð");
	
	OSStatus err = InsertMenuItemTextWithCFString(menuH, MacString("About Crawl"), 0, kMenuItemAttrIgnoreMeta, 'Abut');
	ThrowIfOSErr(err, "Couldn't add the about menu item!");	

	InsertMenu(menuH, 0);
	
	// create the File menu
	err = CreateNewMenu(kFileMenu, kMenuAttrAutoDisable, &menuH);
	ThrowIfOSErr(err, "Couldn't create the file menu!");
	
	err = SetMenuTitleWithCFString(menuH, MacString("File"));
	ThrowIfOSErr(err, "Couldn't set the file menu name!");

	InsertMenu(menuH, 0);
	
	// add the File menu items
	err = AppendMenuItemTextWithCFString(menuH, MacString("Save"), kMenuItemAttrIgnoreMeta, 'Save', NULL);
	ThrowIfOSErr(err, "Couldn't add the save menu item!");	
			
	err = SetMenuItemCommandKey(menuH, 1, false, 'S');
	ThrowIfOSErr(err, "Couldn't set the save menu item's command key!");

	// create the Font menu
	err = CreateNewMenu(kFontMenu, kMenuAttrAutoDisable, &mFontMenu);
	ThrowIfOSErr(err, "Couldn't create the font menu!");
	
	err = CreateStandardFontMenu(mFontMenu, 0, 0, kNilOptions, &mNumFonts);
	ThrowIfOSErr(err, "Couldn't initialize the font menu!");
	
	err = SetMenuTitleWithCFString(mFontMenu, MacString("Font"));
	ThrowIfOSErr(err, "Couldn't set the font menu name!");

	UInt16 numItems = CountMenuItems(mFontMenu);
	for (UInt16 index = 1; index <= numItems; ++index)
	{
		// set the font for each menu item so we're WYSIWYG
		Str255 fontName;
		GetMenuItemText(mFontMenu, index, fontName);

		short fontNum;
		GetFNum(fontName, &fontNum);

		SetMenuItemFontID(mFontMenu, index, fontNum);
		
		// set the command id so we can process the items (CreateStandardFontMenu
		// leaves all of these at 0)
		err = SetMenuItemCommandID(mFontMenu, index, 'Font' + index);
		ThrowIfOSErr(err, "Couldn't set the font menu item's command ids!");
	}

	InsertMenu(mFontMenu, 0);
	
	// create the Size menu
	err = CreateNewMenu(kSizeMenu, kMenuAttrAutoDisable, &menuH);
	ThrowIfOSErr(err, "Couldn't create the size menu!");
	
	err = SetMenuTitleWithCFString(menuH, MacString("Size"));
	ThrowIfOSErr(err, "Couldn't set the size menu name!");

	InsertMenu(menuH, 0);
	
	// add the Size menu items
	const char* items[] = {"9", "10", "12", "16", "18", "20", "32", "48", "64", NULL};
	int sizes[] = {9, 10, 12, 16, 18, 20, 32, 48, 64};
	for (int i = 0; items[i] != NULL; ++i)
	{
		err = AppendMenuItemTextWithCFString(menuH, MacString(items[i]), kMenuItemAttrIgnoreMeta, 'Size' + sizes[i], NULL);
		ThrowIfOSErr(err, "Couldn't add a size menu item!");	
	}
	
	// install a custom menu handler
	std::vector<EventTypeSpec> specs;
	specs.push_back(CreateSpec(kEventClassCommand, kEventCommandProcess));
	specs.push_back(CreateSpec(kEventClassCommand, kEventCommandUpdateStatus));
	specs.push_back(CreateSpec(kEventClassMenu, kEventMenuOpening));
	
	err = InstallApplicationEventHandler(NewEventHandlerUPP(DoMenuEvent), specs.size(), &specs[0], this, NULL);
	ThrowIfOSErr(err, "Couldn't install the menu handlers!");

	// draw the new menubar
	DrawMenuBar();	
}


//---------------------------------------------------------------
//
// CApplication::DoInitWindows
//
//---------------------------------------------------------------
void CApplication::DoInitWindows()	
{	
	// create the window
	Rect bounds = {32, 32, 64, 64};	// we position properly later
	
	WindowAttributes attrs = kWindowCollapseBoxAttribute | kWindowStandardHandlerAttribute;
	OSStatus err = CreateNewWindow(kDocumentWindowClass, attrs, &bounds, &mWindow);
	ThrowIfOSErr(err, "Couldn't create the window!");
		
	// install a custom window handler
	std::vector<EventTypeSpec> specs;
	specs.push_back(CreateSpec(kEventClassWindow, kEventWindowDrawContent));
	
	err = InstallWindowEventHandler(mWindow, NewEventHandlerUPP(DoWindowEvent), specs.size(), &specs[0], this, NULL);
	ThrowIfOSErr(err, "Couldn't install the window event handler!");
}


//---------------------------------------------------------------
//
// CApplication::DoKeyDown								[static]
//
//---------------------------------------------------------------
pascal OSStatus CApplication::DoKeyDown(EventHandlerCallRef handler, EventRef event, void* refCon)
{	
	OSStatus err = eventNotHandledErr;
	
	if (!sInDialog)
	{
		CApplication* thisPtr = static_cast<CApplication*>(refCon);

		try 
		{
			char ch;
			(void) GetEventParameter(event, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(ch), NULL, &ch);
			
			UInt32 key;
			(void) GetEventParameter(event, kEventParamKeyCode, typeUInt32, NULL, sizeof(key), NULL, &key);

			UInt32 modifiers;
			(void) GetEventParameter(event, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifiers), NULL, &modifiers);

	    	if ((modifiers & cmdKey) == 0)
				thisPtr->mChar = thisPtr->DoMungeChar(ch, key, modifiers);
				
			if (thisPtr->mChar != 0)
				QuitApplicationEventLoop();
			
			err = noErr;
		}
		catch (const std::exception& e) 
		{
			DEBUGSTR((std::string("Couldn't complete the operation (") + e.what() + ").").c_str());
			err = eventNotHandledErr;
		} 
		catch (...) 
		{
			DEBUGSTR("Couldn't complete the operation.");
			err = eventNotHandledErr;
		}
	}
		
	return err;
}


//---------------------------------------------------------------
//
// CApplication::DoMenuEvent							[static]
//
//---------------------------------------------------------------
pascal OSStatus CApplication::DoMenuEvent(EventHandlerCallRef handler, EventRef event, void* refCon)
{
	OSStatus err = noErr;
	CApplication* thisPtr = static_cast<CApplication*>(refCon);

	UInt32 kind = GetEventKind(event);
	try 
	{
		HICommand command;

		if (kind == kEventCommandUpdateStatus) 
		{
			err = GetEventParameter(event, kEventParamDirectObject, typeHICommand, NULL, sizeof(command), NULL, &command);
			ThrowIfOSErr(err, "Couldn't get the direct object in DoMenuEvent");
			
			err = thisPtr->DoEnableCommand(command.menu.menuRef, command.commandID, command.menu.menuItemIndex);
		} 
		else if (kind == kEventCommandProcess) 
		{
			err = GetEventParameter(event, kEventParamDirectObject, typeHICommand, NULL, sizeof(command), NULL, &command);
			ThrowIfOSErr(err, "Couldn't get the direct object in DoMenuEvent");
			
			err = thisPtr->DoHandleCommand(command.commandID);
		} 
		else if (kind == kEventMenuOpening) 
		{
			Boolean firstTime;
			err = GetEventParameter(event, kEventParamMenuFirstOpen, typeBoolean, NULL, sizeof(firstTime), NULL, &firstTime);
			ThrowIfOSErr(err, "Couldn't get the first open flag in DoMenuEvent");
						
			if (firstTime)		// only call the callbacks the first time we open the menu (during this drag)
			{
				MenuRef menuH;
				err = GetEventParameter(event, kEventParamDirectObject, typeMenuRef, NULL, sizeof(MenuRef), NULL, &menuH);
				ThrowIfOSErr(err, "Couldn't get the direct object in DoMenuEvent");
			
				err = thisPtr->DoOpenMenu(menuH);
			}
		} 
		else
			err = eventNotHandledErr;
	}
	catch (const std::exception& e) 
	{
		DEBUGSTR((std::string("Couldn't complete the operation (") + e.what() + ").").c_str());
		err = eventNotHandledErr;
	} 
	catch (...) 
	{
		DEBUGSTR("Couldn't complete the operation.");
		err = eventNotHandledErr;
	}
	
	if (kind == kEventCommandProcess)
		HiliteMenu(0);

	return err;
}


//---------------------------------------------------------------
//
// CApplication::DoMungeChar
//
//---------------------------------------------------------------
char CApplication::DoMungeChar(char ch, UInt32 key, UInt32 modifiers) const
{	
    switch (key) {
	    case kNumPad1Key:
	        if (modifiers & shiftKey)
	            ch = 'B';
	        else if (modifiers & controlKey)
	            ch = 2;
	        else
	            ch = 'b';
	        break;

	    case kNumPad2Key:
	    case kDownArrowKey:
	        if (modifiers & shiftKey)
	            ch = 'J';
	        else if (modifiers & controlKey)
	            ch = 10;
	        else
	            ch = 'j';
	        break;

	    case kNumPad3Key:
	        if (modifiers & shiftKey)
	            ch = 'N';
	        else if (modifiers & controlKey)
	            ch = 14;
	        else
	            ch = 'n';
	        break;

	    case kNumPad4Key:
	    case kLeftArrowKey:
	        if (modifiers & shiftKey)
	            ch = 'H';
	        else if (modifiers & controlKey)
	            ch = 8;
	        else
	            ch = 'h';
	        break;

	    case kNumPad5Key:
	        if (modifiers & shiftKey)
	            ch = '5';
	        else
	            ch = '.';
	        break;

	    case kNumPad6Key:
	    case kRightArrowKey:
	        if (modifiers & shiftKey)
	            ch = 'L';
	        else if (modifiers & controlKey)
	            ch = 12;
	        else
	            ch = 'l';
	        break;

	    case kNumPad7Key:
	        if (modifiers & shiftKey)
	            ch = 'Y';
	        else if (modifiers & controlKey)
	            ch = 25;
	        else
	            ch = 'y';
	        break;

	    case kNumPad8Key:
	    case kUpArrowKey:
	        if (modifiers & shiftKey)
	            ch = 'K';
	        else if (modifiers & controlKey)
	            ch = 11;
	        else
	            ch = 'k';
	        break;

	    case kNumPad9Key:
	        if (modifiers & shiftKey)
	            ch = 'U';
	        else if (modifiers & controlKey)
	            ch = 21;
	        else
	            ch = 'u';
	        break;
	}
	
	return ch;
}


//---------------------------------------------------------------
//
// CApplication::DoOpenMenu
//
//---------------------------------------------------------------
OSStatus CApplication::DoOpenMenu(MenuRef menuH)
{
	// select the curret font
		
	return noErr;
}


//---------------------------------------------------------------
//
// CApplication::DoQuit									[static]
//
//---------------------------------------------------------------
pascal OSErr CApplication::DoQuit(const AppleEvent* event, AppleEvent* reply, SInt32 refCon)
{
	CApplication* thisPtr = reinterpret_cast<CApplication*>(refCon);
	thisPtr->Quit();
	
	return noErr;
}


//---------------------------------------------------------------
//
// CApplication::DoReadPrefs
//
//---------------------------------------------------------------
void CApplication::DoReadPrefs()
{
	MacString appID("Crawl4");
	
	// window location
	Rect bounds;

	MacString name("window_x");	
	Boolean existsX = false;												
	bounds.left = CFPreferencesGetAppIntegerValue(name, appID, &existsX);	

	name = MacString("window_y");													
	Boolean existsY = false;												
	bounds.top = CFPreferencesGetAppIntegerValue(name, appID, &existsY);	
	
	if (existsX && existsY)
	{
		bounds.right  = bounds.left + 32;		// DoSetFont will reset the window dimensions
		bounds.bottom = bounds.top + 32;
		
		OSStatus err = SetWindowBounds(mWindow, kWindowStructureRgn, &bounds);			
		ASSERT(err == noErr);
	}
	else
	{
		OSStatus err = RepositionWindow(mWindow, NULL, kWindowCenterOnMainScreen);
		ASSERT(err == noErr);
	}

	// mFontName
	name = MacString("font_name");												

	CFTypeRef dataRef = CFPreferencesCopyAppValue(name, appID);
	if (dataRef != NULL && CFGetTypeID(dataRef) == CFStringGetTypeID()) 
	{
		MacString data(static_cast<CFStringRef>(dataRef));

		Str255 fontName;										
		data.CopyTo(fontName, sizeof(fontName));		
		CFRelease(dataRef);

		// mPointSize
		name = MacString("font_size");													
		CFIndex fontSize = CFPreferencesGetAppIntegerValue(name, appID, NULL);
		if (fontSize > 0)
		    this->DoSetFont(fontName, fontSize);
		else
		    this->DoSetFont(fontName, 12);
	}	
	else
		this->DoSetFont("\pMonaco", 12);
			
	// make sure the window isn't off-screen
	Rect area;
	OSStatus err = GetWindowGreatestAreaDevice(mWindow, kWindowStructureRgn, NULL, &area);
	if (err == noErr)
	{
		err = GetWindowBounds(mWindow, kWindowDragRgn, &bounds);
		if (err == noErr)
		{
			SectRect(&area, &bounds, &bounds);
			
			int pixels = (bounds.right - bounds.left)*(bounds.bottom - bounds.top);
			if (pixels < 64)	// only move the window if there are fewer than 64 draggable pixels
			{
				err = ConstrainWindowToScreen(mWindow, kWindowStructureRgn, kWindowConstrainStandardOptions, NULL, NULL);
				ASSERT(err == noErr);
			}
		}
	}
}
	

//---------------------------------------------------------------
//
// CApplication::DoRender
//
//---------------------------------------------------------------
void CApplication::DoRender()
{	
    ASSERT(mLines.size() == kTermHeight);

	SetPortWindowPort(mWindow);	

    TextFont(mFontNum);
    TextSize(mPointSize);

    RGBBackColor(&mBackColor);
    EraseRect(&kBigRect);

    Rect area;
    for (int y = 0; y < mLines.size(); ++y)
    {
        area.top    = y*mCellHeight;
        area.bottom = area.top + mCellHeight;
        area.left   = 0;
        area.right  = area.left + mCellWidth;

        const Line& line = mLines[y];
        ASSERT(line.size() == kTermWidth);

        for (int x = 0; x < line.size(); ++x)
        {
            const SCell& cell = line[x];

            this->DoDrawCell(area, cell);

            if (x == mCursor.h && y == mCursor.v && mShowCursor)
            {
                ::RGBForeColor(&kWhite);
                ::MoveTo(area.left + 1, area.top + mAscent);
                ::Line(area.right - area.left - 2, 0);
            }

            area.left  += mCellWidth;
            area.right += mCellWidth;
        }
    }
}


//---------------------------------------------------------------
//
// CApplication::DoSetChar
//
//---------------------------------------------------------------
void CApplication::DoSetChar(unsigned char ch)
{
    ASSERT(mCursor.h < kTermWidth);
    ASSERT(mCursor.v < kTermHeight);
    ASSERT(mCursor.h >= 0);
    ASSERT(mCursor.v >= 0);

    int x = mCursor.h;

    Line& line = mLines[mCursor.v];
    ASSERT(line.size() == kTermWidth);

    SCell& cell = line[x++];
    cell.ch = ch;
    cell.color = mForeColor;

    Rect area;
    area.top    = mCursor.v * mCellHeight;
    area.bottom = area.top + mCellHeight;
    area.left   = mCursor.h * mCellWidth;
    area.right  = area.left + mCellWidth;

	(void) InvalWindowRect(mWindow, &kBigRect);

    if (x >= kTermWidth)
    {
        if (++mCursor.v >= kTermHeight)
        {
            mCursor.v = kTermHeight - 1;
            mCursor.h = kTermWidth - 1;
            this->DoScroll();
        }
        x = 0;
    }

    mCursor.h = x;
}


//---------------------------------------------------------------
//
// CApplication::DoScroll
//
//---------------------------------------------------------------
void CApplication::DoScroll()
{
    mLines.erase(mLines.begin());

    mLines.push_back(Line());
    mLines.back().resize(kTermWidth);

	(void) InvalWindowRect(mWindow, &kBigRect);
}


//---------------------------------------------------------------
//
// CApplication::DoSetFont
//
//---------------------------------------------------------------
void CApplication::DoSetFont(const unsigned char* name, int size)
{
    ASSERT(name != NULL);
    ASSERT(size > 0);

    short fontNum;
    GetFNum(name, &fontNum);

    if (fontNum != mFontNum || size != mPointSize)
    {
        BlockMoveData(name, mFontName, name[0] + 1);

        mFontNum = fontNum;
        mPointSize = size;

		SetPortWindowPort(mWindow);	
        TextFont(mFontNum);
        TextSize(mPointSize);

        FontInfo info;
        GetFontInfo(&info);

        mCellHeight = info.ascent + info.descent;
        mAscent = info.ascent;

        short width = StringWidth("\pMMMMM");   // widMax is usually much too wide so we'll compute this ourselves...
        mCellWidth = width/5 + 1;

        Rect bounds;
        bounds.top    = 0;
        bounds.left   = 0;
        bounds.bottom = mCellHeight * kTermHeight;
        bounds.right  = mCellWidth * kTermWidth;
        SizeWindow(mWindow, bounds.right, bounds.bottom, false);

		(void) InvalWindowRect(mWindow, &kBigRect);
    }
}


//---------------------------------------------------------------
//
// CApplication::DoWindowEvent							[static]
//
//---------------------------------------------------------------
pascal OSStatus CApplication::DoWindowEvent(EventHandlerCallRef handler, EventRef event, void* refCon)
{	
	OSStatus err = noErr;
	CApplication* thisPtr = static_cast<CApplication*>(refCon);

	try 
	{
		WindowRef window = NULL;
		(void) GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(window), NULL, &window);
		ASSERT(window == thisPtr->mWindow);
				
		UInt32 kind = GetEventKind(event);
		switch (kind) {
//			case kEventWindowBoundsChanging:			
//				thisPtr->DoConstrainWindow(event);
//				break;
				
			case kEventWindowDrawContent:	
				thisPtr->DoRender();
				break;
				
//			case kEventWindowGetIdealSize:
//			case kEventWindowGetMaximumSize:
//				{
//				::Point maxSize = thisPtr->OnGetMaxSize();
//				SetEventParameter(event, kEventParamDimensions, &maxSize);
//				}
//				break;

//			case kEventWindowGetMinimumSize:
//				{
//				::Point minSize = thisPtr->OnGetMinSize();
//				SetEventParameter(event, kEventParamDimensions, &minSize);
//				}
//				break;

			default:
				err = eventNotHandledErr;
		}
	}
	catch (const std::exception& e) 
	{
		DEBUGSTR((std::string("Couldn't complete the operation (") + e.what() + ").").c_str());
		err = eventNotHandledErr;
	} 
	catch (...) 
	{
		DEBUGSTR("Couldn't complete the operation.");
		err = eventNotHandledErr;
	}
	
	return err;
}


//---------------------------------------------------------------
//
// CApplication::DoWritePrefs	, , mWindow->location
//
//---------------------------------------------------------------
void CApplication::DoWritePrefs()	
{	
	MacString appID("Crawl4");
	
	// mFontName
	MacString name("font_name");												
	MacString data(mFontName);												
	CFPreferencesSetAppValue(name, data, appID);			
	
	// mPointSize
	name = MacString("font_size");												
	data = MacString(mPointSize);												
	CFPreferencesSetAppValue(name, data, appID);
	
	// window location
	Rect bounds;
	OSStatus err = GetWindowBounds(mWindow, kWindowStructureRgn, &bounds);			
	if (err == noErr)
	{
		name = MacString("window_x");												
		data = MacString(bounds.left);												
		CFPreferencesSetAppValue(name, data, appID);

		name = MacString("window_y");												
		data = MacString(bounds.top);												
		CFPreferencesSetAppValue(name, data, appID);
	}
	
	// flush
	VERIFY(CFPreferencesAppSynchronize(appID));			
}

#if __MWERKS__
#pragma mark -
#endif

// ========================================================================
//	Non-ANSI Functions
// ========================================================================

//---------------------------------------------------------------
//
// stricmp
//
// Case insensitive string comparison (code is from MSL which
// is why it looks so dorky).
//
//---------------------------------------------------------------
int stricmp(const char* lhs, const char* rhs)
{
    ASSERT(lhs != NULL);
    ASSERT(rhs != NULL);

    const unsigned char* p1 = (unsigned char*) lhs - 1;
    const unsigned char* p2 = (unsigned char*) rhs - 1;
    unsigned long c1, c2;

    while ((c1 = tolower(*++p1)) == (c2 = tolower(*++p2)))
        if (c1 == '\0')
            return (0);

    return c1 - c2;
}


//---------------------------------------------------------------
//
// strlwr
//
// In place conversion to lower case.
//
//---------------------------------------------------------------
char* strlwr(char* str)
{
    ASSERT(str != NULL);

    for (int i = 0; i < strlen(str); ++i)
        str[i] = tolower(str[i]);

    return str;
}


//---------------------------------------------------------------
//
// itoa
//
// Converts an integer to a string (after liblinux.cc).
//
//---------------------------------------------------------------
void itoa(int value, char* buffer, int radix)
{
    ASSERT(buffer != NULL);
    ASSERT(radix == 10 || radix == 2);

    if (radix == 10)
        sprintf(buffer, "%i", value);

    if (radix == 2)
    {                           /* int to "binary string" */
        unsigned int bitmask = 32768;
        int ctr = 0;
        int startflag = 0;

        while (bitmask)
        {
            if (value & bitmask)
            {
                startflag = 1;
                sprintf(buffer + ctr, "1");

            }
            else
            {
                if (startflag)
                    sprintf(buffer + ctr, "0");
            }

            bitmask = bitmask >> 1;
            if (startflag)
                ctr++;
        }

        if (!startflag)         /* Special case if value == 0 */
            sprintf((buffer + ctr++), "0");
        buffer[ctr] = (char) NULL;
    }
}

#if __MWERKS__
#pragma mark -
#endif

// ========================================================================
//	Curses(?) Functions
// ========================================================================

//---------------------------------------------------------------
//
// window
//
//---------------------------------------------------------------
void window(int x, int y, int lx, int ly)
{
    ASSERT(lx == kTermWidth);   // window size is hard-coded
    ASSERT(ly == kTermHeight);

    gotoxy(x, y);
}


//---------------------------------------------------------------
//
// clrscr
//
//---------------------------------------------------------------
void clrscr()
{
    ASSERT(sApp != NULL);

    sApp->Clear();
}


//---------------------------------------------------------------
//
// textcolor
//
//---------------------------------------------------------------
void textcolor(int c)
{
    ASSERT(c >= 0);
    ASSERT(c < 16);
    ASSERT(sApp != NULL);

    RGBColor color = ConvertColor(c);
    sApp->SetForeColor(color);
}


//---------------------------------------------------------------
//
// textbackground
//
//---------------------------------------------------------------
void textbackground(int c)
{
    ASSERT(c >= 0);
    ASSERT(c < 16);
    ASSERT(sApp != NULL);

    RGBColor color = ConvertColor(c);
    sApp->SetBackColor(color);
}


//---------------------------------------------------------------
//
// gotoxy
//
//---------------------------------------------------------------
void gotoxy(int x, int y)
{
    ASSERT(x >= 1);
    ASSERT(y >= 1);
    ASSERT(sApp != NULL);

    sApp->SetCursor(x - 1, y - 1);
}


//---------------------------------------------------------------
//
// wherex
//
//---------------------------------------------------------------
int wherex()
{
    ASSERT(sApp != NULL);

    Point pos = sApp->GetCursor();

    return pos.h + 1;
}


//---------------------------------------------------------------
//
// wherey
//
//---------------------------------------------------------------
int wherey()
{
    ASSERT(sApp != NULL);

    Point pos = sApp->GetCursor();

    return pos.v + 1;
}


//---------------------------------------------------------------
//
// putch
//
//---------------------------------------------------------------
void putch(char ch)
{
    ASSERT(sApp != NULL);

    char buffer[2];

    buffer[0] = ch;
    buffer[1] = '\0';

    sApp->SetChar(ch);
//  sApp->Print(buffer);
}


//---------------------------------------------------------------
//
// cprintf
//
//---------------------------------------------------------------
void cprintf(const char* format,...)
{
    ASSERT(sApp != NULL);

    char buffer[2048];

    va_list argp;

    va_start(argp, format);
    vsprintf(buffer, format, argp);
    va_end(argp);

    sApp->Print(buffer);
}


//---------------------------------------------------------------
//
// kbhit
//
//---------------------------------------------------------------
int kbhit()
{
    return sApp->PeekChar();
}


//---------------------------------------------------------------
//
// getche
//
//---------------------------------------------------------------
char getche()
{
    char ch = getch();

    if (ch != '\r')
        putch(ch);

    return ch;
}


//---------------------------------------------------------------
//
// getstr
//
//---------------------------------------------------------------
void getstr(char* buffer, int bufferSize)
{
    ASSERT(buffer != NULL);
    ASSERT(bufferSize > 1);

    int index = 0;

    while (index < bufferSize - 1)
    {
        char ch = getche();

        if (ch == '\r')
            break;
        else if (ch == '\b' && index > 0)
            --index;
        else if (isprint(ch))
            buffer[index++] = ch;
    }

    buffer[index] = '\0';
}


//---------------------------------------------------------------
//
// _setcursortype
//
//---------------------------------------------------------------
void _setcursortype(int curstype)
{
    ASSERT(curstype == _NORMALCURSOR || curstype == _NOCURSOR);
    ASSERT(sApp != NULL);

    sApp->ShowCursor(curstype == _NORMALCURSOR);
}


//---------------------------------------------------------------
//
// getch
//
//---------------------------------------------------------------
int getch()
{
    ASSERT(sApp != NULL);

    return sApp->GetChar();
}

#if __MWERKS__
#pragma mark -
#endif

// ========================================================================
//	Misc Functions
// ========================================================================

//---------------------------------------------------------------
//
// delay
//
//---------------------------------------------------------------
void delay(int ms)
{
    ASSERT(ms >= 0);

	usleep(1000*ms);
}


//---------------------------------------------------------------
//
// init_mac
//
//---------------------------------------------------------------
void init_mac()
{
    ASSERT(sApp == NULL);

	// Read in the color table
	sColors = GetCTable(256);
	if (sColors == NULL)
	{
		WarnUser("Couldn't load the colour table!");
		ExitToShell();
	}

	// Create the application object
	sApp = new CApplication;
}


//---------------------------------------------------------------
//
// deinit_mac
//
//---------------------------------------------------------------
void deinit_mac()
{
    delete sApp;
    sApp = NULL;
}


#endif // macintosh
