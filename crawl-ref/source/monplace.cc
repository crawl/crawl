/*
 *  File:       monplace.cc
 *  Summary:    Functions used when placing monsters in the dungeon.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <algorithm>

#include "monplace.h"

#include "arena.h"
#include "branch.h"
#include "directn.h" // for the Compass
#include "externs.h"
#include "ghost.h"
#include "lev-pand.h"
#include "makeitem.h"
#include "message.h"
#include "monstuff.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "player.h"
#include "religion.h"
#include "state.h"
#include "stuff.h"
#include "spells4.h"
#include "terrain.h"
#include "traps.h"
#include "view.h"

static std::vector<int> vault_mon_types;
static std::vector<int> vault_mon_bases;
static std::vector<int> vault_mon_weights;

#define VAULT_MON_TYPES_KEY   "vault_mon_types"
#define VAULT_MON_BASES_KEY   "vault_mon_bases"
#define VAULT_MON_WEIGHTS_KEY "vault_mon_weights"

// NEW place_monster -- note that power should be set to:
// 51 for abyss
// 52 for pandemonium
// x otherwise

// proximity is the same as for mons_place:
// 0 is no restrictions
// 1 attempts to place near player
// 2 attempts to avoid player LOS

#define BIG_BAND        20

static monster_type _resolve_monster_type(monster_type mon_type,
                                          proximity_type proximity,
                                          monster_type &base_type,
                                          coord_def &pos,
                                          unsigned mmask,
                                          dungeon_char_type *stair_type,
                                          int *lev_mons);

static void _define_zombie(int mid, monster_type ztype,
                           monster_type cs, int power, coord_def pos);
static monster_type _band_member(band_type band, int power);
static band_type _choose_band(int mon_type, int power, int &band_size);
// static int _place_monster_aux(int mon_type, beh_type behaviour, int target,
//                               int px, int py, int power, int extra,
//                               bool first_band_member, int dur = 0);

static int _place_monster_aux(const mgen_data &mg, bool first_band_member,
                              bool force_pos = false);

// Returns whether actual_grid is compatible with grid_wanted for monster
// movement (or for monster generation, if generation is true).
bool grid_compatible(dungeon_feature_type grid_wanted,
                     dungeon_feature_type actual_grid)
{
    if (grid_wanted == DNGN_FLOOR)
    {
        return (actual_grid >= DNGN_FLOOR
                    && actual_grid != DNGN_BUILDER_SPECIAL_WALL
                || actual_grid == DNGN_SHALLOW_WATER);
    }

    if (grid_wanted >= DNGN_ROCK_WALL
        && grid_wanted <= DNGN_CLEAR_PERMAROCK_WALL)
    {
        // A monster can only move through or inhabit permanent rock if that's
        // exactly what it's asking for.
        if (actual_grid == DNGN_PERMAROCK_WALL
            || actual_grid == DNGN_CLEAR_PERMAROCK_WALL)
        {
            return (grid_wanted == DNGN_PERMAROCK_WALL
                    || grid_wanted == DNGN_CLEAR_PERMAROCK_WALL);
        }

        return (actual_grid >= DNGN_ROCK_WALL
                && actual_grid <= DNGN_CLEAR_PERMAROCK_WALL);
    }

    // Restricted fountains during generation, so we don't get monsters
    // "trapped" in fountains for easy killing.
    return (grid_wanted == actual_grid
            || (grid_wanted == DNGN_DEEP_WATER
                && (actual_grid == DNGN_SHALLOW_WATER
                    || actual_grid == DNGN_FOUNTAIN_BLUE)));
}

// Can this monster survive on actual_grid?
//
// If you have an actual monster, use this instead of the overloaded function
// that uses only the monster class to make decisions.
bool monster_habitable_grid(const monsters *m,
                            dungeon_feature_type actual_grid)
{
    // Zombified monsters enjoy the same habitat as their original.
    const int montype = mons_is_zombified(m) ? mons_zombie_base(m)
                                             : m->type;

    return (monster_habitable_grid(montype, actual_grid, mons_flies(m),
                                   mons_cannot_move(m)));
}

inline static bool _mons_airborne(int mcls, int flies, bool paralysed)
{
    if (flies == -1)
        flies = mons_class_flies(mcls);

    return (paralysed ? flies == FL_LEVITATE : flies != FL_NONE);
}

// Can monsters of class monster_class live happily on actual_grid?
// Use flies == true to pretend the monster can fly.
//
// [dshaligram] We're trying to harmonise the checks from various places into
// one check, so we no longer care if a water elemental springs into existence
// on dry land, because they're supposed to be able to move onto dry land
// anyway.
bool monster_habitable_grid(int monster_class,
                            dungeon_feature_type actual_grid,
                            int flies, bool paralysed)
{
    const dungeon_feature_type grid_preferred =
        habitat2grid(mons_class_primary_habitat(monster_class));
    const dungeon_feature_type grid_nonpreferred =
        habitat2grid(mons_class_secondary_habitat(monster_class));

    if (grid_compatible(grid_preferred, actual_grid)
        || (grid_nonpreferred != grid_preferred
            && grid_compatible(grid_nonpreferred, actual_grid)))
    {
        return (true);
    }

    // [dshaligram] Flying creatures are all DNGN_FLOOR, so we
    // only have to check for the additional valid grids of deep
    // water and lava.
    if (_mons_airborne(monster_class, flies, paralysed)
        && (actual_grid == DNGN_LAVA || actual_grid == DNGN_DEEP_WATER))
    {
        return (true);
    }

    return (false);
}

// Returns true if the monster can submerge in the given grid.
bool monster_can_submerge(const monsters *mons, dungeon_feature_type grid)
{
    if (mons->type == MONS_TRAPDOOR_SPIDER && grid == DNGN_FLOOR)
        return (!find_trap(mons->pos()));

    switch (mons_primary_habitat(mons))
    {
    case HT_WATER:
        // Monsters can submerge in shallow water - this is intentional.
        return (grid_is_watery(grid));

    case HT_LAVA:
        return (grid == DNGN_LAVA);

    default:
        return (false);
    }
}

static bool _need_moderate_ood(int lev_mons)
{
    return (env.turns_on_level > 700 - lev_mons * 117);
}

static bool _need_super_ood(int lev_mons)
{
    return (env.turns_on_level > 1400 - lev_mons * 117
            && one_chance_in(5000));
}

static int _fuzz_mons_level(int level)
{
    if (one_chance_in(7))
    {
        const int fuzz = random2avg(9, 2);
        return (fuzz > 4? level + fuzz - 4 : level);
    }
    return (level);
}

static void _hell_spawn_random_monsters()
{
    // Monster generation in the Vestibule drops off quickly.
    const int taper_off_turn = 500;
    int genodds = 240;
    // genodds increases once you've spent more than 500 turns in Hell.
    if (env.turns_on_level > taper_off_turn)
    {
        genodds += (env.turns_on_level - taper_off_turn);
        genodds  = (genodds < 0 ? 20000 : std::min(genodds, 20000));
    }

    if (x_chance_in_y(5, genodds))
    {
        mgen_data mg(WANDERING_MONSTER);
        mg.proximity = (one_chance_in(10) ? PROX_NEAR_STAIRS
                                          : PROX_AWAY_FROM_PLAYER);
        mons_place(mg);
        viewwindow(true, false);
    }
}

//#define DEBUG_MON_CREATION

// This function is now only called about once every 5 turns. (Used to be
// every turn independent of how much time an action took, which was not ideal.)
// To arrive at spawning rates close to how they used to be, replace the
// one_chance_in(value) checks with the new x_chance_in_y(5, value). (jpeg)
void spawn_random_monsters()
{
    if (crawl_state.arena)
        return;

#ifdef DEBUG_MON_CREATION
    mpr("in spawn_random_monsters()", MSGCH_DIAGNOSTICS);
#endif
    if (player_in_branch(BRANCH_VESTIBULE_OF_HELL))
    {
        _hell_spawn_random_monsters();
        return;
    }

    if (env.spawn_random_rate == 0)
    {
#ifdef DEBUG_MON_CREATION
        mpr("random monster gen turned off", MSGCH_DIAGNOSTICS);
#endif
        return;
    }

    const int rate = (you.char_direction == GDT_DESCENDING) ?
                     env.spawn_random_rate : 8;

    // Place normal dungeon monsters,  but not in player LOS.
    if (you.level_type == LEVEL_DUNGEON && x_chance_in_y(5, rate))
    {
#ifdef DEBUG_MON_CREATION
        mpr("Create wandering monster...", MSGCH_DIAGNOSTICS);
#endif
        proximity_type prox = (one_chance_in(10) ? PROX_NEAR_STAIRS
                                                 : PROX_AWAY_FROM_PLAYER);

        // The rules change once the player has picked up the Orb...
        if (you.char_direction == GDT_ASCENDING)
            prox = (one_chance_in(6) ? PROX_CLOSE_TO_PLAYER : PROX_ANYWHERE);

        mgen_data mg(WANDERING_MONSTER);
        mg.proximity = prox;
        mons_place(mg);
        viewwindow(true, false);
        return;
    }

    // Place Abyss monsters. (Now happens regularly every 5 turns which might
    // look a bit strange for a place as chaotic as the Abyss. Then again,
    // the player is unlikely to meet all of them and notice this.)
    if (you.level_type == LEVEL_ABYSS
        && (you.char_direction != GDT_GAME_START
            || x_chance_in_y(5, rate)))
    {
        mons_place(mgen_data(WANDERING_MONSTER));
        viewwindow(true, false);
        return;
    }

    // Place Pandemonium monsters.
    if (you.level_type == LEVEL_PANDEMONIUM && x_chance_in_y(5, rate))
    {
        pandemonium_mons();
        viewwindow(true, false);
        return;
    }

    // A portal vault *might* decide to turn on random monster spawning,
    // but it's off by default.
    if (you.level_type == LEVEL_PORTAL_VAULT && x_chance_in_y(5, rate))
    {
        mons_place(mgen_data(WANDERING_MONSTER));
        viewwindow(true, false);
    }

    // No random monsters in the Labyrinth.
}

monster_type pick_random_monster(const level_id &place)
{
    int level;
    if (place.level_type == LEVEL_PORTAL_VAULT)
        level = you.your_level;
    else
        level = place.absdepth();
    return pick_random_monster(place, level, level);
}

monster_type pick_random_monster(const level_id &place, int power,
                                 int &lev_mons)
{
    if (crawl_state.arena)
    {
        monster_type type = arena_pick_random_monster(place, power, lev_mons);
        if (type != RANDOM_MONSTER)
            return (type);
    }

    if (place.level_type == LEVEL_LABYRINTH)
        return (MONS_PROGRAM_BUG);

    if (place == BRANCH_ECUMENICAL_TEMPLE)
        return (MONS_PROGRAM_BUG);

    if (place.level_type == LEVEL_PORTAL_VAULT)
    {
        monster_type      base_type = (monster_type) 0;
        coord_def         dummy1;
        dungeon_char_type dummy2;
        monster_type type =
            _resolve_monster_type(RANDOM_MONSTER, PROX_ANYWHERE, base_type,
                                  dummy1, 0, &dummy2, &lev_mons);

#if DEBUG || DEBUG_DIAGNOSTICS
        if (base_type != 0 && base_type != MONS_PROGRAM_BUG)
            mpr("Random portal vault mon discarding base type.",
                MSGCH_ERROR);
#endif
        return (type);
    }

    monster_type mon_type = MONS_PROGRAM_BUG;

    lev_mons = power;

    if (place == BRANCH_MAIN_DUNGEON
        && lev_mons != 51 && one_chance_in(4))
    {
        lev_mons = random2(power);
    }

    if (place == BRANCH_MAIN_DUNGEON
        && lev_mons <= 27)
    {
        // If on D:1, allow moderately out-of-depth monsters only after
        // a certain elapsed turn count on the level (currently 700 turns).
        if (lev_mons || _need_moderate_ood(lev_mons))
            lev_mons = _fuzz_mons_level(lev_mons);

        // Potentially nasty surprise, but very rare.
        if (_need_super_ood(lev_mons))
            lev_mons += random2(12);

        // Slightly out of depth monsters are more common:
        // [ds] Replaced with a fuzz above for a more varied mix.
        //if (need_moderate_ood(lev_mons))
        //    lev_mons += random2(5);

        lev_mons = std::min(27, lev_mons);
    }

    // Abyss or Pandemonium. Almost never called from Pan; probably only
    // if a random demon gets summon anything spell.
    if (lev_mons == 51
        || place.level_type == LEVEL_PANDEMONIUM
        || place.level_type == LEVEL_ABYSS)
    {
        do
        {
            int count = 0;

            do
            {
                // was: random2(400) {dlb}
                mon_type = static_cast<monster_type>( random2(NUM_MONSTERS) );
                count++;
            }
            while (mons_abyss(mon_type) == 0 && count < 2000);

            if (count == 2000)
                return (MONS_PROGRAM_BUG);
            if (crawl_state.arena && arena_veto_random_monster(mon_type))
                continue;
        }
        while (random2avg(100, 2) > mons_rare_abyss(mon_type)
               && !one_chance_in(100));
    }
    else
    {
        int level, diff, chance;

        lev_mons = std::min(30, lev_mons);

        int i;
        for (i = 0; i < 10000; ++i)
        {
            int count = 0;

            do
            {
                mon_type = static_cast<monster_type>(random2(NUM_MONSTERS));
                count++;
            }
            while (mons_rarity(mon_type, place) == 0 && count < 2000);

            if (count == 2000)
                return (MONS_PROGRAM_BUG);

            if (crawl_state.arena && arena_veto_random_monster(mon_type))
                continue;

            level  = mons_level(mon_type, place);
            diff   = level - lev_mons;
            chance = mons_rarity(mon_type, place) - (diff * diff);

            if ((lev_mons >= level - 5 && lev_mons <= level + 5)
                && random2avg(100, 2) <= chance)
            {
                break;
            }
        }

        if (i == 10000)
            return (MONS_PROGRAM_BUG);
    }

    return (mon_type);
}

static bool _can_place_on_trap(int mon_type, trap_type trap)
{
    if (trap == TRAP_TELEPORT)
        return (false);

    if (trap == TRAP_SHAFT)
    {
        if (mon_type == RANDOM_MONSTER)
            return (false);

        return (mons_class_flag(mon_type, M_FLIES)
                || mons_class_flag(mon_type, M_LEVITATE)
                || get_monster_data(mon_type)->size == SIZE_TINY);
    }

    return (true);
}

bool drac_colour_incompatible(int drac, int colour)
{
    return (drac == MONS_DRACONIAN_SCORCHER && colour == MONS_WHITE_DRACONIAN);
}

static monster_type _resolve_monster_type(monster_type mon_type,
                                          proximity_type proximity,
                                          monster_type &base_type,
                                          coord_def &pos,
                                          unsigned mmask,
                                          dungeon_char_type *stair_type,
                                          int *lev_mons)
{
    if (mon_type == RANDOM_DRACONIAN)
    {
        // Pick any random drac, constrained by colour if requested.
        do
        {
            mon_type =
                static_cast<monster_type>(
                    random_range(MONS_BLACK_DRACONIAN,
                                 MONS_DRACONIAN_SCORCHER));
        }
        while (base_type != MONS_PROGRAM_BUG
               && mon_type != base_type
               && (mons_species(mon_type) == mon_type
                   || drac_colour_incompatible(mon_type, base_type)));
    }
    else if (mon_type == RANDOM_BASE_DRACONIAN)
    {
        mon_type =
            static_cast<monster_type>(
                random_range(MONS_BLACK_DRACONIAN, MONS_PALE_DRACONIAN));
    }
    else if (mon_type == RANDOM_NONBASE_DRACONIAN)
    {
        mon_type =
            static_cast<monster_type>(
                random_range(MONS_DRACONIAN_CALLER, MONS_DRACONIAN_SCORCHER));
    }

    // (2) Take care of non-draconian random monsters.
    else if (mon_type == RANDOM_MONSTER)
    {
        level_id place = level_id::current();

        // Respect destination level for staircases.
        if (proximity == PROX_NEAR_STAIRS)
        {
            int tries = 0;
            int pval  = 0;
            while (++tries <= 320)
            {
                pos = random_in_bounds();

                if (mgrd(pos) != NON_MONSTER || pos == you.pos())
                    continue;

                // Is the grid verboten?
                if (!unforbidden( pos, mmask ))
                    continue;

                // Don't generate monsters on top of teleport traps.
                const trap_def* ptrap = find_trap(pos);
                if (ptrap && !_can_place_on_trap(mon_type, ptrap->type))
                    continue;

                // Check whether there's a stair
                // and whether it leads to another branch.
                pval = near_stairs(pos, 1, *stair_type, place.branch);

                // No monsters spawned in the Temple.
                if (branches[place.branch].id == BRANCH_ECUMENICAL_TEMPLE)
                    continue;

                // Found a position near the stairs!
                if (pval > 0)
                    break;
            }

            if (tries > 320)
            {
                // Give up and try somewhere else.
                proximity = PROX_AWAY_FROM_PLAYER;
            }
            else
            {
                if (*stair_type == DCHAR_STAIRS_DOWN) // deeper level
                    ++*lev_mons;
                else if (*stair_type == DCHAR_STAIRS_UP) // higher level
                {
                    // Monsters don't come from outside the dungeon.
                    if (*lev_mons <= 0)
                    {
                        proximity = PROX_AWAY_FROM_PLAYER;
                        // In that case lev_mons stays as it is.
                    }
                    else
                        --*lev_mons;
                }
            }
        } // end proximity check

        if (place == BRANCH_HALL_OF_BLADES)
            mon_type = MONS_DANCING_WEAPON;
        else
        {
            if (you.level_type == LEVEL_PORTAL_VAULT)
            {
                if (vault_mon_types.size() == 0)
                    return (MONS_PROGRAM_BUG);

                int i = choose_random_weighted(vault_mon_weights.begin(),
                                               vault_mon_weights.end());
                int type = vault_mon_types[i];
                int base = vault_mon_bases[i];

                if (type == -1)
                {
                    place = level_id::from_packed_place(base);
                    // If lev_mons is set to you.your_level, it was probably
                    // set as a default meaning "the current dungeon depth",
                    // which for a portal vault using its own definition
                    // of random monsters means "the depth of whatever place
                    // we're using for picking the random monster".
                    if (*lev_mons == you.your_level)
                        *lev_mons = place.absdepth();
                    // pick_random_monster() is called below
                }
                else
                {
                    base_type = (monster_type) base;
                    mon_type  = (monster_type) type;
                    if (mon_type == RANDOM_DRACONIAN
                        || mon_type == RANDOM_BASE_DRACONIAN
                        || mon_type == RANDOM_NONBASE_DRACONIAN)
                    {
                        mon_type =
                            _resolve_monster_type(mon_type, proximity,
                                                  base_type, pos, mmask,
                                                  stair_type, lev_mons);
                    }
                    return (mon_type);
                }
            }

            int tries = 0;
            while (tries++ < 300)
            {
                // Now pick a monster of the given branch and level.
                mon_type = pick_random_monster(place, *lev_mons, *lev_mons);

                // Don't allow monsters too stupid to use stairs (e.g.
                // zombified undead) to be placed at stairs.
                if (proximity != PROX_NEAR_STAIRS
                    || mons_class_can_use_stairs(mon_type))
                {
                    break;
                }
            }

            if (proximity == PROX_NEAR_STAIRS && tries >= 300)
            {
                proximity = PROX_AWAY_FROM_PLAYER;

                // Reset target level.
                if (*stair_type == DCHAR_STAIRS_DOWN)
                    --*lev_mons;
                else if (*stair_type == DCHAR_STAIRS_UP)
                    ++*lev_mons;

                mon_type = pick_random_monster(place, *lev_mons, *lev_mons);
            }
        }
    }
    return (mon_type);
}

// A short function to check the results of near_stairs().
// Returns 0 if the point is not near stairs.
// Returns 1 if the point is near unoccupied stairs.
// Returns 2 if the point is near player-occupied stairs.
static int _is_near_stairs(coord_def &p)
{
    int result = 0;

    for (int i = -1; i <= 1; ++i)
        for (int j = -1; j <= 1; ++j)
        {
            if (!in_bounds(p))
                continue;

            const dungeon_feature_type feat = grd(p);
            if (is_stair(feat))
            {
                // Shouldn't matter for escape hatches.
                if (grid_is_escape_hatch(feat))
                    continue;

                // Should there be several stairs, don't overwrite the
                // player on stairs info.
                if (result < 2)
                    result = (p == you.pos()) ? 2 : 1;
            }
        }

    return (result);
}

/*
 * Checks if the monster is ok to place at mg_pos. If force_location
 * is true, then we'll be less rigorous in our checks, in particular
 * allowing land monsters to be placed in shallow water and water
 * creatures in fountains.
 */
