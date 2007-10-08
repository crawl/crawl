/*
 *  File:       hash.h
 *  Summary:    Saveable hash-table capable of storing multiple types
 *              of data.
 *  Written by: Matthew Cline
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <1>      10/5/07    MPC    Created
 */

#ifndef HASH_H
#define HASH_H

#include <string>
#include <map>

struct tagHeader;
class  CrawlHashTable;
class  item_def;
class  coord_def;

// NOTE: Changing the ordering of these enums will break savefile
// compatibility.
enum hash_val_type
{
    HV_NONE = 0,
    HV_BOOL,
    HV_BYTE,
    HV_SHORT,
    HV_LONG,
    HV_FLOAT,
    HV_STR,
    HV_COORD,
    HV_HASH,
    HV_ITEM,
    NUM_HASH_VAL_TYPES
};

enum hash_flag_type
{
    HFLAG_UNSET      = (1 << 0),
    HFLAG_CONST_VAL  = (1 << 1),
    HFLAG_CONST_TYPE = (1 << 2),
    HFLAG_NO_ERASE   = (1 << 3)
};


// Can't just cast everything into a void pointer, since a float might
// not fit into a pointer on all systems.
typedef union HashUnion HashUnion;
union HashUnion
{
    bool  boolean;
    char  byte;
    short _short;
    long  _long;
    float _float;
    void* ptr;
};


class CrawlHashValue
{
public:
    CrawlHashValue();
    CrawlHashValue(const CrawlHashValue &other);

    ~CrawlHashValue();

    // Only needed for doing some assertion checking.
    CrawlHashValue &operator = (const CrawlHashValue &other);

protected:
    hash_val_type   type;
    unsigned char   flags;
    HashUnion       val;

public:
    unsigned char   get_flags() const;
    unsigned char   set_flags(unsigned char flags);
    unsigned char   unset_flags(unsigned char flags);
    hash_val_type   get_type() const;

    CrawlHashTable &new_table(unsigned char flags);
    CrawlHashTable &new_table(hash_val_type type, unsigned char flags = 0);

    bool           &get_bool();
    char           &get_byte();
    short          &get_short();
    long           &get_long();
    float          &get_float();
    std::string    &get_string();
    coord_def      &get_coord();
    CrawlHashTable &get_table();
    item_def       &get_item();

    bool           get_bool() const;
    char           get_byte() const;
    short          get_short() const;
    long           get_long() const;
    float          get_float() const;
    std::string    get_string() const;
    coord_def      get_coord() const;

    const CrawlHashTable& get_table() const;
    const item_def&       get_item() const;

    void set_bool(const bool val);
    void set_byte(const char val);
    void set_short(const short val);
    void set_long(const long val);
    void set_float(const float val);
    void set_string(const std::string &val);
    void set_coord(const coord_def &val);
    void set_table(const CrawlHashTable &val);
    void set_item(const item_def &val);

public:
    // NOTE: All operators will assert if the hash value is of the
    // wrong type for the operation.  If the value has no type yet,
    // the operation will set it to the appropriate type.  If the
    // value has no type yet and the operation modifies the existing
    // value rather than replacing it (i.e., ++) the value will be set
    // to a default before the operation is done.

    // If the hash value is a hash table, the table's values can be
    // accessed with the [] operator.
    CrawlHashValue &operator [] (const std::string &key);
    CrawlHashValue &operator [] (const long &key);

    const CrawlHashValue &operator [] (const std::string &key) const;
    const CrawlHashValue &operator [] (const long &key) const;

    // Typecast operators
    &operator bool();
    &operator char();
    &operator short();
    &operator long();
    &operator float();
    &operator std::string();
    &operator coord_def();
    &operator CrawlHashTable();
    &operator item_def();

    operator bool() const;
    operator char() const;
    operator short() const;
    operator long() const;
    operator float() const;
    operator std::string() const;
    operator coord_def() const;

    // Assignment operators
    bool           &operator = (const bool &val);
    char           &operator = (const char &val);
    short          &operator = (const short &val);
    long           &operator = (const long &val);
    float          &operator = (const float &val);
    std::string    &operator = (const std::string &val);
    const char*     operator = (const char* val);
    coord_def      &operator = (const coord_def &val);
    CrawlHashTable &operator = (const CrawlHashTable &val);
    item_def       &operator = (const item_def &val);

    // Misc operators
    std::string &operator += (const std::string &val);

    // Prefix
    long operator ++ ();
    long operator -- ();

