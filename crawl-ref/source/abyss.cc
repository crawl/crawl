/**
 * @file
 * @brief Misc abyss specific functions.
**/

#include "AppHdr.h"

#include "abyss.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>

#include "act-iter.h"
#include "areas.h"
#include "bloodspatter.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "dbg-scan.h"
#include "delay.h"
#include "dgn-overview.h"
#include "dgn-proclayouts.h"
#include "files.h"
#include "god-companions.h" // hep stuff
#include "god-passive.h" // passive_t::slow_abyss
#include "hiscores.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-pathfind.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-transit.h"
#include "notes.h"
#include "output.h" // redraw_screens
#include "religion.h"
#include "spl-clouds.h" // big_cloud
#include "stash.h"
#include "state.h"
#include "stairs.h"
#include "stringutil.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tileview.h"
#include "timed-effects.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

const coord_def ABYSS_CENTRE(GXM / 2, GYM / 2);

static const int ABYSSAL_RUNE_MAX_ROLL = 200;

abyss_state abyssal_state;

static ProceduralLayout *abyssLayout = nullptr, *levelLayout = nullptr;

typedef priority_queue<ProceduralSample, vector<ProceduralSample>, ProceduralSamplePQCompare> sample_queue;

static sample_queue abyss_sample_queue;
static vector<dungeon_feature_type> abyssal_features;
static list<monster*> displaced_monsters;

static void abyss_area_shift();
static void _push_items();
static void _push_displaced_monster(monster* mon);

// If not_seen is true, don't place the feature where it can be seen from
// the centre. Returns the chosen location, or INVALID_COORD if it
// could not be placed.
static coord_def _place_feature_near(const coord_def &centre,
                                     int radius,
                                     dungeon_feature_type candidate,
                                     dungeon_feature_type replacement,
                                     int tries, bool not_seen = false)
{
    coord_def cp = INVALID_COORD;
    for (int i = 0; i < tries; ++i)
    {
        cp = centre + coord_def(random_range(-radius, radius),
                                random_range(-radius, radius));

        if (cp == centre || !in_bounds(cp))
            continue;

        if (not_seen && cell_see_cell_nocache(cp, centre))
            continue;

        if (grd(cp) == candidate)
        {
            dprf(DIAG_ABYSS, "Placing %s at (%d,%d)",
                 dungeon_feature_name(replacement),
                 cp.x, cp.y);
            grd(cp) = replacement;
            return cp;
        }
    }
    return INVALID_COORD;
}

// Returns a feature suitable for use in the proto-Abyss level.
static dungeon_feature_type _abyss_proto_feature()
{
    return random_choose_weighted(3000, DNGN_FLOOR,
                                   600, DNGN_ROCK_WALL,
                                   300, DNGN_STONE_WALL,
                                   100, DNGN_METAL_WALL,
                                     1, DNGN_CLOSED_DOOR);
}

static void _write_abyssal_features()
{
    if (abyssal_features.empty())
        return;

    dprf(DIAG_ABYSS, "Writing a mock-up of old level.");
    ASSERT((int)abyssal_features.size() == sqr(2 * LOS_RADIUS + 1));
    const int scalar = 0xFF;
    int index = 0;
    for (int x = -LOS_RADIUS; x <= LOS_RADIUS; x++)
    {
        for (int y = -LOS_RADIUS; y <= LOS_RADIUS; y++)
        {
            coord_def p(x, y);
            const int dist = p.rdist();
            p += ABYSS_CENTRE;

            int chance = pow(0.98, dist * dist) * scalar;
            if (!map_masked(p, MMT_VAULT))
            {
                if (dist < 2 || x_chance_in_y(chance, scalar))
                {
                    if (abyssal_features[index] != DNGN_UNSEEN)
                    {
                        grd(p) = abyssal_features[index];
                        env.level_map_mask(p) = MMT_VAULT;
                        if (cell_is_solid(p))
                            delete_cloud(p);
                        if (monster* mon = monster_at(p))
                            _push_displaced_monster(mon);
                    }
                }
                else
                {
                    //Entombing the player is lame.
                    grd(p) = DNGN_FLOOR;
                }
            }

            ++index;
        }
    }

    _push_items();
    abyssal_features.clear();
}

// Returns the roll to use to check if we want to create an abyssal rune.
static int _abyssal_rune_roll()
{
    if (you.runes[RUNE_ABYSSAL] || you.depth < ABYSSAL_RUNE_MIN_LEVEL)
        return -1;
    const bool god_favoured = have_passive(passive_t::attract_abyssal_rune);

    const double depth = you.depth + god_favoured;

    return (int) pow(100.0, depth/(1 + brdepth[BRANCH_ABYSS]));
}

static void _abyss_fixup_vault(const vault_placement *vp)
{
    for (vault_place_iterator vi(*vp); vi; ++vi)
    {
        const coord_def p(*vi);
        const dungeon_feature_type feat(grd(p));
        if (feat_is_stair(feat)
            && feat != DNGN_EXIT_ABYSS
            && feat != DNGN_ABYSSAL_STAIR
            && !feat_is_portal_entrance(feat))
        {
            grd(p) = DNGN_FLOOR;
        }

        tile_init_flavour(p);
    }
}

static bool _abyss_place_map(const map_def *mdef)
{
    // This is to prevent the player position from being updated by vaults
    // until after everything is done.
    unwind_bool gen(crawl_state.generating_level, true);

    try
    {
        if (dgn_safe_place_map(mdef, true, false, INVALID_COORD))
        {
            _abyss_fixup_vault(env.level_vaults.back().get());
            return true;
        }
    }
    catch (dgn_veto_exception &e)
    {
        dprf(DIAG_ABYSS, "Abyss map placement vetoed: %s", e.what());
    }
    return false;
}

static bool _abyss_place_vault_tagged(const map_bitmask &abyss_genlevel_mask,
                                      const string &tag)
{
    const map_def *map = random_map_for_tag(tag, true, true, MB_FALSE);
    if (map)
    {
        unwind_vault_placement_mask vaultmask(&abyss_genlevel_mask);
        return _abyss_place_map(map);
    }
    return false;
}

static void _abyss_postvault_fixup()
{
    fixup_misplaced_items();
    link_items();
    env.markers.activate_all();
}

static bool _abyss_place_rune_vault(const map_bitmask &abyss_genlevel_mask)
{
    // Make sure we're not about to link bad items.
    debug_item_scan();

    bool result = false;
    int tries = 10;
    do
    {
        result = _abyss_place_vault_tagged(abyss_genlevel_mask, "abyss_rune");
    }
    while (!result && --tries);

    // Make sure the rune is linked.
    // XXX: I'm fairly sure this does nothing if result == false,
    //      but leaving it alone for now. -- Grunt
    _abyss_postvault_fixup();
    return result;
}

static bool _abyss_place_rune(const map_bitmask &abyss_genlevel_mask)
{
    // Use a rune vault if there's one.
    if (_abyss_place_rune_vault(abyss_genlevel_mask))
        return true;

    coord_def chosen_spot;
    int places_found = 0;

    // Pick a random spot to drop the rune. We specifically do not use
    // random_in_bounds and similar, because we may be dealing with a
    // non-rectangular region, and we want to place the rune fairly.
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const coord_def p(*ri);
        if (abyss_genlevel_mask(p)
            && grd(p) == DNGN_FLOOR && igrd(p) == NON_ITEM
            && one_chance_in(++places_found))
        {
            chosen_spot = p;
        }
    }

    if (places_found)
    {
        dprf(DIAG_ABYSS, "Placing abyssal rune at (%d,%d)",
             chosen_spot.x, chosen_spot.y);
        int item_ind  = items(true, OBJ_RUNES, RUNE_ABYSSAL, 0);
        if (item_ind != NON_ITEM)
            item_colour(mitm[item_ind]);
        move_item_to_grid(&item_ind, chosen_spot);
        return item_ind != NON_ITEM;
    }

    return false;
}

