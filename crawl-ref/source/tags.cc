/*
 *  File:       tags.cc
 *  Summary:    Auxilary functions to make savefile versioning simpler.
 *  Written by: Gordon Lipford
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

/* ------------------------- how tags work ----------------------------------

1. Tag types are enumerated in tags.h.

2. Tags are written to a FILE* using tag_write(tag_type t).  The serialization
   request is forwarded appropriately.

3. Tags are read from a FILE* with tag_read(), which does not need a tag_type
   argument.  A header is read, which tells tag_read what to construct.

4. In order to know which tags are used by a particular file type, a client
   calls tag_set_expected( fileType ), which sets up an array of chars.
   Within the array, a value of 1 means the tag is expected; -1 means that
   the tag is not expected.  A client can then set values in this array to
   anything other than 1 to indicate a successful tag_read() of that tag.

5. A case should be provided in tag_missing() for any tag which might be
   missing from a tagged save file.  For example, if a developer adds
   TAG_YOU_NEW_STUFF to the player save file, he would have to provide a
   case in tag_missing() for this tag since it might not be there in
   earlier savefiles.   The tags defined with the original tag system (and
   so not needing cases in tag_missing()) are as follows:

    TAG_YOU = 1,              // 'you' structure
    TAG_YOU_ITEMS,            // your items
    TAG_YOU_DUNGEON,          // dungeon specs (stairs, branches, features)
    TAG_LEVEL,                // various grids & clouds
    TAG_LEVEL_ITEMS,          // items/traps
    TAG_LEVEL_MONSTERS,       // monsters
    TAG_GHOST,                // ghost

6. The marshalling and unmarshalling of data is done in network order and
   is meant to keep savefiles cross-platform.  They are non-ascii - always
   FTP in binary mode.  Note also that the marshalling sizes are 1, 2, and 4
   for byte, short, and long - assuming that 'int' would have been
   insufficient on 16 bit systems (and that Crawl otherwise lacks
   system-independent data types).

*/

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

#include "AppHdr.h"
REVISION("$Rev$");

#include "abl-show.h"
#include "branch.h"
#include "describe.h"
#include "dungeon.h"
#include "enum.h"
#include "externs.h"
#include "files.h"
#include "ghost.h"
#include "itemname.h"
#include "itemprop.h"
#include "mapmark.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mtransit.h"
#include "quiver.h"
#include "randart.h"
#include "skills.h"
#include "skills2.h"
#include "stuff.h"
#include "tags.h"
#include "tiles.h"
#include "tilemcache.h"
#include "travel.h"
#include "view.h"

// defined in overmap.cc
extern std::map<branch_type, level_id> stair_level;
extern std::map<level_pos, shop_type> shops_present;
extern std::map<level_pos, god_type> altars_present;
extern std::map<level_pos, portal_type> portals_present;
extern std::map<level_pos, std::string> portal_vaults_present;
extern std::map<level_pos, std::string> portal_vault_notes;
extern std::map<level_pos, char> portal_vault_colours;
extern std::map<level_id, std::string> level_annotations;

// temp file pairs used for file level cleanup

level_id_set Generated_Levels;

// The minor version for the tag currently being read.
static int _tag_minor_version = -1;

// Reads input in network byte order, from a file or buffer.
unsigned char reader::readByte()
{
    if (_file)
        return static_cast<unsigned char>(fgetc(_file));
    else
        return (*_pbuf)[_read_offset++];
}

void reader::read(void *data, size_t size)
{
    if (_file)
    {
        if (data)
            fread(data, 1, size, _file);
        else
            fseek(_file, (long)size, SEEK_CUR);
    }
    else
    {
        ASSERT(_read_offset+size <= _pbuf->size());
        if (data)
            memcpy(data, &(*_pbuf)[_read_offset], size);

        _read_offset += size;
    }
}

void writer::writeByte(unsigned char ch)
{
    if (_file)
        fputc(ch, _file);
    else
        _pbuf->push_back(ch);
}

void writer::write(const void *data, size_t size)
{
    if (_file)
        fwrite(data, 1, size, _file);
    else
    {
        const unsigned char* cdata = static_cast<const unsigned char*>(data);
        _pbuf->insert(_pbuf->end(), cdata, cdata+size);
    }
}

long writer::tell()
{
    ASSERT(_file);
    return ftell(_file);
}


// static helpers
static void tag_construct_you(writer &th);
static void tag_construct_you_items(writer &th);
static void tag_construct_you_dungeon(writer &th);
static void tag_construct_lost_monsters(writer &th);
static void tag_construct_lost_items(writer &th);
static void tag_read_you(reader &th, char minorVersion);
static void tag_read_you_items(reader &th, char minorVersion);
static void tag_read_you_dungeon(reader &th);
static void tag_read_lost_monsters(reader &th, int minorVersion);
static void tag_read_lost_items(reader &th, int minorVersion);

static void tag_construct_level(writer &th);
static void tag_construct_level_items(writer &th);
static void tag_construct_level_monsters(writer &th);
static void tag_construct_level_attitude(writer &th);
static void tag_construct_level_tiles(writer &th);
static void tag_read_level(reader &th, char minorVersion);
static void tag_read_level_items(reader &th, char minorVersion);
static void tag_read_level_monsters(reader &th, char minorVersion);
static void tag_read_level_attitude(reader &th);
static void tag_missing_level_attitude();
static void tag_read_level_tiles(struct reader &th);
static void tag_missing_level_tiles();

static void tag_construct_ghost(writer &th);
static void tag_read_ghost(reader &th, char minorVersion);

static void marshallGhost(writer &th, const ghost_demon &ghost);
static ghost_demon unmarshallGhost( reader &th );

static void marshallResists(writer &, const mon_resist_def &);
static void unmarshallResists(reader &, mon_resist_def &);

static void marshallSpells(writer &, const monster_spells &);
static void unmarshallSpells(reader &, monster_spells &);

static void marshall_monster(writer &th, const monsters &m);
static void unmarshall_monster(reader &th, monsters &m);

template<typename T, typename T_iter, typename T_marshal>
static void marshall_iterator(writer &th, T_iter beg, T_iter end,
                              T_marshal marshal);
template<typename T>
static void unmarshall_vector(reader& th, std::vector<T>& vec,
                              T (*T_unmarshall)(reader&));


// provide a wrapper for file writing, just in case.
int write2(FILE * file, const void *buffer, unsigned int count)
{
    return fwrite(buffer, 1, count, file);
}

// provide a wrapper for file reading, just in case.
int read2(FILE * file, void *buffer, unsigned int count)
{
    return fread(buffer, 1, count, file);
}

void marshallByte(writer &th, const char& data)
{
    th.writeByte(data);
}

char unmarshallByte(reader &th)
{
    return th.readByte();
}

void marshallShort(std::vector<unsigned char>& buf, short data)
{
    COMPILE_CHECK(sizeof(data) == 2, c1);
    buf.push_back((unsigned char) ((data & 0xFF00) >> 8));
    buf.push_back((unsigned char) ((data & 0x00FF)     ));
}

