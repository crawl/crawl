/**
 * @file
 * @brief Functions related to clouds.
**/

#pragma once

struct cloud_struct
{
    coord_def     pos;
    cloud_type    type;
    int           decay;
    uint8_t       spread_rate;
    kill_category whose;
    killer_type   killer;
    mid_t         source;
    int           excl_rad;

    cloud_struct() : pos(), type(CLOUD_NONE), decay(0), spread_rate(0),
                     whose(KC_OTHER), killer(KILL_NONE), excl_rad(-1)
    {
    }
    cloud_struct(coord_def p, cloud_type c, int d, int spread, kill_category kc,
                 killer_type kt, mid_t src, int excl);

    bool defined() const { return type != CLOUD_NONE; }
    bool temporary() const { return excl_rad == -1; }
    int exclusion_radius() const { return excl_rad; }

    actor *agent() const;
    void set_whose(kill_category _whose);
    void set_killer(killer_type _killer);

    string cloud_name(bool terse = false) const;
    void announce_actor_engulfed(const actor *engulfee,
                                 bool beneficial = false) const;

    static kill_category killer_to_whose(killer_type killer);
    static killer_type   whose_to_killer(kill_category whose);
};

enum cloud_tile_variation
{
    CTVARY_NONE,     ///< fixed tile (or special case)
    CTVARY_DUR,      ///< tile based on remaining cloud duration
    CTVARY_RANDOM,   ///< choose a random tile in set with every redraw
};

struct cloud_tile_info
{
    tileidx_t base;                  ///< The base tile for the cloud type.
    cloud_tile_variation variation;  ///< How (and if) the tile should vary.
};

#define MEPH_HD_CAP 21

#define BLASTSPARK_POWER_KEY "blastspark_power"
#define MIASMA_IMMUNE_KEY "miasma_immune"

cloud_struct* cloud_at(coord_def pos);

cloud_type cloud_type_at(const coord_def &pos);
bool cloud_is_yours_at(const coord_def &pos);

void delete_all_clouds();
void delete_cloud(coord_def p);
void remove_vortex_clouds(mid_t whose);
void move_cloud(coord_def src, coord_def newpos);
void swap_clouds(coord_def p1, coord_def p2);

coord_def random_walk(coord_def start, int dist);

void check_place_cloud(cloud_type cl_type, const coord_def& p, int lifetime,
                       const actor *agent, int spread_rate = -1,
                       int excl_rad = -1);
void place_cloud(cloud_type cl_type, const coord_def& ctarget,
                 int cl_range, const actor *agent,
                 int spread_rate = -1, int excl_rad = -1,
                 bool do_conducts = true);

void manage_clouds();
void run_cloud_spreaders(int dur);
string desc_cloud_damage(cloud_type cl_type, bool vs_player);
int actor_apply_cloud(actor *act);
bool actor_cloud_immune(const actor &act, const cloud_struct &cloud);
bool actor_cloud_immune(const actor &act, cloud_type type);
bool mons_avoids_cloud(const monster* mons, coord_def pos,
                       bool placement = false);

colour_t get_cloud_colour(const cloud_struct &cloud);
coord_def get_cloud_originator(const coord_def& pos);

bool is_damaging_cloud(cloud_type type, bool temp = false, bool yours = false);
bool cloud_damages_over_time(cloud_type type, bool temp = false, bool yours = false);
bool is_harmless_cloud(cloud_type type);
bool is_opaque_cloud(cloud_type ctype);
string cloud_type_name(cloud_type type, bool terse = true);
cloud_type random_smoke_type();
cloud_type cloud_name_to_type(const string &name);
const cloud_tile_info& cloud_type_tile_info(cloud_type type);

void start_still_winds();
void end_still_winds();
void surround_actor_with_cloud(const actor* a, cloud_type cloud);
