/*
 *  File:       cloud.cc
 *  Summary:    Functions related to clouds.
 *  Written by: Brent Ross
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Creating a cloud module so all the cloud stuff can be isolated.
 *
 *  Change History (most recent first):
 *
 *   <1>    Oct 1/2001   BWR    Created
 */

#include "AppHdr.h"
#include "externs.h"

#include "branch.h"
#include "cloud.h"
#include "mapmark.h"
#include "misc.h"
#include "place.h"
#include "stuff.h"
#include "terrain.h"

// Returns true if this cloud spreads out as it dissipates.
static unsigned char actual_spread_rate(cloud_type type, int spread_rate)
{
    if (spread_rate >= 0)
        return (unsigned char) spread_rate;

    switch (type)
    {
    case CLOUD_STEAM:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLACK_SMOKE:
        return 22;
    default:
        return 0;
    }
}

static void new_cloud( int cloud, cloud_type type, int x, int y, int decay,
                       kill_category whose, unsigned char spread_rate )
{
    ASSERT( env.cloud[ cloud ].type == CLOUD_NONE );

    env.cloud[ cloud ].type = type;
    env.cloud[ cloud ].decay = decay;
    env.cloud[ cloud ].x = x;
    env.cloud[ cloud ].y = y;
    env.cloud[ cloud ].whose = whose;
    env.cloud[ cloud ].spread_rate = spread_rate;
    env.cgrid[ x ][ y ] = cloud;
    env.cloud_no++;
}

static void place_new_cloud(cloud_type cltype, int x, int y, int decay,
                            kill_category whose, int spread_rate)
{
    if (env.cloud_no >= MAX_CLOUDS)
        return;

    // find slot for cloud
    for (int ci = 0; ci < MAX_CLOUDS; ci++)
    {
        if (env.cloud[ci].type == CLOUD_NONE)   // ie is empty
        {
            new_cloud( ci, cltype, x, y, decay, whose, spread_rate );
            break;
        }
    }
}

static int spread_cloud(const cloud_struct &cloud)
{
    const int spreadch = cloud.decay > 30? 80 :
                         cloud.decay > 20? 50 :
                                           30;
    int extra_decay = 0;

    for (int yi = -1; yi <= 1; ++yi)
    {
        for (int xi = -1; xi <= 1; ++xi)
        {
            if ((!xi && !yi) || random2(100) >= spreadch)
                continue;

            const int x = cloud.x + xi;
            const int y = cloud.y + yi;

            if (!in_bounds(x, y) 
                    || env.cgrid[x][y] != EMPTY_CLOUD
                    || grid_is_solid(grd[x][y]))
                continue;

            int newdecay = cloud.decay / 2 + 1;
            if (newdecay >= cloud.decay)
                newdecay = cloud.decay - 1;

            place_new_cloud( cloud.type, x, y, newdecay, cloud.whose,
                             cloud.spread_rate );

            extra_decay += 8;
        }
    }

    return (extra_decay);
}

static void dissipate_cloud(int cc, cloud_struct &cloud, int dissipate)
{
    // apply calculated rate to the actual cloud:
    cloud.decay -= dissipate;

    if (random2(100) < cloud.spread_rate)
    {
        cloud.spread_rate -= div_rand_round(cloud.spread_rate, 10);
        cloud.decay -= spread_cloud(cloud);
    }

    // check for total dissipation and handle accordingly:
    if (cloud.decay < 1)
        delete_cloud( cc );
}

void manage_clouds(void)
{
    // amount which cloud dissipates - must be unsigned! {dlb}
    unsigned int dissipate = 0;

    for (unsigned char cc = 0; cc < MAX_CLOUDS; cc++)
    {
        if (env.cloud[cc].type == CLOUD_NONE)   // no cloud -> next iteration
            continue;

        dissipate = you.time_taken;

        // water -> flaming clouds:
        // lava -> freezing clouds:
        if (env.cloud[cc].type == CLOUD_FIRE
            && grd[env.cloud[cc].x][env.cloud[cc].y] == DNGN_DEEP_WATER)
        {
            dissipate *= 4;
        }
        else if (env.cloud[cc].type == CLOUD_COLD
                && grd[env.cloud[cc].x][env.cloud[cc].y] == DNGN_LAVA)
        {
            dissipate *= 4;
        }

        dissipate_cloud( cc, env.cloud[cc], dissipate );
    }

    return;
}                               // end manage_clouds()

void delete_cloud( int cloud )
{
    if (env.cloud[ cloud ].type != CLOUD_NONE)
    {
        const int cloud_x = env.cloud[ cloud ].x;
        const int cloud_y = env.cloud[ cloud ].y;

        env.cloud[ cloud ].type = CLOUD_NONE;
        env.cloud[ cloud ].decay = 0;
        env.cloud[ cloud ].x = 0;
        env.cloud[ cloud ].y = 0;
        env.cloud[ cloud ].whose = KC_OTHER;
        env.cloud[ cloud ].spread_rate = 0;
        env.cgrid[ cloud_x ][ cloud_y ] = EMPTY_CLOUD;
        env.cloud_no--;
    }
}

