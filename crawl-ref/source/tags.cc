/*
 *  File:       tags.cc
 *  Summary:    Auxilary functions to make savefile versioning simpler.
 *  Written by: Gordon Lipford
 */

/*
   The marshalling and unmarshalling of data is done in big endian and
   is meant to keep savefiles cross-platform.  Note also that the marshalling
   sizes are 1, 2, and 4 for byte, short, and int.  If a strange platform
   with different sizes of these basic types pops up, please sed it to fixed-
   width ones.  For now, that wasn't done in order to keep things convenient.
*/

#include "AppHdr.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>            // for memcpy
#include <iterator>
#include <algorithm>

#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "artefact.h"
#include "branch.h"
#include "coord.h"
#include "coordit.h"
#include "describe.h"
#include "dungeon.h"
#include "enum.h"
#include "errors.h"
#include "map_knowledge.h"
#include "externs.h"
#include "files.h"
#include "ghost.h"
#include "itemname.h"
#include "libutil.h"
#include "mapmark.h"
#include "mon-info.h"
#include "mon-util.h"
#include "mon-transit.h"
#include "quiver.h"
#include "skills.h"
#include "skills2.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "syscalls.h"
#include "tags.h"
#ifdef USE_TILE
 #include "tilemcache.h"
 #include "tilepick.h"
 #include "tileview.h"
 #include "showsymb.h"
#endif
#include "travel.h"

// defined in dgn-overview.cc
extern std::map<branch_type, level_id> stair_level;
extern std::map<level_pos, shop_type> shops_present;
extern std::map<level_pos, god_type> altars_present;
extern std::map<level_pos, portal_type> portals_present;
extern std::map<level_pos, std::string> portal_vaults_present;
extern std::map<level_pos, std::string> portal_vault_notes;
extern std::map<level_pos, uint8_t> portal_vault_colours;
extern std::map<level_id, std::string> level_annotations;
extern std::map<level_id, std::string> level_exclusions;

// temp file pairs used for file level cleanup

level_id_set Generated_Levels;

// The minor version for the tag currently being read.
static int _tag_minor_version = -1;

reader::reader(const std::string &filename, int minorVersion)
    : _chunk(0), _pbuf(NULL), _read_offset(0),
      _minorVersion(minorVersion), seen_enums()
{
    _file       = fopen_u(filename.c_str(), "rb");
    opened_file = !!_file;
}

reader::reader(package *save, const std::string &chunkname, int minorVersion)
    : _file(0), _chunk(0), opened_file(false), _pbuf(0), _read_offset(0),
     _minorVersion(minorVersion)
{
    ASSERT(save);
    _chunk = new chunk_reader(save, chunkname);
}

reader::~reader()
{
    if (_chunk)
        delete _chunk;
    if (opened_file && _file)
        fclose(_file);
}

void reader::advance(size_t offset)
{
    read(NULL, offset);
}

bool reader::valid() const
{
    return ((_file && !feof(_file)) ||
            (_pbuf && _read_offset < _pbuf->size()));
}

// Reads input in network byte order, from a file or buffer.
unsigned char reader::readByte()
{
    if (_file)
    {
        int b = fgetc(_file);
        if (b == EOF)
            throw short_read_exception();
        return b;
    }
    else if (_chunk)
    {
        unsigned char buf;
        if (_chunk->read(&buf, 1) != 1)
            throw short_read_exception();
        return buf;
    }
    else
    {
        if (_read_offset >= _pbuf->size())
            throw short_read_exception();
        return (*_pbuf)[_read_offset++];
    }
}

void reader::read(void *data, size_t size)
{
    if (_file)
    {
        if (data)
        {
            if (fread(data, 1, size, _file) != size)
                throw short_read_exception();
        }
        else
            fseek(_file, (long)size, SEEK_CUR);
    }
    else if (_chunk)
    {
        if (_chunk->read(data, size) != size)
            throw short_read_exception();
    }
    else
    {
        ASSERT(_read_offset+size <= _pbuf->size());
        if (data)
            memcpy(data, &(*_pbuf)[_read_offset], size);

        _read_offset += size;
    }
}

int reader::getMinorVersion()
{
    return _minorVersion;
}

void reader::fail_if_not_eof(const std::string name)
{
    char dummy;
    if (_chunk ? _chunk->read(&dummy, 1) :
        _file ? (fgetc(_file) != EOF) :
        _read_offset >= _pbuf->size())
    {
        fail(("Incomplete read of \"" + name + "\" - aborting.").c_str());
    }
}

void writer::check_ok(bool ok)
{
    if (!ok && !failed)
    {
        failed = true;
        if (!_ignore_errors)
            end(1, true, "Error writing to %s", _filename.c_str());
    }
}

void writer::writeByte(unsigned char ch)
{
    if (failed)
        return;

    if (_chunk)
        _chunk->write(&ch, 1);
    else if (_file)
        check_ok(fputc(ch, _file) != EOF);
    else
        _pbuf->push_back(ch);
}

void writer::write(const void *data, size_t size)
{
    if (failed)
        return;

    if (_chunk)
        _chunk->write(data, size);
    else if (_file)
        check_ok(fwrite(data, 1, size, _file) == size);
    else
    {
        const unsigned char* cdata = static_cast<const unsigned char*>(data);
        _pbuf->insert(_pbuf->end(), cdata, cdata+size);
    }
}

long writer::tell()
{
    ASSERT(!_chunk);
    return _file? ftell(_file) : _pbuf->size();
}


#ifdef DEBUG_GLOBALS
// Force a conditional jump valgrind may pick up, no matter the optimizations.
static volatile uint32_t hashroll;
static void CHECK_INITIALIZED(uint32_t x)
{
    hashroll = 0;
    if ((hashroll += x) & 1)
        hashroll += 2;
}
#else
#define CHECK_INITIALIZED(x)
#endif

// static helpers
static void tag_construct_char(writer &th);
static void tag_construct_you(writer &th);
static void tag_construct_you_items(writer &th);
static void tag_construct_you_dungeon(writer &th);
static void tag_construct_lost_monsters(writer &th);
static void tag_construct_lost_items(writer &th);
static void tag_construct_game_state(writer &th);
static void tag_read_char(reader &th, int minorVersion);
static void tag_read_you(reader &th, int minorVersion);
static void tag_read_you_items(reader &th, int minorVersion);
static void tag_read_you_dungeon(reader &th, int minorVersion);
static void tag_read_lost_monsters(reader &th);
static void tag_read_lost_items(reader &th);
static void tag_read_game_state(reader &th);

static void tag_construct_level(writer &th);
static void tag_construct_level_items(writer &th);
static void tag_construct_level_monsters(writer &th);
static void tag_construct_level_tiles(writer &th);
static void tag_read_level(reader &th, int minorVersion);
static void tag_read_level_items(reader &th, int minorVersion);
static void tag_read_level_monsters(reader &th, int minorVersion);
static void tag_read_level_tiles(reader &th);
static void tag_missing_level_tiles();

static void tag_construct_ghost(writer &th);
static void tag_read_ghost(reader &th, int minorVersion);

static void marshallGhost(writer &th, const ghost_demon &ghost);
static ghost_demon unmarshallGhost(reader &th, int minorVersion);

static void marshallResists(writer &th, const mon_resist_def &res);
static void unmarshallResists(reader &th, mon_resist_def &res,
                              int minorVersion);

static void marshallSpells(writer &, const monster_spells &);
static void unmarshallSpells(reader &, monster_spells &);

template<typename T, typename T_iter, typename T_marshal>
static void marshall_iterator(writer &th, T_iter beg, T_iter end,
                              T_marshal marshal);
template<typename T>
static void unmarshall_vector(reader& th, std::vector<T>& vec,
                              T (*T_unmarshall)(reader&));


void marshallByte(writer &th, int8_t data)
{
    CHECK_INITIALIZED(data);
    th.writeByte(data);
}

int8_t unmarshallByte(reader &th)
{
    return th.readByte();
}

void marshallUByte(writer &th, uint8_t data)
{
    CHECK_INITIALIZED(data);
    th.writeByte(data);
}

uint8_t unmarshallUByte(reader &th)
{
    return th.readByte();
}

// A hack to work around a template breakage.  It accepts shorts and ints
// directly, but chars need to be passed as reference -- but in just one place.
// Perhaps someone could understand this nonsense, but not me -- 1KB
static void marshallUByteRef(writer &th, const uint8_t &data)
{
    CHECK_INITIALIZED(data);
    th.writeByte(data);
}

void marshallShort(std::vector<unsigned char>& buf, short data)
{
    CHECK_INITIALIZED(data);
    COMPILE_CHECK(sizeof(data) == 2, c1);
    buf.push_back((unsigned char) ((data & 0xFF00) >> 8));
    buf.push_back((unsigned char) ((data & 0x00FF)   ));
}

// Marshall 2 byte short in network order.
void marshallShort(writer &th, short data)
{
    CHECK_INITIALIZED(data);
    const char b2 = (char)(data & 0x00FF);
    const char b1 = (char)((data & 0xFF00) >> 8);
    th.writeByte(b1);
    th.writeByte(b2);
}

// Unmarshall 2 byte short in network order.
int16_t unmarshallShort(reader &th)
{
    int16_t b1 = th.readByte();
    int16_t b2 = th.readByte();
    int16_t data = (b1 << 8) | (b2 & 0x00FF);
    return data;
}

void marshallInt(std::vector<unsigned char>& buf, int32_t data)
{
    CHECK_INITIALIZED(data);
    buf.push_back((unsigned char) ((data & 0xFF000000) >> 24));
    buf.push_back((unsigned char) ((data & 0x00FF0000) >> 16));
    buf.push_back((unsigned char) ((data & 0x0000FF00) >>  8));
    buf.push_back((unsigned char) ((data & 0x000000FF)    ));
}

// Marshall 4 byte int in network order.
void marshallInt(writer &th, int32_t data)
{
    CHECK_INITIALIZED(data);
    char b4 = (char) (data & 0x000000FF);
    char b3 = (char)((data & 0x0000FF00) >> 8);
    char b2 = (char)((data & 0x00FF0000) >> 16);
    char b1 = (char)((data & 0xFF000000) >> 24);

    th.writeByte(b1);
    th.writeByte(b2);
    th.writeByte(b3);
    th.writeByte(b4);
}

// Unmarshall 4 byte signed int in network order.
int32_t unmarshallInt(reader &th)
{
    int32_t b1 = th.readByte();
    int32_t b2 = th.readByte();
    int32_t b3 = th.readByte();
    int32_t b4 = th.readByte();

    int32_t data = (b1 << 24) | ((b2 & 0x000000FF) << 16);
    data |= ((b3 & 0x000000FF) << 8) | (b4 & 0x000000FF);
    return data;
}

void marshallUnsigned(writer& th, uint64_t v)
{
    do
    {
        unsigned char b = (unsigned char)(v & 0x7f);
        v >>= 7;
        if (v)
            b |= 0x80;
        th.writeByte(b);
    }
    while (v);
}

uint64_t unmarshallUnsigned(reader& th)
{
    unsigned i = 0;
    uint64_t v = 0;
    for (;;)
    {
        unsigned char b = th.readByte();
        v |= (uint64_t)(b & 0x7f) << i;
        i += 7;
        if (!(b & 0x80))
            break;
    }
    return v;
}

void marshallSigned(writer& th, int64_t v)
{
    if (v < 0)
        marshallUnsigned(th, (uint64_t)((-v - 1) << 1) | 1);
    else
        marshallUnsigned(th, (uint64_t)(v << 1));
}

int64_t unmarshallSigned(reader& th)
{
    uint64_t u;
    unmarshallUnsigned(th, u);
    if (u & 1)
        return (int64_t)(-(u >> 1) - 1);
    else
        return (int64_t)(u >> 1);
}

// FIXME: Kill this abomination - it will break!
template<typename T>
static void _marshall_as_int(writer& th, const T& t)
{
    marshallInt(th, static_cast<int>(t));
}

template <typename data>
void marshallSet(writer &th, const std::set<data> &s,
                 void (*marshall)(writer &, const data &))
{
    marshallInt(th, s.size());
    typename std::set<data>::const_iterator i = s.begin();
    for (; i != s.end(); ++i)
        marshall(th, *i);
}

template<typename key, typename value>
void marshallMap(writer &th, const std::map<key,value>& data,
                 void (*key_marshall)(writer&, const key&),
                 void (*value_marshall)(writer&, const value&))
{
    marshallInt(th, data.size());
    typename std::map<key,value>::const_iterator ci;
    for (ci = data.begin(); ci != data.end(); ++ci)
    {
        key_marshall(th, ci->first);
        value_marshall(th, ci->second);
    }
}

