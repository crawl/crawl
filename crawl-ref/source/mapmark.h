#ifndef __MAPMARK_H__
#define __MAPMARK_H__

#include "dungeon.h"
#include "dgnevent.h"
#include "clua.h"
#include "luadgn.h"

//////////////////////////////////////////////////////////////////////////
// Map markers

// Can't change this order without breaking saves.
enum map_marker_type
{
    MAT_FEATURE,              // Stock marker.
    MAT_LUA_MARKER,
    MAT_FEATURE_NAME,
    NUM_MAP_MARKER_TYPES,
    MAT_ANY
};

class map_marker
{
public:
    map_marker(map_marker_type type, const coord_def &pos);
    virtual ~map_marker();

    map_marker_type get_type() const { return type; }

    virtual void activate();
    virtual void write(tagHeader &) const;
    virtual void read(tagHeader &);
    virtual std::string debug_describe() const = 0;
    virtual std::string feature_description() const;
    virtual std::string property(const std::string &pname) const; 
   
    static map_marker *read_marker(tagHeader&);
    static map_marker *parse_marker(const std::string &text,
                                    const std::string &ctx = "")
        throw (std::string);

public:
    coord_def pos;

protected:
    map_marker_type type;

    typedef map_marker *(*marker_reader)(tagHeader &, map_marker_type);
    typedef map_marker *(*marker_parser)(const std::string &,
                                         const std::string &);
    static marker_reader readers[NUM_MAP_MARKER_TYPES];
    static marker_parser parsers[NUM_MAP_MARKER_TYPES];
};

class map_feature_marker : public map_marker
{
public:
    map_feature_marker(const coord_def &pos = coord_def(0, 0),
                       dungeon_feature_type feat = DNGN_UNSEEN);
    map_feature_marker(const map_feature_marker &other);
    void write(tagHeader &) const;
    void read(tagHeader &);
    std::string debug_describe() const;
    static map_marker *read(tagHeader &, map_marker_type);
    static map_marker *parse(const std::string &s, const std::string &)
        throw (std::string);
    
public:
    dungeon_feature_type feat;
};

// A marker powered by Lua.
class map_lua_marker : public map_marker, public dgn_event_listener
{
public:
    map_lua_marker();
    map_lua_marker(const std::string &s, const std::string &ctx);
    ~map_lua_marker();

    void activate();

    void write(tagHeader &) const;
    void read(tagHeader &);
    std::string debug_describe() const;
    std::string feature_description() const;
    std::string property(const std::string &pname) const;
    
    void notify_dgn_event(const dgn_event &e);
   
    static map_marker *read(tagHeader &, map_marker_type);
    static map_marker *parse(const std::string &s, const std::string &)
        throw (std::string);
private:
    bool initialised;
private:
    void check_register_table();
    bool get_table() const;
    void push_fn_args(const char *fn) const;
    bool callfn(const char *fn, bool warn_err = false, int args = -1) const;
    std::string call_str_fn(const char *fn) const;
};

void env_activate_markers();
void env_add_marker(map_marker *);
void env_remove_marker(map_marker *);
void env_remove_markers_at(const coord_def &c, map_marker_type);
std::vector<map_marker*> env_get_all_markers();
map_marker *env_find_marker(const coord_def &c, map_marker_type);
std::vector<map_marker*> env_get_markers(const coord_def &c);
void env_clear_markers();
std::string env_property_at(const coord_def &c, map_marker_type,
                            const std::string &key);
void env_move_markers(const coord_def &from, const coord_def &to);

#endif
