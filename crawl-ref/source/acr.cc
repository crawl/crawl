/*
 *  File:       acr.cc
 *  Summary:    Main entry point, event loop, and some initialization functions
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <18> 7/29/00         JDJ             values.h isn't included on Macs
 * <17> 19jun2000       GDL             added Windows console support
 * <16> 06mar2000       bwr             changes to berserk
 * <15> 09jan2000       BCR             new Wiz command: blink
 * <14> 10/13/99        BCR             Added auto door opening,
 *                                       move "you have no
 *                                       religion" to describe.cc
 * <13> 10/11/99        BCR             Added Daniel's wizard patch
 * <12> 10/9/99         BCR             swapped 'v' and 'V' commands,
 *                                       added wizard help command
 * <11> 10/7/99         BCR             haste and slow now take amulet of
 *                                       resist slow into account
 * <10> 9/25/99         CDL             Changes to Linux input
 *                                       switch on command enums
 * <9>  6/12/99         BWR             New init code, restructured
 *                                       wiz commands, added equipment
 *                                       listing commands
 * <8>   6/7/99         DML             Autopickup
 * <7>  5/30/99         JDJ             Added game_has_started.
 * <6>  5/25/99         BWR             Changed move() to move_player()
 * <5>  5/20/99         BWR             New berserk code, checking
 *                                       scan_randarts for NO_TELEPORT
 *                                       and NO_SPELLCASTING.
 * <4>  5/12/99         BWR             Solaris support.
 * <3>  5/09/99         JDJ             look_around no longer prints a prompt.
 * <2>  5/08/99         JDJ             you and env are no longer arrays.
 * <1>  -/--/--         LRH             Created
 */

#include "AppHdr.h"

#include <string>

// I don't seem to need values.h for VACPP..
#if !defined(__IBMCPP__)
#include <limits.h>
#endif

#if DEBUG
  // this contains the DBL_MAX constant
  #include <float.h>
#endif

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sstream>
#include <iostream>

#ifdef DOS
#include <dos.h>
#include <conio.h>
#include <file.h>
#endif

#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "abyss.h"
#include "branch.h"
#include "chardump.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "command.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "describe.h"
#include "direct.h"
#include "dungeon.h"
#include "effects.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "hiscores.h"
#include "initfile.h"
#include "invent.h"
#include "item_use.h"
#include "it_use2.h"
#include "it_use3.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "lev-pand.h"
#include "luadgn.h"
#include "macro.h"
#include "makeitem.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "newgame.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "overmap.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spells1.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "tags.h"
#include "terrain.h"
#include "transfor.h"
#include "traps.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"
#include "stash.h"
#include "xom.h"

#include "tiles.h"

// ----------------------------------------------------------------------
// Globals whose construction/destruction order needs to be managed
// ----------------------------------------------------------------------

CLua clua(true);
CLua dlua(false);		// Lua interpreter for the dungeon builder.
crawl_environment env;	// Requires dlua
player you;
system_environment SysEnv;
game_state crawl_state;



std::string init_file_location; // externed in newgame.cc

char info[ INFO_SIZE ];         // messaging queue extern'd everywhere {dlb}

int stealth;                    // externed in view.cc

static key_recorder repeat_again_rec;

// Clockwise, around the compass from north (same order as enum RUN_DIR)
const struct coord_def Compass[8] = 
{ 
    coord_def(0, -1), coord_def(1, -1), coord_def(1, 0), coord_def(1, 1),
    coord_def(0, 1), coord_def(-1, 1), coord_def(-1, 0), coord_def(-1, -1),
};

// Functions in main module
static void do_berserk_no_combat_penalty(void);
static bool initialise(void);
static void input(void);
static void move_player(int move_x, int move_y);
static void open_door(int move_x, int move_y, bool check_confused = true);
static void close_door(int move_x, int move_y);
static void start_running( int dir, int mode );

static void prep_input();
static void input();
static void world_reacts();
static command_type get_next_cmd();
static keycode_type get_next_keycode();
static command_type keycode_to_command( keycode_type key );
static void setup_cmd_repeat();
static void do_prev_cmd_again();
static void update_replay_state();

static void show_commandline_options_help();
static void wanderer_startup_message();
static void god_greeting_message( bool game_start );
static void take_starting_note();
static void startup_tutorial();

#ifdef DGL_SIMPLE_MESSAGING
static void read_messages();
#endif

static void compile_time_asserts();

/*
   It all starts here. Some initialisations are run first, then straight to
   new_game and then input.
*/
#ifdef WIN32TILES
int old_main( int argc, char *argv[] )
#else
int main( int argc, char *argv[] )
#endif
{
    compile_time_asserts();     // just to quiet "unused static function" warning
    // Load in the system environment variables
    get_system_environment();

    // parse command line args -- look only for initfile & crawl_dir entries
    if (!parse_args(argc, argv, true))
    {
        show_commandline_options_help();
        return 1;
    }

    // Init monsters up front - needed to handle the mon_glyph option right.
    init_monsters(mcolour);
    
    // Read the init file.
    init_file_location = read_init_file();

    // now parse the args again, looking for everything else.
    parse_args( argc, argv, false );

    if (Options.sc_entries != 0 || !SysEnv.scorefile.empty())
    {
        hiscores_print_all( Options.sc_entries, Options.sc_format );
        return 0;
    }
    else
    {
        // Don't allow scorefile override for actual gameplay, only for
        // score listings.
        SysEnv.scorefile.clear();
    }

    const bool game_start = initialise();

    // override some options for tutorial
    init_tutorial_options();

    msg::stream << "Welcome, " << you.your_name << " the "
                << species_name( you.species,you.experience_level )
                << " " << you.class_name << "."
                << std::endl;

    // Activate markers only after the welcome message, so the
    // player can see any resulting messages.
    env.markers.activate_all();

    if (game_start && you.char_class == JOB_WANDERER)
        wanderer_startup_message();

    god_greeting_message( game_start );

    // warn player about their weapon, if unsuitable
    wield_warning(false);

    if ( game_start )   
    {
        if (Options.tutorial_left)
            startup_tutorial();
        take_starting_note();
    }

    while (true)
        input();

    // Should never reach this stage, right?
#if defined(USE_TILE)
    libgui_shutdown();
#elif defined(UNIX)
    unixcurses_shutdown();
#endif

    return 0;
}                               // end main()

static void show_commandline_options_help()
{
    puts("Command line options:");
    puts("  -name <string>   character name");
    puts("  -race <arg>      preselect race (by letter, abbreviation, or name)");
    puts("  -class <arg>     preselect class (by letter, abbreviation, or name)");
    puts("  -pizza <string>  crawl pizza");
    puts("  -plain           don't use IBM extended characters");
    puts("  -dir <path>      crawl directory");
    puts("  -rc <file>       init file name");
    puts("  -morgue <dir>    directory to save character dumps");
    puts("  -macro <dir>     directory to save/find macro.txt");
    puts("");
    puts("Command line options override init file options, which override");
    puts("environment options (CRAWL_NAME, CRAWL_PIZZA, CRAWL_DIR, CRAWL_RC).");
    puts("");
    puts("Highscore list options: (Can now be redirected to more, etc)");
    puts("  -scores [N]            highscore list");
    puts("  -tscores [N]           terse highscore list");
    puts("  -vscores [N]           verbose highscore list");
    puts("  -scorefile <filename>  scorefile to report on");
}

static void wanderer_startup_message()
{
    int skill_levels = 0;
    for (int i = 0; i < NUM_SKILLS; i++)
        skill_levels += you.skills[ i ];
    
    if (skill_levels <= 2)
    {
        // Demigods and Demonspawn wanderers stand to not be
        // able to see any of their skills at the start of
        // the game (one or two skills should be easily guessed
        // from starting equipment)... Anyways, we'll give the
        // player a message to warn them (and give a reason why). -- bwr
        mpr("You wake up in a daze, and can't recall much.");
    }
}

static void god_greeting_message( bool game_start )
{
    switch (you.religion)
    {
    case GOD_ZIN:
        simple_god_message( " says: Spread the light, my child." );
        break;
    case GOD_SHINING_ONE:
        simple_god_message( " says: Smite the infidels!" );
        break;
    case GOD_KIKUBAAQUDGHA:
    case GOD_YREDELEMNUL:
    case GOD_NEMELEX_XOBEH:
        simple_god_message( " says: Welcome..." );
        break;
    case GOD_XOM:
        if (game_start)
            simple_god_message( " says: A new plaything!" );
        break;
    case GOD_VEHUMET:
        simple_god_message(" says: Let it end in hellfire!");
        break;
    case GOD_OKAWARU:
        simple_god_message(" says: Welcome, disciple.");
        break;
    case GOD_MAKHLEB:
        god_speaks( you.religion, "Blood and souls for Makhleb!" );
        break;
    case GOD_SIF_MUNA:
        simple_god_message( " whispers: I know many secrets...");
        break;
    case GOD_TROG:
        simple_god_message( " says: Kill them all!" );
        break;
    case GOD_ELYVILON:
        simple_god_message( " says: Go forth and aid the weak!" );
        break;
    case GOD_LUGONU:
        simple_god_message( " says: Spread carnage and corruption!");
        break;
    case GOD_BEOGH:
        simple_god_message(
            " says: Drown the unbelievers in a sea of blood!");
        break;
    case GOD_NO_GOD:
    case NUM_GODS:
    case GOD_RANDOM:
        break;
    }
}

static void take_starting_note()
{
    std::ostringstream notestr;
    notestr << you.your_name << ", the "
            << species_name(you.species,you.experience_level) << " "
            << you.class_name
            << ", began the quest for the Orb.";
    take_note(Note(NOTE_MESSAGE, 0, 0, notestr.str().c_str()));

    notestr.str("");
    notestr.clear();

    notestr << "HP: " << you.hp << "/" << you.hp_max
            << " MP: " << you.magic_points << "/" << you.max_magic_points;
    take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0,
                   notestr.str().c_str()));
}

static void startup_tutorial()
{
    // don't allow triggering at game start 
    Options.tut_just_triggered = true;
    // print stats and everything
    prep_input();
    msg::streams(MSGCH_TUTORIAL)
        << "Press any key to start the tutorial intro, or Escape to skip it."
        << std::endl;
    const int ch = c_getch();

    if (ch != ESCAPE)
        tut_starting_screen();
}

