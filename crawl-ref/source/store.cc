/**
 * @file
 * @brief Saveable hash-table and vector capable of storing
 *             multiple types of data.
**/

#include "AppHdr.h"

#include "store.h"

#include "dlua.h"
#include "externs.h"
#include "libutil.h"
#include "monster.h"
#include "tags.h"
#include "travel.h"

#include <algorithm>

// These tend to be called from tight loops, and C++ method calls don't
// get optimized away except for LTO -fwhole-program builds, so merely
// disabling the function's body is not enough; let's not call them at all.
#ifdef DEBUG
# define ASSERT_VALIDITY(x) x assert_validity()
#else
# define ASSERT_VALIDITY(x)
#endif

CrawlStoreValue::CrawlStoreValue()
    : type(SV_NONE), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
}

CrawlStoreValue::CrawlStoreValue(const CrawlStoreValue &other)
{
    ASSERT_RANGE(other.type, SV_NONE, NUM_STORE_VAL_TYPES);

    val.ptr = NULL;

    type  = other.type;
    flags = other.flags;

    if (flags & SFLAG_UNSET)
    {
        val = other.val;
        return;
    }

    switch (type)
    {
    case SV_NONE:
    case SV_BOOL:
    case SV_BYTE:
    case SV_SHORT:
    case SV_INT:
    case SV_INT64:
    case SV_FLOAT:
        val = other.val;
        break;

    case SV_STR:
    {
        string* str;
        str = new string(*static_cast<string*>(other.val.ptr));
        val.ptr = static_cast<void*>(str);
        break;
    }

    case SV_COORD:
    {
        coord_def* coord;
        coord = new coord_def(*static_cast<coord_def*>(other.val.ptr));
        val.ptr = static_cast<void*>(coord);
        break;
    }

    case SV_ITEM:
    {
        item_def* item;
        item = new item_def(*static_cast<item_def*>(other.val.ptr));
        val.ptr = static_cast<void*>(item);
        break;
    }

    case SV_HASH:
    {
        CrawlHashTable* hash;
        CrawlHashTable* tmp = static_cast<CrawlHashTable*>(other.val.ptr);
        hash = new CrawlHashTable(*tmp);
        val.ptr = static_cast<void*>(hash);
        break;
    }

    case SV_VEC:
    {
        CrawlVector* vec;
        CrawlVector* tmp = static_cast<CrawlVector*>(other.val.ptr);
        vec = new CrawlVector(*tmp);
        val.ptr = static_cast<void*>(vec);
        break;
    }

    case SV_LEV_ID:
    {
        level_id* id;
        id = new level_id(*static_cast<level_id*>(other.val.ptr));
        val.ptr = static_cast<void*>(id);
        break;
    }

    case SV_LEV_POS:
    {
        level_pos* pos;
        pos = new level_pos(*static_cast<level_pos*>(other.val.ptr));
        val.ptr = static_cast<void*>(pos);
        break;
    }

    case SV_MONST:
    {
        monster* mon;
        mon = new monster(*static_cast<monster* >(other.val.ptr));
        val.ptr = static_cast<void*>(mon);
        break;
    }

    case SV_LUA:
    {
        dlua_chunk* chunk;
        chunk = new dlua_chunk(*static_cast<dlua_chunk*>(other.val.ptr));
        val.ptr = static_cast<void*>(chunk);
        break;
    }

    case NUM_STORE_VAL_TYPES:
        die("unknown stored value type");
    }
}

CrawlStoreValue::CrawlStoreValue(const store_flags _flags,
                                 const store_val_type _type)
    : type(_type), flags(_flags)
{
    ASSERT_RANGE(type, SV_NONE, NUM_STORE_VAL_TYPES);
    ASSERT(!(flags & SFLAG_UNSET));

    flags   |= SFLAG_UNSET;
    val.ptr  = NULL;
}

// Conversion constructors
CrawlStoreValue::CrawlStoreValue(const bool _val)
    : type(SV_BOOL), flags(SFLAG_UNSET)
{
    get_bool() = _val;
}

CrawlStoreValue::CrawlStoreValue(const char &_val)
    : type(SV_BYTE), flags(SFLAG_UNSET)
{
    get_byte() = _val;
}

CrawlStoreValue::CrawlStoreValue(const short &_val)
    : type(SV_SHORT), flags(SFLAG_UNSET)
{
    get_short() = _val;
}

CrawlStoreValue::CrawlStoreValue(const int &_val)
    : type(SV_INT), flags(SFLAG_UNSET)
{
    get_int() = _val;
}

CrawlStoreValue::CrawlStoreValue(const int64_t &_val)
    : type(SV_INT), flags(SFLAG_UNSET)
{
    get_int64() = _val;
}

CrawlStoreValue::CrawlStoreValue(const float &_val)
    : type(SV_FLOAT), flags(SFLAG_UNSET)
{
    get_float() = _val;
}

CrawlStoreValue::CrawlStoreValue(const string &_val)
    : type(SV_STR), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_string() = _val;
}

CrawlStoreValue::CrawlStoreValue(const char* _val)
    : type(SV_STR), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_string() = _val;
}

CrawlStoreValue::CrawlStoreValue(const coord_def &_val)
    : type(SV_COORD), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_coord() = _val;
}

CrawlStoreValue::CrawlStoreValue(const item_def &_val)
    : type(SV_ITEM), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_item() = _val;
}

CrawlStoreValue::CrawlStoreValue(const CrawlHashTable &_val)
    : type(SV_HASH), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_table() = _val;
}

