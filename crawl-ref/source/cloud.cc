/**
 * @file
 * @brief Functions related to clouds.
 *
 * Creating a cloud module so all the cloud stuff can be isolated.
**/

#include "AppHdr.h"

#include "cloud.h"

#include <algorithm>

#include "areas.h"
#include "art-enum.h"
#include "colour.h"
#include "coordit.h"
#include "dungeon.h"
#include "english.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "level-state-type.h"
#include "libutil.h" // testbits
#include "los.h"
#include "mapmark.h"
#include "map-knowledge.h"
#include "melee-attack.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-place.h"
#include "nearby-danger.h" // Compass (for random_walk, CloudGenerator)
#include "player-stats.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h" // explode_blastmotes_at
#include "spl-damage.h" // dazzle_target
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "rltiles/tiledef-main.h"
#include "traps.h"
#include "unwind.h"
#include "xom.h"

cloud_struct* cloud_at(coord_def pos)
{
    return map_find(env.cloud, pos);
}

/// damage = base + random2avg(random, random/15 + 1)
struct cloud_damage
{
    int base; ///< Flat damage on every hit, pre-defenses.
    int random; ///< Damage rolled on hit.
    bool extra_player_dam; //< HACK: does 4+random2(8) extra damage to players.
    // Yes, we really hate players, damn their guts.
};

/// Damage for most damaging clouds.
static const cloud_damage NORMAL_CLOUD_DAM = { 6, 16, true };
// 6+r2a(16,2) for monsters, 10+r2a(23,2) for players

/// A portrait of a cloud_type.
struct cloud_data
{
    /// A (relatively) short name for the cloud. May be referenced from lua.
    const char* terse_name;
    /// Another name for the cloud. If nullptr, defaults to terse name.
    const char* verbose_name;
    /// The colour of the cloud in console.
    colour_t colour;
    /// Info for calculating cloud tiles.
    cloud_tile_info tile_info;
    /// The associated "beam" (effect) for this cloud type.
    beam_type beam_effect;
    /// How much damage the cloud does before defenses & resists.
    cloud_damage damage;
    /// Do multiple squares of this cloud block LOS?
    bool opaque;
};

/// A map from cloud_type to cloud_data.
static const cloud_data clouds[] = {
    // CLOUD_NONE,
    { "?", "?",                                 // terse, verbose name
    },
    // CLOUD_FIRE,
    { "flame", "blazing flames",                // terse, verbose name
       COLOUR_UNDEF,                            // colour
       { TILE_CLOUD_FIRE, CTVARY_DUR },         // tile
       BEAM_FIRE,                               // beam_effect
       NORMAL_CLOUD_DAM,                        // base, random damage
    },
    // CLOUD_MEPHITIC,
    { "noxious fumes", nullptr,                 // terse, verbose name
      GREEN,                                    // colour
      { TILE_CLOUD_MEPHITIC, CTVARY_DUR },      // tile
      BEAM_MEPHITIC,                            // beam_effect
    },
    // CLOUD_COLD,
    { "freezing vapour", "freezing vapours",    // terse, verbose name
      COLOUR_UNDEF,                             // colour
      { TILE_CLOUD_COLD, CTVARY_DUR },          // tile
      BEAM_COLD,                                // beam_effect
      NORMAL_CLOUD_DAM,                         // base, random damage
    },
    // CLOUD_POISON,
    { "poison gas", nullptr,                    // terse, verbose name
      LIGHTGREEN,                               // colour
      { TILE_CLOUD_POISON, CTVARY_DUR },        // tile
      BEAM_POISON,                              // beam_effect
      {0, 10},                                  // base, random damage
    },
    // CLOUD_BLACK_SMOKE,
    { "black smoke",  nullptr,                  // terse, verbose name
      DARKGREY,                                 // colour
      { TILE_CLOUD_BLACK_SMOKE, CTVARY_NONE },  // tile
      BEAM_NONE, {},                            // beam & damage
      true,                                     // opacity
    },
    // CLOUD_GREY_SMOKE,
    { "grey smoke",  nullptr,                   // terse, verbose name
      LIGHTGREY,                                // colour
      { TILE_CLOUD_GREY_SMOKE, CTVARY_NONE },   // tile
      BEAM_NONE, {},                            // beam & damage
      true,                                     // opacity
    },
    // CLOUD_BLUE_SMOKE,
    { "blue smoke",  nullptr,                   // terse, verbose name
      LIGHTBLUE,                                // colour
      { TILE_CLOUD_BLUE_SMOKE, CTVARY_NONE },   // tile
      BEAM_NONE, {},                            // beam & damage
      true,                                     // opacity
    },
    // CLOUD_PURPLE_SMOKE,
    { "purple smoke",  nullptr,                 // terse, verbose name
      MAGENTA,                                  // colour
      { TILE_CLOUD_TLOC_ENERGY, CTVARY_NONE },  // tile
      BEAM_NONE, {},                            // beam & damage
      true,                                     // opacity
    },
    // CLOUD_TLOC_ENERGY,
    { "translocational energy",  nullptr,       // terse, verbose name
      MAGENTA,                                  // colour
      { TILE_CLOUD_TLOC_ENERGY, CTVARY_NONE },  // tile
      BEAM_NONE, {},                            // beam & damage
      true,                                     // opacity
    },
    // CLOUD_FOREST_FIRE,
    { "spreading flames", "a forest fire",      // terse, verbose name
      COLOUR_UNDEF,                             // colour
      { TILE_CLOUD_FOREST_FIRE },               // tile
      BEAM_FIRE,                                // beam_effect
      NORMAL_CLOUD_DAM,                         // base, random damage
    },
    // CLOUD_STEAM,
    { "steam", "a cloud of scalding steam",     // terse, verbose name
      LIGHTGREY,                                // colour
      { TILE_CLOUD_GREY_SMOKE, CTVARY_NONE },   // tile
      BEAM_STEAM,                               // beam_effect
      {0, 16},                                  // base, random damage
      true,                                     // opacity
    },
#if TAG_MAJOR_VERSION == 34
    // CLOUD_GLOOM,
    { "gloom", "thick gloom",                   // terse, verbose name
      MAGENTA,                                  // colour
      { TILE_CLOUD_GLOOM },                     // tile
    },
#endif
    // CLOUD_INK,
    { "ink",  nullptr,                          // terse, verbose name
      DARKGREY,                                 // colour
      { TILE_CLOUD_INK },                       // tile
      BEAM_NONE, {},                            // beam_effect & damage
      true,                                     // opacity
    },
    // CLOUD_PETRIFY,
    { "calcifying dust",  nullptr,              // terse, verbose name
      WHITE,                                    // colour
      { TILE_CLOUD_PETRIFY, CTVARY_DUR },       // tile
      BEAM_PETRIFYING_CLOUD, {},                // beam_effect & damage
      true,                                     // opacity
    },
    // CLOUD_HOLY,
    { "blessed fire", nullptr,                  // terse, verbose name
      ETC_HOLY,                                 // colour
      { TILE_CLOUD_YELLOW_SMOKE },              // tile
      BEAM_HOLY,                                // beam_effect
      {4, 12, true},                            // base, random damage
      true,                                     // opacity
    },
    // CLOUD_MIASMA,
    { "foul pestilence", "dark miasma",         // terse, verbose name
      DARKGREY,                                 // colour
      { TILE_CLOUD_MIASMA, CTVARY_DUR },        // tile
      BEAM_MIASMA,                              // beam_effect
      { 0, 12 },                                // base, random damage
    },
    // CLOUD_MIST,
    { "thin mist", nullptr,                     // terse, verbose name
      ETC_MIST,                                 // colour
      { TILE_CLOUD_MIST, CTVARY_NONE },         // tile
    },
    // CLOUD_CHAOS,
    { "seething chaos", nullptr,                // terse, verbose name
      ETC_RANDOM,                               // colour
      { TILE_CLOUD_CHAOS, CTVARY_RANDOM },      // tile
      BEAM_CHAOS,                               // beam_effect
    },
    // CLOUD_RAIN,
    { "rain", "the rain",                       // terse, verbose name
      ETC_MIST,                                 // colour
      { TILE_CLOUD_RAIN, CTVARY_RANDOM },       // tile
      BEAM_NONE,                                // unused
      { 0, 9 },                                 // base, random damage
                                                // but only for fiery mons
    },
    // CLOUD_MUTAGENIC,
    { "mutagenic fog",  nullptr,                // terse, verbose name
      ETC_MUTAGENIC,                            // colour
      { TILE_ERROR, CTVARY_NONE },              // tile
    },
    // CLOUD_MAGIC_TRAIL,
    { "magical condensation", nullptr,          // terse, verbose name
      ETC_MAGIC,                                // colour
      { TILE_CLOUD_MAGIC_TRAIL, CTVARY_DUR },   // tile
    },
    // CLOUD_VORTEX,
    { "whirling frost", nullptr,                // terse, verbose name
      ETC_VORTEX,                               // colour
      { TILE_ERROR },                           // tile
    },
    // CLOUD_DUST,
    { "sparse dust",  nullptr,                  // terse, verbose name
      ETC_EARTH,                                // colour
      { TILE_CLOUD_DUST, CTVARY_DUR },          // tile
    },
    // CLOUD_SPECTRAL,
    { "spectral mist", nullptr,                 // terse, verbose name
      ETC_ELECTRICITY,                          // colour
      { TILE_CLOUD_SPECTRAL, CTVARY_DUR },      // tile
      BEAM_NONE,                                // beam_effect
      { 4, 15 },                                // base, random damage
    },
    // CLOUD_ACID,
    { "acidic fog", nullptr,                    // terse, verbose name
      YELLOW,                                   // colour
      { TILE_CLOUD_ACID, CTVARY_DUR },          // dur
      BEAM_ACID,                                // beam_effect
      { 8, 22, true },                          // base, random damage
    },
    // CLOUD_STORM,
    { "thunder", "a thunderstorm",              // terse, verbose name
      ETC_DARK,                                 // colour
      { TILE_CLOUD_STORM, CTVARY_RANDOM },      // tile
      BEAM_ELECTRICITY,                         // beam_effect
      { 23, 27 },
    },
    // CLOUD_MISERY,
    { "excruciating misery", nullptr,           // terse, verbose name
      ETC_INCARNADINE,                          // colour
      { TILE_CLOUD_MISERY, CTVARY_DUR },           // tile
    },
    // CLOUD_FLUFFY,
    { "white fluffiness",  nullptr,             // terse, verbose name
      WHITE,                                    // colour
      { TILE_CLOUD_WHITE_SMOKE, CTVARY_NONE },  // tile
      BEAM_NONE, {},                            // beam & damage
      true,                                     // opacity
    },
    // CLOUD_XOM_TRAIL,
    { "magical condensation", nullptr,          // terse, verbose name
      ETC_RANDOM,                               // colour
      { TILE_CLOUD_MAGIC_TRAIL, CTVARY_DUR },   // tile
      // TODO: another tile?
    },
    // CLOUD_SALT,
    { "salt",  nullptr,                           // terse, verbose name
      ETC_AIR,                                    // colour
      { TILE_CLOUD_WHITE_SMOKE, CTVARY_NONE },    // tile
      BEAM_NONE, {},                              // beam & damage
      true,                                       // opacity
    },
    // CLOUD_GOLD_DUST,
    { "golden dust",  nullptr,                    // terse, verbose name
      ETC_HOLY,                                   // colour
      { TILE_CLOUD_GOLD_DUST, CTVARY_DUR },       // tile
      BEAM_NONE, {},                              // beam & damage
      true,                                       // opacity
    },
#if TAG_MAJOR_VERSION == 34
    // CLOUD_EMBERS,
    { "smouldering embers", "embers",
        ETC_SMOKE,
        { TILE_CLOUD_BLACK_SMOKE, CTVARY_NONE },
    },
#endif
    // CLOUD_FLAME,
    { "wisps of flame", nullptr,                  // terse, verbose name
      ETC_FIRE,                                   // colour
      { TILE_CLOUD_FLAME, CTVARY_RANDOM },        // tile
    },
    // CLOUD_DEGENERATION,
    { "degeneration",  nullptr,                   // terse, verbose name
      ETC_DARK,                                   // colour
      { TILE_CLOUD_DEGENERATION, CTVARY_NONE },   // tile
      BEAM_NONE, {},                              // beam & damage
      false,                                      // opacity
    },
    // CLOUD_BLASTMOTES,
    { "blastmotes", "volatile sparks",           // terse, verbose name
        ETC_SMOKE,                                // colour
      { TILE_CLOUD_BLASTMOTES, CTVARY_RANDOM },  // tile
    },
    // CLOUD_ELECTRICITY,
    { "sparks", nullptr,         // terse, verbose name
      ETC_ELECTRICITY,                                   // colour
      { TILE_CLOUD_ELECTRICITY, CTVARY_RANDOM },        // tile
    },
    // CLOUD_FAINT_MIASMA,
    { "faint pestilence", nullptr,                // terse, verbose name
      DARKGREY,                                      // colour
      { TILE_CLOUD_FAINT_MIASMA, CTVARY_RANDOM },    // tile
    },
    // CLOUD_MAGNETISED_DUST,
    { "magnetised fragments", nullptr,                  // terse, verbose name
      ETC_ELECTRICITY,                                 // colour
      { TILE_CLOUD_MAGNETISED_DUST, CTVARY_RANDOM },   // tile
    },
};
COMPILE_CHECK(ARRAYSZ(clouds) == NUM_CLOUD_TYPES);

