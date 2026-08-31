/**
 * @file localise.cc
 * @brief String localisation (translation)
 **/

#include "AppHdr.h"
#include "database.h"
#include "localise.h"
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

    string result = getTranslatedString(s);
    return result == "" ? s : result;
}