// Returns true if items can be generated on the given square.
static bool _abyss_square_accepts_items(const map_bitmask &abyss_genlevel_mask,
                                        coord_def p)
{
    return abyss_genlevel_mask(p)
           && grd(p) == DNGN_FLOOR
           && igrd(p) == NON_ITEM
           && !map_masked(p, MMT_VAULT);
}

static int _abyss_create_items(const map_bitmask &abyss_genlevel_mask,
                               bool placed_abyssal_rune)
{
    // During game start, number and level of items mustn't be higher than
    // that on level 1.
    int num_items = 150, items_level = 52;
    int items_placed = 0;

    if (player_in_starting_abyss())
    {
        num_items   = 3 + roll_dice(3, 11);
        items_level = 0;
    }

    const int abyssal_rune_roll = _abyssal_rune_roll();
    bool should_place_abyssal_rune = false;
    vector<coord_def> chosen_item_places;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        if (_abyss_square_accepts_items(abyss_genlevel_mask, *ri))
        {
            if (items_placed < num_items && one_chance_in(200))
            {
                // [ds] Don't place abyssal rune in this loop to avoid
                // biasing rune placement toward the north-west of the
                // abyss level. Instead, make a note of whether we
                // decided to place the abyssal rune at all, and if we
                // did, place it randomly somewhere in the map at the
                // end of the item-gen pass. We may as a result create
                // (num_items + 1) items instead of num_items, which
                // is acceptable.
                if (!placed_abyssal_rune && !should_place_abyssal_rune
                    && abyssal_rune_roll != -1
                    && x_chance_in_y(abyssal_rune_roll, ABYSSAL_RUNE_MAX_ROLL))
                {
                    should_place_abyssal_rune = true;
                }

                chosen_item_places.push_back(*ri);
            }
        }
    }

    if (!placed_abyssal_rune && should_place_abyssal_rune)
    {
        if (_abyss_place_rune(abyss_genlevel_mask))
            ++items_placed;
    }

    for (const coord_def place : chosen_item_places)
    {
        if (_abyss_square_accepts_items(abyss_genlevel_mask, place))
        {
            int thing_created = items(true, OBJ_RANDOM, OBJ_RANDOM,
                                      items_level);
            move_item_to_grid(&thing_created, place);
            if (thing_created != NON_ITEM)
                items_placed++;
        }
    }

    return items_placed;
}

static string _who_banished(const string &who)
{
    return who.empty() ? who : " (" + who + ")";
}

static int _banished_depth(const int power)
{
    // Linear, with the max going from (1,1) to (25,5)
    // and the min going from (9,1) to (27,5)
    // Currently using HD for power

    // This means an orc will send you to A:1, an orc warrior
    // has a small chance of A:2,
    // Elves have a good shot at sending you to A:3, but won't
    // always
    // Ancient Liches are sending you to A:5 and there's nothing
    // you can do about that.
    const int maxdepth = div_rand_round((power + 5), 6);
    const int mindepth = (4 * power + 7) / 23;
    return min(5, max(1, random_range(mindepth, maxdepth)));
}

void banished(const string &who, const int power)
{
    ASSERT(!crawl_state.game_is_arena());
    push_features_to_abyss();
    if (brdepth[BRANCH_ABYSS] == -1)
        return;

    if (player_in_branch(BRANCH_ABYSS))
    {
        if (level_id::current().depth < brdepth[BRANCH_ABYSS])
            down_stairs(DNGN_ABYSSAL_STAIR);
        else
        {
            // On Abyss:5 we can't go deeper; cause a shift to a new area
            mprf(MSGCH_BANISHMENT, "당신은 심연의 다른 구역으로 추방되었다.");
            abyss_teleport();
        }
        return;
    }

    const int depth = _banished_depth(power);
    const string what = make_stringf("어비스 %d층으로 전송됨", depth)
                      + _who_banished(who);
    take_note(Note(NOTE_MESSAGE, 0, 0, what), true);

    stop_delay(true);
    run_animation(ANIMATION_BANISH, UA_BRANCH_ENTRY, false);
    push_features_to_abyss();
    floor_transition(DNGN_ENTER_ABYSS, orig_terrain(you.pos()),
                     level_id(BRANCH_ABYSS, depth), true);
    // This is an honest abyss entry, mark milestone
    mark_milestone("abyss.enter",
        "was cast into the Abyss!" + _who_banished(who), "parent");

    // Xom just might decide to interfere.
    if (you_worship(GOD_XOM) && who != "Xom" && who != "wizard command"
        && who != "a distortion unwield")
    {
        xom_maybe_reverts_banishment(false, false);
    }
}

void push_features_to_abyss()
{
    abyssal_features.clear();

    for (int x = -LOS_RADIUS; x <= LOS_RADIUS; x++)
    {
        for (int y = -LOS_RADIUS; y <= LOS_RADIUS; y++)
        {
            coord_def p(x, y);

            p += you.pos();

            dungeon_feature_type feature = map_bounds(p) ? grd(p) : DNGN_UNSEEN;
            feature = sanitize_feature(feature);
            abyssal_features.push_back(feature);
        }
    }
}

static bool _abyss_check_place_feat(coord_def p,
                                    const int feat_chance,
                                    int *feats_wanted,
                                    bool *use_map,
                                    dungeon_feature_type which_feat,
                                    const map_bitmask &abyss_genlevel_mask)
{
    if (!which_feat)
        return false;

    const bool place_feat = feat_chance && one_chance_in(feat_chance);

    if (place_feat && feats_wanted)
        ++*feats_wanted;

    // Don't place features in bubbles.
    int wall_count = 0;
    for (adjacent_iterator ai(p); ai; ++ai)
        wall_count += cell_is_solid(p);
    if (wall_count > 6)
        return false;

    // There's no longer a need to check for features under items,
    // since we're working on fresh grids that are guaranteed
    // item-free.
    if (place_feat || (feats_wanted && *feats_wanted > 0))
    {
        dprf(DIAG_ABYSS, "Placing abyss feature: %s.",
             dungeon_feature_name(which_feat));

        // When placing Abyss exits, try to use a vault if we have one.
        if (which_feat == DNGN_EXIT_ABYSS
            && use_map && *use_map
            && _abyss_place_vault_tagged(abyss_genlevel_mask, "abyss_exit"))
        {
            *use_map = false;

            // Link the vault-placed items.
            _abyss_postvault_fixup();
        }
        else if (!abyss_genlevel_mask(p))
            return false;
        else
            grd(p) = which_feat;

        if (feats_wanted)
            --*feats_wanted;
        return true;
    }
    return false;
}

static dungeon_feature_type _abyss_pick_altar()
{
    // Lugonu has a flat 50% chance of corrupting the altar.
    if (coinflip())
        return DNGN_ALTAR_LUGONU;

    god_type god;

    do
    {
        god = random_god();
    }
    while (is_good_god(god));

    return altar_for_god(god);
}

static bool _abyssal_rune_at(const coord_def p)
{
    for (stack_iterator si(p); si; ++si)
        if (si->is_type(OBJ_RUNES, RUNE_ABYSSAL))
            return true;
    return false;
}

class xom_abyss_feature_amusement_check
{
private:
    bool exit_was_near;
    bool rune_was_near;

private:
    bool abyss_exit_nearness() const
    {
        // env.map_knowledge().known() doesn't work on unmappable levels because
        // mapping flags are not set on such levels.
        for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
            if (grd(*ri) == DNGN_EXIT_ABYSS && env.map_knowledge(*ri).seen())
                return true;

        return false;
    }

    bool abyss_rune_nearness() const
    {
        // See above comment about env.map_knowledge().known().
        for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
            if (env.map_knowledge(*ri).seen() && _abyssal_rune_at(*ri))
                return true;
        return false;
    }

public:
    xom_abyss_feature_amusement_check()
    {
        exit_was_near = abyss_exit_nearness();
        rune_was_near = abyss_rune_nearness();
    }