#ifdef WIZARD
static void handle_wizard_command( void )
{
    int   wiz_command, i, j, tmp;
    char  specs[256];

    // WIZ_NEVER gives protection for those who have wiz compiles,
    // and don't want to risk their characters.
    if (Options.wiz_mode == WIZ_NEVER)
        return;

    if (!you.wizard)
    {
        mpr( "WARNING: ABOUT TO ENTER WIZARD MODE!", MSGCH_WARN );

#ifndef SCORE_WIZARD_MODE
        mpr( "If you continue, your game will not be scored!", MSGCH_WARN );
#endif

        if (!yesno( "Do you really want to enter wizard mode?", false, 'n' ))
            return;

        you.wizard = true;
        redraw_screen();

        if (crawl_state.cmd_repeat_start)
        {
            crawl_state.cancel_cmd_repeat("Can't repeat entering wizard "
                                          "mode.");
            return;
        }
    }

    mpr( "Enter Wizard Command (? - help): ", MSGCH_PROMPT );
    wiz_command = getch();

    if (crawl_state.cmd_repeat_start)
    {
        // Easiest to list which wizard commands *can* be repeated.
        switch (wiz_command)
        {
        case 'x':
        case '$':
        case 'a':
        case 'c':
        case 'h':
        case 'H':
        case 'm':
        case 'M':
        case 'X':
        case '!':
        case '[':
        case ']':
        case '^':
        case '%':
        case 'o':
        case 'z':
        case 'Z':
            break;

        default:
            crawl_state.cant_cmd_repeat("You cannot repeat that "
                                        "wizard command.");
            return;
        }
    }

    switch (wiz_command)
    { 
    case '?':
        list_commands(true, 0, true);  // tell it to list wizard commands
        break;

    case CONTROL('G'):
        save_ghost(true);
        break; 

    case 'x':
        you.experience = 1 + exp_needed( 2 + you.experience_level );
        level_change();
        break;

    case CONTROL('X'):
        debug_set_xl();
        break;

    case 'O':
        debug_test_explore();
        break;

    case 's':
        you.exp_available = 20000;
        you.redraw_experience = 1;
        break;

    case 'S':
        debug_set_skills();
        break;

    case 'A':
        debug_set_all_skills();
        break;

    case '$':
        you.gold += 1000;
        you.redraw_gold = 1;
        break;

    case 'a':
        acquirement( OBJ_RANDOM, AQ_WIZMODE );
        break;

    case 'v':
        // this command isn't very exciting... feel free to replace
        i = prompt_invent_item( "Value of which item?", MT_INVLIST, -1 );
        if (i == PROMPT_ABORT || !is_random_artefact( you.inv[i] ))
        {
            canned_msg( MSG_OK );
            break;
        }
        else
        {
            mprf("randart val: %d", randart_value(you.inv[i])); 
        }
        break;

    case '+':
        i = prompt_invent_item(
                "Make an artefact out of which item?", MT_INVLIST, -1 );
        if (i == PROMPT_ABORT)
        {
            canned_msg( MSG_OK );
            break;
        }

        // set j == equipment slot of chosen item, remove old randart benefits
        for (j = 0; j < NUM_EQUIP; j++)
        {
            if (you.equip[j] == i)
            {
                if (j == EQ_WEAPON)
                    you.wield_change = true;

                if (is_random_artefact( you.inv[i] ))
                    unuse_randart( i );

                break;
            }
        }

        make_item_randart( you.inv[i] );

        // if equipped, apply new randart benefits
        if (j != NUM_EQUIP)
            use_randart( i );

        mpr( you.inv[i].name(DESC_INVENTORY_EQUIP).c_str() );
        break;

    case '|':
        // create all unrandarts
        for (tmp = 1; tmp < NO_UNRANDARTS; tmp++)
        {
            int islot = get_item_slot();
            if (islot == NON_ITEM)
                break;

            make_item_unrandart( mitm[islot], tmp );

            mitm[ islot ].quantity = 1;
            set_ident_flags( mitm[ islot ], ISFLAG_IDENT_MASK );

            msg::streams(MSGCH_DIAGNOSTICS) << "Made "
                                            << mitm[islot].name(DESC_NOCAP_A)
                                            << std::endl;

            move_item_to_grid( &islot, you.x_pos, you.y_pos );
        }

        // create all fixed artefacts
        for (tmp = SPWPN_START_FIXEDARTS;
             tmp < SPWPN_START_NOGEN_FIXEDARTS; tmp++)
        {
            int islot = get_item_slot();
            if (islot == NON_ITEM)
                break;

            if (make_item_fixed_artefact( mitm[ islot ], false, tmp ))
            {
                mitm[ islot ].quantity = 1;
                item_colour( mitm[ islot ] );
                set_ident_flags( mitm[ islot ], ISFLAG_IDENT_MASK );

                move_item_to_grid( &islot, you.x_pos, you.y_pos );

                msg::streams(MSGCH_DIAGNOSTICS) <<
                    "Made " << mitm[islot].name(DESC_NOCAP_A) << std::endl;
            }
        }

        // Create Horn of Geryon
        {
            int islot = get_item_slot();
            if (islot == NON_ITEM)
                break;

            mitm[islot].base_type = OBJ_MISCELLANY;
            mitm[islot].sub_type  = MISC_HORN_OF_GERYON;
            mitm[islot].plus      = 0;
            mitm[islot].plus2     = 0;
            mitm[islot].special   = 0;
            mitm[islot].flags     = 0;
            mitm[islot].quantity  = 1;

            set_ident_flags( mitm[ islot ], ISFLAG_IDENT_MASK );
            move_item_to_grid( &islot, you.x_pos, you.y_pos );

            msg::streams(MSGCH_DIAGNOSTICS) << "Made "
                                            << mitm[islot].name(DESC_NOCAP_A)
                                            << std::endl;
        }

        break;

    case 'C':
    {
        // this command isn't very exciting... feel free to replace
        i = prompt_invent_item( "(Un)curse which item?", MT_INVLIST, -1 );
        if (i == PROMPT_ABORT)
        {
            canned_msg( MSG_OK );
            break;
        }

        item_def& item(you.inv[i]);

        if (item_cursed(item))
            do_uncurse_item(item);
        else
            do_curse_item(item);

        break;
    }

    case 'B':
        if (you.level_type != LEVEL_ABYSS)
            banished( DNGN_ENTER_ABYSS, "wizard command" );
        else
            down_stairs(you.your_level, DNGN_EXIT_ABYSS);
        break;

    case CONTROL('A'):
        if (you.level_type == LEVEL_ABYSS)
            abyss_teleport(true);
        else
            mpr("You can only abyss_teleport() inside the Abyss.");

        break;

    case 'g':
        debug_add_skills();
        break;

    case 'G':
        debug_dismiss_all_monsters();
        break;

    case 'c':
        // draw a card
        debug_card();
        break;

    case 'h':
        you.rotting = 0;
        you.duration[DUR_POISONING] = 0;
        you.disease = 0;
        set_hp( abs(you.hp_max), false );
        set_hunger( 10999, true );
        break;

    case 'H':
        you.rotting = 0;
        you.duration[DUR_POISONING] = 0;
        if (you.duration[DUR_BEHELD])
        {
            you.duration[DUR_BEHELD] = 0;
            you.beheld_by.clear();
        }
        you.duration[DUR_CONF] = 0;
        you.disease = 0;
        inc_hp( 10, true );
        set_hp( you.hp_max, false );
        set_hunger( 10999, true );
        you.redraw_hit_points = 1;
        break;

    case 'b':
        // wizards can always blink, with no restrictions or
        // magical contamination.
        blink(1000, true, true);
        break;

    case '~':
        wizard_interlevel_travel();
        break;

    case '"':
        debug_list_monsters();
        break;

    case 'd':
    case 'D':
        level_travel(true);
        break;

    case 'u':
    case 'U':
        level_travel(false);
        break;

    case '%':
    case 'o':
        create_spec_object();
        break;

    case 't':
        tweak_object();
        break;

    case 'T':
        debug_make_trap();
        break;

    case '\\':
        debug_make_shop();
        break;

    case CONTROL('F'):
        debug_fight_statistics(false, true);
        break;
    
    case 'f':
        debug_fight_statistics(false);
        break;

    case 'F':
        debug_fight_statistics(true);
        break;

    case 'm':
        create_spec_monster();
        break;

    case 'M':
        create_spec_monster_name();
        break;

    case 'r':
        debug_change_species();
        break;

    case '>':
        wizard_place_stairs(true);
        break;

    case '<':
        wizard_place_stairs(false);
        break;

    case 'p':
        grd[you.x_pos][you.y_pos] = DNGN_ENTER_PANDEMONIUM;
        break;

    case 'P':
    {
        mpr( "Destination for portal (defaults to 'bazaar')? ", MSGCH_PROMPT );
        if (cancelable_get_line( specs, sizeof( specs ) ))
        {
            canned_msg( MSG_OK );
            break;
        }

        std::string dst = specs;
        dst = trim_string(dst);
        dst = replace_all(dst, " ", "_");

        if (dst == "")
            dst = "bazaar";

        if (find_map_by_name(dst) == -1 &&
            random_map_for_tag(dst, false) == -1)
        {
            mprf("No map named '%s' or tagged '%s'.",
                 dst.c_str(), dst.c_str());
            break;
        }

        grd[you.x_pos][you.y_pos] = DNGN_ENTER_PORTAL_VAULT;
        map_wiz_props_marker
            *marker = new map_wiz_props_marker(you.pos());
        marker->set_property("dst", dst);
        marker->set_property("desc", "wizard portal, dest = " + dst);
        env.markers.add(marker);
        break;
    }

    case 'l':
        grd[you.x_pos][you.y_pos] = DNGN_ENTER_LABYRINTH;
        break;

    case 'L':
        debug_place_map();
        break;

    case 'i':
        mpr( "You feel a rush of knowledge." );
        for (i = 0; i < ENDOFPACK; i++)
        {
            if (is_valid_item( you.inv[i] ))
            {
                set_ident_type( you.inv[i].base_type, you.inv[i].sub_type, 
                                ID_KNOWN_TYPE );

                set_ident_flags( you.inv[i], ISFLAG_IDENT_MASK );
            }
        }
        you.wield_change = true;
        you.quiver_change = true;
        break;

    case 'I':
        mpr( "You feel a rush of antiknowledge." );
        for (i = 0; i < ENDOFPACK; i++)
        {
            if (is_valid_item( you.inv[i] ))
            {
                set_ident_type( you.inv[i].base_type, you.inv[i].sub_type, 
                                ID_UNKNOWN_TYPE );

                unset_ident_flags( you.inv[i], ISFLAG_IDENT_MASK );
            }
        }
        you.wield_change = true;
        you.quiver_change = true;

        // Forget things that nearby monsters are carrying, as well
        // (for use with the "give monster an item" wizard targetting
        // command).
        for (i = 0; i < MAX_MONSTERS; i++)
        {
            monsters* mon = &menv[i];

            if (!invalid_monster(mon) && mons_near(mon))
            {
                for (j = 0; j < NUM_MONSTER_SLOTS; j++)
                {
                    if (mon->inv[j] == NON_ITEM)
                        continue;

                    item_def &item = mitm[mon->inv[j]];

                    if (!is_valid_item(item))
                        continue;

                    set_ident_type( item.base_type, item.sub_type, 
                                ID_UNKNOWN_TYPE );

                    unset_ident_flags( item, ISFLAG_IDENT_MASK );
                }
            }
        }
        break;

    case CONTROL('I'):
        debug_item_statistics();
        break;

    case 'X':
        xom_acts(abs(you.piety - 100));
        break;

    case 'z':
        cast_spec_spell();
        break;              /* cast spell by number */

    case 'Z':
        cast_spec_spell_name();
        break;              /* cast spell by name */

    case '(':
        mpr( "Create which feature (by number)? ", MSGCH_PROMPT );
        get_input_line( specs, sizeof( specs ) );

        if (specs[0] != '\0')
        {
            if (const int feat = atoi(specs))
                grd[you.x_pos][you.y_pos] =
                    static_cast<dungeon_feature_type>( feat );
        }
        break;

    case ')':
        mpr("Create which feature (by name)? ", MSGCH_PROMPT);
        get_input_line(specs, sizeof specs);
        if (*specs)
        {
            // Accept both "shallow_water" and "Shallow water"
            std::string name = lowercase_string(specs);
            name = replace_all(name, " ", "_");

            dungeon_feature_type feat = dungeon_feature_by_name(name);
            if (feat == DNGN_UNSEEN)
            {
                std::vector<std::string> matches =
                    dungeon_feature_matches(name);

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
                 you.x_pos, you.y_pos, name.c_str(), feat);
            grd(you.pos()) = feat;
        }
        break;

    case ']':
        if (!debug_add_mutation())
            mpr( "Failure to give mutation." );
        break;

    case '[':
        demonspawn();
        break;

    case ':':
        j = 0;
        for (i = 0; i < NUM_BRANCHES; i++)
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
        break;

    case '{':
        if (testbits(env.level_flags, LFLAG_NOT_MAPPABLE)
            || testbits(get_branch_flags(), BFLAG_NOT_MAPPABLE))
        {
            if (!yesno("Force level to be mappable? ", true, 'n'))
            {
                canned_msg( MSG_OK );
                return;
            }

            unset_level_flags(LFLAG_NOT_MAPPABLE | LFLAG_NO_MAGIC_MAP);
            unset_branch_flags(BFLAG_NOT_MAPPABLE | BFLAG_NO_MAGIC_MAP);
        }

        magic_mapping(1000, 100, true, true);
        break;

    case '@':
        debug_set_stats();
        break;

    case '^':
        {
            if (you.religion == GOD_NO_GOD)
            {
                mpr("You are not religious!");
                break;
            }
            else if (you.religion == GOD_XOM) // increase amusement instead
            {
                xom_is_stimulated(50, XM_NORMAL, true);
                break;
            }

            const int old_piety   = you.piety;
            const int old_penance = you.penance[you.religion];
            if (old_piety >= MAX_PIETY && !old_penance)
            {
                mprf("Your piety (%d) is already %s maximum.",
                     old_piety, old_piety == MAX_PIETY ? "at" : "above");
            }

            // even at maximum, you can still gain gifts
            // try at least once f. maximum, or repeat until something happens
            // Rarely, this might result in several gifts during the same round!
            do {
               gain_piety(50); 
            } while (old_piety < MAX_PIETY && old_piety == you.piety
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
        break;

    case '=':
        mprf( "Cost level: %d  Skill points: %d  Next cost level: %d", 
              you.skill_cost_level, you.total_skill_points,
              skill_cost_needed( you.skill_cost_level + 1 ) );
        break;

    case '_':
        debug_get_religion();
        break;

    case '\'':
        for (i = 0; i < MAX_ITEMS; i++)
        {
            if (mitm[i].link == NON_ITEM)
                continue;
    
            mprf("item:%3d link:%3d cl:%3d ty:%3d pl:%3d pl2:%3d "
                 "sp:%3ld q:%3d",
                 i, mitm[i].link, 
                 mitm[i].base_type, mitm[i].sub_type,
                 mitm[i].plus, mitm[i].plus2, mitm[i].special, 
                 mitm[i].quantity );
        }

        mpr("igrid:");

        for (i = 0; i < GXM; i++)
        {
            for (j = 0; j < GYM; j++)
            {
                if (igrd[i][j] != NON_ITEM)
                {
                    mprf("%3d at (%2d,%2d), cl:%3d ty:%3d pl:%3d pl2:%3d "
                         "sp:%3ld q:%3d", 
                         igrd[i][j], i, j,
                         mitm[i].base_type, mitm[i].sub_type,
                         mitm[i].plus, mitm[i].plus2, mitm[i].special, 
                         mitm[i].quantity );
                }
            }
        }
        break;

    default:
        formatted_mpr(formatted_string::parse_string("Not a <magenta>Wizard</magenta> Command."));
        break;
    }
    you.turn_is_over = false;
}
#endif

// Set up the running variables for the current run.
static void start_running( int dir, int mode )
{
    if (Options.tutorial_events[TUT_SHIFT_RUN] && mode == RMODE_START)
        Options.tutorial_events[TUT_SHIFT_RUN] = 0;
    if (i_feel_safe(true))
        you.running.initialise(dir, mode);
}

static bool recharge_rod( item_def &rod, bool wielded )
{
    if (!item_is_rod(rod) || rod.plus >= rod.plus2 || !enough_mp(1, true))
        return (false);

    const int charge = rod.plus / ROD_CHARGE_MULT;

    int rate = ((charge + 1) * ROD_CHARGE_MULT) / 10;

    rate *= (10 + skill_bump( SK_EVOCATIONS ));
    rate = div_rand_round( rate, 100 );

    if (rate < 5)
        rate = 5;
    else if (rate > ROD_CHARGE_MULT / 2)
        rate = ROD_CHARGE_MULT / 2;

    // If not wielded, the rod charges far more slowly.
    if (!wielded)
        rate /= 5;
    // Shields hamper recharging for wielded rods.
    else if (player_shield())
        rate /= 2;

    if (rod.plus / ROD_CHARGE_MULT != (rod.plus + rate) / ROD_CHARGE_MULT)
    {
        dec_mp(1);
        if (wielded)
            you.wield_change = true;
    }

    rod.plus += rate;
    if (rod.plus > rod.plus2)
        rod.plus = rod.plus2;

    if (wielded && rod.plus == rod.plus2)
    {
        mpr("Your rod has recharged.");
        if (is_resting())
            stop_running();
    }

    return (true);
}

static void recharge_rods()
{
    const int wielded = you.equip[EQ_WEAPON];
    if (wielded != -1)
    {
        if (recharge_rod( you.inv[wielded], true ))
            return ;
    }

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (i != wielded && is_valid_item(you.inv[i])
                && one_chance_in(3)
                && recharge_rod( you.inv[i], false ))
            return;
    }
}

static bool cmd_is_repeatable(command_type cmd, bool is_again = false)
{
    switch(cmd)
    {
    // Informational commands
    case CMD_LOOK_AROUND:
    case CMD_INSPECT_FLOOR:
    case CMD_EXAMINE_OBJECT:
    case CMD_LIST_WEAPONS:
    case CMD_LIST_ARMOUR:
    case CMD_LIST_JEWELLERY:
    case CMD_LIST_EQUIPMENT:
    case CMD_CHARACTER_DUMP:
    case CMD_DISPLAY_COMMANDS:
    case CMD_DISPLAY_INVENTORY:
    case CMD_DISPLAY_KNOWN_OBJECTS:
    case CMD_DISPLAY_MUTATIONS:
    case CMD_DISPLAY_SKILLS:
    case CMD_DISPLAY_OVERMAP:
    case CMD_DISPLAY_RELIGION:
    case CMD_DISPLAY_CHARACTER_STATUS:
    case CMD_DISPLAY_SPELLS:
    case CMD_EXPERIENCE_CHECK:
    case CMD_GET_VERSION:
    case CMD_RESISTS_SCREEN:
    case CMD_READ_MESSAGES:
    case CMD_SEARCH_STASHES:
        mpr("You can't repeat informational commands.");
        return false;

    // Multi-turn commands
    case CMD_PICKUP:
    case CMD_DROP:
    case CMD_BUTCHER:
    case CMD_GO_UPSTAIRS:
    case CMD_GO_DOWNSTAIRS:
    case CMD_WIELD_WEAPON:
    case CMD_WEAPON_SWAP:
    case CMD_WEAR_JEWELLERY:
    case CMD_REMOVE_JEWELLERY:
    case CMD_MEMORISE_SPELL:
    case CMD_EXPLORE:
    case CMD_INTERLEVEL_TRAVEL:
        mpr("You can't repeat multi-turn commands.");
        return false;

    // Miscellaneous non-repeatable commands.
    case CMD_TOGGLE_AUTOPICKUP:
    case CMD_ADJUST_INVENTORY:
    case CMD_REPLAY_MESSAGES:
    case CMD_REDRAW_SCREEN:
    case CMD_MACRO_ADD:
    case CMD_SAVE_GAME:
    case CMD_SAVE_GAME_NOW:
    case CMD_SUSPEND_GAME:
    case CMD_QUIT:
    case CMD_DESTROY_ITEM:
    case CMD_MARK_STASH:
    case CMD_FORGET_STASH:
    case CMD_FIX_WAYPOINT:
    case CMD_CLEAR_MAP:
    case CMD_INSCRIBE_ITEM:
    case CMD_TOGGLE_AUTOPRAYER:
    case CMD_MAKE_NOTE:
    case CMD_CYCLE_QUIVER_FORWARD:
        mpr("You can't repeat that command.");
        return false;

    case CMD_DISPLAY_MAP:
        mpr("You can't repeat map commands.");
        return false;

    case CMD_MOUSE_MOVE:
    case CMD_MOUSE_CLICK:
        mpr("You can't repeat mouse clicks or movements.");
        return false;

    case CMD_REPEAT_CMD:
        mpr("You can't repeat the repeat command!");
        return false;

    case CMD_RUN_LEFT:
    case CMD_RUN_DOWN:
    case CMD_RUN_UP:
    case CMD_RUN_RIGHT:
    case CMD_RUN_UP_LEFT:
    case CMD_RUN_DOWN_LEFT:
    case CMD_RUN_UP_RIGHT:
    case CMD_RUN_DOWN_RIGHT:
        mpr("Why would you want to repeat a run command?");
        return false;

    case CMD_PREV_CMD_AGAIN:
        ASSERT(!is_again);
        if (crawl_state.prev_cmd == CMD_NO_CMD)
        {
            mpr("No previous command to repeat.");
            return false;
        }

        return cmd_is_repeatable(crawl_state.prev_cmd, true);

    case CMD_MOVE_NOWHERE:
    case CMD_REST:
    case CMD_SEARCH:
        return i_feel_safe(true);

    case CMD_MOVE_LEFT:
    case CMD_MOVE_DOWN:
    case CMD_MOVE_UP:
    case CMD_MOVE_RIGHT:
    case CMD_MOVE_UP_LEFT:
    case CMD_MOVE_DOWN_LEFT:
    case CMD_MOVE_UP_RIGHT:
    case CMD_MOVE_DOWN_RIGHT:
        if (!i_feel_safe())
            return yesno("Really repeat movement command while monsters "
                         "are nearby?", false, 'n');

        return true;

    case CMD_NO_CMD:
        mpr("Unknown command, not repeating.");
        return false;

    default:
        return true;
    }

    return false;
}

/* used to determine whether to apply the berserk penalty at end
   of round */
bool apply_berserk_penalty = false;

/*
 * This function handles the player's input. It's called from main(),
 * from inside an endless loop.
 */
static void input()
{
    crawl_state.clear_god_acting();
    check_beholders();

    if (crawl_state.is_replaying_keys() && crawl_state.is_repeating_cmd()
        && kbhit())
    {
        // User pressed a key, so stop repeating commands and discard
        // the keypress
        crawl_state.cancel_cmd_repeat();
        getchm();
        return;
    }

    you.turn_is_over = false;
    prep_input();

    fire_monster_alerts();

    Options.tut_just_triggered = false;

    if ( i_feel_safe() )
    {
        if (Options.tutorial_left)
        {
            if ( 2*you.hp < you.hp_max
                 || 2*you.magic_points < you.max_magic_points )
            {
                tutorial_healing_reminder();
            }
            else if (!you.running
                     && Options.tutorial_events[TUT_SHIFT_RUN]
                     && you.num_turns >= 200
                     && you.hp == you.hp_max
                     && you.magic_points == you.max_magic_points)
            {
                learned_something_new(TUT_SHIFT_RUN);
            }
            else if (!you.running
                     && Options.tutorial_events[TUT_MAP_VIEW]
                     && you.num_turns >= 500
                     && you.hp == you.hp_max
                     && you.magic_points == you.max_magic_points)
            {
                learned_something_new(TUT_MAP_VIEW);
            }
        }
    }
    else
    {
        if (2*you.hp < you.hp_max)
            learned_something_new(TUT_RUN_AWAY);
            
        if (Options.tutorial_type == TUT_MAGIC_CHAR && you.magic_points < 1)
            learned_something_new(TUT_RETREAT_CASTER);
    }
 
    if ( you.cannot_act() )
    {
        crawl_state.cancel_cmd_repeat("Cannot move, cancelling command "
                                      "repetition.");

        world_reacts();
        return;
    }
    
    if (need_to_autopickup())
        autopickup();

    if (need_to_autoinscribe())
        autoinscribe();

    handle_delay();

    const coord_def cwhere = grid2view(you.pos());
    cgotoxy(cwhere.x, cwhere.y);

    if ( you_are_delayed() )
    {
        world_reacts();
        return;
    }

    do_autopray();              // this might set you.turn_is_over

    if ( you.turn_is_over )
    {
        world_reacts();
        return;
    }

    crawl_state.check_term_size();
    if (crawl_state.terminal_resized)
        handle_terminal_resize();

    repeat_again_rec.paused = (crawl_state.is_replaying_keys()
                               && !crawl_state.cmd_repeat_start);

    crawl_state.input_line_curr = 0;

    {
        // Enable the cursor to read input. The cursor stays on while
        // the command is being processed, so subsidiary prompts
        // shouldn't need to turn it on explicitly.
#ifdef USE_TILE
        cursor_control con(false);
#else
        cursor_control con(true);
#endif

        crawl_state.waiting_for_command = true;
        c_input_reset(true);
        
        const command_type cmd = get_next_cmd();

        crawl_state.waiting_for_command = false;

        if (cmd != CMD_PREV_CMD_AGAIN && cmd != CMD_REPEAT_CMD
            && !crawl_state.is_replaying_keys())
        {
            crawl_state.prev_cmd = cmd;
            crawl_state.input_line_strs.clear();
        }

        if (cmd != CMD_MOUSE_MOVE)
            c_input_reset(false);

        // [dshaligram] If get_next_cmd encountered a Lua macro
        // binding, your turn may be ended by the first invoke of the
        // macro.
        if (!you.turn_is_over && cmd != CMD_NEXT_CMD)
            process_command( cmd );

        repeat_again_rec.paused = true;

        if (cmd != CMD_MOUSE_MOVE)
            c_input_reset(false, true);

        // If the command was CMD_REPEAT_CMD, then the key for the
        // command to repeat has been placed into the macro buffer,
        // so return now to let input() be called again while
        // the keys to repeat are recorded.
        if (cmd == CMD_REPEAT_CMD)
            return;
    }

    if (need_to_autoinscribe())
        autoinscribe();

    if (you.turn_is_over)
    {
        if ( apply_berserk_penalty )
            do_berserk_no_combat_penalty();

        world_reacts();
    }
    else
        viewwindow(true, false);

    update_replay_state();

    if (you.num_turns != -1)
    {
        PlaceInfo& curr_PlaceInfo = you.get_place_info();
        PlaceInfo  delta;
 
        delta.turns_total++;
        delta.elapsed_total += you.time_taken;
 
        switch(you.running)
        {
        case RMODE_INTERLEVEL:
            delta.turns_interlevel++;
            delta.elapsed_interlevel += you.time_taken;
            break;
 
        case RMODE_EXPLORE_GREEDY:
        case RMODE_EXPLORE:
            delta.turns_explore++;
            delta.elapsed_explore += you.time_taken;
            break;
 
        case RMODE_TRAVEL:
            delta.turns_travel++;
            delta.elapsed_travel += you.time_taken;
            break;
 
        default:
            // prev_was_rest is needed so that the turn in which
            // a player is interrupted from resting is counted
            // as a resting turn, rather than "other".
            static bool prev_was_rest = false;
 
            if (!you.delay_queue.empty() &&
                you.delay_queue.front().type == DELAY_REST)
                prev_was_rest = true;
 
            if (prev_was_rest)
            {
                delta.turns_resting++;
                delta.elapsed_resting += you.time_taken;
 
            }
            else
            {
                delta.turns_other++;
                delta.elapsed_other += you.time_taken;
            }
 
            if (you.delay_queue.empty() ||
                you.delay_queue.front().type != DELAY_REST)
                prev_was_rest = false;
 
            break;
        }
 
        you.global_info += delta;
        you.global_info.assert_validity();
 
        curr_PlaceInfo += delta;
        curr_PlaceInfo.assert_validity();
    }

    crawl_state.clear_god_acting();
}

static bool toggle_flag( bool* flag, const char* flagname )
{
    *flag = !(*flag);
    mprf( "%s is now %s.", flagname, (*flag) ? "on" : "off" );
    return *flag;
}

static bool stairs_check_beheld()
{
    if (you.duration[DUR_BEHELD] && !you.duration[DUR_CONF])
    {
        mprf("You cannot move away from %s!",
             menv[you.beheld_by[0]].name(DESC_NOCAP_THE, true).c_str());
        return true;
    }

    return false;
}

static void go_downstairs();
static void go_upstairs()
{
    const dungeon_feature_type ygrd = grd(you.pos());

    if (stairs_check_beheld())
        return;
    
    if (you.attribute[ATTR_HELD])
    {
        mpr("You're held in a net!");
        return;
    }

    if (ygrd == DNGN_ENTER_SHOP)
    {
        if ( you.duration[DUR_BERSERKER] )
            canned_msg(MSG_TOO_BERSERK);
        else
            shop();
        return;
    }
    else if (grid_stair_direction(ygrd) != CMD_GO_UPSTAIRS)
    {
        if (ygrd == DNGN_STONE_ARCH)
            mpr("There is nothing on the other side of the stone arch.");
        else
            mpr( "You can't go up here!" );
        return;
    }

    tag_followers();  // only those beside us right now can follow
    start_delay( DELAY_ASCENDING_STAIRS, 
                 1 + (you.burden_state > BS_UNENCUMBERED) );
}

static void go_downstairs()
{
    bool shaft = (trap_type_at_xy(you.x_pos, you.y_pos) == TRAP_SHAFT
                  && grd[you.x_pos][you.y_pos] != DNGN_UNDISCOVERED_TRAP);


    if (stairs_check_beheld())
        return;

    if (shaft && you.flight_mode() == FL_LEVITATE)
    {
        mpr("You can't fall through a shaft while levitating.");
        return;
    }

    if (grid_stair_direction(grd(you.pos())) != CMD_GO_DOWNSTAIRS
        && !shaft)
    {
        if (grd(you.pos()) == DNGN_STONE_ARCH)
            mpr("There is nothing on the other side of the stone arch.");
        else
            mpr( "You can't go down here!" );
        return;
    }

    if (you.attribute[ATTR_HELD])
    {
        mpr("You're held in a net!");
        return;
    }

    if (shaft)
    {
        start_delay( DELAY_DESCENDING_STAIRS, 0, you.your_level );
    }
    else
    {
        tag_followers();  // only those beside us right now can follow
        start_delay( DELAY_DESCENDING_STAIRS,
                     1 + (you.burden_state > BS_UNENCUMBERED),
                     you.your_level );
    }
}

static void experience_check()
{
    mprf("You are a level %d %s %s.",
         you.experience_level,
         species_name(you.species,you.experience_level).c_str(),
         you.class_name);

    if (you.experience_level < 27)
    {
        int xp_needed = (exp_needed(you.experience_level+2)-you.experience)+1;
        mprf( "Level %d requires %ld experience (%d point%s to go!)",
              you.experience_level + 1, 
              exp_needed(you.experience_level + 2) + 1,
              xp_needed, 
              (xp_needed > 1) ? "s" : "");
    }
    else
    {
        mpr( "I'm sorry, level 27 is as high as you can go." );
        mpr( "With the way you've been playing, I'm surprised you got this far." );
    }

    if (you.real_time != -1)
    {
        const time_t curr = you.real_time + (time(NULL) - you.start_time);
        msg::stream << "Play time: " << make_time_string(curr)
                    << " (" << you.num_turns << " turns)"
                    << std::endl;
    }
#ifdef DEBUG_DIAGNOSTICS
    if (wearing_amulet(AMU_THE_GOURMAND))
        mprf(MSGCH_DIAGNOSTICS, "Gourmand charge: %d", 
             you.duration[DUR_GOURMAND]);

    mprf(MSGCH_DIAGNOSTICS, "Turns spent on this level: %d",
         env.turns_on_level);
#endif
}

/* note that in some actions, you don't want to clear afterwards.
   e.g. list_jewellery, etc. */

void process_command( command_type cmd )
{
    apply_berserk_penalty = true;

    switch (cmd)
    {
#ifdef USE_TILE
    case CMD_EDIT_PREFS:
        edit_prefs();
        break;

    case CMD_USE_ITEM:
        {
            int idx;
            InvAction act;
            gui_get_mouse_inv(idx, act);
            tile_use_item(idx, act);
        }
        break;

    case CMD_VIEW_ITEM:
        {
            int idx;
            InvAction act;
            gui_get_mouse_inv(idx, act);

            if (idx < 0)
                describe_item(mitm[-idx]);
            else
                describe_item(you.inv[idx]);
            redraw_screen();
        }
        break;

    case CMD_EDIT_PLAYER_TILE:
        TilePlayerEdit();
        break;
#endif // USE_TILE

    case CMD_OPEN_DOOR_UP_RIGHT:   open_door(-1, -1); break;
    case CMD_OPEN_DOOR_UP:         open_door( 0, -1); break;
    case CMD_OPEN_DOOR_UP_LEFT:    open_door( 1, -1); break;
    case CMD_OPEN_DOOR_RIGHT:      open_door( 1,  0); break;
    case CMD_OPEN_DOOR_DOWN_RIGHT: open_door( 1,  1); break;
    case CMD_OPEN_DOOR_DOWN:       open_door( 0,  1); break;
    case CMD_OPEN_DOOR_DOWN_LEFT:  open_door(-1,  1); break;
    case CMD_OPEN_DOOR_LEFT:       open_door(-1,  0); break;

    case CMD_MOVE_DOWN_LEFT:  move_player(-1,  1); break;
    case CMD_MOVE_DOWN:       move_player( 0,  1); break;
    case CMD_MOVE_UP_RIGHT:   move_player( 1, -1); break;
    case CMD_MOVE_UP:         move_player( 0, -1); break;
    case CMD_MOVE_UP_LEFT:    move_player(-1, -1); break;
    case CMD_MOVE_LEFT:       move_player(-1,  0); break;
    case CMD_MOVE_DOWN_RIGHT: move_player( 1,  1); break;
    case CMD_MOVE_RIGHT:      move_player( 1,  0); break;

    case CMD_REST:
        if (i_feel_safe())
        {
            if ( you.hp == you.hp_max &&
                 you.magic_points == you.max_magic_points )
                mpr("You start searching.");
            else
                mpr("You start resting.");
        }
        start_running( RDIR_REST, RMODE_REST_DURATION );
        break;

    case CMD_RUN_DOWN_LEFT:
        start_running( RDIR_DOWN_LEFT, RMODE_START );
        break;
    case CMD_RUN_DOWN:
        start_running( RDIR_DOWN, RMODE_START );
        break;
    case CMD_RUN_UP_RIGHT:
        start_running( RDIR_UP_RIGHT, RMODE_START );
        break;
    case CMD_RUN_UP:
        start_running( RDIR_UP, RMODE_START );
        break;
    case CMD_RUN_UP_LEFT:
        start_running( RDIR_UP_LEFT, RMODE_START );
        break;
    case CMD_RUN_LEFT:
        start_running( RDIR_LEFT, RMODE_START );
        break;
    case CMD_RUN_DOWN_RIGHT:
        start_running( RDIR_DOWN_RIGHT, RMODE_START );
        break;
    case CMD_RUN_RIGHT:
        start_running( RDIR_RIGHT, RMODE_START );
        break;

    case CMD_DISABLE_MORE:
        Options.show_more_prompt = false;
        break;

    case CMD_ENABLE_MORE:
        Options.show_more_prompt = true;
        break;
        
    case CMD_REPEAT_KEYS:
        ASSERT(crawl_state.is_repeating_cmd());

        if (crawl_state.prev_repetition_turn == you.num_turns
            && !(crawl_state.repeat_cmd == CMD_WIZARD
                 || (crawl_state.repeat_cmd == CMD_PREV_CMD_AGAIN
                     && crawl_state.prev_cmd == CMD_WIZARD)))
        {
            // This is a catch-all that shouldn't really happen.
            // If the command always takes zero turns, then it
            // should be prevented in cmd_is_repeatable().  If
            // a command sometimes takes zero turns (because it
            // can't be done, for instance), then
            // crawl_state.zero_turns_taken() should be called when
            // it does take zero turns, to cancel command repetition
            // before we reach here.
#ifdef WIZARD
            crawl_state.cant_cmd_repeat("Can't repeat a command which "
                                        "takes no turns (unless it's a "
                                        "wizard command), cancelling ");
#else
            crawl_state.cant_cmd_repeat("Can't repeat a command which "
                                        "takes no turns, cancelling "
                                        "repetitions.");
#endif
            crawl_state.cancel_cmd_repeat();
            return;
        }

        crawl_state.cmd_repeat_count++;
        if (crawl_state.cmd_repeat_count >= crawl_state.cmd_repeat_goal)
        {
            crawl_state.cancel_cmd_repeat();
            return;
        }

        ASSERT(crawl_state.repeat_cmd_keys.size() > 0);
        repeat_again_rec.paused = true;
        macro_buf_add(crawl_state.repeat_cmd_keys);
        macro_buf_add(KEY_REPEAT_KEYS);

        crawl_state.prev_repetition_turn = you.num_turns;

        break;

    case CMD_TOGGLE_AUTOPICKUP:
        toggle_flag( &Options.autopickup_on, "Autopickup");
        break;

    case CMD_TOGGLE_AUTOPRAYER:
        if (you.religion == GOD_NEMELEX_XOBEH)
        {
            mpr("Those worshipping Nemelex Xobeh don't need to autopray.");
            Options.autoprayer_on = false;
        }
        else
            toggle_flag( &Options.autoprayer_on, "Autoprayer" );
        break;
   
    case CMD_MAKE_NOTE:
        make_user_note();
        break;

    case CMD_READ_MESSAGES:
#ifdef DGL_SIMPLE_MESSAGING
        if (SysEnv.have_messages)
            read_messages();
#endif
        break;

    case CMD_CLEAR_MAP:
        if (you.level_type != LEVEL_LABYRINTH &&
            you.level_type != LEVEL_ABYSS)
        {
            mpr("Clearing level map.");
            clear_map();
            crawl_view.set_player_at(you.pos(), true);
        }
        break;

    case CMD_GO_UPSTAIRS: go_upstairs(); break;
    case CMD_GO_DOWNSTAIRS: go_downstairs(); break;
    case CMD_DISPLAY_OVERMAP: display_overmap(); break;
    case CMD_OPEN_DOOR: open_door(0, 0); break;
    case CMD_CLOSE_DOOR: close_door(0, 0); break;

    case CMD_DROP:
        drop();
        if (Options.stash_tracking >= STM_DROPPED)
            stashes.add_stash();
        break;
        
    case CMD_SEARCH_STASHES:
        if (Options.tut_stashes)
            Options.tut_stashes = 0;
        stashes.search_stashes();
        break;

    case CMD_MARK_STASH:
        if (Options.stash_tracking >= STM_EXPLICIT)
            stashes.add_stash(-1, -1, true);
        break;

    case CMD_FORGET_STASH:
        if (Options.stash_tracking >= STM_EXPLICIT)
            stashes.no_stash();
        break;

    case CMD_BUTCHER:
        butchery();
        break;

    case CMD_DISPLAY_INVENTORY:
        get_invent(OSEL_ANY);
        break;

    case CMD_EVOKE:
        if (!evoke_wielded())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case CMD_PICKUP:
        pickup();
        break;

    case CMD_INSPECT_FLOOR:
        // item_check(';');
        request_autopickup();
        break;

    case CMD_WIELD_WEAPON:
        wield_weapon(false);
        break;

    case CMD_FIRE:
        fire_thing();
        break;

    case CMD_WEAR_ARMOUR:
        wear_armour();
        break;

    case CMD_REMOVE_ARMOUR:
    {
        int index=0;

        if (armour_prompt("Take off which item?", &index, OPER_TAKEOFF))
            takeoff_armour(index);
    }
    break;

    case CMD_REMOVE_JEWELLERY:
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
        {
           canned_msg(MSG_PRESENT_FORM);
           break;
        }
        remove_ring();
        break;

    case CMD_WEAR_JEWELLERY:
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
        {
           canned_msg(MSG_PRESENT_FORM);
           break;
        }
        puton_ring(-1, false);
        break;

    case CMD_ADJUST_INVENTORY:
        adjust();
        break;

    case CMD_MEMORISE_SPELL:
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
        {
           canned_msg(MSG_PRESENT_FORM);
           break;
        }
        if (!learn_spell())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case CMD_ZAP_WAND:
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
        {
           canned_msg(MSG_PRESENT_FORM);
           break;
        }
        zap_wand();
        break;

    case CMD_EAT:
        eat_food();
        break;

    case CMD_USE_ABILITY:
        if (!activate_ability())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case CMD_DISPLAY_MUTATIONS:
        display_mutations();
        redraw_screen();
        break;

    case CMD_EXAMINE_OBJECT:
        examine_object();
        break;

    case CMD_PRAY:
        pray();
        break;

    case CMD_DISPLAY_RELIGION:
        describe_god( you.religion, true );
        redraw_screen();
        break;

    case CMD_MOVE_NOWHERE:
    case CMD_SEARCH:
        search_around();
        you.turn_is_over = true;
        break;

    case CMD_QUAFF:
        drink();
        break;

    case CMD_READ:
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
        {
           canned_msg(MSG_PRESENT_FORM);
           break;
        }
        read_scroll();
        break;

    case CMD_LOOK_AROUND:
    {
        mpr("Move the cursor around to observe a square "
            "(v - describe square, ? - help)",
            MSGCH_PROMPT);

        struct dist lmove;      // will be initialized by direction()
        direction(lmove, DIR_TARGET, TARG_ANY, true);
        if (lmove.isValid && lmove.isTarget && !lmove.isCancel)
            start_travel( lmove.tx, lmove.ty );
        break;
    }

    case CMD_CAST_SPELL:
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
        {
           canned_msg(MSG_PRESENT_FORM);
           break;
        }
        /* randart wpns */
        if (scan_randarts(RAP_PREVENT_SPELLCASTING))
        {
            mpr("Something interferes with your magic!");
            flush_input_buffer( FLUSH_ON_FAILURE );
            break;
        }
        
        if (Options.tutorial_left)
            Options.tut_spell_counter++;
        if (!cast_a_spell())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case CMD_DISPLAY_SPELLS:
        inspect_spells();
        break;

    case CMD_WEAPON_SWAP:
        wield_weapon(true);
        break;

        // [ds] Waypoints can be added from the level-map, and we need
        // Ctrl+F for nobler things. Who uses waypoints, anyway?
        // Update: Appears people do use waypoints. Reinstating, on
        // CONTROL('W'). This means Ctrl+W is no longer a wizmode
        // trigger, but there's always '&'. :-)
    case CMD_FIX_WAYPOINT:
        travel_cache.add_waypoint();
        break;
        
    case CMD_INTERLEVEL_TRAVEL:
        if (Options.tut_travel)
            Options.tut_travel = 0;
        if (!can_travel_interlevel())
        {
            mpr("Sorry, you can't auto-travel out of here.");
            break;
        }
        start_translevel_travel();
        if (you.running)
            mesclr();
        break;

    case CMD_ANNOTATE_LEVEL:
        annotate_level();
        break;

    case CMD_EXPLORE:
        // Start exploring
        start_explore(Options.explore_greedy);
        break;

    case CMD_DISPLAY_MAP:
        if (Options.tutorial_events[TUT_MAP_VIEW])
            Options.tutorial_events[TUT_MAP_VIEW] = 0;
#if (!DEBUG_DIAGNOSTICS)
        if (!player_in_mappable_area())
        {
            mpr("It would help if you knew where you were, first.");
            break;
        }
#endif
        {
            coord_def pos;
#ifdef USE_TILE
            std::string str = "Move the cursor to view the level map, or "
                              "type <w>?</w> for a list of commands.";
            print_formatted_paragraph(str, get_number_of_cols());
#endif

            show_map(pos, true);
            redraw_screen();
            
#ifdef USE_TILE
            mpr("Returning to the game...");
#endif
            if (pos.x > 0)
                start_travel(pos.x, pos.y);
        }
        break;

    case CMD_DISPLAY_KNOWN_OBJECTS:
        check_item_knowledge();
        break;

    case CMD_REPLAY_MESSAGES:
        replay_messages();
        redraw_screen();
        break;

    case CMD_REDRAW_SCREEN:
        redraw_screen();
        break;

    case CMD_SAVE_GAME_NOW:
        mpr("Saving game... please wait.");
        save_game(true);
        break;

#ifdef USE_UNIX_SIGNALS
    case CMD_SUSPEND_GAME:
        // CTRL-Z suspend behaviour is implemented here,
        // because we want to have CTRL-Y available...
        // and unfortunately they tend to be stuck together. 
        clrscr();
#ifndef USE_TILE
        unixcurses_shutdown();
        kill(0, SIGTSTP);
        unixcurses_startup();
#endif
        redraw_screen();
        break;
#endif

    case CMD_DISPLAY_COMMANDS:
        if (Options.tutorial_left)
        {
            list_tutorial_help();
            redraw_screen();
        }
        else
            list_commands(false, 0, true);
        break;

    case CMD_EXPERIENCE_CHECK:
        experience_check();
        break;

    case CMD_SHOUT:
        yell();
        break;

    case CMD_MOUSE_MOVE:
    {
        const coord_def dest = view2grid(crawl_view.mousep);
        if (in_bounds(dest))
            terse_describe_square(dest);
        break;
    }

    case CMD_MOUSE_CLICK:
    {
        // XXX: We should probably use specific commands such as
        // CMD_MOUSE_TRAVEL and get rid of CMD_MOUSE_CLICK and
        // CMD_MOUSE_MOVE.
        c_mouse_event cme = get_mouse_event();
        if (cme && crawl_view.in_view_viewport(cme.pos))
        {
            const coord_def dest = view2grid(cme.pos);
            if (cme.left_clicked())
            {
                if (in_bounds(dest))
                    start_travel(dest.x, dest.y);
            }
            else if (cme.right_clicked())
            {
                if (see_grid(dest.x, dest.y))
                    full_describe_square(dest);
                else
                    mpr("You can't see that place.");
            }
        }
        break;
    }

    case CMD_DISPLAY_CHARACTER_STATUS:
        display_char_status();
        break;
        
    case CMD_RESISTS_SCREEN:
        print_overview_screen();
        break;

    case CMD_DISPLAY_SKILLS:
        show_skills();
        redraw_screen();
        break;

    case CMD_CHARACTER_DUMP:
        char name_your[kNameLen+1];

        strncpy(name_your, you.your_name, kNameLen);
        name_your[kNameLen] = 0;
        if (dump_char( name_your, false ))
            mpr("Char dumped successfully.");
        else
            mpr("Char dump unsuccessful! Sorry about that.");
        break;

    case CMD_MACRO_ADD:
        macro_add_query();
        break;

    case CMD_CYCLE_QUIVER_FORWARD:
    {
        const int cur = you.quiver[get_quiver_type()];
        if (cur != ENDOFPACK)
        {
            const int next = get_next_fire_item(cur, +1);
            you.quiver[get_quiver_type()] = next;
            you.quiver_change = true;
        }
        break;
    }

    case CMD_LIST_WEAPONS:
        list_weapons();
        break;
        
    case CMD_LIST_ARMOUR:
        list_armour();
        break;
        
    case CMD_LIST_JEWELLERY:
        list_jewellery();
        break;

    case CMD_LIST_EQUIPMENT:
        get_invent( OSEL_EQUIP );
        break;

    case CMD_INSCRIBE_ITEM:
        inscribe_item();
        break;
        
#ifdef WIZARD
    case CMD_WIZARD:
        handle_wizard_command();
        break;
#endif

    case CMD_SAVE_GAME:
        if (yesno("Save game and exit? ", true, 'n'))
            save_game(true);
        break;

    case CMD_QUIT:
        quit_game();
        break;

    case CMD_GET_VERSION:
        version();
        break;

    case CMD_REPEAT_CMD:
        setup_cmd_repeat();
        break;

    case CMD_PREV_CMD_AGAIN:
        do_prev_cmd_again();
        break;

    case CMD_NO_CMD:
    default:
        if (Options.tutorial_left)
        {
           std::string msg = "Unknown command. (For a list of commands type <w>?<lightgray>.)";
           print_formatted_paragraph(msg, get_number_of_cols());
        }
        else // well, not examine, but...
           mpr("Unknown command.", MSGCH_EXAMINE_FILTER);
        break;

    }
}

static void prep_input()
{
    you.time_taken = player_speed();
    you.shield_blocks = 0;              // no blocks this round
    update_screen();

    textcolor(LIGHTGREY);

    set_redraw_status( REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK );
    print_stats();
}

// Decrement a single duration. Print the message if the duration runs out.
static bool decrement_a_duration(duration_type dur, const char* endmsg = NULL,
                                 int midpoint = -1, int midloss = 0,
                                 const char* midmsg = NULL,
                                 msg_channel_type chan = MSGCH_DURATION )
{
    bool rc = false;

    if (you.duration[dur] > 1)
    {
        you.duration[dur]--;
        if (you.duration[dur] == midpoint)
        {
            if ( midmsg )
                mpr(midmsg, chan);
            you.duration[dur] -= midloss;
        }
    }
    else if (you.duration[dur] == 1)
    {
        if ( endmsg )
            mpr(endmsg, chan);

        rc = true;
        you.duration[dur] = 0;
    }

    return rc;
}

static void decrement_durations()
{
    if (wearing_amulet(AMU_THE_GOURMAND))
    {
        if (you.duration[DUR_GOURMAND] < GOURMAND_MAX && coinflip())
            you.duration[DUR_GOURMAND]++;
    }
    else
        you.duration[DUR_GOURMAND] = 0;

    // must come before might/haste/berserk
    if (decrement_a_duration(DUR_BUILDING_RAGE))
        go_berserk(false);

    if (decrement_a_duration(DUR_SLEEP))
        you.awake();

    // paradox: it both lasts longer & does more damage overall if you're
    //          moving slower.
    // rationalisation: I guess it gets rubbed off/falls off/etc if you
    //          move around more.
    if (you.duration[DUR_LIQUID_FLAMES] > 0)
        you.duration[DUR_LIQUID_FLAMES]--;

    if (you.duration[DUR_LIQUID_FLAMES] != 0)
    {
        const int res_fire = player_res_fire();

        mpr( "You are covered in liquid flames!", MSGCH_WARN );
        expose_player_to_element(BEAM_NAPALM, 12);

        if (res_fire > 0)
        {
            ouch( (((random2avg(9, 2) + 1) * you.time_taken) /
                    (1 + (res_fire * res_fire))) / 10, 0, KILLED_BY_BURNING );
        }

        if (res_fire <= 0)
        {
            ouch(((random2avg(9, 2) + 1) * you.time_taken) / 10, 0,
                 KILLED_BY_BURNING);
        }

        if (res_fire < 0)
        {
            ouch(((random2avg(9, 2) + 1) * you.time_taken) / 10, 0,
                 KILLED_BY_BURNING);
        }

        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        {
            mpr("Your icy shield dissipates!", MSGCH_DURATION);
            you.duration[DUR_CONDENSATION_SHIELD] = 0;
            you.redraw_armour_class = true;
        }
    }
   
    if (decrement_a_duration(DUR_ICY_ARMOUR, "Your icy armour evaporates."))
        you.redraw_armour_class = true;

    if (decrement_a_duration(DUR_SILENCE, "Your hearing returns."))
        you.attribute[ATTR_WAS_SILENCED] = 0;

    decrement_a_duration(DUR_REPEL_MISSILES,
                         "You feel less protected from missiles.",
                         6, coinflip(),
                         "Your repel missiles spell is about to expire...");

    decrement_a_duration(DUR_DEFLECT_MISSILES,
                         "You feel less protected from missiles.",
                         6, coinflip(),
                         "Your deflect missiles spell is about to expire...");

    decrement_a_duration(DUR_REGENERATION,
                         "Your skin stops crawling.",
                         6, coinflip(),
                         "Your skin is crawling a little less now.");

    if (you.duration[DUR_PRAYER] > 1)
        you.duration[DUR_PRAYER]--;
    else if (you.duration[DUR_PRAYER] == 1)
    {
        mpr( "Your prayer is over.", MSGCH_PRAY, you.religion );
        you.duration[DUR_PRAYER] = 0;
    }

    if (you.duration[DUR_DIVINE_SHIELD])
    {
        if (you.duration[DUR_DIVINE_SHIELD] > 1)
        {
            if (--you.duration[DUR_DIVINE_SHIELD] == 1)
                mpr("Your divine shield starts to fade.");
        }
        
        if (you.duration[DUR_DIVINE_SHIELD] == 1 && !one_chance_in(3))
        {
            you.redraw_armour_class = true;
            if (--you.attribute[ATTR_DIVINE_SHIELD] == 0)
            {
                you.duration[DUR_DIVINE_SHIELD] = 0;
                mpr("Your divine shield fades completely.");
            }
        }
    }
    
    //jmf: more flexible weapon branding code
    if (you.duration[DUR_WEAPON_BRAND] > 1)
        you.duration[DUR_WEAPON_BRAND]--;
    else if (you.duration[DUR_WEAPON_BRAND] == 1)
    {
        const int wpn = you.equip[EQ_WEAPON];
        const int temp_effect = get_weapon_brand( you.inv[wpn] );

        you.duration[DUR_WEAPON_BRAND] = 0;

        set_item_ego_type( you.inv[wpn], OBJ_WEAPONS, SPWPN_NORMAL );

        std::string msg = you.inv[wpn].name(DESC_CAP_YOUR);

        switch (temp_effect)
        {
        case SPWPN_VORPAL:
            if (get_vorpal_type(you.inv[you.equip[EQ_WEAPON]])==DVORP_SLICING)
                msg += " seems blunter.";
            else
                msg += " feels lighter.";
            break;
        case SPWPN_FLAMING:
            msg += " goes out.";
            break;
        case SPWPN_FREEZING:
            msg += " stops glowing.";
            break;
        case SPWPN_VENOM:
            msg += " stops dripping with poison.";
            break;
        case SPWPN_DRAINING:
            msg += " stops crackling.";
            break;
        case SPWPN_DISTORTION:
            msg += " seems straighter.";
            break;
        case SPWPN_PAIN:
            msg += " seems less painful.";
            break;
        default:
            msg += " seems inexplicably less special.";
            break;
        }

        mpr(msg.c_str(), MSGCH_DURATION);
        you.wield_change = true;
    }

    // Vampire bat transformations are permanent (until ended.)
    if (you.species != SP_VAMPIRE
        || you.attribute[ATTR_TRANSFORMATION] != TRAN_BAT
        || you.duration[DUR_TRANSFORMATION] <= 2)
    {
        if ( decrement_a_duration(DUR_TRANSFORMATION,
                                  NULL, 10, random2(3),
                                  "Your transformation is almost over.") )
        {
            untransform();
            you.duration[DUR_BREATH_WEAPON] = 0;
        }
    }

    // must come after transformation duration
    decrement_a_duration(DUR_BREATH_WEAPON, "You have got your breath back.",
                         -1, 0, NULL, MSGCH_RECOVERY);

    decrement_a_duration(DUR_REPEL_UNDEAD,
                         "Your holy aura fades away.",
                         4, random2(3),
                         "Your holy aura is starting to fade.");
    decrement_a_duration(DUR_SWIFTNESS,
                         "You feel sluggish.",
                         6, coinflip(),
                         "You start to feel a little slower.");
    decrement_a_duration(DUR_INSULATION,
                         "You feel conductive.",
                         6, coinflip(),
                         "You start to feel a little less insulated.");

    if ( decrement_a_duration(DUR_STONEMAIL,
                              "Your scaly stone armour disappears.",
                              6, coinflip(),
                              "Your scaly stone armour is starting "
                              "to flake away.") )
    {
        you.redraw_armour_class = true;
        burden_change();
    }

    if ( decrement_a_duration(DUR_FORESCRY,
                              "You feel firmly rooted in the present.") )
        you.redraw_evasion = true;

    if ( decrement_a_duration(DUR_SEE_INVISIBLE) && !player_see_invis() )
        mpr("Your eyesight blurs momentarily.", MSGCH_DURATION);

    decrement_a_duration(DUR_SEE_INVISIBLE); // jmf: cute message
                                             // handled elsewhere

    if ( decrement_a_duration(DUR_CONDENSATION_SHIELD,
                              "Your icy shield evaporates.") )
        you.redraw_armour_class = true;

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0 && player_res_cold() < 0)
    {
        mpr( "You feel very cold." );
        ouch( 2 + random2avg(13, 2), 0, KILLED_BY_FREEZING );
    }

    if ( decrement_a_duration(DUR_MAGIC_SHIELD,
                              "Your magical shield disappears.") )
        you.redraw_armour_class = true;

    if ( decrement_a_duration(DUR_STONESKIN, "Your skin feels tender.") )
        you.redraw_armour_class = true;

    if ( decrement_a_duration(DUR_TELEPORT) )
    {
        // only to a new area of the abyss sometimes (for abyss teleports)
        you_teleport_now( true, one_chance_in(5) );
        untag_followers();
    }

    decrement_a_duration(DUR_CONTROL_TELEPORT,
                         "You feel uncertain.",
                         6, coinflip(),
                         "You start to feel a little uncertain.");

    decrement_a_duration(DUR_DEATH_CHANNEL,
                         "Your unholy channel expires.",
                         6, coinflip(),
                         "Your unholy channel is weakening.");

    decrement_a_duration(DUR_SAGE, "You feel less studious.");
    decrement_a_duration(DUR_STEALTH, "You feel less stealthy.");
    decrement_a_duration(DUR_RESIST_FIRE, "Your fire resistance expires.");
    decrement_a_duration(DUR_RESIST_COLD, "Your cold resistance expires.");
    decrement_a_duration(DUR_RESIST_POISON, "Your poison resistance expires.");
    decrement_a_duration(DUR_SLAYING, "You feel less lethal.");
    
    decrement_a_duration(DUR_INVIS, "You flicker back into view.",
                         6, coinflip(), "You flicker for a moment.");

    decrement_a_duration(DUR_BARGAIN, "You feel less charismatic.");
    decrement_a_duration(DUR_CONF, "You feel less confused.");
    
    if (decrement_a_duration(DUR_PARALYSIS, "You can move again."))
        you.redraw_evasion = true;
        
    decrement_a_duration(DUR_EXHAUSTED, "You feel less fatigued.");

    decrement_a_duration( DUR_CONFUSING_TOUCH,
                          ((std::string("Your ") + your_hand(true)) +
                          " stop glowing.").c_str() );

    decrement_a_duration( DUR_SURE_BLADE,
                          "The bond with your blade fades away." );

    if ( decrement_a_duration( DUR_BEHELD, "You break out of your daze.",
                               -1, 0, NULL, MSGCH_RECOVERY ))
    {
        you.beheld_by.clear();
    }

    dec_slow_player();
    dec_haste_player();

    if (decrement_a_duration(DUR_MIGHT, "You feel a little less mighty now."))
        modify_stat(STAT_STRENGTH, -5, true, "might running out");

    if (decrement_a_duration(DUR_BERSERKER, "You are no longer berserk."))
    {
        //jmf: guilty for berserking /after/ berserk
        did_god_conduct( DID_STIMULANTS, 6 + random2(6) );

        //
        // Sometimes berserk leaves us physically drained
        //

        // chance of passing out:  
        //     - mutation gives a large plus in order to try and
        //       avoid the mutation being a "death sentence" to
        //       certain characters.
        //     - knowing the spell gives an advantage just
        //       so that people who have invested 3 spell levels
        //       are better off than the casual potion drinker...
        //       this should make it a bit more interesting for
        //       Crusaders again.
        //     - similarly for the amulet
        if (you.berserk_penalty != NO_BERSERK_PENALTY)
        {
            const int chance =
                10 +
                you.mutation[MUT_BERSERK] * 25 +
                (wearing_amulet( AMU_RAGE ) ? 10 : 0) +
                (player_has_spell( SPELL_BERSERKER_RAGE ) ? 5 : 0);

            // Note the beauty of Trog!  They get an extra save that's at
            // the very least 20% and goes up to 100%.
            if ( you.religion == GOD_TROG && you.piety > random2(150) &&
                 !player_under_penance() )
                mpr("Trog's vigour flows through your veins.");
            else if ( one_chance_in(chance) )
            {
                mpr("You pass out from exhaustion.", MSGCH_WARN);
                you.duration[DUR_PARALYSIS] += roll_dice( 1, 4 );
            }
        }

        if ( you.duration[DUR_PARALYSIS] == 0 )
            mpr("You are exhausted.", MSGCH_WARN);

        // this resets from an actual penalty or from NO_BERSERK_PENALTY
        you.berserk_penalty = 0;

        int dur = 12 + roll_dice( 2, 12 );
        you.duration[DUR_EXHAUSTED] += dur;

        // Don't trigger too many tutorial messages
        const bool tut_slow = Options.tutorial_events[TUT_YOU_ENCHANTED];
        Options.tutorial_events[TUT_YOU_ENCHANTED] = false;
        
        {
            // Don't give duplicate 'You feel yourself slow down' messages.
            no_messages nm;
            slow_player( dur );
        }

        make_hungry(700, true);

        if (you.hunger < 50)
            you.hunger = 50;

        calc_hp();
        
        learned_something_new(TUT_POSTBERSERK);
        Options.tutorial_events[TUT_YOU_ENCHANTED] = tut_slow;
    }

    if (you.duration[DUR_BACKLIGHT] > 0 && !--you.duration[DUR_BACKLIGHT] && !you.backlit())
        mpr("You are no longer glowing.", MSGCH_DURATION);

    // Leak piety from the piety pool into actual piety.
    // Note that changes of religious status without corresponding actions
    // (killing monsters, offering items, ...) might be confusing for characters
    // of other religions.
    // For now, though, keep information about what happened hidden.
    if (you.duration[DUR_PIETY_POOL] && one_chance_in(5))
    {
        you.duration[DUR_PIETY_POOL]--; // decrease even if piety at maximum
        gain_piety(1);
        
#if DEBUG_DIAGNOSTICS || DEBUG_SACRIFICE || DEBUG_PIETY
        mpr("Piety increases by 1 due to piety pool.", MSGCH_DIAGNOSTICS);
            
        if (!you.duration[DUR_PIETY_POOL])
            mpr("Piety pool is now empty.", MSGCH_DIAGNOSTICS);
#endif
    }
    

    if (!you.permanent_levitation())
    {
        if ( decrement_a_duration(DUR_LEVITATION,
                                  "You float gracefully downwards.",
                                  10, random2(6),
                                  "You are starting to lose your buoyancy!") )
        {
            burden_change();
            // Landing kills controlled flight.
            you.duration[DUR_CONTROLLED_FLIGHT] = 0;
            // re-enter the terrain:
            move_player_to_grid( you.x_pos, you.y_pos, false, true, true );
        }
    }

    if (!you.permanent_flight())
        if ( decrement_a_duration(DUR_CONTROLLED_FLIGHT) && you.airborne() )
            mpr("You lose control over your flight.", MSGCH_DURATION);
            
    if (you.rotting > 0)
    {
        // XXX: Mummies have an ability (albeit an expensive one) that 
        // can fix rotted HPs now... it's probably impossible for them
        // to even start rotting right now, but that could be changed. -- bwr
        if (you.species == SP_MUMMY)
            you.rotting = 0;
        else if (random2(20) <= (you.rotting - 1))
        {
            mpr("You feel your flesh rotting away.", MSGCH_WARN);
            ouch(1, 0, KILLED_BY_ROTTING);
            rot_hp(1);
            you.rotting--;
        }
    }

    // ghoul rotting is special, but will deduct from you.rotting
    // if it happens to be positive - because this is placed after
    // the "normal" rotting check, rotting attacks can be somewhat
    // more painful on ghouls - reversing order would make rotting
    // attacks somewhat less painful, but that seems wrong-headed {dlb}:
    if (you.species == SP_GHOUL)
    {
        if (one_chance_in(400))
        {
            mpr("You feel your flesh rotting away.", MSGCH_WARN);
            ouch(1, 0, KILLED_BY_ROTTING);
            rot_hp(1);

            if (you.rotting > 0)
                you.rotting--;
        }
    }

    dec_disease_player();

    if (you.duration[DUR_POISONING] > 0)
    {
        if (random2(5) <= (you.duration[DUR_POISONING] - 1))
        {
            if (you.duration[DUR_POISONING] > 10 && random2(you.duration[DUR_POISONING]) >= 8)
            {
                ouch(random2(10) + 5, 0, KILLED_BY_POISON);
                mpr("You feel extremely sick.", MSGCH_DANGER);
            }
            else if (you.duration[DUR_POISONING] > 5 && coinflip())
            {
                ouch((coinflip()? 3 : 2), 0, KILLED_BY_POISON);
                mpr("You feel very sick.", MSGCH_WARN);
            }
            else
            {
                // the poison running through your veins.");
                ouch(1, 0, KILLED_BY_POISON);
                mpr("You feel sick.");
            }

            if ((you.hp == 1 && one_chance_in(3)) || one_chance_in(8))
                reduce_poison_player(1);
        }
    }

    if (you.duration[DUR_DEATHS_DOOR])
    {
        if (you.hp > allowed_deaths_door_hp())
        {
            mpr("Your life is in your own hands once again.", MSGCH_DURATION);
            you.duration[DUR_PARALYSIS] += 5 + random2(5);
            confuse_player( 10 + random2(10) );
            you.hp_max--;
            deflate_hp(you.hp_max, false);
            you.duration[DUR_DEATHS_DOOR] = 0;
        }
        else
            you.duration[DUR_DEATHS_DOOR]--;

        if (you.duration[DUR_DEATHS_DOOR] == 10)
        {
            mpr("Your time is quickly running out!", MSGCH_DURATION);
            you.duration[DUR_DEATHS_DOOR] -= random2(6);
        }
        if (you.duration[DUR_DEATHS_DOOR] == 1)
        {
            mpr("Your life is in your own hands again!", MSGCH_DURATION);
            more();
        }
    }
}

