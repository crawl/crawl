/**
 * @file
 * @brief Functions used when placing monsters in the dungeon.
**/

#include "AppHdr.h"

#include "mon-place.h"
#include "mgen-data.h"

#include <algorithm>
#include <functional>

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
#include "gender-type.h"
#include "ghost.h"
#include "god-abil.h"
#include "god-passive.h" // passive_t::slow_abyss, slow_orb_run
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-pick.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "rltiles/tiledef-player.h"
#endif
#include "tilepick.h"
#include "traps.h"
#include "travel.h"
#include "unwind.h"
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

#define BIG_BAND        20

static monster_type _band_member(band_type band, int which,
                                 level_id parent_place, bool allow_ood);
static band_type _choose_band(monster_type mon_type, int *band_size_p = nullptr,
                              bool *natural_leader_p = nullptr);

static monster* _place_monster_aux(const mgen_data &mg, const monster *leader,
                                   level_id place,
                                   bool force_pos = false,
                                   bool dont_place = false);
static monster* _place_pghost_aux(const mgen_data &mg, const monster *leader,
                                   level_id place,
                                   bool force_pos, bool dont_place);

static int _fill_apostle_band(monster& mons, monster_type* band);

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
           || wanted_feat == DNGN_DEEP_WATER && feat_is_water(actual_feat)
           || wanted_feat == DNGN_FLOOR && feat_has_solid_floor(actual_feat);
}

static bool _hab_requires_mon_flight(dungeon_feature_type g)
{
    return g == DNGN_LAVA || g == DNGN_DEEP_WATER;
}

/**
 * Can this monster survive on a given feature?
 *
 * @param mon         the monster to be checked.
 * @param feat        the feature type that mon might not be able to survive.
 * @returns whether the monster can survive being in/over the feature,
 *          regardless of whether it may be dangerous or harmful.
 */
bool monster_habitable_feat(const monster* mon, dungeon_feature_type feat)
{
    // Zombified monsters enjoy the same habitat as their original,
    // except lava-based monsters.
    const monster_type mt = mons_is_draconian_job(mon->type)
        ? draconian_subspecies(*mon)
        : fixup_zombie_type(mon->type, mons_base_type(*mon));

    bool type_safe = monster_habitable_feat(mt, feat);
    return type_safe || (_hab_requires_mon_flight(feat) && mon->airborne());
}

/**
 * Can monsters of this class survive on on a given feature type?
 *
 * @param mt the monster class to check against
 * @param feat the terrain feature being checked
 */
bool monster_habitable_feat(monster_type mt, dungeon_feature_type feat)
{
    // No monster may be placed in walls etc.
    if (!mons_class_can_pass(mt, feat))
        return false;

#if TAG_MAJOR_VERSION == 34
    // Monsters can't use teleporters, and standing there would look just wrong.
    if (feat == DNGN_TELEPORTER)
        return false;
#endif
    // The kraken is so large it cannot enter shallow water.
    // Its tentacles can, and will, though.
    if ((feat == DNGN_SHALLOW_WATER || feat == DNGN_TOXIC_BOG)
        && mt == MONS_KRAKEN)
    {
        return false;
    }
    // Only eldritch tentacles are allowed to exist on this feature.
    else if (feat == DNGN_MALIGN_GATEWAY)
    {
        return mt == MONS_ELDRITCH_TENTACLE
               || mt == MONS_ELDRITCH_TENTACLE_SEGMENT;
    }

    const dungeon_feature_type feat_preferred =
        habitat2grid(mons_class_primary_habitat(mt));
    const dungeon_feature_type feat_nonpreferred =
        habitat2grid(mons_class_secondary_habitat(mt));

    if (_feat_compatible(feat_preferred, feat)
        || _feat_compatible(feat_nonpreferred, feat))
    {
        return true;
    }

    // [dshaligram] Flying creatures are all HT_LAND, so we
    // only have to check for the additional valid grids of deep
    // water and lava.
    if (_hab_requires_mon_flight(feat) && (mons_class_flag(mt, M_FLIES)))
        return true;

    return false;
}

bool monster_habitable_grid(const monster* mon, const coord_def& pos)
{
    return monster_habitable_feat(mon, env.grid(pos));
}

bool monster_habitable_grid(monster_type mt, const coord_def& pos)
{
    return monster_habitable_feat(mt, env.grid(pos));
}

static int _ood_fuzzspan(level_id &place)
{
    if (place.branch != BRANCH_DUNGEON || place.depth >= 5)
        return 5;

    // Literally any OOD is too nasty for D:1.
    if (place.depth == 1)
        return 0;

    // In early D, since player and enemy strength scale so rapidly
    // with depth, spawn OODs from much closer depths.
    // Only up to D:4 on D:2, up to D:6 on D:3, and D:8 on D:4.
    return place.depth;
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

#ifdef DEBUG_DIAGNOSTICS
    level_id old_place = place;
#endif

    const int fuzzspan = _ood_fuzzspan(place);
    if (fuzzspan && x_chance_in_y(14, 100))
    {
        // We want a left-weighted distribution; slight fuzzing should be much
        // more common than the full depth fuzz. This does mean that OODs are closer
        // to a 6% chance than the 14% implied above, which is a bit silly.
        const int fuzz = random_range(-fuzzspan, fuzzspan, 2);
        if (fuzz > 0)
        {
            place.depth += fuzz;
            dprf("Monster level fuzz: %d (old: %s, new: %s)",
                 fuzz, old_place.describe().c_str(), place.describe().c_str());
        }
    }
}

/**
 * Rate at which random monsters spawn, with lower numbers making
 * them spawn more often (5 or less causes one to spawn about every
 * 5 turns). 0 stops random generation.
 */
static int _get_monster_spawn_rate()
{
    if (player_in_branch(BRANCH_ABYSS))
        return 5 * (have_passive(passive_t::slow_abyss) ? 2 : 1);

    if (player_on_orb_run())
        return have_passive(passive_t::slow_orb_run) ? 36 : 18;

    return 50;
}

//#define DEBUG_MON_CREATION

/**
 * Spawn random monsters.
 */
void spawn_random_monsters()
{
    if (crawl_state.disables[DIS_SPAWNS])
        return;

    if (crawl_state.game_is_arena())
        return;

    if (!player_on_orb_run()
        && !player_in_branch(BRANCH_ABYSS)
        && !player_in_branch(BRANCH_PANDEMONIUM))
    {
        return;
    }

#ifdef DEBUG_MON_CREATION
    mprf(MSGCH_DIAGNOSTICS, "in spawn_random_monsters()");
#endif

    const int rate = _get_monster_spawn_rate();
    if (!x_chance_in_y(5, rate))
        return;

    mgen_data mg(WANDERING_MONSTER);
    if (player_in_branch(BRANCH_PANDEMONIUM)
        && !env.properties.exists("vault_mon_weights")
        && !one_chance_in(40))
    {
        mg.cls = env.mons_alloc[random2(PAN_MONS_ALLOC)];
        mg.flags |= MG_PERMIT_BANDS;
    }

    // Orb spawns. Don't generate orb spawns in Abyss to show some mercy to
    // players that get banished there on the orb run.
    if (player_on_orb_run() && !player_in_branch(BRANCH_ABYSS))
    {
        mg.proximity = PROX_CLOSE_TO_PLAYER;
        mg.foe = MHITYOU;
        // Don't count orb run spawns in the xp_by_level dump
        mg.xp_tracking = XP_UNTRACKED;
    }

    // Deep Abyss can also do orbrun-style spawns in LOS.
    if (player_in_branch(BRANCH_ABYSS)
        && you.depth > 5
        && one_chance_in(10 / (you.depth - 5)))
    {
        mg.proximity = PROX_CLOSE_TO_PLAYER;
        mg.foe = MHITYOU;
    }

    mons_place(mg);
    viewwindow();
    update_screen();
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
           || god_hates_monster(mt);
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

    ASSERT(_is_random_monster(kind) || kind == MONS_NO_MONSTER);

    if (kind == RANDOM_MOBILE_MONSTER)
        return pick_monster(place, mons_class_is_stationary);
    else if (kind == RANDOM_COMPATIBLE_MONSTER)
        return pick_monster(place, _is_incompatible_monster);
    else if (kind == RANDOM_BANDLESS_MONSTER)
        return pick_monster(place, _is_banded_monster);
    else if (crawl_state.game_is_sprint())
        return pick_monster(place, _has_big_aura);
    else
        return pick_monster(place);
}

bool needs_resolution(monster_type mon_type)
{
    return mon_type == RANDOM_DRACONIAN || mon_type == RANDOM_BASE_DRACONIAN
           || mon_type == RANDOM_NONBASE_DRACONIAN
           || mon_type >= RANDOM_DEMON_LESSER && mon_type <= RANDOM_DEMON
           || _is_random_monster(mon_type);
}

monster_type resolve_monster_type(monster_type mon_type,
                                  monster_type &base_type,
                                  proximity_type proximity,
                                  coord_def *pos,
                                  unsigned mmask,
                                  level_id *place,
                                  bool *want_band,
                                  bool allow_ood)
{
    if (want_band)
        *want_band = false;

    if (mon_type == RANDOM_DRACONIAN)
    {
        if (base_type != MONS_NO_MONSTER)
        {
            // Pick the requested colour, if applicable.
            if (coinflip())
                mon_type = base_type;
            else
                mon_type = draconian_job_for_colour(base_type);
        }
        else
        {
            // Pick any random drac.
            if (coinflip())
                mon_type = random_draconian_monster_species();
            else
                mon_type = random_draconian_job();
        }
    }
    else if (mon_type == RANDOM_BASE_DRACONIAN)
        mon_type = random_draconian_monster_species();
    else if (mon_type == RANDOM_NONBASE_DRACONIAN)
        mon_type = random_draconian_job();
    else if (mon_type >= RANDOM_DEMON_LESSER && mon_type <= RANDOM_DEMON)
        mon_type = summon_any_demon(mon_type, true);

    // (2) Take care of non-draconian random monsters.
    else if (_is_random_monster(mon_type))
    {
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
                                             place, want_band, allow_ood);
                }
                return mon_type;
            }
        }

        // Now pick a monster of the given branch and level.
        mon_type = pick_random_monster(*place, mon_type, place, allow_ood);
    }
    return mon_type;
}

monster_type fixup_zombie_type(const monster_type cls,
                               const monster_type base_type)
{
    // Yredelemnul's bound souls can fly - they aren't bound by their old flesh.
    // Other zombies, regrettably, still are.
    // XXX: consider replacing the latter check with monster_class_flies(cls)
    // and adjusting vaults that use spectral krakens.
    if (!mons_class_is_zombified(cls) || cls == MONS_BOUND_SOUL)
        return cls;
    // For generation purposes, don't treat simulacra of lava enemies as
    // being able to place on lava.
    if (mons_class_secondary_habitat(base_type) == HT_LAVA)
        return cls;
    return base_type;
}