template<typename T_iter, typename T_marshall_t>
static void marshall_iterator(writer &th, T_iter beg, T_iter end,
                              T_marshall_t T_marshall)
{
    marshallInt(th, std::distance(beg, end));
    while (beg != end)
    {
        T_marshall(th, *beg);
        ++beg;
    }
}

template<typename T>
static void unmarshall_vector(reader& th, std::vector<T>& vec,
                              T (*T_unmarshall)(reader&))
{
    vec.clear();
    const int num_to_read = unmarshallInt(th);
    for (int i = 0; i < num_to_read; ++i)
        vec.push_back(T_unmarshall(th));
}

template <typename T_container, typename T_inserter, typename T_unmarshall>
static void unmarshall_container(reader &th, T_container &container,
                                 T_inserter inserter, T_unmarshall unmarshal)
{
    container.clear();
    const int num_to_read = unmarshallInt(th);
    for (int i = 0; i < num_to_read; ++i)
        (container.*inserter)(unmarshal(th));
}

// XXX: Redundant with level_id.save()/load().
void marshall_level_id(writer& th, const level_id& id)
{
    marshallByte(th, id.branch);
    marshallInt(th, id.depth);
    marshallByte(th, id.level_type);
}

// XXX: Redundant with level_pos.save()/load().
void marshall_level_pos(writer& th, const level_pos& lpos)
{
    marshallInt(th, lpos.pos.x);
    marshallInt(th, lpos.pos.y);
    marshall_level_id(th, lpos.id);
}

template <typename data, typename set>
void unmarshallSet(reader &th, set &dset,
                   data (*data_unmarshall)(reader &))
{
    dset.clear();
    int len = unmarshallInt(th);
    for (int i = 0; i < len; ++i)
        dset.insert(data_unmarshall(th));
}

template<typename key, typename value, typename map>
void unmarshallMap(reader& th, map& data,
                   key   (*key_unmarshall)  (reader&),
                   value (*value_unmarshall)(reader&))
{
    int i, len = unmarshallInt(th);
    key k;
    for (i = 0; i < len; ++i)
    {
        k = key_unmarshall(th);
        std::pair<key, value> p(k, value_unmarshall(th));
        data.insert(p);
    }
}

template<typename T>
T unmarshall_long_as(reader& th)
{
    return static_cast<T>(unmarshallInt(th));
}

level_id unmarshall_level_id(reader& th)
{
    level_id id;
    id.branch     = static_cast<branch_type>(unmarshallByte(th));
    id.depth      = unmarshallInt(th);
    id.level_type = static_cast<level_area_type>(unmarshallByte(th));
    return (id);
}

level_pos unmarshall_level_pos(reader& th)
{
    level_pos lpos;
    lpos.pos.x = unmarshallInt(th);
    lpos.pos.y = unmarshallInt(th);
    lpos.id    = unmarshall_level_id(th);
    return lpos;
}

void marshallCoord(writer &th, const coord_def &c)
{
    marshallShort(th, c.x);
    marshallShort(th, c.y);
}

coord_def unmarshallCoord(reader &th)
{
    coord_def c;
    c.x = unmarshallShort(th);
    c.y = unmarshallShort(th);
    return (c);
}

template <typename marshall, typename grid>
static void _run_length_encode(writer &th, marshall m, const grid &g,
                               int width, int height)
{
    int last = 0, nlast = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            if (!nlast)
                last = g[x][y];
            if (last == g[x][y] && nlast < 255)
            {
                nlast++;
                continue;
            }

            marshallByte(th, nlast);
            m(th, last);

            last = g[x][y];
            nlast = 1;
        }

    marshallByte(th, nlast);
    m(th, last);
}

template <typename unmarshall, typename grid>
static void _run_length_decode(reader &th, unmarshall um, grid &g,
                               int width, int height)
{
    const int end = width * height;
    int offset = 0;
    while (offset < end)
    {
        const int run = unmarshallUByte(th);
        const int value = um(th);

        for (int i = 0; i < run; ++i)
        {
            const int y = offset / width;
            const int x = offset % width;
            g[x][y] = value;
            ++offset;
        }
    }
}

union float_marshall_kludge
{
    // [ds] Does ANSI C guarantee that sizeof(float) == sizeof(long)?
    // [haranp] no, unfortunately
    // [1KB] on 64 bit arches, long is 64 bits, while float is 32 bits.
    float    f_num;
    int32_t  l_num;
};

// single precision float -- marshall in network order.
void marshallFloat(writer &th, float data)
{
    float_marshall_kludge k;
    k.f_num = data;
    marshallInt(th, k.l_num);
}

// single precision float -- unmarshall in network order.
float unmarshallFloat(reader &th)
{
    float_marshall_kludge k;
    k.l_num = unmarshallInt(th);
    return k.f_num;
}

// string -- 2 byte length, string data
void marshallString(writer &th, const std::string &data, int maxSize)
{
    // allow for very long strings (well, up to 32K).
    int len = data.length();
    if (maxSize > 0 && len > maxSize)
        len = maxSize;
    marshallShort(th, len);

    // put in the actual string -- we'll null terminate on
    // unmarshall.
    th.write(data.c_str(), len);
}

// To pass to marsahllMap
static void marshallStringNoMax(writer &th, const std::string &data)
{
    marshallString(th, data);
}

// string -- unmarshall length & string data
static int unmarshallCString(reader &th, char *data, int maxSize)
{
    ASSERT(maxSize > 0);

    // Get length.
    short len = unmarshallShort(th);
    int copylen = len;

    if (len >= maxSize)
        copylen = maxSize - 1;

    // Read the actual string and null terminate.
    th.read(data, copylen);
    data[copylen] = 0;

    th.read(NULL, len - copylen);
    return (copylen);
}

std::string unmarshallString(reader &th, int maxSize)
{
    if (maxSize <= 0)
        return ("");
    char *buffer = new char [maxSize];
    if (!buffer)
        return ("");
    *buffer = 0;
    const int slen = unmarshallCString(th, buffer, maxSize);
    ASSERT(slen >= 0 && slen < maxSize);
    const std::string res(buffer, slen);
    delete [] buffer;
    return (res);
}

// To pass to unmarshallMap
static std::string unmarshallStringNoMax(reader &th)
{
    return unmarshallString(th);
}

// string -- 4 byte length, non-terminated string data.
void marshallString4(writer &th, const std::string &data)
{
    marshallInt(th, data.length());
    th.write(data.c_str(), data.length());
}
void unmarshallString4(reader &th, std::string& s)
{
    const int len = unmarshallInt(th);
    s.resize(len);
    if (len) th.read(&s.at(0), len);
}

// boolean (to avoid system-dependant bool implementations)
void marshallBoolean(writer &th, bool data)
{
    th.writeByte(data ? 1 : 0);
}

// boolean (to avoid system-dependant bool implementations)
bool unmarshallBoolean(reader &th)
{
    return (th.readByte() != 0);
}

// Saving the date as a string so we're not reliant on a particular epoch.
std::string make_date_string(time_t in_date)
{
    if (in_date <= 0)
    {
        return ("");
    }

    struct tm *date = TIME_FN(&in_date);

    return make_stringf(
              "%4d%02d%02d%02d%02d%02d%s",
              date->tm_year + 1900, date->tm_mon, date->tm_mday,
              date->tm_hour, date->tm_min, date->tm_sec,
              ((date->tm_isdst > 0) ? "D" : "S"));
}

void marshallEnumVal(writer& wr, const enum_info *ei, int val)
{
    enum_write_state& ews = wr.used_enums[ei];

    if (!ews.store_type)
    {
        std::vector<std::pair<int, std::string> > values;

        ei->collect(values);

        for (unsigned i = 0; i < values.size(); ++i)
        {
            ews.names.insert(values[i]);
        }

        ews.store_type = 1;

        if (ews.names.begin() != ews.names.end()
            && (ews.names.rbegin()->first >= 128
                || ews.names.begin()->first <= -1))
        {
            ews.store_type = 2;
        }

        marshallByte(wr, ews.store_type);
    }

    if (ews.store_type == 2)
        marshallShort(wr, val);
    else
        marshallByte(wr, val);

    if (ews.used.find(val) == ews.used.end())
    {
        ASSERT(ews.names.find(val) != ews.names.end());
        marshallString(wr, ews.names[val]);

        ews.used.insert(val);
    }
}

int unmarshallEnumVal(reader& rd, const enum_info *ei)
{
    enum_read_state& ers = rd.seen_enums[ei];

    if (!ers.store_type)
    {
        std::vector<std::pair<int, std::string> > values;

        ei->collect(values);

        for (unsigned i = 0; i < values.size(); ++i)
        {
            ers.names.insert(make_pair(values[i].second, values[i].first));
        }

        if (rd.getMinorVersion() < ei->non_historical_first)
        {
            ers.store_type = ei->historic_bytes;

            const enum_info::enum_val *evi = ei->historical;

            for (; evi->name; ++evi)
            {
                if (ers.names.find(std::string(evi->name)) != ers.names.end())
                {
                    ers.mapping[evi->value] = ers.names[std::string(evi->name)];
                }
                else
                {
                    ers.mapping[evi->value] = ei->replacement;
                }
            }
        }
        else
        {
            ers.store_type = unmarshallByte(rd);
        }
    }

    int raw;

    if (ers.store_type == 2)
        raw = unmarshallShort(rd);
    else
        raw = unmarshallByte(rd);

    if (ers.mapping.find(raw) != ers.mapping.end())
        return ers.mapping[raw];

    ASSERT(rd.getMinorVersion() >= ei->non_historical_first);

    std::string name = unmarshallString(rd);

    if (ers.names.find(name) != ers.names.end())
    {
        ers.mapping[raw] = ers.names[name];
    }
    else
    {
        ers.mapping[raw] = ei->replacement;
    }

    return ers.mapping[raw];
}

static int get_val_from_string(const char *ptr, int len)
{
    int ret = 0;
    int pow = 1;

    for (const char *chr = ptr + len - 1; chr >= ptr; chr--)
    {
        ret += (*chr - '0') * pow;
        pow *= 10;
    }

    return (ret);
}

static time_t _parse_date_string(char buff[20])
{
    struct tm date;

    date.tm_year = get_val_from_string(&buff[0],  4) - 1900;
    date.tm_mon  = get_val_from_string(&buff[4],  2);
    date.tm_mday = get_val_from_string(&buff[6],  2);
    date.tm_hour = get_val_from_string(&buff[8],  2);
    date.tm_min  = get_val_from_string(&buff[10], 2);
    date.tm_sec  = get_val_from_string(&buff[12], 2);

    date.tm_isdst = (buff[14] == 'D');

    return (mktime(&date));
}

// Write a tagged chunk of data to the FILE*.
// tagId specifies what to write.
void tag_write(tag_type tagID, writer &outf)
{
    std::vector<unsigned char> buf;
    writer th(&buf);
    switch (tagID)
    {
    case TAG_CHR:
        tag_construct_char(th);
        tag_construct_game_state(th);
        break;
    case TAG_YOU:
        tag_construct_you(th);
        tag_construct_you_items(th);
        tag_construct_you_dungeon(th);
        tag_construct_lost_monsters(th);
        tag_construct_lost_items(th);
        break;
    case TAG_LEVEL:
        tag_construct_level(th);
        tag_construct_level_items(th);
        tag_construct_level_monsters(th);
        tag_construct_level_tiles(th);
        break;
    case TAG_GHOST:          tag_construct_ghost(th);          break;
    default:
        // I don't know how to make that!
        break;
    }

    // make sure there is some data to write!
    if (buf.size() == 0)
        return;

    // Write tag header.
    marshallInt(outf, buf.size());

    // Write tag data.
    outf.write(&buf[0], buf.size());
}

// Read a piece of data from inf into memory, then run the appropiate reader.
//
// minorVersion is available for any sub-readers that need it
void tag_read(reader &inf, int minorVersion, tag_type tag_id)
{
    // Read header info and data
    std::vector<unsigned char> buf;
    const int data_size = unmarshallInt(inf);
    ASSERT(data_size >= 0);

    // Fetch data in one go
    buf.resize(data_size);
    inf.read(&buf[0], buf.size());

    unwind_var<int> tag_minor_version(_tag_minor_version, minorVersion);

    // Ok, we have data now.
    reader th(buf, minorVersion);
    switch (tag_id)
    {
    case TAG_CHR:
        tag_read_char(th, minorVersion);
        tag_read_game_state(th);
        break;
    case TAG_YOU:
        tag_read_you(th, minorVersion);
        tag_read_you_items(th, minorVersion);
        tag_read_you_dungeon(th, minorVersion);
        tag_read_lost_monsters(th);
        tag_read_lost_items(th);
        break;
    case TAG_LEVEL:
        tag_read_level(th, minorVersion);
        tag_read_level_items(th, minorVersion);
        tag_read_level_monsters(th, minorVersion);
        tag_read_level_tiles(th);
        break;
    case TAG_GHOST:          tag_read_ghost(th, minorVersion);          break;
    default:
        // I don't know how to read that!
        ASSERT(!"unknown tag type");
    }
}

