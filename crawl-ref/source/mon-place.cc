/**
 * @file
 * @brief Functions used when placing monsters in the dungeon.
**/

#include "AppHdr.h"

#include "mon-place.h"
#include "mgen_data.h"

#include <algorithm>

#include "abyss.h"
#include "areas.h"
#include "arena.h"
#include "attitude-change.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "errors.h"
#include "fprop.h"
#include "ghost.h"
#include "godabil.h"
#include "godpassive.h" // passive_t::slow_abyss, slow_orb_run
#include "lev-pand.h"
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-pick.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tiledef-player.h"
#endif
#include "tilepick.h"
#include "traps.h"
#include "travel.h"
#include "unwind.h"
#include "viewchar.h"
#include "view.h"

band_type active_monster_band = BAND_NO_BAND;

static vector<int> vault_mon_types;
static vector<int> vault_mon_bases;
static vector<level_id> vault_mon_places;
static vector<int> vault_mon_weights;
static vector<bool> vault_mon_bands;

#if TAG_MAJOR_VERSION > 34
#define VAULT_MON_TYPES_KEY   "vault_mon_types"
#define VAULT_MON_BASES_KEY   "vault_mon_bases"
#define VAULT_MON_PLACES_KEY  "vault_mon_places"
#define VAULT_MON_WEIGHTS_KEY "vault_mon_weights"
#define VAULT_MON_BANDS_KEY   "vault_mon_bands"
#endif

// proximity is the same as for mons_place:
// 0 is no restrictions
// 1 attempts to place near player
// 2 attempts to avoid player LOS

#define BIG_BAND        20

static monster_type _band_member(band_type band, int which,
                                 level_id parent_place, bool allow_ood);
static band_type _choose_band(monster_type mon_type, int *band_size_p = nullptr,
                              bool *natural_leader_p = nullptr);

static monster* _place_monster_aux(const mgen_data &mg, const monster *leader,
                                   level_id place,
                                   bool force_pos = false,
                                   bool dont_place = false);

/**
 * Is this feature "close enough" to the one we want for monster generation?
 *
 * @param wanted_feat the preferred feature
 * @param actual_feat the feature to be compared to it
 * @returns Whether wanted_feat is considered to be similar enough to
 *          actual_feat that being able to survive in the former means you can
 *          survive in the latter.
 */
static bool _feat_compatible(dungeon_feature_type wanted_feat,
                             dungeon_feature_type actual_feat)
{
    return wanted_feat == actual_feat
           || wanted_feat == DNGN_DEEP_WATER && feat_is_watery(actual_feat)
           || wanted_feat == DNGN_FLOOR && feat_has_solid_floor(actual_feat);
}

/**
 * Can this monster survive on actual_grid?
 *
 * @param mon         the monster to be checked.
 * @param actual_grid the feature type that mon might not be able to survive.
 * @returns whether the monster can survive being in/over the feature,
 *          regardless of whether it may be dangerous or harmful.
 */
bool monster_habitable_grid(const monster* mon,
                            dungeon_feature_type actual_grid)
{
    // Zombified monsters enjoy the same habitat as their original,
    // except lava-based monsters.
    const monster_type mt = fixup_zombie_type(mon->type,
                                              mons_base_type(*mon));

    return monster_habitable_grid(mt, actual_grid, DNGN_UNSEEN, mon->airborne());
}

/**
 * Can monsters of this class survive on actual_grid?
 *
 * @param mt the monster class to check against
 * @param actual_grid the terrain feature being checked
 * @param wanted_grid if == DNGN_UNSEEN, or if the monster can't survive on it,
 *                    ignored. Otherwise, return false even if actual_grid is
 *                    survivable, if actual_grid isn't similar to wanted_grid.
 * @param flies if true, treat the monster as flying even if the monster class
 *              can't usually fly.
 */
bool monster_habitable_grid(monster_type mt,
                            dungeon_feature_type actual_grid,
                            dungeon_feature_type wanted_grid,
                            bool flies)
{
    // No monster may be placed in walls etc.
    if (!mons_class_can_pass(mt, actual_grid))
        return false;

    // Monsters can't use teleporters, and standing there would look just wrong.
    if (actual_grid == DNGN_TELEPORTER)
        return false;

    // The kraken is so large it cannot enter shallow water.
    // Its tentacles can, and will, though.
    if (actual_grid == DNGN_SHALLOW_WATER && mt == MONS_KRAKEN)
        return false;

    const dungeon_feature_type feat_preferred =
        habitat2grid(mons_class_primary_habitat(mt));
    const dungeon_feature_type feat_nonpreferred =
        habitat2grid(mons_class_secondary_habitat(mt));

    const bool monster_is_airborne = mons_class_flag(mt, M_FLIES) || flies;

    // If the caller insists on a specific feature type, try to honour
    // the request. This allows the builder to place amphibious
    // creatures only on land, or flying creatures only on lava, etc.
    if (wanted_grid != DNGN_UNSEEN
        && monster_habitable_grid(mt, wanted_grid, DNGN_UNSEEN, flies))
    {
        return _feat_compatible(wanted_grid, actual_grid);
    }

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

    if (_feat_compatible(feat_preferred, actual_grid)
        || _feat_compatible(feat_nonpreferred, actual_grid))
    {
        return true;
    }

    // [dshaligram] Flying creatures are all HT_LAND, so we
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
    {
        switch (mons_habitat(*mon))
        {
        case HT_WATER:
        case HT_AMPHIBIOUS:
            return feat_is_watery(feat);
        case HT_LAVA:
        case HT_AMPHIBIOUS_LAVA:
            return feat == DNGN_LAVA;
        case HT_LAND:
            return feat == DNGN_FLOOR;
        default:
            return false;
        }
    }
    else
        return false;
}

static bool _is_spawn_scaled_area(const level_id &here)
{
    return is_connected_branch(here.branch)
           && !is_hell_subbranch(here.branch)
           && here.branch != BRANCH_VESTIBULE
           && here.branch != BRANCH_ZOT;
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
    return turns_on_level <= dropoff_start_turns ? base_value :
           turns_on_level > dropoff_start_turns + dropoff_ramp_turns ?
           final_value :

           // Actual scaling, strictly linear at the moment:
           (base_value +
            (target_value - base_value)
            * (turns_on_level - dropoff_start_turns)
            / dropoff_ramp_turns);
}

