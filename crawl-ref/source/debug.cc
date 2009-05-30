/*
 *  File:       debug.cc
 *  Summary:    Debug and wizard related functions.
 *  Written by: Linley Henzell and Jesse Jones
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "debug.h"

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <algorithm>
#include <errno.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "beam.h"
#include "branch.h"
#include "chardump.h"
#include "cio.h"
#include "crash.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "effects.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "ghost.h"
#include "initfile.h"
#include "invent.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "items.h"
#include "makeitem.h"
#include "mapdef.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monspeak.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "mutation.h"
#include "newgame.h"
#include "ouch.h"
#include "output.h"
#include "place.h"
#include "player.h"
#include "quiver.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tiles.h"
#include "traps.h"
#include "travel.h"
#include "version.h"
#include "view.h"

#ifdef WIZARD
#include "macro.h"
#include "shopping.h"
#include "xom.h"
#endif


// ========================================================================
//      Internal Functions
// ========================================================================

static void _dump_levgen();

//---------------------------------------------------------------
// BreakStrToDebugger
//---------------------------------------------------------------
#if DEBUG
static void _BreakStrToDebugger(const char *mesg)
{

#if OSX || defined(__MINGW32__)
    fprintf(stderr, mesg);
// raise(SIGINT);               // this is what DebugStr() does on OS X according to Tech Note 2030
    int* p = NULL;              // but this gives us a stack crawl...
    *p = 0;

#else
    fprintf(stderr, "%s\n", mesg);
    abort();
#endif
}
#endif

// ========================================================================
//      Global Functions
// ========================================================================

//---------------------------------------------------------------
//
// AssertFailed
//
//---------------------------------------------------------------
#if DEBUG
static std::string _assert_msg;

void AssertFailed(const char *expr, const char *file, int line)
{
    char mesg[512];

    const char *fileName = file + strlen(file); // strip off path

    while (fileName > file && fileName[-1] != '\\')
        --fileName;

    sprintf(mesg, "ASSERT(%s) in '%s' at line %d failed.", expr, fileName,
            line);

    _assert_msg = mesg;

    _BreakStrToDebugger(mesg);
}
#endif


//---------------------------------------------------------------
//
// DEBUGSTR
//
//---------------------------------------------------------------
#if DEBUG
void DEBUGSTR(const char *format, ...)
{
    char mesg[2048];

    va_list args;

    va_start(args, format);
    vsprintf(mesg, format, args);
    va_end(args);

    _BreakStrToDebugger(mesg);
}
#endif

static int _debug_prompt_for_monster( void )
{
    char  specs[80];

    mpr("Which monster by name? ", MSGCH_PROMPT);
    if (!cancelable_get_line_autohist(specs, sizeof specs))
    {
        if (specs[0] == '\0')
            return (-1);

        return (get_monster_by_name(specs));
    }
    return (-1);
}

//---------------------------------------------------------------
//
// debug_prompt_for_skill
//
//---------------------------------------------------------------
static int _debug_prompt_for_skill( const char *prompt )
{
    char specs[80];

    mpr(prompt, MSGCH_PROMPT);
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
        return (-1);

    int skill = -1;

    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        // Avoid the bad values.
        if (is_invalid_skill(i))
            continue;

        char sk_name[80];
        strncpy( sk_name, skill_name(i), sizeof( sk_name ) );

        char *ptr = strstr( strlwr(sk_name), strlwr(specs) );
        if (ptr != NULL)
        {
            if (ptr == sk_name && strlen(specs) > 0)
            {
                // We prefer prefixes over partial matches.
                skill = i;
                break;
            }
            else
                skill = i;
        }
    }

    return (skill);
}

#ifdef WIZARD
void wizard_change_species( void )
{
    char specs[80];
    int i;

    mpr("What species would you like to be now? " , MSGCH_PROMPT);
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
        return;
    strlwr(specs);

    species_type sp = SP_UNKNOWN;

    for (i = SP_HUMAN; i < NUM_SPECIES; ++i)
    {
        const species_type si = static_cast<species_type>(i);
        const std::string sp_name =
            lowercase_string(species_name(si, you.experience_level));

        std::string::size_type pos = sp_name.find(specs);
        if (pos != std::string::npos)
        {
            if (pos == 0 && *specs)
            {
                // We prefer prefixes over partial matches.
                sp = si;
                break;
            }
            else
                sp = si;
        }
    }

    if (sp == SP_UNKNOWN)
    {
        mpr("That species isn't available.");
        return;
    }

    // Re-scale skill-points.
    for (i = 0; i < NUM_SKILLS; ++i)
    {
        you.skill_points[i] *= species_skills( i, sp );
        you.skill_points[i] /= species_skills( i, you.species );
    }

    you.species = sp;
    you.is_undead = get_undead_state(sp);

    // Change permanent mutations, but preserve non-permanent ones.
    unsigned char prev_muts[NUM_MUTATIONS];
    you.attribute[ATTR_NUM_DEMONIC_POWERS] = 0;
    for (i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (you.demon_pow[i] > 0)
        {
            if (you.demon_pow[i] > you.mutation[i])
                you.mutation[i] = 0;
            else
                you.mutation[i] -= you.demon_pow[i];

            you.demon_pow[i] = 0;
        }
        prev_muts[i] = you.mutation[i];
    }
    give_basic_mutations(sp);
    for (i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (prev_muts[i] > you.demon_pow[i])
            you.demon_pow[i] = 0;
        else
            you.demon_pow[i] -= prev_muts[i];
    }

    switch (sp)
    {
    case SP_GREEN_DRACONIAN:
        if (you.experience_level >= 7)
            perma_mutate(MUT_POISON_RESISTANCE, 1);
        break;

    case SP_RED_DRACONIAN:
        if (you.experience_level >= 14)
            perma_mutate(MUT_HEAT_RESISTANCE, 1);
        break;

    case SP_WHITE_DRACONIAN:
        if (you.experience_level >= 14)
            perma_mutate(MUT_COLD_RESISTANCE, 1);
        break;


    case SP_BLACK_DRACONIAN:
        if (you.experience_level >= 18)
            perma_mutate(MUT_SHOCK_RESISTANCE, 1);
        break;

    case SP_DEMONSPAWN:
    {
        int powers = 0;

        if (you.experience_level < 4)
            powers = 0;
        else if (you.experience_level < 9)
            powers = 1;
        else if (you.experience_level < 14)
            powers = 2;
        else if (you.experience_level < 19)
            powers = 3;
        else if (you.experience_level < 24)
            powers = 4;
        else if (you.experience_level == 27)
            powers = 5;

        int levels[] = {4, 9, 14, 19, 27};
        int real_level = you.experience_level;

        for (i = 0; i < powers; ++i)
        {
            // The types of demonspawn mutations you get depends on your
            // experience level at the time of gaining it.
            you.experience_level = levels[i];
            demonspawn();
        }
        you.experience_level = real_level;

        break;
    }

    default:
        break;
    }

    redraw_screen();
}
#endif
//---------------------------------------------------------------
//
// debug_prompt_for_int
//
// If nonneg, then it returns a non-negative number or -1 on fail
// If !nonneg, then it returns an integer, and 0 on fail
//
//---------------------------------------------------------------
static int _debug_prompt_for_int( const char *prompt, bool nonneg )
{
    char specs[80];

    mpr(prompt, MSGCH_PROMPT);
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
        return (nonneg ? -1 : 0);

    char *end;
    int   ret = strtol( specs, &end, 10 );

    if (ret < 0 && nonneg || ret == 0 && end == specs)
        ret = (nonneg ? -1 : 0);

    return (ret);
}

// Some debugging functions, accessible in wizard mode.

#ifdef WIZARD
// Casts a specific spell by number.
void wizard_cast_spec_spell(void)
{
    int spell = _debug_prompt_for_int( "Cast which spell by number? ", true );

    if (spell == -1)
        canned_msg( MSG_OK );
    else if (your_spells( static_cast<spell_type>(spell), 0, false )
                == SPRET_ABORT)
    {
        crawl_state.cancel_cmd_repeat();
    }
}
#endif


#ifdef WIZARD
// Casts a specific spell by name.
void wizard_cast_spec_spell_name(void)
{
    char specs[80];

    mpr("Cast which spell by name? ", MSGCH_PROMPT);
    if (cancelable_get_line_autohist( specs, sizeof( specs ) )
        || specs[0] == '\0')
    {
        canned_msg( MSG_OK );
        crawl_state.cancel_cmd_repeat();
        return;
    }

    spell_type type = spell_by_name(specs, true);
    if (type == SPELL_NO_SPELL)
    {
        mpr((one_chance_in(20)) ? "Maybe you should go back to WIZARD school."
                                : "I couldn't find that spell.");
        crawl_state.cancel_cmd_repeat();
        return;
    }

    if (your_spells(type, 0, false) == SPRET_ABORT)
        crawl_state.cancel_cmd_repeat();
}
#endif


#ifdef WIZARD
// Creates a specific monster by mon type number.
void wizard_create_spec_monster(void)
{
    int mon = _debug_prompt_for_int( "Create which monster by number? ", true );

    if (mon == -1 || (mon >= NUM_MONSTERS
                      && mon != RANDOM_MONSTER
                      && mon != RANDOM_DRACONIAN
                      && mon != RANDOM_BASE_DRACONIAN
                      && mon != RANDOM_NONBASE_DRACONIAN
                      && mon != WANDERING_MONSTER))
    {
        canned_msg( MSG_OK );
    }
    else
    {
        create_monster(
            mgen_data::sleeper_at(
                static_cast<monster_type>(mon), you.pos()));
    }
}
#endif


#ifdef WIZARD
// Creates a specific monster by name. Uses the same patterns as
// map definitions.
void wizard_create_spec_monster_name()
{
    char specs[100];
    mpr("Which monster by name? ", MSGCH_PROMPT);
    if (cancelable_get_line_autohist(specs, sizeof specs) || !*specs)
    {
        canned_msg(MSG_OK);
        return;
    }

    mons_list mlist;
    std::string err = mlist.add_mons(specs);

    if (!err.empty())
    {
        // Try for a partial match, but not if the user accidently entered
        // only a few letters.
        monster_type partial = get_monster_by_name(specs);
        if (strlen(specs) >= 3 && partial != NON_MONSTER)
        {
            mlist.clear();
            err = mlist.add_mons(mons_type_name(partial, DESC_PLAIN));
        }

        if (!err.empty())
        {
            mpr(err.c_str());
            return;
        }
    }

    mons_spec mspec = mlist.get_monster(0);
    if (mspec.mid == -1)
    {
        mpr("Such a monster couldn't be found.", MSGCH_DIAGNOSTICS);
        return;
    }

    int type = mspec.mid;
    if (mons_class_is_zombified(mspec.mid))
        type = mspec.monbase;

    coord_def place = find_newmons_square(type, you.pos());
    if (!in_bounds(place))
    {
        // Try again with habitat HT_LAND.
        // (Will be changed to the necessary terrain type in dgn_place_monster.)
        place = find_newmons_square(MONS_PROGRAM_BUG, you.pos());
    }

    if (!in_bounds(place))
    {
        mpr("Found no space to place monster.", MSGCH_DIAGNOSTICS);
        return;
    }

    // Wizmode users should be able to conjure up uniques even if they
    // were already created. Yay, you can meet 3 Sigmunds at once! :p
    if (mons_is_unique(mspec.mid) && you.unique_creatures[mspec.mid])
        you.unique_creatures[mspec.mid] = false;

    if (dgn_place_monster(mspec, you.your_level, place, true, false) == -1)
    {
        mpr("Unable to place monster.", MSGCH_DIAGNOSTICS);
        return;
    }

    // Need to set a name for the player ghost.
    if (mspec.mid == MONS_PLAYER_GHOST)
    {
        unsigned short mid = mgrd(place);

        if (mid >= MAX_MONSTERS || menv[mid].type != MONS_PLAYER_GHOST)
        {
            for (mid = 0; mid < MAX_MONSTERS; mid++)
            {
                if (menv[mid].type == MONS_PLAYER_GHOST
                    && menv[mid].alive())
                {
                    break;
                }
            }
        }

        if (mid >= MAX_MONSTERS)
        {
            mpr("Couldn't find player ghost, probably going to crash.");
            more();
            return;
        }

        monsters    &mon = menv[mid];
        ghost_demon ghost;

        ghost.name = "John Doe";

        char input_str[80];
        mpr("Make player ghost which species? (case-sensitive) ", MSGCH_PROMPT);
        get_input_line( input_str, sizeof( input_str ) );

        int sp_id = get_species_by_abbrev(input_str);
        if (sp_id == -1)
            sp_id = str_to_species(input_str);

        if (sp_id == -1)
        {
            mpr("No such species, making it Human.");
            sp_id = SP_HUMAN;
        }
        ghost.species = static_cast<species_type>(sp_id);

        mpr("Make player ghost which job? ", MSGCH_PROMPT);
        get_input_line( input_str, sizeof( input_str ) );

        int class_id = get_class_by_abbrev(input_str);

        if (class_id == -1)
            class_id = get_class_by_name(input_str);

        if (class_id == -1)
        {
            mpr("No such job, making it a Fighter.");
            class_id = JOB_FIGHTER;
        }
        ghost.job = static_cast<job_type>(class_id);
        ghost.xl = 7;

        mon.set_ghost(ghost);

        ghosts.push_back(ghost);
    }
}
#endif

#ifdef WIZARD
static dungeon_feature_type _find_appropriate_stairs(bool down)
{
    if (you.level_type == LEVEL_DUNGEON)
    {
        int depth = subdungeon_depth(you.where_are_you, you.your_level);
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
#endif

#ifdef WIZARD
void wizard_place_stairs( bool down )
{
    dungeon_feature_type stairs = _find_appropriate_stairs(down);

    if (stairs == DNGN_UNSEEN)
        return;

    dungeon_terrain_changed(you.pos(), stairs, false);
}
#endif

#ifdef WIZARD
// Try to find and use stairs already in the portal vault level,
// since this might be a multi-level portal vault like a ziggurat.
bool _take_portal_vault_stairs( const bool down )
{
    ASSERT(you.level_type == LEVEL_PORTAL_VAULT);

    const command_type cmd = down ? CMD_GO_DOWNSTAIRS : CMD_GO_UPSTAIRS;

    coord_def stair_pos(-1, -1);

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grid_stair_direction(grd(*ri)) == cmd)
        {
            stair_pos = *ri;
            break;
        }
    }

    if (!in_bounds(stair_pos))
        return (false);

    clear_trapping_net();
    you.position = stair_pos;

    if (down)
        down_stairs(you.your_level);
    else
        up_stairs();

    return (true);
}
#endif

#ifdef WIZARD
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
    command_type real_dir = grid_stair_direction(stairs);
    if (down && real_dir == CMD_GO_UPSTAIRS
        || !down && real_dir == CMD_GO_DOWNSTAIRS)
    {
        down = !down;
    }

    if (down)
        down_stairs(you.your_level, stairs);
    else
        up_stairs(stairs);
}

static void _wizard_go_to_level(const level_pos &pos)
{
    const int abs_depth = absdungeon_depth(pos.id.branch, pos.id.depth);
    dungeon_feature_type stair_taken =
        abs_depth > you.your_level? DNGN_STONE_STAIRS_DOWN_I
                                  : DNGN_STONE_STAIRS_UP_I;

    if (abs_depth > you.your_level && pos.id.depth == 1
        && pos.id.branch != BRANCH_MAIN_DUNGEON)
    {
        stair_taken = branches[pos.id.branch].entry_stairs;
    }

    const int old_level = you.your_level;
    const branch_type old_where = you.where_are_you;
    const level_area_type old_level_type = you.level_type;

    you.level_type    = LEVEL_DUNGEON;
    you.where_are_you = static_cast<branch_type>(pos.id.branch);
    you.your_level    = abs_depth;

    const bool newlevel = load(stair_taken, LOAD_ENTER_LEVEL, old_level_type,
        old_level, old_where);
#ifdef USE_TILE
    TileNewLevel(newlevel);
#else
    UNUSED(newlevel);
#endif
    save_game_state();
    new_level();
    viewwindow(true, true);

    // Tell stash-tracker and travel that we've changed levels.
    trackers_init_new_level(true);
}

void wizard_interlevel_travel()
{
    const level_pos pos =
        prompt_translevel_target(TPF_ALLOW_UPDOWN | TPF_SHOW_ALL_BRANCHES).p;

    if (pos.id.depth < 1 || pos.id.depth > branches[pos.id.branch].depth)
    {
        canned_msg(MSG_OK);
        return;
    }

    _wizard_go_to_level(pos);
}

static bool _sort_monster_list(int a, int b)
{
    const monsters* m1 = &menv[a];
    const monsters* m2 = &menv[b];

    if (m1->type == m2->type)
    {
        if (!m1->alive() || !m2->alive())
            return (false);

        return ( m1->name(DESC_PLAIN, true) < m2->name(DESC_PLAIN, true) );
    }

    const unsigned glyph1 = mons_char(m1->type);
    const unsigned glyph2 = mons_char(m2->type);
    if (glyph1 != glyph2)
    {
        return (glyph1 < glyph2);
    }

    return (m1->type < m2->type);
}

void debug_list_monsters()
{
    std::vector<std::string> mons;
    int nfound = 0;

    int mon_nums[MAX_MONSTERS];

    for (int i = 0; i < MAX_MONSTERS; ++i)
        mon_nums[i] = i;

    std::sort(mon_nums, mon_nums + MAX_MONSTERS, _sort_monster_list);

    int total_exp = 0, total_adj_exp = 0;

    std::string prev_name = "";
    int         count     = 0;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        const monsters *m = &menv[mon_nums[i]];
        if (!m->alive())
            continue;

        std::string name = m->name(DESC_PLAIN, true);

        if (prev_name != name && count > 0)
        {
            char buf[80];
            if (count > 1)
                sprintf(buf, "%d %s", count, pluralise(prev_name).c_str());
            else
                sprintf(buf, "%s", prev_name.c_str());
            mons.push_back(buf);

            count = 0;
        }
        nfound++;
        count++;
        prev_name = name;

        int exp = exper_value(m);
        total_exp += exp;

        if ((m->flags & (MF_WAS_NEUTRAL | MF_CREATED_FRIENDLY))
            || m->has_ench(ENCH_ABJ))
        {
            continue;
        }
        if (m->flags & MF_GOT_HALF_XP)
            exp /= 2;

        total_adj_exp += exp;
    }

    char buf[80];
    if (count > 1)
        sprintf(buf, "%d %s", count, pluralise(prev_name).c_str());
    else
        sprintf(buf, "%s", prev_name.c_str());
    mons.push_back(buf);

    mpr_comma_separated_list("Monsters: ", mons);

    if (total_adj_exp == total_exp)
    {
        mprf("%d monsters, %d total exp value",
             nfound, total_exp);
    }
    else
    {
        mprf("%d monsters, %d total exp value (%d adjusted)",
             nfound, total_exp, total_adj_exp);
    }
}

#endif

static void _rune_from_specs(const char* _specs, item_def &item)
{
    char specs[80];
    char obj_name[ ITEMNAME_SIZE ];

    item.sub_type = MISC_RUNE_OF_ZOT;

    if (strstr(_specs, "rune of zot"))
        strncpy(specs, _specs, strlen(_specs) - strlen(" of zot"));
    else
        strcpy(specs, _specs);

    if (strlen(specs) > 4)
    {
        for (int i = 0; i < NUM_RUNE_TYPES; ++i)
        {
            item.plus = i;

            strcpy(obj_name, item.name(DESC_PLAIN).c_str());

            if (strstr(strlwr(obj_name), specs))
                return;
        }
    }

    while (true)
    {
        mpr("[a] iron       [b] obsidian [c] icy      [d] bone     [e] slimy    [f] silver",
            MSGCH_PROMPT);
        mpr("[g] serpentine [h] elven    [i] golden   [j] decaying [k] barnacle [l] demonic",
            MSGCH_PROMPT);
        mpr("[m] abyssal    [n] glowing  [o] magical  [p] fiery    [q] dark     [r] buggy",
            MSGCH_PROMPT);
        mpr("Which rune (ESC to exit)? ", MSGCH_PROMPT);

        int keyin = tolower( get_ch() );

        if (keyin == ESCAPE  || keyin == ' '
            || keyin == '\r' || keyin == '\n')
        {
            canned_msg( MSG_OK );
            item.base_type = OBJ_UNASSIGNED;
            return;
        }

        if (keyin < 'a' || keyin > 'r')
            continue;

        rune_type types[] = {
            RUNE_DIS,
            RUNE_GEHENNA,
            RUNE_COCYTUS,
            RUNE_TARTARUS,
            RUNE_SLIME_PITS,
            RUNE_VAULTS,
            RUNE_SNAKE_PIT,
            RUNE_ELVEN_HALLS,
            RUNE_TOMB,
            RUNE_SWAMP,
            RUNE_SHOALS,

            RUNE_DEMONIC,
            RUNE_ABYSSAL,

            RUNE_MNOLEG,
            RUNE_LOM_LOBON,
            RUNE_CEREBOV,
            RUNE_GLOORX_VLOQ,
            NUM_RUNE_TYPES
        };

        item.plus = types[keyin - 'a'];

        return;
    }
}

static void _deck_from_specs(const char* _specs, item_def &item)
{
    std::string specs    = _specs;
    std::string type_str = "";

    trim_string(specs);

    if (specs.find(" of ") != std::string::npos)
    {
        type_str = specs.substr(specs.find(" of ") + 4);

        if (type_str.find("card") != std::string::npos
            || type_str.find("deck") != std::string::npos)
        {
            type_str = "";
        }

        trim_string(type_str);
    }

    misc_item_type types[] = {
        MISC_DECK_OF_ESCAPE,
        MISC_DECK_OF_DESTRUCTION,
        MISC_DECK_OF_DUNGEONS,
        MISC_DECK_OF_SUMMONING,
        MISC_DECK_OF_WONDERS,
        MISC_DECK_OF_PUNISHMENT,
        MISC_DECK_OF_WAR,
        MISC_DECK_OF_CHANGES,
        MISC_DECK_OF_DEFENCE,
        NUM_MISCELLANY
    };

    item.special  = DECK_RARITY_COMMON;
    item.sub_type = NUM_MISCELLANY;

    if (!type_str.empty())
    {
        for (int i = 0; types[i] != NUM_MISCELLANY; ++i)
        {
            item.sub_type = types[i];
            item.plus     = 1;
            init_deck(item);
            // Remove "plain " from front.
            std::string name = item.name(DESC_PLAIN).substr(6);
            item.props.clear();

            if (name.find(type_str) != std::string::npos)
                break;
        }
    }

    if (item.sub_type == NUM_MISCELLANY)
    {
        while (true)
        {
            mpr(
"[a] escape     [b] destruction [c] dungeons [d] summoning [e] wonders",
                MSGCH_PROMPT);
            mpr(
"[f] punishment [g] war         [h] changes  [i] defence",
                MSGCH_PROMPT);
            mpr("Which deck (ESC to exit)? ");

            int keyin = tolower( get_ch() );

            if (keyin == ESCAPE  || keyin == ' '
                || keyin == '\r' || keyin == '\n')
            {
                canned_msg( MSG_OK );
                item.base_type = OBJ_UNASSIGNED;
                return;
            }

            if (keyin < 'a' || keyin > 'i')
                continue;

            item.sub_type = types[keyin - 'a'];
            break;
        }
    }

    const char* rarities[] = {
        "plain",
        "ornate",
        "legendary",
        NULL
    };

    int rarity_val = -1;

    for (int i = 0; rarities[i] != NULL; ++i)
        if (specs.find(rarities[i]) != std::string::npos)
        {
            rarity_val = i;
            break;
        }

    if (rarity_val == -1)
    {
        while (true)
        {
            mpr("[a] plain [b] ornate [c] legendary? (ESC to exit)",
                MSGCH_PROMPT);

            int keyin = tolower( get_ch() );

            if (keyin == ESCAPE  || keyin == ' '
                || keyin == '\r' || keyin == '\n')
            {
                canned_msg( MSG_OK );
                item.base_type = OBJ_UNASSIGNED;
                return;
            }

            switch (keyin)
            {
            case 'p': keyin = 'a'; break;
            case 'o': keyin = 'b'; break;
            case 'l': keyin = 'c'; break;
            }

            if (keyin < 'a' || keyin > 'c')
                continue;

            rarity_val = keyin - 'a';
            break;
        }
    }

    const deck_rarity_type rarity =
        static_cast<deck_rarity_type>(DECK_RARITY_COMMON + rarity_val);
    item.special = rarity;

    int num = _debug_prompt_for_int("How many cards? ", false);

    if (num <= 0)
    {
        canned_msg( MSG_OK );
        item.base_type = OBJ_UNASSIGNED;
        return;
    }

    item.plus = num;

    init_deck(item);
}

static void _rune_or_deck_from_specs(const char* specs, item_def &item)
{
    if (strstr(specs, "rune"))
        _rune_from_specs(specs, item);
    else if (strstr(specs, "deck"))
        _deck_from_specs(specs, item);
}

static bool _book_from_spell(const char* specs, item_def &item)
{
    spell_type type = spell_by_name(specs, true);

    if (type == SPELL_NO_SPELL)
        return (false);

    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
        for (int j = 0; j < 8; ++j)
            if (which_spell_in_book(i, j) == type)
            {
                item.sub_type = i;
                return (true);
            }

    return (false);
}

//---------------------------------------------------------------
//
// create_spec_object
//
//---------------------------------------------------------------
void wizard_create_spec_object()
{
    char           specs[80];
    char           keyin;
    int            mon;

    object_class_type class_wanted   = OBJ_UNASSIGNED;

    int            thing_created;

    while (class_wanted == OBJ_UNASSIGNED)
    {
        mpr(") - weapons     ( - missiles  [ - armour  / - wands    ?  - scrolls",
            MSGCH_PROMPT);
        mpr("= - jewellery   ! - potions   : - books   | - staves   0  - The Orb",
            MSGCH_PROMPT);
        mpr("} - miscellany  X - corpses   % - food    $ - gold    ESC - exit",
            MSGCH_PROMPT);

        mpr("What class of item? ", MSGCH_PROMPT);

        keyin = toupper( get_ch() );

        if (keyin == ')')
            class_wanted = OBJ_WEAPONS;
        else if (keyin == '(')
            class_wanted = OBJ_MISSILES;
        else if (keyin == '[' || keyin == ']')
            class_wanted = OBJ_ARMOUR;
        else if (keyin == '/' || keyin == '\\')
            class_wanted = OBJ_WANDS;
        else if (keyin == '?')
            class_wanted = OBJ_SCROLLS;
        else if (keyin == '=' || keyin == '"')
            class_wanted = OBJ_JEWELLERY;
        else if (keyin == '!')
            class_wanted = OBJ_POTIONS;
        else if (keyin == ':' || keyin == '+')
            class_wanted = OBJ_BOOKS;
        else if (keyin == '|')
            class_wanted = OBJ_STAVES;
        else if (keyin == '0' || keyin == 'O')
            class_wanted = OBJ_ORBS;
        else if (keyin == '}' || keyin == '{')
            class_wanted = OBJ_MISCELLANY;
        else if (keyin == 'X' || keyin == '&')
            class_wanted = OBJ_CORPSES;
        else if (keyin == '%')
            class_wanted = OBJ_FOOD;
        else if (keyin == '$')
            class_wanted = OBJ_GOLD;
        else if (keyin == ESCAPE || keyin == ' '
                || keyin == '\r' || keyin == '\n')
        {
            canned_msg( MSG_OK );
            return;
        }
    }

    // Allocate an item to play with.
    thing_created = get_item_slot();
    if (thing_created == NON_ITEM)
    {
        mpr("Could not allocate item.");
        return;
    }

    // turn item into appropriate kind:
    if (class_wanted == OBJ_ORBS)
    {
        mitm[thing_created].base_type = OBJ_ORBS;
        mitm[thing_created].sub_type  = ORB_ZOT;
        mitm[thing_created].quantity  = 1;
    }
    else if (class_wanted == OBJ_GOLD)
    {
        int amount = _debug_prompt_for_int( "How much gold? ", true );
        if (amount <= 0)
        {
            canned_msg( MSG_OK );
            return;
        }

        mitm[thing_created].base_type = OBJ_GOLD;
        mitm[thing_created].sub_type = 0;
        mitm[thing_created].quantity = amount;
    }
    else if (class_wanted == OBJ_CORPSES)
    {
        mon = _debug_prompt_for_monster();

        if (mon == -1 || mon == MONS_PROGRAM_BUG)
        {
            mpr("No such monster.");
            return;
        }

        if (mons_weight(mon) <= 0)
        {
            if (!yesno("That monster doesn't leave corpses; make one "
                       "anyway?"))
            {
                return;
            }
        }

        if (mon >= MONS_DRACONIAN_CALLER && mon <= MONS_DRACONIAN_SCORCHER)
        {
            mpr("You can't make a draconian corpse by its job.");
            mon = MONS_DRACONIAN;
        }

        monsters dummy;
        dummy.type = mon;

        if (mons_genus(mon) == MONS_HYDRA)
            dummy.number = _debug_prompt_for_int("How many heads? ", false);

        if (fill_out_corpse(&dummy, mitm[thing_created], true) == -1)
        {
            mpr("Failed to create corpse.");
            mitm[thing_created].clear();
            return;
        }
    }
    else
    {
        mpr("What type of item? ", MSGCH_PROMPT);
        get_input_line( specs, sizeof( specs ) );

        std::string temp = specs;
        trim_string(temp);
        lowercase(temp);
        strcpy(specs, temp.c_str());

        if (specs[0] == '\0')
        {
            canned_msg( MSG_OK );
            return;
        }

        if (!get_item_by_name(&mitm[thing_created], specs, class_wanted, true))
        {
            mpr("No such item.");

            // Clean up item
            destroy_item(thing_created);
            return;
        }
    }

    // Deck colour (which control rarity) already set.
    if (!is_deck(mitm[thing_created]))
        item_colour( mitm[thing_created] );

    move_item_to_grid( &thing_created, you.pos() );

    if (thing_created != NON_ITEM)
    {
        // orig_monnum is used in corpses for things like the Animate
        // Dead spell, so leave it alone.
        if (class_wanted != OBJ_CORPSES)
            origin_acquired(mitm[thing_created], AQ_WIZMODE);
        canned_msg(MSG_SOMETHING_APPEARS);

        // Tell the stash tracker.
        maybe_update_stashes();
    }
}

bool get_item_by_name(item_def *item, char* specs,
                      object_class_type class_wanted, bool create_for_real)
{
    static int max_subtype[] =
    {
        NUM_WEAPONS,
        NUM_MISSILES,
        NUM_ARMOURS,
        NUM_WANDS,
        NUM_FOODS,
        0,              // unknown I
        NUM_SCROLLS,
        NUM_JEWELLERY,
        NUM_POTIONS,
        0,              // unknown II
        NUM_BOOKS,
        NUM_STAVES,
        0,              // Orbs         -- only one, handled specially
        NUM_MISCELLANY,
        0,              // corpses      -- handled specially
        0,              // gold         -- handled specially
        0,              // "gemstones"  -- no items of type
    };

    char           obj_name[ ITEMNAME_SIZE ];
    char*          ptr;
    int            best_index;
    int            type_wanted    = -1;
    int            special_wanted = 0;

    // In order to get the sub-type, we'll fill out the base type...
    // then we're going to iterate over all possible subtype values
    // and see if we get a winner. -- bwr
    item->base_type = class_wanted;
    item->sub_type  = 0;
    item->plus      = 0;
    item->plus2     = 0;
    item->special   = 0;
    item->flags     = 0;
    item->quantity  = 1;
    // Don't use set_ident_flags(), to avoid getting a spurious ID note.
    item->flags    |= ISFLAG_IDENT_MASK;

    if (class_wanted == OBJ_MISCELLANY)
    {
        // Leaves object unmodified if it wasn't a rune or deck.
        _rune_or_deck_from_specs(specs, *item);

        if (item->base_type == OBJ_UNASSIGNED)
        {
            // Rune or deck creation canceled, clean up item->
            return (false);
        }
    }

    if (!item->sub_type)
    {
        type_wanted = -1;
        best_index  = 10000;

        for (int i = 0; i < max_subtype[ item->base_type ]; ++i)
        {
            item->sub_type = i;
            strcpy(obj_name, item->name(DESC_PLAIN).c_str());

            ptr = strstr( strlwr(obj_name), specs );
            if (ptr != NULL)
            {
                // Earliest match is the winner.
                if (ptr - obj_name < best_index)
                {
                    if (create_for_real)
                        mpr(obj_name);
                    type_wanted = i;
                    best_index = ptr - obj_name;
                }
            }
        }

        if (type_wanted != -1)
        {
            item->sub_type = type_wanted;
            if (!create_for_real)
                return (true);
        }
        else
        {
            switch (class_wanted)
            {
            case OBJ_BOOKS:
                // Try if we get a match against a spell.
                if (_book_from_spell(specs, *item))
                    type_wanted = item->sub_type;
                break;

            case OBJ_WEAPONS:
            {
                // Try for fixed artefacts matching the name.
                unique_item_status_type exists;
                for (unsigned which = SPWPN_START_FIXEDARTS;
                     which <= SPWPN_END_FIXEDARTS; which++)
                {
                    exists = get_unique_item_status(OBJ_WEAPONS, which);
                    make_item_fixed_artefact(*item, false, which);
                    strcpy(obj_name, item->name(DESC_PLAIN).c_str());

                    ptr = strstr( strlwr(obj_name), specs );
                    if (ptr != NULL)
                    {
                        if (create_for_real)
                            mpr(obj_name);
                        return(true);
                    }
                    else
                    {
                        set_unique_item_status(OBJ_WEAPONS, which, exists);
                        do_uncurse_item(*item);
                        item->props.clear();
                    }
                }
            }
            // intentional fall-through for the unrandarts
            case OBJ_ARMOUR:
            case OBJ_JEWELLERY:
            {
                bool exists;
                for (int unrand = 0; unrand < NO_UNRANDARTS; unrand++)
                {
                    exists = does_unrandart_exist( unrand );
                    make_item_unrandart(*item, unrand);
                    strcpy(obj_name, item->name(DESC_PLAIN).c_str());

                    ptr = strstr( strlwr(obj_name), specs );
                    if (ptr != NULL && item->base_type == class_wanted)
                    {
                        if (create_for_real)
                            mpr(obj_name);
                        return(true);
                    }

                    set_unrandart_exist(unrand, exists);
                    do_uncurse_item(*item);
                    item->props.clear();
                }

                // Reset base type to class_wanted, if nothing found.
                item->base_type = class_wanted;
                item->sub_type  = 0;
                break;
            }

            default:
                break;
            }
        }

        if (type_wanted == -1)
        {
            // ds -- If specs is a valid int, try using that.
            //       Since zero is atoi's copout, the wizard
            //       must enter (subtype + 1).
            if (!(type_wanted = atoi(specs)))
                return (false);

            type_wanted--;

            item->sub_type = type_wanted;
        }
    }

    if (!create_for_real)
        return (true);

    switch (item->base_type)
    {
    case OBJ_MISSILES:
        item->quantity = 30;
        // intentional fall-through
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
    {
        char buf[80];
        mpr("What ego type? ", MSGCH_PROMPT);
        get_input_line( buf, sizeof( buf ) );

        if (buf[0] != '\0')
        {
            special_wanted = 0;
            best_index = 10000;

            for (int i = SPWPN_NORMAL + 1; i < SPWPN_DEBUG_RANDART; ++i)
            {
                item->special = i;
                strcpy(obj_name, item->name(DESC_PLAIN).c_str());

                ptr = strstr( strlwr(obj_name), strlwr(buf) );
                if (ptr != NULL)
                {
                    // earliest match is the winner
                    if (ptr - obj_name < best_index)
                    {
                        if (create_for_real)
                            mpr(obj_name);
                        special_wanted = i;
                        best_index = ptr - obj_name;
                    }
                }
            }

            item->special = special_wanted;
        }
        break;
    }

    case OBJ_BOOKS:
        if (item->sub_type == BOOK_MANUAL)
        {
            special_wanted =
                    _debug_prompt_for_skill( "A manual for which skill? " );

            if (special_wanted != -1)
            {
                item->plus  = special_wanted;
                item->plus2 = 3 + random2(15);
            }
            else
                mpr("Sorry, no books on that skill today.");
        }
        else if (type_wanted == BOOK_RANDART_THEME)
            make_book_theme_randart(*item, 0, 0, 5 + coinflip(), 20);
        else if (type_wanted == BOOK_RANDART_LEVEL)
        {
            int level = random_range(1, 9);
            int max_spells = 5 + level/3;
            make_book_level_randart(*item, level, max_spells);
        }
        break;

    case OBJ_WANDS:
        item->plus = 24;
        break;

    case OBJ_STAVES:
        if (item_is_rod(*item))
        {
            item->plus  = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
            item->plus2 = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
        }
        break;

    case OBJ_MISCELLANY:
        if (!is_rune(*item) && !is_deck(*item))
            item->plus = 50;
        break;

    case OBJ_POTIONS:
        item->quantity = 12;
        if (is_blood_potion(*item))
        {
            const char* prompt;
            if (item->sub_type == POT_BLOOD)
            {
                prompt = "# turns away from coagulation? "
                         "[ENTER for fully fresh] ";
            }
            else
            {
                prompt = "# turns away from rotting? "
                         "[ENTER for fully fresh] ";
            }
            int age = _debug_prompt_for_int(prompt, false);

            if (age <= 0)
                age = -1;
            else if (item->sub_type == POT_BLOOD)
                age += 500;

            init_stack_blood_potions(*item, age);
        }
        break;

    case OBJ_FOOD:
    case OBJ_SCROLLS:
        item->quantity = 12;
        break;

    case OBJ_JEWELLERY:
        if (jewellery_is_amulet(*item))
            break;

        switch (item->sub_type)
        {
        case RING_SLAYING:
            item->plus2 = 5;
            // intentional fall-through
        case RING_PROTECTION:
        case RING_EVASION:
        case RING_STRENGTH:
        case RING_DEXTERITY:
        case RING_INTELLIGENCE:
            item->plus = 5;
        default:
            break;
        }

    default:
        break;
    }

    return (true);
}

#ifdef WIZARD
const char* _prop_name[RAP_NUM_PROPERTIES] = {
    "Brand",
    "AC",
    "EV",
    "Str",
    "Int",
    "Dex",
    "Fire",
    "Cold",
    "Elec",
    "Pois",
    "Neg",
    "Mag",
    "SInv",
    "Inv",
    "Lev",
    "Blnk",
    "Tele",
    "Bers",
    "Map",
    "Nois",
    "NoSpl",
    "RndTl",
    "NoTel",
    "Anger",
    "Metab",
    "Mut",
    "Acc",
    "Dam",
    "Curse",
    "Stlth",
    "MP"
};

#define RAP_VAL_BOOL 0
#define RAP_VAL_POS  1
#define RAP_VAL_ANY  2

char _prop_type[RAP_NUM_PROPERTIES] = {
    RAP_VAL_POS,  //BRAND
    RAP_VAL_ANY,  //AC
    RAP_VAL_ANY,  //EVASION
    RAP_VAL_ANY,  //STRENGTH
    RAP_VAL_ANY,  //INTELLIGENCE
    RAP_VAL_ANY,  //DEXTERITY
    RAP_VAL_ANY,  //FIRE
    RAP_VAL_ANY,  //COLD
    RAP_VAL_BOOL, //ELECTRICITY
    RAP_VAL_BOOL, //POISON
    RAP_VAL_BOOL, //NEGATIVE_ENERGY
    RAP_VAL_POS,  //MAGIC
    RAP_VAL_BOOL, //EYESIGHT
    RAP_VAL_BOOL, //INVISIBLE
    RAP_VAL_BOOL, //LEVITATE
    RAP_VAL_BOOL, //BLINK
    RAP_VAL_BOOL, //CAN_TELEPORT
    RAP_VAL_BOOL, //BERSERK
    RAP_VAL_BOOL, //MAPPING
    RAP_VAL_POS,  //NOISES
    RAP_VAL_BOOL, //PREVENT_SPELLCASTING
    RAP_VAL_BOOL, //CAUSE_TELEPORTATION
    RAP_VAL_BOOL, //PREVENT_TELEPORTATION
    RAP_VAL_POS,  //ANGRY
    RAP_VAL_POS,  //METABOLISM
    RAP_VAL_POS,  //MUTAGENIC
    RAP_VAL_ANY,  //ACCURACY
    RAP_VAL_ANY,  //DAMAGE
    RAP_VAL_POS,  //CURSED
    RAP_VAL_ANY,  //STEALTH
    RAP_VAL_ANY   //MAGICAL_POWER
};

static void _tweak_randart(item_def &item)
{
    if (item_is_equipped(item))
    {
        mpr("You can't tweak the randart properties of an equipped item.",
            MSGCH_PROMPT);
        return;
    }

    randart_properties_t props;
    randart_wpn_properties(item, props);

    std::string prompt = "";

    for (int i = 0; i < RAP_NUM_PROPERTIES; ++i)
    {
        if (i % 8 == 0 && i != 0)
            prompt += "\n";

        char choice;
        char buf[80];

        if (i < 26)
            choice = 'A' + i;
        else
            choice = '1' + i - 26;

        if (props[i])
            sprintf(buf, "%c) <w>%-5s</w> ", choice, _prop_name[i]);
        else
            sprintf(buf, "%c) %-5s ", choice, _prop_name[i]);

        prompt += buf;
    }
    formatted_message_history(prompt, MSGCH_PROMPT, 0, 80);

    mpr("Change which field? ", MSGCH_PROMPT);

    char keyin = tolower( get_ch() );
    int  choice;

    if (isalpha(keyin))
        choice = keyin - 'a';
    else if (isdigit(keyin) && keyin != '0')
        choice = keyin - '1' + 26;
    else
        return;

    int val;
    switch (_prop_type[choice])
    {
    case RAP_VAL_BOOL:
        mprf(MSGCH_PROMPT, "Toggling %s to %s.", _prop_name[choice],
             props[choice] ? "off" : "on");
        randart_set_property(item, static_cast<randart_prop_type>(choice),
                             !props[choice]);
        break;

    case RAP_VAL_POS:
        mprf(MSGCH_PROMPT, "%s was %d.", _prop_name[choice], props[choice]);
        val = _debug_prompt_for_int("New value? ", true);

        if (val < 0)
        {
            mprf(MSGCH_PROMPT, "Value for %s must be non-negative",
                 _prop_name[choice]);
            return;
        }
        randart_set_property(item, static_cast<randart_prop_type>(choice),
                             val);
        break;
    case RAP_VAL_ANY:
        mprf(MSGCH_PROMPT, "%s was %d.", _prop_name[choice], props[choice]);
        val = _debug_prompt_for_int("New value? ", false);
        randart_set_property(item, static_cast<randart_prop_type>(choice),
                             val);
        break;
    }
}

void wizard_tweak_object(void)
{
    char specs[50];
    char keyin;

    int item = prompt_invent_item("Tweak which item? ", MT_INVLIST, -1);
    if (item == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (item == you.equip[EQ_WEAPON])
        you.wield_change = true;

    const bool is_art = is_artefact(you.inv[item])
        && !is_unrandom_artefact(you.inv[item]);

    while (true)
    {
        void *field_ptr = NULL;

        while (true)
        {
            mpr(you.inv[item].name(DESC_INVENTORY_EQUIP).c_str());

            if (is_art)
            {
                mpr("a - plus  b - plus2  c - art props  d - quantity  "
                    "e - flags  ESC - exit", MSGCH_PROMPT);
            }
            else
            {
                mpr("a - plus  b - plus2  c - special  d - quantity  "
                    "e - flags  ESC - exit", MSGCH_PROMPT);
            }

            mpr("Which field? ", MSGCH_PROMPT);

            keyin = tolower( get_ch() );

            if (keyin == 'a')
                field_ptr = &(you.inv[item].plus);
            else if (keyin == 'b')
                field_ptr = &(you.inv[item].plus2);
            else if (keyin == 'c')
                field_ptr = &(you.inv[item].special);
            else if (keyin == 'd')
                field_ptr = &(you.inv[item].quantity);
            else if (keyin == 'e')
                field_ptr = &(you.inv[item].flags);
            else if (keyin == ESCAPE || keyin == ' '
                    || keyin == '\r' || keyin == '\n')
            {
                canned_msg( MSG_OK );
                return;
            }

            if (keyin >= 'a' && keyin <= 'e')
                break;
        }

        if (is_art && keyin == 'c')
        {
            _tweak_randart(you.inv[item]);
            continue;
        }

        if (keyin != 'c' && keyin != 'e')
        {
            const short *const ptr = static_cast< short * >( field_ptr );
            mprf("Old value: %d (0x%04x)", *ptr, *ptr );
        }
        else
        {
            const long *const ptr = static_cast< long * >( field_ptr );
            mprf("Old value: %ld (0x%08lx)", *ptr, *ptr );
        }

        mpr("New value? ", MSGCH_PROMPT);
        get_input_line( specs, sizeof( specs ) );

        if (specs[0] == '\0')
            return;

        char *end;
        const bool hex = (keyin == 'e');
        int   new_value = strtol(specs, &end, hex ? 16 : 0);

        if (new_value == 0 && end == specs)
            return;

        if (keyin != 'c' && keyin != 'e')
        {
            short *ptr = static_cast< short * >( field_ptr );
            *ptr = new_value;
        }
        else
        {
            long *ptr = static_cast< long * >( field_ptr );
            *ptr = new_value;
        }
    }
}

// Returns whether an item of this type can be an artefact.
static bool _item_type_can_be_artefact( int type)
{
    return (type == OBJ_WEAPONS || type == OBJ_ARMOUR || type == OBJ_JEWELLERY
            || type == OBJ_BOOKS);
}

static bool _make_book_randart(item_def &book)
{
    char type;

    do
    {
        mpr("Make book fixed [t]heme or fixed [l]evel? ", MSGCH_PROMPT);
        type = tolower(getch());
    }
    while (type != 't' && type != 'l');

    if (type == 'l')
        return make_book_level_randart(book);
    else
        return make_book_theme_randart(book);
}

void wizard_value_randart()
{
    int i = prompt_invent_item( "Value of which randart?", MT_INVLIST, -1 );

    if (!prompt_failed(i))
    {
        if (!is_random_artefact( you.inv[i] ))
            mpr("That item is not an artefact!");
        else
            mprf("randart val: %d", randart_value(you.inv[i]));
    }
}

void wizard_create_all_artefacts()
{
    // Create all unrandarts. Start at 1; the unrandart at 0 is a dummy.
    for (int i = 1; i < NO_UNRANDARTS; ++i)
    {
        int islot = get_item_slot();
        if (islot == NON_ITEM)
            break;

        item_def& item = mitm[islot];
        make_item_unrandart(item, i);
        item.quantity = 1;
        set_ident_flags(item, ISFLAG_IDENT_MASK);

        msg::streams(MSGCH_DIAGNOSTICS) << "Made " << item.name(DESC_NOCAP_A)
                                        << std::endl;
        move_item_to_grid(&islot, you.pos());
    }

    // Create all fixed artefacts.
    for (int i = SPWPN_START_FIXEDARTS; i < SPWPN_START_NOGEN_FIXEDARTS; ++i)
    {
        int islot = get_item_slot();
        if (islot == NON_ITEM)
            break;

        item_def& item = mitm[islot];
        if (make_item_fixed_artefact(item, false, i))
        {
            item.quantity = 1;
            item_colour(item);
            set_ident_flags(item, ISFLAG_IDENT_MASK);
            move_item_to_grid( &islot, you.pos() );

            msg::streams(MSGCH_DIAGNOSTICS) << "Made "
                                            << item.name(DESC_NOCAP_A)
                                            << std::endl;
        }
    }

    // Create Horn of Geryon
    int islot = get_item_slot();
    if (islot != NON_ITEM)
    {
        item_def& item = mitm[islot];
        item.clear();
        item.base_type = OBJ_MISCELLANY;
        item.sub_type  = MISC_HORN_OF_GERYON;
        item.quantity  = 1;
        item_colour(item);

        set_ident_flags(item, ISFLAG_IDENT_MASK);
        move_item_to_grid(&islot, you.pos());

        msg::streams(MSGCH_DIAGNOSTICS) << "Made " << item.name(DESC_NOCAP_A)
                                        << std::endl;
    }
}

void wizard_make_object_randart()
{
    int i = prompt_invent_item( "Make an artefact out of which item?",
                                MT_INVLIST, -1 );

    if (prompt_failed(i))
        return;

    item_def &item(you.inv[i]);

    if (!_item_type_can_be_artefact(item.base_type))
    {
        mpr("That item cannot be turned into an artefact.");
        return;
    }

    if (you.weapon() == &item)
        you.wield_change = true;

    if (is_random_artefact(item))
    {
        if (!yesno("Is already a randart; wipe and re-use?"))
        {
            canned_msg(MSG_OK);
            return;
        }

        if (item_is_equipped(item))
            unuse_randart(item);

        item.special = 0;
        item.flags  &= ~ISFLAG_RANDART;
        item.props.clear();
    }

    mpr("Fake item as gift from which god (ENTER to leave alone): ",
        MSGCH_PROMPT);
    char name[80];
    if (!cancelable_get_line(name, sizeof( name )) && name[0])
    {
        god_type god = string_to_god(name, false);
        if (god == GOD_NO_GOD)
           mpr("No such god, leaving item origin alone.");
        else
        {
           mprf("God gift of %s.", god_name(god, false).c_str());
           item.orig_monnum = -god;
        }
    }

    if (item.base_type == OBJ_BOOKS)
    {
        if (!_make_book_randart(item))
        {
            mpr("Failed to turn book into randart.");
            return;
        }
    }
    else if (!make_item_randart(item))
    {
        mpr("Failed to turn item into randart.");
        return;
    }

    if (Options.autoinscribe_randarts)
    {
        add_autoinscription(item,
                            randart_auto_inscription(you.inv[i]));
    }

    // If equipped, apply new randart benefits.
    if (item_is_equipped(item))
        use_randart(item);

    mpr(item.name(DESC_INVENTORY_EQUIP).c_str());
}

// Returns whether an item of this type can be cursed.
static bool _item_type_can_be_cursed(int type)
{
    return (type == OBJ_WEAPONS || type == OBJ_ARMOUR || type == OBJ_JEWELLERY);
}

void wizard_uncurse_item()
{
    const int i = prompt_invent_item("(Un)curse which item?", MT_INVLIST, -1);

    if (!prompt_failed(i))
    {
        item_def& item(you.inv[i]);

        if (item_cursed(item))
            do_uncurse_item(item);
        else if (_item_type_can_be_cursed(item.base_type))
            do_curse_item(item);
        else
            mpr("That type of item cannot be cursed.");
    }
}

void wizard_heal(bool super_heal)
{
    if (super_heal)
    {
        // Clear more stuff and give a HP boost.
        you.magic_contamination = 0;
        you.duration[DUR_LIQUID_FLAMES] = 0;
        if (you.duration[DUR_MESMERISED])
        {
            you.duration[DUR_MESMERISED] = 0;
            you.mesmerised_by.clear();
        }
        // If we're repeating then do the HP increase all at once.
        int amount = 10;
        if (crawl_state.cmd_repeat_goal > 0)
        {
            amount *= crawl_state.cmd_repeat_goal;
            crawl_state.cancel_cmd_repeat();
        }

        inc_hp(amount, true);
    }

    // Clear most status ailments.
    you.rotting = 0;
    you.disease = 0;
    you.duration[DUR_CONF] = 0;
    you.duration[DUR_POISONING] = 0;
    set_hp(you.hp_max, false);
    set_mp(you.max_magic_points, false);
    set_hunger(10999, true);
    you.redraw_hit_points = true;
}

void wizard_set_hunger_state()
{
    std::string hunger_prompt =
        "Set hunger state to s(T)arving, (N)ear starving, (H)ungry";
    if (you.species == SP_GHOUL)
        hunger_prompt += " or (S)atiated";
    else
        hunger_prompt += ", (S)atiated, (F)ull or (E)ngorged";
    hunger_prompt += "? ";

    mprf(MSGCH_PROMPT, "%s", hunger_prompt.c_str());

    const int c = tolower(getch());

    // Values taken from food.cc.
    switch (c)
    {
    case 't': you.hunger = 500;   break;
    case 'n': you.hunger = 1200;  break;
    case 'h': you.hunger = 2400;  break;
    case 's': you.hunger = 5000;  break;
    case 'f': you.hunger = 8000;  break;
    case 'e': you.hunger = 12000; break;
    default:  canned_msg(MSG_OK); break;
    }
    food_change();
    if (you.species == SP_GHOUL && you.hunger_state >= HS_SATIATED)
        mpr("Ghouls can never be full or above!");
}

void wizard_spawn_control()
{
    mpr("(c)hange spawn rate or (s)pawn monsters? ", MSGCH_PROMPT);
    const int c = tolower(getch());

    char specs[256];
    bool done = false;

    if (c == 'c')
    {
        mprf(MSGCH_PROMPT, "Set monster spawn rate to what? (now %d, lower value = higher rate) ",
             env.spawn_random_rate);

        if (!cancelable_get_line(specs, sizeof(specs)))
        {
            const int rate = atoi(specs);
            if (rate)
            {
                env.spawn_random_rate = rate;
                done = true;
            }
        }
    }
    else if (c == 's')
    {
        // 50 spots are reserved for non-wandering monsters.
        int max_spawn = MAX_MONSTERS - 50;
        for (int i = 0; i < MAX_MONSTERS; ++i)
            if (menv[i].alive())
                max_spawn--;

        if (max_spawn <= 0)
        {
            mpr("Level already filled with monsters, get rid of some "
                "of them first.", MSGCH_PROMPT);
            return;
        }

        mprf(MSGCH_PROMPT, "Spawn how many random monsters (max %d)? ",
             max_spawn);

        if (!cancelable_get_line(specs, sizeof(specs)))
        {
            const int num = std::min(atoi(specs), max_spawn);
            if (num > 0)
            {
                int curr_rate = env.spawn_random_rate;
                // Each call to spawn_random_monsters() will spawn one with
                // the rate at 5 or less.
                env.spawn_random_rate = 5;

                for (int i = 0; i < num; ++i)
                    spawn_random_monsters();

                env.spawn_random_rate = curr_rate;
                done = true;
            }
        }
    }

    if (!done)
        canned_msg(MSG_OK);
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
        marker->set_property("desc", "wizard portal, dest = " + dst);
        env.markers.add(marker);
        dungeon_terrain_changed(you.pos(), DNGN_ENTER_PORTAL_VAULT, false);
    }
}

void wizard_identify_pack()
{
    mpr("You feel a rush of knowledge.");
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item = you.inv[i];
        if (is_valid_item(item))
        {
            set_ident_type(item, ID_KNOWN_TYPE);
            set_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }
    you.wield_change  = true;
    you.redraw_quiver = true;
}

void wizard_unidentify_pack()
{
    mpr("You feel a rush of antiknowledge.");
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item = you.inv[i];
        if (is_valid_item(item))
        {
            set_ident_type(item, ID_UNKNOWN_TYPE);
            unset_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }
    you.wield_change  = true;
    you.redraw_quiver = true;

    // Forget things that nearby monsters are carrying, as well.
    // (For use with the "give monster an item" wizard targetting
    // command.)
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters* mon = &menv[i];

        if (mon->alive() && mons_near(mon))
        {
            for (int j = 0; j < NUM_MONSTER_SLOTS; ++j)
            {
                if (mon->inv[j] == NON_ITEM)
                    continue;

                item_def &item = mitm[mon->inv[j]];

                if (!is_valid_item(item))
                    continue;

                set_ident_type(item, ID_UNKNOWN_TYPE);
                unset_ident_flags(item, ISFLAG_IDENT_MASK);
            }
        }
    }
}

void wizard_create_feature_number()
{
    char specs[256];
    int feat_num;
    mpr("Create which feature (by number)? ", MSGCH_PROMPT);

    if (!cancelable_get_line(specs, sizeof(specs))
        && (feat_num = atoi(specs)))
    {
        dungeon_feature_type feat = static_cast<dungeon_feature_type>(feat_num);
        dungeon_terrain_changed(you.pos(), feat, false);
#ifdef USE_TILE
        env.tile_flv(you.pos()).special = 0;
#endif
    }
    else
        canned_msg(MSG_OK);
}

void wizard_create_feature_name()
{
    char specs[256];
    mpr("Create which feature (by name)? ", MSGCH_PROMPT);
    if (!cancelable_get_line(specs, sizeof(specs)) && specs[0] != 0)
    {
        // Accept both "shallow_water" and "Shallow water"
        std::string name = lowercase_string(specs);
        name = replace_all(name, " ", "_");

        dungeon_feature_type feat = dungeon_feature_by_name(name);
        if (feat == DNGN_UNSEEN) // no exact match
        {
            std::vector<std::string> matches = dungeon_feature_matches(name);

            if (matches.empty())
            {
                mprf(MSGCH_DIAGNOSTICS, "No features matching '%s'",
                     name.c_str());
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

        mprf(MSGCH_DIAGNOSTICS, "Setting (%d,%d) to %s (%d)",
             you.pos().x, you.pos().y, name.c_str(), feat);
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

void wizard_gain_piety()
{
    if (you.religion == GOD_NO_GOD)
    {
        mpr("You are not religious!");
        return;
    }
    else if (you.religion == GOD_XOM)
    {
        you.piety = random2(MAX_PIETY+1); // reroll mood
        if (one_chance_in(5))
            you.gift_timeout = 0; // 20% chance to make Xom bored.
        else
            you.gift_timeout = random2(40) + random2(40);  // reroll interest

        const std::string new_xom_favour = describe_xom_favour();
        const std::string msg = "You are now " + new_xom_favour;
        god_speaks(you.religion, msg.c_str());
        return;
    }

    const int old_piety   = you.piety;
    const int old_penance = you.penance[you.religion];
    if (old_piety >= MAX_PIETY && !old_penance)
    {
        mprf("Your piety (%d) is already %s maximum.",
             old_piety, old_piety == MAX_PIETY ? "at" : "above");
    }

    // Even at maximum, you can still gain gifts.
    // Try at least once for maximum, or repeat until something
    // happens. Rarely, this might result in several gifts during the
    // same round!
    do
    {
        gain_piety(50);
    }
    while (old_piety < MAX_PIETY && old_piety == you.piety
           && old_penance == you.penance[you.religion]);

    if (old_penance)
    {
        mprf("Congratulations, your penance was decreased from %d to %d!",
             old_penance, you.penance[you.religion]);
    }
    else if (you.piety > old_piety)
    {
        mprf("Congratulations, your piety went from %d to %d!",
             old_piety, you.piety);
    }
}

void wizard_list_items()
{
    bool has_shops = false;

    for (int i = 0; i < MAX_SHOPS; ++i)
        if (env.shop[i].type != SHOP_UNASSIGNED)
        {
            has_shops = true;
            break;
        }

    if (has_shops)
    {
        mpr("Shop items:");

        for (int i = 0; i < MAX_SHOPS; ++i)
            if (env.shop[i].type != SHOP_UNASSIGNED)
            {
                for (stack_iterator si(coord_def(0, i+5)); si; ++si)
                    mpr(si->name(DESC_PLAIN, false, false, false).c_str());
            }

        mpr(EOL);
    }

    mpr("Item stacks (by location and top item):");
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        item_def &item(mitm[i]);
        if (!is_valid_item(item) || item.held_by_monster())
            continue;

        if (item.link != NON_ITEM)
        {
            mprf("(%2d,%2d): %s", item.pos.x, item.pos.y,
                 item.name(DESC_PLAIN, false, false, false).c_str() );
        }
    }

    mpr(EOL);
    mpr("Floor items (stacks only show top item):");

    const coord_def start(1,1), end(GXM-1, GYM-1);
    for (rectangle_iterator ri(start, end); ri; ++ri)
    {
        int item = igrd(*ri);
        if (item != NON_ITEM)
        {
            mprf("%3d at (%2d,%2d): %s", item, ri->x, ri->y,
                 mitm[item].name(DESC_PLAIN, false, false, false).c_str());
        }
    }
}
#endif

// Prints a number of useful (for debugging, that is) stats on monsters.
void debug_stethoscope(int mon)
{
    dist stth;
    coord_def stethpos;

    int i;

    if (mon != RANDOM_MONSTER)
        i = mon;
    else
    {
        mpr("Which monster?", MSGCH_PROMPT);

        direction(stth);

        if (!stth.isValid)
            return;

        if (stth.isTarget)
            stethpos = stth.target;
        else
            stethpos = you.pos() + stth.delta;

        if (env.cgrid(stethpos) != EMPTY_CLOUD)
        {
            mprf(MSGCH_DIAGNOSTICS, "cloud type: %d delay: %d",
                 env.cloud[ env.cgrid(stethpos) ].type,
                 env.cloud[ env.cgrid(stethpos) ].decay );
        }

        if (mgrd(stethpos) == NON_MONSTER)
        {
            mprf(MSGCH_DIAGNOSTICS, "item grid = %d", igrd(stethpos) );
            return;
        }

        i = mgrd(stethpos);
    }

    monsters& mons(menv[i]);

    // Print type of monster.
    mprf(MSGCH_DIAGNOSTICS, "%s (id #%d; type=%d loc=(%d,%d) align=%s)",
         mons.name(DESC_CAP_THE, true).c_str(),
         i, mons.type, mons.pos().x, mons.pos().y,
         ((mons.attitude == ATT_HOSTILE)       ? "hostile" :
          (mons.attitude == ATT_FRIENDLY)      ? "friendly" :
          (mons.attitude == ATT_NEUTRAL)       ? "neutral" :
          (mons.attitude == ATT_GOOD_NEUTRAL)  ? "good neutral"
                                               : "unknown alignment") );

    // Print stats and other info.
    mprf(MSGCH_DIAGNOSTICS,
         "HD=%d (%lu) HP=%d/%d AC=%d EV=%d MR=%d SP=%d "
         "energy=%d%s%s num=%d flags=%04lx",
         mons.hit_dice,
         mons.experience,
         mons.hit_points, mons.max_hit_points,
         mons.ac, mons.ev,
         mons_resist_magic( &mons ),
         mons.speed, mons.speed_increment,
         mons.base_monster != MONS_PROGRAM_BUG ? " base=" : "",
         mons.base_monster != MONS_PROGRAM_BUG ?
         get_monster_data(mons.base_monster)->name : "",
         mons.number, mons.flags );

    // Print habitat and behaviour information.
    const habitat_type hab = mons_habitat(&mons);

    mprf(MSGCH_DIAGNOSTICS,
         "hab=%s beh=%s(%d) foe=%s(%d) mem=%d target=(%d,%d) god=%s",
         ((hab == HT_LAND)                       ? "land" :
          (hab == HT_AMPHIBIOUS_LAND)            ? "land (amphibious)" :
          (hab == HT_AMPHIBIOUS_WATER)           ? "water (amphibious)" :
          (hab == HT_WATER)                      ? "water" :
          (hab == HT_LAVA)                       ? "lava" :
          (hab == HT_ROCK)                       ? "rock"
                                                 : "unknown"),
         (mons_is_sleeping(&mons)        ? "sleep" :
          mons_is_wandering(&mons)       ? "wander" :
          mons_is_seeking(&mons)         ? "seek" :
          mons_is_fleeing(&mons)         ? "flee" :
          mons_is_cornered(&mons)        ? "cornered" :
          mons_is_panicking(&mons)       ? "panic" :
          mons_is_lurking(&mons)         ? "lurk"
                                         : "unknown"),
         mons.behaviour,
         ((mons.foe == MHITYOU)            ? "you" :
          (mons.foe == MHITNOT)            ? "none" :
          (menv[mons.foe].type == -1)      ? "unassigned monster"
          : menv[mons.foe].name(DESC_PLAIN, true).c_str()),
         mons.foe,
         mons.foe_memory,
         mons.target.x, mons.target.y,
         god_name(mons.god).c_str() );

    // Print resistances.
    mprf(MSGCH_DIAGNOSTICS, "resist: fire=%d cold=%d elec=%d pois=%d neg=%d",
         mons_res_fire( &mons ),
         mons_res_cold( &mons ),
         mons_res_elec( &mons ),
         mons_res_poison( &mons ),
         mons_res_negative_energy( &mons ) );

    mprf(MSGCH_DIAGNOSTICS, "ench: %s",
         mons.describe_enchantments().c_str());

    std::ostringstream spl;
    const monster_spells &hspell_pass = mons.spells;
    bool found_spell = false;
    for (int k = 0; k < NUM_MONSTER_SPELL_SLOTS; ++k)
    {
        if (hspell_pass[k] != SPELL_NO_SPELL)
        {
            if (found_spell)
                spl << ", ";

            found_spell = true;

            spl << k << ": ";

            if (hspell_pass[k] >= NUM_SPELLS)
                spl << "buggy spell";
            else
                spl << spell_title(hspell_pass[k]);

            spl << " (" << static_cast<int>(hspell_pass[k]) << ")";
        }
    }
    if (found_spell)
        mprf(MSGCH_DIAGNOSTICS, "spells: %s", spl.str().c_str());

    if (mons.type == MONS_PLAYER_GHOST
        || mons.type == MONS_PANDEMONIUM_DEMON)
    {
        ASSERT(mons.ghost.get());
        const ghost_demon &ghost = *mons.ghost;
        mprf(MSGCH_DIAGNOSTICS, "Ghost damage: %d; brand: %d",
             ghost.damage, ghost.brand );
    }
}

#if DEBUG_ITEM_SCAN
static void _dump_item( const char *name, int num, const item_def &item )
{
    mpr(name, MSGCH_ERROR);

    mprf("    item #%d:  base: %d; sub: %d; plus: %d; plus2: %d; special: %ld",
         num, item.base_type, item.sub_type,
         item.plus, item.plus2, item.special );

    mprf("    quant: %d; colour: %d; ident: 0x%08lx; ident_type: %d",
         item.quantity, item.colour, item.flags,
         get_ident_type( item ) );

    mprf("    x: %d; y: %d; link: %d", item.pos.x, item.pos.y, item.link );

    crawl_state.cancel_cmd_repeat();
}

//---------------------------------------------------------------
//
// debug_item_scan
//
//---------------------------------------------------------------
void debug_item_scan( void )
{
    int   i;
    char  name[256];

    FixedVector<bool, MAX_ITEMS> visited;
    visited.init(false);

    // First we're going to check all the stacks on the level:
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        // Unlinked temporary items.
        if (*ri == coord_def())
            continue;

        // Looking for infinite stacks (ie more links than items allowed)
        // and for items which have bad coordinates (can't find their stack)
        for (int obj = igrd(*ri); obj != NON_ITEM; obj = mitm[obj].link)
        {
            if (obj < 0 || obj > MAX_ITEMS)
            {
                if (igrd(*ri) == obj)
                {
                    mprf(MSGCH_ERROR, "Igrd has invalid item index %d "
                                      "at (%d, %d)",
                         obj, ri->x, ri->y);
                }
                else
                {
                    mprf(MSGCH_ERROR, "Item in stack at (%d, %d) has ",
                                      "invalid link %d",
                         ri->x, ri->y, obj);
                }
                break;
            }

            // Check for invalid (zero quantity) items that are linked in.
            if (!is_valid_item( mitm[obj] ))
            {
                mprf(MSGCH_ERROR, "Linked invalid item at (%d,%d)!",
                     ri->x, ri->y);
                _dump_item( mitm[obj].name(DESC_PLAIN).c_str(), obj, mitm[obj] );
            }

            // Check that item knows what stack it's in.
            if (mitm[obj].pos != *ri)
            {
                mprf(MSGCH_ERROR,"Item position incorrect at (%d,%d)!",
                     ri->x, ri->y);
                _dump_item( mitm[obj].name(DESC_PLAIN).c_str(),
                            obj, mitm[obj] );
            }

            // If we run into a premarked item we're in real trouble,
            // this will also keep this from being an infinite loop.
            if (visited[obj])
            {
                mprf(MSGCH_ERROR,
                     "Potential INFINITE STACK at (%d, %d)", ri->x, ri->y);
                break;
            }
            visited[obj] = true;
        }
    }

    // Now scan all the items on the level:
    for (i = 0; i < MAX_ITEMS; ++i)
    {
        if (!is_valid_item( mitm[i] ))
            continue;

        strcpy(name, mitm[i].name(DESC_PLAIN).c_str());

        const monsters* mon = mitm[i].holding_monster();

        // Don't check (-1, -1) player items or (-2, -2) monster items
        // (except to make sure that the monster is alive).
        if (mitm[i].pos.origin())
        {
            mpr("Unlinked temporary item:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
        else if (mon != NULL && mon->type == -1)
        {
            mpr("Unlinked item held by dead monster:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
        else if ((mitm[i].pos.x > 0 || mitm[i].pos.y > 0) && !visited[i])
        {
            mpr("Unlinked item:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );

            if (!in_bounds(mitm[i].pos))
            {
                mprf(MSGCH_ERROR, "Item position (%d, %d) is out of bounds",
                     mitm[i].pos.x, mitm[i].pos.y);
            }
            else
            {
                mprf("igrd(%d,%d) = %d",
                     mitm[i].pos.x, mitm[i].pos.y, igrd( mitm[i].pos ));
            }

            // Let's check to see if it's an errant monster object:
            for (int j = 0; j < MAX_MONSTERS; ++j)
                for (int k = 0; k < NUM_MONSTER_SLOTS; ++k)
                {
                    if (menv[j].inv[k] == i)
                    {
                        mprf("Held by monster #%d: %s at (%d,%d)",
                             j, menv[j].name(DESC_CAP_A, true).c_str(),
                             menv[j].pos().x, menv[j].pos().y);
                    }
                }
        }

        // Current bad items of interest:
        //   -- armour and weapons with large enchantments/illegal special vals
        //
        //   -- items described as questionable (the class 100 bug)
        //
        //   -- eggplant is an illegal throwing weapon
        //
        //   -- bola is an illegal fixed artefact
        //
        //   -- items described as buggy (typically adjectives out of range)
        //      (note: covers buggy, bugginess, buggily, whatever else)
        //
        if (strstr( name, "questionable" ) != NULL
            || strstr( name, "eggplant" ) != NULL
            || strstr( name, "bola" ) != NULL
            || strstr( name, "bugg" ) != NULL)
        {
            mpr("Bad item:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
        else if ((mitm[i].base_type == OBJ_WEAPONS
                    && (abs(mitm[i].plus) > 30
                        || abs(mitm[i].plus2) > 30
                        || !is_random_artefact( mitm[i] )
                           && mitm[i].special >= 30
                           && mitm[i].special < 181))

                 || (mitm[i].base_type == OBJ_MISSILES
                     && (abs(mitm[i].plus) > 25
                         || !is_random_artefact( mitm[i] )
                            && mitm[i].special >= 30))

                 || (mitm[i].base_type == OBJ_ARMOUR
                     && (abs(mitm[i].plus) > 25
                         || !is_random_artefact( mitm[i] )
                            && mitm[i].special >= 30)))
        {
            mpr("Bad plus or special value:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
        else if (mitm[i].flags & ISFLAG_SUMMONED
                 && in_bounds(mitm[i].pos))
        {
            mpr("Summoned item on floor:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
    }

    // Quickly scan monsters for "program bug"s.
    for (i = 0; i < MAX_MONSTERS; ++i)
    {
        const monsters& monster = menv[i];

        if (monster.type == -1)
            continue;

        if (monster.name(DESC_PLAIN, true).find("questionable") !=
            std::string::npos)
        {
            mprf(MSGCH_ERROR, "Program bug detected!");
            mprf(MSGCH_ERROR,
                 "Buggy monster detected: monster #%d; position (%d,%d)",
                 i, monster.pos().x, monster.pos().y);
        }
    }
}

#endif

#if DEBUG_MONS_SCAN
static void _announce_level_prob(bool warned)
{
    if (!warned && Generating_Level)
    {
        mpr("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", MSGCH_ERROR);
        mpr("mgrd problem occurred during level generation", MSGCH_ERROR);

        _dump_levgen();
    }
}

static bool _inside_vault(const vault_placement& place, const coord_def &pos)
{
    const coord_def delta = pos - place.pos;

    return (delta.x >= 0 && delta.y >= 0
            && delta.x < place.size.x && delta.y < place.size.y);
}

static std::vector<std::string> _in_vaults(const coord_def &pos)
{
    std::vector<std::string> out;

    for (unsigned int i = 0; i < Level_Vaults.size(); ++i)
    {
        const vault_placement &vault = Level_Vaults[i];
        if (_inside_vault(vault, pos))
            out.push_back(vault.map.name);
    }

    for (unsigned int i = 0; i < Temp_Vaults.size(); ++i)
    {
        const vault_placement &vault = Temp_Vaults[i];
        if (_inside_vault(vault, pos))
            out.push_back(vault.map.name);
    }

    return (out);
}

void debug_mons_scan()
{
    std::vector<coord_def> bogus_pos;
    std::vector<int>       bogus_idx;

    bool warned = false;
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            const int mons = mgrd[x][y];
            if (mons == NON_MONSTER)
                continue;

            if (invalid_monster_index(mons))
            {
                mprf(MSGCH_ERROR, "mgrd at (%d, %d) has invalid monster "
                                  "index %d",
                     x, y, mons);
                continue;
            }

            const monsters *m = &menv[mons];
            const coord_def pos(x, y);
            if (m->pos() != pos)
            {
                bogus_pos.push_back(pos);
                bogus_idx.push_back(mons);

                _announce_level_prob(warned);
                mprf(MSGCH_WARN,
                     "Bogosity: mgrd at (%d,%d) points at %s, "
                     "but monster is at (%d,%d)",
                     x, y, m->name(DESC_PLAIN, true).c_str(),
                     m->pos().x, m->pos().y);
                if (!m->alive())
                    mpr("Additionally, it isn't alive.", MSGCH_WARN);
                warned = true;
            }
            else if (!m->alive())
            {
                _announce_level_prob(warned);
                mprf(MSGCH_WARN,
                     "mgrd at (%d,%d) points at dead monster %s",
                     x, y, m->name(DESC_PLAIN, true).c_str());
                warned = true;
            }
        }

    std::vector<int> floating_mons;
    bool             is_floating[MAX_MONSTERS];

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        is_floating[i] = false;

        const monsters *m = &menv[i];
        if (!m->alive())
            continue;

        coord_def pos = m->pos();

        if (!in_bounds(pos))
        {
            mprf(MSGCH_ERROR, "Out of bounds monster: %s at (%d, %d), "
                              "midx = %d",
                 m->full_name(DESC_PLAIN, true).c_str(),
                 pos.x, pos.y, i);
        }
        else if (mgrd(pos) != i)
        {
            floating_mons.push_back(i);
            is_floating[i] = true;

            _announce_level_prob(warned);
            mprf(MSGCH_WARN, "Floating monster: %s at (%d,%d), midx = %d",
                 m->full_name(DESC_PLAIN, true).c_str(),
                 pos.x, pos.y, i);
            warned = true;
            for (int j = 0; j < MAX_MONSTERS; ++j)
            {
                if (i == j)
                    continue;

                const monsters *m2 = &menv[j];

                if (m2->pos() != m->pos())
                    continue;

                std::string full = m2->full_name(DESC_PLAIN, true);
                if (m2->alive())
                {
                    mprf(MSGCH_WARN, "Also at (%d, %d): %s, midx = %d",
                         pos.x, pos.y, full.c_str(), j);
                }
                else if (m2->type != -1)
                {
                    mprf(MSGCH_WARN, "Dead mon also at (%d, %d): %s,"
                                     "midx = %d",
                         pos.x, pos.y, full.c_str(), j);
                }
            }
        } // if (mgrd(m->pos()) != i)

        for (int j = 0; j < NUM_MONSTER_SLOTS; ++j)
        {
            const int idx = m->inv[j];
            if (idx == NON_ITEM)
                continue;

            if (idx < 0 || idx > MAX_ITEMS)
            {
                mprf(MSGCH_ERROR, "Monster %s (%d, %d) has invalid item "
                                  "index %d in slot %d.",
                     m->full_name(DESC_PLAIN, true).c_str(),
                     pos.x, pos.y, idx, j);
                continue;
            }
            item_def &item(mitm[idx]);

            if (!is_valid_item(item))
            {
                _announce_level_prob(warned);
                warned = true;
                mprf(MSGCH_WARN, "Monster %s (%d, %d) holding invalid item in "
                                 "slot %d (midx = %d)",
                     m->full_name(DESC_PLAIN, true).c_str(),
                     pos.x, pos.y, j, i);
                continue;
            }

            const monsters* holder = item.holding_monster();

            if (holder == NULL)
            {
                _announce_level_prob(warned);
                warned = true;
                mprf(MSGCH_WARN, "Monster %s (%d, %d) holding non-monster "
                                 "item (midx = %d)",
                     m->full_name(DESC_PLAIN, true).c_str(),
                     pos.x, pos.y, i);
                _dump_item( item.name(DESC_PLAIN, false, true).c_str(),
                            idx, item );
                continue;
            }

            if (holder != m)
            {
                _announce_level_prob(warned);
                warned = true;
                mprf(MSGCH_WARN, "Monster %s (%d, %d) [midx = %d] holding "
                                 "item %s, but item thinks it's held by "
                                 "monster %s (%d, %d) [midx = %d]",
                     m->full_name(DESC_PLAIN, true).c_str(),
                     m->pos().x, m->pos().y, i,
                     item.name(DESC_PLAIN).c_str(),
                     holder->full_name(DESC_PLAIN, true).c_str(),
                     holder->pos().x, holder->pos().y, holder->mindex());

                bool found = false;
                for (int k = 0; k < NUM_MONSTER_SLOTS; ++k)
                {
                    if (holder->inv[k] == idx)
                    {
                        mpr("Other monster thinks it's holding the item, too.",
                            MSGCH_WARN);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    mpr("Other monster isn't holding it, though.", MSGCH_WARN);
            } // if (holder != m)
        } // for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
    } // for (int i = 0; i < MAX_MONSTERS; ++i)

    // No problems?
    if (!warned)
        return;

    // If this wasn't the result of generating a level then there's nothing
    // more to report.
    if (!Generating_Level)
    {
        // Force the dev to notice problems. :P
        more();
        return;
    }

    // No vaults to report on?
    if (Level_Vaults.size() == 0 && Temp_Vaults.size() == 0)
    {
        mpr("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", MSGCH_ERROR);
        // Force the dev to notice problems. :P
        more();
        return;
    }

    mpr("");

    for (unsigned int i = 0; i < floating_mons.size(); ++i)
    {
        const int       idx = floating_mons[i];
        const monsters* mon = &menv[idx];
        std::vector<std::string> vaults = _in_vaults(mon->pos());

        std::string str =
            make_stringf("Floating monster %s (%d, %d)",
                         mon->name(DESC_PLAIN, true).c_str(),
                         mon->pos().x, mon->pos().y);

        if (vaults.size() == 0)
            mprf(MSGCH_WARN, "%s not in any vaults.", str.c_str());
        else
        {
            mpr_comma_separated_list(str + " in vault(s) ", vaults,
                                     " and ", ", ", MSGCH_WARN);
        }
    }

    mpr("");

    for (unsigned int i = 0; i < bogus_pos.size(); ++i)
    {
        const coord_def pos = bogus_pos[i];
        const int       idx = bogus_idx[i];
        const monsters* mon = &menv[idx];

        std::string str =
            make_stringf("Bogus mgrd (%d, %d) pointing to %s",
                         pos.x, pos.y, mon->name(DESC_PLAIN, true).c_str());

        std::vector<std::string> vaults = _in_vaults(pos);

        if (vaults.size() == 0)
            mprf(MSGCH_WARN, "%s not in any vaults.", str.c_str());
        else
        {
            mpr_comma_separated_list(str + " in vault(s) ", vaults,
                                     " and ", ", ", MSGCH_WARN);
        }

        // Don't report on same monster twice.
        if (is_floating[idx])
            continue;

        str    = "Monster pointed to";
        vaults = _in_vaults(mon->pos());

        if (vaults.size() == 0)
            mprf(MSGCH_WARN, "%s not in any vaults.", str.c_str());
        else
        {
            mpr_comma_separated_list(str + " in vault(s) ", vaults,
                                     " and ", ", ", MSGCH_WARN);
        }
    }

    mpr("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", MSGCH_ERROR);
    // Force the dev to notice problems. :P
    more();
}
#endif


//---------------------------------------------------------------
//
// debug_item_statistics
//
//---------------------------------------------------------------
#ifdef WIZARD
static void _debug_acquirement_stats(FILE *ostat)
{
    if (grid_destroys_items(grd(you.pos())))
    {
        mpr("You must stand on a square which doesn't destroy items "
            "in order to do this.");
        return;
    }

    int p = get_item_slot(11);
    if (p == NON_ITEM)
    {
        mpr("Too many items on level.");
        return;
    }
    mitm[p].base_type = OBJ_UNASSIGNED;

    mesclr();
    mpr("[a] Weapons [b] Armours [c] Jewellery      [d] Books");
    mpr("[e] Staves  [f] Food    [g] Miscellaneous");
    mpr("What kind of item would you like to get acquirement stats on? ",
        MSGCH_PROMPT);

    object_class_type type;
    const int keyin = tolower( get_ch() );
    switch ( keyin )
    {
    case 'a': type = OBJ_WEAPONS;    break;
    case 'b': type = OBJ_ARMOUR;     break;
    case 'c': type = OBJ_JEWELLERY;  break;
    case 'd': type = OBJ_BOOKS;      break;
    case 'e': type = OBJ_STAVES;     break;
    case 'f': type = OBJ_FOOD;       break;
    case 'g': type = OBJ_MISCELLANY; break;
    default:
        canned_msg( MSG_OK );
        return;
    }

    const int num_itrs = _debug_prompt_for_int("How many iterations? ", true);

    if (num_itrs == 0)
    {
        canned_msg( MSG_OK );
        return;
    }

    int last_percent = 0;
    int acq_calls    = 0;
    int total_quant  = 0;
    int max_plus     = -127;
    int total_plus   = 0;
    int num_arts     = 0;

    int subtype_quants[256];
    int ego_quants[SPWPN_DEBUG_RANDART];

    memset(subtype_quants, 0, sizeof(subtype_quants));
    memset(ego_quants, 0, sizeof(ego_quants));

    for (int i = 0; i < num_itrs; ++i)
    {
        if (kbhit())
        {
            getch();
            mpr("Stopping early due to keyboard input.");
            break;
        }

        int item_index = NON_ITEM;

        if (!acquirement(type, AQ_WIZMODE, true, &item_index)
            || item_index == NON_ITEM
            || !is_valid_item(mitm[item_index]))
        {
            mpr("Acquirement failed, stopping early.");
            break;
        }

        item_def &item(mitm[item_index]);

        acq_calls++;
        total_quant += item.quantity;
        subtype_quants[item.sub_type] += item.quantity;

        max_plus    = std::max(max_plus, item.plus + item.plus2);
        total_plus += item.plus + item.plus2;

        if (is_artefact(item))
            num_arts++;

        if (type == OBJ_WEAPONS)
            ego_quants[get_weapon_brand(item)]++;
        else if (type == OBJ_ARMOUR)
            ego_quants[get_armour_ego_type(item)]++;

        destroy_item(item_index, true);

        int curr_percent = acq_calls * 100 / num_itrs;
        if (curr_percent > last_percent)
        {
            mesclr();
            mprf("%2d%% done.", curr_percent);
            last_percent = curr_percent;
        }
    }

    if (total_quant == 0 || acq_calls == 0)
    {
        mpr("No items generated.");
        return;
    }

    fprintf(ostat, "acquirement called %d times, total quantity = %d\n\n",
            acq_calls, total_quant);

    fprintf(ostat, "%5.2f%% artefacts.\n",
            100.0 * (float) num_arts / (float) acq_calls);

    if (type == OBJ_WEAPONS)
    {
        fprintf(ostat, "Maximum combined pluses: %d\n", max_plus);
        fprintf(ostat, "Average combined pluses: %5.2f\n\n",
                (float) total_plus / (float) acq_calls);

        fprintf(ostat, "Egos (including artefacts):\n");

        const char* names[] = {
            "normal",
            "flaming",
            "freezing",
            "holy wrath",
            "electrocution",
            "orc slaying",
            "dragon slaying",
            "venom",
            "protection",
            "draining",
            "speed",
            "vorpal",
            "flame",
            "frost",
            "vampiricism",
            "pain",
            "distortion",
            "reaching",
            "returning",
            "chaos",
            "confusion",
        };

        for (int i = 0; i <= SPWPN_CONFUSE; ++i)
            if (ego_quants[i] > 0)
            {
                fprintf(ostat, "%14s: %5.2f\n", names[i],
                        100.0 * (float) ego_quants[i] / (float) acq_calls);
            }

        fprintf(ostat, "\n\n");
    }
    else if (type == OBJ_ARMOUR)
    {
        fprintf(ostat, "Maximum plus: %d\n", max_plus);
        fprintf(ostat, "Average plus: %5.2f\n\n",
                (float) total_plus / (float) acq_calls);

        fprintf(ostat, "Egos (excluding artefacts):\n");

        const char* names[] = {
            "normal",
            "running",
            "fire resistance",
            "cold resistance",
            "poison resistance",
            "see invis",
            "darkness",
            "strength",
            "dexterity",
            "intelligence",
            "ponderous",
            "levitation",
            "magic reistance",
            "protection",
            "stealth",
            "resistance",
            "positive energy",
            "archmagi",
            "preservation",
            "reflection"
         };

        const int non_art = acq_calls - num_arts;
        for (int i = 0; i <= SPARM_REFLECTION; ++i)
        {
           if (ego_quants[i] > 0)
               fprintf(ostat, "%17s: %5.2f\n", names[i],
                       100.0 * (float) ego_quants[i] / (float) non_art);
        }
        fprintf(ostat, "\n\n");
    }

    item_def item;
    item.quantity  = 1;
    item.base_type = type;

    int max_width = 0;
    for (int i = 0; i < 256; ++i)
    {
        if (subtype_quants[i] == 0)
            continue;

        item.sub_type = i;

        std::string name = item.name(DESC_DBNAME, true, true);

        max_width = std::max(max_width, (int) name.length());
    }

    char format_str[80];
    sprintf(format_str, "%%%ds: %%6.2f\n", max_width);

    for (int i = 0; i < 256; ++i)
    {
        if (subtype_quants[i] == 0)
            continue;

        item.sub_type = i;

        std::string name = item.name(DESC_DBNAME, true, true);

        fprintf(ostat, format_str, name.c_str(),
                (float) subtype_quants[i] * 100.0 / (float) total_quant);
    }
    fprintf(ostat, "----------------------\n");
}

static void _debug_rap_stats(FILE *ostat)
{
    int i = prompt_invent_item( "Generate randart stats on which item?",
                                MT_INVLIST, -1 );

    if (i == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    // A copy of the item, rather than a reference to the inventory item,
    // so we can fiddle with the item at will.
    item_def item(you.inv[i]);

    // Start off with a non-artefact item.
    item.flags  &= ~ISFLAG_ARTEFACT_MASK;
    item.special = 0;
    item.props.clear();

    if (!make_item_randart(item))
    {
        mpr("Can't make a randart out of that type of item.");
        return;
    }

    // -1 = always bad, 1 = always good, 0 = depends on value
    const int good_or_bad[] = {
         1, //RAP_BRAND
         0, //RAP_AC
         0, //RAP_EVASION
         0, //RAP_STRENGTH
         0, //RAP_INTELLIGENCE
         0, //RAP_DEXTERITY
         0, //RAP_FIRE
         0, //RAP_COLD
         1, //RAP_ELECTRICITY
         1, //RAP_POISON
         1, //RAP_NEGATIVE_ENERGY
         1, //RAP_MAGIC
         1, //RAP_EYESIGHT
         1, //RAP_INVISIBLE
         1, //RAP_LEVITATE
         1, //RAP_BLINK
         1, //RAP_CAN_TELEPORT
         1, //RAP_BERSERK
         1, //RAP_MAPPING
        -1, //RAP_NOISES
        -1, //RAP_PREVENT_SPELLCASTING
        -1, //RAP_CAUSE_TELEPORTATION
        -1, //RAP_PREVENT_TELEPORTATION
        -1, //RAP_ANGRY
        -1, //RAP_METABOLISM
        -1, //RAP_MUTAGENIC
         0, //RAP_ACCURACY
         0, //RAP_DAMAGE
        -1, //RAP_CURSED
         0, //RAP_STEALTH
         0  //RAP_MAGICAL_POWER
    };

    // No bounds checking to speed things up a bit.
    int all_props[RAP_NUM_PROPERTIES];
    int good_props[RAP_NUM_PROPERTIES];
    int bad_props[RAP_NUM_PROPERTIES];
    for (i = 0; i < RAP_NUM_PROPERTIES; ++i)
    {
        all_props[i] = 0;
        good_props[i] = 0;
        bad_props[i] = 0;
    }

    int max_props         = 0, total_props         = 0;
    int max_good_props    = 0, total_good_props    = 0;
    int max_bad_props     = 0, total_bad_props     = 0;
    int max_balance_props = 0, total_balance_props = 0;

    int num_randarts = 0, bad_randarts = 0;

    randart_properties_t proprt;

    for (i = 0; i < RANDART_SEED_MASK; ++i)
    {
        if (kbhit())
        {
            getch();
            mpr("Stopping early due to keyboard input.");
            break;
        }

        item.special = i;

        // Generate proprt once and hand it off to randart_is_bad(),
        // so that randart_is_bad() doesn't generate it a second time.
        randart_wpn_properties( item, proprt );
        if (randart_is_bad(item, proprt))
        {
            bad_randarts++;
            continue;
        }

        num_randarts++;
        proprt[RAP_CURSED] = 0;

        int num_props = 0, num_good_props = 0, num_bad_props = 0;
        for (int j = 0; j < RAP_NUM_PROPERTIES; ++j)
        {
            const int val = proprt[j];
            if (val)
            {
                num_props++;
                all_props[j]++;
                switch (good_or_bad[j])
                {
                case -1:
                    num_bad_props++;
                    break;
                case 1:
                    num_good_props++;
                    break;
                case 0:
                    if (val > 0)
                    {
                        good_props[j]++;
                        num_good_props++;
                    }
                    else
                    {
                        bad_props[j]++;
                        num_bad_props++;
                    }
                }
            }
        }

        int balance = num_good_props - num_bad_props;

        max_props         = std::max(max_props, num_props);
        max_good_props    = std::max(max_good_props, num_good_props);
        max_bad_props     = std::max(max_bad_props, num_bad_props);
        max_balance_props = std::max(max_balance_props, balance);

        total_props         += num_props;
        total_good_props    += num_good_props;
        total_bad_props     += num_bad_props;
        total_balance_props += balance;

        if (i % 16777 == 0)
        {
            mesclr();
            float curr_percent = (float) i * 1000.0
                / (float) RANDART_SEED_MASK;
            mprf("%4.1f%% done.", curr_percent / 10.0);
        }

    }

    fprintf(ostat, "Randarts generated: %d valid, %d invalid\n\n",
            num_randarts, bad_randarts);

    fprintf(ostat, "max # of props = %d, avg # = %5.2f\n",
            max_props, (float) total_props / (float) num_randarts);
    fprintf(ostat, "max # of good props = %d, avg # = %5.2f\n",
            max_good_props, (float) total_good_props / (float) num_randarts);
    fprintf(ostat, "max # of bad props = %d, avg # = %5.2f\n",
            max_bad_props, (float) total_bad_props / (float) num_randarts);
    fprintf(ostat, "max (good - bad) props = %d, avg # = %5.2f\n\n",
            max_balance_props,
            (float) total_balance_props / (float) num_randarts);

    const char* rap_names[] = {
        "RAP_BRAND",
        "RAP_AC",
        "RAP_EVASION",
        "RAP_STRENGTH",
        "RAP_INTELLIGENCE",
        "RAP_DEXTERITY",
        "RAP_FIRE",
        "RAP_COLD",
        "RAP_ELECTRICITY",
        "RAP_POISON",
        "RAP_NEGATIVE_ENERGY",
        "RAP_MAGIC",
        "RAP_EYESIGHT",
        "RAP_INVISIBLE",
        "RAP_LEVITATE",
        "RAP_BLINK",
        "RAP_CAN_TELEPORT",
        "RAP_BERSERK",
        "RAP_MAPPING",
        "RAP_NOISES",
        "RAP_PREVENT_SPELLCASTING",
        "RAP_CAUSE_TELEPORTATION",
        "RAP_PREVENT_TELEPORTATION",
        "RAP_ANGRY",
        "RAP_METABOLISM",
        "RAP_MUTAGENIC",
        "RAP_ACCURACY",
        "RAP_DAMAGE",
        "RAP_CURSED",
        "RAP_STEALTH",
        "RAP_MAGICAL_POWER"
    };

    fprintf(ostat, "                            All    Good   Bad\n");
    fprintf(ostat, "                           --------------------\n");

    for (i = 0; i < RAP_NUM_PROPERTIES; ++i)
    {
        if (all_props[i] == 0)
            continue;

        fprintf(ostat, "%-25s: %5.2f%% %5.2f%% %5.2f%%\n", rap_names[i],
                (float) all_props[i] * 100.0 / (float) num_randarts,
                (float) good_props[i] * 100.0 / (float) num_randarts,
                (float) bad_props[i] * 100.0 / (float) num_randarts);
    }

    fprintf(ostat, "\n-----------------------------------------\n\n");
}

void debug_item_statistics( void )
{
    FILE *ostat = fopen("items.stat", "a");

    if (!ostat)
    {
        mprf(MSGCH_ERROR, "Can't write items.stat: %s", strerror(errno));
        return;
    }

    mpr("Generate stats for: [a] acquirement [b] randart properties");

    const int keyin = tolower( get_ch() );
    switch ( keyin )
    {
    case 'a': _debug_acquirement_stats(ostat); break;
    case 'b': _debug_rap_stats(ostat);
    default:
        canned_msg( MSG_OK );
        break;
    }

    fclose(ostat);
}
#endif

//---------------------------------------------------------------
//
// debug_add_skills
//
//---------------------------------------------------------------
#ifdef WIZARD
void wizard_exercise_skill(void)
{
    int skill = _debug_prompt_for_skill( "Which skill (by name)? " );

    if (skill == -1)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr("Exercising...");
        exercise(skill, 100);
    }
}
#endif

#ifdef WIZARD
void wizard_set_skill_level(void)
{
    int skill = _debug_prompt_for_skill( "Which skill (by name)? " );

    if (skill == -1)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr(skill_name(skill));
        int amount = _debug_prompt_for_int( "To what level? ", true );

        if (amount < 0)
            canned_msg( MSG_OK );
        else
        {
            const int old_amount = you.skills[skill];
            const int points = (skill_exp_needed( amount )
                                * species_skills( skill, you.species )) / 100;

            you.skill_points[skill] = points + 1;
            you.skills[skill] = amount;

            calc_total_skill_points();

            redraw_skill( you.your_name, player_title() );

            switch (skill)
            {
            case SK_FIGHTING:
                calc_hp();
                break;

            case SK_SPELLCASTING:
            case SK_INVOCATIONS:
            case SK_EVOCATIONS:
                calc_mp();
                break;

            case SK_DODGING:
                you.redraw_evasion = true;
                break;

            case SK_ARMOUR:
                you.redraw_armour_class = true;
                you.redraw_evasion = true;
                break;

            default:
                break;
            }

            mprf("%s %s to skill level %d.",
                 (old_amount < amount ? "Increased" :
                  old_amount > amount ? "Lowered"
                                      : "Reset"),
                 skill_name(skill), amount);

            if (skill == SK_STEALTH && amount == 27)
            {
                mpr("If you set the stealth skill to a value higher than 27, "
                    "hide mode is activated, and monsters won't notice you.");
            }
        }
    }
}
#endif


#ifdef WIZARD
void wizard_set_all_skills(void)
{
    int i;
    int amount =
            _debug_prompt_for_int( "Set all skills to what level? ", true );

    if (amount < 0)             // cancel returns -1 -- bwr
        canned_msg( MSG_OK );
    else
    {
        if (amount > 27)
            amount = 27;

        for (i = SK_FIGHTING; i < NUM_SKILLS; ++i)
        {
            if (is_invalid_skill(i))
                continue;

            const int points = (skill_exp_needed( amount )
                                * species_skills( i, you.species )) / 100;

            you.skill_points[i] = points + 1;
            you.skills[i] = amount;
        }

        redraw_skill( you.your_name, player_title() );

        calc_total_skill_points();

        calc_hp();
        calc_mp();

        you.redraw_armour_class = true;
        you.redraw_evasion = true;
    }
}
#endif

#ifdef WIZARD
extern mutation_def mutation_defs[];

bool wizard_add_mutation()
{
    bool success = false;
    char specs[80];

    if (player_mutation_level(MUT_MUTATION_RESISTANCE) > 0
        && !crawl_state.is_replaying_keys())
    {
        const char* msg;

        if (you.mutation[MUT_MUTATION_RESISTANCE] == 3)
            msg = "You are immune to mutations, remove immunity?";
        else
            msg = "You are resistant to mutations, remove resistance?";

        if (yesno(msg, true, 'n'))
        {
            you.mutation[MUT_MUTATION_RESISTANCE] = 0;
            crawl_state.cancel_cmd_repeat();
        }
    }

    bool force = yesno("Force mutation to happen?", true, 'n');

    if (player_mutation_level(MUT_MUTATION_RESISTANCE) == 3 && !force)
    {
        mpr("Can't mutate when immune to mutations without forcing it.");
        crawl_state.cancel_cmd_repeat();
        return (false);
    }

    bool god_gift = yesno("Treat mutation as god gift?", true, 'n');

    mpr("Which mutation (name, 'good', 'bad', 'any', 'xom')? ", MSGCH_PROMPT);
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
        return (false);

    strlwr(specs);

    mutation_type mutat = NUM_MUTATIONS;


    if (strcmp(specs, "good") == 0)
        mutat = RANDOM_GOOD_MUTATION;
    else if (strcmp(specs, "bad") == 0)
        mutat = RANDOM_BAD_MUTATION;
    else if (strcmp(specs, "any") == 0)
        mutat = RANDOM_MUTATION;
    else if (strcmp(specs, "xom") == 0)
        mutat = RANDOM_XOM_MUTATION;

    if (mutat != NUM_MUTATIONS)
    {
        int old_resist = player_mutation_level(MUT_MUTATION_RESISTANCE);

        success = mutate(mutat, true, force, god_gift);

        if (old_resist < player_mutation_level(MUT_MUTATION_RESISTANCE)
            && !force)
        {
            crawl_state.cancel_cmd_repeat("Your mutation resistance has "
                                          "increased.");
        }
        return (success);
    }

    std::vector<mutation_type> partial_matches;

    for (unsigned i = 0; true; ++i)
    {
        if (strcmp(specs, mutation_defs[i].wizname) == 0)
        {
            mutat = mutation_defs[i].mutation;
            break;
        }

        if (strstr(mutation_defs[i].wizname, specs))
            partial_matches.push_back(mutation_defs[i].mutation);

        // FIXME: hack, but I don't want to export the size
        // of the array...this is even worse.
        if (mutation_defs[i].mutation + 1 == NUM_MUTATIONS)
            break;
    }

    // If only one matching mutation, use that.
    if (mutat == NUM_MUTATIONS && partial_matches.size() == 1)
        mutat = partial_matches[0];

    if (mutat == NUM_MUTATIONS)
    {
        crawl_state.cancel_cmd_repeat();

        if (partial_matches.size() == 0)
            mpr("No matching mutation names.");
        else
        {
            std::vector<std::string> matches;

            for (unsigned int i = 0; i < partial_matches.size(); ++i)
                matches.push_back(get_mutation_def(partial_matches[i]).wizname);

            std::string prefix = "No exact match for mutation '" +
                std::string(specs) +  "', possible matches are: ";

            // Use mpr_comma_separated_list() because the list
            // might be *LONG*.
            mpr_comma_separated_list(prefix, matches, " and ", ", ",
                                     MSGCH_DIAGNOSTICS);
        }

        return (false);
    }
    else
    {
        mprf("Found #%d: %s (\"%s\")", (int) mutat,
             get_mutation_def(mutat).wizname,
             mutation_name(mutat, 1, false).c_str());

        const int levels =
            _debug_prompt_for_int("How many levels to increase or decrease? ",
                                  false);

        if (levels == 0)
        {
            canned_msg(MSG_OK);
            success = false;
        }
        else if (levels > 0)
        {
            for (int i = 0; i < levels; ++i)
                if (mutate(mutat, true, force, god_gift))
                    success = true;
        }
        else
        {
            for (int i = 0; i < -levels; ++i)
                if (delete_mutation(mutat, true, force))
                    success = true;
        }
    }

    return (success);
}
#endif

#ifdef WIZARD
void wizard_get_religion(void)
{
    char specs[80];

    mpr("Which god (by name)? ", MSGCH_PROMPT);
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
        return;

    strlwr(specs);

    god_type god = GOD_NO_GOD;

    for (int i = 1; i < NUM_GODS; ++i)
    {
        const god_type gi = static_cast<god_type>(i);
        if (lowercase_string(god_name(gi)).find(specs) != std::string::npos)
        {
            god = gi;
            break;
        }
    }

    if (god == GOD_NO_GOD)
        mpr("That god doesn't seem to be taking followers today.");
    else
    {
        dungeon_feature_type feat =
            static_cast<dungeon_feature_type>( DNGN_ALTAR_FIRST_GOD + god - 1 );
        dungeon_terrain_changed(you.pos(), feat, false);

        pray();
    }
}
#endif


void error_message_to_player(void)
{
    mpr("Oh dear. There appears to be a bug in the program.");
    mpr("I suggest you leave this level then save as soon as possible.");

}

#ifdef WIZARD

static int _create_fsim_monster(int mtype, int hp)
{
    const int mi =
        create_monster(
            mgen_data::hostile_at(
                static_cast<monster_type>(mtype), you.pos()));

    if (mi == -1)
        return (mi);

    monsters *mon = &menv[mi];
    mon->hit_points = mon->max_hit_points = hp;
    return (mi);
}

static skill_type _fsim_melee_skill(const item_def *item)
{
    skill_type sk = SK_UNARMED_COMBAT;
    if (item)
        sk = weapon_skill(*item);
    return (sk);
}

static void _fsim_set_melee_skill(int skill, const item_def *item)
{
    you.skills[_fsim_melee_skill(item)] = skill;
    you.skills[SK_FIGHTING]            = skill * 15 / 27;
}

static void _fsim_set_ranged_skill(int skill, const item_def *item)
{
    you.skills[range_skill(*item)] = skill;
    you.skills[SK_THROWING]        = skill * 15 / 27;
}

static void _fsim_item(FILE *out,
                       bool melee,
                       const item_def *weap,
                       const char *wskill,
                       unsigned long damage,
                       long iterations, long hits,
                       int maxdam, unsigned long time)
{
    double hitdam = hits? double(damage) / hits : 0.0;
    int avspeed = static_cast<int>(time / iterations);
    fprintf(out,
            " %-5s|  %3ld%%    |  %5.2f |    %5.2f  |"
            "   %5.2f |   %3d   |   %2ld\n",
            wskill,
            100 * hits / iterations,
            double(damage) / iterations,
            hitdam,
            double(damage) * player_speed() / avspeed / iterations,
            maxdam,
            time / iterations);
}

static void _fsim_defence_item(FILE *out, long cum, int hits, int max,
                               int speed, long iters)
{
    // AC | EV | Arm | Dod | Acc | Av.Dam | Av.HitDam | Eff.Dam | Max.Dam | Av.Time
    fprintf(out, "%2d   %2d    %2d   %2d   %3ld%%   %5.2f      %5.2f      %5.2f      %3d"
            "       %2d\n",
            player_AC(),
            player_evasion(),
            you.skills[SK_DODGING],
            you.skills[SK_ARMOUR],
            100 * hits / iters,
            double(cum) / iters,
            hits? double(cum) / hits : 0.0,
            double(cum) / iters * speed / 10,
            max,
            100 / speed);
}


static bool _fsim_ranged_combat(FILE *out, int wskill, int mi,
                                const item_def *item, int missile_slot)
{
    monsters &mon = menv[mi];
    unsigned long cumulative_damage = 0L;
    unsigned long time_taken = 0L;
    long hits = 0L;
    int maxdam = 0;

    const int thrown = missile_slot == -1 ? you.m_quiver->get_fire_item() : missile_slot;
    if (thrown == ENDOFPACK || thrown == -1)
    {
        mprf("No suitable missiles for combat simulation.");
        return (false);
    }

    _fsim_set_ranged_skill(wskill, item);

    no_messages mx;
    const long iter_limit = Options.fsim_rounds;
    const int hunger = you.hunger;
    for (long i = 0; i < iter_limit; ++i)
    {
        mon.hit_points = mon.max_hit_points;
        bolt beam;
        you.time_taken = player_speed();

        // throw_it() will decrease quantity by 1
        inc_inv_item_quantity(thrown, 1);

        beam.target = mon.pos();
        if (throw_it(beam, thrown, true, DEBUG_COOKIE))
            hits++;

        you.hunger = hunger;
        time_taken += you.time_taken;

        int damage = (mon.max_hit_points - mon.hit_points);
        cumulative_damage += damage;
        if (damage > maxdam)
            maxdam = damage;
    }
    _fsim_item(out, false, item, make_stringf("%2d", wskill).c_str(),
               cumulative_damage, iter_limit, hits, maxdam, time_taken);

    return (true);
}

static bool _fsim_mon_melee(FILE *out, int dodge, int armour, int mi)
{
    you.skills[SK_DODGING] = dodge;
    you.skills[SK_ARMOUR]  = armour;

    const int yhp  = you.hp;
    const int ymhp = you.hp_max;
    unsigned long cumulative_damage = 0L;
    long hits = 0L;
    int maxdam = 0;
    no_messages mx;

    for (long i = 0; i < Options.fsim_rounds; ++i)
    {
        you.hp = you.hp_max = 5000;
        monster_attack(&menv[mi]);
        const int damage = you.hp_max - you.hp;
        if (damage)
            hits++;
        cumulative_damage += damage;
        if (damage > maxdam)
            maxdam = damage;
    }

    you.hp = yhp;
    you.hp_max = ymhp;

    _fsim_defence_item(out, cumulative_damage, hits, maxdam, menv[mi].speed,
                       Options.fsim_rounds);
    return (true);
}

static bool _fsim_melee_combat(FILE *out, int wskill, int mi,
                               const item_def *item)
{
    monsters &mon = menv[mi];
    unsigned long cumulative_damage = 0L;
    unsigned long time_taken = 0L;
    long hits = 0L;
    int maxdam = 0;

    _fsim_set_melee_skill(wskill, item);

    no_messages mx;
    const long iter_limit = Options.fsim_rounds;
    const int hunger = you.hunger;
    for (long i = 0; i < iter_limit; ++i)
    {
        mon.hit_points = mon.max_hit_points;
        you.time_taken = player_speed();
        if (you_attack(mi, true))
            hits++;

        you.hunger = hunger;
        time_taken += you.time_taken;

        int damage = (mon.max_hit_points - mon.hit_points);
        cumulative_damage += damage;
        if (damage > maxdam)
            maxdam = damage;
    }
    _fsim_item(out, true, item, make_stringf("%2d", wskill).c_str(),
               cumulative_damage, iter_limit, hits, maxdam, time_taken);

    return (true);
}

static bool debug_fight_simulate(FILE *out, int wskill, int mi, int miss_slot)
{
    int weapon = you.equip[EQ_WEAPON];
    const item_def *iweap = weapon != -1? &you.inv[weapon] : NULL;

    if (iweap && iweap->base_type == OBJ_WEAPONS && is_range_weapon(*iweap))
        return _fsim_ranged_combat(out, wskill, mi, iweap, miss_slot);
    else
        return _fsim_melee_combat(out, wskill, mi, iweap);
}

static const item_def *_fsim_weap_item()
{
    const int weap = you.equip[EQ_WEAPON];
    if (weap == -1)
        return NULL;

    return &you.inv[weap];
}

static std::string _fsim_wskill(int missile_slot)
{
    const item_def *iweap = _fsim_weap_item();
    if (!iweap && missile_slot != -1)
        return skill_name(range_skill(you.inv[missile_slot]));

    if (iweap && iweap->base_type == OBJ_WEAPONS)
    {
        if (is_range_weapon(*iweap))
            return skill_name(range_skill(*iweap));

        return skill_name(_fsim_melee_skill(iweap));
    }
    return skill_name(SK_UNARMED_COMBAT);
}

static std::string _fsim_weapon(int missile_slot)
{
    std::string item_buf;
    if (you.equip[EQ_WEAPON] != -1 || missile_slot != -1)
    {
        if (you.equip[EQ_WEAPON] != -1)
        {
            const item_def &weapon = you.inv[ you.equip[EQ_WEAPON] ];
            item_buf = weapon.name(DESC_PLAIN, true);
            if (is_range_weapon(weapon))
            {
                const int missile =
                    (missile_slot == -1 ? you.m_quiver->get_fire_item()
                                        : missile_slot);

                if (missile < ENDOFPACK && missile >= 0)
                {
                    return item_buf + " with "
                           + you.inv[missile].name(DESC_PLAIN);
                }
            }
        }
        else
            return you.inv[missile_slot].name(DESC_PLAIN);
    }
    else
        return "unarmed";

    return item_buf;
}

static std::string _fsim_time_string()
{
    time_t curr_time = time(NULL);
    struct tm *ltime = TIME_FN(&curr_time);
    if (ltime)
    {
        char buf[100];
        snprintf(buf, sizeof buf, "%4d%02d%02d/%2d:%02d:%02d",
                 ltime->tm_year + 1900,
                 ltime->tm_mon  + 1,
                 ltime->tm_mday,
                 ltime->tm_hour,
                 ltime->tm_min,
                 ltime->tm_sec);
        return (buf);
    }
    return ("");
}

static void _fsim_mon_stats(FILE *o, const monsters &mon)
{
    fprintf(o, "Monster   : %s\n", mon.name(DESC_PLAIN, true).c_str());
    fprintf(o, "HD        : %d\n", mon.hit_dice);
    fprintf(o, "AC        : %d\n", mon.ac);
    fprintf(o, "EV        : %d\n", mon.ev);
}

static void _fsim_title(FILE *o, int mon, int ms)
{
    fprintf(o, CRAWL " version " VERSION "\n\n");
    fprintf(o, "Combat simulation: %s %s vs. %s (%ld rounds) (%s)\n",
            species_name(you.species, you.experience_level).c_str(),
            you.class_name,
            menv[mon].name(DESC_PLAIN, true).c_str(),
            Options.fsim_rounds,
            _fsim_time_string().c_str());

    fprintf(o, "Experience: %d\n", you.experience_level);
    fprintf(o, "Strength  : %d\n", you.strength);
    fprintf(o, "Intel.    : %d\n", you.intel);
    fprintf(o, "Dexterity : %d\n", you.dex);
    fprintf(o, "Base speed: %d\n", player_speed());
    fprintf(o, "\n");

    _fsim_mon_stats(o, menv[mon]);

    fprintf(o, "\n");
    fprintf(o, "Weapon    : %s\n", _fsim_weapon(ms).c_str());
    fprintf(o, "Skill     : %s\n", _fsim_wskill(ms).c_str());
    fprintf(o, "\n");
    fprintf(o, "Skill | Accuracy | Av.Dam | Av.HitDam | Eff.Dam | Max.Dam | Av.Time\n");
}

static void _fsim_defence_title(FILE *o, int mon)
{
    fprintf(o, CRAWL " version " VERSION "\n\n");
    fprintf(o, "Combat simulation: %s vs. %s %s (%ld rounds) (%s)\n",
            menv[mon].name(DESC_PLAIN).c_str(),
            species_name(you.species, you.experience_level).c_str(),
            you.class_name,
            Options.fsim_rounds,
            _fsim_time_string().c_str());
    fprintf(o, "Experience: %d\n", you.experience_level);
    fprintf(o, "Strength  : %d\n", you.strength);
    fprintf(o, "Intel.    : %d\n", you.intel);
    fprintf(o, "Dexterity : %d\n", you.dex);
    fprintf(o, "Base speed: %d\n", player_speed());
    fprintf(o, "\n");
    _fsim_mon_stats(o, menv[mon]);
    fprintf(o, "\n");
    fprintf(o, "AC | EV | Dod | Arm | Acc | Av.Dam | Av.HitDam | Eff.Dam | Max.Dam | Av.Time\n");
}

static int cap_stat(int stat)
{
    return (stat <  1  ?   1 :
            stat > 127 ? 127
                       : stat);
}

static bool _fsim_mon_hit_you(FILE *ostat, int mindex, int)
{
    _fsim_defence_title(ostat, mindex);

    for (int sk = 0; sk <= 27; ++sk)
    {
        mesclr();
        mprf("Calculating average damage for %s at dodging %d",
             menv[mindex].name(DESC_PLAIN).c_str(),
             sk);

        if (!_fsim_mon_melee(ostat, sk, 0, mindex))
            return (false);

        fflush(ostat);
        // Not checking in the combat loop itself; that would be more responsive
        // for the user, but slow down the sim with all the calls to kbhit().
        if (kbhit() && getch() == 27)
        {
            mprf("Canceling simulation\n");
            return (false);
        }
    }

    for (int sk = 0; sk <= 27; ++sk)
    {
        mesclr();
        mprf("Calculating average damage for %s at armour %d",
             menv[mindex].name(DESC_PLAIN).c_str(),
             sk);

        if (!_fsim_mon_melee(ostat, 0, sk, mindex))
            return (false);

        fflush(ostat);
        // Not checking in the combat loop itself; that would be more responsive
        // for the user, but slow down the sim with all the calls to kbhit().
        if (kbhit() && getch() == 27)
        {
            mprf("Canceling simulation\n");
            return (false);
        }
    }

    mprf("Done defence simulation with %s",
         menv[mindex].name(DESC_PLAIN).c_str());

    return (true);
}

static bool _fsim_you_hit_mon(FILE *ostat, int mindex, int missile_slot)
{
    _fsim_title(ostat, mindex, missile_slot);
    for (int wskill = 0; wskill <= 27; ++wskill)
    {
        mesclr();
        mprf("Calculating average damage for %s at skill %d",
             _fsim_weapon(missile_slot).c_str(), wskill);

        if (!debug_fight_simulate(ostat, wskill, mindex, missile_slot))
            return (false);

        fflush(ostat);
        // Not checking in the combat loop itself; that would be more responsive
        // for the user, but slow down the sim with all the calls to kbhit().
        if (kbhit() && getch() == 27)
        {
            mprf("Canceling simulation\n");
            return (false);
        }
    }
    mprf("Done fight simulation with %s", _fsim_weapon(missile_slot).c_str());
    return (true);
}

static bool debug_fight_sim(int mindex, int missile_slot,
                            bool (*combat)(FILE *, int mind, int mslot))
{
    FILE *ostat = fopen("fight.stat", "a");
    if (!ostat)
    {
        mprf(MSGCH_ERROR, "Can't write fight.stat: %s", strerror(errno));
        return (false);
    }

    bool success = true;
    FixedVector<unsigned char, 50> skill_backup = you.skills;
    int ystr = you.strength,
        yint = you.intel,
        ydex = you.dex;
    int yxp  = you.experience_level;

    for (int i = SK_FIGHTING; i < NUM_SKILLS; ++i)
        you.skills[i] = 0;

    you.experience_level = Options.fsim_xl;
    if (you.experience_level < 1)
        you.experience_level = 1;
    if (you.experience_level > 27)
        you.experience_level = 27;

    you.strength = cap_stat(Options.fsim_str);
    you.intel    = cap_stat(Options.fsim_int);
    you.dex      = cap_stat(Options.fsim_dex);

    combat(ostat, mindex, missile_slot);

    you.skills = skill_backup;
    you.strength = ystr;
    you.intel    = yint;
    you.dex      = ydex;
    you.experience_level = yxp;

    fprintf(ostat, "-----------------------------------\n\n");
    fclose(ostat);

    return (success);
}

int fsim_kit_equip(const std::string &kit)
{
    int missile_slot = -1;

    std::string::size_type ammo_div = kit.find("/");
    std::string weapon = kit;
    std::string missile;
    if (ammo_div != std::string::npos)
    {
        weapon = kit.substr(0, ammo_div);
        missile = kit.substr(ammo_div + 1);
        trim_string(weapon);
        trim_string(missile);
    }

    if (!weapon.empty())
    {
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if (!is_valid_item(you.inv[i]))
                continue;

            if (you.inv[i].name(DESC_PLAIN).find(weapon) != std::string::npos)
            {
                if (i != you.equip[EQ_WEAPON])
                {
                    wield_weapon(true, i, false);
                    if (i != you.equip[EQ_WEAPON])
                        return -100;
                }
                break;
            }
        }
    }
    else if (you.equip[EQ_WEAPON] != -1)
        unwield_item(false);

    if (!missile.empty())
    {
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if (!is_valid_item(you.inv[i]))
                continue;

            if (you.inv[i].name(DESC_PLAIN).find(missile) != std::string::npos)
            {
                missile_slot = i;
                break;
            }
        }
    }

    return (missile_slot);
}

// Writes statistics about a fight to fight.stat in the current directory.
// For fight purposes, a punching bag is summoned and given lots of hp, and the
// average damage the player does to the p. bag over 10000 hits is noted,
// advancing the weapon skill from 0 to 27, and keeping fighting skill to 2/5
// of current weapon skill.
void debug_fight_statistics(bool use_defaults, bool defence)
{
    int punching_bag = get_monster_by_name(Options.fsim_mons);
    if (punching_bag == -1 || punching_bag == MONS_PROGRAM_BUG)
        punching_bag = MONS_WORM;

    int mindex = _create_fsim_monster(punching_bag, 500);
    if (mindex == -1)
    {
        mprf("Failed to create punching bag");
        return;
    }

    you.exp_available = 0;

    if (!use_defaults || defence)
    {
        debug_fight_sim(mindex, -1,
                        defence? _fsim_mon_hit_you : _fsim_you_hit_mon);
    }
    else
    {
        for (int i = 0, size = Options.fsim_kit.size(); i < size; ++i)
        {
            int missile = fsim_kit_equip(Options.fsim_kit[i]);
            if (missile == -100)
            {
                mprf("Aborting sim on %s", Options.fsim_kit[i].c_str());
                break;
            }
            if (!debug_fight_sim(mindex, missile, _fsim_you_hit_mon))
                break;
        }
    }
    monster_die(&menv[mindex], KILL_DISMISSED, NON_MONSTER);
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
    int trap_slot = find_trap_slot();
    trap_type trap = TRAP_UNASSIGNED;
    int gridch = grd(you.pos());

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

    mprf(MSGCH_PROMPT, "What kind of trap? ");
    get_input_line( requested_trap, sizeof( requested_trap ) );
    if (!*requested_trap)
        return;

    strlwr(requested_trap);
    std::vector<trap_type>   matches;
    std::vector<std::string> match_names;
    for (int t = TRAP_DART; t < NUM_TRAPS; ++t)
    {
        const trap_type tr = static_cast<trap_type>(t);
        const char* tname = trap_name(tr);
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

    place_specific_trap(you.pos(), trap);

    mprf("Created a %s trap, marked it undiscovered", trap_name(trap));

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

    mprf(MSGCH_PROMPT, "What kind of shop? ");
    get_input_line( requested_shop, sizeof( requested_shop ) );
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

    place_spec_shop(you.your_level, you.pos(),
                    new_shop_type, representative);
    link_items();
    mprf("Done.");
}

void wizard_set_stats()
{
    char buf[80];
    mprf(MSGCH_PROMPT, "Enter values for Str, Int, Dex (space separated): ");
    if (cancelable_get_line_autohist(buf, sizeof buf))
        return;

    int sstr = you.strength,
        sdex = you.dex,
        sint = you.intel;

    sscanf(buf, "%d %d %d", &sstr, &sint, &sdex);

    you.max_strength = you.strength = cap_stat(sstr);
    you.max_dex      = you.dex      = cap_stat(sdex);
    you.max_intel    = you.intel    = cap_stat(sint);

    you.redraw_strength     = true;
    you.redraw_dexterity    = true;
    you.redraw_intelligence = true;
    you.redraw_evasion      = true;
}

static const char* dur_names[NUM_DURATIONS] =
{
    "invis",
    "conf",
    "paralysis",
    "slow",
    "mesmerised",
    "haste",
    "might",
    "levitation",
    "berserker",
    "poisoning",
    "confusing touch",
    "sure blade",
    "backlight",
    "deaths door",
    "fire shield",
    "building rage",
    "exhausted",
    "liquid flames",
    "icy armour",
    "repel missiles",
    "prayer",
    "piety pool",
    "divine vigour",
    "divine stamina",
    "divine shield",
    "regeneration",
    "swiftness",
    "stonemail",
    "controlled flight",
    "teleport",
    "control teleport",
    "breath weapon",
    "transformation",
    "death channel",
    "deflect missiles",
    "forescry",
    "see invisible",
    "weapon brand",
    "silence",
    "condensation shield",
    "stoneskin",
    "gourmand",
    "bargain",
    "insulation",
    "resist poison",
    "resist fire",
    "resist cold",
    "slaying",
    "stealth",
    "magic shield",
    "sleep",
    "sage",
    "telepathy",
    "petrified",
    "lowered mr",
    "repel stairs move",
    "repel stairs climb"
};

void wizard_edit_durations( void )
{
    std::vector<int> durs;
    size_t max_len = 0;

    for (int i = 0; i < NUM_DURATIONS; ++i)
    {
        if (!you.duration[i])
            continue;

        max_len = std::max(strlen(dur_names[i]), max_len);
        durs.push_back(i);
    }

    if (durs.size() > 0)
    {
        for (unsigned int i = 0; i < durs.size(); ++i)
        {
            int dur = durs[i];
            mprf(MSGCH_PROMPT, "%c) %-*s : %d", 'a' + i, max_len,
                 dur_names[dur], you.duration[dur]);
        }
        mpr("", MSGCH_PROMPT);
        mpr("Edit which duration (letter or name)? ", MSGCH_PROMPT);
    }
    else
        mpr("Edit which duration (name)? ", MSGCH_PROMPT);

    char buf[80];

    if (cancelable_get_line_autohist(buf, sizeof buf) || strlen(buf) == 0)
    {
        canned_msg( MSG_OK );
        return;
    }

    strcpy(buf, lowercase_string(trimmed_string(buf)).c_str());

    if (strlen(buf) == 0)
    {
        canned_msg( MSG_OK );
        return;
    }

    int choice = -1;

    if (strlen(buf) == 1)
    {
        if (durs.size() == 0)
        {
            mpr("No existing durations to choose from.", MSGCH_PROMPT);
            return;
        }
        choice = buf[0] - 'a';

        if (choice < 0 || choice >= (int) durs.size())
        {
            mpr("Invalid choice.", MSGCH_PROMPT);
            return;
        }
        choice = durs[choice];
    }
    else
    {
        std::vector<int>         matches;
        std::vector<std::string> match_names;
        max_len = 0;

        for (int i = 0; i < NUM_DURATIONS; ++i)
        {
            if (strcmp(dur_names[i], buf) == 0)
            {
                choice = i;
                break;
            }
            if (strstr(dur_names[i], buf) != NULL)
            {
                matches.push_back(i);
                match_names.push_back(dur_names[i]);
            }
        }
        if (choice != -1)
            ;
        else if (matches.size() == 1)
            choice = matches[0];
        else if (matches.size() == 0)
        {
            mprf(MSGCH_PROMPT, "No durations matching '%s'.", buf);
            return;
        }
        else
        {
            std::string prefix = "No exact match for duration '";
            prefix += buf;
            prefix += "', possible matches are: ";

            mpr_comma_separated_list(prefix, match_names, " and ", ", ",
                                     MSGCH_DIAGNOSTICS);
            return;
        }
    }

    sprintf(buf, "Set '%s' to: ", dur_names[choice]);
    int num = _debug_prompt_for_int(buf, false);

    if (num == 0)
    {
        mpr("Can't set duration directly to 0, setting it to 1 instead.",
            MSGCH_PROMPT);
        num = 1;
    }
    you.duration[choice] = num;
}

void wizard_draw_card()
{
    msg::streams(MSGCH_PROMPT) << "Which card? " << std::endl;
    char buf[80];
    if (cancelable_get_line_autohist(buf, sizeof buf))
    {
        mpr("Unknown card.");
        return;
    }

    std::string wanted = buf;
    lowercase(wanted);

    bool found_card = false;
    for ( int i = 0; i < NUM_CARDS; ++i )
    {
        const card_type c = static_cast<card_type>(i);
        std::string card = card_name(c);
        lowercase(card);
        if ( card.find(wanted) != std::string::npos )
        {
            card_effect(c, DECK_RARITY_LEGENDARY);
            found_card = true;
            break;
        }
    }
    if (!found_card)
        mpr("Unknown card.");
}

static void debug_uptick_xl(int newxl)
{
    while (newxl > you.experience_level)
    {
        you.experience = 1 + exp_needed( 2 + you.experience_level );
        level_change(true);
    }
}

static void debug_downtick_xl(int newxl)
{
    you.hp = you.hp_max;
    while (newxl < you.experience_level)
    {
        // Each lose_level() subtracts 4 HP, so do this to avoid death
        // and/or negative HP when going from a high level to a low level.
        you.hp     = std::max(5, you.hp);
        you.hp_max = std::max(5, you.hp_max);

        lose_level();
    }

    you.hp       = std::max(1, you.hp);
    you.hp_max   = std::max(1, you.hp_max);

    you.base_hp  = std::max(5000,              you.base_hp);
    you.base_hp2 = std::max(5000 + you.hp_max, you.base_hp2);
}

void wizard_set_xl()
{
    mprf(MSGCH_PROMPT, "Enter new experience level: ");
    char buf[30];
    if (cancelable_get_line_autohist(buf, sizeof buf))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int newxl = atoi(buf);
    if (newxl < 1 || newxl > 27 || newxl == you.experience_level)
    {
        canned_msg(MSG_OK);
        return;
    }

    no_messages mx;
    if (newxl < you.experience_level)
        debug_downtick_xl(newxl);
    else
        debug_uptick_xl(newxl);
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
    if ((toplace->orient == MAP_FLOAT || toplace->orient == MAP_NONE)
        && place_on_us)
    {
        where = you.pos();
    }

    if (dgn_place_map(toplace, true, false, where))
        mprf("Successfully placed %s.", toplace->name.c_str());
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

// Make all of the monster's original equipment disappear, unless it's a fixed
// artefact or unrand artefact.
static void _vanish_orig_eq(monsters* mons)
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        if (mons->inv[i] == NON_ITEM)
            continue;

        item_def &item(mitm[mons->inv[i]]);

        if (!is_valid_item(item))
            continue;

        if (item.orig_place != 0 || item.orig_monnum != 0
            || !item.inscription.empty()
            || is_unrandom_artefact(item)
            || is_fixed_artefact(item)
            || (item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN | ISFLAG_NOTED_GET
                              | ISFLAG_BEEN_IN_INV) ) )
        {
            continue;
        }
        item.flags |= ISFLAG_SUMMONED;
    }
}

// Dismisses all monsters on the level or all monsters that match a user
// specified regex.
void wizard_dismiss_all_monsters(bool force_all)
{
    char buf[80];
    if (!force_all)
    {
        mpr("Regex of monsters to dismiss (ENTER for all): ", MSGCH_PROMPT);
        bool validline = !cancelable_get_line_autohist(buf, sizeof buf);

        if (!validline)
        {
            canned_msg( MSG_OK );
            return;
        }
    }

    // Make all of the monsters' original equipment disappear unless "keepitem"
    // is found in the regex (except for fixed arts and unrand arts).
    bool keep_item = false;
    if (strstr(buf, "keepitem") != NULL)
    {
        std::string str = replace_all(buf, "keepitem", "");
        trim_string(str);
        strcpy(buf, str.c_str());

        keep_item = true;
    }

    // Dismiss all
    if (buf[0] == '\0' || force_all)
    {
        // Genocide... "unsummon" all the monsters from the level.
        for (int mon = 0; mon < MAX_MONSTERS; mon++)
        {
            monsters *monster = &menv[mon];

            if (monster->alive())
            {
                if (!keep_item)
                    _vanish_orig_eq(monster);
                monster_die(monster, KILL_DISMISSED, NON_MONSTER, false, true);
            }
        }
        // If it was turned off turn autopickup back on.
        autotoggle_autopickup(false);
        return;
    }

    // Dismiss by regex
    text_pattern tpat(buf);
    for (int mon = 0; mon < MAX_MONSTERS; mon++)
    {
        monsters *monster = &menv[mon];

        if (monster->alive() && tpat.matches(monster->name(DESC_PLAIN, true)))
        {
            if (!keep_item)
                _vanish_orig_eq(monster);
            monster_die(monster, KILL_DISMISSED, NON_MONSTER, false, true);
        }
    }
}

static void _debug_kill_traps()
{
    for (rectangle_iterator ri(1); ri; ++ri)
        if (grid_is_trap(grd(*ri), true))
            grd(*ri) = DNGN_FLOOR;
}

static int _debug_time_explore()
{
    viewwindow(true, false);
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
            if (feat == DNGN_CLOSED_DOOR || feat == DNGN_SECRET_DOOR)
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
    _debug_kill_traps(); // doesn't work? (jpeg)
    _debug_destroy_doors();

    forget_map(100);

    // Remember where we are now.
    const coord_def where = you.pos();

    const int explore_turns = _debug_time_explore();

    // Return to starting point.
    you.moveto(where);

    mprf("Explore took %d turns.", explore_turns);
}

#endif

#ifdef WIZARD
extern void force_monster_shout(monsters* monster);

void debug_make_monster_shout(monsters* mon)
{
    mpr("Make the monster (S)hout or (T)alk? ", MSGCH_PROMPT);

    char type = (char) getchm(KMC_DEFAULT);
    type = tolower(type);

    if (type != 's' && type != 't')
    {
        canned_msg( MSG_OK );
        return;
    }

    int num_times = _debug_prompt_for_int("How many times? ", false);

    if (num_times <= 0)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (type == 's')
    {
        if (silenced(you.pos()))
            mpr("You are silenced and likely won't hear any shouts.");
        else if (silenced(mon->pos()))
            mpr("The monster is silenced and likely won't give any shouts.");

        for (int i = 0; i < num_times; ++i)
            force_monster_shout(mon);
    }
    else
    {
        if (mon->invisible())
            mpr("The monster is invisible and likely won't speak.");

        if (silenced(you.pos()) && !silenced(mon->pos()))
        {
            mpr("You are silenced but the monster isn't; you will "
                "probably hear/see nothing.");
        }
        else if (!silenced(you.pos()) && silenced(mon->pos()))
            mpr("The monster is silenced and likely won't say anything.");
        else if (silenced(you.pos()) && silenced(mon->pos()))
        {
            mpr("Both you and the monster are silenced, so you likely "
                "won't hear anything.");
        }

        for (int i = 0; i< num_times; ++i)
            mons_speaks(mon);
    }

    mpr("== Done ==");
}
#endif

#ifdef WIZARD
static bool _force_suitable(const monsters *mon)
{
    return (mon->alive());
}

void wizard_apply_monster_blessing(monsters* mon)
{
    mpr("Apply blessing of (B)eogh, The (S)hining One, or (R)andomly? ",
        MSGCH_PROMPT);

    char type = (char) getchm(KMC_DEFAULT);
    type = tolower(type);

    if (type != 'b' && type != 's' && type != 'r')
    {
        canned_msg( MSG_OK );
        return;
    }
    god_type god = GOD_NO_GOD;
    if (type == 'b' || type == 'r' && coinflip())
        god = GOD_BEOGH;
    else
        god = GOD_SHINING_ONE;

    if (!bless_follower(mon, god, _force_suitable, true))
        mprf("%s won't bless this monster for you!", god_name(god).c_str());
}
#endif

#ifdef WIZARD
void wizard_give_monster_item(monsters *mon)
{
    mon_itemuse_type item_use = mons_itemuse(mon);
    if (item_use < MONUSE_STARTING_EQUIPMENT)
    {
        mpr("That type of monster can't use any items.");
        return;
    }

    int player_slot = prompt_invent_item( "Give which item to monster?",
                                          MT_DROP, -1 );

    if (player_slot == PROMPT_ABORT)
        return;

    for (int i = 0; i < NUM_EQUIP; ++i)
        if (you.equip[i] == player_slot)
        {
            mpr("Can't give equipped items to a monster.");
            return;
        }

    item_def     &item = you.inv[player_slot];
    mon_inv_type mon_slot = NUM_MONSTER_SLOTS;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        // Let wizard specify which slot to put weapon into via
        // inscriptions.
        if (item.inscription.find("first") != std::string::npos
            || item.inscription.find("primary") != std::string::npos)
        {
            mpr("Putting weapon into primary slot by inscription");
            mon_slot = MSLOT_WEAPON;
            break;
        }
        else if (item.inscription.find("second") != std::string::npos
                 || item.inscription.find("alt") != std::string::npos)
        {
            mpr("Putting weapon into alt slot by inscription");
            mon_slot = MSLOT_ALT_WEAPON;
            break;
        }

        // For monsters which can wield two weapons, prefer whichever
        // slot is empty (if there is an empty slot).
        if (mons_wields_two_weapons(mon))
        {
            if (mon->inv[MSLOT_WEAPON] == NON_ITEM)
            {
                mpr("Dual wielding monster, putting into empty primary slot");
                mon_slot = MSLOT_WEAPON;
                break;
            }
            else if (mon->inv[MSLOT_ALT_WEAPON] == NON_ITEM)
            {
                mpr("Dual wielding monster, putting into empty alt slot");
                mon_slot = MSLOT_ALT_WEAPON;
                break;
            }
        }

        // Try to replace a ranged weapon with a ranged weapon and
        // a non-ranged weapon with a non-ranged weapon
        if (mon->inv[MSLOT_WEAPON] != NON_ITEM
            && (is_range_weapon(mitm[mon->inv[MSLOT_WEAPON]])
                == is_range_weapon(item)))
        {
            mpr("Replacing primary slot with similar weapon");
            mon_slot = MSLOT_WEAPON;
            break;
        }
        if (mon->inv[MSLOT_ALT_WEAPON] != NON_ITEM
            && (is_range_weapon(mitm[mon->inv[MSLOT_ALT_WEAPON]])
                == is_range_weapon(item)))
        {
            mpr("Replacing alt slot with similar weapon");
            mon_slot = MSLOT_ALT_WEAPON;
            break;
        }

        // Prefer the empty slot (if any)
        if (mon->inv[MSLOT_WEAPON] == NON_ITEM)
        {
            mpr("Putting weapon into empty primary slot");
            mon_slot = MSLOT_WEAPON;
            break;
        }
        else if (mon->inv[MSLOT_ALT_WEAPON] == NON_ITEM)
        {
            mpr("Putting weapon into empty alt slot");
            mon_slot = MSLOT_ALT_WEAPON;
            break;
        }

        // Default to primary weapon slot
        mpr("Defaulting to primary slot");
        mon_slot = MSLOT_WEAPON;
        break;

    case OBJ_ARMOUR:
    {
        // May only return shield or armour slot.
        equipment_type eq = get_armour_slot(item);

        // Force non-shield, non-body armour to be worn anyway.
        if (eq == EQ_NONE)
            eq = EQ_BODY_ARMOUR;

        mon_slot = equip_slot_to_mslot(eq);
        break;
    }
    case OBJ_MISSILES:
        mon_slot = MSLOT_MISSILE;
        break;
    case OBJ_WANDS:
        mon_slot = MSLOT_WAND;
        break;
    case OBJ_SCROLLS:
        mon_slot = MSLOT_SCROLL;
        break;
    case OBJ_POTIONS:
        mon_slot = MSLOT_POTION;
        break;
    case OBJ_MISCELLANY:
        mon_slot = MSLOT_MISCELLANY;
        break;
    case OBJ_CORPSES:
        if (!mons_eats_corpses(mon))
        {
            mpr("That type of monster doesn't eat corpses.");
            return;
        }
        break;
    default:
        mpr("You can't give that type of item to a monster.");
        return;
    }

    // Shouldn't we be using MONUSE_MAGIC_ITEMS?
    if (item_use == MONUSE_STARTING_EQUIPMENT
        && item.base_type != OBJ_CORPSES
        && !mons_is_unique(mon->type))
    {
        switch (mon_slot)
        {
        case MSLOT_WEAPON:
        case MSLOT_ALT_WEAPON:
        case MSLOT_ARMOUR:
        case MSLOT_MISSILE:
            break;

        default:
            mpr("That type of monster can only use weapons and armour.");
            return;
        }
    }

    int index = get_item_slot(10);
    if (index == NON_ITEM)
    {
        mpr("Too many items on level, bailing.");
        return;
    }

    // Move monster's old item to player's inventory as last step.
    int  old_eq     = NON_ITEM;
    bool unequipped = false;
    if (item.base_type != OBJ_CORPSES
        && mon_slot != NUM_MONSTER_SLOTS
        && mon->inv[mon_slot] != NON_ITEM
        && !items_stack(item, mitm[mon->inv[mon_slot]]))
    {
        old_eq = mon->inv[mon_slot];
        // Alternative weapons don't get (un)wielded unless the monster
        // can wield two weapons.
        if (mon_slot != MSLOT_ALT_WEAPON || mons_wields_two_weapons(mon))
        {
            mon->unequip(*(mon->mslot_item(mon_slot)), mon_slot, 1, true);
            unequipped = true;
        }
        mon->inv[mon_slot] = NON_ITEM;
    }

    mitm[index] = item;

    if (item.base_type == OBJ_CORPSES)
        // In case corpse gets skeletonized.
        move_item_to_grid(&index, mon->pos());

    unwind_var<int> save_speedinc(mon->speed_increment);
    if (!mon->pickup_item(mitm[index], false, true))
    {
        mpr("Monster wouldn't take item.");
        if (old_eq != NON_ITEM && mon_slot != NUM_MONSTER_SLOTS)
        {
            mon->inv[mon_slot] = old_eq;
            if (unequipped)
                mon->equip(mitm[old_eq], mon_slot, 1);
        }
        unlink_item(index);
        // In case corpse gets skeletonized.
        mitm[index].clear();
        return;
    }

    // Item is gone from player's inventory.
    dec_inv_item_quantity(player_slot, item.quantity);

    if ((mon->flags & MF_HARD_RESET) && !(item.flags & ISFLAG_SUMMONED))
    {
       mprf(MSGCH_WARN, "WARNING: Monster has MF_HARD_RESET and all its "
            "items will disappear when it does.");
    }
    else if ((item.flags & ISFLAG_SUMMONED) && !mon->is_summoned())
    {
       mprf(MSGCH_WARN, "WARNING: Item is summoned and will disappear when "
            "the monster does.");
    }
    // Monster's old item moves to player's inventory.
    if (old_eq != NON_ITEM)
    {
        mpr("Fetching monster's old item.");
        if (mitm[old_eq].flags & ISFLAG_SUMMONED)
        {
           mprf(MSGCH_WARN, "WARNING: Item is summoned and shouldn't really "
                "be anywhere but in the inventory of a summoned monster.");
        }
        mitm[old_eq].pos.reset();
        mitm[old_eq].link = NON_ITEM;
        move_item_to_player(old_eq, mitm[old_eq].quantity);
    }
}
#endif

#ifdef WIZARD
static void _move_player(const coord_def& where)
{
    if (!you.can_pass_through_feat(grd(where)))
        grd(where) = DNGN_FLOOR;
    move_player_to_grid(where, false, true, true);
}

static void _move_monster(const coord_def& where, int mid1)
{
    dist moves;
    direction(moves, DIR_NONE, TARG_ANY, -1, false, false, true, true,
              "Move monster to where?");

    if (!moves.isValid || !in_bounds(moves.target))
        return;

    monsters* mon1 = &menv[mid1];

    const int mid2 = mgrd(moves.target);
    monsters* mon2 = monster_at(moves.target);

    mon1->moveto(moves.target);
    mgrd(moves.target) = mid1;
    mon1->check_redraw(moves.target);

    mgrd(where) = mid2;

    if (mon2 != NULL)
    {
        mon2->moveto(where);
        mon1->check_redraw(where);
    }
}

void wizard_move_player_or_monster(const coord_def& where)
{
    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    static bool already_moving = false;

    if (already_moving)
    {
        mpr("Already doing a move command.");
        return;
    }

    already_moving = true;

    int mid = mgrd(where);

    if (mid == NON_MONSTER)
    {
        if (crawl_state.arena_suspended)
        {
            mpr("You can't move yourself into the arena.");
            more();
            return;
        }
        _move_player(where);
    }
    else
        _move_monster(where, mid);

    already_moving = false;
}

void wizard_make_monster_summoned(monsters* mon)
{
    int summon_type = 0;
    if (mon->is_summoned(NULL, &summon_type) || summon_type != 0)
    {
        mpr("Monster is already summoned.", MSGCH_PROMPT);
        return;
    }

    int dur = _debug_prompt_for_int("What summon longevity (1 to 6)? ", true);

    if (dur < 1 || dur > 6)
    {
        canned_msg( MSG_OK );
        return;
    }

    mpr("[a] clone [b] animated [c] chaos [d] miscast [e] zot", MSGCH_PROMPT);
    mpr("[f] wrath [g] aid                [m] misc    [s] spell",
        MSGCH_PROMPT);

    mpr("Which summon type? ", MSGCH_PROMPT);

    char choice = tolower(getch());

    if (!(choice >= 'a' && choice <= 'g') && choice != 'm' && choice != 's')
    {
        canned_msg( MSG_OK );
        return;
    }

    int type = 0;

    switch (choice)
    {
        case 'a': type = MON_SUMM_CLONE; break;
        case 'b': type = MON_SUMM_ANIMATE; break;
        case 'c': type = MON_SUMM_CHAOS; break;
        case 'd': type = MON_SUMM_MISCAST; break;
        case 'e': type = MON_SUMM_ZOT; break;
        case 'f': type = MON_SUMM_WRATH; break;
        case 'g': type = MON_SUMM_AID; break;
        case 'm': type = 0; break;

        case 's':
        {
            char specs[80];

            mpr("Cast which spell by name? ", MSGCH_PROMPT);
            get_input_line( specs, sizeof( specs ) );

            if (specs[0] == '\0')
            {
                canned_msg( MSG_OK );
                return;
            }

            spell_type spell = spell_by_name(specs, true);
            if (spell == SPELL_NO_SPELL)
            {
                mpr("No such spell.", MSGCH_PROMPT);
                return;
            }
            type = (int) spell;
            break;
        }

        default:
            DEBUGSTR("Invalid summon type choice.");
            break;
    }

    mon->mark_summoned(dur, true, type);
    mpr("Monster is now summoned.");
}

void wizard_polymorph_monster(monsters* mon)
{
    int old_type =  mon->type;
    int type     = _debug_prompt_for_monster();

    if (type == -1)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (invalid_monster_class(type) || type == MONS_PROGRAM_BUG)
    {
        mpr("Invalid monster type.", MSGCH_PROMPT);
        return;
    }

    if (type == old_type)
    {
        mpr("Old type and new type are the same, not polymorphing.");
        return;
    }

    if (mons_species(type) == mons_species(old_type))
    {
        mpr("Target species must be different from current species.");
        return;
    }

    monster_polymorph(mon, (monster_type) type, PPT_SAME, true);

    if (!mon->alive())
    {
        mpr("Polymorph killed monster?", MSGCH_ERROR);
        return;
    }

    mon->check_redraw(mon->pos());

    if (mon->type == old_type)
        mpr("Polymorph failed.");
    else if (mon->type != type)
        mpr("Monster turned into something other than the desired type.");
}

void debug_pathfind(int mid)
{
    if (mid == NON_MONSTER)
        return;

    mpr("Choose a destination!");
#ifndef USE_TILE
    more();
#endif
    coord_def dest;
    show_map(dest, true);
    redraw_screen();
    if (!dest.x)
    {
        canned_msg(MSG_OK);
        return;
    }

    monsters &mon = menv[mid];
    mprf("Attempting to calculate a path from (%d, %d) to (%d, %d)...",
         mon.pos().x, mon.pos().y, dest.x, dest.y);
    monster_pathfind mp;
    bool success = mp.init_pathfind(&mon, dest, true);
    if (success)
    {
        std::vector<coord_def> path = mp.backtrack();
        std::string path_str;
        mpr("Here's the shortest path: ");
        for (unsigned int i = 0; i < path.size(); ++i)
        {
            snprintf(info, INFO_SIZE, "(%d, %d)  ", path[i].x, path[i].y);
            path_str += info;
        }
        mpr(path_str.c_str());
        mprf("-> path length: %d", path.size());

        mpr(EOL);
        path = mp.calc_waypoints();
        path_str = "";
        mpr(EOL);
        mpr("And here are the needed waypoints: ");
        for (unsigned int i = 0; i < path.size(); ++i)
        {
            snprintf(info, INFO_SIZE, "(%d, %d)  ", path[i].x, path[i].y);
            path_str += info;
        }
        mpr(path_str.c_str());
        mprf("-> #waypoints: %d", path.size());
    }
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

static void _miscast_screen_update()
{
    viewwindow(true, false);

    you.redraw_status_flags =
        REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK;
    print_stats();

#ifndef USE_TILE
    update_monster_pane();
#endif
}

void debug_miscast( int target_index )
{
    crawl_state.cancel_cmd_repeat();

    actor* target;
    if (target_index == NON_MONSTER)
        target = &you;
    else
        target = &menv[target_index];

    if (!target->alive())
    {
        mpr("Can't make already dead target miscast.");
        return;
    }

    char specs[100];
    mpr("Miscast which school or spell, by name? ", MSGCH_PROMPT);
    if (cancelable_get_line_autohist(specs, sizeof specs) || !*specs)
    {
        canned_msg(MSG_OK);
        return;
    }

    spell_type         spell  = spell_by_name(specs, true);
    spschool_flag_type school = school_by_name(specs);

    // Prefer exact matches for school name over partial matches for
    // spell name.
    if (school != SPTYP_NONE
        && (strcasecmp(specs, spelltype_short_name(school)) == 0
            || strcasecmp(specs, spelltype_long_name(school)) == 0))
    {
        spell = SPELL_NO_SPELL;
    }

    if (spell == SPELL_NO_SPELL && school == SPTYP_NONE)
    {
        mpr("No matching spell or spell school.");
        return;
    }
    else if (spell != SPELL_NO_SPELL && school != SPTYP_NONE)
    {
        mprf("Ambiguous: can be spell '%s' or school '%s'.",
            spell_title(spell), spelltype_short_name(school));
        return;
    }

    int disciplines = 0;
    if (spell != SPELL_NO_SPELL)
    {
        disciplines = get_spell_disciplines(spell);

        if (disciplines == 0)
        {
            mprf("Spell '%s' has no disciplines.", spell_title(spell));
            return;
        }
    }

    if (school == SPTYP_HOLY || (disciplines & SPTYP_HOLY))
    {
        mpr("Can't miscast holy spells.");
        return;
    }

    if (spell != SPELL_NO_SPELL)
        mprf("Miscasting spell %s.", spell_title(spell));
    else
        mprf("Miscasting school %s.", spelltype_long_name(school));

    if (spell != SPELL_NO_SPELL)
        mpr("Enter spell_power,spell_failure: ", MSGCH_PROMPT );
    else
    {
        mpr("Enter miscast_level or spell_power,spell_failure: ",
             MSGCH_PROMPT);
    }

    if (cancelable_get_line_autohist(specs, sizeof specs) || !*specs)
    {
        canned_msg(MSG_OK);
        return;
    }

    int level = -1, pow = -1, fail = -1;

    if (strchr(specs, ','))
    {
        std::vector<std::string> nums = split_string(",", specs);
        pow  = atoi(nums[0].c_str());
        fail = atoi(nums[1].c_str());

        if (pow <= 0 || fail <= 0)
        {
            canned_msg(MSG_OK);
            return;
        }
    }
    else
    {
        if (spell != SPELL_NO_SPELL)
        {
            mpr("Can only enter fixed miscast level for schools, not spells.");
            return;
        }

        level = atoi(specs);
        if (level < 0)
        {
            canned_msg(MSG_OK);
            return;
        }
        else if (level > 3)
        {
            mpr("Miscast level can be at most 3.");
            return;
        }
    }

    // Handle repeats ourselves since miscasts are likely to interrupt
    // command repetions, especially if the player is the target.
    int repeats = _debug_prompt_for_int("Number of repetitions? ", true);
    if (repeats < 1)
    {
        canned_msg(MSG_OK);
        return;
    }

    // Supress "nothing happens" message for monster miscasts which are
    // only harmless messages, since a large number of those are missing
    // monster messages.
    nothing_happens_when_type nothing = NH_DEFAULT;
    if (target_index != NON_MONSTER && level == 0)
        nothing = NH_NEVER;

    MiscastEffect *miscast;

    if (spell != SPELL_NO_SPELL)
        miscast = new MiscastEffect(target, target_index, spell, pow, fail,
                                    "", nothing);
    else
    {
        if (level != -1)
            miscast = new MiscastEffect(target, target_index, school,
                                        level, "wizard testing miscast",
                                        nothing);
        else
            miscast = new MiscastEffect(target, target_index, school,
                                        pow, fail, "wizard testing miscast",
                                        nothing);
    }
    // Merely creating the miscast object causes one miscast effect to
    // happen.
    repeats--;
    if (level != 0)
        _miscast_screen_update();

    while (target->alive() && repeats-- > 0)
    {
        if (kbhit())
        {
            mpr("Key pressed, interrupting miscast testing.");
            getchm();
            break;
        }

        miscast->do_miscast();
        if (level != 0)
            _miscast_screen_update();
    }

    delete miscast;
}
#endif

static void _dump_levgen()
{
    CrawlHashTable &props = env.properties;

    std::string method;
    std::string type;

    if (Generating_Level)
    {
        mpr("Currently generating level.");
        extern std::string dgn_Build_Method;
        method = dgn_Build_Method;
        type   = dgn_Layout_Type;
    }
    else
    {
        if (!props.exists(BUILD_METHOD_KEY))
            method = "ABSENT";
        else
            method = props[BUILD_METHOD_KEY].get_string();

        if (!props.exists(LAYOUT_TYPE_KEY))
            type = "ABSENT";
        else
            type = props[LAYOUT_TYPE_KEY].get_string();
    }

    mprf("dgn_Build_Method = %s", method.c_str());
    mprf("dgn_Layout_Type  = %s", type.c_str());

    std::string extra;

    if (!props.exists(LEVEL_EXTRAS_KEY))
        extra = "ABSENT";
    else
    {
        const CrawlVector &vec = props[LEVEL_EXTRAS_KEY].get_vector();

        for (unsigned int i = 0; i < vec.size(); ++i)
            extra += vec[i].get_string() + ", ";
    }

    mprf("Level extras: %s", extra.c_str());

    mpr("Level vaults:");
    if (!props.exists(LEVEL_VAULTS_KEY))
        mpr("ABSENT");
    else
    {
        const CrawlHashTable &vaults = props[LEVEL_VAULTS_KEY].get_table();
        CrawlHashTable::const_iterator i = vaults.begin();

        for (; i != vaults.end(); ++i)
            mprf("    %s: %s", i->first.c_str(),
                 i->second.get_string().c_str());
    }
    mpr("");

    mpr("Temp vaults:");
    if (!props.exists(TEMP_VAULTS_KEY))
        mpr("ABSENT");
    else
    {
        const CrawlHashTable &vaults = props[TEMP_VAULTS_KEY].get_table();
        CrawlHashTable::const_iterator i = vaults.begin();

        for (; i != vaults.end(); ++i)
        {
            mprf("    %s: %s", i->first.c_str(),
                 i->second.get_string().c_str());
        }
    }
    mpr("");
}

static void _dump_level_info(FILE* file)
{
    CrawlHashTable &props = env.properties;

    fprintf(file, "Place info:" EOL);

    fprintf(file, "your_level = %d, branch = %d, level_type = %d, "
                  "type_name = %s" EOL EOL,
            you.your_level, (int) you.where_are_you, (int) you.level_type,
            you.level_type_name.c_str());

    std::string place = level_id::current().describe();
    std::string orig_place;

    if (!props.exists(LEVEL_ID_KEY))
        orig_place = "ABSENT";
    else
        orig_place = props[LEVEL_ID_KEY].get_string();

    fprintf(file, "Level id: %s" EOL, place.c_str());
    if (place != orig_place)
        fprintf(file, "Level id when level was generated: %s" EOL,
                orig_place.c_str());

    _dump_levgen();
}

static void _dump_player(FILE *file)
{
    // Only dump player info during arena mode if the player is likely
    // the cause of the crash.
    if ((crawl_state.arena || crawl_state.arena_suspended)
        && !in_bounds(you.pos()) && you.hp > 0 && you.hp_max > 0
        && you.strength > 0 && you.intel > 0 && you.dex > 0)
    {
        // Arena mode can change behavior of the rest of the code and/or lead
        // to asserts.
        crawl_state.arena          = false;
        crawl_state.arena_suspended = false;
        return;
    }

    // Arena mode can change behavior of the rest of the code and/or lead
    // to asserts.
    crawl_state.arena          = false;
    crawl_state.arena_suspended = false;

    fprintf(file, "Player:" EOL);
    fprintf(file, "{{{{{{{{{{{" EOL);

    bool name_overrun = true;
    for (int i = 0; i < kNameLen; ++i)
    {
        if (you.your_name[i] == '\0')
        {
            name_overrun = false;
            break;
        }
    }

    if (name_overrun)
    {
        fprintf(file, "Player name runs past end of your_name buffer." EOL);
        you.your_name[kNameLen - 1] = '\0';
    }

    name_overrun = true;
    for (int i = 0; i < 30; ++i)
    {
        if (you.class_name[i] == '\0')
        {
            name_overrun = false;
            break;
        }
    }

    if (name_overrun)
    {
        fprintf(file, "class_name runs past end of buffer." EOL);
        you.class_name[29] = '\0';
    }

    fprintf(file, "Name:       [%s]" EOL, you.your_name);
    fprintf(file, "Species:    %s" EOL, species_name(you.species, 27).c_str());
    fprintf(file, "Class:      %s" EOL EOL, get_class_name(you.char_class));
    fprintf(file, "class_name: %s" EOL EOL, you.class_name);

    fprintf(file, "HP: %d/%d; base: %d/%d" EOL, you.hp, you.hp_max,
            you.base_hp, you.base_hp2);
    fprintf(file, "MP: %d/%d; base: %d/%d" EOL,
            you.magic_points, you.max_magic_points,
            you.base_magic_points, you.base_magic_points2);
    fprintf(file, "Stats: %d (%d) %d (%d) %d (%d)" EOL,
            you.strength, you.max_strength, you.intel, you.max_intel,
            you.dex, you.max_dex);
    fprintf(file, "Position: %s, god:%s (%d), turn_is_over: %d, "
                  "banished: %d" EOL,
            debug_coord_str(you.pos()).c_str(),
            god_name(you.religion).c_str(), (int) you.religion,
            (int) you.turn_is_over, (int) you.banished);

    if (in_bounds(you.pos()))
    {
        const dungeon_feature_type feat = grd(you.pos());
        fprintf(file, "Standing on/in/over feature: %s" EOL,
                raw_feature_description(feat, NUM_TRAPS, true).c_str());
    }
    fprintf(file, EOL);

    if (you.running.runmode != RMODE_NOT_RUNNING)
    {
        fprintf(file, "Runrest:" EOL);
        fprintf(file, "    mode: %d" EOL, you.running.runmode);
        fprintf(file, "      mp: %d" EOL, you.running.mp);
        fprintf(file, "      hp: %d" EOL, you.running.hp);
        fprintf(file, "     pos: %s" EOL EOL,
                debug_coord_str(you.running.pos).c_str());
    }

    if (you.delay_queue.size() > 0)
    {
        fprintf(file, "Delayed (%lu):" EOL,
                (unsigned long) you.delay_queue.size());
        for (unsigned int i = 0; i < you.delay_queue.size(); ++i)
        {
            const delay_queue_item &item = you.delay_queue[i];

            fprintf(file, "    type:     %d", item.type);
            if (item.type <= DELAY_NOT_DELAYED
                || item.type >= NUM_DELAYS)
            {
                fprintf(file, " <invalid>");
            }
            fprintf(file, EOL);
            fprintf(file, "    duration: %d" EOL, item.duration);
            fprintf(file, "    parm1:    %d" EOL, item.parm1);
            fprintf(file, "    parm2:    %d" EOL, item.parm2);
            fprintf(file, "    started:  %d" EOL EOL, (int) item.started);
        }
        fprintf(file, EOL);
    }

    fprintf(file, "Spell bugs:" EOL);
    for (size_t i = 0; i < you.spells.size(); ++i)
    {
        const spell_type spell = you.spells[i];

        if (spell == SPELL_NO_SPELL)
            continue;

        if (!is_valid_spell(spell))
        {
            fprintf(file, "    spell slot #%d: invalid spell #%d" EOL,
                    (int)i, (int)spell);
            continue;
        }

        const unsigned int flags = get_spell_flags(spell);

        if (flags & SPFLAG_MONSTER)
            fprintf(file, "    spell slot #%d: monster only spell %s" EOL,
                    (int)i, spell_title(spell));
        else if (flags & SPFLAG_TESTING)
            fprintf(file, "    spell slot #%d: testing spell %s" EOL,
                    (int)i, spell_title(spell));
        else if (count_bits(get_spell_disciplines(spell)) == 0)
            fprintf(file, "    spell slot #%d: school-less spell %s" EOL,
                    (int)i, spell_title(spell));
    }
    fprintf(file, EOL);

    fprintf(file, "Durations:" EOL);
    for (int i = 0; i < NUM_DURATIONS; ++i)
        if (you.duration[i] != 0)
            fprintf(file, "    #%d: %d" EOL, i, you.duration[i]);

    fprintf(file, EOL);

    fprintf(file, "Attributes:" EOL);
    for (int i = 0; i < NUM_ATTRIBUTES; ++i)
        if (you.attribute[i] != 0)
            fprintf(file, "    #%d: %lu" EOL, i, you.attribute[i]);

    fprintf(file, EOL);

    fprintf(file, "Mutations:" EOL);
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        if (you.mutation[i] > 0)
            fprintf(file, "    #%d: %d" EOL, i, you.mutation[i]);

    fprintf(file, EOL);

    fprintf(file, "Demon mutations:" EOL);
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        if (you.demon_pow[i] > 0)
            fprintf(file, "    #%d: %d" EOL, i, you.demon_pow[i]);

    fprintf(file, EOL);

    fprintf(file, "Inventory bugs:" EOL);
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def &item(you.inv[i]);

        if (item.base_type == OBJ_UNASSIGNED && item.quantity != 0)
        {
            fprintf(file, "    slot #%d: unassigned item has quant %d" EOL,
                    i, item.quantity);
            continue;
        }
        else if (item.base_type != OBJ_UNASSIGNED && item.quantity < 1)
        {
            const int orig_quant = item.quantity;
            item.quantity = 1;

            fprintf(file, "    slot #%d: otherwise valid item '%s' has "
                          "invalid quantity %d" EOL,
                    i, item.name(DESC_PLAIN, false, true).c_str(),
                    orig_quant);
            item.quantity = orig_quant;
            continue;
        }
        else if (!is_valid_item(item))
            continue;

        const std::string name = item.name(DESC_PLAIN, false, true);

        if (item.link != i)
        {
            fprintf(file, "    slot #%d: item '%s' has invalid link %d" EOL,
                    i, name.c_str(), item.link);
        }

        if (item.slot < 0 || item.slot > 127)
        {
            fprintf(file, "    slot #%d: item '%s' has invalid slot %d" EOL,
                    i, name.c_str(), item.slot);
        }

        if (!item.pos.equals(-1, -1))
        {
            fprintf(file, "    slot #%d: item '%s' has invalid pos %s" EOL,
                    i, name.c_str(), debug_coord_str(item.pos).c_str());
        }
    }
    fprintf(file, EOL);

    fprintf(file, "Equipment:" EOL);
    for (int i = 0; i < NUM_EQUIP; ++i)
    {
        char eq = you.equip[i];

        if (eq == -1)
            continue;

        fprintf(file, "    eq slot #%d, inv slot #%d", i, (int) eq);
        if (eq < 0 || eq >= ENDOFPACK)
        {
            fprintf(file, " <invalid>" EOL);
            continue;
        }
        fprintf(file, ": %s" EOL,
                you.inv[eq].name(DESC_PLAIN, false, true).c_str());
    }
    fprintf(file, EOL);

    if (in_bounds(you.pos()) && mgrd(you.pos()) != NON_MONSTER)
    {
        fprintf(file, "Standing on same square as: ");
        const unsigned short midx = mgrd(you.pos());

        if (invalid_monster_index(midx))
            fprintf(file, "invalid monster index %d" EOL, (int) midx);
        else
        {
            const monsters *mon = &menv[midx];
            fprintf(file, "%s:" EOL, debug_mon_str(mon).c_str());
            debug_dump_mon(mon, true);
        }
        fprintf(file, EOL);
    }

    fprintf(file, "}}}}}}}}}}}" EOL EOL);
}

static void _debug_marker_scan()
{
    std::vector<map_marker*> markers = env.markers.get_all();

    for (unsigned int i = 0; i < markers.size(); ++i)
    {
        map_marker* marker = markers[i];

        if (marker == NULL)
        {
            mprf(MSGCH_ERROR, "Marker #%d is NULL", i);
            continue;
        }

        map_marker_type type = marker->get_type();

        if (type < MAT_FEATURE || type >= NUM_MAP_MARKER_TYPES)
            mprf(MSGCH_ERROR, "Makrer #%d at (%d, %d) has invalid type %d",
                 i, marker->pos.x, marker->pos.y, (int) type);

        if (!in_bounds(marker->pos))
        {
            mprf(MSGCH_ERROR, "Marker #%d, type %d at (%d, %d) out of bounds",
                 i, (int) type, marker->pos.x, marker->pos.y);
            continue;
        }

        bool found = false;
        std::vector<map_marker*> at_pos
            = env.markers.get_markers_at(marker->pos);

        for (unsigned int j = 0; j < at_pos.size(); ++j)
        {
            map_marker* tmp = at_pos[j];

            if (tmp == NULL)
                continue;

            if (tmp == marker)
            {
                found = true;
                break;
            }
        }
        if (!found)
            mprf(MSGCH_ERROR, "Marker #%d, type %d at (%d, %d) unlinked",
                 i, (int) type, marker->pos.x, marker->pos.y);
    }

    const coord_def start(MAPGEN_BORDER, MAPGEN_BORDER);
    const coord_def   end(GXM - MAPGEN_BORDER - 2, GYM - MAPGEN_BORDER - 2);
    for (rectangle_iterator ri(start, end); ri; ++ri)
    {
        std::vector<map_marker*> at_pos = env.markers.get_markers_at(*ri);

        for (unsigned int i = 0; i < at_pos.size(); ++i)
        {
            map_marker *marker = at_pos[i];

            if (marker == NULL)
            {
                mprf(MSGCH_ERROR, "Marker #%d at (%d, %d) NULL",
                     i, ri->x, ri->y);
                continue;
            }
            if (marker->pos != *ri)
            {
                mprf(MSGCH_ERROR, "Marker #%d, type %d at (%d, %d) "
                                  "thinks it's at (%d, %d)",
                     i, (int) marker->get_type(), ri->x, ri->y,
                     marker->pos.x, marker->pos.y);

                if (!in_bounds(marker->pos))
                {
                    mpr("Further, it thinks it's out of bounds.",
                        MSGCH_ERROR);
                }
            }
        }
    }
} // _debug_marker_scan()

static void _debug_dump_markers()
{
    std::vector<map_marker*> markers = env.markers.get_all();

    for (unsigned int i = 0; i < markers.size(); ++i)
    {
        map_marker* marker = markers[i];

        if (marker == NULL || marker->get_type() == MAT_LUA_MARKER)
            continue;

        mprf(MSGCH_DIAGNOSTICS, "Marker %d at (%d, %d): %s",
             i, marker->pos.x, marker->pos.y,
             marker->debug_describe().c_str());
    }
}

static void _debug_dump_lua_markers(FILE *file)
{
    std::vector<map_marker*> markers = env.markers.get_all();

    for (unsigned int i = 0; i < markers.size(); ++i)
    {
        map_marker* marker = markers[i];

        if (marker == NULL || marker->get_type() != MAT_LUA_MARKER)
            continue;

        map_lua_marker* lua_marker = dynamic_cast<map_lua_marker*>(marker);

        std::string result = lua_marker->debug_to_string();

        if (result.size() > 0 && result[result.size() - 1] == '\n')
            result = result.substr(0, result.size() - 1);

        fprintf(file, "Lua marker %d at (%d, %d):\n",
                i, marker->pos.x, marker->pos.y);
        fprintf(file, "{{{{\n");
        fprintf(file, "%s", result.c_str());
        fprintf(file, "}}}}\n");
    }
}

static void _debug_dump_lua_persist(FILE* file)
{
    lua_stack_cleaner cln(dlua);

    std::string result;
    if (!dlua.callfn("persist_to_string", 0, 1))
        result = make_stringf("error (persist_to_string): %s",
                              dlua.error.c_str());
    else if (lua_isstring(dlua, -1))
        result = lua_tostring(dlua, -1);
    else
        result = "persist_to_string() returned nothing";

    fprintf(file, "%s", result.c_str());
}

void do_crash_dump()
{
    std::string dir = (!Options.morgue_dir.empty() ? Options.morgue_dir :
                       !SysEnv.crawl_dir.empty()   ? SysEnv.crawl_dir
                                                   : "");

    if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
        dir += FILE_SEPARATOR;

    char name[180];

    sprintf(name, "%scrash-%s-%d.txt", dir.c_str(),
            you.your_name, (int) time(NULL));

    fprintf(stderr, EOL "Writing crash info to %s" EOL, name);
    errno = 0;
    FILE* file = freopen(name, "w+", stderr);

    if (file == NULL || errno != 0)
    {
        fprintf(stdout, EOL "Unable to open file '%s' for writing: %s" EOL,
                name, strerror(errno));
        file = stdout;
    }

    // Unbuffer the file, since if we recursively crash buffered lines
    // won't make it to the file.
    setvbuf(file, NULL, _IONBF, 0);

    set_msg_dump_file(file);

#if DEBUG
    if (!_assert_msg.empty())
        fprintf(file, "%s" EOL EOL, _assert_msg.c_str());
#endif

    fprintf(file, "Revision: %d" EOL, svn_revision());
    fprintf(file, "Version: %s" EOL, CRAWL " " VERSION);
#if defined(UNIX)
    fprintf(file, "Platform: unix" EOL);
#endif
#ifdef USE_TILE
    fprintf(file, "Tiles: yes" EOL EOL);
#else
    fprintf(file, "Tiles: no" EOL EOL);
#endif

    // First get the immediate cause of the crash and the stack trace,
    // since that's most important and later attempts to get more information
    // might themselves cause crashes.
    dump_crash_info(file);
    write_stack_trace(file, 0);

    fprintf(file, EOL);

    // Next information about the level the player is on, plus level
    // generation info if the crash happened during level generation.
    _dump_level_info(file);

    // Dumping information on marker inconsistancy is unlikely to crash,
    // as is dumping the descriptions of non-Lua markers.
    fprintf(file, "Markers:" EOL);
    fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<" EOL);
    _debug_marker_scan();
    _debug_dump_markers();
    fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>" EOL);

    // Dumping current messages is unlikely to crash.
    if (file != stdout)
    {
        fprintf(file, EOL "Messages:" EOL);
        fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<" EOL);
        std::string messages = get_last_messages(NUM_STORED_MESSAGES);
        fprintf(file, "%s", messages.c_str());
        fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>" EOL);
    }

    // Dumping the player state and crawl state is next least likely to cause
    // another crash, so do that next.
    crawl_state.dump();
    _dump_player(file);

    // Next item and monster scans.  Any messages will be sent straight to
    // the file because of set_msg_dump_file()
#if DEBUG_ITEM_SCAN
    debug_item_scan();
#endif
#if DEBUG_MONS_SCAN
    debug_mons_scan();
#endif

    // If anything has screwed up the Lua runtime stacks then trying to
    // print those stacks will likely crash, so do this after the others.
    fprintf(file, "clua stack:" EOL);
    print_clua_stack();

    fprintf(file, "dlua stack:" EOL);
    print_dlua_stack();

    // Lastly try to dump the Lua persistant data and the contents of the Lua
    // markers, since actually running Lua code has the greatest chance of
    // crashing.
    fprintf(file, "Lua persistant data:" EOL);
    fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<" EOL);
    _debug_dump_lua_persist(file);
    fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>" EOL EOL);
    fprintf(file, "Lua marker contents:" EOL);
    fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<" EOL);
    _debug_dump_lua_markers(file);
    fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>" EOL);

    set_msg_dump_file(NULL);

    if (file != stderr)
        fclose(file);
}

std::string debug_coord_str(const coord_def &pos)
{
    return make_stringf("(%d, %d)%s", pos.x, pos.y,
                        !in_bounds(pos) ? " <OoB>" : "");
}

std::string debug_mon_str(const monsters* mon)
{
    const int midx = monster_index(mon);
    if (invalid_monster_index(midx))
        return make_stringf("Invalid monster index %d", midx);

    std::string out = "Monster '" + mon->full_name(DESC_PLAIN, true) + "' ";
    out += make_stringf("%s [midx = %d]", debug_coord_str(mon->pos()).c_str(),
                        midx);

    return (out);
}

void debug_dump_mon(const monsters* mon, bool recurse)
{
    const int midx = monster_index(mon);
    if (invalid_monster_index(midx) || invalid_monster_class(mon->type))
        return;

    fprintf(stderr, "<<<<<<<<<" EOL);

    fprintf(stderr, "Name: %s" EOL, mon->name(DESC_PLAIN, true).c_str());
    fprintf(stderr, "Base name: %s" EOL,
            mon->base_name(DESC_PLAIN, true).c_str());
    fprintf(stderr, "Full name: %s" EOL EOL,
            mon->full_name(DESC_PLAIN, true).c_str());

    if (in_bounds(mon->pos()))
    {
        std::string feat =
            raw_feature_description(grd(mon->pos()), NUM_TRAPS, true);
        fprintf(stderr, "On/in/over feature: %s" EOL EOL, feat.c_str());
    }

    fprintf(stderr, "Foe: ");
    if (mon->foe == MHITNOT)
        fprintf(stderr, "none");
    else if (mon->foe == MHITYOU)
        fprintf(stderr, "player");
    else if (invalid_monster_index(mon->foe))
        fprintf(stderr, "invalid monster index %d", mon->foe);
    else if (mon->foe == midx)
        fprintf(stderr, "self");
    else
        fprintf(stderr, "%s", debug_mon_str(&menv[mon->foe]).c_str());

    fprintf(stderr, EOL);

    fprintf(stderr, "Target: ");
    if (mon->target.origin())
        fprintf(stderr, "none" EOL);
    else
        fprintf(stderr, "%s" EOL, debug_coord_str(mon->target).c_str());

    int target = MHITNOT;
    fprintf(stderr, "At target: ");
    if (mon->target.origin())
        fprintf(stderr, "N/A");
    else if (mon->target == you.pos())
    {
        fprintf(stderr, "player");
        target = MHITYOU;
    }
    else if (mon->target == mon->pos())
    {
        fprintf(stderr, "self");
        target = midx;
    }
    else if (in_bounds(mon->target))
    {
       target = mgrd(mon->target);

       if (target == NON_MONSTER)
           fprintf(stderr, "nothing");
       else if (target == midx)
           fprintf(stderr, "improperly linked self");
       else if (target == mon->foe)
           fprintf(stderr, "same as foe");
       else if (invalid_monster_index(target))
           fprintf(stderr, "invalid monster index %d", target);
       else
           fprintf(stderr, "%s", debug_mon_str(&menv[target]).c_str());
    }
    else
        fprintf(stderr, "<OoB>");

    fprintf(stderr, EOL);

    if (mon->is_patrolling())
    {
        fprintf(stderr, "Patrolling: %s" EOL EOL,
                debug_coord_str(mon->patrol_point).c_str());
    }

    if (mon->travel_target != MTRAV_NONE)
    {
        fprintf(stderr, EOL "Travelling:" EOL);
        fprintf(stderr, "    travel_target      = %d" EOL, mon->travel_target);
        fprintf(stderr, "    travel_path.size() = %lu" EOL,
                (long unsigned int) mon->travel_path.size());

        if (mon->travel_path.size() > 0)
        {
            fprintf(stderr, "    next travel step: %s" EOL,
                    debug_coord_str(mon->travel_path.back()).c_str());
            fprintf(stderr, "    last travel step: %s" EOL,
                    debug_coord_str(mon->travel_path.front()).c_str());
        }
    }
    fprintf(stderr, EOL);

    fprintf(stderr, "Inventory:" EOL);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        const int idx = mon->inv[i];

        if (idx == NON_ITEM)
            continue;

        fprintf(stderr, "    slot #%d: ", i);

        if (idx < 0 || idx > MAX_ITEMS)
        {
            fprintf(stderr, "invalid item index %d" EOL, idx);
            continue;
        }
        const item_def &item(mitm[idx]);

        if (!is_valid_item(item))
        {
            fprintf(stderr, "invalid item" EOL);
            continue;
        }

        fprintf(stderr, "%s", item.name(DESC_PLAIN, false, true).c_str());

        if (!item.held_by_monster())
        {
            fprintf(stderr, " [not held by monster, pos = %s]",
                    debug_coord_str(item.pos).c_str());
        }
        else if (item.holding_monster() != mon)
        {
            fprintf(stderr, " [held by other monster: %s]",
                    debug_mon_str(item.holding_monster()).c_str());
        }

        fprintf(stderr, EOL);
    }
    fprintf(stderr, EOL);

    if (mons_class_flag(mon->type, M_SPELLCASTER))
    {
        fprintf(stderr, "Spells:" EOL);

        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
        {
            spell_type spell = mon->spells[i];

            if (spell == SPELL_NO_SPELL)
                continue;

            fprintf(stderr, "    slot #%d: ", i);
            if (!is_valid_spell(spell))
                fprintf(stderr, "Invalid spell #%d" EOL, (int) spell);
            else
                fprintf(stderr, "%s" EOL, spell_title(spell));
        }
        fprintf(stderr, EOL);
    }

    fprintf(stderr, "attitude: %d, behaviour: %d, number: %d, flags: 0x%lx" EOL,
            mon->attitude, mon->behaviour, mon->number, mon->flags);

    fprintf(stderr, "colour: %d, foe_memory: %d, shield_blocks:%d, "
                  "experience: %lu" EOL,
            mon->colour, mon->foe_memory, mon->shield_blocks,
            mon->experience);

    fprintf(stderr, "god: %s, seen_context: %s" EOL,
            god_name(mon->god).c_str(), mon->seen_context.c_str());

    fprintf(stderr, ">>>>>>>>>" EOL EOL);

    if (!recurse)
        return;

    if (!invalid_monster_index(mon->foe) && mon->foe != midx
        && !invalid_monster_class(menv[mon->foe].type))
    {
        fprintf(stderr, "Foe:" EOL);
        debug_dump_mon(&menv[mon->foe], false);
    }

    if (!invalid_monster_index(target) && target != midx
        && target != mon->foe
        && !invalid_monster_class(menv[target].type))
    {
        fprintf(stderr, "Target:" EOL);
        debug_dump_mon(&menv[target], false);
    }
}


#ifdef DEBUG_DIAGNOSTICS

// Map statistics generation.

static std::map<std::string, int> mapgen_try_count;
static std::map<std::string, int> mapgen_use_count;
static std::map<level_id, int> mapgen_level_mapcounts;
static std::map< level_id, std::pair<int,int> > mapgen_map_builds;
static std::map< level_id, std::set<std::string> > mapgen_level_mapsused;

typedef std::map< std::string, std::set<level_id> > mapname_place_map;
static mapname_place_map mapgen_map_levelsused;
static std::map<std::string, std::string> mapgen_errors;
static std::string mapgen_last_error;

static int mg_levels_tried = 0, mg_levels_failed = 0;
static int mg_build_attempts = 0, mg_vetoes = 0;

void mapgen_report_map_build_start()
{
    mg_build_attempts++;
    mapgen_map_builds[level_id::current()].first++;
}

void mapgen_report_map_veto()
{
    mg_vetoes++;
    mapgen_map_builds[level_id::current()].second++;
}

static map_mask mg_MapMask;

static bool _mg_region_flood(const coord_def &c, int region, bool flag)
{
    bool found_exit = false;

    mg_MapMask(c) = region;

    if (flag)
    {
        env.map(c).flags = 0;
        set_terrain_mapped(c.x, c.y);
    }

    const dungeon_feature_type ft = grd(c);
    if (is_travelable_stair(ft))
        found_exit = true;

    for (int yi = -1; yi <= 1; ++yi)
        for (int xi = -1; xi <= 1; ++xi)
        {
            if (!xi && !yi)
                continue;

            coord_def ci = c + coord_def(xi, yi);
            if (!in_bounds(ci) || mg_MapMask(ci) || !dgn_square_is_passable(ci))
                continue;

            if (_mg_region_flood(ci, region, flag))
                found_exit = true;
        }
    return (found_exit);
}

static bool _mg_is_disconnected_level()
{
    // Don't care about non-Dungeon levels.
    if (you.level_type != LEVEL_DUNGEON
        || (branches[you.where_are_you].branch_flags & BFLAG_ISLANDED))
        return (false);

    std::vector<coord_def> region_seeds;

    mg_MapMask.init(0);

    coord_def c;
    int region = 0;
    int good_regions = 0;
    for (c.y = 0; c.y < GYM; ++c.y)
        for (c.x = 0; c.x < GXM; ++c.x)
            if (!mg_MapMask(c) && dgn_square_is_passable(c))
            {
                if (_mg_region_flood(c, ++region, false))
                    ++good_regions;
                else
                    region_seeds.push_back(c);
            }

    mg_MapMask.init(0);
    for (int i = 0, size = region_seeds.size(); i < size; ++i)
        _mg_region_flood(region_seeds[i], 1, true);

    return (good_regions < region);
}

static bool mg_do_build_level(int niters)
{
    mesclr();
    mprf("On %s (%d); %d g, %d fail, %d err%s, %d uniq, "
         "%d try, %d (%.2lf%%) vetos",
         level_id::current().describe().c_str(), niters,
         mg_levels_tried, mg_levels_failed, mapgen_errors.size(),
         mapgen_last_error.empty()? ""
         : (" (" + mapgen_last_error + ")").c_str(),
         mapgen_use_count.size(),
         mg_build_attempts, mg_vetoes,
         mg_build_attempts? mg_vetoes * 100.0 / mg_build_attempts : 0.0);

    no_messages mx;
    for (int i = 0; i < niters; ++i)
    {
        if (kbhit() && getch() == ESCAPE)
            return (false);

        ++mg_levels_tried;
        if (!builder(you.your_level, you.level_type))
        {
            ++mg_levels_failed;
            continue;
        }

        for (int y = 0; y < GYM; ++y)
            for (int x = 0; x < GXM; ++x)
            {
                switch (grd[x][y])
                {
                case DNGN_SECRET_DOOR:
                    grd[x][y] = DNGN_CLOSED_DOOR;
                    break;
                default:
                    break;
                }
            }

        {
            unwind_bool wiz(you.wizard, true);
            magic_mapping(1000, 100, true, true);
        }
        if (_mg_is_disconnected_level())
        {
            extern std::vector<vault_placement> Level_Vaults;
            std::string vaults;
            for (int j = 0, size = Level_Vaults.size(); j < size; ++j)
            {
                if (j && !vaults.empty())
                    vaults += ", ";
                vaults += Level_Vaults[j].map.name;
            }

            if (!vaults.empty())
                vaults = " (" + vaults + ")";

            extern std::string dgn_Build_Method;
            mprf(MSGCH_ERROR,
                 "Bad (disconnected) level on %s%s",
                 level_id::current().describe().c_str(),
                 vaults.c_str());
            FILE *fp = fopen("map.dump", "w");
            fprintf(fp, "Bad (disconnected) level (%s) on %s%s.\n\n",
                    dgn_Build_Method.c_str(),
                    level_id::current().describe().c_str(),
                    vaults.c_str());

            // Mapping would only have mapped squares that the player can
            // reach - explicitly map the full level.
            coord_def c;
            for (c.y = 0; c.y < GYM; ++c.y)
                for (c.x = 0; c.x < GXM; ++c.x)
                    set_envmap_obj(c, grd(c));

            dump_map(fp);

            return (false);
        }
    }
    return (true);
}

static std::vector<level_id> mg_dungeon_places()
{
    std::vector<level_id> places;

    for (int br = BRANCH_MAIN_DUNGEON; br < NUM_BRANCHES; ++br)
    {
        if (branches[br].depth == -1)
            continue;

        const branch_type branch = static_cast<branch_type>(br);
        for (int depth = 1; depth <= branches[br].depth; ++depth)
            places.push_back( level_id(branch, depth) );
    }

    places.push_back(LEVEL_ABYSS);
    places.push_back(LEVEL_LABYRINTH);
    places.push_back(LEVEL_PANDEMONIUM);
    places.push_back(LEVEL_PORTAL_VAULT);

    return (places);
}

static bool mg_build_dungeon()
{
    const std::vector<level_id> places = mg_dungeon_places();

    for (int i = 0, size = places.size(); i < size; ++i)
    {
        const level_id &lid = places[i];
        you.your_level = absdungeon_depth(lid.branch, lid.depth);
        you.where_are_you = lid.branch;
        you.level_type = lid.level_type;
        if (you.level_type == LEVEL_PORTAL_VAULT)
            you.level_type_tag = you.level_type_name = "bazaar";
        if (!mg_do_build_level(1))
            return (false);
    }
    return (true);
}

static void mg_build_levels(int niters)
{
    mesclr();
    mprf("Generating dungeon map stats");

    for (int i = 0; i < niters; ++i)
    {
        mesclr();
        mprf("On %d of %d; %d g, %d fail, %d err%s, %d uniq, "
             "%d try, %d (%.2lf%%) vetos",
             i, niters,
             mg_levels_tried, mg_levels_failed, mapgen_errors.size(),
             mapgen_last_error.empty()? ""
             : (" (" + mapgen_last_error + ")").c_str(),
             mapgen_use_count.size(),
             mg_build_attempts, mg_vetoes,
             mg_build_attempts? mg_vetoes * 100.0 / mg_build_attempts : 0.0);

        you.uniq_map_tags.clear();
        you.uniq_map_names.clear();
        init_level_connectivity();
        if (!mg_build_dungeon())
            break;
    }
}

void mapgen_report_map_try(const map_def &map)
{
    mapgen_try_count[map.name]++;
}

void mapgen_report_map_use(const map_def &map)
{
    mapgen_use_count[map.name]++;
    mapgen_level_mapcounts[level_id::current()]++;
    mapgen_level_mapsused[level_id::current()].insert(map.name);
    mapgen_map_levelsused[map.name].insert(level_id::current());
}

void mapgen_report_error(const map_def &map, const std::string &err)
{
    mapgen_last_error = err;
}

static void _mapgen_report_available_random_vaults(FILE *outf)
{
    you.uniq_map_tags.clear();
    you.uniq_map_names.clear();

    const std::vector<level_id> places = mg_dungeon_places();
    fprintf(outf, "\n\nRandom vaults available by dungeon level:\n");

    for (std::vector<level_id>::const_iterator i = places.begin();
         i != places.end(); ++i)
    {
        fprintf(outf, "\n%s -------------\n", i->describe().c_str());
        mesclr();
        mprf("Examining random maps at %s", i->describe().c_str());
        mg_report_random_maps(outf, *i);
        if (kbhit() && getch() == ESCAPE)
            break;
        fprintf(outf, "---------------------------------\n");
    }
}

static void _check_mapless(const level_id &lid, std::vector<level_id> &mapless)
{
    if (mapgen_level_mapsused.find(lid) == mapgen_level_mapsused.end())
        mapless.push_back(lid);
}

static void _write_mapgen_stats()
{
    FILE *outf = fopen("mapgen.log", "w");
    fprintf(outf, "Map Generation Stats\n\n");
    fprintf(outf, "Levels attempted: %d, built: %d, failed: %d\n",
            mg_levels_tried, mg_levels_tried - mg_levels_failed,
            mg_levels_failed);

    if (!mapgen_errors.empty())
    {
        fprintf(outf, "\n\nMap errors:\n");
        for (std::map<std::string, std::string>::const_iterator i =
                 mapgen_errors.begin(); i != mapgen_errors.end(); ++i)
        {
            fprintf(outf, "%s: %s\n",
                    i->first.c_str(), i->second.c_str());
        }
    }

    std::vector<level_id> mapless;
    for (int i = BRANCH_MAIN_DUNGEON; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].depth == -1)
            continue;

        const branch_type br = static_cast<branch_type>(i);
        for (int dep = 1; dep <= branches[i].depth; ++dep)
        {
            const level_id lid(br, dep);
            _check_mapless(lid, mapless);
        }
    }

    _check_mapless(level_id(LEVEL_ABYSS), mapless);
    _check_mapless(level_id(LEVEL_PANDEMONIUM), mapless);
    _check_mapless(level_id(LEVEL_LABYRINTH), mapless);
    _check_mapless(level_id(LEVEL_PORTAL_VAULT), mapless);

    if (!mapless.empty())
    {
        fprintf(outf, "\n\nLevels with no maps:\n");
        for (int i = 0, size = mapless.size(); i < size; ++i)
            fprintf(outf, "%3d) %s\n", i + 1, mapless[i].describe().c_str());
    }

    _mapgen_report_available_random_vaults(outf);

    std::vector<std::string> unused_maps;
    for (int i = 0, size = map_count(); i < size; ++i)
    {
        const map_def *map = map_by_index(i);
        if (mapgen_try_count.find(map->name) == mapgen_try_count.end()
            && !map->has_tag("dummy"))
        {
            unused_maps.push_back(map->name);
        }
    }

    if (mg_vetoes)
    {
        fprintf(outf, "\n\nMost vetoed levels:\n");
        std::multimap<int, level_id> sortedvetos;
        for (std::map< level_id, std::pair<int, int> >::const_iterator
                 i = mapgen_map_builds.begin(); i != mapgen_map_builds.end();
             ++i)
        {
            if (!i->second.second)
                continue;

            sortedvetos.insert(
                std::pair<int, level_id>( i->second.second, i->first ));
        }

        int count = 0;
        for (std::multimap<int, level_id>::reverse_iterator
                 i = sortedvetos.rbegin(); i != sortedvetos.rend(); ++i)
        {
            const int vetoes = i->first;
            const int tries  = mapgen_map_builds[i->second].first;
            fprintf(outf, "%3d) %s (%d of %d vetoed, %.2f%%)\n",
                    ++count, i->second.describe().c_str(),
                    vetoes, tries, vetoes * 100.0 / tries);
        }
    }

    if (!unused_maps.empty())
    {
        fprintf(outf, "\n\nUnused maps:\n\n");
        for (int i = 0, size = unused_maps.size(); i < size; ++i)
            fprintf(outf, "%3d) %s\n", i + 1, unused_maps[i].c_str());
    }

    fprintf(outf, "\n\nMaps by level:\n\n");
    for (std::map<level_id, std::set<std::string> >::const_iterator i =
             mapgen_level_mapsused.begin(); i != mapgen_level_mapsused.end();
         ++i)
    {
        std::string line =
            make_stringf("%s ------------\n", i->first.describe().c_str());
        const std::set<std::string> &maps = i->second;
        for (std::set<std::string>::const_iterator j = maps.begin();
             j != maps.end(); ++j)
        {
            if (j != maps.begin())
                line += ", ";
            if (line.length() + j->length() > 79)
            {
                fprintf(outf, "%s\n", line.c_str());
                line = *j;
            }
            else
                line += *j;
        }

        if (!line.empty())
            fprintf(outf, "%s\n", line.c_str());

        fprintf(outf, "------------\n\n");
    }

    fprintf(outf, "\n\nMaps used:\n\n");
    std::multimap<int, std::string> usedmaps;
    for (std::map<std::string, int>::const_iterator i =
             mapgen_try_count.begin(); i != mapgen_try_count.end(); ++i)
        usedmaps.insert(std::pair<int, std::string>(i->second, i->first));

    for (std::multimap<int, std::string>::reverse_iterator i =
             usedmaps.rbegin(); i != usedmaps.rend(); ++i)
    {
        const int tries = i->first;
        std::map<std::string, int>::const_iterator iuse =
            mapgen_use_count.find(i->second);
        const int uses = iuse == mapgen_use_count.end()? 0 : iuse->second;
        if (tries == uses)
            fprintf(outf, "%4d       : %s\n", tries, i->second.c_str());
        else
            fprintf(outf, "%4d (%4d): %s\n", uses, tries, i->second.c_str());
    }

    fprintf(outf, "\n\nMaps and where used:\n\n");
    for (mapname_place_map::iterator i = mapgen_map_levelsused.begin();
         i != mapgen_map_levelsused.end(); ++i)
    {
        fprintf(outf, "%s ============\n", i->first.c_str());
        std::string line;
        for (std::set<level_id>::const_iterator j = i->second.begin();
             j != i->second.end(); ++j)
        {
            if (!line.empty())
                line += ", ";
            std::string level = j->describe();
            if (line.length() + level.length() > 79)
            {
                fprintf(outf, "%s\n", line.c_str());
                line = level;
            }
            else
                line += level;
        }
        if (!line.empty())
            fprintf(outf, "%s\n", line.c_str());

        fprintf(outf, "==================\n\n");
    }
    fclose(outf);
}

void generate_map_stats()
{
    // We have to run map preludes ourselves.
    run_map_preludes();
    mg_build_levels(SysEnv.map_gen_iters);
    _write_mapgen_stats();
}

#endif // DEBUG_DIAGNOSTICS
