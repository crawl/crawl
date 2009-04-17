/*
 *  File:       tags.h
 *  Summary:    Auxilary functions to make savefile versioning simpler.
 *  Written by: Gordon Lipford
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef TAGS_H
#define TAGS_H

#include <cstdio>
#include <stdint.h>
#include "externs.h"

enum tag_type   // used during save/load process to identify data blocks
{
    TAG_NO_TAG = 0,                     // should NEVER be read in!
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
    TAGTYPE_PLAYER = 0,         // Foo.sav
    TAGTYPE_LEVEL,              // Foo.00a, .01a, etc.
    TAGTYPE_GHOST,              // bones.xxx

    TAGTYPE_PLAYER_NAME         // Used only to read the player name
};

enum tag_major_version
{
    TAG_MAJOR_START   = 5,
    TAG_MAJOR_VERSION = 5
};

// Minor version will be reset to zero when major version changes.
// Tags are checked with >=, so a minor version of 11 would logically include
// the dungeon Lua changes.
enum tag_minor_version
{
    TAG_MINOR_PIETY    =  2,     // Added piety_hysteresis
    TAG_MINOR_QUIVER   =  3,     // Added quiver
    TAG_MINOR_MAPMARK  =  4,     // Added sizes to map markers
    TAG_MINOR_MONNAM   =  5,     // Monsters get individual names
    TAG_MINOR_MONBASE  =  6,     // Zombie base monster gets its own field.
    TAG_MINOR_FPICKUP  =  7,     // Added pickup option for allied monsters.
    TAG_MINOR_MPATROL  =  8,     // Added monster patrol points.
    TAG_MINOR_PATHFIND =  9,     // Added monster pathfinding.
    TAG_MINOR_TRTARGET = 10,     // Added travel target.
    TAG_MINOR_LUADGN   = 11,     // Allow dungeon Lua to persist data.
    // Bump version past 12.
    TAG_MINOR_SVNREV   = 13,     // Added SVN revision
    TAG_MINOR_PORTEXT  = 14,     // Keep track of portal vault extensions.
    TAG_MINOR_RELIGION = 15,     // Keep track of ghosts' religions.
    TAG_MINOR_VERSION  = 15      // Current version.  (Keep equal to max.)
};


/* ***********************************************************************
 * writer API
 * *********************************************************************** */

class writer
{
public:
    writer(FILE* output)
        : _file(output), _pbuf(0) {}
    writer(std::vector<unsigned char>* poutput)
        : _file(0), _pbuf(poutput) {}

    void writeByte(unsigned char byte);
    void write(const void *data, size_t size);
    long tell();

private:
    FILE* _file;
    std::vector<unsigned char>* _pbuf;
};

void marshallByte    (writer &, const char& );
void marshallShort   (writer &, int16_t );
void marshallLong    (writer &, int32_t );
void marshallFloat   (writer &, float );
void marshallBoolean (writer &, bool );
void marshallString  (writer &, const std::string &, int maxSize = 0);
void marshallString4 (writer &, const std::string &);
void marshallCoord   (writer &, const coord_def &);
void marshallItem    (writer &, const item_def &);

/* ***********************************************************************
 * reader API
 * *********************************************************************** */

class reader
{
public:
    reader(FILE* input)
        : _file(input), _pbuf(0), _read_offset(0) {}
    reader(const std::vector<unsigned char>& input)
        : _file(0), _pbuf(&input), _read_offset(0) {}

    unsigned char readByte();
    void read(void *data, size_t size);

private:
    FILE* _file;
    const std::vector<unsigned char>* _pbuf;
    unsigned int _read_offset;
};

char        unmarshallByte    (reader &);
int16_t     unmarshallShort   (reader &);
int32_t     unmarshallLong    (reader &);
float       unmarshallFloat   (reader &);
bool        unmarshallBoolean (reader &);
int         unmarshallCString (reader &, char *data, int maxSize);
std::string unmarshallString  (reader &, int maxSize = 1000);
void        unmarshallString4 (reader &, std::string&);
void        unmarshallCoord   (reader &, coord_def &c);
void        unmarshallItem    (reader &, item_def &item);

/* ***********************************************************************
 * Tag interface
 * *********************************************************************** */

tag_type tag_read(FILE* inf, char minorVersion);
void tag_write(tag_type tagID, FILE* outf);
void tag_set_expected(char tags[], int fileType);
void tag_missing(int tag, char minorVersion);

/* ***********************************************************************
 * misc
 * *********************************************************************** */

int write2(FILE * file, const void *buffer, unsigned int count);
int read2(FILE * file, void *buffer, unsigned int count);
std::string make_date_string( time_t in_date );
time_t parse_date_string( char[20] );

#endif // TAGS_H
