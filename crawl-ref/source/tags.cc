/**
 * @file
 * @brief Auxiliary functions to make savefile versioning simpler.
**/

/*
   The marshalling and unmarshalling of data is done in big endian and
   is meant to keep savefiles cross-platform. Note also that the marshalling
   sizes are 1, 2, and 4 for byte, short, and int. If a strange platform
   with different sizes of these basic types pops up, please sed it to fixed-
   width ones. For now, that wasn't done in order to keep things convenient.
*/

#include "AppHdr.h"

#include "tags.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>
#ifdef UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "abyss.h"
#include "act-iter.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "butcher.h"
#if TAG_MAJOR_VERSION == 34
 #include "cloud.h"
 #include "decks.h"
#endif
#include "colour.h"
#include "coordit.h"
#include "dbg-scan.h"
#include "describe.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "end.h"
#include "errors.h"
#include "ghost.h"
#include "god-abil.h" // just for the Ru sac penalty key
#include "god-companions.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "item-type-id-state-type.h"
#include "items.h"
#include "jobs.h"
#include "mapmark.h"
#include "misc.h"
#include "mon-death.h"
#if TAG_MAJOR_VERSION == 34
 #include "mon-place.h"
 #include "mon-poly.h"
 #include "mon-tentacle.h"
 #include "mon-util.h"
#endif
#include "place.h"
#include "player-stats.h"
#include "prompt.h" // index_to_letter
#include "religion.h"
#include "skills.h"
#include "spl-wpnench.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tiledef-player.h"
#include "tilepick.h"
#include "tileview.h"
#ifdef USE_TILE
 #include "tilemcache.h"
#endif
#include "transform.h"
#include "unwind.h"
#include "version.h"

// defined in dgn-overview.cc
extern map<branch_type, set<level_id> > stair_level;
extern map<level_pos, shop_type> shops_present;
extern map<level_pos, god_type> altars_present;
extern map<level_pos, branch_type> portals_present;
extern map<level_pos, string> portal_notes;
extern map<level_id, string> level_annotations;
extern map<level_id, string> level_exclusions;
extern map<level_id, string> level_uniques;
extern set<pair<string, level_id> > auto_unique_annotations;

// defined in abyss.cc
extern abyss_state abyssal_state;

reader::reader(const string &_read_filename, int minorVersion)
    : _filename(_read_filename), _chunk(0), _pbuf(nullptr), _read_offset(0),
      _minorVersion(minorVersion), _safe_read(false)
{
    _file       = fopen_u(_filename.c_str(), "rb");
    opened_file = !!_file;
}

reader::reader(package *save, const string &chunkname, int minorVersion)
    : _file(0), _chunk(0), opened_file(false), _pbuf(0), _read_offset(0),
     _minorVersion(minorVersion), _safe_read(false)
{
    ASSERT(save);
    _chunk = new chunk_reader(save, chunkname);
}

reader::~reader()
{
    if (_chunk)
        delete _chunk;
    close();
}

void reader::close()
{
    if (opened_file && _file)
        fclose(_file);
    _file = nullptr;
}

void reader::advance(size_t offset)
{
    char junk[128];

    while (offset)
    {
        const size_t junklen = min(sizeof(junk), offset);
        offset -= junklen;
        read(junk, junklen);
    }
}

bool reader::valid() const
{
    return (_file && !feof(_file)) ||
           (_pbuf && _read_offset < _pbuf->size());
}

static NORETURN void _short_read(bool safe_read)
{
    if (!crawl_state.need_save || safe_read)
        throw short_read_exception();
    // Would be nice to name the save chunk here, but in interesting cases
    // we're reading a copy from memory (why?).
    die_noline("short read while reading save");
}

// Reads input in network byte order, from a file or buffer.
unsigned char reader::readByte()
{
    if (_file)
    {
        int b = fgetc(_file);
        if (b == EOF)
            _short_read(_safe_read);
        return b;
    }
    else if (_chunk)
    {
        unsigned char buf;
        if (_chunk->read(&buf, 1) != 1)
            _short_read(_safe_read);
        return buf;
    }
    else
    {
        if (_read_offset >= _pbuf->size())
            _short_read(_safe_read);
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
                _short_read(_safe_read);
        }
        else
            fseek(_file, (long)size, SEEK_CUR);
    }
    else if (_chunk)
    {
        if (_chunk->read(data, size) != size)
            _short_read(_safe_read);
    }
    else
    {
        if (_read_offset+size > _pbuf->size())
            _short_read(_safe_read);
        if (data && size)
            memcpy(data, &(*_pbuf)[_read_offset], size);

        _read_offset += size;
    }
}

int reader::getMinorVersion() const
{
    ASSERT(_minorVersion != TAG_MINOR_INVALID);
    return _minorVersion;
}

void reader::setMinorVersion(int minorVersion)
{
    _minorVersion = minorVersion;
}

void reader::fail_if_not_eof(const string &name)
{
    char dummy;
    if (_chunk ? _chunk->read(&dummy, 1) :
        _file ? (fgetc(_file) != EOF) :
        _read_offset >= _pbuf->size())
    {
        fail("Incomplete read of \"%s\" - aborting.", name.c_str());
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
static void tag_construct_companions(writer &th);
static void tag_read_you(reader &th);
static void tag_read_you_items(reader &th);
static void tag_read_you_dungeon(reader &th);
static void tag_read_lost_monsters(reader &th);
static void tag_read_lost_items(reader &th);
static void tag_read_companions(reader &th);

static void tag_construct_level(writer &th);
static void tag_construct_level_items(writer &th);
static void tag_construct_level_monsters(writer &th);
static void tag_construct_level_tiles(writer &th);
static void tag_read_level(reader &th);
static void tag_read_level_items(reader &th);
static void tag_read_level_monsters(reader &th);
static void tag_read_level_tiles(reader &th);
static void _regenerate_tile_flavour();
static void _draw_tiles();

static void tag_construct_ghost(writer &th);
static void tag_read_ghost(reader &th);

static void marshallGhost(writer &th, const ghost_demon &ghost);
static ghost_demon unmarshallGhost(reader &th);

static void marshallSpells(writer &, const monster_spells &);
static void unmarshallSpells(reader &, monster_spells &
#if TAG_MAJOR_VERSION == 34
                             , unsigned hd
#endif
);

static void marshallMonsterInfo (writer &, const monster_info &);
static void unmarshallMonsterInfo (reader &, monster_info &mi);
static void marshallMapCell (writer &, const map_cell &);
static void unmarshallMapCell (reader &, map_cell& cell);

template<typename T, typename T_iter, typename T_marshal>
static void marshall_iterator(writer &th, T_iter beg, T_iter end,
                              T_marshal marshal);
template<typename T, typename U>
static void unmarshall_vector(reader& th, vector<T>& vec, U T_unmarshall);

template<int SIZE>
void marshallFixedBitVector(writer& th, const FixedBitVector<SIZE>& arr);
template<int SIZE>
void unmarshallFixedBitVector(reader& th, FixedBitVector<SIZE>& arr);

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

// Optimized for short vectors that have only the first few bits set, and
// can have invalid length. For long ones you might want to do this
// differently to not lose 1/8 bits and speed.
template<int SIZE>
void marshallFixedBitVector(writer& th, const FixedBitVector<SIZE>& arr)
{
    int last_bit;
    for (last_bit = SIZE - 1; last_bit > 0; last_bit--)
        if (arr[last_bit])
            break;

    int i = 0;
    while (1)
    {
        uint8_t byte = 0;
        for (int j = 0; j < 7; j++)
            if (i < SIZE && arr[i++])
                byte |= 1 << j;
        if (i <= last_bit)
            marshallUByte(th, byte);
        else
        {
            marshallUByte(th, byte | 0x80);
            break;
        }
    }
}

template<int SIZE>
void unmarshallFixedBitVector(reader& th, FixedBitVector<SIZE>& arr)
{
    arr.reset();

    int i = 0;
    while (1)
    {
        uint8_t byte = unmarshallUByte(th);
        for (int j = 0; j < 7; j++)
            if (i < SIZE)
                arr.set(i++, !!(byte & (1 << j)));
        if (byte & 0x80)
            break;
    }
}

// FIXME: Kill this abomination - it will break!
template<typename T>
static void _marshall_as_int(writer& th, const T& t)
{
    marshallInt(th, static_cast<int>(t));
}

template <typename data>
void marshallSet(writer &th, const set<data> &s,
                 void (*marshall)(writer &, const data &))
{
    marshallInt(th, s.size());
    for (const data &elt : s)
        marshall(th, elt);
}

template<typename key, typename value>
void marshallMap(writer &th, const map<key,value>& data,
                 void (*key_marshall)(writer&, const key&),
                 void (*value_marshall)(writer&, const value&))
{
    marshallInt(th, data.size());
    for (const auto &entry : data)
    {
        key_marshall(th, entry.first);
        value_marshall(th, entry.second);
    }
}

template<typename T_iter, typename T_marshall_t>
static void marshall_iterator(writer &th, T_iter beg, T_iter end,
                              T_marshall_t T_marshall)
{
    marshallInt(th, distance(beg, end));
    while (beg != end)
    {
        T_marshall(th, *beg);
        ++beg;
    }
}

template<typename T, typename U>
static void unmarshall_vector(reader& th, vector<T>& vec, U T_unmarshall)
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

static unsigned short _pack(const level_id& id)
{
    return (static_cast<int>(id.branch) << 8) | (id.depth & 0xFF);
}

void marshall_level_id(writer& th, const level_id& id)
{
    marshallShort(th, _pack(id));
}

static void _marshall_level_id_set(writer& th, const set<level_id>& id)
{
    marshallSet(th, id, marshall_level_id);
}

// XXX: Redundant with level_pos.save()/load().
static void _marshall_level_pos(writer& th, const level_pos& lpos)
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
    const int len = unmarshallInt(th);
    key k;
    for (int i = 0; i < len; ++i)
    {
        k = key_unmarshall(th);
        pair<key, value> p(k, value_unmarshall(th));
        data.insert(p);
    }
}

template<typename T>
static T unmarshall_int_as(reader& th)
{
    return static_cast<T>(unmarshallInt(th));
}

#if TAG_MAJOR_VERSION == 34
level_id level_id::from_packed_place(unsigned short place)
#else
level_id _unpack(unsigned short place)
#endif
{
    level_id id;

    id.branch     = static_cast<branch_type>((place >> 8) & 0xFF);
    id.depth      = (int8_t)(place & 0xFF);

    return id;
}

level_id unmarshall_level_id(reader& th)
{
#if TAG_MAJOR_VERSION == 34
    return level_id::from_packed_place(unmarshallShort(th));
#else
    return _unpack(unmarshallShort(th));
#endif
}

static set<level_id> _unmarshall_level_id_set(reader& th)
{
    set<level_id> id;
    unmarshallSet(th, id, unmarshall_level_id);
    return id;
}

static level_pos _unmarshall_level_pos(reader& th)
{
    level_pos lpos;
    lpos.pos.x = unmarshallInt(th);
    lpos.pos.y = unmarshallInt(th);
    lpos.id    = unmarshall_level_id(th);
    return lpos;
}

void marshallCoord(writer &th, const coord_def &c)
{
    marshallInt(th, c.x);
    marshallInt(th, c.y);
}

coord_def unmarshallCoord(reader &th)
{
    coord_def c;
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_COORD_SERIALIZER
        && th.getMinorVersion() != TAG_MINOR_0_11)
    {
#endif
        c.x = unmarshallInt(th);
        c.y = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
    }
    else
    {
        c.x = unmarshallShort(th);
        c.y = unmarshallShort(th);
    }
#endif
    return c;
}

#if TAG_MAJOR_VERSION == 34
// Between TAG_MINOR_OPTIONAL_PARTS and TAG_MINOR_FIXED_CONSTRICTION
// we neglected to marshall the constricting[] map of monsters. Fix
// those up.
static void _fix_missing_constrictions()
{
    for (int i = -1; i < MAX_MONSTERS; ++i)
    {
        const actor* m = i < 0 ? (actor*)&you : (actor*)&menv[i];
        if (!m->alive())
            continue;
        if (!m->constricted_by)
            continue;
        actor *h = actor_by_mid(m->constricted_by);
        // Not a known bug, so don't fix this up.
        if (!h)
            continue;

        if (!h->constricting)
            h->constricting = new actor::constricting_t;
        if (h->constricting->find(m->mid) == h->constricting->end())
        {
            dprf("Fixing missing constriction for %s (mindex=%d mid=%d)"
                 " of %s (mindex=%d mid=%d)",
                 h->name(DESC_PLAIN, true).c_str(), h->mindex(), h->mid,
                 m->name(DESC_PLAIN, true).c_str(), m->mindex(), m->mid);

            (*h->constricting)[m->mid] = 0;
        }
    }
}
#endif

static void _marshall_constriction(writer &th, const actor *who)
{
    _marshall_as_int(th, who->held);
    marshallInt(th, who->constricted_by);
    marshallInt(th, who->escape_attempts);

    // Assumes an empty map is marshalled as just the int 0.
    const actor::constricting_t * const cmap = who->constricting;
    if (cmap)
        marshallMap(th, *cmap, _marshall_as_int<mid_t>, _marshall_as_int<int>);
    else
        marshallInt(th, 0);
}

static void _unmarshall_constriction(reader &th, actor *who)
{
    who->held = unmarshall_int_as<held_type>(th);
    who->constricted_by = unmarshallInt(th);
    who->escape_attempts = unmarshallInt(th);

    actor::constricting_t cmap;
    unmarshallMap(th, cmap, unmarshall_int_as<mid_t>, unmarshallInt);

    if (cmap.size() == 0)
        who->constricting = 0;
    else
        who->constricting = new actor::constricting_t(cmap);
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
void marshallString(writer &th, const string &data)
{
    size_t len = data.length();
    // A limit of 32K.
    if (len > SHRT_MAX)
        die("trying to marshall too long a string (len=%ld)", (long int)len);
    marshallShort(th, len);

    th.write(data.c_str(), len);
}

string unmarshallString(reader &th)
{
    char buffer[SHRT_MAX];

    short len = unmarshallShort(th);
    ASSERT(len >= 0);
    ASSERT(len <= (ssize_t)sizeof(buffer));

    th.read(buffer, len);

    return string(buffer, len);
}

// This one must stay with a 16 bit signed big-endian length tag, to allow
// older versions to browse and list newer saves.
static void marshallString2(writer &th, const string &data)
{
    marshallString(th, data);
}

static string unmarshallString2(reader &th)
{
    return unmarshallString(th);
}

// string -- 4 byte length, non-terminated string data.
void marshallString4(writer &th, const string &data)
{
    marshallInt(th, data.length());
    th.write(data.c_str(), data.length());
}
void unmarshallString4(reader &th, string& s)
{
    const int len = unmarshallInt(th);
    s.resize(len);
    if (len) th.read(&s.at(0), len);
}

// boolean (to avoid system-dependent bool implementations)
void marshallBoolean(writer &th, bool data)
{
    th.writeByte(data ? 1 : 0);
}

// boolean (to avoid system-dependent bool implementations)
bool unmarshallBoolean(reader &th)
{
    return th.readByte() != 0;
}

// Saving the date as a string so we're not reliant on a particular epoch.
string make_date_string(time_t in_date)
{
    if (in_date <= 0)
        return "";

    struct tm *date = TIME_FN(&in_date);

    return make_stringf(
              "%4d%02d%02d%02d%02d%02d%s",
              date->tm_year + 1900, date->tm_mon, date->tm_mday,
              date->tm_hour, date->tm_min, date->tm_sec,
              ((date->tm_isdst > 0) ? "D" : "S"));
}

static void marshallStringVector(writer &th, const vector<string> &vec)
{
    marshall_iterator(th, vec.begin(), vec.end(), marshallString);
}

static vector<string> unmarshallStringVector(reader &th)
{
    vector<string> vec;
    unmarshall_vector(th, vec, unmarshallString);
    return vec;
}

static monster_type unmarshallMonType(reader &th)
{
    monster_type x = static_cast<monster_type>(unmarshallShort(th));

    if (x >= MONS_NO_MONSTER)
        return x;

#if TAG_MAJOR_VERSION == 34
# define AXED(a) if (x > a) --x
    if (th.getMinorVersion() == TAG_MINOR_0_11)
    {
        AXED(MONS_KILLER_BEE); // killer bee larva
        AXED(MONS_SHADOW_IMP); // midge
        AXED(MONS_AGNES);      // Jozef
    }
#endif

    return x;
}

#if TAG_MAJOR_VERSION == 34
// yay marshalling inconsistencies
static monster_type unmarshallMonType_Info(reader &th)
{
    monster_type x = static_cast<monster_type>(unmarshallUnsigned(th));

    if (x >= MONS_NO_MONSTER)
        return x;

    if (th.getMinorVersion() == TAG_MINOR_0_11)
    {
        AXED(MONS_KILLER_BEE); // killer bee larva
        AXED(MONS_SHADOW_IMP); // midge
        AXED(MONS_AGNES);      // Jozef
    }

    return x;
}
#endif

static spell_type unmarshallSpellType(reader &th
#if TAG_MAJOR_VERSION == 34
                                      , bool mons = false
#endif
                                      )
{
    spell_type x = SPELL_NO_SPELL;
#if TAG_MAJOR_VERSION == 34
    if (!mons && th.getMinorVersion() < TAG_MINOR_SHORT_SPELL_TYPE)
        x = static_cast<spell_type>(unmarshallUByte(th));
    else
#endif
        x = static_cast<spell_type>(unmarshallShort(th));

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() == TAG_MINOR_0_11)
    {
        AXED(SPELL_DEBUGGING_RAY); // projected noise
        AXED(SPELL_HEAL_OTHER);    // summon greater holy
    }
#endif

    return x;
}

static dungeon_feature_type rewrite_feature(dungeon_feature_type x,
                                            int minor_version)
{
#if TAG_MAJOR_VERSION == 34
    if (minor_version == TAG_MINOR_0_11)
    {
        // turn old trees into...trees
        if (x == DNGN_OPEN_SEA)
            x = DNGN_TREE;
        else if (x >= DNGN_LAVA_SEA && x < 30)
            x = (dungeon_feature_type)(x - 1);
    }
    // turn mangroves into trees
    else if (minor_version < TAG_MINOR_MANGROVES && x == DNGN_OPEN_SEA)
        x = DNGN_TREE;
    else if (minor_version < TAG_MINOR_FIX_FEAT_SHIFT
             && x > DNGN_OPEN_SEA && x < DNGN_LAVA)
    {
        x = static_cast<dungeon_feature_type>(x - 1);
    }

    if (x >= DNGN_DRY_FOUNTAIN_BLUE && x <= DNGN_DRY_FOUNTAIN_BLOOD)
        x = DNGN_DRY_FOUNTAIN;

    if (x == DNGN_SEALED_DOOR && minor_version < TAG_MINOR_0_12)
        x = DNGN_CLOSED_DOOR;
    if (x == DNGN_BADLY_SEALED_DOOR)
        x = DNGN_SEALED_DOOR;
    if (x == DNGN_ESCAPE_HATCH_UP && player_in_branch(BRANCH_LABYRINTH))
        x = DNGN_EXIT_LABYRINTH;
    if (x == DNGN_DEEP_WATER && player_in_branch(BRANCH_SHOALS)
        && minor_version < TAG_MINOR_SHOALS_LITE)
    {
        x = DNGN_SHALLOW_WATER;
    }

    // ensure that killing TRJ opens the slime:$ vaults
    if (you.where_are_you == BRANCH_SLIME && you.depth == brdepth[BRANCH_SLIME]
        && minor_version < TAG_MINOR_SLIME_WALL_CLEAR
        && x == DNGN_STONE_WALL)
    {
        x = DNGN_CLEAR_STONE_WALL;
    }

#endif

    return x;
}

static dungeon_feature_type unmarshallFeatureType(reader &th)
{
    dungeon_feature_type x = static_cast<dungeon_feature_type>(unmarshallUByte(th));
    return rewrite_feature(x, th.getMinorVersion());
}

#if TAG_MAJOR_VERSION == 34
// yay marshalling inconsistencies
static dungeon_feature_type unmarshallFeatureType_Info(reader &th)
{
    dungeon_feature_type x = static_cast<dungeon_feature_type>(unmarshallUnsigned(th));
    x = rewrite_feature(x, th.getMinorVersion());

    // There was a period of time when this function (only this one, not
    // unmarshallFeatureType) lacked some of the conversions now done by
    // rewrite_feature. In case any saves were transferred through those
    // versions, replace bad features with DNGN_UNSEEN. Questionable, but
    // this is just map_knowledge so the impact should be low.
    return is_valid_feature_type(x) ? x : DNGN_UNSEEN;
}
#endif

#define CANARY     marshallUByte(th, 171)
#if TAG_MAJOR_VERSION == 34
#define EAT_CANARY do if (th.getMinorVersion() >= TAG_MINOR_CANARIES    \
                          && unmarshallUByte(th) != 171)                \
                   {                                                    \
                       die("save corrupted: canary gone");              \
                   } while (0)
#else
#define EAT_CANARY do if ( unmarshallUByte(th) != 171)                  \
                   {                                                    \
                       die("save corrupted: canary gone");              \
                   } while (0)
#endif

#if TAG_MAJOR_VERSION == 34
static void _ensure_entry(branch_type br)
{
    dungeon_feature_type entry = branches[br].entry_stairs;
    for (rectangle_iterator ri(1); ri; ++ri)
        if (grd(*ri) == entry)
            return;

    // Find primary upstairs.
    for (rectangle_iterator ri(1); ri; ++ri)
        if (grd(*ri) == DNGN_STONE_STAIRS_UP_I)
        {
            for (distance_iterator di(*ri); di; ++di)
                if (in_bounds(*di) && grd(*di) == DNGN_FLOOR)
                {
                    grd(*di) = entry; // No need to update LOS, etc.
                    // Announce the repair even in non-debug builds.
                    mprf(MSGCH_ERROR, "Placing missing branch entry: %s.",
                         dungeon_feature_name(entry));
                    return;
                }
            die("no floor to place a branch entrance");
        }
    die("no upstairs on %s???", level_id::current().describe().c_str());
}

