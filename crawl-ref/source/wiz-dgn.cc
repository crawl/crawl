/**
 * @file
 * @brief Dungeon related wizard functions.
**/

#include "AppHdr.h"

#include "wiz-dgn.h"

#include "act-iter.h"
#include "branch.h"
#include "coordit.h"
#include "dactions.h"
#include "delay.h"
#include "describe.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "files.h"
#include "items.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "place.h"
#include "prompt.h"
#include "religion.h"
#include "spl-goditem.h" // detect_items
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "tileview.h"
#include "tiles-build-specific.h"
#include "traps.h"
#include "view.h"
#include "wiz-mon.h"

#ifdef WIZARD
static dungeon_feature_type _find_appropriate_stairs(bool down)
{
    if (player_in_branch(BRANCH_PANDEMONIUM))
    {
        if (down)
            return DNGN_TRANSIT_PANDEMONIUM;
        else
            return DNGN_EXIT_PANDEMONIUM;
    }

    int depth = you.depth;
    if (down)
        depth++;
    else
        depth--;

    // Can't go down from bottom level of a branch.
    if (depth > brdepth[you.where_are_you])
    {
        mpr("Can't go down from the bottom of a branch.");
        return DNGN_UNSEEN;
    }
    // Going up from top level of branch
    else if (depth == 0)
        return your_branch().exit_stairs;
    // Branch non-edge cases
    else if (depth >= 1)
    {
        if (down)
            return DNGN_STONE_STAIRS_DOWN_I;
        else
            return DNGN_ESCAPE_HATCH_UP;
    }
    else
    {
        mpr("Bug in determining level exit.");
        return DNGN_UNSEEN;
    }
}

void wizard_place_stairs(bool down)
{
    dungeon_feature_type stairs = _find_appropriate_stairs(down);

    if (stairs == DNGN_UNSEEN)
        return;

    mprf("Creating %sstairs.", down ? "down" : "up");
    dungeon_terrain_changed(you.pos(), stairs);
}

void wizard_level_travel(bool down)
{
    dungeon_feature_type stairs = _find_appropriate_stairs(down);

    if (stairs == DNGN_UNSEEN)
        return;

    // This lets us, for example, use &U to exit from Pandemonium and
    // &D to go to the next level.
    command_type real_dir = feat_stair_direction(stairs);
    if (down && real_dir == CMD_GO_UPSTAIRS
        || !down && real_dir == CMD_GO_DOWNSTAIRS)
    {
        down = !down;
    }

    if (down)
        down_stairs(stairs, false, false);
    else
        up_stairs(stairs, false);
}

static void _wizard_go_to_level(const level_pos &pos)
{
    dungeon_feature_type stair_taken =
        absdungeon_depth(pos.id.branch, pos.id.depth) > env.absdepth0 ?
        DNGN_STONE_STAIRS_DOWN_I : DNGN_STONE_STAIRS_UP_I;

    if (pos.id.depth == brdepth[pos.id.branch])
        stair_taken = DNGN_STONE_STAIRS_DOWN_I;

    if (!player_in_branch(pos.id.branch) && pos.id.depth == 1
        && pos.id.branch != BRANCH_DUNGEON)
    {
        stair_taken = branches[pos.id.branch].entry_stairs;
    }

    if (is_connected_branch(pos.id.branch))
        you.level_stack.clear();
    else
    {
        for (int i = you.level_stack.size() - 1; i >= 0; i--)
            if (you.level_stack[i].id == pos.id)
                you.level_stack.resize(i);
        if (!player_in_branch(pos.id.branch))
            you.level_stack.push_back(level_pos::current());
    }

    const level_id old_level = level_id::current();

    you.where_are_you = static_cast<branch_type>(pos.id.branch);
    you.depth         = pos.id.depth;

    leaving_level_now(stair_taken);
    const bool newlevel = load_level(stair_taken, LOAD_ENTER_LEVEL, old_level);
    tile_new_level(newlevel);
    if (!crawl_state.test)
        save_game_state();
    new_level();
    seen_monsters_react();
    viewwindow();

    // Tell stash-tracker and travel that we've changed levels.
    trackers_init_new_level(true);
}

void wizard_interlevel_travel()
{
    string name;
    const level_pos pos =
        prompt_translevel_target(TPF_ALLOW_UPDOWN | TPF_SHOW_ALL_BRANCHES, name);

    if (pos.id.depth < 1 || pos.id.depth > brdepth[pos.id.branch])
    {
        canned_msg(MSG_OK);
        return;
    }

    _wizard_go_to_level(pos);
}

