/*
 *  File:       store.h
 *  Summary:    Saveable hash-table and vector capable of storing
 *              multiple types of data.
 *  Written by: Matthew Cline
 */

#ifndef STORE_H
#define STORE_H

#include <limits.h>
#include <map>
#include <string>
#include <vector>

class  reader;
class  writer;
class  CrawlHashTable;
class  CrawlVector;
struct item_def;
struct coord_def;

typedef unsigned char hash_size;
typedef unsigned char vec_size;
typedef unsigned char store_flags;

#define VEC_MAX_SIZE  255
#define HASH_MAX_SIZE 255

// NOTE: Changing the ordering of these enums will break savefile
// compatibility.
enum store_val_type
{
    SV_NONE = 0,
    SV_BOOL,
    SV_BYTE,
    SV_SHORT,
    SV_LONG,
    SV_FLOAT,
    SV_STR,
    SV_COORD,
    SV_ITEM,
    SV_HASH,
    SV_VEC,
    NUM_STORE_VAL_TYPES
};

enum store_flag_type
{
    SFLAG_UNSET      = (1 << 0),
    SFLAG_CONST_VAL  = (1 << 1),
    SFLAG_CONST_TYPE = (1 << 2),
    SFLAG_NO_ERASE   = (1 << 3)
};


// Can't just cast everything into a void pointer, since a float might
// not fit into a pointer on all systems.
typedef union StoreUnion StoreUnion;
union StoreUnion
{
    bool  boolean;
    char  byte;
    short _short;
    long  _long;
    float _float;
    void* ptr;
};


class CrawlStoreValue
{
public:
    CrawlStoreValue();
    CrawlStoreValue(const CrawlStoreValue &other);

    ~CrawlStoreValue();

    // Conversion constructors
    CrawlStoreValue(const bool val);
    CrawlStoreValue(const char &val);
    CrawlStoreValue(const short &val);
    CrawlStoreValue(const long &val);
    CrawlStoreValue(const float &val);
    CrawlStoreValue(const std::string &val);
    CrawlStoreValue(const char* val);
    CrawlStoreValue(const coord_def &val);
    CrawlStoreValue(const item_def &val);
    CrawlStoreValue(const CrawlHashTable &val);
    CrawlStoreValue(const CrawlVector &val);

    // Only needed for doing some assertion checking.
    CrawlStoreValue &operator = (const CrawlStoreValue &other);

protected:
    store_val_type type;
    store_flags    flags;
    StoreUnion     val;

public:
    store_flags    get_flags() const;
    store_flags    set_flags(store_flags flags);
    store_flags    unset_flags(store_flags flags);
    store_val_type get_type() const;

    CrawlHashTable &new_table(store_flags flags);
    CrawlHashTable &new_table(store_val_type type, store_flags flags = 0);

    CrawlVector &new_vector(store_flags flags,
                            vec_size max_size = VEC_MAX_SIZE);
    CrawlVector &new_vector(store_val_type type, store_flags flags = 0,
                            vec_size max_size = VEC_MAX_SIZE);

    bool           &get_bool();
    char           &get_byte();
    short          &get_short();
    long           &get_long();
    float          &get_float();
    std::string    &get_string();
    coord_def      &get_coord();
    CrawlHashTable &get_table();
    CrawlVector    &get_vector();
    item_def       &get_item();

    bool           get_bool()   const;
    char           get_byte()   const;
    short          get_short()  const;
    long           get_long()   const;
    float          get_float()  const;
    std::string    get_string() const;
    coord_def      get_coord()  const;

    const CrawlHashTable& get_table()  const;
    const CrawlVector&    get_vector() const;
    const item_def&       get_item()   const;

#if 0
    // These don't actually exist
    void set_bool(const bool val);
    void set_byte(const char val);
    void set_short(const short val);
    void set_long(const long val);
    void set_float(const float val);
    void set_string(const std::string &val);
    void set_coord(const coord_def &val);
    void set_table(const CrawlHashTable &val);
    void set_vector(const CrawlVector &val);
    void set_item(const item_def &val);
#endif

public:
    // NOTE: All operators will assert if the value is of the wrong
    // type for the operation.  If the value has no type yet, the
    // operation will set it to the appropriate type.  If the value
    // has no type yet and the operation modifies the existing value
    // rather than replacing it (e.g., ++) the value will be set to a
    // default before the operation is done.