static void _apply_ood(level_id &place)
{
    // OODs do not apply to any portal vaults, any 1-level branches, Zot and
    // hells. What with newnewabyss?
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
            dprf(DIAG_MONPLACE, "Monster level fuzz: %d (old: %s, new: %s)",
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
        dprf(DIAG_MONPLACE, "Super OOD roll: Old: %s, New: %s",
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

/**
 * Spawn random monsters.

 * The spawn rate defaults to the current env.spawn_random_rate for the branch,
 * but is modified by whether the player is in the abyss and on what level, as
 * well as whether the player has the orb.
 */
void spawn_random_monsters()
{
    if (crawl_state.disables[DIS_SPAWNS])
        return;

    if (crawl_state.game_is_arena()
        || (crawl_state.game_is_sprint()
            && player_in_connected_branch()
            && you.chapter == CHAPTER_ORB_HUNTING))
    {
        return;
    }

#ifdef DEBUG_MON_CREATION
    mprf(MSGCH_DIAGNOSTICS, "in spawn_random_monsters()");
#endif
    int rate = env.spawn_random_rate;
    if (!rate)
    {
#ifdef DEBUG_MON_CREATION
        mprf(MSGCH_DIAGNOSTICS, "random monster gen turned off");
#endif
        return;
    }

    if (player_in_branch(BRANCH_VESTIBULE))
        rate = _vestibule_spawn_rate();

    if (player_on_orb_run())
        rate = have_passive(passive_t::slow_orb_run) ? 16 : 8;
    else if (!player_in_starting_abyss())
        rate = _scale_spawn_parameter(rate, 6 * rate, 0);

    if (rate == 0)
    {
        dprf(DIAG_MONPLACE, "random monster gen scaled off, %d turns on level",
             env.turns_on_level);
        return;
    }

    if (player_in_branch(BRANCH_ABYSS))
    {
        if (!player_in_starting_abyss())
            rate = 5;
        if (have_passive(passive_t::slow_abyss))
            rate *= 2;
    }

    if (!x_chance_in_y(5, rate))
        return;

    // Place normal dungeon monsters, but not in player LOS. Don't generate orb
    // spawns in Abyss to show some mercy to players that get banished there on
    // the orb run.
    if (player_in_connected_branch()
        || (player_on_orb_run() && !player_in_branch(BRANCH_ABYSS)))
    {
        dprf(DIAG_MONPLACE, "Placing monster, rate: %d, turns here: %d",
             rate, env.turns_on_level);
        proximity_type prox = (one_chance_in(10) ? PROX_NEAR_STAIRS
                                                 : PROX_AWAY_FROM_PLAYER);

        // The rules change once the player has picked up the Orb...
        if (player_on_orb_run())
            prox = (one_chance_in(3) ? PROX_CLOSE_TO_PLAYER : PROX_ANYWHERE);

        mgen_data mg(WANDERING_MONSTER);
        mg.proximity = prox;
        mg.foe = (player_on_orb_run()) ? MHITYOU : MHITNOT;
        mons_place(mg);
        viewwindow();
        return;
    }

    mgen_data mg(WANDERING_MONSTER);
    if (player_in_branch(BRANCH_PANDEMONIUM)
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
    return mt == RANDOM_MONSTER || mt == RANDOM_MOBILE_MONSTER
           || mt == RANDOM_COMPATIBLE_MONSTER
           || mt == RANDOM_BANDLESS_MONSTER
           || mt == WANDERING_MONSTER;
}

static bool _has_big_aura(monster_type mt)
{
    return mt == MONS_SILENT_SPECTRE;
}

static bool _is_incompatible_monster(monster_type mt)
{
    return mons_class_is_stationary(mt)
        || player_will_anger_monster(mt);
}

static bool _is_banded_monster(monster_type mt)
{
    return _choose_band(mt) != BAND_NO_BAND;
}

// Caller must use !invalid_monster_type to check if the return value
// is a real monster.
monster_type pick_random_monster(level_id place,
                                 monster_type kind,
                                 level_id *final_place,
                                 bool allow_ood)
{
    if (crawl_state.game_is_arena())
    {
        monster_type type = arena_pick_random_monster(place);
        if (!_is_random_monster(type))
            return type;
    }

    if (allow_ood)
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
    else if (kind == RANDOM_BANDLESS_MONSTER)
        return pick_monster(place, _is_banded_monster);
    else if (mons_class_is_zombified(kind))
        return pick_local_zombifiable_monster(place, kind, coord_def());
    else if (crawl_state.game_is_sprint())
        return pick_monster(place, _has_big_aura);
    else
        return pick_monster(place);
}

bool can_place_on_trap(monster_type mon_type, trap_type trap)
{
    if (mons_is_tentacle_segment(mon_type))
        return true;

    if (trap == TRAP_TELEPORT || trap == TRAP_TELEPORT_PERMANENT
        || trap == TRAP_SHAFT)
    {
        return false;
    }

    return true;
}

bool drac_colour_incompatible(int drac, int colour)
{
    return drac == MONS_DRACONIAN_SCORCHER && colour == MONS_WHITE_DRACONIAN;
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
    for (branch_iterator it; it; ++it)
    {
        if (it->entry_stairs == feat)
        {
            place = it->id;
            break;
        }
        else if (it->exit_stairs == feat)
        {
            place = brentry[it->id];
            // This can happen on D:1 and in wizmode with random spawns on the
            // first floor of a branch that didn't generate naturally.
            if (!place.is_valid())
                return false;
            break;
        }
    }
    const monster_type habitat_target = MONS_BAT;
    int distance = 3;
    pos = find_newmons_square_contiguous(habitat_target, pos, distance);
    return in_bounds(pos);
}

bool needs_resolution(monster_type mon_type)
{
    return mon_type == RANDOM_DRACONIAN || mon_type == RANDOM_BASE_DRACONIAN
           || mon_type == RANDOM_NONBASE_DRACONIAN
           || mon_type >= RANDOM_DEMON_LESSER && mon_type <= RANDOM_DEMON
           || mon_type == RANDOM_DEMONSPAWN
           || mon_type == RANDOM_BASE_DEMONSPAWN
           || mon_type == RANDOM_NONBASE_DEMONSPAWN
           || _is_random_monster(mon_type);
}

monster_type resolve_monster_type(monster_type mon_type,
                                  monster_type &base_type,
                                  proximity_type proximity,
                                  coord_def *pos,
                                  unsigned mmask,
                                  dungeon_char_type *stair_type,
                                  level_id *place,
                                  bool *want_band,
                                  bool allow_ood)
{
    if (want_band)
        *want_band = false;

    if (mon_type == RANDOM_DRACONIAN)
    {
        // Pick any random drac, constrained by colour if requested.
        do
        {
            if (coinflip())
                mon_type = random_draconian_monster_species();
            else
                mon_type = random_draconian_job();
        }
        while (base_type != MONS_PROGRAM_BUG
               && mon_type != base_type
               && (mons_species(mon_type) == mon_type
                   || drac_colour_incompatible(mon_type, base_type)));
    }
    else if (mon_type == RANDOM_BASE_DRACONIAN)
        mon_type = random_draconian_monster_species();
    else if (mon_type == RANDOM_NONBASE_DRACONIAN)
        mon_type = random_draconian_job();
    else if (mon_type >= RANDOM_DEMON_LESSER && mon_type <= RANDOM_DEMON)
        mon_type = summon_any_demon(mon_type, true);
    else if (mon_type == RANDOM_DEMONSPAWN)
    {
        do
        {
            mon_type =
                static_cast<monster_type>(
                    random_range(MONS_FIRST_DEMONSPAWN,
                                 MONS_LAST_DEMONSPAWN));
        }
        while (base_type != MONS_PROGRAM_BUG
               && mon_type != base_type
               && mons_species(mon_type) == mon_type);
    }
    else if (mon_type == RANDOM_BASE_DEMONSPAWN)
        mon_type = random_demonspawn_monster_species();
    else if (mon_type == RANDOM_NONBASE_DEMONSPAWN)
        mon_type = random_demonspawn_job();

    // (2) Take care of non-draconian random monsters.
    else if (_is_random_monster(mon_type))
    {
        // Respect destination level for staircases.
        if (proximity == PROX_NEAR_STAIRS)
        {
            const level_id orig_place = *place;

            if (_find_mon_place_near_stairs(*pos, stair_type, *place))
            {
                // No monsters spawned in the Temple.
                if (branches[place->branch].id == BRANCH_TEMPLE)
                    proximity = PROX_AWAY_FROM_PLAYER;
            }
            else
                proximity = PROX_AWAY_FROM_PLAYER;
            if (proximity == PROX_NEAR_STAIRS)
            {
                dprf(DIAG_MONPLACE, "foreign monster from %s",
                     place->describe().c_str());
            }
            else // we dunt cotton to no ferrniers in these here parts
                *place = orig_place;

        } // end proximity check

        // Only use the vault list if the monster comes from this level.
        if (!vault_mon_types.empty() && *place == level_id::current())
        {
            int i = 0;
            int tries = 0;
            int type;
            do
            {
                i = choose_random_weighted(vault_mon_weights.begin(),
                                           vault_mon_weights.end());
                type = vault_mon_types[i];

                // Give up after enough attempts: for example, a Yred
                // worshipper casting Shadow Creatures in holy Pan.
                if (tries++ >= 300)
                    type = MONS_NO_MONSTER;
                // If the monster list says not to place, or to place
                // by level, or to place a random monster, accept that.
                // If it's random, we'll be recursively calling ourselves
                // later on for the new monster type.
                if (type == MONS_NO_MONSTER || type == -1
                    || needs_resolution((monster_type)type))
                {
                    break;
                }
            }
            while (mon_type == RANDOM_MOBILE_MONSTER
                      && mons_class_is_stationary((monster_type)type)
                   || mon_type == RANDOM_COMPATIBLE_MONSTER
                      && _is_incompatible_monster((monster_type)type)
                   || mon_type == RANDOM_BANDLESS_MONSTER
                      && _is_banded_monster((monster_type)type));

            int base = vault_mon_bases[i];
            bool banded = vault_mon_bands[i];

            if (type == -1)
                *place = vault_mon_places[i];
            else
            {
                base_type = (monster_type) base;
                mon_type  = (monster_type) type;
                if (want_band)
                    *want_band = banded;
                if (needs_resolution(mon_type))
                {
                    mon_type =
                        resolve_monster_type(mon_type, base_type,
                                             proximity, pos, mmask,
                                             stair_type, place,
                                             want_band, allow_ood);
                }
                return mon_type;
            }
        }

        int tries = 0;
        while (tries++ < 300)
        {
            level_id orig_place = *place;

            // Now pick a monster of the given branch and level.
            mon_type = pick_random_monster(*place, mon_type, place, allow_ood);

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
            mon_type = pick_random_monster(*place, mon_type, place, allow_ood);
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

// For generation purposes, don't treat simulacra of lava enemies as
// being able to place on lava.
const monster_type fixup_zombie_type(const monster_type cls,
                                         const monster_type base_type)
{
    return (mons_class_is_zombified(cls)
            && mons_class_secondary_habitat(base_type) != HT_LAVA)
            ? base_type
            : cls;
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

    const monster_type montype = fixup_zombie_type(mg.cls, mg.base_type);
    if (!monster_habitable_grid(montype, grd(mg_pos), mg.preferred_grid_feature)
        || (mg.behaviour != BEH_FRIENDLY
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
        && grid_distance(you.pos(), mg_pos) <= LOS_RADIUS)
    {
        return false;
    }

    // Don't generate monsters on top of teleport traps.
    // (How did they get there?)
    const trap_def* ptrap = trap_at(mg_pos);
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
    return env.turns_on_level < 1400 - env.absdepth0 * 117;
}

monster* place_monster(mgen_data mg, bool force_pos, bool dont_place)
{
#ifdef DEBUG_MON_CREATION
    mprf(MSGCH_DIAGNOSTICS, "in place_monster()");
#endif

    int tries = 0;
    dungeon_char_type stair_type = NUM_DCHAR_TYPES;

    // (1) Early out (summoned to occupied grid).
    if (mg.use_position() && monster_at(mg.pos))
        return 0;

    if (!mg.place.is_valid())
        mg.place = level_id::current();

    const bool allow_ood = !(mg.flags & MG_NO_OOD);
    bool want_band = false;
    level_id place = mg.place;
    mg.cls = resolve_monster_type(mg.cls, mg.base_type, mg.proximity,
                                  &mg.pos, mg.map_mask,
                                  &stair_type,
                                  &place,
                                  &want_band,
                                  allow_ood);
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
        dprf(DIAG_MONPLACE, "Chose monster with OOD roll: %s,"
                            " disabling band generation",
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

    if (create_band)
    {
#ifdef DEBUG_MON_CREATION
        mprf(MSGCH_DIAGNOSTICS, "Choose band members...");
#endif
        band = _choose_band(mg.cls, &band_size, &leader);
        band_size++;
        for (int i = 1; i < band_size; ++i)
        {
            band_monsters[i] = _band_member(band, i, place, allow_ood);
            if (band_monsters[i] == NUM_MONSTERS)
                die("Unhandled band type %d", band);
        }

        // Set the (very) ugly thing band colour.
        ugly_thing_apply_uniform_band_colour(mg, band_monsters, band_size);
    }

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

    // (4) For first monster, choose location. This is pretty intensive.
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
                if (grid_distance(you.pos(), mg.pos) < 2 + random2(3))
                    proxOK = false;
                break;

            case PROX_CLOSE_TO_PLAYER:
            case PROX_AWAY_FROM_PLAYER:
                // If this is supposed to measure los vs not los,
                // then see_cell(mg.pos) should be used instead. (jpeg)
                close_to_player = (grid_distance(you.pos(), mg.pos) <=
                                   LOS_RADIUS);

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
    else if (mg.proximity == PROX_NEAR_STAIRS && you.can_see(*mon))
    {
        switch (stair_type)
        {
        case DCHAR_STAIRS_DOWN: mon->seen_context = SC_UPSTAIRS; break;
        case DCHAR_STAIRS_UP:   mon->seen_context = SC_DOWNSTAIRS; break;
        case DCHAR_ARCH:        mon->seen_context = SC_ARCH; break;
        default: ;
        }
    }
    else if (player_in_branch(BRANCH_ABYSS) && you.can_see(*mon)
             && !crawl_state.generating_level
             && !mg.summoner
             && !crawl_state.is_god_acting()
             && !(mon->flags & MF_WAS_IN_VIEW)) // is this possible?
    {
        mon->seen_context = SC_ABYSS;
    }

    // Now, forget about banding if the first placement failed, or there are
    // too many monsters already, or we successfully placed by stairs.
    if (mon->mindex() >= MAX_MONSTERS - 30
        || (mg.proximity == PROX_NEAR_STAIRS))
    {
        return mon;
    }

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
                // Don't give XP for the slaves to discourage hunting. Pikel
                // has an artificially large XP modifier to compensate for
                // this.
                member->flags |= MF_NO_REWARD;
                member->props["pikel_band"] = true;
            }
            else if (mon->type == MONS_KIRKE)
                member->props["kirke_band"] = true;
        }
    }

    // Placement of first monster, at least, was a success.
    return mon;
}

monster* get_free_monster()
{
    for (auto &mons : menv_real)
        if (mons.type == MONS_NO_MONSTER)
        {
            mons.reset();
            return &mons;
        }

    return nullptr;
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

    const monster_type montype = fixup_zombie_type(mg.cls, mg.base_type);

    // Setup habitat and placement.
    // If the space is occupied, try some neighbouring square instead.
    if (dont_place)
        fpos.reset();
    else if (leader == 0 && in_bounds(mg.pos)
        && (mg.behaviour == BEH_FRIENDLY ||
            (!is_sanctuary(mg.pos) || mons_is_tentacle_segment(montype)))
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

    // Set pos and link monster into monster grid.
    if (!dont_place && !mon->move_to_pos(fpos))
    {
        env.mid_cache.erase(mon->mid);
        mon->reset();
        return 0;
    }

    // Pick the correct Serpent of Hell.
    if (mon->type == MONS_SERPENT_OF_HELL)
    {
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
        define_monster(*mon);

    if (mon->type == MONS_MUTANT_BEAST)
    {
        vector<int> gen_facets;
        if (mg.props.exists(MUTANT_BEAST_FACETS))
            for (auto facet : mg.props[MUTANT_BEAST_FACETS].get_vector())
                gen_facets.push_back(facet.get_int());

        set<int> avoid_facets;
        if (mg.props.exists(MUTANT_BEAST_AVOID_FACETS))
            for (auto facet : mg.props[MUTANT_BEAST_AVOID_FACETS].get_vector())
                avoid_facets.insert(facet.get_int());

        init_mutant_beast(*mon, mg.hd, gen_facets, avoid_facets);
    }

    // Is it a god gift?
    if (mg.god != GOD_NO_GOD)
        mons_make_god_gift(*mon, mg.god);
    // Not a god gift, give priestly monsters a god.
    else if (mon->is_priest())
    {
        // Berserkers belong to Trog.
        if (mg.cls == MONS_SPRIGGAN_BERSERKER)
            mon->god = GOD_TROG;
        // Death knights belong to Yredelemnul.
        else if (mg.cls == MONS_DEATH_KNIGHT)
            mon->god = GOD_YREDELEMNUL;
        // Asterion belongs to Mahkleb.
        else if (mg.cls == MONS_ASTERION)
            mon->god = GOD_MAKHLEB;
        // Seraphim follow the Shining One.
        else if (mg.cls == MONS_SERAPH)
            mon->god = GOD_SHINING_ONE;
        // Draconian stormcallers worship Qazlal.
        else if (mg.cls == MONS_DRACONIAN_STORMCALLER)
            mon->god = GOD_QAZLAL;
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
    // The Royal Jelly belongs to Jiyva.
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
    else if (mg.cls == MONS_PROFANE_SERVITOR)
        mon->god = GOD_YREDELEMNUL;
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
    if (mon->holiness() & MH_HOLY)
        invalidate_agrid(true);
    if (mg.cls == MONS_SILENT_SPECTRE || mg.cls == MONS_PROFANE_SERVITOR)
        invalidate_agrid(true);

    // If the caller requested a specific colour for this monster, apply
    // it now.
    if ((mg.colour == COLOUR_INHERIT
         && mons_class_colour(mon->type) != COLOUR_UNDEF)
        || mg.colour > COLOUR_UNDEF)
    {
        mon->colour = mg.colour;
    }

    if (mg.mname != "")
        mon->mname = mg.mname;

    if (mg.props.exists(MGEN_NUM_HEADS))
        mon->num_heads = mg.props[MGEN_NUM_HEADS];
    if (mg.props.exists(MGEN_BLOB_SIZE))
        mon->blob_size = mg.props[MGEN_BLOB_SIZE];
    if (mg.props.exists(MGEN_TENTACLE_CONNECT))
        mon->tentacle_connect = mg.props[MGEN_TENTACLE_CONNECT].get_int();

    if (mg.hd != 0)
    {
        int bonus_hp = 0;
        if (mons_is_demonspawn(mg.cls)
            && mg.cls != MONS_DEMONSPAWN
            && mons_species(mg.cls) == MONS_DEMONSPAWN)
        {
            // Nonbase demonspawn get bonuses from their base type.
            const monsterentry *mbase =
                get_monster_data(draco_or_demonspawn_subspecies(*mon));
            bonus_hp = mbase->avg_hp_10x;
        }
        mon->set_hit_dice(mg.hd);
        mon->props[VAULT_HD_KEY] = mg.hd;
        // Re-roll HP.
        const int base_avg_hp = m_ent->avg_hp_10x + bonus_hp;
        const int new_avg_hp = div_rand_round(base_avg_hp * mg.hd, m_ent->HD);
        const int hp = hit_points(new_avg_hp);
        // But only for monsters with random HP. (XXX: should be everything?)
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
        mon->props[KNOWN_MAX_HP_KEY] = mg.hp;
    }

    if (!crawl_state.game_is_arena())
    {
        mon->max_hit_points = min(mon->max_hit_points, MAX_MONSTER_HP);
        mon->hit_points = min(mon->hit_points, MAX_MONSTER_HP);
    }

    // Store the extra flags here.
    mon->flags       |= mg.extra_flags;

    // "Prince Ribbit returns to his original shape as he dies."
    if (mg.cls == MONS_PRINCE_RIBBIT)
        mon->props[ORIGINAL_TYPE_KEY].get_int() = MONS_PRINCE_RIBBIT;

    // The return of Boris is now handled in monster_die(). Not setting
    // this for Boris here allows for multiple Borises in the dungeon at
    // the same time. - bwr
    if (mons_is_unique(mg.cls))
        you.unique_creatures.set(mg.cls);

    if (mons_class_flag(mg.cls, M_INVIS))
        mon->add_ench(ENCH_INVIS);

    if (mons_class_flag(mg.cls, M_CONFUSED))
        mon->add_ench(ENCH_CONFUSION);

    if (mg.cls == MONS_SHAPESHIFTER)
        mon->add_ench(ENCH_SHAPESHIFTER);

    if (mg.cls == MONS_GLOWING_SHAPESHIFTER)
        mon->add_ench(ENCH_GLOWING_SHAPESHIFTER);

    if ((mg.cls == MONS_TOADSTOOL
         || mg.cls == MONS_PILLAR_OF_SALT
         || mg.cls == MONS_BLOCK_OF_ICE)
        && !mg.props.exists(MGEN_NO_AUTO_CRUMBLE))
    {
        // This enchantment is a timer that counts down until death.
        // It should last longer than the lifespan of a corpse, to avoid
        // spawning mushrooms in the same place over and over. Aside
        // from that, the value is slightly randomised to avoid
        // simultaneous die-offs of mushroom rings.
        mon->add_ench(ENCH_SLOWLY_DYING);
    }
    else if (mg.cls == MONS_HYPERACTIVE_BALLISTOMYCETE)
        mon->add_ench(ENCH_EXPLODING);
    else if (mons_is_demonspawn(mon->type)
             && draco_or_demonspawn_subspecies(*mon) == MONS_GELID_DEMONSPAWN)
    {
        mon->add_ench(ENCH_ICEMAIL);
    }

    if (mg.cls == MONS_TWISTER || mg.cls == MONS_DIAMOND_OBELISK)
    {
        mon->props["tornado_since"].get_int() = you.elapsed_time;
        mon->add_ench(mon_enchant(ENCH_TORNADO, 0, 0, INFINITE_DURATION));
    }

    // this MUST follow hd initialization!
    if (mons_is_hepliaklqana_ancestor(mon->type))
    {
        set_ancestor_spells(*mon);
        if (mg.props.exists(MON_GENDER_KEY)) // move this out?
            mon->props[MON_GENDER_KEY] = mg.props[MON_GENDER_KEY].get_int();
        mon->props["dbname"] = mons_class_name(mon->type);
    }

    if (mon->type == MONS_HELLBINDER || mon->type == MONS_CLOUD_MAGE)
    {
        mon->props[MON_GENDER_KEY] = random_choose(GENDER_FEMALE, GENDER_MALE,
                                                   GENDER_NEUTER);
    }

    if (mon->has_spell(SPELL_OZOCUBUS_ARMOUR))
    {
        const int power = (mon->spell_hd(SPELL_OZOCUBUS_ARMOUR) * 15) / 10;
        mon->add_ench(mon_enchant(ENCH_OZOCUBUS_ARMOUR,
                                  20 + random2(power) + random2(power),
                                  mon));
    }

    if (mon->has_spell(SPELL_SHROUD_OF_GOLUBRIA))
        mon->add_ench(ENCH_SHROUD);

    if (mon->has_spell(SPELL_REPEL_MISSILES))
        mon->add_ench(ENCH_REPEL_MISSILES);

    if (mon->has_spell(SPELL_DEFLECT_MISSILES))
        mon->add_ench(ENCH_DEFLECT_MISSILES);

    if (mon->has_spell(SPELL_CIGOTUVIS_EMBRACE))
        mon->add_ench(ENCH_BONE_ARMOUR);

    mon->flags |= MF_JUST_SUMMONED;

    // Don't leave shifters in their starting shape.
    if (mg.cls == MONS_SHAPESHIFTER || mg.cls == MONS_GLOWING_SHAPESHIFTER)
    {
        no_messages nm;
        monster_polymorph(mon, RANDOM_MONSTER);

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
            env.mid_cache.erase(mon->mid);
            mon->reset();
            mgrd(fpos) = NON_MONSTER;
            return 0;
        }
        else
            mon->colour = wpn->get_colour();
    }
    else if (mons_class_itemuse(mg.cls) >= MONUSE_STARTING_EQUIPMENT)
    {
        give_item(mon, place.absdepth(), summoned);
        // Give these monsters a second weapon. - bwr
        if (mons_class_wields_two_weapons(mg.cls))
            give_weapon(mon, place.absdepth());

        unwind_var<int> save_speedinc(mon->speed_increment);
        mon->wield_melee_weapon(MB_FALSE);
    }

    if (mon->type == MONS_SLIME_CREATURE && mon->blob_size > 1)
    {
        // Boost HP to what it would have been if it had grown this
        // big by merging.
        mon->hit_points     *= mon->blob_size;
        mon->max_hit_points *= mon->blob_size;
    }

    if (monster_can_submerge(mon, grd(fpos)) && !summoned)
        mon->add_ench(ENCH_SUBMERGED);

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
        // Instead of looking for dancing weapons, look for Tukima's dance.
        // Dancing weapons can be created with shadow creatures. {due}
        mon->mark_summoned(mg.abjuration_duration,
                           mg.summon_type != SPELL_TUKIMAS_DANCE,
                           mg.summon_type);

        if (mg.summon_type > 0 && mg.summoner && !(mg.flags & MG_DONT_CAP))
        {
            // If this is a band member created by shadow creatures, link its
            // ID and don't count it against the summon cap
            if ((mg.summon_type == SPELL_SHADOW_CREATURES
                 || mg.summon_type == SPELL_WEAVE_SHADOWS)
                 && leader)
            {
                mon->props["summon_id"].get_int() = leader->mid;
            }
            else
            {
                summoned_monster(mon, mg.summoner,
                                static_cast<spell_type>(mg.summon_type));
            }
        }
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

        if (mg.summoner != nullptr && mg.summoner->alive()
            && mg.summoner->type == MONS_MARA)
        {
            blame_prefix = "woven by ";
        }

        if (mg.cls == MONS_DANCING_WEAPON)
            blame_prefix = "animated by ";

        if (mg.summon_type == SPELL_SPECTRAL_CLOUD)
            blame_prefix = "called from beyond by ";
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
    else if (mg.summoner != nullptr && mg.summoner->alive()
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
                                 + sum->full_name(DESC_A)));
            if (sum->props.exists("blame"))
            {
                const CrawlVector& oldblame = sum->props["blame"].get_vector();
                for (const auto &bl : oldblame)
                    mons_add_blame(mon, bl.get_string());
            }
        }
    }

    // Initialise (very) ugly things and pandemonium demons.
    if (mon->type == MONS_UGLY_THING
        || mon->type == MONS_VERY_UGLY_THING)
    {
        ghost_demon ghost;
        colour_t force_colour;
        if (mg.colour < COLOUR_UNDEF)
            force_colour = COLOUR_UNDEF;
        else
            force_colour = mg.colour;
        ghost.init_ugly_thing(mon->type == MONS_VERY_UGLY_THING, false,
                              force_colour);
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
            // Dancing weapons are placed at pretty high power. Remember, the
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
                                           mg.props[TUKIMA_POWER].get_int() : 100);
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

    if (crawl_state.game_is_arena())
        arena_placed_monster(mon);
    else if (!crawl_state.generating_level && !dont_place && you.can_see(*mon))
    {
        if (mg.flags & MG_DONT_COME)
            mon->seen_context = SC_JUST_SEEN;
    }

    // Area effects can produce additional messages, and thus need to be
    // done after come in view ones.
    if (mon->type == MONS_TWISTER && !dont_place)
        _place_twister_clouds(mon);

    if (!(mg.flags & MG_FORCE_BEH)
        && !crawl_state.game_is_arena()
        && !crawl_state.generating_level)
    {
        gozag_set_bribe(mon);
    }

    return mon;
}

// Check base monster class against zombie type and position if set.
static bool _good_zombie(monster_type base, monster_type cs,
                         const coord_def& pos)
{
    base = fixup_zombie_type(cs, base);

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
    monster_type corpse_type = mons_species(mt);

    // XXX: nonbase draconian leave base draconian corpses.
    if (mt != MONS_DRACONIAN && corpse_type == MONS_DRACONIAN)
        corpse_type = random_draconian_monster_species();

    // Zombifiability in general.
    if (!mons_class_can_leave_corpse(corpse_type))
        return true;
    // Monsters that don't really exist
    if (mons_class_flag(mt, M_UNFINISHED))
        return true;
    // Monsters that can have derived undead, but never randomly generated.
    if (mons_class_flag(mt, M_NO_GEN_DERIVED))
        return true;
    if (!mons_zombie_size(corpse_type) || mons_is_unique(mt))
        return true;
    if (!(mons_class_holiness(corpse_type) & MH_NATURAL))
        return true;
    if (!_good_zombie(corpse_type, zombie_kind, pos))
        return true;
    return positioned_monster_picker::veto(mt);
}

static bool _mc_too_slow_for_zombies(monster_type mon)
{
    // zombies slower than the player are boring!
    return mons_class_zombie_base_speed(mons_species(mon)) < BASELINE_DELAY;
}

/**
 * Pick a local monster type that's suitable for turning into a corpse.
 *
 * @param place     The branch/level that the monster type should come from,
 *                  if possible. (Not guaranteed for e.g. branches with no
 *                  corpses.)
 * @return          A monster type that can be used to fill out a corpse.
 */
monster_type pick_local_corpsey_monster(level_id place)
{
    return pick_local_zombifiable_monster(place, MONS_NO_MONSTER, coord_def(),
                                          true);
}

monster_type pick_local_zombifiable_monster(level_id place,
                                            monster_type cs,
                                            const coord_def& pos,
                                            bool for_corpse)
{
    const bool really_in_d = place.branch == BRANCH_DUNGEON;

    if (place.branch == BRANCH_ZIGGURAT)
    {
        // Get Zigs something reasonable to work with, if there's no place
        // explicitly defined.
        place = level_id(BRANCH_DEPTHS, 14 - (27 - place.depth) / 3);
    }
    else
    {
        // Zombies tend to be weaker than their normal counterparts;
        // thus, make them OOD proportional to the current dungeon depth.
        place.depth += 1 + div_rand_round(place.absdepth(), 5);
    }

    zombie_picker picker = zombie_picker(pos, cs);

    place.depth = max(1, min(place.depth, branch_ood_cap(place.branch)));

    const bool need_veto = really_in_d && !for_corpse;
    mon_pick_vetoer veto = need_veto ? _mc_too_slow_for_zombies : nullptr;

    // try to grab a proper zombifiable monster
    monster_type mt = picker.pick_with_veto(zombie_population(place.branch),
                                            place.depth, MONS_0, veto);
    // there might not be one in this branch - if we can't find one, try
    // elsewhere
    if (!mt)
        mt = pick_monster_all_branches(place.absdepth(), picker);

    ASSERT(mons_class_can_be_zombified(mons_species(mt)));
    return mt;
}

void roll_zombie_hp(monster* mon)
{
    ASSERT(mon); // TODO: change to monster &mon
    ASSERT(mons_class_is_zombified(mon->type));

    const int avg_hp_10x = derived_undead_avg_hp(mon->type,
                                                 mon->get_hit_dice());
    const int hp = hit_points(avg_hp_10x);
    mon->max_hit_points = max(hp, 1);
    mon->hit_points     = mon->max_hit_points;
}

void define_zombie(monster* mon, monster_type ztype, monster_type cs)
{
#if TAG_MAJOR_VERSION == 34
    // Upgrading monster enums is a losing battle, they sneak through too many
    // channels, like env props, etc. So convert them on placement, too.
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

    // Set type to the original type to calculate appropriate stats.
    mon->type         = ztype;
    mon->base_monster = MONS_PROGRAM_BUG;
    define_monster(*mon);

    // Zombies and such can't cast spells, except they should still be
    // able to make tentacles!
    monster_spells oldspells = mon->spells;
    mon->spells.clear();
    for (const mon_spell_slot &slot : oldspells)
        if (slot.spell == SPELL_CREATE_TENTACLES)
            mon->spells.push_back(slot);

    // handle zombies with jobs & ghostdemon zombies; they otherwise
    // wouldn't store enough information for us to recreate them right.
    if (mons_is_job(ztype) || mons_is_ghost_demon(ztype))
    {
        mon->props[ZOMBIE_BASE_AC_KEY] = mon->base_armour_class();
        mon->props[ZOMBIE_BASE_EV_KEY] = mon->base_evasion();
    }

    mon->type         = cs;
    mon->base_monster = ztype;

    mon->colour       = COLOUR_INHERIT;
    mon->speed        = (cs == MONS_SPECTRAL_THING
                            ? mons_class_base_speed(mon->base_monster)
                            : mons_class_zombie_base_speed(mon->base_monster));

    // Turn off all melee ability flags except dual-wielding.
    mon->flags       &= (~MF_MELEE_MASK | MF_TWO_WEAPONS);

    // Turn off regeneration if the base monster cannot regenerate.
    // This is needed for e.g. spectral things of non-regenerating
    // monsters.
    if (!mons_class_can_regenerate(mon->base_monster))
        mon->flags   |= MF_NO_REGEN;

    roll_zombie_hp(mon);
}

bool downgrade_zombie_to_skeleton(monster* mon)
{
    if (mon->type != MONS_ZOMBIE || !mons_skeleton(mon->base_monster))
        return false;

    const int old_hp    = mon->hit_points;
    const int old_maxhp = mon->max_hit_points;

    mon->type           = MONS_SKELETON;
    mon->speed          = mons_class_zombie_base_speed(mon->base_monster);
    roll_zombie_hp(mon);

    // Scale the skeleton HP to the zombie HP.
    mon->hit_points     = old_hp * mon->max_hit_points / old_maxhp;
    mon->hit_points     = max(mon->hit_points, 1);

    return true;
}

/// Under what conditions should a band spawn with a monster?
struct band_conditions {
    int chance_denom; ///< A 1/x chance for the band to appear.
    int min_depth; ///< The minimum absdepth for the band.
    function<bool()> custom_condition; ///< Additional conditions.

    /// Are the conditions met?
    bool met() const
    {
        return (!chance_denom || one_chance_in(chance_denom))
               && (!min_depth || env.absdepth0 >= min_depth)
               && (!custom_condition || custom_condition());
    }
};

/// Information about a band of followers that may spawn with some monster.
struct band_info {
    /// The type of the band; used to determine the type of followers.
    band_type type;
    /// The min & max # of followers; doesn't count the leader.
    random_var range;
    /// Should the band followers try very hard to stick to the leader?
    bool natural_leader;
};

/// One or more band_infos, with conditions.
struct band_set {
    /// When should the band actually be generated?
    band_conditions conditions;
    /// The bands to be selected between, with equal weight.
    vector<band_info> bands;
};

static const band_conditions centaur_band_condition
    = { 3, 10, []() { return !player_in_branch(BRANCH_SHOALS); }};

// warrior & mage spawn alone more frequently at shallow depths of Snake
static const band_conditions naga_band_condition
    = { 0, 0, []() { return !player_in_branch(BRANCH_SNAKE)
                            || x_chance_in_y(you.depth, 5); }};

/// can we spawn dracs in the current branch?
static const function<bool()> drac_branch
    = []() { return player_in_connected_branch(); };

/// non-classed draconian band description
static const band_set basic_drac_set = { {3, 19, drac_branch},
                                         {{BAND_DRACONIAN, {2, 5} }}};

/// classed draconian band description
static const band_set classy_drac_set = { {0, 21, drac_branch},
                                          {{BAND_DRACONIAN, {3, 7} }}};

// javelineer & impaler
static const band_conditions mf_band_condition = { 0, 0, []() {
    return !player_in_branch(BRANCH_DEPTHS) &&
          (!player_in_branch(BRANCH_SHOALS) || x_chance_in_y(you.depth, 5));
}};

/// For each monster, what band or bands may follow them?
static const map<monster_type, band_set> bands_by_leader = {
    { MONS_ORC,             { {2}, {{ BAND_ORCS, {2, 5} }}}},
    { MONS_ORC_WIZARD,      { {}, {{ BAND_ORCS, {2, 5} }}}},
    { MONS_ORC_PRIEST,      { {}, {{ BAND_ORC_WARRIOR, {2, 5} }}}},
    { MONS_ORC_WARRIOR,     { {}, {{ BAND_ORC_WARRIOR, {2, 5} }}}},
    { MONS_ORC_WARLORD,     { {}, {{ BAND_ORC_KNIGHT, {8, 16}, true }}}},
    { MONS_SAINT_ROKA,      { {}, {{ BAND_ORC_KNIGHT, {8, 16}, true }}}},
    { MONS_ORC_KNIGHT,      { {}, {{ BAND_ORC_KNIGHT, {3, 7}, true }}}},
    { MONS_ORC_HIGH_PRIEST, { {}, {{ BAND_ORC_KNIGHT, {4, 8}, true }}}},
    { MONS_BIG_KOBOLD,      { {0, 4}, {{ BAND_KOBOLDS, {2, 8} }}}},
    { MONS_KILLER_BEE,      { {}, {{ BAND_KILLER_BEES, {2, 6} }}}},
    { MONS_CAUSTIC_SHRIKE,  { {}, {{ BAND_CAUSTIC_SHRIKE, {2, 5} }}}},
    { MONS_SHARD_SHRIKE,    { {}, {{ BAND_SHARD_SHRIKE, {1, 4} }}}},
    { MONS_FLYING_SKULL,    { {}, {{ BAND_FLYING_SKULLS, {2, 6} }}}},
    { MONS_SLIME_CREATURE,  { {}, {{ BAND_SLIME_CREATURES, {2, 6} }}}},
    { MONS_YAK,             { {}, {{ BAND_YAKS, {2, 6} }}}},
    { MONS_VERY_UGLY_THING, { {0, 19}, {{ BAND_VERY_UGLY_THINGS, {2, 6} }}}},
    { MONS_UGLY_THING,      { {0, 13}, {{ BAND_UGLY_THINGS, {2, 6} }}}},
    { MONS_HELL_HOUND,      { {}, {{ BAND_HELL_HOUNDS, {2, 5} }}}},
    { MONS_JACKAL,          { {}, {{ BAND_JACKALS, {1, 4} }}}},
    { MONS_MARGERY,         { {}, {{ BAND_HELL_KNIGHTS, {4, 8}, true }}}},
    { MONS_HELL_KNIGHT,     { {}, {{ BAND_HELL_KNIGHTS, {4, 8} }}}},
    { MONS_JOSEPHINE,       { {}, {{ BAND_JOSEPHINE, {3, 6}, true }}}},
    { MONS_NECROMANCER,     { {}, {{ BAND_NECROMANCER, {3, 6}, true }}}},
    { MONS_VAMPIRE_MAGE,    { {3}, {{ BAND_JIANGSHI, {2, 4}, true }}}},
    { MONS_JIANGSHI,        { {}, {{ BAND_JIANGSHI, {0, 2} }}}},
    { MONS_GNOLL,           { {0, 1}, {{ BAND_GNOLLS, {2, 4} }}}},
    { MONS_GNOLL_SHAMAN,    { {}, {{ BAND_GNOLLS, {3, 6} }}}},
    { MONS_GNOLL_SERGEANT,  { {}, {{ BAND_GNOLLS, {3, 6} }}}},
    { MONS_DEATH_KNIGHT,    { {0, 0, []() { return x_chance_in_y(2, 3); }},
                                  {{ BAND_DEATH_KNIGHT, {3, 5}, true }}}},
    { MONS_GRUM,            { {}, {{ BAND_WOLVES, {2, 5}, true }}}},
    { MONS_WOLF,            { {}, {{ BAND_WOLVES, {2, 6} }}}},
    { MONS_CENTAUR_WARRIOR, { centaur_band_condition,
                                  {{ BAND_CENTAURS, {2, 6}, true }}}},
    { MONS_CENTAUR, { centaur_band_condition, {{ BAND_CENTAURS, {2, 6} }}}},
    { MONS_YAKTAUR_CAPTAIN, { {2}, {{ BAND_YAKTAURS, {2, 5}, true }}}},
    { MONS_YAKTAUR,         { {2}, {{ BAND_YAKTAURS, {2, 5} }}}},
    { MONS_DEATH_YAK,       { {}, {{ BAND_DEATH_YAKS, {2, 6} }}}},
    { MONS_INSUBSTANTIAL_WISP, { {}, {{ BAND_INSUBSTANTIAL_WISPS, {2, 6} }}}},
    { MONS_OGRE_MAGE,       { {}, {{ BAND_OGRE_MAGE, {4, 8} }}}},
    { MONS_BALRUG,          { {}, {{ BAND_BALRUG, {2, 5}, true }}}},
    { MONS_CACODEMON,       { {}, {{ BAND_CACODEMON, {1, 4}, true }}}},
    { MONS_EXECUTIONER,     { {2}, {{ BAND_EXECUTIONER, {1, 4}, true }}}},
    { MONS_PANDEMONIUM_LORD, { {}, {{ BAND_PANDEMONIUM_LORD, {1, 4}, true }}}},
    { MONS_HELLWING,        { {2}, {{ BAND_HELLWING, {1, 5} }}}},
    { MONS_DEEP_ELF_KNIGHT, { {2}, {{ BAND_DEEP_ELF_KNIGHT, {3, 5} }}}},
    { MONS_DEEP_ELF_ARCHER, { {2}, {{ BAND_DEEP_ELF_KNIGHT, {3, 5} }}}},
    { MONS_DEEP_ELF_HIGH_PRIEST, { {2}, {{ BAND_DEEP_ELF_HIGH_PRIEST, {3, 7},
                                           true }}}},
    { MONS_KOBOLD_DEMONOLOGIST, { {2}, {{ BAND_KOBOLD_DEMONOLOGIST, {3, 9} }}}},
    { MONS_GUARDIAN_SERPENT, { {}, {{ BAND_GUARDIAN_SERPENT, {2, 6} }}}},
    { MONS_NAGA_MAGE,       { naga_band_condition, {{ BAND_NAGAS, {2, 5} }}}},
    { MONS_NAGA_WARRIOR,    { naga_band_condition, {{ BAND_NAGAS, {2, 5} }}}},
    { MONS_NAGA_SHARPSHOOTER, { {2}, {{ BAND_NAGA_SHARPSHOOTER, {1, 4} }}}},
    { MONS_NAGA_RITUALIST,  { {}, {{ BAND_NAGA_RITUALIST, {3, 6} }}}},
    { MONS_RIVER_RAT,       { {}, {{ BAND_GREEN_RATS, {4, 10} }}}},
    { MONS_HELL_RAT,        { {}, {{ BAND_HELL_RATS, {3, 7} }}}},
    { MONS_DREAM_SHEEP,     { {}, {{ BAND_DREAM_SHEEP, {3, 7} }}}},
    { MONS_GHOUL,           { {}, {{ BAND_GHOULS, {2, 5} }}}},
    { MONS_KIRKE,           { {}, {{ BAND_HOGS, {3, 8}, true }}}},
    { MONS_HOG,             { {}, {{ BAND_HOGS, {1, 4} }}}},
    { MONS_VAMPIRE_MOSQUITO, { {}, {{ BAND_VAMPIRE_MOSQUITOES, {1, 4} }}}},
    { MONS_FIRE_BAT,        { {}, {{ BAND_FIRE_BATS, {1, 4} }}}},
    { MONS_DEEP_TROLL_EARTH_MAGE, { {}, {{ BAND_DEEP_TROLLS, {3, 6} }}}},
    { MONS_DEEP_TROLL_SHAMAN, { {}, {{ BAND_DEEP_TROLL_SHAMAN, {3, 6} }}}},
    { MONS_HELL_HOG,        { {}, {{ BAND_HELL_HOGS, {2, 4} }}}},
    { MONS_BOGGART,         { {}, {{ BAND_BOGGARTS, {2, 5} }}}},
    { MONS_PRINCE_RIBBIT,   { {}, {{ BAND_BLINK_FROGS, {2, 5}, true }}}},
    { MONS_BLINK_FROG,      { {}, {{ BAND_BLINK_FROGS, {2, 5} }}}},
    { MONS_WIGHT,           { {}, {{ BAND_WIGHTS, {2, 5} }}}},
    { MONS_ANCIENT_CHAMPION, { {2}, {{ BAND_SKELETAL_WARRIORS, {2, 5}, true}}}},
    { MONS_SKELETAL_WARRIOR, { {}, {{ BAND_SKELETAL_WARRIORS, {2, 5}, true }}}},
    { MONS_CYCLOPS,         { { 0, 0, []() {
        return player_in_branch(BRANCH_SHOALS); }},
                                  {{ BAND_DREAM_SHEEP, {2, 5}, true }}}},
    { MONS_ALLIGATOR,       { { 5, 0, []() {
        return !player_in_branch(BRANCH_LAIR); }},
                                  {{ BAND_ALLIGATOR, {1, 2}, true }}}},
    { MONS_POLYPHEMUS,      { {}, {{ BAND_POLYPHEMUS, {3, 6}, true }}}},
    { MONS_HARPY,           { {}, {{ BAND_HARPIES, {2, 5} }}}},
    { MONS_SALTLING,        { {}, {{ BAND_SALTLINGS, {2, 4} }}}},
    // Journey -- Added Draconian Packs
    { MONS_WHITE_DRACONIAN, basic_drac_set },
    { MONS_RED_DRACONIAN,   basic_drac_set },
    { MONS_PURPLE_DRACONIAN, basic_drac_set },
    { MONS_YELLOW_DRACONIAN, basic_drac_set },
    { MONS_BLACK_DRACONIAN, basic_drac_set },
    { MONS_GREEN_DRACONIAN, basic_drac_set },
    { MONS_GREY_DRACONIAN, basic_drac_set },
    { MONS_PALE_DRACONIAN, basic_drac_set },
    { MONS_DRACONIAN_STORMCALLER, classy_drac_set },
    { MONS_DRACONIAN_MONK, classy_drac_set },
    { MONS_DRACONIAN_SCORCHER, classy_drac_set },
    { MONS_DRACONIAN_KNIGHT, classy_drac_set },
    { MONS_DRACONIAN_ANNIHILATOR, classy_drac_set },
    { MONS_DRACONIAN_SHIFTER, classy_drac_set },
    // yup, scary
    { MONS_TIAMAT,          { {}, {{ BAND_DRACONIAN, {8, 15}, true }}}},
    { MONS_ILSUIW,          { {}, {{ BAND_ILSUIW, {3, 6} }}}},
    { MONS_AZRAEL,          { {}, {{ BAND_AZRAEL, {4, 9}, true }}}},
    { MONS_DUVESSA,         { {}, {{ BAND_DUVESSA, {1, 2} }}}},
    { MONS_KHUFU,           { {}, {{ BAND_KHUFU, {3, 4}, true }}}},
    { MONS_GOLDEN_EYE,      { {}, {{ BAND_GOLDEN_EYE, {1, 6} }}}},
    { MONS_PIKEL,           { {}, {{ BAND_PIKEL, {4, 5}, true }}}},
    { MONS_MERFOLK_AQUAMANCER, { {}, {{ BAND_MERFOLK_AQUAMANCER, {3, 5} }}}},
    { MONS_MERFOLK_JAVELINEER, { mf_band_condition,
                                  {{ BAND_MERFOLK_JAVELINEER, {2, 5} }}}},
    { MONS_MERFOLK_IMPALER, { mf_band_condition,
                                  {{ BAND_MERFOLK_IMPALER, {2, 5} }}}},
    { MONS_ELEPHANT,        { {}, {{ BAND_ELEPHANT, {2, 6} }}}},
    { MONS_REDBACK,         { {}, {{ BAND_REDBACK, {1, 6} }}}},
    { MONS_ENTROPY_WEAVER,  { {}, {{ BAND_REDBACK, {1, 5} }}}},
    { MONS_JUMPING_SPIDER,  { {2}, {{ BAND_JUMPING_SPIDER, {1, 6} }}}},
    { MONS_TARANTELLA,      { {2}, {{ BAND_TARANTELLA, {1, 5} }}}},
    { MONS_VAULT_WARDEN,    { {}, {{ BAND_YAKTAURS, {2, 6}, true },
                                   { BAND_VAULT_WARDEN, {2, 5}, true }}}},
    { MONS_IRONHEART_PRESERVER, { {}, {{ BAND_DEEP_TROLLS, {3, 6}, true },
                                    { BAND_DEEP_ELF_HIGH_PRIEST, {3, 7}, true },
                                    { BAND_OGRE_MAGE_EXTERN, {4, 8}, true }}}},
    { MONS_TENGU_CONJURER,  { {2}, {{ BAND_TENGU, {1, 2}, true }}}},
    { MONS_TENGU_WARRIOR,   { {2}, {{ BAND_TENGU, {1, 2}, true }}}},
    { MONS_SOJOBO,          { {}, {{ BAND_SOJOBO, {2, 3}, true }}}},
    { MONS_SPRIGGAN_AIR_MAGE, { {}, {{ BAND_AIR_ELEMENTALS, {2, 4}, true }}}},
    { MONS_SPRIGGAN_RIDER,  { {3}, {{ BAND_SPRIGGAN_RIDERS, {1, 3} }}}},
    { MONS_SPRIGGAN_BERSERKER, { {2}, {{ BAND_SPRIGGANS, {2, 4} }}}},
    { MONS_SPRIGGAN_DEFENDER, { {}, {{ BAND_SPRIGGAN_ELITES, {2, 5}, true }}}},
    { MONS_THE_ENCHANTRESS, { {}, {{ BAND_ENCHANTRESS, {6, 11}, true }}}},
    { MONS_SHAMBLING_MANGROVE, { {4}, {{ BAND_SPRIGGAN_RIDERS, {1, 2} }}}},
    { MONS_VAMPIRE_KNIGHT,  { {4}, {{ BAND_PHANTASMAL_WARRIORS, {2, 3} }}}},
    { MONS_RAIJU,           { {}, {{ BAND_RAIJU, {2, 4} }}}},
    { MONS_SALAMANDER_MYSTIC, { {}, {{ BAND_SALAMANDERS, {2, 4} }}}},
    { MONS_MONSTROUS_DEMONSPAWN, { {2, 0, []() {
        return !player_in_branch(BRANCH_WIZLAB); // hack for wizlab_wucad_mu
    }},                             {{ BAND_MONSTROUS_DEMONSPAWN, {1, 3}}}}},
    { MONS_GELID_DEMONSPAWN, { {2}, {{ BAND_GELID_DEMONSPAWN, {1, 3} }}}},
    { MONS_INFERNAL_DEMONSPAWN, { {2}, {{ BAND_INFERNAL_DEMONSPAWN, {1, 3} }}}},
    { MONS_TORTUROUS_DEMONSPAWN, {{2}, {{ BAND_TORTUROUS_DEMONSPAWN, {1, 3}}}}},
    { MONS_BLOOD_SAINT,     { {}, {{ BAND_BLOOD_SAINT, {1, 4} }}}},
    { MONS_WARMONGER,       { {}, {{ BAND_WARMONGER, {2, 4} }}}},
    { MONS_CORRUPTER,       { {}, {{ BAND_CORRUPTER, {1, 4} }}}},
    { MONS_BLACK_SUN,       { {}, {{ BAND_BLACK_SUN, {2, 4} }}}},
    { MONS_VASHNIA,         { {}, {{ BAND_VASHNIA, {3, 6}, true }}}},
    { MONS_ROBIN,           { {}, {{ BAND_ROBIN, {10, 13}, true }}}},
    { MONS_RAKSHASA,        { {2, 0, []() {
        return branch_has_monsters(you.where_are_you)
            || !vault_mon_types.empty();
    }},                           {{ BAND_RANDOM_SINGLE, {1, 2} }}}},
    { MONS_CEREBOV,         { {}, {{ BAND_CEREBOV, {5, 8}, true }}}},
    { MONS_GLOORX_VLOQ,     { {}, {{ BAND_GLOORX_VLOQ, {5, 8}, true }}}},
    { MONS_MNOLEG,          { {}, {{ BAND_MNOLEG, {5, 8}, true }}}},
    { MONS_LOM_LOBON,       { {}, {{ BAND_LOM_LOBON, {5, 8}, true }}}},
    { MONS_DEATH_SCARAB,    { {}, {{ BAND_DEATH_SCARABS, {3, 6} }}}},
    { MONS_SERAPH,          { {}, {{ BAND_HOLIES, {1, 4}, true }}}},
    { MONS_IRON_GIANT,      { {}, {{ BAND_ANCIENT_CHAMPIONS, {2, 3}, true }}}},
    { MONS_SPARK_WASP,      { {0, 0, []() {
        return you.where_are_you == BRANCH_DEPTHS;
    }},                           {{ BAND_SPARK_WASPS, {1, 4} }}}},
    { MONS_HOWLER_MONKEY,   { {2, 6}, {{ BAND_HOWLER_MONKEY, {1, 3} }}}},
    { MONS_FLOATING_EYE,   { {0, 0, []() {
        return branch_has_monsters(you.where_are_you)
            || !vault_mon_types.empty();
    }},                           {{ BAND_RANDOM_SINGLE, {1, 2} }}}},
    { MONS_EYE_OF_DRAINING, { {0, 0, []() {
        return branch_has_monsters(you.where_are_you)
            || !vault_mon_types.empty();
    }},                           {{ BAND_RANDOM_SINGLE, {1, 2} }}}},
    { MONS_MELIAI,          { {}, {{ BAND_MELIAI, {2, 3} }}}},
    { MONS_DANCING_WEAPON,  { {0, 0, []() {
        return you.where_are_you == BRANCH_DESOLATION;
    }},                            {{ BAND_DANCING_WEAPONS, {2, 3} }}}},
    { MONS_MOLTEN_GARGOYLE,  { {0, 0, []() {
        return you.where_are_you == BRANCH_DESOLATION;
    }},                            {{ BAND_MOLTEN_GARGOYLES, {2, 3} }}}},

    // special-cased band-sizes
    { MONS_SPRIGGAN_DRUID,  { {3}, {{ BAND_SPRIGGAN_DRUID, {0, 1} }}}},
    { MONS_THRASHING_HORROR, { {}, {{ BAND_THRASHING_HORRORS, {0, 1} }}}},
};

static band_type _choose_band(monster_type mon_type, int *band_size_p,
                              bool *natural_leader_p)
{
#ifdef DEBUG_MON_CREATION
    mprf(MSGCH_DIAGNOSTICS, "in _choose_band()");
#endif
    // Access outparameters by reference, or local dummies if they were null.
    int bs, &band_size = band_size_p ? *band_size_p : bs;
    bool nl, &natural_leader = natural_leader_p ? *natural_leader_p : nl;

    // Band size describes the number of monsters in addition to
    // the band leader.
    band_size = 0; // Single monster, no band.
    natural_leader = false;
    band_type band = BAND_NO_BAND;

    const band_set* bands = map_find(bands_by_leader, mon_type);
    if (bands && bands->conditions.met())
    {
        ASSERT(bands->bands.size() > 0);
        const band_info& band_desc = *random_iterator(bands->bands);
        band = band_desc.type;
        band_size = band_desc.range.roll();
        natural_leader = band_desc.natural_leader;
    }

    // special cases...
    switch (mon_type)
    {
    case MONS_TORPOR_SNAIL:
    {
        natural_leader = true; // snails are natural-born leaders. fact.

        struct band_choice { band_type band; int min; int max; };
        typedef vector<pair<band_choice, int>> band_weights;
        const static map<branch_type, band_weights> band_pick =
        {
            // branch              band             #min #max weight
            { BRANCH_LAIR,   { { { BAND_YAKS,          2, 5 },  3 },
                               { { BAND_DEATH_YAKS,    1, 2 },  1 },
                               { { BAND_DREAM_SHEEP,   2, 4 },  1 },
                             } },
            { BRANCH_SPIDER, { { { BAND_REDBACK,       2, 4 },  1 },
                               { { BAND_RANDOM_SINGLE, 1, 1 },  1 },
                             } },
            { BRANCH_DEPTHS, { { { BAND_RANDOM_SINGLE, 1, 1 },  1 },
                             } },
        };

        if (const auto *weights = map_find(band_pick, you.where_are_you))
        {
            if (const auto *chosen = random_choose_weighted(*weights))
            {
                band = chosen->band;
                band_size = random_range(chosen->min, chosen->max);
            }
        }
        break;
    }

    case MONS_SATYR:
        if (!one_chance_in(3))
        {
            natural_leader = true;
            band = one_chance_in(5) ? BAND_FAUN_PARTY : BAND_FAUNS;
            band_size = 3 + random2(2);
        }
        break;

    case MONS_FAUN:
        if (!one_chance_in(3))
        {
            band = coinflip() ? BAND_FAUNS : BAND_FAUN_PARTY;
            band_size = 2 + random2(2);
        }
        break;

    case MONS_SPRIGGAN_DRUID:
        if (band != BAND_NO_BAND)
            band_size = (one_chance_in(4) ? 3 : 2);
        break;

    case MONS_THRASHING_HORROR:
        // XXX: rewrite this - wrong & bad if horrors aren't in abyss
        band_size = random2(min(brdepth[BRANCH_ABYSS], you.depth));
        break;

    default: ;
    }

    if (band != BAND_NO_BAND && band_size == 0)
        band = BAND_NO_BAND;

    if (band_size >= BIG_BAND)
        band_size = BIG_BAND - 1;

    return band;
}

/// a weighted list of possible monsters for a given band slot.
typedef vector<pair<monster_type, int>> member_possibilites;

/**
 * For each band type, a list of weighted lists of monsters that can appear
 * in that band.
 *
 * Each element in the list, other than the last, is a list of candidates for
 * that slot in the band. The final element lists candidates for all further
 * slots.
 *
 * An example: Let's say we have {BAND_FOO,
 *                                  {{{MONS_A, 1}},
 *                                   {{MONS_B, 1}},
 *                                   {{MONS_C, 1}, {MONS_D, 1}}}}.
 * This indicates that the first member of these bands (not the leader, but
 * their first follower) will be a MONS_A, the second will be a MONS_B, and
 * the third, fourth, etc monsters will either be MONS_C or MONS_D with equal
 * likelihood.
 */
static const map<band_type, vector<member_possibilites>> band_membership = {
    { BAND_HOGS,                {{{MONS_HOG, 1}}}},
    { BAND_YAKS,                {{{MONS_YAK, 1}}}},
    { BAND_FAUNS,               {{{MONS_FAUN, 1}}}},
    { BAND_WOLVES,              {{{MONS_WOLF, 1}}}},
    { BAND_DUVESSA,             {{{MONS_DOWAN, 1}}}},
    { BAND_GNOLLS,              {{{MONS_GNOLL, 1}}}},
    { BAND_HARPIES,             {{{MONS_HARPY, 1}}}},
    { BAND_RAIJU,               {{{MONS_RAIJU, 1}}}},
    { BAND_PIKEL,               {{{MONS_SLAVE, 1}}}},
    { BAND_WIGHTS,              {{{MONS_WIGHT, 1}}}},
    { BAND_JACKALS,             {{{MONS_JACKAL, 1}}}},
    { BAND_KOBOLDS,             {{{MONS_KOBOLD, 1}}}},
    { BAND_JOSEPHINE,           {{{MONS_WRAITH, 1}}}},
    { BAND_MELIAI,              {{{MONS_MELIAI, 1}}}},
    { BAND_BOGGARTS,            {{{MONS_BOGGART, 1}}}},
    { BAND_CENTAURS,            {{{MONS_CENTAUR, 1}}}},
    { BAND_YAKTAURS,            {{{MONS_YAKTAUR, 1}}}},
    { BAND_MERFOLK_IMPALER,     {{{MONS_MERFOLK, 1}}}},
    { BAND_MERFOLK_JAVELINEER,  {{{MONS_MERFOLK, 1}}}},
    { BAND_ELEPHANT,            {{{MONS_ELEPHANT, 1}}}},
    { BAND_FIRE_BATS,           {{{MONS_FIRE_BAT, 1}}}},
    { BAND_HELL_HOGS,           {{{MONS_HELL_HOG, 1}}}},
    { BAND_HELL_RATS,           {{{MONS_HELL_RAT, 1}}}},
    { BAND_JIANGSHI,            {{{MONS_JIANGSHI, 1}}}},
    { BAND_ALLIGATOR,           {{{MONS_ALLIGATOR, 1}}}},
    { BAND_DEATH_YAKS,          {{{MONS_DEATH_YAK, 1}}}},
    { BAND_GREEN_RATS,          {{{MONS_RIVER_RAT, 1}}}},
    { BAND_BLINK_FROGS,         {{{MONS_BLINK_FROG, 1}}}},
    { BAND_GOLDEN_EYE,          {{{MONS_GOLDEN_EYE, 1}}}},
    { BAND_HELL_HOUNDS,         {{{MONS_HELL_HOUND, 1}}}},
    { BAND_KILLER_BEES,         {{{MONS_KILLER_BEE, 1}}}},
    { BAND_SALAMANDERS,         {{{MONS_SALAMANDER, 1}}}},
    { BAND_SPARK_WASPS,         {{{MONS_SPARK_WASP, 1}}}},
    { BAND_UGLY_THINGS,         {{{MONS_UGLY_THING, 1}}}},
    { BAND_DREAM_SHEEP,         {{{MONS_DREAM_SHEEP, 1}}}},
    { BAND_DEATH_SCARABS,       {{{MONS_DEATH_SCARAB, 1}}}},
    { BAND_FLYING_SKULLS,       {{{MONS_FLYING_SKULL, 1}}}},
    { BAND_SHARD_SHRIKE,        {{{MONS_SHARD_SHRIKE, 1}}}},
    { BAND_SOJOBO,              {{{MONS_TENGU_REAVER, 1}}}},
    { BAND_AIR_ELEMENTALS,      {{{MONS_AIR_ELEMENTAL, 1}}}},
    { BAND_HOWLER_MONKEY,       {{{MONS_HOWLER_MONKEY, 1}}}},
    { BAND_CAUSTIC_SHRIKE,      {{{MONS_CAUSTIC_SHRIKE, 1}}}},
    { BAND_DANCING_WEAPONS,     {{{MONS_DANCING_WEAPON, 1}}}},
    { BAND_SLIME_CREATURES,     {{{MONS_SLIME_CREATURE, 1}}}},
    { BAND_SPRIGGAN_RIDERS,     {{{MONS_SPRIGGAN_RIDER, 1}}}},
    { BAND_MOLTEN_GARGOYLES,    {{{MONS_MOLTEN_GARGOYLE, 1}}}},
    { BAND_SKELETAL_WARRIORS,   {{{MONS_SKELETAL_WARRIOR, 1}}}},
    { BAND_THRASHING_HORRORS,   {{{MONS_THRASHING_HORROR, 1}}}},
    { BAND_VAMPIRE_MOSQUITOES,  {{{MONS_VAMPIRE_MOSQUITO, 1}}}},
    { BAND_ANCIENT_CHAMPIONS,   {{{MONS_ANCIENT_CHAMPION, 1}}}},
    { BAND_EXECUTIONER,         {{{MONS_ABOMINATION_LARGE, 1}}}},
    { BAND_VASHNIA,             {{{MONS_NAGA_SHARPSHOOTER, 1}}}},
    { BAND_INSUBSTANTIAL_WISPS, {{{MONS_INSUBSTANTIAL_WISP, 1}}}},
    { BAND_PHANTASMAL_WARRIORS, {{{MONS_PHANTASMAL_WARRIOR, 1}}}},
    { BAND_DEEP_ELF_KNIGHT,     {{{MONS_DEEP_ELF_MAGE, 92},
                                  {MONS_DEEP_ELF_KNIGHT, 24},
                                  {MONS_DEEP_ELF_ARCHER, 24},
                                  {MONS_DEEP_ELF_DEATH_MAGE, 3},
                                  {MONS_DEEP_ELF_DEMONOLOGIST, 2},
                                  {MONS_DEEP_ELF_ANNIHILATOR, 2},
                                  {MONS_DEEP_ELF_SORCERER, 2}}}},
    { BAND_DEEP_ELF_HIGH_PRIEST, {{{MONS_DEEP_ELF_MAGE, 5},
                                   {MONS_DEEP_ELF_KNIGHT, 2},
                                   {MONS_DEEP_ELF_ARCHER, 2},
                                   {MONS_DEEP_ELF_DEMONOLOGIST, 1},
                                   {MONS_DEEP_ELF_ANNIHILATOR, 1},
                                   {MONS_DEEP_ELF_SORCERER, 1},
                                   {MONS_DEEP_ELF_DEATH_MAGE, 1}}}},
    { BAND_BALRUG,              {{{MONS_SUN_DEMON, 1},
                                  {MONS_RED_DEVIL, 1}}}},
    { BAND_HELLWING,            {{{MONS_HELLWING, 1},
                                  {MONS_SMOKE_DEMON, 1}}}},
    { BAND_CACODEMON,           {{{MONS_SIXFIRHY, 1},
                                  {MONS_ORANGE_DEMON, 1}}}},
    { BAND_NECROMANCER,         {{{MONS_ZOMBIE, 2},
                                  {MONS_SKELETON, 2},
                                  {MONS_SIMULACRUM, 1}}}},
    { BAND_HELL_KNIGHTS,        {{{MONS_HELL_KNIGHT, 3},
                                  {MONS_NECROMANCER, 1}}}},
    { BAND_POLYPHEMUS,          {{{MONS_CATOBLEPAS, 1}},

                                 {{MONS_DEATH_YAK, 1}}}},
    { BAND_VERY_UGLY_THINGS,    {{{MONS_UGLY_THING, 3},
                                  {MONS_VERY_UGLY_THING, 4}}}},
    { BAND_ORCS,                {{{MONS_ORC_PRIEST, 6},
                                  {MONS_ORC_WIZARD, 7},
                                  {MONS_ORC, 35}}}},
    { BAND_ORC_WARRIOR,         {{{MONS_ORC_PRIEST, 5},
                                  {MONS_ORC_WIZARD, 7},
                                  {MONS_ORC,       22}}}},
    // XXX: For Beogh punishment, ogres and trolls look out of place...
    // (For normal generation, they're okay, of course.)
    { BAND_ORC_KNIGHT,          {{{MONS_ORC, 12},
                                  {MONS_ORC_WARRIOR, 9},
                                  {MONS_WARG, 2},
                                  {MONS_ORC_WIZARD, 2},
                                  {MONS_ORC_PRIEST, 2},
                                  {MONS_OGRE, 1},
                                  {MONS_TROLL, 1},
                                  {MONS_ORC_SORCERER, 1}}}},
    { BAND_OGRE_MAGE,           {{{MONS_TWO_HEADED_OGRE, 2},
                                  {MONS_OGRE, 1}}}},
    { BAND_OGRE_MAGE_EXTERN,    {{{MONS_OGRE_MAGE, 1}},
                                 {{MONS_TWO_HEADED_OGRE, 1},
                                  {MONS_OGRE, 2}}}},
    { BAND_KOBOLD_DEMONOLOGIST, {{{MONS_KOBOLD, 4},
                                  {MONS_BIG_KOBOLD, 2},
                                  {MONS_KOBOLD_DEMONOLOGIST, 1}}}},
    // Favor tougher naga suited to melee, compared to normal naga bands
    { BAND_GUARDIAN_SERPENT,    {{{MONS_NAGA_MAGE, 5}, {MONS_NAGA_WARRIOR, 10}},

                                 {{MONS_NAGA_MAGE, 5}, {MONS_NAGA_WARRIOR, 10},
                                  {MONS_SALAMANDER, 3}, {MONS_NAGA, 12}},

                                 {{MONS_SALAMANDER, 3}, {MONS_NAGA, 12}}}},
    { BAND_NAGA_RITUALIST,      {{{MONS_BLACK_MAMBA, 15},
                                  {MONS_MANA_VIPER, 7},
                                  {MONS_ANACONDA, 4}}}},
    { BAND_NAGA_SHARPSHOOTER,   {{{MONS_NAGA_SHARPSHOOTER, 1},
                                  {MONS_NAGA, 2}}}},
    { BAND_GHOULS,              {{{MONS_GHOUL, 4},
                                  {MONS_NECROPHAGE, 3},
                                  {MONS_BOG_BODY, 2}}}},
    { BAND_ILSUIW,              {{{MONS_MERFOLK_SIREN, 6},
                                  {MONS_MERFOLK, 3},
                                  {MONS_MERFOLK_JAVELINEER, 2},
                                  {MONS_MERFOLK_IMPALER, 2}}}},
    { BAND_AZRAEL,              {{{MONS_FIRE_ELEMENTAL, 1},
                                  {MONS_HELL_HOUND, 1}}}},
    { BAND_KHUFU,               {{{MONS_GREATER_MUMMY, 1},
                                  {MONS_MUMMY, 1}}}},
    { BAND_MERFOLK_AQUAMANCER,  {{{MONS_MERFOLK, 4},
                                  {MONS_WATER_ELEMENTAL, 11}}}},
    { BAND_DEEP_TROLLS,         {{{MONS_DEEP_TROLL, 18},
                                  {MONS_DEEP_TROLL_EARTH_MAGE, 3},
                                  {MONS_DEEP_TROLL_SHAMAN, 3}}}},
    { BAND_DEEP_TROLL_SHAMAN,   {{{MONS_DEEP_TROLL, 18},
                                  {MONS_IRON_TROLL, 8},
                                  {MONS_DEEP_TROLL_EARTH_MAGE, 3},
                                  {MONS_DEEP_TROLL_SHAMAN, 3}}}},
    { BAND_REDBACK,             {{{MONS_REDBACK, 6},
                                  {MONS_TARANTELLA, 1},
                                  {MONS_JUMPING_SPIDER, 1}}}},
    { BAND_JUMPING_SPIDER,      {{{MONS_JUMPING_SPIDER, 12},
                                  {MONS_WOLF_SPIDER, 8},
                                  {MONS_ORB_SPIDER, 7},
                                  {MONS_REDBACK, 5},
                                  {MONS_DEMONIC_CRAWLER, 2}}}},
    { BAND_TARANTELLA,          {{{MONS_TARANTELLA, 10},
                                  {MONS_REDBACK, 8},
                                  {MONS_WOLF_SPIDER, 7},
                                  {MONS_ORB_SPIDER, 3},
                                  {MONS_DEMONIC_CRAWLER, 2}}}},

    { BAND_VAULT_WARDEN,        {{{MONS_VAULT_SENTINEL, 4},
                                  {MONS_IRONBRAND_CONVOKER, 6},
                                  {MONS_IRONHEART_PRESERVER, 5}},
        // one fancy pal, and a 50% chance of another
                                {{MONS_VAULT_SENTINEL, 4},
                                 {MONS_IRONBRAND_CONVOKER, 6},
                                 {MONS_IRONHEART_PRESERVER, 5},
                                 {MONS_VAULT_GUARD, 15}},

                                {{MONS_VAULT_GUARD, 1}}}},

    { BAND_FAUN_PARTY,          {{{MONS_MERFOLK_SIREN, 1}},

                                 {{MONS_FAUN, 1}}}},

    { BAND_TENGU,               {{{MONS_TENGU_WARRIOR, 1},
                                  {MONS_TENGU_CONJURER, 1}}}},

    { BAND_SPRIGGANS,           {{{MONS_SPRIGGAN_RIDER, 11},
                                  {MONS_SPRIGGAN_AIR_MAGE, 4},
                                  {MONS_SPRIGGAN_BERSERKER, 3}}}},

    { BAND_SPRIGGAN_ELITES,     {{{MONS_SPRIGGAN_DEFENDER, 18},
                                  {MONS_SPRIGGAN_RIDER, 11},
                                  {MONS_SPRIGGAN_AIR_MAGE, 4},
                                  {MONS_SPRIGGAN_BERSERKER, 3}},

                                 {{MONS_SPRIGGAN_RIDER, 11},
                                  {MONS_SPRIGGAN_AIR_MAGE, 4},
                                  {MONS_SPRIGGAN_BERSERKER, 3}}}},

    { BAND_ENCHANTRESS,         {{{MONS_SPRIGGAN_DEFENDER, 1}},

                                 {{MONS_SPRIGGAN_DEFENDER, 1}},

                                 {{MONS_SPRIGGAN_DEFENDER, 1}},

                                 {{MONS_SPRIGGAN_AIR_MAGE, 1}},

                                 {{MONS_SPRIGGAN_AIR_MAGE, 1},
                                  {MONS_SPRIGGAN_BERSERKER, 1},
                                  {MONS_SPRIGGAN_RIDER, 1},
                                  {MONS_SPRIGGAN, 3}}}},

    { BAND_SPRIGGAN_DRUID,      {{{MONS_SPRIGGAN, 2},
                                  {MONS_SPRIGGAN_RIDER, 1}}}},

    { BAND_SALAMANDER_ELITES,   {{{MONS_SALAMANDER_MYSTIC, 1},
                                  {MONS_SALAMANDER, 1}},

                                 {{MONS_SALAMANDER, 1}}}},
    { BAND_ROBIN,               {{{MONS_GOBLIN, 3},
                                  {MONS_HOBGOBLIN, 1}}}},

    { BAND_CEREBOV,             {{{MONS_BRIMSTONE_FIEND, 1}},

                                 {{MONS_BALRUG, 1},
                                  {MONS_SUN_DEMON, 3},
                                  {MONS_EFREET, 3}}}},

    { BAND_GLOORX_VLOQ,         {{{MONS_CURSE_SKULL, 1}},

                                 {{MONS_EXECUTIONER, 1}},

                                 {{MONS_SHADOW_DEMON, 1},
                                  {MONS_DEMONIC_CRAWLER, 3},
                                  {MONS_SHADOW_WRAITH, 3}}}},

    { BAND_MNOLEG,              {{{MONS_TENTACLED_MONSTROSITY, 1}},

                                 {{MONS_TENTACLED_MONSTROSITY, 1},
                                  {MONS_CACODEMON, 2},
                                  {MONS_ABOMINATION_LARGE, 3},
                                  {MONS_VERY_UGLY_THING, 3},
                                  {MONS_NEQOXEC, 3}}}},

    { BAND_LOM_LOBON,           {{{MONS_SPRIGGAN_AIR_MAGE, 1},
                                  {MONS_TITAN, 1},
                                  {MONS_LICH, 1},
                                  {MONS_DRACONIAN_ANNIHILATOR, 2},
                                  {MONS_DEEP_ELF_ANNIHILATOR, 2},
                                  {MONS_GLOWING_ORANGE_BRAIN, 2},
                                  {MONS_BLIZZARD_DEMON, 2},
                                  {MONS_GREEN_DEATH, 2},
                                  {MONS_RAKSHASA, 4},
                                  {MONS_WIZARD, 4}}}},

    { BAND_HOLIES,              {{{MONS_ANGEL, 100},
                                  {MONS_CHERUB, 80},
                                  {MONS_DAEVA, 50},
                                  {MONS_OPHAN, 1}}}}, // !?

    // one supporter, and maybe more
    { BAND_SALTLINGS,           {{{MONS_GUARDIAN_SERPENT, 1},
                                  {MONS_IRONBRAND_CONVOKER, 1},
                                  {MONS_RAGGED_HIEROPHANT, 2},
                                  {MONS_SERVANT_OF_WHISPERS, 2},
                                  {MONS_PEACEKEEPER, 2}},

                                 {{MONS_SALTLING, 150},
                                  {MONS_RAGGED_HIEROPHANT, 5},
                                  {MONS_SERVANT_OF_WHISPERS, 5},
                                  {MONS_PEACEKEEPER, 5},
                                  {MONS_MOLTEN_GARGOYLE, 5},
                                  {MONS_IRONBRAND_CONVOKER, 2},
                                  {MONS_GUARDIAN_SERPENT, 2},
                                  {MONS_IMPERIAL_MYRMIDON, 2}}}},
};

/**
 * Return the type of the nth monster in a band.
 *
 * @param band      The type of band
 * @param which     The index of the monster (starting from 1)
 * @return          The type of monster to create
 */
static monster_type _band_member(band_type band, int which,
                                 level_id parent_place, bool allow_ood)
{
    if (band == BAND_NO_BAND)
        return MONS_PROGRAM_BUG;
    ASSERT(which > 0);

    if (const auto *membership = map_find(band_membership, band))
    {
        ASSERT(membership->size() > 0);
        const size_t idx = min<size_t>(which, membership->size()) - 1;
        const auto *choice = random_choose_weighted((*membership)[idx]);
        ASSERT(choice); /* empty weights vector */
        return *choice;
    }

    switch (band)
    {

    case BAND_PANDEMONIUM_LORD:
        if (one_chance_in(7))
        {
            return random_choose_weighted(50, MONS_LICH,
                                          10, MONS_ANCIENT_LICH);
        }
        else if (one_chance_in(6))
        {
            return random_choose_weighted(50, MONS_ABOMINATION_SMALL,
                                          40, MONS_ABOMINATION_LARGE,
                                          10, MONS_TENTACLED_MONSTROSITY);
        }
        else
        {
            return summon_any_demon(random_choose_weighted(
                                          50, RANDOM_DEMON_COMMON,
                                          20, RANDOM_DEMON_GREATER,
                                          10, RANDOM_DEMON),
                                    true);
        }
        break;

    case BAND_NAGAS:
        if (which == 1 && coinflip() || which == 2 && one_chance_in(4))
        {
            return random_choose_weighted( 8, MONS_NAGA_WARRIOR,
                                          11, MONS_NAGA_MAGE,
                                           6, MONS_NAGA_RITUALIST,
                                           8, MONS_NAGA_SHARPSHOOTER,
                                           6, MONS_SALAMANDER_MYSTIC);
        }
        else
            return one_chance_in(7) ? MONS_SALAMANDER : MONS_NAGA;


    case BAND_DRACONIAN:
        if (env.absdepth0 >= 24 && x_chance_in_y(13, 40))
        {
            // Hack: race is rolled elsewhere.
            return random_choose_weighted(
                1, MONS_DRACONIAN_STORMCALLER,
                2, MONS_DRACONIAN_KNIGHT,
                2, MONS_DRACONIAN_MONK,
                2, MONS_DRACONIAN_SHIFTER,
                2, MONS_DRACONIAN_ANNIHILATOR,
                2, MONS_DRACONIAN_SCORCHER);
        }

        return random_draconian_monster_species();

    case BAND_DEATH_KNIGHT:
        if (!player_in_branch(BRANCH_DUNGEON)
            && which == 1 && x_chance_in_y(2, 3))
        {
            return one_chance_in(3) ? MONS_GHOUL : MONS_FLAYED_GHOST;
        }
        else
            return random_choose_weighted(5, MONS_WRAITH,
                                          6, MONS_FREEZING_WRAITH,
                                          3, MONS_PHANTASMAL_WARRIOR,
                                          3, MONS_SKELETAL_WARRIOR);

    case BAND_MONSTROUS_DEMONSPAWN:
        if (which == 1 || x_chance_in_y(2, 3))
        {
            return random_choose_weighted( 2, MONS_DEMONIC_CRAWLER,
                                           2, MONS_SIXFIRHY,
                                           3, MONS_MONSTROUS_DEMONSPAWN);
        }
        return random_demonspawn_monster_species();

    case BAND_GELID_DEMONSPAWN:
        if (which == 1 || x_chance_in_y(2, 3))
        {
            return random_choose_weighted( 4, MONS_ICE_DEVIL,
                                           3, MONS_GELID_DEMONSPAWN);
        }
        return random_demonspawn_monster_species();

    case BAND_INFERNAL_DEMONSPAWN:
        if (which == 1 || x_chance_in_y(2, 3))
        {
            return random_choose_weighted( 2, MONS_RED_DEVIL,
                                           2, MONS_SUN_DEMON,
                                           3, MONS_INFERNAL_DEMONSPAWN);
        }
        return random_demonspawn_monster_species();

    case BAND_TORTUROUS_DEMONSPAWN:
        if (which == 1 || x_chance_in_y(2, 3))
        {
            return random_choose_weighted( 2, MONS_ORANGE_DEMON,
                                           2, MONS_SIXFIRHY,
                                           3, MONS_TORTUROUS_DEMONSPAWN);
        }
        return random_demonspawn_monster_species();

    case BAND_BLOOD_SAINT:
        if (which == 1 || which == 2 && one_chance_in(3))
        {
            if (x_chance_in_y(2, 3))
                return coinflip() ? MONS_BALRUG : MONS_BLIZZARD_DEMON;
            else
                return random_demonspawn_job();
        }
        return random_demonspawn_monster_species();

    case BAND_WARMONGER:
        if (which == 1 || which == 2 && one_chance_in(3))
        {
            if (x_chance_in_y(2, 3))
                return one_chance_in(4) ? MONS_EXECUTIONER : MONS_REAPER;
            else
                return random_demonspawn_job();
        }
        return random_demonspawn_monster_species();

    case BAND_CORRUPTER:
        if (which == 1 || which == 2 && one_chance_in(3))
        {
            if (x_chance_in_y(2, 3))
                return one_chance_in(4) ? MONS_CACODEMON : MONS_SHADOW_DEMON;
            else
                return random_demonspawn_job();
        }
        return random_demonspawn_monster_species();

    case BAND_BLACK_SUN:
        if (which == 1 || which == 2 && one_chance_in(3))
        {
            if (x_chance_in_y(2, 3))
                return one_chance_in(3) ? MONS_LOROCYPROCA : MONS_SOUL_EATER;
            else
                return random_demonspawn_job();
        }
        return random_demonspawn_monster_species();

    case BAND_RANDOM_SINGLE:
    {
        monster_type tmptype = MONS_PROGRAM_BUG;
        coord_def tmppos;
        dungeon_char_type tmpfeat;
        return resolve_monster_type(RANDOM_BANDLESS_MONSTER, tmptype,
                                    PROX_ANYWHERE, &tmppos, 0, &tmpfeat,
                                    &parent_place, nullptr, allow_ood);
    }

    default:
        return NUM_MONSTERS;
    }
}

/// Check to make sure that all band types are handled.
void debug_bands()
{
    vector<int> unhandled_bands;
    for (int i = 0; i < NUM_BANDS; ++i)
        if (_band_member((band_type)i, 1, BRANCH_DUNGEON, true) == NUM_MONSTERS)
            unhandled_bands.push_back(i);

    if (!unhandled_bands.empty())
    {
        const string fails = "Unhandled bands: "
           + comma_separated_fn(unhandled_bands.begin(), unhandled_bands.end(),
                                [](int i){ return make_stringf("%d", i); });

        dump_test_fails(fails, "mon-bands");
    }
}

// PUBLIC FUNCTION -- mons_place().

static monster_type _pick_zot_exit_defender()
{
    if (one_chance_in(11))
    {
#ifdef DEBUG_MON_CREATION
        mprf(MSGCH_DIAGNOSTICS, "Create a pandemonium lord!");
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

        if (one_chance_in(11))
            return MONS_SERAPH;

        return MONS_PANDEMONIUM_LORD;
    }

    return random_choose_weighted(
        30, RANDOM_DEMON_COMMON,
        30, RANDOM_DEMON,
        20, pick_monster_no_rarity(BRANCH_PANDEMONIUM),
        15, MONS_ORB_GUARDIAN,
        5, RANDOM_DEMON_GREATER);
}

monster* mons_place(mgen_data mg)
{
#ifdef DEBUG_MON_CREATION
    mprf(MSGCH_DIAGNOSTICS, "in mons_place()");
#endif
    const int mon_count = count_if(begin(menv), end(menv),
                                   [] (const monster &mons) -> bool
                                   { return mons.type != MONS_NO_MONSTER; });

    if (mg.cls == WANDERING_MONSTER)
    {
        if (mon_count > MAX_MONSTERS - 50)
            return 0;

#ifdef DEBUG_MON_CREATION
        mprf(MSGCH_DIAGNOSTICS, "Set class RANDOM_MONSTER");
#endif
        mg.cls = RANDOM_MONSTER;
    }

    // All monsters have been assigned? {dlb}
    if (mon_count >= MAX_MONSTERS - 1)
        return 0;

    // This gives a slight challenge to the player as they ascend the
    // dungeon with the Orb.
    if (_is_random_monster(mg.cls) && player_on_orb_run()
        && !player_in_branch(BRANCH_ABYSS) && !mg.summoned())
    {
#ifdef DEBUG_MON_CREATION
        mprf(MSGCH_DIAGNOSTICS, "Call _pick_zot_exit_defender()");
#endif
        mg.cls    = _pick_zot_exit_defender();
        mg.flags |= MG_PERMIT_BANDS;
    }
    else if (_is_random_monster(mg.cls))
        mg.flags |= MG_PERMIT_BANDS;

    if (mg.behaviour == BEH_COPY)
    {
        mg.behaviour = (mg.summoner && mg.summoner->is_player())
                        ? BEH_FRIENDLY
                        : SAME_ATTITUDE(mg.summoner->as_monster());
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

        if (!(mg.flags & MG_FORCE_BEH) && !crawl_state.game_is_arena())
            player_angers_monster(creation);

        behaviour_event(creation, ME_EVAL);
    }

    if (mg.flags & MG_AUTOFOE && (creation->attitude == ATT_FRIENDLY
                                  || mg.behaviour == BEH_CHARMED))
    {
        set_nearest_monster_foe(creation, true);
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
        :  feat_wanted(grdw), maxdistance(maxdist),
           best_distance(0), nfound(0)
    {
        start = pos;
    }

    // This is an overload, not an override!
    coord_def pathfind()
    {
        set_floodseed(start);
        return travel_pathfind::pathfind(RMODE_CONNECTIVITY);
    }

    bool path_flood(const coord_def &c, const coord_def &dc) override
    {
        if (best_distance && traveled_distance > best_distance)
            return true;

        if (!in_bounds(dc)
            || (maxdistance > 0 && traveled_distance > maxdistance))
        {
            return false;
        }
        if (!_feat_compatible(feat_wanted, grd(dc)))
        {
            if (passable.count(grd(dc)))
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

    return in_bounds(p) ? p : coord_def(-1, -1);
}

coord_def find_newmons_square(monster_type mons_class, const coord_def &p,
                              const monster* viable_mon)
{
    coord_def empty;
    coord_def pos(-1, -1);

    if (mons_class == WANDERING_MONSTER)
        mons_class = RANDOM_MONSTER;

    // Might be better if we chose a space and tried to match the monster
    // to it in the case of RANDOM_MONSTER, that way if the target square
    // is surrounded by water or lava this function would work.  -- bwr

    if (find_habitable_spot_near(p, mons_class, 2, true, empty, viable_mon))
        pos = empty;

    return pos;
}

bool can_spawn_mushrooms(coord_def where)
{
    cloud_struct *cloud = cloud_at(where);
    if (!cloud)
        return true;
    if (you_worship(GOD_FEDHAS)
        && (cloud->whose == KC_YOU || cloud->whose == KC_FRIENDLY))
    {
        return true;
    }

    monster dummy;
    dummy.type = MONS_TOADSTOOL;
    define_monster(dummy);

    return actor_cloud_immune(&dummy, *cloud);
}

conduct_type player_will_anger_monster(monster_type type)
{
    monster dummy;
    dummy.type = type;

    // no spellcasting/etc zombies currently; pick something that always works
    if (mons_class_is_zombified(type))
        define_zombie(&dummy, MONS_GOBLIN, type);
    else
        define_monster(dummy);

    return player_will_anger_monster(dummy);
}

/**
 * Does the player's current religion conflict with the given monster? If so,
 * why?
 *
 * XXX: this should ideally return a list of conducts that can be filtered by
 *      callers by god; we're duplicating godconduct.cc right now.
 *
 * @param mon   The monster in question.
 * @return      The reason the player's religion conflicts with the monster
 *              (e.g. DID_EVIL for evil monsters), or DID_NOTHING.
 */
conduct_type player_will_anger_monster(const monster &mon)
{
    if (player_mutation_level(MUT_NO_LOVE) && !mons_is_conjured(mon.type))
    {
        // Player angers all real monsters
        return DID_SACRIFICE_LOVE;
    }

    if (is_good_god(you.religion) && mon.evil())
        return DID_EVIL;

    if (you_worship(GOD_FEDHAS)
        && ((mon.holiness() & MH_UNDEAD && !mon.is_insubstantial())
            || mon.has_corpse_violating_spell()))
    {
        return DID_CORPSE_VIOLATION;
    }

    if (is_evil_god(you.religion) && mon.is_holy())
        return DID_HOLY;

    if (you_worship(GOD_ZIN))
    {
        if (mon.how_unclean())
            return DID_UNCLEAN;
        if (mon.how_chaotic())
            return DID_CHAOS;
    }
    if (god_hates_spellcasting(you.religion) && mon.is_actual_spellcaster())
        return DID_SPELL_CASTING;

    if (you_worship(GOD_DITHMENOS) && mons_is_fiery(mon))
        return DID_FIRE;

    return DID_NOTHING;
}

bool player_angers_monster(monster* mon)
{
    ASSERT(mon); // XXX: change to monster &mon

    // Get the drawbacks, not the benefits... (to prevent e.g. demon-scumming).
    conduct_type why = player_will_anger_monster(*mon);
    if (why && mon->wont_attack())
    {
        mon->attitude = ATT_HOSTILE;
        mon->del_ench(ENCH_CHARM);
        behaviour_event(mon, ME_ALERT, &you);

        if (you.can_see(*mon))
        {
            const string mname = mon->name(DESC_THE);

            switch (why)
            {
            case DID_EVIL:
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
            case DID_FIRE:
                mprf("%s is enraged by your darkness!", mname.c_str());
                break;
            case DID_SACRIFICE_LOVE:
                mprf("%s can only feel hate for you!", mname.c_str());
                break;
            default:
                mprf("%s is enraged by a buggy thing about you!", mname.c_str());
                break;
            }
        }

        return true;
    }

    return false;
}

monster* create_monster(mgen_data mg, bool fail_msg)
{
    ASSERT(in_bounds(mg.pos)); // otherwise it's a guaranteed fail

    const monster_type montype = fixup_zombie_type(mg.cls, mg.base_type);

    monster *summd = 0;

    if (!mg.force_place()
        || monster_at(mg.pos)
        || you.pos() == mg.pos && !fedhas_passthrough_class(mg.cls)
        || !mons_class_can_pass(montype, grd(mg.pos)))
    {
        // Gods other than Xom will try to avoid placing their monsters
        // directly in harm's way.
        if (mg.god != GOD_NO_GOD && mg.god != GOD_XOM)
        {
            monster dummy;
            const monster_type resistless_mon = MONS_HUMAN;
            // If the type isn't known yet assume no resists or anything.
            dummy.type         = needs_resolution(mg.cls) ? resistless_mon
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
            mg.pos = find_newmons_square(montype, mg.pos, &dummy);
        }
        else
            mg.pos = find_newmons_square(montype, mg.pos);
    }

    if (in_bounds(mg.pos))
    {
        summd = mons_place(mg);
        // If the arena vetoed the placement then give no fail message.
        if (crawl_state.game_is_arena())
            fail_msg = false;

        if (!summd && fail_msg && you.see_cell(mg.pos))
            mpr("You see a puff of smoke.");
    }

    return summd;
}

bool find_habitable_spot_near(const coord_def& where, monster_type mon_type,
                              int radius, bool allow_centre, coord_def& empty,
                              const monster* viable_mon)
{
    // XXX: A lot of hacks that could be avoided by passing the
    //      monster generation data through.

    int good_count = 0;

    for (radius_iterator ri(where, radius, C_SQUARE, !allow_centre);
         ri; ++ri)
    {
        bool success = false;

        if (actor_at(*ri))
            continue;

        if (!cell_see_cell(where, *ri, LOS_NO_TRANS))
            continue;

        success = monster_habitable_grid(mon_type, grd(*ri));
        if (success && viable_mon)
            success = !mons_avoids_cloud(viable_mon, *ri, true);

        if (success && one_chance_in(++good_count))
            empty = *ri;
    }

    return good_count > 0;
}

static void _get_vault_mon_list(vector<mons_spec> &list);

monster_type random_demon_by_tier(int tier)
{
    switch (tier)
    {
    case 5:
        return random_choose(MONS_CRIMSON_IMP,
                             MONS_QUASIT,
                             MONS_WHITE_IMP,
                             MONS_UFETUBUS,
                             MONS_IRON_IMP,
                             MONS_SHADOW_IMP);
    case 4:
        return random_choose(MONS_ICE_DEVIL,
                             MONS_RUST_DEVIL,
                             MONS_ORANGE_DEMON,
                             MONS_RED_DEVIL,
                             MONS_CHAOS_SPAWN,
                             MONS_HELLWING);
    case 3:
        return random_choose(MONS_SUN_DEMON,
                             MONS_SOUL_EATER,
                             MONS_SMOKE_DEMON,
                             MONS_NEQOXEC,
                             MONS_YNOXINUL,
                             MONS_SIXFIRHY);
    case 2:
        return random_choose(MONS_GREEN_DEATH,
                             MONS_BLIZZARD_DEMON,
                             MONS_BALRUG,
                             MONS_CACODEMON,
                             MONS_HELL_BEAST,
                             MONS_HELLION,
                             MONS_REAPER,
                             MONS_LOROCYPROCA,
                             MONS_TORMENTOR,
                             MONS_SHADOW_DEMON);
    case 1:
        return random_choose(MONS_BRIMSTONE_FIEND,
                             MONS_ICE_FIEND,
                             MONS_TZITZIMITL,
                             MONS_HELL_SENTINEL,
                             MONS_EXECUTIONER);
    default:
        die("invalid demon tier");
    }
}

monster_type summon_any_demon(monster_type dct, bool use_local_demons)
{
    // Draw random demon types in Pan from the local pools first.
    if (use_local_demons
        && player_in_branch(BRANCH_PANDEMONIUM)
        && !one_chance_in(40))
    {
        monster_type typ = MONS_0;
        int count = 0;
        vector<mons_spec> list;
        _get_vault_mon_list(list);
        const bool list_set = !list.empty();
        const int max = list_set ? list.size() : PAN_MONS_ALLOC;
        for (int i = 0; i < max; i++)
        {
            const monster_type cur = list_set ? list[i].monbase
                                              : env.mons_alloc[i];
            if (invalid_monster_type(cur))
                continue;
            if (dct == RANDOM_DEMON && !mons_is_demon(cur)
                || dct == RANDOM_DEMON_LESSER && mons_demon_tier(cur) != 5
                || dct == RANDOM_DEMON_COMMON
                   && mons_demon_tier(cur) != 4
                   && mons_demon_tier(cur) != 3
                || dct == RANDOM_DEMON_GREATER
                   && mons_demon_tier(cur) != 2
                   && mons_demon_tier(cur) != 1)
            {
                continue;
            }
            const int weight = list_set ? list[i].genweight : 1;
            count += weight;
            if (x_chance_in_y(weight, count))
                typ = cur;
        }
        if (count)
            return typ;
    }

    if (dct == RANDOM_DEMON)
    {
        dct = random_choose(RANDOM_DEMON_LESSER, RANDOM_DEMON_COMMON,
                            RANDOM_DEMON_GREATER);
    }

    switch (dct)
    {
    case RANDOM_DEMON_LESSER:
        return random_demon_by_tier(5);

    case RANDOM_DEMON_COMMON:
        if (x_chance_in_y(6, 10))
            return random_demon_by_tier(4);
        else
            return random_demon_by_tier(3);

    case RANDOM_DEMON_GREATER:
        if (x_chance_in_y(6, 10))
            return random_demon_by_tier(2);
        else
            return random_demon_by_tier(1);

    default:
        return dct;
    }
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
    props.erase(VAULT_MON_PLACES_KEY);
    props.erase(VAULT_MON_WEIGHTS_KEY);
    props.erase(VAULT_MON_BANDS_KEY);

    size_t size = list.size();
    if (size == 0)
    {
        setup_vault_mon_list();
        return;
    }

    props[VAULT_MON_TYPES_KEY].new_vector(SV_INT).resize(size);
    props[VAULT_MON_BASES_KEY].new_vector(SV_INT).resize(size);
    props[VAULT_MON_PLACES_KEY].new_vector(SV_LEV_ID).resize(size);
    props[VAULT_MON_WEIGHTS_KEY].new_vector(SV_INT).resize(size);
    props[VAULT_MON_BANDS_KEY].new_vector(SV_BOOL).resize(size);

    CrawlVector &type_vec   = props[VAULT_MON_TYPES_KEY].get_vector();
    CrawlVector &base_vec   = props[VAULT_MON_BASES_KEY].get_vector();
    CrawlVector &place_vec  = props[VAULT_MON_PLACES_KEY].get_vector();
    CrawlVector &weight_vec = props[VAULT_MON_WEIGHTS_KEY].get_vector();
    CrawlVector &band_vec   = props[VAULT_MON_BANDS_KEY].get_vector();

    for (size_t i = 0; i < size; i++)
    {
        const mons_spec &spec = list[i];

        if (spec.place.is_valid())
        {
            ASSERT(branch_has_monsters(spec.place.branch));
            type_vec[i] = -1;
            base_vec[i] = -1;
            place_vec[i] = spec.place;
        }
        else
        {
            ASSERT(!_is_random_monster(spec.type)
                   && !_is_random_monster(spec.monbase));
            type_vec[i] = spec.type;
            base_vec[i] = spec.monbase;
            band_vec[i] = spec.band;
            place_vec[i] = level_id();
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
    ASSERT(props.exists(VAULT_MON_PLACES_KEY));

    CrawlVector &type_vec   = props[VAULT_MON_TYPES_KEY].get_vector();
    CrawlVector &base_vec   = props[VAULT_MON_BASES_KEY].get_vector();
    CrawlVector &place_vec  = props[VAULT_MON_PLACES_KEY].get_vector();
    CrawlVector &weight_vec = props[VAULT_MON_WEIGHTS_KEY].get_vector();
    CrawlVector &band_vec   = props[VAULT_MON_BANDS_KEY].get_vector();

    size_t size = type_vec.size();
    ASSERT(size == base_vec.size());
    ASSERT(size == place_vec.size());
    ASSERT(size == weight_vec.size());
    ASSERT(size == band_vec.size());

    for (size_t i = 0; i < size; i++)
    {
        monster_type type = static_cast<monster_type>(type_vec[i].get_int());
        monster_type base = static_cast<monster_type>(base_vec[i].get_int());
        level_id    place = place_vec[i];

        mons_spec spec;

        if (place.is_valid())
        {
            ASSERT(branch_has_monsters(place.branch));
            spec.place = place;
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
    vault_mon_places.clear();
    vault_mon_weights.clear();
    vault_mon_bands.clear();

    vector<mons_spec> list;
    _get_vault_mon_list(list);

    unsigned int size = list.size();

    vault_mon_types.resize(size);
    vault_mon_bases.resize(size);
    vault_mon_places.resize(size);
    vault_mon_weights.resize(size);
    vault_mon_bands.resize(size);

    for (size_t i = 0; i < size; i++)
    {
        if (list[i].place.is_valid())
        {
            vault_mon_types[i] = -1;
            vault_mon_bases[i] = -1;
            vault_mon_places[i] = list[i].place;
        }
        else
        {
            vault_mon_types[i] = list[i].type;
            vault_mon_bases[i] = list[i].monbase;
            vault_mon_places[i] = level_id();
        }
        vault_mon_bands[i] = list[i].band;
        vault_mon_weights[i] = list[i].genweight;
    }
    if (size)
        dprf(DIAG_MONPLACE, "Level has a custom monster set.");
}