// Marshall 2 byte short in network order.
void marshallShort(writer &th, short data)
{
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

void marshallLong(std::vector<unsigned char>& buf, int32_t data)
{
    buf.push_back((unsigned char) ((data & 0xFF000000) >> 24));
    buf.push_back((unsigned char) ((data & 0x00FF0000) >> 16));
    buf.push_back((unsigned char) ((data & 0x0000FF00) >>  8));
    buf.push_back((unsigned char) ((data & 0x000000FF)      ));
}

// Marshall 4 byte int in network order.
void marshallLong(writer &th, int32_t data)
{
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
int32_t unmarshallLong(reader &th)
{
    int32_t b1 = th.readByte();
    int32_t b2 = th.readByte();
    int32_t b3 = th.readByte();
    int32_t b4 = th.readByte();

    int32_t data = (b1 << 24) | ((b2 & 0x000000FF) << 16);
    data |= ((b3 & 0x000000FF) << 8) | (b4 & 0x000000FF);
    return data;
}

// FIXME: Kill this abomination - it will break!
template<typename T>
void marshall_as_long(writer& th, const T& t)
{
    marshallLong( th, static_cast<long>(t) );
}

template <typename data>
void marshallSet(writer &th, const std::set<data> &s,
                 void (*marshall)(writer &, const data &))
{
    marshallLong( th, s.size() );
    typename std::set<data>::const_iterator i = s.begin();
    for ( ; i != s.end(); ++i)
        marshall(th, *i);
}

template<typename key, typename value>
void marshallMap(writer &th, const std::map<key,value>& data,
                 void (*key_marshall)(writer&, const key&),
                 void (*value_marshall)(writer&, const value&))
{
    marshallLong( th, data.size() );
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
    marshallLong(th, std::distance(beg, end));
    while ( beg != end )
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
    const long num_to_read = unmarshallLong(th);
    for (long i = 0; i < num_to_read; ++i)
        vec.push_back( T_unmarshall(th) );
}

template <typename T_container, typename T_inserter, typename T_unmarshall>
static void unmarshall_container(reader &th, T_container &container,
                                 T_inserter inserter, T_unmarshall unmarshal)
{
    container.clear();
    const long num_to_read = unmarshallLong(th);
    for (long i = 0; i < num_to_read; ++i)
        (container.*inserter)(unmarshal(th));
}

// XXX: Redundant with level_id.save()/load().
void marshall_level_id( writer& th, const level_id& id )
{
    marshallByte(th, id.branch );
    marshallLong(th, id.depth );
    marshallByte(th, id.level_type);
}

// XXX: Redundant with level_pos.save()/load().
void marshall_level_pos( writer& th, const level_pos& lpos )
{
    marshallLong(th, lpos.pos.x);
    marshallLong(th, lpos.pos.y);
    marshall_level_id(th, lpos.id);
}

template <typename data, typename set>
void unmarshallSet(reader &th, set &dset,
                   data (*data_unmarshall)(reader &))
{
    dset.clear();
    long len = unmarshallLong(th);
    for (long i = 0L; i < len; ++i)
        dset.insert(data_unmarshall(th));
}

template<typename key, typename value, typename map>
void unmarshallMap(reader& th, map& data,
                   key   (*key_unmarshall)  (reader&),
                   value (*value_unmarshall)(reader&) )
{
    long i, len = unmarshallLong(th);
    key k;
    for (i = 0; i < len; ++i)
    {
        k = key_unmarshall(th);
        std::pair<key, value> p(k, value_unmarshall(th));
        data.insert(p);
    }
}

template<typename T>
T unmarshall_long_as( reader& th )
{
    return static_cast<T>(unmarshallLong(th));
}

level_id unmarshall_level_id( reader& th )
{
    level_id id;
    id.branch     = static_cast<branch_type>(unmarshallByte(th));
    id.depth      = unmarshallLong(th);
    id.level_type = static_cast<level_area_type>(unmarshallByte(th));
    return (id);
}

level_pos unmarshall_level_pos( reader& th )
{
    level_pos lpos;
    lpos.pos.x = unmarshallLong(th);
    lpos.pos.y = unmarshallLong(th);
    lpos.id    = unmarshall_level_id(th);
    return lpos;
}

void marshallCoord(writer &th, const coord_def &c)
{
    marshallShort(th, c.x);
    marshallShort(th, c.y);
}

void unmarshallCoord(reader &th, coord_def &c)
{
    c.x = unmarshallShort(th);
    c.y = unmarshallShort(th);
}

template <typename marshall, typename grid>
void run_length_encode(writer &th, marshall m, const grid &g,
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
void run_length_decode(reader &th, unmarshall um, grid &g,
                       int width, int height)
{
    const int end = width * height;
    int offset = 0;
    while (offset < end)
    {
        const int run = (unsigned char) unmarshallByte(th);
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
    float f_num;
    long  l_num;
};

// single precision float -- marshall in network order.
void marshallFloat(writer &th, float data)
{
    float_marshall_kludge k;
    k.f_num = data;
    marshallLong(th, k.l_num);
}

// single precision float -- unmarshall in network order.
float unmarshallFloat(reader &th)
{
    float_marshall_kludge k;
    k.l_num = unmarshallLong(th);
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
int unmarshallCString(reader &th, char *data, int maxSize)
{
    // Get length.
    short len = unmarshallShort(th);
    int copylen = len;

    if (len >= maxSize && maxSize > 0)
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
    marshallLong(th, data.length());
    th.write(data.c_str(), data.length());
}
void unmarshallString4(reader &th, std::string& s)
{
    const int len = unmarshallLong(th);
    s.resize(len);
    if (len) th.read(&s.at(0), len);
}

// boolean (to avoid system-dependant bool implementations)
void marshallBoolean(writer &th, bool data)
{
    char charRep = 0;       // for false
    if (data)
        charRep = 1;

    th.writeByte(charRep);
}

// boolean (to avoid system-dependant bool implementations)
bool unmarshallBoolean(reader &th)
{
    bool data;
    const char read = th.readByte();

    if (read == 1)
        data = true;
    else
        data = false;

    return data;
}

// Saving the date as a string so we're not reliant on a particular epoch.
std::string make_date_string( time_t in_date )
{
    char buff[20];

    if (in_date <= 0)
    {
        buff[0] = 0;
        return (buff);
    }

    struct tm *date = TIME_FN( &in_date );

    snprintf( buff, sizeof buff,
              "%4d%02d%02d%02d%02d%02d%s",
              date->tm_year + 1900, date->tm_mon, date->tm_mday,
              date->tm_hour, date->tm_min, date->tm_sec,
              ((date->tm_isdst > 0) ? "D" : "S") );

    return (buff);
}

static int get_val_from_string( const char *ptr, int len )
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

time_t parse_date_string( char buff[20] )
{
    struct tm date;

    date.tm_year = get_val_from_string( &buff[0],  4 ) - 1900;
    date.tm_mon  = get_val_from_string( &buff[4],  2 );
    date.tm_mday = get_val_from_string( &buff[6],  2 );
    date.tm_hour = get_val_from_string( &buff[8],  2 );
    date.tm_min  = get_val_from_string( &buff[10], 2 );
    date.tm_sec  = get_val_from_string( &buff[12], 2 );

    date.tm_isdst = (buff[14] == 'D');

    return (mktime( &date ));
}

// Write a tagged chunk of data to the FILE*.
// tagId specifies what to write.
void tag_write(tag_type tagID, FILE* outf)
{
    std::vector<unsigned char> buf;
    writer th(&buf);
    switch(tagID)
    {
    case TAG_YOU:            tag_construct_you(th);            break;
    case TAG_YOU_ITEMS:      tag_construct_you_items(th);      break;
    case TAG_YOU_DUNGEON:    tag_construct_you_dungeon(th);    break;
    case TAG_LEVEL:          tag_construct_level(th);          break;
    case TAG_LEVEL_ITEMS:    tag_construct_level_items(th);    break;
    case TAG_LEVEL_MONSTERS: tag_construct_level_monsters(th); break;
    case TAG_LEVEL_TILES:    tag_construct_level_tiles(th);    break;
    case TAG_LEVEL_ATTITUDE: tag_construct_level_attitude(th); break;
    case TAG_GHOST:          tag_construct_ghost(th);          break;
    case TAG_LOST_MONSTERS:
        tag_construct_lost_monsters(th);
        tag_construct_lost_items(th);
        break;
    default:
        // I don't know how to make that!
        break;
    }

    // make sure there is some data to write!
    if (buf.size() == 0)
        return;

    // Write tag header.
    {
        writer tmp(outf);
        marshallShort(tmp, tagID);
        marshallLong(tmp, buf.size());
    }

    // Write tag data.
    write2(outf, &buf[0], buf.size());
}

// Read a single tagged chunk of data from fp into memory.
// TAG_NO_TAG is returned if there's nothing left to read in the file
// (or on an error).
//
// minorVersion is available for any sub-readers that need it
// (like TAG_LEVEL_MONSTERS).
tag_type tag_read(FILE *fp, char minorVersion)
{
    // Read header info and data
    short tag_id;
    std::vector<unsigned char> buf;
    {
        reader tmp(fp);
        tag_id = unmarshallShort(tmp);
        if (tag_id < 0)
            return TAG_NO_TAG;
        const long data_size = unmarshallLong(tmp);
        if (data_size < 0)
            return TAG_NO_TAG;

        // Fetch data in one go
        buf.resize(data_size);
        if (read2(fp, &buf[0], buf.size()) != (int)buf.size())
            return TAG_NO_TAG;
    }

    unwind_var<int> tag_minor_version(_tag_minor_version, minorVersion);

    // Ok, we have data now.
    reader th(buf);
    switch (tag_id)
    {
    case TAG_YOU:            tag_read_you(th, minorVersion);            break;
    case TAG_YOU_ITEMS:      tag_read_you_items(th, minorVersion);      break;
    case TAG_YOU_DUNGEON:    tag_read_you_dungeon(th);                  break;
    case TAG_LEVEL:          tag_read_level(th, minorVersion);          break;
    case TAG_LEVEL_ITEMS:    tag_read_level_items(th, minorVersion);    break;
    case TAG_LEVEL_MONSTERS: tag_read_level_monsters(th, minorVersion); break;
    case TAG_LEVEL_ATTITUDE: tag_read_level_attitude(th);               break;
    case TAG_LEVEL_TILES:    tag_read_level_tiles(th);                  break;
    case TAG_GHOST:          tag_read_ghost(th, minorVersion);          break;
    case TAG_LOST_MONSTERS:
        tag_read_lost_monsters(th, minorVersion);
        tag_read_lost_items(th, minorVersion);
        break;
    default:
        // I don't know how to read that!
        ASSERT(false);
        return TAG_NO_TAG;
    }

    return static_cast<tag_type>(tag_id);
}


// Older savefiles might want to call this to get a tag properly
// initialized if it wasn't part of the savefile.
// For now, none are supported.

// This function will be called AFTER all other tags for the savefile
// are read, so everything that can be initialized should have been by now.

// minorVersion is available for any child functions that need it
// (currently none).
void tag_missing(int tag, char minorVersion)
{
    UNUSED( minorVersion );

    switch(tag)
    {
        case TAG_LEVEL_ATTITUDE:
            tag_missing_level_attitude();
            break;
        case TAG_LEVEL_TILES:
            tag_missing_level_tiles();
            break;
        default:
            perror("Tag is missing; file is likely corrupt.");
            end(-1);
    }
}

// utility
void tag_set_expected(char tags[], int fileType)
{
    int i;

    for (i = 0; i < NUM_TAGS; i++)
    {
        tags[i] = -1;
        switch (fileType)
        {
            case TAGTYPE_PLAYER:
                if (i >= TAG_YOU && i <= TAG_YOU_DUNGEON
                    || i == TAG_LOST_MONSTERS)
                {
                    tags[i] = 1;
                }
                break;
            case TAGTYPE_PLAYER_NAME:
                if (i == TAG_YOU)
                    tags[i] = 1;
                break;
            case TAGTYPE_LEVEL:
                if (i >= TAG_LEVEL && i <= TAG_LEVEL_ATTITUDE && i != TAG_GHOST)
                    tags[i] = 1;
#ifdef USE_TILE
                if (i == TAG_LEVEL_TILES)
                    tags[i] = 1;
#endif
                break;
            case TAGTYPE_GHOST:
                if (i == TAG_GHOST)
                    tags[i] = 1;
                break;
            default:
                // I don't know what kind of file that is!
                break;
        }
    }
}

// NEVER _MODIFY_ THE CONSTRUCT/READ FUNCTIONS, EVER.  THAT IS THE WHOLE POINT
// OF USING TAGS.  Apologies for the screaming.

// Note anyway that the formats are somewhat flexible;  you could change map
// size, the # of slots in player inventory, etc.  Constants like GXM,
// NUM_EQUIP, and NUM_DURATIONS are saved, so the appropriate amount will
// be restored even if a later version increases these constants.

// --------------------------- player tags (foo.sav) -------------------- //
static void tag_construct_you(writer &th)
{
    int i,j;

    marshallString(th, you.your_name, 30);

    marshallByte(th, you.religion);
    marshallByte(th, you.piety);
    marshallByte(th, you.rotting);
    marshallByte(th, you.symbol);
    marshallByte(th, you.colour);
    marshallShort(th, you.pet_target);

    marshallByte(th, you.max_level);
    marshallByte(th, you.where_are_you);
    marshallByte(th, you.char_direction);
    marshallByte(th, you.your_level);
    marshallByte(th, you.is_undead);
    marshallByte(th, you.special_wield);
    marshallByte(th, you.berserk_penalty);
    marshallShort(th, you.sage_bonus_skill);
    marshallLong(th, you.sage_bonus_degree);
    marshallByte(th, you.level_type);
    marshallString(th, you.level_type_name);
    marshallString(th, you.level_type_name_abbrev);
    marshallString(th, you.level_type_origin);
    marshallString(th, you.level_type_tag);
    marshallString(th, you.level_type_ext);
    marshallByte(th, you.entry_cause);
    marshallByte(th, you.entry_cause_god);
    marshallByte(th, you.synch_time);
    marshallByte(th, you.disease);
    marshallByte(th, you.species);

    marshallShort(th, you.hp);

    marshallShort(th, you.hunger);

    // how many you.equip?
    marshallByte(th, NUM_EQUIP);
    for (i = 0; i < NUM_EQUIP; ++i)
        marshallByte(th,you.equip[i]);

    marshallByte(th, you.magic_points);
    marshallByte(th, you.max_magic_points);
    marshallByte(th, you.strength);
    marshallByte(th, you.intel);
    marshallByte(th, you.dex);
    marshallByte(th, you.hit_points_regeneration);
    marshallByte(th, you.magic_points_regeneration);

    marshallShort(th, you.hit_points_regeneration * 100);
    marshallLong(th, you.experience);
    marshallLong(th, you.gold);

    marshallByte(th, you.char_class);
    marshallByte(th, you.experience_level);
    marshallLong(th, you.exp_available);

    // max values
    marshallByte(th, you.max_strength);
    marshallByte(th, you.max_intel);
    marshallByte(th, you.max_dex);

    marshallShort(th, you.base_hp);
    marshallShort(th, you.base_hp2);
    marshallShort(th, you.base_magic_points);
    marshallShort(th, you.base_magic_points2);

    marshallShort(th, you.pos().x);
    marshallShort(th, you.pos().y);

    marshallString(th, you.class_name, 30);

    marshallShort(th, you.burden);

    // how many spells?
    marshallByte(th, 25);
    for (i = 0; i < 25; ++i)
        marshallByte(th, you.spells[i]);

    marshallByte(th, 52);
    for (i = 0; i < 52; i++)
        marshallByte( th, you.spell_letter_table[i] );

    marshallByte(th, 52);
    for (i = 0; i < 52; i++)
        marshallShort( th, you.ability_letter_table[i] );

    // how many skills?
    marshallByte(th, 50);
    for (j = 0; j < 50; ++j)
    {
        marshallByte(th, you.skills[j]);
        marshallByte(th, you.practise_skill[j]);
        marshallLong(th, you.skill_points[j]);
        marshallByte(th, you.skill_order[j]);   // skills ordering
    }

    // how many durations?
    marshallByte(th, NUM_DURATIONS);
    for (j = 0; j < NUM_DURATIONS; ++j)
        marshallLong(th, you.duration[j]);

    // how many attributes?
    marshallByte(th, NUM_ATTRIBUTES);
    for (j = 0; j < NUM_ATTRIBUTES; ++j)
        marshallLong(th, you.attribute[j]);

    // Was: remembered quiver items.
    marshallByte(th, 0);

    // Sacrifice values.
    marshallByte(th, NUM_OBJECT_CLASSES);
    for (j = 0; j < NUM_OBJECT_CLASSES; ++j)
        marshallLong(th, you.sacrifice_value[j]);

    // how many mutations/demon powers?
    marshallShort(th, NUM_MUTATIONS);
    for (j = 0; j < NUM_MUTATIONS; ++j)
    {
        marshallByte(th, you.mutation[j]);
        marshallByte(th, you.demon_pow[j]);
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
        marshallShort(th, you.num_gifts[i]);

    marshallByte(th, you.gift_timeout);
    marshallByte(th, you.normal_vision);
    marshallByte(th, you.current_vision);
    marshallByte(th, you.hell_exit);

    // elapsed time
    marshallFloat(th, (float)you.elapsed_time);

    // wizard mode used
    marshallByte(th, you.wizard);

    // time of game start
    marshallString(th, make_date_string( you.birth_time ).c_str(), 20);

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

    marshallLong( th, you.real_time );
    marshallLong( th, you.num_turns );

    marshallShort(th, you.magic_contamination);

    marshallShort(th, you.transit_stair);
    marshallByte(th, you.entering_level);

    // lava_in_sight and water_in_sight don't need to be saved as they can
    // be recalculated on game start.

    // List of currently beholding monsters (usually empty).
    marshallByte(th, you.mesmerised_by.size());
    for (unsigned int k = 0; k < you.mesmerised_by.size(); k++)
         marshallByte(th, you.mesmerised_by[k]);

    // minorVersion TAG_MINOR_PIETY starts here
    marshallByte(th, you.piety_hysteresis);

    // minorVersion TAG_MINOR_QUIVER starts here
    you.m_quiver->save(th);

    // minorVersion TAG_MINOR_FPICKUP starts here
    marshallByte(th, you.friendly_pickup);

    // minorVersion TAG_MINOR_LUADGN starts here
    if (!dlua.callfn("dgn_save_data", "u", &th))
        mprf(MSGCH_ERROR, "Failed to save Lua data: %s", dlua.error.c_str());

    // minorVersion TAG_MINOR_SVNREV starts here
    // Write a human-readable string out on the off chance that
    // we fail to be able to read this file back in using some later version.
    std::string revision = "SVN:";
    revision += number_to_string(svn_revision());
    revision += ":";
    revision += VERSION_DETAIL;
    marshallString(th, revision);
    marshallLong(th, svn_revision());
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
    marshallByte(th, static_cast<char>(identy.width()));
    // how many subtypes?
    marshallByte(th, static_cast<char>(identy.height()));

    for (i = 0; i < identy.width(); ++i)
        for (j = 0; j < identy.height(); ++j)
            marshallByte(th, static_cast<char>(identy[i][j]));

    // how many unique items?
    marshallByte(th, 50);
    for (j = 0; j < 50; ++j)
        marshallByte(th,you.unique_items[j]);

    marshallByte(th, NUM_FIXED_BOOKS);
    for (j = 0; j < NUM_FIXED_BOOKS; ++j)
        marshallByte(th,you.had_book[j]);

    marshallShort(th, NUM_SPELLS);
    for (j = 0; j < NUM_SPELLS; ++j)
        marshallByte(th,you.seen_spell[j]);

    // how many unrandarts?
    marshallShort(th, NO_UNRANDARTS);

    for (j = 0; j < NO_UNRANDARTS; ++j)
        marshallBoolean(th, does_unrandart_exist(j));
}

static void marshallPlaceInfo(writer &th, PlaceInfo place_info)
{
    marshallLong(th, place_info.level_type);
    marshallLong(th, place_info.branch);

    marshallLong(th, place_info.num_visits);
    marshallLong(th, place_info.levels_seen);

    marshallLong(th, place_info.mon_kill_exp);
    marshallLong(th, place_info.mon_kill_exp_avail);

    for (int i = 0; i < KC_NCATEGORIES; i++)
        marshallLong(th, place_info.mon_kill_num[i]);

    marshallLong(th, place_info.turns_total);
    marshallLong(th, place_info.turns_explore);
    marshallLong(th, place_info.turns_travel);
    marshallLong(th, place_info.turns_interlevel);
    marshallLong(th, place_info.turns_resting);
    marshallLong(th, place_info.turns_other);

    marshallFloat(th, place_info.elapsed_total);
    marshallFloat(th, place_info.elapsed_explore);
    marshallFloat(th, place_info.elapsed_travel);
    marshallFloat(th, place_info.elapsed_interlevel);
    marshallFloat(th, place_info.elapsed_resting);
    marshallFloat(th, place_info.elapsed_other);
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
        marshallLong(th, branches[j].startdepth);
        marshallLong(th, branches[j].branch_flags);
    }

    marshallSet(th, Generated_Levels, marshall_level_id);

    marshallMap(th, stair_level,
                marshall_as_long<branch_type>, marshall_level_id);
    marshallMap(th, shops_present,
                marshall_level_pos, marshall_as_long<shop_type>);
    marshallMap(th, altars_present,
                marshall_level_pos, marshall_as_long<god_type>);
    marshallMap(th, portals_present,
                marshall_level_pos, marshall_as_long<portal_type>);
    marshallMap(th, portal_vaults_present,
                marshall_level_pos, marshallStringNoMax);
    marshallMap(th, portal_vault_notes,
                marshall_level_pos, marshallStringNoMax);
    marshallMap(th, portal_vault_colours,
                marshall_level_pos, marshallByte);
    marshallMap(th, level_annotations,
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
    marshall_monster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        marshallItem(th, f.items[i]);
}

static void unmarshall_follower(reader &th, follower &f)
{
    unmarshall_monster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        unmarshallItem(th, f.items[i]);
}

static void marshall_follower_list(writer &th, const m_transit_list &mlist)
{
    marshallShort( th, mlist.size() );

    for (m_transit_list::const_iterator mi = mlist.begin();
         mi != mlist.end(); ++mi)
    {
        marshall_follower( th, *mi );
    }
}

static void marshall_item_list(writer &th, const i_transit_list &ilist)
{
    marshallShort( th, ilist.size() );

    for (i_transit_list::const_iterator ii = ilist.begin();
         ii != ilist.end(); ++ii)
    {
        marshallItem( th, *ii );
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

static void tag_construct_lost_monsters(writer &th)
{
    marshallMap( th, the_lost_ones, marshall_level_id,
                 marshall_follower_list );
}

static void tag_construct_lost_items(writer &th)
{
    marshallMap( th, transiting_items, marshall_level_id,
                 marshall_item_list );
}

static void tag_read_you(reader &th, char minorVersion)
{
    char buff[20];      // For birth date.
    int i,j;
    char count_c;
    short count_s;

    unmarshallCString(th, you.your_name, 30);

    you.religion          = static_cast<god_type>(unmarshallByte(th));
    you.piety             = unmarshallByte(th);
    you.rotting           = unmarshallByte(th);
    you.symbol            = unmarshallByte(th);
    you.colour            = unmarshallByte(th);
    you.pet_target        = unmarshallShort(th);

    you.max_level         = unmarshallByte(th);
    you.where_are_you     = static_cast<branch_type>( unmarshallByte(th) );
    you.char_direction    = static_cast<game_direction_type>(unmarshallByte(th));
    you.your_level        = unmarshallByte(th);
    you.is_undead         = static_cast<undead_state_type>(unmarshallByte(th));
    you.special_wield     = unmarshallByte(th);
    you.berserk_penalty   = unmarshallByte(th);
    you.sage_bonus_skill  = static_cast<skill_type>(unmarshallShort(th));
    you.sage_bonus_degree = unmarshallLong(th);
    you.level_type        = static_cast<level_area_type>( unmarshallByte(th) );
    you.level_type_name   = unmarshallString(th);

    if (minorVersion >= TAG_MINOR_LUADGN)
    {
        you.level_type_name_abbrev = unmarshallString(th);
        you.level_type_origin      = unmarshallString(th);
        you.level_type_tag         = unmarshallString(th);

        if (minorVersion >= TAG_MINOR_PORTEXT)
            you.level_type_ext = unmarshallString(th);
    }

    you.entry_cause     = static_cast<entry_cause_type>( unmarshallByte(th) );
    you.entry_cause_god = static_cast<god_type>( unmarshallByte(th) );
    you.synch_time      = unmarshallByte(th);
    you.disease         = unmarshallByte(th);
    you.species         = static_cast<species_type>(unmarshallByte(th));
    you.hp              = unmarshallShort(th);
    you.hunger          = unmarshallShort(th);

    // How many you.equip?
    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; ++i)
        you.equip[i] = unmarshallByte(th);

    you.magic_points              = unmarshallByte(th);
    you.max_magic_points          = unmarshallByte(th);
    you.strength                  = unmarshallByte(th);
    you.intel                     = unmarshallByte(th);
    you.dex                       = unmarshallByte(th);
    you.hit_points_regeneration   = unmarshallByte(th);
    you.magic_points_regeneration = unmarshallByte(th);

    you.hit_points_regeneration   = unmarshallShort(th) / 100;
    you.experience                = unmarshallLong(th);
    you.gold                      = unmarshallLong(th);

    you.char_class                = static_cast<job_type>(unmarshallByte(th));
    you.experience_level          = unmarshallByte(th);
    you.exp_available             = unmarshallLong(th);

    // max values
    you.max_strength              = unmarshallByte(th);
    you.max_intel                 = unmarshallByte(th);
    you.max_dex                   = unmarshallByte(th);

    you.base_hp                   = unmarshallShort(th);
    you.base_hp2                  = unmarshallShort(th);
    you.base_magic_points         = unmarshallShort(th);
    you.base_magic_points2        = unmarshallShort(th);

    const int x = unmarshallShort(th);
    const int y = unmarshallShort(th);
    you.moveto(coord_def(x, y));

    unmarshallCString(th, you.class_name, 30);

    you.burden = unmarshallShort(th);

    // how many spells?
    you.spell_no = 0;
    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; ++i)
    {
        you.spells[i] =
            static_cast<spell_type>( (unsigned char) unmarshallByte(th) );
        if (you.spells[i] != SPELL_NO_SPELL)
            you.spell_no++;
    }

    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; i++)
        you.spell_letter_table[i] = unmarshallByte(th);

    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; i++)
    {
        you.ability_letter_table[i] =
            static_cast<ability_type>(unmarshallShort(th));
    }

    // how many skills?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
    {
        you.skills[j]         = unmarshallByte(th);
        you.practise_skill[j] = unmarshallByte(th);
        you.skill_points[j]   = unmarshallLong(th);
        you.skill_order[j]    = unmarshallByte(th);
    }

    // Set up you.total_skill_points and you.skill_cost_level.
    calc_total_skill_points();

    // how many durations?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        you.duration[j] = unmarshallLong(th);

    // how many attributes?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        you.attribute[j] = unmarshallLong(th);

    // old: quiver info.  Discard it.
    count_c = unmarshallByte(th);
    if (minorVersion >= TAG_MINOR_QUIVER)
        ASSERT(count_c == 0);

    for (j = 0; j < count_c; ++j)
        unmarshallByte(th);

    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        you.sacrifice_value[j] = unmarshallLong(th);

    // how many mutations/demon powers?
    count_s = unmarshallShort(th);
    for (j = 0; j < count_s; ++j)
    {
        you.mutation[j]  = unmarshallByte(th);
        you.demon_pow[j] = unmarshallByte(th);
    }

    // how many penances?
    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; i++)
        you.penance[i] = unmarshallByte(th);

    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; i++)
        you.worshipped[i] = unmarshallByte(th);

    for (i = 0; i < count_c; i++)
        you.num_gifts[i] = unmarshallShort(th);

    you.gift_timeout   = unmarshallByte(th);
    you.normal_vision  = unmarshallByte(th);
    you.current_vision = unmarshallByte(th);
    you.hell_exit      = unmarshallByte(th);

    // elapsed time
    you.elapsed_time   = (double)unmarshallFloat(th);

    // wizard mode
    you.wizard         = (bool) unmarshallByte(th);

    // time of character creation
    unmarshallCString( th, buff, 20 );
    you.birth_time = parse_date_string( buff );

    you.real_time  = unmarshallLong(th);
    you.num_turns  = unmarshallLong(th);

    you.magic_contamination = unmarshallShort(th);

    you.transit_stair  = static_cast<dungeon_feature_type>(unmarshallShort(th));
    you.entering_level = unmarshallByte(th);

    // These two need not be saved.
    you.lava_in_sight  = -1;
    you.water_in_sight = -1;

    // List of currently beholding monsters (usually empty).
    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; i++)
         you.mesmerised_by.push_back(unmarshallByte(th));

    if (minorVersion >= TAG_MINOR_PIETY)
        you.piety_hysteresis = unmarshallByte(th);

    if (minorVersion >= TAG_MINOR_QUIVER)
        you.m_quiver->load(th);

    if (minorVersion >= TAG_MINOR_FPICKUP)
        you.friendly_pickup = unmarshallByte(th);

    if (minorVersion >= TAG_MINOR_LUADGN)
    {
        if (!dlua.callfn("dgn_load_data", "u", &th))
            mprf(MSGCH_ERROR, "Failed to load Lua persist table: %s",
                 dlua.error.c_str());
    }

    if (minorVersion >= TAG_MINOR_SVNREV)
    {
        std::string rev_str = unmarshallString(th);
        int rev_int = unmarshallLong(th);

        UNUSED(rev_str);
        UNUSED(rev_int);
    }
}

static void tag_read_you_items(reader &th, char minorVersion)
{
    int i,j;
    char count_c, count_c2;
    short count_s;

    // how many inventory slots?
    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; ++i)
        unmarshallItem(th, you.inv[i]);

    // Item descrip for each type & subtype.
    // how many types?
    count_c = unmarshallByte(th);
    // how many subtypes?
    count_c2 = unmarshallByte(th);
    for (i = 0; i < count_c; ++i)
        for (j = 0; j < count_c2; ++j)
            you.item_description[i][j] = unmarshallByte(th);

    // Identification status.
    // how many types?
    count_c = unmarshallByte(th);
    // how many subtypes?
    count_c2 = unmarshallByte(th);

    // Argh... this is awful!
    for (i = 0; i < count_c; ++i)
        for (j = 0; j < count_c2; ++j)
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

    // how many unique items?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
    {
        you.unique_items[j] =
            static_cast<unique_item_status_type>(unmarshallByte(th));
    }

    // how many books?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        you.had_book[j] = unmarshallByte(th);

    // how many spells?
    count_s = unmarshallShort(th);
    for (j = 0; j < count_s; ++j)
        you.seen_spell[j] = unmarshallByte(th);

    // how many unrandarts?
    count_s = unmarshallShort(th);
    for (j = 0; j < count_s; ++j)
        set_unrandart_exist(j, unmarshallBoolean(th));

    // # of unrandarts could certainly change.
    // If it does, the new ones won't exist yet - zero them out.
    for (; j < NO_UNRANDARTS; j++)
        set_unrandart_exist(j, false);
}