static void tag_construct_char(writer &th)
{
    marshallString(th, you.your_name, kNameLen);
    marshallString(th, Version::Long());

    marshallByte(th, you.species);
    marshallByte(th, you.char_class);
    marshallByte(th, you.experience_level);
    marshallString(th, you.class_name, 30);
    marshallByte(th, you.religion);
    marshallString(th, you.jiyva_second_name);

    marshallByte(th, you.wizard);
}

static void tag_construct_you(writer &th)
{
    int i, j;

    marshallByte(th, you.piety);
    marshallByte(th, you.rotting);
    marshallShort(th, you.pet_target);

    marshallByte(th, you.max_level);
    marshallByte(th, you.where_are_you);
    marshallByte(th, you.char_direction);
    marshallByte(th, you.opened_zot);
    marshallByte(th, you.royal_jelly_dead);
    marshallByte(th, you.transform_uncancellable);
    marshallByte(th, you.absdepth0);
    marshallByte(th, you.is_undead);
    marshallShort(th, you.unrand_reacts);
    marshallByte(th, you.berserk_penalty);
    marshallShort(th, you.sage_bonus_skill);
    marshallInt(th, you.sage_bonus_degree);
    marshallByte(th, you.level_type);
    marshallString(th, you.level_type_name);
    marshallString(th, you.level_type_name_abbrev);
    marshallString(th, you.level_type_origin);
    marshallString(th, you.level_type_tag);
    marshallString(th, you.level_type_ext);
    marshallByte(th, you.entry_cause);
    marshallByte(th, you.entry_cause_god);

    marshallInt(th, you.disease);
    marshallShort(th, you.dead ? 0 : you.hp);

    marshallShort(th, you.hunger);
    marshallBoolean(th, you.fishtail);
    marshallInt(th, you.earth_attunement);

    // how many you.equip?
    marshallByte(th, NUM_EQUIP);
    for (i = 0; i < NUM_EQUIP; ++i)
        marshallByte(th, you.equip[i]);
    for (i = 0; i < NUM_EQUIP; ++i)
        marshallBoolean(th, you.melded[i]);

    marshallByte(th, you.magic_points);
    marshallByte(th, you.max_magic_points);

    COMPILE_CHECK(NUM_STATS == 3, c2);
    for (i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.base_stats[i]);
    for (i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.stat_loss[i]);
    for (i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.stat_zero[i]);
    for (i = 0; i < NUM_STATS; ++i)
        marshallString(th, you.stat_zero_cause[i]);

    marshallByte(th, you.last_chosen);
    marshallByte(th, you.hit_points_regeneration);
    marshallByte(th, you.magic_points_regeneration);

    marshallShort(th, you.hit_points_regeneration * 100);
    marshallInt(th, you.experience);
    marshallInt(th, you.gold);

    marshallInt(th, you.exp_available);

    marshallShort(th, you.base_hp);
    marshallShort(th, you.base_hp2);
    marshallShort(th, you.base_magic_points);
    marshallShort(th, you.base_magic_points2);

    marshallShort(th, you.pos().x);
    marshallShort(th, you.pos().y);

    marshallShort(th, you.burden);

    // how many spells?
    marshallUByte(th, MAX_KNOWN_SPELLS);
    for (i = 0; i < MAX_KNOWN_SPELLS; ++i)
        marshallByte(th, you.spells[i]);

    marshallByte(th, 52);
    for (i = 0; i < 52; i++)
        marshallByte(th, you.spell_letter_table[i]);

    marshallByte(th, 52);
    for (i = 0; i < 52; i++)
        marshallShort(th, you.ability_letter_table[i]);

    // how many skills?
    marshallByte(th, NUM_SKILLS);
    for (j = 0; j < NUM_SKILLS; ++j)
    {
        marshallByte(th, you.skills[j]);
        marshallByte(th, you.practise_skill[j]);
        marshallInt(th, you.skill_points[j]);
        marshallInt(th, you.ct_skill_points[j]);
        marshallByte(th, you.skill_order[j]);   // skills ordering
    }

    marshallInt(th, you.transfer_from_skill);
    marshallInt(th, you.transfer_to_skill);
    marshallInt(th, you.transfer_skill_points);
    marshallInt(th, you.transfer_total_skill_points);

    // how many durations?
    marshallByte(th, NUM_DURATIONS);
    for (j = 0; j < NUM_DURATIONS; ++j)
        marshallInt(th, you.duration[j]);

    // how many attributes?
    marshallByte(th, NUM_ATTRIBUTES);
    for (j = 0; j < NUM_ATTRIBUTES; ++j)
        marshallInt(th, you.attribute[j]);

    // Sacrifice values.
    marshallByte(th, NUM_OBJECT_CLASSES);
    for (j = 0; j < NUM_OBJECT_CLASSES; ++j)
        marshallInt(th, you.sacrifice_value[j]);

    // how many mutations/demon powers?
    marshallShort(th, NUM_MUTATIONS);
    for (j = 0; j < NUM_MUTATIONS; ++j)
    {
        marshallByte(th, you.mutation[j]);
        marshallByte(th, you.innate_mutations[j]);
    }

    marshallByte(th, you.demonic_traits.size());
    for (j = 0; j < int(you.demonic_traits.size()); ++j)
    {
        marshallByte(th, you.demonic_traits[j].level_gained);
        marshallShort(th, you.demonic_traits[j].mutation);
    }

    // how many penances?
    marshallByte(th, MAX_NUM_GODS);
    for (i = 0; i < MAX_NUM_GODS; i++)
        marshallByte(th, you.penance[i]);

    // which gods have been worshipped by this character?
    marshallByte(th, MAX_NUM_GODS);
    for (i = 0; i < MAX_NUM_GODS; i++)
        marshallByte(th, you.worshipped[i]);

    // what is the extent of divine generosity?
    for (i = 0; i < MAX_NUM_GODS; i++)
        marshallShort(th, you.num_current_gifts[i]);
    for (i = 0; i < MAX_NUM_GODS; i++)
        marshallShort(th, you.num_total_gifts[i]);

    marshallByte(th, you.gift_timeout);
#if TAG_MAJOR_VERSION == 31
    marshallByte(th, you.normal_vision);
    marshallByte(th, you.current_vision);
#endif
    marshallByte(th, you.hell_exit);
    marshallByte(th, you.hell_branch);

    // elapsed time
    marshallInt(th, you.elapsed_time);


    // time of game start
    marshallString(th, make_date_string(you.birth_time).c_str(), 20);

    // real_time == -1 means game was started before this feature.
    if (you.real_time != -1)
    {
        const time_t now = time(NULL);
        you.real_time += std::min<time_t>(now - you.start_time,
                                          IDLE_TIME_CLAMP);

        // Reset start_time now that real_time is being saved out...
        // this may just be a level save.
        you.start_time = now;
    }

    marshallInt(th, you.real_time);
    marshallInt(th, you.num_turns);
    marshallInt(th, you.exploration);

    marshallShort(th, you.magic_contamination);

    marshallShort(th, you.transit_stair);
    marshallByte(th, you.entering_level);

    marshallByte(th, you.deaths);
    marshallByte(th, you.lives);

    marshallInt(th, you.dactions.size());
    for (unsigned int k = 0; k < you.dactions.size(); k++)
        marshallByte(th, you.dactions[k]);

    // List of currently beholding monsters (usually empty).
    marshallShort(th, you.beholders.size());
    for (unsigned int k = 0; k < you.beholders.size(); k++)
         marshallShort(th, you.beholders[k]);

    marshallShort(th, you.fearmongers.size());
    for (unsigned int k = 0; k < you.fearmongers.size(); k++)
        marshallShort(th, you.fearmongers[k]);

    marshallByte(th, you.piety_hysteresis);

    you.m_quiver->save(th);

    marshallByte(th, you.friendly_pickup);

    if (!dlua.callfn("dgn_save_data", "u", &th))
        mprf(MSGCH_ERROR, "Failed to save Lua data: %s", dlua.error.c_str());

    // Write a human-readable string out on the off chance that
    // we fail to be able to read this file back in using some later version.
    std::string revision = "Git:";
    revision += Version::Long();
    marshallString(th, revision);

    you.props.write(th);
#if TAG_MAJOR_VERSION == 31
    marshallByte(th, NUM_DC);
    for (int t = 0; t < 2; t++)
        for (int ae = 0; ae < 2; ae++)
            for (i = 0; i < NUM_DC; i++)
                marshallInt(th, you.dcounters[t][ae][i]);
#endif
}

static void tag_construct_you_items(writer &th)
{
    int i,j;

    // how many inventory slots?
    marshallByte(th, ENDOFPACK);
    for (i = 0; i < ENDOFPACK; ++i)
        marshallItem(th, you.inv[i]);

    // Item descrip for each type & subtype.
    // how many types?
    marshallByte(th, NUM_IDESC);
    // how many subtypes?
    marshallByte(th, 50);
    for (i = 0; i < NUM_IDESC; ++i)
        for (j = 0; j < 50; ++j)
            marshallByte(th, you.item_description[i][j]);

    // Identification status.
    const id_arr& identy(get_typeid_array());
    // how many types?
    marshallUByte(th, identy.width());
    // how many subtypes?
    marshallUByte(th, identy.height());

    for (i = 0; i < identy.width(); ++i)
        for (j = 0; j < identy.height(); ++j)
            marshallUByte(th, identy[i][j]);

    // Additional identification info
    get_type_id_props().write(th);

    // how many unique items?
    marshallByte(th, MAX_UNRANDARTS);
    for (j = 0; j < MAX_UNRANDARTS; ++j)
        marshallByte(th,you.unique_items[j]);

    marshallByte(th, NUM_FIXED_BOOKS);
    for (j = 0; j < NUM_FIXED_BOOKS; ++j)
        marshallByte(th,you.had_book[j]);

    marshallShort(th, NUM_SPELLS);
    for (j = 0; j < NUM_SPELLS; ++j)
        marshallByte(th,you.seen_spell[j]);

    marshallShort(th, NUM_WEAPONS);
    for (j = 0; j < NUM_WEAPONS; ++j)
        marshallInt(th,you.seen_weapon[j]);

    marshallShort(th, NUM_ARMOURS);
    for (j = 0; j < NUM_ARMOURS; ++j)
        marshallInt(th,you.seen_armour[j]);
}

static void marshallPlaceInfo(writer &th, PlaceInfo place_info)
{
    marshallInt(th, place_info.level_type);
    marshallInt(th, place_info.branch);

    marshallInt(th, place_info.num_visits);
    marshallInt(th, place_info.levels_seen);

    marshallInt(th, place_info.mon_kill_exp);
    marshallInt(th, place_info.mon_kill_exp_avail);

    for (int i = 0; i < KC_NCATEGORIES; i++)
        marshallInt(th, place_info.mon_kill_num[i]);

    marshallInt(th, place_info.turns_total);
    marshallInt(th, place_info.turns_explore);
    marshallInt(th, place_info.turns_travel);
    marshallInt(th, place_info.turns_interlevel);
    marshallInt(th, place_info.turns_resting);
    marshallInt(th, place_info.turns_other);

    marshallInt(th, place_info.elapsed_total);
    marshallInt(th, place_info.elapsed_explore);
    marshallInt(th, place_info.elapsed_travel);
    marshallInt(th, place_info.elapsed_interlevel);
    marshallInt(th, place_info.elapsed_resting);
    marshallInt(th, place_info.elapsed_other);
}