    // If the value is a hash table or vector, the container's values
    // can be accessed with the [] operator with the approriate key
    // type (strings for hashes, longs for vectors).
    CrawlStoreValue &operator [] (const std::string   &key);
    CrawlStoreValue &operator [] (const vec_size &index);

    const CrawlStoreValue &operator [] (const std::string &key) const;
    const CrawlStoreValue &operator [] (const vec_size &index)  const;

    // Typecast operators
#ifdef TARGET_COMPILER_VC
    operator bool&();
    operator char&();
    operator short&();
    operator long&();
    operator float&();
    operator std::string&();
    operator coord_def&();
    operator CrawlHashTable&();
    operator CrawlVector&();
    operator item_def&();
#else
    &operator bool();
    &operator char();
    &operator short();
    &operator long();
    &operator float();
    &operator std::string();
    &operator coord_def();
    &operator CrawlHashTable();
    &operator CrawlVector();
    &operator item_def();
#endif

    operator bool() const;
    operator char() const;
    operator short() const;
    operator long() const;
    operator float() const;
    operator std::string() const;
    operator coord_def() const;

    // Assignment operators
    CrawlStoreValue &operator = (const bool &val);
    CrawlStoreValue &operator = (const char &val);
    CrawlStoreValue &operator = (const short &val);
    CrawlStoreValue &operator = (const long &val);
    CrawlStoreValue &operator = (const float &val);
    CrawlStoreValue &operator = (const std::string &val);
    CrawlStoreValue &operator = (const char* val);
    CrawlStoreValue &operator = (const coord_def &val);
    CrawlStoreValue &operator = (const CrawlHashTable &val);
    CrawlStoreValue &operator = (const CrawlVector &val);
    CrawlStoreValue &operator = (const item_def &val);

    // Misc operators
    std::string &operator += (const std::string &val);

    // Prefix
    long operator ++ ();
    long operator -- ();

    // Postfix
    long operator ++ (int);
    long operator -- (int);

protected:
    CrawlStoreValue(const store_flags flags,
                    const store_val_type type = SV_NONE);

    void write(writer &) const;
    void read(reader &);

    void unset(bool force = false);

    friend class CrawlHashTable;
    friend class CrawlVector;
};


// A hash table can have a maximum of 255 key/value pairs.  If you
// want more than that you can use nested hash tables.
//
// By default a hash table's value data types are heterogeneous.  To
// make it homogeneous (which causes dynamic type checking) you have
// to give a type to the hash table constructor; once it's been
// created its type (or lack of type) is immutable.
//
// An empty hash table will take up only 1 byte in the savefile.  A
// non-empty hash table will have an overhead of 3 bytes for the hash
// table overall and 2 bytes per key/value pair, over and above the
// number of bytes needed to store the keys and values themselves.
class CrawlHashTable
{
public:
    CrawlHashTable();
    CrawlHashTable(store_flags flags);
    CrawlHashTable(store_val_type type, store_flags flags = 0);

    ~CrawlHashTable();

    typedef std::map<std::string, CrawlStoreValue> hash_map_type;
    typedef hash_map_type::iterator                iterator;
    typedef hash_map_type::const_iterator          const_iterator;

protected:
    store_val_type type;
    store_flags    default_flags;
    hash_map_type  hash_map;

    friend class CrawlStoreValue;

public:
    void write(writer &) const;
    void read(reader &);

    store_flags    get_default_flags() const;
    store_flags    set_default_flags(store_flags flags);
    store_flags    unset_default_flags(store_flags flags);
    store_val_type get_type() const;
    bool           exists(const std::string &key) const;
    void           assert_validity() const;