    // If the player was almost to the exit when it disappeared, Xom
    // is extremely amused. He's also extremely amused if the player
    // winds up right next to an exit when there wasn't one there
    // before. The same applies to Abyssal runes.
    ~xom_abyss_feature_amusement_check()
    {
        // Update known terrain
        viewwindow();

        const bool exit_is_near = abyss_exit_nearness();
        const bool rune_is_near = abyss_rune_nearness();

        if (exit_was_near && !exit_is_near || rune_was_near && !rune_is_near)
            xom_is_stimulated(200, "좀은 낄낄거리며 폭소했다.", true);

        if (!rune_was_near && rune_is_near || !exit_was_near && exit_is_near)
            xom_is_stimulated(200);
    }
};

static void _abyss_lose_monster(monster& mons)
{
    if (mons.needs_abyss_transit())
        mons.set_transit(level_id(BRANCH_ABYSS));
    // make sure we don't end up with an invalid hep ancestor
    else if (hepliaklqana_ancestor() == mons.mid)
    {
        remove_companion(&mons);
        you.duration[DUR_ANCESTOR_DELAY] = random_range(50, 150); //~5-15 turns
    }

    mons.destroy_inventory();
    monster_cleanup(&mons);
}

// If a sanctuary exists and is in LOS, moves it to keep it in the
// same place relative to the player's location after a shift. If the
// sanctuary is not in player LOS, removes it.
static void _abyss_move_sanctuary(const coord_def abyss_shift_start_centre,
                                  const coord_def abyss_shift_end_centre)
{
    if (env.sanctuary_time > 0 && in_bounds(env.sanctuary_pos))
    {
        if (you.see_cell(env.sanctuary_pos))
        {
            env.sanctuary_pos += (abyss_shift_end_centre -
                                  abyss_shift_start_centre);
        }
        else
            remove_sanctuary(false);
    }
}

static void _push_displaced_monster(monster* mon)
{
    displaced_monsters.push_back(mon);
}

static void _place_displaced_monsters()
{
    for (monster *mon : displaced_monsters)
    {
        if (mon->alive() && !mon->find_home_near_place(mon->pos()))
        {
            maybe_bloodify_square(mon->pos());
            if (you.can_see(*mon))
            {
                simple_monster_message(*mon, "은(는) 어비스로 끌려 들어갔다.",
                        MSGCH_BANISHMENT);
            }
            _abyss_lose_monster(*mon);

        }
    }

    displaced_monsters.clear();
}

static bool _pushy_feature(dungeon_feature_type feat)
{
    // Only completely impassible features and lava will push items.
    // In particular, deep water will not push items, because the item
    // will eventually become accessible again through abyss morphing.

    // Perhaps this should instead be merged with (the complement of)
    // _item_safe_square() in terrain.cc. Unlike this function, that
    // one treats traps as unsafe, but closed doors as safe.
    return feat_is_solid(feat) || feat == DNGN_LAVA;
}

static void _push_items()
{
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);
        if (!item.defined() || !in_bounds(item.pos) || item.held_by_monster())
            continue;

        if (!_pushy_feature(grd(item.pos)))
            continue;

        for (distance_iterator di(item.pos); di; ++di)
            if (!_pushy_feature(grd(*di)))
            {
                int j = i;
                move_item_to_grid(&j, *di, true);
                break;
            }
    }
}

// Deletes everything on the level at the given position.
// Things that are wiped:
// 1. Dungeon terrain (set to DNGN_UNSEEN)
// 2. Monsters (the player is unaffected)
// 3. Items
// 4. Clouds
// 5. Terrain properties
// 6. Terrain colours
// 7. Vault (map) mask
// 8. Vault id mask
// 9. Map markers

static void _abyss_wipe_square_at(coord_def p, bool saveMonsters=false)
{
    // Nuke terrain.
    destroy_shop_at(p);
    destroy_trap(p);

    // Nuke vault flag.
    if (map_masked(p, MMT_VAULT))
        env.level_map_mask(p) &= ~MMT_VAULT;

    grd(p) = DNGN_UNSEEN;

    // Nuke items.
    if (igrd(p) != NON_ITEM)
        dprf(DIAG_ABYSS, "Nuke item stack at (%d, %d)", p.x, p.y);
    lose_item_stack(p);

    // Nuke monster.
    if (monster* mon = monster_at(p))
    {
        ASSERT(mon->alive());

        if (saveMonsters)
            _push_displaced_monster(mon);
        else
            _abyss_lose_monster(*mon);
    }

    // Delete cloud.
    delete_cloud(p);

    env.pgrid(p)        = terrain_property_t{};
    env.grid_colours(p) = 0;
#ifdef USE_TILE
    env.tile_bk_fg(p)   = 0;
    env.tile_bk_bg(p)   = 0;
    env.tile_bk_cloud(p)= 0;
#endif
    tile_clear_flavour(p);
    tile_init_flavour(p);

    env.level_map_mask(p) = 0;
    env.level_map_ids(p)  = INVALID_MAP_INDEX;

    remove_markers_and_listeners_at(p);

    env.map_knowledge(p).clear();
    if (env.map_forgotten.get())
        (*env.map_forgotten.get())(p).clear();
    env.map_seen.set(p, false);
    StashTrack.update_stash(p);
}

// Removes monsters, clouds, dungeon features, and items from the
// level, torching all squares for which the supplied mask is false.
static void _abyss_wipe_unmasked_area(const map_bitmask &abyss_preserve_mask)
{
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        if (!abyss_preserve_mask(*ri))
            _abyss_wipe_square_at(*ri);
}

// Moves everything at src to dst.
static void _abyss_move_entities_at(coord_def src, coord_def dst)
{
    dgn_move_entities_at(src, dst, true, true, true);
}

// Move all vaults within the mask by the specified delta.
static void _abyss_move_masked_vaults_by_delta(const coord_def delta)
{
    set<int> vault_indexes;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const int vi = env.level_map_ids(*ri);
        if (vi != INVALID_MAP_INDEX)
            vault_indexes.insert(vi);
    }

    for (auto i : vault_indexes)
    {
        vault_placement &vp(*env.level_vaults[i]);
#ifdef DEBUG_DIAGNOSTICS
        const coord_def oldp = vp.pos;
#endif
        vp.pos += delta;
        dprf(DIAG_ABYSS, "Moved vault (%s) from (%d,%d)-(%d,%d)",
             vp.map.name.c_str(), oldp.x, oldp.y, vp.pos.x, vp.pos.y);
    }
}

/**
 * Updates the destination of a transporter that has been shifted to a new
 * center. Assumes that the transporter itself and its position marker have
 * already been moved.
 *
 * @param pos            Transporter's new (and current) location.
 * @param source_center  Center of where the transporter's vault area was
 *                       shifted from.
 * @param target_center  Center of where the transporter's vault area is being
 *                       shifted to.
 * @param shift_area     A map_bitmask of the entire area being shifted.

 */
static void _abyss_update_transporter(const coord_def &pos,
                                      const coord_def &source_centre,
                                      const coord_def &target_centre,
                                      const map_bitmask &shift_area)
{
    if (grd(pos) != DNGN_TRANSPORTER)
        return;

    // Get the marker, since we will need to modify it.
    map_position_marker *marker =
        get_position_marker_at(pos, DNGN_TRANSPORTER);
    if (!marker || marker->dest == INVALID_COORD)
        return;

    // Original destination is not being preserved, so disable the transporter.
    if (!shift_area.get(marker->dest))
    {
        marker->dest = INVALID_COORD;
        return;
    }

    marker->dest = marker->dest - source_centre + target_centre;
}