static PlaceInfo unmarshallPlaceInfo(reader &th)
{
    PlaceInfo place_info;

    place_info.level_type = (int) unmarshallLong(th);
    place_info.branch     = (int) unmarshallLong(th);

    place_info.num_visits  = (unsigned long) unmarshallLong(th);
    place_info.levels_seen = (unsigned long) unmarshallLong(th);

    place_info.mon_kill_exp       = (unsigned long) unmarshallLong(th);
    place_info.mon_kill_exp_avail = (unsigned long) unmarshallLong(th);

    for (int i = 0; i < KC_NCATEGORIES; i++)
        place_info.mon_kill_num[i] = (unsigned long) unmarshallLong(th);

    place_info.turns_total      = unmarshallLong(th);
    place_info.turns_explore    = unmarshallLong(th);
    place_info.turns_travel     = unmarshallLong(th);
    place_info.turns_interlevel = unmarshallLong(th);
    place_info.turns_resting    = unmarshallLong(th);
    place_info.turns_other      = unmarshallLong(th);

    place_info.elapsed_total      = (double) unmarshallFloat(th);
    place_info.elapsed_explore    = (double) unmarshallFloat(th);
    place_info.elapsed_travel     = (double) unmarshallFloat(th);
    place_info.elapsed_interlevel = (double) unmarshallFloat(th);
    place_info.elapsed_resting    = (double) unmarshallFloat(th);
    place_info.elapsed_other      = (double) unmarshallFloat(th);

    return place_info;
}

