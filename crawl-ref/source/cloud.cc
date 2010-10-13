/*
 *  File:       cloud.cc
 *  Summary:    Functions related to clouds.
 *  Written by: Brent Ross
 *
 *  Creating a cloud module so all the cloud stuff can be isolated.
 */

#include "AppHdr.h"

#include <algorithm>

#include "externs.h"

#include "areas.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "dungeon.h"
#include "env.h"
#include "fight.h"
#include "fprop.h"
#include "godconduct.h"
#include "los.h"
#include "mapmark.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "random.h"
#include "stuff.h"
#include "terrain.h"
#ifdef USE_TILE
#include "tiledef-gui.h"
#include "tiledef-main.h"
#endif

int actual_spread_rate(cloud_type type, int spread_rate)
{
    if (spread_rate >= 0)
        return spread_rate;

    switch (type)
    {
    case CLOUD_GLOOM:
        return 50;
    case CLOUD_STEAM:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLACK_SMOKE:
        return 22;
    case CLOUD_RAIN:
    case CLOUD_INK:
        return 11;
    default:
        return 0;
    }
}

#ifdef ASSERTS
static bool _killer_whose_match(kill_category whose, killer_type killer)
{
    switch (whose)
    {
        case KC_YOU:
            return (killer == KILL_YOU_MISSILE || killer == KILL_YOU_CONF);

        case KC_FRIENDLY:
            return (killer == KILL_MON_MISSILE || killer == KILL_YOU_CONF
                    || killer == KILL_MON);

        case KC_OTHER:
            return (killer == KILL_MON_MISSILE || killer == KILL_MISCAST
                    || killer == KILL_MISC || killer == KILL_MON);

        case KC_NCATEGORIES:
            ASSERT(false);
    }
    return (false);
}
#endif

static bool _is_opaque_cloud(cloud_type ctype);

static void _los_cloud_changed(const coord_def& p, cloud_type t)
{
    if (_is_opaque_cloud(t))
        invalidate_los_around(p);
}

static void _new_cloud( int cloud, cloud_type type, const coord_def& p,
                        int decay, kill_category whose, killer_type killer,
                        uint8_t spread_rate, int colour, std::string name,
                        std::string tile)
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
    c.colour      = colour;
    c.name        = name;
#ifdef USE_TILE
    if (!tile.empty())
    {
        tileidx_t index;
        if (!tile_main_index(tile.c_str(), &index))
        {
            mprf(MSGCH_ERROR, "Invalid tile requested for cloud: '%s'.", tile.c_str());
            tile = "";
        }
    }
#endif
    c.tile        = tile;
    env.cgrid(p)  = cloud;
    env.cloud_no++;

    _los_cloud_changed(p, type);
}

static void _place_new_cloud(cloud_type cltype, const coord_def& p, int decay,
                             kill_category whose, killer_type killer,
                             int spread_rate = -1, int colour = -1,
                             std::string name = "",
                             std::string tile = "")
{
    if (env.cloud_no >= MAX_CLOUDS)
        return;

    // Find slot for cloud.
    for (int ci = 0; ci < MAX_CLOUDS; ci++)
    {
        if (env.cloud[ci].type == CLOUD_NONE)   // i.e., is empty
        {
            _new_cloud( ci, cltype, p, decay, whose, killer, spread_rate, colour,
                        name, tile );
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
            || feat_is_solid(grd(*ai))
            || is_sanctuary(*ai) && !is_harmless_cloud(cloud.type))
        {
            continue;
        }

        if (cloud.type == CLOUD_INK && !feat_is_watery(grd(*ai)))
            continue;

        int newdecay = cloud.decay / 2 + 1;
        if (newdecay >= cloud.decay)
            newdecay = cloud.decay - 1;

        _place_new_cloud( cloud.type, *ai, newdecay, cloud.whose, cloud.killer,
                          cloud.spread_rate, cloud.colour, cloud.name, cloud.tile );

        extra_decay += 8;
    }

    return (extra_decay);
}

