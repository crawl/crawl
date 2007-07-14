/*
 *  File:       tags.cc
 *  Summary:    Auxilary functions to make savefile versioning simpler.
 *  Written by: Gordon Lipford
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <2>   16 Mar 2001      GDL    Added TAG_LEVEL_ATTITUDE
 *   <1>   27 Jan 2001      GDL    Created
 */

/* ------------------------- how tags work ----------------------------------

1. Tag types are enumerated in enum.h, from TAG_VERSION (more a placeholder
   than anything else, it is not actually saved as a tag) to TAG_XXX. NUM_TAGS
   is equal to the actual number of defined tags.

2. Tags are created with tag_construct(), which forwards the construction
   request appropriately.   tag_write() is then used to write the tag to an
   output stream.

3. Tags are parsed with tag_read(), which tries to read a tag header and then
   forwards the request appropriately, returning the ID of the tag it found,
   or zero if no tag was found.

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
   FTP in binary mode.  Note also that the marshalling sizes are 1,2, and 4
   for byte, short, and long - assuming that 'int' would have been
   insufficient on 16 bit systems (and that Crawl otherwise lacks
   system-indepedent data types).

*/

#include <cstdlib>
#include <stdio.h>
#include <string.h>            // for memcpy
#include <iterator>

#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "AppHdr.h"

#include "abl-show.h"
#include "branch.h"
#include "dungeon.h"
#include "enum.h"
#include "externs.h"
#include "files.h"
#include "itemname.h"
#include "itemprop.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mtransit.h"
#include "randart.h"
#include "skills.h"
#include "skills2.h"
#include "stuff.h"
#include "tags.h"
#include "travel.h"

// THE BIG IMPORTANT TAG CONSTRUCTION/PARSE BUFFER
static char *tagBuffer = NULL;

// defined in overmap.cc
extern std::map<branch_type, level_id> stair_level;
extern std::map<level_pos, shop_type> shops_present;
extern std::map<level_pos, god_type> altars_present;
extern std::map<level_pos, portal_type> portals_present;

// temp file pairs used for file level cleanup
FixedArray < bool, MAX_LEVELS, NUM_BRANCHES > tmp_file_pairs;

// static helpers
static void tag_construct_you(struct tagHeader &th);
static void tag_construct_you_items(struct tagHeader &th);
static void tag_construct_you_dungeon(struct tagHeader &th);
static void tag_construct_lost_monsters(tagHeader &th);
static void tag_read_you(struct tagHeader &th, char minorVersion);
static void tag_read_you_items(struct tagHeader &th, char minorVersion);
static void tag_read_you_dungeon(struct tagHeader &th);
static void tag_read_lost_monsters(tagHeader &th, int minorVersion);

static void tag_construct_level(struct tagHeader &th);
static void tag_construct_level_items(struct tagHeader &th);
static void tag_construct_level_monsters(struct tagHeader &th);
static void tag_construct_level_attitude(struct tagHeader &th);
static void tag_read_level(struct tagHeader &th, char minorVersion);
static void tag_read_level_items(struct tagHeader &th, char minorVersion);
static void tag_read_level_monsters(struct tagHeader &th, char minorVersion);
static void tag_read_level_attitude(struct tagHeader &th);
static void tag_missing_level_attitude();

static void tag_construct_ghost(struct tagHeader &th);
static void tag_read_ghost(struct tagHeader &th, char minorVersion);

static void marshallGhost(tagHeader &th, const ghost_demon &ghost);
static ghost_demon unmarshallGhost( tagHeader &th );
static void marshall_monster(tagHeader &th, const monsters &m);
static void unmarshall_monster(tagHeader &th, monsters &m);
static void marshall_item(tagHeader &th, const item_def &item);
static void unmarshall_item(tagHeader &th, item_def &item);

template<typename T, typename T_iter>
static void marshall_iterator(struct tagHeader &th, T_iter beg, T_iter end,
                              void (*T_marshall)(struct tagHeader&, const T&));
template<typename T>
static void unmarshall_vector(struct tagHeader& th, std::vector<T>& vec,
                              T (*T_unmarshall)(struct tagHeader&));

// provide a wrapper for file writing, just in case.
int write2(FILE * file, const char *buffer, unsigned int count)
{
    return fwrite(buffer, 1, count, file);
}

// provide a wrapper for file reading, just in case.
int read2(FILE * file, char *buffer, unsigned int count)
{
    return fread(buffer, 1, count, file);
}

void marshallByte(struct tagHeader &th, char data)
{
    tagBuffer[th.offset] = data;
    th.offset += 1;
}

char unmarshallByte(struct tagHeader &th)
{
    char data = tagBuffer[th.offset];
    th.offset += 1;
    return data;
}

// marshall 2 byte short in network order
void marshallShort(struct tagHeader &th, short data)
{
    char b2 = (char)(data & 0x00FF);
    char b1 = (char)((data & 0xFF00) >> 8);

    tagBuffer[th.offset] = b1;
    tagBuffer[th.offset + 1] = b2;

    th.offset += 2;
}

// unmarshall 2 byte short in network order
short unmarshallShort(struct tagHeader &th)
{
    short b1 = tagBuffer[th.offset];
    short b2 = tagBuffer[th.offset + 1];

    short data = (b1 << 8) | (b2 & 0x00FF);

    th.offset += 2;
    return data;
}

// marshall 4 byte int in network order
void marshallLong(struct tagHeader &th, long data)
{
    char b4 = (char) (data & 0x000000FF);
    char b3 = (char)((data & 0x0000FF00) >> 8);
    char b2 = (char)((data & 0x00FF0000) >> 16);
    char b1 = (char)((data & 0xFF000000) >> 24);

    tagBuffer[th.offset] = b1;
    tagBuffer[th.offset + 1] = b2;
    tagBuffer[th.offset + 2] = b3;
    tagBuffer[th.offset + 3] = b4;

    th.offset += 4;
}

