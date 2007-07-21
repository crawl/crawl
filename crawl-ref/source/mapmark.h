#ifndef __MAPMARK_H__
#define __MAPMARK_H__

#include "dungeon.h"
#include "dgnevent.h"

//////////////////////////////////////////////////////////////////////////
// Map markers

// Can't change this order without breaking saves.
enum map_marker_type
{
    MAT_FEATURE,              // Stock marker.
    MAT_TIMED_FEATURE,
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
    virtual std::string describe() const = 0;
    
    static map_marker *read_marker(tagHeader&);
    static map_marker *parse_marker(const std::string &text)
        throw (std::string);

public:
    coord_def pos;

protected:
    map_marker_type type;

    typedef map_marker *(*marker_reader)(tagHeader &, map_marker_type);
    typedef map_marker *(*marker_parser)(const std::string &);
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
    std::string describe() const;
    static map_marker *read(tagHeader &, map_marker_type);
    static map_marker *parse(const std::string &s) throw (std::string);
    
public:
    dungeon_feature_type feat;
};

class map_timed_feature_marker : public map_feature_marker, dgn_event_listener
{
public:
    map_timed_feature_marker(const coord_def &pos = coord_def(),
                             int duration_turns = 0,
                             dungeon_feature_type feat = DNGN_FLOOR);
    void activate();
    void write(tagHeader &) const;
    void read(tagHeader &);
    std::string describe() const;

    void notify_dgn_event(const dgn_event &e);

    // Expires this marker *now* and cleans it up.
    void timeout(bool verbose);
    
    static map_marker *read(tagHeader &, map_marker_type);
    static map_marker *parse(const std::string &s) throw (std::string);

private:
    const char *bell_urgency(int ticks) const; 
    const char *noise_maker(int ticks) const;
   
public:
    // Ticks are a tenth of a turn.
    int duration_ticks;
    int warn_threshold;
};

void env_activate_markers();
void env_add_marker(map_marker *);
void env_remove_marker(map_marker *);
void env_remove_markers_at(const coord_def &c, map_marker_type);
map_marker *env_find_marker(const coord_def &c, map_marker_type);
std::vector<map_marker*> env_get_markers(const coord_def &c);
void env_clear_markers();

#endif
