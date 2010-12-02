/*
 *  File:       AppHdr.h
 *  Summary:    Precompiled header used by Crawl.
 *  Written by: Jesse Jones
 *
 * Abstract: CodeWarrior and MSVC both support precompiled headers which can
 *      significantly speed up compiles. Unlike CodeWarrior MSVC imposes
 *      some annoying restrictions on precompiled headers: the precompiled
 *      header *must* be the first include in all cc files. Any includes or
 *      other statements that occur before the pch include are ignored. This
 *      is really stupid and can lead to bizarre errors, but it does mean
 *      that we shouldn't run into any problems on systems without precompiled
 *      headers.
 *
 *  Copyright © 1999 Jesse Jones.
 */


#ifndef APPHDR_H
#define APPHDR_H

#include "platform.h"

#ifdef TARGET_COMPILER_VC
/* Disable warning about:
   4290: the way VC handles the throw() specifier
   4267: "possible loss of data" when switching data types without a cast
 */
#pragma warning (disable: 4290 4267)
/* Don't define min and max as macros, define them via STL */
#define NOMINMAX
#endif

// The maximum memory that the user-script Lua interpreter can
// allocate, in kilobytes. This limit is enforced to prevent
// badly-written or malicious user scripts from consuming too much
// memory.
//
#define CLUA_MAX_MEMORY_USE (2 * 1024)

// Enable support for Unicode character glyphs. Note that this needs
// to be accompanied by changes to linker and compiler options and may
// not be available on all platforms. In most cases you want to set
// this option from your makefile, not directly in AppHdr.h (See
// INSTALL for more details.)
//
// #define UNICODE_GLYPHS

// Uncomment to prevent Crawl from looking for a list of saves when
// asking the player to enter a name. This can speed up startup
// considerably if you have a lot of saves lying around.
//
// #define DISABLE_SAVEGAME_LISTS

// Uncomment to prevent Crawl from remembering startup preferences.
//
// #define DISABLE_STICKY_STARTUP_OPTIONS

// Uncomment to let valgrind debug unitialized uses of global classes
// (you, env, clua, dlua, crawl_state).
//
// #define DEBUG_GLOBALS

//
// Define 'UNIX' if the target OS is UNIX-like.
// Unknown OSes are assumed to be here.
//
#if defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX) || \
    defined(TARGET_OS_FREEBSD) || defined(TARGET_OS_NETBSD) || \
    defined(TARGET_OS_OPENBSD) || defined(TARGET_COMPILER_CYGWIN) || \
    defined(TARGET_OS_SOLARIS) || defined(TARGET_OS_UNKNOWN)
    #ifndef UNIX
    #define UNIX
    #endif
#endif

//
// OS X's Terminal.app has color handling problems; dark grey is
// especially bad, so we'll want to remap that. OS X is otherwise
// Unix-ish, so we shouldn't need other special handling.
//
#define COL_TO_REPLACE_DARKGREY     BLUE

//
// MinGW
//
#if defined(TARGET_COMPILER_MINGW)
    #ifndef REGEX_PCRE
    #define REGEX_PCRE
    #endif
#endif

// =========================================================================
//  System Defines
// =========================================================================
// Define plain_term for Unix and dos_term for DOS.

#ifdef UNIX
    // Uncomment if you're running Crawl with dgamelaunch and have
    // problems viewing games in progress. This affects how Crawl
    // clears the screen (see DGL_CLEAR_SCREEN) below.
    //
    // #define DGAMELAUNCH

#ifndef TARGET_COMPILER_MINGW
    #define MULTIUSER
    #define USE_UNIX_SIGNALS
#endif

    // If this is defined, Crawl will attempt to save and exit when it
    // receives a hangup signal.
    #define SIGHUP_SAVE

    #define FILE_SEPARATOR '/'

    #define CHARACTER_SET           0
#ifndef USE_TILE
    // NOTE: Tiles relies on the IBM character set for evaluating glyphs
    //       of magic mapped dungeon cells.
    #define USE_ASCII_CHARACTERS
    #define USE_CURSES
