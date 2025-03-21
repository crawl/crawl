/**
 * @file
 * @brief Level markers (annotations).
**/

#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "beh-type.h"
#include "clua.h"
#include "dgn-event.h"
#include "map-marker-type.h"
#include "tag-version.h"
#include "terrain-change-type.h"

//////////////////////////////////////////////////////////////////////////
// Map markers

class reader;
class writer;

void remove_markers_and_listeners_at(coord_def p);

bool marker_vetoes_operation(const char *op);
coord_def find_marker_position_by_prop(const string &prop,
                                       const string &expected = "");
vector<coord_def> find_marker_positions_by_prop(const string &prop,
                                                const string &expected = "",
                                                unsigned maxresults = 0);
vector<map_marker*> find_markers_by_prop(const string &prop,
                                         const string &expected = "",
                                         unsigned maxresults = 0);

struct bad_map_marker : public runtime_error
{
    explicit bad_map_marker(const string &msg) : runtime_error(msg) {}
    explicit bad_map_marker(const char *msg) : runtime_error(msg) {}
};

/**
 * Create a bad_map_marker exception from a printf-like specification.
 * Users of this macro must #include "stringutil.h" themselves.
 */
#define bad_map_marker_f(...) bad_map_marker(make_stringf(__VA_ARGS__))

class map_marker
{
public:
    map_marker(map_marker_type type, const coord_def &pos);
    virtual ~map_marker();

    map_marker_type get_type() const { return type; }

    virtual map_marker *clone() const = 0;
    virtual void init();
    virtual void activate(bool verbose = true);
    virtual void write(writer &) const;
    virtual void read(reader &);
    virtual string debug_describe() const = 0;
    virtual string property(const string &pname) const;

    static map_marker *read_marker(reader &);
    /// @throws bad_map_marker if text could not be parsed.
    static map_marker *parse_marker(const string &text, const string &ctx = "");

public:
    coord_def pos;

protected:
    map_marker_type type;

    typedef map_marker *(*marker_reader)(reader &, map_marker_type);
    typedef map_marker *(*marker_parser)(const string &, const string &);
    static marker_reader readers[NUM_MAP_MARKER_TYPES];
    static marker_parser parsers[NUM_MAP_MARKER_TYPES];
};

class map_feature_marker : public map_marker
{
public:
    map_feature_marker(const coord_def &pos = coord_def(0, 0),
                       dungeon_feature_type feat = DNGN_UNSEEN);
    map_feature_marker(const map_feature_marker &other);
    void write(writer &) const override;
    void read(reader &) override;
    string debug_describe() const override;
    map_marker *clone() const override;
    static map_marker *read(reader &, map_marker_type);
    /// @throws bad_map_marker if s could not be parsed.
    static map_marker *parse(const string &s, const string &);

public:
    dungeon_feature_type feat;
};

class map_corruption_marker : public map_marker
{
public:
    map_corruption_marker(const coord_def &pos = coord_def(0, 0),
                          int dur = 0);

    void write(writer &) const override;
    void read(reader &) override;
    map_marker *clone() const override;
    string debug_describe() const override;

    static map_marker *read(reader &, map_marker_type);

public:
    int duration;
};

class map_tomb_marker : public map_marker
{
public:
    map_tomb_marker(const coord_def& pos = coord_def(0, 0),
                    int dur = 0, int src = 0, int targ = 0);

    void write(writer &) const override;
    void read(reader &) override;
    map_marker *clone() const override;
    string debug_describe() const override;

    static map_marker *read(reader &, map_marker_type);

public:
    int duration, source, target;
};

class map_malign_gateway_marker : public map_marker
{
public:
    map_malign_gateway_marker (const coord_def& pos = coord_def(0, 0),
                    int dur = 0, bool ip = false, string caster = "",
                    beh_type bh = BEH_HOSTILE, god_type gd = GOD_NO_GOD,
                    int pow = 0);

    void write (writer &) const override;
    void read (reader &) override;
    map_marker *clone() const override;
    string debug_describe() const override;

    static map_marker *read(reader &, map_marker_type);

public:
    int duration;
    bool is_player;
    bool monster_summoned;
    string summoner_string;
    beh_type behaviour;
    god_type god;
    int power;
};

#if TAG_MAJOR_VERSION == 34
// A marker powered by phoenixes!
class map_phoenix_marker : public map_marker
{
public:
    map_phoenix_marker (const coord_def& pos = coord_def(0, 0),
                    int dur = 0, int mnum = 0, beh_type bh = BEH_HOSTILE,
                    mon_attitude_type at = ATT_HOSTILE, god_type gd = GOD_NO_GOD,
                    coord_def cp = coord_def(-1, -1)
                    );

    void write (writer &) const override;
    void read (reader &) override;
    map_marker *clone() const override;
    string debug_describe() const override;

    static map_marker *read(reader &, map_marker_type);

public:
    int duration;
    int mon_num;
    beh_type behaviour;
    mon_attitude_type attitude;
    god_type god;
    coord_def corpse_pos;
};


// A marker for sealed doors
class map_door_seal_marker : public map_marker
{
public:
    map_door_seal_marker (const coord_def& pos = coord_def(0, 0),
                    int dur = 0, int mnum = 0,
                    dungeon_feature_type oldfeat = DNGN_CLOSED_DOOR);

    void write (writer &) const override;
    void read (reader &) override;
    map_marker *clone() const override;
    string debug_describe() const override;

    static map_marker *read(reader &, map_marker_type);

public:
    int duration;
    int mon_num;
    dungeon_feature_type old_feature;
};
#endif