static void tag_construct_you_dungeon(writer &th)
{
    // how many unique creatures?
    marshallShort(th, NUM_MONSTERS);
    for (int j = 0; j < NUM_MONSTERS; ++j)
        marshallByte(th,you.unique_creatures[j]); // unique beasties

    // how many branches?
    marshallByte(th, NUM_BRANCHES);
    for (int j = 0; j < NUM_BRANCHES; ++j)
    {
        marshallInt(th, branches[j].startdepth);
        marshallInt(th, branches[j].branch_flags);
    }

    marshallSet(th, Generated_Levels, marshall_level_id);

    marshallMap(th, stair_level,
                _marshall_as_int<branch_type>, marshall_level_id);
    marshallMap(th, shops_present,
                marshall_level_pos, _marshall_as_int<shop_type>);
    marshallMap(th, altars_present,
                marshall_level_pos, _marshall_as_int<god_type>);
    marshallMap(th, portals_present,
                marshall_level_pos, _marshall_as_int<portal_type>);
    marshallMap(th, portal_vaults_present,
                marshall_level_pos, marshallStringNoMax);
    marshallMap(th, portal_vault_notes,
                marshall_level_pos, marshallStringNoMax);
    marshallMap(th, portal_vault_colours,
                marshall_level_pos, marshallUByteRef);
    marshallMap(th, level_annotations,
                marshall_level_id, marshallStringNoMax);
    marshallMap(th, level_exclusions,
                marshall_level_id, marshallStringNoMax);

    marshallPlaceInfo(th, you.global_info);
    std::vector<PlaceInfo> list = you.get_all_place_info();
    // How many different places we have info on?
    marshallShort(th, list.size());

    for (unsigned int k = 0; k < list.size(); k++)
        marshallPlaceInfo(th, list[k]);

    marshall_iterator(th, you.uniq_map_tags.begin(), you.uniq_map_tags.end(),
                      marshallStringNoMax);
    marshall_iterator(th, you.uniq_map_names.begin(), you.uniq_map_names.end(),
                      marshallStringNoMax);

    write_level_connectivity(th);
}

static void marshall_follower(writer &th, const follower &f)
{
    marshallMonster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        marshallItem(th, f.items[i]);
}

static void unmarshall_follower(reader &th, follower &f)
{
    unmarshallMonster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        unmarshallItem(th, f.items[i]);
}

static void marshall_follower_list(writer &th, const m_transit_list &mlist)
{
    marshallShort(th, mlist.size());

    for (m_transit_list::const_iterator mi = mlist.begin();
         mi != mlist.end(); ++mi)
    {
        marshall_follower(th, *mi);
    }
}

static void marshall_item_list(writer &th, const i_transit_list &ilist)
{
    marshallShort(th, ilist.size());

    for (i_transit_list::const_iterator ii = ilist.begin();
         ii != ilist.end(); ++ii)
    {
        marshallItem(th, *ii);
    }
}


static m_transit_list unmarshall_follower_list(reader &th)
{
    m_transit_list mlist;

    const int size = unmarshallShort(th);

    for (int i = 0; i < size; ++i)
    {
        follower f;
        unmarshall_follower(th, f);
        mlist.push_back(f);
    }

    return (mlist);
}

static i_transit_list unmarshall_item_list(reader &th)
{
    i_transit_list ilist;

    const int size = unmarshallShort(th);

    for (int i = 0; i < size; ++i)
    {
        item_def item;
        unmarshallItem(th, item);
        ilist.push_back(item);
    }

    return (ilist);
}

static void marshall_level_map_masks(writer &th)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        marshallShort(th, env.level_map_mask(*ri));
        marshallShort(th, env.level_map_ids(*ri));
    }
}

static void unmarshall_level_map_masks(reader &th)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        env.level_map_mask(*ri) = unmarshallShort(th);
        env.level_map_ids(*ri)  = unmarshallShort(th);
    }
}

static void marshall_level_map_unique_ids(writer &th)
{
    marshallSet(th, env.level_uniq_maps, marshallStringNoMax);
    marshallSet(th, env.level_uniq_map_tags, marshallStringNoMax);
}

static void unmarshall_level_map_unique_ids(reader &th)
{
    unmarshallSet(th, env.level_uniq_maps, unmarshallStringNoMax);
    unmarshallSet(th, env.level_uniq_map_tags, unmarshallStringNoMax);
}

static void marshall_mapdef(writer &th, const map_def &map)
{
    marshallStringNoMax(th, map.name);
    map.write_full(th);
    map.write_index(th);
    map.write_maplines(th);
    marshallString(th, map.description);
}

static map_def unmarshall_mapdef(reader &th)
{
    map_def map;
    map.name = unmarshallStringNoMax(th);
    map.read_full(th, false);
    map.read_index(th);
    map.read_maplines(th);
    map.description = unmarshallString(th);
    return map;
}

static void marshall_vault_placement(writer &th, const vault_placement &vp)
{
    marshallCoord(th, vp.pos);
    marshallCoord(th, vp.size);
    marshallShort(th, vp.orient);
    marshall_mapdef(th, vp.map);
    marshall_iterator(th, vp.exits.begin(), vp.exits.end(), marshallCoord);
    marshallShort(th, vp.level_number);
    marshallByte(th, vp.seen);
}

static vault_placement unmarshall_vault_placement(reader &th)
{
    vault_placement vp;
    vp.pos = unmarshallCoord(th);
    vp.size = unmarshallCoord(th);
    vp.orient = unmarshallShort(th);
    vp.map = unmarshall_mapdef(th);
    unmarshall_vector(th, vp.exits, unmarshallCoord);
    vp.level_number = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 31
    if (th.getMinorVersion() < TAG_MINOR_RUNE_SUBST)
        unmarshallShort(th);
#endif
    vp.seen = !!unmarshallByte(th);

    return vp;
}

static void marshall_level_vault_placements(writer &th)
{
    marshallShort(th, env.level_vaults.size());
    for (int i = 0, size = env.level_vaults.size(); i < size; ++i)
        marshall_vault_placement(th, *env.level_vaults[i]);
}

static void unmarshall_level_vault_placements(reader &th)
{
    const int nvaults = unmarshallShort(th);
    ASSERT(nvaults >= 0);
    dgn_clear_vault_placements(env.level_vaults);
    for (int i = 0; i < nvaults; ++i)
    {
        env.level_vaults.push_back(
            new vault_placement(unmarshall_vault_placement(th)));
    }
}

static void marshall_level_vault_data(writer &th)
{
    marshallStringNoMax(th, env.level_build_method);
    marshallStringNoMax(th, env.level_layout_type);

    marshall_level_map_masks(th);
    marshall_level_map_unique_ids(th);
    marshall_level_vault_placements(th);
}

static void unmarshall_level_vault_data(reader &th)
{
    env.level_build_method = unmarshallStringNoMax(th);
    env.level_layout_type  = unmarshallStringNoMax(th);
    unmarshall_level_map_masks(th);
    unmarshall_level_map_unique_ids(th);
    unmarshall_level_vault_placements(th);
}

static void tag_construct_lost_monsters(writer &th)
{
    marshallMap(th, the_lost_ones, marshall_level_id,
                 marshall_follower_list);
}

static void tag_construct_lost_items(writer &th)
{
    marshallMap(th, transiting_items, marshall_level_id,
                 marshall_item_list);
}

static void tag_construct_game_state(writer &th)
{
    marshallByte(th, crawl_state.type);
}

static void tag_read_char(reader &th, int minorVersion)
{
    you.your_name         = unmarshallString(th, kNameLen);
    const std::string old_version = unmarshallString(th);
    dprf("Last save Crawl version: %s", old_version.c_str());

    you.species           = static_cast<species_type>(unmarshallByte(th));
    you.char_class        = static_cast<job_type>(unmarshallByte(th));
    you.experience_level  = unmarshallByte(th);
    unmarshallCString(th, you.class_name, 30);
    you.religion          = static_cast<god_type>(unmarshallByte(th));
    you.jiyva_second_name   = unmarshallString(th);

    you.wizard            = unmarshallBoolean(th);
}

static void tag_read_you(reader &th, int minorVersion)
{
    char buff[20];      // For birth date.
    int i,j;
    int count;

    you.piety             = unmarshallByte(th);
    you.rotting           = unmarshallByte(th);
    you.pet_target        = unmarshallShort(th);

    you.max_level         = unmarshallByte(th);
    you.where_are_you     = static_cast<branch_type>(unmarshallByte(th));
    you.char_direction    = static_cast<game_direction_type>(unmarshallByte(th));

    you.opened_zot = unmarshallBoolean(th);

    you.royal_jelly_dead = unmarshallBoolean(th);

    you.transform_uncancellable = unmarshallBoolean(th);

    you.absdepth0         = unmarshallByte(th);
    you.is_undead         = static_cast<undead_state_type>(unmarshallByte(th));
    you.unrand_reacts     = unmarshallShort(th);
    you.berserk_penalty   = unmarshallByte(th);
    you.sage_bonus_skill  = static_cast<skill_type>(unmarshallShort(th));
    you.sage_bonus_degree = unmarshallInt(th);
    you.level_type        = static_cast<level_area_type>(unmarshallByte(th));
    you.level_type_name   = unmarshallString(th);

    you.level_type_name_abbrev = unmarshallString(th);
    you.level_type_origin      = unmarshallString(th);
    you.level_type_tag         = unmarshallString(th);
    you.level_type_ext         = unmarshallString(th);

    you.entry_cause     = static_cast<entry_cause_type>(unmarshallByte(th));
    you.entry_cause_god = static_cast<god_type>(unmarshallByte(th));
    you.disease         = unmarshallInt(th);

    you.hp              = unmarshallShort(th);
    you.hunger          = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 31
    if (minorVersion >= TAG_MINOR_FISHTAIL)
#endif
    you.fishtail        = unmarshallBoolean(th);
#if TAG_MAJOR_VERSION == 31
    if (minorVersion >= TAG_MINOR_EARTH_ATTUNE)
#endif
    you.earth_attunement= unmarshallInt(th);

    // How many you.equip?
    count = unmarshallByte(th);
    for (i = 0; i < count; ++i)
        you.equip[i] = unmarshallByte(th);
    for (i = 0; i < count; ++i)
        you.melded[i] = unmarshallBoolean(th);

    you.magic_points              = unmarshallByte(th);
    you.max_magic_points          = unmarshallByte(th);

    for (i = 0; i < NUM_STATS; ++i)
        you.base_stats[i] = unmarshallByte(th);
    for (i = 0; i < NUM_STATS; ++i)
        you.stat_loss[i] = unmarshallByte(th);
    for (i = 0; i < NUM_STATS; ++i)
        you.stat_zero[i] = unmarshallByte(th);
    for (i = 0; i < NUM_STATS; ++i)
        you.stat_zero_cause[i] = unmarshallString(th);

    you.last_chosen = (stat_type) unmarshallByte(th);

    you.hit_points_regeneration   = unmarshallByte(th);
    you.magic_points_regeneration = unmarshallByte(th);

    you.hit_points_regeneration   = unmarshallShort(th) / 100;
    you.experience                = unmarshallInt(th);
    you.gold                      = unmarshallInt(th);
    you.exp_available             = unmarshallInt(th);

    you.base_hp                   = unmarshallShort(th);
    you.base_hp2                  = unmarshallShort(th);
    you.base_magic_points         = unmarshallShort(th);
    you.base_magic_points2        = unmarshallShort(th);

    const int x = unmarshallShort(th);
    const int y = unmarshallShort(th);
    you.moveto(coord_def(x, y));

    you.burden = unmarshallShort(th);

    // how many spells?
    you.spell_no = 0;
    count = unmarshallUByte(th);
    ASSERT(count >= 0);
    for (i = 0; i < count && i < MAX_KNOWN_SPELLS; ++i)
    {
        you.spells[i] = static_cast<spell_type>(unmarshallUByte(th));
        if (you.spells[i] != SPELL_NO_SPELL)
            you.spell_no++;
    }
    for (; i < count; ++i)
        unmarshallUByte(th);

    count = unmarshallByte(th);
    for (i = 0; i < count; i++)
        you.spell_letter_table[i] = unmarshallByte(th);

    count = unmarshallByte(th);
    for (i = 0; i < count; i++)
    {
        you.ability_letter_table[i] =
            static_cast<ability_type>(unmarshallShort(th));
    }

    // how many skills?
    count = unmarshallByte(th);
    for (j = 0; j < count; ++j)
    {
#if TAG_MAJOR_VERSION == 31
        if (j >= NUM_SKILLS)
        {
            unmarshallByte(th);
            unmarshallByte(th);
            unmarshallInt(th);
            unmarshallByte(th);
            continue;
        }
#endif
        you.skills[j]          = unmarshallByte(th);
        you.practise_skill[j]  = unmarshallByte(th);
        you.skill_points[j]    = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 31
        if (minorVersion >= TAG_MINOR_CROSSTRAIN)
#endif
        you.ct_skill_points[j] = unmarshallInt(th);
        you.skill_order[j]     = unmarshallByte(th);
    }

#if TAG_MAJOR_VERSION == 31
    if (minorVersion >= TAG_MINOR_SLOW_RESKILL)
    {
#endif
    you.transfer_from_skill = static_cast<skill_type>(unmarshallInt(th));
    you.transfer_to_skill = static_cast<skill_type>(unmarshallInt(th));
    you.transfer_skill_points = unmarshallInt(th);
    you.transfer_total_skill_points = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 31
    }
#endif

    // Set up you.total_skill_points and you.skill_cost_level.
    calc_total_skill_points();

    // how many durations?
    count = unmarshallByte(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NUM_DURATIONS; ++j)
        you.duration[j] = unmarshallInt(th);
    for (j = NUM_DURATIONS; j < count; ++j)
        unmarshallInt(th);

    // how many attributes?
    count = unmarshallByte(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NUM_ATTRIBUTES; ++j)
        you.attribute[j] = unmarshallInt(th);
    for (j = count; j < NUM_ATTRIBUTES; ++j)
        you.attribute[j] = 0;
    for (j = NUM_ATTRIBUTES; j < count; ++j)
        unmarshallInt(th);

    count = unmarshallByte(th);
    ASSERT(count == NUM_OBJECT_CLASSES);
    for (j = 0; j < count; ++j)
        you.sacrifice_value[j] = unmarshallInt(th);

    // how many mutations/demon powers?
    count = unmarshallShort(th);
    ASSERT(count >= 0 && count <= NUM_MUTATIONS);
    for (j = 0; j < count; ++j)
    {
        you.mutation[j]  = unmarshallByte(th);
        you.innate_mutations[j] = unmarshallByte(th);
    }
    for (j = count; j < NUM_MUTATIONS; ++j)
        you.mutation[j] = you.innate_mutations[j] = 0;

    count = unmarshallByte(th);
    ASSERT(count >= 0);
    you.demonic_traits.clear();
    for (j = 0; j < count; ++j)
    {
        player::demon_trait dt;
        dt.level_gained = unmarshallByte(th);
        ASSERT(dt.level_gained > 1 && dt.level_gained <= 27);
        dt.mutation = static_cast<mutation_type>(unmarshallShort(th));
        you.demonic_traits.push_back(dt);
    }

    // how many penances?
    count = unmarshallByte(th);
    for (i = 0; i < count; i++)
        you.penance[i] = unmarshallByte(th);

    count = unmarshallByte(th);
    for (i = 0; i < count; i++)
        you.worshipped[i] = unmarshallByte(th);

#if TAG_MAJOR_VERSION == 31
    if (minorVersion >= TAG_MINOR_GOD_GIFTS)
    {
#endif
    for (i = 0; i < count; i++)
        you.num_current_gifts[i] = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 31
    }
#endif
    for (i = 0; i < count; i++)
        you.num_total_gifts[i] = unmarshallShort(th);

    you.gift_timeout   = unmarshallByte(th);

#if TAG_MAJOR_VERSION == 31
    you.normal_vision  = unmarshallByte(th);
    you.current_vision = unmarshallByte(th);
    // it will be recalculated in startup.c:_post_init() anyway
#endif
    you.hell_exit      = unmarshallByte(th);
    you.hell_branch = static_cast<branch_type>(unmarshallByte(th));

    // elapsed time
    you.elapsed_time   = unmarshallInt(th);

    // time of character creation
    unmarshallCString(th, buff, 20);
    you.birth_time = _parse_date_string(buff);

    you.real_time  = unmarshallInt(th);
    you.num_turns  = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 31
    if (minorVersion >= TAG_MINOR_DENSITY)
#endif
    you.exploration = unmarshallInt(th);

    you.magic_contamination = unmarshallShort(th);

    you.transit_stair  = static_cast<dungeon_feature_type>(unmarshallShort(th));
    you.entering_level = unmarshallByte(th);

    you.deaths = unmarshallByte(th);
    you.lives = unmarshallByte(th);
    you.dead = !you.hp;

    int n_dact = unmarshallInt(th);
    you.dactions.resize(n_dact, NUM_DACTIONS);
    for (i = 0; i < n_dact; i++)
        you.dactions[i] = static_cast<daction_type>(unmarshallByte(th));

    // List of currently beholding monsters (usually empty).
    count = unmarshallShort(th);
    for (i = 0; i < count; i++)
        you.beholders.push_back(unmarshallShort(th));

    // Also usually empty.
    count = unmarshallShort(th);
    for (i = 0; i < count; i++)
        you.fearmongers.push_back(unmarshallShort(th));

    you.piety_hysteresis = unmarshallByte(th);

    you.m_quiver->load(th);

    you.friendly_pickup = unmarshallByte(th);

    if (!dlua.callfn("dgn_load_data", "u", &th))
        mprf(MSGCH_ERROR, "Failed to load Lua persist table: %s",
             dlua.error.c_str());

    std::string rev_str = unmarshallString(th);
    UNUSED(rev_str);

    you.props.clear();
    you.props.read(th);
#if TAG_MAJOR_VERSION == 31
    if (minorVersion >= TAG_MINOR_DIAG_COUNTERS)
    {
        count = unmarshallByte(th);
        ASSERT(count <= NUM_DC);
        for (int t = 0; t < 2; t++)
            for (int ae = 0; ae < 2; ae++)
                for (i = 0; i < count; i++)
                    you.dcounters[t][ae][i] = unmarshallInt(th);
    }
#endif
}

