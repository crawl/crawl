/*
 *  File:       hash.cc
 *  Summary:    Saveable hash-table capable of storing multiple types
 *              of data.
 *  Written by: Matthew Cline
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):

 *   <1>      10/5/07    MPC    Created
 */

#include "AppHdr.h"
#include "hash.h"

#include "externs.h"
#include "tags.h"

CrawlHashValue::CrawlHashValue()
    : type(HV_NONE), flags(HFLAG_UNSET)
{
    val.ptr = NULL;
}

CrawlHashValue::CrawlHashValue(const CrawlHashValue &other)
{
    ASSERT(other.type >= HV_NONE && other.type < NUM_HASH_VAL_TYPES);

    type  = other.type;
    flags = other.flags;

    if (flags & HFLAG_UNSET)
    {
        val = other.val;
        return;
    }

    switch (type)
    {
    case HV_NONE:
    case HV_BOOL:
    case HV_BYTE:
    case HV_SHORT:
    case HV_LONG:
    case HV_FLOAT:
        val = other.val;
        break;

    case HV_STR:
    {
        std::string* str;
        str = new std::string(*static_cast<std::string*>(other.val.ptr));
        val.ptr = static_cast<void*>(str);
        break;
    }

    case HV_COORD:
    {
        coord_def* coord;
        coord = new coord_def(*static_cast<coord_def*>(other.val.ptr));
        val.ptr = static_cast<void*>(coord);
        break;
    }

    case HV_HASH:
    {
        CrawlHashTable* hash;
        CrawlHashTable* tmp = static_cast<CrawlHashTable*>(other.val.ptr);
        hash = new CrawlHashTable(*tmp);
        val.ptr = static_cast<void*>(hash);
        break;
    }

    case HV_ITEM:
    {
        item_def* item;
        item = new item_def(*static_cast<item_def*>(other.val.ptr));
        val.ptr = static_cast<void*>(item);
        break;
    }

    case NUM_HASH_VAL_TYPES:
        ASSERT(false);
    }
}

CrawlHashValue::CrawlHashValue(const unsigned char _flags,
                               const hash_val_type _type)
    : type(_type), flags(_flags)
{
    ASSERT(type >= HV_NONE && type < NUM_HASH_VAL_TYPES);
    ASSERT(!(flags & HFLAG_UNSET));

    flags   |= HFLAG_UNSET;
    val.ptr  = NULL;
}

CrawlHashValue::~CrawlHashValue()
{
    unset(true);
}

void CrawlHashValue::unset(bool force)
{
    if (flags & HFLAG_UNSET)
        return;

    if (force)
        flags &= ~HFLAG_NO_ERASE;

    ASSERT(!(flags & HFLAG_NO_ERASE));

    switch (type)
    {
    case HV_BOOL:
        val.boolean = false;
        break;

    case HV_BYTE:
        val.byte = 0;
        break;

    case HV_SHORT:
        val._short = 0;
        break;

    case HV_LONG:
        val._long = 0;
        break;

    case HV_FLOAT:
        val._float = 0.0;
        break;

    case HV_STR:
    {
        std::string* str = static_cast<std::string*>(val.ptr);
        delete str;
        val.ptr = NULL;
        break;
    }

    case HV_COORD:
    {
        coord_def* coord = static_cast<coord_def*>(val.ptr);
        delete coord;
        val.ptr = NULL;
        break;
    }

    case HV_HASH:
    {
        CrawlHashTable* hash = static_cast<CrawlHashTable*>(val.ptr);
        delete hash;
        val.ptr = NULL;
        break;
    }

    case HV_ITEM:
    {
        item_def* item = static_cast<item_def*>(val.ptr);
        delete item;
        val.ptr = NULL;
        break;
    }

    case HV_NONE:
        ASSERT(false);
        
    case NUM_HASH_VAL_TYPES:
        ASSERT(false);
    }

    flags |= HFLAG_UNSET;
}

