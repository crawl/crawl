/*
 *  File:       tags.h
 *  Summary:    Auxilary functions to make savefile versioning simpler.
 *  Written by: Gordon Lipford
 */

#ifndef TAGS_H
#define TAGS_H

#include <cstdio>
#include <stdint.h>

#include "tag-version.h"
#include "package.h"

struct show_type;
struct monster_info;
struct map_cell;

enum tag_type   // used during save/load process to identify data blocks
{
    TAG_NO_TAG = 0,                     // should NEVER be read in!
    TAG_CHR = 1,                        // basic char info
    TAG_YOU,                            // the main part of the save
    TAG_LEVEL,                          // a single dungeon level
    TAG_GHOST,                          // ghost
    NUM_TAGS,

    // Returned when a known tag was deliberately not read. This value is
    // never saved and can safely be changed at any point.
    TAG_SKIP
};

enum tag_file_type   // file types supported by tag system
{
    TAGTYPE_PLAYER = 0,         // Foo.sav
    TAGTYPE_LEVEL,              // Foo.00a, .01a, etc.
    TAGTYPE_GHOST,              // bones.xxx

    TAGTYPE_PLAYER_NAME,        // Used only to read the player name
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
    writer(const std::string &filename, FILE* output,
           bool ignore_errors = false)
        : _filename(filename), _file(output), _chunk(0),
          _ignore_errors(ignore_errors), _pbuf(0), failed(false)
    {
        ASSERT(output);
    }
    writer(std::vector<unsigned char>* poutput)
        : _filename(), _file(0), _chunk(0), _ignore_errors(false),
          _pbuf(poutput), failed(false) { ASSERT(poutput); }
    writer(package *save, const std::string &chunkname)
        : _filename(), _file(0), _chunk(0), _ignore_errors(false),
          failed(false)
    {
        ASSERT(save);
        _chunk = save->writer(chunkname);
    }

    ~writer() { if (_chunk) delete _chunk; }

    void writeByte(unsigned char byte);
    void write(const void *data, size_t size);
    long tell();

    bool succeeded() const { return !failed; }

private:
    void check_ok(bool ok);

private:
    std::string _filename;
    FILE* _file;
    chunk_writer *_chunk;
    bool _ignore_errors;

    std::vector<unsigned char>* _pbuf;

    bool failed;

    std::map<const enum_info*, enum_write_state> used_enums;
    friend void marshallEnumVal(writer&, const enum_info*, int);
};

void marshallByte    (writer &, int8_t );
void marshallShort   (writer &, int16_t );
void marshallInt     (writer &, int32_t );
void marshallFloat   (writer &, float );
void marshallUByte   (writer &, uint8_t );
void marshallBoolean (writer &, bool );
void marshallString  (writer &, const std::string &, int maxSize = 0);
void marshallString4 (writer &, const std::string &);
void marshallCoord   (writer &, const coord_def &);
void marshallItem    (writer &, const item_def &);
void marshallMonster (writer &, const monsters &);
void marshallMonsterInfo (writer &, const monster_info &);
void marshallMapCell (writer &, const map_cell &);

void marshallEnumVal (writer &, const enum_info *, int);

template<typename enm>
inline void marshallEnum(writer& wr, enm value)
{
    marshallEnumVal(wr, &enum_details<enm>::desc, static_cast<int>(value));
}

void marshallUnsigned(writer& th, uint64_t v);
void marshallSigned(writer& th, int64_t v);

/* ***********************************************************************
 * reader API
 * *********************************************************************** */

class reader
{
public:
    reader(const std::string &filename, int minorVersion = TAG_MINOR_VERSION);
    reader(FILE* input, int minorVersion = TAG_MINOR_VERSION)
        : _file(input), _chunk(0), opened_file(false), _pbuf(0),
          _read_offset(0), _minorVersion(minorVersion) {}
    reader(const std::vector<unsigned char>& input,
           int minorVersion = TAG_MINOR_VERSION)
        : _file(0), _chunk(0), opened_file(false), _pbuf(&input),
          _read_offset(0), _minorVersion(minorVersion) {}
    reader(package *save, const std::string &chunkname,
           int minorVersion = TAG_MINOR_VERSION);
    ~reader();

    unsigned char readByte();
    void read(void *data, size_t size);
    void advance(size_t size);
    int getMinorVersion();
    bool valid() const;
    void fail_if_not_eof(const std::string name);

private:
    FILE* _file;
    chunk_reader *_chunk;
    bool  opened_file;
    const std::vector<unsigned char>* _pbuf;
    unsigned int _read_offset;
    int _minorVersion;

    std::map<const enum_info*, enum_read_state> seen_enums;
    friend int unmarshallEnumVal(reader &, const enum_info *);
};

class short_read_exception : std::exception {};

int8_t      unmarshallByte    (reader &);
int16_t     unmarshallShort   (reader &);
int32_t     unmarshallInt    (reader &);
float       unmarshallFloat   (reader &);
uint8_t     unmarshallUByte   (reader &);
bool        unmarshallBoolean (reader &);
std::string unmarshallString  (reader &, int maxSize = 1000);
void        unmarshallString4 (reader &, std::string&);
coord_def   unmarshallCoord   (reader &);
void        unmarshallItem    (reader &, item_def &item);
void        unmarshallMonster (reader &, monsters &item);
void        unmarshallMonsterInfo (reader &, monster_info &mi);
void        unmarshallMapCell (reader &, map_cell& cell);

int         unmarshallEnumVal (reader &, const enum_info *);

template<typename enm>
inline enm unmarshallEnum(writer& wr)
{
    return static_cast<enm>(unmarshallEnumVal(wr, &enum_details<enm>::desc));
}

uint64_t unmarshallUnsigned(reader& th);
template<typename T>
static inline void unmarshallUnsigned(reader& th, T& v)
{
    v = (T)unmarshallUnsigned(th);
}

int64_t unmarshallSigned(reader& th);
template<typename T>
static inline void unmarshallSigned(reader& th, T& v)
{
    v = (T)unmarshallSigned(th);
}

/* ***********************************************************************
 * Tag interface
 * *********************************************************************** */

tag_type tag_read(reader &inf, int minorVersion, int8_t expected_tags[NUM_TAGS]);
void tag_write(tag_type tagID, writer &outf);
void tag_set_expected(int8_t tags[], int fileType);
void tag_missing(int tag);

/* ***********************************************************************
 * misc
 * *********************************************************************** */

int write2(FILE * file, const void *buffer, unsigned int count);
int read2(FILE * file, void *buffer, unsigned int count);
std::string make_date_string( time_t in_date );
time_t parse_date_string( char[20] );

#endif // TAGS_H