static void check_banished()
{
    if (you.banished)
    {
        you.banished = false;
        if (you.level_type != LEVEL_ABYSS)
        {
            mpr("You are cast into the Abyss!");
            banished(DNGN_ENTER_ABYSS, you.banished_by);
        }
        you.banished_by.clear();
    }
}

/* Perhaps we should write functions like: update_repel_undead(),
   update_liquid_flames(), and so on. Even better, we could have a
   vector of callback functions (or objects) which get installed
   at some point.
*/

static void check_shafts()
{
    for (int i = 0; i < MAX_TRAPS; i++)
    {
        trap_struct &trap = env.trap[i];

        if (trap.type != TRAP_SHAFT)
            continue;

        ASSERT(in_bounds(trap.x, trap.y));

        handle_items_on_shaft(trap.x, trap.y, true);
    }
}

static void check_sanctuary()
{
    if (env.sanctuary_time <= 0)
        return;
        
    decrease_sanctuary_radius();
}

static void world_reacts()
{
    crawl_state.clear_god_acting();

    if (you.num_turns != -1)
    {
        if (you.num_turns < LONG_MAX)
            you.num_turns++;
        if (env.turns_on_level < INT_MAX)
            env.turns_on_level++;
        update_turn_count();
    }
    check_banished();

    check_shafts();

    check_sanctuary();

    run_environment_effects();

    if ( !you.cannot_act() && !you.mutation[MUT_BLURRY_VISION] &&
         random2(50) < you.skills[SK_TRAPS_DOORS] )
    {
        search_around(false); // check nonadjacent squares too
    }

    stealth = check_stealth();

#if 0
    // too annoying for regular diagnostics
    mprf(MSGCH_DIAGNOSTICS, "stealth: %d", stealth );
#endif

    if (you.special_wield != SPWLD_NONE)
        special_wielded();

    if (one_chance_in(10))
    {
        // this is instantaneous
        if (player_teleport() > 0 && one_chance_in(100 / player_teleport()))
            you_teleport_now( true );
        else if (you.level_type == LEVEL_ABYSS && one_chance_in(30))
            you_teleport_now( false, true ); // to new area of the Abyss
    }

    if (env.cgrid[you.x_pos][you.y_pos] != EMPTY_CLOUD)
        in_a_cloud();

    decrement_durations();

    const int food_use = player_hunger_rate();

    if (food_use > 0 && you.hunger >= 40)
        make_hungry( food_use, true );

    // XXX: using an int tmp to fix the fact that hit_points_regeneration
    // is only an unsigned char and is thus likely to overflow. -- bwr
    int tmp = static_cast< int >( you.hit_points_regeneration );

    if (you.hp < you.hp_max && !you.disease && !you.duration[DUR_DEATHS_DOOR])
        tmp += player_regen();

    while (tmp >= 100)
    {
        inc_hp(1, false);
        tmp -= 100;

        you.running.check_hp();
    }

    ASSERT( tmp >= 0 && tmp < 100 );
    you.hit_points_regeneration = static_cast< unsigned char >( tmp );

    // XXX: Doing the same as the above, although overflow isn't an
    // issue with magic point regeneration, yet. -- bwr
    tmp = static_cast< int >( you.magic_points_regeneration );

    if (you.magic_points < you.max_magic_points)
        tmp += 7 + you.max_magic_points / 2;

    while (tmp >= 100)
    {
        inc_mp(1, false);
        tmp -= 100;

        you.running.check_mp();
    }

    ASSERT( tmp >= 0 && tmp < 100 );
    you.magic_points_regeneration = static_cast< unsigned char >( tmp );

    // If you're wielding a rod, it'll gradually recharge.
    recharge_rods();

    viewwindow(true, true);

    if (Options.stash_tracking)
        stashes.update_visible_stashes(
            Options.stash_tracking == STM_ALL? 
            StashTracker::ST_AGGRESSIVE :
            StashTracker::ST_PASSIVE);
    
    handle_monsters();
    check_banished();

    ASSERT(you.time_taken >= 0);
    // make sure we don't overflow
    ASSERT(DBL_MAX - you.elapsed_time > you.time_taken);

    you.elapsed_time += you.time_taken;

    if (you.synch_time <= you.time_taken)
    {
        handle_time(200 + (you.time_taken - you.synch_time));
        you.synch_time = 200;
        check_banished();
    }
    else
    {
        you.synch_time -= you.time_taken;
    }

    manage_clouds();

    if (you.duration[DUR_FIRE_SHIELD] > 0)
        manage_fire_shield();

    // food death check:
    if (you.is_undead != US_UNDEAD && you.hunger <= 500)
    {
        if (!you.cannot_act() && one_chance_in(40))
        {
            mpr("You lose consciousness!", MSGCH_FOOD);
            stop_running();

            you.duration[DUR_PARALYSIS] += 5 + random2(8);

            if (you.duration[DUR_PARALYSIS] > 13)
                you.duration[DUR_PARALYSIS] = 13;
        }

        if (you.hunger <= 100)
        {
            mpr( "You have starved to death.", MSGCH_FOOD );
            ouch( INSTANT_DEATH, 0, KILLED_BY_STARVATION );
        }
    }

    viewwindow(true, false);

    if (you.cannot_act() && any_messages())
        more();

    spawn_random_monsters();
}