static void _add_missing_branches()
{
    const level_id lc = level_id::current();

    // Could do all just in case, but this seems safer:
    if (brentry[BRANCH_VAULTS] == lc)
        _ensure_entry(BRANCH_VAULTS);
    if (brentry[BRANCH_ZOT] == lc)
        _ensure_entry(BRANCH_ZOT);
    if (lc == level_id(BRANCH_DEPTHS, 2) || lc == level_id(BRANCH_DUNGEON, 21))
        _ensure_entry(BRANCH_VESTIBULE);
    if (lc == level_id(BRANCH_DEPTHS, 3) || lc == level_id(BRANCH_DUNGEON, 24))
        _ensure_entry(BRANCH_PANDEMONIUM);
    if (lc == level_id(BRANCH_DEPTHS, 4) || lc == level_id(BRANCH_DUNGEON, 25))
        _ensure_entry(BRANCH_ABYSS);
    if (player_in_branch(BRANCH_VESTIBULE))
    {
        for (rectangle_iterator ri(0); ri; ++ri)
        {
            if (grd(*ri) == DNGN_STONE_ARCH)
            {
                map_marker *marker = env.markers.find(*ri, MAT_FEATURE);
                if (marker)
                {
                    map_feature_marker *featm =
                        dynamic_cast<map_feature_marker*>(marker);
                    // [ds] Ensure we're activating the correct feature
                    // markers. Feature markers are also used for other
                    // things, notably to indicate the return point from
                    // a labyrinth or portal vault.
                    switch (featm->feat)
                    {
                    case DNGN_ENTER_COCYTUS:
                    case DNGN_ENTER_DIS:
                    case DNGN_ENTER_GEHENNA:
                    case DNGN_ENTER_TARTARUS:
                        grd(*ri) = featm->feat;
                        dprf("opened %s", dungeon_feature_name(featm->feat));
                        env.markers.remove(marker);
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }
}
#endif


// Write a tagged chunk of data to the FILE*.
// tagId specifies what to write.
void tag_write(tag_type tagID, writer &outf)
{
    vector<unsigned char> buf;
    writer th(&buf);
    switch (tagID)
    {
    case TAG_CHR:
        tag_construct_char(th);
        break;
    case TAG_YOU:
        tag_construct_you(th);
        CANARY;
        tag_construct_you_items(th);
        CANARY;
        tag_construct_you_dungeon(th);
        CANARY;
        tag_construct_lost_monsters(th);
        CANARY;
        tag_construct_lost_items(th);
        CANARY;
        tag_construct_companions(th);
        break;
    case TAG_LEVEL:
        tag_construct_level(th);
        CANARY;
        tag_construct_level_items(th);
        CANARY;
        tag_construct_level_monsters(th);
        CANARY;
        tag_construct_level_tiles(th);
        break;
    case TAG_GHOST:
        tag_construct_ghost(th);
        break;
    default:
        // I don't know how to make that!
        break;
    }

    // make sure there is some data to write!
    if (buf.empty())
        return;

    // Write tag header.
    marshallInt(outf, buf.size());

    // Write tag data.
    outf.write(&buf[0], buf.size());
}

static void _shunt_monsters_out_of_walls()
{
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monster &m(menv[i]);
        if (m.alive() && in_bounds(m.pos()) && cell_is_solid(m.pos())
            && (grd(m.pos()) != DNGN_MALIGN_GATEWAY
                || mons_genus(m.type) != MONS_ELDRITCH_TENTACLE))
        {
            for (distance_iterator di(m.pos()); di; ++di)
                if (!actor_at(*di) && !cell_is_solid(*di))
                {
#if TAG_MAJOR_VERSION == 34
                    // Could have been a rock worm or a dryad.
                    if (m.type != MONS_GHOST)
#endif
                    mprf(MSGCH_ERROR, "Error: monster %s in %s at (%d,%d)",
                         m.name(DESC_PLAIN, true).c_str(),
                         dungeon_feature_name(grd(m.pos())),
                         m.pos().x, m.pos().y);
                    env.mgrid(m.pos()) = NON_MONSTER;
                    m.position = *di;
                    env.mgrid(*di) = i;
                    break;
                }
        }
    }
}

// Read a piece of data from inf into memory, then run the appropriate reader.
//
// minorVersion is available for any sub-readers that need it
void tag_read(reader &inf, tag_type tag_id)
{
    // Read header info and data
    vector<unsigned char> buf;
    const int data_size = unmarshallInt(inf);
    ASSERT(data_size >= 0);

    // Fetch data in one go
    buf.resize(data_size);
    inf.read(&buf[0], buf.size());

    // Ok, we have data now.
    reader th(buf, inf.getMinorVersion());
    switch (tag_id)
    {
    case TAG_YOU:
        tag_read_you(th);
        EAT_CANARY;
        tag_read_you_items(th);
        EAT_CANARY;
        tag_read_you_dungeon(th);
        EAT_CANARY;
        tag_read_lost_monsters(th);
        EAT_CANARY;
        tag_read_lost_items(th);
        EAT_CANARY;
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() >= TAG_MINOR_COMPANION_LIST)
#endif
        tag_read_companions(th);

        // If somebody SIGHUP'ed out of the skill menu with all skills disabled.
        // Doing this here rather in tag_read_you() because you.can_train()
        // requires the player's equipment be loaded.
        init_can_train();
        check_selected_skills();
        break;
    case TAG_LEVEL:
        tag_read_level(th);
        EAT_CANARY;
        tag_read_level_items(th);
        // We have to do this here because tag_read_level_monsters()
        // might kill an elsewhere Ilsuiw follower, which ends up calling
        // terrain.cc:_dgn_check_terrain_items, which checks mitm.
        link_items();
        EAT_CANARY;
        tag_read_level_monsters(th);
        EAT_CANARY;
#if TAG_MAJOR_VERSION == 34
        _add_missing_branches();
#endif
        _shunt_monsters_out_of_walls();
        // The Abyss needs to visit other levels during level gen, before
        // all cells have been filled. We mustn't crash when it returns
        // from those excursions, and generate_abyss will check_map_validity
        // itself after the grid is fully populated.
        if (!player_in_branch(BRANCH_ABYSS))
        {
            unwind_var<coord_def> you_pos(you.position, coord_def());
            check_map_validity();
        }
        tag_read_level_tiles(th);
#if TAG_MAJOR_VERSION == 34
        // We can't do this when we unmarshall shops, since we haven't
        // unmarshalled items yet...
        if (th.getMinorVersion() < TAG_MINOR_SHOP_HACK)
            for (auto& entry : env.shop)
            {
                // Shop items were heaped up at this cell.
                for (stack_iterator si(coord_def(0, entry.second.num+5)); si; ++si)
                {
                    entry.second.stock.push_back(*si);
                    dec_mitm_item_quantity(si.index(), si->quantity);
                }
            }
#endif
        break;
    case TAG_GHOST:
        tag_read_ghost(th);
        break;
    default:
        // I don't know how to read that!
        die("unknown tag type");
    }
}

static void tag_construct_char(writer &th)
{
    marshallByte(th, TAG_CHR_FORMAT);
    // Important: you may never remove or alter a field without bumping
    // CHR_FORMAT. Bumping it makes all saves invisible when browsed in an
    // older version.
    // Please keep this compatible even over major version breaks!

    // Appending fields is fine, but inserting new fields anywhere other than
    // the end of this function is not!

    marshallString2(th, you.your_name);
    marshallString2(th, Version::Long);

    marshallByte(th, you.species);
    marshallByte(th, you.char_class);
    marshallByte(th, you.experience_level);
    marshallString2(th, string(get_job_name(you.char_class)));
    marshallByte(th, you.religion);
    marshallString2(th, you.jiyva_second_name);

    marshallByte(th, you.wizard);

    marshallByte(th, crawl_state.type);
    if (crawl_state.game_is_tutorial())
        marshallString2(th, crawl_state.map);

    marshallString2(th, species_name(you.species));
    marshallString2(th, you.religion ? god_name(you.religion) : "");

    // separate from the tutorial so we don't have to bump TAG_CHR_FORMAT
    marshallString2(th, crawl_state.map);

    marshallByte(th, you.explore);
}

/// is a custom scoring mechanism being stored?
static bool _calc_score_exists()
{
    lua_stack_cleaner clean(dlua);
    dlua.pushglobal("dgn.persist.calc_score");
    return !lua_isnil(dlua, -1);
}

static void tag_construct_you(writer &th)
{
    marshallInt(th, you.last_mid);
    marshallByte(th, you.piety);
    marshallShort(th, you.pet_target);

    marshallByte(th, you.max_level);
    marshallByte(th, you.where_are_you);
    marshallByte(th, you.depth);
    marshallByte(th, you.chapter);
    marshallByte(th, you.royal_jelly_dead);
    marshallByte(th, you.transform_uncancellable);
    marshallByte(th, you.berserk_penalty);
    marshallInt(th, you.abyss_speed);

    marshallInt(th, you.disease);
    ASSERT(you.hp > 0 || you.pending_revival);
    marshallShort(th, you.pending_revival ? 0 : you.hp);

    marshallShort(th, you.hunger);
    marshallBoolean(th, you.fishtail);
    _marshall_as_int(th, you.form);
    CANARY;

    // how many you.equip?
    marshallByte(th, NUM_EQUIP - EQ_FIRST_EQUIP);
    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
        marshallByte(th, you.equip[i]);
    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
        marshallBoolean(th, you.melded[i]);

    ASSERT_RANGE(you.magic_points, 0, you.max_magic_points + 1);
    marshallUByte(th, you.magic_points);
    marshallByte(th, you.max_magic_points);

    COMPILE_CHECK(NUM_STATS == 3);
    for (int i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.base_stats[i]);
    for (int i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.stat_loss[i]);

    CANARY;

    marshallInt(th, you.hit_points_regeneration);
    marshallInt(th, you.magic_points_regeneration);

    marshallInt(th, you.experience);
    marshallInt(th, you.total_experience);
    marshallInt(th, you.gold);

    marshallInt(th, you.exp_available);

    marshallInt(th, you.zigs_completed);
    marshallByte(th, you.zig_max);

    marshallString(th, you.banished_by);

    marshallShort(th, you.hp_max_adj_temp);
    marshallShort(th, you.hp_max_adj_perm);
    marshallShort(th, you.mp_max_adj);

    marshallShort(th, you.pos().x);
    marshallShort(th, you.pos().y);

    // how many spells?
    marshallUByte(th, MAX_KNOWN_SPELLS);
    for (int i = 0; i < MAX_KNOWN_SPELLS; ++i)
        marshallShort(th, you.spells[i]);

    marshallByte(th, 52);
    for (int i = 0; i < 52; i++)
        marshallByte(th, you.spell_letter_table[i]);

    marshallByte(th, 52);
    for (int i = 0; i < 52; i++)
        marshallShort(th, you.ability_letter_table[i]);

    marshallUByte(th, you.old_vehumet_gifts.size());
    for (auto spell : you.old_vehumet_gifts)
        marshallShort(th, spell);

    marshallUByte(th, you.vehumet_gifts.size());
    for (auto spell : you.vehumet_gifts)
        marshallShort(th, spell);

    CANARY;

    // how many skills?
    marshallByte(th, NUM_SKILLS);
    for (int j = 0; j < NUM_SKILLS; ++j)
    {
        marshallUByte(th, you.skills[j]);
        marshallByte(th, you.train[j]);
        marshallByte(th, you.train_alt[j]);
        marshallInt(th, you.training[j]);
        marshallInt(th, you.skill_points[j]);
        marshallInt(th, you.ct_skill_points[j]);
        marshallByte(th, you.skill_order[j]);   // skills ordering
    }

    marshallBoolean(th, you.auto_training);
    marshallByte(th, you.exercises.size());
    for (auto sk : you.exercises)
        marshallInt(th, sk);

    marshallByte(th, you.exercises_all.size());
    for (auto sk : you.exercises_all)
        marshallInt(th, sk);

    marshallByte(th, you.skill_menu_do);
    marshallByte(th, you.skill_menu_view);

    marshallInt(th, you.transfer_from_skill);
    marshallInt(th, you.transfer_to_skill);
    marshallInt(th, you.transfer_skill_points);
    marshallInt(th, you.transfer_total_skill_points);

    CANARY;

    // how many durations?
    marshallUByte(th, NUM_DURATIONS);
    for (int j = 0; j < NUM_DURATIONS; ++j)
        marshallInt(th, you.duration[j]);

    // how many attributes?
    marshallByte(th, NUM_ATTRIBUTES);
    for (int j = 0; j < NUM_ATTRIBUTES; ++j)
        marshallInt(th, you.attribute[j]);

    // Event timers.
    marshallByte(th, NUM_TIMERS);
    for (int j = 0; j < NUM_TIMERS; ++j)
    {
        marshallInt(th, you.last_timer_effect[j]);
        marshallInt(th, you.next_timer_effect[j]);
    }

    // how many mutations/demon powers?
    marshallShort(th, NUM_MUTATIONS);
    for (int j = 0; j < NUM_MUTATIONS; ++j)
    {
        marshallByte(th, you.mutation[j]);
        marshallByte(th, you.innate_mutation[j]);
        marshallByte(th, you.temp_mutation[j]);
        marshallByte(th, you.sacrifices[j]);
    }

    marshallByte(th, you.demonic_traits.size());
    for (int j = 0; j < int(you.demonic_traits.size()); ++j)
    {
        marshallByte(th, you.demonic_traits[j].level_gained);
        marshallShort(th, you.demonic_traits[j].mutation);
    }

    // set up sacrifice piety by ability
    marshallShort(th, 1 + ABIL_FINAL_SACRIFICE - ABIL_FIRST_SACRIFICE);
    for (int j = ABIL_FIRST_SACRIFICE; j <= ABIL_FINAL_SACRIFICE; ++j)
        marshallByte(th, you.sacrifice_piety[j]);

    CANARY;

    // how many penances?
    marshallByte(th, NUM_GODS);
    for (god_iterator it; it; ++it)
        marshallByte(th, you.penance[*it]);

    // which gods have been worshipped by this character?
    for (god_iterator it; it; ++it)
        marshallByte(th, you.worshipped[*it]);

    // what is the extent of divine generosity?
    for (god_iterator it; it; ++it)
        marshallShort(th, you.num_current_gifts[*it]);
    for (god_iterator it; it; ++it)
        marshallShort(th, you.num_total_gifts[*it]);
    for (god_iterator it; it; ++it)
        marshallBoolean(th, you.one_time_ability_used[*it]);

    // how much piety have you achieved at highest with each god?
    for (god_iterator it; it; ++it)
        marshallByte(th, you.piety_max[*it]);

    marshallByte(th, you.gift_timeout);
    marshallUByte(th, you.saved_good_god_piety);
    marshallByte(th, you.previous_good_god);

    for (god_iterator it; it; ++it)
        marshallInt(th, you.exp_docked[*it]);
    for (god_iterator it; it; ++it)
        marshallInt(th, you.exp_docked_total[*it]);

    // elapsed time
    marshallInt(th, you.elapsed_time);

    // time of game start
    marshallInt(th, you.birth_time);

    handle_real_time();

    // TODO: maybe switch to marshalling real_time_ms.
    marshallInt(th, you.real_time());
    marshallInt(th, you.num_turns);
    marshallInt(th, you.exploration);

    marshallInt(th, you.magic_contamination);

#if TAG_MAJOR_VERSION == 34
    marshallUByte(th, 0);
#endif
    marshallUByte(th, you.transit_stair);
    marshallByte(th, you.entering_level);
    marshallBoolean(th, you.travel_ally_pace);

    marshallByte(th, you.deaths);
    marshallByte(th, you.lives);

    CANARY;

    marshallInt(th, you.dactions.size());
    for (daction_type da : you.dactions)
        marshallByte(th, da);

    marshallInt(th, you.level_stack.size());
    for (const level_pos &lvl : you.level_stack)
        lvl.save(th);

    // List of currently beholding monsters (usually empty).
    marshallShort(th, you.beholders.size());
    for (mid_t beh : you.beholders)
         _marshall_as_int(th, beh);

    marshallShort(th, you.fearmongers.size());
    for (mid_t monger : you.fearmongers)
        _marshall_as_int(th, monger);

    marshallByte(th, you.piety_hysteresis);

    you.m_quiver.save(th);

    CANARY;

    // Action counts.
    marshallShort(th, you.action_count.size());
    for (const auto &ac : you.action_count)
    {
        marshallShort(th, ac.first.first);
        marshallInt(th, ac.first.second);
        for (int k = 0; k < 27; k++)
            marshallInt(th, ac.second[k]);
    }

    marshallByte(th, NUM_BRANCHES);
    for (int i = 0; i < NUM_BRANCHES; i++)
        marshallBoolean(th, you.branches_left[i]);

    marshallCoord(th, abyssal_state.major_coord);
    marshallInt(th, abyssal_state.seed);
    marshallInt(th, abyssal_state.depth);
    marshallFloat(th, abyssal_state.phase);
    marshall_level_id(th, abyssal_state.level);

#if TAG_MAJOR_VERSION == 34
    if (abyssal_state.level.branch == BRANCH_DWARF || !abyssal_state.level.is_valid())
        abyssal_state.level = level_id(static_cast<branch_type>(BRANCH_DUNGEON), 19);
#endif

    _marshall_constriction(th, &you);

    marshallUByte(th, you.octopus_king_rings);

    marshallUnsigned(th, you.uncancel.size());
    for (const pair<uncancellable_type, int>& unc : you.uncancel)
    {
        marshallUByte(th, unc.first);
        marshallInt(th, unc.second);
    }

    marshallUnsigned(th, you.recall_list.size());
    for (mid_t recallee : you.recall_list)
        _marshall_as_int(th, recallee);

    marshallUByte(th, NUM_SEEDS);
    for (int i = 0; i < NUM_SEEDS; i++)
        marshallInt(th, you.game_seeds[i]);

    CANARY;

    // don't let vault caching errors leave a normal game with sprint scoring
    if (!crawl_state.game_is_sprint())
        ASSERT(!_calc_score_exists());

    if (!dlua.callfn("dgn_save_data", "u", &th))
        mprf(MSGCH_ERROR, "Failed to save Lua data: %s", dlua.error.c_str());

    CANARY;

    // Write a human-readable string out on the off chance that
    // we fail to be able to read this file back in using some later version.
    string revision = "Git:";
    revision += Version::Long;
    marshallString(th, revision);

    you.props.write(th);
}

static void tag_construct_you_items(writer &th)
{
    // how many inventory slots?
    marshallByte(th, ENDOFPACK);
    for (const auto &item : you.inv)
        marshallItem(th, item);

    marshallFixedBitVector<NUM_RUNE_TYPES>(th, you.runes);
    marshallByte(th, you.obtainable_runes);

    // Item descrip for each type & subtype.
    // how many types?
    marshallUByte(th, NUM_IDESC);
    // how many subtypes?
    marshallUByte(th, MAX_SUBTYPES);
    for (int i = 0; i < NUM_IDESC; ++i)
        for (int j = 0; j < MAX_SUBTYPES; ++j)
            marshallInt(th, you.item_description[i][j]);

    marshallUByte(th, NUM_OBJECT_CLASSES);
    for (int i = 0; i < NUM_OBJECT_CLASSES; ++i)
    {
        if (!item_type_has_ids((object_class_type)i))
            continue;
        for (int j = 0; j < MAX_SUBTYPES; ++j)
            marshallBoolean(th, you.type_ids[i][j]);
    }

    CANARY;

    // how many unique items?
    marshallUByte(th, MAX_UNRANDARTS);
    for (int j = 0; j < MAX_UNRANDARTS; ++j)
        marshallByte(th,you.unique_items[j]);

    marshallByte(th, NUM_FIXED_BOOKS);
    for (int j = 0; j < NUM_FIXED_BOOKS; ++j)
        marshallByte(th,you.had_book[j]);

    marshallShort(th, NUM_SPELLS);
    for (int j = 0; j < NUM_SPELLS; ++j)
        marshallByte(th,you.seen_spell[j]);

    marshallShort(th, NUM_WEAPONS);
    for (int j = 0; j < NUM_WEAPONS; ++j)
        marshallInt(th,you.seen_weapon[j]);

    marshallShort(th, NUM_ARMOURS);
    for (int j = 0; j < NUM_ARMOURS; ++j)
        marshallInt(th,you.seen_armour[j]);

    marshallFixedBitVector<NUM_MISCELLANY>(th, you.seen_misc);

    for (int i = 0; i < NUM_OBJECT_CLASSES; i++)
        for (int j = 0; j < MAX_SUBTYPES; j++)
            marshallInt(th, you.force_autopickup[i][j]);
}

static void marshallPlaceInfo(writer &th, PlaceInfo place_info)
{
    marshallInt(th, place_info.branch);

    marshallInt(th, place_info.num_visits);
    marshallInt(th, place_info.levels_seen);

    marshallInt(th, place_info.mon_kill_exp);

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
        marshallInt(th, brdepth[j]);
        marshall_level_id(th, brentry[j]);
        marshallInt(th, branch_bribe[j]);
    }

    // Root of the dungeon; usually BRANCH_DUNGEON.
    marshallInt(th, root_branch);

    marshallMap(th, stair_level,
                _marshall_as_int<branch_type>, _marshall_level_id_set);
    marshallMap(th, shops_present,
                _marshall_level_pos, _marshall_as_int<shop_type>);
    marshallMap(th, altars_present,
                _marshall_level_pos, _marshall_as_int<god_type>);
    marshallMap(th, portals_present,
                _marshall_level_pos, _marshall_as_int<branch_type>);
    marshallMap(th, portal_notes,
                _marshall_level_pos, marshallString);
    marshallMap(th, level_annotations,
                marshall_level_id, marshallString);
    marshallMap(th, level_exclusions,
                marshall_level_id, marshallString);
    marshallMap(th, level_uniques,
            marshall_level_id, marshallString);
    marshallUniqueAnnotations(th);

    marshallPlaceInfo(th, you.global_info);
    vector<PlaceInfo> list = you.get_all_place_info();
    // How many different places we have info on?
    marshallShort(th, list.size());

    for (const PlaceInfo &place : list)
        marshallPlaceInfo(th, place);

    marshall_iterator(th, you.uniq_map_tags.begin(), you.uniq_map_tags.end(),
                      marshallString);
    marshall_iterator(th, you.uniq_map_names.begin(), you.uniq_map_names.end(),
                      marshallString);
    marshallMap(th, you.vault_list, marshall_level_id, marshallStringVector);

    write_level_connectivity(th);
}

static void marshall_follower(writer &th, const follower &f)
{
    ASSERT(!invalid_monster_type(f.mons.type));
    ASSERT(f.mons.alive());
    marshallMonster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        marshallItem(th, f.items[i]);
}

static follower unmarshall_follower(reader &th)
{
    follower f;
    unmarshallMonster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        unmarshallItem(th, f.items[i]);
    return f;
}

static void marshall_companion(writer &th, const companion &c)
{
    marshall_follower(th, c.mons);
    marshall_level_id(th, c.level);
    marshallInt(th, c.timestamp);
}

static companion unmarshall_companion(reader &th)
{
    companion c;
    c.mons = unmarshall_follower(th);
    c.level = unmarshall_level_id(th);
    c.timestamp = unmarshallInt(th);
    return c;
}

static void marshall_follower_list(writer &th, const m_transit_list &mlist)
{
    marshallShort(th, mlist.size());

    for (const auto &follower : mlist)
        marshall_follower(th, follower);
}

static void marshall_item_list(writer &th, const i_transit_list &ilist)
{
    marshallShort(th, ilist.size());

    for (const auto &item : ilist)
        marshallItem(th, item);
}

static m_transit_list unmarshall_follower_list(reader &th)
{
    m_transit_list mlist;

    const int size = unmarshallShort(th);

    for (int i = 0; i < size; ++i)
    {
        follower f = unmarshall_follower(th);
        if (!f.mons.alive())
        {
            mprf(MSGCH_ERROR,
                 "Dead monster %s in transit list in saved game, ignoring.",
                 f.mons.name(DESC_PLAIN, true).c_str());
        }
        else
            mlist.push_back(f);
    }

    return mlist;
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

    return ilist;
}

static void marshall_level_map_masks(writer &th)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        marshallInt(th, env.level_map_mask(*ri));
        marshallInt(th, env.level_map_ids(*ri));
    }
}

static void unmarshall_level_map_masks(reader &th)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        env.level_map_mask(*ri) = unmarshallInt(th);
        env.level_map_ids(*ri)  = unmarshallInt(th);
    }
}

static void marshall_level_map_unique_ids(writer &th)
{
    marshallSet(th, env.level_uniq_maps, marshallString);
    marshallSet(th, env.level_uniq_map_tags, marshallString);
}

static void unmarshall_level_map_unique_ids(reader &th)
{
    unmarshallSet(th, env.level_uniq_maps, unmarshallString);
    unmarshallSet(th, env.level_uniq_map_tags, unmarshallString);
}

static void marshall_subvault_place(writer &th,
                                    const subvault_place &subvault_place);

static void marshall_mapdef(writer &th, const map_def &map)
{
    marshallString(th, map.name);
    map.write_index(th);
    map.write_maplines(th);
    marshallString(th, map.description);
    marshallMap(th, map.feat_renames,
                _marshall_as_int<dungeon_feature_type>, marshallString);
    marshall_iterator(th,
                      map.subvault_places.begin(),
                      map.subvault_places.end(),
                      marshall_subvault_place);
}

static void marshall_subvault_place(writer &th,
                                    const subvault_place &subvault_place)
{
    marshallCoord(th, subvault_place.tl);
    marshallCoord(th, subvault_place.br);
    marshall_mapdef(th, *subvault_place.subvault);
}

static subvault_place unmarshall_subvault_place(reader &th);
static map_def unmarshall_mapdef(reader &th)
{
    map_def map;
    map.name = unmarshallString(th);
    map.read_index(th);
    map.read_maplines(th);
    map.description = unmarshallString(th);
    unmarshallMap(th, map.feat_renames,
                  unmarshall_int_as<dungeon_feature_type>,
                  unmarshallString);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_REIFY_SUBVAULTS
        && th.getMinorVersion() != TAG_MINOR_0_11)
#endif
        unmarshall_vector(th, map.subvault_places, unmarshall_subvault_place);
    return map;
}

static subvault_place unmarshall_subvault_place(reader &th)
{
    subvault_place subvault;
    subvault.tl = unmarshallCoord(th);
    subvault.br = unmarshallCoord(th);
    subvault.set_subvault(unmarshall_mapdef(th));
    return subvault;
}

static void marshall_vault_placement(writer &th, const vault_placement &vp)
{
    marshallCoord(th, vp.pos);
    marshallCoord(th, vp.size);
    marshallShort(th, vp.orient);
    marshall_mapdef(th, vp.map);
    marshall_iterator(th, vp.exits.begin(), vp.exits.end(), marshallCoord);
#if TAG_MAJOR_VERSION == 34
    marshallShort(th, -1);
#endif
    marshallByte(th, vp.seen);
}

static vault_placement unmarshall_vault_placement(reader &th)
{
    vault_placement vp;
    vp.pos = unmarshallCoord(th);
    vp.size = unmarshallCoord(th);
    vp.orient = static_cast<map_section_type>(unmarshallShort(th));
    vp.map = unmarshall_mapdef(th);
    unmarshall_vector(th, vp.exits, unmarshallCoord);
#if TAG_MAJOR_VERSION == 34
    unmarshallShort(th);
#endif
    vp.seen = !!unmarshallByte(th);

    return vp;
}

static void marshall_level_vault_placements(writer &th)
{
    marshallShort(th, env.level_vaults.size());
    for (unique_ptr<vault_placement> &vp : env.level_vaults)
        marshall_vault_placement(th, *vp);
}

static void unmarshall_level_vault_placements(reader &th)
{
    const int nvaults = unmarshallShort(th);
    ASSERT(nvaults >= 0);
    dgn_clear_vault_placements();
    for (int i = 0; i < nvaults; ++i)
    {
        env.level_vaults.emplace_back(
            new vault_placement(unmarshall_vault_placement(th)));
    }
}

static void marshall_level_vault_data(writer &th)
{
    marshallString(th, env.level_build_method);
    marshallSet(th, env.level_layout_types, marshallString);

    marshall_level_map_masks(th);
    marshall_level_map_unique_ids(th);
#if TAG_MAJOR_VERSION == 34
    marshallInt(th, 0);
#endif
    marshall_level_vault_placements(th);
}