static bool _valid_monster_location(const mgen_data &mg,
                                    const coord_def &mg_pos)
{
    const int montype = (mons_class_is_zombified(mg.cls) ? mg.base_type
                                                         : mg.cls);
    const dungeon_feature_type grid_preferred =
        habitat2grid(mons_class_primary_habitat(montype));
    const dungeon_feature_type grid_nonpreferred =
        habitat2grid(mons_class_secondary_habitat(montype));

    if (!in_bounds(mg_pos))
        return (false);

    // Occupied?
    if (mgrd(mg_pos) != NON_MONSTER || mg_pos == you.pos())
        return (false);

    // Is the monster happy where we want to put it?
    if (!grid_compatible(grid_preferred, grd(mg_pos))
        && (grid_nonpreferred == grid_preferred
            || !grid_compatible(grid_nonpreferred, grd(mg_pos))))
    {
        return (false);
    }

    if (mg.behaviour != BEH_FRIENDLY && is_sanctuary(mg_pos))
        return (false);

    // Don't generate monsters on top of teleport traps.
    // (How did they get there?)
    const trap_def* ptrap = find_trap(mg_pos);
    if (ptrap && !_can_place_on_trap(mg.cls, ptrap->type))
        return (false);

    return (true);
}

static bool _valid_monster_location(mgen_data &mg)
{
    return _valid_monster_location(mg, mg.pos);
}

int place_monster(mgen_data mg, bool force_pos)
{
#ifdef DEBUG_MON_CREATION
    mpr("in place_monster()", MSGCH_DIAGNOSTICS);
#endif
    int tries = 0;
    dungeon_char_type stair_type = NUM_DCHAR_TYPES;
    int id = -1;

    // (1) Early out (summoned to occupied grid).
    if (mg.use_position() && mgrd(mg.pos) != NON_MONSTER)
        return (-1);

    mg.cls = _resolve_monster_type(mg.cls, mg.proximity, mg.base_type,
                                   mg.pos, mg.map_mask,
                                   &stair_type, &mg.power);

    if (mg.cls == MONS_PROGRAM_BUG)
        return (-1);

    // (3) Decide on banding (good lord!)
    int band_size = 1;
    monster_type band_monsters[BIG_BAND];        // band monster types
    band_monsters[0] = mg.cls;

    if (mg.permit_bands())
    {
#ifdef DEBUG_MON_CREATION
        mpr("Choose band members...", MSGCH_DIAGNOSTICS);
#endif
        const band_type band = _choose_band(mg.cls, mg.power, band_size);
        band_size++;

        for (int i = 1; i < band_size; i++)
            band_monsters[i] = _band_member( band, mg.power );
    }

    // Returns 2 if the monster is placed near player-occupied stairs.
    int pval = _is_near_stairs(mg.pos);
    if (mg.proximity == PROX_NEAR_STAIRS)
    {
        // For some cases disallow monsters on stairs.
        if (mons_class_is_stationary(mg.cls)
            || (pval == 2 // Stairs occupied by player.
                && (mons_speed(mg.cls) == 0
                    || grd(mg.pos) == DNGN_LAVA
                    || grd(mg.pos) == DNGN_DEEP_WATER)))
        {
            mg.proximity = PROX_AWAY_FROM_PLAYER;
        }
    }

    // (4) For first monster, choose location.  This is pretty intensive.
    bool proxOK;
    bool close_to_player;

    // Player shoved out of the way?
    bool shoved = false;

    if (!mg.use_position())
    {
        tries = 0;

        // Try to pick px, py that is
        // a) not occupied
        // b) compatible
        // c) in the 'correct' proximity to the player

        while (true)
        {
            // Dropped number of tries from 60.
            if (tries++ >= 45)
                return (-1);

            // Placement already decided for PROX_NEAR_STAIRS.
            // Else choose a random point on the map.
            if (mg.proximity != PROX_NEAR_STAIRS)
                mg.pos = random_in_bounds();

            if (!_valid_monster_location(mg))
                continue;

            // Is the grid verboten?
            if (!unforbidden(mg.pos, mg.map_mask))
                continue;

            // Let's recheck these even for PROX_NEAR_STAIRS, just in case.
            // Check proximity to player.
            proxOK = true;

            switch (mg.proximity)
            {
            case PROX_ANYWHERE:
                if (grid_distance( you.pos(), mg.pos ) < 2 + random2(3))
                    proxOK = false;
                break;

            case PROX_CLOSE_TO_PLAYER:
            case PROX_AWAY_FROM_PLAYER:
                close_to_player = (distance(you.pos(), mg.pos) < 64);

                if (mg.proximity == PROX_CLOSE_TO_PLAYER && !close_to_player
                    || mg.proximity == PROX_AWAY_FROM_PLAYER && close_to_player)
                {
                    proxOK = false;
                }
                break;

            case PROX_NEAR_STAIRS:
                if (pval == 2) // player on stairs
                {
                    // 0 speed monsters can't shove player out of their way.
                    if (mons_speed(mg.cls) == 0)
                    {
                        proxOK = false;
                        break;
                    }
                    // Swap the monster and the player spots, unless the
                    // monster was generated in lava or deep water.
                    if (grd(mg.pos) == DNGN_LAVA
                        || grd(mg.pos) == DNGN_DEEP_WATER)
                    {
                        proxOK = false;
                        break;
                    }
                    shoved = true;
                    coord_def mpos = mg.pos;
                    mg.pos         = you.pos();
                    you.moveto(mpos);
                }
                proxOK = (pval > 0);
                break;
            }

            if (!proxOK)
                continue;

            // Cool.. passes all tests.
            break;
        } // end while... place first monster
    }
    else if (!_valid_monster_location(mg))
    {
        // Sanity check that the specified position is valid.
        return (-1);
    }

    id = _place_monster_aux(mg, true, force_pos);

    // Bail out now if we failed.
    if (id == -1)
        return (-1);

    monsters *mon = &menv[id];
    if (mg.needs_patrol_point())
    {
        mon->patrol_point = mon->pos();
#ifdef DEBUG_PATHFIND
        mprf("Monster %s is patrolling around (%d, %d).",
             mon->name(DESC_PLAIN).c_str(), mon->pos().x, mon->pos().y);
#endif
    }

    // Message to player from stairwell/gate appearance.
    if (see_grid(mg.pos) && mg.proximity == PROX_NEAR_STAIRS)
    {
        std::string msg;

        if (player_monster_visible( &menv[id] ))
            msg = menv[id].name(DESC_CAP_A);
        else if (shoved)
            msg = "Something";

        if (shoved)
        {
            msg += " shoves you out of the ";
            if (stair_type == DCHAR_ARCH)
                msg += "gateway!";
            else
                msg += "stairwell!";
            mpr(msg.c_str());
        }
        else if (!msg.empty())
        {
            if (stair_type == DCHAR_STAIRS_DOWN)
                msg += " comes up the stairs.";
            else if (stair_type == DCHAR_STAIRS_UP)
                msg += " comes down the stairs.";
            else if (stair_type == DCHAR_ARCH)
                msg += " comes through the gate.";
            else
                msg = "";

            if (!msg.empty())
                mpr(msg.c_str());
        }

        // Special case: must update the view for monsters created
        // in player LOS.
        viewwindow(true, false);
    }

    // Now, forget about banding if the first placement failed, or there are
    // too many monsters already, or we successfully placed by stairs.
    if (id >= MAX_MONSTERS - 30 || mg.proximity == PROX_NEAR_STAIRS)
        return (id);

    // Not PROX_NEAR_STAIRS, so it will be part of a band, if there is any.
    if (band_size > 1)
        menv[id].flags |= MF_BAND_MEMBER;

    const bool priest = mons_class_flag(mon->type, M_PRIEST);

    mgen_data band_template = mg;
    // (5) For each band monster, loop call to place_monster_aux().
    for (int i = 1; i < band_size; i++)
    {
        if (band_monsters[i] == MONS_PROGRAM_BUG)
            break;

        band_template.cls = band_monsters[i];
        const int band_id = _place_monster_aux(band_template, false);
        if (band_id != -1 && band_id != NON_MONSTER)
        {
            menv[band_id].flags |= MF_BAND_MEMBER;
            // Priestly band leaders should have an entourage of the
            // same religion.
            if (priest)
                menv[band_id].god = mon->god;
        }
    }

    // Placement of first monster, at least, was a success.
    return (id);
}

