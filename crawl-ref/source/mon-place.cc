/**
 * @file
 * @brief Functions used when placing monsters in the dungeon.
**/

#include "AppHdr.h"

#include <algorithm>

#include "mon-place.h"
#include "mgen_data.h"

#include "abyss.h"
#include "areas.h"
#include "arena.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
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
#include "losglobal.h"
#include "message.h"
#include "mislead.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-pick.h"
#include "mon-stuff.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "state.h"
#include "env.h"
#include "terrain.h"
#include "tilepick.h"
#include "traps.h"
#include "travel.h"
#include "unwind.h"
#include "view.h"
#include "viewchar.h"
#ifdef USE_TILE
 #include "tiledef-player.h"
#endif

band_type active_monster_band = BAND_NO_BAND;

static vector<int> vault_mon_types;
static vector<int> vault_mon_bases;
static vector<int> vault_mon_weights;
static vector<bool> vault_mon_bands;

#define VAULT_MON_TYPES_KEY   "vault_mon_types"
#define VAULT_MON_BASES_KEY   "vault_mon_bases"
#define VAULT_MON_WEIGHTS_KEY "vault_mon_weights"
#define VAULT_MON_BANDS_KEY   "vault_mon_bands"

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
                                          level_id *place,
                                          bool *want_band);

static monster_type _band_member(band_type band, int which);
static band_type _choose_band(monster_type mon_type, int &band_size,
                              bool& natural_leader);

static monster* _place_monster_aux(const mgen_data &mg, const monster *leader,
                                   level_id place,
                                   bool force_pos = false,
                                   bool dont_place = false);

// Returns whether actual_feat is compatible with feat_wanted for monster
// movement and generation.
bool feat_compatible(dungeon_feature_type feat_wanted,
                     dungeon_feature_type actual_feat)
{
    if (feat_wanted == DNGN_FLOOR)
        return feat_has_solid_floor(actual_feat);

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
bool monster_habitable_grid(const monster* mon,
                            dungeon_feature_type actual_grid)
{
    // Zombified monsters enjoy the same habitat as their original.
    const monster_type mt = mons_base_type(mon);

    return (monster_habitable_grid(mt,
                                   actual_grid,
                                   DNGN_UNSEEN,
                                   mons_flies(mon),
                                   mon->cannot_move()));
}

bool mons_airborne(monster_type mcls, int flies, bool paralysed)
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
bool monster_habitable_grid(monster_type mt,
                            dungeon_feature_type actual_grid,
                            dungeon_feature_type wanted_grid_feature,
                            int flies, bool paralysed)
{
    // No monster may be placed on open sea.
    if (actual_grid == DNGN_OPEN_SEA || actual_grid == DNGN_LAVA_SEA)
        return false;

    // Monsters can't use teleporters, and standing there would look just wrong.
    if (actual_grid == DNGN_TELEPORTER)
        return false;

    const dungeon_feature_type feat_preferred =
        habitat2grid(mons_class_primary_habitat(mt));
    const dungeon_feature_type feat_nonpreferred =
        habitat2grid(mons_class_secondary_habitat(mt));

    const bool monster_is_airborne = mons_airborne(mt, flies, paralysed);

    // If the caller insists on a specific feature type, try to honour
    // the request. This allows the builder to place amphibious
    // creatures only on land, or flying creatures only on lava, etc.
    if (wanted_grid_feature != DNGN_UNSEEN
        && (feat_compatible(feat_preferred, wanted_grid_feature)
            || feat_compatible(feat_nonpreferred, wanted_grid_feature)
            || (monster_is_airborne && !feat_is_solid(wanted_grid_feature))))
    {
        return feat_compatible(wanted_grid_feature, actual_grid);
    }

    // Special check for fire elementals since their habitat is floor which
    // is generally considered compatible with shallow water.
    if (mt == MONS_FIRE_ELEMENTAL && feat_is_watery(actual_grid))
        return false;

    if (actual_grid == DNGN_MALIGN_GATEWAY)
    {
        if (mt == MONS_ELDRITCH_TENTACLE
            || mt == MONS_ELDRITCH_TENTACLE_SEGMENT)
        {
            return true;
        }
        else
            return false;
    }

    if (feat_compatible(feat_preferred, actual_grid)
        || (feat_nonpreferred != feat_preferred
            && feat_compatible(feat_nonpreferred, actual_grid)))
    {
        return true;
    }

    // [dshaligram] Flying creatures are all DNGN_FLOOR, so we
    // only have to check for the additional valid grids of deep
    // water and lava.
    if (monster_is_airborne
        && (actual_grid == DNGN_LAVA || actual_grid == DNGN_DEEP_WATER))
    {
        return true;
    }

    return false;
}

// Returns true if the monster can submerge in the given grid.
bool monster_can_submerge(const monster* mon, dungeon_feature_type feat)
{
    if (testbits(env.pgrid(mon->pos()), FPROP_NO_SUBMERGE))
        return false;
    if (!mon->is_habitable_feat(feat))
        return false;
    if (mons_class_flag(mon->type, M_SUBMERGES))
        switch (mons_habitat(mon))
        {
        case HT_WATER:
        case HT_AMPHIBIOUS:
            return feat_is_watery(feat);
        case HT_LAVA:
            return (feat == DNGN_LAVA);
        case HT_LAND:
            // Currently, trapdoor spider and air elemental only.
            return (feat == DNGN_FLOOR);
        default:
            return false;
        }
    else
        return false;
}

static bool _is_spawn_scaled_area(const level_id &here)
{
    return (is_connected_branch(here.branch)
            && !is_hell_subbranch(here.branch)
            && here.branch != BRANCH_VESTIBULE
            && here.branch != BRANCH_ZOT);
}

// Scale monster generation parameter with time spent on level. Note:
// (target_value - base_value) * dropoff_ramp_turns must be < INT_MAX!
static int _scale_spawn_parameter(int base_value,
                                  int target_value,
                                  int final_value,
                                  int dropoff_start_turns = 3000,
                                  int dropoff_ramp_turns  = 12000)
{
    if (!_is_spawn_scaled_area(level_id::current()))
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

static void _apply_ood(level_id &place)
{
    // OODs do not apply to any portal vaults, any 1-level branches, Zot and
    // hells.  What with newnewabyss?
    if (!is_connected_branch(place)
        || place.branch == BRANCH_ZOT
        || is_hell_subbranch(place.branch)
        || brdepth[place.branch] <= 1)
    {
        return;
    }

    // The OOD fuzz roll is not applied at level generation time on
    // D:1, and is applied slightly less often (0.75*0.14) on D:2. All
    // other levels have a straight 14% chance of moderate OOD fuzz
    // for each monster at level generation, and the chances of
    // moderate OODs go up to 100% after a ramp-up period.

    if (place.branch == BRANCH_DUNGEON
        && (place.depth == 1 && env.turns_on_level < 701
         || place.depth == 2 && (env.turns_on_level < 584 || one_chance_in(4))))
    {
        return;
    }

#ifdef DEBUG_DIAGNOSTICS
    level_id old_place = place;
#endif

    if (x_chance_in_y(_scale_spawn_parameter(140, 1000, 1000, 3000, 4800),
                      1000))
    {
        const int fuzzspan = 5;
        const int fuzz = max(0, random_range(-fuzzspan, fuzzspan, 2));

        // Quite bizarre logic: why should we fail in >50% cases here?
        if (fuzz)
        {
            place.depth += fuzz;
            dprf("Monster level fuzz: %d (old: %s, new: %s)",
                 fuzz, old_place.describe().c_str(), place.describe().c_str());
        }
    }

    // On D:13 and deeper, and for those who tarry, something extreme:
    if (env.turns_on_level > 1400 - place.absdepth() * 117
        && x_chance_in_y(_scale_spawn_parameter(2, 10000, 10000, 3000, 9000),
                         10000))
    {
        // this maxes depth most of the time
        place.depth += random2avg(27, 2);
        dprf("Super OOD roll: Old: %s, New: %s",
             old_place.describe().c_str(), place.describe().c_str());
    }
}

static int _vestibule_spawn_rate()
{
    // Monster generation in the Vestibule drops off quickly.
    const int taper_off_turn = 500;
    int genodds = 240;
    // genodds increases once you've spent more than 500 turns in Hell.
    if (env.turns_on_level > taper_off_turn)
    {
        genodds += (env.turns_on_level - taper_off_turn);
        genodds  = (genodds < 0 ? 20000 : min(genodds, 20000));
    }

    return genodds;
}

//#define DEBUG_MON_CREATION

// This function is now only called about once every 5 turns. (Used to be
// every turn independent of how much time an action took, which was not ideal.)
// To arrive at spawning rates close to how they used to be, replace the
// one_chance_in(value) checks with the new x_chance_in_y(5, value). (jpeg)
void spawn_random_monsters()
{
    if (crawl_state.disables[DIS_SPAWNS])
        return;

    if (crawl_state.game_is_arena()
        || (crawl_state.game_is_sprint()
            && player_in_connected_branch()
            && you.char_direction == GDT_DESCENDING))
    {
        return;
    }

#ifdef DEBUG_MON_CREATION
    mpr("in spawn_random_monsters()", MSGCH_DIAGNOSTICS);
#endif
    int rate = env.spawn_random_rate;
    if (!rate)
    {
#ifdef DEBUG_MON_CREATION
        mpr("random monster gen turned off", MSGCH_DIAGNOSTICS);
#endif
        return;
    }

    if (player_in_branch(BRANCH_VESTIBULE))
        rate = _vestibule_spawn_rate();

    rate = (you.char_direction == GDT_DESCENDING) ?
            _scale_spawn_parameter(rate, 6 * rate, 0)
            : (you_worship(GOD_CHEIBRIADOS)) ? 16 : 8;

    if (rate == 0)
    {
        dprf("random monster gen scaled off, %d turns on level",
             env.turns_on_level);
        return;
    }

    if (player_in_branch(BRANCH_ABYSS))
    {
        if (you.char_direction != GDT_GAME_START)
            rate = 5;
        if (you_worship(GOD_CHEIBRIADOS))
            rate *= 2;
    }

    if (!x_chance_in_y(5, rate))
        return;

    // Place normal dungeon monsters, but not in player LOS.
    if (player_in_connected_branch())
    {
        dprf("Placing monster, rate: %d, turns here: %d",
             rate, env.turns_on_level);
        proximity_type prox = (one_chance_in(10) ? PROX_NEAR_STAIRS
                                                 : PROX_AWAY_FROM_PLAYER);

        // The rules change once the player has picked up the Orb...
        if (you.char_direction == GDT_ASCENDING)
            prox = (one_chance_in(3) ? PROX_CLOSE_TO_PLAYER : PROX_ANYWHERE);

        mgen_data mg(WANDERING_MONSTER);
        mg.proximity = prox;
        mg.foe = (you.char_direction == GDT_ASCENDING) ? MHITYOU : MHITNOT;
        mons_place(mg);
        viewwindow();
        return;
    }

    mgen_data mg(WANDERING_MONSTER);
    if (player_in_branch(BRANCH_ABYSS) && one_chance_in(3))
        mg.place = abyssal_state.level;
    else if (player_in_branch(BRANCH_PANDEMONIUM)
             && !env.properties.exists("vault_mon_weights")
             && !one_chance_in(40))
    {
        mg.cls = env.mons_alloc[random2(PAN_MONS_ALLOC)];
        mg.flags |= MG_PERMIT_BANDS;
    }

    mons_place(mg);
    viewwindow();
}

static bool _is_random_monster(monster_type mt)
{
    return (mt == RANDOM_MONSTER || mt == RANDOM_MOBILE_MONSTER
            || mt == RANDOM_COMPATIBLE_MONSTER
            || mt == WANDERING_MONSTER);
}

static bool _is_not_zombifiable(monster_type mt)
{
    return !mons_zombie_size(mt);
}

static bool _has_big_aura(monster_type mt)
{
    return mt == MONS_MOTH_OF_SUPPRESSION || mt == MONS_SILENT_SPECTRE;
}

static bool _is_incompatible_monster(monster_type mt)
{
    return mons_class_is_stationary(mt)
        || player_will_anger_monster(mt);
}

// Caller must use !invalid_monster_type to check if the return value
// is a real monster.
monster_type pick_random_monster(level_id place,
                                 monster_type kind,
                                 level_id *final_place)
{
    if (crawl_state.game_is_arena())
    {
        monster_type type = arena_pick_random_monster(place);
        if (!_is_random_monster(type))
            return type;
    }

    _apply_ood(place);

    place.depth = min(place.depth, branch_ood_cap(place.branch));

    if (final_place)
        *final_place = place;

    if (crawl_state.game_is_arena())
        return pick_monster(place, arena_veto_random_monster);
    else if (kind == RANDOM_MOBILE_MONSTER)
        return pick_monster(place, mons_class_is_stationary);
    else if (kind == RANDOM_COMPATIBLE_MONSTER)
        return pick_monster(place, _is_incompatible_monster);
    else if (mons_class_is_zombified(kind))
        return pick_monster(place, _is_not_zombifiable);
    else if (crawl_state.game_is_sprint())
        return pick_monster(place, _has_big_aura);
    else
        return pick_monster(place);
}

bool can_place_on_trap(monster_type mon_type, trap_type trap)
{
    if (mons_is_tentacle_segment(mon_type))
        return true;

    if (trap == TRAP_TELEPORT)
        return false;

    if (trap == TRAP_SHAFT)
    {
        if (_is_random_monster(mon_type))
            return false;

        return (mons_class_flies(mon_type)
                || get_monster_data(mon_type)->size == SIZE_TINY);
    }

    return true;
}

bool drac_colour_incompatible(int drac, int colour)
{
    return (drac == MONS_DRACONIAN_SCORCHER && colour == MONS_WHITE_DRACONIAN);
}

// Finds a random square as close to a staircase as possible
static bool _find_mon_place_near_stairs(coord_def& pos,
                                        dungeon_char_type *stair_type,
                                        level_id &place)
{
    pos = get_random_stair();
    const dungeon_feature_type feat = grd(pos);
    *stair_type = get_feature_dchar(feat);

    // First, assume a regular stair.
    switch (feat_stair_direction(feat))
    {
    case CMD_GO_UPSTAIRS:
        if (place.depth > 1)
            place.depth--;
        break;
    case CMD_GO_DOWNSTAIRS:
        if (place.depth < brdepth[place.branch])
            place.depth++;
        break;
    default: ;
    }

    // Is it a branch stair?
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].entry_stairs == feat)
        {
            place = static_cast<branch_type>(i);
            break;
        }
        else if (branches[i].exit_stairs == feat)
        {
            place = brentry[i];
            // This can happen on D:1 and in wizmode with random spawns on the
            // first floor of a branch that didn't generate naturally.
            if (!place.is_valid())
                return false;
            break;
        }
    }
    const monster_type habitat_target = MONS_BAT;
    int distance = 3;
    if (crawl_state.game_is_zotdef())
        distance = 9999;
    pos = find_newmons_square_contiguous(habitat_target, pos, distance);
    return in_bounds(pos);
}

