/**
 * @file
 * @brief Auxiliary functions to make savefile versioning simpler.
**/

#ifndef TAGS_H
#define TAGS_H

#include <cstdio>

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

/* ***********************************************************************
 * writer API
 * *********************************************************************** */

class writer
{
public:
    writer(const string &filename, FILE* output, bool ignore_errors = false)
        : _filename(filename), _file(output), _chunk(0),
          _ignore_errors(ignore_errors), _pbuf(0), failed(false)
    {
        ASSERT(output);
    }
    writer(vector<unsigned char>* poutput)
        : _filename(), _file(0), _chunk(0), _ignore_errors(false),
          _pbuf(poutput), failed(false) { ASSERT(poutput); }
    writer(package *save, const string &chunkname)
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
    string _filename;
    FILE* _file;
    chunk_writer *_chunk;
    bool _ignore_errors;

    vector<unsigned char>* _pbuf;

    bool failed;
};

void marshallByte    (writer &, int8_t);
void marshallShort   (writer &, int16_t);
void marshallInt     (writer &, int32_t);
void marshallFloat   (writer &, float);
void marshallUByte   (writer &, uint8_t);
void marshallBoolean (writer &, bool);
void marshallString  (writer &, const string &);
void marshallString4 (writer &, const string &);
void marshallCoord   (writer &, const coord_def &);
void marshallItem    (writer &, const item_def &, bool info = false);
void marshallMonster (writer &, const monster&);
void marshall_level_id(writer& th, const level_id& id);
void marshallUnsigned(writer& th, uint64_t v);
void marshallSigned(writer& th, int64_t v);

/* ***********************************************************************
 * reader API
 * *********************************************************************** */

class reader
{
public:
    reader(const string &filename, int minorVersion = TAG_MINOR_INVALID);
    reader(FILE* input, int minorVersion = TAG_MINOR_INVALID)
        : _file(input), _chunk(0), opened_file(false), _pbuf(0),
          _read_offset(0), _minorVersion(minorVersion) {}
    reader(const vector<unsigned char>& input,
           int minorVersion = TAG_MINOR_INVALID)
        : _file(0), _chunk(0), opened_file(false), _pbuf(&input),
          _read_offset(0), _minorVersion(minorVersion) {}
    reader(package *save, const string &chunkname,
           int minorVersion = TAG_MINOR_INVALID);
    ~reader();

    unsigned char readByte();
    void read(void *data, size_t size);
    void advance(size_t size);
    int getMinorVersion() const;
    void setMinorVersion(int minorVersion);
    bool valid() const;
    void fail_if_not_eof(const string &name);
    void close();

    string filename() const { return _filename; }

private:
    string _filename;
    FILE* _file;
    chunk_reader *_chunk;
    bool  opened_file;
    const vector<unsigned char>* _pbuf;
    unsigned int _read_offset;
    int _minorVersion;
};

class short_read_exception : exception {};

int8_t      unmarshallByte    (reader &);
int16_t     unmarshallShort   (reader &);
int32_t     unmarshallInt     (reader &);
float       unmarshallFloat   (reader &);
uint8_t     unmarshallUByte   (reader &);
bool        unmarshallBoolean (reader &);
string      unmarshallString  (reader &);
void        unmarshallString4 (reader &, string&);
coord_def   unmarshallCoord   (reader &);
void        unmarshallItem    (reader &, item_def &item);
void        unmarshallMonster (reader &, monster& item);
level_id    unmarshall_level_id(reader& th);

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

void tag_read(reader &inf, tag_type tag_id);
void tag_write(tag_type tagID, writer &outf);
void tag_read_char(reader &th, uint8_t format, uint8_t major, uint8_t minor);

/* ***********************************************************************
 * misc
 * *********************************************************************** */

string make_date_string(time_t in_date);

#endif // TAGS_H