CrawlStoreValue::CrawlStoreValue(const CrawlVector &_val)
    : type(SV_VEC), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_vector() = _val;
}

CrawlStoreValue::CrawlStoreValue(const level_id &_val)
    : type(SV_LEV_ID), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_level_id() = _val;
}

CrawlStoreValue::CrawlStoreValue(const level_pos &_val)
    : type(SV_LEV_POS), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_level_pos() = _val;
}

CrawlStoreValue::CrawlStoreValue(const monster& _val)
    : type(SV_MONST), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_monster() = _val;
}

CrawlStoreValue::CrawlStoreValue(const dlua_chunk &_val)
    : type(SV_LUA), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
    get_lua() = _val;
}

CrawlStoreValue::~CrawlStoreValue()
{
    unset(true);
}

void CrawlStoreValue::unset(bool force)
{
    if (flags & SFLAG_UNSET)
        return;

    if (force)
        flags &= ~SFLAG_NO_ERASE;

    ASSERT(!(flags & SFLAG_NO_ERASE));

    switch (type)
    {
    case SV_BOOL:
        val.boolean = false;
        break;

    case SV_BYTE:
        val.byte = 0;
        break;

    case SV_SHORT:
        val._short = 0;
        break;

    case SV_INT:
        val._int = 0;
        break;

    case SV_INT64:
        val._int64 = 0;
        break;

    case SV_FLOAT:
        val._float = 0.0;
        break;

    case SV_STR:
    {
        string* str = static_cast<string*>(val.ptr);
        delete str;
        val.ptr = NULL;
        break;
    }

    case SV_COORD:
    {
        coord_def* coord = static_cast<coord_def*>(val.ptr);
        delete coord;
        val.ptr = NULL;
        break;
    }

    case SV_ITEM:
    {
        item_def* item = static_cast<item_def*>(val.ptr);
        delete item;
        val.ptr = NULL;
        break;
    }

    case SV_HASH:
    {
        CrawlHashTable* hash = static_cast<CrawlHashTable*>(val.ptr);
        delete hash;
        val.ptr = NULL;
        break;
    }

    case SV_VEC:
    {
        CrawlVector* vec = static_cast<CrawlVector*>(val.ptr);
        delete vec;
        val.ptr = NULL;
        break;
    }

    case SV_LEV_ID:
    {
        level_id* id = static_cast<level_id*>(val.ptr);
        delete id;
        val.ptr = NULL;
        break;
    }

    case SV_LEV_POS:
    {
        level_pos* pos = static_cast<level_pos*>(val.ptr);
        delete pos;
        val.ptr = NULL;
        break;
    }

    case SV_MONST:
    {
        monster* mon = static_cast<monster* >(val.ptr);
        delete mon;
        val.ptr = NULL;
        break;
    }

    case SV_LUA:
    {
        dlua_chunk* chunk = static_cast<dlua_chunk*>(val.ptr);
        delete chunk;
        val.ptr = NULL;
        break;
    }

    case SV_NONE:
        die("CrawlStoreValue::unset: unsetting nothing");
        break;

    default:
        die("CrawlStoreValue::unset: unsetting invalid type");
        break;
    }

    flags |= SFLAG_UNSET;
}

#define COPY_PTR(ptr_type) \
    { \
        ptr_type *ptr = static_cast<ptr_type*>(val.ptr); \
        if (ptr != NULL) \
            delete ptr; \
        ptr = static_cast<ptr_type*>(other.val.ptr); \
        val.ptr = (void*) new ptr_type (*ptr);  \
    }

CrawlStoreValue &CrawlStoreValue::operator = (const CrawlStoreValue &other)
{
    ASSERT_RANGE(other.type, SV_NONE, NUM_STORE_VAL_TYPES);
    ASSERT(other.type != SV_NONE || type == SV_NONE);

    // NOTE: We don't bother checking SFLAG_CONST_VAL, since the
    // asignment operator is used when swapping two elements.
    if (!(flags & SFLAG_UNSET))
    {
        if (flags & SFLAG_CONST_TYPE)
            ASSERT(type == SV_NONE || type == other.type);
    }

    type  = other.type;
    flags = other.flags;

    switch (type)
    {
    case SV_NONE:
    case SV_BOOL:
    case SV_BYTE:
    case SV_SHORT:
    case SV_INT:
    case SV_INT64:
    case SV_FLOAT:
        val = other.val;
        break;

    case SV_STR:
        COPY_PTR(string);
        break;

    case SV_COORD:
        COPY_PTR(coord_def);
        break;

    case SV_ITEM:
        COPY_PTR(item_def);
        break;

    case SV_HASH:
        COPY_PTR(CrawlHashTable);
        break;

    case SV_VEC:
        COPY_PTR(CrawlVector);
        break;

    case SV_LEV_ID:
        COPY_PTR(level_id);
        break;

     case SV_LEV_POS:
        COPY_PTR(level_pos);
        break;

     default:
        die("CrawlStoreValue has invalid type");
        break;
    }

    return *this;
}

///////////////////////////////////
// Meta-data accessors and changers
store_flags CrawlStoreValue::get_flags() const
{
    return flags;
}

store_flags CrawlStoreValue::set_flags(store_flags _flags)
{
    flags |= _flags;
    return flags;
}

store_flags CrawlStoreValue::unset_flags(store_flags _flags)
{
    flags &= ~_flags;
    return flags;
}

store_val_type CrawlStoreValue::get_type() const
{
    return type;
}

