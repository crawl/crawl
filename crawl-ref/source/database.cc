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

#define DESC_BASE_NAME "descript"
#define DESC_TXT       (DESC_BASE_NAME ".txt")
#define DESC_DB        (DESC_BASE_NAME ".db")

static void generate_description_db();

time_t file_modtime(const std::string &file)
{
    struct stat filestat;
    if (stat(file.c_str(), &filestat))
        return (0);

    return (filestat.st_mtime);
}

// Returns true if file a is newer than file b.
bool is_newer(const std::string &a, const std::string &b)
{
    return (file_modtime(a) > file_modtime(b));
}

void check_newer(const std::string &target,
                 const std::string &dependency,
                 void (*action)())
{
    if (is_newer(dependency, target))
        action();
}

void databaseSystemInit()
{
    if (!descriptionDB)
    {
        std::string descriptionPath = get_savedir_path(DESC_DB);
        check_newer(descriptionPath,
                    datafile_path(DESC_TXT),
                    generate_description_db);
        descriptionPath.erase(descriptionPath.length() - 3);
        if (!(descriptionDB = openDB(descriptionPath.c_str())))
            end(1, true, "Failed to open DB: %s", descriptionPath.c_str());
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

std::string getLongDescription(const std::string &key)
{
    if (!descriptionDB)
        return ("");

    // We have to canonicalize the key (in case the user typed it
    // in and got the case wrong.
    std::string canonical_key = key;
    lowercase(canonical_key);
    
    // Query the DB.
    datum result = database_fetch(descriptionDB, canonical_key);
    
    // Cons up a (C++) string to return.  The caller must release it.
    return std::string((const char *)result.dptr, result.dsize);
}

static void store_descriptions(const std::string &in, const std::string &out);
static void generate_description_db()
{
    std::string db_path = get_savedir_path(DESC_BASE_NAME);
    std::string full_db_path = get_savedir_path(DESC_DB);
    std::string txt_path = datafile_path(DESC_TXT);

    file_lock lock(get_savedir_path(DESC_BASE_NAME ".lk"), "wb");
    unlink( full_db_path.c_str() );
    store_descriptions(txt_path, db_path);
    DO_CHMOD_PRIVATE(full_db_path.c_str());
}

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

static void parse_descriptions(std::ifstream &inf, DBM *db)
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

static void store_descriptions(const std::string &in, const std::string &out)
{
    std::ifstream inf(in.c_str());
    if (!inf)
        end(1, true, "Unable to open input file: %s", in.c_str());

    if (DBM *db = dbm_open(out.c_str(), O_RDWR | O_CREAT, 0660))
    {
        parse_descriptions(inf, db);
        dbm_close(db);
    }
    else
        end(1, true, "Unable to open DB: %s", out.c_str());

    inf.close();
}