// Only needed to do some assertion checking.
CrawlHashValue &CrawlHashValue::operator = (const CrawlHashValue &other)
{
    ASSERT(other.type >= HV_NONE && other.type < NUM_HASH_VAL_TYPES);
    ASSERT(other.type != HV_NONE || type == HV_NONE);

    // NOTE: We don't bother checking HFLAG_CONST_VAL, since the
    // asignment operator is used when swapping two elements.

    if (!(flags & HFLAG_UNSET))
    {
        if (flags & HFLAG_CONST_TYPE)
            ASSERT(type == HV_NONE || type == other.type);
    }

    type  = other.type;
    flags = other.flags;
    val   = other.val;

    return (*this);
}

///////////////////////////////////
// Meta-data accessors and changers
unsigned char CrawlHashValue::get_flags() const
{
    return flags;
}

unsigned char CrawlHashValue::set_flags(unsigned char _flags)
{
    flags |= _flags;
    return flags;
}

unsigned char CrawlHashValue::unset_flags(unsigned char _flags)
{
    flags &= ~_flags;
    return flags;
}

hash_val_type CrawlHashValue::get_type() const
{
    return type;
}

//////////////////////////////
// Read/write from/to savefile
void CrawlHashValue::write(tagHeader &th) const
{
    ASSERT(!(flags & HFLAG_UNSET));

    marshallByte(th,  (char) type);
    marshallByte(th, (char) flags);

    switch (type)
    {
    case HV_BOOL:
        marshallBoolean(th, val.boolean);
        break;

    case HV_BYTE:
        marshallByte(th, val.byte);
        break;

    case HV_SHORT:
        marshallShort(th, val._short);
        break;

    case HV_LONG:
        marshallLong(th, val._long);
        break;

    case HV_FLOAT:
        marshallFloat(th, val._float);
        break;

    case HV_STR:
    {
        std::string* str = static_cast<std::string*>(val.ptr);
        marshallString(th, *str);
        break;
    }

    case HV_COORD:
    {
        coord_def* coord = static_cast<coord_def*>(val.ptr);
        marshallCoord(th, *coord);
        break;
    }

    case HV_HASH:
    {
        CrawlHashTable* hash = static_cast<CrawlHashTable*>(val.ptr);
        hash->write(th);
        break;
    }

    case HV_ITEM:
    {
        item_def* item = static_cast<item_def*>(val.ptr);
        marshallItem(th, *item);
        break;
    }

    case HV_NONE:
        ASSERT(false);
        
    case NUM_HASH_VAL_TYPES:
        ASSERT(false);
    }
}

void CrawlHashValue::read(tagHeader &th)
{
    type = static_cast<hash_val_type>(unmarshallByte(th));
    flags = (unsigned char) unmarshallByte(th);

    ASSERT(!(flags & HFLAG_UNSET));

    switch (type)
    {
    case HV_BOOL:
        val.boolean = unmarshallBoolean(th);
        break;

    case HV_BYTE:
        val.byte = unmarshallByte(th);
        break;

    case HV_SHORT:
        val._short = unmarshallShort(th);
        break;

    case HV_LONG:
        val._long = unmarshallLong(th);
        break;

    case HV_FLOAT:
        val._float = unmarshallFloat(th);
        break;

    case HV_STR:
    {
        std::string str = unmarshallString(th);
        val.ptr = (void*) new std::string(str);
        break;
    }

    case HV_COORD:
    {
        coord_def coord;
        unmarshallCoord(th, coord);
        val.ptr = (void*) new coord_def(coord);

        break;
    }

    case HV_HASH:
    {
        CrawlHashTable* hash = new CrawlHashTable();
        hash->read(th);
        val.ptr = (void*) hash;

        break;
    }

    case HV_ITEM:
    {
        item_def item;
        unmarshallItem(th, item);
        val.ptr = (void*) new item_def(item);

        break;
    }

    case HV_NONE:
        ASSERT(false);
        
    case NUM_HASH_VAL_TYPES:
        ASSERT(false);
    }
}