// Moves the player, monsters, terrain and items in the square (circle
// in movement distance) around the player with the given radius to
// the square centred on target_centre.
//
// Assumes:
// a) target can be truncated if not fully in bounds
// b) source and target areas may overlap
//
static void _abyss_move_entities(coord_def target_centre,
                                 map_bitmask *shift_area_mask)
{
    const coord_def source_centre = you.pos();
    const coord_def delta = (target_centre - source_centre).sgn();
    const map_bitmask original_area_mask = *shift_area_mask;

    // When moving a region, walk backward to handle overlapping
    // ranges correctly.
    coord_def direction = -delta;

    if (!direction.x)
        direction.x = 1;
    if (!direction.y)
        direction.y = 1;

    coord_def start(MAPGEN_BORDER, MAPGEN_BORDER);
    coord_def end(GXM - 1 - MAPGEN_BORDER, GYM - 1 - MAPGEN_BORDER);

    if (direction.x == -1)
        swap(start.x, end.x);
    if (direction.y == -1)
        swap(start.y, end.y);

    end += direction;

    for (int y = start.y; y != end.y; y += direction.y)
    {
        for (int x = start.x; x != end.x; x += direction.x)
        {
            const coord_def src(x, y);
            if (!shift_area_mask->get(src))
                continue;

            shift_area_mask->set(src, false);

            const coord_def dst = src - source_centre + target_centre;
            if (map_bounds_with_margin(dst, MAPGEN_BORDER))
            {
                shift_area_mask->set(dst);
                // Wipe the dstination clean before dropping things on it.
                _abyss_wipe_square_at(dst);
                _abyss_move_entities_at(src, dst);
                _abyss_update_transporter(dst, source_centre, target_centre,
                                          original_area_mask);
            }
            else
            {
                // Wipe the source clean even if the dst is not in bounds.
                _abyss_wipe_square_at(src);
            }
        }
    }

    _abyss_move_masked_vaults_by_delta(target_centre - source_centre);
}

static void _abyss_expand_mask_to_cover_vault(map_bitmask *mask,
                                              int map_index)
{
    dprf(DIAG_ABYSS, "Expanding mask to cover vault %d (nvaults: %u)",
         map_index, (unsigned int)env.level_vaults.size());
    const vault_placement &vp = *env.level_vaults[map_index];
    for (vault_place_iterator vpi(vp); vpi; ++vpi)
        mask->set(*vpi);
}

// Identifies the smallest square around the given source that can be
// shifted without breaking up any vaults.
static void _abyss_identify_area_to_shift(coord_def source, int radius,
                                          map_bitmask *mask)
{
    mask->reset();

    set<int> affected_vault_indexes;
    for (rectangle_iterator ri(source, radius); ri; ++ri)
    {
        if (!map_bounds_with_margin(*ri, MAPGEN_BORDER))
            continue;

        mask->set(*ri);

        const int map_index = env.level_map_ids(*ri);
        if (map_index != INVALID_MAP_INDEX)
            affected_vault_indexes.insert(map_index);
    }

    for (auto i : affected_vault_indexes)
        _abyss_expand_mask_to_cover_vault(mask, i);
}

static void _abyss_invert_mask(map_bitmask *mask)
{
    for (rectangle_iterator ri(0); ri; ++ri)
        mask->set(*ri, !mask->get(*ri));
}

// Moves everything in the given radius around the player (where radius=0 =>
// only the player) to another part of the level, centred on target_centre.
// Everything not in the given radius is wiped to DNGN_UNSEEN and the provided
// map_bitmask is set to true for all wiped squares.
//
// Things that are moved:
// 1. Dungeon terrain
// 2. Actors (player + monsters)
// 3. Items
// 4. Clouds
// 5. Terrain properties
// 6. Terrain colours
// 7. Vaults
// 8. Vault (map) mask
// 9. Vault id mask
// 10. Map markers
//
// After the shift, any vaults that are no longer referenced in the id
// mask will be discarded. If those vaults had any unique tags or
// names, the tag/name will NOT be unregistered.
//
// Assumes:
// a) radius >= LOS_RADIUS
// b) All points in the source and target areas are in bounds.
static void _abyss_shift_level_contents_around_player(
    int radius,
    coord_def target_centre,
    map_bitmask &abyss_destruction_mask)
{
    const coord_def source_centre = you.pos();

    abyssal_state.major_coord += (source_centre - ABYSS_CENTRE);

    ASSERT(radius >= LOS_RADIUS);
    // This should only really happen due to wizmode blink/movement.
    // 1KB: ... yet at least Xom "swap with monster" triggers it.
    //ASSERT(map_bounds_with_margin(source_centre, radius));
    //ASSERT(map_bounds_with_margin(target_centre, radius));

    _abyss_identify_area_to_shift(source_centre, radius,
                                  &abyss_destruction_mask);

    // Shift sanctuary centre if it's close.
    _abyss_move_sanctuary(you.pos(), target_centre);

    // Zap everything except the area we're shifting, so that there's
    // nothing in the way of moving stuff.
    _abyss_wipe_unmasked_area(abyss_destruction_mask);

    // Move stuff to its new home. This will also move the player.
    _abyss_move_entities(target_centre, &abyss_destruction_mask);

    // [ds] Rezap everything except the shifted area. NOTE: the old
    // code did not do this, leaving a repeated swatch of Abyss behind
    // at the old location for every shift; discussions between Linley
    // and dpeg on ##crawl confirm that this (repeated swatch of
    // terrain left behind) was not intentional.
    _abyss_wipe_unmasked_area(abyss_destruction_mask);

    // So far we've used the mask to track the portions of the level we're
    // preserving. The inverse of the mask represents the area to be filled
    // with brand new abyss:
    _abyss_invert_mask(&abyss_destruction_mask);

    // Update env.level_vaults to discard any vaults that are no longer in
    // the picture.
    dgn_erase_unused_vault_placements();
}

static void _abyss_generate_monsters(int nmonsters)
{
    if (crawl_state.disables[DIS_SPAWNS])
        return;

    mgen_data mg;
    mg.proximity = PROX_ANYWHERE;

    for (int mcount = 0; mcount < nmonsters; mcount++)
    {
        mg.cls = pick_random_monster(level_id::current());
        if (!invalid_monster_type(mg.cls))
            mons_place(mg);
    }
}


void maybe_shift_abyss_around_player()
{
    ASSERT(player_in_branch(BRANCH_ABYSS));
    if (map_bounds_with_margin(you.pos(),
                               MAPGEN_BORDER + ABYSS_AREA_SHIFT_RADIUS + 1))
    {
        return;
    }

    dprf(DIAG_ABYSS, "Shifting abyss at (%d,%d)", you.pos().x, you.pos().y);

    abyss_area_shift();
    if (you.pet_target != MHITYOU)
        you.pet_target = MHITNOT;

#ifdef DEBUG_DIAGNOSTICS
    int j = count_if(begin(mitm), end(mitm), mem_fn(&item_def::defined));

    dprf(DIAG_ABYSS, "Number of items present: %d", j);

    j = 0;
    for (monster_iterator mi; mi; ++mi)
        ++j;

    dprf(DIAG_ABYSS, "Number of monsters present: %d", j);
    dprf(DIAG_ABYSS, "Number of clouds present: %d", int(env.cloud.size()));
#endif
}

void save_abyss_uniques()
{
    for (monster_iterator mi; mi; ++mi)
        if (mi->needs_abyss_transit()
            && !testbits(mi->flags, MF_TAKING_STAIRS))
        {
            mi->set_transit(level_id(BRANCH_ABYSS));
        }
}

static bool _in_wastes(const coord_def &p)
{
    return p.x > 0 && p.x < 0x7FFFFFF && p.y > 0 && p.y < 0x7FFFFFF;
}

static level_id _get_random_level()
{
    vector<level_id> levels;
    for (branch_iterator it; it; ++it)
    {
        if (it->id == BRANCH_ABYSS || it->id == BRANCH_SHOALS)
            continue;
        for (int j = 1; j <= brdepth[it->id]; ++j)
        {
            const level_id id(it->id, j);
            if (is_existing_level(id))
                levels.push_back(id);
        }
    }
    if (levels.empty())
    {
        // Let this fail later on.
        return level_id(static_cast<branch_type>(BRANCH_DUNGEON), 1);
    }

    return levels[hash_rand(levels.size(), abyssal_state.seed)];
}