static int _place_monster_aux(const mgen_data &mg,
                              bool first_band_member, bool force_pos)
{
    int id = -1;
    coord_def fpos;

    // Gotta be able to pick an ID.
    for (id = 0; id < MAX_MONSTERS; id++)
        if (menv[id].type == -1)
            break;

    // Too many monsters on level?
    if (id == MAX_MONSTERS)
        return (-1);

    menv[id].reset();

    const int montype = (mons_class_is_zombified(mg.cls) ? mg.base_type
                                                         : mg.cls);

    // Setup habitat and placement.
    // If the space is occupied, try some neighbouring square instead.
    if (first_band_member && in_bounds(mg.pos)
        && (mg.behaviour == BEH_FRIENDLY || !is_sanctuary(mg.pos))
        && mgrd(mg.pos) == NON_MONSTER && mg.pos != you.pos()
        && (force_pos || monster_habitable_grid(montype, grd(mg.pos))))
    {
        fpos = mg.pos;
    }
    else
    {
        int i;
        // We'll try 1000 times for a good spot.
        for (i = 0; i < 1000; ++i)
        {
            fpos = mg.pos + coord_def( random_range(-3, 3),
                                       random_range(-3, 3) );

            if (_valid_monster_location(mg, fpos))
                break;
        }

        // Did we really try 1000 times?
        if (i == 1000)
            return (-1);
    }

    ASSERT(mgrd(fpos) == NON_MONSTER);

    if (crawl_state.arena)
    {
        if (arena_veto_place_monster(mg, first_band_member, fpos))
            return (-1);
    }

    // Now, actually create the monster. (Wheeee!)
    menv[id].type         = mg.cls;
    menv[id].base_monster = mg.base_type;
    menv[id].number       = mg.number;

    menv[id].moveto(fpos);

    // Link monster into monster grid.
    mgrd(fpos) = id;

    // Generate a brand shiny new monster, or zombie.
    if (mons_class_is_zombified(mg.cls))
        _define_zombie(id, mg.base_type, mg.cls, mg.power, fpos);
    else
        define_monster(id);

    // Is it a god gift?
    if (mg.god != GOD_NO_GOD)
    {
        menv[id].god    = mg.god;
        menv[id].flags |= MF_GOD_GIFT;
    }
    // Not a god gift, give priestly monsters a god.
    else if (mons_class_flag(mg.cls, M_PRIEST))
    {
        switch (mons_genus(mg.cls))
        {
        case MONS_ORC:
            menv[id].god = GOD_BEOGH;
            break;
        case MONS_MUMMY:
            menv[id].god = coinflip() ? GOD_KIKUBAAQUDGHA : GOD_YREDELEMNUL;
            break;
        case MONS_DRACONIAN:
        case MONS_ELF:
        {
            god_type gods[] = {GOD_KIKUBAAQUDGHA, GOD_YREDELEMNUL,
                               GOD_MAKHLEB};
            menv[id].god = RANDOM_ELEMENT(gods);
            break;
        }
        default:
            mprf(MSGCH_ERROR, "ERROR: Invalid monster priest '%s'",
                 menv[id].name(DESC_PLAIN, true).c_str());
            ASSERT(false);
            break;
        }
    }
    // 6 out of 7 non-priestly orcs are believers.
    else if (mons_genus(mg.cls) == MONS_ORC)
    {
        if (x_chance_in_y(6, 7))
            menv[id].god = GOD_BEOGH;
    }
    // Angels and Daevas belong to TSO
    else if (mons_class_holiness(mg.cls) == MH_HOLY)
    {
        menv[id].god = GOD_SHINING_ONE;
    }

    // If the caller requested a specific colour for this monster,
    // apply it now.
    if (mg.colour != BLACK)
        menv[id].colour = mg.colour;

    // The return of Boris is now handled in monster_die()...
    // not setting this for Boris here allows for multiple Borises
    // in the dungeon at the same time.  -- bwr
    if (mons_is_unique(mg.cls))
        you.unique_creatures[mg.cls] = true;

    if (mons_class_flag(mg.cls, M_INVIS))
        menv[id].add_ench(ENCH_INVIS);

    if (mons_class_flag(mg.cls, M_CONFUSED))
        menv[id].add_ench(ENCH_CONFUSION);

    if (mg.cls == MONS_SHAPESHIFTER)
        menv[id].add_ench(ENCH_SHAPESHIFTER);

    if (mg.cls == MONS_GLOWING_SHAPESHIFTER)
        menv[id].add_ench(ENCH_GLOWING_SHAPESHIFTER);

    if (monster_can_submerge(&menv[id], grd(fpos)) && !one_chance_in(5))
        menv[id].add_ench(ENCH_SUBMERGED);

    menv[id].flags |= MF_JUST_SUMMONED;

    // Don't leave shifters in their starting shape.
    if (mg.cls == MONS_SHAPESHIFTER || mg.cls == MONS_GLOWING_SHAPESHIFTER)
    {
        no_messages nm;
        monster_polymorph(&menv[id], RANDOM_MONSTER);

        // It's not actually a known shapeshifter if it happened to be
        // placed in LOS of the player.
        menv[id].flags &= ~MF_KNOWN_MIMIC;
    }

    // dur should always be 1-6 for monsters that can be abjured.
    const bool summoned = mg.abjuration_duration >= 1
                       && mg.abjuration_duration <= 6;

    if (mg.cls == MONS_DANCING_WEAPON && mg.number != 1) // ie not from spell
    {
        give_item(id, mg.power, summoned);

        if (menv[id].inv[MSLOT_WEAPON] != NON_ITEM)
            menv[id].colour = mitm[menv[id].inv[MSLOT_WEAPON]].colour;
    }
    else if (mons_class_itemuse(mg.cls) >= MONUSE_STARTING_EQUIPMENT)
    {
        give_item(id, mg.power, summoned);
        // Give these monsters a second weapon -- bwr
        if (mons_class_wields_two_weapons(mg.cls))
            give_item(id, mg.power, summoned);

        unwind_var<int> save_speedinc(menv[id].speed_increment);
        menv[id].wield_melee_weapon(false);
    }

    // Give manticores 8 to 16 spike volleys.
    if (mg.cls == MONS_MANTICORE)
        menv[id].number = 8 + random2(9);

    // Set attitude, behaviour and target.
    menv[id].attitude  = ATT_HOSTILE;
    menv[id].behaviour = mg.behaviour;

    // Statues cannot sleep (nor wander but it means they are a bit
    // more aware of the player than they'd be otherwise).
    if (mons_is_statue(mg.cls))
        menv[id].behaviour = BEH_WANDER;

    menv[id].foe_memory = 0;

    // Setting attitude will always make the monster wander...
    // If you want sleeping hostiles, use BEH_SLEEP since the default
    // attitude is hostile.
    if (mg.behaviour > NUM_BEHAVIOURS)
    {
        if (mg.behaviour == BEH_FRIENDLY)
            menv[id].attitude = ATT_FRIENDLY;

        if (mg.behaviour == BEH_GOOD_NEUTRAL)
            menv[id].attitude = ATT_GOOD_NEUTRAL;

        if (mg.behaviour == BEH_NEUTRAL)
            menv[id].attitude = ATT_NEUTRAL;

        menv[id].behaviour = BEH_WANDER;
    }

    if (summoned)
        menv[id].mark_summoned( mg.abjuration_duration, true,
                                mg.summon_type );

    menv[id].foe = mg.foe;

    // Initialise pandemonium demons.
    if (menv[id].type == MONS_PANDEMONIUM_DEMON)
    {
        ghost_demon ghost;
        ghost.init_random_demon();
        menv[id].set_ghost(ghost);
        menv[id].pandemon_init();
    }

    mark_interesting_monst(&menv[id], mg.behaviour);

    if (player_monster_visible(&menv[id]) && mons_near(&menv[id]))
        seen_monster(&menv[id]);

    if (crawl_state.arena)
        arena_placed_monster(&menv[id]);

    return (id);
}

static monster_type _pick_random_zombie()
{
    static std::vector<monster_type> zombifiable;
    if (zombifiable.empty())
    {
        for (int i = 0; i < NUM_MONSTERS; ++i)
        {
            if (mons_species(i) != i || i == MONS_PROGRAM_BUG)
                continue;

            const monster_type mcls = static_cast<monster_type>(i);
            if (!mons_zombie_size(mcls))
                continue;

            zombifiable.push_back(mcls);
        }
    }
    return (zombifiable[ random2(zombifiable.size()) ]);
}

