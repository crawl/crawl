/*
 *  database.cc
 *
 *  Created by Peter Berger on 4/15/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cstdlib>
#include <fstream>
#if !_MSC_VER
#include <unistd.h>
#endif

#include "clua.h"
#include "database.h"
#include "files.h"
#include "libutil.h"
#include "stuff.h"

// TextDB handles dependency checking the db vs text files, creating the
// db, loading, and destroying the DB.
class TextDB
{
 public:
    // db_name is the savedir-relative name of the db file,
    // minus the "db" extension.
    TextDB(const char* db_name, ...);
    ~TextDB() { shutdown(); }
    void init();
    void shutdown();
    DBM* get() { return _db; }

    // Make it easier to migrate from raw DBM* to TextDB
    operator bool() const { return _db != 0; }
    operator DBM*() const { return _db; }

 private:
    bool _needs_update() const;
    void _regenerate_db();

 private:
    const char* const _db_name;            // relative to savedir
    std::vector<std::string> _input_files; // relative to datafile dirs
    DBM* _db;
};

// Convenience functions for (read-only) access to generic
// berkeley DB databases.
static void _store_text_db(const std::string &in, const std::string &out);

static TextDB AllDBs[] =
{
    TextDB( "db/descriptions",
            "descript/features.txt",
            "descript/items.txt",
            "descript/unident.txt",
            "descript/monsters.txt",
            "descript/spells.txt",
            "descript/gods.txt",
            "descript/branches.txt",
            "descript/skills.txt",
            "descript/ability.txt",
            "descript/cards.txt",
            NULL),

    TextDB( "db/randart",
            "database/randname.txt",
            "database/rand_wpn.txt", // mostly weapons
            "database/rand_arm.txt", // mostly armour
            "database/rand_all.txt", // jewellery and general
            "database/randbook.txt", // artefact books
            // This doesn't really belong here, but they *are* god gifts...
            "database/monname.txt",  // orcish names for Beogh to choose from
            NULL),

    TextDB( "db/speak",
            "database/monspeak.txt", // monster speech
            "database/monspell.txt", // monster spellcasting speech
            "database/wpnnoise.txt", // noisy weapon speech
            "database/insult.txt",   // imp/demon taunts
            "database/godspeak.txt", // god speech
            NULL),

    TextDB( "db/shout",
            "database/shout.txt",
            "database/insult.txt",   // imp/demon taunts, again
            NULL),

    TextDB( "db/misc",
            "database/miscname.txt", // names for miscellaneous things
            NULL),

    TextDB( "db/quotes",
            "database/quotes.txt",   // quotes for items and monsters
            NULL),

    TextDB( "db/help",               // database for outsourced help texts
            "database/help.txt",
            NULL),

    TextDB( "db/FAQ",                // database for Frequently Asked Questions
            "database/FAQ.txt",
            NULL),
};

static TextDB& DescriptionDB = AllDBs[0];
static TextDB& RandartDB     = AllDBs[1];
static TextDB& SpeakDB       = AllDBs[2];
static TextDB& ShoutDB       = AllDBs[3];
static TextDB& MiscDB        = AllDBs[4];
static TextDB& QuotesDB      = AllDBs[5];
static TextDB& HelpDB        = AllDBs[6];
static TextDB& FAQDB         = AllDBs[7];

// ----------------------------------------------------------------------
// TextDB
// ----------------------------------------------------------------------

TextDB::TextDB(const char* db_name, ...)
    : _db_name(db_name),
      _db(NULL)
{
    va_list args;
    va_start(args, db_name);
    while (true)
    {
        const char* input_file = va_arg(args, const char *);

        if (input_file == 0)
            break;

        ASSERT( strstr(input_file, ".txt") != 0 );      // probably forgot the terminating 0
        _input_files.push_back(input_file);
    }
    va_end(args);
}

void TextDB::init()
{
    if (_needs_update())
        _regenerate_db();

    const std::string full_db_path = get_savedir_path(_db_name);
    _db = dbm_open(full_db_path.c_str(), O_RDONLY, 0660);

    if (_db == NULL)
        end(1, true, "Failed to open DB: %s", full_db_path.c_str());
}

void TextDB::shutdown()
{
    if (_db)
    {
        dbm_close(_db);
        _db = NULL;
    }
}

bool TextDB::_needs_update() const
{
    std::string full_db_path = get_savedir_path(std::string(_db_name) + ".db");
    for (unsigned int i = 0; i < _input_files.size(); i++)
    {
        std::string full_input_path = datafile_path(_input_files[i], true);
        if (is_newer(full_input_path, full_db_path))
            return (true);
    }
    return (false);
}

void TextDB::_regenerate_db()
{
    std::string db_path = get_savedir_path(_db_name);
    std::string full_db_path = db_path + ".db";

    {
        std::string output_dir = get_parent_directory(db_path);
        if (!check_dir("DB directory", output_dir))
            end(1);
    }

    file_lock lock(db_path + ".lk", "wb");
    unlink( full_db_path.c_str() );

    for (unsigned int i = 0; i < _input_files.size(); i++)
    {
        std::string full_input_path = datafile_path(_input_files[i], true);
        _store_text_db(full_input_path, db_path);
    }

    DO_CHMOD_PRIVATE(full_db_path.c_str());
}

// ----------------------------------------------------------------------
// DB system
// ----------------------------------------------------------------------

void databaseSystemInit()
{
    for (unsigned int i = 0; i < ARRAYSZ(AllDBs); i++)
        AllDBs[i].init();
}

void databaseSystemShutdown()
{
    for (unsigned int i = 0; i < ARRAYSZ(AllDBs); i++)
        AllDBs[i].shutdown();
}

////////////////////////////////////////////////////////////////////////////
// Main DB functions


datum database_fetch(DBM *database, const std::string &key)
{
    datum result;
    result.dptr = NULL;
    result.dsize = 0;
    datum dbKey;

    dbKey.dptr = (DPTR_COERCE) key.c_str();
    dbKey.dsize = key.length();

    result = dbm_fetch(database, dbKey);

    return result;
}

std::vector<std::string> database_find_keys(DBM *database,
                                            const std::string &regex,
                                            bool ignore_case,
                                            db_find_filter filter)
{
    text_pattern             tpat(regex, ignore_case);
    std::vector<std::string> matches;

    datum dbKey = dbm_firstkey(database);

    while (dbKey.dptr != NULL)
    {
        std::string key((const char *)dbKey.dptr, dbKey.dsize);

        if (tpat.matches(key)
            && key.find("__") == std::string::npos
            && (filter == NULL || !(*filter)(key, "")))
        {
            matches.push_back(key);
        }

        dbKey = dbm_nextkey(database);
    }

    return (matches);
}

std::vector<std::string> database_find_bodies(DBM *database,
                                              const std::string &regex,
                                              bool ignore_case,
                                              db_find_filter filter)
{
    text_pattern             tpat(regex, ignore_case);
    std::vector<std::string> matches;

    datum dbKey = dbm_firstkey(database);

    while (dbKey.dptr != NULL)
    {
        std::string key((const char *)dbKey.dptr, dbKey.dsize);

        datum dbBody = dbm_fetch(database, dbKey);
        std::string body((const char *)dbBody.dptr, dbBody.dsize);

        if (tpat.matches(body) &&
            key.find("__") == std::string::npos
            && (filter == NULL || !(*filter)(key, body)))
        {
            matches.push_back(key);
        }

        dbKey = dbm_nextkey(database);
    }

    return (matches);
}

///////////////////////////////////////////////////////////////////////////
// Internal DB utility functions
static void _execute_embedded_lua(std::string &str)
{
    // Execute any lua code found between "{{" and "}}".  The lua code
    // is expected to return a string, with which the lua code and
    // braces will be replaced.
    std::string::size_type pos = str.find("{{");
    while (pos != std::string::npos)
    {
        std::string::size_type end = str.find("}}", pos + 2);
        if (end == std::string::npos)
        {
            mpr("Unbalanced {{, bailing.", MSGCH_DIAGNOSTICS);
            break;
        }

        std::string lua_full = str.substr(pos, end - pos + 2);
        std::string lua      = str.substr(pos + 2, end - pos - 2);

        if (clua.execstring(lua.c_str(), "db_embedded_lua", 1))
        {
            std::string err = "{{" + clua.error + "}}";
            str.replace(pos, lua_full.length(), err);
            return;
        }

        std::string result;
        clua.fnreturns(">s", &result);

        str.replace(pos, lua_full.length(), result);

        pos = str.find("{{", pos + result.length());
    }
}

static void _trim_leading_newlines(std::string &s)
{
    s.erase(0, s.find_first_not_of("\n"));
}

static void _add_entry(DBM *db, const std::string &k, std::string &v)
{
    _trim_leading_newlines(v);
    datum key, value;
    key.dptr = (char *) k.c_str();
    key.dsize = k.length();

    value.dptr = (char *) v.c_str();
    value.dsize = v.length();

    if (dbm_store(db, key, value, DBM_REPLACE))
        end(1, true, "Error storing %s", k.c_str());
}

static void _parse_text_db(std::ifstream &inf, DBM *db)
{
    char buf[1000];

    std::string key;
    std::string value;

    bool in_entry = false;
    while (!inf.eof())
    {
        inf.getline(buf, sizeof buf);

        if (*buf == '#')
            continue;

        if (!strncmp(buf, "%%%%", 4))
        {
            if (!key.empty())
                _add_entry(db, key, value);
            key.clear();
            value.clear();
            in_entry = true;
            continue;
        }

        if (!in_entry)
            continue;

        if (key.empty())
        {
            key = buf;
            trim_string(key);
            lowercase(key);
        }
        else
        {
            std::string line = buf;
            trim_string_right(line);
            value += line + "\n";
        }
    }

    if (!key.empty())
        _add_entry(db, key, value);
}

static void _store_text_db(const std::string &in, const std::string &out)
{
    std::ifstream inf(in.c_str());
    if (!inf)
        end(1, true, "Unable to open input file: %s", in.c_str());

    if (DBM *db = dbm_open(out.c_str(), O_RDWR | O_CREAT, 0660))
    {
        _parse_text_db(inf, db);
        dbm_close(db);
    }
    else
        end(1, true, "Unable to open DB: %s", out.c_str());

    inf.close();
}

static std::string _chooseStrByWeight(std::string entry, int fixed_weight = -1)
{
    std::vector<std::string> parts;
    std::vector<int>         weights;

    std::vector<std::string> lines = split_string("\n", entry, false, true);

    int total_weight = 0;
    for (int i = 0, size = lines.size(); i < size; i++)
    {
        // Skip over multiple blank lines, and leading and trailing
        // blank lines.
        while (i < size && lines[i].empty())
            i++;

        if (i == size)
            break;

        int         weight;
        std::string part = "";

        if (sscanf(lines[i].c_str(), "w:%d", &weight))
        {
            i++;
            if (i == size)
                return ("BUG, WEIGHT AT END OF ENTRY");
        }
        else
            weight = 10;

        total_weight += weight;

        while (i < size && !lines[i].empty())
        {
            part += lines[i++];
            part += "\n";
        }
        trim_string(part);

        parts.push_back(part);
        weights.push_back(total_weight);
    }

    if (parts.size() == 0)
        return("BUG, EMPTY ENTRY");

    int choice = 0;
    if (fixed_weight != -1)
        choice = fixed_weight % total_weight;
    else
        choice = random2(total_weight);

    for (int i = 0, size = parts.size(); i < size; i++)
        if (choice < weights[i])
            return(parts[i]);

    return("BUG, NO STRING CHOSEN");
}

#define MAX_RECURSION_DEPTH 10
#define MAX_REPLACEMENTS    100

static std::string _getWeightedString(DBM *database, const std::string &key,
                                      const std::string &suffix,
                                      int fixed_weight = -1)
{
    // We have to canonicalize the key (in case the user typed it
    // in and got the case wrong.)
    std::string canonical_key = key + suffix;
    lowercase(canonical_key);

    // Query the DB.
    datum result = database_fetch(database, canonical_key);

    if (result.dsize <= 0)
    {
        // Try ignoring the suffix.
        canonical_key = key;
        lowercase(canonical_key);

        // Query the DB.
        result = database_fetch(database, canonical_key);

        if (result.dsize <= 0)
            return "";
    }

    // Cons up a (C++) string to return.  The caller must release it.
    std::string str = std::string((const char *)result.dptr, result.dsize);

    return _chooseStrByWeight(str, fixed_weight);
}

static void _call_recursive_replacement(std::string &str, DBM *database,
                                        const std::string &suffix,
                                        int &num_replacements,
                                        int recursion_depth = 0);

std::string getWeightedSpeechString(const std::string &key,
                                    const std::string &suffix,
                                    const int weight)
{
    if (!SpeakDB)
        return ("");

    std::string result = _getWeightedString(SpeakDB, key, suffix, weight);
    if (result.empty())
        return "";

    int num_replacements = 0;
    _call_recursive_replacement(result, SpeakDB, suffix, num_replacements);
    return (result);
}

static std::string _getRandomisedStr(DBM *database, const std::string &key,
                                     const std::string &suffix,
                                     int &num_replacements,
                                     int recursion_depth = 0)
{
    recursion_depth++;
    if (recursion_depth > MAX_RECURSION_DEPTH)
    {
        mpr("Too many nested replacements, bailing.", MSGCH_DIAGNOSTICS);

        return "TOO MUCH RECURSION";
    }

    std::string str = _getWeightedString(database, key, suffix);

    _call_recursive_replacement(str, database, suffix, num_replacements,
                                recursion_depth);

    return str;
}

// Replace any "@foo@" markers that can be found in this database.
// Those that can't be found are left alone for the caller to deal with.
static void _call_recursive_replacement(std::string &str, DBM *database,
                                        const std::string &suffix,
                                        int &num_replacements,
                                        int recursion_depth)
{
    std::string::size_type pos = str.find("@");
    while (pos != std::string::npos)
    {
        num_replacements++;
        if (num_replacements > MAX_REPLACEMENTS)
        {
            mpr("Too many string replacements, bailing.", MSGCH_DIAGNOSTICS);
            return;
        }

        std::string::size_type end = str.find("@", pos + 1);
        if (end == std::string::npos)
        {
            mpr("Unbalanced @, bailing.", MSGCH_DIAGNOSTICS);
            break;
        }

        std::string marker_full = str.substr(pos, end - pos + 1);
        std::string marker      = str.substr(pos + 1, end - pos - 1);

        std::string replacement =
            _getRandomisedStr(database, marker, suffix, num_replacements,
                              recursion_depth);

        if (replacement.empty())
        {
            // Nothing in database, leave it alone and go onto next @foo@
            pos = str.find("@", end + 1);
        }
        else
        {
            str.replace(pos, marker_full.length(), replacement);

            // Start search from pos rather than end + 1, so that if
            // the replacement contains its own @foo@, we can replace
            // that too.
            pos = str.find("@", pos);
        }
    } // while (pos != std::string::npos)
}

static std::string _query_database(DBM *db, std::string key,
                                   bool canonicalise_key, bool run_lua)
{
    if (canonicalise_key)
    {
        // We have to canonicalize the key (in case the user typed it
        // in and got the case wrong.)
        lowercase(key);
    }

    // Query the DB.
    datum result = database_fetch(db, key);

    std::string str((const char *)result.dptr, result.dsize);

    if (run_lua)
        _execute_embedded_lua(str);

    return (str);
}

/////////////////////////////////////////////////////////////////////////////
// Quote DB specific functions.

std::string getQuoteString(const std::string &key)
{
    if (!QuotesDB)
        return ("");

    return _query_database(QuotesDB.get(), key, true, true);
}

/////////////////////////////////////////////////////////////////////////////
// Description DB specific functions.

std::string getLongDescription(const std::string &key)
{
    if (!DescriptionDB.get())
        return ("");

    return _query_database(DescriptionDB.get(), key, true, true);
}

std::vector<std::string> getLongDescKeysByRegex(const std::string &regex,
                                                db_find_filter filter)
{
    if (!DescriptionDB.get())
    {
        std::vector<std::string> empty;
        return (empty);
    }

    return database_find_keys(DescriptionDB.get(), regex, true, filter);
}

std::vector<std::string> getLongDescBodiesByRegex(const std::string &regex,
                                                  db_find_filter filter)
{
    if (!DescriptionDB.get())
    {
        std::vector<std::string> empty;
        return (empty);
    }

    return database_find_bodies(DescriptionDB.get(), regex, true, filter);
}

/////////////////////////////////////////////////////////////////////////////
// Shout DB specific functions.
std::string getShoutString(const std::string &monst,
                           const std::string &suffix)
{
    int num_replacements = 0;

    return _getRandomisedStr(ShoutDB.get(), monst, suffix,
                             num_replacements);
}

/////////////////////////////////////////////////////////////////////////////
// Speak DB specific functions.
std::string getSpeakString(const std::string &key)
{
    if (!SpeakDB)
        return ("");

    int num_replacements = 0;

#ifdef DEBUG_MONSPEAK
    mprf(MSGCH_DIAGNOSTICS, "monster speech lookup for %s", key.c_str());
#endif
    return _getRandomisedStr(SpeakDB, key, "", num_replacements);
}

/////////////////////////////////////////////////////////////////////////////
// Randname DB specific functions.
std::string getRandNameString(const std::string &itemtype,
                              const std::string &suffix)
{
    if (!RandartDB)
        return ("");

    int num_replacements = 0;

    return _getRandomisedStr(RandartDB, itemtype, suffix,
                             num_replacements);
}

/////////////////////////////////////////////////////////////////////////////
// Help DB specific functions.

std::string getHelpString(const std::string &topic)
{
    return _query_database(HelpDB.get(), topic, false, true);
}

/////////////////////////////////////////////////////////////////////////////
// FAQ DB specific functions.
std::vector<std::string> getAllFAQKeys()
{
    if (!FAQDB.get())
    {
        std::vector<std::string> empty;
        return (empty);
    }

    return database_find_keys(FAQDB.get(), "^q.+", false);
}

std::string getFAQ_Question(const std::string &key)
{
    return _query_database(FAQDB.get(), key, false, true);
}

std::string getFAQ_Answer(const std::string &question)
{
    std::string key = "a" + question.substr(1, question.length()-1);
    return _query_database(FAQDB.get(), key, false, true);
}

/////////////////////////////////////////////////////////////////////////////
// Miscellaneous DB specific functions.

std::string getMiscString(const std::string &misc,
                          const std::string &suffix)

{
    if (!MiscDB)
        return ("");

    int num_replacements = 0;

    return _getRandomisedStr(MiscDB, misc, suffix, num_replacements);
}