#endif

    // Unix builds use curses/ncurses, which supports colour.
    //
    // This will allow using the standout attribute in curses to
    // mark friendly monsters... results depend on the type of
    // term used... under X Windows try "rxvt".
    #define USE_COLOUR_OPTS

    // More sophisticated character handling
    #define CURSES_USE_KEYPAD

    // How long (milliseconds) curses should wait for additional characters
    // after seeing an Escape (0x1b) keypress. May not be available on all
    // curses implementations.
    #define CURSES_SET_ESCDELAY 20

    // Have the utimes function to set access and modification time on
    // a file.
    #define HAVE_UTIMES

    // Use this to seed the PRNG with a bit more than just time()... turning
    // this off is perfectly okay, the game just becomes more exploitable
    // with a bit of hacking (ie only by people who know how).
    //
    // For now, we'll make it default to on for Linux (who should have
    // no problems with compiling this).
#ifndef TARGET_COMPILER_MINGW
    #define USE_MORE_SECURE_SEED
#endif

    // Use POSIX regular expressions
#ifndef REGEX_PCRE
    #define REGEX_POSIX
#endif

    // If you have libpcre, you can use that instead of POSIX regexes -
    // uncomment the line below and add -lpcre to your makefile.
    // #define REGEX_PCRE

    // Uncomment (and edit as appropriate) to play sounds.
    //
    // WARNING: Enabling sounds may compromise security if Crawl is installed
    //          setuid or setgid. Filenames passed to this command *are not
    //          validated in any way*.
    //
    // #define SOUND_PLAY_COMMAND "/usr/bin/play -v .5 %s 2>/dev/null &"

    // For cases when the game will be played on terms that don't support the
    // curses "bold == lighter" 16 colour mode. -- bwr
    //
    // Darkgrey is a particular problem in the 8 colour mode.  Popular values
    // for replacing it around here are: WHITE, BLUE, and MAGENTA.  This
    // option has no affect in 16 colour mode. -- bwr
    //
    // #define USE_8_COLOUR_TERM_MAP
    // #define COL_TO_REPLACE_DARKGREY     MAGENTA

    #include "libunix.h"

#elif defined(TARGET_OS_DOS)
    #define SHORT_FILE_NAMES
    #define CHARACTER_SET           A_ALTCHARSET

    #define FILE_SEPARATOR '\\'

    #include <string>
    #include "libdos.h"

    #include <dos.h>
    #include <file.h>

    #define round(x) floor((x)+0.5)

    // Use Perl-compatible regular expressions. libpcre must be available and
    // linked in.  This is optional.
    #ifndef REGEX_PCRE
    #define REGEX_PCRE
    #endif

#elif defined(TARGET_OS_WINDOWS)
    #if !defined(USE_TILE)
        #include "libw32c.h"
    #endif
    #define CHARACTER_SET           A_ALTCHARSET
    #define getstr(X,Y)         get_console_string(X,Y)

    // NT and better are happy with /; I'm not sure how 9x reacts.
    #define FILE_SEPARATOR '/'
    #define ALT_FILE_SEPARATOR '\\'

    // Uncomment to play sounds. winmm must be linked in if this is uncommented.
    // #define WINMM_PLAY_SOUNDS

    // Use Perl-compatible regular expressions. libpcre must be available and
    // linked in.  This is optional.
    #ifndef REGEX_PCRE
    #define REGEX_PCRE
    #endif

    #define NEED_USLEEP
#else
    #error Missing platform #define or unsupported compiler.
#endif

// =========================================================================
//  Defines for dgamelaunch-specific things.
// =========================================================================