//////////////////////////////
// Read/write from/to savefile
void CrawlStoreValue::write(writer &th) const
{
    ASSERT(type != SV_NONE || (flags & SFLAG_UNSET));
    ASSERT(!(flags & SFLAG_UNSET) || (type == SV_NONE));

    marshallByte(th,  (char) type);
    marshallByte(th, (char) flags);

    switch (type)
    {
    case SV_BOOL:
        marshallBoolean(th, val.boolean);
        break;

    case SV_BYTE:
        marshallByte(th, val.byte);
        break;

    case SV_SHORT:
        marshallShort(th, val._short);
        break;

    case SV_INT:
        marshallInt(th, val._int);
        break;

    case SV_INT64:
        marshallSigned(th, val._int64);
        break;

    case SV_FLOAT:
        marshallFloat(th, val._float);
        break;

    case SV_STR:
    {
        string* str = static_cast<string*>(val.ptr);
        marshallString(th, *str);
        break;
    }

    case SV_COORD:
    {
        coord_def* coord = static_cast<coord_def*>(val.ptr);
        marshallCoord(th, *coord);
        break;
    }

    case SV_ITEM:
    {
        item_def* item = static_cast<item_def*>(val.ptr);
        marshallItem(th, *item);
        break;
    }

    case SV_HASH:
    {
        CrawlHashTable* hash = static_cast<CrawlHashTable*>(val.ptr);
        hash->write(th);
        break;
    }

    case SV_VEC:
    {
        CrawlVector* vec = static_cast<CrawlVector*>(val.ptr);
        vec->write(th);
        break;
    }

    case SV_LEV_ID:
    {
        level_id* id = static_cast<level_id*>(val.ptr);
        id->save(th);
        break;
    }

    case SV_LEV_POS:
    {
        level_pos* pos = static_cast<level_pos*>(val.ptr);
        pos->save(th);
        break;
    }

    case SV_MONST:
    {
        monster* mon = static_cast<monster* >(val.ptr);
        marshallMonster(th, *mon);
        break;
    }

    case SV_LUA:
    {
        dlua_chunk* chunk = static_cast<dlua_chunk*>(val.ptr);
        chunk->write(th);
        break;
    }

    case SV_NONE:
        break;

    case NUM_STORE_VAL_TYPES:
        die("unknown stored value type");
    }
}

void CrawlStoreValue::read(reader &th)
{
    type = static_cast<store_val_type>(unmarshallByte(th));
    flags = (store_flags) unmarshallByte(th);

    ASSERT(type != SV_NONE || (flags & SFLAG_UNSET));
    ASSERT(!(flags & SFLAG_UNSET) || (type == SV_NONE));

    switch (type)
    {
    case SV_BOOL:
        val.boolean = unmarshallBoolean(th);
        break;

    case SV_BYTE:
        val.byte = unmarshallByte(th);
        break;

    case SV_SHORT:
        val._short = unmarshallShort(th);
        break;

    case SV_INT:
        val._int = unmarshallInt(th);
        break;

    case SV_INT64:
        val._int64 = unmarshallSigned(th);
        break;

    case SV_FLOAT:
        val._float = unmarshallFloat(th);
        break;

    case SV_STR:
    {
        string str = unmarshallString(th);
        val.ptr = (void*) new string(str);
        break;
    }

    case SV_COORD:
    {
        const coord_def coord = unmarshallCoord(th);
        val.ptr = (void*) new coord_def(coord);

        break;
    }

    case SV_ITEM:
    {
        item_def item;
        unmarshallItem(th, item);
        val.ptr = (void*) new item_def(item);

        break;
    }

    case SV_HASH:
    {
        CrawlHashTable* hash = new CrawlHashTable();
        hash->read(th);
        val.ptr = (void*) hash;

        break;
    }

    case SV_VEC:
    {
        CrawlVector* vec = new CrawlVector();
        vec->read(th);
        val.ptr = (void*) vec;

        break;
    }

    case SV_LEV_ID:
    {
        level_id* id = new level_id();
        id->load(th);
        val.ptr = (void*) id;

        break;
    }

    case SV_LEV_POS:
    {
        level_pos* pos = new level_pos();
        pos->load(th);
        val.ptr = (void*) pos;

        break;
    }

    case SV_MONST:
    {
        monster mon;
        unmarshallMonster(th, mon);
        val.ptr = (void*) new monster(mon);

        break;
    }

    case SV_LUA:
    {
        dlua_chunk chunk;
        chunk.read(th);
        val.ptr = (void*) new dlua_chunk(chunk);

        break;
    }

    case SV_NONE:
        break;

    case NUM_STORE_VAL_TYPES:
        die("unknown stored value type");
    }
}

CrawlHashTable &CrawlStoreValue::new_table()
{
    return get_table();
}

////////////////////////////////////////////////////////////////
// Setup a new vector with the given flags and/or type; assert if
// a vector already exists.
CrawlVector &CrawlStoreValue::new_vector(store_flags _flags,
                                         vec_size max_size)
{
    return new_vector(SV_NONE, flags, max_size);
}

CrawlVector &CrawlStoreValue::new_vector(store_val_type _type,
                                         store_flags _flags,
                                         vec_size _max_size)
{
#ifdef DEBUG
    CrawlVector* old_vec = static_cast<CrawlVector*>(val.ptr);

    ASSERT(flags & SFLAG_UNSET);
    ASSERT(type == SV_NONE
           || (type == SV_VEC
               && old_vec->empty()
               && old_vec->get_type() == SV_NONE
               && old_vec->get_default_flags() == 0
               && old_vec->get_max_size() == VEC_MAX_SIZE));
#endif

    CrawlVector &vec = get_vector();

    vec.default_flags = _flags;
    vec.type          = _type;

    type   =  SV_VEC;
    flags &= ~SFLAG_UNSET;

    return vec;
}