// Checks if the monster is ok to place at mg_pos. If force_location
// is true, then we'll be less rigorous in our checks, in particular
// allowing land monsters to be placed in shallow water.
static bool _valid_monster_generation_location(const mgen_data &mg,
                                                const coord_def &mg_pos)
{
    if (!in_bounds(mg_pos)
        || monster_at(mg_pos)
        || you.pos() == mg_pos && !fedhas_passthrough_class(mg.cls))
    {
        ASSERT(!crawl_state.generating_level
                || !in_bounds(mg_pos)
                || you.pos() != mg_pos
                || you.where_are_you == BRANCH_ABYSS);
        return false;
    }

    const monster_type montype = fixup_zombie_type(mg.cls, mg.base_type);
    if (!monster_habitable_grid(montype, mg_pos)
        || (mg.behaviour != BEH_FRIENDLY
            && is_sanctuary(mg_pos)
            && !mons_is_tentacle_segment(montype)))
    {
        return false;
    }

    // If we've been requested to place amphibious monsters on solid ground, do
    // so if possible.
    if (mg.flags & MG_PREFER_LAND)
    {
        habitat_type habitat = mons_class_primary_habitat(montype);
        if (habitat != HT_WATER && habitat != HT_LAVA
            && !feat_has_solid_floor(env.grid(mg_pos)))
        {
            return false;
        }
    }

    bool close_to_player = grid_distance(you.pos(), mg_pos) <= LOS_RADIUS;
    if (mg.proximity == PROX_AWAY_FROM_PLAYER && close_to_player
        || mg.proximity == PROX_CLOSE_TO_PLAYER && !close_to_player)
    {
        ASSERT(!crawl_state.generating_level || you.where_are_you == BRANCH_ABYSS);
        return false;
    }
    // Check that the location is not proximal to level stairs.
    else if (mg.proximity == PROX_AWAY_FROM_STAIRS)
    {
        for (distance_iterator di(mg_pos, false, false, LOS_RADIUS); di; ++di)
            if (feat_is_stone_stair(env.grid(*di)))
                return false;
    }
    // Check that the location is not proximal to an area where the player
    // begins the game.
    else if (mg.proximity == PROX_AWAY_FROM_ENTRANCE)
    {
        for (distance_iterator di(mg_pos, false, false, LOS_RADIUS); di; ++di)
        {
            // for consistency, this should happen regardless of whether the
            // player is starting on D:1
            if (env.absdepth0 == 0)
            {
                if (feat_is_branch_exit(env.grid(*di))
                    // We may be checking before branch exit cleanup.
                    || feat_is_stone_stair_up(env.grid(*di)))
                {
                    return false;
                }
            }
            else if (env.absdepth0 == starting_absdepth())
            {
                // Delvers start on a (specific) D:5 downstairs.
                if (env.grid(*di) == DNGN_STONE_STAIRS_DOWN_I)
                    return false;
            }
        }
    }

    return true;
}

static bool _valid_monster_generation_location(mgen_data &mg)
{
    return _valid_monster_generation_location(mg, mg.pos);
}

static void _inherit_kmap(monster &mon, const actor *summoner)
{
    if (!summoner)
        return;
    const monster* monsum = summoner->as_monster();
    if (!monsum || !monsum->has_originating_map())
        return;
    mon.props[MAP_KEY] = monsum->originating_map();
}

monster* place_monster(mgen_data mg, bool force_pos, bool dont_place)
{
    rng::subgenerator monster_rng;

#ifdef DEBUG_MON_CREATION
    mprf(MSGCH_DIAGNOSTICS, "in place_monster()");
#endif

    const int mon_count = count_if(begin(env.mons), end(env.mons),
                                   [] (const monster &mons) -> bool
                                   { return mons.type != MONS_NO_MONSTER; });
    // All monsters have been assigned? {dlb}
    if (mon_count >= MAX_MONSTERS - 1)
        return nullptr;

    int tries = 0;

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
                                  &place, &want_band, allow_ood);
    // Place may have been updated inside resolve_monster_type
    // and then inside pick_random_monster for OOD.
    bool chose_ood_monster = place.absdepth() > mg.place.absdepth() + 5;
    if (want_band)
        mg.flags |= MG_PERMIT_BANDS;

    if (mg.cls == MONS_NO_MONSTER || mg.cls == MONS_PROGRAM_BUG)
        return 0;

    bool create_band = mg.permit_bands();
    // If we drew an OOD monster and the level has less absdepth than D:13
    // disable band generation. This applies only to randomly picked monsters
    // -- chose_ood_monster will never be set true for explicitly specified
    // monsters in vaults and other places.
    if (chose_ood_monster && env.absdepth0 < 12)
    {
        dprf(DIAG_MONPLACE,
             "Chose monster with OOD roll: %s, disabling band generation",
             get_monster_data(mg.cls)->name);
        create_band = false;
    }

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

    // For first monster, choose location. This is pretty intensive.
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
                return nullptr;

            mg.pos = random_in_bounds();

            if (!_valid_monster_generation_location(mg))
                continue;

            // Is the grid verboten?
            if (map_masked(mg.pos, mg.map_mask))
                continue;

            break;
        }
    }
    // Sanity check that the specified position is valid.
    else if (!_valid_monster_generation_location(mg) && !dont_place)
        return nullptr;

    monster* mon;
    if (mg.cls == MONS_PLAYER_GHOST)
        mon = _place_pghost_aux(mg, nullptr, place, force_pos, dont_place);
    else
        mon = _place_monster_aux(mg, nullptr, place, force_pos, dont_place);

    if (!mon)
        return nullptr;

    if (mg.props.exists(MAP_KEY))
        mon->set_originating_map(mg.props[MAP_KEY].get_string());

    if (chose_ood_monster)
        mon->props[MON_OOD_KEY].get_bool() = true;

    if (mg.needs_patrol_point())
    {
        mon->patrol_point = mon->pos();
#ifdef DEBUG_PATHFIND
        mprf("Monster %s is patrolling around (%d, %d).",
             mon->name(DESC_PLAIN).c_str(), mon->pos().x, mon->pos().y);
#endif
    }

    if (player_in_branch(BRANCH_ABYSS)
        && !mg.summoner
        && !(mg.extra_flags & MF_WAS_IN_VIEW))
    {
        if (in_bounds(mon->pos()) && !cell_is_solid(mon->pos()))
            big_cloud(CLOUD_TLOC_ENERGY, mon, mon->pos(), 3 + random2(3), 3, 3);

        if (you.can_see(*mon)
             && !crawl_state.generating_level
             && !crawl_state.is_god_acting())
        {
            mon->seen_context = SC_ABYSS;
        }
    }

    // Now, forget about banding if the first placement failed, or there are
    // too many monsters already.
    if (mon->mindex() >= MAX_MONSTERS - 30)
        return mon;

    if (band_size > 1)
        mon->flags |= MF_BAND_LEADER;

    const bool priest = mon->is_priest();

    mgen_data band_template = mg;

    // Create apostle bands via custom method, before they would be used
    if (mon->type == MONS_ORC_APOSTLE && create_band)
    {
        band_size = _fill_apostle_band(*mon, band_monsters);
        leader = true;
    }

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
            member->flags |= MF_BAND_FOLLOWER;
            member->set_band_leader(*mon);
            member->set_originating_map(mon->originating_map());

            // Priestly band leaders should have an entourage of the
            // same religion, unless members of that entourage already
            // have a different one.
            if (priest && member->god == GOD_NO_GOD)
                member->god = mon->god;

            if (mon->type == MONS_PIKEL)
            {
                // Don't give XP for the band to discourage hunting. Pikel
                // has an artificially large XP modifier to compensate for
                // this.
                member->flags |= MF_NO_REWARD;
                member->props[PIKEL_BAND_KEY] = true;
            }
            else if (mon->type == MONS_KIRKE)
                member->props[KIRKE_BAND_KEY] = true;
            else if (mon->type == MONS_ORC_APOSTLE)
            {
                member->flags |= (MF_HARD_RESET | MF_APOSTLE_BAND | MF_NO_REWARD);
                member->mark_summoned();
            }
        }
    }
    dprf(DIAG_DNGN, "Placing %s at %d,%d", mon->name(DESC_PLAIN, true).c_str(),
                mon->pos().x, mon->pos().y);

    // Placement of first monster, at least, was a success.
    return mon;
}

monster* get_free_monster()
{
    for (auto &mons : menv_real)
        if (mons.type == MONS_NO_MONSTER)
        {
            if (mons.mindex() > env.max_mon_index)
                env.max_mon_index = mons.mindex();

            mons.reset();
            return &mons;
        }

    return nullptr;
}

// TODO: make mon a reference
void mons_add_blame(monster* mon, const string &blame_string, bool at_front)
{
    const bool exists = mon->props.exists(BLAME_KEY);
    CrawlStoreValue& blame = mon->props[BLAME_KEY];
    if (!exists)
        blame.new_vector(SV_STR, SFLAG_CONST_TYPE);
    if (at_front)
        blame.get_vector().insert(0, blame_string);
    else
        blame.get_vector().push_back(blame_string);
}

static void _place_twister_clouds(monster *mon)
{
    // Yay for the abj_degree having a huge granularity.
    if (mon->has_ench(ENCH_SUMMON_TIMER))
    {
        mon_enchant abj = mon->get_ench(ENCH_SUMMON_TIMER);
        mon->lose_ench_duration(abj, abj.duration / 2);
    }

    polar_vortex_damage(mon, -10);
}