/**************************************************************/
/* Fixed layouts (ie, those that depend only on abyss coords) */
/**************************************************************/
const static WastesLayout wastes;
const static DiamondLayout diamond30(3,0);
const static DiamondLayout diamond21(2,1);
const static ColumnLayout column2(2);
const static ColumnLayout column26(2,6);
const static ProceduralLayout* regularLayouts[] =
{
    &diamond30, &diamond21, &column2, &column26,
};
const static vector<const ProceduralLayout*> layout_vec(regularLayouts,
    regularLayouts + ARRAYSZ(regularLayouts));
const static WorleyLayout worleyL(123456, layout_vec);
const static RoilingChaosLayout chaosA(8675309, 450);
const static RoilingChaosLayout chaosB(7654321, 400);
const static RoilingChaosLayout chaosC(24324,   380);
const static RoilingChaosLayout chaosD(24816,   500);
const static NewAbyssLayout newAbyssLayout(7629);
const static ProceduralLayout* mixedLayouts[] =
{
    &chaosA, &worleyL, &chaosB, &chaosC, &chaosD, &newAbyssLayout,
};
const static vector<const ProceduralLayout*> mixed_vec(mixedLayouts,
    mixedLayouts + ARRAYSZ(mixedLayouts));
const static WorleyLayout layout(4321, mixed_vec);
const static ProceduralLayout* baseLayouts[] = { &newAbyssLayout, &layout };
const static vector<const ProceduralLayout*> base_vec(baseLayouts,
    baseLayouts + ARRAYSZ(baseLayouts));
const static WorleyLayout baseLayout(314159, base_vec, 5.0);
const static RiverLayout rivers(1800, baseLayout);
// This one is not fixed: [0] is a level pulled from the current game
static vector<const ProceduralLayout*> complex_vec(2);

static ProceduralSample _abyss_grid(const coord_def &p)
{
    const coord_def pt = p + abyssal_state.major_coord;

    if (_in_wastes(pt))
    {
        ProceduralSample sample = wastes(pt, abyssal_state.depth);
        abyss_sample_queue.push(sample);
        return sample;
    }

    if (abyssLayout == nullptr)
    {
        const level_id lid = _get_random_level();
        levelLayout = new LevelLayout(lid, 5, rivers);
        complex_vec[0] = levelLayout;
        complex_vec[1] = &rivers; // const
        abyssLayout = new WorleyLayout(23571113, complex_vec, 6.1);
    }

    const ProceduralSample sample = (*abyssLayout)(pt, abyssal_state.depth);
    ASSERT(sample.feat() > DNGN_UNSEEN);

    abyss_sample_queue.push(sample);
    return sample;
}

static cloud_type _cloud_from_feat(const dungeon_feature_type &ft)
{
    switch (ft)
    {
        case DNGN_CLOSED_DOOR:
        case DNGN_METAL_WALL:
            return CLOUD_GREY_SMOKE;
        case DNGN_CRYSTAL_WALL:
        case DNGN_ROCK_WALL:
        case DNGN_SLIMY_WALL:
        case DNGN_STONE_WALL:
        case DNGN_PERMAROCK_WALL:
            return random_choose(CLOUD_BLUE_SMOKE, CLOUD_PURPLE_SMOKE);
        case DNGN_CLEAR_ROCK_WALL:
        case DNGN_CLEAR_STONE_WALL:
        case DNGN_CLEAR_PERMAROCK_WALL:
        case DNGN_GRATE:
            return CLOUD_MIST;
        case DNGN_ORCISH_IDOL:
        case DNGN_GRANITE_STATUE:
        case DNGN_LAVA:
            return CLOUD_BLACK_SMOKE;
        case DNGN_DEEP_WATER:
        case DNGN_SHALLOW_WATER:
        case DNGN_FOUNTAIN_BLUE:
            return one_chance_in(5) ? CLOUD_RAIN : CLOUD_BLUE_SMOKE;
        case DNGN_FOUNTAIN_SPARKLING:
            return CLOUD_RAIN;
        default:
            return CLOUD_NONE;
    }
}

static dungeon_feature_type _veto_dangerous_terrain(dungeon_feature_type feat)
{
    if (feat == DNGN_DEEP_WATER)
        return DNGN_SHALLOW_WATER;
    if (feat == DNGN_LAVA)
        return DNGN_FLOOR;
    if (feat_is_solid(feat))
        return DNGN_FLOOR;

    return feat;
}

static void _update_abyss_terrain(const coord_def &p,
    const map_bitmask &abyss_genlevel_mask, bool morph)
{
    const coord_def rp = p - abyssal_state.major_coord;
    // ignore dead coordinates
    if (!in_bounds(rp))
        return;

    const dungeon_feature_type currfeat = grd(rp);

    // Don't decay vaults.
    if (map_masked(rp, MMT_VAULT))
        return;

    switch (currfeat)
    {
        case DNGN_EXIT_ABYSS:
        case DNGN_ABYSSAL_STAIR:
            return;
        default:
            break;
    }

    if (feat_is_altar(currfeat))
        return;

    if (!abyss_genlevel_mask(rp))
        return;

    if (currfeat != DNGN_UNSEEN && !morph)
        return;

    // What should have been there previously?  It might not be because
    // of external changes such as digging.
    const ProceduralSample sample = _abyss_grid(rp);

    // Enqueue the update, but don't morph.
    if (_abyssal_rune_at(rp))
        return;

    dungeon_feature_type feat = sample.feat();

    // Don't replace open doors with closed doors!
    if (feat_is_door(currfeat) && feat_is_door(feat))
        return;

    // Veto dangerous terrain.
    if (you.pos() == rp)
        feat = _veto_dangerous_terrain(feat);
    // Veto morph when there's a submerged monster (or a plant) below you.
    if (you.pos() == rp && mgrd(rp) != NON_MONSTER)
        feat = currfeat;

    // If the selected grid is already there, *or* if we're morphing and
    // the selected grid should have been there, do nothing.
    if (feat != currfeat)
    {
        grd(rp) = feat;
        if (feat == DNGN_FLOOR && in_los_bounds_g(rp))
        {
            cloud_type cloud = _cloud_from_feat(currfeat);
            int cloud_life = (_in_wastes(abyssal_state.major_coord) ? 5 : 2) + random2(2);
            if (cloud != CLOUD_NONE)
                check_place_cloud(_cloud_from_feat(currfeat), rp, cloud_life, 0, 3);
        }
        else if (feat_is_solid(feat))
            delete_cloud(rp);
        monster* mon = monster_at(rp);
        if (mon && !monster_habitable_grid(mon, feat))
            _push_displaced_monster(mon);
    }
}

static void _destroy_all_terrain(bool vaults)
{
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        if (vaults && env.level_map_mask(*ri) & MMT_VAULT)
            continue;
        env.level_map_mask(*ri) = MMT_TURNED_TO_FLOOR;
    }
}

static void _ensure_player_habitable(bool dig_instead)
{
    dungeon_feature_type feat = grd(you.pos());
    if (!you.can_pass_through_feat(feat) || is_feat_dangerous(feat))
    {
        bool shoved = you.shove();
        if (!shoved)
        {
            // legal only if we just placed a vault
            ASSERT(dig_instead);
            grd(you.pos()) = DNGN_FLOOR;
        }
    }
}