static void unmarshall_level_vault_data(reader &th)
{
    env.level_build_method = unmarshallString(th);
    unmarshallSet(th, env.level_layout_types, unmarshallString);

    unmarshall_level_map_masks(th);
    unmarshall_level_map_unique_ids(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_VAULT_LIST) // 33:17 has it
        unmarshallStringVector(th);
#endif
    unmarshall_level_vault_placements(th);
}

static void marshall_shop(writer &th, const shop_struct& shop)
{
    marshallByte(th, shop.type);
    marshallByte(th, shop.keeper_name[0]);
    marshallByte(th, shop.keeper_name[1]);
    marshallByte(th, shop.keeper_name[2]);
    marshallByte(th, shop.pos.x);
    marshallByte(th, shop.pos.y);
    marshallByte(th, shop.greed);
    marshallByte(th, shop.level);
    marshallString(th, shop.shop_name);
    marshallString(th, shop.shop_type_name);
    marshallString(th, shop.shop_suffix_name);
    marshall_iterator(th, shop.stock.begin(), shop.stock.end(),
                          bind(marshallItem, placeholders::_1, placeholders::_2, false));
}

static void unmarshall_shop(reader &th, shop_struct& shop)
{
    shop.type  = static_cast<shop_type>(unmarshallByte(th));
#if TAG_MAJOR_VERSION == 34
    if (shop.type == SHOP_UNASSIGNED)
        return;
    if (th.getMinorVersion() < TAG_MINOR_MISC_SHOP_CHANGE
        && shop.type == NUM_SHOPS)
    {
        // This was SHOP_MISCELLANY, which is now part of SHOP_EVOKABLES.
        shop.type = SHOP_EVOKABLES;
    }
#else
    ASSERT(shop.type != SHOP_UNASSIGNED);
#endif
    shop.keeper_name[0] = unmarshallUByte(th);
    shop.keeper_name[1] = unmarshallUByte(th);
    shop.keeper_name[2] = unmarshallUByte(th);
    shop.pos.x = unmarshallByte(th);
    shop.pos.y = unmarshallByte(th);
    shop.greed = unmarshallByte(th);
    shop.level = unmarshallByte(th);
    shop.shop_name = unmarshallString(th);
    shop.shop_type_name = unmarshallString(th);
    shop.shop_suffix_name = unmarshallString(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_SHOP_HACK)
        shop.stock.clear();
    else
#endif
    unmarshall_vector(th, shop.stock, [] (reader& r) -> item_def
                                      {
                                          item_def ret;
                                          unmarshallItem(r, ret);
                                          return ret;
                                      });
}

void ShopInfo::save(writer& outf) const
{
    marshall_shop(outf, shop);
}

void ShopInfo::load(reader& inf)
{
#if TAG_MAJOR_VERSION == 34
    if (inf.getMinorVersion() < TAG_MINOR_SHOPINFO
        || inf.getMinorVersion() == TAG_MINOR_UNSHOPINFO)
    {
        shop.type = static_cast<shop_type>(unmarshallShort(inf));

        shop.pos.x = unmarshallShort(inf);
        shop.pos.x &= 0xFF;

        shop.pos.y = unmarshallShort(inf);

        int itemcount = unmarshallShort(inf);

        // xref hack in shopping.cc:shop_name()
        shop.shop_name = " ";
        unmarshallString4(inf, shop.shop_type_name);
        for (int i = 0; i < itemcount; ++i)
        {
            shop.stock.emplace_back();
            unmarshallItem(inf, shop.stock.back());
            int cost = unmarshallShort(inf);
            shop.greed = cost * 10 / item_value(shop.stock.back(),
                shoptype_identifies_stock(shop.type));
        }
    }
    else
#endif
    unmarshall_shop(inf, shop);
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

static void tag_construct_companions(writer &th)
{
#if TAG_MAJOR_VERSION == 34
    fixup_bad_companions();
#endif
    marshallMap(th, companion_list, _marshall_as_int<mid_t>,
                 marshall_companion);
}

// Save versions 30-32.26 are readable but don't store the names.
static const char* old_species[]=
{
    "Human", "High Elf", "Deep Elf", "Sludge Elf", "Mountain Dwarf", "Halfling",
    "Hill Orc", "Kobold", "Mummy", "Naga", "Ogre", "Troll",
    "Red Draconian", "White Draconian", "Green Draconian", "Yellow Draconian",
    "Grey Draconian", "Black Draconian", "Purple Draconian", "Mottled Draconian",
    "Pale Draconian", "Draconian", "Centaur", "Demigod", "Spriggan", "Minotaur",
    "Demonspawn", "Ghoul", "Tengu", "Merfolk", "Vampire", "Deep Dwarf", "Felid",
    "Octopode",
};

static const char* old_gods[]=
{
    "", "Zin", "The Shining One", "Kikubaaqudgha", "Yredelemnul", "Xom",
    "Vehumet", "Okawaru", "Makhleb", "Sif Muna", "Trog", "Nemelex Xobeh",
    "Elyvilon", "Lugonu", "Beogh", "Jiyva", "Fedhas", "Cheibriados",
    "Ashenzari",
};

void tag_read_char(reader &th, uint8_t format, uint8_t major, uint8_t minor)
{
    // Important: values out of bounds are good here, the save browser needs to
    // be forward-compatible. We validate them only on an actual restore.
    you.your_name         = unmarshallString2(th);
    you.prev_save_version = unmarshallString2(th);
    dprf("Last save Crawl version: %s", you.prev_save_version.c_str());

    you.species           = static_cast<species_type>(unmarshallUByte(th));
    you.char_class        = static_cast<job_type>(unmarshallUByte(th));
    you.experience_level  = unmarshallByte(th);
    you.chr_class_name    = unmarshallString2(th);
    you.religion          = static_cast<god_type>(unmarshallUByte(th));
    you.jiyva_second_name = unmarshallString2(th);

    you.wizard            = unmarshallBoolean(th);

    // this was mistakenly inserted in the middle for a few tag versions - this
    // just makes sure that games generated in that time period are still
    // readable, but should not be used for new games
#if TAG_CHR_FORMAT == 0
    // TAG_MINOR_EXPLORE_MODE and TAG_MINOR_FIX_EXPLORE_MODE
    if (major == 34 && (minor >= 121 && minor < 130))
        you.explore = unmarshallBoolean(th);
#endif

    crawl_state.type = (game_type) unmarshallUByte(th);
    if (crawl_state.game_is_tutorial())
        crawl_state.map = unmarshallString2(th);
    else
        crawl_state.map = "";

    if (major > 32 || major == 32 && minor > 26)
    {
        you.chr_species_name = unmarshallString2(th);
        you.chr_god_name     = unmarshallString2(th);
    }
    else
    {
        if (you.species >= 0 && you.species < (int)ARRAYSZ(old_species))
            you.chr_species_name = old_species[you.species];
        else
            you.chr_species_name = "Yak";
        if (you.religion >= 0 && you.religion < (int)ARRAYSZ(old_gods))
            you.chr_god_name = old_gods[you.religion];
        else
            you.chr_god_name = "Marduk";
    }

    if (major > 34 || major == 34 && minor >= 29)
        crawl_state.map = unmarshallString2(th);

    if (major > 34 || major == 34 && minor >= 130)
        you.explore = unmarshallBoolean(th);
}

static void tag_read_you(reader &th)
{
    int count;

    ASSERT_RANGE(you.species, 0, NUM_SPECIES);
    ASSERT_RANGE(you.char_class, 0, NUM_JOBS);
    ASSERT_RANGE(you.experience_level, 1, 28);
    ASSERT(you.religion < NUM_GODS);
    ASSERT_RANGE(crawl_state.type, GAME_TYPE_UNSPECIFIED + 1, NUM_GAME_TYPE);
    you.last_mid          = unmarshallInt(th);
    you.piety             = unmarshallUByte(th);
    ASSERT(you.piety <= MAX_PIETY);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_ROTTING)
        unmarshallUByte(th);
#endif
    you.pet_target        = unmarshallShort(th);

    you.max_level         = unmarshallByte(th);
    you.where_are_you     = static_cast<branch_type>(unmarshallUByte(th));
    ASSERT(you.where_are_you < NUM_BRANCHES);
    you.depth             = unmarshallByte(th);
    ASSERT(you.depth > 0);
    you.chapter           = static_cast<game_chapter>(unmarshallUByte(th));
    ASSERT(you.chapter < NUM_CHAPTERS);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_ZOT_OPEN)
        unmarshallBoolean(th);
#endif
    you.royal_jelly_dead = unmarshallBoolean(th);
    you.transform_uncancellable = unmarshallBoolean(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_IS_UNDEAD)
        unmarshallUByte(th);
    if (th.getMinorVersion() < TAG_MINOR_CALC_UNRAND_REACTS)
        unmarshallShort(th);
#endif
    you.berserk_penalty   = unmarshallByte(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_GARGOYLE_DR
      && th.getMinorVersion() < TAG_MINOR_RM_GARGOYLE_DR)
    {
        unmarshallInt(th); // Slough an integer.
    }

    if (th.getMinorVersion() < TAG_MINOR_AUTOMATIC_MANUALS)
    {
        unmarshallShort(th);
        unmarshallInt(th);
    }
#endif

    you.abyss_speed = unmarshallInt(th);

    you.disease         = unmarshallInt(th);
    you.hp              = unmarshallShort(th);
    you.hunger          = unmarshallShort(th);
    you.fishtail        = unmarshallBoolean(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_NOME_NO_MORE)
        unmarshallInt(th);
#endif
    you.form            = unmarshall_int_as<transformation>(th);
    ASSERT_RANGE(static_cast<int>(you.form), 0, NUM_TRANSFORMS);
#if TAG_MAJOR_VERSION == 34
    // Fix the effects of #7668 (Vampire lose undead trait once coming back
    // from lich form).
    if (you.form == transformation::none)
        you.transform_uncancellable = false;
#else
    ASSERT(you.form != transformation::none || !you.transform_uncancellable);
#endif
    EAT_CANARY;

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_SAGE_REMOVAL)
    {
        count = unmarshallShort(th);
        ASSERT_RANGE(count, 0, 32768);
        for (int i = 0; i < count; ++i)
        {
            unmarshallByte(th);
            unmarshallInt(th);
            unmarshallInt(th);
        }
    }
#endif

    // How many you.equip?
    count = unmarshallByte(th);
    ASSERT(count <= NUM_EQUIP);
    for (int i = EQ_FIRST_EQUIP; i < count; ++i)
    {
        you.equip[i] = unmarshallByte(th);
        ASSERT_RANGE(you.equip[i], -1, ENDOFPACK);
    }
    for (int i = count; i < NUM_EQUIP; ++i)
        you.equip[i] = -1;
    for (int i = 0; i < count; ++i)
        you.melded.set(i, unmarshallBoolean(th));
    for (int i = count; i < NUM_EQUIP; ++i)
        you.melded.set(i, false);

    you.magic_points              = unmarshallUByte(th);
    you.max_magic_points          = unmarshallByte(th);

    for (int i = 0; i < NUM_STATS; ++i)
        you.base_stats[i] = unmarshallByte(th);
    for (int i = 0; i < NUM_STATS; ++i)
        you.stat_loss[i] = unmarshallByte(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_STAT_ZERO_DURATION)
    {
        for (int i = 0; i < NUM_STATS; ++i)
            unmarshallUByte(th);
    }
    if (th.getMinorVersion() < TAG_MINOR_STAT_ZERO)
    {
        for (int i = 0; i < NUM_STATS; ++i)
            unmarshallString(th);
    }
#endif
    EAT_CANARY;

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_INT_REGEN)
    {
        you.hit_points_regeneration   = unmarshallByte(th);
        you.magic_points_regeneration = unmarshallByte(th);
        unmarshallShort(th);
    }
    else
    {
#endif
    you.hit_points_regeneration   = unmarshallInt(th);
    you.magic_points_regeneration = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
    }
#endif

    you.experience                = unmarshallInt(th);
    you.total_experience = unmarshallInt(th);
    you.gold                      = unmarshallInt(th);
    you.exp_available             = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_NO_ZOTDEF)
        unmarshallInt(th);
#endif
    you.zigs_completed            = unmarshallInt(th);
    you.zig_max                   = unmarshallByte(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_TRACK_BANISHER)
        you.banished_by = "";
    else
#endif
        you.banished_by           = unmarshallString(th);

    you.hp_max_adj_temp           = unmarshallShort(th);
    you.hp_max_adj_perm           = unmarshallShort(th);
    you.mp_max_adj                = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_REMOVE_BASE_MP)
    {
        int baseadj = unmarshallShort(th);
        you.mp_max_adj += baseadj;
    }
    if (th.getMinorVersion() < TAG_MINOR_CLASS_HP_0)
        you.hp_max_adj_perm -= 8;
#endif

    const int x = unmarshallShort(th);
    const int y = unmarshallShort(th);
    // SIGHUP during Step from Time/etc is ok.
    ASSERT(!x && !y || in_bounds(x, y));
    you.moveto(coord_def(x, y));

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_WEIGHTLESS)
        unmarshallShort(th);
#endif

    // how many spells?
    you.spell_no = 0;
    count = unmarshallUByte(th);
    ASSERT(count >= 0);
    for (int i = 0; i < count && i < MAX_KNOWN_SPELLS; ++i)
    {
        you.spells[i] = unmarshallSpellType(th);
        if (you.spells[i] != SPELL_NO_SPELL)
            you.spell_no++;
    }
    for (int i = MAX_KNOWN_SPELLS; i < count; ++i)
    {
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() < TAG_MINOR_SHORT_SPELL_TYPE)
            unmarshallUByte(th);
        else
#endif
            unmarshallShort(th);
    }

    count = unmarshallByte(th);
    ASSERT(count == (int)you.spell_letter_table.size());
    for (int i = 0; i < count; i++)
    {
        int s = unmarshallByte(th);
        ASSERT_RANGE(s, -1, MAX_KNOWN_SPELLS);
        you.spell_letter_table[i] = s;
    }

    count = unmarshallByte(th);
    ASSERT(count == (int)you.ability_letter_table.size());
#if TAG_MAJOR_VERSION == 34
    bool found_fly = false;
    bool found_stop_flying = false;
#endif
    for (int i = 0; i < count; i++)
    {
        int a = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() < TAG_MINOR_ABIL_1000)
        {
            if (a >= 230)
                a += 2000 - 230;
            else if (a >= 50)
                a += 1000 - 50;
        }
        if (th.getMinorVersion() < TAG_MINOR_ABIL_GOD_FIXUP)
        {
            if (a >= ABIL_ASHENZARI_END_TRANSFER + 1
                && a <= ABIL_ASHENZARI_END_TRANSFER + 3)
            {
                a += ABIL_STOP_RECALL - ABIL_ASHENZARI_END_TRANSFER - 1;
            }
        }
        if (a == ABIL_FLY
            || a == ABIL_WISP_BLINK // was ABIL_FLY_II
               && th.getMinorVersion() < TAG_MINOR_0_12)
        {
            if (found_fly)
                a = ABIL_NON_ABILITY;
            else
                a = ABIL_FLY;
            found_fly = true;
        }
        if (a == ABIL_EVOKE_STOP_LEVITATING
            || a == ABIL_STOP_FLYING)
        {
            if (found_stop_flying)
                a = ABIL_NON_ABILITY;
            else
                a = ABIL_STOP_FLYING;
            found_stop_flying = true;
        }

        if (th.getMinorVersion() < TAG_MINOR_NO_JUMP)
        {
            // ABIL_JUMP deleted (ABIL_DIG has its old spot), map it
            // away and shift following intrinsic abilities down.
            // ABIL_EVOKE_JUMP was also deleted, but was the last
            // evocable ability, so just map it away.
            if (a == ABIL_DIG || a == ABIL_EVOKE_TELEPORT_CONTROL + 1)
                a = ABIL_NON_ABILITY;
            else if (a > ABIL_DIG && a < ABIL_MIN_EVOKE)
                a -= 1;
        }

        if (th.getMinorVersion() < TAG_MINOR_MOTTLED_REMOVAL)
        {
            if (a == ABIL_BREATHE_STICKY_FLAME)
                a = ABIL_BREATHE_FIRE;
        }

        // Bad offset from games transferred prior to 0.17-a0-2121-g4af814f.
        if (a == NUM_ABILITIES)
            a = ABIL_NON_ABILITY;
#endif
        ASSERT_RANGE(a, ABIL_NON_ABILITY, NUM_ABILITIES);
        ASSERT(a != 0);
        you.ability_letter_table[i] = static_cast<ability_type>(a);
    }

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_VEHUMET_SPELL_GIFT
        && th.getMinorVersion() != TAG_MINOR_0_11)
    {
#endif
        count = unmarshallUByte(th);
        for (int i = 0; i < count; ++i)
            you.old_vehumet_gifts.insert(unmarshallSpellType(th));

#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() < TAG_MINOR_VEHUMET_MULTI_GIFTS)
            you.vehumet_gifts.insert(unmarshallSpellType(th));
        else
        {
#endif
            count = unmarshallUByte(th);
            for (int i = 0; i < count; ++i)
                you.vehumet_gifts.insert(unmarshallSpellType(th));
#if TAG_MAJOR_VERSION == 34
        }
    }
#endif
    EAT_CANARY;

    // how many skills?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_SKILLS);
    for (int j = 0; j < count; ++j)
    {
        you.skills[j]          = unmarshallUByte(th);
        ASSERT(you.skills[j] <= 27 || you.wizard);

        you.train[j]    = (training_status)unmarshallByte(th);
        you.train_alt[j]    = (training_status)unmarshallByte(th);
        you.training[j] = unmarshallInt(th);
        you.skill_points[j]    = unmarshallInt(th);
        you.ct_skill_points[j] = unmarshallInt(th);
        you.skill_order[j]     = unmarshallByte(th);
    }

    you.auto_training = unmarshallBoolean(th);

    count = unmarshallByte(th);
    for (int i = 0; i < count; i++)
        you.exercises.push_back((skill_type)unmarshallInt(th));

    count = unmarshallByte(th);
    for (int i = 0; i < count; i++)
        you.exercises_all.push_back((skill_type)unmarshallInt(th));

    you.skill_menu_do = static_cast<skill_menu_state>(unmarshallByte(th));
    you.skill_menu_view = static_cast<skill_menu_state>(unmarshallByte(th));
    you.transfer_from_skill = static_cast<skill_type>(unmarshallInt(th));
    ASSERT(you.transfer_from_skill == SK_NONE || you.transfer_from_skill < NUM_SKILLS);
    you.transfer_to_skill = static_cast<skill_type>(unmarshallInt(th));
    ASSERT(you.transfer_to_skill == SK_NONE || you.transfer_to_skill < NUM_SKILLS);
    you.transfer_skill_points = unmarshallInt(th);
    you.transfer_total_skill_points = unmarshallInt(th);

    // Set up you.skill_cost_level.
    you.skill_cost_level = 0;
    check_skill_cost_change();

    EAT_CANARY;

    // how many durations?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_DURATIONS < 256);
    for (int j = 0; j < count && j < NUM_DURATIONS; ++j)
        you.duration[j] = unmarshallInt(th);
    for (int j = NUM_DURATIONS; j < count; ++j)
        unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC)
        you.duration[DUR_MAGIC_ARMOUR] = 0;

    if (th.getMinorVersion() < TAG_MINOR_FUNGUS_FORM
        && you.form == transformation::fungus)
    {
        you.duration[DUR_CONFUSING_TOUCH] = 0;
    }

    you.duration[DUR_JELLY_PRAYER] = 0;
#endif

    // how many attributes?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_ATTRIBUTES < 256);
    for (int j = 0; j < count && j < NUM_ATTRIBUTES; ++j)
    {
#if TAG_MAJOR_VERSION == 34
        if (j == ATTR_BANISHMENT_IMMUNITY && th.getMinorVersion() == TAG_MINOR_0_11)
        {
            unmarshallInt(th); // ATTR_UNUSED_1
            count--;
        }
        if (j == ATTR_NOISES && th.getMinorVersion() == TAG_MINOR_CLASS_HP_0
            && count == 40)
        {
            dprf("recovering ATTR_NOISES");
            j++, count++;
        }
#endif
        you.attribute[j] = unmarshallInt(th);
    }
#if TAG_MAJOR_VERSION == 34
    if (count == ATTR_PAKELLAS_EXTRA_MP && you_worship(GOD_PAKELLAS))
        you.attribute[ATTR_PAKELLAS_EXTRA_MP] = POT_MAGIC_MP;
#endif
    for (int j = count; j < NUM_ATTRIBUTES; ++j)
        you.attribute[j] = 0;
    for (int j = NUM_ATTRIBUTES; j < count; ++j)
        unmarshallInt(th);

#if TAG_MAJOR_VERSION == 34
    if (you.attribute[ATTR_DIVINE_REGENERATION])
    {
        you.attribute[ATTR_DIVINE_REGENERATION] = 0;
        you.duration[DUR_TROGS_HAND] = max(you.duration[DUR_TROGS_HAND],
                                           you.duration[DUR_REGENERATION]);
        you.duration[DUR_REGENERATION] = 0;
    }
    if (you.attribute[ATTR_SEARING_RAY] > 3)
        you.attribute[ATTR_SEARING_RAY] = 0;

    if (th.getMinorVersion() < TAG_MINOR_STAT_LOSS_XP)
    {
        for (int i = 0; i < NUM_STATS; ++i)
        {
            if (you.stat_loss[i] > 0)
            {
                you.attribute[ATTR_STAT_LOSS_XP] = stat_loss_roll();
                break;
            }
        }
    }

#endif

#if TAG_MAJOR_VERSION == 34
    // Nemelex item type sacrifice toggles.
    if (th.getMinorVersion() < TAG_MINOR_NEMELEX_WEIGHTS)
    {
        count = unmarshallByte(th);
        ASSERT(count <= NUM_OBJECT_CLASSES);
        for (int j = 0; j < count; ++j)
            unmarshallInt(th);
    }
#endif

    int timer_count = 0;
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_EVENT_TIMERS)
    {
#endif
    timer_count = unmarshallByte(th);
    ASSERT(timer_count <= NUM_TIMERS);
    for (int j = 0; j < timer_count; ++j)
    {
        you.last_timer_effect[j] = unmarshallInt(th);
        you.next_timer_effect[j] = unmarshallInt(th);
    }
#if TAG_MAJOR_VERSION == 34
    }
    else
        timer_count = 0;
#endif
    // We'll have to fix up missing/broken timer entries after
    // we unmarshall you.elapsed_time.

    // how many mutations/demon powers?
    count = unmarshallShort(th);
    ASSERT_RANGE(count, 0, NUM_MUTATIONS + 1);
    for (int j = 0; j < count; ++j)
    {
        you.mutation[j]         = unmarshallUByte(th);
        you.innate_mutation[j]  = unmarshallUByte(th);
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() >= TAG_MINOR_TEMP_MUTATIONS
            && th.getMinorVersion() != TAG_MINOR_0_11)
        {
#endif
        you.temp_mutation[j]    = unmarshallUByte(th);
#if TAG_MAJOR_VERSION == 34
        }
        if (th.getMinorVersion() < TAG_MINOR_RU_SACRIFICES)
            you.sacrifices[j]   = 0;
        else
        {
#endif
        you.sacrifices[j]       = unmarshallUByte(th);
#if TAG_MAJOR_VERSION == 34
        }

        if (you.innate_mutation[j] + you.temp_mutation[j] > you.mutation[j])
        {
            mprf(MSGCH_ERROR, "Mutation #%d out of sync, fixing up.", j);
            you.mutation[j] = you.innate_mutation[j] + you.temp_mutation[j];
        }
#endif
    }

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_STAT_MUT)
    {
        // Convert excess mutational stats into base stats.
        mutation_type stat_mutations[] = { MUT_STRONG, MUT_CLEVER, MUT_AGILE };
        stat_type stat_types[] = { STAT_STR, STAT_INT, STAT_DEX };
        for (int j = 0; j < 3; ++j)
        {
            mutation_type mut = stat_mutations[j];
            stat_type stat = stat_types[j];
            int total_mutation_level = you.temp_mutation[mut] + you.mutation[mut];
            if (total_mutation_level > 2)
            {
                int new_level = max(0, min(you.temp_mutation[mut] - you.mutation[mut], 2));
                you.temp_mutation[mut] = new_level;
            }
            if (you.mutation[mut] > 2)
            {
                int excess = you.mutation[mut] - 4;
                if (excess > 0)
                    you.base_stats[stat] += excess;
                you.mutation[mut] = 2;
            }
        }
        mutation_type bad_stat_mutations[] = { MUT_WEAK, MUT_DOPEY, MUT_CLUMSY };
        for (int j = 0; j < 3; ++j)
        {
            mutation_type mut = bad_stat_mutations[j];
            int level = you.mutation[mut];
            switch (level)
            {
            case 0:
            case 1:
                you.mutation[mut] = 0;
                break;
            case 2:
            case 3:
                you.mutation[mut] = 1;
                break;
            default:
                you.mutation[mut] = 2;
                break;
            };
            if (you.temp_mutation[mut] > 2 && you.mutation[mut] < 2)
                you.temp_mutation[mut] = 1;
            else
                you.temp_mutation[mut] = 0;
        }
    }
    you.mutation[MUT_FAST] = you.innate_mutation[MUT_FAST];
    you.mutation[MUT_SLOW] = you.innate_mutation[MUT_SLOW];
    you.mutation[MUT_BREATHE_FLAMES] = 0;
    if (you.species != SP_NAGA)
        you.mutation[MUT_SPIT_POISON] = 0;
