/*
 *  File:       acr.cc
 *  Summary:    Main entry point, event loop, and some initialization functions
 *  Written by: Linley Henzell
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
#if !defined(__IBMCPP__) && !defined(MAC)
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

#ifdef DOS
#include <conio.h>
#include <file.h>
#endif

#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

#ifdef USE_EMX
#include <sys/types.h>
#endif

#ifdef OS9
#include <stat.h>
#else
#include <sys/stat.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "abyss.h"
#include "chardump.h"
#include "command.h"
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
#include "items.h"
#include "lev-pand.h"
#include "macro.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "newgame.h"
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
#include "stuff.h"
#include "tags.h"
#include "transfor.h"
#include "view.h"
#include "wpn-misc.h"

struct crawl_environment env;
struct player you;
struct system_environment SysEnv;

char info[ INFO_SIZE ];         // messaging queue extern'd everywhere {dlb}

int stealth;                    // externed in view.h     // no it is not {dlb}
char use_colour = 1;

FixedVector< char, NUM_STATUE_TYPES >  Visible_Statue;

// set to true once a new game starts or an old game loads
bool game_has_started = false;

// Clockwise, around the compass from north (same order as enum RUN_DIR)
static const struct coord_def Compass[8] = 
{ 
    {  0, -1 }, {  1, -1 }, {  1,  0 }, {  1,  1 },
    {  0,  1 }, { -1,  1 }, { -1,  0 }, { -1, -1 },
};

/*

   Functions needed:
   New:
   int player_speed(player you);
   hit_player(damage, flavour, then last two ouch values, env);


   Old:
   wield(player you);
   show_map
   noisy()
   losight

*/

void (*viewwindow) (char, bool);

/* these are all defined in view.cc: */
extern unsigned char (*mapch) (unsigned char);
extern unsigned char (*mapch2) (unsigned char);
unsigned char mapchar(unsigned char ldfk);
unsigned char mapchar2(unsigned char ldfk);
unsigned char mapchar3(unsigned char ldfk);
unsigned char mapchar4(unsigned char ldfk);

/*
   Function pointers are used to make switching between Linux and DOS char sets
   possible as a runtime option (command-line -c)
*/

// these two are defined in view.cc. What does the player look like?
// (changed for shapechanging)
extern unsigned char your_sign;
extern unsigned char your_colour;

// Functions in main module
static void close_door(char move_x, char move_y);
static void do_berserk_no_combat_penalty(void);
static bool initialise(void);
static void input(void);
static void move_player(char move_x, char move_y);
static void open_door(char move_x, char move_y);

/*
   It all starts here. Some initialisations are run first, then straight to
   new_game and then input.
*/
int main( int argc, char *argv[] )
{
#ifdef USE_ASCII_CHARACTERS
    // Default to the non-ibm set when it makes sense.
    viewwindow = &viewwindow3;
    mapch = &mapchar3;
    mapch2 = &mapchar4;
#else
    // Use the standard ibm default
    viewwindow = &viewwindow2;
    mapch = &mapchar;
    mapch2 = &mapchar2;
#endif

    // Load in the system environment variables
    get_system_environment();

    // parse command line args -- look only for initfile & crawl_dir entries
    if (!parse_args(argc, argv, true))
    {
        // print help
        puts("Command line options:");
        puts("  -name <string>   character name");
        puts("  -race <arg>      preselect race (by letter, abbreviation, or name)");
        puts("  -class <arg>     preselect class (by letter, abbreviation, or name)");
        puts("  -pizza <string>  crawl pizza");
        puts("  -plain           don't use IBM extended characters");
        puts("  -dir <path>      crawl directory");
        puts("  -rc <file>       init file name");
        puts("");
        puts("Command line options override init file options, which override");
        puts("environment options (CRAWL_NAME, CRAWL_PIZZA, CRAWL_DIR, CRAWL_RC).");
        puts("");
        puts("Highscore list options: (Can now be redirected to more, etc)");
        puts("  -scores [N]      highscore list");
        puts("  -tscores [N]     terse highscore list");
        puts("  -vscores [N]     verbose highscore list");
        exit(1);
    }

    // Read the init file
    read_init_file();

    // now parse the args again, looking for everything else.
    parse_args( argc, argv, false );

    if (Options.sc_entries > 0)
    {
        printf( " Best Crawlers -" EOL );
        hiscores_print_list( Options.sc_entries, Options.sc_format );
        exit(0);
    }

#ifdef LINUX
    lincurses_startup();
#endif

#ifdef MAC
    init_mac();
#endif

#ifdef WIN32CONSOLE
    init_libw32c();
#endif

#ifdef USE_MACROS
    // Load macros
    macro_init();
#endif

    init_overmap();             // in overmap.cc (duh?)
    clear_ids();                // in itemname.cc

    bool game_start = initialise();

    if (game_start || Options.always_greet)
    {
        snprintf( info, INFO_SIZE, "Welcome, %s the %s %s.", 
                  you.your_name, species_name( you.species,you.experience_level ), you.class_name );

        mpr( info );

        // Starting messages can go here as this should only happen
        // at the start of a new game -- bwr
        // This message isn't appropriate for Options.always_greet
        if (you.char_class == JOB_WANDERER && game_start)
        {
            int skill_levels = 0;
            for (int i = 0; i <= NUM_SKILLS; i++)
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

        // These need some work -- should make sure that the god's
        // name is metioned, else the message might be confusing.
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
            god_speaks( you.religion, "Let it end in hellfire!");
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
        default:
            break;
        }

        // warn player about their weapon, if unsuitable
        wield_warning(false);
    }

    while (true)
    {
        input();
        //      cprintf("x");
    }

    // Should never reach this stage, right?

#ifdef LINUX
    lincurses_shutdown();
#endif

#ifdef MAC
    deinit_mac();
#endif

#ifdef USE_EMX
    deinit_emx();
#endif

    return 0;
}                               // end main()

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

        if (!yesno( "Do you really want to enter wizard mode?", false ))
            return;

        you.wizard = true;
        redraw_screen();
    }

    mpr( "Enter Wizard Command: ", MSGCH_PROMPT );
    wiz_command = getch();

    switch (wiz_command)
    { 
    case '?':
        list_commands(true);        // tell it to list wizard commands
        redraw_screen();
        break;

    case CONTROL('G'):
        save_ghost(true);
        break; 

    case 'x':
        you.experience = 1 + exp_needed( 2 + you.experience_level );
        level_change();
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
        acquirement( OBJ_RANDOM );
        break;

    case 'v':
        // this command isn't very exciting... feel free to replace
        i = prompt_invent_item( "Value of which item?", -1 );
        if (i == PROMPT_ABORT || !is_random_artefact( you.inv[i] ))
        {
            canned_msg( MSG_OK );
            break;
        }
        else
        {
            snprintf( info, INFO_SIZE, "randart val: %d", randart_value( you.inv[i] ) ); 
            mpr( info );
        }
        break;

    case '+':
        i = prompt_invent_item( "Make an artefact out of which item?", -1 );
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

        // if equiped, apply new randart benefits
        if (j != NUM_EQUIP)
            use_randart( i );

        item_name( you.inv[i], DESC_INVENTORY_EQUIP, info );
        mpr( info );
        break;

    case '|':
        // create all unrand arts
        for (tmp = 1; tmp < NO_UNRANDARTS; tmp++)
        {
            int islot = get_item_slot();
            if (islot == NON_ITEM)
                break;

            make_item_unrandart( mitm[islot], tmp );

            mitm[ islot ].quantity = 1;
            set_ident_flags( mitm[ islot ], ISFLAG_IDENT_MASK );

            move_item_to_grid( &islot, you.x_pos, you.y_pos );
        }

        // create all fixed artefacts
        for (tmp = SPWPN_SINGING_SWORD; tmp <= SPWPN_STAFF_OF_WUCAD_MU; tmp++) 
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
            }
        }
        break;

    case 'B':
        if (you.level_type != LEVEL_ABYSS)
            banished( DNGN_ENTER_ABYSS );
        else
        {
            grd[you.x_pos][you.y_pos] = DNGN_EXIT_ABYSS;
            down_stairs(true, you.your_level);
            untag_followers();
        }
        break;

    case 'g':
        debug_add_skills();
        break;

    case 'G':
        // Genocide... "unsummon" all the monsters from the level.
        for (int mon = 0; mon < MAX_MONSTERS; mon++)
        {
            struct monsters *monster = &menv[mon];

            if (monster->type == -1)
                continue;

            monster_die(monster, KILL_RESET, 0);

        }
        break;

    case 'h':
        you.rotting = 0;
        you.poison = 0;
        you.disease = 0;
        set_hp( abs(you.hp_max), false );
        set_hunger( 5000 + abs(you.hunger), true );
        break;

    case 'H':
        you.rotting = 0;
        you.poison = 0;
        you.disease = 0;
        inc_hp( 10, true );
        set_hp( you.hp_max, false );
        set_hunger( 12000, true );
        you.redraw_hit_points = 1;
        break;

    case 'b':
        blink();            // wizards can always blink
        break;

    case '\"':
    case '~':
        level_travel(0);
        break;

    case 'd':
    case 'D':
        level_travel(1);
        break;

    case 'u':
    case 'U':
        level_travel(-1);
        break;

    case '%':
    case 'o':
        create_spec_object();
        break;

    case 't':
        tweak_object();
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
        grd[you.x_pos][you.y_pos] = DNGN_STONE_STAIRS_DOWN_I;
        break;

    case '<':
        grd[you.x_pos][you.y_pos] = DNGN_ROCK_STAIRS_UP;
        break;

    case 'p':
        grd[you.x_pos][you.y_pos] = DNGN_ENTER_PANDEMONIUM;
        break;

    case 'l':
        grd[you.x_pos][you.y_pos] = DNGN_ENTER_LABYRINTH;
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
        break;

    case 'X':
        Xom_acts(true, 20, true);
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
            grd[you.x_pos][you.y_pos] = atoi(specs);
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
        for (i = 0; i < 20; i++)
        {
            if (you.branch_stairs[i] == 0)
                continue;

            snprintf( info, INFO_SIZE, "Branch %2d is on level %2d", 
                     i, you.branch_stairs[i] + 1 );

            mpr(info);
        }
        break;

    case '{':
        magic_mapping(99, 100);
        break;

    case '^':
        {
            int old_piety = you.piety;

            gain_piety(50);
            snprintf( info, INFO_SIZE, "Congratulations, your piety went from %d to %d!",
                    old_piety, you.piety);
            mpr(info);
        }
        break;

    case '=':
        snprintf( info, INFO_SIZE, 
                  "Cost level: %d  Skill points: %d  Next cost level: %d", 
                  you.skill_cost_level, you.total_skill_points,
                  skill_cost_needed( you.skill_cost_level + 1 ) );

        mpr( info );
        break;

    case '_':
        debug_get_religion();
        break;

    case '\'':
        for (i = 0; i < MAX_ITEMS; i++)
        {
            if (mitm[i].link == NON_ITEM)
                continue;
    
            snprintf( info, INFO_SIZE, "item:%3d link:%3d cl:%3d ty:%3d pl:%3d pl2:%3d sp:%3ld q:%3d",
                     i, mitm[i].link, 
                     mitm[i].base_type, mitm[i].sub_type,
                     mitm[i].plus, mitm[i].plus2, mitm[i].special, 
                     mitm[i].quantity );

            mpr(info);
        }

        strcpy(info, "igrid:");
        mpr(info);

        for (i = 0; i < GXM; i++)
        {
            for (j = 0; j < GYM; j++)
            {
                if (igrd[i][j] != NON_ITEM)
                {
                    snprintf( info, INFO_SIZE, "%3d at (%2d,%2d), cl:%3d ty:%3d pl:%3d pl2:%3d sp:%3ld q:%3d", 
                             igrd[i][j], i, j,
                             mitm[i].base_type, mitm[i].sub_type,
                             mitm[i].plus, mitm[i].plus2, mitm[i].special, 
                             mitm[i].quantity );

                    mpr(info);
                }
            }
        }
        break;

    default:
        mpr("Not a Wizard Command.");
        break;
    }
}
#endif