#ifdef DGL_SIMPLE_MESSAGING

static struct stat mfilestat;

static void show_message_line(std::string line)
{
    const std::string::size_type sender_pos = line.find(":");
    if (sender_pos == std::string::npos)
        mpr(line.c_str());
    else
    {
        std::string sender = line.substr(0, sender_pos);
        line = line.substr(sender_pos + 1);
        trim_string(line);
        formatted_string fs;
        fs.textcolor(WHITE);
        fs.cprintf("%s: ", sender.c_str());
        fs.textcolor(LIGHTGREY);
        fs.cprintf("%s", line.c_str());
        formatted_mpr(fs, MSGCH_PLAIN, 0);
        take_note(Note( NOTE_MESSAGE, MSGCH_PLAIN, 0,
                        (sender + ": " + line).c_str() ));
    }
}

static void read_each_message()
{
    bool say_got_msg = true;
    FILE *mf = fopen(SysEnv.messagefile.c_str(), "r+");
    if (!mf)
    {
        mprf(MSGCH_WARN, "Couldn't read %s: %s", SysEnv.messagefile.c_str(),
             strerror(errno));
        goto kill_messaging;
    }

    // Read messages, code borrowed from the SIMPLEMAIL patch.
    char line[120];
    
    if (!lock_file_handle(mf, F_RDLCK))
    {
        mprf(MSGCH_WARN, "Failed to lock %s: %s", SysEnv.messagefile.c_str(),
             strerror(errno));
        goto kill_messaging;
    }

    while (fgets(line, sizeof line, mf))
    {
        unlock_file_handle(mf);

        const int len = strlen(line);
        if (len)
        {
            if (line[len - 1] == '\n')
                line[len - 1] = 0;

            if (say_got_msg)
            {
                mprf(MSGCH_PROMPT, "Your messages:");
                say_got_msg = false;
            }

            show_message_line(line);
        }

        if (!lock_file_handle(mf, F_RDLCK))
        {
            mprf(MSGCH_WARN, "Failed to lock %s: %s",
                 SysEnv.messagefile.c_str(),
                 strerror(errno));
            goto kill_messaging;
        }
    }
    if (!lock_file_handle(mf, F_WRLCK))
    {
        mprf(MSGCH_WARN, "Unable to write lock %s: %s",
             SysEnv.messagefile.c_str(),
             strerror(errno));
    }
    if (!ftruncate(fileno(mf), 0))
        mfilestat.st_mtime = 0;
    unlock_file_handle(mf);
    fclose(mf);

    SysEnv.have_messages = false;
    return;

kill_messaging:
    if (mf)
        fclose(mf);
    SysEnv.have_messages = false;
    Options.messaging = false;
}