static void _define_zombie( int mid, monster_type ztype,
                            monster_type cs, int power, coord_def pos )
{
    monster_type cls       = MONS_PROGRAM_BUG;
    monster_type mons_sec2 = MONS_PROGRAM_BUG;
    int  zombie_size       = 0;
    bool ignore_rarity     = false;

    if (power > 27)
        power = 27;

    // Set size based on zombie class (cs).
    switch (cs)
    {
        case MONS_ZOMBIE_SMALL:
        case MONS_SIMULACRUM_SMALL:
        case MONS_SKELETON_SMALL:
            zombie_size = Z_SMALL;
            break;

        case MONS_ZOMBIE_LARGE:
        case MONS_SIMULACRUM_LARGE:
        case MONS_SKELETON_LARGE:
            zombie_size = Z_BIG;
            break;

        case MONS_SPECTRAL_THING:
            zombie_size = -1;
            break;

        default:
            // This should NEVER happen.
            perror("\ncreate_zombie() got passed incorrect zombie type!\n");
            end(0);
            break;
    }

    // That is, random creature from which to fashion undead.
    if (ztype == MONS_PROGRAM_BUG)
    {
        // How OOD this zombie can be.
        int relax = 5;

        // Pick an appropriate creature to make a zombie out of,
        // levelwise.  The old code was generating absolutely
        // incredible OOD zombies.
        while (true)
        {
            cls = _pick_random_zombie();

            // Actually pick a monster that is happy where we want to put it.
            // Fish zombies on land are helpless and uncool.
            if (!monster_habitable_grid(cls, grd(pos)))
                continue;

            // On certain branches, zombie creation will fail if we use
            // the mons_rarity() functions, because (for example) there
            // are NO zombifiable "native" abyss creatures. Other branches
            // where this is a problem are hell levels and the crypt.
            // we have to watch for summoned zombies on other levels, too,
            // such as the Temple, HoB, and Slime Pits.
            if (you.level_type != LEVEL_DUNGEON
                || player_in_hell()
                || player_in_branch( BRANCH_HALL_OF_ZOT )
                || player_in_branch( BRANCH_VESTIBULE_OF_HELL )
                || player_in_branch( BRANCH_ECUMENICAL_TEMPLE )
                || player_in_branch( BRANCH_CRYPT )
                || player_in_branch( BRANCH_TOMB )
                || player_in_branch( BRANCH_HALL_OF_BLADES )
                || player_in_branch( BRANCH_SNAKE_PIT )
                || player_in_branch( BRANCH_SLIME_PITS )
                || one_chance_in(1000))
            {
                ignore_rarity = true;
            }

            // Don't make out-of-rarity zombies when we don't have to.
            if (!ignore_rarity && mons_rarity(cls) == 0)
                continue;

            // If skeleton, monster must have a skeleton.
            if ((cs == MONS_SKELETON_SMALL || cs == MONS_SKELETON_LARGE)
                && !mons_skeleton(cls))
            {
                continue;
            }

            // Size must match, but you can make a spectral thing out
            // of anything.
            if (zombie_size != -1 && mons_zombie_size(cls) != zombie_size)
                continue;

            if (cs == MONS_SKELETON_SMALL || cs == MONS_SIMULACRUM_SMALL)
            {
                // Skeletal or icy draconians shouldn't be coloured.
                // How could you tell?
                if (mons_genus(cls) == MONS_DRACONIAN
                    && cls != MONS_DRACONIAN)
                {
                    cls = MONS_DRACONIAN;
                }
                // The same goes for rats.
                else if (mons_genus(cls) == MONS_RAT
                    && cls != MONS_RAT)
                {
                    cls = MONS_RAT;
                }
            }

            // Hack -- non-dungeon zombies are always made out of nastier
            // monsters.
            if (you.level_type != LEVEL_DUNGEON && mons_power(cls) > 8)
                break;

            // Check for rarity.. and OOD - identical to mons_place()
            int level, diff, chance;

            level = mons_level(cls) - 4;
            diff  = level - power;

            chance = (ignore_rarity) ? 100
                                     : mons_rarity(cls) - (diff * diff) / 2;

            if (power > level - relax && power < level + relax
                && random2avg(100, 2) <= chance)
            {
                break;
            }

            // Every so often, we'll relax the OOD restrictions.  Avoids
            // infinite loops (if we don't do this, things like creating
            // a large skeleton on level 1 may hang the game!)
            if (one_chance_in(5))
                relax++;
        }

        // Set type and secondary appropriately.
        menv[mid].base_monster = cls;
        mons_sec2 = cls;
    }
    else
    {
        menv[mid].base_monster = mons_species(ztype);
        mons_sec2              = menv[mid].base_monster;
    }

    // Set type to the base type to calculate appropriate stats.
    menv[mid].type = menv[mid].base_monster;

    define_monster(mid);

    menv[mid].hit_points     = hit_points( menv[mid].hit_dice, 6, 5 );
    menv[mid].max_hit_points = menv[mid].hit_points;

    menv[mid].ac -= 2;

    if (menv[mid].ac < 0)
        menv[mid].ac = 0;

    menv[mid].ev -= 5;

    if (menv[mid].ev < 0)
        menv[mid].ev = 0;

    menv[mid].speed -= 2;

    if (menv[mid].speed < 3)
        menv[mid].speed = 3;

    menv[mid].speed_increment = 70;

    // Now override type with the required type.
    if (cs == MONS_ZOMBIE_SMALL || cs == MONS_ZOMBIE_LARGE)
    {
        menv[mid].type = ((mons_zombie_size(menv[mid].base_monster) == Z_BIG)
                          ? MONS_ZOMBIE_LARGE : MONS_ZOMBIE_SMALL);
    }
    else if (cs == MONS_SKELETON_SMALL || cs == MONS_SKELETON_LARGE)
    {
        menv[mid].hit_points     = hit_points( menv[mid].hit_dice, 5, 4 );
        menv[mid].max_hit_points = menv[mid].hit_points;

        menv[mid].ac -= 4;

        if (menv[mid].ac < 0)
            menv[mid].ac = 0;

        menv[mid].ev -= 2;

        if (menv[mid].ev < 0)
            menv[mid].ev = 0;

        menv[mid].type = ((mons_zombie_size( menv[mid].base_monster ) == Z_BIG)
                          ? MONS_SKELETON_LARGE : MONS_SKELETON_SMALL);
    }
    else if (cs == MONS_SIMULACRUM_SMALL || cs == MONS_SIMULACRUM_LARGE)
    {
        // Simulacra aren't tough, but you can create piles of them. -- bwr
        menv[mid].hit_points     = hit_points( menv[mid].hit_dice, 1, 4 );
        menv[mid].max_hit_points = menv[mid].hit_points;
        menv[mid].type = ((mons_zombie_size( menv[mid].base_monster ) == Z_BIG)
                            ? MONS_SIMULACRUM_LARGE : MONS_SIMULACRUM_SMALL);
    }
    else if (cs == MONS_SPECTRAL_THING)
    {
        menv[mid].hit_points     = hit_points( menv[mid].hit_dice, 4, 4 );
        menv[mid].max_hit_points = menv[mid].hit_points;
        menv[mid].ac            += 4;
        menv[mid].type           = MONS_SPECTRAL_THING;
    }

    menv[mid].base_monster = mons_sec2;
    menv[mid].colour       = mons_class_colour(cs);
}

static band_type _choose_band(int mon_type, int power, int &band_size)
{
#ifdef DEBUG_MON_CREATION
    mpr("in choose_band()", MSGCH_DIAGNOSTICS);
#endif
    // Band size describes the number of monsters in addition to
    // the band leader.
    band_size = 0; // Single monster, no band.
    band_type band = BAND_NO_BAND;

    switch (mon_type)
    {
    case MONS_ORC:
        if (coinflip())
            break;
        // intentional fall-through {dlb}
    case MONS_ORC_WIZARD:
        band = BAND_ORCS;
        band_size = 2 + random2(3);
        break;

    case MONS_ORC_PRIEST:
    case MONS_ORC_WARRIOR:
        band = BAND_ORC_WARRIOR;
        band_size = 2 + random2(3);
        break;

    case MONS_ORC_WARLORD:
    case MONS_SAINT_ROKA:
        band_size = 5 + random2(5);   // warlords have large bands
        // intentional fall through
    case MONS_ORC_KNIGHT:
        band = BAND_ORC_KNIGHT;       // orcs + knight
        band_size += 3 + random2(4);
        break;

    case MONS_ORC_HIGH_PRIEST:
        band = BAND_ORC_HIGH_PRIEST;
        band_size = 4 + random2(4);
        break;

    case MONS_BIG_KOBOLD:
        if (power > 3)
        {
            band = BAND_KOBOLDS;
            band_size = 2 + random2(6);
        }
        break;

    case MONS_KILLER_BEE:
        band = BAND_KILLER_BEES;
        band_size = 2 + random2(4);
        break;

    case MONS_FLYING_SKULL:
        band = BAND_FLYING_SKULLS;
        band_size = 2 + random2(4);
        break;
    case MONS_SLIME_CREATURE:
        band = BAND_SLIME_CREATURES;
        band_size = 2 + random2(4);
        break;
    case MONS_YAK:
        band = BAND_YAKS;
        band_size = 2 + random2(4);
        break;
    case MONS_UGLY_THING:
        band = BAND_UGLY_THINGS;
        band_size = 2 + random2(4);
        break;
    case MONS_HELL_HOUND:
        band = BAND_HELL_HOUNDS;
        band_size = 2 + random2(3);
        break;
    case MONS_JACKAL:
        band = BAND_JACKALS;
        band_size = 1 + random2(3);
        break;
    case MONS_HELL_KNIGHT:
    case MONS_MARGERY:
        band = BAND_HELL_KNIGHTS;
        band_size = 4 + random2(4);
        break;
    case MONS_JOSEPHINE:
    case MONS_NECROMANCER:
    case MONS_VAMPIRE_MAGE:
        band = BAND_NECROMANCER;
        band_size = 4 + random2(4);
        break;
    case MONS_GNOLL:
        band = BAND_GNOLLS;
        band_size = (coinflip() ? 3 : 2);
        break;
    case MONS_BUMBLEBEE:
        band = BAND_BUMBLEBEES;
        band_size = 2 + random2(4);
        break;
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        if (power > 9 && one_chance_in(3))
        {
            band = BAND_CENTAURS;
            band_size = 2 + random2(4);
        }
        break;

    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        if (coinflip())
        {
            band = BAND_YAKTAURS;
            band_size = 2 + random2(3);
        }
        break;

    case MONS_DEATH_YAK:
        band = BAND_DEATH_YAKS;
        band_size = 2 + random2(4);
        break;
    case MONS_INSUBSTANTIAL_WISP:
        band = BAND_INSUBSTANTIAL_WISPS;
        band_size = 4 + random2(5);
        break;
    case MONS_OGRE_MAGE:
        band = BAND_OGRE_MAGE;
        band_size = 4 + random2(4);
        break;
    case MONS_BALRUG:
        band = BAND_BALRUG;      // RED gr demon
        band_size = 2 + random2(3);
        break;
    case MONS_CACODEMON:
        band = BAND_CACODEMON;      // BROWN gr demon
        band_size = 1 + random2(3);
        break;

    case MONS_EXECUTIONER:
        if (coinflip())
        {
            band = BAND_EXECUTIONER;  // DARKGREY gr demon
            band_size = 1 + random2(3);
        }
        break;

    case MONS_PANDEMONIUM_DEMON:
        band = BAND_PANDEMONIUM_DEMON;
        band_size = random_range(1, 3);
        break;

    case MONS_HELLWING:
        if (coinflip())
        {
            band = BAND_HELLWING;  // LIGHTGREY gr demon
            band_size = 1 + random2(4);
        }
        break;

    case MONS_DEEP_ELF_FIGHTER:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_FIGHTER;
            band_size = 3 + random2(4);
        }
        break;

    case MONS_DEEP_ELF_KNIGHT:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_KNIGHT;
            band_size = 3 + random2(4);
        }
        break;

    case MONS_DEEP_ELF_HIGH_PRIEST:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_HIGH_PRIEST;
            band_size = 3 + random2(4);
        }
        break;

    case MONS_KOBOLD_DEMONOLOGIST:
        if (coinflip())
        {
            band = BAND_KOBOLD_DEMONOLOGIST;
            band_size = 3 + random2(6);
        }
        break;

    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
        band = BAND_NAGAS;
        band_size = 3 + random2(4);
        break;

    case MONS_WAR_DOG:
        band = BAND_WAR_DOGS;
        band_size = 2 + random2(4);
        break;

    case MONS_GREY_RAT:
        band = BAND_GREY_RATS;
        band_size = 4 + random2(6);
        break;

    case MONS_GREEN_RAT:
        band = BAND_GREEN_RATS;
        band_size = 4 + random2(6);
        break;

    case MONS_ORANGE_RAT:
        band = BAND_ORANGE_RATS;
        band_size = 3 + random2(4);
        break;

    case MONS_SHEEP:
        band = BAND_SHEEP;
        band_size = 3 + random2(5);
        break;

    case MONS_GHOUL:
        band = BAND_GHOULS;
        band_size = 2 + random2(3);
        break;

    case MONS_HOG:
        band = BAND_HOGS;
        band_size = 1 + random2(3);
        break;

    case MONS_GIANT_MOSQUITO:
        band = BAND_GIANT_MOSQUITOES;
        band_size = 1 + random2(3);
        break;

    case MONS_DEEP_TROLL:
        band = BAND_DEEP_TROLLS;
        band_size = 3 + random2(3);
        break;

    case MONS_HELL_HOG:
        band = BAND_HELL_HOGS;
        band_size = 1 + random2(3);
        break;

    case MONS_BOGGART:
        band = BAND_BOGGARTS;
        band_size = 2 + random2(3);
        break;

    case MONS_BLINK_FROG:
        band = BAND_BLINK_FROGS;
        band_size = 2 + random2(3);
        break;

    case MONS_SKELETAL_WARRIOR:
        band = BAND_SKELETAL_WARRIORS;
        band_size = 2 + random2(3);
        break;

    case MONS_CYCLOPS:
        if (one_chance_in(5) || player_in_branch(BRANCH_SHOALS))
        {
            band = BAND_SHEEP;  // Odyssey reference
            band_size = 2 + random2(3);
        }
        break;

    case MONS_POLYPHEMUS:
        band = BAND_DEATH_YAKS;
        band_size = 3 + random2(3);
        break;

    case MONS_HARPY:
        band = BAND_HARPIES;
        band_size = 2 + random2(3);
        break;

    // Journey -- Added Draconian Packs
    case MONS_WHITE_DRACONIAN:
    case MONS_RED_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_PALE_DRACONIAN:
        if (power > 18 && one_chance_in(3) && you.level_type == LEVEL_DUNGEON)
        {
            band = BAND_DRACONIAN;
            band_size = random_range(2, 4);
        }
        break;

    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_SHIFTER:
        if (power > 20 && you.level_type == LEVEL_DUNGEON)
        {
            band = BAND_DRACONIAN;
            band_size = random_range(3, 6);
        }
        break;

    case MONS_TIAMAT:
        band = BAND_DRACONIAN;
        // yup, scary
        band_size = random_range(3,6) + random_range(3,6) + 2;
        break;

    case MONS_ILSUIW:
        band = BAND_ILSUIW;
        band_size = 3 + random2(3);
        break;

    case MONS_AZRAEL:
        band = BAND_AZRAEL;
        band_size = 4 + random2(5);
        break;
    } // end switch

    if (band != BAND_NO_BAND && band_size == 0)
        band = BAND_NO_BAND;

    if (band_size >= BIG_BAND)
        band_size = BIG_BAND - 1;

    return (band);
}