static void _spread_fire(const cloud_struct &cloud)
{
    int make_flames = one_chance_in(5);

    for ( adjacent_iterator ai(cloud.pos); ai; ++ai )
    {
        if (!in_bounds(*ai)
            || env.cgrid(*ai) != EMPTY_CLOUD
            || is_sanctuary(*ai))
            continue;

        // burning trees produce flames all around
        if (!cell_is_solid(*ai) && make_flames)
        {
            _place_new_cloud( CLOUD_FIRE, *ai, cloud.decay/2+1, cloud.whose,
                              cloud.killer, cloud.spread_rate, cloud.colour,
                              cloud.name, cloud.tile );
        }

        // forest fire doesn't spread in all directions at once,
        // every neighbouring square gets a separate roll
        if (grd(*ai) == DNGN_TREE && one_chance_in(20))
        {
            if (env.markers.property_at(*ai, MAT_ANY, "veto_fire") == "veto")
                continue;

            if (you.see_cell(*ai))
                mpr("The forest fire spreads!");
            grd(*ai) = dgn_tree_base_feature_at(*ai);
            set_terrain_changed(*ai);
            _place_new_cloud( cloud.type, *ai, random2(30)+25, cloud.whose,
                              cloud.killer, cloud.spread_rate, cloud.colour,
                              cloud.name, cloud.tile );
            if (cloud.whose == KC_YOU)
                did_god_conduct(DID_KILL_PLANT, 1);
            else if (cloud.whose == KC_FRIENDLY)
                did_god_conduct(DID_PLANT_KILLED_BY_SERVANT, 1);
        }

    }
}

static void _cloud_fire_interacts_with_terrain(const cloud_struct &cloud)
{
    for (adjacent_iterator ai(cloud.pos); ai; ++ai)
    {
        const coord_def p(*ai);
        if (feat_is_watery(grd(p))
            && env.cgrid(p) == EMPTY_CLOUD
            && one_chance_in(5))
        {
            _place_new_cloud(CLOUD_STEAM, p, cloud.decay / 2 + 1,
                             cloud.whose, cloud.killer);
        }
    }
}

void cloud_interacts_with_terrain(const cloud_struct &cloud)
{
    if (cloud.type == CLOUD_FIRE || cloud.type == CLOUD_FOREST_FIRE)
        _cloud_fire_interacts_with_terrain(cloud);
}

static void _dissipate_cloud(int cloudidx, int dissipate)
{
    cloud_struct &cloud = env.cloud[cloudidx];
    // Apply calculated rate to the actual cloud.
    cloud.decay -= dissipate;

    if (cloud.type == CLOUD_FOREST_FIRE)
        _spread_fire(cloud);
    else if (x_chance_in_y(cloud.spread_rate, 100))
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
        // rain and cold clouds dissipate faster over lava.
        if (cloud.type == CLOUD_FIRE && grd(cloud.pos) == DNGN_DEEP_WATER)
            dissipate *= 4;
        else if ((cloud.type == CLOUD_COLD || cloud.type == CLOUD_RAIN)
                 && grd(cloud.pos) == DNGN_LAVA)
            dissipate *= 4;
        // Ink cloud doesn't appear outside of water.
        else if (cloud.type == CLOUD_INK && !feat_is_watery(grd(cloud.pos)))
            dissipate *= 40;
        else if (cloud.type == CLOUD_GLOOM)
        {
            int count = 0;
            for (adjacent_iterator ai(cloud.pos); ai; ++ai)
                if (env.cgrid(*ai) != EMPTY_CLOUD)
                    if (env.cloud[env.cgrid(*ai)].type == CLOUD_GLOOM)
                        count++;

            if (haloed(cloud.pos) && !silenced(cloud.pos))
                count = 0;

            if (count < 4)
                dissipate *= 50;
            else
                dissipate /= 20;
        }

        cloud_interacts_with_terrain(cloud);
        expose_items_to_element(cloud2beam(cloud.type), cloud.pos, 2);

        _dissipate_cloud(i, dissipate);
    }
}