static void tag_read_you_dungeon(reader &th)
{
    // how many unique creatures?
    int count_c = unmarshallShort(th);
    you.unique_creatures.init(false);
    for (int j = 0; j < count_c; ++j)
    {
        const bool created = static_cast<bool>(unmarshallByte(th));

        if (j < NUM_MONSTERS)
            you.unique_creatures[j] = created;
    }

    // how many branches?
    count_c = unmarshallByte(th);
    for (int j = 0; j < count_c; ++j)
    {
        branches[j].startdepth   = unmarshallLong(th);
        branches[j].branch_flags = (unsigned long) unmarshallLong(th);
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
                  unmarshall_level_pos, unmarshallByte);
    unmarshallMap(th, level_annotations,
                  unmarshall_level_id, unmarshallStringNoMax);

    PlaceInfo place_info = unmarshallPlaceInfo(th);
    ASSERT(place_info.is_global());
    you.set_place_info(place_info);

    std::vector<PlaceInfo> list = you.get_all_place_info();
    unsigned short count_p = (unsigned short) unmarshallShort(th);
    // Use "<=" so that adding more branches or non-dungeon places
    // won't break save-file compatibility.
    ASSERT(count_p <= list.size());

    for (int i = 0; i < count_p; i++)
    {
        place_info = unmarshallPlaceInfo(th);
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

static void tag_read_lost_monsters(reader &th, int minorVersion)
{
    the_lost_ones.clear();
    unmarshallMap(th, the_lost_ones,
                  unmarshall_level_id, unmarshall_follower_list);
}

static void tag_read_lost_items(reader &th, int minorVersion)
{
    transiting_items.clear();

    unmarshallMap(th, transiting_items,
                  unmarshall_level_id, unmarshall_item_list);
}

// ------------------------------- level tags ---------------------------- //

static void tag_construct_level(writer &th)
{
    marshallByte(th, env.floor_colour);
    marshallByte(th, env.rock_colour);

    marshallLong(th, env.level_flags);

    marshallFloat(th, (float)you.elapsed_time);

    // Map grids.
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    marshallLong(th, env.turns_on_level);

    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallByte(th, grd[count_x][count_y]);
            marshallShort(th, env.map[count_x][count_y].object);
            marshallShort(th, env.map[count_x][count_y].colour);
            marshallShort(th, env.map[count_x][count_y].flags);
            marshallLong(th, env.map[count_x][count_y].property);
            marshallShort(th, env.cgrid[count_x][count_y]);
        }

    run_length_encode(th, marshallByte, env.grid_colours, GXM, GYM);

    marshallShort(th, env.cloud_no);

    // how many clouds?
    marshallShort(th, MAX_CLOUDS);
    for (int i = 0; i < MAX_CLOUDS; i++)
    {
        marshallByte(th, env.cloud[i].pos.x);
        marshallByte(th, env.cloud[i].pos.y);
        marshallByte(th, env.cloud[i].type);
        marshallShort(th, env.cloud[i].decay);
        marshallByte(th,  (char) env.cloud[i].spread_rate);
        marshallByte(th, env.cloud[i].whose);
        marshallByte(th, env.cloud[i].killer);
    }

    // how many shops?
    marshallByte(th, MAX_SHOPS);
    for (int i = 0; i < MAX_SHOPS; i++)
    {
        marshallByte(th, env.shop[i].keeper_name[0]);
        marshallByte(th, env.shop[i].keeper_name[1]);
        marshallByte(th, env.shop[i].keeper_name[2]);
        marshallByte(th, env.shop[i].pos.x);
        marshallByte(th, env.shop[i].pos.y);
        marshallByte(th, env.shop[i].greed);
        marshallByte(th, env.shop[i].type);
        marshallByte(th, env.shop[i].level);
    }

    marshallCoord(th, env.sanctuary_pos);
    marshallByte(th, env.sanctuary_time);

    marshallLong(th, env.spawn_random_rate);

    env.markers.write(th);
    env.properties.write(th);
}

void marshallItem(writer &th, const item_def &item)
{
    marshallByte(th, item.base_type);
    marshallByte(th, item.sub_type);
    marshallShort(th, item.plus);
    marshallShort(th, item.plus2);
    marshallLong(th, item.special);
    marshallShort(th, item.quantity);

    marshallByte(th, item.colour);
    marshallShort(th, item.pos.x);
    marshallShort(th, item.pos.y);
    marshallLong(th, item.flags);

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
    item.sub_type    = (unsigned char) unmarshallByte(th);
    item.plus        = unmarshallShort(th);
    item.plus2       = unmarshallShort(th);
    item.special     = unmarshallLong(th);
    item.quantity    = unmarshallShort(th);
    item.colour      = (unsigned char) unmarshallByte(th);
    item.pos.x       = unmarshallShort(th);
    item.pos.y       = unmarshallShort(th);
    item.flags       = (unsigned long) unmarshallLong(th);
    item.link        = unmarshallShort(th);

    unmarshallShort(th);  // igrd[item.x][item.y] -- unused

    item.slot        = unmarshallByte(th);

    item.orig_place  = unmarshallShort(th);
    item.orig_monnum = unmarshallShort(th);
    item.inscription = unmarshallString(th, 80);

    item.props.clear();
    item.props.read(th);
}

static void tag_construct_level_items(writer &th)
{
    // how many traps?
    marshallShort(th, MAX_TRAPS);
    for (int i = 0; i < MAX_TRAPS; ++i)
    {
        marshallByte(th, env.trap[i].type);
        marshallCoord(th, env.trap[i].pos);
        marshallShort(th, env.trap[i].ammo_qty);
    }

    // how many items?
    marshallShort(th, MAX_ITEMS);
    for (int i = 0; i < MAX_ITEMS; ++i)
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
    me.ench        = static_cast<enchant_type>( unmarshallShort(th) );
    me.degree      = unmarshallShort(th);
    me.who         = static_cast<kill_category>( unmarshallShort(th) );
    me.duration    = unmarshallShort(th);
    me.maxduration = unmarshallShort(th);
    return (me);
}

static void marshall_monster(writer &th, const monsters &m)
{
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

    // monster pathfinding (TAG_MINOR_PATHFIND)
    marshallShort(th, m.travel_path.size());
    for (unsigned int i = 0; i < m.travel_path.size(); i++)
        marshallCoord(th, m.travel_path[i]);

    marshallLong(th, m.flags);
    marshallLong(th, m.experience);

    marshallShort(th, m.enchantments.size());
    for (mon_enchant_list::const_iterator i = m.enchantments.begin();
         i != m.enchantments.end(); ++i)
    {
        marshall_mon_enchant(th, i->second);
    }
    marshallByte(th, m.ench_countdown);

    marshallShort(th, m.type);
    marshallShort(th, m.hit_points);
    marshallShort(th, m.max_hit_points);
    marshallShort(th, m.number);
    marshallShort(th, m.base_monster);
    marshallShort(th, m.colour);

    for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
        marshallShort(th, m.inv[j]);

    marshallSpells(th, m.spells);
    marshallByte(th, m.god);

    if (m.type == MONS_PLAYER_GHOST || m.type == MONS_PANDEMONIUM_DEMON)
    {
        // *Must* have ghost field set.
        ASSERT(m.ghost.get());
        marshallGhost(th, *m.ghost);
    }
}

static void tag_construct_level_monsters(writer &th)
{
    // how many mons_alloc?
    marshallByte(th, 20);
    for (int i = 0; i < 20; ++i)
        marshallShort(th, env.mons_alloc[i]);

    // how many monsters?
    marshallShort(th, MAX_MONSTERS);
    // how many monster inventory slots?
    marshallByte(th, NUM_MONSTER_SLOTS);

    for (int i = 0; i < MAX_MONSTERS; i++)
        marshall_monster(th, menv[i]);
}

void tag_construct_level_attitude(writer &th)
{
    int i;

    // how many monsters?
    marshallShort(th, MAX_MONSTERS);

    for (i = 0; i < MAX_MONSTERS; i++)
    {
        marshallByte(th, menv[i].attitude);
        marshallShort(th, menv[i].foe);
    }
}

void tag_construct_level_tiles(writer &th)
{
#ifdef USE_TILE
    unsigned short rle_count = 0; // for run-length encoding
    unsigned int tile = 0;
    unsigned int last_tile = 0;

    // tile routine subversion
    marshallShort(th, TILETAG_CURRENT);

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
                    marshallLong(th, last_tile);
                    marshallByte(th, (char)0xFF);
                    rle_count = 1;
                }
            }
            else
            {
                marshallLong(th, last_tile);
                // Note: the unsigned char tile count gets streamed
                // as a signed char here.  It gets read back into
                // an unsigned char in the read function.
                marshallByte(th, rle_count);
                rle_count = 1;
            }
        }

    marshallLong(th, tile);
    marshallByte(th, rle_count);

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
                    marshallLong(th, last_tile);
                    marshallByte(th, (char)0xFF);
                    rle_count = 1;
                }
            }
            else
            {
                marshallLong(th, last_tile);
                marshallByte(th, rle_count);
                rle_count = 1;
            }
        }

    marshallLong(th, tile);
    marshallByte(th, rle_count);

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
        }

    mcache.construct(th);