static void read_messages()
{
    read_each_message();
    update_message_status();
}

static void announce_messages()
{
    // XXX: We could do a NetHack-like mail daemon here at some point.
    mprf("Beep! Your pager goes off! Use _ to check your messages.");
}

static void check_messages()
{
    if (!Options.messaging
        || SysEnv.have_messages
        || SysEnv.messagefile.empty()
        || kbhit()
        || (SysEnv.message_check_tick++ % DGL_MESSAGE_CHECK_INTERVAL))
    {
        return;
    }

    const bool had_messages = SysEnv.have_messages;
    struct stat st;
    if (stat(SysEnv.messagefile.c_str(), &st))
    {
        mfilestat.st_mtime = 0;
        return;
    }

    if (st.st_mtime > mfilestat.st_mtime)
    {
        if (st.st_size)
            SysEnv.have_messages = true;
        mfilestat.st_mtime = st.st_mtime;
    }
    
    if (SysEnv.have_messages && !had_messages)
    {
        announce_messages();
        update_message_status();
    }
}
#endif

static command_type get_next_cmd()
{
#ifdef DGL_SIMPLE_MESSAGING
    check_messages();
#endif

#if DEBUG_DIAGNOSTICS
    // save hunger at start of round
    // for use with hunger "delta-meter" in  output.cc
    you.old_hunger = you.hunger;        
#endif
    
#if DEBUG_ITEM_SCAN
    debug_item_scan();
#endif

    const time_t before = time(NULL);
    keycode_type keyin = get_next_keycode();
    
    const time_t after = time(NULL);

    // Clamp idle time so that play time is more meaningful.
    if (after - before > IDLE_TIME_CLAMP)
    {
        you.real_time  += int(before - you.start_time) + IDLE_TIME_CLAMP;
        you.start_time  = after;
    }

    if (is_userfunction(keyin))
    {
        run_macro(get_userfunction(keyin));
        return (CMD_NEXT_CMD);
    }

    return keycode_to_command(keyin);
}