static void _place_monster_maybe_override_god(monster *mon, monster_type cls,
                                              level_id place)
{
    // [ds] Vault defs can request priest monsters of unusual types.
    if (mon->is_priest() && mon->god == GOD_NO_GOD)
    {
        mon->god = GOD_NAMELESS;
        return;
    }

    // 1 out of 7 non-priestly orcs who are unbelievers stay that way,
    // and the others follow Beogh's word.
    if (mons_genus(cls) == MONS_ORC && mon->god == GOD_NO_GOD)
    {
        if (!one_chance_in(7))
            mon->god = GOD_BEOGH;
    }
    // 1 out of 7 angels or daevas who normally worship TSO are adopted
    // by Xom if they're in the Abyss.
    else if ((cls == MONS_ANGEL || cls == MONS_DAEVA)
              && mon->god == GOD_SHINING_ONE)
    {
        if (one_chance_in(7) && place == BRANCH_ABYSS)
            mon->god = GOD_XOM;
    }
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
    else if (!leader
        && in_bounds(mg.pos)
        && (mg.behaviour == BEH_FRIENDLY ||
            (!is_sanctuary(mg.pos) || mons_is_tentacle_segment(montype)))
        && !monster_at(mg.pos)
        && (you.pos() != mg.pos || fedhas_passthrough_class(mg.cls))
        && (force_pos || monster_habitable_grid(montype, mg.pos)))
    {
        fpos = mg.pos;
    }
    else
    {
        int i;
        // We'll try 1000 times for a good spot.
        for (i = 0; i < 1000; ++i)
        {
            fpos = mg.pos;
            fpos.x += random_range(-3, 3);
            fpos.y += random_range(-3, 3);

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
    mon->xp_tracking  = mg.xp_tracking;

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

    // Pass along type props for apostles
    if (mon->type == MONS_ORC_APOSTLE)
    {
        if (mg.props.exists(APOSTLE_TYPE_KEY))
            mon->props[APOSTLE_TYPE_KEY] = mg.props[APOSTLE_TYPE_KEY].get_int();

        if (mg.props.exists(APOSTLE_POWER_KEY))
            mon->props[APOSTLE_POWER_KEY] = mg.props[APOSTLE_POWER_KEY].get_int();

        if (mg.props.exists(APOSTLE_BAND_POWER_KEY))
            mon->props[APOSTLE_BAND_POWER_KEY] = mg.props[APOSTLE_BAND_POWER_KEY].get_int();
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
        define_monster(*mon, mg.behaviour == BEH_FRIENDLY
                || mg.behaviour == BEH_GOOD_NEUTRAL);

    if (mons_genus(mg.cls) == MONS_HYDRA)
    {
        // We're about to check m_ent->attack[1], so we may as well add a
        // compile-time check to ensure that the array is at least 2 elements
        // large, else we risk undefined behaviour (The array's size is known at
        // compile time even though its value is not). This CHECK would only
        // ever fail if we made it impossible for monsters to have two melee
        // attacks, in which case the ASSERT becomes silly.
        COMPILE_CHECK(ARRAYSZ(m_ent->attack) > 1);

        // Usually hydrae have exactly one attack (which is implicitly repeated
        // for each head), but a "hydra" may have zero if it is actually a
        // hydra-shaped block of ice. We verify here that nothing "hydra-shaped"
        // has more than one attack, because any that do will need cleaning up
        // to fit into the attack-per-head policy.

        ASSERT(m_ent->attack[1].type == AT_NONE);
    }

    if (mon->type == MONS_MUTANT_BEAST)
    {
        vector<int> gen_facets;
        if (mg.props.exists(MUTANT_BEAST_FACETS))
            for (auto facet : mg.props[MUTANT_BEAST_FACETS].get_vector())
                gen_facets.push_back(facet.get_int());

        init_mutant_beast(*mon, mg.hd, gen_facets);
    }

    // Is it a god gift?
    if (mg.god != GOD_NO_GOD)
        mons_make_god_gift(*mon, mg.god);
    // Not a god gift. Give the monster a god.
    else
    {
        mon->god = m_ent->god;
        _place_monster_maybe_override_god(mon, mg.cls, mg.place);
    }

    // Monsters that need halos/silence auras/umbras.
    if ((mon->holiness() & MH_HOLY)
         || mg.cls == MONS_SILENT_SPECTRE
         || mg.cls == MONS_PROFANE_SERVITOR
         || mons_is_ghost_demon(mg.cls))
    {
        invalidate_agrid(true);
    }

    // If the caller requested a specific colour for this monster, apply
    // it now.
    if ((mg.colour == COLOUR_INHERIT
         && mons_class_colour(mon->type) != COLOUR_UNDEF)
        || mg.colour > COLOUR_UNDEF)
    {
        mon->colour = mg.colour;
    }

    if (!mg.mname.empty())
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

    if (mg.cls == MONS_ABOMINATION_SMALL || mg.cls == MONS_ABOMINATION_LARGE)
    {
        enchant_type buff = random_choose(ENCH_MIGHT, ENCH_HASTE, ENCH_REGENERATION);
        mon->add_ench(mon_enchant(buff, 0, 0, INFINITE_DURATION));
    }

    if (mg.cls == MONS_TWISTER || mg.cls == MONS_DIAMOND_OBELISK)
    {
        mon->props[POLAR_VORTEX_KEY].get_int() = you.elapsed_time;
        mon->add_ench(mon_enchant(ENCH_POLAR_VORTEX, 0, 0, INFINITE_DURATION));
    }

    // this MUST follow hd initialization!
    if (mons_is_hepliaklqana_ancestor(mon->type))
    {
        set_ancestor_spells(*mon);
        if (mg.props.exists(MON_GENDER_KEY)) // move this out?
            mon->props[MON_GENDER_KEY] = mg.props[MON_GENDER_KEY].get_int();
        mon->props[DBNAME_KEY] = mons_class_name(mon->type);
    }

    if (mon->type == MONS_HELLBINDER || mon->type == MONS_CLOUD_MAGE
        || mon->type == MONS_HEADMASTER)
    {
        mon->props[MON_GENDER_KEY] = random_choose(GENDER_FEMALE, GENDER_MALE,
                                                   GENDER_NEUTRAL);
    }

    if (mg.props.exists(CUSTOM_SPELL_LIST_KEY))
    {
        for (int spell : mg.props[CUSTOM_SPELL_LIST_KEY].get_vector())
        {
            mon_spell_slot slot((spell_type)spell, 200, MON_SPELL_MAGICAL);
            mon->spells.push_back(slot);
        }
        mon->props[CUSTOM_SPELLS_KEY] = true;
    }


    if (mon->has_spell(SPELL_REPEL_MISSILES))
        mon->add_ench(mon_enchant(ENCH_REPEL_MISSILES, 1, mon, INFINITE_DURATION));

    if (mons_class_flag(mon->type, M_FIRE_RING))
        mon->add_ench(ENCH_RING_OF_FLAMES);

    if (mons_class_flag(mon->type, M_THUNDER_RING))
        mon->add_ench(ENCH_RING_OF_THUNDER);

    if (mons_class_flag(mon->type, M_MIASMA_RING))
        mon->add_ench(ENCH_RING_OF_MIASMA);

    mon->flags |= MF_JUST_SUMMONED;

    if (mons_class_is_animated_weapon(mg.cls))
    {
        if (mg.props.exists(TUKIMA_WEAPON))
        {
            give_specific_item(mon, mg.props[TUKIMA_WEAPON].get_item());
            mon->props[TUKIMA_WEAPON] = true;
        }
        else
            give_item(mon, place.absdepth(), mg.is_summoned());


        // Dancing weapons *always* have a weapon. Fail to create them
        // otherwise.
        item_def* wpn = mon->mslot_item(MSLOT_WEAPON);
        if (!wpn)
        {
            // If they got created with an alt weapon, swap it in.
            item_def* alt_wpn = mon->mslot_item(MSLOT_ALT_WEAPON);
            if (alt_wpn != nullptr)
            {
                swap(mon->inv[MSLOT_WEAPON], mon->inv[MSLOT_ALT_WEAPON]);
                wpn = alt_wpn;
            }
            else
            {
                mon->destroy_inventory();
                env.mid_cache.erase(mon->mid);
                mon->reset();
                env.mgrid(fpos) = NON_MONSTER;
                return 0;
            }
        }

        mon->colour = wpn->get_colour();
    }
    else if (mons_class_itemuse(mg.cls) >= MONUSE_STARTING_EQUIPMENT
             && !mg.props.exists(KIKU_WRETCH_KEY))
    {
        give_item(mon, place.absdepth(), mg.is_summoned());
        // Give these monsters a second weapon. - bwr
        if (mons_class_wields_two_weapons(mg.cls))
            give_weapon(mon, place.absdepth());

        unwind_var<int> save_speedinc(mon->speed_increment);
        mon->wield_melee_weapon(false);
    }

    if (mon->type == MONS_SLIME_CREATURE && mon->blob_size > 1)
    {
        // Boost HP to what it would have been if it had grown this
        // big by merging.
        mon->hit_points     *= mon->blob_size;
        mon->max_hit_points *= mon->blob_size;
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
        {
            mon->attitude = ATT_FRIENDLY;
            mon->flags   |= MF_NO_REWARD;
        }

        if (mg.behaviour == BEH_GOOD_NEUTRAL)
        {
            mon->attitude = ATT_GOOD_NEUTRAL;
            mon->flags   |= MF_WAS_NEUTRAL;
        }

        if (mg.behaviour == BEH_NEUTRAL)
        {
            mon->attitude = ATT_NEUTRAL;
            mon->flags   |= MF_WAS_NEUTRAL;
        }

        mon->behaviour = BEH_WANDER;
    }

    // Set pos and link monster into monster grid.
    // This must be done after setting the monster's attitude as `move_to_pos`
    // might check it.
    if (!dont_place && !mon->move_to_pos(fpos))
    {
        env.mid_cache.erase(mon->mid);
        mon->reset();
        return 0;
    }

    // Don't leave shifters in their starting shape.
    // (This must be done after they are moved into a real position, or
    // monster_polymorph will veto any possible thing they could turn into.)
    if (mg.cls == MONS_SHAPESHIFTER || mg.cls == MONS_GLOWING_SHAPESHIFTER)
    {
        msg::suppress nm;
        monster_polymorph(mon, RANDOM_MONSTER);

        // It's not actually a known shapeshifter if it happened to be
        // placed in LOS of the player.
        mon->flags &= ~MF_KNOWN_SHIFTER;
    }

    if (mg.is_summoned())
    {
        // Dancing weapons created by Tukima's Dance shouldn't mark their
        // inventory as summoned so that they can drop themselves to the floor
        // upon death.
        mon->mark_summoned(mg.summon_type, mg.summon_duration,
                           mg.summon_type != SPELL_TUKIMAS_DANCE);
        _inherit_kmap(*mon, mg.summoner);

        if (mg.summon_type > 0 && mg.summoner)
        {
            // If this is a band member created by shadow creatures, link its
            // ID and don't count it against the summon cap
            if (mg.summon_type == SPELL_SHADOW_CREATURES && leader)
                mon->props[SUMMON_ID_KEY].get_int() = leader->mid;
            else
            {
                summoned_monster(mon, mg.summoner,
                                static_cast<spell_type>(mg.summon_type));
            }
        }
    }

    // Perm summons shouldn't leave gear either.
    if (mg.extra_flags & MF_HARD_RESET && mg.extra_flags & MF_NO_REWARD)
        mon->mark_summoned();

    ASSERT(!invalid_monster_index(mg.foe)
           || mg.foe == MHITYOU || mg.foe == MHITNOT);
    mon->foe = mg.foe;

    string blame_prefix;

    if (mg.flags & MG_BAND_MINION)
        blame_prefix = "led by ";
    else if (mg.summon_duration > 0)
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
            if (sum->props.exists(BLAME_KEY))
            {
                const CrawlVector& oldblame = sum->props[BLAME_KEY].get_vector();
                for (const auto &bl : oldblame)
                    mons_add_blame(mon, bl.get_string());
            }
        }
    }

    // Initialise (very) ugly things and dancing weapons
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
            ghost.init_spectral_weapon(*(mon->mslot_item(MSLOT_WEAPON)));
        mon->set_ghost(ghost);
        mon->ghost_demon_init();
    }
    else if (mon->type == MONS_ORC_APOSTLE)
        mon->flags |= (MF_APOSTLE_BAND | MF_HARD_RESET);

    tile_init_props(mon);

    init_poly_set(mon);

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
            mons_set_just_seen(mon);
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

    if (mg.summoner && mg.summoner->is_player()
        && you.unrand_equipped(UNRAND_JUSTICARS_REGALIA))
    {
        mon->add_ench(mon_enchant(ENCH_REGENERATION, 0, &you, random_range(300, 500)));
    }

    return mon;
}

static monster* _place_pghost_aux(const mgen_data &mg, const monster *leader,
                                   level_id place,
                                   bool force_pos, bool dont_place)
{
    // we need to isolate the generation of a pghost from the caller's RNG,
    // since depending on the ghost, the aux call can trigger variation in
    // things like whether an enchantment (with a random duration) is
    // triggered.
    rng::generator rng(rng::SYSTEM_SPECIFIC);
    return _place_monster_aux(mg, leader, place, force_pos, dont_place);
}

