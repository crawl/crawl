#include "AppHdr.h"

#include "ng-input.h"

#include <cwctype>

#include "end.h"
#include "format.h"
#include "item-name.h" // make_name
#include "libutil.h"
#include "options.h"
#include "stringutil.h"
#include "unicode.h"
#include "version.h"

// Eventually, this should be something more grand. {dlb}
formatted_string opening_screen()
{
    string msg =
    "<yellow>Hello, welcome to " CRAWL " " + string(Version::Long) + "!</yellow>\n"
    "<brown>(c) Copyright 1997-2002 Linley Henzell, 2002-2020 Crawl DevTeam";

    return formatted_string::parse_string(msg);
}

formatted_string options_read_status()
{
    string msg;
    FileLineInput f(Options.filename.c_str());

    if (!f.error())
    {
        msg += "<lightgrey>Options read from \"";
#ifdef DGAMELAUNCH
        // For dgl installs, show only the last segment of the .crawlrc
        // file name so that we don't leak details of the directory
        // structure to (untrusted) users.
        msg += Options.basefilename;
#else
        msg += Options.filename;
#endif
        msg += "\".</lightgrey>";
    }
    else
    {
        msg += "<lightred>Options file ";
        if (!Options.filename.empty())
        {
            msg += make_stringf("\"%s\" is not readable",
                                Options.filename.c_str());
        }
        else
            msg += "not found";
        msg += "; using defaults.</lightred>";
    }

    msg += "\n";

    return formatted_string::parse_string(msg);
}

bool is_good_name(const string& name, bool blankOK)
{
    // verification begins here {dlb}:
    // Disallow names that would result in a save named just ".cs".
    if (strip_filename_unsafe_chars(name).empty())
        return blankOK && name.empty();
    return validate_player_name(name);
}

bool validate_player_name(const string &name)
{
#if defined(TARGET_OS_WINDOWS)
    // Quick check for CON -- blows up real good under DOS/Windows.
    if (strcasecmp(name.c_str(), "con") == 0
        || strcasecmp(name.c_str(), "nul") == 0
        || strcasecmp(name.c_str(), "prn") == 0
        || strnicmp(name.c_str(), "LPT", 3) == 0)
    {
        return false;
    }
#endif

    if (strwidth(name) > MAX_NAME_LENGTH)
        return false;

    char32_t c;
    for (const char *str = name.c_str(); int l = utf8towc(&c, str); str += l)
    {
        // The technical reasons are gone, but enforcing some sanity doesn't
        // hurt.
        if (!iswalnum(c) && c != '-' && c != '.' && c != '_' && c != ' ')
            return false;
    }

    return true;
}