#endif

    for (int j = count; j < NUM_MUTATIONS; ++j)
        you.mutation[j] = you.innate_mutation[j] = you.sacrifices[j];

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_NO_POTION_HEAL)
    {   // These use to apply no matter what the minor tag
        // was, so when TAG_MINOR_NO_POTION_HEAL was added
        // these were all moved to only apply to previous
        // tags.
        if (you.mutation[MUT_TELEPORT_CONTROL] == 1)
            you.mutation[MUT_TELEPORT_CONTROL] = 0;
        if (you.mutation[MUT_TRAMPLE_RESISTANCE] > 0)
            you.mutation[MUT_TRAMPLE_RESISTANCE] = 0;
        if (you.mutation[MUT_CLING] == 1)
            you.mutation[MUT_CLING] = 0;
        if (you.species == SP_GARGOYLE)
        {
            you.mutation[MUT_POISON_RESISTANCE] =
            you.innate_mutation[MUT_POISON_RESISTANCE] = 0;
        }
        if (you.species == SP_FORMICID)
        {
            you.mutation[MUT_ANTENNAE] = you.innate_mutation[MUT_ANTENNAE] = 3;
            you.mutation[MUT_EXOSKELETON] =
            you.innate_mutation[MUT_EXOSKELETON] = 0;
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_DIET_MUT)
    {
        you.mutation[MUT_CARNIVOROUS] = you.innate_mutation[MUT_CARNIVOROUS];
        you.mutation[MUT_HERBIVOROUS] = you.innate_mutation[MUT_HERBIVOROUS];

    }

    if (th.getMinorVersion() < TAG_MINOR_SAPROVOROUS
        && you.species == SP_OGRE)
    {
        // Remove the innate level of fast metabolism
        you.mutation[MUT_FAST_METABOLISM] -= 1;
        you.innate_mutation[MUT_FAST_METABOLISM] -= 1;
    }

    if (th.getMinorVersion() < TAG_MINOR_CE_HA_DIET)
    {
        if (you.species == SP_CENTAUR)
        {
            you.mutation[MUT_FAST_METABOLISM] -= 1;
            you.innate_mutation[MUT_FAST_METABOLISM] -= 1;

            you.mutation[MUT_HERBIVOROUS] -= 1;
            you.innate_mutation[MUT_HERBIVOROUS] -= 1;
        }
        else if (you.species == SP_HALFLING)
        {
            you.mutation[MUT_SLOW_METABOLISM] -= 1;
            you.innate_mutation[MUT_SLOW_METABOLISM] -= 1;
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_ROT_IMMUNITY)
    {
        if (you.species == SP_VINE_STALKER)
        {
            you.mutation[MUT_NO_POTION_HEAL] =
            you.innate_mutation[MUT_NO_POTION_HEAL] = 3;
        }

        if (you.species == SP_VINE_STALKER
            || you.species == SP_GARGOYLE)
        {
            you.mutation[MUT_ROT_IMMUNITY] =
            you.innate_mutation[MUT_ROT_IMMUNITY] = 1;
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_FOUL_STENCH
        && you.species == SP_DEMONSPAWN
        && you.innate_mutation[MUT_SAPROVOROUS])
    {
        you.mutation[MUT_ROT_IMMUNITY] =
        you.innate_mutation[MUT_ROT_IMMUNITY] = 1;
    }

    you.mutation[MUT_SAPROVOROUS] =
    you.innate_mutation[MUT_SAPROVOROUS] = 0;

    if (th.getMinorVersion() < TAG_MINOR_DS_CLOUD_MUTATIONS
        && you.species == SP_DEMONSPAWN)
    {
        if (you.innate_mutation[MUT_CONSERVE_POTIONS])
        {
            you.mutation[MUT_CONSERVE_POTIONS] =
            you.innate_mutation[MUT_CONSERVE_POTIONS] = 0;

            you.mutation[MUT_FREEZING_CLOUD_IMMUNITY] =
            you.innate_mutation[MUT_FREEZING_CLOUD_IMMUNITY] = 1;
        }
        if (you.innate_mutation[MUT_CONSERVE_SCROLLS])
        {
            you.mutation[MUT_CONSERVE_SCROLLS] =
            you.innate_mutation[MUT_CONSERVE_SCROLLS] = 0;

            you.mutation[MUT_FLAME_CLOUD_IMMUNITY] =
            you.innate_mutation[MUT_FLAME_CLOUD_IMMUNITY] = 1;
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_METABOLISM)
    {
        you.mutation[MUT_FAST_METABOLISM] =
        you.innate_mutation[MUT_FAST_METABOLISM];

        you.mutation[MUT_SLOW_METABOLISM] =
        you.innate_mutation[MUT_SLOW_METABOLISM];
    }

    if (th.getMinorVersion() < TAG_MINOR_NO_JUMP
        && you.species == SP_FELID && you.innate_mutation[MUT_JUMP] != 0)
    {
        you.mutation[MUT_JUMP] = 0;
    }

    // No minor version needed: all old felids should get MUT_PAWS.
    if (you.species == SP_FELID && you.innate_mutation[MUT_PAWS] < 1)
        you.mutation[MUT_PAWS] = you.innate_mutation[MUT_PAWS] = 1;

    if (th.getMinorVersion() < TAG_MINOR_SPIT_POISON
        && you.species == SP_NAGA)
    {
        if (you.innate_mutation[MUT_SPIT_POISON] < 2)
        {
            you.mutation[MUT_SPIT_POISON] =
            you.innate_mutation[MUT_SPIT_POISON] = 2;
        }
        if (you.mutation[MUT_BREATHE_POISON])
        {
            you.mutation[MUT_BREATHE_POISON] = 0;
            you.mutation[MUT_SPIT_POISON] = 3;
        }
    }

    // Give nagas constrict, tengu flight, and mummies restoration/enhancers.
    if (th.getMinorVersion() < TAG_MINOR_REAL_MUTS
        && (you.species == SP_NAGA
            || you.species == SP_TENGU
            || you.species == SP_MUMMY))
    {
        for (int xl = 2; xl <= you.experience_level; ++xl)
            give_level_mutations(you.species, xl);
    }

    if (th.getMinorVersion() < TAG_MINOR_NO_FORLORN)
    {
        if (you.mutation[MUT_FORLORN])
            you.mutation[MUT_FORLORN] = 0;
    }

    if (th.getMinorVersion() < TAG_MINOR_MP_WANDS)
    {
        if (you.mutation[MUT_MP_WANDS] > 1)
            you.mutation[MUT_MP_WANDS] = 1;
    }

    if (th.getMinorVersion() < TAG_MINOR_NAGA_METABOLISM)
    {
        if (you.species == SP_NAGA)
        {
            you.mutation[MUT_SLOW_METABOLISM] =
                you.innate_mutation[MUT_SLOW_METABOLISM] = 1;
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_DETERIORATION)
    {
        if (you.mutation[MUT_DETERIORATION] > 2)
            you.mutation[MUT_DETERIORATION] = 2;
    }

    if (th.getMinorVersion() < TAG_MINOR_BLINK_MUT)
    {
        if (you.mutation[MUT_BLINK] > 1)
            you.mutation[MUT_BLINK] = 1;
    }

    if (th.getMinorVersion() < TAG_MINOR_MUMMY_RESTORATION)
    {
        if (you.mutation[MUT_MUMMY_RESTORATION])
            you.mutation[MUT_MUMMY_RESTORATION] = 0;
        if (you.mutation[MUT_SUSTAIN_ATTRIBUTES])
            you.mutation[MUT_SUSTAIN_ATTRIBUTES] = 0;
    }

    if (th.getMinorVersion() < TAG_MINOR_SPIT_POISON_AGAIN)
    {
        if (you.mutation[MUT_SPIT_POISON] > 1)
            you.mutation[MUT_SPIT_POISON] -= 1;
    }

    // Slow regeneration split into two single-level muts:
    // * Inhibited regeneration (no regen in los of monsters, what Gh get)
    // * No regeneration (what DDs get)
    {
        if (you.species == SP_DEEP_DWARF
            && (you.mutation[MUT_INHIBITED_REGENERATION] > 0
                || you.mutation[MUT_NO_REGENERATION] != 1))
        {
            you.innate_mutation[MUT_INHIBITED_REGENERATION] = 0;
            you.mutation[MUT_INHIBITED_REGENERATION] = 0;
            you.innate_mutation[MUT_NO_REGENERATION] = 1;
            you.mutation[MUT_NO_REGENERATION] = 1;
        }
        else if (you.species == SP_GHOUL
                 && you.mutation[MUT_INHIBITED_REGENERATION] > 1)
        {
            you.innate_mutation[MUT_INHIBITED_REGENERATION] = 1;
            you.mutation[MUT_INHIBITED_REGENERATION] = 1;
        }
        else if (you.mutation[MUT_INHIBITED_REGENERATION] > 1)
            you.mutation[MUT_INHIBITED_REGENERATION] = 1;
    }

    // Fixup for Sacrifice XP from XL 27 (#9895). No minor tag, but this
    // should still be removed on a major bump.
    const int xl_remaining = you.get_max_xl() - you.experience_level;
    if (xl_remaining < 0)
        adjust_level(xl_remaining);
#endif

    count = unmarshallUByte(th);
    you.demonic_traits.clear();
    for (int j = 0; j < count; ++j)
    {
        player::demon_trait dt;
        dt.level_gained = unmarshallByte(th);
        ASSERT_RANGE(dt.level_gained, 1, 28);
        dt.mutation = static_cast<mutation_type>(unmarshallShort(th));
#if TAG_MAJOR_VERSION == 34
        if (dt.mutation == MUT_CONSERVE_POTIONS)
            dt.mutation = MUT_FREEZING_CLOUD_IMMUNITY;
        else if (dt.mutation == MUT_CONSERVE_SCROLLS)
            dt.mutation = MUT_FLAME_CLOUD_IMMUNITY;
#endif
        ASSERT_RANGE(dt.mutation, 0, NUM_MUTATIONS);
        you.demonic_traits.push_back(dt);
    }

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_SAC_PIETY_LEN)
    {
        const int OLD_NUM_ABILITIES = 1503;

        // set up sacrifice piety by abilities
        for (int j = 0; j < NUM_ABILITIES; ++j)
        {
            if (th.getMinorVersion() < TAG_MINOR_RU_PIETY_CONSISTENCY
                || j >= OLD_NUM_ABILITIES) // NUM_ABILITIES may have increased
            {
                you.sacrifice_piety[j] = 0;
            }
            else
                you.sacrifice_piety[j] = unmarshallUByte(th);
        }

        // If NUM_ABILITIES decreased, discard the extras.
        if (th.getMinorVersion() >= TAG_MINOR_RU_PIETY_CONSISTENCY)
        {
            for (int j = NUM_ABILITIES; j < OLD_NUM_ABILITIES; ++j)
                (void) unmarshallUByte(th);
        }
    }
    else
#endif
    {
        const int num_saved = unmarshallShort(th);

        you.sacrifice_piety.init(0);
        for (int j = 0; j < num_saved; ++j)
        {
            const int idx = ABIL_FIRST_SACRIFICE + j;
            const uint8_t val = unmarshallUByte(th);
            if (idx <= ABIL_FINAL_SACRIFICE)
                you.sacrifice_piety[idx] = val;
        }
    }

    EAT_CANARY;

    // how many penances?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_GODS);
    for (int i = 0; i < count; i++)
    {
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() < TAG_MINOR_XP_PENANCE && i == GOD_GOZAG)
        {
            unmarshallUByte(th);
            you.penance[i] = 0;
            continue;
        }
#endif
        you.penance[i] = unmarshallUByte(th);
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() < TAG_MINOR_NEMELEX_WRATH
            && player_under_penance(GOD_NEMELEX_XOBEH)
            && i == GOD_NEMELEX_XOBEH)
        {
            you.penance[i] = max(you.penance[i] - 100, 0);
        }
#endif
        ASSERT(you.penance[i] <= MAX_PENANCE);
    }

#if TAG_MAJOR_VERSION == 34
    // Fix invalid ATTR_GOD_WRATH_XP if no god is giving penance.
    // cf. 0.14-a0-2640-g5c5a558
    if (you.attribute[ATTR_GOD_WRATH_XP] != 0
        || you.attribute[ATTR_GOD_WRATH_COUNT] != 0)
    {
        god_iterator it;
        for (; it; ++it)
        {
            if (player_under_penance(*it))
                break;
        }
        if (!it)
        {
            you.attribute[ATTR_GOD_WRATH_XP] = 0;
            you.attribute[ATTR_GOD_WRATH_COUNT] = 0;
        }
    }
#endif

    for (int i = 0; i < count; i++)
        you.worshipped[i] = unmarshallByte(th);

    for (int i = 0; i < count; i++)
        you.num_current_gifts[i] = unmarshallShort(th);
    for (int i = 0; i < count; i++)
        you.num_total_gifts[i] = unmarshallShort(th);
    for (int i = 0; i < count; i++)
        you.one_time_ability_used.set(i, unmarshallBoolean(th));
    for (int i = 0; i < count; i++)
        you.piety_max[i] = unmarshallByte(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_NEMELEX_DUNGEONS)
    {
        unmarshallByte(th);
        for (int i = 0; i < NEM_GIFT_SUMMONING; i++)
            unmarshallBoolean(th);
        unmarshallBoolean(th); // dungeons weight
        for (int i = NEM_GIFT_SUMMONING; i < NUM_NEMELEX_GIFT_TYPES; i++)
            unmarshallBoolean(th);
    }
    else if (th.getMinorVersion() < TAG_MINOR_NEMELEX_WEIGHTS)
    {
        count = unmarshallByte(th);
        ASSERT(count == NUM_NEMELEX_GIFT_TYPES);
        for (int i = 0; i < count; i++)
            unmarshallBoolean(th);
    }
#endif

    you.gift_timeout   = unmarshallByte(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_SAVED_PIETY)
    {
        you.saved_good_god_piety = 0;
        you.previous_good_god = GOD_NO_GOD;
    }
    else
    {
#endif
    you.saved_good_god_piety = unmarshallUByte(th);
    you.previous_good_god = static_cast<god_type>(unmarshallByte(th));
#if TAG_MAJOR_VERSION == 34
    }
#endif

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_BRANCH_ENTRY)
    {
        int depth = unmarshallByte(th);
        branch_type br = static_cast<branch_type>(unmarshallByte(th));
        ASSERT(br < NUM_BRANCHES);
        brentry[BRANCH_VESTIBULE] = level_id(br, depth);
    }
#endif

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_XP_PENANCE)
    {
        for (god_iterator it; it; ++it)
        {
            if (*it == GOD_ASHENZARI)
                you.exp_docked[*it] = unmarshallInt(th);
            else
                you.exp_docked[*it] = 0;
        }
        for (god_iterator it; it; ++it)
        {
            if (*it == GOD_ASHENZARI)
                you.exp_docked_total[*it] = unmarshallInt(th);
            else
                you.exp_docked_total[*it] = 0;
        }
    }
    else
    {
#endif
    for (int i = 0; i < count; i++)
        you.exp_docked[i] = unmarshallInt(th);
    for (int i = 0; i < count; i++)
        you.exp_docked_total[i] = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
    }
    if (th.getMinorVersion() < TAG_MINOR_PAKELLAS_WRATH
        && player_under_penance(GOD_PAKELLAS))
    {
        you.exp_docked[GOD_PAKELLAS] = exp_needed(min<int>(you.max_level, 27) + 1)
                                  - exp_needed(min<int>(you.max_level, 27));
        you.exp_docked_total[GOD_PAKELLAS] = you.exp_docked[GOD_PAKELLAS];
    }
    if (th.getMinorVersion() < TAG_MINOR_ELYVILON_WRATH
        && player_under_penance(GOD_ELYVILON))
    {
        you.exp_docked[GOD_ELYVILON] = exp_needed(min<int>(you.max_level, 27) + 1)
                                  - exp_needed(min<int>(you.max_level, 27));
        you.exp_docked_total[GOD_ELYVILON] = you.exp_docked[GOD_ELYVILON];
    }

#endif

    // elapsed time
    you.elapsed_time   = unmarshallInt(th);
    you.elapsed_time_at_last_input = you.elapsed_time;

    // Initialize new timers now that we know the time.
    const int last_20_turns = you.elapsed_time - (you.elapsed_time % 200);
    for (int j = timer_count; j < NUM_TIMERS; ++j)
    {
        you.last_timer_effect[j] = last_20_turns;
        you.next_timer_effect[j] = last_20_turns + 200;
    }

    // Verify that timers aren't scheduled for the past.
    for (int j = 0; j < NUM_TIMERS; ++j)
    {
        if (you.next_timer_effect[j] < you.elapsed_time)
        {
#if TAG_MAJOR_VERSION == 34
            if (th.getMinorVersion() >= TAG_MINOR_EVENT_TIMERS
                && th.getMinorVersion() < TAG_MINOR_EVENT_TIMER_FIX)
            {
                dprf("Fixing up timer %d from %d to %d",
                     j, you.next_timer_effect[j], last_20_turns + 200);
                you.last_timer_effect[j] = last_20_turns;
                you.next_timer_effect[j] = last_20_turns + 200;
            }
            else
#endif
            die("Timer %d next trigger in the past [%d < %d]",
                j, you.next_timer_effect[j], you.elapsed_time);
        }
    }

    // time of character creation
    you.birth_time = unmarshallInt(th);

    const int real_time  = unmarshallInt(th);
    you.real_time_ms = chrono::milliseconds(real_time * 1000);
    you.num_turns  = unmarshallInt(th);
    you.exploration = unmarshallInt(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_CONTAM_SCALE)
        you.magic_contamination = unmarshallShort(th) * 1000;
    else
#endif
        you.magic_contamination = unmarshallInt(th);

#if TAG_MAJOR_VERSION == 34
    unmarshallUByte(th);
#endif
    you.transit_stair  = unmarshallFeatureType(th);
    you.entering_level = unmarshallByte(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_TRAVEL_ALLY_PACE)
    {
#endif
        you.travel_ally_pace = unmarshallBoolean(th);
#if TAG_MAJOR_VERSION == 34
    }
#endif

    you.deaths = unmarshallByte(th);
    you.lives = unmarshallByte(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_LORC_TEMPERATURE &&
        th.getMinorVersion() < TAG_MINOR_NO_MORE_LORC)
    {
        // These were once the temperature fields on player for lava orcs.
        // Still need to unmarshall them from older saves.
        unmarshallFloat(th); // was you.temperature
        unmarshallFloat(th); // was you.temperature_last
    }
#endif

    you.pending_revival = !you.hp;

    EAT_CANARY;

    int n_dact = unmarshallInt(th);
    ASSERT_RANGE(n_dact, 0, 100000); // arbitrary, sanity check
    you.dactions.resize(n_dact, NUM_DACTIONS);
    for (int i = 0; i < n_dact; i++)
    {
        int a = unmarshallUByte(th);
        ASSERT(a < NUM_DACTIONS);
        you.dactions[i] = static_cast<daction_type>(a);
    }

    you.level_stack.clear();
    int n_levs = unmarshallInt(th);
    for (int k = 0; k < n_levs; k++)
    {
        level_pos pos;
        pos.load(th);
        you.level_stack.push_back(pos);
    }

    // List of currently beholding monsters (usually empty).
    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (int i = 0; i < count; i++)
    {
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_MID_BEHOLDERS)
    {
        unmarshallShort(th);
        you.duration[DUR_MESMERISED] = 0;
    }
    else
#endif
        you.beholders.push_back(unmarshall_int_as<mid_t>(th));
    }

    // Also usually empty.
    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (int i = 0; i < count; i++)
    {
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_MID_BEHOLDERS)
    {
        unmarshallShort(th);
        you.duration[DUR_AFRAID] = 0;
    }
    else
#endif
        you.fearmongers.push_back(unmarshall_int_as<mid_t>(th));
    }

    you.piety_hysteresis = unmarshallByte(th);

    you.m_quiver.load(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_FRIENDLY_PICKUP)
        unmarshallByte(th);
    if (th.getMinorVersion() < TAG_MINOR_NO_ZOTDEF)
        unmarshallString(th);
#endif

    EAT_CANARY;

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() == TAG_MINOR_0_11)
    {
        for (unsigned int k = 0; k < 5; k++)
            unmarshallInt(th);
    }
#endif

    // Counts of actions made, by type.
    count = unmarshallShort(th);
    for (int i = 0; i < count; i++)
    {
        caction_type caction = (caction_type)unmarshallShort(th);
        int subtype = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
        if ((th.getMinorVersion() < TAG_MINOR_ACTION_THROW
             || th.getMinorVersion() == TAG_MINOR_0_11) && caction == CACT_THROW)
        {
            subtype = subtype | (OBJ_MISSILES << 16);
        }
#endif
        for (int j = 0; j < 27; j++)
            you.action_count[make_pair(caction, subtype)][j] = unmarshallInt(th);
    }

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_BRANCHES_LEFT) // 33:17 has it
    {
#endif
    count = unmarshallByte(th);
    for (int i = 0; i < count; i++)
        you.branches_left.set(i, unmarshallBoolean(th));
#if TAG_MAJOR_VERSION == 34
    }
    else
    {
        // Assume all branches already exited in transferred games.
        you.branches_left.init(true);
    }
#endif

    abyssal_state.major_coord = unmarshallCoord(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_DEEP_ABYSS
        && th.getMinorVersion() != TAG_MINOR_0_11)
    {
        if (th.getMinorVersion() >= TAG_MINOR_REMOVE_ABYSS_SEED
            && th.getMinorVersion() < TAG_MINOR_ADD_ABYSS_SEED)
        {
            abyssal_state.seed = get_uint32();
        }
        else
#endif
            abyssal_state.seed = unmarshallInt(th);
        abyssal_state.depth = unmarshallInt(th);
        abyssal_state.destroy_all_terrain = false;
#if TAG_MAJOR_VERSION == 34
    }
    else
    {
        unmarshallFloat(th); // converted abyssal_state.depth to int.
        abyssal_state.depth = 0;
        abyssal_state.destroy_all_terrain = true;
        abyssal_state.seed = get_uint32();
    }
#endif
    abyssal_state.phase = unmarshallFloat(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_ABYSS_BRANCHES)
        abyssal_state.level = unmarshall_level_id(th);
    if (!abyssal_state.level.is_valid())
    {
        abyssal_state.level.branch = BRANCH_DEPTHS;
        abyssal_state.level.depth = 1;
    }
#else
    abyssal_state.level = unmarshall_level_id(th);
#endif

    _unmarshall_constriction(th, &you);

    you.octopus_king_rings = unmarshallUByte(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_UNCANCELLABLES
        && th.getMinorVersion() != TAG_MINOR_0_11)
    {
#endif
    count = unmarshallUnsigned(th);
    ASSERT_RANGE(count, 0, 16); // sanity check
    you.uncancel.resize(count);
    for (int i = 0; i < count; i++)
    {
        you.uncancel[i].first = (uncancellable_type)unmarshallUByte(th);
        you.uncancel[i].second = unmarshallInt(th);
    }
#if TAG_MAJOR_VERSION == 34
    }

    if (th.getMinorVersion() >= TAG_MINOR_INCREMENTAL_RECALL)
    {
#endif
    count = unmarshallUnsigned(th);
    you.recall_list.resize(count);
    for (int i = 0; i < count; i++)
        you.recall_list[i] = unmarshall_int_as<mid_t>(th);
#if TAG_MAJOR_VERSION == 34
    }

    if (th.getMinorVersion() >= TAG_MINOR_SEEDS)
    {
#endif
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_SEEDS);
    for (int i = 0; i < count; i++)
        you.game_seeds[i] = unmarshallInt(th);
    for (int i = count; i < NUM_SEEDS; i++)
        you.game_seeds[i] = get_uint32();
#if TAG_MAJOR_VERSION == 34
    }
#endif

    EAT_CANARY;

    if (!dlua.callfn("dgn_load_data", "u", &th))
    {
        mprf(MSGCH_ERROR, "Failed to load Lua persist table: %s",
             dlua.error.c_str());
    }

    EAT_CANARY;

    crawl_state.save_rcs_version = unmarshallString(th);

    you.props.clear();
    you.props.read(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_STICKY_FLAME)
    {
        if (you.props.exists("napalmer"))
            you.props["sticky_flame_source"] = you.props["napalmer"];
        if (you.props.exists("napalm_aux"))
            you.props["sticky_flame_aux"] = you.props["napalm_aux"];
    }

    if (you.duration[DUR_EXCRUCIATING_WOUNDS] && !you.props.exists(ORIGINAL_BRAND_KEY))
        you.props[ORIGINAL_BRAND_KEY] = SPWPN_NORMAL;

    // Both saves prior to TAG_MINOR_RU_DELAY_STACKING, and saves transferred
    // from before that tag to a version where this minor tag was backwards.
    if (!you.props.exists(RU_SACRIFICE_PENALTY_KEY))
        you.props[RU_SACRIFICE_PENALTY_KEY] = 0;
    if (th.getMinorVersion() < TAG_MINOR_ZIGFIGS)
        you.props["zig-fixup"] = true;
