/**
 * @file  regex-wrapper.cc
 * @brief Wrapper for chosen regex library (PCRE or POSIX)
 **/

#include "AppHdr.h"
#include "regex-wrapper.h"

#ifdef REGEX_PCRE
    // Statically link pcre on Windows
    #if defined(TARGET_OS_WINDOWS)
        #define PCRE_STATIC
    #endif

    #include <pcre.h>
#else
    // REGEX_POSIX
    #include <regex>
    #include <codecvt>
#endif

using namespace std;

#ifndef debuglog
#define debuglog(...) {}
#endif

// warn about bad regexes, but only once each
static void _warn_bad_regex(const string& re)
{
    static vector<string> _bad_regexes;
    if (find(_bad_regexes.begin(), _bad_regexes.end(), re) == _bad_regexes.end())
    {
        fprintf(stderr, "WARNING: Bad regex: /%s/\n", re.c_str());
        _bad_regexes.push_back(re);
    }
}

#ifdef REGEX_PCRE

// ovector needs to be big enough for all submatches (capture groups),
// even if we don't use all of them, because if it's not big enough, we won't
// get any results at all
static const int MAX_SUBMATCHES = 20;
static const int OVECTOR_SIZE = 3 * (MAX_SUBMATCHES + 1); // extra 1 for main match

// compile regex and return pointer to resul, or nullptr on error
static pcre* _compile_regex(const string& pattern, bool ignore_case = false)
{
    const char *error = nullptr;
    int erroffset = -1;
    int flags = PCRE_UTF8;
    if (ignore_case)
        flags |= PCRE_CASELESS;

    pcre* re = pcre_compile(pattern.c_str(), flags, &error, &erroffset, nullptr);
    if (re == nullptr)
    {
        debuglog("Error in regex \"%s\" at position %d: %s", pattern.c_str(),
             erroffset, error ? error : "Unknown error");
    }
    return re;
}

bool regexp_valid(const string& pattern)
{
    try
    {
        pcre* re = _compile_regex(pattern);
        if (re == nullptr)
            return false;
        else
        {
            pcre_free(re);
            return true;
        }

    }
    catch (const exception& e)
    {
        return false;
    }
}

bool regexp_match(const string& s, const string& pattern, bool ignore_case,
                  size_t* mpos, size_t* mlength)
{
    bool result = false;
    pcre* re = nullptr;

    try
    {
        re = _compile_regex(pattern, ignore_case);
        if (re == nullptr)
            return "";

        int ovector[OVECTOR_SIZE] = {-1};
        int rc = pcre_exec(re, nullptr, s.c_str(), s.length(), 0, 0,
                           ovector, OVECTOR_SIZE);

        // if rc is positive, it is a count of matches
        if (rc > 0 && ovector[0] >= 0 && ovector[1] >= 0)
        {
            if (mpos)
                *mpos = (size_t)ovector[0];
            if (mlength)
                *mlength = (size_t)(ovector[1] - ovector[0]);
            result = true;
        }
        else if (rc != PCRE_ERROR_NOMATCH)
            debuglog("Error in regexp_search(\"%s\", \"%s\"): %d", s.c_str(), pattern.c_str(), rc);
    }
    catch (exception& e)
    {
        _warn_bad_regex(pattern);
    }

    if (re)
        pcre_free(re);

    return result;
}

string regexp_search(const string& s, const string& pattern, bool ignore_case)
{
    size_t mpos, mlength;

    if (regexp_match(s, pattern, ignore_case, &mpos, &mlength))
    {
        if (mpos < s.length() && mpos + mlength <= s.length())
            return s.substr(mpos, mlength);
    }
    return "";
}

static string _handle_backreferences(const string &s, const string& subst, int* ovector, int num_matches)
{
    string result;
    size_t pos = 0, prev = 0;

    while ((pos = subst.find('$', prev)) != string::npos)
    {
        // add non-matching prefix
        if (pos > prev)
        {
            result += subst.substr(prev, pos - prev);
            prev = pos;
        }

        pos++;

        // $$ is an escape sequence for a literal dollar sign
        if (pos < subst.length() && subst[pos] == '$')
        {
            result += '$';
            pos++;
            prev = pos;
            continue;
        }

        int id = 0;
        while (pos < subst.length() && subst[pos] >= '0' && subst[pos] <= '9')
        {
            id = (id * 10) + (subst[pos] - '0');
            pos++;
        }

        bool success = false;
        // note: num_matches includes the main match, so number of submatches is one less
        if (id > 0 && id < num_matches)
        {
            int start = ovector[id*2];
            int end = ovector[id*2 + 1];
            if (start >= 0 && end >= 0 && start < (int)s.length() && end <= (int)s.length())
            {
                result += s.substr(start, end - start);
                success = true;
            }
            else
                debuglog("Bad indexes for backreference %d: %d, %d", id, start, end);
        }

        if (!success)
            debuglog("Bad regex backreference %d in \"%s\"", id, subst.c_str());

        prev = pos;
    }

    // add non-matching suffix
    if (prev < subst.length())
        result += subst.substr(prev);

    return result;
}

