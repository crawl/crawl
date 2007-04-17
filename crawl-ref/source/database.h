/*
 *  database.h
 *  Crawl
 *
 *  Created by Peter Berger on 4/15/07.
 *  $Id:$
 */


#ifndef DATABASE_H
#define DATABASE_H

#undef DEBUG // hack
#define NDEBUG
#include "externs.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <ndbm.h>

// For convenience during shutdown.
typedef struct dbList_s dbList_t;
#ifdef __cplusplus
}
#endif
extern dbList_t *openDBList;

void databaseSystemInit();
void databaseSystemShutdown();
DBM  *openDB(const char *dbFilename);
datum *database_fetch(DBM *database, char *key);


std::string getLongDescription(const char *key);

#endif