// A marker for temporary terrain changes
class map_terrain_change_marker : public map_marker
{
public:
    map_terrain_change_marker (const coord_def& pos = coord_def(0, 0),
                    dungeon_feature_type oldfeat = DNGN_FLOOR,
                    dungeon_feature_type newfeat = DNGN_FLOOR,
                    unsigned short flv_oldfeat = 0,
                    unsigned short flv_oldfeat_idx = 0,
                    int dur = 0, terrain_change_type type = TERRAIN_CHANGE_GENERIC,
                    int mnum = 0, int oldcol = BLACK);

    void write (writer &) const override;
    void read (reader &) override;
    map_marker *clone() const override;
    string debug_describe() const override;

    static map_marker *read(reader &, map_marker_type);
    static bool any_at(coord_def pos, function<bool(map_terrain_change_marker&)> predicate);

public:
    int duration;
    int mon_num;
    dungeon_feature_type old_feature;
    dungeon_feature_type new_feature;
    unsigned short flv_old_feature;
    unsigned short flv_old_feature_idx;
    terrain_change_type  change_type;
    int colour;
};

class map_cloud_spreader_marker : public map_marker
{
public:
    map_cloud_spreader_marker(const coord_def &pos = coord_def(0, 0),
                              cloud_type type = CLOUD_NONE,
                              int speed = 10, int amount = 35,
                              int max_radius = LOS_DEFAULT_RANGE,
                              int dur = 10, actor* agent = nullptr);

    void write(writer &) const override;
    void read(reader &) override;
    map_marker *clone() const override;
    string debug_describe() const override;

    static map_marker *read(reader &, map_marker_type);

public:
    cloud_type ctype;
    int speed;
    int remaining;
    int max_rad;
    int duration;
    int speed_increment;
    mid_t agent_mid;
    kill_category kcat;
};

// A marker powered by Lua.
class map_lua_marker : public map_marker, public dgn_event_listener
{
public:
    map_lua_marker();
    map_lua_marker(const lua_datum &function);
    map_lua_marker(const string &s, const string &ctx,
                   bool mapdef_marker = true);
    ~map_lua_marker();

    void init() override;
    void activate(bool verbose) override;

    void write(writer &) const override;
    void read(reader &) override;
    map_marker *clone() const override;
    string debug_describe() const override;
    string property(const string &pname) const override;

    bool notify_dgn_event(const dgn_event &e) override;

    static map_marker *read(reader &, map_marker_type);
    /// @throws bad_map_marker if s could not be parsed.
    static map_marker *parse(const string &s, const string &);

    string debug_to_string() const;
private:
    bool initialised;
    unique_ptr<lua_datum> marker_table;

private:
    void check_register_table();
    bool get_table() const;
    void push_fn_args(const char *fn) const;
    bool callfn(const char *fn, bool warn_err = false, int args = -1) const;
    string call_str_fn(const char *fn) const;
};

class map_wiz_props_marker : public map_marker
{
public:
    map_wiz_props_marker(const coord_def &pos = coord_def(0, 0));
    map_wiz_props_marker(const map_wiz_props_marker &other);
    void write(writer &) const override;
    void read(reader &) override;
    string debug_describe() const override;
    string property(const string &pname) const override;
    string set_property(const string &key, const string &val);
    map_marker *clone() const override;
    static map_marker *read(reader &, map_marker_type);
    /// @throws bad_map_marker if s could not be parsed.
    static map_marker *parse(const string &s, const string &);

public:
    map<string, string> properties;
};

class map_position_marker : public map_marker
{

public:
    map_position_marker(const coord_def &pos = coord_def(0, 0),
                        dungeon_feature_type feat = DNGN_UNSEEN,
                        const coord_def _dest = INVALID_COORD);
    map_position_marker(const map_position_marker &other);
    void write(writer &) const override;
    void read(reader &) override;
    string debug_describe() const override;
    map_marker *clone() const override;
    static map_marker *read(reader &, map_marker_type);

public:
    dungeon_feature_type feat;
    coord_def dest;

};

class map_markers
{
public:
    map_markers();
    map_markers(const map_markers &);
    map_markers &operator = (const map_markers &);
    ~map_markers();

    bool need_activate() const { return have_inactive_markers; }
    void clear_need_activate();
    void init_all();
    void activate_all(bool verbose = true);
    void activate_markers_at(coord_def p);
    void add(map_marker *marker);
    void remove(map_marker *marker);
    void remove_markers_at(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(const coord_def &c, map_marker_type type = MAT_ANY);
    map_marker *find(map_marker_type type);
    void move(const coord_def &from, const coord_def &to);
    void move_marker(map_marker *marker, const coord_def &to);
    vector<map_marker*> get_all(map_marker_type type = MAT_ANY);
    vector<map_marker*> get_all(const string &key, const string &val = "");
    vector<map_marker*> get_markers_at(const coord_def &c);
    string property_at(const coord_def &c, map_marker_type type,
                       const string &key);
    string property_at(const coord_def &c, map_marker_type type,
                       const char *key)
    { return property_at(c, type, string(key)); }
    void clear();

    void write(writer &) const;
    void read(reader &);

private:
    typedef multimap<coord_def, map_marker *> dgn_marker_map;
    typedef pair<coord_def, map_marker *> dgn_pos_marker;

    void init_from(const map_markers &);
    void unlink_marker(const map_marker *);
    void check_empty();

private:
    dgn_marker_map markers;
    bool have_inactive_markers;
};

map_position_marker *get_position_marker_at(const coord_def &pos,
                                            dungeon_feature_type feat);
coord_def get_transporter_dest(const coord_def &pos);