// The current use of this function is for shifting in the abyss, so
// that clouds get moved along with the rest of the map. 
void move_cloud( int cloud, int new_x, int new_y )
{
    if (cloud != EMPTY_CLOUD)
    {
        const int old_x = env.cloud[ cloud ].x;
        const int old_y = env.cloud[ cloud ].y;

        env.cgrid[ new_x ][ new_y ] = cloud;
        env.cloud[ cloud ].x = new_x;
        env.cloud[ cloud ].y = new_y;
        env.cgrid[ old_x ][ old_y ] = EMPTY_CLOUD;
    }
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud( cloud_type cl_type, int x, int y, int lifetime,
                        kill_category whose, int spread_rate )
{
    if (!in_bounds(x, y) || env.cgrid[x][y] != EMPTY_CLOUD)
        return;

    place_cloud( cl_type, x, y, lifetime, whose, spread_rate );
}

int steam_cloud_damage(const cloud_struct &cloud)
{
    int decay = cloud.decay;
    if (decay > 60)
        decay = 60;
    else if (decay < 10)
        decay = 10;

    // Damage in range 3 - 16.
    return ((decay * 13 + 20) / 50);
}

//   Places a cloud with the given stats. May delete old clouds to
//   make way if there are too many on level. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(cloud_type cl_type, int ctarget_x,
                 int ctarget_y, int cl_range,
                 kill_category whose, int _spread_rate)
{
    int cl_new = -1;

    // more compact {dlb}
    const int target_cgrid = env.cgrid[ctarget_x][ctarget_y];

    // that is, another cloud already there {dlb}
    if (target_cgrid != EMPTY_CLOUD)
    {
        if ((env.cloud[ target_cgrid ].type >= CLOUD_GREY_SMOKE
                && env.cloud[ target_cgrid ].type <= CLOUD_STEAM)
            || env.cloud[ target_cgrid ].type == CLOUD_STINK
            || env.cloud[ target_cgrid ].type == CLOUD_BLACK_SMOKE
            || env.cloud[ target_cgrid ].type == CLOUD_MIST
            || env.cloud[ target_cgrid ].decay <= 20)     //soon gone
        {
            cl_new = env.cgrid[ ctarget_x ][ ctarget_y ];
            delete_cloud( env.cgrid[ ctarget_x ][ ctarget_y ] );
        }
        else
        {
            return;
        }
    }

    unsigned char spread_rate = actual_spread_rate( cl_type, _spread_rate );

    // too many clouds
    if (env.cloud_no >= MAX_CLOUDS) 
    {
        // default to random in case there's no low quality clouds
        int cl_del = random2( MAX_CLOUDS );

        for (int ci = 0; ci < MAX_CLOUDS; ci++)
        {
            if ((env.cloud[ ci ].type >= CLOUD_GREY_SMOKE
                    && env.cloud[ ci ].type <= CLOUD_STEAM)
                || env.cloud[ ci ].type == CLOUD_STINK
                || env.cloud[ ci ].type == CLOUD_BLACK_SMOKE
                || env.cloud[ ci ].type == CLOUD_MIST
                || env.cloud[ ci ].decay <= 20)     //soon gone
            {
                cl_del = ci;
                break;
            }
        }

        delete_cloud( cl_del );
        cl_new = cl_del;
    }

    // create new cloud
    if (cl_new != -1)
        new_cloud( cl_new, cl_type, ctarget_x, ctarget_y, cl_range * 10,
                   whose, spread_rate );
    else
    {
        // find slot for cloud
        for (int ci = 0; ci < MAX_CLOUDS; ci++)
        {
            if (env.cloud[ci].type == CLOUD_NONE)   // ie is empty
            {
                new_cloud( ci, cl_type, ctarget_x, ctarget_y, cl_range * 10,
                           whose, spread_rate );
                break;
            }
        }
    }
}                               // end place_cloud();

bool is_opaque_cloud(unsigned char cloud_idx)
{
    if ( cloud_idx == EMPTY_CLOUD )
        return false;
    const int ctype = env.cloud[cloud_idx].type;
    return ( ctype == CLOUD_BLACK_SMOKE ||
             (ctype >= CLOUD_GREY_SMOKE && ctype <= CLOUD_STEAM) );
}

cloud_type random_smoke_type()
{
    // excludes black (reproducing existing behaviour)
    switch ( random2(3) )
    {
    case 0: return CLOUD_GREY_SMOKE;
    case 1: return CLOUD_BLUE_SMOKE;
    case 2: return CLOUD_PURP_SMOKE;
    }
    return CLOUD_DEBUGGING;
}

