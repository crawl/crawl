/*
 *  File:       monplace.cc
 *  Summary:    Functions used when placing monsters in the dungeon.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include <algorithm>

#include "mon-place.h"
#include "mgen_data.h"

#include "arena.h"
#include "branch.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "dungeon.h"
#include "fprop.h"
#include "godabil.h"
#include "externs.h"
#include "options.h"
#include "ghost.h"
#include "lev-pand.h"
#include "libutil.h"
#include "message.h"
#include "mislead.h"
#include "mon-behv.h"
#include "mon-gear.h"
#include "mon-iter.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "mon-stuff.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "sprint.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#ifdef USE_TILE
#include "tiledef-player.h"
#endif

band_type active_monster_band = BAND_NO_BAND;

static std::vector<int> vault_mon_types;
static std::vector<int> vault_mon_bases;
static std::vector<int> vault_mon_weights;

#define VAULT_MON_TYPES_KEY   "vault_mon_types"
#define VAULT_MON_BASES_KEY   "vault_mon_bases"
#define VAULT_MON_WEIGHTS_KEY "vault_mon_weights"

// NEW place_monster -- note that power should be set to:
// DEPTH_ABYSS for abyss
// DEPTH_PAN for pandemonium
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
                                          int *lev_mons,
                                          bool *chose_ood_monster);

static void _define_zombie(monsters* mon, monster_type ztype, monster_type cs,
                           int power, coord_def pos);
static monster_type _band_member(band_type band, int power);
static band_type _choose_band(int mon_type, int power, int &band_size,
                              bool& natural_leader);
// static int _place_monster_aux(int mon_type, beh_type behaviour, int target,
//                               int px, int py, int power, int extra,
//                               bool first_band_member, int dur = 0);

static int _place_monster_aux(const mgen_data &mg, bool first_band_member,
                              bool force_pos = false);

// Returns whether actual_feat is compatible with feat_wanted for monster
// movement and generation.
bool feat_compatible(dungeon_feature_type feat_wanted,
                     dungeon_feature_type actual_feat)
{
    if (feat_wanted == DNGN_FLOOR)
    {
        return (actual_feat >= DNGN_FLOOR
                    && actual_feat != DNGN_BUILDER_SPECIAL_WALL
                || actual_feat == DNGN_SHALLOW_WATER);
    }

    if (feat_wanted >= DNGN_ROCK_WALL
        && feat_wanted <= DNGN_CLEAR_PERMAROCK_WALL)
    {
        // A monster can only move through or inhabit permanent rock if that's
        // exactly what it's asking for.
        if (actual_feat == DNGN_PERMAROCK_WALL
            || actual_feat == DNGN_CLEAR_PERMAROCK_WALL)
        {
            return (feat_wanted == DNGN_PERMAROCK_WALL
                    || feat_wanted == DNGN_CLEAR_PERMAROCK_WALL);
        }

        return (actual_feat >= DNGN_ROCK_WALL
                && actual_feat <= DNGN_CLEAR_PERMAROCK_WALL);
    }

    return (feat_wanted == actual_feat
            || (feat_wanted == DNGN_DEEP_WATER
                && (actual_feat == DNGN_SHALLOW_WATER
                    || actual_feat == DNGN_FOUNTAIN_BLUE)));
}

// Can this monster survive on actual_grid?
//
// If you have an actual monster, use this instead of the overloaded function
// that uses only the monster class to make decisions.
bool monster_habitable_grid(const monsters *m,
                            dungeon_feature_type actual_grid)
{
    // Zombified monsters enjoy the same habitat as their original.
    const monster_type montype = mons_is_zombified(m) ? mons_zombie_base(m)
                                                      : m->type;

    return (monster_habitable_grid(montype,
                                   actual_grid,
                                   DNGN_UNSEEN,
                                   mons_flies(m),
                                   m->cannot_move()));
}

bool mons_airborne(int mcls, int flies, bool paralysed)
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
bool monster_habitable_grid(monster_type montype,
                            dungeon_feature_type actual_grid,
                            dungeon_feature_type wanted_grid_feature,
                            int flies, bool paralysed)
{
    // No monster may be placed on open sea.
    if (actual_grid == DNGN_OPEN_SEA)
        return (false);

    const dungeon_feature_type feat_preferred =
        habitat2grid(mons_class_primary_habitat(montype));
    const dungeon_feature_type feat_nonpreferred =
        habitat2grid(mons_class_secondary_habitat(montype));

    const bool monster_is_airborne = mons_airborne(montype, flies, paralysed);

    // If the caller insists on a specific feature type, try to honour
    // the request. This allows the builder to place amphibious
    // creatures only on land, or flying creatures only on lava, etc.
    if (wanted_grid_feature != DNGN_UNSEEN
        && (feat_compatible(feat_preferred, wanted_grid_feature)
            || feat_compatible(feat_nonpreferred, wanted_grid_feature)
            || (monster_is_airborne && !feat_is_solid(wanted_grid_feature))))
    {
        return (feat_compatible(wanted_grid_feature, actual_grid));
    }

    // Special check for fire elementals since their habitat is floor which
    // is generally considered compatible with shallow water.
    if (montype == MONS_FIRE_ELEMENTAL && feat_is_watery(actual_grid))
        return (false);

    if (feat_compatible(feat_preferred, actual_grid)
        || (feat_nonpreferred != feat_preferred
            && feat_compatible(feat_nonpreferred, actual_grid)))
    {
        return (true);
    }

    // [dshaligram] Flying creatures are all DNGN_FLOOR, so we
    // only have to check for the additional valid grids of deep
    // water and lava.
    if (monster_is_airborne
        && (actual_grid == DNGN_LAVA || actual_grid == DNGN_DEEP_WATER))
    {
        return (true);
    }

    return (false);
}

// Returns true if the monster can submerge in the given grid.
bool monster_can_submerge(const monsters *mons, dungeon_feature_type feat)
{
    if (testbits(env.pgrid(mons->pos()), FPROP_NO_SUBMERGE))
        return (false);
    if (!mons->is_habitable_feat(feat))
        return (false);
    if (mons_class_flag(mons->type, M_SUBMERGES))
        switch (mons_habitat(mons))
        {
        case HT_WATER:
        case HT_AMPHIBIOUS:
            return (feat_is_watery(feat));
        case HT_LAVA:
            return (feat == DNGN_LAVA);
        case HT_LAND:
            // Currently, trapdoor spider only.
            return (feat_is_floor(feat));
        default:
            return (false);
        }
    else
        return (false);
}


bool is_spawn_scaled_area(const level_id &here)
{
    return (here.level_type == LEVEL_DUNGEON
            && !is_hell_subbranch(here.branch)
            && here.branch != BRANCH_HALL_OF_ZOT);
}

// Scale monster generation parameter with time spent on level. Note:
// (target_value - base_value) * dropoff_ramp_turns must be < INT_MAX!
static int _scale_spawn_parameter(int base_value,
                                  int target_value,
                                  int final_value,
                                  int dropoff_start_turns = 3000,
                                  int dropoff_ramp_turns  = 12000)
{
    if (!is_spawn_scaled_area(level_id::current()))
        return base_value;

    const int turns_on_level = env.turns_on_level;
    return (turns_on_level <= dropoff_start_turns ? base_value :
            turns_on_level > dropoff_start_turns + dropoff_ramp_turns ?
            final_value :

            // Actual scaling, strictly linear at the moment:
            (base_value +
             (target_value - base_value)
             * (turns_on_level - dropoff_start_turns)
             / dropoff_ramp_turns));
}

static bool _need_super_ood(int lev_mons)
{
    return (env.turns_on_level > 1400 - lev_mons * 117
            && x_chance_in_y(
                _scale_spawn_parameter(2, 10000, 10000, 3000, 9000),
                10000));
}

static int _fuzz_mons_level(int level)
{
    // Apply a fuzz to the monster level we're looking for. The fuzz
    // is intended to mix up monster generation producing moderately
    // OOD monsters, over and above the +5 OOD that's baked into the
    // monster selection loop.
    //
    // The OOD fuzz roll is not applied at level generation time on
    // D:1, and is applied slightly less often (0.75*0.14) on D:2. All
    // other levels have a straight 14% chance of moderate OOD fuzz
    // for each monster at level generation, and the chances of
    // moderate OODs go up to 100% after a ramp-up period.
    if ((level > 1
         // Give 25% chance of not trying for moderate OOD on D:2
         || (level == 1 && !one_chance_in(4))
         // Try moderate OOD after 700 turns on level on D:1, or 583 turns
         // spent on D:2.
         || env.turns_on_level > 700 - level * 117)
        && x_chance_in_y(
            _scale_spawn_parameter(140, 1000, 1000, 3000, 4800),
            1000))
    {
        const int fuzzspan = 5;
        const int fuzz = std::max(0, random_range(-fuzzspan, fuzzspan, 2));

#ifdef DEBUG_DIAGNOSTICS
        if (fuzz)
            dprf("Monster level fuzz: %d (old: %d, new: %d)",
                 fuzz, level, level + fuzz);
#endif
        return level + fuzz;
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
        viewwindow(false);
    }
}

//#define DEBUG_MON_CREATION

// This function is now only called about once every 5 turns. (Used to be
// every turn independent of how much time an action took, which was not ideal.)
// To arrive at spawning rates close to how they used to be, replace the
// one_chance_in(value) checks with the new x_chance_in_y(5, value). (jpeg)
void spawn_random_monsters()
{
    if (crawl_state.game_is_arena())
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
        _scale_spawn_parameter(env.spawn_random_rate,
                               6 * env.spawn_random_rate,
                               0)
        : 8;

    if (rate == 0)
    {
        dprf("random monster gen scaled off, %d turns on level",
             env.turns_on_level);
        return;
    }

    // Place normal dungeon monsters,  but not in player LOS.
    if (you.level_type == LEVEL_DUNGEON && x_chance_in_y(5, rate))
    {
        dprf("Placing monster, rate: %d, turns here: %d",
             rate, env.turns_on_level);
        proximity_type prox = (one_chance_in(10) ? PROX_NEAR_STAIRS
                                                 : PROX_AWAY_FROM_PLAYER);

        // The rules change once the player has picked up the Orb...
        if (you.char_direction == GDT_ASCENDING)
            prox = (one_chance_in(6) ? PROX_CLOSE_TO_PLAYER : PROX_ANYWHERE);

        mgen_data mg(WANDERING_MONSTER);
        mg.proximity = prox;
        mg.foe = (you.char_direction == GDT_ASCENDING) ? MHITYOU : MHITNOT;
        mons_place(mg);
        viewwindow(false);
        return;
    }

    // Place Abyss monsters. (Now happens regularly every 5 turns which might
    // look a bit strange for a place as chaotic as the Abyss. Then again,
    // the player is unlikely to meet all of them and notice this.)
    if (you.level_type == LEVEL_ABYSS
        && (you.char_direction != GDT_GAME_START
            || x_chance_in_y(5, rate))
        && (you.religion != GOD_CHEIBRIADOS || coinflip()))
    {
        mons_place(mgen_data(WANDERING_MONSTER));
        viewwindow(false);
        return;
    }

    // Place Pandemonium monsters.
    if (you.level_type == LEVEL_PANDEMONIUM && x_chance_in_y(5, rate))
    {
        pandemonium_mons();
        viewwindow(false);
        return;
    }

    // A portal vault *might* decide to turn on random monster spawning,
    // but it's off by default.
    if (you.level_type == LEVEL_PORTAL_VAULT && x_chance_in_y(5, rate))
    {
        mons_place(mgen_data(WANDERING_MONSTER));
        viewwindow(false);
    }

    // No random monsters in the Labyrinth.
}

monster_type pick_random_monster(const level_id &place,
                                 bool *chose_ood_monster)
{
    int level;
    if (place.level_type == LEVEL_PORTAL_VAULT)
        level = you.absdepth0;
    else
        level = place.absdepth();
    return pick_random_monster(place, level, level, chose_ood_monster);
}

std::vector<monster_type> _find_valid_monster_types(const level_id &place)
{
    static std::vector<monster_type> valid_monster_types;
    static level_id last_monster_type_place;

    if (last_monster_type_place == place)
        return (valid_monster_types);

    valid_monster_types.clear();
    for (int i = 0; i < NUM_MONSTERS; ++i)
        if (mons_rarity(static_cast<monster_type>(i), place) > 0)
            valid_monster_types.push_back(static_cast<monster_type>(i));
    last_monster_type_place = place;
    return (valid_monster_types);
}

monster_type pick_random_monster(const level_id &place, int power,
                                 int &lev_mons,
                                 bool *chose_ood_monster)
{
    bool ood_dummy = false;
    bool *isood = chose_ood_monster? chose_ood_monster : &ood_dummy;

    *isood = false;

    if (crawl_state.game_is_arena())
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
                                  dummy1, 0, &dummy2, &lev_mons,
                                  chose_ood_monster);

#if defined(DEBUG) || defined(DEBUG_DIAGNOSTICS)
        if (base_type != 0 && base_type != MONS_PROGRAM_BUG)
            mpr("Random portal vault mon discarding base type.",
                MSGCH_ERROR);
#endif
        return (type);
    }

    monster_type mon_type = MONS_PROGRAM_BUG;

    lev_mons = power;

    if (place == BRANCH_MAIN_DUNGEON
        && lev_mons != DEPTH_ABYSS && one_chance_in(4))
    {
        lev_mons = random2(power);
    }

    const int original_level = lev_mons;

    // OODs do not apply to the Abyss, Pan, etc.
    if (you.level_type == LEVEL_DUNGEON && lev_mons <= 27)
    {
        // Apply moderate OOD fuzz where appropriate.
        lev_mons = _fuzz_mons_level(lev_mons);

        // Potentially nasty surprise, but very rare.
        if (_need_super_ood(lev_mons))
        {
            const int new_level = lev_mons + random2avg(27, 2);
            dprf("Super OOD roll: Old: %d, New: %d", lev_mons, new_level);
            lev_mons = new_level;
        }

        lev_mons = std::min(30, lev_mons);
    }

    // Abyss or Pandemonium. Almost never called from Pan; probably only
    // if a random demon gets summon anything spell.
    if (lev_mons == DEPTH_ABYSS
        || place.level_type == LEVEL_PANDEMONIUM
        || place.level_type == LEVEL_ABYSS)
    {
        do
        {
            int count;

            do
            {
                count = 0;
                do
                {
                    // was: random2(400) {dlb}
                    mon_type = static_cast<monster_type>( random2(NUM_MONSTERS) );
                    count++;
                }
                while (mons_abyss(mon_type) == 0 && count < 2000);
            } while ((crawl_state.game_is_arena() &&
                      arena_veto_random_monster(mon_type)) ||
                     (crawl_state.game_is_sprint() &&
                      sprint_veto_random_abyss_monster(mon_type)));

            if (count == 2000)
                return (MONS_PROGRAM_BUG);
        }
        while (random2avg(100, 2) > mons_rare_abyss(mon_type)
               && !one_chance_in(100));
    }
    else
    {
        int level = 0, diff, chance;

        lev_mons = std::min(30, lev_mons);

        const int n_pick_tries   = 10000;
        const int n_relax_margin = n_pick_tries / 10;
        int monster_pick_tries = 10000;
        const std::vector<monster_type> valid_monster_types =
            _find_valid_monster_types(place);

        if (valid_monster_types.empty())
            return MONS_PROGRAM_BUG;

        while (monster_pick_tries-- > 0)
        {
            mon_type = valid_monster_types[random2(valid_monster_types.size())];

            if (crawl_state.game_is_arena() && arena_veto_random_monster(mon_type))
                continue;

            level  = mons_level(mon_type, place);
            diff   = level - lev_mons;

            // If we're running low on tries, ignore level differences.
            if (monster_pick_tries < n_relax_margin)
                diff = 0;

            chance = mons_rarity(mon_type, place) - (diff * diff);

            // If we're running low on tries, remove level restrictions.
            if ((monster_pick_tries < n_relax_margin
                 || std::abs(lev_mons - level) <= 5)
                && random2avg(100, 2) <= chance)
            {
                break;
            }
        }

        if (monster_pick_tries <= 0)
            return (MONS_PROGRAM_BUG);

        if (level > original_level + 5)
            *isood = true;
    }

#ifdef DEBUG_DIAGNOSTICS
    if (lev_mons > original_level)
        dprf("Orginal level: %d, Final level: %d, Monster: %s, OOD: %s",
             original_level, lev_mons,
             mon_type == MONS_NO_MONSTER || mon_type == MONS_PROGRAM_BUG ?
             "NONE" : get_monster_data(mon_type)->name,
             *isood? "YES" : "no");
#endif

    return (mon_type);
}

bool can_place_on_trap(int mon_type, trap_type trap)
{
    if (trap == TRAP_TELEPORT)
        return (false);

    if (trap == TRAP_SHAFT)
    {
        if (mon_type == RANDOM_MONSTER)
            return (false);

        return (mons_class_flies(mon_type) != FL_NONE
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
                                          int *lev_mons,
                                          bool *chose_ood_monster)
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
        mon_type = random_draconian_monster_species();
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

                if (actor_at(pos))
                    continue;

                // Is the grid verboten?
                if (!unforbidden( pos, mmask ))
                    continue;

                // Don't generate monsters on top of teleport traps.
                const trap_def* ptrap = find_trap(pos);
                if (ptrap && !can_place_on_trap(mon_type, ptrap->type))
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
            if (you.level_type == LEVEL_PORTAL_VAULT
                && vault_mon_types.size() > 0)
            {
                int i = choose_random_weighted(vault_mon_weights.begin(),
                                               vault_mon_weights.end());
                int type = vault_mon_types[i];
                int base = vault_mon_bases[i];

                if (type == -1)
                {
                    place = level_id::from_packed_place(base);
                    // If lev_mons is set to you.absdepth0, it was probably
                    // set as a default meaning "the current dungeon depth",
                    // which for a portal vault using its own definition
                    // of random monsters means "the depth of whatever place
                    // we're using for picking the random monster".
                    if (*lev_mons == you.absdepth0)
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
                                                  stair_type, lev_mons,
                                                  chose_ood_monster);
                    }
                    return (mon_type);
                }
            }
            else if (you.level_type == LEVEL_PORTAL_VAULT)
            {
                // XXX: We don't have a random monster list here, so pick one
                // from where we were.
                place.level_type = LEVEL_DUNGEON;
                *lev_mons = place.absdepth();
            }

            int tries = 0;
            while (tries++ < 300)
            {
                const int original_level = *lev_mons;
                // Now pick a monster of the given branch and level.
                mon_type = pick_random_monster(place, *lev_mons, *lev_mons,
                                               chose_ood_monster);

                // Don't allow monsters too stupid to use stairs (e.g.
                // non-spectral zombified undead) to be placed near
                // stairs.
                if (proximity != PROX_NEAR_STAIRS
                    || mons_class_can_use_stairs(mon_type))
                {
                    break;
                }
                *lev_mons = original_level;
            }

            if (proximity == PROX_NEAR_STAIRS && tries >= 300)
            {
                proximity = PROX_AWAY_FROM_PLAYER;

                // Reset target level.
                if (*stair_type == DCHAR_STAIRS_DOWN)
                    --*lev_mons;
                else if (*stair_type == DCHAR_STAIRS_UP)
                    ++*lev_mons;

                mon_type = pick_random_monster(place, *lev_mons, *lev_mons,
                                               chose_ood_monster);
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
            if (feat_is_stair(feat))
            {
                // Shouldn't matter for escape hatches.
                if (feat_is_escape_hatch(feat))
                    continue;

                // Should there be several stairs, don't overwrite the
                // player on stairs info.
                if (result < 2)
                    result = (p == you.pos()) ? 2 : 1;
            }
        }

    return (result);
}

// Checks if the monster is ok to place at mg_pos. If force_location
// is true, then we'll be less rigorous in our checks, in particular
// allowing land monsters to be placed in shallow water and water
// creatures in fountains.
static bool _valid_monster_generation_location( const mgen_data &mg,
                                                const coord_def &mg_pos)
{
    if (!in_bounds(mg_pos)
        || monster_at(mg_pos)
        || you.pos() == mg_pos && !fedhas_passthrough_class(mg.cls))
        return (false);

    const monster_type montype = (mons_class_is_zombified(mg.cls) ? mg.base_type
                                                                  : mg.cls);
    if (!monster_habitable_grid(montype, grd(mg_pos), mg.preferred_grid_feature,
                                mons_class_flies(montype), false)
        || (mg.behaviour != BEH_FRIENDLY && is_sanctuary(mg_pos)))
    {
        return (false);
    }

    // Check player proximity to avoid band members being placed
    // close to the player erroneously.
    // XXX: This is a little redundant with proximity checks in
    // place_monster.
    if (mg.proximity == PROX_AWAY_FROM_PLAYER
        && distance(you.pos(), mg_pos) <= LOS_RADIUS_SQ)
    {
        return (false);
    }

    // Don't generate monsters on top of teleport traps.
    // (How did they get there?)
    const trap_def* ptrap = find_trap(mg_pos);
    if (ptrap && !can_place_on_trap(mg.cls, ptrap->type))
        return (false);

    return (true);
}

static bool _valid_monster_generation_location(mgen_data &mg)
{
    return _valid_monster_generation_location(mg, mg.pos);
}

// Returns true if the player is on a level that should be sheltered from
// OOD packs, based on depth and time spent on-level.
static bool _in_ood_pack_protected_place()
{
    return (env.turns_on_level < 1400 - you.absdepth0 * 117);
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
    if (mg.use_position() && monster_at(mg.pos))
        return (-1);

    bool chose_ood_monster = false;
    mg.cls = _resolve_monster_type(mg.cls, mg.proximity, mg.base_type,
                                   mg.pos, mg.map_mask,
                                   &stair_type, &mg.power,
                                   &chose_ood_monster);

    if (mg.cls == MONS_NO_MONSTER || mg.cls == MONS_PROGRAM_BUG)
        return (-1);

    bool create_band = mg.permit_bands();
    // If we drew an OOD monster and there hasn't been much time spent
    // on level, disable band generation. This applies only to
    // randomly picked monsters -- chose_ood_monster will never be set
    // true for explicitly specified monsters in vaults and other
    // places.
    if (chose_ood_monster && _in_ood_pack_protected_place())
    {
        dprf("Chose monster with OOD roll: %s, disabling band generation",
             get_monster_data(mg.cls)->name);
        create_band = false;
    }

    // (3) Decide on banding (good lord!)
    int band_size = 1;
    bool leader = false;
    monster_type band_monsters[BIG_BAND];        // band monster types
    band_type band = BAND_NO_BAND;
    band_monsters[0] = mg.cls;

    // The (very) ugly thing band colour.
    static unsigned char ugly_colour = BLACK;

    if (create_band)
    {
#ifdef DEBUG_MON_CREATION
        mpr("Choose band members...", MSGCH_DIAGNOSTICS);
#endif
        band = _choose_band(mg.cls, mg.power, band_size, leader);
        band_size++;
        for (int i = 1; i < band_size; ++i)
        {
            band_monsters[i] = _band_member(band, mg.power);

            // Get the (very) ugly thing band colour, so that all (very)
            // ugly things in a band will start with it.
            if ((band_monsters[i] == MONS_UGLY_THING
                || band_monsters[i] == MONS_VERY_UGLY_THING)
                    && ugly_colour == BLACK)
            {
                ugly_colour = ugly_thing_random_colour();
            }
        }
    }

    // Set the (very) ugly thing band colour.
    if (ugly_colour != BLACK)
        mg.colour = ugly_colour;

    // Returns 2 if the monster is placed near player-occupied stairs.
    int pval = _is_near_stairs(mg.pos);
    if (mg.proximity == PROX_NEAR_STAIRS)
    {
        // For some cases disallow monsters on stairs.
        if (mons_class_is_stationary(mg.cls)
            || (pval == 2 // Stairs occupied by player.
                && (mons_class_base_speed(mg.cls) == 0
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

        // Try to pick a position that is
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

            if (!_valid_monster_generation_location(mg))
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
                // If this is supposed to measure los vs not los,
                // then see_cell(mg.pos) should be used instead. (jpeg)
                close_to_player = (distance(you.pos(), mg.pos) <=
                                   LOS_RADIUS_SQ);

                if (mg.proximity == PROX_CLOSE_TO_PLAYER && !close_to_player
                    || mg.proximity == PROX_AWAY_FROM_PLAYER && close_to_player)
                {
                    proxOK = false;
                }
                break;

            case PROX_NEAR_STAIRS:
                if (pval == 2) // player on stairs
                {
                    if (mons_class_base_speed(mg.cls) == 0)
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

                    // You can't be shoved if you're caught in a net.
                    if (you.caught())
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
    else if (!_valid_monster_generation_location(mg))
    {
        // Sanity check that the specified position is valid.
        return (-1);
    }

    id = _place_monster_aux(mg, true, force_pos);

    // Reset the (very) ugly thing band colour.
    if (ugly_colour != BLACK)
        ugly_colour = BLACK;

    // Bail out now if we failed.
    if (id == -1)
        return (-1);

    monsters *mon = &menv[id];
    if (mg.needs_patrol_point()
        || (mon->type == MONS_ALLIGATOR
            && !testbits(mon->flags, MF_BAND_MEMBER)))
    {
        mon->patrol_point = mon->pos();
#ifdef DEBUG_PATHFIND
        mprf("Monster %s is patrolling around (%d, %d).",
             mon->name(DESC_PLAIN).c_str(), mon->pos().x, mon->pos().y);
#endif
    }

    // Message to player from stairwell/gate appearance.
    if (you.see_cell(mg.pos) && mg.proximity == PROX_NEAR_STAIRS)
    {
        std::string msg;

        if (menv[id].visible_to(&you))
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
        viewwindow(false);
    }

    // Now, forget about banding if the first placement failed, or there are
    // too many monsters already, or we successfully placed by stairs.
    if (id >= MAX_MONSTERS - 30 || mg.proximity == PROX_NEAR_STAIRS)
        return (id);

    // Not PROX_NEAR_STAIRS, so it will be part of a band, if there is any.
    if (band_size > 1)
        menv[id].flags |= MF_BAND_MEMBER;

    const bool priest = mon->is_priest();

    mgen_data band_template = mg;

    if (leader && !mg.summoner)
    {
        band_template.summoner = &menv[id];
        band_template.flags |= MG_BAND_MINION;
    }

    unwind_var<band_type> current_band(active_monster_band, band);
    // (5) For each band monster, loop call to place_monster_aux().
    for (int i = 1; i < band_size; i++)
    {
        if (band_monsters[i] == MONS_NO_MONSTER)
            break;

        band_template.cls = band_monsters[i];

        // We don't want to place a unique that has already been
        // generated.
        if (mons_is_unique(band_template.cls)
            && you.unique_creatures[band_template.cls])
        {
            continue;
        }

        const int band_id = _place_monster_aux(band_template, false);
        if (band_id != -1 && band_id != NON_MONSTER)
        {
            menv[band_id].flags |= MF_BAND_MEMBER;
            // Priestly band leaders should have an entourage of the
            // same religion.
            if (priest)
                menv[band_id].god = mon->god;

            if (mon->type == MONS_PIKEL)
            {
                // Don't give XP for the slaves to discourage hunting.  Pikel
                // has an artificially large XP modifier to compensate for
                // this.
                menv[band_id].flags |= MF_NO_REWARD;
                menv[band_id].props["pikel_band"] = true;
            }
        }
    }

    // Placement of first monster, at least, was a success.
    return (id);
}

monsters* get_free_monster()
{
    for (int i = 0; i < MAX_MONSTERS; ++i)
        if (env.mons[i].type == MONS_NO_MONSTER)
        {
            env.mons[i].reset();
            return (&env.mons[i]);
        }

    return (NULL);
}

void mons_add_blame(monsters *mon, const std::string &blame_string)
{
    const bool exists = mon->props.exists("blame");
    CrawlStoreValue& blame = mon->props["blame"];
    if (!exists)
        blame.new_vector(SV_STR, SFLAG_CONST_TYPE);
    blame.get_vector().push_back(blame_string);
}

#ifdef USE_TILE
// For some tiles, always use the fixed same variant out of a set
// of tiles. (Where this is not handled by number or colour already.)
static void _maybe_init_tilenum_props(monsters *mon)
{
    // Not necessary.
    if (mon->props.exists("monster_tile") || mon->props.exists("tile_num"))
        return;

    // Special-case for monsters masquerading as items.
    if (mon->type == MONS_DANCING_WEAPON || mons_is_mimic(mon->type))
        return;

    // Only add the property for tiles that have several variants.
    const int base_tile = tileidx_monster_base(mon);
    if (tile_player_count(base_tile) > 1)
        mon->props["tile_num"] = short(random2(256));
}
#endif

static int _place_monster_aux(const mgen_data &mg,
                              bool first_band_member, bool force_pos)
{
    coord_def fpos;

    // Some sanity checks.
    if (mons_is_unique(mg.cls) && you.unique_creatures[mg.cls]
        || mg.cls == MONS_MERGED_SLIME_CREATURE)
    {
        ASSERT(false);
        return (-1);
    }

    const monsterentry *m_ent = get_monster_data(mg.cls);

    monsters* mon = get_free_monster();
    if (!mon)
        return (-1);

    const monster_type montype = (mons_class_is_zombified(mg.cls) ? mg.base_type
                                                                  : mg.cls);

    // Setup habitat and placement.
    // If the space is occupied, try some neighbouring square instead.
    if (first_band_member && in_bounds(mg.pos)
        && (mg.behaviour == BEH_FRIENDLY || !is_sanctuary(mg.pos))
        && !monster_at(mg.pos)
        && (you.pos() != mg.pos || fedhas_passthrough_class(mg.cls))
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
            fpos = mg.pos + coord_def(random_range(-3, 3),
                                      random_range(-3, 3));

            if (_valid_monster_generation_location(mg, fpos))
                break;
        }

        // Did we really try 1000 times?
        if (i == 1000)
            return (-1);
    }

    ASSERT(!monster_at(fpos));

    if (crawl_state.game_is_arena()
        && arena_veto_place_monster(mg, first_band_member, fpos))
    {
        return (-1);
    }

    // Now, actually create the monster. (Wheeee!)
    mon->type         = mg.cls;
    mon->base_monster = mg.base_type;
    mon->number       = mg.number;

    // Set pos and link monster into monster grid.
    if (!mon->move_to_pos(fpos))
    {
        mon->reset();
        return (-1);
    }

    if (mons_is_mimic(mg.cls))
    {
        // Mimics who mimic thin air get the axe.
        if (!give_mimic_item(mon))
        {
            mon->reset();
            mgrd(fpos) = NON_MONSTER;
            return (-1);
        }
    }

    // Generate a brand shiny new monster, or zombie.
    if (mons_class_is_zombified(mg.cls))
        _define_zombie(mon, mg.base_type, mg.cls, mg.power, fpos);
    else
        define_monster(mon);

    // Is it a god gift?
    if (mg.god != GOD_NO_GOD)
    {
        mon->god    = mg.god;
        mon->flags |= MF_GOD_GIFT;
    }
    // Not a god gift, give priestly monsters a god.
    else if (mons_class_flag(mg.cls, M_PRIEST))
    {
        if (mg.cls == MONS_WAYNE)
            mon->god = GOD_OKAWARU;
        else
        {
            switch (mons_genus(mg.cls))
            {
            case MONS_ORC:
                mon->god = GOD_BEOGH;
                break;
            case MONS_JELLY:
                mon->god = GOD_JIYVA;
                break;
            case MONS_MUMMY:
            case MONS_DRACONIAN:
            case MONS_ELF:
                // [ds] Vault defs can request priest monsters of unusual types.
            default:
                mon->god = GOD_NAMELESS;
                break;
            }
        }
    }
    // 1 out of 7 non-priestly orcs are unbelievers.
    else if (mons_genus(mg.cls) == MONS_ORC)
    {
        if (!one_chance_in(7))
            mon->god = GOD_BEOGH;
    }
    // The royal jelly belongs to Jiyva.
    else if (mg.cls == MONS_ROYAL_JELLY)
        mon->god = GOD_JIYVA;
    // Angels and Daevas belong to TSO, but 1 out of 7 in the Abyss are
    // adopted by Xom.
    else if (mons_class_holiness(mg.cls) == MH_HOLY)
    {
        if (mg.level_type != LEVEL_ABYSS || !one_chance_in(7))
            mon->god = GOD_SHINING_ONE;
        else
            mon->god = GOD_XOM;
    }
    // 6 out of 7 demons in the Abyss belong to Lugonu.
    else if (mons_class_holiness(mg.cls) == MH_DEMONIC)
    {
        if (mg.level_type == LEVEL_ABYSS && !one_chance_in(7))
            mon->god = GOD_LUGONU;
    }

    // If the caller requested a specific colour for this monster, apply
    // it now.
    if (mg.colour != BLACK)
        mon->colour = mg.colour;

    if (mg.mname != "")
        mon->mname = mg.mname;

    if (mg.hd != 0)
    {
        mon->hit_dice = mg.hd;
        // Re-roll HP.
        int hp = hit_points(mg.hd, m_ent->hpdice[1], m_ent->hpdice[2]);
        // But only for monsters with random HP.
        if (hp > 0)
        {
            mon->max_hit_points = hp;
            mon->hit_points = hp;
        }
    }

    if (mg.hp != 0)
    {
        mon->max_hit_points = mg.hp;
        mon->hit_points = mg.hp;
    }

    // Store the extra flags here.
    mon->flags       |= mg.extra_flags;

    // The return of Boris is now handled in monster_die().  Not setting
    // this for Boris here allows for multiple Borises in the dungeon at
    // the same time. - bwr
    if (mons_is_unique(mg.cls))
        you.unique_creatures[mg.cls] = true;

    if (mons_class_flag(mg.cls, M_INVIS))
        mon->add_ench(ENCH_INVIS);

    if (mons_class_flag(mg.cls, M_CONFUSED))
        mon->add_ench(ENCH_CONFUSION);

    if (mg.cls == MONS_SHAPESHIFTER)
        mon->add_ench(ENCH_SHAPESHIFTER);

    if (mg.cls == MONS_GLOWING_SHAPESHIFTER)
        mon->add_ench(ENCH_GLOWING_SHAPESHIFTER);

    if (mg.cls == MONS_TOADSTOOL)
    {
        // This enchantment is a timer that counts down until death.
        // It should last longer than the lifespan of a corpse, to avoid
        // spawning mushrooms in the same place over and over.  Aside
        // from that, the value is slightly randomised to avoid
        // simultaneous die-offs of mushroom rings.
        mon->add_ench(ENCH_SLOWLY_DYING);
    }
    else if (mg.cls == MONS_HYPERACTIVE_BALLISTOMYCETE)
    {
        mon->add_ench(ENCH_EXPLODING);
    }

    if (!crawl_state.game_is_arena() && you.misled())
        update_mislead_monster(mon);

    if (monster_can_submerge(mon, grd(fpos)) && !one_chance_in(5))
        mon->add_ench(ENCH_SUBMERGED);

    mon->flags |= MF_JUST_SUMMONED;

    // Don't leave shifters in their starting shape.
    if (mg.cls == MONS_SHAPESHIFTER || mg.cls == MONS_GLOWING_SHAPESHIFTER)
    {
        no_messages nm;
        monster_polymorph(mon, RANDOM_MONSTER);

        // It's not actually a known shapeshifter if it happened to be
        // placed in LOS of the player.
        mon->flags &= ~MF_KNOWN_MIMIC;
    }

    // dur should always be 1-6 for monsters that can be abjured.
    const bool summoned = mg.abjuration_duration >= 1
                       && mg.abjuration_duration <= 6;

    if (mg.cls == MONS_DANCING_WEAPON)
    {
        give_item(mon->mindex(), mg.power, summoned);

        // Dancing weapons *always* have a weapon. Fail to create them
        // otherwise.
        const item_def* wpn = mon->mslot_item(MSLOT_WEAPON);
        if (!wpn)
        {
            mon->destroy_inventory();
            mon->reset();
            mgrd(fpos) = NON_MONSTER;
            return (-1);
        }
        else
            mon->colour = wpn->colour;
    }
    else if (mons_class_itemuse(mg.cls) >= MONUSE_STARTING_EQUIPMENT)
    {
        give_item(mon->mindex(), mg.power, summoned);
        // Give these monsters a second weapon. - bwr
        if (mons_class_wields_two_weapons(mg.cls))
            give_item(mon->mindex(), mg.power, summoned);

        unwind_var<int> save_speedinc(mon->speed_increment);
        mon->wield_melee_weapon(false);
    }

    if (mg.cls == MONS_SLIME_CREATURE)
    {
        if (mg.number > 1)
        {
            // Boost HP to what it would have been if it had grown this
            // big by merging.
            mon->hit_points     *= mg.number;
            mon->max_hit_points *= mg.number;
        }
    }

    // Set attitude, behaviour and target.
    mon->attitude  = ATT_HOSTILE;
    mon->behaviour = mg.behaviour;

    // Statues cannot sleep (nor wander but it means they are a bit
    // more aware of the player than they'd be otherwise).
    if (mons_is_statue(mg.cls))
        mon->behaviour = BEH_WANDER;

    mon->foe_memory = 0;

    // Setting attitude will always make the monster wander...
    // If you want sleeping hostiles, use BEH_SLEEP since the default
    // attitude is hostile.
    if (mg.behaviour > NUM_BEHAVIOURS)
    {
        if (mg.behaviour == BEH_FRIENDLY)
            mon->attitude = ATT_FRIENDLY;

        if (mg.behaviour == BEH_GOOD_NEUTRAL)
            mon->attitude = ATT_GOOD_NEUTRAL;

        if (mg.behaviour == BEH_NEUTRAL)
            mon->attitude = ATT_NEUTRAL;

        if (mg.behaviour == BEH_STRICT_NEUTRAL)
            mon->attitude = ATT_STRICT_NEUTRAL;

        mon->behaviour = BEH_WANDER;
    }

    if (summoned)
    {
        mon->mark_summoned(mg.abjuration_duration, true,
                           mg.summon_type);
    }
    mon->foe = mg.foe;

    std::string blame_prefix;

    if (mg.flags & MG_BAND_MINION)
    {
        blame_prefix = "led by ";
    }
    else if (mg.abjuration_duration > 0)
    {
        blame_prefix = "summoned by ";

        if (mg.summoner != NULL && mg.summoner->alive()
            && mg.summoner->atype() == ACT_MONSTER
            && static_cast<const monsters*>(mg.summoner)->type == MONS_MARA)
        {
                blame_prefix = "woven by ";
        }
    }
    else if (mons_class_is_zombified(mg.cls))
    {
        blame_prefix = "animated by ";
    }
    else
    {
        blame_prefix = "created by ";
    }

    if (!mg.non_actor_summoner.empty())
    {
        mons_add_blame(mon, blame_prefix + mg.non_actor_summoner);
    }
    // NOTE: The summoner might be dead if the summoned is placed by a
    // beam which killed the summoner first (like fire vortexes placed
    // by the Fire Storm spell).
    else if (mg.summoner != NULL && mg.summoner->alive())
    {
        ASSERT(mg.summoner->alive());
        if (mg.summoner->atype() == ACT_PLAYER)
        {
            mons_add_blame(mon, blame_prefix + "the player character");
        }
        else
        {
            monsters* sum = mg.summoner->as_monster();
            mons_add_blame(mon, (blame_prefix
                                 + sum->full_name(DESC_NOCAP_A, true)));
            if (sum->props.exists("blame"))
            {
                CrawlVector& oldblame = sum->props["blame"].get_vector();
                for (CrawlVector::iterator i = oldblame.begin();
                     i != oldblame.end(); ++i)
                {
                    mons_add_blame(mon, *i);
                }
            }
        }
    }

    // Initialise (very) ugly things and pandemonium demons.
    if (mon->type == MONS_UGLY_THING
        || mon->type == MONS_VERY_UGLY_THING)
    {
        ghost_demon ghost;
        ghost.init_ugly_thing(mon->type == MONS_VERY_UGLY_THING, false,
                              mg.colour);
        mon->set_ghost(ghost, false);
        mon->uglything_init();
    }
    else if (mon->type == MONS_DANCING_WEAPON)
    {
        ghost_demon ghost;
        // We can't use monsters::weapon here because it wants to look
        // at attack types, which are in the ghost structure we're
        // building.
        ASSERT(mon->mslot_item(MSLOT_WEAPON));
        // Dancing weapons are placed at pretty high power.  Remember, the
        // player is fighting them one-on-one, while he will often summon
        // several.
        ghost.init_dancing_weapon(*(mon->mslot_item(MSLOT_WEAPON)), 180);
        mon->set_ghost(ghost);
        mon->dancing_weapon_init();
    }

#ifdef USE_TILE
    _maybe_init_tilenum_props(mon);
#endif

    mark_interesting_monst(mon, mg.behaviour);

    if (!Generating_Level && you.can_see(mon))
        handle_seen_interrupt(mon);

    if (crawl_state.game_is_arena())
        arena_placed_monster(mon);

    return (mon->mindex());
}

monster_type pick_random_zombie()
{
    static std::vector<monster_type> zombifiable;
    if (zombifiable.empty())
    {
        for (int i = 0; i < NUM_MONSTERS; ++i)
        {
            if (mons_species(i) != i || i == MONS_PROGRAM_BUG)
                continue;

            const monster_type mcls = static_cast<monster_type>(i);
            if (!mons_zombie_size(mcls) || mons_is_unique(mcls))
                continue;

            zombifiable.push_back(mcls);
        }
    }
    return (zombifiable[random2(zombifiable.size())]);
}

// Size based on zombie class.
static zombie_size_type _zombie_class_size(monster_type cs)
{
    switch (cs)
    {
        case MONS_ZOMBIE_SMALL:
        case MONS_SIMULACRUM_SMALL:
        case MONS_SKELETON_SMALL:
            return (Z_SMALL);
        case MONS_ZOMBIE_LARGE:
        case MONS_SIMULACRUM_LARGE:
        case MONS_SKELETON_LARGE:
            return (Z_BIG);
        case MONS_SPECTRAL_THING:
            return (Z_NOZOMBIE);
        default:
            ASSERT(false);
            return (Z_NOZOMBIE);
    }
}

// Check base monster class against zombie type and position
// if set.
static bool _good_zombie(monster_type base, monster_type cs,
                         const coord_def& pos)
{
    // Actually pick a monster that is happy where we want to put it.
    // Fish zombies on land are helpless and uncool.
    if (in_bounds(pos) && !monster_habitable_grid(base, grd(pos)))
        return (false);

    if (cs == MONS_NO_MONSTER)
        return (true);

    // If skeleton, monster must have a skeleton.
    if ((cs == MONS_SKELETON_SMALL || cs == MONS_SKELETON_LARGE)
        && !mons_skeleton(base))
    {
        return (false);
    }

    // Size must match, but you can make a spectral thing out
    // of anything.
    if (cs != MONS_SPECTRAL_THING
        && mons_zombie_size(base) != _zombie_class_size(cs))
    {
        return (false);
    }

    return (true);
}

monster_type pick_local_zombifiable_monster(int power, bool hack_hd,
                                            monster_type cs,
                                            const coord_def& pos)
{
    bool ignore_rarity = false;
    power = std::min(27, power);

    // How OOD this zombie can be.
    int relax = 5;

    // Pick an appropriate creature to make a zombie out of,
    // levelwise.  The old code was generating absolutely
    // incredible OOD zombies.
    while (true)
    {
        monster_type base = pick_random_zombie();

        // On certain branches, zombie creation will fail if we use
        // the mons_rarity() functions, because (for example) there
        // are NO zombifiable "native" abyss creatures. Other branches
        // where this is a problem are hell levels and the crypt.
        // we have to watch for summoned zombies on other levels, too,
        // such as the Temple, HoB, and Slime Pits.
        if (you.level_type != LEVEL_DUNGEON
            || player_in_hell()
            || player_in_branch(BRANCH_HALL_OF_ZOT)
            || player_in_branch(BRANCH_VESTIBULE_OF_HELL)
            || player_in_branch(BRANCH_ECUMENICAL_TEMPLE)
            || player_in_branch(BRANCH_CRYPT)
            || player_in_branch(BRANCH_TOMB)
            || player_in_branch(BRANCH_HALL_OF_BLADES)
            || player_in_branch(BRANCH_SNAKE_PIT)
            || player_in_branch(BRANCH_SLIME_PITS)
            || one_chance_in(1000))
        {
            ignore_rarity = true;
        }

        // Don't make out-of-rarity zombies when we don't have to.
        if (!ignore_rarity && mons_rarity(base) == 0)
            continue;

        // Does the zombie match the parameters?
        if (!_good_zombie(base, cs, pos))
            continue;

        // Hack -- non-dungeon zombies are always made out of nastier
        // monsters.
        if (hack_hd && you.level_type != LEVEL_DUNGEON && mons_power(base) > 8)
            return (base);

        // Check for rarity.. and OOD - identical to mons_place()
        int level, diff, chance;

        level = mons_level(base) - 4;
        diff  = level - power;

        chance = (ignore_rarity) ? 100
                                 : mons_rarity(base) - (diff * diff) / 2;

        if (power > level - relax && power < level + relax
            && random2avg(100, 2) <= chance)
        {
            return (base);
        }

        // Every so often, we'll relax the OOD restrictions.  Avoids
        // infinite loops (if we don't do this, things like creating
        // a large skeleton on level 1 may hang the game!).
        if (one_chance_in(5))
            relax++;
    }
}

static void _define_zombie(monsters* mon, monster_type ztype, monster_type cs,
                           int power, coord_def pos)
{
    ASSERT(mons_class_is_zombified(cs));

    monster_type base;
    if (ztype == MONS_NO_MONSTER)
        base = pick_local_zombifiable_monster(power, true, cs, pos);
    else
        base = mons_species(ztype);

    ASSERT(_zombie_class_size(cs) == Z_NOZOMBIE
           || _zombie_class_size(cs) == mons_zombie_size(base));

    // Set type to the base type to calculate appropriate stats.
    mon->type = base;
    define_monster(mon);

    mon->type         = cs;
    mon->base_monster = base;

    mon->colour       = mons_class_colour(cs);
    mon->speed        = mons_class_zombie_base_speed(mon->base_monster);

    // Turn off all spellcasting flags.
    // Hack - kraken get to keep their spell-like ability.
    if (mon->base_monster != MONS_KRAKEN)
        mon->flags   &= ~MF_SPELLCASTER & ~MF_ACTUAL_SPELLS & ~MF_PRIEST;

    int hp = 0;
    int acmod = 0, evmod = 0;
    switch (cs)
    {
    case MONS_ZOMBIE_SMALL:
    case MONS_ZOMBIE_LARGE:
        hp    = hit_points(mon->hit_dice, 6, 5);
        acmod = -2;
        evmod = -5;
        break;

    case MONS_SKELETON_SMALL:
    case MONS_SKELETON_LARGE:
        hp    = hit_points(mon->hit_dice, 5, 4);
        acmod = -6;
        evmod = -7;
        break;

    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
        // Simulacra aren't tough, but you can create piles of them. - bwr
        hp    = hit_points(mon->hit_dice, 1, 4);
        acmod = -2;
        acmod = -5;
        break;

    case MONS_SPECTRAL_THING:
        hp    = hit_points(mon->hit_dice, 4, 4);
        acmod = +2;
        evmod = -5;
        break;

    default:
        ASSERT(false);
        break;
    }

    mon->hit_points = mon->max_hit_points = hp;
    mon->ac         = std::max(mon->ac + acmod, 0);
    mon->ev         = std::max(mon->ev + evmod, 0);
}

static band_type _choose_band(int mon_type, int power, int &band_size,
                              bool &natural_leader)
{
#ifdef DEBUG_MON_CREATION
    mpr("in _choose_band()", MSGCH_DIAGNOSTICS);
#endif
    // Band size describes the number of monsters in addition to
    // the band leader.
    band_size = 0; // Single monster, no band.
    natural_leader = false;
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
        natural_leader = true;
        break;

    case MONS_ORC_HIGH_PRIEST:
        band = BAND_ORC_HIGH_PRIEST;
        band_size = 4 + random2(4);
        natural_leader = true;
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
    case MONS_VERY_UGLY_THING:
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
    case MONS_MARGERY:
        natural_leader = true;
    case MONS_HELL_KNIGHT:
        band = BAND_HELL_KNIGHTS;
        band_size = 4 + random2(4);
        break;
    case MONS_JOSEPHINE:
    case MONS_NECROMANCER:
    case MONS_VAMPIRE_MAGE:
        natural_leader = true;
        band = BAND_NECROMANCER;
        band_size = 4 + random2(4);
        break;
    case MONS_GNOLL:
        band = BAND_GNOLLS;
        band_size = (coinflip() ? 3 : 2);
        break;
    case MONS_GRUM:
        natural_leader = true;
        band = BAND_WAR_DOGS;
        band_size = 2 + random2(3);
        break;
    case MONS_BUMBLEBEE:
        band = BAND_BUMBLEBEES;
        band_size = 2 + random2(4);
        break;
    case MONS_CENTAUR_WARRIOR:
        natural_leader = true;
    case MONS_CENTAUR:
        if (power > 9 && one_chance_in(3) && you.where_are_you != BRANCH_SHOALS)
        {
            band = BAND_CENTAURS;
            band_size = 2 + random2(4);
        }
        break;

    case MONS_YAKTAUR_CAPTAIN:
        natural_leader = true;
    case MONS_YAKTAUR:
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
        natural_leader = true;
        band = BAND_OGRE_MAGE;
        band_size = 4 + random2(4);
        break;
    case MONS_BALRUG:
        natural_leader = true;
        band = BAND_BALRUG;      // RED gr demon
        band_size = 2 + random2(3);
        break;
    case MONS_CACODEMON:
        natural_leader = true;
        band = BAND_CACODEMON;      // BROWN gr demon
        band_size = 1 + random2(3);
        break;

    case MONS_EXECUTIONER:
        if (coinflip())
        {
            natural_leader = true;
            band = BAND_EXECUTIONER;  // DARKGREY gr demon
            band_size = 1 + random2(3);
        }
        break;

    case MONS_PANDEMONIUM_DEMON:
        natural_leader = true;
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
            natural_leader = true;
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

    case MONS_KIRKE:
        band_size = 2 + random2(3);
        natural_leader = true;
    case MONS_HOG:
        band = BAND_HOGS;
        band_size += 1 + random2(3);
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
            natural_leader = true;
            band = BAND_SHEEP;  // Odyssey reference
            band_size = 2 + random2(3);
        }
        break;

    case MONS_ALLIGATOR:
        // Alligators with kids!
        if (one_chance_in(5))
        {
            natural_leader = true;
            band = BAND_ALLIGATOR;
            band_size = 2 + random2(3);
        }
        break;

    case MONS_POLYPHEMUS:
        natural_leader = true;
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
        natural_leader = true;
        band = BAND_DRACONIAN;
        // yup, scary
        band_size = random_range(3,6) + random_range(3,6) + 2;
        break;

    case MONS_ILSUIW:
        band = BAND_ILSUIW;
        band_size = 3 + random2(3);
        break;

    case MONS_AZRAEL:
        natural_leader = true;
        band = BAND_AZRAEL;
        band_size = 4 + random2(5);
        break;

    case MONS_DUVESSA:
        // no natural_leader since this band is supposed to be symmetric
        band = BAND_DUVESSA;
        band_size = 1;
        break;

    case MONS_KHUFU:
        natural_leader = true;
        band = BAND_KHUFU;
        band_size = 3;
        break;

    case MONS_GOLDEN_EYE:
        band = BAND_GOLDEN_EYE;
        band_size = 1 + random2(5);
        break;

    case MONS_PIKEL:
        natural_leader = true;
        band = BAND_PIKEL;
        band_size = 4;
        break;

    case MONS_MERFOLK_AQUAMANCER:
        band = BAND_MERFOLK_AQUAMANCER;
        band_size = random_range(3, 6);
        break;

    case MONS_MERFOLK_JAVELINEER:
        band = BAND_MERFOLK_JAVELINEER;
        band_size = random_range(3, 5);
        break;

    case MONS_MERFOLK_IMPALER:
        band = BAND_MERFOLK_IMPALER;
        band_size = random_range(3, 5);
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
        mon_type = (coinflip() ? MONS_NEQOXEC : MONS_ORANGE_DEMON);
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
        mon_type = static_cast<monster_type>(
            random_choose_weighted(30, MONS_MERMAID,
                                   15, MONS_MERFOLK,
                                   10, MONS_MERFOLK_JAVELINEER,
                                   10, MONS_MERFOLK_IMPALER,
                                   0));
        break;

    case BAND_AZRAEL:
        mon_type = coinflip()? MONS_FIRE_ELEMENTAL : MONS_HELL_HOUND;
        break;

    case BAND_DUVESSA:
        mon_type = MONS_DOWAN;
        break;

    case BAND_ALLIGATOR:
        mon_type = MONS_BABY_ALLIGATOR;
        break;

    case BAND_KHUFU:
        mon_type = coinflip()? MONS_GREATER_MUMMY : MONS_MUMMY;
        break;

    case BAND_GOLDEN_EYE:
        mon_type = MONS_GOLDEN_EYE;
        break;

    case BAND_PIKEL:
        mon_type = MONS_SLAVE;
        break;

    case BAND_MERFOLK_AQUAMANCER:
        mon_type = static_cast<monster_type>(
            random_choose_weighted(8, MONS_MERFOLK,
                                   10, MONS_ICE_BEAST,
                                   0));
        break;

    case BAND_MERFOLK_IMPALER:
    case BAND_MERFOLK_JAVELINEER:
        mon_type = MONS_MERFOLK;
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

void mark_interesting_monst(monsters* monster, beh_type behaviour)
{
    if (crawl_state.game_is_arena())
        return;

    bool interesting = false;

    // Unique monsters are always intersting
    if (mons_is_unique(monster->type))
        interesting = true;
    // If it's never going to attack us, then not interesting
    else if (behaviour == BEH_FRIENDLY)
        interesting = false;
    else if (you.where_are_you == BRANCH_MAIN_DUNGEON
             && you.level_type == LEVEL_DUNGEON
             && mons_level(monster->type) >= you.absdepth0 + _ood_limit()
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
{
#ifdef DEBUG_MON_CREATION
    mpr("in mons_place()", MSGCH_DIAGNOSTICS);
#endif
    int mon_count = 0;
    for (int il = 0; il < MAX_MONSTERS; il++)
        if (menv[il].type != MONS_NO_MONSTER)
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
        mg.power = you.absdepth0;
        break;
    }

    if (mg.behaviour == BEH_COPY)
    {
        mg.behaviour = (mg.summoner == &you) ? BEH_FRIENDLY
                            : SAME_ATTITUDE((&menv[mg.summoner->mindex()]));
    }

    int mid = place_monster(mg);
    if (mid == -1)
        return (-1);

    dprf("Created a %s.", menv[mid].base_name(DESC_PLAIN, true).c_str());

    monsters *creation = &menv[mid];

    // Look at special cases: CHARMED, FRIENDLY, NEUTRAL, GOOD_NEUTRAL,
    // HOSTILE.
    if (mg.behaviour > NUM_BEHAVIOURS)
    {
        if (mg.behaviour == BEH_FRIENDLY)
            creation->flags |= MF_NO_REWARD;

        if (mg.behaviour == BEH_NEUTRAL || mg.behaviour == BEH_GOOD_NEUTRAL
            || mg.behaviour == BEH_STRICT_NEUTRAL)
        {
            creation->flags |= MF_WAS_NEUTRAL;
        }

        if (mg.behaviour == BEH_CHARMED)
        {
            creation->attitude = ATT_HOSTILE;
            creation->add_ench(ENCH_CHARM);
        }

        if (creation->type == MONS_RAKSHASA_FAKE && !one_chance_in(3))
            creation->add_ench(ENCH_INVIS);

        if (!(mg.flags & MG_FORCE_BEH) && !crawl_state.game_is_arena())
            player_angers_monster(creation);

        behaviour_event(creation, ME_EVAL);
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
    dungeon_feature_type feat_wanted;
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
        :  feat_wanted(grdw), start(pos), maxdistance(maxdist),
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
        if (!feat_compatible(feat_wanted, grd(dc)))
        {
            if (passable.find(grd(dc)) != passable.end())
                good_square(dc);
            return (false);
        }
        if (actor_at(dc) == NULL && one_chance_in(++nfound))
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

    const dungeon_feature_type feat_preferred =
        _monster_primary_habitat_feature(mons_class);
    const dungeon_feature_type feat_nonpreferred =
        _monster_secondary_habitat_feature(mons_class);

    newmons_square_find nmpfind(feat_preferred, start, distance);
    const coord_def pp = nmpfind.pathfind();
    p = pp;

    if (feat_nonpreferred != feat_preferred && !in_bounds(pp))
    {
        newmons_square_find nmsfind(feat_nonpreferred, start, distance);
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

    const dungeon_feature_type feat_preferred =
        _monster_primary_habitat_feature(mons_class);
    const dungeon_feature_type feat_nonpreferred =
        _monster_secondary_habitat_feature(mons_class);

    // Might be better if we chose a space and tried to match the monster
    // to it in the case of RANDOM_MONSTER, that way if the target square
    // is surrounded by water or lava this function would work.  -- bwr
    if (empty_surrounds(p, feat_preferred, 2, true, empty))
        pos = empty;

    if (feat_nonpreferred != feat_preferred && !in_bounds(pos)
        && empty_surrounds(p, feat_nonpreferred, 2, true, empty))
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
        (is_good_god(you.religion) && (mon->is_unholy() || mon->is_evil()));
    const bool isUnholy =
        (is_evil_god(you.religion) && mon->is_holy());
    const bool isLawful =
        (you.religion == GOD_ZIN && (mon->is_unclean() || mon->is_chaotic()));
    const bool isAntimagical =
        (you.religion == GOD_TROG && mon->is_actual_spellcaster());

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
        && mon->wont_attack())
    {
        mon->attitude = ATT_HOSTILE;
        mon->del_ench(ENCH_CHARM);
        behaviour_event(mon, ME_ALERT, MHITYOU);

        if (you.can_see(mon))
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
        || monster_at(mg.pos)
        || you.pos() == mg.pos && !fedhas_passthrough_class(mg.cls)
        || !mons_class_can_pass(montype, grd(mg.pos)))
    {
        mg.pos = find_newmons_square(montype, mg.pos);

        // Gods other than Xom will try to avoid placing their monsters
        // directly in harm's way.
        if (mg.god != GOD_NO_GOD && mg.god != GOD_XOM)
        {
            monsters dummy;
            // If the type isn't known yet assume no resists or anything.
            dummy.type         = (mg.cls == RANDOM_MONSTER) ? MONS_HUMAN
                                                            : mg.cls;
            dummy.base_monster = mg.base_type;
            dummy.god          = mg.god;

            // FIXME: resistence checks use the ghost_demon member for
            // monster types that use it, so a call to mons_avoids_cloud()
            // will crash for dummy monsters which should have a
            // ghost_demon setup.
            if (mons_is_ghost_demon(dummy.type))
                return (-1);

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
        if (crawl_state.game_is_arena())
            fail_msg = false;
    }

    // Determine whether creating a monster is successful (summd != -1) {dlb}:
    // then handle the outcome. {dlb}:
    if (fail_msg && summd == -1 && you.see_cell(mg.pos))
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

    for (radius_iterator ri(where, radius, true, false, !allow_centre);
         ri; ++ri)
    {
        bool success = false;

        if (actor_at(*ri))
            continue;

        // Players won't summon out of LOS, or past transparent walls.
        if (!you.see_cell_no_trans(*ri) && playerSummon)
            continue;

        success =
            (grd(*ri) == spc_wanted) || feat_compatible(spc_wanted, grd(*ri));

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
        temp_rand = random2(64);
        mon = ((temp_rand > 54) ? MONS_IMP :        // 15.63%
               (temp_rand > 45) ? MONS_WHITE_IMP :  // 14.06%
               (temp_rand > 36) ? MONS_LEMURE :     // 14.06%
               (temp_rand > 27) ? MONS_UFETUBUS :   // 14.06%
               (temp_rand > 18) ? MONS_IRON_IMP :   // 14.06%
               (temp_rand > 9)  ? MONS_MIDGE        // 14.06%
                                : MONS_SHADOW_IMP); // 14.06%
        break;

    case DEMON_COMMON:
        temp_rand = random2(4016);
        mon = ((temp_rand > 3897) ? MONS_SIXFIRHY :      //  2.94%
               (temp_rand > 3317) ? MONS_NEQOXEC :       // 14.44%
               (temp_rand > 2737) ? MONS_ORANGE_DEMON :  // 14.44%
               (temp_rand > 2157) ? MONS_HELLWING :      // 14.44%
               (temp_rand > 1577) ? MONS_SMOKE_DEMON :   // 14.44%
               (temp_rand > 997)  ? MONS_YNOXINUL :      // 14.44%
               (temp_rand > 839)  ? MONS_RED_DEVIL :     //  3.93%
               (temp_rand > 760)  ? MONS_HELLION :       //  1.97%
               (temp_rand > 681)  ? MONS_ROTTING_DEVIL : //  1.97%
               (temp_rand > 602)  ? MONS_TORMENTOR :     //  1.97%
               (temp_rand > 523)  ? MONS_REAPER :        //  1.97%
               (temp_rand > 444)  ? MONS_SOUL_EATER :    //  1.97%
               (temp_rand > 365)  ? MONS_HAIRY_DEVIL :   //  1.97%
               (temp_rand > 286)  ? MONS_ICE_DEVIL :     //  1.97%
               (temp_rand > 207)  ? MONS_BLUE_DEVIL :    //  1.97%
               (temp_rand > 128)  ? MONS_BEAST :         //  1.97%
               (temp_rand > 49)   ? MONS_IRON_DEVIL      //  1.97%
                                  : MONS_SUN_DEMON);     //  1.22%
        break;

    case DEMON_GREATER:
        temp_rand = random2(1000);
        mon = ((temp_rand > 868) ? MONS_CACODEMON :        // 13.10%
               (temp_rand > 737) ? MONS_BALRUG :           // 13.10%
               (temp_rand > 606) ? MONS_BLUE_DEATH :       // 13.10%
               (temp_rand > 475) ? MONS_GREEN_DEATH :      // 13.10%
               (temp_rand > 344) ? MONS_EXECUTIONER :      // 13.10%
               (temp_rand > 244) ? MONS_FIEND :            // 10.00%
               (temp_rand > 154) ? MONS_ICE_FIEND :        //  9.00%
               (temp_rand > 73)  ? MONS_SHADOW_FIEND       //  8.10%
                                 : MONS_PIT_FIEND);        //  7.30%
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
               (temp_rand > 34) ? MONS_FIRE_DRAKE :
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

    CrawlVector &type_vec   = props[VAULT_MON_TYPES_KEY].get_vector();
    CrawlVector &base_vec   = props[VAULT_MON_BASES_KEY].get_vector();
    CrawlVector &weight_vec = props[VAULT_MON_WEIGHTS_KEY].get_vector();

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

    CrawlVector &type_vec   = props[VAULT_MON_TYPES_KEY].get_vector();
    CrawlVector &base_vec   = props[VAULT_MON_BASES_KEY].get_vector();
    CrawlVector &weight_vec = props[VAULT_MON_WEIGHTS_KEY].get_vector();

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
