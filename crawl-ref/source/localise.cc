/**
 * @file localise.cc
 * @brief String localisation (translation)
 **/

#include "AppHdr.h"
#include "database.h"
#include "localise.h"
#include "localise-util.h"
#include "options.h"
#include "stringutil.h"

bool localisation_active()
{
    return Options.language != lang_t::EN;
}

string localise(const string &s)
{
    if (!localisation_active() || s.empty())
        return s;

    // try simple translation first
    string result = xlate(s);
    if (!result.empty())
        return result;

    // check for leading/trailing whitespace
    string trimmed = trimmed_string(s);
    if (trimmed.length() != s.length())
    {
        if (trimmed.empty())
        {
            // all whitespace
            return s;
        }
        return replace_all(s, trimmed, localise(trimmed));
    }

    // TODO: handle more complicated cases

    // failed - return the original
    return s;
}
