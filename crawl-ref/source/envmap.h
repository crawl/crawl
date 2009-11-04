#ifndef ENVMAP_H
#define ENVMAP_H

#include "show.h"

struct coord_def;

void set_envmap_obj(const coord_def& where, show_type object);
unsigned get_envmap_char(int x, int y);
inline unsigned get_envmap_char(const coord_def& c) {
    return get_envmap_char(c.x, c.y);
}
show_type get_envmap_obj(int x, int y);
inline show_type get_envmap_obj(const coord_def& c) {
    return get_envmap_obj(c.x, c.y);
}
void set_envmap_detected_item(int x, int y, bool detected = true);
inline void set_envmap_detected_item(const coord_def& c, bool detected = true) {
    set_envmap_detected_item(c.x, c.y, detected);
}

void set_envmap_detected_mons(int x, int y, bool detected = true);
inline void set_envmap_detected_mons(const coord_def& c, bool detected = true) {
    set_envmap_detected_mons(c.x, c.y, detected);
}
void set_envmap_col( int x, int y, int colour, int flags );
void set_envmap_col( int x, int y, int colour );
bool is_sanctuary( const coord_def& p );
bool is_bloodcovered( const coord_def& p );

bool is_envmap_detected_item(int x, int y);
inline bool is_envmap_detected_item(const coord_def& c) {
    return is_envmap_detected_item(c.x, c.y);
}

bool is_envmap_detected_mons(int x, int y);
inline bool is_envmap_detected_mons(const coord_def& c) {
    return is_envmap_detected_mons(c.x, c.y);
}
bool is_envmap_item(int x, int y);
inline bool is_envmap_item(const coord_def& c) {
    return is_envmap_item(c.x, c.y);
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

int get_envmap_col(const coord_def& p);

void set_envmap_glyph(const coord_def& c, show_type object, int col);

void clear_envmap_grid(const coord_def& p);

#endif