#endif
}

static void tag_read_you_items(reader &th)
{
    int count, count2;

    // how many inventory slots?
    count = unmarshallByte(th);
    ASSERT(count == ENDOFPACK); // not supposed to change
#if TAG_MAJOR_VERSION == 34
    string bad_slots;
#endif
    for (int i = 0; i < count; ++i)
    {
        item_def &it = you.inv[i];
        unmarshallItem(th, it);
#if TAG_MAJOR_VERSION == 34
        // Fixups for actual items.
        if (it.defined())
        {
            // From 0.18-a0-273-gf174401 to 0.18-a0-290-gf199c8b, stash
            // search would change the position of items in inventory.
            if (it.pos != ITEM_IN_INVENTORY)
            {
                bad_slots += index_to_letter(i);
                it.pos = ITEM_IN_INVENTORY;
            }

            // Items in inventory have already been handled.
            if (th.getMinorVersion() < TAG_MINOR_ISFLAG_HANDLED)
                it.flags |= ISFLAG_HANDLED;
        }
#endif
    }
#if TAG_MAJOR_VERSION == 34
    if (!bad_slots.empty())
    {
        mprf(MSGCH_ERROR, "Fixed bad positions for inventory slots %s",
                          bad_slots.c_str());
    }
#endif

    // Initialize cache of equipped unrand functions
    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
    {
        const item_def *item = you.slot_item(static_cast<equipment_type>(i));

        if (item && is_unrandom_artefact(*item))
        {
#if TAG_MAJOR_VERSION == 34
            // save-compat: if the player was wearing the Ring of Vitality before
            // it turned into an amulet, take it off safely
            if (is_unrandom_artefact(*item, UNRAND_VITALITY) && i != EQ_AMULET)
            {
                you.equip[i] = -1;
                you.melded.set(i, false);
                // XXX: need to update ash bondage, or is this too early?
                continue;
            }
#endif

            const unrandart_entry *entry = get_unrand_entry(item->unrand_idx);

            if (entry->world_reacts_func)
                you.unrand_reacts.set(i);
        }
    }

    unmarshallFixedBitVector<NUM_RUNE_TYPES>(th, you.runes);
    you.obtainable_runes = unmarshallByte(th);

    // Item descrip for each type & subtype.
    // how many types?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_IDESC);
    // how many subtypes?
    count2 = unmarshallUByte(th);
    ASSERT(count2 <= MAX_SUBTYPES);
    for (int i = 0; i < count; ++i)
        for (int j = 0; j < count2; ++j)
#if TAG_MAJOR_VERSION == 34
        {
            if (th.getMinorVersion() < TAG_MINOR_CONSUM_APPEARANCE)
                you.item_description[i][j] = unmarshallUByte(th);
            else
#endif
                you.item_description[i][j] = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
            // We briefly had a problem where we sign-extended the old
            // 8-bit item_descriptions on conversion. Fix those up.
            if (th.getMinorVersion() < TAG_MINOR_NEG_IDESC
                && (int)you.item_description[i][j] < 0)
            {
                you.item_description[i][j] &= 0xff;
            }
        }
#endif
    for (int i = 0; i < count; ++i)
        for (int j = count2; j < MAX_SUBTYPES; ++j)
            you.item_description[i][j] = 0;
    int iclasses = unmarshallUByte(th);
    ASSERT(iclasses <= NUM_OBJECT_CLASSES);

    // Identification status.
    for (int i = 0; i < iclasses; ++i)
    {
        if (!item_type_has_ids((object_class_type)i))
            continue;
        for (int j = 0; j < count2; ++j)
        {
#if TAG_MAJOR_VERSION == 34
            if (th.getMinorVersion() < TAG_MINOR_ID_STATES)
            {
                uint8_t x;

                if (th.getMinorVersion() < TAG_MINOR_BOOK_ID && i == OBJ_BOOKS)
                    x = ID_UNKNOWN_TYPE;
                else
                    x = unmarshallUByte(th);

                ASSERT(x < NUM_ID_STATE_TYPES);
                if (x > ID_UNKNOWN_TYPE)
                    you.type_ids[i][j] = true;
                else
                    you.type_ids[i][j] = false;
            }
            else
#endif
            you.type_ids[i][j] = unmarshallBoolean(th);
        }
        for (int j = count2; j < MAX_SUBTYPES; ++j)
            you.type_ids[i][j] = false;
    }

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_ID_STATES)
    {
        CrawlHashTable old_type_id_props;
        old_type_id_props.read(th);
    }
#endif

    EAT_CANARY;

    // how many unique items?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_UNRANDARTS <= 256);
    for (int j = 0; j < count && j < NUM_UNRANDARTS; ++j)
    {
        you.unique_items[j] =
            static_cast<unique_item_status_type>(unmarshallByte(th));
    }
    // # of unrandarts could certainly change.
    // If it does, the new ones won't exist yet - zero them out.
    for (int j = count; j < NUM_UNRANDARTS; j++)
        you.unique_items[j] = UNIQ_NOT_EXISTS;
    for (int j = NUM_UNRANDARTS; j < count; j++)
        unmarshallByte(th);

    // how many books?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_FIXED_BOOKS <= 256);
    for (int j = 0; j < count && j < NUM_FIXED_BOOKS; ++j)
        you.had_book.set(j, unmarshallByte(th));
    for (int j = count; j < NUM_FIXED_BOOKS; ++j)
        you.had_book.set(j, false);
    for (int j = NUM_FIXED_BOOKS; j < count; ++j)
        unmarshallByte(th);

    // how many spells?
    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (int j = 0; j < count && j < NUM_SPELLS; ++j)
        you.seen_spell.set(j, unmarshallByte(th));
    for (int j = count; j < NUM_SPELLS; ++j)
        you.seen_spell.set(j, false);
    for (int j = NUM_SPELLS; j < count; ++j)
        unmarshallByte(th);

    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (int j = 0; j < count && j < NUM_WEAPONS; ++j)
        you.seen_weapon[j] = unmarshallInt(th);
    for (int j = count; j < NUM_WEAPONS; ++j)
        you.seen_weapon[j] = 0;
    for (int j = NUM_WEAPONS; j < count; ++j)
        unmarshallInt(th);

    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (int j = 0; j < count && j < NUM_ARMOURS; ++j)
        you.seen_armour[j] = unmarshallInt(th);
    for (int j = count; j < NUM_ARMOURS; ++j)
        you.seen_armour[j] = 0;
    for (int j = NUM_ARMOURS; j < count; ++j)
        unmarshallInt(th);
    unmarshallFixedBitVector<NUM_MISCELLANY>(th, you.seen_misc);

    for (int i = 0; i < iclasses; i++)
        for (int j = 0; j < count2; j++)
            you.force_autopickup[i][j] = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_FOOD_AUTOPICKUP)
    {
        const int oldstate = you.force_autopickup[OBJ_FOOD][NUM_FOODS];
        you.force_autopickup[OBJ_FOOD][FOOD_MEAT_RATION] = oldstate;
        you.force_autopickup[OBJ_FOOD][FOOD_FRUIT] = oldstate;
        you.force_autopickup[OBJ_FOOD][FOOD_ROYAL_JELLY] = oldstate;

        you.force_autopickup[OBJ_BOOKS][BOOK_MANUAL] =
            you.force_autopickup[OBJ_BOOKS][NUM_BOOKS];
    }
    if (th.getMinorVersion() < TAG_MINOR_FOOD_PURGE_AP_FIX)
    {
        FixedVector<int, MAX_SUBTYPES> &food_pickups =
            you.force_autopickup[OBJ_FOOD];

        // If fruit pickup was not set explicitly during the time between
        // FOOD_PURGE and FOOD_PURGE_AP_FIX, copy the old exemplar FOOD_PEAR.
        if (food_pickups[FOOD_FRUIT] == 0)
            food_pickups[FOOD_FRUIT] = food_pickups[FOOD_PEAR];
    }
    if (you.species == SP_FORMICID)
        remove_one_equip(EQ_HELMET, false, true);

    if (th.getMinorVersion() < TAG_MINOR_CONSUM_APPEARANCE)
    {
        // merge scroll seeds
        for (int subtype = 0; subtype < MAX_SUBTYPES; subtype++)
        {
            const int seed1 = you.item_description[IDESC_SCROLLS][subtype]
                              & 0xff;
            const int seed2 = you.item_description[IDESC_SCROLLS_II][subtype]
                              & 0xff;
            const int seed3 = OBJ_SCROLLS & 0xff;
            you.item_description[IDESC_SCROLLS][subtype] =    seed1
                                                           | (seed2 << 8)
                                                           | (seed3 << 16);
        }
    }
    // Remove any decks if no longer worshipping Nemelex, now that items have
    // been loaded.
    if (th.getMinorVersion() < TAG_MINOR_NEMELEX_WRATH
        && !you_worship(GOD_NEMELEX_XOBEH)
        && you.num_total_gifts[GOD_NEMELEX_XOBEH])
    {
        nemelex_reclaim_decks();
    }
#endif
}

static PlaceInfo unmarshallPlaceInfo(reader &th)
{
    PlaceInfo place_info;

#if TAG_MAJOR_VERSION == 34
    int br = unmarshallInt(th);
    // This is for extremely old saves that predate NUM_BRANCHES, probably only
    // a very small window of time in the 34 major version.
    if (br == -1)
        br = GLOBAL_BRANCH_INFO;
    ASSERT(br >= 0);
    // at the time NUM_BRANCHES was one above BRANCH_DEPTHS, so we check that
    if (th.getMinorVersion() < TAG_MINOR_GLOBAL_BR_INFO && br == BRANCH_DEPTHS+1)
        br = GLOBAL_BRANCH_INFO;
    place_info.branch      = static_cast<branch_type>(br);
#else
    place_info.branch      = static_cast<branch_type>(unmarshallInt(th));
#endif

    place_info.num_visits  = unmarshallInt(th);
    place_info.levels_seen = unmarshallInt(th);

    place_info.mon_kill_exp       = unmarshallInt(th);

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

#if TAG_MAJOR_VERSION == 34
static branch_type old_entries[] =
{
    /* D */      NUM_BRANCHES,
    /* Temple */ BRANCH_DUNGEON,
    /* Orc */    BRANCH_DUNGEON,
    /* Elf */    BRANCH_ORC,
    /* Dwarf */  BRANCH_ELF,
    /* Lair */   BRANCH_DUNGEON,
    /* Swamp */  BRANCH_LAIR,
    /* Shoals */ BRANCH_LAIR,
    /* Snake */  BRANCH_LAIR,
    /* Spider */ BRANCH_LAIR,
    /* Slime */  BRANCH_LAIR,
    /* Vaults */ BRANCH_DUNGEON,
    /* Blade */  BRANCH_VAULTS,
    /* Crypt */  BRANCH_VAULTS,
    /* Tomb */   BRANCH_CRYPT, // or Forest
    /* Hell */   NUM_BRANCHES,
    /* Dis */    BRANCH_VESTIBULE,
    /* Geh */    BRANCH_VESTIBULE,
    /* Coc */    BRANCH_VESTIBULE,
    /* Tar */    BRANCH_VESTIBULE,
    /* Zot */    BRANCH_DUNGEON,
    /* Forest */ BRANCH_VAULTS,
    /* Abyss */  NUM_BRANCHES,
    /* Pan */    NUM_BRANCHES,
    /* various portal branches */ NUM_BRANCHES,
    NUM_BRANCHES, NUM_BRANCHES, NUM_BRANCHES, NUM_BRANCHES, NUM_BRANCHES,
    NUM_BRANCHES, NUM_BRANCHES, NUM_BRANCHES, NUM_BRANCHES, NUM_BRANCHES,
    NUM_BRANCHES,
};
#endif

static void tag_read_you_dungeon(reader &th)
{
    // how many unique creatures?
    int count = unmarshallShort(th);
    you.unique_creatures.reset();
    for (int j = 0; j < count; ++j)
    {
        const bool created = unmarshallBoolean(th);

        if (j < NUM_MONSTERS)
            you.unique_creatures.set(j, created);
    }

    // how many branches?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_BRANCHES);
    for (int j = 0; j < count; ++j)
    {
        brdepth[j]    = unmarshallInt(th);
        ASSERT_RANGE(brdepth[j], -1, MAX_BRANCH_DEPTH + 1);
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() < TAG_MINOR_BRANCH_ENTRY)
        {
            int depth = unmarshallInt(th);
            if (j != BRANCH_VESTIBULE)
                brentry[j] = level_id(old_entries[j], depth);
        }
        else
#endif
        brentry[j]    = unmarshall_level_id(th);
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() < TAG_MINOR_BRIBE_BRANCH)
            branch_bribe[j] = 0;
        else
#endif
        branch_bribe[j] = unmarshallInt(th);
    }
    // Initialize data for any branches added after this save version.
    for (int j = count; j < NUM_BRANCHES; ++j)
    {
        brdepth[j] = branches[j].numlevels;
        brentry[j] = level_id(branches[j].parent_branch, branches[j].mindepth);
        branch_bribe[j] = 0;
    }

#if TAG_MAJOR_VERSION == 34
    // Deepen the Abyss; this is okay since new abyssal stairs will be
    // generated as the place shifts.
    if (crawl_state.game_is_normal() && th.getMinorVersion() <= TAG_MINOR_ORIG_MONNUM)
        brdepth[BRANCH_ABYSS] = 5;
#endif

    ASSERT(you.depth <= brdepth[you.where_are_you]);

    // Root of the dungeon; usually BRANCH_DUNGEON.
    root_branch = static_cast<branch_type>(unmarshallInt(th));
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_BRANCH_ENTRY)
    {
        brentry[root_branch].clear();
        if (brentry[BRANCH_FOREST].is_valid())
            brentry[BRANCH_TOMB].branch = BRANCH_FOREST;
    }
#endif

    unmarshallMap(th, stair_level,
                  unmarshall_int_as<branch_type>,
                  _unmarshall_level_id_set);
    unmarshallMap(th, shops_present,
                  _unmarshall_level_pos, unmarshall_int_as<shop_type>);
    unmarshallMap(th, altars_present,
                  _unmarshall_level_pos, unmarshall_int_as<god_type>);
    unmarshallMap(th, portals_present,
                  _unmarshall_level_pos, unmarshall_int_as<branch_type>);
    unmarshallMap(th, portal_notes,
                  _unmarshall_level_pos, unmarshallString);
    unmarshallMap(th, level_annotations,
                  unmarshall_level_id, unmarshallString);
    unmarshallMap(th, level_exclusions,
                  unmarshall_level_id, unmarshallString);
    unmarshallMap(th, level_uniques,
                  unmarshall_level_id, unmarshallString);
    unmarshallUniqueAnnotations(th);

    PlaceInfo place_info = unmarshallPlaceInfo(th);
    ASSERT(place_info.is_global());
    you.set_place_info(place_info);

    unsigned short count_p = (unsigned short) unmarshallShort(th);

    auto places = you.get_all_place_info();
    // Use "<=" so that adding more branches or non-dungeon places
    // won't break save-file compatibility.
    ASSERT(count_p <= places.size());

    for (int i = 0; i < count_p; i++)
    {
        place_info = unmarshallPlaceInfo(th);
        if (place_info.is_global()
            && th.getMinorVersion() <= TAG_MINOR_DESOLATION_GLOBAL)
        {
            // This is to fix some crashing saves that didn't import
            // correctly, where the desolation slot occasionally gets marked
            // as global on conversion from pre-0.19 to post-0.19a.   This
            // assumes that the order in `logical_branch_order` (branch.cc)
            // hasn't changed since the save version (which is moderately safe).
            const branch_type branch_to_fix = places[i].branch;
            ASSERT(branch_to_fix == BRANCH_DESOLATION);
            place_info.branch = branch_to_fix;
        }
        else
        {
            // These should all be branch-specific, not global
            ASSERT(!place_info.is_global());
        }
        you.set_place_info(place_info);
    }
    typedef pair<string_set::iterator, bool> ssipair;
    unmarshall_container(th, you.uniq_map_tags,
                         (ssipair (string_set::*)(const string &))
                         &string_set::insert,
                         unmarshallString);
    unmarshall_container(th, you.uniq_map_names,
                         (ssipair (string_set::*)(const string &))
                         &string_set::insert,
                         unmarshallString);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_VAULT_LIST) // 33:17 has it
#endif
    unmarshallMap(th, you.vault_list, unmarshall_level_id,
                  unmarshallStringVector);

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

static void tag_read_companions(reader &th)
{
    companion_list.clear();

    unmarshallMap(th, companion_list, unmarshall_int_as<mid_t>,
                  unmarshall_companion);
}

template <typename Z>
static int _last_used_index(const Z &thinglist, int max_things)
{
    for (int i = max_things - 1; i >= 0; --i)
        if (thinglist[i].defined())
            return i + 1;
    return 0;
}

// ------------------------------- level tags ---------------------------- //

static void tag_construct_level(writer &th)
{
    marshallByte(th, env.floor_colour);
    marshallByte(th, env.rock_colour);

    marshallInt(th, you.on_current_level ? you.elapsed_time : env.elapsed_time);
    marshallCoord(th, you.pos());

    // Map grids.
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    marshallInt(th, env.turns_on_level);

    CANARY;

    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallByte(th, grd[count_x][count_y]);
            marshallMapCell(th, env.map_knowledge[count_x][count_y]);
            marshallInt(th, env.pgrid[count_x][count_y]);
        }

    marshallBoolean(th, !!env.map_forgotten.get());
    if (env.map_forgotten.get())
        for (int x = 0; x < GXM; x++)
            for (int y = 0; y < GYM; y++)
                marshallMapCell(th, (*env.map_forgotten)[x][y]);

    _run_length_encode(th, marshallByte, env.grid_colours, GXM, GYM);

    CANARY;

    // how many clouds?
    marshallShort(th, env.cloud.size());
    for (const auto& entry : env.cloud)
    {
        const cloud_struct& cloud = entry.second;
        marshallByte(th, cloud.type);
        ASSERT(cloud.type != CLOUD_NONE);
        ASSERT_IN_BOUNDS(cloud.pos);
        marshallByte(th, cloud.pos.x);
        marshallByte(th, cloud.pos.y);
        marshallShort(th, cloud.decay);
        marshallByte(th, cloud.spread_rate);
        marshallByte(th, cloud.whose);
        marshallByte(th, cloud.killer);
        marshallInt(th, cloud.source);
        marshallInt(th, cloud.excl_rad);
    }

    CANARY;

    // how many shops?
    marshallShort(th, env.shop.size());
    for (const auto& entry : env.shop)
        marshall_shop(th, entry.second);

    CANARY;

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

    CANARY;

    marshallInt(th, env.forest_awoken_until);
    marshall_level_vault_data(th);
    marshallInt(th, env.density);

    marshallShort(th, env.sunlight.size());
    for (const auto &sunspot : env.sunlight)
    {
        marshallCoord(th, sunspot.first);
        marshallInt(th, sunspot.second);
    }
}

void marshallItem(writer &th, const item_def &item, bool iinfo)
{
    marshallByte(th, item.base_type);
    if (item.base_type == OBJ_UNASSIGNED)
        return;

#if TAG_MAJOR_VERSION == 34
    if (!item.is_valid(iinfo))
    {
        string name;
        item_def dummy = item;
        if (!item.quantity)
            name = "(quantity: 0) ", dummy.quantity = 1;
        name += dummy.name(DESC_PLAIN, true);
        die("Invalid item: %s", name.c_str());
    }
#endif
    ASSERT(item.is_valid(iinfo));

    marshallByte(th, item.sub_type);
    marshallShort(th, item.plus);
    marshallShort(th, item.plus2);
    marshallInt(th, item.special);
    marshallShort(th, item.quantity);

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

    item.orig_place.save(th);
    marshallShort(th, item.orig_monnum);
    marshallString(th, item.inscription);

    item.props.write(th);
}

#if TAG_MAJOR_VERSION == 34
static void _trim_god_gift_inscrip(item_def& item)
{
    item.inscription = replace_all(item.inscription, "god gift, ", "");
    item.inscription = replace_all(item.inscription, "god gift", "");
    item.inscription = replace_all(item.inscription, "Psyche", "");
    item.inscription = replace_all(item.inscription, "Sonja", "");
    item.inscription = replace_all(item.inscription, "Donald", "");
}

/// Replace "dragon armour" with "dragon scales" in an artefact's name.
static void _fixup_dragon_artefact_name(item_def &item, string name_key)
{
    if (!item.props.exists(name_key))
        return;

    string &name = item.props[name_key].get_string();
    static const string to_repl = "dragon armour";
    string::size_type found = name.find(to_repl, 0);
    if (found != string::npos)
        name.replace(found, to_repl.length(), "dragon scales");
}
#endif

void unmarshallItem(reader &th, item_def &item)
{
    item.base_type   = static_cast<object_class_type>(unmarshallByte(th));
    if (item.base_type == OBJ_UNASSIGNED)
        return;
    item.sub_type    = unmarshallUByte(th);
    item.plus        = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_RUNE_TYPE
        && item.is_type(OBJ_MISCELLANY, MISC_RUNE_OF_ZOT))
    {
        item.base_type = OBJ_RUNES;
        item.sub_type = item.plus;
        item.plus = 0;
    }
    if (th.getMinorVersion() < TAG_MINOR_ZIGFIGS
        // enum was accidentally inserted in the middle
        && item.is_type(OBJ_MISCELLANY, MISC_ZIGGURAT))
    {
        item.sub_type = MISC_PHANTOM_MIRROR;
    }
#endif
    item.plus2       = unmarshallShort(th);
    item.special     = unmarshallInt(th);
    item.quantity    = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
    // These used to come in stacks in monster inventory as throwing weapons.
    // Replace said stacks (but not single items) with tomahawks.
    if (item.quantity > 1 && item.base_type == OBJ_WEAPONS
        && (item.sub_type == WPN_CLUB || item.sub_type == WPN_HAND_AXE
            || item.sub_type == WPN_DAGGER || item.sub_type == WPN_SPEAR))
    {
        item.base_type = OBJ_MISSILES;
        item.sub_type = MI_TOMAHAWK;
        item.plus = item.plus2 = 0;
        item.brand = SPMSL_NORMAL;
    }

    // Strip vestiges of distracting gold.
    if (item.base_type == OBJ_GOLD)
        item.special = 0;

    if (th.getMinorVersion() < TAG_MINOR_REMOVE_ITEM_COLOUR)
        /* item.colour = */ unmarshallUByte(th);
#endif

    item.rnd          = unmarshallUByte(th);

    item.pos.x       = unmarshallShort(th);
    item.pos.y       = unmarshallShort(th);
    item.flags       = unmarshallInt(th);
    item.link        = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
    // ITEM_IN_SHOP was briefly NON_ITEM + NON_ITEM (1e85cf0), but that
    // doesn't fit in a short.
    if (item.link == static_cast<signed short>(54000))
        item.link = ITEM_IN_SHOP;
#endif

    unmarshallShort(th);  // igrd[item.x][item.y] -- unused

    item.slot        = unmarshallByte(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_PLACE_UNPACK)
    {
        unsigned short packed = unmarshallShort(th);
        if (packed == 0)
            item.orig_place.clear();
        else if (packed == 0xFFFF)
            item.orig_place = level_id(BRANCH_DUNGEON, 0);
        else
            item.orig_place = level_id::from_packed_place(packed);
    }
    else
#endif
    item.orig_place.load(th);

    item.orig_monnum = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_ORIG_MONNUM && item.orig_monnum > 0)
        item.orig_monnum--;
#endif
    item.inscription = unmarshallString(th);

    item.props.clear();
    item.props.read(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_CORPSE_COLOUR
        && item.base_type == OBJ_CORPSES
        && item.props.exists(FORCED_ITEM_COLOUR_KEY)
        && !item.props[FORCED_ITEM_COLOUR_KEY].get_int())
    {
        item.props[FORCED_ITEM_COLOUR_KEY] = LIGHTRED;
    }

    // If we lost the monster held in an orc corpse because we marshalled
    // it as a dead monster, clear out the prop.
    if (item.props.exists(ORC_CORPSE_KEY)
        && item.props[ORC_CORPSE_KEY].get_monster().type == MONS_NO_MONSTER)
    {
        item.props.erase(ORC_CORPSE_KEY);
    }
#endif
    // Fixup artefact props to handle reloading items when the new version
    // of Crawl has more artefact props.
    if (is_artefact(item))
        artefact_fixup_props(item);