void place_fog_machine(fog_machine_type fm_type, cloud_type cl_type,
                       int x, int y, int size, int power)
{
    ASSERT(fm_type >= FM_GEYSER && fm_type < NUM_FOG_MACHINE_TYPES);
    ASSERT(cl_type > CLOUD_NONE && (cl_type < CLOUD_RANDOM
                                    || cl_type == CLOUD_DEBUGGING));
    ASSERT(size  >= 1);
    ASSERT(power >= 1);

    const char* fog_types[] = {
        "geyser",
        "spread",
        "brownian"
    };

    try
    {
        char buf [160];
        sprintf(buf, "lua_mapless:fog_machine_%s(\"%s\", %d, %d)",
                fog_types[fm_type], cloud_name(cl_type).c_str(),
                size, power);

        map_marker *mark = map_lua_marker::parse_marker(buf, "");

        if (mark == NULL)
        {
            mprf(MSGCH_DIAGNOSTICS, "Unable to parse fog machine from '%s'",
                 buf);
            return;
        }

        mark->pos = coord_def(x, y);
        env.markers.add(mark);
    }
    catch (const std::string &err)
    {
        mprf(MSGCH_DIAGNOSTICS, "Error while making fog machine: %s",
             err.c_str());
    }
}

void place_fog_machine(fog_machine_data data, int x, int y)
{
    place_fog_machine(data.fm_type, data.cl_type, x, y, data.size,
                      data.power);
}

bool valid_fog_machine_data(fog_machine_data data)
{
    if (data.fm_type < FM_GEYSER ||  data.fm_type >= NUM_FOG_MACHINE_TYPES)
        return false;

    if (data.cl_type <= CLOUD_NONE || (data.cl_type >= CLOUD_RANDOM
                                       && data.cl_type != CLOUD_DEBUGGING))
        return false;

    if (data.size < 1 || data.power < 1)
        return false;

    return true;
}

int num_fogs_for_place(int level_number, const level_id &place)
{
    if (level_number == -1)
    {
        switch(place.level_type)
        {
        case LEVEL_DUNGEON:
            level_number = absdungeon_depth(place.branch, place.depth);
            break;
        case LEVEL_ABYSS:
            level_number = 51;
            break;
        case LEVEL_PANDEMONIUM:
            level_number = 52;
            break;
        default:
            level_number = you.your_level;
        }
    }

    switch(place.level_type)
    {
    case LEVEL_DUNGEON:
    {
        Branch &branch = branches[place.branch];
        ASSERT((branch.num_fogs_function == NULL
                && branch.rand_fog_function == NULL)
               || (branch.num_fogs_function != NULL
                   && branch.rand_fog_function != NULL));

        if (branch.num_fogs_function == NULL)
            return 0;

        return branch.num_fogs_function(level_number);
    }
    case LEVEL_ABYSS:
        return fogs_abyss_number(level_number);
    case LEVEL_PANDEMONIUM:
        return fogs_pan_number(level_number);
    case LEVEL_LABYRINTH:
        return fogs_lab_number(level_number);
    default:
        return 0;
    }

    return 0;
}

fog_machine_data random_fog_for_place(int level_number, const level_id &place)
{
    fog_machine_data data = {NUM_FOG_MACHINE_TYPES, CLOUD_NONE, -1, -1};

    if (level_number == -1)
    {
        switch(place.level_type)
        {
        case LEVEL_DUNGEON:
            level_number = absdungeon_depth(place.branch, place.depth);
            break;
        case LEVEL_ABYSS:
            level_number = 51;
            break;
        case LEVEL_PANDEMONIUM:
            level_number = 52;
            break;
        default:
            level_number = you.your_level;
        }
    }

    switch(place.level_type)
    {
    case LEVEL_DUNGEON:
    {
        Branch &branch = branches[place.branch];
        ASSERT(branch.num_fogs_function != NULL
                && branch.rand_fog_function != NULL);
        branch.rand_fog_function(level_number, data);
        return data;
    }
    case LEVEL_ABYSS:
        return fogs_abyss_type(level_number);
    case LEVEL_PANDEMONIUM:
        return fogs_pan_type(level_number);
    case LEVEL_LABYRINTH:
        return fogs_lab_type(level_number);
    default:
        ASSERT(false);
        return data;
    }

    ASSERT(false);
    return data;
}

int fogs_pan_number(int level_number)
{
    return 0;
}

fog_machine_data fogs_pan_type(int level_number)
{
    fog_machine_data data = {NUM_FOG_MACHINE_TYPES, CLOUD_NONE, -1, -1};

    return data;
}

int fogs_abyss_number(int level_number)
{
    return 0;
}

fog_machine_data fogs_abyss_type(int level_number)
{
    fog_machine_data data = {NUM_FOG_MACHINE_TYPES, CLOUD_NONE, -1, -1};

    return data;
}

int fogs_lab_number(int level_number)
{
    return 0;
}

fog_machine_data fogs_lab_type(int level_number)
{
    fog_machine_data data = {NUM_FOG_MACHINE_TYPES, CLOUD_NONE, -1, -1};

    return data;
}

////////////////////////////////////////////////////////////////////////
// cloud_struct

killer_type cloud_struct::beam_thrower() const
{
    return (whose == KC_YOU? KILL_YOU_MISSILE : KILL_MON_MISSILE);
}