////////////////////////////////////////////////////////////////
// Setup a new table with the given flags and/or type; assert if
// a table already exists.
CrawlHashTable &CrawlHashValue::new_table(unsigned char _flags)
{
    return new_table(HV_NONE, flags);
}

CrawlHashTable &CrawlHashValue::new_table(hash_val_type _type,
                                          unsigned char _flags)
{
    CrawlHashTable* old_table = static_cast<CrawlHashTable*>(val.ptr);

    ASSERT(flags & HFLAG_UNSET);
    ASSERT(type == HV_NONE
           || (type == HV_HASH
               && old_table->size() == 0
               && old_table->get_type() == HV_NONE
               && old_table->get_default_flags() == 0));

    CrawlHashTable &table = get_table();

    table.default_flags = _flags;
    table.type          = _type;

    type   =  HV_HASH;
    flags &= ~HFLAG_UNSET;

    return table;
}

///////////////////////////////////////////
// Dynamic type-checking accessor functions
#define GET_VAL(x, _type, field, value)                            \
    ASSERT((flags & HFLAG_UNSET) || !(flags & HFLAG_CONST_VAL)); \
    if (type != (x)) \
    { \
        if (type == HV_NONE) \
        { \
            type  = (x); \
            field = (value); \
        } \
        else \
        { \
            ASSERT(!(flags & HFLAG_CONST_TYPE)); \
            switch(type) \
            { \
            case HV_BOOL: \
                field = (_type) val.boolean; \
                break; \
            case HV_BYTE: \
                field = (_type) val.byte; \
                break; \
            case HV_SHORT: \
                field = (_type) val._short; \
                break; \
            case HV_LONG: \
                field = (_type) val._long; \
                break; \
            case HV_FLOAT: \
                field = (_type) val._float; \
                break; \
            default: \
                ASSERT(false); \
            } \
            type = (x); \
        } \
    } \
    flags &= ~HFLAG_UNSET; \
    return field;