static monster_type _band_member(band_type band, int power)
{
    monster_type mon_type = MONS_PROGRAM_BUG;
    int temp_rand;

    if (band == BAND_NO_BAND)
        return (MONS_PROGRAM_BUG);

    switch (band)
    {
    case BAND_KOBOLDS:
        mon_type = MONS_KOBOLD;
        break;

    case BAND_ORCS:
        mon_type = MONS_ORC;
        if (one_chance_in(6))
            mon_type = MONS_ORC_WIZARD;
        if (one_chance_in(8))
            mon_type = MONS_ORC_PRIEST;
        break;

    case BAND_ORC_WARRIOR:
        mon_type = MONS_ORC;
        if (one_chance_in(5))
            mon_type = MONS_ORC_WIZARD;
        if (one_chance_in(7))
            mon_type = MONS_ORC_PRIEST;
        break;

    case BAND_ORC_KNIGHT:
    case BAND_ORC_HIGH_PRIEST:
        // XXX: For Beogh punishment, ogres and trolls look out of place...
        // (For normal generation, they're okay, of course.)
        temp_rand = random2(30);
        mon_type = ((temp_rand > 17) ? MONS_ORC :          // 12 in 30
                    (temp_rand >  8) ? MONS_ORC_WARRIOR :  //  9 in 30
                    (temp_rand >  6) ? MONS_WARG :         //  2 in 30
                    (temp_rand >  4) ? MONS_ORC_WIZARD :   //  2 in 30
                    (temp_rand >  2) ? MONS_ORC_PRIEST :   //  2 in 30
                    (temp_rand >  1) ? MONS_OGRE :         //  1 in 30
                    (temp_rand >  0) ? MONS_TROLL          //  1 in 30
                                     : MONS_ORC_SORCERER); //  1 in 30
        break;

    case BAND_KILLER_BEES:
        mon_type = MONS_KILLER_BEE;
        break;

    case BAND_FLYING_SKULLS:
        mon_type = MONS_FLYING_SKULL;
        break;

    case BAND_SLIME_CREATURES:
        mon_type = MONS_SLIME_CREATURE;
        break;

    case BAND_YAKS:
        mon_type = MONS_YAK;
        break;

    case BAND_HARPIES:
        mon_type = MONS_HARPY;
        break;

    case BAND_UGLY_THINGS:
        mon_type = ((power > 21 && one_chance_in(4)) ?
                                MONS_VERY_UGLY_THING : MONS_UGLY_THING);
        break;

    case BAND_HELL_HOUNDS:
        mon_type = MONS_HELL_HOUND;
        break;

    case BAND_JACKALS:
        mon_type = MONS_JACKAL;
        break;

    case BAND_GNOLLS:
        mon_type = MONS_GNOLL;
        break;

    case BAND_BUMBLEBEES:
        mon_type = MONS_BUMBLEBEE;
        break;

    case BAND_CENTAURS:
        mon_type = MONS_CENTAUR;
        break;

    case BAND_YAKTAURS:
        mon_type = MONS_YAKTAUR;
        break;

    case BAND_INSUBSTANTIAL_WISPS:
        mon_type = MONS_INSUBSTANTIAL_WISP;
        break;

    case BAND_DEATH_YAKS:
        mon_type = MONS_DEATH_YAK;
        break;

    case BAND_NECROMANCER:                // necromancer
        temp_rand = random2(13);
        mon_type = ((temp_rand > 9) ? MONS_ZOMBIE_SMALL :   // 3 in 13
                    (temp_rand > 6) ? MONS_ZOMBIE_LARGE :   // 3 in 13
                    (temp_rand > 3) ? MONS_SKELETON_SMALL : // 3 in 13
                    (temp_rand > 0) ? MONS_SKELETON_LARGE   // 3 in 13
                                    : MONS_NECROPHAGE);     // 1 in 13
        break;

    case BAND_BALRUG:
        mon_type = (coinflip()? MONS_NEQOXEC : MONS_ORANGE_DEMON);
        break;

    case BAND_CACODEMON:
        mon_type = MONS_LEMURE;
        break;

    case BAND_EXECUTIONER:
        mon_type = (coinflip() ? MONS_ABOMINATION_SMALL
                               : MONS_ABOMINATION_LARGE);
        break;

    case BAND_PANDEMONIUM_DEMON:
        if (one_chance_in(7))
        {
            mon_type = static_cast<monster_type>(
                random_choose_weighted(50, MONS_LICH,
                                       10, MONS_ANCIENT_LICH,
                                       0));
        }
        else if (one_chance_in(6))
        {
            mon_type = static_cast<monster_type>(
                random_choose_weighted(50, MONS_ABOMINATION_SMALL,
                                       40, MONS_ABOMINATION_LARGE,
                                       10, MONS_TENTACLED_MONSTROSITY,
                                       0));
        }
        else
        {
            mon_type =
                summon_any_demon(
                    static_cast<demon_class_type>(
                        random_choose_weighted(50, DEMON_COMMON,
                                               20, DEMON_GREATER,
                                               10, DEMON_RANDOM,
                                               0)));
        }
        break;

    case BAND_HELLWING:
        mon_type = (coinflip() ? MONS_HELLWING : MONS_SMOKE_DEMON);
        break;

    case BAND_DEEP_ELF_FIGHTER:    // deep elf fighter
        temp_rand = random2(11);
        mon_type = ((temp_rand >  4) ? MONS_DEEP_ELF_SOLDIER : // 6 in 11
                    (temp_rand == 4) ? MONS_DEEP_ELF_FIGHTER : // 1 in 11
                    (temp_rand == 3) ? MONS_DEEP_ELF_KNIGHT :  // 1 in 11
                    (temp_rand == 2) ? MONS_DEEP_ELF_CONJURER :// 1 in 11
                    (temp_rand == 1) ? MONS_DEEP_ELF_MAGE      // 1 in 11
                                     : MONS_DEEP_ELF_PRIEST);  // 1 in 11
        break;

    case BAND_DEEP_ELF_KNIGHT:                    // deep elf knight
        temp_rand = random2(208);
        mon_type =
                ((temp_rand > 159) ? MONS_DEEP_ELF_SOLDIER :    // 23.08%
                 (temp_rand > 111) ? MONS_DEEP_ELF_FIGHTER :    // 23.08%
                 (temp_rand >  79) ? MONS_DEEP_ELF_KNIGHT :     // 15.38%
                 (temp_rand >  51) ? MONS_DEEP_ELF_MAGE :       // 13.46%
                 (temp_rand >  35) ? MONS_DEEP_ELF_PRIEST :     //  7.69%
                 (temp_rand >  19) ? MONS_DEEP_ELF_CONJURER :   //  7.69%
                 (temp_rand >   3) ? MONS_DEEP_ELF_SUMMONER :    // 7.69%
                 (temp_rand ==  3) ? MONS_DEEP_ELF_DEMONOLOGIST :// 0.48%
                 (temp_rand ==  2) ? MONS_DEEP_ELF_ANNIHILATOR : // 0.48%
                 (temp_rand ==  1) ? MONS_DEEP_ELF_SORCERER      // 0.48%
                                   : MONS_DEEP_ELF_DEATH_MAGE);  // 0.48%
        break;

    case BAND_DEEP_ELF_HIGH_PRIEST:                // deep elf high priest
        temp_rand = random2(16);
        mon_type =
                ((temp_rand > 12) ? MONS_DEEP_ELF_SOLDIER :     // 3 in 16
                 (temp_rand >  9) ? MONS_DEEP_ELF_FIGHTER :     // 3 in 16
                 (temp_rand >  6) ? MONS_DEEP_ELF_PRIEST :      // 3 in 16
                 (temp_rand == 6) ? MONS_DEEP_ELF_MAGE :        // 1 in 16
                 (temp_rand == 5) ? MONS_DEEP_ELF_SUMMONER :    // 1 in 16
                 (temp_rand == 4) ? MONS_DEEP_ELF_CONJURER :    // 1 in 16
                 (temp_rand == 3) ? MONS_DEEP_ELF_DEMONOLOGIST :// 1 in 16
                 (temp_rand == 2) ? MONS_DEEP_ELF_ANNIHILATOR : // 1 in 16
                 (temp_rand == 1) ? MONS_DEEP_ELF_SORCERER      // 1 in 16
                                  : MONS_DEEP_ELF_DEATH_MAGE);  // 1 in 16
        break;

    case BAND_HELL_KNIGHTS:
        mon_type = MONS_HELL_KNIGHT;
        if (one_chance_in(4))
            mon_type = MONS_NECROMANCER;
        break;

    case BAND_OGRE_MAGE:
        mon_type = MONS_OGRE;
        if (one_chance_in(3))
            mon_type = MONS_TWO_HEADED_OGRE;
        break;                  // ogre mage

    case BAND_KOBOLD_DEMONOLOGIST:
        temp_rand = random2(13);
        mon_type = ((temp_rand > 4) ? MONS_KOBOLD :             // 8 in 13
                    (temp_rand > 0) ? MONS_BIG_KOBOLD           // 4 in 13
                                    : MONS_KOBOLD_DEMONOLOGIST);// 1 in 13
        break;

    case BAND_NAGAS:
        mon_type = MONS_NAGA;
        break;
    case BAND_WAR_DOGS:
        mon_type = MONS_WAR_DOG;
        break;
    case BAND_GREY_RATS:
        mon_type = MONS_GREY_RAT;
        break;
    case BAND_GREEN_RATS:
        mon_type = MONS_GREEN_RAT;
        break;
    case BAND_ORANGE_RATS:
        mon_type = MONS_ORANGE_RAT;
        break;
    case BAND_SHEEP:
        mon_type = MONS_SHEEP;
        break;
    case BAND_GHOULS:
        mon_type = (coinflip() ? MONS_GHOUL : MONS_NECROPHAGE);
        break;
    case BAND_DEEP_TROLLS:
        mon_type = MONS_DEEP_TROLL;
        break;
    case BAND_HOGS:
        mon_type = MONS_HOG;
        break;
    case BAND_HELL_HOGS:
        mon_type = MONS_HELL_HOG;
        break;
    case BAND_GIANT_MOSQUITOES:
        mon_type = MONS_GIANT_MOSQUITO;
        break;
    case BAND_BOGGARTS:
        mon_type = MONS_BOGGART;
        break;
    case BAND_BLINK_FROGS:
        mon_type = MONS_BLINK_FROG;
        break;
    case BAND_SKELETAL_WARRIORS:
        mon_type = MONS_SKELETAL_WARRIOR;
        break;
    case BAND_DRACONIAN:
    {
        temp_rand = random2( (power < 24) ? 24 : 37 );
        mon_type =
                ((temp_rand > 35) ? MONS_DRACONIAN_CALLER :     // 1 in 34
                 (temp_rand > 33) ? MONS_DRACONIAN_KNIGHT :     // 2 in 34
                 (temp_rand > 31) ? MONS_DRACONIAN_MONK :       // 2 in 34
                 (temp_rand > 29) ? MONS_DRACONIAN_SHIFTER :    // 2 in 34
                 (temp_rand > 27) ? MONS_DRACONIAN_ANNIHILATOR :// 2 in 34
                 (temp_rand > 25) ? MONS_DRACONIAN_SCORCHER :   // 2 in 34
                 (temp_rand > 23) ? MONS_DRACONIAN_ZEALOT :     // 2 in 34
                 (temp_rand > 20) ? MONS_YELLOW_DRACONIAN :     // 3 in 34
                 (temp_rand > 17) ? MONS_GREEN_DRACONIAN :      // 3 in 34
                 (temp_rand > 14) ? MONS_BLACK_DRACONIAN :      // 3 in 34
                 (temp_rand > 11) ? MONS_WHITE_DRACONIAN :      // 3 in 34
                 (temp_rand >  8) ? MONS_PALE_DRACONIAN :       // 3 in 34
                 (temp_rand >  5) ? MONS_PURPLE_DRACONIAN :     // 3 in 34
                 (temp_rand >  2) ? MONS_MOTTLED_DRACONIAN :    // 3 in 34
                                    MONS_RED_DRACONIAN );       // 3 in 34
        break;
    }
    case BAND_ILSUIW:
        mon_type = coinflip()? MONS_MERFOLK : MONS_MERMAID;
        break;

    case BAND_AZRAEL:
        mon_type = coinflip()? MONS_FIRE_ELEMENTAL : MONS_HELL_HOUND;
        break;

    default:
        break;
    }

    return (mon_type);
}