bool wizard_create_feature(const coord_def& pos)
{
    const bool mimic = (pos != you.pos());
    char specs[256];
    dungeon_feature_type feat;
    if (mimic)
        mprf(MSGCH_PROMPT, "Create what kind of feature mimic? ");
    else
        mprf(MSGCH_PROMPT, "Create which feature? ");

    if (cancellable_get_line_autohist(specs, sizeof(specs)) || specs[0] == 0)
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (int feat_num = atoi(specs))
        feat = static_cast<dungeon_feature_type>(feat_num);
    else
    {
        string name = lowercase_string(specs);
        name = replace_all(name, " ", "_");
        feat = dungeon_feature_by_name(name);
        if (feat == DNGN_UNSEEN) // no exact match
        {
            vector<string> matches = dungeon_feature_matches(name);

            if (matches.empty())
            {
                const feature_property_type fprop(str_to_fprop(name));
                if (fprop != FPROP_NONE)
                {
                    env.pgrid(you.pos()) |= fprop;
                    mprf("Set fprops \"%s\" at (%d,%d)",
                         name.c_str(), you.pos().x, you.pos().y);
                }
                else
                {
                    mprf(MSGCH_DIAGNOSTICS, "No features matching '%s'",
                         name.c_str());
                }
                return false;
            }

            // Only one possible match, use that.
            if (matches.size() == 1)
            {
                name = matches[0];
                feat = dungeon_feature_by_name(name);
            }
            // Multiple matches, list them to wizard
            else
            {
                string prefix = "No exact match for feature '" +
                                name +  "', possible matches are: ";

                // Use mpr_comma_separated_list() because the list
                // might be *LONG*.
                mpr_comma_separated_list(prefix, matches, " and ", ", ",
                                         MSGCH_DIAGNOSTICS);
                return wizard_create_feature(pos);
            }
        }
    }

    if (mimic && !feat_is_mimicable(feat, false)
        && !yesno("This isn't a valid feature mimic. Create it anyway? ",
                  true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (feat == DNGN_ENTER_SHOP)
        return debug_make_shop(pos);

    if (feat_is_trap(feat, true))
        return debug_make_trap(pos);

    env.tile_flv(pos).feat = 0;
    env.tile_flv(pos).special = 0;
    env.grid_colours(pos) = 0;
    const dungeon_feature_type old_feat = grd(pos);
    dungeon_terrain_changed(pos, feat, false, false, false, true);
    // Update gate tiles, if existing.
    if (feat_is_door(old_feat) || feat_is_door(feat))
    {
        const coord_def left  = pos - coord_def(1, 0);
        const coord_def right = pos + coord_def(1, 0);
        if (map_bounds(left) && feat_is_door(grd(left)))
            tile_init_flavour(left);
        if (map_bounds(right) && feat_is_door(grd(right)))
            tile_init_flavour(right);
    }
    if (pos == you.pos() && cell_is_solid(pos))
        you.wizmode_teleported_into_rock = true;

    if (mimic)
        env.level_map_mask(pos) |= MMT_MIMIC;

    if (you.see_cell(pos))
        view_update_at(pos);

    return true;
}

void wizard_list_branches()
{
    for (branch_iterator it; it; ++it)
    {
        if (parent_branch(it->id) == NUM_BRANCHES)
            continue;
        else if (brentry[it->id].is_valid())
        {
            mprf(MSGCH_DIAGNOSTICS, "Branch %d (%s) is on %s",
                 it->id, it->longname, brentry[it->id].describe().c_str());
        }
        else if (is_random_subbranch(it->id))
        {
            mprf(MSGCH_DIAGNOSTICS, "Branch %d (%s) was not generated "
                 "this game", it->id, it->longname);
        }
    }

    if (!you.props.exists(OVERFLOW_TEMPLES_KEY))
        return;

    mprf(MSGCH_DIAGNOSTICS, "----");
    mprf(MSGCH_DIAGNOSTICS, "Overflow temples: ");

    CrawlVector &levels = you.props[OVERFLOW_TEMPLES_KEY].get_vector();

    vector<string> temple_strings;
    vector<string> god_names;

    for (unsigned int i = 0; i < levels.size(); i++)
    {
        CrawlStoreValue &val = levels[i];

        // Does this level have an overflow temple?
        if (val.get_flags() & SFLAG_UNSET)
            continue;

        CrawlVector &temples = val.get_vector();

        if (temples.empty())
            continue;

        temple_strings.clear();

        for (CrawlHashTable &temple_hash : temples)
        {
            god_names.clear();
            CrawlVector    &gods        = temple_hash[TEMPLE_GODS_KEY];

            for (unsigned int k = 0; k < gods.size(); k++)
            {
                god_type god = (god_type) gods[k].get_byte();

                god_names.push_back(god_name(god));
            }
            temple_strings.push_back(
                comma_separated_line(god_names.begin(), god_names.end()));
        }

        mprf(MSGCH_DIAGNOSTICS, "%u on D:%u (%s)", temples.size(),
             i + 1,
             comma_separated_line(temple_strings.begin(),
                                  temple_strings.end(), "; ", "; ").c_str()
          );
    }
}

void wizard_reveal_traps()
{
    int traps_revealed = reveal_traps(1000);
    mprf("Revealed %d traps.", traps_revealed);
}

void wizard_map_level()
{
    if (!is_map_persistent())
    {
        if (!yesno("Force level to be mappable?", true, 'n'))
        {
            canned_msg(MSG_OK);
            return;
        }
        env.properties[FORCE_MAPPABLE_KEY] = true;
    }

    mpr("Mapping level.");
    magic_mapping(1000, 100, true, true);

    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        update_item_at(*ri, false, true);
        show_update_at(*ri, LAYER_ITEMS);
#ifdef USE_TILE
        tiles.update_minimap(*ri);
#endif
    }
}

bool debug_make_trap(const coord_def& pos)
{
    char requested_trap[80];
    trap_type trap = TRAP_UNASSIGNED;
    int gridch     = grd(pos);

    if (gridch != DNGN_FLOOR)
    {
        mpr("You need to be on a floor square to make a trap.");
        return false;
    }

    msgwin_get_line("What kind of trap? ",
                    requested_trap, sizeof(requested_trap));
    if (!*requested_trap)
    {
        canned_msg(MSG_OK);
        return false;
    }

    string spec = lowercase_string(requested_trap);
    vector<trap_type> matches;
    vector<string>    match_names;

    if (spec == "random" || spec == "any")
        trap = TRAP_RANDOM;

    for (int t =
#if TAG_MAJOR_VERSION == 34
            TRAP_DART
#else
            TRAP_ARROW
#endif
            ; t < NUM_TRAPS; ++t)
    {
        const trap_type tr = static_cast<trap_type>(t);
        string tname       = lowercase_string(trap_name(tr));
        if (spec.find(tname) != spec.npos)
        {
            trap = tr;
            break;
        }
        else if (tname.find(spec) != tname.npos)
        {
            matches.push_back(tr);
            match_names.push_back(tname);
        }
    }

    if (trap == TRAP_UNASSIGNED)
    {
        if (matches.empty())
        {
            mprf("I know no traps named \"%s\".", spec.c_str());
            return false;
        }
        // Only one match, use that
        else if (matches.size() == 1)
            trap = matches[0];
        else
        {
            string prefix = "No exact match for trap '";
            prefix += spec;
            prefix += "', possible matches are: ";
            mpr_comma_separated_list(prefix, match_names);
            return false;
        }
    }

    place_specific_trap(you.pos(), trap);
    mprf("Created %s, marked it undiscovered.",
         (trap == TRAP_RANDOM)
            ? "a random trap"
            : trap_at(you.pos())->name(DESC_A).c_str());

    if (trap == TRAP_SHAFT && !is_valid_shaft_level())
        mpr("NOTE: Shaft traps aren't valid on this level.");

    return true;
}

bool debug_make_shop(const coord_def& pos)
{
    if (grd(pos) != DNGN_FLOOR)
    {
        mpr("Insufficient floor-space for new Wal-Mart.");
        return false;
    }

    char requested_shop[80];
    msgwin_get_line("What kind of shop? ",
                    requested_shop, sizeof(requested_shop));
    if (!*requested_shop)
    {
        canned_msg(MSG_OK);
        return false;
    }

    const shop_type new_shop_type = str_to_shoptype(requested_shop);

    if (new_shop_type == SHOP_UNASSIGNED)
    {
        mprf("Bad shop type: \"%s\"", requested_shop);
        list_shop_types();
        return false;
    }

    place_spec_shop(pos, new_shop_type);
    mpr("Done.");
    return true;
}

static void _free_all_vaults()
{
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        env.level_map_ids(*ri) = INVALID_MAP_INDEX;

    for (auto &vp : env.level_vaults)
        vp->seen = false;

    dgn_erase_unused_vault_placements();
}

static void debug_load_map_by_name(string name, bool primary)
{
    if (primary)
        _free_all_vaults();

    const bool place_on_us = !primary && strip_tag(name, "*", true);

    level_clear_vault_memory();
    const map_def *toplace = find_map_by_name(name);
    if (!toplace)
    {
        vector<string> matches = find_map_matches(name);

        if (matches.empty())
        {
            mprf("Can't find map named '%s'.", name.c_str());
            return;
        }
        else if (matches.size() == 1)
        {
            string prompt = "Only match is '";
            prompt += matches[0];
            prompt += "', use that?";
            if (!yesno(prompt.c_str(), true, 'y'))
            {
                canned_msg(MSG_OK);
                return;
            }

            toplace = find_map_by_name(matches[0]);
        }
        else
        {
            string prompt = "No exact matches for '";
            prompt += name;
            prompt += "', possible matches are: ";
            mpr_comma_separated_list(prompt, matches);
            return;
        }
    }

    coord_def where(-1, -1);
    if (place_on_us)
    {
        if (toplace->orient == MAP_FLOAT || toplace->orient == MAP_NONE)
        {
            coord_def size = toplace->map.size();
            coord_def tl   = you.pos() - (size / 2) - coord_def(-1, -1);
            coord_def br   = you.pos() + (size / 2) + coord_def(-1, -1);

            for (rectangle_iterator ri(tl, br); ri; ++ri)
            {
                if (!in_bounds(*ri))
                {
                    mprf("Placing %s on top of you would put part of the "
                         "map outside of the level, cancelling.",
                         toplace->name.c_str());
                    return;
                }
            }
            // We're okay.
            where = you.pos();
        }
        else
        {
            mprf("%s decides where it goes, can't place where you are.",
                 toplace->name.c_str());
        }
    }

    if (primary)
    {
        if (toplace->orient == MAP_ENCOMPASS
            && !toplace->is_usable_in(level_id::current())
            && !yesno("Warning: this is an encompass vault not designed "
                       "for this location; placing it with &P may result in "
                       "crashes and save corruption. Continue?", true, 'y'))
        {
            mprf("Ok; try placing with &L or go to the relevant location to "
                 "safely place with &P.");
            return;
        }
        if (toplace->is_minivault())
            you.props["force_minivault"] = toplace->name;
        else
            you.props["force_map"] = toplace->name;
        wizard_recreate_level();
        you.props.erase("force_minivault");
        you.props.erase("force_map");

        // We just saved with you.props["force_map"] set; save again in
        // case we crash (lest we have the property forever).
        if (!crawl_state.test)
            save_game_state();
    }
    else
    {
        unwind_var<string_set> um(you.uniq_map_names, string_set());
        unwind_var<string_set> umt(you.uniq_map_tags, string_set());
        unwind_var<string_set> lum(env.level_uniq_maps, string_set());
        unwind_var<string_set> lumt(env.level_uniq_map_tags, string_set());
        if (dgn_place_map(toplace, false, false, where))
        {
            mprf("Successfully placed %s.", toplace->name.c_str());
            // Fix up doors from vaults and any changes to the default walls
            // and floors from the vault.
            tile_init_flavour();
            // Transporters would normally be made from map markers by the
            // builder.
            dgn_make_transporters_from_markers();
        }
        else
            mprf("Failed to place %s.", toplace->name.c_str());
    }
}

static input_history mini_hist(10), primary_hist(10);

void debug_place_map(bool primary)
{
    char what_to_make[100];
    clear_messages();
    mprf(MSGCH_PROMPT, primary ? "Enter map name: " :
         "Enter map name (prefix it with * for local placement): ");
    if (cancellable_get_line(what_to_make, sizeof what_to_make,
                            primary ? &primary_hist : &mini_hist))
    {
        canned_msg(MSG_OK);
        return;
    }

    string what = what_to_make;
    trim_string(what);
    if (what.empty())
    {
        canned_msg(MSG_OK);
        return;
    }

    debug_load_map_by_name(what, primary);
}

static void _debug_kill_traps()
{
    for (rectangle_iterator ri(1); ri; ++ri)
        if (feat_is_trap(grd(*ri), true))
            destroy_trap(*ri);
}

static int _debug_time_explore()
{
    viewwindow();
    start_explore(false);

    unwind_var<int> es(Options.explore_stop, 0);

    const int start = you.num_turns;
    while (you_are_delayed())
    {
        you.turn_is_over = false;
        handle_delay();
        you.num_turns++;
    }

    // Elapsed time might not match up if explore had to go through
    // shallow water.
    PlaceInfo& pi = you.get_place_info();
    pi.elapsed_total = (pi.elapsed_explore + pi.elapsed_travel +
                        pi.elapsed_interlevel + pi.elapsed_resting +
                        pi.elapsed_other);

    PlaceInfo& pi2 = you.global_info;
    pi2.elapsed_total = (pi2.elapsed_explore + pi2.elapsed_travel +
                         pi2.elapsed_interlevel + pi2.elapsed_resting +
                         pi2.elapsed_other);

    return you.num_turns - start;
}

static void _debug_destroy_doors()
{
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            const dungeon_feature_type feat = grd[x][y];
            if (feat_is_closed_door(feat))
                grd[x][y] = DNGN_FLOOR;
        }
}

