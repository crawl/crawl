/*
 *  database.cc
 *  Crawl
 *
 *  Created by Peter Berger on 4/15/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#endif
#include <stdlib.h>
#include "database.h"
#include "files.h"

// Convenience functions for (read-only) access to generic
// berkeley DB databases.

#define nil 0

static bool databaseIsInitted = false;
dbList_t *openDBList;
DBM      *descriptionDB;

struct dbList_s {
    DBM         *dbPtr;
    dbList_s    *next;
} dbList_s;


void databaseSystemInit() {
    if (!databaseIsInitted) {
        databaseIsInitted = true;
        openDBList = nil;
        std::string descriptionPath = datafile_path("descriptions.db");
        descriptionPath.erase(descriptionPath.length() - 3, descriptionPath.length());
        descriptionDB = openDB(descriptionPath.c_str());
    }
}

void databaseSystemShutdown() {
    if (!databaseIsInitted) {
        // All done.
        return;
    }
    dbList_t *current = openDBList;
    dbList_t *prev = nil;
    while (current) {
        dbm_close(current->dbPtr);
        prev = current;
        current = current->next;
        free(prev);
    }
}

// This is here, and is external, just for future expansion -- if we
// want to allow external modules to manage their own DB, they can
// use this for the sake of convenience.  It's arguable that it's
// morally wrong to have the database module manage the memory here.
// But hey, life is hard and you can write your own berkeley DB
// calls if you like.
DBM   *openDB(const char *dbFilename) {
    if (!databaseIsInitted) {
        return nil;
    }
    DBM *dbToReturn = dbm_open(dbFilename, O_RDONLY, 0660);
    if (dbToReturn) {
        if (openDBList) {
            dbList_t *endOfDBList = openDBList; 
            while (openDBList->next) {
                endOfDBList = openDBList->next;
            }
            endOfDBList->next = (dbList_t *)malloc(sizeof(dbList_t));
            endOfDBList = endOfDBList->next;
            endOfDBList->next = nil;
            endOfDBList->dbPtr = dbToReturn;
            
        } else {
            openDBList = (dbList_t *)malloc(sizeof(dbList_t));
            openDBList->next = nil;
            openDBList->dbPtr = dbToReturn;
        }
    } else {
        dbToReturn = nil;
    }
    return dbToReturn;
}

datum database_fetch(DBM *database, const char *key) {
    datum result;
    result.dptr = nil;
    result.dsize = 0;
    if (!databaseIsInitted) {
        return result;
    }
    datum dbKey;
    
    dbKey.dptr = (void *)key;
    dbKey.dsize = strlen(key); 
    
    result = dbm_fetch(database, dbKey);
    
    return result;
}

char *private_strlwr(char *string) {
    int i;
    for (i = 0; string[i]; i++) {
        string[i] = tolower(string[i]);
    }
    return string;
} 

std::string getLongDescription(const char *key) {
    if (!databaseIsInitted) {
        return nil;
    }
    // We have to canonicalize the key (in case the user typed it
    // in and got the case wrong.
    int len = strlen(key);
    char *lowercaseKey = (char *)malloc(len+1);
    (void) strncpy(lowercaseKey, key, len+1);
    lowercaseKey = private_strlwr(lowercaseKey);
    
    // Query the DB.
    datum result = database_fetch(descriptionDB, (const char *)lowercaseKey);
    
    // Cons up a (C++) string to return.  The caller must release it.
    std::string stringToReturn((const char *)result.dptr, result.dsize);
    // free the result...
    free(lowercaseKey);
    
    return stringToReturn;
}