#if TAG_MAJOR_VERSION == 34
    // Remove artefact autoinscriptions from the saved inscription.
    if ((th.getMinorVersion() < TAG_MINOR_AUTOINSCRIPTIONS
         || th.getMinorVersion() == TAG_MINOR_0_11) && is_artefact(item))
    {
        string art_ins = artefact_inscription(item);
        if (!art_ins.empty())
        {
            item.inscription = replace_all(item.inscription, art_ins + ",", "");
            item.inscription = replace_all(item.inscription, art_ins, "");

            // Avoid q - the ring "Foo" {+Fly rF+, +Lev rF+}
            art_ins = replace_all(art_ins, "+Fly", "+Lev");
            item.inscription = replace_all(item.inscription, art_ins + ",", "");
            item.inscription = replace_all(item.inscription, art_ins, "");

            trim_string(item.inscription);
        }
    }

    // Upgrade item knowledge to cope with the fix for #1083
    if (item.base_type == OBJ_JEWELLERY)
    {
        if (item.flags & ISFLAG_KNOW_PROPERTIES)
            item.flags |= ISFLAG_KNOW_TYPE;
    }

    if (item.is_type(OBJ_POTIONS, POT_WATER)
        || item.is_type(OBJ_POTIONS, POT_POISON))
    {
        item.sub_type = POT_DEGENERATION;
    }

    if (item.is_type(OBJ_POTIONS, POT_CURE_MUTATION)
        || item.is_type(OBJ_POTIONS, POT_BENEFICIAL_MUTATION))
    {
        item.sub_type = POT_MUTATION;
    }

    if (item.is_type(OBJ_STAVES, STAFF_CHANNELING))
        item.sub_type = STAFF_ENERGY;

    if (th.getMinorVersion() < TAG_MINOR_GOD_GIFT)
    {
        _trim_god_gift_inscrip(item);
        if (is_stackable_item(item))
            origin_reset(item);
    }

    if (th.getMinorVersion() < TAG_MINOR_NO_SPLINT
        && item.base_type == OBJ_ARMOUR && item.sub_type > ARM_CHAIN_MAIL)
    {
        --item.sub_type;
    }

    if (th.getMinorVersion() < TAG_MINOR_BOX_OF_BEASTS_CHARGES
        && item.is_type(OBJ_MISCELLANY, MISC_BOX_OF_BEASTS))
    {
        // Give charges to box of beasts. If the player used it
        // already then, well, they got some freebies.
        item.plus = random_range(5, 15, 2);
    }

    if (item.is_type(OBJ_MISCELLANY, MISC_BUGGY_EBONY_CASKET))
    {
        item.sub_type = MISC_BOX_OF_BEASTS;
        item.plus = 1;
    }

    // was spiked flail
    if (item.is_type(OBJ_WEAPONS, WPN_SPIKED_FLAIL)
        && th.getMinorVersion() <= TAG_MINOR_FORGOTTEN_MAP)
    {
        item.sub_type = WPN_FLAIL;
    }

    if (item.base_type == OBJ_WEAPONS
        && (item.brand == SPWPN_RETURNING
            || item.brand == SPWPN_REACHING
            || item.brand == SPWPN_ORC_SLAYING
            || item.brand == SPWPN_DRAGON_SLAYING
            || item.brand == SPWPN_EVASION))
    {
        item.brand = SPWPN_NORMAL;
    }

    // Not putting these in a minor tag since it's possible for an old
    // random monster spawn list to place flame/frost weapons.
    if (item.base_type == OBJ_WEAPONS && get_weapon_brand(item) == SPWPN_FROST)
    {
        if (is_artefact(item))
            artefact_set_property(item, ARTP_BRAND, SPWPN_FREEZING);
        else
            item.brand = SPWPN_FREEZING;
    }
    if (item.base_type == OBJ_WEAPONS && get_weapon_brand(item) == SPWPN_FLAME)
    {
        if (is_artefact(item))
            artefact_set_property(item, ARTP_BRAND, SPWPN_FLAMING);
        else
            item.brand = SPWPN_FLAMING;
    }

    // Rescale old MR (range 35-99) to new discrete steps (40/80/120)
    // Negative MR was only supposed to exist for Folly, but paranoia.
    if (th.getMinorVersion() < TAG_MINOR_MR_ITEM_RESCALE
        && is_artefact(item)
        && artefact_property(item, ARTP_MAGIC_RESISTANCE))
    {
        int prop_mr = artefact_property(item, ARTP_MAGIC_RESISTANCE);
        if (prop_mr > 99)
            artefact_set_property(item, ARTP_MAGIC_RESISTANCE, 3);
        else if (prop_mr > 79)
            artefact_set_property(item, ARTP_MAGIC_RESISTANCE, 2);
        else if (prop_mr < -40)
            artefact_set_property(item, ARTP_MAGIC_RESISTANCE, -2);
        else if (prop_mr < 0)
            artefact_set_property(item, ARTP_MAGIC_RESISTANCE, -1);
        else
            artefact_set_property(item, ARTP_MAGIC_RESISTANCE, 1);
    }

    // Rescale stealth (range 10..79 and -10..-98) to discrete steps (+-50/100)
    if (th.getMinorVersion() < TAG_MINOR_STEALTH_RESCALE && is_artefact(item))
    {
        if (artefact_property(item, ARTP_STEALTH))
        {
            int prop_st = artefact_property(item, ARTP_STEALTH);
            if (prop_st > 60)
                artefact_set_property(item, ARTP_STEALTH, 2);
            else if (prop_st < -70)
                artefact_set_property(item, ARTP_STEALTH, -2);
            else if (prop_st < 0)
                artefact_set_property(item, ARTP_STEALTH, -1);
            else
                artefact_set_property(item, ARTP_STEALTH, 1);
        }

        // Remove fast metabolism property
        if (artefact_property(item, ARTP_METABOLISM))
        {
            artefact_set_property(item, ARTP_METABOLISM, 0);
            artefact_set_property(item, ARTP_STEALTH, -1);
        }

        // Make sure no weird fake-rap combinations are produced by the upgrade
        // from rings of sustenance/hunger with {Stlth} to stealth/loudness
        if (item.base_type == OBJ_JEWELLERY
            && (item.sub_type == RING_STEALTH || item.sub_type == RING_LOUDNESS))
        {
            artefact_set_property(item, ARTP_STEALTH, 0);
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_NO_POT_FOOD)
    {
        // Replace War Chants with Battle to avoid empty-book errors.

        // Moved under TAG_MINOR_NO_POT_FOOD because it was formerly
        // not restricted to a particular range of minor tags.
        if (item.is_type(OBJ_BOOKS, BOOK_WAR_CHANTS))
            item.sub_type = BOOK_BATTLE;

        if (item.base_type == OBJ_FOOD && (item.sub_type == FOOD_UNUSED
                                           || item.sub_type == FOOD_AMBROSIA))
        {
            item.sub_type = FOOD_ROYAL_JELLY;
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_FOOD_PURGE)
    {
        if (item.base_type == OBJ_FOOD)
        {
            if (item.sub_type == FOOD_SAUSAGE)
                item.sub_type = FOOD_BEEF_JERKY;
            if (item.sub_type == FOOD_CHEESE)
                item.sub_type = FOOD_PIZZA;
            if (item.sub_type == FOOD_PEAR
                || item.sub_type == FOOD_APPLE
                || item.sub_type == FOOD_CHOKO
                || item.sub_type == FOOD_APRICOT
                || item.sub_type == FOOD_ORANGE
                || item.sub_type == FOOD_BANANA
                || item.sub_type == FOOD_STRAWBERRY
                || item.sub_type == FOOD_RAMBUTAN
                || item.sub_type == FOOD_GRAPE
                || item.sub_type == FOOD_SULTANA
                || item.sub_type == FOOD_LYCHEE
                || item.sub_type == FOOD_LEMON)
            {
                item.sub_type = FOOD_FRUIT;
            }
        }
    }
    if (th.getMinorVersion() < TAG_MINOR_FOOD_PURGE_RELOADED)
    {
        if (item.base_type == OBJ_FOOD)
        {
            if (item.sub_type == FOOD_BEEF_JERKY
                || item.sub_type == FOOD_PIZZA)
            {
                item.sub_type = FOOD_ROYAL_JELLY;
            }
        }
    }

    // Combine old rings of slaying (Acc/Dam) to new (Dam).
    // Also handle the changes to the respective ARTP_.
    if (th.getMinorVersion() < TAG_MINOR_SLAYRING_PLUSES)
    {
        int acc, dam, slay = 0;

        if (item.props.exists(ARTEFACT_PROPS_KEY))
        {
            acc = artefact_property(item, ARTP_ACCURACY);
            dam = artefact_property(item, ARTP_SLAYING);
            slay = dam < 0 ? dam : max(acc, dam);

            artefact_set_property(item, ARTP_SLAYING, slay);
        }

        if (item.is_type(OBJ_JEWELLERY, RING_SLAYING))
        {
            acc = item.plus;
            dam = item.plus2;
            slay = dam < 0 ? dam : max(acc, dam);

            item.plus = slay;
            item.plus2 = 0; // probably harmless but might as well
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_MERGE_EW)
    {
        // Combine EW1/EW2/EW3 scrolls into single enchant weapon scroll.
        if (item.base_type == OBJ_SCROLLS
            && (item.sub_type == SCR_ENCHANT_WEAPON_II
                || item.sub_type == SCR_ENCHANT_WEAPON_III))
        {
            item.sub_type = SCR_ENCHANT_WEAPON;
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_WEAPON_PLUSES)
    {
        int acc, dam, slay = 0;

        if (item.base_type == OBJ_WEAPONS)
        {
            acc = item.plus;
            dam = item.plus2;
            slay = dam < 0 ? dam : max(acc,dam);

            item.plus = slay;
            item.plus2 = 0; // probably harmless but might as well
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_CUT_CUTLASSES)
    {
        if (item.is_type(OBJ_WEAPONS, WPN_CUTLASS))
            item.sub_type = WPN_RAPIER;
    }

    if (th.getMinorVersion() < TAG_MINOR_INIT_RND)
    {
        // 0 is now reserved to indicate that rnd is uninitialized
        if (item.rnd == 0)
            item.rnd = 1 + random2(255);
    }

    if (th.getMinorVersion() < TAG_MINOR_RING_PLUSSES)
        if (item.base_type == OBJ_JEWELLERY && item.plus > 6)
            item.plus = 6;

    if (th.getMinorVersion() < TAG_MINOR_BLESSED_WPNS
        && item.base_type == OBJ_WEAPONS)
    {
        const int initial_type = item.sub_type;
        switch (item.sub_type)
        {
        case WPN_BLESSED_FALCHION:     item.sub_type = WPN_FALCHION; break;
        case WPN_BLESSED_LONG_SWORD:   item.sub_type = WPN_LONG_SWORD; break;
        case WPN_BLESSED_SCIMITAR:     item.sub_type = WPN_SCIMITAR; break;
        case WPN_BLESSED_DOUBLE_SWORD: item.sub_type = WPN_DOUBLE_SWORD; break;
        case WPN_BLESSED_GREAT_SWORD:  item.sub_type = WPN_GREAT_SWORD; break;
        case WPN_BLESSED_TRIPLE_SWORD: item.sub_type = WPN_TRIPLE_SWORD; break;
        default:                       break;
        }
        if (initial_type != item.sub_type)
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_HOLY_WRATH);
    }

    if (th.getMinorVersion() < TAG_MINOR_CONSUM_APPEARANCE)
    {
        if (item.base_type == OBJ_POTIONS)
            item.subtype_rnd = item.plus; // was consum_desc
        else if (item.base_type == OBJ_SCROLLS)
        {
            // faithfully preserve weirdness
            item.subtype_rnd = item.subtype_rnd
                               | (item.plus << 8) // was consum_desc
                               | (OBJ_SCROLLS << 16);
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_MANGLE_CORPSES)
        if (item.props.exists("never_hide"))
            item.props.erase("never_hide");

    if (th.getMinorVersion() < TAG_MINOR_ISFLAG_HANDLED
        && item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN))
    {
       // Items we've dropped or thrown have been handled already.
       item.flags |= ISFLAG_HANDLED;
    }

    if (th.getMinorVersion() < TAG_MINOR_UNSTACKABLE_EVOKERS
        && is_xp_evoker(item))
    {
        item.quantity = 1;
    }

    if (th.getMinorVersion() < TAG_MINOR_NO_NEGATIVE_VULN
        && is_artefact(item)
        && artefact_property(item, ARTP_NEGATIVE_ENERGY))
    {
        if (artefact_property(item, ARTP_NEGATIVE_ENERGY) < 0)
            artefact_set_property(item, ARTP_NEGATIVE_ENERGY, 0);
    }

    if (th.getMinorVersion() < TAG_MINOR_NO_RPOIS_MINUS
        && is_artefact(item)
        && artefact_property(item, ARTP_POISON))
    {
        if (artefact_property(item, ARTP_POISON) < 0)
            artefact_set_property(item, ARTP_POISON, 0);
    }

    if (th.getMinorVersion() < TAG_MINOR_TELEPORTITIS
        && is_artefact(item)
        && artefact_property(item, ARTP_CAUSE_TELEPORTATION) > 1)
    {
        artefact_set_property(item, ARTP_CAUSE_TELEPORTATION, 1);
    }

    if (th.getMinorVersion() < TAG_MINOR_NO_TWISTER
        && is_artefact(item)
        && artefact_property(item, ARTP_TWISTER))
    {
        artefact_set_property(item, ARTP_TWISTER, 0);
    }


    // Monsters could zap wands below zero from 0.17-a0-739-g965e8eb
    // to 0.17-a0-912-g3e33c8f. Also check for overcharged wands, in
    // case someone was patient enough to let it wrap around.
    if (item.base_type == OBJ_WANDS
        && (item.charges < 0 || item.charges > wand_max_charges(item)))
    {
        item.charges = 0;
    }

    // This works around a bug in Pakellas' supercharge wherein used_count
    // wasn't reset properly, marking the wand as empty despite being
    // fully charged. (This bug has now been fixed and was never in trunk, so
    // this code can probably be removed from trunk.)
    if (item.base_type == OBJ_WANDS && item.charges > 0
        && item.used_count == ZAPCOUNT_EMPTY)
    {
        item.used_count = 0;
    }

    if (item.base_type == OBJ_RODS && item.cursed())
        do_uncurse_item(item); // rods can't be cursed anymore

    // turn old hides into the corresponding armour
    static const map<int, armour_type> hide_to_armour = {
        { ARM_TROLL_HIDE,               ARM_TROLL_LEATHER_ARMOUR },
        { ARM_FIRE_DRAGON_HIDE,         ARM_FIRE_DRAGON_ARMOUR },
        { ARM_ICE_DRAGON_HIDE,          ARM_ICE_DRAGON_ARMOUR },
        { ARM_STEAM_DRAGON_HIDE,        ARM_STEAM_DRAGON_ARMOUR },
        { ARM_STORM_DRAGON_HIDE,        ARM_STORM_DRAGON_ARMOUR },
        { ARM_GOLD_DRAGON_HIDE,         ARM_GOLD_DRAGON_ARMOUR },
        { ARM_SWAMP_DRAGON_HIDE,        ARM_SWAMP_DRAGON_ARMOUR },
        { ARM_PEARL_DRAGON_HIDE,        ARM_PEARL_DRAGON_ARMOUR },
        { ARM_SHADOW_DRAGON_HIDE,       ARM_SHADOW_DRAGON_ARMOUR },
        { ARM_QUICKSILVER_DRAGON_HIDE,  ARM_QUICKSILVER_DRAGON_ARMOUR },
    };
    // ASSUMPTION: there was no such thing as an artefact hide
    if (item.base_type == OBJ_ARMOUR && hide_to_armour.count(item.sub_type))
        item.sub_type = *map_find(hide_to_armour, item.sub_type);

    if (th.getMinorVersion() < TAG_MINOR_HIDE_TO_SCALE && armour_is_hide(item))
    {
        _fixup_dragon_artefact_name(item, ARTEFACT_NAME_KEY);
        _fixup_dragon_artefact_name(item, ARTEFACT_APPEAR_KEY);
    }
#endif

    if (is_unrandom_artefact(item))
        setup_unrandart(item, false);

    bind_item_tile(item);
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

    if (cell.item())
        flags |= MAP_SERIALIZE_ITEM;

    if (cell.monster() != MONS_NO_MONSTER)
        flags |= MAP_SERIALIZE_MONSTER;

    marshallUnsigned(th, flags);

    switch (flags & MAP_SERIALIZE_FLAGS_MASK)
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
#if TAG_MAJOR_VERSION == 34
        marshallUnsigned(th, cell.feat());
#else
        marshallUByte(th, cell.feat());
#endif

    if (flags & MAP_SERIALIZE_FEATURE_COLOUR)
        marshallUnsigned(th, cell.feat_colour());

    if (feat_is_trap(cell.feat()))
        marshallByte(th, cell.trap());

    if (flags & MAP_SERIALIZE_CLOUD)
    {
        cloud_info* ci = cell.cloudinfo();
        marshallUnsigned(th, ci->type);
        marshallUnsigned(th, ci->colour);
        marshallUnsigned(th, ci->duration);
        marshallShort(th, ci->tile);
        marshallUByte(th, ci->killer);
    }

    if (flags & MAP_SERIALIZE_ITEM)
        marshallItem(th, *cell.item(), true);

    if (flags & MAP_SERIALIZE_MONSTER)
        marshallMonsterInfo(th, *cell.monsterinfo());
}

void unmarshallMapCell(reader &th, map_cell& cell)
{
    unsigned flags = unmarshallUnsigned(th);
    unsigned cell_flags = 0;
    trap_type trap = TRAP_UNASSIGNED;

    cell.clear();

    switch (flags & MAP_SERIALIZE_FLAGS_MASK)
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
#if TAG_MAJOR_VERSION == 34
        feature = unmarshallFeatureType_Info(th);
#else
        feature = unmarshallFeatureType(th);
#endif

    if (flags & MAP_SERIALIZE_FEATURE_COLOUR)
        feat_colour = unmarshallUnsigned(th);

    if (feat_is_trap(feature))
    {
        trap = (trap_type)unmarshallByte(th);
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() == TAG_MINOR_0_11 && trap >= TRAP_TELEPORT)
            trap = (trap_type)(trap - 1);
        if (trap == TRAP_ALARM)
            feature = DNGN_TRAP_ALARM;
        else if (trap == TRAP_ZOT)
            feature = DNGN_TRAP_ZOT;
        else if (trap == TRAP_GOLUBRIA)
            feature = DNGN_PASSAGE_OF_GOLUBRIA;
#endif
    }

    cell.set_feature(feature, feat_colour, trap);

    if (flags & MAP_SERIALIZE_CLOUD)
    {
        cloud_info ci;
        ci.type = (cloud_type)unmarshallUnsigned(th);
        unmarshallUnsigned(th, ci.colour);
        unmarshallUnsigned(th, ci.duration);
        ci.tile = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() >= TAG_MINOR_CLOUD_OWNER)
#endif
        ci.killer = static_cast<killer_type>(unmarshallUByte(th));
        cell.set_cloud(ci);
    }

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

    // set this last so the other sets don't override this
    cell.flags = cell_flags;
}

static void tag_construct_level_items(writer &th)
{
    // how many traps?
    marshallShort(th, env.trap.size());
    for (const auto& entry : env.trap)
    {
        const trap_def& trap = entry.second;
        marshallByte(th, trap.type);
        marshallCoord(th, trap.pos);
        marshallShort(th, trap.ammo_qty);
        marshallUByte(th, trap.skill_rnd);
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
    marshallInt(th, me.source);
    marshallShort(th, min(me.duration, INFINITE_DURATION));
    marshallShort(th, min(me.maxduration, INFINITE_DURATION));
}

static mon_enchant unmarshall_mon_enchant(reader &th)
{
    mon_enchant me;
    me.ench        = static_cast<enchant_type>(unmarshallShort(th));
    me.degree      = unmarshallShort(th);
    me.who         = static_cast<kill_category>(unmarshallShort(th));
    me.source      = unmarshallInt(th);
    me.duration    = unmarshallShort(th);
    me.maxduration = unmarshallShort(th);
    return me;
}

enum mon_part_t
{
    MP_GHOST_DEMON      = BIT(0),
    MP_CONSTRICTION     = BIT(1),
    MP_ITEMS            = BIT(2),
    MP_SPELLS           = BIT(3),
};

void marshallMonster(writer &th, const monster& m)
{
    if (!m.alive())
    {
        marshallShort(th, MONS_NO_MONSTER);
        return;
    }

    uint32_t parts = 0;
    if (mons_is_ghost_demon(m.type))
        parts |= MP_GHOST_DEMON;
    if (m.held || m.constricting && m.constricting->size())
        parts |= MP_CONSTRICTION;
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
        if (m.inv[i] != NON_ITEM)
            parts |= MP_ITEMS;
    if (m.spells.size() > 0)
        parts |= MP_SPELLS;

    marshallShort(th, m.type);
    marshallUnsigned(th, parts);
    ASSERT(m.mid > 0);
    marshallInt(th, m.mid);
    marshallString(th, m.mname);
    marshallByte(th, m.get_experience_level());
    marshallByte(th, m.speed);
    marshallByte(th, m.speed_increment);
    marshallByte(th, m.behaviour);
    marshallByte(th, m.pos().x);
    marshallByte(th, m.pos().y);
    marshallByte(th, m.target.x);
    marshallByte(th, m.target.y);
    marshallCoord(th, m.firing_pos);
    marshallCoord(th, m.patrol_point);
    int help = m.travel_target;
    marshallByte(th, help);

    marshallShort(th, m.travel_path.size());
    for (coord_def pos : m.travel_path)
        marshallCoord(th, pos);

    marshallUnsigned(th, m.flags.flags);
    marshallInt(th, m.experience);

    marshallShort(th, m.enchantments.size());
    for (const auto &entry : m.enchantments)
        marshall_mon_enchant(th, entry.second);
    marshallByte(th, m.ench_countdown);

    marshallShort(th, min(m.hit_points, MAX_MONSTER_HP));
    marshallShort(th, min(m.max_hit_points, MAX_MONSTER_HP));
    marshallInt(th, m.number);
    marshallShort(th, m.base_monster);
    marshallShort(th, m.colour);
    marshallInt(th, m.summoner);

    if (parts & MP_ITEMS)
        for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
            marshallShort(th, m.inv[j]);
    if (parts & MP_SPELLS)
        marshallSpells(th, m.spells);
    marshallByte(th, m.god);
    marshallByte(th, m.attitude);
    marshallShort(th, m.foe);
    marshallInt(th, m.foe_memory);
    marshallShort(th, m.damage_friendly);
    marshallShort(th, m.damage_total);
    marshallByte(th, m.went_unseen_this_turn);
    marshallCoord(th, m.unseen_pos);

    if (parts & MP_GHOST_DEMON)
    {
        // *Must* have ghost field set.
        ASSERT(m.ghost.get());
        marshallGhost(th, *m.ghost);
    }

    if (parts & MP_CONSTRICTION)
        _marshall_constriction(th, &m);

    m.props.write(th);
}

static void _marshall_mi_attack(writer &th, const mon_attack_def &attk)
{
    marshallInt(th, attk.type);
    marshallInt(th, attk.flavour);
    marshallInt(th, attk.damage);
}

static mon_attack_def _unmarshall_mi_attack(reader &th)
{
    mon_attack_def attk;
    attk.type = static_cast<attack_type>(unmarshallInt(th));
    attk.flavour = static_cast<attack_flavour>(unmarshallInt(th));
    attk.damage = unmarshallInt(th);

    return attk;
}

void marshallMonsterInfo(writer &th, const monster_info& mi)
{
    marshallFixedBitVector<NUM_MB_FLAGS>(th, mi.mb);
    marshallString(th, mi.mname);
#if TAG_MAJOR_VERSION == 34
    marshallUnsigned(th, mi.type);
    marshallUnsigned(th, mi.base_type);
#else
    marshallShort(th, mi.type);
    marshallShort(th, mi.base_type);
#endif
    marshallUnsigned(th, mi.number);
    marshallInt(th, mi._colour);
    marshallUnsigned(th, mi.attitude);
    marshallUnsigned(th, mi.threat);
    marshallUnsigned(th, mi.dam);
    marshallUnsigned(th, mi.fire_blocker);
    marshallString(th, mi.description);
    marshallString(th, mi.quote);
    marshallUnsigned(th, mi.holi.flags);
    marshallUnsigned(th, mi.mintel);
    marshallUnsigned(th, mi.hd);
    marshallUnsigned(th, mi.ac);
    marshallUnsigned(th, mi.ev);
    marshallUnsigned(th, mi.base_ev);
    marshallInt(th, mi.mresists);
    marshallUnsigned(th, mi.mitemuse);
    marshallByte(th, mi.mbase_speed);
    marshallByte(th, mi.menergy.move);
    marshallByte(th, mi.menergy.swim);
    marshallByte(th, mi.menergy.attack);
    marshallByte(th, mi.menergy.missile);
    marshallByte(th, mi.menergy.spell);
    marshallByte(th, mi.menergy.special);
    marshallByte(th, mi.menergy.item);
    marshallByte(th, mi.menergy.pickup_percent);
    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
        _marshall_mi_attack(th, mi.attack[i]);
    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (mi.inv[i].get())
        {
            marshallBoolean(th, true);
            marshallItem(th, *mi.inv[i].get(), true);
        }
        else
            marshallBoolean(th, false);
    }
    if (mons_is_pghost(mi.type))
    {
        marshallUnsigned(th, mi.i_ghost.species);
        marshallUnsigned(th, mi.i_ghost.job);
        marshallUnsigned(th, mi.i_ghost.religion);
        marshallUnsigned(th, mi.i_ghost.best_skill);
        marshallShort(th, mi.i_ghost.best_skill_rank);
        marshallShort(th, mi.i_ghost.xl_rank);
        marshallShort(th, mi.i_ghost.damage);
        marshallShort(th, mi.i_ghost.ac);
    }

    mi.props.write(th);
}

void unmarshallMonsterInfo(reader &th, monster_info& mi)
{
    unmarshallFixedBitVector<NUM_MB_FLAGS>(th, mi.mb);
    mi.mname = unmarshallString(th);
#if TAG_MAJOR_VERSION == 34
    mi.type = unmarshallMonType_Info(th);
    ASSERT(!invalid_monster_type(mi.type));
    mi.base_type = unmarshallMonType_Info(th);
    if ((mons_genus(mi.type) == MONS_DRACONIAN
        || (mons_genus(mi.type) == MONS_DEMONSPAWN
            && th.getMinorVersion() >= TAG_MINOR_DEMONSPAWN))
        && th.getMinorVersion() < TAG_MINOR_NO_DRACO_TYPE)
    {
        unmarshallMonType_Info(th); // was draco_type
    }
#else
    mi.type = unmarshallMonType(th);
    ASSERT(!invalid_monster_type(mi.type));
    mi.base_type = unmarshallMonType(th);
#endif
    unmarshallUnsigned(th, mi.number);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_MON_COLOUR_LOOKUP)
        mi._colour = int(unmarshallUnsigned(th));
    else
#endif
        mi._colour = unmarshallInt(th);
    unmarshallUnsigned(th, mi.attitude);
    unmarshallUnsigned(th, mi.threat);
    unmarshallUnsigned(th, mi.dam);
    unmarshallUnsigned(th, mi.fire_blocker);
    mi.description = unmarshallString(th);
    mi.quote = unmarshallString(th);

    uint64_t holi_flags = unmarshallUnsigned(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_MULTI_HOLI)
    {
#endif
        mi.holi.flags = holi_flags;
#if TAG_MAJOR_VERSION == 34
    }
    else
        mi.holi.flags = 1<<holi_flags;
#endif

#if TAG_MAJOR_VERSION == 34
    // XXX: special case MH_UNDEAD becoming MH_UNDEAD | MH_NATURAL
    // to save MF_FAKE_UNDEAD. Beware if you add a NATURAL bit
    // to an undead monster.
    if (mons_class_holiness(mi.type) & ~mi.holi
        && !(mi.holi & MH_UNDEAD) && !(mons_class_holiness(mi.type) & MH_NATURAL))
    {
        mi.holi |= mons_class_holiness(mi.type);
    }
#endif

    unmarshallUnsigned(th, mi.mintel);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() >= TAG_MINOR_MON_HD_INFO)
    {
#endif
        unmarshallUnsigned(th, mi.hd);
#if TAG_MAJOR_VERSION == 34
    }
    else
        mi.hd = mons_class_hit_dice(mi.type);

    if (th.getMinorVersion() >= TAG_MINOR_DISPLAY_MON_AC_EV)
    {
#endif
        unmarshallUnsigned(th, mi.ac);
        unmarshallUnsigned(th, mi.ev);
        unmarshallUnsigned(th, mi.base_ev);
#if TAG_MAJOR_VERSION == 34
    }
    else
    {
        mi.ac = get_mons_class_ac(mi.type);
        mi.ev = mi.base_ev = get_mons_class_ev(mi.type);
    }
