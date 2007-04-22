/*
 *  database.h
 *  Crawl
 *
 *  Created by Peter Berger on 4/15/07.
 *  $Id:$
 */


#ifndef DATABASE_H
#define DATABASE_H

#include "AppHdr.h"
#include "externs.h"
#include <list>

#ifdef DB_NDBM
extern "C" {
#   include <ndbm.h>
}
#elif defined(DB_DBH)
extern "C" {
#   define DB_DBM_HSEARCH 1
#   include <db.h>
}
#elif defined(USE_SQLITE_DBM)
#   include <sqldbm.h>
#else
#   error DBM interfaces unavailable!
#endif

#define DPTR_COERCE char *

typedef std::list<DBM *> db_list;

extern db_list openDBList;

void databaseSystemInit();
void databaseSystemShutdown();
DBM  *openDB(const char *dbFilename);
datum database_fetch(DBM *database, const std::string &key);

std::string getLongDescription(const std::string &key);

#endif