static monster_type _resolve_monster_type(monster_type mon_type,
                                          proximity_type proximity,
                                          monster_type &base_type,
                                          coord_def &pos,
                                          unsigned mmask,
                                          dungeon_char_type *stair_type,
                                          level_id *place,
                                          bool *want_band)
{
    if (want_band)
        *want_band = false;

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
    else if (mon_type >= RANDOM_DEMON_LESSER && mon_type <= RANDOM_DEMON)
        mon_type = summon_any_demon(mon_type);

    // (2) Take care of non-draconian random monsters.
    else if (_is_random_monster(mon_type))
    {
        // Respect destination level for staircases.
        if (proximity == PROX_NEAR_STAIRS)
        {
            const level_id orig_place = *place;

            if (_find_mon_place_near_stairs(pos, stair_type, *place))
            {
                // No monsters spawned in the Temple.
                if (branches[place->branch].id == BRANCH_TEMPLE)
                    proximity = PROX_AWAY_FROM_PLAYER;
            }
            else
                proximity = PROX_AWAY_FROM_PLAYER;
            if (proximity == PROX_NEAR_STAIRS)
                dprf("foreign monster from %s", place->describe().c_str());
            else // we dunt cotton to no ferrniers in these here parts
                *place = orig_place;

        } // end proximity check

        if (!vault_mon_types.empty())
        {
            // XXX: not respecting RANDOM_MOBILE_MONSTER currently.
            int i = choose_random_weighted(vault_mon_weights.begin(),
                                           vault_mon_weights.end());
            int type = vault_mon_types[i];
            int base = vault_mon_bases[i];
            bool banded = vault_mon_bands[i];

            if (type == -1)
                *place = level_id::from_packed_place(base);
            else
            {
                base_type = (monster_type) base;
                mon_type  = (monster_type) type;
                if (want_band)
                    *want_band = banded;
                if (mon_type == RANDOM_DRACONIAN
                    || mon_type == RANDOM_BASE_DRACONIAN
                    || mon_type == RANDOM_NONBASE_DRACONIAN)
                {
                    mon_type =
                        _resolve_monster_type(mon_type, proximity,
                                              base_type, pos, mmask,
                                              stair_type,
                                              place,
                                              want_band);
                }
                return mon_type;
            }
        }

        int tries = 0;
        while (tries++ < 300)
        {
            level_id orig_place = *place;

            // Now pick a monster of the given branch and level.
            mon_type = pick_random_monster(*place, mon_type, place);

            // Don't allow monsters too stupid to use stairs (e.g.
            // non-spectral zombified undead) to be placed near
            // stairs.
            if (proximity != PROX_NEAR_STAIRS
                || mons_class_can_use_stairs(mon_type))
            {
                break;
            }

            *place = orig_place;
        }

        if (proximity == PROX_NEAR_STAIRS && tries >= 300)
            mon_type = pick_random_monster(*place, mon_type, place);
    }
    return mon_type;
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

    return result;
}

// Checks if the monster is ok to place at mg_pos. If force_location
// is true, then we'll be less rigorous in our checks, in particular
// allowing land monsters to be placed in shallow water and water
// creatures in fountains.
static bool _valid_monster_generation_location(const mgen_data &mg,
                                                const coord_def &mg_pos)
{
    if (!in_bounds(mg_pos)
        || monster_at(mg_pos)
        || you.pos() == mg_pos && !fedhas_passthrough_class(mg.cls))
    {
        return false;
    }

    const monster_type montype = (mons_class_is_zombified(mg.cls) ? mg.base_type
                                                                  : mg.cls);
    if (!monster_habitable_grid(montype, grd(mg_pos), mg.preferred_grid_feature,
                                mons_class_flies(montype), false)
        || (mg.behaviour != BEH_FRIENDLY && !mons_is_mimic(montype)
            && is_sanctuary(mg_pos)
            && !mons_is_tentacle_segment(montype)))
    {
        return false;
    }

    // Check player proximity to avoid band members being placed
    // close to the player erroneously.
    // XXX: This is a little redundant with proximity checks in
    // place_monster.
    if (mg.proximity == PROX_AWAY_FROM_PLAYER
        && distance2(you.pos(), mg_pos) <= LOS_RADIUS_SQ)
    {
        return false;
    }

    // Don't generate monsters on top of teleport traps.
    // (How did they get there?)
    const trap_def* ptrap = find_trap(mg_pos);
    if (ptrap && !can_place_on_trap(mg.cls, ptrap->type))
        return false;

    return true;
}

static bool _valid_monster_generation_location(mgen_data &mg)
{
    return _valid_monster_generation_location(mg, mg.pos);
}

// Returns true if the player is on a level that should be sheltered from
// OOD packs, based on depth and time spent on-level.
static bool _in_ood_pack_protected_place()
{
    return (env.turns_on_level < 1400 - env.absdepth0 * 117);
}

static void _abyss_monster_creation_message(const monster* mon)
{
    if (mon->type == MONS_DEATH_COB)
    {
        mprf(coinflip() ? "%s appears in a burst of microwaves!"
                        : "%s pops from nullspace!",
             mon->name(DESC_A).c_str()); // always "a death cob"
        return;
    }

    mprf(random_choose_weighted(
         17, "%s appears in a shower of translocational energy.",
         34, "%s appears in a shower of sparks.",
         45, "%s materialises.",
         13, "%s emerges from chaos.",
         26, "%s emerges from the beyond.",
         33, "%s assembles %s!",
          9, "%s erupts from nowhere!",
         18, "%s bursts from nowhere!",
          7, "%s is cast out of space!",
         14, "%s is cast out of reality!",
          5, "%s coalesces out of pure chaos.",
         10, "%s coalesces out of seething chaos.",
          2, "%s punctures the fabric of time!",
          7, "%s punctures the fabric of the universe.",
          3, "%s manifests%3$s!",
          0),
         mon->name(DESC_A).c_str(), mon->pronoun(PRONOUN_REFLEXIVE).c_str(),
         silenced(you.pos()) ? "" : " with a bang");
}