    // Postfix
    long operator ++ (int);
    long operator -- (int);

protected:
    CrawlHashValue(const unsigned char flags,
                   const hash_val_type type = HV_NONE);

    void write(tagHeader &th) const;
    void read(tagHeader &th);

    void unset(bool force = false);

    friend class CrawlHashTable;
};


// A hash table can have a maximum of 255 key/value pairs.  If you
// want more than that you can use nested hash tables.
//
// By default a hash table's value data types are heterogeneous.  To
// make it homogeneous (which causes dynamic type checking) you have
// to give a type to the hash table constructor; once it's been
// created it's type (or lack of type) is immutable.
//
// An empty hash table will take up only 1 byte in the savefile.  A
// non-empty hash table will have an overhead of 3 bytes for the hash
// table overall and 2 bytes per key/value pair, over and above the
// number of bytes needed to store the keys and values themselves.
class CrawlHashTable
{
public:
    CrawlHashTable();
    CrawlHashTable(unsigned char flags);
    CrawlHashTable(hash_val_type type, unsigned char flags = 0);

    ~CrawlHashTable();

    typedef std::map<std::string, CrawlHashValue> hash_map_type;

protected:
    hash_val_type type;
    unsigned char default_flags;
    hash_map_type hash_map;

    friend class CrawlHashValue;

public:
    void write(tagHeader &th) const;
    void read(tagHeader &th);

    unsigned char get_default_flags() const;
    unsigned char set_default_flags(unsigned char flags);
    unsigned char unset_default_flags(unsigned char flags);
    hash_val_type get_type() const;
    bool          exists(const std::string key) const;
    bool          exists(const long key) const;
    void          assert_validity() const;
    int           compact_indicies(long min_index, long max_index,
                                   bool compact_down = true);
    bool          fixup_indexed_array(std::string name = "");

    // NOTE: If get_value() or [] is given a key which doesn't exist
    // in the table, an unset/empty CrawlHashValue will be created
    // with that key and returned.  If it is not then given a value
    // then the next call to assert_validity() will fail.  If the
    // hash table has a type (rather than being heterogeneous)
    // then trying to assign a different type to the CrawlHashValue
    // will assert.
    CrawlHashValue& get_value(const std::string &key);
    CrawlHashValue& get_value(const long &index);
    CrawlHashValue& operator[] (const std::string &key);
    CrawlHashValue& operator[] (const long &index);

    // NOTE: If the const versions of get_value() or [] are given a
    // key which doesn't exist, they will assert.
    const CrawlHashValue& get_value(const std::string &key) const;
    const CrawlHashValue& get_value(const long &index) const;
    const CrawlHashValue& operator[] (const std::string &key) const;
    const CrawlHashValue& operator[] (const long &index) const;

    // std::map style interface
    size_t size() const;
    bool   empty() const;

    void   erase(const std::string key);
    void   erase(const long index);
    void   clear();

    hash_map_type::iterator begin();
    hash_map_type::iterator end();

    hash_map_type::const_iterator begin() const;
    hash_map_type::const_iterator end() const;
};


// A wrapper for non-heterogeneous hash tables, so that the values can
// be accessed without using get_foo().  T needs to have both normal
// and const type-cast operators defined by CrawlHashValue for this
// template to work.
template <typename T, hash_val_type TYPE>
class CrawlTableWrapper
{
public:
    CrawlTableWrapper();
    CrawlTableWrapper(CrawlHashTable& table);
    CrawlTableWrapper(CrawlHashTable* table);

protected:
    CrawlHashTable* table;

public:
    void wrap(CrawlHashTable& table);
    void wrap(CrawlHashTable* table);

    CrawlHashTable* get_table();
    T& operator[] (const std::string &key);
    T& operator[] (const long &index);

    const CrawlHashTable* get_table() const;
    const T operator[] (const std::string &key) const;
    const T operator[] (const long &index) const;
};

typedef CrawlTableWrapper<bool, HV_BOOL>       CrawlBoolTable;
typedef CrawlTableWrapper<char, HV_BYTE>       CrawlByteTable;
typedef CrawlTableWrapper<short, HV_SHORT>     CrawlShortTable;
typedef CrawlTableWrapper<long, HV_LONG>       CrawlLongTable;
typedef CrawlTableWrapper<float, HV_FLOAT>     CrawlFloatTable;
typedef CrawlTableWrapper<std::string, HV_STR> CrawlStringTable;
typedef CrawlTableWrapper<coord_def, HV_COORD> CrawlCoordTable;

#endif