static int _actor_cloud_damage(const actor *act, const cloud_struct &cloud,
                               bool maximum_damage);

static int _actual_spread_rate(cloud_type type, int spread_rate)
{
    if (spread_rate >= 0)
        return spread_rate;

    switch (type)
    {
#if TAG_MAJOR_VERSION == 34
    case CLOUD_GLOOM:
        return 50;
#endif
    case CLOUD_STEAM:
    case CLOUD_GREY_SMOKE:
    case CLOUD_BLACK_SMOKE:
    case CLOUD_PURPLE_SMOKE:
    case CLOUD_BLUE_SMOKE:
    case CLOUD_FLUFFY:
        return 22;
    case CLOUD_RAIN:
    case CLOUD_INK:
        return 11;
    default:
        return 0;
    }
}

static beam_type _cloud2beam(cloud_type flavour)
{
    if (flavour == CLOUD_RANDOM)
        return BEAM_RANDOM;
    return clouds[flavour].beam_effect;
}

#ifdef ASSERTS
static bool _killer_whose_match(kill_category whose, killer_type killer)
{
    switch (whose)
    {
        case KC_YOU:
            return killer == KILL_YOU_MISSILE || killer == KILL_YOU_CONF;

        case KC_FRIENDLY:
            return killer == KILL_MON_MISSILE || killer == KILL_YOU_CONF
                   || killer == KILL_MON;

        case KC_OTHER:
            return killer == KILL_MON_MISSILE
                   || killer == KILL_NON_ACTOR || killer == KILL_MON;

        case KC_NCATEGORIES:
            die("kill category not matching killer type");
    }
    return false;
}
#endif

/*
 * The LOS may have changed based on cloud changes at position `p`.
 *
 * @param p   The position that may have changed.
 * @param t   The cloud type now there; CLOUD_NONE if there is no cloud there now.
 * @param old The cloud type that was there; CLOUD_NONE if the was none.
 */
static void _los_cloud_changed(const coord_def& p, const cloud_type t, const cloud_type old)
{
    if (is_opaque_cloud(t) || is_opaque_cloud(old))
        los_terrain_changed(p);
}

cloud_struct::cloud_struct(coord_def p, cloud_type c, int d, int spread,
                           kill_category kc, killer_type kt, mid_t src,
                           int excl)
    : pos(p), type(c), decay(d), spread_rate(spread), whose(kc), killer(kt),
      source(src), excl_rad(excl)
{
    ASSERT(_killer_whose_match(whose, killer));

    if (type == CLOUD_RANDOM_SMOKE)
        type = random_smoke_type();
}

static int _spread_cloud(const cloud_struct &cloud)
{
    const int spreadch = cloud.decay > 30 ? 80 :
                         cloud.decay > 20 ? 50 :
                                            30;
    int extra_decay = 0;
    for (adjacent_iterator ai(cloud.pos); ai; ++ai)
    {
        if (random2(100) >= spreadch)
            continue;

        if (!in_bounds(*ai)
            || cloud_at(*ai)
            || cell_is_solid(*ai)
            || is_sanctuary(*ai) && !is_harmless_cloud(cloud.type))
        {
            continue;
        }

        if (cloud.type == CLOUD_INK && !feat_is_water(env.grid(*ai)))
            continue;

        int newdecay = cloud.decay / 2 + 1;
        if (newdecay >= cloud.decay)
            newdecay = cloud.decay - 1;

        env.cloud[*ai] = cloud;
        env.cloud[*ai].pos = *ai;
        env.cloud[*ai].decay = newdecay;
        _los_cloud_changed(env.cloud[*ai].pos, env.cloud[*ai].type, CLOUD_NONE);

        extra_decay += 8;
    }

    return extra_decay;
}