// replace all instances of pattern with the specified replacement string
string regexp_replace(const string& s, const string& pattern, const string& subst)
{
    string result;
    pcre* re = nullptr;

    try
    {
        re = _compile_regex(pattern);
        if (re == nullptr)
            return s;

        int ovector[OVECTOR_SIZE] = {-1};
        int pos = 0;
        int rc;

        do {
            rc = pcre_exec(re, nullptr, s.c_str(), s.length(), pos, 0,
                           ovector, OVECTOR_SIZE);

            // if rc is positive, it is the number of matches,
            // -1 means no match, other negative values are errors
            //debuglog("Return code from pcre_exec: %d", rc);
            if (rc > 0)
            {
                int mstart = ovector[0];
                int mend = ovector[1];

                // add non-matching prefix
                if (mstart > pos)
                    result += s.substr(pos, mstart - pos);

                // add replacement for match
                result += _handle_backreferences(s, subst, ovector, rc);

                pos = mend;
            }
            else if (rc != PCRE_ERROR_NOMATCH)
                debuglog("Error in regexp_replace(\"%s\", \"%s\", \"%s\"): %d", s.c_str(), pattern.c_str(), subst.c_str(), rc);

        } while (rc > 0);

        // add non-matching suffix
        if (pos < (int)s.length())
            result += s.substr(pos);
    }
    catch (exception& e)
    {
        _warn_bad_regex(pattern);
        result = s;
    }

    if (re)
        pcre_free(re);

    return result;
}

#else // REGEX_POSIX

// std::regex doesn't work properly for UTF-8, so we are forced to convert to wstring
static std::wstring_convert<std::codecvt_utf8<wchar_t>> _conv;

static wregex& _compile_pattern(const string& pattern, bool ignore_case)
{
    // cache last compiled regex for performance reasons
    // even with this, POSIX regex is 5-10 times slower than PCRE, depending on the operation
    static wregex wre;
    static string last_pattern;
    static bool last_ignore_case = false;

    if (pattern != last_pattern || ignore_case != last_ignore_case)
    {
        // default for wregex constructor is ECMAScript
        auto flags = regex_constants::ECMAScript;
        if (ignore_case)
            flags |= regex_constants::icase;
        wstring wpattern = _conv.from_bytes(pattern);
        wregex wre_new(wpattern, flags);
        wre = wre_new;

        last_pattern = pattern;
        last_ignore_case = ignore_case;
    }

    return wre;
}

bool regexp_valid(const string& pattern)
{
    try
    {
        // passing ignore_case = true to increase chance of cached pattern being re-used
        // (this is usually called to check a user's search, so the subsequent
        // call to regexp_match will use ignore_case = true)
        _compile_pattern(pattern, true);
        return true;
    }
    catch (const exception& e)
    {
        // wregex constructor will throw an exception if pattern is invalid
        return false;
    }
}

bool regexp_match(const string& s, const string& pattern, bool ignore_case,
                  size_t* mpos, size_t* mlength)
{
    string match = regexp_search(s, pattern, ignore_case);
    if (match.empty())
        return false;

    if (mpos)
        *mpos = s.find(match);
    if (mlength)
        *mlength = match.length();

    return true;
}

string regexp_search(const string& s, const string& pattern, bool ignore_case)
{
    try
    {
        wstring ws = _conv.from_bytes(s);

        wregex& wre = _compile_pattern(pattern, ignore_case);
        wsmatch wmatch;
        if (regex_search(ws, wmatch, wre))
            return _conv.to_bytes(wmatch.str());
    }
    catch (exception& e)
    {
        _warn_bad_regex(pattern);
    }
    return "";
}

// replace all instances of pattern with the specified replacement string
string regexp_replace(const string& s, const string& pattern, const string& subst)
{
    try
    {
        wstring ws = _conv.from_bytes(s);
        wstring wsubst = _conv.from_bytes(subst);

        wregex& wre = _compile_pattern(pattern, false);
        wstring result = regex_replace(ws, wre, wsubst);

        return _conv.to_bytes(result);
    }
    catch (exception& e)
    {
        _warn_bad_regex(pattern);
        return s;
    }
}

#endif