static void _abyss_apply_terrain(const map_bitmask &abyss_genlevel_mask,
                                 bool morph = false, bool now = false)
{
    // The chance is reciprocal to these numbers.
    const int exit_chance = you.runes[RUNE_ABYSSAL] ? 1250
                            : 7500 - 1250 * (you.depth - 1);

    int exits_wanted  = 0;
    int altars_wanted = 0;
    bool use_abyss_exit_map = true;
    bool used_queue = false;
    if (morph && !abyss_sample_queue.empty())
    {
        int ii = 0;
        used_queue = true;
        while (!abyss_sample_queue.empty()
            && abyss_sample_queue.top().changepoint() < abyssal_state.depth)
        {
            ++ii;
            coord_def p = abyss_sample_queue.top().coord();
            _update_abyss_terrain(p, abyss_genlevel_mask, morph);
            abyss_sample_queue.pop();
        }
/*
        if (ii)
            dprf(DIAG_ABYSS, "Examined %d features.", ii);
*/
    }

    int ii = 0;
    int delta = you.time_taken * (you.abyss_speed + 40) / 200;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const coord_def p(*ri);
        const coord_def abyss_coord = p + abyssal_state.major_coord;
        bool turned_to_floor = map_masked(p, MMT_TURNED_TO_FLOOR);
        if (used_queue && !turned_to_floor)
            continue;

        if (turned_to_floor && (now || x_chance_in_y(delta, 50))
            || !turned_to_floor && !used_queue)
        {
            ++ii;
            _update_abyss_terrain(abyss_coord, abyss_genlevel_mask, morph);
            env.level_map_mask(p) &= ~MMT_TURNED_TO_FLOOR;
        }
        if (morph)
            continue;

        // Place abyss exits, stone arches, and altars to liven up the scene
        // (only on area creation, not on morphing).
        _abyss_check_place_feat(p, exit_chance,
                                &exits_wanted,
                                &use_abyss_exit_map,
                                DNGN_EXIT_ABYSS,
                                abyss_genlevel_mask)
        ||
        _abyss_check_place_feat(p, 10000,
                                &altars_wanted,
                                nullptr,
                                _abyss_pick_altar(),
                                abyss_genlevel_mask)
        ||
        level_id::current().depth < brdepth[BRANCH_ABYSS]
        && _abyss_check_place_feat(p, 1900, nullptr, nullptr,
                                   DNGN_ABYSSAL_STAIR,
                                   abyss_genlevel_mask);
    }
    if (ii)
        dprf(DIAG_ABYSS, "Nuked %d features", ii);
    _ensure_player_habitable(false);
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        ASSERT_RANGE(grd(*ri), DNGN_UNSEEN + 1, NUM_FEATURES);
}

static int _abyss_place_vaults(const map_bitmask &abyss_genlevel_mask)
{
    unwind_vault_placement_mask vaultmask(&abyss_genlevel_mask);

    int vaults_placed = 0;

    bool extra = false;
    const int maxvaults = 6;
    int tries = 0;
    while (vaults_placed < maxvaults)
    {
        const map_def *map = random_map_in_depth(level_id::current(), extra);
        if (map)
        {
            if (_abyss_place_map(map) && !map->has_tag("extra"))
            {
                extra = true;

                if (!one_chance_in(2 + (++vaults_placed)))
                    break;
            }
            else
            {
                if (tries++ >= 100)
                    break;

                continue;
            }

        }
        else
        {
            if (extra)
                break;
            else
            {
                extra = true;
                continue;
            }
        }
    }

    return vaults_placed;
}

static void _generate_area(const map_bitmask &abyss_genlevel_mask)
{
    // Any rune on the floor prevents the abyssal rune from being generated.
    const bool placed_abyssal_rune = find_floor_item(OBJ_RUNES);

    dprf(DIAG_ABYSS, "_generate_area(). turns_on_level: %d, rune_on_floor: %s",
         env.turns_on_level, placed_abyssal_rune? "yes" : "no");

    _abyss_apply_terrain(abyss_genlevel_mask);

    // Make sure we're not about to link bad items.
    debug_item_scan();
    _abyss_place_vaults(abyss_genlevel_mask);

    // Link the vault-placed items.
    _abyss_postvault_fixup();

    _abyss_create_items(abyss_genlevel_mask, placed_abyssal_rune);
    setup_environment_effects();

    _ensure_player_habitable(true);

    // Abyss has a constant density.
    env.density = 0;
}

static void _initialize_abyss_state()
{
    abyssal_state.major_coord.x = get_uint32() & 0x7FFFFFFF;
    abyssal_state.major_coord.y = get_uint32() & 0x7FFFFFFF;
    abyssal_state.seed = get_uint32() & 0x7FFFFFFF;
    abyssal_state.phase = 0.0;
    abyssal_state.depth = get_uint32() & 0x7FFFFFFF;
    abyssal_state.destroy_all_terrain = false;
    abyssal_state.level = _get_random_level();
    abyss_sample_queue = sample_queue(ProceduralSamplePQCompare());
}

void set_abyss_state(coord_def coord, uint32_t depth)
{
    abyssal_state.major_coord = coord;
    abyssal_state.depth = depth;
    abyssal_state.seed = get_uint32() & 0x7FFFFFFF;
    abyssal_state.phase = 0.0;
    abyssal_state.destroy_all_terrain = true;
    abyss_sample_queue = sample_queue(ProceduralSamplePQCompare());
    you.moveto(ABYSS_CENTRE);
    map_bitmask abyss_genlevel_mask(true);
    _abyss_apply_terrain(abyss_genlevel_mask, true, true);
}

static void abyss_area_shift()
{
    dprf(DIAG_ABYSS, "area_shift() - player at pos (%d, %d)",
         you.pos().x, you.pos().y);

    {
        xom_abyss_feature_amusement_check xomcheck;

        // A teleport may move you back to the center, resulting in a (0,0)
        // shift. The code can't handle those. We still to forget the map,
        // spawn new monsters or allow return from transit, though.
        if (you.pos() != ABYSS_CENTRE)
        {
            // Use a map mask to track the areas that the shift destroys and
            // that must be regenerated by _generate_area.
            map_bitmask abyss_genlevel_mask;
            _abyss_shift_level_contents_around_player(
                ABYSS_AREA_SHIFT_RADIUS, ABYSS_CENTRE, abyss_genlevel_mask);
            _generate_area(abyss_genlevel_mask);
        }
        forget_map(true);

        // Update LOS at player's new abyssal vacation retreat.
        los_changed();
    }

    // Place some monsters to keep the abyss party going.
    int num_monsters = 15 + you.depth * (1 + coinflip());
    _abyss_generate_monsters(num_monsters);

    // And allow monsters in transit another chance to return.
    place_transiting_monsters();

    check_map_validity();
}

void destroy_abyss()
{
    if (abyssLayout)
    {
        delete abyssLayout;
        abyssLayout = nullptr;
        delete levelLayout;
        levelLayout = nullptr;
    }
}

static colour_t _roll_abyss_floor_colour()
{
    return random_choose_weighted<colour_t>(
         108, BLUE,
         632, GREEN,
         // no CYAN (silence)
         932, RED,
         488, MAGENTA,
         433, BROWN,
        3438, LIGHTGRAY,
         // no DARKGREY (out of LOS)
         766, LIGHTBLUE,
         587, LIGHTGREEN,
         794, LIGHTCYAN,
         566, LIGHTRED,
         313, LIGHTMAGENTA,
         // no YELLOW (halo)
         890, WHITE,
          50, ETC_FIRE);
}

static colour_t _roll_abyss_rock_colour()
{
    return random_choose_weighted<colour_t>(
         130, BLUE,
         409, GREEN,
         // no CYAN (metal)
         770, RED,
         522, MAGENTA,
        1292, BROWN,
         // no LIGHTGRAY (stone)
         // no DARKGREY (out of LOS)
         570, LIGHTBLUE,
         705, LIGHTGREEN,
         // no LIGHTCYAN (glass)
        1456, LIGHTRED,
         377, LIGHTMAGENTA,
         105, YELLOW,
         101, WHITE,
          60, ETC_FIRE);
}

static void _abyss_generate_new_area()
{
    _initialize_abyss_state();
    dprf(DIAG_ABYSS, "Abyss Coord (%d, %d)",
         abyssal_state.major_coord.x, abyssal_state.major_coord.y);
    remove_sanctuary(false);

    env.floor_colour = _roll_abyss_floor_colour();
    env.rock_colour = _roll_abyss_rock_colour();
    tile_init_flavour();

    map_bitmask abyss_genlevel_mask;
    _abyss_wipe_unmasked_area(abyss_genlevel_mask);
    dgn_erase_unused_vault_placements();

    you.moveto(ABYSS_CENTRE);
    abyss_genlevel_mask.init(true);
    _generate_area(abyss_genlevel_mask);
    if (one_chance_in(5))
    {
        _place_feature_near(you.pos(), LOS_RADIUS,
                            DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50);
    }

    los_changed();
    place_transiting_monsters();
}