// Turns off greedy explore, then:
// a) Destroys all traps on the level.
// b) Kills all monsters on the level.
// c) Suppresses monster generation.
// d) Converts all closed doors to floor.
// e) Forgets map.
// f) Counts number of turns needed to explore the level.
void debug_test_explore()
{
    wizard_dismiss_all_monsters(true);
    _debug_kill_traps();
    _debug_destroy_doors();

    forget_map();

    // Remember where we are now.
    const coord_def where = you.pos();

    const int explore_turns = _debug_time_explore();

    // Return to starting point.
    you.moveto(where);

    mprf("Explore took %d turns.", explore_turns);
}

void wizard_list_levels()
{
    if (!you.level_stack.empty())
    {
        mpr("Level stack:");
        for (unsigned int i = 0; i < you.level_stack.size(); i++)
        {
            mprf(MSGCH_DIAGNOSTICS, i+1, // inhibit merging
                 "%-10s (%d,%d)", you.level_stack[i].id.describe().c_str(),
                 you.level_stack[i].pos.x, you.level_stack[i].pos.y);
        }
    }

    travel_cache.update_daction_counters();

    vector<level_id> levs = travel_cache.known_levels();

    mpr("Known levels:");
    for (unsigned int i = 0; i < levs.size(); i++)
    {
        const LevelInfo* lv = travel_cache.find_level_info(levs[i]);
        ASSERT(lv);

        string cnts = "";
        for (int j = 0; j < NUM_DACTION_COUNTERS; j++)
        {
            char num[20];
            sprintf(num, "%d/", lv->daction_counters[j]);
            cnts += num;
        }
        mprf(MSGCH_DIAGNOSTICS, i+1, // inhibit merging
             "%-10s : %s", levs[i].describe().c_str(), cnts.c_str());
    }

    string cnts = "";
    for (int j = 0; j < NUM_DACTION_COUNTERS; j++)
    {
        char num[20];
        sprintf(num, "%d/", query_daction_counter((daction_type)j));
        cnts += num;
    }
    mprf("%-10s : %s", "`- total", cnts.c_str());
}