static void _maybe_leave_water(const cloud_struct& c)
{
    // Rain clouds can occasionally leave shallow water or deepen it:
    // If we're near lava, chance of leaving water is lower;
    // if we're near deep water already, chance of leaving water
    // is slightly higher.
    if (one_chance_in((5 + count_neighbours(c.pos, DNGN_LAVA)) -
                           count_neighbours(c.pos, DNGN_DEEP_WATER)))
    {
        dungeon_feature_type feat;

       if (grd(c.pos) == DNGN_FLOOR)
           feat = DNGN_SHALLOW_WATER;
       else if (grd(c.pos) == DNGN_SHALLOW_WATER && you.pos() != c.pos
                && one_chance_in(3))
           // Don't drown the player!
           feat = DNGN_DEEP_WATER;
       else
           feat = grd(c.pos);

        if (grd(c.pos) != feat)
        {
            if (you.pos() == c.pos)
                mpr("The rain has left you waist-deep in water!");
            dungeon_terrain_changed(c.pos, feat);
        }
    }
}

void delete_cloud_at(coord_def p)
{
    const int cloudno = env.cgrid(p);
    if (cloudno != EMPTY_CLOUD)
        delete_cloud(cloudno);
}

void delete_cloud( int cloud )
{
    cloud_struct& c = env.cloud[cloud];
    if (c.type != CLOUD_NONE)
    {
        cloud_type t = c.type;
        if (c.type == CLOUD_RAIN)
            _maybe_leave_water(c);

        c.type        = CLOUD_NONE;
        c.decay       = 0;
        c.whose       = KC_OTHER;
        c.killer      = KILL_NONE;
        c.spread_rate = 0;
        c.colour      = -1;
        c.name        = "";
        c.tile        = "";

        env.cgrid(c.pos) = EMPTY_CLOUD;
        _los_cloud_changed(c.pos, t);
        c.pos.reset();
        env.cloud_no--;
    }
}

void move_cloud_to(coord_def src, coord_def dst)
{
    const int cloudno = env.cgrid(src);
    move_cloud(cloudno, dst);
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
        _los_cloud_changed(oldpos, env.cloud[cloud].type);
        _los_cloud_changed(newpos, env.cloud[cloud].type);
    }
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud( cloud_type cl_type, const coord_def& p, int lifetime,
                        kill_category whose, int spread_rate, int colour,
                        std::string name, std::string tile)
{
    check_place_cloud(cl_type, p, lifetime, whose,
                      cloud_struct::whose_to_killer(whose), spread_rate, colour,
                      name, tile);
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud( cloud_type cl_type, const coord_def& p, int lifetime,
                        killer_type killer, int spread_rate, int colour,
                        std::string name, std::string tile)
{
    check_place_cloud(cl_type, p, lifetime,
                      cloud_struct::killer_to_whose(killer), killer,
                      spread_rate, colour, name, tile);
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud( cloud_type cl_type, const coord_def& p, int lifetime,
                        kill_category whose, killer_type killer,
                        int spread_rate, int colour, std::string name,
                        std::string tile)
{
    if (!in_bounds(p) || env.cgrid(p) != EMPTY_CLOUD)
        return;

    if (cl_type == CLOUD_INK && !feat_is_watery(grd(p)))
        return;

    place_cloud( cl_type, p, lifetime, whose, killer, spread_rate, colour,
                 name, tile );
}

int steam_cloud_damage(const cloud_struct &cloud)
{
    return steam_cloud_damage(cloud.decay);
}

int steam_cloud_damage(int decay)
{
    decay = std::min(decay, 60);
    decay = std::max(decay, 10);

    // Damage in range 3 - 16.
    return ((decay * 13 + 20) / 50);
}

//   Places a cloud with the given stats. May delete old clouds to
//   make way if there are too many on level. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(cloud_type cl_type, const coord_def& ctarget, int cl_range,
                 kill_category whose, int _spread_rate, int colour,
                 std::string name, std::string tile)
{
    place_cloud(cl_type, ctarget, cl_range, whose,
                cloud_struct::whose_to_killer(whose), _spread_rate,
                colour, name, tile);
}

//   Places a cloud with the given stats. May delete old clouds to
//   make way if there are too many on level. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(cloud_type cl_type, const coord_def& ctarget, int cl_range,
                 killer_type killer, int _spread_rate, int colour,
                 std::string name, std::string tile)
{
    place_cloud(cl_type, ctarget, cl_range,
                cloud_struct::killer_to_whose(killer), killer, _spread_rate,
                colour, name, tile);
}

bool cloud_is_inferior(cloud_type inf, cloud_type superior)
{
    return (inf == CLOUD_STINK && superior == CLOUD_POISON);
}