#define GET_VAL_PTR(x, _type, value) \
    ASSERT((flags & HFLAG_UNSET) || !(flags & HFLAG_CONST_VAL)); \
    if (type != (x)) \
    { \
        if (type == HV_NONE) \
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
    flags &= ~HFLAG_UNSET; \
    return *((_type) val.ptr);

bool &CrawlHashValue::get_bool()
{
    GET_VAL(HV_BOOL, bool, val.boolean, false);
}

char &CrawlHashValue::get_byte()
{
    GET_VAL(HV_BYTE, char, val.byte, 0);
}

short &CrawlHashValue::get_short()
{
    GET_VAL(HV_SHORT, short, val._short, 0);
}

long &CrawlHashValue::get_long()
{
    GET_VAL(HV_LONG, long, val._long, 0);
}

float &CrawlHashValue::get_float()
{
    GET_VAL(HV_FLOAT, float, val._float, 0.0);
}

std::string &CrawlHashValue::get_string()
{
    GET_VAL_PTR(HV_STR, std::string*, new std::string(""));
}

coord_def &CrawlHashValue::get_coord()
{
    GET_VAL_PTR(HV_COORD, coord_def*, new coord_def());
}

CrawlHashTable &CrawlHashValue::get_table()
{
    GET_VAL_PTR(HV_HASH, CrawlHashTable*, new CrawlHashTable());
}

item_def &CrawlHashValue::get_item()
{
    GET_VAL_PTR(HV_ITEM, item_def*, new item_def());
}

///////////////////////////
// Const accessor functions
#define GET_CONST_SETUP(x) \
    ASSERT(!(flags & HFLAG_UNSET)); \
    ASSERT(type == (x));

bool CrawlHashValue::get_bool() const
{
    GET_CONST_SETUP(HV_BOOL);
    return val.boolean;
}

char CrawlHashValue::get_byte() const
{
    GET_CONST_SETUP(HV_BYTE);
    return val.byte;
}

short CrawlHashValue::get_short() const
{
    GET_CONST_SETUP(HV_SHORT);
    return val._short;
}

long CrawlHashValue::get_long() const
{
    GET_CONST_SETUP(HV_LONG);
    return val._long;
}

float CrawlHashValue::get_float() const
{
    GET_CONST_SETUP(HV_FLOAT);
    return val._float;
}

std::string CrawlHashValue::get_string() const
{
    GET_CONST_SETUP(HV_STR);
    return *((std::string*)val.ptr);
}

coord_def CrawlHashValue::get_coord() const
{
    GET_CONST_SETUP(HV_COORD);
    return *((coord_def*)val.ptr);
}

const CrawlHashTable& CrawlHashValue::get_table() const
{
    GET_CONST_SETUP(HV_HASH);
    return *((CrawlHashTable*)val.ptr);
}

const item_def& CrawlHashValue::get_item() const
{
    GET_CONST_SETUP(HV_ITEM);
    return *((item_def*)val.ptr);
}

/////////////////////
// Typecast operators
&CrawlHashValue::operator bool()
{
    return get_bool();
}

&CrawlHashValue::operator char()
{
    return get_byte();
}

&CrawlHashValue::operator short()
{
    return get_short();
}

&CrawlHashValue::operator float()
{
    return get_float();
}

&CrawlHashValue::operator long()
{
    return get_long();
}

&CrawlHashValue::operator std::string()
{
    return get_string();
}

&CrawlHashValue::operator coord_def()
{
    return get_coord();
}

&CrawlHashValue::operator CrawlHashTable()
{
    return get_table();
}

&CrawlHashValue::operator item_def()
{
    return get_item();
}

///////////////////////////
// Const typecast operators
CrawlHashValue::operator bool() const
{
    return get_bool();
}

#define CONST_INT_CAST() \
    switch(type) \
    { \
    case HV_BYTE: \
        return get_byte(); \
    case HV_SHORT: \
        return get_short(); \
    case HV_LONG: \
        return get_long(); \
    default: \
        ASSERT(false); \
        return 0; \
    }

CrawlHashValue::operator char() const
{
    CONST_INT_CAST();
}

CrawlHashValue::operator short() const
{
    CONST_INT_CAST();
}

CrawlHashValue::operator long() const
{
    CONST_INT_CAST();
}

CrawlHashValue::operator float() const
{
    return get_float();
}

CrawlHashValue::operator std::string() const
{
    return get_string();
}

CrawlHashValue::operator coord_def() const
{
    return get_coord();
}

///////////////////////
// Assignment operators
bool &CrawlHashValue::operator = (const bool &_val)
{
    return (get_bool() = _val);
}

char &CrawlHashValue::operator = (const char &_val)
{
    return (get_byte() = _val);
}

short &CrawlHashValue::operator = (const short &_val)
{
    return (get_short() = _val);
}

long &CrawlHashValue::operator = (const long &_val)
{
    return (get_long() = _val);
}

float &CrawlHashValue::operator = (const float &_val)
{
    return (get_float() = _val);
}

std::string &CrawlHashValue::operator = (const std::string &_val)
{
    return (get_string() = _val);
}

const char* CrawlHashValue::operator = (const char* _val)
{
    get_string() = _val;
    return get_string().c_str();
}

coord_def &CrawlHashValue::operator = (const coord_def &_val)
{
    return (get_coord() = _val);
}

CrawlHashTable &CrawlHashValue::operator = (const CrawlHashTable &_val)
{
    return (get_table() = _val);
}

item_def &CrawlHashValue::operator = (const item_def &_val)
{
    return (get_item() = _val);
}

///////////////////////////////////////////////////
// Non-assignment operators which affect the lvalue
#define INT_OPERATOR_UNARY(op) \
    switch(type) \
    { \
    case HV_BYTE: \
    { \
        char &temp = get_byte(); \
        temp op; \
        return temp; \
    } \
 \
    case HV_SHORT: \
    { \
        short &temp = get_short(); \
        temp op; \
        return temp; \
    } \
    case HV_LONG: \
    { \
        long &temp = get_long(); \
        temp op; \
        return temp; \
    } \
 \
    default: \
        ASSERT(false); \
        return 0; \
    }

// Prefix
long CrawlHashValue::operator ++ ()
{
    INT_OPERATOR_UNARY(++);
}

long CrawlHashValue::operator -- ()
{
    INT_OPERATOR_UNARY(--);
}

// Postfix
long CrawlHashValue::operator ++ (int)
{
    INT_OPERATOR_UNARY(++);
}

long CrawlHashValue::operator -- (int)
{
    INT_OPERATOR_UNARY(--);
}

std::string &CrawlHashValue::operator += (const std::string &_val)
{
    return (get_string() += _val);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

CrawlHashTable::CrawlHashTable()
    : type(HV_NONE), default_flags(0)
{
}

CrawlHashTable::CrawlHashTable(unsigned char flags)
    : type(HV_NONE), default_flags(flags)
{
    ASSERT(!(default_flags & HFLAG_UNSET));
}

CrawlHashTable::CrawlHashTable(hash_val_type _type, unsigned char flags)
    : type(_type), default_flags(flags)
{
    ASSERT(type >= HV_NONE && type < NUM_HASH_VAL_TYPES);
    ASSERT(!(default_flags & HFLAG_UNSET));
}

CrawlHashTable::~CrawlHashTable()
{
    assert_validity();
}

//////////////////////////////
// Read/write from/to savefile
void CrawlHashTable::write(tagHeader &th) const
{
    assert_validity();
    if (empty())
    {
        marshallByte(th, 0);
        return;
    }

    marshallByte(th, size());
    marshallByte(th, static_cast<char>(type));
    marshallByte(th, (char) default_flags);

    CrawlHashTable::hash_map_type::const_iterator i = hash_map.begin();

    for (; i != hash_map.end(); i++)
    {
        marshallString(th, i->first);
        i->second.write(th);
    }

    assert_validity();
}

void CrawlHashTable::read(tagHeader &th)
{
    assert_validity();

    ASSERT(empty());
    ASSERT(type == HV_NONE);
    ASSERT(default_flags == 0);

    unsigned char _size = (unsigned char) unmarshallByte(th);

    if (_size == 0)
        return;

    type          = static_cast<hash_val_type>(unmarshallByte(th));
    default_flags = (unsigned char) unmarshallByte(th);

    for (unsigned char i = 0; i < _size; i++)
    {
        std::string    key  = unmarshallString(th);
        CrawlHashValue &val = (*this)[key];

        val.read(th);
    }

    assert_validity();
}


//////////////////
// Misc functions

unsigned char CrawlHashTable::get_default_flags() const
{
    assert_validity();
    return default_flags;
}

unsigned char CrawlHashTable::set_default_flags(unsigned char flags)
{
    assert_validity();
    ASSERT(!(flags & HFLAG_UNSET));
    default_flags |= flags;

    return default_flags;
}

unsigned char CrawlHashTable::unset_default_flags(unsigned char flags)
{
    assert_validity();
    ASSERT(!(flags & HFLAG_UNSET));
    default_flags &= ~flags;

    return default_flags;
}

hash_val_type CrawlHashTable::get_type() const
{
    assert_validity();
    return type;
}

bool CrawlHashTable::exists(const std::string key) const
{
    assert_validity();
    hash_map_type::const_iterator i = hash_map.find(key);

    return (i != hash_map.end());
}

bool CrawlHashTable::exists(const long index) const
{
    char buf[12];
    sprintf(buf, "%ld", index);
    return exists(buf); 
}

void CrawlHashTable::assert_validity() const
{
#if DEBUG
    ASSERT(!(default_flags & HFLAG_UNSET));

    hash_map_type::const_iterator i = hash_map.begin();

    unsigned long actual_size = 0;

    for (; i != hash_map.end(); i++)
    {
        actual_size++;

        const std::string    &key = i->first;
        const CrawlHashValue &val = i->second;

        ASSERT(key != "");
        std::string trimmed = trimmed_string(key);
        ASSERT(key == trimmed);

        ASSERT(val.type != HV_NONE);
        ASSERT(!(val.flags & HFLAG_UNSET));

        switch(val.type)
        {
        case HV_STR:
        case HV_COORD:
        case HV_ITEM:
            ASSERT(val.val.ptr != NULL);
            break;

        case HV_HASH:
        {
            ASSERT(val.val.ptr != NULL);

            CrawlHashTable* nested;
            nested = static_cast<CrawlHashTable*>(val.val.ptr);

            nested->assert_validity();
            break;
        }

        default:
            break;
        }
    }

    ASSERT(size() == actual_size);

    ASSERT(size() <= 255);
#endif
}

int CrawlHashTable::compact_indicies(long min_index, long max_index,
                                     bool compact_down)
{
    ASSERT(min_index <= max_index);

    if (min_index == max_index)
        return 0;

    long min_exists, max_exists;

    for (min_exists = min_index; min_exists <= max_index; min_exists++)
        if (exists(min_exists))
            break;

    if (min_exists > max_index)
        return 0;

    for (max_exists = max_index; max_exists >= min_exists; max_exists--)
        if (exists(max_exists))
            break;

    if (max_exists <= min_exists)
        return 0;

    bool hole_found = false;
    for (long i = min_exists; i < max_exists; i++)
        if (!exists(i))
        {
            hole_found = true;
            break;
        }

    if (!hole_found)
        return 0;

    long start, stop, dir;
    if (compact_down)
    {
        start = min_exists;
        stop  = max_exists;
        dir   = +1;
    }
    else
    {
        start = max_exists;
        stop  = min_exists;
        dir   = -1;
    }
    stop += dir;

    long move = 0;
    for (long i = start; i != stop; i += dir)
    {
        if (!exists(i))
        {
            move++;
            continue;
        }

        if (move == 0)
            continue;

        char buf1[12], buf2[12];
        sprintf(buf1, "%ld", i - (move * dir));
        sprintf(buf2, "%ld", i);

        CrawlHashValue &val = hash_map[buf2];
        hash_map[buf1] = val;
            
        // Ensure a new()'d object isn't freed, since the other
        // CrawlHashValue now owns it.
        val.type = HV_BOOL;
        erase(buf2);
    }

    return move;
}

bool CrawlHashTable::fixup_indexed_array(std::string name)
{
    bool problems  = false;
    long max_index = 0;
    std::vector<std::string> bad_keys;
    for (hash_map_type::iterator i = begin(); i != end(); i++)
    {
        int index = strtol(i->first.c_str(), NULL, 10);

        if (index < 0)
        {
            if (name != "")
                mprf("Negative index %ld found in %s.",
                     index, name.c_str());
            problems = true;
            bad_keys.push_back(i->first);
        }
        else if (index == 0 && i->first != "0")
        {
            if (name != "")
                mprf("Non-numerical index '%s' found in %s.",
                     i->first.c_str(), name.c_str());
            problems = true;
            bad_keys.push_back(i->first);
        }
        else if (index > max_index)
            max_index = index;
    }
    for (unsigned long i = 0, _size = bad_keys.size(); i < _size; i++)
        erase(bad_keys[i]);
    int holes = compact_indicies(0, max_index);
    if (holes > 0 && name != "")
        mprf("%d holes found in %s.", holes, name.c_str());

    return problems;
}

////////////
// Accessors

CrawlHashValue& CrawlHashTable::get_value(const std::string &key)
{
    assert_validity();
    hash_map_type::iterator i = hash_map.find(key);

    if (i == hash_map.end())
    {
        hash_map[key]       = CrawlHashValue(default_flags);
        CrawlHashValue &val = hash_map[key];

        if (type != HV_NONE)
        {
            val.type   = type;
            val.flags |= HFLAG_CONST_TYPE;
        }

        return (val);
    }

    return (i->second);
}

CrawlHashValue& CrawlHashTable::get_value(const long &index)
{
    char buf[12];
    sprintf(buf, "%ld", index);
    return get_value(buf);
}

const CrawlHashValue& CrawlHashTable::get_value(const std::string &key) const
{
    assert_validity();
    hash_map_type::const_iterator i = hash_map.find(key);

    ASSERT(i != hash_map.end());
    ASSERT(i->second.type != HV_NONE);
    ASSERT(!(i->second.flags & HFLAG_UNSET));

    return (i->second);
}

const CrawlHashValue& CrawlHashTable::get_value(const long &index) const
{
    char buf[12];
    sprintf(buf, "%ld", index);
    return get_value(buf);
}

CrawlHashValue& CrawlHashTable::operator[] (const std::string &key)
{
    return get_value(key);
}

CrawlHashValue& CrawlHashTable::operator[] (const long &index)
{
    return get_value(index);
}

const CrawlHashValue& CrawlHashTable::operator[] (const std::string &key) const
{
    return get_value(key);
}

const CrawlHashValue& CrawlHashTable::operator[] (const long &index) const
{
    return get_value(index);
}

///////////////////////////
// std::map style interface
size_t CrawlHashTable::size() const
{
    return hash_map.size();
}

bool CrawlHashTable::empty() const
{
    return hash_map.empty();
}

void CrawlHashTable::erase(const std::string key)
{
    assert_validity();
    hash_map_type::iterator i = hash_map.find(key);

    if (i != hash_map.end())
    {
        CrawlHashValue &val = i->second;

        ASSERT(!(val.flags & HFLAG_NO_ERASE));

        hash_map.erase(i);
    }
}

void CrawlHashTable::erase(const long index)
{
    char buf[12];
    sprintf(buf, "%ld", index);
    erase(buf);
}

void CrawlHashTable::clear()
{
    assert_validity();
    ASSERT(!(default_flags & HFLAG_NO_ERASE));

    hash_map_type::iterator i = hash_map.begin();
    for (; i != hash_map.end(); i++)
        ASSERT(!(i->second.flags & HFLAG_NO_ERASE));

    hash_map.clear();
}

CrawlHashTable::hash_map_type::iterator CrawlHashTable::begin()
{
    assert_validity();
    return hash_map.begin();
}

CrawlHashTable::hash_map_type::iterator CrawlHashTable::end()
{
    assert_validity();
    return hash_map.end();
}

CrawlHashTable::hash_map_type::const_iterator CrawlHashTable::begin() const
{
    assert_validity();
    return hash_map.begin();
}

CrawlHashTable::hash_map_type::const_iterator CrawlHashTable::end() const
{
    assert_validity();
    return hash_map.end();
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

template <typename T, hash_val_type TYPE>
CrawlTableWrapper<T, TYPE>::CrawlTableWrapper()
{
    table = NULL;
}

template <typename T, hash_val_type TYPE>
CrawlTableWrapper<T, TYPE>::CrawlTableWrapper(CrawlHashTable& _table)
{
    wrap(_table);
}

template <typename T, hash_val_type TYPE>
CrawlTableWrapper<T, TYPE>::CrawlTableWrapper(CrawlHashTable* _table)
{
    wrap(_table);
}

template <typename T, hash_val_type TYPE>
void CrawlTableWrapper<T, TYPE>::wrap(CrawlHashTable& _table)
{
    wrap(&_table);
}

template <typename T, hash_val_type TYPE>
void CrawlTableWrapper<T, TYPE>::wrap(CrawlHashTable* _table)
{
    ASSERT(_table != NULL);
    ASSERT(_table->get_type() == TYPE);

    table = _table;
}

template <typename T, hash_val_type TYPE>
T& CrawlTableWrapper<T, TYPE>::operator[] (const std::string &key)
{
    return (T&) (*table)[key];
}

template <typename T, hash_val_type TYPE>
T& CrawlTableWrapper<T, TYPE>::operator[] (const long &index)
{
    return (T&) (*table)[index];
}

template <typename T, hash_val_type TYPE>
const T CrawlTableWrapper<T, TYPE>::operator[] (const std::string &key) const
{
    return (T) (*table)[key];
}

template <typename T, hash_val_type TYPE>
const T CrawlTableWrapper<T, TYPE>::operator[] (const long &index) const
{
    return (T) (*table)[index];
}
