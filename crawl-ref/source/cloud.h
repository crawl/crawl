/**
 * @file
 * @brief Functions related to clouds.
**/

#pragma once

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

cloud_struct* cloud_at(coord_def pos);

cloud_type cloud_type_at(const coord_def &pos);
bool cloud_is_yours_at(const coord_def &pos);

void delete_all_clouds();
void delete_cloud(coord_def p);
void remove_tornado_clouds(mid_t whose);
void move_cloud(coord_def src, coord_def newpos);
void swap_clouds(coord_def p1, coord_def p2);

coord_def random_walk(coord_def start, int dist);

void check_place_cloud(cloud_type cl_type, const coord_def& p, int lifetime,
                       const actor *agent, int spread_rate = -1,
                       int excl_rad = -1);
void place_cloud(cloud_type cl_type, const coord_def& ctarget,
                 int cl_range, const actor *agent,
                 int spread_rate = -1, int excl_rad = -1);

void manage_clouds();
void run_cloud_spreaders(int dur);
int max_cloud_damage(cloud_type cl_type, int power = -1);
int actor_apply_cloud(actor *act);
bool actor_cloud_immune(const actor &act, const cloud_struct &cloud);
bool actor_cloud_immune(const actor &act, cloud_type type);
bool mons_avoids_cloud(const monster* mons, coord_def pos,
                       bool placement = false);

colour_t get_cloud_colour(const cloud_struct &cloud);
coord_def get_cloud_originator(const coord_def& pos);

bool is_damaging_cloud(cloud_type type, bool temp = false, bool yours = false);
bool is_harmless_cloud(cloud_type type);
bool is_opaque_cloud(cloud_type ctype);
string cloud_type_name(cloud_type type, bool terse = true);
cloud_type random_smoke_type();
cloud_type cloud_name_to_type(const string &name);
const cloud_tile_info& cloud_type_tile_info(cloud_type type);

void start_still_winds();
void end_still_winds();

void manage_cloud_generators();

enum class special_fog_type {
    normal,
    chained,
};

class CloudGenerator
{
public:
    CloudGenerator
    (coord_def _loc, cloud_type _type,
        int _walk_dist, int _pow_min,
        int _pow_max, int _pow_rolls,
        kill_category _kc, int _size_min,
        int _size_max, int _spread_rate,
        int _start_clouds, int _excl_rad,
        int _size_buildup_amnt, int _size_buildup_time,
        int _spread_buildup_amnt, int _spread_buildup_time,
        int _buildup_turns, int _delay_min,
        int _delay_max, special_fog_type _special,
        string _marker_id, vector<string> _slaves)
        :loc(_loc), type(_type), walk_dist(_walk_dist),
        pow_min(_pow_min), pow_max(_pow_max),
        pow_rolls(_pow_rolls), kc(_kc),
        size_min(_size_max), size_max(_size_max),
        spread_rate(_spread_rate),
        start_clouds(_start_clouds),
        exclusion_radius(_excl_rad),
        size_buildup_amnt(_size_buildup_amnt),
        size_buildup_time(_size_buildup_time),
        spread_buildup_amnt(_spread_buildup_amnt),
        spread_buildup_time(_spread_buildup_time),
        buildup_turns(_buildup_turns), delay_min(_delay_min),
        delay_max(_delay_max), special(_special),
        marker_id(_marker_id), slaves(_slaves)
    {
        delay = delay_max;
    }

    //Main loop for cloud generators
    void manage();
    //Actual "create a cloud please" code
    void run(int runs);
    //Whether this cloud generator has been run via a managed group
    int touched = 0;

    coord_def getLoc() const { return loc; }
    cloud_type getType() const { return type; }

private:
    /// Center of cloud generation.
    coord_def loc;
    /// Type of cloud to generate (e.g. CLOUD_BLUE_SMOKE)
    cloud_type type;
    /// The distance to move over the course of one random walk.
    int walk_dist;
    /// Cloud lifetime: [pow_min, pow_max]. More pow_rolls decrease variance.
    int pow_min, pow_max, pow_rolls;
    /// Kill category
    kill_category kc;
    /// # of clouds that are generated with each firing: [size_min, size_max].
    int size_min, size_max;
    /// Rate at which clouds spread; -1 is default for type, otherwise [0-100].
    int spread_rate;
    /**
     * The number of clouds to lay when the level containing the cloud machine
     * is entered. (Clouds are cleared when the player leaves a level.)
    */
    int start_clouds;
    /**
     * Size of exclusions automatically placed around clouds from this
     * generator. If 0, no exclusion will be placed.
    */
    int exclusion_radius;
    /**
     * If nonzero, adds
     * (size_buildup_amnt / size_buildup_max * size_buildup_time)
     * to size_min and size_max, maxing out at size_buildup_amnt.
    */
    int size_buildup_amnt, size_buildup_time;
    int spread_buildup_amnt, spread_buildup_time;
    /// How long has this generator existed?
    int buildup_turns;
    /// Delay between each cloud placement: [delay_min, delay_max].
    int delay_min, delay_max;
    /// Whether this cloud is part of a managed group
    special_fog_type special;
    /// Synchronized generator variables passed from the Lua
    string marker_id;
    vector<string> slaves;


    // Stored pointers to managed cloud generators
    vector<CloudGenerator*> slave_references;

    // How long until the next cloud generation event?
    int delay;

    pair<int, int> get_size_range() const;
    int get_spread() const;
};
