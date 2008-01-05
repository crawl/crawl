/*
 *  File:       tags.h
 *  Summary:    Auxilary functions to make savefile versioning simpler.
 *  Written by: Gordon Lipford
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <1>   27 Jan 2001      GDL    Created
 */

#ifndef TAGS_H
#define TAGS_H

#include <stdio.h>
#include "externs.h"

enum tag_type   // used during save/load process to identify data blocks
{
    TAG_VERSION = 0,                    // should NEVER be read in!
    TAG_YOU = 1,                        // 'you' structure
    TAG_YOU_ITEMS,                      // your items
    TAG_YOU_DUNGEON,                    // dungeon specs (stairs, branches, features)
    TAG_LEVEL,                          // various grids & clouds
    TAG_LEVEL_ITEMS,                    // items/traps
    TAG_LEVEL_MONSTERS,                 // monsters
    TAG_GHOST,                          // ghost
    TAG_LEVEL_ATTITUDE,                 // monster attitudes
    TAG_LOST_MONSTERS,                  // monsters in transit
    TAG_LEVEL_TILES,
    NUM_TAGS
};

enum tag_file_type   // file types supported by tag system
{
    TAGTYPE_PLAYER=0,           // Foo.sav
    TAGTYPE_LEVEL,              // Foo.00a, .01a, etc.
    TAGTYPE_GHOST,              // bones.xxx

    TAGTYPE_PLAYER_NAME         // Used only to read the player name
};

struct tagHeader 
{
    short tagID;
    long offset;

    // File handle for direct file writes.
    FILE *file;

    tagHeader() : tagID(0), offset(0L), file(NULL) { }
    tagHeader(FILE *f) : tagID(0), offset(0L), file(f) { }
    unsigned char readByte();
    void writeByte(unsigned char byte);
    void write(const void *data, size_t size);
    void read(void *data, size_t size);
    void advance(int skip);
};

// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files tags
 * *********************************************************************** */
int write2(FILE * file, const char *buffer, unsigned int count);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files tags
 * *********************************************************************** */
int read2(FILE * file, char *buffer, unsigned int count);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files tags
 * *********************************************************************** */
void marshallByte(tagHeader &th, char data);
void marshallShort(tagHeader &th, short data);
void marshallLong(tagHeader &th, long data);
void marshallFloat(tagHeader &th, float data);
void marshallBoolean(tagHeader &th, bool data);
void marshallString(tagHeader &th, const std::string &data,
                    int maxSize = 0);
void marshallCoord(tagHeader &th, const coord_def &c);
void marshallItem(tagHeader &th, const item_def &item);

// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: tags files
 * *********************************************************************** */
char unmarshallByte(tagHeader &th);
short unmarshallShort(tagHeader &th);
long unmarshallLong(tagHeader &th);
float unmarshallFloat(tagHeader &th);
bool unmarshallBoolean(tagHeader &th);
int unmarshallCString(tagHeader &th, char *data, int maxSize);
std::string unmarshallString(tagHeader &th, int maxSize = 1000);
void unmarshallCoord(tagHeader &th, coord_def &c);
void unmarshallItem(tagHeader &th, item_def &item);

std::string make_date_string( time_t in_date );
time_t parse_date_string( char[20] );

// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void tag_init(long largest_tag = 100000);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void tag_construct(tagHeader &th, int i);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void tag_write(tagHeader &th, FILE *saveFile);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void tag_set_expected(char tags[], int fileType);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void tag_missing(int tag, char minorVersion);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
int tag_read(FILE *fp, char minorVersion);

#endif // TAGS_H