///////////////////////////////////////////
// Dynamic type-checking accessor functions
#define GET_VAL(x, _type, field, value)                            \
    ASSERT((flags & SFLAG_UNSET) || !(flags & SFLAG_CONST_VAL)); \
    if (type != (x)) \
    { \
        if (type == SV_NONE) \
        { \
            type  = (x); \
            field = (value); \
        } \
        else \
        { \
            ASSERT(!(flags & SFLAG_CONST_TYPE)); \
            switch (type) \
            { \
            case SV_BOOL: \
                field = (_type) val.boolean; \
                break; \
            case SV_BYTE: \
                field = (_type) val.byte; \
                break; \
            case SV_SHORT: \
                field = (_type) val._short; \
                break; \
            case SV_INT: \
                field = (_type) val._int; \
                break; \
            case SV_INT64: \
                field = (_type) val._int64; \
                break; \
            case SV_FLOAT: \
                field = (_type) val._float; \
                break; \
            default: \
                die("unknown stored value type"); \
            } \
            type = (x); \
        } \
    } \
    flags &= ~SFLAG_UNSET; \
    return field;

#define GET_VAL_PTR(x, _type, value) \
    ASSERT((flags & SFLAG_UNSET) || !(flags & SFLAG_CONST_VAL)); \
    if (type != (x) || (flags & SFLAG_UNSET)) \
    { \
        if (type == SV_NONE) \
        { \
            type    = (x); \
            val.ptr = (value); \
        } \
        else \
        { \
            unset(); \
            val.ptr = (value); \
            type    = (x); \
        } \
    } \
    else \
        delete (value); \
    flags &= ~SFLAG_UNSET; \
    return *((_type) val.ptr);

bool &CrawlStoreValue::get_bool()
{
    GET_VAL(SV_BOOL, bool, val.boolean, false);
}

char &CrawlStoreValue::get_byte()
{
    GET_VAL(SV_BYTE, char, val.byte, 0);
}

short &CrawlStoreValue::get_short()
{
    GET_VAL(SV_SHORT, short, val._short, 0);
}

int &CrawlStoreValue::get_int()
{
    GET_VAL(SV_INT, int, val._int, 0);
}

int64_t &CrawlStoreValue::get_int64()
{
    GET_VAL(SV_INT64, int64_t, val._int64, 0);
}

float &CrawlStoreValue::get_float()
{
    GET_VAL(SV_FLOAT, float, val._float, 0.0);
}

string &CrawlStoreValue::get_string()
{
    GET_VAL_PTR(SV_STR, string*, new string(""));
}

coord_def &CrawlStoreValue::get_coord()
{
    GET_VAL_PTR(SV_COORD, coord_def*, new coord_def());
}

item_def &CrawlStoreValue::get_item()
{
    GET_VAL_PTR(SV_ITEM, item_def*, new item_def());
}

CrawlHashTable &CrawlStoreValue::get_table()
{
    GET_VAL_PTR(SV_HASH, CrawlHashTable*, new CrawlHashTable());
}

CrawlVector &CrawlStoreValue::get_vector()
{
    GET_VAL_PTR(SV_VEC, CrawlVector*, new CrawlVector());
}

level_id &CrawlStoreValue::get_level_id()
{
    GET_VAL_PTR(SV_LEV_ID, level_id*, new level_id());
}

level_pos &CrawlStoreValue::get_level_pos()
{
    GET_VAL_PTR(SV_LEV_POS, level_pos*, new level_pos());
}

monster &CrawlStoreValue::get_monster()
{
    GET_VAL_PTR(SV_MONST, monster* , new monster());
}

dlua_chunk &CrawlStoreValue::get_lua()
{
    GET_VAL_PTR(SV_LUA, dlua_chunk*, new dlua_chunk());
}

///////////////////////////
// Const accessor functions
#define GET_CONST_SETUP(x) \
    ASSERT(!(flags & SFLAG_UNSET)); \
    ASSERT(type == (x));

bool CrawlStoreValue::get_bool() const
{
    GET_CONST_SETUP(SV_BOOL);
    return val.boolean;
}

char CrawlStoreValue::get_byte() const
{
    GET_CONST_SETUP(SV_BYTE);
    return val.byte;
}

short CrawlStoreValue::get_short() const
{
    GET_CONST_SETUP(SV_SHORT);
    return val._short;
}

int CrawlStoreValue::get_int() const
{
    GET_CONST_SETUP(SV_INT);
    return val._int;
}

int64_t CrawlStoreValue::get_int64() const
{
    GET_CONST_SETUP(SV_INT64);
    return val._int64;
}

float CrawlStoreValue::get_float() const
{
    GET_CONST_SETUP(SV_FLOAT);
    return val._float;
}

string CrawlStoreValue::get_string() const
{
    GET_CONST_SETUP(SV_STR);
    return *((string*)val.ptr);
}

coord_def CrawlStoreValue::get_coord() const
{
    GET_CONST_SETUP(SV_COORD);
    return *((coord_def*)val.ptr);
}

const item_def& CrawlStoreValue::get_item() const
{
    GET_CONST_SETUP(SV_ITEM);
    return *((item_def*)val.ptr);
}

const CrawlHashTable& CrawlStoreValue::get_table() const
{
    GET_CONST_SETUP(SV_HASH);
    return *((CrawlHashTable*)val.ptr);
}