static void _spread_fire(const cloud_struct &cloud)
{
    int make_flames = one_chance_in(5);

    for (adjacent_iterator ai(cloud.pos); ai; ++ai)
    {
        if (!in_bounds(*ai)
            || cloud_at(*ai)
            || is_sanctuary(*ai))
        {
            continue;
        }

        // burning trees produce flames all around
        if (!cell_is_solid(*ai) && make_flames)
        {
            env.cloud[*ai] = cloud;
            env.cloud[*ai].type = CLOUD_FIRE;
            env.cloud[*ai].pos = *ai;
            env.cloud[*ai].decay = cloud.decay / 2 + 1;
        }

        // forest fire doesn't spread in all directions at once,
        // every neighbouring square gets a separate roll
        if (!feat_is_flammable(env.grid(*ai)) || is_temp_terrain(*ai)
            || x_chance_in_y(19, 20))
        {
            continue;
        }

        if (env.markers.property_at(*ai, MAT_ANY, "veto_destroy") == "veto")
            continue;

        if (you.see_cell(*ai))
            mpr("The forest fire spreads!");
        destroy_wall(*ai);
        env.cloud[*ai] = cloud;
        env.cloud[*ai].pos = *ai;
        env.cloud[*ai].decay = random2(30) + 25;

    }
}

static void _cloud_interacts_with_terrain(const cloud_struct &cloud)
{
    if (cloud.type != CLOUD_FIRE && cloud.type != CLOUD_FOREST_FIRE)
        return;

    for (adjacent_iterator ai(cloud.pos); ai; ++ai)
    {
        const coord_def p(*ai);
        if (in_bounds(p)
            && feat_is_water(env.grid(p))
            && !cell_is_solid(p)
            && !cloud_at(p)
            && one_chance_in(14))
        {
            const cloud_type old = cloud_type_at(p);
            env.cloud[p] = cloud_struct(p, CLOUD_STEAM, 2 + random2(5),
                                        11, cloud.whose, cloud.killer,
                                        cloud.source, -1);
            _los_cloud_changed(p, env.cloud[p].type, old);
        }
    }
}

/**
 * How fast should a given cloud fade away this turn?
 *
 * @param cloud_idx     The cloud in question.
 * @return              The rate at which the cloud's "decay" should decrease
 *                      this turn.
 */
static int _cloud_dissipation_rate(const cloud_struct &cloud)
{
    int dissipate = you.time_taken;

    // Player-created non-opaque clouds vanish instantly when outside LOS.
    // (Opaque clouds don't to prevent cloud suicide.)
    if ((cloud.source == MID_PLAYER || cloud.source == MID_YOU_FAULTLESS)
        && !you.see_cell_no_trans(cloud.pos)
        && !is_opaque_cloud(cloud.type))
    {
        return cloud.decay;
    }

    // Ink cloud shouldn't appear outside of water.
    if (cloud.type == CLOUD_INK && !feat_is_water(env.grid(cloud.pos)))
        return cloud.decay;

    return dissipate;
}

static void _dissipate_cloud(cloud_struct& cloud)
{
    // Apply calculated rate to the actual cloud.
    cloud.decay -= _cloud_dissipation_rate(cloud);

    if (cloud.type == CLOUD_FOREST_FIRE)
        _spread_fire(cloud);
    else if (x_chance_in_y(cloud.spread_rate, 100))
    {
        cloud.spread_rate -= div_rand_round(cloud.spread_rate, 10);
        cloud.decay       -= _spread_cloud(cloud);
    }

    // Faint miasma becomes proper miasma after the first tick (regardless of
    // duration), but will not spawn beneath the player.
    // XXX: This is currently very player-centric, but the player is also the
    //      only possible source of this at present.
    if (cloud.type == CLOUD_FAINT_MIASMA)
    {
        if (cloud.decay > 1)
            cloud.decay = 1;
        else if (cloud.pos != you.pos())
        {
            cloud.decay = random_range(50, 90);
            cloud.type = CLOUD_MIASMA;
        }
        else
            delete_cloud(cloud.pos);
    }

    // Check for total dissipation and handle accordingly.
    if (cloud.decay < 1 && cloud.type != CLOUD_FAINT_MIASMA)
        delete_cloud(cloud.pos);
}

static void _handle_spectral_cloud(const cloud_struct& cloud)
{
    if (actor_at(cloud.pos) || !actor_by_mid(cloud.source))
        return;

    int countn = 0;
    for (distance_iterator di(cloud.pos, false, false, 2); di; ++di)
    {
        if (monster_at(*di) && monster_at(*di)->type == MONS_SPECTRAL_THING)
            countn++;
    }

    int rate[5] = {650, 175, 45, 20, 0};
    int chance = rate[(min(4, countn))];

    if (!x_chance_in_y(chance, you.time_taken * 600))
        return;

    monster_type basetype =
        random_choose_weighted(4,   MONS_ANACONDA,
                               6,   MONS_HYDRA,
                               3,   MONS_SNAPPING_TURTLE,
                               2,   MONS_ALLIGATOR_SNAPPING_TURTLE,
                               100, RANDOM_MONSTER);

    monster* agent = monster_by_mid(cloud.source);
    create_monster(mgen_data(MONS_SPECTRAL_THING,
                             (cloud.whose == KC_OTHER ?
                                BEH_HOSTILE :
                                BEH_FRIENDLY), cloud.pos,
                             (agent ? agent->foe : short{MHITYOU}),
                             MG_FORCE_PLACE)
                    .set_base(basetype)
                    .set_summoned(actor_by_mid(cloud.source),
                                  SPELL_SPECTRAL_CLOUD, summ_dur(1)));
}

void manage_clouds()
{
    // We can't iterate over env.cloud directly because _dissipate_cloud
    // will remove this cloud and invalidate our iterator.
    vector<cloud_struct *> cloud_ptrs;
    for (auto& entry : env.cloud)
        cloud_ptrs.push_back(&entry.second);

    for (auto ptr : cloud_ptrs)
    {
        cloud_struct& cloud = *ptr;

#ifdef ASSERTS
        if (cell_is_solid(cloud.pos))
        {
            die("cloud %s in %s at (%d,%d)", cloud_type_name(cloud.type).c_str(),
                dungeon_feature_name(env.grid(cloud.pos)), cloud.pos.x, cloud.pos.y);
        }
#endif

        if (cloud.type == CLOUD_SPECTRAL)
            _handle_spectral_cloud(cloud);

        _cloud_interacts_with_terrain(cloud);

        _dissipate_cloud(cloud);
    }

    update_cloud_knowledge();
}

static void _maybe_leave_water(const coord_def pos)
{
    ASSERT_IN_BOUNDS(pos);

    // Rain clouds can occasionally leave shallow water.
    if (env.grid(pos) != DNGN_FLOOR || !one_chance_in(5))
        return;

    if (you.pos() == pos && you.ground_level())
        mpr("The rain has left you waist-deep in water!");
    temp_change_terrain(pos, DNGN_SHALLOW_WATER,
                        random_range(500, 1000),
                        TERRAIN_CHANGE_FLOOD);
}

void delete_cloud(coord_def p)
{
    if (!cloud_at(p))
        return;
    const cloud_type type = cloud_at(p)->type;
    env.cloud.erase(p);
    if (type == CLOUD_RAIN)
        _maybe_leave_water(p);
    _los_cloud_changed(p, CLOUD_NONE, type);
}

void delete_all_clouds()
{
    // We can't iterate over env.cloud directly because delete_cloud
    // will remove this cloud and invalidate our iterator.
    vector<coord_def> cloud_locs;
    for (auto& entry : env.cloud)
        cloud_locs.push_back(entry.first);

    for (auto pos : cloud_locs)
        delete_cloud(pos);
}

// The current use of this function is for shifting in the abyss, so
// that clouds get moved along with the rest of the map.
void move_cloud(coord_def src, coord_def newpos)
{
    if (!cloud_at(src))
        return;
    ASSERT(!cell_is_solid(newpos));

    const cloud_type old = cloud_type_at(newpos);

    env.cloud[newpos] = env.cloud[src];
    env.cloud.erase(src);
    env.cloud[newpos].pos = newpos;
    _los_cloud_changed(src, CLOUD_NONE, env.cloud[newpos].type);
    _los_cloud_changed(newpos, env.cloud[newpos].type, old);
}