/* for now, this is an extremely yucky hack */
command_type keycode_to_command( keycode_type key )
{
    switch ( key )
    {
#ifdef USE_TILE
    case '-': return CMD_EDIT_PLAYER_TILE;
    case CK_MOUSE_DONE: return CMD_NEXT_CMD;
    case CK_MOUSE_B1ITEM: return CMD_USE_ITEM;
    case CK_MOUSE_B2ITEM: return CMD_VIEW_ITEM;
#endif // USE_TILE

    case KEY_MACRO_DISABLE_MORE: return CMD_DISABLE_MORE;
    case KEY_MACRO_ENABLE_MORE:  return CMD_ENABLE_MORE;
    case KEY_REPEAT_KEYS:        return CMD_REPEAT_KEYS;
    case 'b': return CMD_MOVE_DOWN_LEFT;
    case 'h': return CMD_MOVE_LEFT;
    case 'j': return CMD_MOVE_DOWN;
    case 'k': return CMD_MOVE_UP;
    case 'l': return CMD_MOVE_RIGHT;
    case 'n': return CMD_MOVE_DOWN_RIGHT;
    case 'u': return CMD_MOVE_UP_RIGHT;
    case 'y': return CMD_MOVE_UP_LEFT;

    case 'a': return CMD_USE_ABILITY;
    case 'c': return CMD_BUTCHER;
    case 'd': return CMD_DROP;
    case 'e': return CMD_EAT;
    case 'f': return CMD_FIRE;
    case 'g': return CMD_PICKUP;
    case 'i': return CMD_DISPLAY_INVENTORY;
    case 'm': return CMD_DISPLAY_SKILLS;
    case 'o': return CMD_EXPLORE;
    case 'p': return CMD_PRAY;
    case 'q': return CMD_QUAFF;
    case 'r': return CMD_READ;
    case 's': return CMD_SEARCH;
    case 't': return CMD_SHOUT;
    case 'v': return CMD_EVOKE;
    case 'w': return CMD_WIELD_WEAPON;
    case 'x': return CMD_LOOK_AROUND;
    case 'z': return CMD_CAST_SPELL;

    case 'B': return CMD_RUN_DOWN_LEFT;
    case 'H': return CMD_RUN_LEFT;
    case 'J': return CMD_RUN_DOWN;
    case 'K': return CMD_RUN_UP;
    case 'L': return CMD_RUN_RIGHT;
    case 'N': return CMD_RUN_DOWN_RIGHT;
    case 'U': return CMD_RUN_UP_RIGHT;
    case 'Y': return CMD_RUN_UP_LEFT;

    case 'A': return CMD_DISPLAY_MUTATIONS;
    case 'C': return CMD_CLOSE_DOOR;
    case 'E': return CMD_EXPERIENCE_CHECK;
    case 'F': return CMD_NO_CMD;
    case 'G': return CMD_NO_CMD;
    case 'I': return CMD_DISPLAY_SPELLS;
    case 'M': return CMD_MEMORISE_SPELL;
    case 'O': return CMD_OPEN_DOOR;
    case 'P': return CMD_WEAR_JEWELLERY;
    case 'Q': return CMD_QUIT;
    case 'R': return CMD_REMOVE_JEWELLERY;
    case 'S': return CMD_SAVE_GAME;
    case 'T': return CMD_REMOVE_ARMOUR;
    case 'V': return CMD_GET_VERSION;
    case 'W': return CMD_WEAR_ARMOUR;
    case 'X': return CMD_DISPLAY_MAP;
    case 'Z': return CMD_ZAP_WAND;

    case '.': return CMD_MOVE_NOWHERE;
    case '<': return CMD_GO_UPSTAIRS;
    case '>': return CMD_GO_DOWNSTAIRS;
    case '@': return CMD_DISPLAY_CHARACTER_STATUS;
    case '%': return CMD_RESISTS_SCREEN;
    case ',': return CMD_PICKUP;
    case ':': return CMD_MAKE_NOTE;
    case '_': return CMD_READ_MESSAGES;
    case ';': return CMD_INSPECT_FLOOR;
    case '^': return CMD_DISPLAY_RELIGION;
    case '#': return CMD_CHARACTER_DUMP;
    case '=': return CMD_ADJUST_INVENTORY;
    case '?': return CMD_DISPLAY_COMMANDS;
    case '!': return CMD_ANNOTATE_LEVEL;
    case CONTROL('D'):
    case '~': return CMD_MACRO_ADD;
    case '&': return CMD_WIZARD;
    case '"': return CMD_LIST_JEWELLERY;
    case '{': return CMD_INSCRIBE_ITEM;
    case '[': return CMD_LIST_ARMOUR;
    case ']': return CMD_LIST_EQUIPMENT;
    case '(': return CMD_CYCLE_QUIVER_FORWARD;
    case ')': return CMD_LIST_WEAPONS;
    case '\\': return CMD_DISPLAY_KNOWN_OBJECTS;
    case '\'': return CMD_WEAPON_SWAP;
    case '`': return CMD_PREV_CMD_AGAIN;

    case '0': return CMD_REPEAT_CMD;
    case '5': return CMD_REST;

    case CONTROL('B'): return CMD_OPEN_DOOR_DOWN_LEFT;
    case CONTROL('H'): return CMD_OPEN_DOOR_LEFT;
    case CONTROL('J'): return CMD_OPEN_DOOR_DOWN;
    case CONTROL('K'): return CMD_OPEN_DOOR_UP;
    case CONTROL('L'): return CMD_OPEN_DOOR_RIGHT;
    case CONTROL('N'): return CMD_OPEN_DOOR_DOWN_RIGHT;
    case CONTROL('U'): return CMD_OPEN_DOOR_UP_LEFT;
    case CONTROL('Y'): return CMD_OPEN_DOOR_UP_RIGHT;

    case CONTROL('A'): return CMD_TOGGLE_AUTOPICKUP;
    case CONTROL('C'): return CMD_CLEAR_MAP;
    case CONTROL('E'): return CMD_FORGET_STASH;
    case CONTROL('F'): return CMD_SEARCH_STASHES;
    case CONTROL('G'): return CMD_INTERLEVEL_TRAVEL;
    case CONTROL('M'): return CMD_NO_CMD;
    case CONTROL('O'): return CMD_DISPLAY_OVERMAP;
    case CONTROL('P'): return CMD_REPLAY_MESSAGES;
#ifdef USE_TILE
    case CONTROL('Q'): return CMD_EDIT_PREFS;
#else
    case CONTROL('Q'): return CMD_NO_CMD;
#endif
    case CONTROL('R'): return CMD_REDRAW_SCREEN;
    case CONTROL('S'): return CMD_MARK_STASH;
    case CONTROL('V'): return CMD_TOGGLE_AUTOPRAYER;
    case CONTROL('W'): return CMD_FIX_WAYPOINT;
    case CONTROL('X'): return CMD_SAVE_GAME_NOW;
    case CONTROL('Z'): return CMD_SUSPEND_GAME;

    case CK_MOUSE_MOVE:  return CMD_MOUSE_MOVE;
    case CK_MOUSE_CLICK: return CMD_MOUSE_CLICK;
    default: return CMD_NO_CMD;
    }
}

