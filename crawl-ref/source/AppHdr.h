/**
 * @file
 * @brief Precompiled header used by Crawl.
 *
 * CodeWarrior and MSVC both support precompiled headers which can
 * significantly speed up compiles. Unlike CodeWarrior MSVC imposes
 * some annoying restrictions on precompiled headers: the precompiled
 * header *must* be the first include in all cc files. Any includes or
 * other statements that occur before the pch include are ignored. This
 * is really stupid and can lead to bizarre errors, but it does mean
 * that we shouldn't run into any problems on systems without precompiled
 * headers.
**/


#ifndef APPHDR_H
#define APPHDR_H

/* Fix annoying precompiled header compile errors caused by unused Objective-C variant. */
#if !defined(__OBJC__)

#include "platform.h"
#include <stdint.h>
namespace std {};
using namespace std;

#if !defined(TARGET_COMPILER_VC) && defined(__cplusplus) && __cplusplus < 201103
# define unique_ptr auto_ptr
template<typename T>
static inline T move(T x) { return x; } // good enough for our purposes
# include <cstddef>
# define nullptr NULL
#endif

#ifdef TARGET_COMPILER_VC
/* Disable warning about:
   4290: the way VC handles the throw() specifier
   4267: "possible loss of data" when switching data types without a cast
 */
#pragma warning (disable: 4290 4267)
/* Don't define min and max as macros, define them via STL */
#define NOMINMAX
#define ENUM_INT64 : unsigned long long
#else
#define ENUM_INT64
#endif

#ifdef __sun
// Solaris libc has ambiguous overloads for float, double, long float, so
// we need to upgrade ints explicitly:
#include <math.h>
static inline double sqrt(int x) { return sqrt((double)x); }
static inline double atan2(int x, int y) { return atan2((double)x, (double)y); }
static inline double pow(int x, int y) { return std::pow((double)x, y); }
static inline double pow(int x, double y) { return std::pow((double)x, y); }
#endif

// The maximum memory that the user-script Lua interpreter can
// allocate, in kilobytes. This limit is enforced to prevent
// badly-written or malicious user scripts from consuming too much
// memory.
//
#define CLUA_MAX_MEMORY_USE (2 * 1024)

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
// MinGW
//
#if defined(TARGET_COMPILER_MINGW)
    #ifndef REGEX_PCRE
    #define REGEX_PCRE
    #endif
#endif

#if !defined(__cplusplus) || (__cplusplus < 201103)
# define constexpr const
#endif

// =========================================================================
//  System Defines
// =========================================================================

#ifdef UNIX
    // Uncomment if you're running Crawl with dgamelaunch and have
    // problems viewing games in progress. This affects how Crawl
    // clears the screen (see DGL_CLEAR_SCREEN) below.
    //
    // #define DGAMELAUNCH

    #define USE_UNIX_SIGNALS

    #define FILE_SEPARATOR '/'
#ifndef USE_TILE_LOCAL
    #define USE_CURSES
#endif

    // More sophisticated character handling
    #define CURSES_USE_KEYPAD

    // How long (milliseconds) curses should wait for additional characters
    // after seeing an Escape (0x1b) keypress. May not be available on all
    // curses implementations.
    #define CURSES_SET_ESCDELAY 20

    // Have the utimes function to set access and modification time on
    // a file.
    #define HAVE_UTIMES

    // Use POSIX regular expressions
#ifndef REGEX_PCRE
    #define REGEX_POSIX
#endif

    // If you have libpcre, you can use that instead of POSIX regexes -
    // uncomment the line below and add -lpcre to your makefile.
    // #define REGEX_PCRE

    // Uncomment (and edit as appropriate) to play sounds.
    //
    // WARNING: Filenames passed to this command *are not validated in any way*.
    //
    // #define SOUND_PLAY_COMMAND "/usr/bin/play -v .5 \"%s\" 2>/dev/null &"

    #include "libunix.h"