// Check base monster class against zombie type and position if set.
static bool _good_zombie(monster_type base, monster_type cs,
                         const coord_def& pos)
{
    base = fixup_zombie_type(cs, base);

    // Actually pick a monster that is happy where we want to put it.
    // Fish zombies on land are helpless and uncool.
    if (in_bounds(pos) && !monster_habitable_grid(base, pos))
        return false;

    if (cs == MONS_NO_MONSTER)
        return true;

    // If skeleton, monster must have a skeleton.
    if (cs == MONS_SKELETON && !mons_skeleton(base))
        return false;

    // If zombie, monster must have a corpse.
    if (cs == MONS_ZOMBIE && !mons_class_can_be_zombified(base))
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
    if (!mons_class_can_be_zombified(corpse_type))
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

static bool _mc_bad_wretch(monster_type mon)
{
    // goofy on-death effect - probably other things could go here too
    return mon == MONS_SPRIGGAN_DRUID;
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
                                            bool for_wretch)
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

    place.depth = min(place.depth, branch_zombie_cap(place.branch));
    place.depth = max(1, place.depth);

    mon_pick_vetoer veto = for_wretch ? _mc_bad_wretch :
                           really_in_d ? _mc_too_slow_for_zombies
                                        : nullptr;

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
    if (mons_is_draconian_job(ztype) || mons_is_ghost_demon(ztype))
    {
        mon->props[ZOMBIE_BASE_AC_KEY] = mon->base_armour_class();
        mon->props[ZOMBIE_BASE_EV_KEY] = mon->base_evasion();
    }

    mon->type         = cs;
    mon->base_monster = ztype;

    mon->colour       = COLOUR_INHERIT;
    mon->speed        = ((cs == MONS_SPECTRAL_THING || cs == MONS_BOUND_SOUL)
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

/// Under what conditions should a band spawn with a monster?
struct band_conditions
{
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
struct band_info
{
    /// The type of the band; used to determine the type of followers.
    band_type type;
    /// The min & max # of followers; doesn't count the leader.
    random_var range;
    /// Should the leader of this band be credited for kills?
    bool natural_leader;
};

/// One or more band_infos, with conditions.
struct band_set
{
    /// When should the band actually be generated?
    band_conditions conditions;
    /// The bands to be selected between, with equal weight.
    vector<band_info> bands;
};

// We handle Vaults centaur warriors specially.
static const band_conditions centaur_band_condition
    = { 3, 10, []() { return !player_in_branch(BRANCH_SHOALS)
                          && !player_in_branch(BRANCH_VAULTS); }};

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
                                          {{BAND_DRACONIAN, {3, 7}, true }}};

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
    { MONS_ORC_WARLORD,     { {0, 0, [](){ return !player_in_branch(BRANCH_VAULTS); }},
                                         {{ BAND_ORC_KNIGHT, {8, 16}, true }}}},
    { MONS_SAINT_ROKA,     { {0, 0, [](){ return !player_in_branch(BRANCH_VAULTS) ||
                                                 !player_in_branch(BRANCH_DEPTHS) ||
                                                 !player_in_branch(BRANCH_CRYPT); }},
                                         {{ BAND_ORC_KNIGHT, {8, 16}, true }}}},
    { MONS_ORC_KNIGHT,      { {}, {{ BAND_ORC_KNIGHT, {3, 7}, true }}}},
    { MONS_ORC_HIGH_PRIEST, { {}, {{ BAND_ORC_KNIGHT, {4, 8}, true }}}},
    { MONS_KOBOLD_BRIGAND,  { {0, 4}, {{ BAND_KOBOLDS, {2, 8} }}}},
    { MONS_KILLER_BEE,      { {}, {{ BAND_KILLER_BEES, {2, 6} }}}},
    { MONS_CAUSTIC_SHRIKE,  { {}, {{ BAND_CAUSTIC_SHRIKE, {2, 5} }}}},
    { MONS_SHARD_SHRIKE,    { {}, {{ BAND_SHARD_SHRIKE, {1, 4} }}}},
    { MONS_SLIME_CREATURE,  { {}, {{ BAND_SLIME_CREATURES, {2, 6} }}}},
    { MONS_YAK,             { {}, {{ BAND_YAKS, {2, 6} }}}},
    { MONS_VERY_UGLY_THING, { {0, 19}, {{ BAND_VERY_UGLY_THINGS, {2, 6} }}}},
    { MONS_UGLY_THING,      { {0, 13}, {{ BAND_UGLY_THINGS, {2, 6} }}}},
    { MONS_HELL_HOUND,      { {}, {{ BAND_HELL_HOUNDS, {2, 5} }}}},
    { MONS_JACKAL,          { {}, {{ BAND_JACKALS, {1, 4} }}}},
    { MONS_HELL_KNIGHT,     { {}, {{ BAND_HELL_KNIGHTS, {4, 8} }}}},
    { MONS_MARGERY,         { {}, {{ BAND_MARGERY, {5, 7}, true }}}},
    { MONS_AMAEMON,         { {}, {{ BAND_ORANGE_DEMONS, {1, 2}, true }}}},
    { MONS_JOSEPHINE,       { {}, {{ BAND_JOSEPHINE, {3, 6}, true }}}},
    { MONS_NECROMANCER,     { {}, {{ BAND_NECROMANCER, {3, 6}, true }}}},
    { MONS_VAMPIRE_MAGE,    { {3}, {{ BAND_JIANGSHI, {2, 4}, true }}}},
    { MONS_JIANGSHI,        { {}, {{ BAND_JIANGSHI, {0, 2} }}}},
    { MONS_GNOLL,           { {0, 1}, {{ BAND_GNOLLS, {2, 4} }}}},
    { MONS_GNOLL_BOUDA,     { {}, {{ BAND_GNOLLS, {3, 6}, true }}}},
    { MONS_GNOLL_SERGEANT,  { {}, {{ BAND_GNOLLS, {3, 6}, true }}}},
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
    { MONS_OGRE_MAGE,       { {}, {{ BAND_OGRE_MAGE, {4, 8} }}}},
    { MONS_LODUL,           { {}, {{ BAND_OGRES, {6, 10}, true }}}},
    { MONS_BALRUG,          { {0, 0, []() { return !player_in_hell(); }},
                                   {{ BAND_BALRUG, {2, 5}, true }}}},
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
    { MONS_NAGA_RITUALIST,  { {}, {{ BAND_NAGA_RITUALIST, {3, 6}, true }}}},
    { MONS_RIVER_RAT,       { {}, {{ BAND_GREEN_RATS, {4, 10} }}}},
    { MONS_HELL_RAT,        { {}, {{ BAND_HELL_RATS, {3, 7} }}}},
    { MONS_DREAM_SHEEP,     { {}, {{ BAND_DREAM_SHEEP, {3, 7} }}}},
    { MONS_GHOUL,           { {}, {{ BAND_GHOULS, {1, 4} }}}},
    { MONS_KIRKE,           { {}, {{ BAND_HOGS, {3, 8}, true }}}},
    { MONS_HOG,             { {}, {{ BAND_HOGS, {1, 4} }}}},
    { MONS_VAMPIRE_MOSQUITO, { {}, {{ BAND_VAMPIRE_MOSQUITOES, {1, 4} }}}},
    { MONS_FIRE_BAT,        { {}, {{ BAND_FIRE_BATS, {1, 4} }}}},
    { MONS_DEEP_TROLL_EARTH_MAGE, { {}, {{ BAND_DEEP_TROLL_SHAMAN, {3, 6} }}}},
    { MONS_DEEP_TROLL_SHAMAN, { {}, {{ BAND_DEEP_TROLL_SHAMAN, {3, 6} }}}},
    { MONS_HELL_HOG,        { {}, {{ BAND_HELL_HOGS, {2, 4} }}}},
    { MONS_BOGGART,         { {}, {{ BAND_BOGGARTS, {2, 5} }}}},
    { MONS_PRINCE_RIBBIT,   { {}, {{ BAND_BLINK_FROGS, {2, 5}, true }}}},
    { MONS_BLINK_FROG,      { {}, {{ BAND_BLINK_FROGS, {2, 5} }}}},
    { MONS_WIGHT,           { {}, {{ BAND_WIGHTS, {2, 5} }}}},
    { MONS_ANCIENT_CHAMPION, { {2, 0, []() {
        return !player_in_hell(); }},
                                  {{ BAND_SKELETAL_WARRIORS, {2, 5}, true}}}},
    { MONS_SKELETAL_WARRIOR, { {}, {{ BAND_SKELETAL_WARRIORS, {2, 5} }}}},
    { MONS_CYCLOPS,         { { 0, 0, []() {
        return player_in_branch(BRANCH_SHOALS); }},
                                  {{ BAND_DREAM_SHEEP, {2, 5}, true }}}},
    { MONS_ALLIGATOR,       { { 5, 0, []() {
        return !player_in_branch(BRANCH_LAIR); }},
                                  {{ BAND_ALLIGATOR, {1, 2} }}}},
    { MONS_FORMLESS_JELLYFISH, { { 0, 0, []() {
        return player_in_branch(BRANCH_SLIME); }},
                                  {{ BAND_JELLYFISH, {1, 3} }}}},
    { MONS_POLYPHEMUS,      { {}, {{ BAND_POLYPHEMUS, {3, 6}, true }}}},
    { MONS_HARPY,           { {}, {{ BAND_HARPIES, {2, 5} }}}},
    { MONS_SALTLING,        { {}, {{ BAND_SALTLINGS, {2, 4} }}}},
    { MONS_PEACEKEEPER,     { { 0, 0, []() {
        return player_in_branch(BRANCH_VAULTS); }},
                                  {{ BAND_GOLEMS, {1, 3}, true }}}},
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
    { MONS_ILSUIW,          { {}, {{ BAND_ILSUIW, {3, 6}, true }}}},
    { MONS_AZRAEL,          { {}, {{ BAND_AZRAEL, {4, 9}, true }}}},
    { MONS_DUVESSA,         { {}, {{ BAND_DUVESSA, {1, 2} }}}},
    { MONS_KHUFU,           { {}, {{ BAND_KHUFU, {3, 4}, true }}}},
    { MONS_GOLDEN_EYE,      { {}, {{ BAND_GOLDEN_EYE, {1, 6} }}}},
    { MONS_PIKEL,           { {}, {{ BAND_PIKEL, {4, 5}, true }}}},
    { MONS_MERFOLK_AQUAMANCER, { {0, 0, [](){ return !player_in_branch(BRANCH_PANDEMONIUM); }},
                                   {{ BAND_MERFOLK_AQUAMANCER, {3, 5}, true }}}},
    { MONS_MERFOLK_JAVELINEER, { mf_band_condition,
                                  {{ BAND_MERFOLK_JAVELINEER, {2, 5}, true }}}},
    { MONS_MERFOLK_IMPALER, { mf_band_condition,
                                  {{ BAND_MERFOLK_IMPALER, {2, 5}, true }}}},
    { MONS_ELEPHANT,        { {}, {{ BAND_ELEPHANT, {2, 6} }}}},
    { MONS_REDBACK,         { {}, {{ BAND_REDBACK, {1, 5} }}}},
    { MONS_CULICIVORA,      { {}, {{ BAND_MIXED_SPIDERS, {1, 4} }}}},
    { MONS_ENTROPY_WEAVER,  { {0, 0, [](){ return !player_in_branch(BRANCH_PANDEMONIUM); }},
                                         {{ BAND_REDBACK, {1, 4} }}}},
    { MONS_PHARAOH_ANT,     { {}, {{ BAND_MIXED_SPIDERS, {1, 3} }}}},
    { MONS_JOROGUMO,        { {0, 0, [](){ return !player_in_branch(BRANCH_PANDEMONIUM); }},
                                         {{ BAND_MIXED_SPIDERS, {1, 3}, true }}}},
    { MONS_BROODMOTHER,     { {}, {{ BAND_MIXED_SPIDERS, {2, 4}, true }}}},
    { MONS_SUN_MOTH,        { {0, 0, [](){ return !player_in_branch(BRANCH_PANDEMONIUM); }},
                                         {{ BAND_MIXED_SPIDERS, {1, 3}, true }}}},
    { MONS_RADROACH,        { {}, {{ BAND_MIXED_SPIDERS, {1, 3} }}}},
    { MONS_JUMPING_SPIDER,  { {2}, {{ BAND_JUMPING_SPIDER, {1, 6} }}}},
    { MONS_TARANTELLA,      { {2}, {{ BAND_TARANTELLA, {1, 5} }}}},
    { MONS_VAULT_WARDEN,    { {}, {{ BAND_YAKTAURS, {2, 6}, true },
                                   { BAND_VAULT_WARDEN, {2, 5}, true }}}},
    { MONS_IRONBOUND_PRESERVER, { {}, {{ BAND_PRESERVER, {3, 6}, true }}}},
    { MONS_TENGU_CONJURER,  { {2}, {{ BAND_TENGU, {1, 2} }}}},
    { MONS_TENGU_WARRIOR,   { {2}, {{ BAND_TENGU, {1, 2} }}}},
    { MONS_SOJOBO,          { {}, {{ BAND_SOJOBO, {2, 3}, true }}}},
    { MONS_SPRIGGAN_RIDER,  { {3}, {{ BAND_SPRIGGAN_RIDERS, {1, 3} }}}},
    { MONS_SPRIGGAN_BERSERKER, { {2}, {{ BAND_SPRIGGANS, {2, 4} }}}},
    { MONS_SPRIGGAN_DEFENDER, { {}, {{ BAND_SPRIGGAN_ELITES, {2, 5} }}}},
    { MONS_ENCHANTRESS, { {}, {{ BAND_ENCHANTRESS, {6, 11}, true }}}},
    { MONS_SHAMBLING_MANGROVE, { {4}, {{ BAND_SPRIGGAN_RIDERS, {1, 2} }}}},
    { MONS_VAMPIRE_KNIGHT,  { {4}, {{ BAND_PHANTASMAL_WARRIORS, {2, 3}, true }}}},
    { MONS_RAIJU,           { {}, {{ BAND_RAIJU, {2, 4} }}}},
    { MONS_SALAMANDER_MYSTIC, { {}, {{ BAND_SALAMANDERS, {2, 4} }}}},
    { MONS_SALAMANDER_TYRANT, { {0, 0, [](){ return !player_in_branch(BRANCH_GEHENNA); }}, {{ BAND_SALAMANDER_ELITES, {2, 5} }}}},
    { MONS_DEMONSPAWN_BLOOD_SAINT, { {}, {{ BAND_BLOOD_SAINT, {1, 3} }}}},
    { MONS_DEMONSPAWN_WARMONGER, { {}, {{ BAND_WARMONGER, {1, 3} }}}},
    { MONS_DEMONSPAWN_CORRUPTER, { {}, {{ BAND_CORRUPTER, {1, 3} }}}},
    { MONS_DEMONSPAWN_SOUL_SCHOLAR, { {}, {{ BAND_SOUL_SCHOLAR, {1, 3} }}}},
    { MONS_VASHNIA,         { {}, {{ BAND_VASHNIA, {3, 6}, true }}}},
    { MONS_ROBIN,           { {}, {{ BAND_ROBIN, {10, 13}, true }}}},
    { MONS_RAKSHASA,        { {2, 0, []() {
        return branch_has_monsters(you.where_are_you)
            || !vault_mon_types.empty();
    }},                           {{ BAND_RANDOM_SINGLE, {1, 2} }}}},
    { MONS_POLTERGUARDIAN,  { {2, 0, []() {
        return branch_has_monsters(you.where_are_you)
            || !vault_mon_types.empty();
    }},                           {{ BAND_RANDOM_SINGLE, {1, 3} }}}},
    { MONS_CEREBOV,         { {}, {{ BAND_CEREBOV, {5, 8}, true }}}},
    { MONS_GLOORX_VLOQ,     { {}, {{ BAND_GLOORX_VLOQ, {5, 8}, true }}}},
    { MONS_MNOLEG,          { {}, {{ BAND_MNOLEG, {5, 8}, true }}}},
    { MONS_LOM_LOBON,       { {}, {{ BAND_LOM_LOBON, {5, 8}, true }}}},
    { MONS_DEATH_SCARAB,  { {0, 0, []() {
        return you.where_are_you == BRANCH_TOMB;
    }},                            {{ BAND_DEATH_SCARABS, {3, 6} }}}},
    { MONS_SERAPH,          { {}, {{ BAND_HOLIES, {1, 4}, true }}}},
    { MONS_IRON_GIANT,      { {}, {{ BAND_IRON_GOLEMS, {2, 3}, true }}}},
    { MONS_SPARK_WASP,      { {0, 0, []() {
        return you.where_are_you == BRANCH_DEPTHS;
    }},                           {{ BAND_SPARK_WASPS, {1, 4} }}}},
    { MONS_HOWLER_MONKEY,   { {2, 6}, {{ BAND_HOWLER_MONKEY, {1, 3} }}}},
    { MONS_GLASS_EYE,   { {0, 0, []() {
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
    { MONS_LINDWURM,         { {0, 0, []() {
        return you.where_are_you == BRANCH_VAULTS;
    }},                            {{ BAND_LINDWURMS, {1, 3} }}}},
    { MONS_DIRE_ELEPHANT,    { {0, 0, []() {
        return you.where_are_you == BRANCH_VAULTS;
    }},                            {{ BAND_DIRE_ELEPHANTS, {2, 4} }}}},
    { MONS_ARCANIST,  { {0, 0, []() {
        return player_in_branch(BRANCH_VAULTS);
    }},                            {{ BAND_UGLY_THINGS, {2, 4}, true }}}},
    { MONS_WENDIGO, { {}, {{ BAND_SIMULACRA, {2, 6} }}}},
    { MONS_JOSEPHINA, { {}, {{ BAND_SIMULACRA, {4, 6}, true }}}},
    { MONS_BONE_DRAGON, { {0, 0, []() { return player_in_hell(); }},
                                   {{ BAND_BONE_DRAGONS, {1, 2}} }}},
    { MONS_EIDOLON, { {0, 0, []() { return player_in_hell(); }},
                                   {{ BAND_SPECTRALS, {2, 6}, true} }}},
    { MONS_GRUNN,            { {}, {{ BAND_DOOM_HOUNDS, {2, 4}, true }}}},
    { MONS_NORRIS,           { {}, {{ BAND_SKYSHARKS, {2, 5}, true }}}},
    { MONS_UFETUBUS,         { {}, {{ BAND_UFETUBI, {1, 2} }}}},
    { MONS_SIN_BEAST,        { {}, {{ BAND_SIN_BEASTS, {1, 2} }}}},
    { MONS_KOBOLD_BLASTMINER, { {}, {{ BAND_BLASTMINER, {0, 2} }}}},
    { MONS_ARACHNE,          { {}, {{ BAND_ORB_SPIDERS, {3, 5}}}}},

    // special-cased band-sizes
    { MONS_SPRIGGAN_DRUID,  { {3}, {{ BAND_SPRIGGAN_DRUID, {0, 1}, true }}}},
    { MONS_THRASHING_HORROR, { {}, {{ BAND_THRASHING_HORRORS, {0, 1} }}}},
    { MONS_BRAIN_WORM, { {}, {{ BAND_BRAIN_WORMS, {0, 1} }}}},
    { MONS_LAUGHING_SKULL, { {}, {{ BAND_LAUGHING_SKULLS, {0, 1} }}}},
    { MONS_WEEPING_SKULL, { {}, {{ BAND_WEEPING_SKULLS, {0, 1} }}}},
    { MONS_SPHINX_MARAUDER, { {}, {{ BAND_HARPIES, {0, 1} }}}},
    { MONS_PROTEAN_PROGENITOR, { {}, {{ BAND_PROTEAN_PROGENITORS, {0, 1} }}}},
    { MONS_THERMIC_DYNAMO, { {}, {{ BAND_THERMIC_DYNAMOS, {0, 1} }}}},
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
            { BRANCH_SPIDER, { { { BAND_MIXED_SPIDERS, 2, 4 },  1 },
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

    // Per-branch hacks. TODO: move this into the main branch structure
    // (probably moving conditionals inside band_info?)
    case MONS_CENTAUR_WARRIOR:
        if (player_in_branch(BRANCH_VAULTS))
        {
            band = BAND_CENTAUR_WARRIORS;
            band_size = random_range(2, 4);
        }
        break;

    case MONS_ORC_WARLORD:
        if (player_in_branch(BRANCH_VAULTS))
        {
            band = BAND_ORC_WARLORD;
            band_size = random_range(2, 4);
        }
        break;

    case MONS_SAINT_ROKA:
        if (player_in_branch(BRANCH_VAULTS) ||
            player_in_branch(BRANCH_DEPTHS) ||
            player_in_branch(BRANCH_CRYPT))
        {
            band = BAND_LATE_ROKA;
            band_size = random_range(5, 7);
        }
        break;

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
            band = random_choose(BAND_FAUNS, BAND_FAUN_PARTY);
            band_size = 2 + random2(2);
        }
        break;

    case MONS_SPRIGGAN_DRUID:
        if (band != BAND_NO_BAND)
            band_size = (one_chance_in(4) ? 3 : 2);
        break;

    case MONS_LINDWURM:
        if (player_in_branch(BRANCH_VAULTS)
            && x_chance_in_y(min(you.depth, 3), 3))
        {
            band = BAND_WURMS_AND_MASTER;
            band_size += 1;
        }
        break;

    case MONS_SLIME_CREATURE:
        if (player_in_branch(BRANCH_VAULTS)
            && x_chance_in_y(min(you.depth - 1, 5), 5))
        {
            band = BAND_SLIMES_AND_MASTER;
            if (x_chance_in_y(you.depth, 4))
                band_size += 1;
        }
        break;

    case MONS_DIRE_ELEPHANT:
        if (player_in_branch(BRANCH_VAULTS)
            && x_chance_in_y(3 + min(you.depth, 7), 10))
        {
            band = BAND_ELEPHANTS_AND_MASTER;
        }
        break;

    case MONS_LAUGHING_SKULL:
        if (player_in_branch(BRANCH_DUNGEON))
            band_size = 1;
        else
            band_size = random_range(2, 5);
        break;

    case MONS_WEEPING_SKULL:
        if (player_in_branch(BRANCH_ABYSS) && you.depth > 1)
            band_size = 1;
        break;

    case MONS_BRAIN_WORM:
        if (player_in_branch(BRANCH_ABYSS))
            band_size = random2(you.depth) / 2;
        break;

    case MONS_THRASHING_HORROR:
        if (player_in_branch(BRANCH_ABYSS))
            band_size = random2(min(brdepth[BRANCH_ABYSS], you.depth));
        break;

    case MONS_SPHINX_MARAUDER:
         if (player_in_branch(BRANCH_VAULTS))
         {
             band = BAND_SPHINXES;
             band_size = x_chance_in_y(min(you.depth + 2, 6), 6) ? 1 : 0;
         }
         else if (!(player_in_branch(BRANCH_DUNGEON)))
         {
             // Gets harpies.
             band_size = x_chance_in_y(2, 5) ? 2 : 0;
         }
         break;

    case MONS_PROTEAN_PROGENITOR:
    case MONS_THERMIC_DYNAMO:
        if (x_chance_in_y(2, 3))
            band_size = 1;
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
typedef vector<pair<monster_type, int>> member_possibilities;

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
static const map<band_type, vector<member_possibilities>> band_membership = {
    { BAND_HOGS,                {{{MONS_HOG, 1}}}},
    { BAND_YAKS,                {{{MONS_YAK, 1}}}},
    { BAND_FAUNS,               {{{MONS_FAUN, 1}}}},
    { BAND_OGRES,               {{{MONS_OGRE, 1}}}},
    { BAND_WOLVES,              {{{MONS_WOLF, 1}}}},
    { BAND_DUVESSA,             {{{MONS_DOWAN, 1}}}},
    { BAND_GNOLLS,              {{{MONS_GNOLL, 1}}}},
    { BAND_HARPIES,             {{{MONS_HARPY, 1}}}},
    { BAND_RAIJU,               {{{MONS_RAIJU, 1}}}},
    { BAND_WIGHTS,              {{{MONS_WIGHT, 1}}}},
    { BAND_JACKALS,             {{{MONS_JACKAL, 1}}}},
    { BAND_KOBOLDS,             {{{MONS_KOBOLD, 1}}}},
    { BAND_PIKEL,               {{{MONS_LEMURE, 1}}}},
    { BAND_JOSEPHINE,           {{{MONS_WRAITH, 1}}}},
    { BAND_MELIAI,              {{{MONS_MELIAI, 1}}}},
    { BAND_BOGGARTS,            {{{MONS_BOGGART, 1}}}},
    { BAND_CENTAURS,            {{{MONS_CENTAUR, 1}}}},
    { BAND_YAKTAURS,            {{{MONS_YAKTAUR, 1}}}},
    { BAND_MERFOLK_IMPALER,     {{{MONS_MERFOLK, 1}}}},
    { BAND_MERFOLK_JAVELINEER,  {{{MONS_MERFOLK, 1}}}},
    { BAND_ELEPHANT,            {{{MONS_ELEPHANT, 1}}}},
    { BAND_SPHINXES,            {{{MONS_GUARDIAN_SPHINX, 1}}}},
    { BAND_FIRE_BATS,           {{{MONS_FIRE_BAT, 1}}}},
    { BAND_HELL_HOGS,           {{{MONS_HELL_HOG, 1}}}},
    { BAND_HELL_RATS,           {{{MONS_HELL_RAT, 1}}}},
    { BAND_JIANGSHI,            {{{MONS_JIANGSHI, 1}}}},
    { BAND_LINDWURMS,           {{{MONS_LINDWURM, 1}}}},
    { BAND_ALLIGATOR,           {{{MONS_ALLIGATOR, 1}}}},
    { BAND_JELLYFISH,           {{{MONS_FORMLESS_JELLYFISH, 1}}}},
    { BAND_DEATH_YAKS,          {{{MONS_DEATH_YAK, 1}}}},
    { BAND_GREEN_RATS,          {{{MONS_RIVER_RAT, 1}}}},
    { BAND_BRAIN_WORMS,         {{{MONS_BRAIN_WORM, 1}}}},
    { BAND_BLINK_FROGS,         {{{MONS_BLINK_FROG, 1}}}},
    { BAND_GOLDEN_EYE,          {{{MONS_GOLDEN_EYE, 1}}}},
    { BAND_HELL_HOUNDS,         {{{MONS_HELL_HOUND, 1}}}},
    { BAND_KILLER_BEES,         {{{MONS_KILLER_BEE, 1}}}},
    { BAND_ORC_WARLORD,         {{{MONS_ORC_KNIGHT, 1}}}},
    { BAND_SALAMANDERS,         {{{MONS_SALAMANDER, 1}}}},
    { BAND_SPARK_WASPS,         {{{MONS_SPARK_WASP, 1}}}},
    { BAND_UGLY_THINGS,         {{{MONS_UGLY_THING, 1}}}},
    { BAND_DREAM_SHEEP,         {{{MONS_DREAM_SHEEP, 1}}}},
    { BAND_DEATH_SCARABS,       {{{MONS_DEATH_SCARAB, 1}}}},
    { BAND_ORANGE_DEMONS,       {{{MONS_ORANGE_DEMON, 1}}}},
    { BAND_SHARD_SHRIKE,        {{{MONS_SHARD_SHRIKE, 1}}}},
    { BAND_SOJOBO,              {{{MONS_TENGU_REAVER, 1}}}},
    { BAND_HOWLER_MONKEY,       {{{MONS_HOWLER_MONKEY, 1}}}},
    { BAND_WEEPING_SKULLS,      {{{MONS_WEEPING_SKULL, 1}}}},
    { BAND_DIRE_ELEPHANTS,      {{{MONS_DIRE_ELEPHANT, 1}}}},
    { BAND_CAUSTIC_SHRIKE,      {{{MONS_CAUSTIC_SHRIKE, 1}}}},
    { BAND_LAUGHING_SKULLS,     {{{MONS_LAUGHING_SKULL, 1}}}},
    { BAND_DANCING_WEAPONS,     {{{MONS_DANCING_WEAPON, 1}}}},
    { BAND_SLIME_CREATURES,     {{{MONS_SLIME_CREATURE, 1}}}},
    { BAND_SPRIGGAN_RIDERS,     {{{MONS_SPRIGGAN_RIDER, 1}}}},
    { BAND_CENTAUR_WARRIORS,    {{{MONS_CENTAUR_WARRIOR, 1}}}},
    { BAND_MOLTEN_GARGOYLES,    {{{MONS_MOLTEN_GARGOYLE, 1}}}},
    { BAND_SKELETAL_WARRIORS,   {{{MONS_SKELETAL_WARRIOR, 1}}}},
    { BAND_THRASHING_HORRORS,   {{{MONS_THRASHING_HORROR, 1}}}},
    { BAND_VAMPIRE_MOSQUITOES,  {{{MONS_VAMPIRE_MOSQUITO, 1}}}},
    { BAND_IRON_GOLEMS,         {{{MONS_IRON_GOLEM, 1}}}},
    { BAND_EXECUTIONER,         {{{MONS_ABOMINATION_LARGE, 1}}}},
    { BAND_VASHNIA,             {{{MONS_NAGA_SHARPSHOOTER, 1}}}},
    { BAND_PHANTASMAL_WARRIORS, {{{MONS_PHANTASMAL_WARRIOR, 1}}}},
    { BAND_PRESERVER,           {{{MONS_DEEP_TROLL, 10},
                                  {MONS_POLTERGUARDIAN, 2}},
                                {{MONS_DEEP_TROLL, 1}}}},
    { BAND_BONE_DRAGONS,        {{{MONS_BONE_DRAGON, 1}}}},
    { BAND_SPECTRALS,           {{{MONS_SPECTRAL_THING, 1}}}},
    { BAND_UFETUBI,             {{{MONS_UFETUBUS, 1}}}},
    { BAND_SIN_BEASTS,          {{{MONS_SIN_BEAST, 1}}}},
    { BAND_BLASTMINER,          {{{MONS_KOBOLD_BLASTMINER, 1}}}},
    { BAND_THERMIC_DYNAMOS,     {{{MONS_THERMIC_DYNAMO, 1}}}},
    { BAND_PROTEAN_PROGENITORS, {{{MONS_PROTEAN_PROGENITOR, 1}}}},
    { BAND_DEEP_ELF_KNIGHT,     {{{MONS_DEEP_ELF_AIR_MAGE, 46},
                                  {MONS_DEEP_ELF_FIRE_MAGE, 46},
                                  {MONS_DEEP_ELF_KNIGHT, 24},
                                  {MONS_DEEP_ELF_ARCHER, 24},
                                  {MONS_DEEP_ELF_DEATH_MAGE, 3},
                                  {MONS_DEEP_ELF_DEMONOLOGIST, 2},
                                  {MONS_DEEP_ELF_ANNIHILATOR, 2},
                                  {MONS_DEEP_ELF_SORCERER, 2}}}},
    { BAND_DEEP_ELF_HIGH_PRIEST, {{{MONS_DEEP_ELF_AIR_MAGE, 3},
                                   {MONS_DEEP_ELF_FIRE_MAGE, 3},
                                   {MONS_DEEP_ELF_KNIGHT, 2},
                                   {MONS_DEEP_ELF_ARCHER, 2},
                                   {MONS_DEEP_ELF_DEMONOLOGIST, 1},
                                   {MONS_DEEP_ELF_ANNIHILATOR, 1},
                                   {MONS_DEEP_ELF_SORCERER, 1},
                                   {MONS_DEEP_ELF_DEATH_MAGE, 1}}}},
    { BAND_BALRUG,              {{{MONS_SUN_DEMON, 1}}}},
    { BAND_HELLWING,            {{{MONS_HELLWING, 1},
                                  {MONS_SMOKE_DEMON, 1}}}},
    { BAND_CACODEMON,           {{{MONS_SIXFIRHY, 1},
                                  {MONS_ORANGE_DEMON, 1}}}},
    { BAND_NECROMANCER,         {{{MONS_ZOMBIE, 2},
                                  {MONS_SKELETON, 2},
                                  {MONS_SIMULACRUM, 1}}}},
    { BAND_HELL_KNIGHTS,        {{{MONS_HELL_KNIGHT, 3},
                                  {MONS_NECROMANCER, 1}}}},

    { BAND_MARGERY,             {{{MONS_HELLEPHANT, 4},
                                  {MONS_SEARING_WRETCH, 3}},

                                {{MONS_DEEP_ELF_DEATH_MAGE, 4},
                                 {MONS_DEEP_ELF_HIGH_PRIEST, 3}},

                                {{MONS_HELL_KNIGHT, 1}},

                                {{MONS_HELL_KNIGHT, 3},
                                 {MONS_NECROMANCER, 1}}}},

    { BAND_POLYPHEMUS,          {{{MONS_CATOBLEPAS, 1}},

                                 {{MONS_DEATH_YAK, 1}}}},
    { BAND_VERY_UGLY_THINGS,    {{{MONS_UGLY_THING, 3},
                                  {MONS_VERY_UGLY_THING, 4}}}},
    { BAND_GOLEMS,              {{{MONS_WAR_GARGOYLE, 1},
                                  {MONS_CRYSTAL_GUARDIAN, 1}}}},
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

    { BAND_LATE_ROKA,           {{{MONS_ORC_PRIEST, 1}},

                                 {{MONS_ORC_KNIGHT, 1}},

                                 {{MONS_ORC_PRIEST, 2},
                                  {MONS_ORC_KNIGHT, 3},
                                  {MONS_ORC_SORCERER, 3},
                                  {MONS_ORC_HIGH_PRIEST, 4}}}},

    { BAND_OGRE_MAGE,           {{{MONS_TWO_HEADED_OGRE, 2},
                                  {MONS_OGRE, 1}}}},
    { BAND_OGRE_MAGE_EXTERN,    {{{MONS_OGRE_MAGE, 1}},
                                 {{MONS_TWO_HEADED_OGRE, 1},
                                  {MONS_OGRE, 2}}}},
    { BAND_KOBOLD_DEMONOLOGIST, {{{MONS_KOBOLD, 4},
                                  {MONS_KOBOLD_BRIGAND, 2},
                                  {MONS_KOBOLD_DEMONOLOGIST, 1}}}},
    // Favour tougher naga suited to melee, compared to normal naga bands
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
                                  {MONS_BOG_BODY, 5}}}},
    { BAND_ILSUIW,              {{{MONS_MERFOLK_SIREN, 6},
                                  {MONS_MERFOLK, 3},
                                  {MONS_MERFOLK_JAVELINEER, 2},
                                  {MONS_MERFOLK_IMPALER, 2}}}},
    { BAND_AZRAEL,              {{{MONS_FIRE_ELEMENTAL, 1},
                                  {MONS_HELL_HOUND, 1}}}},
    { BAND_KHUFU,               {{{MONS_ROYAL_MUMMY, 1},
                                  {MONS_MUMMY, 1}}}},
    { BAND_MERFOLK_AQUAMANCER,  {{{MONS_MERFOLK, 4},
                                  {MONS_WATER_ELEMENTAL, 11}}}},
    { BAND_DEEP_TROLL_SHAMAN,   {{{MONS_DEEP_TROLL, 18},
                                  {MONS_IRON_TROLL, 8},
                                  {MONS_DEEP_TROLL_EARTH_MAGE, 3},
                                  {MONS_DEEP_TROLL_SHAMAN, 3}}}},
    { BAND_REDBACK,             {{{MONS_REDBACK, 9},
                                  {MONS_TARANTELLA, 1},
                                  {MONS_CULICIVORA, 1},
                                  {MONS_JUMPING_SPIDER, 1}}}},
    { BAND_JUMPING_SPIDER,      {{{MONS_JUMPING_SPIDER, 6},
                                  {MONS_REDBACK, 2},
                                  {MONS_CULICIVORA, 1},
                                  {MONS_WOLF_SPIDER, 1},
                                  {MONS_ORB_SPIDER, 1},
                                  {MONS_TARANTELLA, 1}}}},
    { BAND_TARANTELLA,          {{{MONS_TARANTELLA, 10},
                                  {MONS_REDBACK, 3},
                                  {MONS_WOLF_SPIDER, 3},
                                  {MONS_CULICIVORA, 3},
                                  {MONS_ORB_SPIDER, 1}}}},
    { BAND_MIXED_SPIDERS,       {{{MONS_JUMPING_SPIDER, 3},
                                  {MONS_WOLF_SPIDER, 3},
                                  {MONS_TARANTELLA, 3},
                                  {MONS_ORB_SPIDER, 1},
                                  {MONS_REDBACK, 4},
                                  {MONS_CULICIVORA, 3}}}},

    { BAND_VAULT_WARDEN,        {{{MONS_VAULT_SENTINEL, 4},
                                  {MONS_IRONBOUND_CONVOKER, 6},
                                  {MONS_IRONBOUND_PRESERVER, 5},
                                  {MONS_IRONBOUND_FROSTHEART, 3},
                                  {MONS_IRONBOUND_THUNDERHULK, 2}},
        // one fancy pal, and a 50% chance of another
                                {{MONS_VAULT_SENTINEL, 4},
                                 {MONS_IRONBOUND_CONVOKER, 6},
                                 {MONS_IRONBOUND_PRESERVER, 5},
                                 {MONS_IRONBOUND_FROSTHEART, 3},
                                 {MONS_IRONBOUND_THUNDERHULK, 2},
                                 {MONS_VAULT_GUARD, 20}},

        // 25% chance of a polterguardian
                                {{MONS_POLTERGUARDIAN, 1},
                                 {MONS_VAULT_GUARD, 4}},

                                {{MONS_VAULT_GUARD, 1}}}},

    { BAND_FAUN_PARTY,          {{{MONS_MERFOLK_SIREN, 1}},

                                 {{MONS_FAUN, 1}}}},

    { BAND_WURMS_AND_MASTER,    {{{MONS_IRONBOUND_BEASTMASTER, 1}},

                                 {{MONS_LINDWURM, 1}}}},

    { BAND_SLIMES_AND_MASTER,   {{{MONS_IRONBOUND_BEASTMASTER, 1}},

                                 {{MONS_SLIME_CREATURE, 1}}}},

    { BAND_ELEPHANTS_AND_MASTER,{{{MONS_IRONBOUND_BEASTMASTER, 1}},

                                 {{MONS_DIRE_ELEPHANT, 1}}}},

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
                                  {MONS_SUN_MOTH, 3},
                                  {MONS_EFREET, 3}}}},

    { BAND_GLOORX_VLOQ,         {{{MONS_CURSE_SKULL, 1}},

                                 {{MONS_EXECUTIONER, 1}},

                                 {{MONS_SHADOW_DEMON, 1},
                                  {MONS_REAPER, 3},
                                  {MONS_SHADOW_WRAITH, 3}}}},

    { BAND_MNOLEG,              {{{MONS_PROTEAN_PROGENITOR, 1}},

                                 {{MONS_TENTACLED_MONSTROSITY, 1},
                                  {MONS_CACODEMON, 2},
                                  {MONS_SHADOW_DEMON, 3},
                                  {MONS_VERY_UGLY_THING, 3},
                                  {MONS_NEQOXEC, 3}}}},

    { BAND_LOM_LOBON,           {{{MONS_SPRIGGAN_AIR_MAGE, 1},
                                  {MONS_NAGARAJA, 1},
                                  {MONS_MERFOLK_AQUAMANCER, 1},
                                  {MONS_JOROGUMO, 1},
                                  {MONS_FENSTRIDER_WITCH, 1},
                                  {MONS_DRACONIAN_ANNIHILATOR, 2},
                                  {MONS_DEEP_ELF_ANNIHILATOR, 2},
                                  {MONS_GLOWING_ORANGE_BRAIN, 2},
                                  {MONS_BLIZZARD_DEMON, 2},
                                  {MONS_GREEN_DEATH, 2},
                                  {MONS_RAKSHASA, 4},
                                  {MONS_TITAN, 1},
                                  {MONS_LICH, 1}}}},

    { BAND_HOLIES,              {{{MONS_ANGEL, 100},
                                  {MONS_CHERUB, 80},
                                  {MONS_DAEVA, 50},
                                  {MONS_OPHAN, 1}}}}, // !?

    // one supporter, and maybe more
    { BAND_SALTLINGS,           {{{MONS_GUARDIAN_SERPENT, 1},
                                  {MONS_IRONBOUND_CONVOKER, 1},
                                  {MONS_RAGGED_HIEROPHANT, 2},
                                  {MONS_SERVANT_OF_WHISPERS, 2},
                                  {MONS_PEACEKEEPER, 2}},

                                 {{MONS_SALTLING, 150},
                                  {MONS_RAGGED_HIEROPHANT, 5},
                                  {MONS_SERVANT_OF_WHISPERS, 5},
                                  {MONS_PEACEKEEPER, 5},
                                  {MONS_MOLTEN_GARGOYLE, 5},
                                  {MONS_IRONBOUND_CONVOKER, 2},
                                  {MONS_GUARDIAN_SERPENT, 2},
                                  {MONS_IMPERIAL_MYRMIDON, 2}}}},
    // for wendigo ammo, mostly
    { BAND_SIMULACRA,           {{{MONS_SIMULACRUM, 1}}}},

    { BAND_BLOOD_SAINT,         {{{MONS_BALRUG, 1},
                                  {MONS_BLIZZARD_DEMON, 1}},

                                 {{MONS_DEMONSPAWN_BLOOD_SAINT, 1},
                                  {MONS_DEMONSPAWN_WARMONGER, 1},
                                  {MONS_DEMONSPAWN_CORRUPTER, 1},
                                  {MONS_DEMONSPAWN_SOUL_SCHOLAR, 1}}}},

    { BAND_WARMONGER,           {{{MONS_EXECUTIONER, 1},
                                  {MONS_SIN_BEAST, 3}},

                                 {{MONS_DEMONSPAWN_BLOOD_SAINT, 1},
                                  {MONS_DEMONSPAWN_WARMONGER, 1},
                                  {MONS_DEMONSPAWN_CORRUPTER, 1},
                                  {MONS_DEMONSPAWN_SOUL_SCHOLAR, 1}}}},

    { BAND_CORRUPTER,           {{{MONS_CACODEMON, 1},
                                  {MONS_SHADOW_DEMON, 3}},

                                 {{MONS_DEMONSPAWN_BLOOD_SAINT, 1},
                                  {MONS_DEMONSPAWN_WARMONGER, 1},
                                  {MONS_DEMONSPAWN_CORRUPTER, 1},
                                  {MONS_DEMONSPAWN_SOUL_SCHOLAR, 1}}}},

    { BAND_SOUL_SCHOLAR,        {{{MONS_REAPER, 1},
                                  {MONS_SOUL_EATER, 1}},

                                 {{MONS_DEMONSPAWN_BLOOD_SAINT, 1},
                                  {MONS_DEMONSPAWN_WARMONGER, 1},
                                  {MONS_DEMONSPAWN_CORRUPTER, 1},
                                  {MONS_DEMONSPAWN_SOUL_SCHOLAR, 1}}}},
    // for Grunn
    { BAND_DOOM_HOUNDS,         {{{MONS_DOOM_HOUND, 1}}}},
    // for Norris
    { BAND_SKYSHARKS,           {{{MONS_SKYSHARK, 1}}}},
    // for Arachne
    { BAND_ORB_SPIDERS,         {{{MONS_ORB_SPIDER, 1}}}},
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
    case BAND_RANDOM_SINGLE:
    {
        monster_type tmptype = MONS_PROGRAM_BUG;
        coord_def tmppos;
        return resolve_monster_type(RANDOM_BANDLESS_MONSTER, tmptype,
                                    PROX_ANYWHERE, &tmppos, 0,
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
    // 10% Pan lord or Boris
    //  - ~1% named pan lord / seraph / Boris
    //  - ~9% random pan lord
    // 15% Orb Guardian
    // 40% Demon
    //  - 25% greater demon
    //  - 10% common demon
    // 40% Pan spawn (can also include pan lords and demons)
    if (one_chance_in(10))
    {
        for (int i = 0; i < 4; i++)
        {
            // Sometimes pick an unique lord whose rune you've stolen.
            if (you.runes[RUNE_MNOLEG + i]
                && !you.unique_creatures[MONS_MNOLEG + i]
                && one_chance_in(10))
            {
                return static_cast<monster_type>(MONS_MNOLEG + i);
            }
        }

        // If Boris has spawned once and is not
        // currently alive he has a chance of coming for you on
        // the orb run
        if (you.props[KILLED_BORIS_KEY]
            && !you.unique_creatures[MONS_BORIS] && one_chance_in(10))
        {
            return MONS_BORIS;
        }

        if (one_chance_in(10))
            return MONS_SERAPH;

        return MONS_PANDEMONIUM_LORD;
    }

    return random_choose_weighted(
        15, MONS_ORB_GUARDIAN,
        25, RANDOM_DEMON_GREATER,
        10, RANDOM_DEMON_COMMON,
        40, pick_monster_no_rarity(BRANCH_PANDEMONIUM));
}

monster* mons_place(mgen_data mg)
{
#ifdef DEBUG_MON_CREATION
    mprf(MSGCH_DIAGNOSTICS, "in mons_place()");
#endif

    if (mg.cls == WANDERING_MONSTER)
    {
#ifdef DEBUG_MON_CREATION
        mprf(MSGCH_DIAGNOSTICS, "Set class RANDOM_MONSTER");
#endif
        mg.cls = RANDOM_MONSTER;
    }

    // This gives a slight challenge to the player as they ascend the
    // dungeon with the Orb.
    if (_is_random_monster(mg.cls) && player_on_orb_run()
        && !player_in_branch(BRANCH_ABYSS) && !mg.is_summoned())
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

        if (mg.behaviour == BEH_NEUTRAL || mg.behaviour == BEH_GOOD_NEUTRAL)
            creation->flags |= MF_WAS_NEUTRAL;

        if (mg.behaviour == BEH_CHARMED)
        {
            creation->attitude = ATT_HOSTILE;
            creation->add_ench(ENCH_CHARM);
        }

        if (!(mg.flags & MG_FORCE_BEH) && !crawl_state.game_is_arena())
            check_lovelessness(*creation);

        behaviour_event(creation, ME_EVAL);
    }

    // If MG_AUTOFOE is set, find the nearest valid foe and point this monster
    // towards it immediately.
    if (mg.flags & MG_AUTOFOE && (creation->attitude == ATT_FRIENDLY
                                  || mg.behaviour == BEH_CHARMED))
    {
        set_nearest_monster_foe(creation, true);
        const actor* foe = creation->get_foe();
        if (foe)
        {
            creation->behaviour = BEH_SEEK;
            creation->target = foe->pos();
        }
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

static bool _valid_spot(coord_def pos, bool check_mask=true)
{
    if (actor_at(pos))
        return false;
    if (check_mask && env.level_map_mask(pos) & MMT_NO_MONS)
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
    bool levelgen;
public:
    // Terrain that we can't spawn on, but that we can skip through.
    set<dungeon_feature_type> passable;
public:
    newmons_square_find(dungeon_feature_type grdw,
                        const coord_def &pos,
                        int maxdist = 0,
                        bool _levelgen=true)
        :  feat_wanted(grdw), maxdistance(maxdist),
           best_distance(0), nfound(0), levelgen(_levelgen)
    {
        start = pos;
    }

    // This is an overload, not an override!
    coord_def pathfind()
    {
        set_floodseed(start);
        return travel_pathfind::pathfind(RMODE_CONNECTIVITY);
    }

    bool path_flood(const coord_def &/*c*/, const coord_def &dc) override
    {
        if (best_distance && traveled_distance > best_distance)
            return true;

        if (!in_bounds(dc)
            || (maxdistance > 0 && traveled_distance > maxdistance))
        {
            return false;
        }
        if (!_feat_compatible(feat_wanted, env.grid(dc)))
        {
            if (passable.count(env.grid(dc)))
                good_square(dc);
            return false;
        }
        if (_valid_spot(dc, levelgen) && one_chance_in(++nfound))
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
                                         int distance,
                                         bool levelgen)
{
    coord_def p;

    const dungeon_feature_type feat_preferred =
        _monster_primary_habitat_feature(mons_class);
    const dungeon_feature_type feat_nonpreferred =
        _monster_secondary_habitat_feature(mons_class);

    newmons_square_find nmpfind(feat_preferred, start, distance, levelgen);
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
                              int preferred_radius, int max_radius,
                              int exclude_radius,
                              const actor* in_sight_of)
{
    coord_def pos(0, 0);

    // First search within our preferred radius.
    if (find_habitable_spot_near(p, mons_class, preferred_radius, pos,
                                 exclude_radius, in_sight_of))
    {
        return pos;
    }

    // If we didn't find a spot there, expand our search one radius at a time,
    // until we hit max_radius.
    for (int rad = preferred_radius + 1; rad <= max_radius; ++rad)
    {
        if (find_habitable_spot_near(p, mons_class, rad, pos, rad - 1, in_sight_of))
            return pos;
    }

    return pos;
}

conduct_type god_hates_monster(monster_type type)
{
    monster dummy;
    dummy.type = type;

    // no spellcasting/etc zombies currently; pick something that always works
    if (mons_class_is_zombified(type))
        define_zombie(&dummy, MONS_GOBLIN, type);
    else
        define_monster(dummy);

    return god_hates_monster(dummy);
}

/**
 * Is the player hated by all? If so, does this monster care?
 */
bool mons_can_hate(monster_type type)
{
    return you.allies_forbidden()
        // don't turn foxfire, blocks of ice, etc hostile
        && !mons_class_is_peripheral(type)
        // Thematically just the player poltergeist taking up more tiles
        && type != MONS_HAUNTED_ARMOUR;
}

void check_lovelessness(monster &mons)
{
    if (!mons_can_hate(mons.type) || !mons.wont_attack())
        return;

    mons.attitude = ATT_HOSTILE;
    mons.del_ench(ENCH_CHARM);
    behaviour_event(&mons, ME_ALERT, &you);
    mprf("%s feels only hate for you!", mons.name(DESC_THE).c_str());
}

/**
 * Does the player's current religion conflict with the given monster? If so,
 * why?
 *
 * XXX: this should ideally return a list of conducts that can be filtered by
 *      callers by god; we're duplicating god-conduct.cc right now.
 *
 * @param mon   The monster in question.
 * @return      The reason the player's religion conflicts with the monster
 *              (e.g. DID_EVIL for evil monsters), or DID_NOTHING.
 */
conduct_type god_hates_monster(const monster &mon)
{
    // Player angers all real monsters
    if (mons_can_hate(mon.type))
        return DID_SACRIFICE_LOVE;

    if (is_good_god(you.religion) && mon.evil())
        return DID_EVIL;

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

    return DID_NOTHING;
}

monster* create_monster(mgen_data mg, bool fail_msg)
{
    ASSERT(in_bounds(mg.pos)); // otherwise it's a guaranteed fail

    const monster_type montype = fixup_zombie_type(mg.cls, mg.base_type);

    monster *summd = 0;

    if (!mg.force_place()
        || monster_at(mg.pos)
        || you.pos() == mg.pos && !fedhas_passthrough_class(mg.cls)
        || !mons_class_can_pass(montype, env.grid(mg.pos)))
    {
        mg.pos = find_newmons_square(montype, mg.pos, mg.range_preferred,
                                     mg.range_max, mg.range_min,
                                     (mg.flags & MG_SEE_SUMMONER)
                                        ? mg.summoner
                                        : nullptr);
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

/**
 * Attempts to choose a random empty cell that a monster could occupy, within a
 * certain radius of a given position and with valid line of sight to that
 * position.
 *
 * @param where          The center point to begin searching.
 * @param mon_type       The type of monster to test habitability for.
 * @param radius         How far from the center point to include in the search.
 * @param result [out]  If a spot is chosen, this will be set to its location.
 *                      Will remain unchanged if no valid spot is found.
 * @param exclude_radius All spots within this distance of the center point will
 *                       be excluded from the search. (Defaults to -1, which
 *                       excludes nothing. 0 will exclude only the center.)
 * @param in_sight_of  If not null, spots will only be considered valid so long
 *                     as they are in sight of the given actor.
 *
 * @return Whether a valid spot was found. (If true, empty will be set to this spot)
 */
bool find_habitable_spot_near(const coord_def& where, monster_type mon_type,
                              int radius, coord_def& result, int exclude_radius,
                              const actor* in_sight_of)
{
    int good_count = 0;

    for (radius_iterator ri(where, radius, C_SQUARE, exclude_radius >= 0);
         ri; ++ri)
    {
        if (exclude_radius > 0 && grid_distance(where, *ri) <= exclude_radius)
            continue;

        if (actor_at(*ri))
            continue;

        if (!cell_see_cell(where, *ri, LOS_NO_TRANS))
            continue;

        if (!monster_habitable_grid(mon_type, *ri))
            continue;

        if (in_sight_of && !in_sight_of->see_cell_no_trans(*ri))
            continue;

        if (one_chance_in(++good_count))
            result = *ri;
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
                             MONS_SIN_BEAST,
                             MONS_HELLION,
                             MONS_REAPER,
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

void replace_boris()
{
    // Initial generation is governed by the vault uniq_boris. Once he is killed
    // a first time, as long as he isn't alive somewhere, he can regenerate when
    // a new level is entered.
    if (!you.props[KILLED_BORIS_KEY]
        || you.unique_creatures[MONS_BORIS]
        || !one_chance_in(6))
    {
        return;
    }

    // TODO: kind of ad hoc, maybe read from uniq_boris vault?
    switch (you.where_are_you)
    {
    case BRANCH_DEPTHS:
    case BRANCH_VAULTS:
    case BRANCH_TOMB:
    case BRANCH_CRYPT:
        break;
    default:
        return;
    }

    mgen_data boris = mgen_data(MONS_BORIS);
    mons_place(boris);
}

/////////////////////////////////////////////////////////////////////////////
//
// Random monsters for portal vaults. Used for e.g. shadow creatures.
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

// Code for determining Beogh apostle bands, based on power and apostle type
enum apostle_band_type
{
    APOSTLE_BAND_ORCS,
    APOSTLE_BAND_BRUTES,
    APOSTLE_BAND_DEMONS,
    APOSTLE_BAND_BEASTS,

    NUM_APOSTLE_BANDS,
};

static const vector<pop_entry> band_weights[] =
{

// APOSTLE_BAND_ORCS,
{
    {  0,  35,  400, FALL, MONS_ORC },
    {  0,  55,  350, SEMI, MONS_ORC_WARRIOR },
    { -10, 75,  125, PEAK, MONS_ORC_PRIEST },
    {  0,  55,  200, FALL, MONS_ORC_WIZARD },
    {  25, 100, 250, SEMI, MONS_ORC_KNIGHT },
    {  25, 110, 150, SEMI, MONS_ORC_HIGH_PRIEST },
    {  25, 110, 100, SEMI, MONS_ORC_SORCERER },
    {  50, 100, 200, RISE, MONS_ORC_WARLORD },
},

// APOSTLE_BAND_BRUTES
{
    {   0,  40,  50, FALL, MONS_NO_MONSTER },
    {   0,  30,  50, FALL, MONS_OGRE },
    {  20,  45,  35, FLAT, MONS_TROLL },
    {  25,  70,  50, SEMI, MONS_CYCLOPS },
    {  40, 120, 150, RISE, MONS_ETTIN },
    {  45, 80,   80, SEMI, MONS_IRON_TROLL },
    {  60, 120, 100, RISE, MONS_STONE_GIANT },
},

// APOSTLE_BAND_DEMONS,
{
    {0, 25, 100, FALL, MONS_CRIMSON_IMP},
    {0, 25, 100, FALL, MONS_WHITE_IMP},
    {0, 25, 100, FALL, MONS_UFETUBUS},
    {0, 25, 100, FALL, MONS_IRON_IMP},
    {0, 25, 100, FALL, MONS_SHADOW_IMP},

    {20, 55, 125, SEMI, MONS_ICE_DEVIL},
    {20, 55, 125, SEMI, MONS_RUST_DEVIL},
    {20, 55, 125, SEMI, MONS_ORANGE_DEMON},
    {20, 55, 125, SEMI, MONS_RED_DEVIL},
    {20, 55, 125, SEMI, MONS_HELLWING},

    {45, 70, 150, SEMI, MONS_SUN_DEMON},
    {45, 70, 150, SEMI, MONS_SOUL_EATER},
    {45, 70, 150, SEMI, MONS_SMOKE_DEMON},
    {45, 70, 150, SEMI, MONS_YNOXINUL},
    {45, 70, 150, SEMI, MONS_SIXFIRHY},

    {55, 110, 150, RISE, MONS_REAPER},
    {55, 110, 150, RISE, MONS_GREEN_DEATH},
    {55, 110, 150, RISE, MONS_BLIZZARD_DEMON},
    {55, 110, 150, RISE, MONS_BALRUG},
    {55, 110, 150, RISE, MONS_HELLION},
    {55, 110,  80, RISE, MONS_SHADOW_DEMON},
},

// APOSTLE_BAND_BEASTS,
{
    {0,  20,  200, FALL, MONS_HOUND},
    {0,  55,  200, PEAK, MONS_WOLF},
    {25,  60, 220, FLAT, MONS_WARG},
    {45,  70, 150, RISE, MONS_HELL_HOUND},
    {40,  70,  80, RISE, MONS_RAIJU},
    {35,  75, 120, SEMI, MONS_MANTICORE},
    {40,  80, 140, SEMI, MONS_LINDWURM},
    {70, 110,  90, PEAK, MONS_LINDWURM},
},

};

static void _fill_types_by_weight(vector<monster_type>& vec, apostle_band_type type, int pow,
                                  int num, bool all_same = false)
{
    monster_picker picker = monster_picker();

    monster_type mtype = picker.pick(band_weights[int(type)], pow, MONS_NO_MONSTER);

    for (int i = 0; i < num; ++i)
    {
        if (mtype != MONS_NO_MONSTER)
            vec.push_back(mtype);

        if (!all_same)
            mtype = picker.pick(band_weights[int(type)], pow, MONS_NO_MONSTER);
    }
}

static int _fill_apostle_band(monster& mons, monster_type* band)
{
    // Pull type and power from props, if they have been set
    const apostle_type type = mons.props.exists(APOSTLE_TYPE_KEY)
                                ? static_cast<apostle_type>(mons.props[APOSTLE_TYPE_KEY].get_int())
                                : APOSTLE_WARRIOR;

    const int pow = mons.props.exists(APOSTLE_BAND_POWER_KEY)
                        ? mons.props[APOSTLE_BAND_POWER_KEY].get_int()
                        : 50;

    vector<monster_type> vec;

    switch (type)
    {
        default:
        case APOSTLE_WARRIOR:
        case APOSTLE_PRIEST:
        {
            int num_orcs = random_range(0, 5);
            int num_brutes = (5 - num_orcs) / 2;

            if (pow > 50)
                num_brutes += random_range(0, 2);

            _fill_types_by_weight(vec, APOSTLE_BAND_ORCS, pow, num_orcs);
            _fill_types_by_weight(vec, APOSTLE_BAND_BRUTES, pow, num_brutes);
        }
        break;

        case APOSTLE_WIZARD:
            _fill_types_by_weight(vec, APOSTLE_BAND_DEMONS, pow, random_range(2, 3), coinflip());
            if (coinflip())
            {
                if (coinflip())
                    _fill_types_by_weight(vec, APOSTLE_BAND_DEMONS, pow / 2, random_range(2, 3), coinflip());
                else
                    _fill_types_by_weight(vec, APOSTLE_BAND_ORCS, pow, random_range(2, 3));
            }
        break;
    }

    for (unsigned int i = 0; i < vec.size(); ++i)
        band[i+1] = vec[i];

    return vec.size()+1;
}