static int _ood_limit()
{
    return Options.ood_interesting;
}

void mark_interesting_monst(struct monsters* monster, beh_type behaviour)
{
    if (crawl_state.arena)
        return;

    bool interesting = false;

    // Unique monsters are always intersting
    if ( mons_is_unique(monster->type) )
        interesting = true;
    // If it's never going to attack us, then not interesting
    else if (behaviour == BEH_FRIENDLY)
        interesting = false;
    else if (you.where_are_you == BRANCH_MAIN_DUNGEON
             && you.level_type == LEVEL_DUNGEON
             && mons_level(monster->type) >= you.your_level + _ood_limit()
             && mons_level(monster->type) < 99
             && !(monster->type >= MONS_EARTH_ELEMENTAL
                  && monster->type <= MONS_AIR_ELEMENTAL)
             && !mons_class_flag( monster->type, M_NO_EXP_GAIN ))
    {
        interesting = true;
    }
    else if ((you.level_type == LEVEL_DUNGEON
                || you.level_type == LEVEL_ABYSS)
             && mons_rarity(monster->type) <= Options.rare_interesting
             && monster->hit_dice > 2 // Don't note the really low-hd monsters.
             && mons_rarity(monster->type) > 0)
    {
        interesting = true;
    }
    // Don't waste time on moname() if user isn't using this option
    else if (Options.note_monsters.size() > 0)
    {
        const std::string iname = mons_type_name(monster->type, DESC_NOCAP_A);
        for (unsigned i = 0; i < Options.note_monsters.size(); ++i)
        {
            if (Options.note_monsters[i].matches(iname))
            {
                interesting = true;
                break;
            }
        }
    }

    if (interesting)
        monster->flags |= MF_INTERESTING;
}

// PUBLIC FUNCTION -- mons_place().

static monster_type _pick_zot_exit_defender()
{
    if (one_chance_in(11))
    {
#ifdef DEBUG_MON_CREATION
        mpr("Create a pandemonium demon!", MSGCH_DIAGNOSTICS);
#endif
        return (MONS_PANDEMONIUM_DEMON);
    }

    const int temp_rand = random2(276);
    const int mon_type =
        ((temp_rand > 184) ? MONS_WHITE_IMP + random2(15) :  // 33.33%
         (temp_rand > 104) ? MONS_HELLION + random2(10) :    // 28.99%
         (temp_rand > 78)  ? MONS_HELL_HOUND :               //  9.06%
         (temp_rand > 54)  ? MONS_ABOMINATION_LARGE :        //  8.70%
         (temp_rand > 33)  ? MONS_ABOMINATION_SMALL :        //  7.61%
         (temp_rand > 13)  ? MONS_RED_DEVIL                  //  7.25%
                           : MONS_PIT_FIEND);                //  5.07%

    return static_cast<monster_type>(mon_type);
}

int mons_place(mgen_data mg)
    // int mon_type, beh_type behaviour, int target, bool summoned,
    //             int px, int py, int level_type, proximity_type proximity,
    //             int extra, int dur, bool permit_bands )
{
#ifdef DEBUG_MON_CREATION
    mpr("in mons_place()", MSGCH_DIAGNOSTICS);
#endif
    int mon_count = 0;
    for (int il = 0; il < MAX_MONSTERS; il++)
        if (menv[il].type != -1)
            mon_count++;

    if (mg.cls == WANDERING_MONSTER)
    {
        if (mon_count > MAX_MONSTERS - 50)
            return (-1);

#ifdef DEBUG_MON_CREATION
        mpr("Set class RANDOM_MONSTER", MSGCH_DIAGNOSTICS);
#endif
        mg.cls = RANDOM_MONSTER;
    }

    // All monsters have been assigned? {dlb}
    if (mon_count >= MAX_MONSTERS - 1)
        return (-1);

    // This gives a slight challenge to the player as they ascend the
    // dungeon with the Orb.
    if (you.char_direction == GDT_ASCENDING && mg.cls == RANDOM_MONSTER
        && you.level_type == LEVEL_DUNGEON && !mg.summoned())
    {
#ifdef DEBUG_MON_CREATION
        mpr("Call _pick_zot_exit_defender()", MSGCH_DIAGNOSTICS);
#endif
        mg.cls    = _pick_zot_exit_defender();
        mg.flags |= MG_PERMIT_BANDS;
    }
    else if (mg.cls == RANDOM_MONSTER || mg.level_type == LEVEL_PANDEMONIUM)
        mg.flags |= MG_PERMIT_BANDS;

    // Translate level_type.
    switch (mg.level_type)
    {
    case LEVEL_PANDEMONIUM:
    case LEVEL_ABYSS:
        mg.power = level_id(mg.level_type).absdepth();
        break;
    case LEVEL_DUNGEON:
    default:
        mg.power = you.your_level;
        break;
    }

    int mid = place_monster(mg);
    if (mid == -1)
        return (-1);

    monsters *creation = &menv[mid];

    // Look at special cases: CHARMED, FRIENDLY, NEUTRAL, GOOD_NEUTRAL,
    // HOSTILE.
    if (mg.behaviour > NUM_BEHAVIOURS)
    {
        if (mg.behaviour == BEH_FRIENDLY)
            creation->flags |= MF_CREATED_FRIENDLY;

        if (mg.behaviour == BEH_NEUTRAL || mg.behaviour == BEH_GOOD_NEUTRAL)
            creation->flags |= MF_WAS_NEUTRAL;

        if (mg.behaviour == BEH_CHARMED)
        {
            creation->attitude = ATT_HOSTILE;
            creation->add_ench(ENCH_CHARM);
        }

        if (creation->type == MONS_RAKSHASA_FAKE && !one_chance_in(3))
            creation->add_ench(ENCH_INVIS);

        if (!(mg.flags & MG_FORCE_BEH) && !crawl_state.arena)
            player_angers_monster(creation);

        if (crawl_state.arena)
            behaviour_event(creation, ME_EVAL);
        else
            // Make summoned being aware of player's presence.
            behaviour_event(creation, ME_ALERT, MHITYOU);
    }

    return (mid);
}

static dungeon_feature_type _monster_primary_habitat_feature(int mc)
{
    if (mc == RANDOM_MONSTER)
        return (DNGN_FLOOR);
    return (habitat2grid(mons_class_primary_habitat(mc)));
}

static dungeon_feature_type _monster_secondary_habitat_feature(int mc)
{
    if (mc == RANDOM_MONSTER)
        return (DNGN_FLOOR);
    return (habitat2grid(mons_class_secondary_habitat(mc)));
}

class newmons_square_find : public travel_pathfind
{
private:
    dungeon_feature_type grid_wanted;
    coord_def start;
    int maxdistance;

    int best_distance;
    int nfound;
public:
    // Terrain that we can't spawn on, but that we can skip through.
    std::set<dungeon_feature_type> passable;
public:
    newmons_square_find(dungeon_feature_type grdw,
                        const coord_def &pos,
                        int maxdist = 0)
        :  grid_wanted(grdw), start(pos), maxdistance(maxdist),
           best_distance(0), nfound(0)
    {
    }

    coord_def pathfind()
    {
        set_floodseed(start);
        return travel_pathfind::pathfind(RMODE_EXPLORE);
    }

    bool path_flood(const coord_def &c, const coord_def &dc)
    {
        if (best_distance && traveled_distance > best_distance)
            return (true);

        if (!in_bounds(dc)
            || (maxdistance > 0 && traveled_distance > maxdistance))
        {
            return (false);
        }
        if (!grid_compatible(grid_wanted, grd(dc)))
        {
            if (passable.find(grd(dc)) != passable.end())
                good_square(dc);
            return (false);
        }
        if (mgrd(dc) == NON_MONSTER && dc != you.pos()
            && one_chance_in(++nfound))
        {
            greedy_dist = traveled_distance;
            greedy_place = dc;
            best_distance = traveled_distance;
        }
        else
        {
            good_square(dc);
        }
        return (false);
    }
};

// Finds a square for a monster of the given class, pathfinding
// through only contiguous squares of habitable terrain.
coord_def find_newmons_square_contiguous(monster_type mons_class,
                                         const coord_def &start,
                                         int distance)
{
    coord_def p;

    const dungeon_feature_type grid_preferred =
        _monster_primary_habitat_feature(mons_class);
    const dungeon_feature_type grid_nonpreferred =
        _monster_secondary_habitat_feature(mons_class);

    newmons_square_find nmpfind(grid_preferred, start, distance);
    const coord_def pp = nmpfind.pathfind();
    p = pp;

    if (grid_nonpreferred != grid_preferred && !in_bounds(pp))
    {
        newmons_square_find nmsfind(grid_nonpreferred, start, distance);
        const coord_def ps = nmsfind.pathfind();
        p = ps;
    }

    return (in_bounds(p) ? p : coord_def(-1, -1));
}

coord_def find_newmons_square(int mons_class, const coord_def &p)
{
    coord_def empty;
    coord_def pos(-1, -1);

    if (mons_class == WANDERING_MONSTER)
        mons_class = RANDOM_MONSTER;

    const dungeon_feature_type grid_preferred =
        _monster_primary_habitat_feature(mons_class);
    const dungeon_feature_type grid_nonpreferred =
        _monster_secondary_habitat_feature(mons_class);

    // Might be better if we chose a space and tried to match the monster
    // to it in the case of RANDOM_MONSTER, that way if the target square
    // is surrounded by water or lava this function would work.  -- bwr
    if (empty_surrounds(p, grid_preferred, 2, true, empty))
        pos = empty;

    if (grid_nonpreferred != grid_preferred && !in_bounds(pos)
        && empty_surrounds(p, grid_nonpreferred, 2, true, empty))
    {
        pos = empty;
    }

    return (pos);
}

bool player_will_anger_monster(monster_type type, bool *holy,
                               bool *unholy, bool *lawful,
                               bool *antimagical)
{
    monsters dummy;
    dummy.type = type;

    return (player_will_anger_monster(&dummy, holy, unholy, lawful,
                                      antimagical));
}

bool player_will_anger_monster(monsters *mon, bool *holy,
                               bool *unholy, bool *lawful,
                               bool *antimagical)
{
    const bool isHoly =
        (is_good_god(you.religion) && mons_is_evil_or_unholy(mon));
    const bool isUnholy =
        (is_evil_god(you.religion) && mons_is_holy(mon));
    const bool isLawful =
        (you.religion == GOD_ZIN && mons_is_chaotic(mon));
    const bool isAntimagical =
        (you.religion == GOD_TROG && mons_is_magic_user(mon));

    if (holy)
        *holy = isHoly;
    if (unholy)
        *unholy = isUnholy;
    if (lawful)
        *lawful = isLawful;
    if (antimagical)
        *antimagical = isAntimagical;

    return (isHoly || isUnholy || isLawful || isAntimagical);
}

bool player_angers_monster(monsters *mon)
{
    bool holy;
    bool unholy;
    bool lawful;
    bool antimagical;

    // Get the drawbacks, not the benefits... (to prevent e.g. demon-scumming).
    if (player_will_anger_monster(mon, &holy, &unholy, &lawful, &antimagical)
        && mons_wont_attack(mon))
    {
        mon->attitude = ATT_HOSTILE;
        mon->del_ench(ENCH_CHARM);
        behaviour_event(mon, ME_ALERT, MHITYOU);

        if (see_grid(mon->pos()) && player_monster_visible(mon))
        {
            std::string aura;

            if (holy)
                aura = "holy";
            else if (unholy)
                aura = "unholy";
            else if (lawful)
                aura = "lawful";
            else if (antimagical)
                aura = "anti-magical";

            mprf("%s is enraged by your %s aura!",
                 mon->name(DESC_CAP_THE).c_str(), aura.c_str());
        }

        return (true);
    }

    return (false);
}

int create_monster(mgen_data mg, bool fail_msg)
{
    const int montype = (mons_class_is_zombified(mg.cls) ? mg.base_type
                                                         : mg.cls);

    int summd = -1;

    if (!mg.force_place()
        || !in_bounds(mg.pos)
        || mgrd(mg.pos) != NON_MONSTER
        || mg.pos == you.pos()
        || !mons_class_can_pass(montype, grd(mg.pos)))
    {
        mg.pos = find_newmons_square(montype, mg.pos);
        // Gods other than Xom will try to avoid placing their monsters
        // directly in harm's way.
        if (mg.god != GOD_NO_GOD && mg.god != GOD_XOM)
        {
            monsters dummy;
            dummy.type         = mg.cls;
            dummy.base_monster = mg.base_type;
            dummy.god          = mg.god;

            int tries = 0;
            while (tries++ < 50
                   && (!in_bounds(mg.pos)
                       || mons_avoids_cloud(&dummy, env.cgrid(mg.pos),
                                            NULL, true)))
            {
                mg.pos = find_newmons_square(montype, mg.pos);
            }
            if (!in_bounds(mg.pos))
                return (-1);

            const int cloud_num = env.cgrid(mg.pos);
            // Don't place friendly god gift in a damaging cloud created by
            // you if that would anger the god.
            if (mons_avoids_cloud(&dummy, cloud_num, NULL, true)
                && mg.behaviour == BEH_FRIENDLY
                && god_hates_attacking_friend(you.religion, &dummy)
                && YOU_KILL(env.cloud[cloud_num].killer))
            {
                return (-1);
            }
        }
    }

    if (in_bounds(mg.pos))
    {
        summd = mons_place(mg);
        // If the arena vetoed the placement then give no fail message.
        if (crawl_state.arena)
            fail_msg = false;
    }

    // Determine whether creating a monster is successful (summd != -1) {dlb}:
    // then handle the outcome. {dlb}:
    if (fail_msg && summd == -1 && see_grid(mg.pos))
        mpr("You see a puff of smoke.");

    // The return value is either -1 (failure of some sort)
    // or the index of the monster placed (if I read things right). {dlb}
    return (summd);
}