#endif
}

static void tag_read_level( reader &th, char minorVersion )
{

    env.floor_colour = unmarshallByte(th);
    env.rock_colour  = unmarshallByte(th);

    env.level_flags  = (unsigned long) unmarshallLong(th);

    env.elapsed_time = unmarshallFloat(th);

    // Map grids.
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);

    env.turns_on_level = unmarshallLong(th);

    for (int i = 0; i < gx; i++)
        for (int j = 0; j < gy; j++)
        {
            grd[i][j] =
                static_cast<dungeon_feature_type>(
                    static_cast<unsigned char>(unmarshallByte(th)) );

            env.map[i][j].object   = unmarshallShort(th);
            env.map[i][j].colour   = unmarshallShort(th);
            env.map[i][j].flags    = unmarshallShort(th);
            env.map[i][j].property = unmarshallLong(th);

            mgrd[i][j] = NON_MONSTER;
            env.cgrid[i][j] = (unsigned short) unmarshallShort(th);
        }

    env.grid_colours.init(BLACK);
    run_length_decode(th, unmarshallByte, env.grid_colours, GXM, GYM);

    env.cloud_no = unmarshallShort(th);

    // how many clouds?
    const int num_clouds = unmarshallShort(th);
    for (int i = 0; i < num_clouds; i++)
    {
        env.cloud[i].pos.x = unmarshallByte(th);
        env.cloud[i].pos.y = unmarshallByte(th);
        env.cloud[i].type  = static_cast<cloud_type>(unmarshallByte(th));
        env.cloud[i].decay = unmarshallShort(th);
        env.cloud[i].spread_rate = (unsigned char) unmarshallByte(th);
        env.cloud[i].whose = static_cast<kill_category>(unmarshallByte(th));
        env.cloud[i].killer = static_cast<killer_type>(unmarshallByte(th));
    }

    // how many shops?
    const int num_shops = unmarshallByte(th);
    ASSERT(num_shops <= MAX_SHOPS);
    for (int i = 0; i < num_shops; i++)
    {
        env.shop[i].keeper_name[0] = unmarshallByte(th);
        env.shop[i].keeper_name[1] = unmarshallByte(th);
        env.shop[i].keeper_name[2] = unmarshallByte(th);
        env.shop[i].pos.x = unmarshallByte(th);
        env.shop[i].pos.y = unmarshallByte(th);
        env.shop[i].greed = unmarshallByte(th);
        env.shop[i].type  = static_cast<shop_type>(unmarshallByte(th));
        env.shop[i].level = unmarshallByte(th);
    }

    unmarshallCoord(th, env.sanctuary_pos);
    env.sanctuary_time = unmarshallByte(th);

    env.spawn_random_rate = unmarshallLong(th);

    env.markers.read(th, minorVersion);

    env.properties.clear();
    env.properties.read(th);
}