void swap_clouds(coord_def p1, coord_def p2)
{
    if (p1 == p2)
        return;
    if (!cloud_at(p1))
    {
        move_cloud(p2, p1);
        return;
    }
    else if (!cloud_at(p2))
    {
        move_cloud(p1, p2);
        return;
    }

    cloud_struct temp = env.cloud[p1];
    env.cloud[p1] = env.cloud[p2];
    env.cloud[p2] = temp;
    env.cloud[p1].pos = p1;
    env.cloud[p2].pos = p2;
    _los_cloud_changed(p1, env.cloud[p1].type, env.cloud[p2].type);
    _los_cloud_changed(p2, env.cloud[p2].type, env.cloud[p1].type);
}

// Places a cloud with the given stats assuming one doesn't already
// exist at that point.
void check_place_cloud(cloud_type cl_type, const coord_def& p, int lifetime,
                       const actor *agent, int spread_rate, int excl_rad)
{
    if (!in_bounds(p) || cloud_at(p))
        return;

    place_cloud(cl_type, p, lifetime, agent, spread_rate, excl_rad);
}

bool cloud_is_stronger(cloud_type ct, const cloud_struct& cloud)
{
    return (is_harmless_cloud(cloud.type) && !is_opaque_cloud(cloud.type))
           || cloud.type == CLOUD_STEAM
           || cloud.type == CLOUD_BLASTMOTES
           || ct == CLOUD_VORTEX; // soon gone
}

/*
 * Places a cloud with the given stats. Will overwrite an old cloud under some
 * circumstances.
 *
 * @param cl_type     The type of cloud to place.
 * @param ctarget     The location of the cloud.
 * @param cl_range    How many turns the cloud will take to decay.
 * @param agent       Any agent that may have caused the cloud. If this is the
 *                    player, god conducts are applied.
 * @param spread_rate How quickly the cloud spreads.
 * @param excl_rad    How large of an exclusion radius to make around the
 *                    cloud.
 * @param do_conducts If true, apply any relevant god conducts for flame
 *                    placement.
*/
void place_cloud(cloud_type cl_type, const coord_def& ctarget, int cl_range,
                 const actor *agent, int spread_rate, int excl_rad,
                 bool do_conducts)
{
    if (is_sanctuary(ctarget) && !is_harmless_cloud(cl_type))
        return;

    if (cl_type == CLOUD_INK && !feat_is_water(env.grid(ctarget)))
        return;

    if (env.level_state & LSTATE_STILL_WINDS
        && cl_type != CLOUD_VORTEX
        && cl_type != CLOUD_INK)
    {
        return;
    }

    const monster * const mons = monster_at(ctarget);

    // Fedhas et al protect their chosen ones from damaging clouds.
    // XX demonic guardians? This logic mostly doesn't apply because protected
    // monsters are also cloud immune, mostly
    if (god_protects(agent, mons)
        && !actor_cloud_immune(*mons, cl_type))
    {
        return;
    }

    ASSERT(!cell_is_solid(ctarget));

    god_conduct_trigger conducts[3];
    kill_category whose = KC_OTHER;
    killer_type killer  = KILL_NON_ACTOR;
    mid_t source        = MID_NOBODY;
    if (agent && agent->is_player())
    {
        if (do_conducts
            && mons && mons->alive()
            && !actor_cloud_immune(*mons, cl_type))
        {
            set_attack_conducts(conducts, *mons, you.can_see(*mons));
        }

        whose = KC_YOU;
        killer = KILL_YOU_MISSILE;
        source = MID_PLAYER;
    }
    else if (agent && agent->is_monster())
    {
        if (agent->as_monster()->friendly())
            whose = KC_FRIENDLY;
        else
            whose = KC_OTHER;
        killer = KILL_MON_MISSILE;
        source = agent->mid;
    }

    // There's already a cloud here. See if we can overwrite it.
    const cloud_struct *cloud = cloud_at(ctarget);
    if (cloud && !cloud_is_stronger(cl_type, *cloud))
        return;

    // If the old cloud was opaque, may need to recalculate los. It *is*
    // possible to overwrite an opaque cloud with a non-opaque one; OOD will do
    // this.
    const cloud_type old = cloud ? cloud->type : CLOUD_NONE;
    env.cloud[ctarget] = cloud_struct(ctarget, cl_type, cl_range * 10,
            _actual_spread_rate(cl_type, spread_rate), whose, killer, source,
            excl_rad);
    _los_cloud_changed(ctarget, env.cloud[ctarget].type, old);
}

bool is_opaque_cloud(cloud_type ctype)
{
    return ctype >= CLOUD_NONE && ctype < NUM_CLOUD_TYPES
           && clouds[ctype].opaque;
}

cloud_type cloud_type_at(const coord_def &c)
{
    return cloud_at(c) ? cloud_at(c)->type : CLOUD_NONE;
}

bool cloud_is_yours_at(const coord_def &c)
{
    return cloud_at(c) ? YOU_KILL(cloud_at(c)->killer) : false;
}

cloud_type random_smoke_type()
{
    return random_choose(CLOUD_GREY_SMOKE, CLOUD_BLUE_SMOKE,
                         CLOUD_BLACK_SMOKE, CLOUD_PURPLE_SMOKE);
}

// Returns true if the cloud type has negative side effects beyond
// plain damage and inventory destruction effects.
static bool _cloud_has_negative_side_effects(cloud_type cloud)
{
    switch (cloud)
    {
    case CLOUD_MEPHITIC:
    case CLOUD_MIASMA:
    case CLOUD_MUTAGENIC:
    case CLOUD_DEGENERATION:
    case CLOUD_CHAOS:
    case CLOUD_PETRIFY:
    case CLOUD_ACID:
    case CLOUD_MISERY:
    case CLOUD_BLASTMOTES:
        return true;
    default:
        return false;
    }
}

static int _cloud_damage_calc(int size, int n_average, int extra,
                              bool maximum_damage)
{
    return maximum_damage?
           extra + size - 1
           : random2avg(size, n_average) + extra;
}

static int _base_dam(const cloud_damage &dam, bool vs_player)
{
    if (vs_player && dam.extra_player_dam)
        return dam.base + 4;
    return dam.base;
}

static int _rand_dam(const cloud_damage &dam, bool vs_player)
{
    if (vs_player && dam.extra_player_dam)
        return dam.random + 7;
    return dam.random;
}

// Calculates the base damage that the cloud does to an actor without
// considering resistances and time spent in the cloud.
static int _cloud_base_damage(const actor *act,
                              cloud_type flavour,
                              bool maximum_damage)
{
    const cloud_damage &dam = clouds[flavour].damage;
    const bool vs_player = act->is_player();
    const int random_dam = _rand_dam(dam, vs_player)
    // Replicate the old acid_splash damage. Boy we hate players, huh?
                         + (flavour == CLOUD_ACID && vs_player ? 12 : 0);
    const int base_dam = _base_dam(dam, vs_player);
    const int trials = dam.random/15 + 1;

    return _cloud_damage_calc(random_dam, trials, base_dam, maximum_damage);

}

/**
 * Is the given actor immune to cloud damage and other negative side effects
 * (other than opaque clouds + invis) from all clouds of the given type?
 */
bool actor_cloud_immune(const actor &act, cloud_type type)
{
    // Qazlalites and scarfwearers get immunity to clouds.
    // and the Cloud Mage too!
    if (is_harmless_cloud(type) || act.cloud_immune())
        return true;

    switch (type)
    {
        case CLOUD_FIRE:
        case CLOUD_FOREST_FIRE:
            if (!act.is_player())
                return act.res_fire() >= 3;
            return player_equip_unrand(UNRAND_SALAMANDER)
#if TAG_MAJOR_VERSION == 34
                   || you.has_mutation(MUT_FLAME_CLOUD_IMMUNITY)
#endif
                   || player_equip_unrand(UNRAND_FIRESTARTER)
                   || you.has_mutation(MUT_IGNITE_BLOOD);
        case CLOUD_HOLY:
            return act.res_holy_energy() >= 3;
        case CLOUD_COLD:
            if (!act.is_player())
                return act.res_cold() >= 3;
            return player_equip_unrand(UNRAND_FROSTBITE)
#if TAG_MAJOR_VERSION == 34
                   || you.has_mutation(MUT_FREEZING_CLOUD_IMMUNITY)
#endif
                   ;
        case CLOUD_MEPHITIC:
            return act.res_poison() > 0 || act.clarity();
        case CLOUD_POISON:
            return act.res_poison() > 0;
        case CLOUD_STEAM:
            return act.res_steam() > 0;
        case CLOUD_MIASMA:
            return act.res_miasma();
        case CLOUD_PETRIFY:
            return act.res_petrify();
        case CLOUD_SPECTRAL:
            return bool(act.holiness() & MH_UNDEAD)
                   || act.is_player()
                      && have_passive(passive_t::r_spectral_mist);
        case CLOUD_ACID:
            return act.res_acid() > 0;
        case CLOUD_STORM:
            return act.res_elec() >= 3;
        case CLOUD_MISERY:
            return act.res_negative_energy() >= 3;
        case CLOUD_VORTEX:
            return act.res_polar_vortex();
        case CLOUD_RAIN:
            return !act.is_fiery();
        default:
            return false;
    }
}