#endif

    mi.mr = mons_class_res_magic(mi.type, mi.base_type);
    mi.can_see_invis = mons_class_sees_invis(mi.type, mi.base_type);

    mi.mresists = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
    if (mi.mresists & MR_OLD_RES_ACID)
        set_resist(mi.mresists, MR_RES_ACID, 3);
#endif
    unmarshallUnsigned(th, mi.mitemuse);
    mi.mbase_speed = unmarshallByte(th);

#if TAG_MAJOR_VERSION == 34
    // See comment in unmarshallMonster(): this could be an elemental
    // wellspring masquerading as a spectral weapon, or a polymoth
    // masquerading as a wellspring.
    if (th.getMinorVersion() < TAG_MINOR_CANARIES
        && th.getMinorVersion() >= TAG_MINOR_WAR_DOG_REMOVAL
        && mi.type >= MONS_SPECTRAL_WEAPON
        && mi.type <= MONS_POLYMOTH)
    {
        switch (mi.base_speed())
        {
        case 10:
            mi.type = MONS_ELEMENTAL_WELLSPRING;
            break;
        case 12:
            mi.type = MONS_POLYMOTH;
            break;
        case 25:
        case 30:
            mi.type = MONS_SPECTRAL_WEAPON;
            break;
        default:
            die("Unexpected monster_info with type %d and speed %d",
                mi.type, mi.base_speed());
        }
    }

    // As above; this could be one of several monsters.
    if (th.getMinorVersion() < TAG_MINOR_DEMONSPAWN
        && mi.type >= MONS_MONSTROUS_DEMONSPAWN
        && mi.type <= MONS_SALAMANDER_MYSTIC)
    {
        switch (mi.colour(true))
        {
        case BROWN:        // monstrous demonspawn, naga ritualist
            if (mi.spells[0].spell == SPELL_FORCE_LANCE)
                mi.type = MONS_NAGA_RITUALIST;
            else
                mi.type = MONS_MONSTROUS_DEMONSPAWN;
            break;
        case BLUE:         // gelid demonspawn
            mi.type = MONS_GELID_DEMONSPAWN;
            break;
        case RED:          // infernal demonspawn
            mi.type = MONS_INFERNAL_DEMONSPAWN;
            break;
        case LIGHTGRAY:    // torturous demonspawn, naga sharpshooter
            if (mi.spells[0].spell == SPELL_PORTAL_PROJECTILE)
                mi.type = MONS_NAGA_SHARPSHOOTER;
            else
                mi.type = MONS_TORTUROUS_DEMONSPAWN;
            break;
        case LIGHTBLUE:    // blood saint, shock serpent
            if (mi.base_type != MONS_NO_MONSTER)
                mi.type = MONS_BLOOD_SAINT;
            else
                mi.type = MONS_SHOCK_SERPENT;
            break;
        case LIGHTCYAN:    // warmonger, drowned soul
            if (mi.base_type != MONS_NO_MONSTER)
                mi.type = MONS_WARMONGER;
            else
                mi.type = MONS_DROWNED_SOUL;
            break;
        case LIGHTGREEN:   // corrupter
            mi.type = MONS_CORRUPTER;
            break;
        case LIGHTMAGENTA: // black sun
            mi.type = MONS_BLACK_SUN;
            break;
        case CYAN:         // worldbinder
            mi.type = MONS_WORLDBINDER;
            break;
        case MAGENTA:      // vine stalker, mana viper, grand avatar
            if (mi.base_speed() == 30)
                mi.type = MONS_GRAND_AVATAR;
            else
                mi.type = MONS_MANA_VIPER;
            break;
        case WHITE:        // salamander firebrand
            mi.type = MONS_SALAMANDER_FIREBRAND;
            break;
        case YELLOW:       // salamander mystic
            mi.type = MONS_SALAMANDER_MYSTIC;
            break;
        default:
            die("Unexpected monster with type %d and colour %d",
                mi.type, mi.colour(true));
        }
        if (mons_is_demonspawn(mi.type)
            && mons_species(mi.type) == MONS_DEMONSPAWN
            && mi.type != MONS_DEMONSPAWN)
        {
            ASSERT(mi.base_type != MONS_NO_MONSTER);
        }
    }

    if (th.getMinorVersion() < TAG_MINOR_MONINFO_ENERGY)
        mi.menergy = mons_class_energy(mi.type);
    else
    {
#endif
    mi.menergy.move = unmarshallByte(th);
    mi.menergy.swim = unmarshallByte(th);
    mi.menergy.attack = unmarshallByte(th);
    mi.menergy.missile = unmarshallByte(th);
    mi.menergy.spell = unmarshallByte(th);
    mi.menergy.special = unmarshallByte(th);
    mi.menergy.item = unmarshallByte(th);
    mi.menergy.pickup_percent = unmarshallByte(th);
#if TAG_MAJOR_VERSION == 34
    }
#endif

    // Some TAG_MAJOR_VERSION == 34 saves suffered data loss here, beware.
    // Should be harmless, hopefully.
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_BOOL_FLIGHT)
        unmarshallUnsigned(th);
#endif
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_ATTACK_DESCS)
    {
        for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
        {
            mi.attack[i] = get_monster_data(mi.type)->attack[i];
            mi.attack[i].damage = 0;
        }
    }
    else
#endif
    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
        mi.attack[i] = _unmarshall_mi_attack(th);

    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (unmarshallBoolean(th))
        {
            mi.inv[i].reset(new item_def());
            unmarshallItem(th, *mi.inv[i].get());
        }
    }

    if (mons_is_pghost(mi.type))
    {
        unmarshallUnsigned(th, mi.i_ghost.species);
        unmarshallUnsigned(th, mi.i_ghost.job);
        unmarshallUnsigned(th, mi.i_ghost.religion);
        unmarshallUnsigned(th, mi.i_ghost.best_skill);
        mi.i_ghost.best_skill_rank = unmarshallShort(th);
        mi.i_ghost.xl_rank = unmarshallShort(th);
        mi.i_ghost.damage = unmarshallShort(th);
        mi.i_ghost.ac = unmarshallShort(th);
    }
#if TAG_MAJOR_VERSION == 34
    if ((mons_is_ghost_demon(mi.type)
         || (mi.type == MONS_LICH || mi.type == MONS_ANCIENT_LICH
             || mi.type == MONS_SPELLFORGED_SERVITOR)
            && th.getMinorVersion() < TAG_MINOR_EXORCISE)
        && th.getMinorVersion() >= TAG_MINOR_GHOST_SINV
        && th.getMinorVersion() < TAG_MINOR_GHOST_NOSINV)
    {
        unmarshallBoolean(th); // was can_sinv
    }
#endif

    mi.props.clear();
    mi.props.read(th);

#if TAG_MAJOR_VERSION == 34
    if (mi.type == MONS_ZOMBIE_SMALL || mi.type == MONS_ZOMBIE_LARGE)
        mi.type = MONS_ZOMBIE;
    if (mi.type == MONS_SKELETON_SMALL || mi.type == MONS_SKELETON_LARGE)
        mi.type = MONS_SKELETON;
    if (mi.type == MONS_SIMULACRUM_SMALL || mi.type == MONS_SIMULACRUM_LARGE)
        mi.type = MONS_SIMULACRUM;
    if (th.getMinorVersion() < TAG_MINOR_WAR_DOG_REMOVAL)
    {
        if (mi.type == MONS_WAR_DOG)
            mi.type = MONS_WOLF;
    }
#endif

    if (mons_is_removed(mi.type))
    {
        mi.type = MONS_GHOST;
        mi.props.clear();
    }
}

static void tag_construct_level_monsters(writer &th)
{
    int nm = 0;
    for (int i = 0; i < MAX_MONS_ALLOC; ++i)
        if (env.mons_alloc[i] != MONS_NO_MONSTER)
            nm = i + 1;

    // how many mons_alloc?
    marshallByte(th, nm);
    for (int i = 0; i < nm; ++i)
        marshallShort(th, env.mons_alloc[i]);

    // how many monsters?
    nm = _last_used_index(menv, MAX_MONSTERS);
    marshallShort(th, nm);

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
    // Map grids.
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    marshallShort(th, env.tile_names.size());
    for (const string &name : env.tile_names)
    {
        marshallString(th, name);
#ifdef DEBUG_TILE_NAMES
        mprf("Writing '%s' into save.", name.c_str());
#endif
    }

    // flavour
    marshallShort(th, env.tile_default.wall_idx);
    marshallShort(th, env.tile_default.floor_idx);

    marshallShort(th, env.tile_default.wall);
    marshallShort(th, env.tile_default.floor);
    marshallShort(th, env.tile_default.special);

    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallShort(th, env.tile_flv[count_x][count_y].wall_idx);
            marshallShort(th, env.tile_flv[count_x][count_y].floor_idx);
            marshallShort(th, env.tile_flv[count_x][count_y].feat_idx);

            marshallShort(th, env.tile_flv[count_x][count_y].wall);
            marshallShort(th, env.tile_flv[count_x][count_y].floor);
            marshallShort(th, env.tile_flv[count_x][count_y].feat);
            marshallShort(th, env.tile_flv[count_x][count_y].special);
        }

    marshallInt(th, TILE_WALL_MAX);
}

static void tag_read_level(reader &th)
{
    env.floor_colour = unmarshallUByte(th);
    env.rock_colour  = unmarshallUByte(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_NO_LEVEL_FLAGS)
        unmarshallInt(th);
#endif

    env.elapsed_time = unmarshallInt(th);
    env.old_player_pos = unmarshallCoord(th);
    env.absdepth0 = absdungeon_depth(you.where_are_you, you.depth);

    // Map grids.
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);
    ASSERT(gx == GXM);
    ASSERT(gy == GYM);

    env.turns_on_level = unmarshallInt(th);

    EAT_CANARY;

    env.map_seen.reset();
    for (int i = 0; i < gx; i++)
        for (int j = 0; j < gy; j++)
        {
            dungeon_feature_type feat = unmarshallFeatureType(th);
            grd[i][j] = feat;
            ASSERT(feat < NUM_FEATURES);

            unmarshallMapCell(th, env.map_knowledge[i][j]);
            // Fixup positions
            if (env.map_knowledge[i][j].monsterinfo())
                env.map_knowledge[i][j].monsterinfo()->pos = coord_def(i, j);
            if (env.map_knowledge[i][j].cloudinfo())
                env.map_knowledge[i][j].cloudinfo()->pos = coord_def(i, j);

            env.map_knowledge[i][j].flags &= ~MAP_VISIBLE_FLAG;
            if (env.map_knowledge[i][j].seen())
                env.map_seen.set(i, j);
            env.pgrid[i][j] = unmarshallInt(th);

            mgrd[i][j] = NON_MONSTER;
        }

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_FORGOTTEN_MAP)
        env.map_forgotten.reset();
    else
#endif
    if (unmarshallBoolean(th))
    {
        MapKnowledge *f = new MapKnowledge();
        for (int x = 0; x < GXM; x++)
            for (int y = 0; y < GYM; y++)
                unmarshallMapCell(th, (*f)[x][y]);
        env.map_forgotten.reset(f);
    }
    else
        env.map_forgotten.reset();

    env.grid_colours.init(BLACK);
    _run_length_decode(th, unmarshallByte, env.grid_colours, GXM, GYM);

    EAT_CANARY;

    env.cloud.clear();
    // how many clouds?
    const int num_clouds = unmarshallShort(th);
    cloud_struct cloud;
    for (int i = 0; i < num_clouds; i++)
    {
        cloud.type  = static_cast<cloud_type>(unmarshallByte(th));
#if TAG_MAJOR_VERSION == 34
        // old system marshalled empty clouds this way
        if (cloud.type == CLOUD_NONE)
            continue;
#else
        ASSERT(cloud.type != CLOUD_NONE);
#endif
        cloud.pos.x = unmarshallByte(th);
        cloud.pos.y = unmarshallByte(th);
        ASSERT_IN_BOUNDS(cloud.pos);
        cloud.decay = unmarshallShort(th);
        cloud.spread_rate = unmarshallUByte(th);
        cloud.whose = static_cast<kill_category>(unmarshallUByte(th));
        cloud.killer = static_cast<killer_type>(unmarshallUByte(th));
        cloud.source = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() < TAG_MINOR_DECUSTOM_CLOUDS)
        {
            unmarshallShort(th); // was cloud.colour
            unmarshallString(th); // was cloud.name
            unmarshallString(th); // was cloud.tile
        }
#endif
        cloud.excl_rad = unmarshallInt(th);

#if TAG_MAJOR_VERSION == 34
        // Remove clouds stuck in walls, from 0.18-a0-603-g332275c to
        // 0.18-a0-629-g16988c9.
        if (!cell_is_solid(cloud.pos))
#endif
            env.cloud[cloud.pos] = cloud;
    }

    EAT_CANARY;

    // how many shops?
    const int num_shops = unmarshallShort(th);
    shop_struct shop;
    for (int i = 0; i < num_shops; i++)
    {
        unmarshall_shop(th, shop);
        if (shop.type == SHOP_UNASSIGNED)
            continue;
#if TAG_MAJOR_VERSION == 34
        shop.num = i;
#endif
        env.shop[shop.pos] = shop;
    }

    EAT_CANARY;

    env.sanctuary_pos  = unmarshallCoord(th);
    env.sanctuary_time = unmarshallByte(th);

    env.spawn_random_rate = unmarshallInt(th);

    env.markers.read(th);

    env.properties.clear();
    env.properties.read(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_PLACE_UNPACK)
    {
        CrawlHashTable &props = env.properties;
        if (props.exists(VAULT_MON_BASES_KEY))
        {
            ASSERT(!props.exists(VAULT_MON_PLACES_KEY));
            CrawlVector &type_vec = props[VAULT_MON_TYPES_KEY].get_vector();
            CrawlVector &base_vec = props[VAULT_MON_BASES_KEY].get_vector();
            size_t size = type_vec.size();
            props[VAULT_MON_PLACES_KEY].new_vector(SV_LEV_ID).resize(size);
            CrawlVector &place_vec = props[VAULT_MON_PLACES_KEY].get_vector();
            for (size_t i = 0; i < size; i++)
            {
                if (type_vec[i].get_int() == -1)
                   place_vec[i] = level_id::from_packed_place(base_vec[i].get_int());
                else
                   place_vec[i] = level_id();
            }
        }
    }
#endif

    env.dactions_done = unmarshallInt(th);

    // Restore heightmap
    env.heightmap.reset(nullptr);
    const bool have_heightmap = unmarshallBoolean(th);
    if (have_heightmap)
    {
        env.heightmap.reset(new grid_heightmap);
        grid_heightmap &heightmap(*env.heightmap);
        for (rectangle_iterator ri(0); ri; ++ri)
            heightmap(*ri) = unmarshallShort(th);
    }

    EAT_CANARY;

    env.forest_awoken_until = unmarshallInt(th);
    unmarshall_level_vault_data(th);
    env.density = unmarshallInt(th);

    int num_lights = unmarshallShort(th);
    ASSERT(num_lights >= 0);
    env.sunlight.clear();
    while (num_lights-- > 0)
    {
        coord_def c = unmarshallCoord(th);
        env.sunlight.emplace_back(c, unmarshallInt(th));
    }
}

#if TAG_MAJOR_VERSION == 34
static spell_type _fixup_soh_breath(monster_type mtyp)
{
    switch (mtyp)
    {
        case MONS_SERPENT_OF_HELL:
        default:
            return SPELL_SERPENT_OF_HELL_GEH_BREATH;
        case MONS_SERPENT_OF_HELL_COCYTUS:
            return SPELL_SERPENT_OF_HELL_COC_BREATH;
        case MONS_SERPENT_OF_HELL_DIS:
            return SPELL_SERPENT_OF_HELL_DIS_BREATH;
        case MONS_SERPENT_OF_HELL_TARTARUS:
            return SPELL_SERPENT_OF_HELL_TAR_BREATH;
    }
}
#endif

static void tag_read_level_items(reader &th)
{
    env.trap.clear();
    // how many traps?
    const int trap_count = unmarshallShort(th);
    trap_def trap;
    for (int i = 0; i < trap_count; ++i)
    {
        trap.type = static_cast<trap_type>(unmarshallUByte(th));
#if TAG_MAJOR_VERSION == 34
        if (trap.type == TRAP_UNASSIGNED)
            continue;
#else
        ASSERT(trap.type != TRAP_UNASSIGNED);
#endif
        trap.pos      = unmarshallCoord(th);
        trap.ammo_qty = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() == TAG_MINOR_0_11 && trap.type >= TRAP_TELEPORT)
            trap.type = (trap_type)(trap.type - 1);
        if (th.getMinorVersion() < TAG_MINOR_TRAPS_DETERM
            || th.getMinorVersion() == TAG_MINOR_0_11)
        {
            trap.skill_rnd = random2(256);
        }
        else
#endif
        trap.skill_rnd = unmarshallUByte(th);
        env.trap[trap.pos] = trap;
    }

#if TAG_MAJOR_VERSION == 34
    // Fix up floor that trap_def::destroy left as a trap (from
    // 0.18-a0-605-g5e852a4 to 0.18-a0-614-gc92b81f).
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
        {
            coord_def pos(i, j);
            if (feat_is_trap(grd(pos), true) && !map_find(env.trap, pos))
                grd(pos) = DNGN_FLOOR;
        }
#endif

    // how many items?
    const int item_count = unmarshallShort(th);
    ASSERT_RANGE(item_count, 0, MAX_ITEMS + 1);
    for (int i = 0; i < item_count; ++i)
        unmarshallItem(th, mitm[i]);
    for (int i = item_count; i < MAX_ITEMS; ++i)
        mitm[i].clear();

#ifdef DEBUG_ITEM_SCAN
    // There's no way to fix this, even with wizard commands, so get
    // rid of it when restoring the game.
    for (auto &item : mitm)
        if (item.pos.origin())
            item.clear();
#endif
}

void unmarshallMonster(reader &th, monster& m)
{
    m.reset();

    m.type           = unmarshallMonType(th);
    if (m.type == MONS_NO_MONSTER)
        return;

    ASSERT(!invalid_monster_type(m.type));

#if TAG_MAJOR_VERSION == 34
    uint32_t parts    = 0;
    if (th.getMinorVersion() < TAG_MINOR_MONSTER_PARTS)
    {
        if (mons_is_ghost_demon(m.type))
            parts |= MP_GHOST_DEMON;
    }
    else
        parts         = unmarshallUnsigned(th);
    if (th.getMinorVersion() < TAG_MINOR_OPTIONAL_PARTS)
        parts |= MP_CONSTRICTION | MP_ITEMS | MP_SPELLS;
#else
    uint32_t parts    = unmarshallUnsigned(th);
#endif
    m.mid             = unmarshallInt(th);
    ASSERT(m.mid > 0);
    m.mname           = unmarshallString(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_REMOVE_MON_AC_EV)
    {
        unmarshallByte(th);
        unmarshallByte(th);
    }
#endif
    m.set_hit_dice(     unmarshallByte(th));
#if TAG_MAJOR_VERSION == 34
    // Draining used to be able to take a monster to 0 HD, but that
    // caused crashes if they tried to cast spells.
    m.set_hit_dice(max(m.get_experience_level(), 1));
#else
    ASSERT(m.get_experience_level() > 0);
#endif
    m.speed           = unmarshallByte(th);
    // Avoid sign extension when loading files (Elethiomel's hang)
    m.speed_increment = unmarshallUByte(th);
    m.behaviour       = static_cast<beh_type>(unmarshallUByte(th));
    int x             = unmarshallByte(th);
    int y             = unmarshallByte(th);
    m.set_position(coord_def(x,y));
    m.target.x        = unmarshallByte(th);
    m.target.y        = unmarshallByte(th);

    m.firing_pos      = unmarshallCoord(th);
    m.patrol_point    = unmarshallCoord(th);

    int help = unmarshallByte(th);
    m.travel_target = static_cast<montravel_target_type>(help);

    const int len = unmarshallShort(th);
    for (int i = 0; i < len; ++i)
        m.travel_path.push_back(unmarshallCoord(th));

    m.flags.flags = unmarshallUnsigned(th);
    m.experience  = unmarshallInt(th);

    m.enchantments.clear();
    const int nenchs = unmarshallShort(th);
    for (int i = 0; i < nenchs; ++i)
    {
        mon_enchant me = unmarshall_mon_enchant(th);
        m.enchantments[me.ench] = me;
        m.ench_cache.set(me.ench, true);
    }
    m.ench_countdown = unmarshallByte(th);

    m.hit_points     = unmarshallShort(th);
    m.max_hit_points = unmarshallShort(th);
    m.number         = unmarshallInt(th);
    m.base_monster   = unmarshallMonType(th);
    m.colour         = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_SUMMONER)
        m.summoner = 0;
    else
#endif
    m.summoner       = unmarshallInt(th);

    if (parts & MP_ITEMS)
        for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
            m.inv[j] = unmarshallShort(th);

    if (parts & MP_SPELLS)
    {
        unmarshallSpells(th, m.spells
#if TAG_MAJOR_VERSION == 34
                         , m.get_experience_level()
#endif
                         );
#if TAG_MAJOR_VERSION == 34
    monster_spells oldspells = m.spells;
    m.spells.clear();
    for (mon_spell_slot &slot : oldspells)
    {
        if (mons_is_zombified(m) && !mons_enslaved_soul(m)
            && slot.spell != SPELL_CREATE_TENTACLES)
        {
            // zombies shouldn't have (most) spells
        }
        else if (slot.spell == SPELL_DRACONIAN_BREATH)
        {
            // Replace Draconian Breath with the colour-specific spell,
            // and remove Azrael's bad breath while we're at it.
            if (mons_genus(m.type) == MONS_DRACONIAN)
                m.spells.push_back(drac_breath(draco_or_demonspawn_subspecies(m)));
        }
        // Give Mnoleg back malign gateway in place of tentacles.
        else if (slot.spell == SPELL_CREATE_TENTACLES
                 && m.type == MONS_MNOLEG)
        {
            slot.spell = SPELL_MALIGN_GATEWAY;
            slot.freq = 27;
            m.spells.push_back(slot);
        }
        else if (slot.spell == SPELL_CHANT_FIRE_STORM)
        {
            slot.spell = SPELL_FIRE_STORM;
            m.spells.push_back(slot);
        }
        else if (slot.spell == SPELL_SERPENT_OF_HELL_BREATH_REMOVED)
        {
            slot.spell = _fixup_soh_breath(m.type);
            m.spells.push_back(slot);
        }
#if TAG_MAJOR_VERSION == 34
        else if (slot.spell != SPELL_DELAYED_FIREBALL
                 && slot.spell != SPELL_MELEE)
        {
            m.spells.push_back(slot);
        }
#endif
        else if (slot.spell == SPELL_CORRUPT_BODY)
        {
            slot.spell = SPELL_CORRUPTING_PULSE;
            m.spells.push_back(slot);
        }
    }
#endif
    }

    m.god      = static_cast<god_type>(unmarshallByte(th));
    m.attitude = static_cast<mon_attitude_type>(unmarshallByte(th));
    m.foe      = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
    // In 0.16 alpha we briefly allowed YOU_FAULTLESS as a monster's foe.
    if (m.foe == YOU_FAULTLESS)
        m.foe = MHITYOU;