#elif defined(TARGET_OS_WINDOWS)
    #if defined(TARGET_COMPILER_MINGW)
        #define USE_UNIX_SIGNALS
    #endif

    #ifdef USE_TILE_WEB
        #error Webtiles are not supported on Windows.
    #endif
    #ifndef USE_TILE_LOCAL
        #include "libw32c.h"
    #endif

    // NT and better are happy with /; I'm not sure how 9x reacts.
    #define FILE_SEPARATOR '/'
    #define ALT_FILE_SEPARATOR '\\'

    // Uncomment to play sounds. winmm must be linked in if this is uncommented.
    // #define WINMM_PLAY_SOUNDS

    // Use Perl-compatible regular expressions. libpcre must be available and
    // linked in.  Required in the absence of POSIX regexes.
    #ifndef REGEX_PCRE
    #define REGEX_PCRE
    #endif
#else
    #error Missing platform #define or unsupported compiler.
#endif

#ifndef _WIN32_WINNT
// Allow using Win2000 syscalls.
# define _WIN32_WINNT 0x501
#endif

// See the GCC __attribute__ documentation for what these mean.
// Note: clang does masquerade as GNUC.

#if defined(__GNUC__)
# define NORETURN __attribute__ ((noreturn))
#elif defined(_MSC_VER)
# define NORETURN __declspec(noreturn)
#else
# define NORETURN
#endif

#if defined(__GNUC__)
# define PURE __attribute__ ((pure))
# define IMMUTABLE __attribute__ ((const))
#else
# define PURE
# define IMMUTABLE
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

    #define DEBUG_BONES
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

// clamp time between command inputs at 30 seconds when reporting play time.
// Anything longer means you do something other than playing -- heck, even 30s
// is too long as when thinking in a tight spot people re-read the inventory,
// check monsters, etc.
#define IDLE_TIME_CLAMP  30

// Number of top scores to keep. See above for the dgamelaunch setting.
#ifndef SCORE_FILE_ENTRIES
#define SCORE_FILE_ENTRIES      100
#endif

// Option to allow scoring of wizard characters.  Note that even if
// you define this option, wizard characters are still tagged as such
// in the score file.
// #define SCORE_WIZARD_CHARACTERS

// Option to allow all characters to enter wizmode on turn zero, regardless
// of WIZ_NEVER status, or dgl permissioning.
// #define TURN_ZERO_WIZARD

#define SAVE_SUFFIX ".cs"

// If you are installing Crawl for multiple users, define SAVE_DIR
// to the directory where saves, bones, and score file will go...
// end it with a '/'. Only one system user should be able to access
// these -- usually this means you should place them in ~/.crawl/
// unless it's a DGL build.

#if !defined(DB_NDBM) && !defined(DB_DBH) && !defined(USE_SQLITE_DBM)
#define USE_SQLITE_DBM
#endif

// Uncomment these if you can't find these functions on your system
// #define NEED_USLEEP

#ifdef USE_TILE_LOCAL
# ifndef PROPORTIONAL_FONT
#  error PROPORTIONAL_FONT not defined
# endif
# ifndef MONOSPACED_FONT
#  error MONOSPACED_FONT not defined
# endif
#endif

#ifdef __cplusplus

template < class T >
static inline void UNUSED(const volatile T &)
{
}

#endif // __cplusplus


#ifdef __GNUC__
// show warnings about the format string
# define PRINTF(x, dfmt) const char *format dfmt, ...) \
                   __attribute__((format (printf, x+1, x+2))
#else
# define PRINTF(x, dfmt) const char *format dfmt, ...
#endif


// And now headers we want precompiled
#ifdef TARGET_COMPILER_VC
# include "msvc.h"
#endif

#include "debug.h"
#include "externs.h"

#ifdef TARGET_COMPILER_VC
# include "libw32c.h"
#endif

#ifdef __cplusplus
# ifdef USE_TILE
#  include "libgui.h"
# endif
# include "tiles.h"
#endif

#endif // !defined __OBJC__

#endif // APPHDR_H
