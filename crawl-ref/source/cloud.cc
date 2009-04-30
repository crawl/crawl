/*
 *  File:       cloud.cc
 *  Summary:    Functions related to clouds.
 *  Written by: Brent Ross
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Creating a cloud module so all the cloud stuff can be isolated.
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <algorithm>

#include "externs.h"

#include "branch.h"
#include "cloud.h"
#include "it_use2.h"
#include "mapmark.h"
#include "ouch.h"
#include "place.h"
#include "player.h"
#include "spells4.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"

static int _actual_spread_rate(cloud_type type, int spread_rate)
{
    if (spread_rate >= 0)
        return spread_rate;

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

static bool _killer_whose_match(kill_category whose, killer_type killer)
{
    switch (whose)
    {
        case KC_YOU:
            return (killer == KILL_YOU_MISSILE || killer == KILL_YOU_CONF);

        case KC_FRIENDLY:
            return (killer == KILL_MON_MISSILE || killer == KILL_YOU_CONF);

        case KC_OTHER:
            return (killer == KILL_MON_MISSILE || killer == KILL_MISC);

        case KC_NCATEGORIES:
            ASSERT(false);
    }
    return (false);
}

static void _new_cloud( int cloud, cloud_type type, const coord_def& p,
                        int decay, kill_category whose, killer_type killer,
                        unsigned char spread_rate )
{
    ASSERT( env.cloud[cloud].type == CLOUD_NONE );
    ASSERT(_killer_whose_match(whose, killer));

    cloud_struct& c = env.cloud[cloud];

    c.type        = type;
    c.decay       = decay;
    c.pos         = p;
    c.whose       = whose;
    c.killer      = killer;
    c.spread_rate = spread_rate;
    env.cgrid(p)  = cloud;
    env.cloud_no++;
}

static void _place_new_cloud(cloud_type cltype, const coord_def& p, int decay,
                             kill_category whose, killer_type killer,
                             int spread_rate)
{
    if (env.cloud_no >= MAX_CLOUDS)
        return;

    // Find slot for cloud.
    for (int ci = 0; ci < MAX_CLOUDS; ci++)
    {
        if (env.cloud[ci].type == CLOUD_NONE)   // i.e., is empty
        {
            _new_cloud( ci, cltype, p, decay, whose, killer, spread_rate );
            break;
        }
    }
}

static int _spread_cloud(const cloud_struct &cloud)
{
    const int spreadch = cloud.decay > 30? 80 :
                         cloud.decay > 20? 50 :
                                           30;
    int extra_decay = 0;
    for ( adjacent_iterator ai(cloud.pos); ai; ++ai )
    {
        if (random2(100) >= spreadch)
            continue;

        if (!in_bounds(*ai)
            || env.cgrid(*ai) != EMPTY_CLOUD
            || grid_is_solid(grd(*ai))
            || is_sanctuary(*ai) && !is_harmless_cloud(cloud.type))
        {
            continue;
        }

        int newdecay = cloud.decay / 2 + 1;
        if (newdecay >= cloud.decay)
            newdecay = cloud.decay - 1;

        _place_new_cloud( cloud.type, *ai, newdecay, cloud.whose, cloud.killer,
                          cloud.spread_rate );

        extra_decay += 8;
    }

    return (extra_decay);
}

static void _dissipate_cloud(int cloudidx, int dissipate)
{
    cloud_struct &cloud = env.cloud[cloudidx];
    // Apply calculated rate to the actual cloud.
    cloud.decay -= dissipate;

    if (x_chance_in_y(cloud.spread_rate, 100))
    {
        cloud.spread_rate -= div_rand_round(cloud.spread_rate, 10);
        cloud.decay       -= _spread_cloud(cloud);
    }

    // Check for total dissipation and handle accordingly.
    if (cloud.decay < 1)
        delete_cloud(cloudidx);
}

void manage_clouds()
{
    for (int i = 0; i < MAX_CLOUDS; ++i)
    {
        cloud_struct& cloud = env.cloud[i];

        if (cloud.type == CLOUD_NONE)
            continue;

        int dissipate = you.time_taken;

        // Fire clouds dissipate faster over water,
        // cold clouds dissipate faster over lava.
        if (cloud.type == CLOUD_FIRE && grd(cloud.pos) == DNGN_DEEP_WATER)
            dissipate *= 4;
        else if (cloud.type == CLOUD_COLD && grd(cloud.pos) == DNGN_LAVA)
            dissipate *= 4;

        expose_items_to_element(cloud2beam(cloud.type), cloud.pos, 2);

        _dissipate_cloud(i, dissipate);
    }
}

void delete_cloud( int cloud )
{
    cloud_struct& c = env.cloud[cloud];
    if (c.type != CLOUD_NONE)
    {
        c.type        = CLOUD_NONE;
        c.decay       = 0;
        c.whose       = KC_OTHER;
        c.killer      = KILL_NONE;
        c.spread_rate = 0;

        env.cgrid(c.pos) = EMPTY_CLOUD;
        c.pos.reset();
        env.cloud_no--;
    }
}

// The current use of this function is for shifting in the abyss, so
// that clouds get moved along with the rest of the map.
void move_cloud( int cloud, const coord_def& newpos )
{
    if (cloud != EMPTY_CLOUD)
    {
        const coord_def oldpos = env.cloud[cloud].pos;
        env.cgrid(oldpos) = EMPTY_CLOUD;
        env.cgrid(newpos) = cloud;
        env.cloud[cloud].pos = newpos;
    }
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud( cloud_type cl_type, const coord_def& p, int lifetime,
                        kill_category whose, int spread_rate )
{
    check_place_cloud(cl_type, p, lifetime, whose,
                      cloud_struct::whose_to_killer(whose), spread_rate);
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud( cloud_type cl_type, const coord_def& p, int lifetime,
                        killer_type killer, int spread_rate )
{
    check_place_cloud(cl_type, p, lifetime,
                      cloud_struct::killer_to_whose(killer), killer,
                      spread_rate);
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud( cloud_type cl_type, const coord_def& p, int lifetime,
                        kill_category whose, killer_type killer,
                        int spread_rate )
{
    if (!in_bounds(p) || env.cgrid(p) != EMPTY_CLOUD)
        return;

    place_cloud( cl_type, p, lifetime, whose, killer, spread_rate );
}

int steam_cloud_damage(const cloud_struct &cloud)
{
    int decay = cloud.decay;
    decay = std::min(decay, 60);
    decay = std::max(decay, 10);

    // Damage in range 3 - 16.
    return ((decay * 13 + 20) / 50);
}

//   Places a cloud with the given stats. May delete old clouds to
//   make way if there are too many on level. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(cloud_type cl_type, const coord_def& ctarget, int cl_range,
                 kill_category whose, int _spread_rate)
{
    place_cloud(cl_type, ctarget, cl_range, whose,
                cloud_struct::whose_to_killer(whose), _spread_rate);
}

//   Places a cloud with the given stats. May delete old clouds to
//   make way if there are too many on level. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(cloud_type cl_type, const coord_def& ctarget, int cl_range,
                 killer_type killer, int _spread_rate)
{
    place_cloud(cl_type, ctarget, cl_range,
                cloud_struct::killer_to_whose(killer), killer, _spread_rate);
}

//   Places a cloud with the given stats. May delete old clouds to
//   make way if there are too many on level. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(cloud_type cl_type, const coord_def& ctarget, int cl_range,
                 kill_category whose, killer_type killer, int _spread_rate)
{
    if (is_sanctuary(ctarget) && !is_harmless_cloud(cl_type))
        return;

    int cl_new = -1;

    const int target_cgrid = env.cgrid(ctarget);
    if (target_cgrid != EMPTY_CLOUD)
    {
        // There's already a cloud here. See if we can overwrite it.
        cloud_struct& old_cloud = env.cloud[target_cgrid];
        if (old_cloud.type >= CLOUD_GREY_SMOKE && old_cloud.type <= CLOUD_STEAM
            || old_cloud.type == CLOUD_STINK
            || old_cloud.type == CLOUD_BLACK_SMOKE
            || old_cloud.type == CLOUD_MIST
            || old_cloud.decay <= 20) // soon gone
        {
            // Delete this cloud and replace it.
            cl_new = target_cgrid;
            delete_cloud(target_cgrid);
        }
        else                    // Guess not.
            return;
    }

    const int spread_rate = _actual_spread_rate(cl_type, _spread_rate);

    // Too many clouds.
    if (env.cloud_no >= MAX_CLOUDS)
    {
        // Default to random in case there's no low quality clouds.
        int cl_del = random2(MAX_CLOUDS);

        for (int ci = 0; ci < MAX_CLOUDS; ci++)
        {
            cloud_struct& cloud = env.cloud[ci];
            if (cloud.type >= CLOUD_GREY_SMOKE && cloud.type <= CLOUD_STEAM
                || cloud.type == CLOUD_STINK
                || cloud.type == CLOUD_BLACK_SMOKE
                || cloud.type == CLOUD_MIST
                || cloud.decay <= 20) // soon gone
            {
                cl_del = ci;
                break;
            }
        }

        delete_cloud(cl_del);
        cl_new = cl_del;
    }

    // Create new cloud.
    if (cl_new != -1)
    {
        _new_cloud( cl_new, cl_type, ctarget, cl_range * 10,
                    whose, killer, spread_rate );
    }
    else
    {
        // Find slot for cloud.
        for (int ci = 0; ci < MAX_CLOUDS; ci++)
        {
            if (env.cloud[ci].type == CLOUD_NONE)   // ie is empty
            {
                _new_cloud( ci, cl_type, ctarget, cl_range * 10,
                            whose, killer, spread_rate );
                break;
            }
        }
    }
}

bool is_opaque_cloud(unsigned char cloud_idx)
{
    if (cloud_idx == EMPTY_CLOUD)
        return (false);

    const int ctype = env.cloud[cloud_idx].type;
    return (ctype == CLOUD_BLACK_SMOKE
            || ctype >= CLOUD_GREY_SMOKE && ctype <= CLOUD_STEAM);
}

cloud_type cloud_type_at(const coord_def &c)
{
    const int cloudno = env.cgrid(c);
    return (cloudno == EMPTY_CLOUD ? CLOUD_NONE
                                   : env.cloud[cloudno].type);
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

cloud_type beam2cloud(beam_type flavour)
{
    switch (flavour)
    {
    default:
    case BEAM_NONE:
        return CLOUD_NONE;
    case BEAM_FIRE:
    case BEAM_POTION_FIRE:
        return CLOUD_FIRE;
    case BEAM_POTION_STINKING_CLOUD:
        return CLOUD_STINK;
    case BEAM_COLD:
    case BEAM_POTION_COLD:
        return CLOUD_COLD;
    case BEAM_POISON:
    case BEAM_POTION_POISON:
        return CLOUD_POISON;
    case BEAM_POTION_BLACK_SMOKE:
        return CLOUD_BLACK_SMOKE;
    case BEAM_POTION_GREY_SMOKE:
        return CLOUD_GREY_SMOKE;
    case BEAM_POTION_BLUE_SMOKE:
        return CLOUD_BLUE_SMOKE;
    case BEAM_POTION_PURP_SMOKE:
        return CLOUD_PURP_SMOKE;
    case BEAM_STEAM:
    case BEAM_POTION_STEAM:
        return CLOUD_STEAM;
    case BEAM_MIASMA:
    case BEAM_POTION_MIASMA:
        return CLOUD_MIASMA;
    case BEAM_CHAOS:
        return CLOUD_CHAOS;
    case BEAM_RANDOM:
        return CLOUD_RANDOM;
    }
}

beam_type cloud2beam(cloud_type flavour)
{
    switch (flavour)
    {
    default:
    case CLOUD_NONE:        return BEAM_NONE;
    case CLOUD_FIRE:        return BEAM_FIRE;
    case CLOUD_STINK:       return BEAM_POTION_STINKING_CLOUD;
    case CLOUD_COLD:        return BEAM_COLD;
    case CLOUD_POISON:      return BEAM_POISON;
    case CLOUD_BLACK_SMOKE: return BEAM_POTION_BLACK_SMOKE;
    case CLOUD_GREY_SMOKE:  return BEAM_POTION_GREY_SMOKE;
    case CLOUD_BLUE_SMOKE:  return BEAM_POTION_BLUE_SMOKE;
    case CLOUD_PURP_SMOKE:  return BEAM_POTION_PURP_SMOKE;
    case CLOUD_STEAM:       return BEAM_STEAM;
    case CLOUD_MIASMA:      return BEAM_MIASMA;
    case CLOUD_CHAOS:       return BEAM_CHAOS;
    case CLOUD_RANDOM:      return BEAM_RANDOM;
    }
}

void in_a_cloud()
{
    int cl = env.cgrid(you.pos());
    int hurted = 0;
    int resist;

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        remove_condensation_shield();

    switch (env.cloud[cl].type)
    {
    case CLOUD_FIRE:
        if (you.duration[DUR_FIRE_SHIELD])
            return;

        mpr("You are engulfed in roaring flames!");

        resist = player_res_fire();

        if (resist <= 0)
        {
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;

            if (resist < 0)
                hurted += ((random2avg(14, 2) + 3) * you.time_taken) / 10;

            hurted -= random2(player_AC());

            if (hurted < 0)
                hurted = 0;
            else
                ouch(hurted, cl, KILLED_BY_CLOUD, "flame");
        }
        else
        {
            canned_msg(MSG_YOU_RESIST);
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;
            hurted /= (1 + resist * resist);
            ouch(hurted, cl, KILLED_BY_CLOUD, "flame");
        }
        expose_player_to_element(BEAM_FIRE, 7);
        break;

    case CLOUD_STINK:
        // If you don't have to breathe, unaffected
        mpr("You are engulfed in noxious fumes!");
        if (player_res_poison())
            break;

        hurted += (random2(3) * you.time_taken) / 10;
        if (hurted < 1)
            hurted = 0;
        else
            ouch((hurted * you.time_taken) / 10, cl, KILLED_BY_CLOUD,
                 "noxious fumes");

        if (1 + random2(27) >= you.experience_level)
        {
            mpr("You choke on the stench!");
            confuse_player( (coinflip() ? 3 : 2) );
        }
        break;

    case CLOUD_COLD:
        mpr("You are engulfed in freezing vapours!");

        resist = player_res_cold();

        if (resist <= 0)
        {
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;

            if (resist < 0)
                hurted += ((random2avg(14, 2) + 3) * you.time_taken) / 10;

            hurted -= random2(player_AC());
            if (hurted < 0)
                hurted = 0;

            ouch(hurted, cl, KILLED_BY_CLOUD, "freezing vapour");
        }
        else
        {
            canned_msg(MSG_YOU_RESIST);
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;
            hurted /= (1 + resist * resist);
            ouch(hurted, cl, KILLED_BY_CLOUD, "freezing vapour");
        }
        expose_player_to_element(BEAM_COLD, 7);
        break;

    case CLOUD_POISON:
        // If you don't have to breathe, unaffected
        mpr("You are engulfed in poison gas!");
        if (!player_res_poison())
        {
            ouch((random2(10) * you.time_taken) / 10, cl, KILLED_BY_CLOUD,
                 "poison gas");
            poison_player(1);
        }
        break;

    case CLOUD_GREY_SMOKE:
    case CLOUD_BLUE_SMOKE:
    case CLOUD_PURP_SMOKE:
    case CLOUD_BLACK_SMOKE:
        mpr("You are engulfed in a cloud of smoke!");
        break;

    case CLOUD_STEAM:
    {
        mpr("You are engulfed in a cloud of scalding steam!");
        if (player_res_steam() > 0)
        {
            mpr("It doesn't seem to affect you.");
            return;
        }

        const int base_dam = steam_cloud_damage(env.cloud[cl]);
        hurted += (random2avg(base_dam, 2) * you.time_taken) / 10;

        const int res_fire = player_res_fire();
        if (res_fire < 0)
            hurted += (random2(base_dam / 2 + 1) * you.time_taken) / 10;
        else if (res_fire)
            hurted /= 1 + (res_fire / 2);

        if (hurted < 0)
            hurted = 0;

        ouch((hurted * you.time_taken) / 10, cl, KILLED_BY_CLOUD,
             "steam");
        break;
    }

    case CLOUD_MIASMA:
        mpr("You are engulfed in a dark miasma.");

        if (x_chance_in_y(player_prot_life(), 3))
            return;

        poison_player(1);

        hurted += (random2avg(12, 3) * you.time_taken) / 10;    // 3

        if (hurted < 0)
            hurted = 0;

        ouch(hurted, cl, KILLED_BY_CLOUD, "foul pestilence");
        potion_effect(POT_SLOWING, 5);

        if (you.hp_max > 4 && coinflip())
            rot_hp(1);

        break;
    default:
        break;
    }

    return;
}                               // end in_a_cloud()

bool is_damaging_cloud(cloud_type type, bool temp)
{
    switch (type)
    {
    // always harmful...
    case CLOUD_FIRE:
        // ... unless a Ring of Flames is up and it's a fire cloud.
        if (temp && you.duration[DUR_FIRE_SHIELD])
            return (false);
    case CLOUD_CHAOS:
    case CLOUD_COLD:
        return (true);

    // Only harmful if the player doesn't have the necessary resistances.
    // Takes into account what the player can *know* and what s/he can
    // also expect to be the case a few turns later (ignores spells).
    case CLOUD_STINK:
    case CLOUD_POISON:
        return (!player_res_poison(false, temp));
    case CLOUD_STEAM:
        return (player_res_steam(false, temp) <= 0);
    case CLOUD_MIASMA:
        return (player_prot_life(false, temp) <= 2);

    default:
        // Smoke, never harmful.
        return (false);
    }
}

bool is_harmless_cloud(cloud_type type)
{
    switch (type)
    {
    case CLOUD_NONE:
    case CLOUD_BLACK_SMOKE:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLUE_SMOKE:
    case CLOUD_PURP_SMOKE:
    case CLOUD_MIST:
    case CLOUD_DEBUGGING:
        return (true);
    default:
        return (false);
    }
}

std::string cloud_name(cloud_type type)
{
    switch (type)
    {
    case CLOUD_FIRE:
        return "flame";
    case CLOUD_STINK:
        return "noxious fumes";
    case CLOUD_COLD:
        return "freezing vapour";
    case CLOUD_POISON:
        return "poison gases";
    case CLOUD_GREY_SMOKE:
        return "grey smoke";
    case CLOUD_BLUE_SMOKE:
        return "blue smoke";
    case CLOUD_PURP_SMOKE:
        return "purple smoke";
    case CLOUD_STEAM:
        return "steam";
    case CLOUD_MIASMA:
        return "foul pestilence";
    case CLOUD_BLACK_SMOKE:
        return "black smoke";
    case CLOUD_MIST:
        return "thin mist";
    case CLOUD_CHAOS:
        return "seething chaos";
    default:
        return "buggy goodness";
    }
}

////////////////////////////////////////////////////////////////////////
// cloud_struct

kill_category cloud_struct::killer_to_whose(killer_type killer)
{
    switch (killer)
    {
        case KILL_YOU:
        case KILL_YOU_MISSILE:
        case KILL_YOU_CONF:
            return (KC_YOU);

        case KILL_MON:
        case KILL_MON_MISSILE:
        case KILL_MISC:
            return (KC_OTHER);

        default:
            ASSERT(false);
    }
    return (KC_OTHER);
}

killer_type cloud_struct::whose_to_killer(kill_category whose)
{
    switch (whose)
    {
        case KC_YOU:         return(KILL_YOU_MISSILE);
        case KC_FRIENDLY:    return(KILL_MON_MISSILE);
        case KC_OTHER:       return(KILL_MON_MISSILE);
        case KC_NCATEGORIES: ASSERT(false);
    }
    return (KILL_NONE);
}

void cloud_struct::set_whose(kill_category _whose)
{
    whose  = _whose;
    killer = whose_to_killer(whose);
}

void cloud_struct::set_killer(killer_type _killer)
{
    killer = _killer;
    whose  = killer_to_whose(killer);

    switch (killer)
    {
        case KILL_YOU:
            killer = KILL_YOU_MISSILE;
            break;

        case KILL_MON:
            killer = KILL_MON_MISSILE;
            break;

        default:
            break;
     }
}
//////////////////////////////////////////////////////////////////////////
// Fog machine stuff

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
        mprf(MSGCH_ERROR, "Error while making fog machine: %s",
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
        return (false);

    if (data.cl_type <= CLOUD_NONE || (data.cl_type >= CLOUD_RANDOM
                                       && data.cl_type != CLOUD_DEBUGGING))
        return (false);

    if (data.size < 1 || data.power < 1)
        return (false);

    return (true);
}

int num_fogs_for_place(int level_number, const level_id &place)
{
    if (level_number == -1)
        level_number = place.absdepth();

    switch (place.level_type)
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
        level_number = place.absdepth();

    switch (place.level_type)
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
