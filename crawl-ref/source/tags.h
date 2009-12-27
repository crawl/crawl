/*
 *  File:       tags.h
 *  Summary:    Auxilary functions to make savefile versioning simpler.
 *  Written by: Gordon Lipford
 */

#ifndef TAGS_H
#define TAGS_H

#include <cstdio>
#include <stdint.h>

struct show_type;

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
    TAG_MAJOR_VERSION = 11
};

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_RESET     = 0, // Minor tags were reset
    TAG_MINOR_HEIGHTMAP = 1,
    TAG_MINOR_VERSION   = 1  // Current version.  (Keep equal to max.)
};

struct enum_info
{
    void (*collect)(std::vector<std::pair<int,std::string> >& prs);
    int replacement;

    struct enum_val
    {
        int value;
        const char *name;
    };

    const enum_val *historical;
    tag_minor_version non_historical_first;
    char historic_bytes;
};

struct enum_write_state
{
    std::set<int> used;
    std::map<int, std::string> names;
    char store_type;

    enum_write_state() : used(), names(), store_type(0) {}
};

struct enum_read_state
{
    std::map<int, int> mapping;
    std::map<std::string, int> names;
    char store_type;

    enum_read_state() : mapping(), names(), store_type(0) {}
};

template<typename enm> struct enum_details;

// TO ADD A NEW ENUM UNDER THE UMBRELLA OF marshallEnum:
// * Create an enum_info instance
// * Instanciate the enum_details template
// * Change existing serialization to use marshallEnum
// * Bump minor version

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

    std::map<const enum_info*, enum_write_state> used_enums;
    friend void marshallEnumVal(writer&, const enum_info*, int);
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
void marshallMonster (writer &, const monsters &);
void marshallShowtype (writer &, const show_type &);

void marshallEnumVal (writer &, const enum_info *, int);

template<typename enm>
inline void marshallEnum(writer& wr, enm value)
{
    marshallEnumVal(wr, &enum_details<enm>::desc, static_cast<int>(value));
}

/* ***********************************************************************
 * reader API
 * *********************************************************************** */

class reader
{
public:
    reader(FILE* input, char minorVersion = TAG_MINOR_VERSION)
        : _file(input), _pbuf(0), _read_offset(0),
          _minorVersion(minorVersion) {}
    reader(const std::vector<unsigned char>& input,
           char minorVersion = TAG_MINOR_VERSION)
        : _file(0), _pbuf(&input), _read_offset(0),
          _minorVersion(minorVersion) {}

    unsigned char readByte();
    void read(void *data, size_t size);
    char getMinorVersion();

private:
    FILE* _file;
    const std::vector<unsigned char>* _pbuf;
    unsigned int _read_offset;
    char _minorVersion;

    std::map<const enum_info*, enum_read_state> seen_enums;
    friend int unmarshallEnumVal(reader &, const enum_info *);
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
void        unmarshallMonster (reader &, monsters &item);
show_type   unmarshallShowtype (reader &);

int         unmarshallEnumVal (reader &, const enum_info *);

template<typename enm>
inline enm unmarshallEnum(writer& wr)
{
    return static_cast<enm>(unmarshallEnumVal(wr, &enum_details<enm>::desc));
}

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
