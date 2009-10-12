/*
 *  database.h
 *
 *  Created by Peter Berger on 4/15/07.
 */


#ifndef DATABASE_H
#define DATABASE_H

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

void databaseSystemInit();
void databaseSystemShutdown();
datum database_fetch(DBM *database, const std::string &key);

typedef bool (*db_find_filter)(std::string key, std::string body);

std::vector<std::string> database_find_keys(DBM *database,
                                            const std::string &regex,
                                            bool ignore_case = false,
                                            db_find_filter filter = NULL);
std::vector<std::string> database_find_bodies(DBM *database,
                                              const std::string &regex,
                                              bool ignore_case = false,
                                              db_find_filter filter = NULL);


std::string getWeightedSpeechString(const std::string &key,
                                    const std::string &suffix,
                                    const int weight = -1);

std::string getQuoteString(const std::string &key);
std::string getLongDescription(const std::string &key);

std::vector<std::string> getLongDescKeysByRegex(const std::string &regex,
                                                db_find_filter filter = NULL);
std::vector<std::string> getLongDescBodiesByRegex(const std::string &regex,
                                                  db_find_filter filter = NULL);

std::string getShoutString(const std::string &monst,
                           const std::string &suffix = "");
std::string getSpeakString(const std::string &key);
std::string getRandNameString(const std::string &itemtype,
                              const std::string &suffix = "");
std::string getHelpString(const std::string &topic);
std::string getMiscString(const std::string &misc,
                          const std::string &suffix = "");

std::vector<std::string> getAllFAQKeys(void);
std::string getFAQ_Question(const std::string &key);
std::string getFAQ_Answer(const std::string &question);
#endif