// Check if there is a path between the abyss centre and an exit location.
static bool _abyss_has_path(const coord_def &to)
{
    ASSERT(grd(to) == DNGN_EXIT_ABYSS);

    monster_pathfind pf;
    return pf.init_pathfind(ABYSS_CENTRE, to);
}

// Generate the initial (proto) Abyss level. The proto Abyss is where
// the player lands when they arrive in the Abyss from elsewhere.
// _generate_area generates all other Abyss areas.
void generate_abyss()
{
    env.level_build_method += " abyss";
    env.level_layout_types.insert("abyss");
    destroy_abyss();

retry:
    _initialize_abyss_state();

    dprf(DIAG_ABYSS, "generate_abyss(); turn_on_level: %d",
         env.turns_on_level);

    // Generate the initial abyss without vaults. Vaults are horrifying.
    _abyss_generate_new_area();
    _write_abyssal_features();
    map_bitmask abyss_genlevel_mask(true);
    _abyss_apply_terrain(abyss_genlevel_mask);

    grd(you.pos()) = _veto_dangerous_terrain(grd(you.pos()));
    _place_displaced_monsters();

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        ASSERT(grd(*ri) > DNGN_UNSEEN);
    check_map_validity();

    // If we're starting out in the Abyss, make sure the starting grid is
    // an altar to Lugonu and there's an exit near-by.
    // Otherwise, we start out on floor and there's a chance there's an
    // altar near-by.
    if (player_in_starting_abyss())
    {
        grd(ABYSS_CENTRE) = DNGN_ALTAR_LUGONU;
        const coord_def eloc = _place_feature_near(ABYSS_CENTRE, LOS_RADIUS + 2,
                                                   DNGN_FLOOR, DNGN_EXIT_ABYSS,
                                                   50, true);
        // Now make sure there is a path from the abyss centre to the exit.
        // If for some reason an exit could not be placed, don't bother.
        if (eloc == INVALID_COORD || !_abyss_has_path(eloc))
            goto retry;
    }
    else
    {
        grd(ABYSS_CENTRE) = DNGN_FLOOR;
        if (one_chance_in(5))
        {
            _place_feature_near(ABYSS_CENTRE, LOS_RADIUS,
                                DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50);
        }
    }

    setup_environment_effects();
}

static void _increase_depth()
{
    int delta = you.time_taken * (you.abyss_speed + 40) / 200;
    if (!have_passive(passive_t::slow_abyss))
        delta *= 2;
    if (you.duration[DUR_TELEPORT])
        delta *= 5;
    const double theta = abyssal_state.phase;
    double depth_change = delta * (0.2 + 2.8 * pow(sin(theta/2), 10.0));
    abyssal_state.depth += depth_change;
    abyssal_state.phase += delta / 100.0;
    if (abyssal_state.phase > PI)
        abyssal_state.phase -= PI;
}

void abyss_morph()
{
    if (abyssal_state.destroy_all_terrain)
    {
        _destroy_all_terrain(false);
        abyssal_state.destroy_all_terrain = false;
    }
    if (!player_in_branch(BRANCH_ABYSS))
        return;
    _increase_depth();
    map_bitmask abyss_genlevel_mask(true);
    dgn_erase_unused_vault_placements();
    _abyss_apply_terrain(abyss_genlevel_mask, true);
    _place_displaced_monsters();
    _push_items();
    los_changed();
}

void abyss_teleport()
{
    xom_abyss_feature_amusement_check xomcheck;
    dprf(DIAG_ABYSS, "New area Abyss teleport.");
    mprf(MSGCH_BANISHMENT, "당신은 갑자기 심연의 다른 구역으로 끌려 들어갔다!");
    _abyss_generate_new_area();
    _write_abyssal_features();
    grd(you.pos()) = _veto_dangerous_terrain(grd(you.pos()));
    stop_delay(true);
    forget_map(false);
    clear_excludes();
    more();
}

//////////////////////////////////////////////////////////////////////////////
// Abyss effects in other levels, courtesy Lugonu.

struct corrupt_env
{
    int rock_colour, floor_colour;
    corrupt_env(): rock_colour(BLACK), floor_colour(BLACK) { }
};

static void _place_corruption_seed(const coord_def &pos, int duration)
{
    env.markers.add(new map_corruption_marker(pos, duration));
    // Corruption markers don't need activation, though we might
    // occasionally miss other unactivated markers by clearing.
    env.markers.clear_need_activate();
}

static void _initialise_level_corrupt_seeds(int power)
{
    const int low = power * 40 / 100, high = power * 140 / 100;
    const int nseeds = random_range(-1, min(2 + power / 110, 4), 2);

    const int aux_seed_radius = 4;

    dprf("Placing %d corruption seeds (power: %d)", nseeds, power);

    // The corruption centreed on the player is free.
    _place_corruption_seed(you.pos(), high + 300);

    for (int i = 0; i < nseeds; ++i)
    {
        coord_def where;
        int tries = 100;
        while (tries-- > 0)
        {
            where = dgn_random_point_from(you.pos(), aux_seed_radius, 2);
            if (grd(where) == DNGN_FLOOR && !env.markers.find(where, MAT_ANY))
                break;
            where.reset();
        }

        if (!where.origin())
            _place_corruption_seed(where, random_range(low, high, 2) + 300);
    }
}

static bool _incorruptible(monster_type mt)
{
    return mons_is_abyssal_only(mt) || mons_class_holiness(mt) == MH_HOLY;
}

// Create a corruption spawn at the given position. Returns false if further
// monsters should not be placed near this spot (overcrowding), true if
// more monsters can fit in.
static bool _spawn_corrupted_servant_near(const coord_def &pos)
{
    // Chance to fail to place a monster (but allow continued attempts).
    if (x_chance_in_y(100, 200 + you.skill(SK_INVOCATIONS, 25)))
        return true;

    // Thirty tries for a place.
    for (int i = 0; i < 30; ++i)
    {
        const int offsetX = random2avg(4, 3) + random2(3);
        const int offsetY = random2avg(4, 3) + random2(3);
        const coord_def p(pos.x + random_choose(offsetX, -offsetX),
                          pos.y + random_choose(offsetY, -offsetY));
        if (!in_bounds(p) || actor_at(p))
            continue;

        monster_type mons = pick_monster(level_id(BRANCH_ABYSS), _incorruptible);
        ASSERT(mons);
        if (!monster_habitable_grid(mons, grd(p)))
            continue;
        mgen_data mg(mons, BEH_NEUTRAL, p);
        mg.set_summoned(0, 5, 0).set_non_actor_summoner("Lugonu's corruption");
        mg.place = BRANCH_ABYSS;
        return create_monster(mg);
    }

    return false;
}

static void _apply_corruption_effect(map_marker *marker, int duration)
{
    if (!duration)
        return;

    map_corruption_marker *cmark = dynamic_cast<map_corruption_marker*>(marker);
    if (cmark->duration < 1)
        return;

    const int neffects = max(div_rand_round(duration, 5), 1);

    for (int i = 0; i < neffects; ++i)
    {
        if (x_chance_in_y(cmark->duration, 4000)
            && !_spawn_corrupted_servant_near(cmark->pos))
        {
            break;
        }
    }
    cmark->duration -= duration;
}

void run_corruption_effects(int duration)
{
    for (map_marker *mark : env.markers.get_all(MAT_CORRUPTION_NEXUS))
    {
        if (mark->get_type() != MAT_CORRUPTION_NEXUS)
            continue;

        _apply_corruption_effect(mark, duration);
    }
}

static bool _is_grid_corruptible(const coord_def &c)
{
    if (c == you.pos())
        return false;

    const dungeon_feature_type feat = grd(c);

    // Stairs and portals cannot be corrupted.
    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return false;

    switch (feat)
    {
    case DNGN_PERMAROCK_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
    case DNGN_OPEN_SEA:
    case DNGN_LAVA_SEA:
        return false;

    case DNGN_METAL_WALL:
    case DNGN_CRYSTAL_WALL:
        return one_chance_in(4);

    case DNGN_STONE_WALL:
    case DNGN_CLEAR_STONE_WALL:
        return one_chance_in(3);

    case DNGN_ROCK_WALL:
    case DNGN_CLEAR_ROCK_WALL:
        return !one_chance_in(3);

    default:
        return true;
    }
}

