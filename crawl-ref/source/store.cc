/*
 *  File:       store.cc
 *  Summary:    Saveable hash-table and vector capable of storing
 *              multiple types of data.
 *  Written by: Matthew Cline
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "store.h"

#include "externs.h"
#include "tags.h"

CrawlStoreValue::CrawlStoreValue()
    : type(SV_NONE), flags(SFLAG_UNSET)
{
    val.ptr = NULL;
}

CrawlStoreValue::CrawlStoreValue(const CrawlStoreValue &other)
{
    ASSERT(other.type >= SV_NONE && other.type < NUM_STORE_VAL_TYPES);

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
    case SV_LONG:
    case SV_FLOAT:
        val = other.val;
        break;

    case SV_STR:
    {
        std::string* str;
        str = new std::string(*static_cast<std::string*>(other.val.ptr));
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

    case NUM_STORE_VAL_TYPES:
        ASSERT(false);
    }
}

CrawlStoreValue::CrawlStoreValue(const store_flags _flags,
                                 const store_val_type _type)
    : type(_type), flags(_flags)
{
    ASSERT(type >= SV_NONE && type < NUM_STORE_VAL_TYPES);
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

CrawlStoreValue::CrawlStoreValue(const long &_val)
    : type(SV_LONG), flags(SFLAG_UNSET)
{
    get_long() = _val;
}

CrawlStoreValue::CrawlStoreValue(const float &_val)
    : type(SV_FLOAT), flags(SFLAG_UNSET)
{
    get_float() = _val;
}

CrawlStoreValue::CrawlStoreValue(const std::string &_val)
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

    case SV_LONG:
        val._long = 0;
        break;

    case SV_FLOAT:
        val._float = 0.0;
        break;

    case SV_STR:
    {
        std::string* str = static_cast<std::string*>(val.ptr);
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

    case SV_NONE:
        ASSERT(false);

    case NUM_STORE_VAL_TYPES:
        ASSERT(false);
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
    ASSERT(other.type >= SV_NONE && other.type < NUM_STORE_VAL_TYPES);
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

    switch(type)
    {
    case SV_NONE:
        break;

    case SV_BOOL:
    case SV_BYTE:
    case SV_SHORT:
    case SV_LONG:
    case SV_FLOAT:
        val = other.val;
        break;

    case SV_STR:
        COPY_PTR(std::string);
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

    case NUM_STORE_VAL_TYPES:
        DEBUGSTR("CrawlStoreValue has type NUM_STORE_VAL_TYPES");
        break;
    }

    return (*this);
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
    ASSERT(!(flags & SFLAG_UNSET));

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

    case SV_LONG:
        marshallLong(th, val._long);
        break;

    case SV_FLOAT:
        marshallFloat(th, val._float);
        break;

    case SV_STR:
    {
        std::string* str = static_cast<std::string*>(val.ptr);
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

    case SV_NONE:
        ASSERT(false);

    case NUM_STORE_VAL_TYPES:
        ASSERT(false);
    }
}

void CrawlStoreValue::read(reader &th)
{
    type = static_cast<store_val_type>(unmarshallByte(th));
    flags = (store_flags) unmarshallByte(th);

    ASSERT(!(flags & SFLAG_UNSET));

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

    case SV_LONG:
        val._long = unmarshallLong(th);
        break;

    case SV_FLOAT:
        val._float = unmarshallFloat(th);
        break;

    case SV_STR:
    {
        std::string str = unmarshallString(th);
        val.ptr = (void*) new std::string(str);
        break;
    }

    case SV_COORD:
    {
        coord_def coord;
        unmarshallCoord(th, coord);
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

    case SV_NONE:
        ASSERT(false);

    case NUM_STORE_VAL_TYPES:
        ASSERT(false);
    }
}

////////////////////////////////////////////////////////////////
// Setup a new table with the given flags and/or type; assert if
// a table already exists.
CrawlHashTable &CrawlStoreValue::new_table(store_flags _flags)
{
    return new_table(SV_NONE, flags);
}

CrawlHashTable &CrawlStoreValue::new_table(store_val_type _type,
                                          store_flags _flags)
{
#if DEBUG
    CrawlHashTable* old_table = static_cast<CrawlHashTable*>(val.ptr);

    ASSERT(flags & SFLAG_UNSET);
    ASSERT(type == SV_NONE
           || (type == SV_HASH
               && old_table->size() == 0
               && old_table->get_type() == SV_NONE
               && old_table->get_default_flags() == 0));
#endif

    CrawlHashTable &table = get_table();

    table.default_flags = _flags;
    table.type          = _type;

    type   =  SV_HASH;
    flags &= ~SFLAG_UNSET;

    return table;
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
#if DEBUG
    CrawlVector* old_vector = static_cast<CrawlVector*>(val.ptr);

    ASSERT(flags & SFLAG_UNSET);
    ASSERT(type == SV_NONE
           || (type == SV_VEC
               && old_vector->size() == 0
               && old_vector->get_type() == SV_NONE
               && old_vector->get_default_flags() == 0
               && old_vector->get_max_size() == VEC_MAX_SIZE));
#endif

    CrawlVector &vector = get_vector();

    vector.default_flags = _flags;
    vector.type          = _type;

    type   =  SV_VEC;
    flags &= ~SFLAG_UNSET;

    return vector;
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
            switch(type) \
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
            case SV_LONG: \
                field = (_type) val._long; \
                break; \
            case SV_FLOAT: \
                field = (_type) val._float; \
                break; \
            default: \
                ASSERT(false); \
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

long &CrawlStoreValue::get_long()
{
    GET_VAL(SV_LONG, long, val._long, 0);
}

float &CrawlStoreValue::get_float()
{
    GET_VAL(SV_FLOAT, float, val._float, 0.0);
}

std::string &CrawlStoreValue::get_string()
{
    GET_VAL_PTR(SV_STR, std::string*, new std::string(""));
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

CrawlStoreValue &CrawlStoreValue::operator [] (const std::string &key)
{
    return get_table()[key];
}

CrawlStoreValue &CrawlStoreValue::operator [] (const vec_size &index)
{
    return get_vector()[index];
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

long CrawlStoreValue::get_long() const
{
    GET_CONST_SETUP(SV_LONG);
    return val._long;
}

float CrawlStoreValue::get_float() const
{
    GET_CONST_SETUP(SV_FLOAT);
    return val._float;
}

std::string CrawlStoreValue::get_string() const
{
    GET_CONST_SETUP(SV_STR);
    return *((std::string*)val.ptr);
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

const CrawlStoreValue &CrawlStoreValue::operator
    [] (const std::string &key) const
{
    return get_table()[key];
}

const CrawlStoreValue &CrawlStoreValue::operator
    [](const vec_size &index) const
{
    return get_vector()[index];
}

/////////////////////
// Typecast operators
#if _MSC_VER
CrawlStoreValue::operator bool&()                       { return get_bool();   }
CrawlStoreValue::operator char&()                       { return get_byte();   }
CrawlStoreValue::operator short&()                      { return get_short();  }
CrawlStoreValue::operator float&()                      { return get_float();  }
CrawlStoreValue::operator long&()                       { return get_long();   }
CrawlStoreValue::operator std::string&()                { return get_string(); }
CrawlStoreValue::operator coord_def&()                  { return get_coord();  }
CrawlStoreValue::operator CrawlHashTable&()             { return get_table();  }
CrawlStoreValue::operator CrawlVector&()                { return get_vector(); }
CrawlStoreValue::operator item_def&()                   { return get_item();   }
#else
&CrawlStoreValue::operator bool()
{
    return get_bool();
}

&CrawlStoreValue::operator char()
{
    return get_byte();
}

&CrawlStoreValue::operator short()
{
    return get_short();
}

&CrawlStoreValue::operator float()
{
    return get_float();
}

&CrawlStoreValue::operator long()
{
    return get_long();
}

&CrawlStoreValue::operator std::string()
{
    return get_string();
}

&CrawlStoreValue::operator coord_def()
{
    return get_coord();
}

&CrawlStoreValue::operator CrawlHashTable()
{
    return get_table();
}

&CrawlStoreValue::operator CrawlVector()
{
    return get_vector();
}

&CrawlStoreValue::operator item_def()
{
    return get_item();
}
#endif

///////////////////////////
// Const typecast operators
CrawlStoreValue::operator bool() const
{
    return get_bool();
}

#define CONST_INT_CAST() \
    switch(type) \
    { \
    case SV_BYTE: \
        return get_byte(); \
    case SV_SHORT: \
        return get_short(); \
    case SV_LONG: \
        return get_long(); \
    default: \
        ASSERT(false); \
        return 0; \
    }

CrawlStoreValue::operator char() const
{
    CONST_INT_CAST();
}

CrawlStoreValue::operator short() const
{
    CONST_INT_CAST();
}

CrawlStoreValue::operator long() const
{
    CONST_INT_CAST();
}

CrawlStoreValue::operator float() const
{
    return get_float();
}

CrawlStoreValue::operator std::string() const
{
    return get_string();
}

CrawlStoreValue::operator coord_def() const
{
    return get_coord();
}

///////////////////////
// Assignment operators
CrawlStoreValue &CrawlStoreValue::operator = (const bool &_val)
{
    get_bool() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const char &_val)
{
    get_byte() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const short &_val)
{
    get_short() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const long &_val)
{
    get_long() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const float &_val)
{
    get_float() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const std::string &_val)
{
    get_string() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const char* _val)
{
    get_string() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const coord_def &_val)
{
    get_coord() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const CrawlHashTable &_val)
{
    get_table() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const CrawlVector &_val)
{
    get_vector() = _val;
    return (*this);
}

CrawlStoreValue &CrawlStoreValue::operator = (const item_def &_val)
{
    get_item() = _val;
    return (*this);
}

///////////////////////////////////////////////////
// Non-assignment operators which affect the lvalue
#define INT_OPERATOR_UNARY(op) \
    switch(type) \
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
    case SV_LONG: \
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
long CrawlStoreValue::operator ++ ()
{
    INT_OPERATOR_UNARY(++);
}

long CrawlStoreValue::operator -- ()
{
    INT_OPERATOR_UNARY(--);
}

// Postfix
long CrawlStoreValue::operator ++ (int)
{
    INT_OPERATOR_UNARY(++);
}

long CrawlStoreValue::operator -- (int)
{
    INT_OPERATOR_UNARY(--);
}

std::string &CrawlStoreValue::operator += (const std::string &_val)
{
    return (get_string() += _val);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

CrawlHashTable::CrawlHashTable()
    : type(SV_NONE), default_flags(0)
{
}

CrawlHashTable::CrawlHashTable(store_flags flags)
    : type(SV_NONE), default_flags(flags)
{
    ASSERT(!(default_flags & SFLAG_UNSET));
}

CrawlHashTable::CrawlHashTable(store_val_type _type, store_flags flags)
    : type(_type), default_flags(flags)
{
    ASSERT(type >= SV_NONE && type < NUM_STORE_VAL_TYPES);
    ASSERT(!(default_flags & SFLAG_UNSET));
}

CrawlHashTable::~CrawlHashTable()
{
    assert_validity();
}

//////////////////////////////
// Read/write from/to savefile
void CrawlHashTable::write(writer &th) const
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

void CrawlHashTable::read(reader &th)
{
    assert_validity();

    ASSERT(empty());
    ASSERT(type == SV_NONE);
    ASSERT(default_flags == 0);

    hash_size _size = (hash_size) unmarshallByte(th);

    if (_size == 0)
        return;

    type          = static_cast<store_val_type>(unmarshallByte(th));
    default_flags = (store_flags) unmarshallByte(th);

    for (hash_size i = 0; i < _size; i++)
    {
        std::string      key = unmarshallString(th);
        CrawlStoreValue &val = (*this)[key];

        val.read(th);
    }

    assert_validity();
}


//////////////////
// Misc functions

store_flags CrawlHashTable::get_default_flags() const
{
    assert_validity();
    return default_flags;
}

store_flags CrawlHashTable::set_default_flags(store_flags flags)
{
    assert_validity();
    ASSERT(!(flags & SFLAG_UNSET));
    default_flags |= flags;

    return default_flags;
}

store_flags CrawlHashTable::unset_default_flags(store_flags flags)
{
    assert_validity();
    ASSERT(!(flags & SFLAG_UNSET));
    default_flags &= ~flags;

    return default_flags;
}

store_val_type CrawlHashTable::get_type() const
{
    assert_validity();
    return type;
}

bool CrawlHashTable::exists(const std::string &key) const
{
    assert_validity();
    hash_map_type::const_iterator i = hash_map.find(key);

    return (i != hash_map.end());
}

void CrawlHashTable::assert_validity() const
{
#if DEBUG
    ASSERT(!(default_flags & SFLAG_UNSET));

    hash_map_type::const_iterator i = hash_map.begin();

    unsigned long actual_size = 0;

    for (; i != hash_map.end(); i++)
    {
        actual_size++;

        const std::string    &key = i->first;
        const CrawlStoreValue &val = i->second;

        ASSERT(!key.empty());
        std::string trimmed = trimmed_string(key);
        ASSERT(key == trimmed);

        ASSERT(val.type != SV_NONE);
        ASSERT(!(val.flags & SFLAG_UNSET));

        switch(val.type)
        {
        case SV_STR:
        case SV_COORD:
        case SV_ITEM:
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

        default:
            break;
        }
    }

    ASSERT(size() == actual_size);
#endif
}

////////////////////////////////
// Accessors to contained values

CrawlStoreValue& CrawlHashTable::get_value(const std::string &key)
{
    assert_validity();
    iterator i = hash_map.find(key);

    if (i == hash_map.end())
    {
        hash_map[key]        = CrawlStoreValue(default_flags);
        CrawlStoreValue &val = hash_map[key];

        if (type != SV_NONE)
        {
            val.type   = type;
            val.flags |= SFLAG_CONST_TYPE;
        }

        return (val);
    }

    return (i->second);
}

const CrawlStoreValue& CrawlHashTable::get_value(const std::string &key) const
{
    assert_validity();
    hash_map_type::const_iterator i = hash_map.find(key);

    ASSERT(i != hash_map.end());
    ASSERT(i->second.type != SV_NONE);
    ASSERT(!(i->second.flags & SFLAG_UNSET));

    return (i->second);
}

CrawlStoreValue& CrawlHashTable::operator[] (const std::string &key)
{
    return get_value(key);
}

const CrawlStoreValue& CrawlHashTable::operator[] (const std::string &key)
    const
{
    return get_value(key);
}

///////////////////////////
// std::map style interface
hash_size CrawlHashTable::size() const
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
    iterator i = hash_map.find(key);

    if (i != hash_map.end())
    {
#if DEBUG
        CrawlStoreValue &val = i->second;
        ASSERT(!(val.flags & SFLAG_NO_ERASE));
#endif

        hash_map.erase(i);
    }
}

void CrawlHashTable::clear()
{
    assert_validity();
    ASSERT(!(default_flags & SFLAG_NO_ERASE));

    iterator i = hash_map.begin();
    for (; i != hash_map.end(); i++)
        ASSERT(!(i->second.flags & SFLAG_NO_ERASE));

    hash_map.clear();
    default_flags = 0;
    type          = SV_NONE;
}

CrawlHashTable::iterator CrawlHashTable::begin()
{
    assert_validity();
    return hash_map.begin();
}

CrawlHashTable::iterator CrawlHashTable::end()
{
    assert_validity();
    return hash_map.end();
}

CrawlHashTable::const_iterator CrawlHashTable::begin() const
{
    assert_validity();
    return hash_map.begin();
}

CrawlHashTable::const_iterator CrawlHashTable::end() const
{
    assert_validity();
    return hash_map.end();
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
    ASSERT(type >= SV_NONE && type < NUM_STORE_VAL_TYPES);
    ASSERT(!(default_flags & SFLAG_UNSET));
    ASSERT(max_size > 0);
}

CrawlVector::~CrawlVector()
{
    assert_validity();
}

//////////////////////////////
// Read/write from/to savefile
void CrawlVector::write(writer &th) const
{
    assert_validity();
    if (empty())
    {
        marshallByte(th, 0);
        return;
    }

    marshallByte(th, (char) size());
    marshallByte(th, (char) max_size);
    marshallByte(th, static_cast<char>(type));
    marshallByte(th, (char) default_flags);

    for (vec_size i = 0; i < size(); i++)
    {
        CrawlStoreValue val = vector[i];
        ASSERT(val.type != SV_NONE);
        ASSERT(!(val.flags & SFLAG_UNSET));
        val.write(th);
    }

    assert_validity();
}

void CrawlVector::read(reader &th)
{
    assert_validity();

    ASSERT(empty());
    ASSERT(type == SV_NONE);
    ASSERT(default_flags == 0);
    ASSERT(max_size == VEC_MAX_SIZE);

    vec_size _size = (vec_size) unmarshallByte(th);

    if (_size == 0)
        return;

    max_size      = static_cast<vec_size>(unmarshallByte(th));
    type          = static_cast<store_val_type>(unmarshallByte(th));
    default_flags = static_cast<store_flags>(unmarshallByte(th));

    ASSERT(_size <= max_size);

    vector.resize(_size);

    for (vec_size i = 0; i < _size; i++)
        vector[i].read(th);

    assert_validity();
}


//////////////////
// Misc functions

store_flags CrawlVector::get_default_flags() const
{
    assert_validity();
    return default_flags;
}

store_flags CrawlVector::set_default_flags(store_flags flags)
{
    assert_validity();
    ASSERT(!(flags & SFLAG_UNSET));
    default_flags |= flags;

    return default_flags;
}

store_flags CrawlVector::unset_default_flags(store_flags flags)
{
    assert_validity();
    ASSERT(!(flags & SFLAG_UNSET));
    default_flags &= ~flags;

    return default_flags;
}

store_val_type CrawlVector::get_type() const
{
    assert_validity();
    return type;
}

void CrawlVector::assert_validity() const
{
#if DEBUG
    ASSERT(!(default_flags & SFLAG_UNSET));
    ASSERT(max_size > 0);
    ASSERT(max_size >= size());

    for (vec_size i = 0, _size = size(); i < _size; i++)
    {
        const CrawlStoreValue &val = vector[i];

        // A vector might be resize()'d and filled up with unset
        // values, which are then set one by one, so we can't
        // assert over that here.
        if (val.type == SV_NONE || (val.flags & SFLAG_UNSET))
            continue;

        switch(val.type)
        {
        case SV_STR:
        case SV_COORD:
        case SV_ITEM:
            ASSERT(val.val.ptr != NULL);
            break;

        case SV_HASH:
        {
            ASSERT(val.val.ptr != NULL);

            CrawlVector* nested;
            nested = static_cast<CrawlVector*>(val.val.ptr);

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
    ASSERT(max_size == VEC_MAX_SIZE || max_size == _size);
    max_size = _size;

    vector.reserve(max_size);
}

vec_size CrawlVector::get_max_size() const
{
    return max_size;
}


////////////////////////////////
// Accessors to contained values

CrawlStoreValue& CrawlVector::get_value(const vec_size &index)
{
    assert_validity();

    ASSERT(index <= max_size);
    ASSERT(index <= vector.size());

    return vector[index];
}

const CrawlStoreValue& CrawlVector::get_value(const vec_size &index) const
{
    assert_validity();

    ASSERT(index <= max_size);
    ASSERT(index <= vector.size());

    return vector[index];
}

CrawlStoreValue& CrawlVector::operator[] (const vec_size &index)
{
    return get_value(index);
}

const CrawlStoreValue& CrawlVector::operator[] (const vec_size &index) const
{
    return get_value(index);
}

///////////////////////////
// std::vector style interface
vec_size CrawlVector::size() const
{
    return vector.size();
}

bool CrawlVector::empty() const
{
    return vector.empty();
}

CrawlStoreValue& CrawlVector::pop_back()
{
    assert_validity();
    ASSERT(vector.size() > 0);

    CrawlStoreValue& val = vector[vector.size() - 1];
    vector.pop_back();
    return val;
}

void CrawlVector::push_back(CrawlStoreValue val)
{
    assert_validity();
    ASSERT(vector.size() < max_size);
    ASSERT(type == SV_NONE
           || (val.type == SV_NONE && (val.flags & SFLAG_UNSET))
           || (val.type == type));
    val.flags |= default_flags;
    if (type != SV_NONE)
    {
        val.type   = type;
        val.flags |= SFLAG_CONST_TYPE;
    }
    vector.push_back(val);
}

void CrawlVector::insert(const vec_size index, CrawlStoreValue val)
{
    assert_validity();
    ASSERT(vector.size() < max_size);
    ASSERT(type == SV_NONE
           || (val.type == SV_NONE && (val.flags & SFLAG_UNSET))
           || (val.type == type));
    val.flags |= default_flags;
    if (type != SV_NONE)
    {
        val.type   = type;
        val.flags |= SFLAG_CONST_TYPE;
    }
    vector.insert(vector.begin() + index, val);
}

void CrawlVector::resize(const vec_size _size)
{
    assert_validity();
    ASSERT(max_size == VEC_MAX_SIZE);
    ASSERT(_size < max_size);

    vec_size old_size = size();
    vector.resize(_size);

    for (vec_size i = old_size; i < _size; i++)
    {
        vector[i].flags = SFLAG_UNSET | default_flags;
        vector[i].type  = SV_NONE;
    }
}

void CrawlVector::erase(const vec_size index)
{
    assert_validity();
    ASSERT(index <= max_size);
    ASSERT(index <= vector.size());

    vector.erase(vector.begin() + index);
}

void CrawlVector::clear()
{
    assert_validity();
    ASSERT(!(default_flags & SFLAG_NO_ERASE));

    for (vec_size i = 0, _size = size(); i < _size; i++)
        ASSERT(!(vector[i].flags & SFLAG_NO_ERASE));

    vector.clear();
    default_flags = 0;
    type          = SV_NONE;
}

CrawlVector::iterator CrawlVector::begin()
{
    assert_validity();
    return vector.begin();
}

CrawlVector::iterator CrawlVector::end()
{
    assert_validity();
    return vector.end();
}

CrawlVector::const_iterator CrawlVector::begin() const
{
    assert_validity();
    return vector.begin();
}

CrawlVector::const_iterator CrawlVector::end() const
{
    assert_validity();
    return vector.end();
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

template <typename T, store_val_type TYPE>
CrawlTableWrapper<T, TYPE>::CrawlTableWrapper()
{
    table = NULL;
}

template <typename T, store_val_type TYPE>
CrawlTableWrapper<T, TYPE>::CrawlTableWrapper(CrawlHashTable& _table)
{
    wrap(_table);
}

template <typename T, store_val_type TYPE>
CrawlTableWrapper<T, TYPE>::CrawlTableWrapper(CrawlHashTable* _table)
{
    wrap(_table);
}

template <typename T, store_val_type TYPE>
void CrawlTableWrapper<T, TYPE>::wrap(CrawlHashTable& _table)
{
    wrap(&_table);
}

template <typename T, store_val_type TYPE>
void CrawlTableWrapper<T, TYPE>::wrap(CrawlHashTable* _table)
{
    ASSERT(_table != NULL);
    ASSERT(_table->get_type() == TYPE);

    table = _table;
}

template <typename T, store_val_type TYPE>
T& CrawlTableWrapper<T, TYPE>::operator[] (const std::string &key)
{
    return (T&) (*table)[key];
}

template <typename T, store_val_type TYPE>
const T CrawlTableWrapper<T, TYPE>::operator[] (const std::string &key) const
{
    return (T) (*table)[key];
}