// unmarshall 4 byte int in network order
long unmarshallLong(struct tagHeader &th)
{
    long b1 = tagBuffer[th.offset];
    long b2 = tagBuffer[th.offset + 1];
    long b3 = tagBuffer[th.offset + 2];
    long b4 = tagBuffer[th.offset + 3];

    long data = (b1 << 24) | ((b2 & 0x000000FF) << 16);
    data |= ((b3 & 0x000000FF) << 8) | (b4 & 0x000000FF);

    th.offset += 4;
    return data;
}

template<typename T>
void marshall_as_long(struct tagHeader& th, const T& t)
{
    marshallLong( th, static_cast<long>(t) );
}

template<typename key, typename value>
void marshallMap(struct tagHeader &th, const std::map<key,value>& data,
                 void (*key_marshall)(struct tagHeader&, const key&),
                 void (*value_marshall)(struct tagHeader&, const value&))
{
    marshallLong( th, data.size() );
    typename std::map<key,value>::const_iterator ci;
    for ( ci = data.begin(); ci != data.end(); ++ci )
    {
        key_marshall(th, ci->first);
        value_marshall(th, ci->second);
    }
}

template<typename T, typename T_iter>
static void marshall_iterator(struct tagHeader &th, T_iter beg, T_iter end,
                              void (*T_marshall)(struct tagHeader&, const T&))
{
    marshallLong(th, std::distance(beg, end));
    while ( beg != end )
    {
        T_marshall(th, *beg);
        ++beg;
    }
}

template<typename T>
static void unmarshall_vector(struct tagHeader& th, std::vector<T>& vec,
                              T (*T_unmarshall)(struct tagHeader&))
{
    vec.clear();
    const long num_to_read = unmarshallLong(th);
    for ( long i = 0; i < num_to_read; ++i )
        vec.push_back( T_unmarshall(th) );
}

void marshall_level_id( tagHeader& th, const level_id& id )
{
    marshallByte(th, id.branch );
    marshallLong(th, id.depth );
    marshallByte(th, id.level_type);
}

void marshall_level_pos( struct tagHeader& th, const level_pos& lpos )
{
    marshallLong(th, lpos.pos.x);
    marshallLong(th, lpos.pos.y);
    marshall_level_id(th, lpos.id);
}

template<typename key, typename value, typename map>
void unmarshallMap(struct tagHeader& th, map& data,
                   key   (*key_unmarshall)  (tagHeader&),
                   value (*value_unmarshall)(tagHeader&) )
{
    long i, len = unmarshallLong(th);
    key k;
    for ( i = 0; i < len; ++i )
    {
        k = key_unmarshall(th);
        std::pair<key, value> p(k, value_unmarshall(th));
        data.insert(p);
    }
}

template<typename T>
T unmarshall_long_as( struct tagHeader& th )
{
    return static_cast<T>(unmarshallLong(th));
}

level_id unmarshall_level_id( tagHeader& th )
{
    level_id id;
    id.branch = static_cast<branch_type>(unmarshallByte(th));
    id.depth = unmarshallLong(th);
    id.level_type = static_cast<level_area_type>(unmarshallByte(th));
    return (id);
}

level_pos unmarshall_level_pos( struct tagHeader& th )
{
    level_pos lpos;
    lpos.pos.x = unmarshallLong(th);
    lpos.pos.y = unmarshallLong(th);
    lpos.id    = unmarshall_level_id(th);
    return lpos;
}

void marshallCoord(tagHeader &th, const coord_def &c)
{
    marshallShort(th, c.x);
    marshallShort(th, c.y);
}

void unmarshallCoord(tagHeader &th, coord_def &c)
{
    c.x = unmarshallShort(th);
    c.y = unmarshallShort(th);
}

union float_marshall_kludge
{
    // [ds] Does ANSI C guarantee that sizeof(float) == sizeof(long)?
    // [haranp] no, unfortunately
    float f_num;
    long  l_num;
};

// single precision float -- marshall in network order.
void marshallFloat(struct tagHeader &th, float data)
{
    float_marshall_kludge k;
    k.f_num = data;
    marshallLong(th, k.l_num);
}

// single precision float -- unmarshall in network order.
float unmarshallFloat(struct tagHeader &th)
{
    float_marshall_kludge k;
    k.l_num = unmarshallLong(th);
    return k.f_num;
}

// string -- marshall length & string data
void marshallString(struct tagHeader &th, const char *data, int maxSize)
{
    // allow for very long strings.
    short len = strlen(data);
    if (maxSize > 0 && len > maxSize)
        len = maxSize;
    marshallShort(th, len);

    // put in the actual string -- we'll null terminate on
    // unmarshall.
    memcpy(&tagBuffer[th.offset], data, len);
    th.offset += len;
}

// string -- unmarshall length & string data
void unmarshallString(struct tagHeader &th, char *data, int maxSize)
{
    // get length
    short len = unmarshallShort(th);
    int copylen = len;

    if (len >= maxSize && maxSize > 0)
        copylen = maxSize - 1;

    // read the actual string and null terminate
    memcpy(data, &tagBuffer[th.offset], copylen);
    data[copylen] = 0;

    th.offset += len;
}

std::string unmarshallString(tagHeader &th, int maxSize)
{
    if (maxSize <= 0)
        return ("");    
    char *buffer = new char [maxSize];
    if (!buffer)
        return ("");    
    *buffer = 0;
    unmarshallString(th, buffer, maxSize);
    const std::string res = buffer;
    delete [] buffer;

    return (res);
}

// boolean (to avoid system-dependant bool implementations)
void marshallBoolean(struct tagHeader &th, bool data)
{
    char charRep = 0;       // for false
    if (data)
        charRep = 1;

    tagBuffer[th.offset] = charRep;
    th.offset += 1;
}