// Returns true if the actor is immune to cloud damage and other negative
// side effects of the given cloud (other than opaque clouds + invis).
//
// Note that actor_cloud_immune may be false even if the actor will
// not be harmed by the cloud. The cloud may have positive
// side-effects on the actor.
bool actor_cloud_immune(const actor &act, const cloud_struct &cloud)
{
    if (actor_cloud_immune(act, cloud.type))
        return true;

    const bool player = act.is_player();

    if (!player
        && (god_protects(*act.as_monster())
            || testbits(act.as_monster()->flags, MF_DEMONIC_GUARDIAN))
        && (cloud.whose == KC_YOU || cloud.whose == KC_FRIENDLY)
        && (act.as_monster()->friendly() || act.as_monster()->neutral())
        && (cloud.whose == KC_YOU || cloud.whose == KC_FRIENDLY))
    {
        return true;
    }

    if (!player && have_passive(passive_t::cloud_immunity)
        && act.was_created_by(MON_SUMM_AID))
    {
        return true;
    }

    return false;
}

// Returns a numeric resistance value for the actor's resistance to
// the cloud's effects. If the actor is immune to the cloud's damage,
// returns WILL_INVULN.
static int _actor_cloud_resist(const actor *act, const cloud_struct &cloud)
{
    if (actor_cloud_immune(*act, cloud))
        return WILL_INVULN;
    switch (cloud.type)
    {
    case CLOUD_RAIN:
        return act->is_fiery() ? 0 : WILL_INVULN;
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        return act->res_fire();
    case CLOUD_HOLY:
        return act->res_holy_energy();
    case CLOUD_COLD:
        return act->res_cold();
    case CLOUD_PETRIFY:
        return act->res_petrify();
    case CLOUD_ACID:
        return act->res_acid();
    case CLOUD_STORM:
        return act->res_elec();
    case CLOUD_MISERY:
        return act->res_negative_energy();

    default:
        return 0;
    }
}

static bool _mephitic_cloud_roll(const monster* mons)
{
    return mons->get_hit_dice() >= MEPH_HD_CAP ? one_chance_in(50)
           : !x_chance_in_y(mons->get_hit_dice(), MEPH_HD_CAP);
}

// Applies cloud messages and side-effects and returns true if the
// cloud had a side-effect. This function does not check for cloud immunity.
// ... but it's only called if the actor isn't immune
static bool _actor_apply_cloud_side_effects(actor *act,
                                            const cloud_struct &cloud,
                                            int final_damage)
{
    ASSERT(act); // XXX: change to actor &act
    const bool player = act->is_player();
    monster *mons = !player? act->as_monster() : nullptr;
    switch (cloud.type)
    {
    case CLOUD_FIRE:
    case CLOUD_STEAM:
        if (player)
            maybe_melt_player_enchantments(BEAM_FIRE, final_damage);
        break;
    case CLOUD_MEPHITIC:
    {
        if (player)
        {
            if (1 + random2(27) >= you.experience_level)
            {
                mpr("You choke on the stench!");
                // effectively one or two turns, since it will be
                // decremented right away
                confuse_player(random_range(2, 3));
                return true;
            }
        }
        else
        {
            bolt beam;
            beam.flavour = BEAM_CONFUSION;
            beam.thrower = cloud.killer;

            if (cloud.whose == KC_FRIENDLY)
                beam.source_id = MID_ANON_FRIEND;

            if (_mephitic_cloud_roll(mons))
            {
                beam.apply_enchantment_to_monster(mons);
                return true;
            }
        }
        break;
    }

    case CLOUD_PETRIFY:
    {
        if (player)
        {
            if (random2(55) - 13 >= you.experience_level)
            {
                you.petrify(cloud.agent());
                return true;
            }
        }
        else
        {
            bolt beam;
            beam.flavour = BEAM_PETRIFY;
            beam.thrower = cloud.killer;

            if (cloud.whose == KC_FRIENDLY)
                beam.source_id = MID_ANON_FRIEND;

            beam.apply_enchantment_to_monster(mons);
            return true;
        }
        break;
    }

    case CLOUD_POISON:
        if (player)
        {
            const actor* agent = cloud.agent();
            poison_player(5 + roll_dice(3, 8), agent ? agent->name(DESC_A) : "",
                          cloud.cloud_name());
        }
        else
            poison_monster(mons, cloud.agent());
        return true;

    case CLOUD_MIASMA:
        if (player)
            return miasma_player(cloud.agent(), cloud.cloud_name());
        else
            return miasma_monster(mons, cloud.agent());

    case CLOUD_MUTAGENIC:
        if (player)
        {
            mpr("The mutagenic energy flows into you.");
            // It's possible that you got trampled into the mutagenic cloud
            // and it's not your fault... so we'll say it's not intentional.
            // (it's quite bad in any case, so players won't scum, probably.)
            contaminate_player(1300 + random2(1250), false);
            // min 2 turns to yellow, max 4
            return true;
        }
        else if (coinflip() && mons->malmutate("mutagenic cloud"))
        {
            if (you_worship(GOD_ZIN) && cloud.whose == KC_YOU)
                did_god_conduct(DID_DELIBERATE_MUTATING, 5 + random2(3));
            return true;
        }
        return false;

    case CLOUD_DEGENERATION:
        if (player && one_chance_in(4))
        {
            mpr("You feel yourself deteriorate.");
            lose_stat(STAT_RANDOM, 1 + random2avg(4,2));
            return true;
        }
        return false;

    case CLOUD_CHAOS:
        if (coinflip())
        {
            chaos_affects_actor(act, cloud.agent());
            return true;
        }
        break;

    case CLOUD_ACID:
        act->acid_corrode(5);
        return true;

    case CLOUD_MISERY:
    {
        int dam = 0;
        actor* agent = cloud.agent();

        // Take 10% of player max hp per tick and 15% for monsters.
        if (act->is_player())
            dam = max(1, div_rand_round(get_real_hp(true, false), 10));
        else
        {
            monster* mon = act->as_monster();
            dam = max(1, div_rand_round(mon->max_hit_points * 3, 20));
        }

        dam = resist_adjust_damage(act, BEAM_NEG, dam);
        dam = timescale_damage(act, dam);

        if (dam > 0)
        {
            act->hurt(agent, dam, BEAM_NEG, KILLED_BY_CLOUD, "", cloud.cloud_name(true));
            if (cloud.whose == KC_YOU)
                did_god_conduct(DID_EVIL, 5 + random2(3));
        }

        return true;
    }
    break;

    case CLOUD_BLASTMOTES:
        if (act->props.exists(BLASTMOTE_IMMUNE_KEY))
            return false;
        explode_blastmotes_at(cloud.pos);
        return true;

    default:
        break;
    }
    return false;
}

static int _actor_cloud_base_damage(const actor *act,
                                    const cloud_struct &cloud,
                                    int resist,
                                    bool maximum_damage)
{
    if (actor_cloud_immune(*act, cloud))
        return 0;

    const int cloud_raw_base_damage =
        _cloud_base_damage(act, cloud.type, maximum_damage);
    const int cloud_base_damage = (resist == WILL_INVULN ?
                                   0 : cloud_raw_base_damage);
    return cloud_base_damage;
}

static int _cloud_damage_output(const actor *actor,
                                beam_type flavour,
                                int base_damage,
                                bool maximum_damage = false)
{
    if (maximum_damage)
        return resist_adjust_damage(actor, flavour, base_damage);

    int dam = actor->apply_ac(base_damage);
    dam = resist_adjust_damage(actor, flavour, dam);
    return max(0, dam);
}

/**
 * How much damage will this cloud do to the given actor?
 *
 * @param act               The actor in question.
 * @param cloud             The cloud in question.
 * @param maximum_damage    Whether to return the maximum possible damage.
 */
