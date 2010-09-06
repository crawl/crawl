/*
 *  File:       wiz-dgn.h
 *  Summary:    Dungeon related wizard functions.
 *  Written by: Linley Henzell and Jesse Jones
 */

#include "AppHdr.h"

#include "wiz-dgn.h"

#include "branch.h"
#include "cio.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "dgn-actions.h"
#include "dungeon.h"
#include "effects.h"
#include "env.h"
#include "files.h"
#include "items.h"
#include "l_defs.h"
#include "libutil.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "options.h"
#include "place.h"
#include "player.h"
#include "religion.h"
#include "stairs.h"
#include "stuff.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tileview.h"
#endif
#include "travel.h"
#include "traps.h"
#include "view.h"
#include "wiz-mon.h"

#ifdef WIZARD
static dungeon_feature_type _find_appropriate_stairs(bool down)
{
    if (you.level_type == LEVEL_DUNGEON)
    {
        int depth = subdungeon_depth(you.where_are_you, you.absdepth0);
        if (down)
            depth++;
        else
            depth--;

        // Can't go down from bottom level of a branch.
        if (depth > branches[you.where_are_you].depth)
        {
            mpr("Can't go down from the bottom of a branch.");
            return DNGN_UNSEEN;
        }
        // Going up from top level of branch
        else if (depth == 0)
        {
            // Special cases
            if (you.where_are_you == BRANCH_VESTIBULE_OF_HELL)
                return DNGN_EXIT_HELL;
            else if (you.where_are_you == BRANCH_MAIN_DUNGEON)
                return DNGN_STONE_STAIRS_UP_I;

            dungeon_feature_type stairs = your_branch().exit_stairs;

            if (stairs < DNGN_RETURN_FROM_FIRST_BRANCH
                || stairs > DNGN_RETURN_FROM_LAST_BRANCH)
            {
                mpr("This branch has no exit stairs defined.");
                return DNGN_UNSEEN;
            }
            return (stairs);
        }
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

    switch (you.level_type)
    {
    case LEVEL_LABYRINTH:
        if (down)
        {
            mpr("Can't go down in the Labyrinth.");
            return DNGN_UNSEEN;
        }
        else
            return DNGN_ESCAPE_HATCH_UP;

    case LEVEL_ABYSS:
        return DNGN_EXIT_ABYSS;

    case LEVEL_PANDEMONIUM:
        if (down)
            return DNGN_TRANSIT_PANDEMONIUM;
        else
            return DNGN_EXIT_PANDEMONIUM;

    case LEVEL_PORTAL_VAULT:
        return DNGN_EXIT_PORTAL_VAULT;

    default:
        mpr("Unknown level type.");
        return DNGN_UNSEEN;
    }

    mpr("Impossible occurrence in find_appropriate_stairs()");
    return DNGN_UNSEEN;
}

void wizard_place_stairs( bool down )
{
    dungeon_feature_type stairs = _find_appropriate_stairs(down);

    if (stairs == DNGN_UNSEEN)
        return;

    dungeon_terrain_changed(you.pos(), stairs, false);
}

// Try to find and use stairs already in the portal vault level,
// since this might be a multi-level portal vault like a ziggurat.
static bool _take_portal_vault_stairs( const bool down )
{
    ASSERT(you.level_type == LEVEL_PORTAL_VAULT);

    const command_type cmd = down ? CMD_GO_DOWNSTAIRS : CMD_GO_UPSTAIRS;

    coord_def stair_pos(-1, -1);

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (feat_stair_direction(grd(*ri)) == cmd)
        {
            stair_pos = *ri;
            break;
        }
    }

    if (!in_bounds(stair_pos))
        return (false);

    clear_trapping_net();
    you.set_position(stair_pos);

    if (down)
        down_stairs();
    else
        up_stairs();

    return (true);
}

void wizard_level_travel( bool down )
{
    if (you.level_type == LEVEL_PORTAL_VAULT)
        if (_take_portal_vault_stairs(down))
            return;

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
        down_stairs(stairs);
    else
        up_stairs(stairs);
}