// boolean (to avoid system-dependant bool implementations)
bool unmarshallBoolean(struct tagHeader &th)
{
    bool data;

    if (tagBuffer[th.offset] == 1)
        data = true;
    else
        data = false;

    th.offset += 1;
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

    struct tm *date = localtime( &in_date );

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

// PUBLIC TAG FUNCTIONS
void tag_init(long largest_tag)
{
    if (tagBuffer != NULL)
        return;

    tagBuffer = (char *)malloc(largest_tag);
}

void tag_construct(struct tagHeader &th, int tagID)
{
    th.offset = 0;
    th.tagID = tagID;

    switch(tagID)
    {
        case TAG_YOU:
            tag_construct_you(th);
            break;
        case TAG_YOU_ITEMS:
            tag_construct_you_items(th);
            break;
        case TAG_YOU_DUNGEON:
            tag_construct_you_dungeon(th);
            break;
        case TAG_LEVEL:
            tag_construct_level(th);
            break;
        case TAG_LEVEL_ITEMS:
            tag_construct_level_items(th);
            break;
        case TAG_LEVEL_MONSTERS:
            tag_construct_level_monsters(th);
            break;
        case TAG_LEVEL_ATTITUDE:
            tag_construct_level_attitude(th);
            break;
        case TAG_GHOST:
            tag_construct_ghost(th);
            break;
        case TAG_LOST_MONSTERS:
            tag_construct_lost_monsters(th);
            break;
        default:
            // I don't know how to make that!
            break;
    }
}

void tag_write(struct tagHeader &th, FILE *saveFile)
{
    const int tagHdrSize = 6;
    int tagSize=0;

    char swap[tagHdrSize];

    // make sure there is some data to write!
    if (th.offset == 0)
        return;

    // special case: TAG_VERSION.  Skip tag header.
    if (th.tagID != TAG_VERSION)
    {
        // swap out first few bytes
        memcpy(swap, tagBuffer, tagHdrSize);

        // save tag size
        tagSize = th.offset;

        // swap in the header
        th.offset = 0;
        marshallShort(th, th.tagID);
        marshallLong(th, tagSize);

        // write header
        write2(saveFile, tagBuffer, th.offset);

        // swap real data back in
        memcpy(tagBuffer, swap, tagHdrSize);

        // reset tag size
        th.offset = tagSize;
    }

    // write tag data
    write2(saveFile, tagBuffer, th.offset);
    return;
}

// minorVersion is available for any sub-readers that need it
// (like TAG_LEVEL_MONSTERS)
int tag_read(FILE *fp, char minorVersion)
{
    const int tagHdrSize = 6;
    struct tagHeader hdr, th;
    th.offset = 0;

    // read tag header
    if (read2(fp, tagBuffer, tagHdrSize) != tagHdrSize)
        return 0;

    // unmarshall tag type and length (not including header)
    hdr.tagID = unmarshallShort(th);
    hdr.offset = unmarshallLong(th);

    // sanity check
    if (hdr.tagID <= 0 || hdr.offset <= 0)
        return 0;

    // now reset th and read actual data
    th.offset = 0;
    if (read2(fp, tagBuffer, hdr.offset) != hdr.offset)
        return 0;

    // ok, we have data now.
    switch(hdr.tagID)
    {
        case TAG_YOU:
            tag_read_you(th, minorVersion);
            break;
        case TAG_YOU_ITEMS:
            tag_read_you_items(th, minorVersion);
            break;
        case TAG_YOU_DUNGEON:
            tag_read_you_dungeon(th);
            break;
        case TAG_LEVEL:
            tag_read_level(th, minorVersion);
            break;
        case TAG_LEVEL_ITEMS:
            tag_read_level_items(th, minorVersion);
            break;
        case TAG_LEVEL_MONSTERS:
            tag_read_level_monsters(th, minorVersion);
            break;
        case TAG_LEVEL_ATTITUDE:
            tag_read_level_attitude(th);
            break;
        case TAG_GHOST:
            tag_read_ghost(th, minorVersion);
            break;
        case TAG_LOST_MONSTERS:
            tag_read_lost_monsters(th, minorVersion);
            break;
        default:
            // I don't know how to read that!
            return 0;
    }

    return hdr.tagID;
}


// older savefiles might want to call this to get a tag
// properly initialized if it wasn't part of the savefile.
// For now, none are supported.

// This function will be called AFTER all other tags for
// the savefile are read, so everything that can be
// initialized should have been by now.

// minorVersion is available for any child functions that need
// it (currently none)
void tag_missing(int tag, char minorVersion)
{
    UNUSED( minorVersion );

    switch(tag)
    {
        case TAG_LEVEL_ATTITUDE:
            tag_missing_level_attitude();
            break;
        default:
            perror("Tag %d is missing;  file is likely corrupt.");
            end(-1);
    }
}

// utility
void tag_set_expected(char tags[], int fileType)
{
    int i;

    for(i=0; i<NUM_TAGS; i++)
    {
        tags[i] = -1;
        switch(fileType)
        {
            case TAGTYPE_PLAYER:
                if ((i >= TAG_YOU && i <=TAG_YOU_DUNGEON)
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
                break;
            case TAGTYPE_GHOST:
                if (i == TAG_GHOST)
                    tags[i] = 1;
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
static void tag_construct_you(struct tagHeader &th)
{
    int i,j;

    marshallString(th, you.your_name, 30);

    marshallByte(th,you.religion);
    marshallByte(th,you.piety);
    marshallByte(th,you.rotting);
    marshallByte(th,you.symbol);
    marshallByte(th,you.colour);
    marshallByte(th,you.pet_target);

    marshallByte(th,you.max_level);
    marshallByte(th,you.where_are_you);
    marshallByte(th,you.char_direction);
    marshallByte(th,you.your_level);
    marshallByte(th,you.is_undead);
    marshallByte(th,you.special_wield);
    marshallByte(th,you.berserk_penalty);
    marshallByte(th,you.level_type);
    marshallByte(th,you.synch_time);
    marshallByte(th,you.disease);
    marshallByte(th,you.species);

    marshallShort(th, you.hp);

    marshallShort(th, you.hunger);

    // how many you.equip?
    marshallByte(th, NUM_EQUIP);
    for (i = 0; i < NUM_EQUIP; ++i)
        marshallByte(th,you.equip[i]);

    marshallByte(th,you.magic_points);
    marshallByte(th,you.max_magic_points);
    marshallByte(th,you.strength);
    marshallByte(th,you.intel);
    marshallByte(th,you.dex);
    marshallByte(th,you.hit_points_regeneration);
    marshallByte(th,you.magic_points_regeneration);

    marshallShort(th, you.hit_points_regeneration * 100);
    marshallLong(th, you.experience);
    marshallLong(th, you.gold);

    marshallByte(th,you.char_class);
    marshallByte(th,you.experience_level);
    marshallLong(th, you.exp_available);

    /* max values */
    marshallByte(th,you.max_strength);
    marshallByte(th,you.max_intel);
    marshallByte(th,you.max_dex);

    marshallShort(th, you.base_hp);
    marshallShort(th, you.base_hp2);
    marshallShort(th, you.base_magic_points);
    marshallShort(th, you.base_magic_points2);

    marshallShort(th, you.x_pos);
    marshallShort(th, you.y_pos);

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
        marshallByte(th,you.skills[j]);   /* skills! */
        marshallByte(th,you.practise_skill[j]);   /* skills! */
        marshallLong(th,you.skill_points[j]);
        marshallByte(th,you.skill_order[j]);   /* skills ordering */
    }

    // how many durations?
    marshallByte(th, NUM_DURATIONS);
    for (j = 0; j < NUM_DURATIONS; ++j)
        marshallLong(th,you.duration[j]);

    // how many attributes?
    marshallByte(th, NUM_ATTRIBUTES);
    for (j = 0; j < NUM_ATTRIBUTES; ++j)
        marshallByte(th,you.attribute[j]);

    // sacrifice values
    marshallByte(th, NUM_OBJECT_CLASSES);
    for (j = 0; j < NUM_OBJECT_CLASSES; ++j)
        marshallLong(th, you.sacrifice_value[j]);

    // how many mutations/demon powers?
    marshallShort(th, NUM_MUTATIONS);
    for (j = 0; j < NUM_MUTATIONS; ++j)
    {
        marshallByte(th,you.mutation[j]);
        marshallByte(th,you.demon_pow[j]);
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

    // real_time == -1 means game was started before this feature
    if (you.real_time != -1)
    {
        const time_t now = time(NULL);
        you.real_time += (now - you.start_time);

        // Reset start_time now that real_time is being saved out...
        // this may just be a level save.
        you.start_time = now;
    }

    marshallLong( th, you.real_time );
    marshallLong( th, you.num_turns );

    marshallShort(th, you.magic_contamination);

    marshallShort(th, you.transit_stair);
    marshallByte(th, you.entering_level);
}

static void tag_construct_you_items(struct tagHeader &th)
{
    int i,j;

    // how many inventory slots?
    marshallByte(th, ENDOFPACK);
    for (i = 0; i < ENDOFPACK; ++i)
    {
        marshallByte(th,you.inv[i].base_type);
        marshallByte(th,you.inv[i].sub_type);
        marshallShort(th,you.inv[i].plus);
        marshallLong(th,you.inv[i].special);
        marshallByte(th,you.inv[i].colour);
        marshallLong(th,you.inv[i].flags);
        marshallShort(th,you.inv[i].quantity);
        marshallShort(th,you.inv[i].plus2);
        marshallShort(th, you.inv[i].orig_place);
        marshallShort(th, you.inv[i].orig_monnum);
        marshallString(th, you.inv[i].inscription.c_str(), 80);
    }

    marshallByte(th, you.quiver);

    // item descrip for each type & subtype
    // how many types?
    marshallByte(th, 5);
    // how many subtypes?
    marshallByte(th, 50);
    for (i = 0; i < 5; ++i)
    {
        for (j = 0; j < 50; ++j)
            marshallByte(th, you.item_description[i][j]);
    }

    // identification status
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

    marshallByte(th, NUM_BOOKS);
    for (j = 0; j < NUM_BOOKS; ++j)
        marshallByte(th,you.had_book[j]);

    // how many unrandarts?
    marshallShort(th, NO_UNRANDARTS);

    for (j = 0; j < NO_UNRANDARTS; ++j)
        marshallBoolean(th, does_unrandart_exist(j));
}

static void tag_construct_you_dungeon(struct tagHeader &th)
{
    int i,j;

    // how many unique creatures?
    marshallShort(th, NUM_MONSTERS);
    for (j = 0; j < NUM_MONSTERS; ++j)
        marshallByte(th,you.unique_creatures[j]); /* unique beasties */

    // how many branches?
    marshallByte(th, NUM_BRANCHES);
    for (j = 0; j < NUM_BRANCHES; ++j)
        marshallLong(th, branches[j].startdepth);

    marshallShort(th, MAX_LEVELS);
    for (i = 0; i < MAX_LEVELS; ++i)
        for (j = 0; j < NUM_BRANCHES; ++j)
            marshallBoolean(th, tmp_file_pairs[i][j]);

    marshallMap(th, stair_level,
                marshall_as_long<branch_type>, marshall_level_id);
    marshallMap(th, shops_present,
                marshall_level_pos, marshall_as_long<shop_type>);
    marshallMap(th, altars_present,
                marshall_level_pos, marshall_as_long<god_type>);
    marshallMap(th, portals_present,
                marshall_level_pos, marshall_as_long<portal_type>);
}

static void marshall_follower(tagHeader &th, const follower &f)
{
    marshall_monster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        marshall_item(th, f.items[i]);    
}

static void unmarshall_follower(tagHeader &th, follower &f)
{
    unmarshall_monster(th, f.mons);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        unmarshall_item(th, f.items[i]);
}

static void marshall_follower_list(tagHeader &th, const m_transit_list &mlist)
{
    marshallShort( th, mlist.size() );
    
    for (m_transit_list::const_iterator mi = mlist.begin();
         mi != mlist.end(); ++mi)
    {
        marshall_follower( th, *mi );
    }
}

static m_transit_list unmarshall_follower_list(tagHeader &th)
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

static void tag_construct_lost_monsters(tagHeader &th)
{
    marshallMap( th, the_lost_ones, marshall_level_id,
                 marshall_follower_list );
}

static void tag_read_you(struct tagHeader &th, char minorVersion)
{
    char buff[20];      // For birth date
    int i,j;
    char count_c;
    short count_s;

    unmarshallString(th, you.your_name, 30);

    you.religion = static_cast<god_type>(unmarshallByte(th));
    you.piety = unmarshallByte(th);
    you.rotting = unmarshallByte(th);
    you.symbol = unmarshallByte(th);
    you.colour = unmarshallByte(th);
    you.pet_target = unmarshallByte(th);

    you.max_level = unmarshallByte(th);
    you.where_are_you = static_cast<branch_type>( unmarshallByte(th) );
    you.char_direction = static_cast<game_direction_type>(unmarshallByte(th));
    you.your_level = unmarshallByte(th);
    you.is_undead = static_cast<undead_state_type>(unmarshallByte(th));
    you.special_wield = unmarshallByte(th);
    you.berserk_penalty = unmarshallByte(th);
    you.level_type = static_cast<level_area_type>( unmarshallByte(th) );
    you.synch_time = unmarshallByte(th);
    you.disease = unmarshallByte(th);
    you.species = static_cast<species_type>(unmarshallByte(th));
    you.hp = unmarshallShort(th);
    you.hunger = unmarshallShort(th);

    // how many you.equip?
    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; ++i)
        you.equip[i] = unmarshallByte(th);

    you.magic_points = unmarshallByte(th);
    you.max_magic_points = unmarshallByte(th);
    you.strength = unmarshallByte(th);
    you.intel = unmarshallByte(th);
    you.dex = unmarshallByte(th);
    you.hit_points_regeneration = unmarshallByte(th);
    you.magic_points_regeneration = unmarshallByte(th);

    you.hit_points_regeneration = unmarshallShort(th) / 100;
    you.experience = unmarshallLong(th);
    you.gold = unmarshallLong(th);

    you.char_class = static_cast<job_type>(unmarshallByte(th));
    you.experience_level = unmarshallByte(th);
    you.exp_available = unmarshallLong(th);

    /* max values */
    you.max_strength = unmarshallByte(th);
    you.max_intel = unmarshallByte(th);
    you.max_dex = unmarshallByte(th);

    you.base_hp = unmarshallShort(th);
    you.base_hp2 = unmarshallShort(th);
    you.base_magic_points = unmarshallShort(th);
    you.base_magic_points2 = unmarshallShort(th);

    const int x = unmarshallShort(th);
    const int y = unmarshallShort(th);
    you.moveto(x, y);

    unmarshallString(th, you.class_name, 30);

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
        you.ability_letter_table[i] =
            static_cast<ability_type>(unmarshallShort(th));

    // how many skills?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
    {
        you.skills[j] = unmarshallByte(th);
        you.practise_skill[j] = unmarshallByte(th);
        you.skill_points[j] = unmarshallLong(th);
        you.skill_order[j] = unmarshallByte(th);
    }

    // set up you.total_skill_points and you.skill_cost_level
    calc_total_skill_points();

    // how many durations?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        you.duration[j] = unmarshallLong(th);

    // how many attributes?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        you.attribute[j] = unmarshallByte(th);

    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        you.sacrifice_value[j] = unmarshallLong(th);

    // how many mutations/demon powers?
    count_s = unmarshallShort(th);
    for (j = 0; j < count_s; ++j)
    {
        you.mutation[j] = unmarshallByte(th);
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

    you.gift_timeout = unmarshallByte(th);
    you.normal_vision = unmarshallByte(th);
    you.current_vision = unmarshallByte(th);
    you.hell_exit = unmarshallByte(th);

    // elapsed time
    you.elapsed_time = (double)unmarshallFloat(th);

    // wizard mode
    you.wizard = (bool) unmarshallByte(th);

    // time of character creation
    unmarshallString( th, buff, 20 );
    you.birth_time = parse_date_string( buff );

    you.real_time = unmarshallLong(th);
    you.num_turns = unmarshallLong(th);

    you.magic_contamination = unmarshallShort(th);

    you.transit_stair  = static_cast<dungeon_feature_type>(unmarshallShort(th));
    you.entering_level = unmarshallByte(th);
}

static void tag_read_you_items(struct tagHeader &th, char minorVersion)
{
    int i,j;
    char count_c, count_c2;
    short count_s;

    // how many inventory slots?
    count_c = unmarshallByte(th);
    for (i = 0; i < count_c; ++i)
    {
        you.inv[i].base_type =
            static_cast<object_class_type>(unmarshallByte(th));
        you.inv[i].sub_type = (unsigned char) unmarshallByte(th);
        you.inv[i].plus = unmarshallShort(th);
        you.inv[i].special = unmarshallLong(th);
        you.inv[i].colour = (unsigned char) unmarshallByte(th);
        you.inv[i].flags = (unsigned long) unmarshallLong(th);
        you.inv[i].quantity = unmarshallShort(th);
        you.inv[i].plus2 = unmarshallShort(th);       
        you.inv[i].orig_place  = unmarshallShort(th);
        you.inv[i].orig_monnum = unmarshallShort(th);
        you.inv[i].inscription = unmarshallString(th, 80);

        // these never need to be saved for items in the inventory -- bwr
        you.inv[i].x = -1;
        you.inv[i].y = -1;
        you.inv[i].link = i;
        you.inv[i].slot = index_to_letter(i);
    }

    you.quiver = unmarshallByte(th);

    // item descrip for each type & subtype
    // how many types?
    count_c = unmarshallByte(th);
    // how many subtypes?
    count_c2 = unmarshallByte(th);
    for (i = 0; i < count_c; ++i)
        for (j = 0; j < count_c2; ++j)
            you.item_description[i][j] = unmarshallByte(th);

    // identification status
    // how many types?
    count_c = unmarshallByte(th);
    // how many subtypes?
    count_c2 = unmarshallByte(th);

    // argh.. this is awful.
    for (i = 0; i < count_c; ++i)
    {
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
            }
        }
    }

    // how many unique items?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        you.unique_items[j] =
            static_cast<unique_item_status_type>(unmarshallByte(th));

    // how many books?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        you.had_book[j] = unmarshallByte(th);

    // how many unrandarts?
    count_s = unmarshallShort(th);
    for (j = 0; j < count_s; ++j)
        set_unrandart_exist(j, unmarshallBoolean(th));

    // # of unrandarts could certainly change.  If it does,
    // the new ones won't exist yet - zero them out.
    for (; j < NO_UNRANDARTS; j++)
        set_unrandart_exist(j, 0);
}

static void tag_read_you_dungeon(struct tagHeader &th)
{
    int i,j;
    int count_c;
    short count_s;

    // how many unique creatures?
    count_c = unmarshallShort(th);
    you.unique_creatures.init(false);
    for (j = 0; j < count_c; ++j)
    {
        const bool created = static_cast<bool>(unmarshallByte(th));

        if (j < NUM_MONSTERS)
            you.unique_creatures[j] = created;
    }

    // how many branches?
    count_c = unmarshallByte(th);
    for (j = 0; j < count_c; ++j)
        branches[j].startdepth = unmarshallLong(th);

    count_s = unmarshallShort(th);
    for (i = 0; i < count_s; ++i)
    {
        for (j = 0; j < count_c; ++j)
            tmp_file_pairs[i][j] = unmarshallBoolean(th);
    }
    
    unmarshallMap(th, stair_level,
                  unmarshall_long_as<branch_type>,
                  unmarshall_level_id);
    unmarshallMap(th, shops_present,
                  unmarshall_level_pos, unmarshall_long_as<shop_type>);
    unmarshallMap(th, altars_present,
                  unmarshall_level_pos, unmarshall_long_as<god_type>);
    unmarshallMap(th, portals_present,
                  unmarshall_level_pos, unmarshall_long_as<portal_type>);
}

static void tag_read_lost_monsters(tagHeader &th, int minorVersion)
{
    the_lost_ones.clear();
    
    unmarshallMap(th, the_lost_ones,
                  unmarshall_level_id, unmarshall_follower_list);
}

// ------------------------------- level tags ---------------------------- //

static void tag_construct_level(struct tagHeader &th)
{
    marshallFloat(th, (float)you.elapsed_time);

    // map grids
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    marshallLong(th, env.turns_on_level);

    for (int count_x = 0; count_x < GXM; count_x++)
    {
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallByte(th, grd[count_x][count_y]);
            marshallShort(th, env.map[count_x][count_y].object);
            marshallShort(th, env.map[count_x][count_y].colour);            
            marshallShort(th, env.map[count_x][count_y].flags);
            marshallByte(th, env.cgrid[count_x][count_y]);
        }
    }

    marshallShort(th, env.cloud_no);

    // how many clouds?
    marshallShort(th, MAX_CLOUDS);
    for (int i = 0; i < MAX_CLOUDS; i++)
    {
        marshallByte(th, env.cloud[i].x);
        marshallByte(th, env.cloud[i].y);
        marshallByte(th, env.cloud[i].type);
        marshallShort(th, env.cloud[i].decay);
        marshallShort(th, env.cloud[i].whose);
    }

    // how many shops?
    marshallByte(th, 5);
    for (int i = 0; i < 5; i++)
    {
        marshallByte(th, env.shop[i].keeper_name[0]);
        marshallByte(th, env.shop[i].keeper_name[1]);
        marshallByte(th, env.shop[i].keeper_name[2]);
        marshallByte(th, env.shop[i].x);
        marshallByte(th, env.shop[i].y);
        marshallByte(th, env.shop[i].greed);
        marshallByte(th, env.shop[i].type);
        marshallByte(th, env.shop[i].level);
    }

    // how many markers
    marshallShort(th, env.markers.size());
    for (dgn_marker_map::const_iterator i = env.markers.begin();
         i != env.markers.end(); ++i)
    {
        i->second->write(th);
    }
}

static void marshall_item(tagHeader &th, const item_def &item)
{
    marshallByte(th, item.base_type);
    marshallByte(th, item.sub_type);
    marshallShort(th, item.plus);
    marshallShort(th, item.plus2);
    marshallLong(th, item.special);
    marshallShort(th, item.quantity);

    marshallByte(th, item.colour);
    marshallShort(th, item.x);
    marshallShort(th, item.y);
    marshallLong(th, item.flags);

    marshallShort(th, item.link);                //  unused
    marshallShort(th, igrd[item.x][item.y]);  //  unused

    marshallByte(th, item.slot);

    marshallShort(th, item.orig_place);
    marshallShort(th, item.orig_monnum);
    marshallString(th, item.inscription.c_str(), 80);
}

static void unmarshall_item(tagHeader &th, item_def &item)
{
    item.base_type = static_cast<object_class_type>(unmarshallByte(th));
    item.sub_type = (unsigned char) unmarshallByte(th);
    item.plus = unmarshallShort(th);
    item.plus2 = unmarshallShort(th);
    item.special = unmarshallLong(th);
    item.quantity = unmarshallShort(th);
    item.colour = (unsigned char) unmarshallByte(th);
    item.x = unmarshallShort(th);
    item.y = unmarshallShort(th);
    item.flags = (unsigned long) unmarshallLong(th);
        
    // [dshaligram] FIXME, remove this kludge when ARM_CAP is fully
    // integrated.
    if (item.base_type == OBJ_ARMOUR && item.sub_type == ARM_CAP)
        item.sub_type = ARM_HELMET;

    unmarshallShort(th);  // mitm[].link -- unused
    unmarshallShort(th);  // igrd[item.x][item.y] -- unused

    item.slot = unmarshallByte(th);

    item.orig_place  = unmarshallShort(th);
    item.orig_monnum = unmarshallShort(th);        
    item.inscription = unmarshallString(th, 80);
}

static void tag_construct_level_items(struct tagHeader &th)
{
    // how many traps?
    marshallShort(th, MAX_TRAPS);
    for (int i = 0; i < MAX_TRAPS; ++i)
    {
        marshallByte(th, env.trap[i].type);
        marshallByte(th, env.trap[i].x);
        marshallByte(th, env.trap[i].y);
    }

    // how many items?
    marshallShort(th, MAX_ITEMS);
    for (int i = 0; i < MAX_ITEMS; ++i)
        marshall_item(th, mitm[i]);
}

static void marshall_mon_enchant(tagHeader &th, const mon_enchant &me)
{
    marshallShort(th, me.ench);
    marshallShort(th, me.degree);
    marshallShort(th, me.who);
    marshallShort(th, me.duration);
    marshallShort(th, me.maxduration);
}

static mon_enchant unmarshall_mon_enchant(tagHeader &th)
{
    mon_enchant me;
    me.ench   = static_cast<enchant_type>( unmarshallShort(th) );
    me.degree = unmarshallShort(th);
    me.who    = static_cast<kill_category>( unmarshallShort(th) );
    me.duration = unmarshallShort(th);
    me.maxduration = unmarshallShort(th);
    return (me);
}

static void marshall_monster(tagHeader &th, const monsters &m)
{
    marshallByte(th, m.ac);
    marshallByte(th, m.ev);
    marshallByte(th, m.hit_dice);
    marshallByte(th, m.speed);
    marshallByte(th, m.speed_increment);
    marshallByte(th, m.behaviour);
    marshallByte(th, m.x);
    marshallByte(th, m.y);
    marshallByte(th, m.target_x);
    marshallByte(th, m.target_y);
    marshallLong(th, m.flags);

    marshallShort(th, m.enchantments.size());
    for (mon_enchant_list::const_iterator i = m.enchantments.begin();
         i != m.enchantments.end(); ++i)
    {
        marshall_mon_enchant(th, i->second);
    }

    marshallShort(th, m.type);
    marshallShort(th, m.hit_points);
    marshallShort(th, m.max_hit_points);
    marshallShort(th, m.number);
    marshallShort(th, m.colour);

    for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
        marshallShort(th, m.inv[j]);

    for (int j = 0; j < NUM_MONSTER_SPELL_SLOTS; ++j)
        marshallShort(th, m.spells[j]);

    marshallByte(th, m.god);

    if (m.type == MONS_PLAYER_GHOST || m.type == MONS_PANDEMONIUM_DEMON)
    {
        // *Must* have ghost field set.
        ASSERT(m.ghost.get());
        marshallGhost(th, *m.ghost);
    }
}

static void tag_construct_level_monsters(struct tagHeader &th)
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

void tag_construct_level_attitude(struct tagHeader &th)
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


static void tag_read_level( struct tagHeader &th, char minorVersion )
{
    env.elapsed_time = unmarshallFloat(th);
    // map grids
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);

    env.turns_on_level = unmarshallLong(th);
    
    for (int i = 0; i < gx; i++)
    {
        for (int j = 0; j < gy; j++)
        {
            grd[i][j] =
                static_cast<dungeon_feature_type>(
                    static_cast<unsigned char>(unmarshallByte(th)) );

            env.map[i][j].object = unmarshallShort(th);
            env.map[i][j].colour = unmarshallShort(th);
            env.map[i][j].flags  = unmarshallShort(th);

            mgrd[i][j] = NON_MONSTER;
            env.cgrid[i][j] = (unsigned char) unmarshallByte(th);
        }
    }

    env.cloud_no = unmarshallShort(th);

    // how many clouds?
    const int num_clouds = unmarshallShort(th);
    for (int i = 0; i < num_clouds; i++)
    {
        env.cloud[i].x = unmarshallByte(th);
        env.cloud[i].y = unmarshallByte(th);
        env.cloud[i].type = static_cast<cloud_type>(unmarshallByte(th));
        env.cloud[i].decay = unmarshallShort(th);
        env.cloud[i].whose = static_cast<kill_category>(unmarshallShort(th));
    }

    // how many shops?
    const int num_shops = unmarshallByte(th);
    for (int i = 0; i < num_shops; i++)
    {
        env.shop[i].keeper_name[0] = unmarshallByte(th);
        env.shop[i].keeper_name[1] = unmarshallByte(th);
        env.shop[i].keeper_name[2] = unmarshallByte(th);
        env.shop[i].x = unmarshallByte(th);
        env.shop[i].y = unmarshallByte(th);
        env.shop[i].greed = unmarshallByte(th);
        env.shop[i].type = static_cast<shop_type>(unmarshallByte(th));
        env.shop[i].level = unmarshallByte(th);
    }

    env.clear_markers();
    const int nmarkers = unmarshallShort(th);
    for (int i = 0; i < nmarkers; ++i)
    {
        if (map_marker *mark = map_marker::read_marker(th))
            env.add_marker(mark);
    }
}

static void tag_read_level_items(struct tagHeader &th, char minorVersion)
{
    // how many traps?
    const int trap_count = unmarshallShort(th);
    for (int i = 0; i < trap_count; ++i)
    {
        env.trap[i].type =
            static_cast<trap_type>(
                static_cast<unsigned char>(unmarshallByte(th)) );
        env.trap[i].x = unmarshallByte(th);
        env.trap[i].y = unmarshallByte(th);
    }

    // how many items?
    const int item_count = unmarshallShort(th);
    for (int i = 0; i < item_count; ++i)
        unmarshall_item(th, mitm[i]);
}

static void unmarshall_monster(tagHeader &th, monsters &m)
{
    m.ac = unmarshallByte(th);
    m.ev = unmarshallByte(th);
    m.hit_dice = unmarshallByte(th);
    m.speed = unmarshallByte(th);
    // Avoid sign extension when loading files (Elethiomel's hang)
    m.speed_increment = (unsigned char) unmarshallByte(th);
    m.behaviour = static_cast<beh_type>(unmarshallByte(th));
    m.x = unmarshallByte(th);
    m.y = unmarshallByte(th);
    m.target_x = unmarshallByte(th);
    m.target_y = unmarshallByte(th);
    m.flags = unmarshallLong(th);

    m.enchantments.clear();
    const int nenchs = unmarshallShort(th);
    for (int i = 0; i < nenchs; ++i)
    {
        mon_enchant me = unmarshall_mon_enchant(th);
        m.enchantments[me.ench] = me;
    }

    m.type = unmarshallShort(th);
    m.hit_points = unmarshallShort(th);
    m.max_hit_points = unmarshallShort(th);
    m.number = unmarshallShort(th);

    m.colour = unmarshallShort(th);

    for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
        m.inv[j] = unmarshallShort(th);

    for (int j = 0; j < NUM_MONSTER_SPELL_SLOTS; ++j)
        m.spells[j] = static_cast<spell_type>( unmarshallShort(th) );

    m.god = (god_type) unmarshallByte(th);

    if (m.type == MONS_PLAYER_GHOST || m.type == MONS_PANDEMONIUM_DEMON)
        m.set_ghost( unmarshallGhost(th) );

    m.check_speed();
}

static void tag_read_level_monsters(struct tagHeader &th, char minorVersion)
{
    int i;
    int count, icount;

    // how many mons_alloc?
    count = unmarshallByte(th);
    for (i = 0; i < count; ++i)
        env.mons_alloc[i] = unmarshallShort(th);

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
            mgrd[m.x][m.y] = i;
    }
}