static int _actor_cloud_damage(const actor *act,
                               const cloud_struct &cloud,
                               bool maximum_damage)
{
    const int resist = _actor_cloud_resist(act, cloud);
    const int cloud_base_damage = _actor_cloud_base_damage(act, cloud,
                                                           resist,
                                                           maximum_damage);
    int final_damage = cloud_base_damage;

    switch (cloud.type)
    {
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
    case CLOUD_HOLY:
    case CLOUD_COLD:
    case CLOUD_STEAM:
    case CLOUD_SPECTRAL:
    case CLOUD_ACID:
    case CLOUD_STORM:
        final_damage =
            _cloud_damage_output(act, _cloud2beam(cloud.type),
                                 cloud_base_damage,
                                 maximum_damage);
        break;
    default:
        break;
    }

    return timescale_damage(act, final_damage);
}

static void _actor_apply_cloud(actor *act, cloud_struct &cloud)
{
    const bool player = act->is_player();
    monster *mons = act->as_monster();
    if (mons && !mons->alive())
        return;

    if (actor_cloud_immune(*act, cloud))
        return;

    const int resist = _actor_cloud_resist(act, cloud);
    const int cloud_max_base_damage =
        _actor_cloud_base_damage(act, cloud, resist, true);
    const int final_damage = _actor_cloud_damage(act, cloud, false);

    if ((player || final_damage > 0
         || _cloud_has_negative_side_effects(cloud.type))
        && cloud.type != CLOUD_BLASTMOTES) // no effect over time
    {
        cloud.announce_actor_engulfed(act);
    }
    if (player && cloud_max_base_damage > 0 && resist > 0)
        canned_msg(MSG_YOU_RESIST);

    const beam_type cloud_flavour = _cloud2beam(cloud.type);
    if (cloud_flavour != BEAM_NONE)
        act->expose_to_element(cloud_flavour, 7);

    const bool side_effects =
        _actor_apply_cloud_side_effects(act, cloud, final_damage);

    if (!player && (side_effects || final_damage > 0))
        behaviour_event(mons, ME_DISTURB, 0, act->pos());

    if (final_damage)
    {
        actor *oppressor = cloud.agent();
        const string oppr_name =
            oppressor ? " "+apostrophise(oppressor->name(DESC_THE))
                      : "";
        dprf("%s %s %d damage from%s cloud: %s.",
             act->name(DESC_THE).c_str(),
             act->conj_verb("take").c_str(),
             final_damage,
             oppr_name.c_str(),
             cloud.cloud_name().c_str());

        act->hurt(oppressor, final_damage, BEAM_MISSILE,
                  KILLED_BY_CLOUD, "", cloud.cloud_name(true));
    }
}

// Applies damage and side effects for an actor in a cloud and returns
// the damage dealt.
void actor_apply_cloud(actor *act)
{
    cloud_struct* cl = cloud_at(act->pos());
    if (!cl)
        return;
    cloud_struct &cloud = *cl;

    if (cloud.type == CLOUD_STORM)
    {
        // Rain always falls in a storm...
        cloud.type = CLOUD_RAIN;
        _actor_apply_cloud(act, cloud);
        cloud.type = CLOUD_STORM;
        // But lightning, eh, it's a tossup.
        if (coinflip())
            return;
    }
    _actor_apply_cloud(act, cloud);
}


// Describe cloud damage in the form "3-18". If vs_player is set,
// extra anti-player damage is included.
string desc_cloud_damage(cloud_type cl_type, bool vs_player)
{
    const cloud_damage &dam_info = clouds[cl_type].damage;
    const int base = _base_dam(dam_info, vs_player);
    const int rand = _rand_dam(dam_info, vs_player);
    if (rand == 0)
    {
        if (base == 0)
            return "";
        return make_stringf("%d", base);
    }
    return make_stringf("%d-%d", base, base + rand - 1);
}

static bool _cloud_is_harmful(actor *act, cloud_struct &cloud,
                              int maximum_negligible_damage)
{
    return !actor_cloud_immune(*act, cloud)
           && (_cloud_has_negative_side_effects(cloud.type)
               || (_actor_cloud_damage(act, cloud, true) >
                   maximum_negligible_damage));
}

/**
 * Is this cloud type dangerous to you?
 *
 * @param type the type of cloud to look at.
 * @param accept_temp_resistances whether to look at resistances from your form
 *        or durations; items and gods are used regardless of this parameter's value.
 * @param yours whether to treat this cloud as being made by you.
 */
bool is_damaging_cloud(cloud_type type, bool accept_temp_resistances, bool yours)
{
    // If you're immune to clouds, then no clouds are damaging. Bing bong so simple!
    if (you.cloud_immune())
        return false;

    // A nasty hack; map_knowledge doesn't preserve whom the cloud belongs to.
    if (type == CLOUD_VORTEX)
        return !you.duration[DUR_VORTEX] && !you.duration[DUR_VORTEX_COOLDOWN];

    if (accept_temp_resistances)
    {
        cloud_struct cloud;
        cloud.type = type;
        cloud.decay = 100;
        if (yours)
            cloud.set_killer(KILL_YOU);
        return _cloud_is_harmful(&you, cloud, 0);
    }
    else
    {
        // [ds] Yes, this is an ugly kludge: temporarily hide
        // durations and transforms.
        unwind_var<durations_t> old_durations(you.duration);
        unwind_var<transformation> old_form(you.form, you.default_form);
        you.duration.init(0);
        return is_damaging_cloud(type, true, yours);
    }
}

bool cloud_damages_over_time(cloud_type type, bool accept_temp_resistances, bool yours)
{
    return type != CLOUD_BLASTMOTES
        && is_damaging_cloud(type, accept_temp_resistances, yours);
}

/**
 * Will the given monster refuse to walk into the given cloud?
 *
 * @param mons              The monster in question.
 * @param cloud             The cloud in question.
 * @param extra_careful     Whether the monster could suffer any harm from the
 *                          cloud at all, even if it would normally be brave
 *                          enough (based on e.g. hp) to enter the cloud.
 * @return                  Whether the monster is NOT ok to enter the cloud.
 */
static bool _mons_avoids_cloud(const monster* mons, const cloud_struct& cloud,
                               bool extra_careful)
{
    // clouds you're immune to are inherently safe.
    if (actor_cloud_immune(*mons, cloud))
        return false;

    // harmless clouds, likewise.
    if (is_harmless_cloud(cloud.type))
        return false;

    // Berserk monsters are less careful and will blindly plow through any
    // dangerous cloud, just to kill you. {due}
    // Fleeing monsters are heedless and will make very poor life choices.
    if (!extra_careful && (mons->berserk_or_frenzied() || mons_is_fleeing(*mons)))
        return false;

    switch (cloud.type)
    {
    case CLOUD_BLASTMOTES:
        // As with traps, make friendly monsters not walk into blastmotes.
        return mons->attitude == ATT_FRIENDLY
        // Hack: try to avoid penance.
            || mons->attitude == ATT_GOOD_NEUTRAL;

    case CLOUD_RAIN:
        return !mons->is_fiery() || !extra_careful;

    default:
    {
        if (extra_careful)
            return true;

        // calc damage here instead of using _cloud_base_damage() so we can
        // set our own # of trials, to try to make the AI more consistent
        // XXX: add a param instead?
        const cloud_damage &dam_info = clouds[cloud.type].damage;
        const int base_damage = _cloud_damage_calc(dam_info.random,
                                                   max(1, dam_info.random / 9),
                                                   dam_info.base, false);
        // Add in an arbitrary proxy for poison damage from poison/miasma clouds.
        const int bonus_dam = cloud.type == CLOUD_POISON ? roll_dice(3, 4)
                              : cloud.type == CLOUD_MIASMA ? roll_dice(3, 5) : 0;
        const int damage = resist_adjust_damage(mons,
                                                clouds[cloud.type].beam_effect,
                                                base_damage + bonus_dam);
        const int hp_threshold = damage * 3;

        // intelligent monsters want a larger margin of safety
        const int safety_mult = (mons_intel(*mons) > I_ANIMAL) ? 2 : 1;
        // dare we risk the damage?
        const bool hp_ok = mons->hit_points > safety_mult * hp_threshold;
        // dare we risk the status effects?
        const bool sfx_ok = cloud.type != CLOUD_MEPHITIC
                            || x_chance_in_y(mons->get_hit_dice() - 1, 5);
        if (hp_ok && sfx_ok)
            return false;
        break;
    }
    }

    // Exceedingly dumb creatures will wander into harmful clouds.
    if (mons_intel(*mons) == I_BRAINLESS && !extra_careful)
        return false;

    // If we get here, the cloud is potentially harmful.
    return true;
}