// This function creates "equivalence classes" so that undiscovered
// traps and secret doors aren't running stopping points.
static char base_grid_type( char grid )
{
    // Don't stop for undiscovered traps:
    if (grid == DNGN_UNDISCOVERED_TRAP)
        return (DNGN_FLOOR);

    // Or secret doors (which currently always look like rock walls):
    if (grid == DNGN_SECRET_DOOR)
        return (DNGN_ROCK_WALL);

    return (grid);
}

// Set up the front facing array for detecting terrain based stops
static void set_run_check( int index, int dir )
{
    you.run_check[index].dx = Compass[dir].x;   
    you.run_check[index].dy = Compass[dir].y;   

    const int targ_x = you.x_pos + Compass[dir].x;
    const int targ_y = you.y_pos + Compass[dir].y;

    you.run_check[index].grid = base_grid_type( grd[ targ_x ][ targ_y ] );
}

// Set up the running variables for the current run.
static void start_running( int dir, char mode )
{
    if (dir == RDIR_REST)
    {
        you.run_x = 0;
        you.run_y = 0;
        you.running = mode;
        return;
    }

    ASSERT( dir >= 0 && dir <= 7 );

    you.run_x = Compass[dir].x;
    you.run_y = Compass[dir].y;
    you.running = mode;

    // Get the compass point to the left/right of intended travel:
    const int left  = (dir - 1 < 0) ? 7 : (dir - 1);
    const int right = (dir + 1 > 7) ? 0 : (dir + 1);

    // Record the direction and starting tile type for later reference:
    set_run_check( 0, left );
    set_run_check( 1, dir );
    set_run_check( 2, right );
}

// Returns true if one of the front three grids has changed.
static bool check_stop_running( void )
{
    if (env.cgrid[you.x_pos + you.run_x][you.y_pos + you.run_y] != EMPTY_CLOUD)
        return (true);

    if (mgrd[you.x_pos + you.run_x][you.y_pos + you.run_y] != NON_MONSTER)
        return (true);

    for (int i = 0; i < 3; i++)
    {
        const int targ_x = you.x_pos + you.run_check[i].dx;
        const int targ_y = you.y_pos + you.run_check[i].dy;
        const int targ_grid = base_grid_type( grd[ targ_x ][ targ_y ] );

        if (you.run_check[i].grid != targ_grid)
            return (true);
    }

    return (false);
}


/*
   This function handles the player's input. It's called from main(), from
   inside an endless loop.
 */