    // NOTE: If the const versions of get_value() or [] are given a
    // key which doesn't exist, they will assert.
    const CrawlStoreValue& get_value(const std::string &key) const;
    const CrawlStoreValue& operator[] (const std::string &key) const;

    // NOTE: If get_value() or [] is given a key which doesn't exist
    // in the table, an unset/empty CrawlStoreValue will be created
    // with that key and returned.  If it is not then given a value
    // then the next call to assert_validity() will fail.  If the
    // hash table has a type (rather than being heterogeneous)
    // then trying to assign a different type to the CrawlStoreValue
    // will assert.
    CrawlStoreValue& get_value(const std::string &key);
    CrawlStoreValue& operator[] (const std::string &key);

    // std::map style interface
    hash_size size() const;
    bool      empty() const;

    void      erase(const std::string key);
    void      clear();

    const_iterator begin() const;
    const_iterator end() const;

    iterator  begin();
    iterator  end();
};

// A CrawlVector is the vector version of CrawlHashTable, except that
// a non-empty CrawlVector has one more byte of savefile overhead that
// a hash table, and that can specify a maximum size to make it act
// similarly to a FixedVec.
class CrawlVector
{
public:
    CrawlVector();
    CrawlVector(store_flags flags, vec_size max_size = VEC_MAX_SIZE);
    CrawlVector(store_val_type type, store_flags flags = 0,
                vec_size max_size = VEC_MAX_SIZE);

    ~CrawlVector();

    typedef std::vector<CrawlStoreValue> vector_type;
    typedef vector_type::iterator        iterator;
    typedef vector_type::const_iterator  const_iterator;

protected:
    store_val_type type;
    store_flags    default_flags;
    vec_size       max_size;
    vector_type    vector;

    friend class CrawlStoreValue;

public:
    void write(writer &) const;
    void read(reader &);

    store_flags    get_default_flags() const;
    store_flags    set_default_flags(store_flags flags);
    store_flags    unset_default_flags(store_flags flags);
    store_val_type get_type() const;
    void           assert_validity() const;
    void           set_max_size(vec_size size);
    vec_size       get_max_size() const;

    // NOTE: If the const versions of get_value() or [] are given an
    // index which doesn't exist, they will assert.
    const CrawlStoreValue& get_value(const vec_size &index) const;
    const CrawlStoreValue& operator[] (const vec_size &index) const;

    CrawlStoreValue& get_value(const vec_size &index);
    CrawlStoreValue& operator[] (const vec_size &index);

    // std::vector style interface
    vec_size size() const;
    bool     empty() const;

    // NOTE: push_back() and insert() have val passed by value rather
    // than by reference so that conversion constructors will work.
    CrawlStoreValue& pop_back();
    void             push_back(CrawlStoreValue val);
    void insert(const vec_size index, CrawlStoreValue val);

    // resize() will assert if the maximum size has been set.
    void resize(const vec_size size);
    void erase(const vec_size index);
    void clear();

    const_iterator begin() const;
    const_iterator end() const;

    iterator begin();
    iterator end();
};

// A wrapper for non-heterogeneous hash tables, so that the values can
// be accessed without using get_foo().  T needs to have both normal
// and const type-cast operators defined by CrawlStoreValue for this
// template to work.
template <typename T, store_val_type TYPE>
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

    const CrawlHashTable* get_table() const;
    const T operator[] (const std::string &key) const;

    CrawlHashTable* get_table();
    T& operator[] (const std::string &key);
};

typedef CrawlTableWrapper<bool, SV_BOOL>       CrawlBoolTable;
typedef CrawlTableWrapper<char, SV_BYTE>       CrawlByteTable;
typedef CrawlTableWrapper<short, SV_SHORT>     CrawlShortTable;
typedef CrawlTableWrapper<long, SV_LONG>       CrawlLongTable;
typedef CrawlTableWrapper<float, SV_FLOAT>     CrawlFloatTable;
typedef CrawlTableWrapper<std::string, SV_STR> CrawlStringTable;
typedef CrawlTableWrapper<coord_def, SV_COORD> CrawlCoordTable;

#endif