static void tag_read_level_items(reader &th, char minorVersion)
{
    // how many traps?
    const int trap_count = unmarshallShort(th);
    for (int i = 0; i < trap_count; ++i)
    {
        env.trap[i].type =
            static_cast<trap_type>(
                static_cast<unsigned char>(unmarshallByte(th)) );
        unmarshallCoord(th, env.trap[i].pos);
        env.trap[i].ammo_qty = unmarshallShort(th);
    }

    // how many items?
    const int item_count = unmarshallShort(th);
    for (int i = 0; i < item_count; ++i)
        unmarshallItem(th, mitm[i]);

#if DEBUG_ITEM_SCAN
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

static void unmarshall_monster(reader &th, monsters &m)
{
    m.reset();

    if (_tag_minor_version >= TAG_MINOR_MONNAM)
        m.mname = unmarshallString(th, 100);

    m.ac              = unmarshallByte(th);
    m.ev              = unmarshallByte(th);
    m.hit_dice        = unmarshallByte(th);
    m.speed           = unmarshallByte(th);
    // Avoid sign extension when loading files (Elethiomel's hang)
    m.speed_increment = (unsigned char) unmarshallByte(th);
    m.behaviour       = static_cast<beh_type>(unmarshallByte(th));
    m.position.x      = unmarshallByte(th);
    m.position.y      = unmarshallByte(th);
    m.target.x        = unmarshallByte(th);
    m.target.y        = unmarshallByte(th);

    if (_tag_minor_version >= TAG_MINOR_MPATROL)
        unmarshallCoord(th, m.patrol_point);

    if (_tag_minor_version >= TAG_MINOR_TRTARGET)
    {
        int help = unmarshallByte(th);
        m.travel_target = static_cast<montravel_target_type>(help);
    }

    if (_tag_minor_version >= TAG_MINOR_PATHFIND)
    {
        const int len = unmarshallShort(th);
        for (int i = 0; i < len; ++i)
        {
            coord_def c;
            unmarshallCoord(th, c);
            m.travel_path.push_back(c);
        }
    }

    m.flags           = unmarshallLong(th);
    m.experience      = static_cast<unsigned long>(unmarshallLong(th));

    m.enchantments.clear();
    const int nenchs = unmarshallShort(th);
    for (int i = 0; i < nenchs; ++i)
    {
        mon_enchant me = unmarshall_mon_enchant(th);
        m.enchantments[me.ench] = me;
    }
    m.ench_countdown = unmarshallByte(th);

    m.type           = unmarshallShort(th);
    m.hit_points     = unmarshallShort(th);
    m.max_hit_points = unmarshallShort(th);
    m.number         = unmarshallShort(th);
    if (_tag_minor_version >= TAG_MINOR_MONBASE)
        m.base_monster = static_cast<monster_type>(unmarshallShort(th));
    else
        m.base_monster = static_cast<monster_type>(m.number);

    m.colour         = unmarshallShort(th);

    for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
        m.inv[j] = unmarshallShort(th);

    unmarshallSpells(th, m.spells);

    m.god = (god_type) unmarshallByte(th);

    if (m.type == MONS_PLAYER_GHOST || m.type == MONS_PANDEMONIUM_DEMON)
        m.set_ghost( unmarshallGhost(th) );

    m.check_speed();
}

static void tag_read_level_monsters(reader &th, char minorVersion)
{
    int i;
    int count, icount;

    for (i = 0; i < MAX_MONSTERS; i++)
        menv[i].reset();

    // how many mons_alloc?
    count = unmarshallByte(th);
    for (i = 0; i < count; ++i)
        env.mons_alloc[i] = static_cast<monster_type>( unmarshallShort(th) );

    // how many monsters?
    count = unmarshallShort(th);
    // how many monster inventory slots?
    icount = unmarshallByte(th);

    for (i = 0; i < count; i++)
    {
        monsters &m = menv[i];
        unmarshall_monster(th, m);
        // place monster
        if (m.type != -1)
        {
#if defined(DEBUG) || defined(DEBUG_MONS_SCAN)
            int midx = mgrd(m.pos());
            if (midx != NON_MONSTER)
                mprf(MSGCH_ERROR, "(%d,%d) for %s already occupied by %s",
                     m.pos().x, m.pos().y,
                     m.name(DESC_PLAIN, true).c_str(),
                     menv[midx].name(DESC_PLAIN, true).c_str());
#endif
            mgrd(m.pos()) = i;
        }
    }
}

void tag_read_level_attitude(reader &th)
{
    int i, count;

    // how many monsters?
    count = unmarshallShort(th);

    for (i = 0; i < count; i++)
    {
        menv[i].attitude = static_cast<mon_attitude_type>(unmarshallByte(th));
        menv[i].foe      = unmarshallShort(th);
    }
}

void tag_missing_level_attitude()
{
    // We don't really have to do a lot here, just set foe to MHITNOT;
    // they'll pick up a foe first time through handle_monster() if
    // there's one around.

    // As for attitude, a couple simple checks can be used to determine
    // friendly/good neutral/neutral/hostile.
    int i;
    bool is_friendly;
    unsigned int new_beh = BEH_WANDER;

    for (i = 0; i < MAX_MONSTERS; i++)
    {
        // only do actual monsters
        if (menv[i].type < 0)
            continue;

        is_friendly = testbits(menv[i].flags, MF_CREATED_FRIENDLY);

        menv[i].foe = MHITNOT;

        switch(menv[i].behaviour)
        {
            case 0:         // old BEH_SLEEP
                new_beh = BEH_SLEEP;    // don't wake sleepers
                break;
            case 3:         // old BEH_FLEE
            case 10:        // old BEH_FLEE_FRIEND
                new_beh = BEH_FLEE;
                break;
            case 1:         // old BEH_CHASING_I
            case 6:         // old BEH_FIGHT
                new_beh = BEH_SEEK;
                break;
            case 7:         // old BEH_ENSLAVED
                if (!menv[i].has_ench(ENCH_CHARM))
                    is_friendly = true;
                break;
            default:
                break;
        }

        menv[i].attitude = (is_friendly) ? ATT_FRIENDLY : ATT_HOSTILE;
        menv[i].behaviour = static_cast<beh_type>(new_beh);
        menv[i].foe_memory = 0;
    }
}

void tag_read_level_tiles(struct reader &th)
{
#ifdef USE_TILE
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
        {
            env.tile_bk_bg[i][j] = 0;
            env.tile_bk_fg[i][j] = 0;
        }

    unsigned char rle_count = 0;
    unsigned int tile = 0;

    int ver = unmarshallShort(th);
    if (ver == 0) return;

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
                tile      = unmarshallLong(th);
                rle_count = unmarshallByte(th);
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
                tile      = unmarshallLong(th);
                rle_count = unmarshallByte(th);
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
        }

    if (ver > TILETAG_PRE_MCACHE)
        mcache.read(th);
    else
        mcache.clear_all();

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
            unsigned int fg, bg;
            tileidx_unseen(fg, bg, get_envmap_char(i, j), gc);
            env.tile_bk_fg[i][j] = fg;
            env.tile_bk_bg[i][j] = bg;
        }

    mcache.clear_all();

    TileNewLevel(true);
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
    marshallByte(th, res.pierce);
    marshallByte(th, res.slice);
    marshallByte(th, res.bludgeon);
}