static void tag_read_you_items(reader &th, int minorVersion)
{
    int i,j;
    int count, count2;

    // how many inventory slots?
    count = unmarshallByte(th);
    ASSERT(count == ENDOFPACK); // not supposed to change
    for (i = 0; i < count; ++i)
        unmarshallItem(th, you.inv[i]);

    // Item descrip for each type & subtype.
    // how many types?
    count = unmarshallByte(th);
    // how many subtypes?
    count2 = unmarshallByte(th);
    for (i = 0; i < count; ++i)
        for (j = 0; j < count2; ++j)
            you.item_description[i][j] = unmarshallByte(th);

    // Identification status.
    // how many types?
    count = unmarshallByte(th);
    // how many subtypes?
    count2 = unmarshallByte(th);

    // Argh... this is awful!
    for (i = 0; i < count; ++i)
        for (j = 0; j < count2; ++j)
        {
            const item_type_id_state_type ch =
                static_cast<item_type_id_state_type>(unmarshallByte(th));

            switch (i)
            {
                case IDTYPE_WANDS:
                    set_ident_type(OBJ_WANDS, j, ch);
                    break;
                case IDTYPE_SCROLLS:
                    set_ident_type(OBJ_SCROLLS, j, ch);
                    break;
                case IDTYPE_JEWELLERY:
                    set_ident_type(OBJ_JEWELLERY, j, ch);
                    break;
                case IDTYPE_POTIONS:
                    set_ident_type(OBJ_POTIONS, j, ch);
                    break;
                case IDTYPE_STAVES:
                    set_ident_type(OBJ_STAVES, j, ch);
                    break;
            }
        }

    // Additional identification info
#if TAG_MAJOR_VERSION == 31
    if (minorVersion >= TAG_MINOR_ADD_ID_INFO)
#endif
    get_type_id_props().read(th);

    // how many unique items?
    count = unmarshallByte(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NO_UNRANDARTS; ++j)
    {
        you.unique_items[j] =
            static_cast<unique_item_status_type>(unmarshallByte(th));
    }
    // # of unrandarts could certainly change.
    // If it does, the new ones won't exist yet - zero them out.
    for (; j < NO_UNRANDARTS; j++)
        you.unique_items[j] = UNIQ_NOT_EXISTS;
    for (j = NO_UNRANDARTS; j < count; j++)
        unmarshallByte(th);

    // how many books?
    count = unmarshallByte(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NUM_FIXED_BOOKS; ++j)
        you.had_book[j] = unmarshallByte(th);
    for (j = count; j < NUM_FIXED_BOOKS; ++j)
        you.seen_spell[j] = 0;
    for (j = NUM_FIXED_BOOKS; j < count; ++j)
        unmarshallByte(th);

    // how many spells?
    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NUM_SPELLS; ++j)
        you.seen_spell[j] = unmarshallByte(th);
    for (j = count; j < NUM_SPELLS; ++j)
        you.seen_spell[j] = 0;
    for (j = NUM_SPELLS; j < count; ++j)
        unmarshallByte(th);

    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NUM_WEAPONS; ++j)
        you.seen_weapon[j] = unmarshallInt(th);
    for (j = count; j < NUM_WEAPONS; ++j)
        you.seen_weapon[j] = 0;
    for (j = NUM_WEAPONS; j < count; ++j)
        unmarshallInt(th);

    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (j = 0; j < count && j < NUM_ARMOURS; ++j)
        you.seen_armour[j] = unmarshallInt(th);
    for (j = count; j < NUM_ARMOURS; ++j)
        you.seen_armour[j] = 0;
    for (j = NUM_ARMOURS; j < count; ++j)
        unmarshallInt(th);
}

static PlaceInfo unmarshallPlaceInfo(reader &th, int minorVersion)
{
    PlaceInfo place_info;

    place_info.level_type = unmarshallInt(th);
    place_info.branch     = unmarshallInt(th);

    place_info.num_visits  = unmarshallInt(th);
    place_info.levels_seen = unmarshallInt(th);

    place_info.mon_kill_exp       = unmarshallInt(th);
    place_info.mon_kill_exp_avail = unmarshallInt(th);

    for (int i = 0; i < KC_NCATEGORIES; i++)
        place_info.mon_kill_num[i] = unmarshallInt(th);

    place_info.turns_total      = unmarshallInt(th);
    place_info.turns_explore    = unmarshallInt(th);
    place_info.turns_travel     = unmarshallInt(th);
    place_info.turns_interlevel = unmarshallInt(th);
    place_info.turns_resting    = unmarshallInt(th);
    place_info.turns_other      = unmarshallInt(th);

    place_info.elapsed_total      = unmarshallInt(th);
    place_info.elapsed_explore    = unmarshallInt(th);
    place_info.elapsed_travel     = unmarshallInt(th);
    place_info.elapsed_interlevel = unmarshallInt(th);
    place_info.elapsed_resting    = unmarshallInt(th);
    place_info.elapsed_other      = unmarshallInt(th);

    return place_info;
}

static void tag_read_you_dungeon(reader &th, int minorVersion)
{
    // how many unique creatures?
    int count = unmarshallShort(th);
    you.unique_creatures.init(false);
    for (int j = 0; j < count; ++j)
    {
        const bool created = unmarshallBoolean(th);

        if (j < NUM_MONSTERS)
            you.unique_creatures[j] = created;
    }

    // how many branches?
    count = unmarshallByte(th);
    for (int j = 0; j < count; ++j)
    {
        branches[j].startdepth   = unmarshallInt(th);
        branches[j].branch_flags = unmarshallInt(th);
    }

    unmarshallSet(th, Generated_Levels, unmarshall_level_id);

    unmarshallMap(th, stair_level,
                  unmarshall_long_as<branch_type>,
                  unmarshall_level_id);
    unmarshallMap(th, shops_present,
                  unmarshall_level_pos, unmarshall_long_as<shop_type>);
    unmarshallMap(th, altars_present,
                  unmarshall_level_pos, unmarshall_long_as<god_type>);
    unmarshallMap(th, portals_present,
                  unmarshall_level_pos, unmarshall_long_as<portal_type>);
    unmarshallMap(th, portal_vaults_present,
                  unmarshall_level_pos, unmarshallStringNoMax);
    unmarshallMap(th, portal_vault_notes,
                  unmarshall_level_pos, unmarshallStringNoMax);
    unmarshallMap(th, portal_vault_colours,
                  unmarshall_level_pos, unmarshallUByte);
    unmarshallMap(th, level_annotations,
                  unmarshall_level_id, unmarshallStringNoMax);
    unmarshallMap(th, level_exclusions,
                  unmarshall_level_id, unmarshallStringNoMax);

    PlaceInfo place_info = unmarshallPlaceInfo(th, minorVersion);
    ASSERT(place_info.is_global());
    you.set_place_info(place_info);

    std::vector<PlaceInfo> list = you.get_all_place_info();
    unsigned short count_p = (unsigned short) unmarshallShort(th);
    // Use "<=" so that adding more branches or non-dungeon places
    // won't break save-file compatibility.
    ASSERT(count_p <= list.size());

    for (int i = 0; i < count_p; i++)
    {
        place_info = unmarshallPlaceInfo(th, minorVersion);
        ASSERT(!place_info.is_global());
        you.set_place_info(place_info);
    }

    typedef std::set<std::string> string_set;
    typedef std::pair<string_set::iterator, bool> ssipair;
    unmarshall_container(th, you.uniq_map_tags,
                         (ssipair (string_set::*)(const std::string &))
                         &string_set::insert,
                         unmarshallStringNoMax);
    unmarshall_container(th, you.uniq_map_names,
                         (ssipair (string_set::*)(const std::string &))
                         &string_set::insert,
                         unmarshallStringNoMax);

    read_level_connectivity(th);
}