void tag_read_level_attitude(struct tagHeader &th)
{
    int i, count;

    // how many monsters?
    count = unmarshallShort(th);

    for (i = 0; i < count; i++)
    {
        menv[i].attitude = static_cast<mon_attitude_type>(unmarshallByte(th));
        menv[i].foe = unmarshallShort(th);
    }
}

void tag_missing_level_attitude()
{
    // we don't really have to do a lot here.
    // just set foe to MHITNOT;  they'll pick up
    // a foe first time through handle_monster() if
    // there's one around.

    // as for attitude, a couple simple checks
    // can be used to determine friendly/neutral/
    // hostile.
    int i;
    bool isFriendly;
    unsigned int new_beh = BEH_WANDER;

    for(i=0; i<MAX_MONSTERS; i++)
    {
        // only do actual monsters
        if (menv[i].type < 0)
            continue;

        isFriendly = testbits(menv[i].flags, MF_CREATED_FRIENDLY);

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
                    isFriendly = true;
                break;
            default:
                break;
        }

        menv[i].attitude = (isFriendly)?ATT_FRIENDLY : ATT_HOSTILE;
        menv[i].behaviour = static_cast<beh_type>(new_beh);
        menv[i].foe_memory = 0;
    }
}


// ------------------------------- ghost tags ---------------------------- //