// Like the above, but allow a monster to move from one damaging cloud
// to another, even if they're of different types.
bool mons_avoids_cloud(const monster* mons, coord_def pos, bool placement)
{
    if (!cloud_at(pos))
        return false;

    // Is the target cloud okay?
    if (!_mons_avoids_cloud(mons, *cloud_at(pos), placement))
        return false;

    // If we're already in a cloud that we'd want to avoid then moving
    // from one to the other is okay.
    if (!in_bounds(mons->pos()) || mons->pos() == pos)
        return true;

    if (!cloud_at(mons->pos()))
        return true;

    return !_mons_avoids_cloud(mons, *cloud_at(mons->pos()), true);
}

bool is_harmless_cloud(cloud_type type)
{
    return clouds[type].beam_effect == BEAM_NONE
           && clouds[type].damage.base == 0
           && clouds[type].damage.random == 0
           && !_cloud_has_negative_side_effects(type)
           && type != CLOUD_VORTEX;
}

string cloud_type_name(cloud_type type, bool terse)
{
    if (type <= CLOUD_NONE || type >= NUM_CLOUD_TYPES)
        return "buggy goodness";

    ASSERT(clouds[type].terse_name);
    if (terse || clouds[type].verbose_name == nullptr)
        return clouds[type].terse_name;
    return clouds[type].verbose_name;
}

cloud_type cloud_name_to_type(const string &name)
{
    const string lower_name = lowercase_string(name);

    if (lower_name == "random")
        return CLOUD_RANDOM;
    else if (lower_name == "debugging")
        return CLOUD_DEBUGGING;

    for (int i = CLOUD_NONE; i < CLOUD_RANDOM; i++)
        if (cloud_type_name(static_cast<cloud_type>(i)) == lower_name)
            return static_cast<cloud_type>(i);

    return CLOUD_NONE;
}

coord_def random_walk(coord_def start, int dist)
{
    ASSERT(in_bounds(start));
    ASSERT(dist >= 1);

    int moves_left = dist;
    coord_def pos = start;
    while (moves_left-- > 0)
    {
        int okay_dirs = 0;
        int dir       = -1;
        for (int j = 0; j < 8; j++)
        {
            const coord_def new_pos   = pos + Compass[j];

            if (in_bounds(new_pos) && !feat_is_solid(env.grid(new_pos))
                && one_chance_in(++okay_dirs))
            {
                dir = j;
            }
        }

        if (okay_dirs == 0)
            break;

        if (one_chance_in(++okay_dirs))
            continue;

        pos       += Compass[dir];
    }

    return pos;
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
            return KC_YOU;

        case KILL_MON:
        case KILL_MON_MISSILE:
        case KILL_NON_ACTOR:
            return KC_OTHER;

        default:
            die("invalid killer type");
    }
    return KC_OTHER;
}

killer_type cloud_struct::whose_to_killer(kill_category _whose)
{
    switch (_whose)
    {
        case KC_YOU:         return KILL_YOU_MISSILE;
        case KC_FRIENDLY:    return KILL_MON_MISSILE;
        case KC_OTHER:       return KILL_NON_ACTOR;
        case KC_NCATEGORIES: die("invalid kill category");
    }
    return KILL_NONE;
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

actor *cloud_struct::agent() const
{
    return find_agent(source, whose);
}

string cloud_struct::cloud_name(bool terse) const
{
    return cloud_type_name(type, terse);
}

void cloud_struct::announce_actor_engulfed(const actor *act,
                                           bool beneficial) const
{
    ASSERT(act); // XXX: change to const actor &act
    if (!you.can_see(*act))
        return;

    if (type != CLOUD_RAIN)
    {
        mprf("%s %s in %s.",
             act->name(DESC_THE).c_str(),
             beneficial ? act->conj_verb("bask").c_str()
                        : (act->conj_verb("are") + " engulfed").c_str(),
             cloud_name().c_str());
        return;
    } else {
        mprf("%s %s in the rain.",
            act->name(DESC_THE).c_str(),
            act->conj_verb(silenced(act->pos())?
                        "steam" : "sizzle").c_str());
    }
}

/**
 * What colour is the given cloud?
 *
 * @param cloudno       The cloud in question.
 * @return              An appropriate colour for the cloud.
 *                      May vary from call to call (randomized for some cloud
 *                      types).
 */
colour_t get_cloud_colour(const cloud_struct &cloud)
{
    // if we have the colour in data, use that.
    if (clouds[cloud.type].colour)
        return clouds[cloud.type].colour;

    // weird clouds
    switch (cloud.type)
    {
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (cloud.decay <= 20)
            return RED;
        if (cloud.decay <= 40)
            return LIGHTRED;

        // total weight 16
        return random_choose_weighted(9, YELLOW,
                                      4, RED,
                                      3, LIGHTRED);

    case CLOUD_COLD:
        if (cloud.decay <= 20)
            return BLUE;
        if (cloud.decay <= 40)
            return LIGHTBLUE;

        // total weight 16
        return random_choose_weighted(9, WHITE,
                                      4, BLUE,
                                      3, LIGHTBLUE);
        break;

    default:
        return LIGHTGREY;
    }
}

coord_def get_cloud_originator(const coord_def& pos)
{
    const cloud_struct* cloud = cloud_at(pos);
    if (!cloud)
        return coord_def();
    const actor *agent = actor_by_mid(cloud->source);
    if (!agent)
        return coord_def();
    return agent->pos();
}

void remove_vortex_clouds(mid_t whose)
{
    // Needed to clean up after the end of tornado cooldown, so we can again
    // assume all "raging winds" clouds are harmful. This is needed only
    // because map_knowledge doesn't preserve the knowledge about whom the
    // cloud belongs to. If this changes, please remove this function. For
    // example, this approach doesn't work if we ever make Tornado a monster
    // spell (excluding immobile and mindless casters).
    // XXX: this comment seems impossibly out of date? ^

    // We can't iterate over env.cloud directly because delete_cloud
    // will remove this cloud and invalidate our iterator.
    vector<coord_def> vortices;
    for (auto& entry : env.cloud)
        if (entry.second.type == CLOUD_VORTEX && entry.second.source == whose)
            vortices.push_back(entry.first);

    for (auto pos : vortices)
        delete_cloud(pos);
}

static void _spread_cloud(coord_def pos, cloud_type type, int radius, int pow,
                          int &remaining, int ratio = 10,
                          mid_t agent_mid = 0, kill_category kcat = KC_OTHER)
{
    bolt beam;
    beam.target = pos;
    beam.use_target_as_pos = true;
    explosion_map exp_map;
    exp_map.init(INT_MAX);
    beam.determine_affected_cells(exp_map, coord_def(), 0,
                                  radius, true, true);

    coord_def centre(9,9);
    for (distance_iterator di(pos, true, false); di; ++di)
    {
        if (di.radius() > radius)
            return;

        if ((exp_map(*di - pos + centre) < INT_MAX) && !cloud_at(*di)
            && (di.radius() < radius || x_chance_in_y(ratio, 100)))
        {
            place_cloud(type, *di, pow + random2(pow), nullptr);
            --remaining;

            // Setting this way since the agent of the cloud may be dead before
            // cloud is placed, so no agent exists to pass to place_cloud (though
            // proper blame should still be assigned)
            if (cloud_struct* cloud = cloud_at(*di))
            {
                cloud->source = agent_mid;
                cloud->whose = kcat;
            }
        }

        // Placed all clouds for this spreader
        if (remaining == 0)
            return;
    }
}

void run_cloud_spreaders(int dur)
{
    if (!dur)
        return;

    for (map_marker *marker : env.markers.get_all(MAT_CLOUD_SPREADER))
    {
        map_cloud_spreader_marker * const mark
            = dynamic_cast<map_cloud_spreader_marker*>(marker);

        mark->speed_increment += dur;
        int rad = min(mark->speed_increment / mark->speed, mark->max_rad - 1) + 1;
        int ratio = (mark->speed_increment - ((rad - 1) * mark->speed))
                    * 100 / mark->speed;

        if (ratio == 0)
        {
            rad--;
            ratio = 100;
        }

        _spread_cloud(mark->pos, mark->ctype, rad, mark->duration,
                        mark->remaining, ratio, mark->agent_mid, mark->kcat);
        if ((rad >= mark->max_rad && ratio >= 100) || mark->remaining == 0)
        {
            env.markers.remove(mark);
            break;
        }
    }
}

const cloud_tile_info& cloud_type_tile_info(cloud_type type)
{
    return clouds[type].tile_info;
}

/// Knock out clouds & set the still winds level flag; also message.
void start_still_winds()
{
    delete_all_clouds();
    env.level_state |= LSTATE_STILL_WINDS;
    mprf(MSGCH_WARN, "%s", "The air becomes perfectly still.");
}

void end_still_winds()
{
    env.level_state &= ~LSTATE_STILL_WINDS;
    mpr("The air resumes its normal movements.");
}

/**
 * Surround a monster with clouds of a certain type (excepting squares with an
 * allied monster). This also deletes the same cloud if it's on top of the
 * monster (which will happen as they walk around).
 */
void surround_actor_with_cloud(const actor* a, cloud_type cloud)
{
    const coord_def pos = a->pos();
    const cloud_struct* overhead = cloud_at(pos);
    if (overhead && overhead->type == cloud)
        delete_cloud(pos);
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        const cloud_struct* existing = cloud_at(*ai);
        // dprf("surround_actor_with_cloud x:%d y:%d solid:%d cloud_at:%s",
        //      ai->x, ai->y, cell_is_solid(*ai), existing ? "y" : "n");
        if (cell_is_solid(*ai))
            continue;
        if (existing && existing->type != cloud)
            continue;
        const monster* mons = monster_at(*ai);
        if (mons && mons->alive() && mons_aligned(a, mons))
            continue;
        place_cloud(cloud, *ai, 2 + random2(6), a);
    }
}