static void tag_read_lost_monsters(reader &th)
{
    the_lost_ones.clear();
    unmarshallMap(th, the_lost_ones,
                  unmarshall_level_id, unmarshall_follower_list);
}

static void tag_read_lost_items(reader &th)
{
    transiting_items.clear();

    unmarshallMap(th, transiting_items,
                  unmarshall_level_id, unmarshall_item_list);
}

static void tag_read_game_state(reader &th)
{
    crawl_state.type = (game_type) unmarshallByte(th);
}

template <typename Z>
static int _last_used_index(const Z &thinglist, int max_things)
{
    for (int i = max_things - 1; i >= 0; --i)
        if (thinglist[i].defined())
            return (i + 1);
    return (0);
}

// ------------------------------- level tags ---------------------------- //

static void tag_construct_level(writer &th)
{
    marshallByte(th, env.floor_colour);
    marshallByte(th, env.rock_colour);

    marshallInt(th, env.level_flags);

    marshallInt(th, you.elapsed_time);
    marshallCoord(th, you.pos());

    // Map grids.
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    marshallInt(th, env.turns_on_level);

    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallByte(th, grd[count_x][count_y]);
            marshallMapCell(th, env.map_knowledge[count_x][count_y]);
            marshallInt(th, env.pgrid[count_x][count_y]);
        }

    _run_length_encode(th, marshallByte, env.grid_colours, GXM, GYM);

    // how many clouds?
    const int nc = _last_used_index(env.cloud, MAX_CLOUDS);
    marshallShort(th, nc);
    for (int i = 0; i < nc; i++)
    {
        marshallByte(th, env.cloud[i].type);
        if (env.cloud[i].type == CLOUD_NONE)
            continue;
        marshallByte(th, env.cloud[i].pos.x);
        marshallByte(th, env.cloud[i].pos.y);
        marshallShort(th, env.cloud[i].decay);
        marshallByte(th, env.cloud[i].spread_rate);
        marshallByte(th, env.cloud[i].whose);
        marshallByte(th, env.cloud[i].killer);
        marshallShort(th, env.cloud[i].colour);
        marshallString(th, env.cloud[i].name);
        marshallString(th, env.cloud[i].tile);
    }

    // how many shops?
    const int ns = _last_used_index(env.shop, MAX_SHOPS);
    marshallShort(th, ns);
    for (int i = 0; i < ns; i++)
    {
        marshallByte(th, env.shop[i].type);
        if (env.shop[i].type == SHOP_UNASSIGNED)
            continue;
        marshallByte(th, env.shop[i].keeper_name[0]);
        marshallByte(th, env.shop[i].keeper_name[1]);
        marshallByte(th, env.shop[i].keeper_name[2]);
        marshallByte(th, env.shop[i].pos.x);
        marshallByte(th, env.shop[i].pos.y);
        marshallByte(th, env.shop[i].greed);
        marshallByte(th, env.shop[i].level);
    }

    marshallCoord(th, env.sanctuary_pos);
    marshallByte(th, env.sanctuary_time);

    marshallInt(th, env.spawn_random_rate);

    env.markers.write(th);
    env.properties.write(th);

    marshallInt(th, you.dactions.size());

    // Save heightmap, if present.
    marshallByte(th, !!env.heightmap.get());
    if (env.heightmap.get())
    {
        grid_heightmap &heightmap(*env.heightmap);
        for (rectangle_iterator ri(0); ri; ++ri)
            marshallShort(th, heightmap(*ri));
    }

    marshallInt(th, env.forest_awoken_until);
    marshall_level_vault_data(th);
    marshallInt(th, env.density);
}

void marshallItem(writer &th, const item_def &item)
{
    marshallByte(th, item.base_type);
    if (item.base_type == OBJ_UNASSIGNED)
        return;

    marshallByte(th, item.sub_type);
    marshallShort(th, item.plus);
    marshallShort(th, item.plus2);
    marshallInt(th, item.special);
    marshallShort(th, item.quantity);

    marshallByte(th, item.colour);
    marshallByte(th, item.rnd);
    marshallShort(th, item.pos.x);
    marshallShort(th, item.pos.y);
    marshallInt(th, item.flags);

    marshallShort(th, item.link);
    if (item.pos.x >= 0 && item.pos.y >= 0)
        marshallShort(th, igrd(item.pos));  //  unused
    else
        marshallShort(th, -1); // unused

    marshallByte(th, item.slot);

    marshallShort(th, item.orig_place);
    marshallShort(th, item.orig_monnum);
    marshallString(th, item.inscription.c_str(), 80);

    item.props.write(th);
}

void unmarshallItem(reader &th, item_def &item)
{
    item.base_type   = static_cast<object_class_type>(unmarshallByte(th));
    if (item.base_type == OBJ_UNASSIGNED)
        return;
    item.sub_type    = unmarshallUByte(th);
    item.plus        = unmarshallShort(th);
    item.plus2       = unmarshallShort(th);
    item.special     = unmarshallInt(th);
    item.quantity    = unmarshallShort(th);
    item.colour      = unmarshallUByte(th);
    item.rnd         = unmarshallUByte(th);

    item.pos.x       = unmarshallShort(th);
    item.pos.y       = unmarshallShort(th);
    item.flags       = unmarshallInt(th);
    item.link        = unmarshallShort(th);

    unmarshallShort(th);  // igrd[item.x][item.y] -- unused

    item.slot        = unmarshallByte(th);

    item.orig_place  = unmarshallShort(th);
    item.orig_monnum = unmarshallShort(th);
    item.inscription = unmarshallString(th, 80);

    item.props.clear();
    item.props.read(th);

    // Fixup artefact props to handle reloading items when the new version
    // of Crawl has more artefact props.
    if (is_artefact(item))
        artefact_fixup_props(item);
}

#define MAP_SERIALIZE_FLAGS_MASK 3
#define MAP_SERIALIZE_FLAGS_8 1
#define MAP_SERIALIZE_FLAGS_16 2
#define MAP_SERIALIZE_FLAGS_32 3

#define MAP_SERIALIZE_FEATURE 4
#define MAP_SERIALIZE_FEATURE_COLOUR 8
#define MAP_SERIALIZE_ITEM 0x10
#define MAP_SERIALIZE_CLOUD 0x20
#define MAP_SERIALIZE_MONSTER 0x40
#define MAP_SERIALIZE_CLOUD_COLOUR 0x80

void marshallMapCell(writer &th, const map_cell &cell)
{
    unsigned flags = 0;

    if (cell.flags > 0xffff)
        flags |= MAP_SERIALIZE_FLAGS_32;
    else if (cell.flags > 0xff)
        flags |= MAP_SERIALIZE_FLAGS_16;
    else if (cell.flags)
        flags |= MAP_SERIALIZE_FLAGS_8;

    if (cell.feat() != DNGN_UNSEEN)
        flags |= MAP_SERIALIZE_FEATURE;

    if (cell.feat_colour())
        flags |= MAP_SERIALIZE_FEATURE_COLOUR;

    if (cell.cloud() != CLOUD_NONE)
        flags |= MAP_SERIALIZE_CLOUD;

    if (cell.cloud_colour())
        flags |= MAP_SERIALIZE_CLOUD_COLOUR;

    if (cell.item())
        flags |= MAP_SERIALIZE_ITEM;

    if (cell.monster() != MONS_NO_MONSTER && !cell.detected_monster())
        flags |= MAP_SERIALIZE_MONSTER;

    marshallUnsigned(th, flags);

    switch(flags & MAP_SERIALIZE_FLAGS_MASK)
    {
    case MAP_SERIALIZE_FLAGS_8:
        marshallByte(th, cell.flags);
        break;
    case MAP_SERIALIZE_FLAGS_16:
        marshallShort(th, cell.flags);
        break;
    case MAP_SERIALIZE_FLAGS_32:
        marshallInt(th, cell.flags);
        break;
    }

    if (flags & MAP_SERIALIZE_FEATURE)
        marshallUnsigned(th, cell.feat());

    if (flags & MAP_SERIALIZE_FEATURE_COLOUR)
        marshallUnsigned(th, cell.feat_colour());

    if (flags & MAP_SERIALIZE_CLOUD)
        marshallUnsigned(th, cell.cloud());

    if (flags & MAP_SERIALIZE_CLOUD_COLOUR)
        marshallUnsigned(th, cell.cloud_colour());

    if (flags & MAP_SERIALIZE_ITEM)
        marshallItem(th, *cell.item());

    if (flags & MAP_SERIALIZE_MONSTER)
        marshallMonsterInfo(th, *cell.monsterinfo());
    else if (cell.detected_monster())
        marshallUnsigned(th, cell.monster());
}

void unmarshallMapCell(reader &th, map_cell& cell)
{
    unsigned flags = unmarshallUnsigned(th);
    unsigned cell_flags = 0;

    cell.clear();

    switch(flags & MAP_SERIALIZE_FLAGS_MASK)
    {
    case MAP_SERIALIZE_FLAGS_8:
        cell_flags = unmarshallByte(th);
        break;
    case MAP_SERIALIZE_FLAGS_16:
        cell_flags = unmarshallShort(th);
        break;
    case MAP_SERIALIZE_FLAGS_32:
        cell_flags = unmarshallInt(th);
        break;
    }

    dungeon_feature_type feature = DNGN_UNSEEN;
    unsigned feat_colour = 0;

    if (flags & MAP_SERIALIZE_FEATURE)
        feature = (dungeon_feature_type)unmarshallUnsigned(th);
#if TAG_MAJOR_VERSION == 31
    if (flags & MAP_SERIALIZE_FEATURE && th.getMinorVersion() < TAG_MINOR_GRATE)
        if (feature >= DNGN_GRATE && feature < DNGN_GRANITE_STATUE)
            feature = (dungeon_feature_type)(feature + 1);
#endif

    if (flags & MAP_SERIALIZE_FEATURE_COLOUR)
        feat_colour = unmarshallUnsigned(th);

    cell.set_feature(feature, feat_colour);

    cloud_type cloud = CLOUD_NONE;
    unsigned cloud_colour = 0;
    if (flags & MAP_SERIALIZE_CLOUD)
        cloud = (cloud_type)unmarshallUnsigned(th);

    if (flags & MAP_SERIALIZE_CLOUD_COLOUR)
        cloud_colour = unmarshallUnsigned(th);

    cell.set_cloud(cloud, cloud_colour);

    if (flags & MAP_SERIALIZE_ITEM)
    {
        item_def item;
        unmarshallItem(th, item);
        cell.set_item(item, false);
    }

    if (flags & MAP_SERIALIZE_MONSTER)
    {
        monster_info mi;
        unmarshallMonsterInfo(th, mi);
        cell.set_monster(mi);
    }
    else if (cell_flags & MAP_DETECTED_MONSTER)
        cell.set_detected_monster((monster_type)unmarshallUnsigned(th));

    // set this last so the other sets don't override this
    cell.flags = cell_flags;
}

static void tag_construct_level_items(writer &th)
{
    // how many traps?
    const int nt = _last_used_index(env.trap, MAX_TRAPS);
    marshallShort(th, nt);
    for (int i = 0; i < nt; ++i)
    {
        marshallByte(th, env.trap[i].type);
        if (env.trap[i].type == TRAP_UNASSIGNED)
            continue;
        marshallCoord(th, env.trap[i].pos);
        marshallShort(th, env.trap[i].ammo_qty);
    }

    // how many items?
    const int ni = _last_used_index(mitm, MAX_ITEMS);
    marshallShort(th, ni);
    for (int i = 0; i < ni; ++i)
        marshallItem(th, mitm[i]);
}

static void marshall_mon_enchant(writer &th, const mon_enchant &me)
{
    marshallShort(th, me.ench);
    marshallShort(th, me.degree);
    marshallShort(th, me.who);
    marshallShort(th, me.duration);
    marshallShort(th, me.maxduration);
}

static mon_enchant unmarshall_mon_enchant(reader &th)
{
    mon_enchant me;
    me.ench        = static_cast<enchant_type>(unmarshallShort(th));
    me.degree      = unmarshallShort(th);
    me.who         = static_cast<kill_category>(unmarshallShort(th));
    me.duration    = unmarshallShort(th);
    me.maxduration = unmarshallShort(th);
    return (me);
}