static void input(void)
{

    bool its_quiet;             //jmf: for silence messages
    FixedVector < int, 2 > plox;
    char move_x = 0;
    char move_y = 0;

    int keyin = 0;

#ifdef LINUX
    // Stuff for the Unix keypad kludge
    bool running = false;
    bool opening = false;
#endif

    you.shield_blocks = 0;              // no blocks this round

    you.time_taken = player_speed();

#ifdef LINUX
    update_screen();
#else
    window( 1, 1, 80, get_number_of_lines() );
#endif

    textcolor(LIGHTGREY);

    set_redraw_status( REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK );
    print_stats();

    if (you.paralysis)
    {
        keyin = '.';            // end of if you.paralysis == 0
    }
    else
    {
        handle_delay(); 

        gotoxy(18, 9);

        if (you_are_delayed())
            keyin = '.';
        else
        {

            if (you.running > 0)
            {
                keyin = 128;

                move_x = you.run_x;
                move_y = you.run_y;

                if (kbhit())
                {
                    you.running = 0;
                    goto gutch;
                }

                if (you.run_x == 0 && you.run_y == 0)
                {
                    you.running--;
                    keyin = '.';
                }
            }
            else
            {

#if DEBUG_DIAGNOSTICS
                // save hunger at start of round
                // for use with hunger "delta-meter" in  output.cc
                you.old_hunger = you.hunger;        
#endif

#if DEBUG_ITEM_SCAN
                debug_item_scan();
#endif

              gutch:
                flush_input_buffer( FLUSH_BEFORE_COMMAND );
                keyin = getch_with_command_macros();
            }

            mesclr();

#ifdef LINUX
            // Kludging running and opening as two character sequences
            // for Unix systems.  This is an easy way out... all the
            // player has to do is find a termcap and numlock setting
            // that will get curses the numbers from the keypad.  This
            // will hopefully be easy.

            if (keyin == '*')
            {
                opening = true;
                keyin = getch();
            }
            else if (keyin == '/')
            {
                running = true;
                keyin = getch();
            }

            // Translate keypad codes into command enums
            keyin = key_to_command(keyin);
#else
            // Old DOS keypad support
            if (keyin == 0)     // ALT also works - see ..\KEYTEST.CPP
            {
                keyin = getch();
                switch (keyin)
                {
                case 'O': move_x = -1; move_y =  1; break;
                case 'P': move_x =  0; move_y =  1; break;
                case 'I': move_x =  1; move_y = -1; break;
                case 'H': move_x =  0; move_y = -1; break;
                case 'G': move_x = -1; move_y = -1; break;
                case 'K': move_x = -1; move_y =  0; break;
                case 'Q': move_x =  1; move_y =  1; break;
                case 'M': move_x =  1; move_y =  0; break;

                case 119: open_door(-1, -1); move_x = 0; move_y = 0; break;
                case 141: open_door( 0, -1); move_x = 0; move_y = 0; break;
                case 132: open_door( 1, -1); move_x = 0; move_y = 0; break;
                case 116: open_door( 1,  0); move_x = 0; move_y = 0; break;
                case 118: open_door( 1,  1); move_x = 0; move_y = 0; break;
                case 145: open_door( 0,  1); move_x = 0; move_y = 0; break;
                case 117: open_door(-1,  1); move_x = 0; move_y = 0; break;
                case 115: open_door(-1,  0); move_x = 0; move_y = 0; break;

                case 76:
                case 'S':
                    keyin = '.';
                    goto get_keyin_again;
                }

                keyin = 128;
            }
#endif
        }
    }

    if (keyin != 128)
    {
        move_x = 0;
        move_y = 0;
        you.turn_is_over = 0;
    }

#ifndef LINUX
  get_keyin_again:
#endif //jmf: just stops an annoying gcc warning



    switch (keyin)
    {
    case CONTROL('Y'):
    case CMD_OPEN_DOOR_UP_RIGHT:
        open_door(-1, -1); move_x = 0; move_y = 0; break;

    case CONTROL('K'):
    case CMD_OPEN_DOOR_UP:
        open_door( 0, -1); move_x = 0; move_y = 0; break;

    case CONTROL('U'):
    case CMD_OPEN_DOOR_UP_LEFT:
        open_door( 1, -1); move_x = 0; move_y = 0; break;

    case CONTROL('L'):
    case CMD_OPEN_DOOR_RIGHT:
        open_door( 1,  0); move_x = 0; move_y = 0; break;

    case CONTROL('N'):
    case CMD_OPEN_DOOR_DOWN_RIGHT:
        open_door( 1,  1); move_x = 0; move_y = 0; break;

    case CONTROL('J'):
    case CMD_OPEN_DOOR_DOWN:
        open_door( 0,  1); move_x = 0; move_y = 0; break;

    case CONTROL('B'):
    case CMD_OPEN_DOOR_DOWN_LEFT:
        open_door(-1,  1); move_x = 0; move_y = 0; break;

    case CONTROL('H'):
    case CMD_OPEN_DOOR_LEFT:
        open_door(-1,  0); move_x = 0; move_y = 0; break;

    case 'b': case CMD_MOVE_DOWN_LEFT:  move_x = -1; move_y =  1; break;
    case 'j': case CMD_MOVE_DOWN:       move_y =  1; move_x =  0; break;
    case 'u': case CMD_MOVE_UP_RIGHT:   move_x =  1; move_y = -1; break;
    case 'k': case CMD_MOVE_UP:         move_y = -1; move_x =  0; break;
    case 'y': case CMD_MOVE_UP_LEFT:    move_y = -1; move_x = -1; break;
    case 'h': case CMD_MOVE_LEFT:       move_x = -1; move_y =  0; break;
    case 'n': case CMD_MOVE_DOWN_RIGHT: move_y =  1; move_x =  1; break;
    case 'l': case CMD_MOVE_RIGHT:      move_x =  1; move_y =  0; break;

    case CMD_REST:
        // Yes this is backwards, but everyone here is used to using
        // straight 5s for long rests... might need to define a roguelike
        // rest key and get people switched over. -- bwr

#ifdef LINUX
        if (!running && !opening)
            start_running( RDIR_REST, 100 );
        else
        {
            search_around();
            move_x = 0;
            move_y = 0;
            you.turn_is_over = 1;
        }
#endif
        break;

    case 'B': case CMD_RUN_DOWN_LEFT:   
        start_running( RDIR_DOWN_LEFT, 2 ); break;

    case 'J': case CMD_RUN_DOWN:        
        start_running( RDIR_DOWN, 2 ); break;

    case 'U': case CMD_RUN_UP_RIGHT:    
        start_running( RDIR_UP_RIGHT, 2 ); break;

    case 'K': case CMD_RUN_UP:          
        start_running( RDIR_UP, 2 ); break;

    case 'Y': case CMD_RUN_UP_LEFT:     
        start_running( RDIR_UP_LEFT, 2 ); break;

    case 'H': case CMD_RUN_LEFT:        
        start_running( RDIR_LEFT, 2 ); break;

    case 'N': case CMD_RUN_DOWN_RIGHT:  
        start_running( RDIR_DOWN_RIGHT, 2 ); break;

    case 'L': case CMD_RUN_RIGHT:       
        start_running( RDIR_RIGHT, 2 ); break;

#ifdef LINUX
        // taken care of via key -> command mapping
#else
        // Old DOS keypad support
    case '1': start_running( RDIR_DOWN_LEFT, 2 ); break;
    case '2': start_running( RDIR_DOWN, 2 ); break;
    case '9': start_running( RDIR_UP_RIGHT, 2 ); break;
    case '8': start_running( RDIR_UP, 2 ); break;
    case '7': start_running( RDIR_UP_LEFT, 2 ); break;
    case '4': start_running( RDIR_LEFT, 2 ); break;
    case '3': start_running( RDIR_DOWN_RIGHT, 2 ); break;
    case '6': start_running( RDIR_RIGHT, 2 ); break;
    case '5': start_running( RDIR_REST, 100 ); break;

#endif

    case CONTROL('A'):
    case CMD_TOGGLE_AUTOPICKUP:
        autopickup_on = !autopickup_on;
        strcpy(info, "Autopickup is now ");
        strcat(info, (autopickup_on) ? "on" : "off");
        strcat(info, ".");
        mpr(info);
        break;

    case '<':
    case CMD_GO_UPSTAIRS:
        if (grd[you.x_pos][you.y_pos] == DNGN_ENTER_SHOP)
        {   
            shop();
            break;
        }
        else if ((grd[you.x_pos][you.y_pos] < DNGN_STONE_STAIRS_UP_I
                    || grd[you.x_pos][you.y_pos] > DNGN_ROCK_STAIRS_UP)
                && (grd[you.x_pos][you.y_pos] < DNGN_RETURN_FROM_ORCISH_MINES 
                    || grd[you.x_pos][you.y_pos] >= 150))
        {   
            mpr( "You can't go up here!" );
            break;
        }

        tag_followers();  // only those beside us right now can follow
        start_delay( DELAY_ASCENDING_STAIRS, 
                     1 + (you.burden_state > BS_UNENCUMBERED) );
        break;

    case '>':
    case CMD_GO_DOWNSTAIRS:
        if ((grd[you.x_pos][you.y_pos] < DNGN_ENTER_LABYRINTH
                || grd[you.x_pos][you.y_pos] > DNGN_ROCK_STAIRS_DOWN)
            && grd[you.x_pos][you.y_pos] != DNGN_ENTER_HELL
            && ((grd[you.x_pos][you.y_pos] < DNGN_ENTER_DIS
                    || grd[you.x_pos][you.y_pos] > DNGN_TRANSIT_PANDEMONIUM)
                && grd[you.x_pos][you.y_pos] != DNGN_STONE_ARCH)
            && !(grd[you.x_pos][you.y_pos] >= DNGN_ENTER_ORCISH_MINES
                && grd[you.x_pos][you.y_pos] < DNGN_RETURN_FROM_ORCISH_MINES))
        {
            mpr( "You can't go down here!" );
            break;
        }

        tag_followers();  // only those beside us right now can follow
        start_delay( DELAY_DESCENDING_STAIRS,
                     1 + (you.burden_state > BS_UNENCUMBERED),
                     you.your_level );
        break;

    case 'O':
    case CMD_DISPLAY_OVERMAP:
        display_overmap();
        break;

    case 'o':
    case CMD_OPEN_DOOR:
        open_door(0, 0);
        break;
    case 'c':
    case CMD_CLOSE_DOOR:
        close_door(0, 0);
        break;

    case 'd':
    case CMD_DROP:
        drop();
        break;

    case 'D':
    case CMD_BUTCHER:
        butchery();
        break;

    case 'i':
    case CMD_DISPLAY_INVENTORY:
        get_invent(-1);
        break;

    case 'I':
    case CMD_OBSOLETE_INVOKE:
        // We'll leave this message in for a while.  Eventually, this
        // might be some special for of inventory command, or perhaps
        // actual god invocations will be split to here from abilities. -- bwr
        mpr( "This command is now 'E'voke wielded item.", MSGCH_WARN );
        break;
    
    case 'E':
    case CMD_EVOKE:
        if (!evoke_wielded())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case 'g':
    case ',':
    case CMD_PICKUP:
        pickup();
        break;

    case ';':
    case CMD_INSPECT_FLOOR:
        item_check(';');
        break;

    case 'w':
    case CMD_WIELD_WEAPON:
        wield_weapon(false);
        break;

    case 't':
    case CMD_THROW:
        throw_anything();
        break;

    case 'f':
    case CMD_FIRE:
        shoot_thing();
        break;

    case 'W':
    case CMD_WEAR_ARMOUR:
        wear_armour();
        break;

    case 'T':
    case CMD_REMOVE_ARMOUR:
        {
            int index=0;

            if (armour_prompt("Take off which item?", &index))
                takeoff_armour(index);
        }
        break;

    case 'R':
    case CMD_REMOVE_JEWELLERY:
        remove_ring();
        break;
    case 'P':
    case CMD_WEAR_JEWELLERY:
        puton_ring();
        break;

    case '=':
    case CMD_ADJUST_INVENTORY:
        adjust();
        return;

    case 'M':
    case CMD_MEMORISE_SPELL:
        if (!learn_spell())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case 'z':
    case CMD_ZAP_WAND:
        zap_wand();
        break;

    case 'e':
    case CMD_EAT:
        eat_food();
        break;

    case 'a':
    case CMD_USE_ABILITY:
        if (!activate_ability())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case 'A':
    case CMD_DISPLAY_MUTATIONS:
        display_mutations();
        redraw_screen();
        break;

    case 'v':
    case CMD_EXAMINE_OBJECT:
        original_name();
        break;

    case 'p':
    case CMD_PRAY:
        pray();
        break;

    case '^':
    case CMD_DISPLAY_RELIGION:
        describe_god( you.religion, true );
        redraw_screen();
        break;

    case '.':
    case CMD_MOVE_NOWHERE:
        search_around();
        move_x = 0;
        move_y = 0;
        you.turn_is_over = 1;
        break;

    case 'q':
    case CMD_QUAFF:
        drink();
        break;

    case 'r':
    case CMD_READ:
        read_scroll();
        break;

    case 'x':
    case CMD_LOOK_AROUND:
        mpr("Move the cursor around to observe a square.", MSGCH_PROMPT);
        mpr("Press '?' for a monster description.", MSGCH_PROMPT);

        struct dist lmove;
        look_around( lmove, true );
        break;

    case 's':
    case CMD_SEARCH:
        search_around();
        you.turn_is_over = 1;
        break;

    case 'Z':
    case CMD_CAST_SPELL:
        /* randart wpns */
        if (scan_randarts(RAP_PREVENT_SPELLCASTING))
        {
            mpr("Something interferes with your magic!");
            flush_input_buffer( FLUSH_ON_FAILURE );
            break;
        }

        if (!cast_a_spell())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case '\'':
    case CMD_WEAPON_SWAP:
        wield_weapon(true);
        break;

    case 'X':
    case CMD_DISPLAY_MAP:
#if (!DEBUG_DIAGNOSTICS)
        if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        {
            mpr("It would help if you knew where you were, first.");
            break;
        }
#endif
        plox[0] = 0;
        show_map(plox);
        redraw_screen();
        break;

    case '\\':
    case CMD_DISPLAY_KNOWN_OBJECTS:
        check_item_knowledge(); //nothing = check_item_knowledge();
        redraw_screen();
        break;

#ifdef ALLOW_DESTROY_ITEM_COMMAND
    case CONTROL('D'):
    case CMD_DESTROY_ITEM:
        cmd_destroy_item();
        break;
#endif

    case CONTROL('P'):
    case CMD_REPLAY_MESSAGES:
        replay_messages();
        redraw_screen();
        break;

    case CONTROL('R'):
    case CMD_REDRAW_SCREEN:
        redraw_screen();
        break;

    case CONTROL('X'):
    case CMD_SAVE_GAME_NOW:
        mpr("Saving game... please wait.");
        save_game(true);
        break;

#ifdef USE_UNIX_SIGNALS
    case CONTROL('Z'):
    case CMD_SUSPEND_GAME:
        // CTRL-Z suspend behaviour is implemented here,
        // because we want to have CTRL-Y available...
        // and unfortuantely they tend to be stuck together. 
        clrscr();
        lincurses_shutdown();
        kill(0, SIGTSTP);
        lincurses_startup();
        redraw_screen();
        break;
#endif

    case '?':
    case CMD_DISPLAY_COMMANDS:
        list_commands(false);
        redraw_screen();
        break;

    case 'C':
    case CMD_EXPERIENCE_CHECK:
        snprintf( info, INFO_SIZE, "You are a level %d %s %s.", you.experience_level,
                species_name(you.species,you.experience_level), you.class_name);
        mpr(info);

        if (you.experience_level < 27)
        {
            int xp_needed = (exp_needed(you.experience_level+2) - you.experience) + 1;
            snprintf( info, INFO_SIZE, "Level %d requires %ld experience (%d point%s to go!)",
                    you.experience_level + 1, 
                    exp_needed(you.experience_level + 2) + 1,
                    xp_needed, 
                    (xp_needed > 1) ? "s" : "");
            mpr(info);
        }
        else
        {
            mpr( "I'm sorry, level 27 is as high as you can go." );
            mpr( "With the way you've been playing, I'm surprised you got this far." );
        }

        if (you.real_time != -1)
        {
            const time_t curr = you.real_time + (time(NULL) - you.start_time);
            char buff[200];

            make_time_string( curr, buff, sizeof(buff) );

            snprintf( info, INFO_SIZE, "Play time: %s (%ld turns)", 
                      buff, you.num_turns );

            mpr( info );
        }
        break;


    case '!':
    case CMD_SHOUT:
        yell();                 /* in effects.cc */
        break;

    case '@':
    case CMD_DISPLAY_CHARACTER_STATUS:
        display_char_status();
        break;

    case 'm':
    case CMD_DISPLAY_SKILLS:
        show_skills();
        redraw_screen();
        break;

    case '#':
    case CMD_CHARACTER_DUMP:
        char name_your[kNameLen+1];

        strncpy(name_your, you.your_name, kNameLen);
        name_your[kNameLen] = '\0';
        if (dump_char( name_your, false ))
            strcpy(info, "Char dumped successfully.");
        else
            strcat(info, "Char dump unsuccessful! Sorry about that.");
        mpr(info);
        break;

#ifdef USE_MACROS
    case '`':
    case CMD_MACRO_ADD:
        macro_add_query();
        break;
    case '~':
    case CMD_MACRO_SAVE:
        mpr("Saving macros.");
        macro_save();
        break;
#endif

    case ')':
    case CMD_LIST_WEAPONS:
        list_weapons();
        return;

    case ']':
    case CMD_LIST_ARMOUR:
        list_armour();
        return;

    case '"':
    case CMD_LIST_JEWELLERY:
        list_jewellery();
        return;

#ifdef WIZARD
    case CONTROL('W'):
    case CMD_WIZARD:
    case '&':
        handle_wizard_command();
        break;
#endif

    case 'S':
    case CMD_SAVE_GAME:
        if (yesno("Save game and exit?", false))
            save_game(true);
        break;

    case 'Q':
    case CMD_QUIT:
        quit_game();
        break;

    case 'V':
    case CMD_GET_VERSION:
        version();
        break;

    case 128:   // Can't use this char -- it's the special value 128
        break;

    default:
    case CMD_NO_CMD:
        mpr("Unknown command.");
        break;

    }

#ifdef LINUX
    // New Unix keypad stuff
    if (running)
    {
        int dir = -1;
        
        // XXX: ugly hack to interface this with the new running code. -- bwr
        for (int i = 0; i < 8; i++)
        {
            if (Compass[i].x == move_x && Compass[i].y == move_y)
            {
                dir = i;
                break;
            }
        }

        if (dir != -1)
        {
            start_running( dir, 2 );
            move_x = 0;
            move_y = 0;
        }
    }
    else if (opening)
    {
        open_door(move_x, move_y);
        move_x = 0;
        move_y = 0;
    }
#endif

    if (move_x != 0 || move_y != 0)
        move_player(move_x, move_y);
    else if (you.turn_is_over)      // we did something other than move/attack
        do_berserk_no_combat_penalty();

    if (!you.turn_is_over)
    {
        viewwindow(1, false);
        return;
    }

    if (you.num_turns != -1)
        you.num_turns++;

    //if (random2(10) < you.skills [SK_TRAPS_DOORS] + 2) search_around();

    stealth = check_stealth();

#if 0
    // too annoying for regular diagnostics
    snprintf( info, INFO_SIZE, "stealth: %d", stealth );
    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (you.special_wield != SPWLD_NONE)
        special_wielded();

    if (one_chance_in(10))
    {   
        // this is instantaneous
        if (player_teleport() > 0 && one_chance_in(100 / player_teleport()))
            you_teleport2( true ); 
        else if (you.level_type == LEVEL_ABYSS && one_chance_in(30))
            you_teleport2( false, true ); // to new area of the Abyss
    }

    if (env.cgrid[you.x_pos][you.y_pos] != EMPTY_CLOUD)
        in_a_cloud();

    if (you.duration[DUR_REPEL_UNDEAD] > 1)
        you.duration[DUR_REPEL_UNDEAD]--;

    if (you.duration[DUR_REPEL_UNDEAD] == 4)
    {
        mpr( "Your holy aura is starting to fade.", MSGCH_DURATION );
        you.duration[DUR_REPEL_UNDEAD] -= random2(3);
    }

    if (you.duration[DUR_REPEL_UNDEAD] == 1)
    {
        mpr( "Your holy aura fades away.", MSGCH_DURATION );
        you.duration[DUR_REPEL_UNDEAD] = 0;
    }

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
        scrolls_burn(8, OBJ_SCROLLS);

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
            you.redraw_armour_class = 1;
        }
    }

    if (you.duration[DUR_ICY_ARMOUR] > 1)
    {
        you.duration[DUR_ICY_ARMOUR]--;
        //scrolls_burn(4, OBJ_POTIONS);
    }
    else if (you.duration[DUR_ICY_ARMOUR] == 1)
    {
        mpr("Your icy armour evaporates.", MSGCH_DURATION);
        you.redraw_armour_class = 1;     // is this needed? 2apr2000 {dlb}
        you.duration[DUR_ICY_ARMOUR] = 0;
    }

    if (you.duration[DUR_REPEL_MISSILES] > 1)
    {
        you.duration[DUR_REPEL_MISSILES]--;
        if (you.duration[DUR_REPEL_MISSILES] == 6)
        {
            mpr("Your repel missiles spell is about to expire...", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_REPEL_MISSILES]--;
        }
    }
    else if (you.duration[DUR_REPEL_MISSILES] == 1)
    {
        mpr("You feel less protected from missiles.", MSGCH_DURATION);
        you.duration[DUR_REPEL_MISSILES] = 0;
    }

    if (you.duration[DUR_DEFLECT_MISSILES] > 1)
    {
        you.duration[DUR_DEFLECT_MISSILES]--;
        if (you.duration[DUR_DEFLECT_MISSILES] == 6)
        {
            mpr("Your deflect missiles spell is about to expire...", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_DEFLECT_MISSILES]--;
        }
    }
    else if (you.duration[DUR_DEFLECT_MISSILES] == 1)
    {
        mpr("You feel less protected from missiles.", MSGCH_DURATION);
        you.duration[DUR_DEFLECT_MISSILES] = 0;
    }

    if (you.duration[DUR_REGENERATION] > 1)
    {
        you.duration[DUR_REGENERATION]--;

        if (you.duration[DUR_REGENERATION] == 6)
        {
            mpr("Your skin is crawling a little less now.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_REGENERATION]--;
        }
    }
    else if (you.duration[DUR_REGENERATION] == 1)
    {
        mpr("Your skin stops crawling.", MSGCH_DURATION);
        you.duration[DUR_REGENERATION] = 0;
    }

    if (you.duration[DUR_PRAYER] > 1)
        you.duration[DUR_PRAYER]--;
    else if (you.duration[DUR_PRAYER] == 1)
    {
        god_speaks(you.religion, "Your prayer is over.");
        you.duration[DUR_PRAYER] = 0;
    }


    //jmf: more flexible weapon branding code
    if (you.duration[DUR_WEAPON_BRAND] > 1)
        you.duration[DUR_WEAPON_BRAND]--;
    else if (you.duration[DUR_WEAPON_BRAND] == 1)
    {
        const int wpn = you.equip[EQ_WEAPON];
        const int temp_effect = get_weapon_brand( you.inv[wpn] );

        you.duration[DUR_WEAPON_BRAND] = 0;

        char str_pass[ITEMNAME_SIZE];

        set_item_ego_type( you.inv[wpn], OBJ_WEAPONS, SPWPN_NORMAL );
        in_name(wpn, DESC_CAP_YOUR, str_pass);
        strncpy(info, str_pass, INFO_SIZE);

        switch (temp_effect)
        {
        case SPWPN_VORPAL:
            if (damage_type(you.inv[you.equip[EQ_WEAPON]].base_type,
                     you.inv[you.equip[EQ_WEAPON]].sub_type) != DVORP_CRUSHING)
            {
                strcat(info, " seems blunter.");
            }
            else
            {
                //jmf: for Maxwell's Silver Hammer
                strcat(info, " feels lighter.");
            }
            break;

        case SPWPN_FLAMING:
            strcat(info, " goes out.");
            break;
        case SPWPN_FREEZING:
            strcat(info, " stops glowing.");
            break;
        case SPWPN_VENOM:
            strcat(info, " stops dripping with poison.");
            break;
        case SPWPN_DRAINING:
            strcat(info, " stops crackling.");
            break;
        case SPWPN_DISTORTION:
            strcat( info, " seems straighter." );
            miscast_effect( SPTYP_TRANSLOCATION, 9, 90, 100, "a distortion effect" );
            break;
        default:
            strcat(info, " seems inexplicably less special.");
            break;
        }

        //you.attribute[ATTR_WEAPON_BRAND] = 0;
        mpr(info, MSGCH_DURATION);
        you.wield_change = true;
    }

    if (you.duration[DUR_BREATH_WEAPON] > 1)
        you.duration[DUR_BREATH_WEAPON]--;
    else if (you.duration[DUR_BREATH_WEAPON] == 1)
    {
        mpr("You have got your breath back.", MSGCH_RECOVERY);
        you.duration[DUR_BREATH_WEAPON] = 0;
    }

    if (you.duration[DUR_TRANSFORMATION] > 1)
    {
        you.duration[DUR_TRANSFORMATION]--;

        if (you.duration[DUR_TRANSFORMATION] == 10)
        {
            mpr("Your transformation is almost over.", MSGCH_DURATION);
            you.duration[DUR_TRANSFORMATION] -= random2(3);
        }
    }
    else if (you.duration[DUR_TRANSFORMATION] == 1)
    {
        untransform();
        you.duration[DUR_BREATH_WEAPON] = 0;
    }

    if (you.duration[DUR_SWIFTNESS] > 1)
    {
        you.duration[DUR_SWIFTNESS]--;
        if (you.duration[DUR_SWIFTNESS] == 6)
        {
            mpr("You start to feel a little slower.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_SWIFTNESS]--;
        }
    }
    else if (you.duration[DUR_SWIFTNESS] == 1)
    {
        mpr("You feel sluggish.", MSGCH_DURATION);
        you.duration[DUR_SWIFTNESS] = 0;
    }

    if (you.duration[DUR_INSULATION] > 1)
    {
        you.duration[DUR_INSULATION]--;
        if (you.duration[DUR_INSULATION] == 6)
        {
            mpr("You start to feel a little less insulated.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_INSULATION]--;
        }
    }
    else if (you.duration[DUR_INSULATION] == 1)
    {
        mpr("You feel conductive.", MSGCH_DURATION);
        you.duration[DUR_INSULATION] = 0;
    }

    if (you.duration[DUR_STONEMAIL] > 1)
    {
        you.duration[DUR_STONEMAIL]--;
        if (you.duration[DUR_STONEMAIL] == 6)
        {
            mpr("Your scaley stone armour is starting to flake away.", MSGCH_DURATION);
            you.redraw_armour_class = 1;
            if (coinflip())
                you.duration[DUR_STONEMAIL]--;
        }
    }
    else if (you.duration[DUR_STONEMAIL] == 1)
    {
        mpr("Your scaley stone armour disappears.", MSGCH_DURATION);
        you.duration[DUR_STONEMAIL] = 0;
        you.redraw_armour_class = 1;
        burden_change();
    }

    if (you.duration[DUR_FORESCRY] > 1) //jmf: added
        you.duration[DUR_FORESCRY]--;
    else if (you.duration[DUR_FORESCRY] == 1)
    {
        mpr("You feel firmly rooted in the present.", MSGCH_DURATION);
        you.duration[DUR_FORESCRY] = 0;
        you.redraw_evasion = 1; 
    }

    if (you.duration[DUR_SEE_INVISIBLE] > 1)    //jmf: added
        you.duration[DUR_SEE_INVISIBLE]--;
    else if (you.duration[DUR_SEE_INVISIBLE] == 1)
    {
        you.duration[DUR_SEE_INVISIBLE] = 0;

        if (!player_see_invis())
            mpr("Your eyesight blurs momentarily.", MSGCH_DURATION);
    }

    if (you.duration[DUR_SILENCE] > 0)  //jmf: cute message handled elsewhere
        you.duration[DUR_SILENCE]--;

    if (you.duration[DUR_CONDENSATION_SHIELD] > 1)
    {
        you.duration[DUR_CONDENSATION_SHIELD]--;

        scrolls_burn( 1, OBJ_POTIONS );
        
        if (player_res_cold() < 0)
        {
            mpr( "You feel very cold." );
            ouch( 2 + random2avg(13, 2), 0, KILLED_BY_FREEZING );
        }
    }
    else if (you.duration[DUR_CONDENSATION_SHIELD] == 1)
    {
        you.duration[DUR_CONDENSATION_SHIELD] = 0;
        mpr("Your icy shield evaporates.", MSGCH_DURATION);
        you.redraw_armour_class = 1;
    }

    if (you.duration[DUR_STONESKIN] > 1)
        you.duration[DUR_STONESKIN]--;
    else if (you.duration[DUR_STONESKIN] == 1)
    {
        mpr("Your skin feels tender.", MSGCH_DURATION);
        you.redraw_armour_class = 1;
        you.duration[DUR_STONESKIN] = 0;
    }

    if (you.duration[DUR_GLAMOUR] > 1)  //jmf: actually GLAMOUR_RELOAD, like
        you.duration[DUR_GLAMOUR]--;    //     the breath weapon delay
    else if (you.duration[DUR_GLAMOUR] == 1)
    {
        you.duration[DUR_GLAMOUR] = 0;
        //FIXME: cute message or not?
    }

    if (you.duration[DUR_TELEPORT] > 1)
        you.duration[DUR_TELEPORT]--;
    else if (you.duration[DUR_TELEPORT] == 1)
    {
        // only to a new area of the abyss sometimes (for abyss teleports)
        you_teleport2( true, one_chance_in(5) ); 
        you.duration[DUR_TELEPORT] = 0;
    }

    if (you.duration[DUR_CONTROL_TELEPORT] > 1)
    {
        you.duration[DUR_CONTROL_TELEPORT]--;

        if (you.duration[DUR_CONTROL_TELEPORT] == 6)
        {
            mpr("You start to feel a little uncertain.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_CONTROL_TELEPORT]--;
        }
    }
    else if (you.duration[DUR_CONTROL_TELEPORT] == 1)
    {
        mpr("You feel uncertain.", MSGCH_DURATION);
        you.duration[DUR_CONTROL_TELEPORT] = 0;
        you.attribute[ATTR_CONTROL_TELEPORT]--;
    }

    if (you.duration[DUR_RESIST_POISON] > 1)
    {
        you.duration[DUR_RESIST_POISON]--;
        if (you.duration[DUR_RESIST_POISON] == 6)
        {
            mpr("Your poison resistance is about to expire.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_RESIST_POISON]--;
        }
    }
    else if (you.duration[DUR_RESIST_POISON] == 1)
    {
        mpr("Your poison resistance expires.", MSGCH_DURATION);
        you.duration[DUR_RESIST_POISON] = 0;
    }

    if (you.duration[DUR_DEATH_CHANNEL] > 1)
    {
        you.duration[DUR_DEATH_CHANNEL]--;
        if (you.duration[DUR_DEATH_CHANNEL] == 6)
        {
            mpr("Your unholy channel is weakening.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_DEATH_CHANNEL]--;
        }
    }
    else if (you.duration[DUR_DEATH_CHANNEL] == 1)
    {
        mpr("Your unholy channel expires.", MSGCH_DURATION);    // Death channel wore off
        you.duration[DUR_DEATH_CHANNEL] = 0;
    }

    if (you.duration[DUR_SHUGGOTH_SEED_RELOAD] > 0)     //jmf: added
        you.duration[DUR_SHUGGOTH_SEED_RELOAD]--;

    if (you.duration[DUR_INFECTED_SHUGGOTH_SEED] > 1)
        you.duration[DUR_INFECTED_SHUGGOTH_SEED]--;
    else if (you.duration[DUR_INFECTED_SHUGGOTH_SEED] == 1)
    {
        //jmf: use you.max_hp instead? or would that be too evil?
        you.duration[DUR_INFECTED_SHUGGOTH_SEED] = 0;
        mpr("A horrible thing bursts from your chest!", MSGCH_WARN);
        ouch(1 + you.hp / 2, 0, KILLED_BY_SHUGGOTH);
        make_shuggoth(you.x_pos, you.y_pos, 1 + you.hp / 2);
    }

    if (you.invis > 1)
    {
        you.invis--;

        if (you.invis == 6)
        {
            mpr("You flicker for a moment.", MSGCH_DURATION);
            if (coinflip())
                you.invis--;
        }
    }
    else if (you.invis == 1)
    {
        mpr("You flicker back into view.", MSGCH_DURATION);
        you.invis = 0;
    }

    if (you.conf > 0)
        reduce_confuse_player(1);

    if (you.paralysis > 1)
        you.paralysis--;
    else if (you.paralysis == 1)
    {
        mpr("You can move again.", MSGCH_DURATION);
        you.paralysis = 0;
    }

    if (you.exhausted > 1)
        you.exhausted--;
    else if (you.exhausted == 1)
    {
        mpr("You feel less fatigued.", MSGCH_DURATION);
        you.exhausted = 0;
    }

    dec_slow_player();
    dec_haste_player();

    if (you.might > 1)
        you.might--;
    else if (you.might == 1)
    {
        mpr("You feel a little less mighty now.", MSGCH_DURATION);
        you.might = 0;
        modify_stat(STAT_STRENGTH, -5, true);
    }

    if (you.berserker > 1)
        you.berserker--;
    else if (you.berserker == 1)
    {
        mpr( "You are no longer berserk.", MSGCH_DURATION );
        you.berserker = 0;

        //jmf: guilty for berserking /after/ berserk
        naughty( NAUGHTY_STIMULANTS, 6 + random2(6) );

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
        int chance = 10 + you.mutation[MUT_BERSERK] * 25 
                        + (wearing_amulet( AMU_RAGE ) ? 10 : 0)
                        + (player_has_spell( SPELL_BERSERKER_RAGE ) ? 5 : 0);

        // Note the beauty of Trog!  They get an extra save that's at
        // the very least 20% and goes up to 100%.
        if (you.berserk_penalty == NO_BERSERK_PENALTY
            || (you.religion == GOD_TROG && you.piety > random2(150))
            || !one_chance_in( chance ))
        {
            mpr("You are exhausted.");
        }
        else
        {
            mpr("You pass out from exhaustion.", MSGCH_WARN);
            you.paralysis += roll_dice( 1, 4 );
        }

        // this resets from an actual penalty or from NO_BERSERK_PENALTY
        you.berserk_penalty = 0;

        int dur = 12 + roll_dice( 2, 12 );
        you.exhausted += dur;
        slow_player( dur );

        make_hungry(700, true);

        if (you.hunger < 50)
            you.hunger = 50;

        calc_hp();
    }

    if (you.confusing_touch > 1)
        you.confusing_touch--;
    else if (you.confusing_touch == 1)
    {
        snprintf( info, INFO_SIZE, "Your %s stop glowing.", your_hand(true) );
        mpr( info, MSGCH_DURATION );
        you.confusing_touch = 0;
    }

    if (you.sure_blade > 1)
        you.sure_blade--;
    else if (you.sure_blade == 1)
    {
        mpr("The bond with your blade fades away.", MSGCH_DURATION);
        you.sure_blade = 0;
    }

    if (you.levitation > 1)
    {
        if (you.species != SP_KENKU || you.experience_level < 15)
            you.levitation--;

        if (player_equip_ego_type( EQ_BOOTS, SPARM_LEVITATION ))
            you.levitation = 2;

        if (you.levitation == 10)
        {
            mpr("You are starting to lose your buoyancy!", MSGCH_DURATION);
            you.levitation -= random2(6);

            if (you.duration[DUR_CONTROLLED_FLIGHT] > 0)
                you.duration[DUR_CONTROLLED_FLIGHT] = you.levitation;
        }
    }
    else if (you.levitation == 1)
    {
        mpr("You float gracefully downwards.", MSGCH_DURATION);
        you.levitation = 0;
        burden_change();
        you.duration[DUR_CONTROLLED_FLIGHT] = 0;

        if (grd[you.x_pos][you.y_pos] == DNGN_LAVA
            || grd[you.x_pos][you.y_pos] == DNGN_DEEP_WATER
            || grd[you.x_pos][you.y_pos] == DNGN_SHALLOW_WATER)
        {
            if (you.species == SP_MERFOLK 
                && grd[you.x_pos][you.y_pos] != DNGN_LAVA)
            {
                mpr("You dive into the water and return to your normal form.");
                merfolk_start_swimming();
            }

            if (grd[you.x_pos][you.y_pos] != DNGN_SHALLOW_WATER)
                fall_into_a_pool(true, grd[you.x_pos][you.y_pos]);
        }
    }

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

    if (you.poison > 0)
    {
        if (random2(5) <= (you.poison - 1))
        {
            if (you.poison > 10 && random2(you.poison) >= 8)
            {
                ouch(random2(10) + 5, 0, KILLED_BY_POISON);
                mpr("You feel extremely sick.", MSGCH_DANGER);
            }
            else if (you.poison > 5 && coinflip())
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

    if (you.deaths_door)
    {
        if (you.hp > allowed_deaths_door_hp())
        {
            mpr("Your life is in your own hands once again.", MSGCH_DURATION);
            you.paralysis += 5 + random2(5);
            confuse_player( 10 + random2(10) );
            you.hp_max--;
            deflate_hp(you.hp_max, false);
            you.deaths_door = 0;
        }
        else
            you.deaths_door--;

        if (you.deaths_door == 10)
        {
            mpr("Your time is quickly running out!", MSGCH_DURATION);
            you.deaths_door -= random2(6);
        }
        if (you.deaths_door == 1)
        {
            mpr("Your life is in your own hands again!", MSGCH_DURATION);
            more();
        }
    }

    const int food_use = player_hunger_rate();

    if (food_use > 0 && you.hunger >= 40)
        make_hungry( food_use, true );

    // XXX: using an int tmp to fix the fact that hit_points_regeneration
    // is only an unsigned char and is thus likely to overflow. -- bwr
    int tmp = static_cast< int >( you.hit_points_regeneration );

    if (you.hp < you.hp_max && !you.disease && !you.deaths_door)
        tmp += player_regen();

    while (tmp >= 100)
    {
        if (you.hp >= you.hp_max - 1 
            && you.running && you.run_x == 0 && you.run_y == 0)
        {
            you.running = 0;
        }

        inc_hp(1, false);
        tmp -= 100;
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
        if (you.magic_points >= you.max_magic_points - 1 
            && you.running && you.run_x == 0 && you.run_y == 0)
        {
            you.running = 0;
        }

        inc_mp(1, false);
        tmp -= 100;
    }

    ASSERT( tmp >= 0 && tmp < 100 );
    you.magic_points_regeneration = static_cast< unsigned char >( tmp );

    viewwindow(1, true);

    handle_monsters();

    ASSERT(you.time_taken >= 0);
    // make sure we don't overflow
    ASSERT(DBL_MAX - you.elapsed_time > you.time_taken);

    you.elapsed_time += you.time_taken;

    if (you.synch_time <= you.time_taken)
    {
        handle_time(200 + (you.time_taken - you.synch_time));
        you.synch_time = 200;
    }
    else
    {
        you.synch_time -= you.time_taken;
    }

    manage_clouds();

    if (you.fire_shield > 0)
        manage_fire_shield();

    // There used to be signs of intent to have statues as some sort
    // of more complex state machine... I'm boiling them down to bare
    // basics for now.  -- bwr
    if (Visible_Statue[ STATUE_SILVER ])
    {
        if ((!you.invis && one_chance_in(3)) || one_chance_in(5))
        {
            char wc[30];

            weird_colours( random2(256), wc );
            snprintf(info, INFO_SIZE, "The silver statue's eyes glow %s.", wc);
            mpr( info, MSGCH_WARN );

            create_monster( summon_any_demon((coinflip() ? DEMON_COMMON
                                                         : DEMON_LESSER)),
                                     ENCH_ABJ_V, BEH_HOSTILE,
                                     you.x_pos, you.y_pos,
                                     MHITYOU, 250 );
        }

        Visible_Statue[ STATUE_SILVER ] = 0;
    }

    if (Visible_Statue[ STATUE_ORANGE_CRYSTAL ])
    {
        if ((!you.invis && coinflip()) || one_chance_in(4))
        {
            mpr("A hostile presence attacks your mind!", MSGCH_WARN);

            miscast_effect( SPTYP_DIVINATION, random2(15), random2(150), 100,
                            "an orange crystal statue" );
        }

        Visible_Statue[ STATUE_ORANGE_CRYSTAL ] = 0;
    }

    // food death check:
    if (you.is_undead != US_UNDEAD && you.hunger <= 500)
    {
        if (!you.paralysis && one_chance_in(40))
        {
            mpr("You lose consciousness!", MSGCH_FOOD);
            you.paralysis += 5 + random2(8);

            if (you.paralysis > 13)
                you.paralysis = 13;
        }

        if (you.hunger <= 100)
        {
            mpr( "You have starved to death.", MSGCH_FOOD );
            ouch( -9999, 0, KILLED_BY_STARVATION );
        }
    }

    //jmf: added silence messages
    its_quiet = silenced(you.x_pos, you.y_pos);

    if (you.attribute[ATTR_WAS_SILENCED] != its_quiet)
    {
        if (its_quiet)
        {
            if (random2(30))
                mpr("You are enveloped in profound silence.", MSGCH_WARN);
            else
                mpr("The dungeon seems quiet ... too quiet!", MSGCH_WARN);
        }
        else
        {
            mpr("Your hearing returns.", MSGCH_RECOVERY);
        }

        you.attribute[ATTR_WAS_SILENCED] = its_quiet;
    }

    viewwindow(1, false);

    if (you.paralysis > 0 && any_messages())
        more();

    // place normal dungeon monsters,  but not in player LOS
    if (you.level_type == LEVEL_DUNGEON
        && !player_in_branch( BRANCH_ECUMENICAL_TEMPLE )
        && one_chance_in((you.char_direction == DIR_DESCENDING) ? 240 : 10))
    {
        int prox = (one_chance_in(10) ? PROX_NEAR_STAIRS 
                                      : PROX_AWAY_FROM_PLAYER);

        // The rules change once the player has picked up the Orb...
        if (you.char_direction == DIR_ASCENDING)
            prox = (one_chance_in(10) ? PROX_CLOSE_TO_PLAYER : PROX_ANYWHERE);

        mons_place( WANDERING_MONSTER, BEH_HOSTILE, MHITNOT, false,
                    50, 50, LEVEL_DUNGEON, prox );
    }

    // place Abyss monsters.
    if (you.level_type == LEVEL_ABYSS && one_chance_in(5))
    {
        mons_place( WANDERING_MONSTER, BEH_HOSTILE, MHITNOT, false,
                    50, 50, LEVEL_ABYSS, PROX_ANYWHERE );
    }

    // place Pandemonium monsters
    if (you.level_type == LEVEL_PANDEMONIUM && one_chance_in(50))
        pandemonium_mons();

    // No monsters in the Labyrinth,  or Ecumenical Temple
    return;
}

/*
   Opens doors and handles some aspects of untrapping. If either move_x or
   move_y are non-zero,  the pair carries a specific direction for the door
   to be opened (eg if you type ctrl - dir).
 */
static void open_door(char move_x, char move_y)
{
    struct dist door_move;
    int dx, dy;             // door x, door y

    door_move.dx = move_x;
    door_move.dy = move_y;

    if (move_x || move_y)
    {
        // convenience
        dx = you.x_pos + move_x;
        dy = you.y_pos + move_y;

        const int mon = mgrd[dx][dy];

        if (mon != NON_MONSTER && !mons_has_ench( &menv[mon], ENCH_SUBMERGED ))
        {
            you_attack(mgrd[dx][dy], true);
            you.turn_is_over = 1;

            if (you.berserk_penalty != NO_BERSERK_PENALTY)
                you.berserk_penalty = 0;

            return;
        }

        if (grd[dx][dy] >= DNGN_TRAP_MECHANICAL && grd[dx][dy] <= DNGN_TRAP_III)
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
    }

    if (grd[dx][dy] == DNGN_CLOSED_DOOR)
    {
        int skill = you.dex + (you.skills[SK_TRAPS_DOORS] + you.skills[SK_STEALTH]) / 2;

        if (one_chance_in(skill) && !silenced(you.x_pos, you.y_pos))
        {
            mpr( "As you open the door, it creaks loudly!" );
            noisy( 10, you.x_pos, you.y_pos );
        }
        else
        {
            mpr( player_is_levitating() ? "You reach down and open the door."
                                        : "You open the door." );
        }

        grd[dx][dy] = DNGN_OPEN_DOOR;
        you.turn_is_over = 1;
    }
    else
    {
        mpr("You swing at nothing.");
        make_hungry(3, true);
        you.turn_is_over = 1;
    }
}                               // end open_door()

/*
   Similar to open_door. Can you spot the difference?
 */
static void close_door(char door_x, char door_y)
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
        if (mgrd[dx][dy] != NON_MONSTER)
        {
            // Need to make sure that turn_is_over = 1 if creature is invisible
            mpr("There's a creature in the doorway!");
            door_move.dx = 0;
            door_move.dy = 0;
            return;
        }

        if (igrd[dx][dy] != NON_ITEM)
        {
            mpr("There's something blocking the doorway.");
            door_move.dx = 0;
            door_move.dy = 0;
            return;
        }

        int skill = you.dex + (you.skills[SK_TRAPS_DOORS] + you.skills[SK_STEALTH]) / 2;

        if (one_chance_in(skill) && !silenced(you.x_pos, you.y_pos))
        {
            mpr("As you close the door, it creaks loudly!");
            noisy( 10, you.x_pos, you.y_pos );
        }
        else
        {
            mpr( player_is_levitating() ? "You reach down and close the door."
                                        : "You close the door." );
        }

        grd[dx][dy] = DNGN_CLOSED_DOOR;
        you.turn_is_over = 1;
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
    bool ret;

    int i = 0, j = 0;           // counter variables {dlb}

    your_sign = '@';
    your_colour = LIGHTGREY;

    // system initialisation stuff:
    textbackground(0);

#ifdef DOS
    directvideo = 1;
#endif

#ifdef USE_EMX
    init_emx();
#endif

    srandom(time(NULL));
    srand(time(NULL));
    cf_setseed();               // required for stuff::coinflip()

    mons_init(mcolour);          // this needs to be way up top {dlb}
    init_playerspells();        // this needs to be way up top {dlb}

    clrscr();

    // init item array:
    for (i = 1; i < MAX_ITEMS; i++)
        init_item( i );

    // empty messaging string
    strcpy(info, "");

    for (i = 0; i < MAX_MONSTERS; i++)
    {
        menv[i].type = -1;
        menv[i].speed_increment = 10;
        menv[i].flags = 0;
        menv[i].behaviour = BEH_SLEEP;

        menv[i].foe = NON_MONSTER;
        menv[i].attitude = ATT_HOSTILE;

        for (j = 0; j < NUM_MON_ENCHANTS; j++)
            menv[i].enchantment[j] = ENCH_NONE;

        for (j = 0; j < NUM_MONSTER_SLOTS; j++)
            menv[i].inv[j] = NON_ITEM;

        menv[i].number = 0;
    }

    for (i = 0; i < GXM; i++)
    {
        for (j = 0; j < GYM; j++)
        {
            igrd[i][j] = NON_ITEM;
            mgrd[i][j] = NON_MONSTER;
            env.map[i][j] = 0;
        }
    }

    for (i = 0; i < 50; i++)
    {
        you.unique_creatures[i] = 0;
        you.unique_items[i] = UNIQ_NOT_EXISTS;
    }

    for (i = 0; i < NUM_STATUE_TYPES; i++)
        Visible_Statue[i] = 0;

    // initialize tag system before we try loading anything!
    tag_init();

    // sets up a new game:
    bool newc = new_game();
    ret = newc;  // newc will be mangled later so we'll take a copy --bwr

    if (!newc)
        restore_game();

    game_has_started = true;

    calc_hp();
    calc_mp();

    load( 82, (newc ? LOAD_START_GAME : LOAD_RESTART_GAME), false, 0, 
          you.where_are_you );

#if DEBUG_DIAGNOSTICS
    // Debug compiles display a lot of "hidden" information, so we auto-wiz
    you.wizard = true;
#endif

    init_properties();
    burden_change();
    make_hungry(0,true);

    you.redraw_strength = 1;
    you.redraw_intelligence = 1;
    you.redraw_dexterity = 1;
    you.redraw_armour_class = 1;
    you.redraw_evasion = 1;
    you.redraw_experience = 1;
    you.redraw_gold = 1;
    you.wield_change = true;

    you.start_time = time( NULL );      // start timer on session

    draw_border();
    new_level();

    // set vision radius to player's current vision
    setLOSRadius( you.current_vision );
    viewwindow(1, false);   // This just puts the view up for the first turn.
    item();

    return (ret);
}

// An attempt to tone down berserk a little bit. -- bwross
//
// This function does the accounting for not attacking while berserk
// This gives a triangluar number function for the additional penalty
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

    if (you.berserker)
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
        you.berserker -= you.berserk_penalty;
        if (you.berserker < 1)
            you.berserker = 1;

        you.might -= you.berserk_penalty;
        if (you.might < 1)
            you.might = 1;

        you.haste -= you.berserk_penalty;
        if (you.haste < 1)
            you.haste = 1;
    }
    return;
}                               // end do_berserk_no_combat_penalty()


// Called when the player moves by walking/running. Also calls
// attack function and trap function etc when necessary.
static void move_player(char move_x, char move_y)
{
    bool attacking = false;
    bool moving = true;         // used to prevent eventual movement (swap)

    int i;
    bool trap_known;

    if (you.conf)
    {
        if (!one_chance_in(3))
        {
            move_x = random2(3) - 1;
            move_y = random2(3) - 1;
        }

        const int new_targ_x = you.x_pos + move_x;
        const int new_targ_y = you.y_pos + move_y;
        const unsigned char new_targ_grid = grd[ new_targ_x ][ new_targ_y ];

        if (new_targ_grid < MINMOVE)
        {
            you.turn_is_over = 1;
            mpr("Ouch!");
            return;
        }

        if (new_targ_grid == DNGN_LAVA 
            && you.duration[DUR_CONDENSATION_SHIELD] > 0)
        {
            mpr("Your icy shield dissipates!", MSGCH_DURATION);
            you.duration[DUR_CONDENSATION_SHIELD] = 0;
            you.redraw_armour_class = 1;
        }

        if ((new_targ_grid == DNGN_LAVA 
                || new_targ_grid == DNGN_DEEP_WATER
                || new_targ_grid == DNGN_SHALLOW_WATER)
             && !player_is_levitating())
        {
            if (you.species == SP_MERFOLK && new_targ_grid != DNGN_LAVA)
            {
                mpr("You stumble into the water and return to your normal form.");
                merfolk_start_swimming();
            }

            if (new_targ_grid != DNGN_SHALLOW_WATER)
                fall_into_a_pool( false, new_targ_grid );

            you.turn_is_over = 1;
            do_berserk_no_combat_penalty();
            return;
        }
    }                           // end of if you.conf

    if (you.running > 0 && you.running != 2 && check_stop_running())
    {
        you.running = 0;
        move_x = 0;
        move_y = 0;
        you.turn_is_over = 0;
        return;
    }

    const int targ_x = you.x_pos + move_x;
    const int targ_y = you.y_pos + move_y;
    const unsigned char old_grid   =  grd[ you.x_pos ][ you.y_pos ];
    const unsigned char targ_grid  =  grd[ targ_x ][ targ_y ];
    const unsigned char targ_monst = mgrd[ targ_x ][ targ_y ];

    if (targ_monst != NON_MONSTER)
    {
        struct monsters *mon = &menv[targ_monst];

        if (mons_has_ench( mon, ENCH_SUBMERGED ))
            goto break_out;

        // you can swap places with a friendly monster if you
        // can see it and you're not confused
        if (mons_friendly( mon ) && player_monster_visible( mon ) && !you.conf)
        {
            if (!swap_places( mon ))
                moving = false;

            goto break_out;
        }

        you_attack( targ_monst, true );
        you.turn_is_over = 1;

        // we don't want to create a penalty if there isn't
        // supposed to be one
        if (you.berserk_penalty != NO_BERSERK_PENALTY)
            you.berserk_penalty = 0;

        attacking = true;
    }

  break_out:
    if (targ_grid == DNGN_LAVA && you.duration[DUR_CONDENSATION_SHIELD] > 0)
    {
        mpr("Your icy shield dissipates!", MSGCH_DURATION);
        you.duration[DUR_CONDENSATION_SHIELD] = 0;
        you.redraw_armour_class = 1;
    }

    // Handle dangerous tiles
    if ((targ_grid == DNGN_LAVA 
            || targ_grid == DNGN_DEEP_WATER
            || targ_grid == DNGN_SHALLOW_WATER)
        && !attacking && !player_is_levitating() && moving)
    {
        // Merfold automatically enter deep water... every other case
        // we ask for confirmation.
        if (you.species == SP_MERFOLK && targ_grid != DNGN_LAVA)
        {
            // Only mention diving if we just entering the water.
            if (!player_in_water())
            {
                mpr("You dive into the water and return to your normal form.");
                merfolk_start_swimming();
            }
        }
        else if (targ_grid != DNGN_SHALLOW_WATER)
        {
            bool enter = yesno("Do you really want to step there?", false);

            if (enter)
            {
                fall_into_a_pool( false, targ_grid );
                you.turn_is_over = 1;
                do_berserk_no_combat_penalty();
                return;
            }
            else
            {
                canned_msg(MSG_OK);
                return;
            }
        }
    }

    if (!attacking && targ_grid >= MINMOVE && moving)
    {
        if (targ_grid == DNGN_UNDISCOVERED_TRAP
                && random2(you.skills[SK_TRAPS_DOORS] + 1) > 3)
        {
            strcpy(info, "Wait a moment, ");
            strcat(info, you.your_name);
            strcat(info, "! Do you really want to step there?");
            mpr(info, MSGCH_WARN);
            more();
            you.turn_is_over = 0;

            i = trap_at_xy( targ_x, targ_y );
            if (i != -1)  
                grd[ targ_x ][ targ_y ] = trap_category(env.trap[i].type);
            return;
        }

        you.x_pos += move_x;
        you.y_pos += move_y;

        if (targ_grid == DNGN_SHALLOW_WATER && !player_is_levitating())
        {
            if (you.species != SP_MERFOLK)
            {
                if (one_chance_in(3) && !silenced(you.x_pos, you.y_pos))
                {
                    mpr("Splash!");
                    noisy( 10, you.x_pos, you.y_pos );
                }

                you.time_taken *= 13 + random2(8);
                you.time_taken /= 10;

                if (old_grid != DNGN_SHALLOW_WATER)
                {
                    mpr( "You enter the shallow water. "
                         "Moving in this stuff is going to be slow." );

                    if (you.invis)
                        mpr( "And don't expect to remain undetected." );
                }
            }
            else if (old_grid != DNGN_SHALLOW_WATER
                    && old_grid != DNGN_DEEP_WATER)
            {
                mpr("You return to your normal form as you enter the water.");
                merfolk_start_swimming();
            }
        }

        move_x = 0;
        move_y = 0;

        you.time_taken *= player_movement_speed();
        you.time_taken /= 10;

        you.turn_is_over = 1;
        item_check(0);

        if (targ_grid >= DNGN_TRAP_MECHANICAL
                                && targ_grid <= DNGN_UNDISCOVERED_TRAP)
        {
            if (targ_grid == DNGN_UNDISCOVERED_TRAP)
            {
                i = trap_at_xy(you.x_pos, you.y_pos);
                if (i != -1)
                    grd[ you.x_pos ][ you.y_pos ] = trap_category(env.trap[i].type);
                trap_known = false;
            }
            else
            {
                trap_known = true;
            }

            i = trap_at_xy( you.x_pos, you.y_pos );
            if (i != -1)
            {
                if (player_is_levitating()
                    && trap_category(env.trap[i].type) == DNGN_TRAP_MECHANICAL)
                {
                    goto out_of_traps;      // can fly over mechanical traps
                }

                handle_traps(env.trap[i].type, i, trap_known);
            }
        }                       // end of if another grd == trap
    }

  out_of_traps:
    // BCR - Easy doors single move
    if (targ_grid == DNGN_CLOSED_DOOR && Options.easy_open)
        open_door(move_x, move_y);
    else if (targ_grid <= MINMOVE)
    {
        you.running = 0;
        move_x = 0;
        move_y = 0;
        you.turn_is_over = 0;
    }

    if (you.running == 2)
        you.running = 1;

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

        snprintf( info, INFO_SIZE, "Number of items present: %d", ig2 );
        mpr( info, MSGCH_DIAGNOSTICS );

        ig2 = 0;
        for (igly = 0; igly < MAX_MONSTERS; igly++)
        {
            if (menv[igly].type != -1)
                ig2++;
        }

        snprintf( info, INFO_SIZE, "Number of monsters present: %d", ig2 );
        mpr( info, MSGCH_DIAGNOSTICS );

        snprintf( info, INFO_SIZE, "Number of clouds present: %d", env.cloud_no );
        mpr( info, MSGCH_DIAGNOSTICS );
#endif
    }

    if (!attacking)
    {
        do_berserk_no_combat_penalty();
    }
}                               // end move_player()
