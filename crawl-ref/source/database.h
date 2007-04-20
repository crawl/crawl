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

extern "C" {
#ifdef DB_NDBM

#   include <ndbm.h>
#   define DPTR_COERCE void *

#else

#   define DB_DBM_HSEARCH 1
#   include <db.h>
#   define DPTR_COERCE char *

#endif
}

typedef std::list<DBM *> db_list;

extern db_list openDBList;

void databaseSystemInit();
void databaseSystemShutdown();
DBM  *openDB(const char *dbFilename);
datum database_fetch(DBM *database, const std::string &key);

std::string getLongDescription(const std::string &key);

#endif