const CrawlVector& CrawlStoreValue::get_vector() const
{
    GET_CONST_SETUP(SV_VEC);
    return *((CrawlVector*)val.ptr);
}

const monster& CrawlStoreValue::get_monster() const
{
    GET_CONST_SETUP(SV_MONST);
    return *((monster*)val.ptr);
}

level_id CrawlStoreValue::get_level_id() const
{
    GET_CONST_SETUP(SV_LEV_ID);
    return *((level_id*)val.ptr);
}

level_pos CrawlStoreValue::get_level_pos() const
{
    GET_CONST_SETUP(SV_LEV_POS);
    return *((level_pos*)val.ptr);
}

/////////////////////
// Typecast operators
CrawlStoreValue::operator bool&()                  { return get_bool();       }
CrawlStoreValue::operator char&()                  { return get_byte();       }
CrawlStoreValue::operator short&()                 { return get_short();      }
CrawlStoreValue::operator float&()                 { return get_float();      }
CrawlStoreValue::operator int&()                   { return get_int();        }
CrawlStoreValue::operator int64_t&()               { return get_int64();      }
CrawlStoreValue::operator string&()                { return get_string();     }
CrawlStoreValue::operator coord_def&()             { return get_coord();      }
CrawlStoreValue::operator CrawlHashTable&()        { return get_table();      }
CrawlStoreValue::operator CrawlVector&()           { return get_vector();     }
CrawlStoreValue::operator item_def&()              { return get_item();       }
CrawlStoreValue::operator level_id&()              { return get_level_id();   }
CrawlStoreValue::operator level_pos&()             { return get_level_pos();  }
CrawlStoreValue::operator monster& ()              { return get_monster();    }
CrawlStoreValue::operator dlua_chunk&()            { return get_lua(); }

///////////////////////////
// Const typecast operators
CrawlStoreValue::operator bool() const
{
    return get_bool();
}

#define CONST_INT_CAST() \
    switch (type) \
    { \
    case SV_BYTE: \
        return get_byte(); \
    case SV_SHORT: \
        return get_short(); \
    case SV_INT: \
        return get_int(); \
    default: \
        die("unknown stored value type"); \
    }

CrawlStoreValue::operator char() const
{
    CONST_INT_CAST();
}

CrawlStoreValue::operator short() const
{
    CONST_INT_CAST();
}

CrawlStoreValue::operator int() const
{
    CONST_INT_CAST();
}

CrawlStoreValue::operator int64_t() const
{
    // Allow upgrading but not downgrading.
    switch (type)
    {
    case SV_BYTE:
        return get_byte();
    case SV_SHORT:
        return get_short();
    case SV_INT:
        return get_int();
    case SV_INT64:
        return get_int64();
    default:
        die("unknown stored value type");
    }
}

CrawlStoreValue::operator float() const
{
    return get_float();
}

CrawlStoreValue::operator string() const
{
    return get_string();
}

CrawlStoreValue::operator coord_def() const
{
    return get_coord();
}

CrawlStoreValue::operator level_id() const
{
    return get_level_id();
}

CrawlStoreValue::operator level_pos() const
{
    return get_level_pos();
}

///////////////////////
// Assignment operators
CrawlStoreValue &CrawlStoreValue::operator = (const bool &_val)
{
    get_bool() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const char &_val)
{
    get_byte() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const short &_val)
{
    get_short() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const int &_val)
{
    get_int() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const int64_t &_val)
{
    get_int64() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const float &_val)
{
    get_float() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const string &_val)
{
    get_string() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const char* _val)
{
    get_string() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const coord_def &_val)
{
    get_coord() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const CrawlHashTable &_val)
{
    get_table() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const CrawlVector &_val)
{
    get_vector() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const item_def &_val)
{
    get_item() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const level_id &_val)
{
    get_level_id() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const level_pos &_val)
{
    get_level_pos() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const monster& _val)
{
    get_monster() = _val;
    return *this;
}

CrawlStoreValue &CrawlStoreValue::operator = (const dlua_chunk &_val)
{
    get_lua() = _val;
    return *this;
}

///////////////////////////////////////////////////
// Non-assignment operators which affect the lvalue
#define INT_OPERATOR_UNARY(op) \
    switch (type) \
    { \
    case SV_BYTE: \
    { \
        char &temp = get_byte(); \
        temp op; \
        return temp; \
    } \
 \
    case SV_SHORT: \
    { \
        short &temp = get_short(); \
        temp op; \
        return temp; \
    } \
    case SV_INT: \
    { \
        int &temp = get_int(); \
        temp op; \
        return temp; \
    } \
    case SV_INT64: \
    { \
        int64_t &temp = get_int64(); \
        temp op; \
        return temp; \
    } \
 \
    default: \
        die("unknown stored value type"); \
        return 0; \
    }

// Prefix
int CrawlStoreValue::operator ++ ()
{
    INT_OPERATOR_UNARY(++);
}

int CrawlStoreValue::operator -- ()
{
    INT_OPERATOR_UNARY(--);
}

// Postfix
int CrawlStoreValue::operator ++ (int)
{
    INT_OPERATOR_UNARY(++);
}

int CrawlStoreValue::operator -- (int)
{
    INT_OPERATOR_UNARY(--);
}

