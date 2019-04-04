/**
 * @file
 * @brief Version (and revision) functionality.
**/

#include "AppHdr.h"

#include "version.h"

#include "build.h"
#include "compflag.h"
#include "notes.h"
#include "player.h"
#include "state.h"
#include "stringutil.h"

namespace Version
{
    const char* Major          = CRAWL_VERSION_MAJOR;
    const char* Short          = CRAWL_VERSION_SHORT;
    const char* Long           = CRAWL_VERSION_LONG;
    const rel_type ReleaseType = CRAWL_VERSION_RELEASE;
}

#if defined(__GNUC__) && defined(__VERSION__)
 #define COMPILER "GCC " __VERSION__
#elif defined(__GNUC__)
 #define COMPILER "GCC (unknown version)"
#elif defined(TARGET_COMPILER_MINGW)
 #define COMPILER "MINGW"
#elif defined(TARGET_COMPILER_CYGWIN)
 #define COMPILER "CYGWIN"
#elif defined(TARGET_COMPILER_VC)
 #define COMPILER "Visual C++"
#elif defined(TARGET_COMPILER_ICC)
 #define COMPILER "Intel C++"
#else
 #define COMPILER "Unknown compiler"
#endif

const char* compilation_info =
    "Compiled with " COMPILER "\n"
    "Build platform: " CRAWL_HOST "\n"
    "Platform: " CRAWL_ARCH "\n"
    "CFLAGS: " CRAWL_CFLAGS "\n"
    "LDFLAGS: " CRAWL_LDFLAGS "\n";

#define VERSION_HISTORY_PROP "version_history"

void Version::record(string prev)
{
    // TODO: do away with you.prev_save_version?
    if (!you.props.exists(VERSION_HISTORY_PROP))
    {
        string v;
        you.props[VERSION_HISTORY_PROP].new_vector(SV_STR);
        if (prev.size())
        {
            if (prev == Long)
                v = "Missing version history before: ";
            else
            {
                you.props[VERSION_HISTORY_PROP].get_vector().push_back(
                    CrawlStoreValue(make_stringf(
                            "Missing version history before: %s",
                            prev.c_str())));
            }
        }
        else
            v = "Game started: ";
        v += Long;
        you.props[VERSION_HISTORY_PROP].get_vector().push_back(
                                                        CrawlStoreValue(v));
    }
    else if (prev != Long)
    {
        you.props[VERSION_HISTORY_PROP].get_vector().push_back(
                                                    CrawlStoreValue(Long));
        const string note = make_stringf("Upgraded the game from %s to %s",
                                         prev.c_str(), Long);
        take_note(Note(NOTE_MESSAGE, 0, 0, note));
    }
}

size_t Version::history_size()
{
    if (!crawl_state.need_save || !you.props.exists(VERSION_HISTORY_PROP))
        return 0;
    else
        return you.props[VERSION_HISTORY_PROP].get_vector().size();
}

string Version::history()
{
    if (history_size() == 0)
        return make_stringf("No version history (current version is %s)", Long);

    string result;
    for (auto v : you.props[VERSION_HISTORY_PROP].get_vector())
    {
        result += v.get_string();
        result += "\n";
    }
    if (result.size())
        result.pop_back();
    return result;
}