// Returns true if the square has <= 4 traversable neighbours.
static bool _is_crowded_square(const coord_def &c)
{
    int neighbours = 0;
    for (int xi = -1; xi <= 1; ++xi)
        for (int yi = -1; yi <= 1; ++yi)
        {
            if (!xi && !yi)
                continue;

            const coord_def n(c.x + xi, c.y + yi);
            if (!in_bounds(n) || !feat_is_traversable(grd(n)))
                continue;

            if (++neighbours > 4)
                return false;
        }

    return true;
}

// Returns true if the square has all opaque neighbours.
static bool _is_sealed_square(const coord_def &c)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if (!feat_is_opaque(grd(*ai)))
            return false;

    return true;
}

static void _corrupt_square(const corrupt_env &cenv, const coord_def &c)
{
    // Ask dungeon_change_terrain to preserve things that are not altars.
    // This only actually matters for malign gateways and features that happen
    // to be on squares that happen to contain map markers (e.g. sealed doors,
    // which come with their own markers). MAT_CORRUPTION_NEXUS is a marker,
    // and some of those are certainly nearby, but they only appear on
    // DNGN_FLOOR. preserve_features=true would ordinarily cause
    // dungeon_terrain_changed to protect stairs/branch entries/portals as
    // well, but _corrupt_square is not called on squares containing those
    // features.
    bool preserve_features = true;
    dungeon_feature_type feat = DNGN_UNSEEN;
    if (feat_altar_god(grd(c)) != GOD_NO_GOD)
    {
        preserve_features = false;
        if (!one_chance_in(3))
            feat = DNGN_ALTAR_LUGONU;
    }
    else
        feat = _abyss_proto_feature();

    if (feat_is_trap(feat, true)
        || feat == DNGN_UNSEEN
        || (feat_is_traversable(grd(c)) && !feat_is_traversable(feat)
            && coinflip()))
    {
        feat = DNGN_FLOOR;
    }

    if (feat_is_traversable(grd(c)) && !feat_is_traversable(feat)
        && _is_crowded_square(c))
    {
        return;
    }

    if (!feat_is_traversable(grd(c)) && feat_is_traversable(feat)
        && _is_sealed_square(c))
    {
        return;
    }

    if (feat == DNGN_EXIT_ABYSS)
        feat = DNGN_ENTER_ABYSS;

    // If we are trying to place a wall on top of a creature or item, try to
    // move it aside. If this fails, simply place floor instead.
    actor* act = actor_at(c);
    if (feat_is_solid(feat) && (igrd(c) != NON_ITEM || act))
    {
        coord_def newpos;
        get_push_space(c, newpos, act, true);
        if (!newpos.origin())
        {
            move_items(c, newpos);
            if (act)
                actor_at(c)->move_to_pos(newpos);
        }
        else
            feat = DNGN_FLOOR;
    }

    dungeon_terrain_changed(c, feat, preserve_features, true);
    if (feat == DNGN_ROCK_WALL)
        env.grid_colours(c) = cenv.rock_colour;
    else if (feat == DNGN_FLOOR)
        env.grid_colours(c) = cenv.floor_colour;

    if (feat == DNGN_ROCK_WALL)
    {
        tileidx_t idx = tile_dngn_coloured(TILE_WALL_ABYSS,
                                           cenv.floor_colour);
        env.tile_flv(c).wall = idx + random2(tile_dngn_count(idx));
    }
    else if (feat == DNGN_FLOOR)
    {
        tileidx_t idx = tile_dngn_coloured(TILE_FLOOR_NERVES,
                                           cenv.floor_colour);
        env.tile_flv(c).floor = idx + random2(tile_dngn_count(idx));
    }
}

static void _corrupt_level_features(const corrupt_env &cenv)
{
    vector<coord_def> corrupt_seeds;

    for (const map_marker *mark : env.markers.get_all(MAT_CORRUPTION_NEXUS))
        corrupt_seeds.push_back(mark->pos);

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        int idistance = GXM + GYM;
        for (const coord_def seed : corrupt_seeds)
            idistance = min(idistance, (*ri - seed).rdist());

        const int ground_zero_radius = 2;

        // Corruption odds are 100% within 2 squares, decaying to 30%
        // at LOS range (radius 7). Even if the corruption roll is made,
        // the feature still gets a chance to resist if it's a wall.
        const int corrupt_perc_chance =
            (idistance <= ground_zero_radius) ? 100 :
            max(1, 100 - (sqr(idistance) - sqr(ground_zero_radius)) * 70 / 45);

        if (random2(100) < corrupt_perc_chance && _is_grid_corruptible(*ri))
            _corrupt_square(cenv, *ri);
    }
}

static bool _is_level_corrupted()
{
    if (player_in_branch(BRANCH_ABYSS))
        return true;

    return !!env.markers.find(MAT_CORRUPTION_NEXUS);
}

bool is_level_incorruptible(bool quiet)
{
    if (_is_level_corrupted())
    {
        if (!quiet)
            mpr("이 공간은 이미 악과 부패로 가득 차 있다.");
        return true;
    }

    return false;
}

static void _corrupt_choose_colours(corrupt_env *cenv)
{
    colour_t colour = BLACK;
    do
    {
        colour = random_uncommon_colour();
    }
    while (colour == env.rock_colour || colour == LIGHTGREY || colour == WHITE);
    cenv->rock_colour = colour;

    do
    {
        colour = random_uncommon_colour();
    }
    while (colour == env.floor_colour || colour == LIGHTGREY
           || colour == WHITE);
    cenv->floor_colour = colour;
}

bool lugonu_corrupt_level(int power)
{
    if (is_level_incorruptible())
        return false;

    simple_god_message("의 타락의 손이 뻗쳐온다!");
    take_note(Note(NOTE_MESSAGE, 0, 0, make_stringf("Corrupted %s",
              level_id::current().describe().c_str()).c_str()));
    mark_corrupted_level(level_id::current());

    flash_view(UA_PLAYER, MAGENTA);

    _initialise_level_corrupt_seeds(power);

    corrupt_env cenv;
    _corrupt_choose_colours(&cenv);
    _corrupt_level_features(cenv);
    run_corruption_effects(300);

#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    scaled_delay(1000);
#endif

    return true;
}

/// If the player has earned enough XP, spawn an exit or stairs down.
void abyss_maybe_spawn_xp_exit()
{
    if (!player_in_branch(BRANCH_ABYSS)
        || !you.props.exists(ABYSS_STAIR_XP_KEY)
        || you.props[ABYSS_STAIR_XP_KEY].get_int() > 0
        || !in_bounds(you.pos())
        || feat_is_staircase(grd(you.pos())))
    {
        return;
    }
    const bool stairs = !at_branch_bottom()
                        && you.props.exists(ABYSS_SPAWNED_XP_EXIT_KEY)
                        && you.props[ABYSS_SPAWNED_XP_EXIT_KEY].get_bool();

    destroy_wall(you.pos()); // fires listeners etc even if it wasn't a wall
    grd(you.pos()) = stairs ? DNGN_ABYSSAL_STAIR : DNGN_EXIT_ABYSS;
    big_cloud(CLOUD_TLOC_ENERGY, &you, you.pos(), 3 + random2(3), 3, 3);
    redraw_screen(); // before the force-more
    mprf(MSGCH_BANISHMENT,
         "심연의 물질계가 격렬하게 뒤틀리고, "
         " %s으로 향하는 관문이 나타났다!", stairs ? "더 깊은 곳" : "밖");

    you.props[ABYSS_STAIR_XP_KEY] = EXIT_XP_COST;
    you.props[ABYSS_SPAWNED_XP_EXIT_KEY] = true;
}
