#ifndef MAP_KNOWLEDGE_H
#define MAP_KNOWLEDGE_H

#include "show.h"

/*
 * A map_cell stores what the player knows about a cell.
 * These go in env.map_knowledge_knowledge.
 */
struct map_cell
{
    show_type object;       // The object: monster, item, feature, or cloud.
    unsigned short flags;   // Flags describing the mappedness of this square.
    unsigned short colour;

    map_cell() : object(), flags(0), colour(0) { }
    void clear() { flags = colour = 0; object = show_type(); }

    unsigned glyph() const;
    bool known() const;
    bool seen() const;
};

void set_map_knowledge_obj(const coord_def& where, show_type object);
unsigned get_map_knowledge_char(int x, int y);
inline unsigned get_map_knowledge_char(const coord_def& c) {
    return get_map_knowledge_char(c.x, c.y);
}
show_type get_map_knowledge_obj(int x, int y);
inline show_type get_map_knowledge_obj(const coord_def& c) {
    return get_map_knowledge_obj(c.x, c.y);
}
void set_map_knowledge_detected_item(int x, int y, bool detected = true);
inline void set_map_knowledge_detected_item(const coord_def& c, bool detected = true) {
    set_map_knowledge_detected_item(c.x, c.y, detected);
}

void set_map_knowledge_detected_mons(int x, int y, bool detected = true);
inline void set_map_knowledge_detected_mons(const coord_def& c, bool detected = true) {
    set_map_knowledge_detected_mons(c.x, c.y, detected);
}
void set_map_knowledge_col( int x, int y, int colour, int flags );
void set_map_knowledge_col( int x, int y, int colour );

bool is_map_knowledge_detected_item(int x, int y);
inline bool is_map_knowledge_detected_item(const coord_def& c) {
    return is_map_knowledge_detected_item(c.x, c.y);
}

bool is_map_knowledge_detected_mons(int x, int y);
inline bool is_map_knowledge_detected_mons(const coord_def& c) {
    return is_map_knowledge_detected_mons(c.x, c.y);
}
bool is_map_knowledge_item(int x, int y);
inline bool is_map_knowledge_item(const coord_def& c) {
    return is_map_knowledge_item(c.x, c.y);
}
void set_terrain_mapped( int x, int y );
inline void set_terrain_mapped( const coord_def& c ) {
    set_terrain_mapped(c.x,c.y);
}
void set_terrain_seen( int x, int y );
inline void set_terrain_seen( const coord_def& c ) {
    set_terrain_seen(c.x, c.y);
}
void set_terrain_changed( int x, int y );
bool is_terrain_known( int x, int y );
bool is_terrain_seen( int x, int y );
bool is_terrain_changed( int x, int y );
inline bool is_terrain_changed( const coord_def& c ) {
    return is_terrain_changed(c.x,c.y);
}
bool is_terrain_known(const coord_def &p);
bool is_terrain_mapped(const coord_def &p);
bool is_notable_terrain(dungeon_feature_type ftype);

inline bool is_terrain_seen(const coord_def &c)
{
    return (is_terrain_seen(c.x, c.y));
}

inline void set_terrain_changed(const coord_def &c)
{
    set_terrain_changed(c.x, c.y);
}

int count_detected_mons(void);
void clear_map(bool clear_items = true, bool clear_mons = true);

int get_map_knowledge_col(const coord_def& p);

void set_map_knowledge_glyph(const coord_def& c, show_type object, int col);

void clear_map_knowledge_grid(const coord_def& p);

#endif