bool cloud_is_removed(cloud_type cloud)
{
    switch (cloud)
    {
#if TAG_MAJOR_VERSION == 34
    case CLOUD_GLOOM:
    case CLOUD_EMBERS:
        return true;
#endif
    default:
        return false;
    }
}

static bool _is_chaos_polyable(const actor &defender)
{
    if (!defender.can_safely_mutate())
        return false;  // no polymorphing undead

    const monster* mon = defender.as_monster();
    if (!mon)
        return true;

    return !mon->is_firewood() && !mons_invuln_will(*mon);
}

static bool _is_chaos_slowable(const actor &defender)
{
    return !defender.is_firewood() && !defender.stasis();
}

struct chaos_effect
{
    string name;
    int chance;
    function<bool(const actor& def)> valid;
    beam_type flavour;
    function<bool(actor* victim, actor* source)> misc_effect;
};

// TODO: Unite this with _chaos_beam_flavour in beam.cc.
// For now, update that when you update this.
static const vector<chaos_effect> chaos_effects = {
    {
        "clone", 1, [](const actor &d) {
            return d.is_monster() && mons_clonable(d.as_monster(), true);
        },
        BEAM_NONE, [](actor* victim, actor* /*source*/) {
            ASSERT(victim->is_monster());
            monster *clone = clone_mons(victim->as_monster());
            if (!clone)
                return false;

            const bool obvious_effect = you.can_see(*victim) && you.can_see(*clone);

            if (one_chance_in(3))
                clone->attitude = coinflip() ? ATT_FRIENDLY : ATT_NEUTRAL;

            // The player shouldn't get new permanent followers from cloning.
            if (clone->attitude == ATT_FRIENDLY && !clone->is_summoned())
                clone->mark_summoned(MON_SUMM_CLONE, summ_dur(6));
            else
                clone->flags |= (MF_NO_REWARD | MF_HARD_RESET);

            // Monsters being cloned is interesting.
            xom_is_stimulated(clone->friendly() ? 12 : 25);
            return obvious_effect;
        },
    },
    {
        "polymorph", 2, _is_chaos_polyable, BEAM_POLYMORPH,
    },
    {
        "rage", 5, [](const actor &victim) {
            return victim.can_go_berserk() && !victim.clarity();
        }, BEAM_NONE, [](actor* victim, actor* source) {
            if (victim->is_monster())
            {
                monster* mon = victim->as_monster();
                ASSERT(mon);
                if (mon->can_go_frenzy()) {
                    mon->go_frenzy(source);
                }
            }
            else
                victim->go_berserk(false);

            return you.can_see(*victim);
        },
    },
    { "hasting", 12, _is_chaos_slowable, BEAM_HASTE },
    { "mighting", 12, [](const actor &victim) {
        return !victim.is_monster()
               || (mons_has_attacks(*(victim.as_monster()))
                   && !victim.as_monster()->has_ench(ENCH_MIGHT));
    }, BEAM_MIGHT },
    { "resistance", 10, [](const actor &victim) {
        return victim.res_fire() < 3 && victim.res_cold() < 3 &&
               victim.res_elec() < 3 && victim.res_poison() < 3 &&
               victim.res_acid() < 3; }, BEAM_RESISTANCE, },
    { "slowing", 10, _is_chaos_slowable, BEAM_SLOW },
    { "confusing", 12, [](const actor &victim) {
        return !victim.clarity() && !victim.is_peripheral(); },
               BEAM_CONFUSION },
    { "weakening", 10, [](const actor & victim) {
        return !victim.is_monster()
               || mons_has_attacks(*(victim.as_monster()));
    }, BEAM_WEAKNESS, },
    { "will-halving", 10, [](const actor &victim) {
       return !victim.is_monster()
              || mons_invuln_will(*(victim.as_monster()));
    }, BEAM_VULNERABILITY, },
    { "blinking", 3, nullptr, BEAM_BLINK },
    { "corroding", 5, [](const actor &victim) {
        return victim.res_acid() < 3; },
        BEAM_NONE, [](actor* victim, actor* /*source*/) {
           victim->corrode_equipment();
           return you.can_see(*victim);
       },
    },
    { "vitrifying", 5, nullptr, BEAM_VITRIFY, },
    { "ensnaring", 3, [](const actor &victim) {
        return !victim.is_web_immune(); },
        BEAM_NONE, [](actor* victim, actor* /*source*/) {
           ensnare(victim);
           return you.can_see(*victim);
       },
    },
    {
        "minipara", 3, _is_chaos_slowable, BEAM_NONE,
        [](actor* victim, actor* source) {
            victim->paralyse(source, 1);
            return you.can_see(*victim);
        },
    },
    {
        "sleep", 3, [](const actor &victim) {
            return victim.can_sleep();
        }, BEAM_SLEEP,
    },
    {
        "petrify", 3, [](const actor &victim) {
            return _is_chaos_slowable(victim) && !victim.res_petrify();
        }, BEAM_PETRIFY,
    },
    {
        "blinding", 5, [](const actor &victim) {
            return victim.can_be_dazzled();
        }, BEAM_NONE, [](actor* victim, actor* source) {
            if (victim->is_player())
                blind_player(random_range(7, 12), ETC_RANDOM);
            else
                dazzle_target(victim, source, 149);
            return you.can_see(*victim);
        },
    },
};

// Applies a debuff-style random chaos effect to an actor. This may have a source,
// in the case of chaos weapons, or no source in the case of some clouds.
//
// Returns true if the effect was obvious to the player.
bool chaos_affects_actor(actor* victim, actor* source)
{
    ASSERT(victim);

    bool obvious_effect = false;

    vector<pair<const chaos_effect&, int>> weights;
    for (const chaos_effect &effect : chaos_effects)
        if (!effect.valid || effect.valid(*victim))
            weights.push_back({effect, effect.chance});

    const chaos_effect &effect = *random_choose_weighted(weights);

#ifdef NOTE_DEBUG_CHAOS_EFFECTS
    take_note(Note(NOTE_MESSAGE, 0, 0, "CHAOS effect: " + effect.name), true);
#endif

    if (effect.misc_effect && effect.misc_effect(victim, source))
        obvious_effect = true;

    bolt beam;
    beam.flavour = effect.flavour;
    beam.no_saving_throw = true;

    if (beam.flavour != BEAM_NONE)
    {
        if (victim->is_player() && have_passive(passive_t::no_haste)
            && beam.flavour == BEAM_HASTE)
        {
            simple_god_message(" protects you from inadvertent hurry.");
            return true;
        }

        beam.glyph        = 0;
        beam.range        = 0;
        beam.colour       = BLACK;
        beam.effect_known = false;

        beam.thrower =
            source && source->is_player()                       ? KILL_YOU
            : source && source->as_monster()->confused_by_you() ? KILL_YOU_CONF
                                                                : KILL_MON;

        if (beam.thrower == KILL_YOU || (source && source->as_monster()->friendly()))
            beam.attitude = ATT_FRIENDLY;

        beam.source_id = source ? source->mid : MID_NOBODY;

        beam.source = victim->pos();
        beam.target = victim->pos();
        beam.ench_power = 100;

        const bool you_could_see = you.can_see(*victim);
        beam.fire();

        if (you_could_see)
        {
            obvious_effect = beam.obvious_effect;
            if (!victim->wont_attack() &&
                (beam.flavour == BEAM_HASTE || beam.flavour == BEAM_MIGHT))
            {
                xom_is_stimulated(12);
            }
        }

    }

    return obvious_effect;
}
