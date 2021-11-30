/**
 * @file  xlate.cc
 * @brief Low-level translation routines.
 * This implementation uses gnu gettext, but references to gettext are deliberately kept
 * within this file to allow easy change to a different implementation.
 **/

#include "AppHdr.h"
#include "xlate.h"
#include "clua.h"
#include "database.h"
#include "stringutil.h"

#include <cstring>
using namespace std;

#ifdef NO_TRANSLATE
//// compile without translation logic ////

string cxlate(const string &context, const string &msgid)
{
    return msgid;
}

string cnxlate(const string &context,
        const string &msgid1, const string &msgid2, unsigned long n)
{
    return (n == 1 ? msgid1 : msgid2);
}

#else
//// compile with translation logic ////

#include <clocale>
#include <libintl.h>

// markers for embedded expressions
const string exp_start = "((";
const string exp_end = "))";


// translate with context
//
// context = the context in which the text is being used (optional, default=none)
//  (for disambiguating same English text used in different contexts which may require different translations)
//  if no translation is found in the specified context, will look for translation at global (no) context
// msgid = English text to be translated
// fallback_en = fall back to English if no translation is found?
string cxlate(const string &context, const string &msgid, bool fallback_en)
{
    if (msgid.empty())
    {
        return msgid;
    }

    string translation;

    if (!context.empty())
    {
        // check for translation in specific context
        string ctx_msgid = string("{") + context + "}" + msgid;
        translation = getTranslatedString(ctx_msgid);
    }

    if (translation.empty())
    {
        // check for translation in global context
        translation = getTranslatedString(msgid);
    }

    if (translation.empty() && fallback_en)
        return msgid;
    else
        return translation;
}

// translate with context and number
// select the plural form corresponding to number
//
// context = the context in which the text is being used (optional, default=none)
//  (for disambiguating same English text used in different contexts which may require different translations)
//  if no translation is found in the specified context, will look for translation at global (no) context
// msgid1 = English singular text
// msgid2 = English plural text
// n = the count of whatever it is
string cnxlate(const string &context,
        const string &msgid1, const string &msgid2, unsigned long n)
{
    if (msgid1.empty() || msgid2.empty())
    {
        // apply English rules
        return (n == 1 ? msgid1 : msgid2);
    }

    if (n == 1)
    {
        return cxlate(context, msgid1);
    }
    else
    {
        string result = cxlate(context, msgid2);

        if (result.substr(0, exp_start.length()) != exp_start)
            // single plural form
           return result;

        // pass in the value of n. Surely there's a better way to do this.
        string lua_prefix = make_stringf("n=%ld\nreturn ", n);

        vector<string> lines = split_string("\n", result, false, false);
        for (const string& line: lines)
        {
            size_t pos1 = line.find(exp_start);
            size_t pos2 = line.rfind(exp_end);
            if (pos1 != string::npos && pos2 != string::npos)
            {
                string condition = line.substr(pos1 + exp_start.length(), pos2 - pos1 - exp_start.length());
                if (condition == "")
                {
                    // unconditional
                    return line.substr(pos2 + exp_end.length());
                }

                // evaluate the condition
                string lua = lua_prefix + condition;
                if (clua.execstring(lua.c_str(), "plural_lua", 1))
                {
                    // error
                    continue;
                }

                bool res;
                clua.fnreturns(">b", &res);
                if (res)
                    return line.substr(pos2 + exp_end.length());
            }
        }

        return msgid2;
    }
}

#endif