bool empty_surrounds(const coord_def& where, dungeon_feature_type spc_wanted,
                     int radius, bool allow_centre, coord_def& empty)
{
    // Assume all player summoning originates from player x,y.
    bool playerSummon = (where == you.pos());

    int good_count = 0;

    for ( radius_iterator ri(where, radius, true, false, !allow_centre);
          ri; ++ri)
    {
        bool success = false;

        if ( *ri == you.pos() )
            continue;

        if (mgrd(*ri) != NON_MONSTER)
            continue;

        // Players won't summon out of LOS, or past transparent walls.
        if (!see_grid_no_trans(*ri) && playerSummon)
            continue;

        success =
            (grd(*ri) == spc_wanted) || grid_compatible(spc_wanted, grd(*ri));

        if (success && one_chance_in(++good_count))
            empty = *ri;
    }

    return (good_count > 0);
}

monster_type summon_any_demon(demon_class_type dct)
{
    monster_type mon = MONS_PROGRAM_BUG;

    if (dct == DEMON_RANDOM)
        dct = static_cast<demon_class_type>(random2(DEMON_RANDOM));

    int temp_rand;          // probability determination {dlb}

    switch (dct)
    {
    case DEMON_LESSER:
        temp_rand = random2(60);
        mon = ((temp_rand > 49) ? MONS_IMP :        // 10 in 60
               (temp_rand > 40) ? MONS_WHITE_IMP :  //  9 in 60
               (temp_rand > 31) ? MONS_LEMURE :     //  9 in 60
               (temp_rand > 22) ? MONS_UFETUBUS :   //  9 in 60
               (temp_rand > 13) ? MONS_MANES :      //  9 in 60
               (temp_rand > 4)  ? MONS_MIDGE        //  9 in 60
                                : MONS_SHADOW_IMP); //  5 in 60
        break;

    case DEMON_COMMON:
        temp_rand = random2(3948);
        mon = ((temp_rand > 3367) ? MONS_NEQOXEC :         // 14.69%
               (temp_rand > 2787) ? MONS_ORANGE_DEMON :    // 14.69%
               (temp_rand > 2207) ? MONS_HELLWING :        // 14.69%
               (temp_rand > 1627) ? MONS_SMOKE_DEMON :     // 14.69%
               (temp_rand > 1047) ? MONS_YNOXINUL :        // 14.69%
               (temp_rand > 889)  ? MONS_RED_DEVIL :       //  4.00%
               (temp_rand > 810)  ? MONS_HELLION :         //  2.00%
               (temp_rand > 731)  ? MONS_ROTTING_DEVIL :   //  2.00%
               (temp_rand > 652)  ? MONS_TORMENTOR :       //  2.00%
               (temp_rand > 573)  ? MONS_REAPER :          //  2.00%
               (temp_rand > 494)  ? MONS_SOUL_EATER :      //  2.00%
               (temp_rand > 415)  ? MONS_HAIRY_DEVIL :     //  2.00%
               (temp_rand > 336)  ? MONS_ICE_DEVIL :       //  2.00%
               (temp_rand > 257)  ? MONS_BLUE_DEVIL :      //  2.00%
               (temp_rand > 178)  ? MONS_BEAST :           //  2.00%
               (temp_rand > 99)   ? MONS_IRON_DEVIL :      //  2.00%
               (temp_rand > 49)   ? MONS_SUN_DEMON         //  1.26%
                                  : MONS_SHADOW_IMP);      //  1.26%
        break;

    case DEMON_GREATER:
        temp_rand = random2(1000);
        mon = ((temp_rand > 868) ? MONS_CACODEMON :        // 13.1%
               (temp_rand > 737) ? MONS_BALRUG :           // 13.1%
               (temp_rand > 606) ? MONS_BLUE_DEATH :       // 13.1%
               (temp_rand > 475) ? MONS_GREEN_DEATH :      // 13.1%
               (temp_rand > 344) ? MONS_EXECUTIONER :      // 13.1%
               (temp_rand > 244) ? MONS_FIEND :            // 10.0%
               (temp_rand > 154) ? MONS_ICE_FIEND :        //  9.0%
               (temp_rand > 73)  ? MONS_SHADOW_FIEND       //  8.1%
                                 : MONS_PIT_FIEND);        //  7.4%
        break;

    default:
        break;
    }

    return (mon);
}

monster_type summon_any_holy_being(holy_being_class_type hbct)
{
    monster_type mon = MONS_PROGRAM_BUG;

    switch (hbct)
    {
    case HOLY_BEING_WARRIOR:
        mon = coinflip() ? MONS_DAEVA : MONS_ANGEL;
        break;

    default:
        break;
    }

    return (mon);
}

monster_type summon_any_dragon(dragon_class_type dct)
{
    monster_type mon = MONS_PROGRAM_BUG;

    int temp_rand;

    switch (dct)
    {
    case DRAGON_LIZARD:
        temp_rand = random2(100);
        mon = ((temp_rand > 80) ? MONS_SWAMP_DRAKE :
               (temp_rand > 59) ? MONS_KOMODO_DRAGON :
               (temp_rand > 34) ? MONS_FIREDRAKE :
               (temp_rand > 11) ? MONS_DEATH_DRAKE :
                                  MONS_DRAGON);
        break;

    case DRAGON_DRACONIAN:
        temp_rand = random2(70);
        mon = ((temp_rand > 60) ? MONS_YELLOW_DRACONIAN :
               (temp_rand > 50) ? MONS_BLACK_DRACONIAN :
               (temp_rand > 40) ? MONS_PALE_DRACONIAN :
               (temp_rand > 30) ? MONS_GREEN_DRACONIAN :
               (temp_rand > 20) ? MONS_PURPLE_DRACONIAN :
               (temp_rand > 10) ? MONS_RED_DRACONIAN
                                : MONS_WHITE_DRACONIAN);
        break;

    case DRAGON_DRAGON:
        temp_rand = random2(90);
        mon = ((temp_rand > 80) ? MONS_MOTTLED_DRAGON :
               (temp_rand > 70) ? MONS_LINDWURM :
               (temp_rand > 60) ? MONS_STORM_DRAGON :
               (temp_rand > 50) ? MONS_MOTTLED_DRAGON :
               (temp_rand > 40) ? MONS_STEAM_DRAGON :
               (temp_rand > 30) ? MONS_DRAGON :
               (temp_rand > 20) ? MONS_ICE_DRAGON :
               (temp_rand > 10) ? MONS_SWAMP_DRAGON
                                : MONS_SHADOW_DRAGON);
        break;

    default:
        break;
    }

    return (mon);
}


/////////////////////////////////////////////////////////////////////////////
// monster_pathfind

// The pathfinding is an implementation of the A* algorithm. Beginning at the
// destination square we check all neighbours of a given grid, estimate the
// distance needed for any shortest path including this grid and push the
// result into a hash. We can then easily access all points with the shortest
// distance estimates and then check _their_ neighbours and so on.
// The algorithm terminates once we reach the monster position since - because
// of the sorting of grids by shortest distance in the hash - there can be no
// path between start and target that is shorter than the current one. There
// could be other paths that have the same length but that has no real impact.
// If the hash has been cleared and the start grid has not been encountered,
// then there's no path that matches the requirements fed into monster_pathfind.
// (These requirements are usually preference of habitat of a specific monster
// or a limit of the distance between start and any grid on the path.)

int mons_tracking_range(const monsters *mon)
{

    int range = 0;
    switch (mons_intel(mon))
    {
    case I_PLANT:
        range = 2;
        break;
    case I_INSECT:
        range = 4;
        break;
    case I_ANIMAL:
        range = 5;
        break;
    case I_NORMAL:
        range = LOS_RADIUS;
        break;
    default:
        // Highly intelligent monsters can find their way
        // anywhere. (range == 0 means no restriction.)
        break;
    }

    if (range)
    {
        if (mons_is_native_in_branch(mon))
            range += 3;
        else if (mons_class_flag(mon->type, M_BLOOD_SCENT))
            range++;
    }

    return (range);
}

//#define DEBUG_PATHFIND
monster_pathfind::monster_pathfind()
    : mons(), target(), range(0), min_length(0), max_length(0), dist(), prev()
{
}

monster_pathfind::~monster_pathfind()
{
}

void monster_pathfind::set_range(int r)
{
    if (r >= 0)
        range = r;
}

coord_def monster_pathfind::next_pos(const coord_def &c) const
{
    return c + Compass[prev[c.x][c.y]];
}

// The main method in the monster_pathfind class.
// Returns true if a path was found, else false.
bool monster_pathfind::init_pathfind(const monsters *mon, coord_def dest,
                                     bool diag, bool msg, bool pass_unmapped)
{
    mons   = mon;

    // We're doing a reverse search from target to monster.
    start  = dest;
    target = mon->pos();
    pos    = start;
    allow_diagonals   = diag;
    traverse_unmapped = pass_unmapped;

    // Easy enough. :P
    if (start == target)
    {
        if (msg)
            mpr("The monster is already there!");

        return (true);
    }

    return start_pathfind(msg);
}

bool monster_pathfind::init_pathfind(coord_def src, coord_def dest, bool diag,
                                     bool msg)
{
    start  = src;
    target = dest;
    pos    = start;
    allow_diagonals = diag;

    // Easy enough. :P
    if (start == target)
        return (true);

    return start_pathfind(msg);
}

bool monster_pathfind::start_pathfind(bool msg)
{
    // NOTE: We never do any traversable() check for the starting square
    //       (target). This means that even if the target cannot be reached
    //       we may still find a path leading adjacent to this position, which
    //       is desirable if e.g. the player is hovering over deep water
    //       surrounded by shallow water or floor, or if a foe is hiding in
    //       a wall.
    //       If the surrounding squares also are not traversable, we return
    //       early that no path could be found.

    max_length = min_length = grid_distance(pos.x, pos.y, target.x, target.y);
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
            dist[i][j] = INFINITE_DISTANCE;

    dist[pos.x][pos.y] = 0;

    bool success = false;
    do
    {
        // Calculate the distance to all neighbours of the current position,
        // and add them to the hash, if they haven't already been looked at.
        success = calc_path_to_neighbours();
        if (success)
            return (true);

        // Pull the position with shortest distance estimate to our target grid.
        success = get_best_position();

        if (!success)
        {
            if (msg)
            {
                mprf("Couldn't find a path from (%d,%d) to (%d,%d).",
                     target.x, target.y, start.x, start.y);
            }
            return (false);
        }
    }
    while (true);
}

// Returns true as soon as we encounter the target.
bool monster_pathfind::calc_path_to_neighbours()
{
    coord_def npos;
    int distance, old_dist, total;

    // For each point, we look at all neighbour points. Check the orthogonals
    // last, so that, should an orthogonal and a diagonal direction have the
    // same total travel cost, the orthogonal will be picked first, and thus
    // zigzagging will be significantly reduced.
    //
    //      1  0  3       This means directions are looked at, in order,
    //       \ | /        1, 3, 5, 7 (diagonals) followed by 0, 2, 4, 6
    //      6--.--2       (orthogonals). This is achieved by the assignment
    //       / | \        of (dir = 0) once dir has passed 7.
    //      7  4  5
    //
    for (int dir = 1; dir < 8; (dir += 2) == 9 && (dir = 0))
    {
        // Skip diagonal movement.
        if (!allow_diagonals && (dir % 2))
            continue;

        npos = pos + Compass[dir];

#ifdef DEBUG_PATHFIND
        mprf("Looking at neighbour (%d,%d)", npos.x, npos.y);
#endif
        if (!in_bounds(npos))
            continue;

        if (!traversable(npos))
            continue;

        // Ignore this grid if it takes us above the allowed distance.
        if (range && estimated_cost(npos) > range)
            continue;

        distance = dist[pos.x][pos.y] + travel_cost(npos);
        old_dist = dist[npos.x][npos.y];
#ifdef DEBUG_PATHFIND
        mprf("old dist: %d, new dist: %d, infinite: %d", old_dist, distance,
             INFINITE_DISTANCE);
#endif
        // If the new distance is better than the old one (initialized with
        // INFINITE), update the position.
        if (distance < old_dist)
        {
            // Calculate new total path length.
            total = distance + estimated_cost(npos);
            if (old_dist == INFINITE_DISTANCE)
            {
#ifdef DEBUG_PATHFIND
                mprf("Adding (%d,%d) to hash (total dist = %d)",
                     npos.x, npos.y, total);
#endif
                add_new_pos(npos, total);
                if (total > max_length)
                    max_length = total;
            }
            else
            {
#ifdef DEBUG_PATHFIND
                mprf("Improving (%d,%d) to total dist %d",
                     npos.x, npos.y, total);
#endif

                update_pos(npos, total);
            }

            // Update distance start->pos.
            dist[npos.x][npos.y] = distance;

            // Set backtracking information.
            // Converts the Compass direction to its counterpart.
            //      0  1  2         4  5  6
            //      7  .  3   ==>   3  .  7       e.g. (3 + 4) % 8          = 7
            //      6  5  4         2  1  0            (7 + 4) % 8 = 11 % 8 = 3

            prev[npos.x][npos.y] = (dir + 4) % 8;

            // Are we finished?
            if (npos == target)
            {
#ifdef DEBUG_PATHFIND
                mpr("Arrived at target.");
#endif
                return (true);
            }
        }
    }
    return (false);
}