static void _wizard_go_to_level(const level_pos &pos)
{
    const int abs_depth = absdungeon_depth(pos.id.branch, pos.id.depth);
    dungeon_feature_type stair_taken =
        abs_depth > you.absdepth0? DNGN_STONE_STAIRS_DOWN_I
                                  : DNGN_STONE_STAIRS_UP_I;

    if (abs_depth > you.absdepth0 && pos.id.depth == 1
        && pos.id.branch != BRANCH_MAIN_DUNGEON)
    {
        stair_taken = branches[pos.id.branch].entry_stairs;
    }

    const level_id old_level = level_id::current();
    const bool keep_travel_data = can_travel_interlevel();

    you.level_type    = LEVEL_DUNGEON;
    you.where_are_you = static_cast<branch_type>(pos.id.branch);
    you.absdepth0    = abs_depth;

    const bool newlevel = load(stair_taken, LOAD_ENTER_LEVEL, old_level);
#ifdef USE_TILE
    tile_new_level(newlevel);
#else
    UNUSED(newlevel);
#endif
    if (!crawl_state.test)
        save_game_state();
    new_level();
    seen_monsters_react();
    viewwindow();

    if (!keep_travel_data)
        travel_cache.erase_level_info(old_level);
    // Tell stash-tracker and travel that we've changed levels.
    trackers_init_new_level(true);
}

void wizard_interlevel_travel()
{
    std::string name;
    const level_pos pos =
        prompt_translevel_target(TPF_ALLOW_UPDOWN | TPF_SHOW_ALL_BRANCHES, name).p;

    if (pos.id.depth < 1 || pos.id.depth > branches[pos.id.branch].depth)
    {
        canned_msg(MSG_OK);
        return;
    }

    _wizard_go_to_level(pos);
}

void wizard_create_portal()
{
    mpr("Destination for portal (defaults to 'bazaar')? ", MSGCH_PROMPT);
    char specs[256];
    if (cancelable_get_line(specs, sizeof(specs)))
    {
        canned_msg( MSG_OK );
        return;
    }

    std::string dst = specs;
    dst = trim_string(dst);
    dst = replace_all(dst, " ", "_");

    if (dst.empty())
        dst = "bazaar";

    if (!find_map_by_name(dst) && !random_map_for_tag(dst))
    {
        mprf("No map named '%s' or tagged '%s'.", dst.c_str(), dst.c_str());
    }
    else
    {
        map_wiz_props_marker *marker = new map_wiz_props_marker(you.pos());
        marker->set_property("dst", dst);
        marker->set_property("feature_description",
                             "wizard portal, dest = " + dst);
        env.markers.add(marker);
        dungeon_terrain_changed(you.pos(), DNGN_ENTER_PORTAL_VAULT, false);
    }
}

void wizard_create_feature()
{
    char specs[256];
    int feat_num;
    dungeon_feature_type feat;
    mpr("Create which feature? ", MSGCH_PROMPT);

    if (!cancelable_get_line(specs, sizeof(specs)) && specs[0] != 0)
    {
        if ((feat_num = atoi(specs)))
        {
            feat = static_cast<dungeon_feature_type>(feat_num);
        }
        else
        {
            std::string name = lowercase_string(specs);
            name = replace_all(name, " ", "_");
            feat = dungeon_feature_by_name(name);
            if (feat == DNGN_UNSEEN) // no exact match
            {
                std::vector<std::string> matches =
                    dungeon_feature_matches(name);

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
                    return;
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
                    std::string prefix = "No exact match for feature '" +
                        name +  "', possible matches are: ";

                    // Use mpr_comma_separated_list() because the list
                    // might be *LONG*.
                    mpr_comma_separated_list(prefix, matches, " and ", ", ",
                                             MSGCH_DIAGNOSTICS);
                    return;
                }
            }
        }

        if (feat == DNGN_ENTER_SHOP)
        {
            debug_make_shop();
            return;
        }

        dungeon_terrain_changed(you.pos(), feat, false);
#ifdef USE_TILE
        env.tile_flv(you.pos()).special = 0;
#endif
    }
    else
        canned_msg(MSG_OK);
}