#ifdef DGAMELAUNCH
    // DGL_CLEAR_SCREEN specifies the escape sequence to use to clear
    // the screen (used only when DGAMELAUNCH is defined). We make no
    // attempt to discover an appropriate escape sequence for the
    // term, assuming that dgamelaunch admins can adjust this as
    // needed.
    //
    // Why this is necessary: dgamelaunch's ttyplay initialises
    // playback by jumping to the last screen clear and playing back
    // from there. For that to work, ttyplay must be able to recognise
    // the clear screen sequence, and ncurses clear()+refresh()
    // doesn't do the trick.
    //
    #define DGL_CLEAR_SCREEN "\033[2J"

    // Create .des and database cache files in a directory named with the
    // game version so that multiple save-compatible Crawl versions can
    // share the same savedir.
    #define DGL_VERSIONED_CACHE_DIR

    // Update (text) database files safely; when a Crawl process
    // starts up and notices that a db file is out-of-date, it updates
    // it in-place, instead of torching the old file.
    #define DGL_REWRITE_PROTECT_DB_FILES

    #ifndef USE_MORE_SECURE_SEED
    #error DGAMELAUNCH builds should define USE_MORE_SECURE_SEED
    #endif

    // This secures the PRNG itself by hashing the values with SHA256.
    // It doesn't have much point if USE_MORE_SECURE_SEED is not used.
    // PRNG will be about 15 times slower when this is turned on, but
    // even with that the cpu time used by the PRNG is relatively small.
    #define MORE_HARDENED_PRNG

    // Startup preferences are saved by player name rather than uid,
    // since all players use the same uid in dgamelaunch.
    #ifndef DGL_NO_STARTUP_PREFS_BY_NAME
    #define DGL_STARTUP_PREFS_BY_NAME
    #endif

    // Increase the size of the topscores file for public servers.
    #ifndef SCORE_FILE_ENTRIES
    #define SCORE_FILE_ENTRIES 1000
    #endif

    // If defined, the hiscores code dumps preformatted verbose and terse
    // death message strings in the logfile for the convenience of logfile
    // parsers.
    #define DGL_EXTENDED_LOGFILES

    // Basic messaging for dgamelaunch, based on SIMPLE_MAIL for
    // NetHack and Slash'EM. I'm calling this "messaging" because that's
    // closer to reality.
    #define DGL_SIMPLE_MESSAGING

    // How often we check for messages. This is not once per turn, but once
    // per player-input. Message checks are not performed if the keyboard
    // buffer is full, so messages should not interrupt macros.
    #define DGL_MESSAGE_CHECK_INTERVAL 1

    // Record game milestones in an xlogfile.
    #define DGL_MILESTONES

    // Save a timestamp every 100 turns so that external tools can seek in
    // game recordings more easily.
    #define DGL_TURN_TIMESTAMPS

    // Record where players are currently.
    #define DGL_WHEREIS

    // Uses <playername>-macro.txt as the macro file if uncommented.
    // #define DGL_NAMED_MACRO_FILE

    // Uses Options.macro_dir as the full path to the macro file. Mutually
    // exclusive with DGL_NAMED_MACRO_FILE.
    #define DGL_MACRO_ABSOLUTE_PATH

    // Makes the game ask the user to hit Enter after bailing out with
    // an error message.
    #define DGL_PAUSE_AFTER_ERROR

    // Enable core dumps. Note that this will not create core dumps if
    // Crawl is installed setuid or setgid, but dgamelaunch installs should
    // not be installing Crawl set[ug]id anyway.
    #define DGL_ENABLE_CORE_DUMP

    // Use UTC for dgamelaunch servers.
    #define TIME_FN gmtime
#endif

#ifndef TIME_FN
#define TIME_FN localtime
#endif

#if defined(REGEX_POSIX) && defined(REGEX_PCRE)
#error You can use either REGEX_POSIX or REGEX_PCRE, or neither, but not both.
#endif

// =========================================================================
//  Debugging Defines
// =========================================================================
#ifdef FULLDEBUG
    // Bounds checking and asserts
    #ifndef DEBUG
    #define DEBUG
    #endif

    // Outputs many "hidden" details, defaults to wizard on.
    #define DEBUG_DIAGNOSTICS

    // Scan for bad items before every input (may be slow)
    //
    // This function might slow things down quite a bit
    // on slow machines because it's going to go through
    // every item on the level and do string comparisons
    // against the name.  Still, it is nice to know the
    // turn in which "bad" items appear.
    #define DEBUG_ITEM_SCAN
    #define DEBUG_MONS_SCAN
#endif

#ifdef _DEBUG       // this is how MSVC signals a debug build
    #ifndef DEBUG
    #define DEBUG
    #endif
#endif

#ifdef DEBUG
    #ifdef __MWERKS__
        #define MSIPL_DEBUG_MODE
    #endif
#else
    #if !defined(NDEBUG)
        #define NDEBUG                  // used by <assert.h>
    #endif
#endif


