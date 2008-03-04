 /*
 *  File:       debug.cc
 *  Summary:    Debug and wizard related functions.
 *  Written by: Linley Henzell and Jesse Jones
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <4>     14/12/99       LRH             Added cast_spec_spell_name()
 *               <3>     5/06/99        JDJ     Added TRACE.
 *               <2>     -/--/--        JDJ     Added a bunch od debugging macros.
 *                                                   Old code is now #if WIZARD.
 *               <1>     -/--/--        LRH     Created
 */

#include "AppHdr.h"
#include "debug.h"

#include <iostream>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#ifdef UNIX
#include <errno.h>
#endif

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "beam.h"
#include "branch.h"
#include "cio.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "direct.h"
#include "dungeon.h"
#include "effects.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "ghost.h"

#ifdef DEBUG_DIAGNOSTICS
#include "initfile.h"
#endif

#include "invent.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "items.h"

#ifdef WIZARD
#include "macro.h"
#endif

#include "makeitem.h"
#include "mapdef.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monspeak.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "newgame.h"
#include "ouch.h"
#include "place.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tiles.h"
#include "traps.h"
#include "travel.h"
#include "version.h"
#include "view.h"

// ========================================================================
//      Internal Functions
// ========================================================================

//---------------------------------------------------------------
//
// BreakStrToDebugger
//
//---------------------------------------------------------------
#if DEBUG
static void BreakStrToDebugger(const char *mesg)
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
void AssertFailed(const char *expr, const char *file, int line)
{
    char mesg[512];

    const char *fileName = file + strlen(file); // strip off path

    while (fileName > file && fileName[-1] != '\\')
        --fileName;

    sprintf(mesg, "ASSERT(%s) in '%s' at line %d failed.", expr, fileName,
            line);

    BreakStrToDebugger(mesg);
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

    BreakStrToDebugger(mesg);
}
#endif

#ifdef WIZARD

static int debug_prompt_for_monster( void )
{
    char  specs[80];

    mpr( "Which monster by name? ", MSGCH_PROMPT );
    if (!cancelable_get_line(specs, sizeof specs))
    {
        if (specs[0] == '\0')
            return (-1);

        return (get_monster_by_name(specs));
    }
    return (-1);
}
#endif