keycode_type get_next_keycode()
{
    keycode_type keyin;

    flush_input_buffer( FLUSH_BEFORE_COMMAND );

#ifdef USE_TILE
    tile_draw_inv(-1, REGION_INV1);
 #ifdef USE_X11
    update_screen();
 #endif
    mouse_set_mode(MOUSE_MODE_COMMAND);
    keyin = unmangle_direction_keys(getch_with_command_macros());
    mouse_set_mode(MOUSE_MODE_NORMAL);
#else // !USE_TILE
    keyin = unmangle_direction_keys(getch_with_command_macros());
#endif

    if (!is_synthetic_key(keyin))
        mesclr();

    return (keyin);
}

/*
   Opens doors and handles some aspects of untrapping. If either move_x or
   move_y are non-zero,  the pair carries a specific direction for the door
   to be opened (eg if you type ctrl - dir).
 */
static void open_door(int move_x, int move_y, bool check_confused)
{
    struct dist door_move;
    int dx, dy;             // door x, door y

    if (you.attribute[ATTR_HELD])
    {
        free_self_from_net();
        you.turn_is_over = true;
        return;
    }

    if (check_confused && you.duration[DUR_CONF] && !one_chance_in(3))
    {
        move_x = random2(3) - 1;
        move_y = random2(3) - 1;
    }

    door_move.dx = move_x;
    door_move.dy = move_y;

    if (move_x || move_y)
    {
        // convenience
        dx = you.x_pos + move_x;
        dy = you.y_pos + move_y;

        const int mon = mgrd[dx][dy];

        if (mon != NON_MONSTER && player_can_hit_monster(&menv[mon]))
        {
            if (mons_is_caught(&menv[mon]))
            {

                std::string prompt  = "Do you want to try to take the net off ";
                            prompt += (&menv[mon])->name(DESC_NOCAP_THE);
                            prompt += '?';

                if (yesno(prompt.c_str(), true, 'n'))
                {
                    remove_net_from(&menv[mon]);
                    return;
                }

            }

            you_attack(mgrd[dx][dy], true);
            you.turn_is_over = true;

            if (you.berserk_penalty != NO_BERSERK_PENALTY)
                you.berserk_penalty = 0;

            return;
        }

        if (grd[dx][dy] >= DNGN_TRAP_MECHANICAL
            && grd[dx][dy] <= DNGN_TRAP_NATURAL)
        {
            if (env.cgrid[dx][dy] != EMPTY_CLOUD)
            {
                mpr("You can't get to that trap right now.");
                return;
            }

            disarm_trap(door_move);
            return;
        }

    } 
    else 
    {
        mpr("Which direction?", MSGCH_PROMPT);
        direction( door_move, DIR_DIR );
        if (!door_move.isValid)
            return;

        // convenience
        dx = you.x_pos + door_move.dx;
        dy = you.y_pos + door_move.dy;

        if (!in_bounds(dx, dy) || grd[dx][dy] != DNGN_CLOSED_DOOR)
        {
            mpr( "There's no door there." );
            // Don't lose a turn.
            return;
        }
    }

    if (grd[dx][dy] == DNGN_CLOSED_DOOR)
    {
        std::set<coord_def> all_door;
        _find_connected_identical(coord_def(dx,dy), grd[dx][dy], all_door);
        const char* noun = get_door_noun(all_door.size()).c_str();
        
        int skill = you.dex + (you.skills[SK_TRAPS_DOORS] + you.skills[SK_STEALTH]) / 2;

        if (you.duration[DUR_BERSERKER])
        {
            if (silenced(you.x_pos, you.y_pos))
                mprf("The %s flies open!", noun);
            else
            {
                // XXX: better flavor for gateways?
                mprf("The %s flies open with a bang!", noun);
                noisy( 15, you.x_pos, you.y_pos );
            }
        }
        else if (one_chance_in(skill) && !silenced(you.x_pos, you.y_pos))
        {
            mprf( "As you open the %s, it creaks loudly!", noun );
            noisy( 10, you.x_pos, you.y_pos );
        }
        else
        {
            const char* verb = player_is_airborne() ? "reach down and open" : "open";
            mprf( "You %s the %s.", verb, noun );
        }

        for (std::set<coord_def>::iterator i = all_door.begin();
             i != all_door.end(); ++i)
        {
            const coord_def& dc = *i;
            grd[dc.x][dc.y] = DNGN_OPEN_DOOR;
        }
        you.turn_is_over = true;
    }
    else
    {
        mpr("You swing at nothing.");
        make_hungry(3, true);
        you.turn_is_over = true;
    }
}                               // end open_door()

/*
 * Similar to open_door. Can you spot the difference?
 * FIX ME: closing a gate should update all tiles involved!
 */
static void close_door(int door_x, int door_y)
{
    struct dist door_move;
    int dx, dy;             // door x, door y

    door_move.dx = door_x;
    door_move.dy = door_y;

    if (!(door_x || door_y))
    {
        mpr("Which direction?", MSGCH_PROMPT);
        direction( door_move, DIR_DIR );
        if (!door_move.isValid)
            return;
    }

    if (door_move.dx == 0 && door_move.dy == 0)
    {
        mpr("You can't close doors on yourself!");
        return;
    }

    // convenience
    dx = you.x_pos + door_move.dx;
    dy = you.y_pos + door_move.dy;

    if (grd[dx][dy] == DNGN_OPEN_DOOR)
    {
        std::set<coord_def> all_door;
        _find_connected_identical(coord_def(dx,dy), grd[dx][dy], all_door);
        const char* noun = get_door_noun(all_door.size()).c_str();

        const coord_def you_coord(you.x_pos, you.y_pos);
        for (std::set<coord_def>::iterator i = all_door.begin();
             i != all_door.end(); ++i)
        {
            const coord_def& dc = *i;
            if (mgrd[dc.x][dc.y] != NON_MONSTER)
            {
                // Need to make sure that turn_is_over is set if creature is 
                // invisible
                mprf("There's a creature in the %sway!", noun);
                door_move.dx = 0;
                door_move.dy = 0;
                return;
            }

            if (igrd[dc.x][dc.y] != NON_ITEM)
            {
                mprf("There's something blocking the %sway.", noun);
                door_move.dx = 0;
                door_move.dy = 0;
                return;
            }

            if (you_coord == dc)
            {
                mprf("There's a thickheaded creature in the %sway!", noun);
                return;
            }
        }

        int skill = you.dex + (you.skills[SK_TRAPS_DOORS] + you.skills[SK_STEALTH]) / 2;

        if (you.duration[DUR_BERSERKER])
        {
            if (silenced(you.x_pos, you.y_pos))
                mprf("You slam the %s shut!", noun);
            else
            {
                mprf("You slam the %s shut with an echoing bang!", noun);
                noisy( 25, you.x_pos, you.y_pos );
            }
        }
        else if (one_chance_in(skill) && !silenced(you.x_pos, you.y_pos))
        {
            mprf("As you close the %s, it creaks loudly!", noun);
            noisy( 10, you.x_pos, you.y_pos );
        }
        else
        {
            const char* verb = player_is_airborne() ? "reach down and close" : "close";
            mprf( "You %s the %s.", verb, noun );
        }

        for (std::set<coord_def>::iterator i = all_door.begin();
             i != all_door.end(); ++i)
        {
            const coord_def& dc = *i;
            grd[dc.x][dc.y] = DNGN_CLOSED_DOOR;
        }
        you.turn_is_over = true;
    }
    else
    {
        mpr("There isn't anything that you can close there!");
    }
}                               // end open_door()

// initialise whole lot of stuff...
// returns true if a new character
static bool initialise(void)
{
    Options.fixup_options();
    
    // read the options the player used last time they created a new
    // character.
    read_startup_prefs();
    
    you.symbol = '@';
    you.colour = LIGHTGREY;

    seed_rng();
    get_typeid_array().init(ID_UNKNOWN_TYPE);
    init_char_table(Options.char_set);
    init_feature_table();
    init_monster_symbols();
    init_properties();
    init_spell_descs();        // this needs to be way up top {dlb}
    init_mon_name_cache();

    msg::initialise_mpr_streams();

    // init item array:
    for (int i = 0; i < MAX_ITEMS; i++)
        init_item(i);

    // empty messaging string
    info[0] = 0;

    for (int i = 0; i < MAX_MONSTERS; i++)
        menv[i].reset();

    igrd.init(NON_ITEM);
    mgrd.init(NON_MONSTER);
    env.map.init(map_cell());

    you.unique_creatures.init(false);
    you.unique_items.init(UNIQ_NOT_EXISTS);

    // initialise tag system before we try loading anything!
    tag_init();

    // set up the Lua interpreter for the dungeon builder.
    init_dungeon_lua();
    
    // Read special levels and vaults.
    read_maps();

    // Initialise internal databases.
    databaseSystemInit();

    init_feat_desc_cache();
    init_spell_name_cache();
    init_item_name_cache();
    
    cio_init();

    // system initialisation stuff:
    textbackground(0);

    clrscr();

#ifdef DEBUG_DIAGNOSTICS
    sanity_check_mutation_defs();
    if (crawl_state.map_stat_gen)
    {
        generate_map_stats();
        end(0, false);
    }
#endif
    
    // sets up a new game:
    const bool newc = new_game();
    if (!newc)
        restore_game();

    // Fix the mutation definitions for the species we're playing.
    fixup_mutations();
    
    // Load macros
    macro_init();

    crawl_state.need_save = true;

    calc_hp();
    calc_mp();

    run_map_preludes();

    load( you.entering_level? you.transit_stair : DNGN_STONE_STAIRS_DOWN_I,
          you.entering_level? LOAD_ENTER_LEVEL :
          newc              ? LOAD_START_GAME : LOAD_RESTART_GAME,
          NUM_LEVEL_AREA_TYPES, -1, you.where_are_you );

#if DEBUG_DIAGNOSTICS
    // Debug compiles display a lot of "hidden" information, so we auto-wiz
    you.wizard = true;
#endif

#ifdef USE_TILE
    TilePlayerInit();
    TileInitItems();
    TileNewLevel(true);
#endif

    init_properties();
    burden_change();
    make_hungry(0,true);

    you.redraw_strength     = true;
    you.redraw_intelligence = true;
    you.redraw_dexterity    = true;
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_experience   = true;
    you.redraw_gold         = true;
    you.wield_change        = true;
    you.quiver_change       = true;

    you.start_time = time( NULL );      // start timer on session

#ifdef CLUA_BINDINGS
    clua.runhook("chk_startgame", "b", newc);
    std::string yname = you.your_name;
    read_init_file(true);
    Options.fixup_options();
    strncpy(you.your_name, yname.c_str(), kNameLen);
    you.your_name[kNameLen - 1] = 0;

    // In case Lua changed the character set.
    init_char_table(Options.char_set);
    init_feature_table();
    init_monster_symbols();
#endif

    draw_border();
    new_level();
    update_turn_count();

    trackers_init_new_level(false);
    // Mark items in inventory as of unknown origin.
    origin_set_inventory(origin_set_unknown);

    // set vision radius to player's current vision
    setLOSRadius( you.current_vision );

    if (newc)
    {
        // For a new game, wipe out monsters in LOS.
        zap_los_monsters();
    }

    set_cursor_enabled(false);
    viewwindow(1, false);   // This just puts the view up for the first turn.

    activate_notes(true);

    add_key_recorder(&repeat_again_rec);

    return (newc);
}

// An attempt to tone down berserk a little bit. -- bwross
//
// This function does the accounting for not attacking while berserk
// This gives a triangular number function for the additional penalty
// Turn:    1  2  3   4   5   6   7   8
// Penalty: 1  3  6  10  15  21  28  36
//
// Total penalty (including the standard one during upkeep is:
//          2  5  9  14  20  27  35  44
//
static void do_berserk_no_combat_penalty(void)
{
    // Butchering/eating a corpse will maintain a blood rage.
    const int delay = current_delay_action();
    if (delay == DELAY_BUTCHER || delay == DELAY_EAT)
        return;

    if (you.berserk_penalty == NO_BERSERK_PENALTY)
        return;

    if (you.duration[DUR_BERSERKER])
    {
        you.berserk_penalty++;

        switch (you.berserk_penalty)
        {
        case 2:
            mpr("You feel a strong urge to attack something.", MSGCH_DURATION);
            break;
        case 4:
            mpr("You feel your anger subside.", MSGCH_DURATION);
            break;
        case 6:
            mpr("Your blood rage is quickly leaving you.", MSGCH_DURATION);
            break;
        }

        // I do these three separately, because the might and
        // haste counters can be different.
        you.duration[DUR_BERSERKER] -= you.berserk_penalty;
        if (you.duration[DUR_BERSERKER] < 1)
            you.duration[DUR_BERSERKER] = 1;

        you.duration[DUR_MIGHT] -= you.berserk_penalty;
        if (you.duration[DUR_MIGHT] < 1)
            you.duration[DUR_MIGHT] = 1;

        you.duration[DUR_HASTE] -= you.berserk_penalty;
        if (you.duration[DUR_HASTE] < 1)
            you.duration[DUR_HASTE] = 1;
    }
    return;
}                               // end do_berserk_no_combat_penalty()