static void marshallGhost(tagHeader &th, const ghost_demon &ghost)
{
    marshallString(th, ghost.name.c_str(), 20);

    // how many ghost values?
    marshallByte(th, NUM_GHOST_VALUES);

    for (int i = 0; i < NUM_GHOST_VALUES; i++)
        marshallShort( th, ghost.values[i] );
}

static void tag_construct_ghost(struct tagHeader &th)
{
    // How many ghosts?
    marshallShort(th, ghosts.size());

    for (int i = 0, size = ghosts.size(); i < size; ++i)
        marshallGhost(th, ghosts[i]);
}

static ghost_demon unmarshallGhost( tagHeader &th )
{
    ghost_demon ghost;
    
    ghost.name = unmarshallString(th, 20);

    // how many ghost values?
    int count_c = unmarshallByte(th);

    if (count_c > NUM_GHOST_VALUES)
        count_c = NUM_GHOST_VALUES;
    
    for (int i = 0; i < count_c; i++)
        ghost.values[i] = unmarshallShort(th);

    return (ghost);
}

static void tag_read_ghost(struct tagHeader &th, char minorVersion)
{
    int nghosts = unmarshallShort(th);

    if (nghosts < 1 || nghosts > MAX_GHOSTS)
        return;

    for (int i = 0; i < nghosts; ++i)
        ghosts.push_back( unmarshallGhost(th) );
}