//   Places a cloud with the given stats. May delete old clouds to
//   make way if there are too many on level. Will overwrite an old
//   cloud under some circumstances.
void place_cloud(cloud_type cl_type, const coord_def& ctarget, int cl_range,
                 kill_category whose, killer_type killer, int _spread_rate,
                 int colour, std::string name, std::string tile)
{
    if (is_sanctuary(ctarget) && !is_harmless_cloud(cl_type))
        return;

    if (cl_type == CLOUD_INK && !feat_is_watery(grd(ctarget)))
        return;

    int cl_new = -1;

    const int target_cgrid = env.cgrid(ctarget);
    if (target_cgrid != EMPTY_CLOUD)
    {
        // There's already a cloud here. See if we can overwrite it.
        cloud_struct& old_cloud = env.cloud[target_cgrid];
        if (old_cloud.type >= CLOUD_GREY_SMOKE && old_cloud.type <= CLOUD_STEAM
            || cloud_is_inferior(old_cloud.type, cl_type)
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

    const int spread_rate = actual_spread_rate(cl_type, _spread_rate);

    // Too many clouds.
    if (env.cloud_no >= MAX_CLOUDS)
    {
        // Default to random in case there's no low quality clouds.
        int cl_del = random2(MAX_CLOUDS);

        for (int ci = 0; ci < MAX_CLOUDS; ci++)
        {
            cloud_struct& cloud = env.cloud[ci];
            if (cloud.type >= CLOUD_GREY_SMOKE && cloud.type <= CLOUD_STEAM
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
                    whose, killer, spread_rate, colour, name, tile );
    }
    else
    {
        // Find slot for cloud.
        for (int ci = 0; ci < MAX_CLOUDS; ci++)
        {
            if (env.cloud[ci].type == CLOUD_NONE)   // ie is empty
            {
                _new_cloud( ci, cl_type, ctarget, cl_range * 10,
                            whose, killer, spread_rate, colour, name, tile );
                break;
            }
        }
    }
}

static bool _is_opaque_cloud(cloud_type ctype)
{
    return (ctype >= CLOUD_OPAQUE_FIRST && ctype <= CLOUD_OPAQUE_LAST);
}

bool is_opaque_cloud(int cloud_idx)
{
    if (cloud_idx == EMPTY_CLOUD)
        return (false);

    const int ctype = env.cloud[cloud_idx].type;
    return (ctype >= CLOUD_OPAQUE_FIRST && ctype <= CLOUD_OPAQUE_LAST);
}

cloud_type cloud_type_at(const coord_def &c)
{
    const int cloudno = env.cgrid(c);
    return (cloudno == EMPTY_CLOUD ? CLOUD_NONE
                                   : env.cloud[cloudno].type);
}

cloud_type random_smoke_type()
{
    // including black to keep variety
    switch ( random2(4) )
    {
    case 0: return CLOUD_GREY_SMOKE;
    case 1: return CLOUD_BLUE_SMOKE;
    case 2: return CLOUD_BLACK_SMOKE;
    case 3: return CLOUD_PURPLE_SMOKE;
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
    case BEAM_POTION_PURPLE_SMOKE:
        return CLOUD_PURPLE_SMOKE;
    case BEAM_STEAM:
    case BEAM_POTION_STEAM:
        return CLOUD_STEAM;
    case BEAM_MIASMA:
    case BEAM_POTION_MIASMA:
        return CLOUD_MIASMA;
    case BEAM_CHAOS:
        return CLOUD_CHAOS;
    case BEAM_POTION_RAIN:
        return CLOUD_RAIN;
    case BEAM_POTION_MUTAGENIC:
        return CLOUD_MUTAGENIC;
    case BEAM_GLOOM:
        return CLOUD_GLOOM;
    case BEAM_RANDOM:
        return CLOUD_RANDOM;
    case BEAM_INK:
        return CLOUD_INK;
    case BEAM_HOLY_FLAME:
        return CLOUD_HOLY_FLAMES;
    }
}

beam_type cloud2beam(cloud_type flavour)
{
    switch (flavour)
    {
    default:
    case CLOUD_NONE:         return BEAM_NONE;
    case CLOUD_FIRE:         return BEAM_FIRE;
    case CLOUD_FOREST_FIRE:  return BEAM_FIRE;
    case CLOUD_STINK:        return BEAM_POTION_STINKING_CLOUD;
    case CLOUD_COLD:         return BEAM_COLD;
    case CLOUD_POISON:       return BEAM_POISON;
    case CLOUD_BLACK_SMOKE:  return BEAM_POTION_BLACK_SMOKE;
    case CLOUD_GREY_SMOKE:   return BEAM_POTION_GREY_SMOKE;
    case CLOUD_BLUE_SMOKE:   return BEAM_POTION_BLUE_SMOKE;
    case CLOUD_PURPLE_SMOKE: return BEAM_POTION_PURPLE_SMOKE;
    case CLOUD_STEAM:        return BEAM_STEAM;
    case CLOUD_MIASMA:       return BEAM_MIASMA;
    case CLOUD_CHAOS:        return BEAM_CHAOS;
    case CLOUD_RAIN:         return BEAM_POTION_RAIN;
    case CLOUD_MUTAGENIC:    return BEAM_POTION_MUTAGENIC;
    case CLOUD_GLOOM:        return BEAM_GLOOM;
    case CLOUD_INK:          return BEAM_INK;
    case CLOUD_HOLY_FLAMES:  return BEAM_HOLY_FLAME;
    case CLOUD_RANDOM:       return BEAM_RANDOM;
    }
}

// Returns by how much damage gets divided due to elemental resistances.
// Damage is reduced to, level 1 -> 1/2, level 2 -> 1/3, level 3 -> 1/5, or
// for "boolean" attacks (which use bonus_res = 1, sticky flame/electricity)
// to level 1 -> 1/3, level 2 -> 1/4, or level 3 -> 1/6.
// With the old formula (1 + resist * resist) this used to be
// 1/2, 1/5, 1/10 (normal) and 1/3, 1/6, 1/11 (boolean), respectively.
int resist_fraction(int resist, int bonus_res)
{
    return ((3*resist + 1)/2 + bonus_res);
}

// NOTE: Keep in sync with in_a_cloud()
int max_cloud_damage(cloud_type cl_type, int power)
{
    int speed  = player_speed();
    int dam    = 0;
    int resist = 0;

    switch (cl_type)
    {
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (you.duration[DUR_FIRE_SHIELD])
            return (0);
        resist = player_res_fire();

        // Intentional fall-through
    case CLOUD_HOLY_FLAMES:
        if (resist == 0)
        {
            if (you.is_evil() || you.is_unholy())
                resist = -1;
            else if (you.is_holy())
                resist = 3;
        }

        // Intentional fall through
    case CLOUD_COLD:
        if (you.mutation[MUT_PASSIVE_FREEZE])
            return (0);
        if (cl_type == CLOUD_COLD)
            resist = player_res_cold();

        if (resist <= 0)
        {
            dam += 32 * speed / 10;

            if (resist < 0)
                dam += 16 * speed / 10;
        }
        else
        {
            dam += 32 * speed / 10;
            dam /= resist_fraction(resist);
        }
        break;

    case CLOUD_STINK:
        if (player_res_poison() || you.species == SP_GREY_DRACONIAN)
            return (0);

        dam += 2 * speed / 10;
        break;

    case CLOUD_POISON:
        if (player_res_poison())
            return (0);

        dam += 9 * speed / 10;
        break;

    case CLOUD_STEAM:
    {
        ASSERT(power >= 0);

        if (player_res_steam() > 0 || power == 0)
            return (0);

        const int base_dam = steam_cloud_damage(power * 10);
        dam += (base_dam - 1) * speed / 10;

        const int res_fire = player_res_fire();
        if (res_fire < 0)
            dam += base_dam / 2 * speed / 10;
        else if (res_fire)
            dam /= 1 + (res_fire / 2);

        break;
    }

    case CLOUD_MIASMA:
        if (you.res_rotting())
            return (0);

        dam += 11 * speed / 10;
        break;

    default:
        break;
    }

    if (dam < 0)
        dam = 0;

    return (dam);
}

// NOTE: Keep in sync with max_cloud_damage()
void in_a_cloud()
{
    int cl = env.cgrid(you.pos());
    int hurted = 0;
    int resist = 0;
    std::string name = env.cloud[cl].name;

    const cloud_struct &cloud(env.cloud[cl]);

    switch (cloud.type)
    {
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (you.duration[DUR_FIRE_SHIELD])
            return;

        cloud.announce_actor_engulfed(&you);
        resist = player_res_fire();

        if (resist <= 0)
        {
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;

            if (resist < 0)
                hurted += ((random2avg(14, 2) + 3) * you.time_taken) / 10;

            hurted -= random2(you.armour_class());

            if (hurted < 0)
                hurted = 0;
            else
                ouch(hurted, cl, KILLED_BY_CLOUD, "flame");
        }
        else
        {
            canned_msg(MSG_YOU_RESIST);
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;
            hurted /= resist_fraction(resist);
            ouch(hurted, cl, KILLED_BY_CLOUD, "flame");
        }
        expose_player_to_element(BEAM_FIRE, 7);
        break;

    case CLOUD_STINK:
        cloud.announce_actor_engulfed(&you);

        if (player_res_poison() || you.species == SP_GREY_DRACONIAN)
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
            // effectively one or two turns, since it will be
            // decremented right away
            confuse_player( (coinflip() ? 3 : 2) );
        }
        break;

    case CLOUD_COLD:
        if (you.mutation[MUT_PASSIVE_FREEZE])
            break;

        cloud.announce_actor_engulfed(&you);
        resist = player_res_cold();

        if (resist <= 0)
        {
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;

            if (resist < 0)
                hurted += ((random2avg(14, 2) + 3) * you.time_taken) / 10;

            hurted -= random2(you.armour_class());
            if (hurted < 0)
                hurted = 0;

            ouch(hurted, cl, KILLED_BY_CLOUD, "freezing vapour");
        }
        else
        {
            canned_msg(MSG_YOU_RESIST);
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;
            hurted /= resist_fraction(resist);
            ouch(hurted, cl, KILLED_BY_CLOUD, "freezing vapour");
        }
        expose_player_to_element(BEAM_COLD, 7);
        break;

    case CLOUD_POISON:
        // If you don't have to breathe, unaffected
        cloud.announce_actor_engulfed(&you);
        if (!player_res_poison())
        {
            ouch((random2(10) * you.time_taken) / 10, cl, KILLED_BY_CLOUD,
                 "poison gas");
            // We don't track the source of the cloud so we can't assign blame.
            poison_player(1, "", cloud.cloud_name("poison gas"));
        }
        break;

    case CLOUD_GREY_SMOKE:
    case CLOUD_BLUE_SMOKE:
    case CLOUD_TLOC_ENERGY:
    case CLOUD_PURPLE_SMOKE:
    case CLOUD_BLACK_SMOKE:
        cloud.announce_actor_engulfed(&you);
        break;

    case CLOUD_STEAM:
    {
        cloud.announce_actor_engulfed(&you);
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

        ouch(hurted, cl, KILLED_BY_CLOUD, "steam");
        break;
    }

    case CLOUD_MIASMA:
        cloud.announce_actor_engulfed(&you);
        if (you.res_rotting())
            return;

        // We'd want to blame it to a specific monster...
        miasma_player(cloud.cloud_name("a cloud of dark miasma"));

        hurted += (random2avg(12, 3) * you.time_taken) / 10;    // 3

        if (hurted < 0)
            hurted = 0;

        ouch(hurted, cl, KILLED_BY_CLOUD, "foul pestilence");
        break;

    case CLOUD_RAIN:
        if (you.duration[DUR_FIRE_SHIELD])
            you.duration[DUR_FIRE_SHIELD] = 1;

        if (you.misled())
        {
            mpr("The rain washes away illusions!", MSGCH_DURATION);
            you.duration[DUR_MISLED] = 0;
        }

        {
            const std::string rainname = cloud.cloud_name("the rain");
            if (rainname == "the rain")
                mpr("You are standing in the rain.");
            else
                cloud.announce_actor_engulfed(&you);
        }
        break;

    case CLOUD_MUTAGENIC:
        cloud.announce_actor_engulfed(&you);
        if (coinflip())
        {
            mpr("Strange energies course through your body.");
            if (one_chance_in(3))
                you.mutate();
            else
                give_bad_mutation();
        }
        break;

    case CLOUD_GLOOM:
        cloud.announce_actor_engulfed(&you);
        break;

    case CLOUD_CHAOS:
        if (coinflip())
        {
            cloud.announce_actor_engulfed(&you);
            chaos_affect_actor(&you);
        }
        break;

    case CLOUD_HOLY_FLAMES:
        cloud.announce_actor_engulfed(&you);

        // Stats are the same for fire, except resists are based on holiness.
        // Damage is reduced if you are holy, increased if you are evil/unholy.
        if (you.is_evil() || you.is_unholy())
            resist = -1;
        else if (you.is_holy())
            resist = 3;

        if (resist <= 0)
        {
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;

            if (resist < 0)
                hurted += ((random2avg(14, 2) + 3) * you.time_taken) / 10;

            hurted -= random2(you.armour_class());

            if (hurted < 0)
                hurted = 0;
            else
                ouch(hurted, cl, KILLED_BY_CLOUD, "blessed fire");
        }
        else
        {
            canned_msg(MSG_YOU_RESIST);
            hurted += ((random2avg(23, 3) + 10) * you.time_taken) / 10;
            hurted /= resist_fraction(resist);
            ouch(hurted, cl, KILLED_BY_CLOUD, "blessed fire");
        }
        break;

    default:
        break;
    }
}

bool is_damaging_cloud(cloud_type type, bool temp)
{
    switch (type)
    {
    // always harmful...
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        // ... unless a Ring of Flames is up and it's a fire cloud.
        if (temp && you.duration[DUR_FIRE_SHIELD])
            return (false);
    case CLOUD_CHAOS:
        return (true);
    case CLOUD_COLD:
        return (!you.mutation[MUT_PASSIVE_FREEZE]);

    // Only harmful if the player doesn't have the necessary resistances.
    // Takes into account what the player can *know* and what s/he can
    // also expect to be the case a few turns later (ignores spells).
    case CLOUD_STINK:
         if (you.species == SP_GREY_DRACONIAN)
             return (false);
    case CLOUD_POISON:
        return !(player_res_poison(false, temp));
    case CLOUD_STEAM:
        return (player_res_steam(false, temp) <= 0);
    case CLOUD_MIASMA:
        return (!you.res_rotting());
    case CLOUD_MUTAGENIC:
        return (you.can_mutate());
    case CLOUD_HOLY_FLAMES:
        if (you.is_holy())
            return (false);

        return (true);
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
    case CLOUD_PURPLE_SMOKE:
    case CLOUD_TLOC_ENERGY:
    case CLOUD_MIST:
    case CLOUD_RAIN:
    case CLOUD_MAGIC_TRAIL:
    case CLOUD_GLOOM:
    case CLOUD_INK:
    case CLOUD_DEBUGGING:
        return (true);
    default:
        return (false);
    }
}

bool in_what_cloud(cloud_type type)
{
    int cl = env.cgrid(you.pos());

    if (env.cgrid(you.pos()) == EMPTY_CLOUD)
        return (false);

    if (env.cloud[cl].type == type)
        return (true);

    return (false);
}

cloud_type in_what_cloud()
{
    int cl = env.cgrid(you.pos());

    if (env.cgrid(you.pos()) == EMPTY_CLOUD)
        return (CLOUD_NONE);

    return (env.cloud[cl].type);
}

std::string cloud_name_at_index(int cloudno)
{
    if (!env.cloud[cloudno].name.empty())
        return (env.cloud[cloudno].name);
    else
        return cloud_type_name(env.cloud[cloudno].type);
}

// [ds] XXX: Some of these aren't so terse and some of the verbose
// names aren't very verbose. Be warned that names in
// _terse_cloud_names may be referenced by fog machines.
static const char *_terse_cloud_names[] =
{
    "?",
    "flame", "noxious fumes", "freezing vapour", "poison gasses",
    "black smoke", "grey smoke", "blue smoke",
    "purple smoke", "translocational energy", "fire",
    "steam", "gloom", "ink", "blessed fire", "foul pestilence", "thin mist",
    "seething chaos", "rain", "mutagenic fog", "magical condensation",
};

static const char *_verbose_cloud_names[] =
{
    "?",
    "roaring flames", "noxious fumes", "freezing vapours", "poison gas",
    "black smoke", "grey smoke", "blue smoke",
    "purple smoke", "translocational energy", "roaring flames",
    "a cloud of scalding steam", "a thick gloom", "ink", "blessed fire",
    "a dark miasma", "thin mist", "seething chaos", "the rain",
    "a mutagenic fog", "magical condensation",
};

std::string cloud_type_name(cloud_type type, bool terse)
{
    COMPILE_CHECK(ARRAYSZ(_terse_cloud_names) == NUM_CLOUD_TYPES,
                  check_terse_cloud_names);
    COMPILE_CHECK(ARRAYSZ(_verbose_cloud_names) == NUM_CLOUD_TYPES,
                  check_verbose_cloud_names);

    return (type <= CLOUD_NONE || type >= NUM_CLOUD_TYPES
            ? "buggy goodness"
            : (terse? _terse_cloud_names : _verbose_cloud_names)[type]);
}

////////////////////////////////////////////////////////////////////////
// cloud_struct

kill_category cloud_struct::killer_to_whose(killer_type _killer)
{
    switch (_killer)
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

killer_type cloud_struct::whose_to_killer(kill_category _whose)
{
    switch (_whose)
    {
        case KC_YOU:         return(KILL_YOU_MISSILE);
        case KC_FRIENDLY:    return(KILL_MON_MISSILE);
        case KC_OTHER:       return(KILL_MISC);
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

std::string cloud_struct::cloud_name(const std::string &defname) const
{
    return (!name.empty()    ? name :
            !defname.empty() ? defname :
                               cloud_type_name(type, false));
}

void cloud_struct::announce_actor_engulfed(const actor *act,
                                           bool beneficial) const
{
    if (you.can_see(act))
    {
        mprf("%s %s in %s.",
             act->name(DESC_CAP_THE).c_str(),
             beneficial ? act->conj_verb("bask").c_str()
                        : (act->conj_verb("are") + " engulfed").c_str(),
             cloud_name().c_str());
    }
}

int get_cloud_colour(int cloudno)
{
    int which_colour = LIGHTGREY;
    if (env.cloud[cloudno].colour != -1)
        return (env.cloud[cloudno].colour);

    switch (env.cloud[cloudno].type)
    {
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (env.cloud[cloudno].decay <= 20)
            which_colour = RED;
        else if (env.cloud[cloudno].decay <= 40)
            which_colour = LIGHTRED;
        else if (one_chance_in(4))
            which_colour = RED;
        else if (one_chance_in(4))
            which_colour = LIGHTRED;
        else
            which_colour = YELLOW;
        break;

    case CLOUD_STINK:
        which_colour = GREEN;
        break;

    case CLOUD_COLD:
        if (env.cloud[cloudno].decay <= 20)
            which_colour = BLUE;
        else if (env.cloud[cloudno].decay <= 40)
            which_colour = LIGHTBLUE;
        else if (one_chance_in(4))
            which_colour = BLUE;
        else if (one_chance_in(4))
            which_colour = LIGHTBLUE;
        else
            which_colour = WHITE;
        break;

    case CLOUD_POISON:
        which_colour = (one_chance_in(3) ? LIGHTGREEN : GREEN);
        break;

    case CLOUD_BLUE_SMOKE:
        which_colour = LIGHTBLUE;
        break;

    case CLOUD_PURPLE_SMOKE:
    case CLOUD_TLOC_ENERGY:
    case CLOUD_GLOOM:
        which_colour = MAGENTA;
        break;

    case CLOUD_MIASMA:
    case CLOUD_BLACK_SMOKE:
    case CLOUD_INK:
        which_colour = DARKGREY;
        break;

    case CLOUD_RAIN:
    case CLOUD_MIST:
        which_colour = ETC_MIST;
        break;

    case CLOUD_CHAOS:
        which_colour = ETC_RANDOM;
        break;

    case CLOUD_MUTAGENIC:
        which_colour = ETC_MUTAGENIC;
        break;

    case CLOUD_MAGIC_TRAIL:
        which_colour = ETC_MAGIC;
        break;

    case CLOUD_HOLY_FLAMES:
        which_colour = ETC_HOLY;
        break;

    default:
        which_colour = LIGHTGREY;
        break;
    }
    return (which_colour);
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
        snprintf(buf, sizeof(buf), "lua_mapless:fog_machine_%s(\"%s\", %d, %d)",
                fog_types[fm_type], cloud_type_name(cl_type).c_str(),
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