#endif
    m.foe_memory = unmarshallInt(th);

    m.damage_friendly = unmarshallShort(th);
    m.damage_total = unmarshallShort(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_UNSEEN_MONSTER)
    {
        m.went_unseen_this_turn = false;
        m.unseen_pos = coord_def(0, 0);
    }
    else
    {
#endif
    m.went_unseen_this_turn = unmarshallByte(th);
    m.unseen_pos = unmarshallCoord(th);
#if TAG_MAJOR_VERSION == 34
    }
#endif

#if TAG_MAJOR_VERSION == 34
    if (m.type == MONS_LABORATORY_RAT)
        unmarshallGhost(th), m.type = MONS_RAT;

    // MONS_SPECTRAL_WEAPON was inserted into the wrong place
    // (0.13-a0-1964-g2fab1c1, merged into trunk in 0.13-a0-1981-g9e80fb2),
    // and then had a ghost_demon structure added (0.13-a0-2055-g6cfaa00).
    // Neither event had an associated tag, but both were between the
    // same two adjacent tags.
    if (th.getMinorVersion() < TAG_MINOR_CANARIES
        && th.getMinorVersion() >= TAG_MINOR_WAR_DOG_REMOVAL
        && m.type >= MONS_SPECTRAL_WEAPON
        && m.type <= MONS_POLYMOTH)
    {
        // But fortunately the three monsters it could be all have different
        // speeds, and none of those speeds are 3/2 or 2/3 any others. We will
        // assume that none of these had the wretched enchantment. Ugh.
        switch (m.speed)
        {
        case 6: case 7: // slowed
        case 10:
        case 15: // hasted/berserked
            m.type = MONS_GHOST; // wellspring
            break;
        case 8: // slowed
        case 12:
        case 18: // hasted/berserked
            m.type = MONS_POLYMOTH;
            break;
        case 16: case 17: case 20: // slowed
        case 25:
        case 30:
        case 37: case 38: case 45: // hasted/berserked
            m.type = MONS_SPECTRAL_WEAPON;
            break;
        default:
            die("Unexpected monster with type %d and speed %d",
                m.type, m.speed);
        }
    }

    // Spectral weapons became speed 30 in the commit immediately preceding
    // the one that added the ghost_demon. Since the commits were in the
    // same batch, no one should have saves where the speed is 30 and the
    // spectral weapon didn't have a ghost_demon, or where the speed is
    // 25 and it did.
    if (th.getMinorVersion() < TAG_MINOR_CANARIES
        && m.type == MONS_SPECTRAL_WEAPON
        // normal, slowed, and hasted, respectively.
        && m.speed != 30 && m.speed != 20 && m.speed != 45)
    {
        // Don't bother trying to fix it up.
        m.type = MONS_WOOD_GOLEM; // anything removed
        m.mid = ++you.last_mid;   // sabotage the bond
        ASSERT(m.mid < MID_FIRST_NON_MONSTER);
        parts &= MP_GHOST_DEMON;
    }
    else if (m.type == MONS_CHIMERA
             && th.getMinorVersion() < TAG_MINOR_CHIMERA_GHOST_DEMON)
    {
        // Don't unmarshall the ghost demon if this is an invalid chimera
    }
    else if (th.getMinorVersion() < TAG_MINOR_DEMONSPAWN
             && m.type >= MONS_MONSTROUS_DEMONSPAWN
             && m.type <= MONS_SALAMANDER_MYSTIC)
    {
        // The demonspawn-enemies branch was merged in such a fashion
        // that it bumped several monster enums (see merge commit:
        // 0.14-a0-2321-gdab6825).
        // Try to figure out what it is.
        switch (m.colour)
        {
        case BROWN:        // monstrous demonspawn, naga ritualist
            if (m.spells[0].spell == SPELL_FORCE_LANCE)
                m.type = MONS_NAGA_RITUALIST;
            else
                m.type = MONS_MONSTROUS_DEMONSPAWN;
            break;
        case BLUE:         // gelid demonspawn
            m.type = MONS_GELID_DEMONSPAWN;
            break;
        case RED:          // infernal demonspawn
            m.type = MONS_INFERNAL_DEMONSPAWN;
            break;
        case LIGHTGRAY:    // torturous demonspawn, naga sharpshooter
            if (m.spells[0].spell == SPELL_PORTAL_PROJECTILE)
                m.type = MONS_NAGA_SHARPSHOOTER;
            else
                m.type = MONS_TORTUROUS_DEMONSPAWN;
            break;
        case LIGHTBLUE:    // blood saint, shock serpent
            if (m.base_monster != MONS_NO_MONSTER)
                m.type = MONS_BLOOD_SAINT;
            else
                m.type = MONS_SHOCK_SERPENT;
            break;
        case LIGHTCYAN:    // warmonger, drowned soul
            if (m.base_monster != MONS_NO_MONSTER)
                m.type = MONS_WARMONGER;
            else
                m.type = MONS_DROWNED_SOUL;
            break;
        case LIGHTGREEN:   // corrupter
            m.type = MONS_CORRUPTER;
            break;
        case LIGHTMAGENTA: // black sun
            m.type = MONS_BLACK_SUN;
            break;
        case CYAN:         // worldbinder
            m.type = MONS_WORLDBINDER;
            break;
        case MAGENTA:      // vine stalker, mana viper, grand avatar
            switch (m.speed)
            {
                case 20:
                case 30:
                case 45:
                    m.type = MONS_GRAND_AVATAR;
                    break;
                case 9:
                case 10:
                case 14:
                case 21:
                    m.type = MONS_MANA_VIPER;
                    break;
                default:
                    die("Unexpected monster with type %d and speed %d",
                        m.type, m.speed);
            }
            break;
        case WHITE:        // salamander firebrand
            m.type = MONS_SALAMANDER_FIREBRAND;
            break;
        case YELLOW:       // salamander mystic
            m.type = MONS_SALAMANDER_MYSTIC;
            break;
        default:
            die("Unexpected monster with type %d and colour %d",
                m.type, m.colour);
        }
        if (mons_is_demonspawn(m.type)
            && mons_species(m.type) == MONS_DEMONSPAWN
            && m.type != MONS_DEMONSPAWN)
        {
            ASSERT(m.base_monster != MONS_NO_MONSTER);
        }
    }
    else if (th.getMinorVersion() < TAG_MINOR_EXORCISE
        && th.getMinorVersion() >= TAG_MINOR_RANDLICHES
        && (m.type == MONS_LICH || m.type == MONS_ANCIENT_LICH
            || m.type == MONS_SPELLFORGED_SERVITOR))
    {
        m.spells = unmarshallGhost(th).spells;
    }
    else
#endif
    if (parts & MP_GHOST_DEMON)
        m.set_ghost(unmarshallGhost(th));

#if TAG_MAJOR_VERSION == 34
    // Turn elephant slugs into ghosts because they are dummies now.
    if (m.type == MONS_ELEPHANT_SLUG)
        m.type = MONS_GHOST;
#endif

    if (parts & MP_CONSTRICTION)
        _unmarshall_constriction(th, &m);

    m.props.clear();
    m.props.read(th);

    if (m.props.exists("monster_tile_name"))
    {
        string tile = m.props["monster_tile_name"].get_string();
        tileidx_t index;
        if (!tile_player_index(tile.c_str(), &index))
        {
            // If invalid tile name, complain and discard the props.
            dprf("bad tile name: \"%s\".", tile.c_str());
            m.props.erase("monster_tile_name");
            if (m.props.exists("monster_tile"))
                m.props.erase("monster_tile");
        }
        else // Update monster tile.
            m.props["monster_tile"] = short(index);
    }

#if TAG_MAJOR_VERSION == 34
    // Forget seen spells if the monster doesn't have any, most likely because
    // of a polymorph that happened before polymorph began removing this key.
    if (m.spells.empty())
        m.props.erase(SEEN_SPELLS_KEY);

    // Battlespheres that don't know their creator's mid must have belonged
    // to the player pre-monster-battlesphere.
    if (th.getMinorVersion() < TAG_MINOR_BATTLESPHERE_MID
        && m.type == MONS_BATTLESPHERE && !m.props.exists("bs_mid"))
    {
        // It must have belonged to the player.
        m.summoner = MID_PLAYER;
    }
    else if (m.props.exists("bs_mid"))
    {
        m.summoner = m.props["bs_mid"].get_int();
        m.props.erase("bs_mid");
    }

    if (m.props.exists(IOOD_MID))
        m.summoner = m.props[IOOD_MID].get_int(), m.props.erase(IOOD_MID);

    if (m.props.exists("siren_call"))
    {
        m.props["merfolk_avatar_call"] = m.props["siren_call"].get_bool();
        m.props.erase("siren_call");
    }

    if (m.type == MONS_ZOMBIE_SMALL || m.type == MONS_ZOMBIE_LARGE)
        m.type = MONS_ZOMBIE;
    if (m.type == MONS_SKELETON_SMALL || m.type == MONS_SKELETON_LARGE)
        m.type = MONS_SKELETON;
    if (m.type == MONS_SIMULACRUM_SMALL || m.type == MONS_SIMULACRUM_LARGE)
        m.type = MONS_SIMULACRUM;

    if (th.getMinorVersion() < TAG_MINOR_WAR_DOG_REMOVAL)
    {
        if (m.type == MONS_WAR_DOG)
            m.type = MONS_WOLF;
    }

    if (m.props.exists("no_hide"))
        m.props.erase("no_hide");

    if (m.props.exists("original_name"))
    {
        m.props[ORIGINAL_TYPE_KEY].get_int() =
            get_monster_by_name(m.props["original_name"].get_string());
    }

    if (m.props.exists("given beogh weapon"))
    {
        m.props.erase("given beogh weapon");
        m.props[BEOGH_MELEE_WPN_GIFT_KEY] = true;
    }
#endif

    if (m.type != MONS_PROGRAM_BUG && mons_species(m.type) == MONS_PROGRAM_BUG)
    {
        m.type = MONS_GHOST;
        m.props.clear();
    }

    // If an upgrade synthesizes ghost_demon, please mark it in "parts" above.
    ASSERT(parts & MP_GHOST_DEMON || !mons_is_ghost_demon(m.type));

    m.check_speed();
}

static void tag_read_level_monsters(reader &th)
{
    int count;

    reset_all_monsters();

    // how many mons_alloc?
    count = unmarshallByte(th);
    ASSERT(count >= 0);
    for (int i = 0; i < count && i < MAX_MONS_ALLOC; ++i)
        env.mons_alloc[i] = unmarshallMonType(th);
    for (int i = MAX_MONS_ALLOC; i < count; ++i)
        unmarshallShort(th);
    for (int i = count; i < MAX_MONS_ALLOC; ++i)
        env.mons_alloc[i] = MONS_NO_MONSTER;

    // how many monsters?
    count = unmarshallShort(th);
    ASSERT_RANGE(count, 0, MAX_MONSTERS + 1);

    for (int i = 0; i < count; i++)
    {
        monster& m = menv[i];
        unmarshallMonster(th, m);

        // place monster
        if (!m.alive())
            continue;

        // companion_is_elsewhere checks the mid cache
        env.mid_cache[m.mid] = i;
        if (m.is_divine_companion() && companion_is_elsewhere(m.mid))
        {
            dprf("Killed elsewhere companion %s(%d) on %s",
                    m.name(DESC_PLAIN, true).c_str(), m.mid,
                    level_id::current().describe(false, true).c_str());
            monster_die(&m, KILL_RESET, -1, true, false);
            continue;
        }

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
        {
            mprf(MSGCH_ERROR, "(%d, %d) for %s already occupied by %s",
                 m.pos().x, m.pos().y,
                 m.name(DESC_PLAIN, true).c_str(),
                 menv[midx].name(DESC_PLAIN, true).c_str());
        }
#endif
        mgrd(m.pos()) = i;
    }
#if TAG_MAJOR_VERSION == 34
    // This relies on TAG_YOU (including lost monsters) being unmarshalled
    // on game load before the initial level.
    if (th.getMinorVersion() < TAG_MINOR_FIXED_CONSTRICTION
        && th.getMinorVersion() >= TAG_MINOR_OPTIONAL_PARTS)
    {
        _fix_missing_constrictions();
    }
    if (th.getMinorVersion() < TAG_MINOR_TENTACLE_MID)
    {
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->props.exists("inwards"))
            {
                const int old_midx = mi->props["inwards"].get_int();
                if (invalid_monster_index(old_midx))
                    mi->props["inwards"].get_int() = MID_NOBODY;
                else
                    mi->props["inwards"].get_int() = menv[old_midx].mid;
            }
            if (mi->props.exists("outwards"))
            {
                const int old_midx = mi->props["outwards"].get_int();
                if (invalid_monster_index(old_midx))
                    mi->props["outwards"].get_int() = MID_NOBODY;
                else
                    mi->props["outwards"].get_int() = menv[old_midx].mid;
            }
            if (mons_is_tentacle_or_tentacle_segment(mi->type))
                mi->tentacle_connect = menv[mi->tentacle_connect].mid;
        }
    }
#endif
}

static void _debug_count_tiles()
{
#ifdef DEBUG_DIAGNOSTICS
# ifdef USE_TILE
    map<int,bool> found;
    int t, cnt = 0;
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
        {
            t = env.tile_bk_bg[i][j];
            if (!found.count(t))
                cnt++, found[t] = true;
            t = env.tile_bk_fg[i][j];
            if (!found.count(t))
                cnt++, found[t] = true;
            t = env.tile_bk_cloud[i][j];
            if (!found.count(t))
                cnt++, found[t] = true;
        }
    dprf("Unique tiles found: %d", cnt);
# endif
#endif
}

void tag_read_level_tiles(reader &th)
{
    // Map grids.
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);

    env.tile_names.clear();
    unsigned int num_tilenames = unmarshallShort(th);
    for (unsigned int i = 0; i < num_tilenames; ++i)
    {
#ifdef DEBUG_TILE_NAMES
        string temp = unmarshallString(th);
        mprf("Reading tile_names[%d] = %s", i, temp.c_str());
        env.tile_names.push_back(temp);
#else
        env.tile_names.push_back(unmarshallString(th));
#endif
    }

    // flavour
    env.tile_default.wall_idx  = unmarshallShort(th);
    env.tile_default.floor_idx = unmarshallShort(th);
    env.tile_default.wall      = unmarshallShort(th);
    env.tile_default.floor     = unmarshallShort(th);
    env.tile_default.special   = unmarshallShort(th);

    for (int x = 0; x < gx; x++)
        for (int y = 0; y < gy; y++)
        {
            env.tile_flv[x][y].wall_idx  = unmarshallShort(th);
            env.tile_flv[x][y].floor_idx = unmarshallShort(th);
            env.tile_flv[x][y].feat_idx  = unmarshallShort(th);

            // These get overwritten by _regenerate_tile_flavour
            env.tile_flv[x][y].wall    = unmarshallShort(th);
            env.tile_flv[x][y].floor   = unmarshallShort(th);
            env.tile_flv[x][y].feat    = unmarshallShort(th);
            env.tile_flv[x][y].special = unmarshallShort(th);
        }

    _debug_count_tiles();

    _regenerate_tile_flavour();

    // Draw remembered map
    _draw_tiles();
}

static tileidx_t _get_tile_from_vector(const unsigned int idx)
{
    if (idx <= 0 || idx > env.tile_names.size())
    {
#ifdef DEBUG_TILE_NAMES
        mprf("Index out of bounds: idx = %d - 1, size(tile_names) = %d",
            idx, env.tile_names.size());
#endif
        return 0;
    }
    string tilename = env.tile_names[idx - 1];

    tileidx_t tile;
    if (!tile_dngn_index(tilename.c_str(), &tile))
    {
#ifdef DEBUG_TILE_NAMES
        mprf("tilename %s (index %d) not found",
             tilename.c_str(), idx - 1);
#endif
        return 0;
    }
#ifdef DEBUG_TILE_NAMES
    mprf("tilename %s (index %d) resolves to tile %d",
         tilename.c_str(), idx - 1, (int) tile);
#endif

    return tile;
}

static void _regenerate_tile_flavour()
{
    /* Remember the wall_idx and floor_idx; tile_init_default_flavour
       sets them to 0 */
    tileidx_t default_wall_idx = env.tile_default.wall_idx;
    tileidx_t default_floor_idx = env.tile_default.floor_idx;
    tile_init_default_flavour();
    if (default_wall_idx)
    {
        tileidx_t new_wall = _get_tile_from_vector(default_wall_idx);
        if (new_wall)
        {
            env.tile_default.wall_idx = default_wall_idx;
            env.tile_default.wall = new_wall;
        }
    }
    if (default_floor_idx)
    {
        tileidx_t new_floor = _get_tile_from_vector(default_floor_idx);
        if (new_floor)
        {
            env.tile_default.floor_idx = default_floor_idx;
            env.tile_default.floor = new_floor;
        }
    }

    for (rectangle_iterator ri(coord_def(0, 0), coord_def(GXM-1, GYM-1));
         ri; ++ri)
    {
        tile_flavour &flv = env.tile_flv(*ri);
        flv.wall = 0;
        flv.floor = 0;
        flv.feat = 0;
        flv.special = 0;

        if (flv.wall_idx)
        {
            tileidx_t new_wall = _get_tile_from_vector(flv.wall_idx);
            if (!new_wall)
                flv.wall_idx = 0;
            else
                flv.wall = new_wall;
        }
        if (flv.floor_idx)
        {
            tileidx_t new_floor = _get_tile_from_vector(flv.floor_idx);
            if (!new_floor)
                flv.floor_idx = 0;
            else
                flv.floor = new_floor;
        }
        if (flv.feat_idx)
        {
            tileidx_t new_feat = _get_tile_from_vector(flv.feat_idx);
            if (!new_feat)
                flv.feat_idx = 0;
            else
                flv.feat = new_feat;
        }
    }

    tile_new_level(true, false);
}

static void _draw_tiles()
{
#ifdef USE_TILE
    for (rectangle_iterator ri(coord_def(0, 0), coord_def(GXM-1, GYM-1));
         ri; ++ri)
    {
        tile_draw_map_cell(*ri);
    }
#endif
}
// ------------------------------- ghost tags ---------------------------- //

static void marshallSpells(writer &th, const monster_spells &spells)
{
    const uint8_t spellsize = spells.size();
    marshallByte(th, spellsize);
    for (int j = 0; j < spellsize; ++j)
    {
        marshallShort(th, spells[j].spell);
        marshallByte(th, spells[j].freq);
        marshallShort(th, spells[j].flags.flags);
    }
}

#if TAG_MAJOR_VERSION == 34
static const int NUM_MONSTER_SPELL_SLOTS = 6;

static void _fixup_spells(monster_spells &spells, int hd)
{
    unsigned count = 0;
    for (size_t i = 0; i < spells.size(); i++)
    {
        if (spells[i].spell == SPELL_NO_SPELL)
            continue;

        count++;

        spells[i].flags |= MON_SPELL_WIZARD;

        if (i == NUM_MONSTER_SPELL_SLOTS - 1)
            spells[i].flags |= MON_SPELL_EMERGENCY;
    }

    if (!count)
    {
        spells.clear();
        return;
    }

    erase_if(spells, [](const mon_spell_slot &t) {
        return t.spell == SPELL_NO_SPELL;
    });

    if (!spells.size())
        return;

    for (auto& slot : spells)
        slot.freq = (hd + 50) / spells.size();

    normalize_spell_freq(spells, hd);
}
#endif

static void unmarshallSpells(reader &th, monster_spells &spells
#if TAG_MAJOR_VERSION == 34
                             , unsigned hd
#endif
                            )
{
    const uint8_t spellsize =
#if TAG_MAJOR_VERSION == 34

        (th.getMinorVersion() < TAG_MINOR_ARB_SPELL_SLOTS)
            ? NUM_MONSTER_SPELL_SLOTS :
#endif
        unmarshallByte(th);
    spells.clear();
    spells.resize(spellsize);
    for (int j = 0; j < spellsize; ++j)
    {
        spells[j].spell = unmarshallSpellType(th

#if TAG_MAJOR_VERSION == 34
            , true
#endif
            );
#if TAG_MAJOR_VERSION == 34
        if (th.getMinorVersion() < TAG_MINOR_MALMUTATE
            && spells[j].spell == SPELL_POLYMORPH)
        {
            spells[j].spell = SPELL_MALMUTATE;
        }

        if (spells[j].spell == SPELL_FAKE_RAKSHASA_SUMMON)
            spells[j].spell = SPELL_PHANTOM_MIRROR;

        if (spells[j].spell == SPELL_SUNRAY)
            spells[j].spell = SPELL_STONE_ARROW;

        if (th.getMinorVersion() >= TAG_MINOR_MONSTER_SPELL_SLOTS)
        {
#endif
        spells[j].freq = unmarshallByte(th);
        spells[j].flags.flags = unmarshallShort(th);
#if TAG_MAJOR_VERSION == 34
            if (th.getMinorVersion() < TAG_MINOR_DEMONIC_SPELLS)
            {
                if (spells[j].flags & MON_SPELL_DEMONIC)
                {
                    spells[j].flags &= ~MON_SPELL_DEMONIC;
                    spells[j].flags |= MON_SPELL_MAGICAL;
                }
            }
        }
#endif
    }

#if TAG_MAJOR_VERSION == 34
    // This will turn all old spells into wizard spells, which
    // isn't right but is the simplest way to do this.
    if (th.getMinorVersion() < TAG_MINOR_MONSTER_SPELL_SLOTS)
        _fixup_spells(spells, hd);
#endif
}

static void marshallGhost(writer &th, const ghost_demon &ghost)
{
    marshallString(th, ghost.name);

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
    marshallShort(th, ghost.move_energy);
    marshallByte(th, ghost.see_invis);
    marshallShort(th, ghost.brand);
    marshallShort(th, ghost.att_type);
    marshallShort(th, ghost.att_flav);
    marshallInt(th, ghost.resists);
    marshallByte(th, ghost.colour);
    marshallBoolean(th, ghost.flies);

    marshallSpells(th, ghost.spells);
}

static ghost_demon unmarshallGhost(reader &th)
{
    ghost_demon ghost;

    ghost.name             = unmarshallString(th);
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
#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_GHOST_ENERGY)
        ghost.move_energy  = 10;
    else
#endif
    ghost.move_energy      = unmarshallShort(th);
    // fix up ghost_demons that forgot to have move_energy initialized
    if (ghost.move_energy < FASTEST_PLAYER_MOVE_SPEED
        || ghost.move_energy > 15) // Ponderous naga
    {
        ghost.move_energy = 10;
    }
    ghost.see_invis        = unmarshallByte(th);
    ghost.brand            = static_cast<brand_type>(unmarshallShort(th));
    ghost.att_type = static_cast<attack_type>(unmarshallShort(th));
    ghost.att_flav = static_cast<attack_flavour>(unmarshallShort(th));
    ghost.resists          = unmarshallInt(th);
#if TAG_MAJOR_VERSION == 34
    if (ghost.resists & MR_OLD_RES_ACID)
        set_resist(ghost.resists, MR_RES_ACID, 3);
    if (th.getMinorVersion() < TAG_MINOR_NO_GHOST_SPELLCASTER)
        unmarshallByte(th);
    if (th.getMinorVersion() < TAG_MINOR_MON_COLOUR_LOOKUP)
        unmarshallByte(th);
#endif
    ghost.colour           = unmarshallByte(th);

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_BOOL_FLIGHT)
        ghost.flies        = unmarshallShort(th);
    else
#endif
    ghost.flies        = unmarshallBoolean(th);

    unmarshallSpells(th, ghost.spells
#if TAG_MAJOR_VERSION == 34
                     , ghost.xl
#endif
                    );

    return ghost;
}

static void tag_construct_ghost(writer &th)
{
    // How many ghosts?
    marshallShort(th, ghosts.size());

    for (const ghost_demon &ghost : ghosts)
        marshallGhost(th, ghost);
}

static void tag_read_ghost(reader &th)
{
    int nghosts = unmarshallShort(th);

    if (nghosts < 1 || nghosts > MAX_GHOSTS)
        return;

    for (int i = 0; i < nghosts; ++i)
        ghosts.push_back(unmarshallGhost(th));
}