void marshallMonster(writer &th, const monster& m)
{
    if (!m.alive())
    {
        marshallShort(th, MONS_NO_MONSTER);
        return;
    }

    marshallShort(th, m.type);
    marshallString(th, m.mname);
    marshallByte(th, m.ac);
    marshallByte(th, m.ev);
    marshallByte(th, m.hit_dice);
    marshallByte(th, m.speed);
    marshallByte(th, m.speed_increment);
    marshallByte(th, m.behaviour);
    marshallByte(th, m.pos().x);
    marshallByte(th, m.pos().y);
    marshallByte(th, m.target.x);
    marshallByte(th, m.target.y);
    marshallCoord(th, m.patrol_point);
    int help = m.travel_target;
    marshallByte(th, help);

    marshallShort(th, m.travel_path.size());
    for (unsigned int i = 0; i < m.travel_path.size(); i++)
        marshallCoord(th, m.travel_path[i]);

    marshallInt(th, m.flags);
    marshallInt(th, m.experience);

    marshallShort(th, m.enchantments.size());
    for (mon_enchant_list::const_iterator i = m.enchantments.begin();
         i != m.enchantments.end(); ++i)
    {
        marshall_mon_enchant(th, i->second);
    }
    marshallByte(th, m.ench_countdown);

    marshallShort(th, m.hit_points);
    marshallShort(th, m.max_hit_points);
    marshallShort(th, m.number);
    marshallShort(th, m.base_monster);
    marshallShort(th, m.colour);

    for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
        marshallShort(th, m.inv[j]);

    marshallSpells(th, m.spells);
    marshallByte(th, m.god);
    marshallByte(th, m.attitude);
    marshallShort(th, m.foe);
    marshallShort(th, m.damage_friendly);
    marshallShort(th, m.damage_total);

    if (mons_is_ghost_demon(m.type))
    {
        // *Must* have ghost field set.
        ASSERT(m.ghost.get());
        marshallGhost(th, *m.ghost);
    }

    m.props.write(th);
}

void marshallMonsterInfo(writer &th, const monster_info& mi)
{
    marshallCoord(th, mi.pos);
    marshallUnsigned(th, mi.mb);
    marshallString(th, mi.mname);
    marshallUnsigned(th, mi.type);
    marshallUnsigned(th, mi.base_type);
    marshallUnsigned(th, mi.number);
    marshallUnsigned(th, mi.colour);
    marshallUnsigned(th, mi.attitude);
    marshallUnsigned(th, mi.dam);
    marshallUnsigned(th, mi.fire_blocker);
    marshallString(th, mi.description);
    marshallString(th, mi.quote);
    marshallUnsigned(th, mi.fly);
}

void unmarshallMonsterInfo(reader &th, monster_info& mi)
{
    mi.pos = unmarshallCoord(th);
    unmarshallUnsigned(th, mi.mb);
    mi.mname = unmarshallString(th);
    unmarshallUnsigned(th, mi.type);
    unmarshallUnsigned(th, mi.base_type);
    unmarshallUnsigned(th, mi.number);
    unmarshallUnsigned(th, mi.colour);
    unmarshallUnsigned(th, mi.attitude);
#if TAG_MAJOR_VERSION == 31
    if (th.getMinorVersion() < TAG_MINOR_ATT_SWAP)
    {
        if (mi.attitude == ATT_GOOD_NEUTRAL)
            mi.attitude = ATT_STRICT_NEUTRAL;
        else if (mi.attitude == ATT_STRICT_NEUTRAL)
            mi.attitude = ATT_GOOD_NEUTRAL;
    }
#endif
    unmarshallUnsigned(th, mi.dam);
    unmarshallUnsigned(th, mi.fire_blocker);
    mi.description = unmarshallString(th);
    mi.quote = unmarshallString(th);
    unmarshallUnsigned(th, mi.fly);
}

static void tag_construct_level_monsters(writer &th)
{
    // how many mons_alloc?
    marshallByte(th, MAX_MONS_ALLOC);
    for (int i = 0; i < MAX_MONS_ALLOC; ++i)
        marshallShort(th, env.mons_alloc[i]);

    // how many monsters?
    const int nm = _last_used_index(menv, MAX_MONSTERS);
    marshallShort(th, nm);
    // how many monster inventory slots?
    marshallByte(th, NUM_MONSTER_SLOTS);

    for (int i = 0; i < nm; i++)
    {
        monster& m(menv[i]);

#if defined(DEBUG) || defined(DEBUG_MONS_SCAN)
        if (m.type != MONS_NO_MONSTER)
        {
            if (invalid_monster_type(m.type))
            {
                mprf(MSGCH_ERROR, "Marshalled monster #%d %s",
                     i, m.name(DESC_PLAIN, true).c_str());
            }
            if (!in_bounds(m.pos()))
            {
                mprf(MSGCH_ERROR,
                     "Marshalled monster #%d %s out of bounds at (%d, %d)",
                     i, m.name(DESC_PLAIN, true).c_str(),
                     m.pos().x, m.pos().y);
            }
        }
#endif
        marshallMonster(th, m);
    }
}

void tag_construct_level_tiles(writer &th)
{
#ifdef USE_TILE
    unsigned int rle_count = 0; // for run-length encoding
    unsigned int tile = 0;
    unsigned int last_tile = 0;

    marshallBoolean(th, true);
    // Legacy version number.
    marshallShort(th, 0);

    // Map grids.
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    tile = env.tile_bk_bg[0][0];
    //  bg first
    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            last_tile = tile;
            tile = env.tile_bk_bg[count_x][count_y];

            if (tile == last_tile)
            {
                rle_count++;
                if (rle_count == 0x100)
                {
                    marshallInt(th, last_tile);
                    marshallUByte(th, 0xFF);
                    rle_count = 1;
                }
            }
            else
            {
                marshallInt(th, last_tile);
                // Note: the unsigned char tile count gets streamed
                // as a signed char here.  It gets read back into
                // an unsigned char in the read function.
                marshallUByte(th, rle_count);
                rle_count = 1;
            }
        }

    marshallInt(th, tile);
    marshallUByte(th, rle_count);

    // fg
    tile = env.tile_bk_fg[0][0];
    rle_count = 0;
    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            last_tile = tile;
            tile = env.tile_bk_fg[count_x][count_y];

            if (tile == last_tile)
            {
                rle_count++;
                if (rle_count == 0x100)
                {
                    marshallInt(th, last_tile);
                    marshallUByte(th, (char)0xFF);
                    rle_count = 1;
                }
            }
            else
            {
                marshallInt(th, last_tile);
                marshallUByte(th, rle_count);
                rle_count = 1;
            }
        }

    marshallInt(th, tile);
    marshallUByte(th, rle_count);

    // flavour
    marshallShort(th, env.tile_default.wall);
    marshallShort(th, env.tile_default.floor);
    marshallShort(th, env.tile_default.special);

    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallShort(th, env.tile_flv[count_x][count_y].wall);
            marshallShort(th, env.tile_flv[count_x][count_y].floor);
            marshallShort(th, env.tile_flv[count_x][count_y].special);
            marshallShort(th, env.tile_flv[count_x][count_y].feat);
        }

    mcache.construct(th);

#else
    marshallBoolean(th, false);
#endif
}

static void tag_read_level(reader &th, int minorVersion)
{

    env.floor_colour = unmarshallUByte(th);
    env.rock_colour  = unmarshallUByte(th);

    env.level_flags  = unmarshallInt(th);

    env.elapsed_time = unmarshallInt(th);
    env.old_player_pos = unmarshallCoord(th);

    // Map grids.
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);

    env.turns_on_level = unmarshallInt(th);

    for (int i = 0; i < gx; i++)
        for (int j = 0; j < gy; j++)
        {
            grd[i][j] = static_cast<dungeon_feature_type>(unmarshallUByte(th));
#if TAG_MAJOR_VERSION == 31
            if (minorVersion < TAG_MINOR_GRATE)
                if (grd[i][j] >= DNGN_GRATE && grd[i][j] < DNGN_GRANITE_STATUE)
                    grd[i][j] = (dungeon_feature_type)(grd[i][j] + 1);
#endif

            unmarshallMapCell(th, env.map_knowledge[i][j]);
            env.map_knowledge[i][j].flags &=~ MAP_VISIBLE_FLAG;
            env.pgrid[i][j] = unmarshallInt(th);

            mgrd[i][j] = NON_MONSTER;
            env.cgrid[i][j] = EMPTY_CLOUD;
            env.tgrid[i][j] = NON_ENTITY;
        }

    env.grid_colours.init(BLACK);
    _run_length_decode(th, unmarshallByte, env.grid_colours, GXM, GYM);

    env.cloud_no = 0;

    // how many clouds?
    const int num_clouds = unmarshallShort(th);
    ASSERT(num_clouds >= 0 && num_clouds <= MAX_CLOUDS);
    for (int i = 0; i < num_clouds; i++)
    {
        env.cloud[i].type  = static_cast<cloud_type>(unmarshallByte(th));
        if (env.cloud[i].type == CLOUD_NONE)
            continue;
        env.cloud[i].pos.x = unmarshallByte(th);
        env.cloud[i].pos.y = unmarshallByte(th);
        env.cloud[i].decay = unmarshallShort(th);
        env.cloud[i].spread_rate = unmarshallUByte(th);
        env.cloud[i].whose = static_cast<kill_category>(unmarshallUByte(th));
        env.cloud[i].killer = static_cast<killer_type>(unmarshallUByte(th));
        env.cloud[i].colour = unmarshallShort(th);
        env.cloud[i].name = unmarshallString(th);
        env.cloud[i].tile = unmarshallString(th);
        ASSERT(in_bounds(env.cloud[i].pos));
        env.cgrid(env.cloud[i].pos) = i;
        env.cloud_no++;
    }
    for (int i = num_clouds; i < MAX_CLOUDS; i++)
        env.cloud[i].type = CLOUD_NONE;

    // how many shops?
    const int num_shops = unmarshallShort(th);
    ASSERT(num_shops >= 0 && num_shops <= MAX_SHOPS);
    for (int i = 0; i < num_shops; i++)
    {
        env.shop[i].type  = static_cast<shop_type>(unmarshallByte(th));
        if (env.shop[i].type == SHOP_UNASSIGNED)
            continue;
        env.shop[i].keeper_name[0] = unmarshallUByte(th);
        env.shop[i].keeper_name[1] = unmarshallUByte(th);
        env.shop[i].keeper_name[2] = unmarshallUByte(th);
        env.shop[i].pos.x = unmarshallByte(th);
        env.shop[i].pos.y = unmarshallByte(th);
        env.shop[i].greed = unmarshallByte(th);
        env.shop[i].level = unmarshallByte(th);
        env.tgrid(env.shop[i].pos) = i;
    }
    for (int i = num_shops; i < MAX_SHOPS; ++i)
        env.shop[i].type = SHOP_UNASSIGNED;

    env.sanctuary_pos  = unmarshallCoord(th);
    env.sanctuary_time = unmarshallByte(th);

    env.spawn_random_rate = unmarshallInt(th);

    env.markers.read(th, minorVersion);

    env.properties.clear();
    env.properties.read(th);

    env.dactions_done = unmarshallInt(th);

    // Restore heightmap
    env.heightmap.reset(NULL);
    const bool have_heightmap = unmarshallBoolean(th);
    if (have_heightmap)
    {
        env.heightmap.reset(new grid_heightmap);
        grid_heightmap &heightmap(*env.heightmap);
        for (rectangle_iterator ri(0); ri; ++ri)
            heightmap(*ri) = unmarshallShort(th);
    }

    env.forest_awoken_until = unmarshallInt(th);
    unmarshall_level_vault_data(th);
#if TAG_MAJOR_VERSION == 31
    if (minorVersion < TAG_MINOR_DENSITY)
        env.density = 0;
    else
#endif
    env.density = unmarshallInt(th);
}

static void tag_read_level_items(reader &th, int minorVersion)
{
    // how many traps?
    const int trap_count = unmarshallShort(th);
    ASSERT(trap_count >= 0 && trap_count <= MAX_TRAPS);
    for (int i = 0; i < trap_count; ++i)
    {
        env.trap[i].type =
            static_cast<trap_type>(unmarshallUByte(th));
        if (env.trap[i].type == TRAP_UNASSIGNED)
            continue;
        env.trap[i].pos      = unmarshallCoord(th);
        env.trap[i].ammo_qty = unmarshallShort(th);
        env.tgrid(env.trap[i].pos) = i;
    }
    for (int i = trap_count; i < MAX_TRAPS; ++i)
        env.trap[i].type = TRAP_UNASSIGNED;

    // how many items?
    const int item_count = unmarshallShort(th);
    ASSERT(item_count >= 0 && item_count <= MAX_ITEMS);
    for (int i = 0; i < item_count; ++i)
        unmarshallItem(th, mitm[i]);
    for (int i = item_count; i < MAX_ITEMS; ++i)
        mitm[i].clear();

#ifdef DEBUG_ITEM_SCAN
    // There's no way to fix this, even with wizard commands, so get
    // rid of it when restoring the game.
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def &item(mitm[i]);

        if (item.pos.origin())
            item.clear();
    }