#ifdef USE_TILE
    #ifdef __cplusplus
    #include "libgui.h"
    #include "tilesdl.h"
    #endif
#endif

// =========================================================================
//  Lua user scripts (NOTE: this may also be enabled in your makefile!)
// =========================================================================
//
// Enables Crawl's Lua bindings for user scripts. See the section on
// Lua in INSTALL for more information. NOTE: CLUA_BINDINGS is enabled
// by default in all standard makefiles. Commenting it out here may
// not be sufficient to disable user-Lua - you must also check your
// makefile and remove -DCLUA_BINDINGS. You can use V in-game to check
// whether user-scripts are enabled.
//
// #define CLUA_BINDINGS

// =========================================================================
//  Game Play Defines
// =========================================================================
// number of older messages stored during play and in save files
#define NUM_STORED_MESSAGES   1000

// clamp time between command inputs at 5 minutes when reporting play time.
#define IDLE_TIME_CLAMP  (5 * 60)

// Number of top scores to keep. See above for the dgamelaunch setting.
#ifndef SCORE_FILE_ENTRIES
#define SCORE_FILE_ENTRIES      100
#endif

// Option to allow scoring of wizard characters.  Note that even if
// you define this option, wizard characters are still tagged as such
// in the score file.
// #define SCORE_WIZARD_CHARACTERS

// ================================================= --------------------------
//jmf: New defines for a bunch of optional features.
// ================================================= --------------------------

// Use special colours for various channels of messages
#define USE_COLOUR_MESSAGES

// Wizard death option (needed to test new death messages)
#define USE_OPTIONAL_WIZARD_DEATH

// bwr: define this if you want to know the pluses, "runed" status
// of the monster's weapons in the hiscore file.
#define HISCORE_WEAPON_DETAIL

// ====================== -----------------------------------------------------
//jmf: end of new defines
// ====================== -----------------------------------------------------

#define SAVE_SUFFIX ".cs"

#ifdef MULTIUSER
    // If you are installing Crawl for multiple users, define SAVE_DIR
    // to the directory where saves, bones, and score file will go...
    // end it with a '/'. Since all player files will be in the same
    // directory, the players UID will be appended when this option is
    // set.
    //
    // If you want to build Crawl that only one user will use, you do not need
    // to set SAVE_DIR_PATH (and do not need to run make install).
    //
    // Setting it to nothing or not setting it will cause all game files to
    // be dumped in the current directory.
    //
    // #define SAVE_DIR_PATH       "/opt/crawl/lib/"
    // #define SAVE_DIR_PATH       ""

    // This defines the chmod permissions for score and bones files.
    #define SHARED_FILES_CHMOD_PRIVATE  0664
    #define SHARED_FILES_CHMOD_PUBLIC   0664

    // If we're on a multiuser system, file locking of shared files is
    // very important (else things will just keep getting corrupted)
    #define USE_FILE_LOCKING

    // Define this if you'd rather have the game block on locked files,
    // commenting it will poll the file lock once a second for thirty
    // seconds before giving up.
    #define USE_BLOCKING_LOCK

    // some files needed for file locking
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/stat.h>

#endif /* MULTIUSER */

#if defined(DGL_SIMPLE_MESSAGING) && !defined(USE_FILE_LOCKING)
#error Must define USE_FILE_LOCKING for DGL_SIMPLE_MESSAGING
#endif

#if !defined(DB_NDBM) && !defined(DB_DBH) && !defined(USE_SQLITE_DBM)
#define USE_SQLITE_DBM
#endif

// Uncomment these if you can't find these functions on your system
// #define NEED_USLEEP

#ifndef PROPORTIONAL_FONT
    #define PROPORTIONAL_FONT "Vera.ttf"
#endif
#ifndef MONOSPACED_FONT
    #define MONOSPACED_FONT "VeraMono.ttf"
#endif

#ifdef __cplusplus


template < class T >
inline void UNUSED(const volatile T &)
{
}

// And now headers we want precompiled
#ifdef TARGET_COMPILER_VC
#include "msvc.h"
#endif

#include "externs.h"
#include "unwind.h"
#include "version.h"

#ifdef TARGET_COMPILER_VC
#include "libw32c.h"
#endif

#endif // __cplusplus

#endif // APPHDR_H