void wizard_recreate_level()
{
    mpr("Regenerating level.");

    // Need to allow reuse of vaults, otherwise we'd run out of them fast.
    _free_all_vaults();

    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_is_unique(mi->type))
        {
            remove_unique_annotation(*mi);
            you.unique_creatures.set(mi->type, false);
            mi->flags |= MF_TAKING_STAIRS; // no Abyss transit
        }
    }

    level_id lev = level_id::current();
    dungeon_feature_type stair_taken = DNGN_STONE_STAIRS_DOWN_I;

    if (lev.depth == 1 && lev != BRANCH_DUNGEON)
        stair_taken = branches[lev.branch].entry_stairs;

    leaving_level_now(stair_taken);
    you.get_place_info().levels_seen--;
    delete_level(lev);
    const bool newlevel = load_level(stair_taken, LOAD_START_GAME, lev);
    tile_new_level(newlevel);
    if (!crawl_state.test)
        save_game_state();
    new_level();
    seen_monsters_react();
    viewwindow();

    trackers_init_new_level(true);
}

void wizard_clear_used_vaults()
{
    you.uniq_map_tags.clear();
    you.uniq_map_names.clear();
    env.level_uniq_maps.clear();
    env.level_uniq_map_tags.clear();
    mpr("All vaults are now eligible for [re]use.");
}

void wizard_abyss_speed()
{
    char specs[256];
    mprf(MSGCH_PROMPT, "Set Abyss speed to what? (now %d, higher value = "
                       "higher speed) ", you.abyss_speed);

    if (!cancellable_get_line(specs, sizeof(specs)))
    {
        const int speed = atoi(specs);
        if (speed || specs[0] == '0')
        {
            mprf("Setting Abyss speed to %i.", speed);
            you.abyss_speed = speed;
            return;
        }
    }

    canned_msg(MSG_OK);
}
#endif
