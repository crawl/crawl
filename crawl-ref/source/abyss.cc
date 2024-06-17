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
#include "dbg-util.h"
#include "delay.h"
#include "dgn-overview.h"
#include "dgn-proclayouts.h"
#include "tile-env.h"
#include "files.h"
#include "god-companions.h" // hep stuff
#include "god-passive.h" // passive_t::slow_abyss
#include "hiscores.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
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
#include "rltiles/tiledef-dngn.h"
#include "tileview.h"
#include "timed-effects.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

const coord_def ABYSS_CENTRE(GXM / 2, GYM / 2);

static const int ABYSSAL_RUNE_MAX_ROLL = 60;

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
                                     int tries)
{
    coord_def cp = INVALID_COORD;
    for (int i = 0; i < tries; ++i)
    {
        coord_def offset;
        offset.x = random_range(-radius, radius);
        offset.y = random_range(-radius, radius);
        cp = centre + offset;

        if (cp == centre || !in_bounds(cp))
            continue;

        if (env.grid(cp) == candidate)
        {
            dprf(DIAG_ABYSS, "Placing %s at (%d,%d)",
                 dungeon_feature_name(replacement),
                 cp.x, cp.y);
            env.grid(cp) = replacement;
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
                        env.grid(p) = abyssal_features[index];
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
                    env.grid(p) = DNGN_FLOOR;
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

    return (int) pow(100.0, depth/6);
}

static void _abyss_fixup_vault(const vault_placement *vp)
{
    for (vault_place_iterator vi(*vp); vi; ++vi)
    {
        const coord_def p(*vi);
        const dungeon_feature_type feat(env.grid(p));
        if (feat_is_stair(feat)
            && feat != DNGN_EXIT_ABYSS
            && feat != DNGN_ABYSSAL_STAIR
            && !feat_is_portal_entrance(feat))
        {
            env.grid(p) = DNGN_FLOOR;
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
    const map_def *map = random_map_for_tag(tag, true, true, false);
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
    dgn_make_transporters_from_markers();
    env.markers.activate_all();
}

// returns whether the abyssal rune is at `p`, updating map knowledge as a
// side-effect
static bool _sync_rune_knowledge(coord_def p)
{
    if (!in_bounds(p))
        return false;
    // somewhat convoluted because to update map knowledge properly, we need
    // an actual rune item
    const bool already = env.map_knowledge(p).item();
    const bool rune_memory = already && env.map_knowledge(p).item()->is_type(
                                                    OBJ_RUNES, RUNE_ABYSSAL);
    for (stack_iterator si(p); si; ++si)
    {
        if (si->is_type(OBJ_RUNES, RUNE_ABYSSAL))
        {
            // found! make sure map memory is up-to-date
            if (!rune_memory)
                env.map_knowledge(p).set_item(*si, already);

            if (!you.see_cell(p))
                env.map_knowledge(p).flags |= MAP_DETECTED_ITEM;
            return true;
        }
    }
    // no rune found, clear as needed
    if (already && (!rune_memory
                    || !!(env.map_knowledge(p).flags & MAP_MORE_ITEMS)))
    {
        // something else seems to have been there, clear the rune but leave
        // a remnant
        env.map_knowledge(p).set_detected_item();
    }
    else
        env.map_knowledge(p).clear();
    return false;
}

void clear_abyssal_rune_knowledge()
{
    coord_def &cur_loc = you.props[ABYSSAL_RUNE_LOC_KEY].get_coord();
    if (in_bounds(cur_loc) && !you.runes[RUNE_ABYSSAL])
        mpr("Your memory of the abyssal rune fades away.");
    cur_loc = coord_def(-1,-1);
}

static void _update_abyssal_map_knowledge()
{
    // reset any waypoints set while in the abyss so far.
    // XX currently maprot doesn't clear waypoints, but they then don't show in
    // the map view. Maybe not an issue for programmatic users.
    travel_cache.flush_invalid_waypoints();

    // Everything else here is managing rune knowledge.

    // Don't print misleading messages about the rune disappearing
    // after you already picked it up.
    if (you.runes[RUNE_ABYSSAL])
    {
        clear_abyssal_rune_knowledge();
        return;
    }

    if (!you.props.exists(ABYSSAL_RUNE_LOC_KEY))
        you.props[ABYSSAL_RUNE_LOC_KEY].get_coord() = coord_def(-1,-1);
    coord_def &cur_loc = you.props[ABYSSAL_RUNE_LOC_KEY].get_coord();
    const bool existed = in_bounds(cur_loc);
    if (existed && _sync_rune_knowledge(cur_loc))
        return; // still exists in the same place, no need to do anything more

    // now we need to check if a new or moved rune appeared

    bool detected = false;
    cur_loc = coord_def(-1,-1);
    // check if a new one generated
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        // sets map knowledge by side effect if found
        if (_sync_rune_knowledge(*ri))
        {
            detected = true;
            cur_loc = *ri;
            break;
        }
    }

    if (existed)
    {
        mprf("The abyss shimmers again as the detected "
            "abyssal rune vanishes from your memory%s.",
            detected ? " and reappears" : "");
    }
    else if (detected)
    {
        mpr("The abyssal matter surrounding you shimmers strangely.");
        mpr("You detect the abyssal rune!");
    }

    // XX could do the xom check from here?
}

static bool _abyss_place_rune_vault(const map_bitmask &abyss_genlevel_mask)
{
    bool result = false;
    int tries = 10;
    do
    {
        result = _abyss_place_vault_tagged(abyss_genlevel_mask, "abyss_rune");
    }
    while (!result && --tries);

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
            && env.grid(p) == DNGN_FLOOR && env.igrid(p) == NON_ITEM
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
            item_colour(env.item[item_ind]);
        move_item_to_grid(&item_ind, chosen_spot);
        return item_ind != NON_ITEM;
    }

    return false;
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
    const int mindepth = min(maxdepth, (4 * power + 7) / 23);
    const int bottom = brdepth[BRANCH_ABYSS];
    return min(bottom, max(1, random_range(mindepth, maxdepth)));
}

void banished(const string &who, const int power)
{
    ASSERT(!crawl_state.game_is_arena());
    if (brdepth[BRANCH_ABYSS] == -1)
        return;

    if (player_in_branch(BRANCH_ABYSS))
    {
        if (level_id::current().depth < brdepth[BRANCH_ABYSS])
            down_stairs(DNGN_ABYSSAL_STAIR);
        else
        {
            // On Abyss:$ we can't go deeper; cause a shift to a new area
            mprf(MSGCH_BANISHMENT, "You are banished to a different region of the Abyss.");
            abyss_teleport();
        }
        return;
    }

    const int depth = _banished_depth(power);

    stop_delay(true);
    splash_corruption(you.pos());
    run_animation(ANIMATION_BANISH, UA_BRANCH_ENTRY, false);
    push_features_to_abyss();
    floor_transition(DNGN_ENTER_ABYSS, orig_terrain(you.pos()),
                     level_id(BRANCH_ABYSS, depth), true);
    // This is an honest abyss entry, mark milestone and take note
    // floor_transition might change our final destination in the abyss
    const string what = make_stringf("Cast into level %d of the Abyss",
                                     you.depth) + _who_banished(who);
    take_note(Note(NOTE_MESSAGE, 0, 0, what), true);
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

            dungeon_feature_type feature = map_bounds(p) ? env.grid(p) : DNGN_UNSEEN;
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
        wall_count += cell_is_solid(*ai);
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
            env.grid(p) = which_feat;

        if (feats_wanted)
            --*feats_wanted;
        return true;
    }
    return false;
}

static dungeon_feature_type _abyss_pick_altar()
{
    // Lugonu has a flat 90% chance of corrupting the altar.
    if (!one_chance_in(10))
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
            if (env.grid(*ri) == DNGN_EXIT_ABYSS && env.map_knowledge(*ri).seen())
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
        update_screen();

        const bool exit_is_near = abyss_exit_nearness();
        const bool rune_is_near = abyss_rune_nearness();

        if (exit_was_near && !exit_is_near || rune_was_near && !rune_is_near)
            xom_is_stimulated(200, "Xom snickers loudly.", true);

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
        simple_monster_message(mons, " is pulled into the Abyss.",
                MSGCH_BANISHMENT);
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
            remove_sanctuary();
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
            // hep messaging is done in _abyss_lose_monster
            if (you.can_see(*mon) && hepliaklqana_ancestor() != mon->mid)
            {
                simple_monster_message(*mon, " is pulled into the Abyss.",
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
        item_def& item(env.item[i]);
        if (!item.defined() || !in_bounds(item.pos) || item.held_by_monster())
            continue;

        if (env.item[i].flags & ISFLAG_SUMMONED)
        {
            // this is here because of hep-related crashes that no one has
            // figured out. Under some circumstances, a hep ancestor can drop
            // its items in the abyss in a stack of copies(??), and when they
            // do, on the next abyss morph the call below to move_item_to_grid
            // will trigger a crash, softlocking the player out of abyss.
            // Therefore, preemptively clean things up until someone figures
            // out this bug. See
            // https://crawl.develz.org/mantis/view.php?id=11756 among others.
            debug_dump_item(item.name(DESC_PLAIN).c_str(),
                i, item, "Cleaning up buggy summoned item!");
            destroy_item(i);
            continue;
        }
        if (!_pushy_feature(env.grid(item.pos)))
            continue;

        for (distance_iterator di(item.pos); di; ++di)
            if (!_pushy_feature(env.grid(*di)))
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

    env.grid(p) = DNGN_UNSEEN;

    // Nuke items.
    if (env.igrid(p) != NON_ITEM)
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
    tile_env.bk_fg(p)   = 0;
    tile_env.bk_bg(p)   = 0;
    tile_env.bk_cloud(p)= 0;
#endif
    tile_clear_flavour(p);
    tile_init_flavour(p);

    env.level_map_mask(p) = 0;
    env.level_map_ids(p)  = INVALID_MAP_INDEX;

    remove_markers_and_listeners_at(p);

    env.map_knowledge(p).clear();
    if (env.map_forgotten)
        (*env.map_forgotten)(p).clear();
    env.map_seen.set(p, false);
#ifdef USE_TILE
    tile_forget_map(p);
#endif
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
    if (env.grid(pos) != DNGN_TRANSPORTER)
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
                // Wipe the destination clean before dropping things on it.
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
    // and dpeg on IRC confirm that this (repeated swatch of terrain left
    // behind) was not intentional.
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
    int j = count_if(begin(env.item), end(env.item), mem_fn(&item_def::defined));

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
        if (it->id == BRANCH_VESTIBULE || it->id == BRANCH_ABYSS
            || it->id == BRANCH_SHOALS)
        {
            continue;
        }
        for (int j = 1; j <= brdepth[it->id]; ++j)
        {
            const level_id id(it->id, j);
            // for pregen, this will use levels the player hasn't seen yet
            if (is_existing_level(id))
                levels.push_back(id);
        }
    }
    if (levels.empty())
    {
        // Let this fail later on.
        return level_id(static_cast<branch_type>(BRANCH_DUNGEON), 1);
    }

    return levels[hash_with_seed(levels.size(), abyssal_state.seed)];
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
        if (is_existing_level(lid))
        {
            auto &vault_list =  you.vault_list[level_id::current()];
            vault_list.push_back("base: " + lid.describe(false));
        }
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
        case DNGN_OPEN_DOOR:
        case DNGN_BROKEN_DOOR:
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
        case DNGN_CLOSED_CLEAR_DOOR:
        case DNGN_OPEN_CLEAR_DOOR:
        case DNGN_BROKEN_CLEAR_DOOR:
            return CLOUD_MIST;
        case DNGN_ORCISH_IDOL:
        case DNGN_METAL_STATUE:
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

    const dungeon_feature_type currfeat = env.grid(rp);

    // Don't decay vaults.
    if (map_masked(rp, MMT_VAULT))
        return;

    switch (currfeat)
    {
        case DNGN_RUNELIGHT:
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
    if (you.pos() == rp && env.mgrid(rp) != NON_MONSTER)
        feat = currfeat;

    // If the selected grid is already there, *or* if we're morphing and
    // the selected grid should have been there, do nothing.
    if (feat != currfeat)
    {
        env.grid(rp) = feat;
        if (feat == DNGN_FLOOR && in_los_bounds_g(rp))
        {
            cloud_type cloud = _cloud_from_feat(currfeat);
            int cloud_life = _in_wastes(abyssal_state.major_coord) ? 5 : 2;
            cloud_life += random2(2); // force a sequence point, just in case
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
    dungeon_feature_type feat = env.grid(you.pos());
    if (!you.can_pass_through_feat(feat) || is_feat_dangerous(feat))
    {
        bool shoved = you.shove();
        if (!shoved)
        {
            // legal only if we just placed a vault
            ASSERT(dig_instead);
            env.grid(you.pos()) = DNGN_FLOOR;
        }
    }
}

static void _abyss_apply_terrain(const map_bitmask &abyss_genlevel_mask,
                                 bool morph = false, bool now = false)
{
    // The chance is reciprocal to these numbers.
    const int depth = you.runes[RUNE_ABYSSAL] ? 5 : you.depth - 2;
    // Cap the chance at the old max depth (5).
    const int exit_chance = 7250 - 1250 * min(depth, 5);

    int exits_wanted  = 0;
    int altars_wanted = 0;
    bool use_abyss_exit_map = true;
    bool used_queue = false;
    if (morph && !abyss_sample_queue.empty())
    {
        used_queue = true;
        while (!abyss_sample_queue.empty()
            && abyss_sample_queue.top().changepoint() < abyssal_state.depth)
        {
            coord_def p = abyss_sample_queue.top().coord();
            _update_abyss_terrain(p, abyss_genlevel_mask, morph);
            abyss_sample_queue.pop();
        }
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
        _abyss_check_place_feat(p, 3000,
                                &altars_wanted,
                                nullptr,
                                _abyss_pick_altar(),
                                abyss_genlevel_mask)
        ||
        level_id::current().depth < brdepth[BRANCH_ABYSS]
        && _abyss_check_place_feat(p, 1750, nullptr, nullptr,
                                   DNGN_ABYSSAL_STAIR,
                                   abyss_genlevel_mask);
    }
    if (ii)
        dprf(DIAG_ABYSS, "Nuked %d features", ii);
    _ensure_player_habitable(false);
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        ASSERT_RANGE(env.grid(*ri), DNGN_UNSEEN + 1, NUM_FEATURES);
}

static int _abyss_place_vaults(const map_bitmask &abyss_genlevel_mask, bool placed_abyssal_rune)
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
            if (_abyss_place_map(map) && !map->is_extra_vault())
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

    const int abyssal_rune_roll = _abyssal_rune_roll();
    if (!placed_abyssal_rune && abyssal_rune_roll != -1
        && x_chance_in_y(abyssal_rune_roll, ABYSSAL_RUNE_MAX_ROLL))
    {
        if (_abyss_place_rune(abyss_genlevel_mask))
            ++vaults_placed;
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
    _abyss_place_vaults(abyss_genlevel_mask, placed_abyssal_rune);
    // Link the vault-placed items.
    _abyss_postvault_fixup();

    setup_environment_effects();

    _ensure_player_habitable(true);

    _update_abyssal_map_knowledge();

    // Abyss has a constant density.
    env.density = 0;
}

static void _initialize_abyss_state()
{
    abyssal_state.major_coord.x = rng::get_uint32() & 0x7FFFFFFF;
    abyssal_state.major_coord.y = rng::get_uint32() & 0x7FFFFFFF;
    abyssal_state.seed = rng::get_uint32() & 0x7FFFFFFF;
    abyssal_state.phase = 0.0;
    abyssal_state.depth = rng::get_uint32() & 0x7FFFFFFF;
    abyssal_state.destroy_all_terrain = false;
    abyssal_state.level = _get_random_level();
    abyss_sample_queue = sample_queue(ProceduralSamplePQCompare());
}

void set_abyss_state(coord_def coord, uint32_t depth)
{
    abyssal_state.major_coord = coord;
    abyssal_state.depth = depth;
    abyssal_state.seed = rng::get_uint32() & 0x7FFFFFFF;
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

    auto &vault_list =  you.vault_list[level_id::current()];
#ifdef DEBUG
    vault_list.push_back("[shift]");
#endif
    const auto &level_vaults = level_vault_names();
    vault_list.insert(vault_list.end(),
                        level_vaults.begin(), level_vaults.end());


    check_map_validity();
    // TODO: should dactions be rerun at this point instead? That would cover
    // this particular case...
    gozag_move_level_gold_to_top();
    _update_abyssal_map_knowledge();
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
    remove_sanctuary();

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

// Generate the initial (proto) Abyss level. The proto Abyss is where
// the player lands when they arrive in the Abyss from elsewhere.
// _generate_area generates all other Abyss areas.
void generate_abyss()
{
    env.level_build_method += " abyss";
    env.level_layout_types.insert("abyss");
    destroy_abyss();

    _initialize_abyss_state();

    dprf(DIAG_ABYSS, "generate_abyss(); turn_on_level: %d",
         env.turns_on_level);

    // Generate the initial abyss without vaults. Vaults are horrifying.
    _abyss_generate_new_area();
    _write_abyssal_features();
    map_bitmask abyss_genlevel_mask(true);
    _abyss_apply_terrain(abyss_genlevel_mask);

    env.grid(you.pos()) = _veto_dangerous_terrain(env.grid(you.pos()));
    _place_displaced_monsters();

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        ASSERT(env.grid(*ri) > DNGN_UNSEEN);
    check_map_validity();

    // Start out on floor, and there's a chance there's an  altar nearby.
    env.grid(ABYSS_CENTRE) = DNGN_FLOOR;
    if (one_chance_in(5))
    {
        _place_feature_near(ABYSS_CENTRE, LOS_RADIUS,
                            DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50);
    }

    setup_environment_effects();
    _update_abyssal_map_knowledge();
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
    // TODO: does gozag gold detection need to be here too?
    los_changed();
}

// Force the player one level deeper in the abyss during an abyss teleport with
// probability:
//   (max(0, XL - Depth))^2 / 27^2 * 3
//
// Consequences of this formula:
// - Chance to be pulled deeper increases with XL and decreases with current
//   abyss depth.
// - Characters won't get pulled to a depth higher than their XL
// - Characters at XL 13 have chances for getting pulled from A:1/A:2/A:3/A:4
//   of about 6.6%/5.5%/4.5%/3.7%.
// - Characters at XL 27 have chances for getting pulled from A:1/A:2/A:3/A:4
//   of about 31%/28.5%/26.3%/24.1%.
static bool _abyss_force_descent()
{
    const int depth = level_id::current().depth;
    const int xl_factor = max(0, you.experience_level - depth);
    return x_chance_in_y(xl_factor * xl_factor, 729 * 3);
}

void abyss_teleport(bool wizard_tele)
{
    xom_abyss_feature_amusement_check xomcheck;
    dprf(DIAG_ABYSS, "New area Abyss teleport.");

    if (level_id::current().depth < brdepth[BRANCH_ABYSS]
        && _abyss_force_descent() && !wizard_tele)
    {
        down_stairs(DNGN_ABYSSAL_STAIR);
        more();
        return;
    }

    mprf(MSGCH_BANISHMENT, "You are suddenly pulled into a different region of"
        " the Abyss!");
    _abyss_generate_new_area();
    _write_abyssal_features();
    env.grid(you.pos()) = _veto_dangerous_terrain(env.grid(you.pos()));

    stop_delay(true);
    forget_map(false);
    clear_excludes();
    gozag_move_level_gold_to_top();
    auto &vault_list =  you.vault_list[level_id::current()];
#ifdef DEBUG
    vault_list.push_back("[tele]");
#endif
    const auto &level_vaults = level_vault_names();
    vault_list.insert(vault_list.end(),
                        level_vaults.begin(), level_vaults.end());

    _update_abyssal_map_knowledge();
    more();
}

//////////////////////////////////////////////////////////////////////////////
// Abyss effects in other levels, courtesy Lugonu.

struct corrupt_env
{
    int rock_colour, floor_colour, secondary_floor_colour;
    corrupt_env()
        : rock_colour(BLACK), floor_colour(BLACK),
           secondary_floor_colour(MAGENTA)
    { }

    int pick_floor_colour() const
    {
        return random2(10) < 2 ? secondary_floor_colour : floor_colour;
    }
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

    // The corruption seed centred on the player is guaranteed.
    _place_corruption_seed(you.pos(), high + 300);

    // Other corruption seeds are not guaranteed
    for (int i = 0; i < nseeds; ++i)
    {
        coord_def where;
        int tries = 100;
        while (tries-- > 0)
        {
            where = dgn_random_point_from(you.pos(), aux_seed_radius, 2);
            if (env.grid(where) == DNGN_FLOOR && !env.markers.find(where, MAT_ANY))
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
        int offsetX = random2avg(4, 3);
        offsetX += random2(3); // force a sequence point between random calls
        int offsetY = random2avg(4, 3);
        offsetY += random2(3); // ditto
        coord_def p;
        p.x = pos.x + random_choose(offsetX, -offsetX);
        p.y = pos.y + random_choose(offsetY, -offsetY);
        if (!in_bounds(p) || actor_at(p))
            continue;

        monster_type mons = pick_monster(level_id(BRANCH_ABYSS), _incorruptible);
        ASSERT(mons);
        if (!monster_habitable_grid(mons, env.grid(p)))
            continue;
        mgen_data mg(mons, BEH_NEUTRAL, p);
        mg.set_summoned(0, 5, 0).set_non_actor_summoner("Lugonu's corruption");
        mg.place = BRANCH_ABYSS;
        return create_monster(mg);
    }

    return false;
}

static void _spawn_corrupted_servant_near_monster(const monster &who)
{
    const coord_def pos = who.pos();

    // Thirty tries for a place.
    for (int i = 0; i < 30; ++i)
    {
        coord_def p;
        p.x = pos.x + random_choose(3, -3);
        p.y = pos.y + random_choose(3, -3);
        if (!in_bounds(p) || actor_at(p) || !cell_see_cell(pos, p, LOS_SOLID))
            continue;
        monster_type mons = pick_monster(level_id(BRANCH_ABYSS), _incorruptible);
        ASSERT(mons);
        if (!monster_habitable_grid(mons, env.grid(p)))
            continue;
        mgen_data mg(mons, BEH_COPY, p);
        mg.set_summoned(&who, 3, 0);
        mg.place = BRANCH_ABYSS;
        if (create_monster(mg))
            return;
    }
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

    const dungeon_feature_type feat = env.grid(c);

    // Stairs and portals cannot be corrupted.
    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return false;

    switch (feat)
    {
    case DNGN_PERMAROCK_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
    case DNGN_OPEN_SEA:
    case DNGN_LAVA_SEA:
    case DNGN_TRANSPORTER_LANDING: // entry already taken care of as stairs
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
            if (!in_bounds(n) || !feat_is_traversable(env.grid(n)))
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
        if (!feat_is_opaque(env.grid(*ai)))
            return false;

    return true;
}

static void _recolour_wall(coord_def c, tileidx_t tile)
{
    tile_env.flv(c).wall_idx = store_tilename_get_index(tile_dngn_name(tile));
}

static void _corrupt_square_flavor(const corrupt_env &cenv, const coord_def &c)
{
    dungeon_feature_type feat = env.grid(c);
    int floor = cenv.pick_floor_colour();

    if (feat == DNGN_ROCK_WALL || feat == DNGN_METAL_WALL
        || feat == DNGN_STONE_WALL)
    {
        env.grid_colours(c) = cenv.rock_colour;
    }
    else if (feat == DNGN_FLOOR)
        env.grid_colours(c) = floor;

    // if you add new features to this, you'll probably need to do some
    // hand-tweaking in tileview.cc apply_variations.
    // TODO: these tile assignments here seem to get overridden in
    // apply_variations, or not used at all...what gives?
    if (feat == DNGN_ROCK_WALL)
    {
        tileidx_t idx = tile_dngn_coloured(TILE_WALL_ABYSS,
                                           cenv.rock_colour);
        tile_env.flv(c).wall = idx + random2(tile_dngn_count(idx));
        _recolour_wall(c, idx);
    }
    else if (feat == DNGN_FLOOR)
    {
        tileidx_t idx = tile_dngn_coloured(TILE_FLOOR_NERVES,
                                           floor);
        tile_env.flv(c).floor = idx + random2(tile_dngn_count(idx));
        const string name = tile_dngn_name(idx);
        tile_env.flv(c).floor_idx = store_tilename_get_index(name);
    }
    else if (feat == DNGN_STONE_WALL)
    {
        // recoloring stone and metal is also impacted heavily by the rolls
        // in _is_grid_corruptible
        tileidx_t idx = tile_dngn_coloured(TILE_DNGN_STONE_WALL,
                                           cenv.rock_colour);
        tile_env.flv(c).wall = idx + random2(tile_dngn_count(idx));
        _recolour_wall(c, idx);
    }
    else if (feat == DNGN_METAL_WALL)
    {
        tileidx_t idx = tile_dngn_coloured(TILE_DNGN_METAL_WALL,
                                           cenv.rock_colour);
        tile_env.flv(c).wall = idx + random2(tile_dngn_count(idx));
        _recolour_wall(c, idx);
    }
    else if (feat_is_tree(feat))
    {
        // This is technically not flavour because of how these burn but ok
        dungeon_terrain_changed(c, DNGN_DEMONIC_TREE, false, false);
    }
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
    if (feat_altar_god(env.grid(c)) != GOD_NO_GOD)
    {
        preserve_features = false;
        if (!one_chance_in(3))
            feat = DNGN_ALTAR_LUGONU;
    }
    else
        feat = _abyss_proto_feature();

    if (feat_is_trap(feat)
        || feat == DNGN_UNSEEN
        || (feat_is_traversable(env.grid(c)) && !feat_is_traversable(feat)
            && coinflip()))
    {
        feat = DNGN_FLOOR;
    }

    if (feat_is_traversable(env.grid(c)) && !feat_is_traversable(feat)
        && _is_crowded_square(c))
    {
        return;
    }

    if (!feat_is_traversable(env.grid(c)) && feat_is_traversable(feat)
        && _is_sealed_square(c))
    {
        return;
    }

    if (feat == DNGN_EXIT_ABYSS)
        feat = DNGN_ENTER_ABYSS;

    // If we are trying to place a wall on top of a creature or item, try to
    // move it aside. If this fails, simply place floor instead.
    actor* act = actor_at(c);
    if (feat_is_solid(feat) && (env.igrid(c) != NON_ITEM || act))
    {
        push_items_from(c, nullptr);
        push_actor_from(c, nullptr, true);
        if (actor_at(c) || env.igrid(c) != NON_ITEM)
            feat = DNGN_FLOOR;
    }

    dungeon_terrain_changed(c, feat, preserve_features, true);
    _corrupt_square_flavor(cenv, c);
}

static void _corrupt_square_monster(const corrupt_env &cenv, const coord_def &c)
{
    // See comment in _corrupt_square for details on this block. Note that
    // this function differs in that the changes to terrain are temporary
    dungeon_feature_type feat = DNGN_UNSEEN;
    feat = _abyss_proto_feature();

    if (feat_is_trap(feat)
        || feat == DNGN_UNSEEN
        || (feat_is_traversable(env.grid(c)) && !feat_is_traversable(feat)
            && coinflip()))
    {
        feat = DNGN_FLOOR;
    }

    if (feat_is_traversable(env.grid(c)) && !feat_is_traversable(feat)
        && _is_crowded_square(c))
    {
        return;
    }

    if (!feat_is_traversable(env.grid(c)) && feat_is_traversable(feat)
        && _is_sealed_square(c))
    {
        return;
    }

    // If we are trying to place a wall on top of a creature or item, try to
    // move it aside. If this fails, simply place floor instead.
    actor* act = actor_at(c);
    if (feat_is_solid(feat) && (env.igrid(c) != NON_ITEM || act))
    {
        push_items_from(c, nullptr);
        push_actor_from(c, nullptr, true);
        if (actor_at(c) || env.igrid(c) != NON_ITEM)
            feat = DNGN_FLOOR;
    }
    temp_change_terrain(c, feat, random_range(60, 120),
                                    TERRAIN_CHANGE_GENERIC);
    _corrupt_square_flavor(cenv, c);
}

static void _corrupt_level_features(const corrupt_env &cenv)
{
    vector<coord_def> corrupt_seeds;

    for (const map_marker *mark : env.markers.get_all(MAT_CORRUPTION_NEXUS))
        corrupt_seeds.push_back(mark->pos);

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        int idistance = GXM + GYM;
        for (const coord_def &seed : corrupt_seeds)
            idistance = min(idistance, (*ri - seed).rdist());

        const int ground_zero_radius = 2;

        // Corruption odds are 100% within 2 squares, decaying to 30%
        // at LOS range (radius 7). Even if the corruption roll is made,
        // the feature still gets a chance to resist if it's a wall.
        const int corrupt_perc_chance =
            (idistance <= ground_zero_radius) ? 1000 :
            max(0, 1000 - (sqr(idistance) - sqr(ground_zero_radius)) * 700 / 45);

        // linear function that is at 30% at range 7, 15% at radius 20,
        // maxed to 1%. Only affects outside of range 7. For cells within los
        // that don't make the regular roll, this also gives them a 50%
        // chance to get flavor-only corruption.
        const int corrupt_flavor_chance =
            (idistance <= 7) ? (corrupt_perc_chance + 1000) / 2 :
            max(10, 380 - 150 * idistance / 13);

        const int roll = random2(1000);

        if (roll < corrupt_perc_chance && _is_grid_corruptible(*ri))
            _corrupt_square(cenv, *ri);
        else if (roll < corrupt_flavor_chance && _is_grid_corruptible(*ri))
            _corrupt_square_flavor(cenv, *ri);
    }
}

static void _corrupt_level_features_monster(const corrupt_env &cenv, monster mons)
{
    // the only "corruption seed" in the monster version is the casting monster
    for (radius_iterator ri(mons.pos(), LOS_DEFAULT); ri; ++ri)
    {
        const int idistance = (*ri - mons.pos()).rdist();

        const int ground_zero_radius = 2;

        // Corruption odds are 100% within 2 squares, decaying to 30%
        // at LOS range (radius 7).
        const int corrupt_perc_chance =
            (idistance <= ground_zero_radius) ? 1000 :
            max(0, 1000 - (sqr(idistance) - sqr(ground_zero_radius)) * 700 / 45);

        // Monster version: don't affect outside of range 7.
        // For cells within los that don't make the regular roll, this also
        // gives them a 50% chance to get flavor-only corruption.
        const int corrupt_flavor_chance = (corrupt_perc_chance + 1000) / 2;

        const int roll = random2(3000);

        // In the monster version of the effect we have an extra check here
        // which will prevent the effect from triggering _corrupt_square
        // on anything other than clear dungeon floor.
        // This stops the corruption from breaking into vaults, opening
        // temporary teleport closets for the player, and other unwanted
        // outcomes.
        if (env.grid(*ri) == DNGN_FLOOR)
        {
            if (roll < corrupt_perc_chance && _is_grid_corruptible(*ri))
                _corrupt_square_monster(cenv, *ri);
            else if (roll < corrupt_flavor_chance && _is_grid_corruptible(*ri))
                _corrupt_square_flavor(cenv, *ri);
        }
        else
        {
            // chance to change the colour of any grid
            if (roll < corrupt_flavor_chance && _is_grid_corruptible(*ri))
                _corrupt_square_flavor(cenv, *ri);
        }
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
            mpr("This place is already infused with evil and corruption.");
        return true;
    }

    return false;
}

bool is_level_incorruptible_monster()
{
    if (player_in_branch(BRANCH_ABYSS))
        return true;
    else
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

void lugonu_corrupt_level(int power)
{
    if (is_level_incorruptible())
        return;

    simple_god_message("'s Hand of Corruption reaches out!");
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
}

void lugonu_corrupt_level_monster(const monster &who)
{
    if (is_level_incorruptible_monster())
        return;

    flash_view_delay(UA_MONSTER, MAGENTA, 200);

    corrupt_env cenv;
    _corrupt_choose_colours(&cenv);
    _corrupt_level_features_monster(cenv, who);

    // Monster version does not use a timed effect to handle monster summons.
    // This simplifies the effect and allows for the summons to be abjured once
    // the monster is killed, as normal
    const int count = random2(3) + 1;
    for (int i = 0; i < count; ++i)
        _spawn_corrupted_servant_near_monster(who);

#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    scaled_delay(300);
#endif
}

/// Splash decorative corruption around the given space.
void splash_corruption(coord_def centre)
{
    corrupt_env cenv;
    _corrupt_choose_colours(&cenv);
    _corrupt_square_flavor(cenv, centre);
    for (adjacent_iterator ai(centre); ai; ++ai)
        if (in_bounds(*ai) && coinflip())
            _corrupt_square_flavor(cenv, *ai);
}

static void _cleanup_temp_terrain_at(coord_def pos)
{
    for (map_marker *mark : env.markers.get_all(MAT_TERRAIN_CHANGE))
    {
        map_terrain_change_marker *marker =
                dynamic_cast<map_terrain_change_marker*>(mark);
        if (mark->pos == pos)
            revert_terrain_change(pos, marker->change_type);
    }
}

/// If the player has earned enough XP, spawn an exit or stairs down.
void abyss_maybe_spawn_xp_exit()
{
    if (!player_in_branch(BRANCH_ABYSS)
        || !you.props.exists(ABYSS_STAIR_XP_KEY)
        || you.props[ABYSS_STAIR_XP_KEY].get_int() > 0
        || !in_bounds(you.pos())
        || feat_is_critical(env.grid(you.pos())))
    {
        return;
    }
    const bool stairs = !at_branch_bottom()
                        && you.props.exists(ABYSS_SPAWNED_XP_EXIT_KEY)
                        && coinflip()
                        && you.props[ABYSS_SPAWNED_XP_EXIT_KEY].get_bool();

    _cleanup_temp_terrain_at(you.pos());
    destroy_wall(you.pos()); // fires listeners etc even if it wasn't a wall
    env.grid(you.pos()) = stairs ? DNGN_ABYSSAL_STAIR : DNGN_EXIT_ABYSS;
    big_cloud(CLOUD_TLOC_ENERGY, &you, you.pos(), 3 + random2(3), 3, 3);
    redraw_screen(); // before the force-more
    update_screen();
    mprf(MSGCH_BANISHMENT,
         "The substance of the Abyss twists violently,"
         " and a gateway leading %s appears!", stairs ? "down" : "out");

    you.props[ABYSS_STAIR_XP_KEY] = EXIT_XP_COST;
    you.props[ABYSS_SPAWNED_XP_EXIT_KEY] = true;
    interrupt_activity(activity_interrupt::abyss_exit_spawned);
}