// Called when the player moves by walking/running. Also calls attack
// function etc when necessary.
static void move_player(int move_x, int move_y)
{
    bool attacking = false;
    bool moving = true;         // used to prevent eventual movement (swap)
    bool swap = false;
    monsters *beholder = NULL;  // beholding monster preventing movement

    if (you.attribute[ATTR_HELD])
    {
        free_self_from_net();
        you.turn_is_over = true;
        return;
    }

    if (you.duration[DUR_CONF])
    {
        if (!one_chance_in(3))
        {
            move_x = random2(3) - 1;
            move_y = random2(3) - 1;
            you.reset_prev_move();
        }

        const int new_targ_x = you.x_pos + move_x;
        const int new_targ_y = you.y_pos + move_y;
        if (!in_bounds(new_targ_x, new_targ_y)
            || !you.can_pass_through(new_targ_x, new_targ_y))
        {
            you.turn_is_over = true;
            mpr("Ouch!");
            apply_berserk_penalty = true;
            crawl_state.cancel_cmd_repeat();

            return;
        }
    } // end of if you.duration[DUR_CONF]

    const int targ_x = you.x_pos + move_x;
    const int targ_y = you.y_pos + move_y;
    const dungeon_feature_type targ_grid  =  grd[ targ_x ][ targ_y ];
    const unsigned short targ_monst = mgrd[ targ_x ][ targ_y ];
    const bool           targ_pass  = you.can_pass_through(targ_x, targ_y);

    // you can swap places with a friendly monster if you're not confused
    // or if both of you are inside a sanctuary
    const bool can_swap_places = targ_monst != NON_MONSTER
                                 && (mons_friendly(&menv[targ_monst])
                                       && !you.duration[DUR_CONF]
                                     || is_sanctuary(you.x_pos, you.y_pos)
                                        && is_sanctuary(targ_x, targ_y));

    // cannot move away from mermaid but you CAN fight neighbouring squares
    if (you.duration[DUR_BEHELD] && !you.duration[DUR_CONF])
    {   
        for (unsigned int i = 0; i < you.beheld_by.size(); i++)
        {
             monsters* mon = &menv[you.beheld_by[i]];
             coord_def pos = mon->pos();
             int olddist = distance(you.x_pos, you.y_pos, pos.x, pos.y);
             int newdist = distance(you.x_pos + move_x, you.y_pos + move_y,
                                    pos.x, pos.y);

             if (olddist < newdist)
             {
                 beholder = mon;
                 break;
             }
        }
    } // end of beholding check

    if (you.running.check_stop_running())
    {
        move_x = 0;
        move_y = 0;
        // [ds] Do we need this? Shouldn't it be false to start with?
        you.turn_is_over = false;
        return;
    }

    if (targ_monst != NON_MONSTER && !mons_is_submerged(&menv[targ_monst]))
    {
        monsters *mon = &menv[targ_monst];

        if (can_swap_places && !beholder)
        {
            if (swap_places( mon ))
                swap = true;
            else
                moving = false;
        }
        else if (!can_swap_places) // attack!
        {
            if (is_sanctuary(you.x_pos, you.y_pos)
                || is_sanctuary(mon->x, mon->y))
            {
                snprintf(info, INFO_SIZE,
                         "Really attack %s, despite your sanctuary? ",
                         mon->name(DESC_NOCAP_THE).c_str());
                         
                if (!yesno(info, true, 'n'))
                    return;
            }

            // XXX: Moving into a normal wall does nothing and uses no
            // turns or energy, but moving into a wall which contains
            // an invisible monster attacks the monster, thus allowing
            // the player to figure out which adjacent wall an invis
            // monster is in "for free".
            you_attack( targ_monst, true );
            you.turn_is_over = true;

            // we don't want to create a penalty if there isn't
            // supposed to be one
            if (you.berserk_penalty != NO_BERSERK_PENALTY)
                you.berserk_penalty = 0;

            attacking = true;
        }
    }

    if (!attacking && targ_pass && moving && !beholder)
    {
        you.time_taken *= player_movement_speed();
        you.time_taken /= 10;
        if (!move_player_to_grid(targ_x, targ_y, true, false, swap))
            return;

        you.prev_move_x = move_x;
        you.prev_move_y = move_y;

        move_x = 0;
        move_y = 0;

        you.turn_is_over = true;
        // item_check( false );
        request_autopickup();

    }

    // BCR - Easy doors single move
    if (targ_grid == DNGN_CLOSED_DOOR && Options.easy_open && !attacking)
    {
        open_door(move_x, move_y, false);
        you.prev_move_x = move_x;
        you.prev_move_y = move_y;
    }
    else if (!targ_pass && !attacking)
    {
        stop_running();

        move_x = 0;
        move_y = 0;

        you.turn_is_over = 0;
        crawl_state.cancel_cmd_repeat();
    }
    else if (beholder)
    {
        mprf("You cannot move away from %s!",
            beholder->name(DESC_NOCAP_THE, true).c_str());

        move_x = 0;
        move_y = 0;
        return;
    }

    if (you.running == RMODE_START)
        you.running = RMODE_CONTINUE;

    if (you.level_type == LEVEL_ABYSS
            && (you.x_pos <= 15 || you.x_pos >= (GXM - 16)
                    || you.y_pos <= 15 || you.y_pos >= (GYM - 16)))
    {
        area_shift();
        you.pet_target = MHITNOT;

#if DEBUG_DIAGNOSTICS
        mpr( "Shifting.", MSGCH_DIAGNOSTICS );
        int igly = 0;
        int ig2 = 0;

        for (igly = 0; igly < MAX_ITEMS; igly++)
        {
            if (is_valid_item( mitm[igly] ))
                ig2++;
        }

        mprf( MSGCH_DIAGNOSTICS, "Number of items present: %d", ig2 );

        ig2 = 0;
        for (igly = 0; igly < MAX_MONSTERS; igly++)
        {
            if (menv[igly].type != -1)
                ig2++;
        }

        mprf( MSGCH_DIAGNOSTICS, "Number of monsters present: %d", ig2);
        mprf( MSGCH_DIAGNOSTICS, "Number of clouds present: %d", env.cloud_no);
#endif
    }

    apply_berserk_penalty = !attacking;
}                               // end move_player()


static int get_num_and_char_keyfun(int &ch)
{
    if (ch == CK_BKSP || isdigit(ch) || ch >= 128)
        return 1;

    return -1;
}

static int get_num_and_char(const char* prompt, char* buf, int buf_len)
{
    if (prompt != NULL)
        mpr(prompt);

    line_reader reader(buf, buf_len);

    reader.set_keyproc(get_num_and_char_keyfun);

    return reader.read_line(true);
}

static void setup_cmd_repeat()
{
    if (is_processing_macro())
    {
        flush_input_buffer(FLUSH_ABORT_MACRO);
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return;
    }

    ASSERT(!crawl_state.is_repeating_cmd());

    char buf[80];

    // Function ensures that the buffer contains only digits.
    int ch = get_num_and_char("Number of times to repeat, then command key: ",
                              buf, 80);

    if (ch == ESCAPE)
    {
        // This *might* be part of the trigger for a macro.
        keyseq trigger;
        trigger.push_back(ch);

        if (get_macro_buf_size() == 0)
        {
            // Was just a single ESCAPE key, so not a macro trigger.
            canned_msg( MSG_OK );
            crawl_state.cancel_cmd_again();
            crawl_state.cancel_cmd_repeat();
            flush_input_buffer(FLUSH_REPLAY_SETUP_FAILURE);
            return;
        }
        ch = getchm();
        trigger.push_back(ch);

        // Now that we have the entirety of the (possible) macro trigger,
        // clear out the keypress recorder so that we won't have recorded
        // the trigger twice.
        repeat_again_rec.clear();

        insert_macro_into_buff(trigger);

        ch = getchm();
        if (ch == ESCAPE)
        {
            if (get_macro_buf_size() > 0)
                // User pressed an Alt key which isn't bound to a macro.
                mpr("That key isn't bound to a macro.");
            else
                // Wasn't a macro trigger, just an ordinary escape.
                canned_msg( MSG_OK );

            crawl_state.cancel_cmd_again();
            crawl_state.cancel_cmd_repeat();
            flush_input_buffer(FLUSH_REPLAY_SETUP_FAILURE);
            return;
        }
        // *WAS* a macro trigger, keep going.
    }

    if (strlen(buf) == 0)
    {
        mpr("You must enter the number of times for the command to repeat.");

        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        flush_input_buffer(FLUSH_REPLAY_SETUP_FAILURE);

        return;
    }

    int count = atoi(buf);
    
    if (crawl_state.doing_prev_cmd_again)
        count = crawl_state.prev_cmd_repeat_goal;

    if (count <= 0)
    {
        canned_msg( MSG_OK );
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        flush_input_buffer(FLUSH_REPLAY_SETUP_FAILURE);
        return;
    }

    if (crawl_state.doing_prev_cmd_again)
    {
        // If a "do previous command again" caused a command
        // repetition to be redone, the keys to be repeated are
        // already in the key rercording buffer, so we just need to
        // discard all the keys saying how many times the command
        // should be repeated.
        do {
            repeat_again_rec.keys.pop_front();
        } while (repeat_again_rec.keys[0] != ch);

        repeat_again_rec.keys.pop_front();
    }

    // User can type space or enter and then the command key, in case
    // they want to repeat a command bound to a number key.
    c_input_reset(true);
    if (ch == ' ' || ch == CK_ENTER)
    {
        if (!crawl_state.doing_prev_cmd_again)
            repeat_again_rec.keys.pop_back();

        mpr("Enter command to be repeated: ");
        // Enable the cursor to read input. The cursor stays on while
        // the command is being processed, so subsidiary prompts
        // shouldn't need to turn it on explicitly.
        cursor_control con(true);

        crawl_state.waiting_for_command = true;

        ch = get_next_keycode();

        crawl_state.waiting_for_command = false;
    }

    command_type cmd = keycode_to_command( (keycode_type) ch);

    if (cmd != CMD_MOUSE_MOVE)
        c_input_reset(false);

    if (!is_processing_macro() && !cmd_is_repeatable(cmd))
    {
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        flush_input_buffer(FLUSH_REPLAY_SETUP_FAILURE);
        return;
    }

    if (!crawl_state.doing_prev_cmd_again && cmd != CMD_PREV_CMD_AGAIN)
        crawl_state.prev_cmd_keys = repeat_again_rec.keys;

    if (is_processing_macro())
    {
        // Put back in first key of the expanded macro, which
        // get_next_keycode() fetched
        repeat_again_rec.paused = true;
        macro_buf_add(ch, true);

        // If we're repeating a macro, get rid the keys saying how
        // many times to repeat, because the way that macros are
        // repeated means that the number keys will be repeated if
        // they aren't discarded.
        keyseq &keys = repeat_again_rec.keys;
        ch = keys[keys.size() - 1];
        while (isdigit(ch) || ch == ' ' || ch == CK_ENTER)
        {
            keys.pop_back();
            ASSERT(keys.size() > 0);
            ch = keys[keys.size() - 1];
        }
    }

    repeat_again_rec.paused = false;
    // Discard the setup for the command repetition, since what's
    // going to be repeated is yet to be typed, except for the fist
    // key typed which has to be put back in (unless we're repeating a
    // macro, in which case everything to be repeated is already in
    // the macro buffer).
    if (!is_processing_macro())
    {
        repeat_again_rec.clear();
        macro_buf_add(ch, crawl_state.doing_prev_cmd_again);
    }

    crawl_state.cmd_repeat_start     = true;
    crawl_state.cmd_repeat_count     = 0;
    crawl_state.repeat_cmd           = cmd;
    crawl_state.cmd_repeat_goal      = count;
    crawl_state.prev_cmd_repeat_goal = count;
    crawl_state.prev_repetition_turn = you.num_turns;

    crawl_state.cmd_repeat_started_unsafe = !i_feel_safe();

    crawl_state.input_line_strs.clear();
}

static void do_prev_cmd_again()
{
    if (is_processing_macro())
    {
        mpr("Can't re-do previous command from within a macro.");
        flush_input_buffer(FLUSH_ABORT_MACRO);
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return;
    }

    if (crawl_state.prev_cmd == CMD_NO_CMD)
    {
        mpr("No previous command to re-do.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return;
    }

    ASSERT(!crawl_state.doing_prev_cmd_again
           || (crawl_state.is_repeating_cmd()
               && crawl_state.repeat_cmd == CMD_PREV_CMD_AGAIN));

    crawl_state.doing_prev_cmd_again = true;
    repeat_again_rec.paused          = false;

    if (crawl_state.prev_cmd == CMD_REPEAT_CMD)
    {
        crawl_state.cmd_repeat_start     = true;
        crawl_state.cmd_repeat_count     = 0;
        crawl_state.cmd_repeat_goal      = crawl_state.prev_cmd_repeat_goal;
        crawl_state.prev_repetition_turn = you.num_turns;
    }

    const keyseq &keys = crawl_state.prev_cmd_keys;
    ASSERT(keys.size() > 0);

    // Insert keys at front of input buffer, rather than at the end,
    // since if the player holds down the "`" key, then the buffer
    // might get two "`" in a row, and if the keys to be replayed go after
    // the second "`" then we get an assertion.
    macro_buf_add(keys, true);

    bool was_doing_repeats = crawl_state.is_repeating_cmd();

    input();

    // crawl_state.doing_prev_cmd_again can be set to false
    // while input() does its stuff if something causes
    // crawl_state.cancel_cmd_again() to be called.
    while (!was_doing_repeats && crawl_state.is_repeating_cmd()
           && crawl_state.doing_prev_cmd_again)
    {
        input();
    }

    if (!was_doing_repeats && crawl_state.is_repeating_cmd()
        && !crawl_state.doing_prev_cmd_again)
    {
        crawl_state.cancel_cmd_repeat();
    }

    crawl_state.doing_prev_cmd_again = false;
}

static void update_replay_state()
{
    if (crawl_state.is_repeating_cmd())
    {
        // First repeat is to copy down the keys the user enters,
        // grab them so we can go on autopilot for the remaining
        // iterations.
        if (crawl_state.cmd_repeat_start)
        {
            ASSERT(repeat_again_rec.keys.size() > 0);

            crawl_state.cmd_repeat_start = false;
            crawl_state.repeat_cmd_keys  = repeat_again_rec.keys;

            // Setting up the "previous command key sequence"
            // for a repeated command is different than normal,
            // since in addition to all of the keystrokes for
            // the command, it needs the repeat command plus the
            // number of repeats at the very beginning of the
            // sequence.
            if (!crawl_state.doing_prev_cmd_again)
            {
                keyseq &prev = crawl_state.prev_cmd_keys;
                keyseq &curr = repeat_again_rec.keys;

                if (is_processing_macro())
                    prev = curr;
                else
                {
                    // Skip first key, because that's command key that's
                    // being repeated, which crawl_state.prev_cmd_keys
                    // aleardy contains.
                    keyseq::iterator begin = curr.begin();
                    begin++;

                    prev.insert(prev.end(), begin, curr.end());
                }
            }
            
            repeat_again_rec.paused = true;
            macro_buf_add(KEY_REPEAT_KEYS);
        }
    }

    if (!crawl_state.is_replaying_keys() && !crawl_state.cmd_repeat_start
        && crawl_state.prev_cmd != CMD_NO_CMD)
    {
        if (repeat_again_rec.keys.size() > 0)
            crawl_state.prev_cmd_keys = repeat_again_rec.keys;
    }

    if (!is_processing_macro())
        repeat_again_rec.clear();
}


void compile_time_asserts()
{
    // Check that the numbering comments in enum.h haven't been
    // disturbed accidentally.
    COMPILE_CHECK(SK_UNARMED_COMBAT == 19       , c1);
    COMPILE_CHECK(SK_EVOCATIONS == 39           , c2);
    COMPILE_CHECK(SP_VAMPIRE == 34              , c3);
    COMPILE_CHECK(SPELL_BOLT_OF_MAGMA == 18     , c4);
    COMPILE_CHECK(SPELL_POISON_ARROW == 94      , c5);
    COMPILE_CHECK(SPELL_SUMMON_MUSHROOMS == 221 , c6);

    //jmf: NEW ASSERTS: we ought to do a *lot* of these
    COMPILE_CHECK(NUM_SPELLS < SPELL_NO_SPELL   , c7);
    COMPILE_CHECK(NUM_JOBS < JOB_UNKNOWN        , c8);
}