monster* place_monster(mgen_data mg, bool force_pos, bool dont_place)
{
#ifdef DEBUG_MON_CREATION
    mpr("in place_monster()", MSGCH_DIAGNOSTICS);
#endif

    int tries = 0;
    dungeon_char_type stair_type = NUM_DCHAR_TYPES;

    // (1) Early out (summoned to occupied grid).
    if (mg.use_position() && monster_at(mg.pos))
        return 0;

    if (!mg.place.is_valid())
        mg.place = level_id::current();

    bool want_band = false;
    level_id place = mg.place;
    mg.cls = _resolve_monster_type(mg.cls, mg.proximity, mg.base_type,
                                   mg.pos, mg.map_mask,
                                   &stair_type,
                                   &place,
                                   &want_band);
    bool chose_ood_monster = place.absdepth() > mg.place.absdepth() + 5;
    if (want_band)
        mg.flags |= MG_PERMIT_BANDS;

    if (mg.cls == MONS_NO_MONSTER || mg.cls == MONS_PROGRAM_BUG)
        return 0;

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

    // Re-check for PROX_NEAR_STAIRS here - if original monster
    // type wasn't RANDOM_MONSTER then the position won't
    // have been set.
    if (mg.proximity == PROX_NEAR_STAIRS && mg.pos.origin())
    {
        level_id lev;
        if (!_find_mon_place_near_stairs(mg.pos, &stair_type, lev))
            mg.proximity = PROX_AWAY_FROM_PLAYER;
    } // end proximity check

    if (mg.cls == MONS_PROGRAM_BUG)
        return 0;

    // (3) Decide on banding (good lord!)
    int band_size = 1;
    bool leader = false;
    monster_type band_monsters[BIG_BAND];        // band monster types
    band_type band = BAND_NO_BAND;
    band_monsters[0] = mg.cls;

    // The (very) ugly thing band colour.
    static colour_t ugly_colour = BLACK;

    if (create_band)
    {
#ifdef DEBUG_MON_CREATION
        mpr("Choose band members...", MSGCH_DIAGNOSTICS);
#endif
        band = _choose_band(mg.cls, band_size, leader);
        band_size++;
        for (int i = 1; i < band_size; ++i)
        {
            band_monsters[i] = _band_member(band, i);

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

    if (!mg.use_position() && !force_pos)
    {
        tries = 0;

        // Try to pick a position that is
        // a) not occupied
        // b) compatible
        // c) in the 'correct' proximity to the player

        while (true)
        {
            if (tries++ >= 45)
                return 0;

            // Placement already decided for PROX_NEAR_STAIRS.
            // Else choose a random point on the map.
            if (mg.proximity != PROX_NEAR_STAIRS)
                mg.pos = random_in_bounds();

            if (!_valid_monster_generation_location(mg))
                continue;

            // Is the grid verboten?
            if (map_masked(mg.pos, mg.map_mask))
                continue;

            // Let's recheck these even for PROX_NEAR_STAIRS, just in case.
            // Check proximity to player.
            proxOK = true;

            switch (mg.proximity)
            {
            case PROX_ANYWHERE:
                if (distance2(you.pos(), mg.pos) < dist_range(2 + random2(3)))
                    proxOK = false;
                break;

            case PROX_CLOSE_TO_PLAYER:
            case PROX_AWAY_FROM_PLAYER:
                // If this is supposed to measure los vs not los,
                // then see_cell(mg.pos) should be used instead. (jpeg)
                close_to_player = (distance2(you.pos(), mg.pos) <=
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
    else if (!_valid_monster_generation_location(mg) && !dont_place)
    {
        // Sanity check that the specified position is valid.
        return 0;
    }

    // Reset the (very) ugly thing band colour.
    if (ugly_colour != BLACK)
        ugly_colour = BLACK;

    monster* mon = _place_monster_aux(mg, 0, place, force_pos, dont_place);

    // Bail out now if we failed.
    if (!mon)
        return 0;

    if (mg.props.exists("map"))
        mon->set_originating_map(mg.props["map"].get_string());

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

    if (player_in_branch(BRANCH_ABYSS) && !mg.summoner
        && in_bounds(mon->pos())
        && !(mg.extra_flags & MF_WAS_IN_VIEW)
        && !cell_is_solid(mon->pos()))
    {
        big_cloud(CLOUD_TLOC_ENERGY, mon, mon->pos(), 3 + random2(3), 3, 3);
    }

    // Message to player from stairwell/gate/abyss appearance.
    if (shoved)
    {
        mprf("%s shoves you out of the %s!",
             mon->visible_to(&you) ? mon->name(DESC_A).c_str() : "Something",
             stair_type == DCHAR_ARCH ? "gateway" : "stairwell");
    }
    else if (mg.proximity == PROX_NEAR_STAIRS && you.can_see(mon))
    {
        const char *msg = nullptr;
        switch (stair_type)
        {
        case DCHAR_STAIRS_DOWN: msg = "up the stairs."; break;
        case DCHAR_STAIRS_UP:   msg = "down the stairs."; break;
        case DCHAR_ARCH:        msg = "through the gate."; break;
        default: ;
        }
        if (msg)
            mprf("%s comes %s", mon->name(DESC_A).c_str(), msg);
    }
    else if (player_in_branch(BRANCH_ABYSS) && you.can_see(mon)
             && !crawl_state.generating_level
             && !mg.summoner && !mons_is_mimic(mon->type)
             && !crawl_state.is_god_acting()
             && !(mon->flags & MF_WAS_IN_VIEW)) // is this possible?
    {
        _abyss_monster_creation_message(mon);
    }

    // Now, forget about banding if the first placement failed, or there are
    // too many monsters already, or we successfully placed by stairs.
    // Zotdef change - banding allowed on stairs for extra challenge!
    // Frequency reduced, though, and only after 2K turns.
    if (mon->mindex() >= MAX_MONSTERS - 30
        || (mg.proximity == PROX_NEAR_STAIRS && !crawl_state.game_is_zotdef())
        || (crawl_state.game_is_zotdef() && you.num_turns < 2000))
        return mon;

    // Not PROX_NEAR_STAIRS, so it will be part of a band, if there is any.
    if (band_size > 1)
        mon->flags |= MF_BAND_MEMBER;

    const bool priest = mon->is_priest();

    mgen_data band_template = mg;

    if (leader && !mg.summoner)
    {
        band_template.summoner = mon;
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

        if (monster *member = _place_monster_aux(band_template, mon, place))
        {
            member->flags |= MF_BAND_MEMBER;
            member->props["band_leader"].get_int() = mon->mid;
            member->set_originating_map(mon->originating_map());

            // Priestly band leaders should have an entourage of the
            // same religion, unless members of that entourage already
            // have a different one.
            if (priest && member->god == GOD_NO_GOD)
                member->god = mon->god;

            if (mon->type == MONS_PIKEL)
            {
                // Don't give XP for the slaves to discourage hunting.  Pikel
                // has an artificially large XP modifier to compensate for
                // this.
                member->flags |= MF_NO_REWARD;
                member->props["pikel_band"] = true;
            }
            else if (mon->type == MONS_KIRKE)
                member->props["kirke_band"] = true;
            else if (mon->type == MONS_SHEDU)
            {
                // We store these here for later resurrection, etc.
                member->number = mon->mid;
                mon->number = member->mid;
            }
        }
    }

    // Placement of first monster, at least, was a success.
    return mon;
}

monster* get_free_monster()
{
    for (int i = 0; i < MAX_MONSTERS; ++i)
        if (env.mons[i].type == MONS_NO_MONSTER)
        {
            env.mons[i].reset();
            return (&env.mons[i]);
        }

    return NULL;
}

void mons_add_blame(monster* mon, const string &blame_string)
{
    const bool exists = mon->props.exists("blame");
    CrawlStoreValue& blame = mon->props["blame"];
    if (!exists)
        blame.new_vector(SV_STR, SFLAG_CONST_TYPE);
    blame.get_vector().push_back(blame_string);
}

static void _place_twister_clouds(monster *mon)
{
    // Yay for the abj_degree having a huge granularity.
    if (mon->has_ench(ENCH_ABJ))
    {
        mon_enchant abj = mon->get_ench(ENCH_ABJ);
        mon->lose_ench_duration(abj, abj.duration / 2);
    }

    tornado_damage(mon, -10);
}

static monster* _place_monster_aux(const mgen_data &mg, const monster *leader,
                                   level_id place,
                                   bool force_pos, bool dont_place)
{
    coord_def fpos;

    // Some sanity checks.
    if (mons_is_unique(mg.cls) && you.unique_creatures[mg.cls]
            && !crawl_state.game_is_arena()
        || mons_class_flag(mg.cls, M_CANT_SPAWN))
    {
        die("invalid monster to place: %s (%d)", mons_class_name(mg.cls), mg.cls);
    }

    const monsterentry *m_ent = get_monster_data(mg.cls);

    monster* mon = get_free_monster();
    if (!mon)
        return 0;

    const monster_type montype = (mons_class_is_zombified(mg.cls) ? mg.base_type
                                                                  : mg.cls);

    // Setup habitat and placement.
    // If the space is occupied, try some neighbouring square instead.
    if (dont_place)
        fpos.reset();
    else if (leader == 0 && in_bounds(mg.pos)
        && (mg.behaviour == BEH_FRIENDLY ||
            (!is_sanctuary(mg.pos) || mons_is_tentacle_segment(montype))
            || mons_is_mimic(montype))
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

            // Place members within LOS_SOLID of their leader.
            // TODO nfm - allow placing around corners but not across walls.
            if ((leader == 0 || cell_see_cell(fpos, leader->pos(), LOS_SOLID))
                && _valid_monster_generation_location(mg, fpos))
            {
                break;
            }
        }

        // Did we really try 1000 times?
        if (i == 1000)
            return 0;
    }

    ASSERT(!monster_at(fpos));

    if (crawl_state.game_is_arena()
        && arena_veto_place_monster(mg, leader == 0, fpos))
    {
        return 0;
    }

    // Now, actually create the monster. (Wheeee!)
    mon->set_new_monster_id();
    mon->type         = mg.cls;
    mon->base_monster = mg.base_type;
    mon->number       = mg.number;

    // Set pos and link monster into monster grid.
    if (!dont_place && !mon->move_to_pos(fpos))
    {
        mon->reset();
        return 0;
    }

    // Pick the correct Serpent of Hell.
    if (mon->type == MONS_SERPENT_OF_HELL)
        switch (place.branch)
        {
        case BRANCH_COCYTUS:
            mon->type = MONS_SERPENT_OF_HELL_COCYTUS;
            break;
        case BRANCH_DIS:
            mon->type = MONS_SERPENT_OF_HELL_DIS;
            break;
        case BRANCH_TARTARUS:
            mon->type = MONS_SERPENT_OF_HELL_TARTARUS;
            break;
        default: ; // if it spawns out of Hell (sprint, wizmode), use Gehenna
        }

    // Generate a brand shiny new monster, or zombie.
    if (mons_class_is_zombified(mg.cls))
    {
        monster_type ztype = mg.base_type;

        if (ztype == MONS_NO_MONSTER || ztype == RANDOM_MONSTER)
            ztype = pick_local_zombifiable_monster(place, mg.cls, fpos);

        define_zombie(mon, ztype, mg.cls);
    }
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
        // Berserkers belong to Trog.
        if (mg.cls == MONS_DEEP_DWARF_BERSERKER
            || mg.cls == MONS_SPRIGGAN_BERSERKER)
        {
            mon->god = GOD_TROG;
        }
        // Profane servitors and deep dwarf death knights belong to Yredelemnul.
        else if (mg.cls == MONS_PROFANE_SERVITOR
                 || mg.cls == MONS_DEEP_DWARF_DEATH_KNIGHT)
        {
            mon->god = GOD_YREDELEMNUL;
        }
        // Wiglaf belongs to Okawaru.
        else if (mg.cls == MONS_WIGLAF)
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
    // XXX: Unborn belong to Yredelemnul, but only so they'll get the right
    // message when casting Injury Mirror.
    else if (mg.cls == MONS_UNBORN)
        mon->god = GOD_YREDELEMNUL;
    // The royal jelly belongs to Jiyva.
    else if (mg.cls == MONS_ROYAL_JELLY)
        mon->god = GOD_JIYVA;
    // Mennas belongs to Zin.
    else if (mg.cls == MONS_MENNAS)
        mon->god = GOD_ZIN;
    // Yiuf is a faithful Xommite.
    else if (mg.cls == MONS_CRAZY_YIUF)
        mon->god = GOD_XOM;
    // Grinder and Ignacio belong to Makhleb.
    else if (mg.cls == MONS_GRINDER
             || mg.cls == MONS_IGNACIO)
    {
        mon->god = GOD_MAKHLEB;
    }
    // 1 out of 7 non-priestly orcs are unbelievers.
    else if (mons_genus(mg.cls) == MONS_ORC)
    {
        if (!one_chance_in(7))
            mon->god = GOD_BEOGH;
    }
    else if (mg.cls == MONS_APIS)
        mon->god = GOD_ELYVILON;
    // Angels (other than Mennas) and daevas belong to TSO, but 1 out of
    // 7 in the Abyss are adopted by Xom.
    else if (mons_class_holiness(mg.cls) == MH_HOLY)
    {
        if (mg.place != BRANCH_ABYSS || !one_chance_in(7))
            mon->god = GOD_SHINING_ONE;
        else
            mon->god = GOD_XOM;
    }

    // Holy monsters need their halo!
    if (mon->holiness() == MH_HOLY)
        invalidate_agrid(true);
    if (mg.cls == MONS_SILENT_SPECTRE || mg.cls == MONS_PROFANE_SERVITOR
        || mg.cls == MONS_MOTH_OF_SUPPRESSION)
    {
        invalidate_agrid(true);
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

    if (!crawl_state.game_is_arena())
    {
        mon->max_hit_points = min(mon->max_hit_points, MAX_MONSTER_HP);
        mon->hit_points = min(mon->hit_points, MAX_MONSTER_HP);
    }

    // Store the extra flags here.
    mon->flags       |= mg.extra_flags;

    // The return of Boris is now handled in monster_die().  Not setting
    // this for Boris here allows for multiple Borises in the dungeon at
    // the same time. - bwr
    if (mons_is_unique(mg.cls))
        you.unique_creatures.set(mg.cls);

    if (mons_class_flag(mg.cls, M_INVIS))
        mon->add_ench(ENCH_INVIS);

    if (mons_class_flag(mg.cls, M_CONFUSED))
        mon->add_ench(ENCH_CONFUSION);

    if (montype == MONS_SHAPESHIFTER)
        mon->add_ench(ENCH_SHAPESHIFTER);

    if (montype == MONS_GLOWING_SHAPESHIFTER)
        mon->add_ench(ENCH_GLOWING_SHAPESHIFTER);

    if (mg.cls == MONS_TOADSTOOL || mg.cls == MONS_PILLAR_OF_SALT)
    {
        // This enchantment is a timer that counts down until death.
        // It should last longer than the lifespan of a corpse, to avoid
        // spawning mushrooms in the same place over and over.  Aside
        // from that, the value is slightly randomised to avoid
        // simultaneous die-offs of mushroom rings.
        mon->add_ench(ENCH_SLOWLY_DYING);
    }
    else if (mg.cls == MONS_HYPERACTIVE_BALLISTOMYCETE)
        mon->add_ench(ENCH_EXPLODING);

    if (mg.cls == MONS_TWISTER || mg.cls == MONS_DIAMOND_OBELISK)
    {
        mon->props["tornado_since"].get_int() = you.elapsed_time;
        mon->add_ench(mon_enchant(ENCH_TORNADO, 0, 0, INFINITE_DURATION));
    }

    if (!crawl_state.game_is_arena() && you.misled())
        update_mislead_monster(mon);

    mon->flags |= MF_JUST_SUMMONED;

    // Don't leave shifters in their starting shape.
    if (mg.cls == MONS_SHAPESHIFTER || mg.cls == MONS_GLOWING_SHAPESHIFTER)
    {
        no_messages nm;
        monster_polymorph(mon, mg.initial_shifter);

        // It's not actually a known shapeshifter if it happened to be
        // placed in LOS of the player.
        mon->flags &= ~MF_KNOWN_SHIFTER;
    }

    // dur should always be 1-6 for monsters that can be abjured.
    const bool summoned = mg.abjuration_duration >= 1
                       && mg.abjuration_duration <= 6;

    if (mons_class_is_animated_weapon(mg.cls))
    {
        if (mg.props.exists(TUKIMA_WEAPON))
            give_specific_item(mon, mg.props[TUKIMA_WEAPON].get_item());
        else
            give_item(mon, place.absdepth(), summoned);

        // Dancing weapons *always* have a weapon. Fail to create them
        // otherwise.
        const item_def* wpn = mon->mslot_item(MSLOT_WEAPON);
        if (!wpn)
        {
            mon->destroy_inventory();
            mon->reset();
            mgrd(fpos) = NON_MONSTER;
            return 0;
        }
        else
            mon->colour = wpn->colour;
    }
    else if (mons_class_itemuse(mg.cls) >= MONUSE_STARTING_EQUIPMENT)
    {
        give_item(mon, place.absdepth(), summoned);
        // Give these monsters a second weapon. - bwr
        if (mons_class_wields_two_weapons(mg.cls))
            give_weapon(mon, place.absdepth(), summoned);

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

    if (monster_can_submerge(mon, grd(fpos)) && !one_chance_in(5) && !summoned)
        mon->add_ench(ENCH_SUBMERGED);

    if (mons_is_mimic(mg.cls))
        mon->props = mg.props;

    // Set attitude, behaviour and target.
    mon->attitude  = ATT_HOSTILE;
    mon->behaviour = mg.behaviour;

    // Statues cannot sleep (nor wander but it means they are a bit
    // more aware of the player than they'd be otherwise).
    if (mons_is_statue(mg.cls))
        mon->behaviour = BEH_WANDER;
    // Trapdoor spiders lurk, they don't sleep
    if (mg.cls == MONS_TRAPDOOR_SPIDER)
        mon->behaviour = BEH_LURK;

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
        // Instead of looking for dancing weapons, look for Tukima's dance.
        // Dancing weapons can be created with shadow creatures. {due}
        bool mark_items = mg.summon_type != SPELL_TUKIMAS_DANCE;

        mon->mark_summoned(mg.abjuration_duration,
                           mark_items,
                           mg.summon_type);
    }

    // Perm summons shouldn't leave gear either.
    if (mg.extra_flags & MF_HARD_RESET && mg.extra_flags & MF_NO_REWARD)
        mon->mark_summoned(0, true, 0, false);

    ASSERT(!invalid_monster_index(mg.foe)
           || mg.foe == MHITYOU || mg.foe == MHITNOT);
    mon->foe = mg.foe;

    string blame_prefix;

    if (mg.flags & MG_BAND_MINION)
        blame_prefix = "led by ";
    else if (mg.abjuration_duration > 0)
    {
        blame_prefix = "summoned by ";

        if (mg.summoner != NULL && mg.summoner->alive()
            && mg.summoner->type == MONS_MARA)
        {
            blame_prefix = "woven by ";
        }

        if (mg.cls == MONS_DANCING_WEAPON)
            blame_prefix = "animated by ";
    }
    else if (mons_class_is_zombified(mg.cls))
        blame_prefix = "animated by ";
    else if (mg.summon_type == SPELL_STICKS_TO_SNAKES)
        blame_prefix = "transmuted by ";
    else if (mg.cls == MONS_ELDRITCH_TENTACLE
             || mg.cls == MONS_ELDRITCH_TENTACLE_SEGMENT)
    {
        blame_prefix = "called by ";
    }
    else if (mons_is_child_tentacle(mg.cls))
        blame_prefix = "attached to ";
    else
        blame_prefix = "created by ";

    if (!mg.non_actor_summoner.empty())
        mons_add_blame(mon, blame_prefix + mg.non_actor_summoner);
    // NOTE: The summoner might be dead if the summoned is placed by a
    // beam which killed the summoner first (like fire vortexes placed
    // by the Fire Storm spell); a deceased summoner's mindex might also
    // be reused to create its summon, so make sure the summon doesn't
    // think it has summoned itself.
    else if (mg.summoner != NULL && mg.summoner->alive()
             && mg.summoner != mon)
    {
        ASSERT(mg.summoner->alive());
        mon->summoner = mg.summoner->mid;
        if (mg.summoner->is_player())
            mons_add_blame(mon, blame_prefix + "the player character");
        else
        {
            const monster* sum = mg.summoner->as_monster();
            mons_add_blame(mon, (blame_prefix
                                 + sum->full_name(DESC_A, true)));
            if (sum->props.exists("blame"))
            {
                const CrawlVector& oldblame = sum->props["blame"].get_vector();
                for (CrawlVector::const_iterator i = oldblame.begin();
                     i != oldblame.end(); ++i)
                {
                    mons_add_blame(mon, i->get_string());
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
        mon->set_ghost(ghost);
        mon->uglything_init();
    }
#if TAG_MAJOR_VERSION == 34
    else if (mon->type == MONS_LABORATORY_RAT)
        mon->type = MONS_RAT;
#endif
    else if (mons_class_is_animated_weapon(mon->type))
    {
        ghost_demon ghost;
        // We can't use monster::weapon here because it wants to look
        // at attack types, which are in the ghost structure we're
        // building.
        ASSERT(mon->mslot_item(MSLOT_WEAPON));
        if (mon->type == MONS_DANCING_WEAPON)
        {
            // Dancing weapons are placed at pretty high power.  Remember, the
            // player is fighting them one-on-one, while he will often summon
            // several.
            ghost.init_dancing_weapon(*(mon->mslot_item(MSLOT_WEAPON)),
                                      mg.props.exists(TUKIMA_POWER) ?
                                          mg.props[TUKIMA_POWER].get_int() : 100);
        }
        else
        {
            // Spectral weapons are placed at pretty high power.
            // They shouldn't ever be placed in a normal game.
            ghost.init_spectral_weapon(*(mon->mslot_item(MSLOT_WEAPON)),
                                       mg.props.exists(TUKIMA_POWER) ?
                                           mg.props[TUKIMA_POWER].get_int() : 100,
                                       mg.props.exists(TUKIMA_SKILL) ?
                                           mg.props[TUKIMA_SKILL].get_int() : 270);
        }
        mon->set_ghost(ghost);
        mon->ghost_demon_init();
    }
    else if (mons_class_is_chimeric(mon->type))
    {
        ghost_demon ghost;

        // Requires 3 parts
        if (mg.chimera_mons.size() != 3)
        {
            if (!ghost.init_chimera_for_place(mon, place, mg.cls, fpos))
            {
                mon->reset();
                return 0;
            }
        }
        else
        {
            monster_type parts[] =
            {
                mg.chimera_mons[0],
                mg.chimera_mons[1],
                mg.chimera_mons[2],
            };
            ghost.init_chimera(mon, parts);
        }
        mon->set_ghost(ghost);
        mon->ghost_demon_init();
    }

    tile_init_props(mon);

#ifndef DEBUG_DIAGNOSTICS
    // A rare case of a debug message NOT showing in the debug mode.
    if (mons_class_flag(mon->type, M_UNFINISHED))
    {
        mprf(MSGCH_WARN, "Warning: monster '%s' is not yet fully coded.",
             mon->name(DESC_PLAIN, true).c_str());
    }
#endif

    mark_interesting_monst(mon, mg.behaviour);

    if (crawl_state.game_is_arena())
        arena_placed_monster(mon);
    else if (!crawl_state.generating_level && !dont_place && you.can_see(mon))
    {
        if (mg.flags & MG_DONT_COME)
            mon->seen_context = SC_JUST_SEEN;
        // FIXME: This causes "comes into view" messages at the
        //        wrong time, since code checks for placement
        //        success before printing messages.
        handle_seen_interrupt(mon);
    }

    // Area effects can produce additional messages, and thus need to be
    // done after come in view ones.
    if (mon->type == MONS_TWISTER && !dont_place)
        _place_twister_clouds(mon);

    return mon;
}

monster_type pick_random_zombie()
{
    static vector<monster_type> zombifiable;

    if (zombifiable.empty())
    {
        for (monster_type mcls = MONS_0; mcls < NUM_MONSTERS; ++mcls)
        {
            if (mons_species(mcls) != mcls || mcls == MONS_PROGRAM_BUG)
                continue;

            if (!mons_zombie_size(mcls) || mons_is_unique(mcls))
                continue;
            if (mons_class_holiness(mcls) != MH_NATURAL)
                continue;

            zombifiable.push_back(mcls);
        }

        ASSERT(!zombifiable.empty());
    }

    return (zombifiable[random2(zombifiable.size())]);
}

// Check base monster class against zombie type and position if set.
static bool _good_zombie(monster_type base, monster_type cs,
                         const coord_def& pos)
{
    // Actually pick a monster that is happy where we want to put it.
    // Fish zombies on land are helpless and uncool.
    if (in_bounds(pos) && !monster_habitable_grid(base, grd(pos)))
        return false;

    if (cs == MONS_NO_MONSTER)
        return true;

    // If skeleton, monster must have a skeleton.
    if (cs == MONS_SKELETON && !mons_skeleton(base))
        return false;

    // If zombie, monster must have unrotted meat.
    if (cs == MONS_ZOMBIE && !mons_zombifiable(base))
        return false;

    return true;
}

// Veto for the zombie picker class
bool zombie_picker::veto(monster_type mt)
{
    // Zombifiability in general.
    if (mons_species(mt) != mt)
        return true;
    // Monsters that don't really exist
    if (mons_class_flag(mt, M_UNFINISHED))
        return true;
    // Monsters that can have derived undead, but never randomly generated.
    if (mons_class_flag(mt, M_NO_GEN_DERIVED))
        return true;
    if (!mons_zombie_size(mt) || mons_is_unique(mt))
        return true;
    if (mons_class_holiness(mt) != MH_NATURAL)
        return true;

    return !_good_zombie(mt, zombie_kind, pos);
}

monster_type pick_local_zombifiable_monster(level_id place,
                                            monster_type cs,
                                            const coord_def& pos)
{
    if (crawl_state.game_is_zotdef())
    {
        place = level_id(BRANCH_DUNGEON,
                         you.num_turns / (2 * ZOTDEF_CYCLE_LENGTH) + 6);
    }
    else if (place.branch == BRANCH_ZIGGURAT)
    {
        // Get Zigs something reasonable to work with, if there's no place
        // explicitly defined.
        place = level_id(BRANCH_DUNGEON, 31 - (27 - place.depth) / 3);
    }
    else
    {
        // Zombies tend to be weaker than their normal counterparts;
        // thus, make them OOD proportional to the current dungeon depth.
        place.depth += 1 + div_rand_round(place.absdepth(), 5);
    }

    zombie_picker picker = zombie_picker(pos, cs);

    place.depth = max(1, min(place.depth, branch_ood_cap(place.branch)));

    if (monster_type mt = pick_monster(place, picker))
        return mt;

    return pick_monster_all_branches(place.absdepth(), picker);
}

void roll_zombie_hp(monster* mon)
{
    ASSERT(mons_class_is_zombified(mon->type));

    int hp = 0;

    switch (mon->type)
    {
    case MONS_ZOMBIE:
        hp = hit_points(mon->hit_dice, 6, 5);
        break;

    case MONS_SKELETON:
        hp = hit_points(mon->hit_dice, 5, 4);
        break;

    case MONS_SIMULACRUM:
        // Simulacra aren't tough, but you can create piles of them. - bwr
        hp = hit_points(mon->hit_dice, 1, 4);
        break;

    case MONS_SPECTRAL_THING:
        hp = hit_points(mon->hit_dice, 4, 4);
        break;

    default:
        die("invalid zombie type %d (%s)", mon->type,
            mons_class_name(mon->type));
    }

    mon->max_hit_points = max(hp, 1);
    mon->hit_points     = mon->max_hit_points;
}

static void _roll_zombie_ac_ev_mods(monster* mon, int& acmod, int& evmod)
{
    ASSERT(mons_class_is_zombified(mon->type));

    switch (mon->type)
    {
    case MONS_ZOMBIE:
        acmod = -2;
        evmod = -5;
        break;

    case MONS_SKELETON:
        acmod = -6;
        evmod = -7;
        break;

    case MONS_SIMULACRUM:
        // Simulacra aren't tough, but you can create piles of them. - bwr
        acmod = -2;
        evmod = -5;
        break;

    case MONS_SPECTRAL_THING:
        acmod = +2;
        evmod = -5;
        break;

    default:
        die("invalid zombie type %d (%s)", mon->type,
            mons_class_name(mon->type));
    }
}

static void _roll_zombie_ac_ev(monster* mon)
{
    ASSERT(mons_class_is_zombified(mon->type));

    int acmod = 0;
    int evmod = 0;

    _roll_zombie_ac_ev_mods(mon, acmod, evmod);

    mon->ac = max(mon->ac + acmod, 0);
    mon->ev = max(mon->ev + evmod, 0);
}

void define_zombie(monster* mon, monster_type ztype, monster_type cs)
{
#if TAG_MAJOR_VERSION == 34
    // Upgrading monster enums is a losing battle, they sneak through too many
    // channels, like env props, etc.  So convert them on placement, too.
    if (cs == MONS_ZOMBIE_SMALL || cs == MONS_ZOMBIE_LARGE)
        cs = MONS_ZOMBIE;
    if (cs == MONS_SKELETON_SMALL || cs == MONS_SKELETON_LARGE)
        cs = MONS_SKELETON;
    if (cs == MONS_SIMULACRUM_SMALL || cs == MONS_SIMULACRUM_LARGE)
        cs = MONS_SIMULACRUM;
#endif

    ASSERT(ztype != MONS_NO_MONSTER);
    ASSERT(!invalid_monster_type(ztype));
    ASSERT(mons_class_is_zombified(cs));

    monster_type base = mons_species(ztype);

    // Set type to the original type to calculate appropriate stats.
    mon->type         = ztype;
    mon->base_monster = MONS_PROGRAM_BUG;
    define_monster(mon);

    mon->type         = cs;
    mon->base_monster = base;

    mon->colour       = mons_class_colour(mon->type);
    mon->speed        = (cs == MONS_SPECTRAL_THING
                            ? mons_class_base_speed(mon->base_monster)
                            : mons_class_zombie_base_speed(mon->base_monster));

    // Turn off all melee ability flags except dual-wielding.
    mon->flags       &= (~MF_MELEE_MASK | MF_TWO_WEAPONS);

    // Turn off all spellcasting and priestly ability flags.
    // Hack - kraken get to keep their spell-like ability.
    if (mon->base_monster != MONS_KRAKEN)
        mon->flags   &= ~MF_SPELL_MASK;

    // Turn off regeneration if the base monster cannot regenerate.
    // This is needed for e.g. spectral things of non-regenerating
    // monsters.
    if (!mons_class_can_regenerate(mon->base_monster))
        mon->flags   |= MF_NO_REGEN;

    roll_zombie_hp(mon);
    _roll_zombie_ac_ev(mon);
}

bool downgrade_zombie_to_skeleton(monster* mon)
{
    if (mon->type != MONS_ZOMBIE || !mons_skeleton(mon->base_monster))
        return false;

    int acmod = 0;
    int evmod = 0;

    _roll_zombie_ac_ev_mods(mon, acmod, evmod);

    // Reverse the zombie AC and EV mods, since they will be replaced
    // with the skeleton AC and EV mods below.
    mon->ac = max(mon->ac - acmod, 0);
    mon->ev = max(mon->ev - evmod, 0);

    const int old_hp    = mon->hit_points;
    const int old_maxhp = mon->max_hit_points;

    mon->type           = MONS_SKELETON;
    mon->colour         = mons_class_colour(mon->type);
    mon->speed          = mons_class_zombie_base_speed(mon->base_monster);

    roll_zombie_hp(mon);
    _roll_zombie_ac_ev(mon);

    // Scale the skeleton HP to the zombie HP.
    mon->hit_points     = old_hp * mon->max_hit_points / old_maxhp;
    mon->hit_points     = max(mon->hit_points, 1);

    return true;
}

static band_type _choose_band(monster_type mon_type, int &band_size,
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
        if (env.absdepth0 > 3)
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
        band_size = 1 + random2(2);
        // intentional fall-through
    case MONS_NECROMANCER:
        natural_leader = true;
        band = BAND_NECROMANCER;
        band_size += 3 + random2(3);
        break;
    case MONS_VAMPIRE_MAGE:
        if (one_chance_in(3))
        {
            natural_leader = true;
            band = BAND_JIANGSHI;
            band_size = 2 + random2(2);
        }
        break;
    case MONS_JIANGSHI:
            band = BAND_JIANGSHI;
            band_size = random2(3);
        break;
    case MONS_GNOLL:
        if (!player_in_branch(BRANCH_DUNGEON) || you.depth > 1)
        {
            band = BAND_GNOLLS;
            band_size = (coinflip() ? 3 : 2);
        }
        break;
    case MONS_GNOLL_SHAMAN:
    case MONS_GNOLL_SERGEANT:
        band = BAND_GNOLLS;
        band_size = 3 + random2(4);
        break;
    case MONS_DEEP_DWARF_DEATH_KNIGHT:
        if (x_chance_in_y(2, 3))
        {
            natural_leader = true;
            band = BAND_DEATH_KNIGHT;
            band_size = 3 + random2(2);
        }
        break;
    case MONS_GRUM:
        natural_leader = true;
        band = BAND_WOLVES;
        band_size = 2 + random2(3);
        break;
    case MONS_CENTAUR_WARRIOR:
        natural_leader = true;
    case MONS_CENTAUR:
        if (env.absdepth0 > 9 && one_chance_in(3) && !player_in_branch(BRANCH_SHOALS))
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
        band = BAND_BALRUG;
        band_size = 2 + random2(3);
        break;
    case MONS_CACODEMON:
        natural_leader = true;
        band = BAND_CACODEMON;
        band_size = 1 + random2(3);
        break;

    case MONS_EXECUTIONER:
        if (coinflip())
        {
            natural_leader = true;
            band = BAND_EXECUTIONER;
            band_size = 1 + random2(3);
        }
        break;

    case MONS_PANDEMONIUM_LORD:
        natural_leader = true;
        band = BAND_PANDEMONIUM_LORD;
        band_size = random_range(1, 3);
        break;

    case MONS_HELLWING:
        if (coinflip())
        {
            band = BAND_HELLWING;
            band_size = 1 + random2(4);
        }
        break;

    case MONS_DEEP_ELF_FIGHTER:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_FIGHTER;
            band_size = 2 + random2(3);
        }
        break;

    case MONS_DEEP_ELF_KNIGHT:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_KNIGHT;
            band_size = 3 + random2(2);
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

    case MONS_WOLF:
        band = BAND_WOLVES;
        band_size = 2 + random2(4);
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

    case MONS_VAMPIRE_MOSQUITO:
        band = BAND_VAMPIRE_MOSQUITOES;
        band_size = 1 + random2(3);
        break;

    case MONS_FIRE_BAT:
        band = BAND_FIRE_BATS;
        band_size = 1 + random2(3);
        break;

    case MONS_DEEP_TROLL_EARTH_MAGE:
    case MONS_DEEP_TROLL_SHAMAN:
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

    case MONS_PRINCE_RIBBIT:
        natural_leader = true;
        // Intentional fallthrough
    case MONS_BLINK_FROG:
        band = BAND_BLINK_FROGS;
        band_size += 2 + random2(3);
        break;

    case MONS_WIGHT:
        band = BAND_WIGHTS;
        band_size = 2 + random2(3);
        break;

    case MONS_ANCIENT_CHAMPION:
        if (coinflip())
            break;

        natural_leader = true;
        // Intentional fallthrough
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
        band = BAND_POLYPHEMUS;
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
    case MONS_GREY_DRACONIAN:
    case MONS_PALE_DRACONIAN:
        if (env.absdepth0 > 18 && one_chance_in(3) && player_in_connected_branch())
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
        if (env.absdepth0 > 20 && player_in_connected_branch())
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

    case MONS_ELEPHANT:
        band = BAND_ELEPHANT;
        band_size = 2 + random2(4);
        break;

    case MONS_SHEDU:
        band = BAND_SHEDU;
        band_size = 1;
        break;

    case MONS_REDBACK:
        band = BAND_REDBACK;
        band_size = 1 + random2(5);
        break;

    case MONS_SPIDER:
        band = BAND_SPIDER;
        band_size = 1 + random2(4);
        break;

    case MONS_JUMPING_SPIDER:
        if (coinflip())
        {
            band = BAND_JUMPING_SPIDER;
            band_size = 1 + random2(5);
        }
        break;

    case MONS_TARANTELLA:
        if (coinflip())
        {
            band = BAND_TARANTELLA;
            band_size = 1 + random2(4);
        }
        break;

    case MONS_VAULT_WARDEN:
        natural_leader = true;
        if (coinflip())
        {
            band = BAND_YAKTAURS;
            band_size = 2 + random2(4);
        }
        else
        {
            band = BAND_VAULT_WARDEN;
            band_size = 2 + random2(3);
        }
        break;

    case MONS_IRONHEART_PRESERVER:
        natural_leader = true;
        switch (random2(3))
        {
            case 0:
                band = BAND_DEEP_ELF_HIGH_PRIEST;
                band_size = 3 + random2(4);
                break;
            case 1:
                band = BAND_DEEP_TROLLS;
                band_size = 3 + random2(3);
                break;
            case 2:
                band = BAND_OGRE_MAGE_EXTERN;
                band_size = 4 + random2(4);
                break;
        }
        break;

    case MONS_SATYR:
        natural_leader = true;
    case MONS_FAUN:
        band = BAND_FAUNS;
        band_size = 2 + random2(3);
        break;

    case MONS_TENGU_CONJURER:
    case MONS_TENGU_WARRIOR:
        natural_leader = true;
    case MONS_TENGU:
        if (coinflip())
            break;
        band = BAND_TENGU;
        band_size = 2 + random2(3);
        break;

    case MONS_SOJOBO:
        natural_leader = true;
        band = BAND_SOJOBO;
        band_size = 2;
        break;

    case MONS_SPRIGGAN_AIR_MAGE:
        natural_leader = true;
        band = BAND_AIR_ELEMENTALS;
        band_size = random_range(2, 3);
        break;

    case MONS_SPRIGGAN_RIDER:
        band = BAND_SPRIGGAN_RIDERS;
        band_size = random_range(2, 3);
        break;

    case MONS_SPRIGGAN_DRUID:
    case MONS_SPRIGGAN_BERSERKER:
        if (one_chance_in(3))
            break;
        natural_leader = true;
    case MONS_SPRIGGAN:
        band = BAND_SPRIGGANS;
        band_size = 2 + random2(3);
        break;

    case MONS_SPRIGGAN_DEFENDER:
        natural_leader = true;
        band = BAND_SPRIGGAN_ELITES;
        band_size = 2 + random2(4);
        break;

    case MONS_THE_ENCHANTRESS:
        natural_leader = true;
        band = BAND_ENCHANTRESS;
        band_size = 6 + random2avg(5, 2);
        break;

    case MONS_TREANT:
        if (one_chance_in(4))
        {
            band = BAND_SPRIGGAN_DRUID;
            band_size = 1;
        }
        break;

    case MONS_SPIRIT_WOLF:
        band = BAND_SPIRIT_WOLVES;
        band_size = random_range(0, 2);
        break;

    case MONS_VAMPIRE_KNIGHT:
        if (one_chance_in(4))
        {
            band = BAND_PHANTASMAL_WARRIORS;
            band_size = 2;
        }
        break;

    case MONS_ELEMENTAL_WELLSPRING:
        natural_leader = true;
        band = BAND_WATER_ELEMENTALS;
        band_size = random_range(2, 3);
        break;

    case MONS_THRASHING_HORROR:
    {
        int depth = min(brdepth[BRANCH_ABYSS], you.depth);
        band = BAND_THRASHING_HORRORS;
        band_size = random2(depth);
        break;
    }

    default: ;
    }

    if (band != BAND_NO_BAND && band_size == 0)
        band = BAND_NO_BAND;

    if (band_size >= BIG_BAND)
        band_size = BIG_BAND - 1;

    return band;
}

static monster_type _band_member(band_type band, int which)
{
    if (band == BAND_NO_BAND)
        return MONS_PROGRAM_BUG;

    switch (band)
    {
    case BAND_KOBOLDS:
        return MONS_KOBOLD;

    case BAND_ORCS:
        if (one_chance_in(8)) // 12.50%
            return MONS_ORC_PRIEST;
        if (one_chance_in(6)) // 14.58%
            return MONS_ORC_WIZARD;
        return MONS_ORC;

    case BAND_ORC_WARRIOR:
        if (one_chance_in(7)) // 14.29%
            return MONS_ORC_PRIEST;
        if (one_chance_in(5)) // 17.14%
            return MONS_ORC_WIZARD;
        return MONS_ORC;

    case BAND_ORC_KNIGHT:
    case BAND_ORC_HIGH_PRIEST:
        // XXX: For Beogh punishment, ogres and trolls look out of place...
        // (For normal generation, they're okay, of course.)
        return random_choose_weighted(12, MONS_ORC,
                                       9, MONS_ORC_WARRIOR,
                                       2, MONS_WARG,
                                       2, MONS_ORC_WIZARD,
                                       2, MONS_ORC_PRIEST,
                                       1, MONS_OGRE,
                                       1, MONS_TROLL,
                                       1, MONS_ORC_SORCERER,
                                       0);

    case BAND_KILLER_BEES:
        return MONS_KILLER_BEE;

    case BAND_FLYING_SKULLS:
        return MONS_FLYING_SKULL;

    case BAND_SLIME_CREATURES:
        return MONS_SLIME_CREATURE;

    case BAND_YAKS:
        return MONS_YAK;

    case BAND_HARPIES:
        return MONS_HARPY;

    case BAND_UGLY_THINGS:
        return (env.absdepth0 > 21 && one_chance_in(4)) ?
                   MONS_VERY_UGLY_THING : MONS_UGLY_THING;

    case BAND_HELL_HOUNDS:
        return MONS_HELL_HOUND;

    case BAND_JACKALS:
        return MONS_JACKAL;

    case BAND_GNOLLS:
        return MONS_GNOLL;

    case BAND_CENTAURS:
        return MONS_CENTAUR;

    case BAND_YAKTAURS:
        return MONS_YAKTAUR;

    case BAND_INSUBSTANTIAL_WISPS:
        return MONS_INSUBSTANTIAL_WISP;

    case BAND_POLYPHEMUS:
        if (which == 1)
            return MONS_CATOBLEPAS;
    case BAND_DEATH_YAKS:
        return MONS_DEATH_YAK;

    case BAND_NECROMANCER:
        return random_choose_weighted(6, MONS_ZOMBIE,
                                      6, MONS_SKELETON,
                                      3, MONS_SIMULACRUM,
                                      0);

    case BAND_BALRUG:
        return coinflip() ? MONS_SUN_DEMON : MONS_RED_DEVIL;

    case BAND_CACODEMON:
        return coinflip() ? MONS_SIXFIRHY : MONS_ORANGE_DEMON;
        break;

    case BAND_EXECUTIONER:
        return MONS_ABOMINATION_LARGE;
        break;

    case BAND_PANDEMONIUM_LORD:
        if (one_chance_in(7))
        {
            return random_choose_weighted(50, MONS_LICH,
                                          10, MONS_ANCIENT_LICH,
                                           0);
        }
        else if (one_chance_in(6))
        {
            return random_choose_weighted(50, MONS_ABOMINATION_SMALL,
                                          40, MONS_ABOMINATION_LARGE,
                                          10, MONS_TENTACLED_MONSTROSITY,
                                           0);
        }
        else
        {
            return summon_any_demon(random_choose_weighted(
                                          50, RANDOM_DEMON_COMMON,
                                          20, RANDOM_DEMON_GREATER,
                                          10, RANDOM_DEMON,
                                           0));
        }
        break;

    case BAND_HELLWING:
        return coinflip() ? MONS_HELLWING : MONS_SMOKE_DEMON;

    case BAND_DEEP_ELF_FIGHTER:
        return random_choose_weighted(3, MONS_DEEP_ELF_FIGHTER,
                                      3, MONS_DEEP_ELF_MAGE,
                                      2, MONS_DEEP_ELF_PRIEST,
                                      1, MONS_DEEP_ELF_CONJURER,
                                      0);

    case BAND_DEEP_ELF_KNIGHT:
        return random_choose_weighted(66, MONS_DEEP_ELF_FIGHTER,
                                      52, MONS_DEEP_ELF_MAGE,
                                      28, MONS_DEEP_ELF_KNIGHT,
                                      20, MONS_DEEP_ELF_CONJURER,
                                      16, MONS_DEEP_ELF_PRIEST,
                                      12, MONS_DEEP_ELF_SUMMONER,
                                       3, MONS_DEEP_ELF_DEATH_MAGE,
                                       2, MONS_DEEP_ELF_DEMONOLOGIST,
                                       2, MONS_DEEP_ELF_ANNIHILATOR,
                                       2, MONS_DEEP_ELF_SORCERER,
                                       0);

    case BAND_DEEP_ELF_HIGH_PRIEST:
        return random_choose_weighted(5, MONS_DEEP_ELF_FIGHTER,
                                      2, MONS_DEEP_ELF_PRIEST,
                                      2, MONS_DEEP_ELF_MAGE,
                                      1, MONS_DEEP_ELF_SUMMONER,
                                      1, MONS_DEEP_ELF_CONJURER,
                                      1, MONS_DEEP_ELF_DEMONOLOGIST,
                                      1, MONS_DEEP_ELF_ANNIHILATOR,
                                      1, MONS_DEEP_ELF_SORCERER,
                                      1, MONS_DEEP_ELF_DEATH_MAGE,
                                      0);

    case BAND_HELL_KNIGHTS:
        if (one_chance_in(4))
            return MONS_NECROMANCER;
        return MONS_HELL_KNIGHT;

    case BAND_OGRE_MAGE_EXTERN:
        if (which == 1)
            return MONS_OGRE_MAGE;
        // Deliberate fallthrough
    case BAND_OGRE_MAGE:
        if (one_chance_in(3))
            return MONS_TWO_HEADED_OGRE;
        return MONS_OGRE;

    case BAND_KOBOLD_DEMONOLOGIST:
        return random_choose_weighted(8, MONS_KOBOLD,
                                          4, MONS_BIG_KOBOLD,
                                          1, MONS_KOBOLD_DEMONOLOGIST,
                                          0);
        break;

    case BAND_NAGAS:
        return MONS_NAGA;
    case BAND_WOLVES:
        return MONS_WOLF;
    case BAND_GREEN_RATS:
        return MONS_GREEN_RAT;
    case BAND_ORANGE_RATS:
        return MONS_ORANGE_RAT;
    case BAND_SHEEP:
        return MONS_SHEEP;
    case BAND_GHOULS:
        return random_choose_weighted(4, MONS_GHOUL,
                                      3, MONS_NECROPHAGE,
                                      2, MONS_PLAGUE_SHAMBLER,
                                      0);
    case BAND_DEEP_TROLLS:
        if (one_chance_in(3))
            return random_choose(MONS_DEEP_TROLL_EARTH_MAGE,
                                 MONS_DEEP_TROLL_SHAMAN,
                                 -1);
        return MONS_DEEP_TROLL;
    case BAND_HOGS:
        return MONS_HOG;
    case BAND_HELL_HOGS:
        return MONS_HELL_HOG;
    case BAND_VAMPIRE_MOSQUITOES:
        return MONS_VAMPIRE_MOSQUITO;
    case BAND_FIRE_BATS:
        return MONS_FIRE_BAT;
    case BAND_BOGGARTS:
        return MONS_BOGGART;
    case BAND_BLINK_FROGS:
        return MONS_BLINK_FROG;
    case BAND_WIGHTS:
        return MONS_WIGHT;
    case BAND_SKELETAL_WARRIORS:
        return MONS_SKELETAL_WARRIOR;

    case BAND_DRACONIAN:
        if (env.absdepth0 >= 24 && x_chance_in_y(13, 40))
        {
            // Hack: race is rolled elsewhere.
            return random_choose_weighted(
                1, MONS_DRACONIAN_CALLER,
                2, MONS_DRACONIAN_KNIGHT,
                2, MONS_DRACONIAN_MONK,
                2, MONS_DRACONIAN_SHIFTER,
                2, MONS_DRACONIAN_ANNIHILATOR,
                2, MONS_DRACONIAN_SCORCHER,
                2, MONS_DRACONIAN_ZEALOT,
                0);
        }

        return random_draconian_monster_species();

    case BAND_ILSUIW:
        return random_choose_weighted(30, MONS_MERMAID,
                                      15, MONS_MERFOLK,
                                      10, MONS_MERFOLK_JAVELINEER,
                                      10, MONS_MERFOLK_IMPALER,
                                       0);

    case BAND_AZRAEL:
        return coinflip() ? MONS_FIRE_ELEMENTAL : MONS_HELL_HOUND;

    case BAND_DUVESSA:
        return MONS_DOWAN;

    case BAND_ALLIGATOR:
        return MONS_BABY_ALLIGATOR;

    case BAND_KHUFU:
        return coinflip() ? MONS_GREATER_MUMMY : MONS_MUMMY;

    case BAND_GOLDEN_EYE:
        return MONS_GOLDEN_EYE;

    case BAND_PIKEL:
        return MONS_SLAVE;

    case BAND_MERFOLK_AQUAMANCER:
        return random_choose_weighted( 8, MONS_MERFOLK,
                                      10, MONS_WATER_ELEMENTAL,
                                       0);

    case BAND_MERFOLK_IMPALER:
    case BAND_MERFOLK_JAVELINEER:
        return MONS_MERFOLK;

    case BAND_ELEPHANT:
        return MONS_ELEPHANT;

    case BAND_SHEDU:
        return MONS_SHEDU;

    case BAND_REDBACK:
        return random_choose_weighted(30, MONS_REDBACK,
                                       5, MONS_TARANTELLA,
                                       5, MONS_JUMPING_SPIDER,
                                       0);

    case BAND_SPIDER:
        return MONS_SPIDER;

    case BAND_JUMPING_SPIDER:
        return random_choose_weighted(12, MONS_JUMPING_SPIDER,
                                       8, MONS_WOLF_SPIDER,
                                       7, MONS_ORB_SPIDER,
                                       6, MONS_SPIDER,
                                       5, MONS_REDBACK,
                                       2, MONS_DEMONIC_CRAWLER,
                                       0);

    case BAND_TARANTELLA:
        return random_choose_weighted(10, MONS_TARANTELLA,
                                       7, MONS_WOLF_SPIDER,
                                       3, MONS_ORB_SPIDER,
                                       8, MONS_REDBACK,
                                      10, MONS_SPIDER,
                                       2, MONS_DEMONIC_CRAWLER,
                                       0);

    case BAND_VAULT_WARDEN:
        if (which == 1 || which == 2 && coinflip())
            return random_choose_weighted( 8, MONS_VAULT_SENTINEL,
                                          12, MONS_IRONBRAND_CONVOKER,
                                          10, MONS_IRONHEART_PRESERVER,
                                           0);
        else
            return MONS_VAULT_GUARD;

    case BAND_DEATH_KNIGHT:
        if (which == 1 && x_chance_in_y(2, 3))
            return (one_chance_in(3) ? MONS_GHOUL : MONS_FLAYED_GHOST);
        else
            return random_choose_weighted(5, MONS_WRAITH,
                                          6, MONS_FREEZING_WRAITH,
                                          3, MONS_PHANTASMAL_WARRIOR,
                                          2, MONS_FLAMING_CORPSE,
                                          3, MONS_SKELETAL_WARRIOR,
                                          0);

    case BAND_JIANGSHI:
        return MONS_JIANGSHI;

    case BAND_FAUNS:
        return MONS_FAUN;

    case BAND_TENGU:
        return MONS_TENGU;

    case BAND_SOJOBO:
        return MONS_TENGU_REAVER;

    case BAND_ENCHANTRESS:
        if (which <= 3)
            return MONS_SPRIGGAN_DEFENDER;
        else if (which <= 4)
            return MONS_SPRIGGAN_AIR_MAGE;
        if (coinflip())
        {
            return random_choose(MONS_SPRIGGAN_AIR_MAGE,
                                 MONS_SPRIGGAN_BERSERKER,
                                 MONS_SPRIGGAN_RIDER,
                                 -1);
        }
        return MONS_SPRIGGAN;
    case BAND_SPRIGGAN_ELITES:
        if (which == 1 && one_chance_in(3))
            return (coinflip() ? MONS_SPRIGGAN_ENCHANTER : MONS_SPRIGGAN_ASSASSIN);
    case BAND_SPRIGGANS:
        if ((band == BAND_SPRIGGAN_ELITES && which <= 2)
            || one_chance_in(4))
        {
            return random_choose_weighted(5, MONS_SPRIGGAN_AIR_MAGE,
                                          3, MONS_SPRIGGAN_BERSERKER,
                                          1, MONS_SPRIGGAN_DRUID,
                                          2, MONS_SPRIGGAN_RIDER,
                                          0);
        }
        return MONS_SPRIGGAN;

    case BAND_AIR_ELEMENTALS:
        return MONS_AIR_ELEMENTAL;

    case BAND_SPRIGGAN_DRUID:
        return MONS_SPRIGGAN_DRUID;

    case BAND_SPRIGGAN_RIDERS:
        return MONS_SPRIGGAN_RIDER;

    case BAND_SPIRIT_WOLVES:
        return MONS_SPIRIT_WOLF;

    case BAND_PHANTASMAL_WARRIORS:
        return MONS_PHANTASMAL_WARRIOR;

    case BAND_WATER_ELEMENTALS:
        return MONS_WATER_ELEMENTAL;

    case BAND_THRASHING_HORRORS:
        return MONS_THRASHING_HORROR;

    default:
        die("unhandled band type %d", band);
    }
}

void mark_interesting_monst(monster* mons, beh_type behaviour)
{
    if (crawl_state.game_is_arena())
        return;

    bool interesting = false;

    // Unique monsters are always interesting
    if (mons_is_unique(mons->type))
        interesting = true;
    // If it's never going to attack us, then not interesting
    else if (behaviour == BEH_FRIENDLY)
        interesting = false;
    // Jellies are never interesting to Jiyva.
    else if (mons->type == MONS_JELLY && you_worship(GOD_JIYVA))
        interesting = false;
    else if (mons_threat_level(mons) == MTHRT_NASTY)
        interesting = true;
    // Don't waste time on moname() if user isn't using this option
    else if (!Options.note_monsters.empty())
    {
        const string iname = mons_type_name(mons->type, DESC_A);
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
        mons->flags |= MF_INTERESTING;
}

// PUBLIC FUNCTION -- mons_place().

static monster_type _pick_zot_exit_defender()
{
    if (one_chance_in(11))
    {
#ifdef DEBUG_MON_CREATION
        mpr("Create a pandemonium lord!", MSGCH_DIAGNOSTICS);
#endif
        for (int i = 0; i < 4; i++)
        {
            // Sometimes pick an unique lord whose rune you've stolen.
            //
            if (you.runes[RUNE_MNOLEG + i]
                && !you.unique_creatures[MONS_MNOLEG + i]
                && one_chance_in(10))
            {
                return static_cast<monster_type>(MONS_MNOLEG + i);
            }
        }
        return MONS_PANDEMONIUM_LORD;
    }

    return random_choose_weighted(
        30, RANDOM_DEMON_COMMON,
        30, RANDOM_DEMON,
        20, pick_monster_no_rarity(BRANCH_PANDEMONIUM),
        15, MONS_ORB_GUARDIAN,
        5, RANDOM_DEMON_GREATER,
        0);
}

monster* mons_place(mgen_data mg)
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
            return 0;

#ifdef DEBUG_MON_CREATION
        mpr("Set class RANDOM_MONSTER", MSGCH_DIAGNOSTICS);
#endif
        mg.cls = RANDOM_MONSTER;
    }

    // All monsters have been assigned? {dlb}
    if (mon_count >= MAX_MONSTERS - 1)
        return 0;

    // This gives a slight challenge to the player as they ascend the
    // dungeon with the Orb.
    if (you.char_direction == GDT_ASCENDING && _is_random_monster(mg.cls)
        && player_in_connected_branch() && !mg.summoned())
    {
#ifdef DEBUG_MON_CREATION
        mpr("Call _pick_zot_exit_defender()", MSGCH_DIAGNOSTICS);
#endif
        mg.cls    = _pick_zot_exit_defender();
        mg.flags |= MG_PERMIT_BANDS;
    }
    else if (_is_random_monster(mg.cls))
        mg.flags |= MG_PERMIT_BANDS;

    if (crawl_state.game_is_zotdef()) // check if emulation of old mg.power is there
        ASSERT(mg.place.is_valid());

    if (mg.behaviour == BEH_COPY)
    {
        mg.behaviour = (mg.summoner && mg.summoner->is_player())
                        ? BEH_FRIENDLY
                        : SAME_ATTITUDE((&menv[mg.summoner->mindex()]));
    }

    monster* creation = place_monster(mg);
    if (!creation)
        return 0;

    dprf(DIAG_MONPLACE, "Created %s.", creation->base_name(DESC_A, true).c_str());

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

    return creation;
}

static dungeon_feature_type _monster_primary_habitat_feature(monster_type mc)
{
    if (_is_random_monster(mc))
        return DNGN_FLOOR;
    return habitat2grid(mons_class_primary_habitat(mc));
}

static dungeon_feature_type _monster_secondary_habitat_feature(monster_type mc)
{
    if (_is_random_monster(mc))
        return DNGN_FLOOR;
    return habitat2grid(mons_class_secondary_habitat(mc));
}

static bool _valid_spot(coord_def pos)
{
    if (actor_at(pos))
        return false;
    if (env.level_map_mask(pos) & MMT_NO_MONS)
        return false;
    return true;
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
    set<dungeon_feature_type> passable;
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
        return travel_pathfind::pathfind(RMODE_CONNECTIVITY);
    }

    bool path_flood(const coord_def &c, const coord_def &dc)
    {
        if (best_distance && traveled_distance > best_distance)
            return true;

        if (!in_bounds(dc)
            || (maxdistance > 0 && traveled_distance > maxdistance))
        {
            return false;
        }
        if (!feat_compatible(feat_wanted, grd(dc)))
        {
            if (passable.find(grd(dc)) != passable.end())
                good_square(dc);
            return false;
        }
        if (_valid_spot(dc) && one_chance_in(++nfound))
        {
            greedy_dist = traveled_distance;
            greedy_place = dc;
            best_distance = traveled_distance;
        }
        else
            good_square(dc);
        return false;
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

coord_def find_newmons_square(monster_type mons_class, const coord_def &p)
{
    coord_def empty;
    coord_def pos(-1, -1);

    if (mons_class == WANDERING_MONSTER)
        mons_class = RANDOM_MONSTER;

    // Might be better if we chose a space and tried to match the monster
    // to it in the case of RANDOM_MONSTER, that way if the target square
    // is surrounded by water or lava this function would work.  -- bwr

    if (find_habitable_spot_near(p, mons_class, 2, true, empty))
        pos = empty;

    return pos;
}

bool can_spawn_mushrooms(coord_def where)
{
    int cl = env.cgrid(where);
    if (cl == EMPTY_CLOUD)
        return true;

    cloud_struct &cloud = env.cloud[env.cgrid(where)];
    if (you_worship(GOD_FEDHAS)
        && (cloud.whose == KC_YOU || cloud.whose == KC_FRIENDLY))
    {
        return true;
    }

    return is_harmless_cloud(cloud.type);
}

conduct_type player_will_anger_monster(monster_type type)
{
    monster dummy;
    dummy.type = type;

    // no spellcasting/etc zombies currently
    if (!mons_class_is_zombified(type))
        define_monster(&dummy);

    return player_will_anger_monster(&dummy);
}

conduct_type player_will_anger_monster(monster* mon)
{
    if (is_good_god(you.religion) && mon->is_unholy())
        return DID_UNHOLY;
    if (is_good_god(you.religion) && mon->is_evil())
        return DID_NECROMANCY;
    if (you_worship(GOD_FEDHAS)
        && ((mon->holiness() == MH_UNDEAD && !mon->is_insubstantial())
            || mon->has_corpse_violating_spell()))
    {
        return DID_CORPSE_VIOLATION;
    }
    if (is_evil_god(you.religion) && mon->is_holy())
        return DID_HOLY;
    if (you_worship(GOD_ZIN))
    {
        if (mon->is_unclean())
            return DID_UNCLEAN;
        if (mon->is_chaotic())
            return DID_CHAOS;
    }
    if (you_worship(GOD_TROG) && mon->is_actual_spellcaster())
        return DID_SPELL_CASTING;

    return DID_NOTHING;
}

bool player_angers_monster(monster* mon)
{
    // Get the drawbacks, not the benefits... (to prevent e.g. demon-scumming).
    conduct_type why = player_will_anger_monster(mon);
    if (why && mon->wont_attack())
    {
        mon->attitude = ATT_HOSTILE;
        mon->del_ench(ENCH_CHARM);
        behaviour_event(mon, ME_ALERT, &you);

        if (you.can_see(mon))
        {
            const string mname = mon->name(DESC_THE).c_str();

            switch (why)
            {
            case DID_UNHOLY:
            case DID_NECROMANCY:
                mprf("%s is enraged by your holy aura!", mname.c_str());
                break;
            case DID_CORPSE_VIOLATION:
                mprf("%s is revulsed by your support of nature!", mname.c_str());
                break;
            case DID_HOLY:
                mprf("%s is enraged by your evilness!", mname.c_str());
                break;
            case DID_UNCLEAN:
            case DID_CHAOS:
                mprf("%s is enraged by your lawfulness!", mname.c_str());
                break;
            case DID_SPELL_CASTING:
                mprf("%s is enraged by your magic-hating god!", mname.c_str());
                break;
            default:
                mprf("%s is enraged by a buggy thing about you!", mname.c_str());
                break;
            }
        }

        // Anger a shedu's mate.  This won't be an infinite recursion
        // because the original is already hostile.
        if (mons_is_shedu(mon) && shedu_pair_alive(mon))
            player_angers_monster(get_shedu_pair(mon));

        return true;
    }

    return false;
}

monster* create_monster(mgen_data mg, bool fail_msg)
{
    const monster_type montype = mons_class_is_zombified(mg.cls) ? mg.base_type
                                                                 : mg.cls;

    monster *summd = 0;

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
            monster dummy;
            const monster_type resistless_mon = MONS_HUMAN;
            // If the type isn't known yet assume no resists or anything.
            dummy.type         = _is_random_monster(mg.cls) ? resistless_mon
                                                            : mg.cls;
            dummy.base_monster = mg.base_type;
            dummy.god          = mg.god;
            dummy.behaviour    = mg.behaviour;

            // Monsters that have resistance info in the ghost
            // structure cannot be handled as dummies, so treat them
            // as a known no-resist monster. mons_avoids_cloud() will
            // crash for dummy monsters which should have a
            // ghost_demon setup.
            if (mons_is_ghost_demon(dummy.type))
                dummy.type = resistless_mon;

            int tries = 0;
            while (tries++ < 50
                   && (!in_bounds(mg.pos)
                       || mons_avoids_cloud(&dummy, env.cgrid(mg.pos), true)))
            {
                mg.pos = find_newmons_square(montype, mg.pos);
            }
            if (!in_bounds(mg.pos))
                return 0;

            const int cloud_num = env.cgrid(mg.pos);
            // Don't place friendly god gift in a damaging cloud created by
            // you if that would anger the god.
            if (mons_avoids_cloud(&dummy, cloud_num, true)
                && mg.behaviour == BEH_FRIENDLY
                && god_hates_attacking_friend(you.religion, &dummy)
                && YOU_KILL(env.cloud[cloud_num].killer))
            {
                return 0;
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

    if (!summd && fail_msg && you.see_cell(mg.pos))
        mpr("You see a puff of smoke.");

    return summd;
}

bool find_habitable_spot_near(const coord_def& where, monster_type mon_type,
                              int radius, bool allow_centre, coord_def& empty)
{
    // XXX: A lot of hacks that could be avoided by passing the
    //      monster generation data through.

    int good_count = 0;

    for (radius_iterator ri(where, radius, C_ROUND, !allow_centre);
         ri; ++ri)
    {
        bool success = false;

        if (actor_at(*ri))
            continue;

        if (!cell_see_cell(where, *ri, LOS_NO_TRANS))
            continue;

        success = monster_habitable_grid(mon_type, grd(*ri));

        if (success && one_chance_in(++good_count))
            empty = *ri;
    }

    return (good_count > 0);
}

static void _get_vault_mon_list(vector<mons_spec> &list);

monster_type summon_any_demon(monster_type dct)
{
    if (player_in_branch(BRANCH_PANDEMONIUM) && !one_chance_in(40))
    {
        monster_type typ = MONS_0;
        int count = 0;
        vector<mons_spec> list;
        _get_vault_mon_list(list);
        const bool major = !list.empty();
        const int max = major ? list.size() : PAN_MONS_ALLOC;
        for (int i = 0; i < max; i++)
        {
            const monster_type cur = major ? list[i].monbase
                                           : env.mons_alloc[i];
            if (invalid_monster_type(cur))
                continue;
            const monsterentry *mentry = get_monster_data(cur);
            if (dct == RANDOM_DEMON && mons_class_holiness(cur) != MH_DEMONIC
                || dct == RANDOM_DEMON_LESSER && mentry->basechar != '5'
                || dct == RANDOM_DEMON_COMMON
                   && mentry->basechar != '4'
                   && mentry->basechar != '3'
                || dct == RANDOM_DEMON_GREATER
                   && mentry->basechar != '2'
                   && mentry->basechar != '1')
            {
                continue;
            }
            const int weight = major ? list[i].genweight : 1;
            count += weight;
            if (x_chance_in_y(weight, count))
                typ = cur;
        }
        if (count)
            return typ;
    }

    if (dct == RANDOM_DEMON)
        dct = static_cast<monster_type>(RANDOM_DEMON_LESSER + random2(3));

    switch (dct)
    {
    case RANDOM_DEMON_LESSER:
        // tier 5
        return random_choose_weighted(
            1, MONS_CRIMSON_IMP,
            1, MONS_QUASIT,
            1, MONS_WHITE_IMP,
            1, MONS_LEMURE,
            1, MONS_UFETUBUS,
            1, MONS_IRON_IMP,
            1, MONS_SHADOW_IMP,
            0);

    case RANDOM_DEMON_COMMON:
        if (x_chance_in_y(6, 10))
        {
            // tier 4
            return random_choose_weighted(
                1, MONS_BLUE_DEVIL,
                1, MONS_IRON_DEVIL,
                1, MONS_ORANGE_DEMON,
                1, MONS_RED_DEVIL,
                1, MONS_ROTTING_DEVIL,
                1, MONS_SIXFIRHY,
                1, MONS_HELLWING,
                0);
        }
        else
        {
            // tier 3
            return random_choose_weighted(
                1, MONS_SUN_DEMON,
                1, MONS_SOUL_EATER,
                1, MONS_ICE_DEVIL,
                1, MONS_SMOKE_DEMON,
                1, MONS_NEQOXEC,
                1, MONS_YNOXINUL,
                1, MONS_CHAOS_SPAWN,
                0);
        }

    case RANDOM_DEMON_GREATER:
        if (x_chance_in_y(6, 10))
        {
            // tier 2
            return random_choose_weighted(
                1, MONS_GREEN_DEATH,
                1, MONS_BLIZZARD_DEMON,
                1, MONS_BALRUG,
                1, MONS_CACODEMON,
                1, MONS_HELL_BEAST,
                1, MONS_HELLION,
                1, MONS_REAPER,
                1, MONS_LOROCYPROCA,
                1, MONS_TORMENTOR,
                1, MONS_SHADOW_DEMON,
                0);
        }
        else
        {
            // tier 1
            return random_choose_weighted(
                1, MONS_BRIMSTONE_FIEND,
                1, MONS_ICE_FIEND,
                1, MONS_SHADOW_FIEND,
                1, MONS_HELL_SENTINEL,
                1, MONS_EXECUTIONER,
                0);
        }

    default:
        return dct;
    }
}

monster_type summon_any_dragon(dragon_class_type dct)
{
    monster_type mon = MONS_PROGRAM_BUG;

    switch (dct)
    {
    case DRAGON_LIZARD:
        mon = random_choose_weighted(
            5, MONS_SWAMP_DRAKE,
            5, MONS_KOMODO_DRAGON,
            5, MONS_WIND_DRAKE,
            6, MONS_FIRE_DRAKE,
            6, MONS_DEATH_DRAKE,
            3, MONS_DRAGON,
            0);
        break;

    case DRAGON_DRACONIAN:
        mon = random_draconian_monster_species();
        break;

    case DRAGON_DRAGON:
        mon = random_choose_weighted(
            1, MONS_MOTTLED_DRAGON,
            1, MONS_LINDWURM,
            1, MONS_STORM_DRAGON,
            1, MONS_STEAM_DRAGON,
            1, MONS_DRAGON,
            1, MONS_ICE_DRAGON,
            1, MONS_SWAMP_DRAGON,
            1, MONS_SHADOW_DRAGON,
            0);
        break;

    default:
        break;
    }

    return mon;
}

/////////////////////////////////////////////////////////////////////////////
//
// Random monsters for portal vaults.
//
/////////////////////////////////////////////////////////////////////////////

void set_vault_mon_list(const vector<mons_spec> &list)
{
    CrawlHashTable &props = env.properties;

    props.erase(VAULT_MON_TYPES_KEY);
    props.erase(VAULT_MON_BASES_KEY);
    props.erase(VAULT_MON_WEIGHTS_KEY);
    props.erase(VAULT_MON_BANDS_KEY);

    unsigned int size = list.size();
    if (size == 0)
    {
        setup_vault_mon_list();
        return;
    }

    props[VAULT_MON_TYPES_KEY].new_vector(SV_INT).resize(size);
    props[VAULT_MON_BASES_KEY].new_vector(SV_INT).resize(size);
    props[VAULT_MON_WEIGHTS_KEY].new_vector(SV_INT).resize(size);
    props[VAULT_MON_BANDS_KEY].new_vector(SV_BOOL).resize(size);

    CrawlVector &type_vec   = props[VAULT_MON_TYPES_KEY].get_vector();
    CrawlVector &base_vec   = props[VAULT_MON_BASES_KEY].get_vector();
    CrawlVector &weight_vec = props[VAULT_MON_WEIGHTS_KEY].get_vector();
    CrawlVector &band_vec   = props[VAULT_MON_BANDS_KEY].get_vector();

    for (unsigned int i = 0; i < size; i++)
    {
        const mons_spec &spec = list[i];

        if (spec.place.is_valid())
        {
            ASSERT(branch_has_monsters(spec.place.branch));
            type_vec[i] = -1;
            base_vec[i] = spec.place.packed_place();
        }
        else
        {
            ASSERT(!_is_random_monster(spec.type)
                   && !_is_random_monster(spec.monbase));
            type_vec[i] = spec.type;
            base_vec[i] = spec.monbase;
            band_vec[i] = spec.band;
        }
        weight_vec[i] = spec.genweight;
    }

    setup_vault_mon_list();
}

static void _get_vault_mon_list(vector<mons_spec> &list)
{
    list.clear();

    CrawlHashTable &props = env.properties;

    if (!props.exists(VAULT_MON_TYPES_KEY))
        return;

    ASSERT(props.exists(VAULT_MON_BASES_KEY));
    ASSERT(props.exists(VAULT_MON_WEIGHTS_KEY));
    ASSERT(props.exists(VAULT_MON_BANDS_KEY));

    CrawlVector &type_vec   = props[VAULT_MON_TYPES_KEY].get_vector();
    CrawlVector &base_vec   = props[VAULT_MON_BASES_KEY].get_vector();
    CrawlVector &weight_vec = props[VAULT_MON_WEIGHTS_KEY].get_vector();
    CrawlVector &band_vec   = props[VAULT_MON_BANDS_KEY].get_vector();

    ASSERT(type_vec.size() == base_vec.size());
    ASSERT(type_vec.size() == weight_vec.size());
    ASSERT(type_vec.size() == band_vec.size());

    unsigned int size = type_vec.size();
    for (unsigned int i = 0; i < size; i++)
    {
        monster_type type = static_cast<monster_type>(static_cast<int>(type_vec[i]));
        monster_type base = static_cast<monster_type>(static_cast<int>(base_vec[i]));

        mons_spec spec;

#if TAG_MAJOR_VERSION == 34
        if ((int)type == -1)
            type = MONS_NO_MONSTER;
#endif
        if (type == MONS_NO_MONSTER && base != MONS_NO_MONSTER)
        {
            spec.place = level_id::from_packed_place(base);
            ASSERT(spec.place.is_valid());
            ASSERT(branch_has_monsters(spec.place.branch));
        }
        else
        {
            spec.type    = type;
            spec.monbase = base;
            ASSERT(!_is_random_monster(spec.type)
                   && !_is_random_monster(spec.monbase));
        }
        spec.genweight = weight_vec[i];
        spec.band = band_vec[i];

        list.push_back(spec);
    }
}

void setup_vault_mon_list()
{
    vault_mon_types.clear();
    vault_mon_bases.clear();
    vault_mon_weights.clear();
    vault_mon_bands.clear();

    vector<mons_spec> list;
    _get_vault_mon_list(list);

    unsigned int size = list.size();

    vault_mon_types.resize(size);
    vault_mon_bases.resize(size);
    vault_mon_weights.resize(size);
    vault_mon_bands.resize(size);

    for (unsigned int i = 0; i < size; i++)
    {
        if (list[i].place.is_valid())
        {
            vault_mon_types[i] = -1;
            vault_mon_bases[i] = list[i].place.packed_place();
        }
        else
        {
            vault_mon_types[i] = list[i].type;
            vault_mon_bases[i] = list[i].monbase;
        }
        vault_mon_bands[i] = list[i].band;
        vault_mon_weights[i] = list[i].genweight;
    }
    if (size)
        dprf("Level has a custom monster set.");
}