#endif
}

void unmarshallMonster(reader &th, monster& m)
{
    m.reset();

    m.type           = static_cast<monster_type>(unmarshallShort(th));
    if (m.type == MONS_NO_MONSTER)
        return;

    m.mname           = unmarshallString(th, 100);
    m.ac              = unmarshallByte(th);
    m.ev              = unmarshallByte(th);
    m.hit_dice        = unmarshallByte(th);
    m.speed           = unmarshallByte(th);
    // Avoid sign extension when loading files (Elethiomel's hang)
    m.speed_increment = unmarshallUByte(th);
    m.behaviour       = static_cast<beh_type>(unmarshallUByte(th));
    int x             = unmarshallByte(th);
    int y             = unmarshallByte(th);
    m.set_position(coord_def(x,y));
    m.target.x        = unmarshallByte(th);
    m.target.y        = unmarshallByte(th);

    m.patrol_point    = unmarshallCoord(th);

    int help = unmarshallByte(th);
    m.travel_target = static_cast<montravel_target_type>(help);

    const int len = unmarshallShort(th);
    for (int i = 0; i < len; ++i)
        m.travel_path.push_back(unmarshallCoord(th));

    m.flags      = unmarshallInt(th);
    m.experience = unmarshallInt(th);

    m.enchantments.clear();
    const int nenchs = unmarshallShort(th);
    for (int i = 0; i < nenchs; ++i)
    {
        mon_enchant me = unmarshall_mon_enchant(th);
        m.enchantments[me.ench] = me;
    }
    m.ench_countdown = unmarshallByte(th);

    m.hit_points     = unmarshallShort(th);
    m.max_hit_points = unmarshallShort(th);
    m.number         = unmarshallShort(th);
    m.base_monster   = static_cast<monster_type>(unmarshallShort(th));
    m.colour         = unmarshallShort(th);

    for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
        m.inv[j] = unmarshallShort(th);

    unmarshallSpells(th, m.spells);

    m.god      = static_cast<god_type>(unmarshallByte(th));
    m.attitude = static_cast<mon_attitude_type>(unmarshallByte(th));
#if TAG_MAJOR_VERSION == 31
    if (th.getMinorVersion() < TAG_MINOR_ATT_SWAP)
    {
        if (m.attitude == ATT_GOOD_NEUTRAL)
            m.attitude = ATT_STRICT_NEUTRAL;
        else if (m.attitude == ATT_STRICT_NEUTRAL)
            m.attitude = ATT_GOOD_NEUTRAL;
    }
#endif
    m.foe      = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 31
    if (th.getMinorVersion() >= TAG_MINOR_XP_STEALING)
    {
#endif
    unmarshallShort(th);
    unmarshallShort(th);
#if TAG_MAJOR_VERSION == 31
    }
    else
        m.damage_friendly = m.damage_total = 0;
#endif

    if (mons_is_ghost_demon(m.type))
        m.set_ghost(unmarshallGhost(th, _tag_minor_version));

    m.props.clear();
    m.props.read(th);

    m.check_speed();
}

static void tag_read_level_monsters(reader &th, int minorVersion)
{
    int i;
    int count, icount;

    for (i = 0; i < MAX_MONSTERS; i++)
        menv[i].reset();

    // how many mons_alloc?
    count = unmarshallByte(th);
    ASSERT(count >= 0);
    for (i = 0; i < count && i < MAX_MONS_ALLOC; ++i)
        env.mons_alloc[i] = static_cast<monster_type>(unmarshallShort(th));
    for (i = MAX_MONS_ALLOC; i < count; ++i)
        unmarshallShort(th);
    for (i = count; i < MAX_MONS_ALLOC; ++i)
        env.mons_alloc[i] = MONS_NO_MONSTER;

    // how many monsters?
    count = unmarshallShort(th);
    ASSERT(count >= 0 && count <= MAX_MONSTERS);
    // how many monster inventory slots?
    icount = unmarshallByte(th);

    for (i = 0; i < count; i++)
    {
        monster& m = menv[i];
        unmarshallMonster(th, m);

        // place monster
        if (m.alive())
        {
#if defined(DEBUG) || defined(DEBUG_MONS_SCAN)
            if (invalid_monster_type(m.type))
            {
                mprf(MSGCH_ERROR, "Unmarshalled monster #%d %s",
                     i, m.name(DESC_PLAIN, true).c_str());
            }
            if (!in_bounds(m.pos()))
            {
                mprf(MSGCH_ERROR,
                     "Unmarshalled monster #%d %s out of bounds at (%d, %d)",
                     i, m.name(DESC_PLAIN, true).c_str(),
                     m.pos().x, m.pos().y);
            }
            int midx = mgrd(m.pos());
            if (midx != NON_MONSTER)
                mprf(MSGCH_ERROR, "(%d, %d) for %s already occupied by %s",
                     m.pos().x, m.pos().y,
                     m.name(DESC_PLAIN, true).c_str(),
                     menv[midx].name(DESC_PLAIN, true).c_str());
#endif
            mgrd(m.pos()) = i;
        }
    }
}

void tag_read_level_tiles(reader &th)
{
    if (!unmarshallBoolean(th))
    {
        tag_missing_level_tiles();
        return;
    }
#ifdef USE_TILE
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
        {
            env.tile_bk_bg[i][j] = 0;
            env.tile_bk_fg[i][j] = 0;
        }

    unsigned int rle_count = 0;
    unsigned int tile = 0;

    int ver = unmarshallShort(th);
    UNUSED(ver);

    // Map grids.
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);

    // BG first
    for (int i = 0; i < gx; i++)
        for (int j = 0; j < gy; j++)
        {
            if (rle_count == 0)
            {
                tile      = unmarshallInt(th);
                rle_count = unmarshallUByte(th);
            }
            env.tile_bk_bg[i][j] = tile;
            rle_count--;
        }

    // FG
    rle_count = 0;
    for (int i = 0; i < gx; i++)
        for (int j = 0; j < gy; j++)
        {
            if (rle_count == 0)
            {
                tile      = unmarshallInt(th);
                rle_count = unmarshallUByte(th);
            }
            env.tile_bk_fg[i][j] = tile;
            rle_count--;
        }

    // flavour
    env.tile_default.wall = unmarshallShort(th);
    env.tile_default.floor = unmarshallShort(th);
    env.tile_default.special = unmarshallShort(th);

    for (int x = 0; x < gx; x++)
        for (int y = 0; y < gy; y++)
        {
            env.tile_flv[x][y].wall    = unmarshallShort(th);
            env.tile_flv[x][y].floor   = unmarshallShort(th);
            env.tile_flv[x][y].special = unmarshallShort(th);
            env.tile_flv[x][y].feat   = unmarshallShort(th);
        }

    mcache.read(th);
#else
    // Snarf all remaining data, throwing it out.
    // This can happen only when loading in console a save from tiles.
    // It's a data loss bug that needs to be fixed.
    try
    {
        while (1)
            unmarshallByte(th);
    }
    catch (short_read_exception E)
    {
    }
#endif
}

static void tag_missing_level_tiles()
{
#ifdef USE_TILE
    tile_init_default_flavour();
    tile_clear_flavour();

    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
        {
            coord_def gc(i, j);
            tileidx_t fg, bg;
            tileidx_from_map_cell(&fg, &bg, env.map_knowledge(gc));

            env.tile_bk_bg(gc) = bg;
            env.tile_bk_fg(gc) = fg;
        }

    mcache.clear_all();

    tile_new_level(true);
#endif
}

// ------------------------------- ghost tags ---------------------------- //

static void marshallResists(writer &th, const mon_resist_def &res)
{
    marshallByte(th, res.elec);
    marshallByte(th, res.poison);
    marshallByte(th, res.fire);
    marshallByte(th, res.steam);
    marshallByte(th, res.cold);
    marshallByte(th, res.hellfire);
    marshallByte(th, res.asphyx);
    marshallByte(th, res.acid);
    marshallByte(th, res.sticky_flame);
    marshallByte(th, res.rotting);
    marshallByte(th, res.pierce);
    marshallByte(th, res.slice);
    marshallByte(th, res.bludgeon);
}

static void unmarshallResists(reader &th, mon_resist_def &res,
                              int minorVersion)
{
    res.elec         = unmarshallByte(th);
    res.poison       = unmarshallByte(th);
    res.fire         = unmarshallByte(th);
    res.steam        = unmarshallByte(th);
    res.cold         = unmarshallByte(th);
    res.hellfire     = unmarshallByte(th);
    res.asphyx       = unmarshallByte(th);
    res.acid         = unmarshallByte(th);
    res.sticky_flame = unmarshallByte(th);
    res.rotting      = unmarshallByte(th);
    res.pierce       = unmarshallByte(th);
    res.slice        = unmarshallByte(th);
    res.bludgeon     = unmarshallByte(th);
}

static void marshallSpells(writer &th, const monster_spells &spells)
{
    for (int j = 0; j < NUM_MONSTER_SPELL_SLOTS; ++j)
        marshallShort(th, spells[j]);
}

static void unmarshallSpells(reader &th, monster_spells &spells)
{
    for (int j = 0; j < NUM_MONSTER_SPELL_SLOTS; ++j)
        spells[j] = static_cast<spell_type>(unmarshallShort(th));
}

static void marshallGhost(writer &th, const ghost_demon &ghost)
{
    marshallString(th, ghost.name.c_str(), 20);

    marshallShort(th, ghost.species);
    marshallShort(th, ghost.job);
    marshallByte(th, ghost.religion);
    marshallShort(th, ghost.best_skill);
    marshallShort(th, ghost.best_skill_level);
    marshallShort(th, ghost.xl);
    marshallShort(th, ghost.max_hp);
    marshallShort(th, ghost.ev);
    marshallShort(th, ghost.ac);
    marshallShort(th, ghost.damage);
    marshallShort(th, ghost.speed);
    marshallByte(th, ghost.see_invis);
    marshallShort(th, ghost.brand);
    marshallShort(th, ghost.att_type);
    marshallShort(th, ghost.att_flav);

    marshallResists(th, ghost.resists);

    marshallByte(th, ghost.spellcaster);
    marshallByte(th, ghost.cycle_colours);
    marshallByte(th, ghost.colour);
    marshallShort(th, ghost.fly);

    marshallSpells(th, ghost.spells);
}

static ghost_demon unmarshallGhost(reader &th, int minorVersion)
{
    ghost_demon ghost;

    ghost.name             = unmarshallString(th, 20);
    ghost.species          = static_cast<species_type>(unmarshallShort(th));
    ghost.job              = static_cast<job_type>(unmarshallShort(th));
    ghost.religion         = static_cast<god_type>(unmarshallByte(th));
    ghost.best_skill       = static_cast<skill_type>(unmarshallShort(th));
    ghost.best_skill_level = unmarshallShort(th);
    ghost.xl               = unmarshallShort(th);
    ghost.max_hp           = unmarshallShort(th);
    ghost.ev               = unmarshallShort(th);
    ghost.ac               = unmarshallShort(th);
    ghost.damage           = unmarshallShort(th);
    ghost.speed            = unmarshallShort(th);
    ghost.see_invis        = unmarshallByte(th);
    ghost.brand            = static_cast<brand_type>(unmarshallShort(th));

    ghost.att_type = static_cast<mon_attack_type>(unmarshallShort(th));
    ghost.att_flav = static_cast<mon_attack_flavour>(unmarshallShort(th));

    unmarshallResists(th, ghost.resists, minorVersion);

    ghost.spellcaster      = unmarshallByte(th);
    ghost.cycle_colours    = unmarshallByte(th);
    ghost.colour           = unmarshallByte(th);

    ghost.fly              = static_cast<flight_type>(unmarshallShort(th));

    unmarshallSpells(th, ghost.spells);

    return (ghost);
}

static void tag_construct_ghost(writer &th)
{
    // How many ghosts?
    marshallShort(th, ghosts.size());

    for (int i = 0, size = ghosts.size(); i < size; ++i)
        marshallGhost(th, ghosts[i]);
}

static void tag_read_ghost(reader &th, int minorVersion)
{
    int nghosts = unmarshallShort(th);

    if (nghosts < 1 || nghosts > MAX_GHOSTS)
        return;

    for (int i = 0; i < nghosts; ++i)
        ghosts.push_back(unmarshallGhost(th, minorVersion));
}