static void unmarshallResists(reader &th, mon_resist_def &res)
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
        spells[j] = static_cast<spell_type>( unmarshallShort(th) );
}

static void marshallGhost(writer &th, const ghost_demon &ghost)
{
    marshallString(th, ghost.name.c_str(), 20);

    marshallShort(th, ghost.species);
    marshallShort(th, ghost.job);
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

    marshallResists(th, ghost.resists);

    marshallByte(th, ghost.spellcaster);
    marshallByte(th, ghost.cycle_colours);
    marshallShort(th, ghost.fly);

    marshallSpells(th, ghost.spells);
}

static ghost_demon unmarshallGhost( reader &th )
{
    ghost_demon ghost;

    ghost.name             = unmarshallString(th, 20);

    ghost.species          = static_cast<species_type>( unmarshallShort(th) );
    ghost.job              = static_cast<job_type>( unmarshallShort(th) );
    ghost.best_skill       = static_cast<skill_type>( unmarshallShort(th) );
    ghost.best_skill_level = unmarshallShort(th);
    ghost.xl               = unmarshallShort(th);
    ghost.max_hp           = unmarshallShort(th);
    ghost.ev               = unmarshallShort(th);
    ghost.ac               = unmarshallShort(th);
    ghost.damage           = unmarshallShort(th);
    ghost.speed            = unmarshallShort(th);
    ghost.see_invis        = unmarshallByte(th);
    ghost.brand            = static_cast<brand_type>( unmarshallShort(th) );

    unmarshallResists(th, ghost.resists);

    ghost.spellcaster      = unmarshallByte(th);
    ghost.cycle_colours    = unmarshallByte(th);
    ghost.fly              = static_cast<flight_type>( unmarshallShort(th) );

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

static void tag_read_ghost(reader &th, char minorVersion)
{
    int nghosts = unmarshallShort(th);

    if (nghosts < 1 || nghosts > MAX_GHOSTS)
        return;

    for (int i = 0; i < nghosts; ++i)
        ghosts.push_back( unmarshallGhost(th) );
}
