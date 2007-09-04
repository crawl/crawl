/*
 *  database.cc
 *  Crawl
 *
 *  Created by Peter Berger on 4/15/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstdlib>
#include <fstream>
#include "database.h"
#include "files.h"
#include "libutil.h"
#include "stuff.h"

// Convenience functions for (read-only) access to generic
// berkeley DB databases.

db_list openDBList;
DBM     *descriptionDB;

DBM     *shoutDB;
DBM     *speakDB;

#define DESC_BASE_NAME "descript"
#define DESC_TXT_DIR   "descript"
#define DESC_DB        (DESC_BASE_NAME ".db")

#define SHOUT_BASE_NAME "shout"
#define SHOUT_TXT       (SHOUT_BASE_NAME ".txt")
#define SHOUT_DB        (SHOUT_BASE_NAME ".db")

#define SPEAK_BASE_NAME "speak"
#define SPEAK_TXT       (SPEAK_BASE_NAME ".txt")
#define SPEAK_DB        (SPEAK_BASE_NAME ".db")


static std::vector<std::string> description_txt_paths();
static void generate_description_db();
static void generate_shout_db();
static void generate_speak_db();

void databaseSystemInit()
{
    if (!descriptionDB)
    {
        std::string descriptionPath = get_savedir_path(DESC_DB);
        std::vector<std::string> textPaths = description_txt_paths();

        // If any of the description text files are newer then
        // aggregated description db, then regenerate the whole db
        for (int i = 0, size = textPaths.size(); i < size; i++)
            if (is_newer(textPaths[i], descriptionPath))
            {
                generate_description_db();
                break;
            }

        descriptionPath.erase(descriptionPath.length() - 3);
        if (!(descriptionDB = openDB(descriptionPath.c_str())))
            end(1, true, "Failed to open DB: %s", descriptionPath.c_str());
    }

    if (!shoutDB)
    {
        std::string shoutPath = get_savedir_path(SHOUT_DB);
        std::string shoutText = datafile_path(SHOUT_TXT);

        check_newer(shoutPath, shoutText, generate_shout_db);

        shoutPath.erase(shoutPath.length() - 3);
        if (!(shoutDB = openDB(shoutPath.c_str())))
            end(1, true, "Failed to open DB: %s", shoutPath.c_str());
    }
    if (!speakDB)
    {
        std::string speakPath = get_savedir_path(SPEAK_DB);
        std::string speakText = datafile_path(SPEAK_TXT);

        check_newer(speakPath, speakText, generate_speak_db);

        speakPath.erase(speakPath.length() - 3);
        if (!(speakDB = openDB(speakPath.c_str())))
            end(1, true, "Failed to open DB: %s", speakPath.c_str());
    }

}

void databaseSystemShutdown()
{
    for (db_list::iterator i = openDBList.begin();
         i != openDBList.end(); ++i)
    {
        dbm_close(*i);
    }
    openDBList.clear();
    descriptionDB = NULL;
}

////////////////////////////////////////////////////////////////////////////
// Main DB functions

// This is here, and is external, just for future expansion -- if we
// want to allow external modules to manage their own DB, they can
// use this for the sake of convenience.  It's arguable that it's
// morally wrong to have the database module manage the memory here.
// But hey, life is hard and you can write your own berkeley DB
// calls if you like.
DBM   *openDB(const char *dbFilename)
{
    DBM *dbToReturn = dbm_open(dbFilename, O_RDONLY, 0660);
    if (dbToReturn)
        openDBList.push_front(dbToReturn);

    return dbToReturn;
}

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

///////////////////////////////////////////////////////////////////////////
// Internal DB utility functions
static void trim_right(std::string &s)
{
    s.erase(s.find_last_not_of(" \r\t\n") + 1);
}

static void trim_leading_newlines(std::string &s)
{
    s.erase(0, s.find_first_not_of("\n"));
}

static void add_entry(DBM *db, const std::string &k, std::string &v)
{
    trim_leading_newlines(v);
    datum key, value;
    key.dptr = (char *) k.c_str();
    key.dsize = k.length();

    value.dptr = (char *) v.c_str();
    value.dsize = v.length();

    if (dbm_store(db, key, value, DBM_REPLACE))
        end(1, true, "Error storing %s", k.c_str());
}

static void parse_text_db(std::ifstream &inf, DBM *db)
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
                add_entry(db, key, value);
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
            trim_right(line);
            value += line + "\n";
        }
    }

    if (!key.empty())
        add_entry(db, key, value);
}

static void store_text_db(const std::string &in, const std::string &out)
{
    std::ifstream inf(in.c_str());
    if (!inf)
        end(1, true, "Unable to open input file: %s", in.c_str());

    if (DBM *db = dbm_open(out.c_str(), O_RDWR | O_CREAT, 0660))
    {
        parse_text_db(inf, db);
        dbm_close(db);
    }
    else
        end(1, true, "Unable to open DB: %s", out.c_str());

    inf.close();
}

static std::string chooseStrByWeight(std::string entry)
{
    std::vector<std::string> parts;
    std::vector<int>         weights;

    std::vector<std::string> lines = split_string("\n", entry, false, true);

    int total_weight = 0;
    for (int i = 0, size = lines.size(); i < size; i++)
    {
        // Skip over multiple blank lines, and leading and trailing
        // blank lines.
        while (i < size && lines[i] == "")
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

        while (i < size && lines[i] != "")
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

    int choice = random2(total_weight);
    std::string str = "";

    for (int i = 0, size = parts.size(); i < size; i++)
        if (choice < weights[i])
            return(parts[i]);

    return("BUG, NO STRING CHOSEN");
}

#define MAX_RECURSION_DEPTH 10
#define MAX_REPLACEMENTS    100

static std::string getRandomizedStr(DBM *database, const std::string &key,
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

    // We have to canonicalize the key (in case the user typed it
    // in and got the case wrong.)
    std::string canonical_key = key + suffix;
    lowercase(canonical_key);

    // Query the DB.
    datum result = database_fetch(database, canonical_key);

    if (result.dsize <= 0)
    {
        // Try ignoring the suffix
        canonical_key = key;
        lowercase(canonical_key);

        // Query the DB.
        result = database_fetch(database, canonical_key);

        if (result.dsize <= 0)
            return "";
    }

    // Cons up a (C++) string to return.  The caller must release it.
    std::string str = std::string((const char *)result.dptr, result.dsize);

    str = chooseStrByWeight(str);

    // Replace any "@foo@" markers that can be found in this database;
    // those that can't be found are left alone for the caller to deal
    // with.
    std::string::size_type pos = str.find("@");
    while (pos != std::string::npos)
    {
        num_replacements++;
        if (num_replacements > MAX_REPLACEMENTS)
        {
            mpr("Too many string replacements, bailing.", MSGCH_DIAGNOSTICS);

            return "TOO MANY REPLACEMENTS";
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
            getRandomizedStr(database, marker, suffix, num_replacements,
                             recursion_depth);

        if (replacement == "")
            // Nothing in database, leave it alone and go onto next @foo@
            pos = str.find("@", end + 1);
        else
        {
            str.replace(pos, marker_full.length(), replacement);

            // Start search from pos rather than end + 1, so that if
            // the replacement contains its own @foo@, we can replace
            // that too.
            pos = str.find("@", pos);
        }
    } // while (pos != std::string::npos)

    return str;
}

/////////////////////////////////////////////////////////////////////////////
// Description DB specific functions.

std::string getLongDescription(const std::string &key)
{
    if (!descriptionDB)
        return ("");

    // We have to canonicalize the key (in case the user typed it
    // in and got the case wrong.)
    std::string canonical_key = key;
    lowercase(canonical_key);
    
    // Query the DB.
    datum result = database_fetch(descriptionDB, canonical_key);
    
    // Cons up a (C++) string to return.  The caller must release it.
    return std::string((const char *)result.dptr, result.dsize);
}

static std::vector<std::string> description_txt_paths()
{
    std::vector<std::string> txt_file_names;
    std::vector<std::string> paths;

    txt_file_names.push_back("features");
    txt_file_names.push_back("items");
    txt_file_names.push_back("monsters");
    txt_file_names.push_back("spells");
/*
    txt_file_names.push_back("shout");
    txt_file_names.push_back("speak");
*/
    for (int i = 0, size = txt_file_names.size(); i < size; i++)
    {
        std::string name = DESC_TXT_DIR;
        name += FILE_SEPARATOR;
        name += txt_file_names[i];
        name += ".txt";

        std::string txt_path = datafile_path(name);

        if (!txt_path.empty())
            paths.push_back(txt_path);
    }

    return (paths);
}