// Starting at known min_length (minimum total estimated path distance), check
// the hash for existing vectors, then pick the last entry of the first vector
// that matches. Update min_length, if necessary.
bool monster_pathfind::get_best_position()
{
    for (int i = min_length; i <= max_length; i++)
    {
        if (!hash[i].empty())
        {
            if (i > min_length)
                min_length = i;

            std::vector<coord_def> &vec = hash[i];
            // Pick the last position pushed into the vector as it's most
            // likely to be close to the target.
            pos = vec[vec.size()-1];
            vec.pop_back();

#ifdef DEBUG_PATHFIND
            mprf("Returning (%d, %d) as best pos with total dist %d.",
                 pos.x, pos.y, min_length);
#endif

            return (true);
        }
#ifdef DEBUG_PATHFIND
        mprf("No positions for path length %d.", i);
#endif
    }

    // Nothing found? Then there's no path! :(
    return (false);
}

// Using the prev vector backtrack from start to target to find all steps to
// take along the shortest path.
std::vector<coord_def> monster_pathfind::backtrack()
{
#ifdef DEBUG_PATHFIND
    mpr("Backtracking...");
#endif
    std::vector<coord_def> path;
    pos = target;
    path.push_back(pos);

    if (pos == start)
        return path;

    int dir;
    do
    {
        dir = prev[pos.x][pos.y];
        pos = pos + Compass[dir];
        ASSERT(in_bounds(pos));
#ifdef DEBUG_PATHFIND
        mprf("prev: (%d, %d), pos: (%d, %d)", Compass[dir].x, Compass[dir].y,
                                              pos.x, pos.y);
#endif
        path.push_back(pos);

        if (pos.x == 0 && pos.y == 0)
            break;
    }
    while (pos != start);
    ASSERT(pos == start);

    return (path);
}

// Reduces the path coordinates to only a couple of key waypoints needed
// to reach the target. Waypoints are chosen such that from one waypoint you
// can see (and, more importantly, reach) the next one. Note that
// grid_see_grid() is probably rather too conservative in these estimates.
// This is done because Crawl's pathfinding - once a target is in sight and easy
// reach - is both very robust and natural, especially if we want to flexibly
// avoid plants and other monsters in the way.
std::vector<coord_def> monster_pathfind::calc_waypoints()
{
    std::vector<coord_def> path = backtrack();

    // If no path found, nothing to be done.
    if (path.empty())
        return path;

    dungeon_feature_type can_move;
    if (mons_amphibious(mons))
        can_move = DNGN_DEEP_WATER;
    else
        can_move = DNGN_SHALLOW_WATER;

    std::vector<coord_def> waypoints;
    pos = path[0];

#ifdef DEBUG_PATHFIND
    mpr(EOL "Waypoints:");
#endif
    for (unsigned int i = 1; i < path.size(); i++)
    {
        if (grid_see_grid(pos, path[i], can_move))
            continue;
        else
        {
            pos = path[i-1];
            waypoints.push_back(pos);
#ifdef DEBUG_PATHFIND
            mprf("waypoint: (%d, %d)", pos.x, pos.y);
#endif
        }
    }

    // Add the actual target to the list of waypoints, so we can later check
    // whether a tracked enemy has moved too much, in case we have to update
    // the path.
    if (pos != path[path.size() - 1])
        waypoints.push_back(path[path.size() - 1]);

    return (waypoints);
}

bool monster_pathfind::traversable(const coord_def p)
{
    if (traverse_unmapped && grd(p) == DNGN_UNSEEN)
        return (true);

    if (mons)
        return mons_traversable(p);

    return (!grid_is_solid(grd(p)) && !grid_destroys_items(grd(p)));
}

// Checks whether a given monster can pass over a certain position, respecting
// its preferred habit and capability of flight or opening doors.
bool monster_pathfind::mons_traversable(const coord_def p)
{
    const int montype = mons_is_zombified(mons) ? mons_zombie_base(mons)
                                                : mons->type;

    if (!monster_habitable_grid(montype, grd(p)))
        return (false);

    // Monsters that can't open doors won't be able to pass them.
    if (grd(p) == DNGN_CLOSED_DOOR || grd(p) == DNGN_SECRET_DOOR)
    {
        if (mons_is_zombified(mons))
        {
            if (mons_class_itemuse(montype) < MONUSE_OPEN_DOORS)
                return (false);
        }
        else if (mons_itemuse(mons) < MONUSE_OPEN_DOORS)
            return (false);
    }

    // Your friends only know about doors you know about, unless they feel
    // at home in this branch.
    if (grd(p) == DNGN_SECRET_DOOR && mons_friendly(mons)
        && (mons_intel(mons) < I_NORMAL
            || !mons_is_native_in_branch(mons)))
    {
        return (false);
    }

    const trap_def* ptrap = find_trap(p);
    if (ptrap)
    {
        const trap_type tt = ptrap->type;

        // Don't allow allies to pass over known (to them) Zot traps.
        if (tt == TRAP_ZOT
            && ptrap->is_known(mons)
            && mons_friendly(mons))
        {
            return (false);
        }

        // Monsters cannot travel over teleport traps.
        if (!_can_place_on_trap(montype, tt))
            return (false);
    }

    return (true);
}

int monster_pathfind::travel_cost(coord_def npos)
{
    if (mons)
        return mons_travel_cost(npos);

    return (1);
}

// Assumes that grids that really cannot be entered don't even get here.
// (Checked by traversable().)
int monster_pathfind::mons_travel_cost(coord_def npos)
{
    ASSERT(grid_distance(pos, npos) <= 1);

    // Doors need to be opened.
    if (grd(npos) == DNGN_CLOSED_DOOR || grd(npos) == DNGN_SECRET_DOOR)
        return 2;

    const int montype = mons_is_zombified(mons) ? mons_zombie_base(mons)
                                                : mons->type;

    const bool airborne = _mons_airborne(montype, -1, false);

    // Travelling through water, entering or leaving water is more expensive
    // for non-amphibious monsters, so they'll avoid it where possible.
    // (The resulting path might not be optimal but it will lead to a path
    // a monster of such habits is likely to prefer.)
    // Only tested for shallow water since they can't enter deep water anyway.
    if (!airborne && !mons_class_amphibious(montype)
        && (grd(pos) == DNGN_SHALLOW_WATER || grd(npos) == DNGN_SHALLOW_WATER))
    {
        return 2;
    }

    // Try to avoid (known) traps.
    const trap_def* ptrap = find_trap(npos);
    if (ptrap)
    {
        const bool knows_trap = ptrap->is_known(mons);
        const trap_type tt = ptrap->type;
        if (tt == TRAP_ALARM || tt == TRAP_ZOT)
        {
            // Your allies take extra precautions to avoid known alarm traps.
            // Zot traps are considered intraversable.
            if (knows_trap && mons_friendly(mons))
                return (3);

            // To hostile monsters, these traps are completely harmless.
            return 1;
        }

        // Mechanical traps can be avoided by flying, as can shafts, and
        // tele traps are never traversable anyway.
        if (knows_trap && !airborne)
            return 2;
    }

    return 1;
}

// The estimated cost to reach a grid is simply max(dx, dy).
int monster_pathfind::estimated_cost(coord_def p)
{
    return (grid_distance(p, target));
}

void monster_pathfind::add_new_pos(coord_def npos, int total)
{
    hash[total].push_back(npos);
}

void monster_pathfind::update_pos(coord_def npos, int total)
{
    // Find hash position of old distance and delete it,
    // then call_add_new_pos.
    int old_total = dist[npos.x][npos.y] + estimated_cost(npos);

    std::vector<coord_def> &vec = hash[old_total];
    for (unsigned int i = 0; i < vec.size(); i++)
    {
        if (vec[i] == npos)
        {
            vec.erase(vec.begin() + i);
            break;
        }
    }

    add_new_pos(npos, total);
}

/////////////////////////////////////////////////////////////////////////////
//
// Random monsters for portal vaults.
//
/////////////////////////////////////////////////////////////////////////////

void set_vault_mon_list(const std::vector<mons_spec> &list)
{
    CrawlHashTable &props = env.properties;

    props.erase(VAULT_MON_TYPES_KEY);
    props.erase(VAULT_MON_BASES_KEY);
    props.erase(VAULT_MON_WEIGHTS_KEY);

    unsigned int size = list.size();
    if (size == 0)
    {
        setup_vault_mon_list();
        return;
    }

    props[VAULT_MON_TYPES_KEY].new_vector(SV_LONG).resize(size);
    props[VAULT_MON_BASES_KEY].new_vector(SV_LONG).resize(size);
    props[VAULT_MON_WEIGHTS_KEY].new_vector(SV_LONG).resize(size);

    CrawlVector &type_vec   = props[VAULT_MON_TYPES_KEY];
    CrawlVector &base_vec   = props[VAULT_MON_BASES_KEY];
    CrawlVector &weight_vec = props[VAULT_MON_WEIGHTS_KEY];

    for (unsigned int i = 0; i < size; i++)
    {
        const mons_spec &spec = list[i];

        if (spec.place.is_valid())
        {
            ASSERT(spec.place.level_type != LEVEL_LABYRINTH
                   && spec.place.level_type != LEVEL_PORTAL_VAULT);
            type_vec[i] = (long) -1;
            base_vec[i] = (long) spec.place.packed_place();
        }
        else
        {
            ASSERT(spec.mid != RANDOM_MONSTER
                   && spec.monbase != RANDOM_MONSTER);
            type_vec[i] = (long) spec.mid;
            base_vec[i] = (long) spec.monbase;
        }
        weight_vec[i] = (long) spec.genweight;
    }

    setup_vault_mon_list();
}

void get_vault_mon_list(std::vector<mons_spec> &list)
{
    list.clear();

    CrawlHashTable &props = env.properties;

    if (!props.exists(VAULT_MON_TYPES_KEY))
        return;

    ASSERT(props.exists(VAULT_MON_BASES_KEY));
    ASSERT(props.exists(VAULT_MON_WEIGHTS_KEY));

    CrawlVector &type_vec   = props[VAULT_MON_TYPES_KEY];
    CrawlVector &base_vec   = props[VAULT_MON_BASES_KEY];
    CrawlVector &weight_vec = props[VAULT_MON_WEIGHTS_KEY];

    ASSERT(type_vec.size() == base_vec.size());
    ASSERT(type_vec.size() == weight_vec.size());

    unsigned int size = type_vec.size();
    for (unsigned int i = 0; i < size; i++)
    {
        int type = (long) type_vec[i];
        int base = (long) base_vec[i];

        mons_spec spec;

        if (type == -1)
        {
            spec.place = level_id::from_packed_place(base);
            ASSERT(spec.place.is_valid());
            ASSERT(spec.place.level_type != LEVEL_LABYRINTH
                   && spec.place.level_type != LEVEL_PORTAL_VAULT);
        }
        else
        {
            spec.mid     = type;
            spec.monbase = (monster_type) base;
            ASSERT(spec.mid != RANDOM_MONSTER
                   && spec.monbase != RANDOM_MONSTER);
        }
        spec.genweight = (long) weight_vec[i];

        list.push_back(spec);
    }
}

void setup_vault_mon_list()
{
    vault_mon_types.clear();
    vault_mon_bases.clear();
    vault_mon_weights.clear();

    std::vector<mons_spec> list;
    get_vault_mon_list(list);

    unsigned int size = list.size();

    vault_mon_types.resize(size);
    vault_mon_bases.resize(size);
    vault_mon_weights.resize(size);

    for (unsigned int i = 0; i < size; i++)
    {
        if (list[i].place.is_valid())
        {
            vault_mon_types[i] = -1;
            vault_mon_bases[i] = list[i].place.packed_place();
        }
        else
        {
            vault_mon_types[i] = list[i].mid;
            vault_mon_bases[i] = list[i].monbase;
        }
        vault_mon_weights[i] = list[i].genweight;
    }
}