void wizard_list_branches()
{
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].startdepth != - 1)
        {
            mprf(MSGCH_DIAGNOSTICS, "Branch %d (%s) is on level %d of %s",
                 i, branches[i].longname, branches[i].startdepth,
                 branches[branches[i].parent_branch].abbrevname);
        }
        else if (i == BRANCH_SWAMP || i == BRANCH_SHOALS)
        {
            mprf(MSGCH_DIAGNOSTICS, "Branch %d (%s) was not generated "
                 "this game", i, branches[i].longname);
        }
    }

    if (!you.props.exists(OVERFLOW_TEMPLES_KEY))
        return;

    mpr("----", MSGCH_DIAGNOSTICS);
    mpr("Overflow temples: ", MSGCH_DIAGNOSTICS);

    CrawlVector &levels = you.props[OVERFLOW_TEMPLES_KEY].get_vector();

    for (unsigned int i = 0; i < levels.size(); i++)
    {
        CrawlStoreValue &val = levels[i];

        // Does this level have an overflow temple?
        if (val.get_flags() & SFLAG_UNSET)
            continue;

        CrawlVector &temples = val.get_vector();

        if (temples.size() == 0)
            continue;

        std::vector<std::string> god_names;

        for (unsigned int j = 0; j < temples.size(); j++)
        {
            CrawlHashTable &temple_hash = temples[j];
            CrawlVector    &gods        = temple_hash[TEMPLE_GODS_KEY];

            for (unsigned int k = 0; k < gods.size(); k++)
            {
                god_type god = (god_type) gods[k].get_byte();

                god_names.push_back(god_name(god));
            }
        }

        mprf(MSGCH_DIAGNOSTICS, "%u on D:%u (%s)", temples.size(),
             i + 1,
             comma_separated_line( god_names.begin(),
                                   god_names.end() ).c_str()
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
    if (testbits(env.level_flags, LFLAG_NOT_MAPPABLE)
        || testbits(get_branch_flags(), BFLAG_NOT_MAPPABLE))
    {
        if (!yesno("Force level to be mappable?", true, 'n'))
        {
            canned_msg( MSG_OK );
            return;
        }

        unset_level_flags(LFLAG_NOT_MAPPABLE | LFLAG_NO_MAGIC_MAP);
        unset_branch_flags(BFLAG_NOT_MAPPABLE | BFLAG_NO_MAGIC_MAP);
    }

    magic_mapping(1000, 100, true, true);
}

static int find_trap_slot()
{
    for (int i = 0; i < MAX_TRAPS; ++i)
        if (env.trap[i].type == TRAP_UNASSIGNED)
            return (i);

    return (-1);
}

void debug_make_trap()
{
    char requested_trap[80];
    int trap_slot  = find_trap_slot();
    trap_type trap = TRAP_UNASSIGNED;
    int gridch     = grd(you.pos());

    if (trap_slot == -1)
    {
        mpr("Sorry, this level can't take any more traps.");
        return;
    }

    if (gridch != DNGN_FLOOR)
    {
        mpr("You need to be on a floor square to make a trap.");
        return;
    }

    msgwin_get_line("What kind of trap? ",
                    requested_trap, sizeof(requested_trap));
    if (!*requested_trap)
        return;

    strlwr(requested_trap);
    std::vector<trap_type>   matches;
    std::vector<std::string> match_names;
    for (int t = TRAP_DART; t < NUM_TRAPS; ++t)
    {
        const trap_type tr = static_cast<trap_type>(t);
        const char* tname  = trap_name(tr);
        if (strstr(requested_trap, tname))
        {
            trap = tr;
            break;
        }
        else if (strstr(tname, requested_trap))
        {
            matches.push_back(tr);
            match_names.push_back(tname);
        }
    }

    if (trap == TRAP_UNASSIGNED)
    {
        if (matches.empty())
        {
            mprf("I know no traps named \"%s\"", requested_trap);
            return;
        }
        // Only one match, use that
        else if (matches.size() == 1)
            trap = matches[0];
        else
        {
            std::string prefix = "No exact match for trap '";
            prefix += requested_trap;
            prefix += "', possible matches are: ";
            mpr_comma_separated_list(prefix, match_names);

            return;
        }
    }

    if (place_specific_trap(you.pos(), trap))
        mprf("Created a %s trap, marked it undiscovered", trap_name(trap));
    else
        mpr("Could not create trap - too many traps on level.");

    if (trap == TRAP_SHAFT && !is_valid_shaft_level())
        mpr("NOTE: Shaft traps aren't valid on this level.");
}

void debug_make_shop()
{
    char requested_shop[80];
    int gridch = grd(you.pos());
    bool have_shop_slots = false;
    int new_shop_type = SHOP_UNASSIGNED;
    bool representative = false;

    if (gridch != DNGN_FLOOR)
    {
        mpr("Insufficient floor-space for new Wal-Mart.");
        return;
    }

    for (int i = 0; i < MAX_SHOPS; ++i)
    {
        if (env.shop[i].type == SHOP_UNASSIGNED)
        {
            have_shop_slots = true;
            break;
        }
    }

    if (!have_shop_slots)
    {
        mpr("There are too many shops on this level.");
        return;
    }

    msgwin_get_line("What kind of shop? ",
                    requested_shop, sizeof(requested_shop));
    if (!*requested_shop)
        return;

    strlwr(requested_shop);
    std::string s = replace_all_of(requested_shop, "*", "");
    new_shop_type = str_to_shoptype(s);

    if (new_shop_type == SHOP_UNASSIGNED || new_shop_type == -1)
    {
        mprf("Bad shop type: \"%s\"", requested_shop);
        return;
    }

    representative = !!strchr(requested_shop, '*');

    place_spec_shop(you.absdepth0, you.pos(),
                    new_shop_type, representative);
    link_items();
    mprf("Done.");
}

static void debug_load_map_by_name(std::string name)
{
    const bool place_on_us = strip_tag(name, "*", true);

    level_clear_vault_memory();
    const map_def *toplace = find_map_by_name(name);
    if (!toplace)
    {
        std::vector<std::string> matches = find_map_matches(name);

        if (matches.empty())
        {
            mprf("Can't find map named '%s'.", name.c_str());
            return;
        }
        else if (matches.size() == 1)
        {
            std::string prompt = "Only match is '";
            prompt += matches[0];
            prompt += "', use that?";
            if (!yesno(prompt.c_str(), true, 'y'))
                return;

            toplace = find_map_by_name(matches[0]);
        }
        else
        {
            std::string prompt = "No exact matches for '";
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

    if (dgn_place_map(toplace, true, false, where))
    {
        mprf("Successfully placed %s.", toplace->name.c_str());
#ifdef USE_TILE
        // Fix up doors from vaults and any changes to the default walls
        // and floors from the vault.
        tile_init_flavour();
#endif
    }
    else
        mprf("Failed to place %s.", toplace->name.c_str());
}

void debug_place_map()
{
    char what_to_make[100];
    mesclr();
    mprf(MSGCH_PROMPT, "Enter map name: ");
    if (cancelable_get_line_autohist(what_to_make, sizeof what_to_make))
    {
        canned_msg(MSG_OK);
        return;
    }

    std::string what = what_to_make;
    trim_string(what);
    if (what.empty())
    {
        canned_msg(MSG_OK);
        return;
    }

    debug_load_map_by_name(what);
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

    const long start = you.num_turns;
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

    return (you.num_turns - start);
}

static void _debug_destroy_doors()
{
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            const dungeon_feature_type feat = grd[x][y];
            if (feat == DNGN_SECRET_DOOR || feat_is_closed_door(feat))
                grd[x][y] = DNGN_FLOOR;
        }
}

// Turns off greedy explore, then:
// a) Destroys all traps on the level.
// b) Kills all monsters on the level.
// c) Suppresses monster generation.
// d) Converts all closed doors and secret doors to floor.
// e) Forgets map.
// f) Counts number of turns needed to explore the level.
void debug_test_explore()
{
    wizard_dismiss_all_monsters(true);
    _debug_kill_traps();
    _debug_destroy_doors();

    forget_map(100);

    // Remember where we are now.
    const coord_def where = you.pos();

    const int explore_turns = _debug_time_explore();

    // Return to starting point.
    you.moveto(where);

    mprf("Explore took %d turns.", explore_turns);
}

void debug_shift_labyrinth()
{
    if (you.level_type != LEVEL_LABYRINTH)
    {
        mpr("This only makes sense in a labyrinth!");
        return;
    }
    change_labyrinth(true);
}

void wizard_list_levels()
{
    travel_cache.update_da_counters();

    std::vector<level_id> levs = travel_cache.known_levels();

    mpr("Known levels:");
    for (unsigned int i = 0; i < levs.size(); i++)
    {
        const LevelInfo* lv = travel_cache.find_level_info(levs[i]);
        ASSERT(lv);

        std::string cnts = "";
        for (int j = 0; j < NUM_DA_COUNTERS; j++)
        {
            char num[20];
            sprintf(num, "%d/", lv->da_counters[j]);
            cnts += num;
        }
        mprf(MSGCH_DIAGNOSTICS, i+1, // inhibit merging
             "%-10s : %s", levs[i].describe().c_str(), cnts.c_str());
    }

    std::string cnts = "";
    for (int j = 0; j < NUM_DA_COUNTERS; j++)
    {
        char num[20];
        sprintf(num, "%d/", query_da_counter((daction_type)j));
        cnts += num;
    }
    mprf("%-10s : %s", "`- total", cnts.c_str());
}
#endif