static void generate_description_db()
{
    std::string db_path = get_savedir_path(DESC_BASE_NAME);
    std::string full_db_path = get_savedir_path(DESC_DB);

    std::vector<std::string> txt_paths = description_txt_paths();

    file_lock lock(get_savedir_path(DESC_BASE_NAME ".lk"), "wb");
    unlink( full_db_path.c_str() );

    for (int i = 0, size = txt_paths.size(); i < size; i++)
        store_text_db(txt_paths[i], db_path);
    DO_CHMOD_PRIVATE(full_db_path.c_str());
}

/////////////////////////////////////////////////////////////////////////////
// Shout DB specific functions.
std::string getShoutString(const std::string &monst,
                           const std::string &suffix)
{
    int num_replacements = 0;

    return getRandomizedStr(shoutDB, monst, suffix, num_replacements); 
}

static void generate_shout_db()
{
    std::string db_path      = get_savedir_path(SHOUT_BASE_NAME);
    std::string full_db_path = get_savedir_path(SHOUT_DB);
    std::string txt_path     = datafile_path(SHOUT_TXT);

    file_lock lock(get_savedir_path(SHOUT_BASE_NAME ".lk"), "wb");
    unlink( full_db_path.c_str() );

    store_text_db(txt_path, db_path);
    DO_CHMOD_PRIVATE(full_db_path.c_str());
}

/////////////////////////////////////////////////////////////////////////////
// Speak DB specific functions.
std::string getSpeakString(const std::string &monst)
{
    int num_replacements = 0;

    return getRandomizedStr(speakDB, monst, "", num_replacements); 
}

static void generate_speak_db()
{
    std::string db_path      = get_savedir_path(SPEAK_BASE_NAME);
    std::string full_db_path = get_savedir_path(SPEAK_DB);
    std::string txt_path     = datafile_path(SPEAK_TXT);

    file_lock lock(get_savedir_path(SPEAK_BASE_NAME ".lk"), "wb");
    unlink( full_db_path.c_str() );

    store_text_db(txt_path, db_path);
    DO_CHMOD_PRIVATE(full_db_path.c_str());
}