//---------------------------------------------------------------
//
// debug_prompt_for_skill
//
//---------------------------------------------------------------
#ifdef WIZARD
static int debug_prompt_for_skill( const char *prompt )
{
    char specs[80];

    mpr( prompt, MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    if (specs[0] == '\0')
        return (-1);

    int skill = -1;

    for (int i = 0; i < NUM_SKILLS; i++)
    {
        // avoid the bad values:
        if (i == SK_UNUSED_1 || (i > SK_UNARMED_COMBAT && i < SK_SPELLCASTING))
            continue;

        char sk_name[80];
        strncpy( sk_name, skill_name(i), sizeof( sk_name ) );

        char *ptr = strstr( strlwr(sk_name), strlwr(specs) );
        if (ptr != NULL)
        {
            if (ptr == sk_name && strlen(specs) > 0)
            {
                // we prefer prefixes over partial matches
                skill = i;
                break;
            }
            else
                skill = i;
        }
    }

    return (skill);
}
#endif

//---------------------------------------------------------------
//
// debug_change_species
//
//---------------------------------------------------------------
#ifdef WIZARD
void debug_change_species( void ) 
{
    char specs[80];
    int i;

    mpr( "What species would you like to be now? " , MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    if (specs[0] == '\0')
        return;
    strlwr(specs);

    species_type sp = SP_UNKNOWN;

    for (i = SP_HUMAN; i < NUM_SPECIES; i++)
    {
        const species_type si = static_cast<species_type>(i);
        const std::string sp_name =
            lowercase_string(species_name(si, you.experience_level));

        std::string::size_type pos = sp_name.find(specs);
        if (pos != std::string::npos)
        {
            if (pos == 0 && *specs)
            {
                // we prefer prefixes over partial matches
                sp = si;
                break;
            }
            else
                sp = si;
        }
    }

    if (sp == SP_UNKNOWN)
        mpr( "That species isn't available." );
    else
    {
        for (i = 0; i < NUM_SKILLS; i++)
        {
            you.skill_points[i] *= species_skills( i, sp );
            you.skill_points[i] /= species_skills( i, you.species );
        }

        you.species = sp;         
        you.is_undead = ((you.species == SP_MUMMY) ? US_UNDEAD :
                         (you.species == SP_GHOUL
                          || you.species == SP_VAMPIRE) ? US_HUNGRY_DEAD
                         : US_ALIVE);
        redraw_screen();
    }
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
#ifdef WIZARD
static int debug_prompt_for_int( const char *prompt, bool nonneg )
{
    char specs[80];

    mpr( prompt, MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
        return (nonneg ? -1 : 0);

    char *end;
    int   ret = strtol( specs, &end, 10 );

    if ((ret < 0 && nonneg) || (ret == 0 && end == specs))
        ret = (nonneg ? -1 : 0);

    return (ret);
}
#endif

/*
   Some debugging functions, accessable through keys like %, $, &, ) etc when
   a section of code in input() in acr.cc is uncommented.
 */

//---------------------------------------------------------------
//
// cast_spec_spell
//
//---------------------------------------------------------------
#ifdef WIZARD
void cast_spec_spell(void)
{
    int spell = debug_prompt_for_int( "Cast which spell by number? ", true );

    if (spell == -1)
        canned_msg( MSG_OK );
    else
        if (your_spells( static_cast<spell_type>(spell), 0, false )
            == SPRET_ABORT)
        {
            crawl_state.cancel_cmd_repeat();
        }
}
#endif


//---------------------------------------------------------------
//
// cast_spec_spell_name
//
//---------------------------------------------------------------
#ifdef WIZARD
void cast_spec_spell_name(void)
{
    char specs[80];

    mpr( "Cast which spell by name? ", MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
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


//---------------------------------------------------------------
//
// create_spec_monster
//
//---------------------------------------------------------------
#ifdef WIZARD
void create_spec_monster(void)
{
    int mon = debug_prompt_for_int( "Create which monster by number? ", true );
    
    if (mon == -1)
        canned_msg( MSG_OK );
    else
        create_monster( mon, 0, BEH_SLEEP, 
                        you.x_pos, you.y_pos, MHITNOT, MONS_PROGRAM_BUG,
                        true );
}                               // end create_spec_monster()
#endif


//---------------------------------------------------------------
//
// create_spec_monster_name
//
//---------------------------------------------------------------
#ifdef WIZARD
void create_spec_monster_name(int x, int y)
{
    char specs[100];
    mpr( "Which monster by name? ", MSGCH_PROMPT );
    if (cancelable_get_line(specs, sizeof specs) || !*specs)
    {
        canned_msg(MSG_OK);
        return;
    }

    mons_list mlist;
    std::string err = mlist.add_mons(specs);
    
    if (!err.empty())
    {
        mpr(err.c_str());
        return;
    }
    
    const bool force_place = (x != -1 && y != -1);
    if (x == -1)
        x = you.x_pos;
    if (y == -1)
        y = you.y_pos;

    mons_spec mspec = mlist.get_monster(0);
    if (!force_place && mspec.mid != -1)
    {
        coord_def place = find_newmons_square(mspec.mid, x, y);
        if (in_bounds(place))
        {
            x = place.x;
            y = place.y;
        }
    }
    
    if (!dgn_place_monster(mspec, you.your_level, x, y, false))
    {
        mpr("Unable to place monster");
        return;
    }

    // Need to set a name for the player ghost
    if (mspec.mid == MONS_PLAYER_GHOST)
    {
        unsigned short mid  = mgrd[x][y];

        if (mid >= MAX_MONSTERS || menv[mid].type != MONS_PLAYER_GHOST)
        {
            for (mid = 0; mid < MAX_MONSTERS; mid++)
                if (menv[mid].type == MONS_PLAYER_GHOST 
                    && menv[mid].alive())
                    break;
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
        mpr( "Make player ghost which species? (case-sensitive) ", MSGCH_PROMPT );
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
        
        mpr( "Make player ghost which class? ", MSGCH_PROMPT );
        get_input_line( input_str, sizeof( input_str ) );

        int class_id = get_class_by_abbrev(input_str);

        if (class_id == -1)
            class_id = get_class_by_name(input_str);

        if (class_id == -1)
        {
            mpr("No such class, making it a Fighter.");
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
static dungeon_feature_type find_appropriate_stairs(bool down)
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
                return DNGN_ROCK_STAIRS_UP;
        }
        else
        {
            mpr("Bug in determing level exit.");
            return DNGN_UNSEEN;
        }
    }

    switch(you.level_type)
    {
    case LEVEL_LABYRINTH:
        if (down)
        {
            // Can't go down in the Labyrinth
            mpr("Can't go down in the Labyrinth.");
            return DNGN_UNSEEN;
        }
        else
            return DNGN_ROCK_STAIRS_UP;
        break;

    case LEVEL_ABYSS:
        return DNGN_EXIT_ABYSS;
        break;

    case LEVEL_PANDEMONIUM:
        if (down)
            return DNGN_TRANSIT_PANDEMONIUM;
        else
            return DNGN_EXIT_PANDEMONIUM;
        break;

    case LEVEL_PORTAL_VAULT:
        return DNGN_EXIT_PORTAL_VAULT;
        break;

    default:
        mpr("Unknown level type.");
        return DNGN_UNSEEN;
    }

    mpr("Impossible occurence in find_appropriate_stairs()");
    return DNGN_UNSEEN;
}
#endif

#ifdef WIZARD
void wizard_place_stairs( bool down )
{
    dungeon_feature_type stairs = find_appropriate_stairs(down);

    if (stairs == DNGN_UNSEEN)
        return;

    grd[you.x_pos][you.y_pos] = stairs;
}
#endif

//---------------------------------------------------------------
//
// level_travel
//
//---------------------------------------------------------------
#ifdef WIZARD
void level_travel( bool down )
{
    dungeon_feature_type stairs = find_appropriate_stairs(down);

    if (stairs == DNGN_UNSEEN)
        return;

    // This lets us, for example, use &U to exit from Pandemonium and
    // &D to go to the next level.
    command_type real_dir = grid_stair_direction(stairs);
    if ((down && real_dir == CMD_GO_UPSTAIRS)
        || (!down && real_dir == CMD_GO_DOWNSTAIRS))
        down = !down;

    if (down)
        down_stairs(you.your_level, stairs);
    else
        up_stairs(stairs);
}                               // end level_travel()

static void wizard_go_to_level(const level_pos &pos)
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
    viewwindow(1, true);

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

    wizard_go_to_level(pos);
}

void debug_list_monsters()
{
    std::vector<std::string> mons;
    int nfound = 0;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        const monsters *m = &menv[i];
        if (!m->alive())
            continue;

        mons.push_back(m->name(DESC_PLAIN, true));
        nfound++;
    }
    mpr_comma_separated_list("Monsters: ", mons);
    mprf("%d monsters", nfound);
}

#endif

#ifdef WIZARD

static void rune_from_specs(const char* _specs, item_def &item)
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
        for (int i = 0; i < NUM_RUNE_TYPES; i++)
        {
            item.plus = i;

            strcpy(obj_name, item.name(DESC_PLAIN).c_str());

            if (strstr(strlwr(obj_name), specs))
                return;
        }
    }

    while (true)
    {
        mpr(
"[a] iron       [b] obsidian [c] icy      [d] bone     [e] slimy   [f] silver",
            MSGCH_PROMPT);
        mpr(
"[g] serpentine [h] elven    [i] golden   [j] decaying [k] barnacle [l] demonic",
            MSGCH_PROMPT);
        mpr(
"[m] abyssal    [n] glowing  [o] magical  [p] fiery    [q] dark    [r] buggy",
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

        item.plus = static_cast<int>(types[keyin - 'a']);

        return;
    }
}

static void deck_from_specs(const char* _specs, item_def &item)
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

    if (type_str != "")
    {
        for (int i = 0; types[i] != NUM_MISCELLANY; i++)
        {
            item.sub_type = types[i];
            item.plus     = 1;
            init_deck(item);
            // Remove "plain " from front
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

    for (int i = 0; rarities[i] != NULL; i++)
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

    int              base   = static_cast<int>(DECK_RARITY_COMMON);
    deck_rarity_type rarity =
        static_cast<deck_rarity_type>(base + rarity_val);
    item.special = rarity;

    int num = debug_prompt_for_int("How many cards? ", false);

    if (num <= 0)
    {
        canned_msg( MSG_OK );
        item.base_type = OBJ_UNASSIGNED;
        return;
    }

    item.plus = num; 

    init_deck(item);
}

static void rune_or_deck_from_specs(const char* specs, item_def &item)
{
    if (strstr(specs, "rune"))
        rune_from_specs(specs, item);
    else if (strstr(specs, "deck"))
        deck_from_specs(specs, item);
}

//---------------------------------------------------------------
//
// create_spec_object
//
//---------------------------------------------------------------
void create_spec_object()
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

    char           specs[80];
    char           obj_name[ ITEMNAME_SIZE ];
    char           keyin;

    char *         ptr;
    int            best_index;
    int            mon;
    int            i;

    object_class_type class_wanted   = OBJ_UNASSIGNED;
    int            type_wanted    = -1;
    int            special_wanted = 0;

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

    // allocate an item to play with:
    thing_created = get_item_slot();
    if (thing_created == NON_ITEM)
    {
        mpr( "Could not allocate item." );
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
        int amount = debug_prompt_for_int( "How much gold? ", true );
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
        mon = debug_prompt_for_monster();

        if (mon == -1 || mon == MONS_PROGRAM_BUG)
        {
            mpr( "No such monster." );
            return;
        }

        mitm[thing_created].base_type = OBJ_CORPSES;
        mitm[thing_created].sub_type  = CORPSE_BODY;
        mitm[thing_created].plus      = mon;
        mitm[thing_created].plus2     = 0;
        mitm[thing_created].special   = 210;
        mitm[thing_created].colour    = mons_class_colour(mon);;
        mitm[thing_created].quantity  = 1;
        mitm[thing_created].flags     = 0;
    }
    else
    {
        mpr( "What type of item? ", MSGCH_PROMPT );
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

        // In order to get the sub-type, we'll fill out the base type... 
        // then we're going to iterate over all possible subtype values 
        // and see if we get a winner. -- bwr
        mitm[thing_created].base_type = class_wanted;
        mitm[thing_created].sub_type  = 0;
        mitm[thing_created].plus      = 0;
        mitm[thing_created].plus2     = 0;
        mitm[thing_created].special   = 0;
        mitm[thing_created].flags     = 0;
        mitm[thing_created].quantity  = 1;
        set_ident_flags( mitm[thing_created], ISFLAG_IDENT_MASK );

        if (class_wanted == OBJ_MISCELLANY)
        {
            // Leaves object unmodified if it wasn't a rune or deck
            rune_or_deck_from_specs(specs, mitm[thing_created]);

            if (mitm[thing_created].base_type == OBJ_UNASSIGNED)
            {
                // Rune or deck creation canceled, clean up item
                destroy_item(thing_created);
                return;
            }
        }

        if (!mitm[thing_created].sub_type)
        {
            type_wanted = -1;
            best_index  = 10000;

            for (i = 0; i < max_subtype[ mitm[thing_created].base_type ]; i++)
            {
                mitm[thing_created].sub_type = i;
                strcpy(obj_name,mitm[thing_created].name(DESC_PLAIN).c_str());

                ptr = strstr( strlwr(obj_name), specs );
                if (ptr != NULL)
                {
                    // earliest match is the winner
                    if (ptr - obj_name < best_index)
                    {
                        mpr( obj_name );
                        type_wanted = i;    
                        best_index = ptr - obj_name;
                    }
                }
            }

            if (type_wanted == -1)
            {
                // ds -- if specs is a valid int, try using that.
                //       Since zero is atoi's copout, the wizard
                //       must enter (subtype + 1).
                if (!(type_wanted = atoi(specs)))
                {
                    mpr( "No such item." );

                    // Clean up item
                    destroy_item(thing_created);
                    return;
                }
                type_wanted--;
            }
            
            mitm[thing_created].sub_type = type_wanted;
        }
        
        switch (mitm[thing_created].base_type)
        {
        case OBJ_MISSILES:
            mitm[thing_created].quantity = 30;
            // intentional fall-through
        case OBJ_WEAPONS:
        case OBJ_ARMOUR:
            mpr( "What ego type? ", MSGCH_PROMPT );
            get_input_line( specs, sizeof( specs ) );
            
            if (specs[0] != '\0')
            {
                special_wanted = 0;
                best_index = 10000;

                for (i = 1; i < 25; i++)
                {
                    mitm[thing_created].special = i;
                    strcpy(obj_name,
                           mitm[thing_created].name(DESC_PLAIN).c_str());

                    ptr = strstr( strlwr(obj_name), strlwr(specs) );
                    if (ptr != NULL)
                    {
                        // earliest match is the winner
                        if (ptr - obj_name < best_index)
                        {
                            mpr( obj_name );
                            special_wanted = i;    
                            best_index = ptr - obj_name;
                        }
                    }
                }

                mitm[thing_created].special = special_wanted;
            }
            break;

        case OBJ_BOOKS:
            if (mitm[thing_created].sub_type == BOOK_MANUAL)
            {
                special_wanted = debug_prompt_for_skill( "A manual for which skill? " );
                if (special_wanted != -1)
                    mitm[thing_created].plus = special_wanted;
                else
                    mpr( "Sorry, no books on that skill today." );
            }
            break;
        
        case OBJ_WANDS:
            mitm[thing_created].plus = 24;
            break;

        case OBJ_STAVES:
            if (item_is_rod( mitm[thing_created] ))
            {
                mitm[thing_created].plus = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
                mitm[thing_created].plus2 = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
            }
            break;

        case OBJ_MISCELLANY:
            if (!is_rune(mitm[thing_created]) && !is_deck(mitm[thing_created]))
                mitm[thing_created].plus = 50;
            break;

        case OBJ_FOOD:
        case OBJ_SCROLLS:
        case OBJ_POTIONS:
            mitm[thing_created].quantity = 12;
            break;

        default:
            break;
        }
    }

    // Deck colour (which control rarity) already set
    if (!is_deck(mitm[thing_created]))
        item_colour( mitm[thing_created] );

    move_item_to_grid( &thing_created, you.x_pos, you.y_pos );

    if (thing_created != NON_ITEM)
    {
        origin_acquired( mitm[thing_created], AQ_WIZMODE );
        canned_msg( MSG_SOMETHING_APPEARS );
    }
}
#endif


//---------------------------------------------------------------
//
// create_spec_object
//
//---------------------------------------------------------------
#ifdef WIZARD
void tweak_object(void)
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

    for (;;)
    {
        void *field_ptr = NULL;

        for (;;) 
        {
            mpr( you.inv[item].name(DESC_INVENTORY_EQUIP).c_str() );

            mpr( "a - plus  b - plus2  c - special  d - quantity  e - flags  ESC - exit",
                 MSGCH_PROMPT );
            mpr( "Which field? ", MSGCH_PROMPT );

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

        mpr( "New value? ", MSGCH_PROMPT );
        get_input_line( specs, sizeof( specs ) );
        
        if (specs[0] == '\0')
            return;

        char *end;
        int   new_value = strtol( specs, &end, 0 );

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
#endif


//---------------------------------------------------------------
//
// stethoscope
//
//---------------------------------------------------------------
#if DEBUG_DIAGNOSTICS

void stethoscope(int mwh)
{
    struct dist stth;
    int steth_x, steth_y;
    int i;

    if (mwh != RANDOM_MONSTER)
        i = mwh;
    else
    {
        mpr( "Which monster?", MSGCH_PROMPT );

        direction( stth );

        if (!stth.isValid)
            return;

        if (stth.isTarget)
        {
            steth_x = stth.tx;
            steth_y = stth.ty;
        }
        else
        {
            steth_x = you.x_pos + stth.dx;
            steth_y = you.x_pos + stth.dy;
        }

        if (env.cgrid[steth_x][steth_y] != EMPTY_CLOUD)
        {
            mprf(MSGCH_DIAGNOSTICS, "cloud type: %d delay: %d", 
                 env.cloud[ env.cgrid[steth_x][steth_y] ].type,
                 env.cloud[ env.cgrid[steth_x][steth_y] ].decay );
        }

        if (mgrd[steth_x][steth_y] == NON_MONSTER)
        {
            mprf(MSGCH_DIAGNOSTICS, "item grid = %d", igrd[steth_x][steth_y] );
            return;
        }

        i = mgrd[steth_x][steth_y];
    }

    // print type of monster
    mprf(MSGCH_DIAGNOSTICS, "%s (id #%d; type=%d loc=(%d,%d) align=%s)",
         menv[i].name(DESC_CAP_THE, true).c_str(),
         i, menv[i].type, menv[i].x, menv[i].y,
         ((menv[i].attitude == ATT_FRIENDLY) ? "friendly" :
          (menv[i].attitude == ATT_HOSTILE)  ? "hostile" :
          (menv[i].attitude == ATT_NEUTRAL)  ? "neutral" 
                                             : "unknown alignment") );

    // print stats and other info
    mprf(MSGCH_DIAGNOSTICS,
         "HD=%d (%lu) HP=%d/%d AC=%d EV=%d MR=%d SP=%d "
         "energy=%d num=%d flags=%04lx",
         menv[i].hit_dice,
         menv[i].experience,
         menv[i].hit_points, menv[i].max_hit_points, 
         menv[i].ac, menv[i].ev,
         mons_resist_magic( &menv[i] ),
         menv[i].speed, menv[i].speed_increment,
         menv[i].number, menv[i].flags );

    // print behaviour information

    const habitat_type hab = mons_habitat( &menv[i] );

    mprf(MSGCH_DIAGNOSTICS,
         "hab=%s beh=%s(%d) foe=%s(%d) mem=%d target=(%d,%d)",
         ((hab == HT_WATER)                   ? "water" :
          (hab == HT_LAVA)                    ? "lava" :
          (hab == HT_ROCK)                    ? "rock" :
          (hab == HT_LAND)                    ? "floor"
                                              : "unknown"),
         ((menv[i].behaviour == BEH_SLEEP)    ? "sleep" :
          (menv[i].behaviour == BEH_WANDER)   ? "wander" :
          (menv[i].behaviour == BEH_SEEK)     ? "seek" :
          (menv[i].behaviour == BEH_FLEE)     ? "flee" :
          (menv[i].behaviour == BEH_CORNERED) ? "cornered"
                                              : "unknown"),
         menv[i].behaviour,
         ((menv[i].foe == MHITYOU)            ? "you" :
          (menv[i].foe == MHITNOT)            ? "none" :
          (menv[menv[i].foe].type == -1)      ? "unassigned monster"
          : menv[menv[i].foe].name(DESC_PLAIN, true).c_str()),
         menv[i].foe,
         menv[i].foe_memory,
         menv[i].target_x, menv[i].target_y );

    // print resistances
    mprf(MSGCH_DIAGNOSTICS, "resist: fire=%d cold=%d elec=%d pois=%d neg=%d",
         mons_res_fire( &menv[i] ),
         mons_res_cold( &menv[i] ),
         mons_res_elec( &menv[i] ),
         mons_res_poison( &menv[i] ),
         mons_res_negative_energy( &menv[i] ) );

    mprf(MSGCH_DIAGNOSTICS, "ench: %s",
         menv[i].describe_enchantments().c_str());

    if (menv[i].type == MONS_PLAYER_GHOST 
        || menv[i].type == MONS_PANDEMONIUM_DEMON)
    {
        ASSERT(menv[i].ghost.get());
        const ghost_demon &ghost = *menv[i].ghost;
        mprf( MSGCH_DIAGNOSTICS,
              "Ghost damage: %d; brand: %d",
              ghost.damage, ghost.brand );
    }
}                               // end stethoscope()
#endif

#if DEBUG_ITEM_SCAN
//---------------------------------------------------------------
//
// dump_item
//
//---------------------------------------------------------------
static void dump_item( const char *name, int num, const item_def &item )
{
    mpr( name, MSGCH_WARN );

    mprf("    item #%d:  base: %d; sub: %d; plus: %d; plus2: %d; special: %ld",
         num, item.base_type, item.sub_type,
             item.plus, item.plus2, item.special );

    mprf("    quant: %d; colour: %d; ident: 0x%08lx; ident_type: %d",
         item.quantity, item.colour, item.flags,
         get_ident_type( item.base_type, item.sub_type ) );

    mprf("    x: %d; y: %d; link: %d", item.x, item.y, item.link );

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
    for (int x = 0; x < GXM; x++)
    {
        for (int y = 0; y < GYM; y++)
        {
            // These are unlinked monster inventory items -- skip them:
            if (x == 0 && y == 0)
                continue;

            // Looking for infinite stacks (ie more links than tems allowed)
            // and for items which have bad coordinates (can't find their stack)
            for (int obj = igrd[x][y]; obj != NON_ITEM; obj = mitm[obj].link)
            {
                // Check for invalid (zero quantity) items that are linked in
                if (!is_valid_item( mitm[obj] ))
                {
                    mprf(MSGCH_WARN, "Linked invalid item at (%d,%d)!", x, y);
                    dump_item( mitm[obj].name(DESC_PLAIN).c_str(),
                               obj, mitm[obj] );
                }

                // Check that item knows what stack it's in
                if (mitm[obj].x != x || mitm[obj].y != y)
                {
                    mprf(MSGCH_WARN,"Item position incorrect at (%d,%d)!",x,y);
                    dump_item( mitm[obj].name(DESC_PLAIN).c_str(),
                               obj, mitm[obj] );
                }

                // If we run into a premarked item we're in real trouble,
                // this will also keep this from being an infinite loop.
                if (visited[obj])
                {
                    mprf(MSGCH_WARN,
                         "Potential INFINITE STACK at (%d, %d)", x, y);
                    break;
                }
                visited[obj] = true;
            }
        }
    }

    // Now scan all the items on the level:
    for (i = 0; i < MAX_ITEMS; i++)
    {
        if (!is_valid_item( mitm[i] ))
            continue;

        strcpy(name, mitm[i].name(DESC_PLAIN).c_str());

        // Don't check (-1,-1) player items or (0,0) monster items
        if ((mitm[i].x > 0 || mitm[i].y > 0) && !visited[i])
        {
            mpr( "Unlinked item:", MSGCH_WARN );
            dump_item( name, i, mitm[i] );
            
            mprf("igrd(%d,%d) = %d",
                 mitm[i].x, mitm[i].y, igrd[ mitm[i].x ][ mitm[i].y ] );

            // Let's check to see if it's an errant monster object:
            for (int j = 0; j < MAX_MONSTERS; j++)
            {
                for (int k = 0; k < NUM_MONSTER_SLOTS; k++)
                {
                    if (menv[j].inv[k] == i)
                    {
                        mprf("Held by monster #%d: %s at (%d,%d)", 
                             j, menv[j].name(DESC_CAP_A, true).c_str(),
                             menv[j].x, menv[j].y );
                    }
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
            mpr( "Bad item:", MSGCH_WARN );
            dump_item( name, i, mitm[i] );
        }
        else if ((mitm[i].base_type == OBJ_WEAPONS 
                && (abs(mitm[i].plus) > 30 
                    || abs(mitm[i].plus2) > 30
                    || (!is_random_artefact( mitm[i] )
                        && (mitm[i].special >= 30 
                            && mitm[i].special < 181))))

            || (mitm[i].base_type == OBJ_MISSILES 
                && (abs(mitm[i].plus) > 25 
                    || (!is_random_artefact( mitm[i] ) 
                        && mitm[i].special >= 30)))

            || (mitm[i].base_type == OBJ_ARMOUR
                && (abs(mitm[i].plus) > 25 
                    || (!is_random_artefact( mitm[i] )
                        && mitm[i].special >= 30))))
        {
            mpr( "Bad plus or special value:", MSGCH_WARN );
            dump_item( name, i, mitm[i] );
        }
    }

    // Quickly scan monsters for "program bug"s.
    for (i = 0; i < MAX_MONSTERS; i++)
    {
        const monsters& monster = menv[i];

        if (monster.type == -1)
            continue;

        if (monster.name(DESC_PLAIN, true).find("questionable") !=
            std::string::npos)
        {
            mprf( MSGCH_WARN, "Program bug detected!" );
            mprf( MSGCH_WARN,
                  "Buggy monster detected: monster #%d; position (%d,%d)",
                  i, monster.x, monster.y );
        }   
    }
}
#endif

//---------------------------------------------------------------
//
// debug_item_statistics
//
//---------------------------------------------------------------
#ifdef WIZARD
static void debug_acquirement_stats(FILE *ostat)
{
    if (grid_destroys_items(grd[you.x_pos][you.y_pos]))
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
    mpr( "[a] Weapons [b] Armours [c] Jewellery      [d] Books" );
    mpr( "[e] Staves  [f] Food    [g] Miscellaneous" );
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

    const int num_itrs = debug_prompt_for_int("How many iterations? ", true);

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

    int subtype_quants[256];
    memset(subtype_quants, 0, sizeof(subtype_quants));

    for (int i = 0; i < num_itrs; i++)
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

    if (type == OBJ_WEAPONS)
    {
        fprintf(ostat, "Maximum combined pluses: %d\n", max_plus);
        fprintf(ostat, "Average combined pluses: %5.2f\n\n",
                (float) total_plus / (float) acq_calls);
    }
    else if (type == OBJ_ARMOUR)
    {
        fprintf(ostat, "Maximum plus: %d\n", max_plus);
        fprintf(ostat, "Average plus: %5.2f\n\n",
                (float) total_plus / (float) acq_calls);
    }

    item_def item;
    item.quantity  = 1;
    item.base_type = type;

    int max_width = 0;
    for (int i = 0; i < 256; i++)
    {
        if (subtype_quants[i] == 0)
            continue;

        item.sub_type = i;

        std::string name = item.name(DESC_DBNAME, true, true);

        max_width = std::max(max_width, (int) name.length());
    }

    char format_str[80];
    sprintf(format_str, "%%%ds: %%6.2f\n", max_width);

    for (int i = 0; i < 256; i++)
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

static void debug_rap_stats(FILE *ostat)
{
    int i = prompt_invent_item(
                "Generate ranandart stats on which item?", MT_INVLIST, -1 );

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
    for (i = 0; i < RAP_NUM_PROPERTIES; i++)
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

    for (i = 0; i < RANDART_SEED_MASK; i++)
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
        for (int j = 0; j < RAP_NUM_PROPERTIES; j++)
        {
            const int val = proprt[j];
            if(val)
            {
                num_props++;
                all_props[j]++;
                switch(good_or_bad[j])
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

    for (i = 0; i < RAP_NUM_PROPERTIES; i++)
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
#ifndef DOS
        mprf("Can't write items.stat: %s", strerror(errno));
#endif
        return;
    }

    mpr( "Generate stats for: [a] aquirement [b] randart properties");

    const int keyin = tolower( get_ch() );
    switch ( keyin )
    {
    case 'a': debug_acquirement_stats(ostat); break;
    case 'b': debug_rap_stats(ostat);
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
void debug_add_skills(void)
{
    int skill = debug_prompt_for_skill( "Which skill (by name)? " );

    if (skill == -1)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr("Exercising...");
        exercise(skill, 100);
    }
}                               // end debug_add_skills()
#endif

//---------------------------------------------------------------
//
// debug_set_skills
//
//---------------------------------------------------------------
#ifdef WIZARD
void debug_set_skills(void)
{
    int skill = debug_prompt_for_skill( "Which skill (by name)? " );

    if (skill == -1)
        mpr("That skill doesn't seem to exist.");
    else
    {
        mpr( skill_name(skill) );
        int amount = debug_prompt_for_int( "To what level? ", true );

        if (amount == -1)
            canned_msg( MSG_OK );
        else
        {
            const int points = (skill_exp_needed( amount + 1 ) 
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
                you.redraw_evasion = 1;
                break;

            case SK_ARMOUR:
                you.redraw_armour_class = 1;
                you.redraw_evasion = 1;
                break;

            default:
                break;
            }
        }
    }
}                               // end debug_add_skills()
#endif


//---------------------------------------------------------------
//
// debug_set_all_skills
//
//---------------------------------------------------------------
#ifdef WIZARD
void debug_set_all_skills(void)
{
    int i;
    int amount = debug_prompt_for_int( "Set all skills to what level? ", true );

    if (amount < 0)             // cancel returns -1 -- bwr
        canned_msg( MSG_OK );
    else
    {
        if (amount > 27)
            amount = 27;

        for (i = SK_FIGHTING; i < NUM_SKILLS; i++)
        {
            if (i == SK_UNUSED_1 
                || (i > SK_UNARMED_COMBAT && i < SK_SPELLCASTING))
            {
                continue;
            }

            const int points = (skill_exp_needed( amount + 1 ) 
                                * species_skills( i, you.species )) / 100;

            you.skill_points[i] = points + 1;
            you.skills[i] = amount;
        }

        redraw_skill( you.your_name, player_title() );

        calc_total_skill_points();

        calc_hp();
        calc_mp();

        you.redraw_armour_class = 1;
        you.redraw_evasion = 1;
    }
}                               // end debug_add_skills()
#endif


//---------------------------------------------------------------
//
// debug_add_mutation
//
//---------------------------------------------------------------
#ifdef WIZARD

static const char *mutation_type_names[] = {
    "tough skin",
    "strong",
    "clever",
    "agile",
    "green scales",
    "black scales",
    "grey scales",
    "boney plates",
    "repulsion field",
    "poison resistance",
    "carnivorous",
    "herbivorous",
    "heat resistance",
    "cold resistance",
    "shock resistance",
    "regeneration",
    "fast metabolism",
    "slow metabolism",
    "weak",
    "dopey",
    "clumsy",
    "teleport control",
    "teleport",
    "magic resistance",
    "fast",
    "acute vision",
    "deformed",
    "teleport at will",
    "spit poison",
    "mapping",
    "breathe flames",
    "blink",
    "horns",
    "strong stiff",
    "flexible weak",
    "scream",
    "clarity",
    "berserk",
    "deterioration",
    "blurry vision",
    "mutation resistance",
    "frail",
    "robust",
    "torment resistance",
    "negative energy resistance",
    "summon minor demons",
    "summon demons",
    "hurl hellfire",
    "call torment",
    "raise dead",
    "control demons",
    "pandemonium",
    "death strength",
    "channel hell",
    "drain life",
    "throw flames",
    "throw frost",
    "smite",
    "claws",
    "fangs",
    "hooves",
    "talons",
    "paws",
    "breathe poison",
    "stinger",
    "big wings",
    "blue marks",
    "green marks",
    "saprovorous",
    "shaggy fur",
    "high mp",
    "low mp",
    "sleepiness",
    "",
    "",

    // from here on scales
    "red scales",
    "nacreous scales",
    "grey2 scales",
    "metallic scales",
    "black2 scales",
    "white scales",
    "yellow scales",
    "brown scales",
    "blue scales",
    "purple scales",
    "speckled scales",
    "orange scales",
    "indigo scales",
    "red2 scales",
    "iridescent scales",
    "patterned scales"
};

bool debug_add_mutation(void)
{
    bool success = false;
    char specs[80];

    if ((sizeof(mutation_type_names) / sizeof(char*)) != NUM_MUTATIONS)
    {
        mprf("Mutation name list has %d entries, but there are %d "
             "mutations total; update mutation_type_names in debug.cc "
             "to reflect current list.",
             (sizeof(mutation_type_names) / sizeof(char*)),
             (int) NUM_MUTATIONS);
        crawl_state.cancel_cmd_repeat();
        return (false);
    }

    if (you.mutation[MUT_MUTATION_RESISTANCE] > 0 &&
        !crawl_state.is_replaying_keys())
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

    if (you.mutation[MUT_MUTATION_RESISTANCE] == 3 && !force)
    {
        mpr("Can't mutate when immune to mutations without forcing it.");
        crawl_state.cancel_cmd_repeat();
        return (false);
    }

    // Yeah, the gaining message isn't too good for this... but
    // there isn't an array of simple mutation names. -- bwr
    mpr( "Which mutation ('any' for any, 'xom' for xom mutation)? ",
         MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    if (specs[0] == '\0')
        return (false);

    if (strcasecmp(specs, "any") == 0)
    {
        int old_resist = you.mutation[MUT_MUTATION_RESISTANCE];

        success = mutate(RANDOM_MUTATION, true, force);

        if (old_resist < you.mutation[MUT_MUTATION_RESISTANCE] && !force)
            crawl_state.cancel_cmd_repeat("Your mutation resistance has "
                                          "increased.");
        return (success);
    }

    if (strcasecmp(specs, "xom") == 0)
        return mutate(RANDOM_XOM_MUTATION, true, force);

    std::vector<int> partial_matches;
    mutation_type mutation = NUM_MUTATIONS;

    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (strcasecmp(specs, mutation_type_names[i]) == 0)
        {
            mutation = (mutation_type) i;
            break;
        }

        if (strstr(mutation_type_names[i] , strlwr(specs) ))
            partial_matches.push_back(i);
    }

    // If only one matching mutation, use that.
    if (mutation == NUM_MUTATIONS)
    {
        if (partial_matches.size() == 1)
            mutation = (mutation_type) partial_matches[0];
    }

    if (mutation == NUM_MUTATIONS)
    {
        crawl_state.cancel_cmd_repeat();

        if (partial_matches.size() == 0)
            mpr("No matching mutation names.");
        else
        {
            std::vector<std::string> matches;

            for (unsigned int i = 0, size = partial_matches.size();
                 i < size; i++)
            {
                matches.push_back(mutation_type_names[partial_matches[i]]);
            }
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
        mprf("Found #%d: %s (\"%s\")", (int) mutation,
             mutation_type_names[mutation], mutation_name( mutation, 1 ) );

        const int levels =
            debug_prompt_for_int( "How many levels to increase or decrease? ",
                                  false );

        if (levels == 0)
        {
            canned_msg( MSG_OK );
            success = false;
        }
        else if (levels > 0)
        {
            for (int i = 0; i < levels; i++)
            {
                if (mutate( mutation, true, force ))
                    success = true;
            }
        }
        else 
        {
            for (int i = 0; i < -levels; i++)
            {
                if (delete_mutation( mutation, force ))
                    success = true;
            }
        }
    }

    return (success);
}                               // end debug_add_mutation()
#endif


//---------------------------------------------------------------
//
// debug_get_religion
//
//---------------------------------------------------------------
#ifdef WIZARD
void debug_get_religion(void)
{
    char specs[80];

    mpr( "Which god (by name)? ", MSGCH_PROMPT );
    get_input_line( specs, sizeof( specs ) );
    
    if (specs[0] == '\0')
        return;

    strlwr(specs);

    god_type god = GOD_NO_GOD;

    for (int i = 1; i < NUM_GODS; i++)
    {
        const god_type gi = static_cast<god_type>(i);
        if ( lowercase_string(god_name(gi)).find(specs) != std::string::npos)
        {
            god = gi;
            break;
        }
    }

    if (god == GOD_NO_GOD)
        mpr( "That god doesn't seem to be taking followers today." );
    else
    {
        grd[you.x_pos][you.y_pos] =
            static_cast<dungeon_feature_type>( DNGN_ALTAR_FIRST_GOD + god - 1 );
        god_pitch(god);
    }
}                               // end debug_add_skills()
#endif


void error_message_to_player(void)
{
    mpr("Oh dear. There appears to be a bug in the program.");
    mpr("I suggest you leave this level then save as soon as possible.");

}                               // end error_message_to_player()

#ifdef WIZARD

static int create_fsim_monster(int mtype, int hp)
{
    const int mi = 
        create_monster( mtype, 0, BEH_HOSTILE, you.x_pos, you.y_pos, 
                        MHITNOT, MONS_PROGRAM_BUG );

    if (mi == -1)
        return (mi);

    monsters *mon = &menv[mi];
    mon->hit_points = mon->max_hit_points = hp;
    return (mi);
}

static skill_type fsim_melee_skill(const item_def *item)
{
    skill_type sk = SK_UNARMED_COMBAT;
    if (item)
        sk = weapon_skill(*item);
    return (sk);
}

static void fsim_set_melee_skill(int skill, const item_def *item)
{
    you.skills[fsim_melee_skill(item)] = skill;
    you.skills[SK_FIGHTING]           = skill * 15 / 27;
}

static void fsim_set_ranged_skill(int skill, const item_def *item)
{
    you.skills[range_skill(*item)] = skill;
    you.skills[SK_THROWING]        = skill * 15 / 27;
}

static void fsim_item(FILE *out, 
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

static void fsim_defence_item(FILE *out, long cum, int hits, int max,
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
                              

static bool fsim_ranged_combat(FILE *out, int wskill, int mi, 
                             const item_def *item, int missile_slot)
{
    monsters &mon = menv[mi];
    unsigned long cumulative_damage = 0L;
    unsigned long time_taken = 0L;
    long hits = 0L;
    int maxdam = 0;

    const int thrown = missile_slot == -1? get_current_fire_item() : missile_slot;
    if (thrown == ENDOFPACK || thrown == -1)
    {
        mprf("No suitable missiles for combat simulation.");
        return (false);
    }

    fsim_set_ranged_skill(wskill, item);

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

        beam.target_x = mon.x;
        beam.target_y = mon.y;
        if (throw_it(beam, thrown, true, DEBUG_COOKIE))
            hits++;

        you.hunger = hunger;
        time_taken += you.time_taken;

        int damage = (mon.max_hit_points - mon.hit_points);
        cumulative_damage += damage;
        if (damage > maxdam)
            maxdam = damage;
    }
    fsim_item(out, false, item, make_stringf("%2d", wskill).c_str(),
              cumulative_damage, iter_limit, hits, maxdam, time_taken);

    return (true);
}

static bool fsim_mon_melee(FILE *out, int dodge, int armour, int mi)
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
        monster_attack(mi);
        const int damage = you.hp_max - you.hp;
        if (damage)
            hits++;
        cumulative_damage += damage;
        if (damage > maxdam)
            maxdam = damage;
    }

    you.hp = yhp;
    you.hp_max = ymhp;

    fsim_defence_item(out, cumulative_damage, hits, maxdam, menv[mi].speed,
                      Options.fsim_rounds);
    return (true);
}

static bool fsim_melee_combat(FILE *out, int wskill, int mi, 
                             const item_def *item)
{
    monsters &mon = menv[mi];
    unsigned long cumulative_damage = 0L;
    unsigned long time_taken = 0L;
    long hits = 0L;
    int maxdam = 0;

    fsim_set_melee_skill(wskill, item);

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
    fsim_item(out, true, item, make_stringf("%2d", wskill).c_str(),
              cumulative_damage, iter_limit, hits, maxdam, time_taken);

    return (true);
}

static bool debug_fight_simulate(FILE *out, int wskill, int mi, int miss_slot)
{
    int weapon = you.equip[EQ_WEAPON];
    const item_def *iweap = weapon != -1? &you.inv[weapon] : NULL;

    if (iweap && iweap->base_type == OBJ_WEAPONS && is_range_weapon(*iweap))
        return fsim_ranged_combat(out, wskill, mi, iweap, miss_slot);
    else
        return fsim_melee_combat(out, wskill, mi, iweap);
}

static const item_def *fsim_weap_item()
{
    const int weap = you.equip[EQ_WEAPON];
    if (weap == -1)
        return NULL;

    return &you.inv[weap];
}

static std::string fsim_wskill(int missile_slot)
{
    const item_def *iweap = fsim_weap_item();
    if (!iweap && missile_slot != -1)
        return skill_name(range_skill(you.inv[missile_slot]));
    
    return iweap && iweap->base_type == OBJ_WEAPONS 
             && is_range_weapon(*iweap)?
                        skill_name( range_skill(*iweap) ) :
            iweap?      skill_name( fsim_melee_skill(iweap) ) :
                        skill_name( SK_UNARMED_COMBAT );
}

static std::string fsim_weapon(int missile_slot)
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
                    missile_slot == -1? get_current_fire_item() :
                    missile_slot;
                if (missile < ENDOFPACK)
                    return item_buf + " with "
                        + you.inv[missile].name(DESC_PLAIN);
            }
        }
        else
            return you.inv[missile_slot].name(DESC_PLAIN);
    }
    else
    {
        return "unarmed";
    }
    return item_buf;
}

static std::string fsim_time_string()
{
    time_t curr_time = time(NULL);
    struct tm *ltime = localtime(&curr_time);
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

static void fsim_mon_stats(FILE *o, const monsters &mon)
{
    fprintf(o, "Monster   : %s\n", mon.name(DESC_PLAIN, true).c_str());
    fprintf(o, "HD        : %d\n", mon.hit_dice);
    fprintf(o, "AC        : %d\n", mon.ac);
    fprintf(o, "EV        : %d\n", mon.ev);
}

static void fsim_title(FILE *o, int mon, int ms)
{
    fprintf(o, CRAWL " version " VERSION "\n\n");
    fprintf(o, "Combat simulation: %s %s vs. %s (%ld rounds) (%s)\n",
            species_name(you.species, you.experience_level).c_str(),
            you.class_name,
            menv[mon].name(DESC_PLAIN, true).c_str(),
            Options.fsim_rounds,
            fsim_time_string().c_str());
    fprintf(o, "Experience: %d\n", you.experience_level);
    fprintf(o, "Strength  : %d\n", you.strength);
    fprintf(o, "Intel.    : %d\n", you.intel);
    fprintf(o, "Dexterity : %d\n", you.dex);
    fprintf(o, "Base speed: %d\n", player_speed());
    fprintf(o, "\n");
    fsim_mon_stats(o, menv[mon]);
    fprintf(o, "\n");
    fprintf(o, "Weapon    : %s\n", fsim_weapon(ms).c_str());
    fprintf(o, "Skill     : %s\n", fsim_wskill(ms).c_str());
    fprintf(o, "\n");
    fprintf(o, "Skill | Accuracy | Av.Dam | Av.HitDam | Eff.Dam | Max.Dam | Av.Time\n");
}

static void fsim_defence_title(FILE *o, int mon)
{
    fprintf(o, CRAWL " version " VERSION "\n\n");
    fprintf(o, "Combat simulation: %s vs. %s %s (%ld rounds) (%s)\n",
            menv[mon].name(DESC_PLAIN).c_str(),
            species_name(you.species, you.experience_level).c_str(),
            you.class_name,
            Options.fsim_rounds,
            fsim_time_string().c_str());
    fprintf(o, "Experience: %d\n", you.experience_level);
    fprintf(o, "Strength  : %d\n", you.strength);
    fprintf(o, "Intel.    : %d\n", you.intel);
    fprintf(o, "Dexterity : %d\n", you.dex);
    fprintf(o, "Base speed: %d\n", player_speed());
    fprintf(o, "\n");
    fsim_mon_stats(o, menv[mon]);
    fprintf(o, "\n");
    fprintf(o, "AC | EV | Dod | Arm | Acc | Av.Dam | Av.HitDam | Eff.Dam | Max.Dam | Av.Time\n");
}

static int cap_stat(int stat)
{
    return (stat <  1 ? 1  :
            stat > 127 ? 127 :
                        stat);
}

static bool fsim_mon_hit_you(FILE *ostat, int mindex, int)
{
    fsim_defence_title(ostat, mindex);

    for (int sk = 0; sk <= 27; ++sk)
    {
        mesclr();
        mprf("Calculating average damage for %s at dodging %d",
             menv[mindex].name(DESC_PLAIN).c_str(),
             sk);

        if (!fsim_mon_melee(ostat, sk, 0, mindex))
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

        if (!fsim_mon_melee(ostat, 0, sk, mindex))
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

static bool fsim_you_hit_mon(FILE *ostat, int mindex, int missile_slot)
{
    fsim_title(ostat, mindex, missile_slot);
    for (int wskill = 0; wskill <= 27; ++wskill)
    {
        mesclr();
        mprf("Calculating average damage for %s at skill %d",
                fsim_weapon(missile_slot).c_str(), wskill);
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
    mprf("Done fight simulation with %s", fsim_weapon(missile_slot).c_str());
    return (true);
}

static bool debug_fight_sim(int mindex, int missile_slot,
                            bool (*combat)(FILE *, int mind, int mslot))
{
    FILE *ostat = fopen("fight.stat", "a");
    if (!ostat)
    {
        // I'm not sure what header provides errno on djgpp,
        // and it's insufficiently important for a wizmode-only
        // feature.
#ifndef DOS
        mprf("Can't write fight.stat: %s", strerror(errno));
#endif
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

    int mindex = create_fsim_monster(punching_bag, 500);
    if (mindex == -1)
    {
        mprf("Failed to create punching bag");
        return;
    }

    you.exp_available = 0;
    
    if (!use_defaults || defence)
        debug_fight_sim(mindex, -1,
                        defence? fsim_mon_hit_you : fsim_you_hit_mon);
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
            if (!debug_fight_sim(mindex, missile, fsim_you_hit_mon))
                break;
        }
    }
    monster_die(&menv[mindex], KILL_DISMISSED, 0);    
}

static int find_trap_slot()
{
    for (int i = 0; i < MAX_TRAPS; ++i)
    {
        if (env.trap[i].type == TRAP_UNASSIGNED)
            return (i);
    }
    return (-1);
}

void debug_make_trap()
{
    char requested_trap[80];
    int trap_slot = find_trap_slot();
    trap_type trap = TRAP_UNASSIGNED;
    int gridch = grd[you.x_pos][you.y_pos];

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
    std::vector<int>         matches;
    std::vector<std::string> match_names;
    for (int t = TRAP_DART; t < NUM_TRAPS; ++t)
    {
        if (strstr(requested_trap, 
                   trap_name(trap_type(t))))
        {
            trap = trap_type(t);
            break;
        }
        else if (strstr(trap_name(trap_type(t)), requested_trap))
        {
            matches.push_back(t);
            match_names.push_back(trap_name(trap_type(t)));
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
            trap = trap_type(matches[0]);
        else
        {
            std::string prefix = "No exact match for trap '";
            prefix += requested_trap;
            prefix += "', possible matches are: ";
            mpr_comma_separated_list(prefix, match_names);

            return;
        }
    }

    place_specific_trap(you.x_pos, you.y_pos, trap);

    mprf("Created a %s trap, marked it undiscovered",
            trap_name(trap));
}

void debug_make_shop()
{
    char requested_shop[80];
    int gridch = grd[you.x_pos][you.y_pos];
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

    place_spec_shop(you.your_level, you.x_pos, you.y_pos, 
                    new_shop_type, representative);
    link_items();
    mprf("Done.");
}

void debug_set_stats()
{
    char buf[80];
    mprf(MSGCH_PROMPT, "Enter values for Str, Int, Dex (space separated): ");
    if (cancelable_get_line(buf, sizeof buf))
        return;

    int sstr = you.strength,
        sdex = you.dex,
        sint = you.intel;
    sscanf(buf, "%d %d %d", &sstr, &sint, &sdex);

    you.max_strength = you.strength = cap_stat(sstr);
    you.max_dex      = you.dex      = cap_stat(sdex);
    you.max_intel    = you.intel    = cap_stat(sint);

    you.redraw_strength = true;
    you.redraw_dexterity = true;
    you.redraw_intelligence = true;
    you.redraw_evasion = true;
}

void debug_card()
{
    msg::streams(MSGCH_PROMPT) << "Which card? " << std::endl;
    char buf[80];
    if (cancelable_get_line(buf, sizeof buf))
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
        lose_level();
}

void debug_set_xl()
{
    mprf(MSGCH_PROMPT, "Enter new experience level: ");
    char buf[30];
    if (cancelable_get_line(buf, sizeof buf))
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
    int map = find_map_by_name(name);
    if (map == -1)
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

            map = find_map_by_name(matches[0]);
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

    const map_def *toplace = map_by_index(map);
    coord_def where(-1, -1);
    if ((toplace->orient == MAP_FLOAT || toplace->orient == MAP_NONE)
        && place_on_us)
    {
        where = you.pos();
    }
    
    if (dgn_place_map(map, false, true, false, where))
        mprf("Successfully placed %s.", map_by_index(map)->name.c_str());
    else
        mprf("Failed to place %s.", map_by_index(map)->name.c_str());
}

void debug_place_map()
{
    char what_to_make[100];
    mesclr();
    mprf(MSGCH_PROMPT, "Enter map name: ");
    if (cancelable_get_line(what_to_make, sizeof what_to_make))
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

void debug_dismiss_all_monsters(bool force_all)
{
    char buf[80];
    if (!force_all)
    {
        mpr("Regex of monsters to dismiss (ENTER for all): ", MSGCH_PROMPT);
        bool validline = !cancelable_get_line(buf, sizeof buf, 80);

        if (!validline)
        {
            canned_msg( MSG_OK );
            return;
        }
    }

    // Dismiss all
    if (buf[0] == '\0' || force_all)
    {
        // Genocide... "unsummon" all the monsters from the level.
        for (int mon = 0; mon < MAX_MONSTERS; mon++)
        {
            monsters *monster = &menv[mon];

            if (monster->alive())
                monster_die(monster, KILL_DISMISSED, 0);
        }
        return;
    }

    // Dismiss by regex
    text_pattern tpat(buf);
    for (int mon = 0; mon < MAX_MONSTERS; mon++)
    {
        monsters *monster = &menv[mon];

        if (monster->alive() && tpat.matches(monster->name(DESC_PLAIN)))
            monster_die(monster, KILL_DISMISSED, 0);
    }
}

void debug_kill_traps()
{
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
            if (grid_is_trap(grd[x][y]) || grd[x][y] == DNGN_UNDISCOVERED_TRAP)
                grd[x][y] = DNGN_FLOOR;
}

static int debug_time_explore()
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

static void debug_destroy_doors()
{
    for (int y = 0; y < GYM; ++y)
    {
        for (int x = 0; x < GXM; ++x)
        {
            const dungeon_feature_type feat = grd[x][y];
            if (feat == DNGN_CLOSED_DOOR || feat == DNGN_SECRET_DOOR)
                grd[x][y] = DNGN_FLOOR;
        }
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
    debug_dismiss_all_monsters(true);
    debug_kill_traps();
    forget_map(100);

    debug_destroy_doors();

    // Remember where we are now.
    const coord_def where = you.pos();

    const int explore_turns = debug_time_explore();

    // Return to starting point.
    you.moveto(where);

    mprf("Explore took %d turns.", explore_turns);
}

#endif

#ifdef WIZARD
extern void force_monster_shout(monsters* monster);

void debug_make_monster_shout(monsters* mon)
{

    mpr("Make the monster (S)hout or (T)alk?", MSGCH_PROMPT);

    char type = (char) getchm(KC_DEFAULT);
    type = tolower(type);

    if (type != 's' && type != 't')
    {
        canned_msg( MSG_OK );
        return;
    }

    int num_times = debug_prompt_for_int("How many times? ", false);

    if (num_times <= 0)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (type == 's')
    {
        if (silenced(you.x_pos, you.y_pos))
            mpr("You are silenced and likely won't hear any shouts.");
        else if (silenced(mon->x, mon->y))
            mpr("The monster is silenced and likely won't give any shouts.");

        for (int i = 0; i < num_times; i++)
            force_monster_shout(mon);
    }
    else
    {
        if (mon->invisible())
            mpr("The monster is invisible and likely won't speak.");

        if (silenced(you.x_pos, you.y_pos) && !silenced(mon->x, mon->y))
            mpr("You are silenced but the monster isn't; you will "
                "probably hear/see nothing.");
        else if (!silenced(you.x_pos, you.y_pos) && silenced(mon->x, mon->y))
            mpr("The monster is silenced and likely won't say anything.");
        else if (silenced(you.x_pos, you.y_pos) && silenced(mon->x, mon->y))
            mpr("Both you and the monster are silenced, so you likely "
                "won't hear anything.");

        for (int i = 0; i< num_times; i++)
            mons_speaks(mon);
    }

    mpr("== Done ==");
}
#endif

#ifdef WIZARD
void wizard_give_monster_item(monsters *mon)
{
    mon_itemuse_type item_use = mons_itemuse( mon->type );
    if (item_use < MONUSE_STARTING_EQUIPMENT)
    {
        mpr("That type of monster can't use any items.");
        return;
    }

    int player_slot = prompt_invent_item( "Give which item to monster?",
                                          MT_DROP, -1 );

    if (player_slot == PROMPT_ABORT)
        return;

    for (int i = 0; i < NUM_EQUIP; i++)
        if (you.equip[i] == player_slot)
        {
            mpr("Can't give equipped items to a monster.");
            return;
        }

    item_def     &item = you.inv[player_slot];
    mon_inv_type mon_slot;

    switch(item.base_type)
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
        mon_slot = MSLOT_ARMOUR;
        break;
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
    default:
        mpr("You can't give that type of item to a monster.");
        return;
    }

    // Shouldn't be be using MONUSE_MAGIC_ITEMS?
    if (item_use == MONUSE_STARTING_EQUIPMENT 
        && !mons_is_unique( mon->type ))
    {
        switch(mon_slot)
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

    // Move monster's old item to player's inventory as last step
    int old_eq = NON_ITEM;
    if (mon->inv[mon_slot] != NON_ITEM)
    {
        old_eq = mon->inv[mon_slot];

        // Alternative weapons don't get (un)wielded unless the monster
        // can wield two weapons.
        if (mon_slot != MSLOT_ALT_WEAPON || mons_wields_two_weapons(mon))
            mon->unequip(*(mon->mslot_item(mon_slot)), mon_slot, 1, true);
    }

    item_def &new_item = mitm[index];
    new_item = item;
    new_item.link = NON_ITEM;
    new_item.x    = 0;
    new_item.y    = 0;

    mon->inv[mon_slot] = index;

    // Alternative weapons don't get (un)wielded unless the monster
    // can wield two weapons.
    if (mon_slot != MSLOT_ALT_WEAPON || mons_wields_two_weapons(mon))
        mon->equip(new_item, mon_slot, 1);

    // Item is gone from player's inventory
    dec_inv_item_quantity(player_slot, item.quantity);

    // Monster's old item moves to player's inventory.
    if (old_eq != NON_ITEM)
    {
        mpr("Fetching monster's old item.");
        move_item_to_player(old_eq, mitm[old_eq].quantity);
    }
}
#endif

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

static bool mg_do_build_level(int niters)
{
    if (niters > 1)
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
    }

    no_messages mx;
    for (int i = 0; i < niters; ++i)
    {
        if (kbhit() && getch() == ESCAPE)
            return (false);

        ++mg_levels_tried;
        if (!builder(you.your_level, you.level_type))
            ++mg_levels_failed;
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

static void mg_build_dungeon()
{
    const std::vector<level_id> places = mg_dungeon_places();

    for (int i = 0, size = places.size(); i < size; ++i)
    {
        const level_id &lid = places[i];
        you.your_level = absdungeon_depth(lid.branch, lid.depth);
        you.where_are_you = lid.branch;
        you.level_type = lid.level_type;
        if (you.level_type == LEVEL_PORTAL_VAULT)
            you.level_type_name = "bazaar";
        if (!mg_do_build_level(1))
            return;
    }
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
        mg_build_dungeon();
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

static void mapgen_report_avaiable_random_vaults(FILE *outf)
{
    you.uniq_map_tags.clear();
    you.uniq_map_names.clear();
    
    const std::vector<level_id> places = mg_dungeon_places();
    fprintf(outf, "\n\nRandom vaults available by dungeon level:\n");

    for (std::vector<level_id>::const_iterator i = places.begin();
         i != places.end(); ++i)
    {
        fprintf(outf, "\n%s -------------\n", i->describe().c_str());
        mg_report_random_maps(outf, *i);
        fprintf(outf, "---------------------------------\n");
    }
}

static void check_mapless(const level_id &lid, std::vector<level_id> &mapless)
{
    if (mapgen_level_mapsused.find(lid) == mapgen_level_mapsused.end())
        mapless.push_back(lid);
}

static void write_mapgen_stats()
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
            check_mapless(lid, mapless);
        }
    }

    check_mapless(level_id(LEVEL_ABYSS), mapless);
    check_mapless(level_id(LEVEL_PANDEMONIUM), mapless);
    check_mapless(level_id(LEVEL_LABYRINTH), mapless);
    check_mapless(level_id(LEVEL_PORTAL_VAULT), mapless);

    if (!mapless.empty())
    {
        fprintf(outf, "\n\nLevels with no maps:\n");
        for (int i = 0, size = mapless.size(); i < size; ++i)
            fprintf(outf, "%3d) %s\n", i + 1, mapless[i].describe().c_str());
    }

    mapgen_report_avaiable_random_vaults(outf);

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
    write_mapgen_stats();
}

#endif // DEBUG_DIAGNOSTICS
