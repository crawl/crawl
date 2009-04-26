/*
 *  File:       view.h
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef VIEW_H
#define VIEW_H


#include "externs.h"

// various elemental colour schemes... used for abstracting random
// short lists. When adding colours, please also add their names in
// initfile.cc (str_to_colour)!
enum element_type
{
    ETC_FIRE = 32,      // fiery colours (must be first and > highest colour)
    ETC_ICE,            // icy colours
    ETC_EARTH,          // earthy colours
    ETC_ELECTRICITY,    // electrical side of air
    ETC_AIR,            // non-electric and general air magic
    ETC_POISON,         // used only for venom mage and stalker stuff
    ETC_WATER,          // used only for the elemental
    ETC_MAGIC,          // general magical effect
    ETC_MUTAGENIC,      // transmute, poly, radiation effects
    ETC_WARP,           // teleportation and anything similar
    ETC_ENCHANT,        // magical enhancements
    ETC_HEAL,           // holy healing (not necromantic stuff)
    ETC_HOLY,           // general "good" god effects
    ETC_DARK,           // darkness
    ETC_DEATH,          // currently only assassin (and equal to ETC_NECRO)
    ETC_NECRO,          // necromancy stuff
    ETC_UNHOLY,         // demonology stuff
    ETC_VEHUMET,        // vehumet's oddball colours
    ETC_BEOGH,          // Beogh altar colours
    ETC_CRYSTAL,        // colours of crystal
    ETC_BLOOD,          // colours of blood
    ETC_SMOKE,          // colours of smoke
    ETC_SLIME,          // colours of slime
    ETC_JEWEL,          // colourful
    ETC_ELVEN,          // used for colouring elf fabric items
    ETC_DWARVEN,        // used for colouring dwarf fabric items
    ETC_ORCISH,         // used for colouring orc fabric items
    ETC_GILA,           // gila monster colours
    ETC_FLOOR,          // colour of the area's floor
    ETC_ROCK,           // colour of the area's rock
    ETC_STONE,          // colour of the area's stone
    ETC_MIST,           // colour of mist
    ETC_SHIMMER_BLUE,   // shimmering colours of blue.
    ETC_DECAY,          // colour of decay/swamp
    ETC_SILVER,         // colour of silver
    ETC_GOLD,           // colour of gold
    ETC_IRON,           // colour of iron
    ETC_BONE,           // colour of bone
    ETC_RANDOM          // any colour (except BLACK)
};

void init_char_table(char_set_type set);
void init_feature_table();
void init_monsters_seens();

void beogh_follower_convert(monsters *monster, bool orc_hit = false);
bool mons_near(const monsters *monster, unsigned short foe = MHITYOU);
bool mon_enemies_around(const monsters *monster);

void find_features(const std::vector<coord_def>& features,
        unsigned char feature, std::vector<coord_def> *found);

void losight(env_show_grid &sh, feature_grid &gr,
             const coord_def& center, bool clear_walls_block = false,
             bool ignore_clouds = false);


bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force = false);

bool noisy(int loudness, const coord_def& where, const char *msg = NULL,
           bool mermaid = false);
void blood_smell( int strength, const coord_def& where);
void handle_monster_shouts(monsters* monster, bool force = false);

void show_map( coord_def &spec_place, bool travel_mode );
void setLOSRadius(int newLR);
bool check_awaken(monsters* monster);
int count_detected_mons(void);
void clear_map(bool clear_items = true, bool clear_mons = true);
bool is_feature(int feature, const coord_def& where);
void get_item_glyph(const item_def *item, unsigned *glych,
                    unsigned short *glycol);
void get_mons_glyph(const monsters *mons, unsigned *glych,
                    unsigned short *glycol);
unsigned get_screen_glyph( int x, int y );
unsigned get_screen_glyph( const coord_def &p );
std::string stringize_glyph(unsigned glyph);
int multibyte_strlen(const std::string &s);

void get_item_symbol(unsigned int object, unsigned *ch,
                     unsigned short *colour);

// Applies ETC_ colour substitutions and brands.
unsigned real_colour(unsigned raw_colour);
int get_mons_colour(const monsters *mons);

const feature_def &get_feature_def(dungeon_feature_type feat);

void set_envmap_obj( const coord_def& where, int object );
unsigned get_envmap_char(int x, int y);
inline unsigned get_envmap_char(const coord_def& c) {
    return get_envmap_char(c.x, c.y);
}
bool inside_level_bounds(int x, int y);
bool inside_level_bounds(const coord_def &p);
int get_envmap_obj(int x, int y);
inline int get_envmap_obj(const coord_def& c) {
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
bool is_notable_terrain(dungeon_feature_type ftype);

inline bool is_terrain_seen(const coord_def &c)
{
    return (is_terrain_seen(c.x, c.y));
}

inline void set_terrain_changed(const coord_def &c)
{
    set_terrain_changed(c.x, c.y);
}

void clear_feature_overrides();
void add_feature_override(const std::string &text);
void clear_cset_overrides();
void add_cset_override(char_set_type set, const std::string &overrides);

bool see_grid( const env_show_grid &show,
               const coord_def &c,
               const coord_def &pos );
bool see_grid(const coord_def &p);
bool see_grid_no_trans( const coord_def &p );
bool trans_wall_blocking( const coord_def &p );
bool grid_see_grid(const coord_def& p1, const coord_def& p2,
                   dungeon_feature_type allowed = DNGN_UNSEEN);
unsigned grid_character_at(const coord_def &c);

inline bool see_grid( int grx, int gry )
{
    return see_grid(coord_def(grx, gry));
}

inline bool see_grid_no_trans(int x, int y)
{
    return see_grid_no_trans(coord_def(x, y));
}

inline bool trans_wall_blocking(int x, int y)
{
    return trans_wall_blocking(coord_def(x, y));
}

std::string screenshot(bool fullscreen = false);

dungeon_char_type get_feature_dchar( dungeon_feature_type feat );
unsigned dchar_glyph(dungeon_char_type dchar);
unsigned get_sightmap_char(int feature);
unsigned get_magicmap_char(int feature);

bool view_update();
void view_update_at(const coord_def &pos);
#ifndef USE_TILE
void flash_monster_colour(const monsters *mon, unsigned char fmc_colour,
                          int fmc_delay);
#endif
void calc_show_los();
void viewwindow(bool draw_it, bool do_updates);
void update_monsters_in_view();
void handle_seen_interrupt(monsters* monster);
void flush_comes_into_view();

struct ray_def;
bool find_ray( const coord_def& source, const coord_def& target,
               bool allow_fallback, ray_def& ray, int cycle_dir = 0,
               bool find_shortest = false, bool ignore_solid = false );

int num_feats_between(const coord_def& source, const coord_def& target,
                      dungeon_feature_type min_feat,
                      dungeon_feature_type max_feat,
                      bool exclude_endpoints = true,
                      bool just_check = false);

dungeon_char_type dchar_by_name(const std::string &name);

void handle_terminal_resize(bool redraw = true);

#if defined(WIN32CONSOLE) || defined(DOS) || defined(USE_TILE)
unsigned short dos_brand( unsigned short colour,
                          unsigned brand = CHATTR_REVERSE);
#endif

#define _monster_los_LSIZE (2 * LOS_RADIUS + 1)

// This class can be used to fill the entire surroundings (los_range)
// of a monster or other position with seen/unseen values, so as to be able
// to compare several positions within this range.
class monster_los
{
public:
    monster_los();
    virtual ~monster_los();

    // public methods
    void set_monster(monsters *mon);
    void set_los_centre(int x, int y);
    void set_los_centre(const coord_def& p) { this->set_los_centre(p.x, p.y); }
    void set_los_range(int r);
    void fill_los_field(void);
    bool in_sight(int x, int y);
    bool in_sight(const coord_def& p) { return this->in_sight(p.x, p.y); }

protected:
    // protected methods
    coord_def pos_to_index(coord_def &p);
    coord_def index_to_pos(coord_def &i);

    void set_los_value(int x, int y, bool blocked, bool override = false);
    int get_los_value(int x, int y);
    bool is_blocked(int x, int y);
    bool is_unknown(int x, int y);

    void check_los_beam(int dx, int dy);

    // The (fixed) size of the array.
    static const int LSIZE;

    static const int L_VISIBLE;
    static const int L_UNKNOWN;
    static const int L_BLOCKED;

    // The centre of our los field.
    int gridx, gridy;

    // Habitat checks etc. should be done for this monster.
    // Usually the monster whose LOS we're trying to calculate
    // (if mon->x == gridx, mon->y == gridy).
    // Else, any monster trying to move around within this los field.
    monsters *mons;

    // Range may never be greater than LOS_RADIUS!
    int range;

    // The array to store the LOS values.
    int los_field[_monster_los_LSIZE][_monster_los_LSIZE];
};

#endif