string &CrawlStoreValue::operator += (const string &_val)
{
    return get_string() += _val;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

CrawlHashTable::CrawlHashTable()
{
    hash_map = NULL;
}

CrawlHashTable::CrawlHashTable(const CrawlHashTable& other)
{
    if (other.hash_map == NULL)
    {
        hash_map = NULL;
        return;
    }

    hash_map = new hash_map_type(*(other.hash_map));
}

CrawlHashTable::~CrawlHashTable()
{
    // NOTE: Not using unique_ptr because making hash_map an unique_ptr
    // causes compile weirdness in externs.h
    if (hash_map == NULL)
        return;

    delete hash_map;
    hash_map = NULL;
}

CrawlHashTable &CrawlHashTable::operator = (const CrawlHashTable &other)
{
    if (hash_map != NULL)
        delete hash_map;

    if (other.hash_map == NULL)
    {
        hash_map = NULL;
        return *this;
    }

    hash_map = new hash_map_type(*(other.hash_map));

    return *this;
}

//////////////////////////////
// Read/write from/to savefile
void CrawlHashTable::write(writer &th) const
{
    ASSERT_VALIDITY();
    if (empty())
    {
        marshallUnsigned(th, 0);
        return;
    }

    marshallUnsigned(th, size());

    CrawlHashTable::hash_map_type::const_iterator i = hash_map->begin();

    for (; i != hash_map->end(); ++i)
    {
        marshallString(th, i->first);
        i->second.write(th);
    }

    ASSERT_VALIDITY();
}

void CrawlHashTable::read(reader &th)
{
    ASSERT_VALIDITY();

    ASSERT(empty());

#if TAG_MAJOR_VERSION == 34
    unsigned int _size;
    if (th.getMinorVersion() < TAG_MINOR_16_BIT_TABLE)
        _size = unmarshallByte(th);
    else
        _size = unmarshallUnsigned(th);
#else
    unsigned int _size = unmarshallUnsigned(th);
#endif

    if (_size == 0)
        return;

    init_hash_map();

    for (unsigned int i = 0; i < _size; i++)
    {
        string           key = unmarshallString(th);
        CrawlStoreValue &val = (*this)[key];

        val.read(th);
    }

    ASSERT_VALIDITY();
}


#ifdef DEBUG_PROPS
static map<string, int> accesses;
# define ACCESS(x) ++accesses[x]
#else
# define ACCESS(x)
#endif

//////////////////
// Misc functions

bool CrawlHashTable::exists(const string &key) const
{
    if (hash_map == NULL)
        return false;

    ACCESS(key);
    ASSERT_VALIDITY();
    hash_map_type::const_iterator i = hash_map->find(key);

    return (i != hash_map->end());
}

void CrawlHashTable::assert_validity() const
{
#ifdef DEBUG
    if (hash_map == NULL)
        return;

    hash_map_type::const_iterator i = hash_map->begin();

    size_t actual_size = 0;

    for (; i != hash_map->end(); ++i)
    {
        actual_size++;

        const string          &key = i->first;
        const CrawlStoreValue &val = i->second;

        ASSERT(!key.empty());
        string trimmed = trimmed_string(key);
        ASSERT(key == trimmed);

        ASSERT(val.type != SV_NONE);
        ASSERT(!(val.flags & SFLAG_UNSET));

        switch (val.type)
        {
        case SV_STR:
        case SV_COORD:
        case SV_ITEM:
        case SV_LEV_ID:
        case SV_LEV_POS:
            ASSERT(val.val.ptr != NULL);
            break;

        case SV_HASH:
        {
            ASSERT(val.val.ptr != NULL);

            CrawlHashTable* nested;
            nested = static_cast<CrawlHashTable*>(val.val.ptr);

            nested->assert_validity();
            break;
        }

        case SV_VEC:
        {
            ASSERT(val.val.ptr != NULL);

            CrawlVector* nested;
            nested = static_cast<CrawlVector*>(val.val.ptr);

            nested->assert_validity();
            break;
        }

        default:
            break;
        }
    }

    ASSERT(size() == actual_size);
#endif
}

////////////////////////////////
// Accessors to contained values

CrawlStoreValue& CrawlHashTable::get_value(const string &key)
{
    ASSERT_VALIDITY();
    init_hash_map();

    ACCESS(key);
    iterator i = hash_map->find(key);

    if (i == hash_map->end())
    {
        (*hash_map)[key]     = CrawlStoreValue();
        CrawlStoreValue &val = (*hash_map)[key];

        return val;
    }

    return i->second;
}

const CrawlStoreValue& CrawlHashTable::get_value(const string &key) const
{
#ifdef ASSERTS
    if (!hash_map)
        die("trying to read non-existent property \"%s\"", key.c_str());
#endif
    ASSERT_VALIDITY();

    ACCESS(key);
    hash_map_type::const_iterator i = hash_map->find(key);

#ifdef ASSERTS
    if (i == hash_map->end())
        die("trying to read non-existent property \"%s\"", key.c_str());
#endif
    ASSERT(i->second.type != SV_NONE);
    ASSERT(!(i->second.flags & SFLAG_UNSET));

    return i->second;
}

///////////////////////////
// std::map style interface
unsigned int CrawlHashTable::size() const
{
    if (hash_map == NULL)
        return 0;

    return hash_map->size();
}

bool CrawlHashTable::empty() const
{
    if (hash_map == NULL)
        return true;

    return hash_map->empty();
}

void CrawlHashTable::erase(const string key)
{
    ASSERT_VALIDITY();
    init_hash_map();

    ACCESS(key);
    iterator i = hash_map->find(key);

    if (i != hash_map->end())
    {
#ifdef ASSERTS
        CrawlStoreValue &val = i->second;
        ASSERT(!(val.flags & SFLAG_NO_ERASE));
#endif

        hash_map->erase(i);
    }
}

void CrawlHashTable::clear()
{
    ASSERT_VALIDITY();
    if (hash_map == NULL)
        return;

    delete hash_map;
    hash_map = NULL;
}

CrawlHashTable::iterator CrawlHashTable::begin()
{
    ASSERT_VALIDITY();
    init_hash_map();

    return hash_map->begin();
}

CrawlHashTable::iterator CrawlHashTable::end()
{
    ASSERT_VALIDITY();
    init_hash_map();

    return hash_map->end();
}

CrawlHashTable::const_iterator CrawlHashTable::begin() const
{
    ASSERT(hash_map != NULL);
    ASSERT_VALIDITY();

    return hash_map->begin();
}

CrawlHashTable::const_iterator CrawlHashTable::end() const
{
    ASSERT(hash_map != NULL);
    ASSERT_VALIDITY();

    return hash_map->end();
}

void CrawlHashTable::init_hash_map()
{
    if (hash_map != NULL)
        return;

    hash_map = new hash_map_type();
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

CrawlVector::CrawlVector()
    : type(SV_NONE), default_flags(0), max_size(VEC_MAX_SIZE)
{
}

CrawlVector::CrawlVector(store_flags flags, vec_size _max_size)
    : type(SV_NONE), default_flags(flags), max_size(_max_size)
{
    ASSERT(!(default_flags & SFLAG_UNSET));
    ASSERT(max_size > 0);
}

CrawlVector::CrawlVector(store_val_type _type, store_flags flags,
                         vec_size _max_size)
    : type(_type), default_flags(flags), max_size(_max_size)
{
    ASSERT_RANGE(type, SV_NONE, NUM_STORE_VAL_TYPES);
    ASSERT(!(default_flags & SFLAG_UNSET));
    ASSERT(max_size > 0);
}

CrawlVector::~CrawlVector()
{
    ASSERT_VALIDITY();
}

//////////////////////////////
// Read/write from/to savefile
void CrawlVector::write(writer &th) const
{
    ASSERT_VALIDITY();
    if (empty())
    {
        marshallByte(th, 0);
        return;
    }

    marshallUnsigned(th, (char) size());
    marshallUnsigned(th, (char) max_size);
    marshallByte(th, static_cast<char>(type));
    marshallByte(th, (char) default_flags);

    for (vec_size i = 0; i < size(); i++)
    {
        CrawlStoreValue val = vec[i];
        val.write(th);
    }

    ASSERT_VALIDITY();
}

void CrawlVector::read(reader &th)
{
    ASSERT_VALIDITY();

    ASSERT(empty());
    ASSERT(type == SV_NONE);
    ASSERT(default_flags == 0);
    ASSERT(max_size == VEC_MAX_SIZE);

#if TAG_MAJOR_VERSION == 34
    vec_size _size;
    if (th.getMinorVersion() < TAG_MINOR_16_BIT_TABLE)
        _size = (vec_size) unmarshallByte(th);
    else
        _size = (vec_size) unmarshallUnsigned(th);
#else
    vec_size _size = (vec_size) unmarshallUnsigned(th);
#endif

    if (_size == 0)
        return;

#if TAG_MAJOR_VERSION == 34
    if (th.getMinorVersion() < TAG_MINOR_16_BIT_TABLE)
        max_size = static_cast<vec_size>(unmarshallByte(th));
    else
#endif
    max_size      = static_cast<vec_size>(unmarshallUnsigned(th));
    type          = static_cast<store_val_type>(unmarshallByte(th));
    default_flags = static_cast<store_flags>(unmarshallByte(th));

    ASSERT(_size <= max_size);

    vec.resize(_size);

    for (vec_size i = 0; i < _size; i++)
        vec[i].read(th);

    ASSERT_VALIDITY();
}


//////////////////
// Misc functions

store_flags CrawlVector::get_default_flags() const
{
    ASSERT_VALIDITY();
    return default_flags;
}

store_flags CrawlVector::set_default_flags(store_flags flags)
{
    ASSERT_VALIDITY();
    ASSERT(!(flags & SFLAG_UNSET));
    default_flags |= flags;

    return default_flags;
}

store_flags CrawlVector::unset_default_flags(store_flags flags)
{
    ASSERT_VALIDITY();
    ASSERT(!(flags & SFLAG_UNSET));
    default_flags &= ~flags;

    return default_flags;
}

store_val_type CrawlVector::get_type() const
{
    ASSERT_VALIDITY();
    return type;
}

void CrawlVector::assert_validity() const
{
#ifdef DEBUG
    ASSERT(!(default_flags & SFLAG_UNSET));
    ASSERT(max_size > 0);
    ASSERT(max_size >= size());

    for (vec_size i = 0, _size = size(); i < _size; i++)
    {
        const CrawlStoreValue &val = vec[i];

        if (type != SV_NONE)
            ASSERT(val.type == SV_NONE || val.type == type);

        // A vector might be resize()'d and filled up with unset
        // values, which are then set one by one, so we can't
        // assert over that here.
        if (val.type == SV_NONE || (val.flags & SFLAG_UNSET))
            continue;

        switch (val.type)
        {
        case SV_STR:
        case SV_COORD:
        case SV_ITEM:
        case SV_LEV_ID:
        case SV_LEV_POS:
            ASSERT(val.val.ptr != NULL);
            break;

        case SV_HASH:
        {
            ASSERT(val.val.ptr != NULL);

            CrawlHashTable* nested;
            nested = static_cast<CrawlHashTable*>(val.val.ptr);

            nested->assert_validity();
            break;
        }

        case SV_VEC:
        {
            ASSERT(val.val.ptr != NULL);

            CrawlVector* nested;
            nested = static_cast<CrawlVector*>(val.val.ptr);

            nested->assert_validity();
            break;
        }

        default:
            break;
        }
    }
#endif
}

void CrawlVector::set_max_size(vec_size _size)
{
    ASSERT(_size > 0);
    ASSERT(max_size == VEC_MAX_SIZE || max_size < _size);
    max_size = _size;

    vec.reserve(max_size);
}

vec_size CrawlVector::get_max_size() const
{
    return max_size;
}

////////////////////////////////
// Accessors to contained values

CrawlStoreValue& CrawlVector::get_value(const vec_size &index)
{
    ASSERT_VALIDITY();

    ASSERT(index <= max_size);
    ASSERT(index <= vec.size());

    return vec[index];
}

const CrawlStoreValue& CrawlVector::get_value(const vec_size &index) const
{
    ASSERT_VALIDITY();

    ASSERT(index <= max_size);
    ASSERT(index <= vec.size());

    return vec[index];
}

///////////////////////////
// std::vector style interface
vec_size CrawlVector::size() const
{
    return vec.size();
}

bool CrawlVector::empty() const
{
    return vec.empty();
}

CrawlStoreValue& CrawlVector::pop_back()
{
    ASSERT_VALIDITY();
    ASSERT(!vec.empty());

    CrawlStoreValue& val = vec[vec.size() - 1];
    vec.pop_back();
    return val;
}

void CrawlVector::push_back(CrawlStoreValue val)
{
#ifdef DEBUG
    if (type != SV_NONE)
        ASSERT(type == val.type);

    switch (val.type)
    {
    case SV_STR:
    case SV_COORD:
    case SV_ITEM:
    case SV_LEV_ID:
    case SV_LEV_POS:
        ASSERT(val.val.ptr != NULL);
        break;

    case SV_HASH:
    {
        ASSERT(val.val.ptr != NULL);

        CrawlHashTable* nested;
        nested = static_cast<CrawlHashTable*>(val.val.ptr);

        nested->assert_validity();
        break;
    }

    case SV_VEC:
    {
        ASSERT(val.val.ptr != NULL);

        CrawlVector* nested;
        nested = static_cast<CrawlVector*>(val.val.ptr);

        nested->assert_validity();
        break;
    }

    default:
        break;
    }
#endif

    ASSERT_VALIDITY();
    ASSERT(vec.size() < max_size);
    ASSERT(type == SV_NONE
           || (val.type == SV_NONE && (val.flags & SFLAG_UNSET))
           || (val.type == type));
    val.flags |= default_flags;
    if (type != SV_NONE)
    {
        val.type   = type;
        val.flags |= SFLAG_CONST_TYPE;
    }
    vec.push_back(val);
    ASSERT_VALIDITY();
}

void CrawlVector::insert(const vec_size index, CrawlStoreValue val)
{
    ASSERT_VALIDITY();
    ASSERT(vec.size() < max_size);
    ASSERT(type == SV_NONE
           || (val.type == SV_NONE && (val.flags & SFLAG_UNSET))
           || (val.type == type));
    val.flags |= default_flags;
    if (type != SV_NONE)
    {
        val.type   = type;
        val.flags |= SFLAG_CONST_TYPE;
    }
    vec.insert(vec.begin() + index, val);
}

void CrawlVector::resize(const vec_size _size)
{
    ASSERT_VALIDITY();
    ASSERT(max_size == VEC_MAX_SIZE);
    ASSERT(_size < max_size);

    vec_size old_size = size();
    vec.resize(_size);

    for (vec_size i = old_size; i < _size; i++)
    {
        vec[i].flags = SFLAG_UNSET | default_flags;
        vec[i].type  = SV_NONE;
    }
}

void CrawlVector::erase(const vec_size index)
{
    ASSERT_VALIDITY();
    ASSERT(index <= max_size);
    ASSERT(index <= vec.size());

    vec.erase(vec.begin() + index);
}

void CrawlVector::clear()
{
    ASSERT_VALIDITY();
    ASSERT(!(default_flags & SFLAG_NO_ERASE));

    for (vec_size i = 0, _size = size(); i < _size; i++)
        ASSERT(!(vec[i].flags & SFLAG_NO_ERASE));

    vec.clear();
    default_flags = 0;
    type          = SV_NONE;
}

CrawlVector::iterator CrawlVector::begin()
{
    ASSERT_VALIDITY();
    return vec.begin();
}

CrawlVector::iterator CrawlVector::end()
{
    ASSERT_VALIDITY();
    return vec.end();
}

CrawlVector::const_iterator CrawlVector::begin() const
{
    ASSERT_VALIDITY();
    return vec.begin();
}

CrawlVector::const_iterator CrawlVector::end() const
{
    ASSERT_VALIDITY();
    return vec.end();
}


#ifdef DEBUG_PROPS
static bool _cmp(string a, string b)
{
    return accesses[a] > accesses[b];
}

void dump_prop_accesses()
{
    FILE *f = fopen("prop_accesses", "w");
    ASSERT(f);

    vector<string> props;

    for (map<string, int>::const_iterator i = accesses.begin();
         i != accesses.end(); ++i)
    {
        props.push_back(i->first);
    }

    sort(props.begin(), props.end(), _cmp);
    for (vector<string>::const_iterator i = props.begin();
         i != props.end(); ++i)
    {
        fprintf(f, "%10d %s\n", accesses[*i], i->c_str());
    }
    fclose(f);
}
#endif
