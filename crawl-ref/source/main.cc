/**
 * @file
 * @brief Main entry point, event loop, and some initialization functions
**/

#include "AppHdr.h"

#include <string>
#include <algorithm>

#include <errno.h>
#ifndef TARGET_OS_WINDOWS
# ifndef __ANDROID__
#  include <langinfo.h>
# endif
#endif
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <list>
#include <sstream>
#include <iostream>

#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "abyss.h"
#include "acquire.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h"
#include "arena.h"
#include "beam.h"
#include "branch.h"
#include "chardump.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "colour.h"
#include "command.h"
#include "coord.h"
#include "coordit.h"
#include "crash.h"
#include "database.h"
#include "dbg-scan.h"
#include "dbg-util.h"
#include "delay.h"
#include "describe.h"
#include "dgn-overview.h"
#include "dgn-shoals.h"
#include "dlua.h"
#include "dungeon.h"
#include "effects.h"
#include "env.h"
#include "errors.h"
#include "map_knowledge.h"
#include "fprop.h"
#include "fight.h"
#include "files.h"
#include "fineff.h"
#include "food.h"
#include "godabil.h"
#include "godcompanions.h"
#include "godpassive.h"
#include "godprayer.h"
#include "hiscores.h"
#include "initfile.h"
#include "invent.h"
#include "item_use.h"
#include "evoke.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "luaterp.h"
#include "macro.h"
#include "makeitem.h"
#include "mapmark.h"
#include "maps.h"
#include "melee_attack.h"
#include "message.h"
#include "misc.h"
#include "mislead.h"
#include "mon-act.h"
#include "mon-abil.h"
#include "mon-cast.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "mon-transit.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "player-equip.h"
#include "player-stats.h"
#include "quiver.h"
#include "random.h"
#include "religion.h"
#include "godconduct.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "species.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "startup.h"
#include "tags.h"
#include "target.h"
#include "terrain.h"
#include "throw.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"
#include "shout.h"
#include "stash.h"
#include "uncancel.h"
#include "version.h"
#include "view.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "viewmap.h"
#include "wiz-dgn.h"
#include "wiz-fsim.h"
#include "wiz-item.h"
#include "wiz-mon.h"
#include "wiz-you.h"
#include "xom.h"
#include "zotdef.h"

#ifdef USE_TILE
 #include "tiledef-dngn.h"
 #include "tilepick.h"
#endif

#ifdef DGL_SIMPLE_MESSAGING
#include "dgl-message.h"
#endif

// ----------------------------------------------------------------------
// Globals whose construction/destruction order needs to be managed
// ----------------------------------------------------------------------

#ifdef DEBUG_GLOBALS
CLua clua;
CLua dlua;
#else
CLua clua(true);
CLua dlua(false);      // Lua interpreter for the dungeon builder.
#endif
crawl_environment env; // Requires dlua.

player you;

game_state crawl_state;

string init_file_error;    // externed in newgame.cc

char info[ INFO_SIZE ];    // messaging queue extern'd everywhere {dlb}

int stealth;               // externed in view.cc

void world_reacts();

static key_recorder repeat_again_rec;

// Clockwise, around the compass from north (same order as enum RUN_DIR)
const struct coord_def Compass[9] =
{
    coord_def(0, -1), coord_def(1, -1), coord_def(1, 0), coord_def(1, 1),
    coord_def(0, 1), coord_def(-1, 1), coord_def(-1, 0), coord_def(-1, -1),
    coord_def(0, 0)
};

// Functions in main module
static void _launch_game_loop();
NORETURN static void _launch_game();

static void _do_berserk_no_combat_penalty(void);
static void _input(void);
static void _move_player(int move_x, int move_y);
static void _move_player(coord_def move);
static int  _check_adjacent(dungeon_feature_type feat, coord_def& delta);
static void _open_door(coord_def move, bool check_confused = true);
static void _open_door(int x, int y, bool check_confused = true)
{
    _open_door(coord_def(x, y), check_confused);
}
static void _close_door(coord_def move);
static void _start_running(int dir, int mode);

static void _prep_input();
static command_type _get_next_cmd();
static keycode_type _get_next_keycode();
static command_type _keycode_to_command(keycode_type key);
static void _do_cmd_repeat();
static void _do_prev_cmd_again();
static void _update_replay_state();

static void _show_commandline_options_help();
static void _wanderer_startup_message();
static void _announce_goal_message();
static void _god_greeting_message(bool game_start);
static void _take_starting_note();
static void _startup_hints_mode();

static void _compile_time_asserts();

#ifdef WIZARD
static void _handle_wizard_command(void);
#endif

//
//  It all starts here. Some initialisations are run first, then straight
//  to new_game and then input.
//

#ifdef USE_SDL
# include <SDL_main.h>
# if defined(__GNUC__) && !defined(__clang__)
// SDL plays nasty tricks with main() (actually, _SDL_main()), which for
// Windows builds somehow fail with -fwhole-program.  Thus, exempt SDL_main()
// from this treatment.
__attribute__((externally_visible))
# endif
#endif

int main(int argc, char *argv[])
{
#ifndef __ANDROID__
# ifdef DGAMELAUNCH
    // avoid commas instead of dots, etc, on CDO
    setlocale(LC_CTYPE, "");
# else
    setlocale(LC_ALL, "");
# endif
#endif
#ifdef TARGET_OS_WINDOWS
    // No documentation about resetting this, nor about which versions of
    // Windows is required.  Previous ones can't handle writing to the console
    // outside ancient locales AT ALL via standard means.
    // Even with _O_U8TEXT, output tends to fail unless the user manually
    // switches the terminal to a truetype font.  And even that fails for
    // anything not directly in the font, above U+FFFF, or within Arabic or
    // any complex scripts.
# ifndef _O_U8TEXT
#  define _O_U8TEXT 0x40000
# endif
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
#endif
#ifdef USE_TILE_WEB
    if (strcasecmp(nl_langinfo(CODESET), "UTF-8"))
    {
        fprintf(stderr, "Webtiles require an UTF-8 locale.\n");
        exit(1);
    }
#endif
#ifdef DEBUG_GLOBALS
    real_Options = new game_options();
    real_you = new player();
    real_clua = new CLua(true);
    real_dlua = new CLua(false);
    real_crawl_state = new game_state();
    real_env = new crawl_environment();
#endif
    init_crash_handler();

    _compile_time_asserts();  // Actually, not just compile time.

    // Hardcoded initial keybindings.
    init_keybindings();

    init_element_colours();

    // Load in the system environment variables
    get_system_environment();
    init_signals();

    // Parse command line args -- look only for initfile & crawl_dir entries.
    if (!parse_args(argc, argv, true))
    {
        _show_commandline_options_help();
        return 1;
    }

    // Init monsters up front - needed to handle the mon_glyph option right.
    init_char_table(CSET_ASCII);
    init_monsters();

    // Init name cache. Currently unused, but item_glyph will need these
    // once implemented.
    init_properties();
    init_item_name_cache();

    // Read the init file.
    init_file_error = read_init_file();

    // Now parse the args again, looking for everything else.
    parse_args(argc, argv, false);

    if (Options.sc_entries != 0 || !SysEnv.scorefile.empty())
    {
        crawl_state.type = Options.game.type;
        crawl_state.map = crawl_state.sprint_map;
        hiscores_print_all(Options.sc_entries, Options.sc_format);
        return 0;
    }
    else
    {
        // Don't allow scorefile override for actual gameplay, only for
        // score listings.
        SysEnv.scorefile.clear();
    }

#ifdef USE_TILE
    if (!tiles.initialise())
        return -1;
#endif

    _launch_game_loop();
    end(0);

    return 0;
}

static void _reset_game()
{
    clrscr();
    crawl_state.type = GAME_TYPE_UNSPECIFIED;
    crawl_state.updating_scores = false;
    clear_message_store();
    macro_clear_buffers();
    transit_lists_clear();
    you.init();
    StashTrack = StashTracker();
    travel_cache = TravelCache();
    clear_level_target();
    you.clear_place_info();
    overview_clear();
    clear_message_window();
    note_list.clear();
    msg::deinitialise_mpr_streams();

#ifdef USE_TILE_LOCAL
    // [ds] Don't show the title screen again, just go back to
    // the menu.
    crawl_state.title_screen = false;
#endif
#ifdef USE_TILE
    tiles.clear_text_tags(TAG_NAMED_MONSTER);
#endif
}

static void _launch_game_loop()
{
    bool game_ended = false;
    do
    {
        try
        {
            game_ended = false;
            _launch_game();
        }
        catch (game_ended_condition &ge)
        {
            game_ended = true;
            _reset_game();
        }
        catch (ext_fail_exception &fe)
        {
            end(1, false, "%s", fe.msg.c_str());
        }
        catch(short_read_exception &E)
        {
            end(1, false, "Error: truncation inside the save file.\n");
        }
    } while (Options.restart_after_game
             && game_ended
             && !crawl_state.seen_hups);
}

NORETURN static void _launch_game()
{
    const bool game_start = startup_step();

    // Attach the macro key recorder
    remove_key_recorder(&repeat_again_rec);
    add_key_recorder(&repeat_again_rec);

    // Override some options when playing in hints mode.
    init_hints_options();

    if (!game_start && you.prev_save_version != Version::Long)
    {
        snprintf(info, INFO_SIZE, "Upgraded the game from %s to %s",
                                  you.prev_save_version.c_str(),
                                  Version::Long);
        take_note(Note(NOTE_MESSAGE, 0, 0, info));
    }

    if (!crawl_state.game_is_tutorial())
    {
        msg::stream << "<yellow>Welcome" << (game_start? "" : " back") << ", "
                    << you.your_name << " the "
                    << species_name(you.species)
                    << " " << you.class_name << ".</yellow>"
                    << endl;
    }

#ifdef USE_TILE
    viewwindow();
#endif

    if (!game_start)
        env.map_shadow = env.map_knowledge;

    if (game_start && you.char_class == JOB_WANDERER)
        _wanderer_startup_message();

    if (game_start)
       _announce_goal_message();

    _god_greeting_message(game_start);

    if (!crawl_state.game_is_tutorial())
        mpr("Press <w>?</w> for a list of commands and other information.");

    _prep_input();

    if (game_start)
    {
        // TODO: convert this to the hints mode
        if (crawl_state.game_is_hints())
            _startup_hints_mode();
        _take_starting_note();
    }
    else
        hints_load_game();

    // Catch up on any experience levels we did not assign last time. This
    // can happen if Crawl sees SIGHUP while it is waiting for the player
    // to dismiss a level-up prompt.
    level_change();

    // Initialise save game so we can recover from crashes on D:1.
    save_game_state();

#ifdef USE_TILE_WEB
    // Send initial game state before we do any UI updates
    tiles.redraw();
#endif

    run_uncancels();

    cursor_control ccon(!Options.use_fake_player_cursor);
    while (true)
        _input();
}

static void _show_commandline_options_help()
{
#if defined(TARGET_OS_WINDOWS) && defined(USE_TILE_LOCAL)
    string help;
# define puts(x) (help += x, help += '\n')
#endif

    puts("Command line options:");
    puts("  -help                 prints this list of options");
    puts("  -name <string>        character name");
    puts("  -species <arg>        preselect character species (by letter, abbreviation, or name)");
    puts("  -background <arg>     preselect character background (by letter, abbreviation, or name)");
    puts("  -plain                don't use IBM extended characters");
    puts("  -seed <num>           init the rng to a given sequence (a hex number > 0)");
    puts("  -dir <path>           crawl directory");
    puts("  -rc <file>            init file name");
    puts("  -rcdir <dir>          directory that contains (included) rc files");
    puts("  -morgue <dir>         directory to save character dumps");
    puts("  -macro <dir>          directory to save/find macro.txt");
    puts("  -version              Crawl version (and compilation info)");
    puts("  -save-version <name>  Save file version for the given player");
    puts("  -sprint               select Sprint");
    puts("  -sprint-map <name>    preselect a Sprint map");
    puts("  -tutorial             select the Tutorial");
    puts("  -zotdef               select Zot Defence");
#ifdef WIZARD
    puts("  -wizard               allow access to wizard mode");
#endif

    puts("");

    puts("Command line options override init file options, which override");
    puts("environment options (CRAWL_NAME, CRAWL_DIR, CRAWL_RC).");
    puts("");
    puts("  -extra-opt-first optname=optval");
    puts("  -extra-opt-last  optname=optval");
    puts("");
    puts("Acts as if 'optname=optval' was at the top or bottom of the init");
    puts("file.  Can be used multiple times.");
    puts("");

    puts("Highscore list options: (Can be redirected to more, etc.)");
    puts("  -scores [N]            highscore list");
    puts("  -tscores [N]           terse highscore list");
    puts("  -vscores [N]           verbose highscore list");
    puts("  -scorefile <filename>  scorefile to report on");
    puts("");
    puts("Arena options: (Stage a tournament between various monsters.)");
    puts("  -arena \"<monster list> v <monster list> arena:<arena map>\"");
#ifdef DEBUG_DIAGNOSTICS
    puts("");
    puts("  -test            run test cases in ./test");
    puts("  -script <name>   run script matching <name> in ./scripts");
#endif
    puts("");
    puts("Miscellaneous options:");
    puts("  -dump-maps       write map Lua to stderr when parsing .des files");

#if defined(TARGET_OS_WINDOWS) && defined(USE_TILE_LOCAL)
    text_popup(help, L"Dungeon Crawl command line help");
#endif
}

static void _wanderer_startup_message()
{
    int skill_levels = 0;
    for (int i = 0; i < NUM_SKILLS; ++i)
        skill_levels += you.skills[ i ];

    if (skill_levels <= 2)
    {
        // Some wanderers stand to not be able to see any of their
        // skills at the start of the game (one or two skills should be
        // easily guessed from starting equipment).  Anyway, we'll give
        // the player a message to warn them (and a reason why). - bwr
        mpr("You wake up in a daze, and can't recall much.");
    }
}

// A one-liner upon game start to mention the orb.
static void _announce_goal_message()
{
    string type = crawl_state.game_type_name();
    if (crawl_state.game_is_hints())
        type = "Hints";
    if (!type.empty())
        type = " " + type;
    mprf(MSGCH_PLAIN, "<yellow>%s</yellow>",
         getMiscString("welcome_spam" + type).c_str());
}

static void _god_greeting_message(bool game_start)
{
    if (you.religion == GOD_NO_GOD)
        return;

    string msg = god_name(you.religion);

    if (brdepth[BRANCH_ABYSS] == -1 && you.religion == GOD_LUGONU)
        msg += " welcome";
    else if (game_start)
        msg += " newgame";
    else if (you.religion == GOD_XOM)
    {
        if (you.gift_timeout <= 1)
            msg += " bored";
        else
            msg += " generic";
    }
    else
    {
        if (player_under_penance())
            msg += " penance";
        else
            msg += " welcome";
    }

    string result = getSpeakString(msg);

    if (!result.empty())
        god_speaks(you.religion, result.c_str());
}

static void _take_starting_note()
{
    ostringstream notestr;
    notestr << you.your_name << ", the "
            << species_name(you.species) << " "
            << you.class_name
            << ", began the quest for the Orb.";
    take_note(Note(NOTE_MESSAGE, 0, 0, notestr.str().c_str()));

    notestr.str("");
    notestr.clear();

#ifdef WIZARD
    if (you.wizard)
    {
        notestr << "You started the game in wizard mode.";
        take_note(Note(NOTE_MESSAGE, 0, 0, notestr.str().c_str()));

        notestr.str("");
        notestr.clear();
    }
#endif

    notestr << "HP: " << you.hp << "/" << you.hp_max
            << " MP: " << you.magic_points << "/" << you.max_magic_points;

    take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0,
                   notestr.str().c_str()));
}

static void _startup_hints_mode()
{
    // Don't allow triggering at game start.
    Hints.hints_just_triggered = true;

    msg::streams(MSGCH_TUTORIAL)
        << "Press any key to start the hints mode intro, or Escape to skip it."
        << endl;

    flush_prev_message();
    const int ch = getch_ck();
    if (!key_is_escape(ch))
        hints_starting_screen();
}

#ifdef WIZARD
static void _do_wizard_command(int wiz_command, bool silent_fail)
{
    ASSERT(you.wizard);

    switch (wiz_command)
    {
    case '?':
    {
        const int key = list_wizard_commands(true);
        _do_wizard_command(key, true);
        return;
    }

    case CONTROL('B'): you.teleport(true, false, true); break;
    case CONTROL('D'): wizard_edit_durations(); break;
    case CONTROL('E'): debug_dump_levgen(); break;
    case CONTROL('F'): wizard_fight_sim(true); break;
#ifdef DEBUG_BONES
    case CONTROL('G'): debug_ghosts(); break;
#endif
    case CONTROL('H'): wizard_set_hunger_state(); break;
    case CONTROL('I'): debug_item_statistics(); break;
    case CONTROL('K'): wizard_clear_used_vaults(); break;
    case CONTROL('L'): wizard_set_xl(); break;
    case CONTROL('M'): wizard_memorise_spec_spell(); break;
    case CONTROL('P'): wizard_transform(); break;
    case CONTROL('Q'): wizard_toggle_dprf(); break;
    case CONTROL('R'): wizard_recreate_level(); break;
    case CONTROL('S'): wizard_abyss_speed(); break;
    case CONTROL('T'): debug_terp_dlua(); break;
    case CONTROL('V'): wizard_toggle_xray_vision(); break;
    case CONTROL('X'): debug_xom_effects(); break;

    case CONTROL('C'): die("Intentional crash");

    case 'O': debug_test_explore();                  break;
    case 'S': wizard_set_skill_level();              break;
    case 'A': wizard_set_all_skills();               break;
    case 'a': acquirement(OBJ_RANDOM, AQ_WIZMODE);   break;
    case 'v': wizard_value_artefact();               break;
    case '+': wizard_make_object_randart();          break;
    case '|': wizard_create_all_artefacts();         break;
    case 'C': wizard_uncurse_item();                 break;
    case 'g': wizard_exercise_skill();               break;
    case 'G': wizard_dismiss_all_monsters();         break;
    case 'c': wizard_draw_card();                    break;
    case 'H': wizard_heal(true);                     break;
    case 'h': wizard_heal(false);                    break;
    case 'b': blink(1000, true, true);               break;
    case '~': wizard_interlevel_travel();            break;
    case '"': debug_list_monsters();                 break;
    case 't': wizard_tweak_object();                 break;
    case 'T': debug_make_trap();                     break;
    case '\\': debug_make_shop();                    break;
    case 'f': wizard_quick_fsim();                   break;
    case 'F': wizard_fight_sim(false);               break;
    case 'm': wizard_create_spec_monster();          break;
    case 'M': wizard_create_spec_monster_name();     break;
    case 'R': wizard_spawn_control();                break;
    case 'r': wizard_change_species();               break;
    case '>': wizard_place_stairs(true);             break;
    case '<': wizard_place_stairs(false);            break;
    case 'p': wizard_create_portal();                break;
    case 'L': debug_place_map(false);                break;
    case 'P': debug_place_map(true);                 break;
    case 'i': wizard_identify_pack();                break;
    case 'I': wizard_unidentify_pack();              break;
    case 'z': wizard_cast_spec_spell();              break;
    case '(': wizard_create_feature();               break;
    case ')': wizard_mod_tide();                     break;
    case ':': wizard_list_branches();                break;
    case ';': wizard_list_levels();                  break;
    case '{': wizard_map_level();                    break;
    case '}': wizard_reveal_traps();                 break;
    case '@': wizard_set_stats();                    break;
    case '^': wizard_set_piety();                    break;
    case '_': zotdef_create_altar(true);             break;
    case '-': wizard_get_god_gift();                 break;
    case '\'': wizard_list_items();                  break;
    case 'd': wizard_level_travel(true);             break;
    case 'D': wizard_detect_creatures();             break;
    case 'u': case 'U': wizard_level_travel(false);  break;
    case 'o': wizard_create_spec_object();           break;
    case '%': wizard_create_spec_object_by_name();   break;
    case 'J': jiyva_eat_offlevel_items();            break;
    case 'W': wizard_god_wrath();                    break;
    case 'w': wizard_god_mollify();                  break;
    case '#': wizard_load_dump_file();               break;
    case '&': wizard_list_companions();              break;

    case 'x':
        you.experience = 1 + exp_needed(1 + you.experience_level);
        level_change();
        break;

    case 's':
        you.exp_available += HIGH_EXP_POOL;
        level_change();
        you.redraw_experience = true;
        break;

    case '$':
        you.add_gold(1000);
        if (!Options.show_gold_turns)
        {
            mprf("You now have %d gold piece%s.",
                 you.gold, you.gold != 1 ? "s" : "");
        }
        break;

    case 'B':
        if (!player_in_branch(BRANCH_ABYSS))
            banished("wizard command");
        else
            down_stairs(DNGN_EXIT_ABYSS);
        break;

    case CONTROL('A'):
        if (player_in_branch(BRANCH_ABYSS))
            wizard_set_abyss();
        else
            mpr("You can only abyss_teleport() inside the Abyss.");
        break;

    case ']':
        if (!wizard_add_mutation())
            mpr("Failure to give mutation.");
        break;

    case '=':
        mprf("Cost level: %d  Total experience: %d  Next cost level: %d Skill cost: %d",
              you.skill_cost_level, you.total_experience,
              skill_cost_needed(you.skill_cost_level + 1),
              calc_skill_cost(you.skill_cost_level));
        break;

    case 'X':
    {
        int result = 0;
        do
        {
            if (you.religion == GOD_XOM)
                result = xom_acts(abs(you.piety - HALF_MAX_PIETY));
            else
                result = xom_acts(coinflip(), random_range(0, HALF_MAX_PIETY));
        }
        while (result == 0);
        break;
    }

    case 'k':
        if (player_in_branch(BRANCH_LABYRINTH))
            change_labyrinth(true);
        else
            mpr("This only makes sense in a labyrinth!");
        break;

    case 'Z':
    case CONTROL('Z'):
        if (crawl_state.game_is_zotdef())
        {
            you.zot_points = 1000000;
            you.redraw_experience = true;
        }
        else
            mpr("But you're not in Zot Defence!");
        break;


    default:
        if (!silent_fail)
        {
            formatted_mpr(formatted_string::parse_string(
                              "Not a <magenta>Wizard</magenta> Command."));
        }
        break;
    }
    // Force the placement of any delayed monster gifts.
    you.turn_is_over = true;
    religion_turn_end();

    you.turn_is_over = false;
}

static void _handle_wizard_command(void)
{
    int wiz_command;

    // WIZ_NEVER gives protection for those who have wiz compiles,
    // and don't want to risk their characters. Also, and hackishly,
    // it's used to prevent access for non-authorised users to wizard
    // builds in dgamelaunch builds unless the game is started with the
    // -wizard flag.
    if (Options.wiz_mode == WIZ_NEVER)
        return;

    if (!you.wizard)
    {
        mpr("WARNING: ABOUT TO ENTER WIZARD MODE!", MSGCH_WARN);

#ifndef SCORE_WIZARD_CHARACTERS
        mpr("If you continue, your game will not be scored!", MSGCH_WARN);
#endif

        if (!yesno("Do you really want to enter wizard mode?", false, 'n'))
        {
            canned_msg(MSG_OK);
            return;
        }

        take_note(Note(NOTE_MESSAGE, 0, 0, "Entered wizard mode."));

        you.wizard = true;
        save_game(false);
        redraw_screen();

        if (crawl_state.cmd_repeat_start)
        {
            crawl_state.cancel_cmd_repeat("Can't repeat entering wizard "
                                          "mode.");
            return;
        }
    }

    {
        mpr("Enter Wizard Command (? - help): ", MSGCH_PROMPT);
        cursor_control con(true);
        wiz_command = getchm();
        if (wiz_command == '*')
            wiz_command = CONTROL(toupper(getchm()));
    }

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

    _do_wizard_command(wiz_command, false);
}
#endif

// Set up the running variables for the current run.
static void _start_running(int dir, int mode)
{
    if (Hints.hints_events[HINT_SHIFT_RUN] && mode == RMODE_START)
        Hints.hints_events[HINT_SHIFT_RUN] = false;

    if (!i_feel_safe(true))
        return;

    coord_def next_pos = you.pos() + Compass[dir];
    for (adjacent_iterator ai(next_pos); ai; ++ai)
    {
        if (env.grid(*ai) == DNGN_SLIMY_WALL
            && (you.religion != GOD_JIYVA || you.penance[GOD_JIYVA]))
        {
            mpr("You're about to run into the slime covered wall!",
                MSGCH_WARN);
            return;
        }
    }

    you.running.initialise(dir, mode);
}

static bool _cmd_is_repeatable(command_type cmd, bool is_again = false)
{
    switch (cmd)
    {
    // Informational commands
    case CMD_LOOK_AROUND:
    case CMD_INSPECT_FLOOR:
    case CMD_SHOW_TERRAIN:
    case CMD_EXAMINE_OBJECT:
    case CMD_LIST_ARMOUR:
    case CMD_LIST_JEWELLERY:
    case CMD_LIST_EQUIPMENT:
    case CMD_LIST_GOLD:
    case CMD_CHARACTER_DUMP:
    case CMD_DISPLAY_COMMANDS:
    case CMD_DISPLAY_INVENTORY:
    case CMD_DISPLAY_KNOWN_OBJECTS:
    case CMD_DISPLAY_MUTATIONS:
    case CMD_DISPLAY_SKILLS:
    case CMD_DISPLAY_OVERMAP:
    case CMD_DISPLAY_RELIGION:
    case CMD_DISPLAY_RUNES:
    case CMD_DISPLAY_CHARACTER_STATUS:
    case CMD_DISPLAY_SPELLS:
    case CMD_EXPERIENCE_CHECK:
    case CMD_RESISTS_SCREEN:
    case CMD_READ_MESSAGES:
    case CMD_SEARCH_STASHES:
        mpr("You can't repeat informational commands.");
        return false;

    // Multi-turn commands
    case CMD_PICKUP:
    case CMD_DROP:
    case CMD_DROP_LAST:
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
    case CMD_TOGGLE_FRIENDLY_PICKUP:
    case CMD_TOGGLE_TRAVEL_SPEED:
    case CMD_ADJUST_INVENTORY:
    case CMD_QUIVER_ITEM:
    case CMD_REPLAY_MESSAGES:
    case CMD_REDRAW_SCREEN:
    case CMD_MACRO_ADD:
    case CMD_SAVE_GAME:
    case CMD_SAVE_GAME_NOW:
    case CMD_SUSPEND_GAME:
    case CMD_QUIT:
    case CMD_DESTROY_ITEM:
    case CMD_FIX_WAYPOINT:
    case CMD_CLEAR_MAP:
    case CMD_INSCRIBE_ITEM:
    case CMD_MAKE_NOTE:
    case CMD_CYCLE_QUIVER_FORWARD:
#ifdef USE_TILE
    case CMD_EDIT_PLAYER_TILE:
#endif
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

        return _cmd_is_repeatable(crawl_state.prev_cmd, true);

    case CMD_MOVE_NOWHERE:
    case CMD_REST:
    case CMD_WAIT:
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
        {
            return yesno("Really repeat movement command while monsters "
                         "are nearby?", false, 'n');
        }

        return true;

    case CMD_NO_CMD:
    case CMD_NO_CMD_DEFAULT:
        mpr("Unknown command, not repeating.");
        return false;

    default:
        return true;
    }

    return false;
}

// Used to determine whether to apply the berserk penalty at end of round.
bool apply_berserk_penalty = false;

static void _center_cursor()
{
#ifndef USE_TILE_LOCAL
    const coord_def cwhere = crawl_view.grid2view(you.pos());
    cgotoxy(cwhere.x, cwhere.y, GOTO_DNGN);
#endif
}


// We have to refresh the SH display if the player's incapacitated state
// changes (getting confused/paralyzed/etc. sets SH to 0, recovering
// from the condition sets SH back to normal).
struct disable_check
{
    disable_check(bool current)
    {
        was_disabled = current;
    }
    ~disable_check()
    {
        if (you.incapacitated() != was_disabled)
            you.redraw_armour_class = true;
    }

    bool was_disabled;
};

static void _update_place_info()
{
    if (you.num_turns == -1)
        return;

    PlaceInfo  delta;

    delta.turns_total++;
    delta.elapsed_total += you.time_taken;

    switch (you.running)
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

        if (!you.delay_queue.empty()
            && you.delay_queue.front().type == DELAY_REST)
        {
            prev_was_rest = true;
        }

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

        if (you.delay_queue.empty()
            || you.delay_queue.front().type != DELAY_REST)
        {
            prev_was_rest = false;
        }
        break;
    }

    you.global_info += delta;
    you.global_info.assert_validity();

    PlaceInfo& curr_PlaceInfo = you.get_place_info();
    curr_PlaceInfo += delta;
    curr_PlaceInfo.assert_validity();
}

//
//  This function handles the player's input. It's called from main(),
//  from inside an endless loop.
//
static void _input()
{
    if (crawl_state.seen_hups)
        save_game(true, "Game saved, see you later!");

    crawl_state.clear_mon_acting();

    disable_check player_disabled(you.incapacitated());
    religion_turn_start();
    god_conduct_turn_start();
    you.walking = 0;

    // Currently only set if Xom accidentally kills the player.
    you.reset_escaped_death();

    reset_damage_counters();

    if (you.dead)
    {
        revive();
        bring_to_safety();
        redraw_screen();
    }

    if (crawl_state.is_replaying_keys() && crawl_state.is_repeating_cmd()
        && kbhit())
    {
        // User pressed a key, so stop repeating commands and discard
        // the keypress.
        crawl_state.cancel_cmd_repeat("Key pressed, interrupting command "
                                      "repetition.");
        crawl_state.prev_cmd = CMD_NO_CMD;
        flush_prev_message();
        getchm();
        return;
    }

    _prep_input();

    update_monsters_in_view();

    // Monster update can cause a weapon swap.
    if (you.turn_is_over)
    {
        world_reacts();
        return;
    }

    hints_new_turn();

    if (you.cannot_act())
    {
        if (crawl_state.repeat_cmd != CMD_WIZARD)
        {
            crawl_state.cancel_cmd_repeat("Cannot move, cancelling command "
                                          "repetition.");
        }
        world_reacts();
        return;
    }

    // Stop autoclearing more now that we have control back.
    if (!you_are_delayed())
        set_more_autoclear(false);

    if (need_to_autopickup())
    {
        autopickup();
        if (you.turn_is_over)
        {
            world_reacts();
            return;
        }
    }

    if (need_to_autoinscribe())
        autoinscribe();

    if (you_are_delayed() && current_delay_action() != DELAY_MACRO_PROCESS_KEY)
    {
        handle_delay();

        // Some delays reset you.time_taken.
        if (you.time_taken || you.turn_is_over)
        {
            if (you.berserk())
                _do_berserk_no_combat_penalty();
            world_reacts();
        }

        if (!you_are_delayed())
            update_can_train();

#ifdef USE_TILE_WEB
        tiles.flush_messages();
#endif

        return;
    }

    ASSERT(!you.turn_is_over);

    crawl_state.check_term_size();
    if (crawl_state.terminal_resized)
        handle_terminal_resize();

    repeat_again_rec.paused = crawl_state.is_replaying_keys();

    {
        clear_macro_process_key_delay();

        // At this point we are guaranteed to not be in any recursion, so the
        // Lua stack must be empty.  Unless there's a leak.
        ASSERT(lua_gettop(clua.state()) == 0);

        if (!has_pending_input() && !kbhit())
        {
            if (++crawl_state.lua_calls_no_turn > 1000)
                mprf(MSGCH_ERROR, "Infinite lua loop detected, aborting.");
            else
                clua.callfn("ready", 0, 0);
        }

        // We're not in an infinite loop, reset the timer.
        watchdog();

        // Flush messages and display message window.
        msgwin_new_cmd();

        crawl_state.waiting_for_command = true;
        c_input_reset(true);

#ifdef USE_TILE_LOCAL
        cursor_control con(false);
#endif
        const command_type cmd = _get_next_cmd();

        if (crawl_state.seen_hups)
            save_game(true, "Game saved, see you later!");

        crawl_state.waiting_for_command = false;

        if (!crawl_state.is_replaying_keys())
            you.elapsed_time_at_last_input = you.elapsed_time;

        if (cmd != CMD_PREV_CMD_AGAIN && cmd != CMD_NO_CMD
            && !crawl_state.is_replaying_keys())
        {
            crawl_state.prev_cmd = cmd;
        }

        if (cmd != CMD_MOUSE_MOVE)
            c_input_reset(false);

        // [dshaligram] If get_next_cmd encountered a Lua macro
        // binding, your turn may be ended by the first invoke of the
        // macro.
        if (!you.turn_is_over && cmd != CMD_NEXT_CMD)
            process_command(cmd);

        repeat_again_rec.paused = true;

        if (cmd != CMD_MOUSE_MOVE)
            c_input_reset(false, true);

        // If the command was CMD_REPEAT_CMD, then the key for the
        // command to repeat has been placed into the macro buffer,
        // so return now to let input() be called again while
        // the keys to repeat are recorded.
        if (cmd == CMD_REPEAT_CMD)
            return;

        // If the command was CMD_PREV_CMD_AGAIN then _input() has been
        // recursively called by _do_prev_cmd_again() via process_command()
        // to re-do the command, so there's nothing more to do.
        if (cmd == CMD_PREV_CMD_AGAIN)
            return;
    }

    if (need_to_autoinscribe())
        autoinscribe();

    if (you.turn_is_over)
    {
        if (apply_berserk_penalty)
            _do_berserk_no_combat_penalty();

        update_can_train();
        world_reacts();
    }

    _update_replay_state();

    _update_place_info();

    crawl_state.clear_god_acting();
}

static bool _can_take_stairs(dungeon_feature_type ftype, bool down,
                             bool known_shaft)
{
    // Immobile
    if (you.form == TRAN_TREE)
    {
        canned_msg(MSG_CANNOT_MOVE);
        return false;
    }

    // Mesmerized
    if (you.beheld() && !you.confused())
    {
        const monster* beholder = you.get_any_beholder();
        mprf("You cannot move away from %s!",
             beholder->name(DESC_THE, true).c_str());
        return false;
    }

    // Held
    if (you.attribute[ATTR_HELD])
    {
        mprf("You can't do that while %s.", held_status());
        return false;
    }

    // Up and down both work for shops and portals.
    if (ftype == DNGN_ENTER_SHOP)
    {
        if (you.berserk())
            canned_msg(MSG_TOO_BERSERK);
        else
            shop();
        // Even though we may have "succeeded", return false so we don't keep
        // trying to go downstairs.
        return false;
    }

    // If it's not bidirectional, check that the player is headed
    // in the right direction.
    if (!feat_is_bidirectional_portal(ftype))
    {
        if (feat_stair_direction(ftype) != (down ? CMD_GO_DOWNSTAIRS
                                                 : CMD_GO_UPSTAIRS)
            // probably shouldn't be passed known_shaft=true
            // if going up, but just in case...
            && (!down || !known_shaft))
        {
            if (ftype == DNGN_STONE_ARCH)
                mpr("There is nothing on the other side of the stone arch.");
            else if (ftype == DNGN_ABANDONED_SHOP)
                mpr("This shop appears to be closed.");
            else if (down)
                mpr("You can't go down here!");
            else
                mpr("You can't go up here!");
            return false;
        }
    }

    // Overloaded, can't go up stairs.
    if (!down && you.burden_state == BS_OVERLOADED
        && !feat_is_escape_hatch(ftype) && !feat_is_gate(ftype))
    {
        mpr("You are carrying too much to climb upwards.");
        return false;
    }

    return true;
}

static bool _marker_vetoes_stair()
{
    return marker_vetoes_operation("veto_stair");
}

// Maybe prompt to enter a portal, return true if we should enter the
// portal, false if the user said no at the prompt.
static bool _prompt_dangerous_portal(dungeon_feature_type ftype)
{
    switch (ftype)
    {
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_ENTER_ABYSS:
        return yesno("If you enter this portal you will not be able to return "
                     "immediately. Continue?", false, 'n');

    case DNGN_MALIGN_GATEWAY:
        return yesno("Are you sure you wish to approach this portal? There's no "
                     "telling what its forces would wreak upon your fragile "
                     "self.", false, 'n');

    default:
        return true;
    }
}

static bool _prompt_unique_pan_rune(dungeon_feature_type ygrd)
{
    if (ygrd != DNGN_TRANSIT_PANDEMONIUM
        && ygrd != DNGN_EXIT_PANDEMONIUM
        && ygrd != DNGN_ENTER_ABYSS)
    {
        return true;
    }

    item_def* rune = find_floor_item(OBJ_MISCELLANY, MISC_RUNE_OF_ZOT);
    if (rune && item_is_unique_rune(*rune))
    {
        return yesno("An item of great power still resides in this realm, "
                "and once you leave you can never return. "
                "Are you sure you want to leave?");
    }
    return true;
}

static bool _prompt_stairs(dungeon_feature_type ygrd, bool down)
{
    // Certain portal types always carry warnings.
    if (!_prompt_dangerous_portal(ygrd))
    {
        canned_msg(MSG_OK);
        return false;
    }

    // Does the next level have a warning annotation?
    // Also checks for entering a labyrinth with teleportitis.
    if (!check_annotation_exclusion_warning())
        return false;

    // Toll portals, eg. troves, ziggurats. (Using vetoes like this is hacky.)
    if (_marker_vetoes_stair())
        return false;

    // Exiting Ziggurats early.
    if (ygrd == DNGN_EXIT_PORTAL_VAULT
        && player_in_branch(BRANCH_ZIGGURAT)
        && you.depth < brdepth[BRANCH_ZIGGURAT])
    {
        if (!yesno("Are you sure you want to leave this Ziggurat?"))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    // Leaving Pan runes behind.
    if (!_prompt_unique_pan_rune(ygrd))
    {
        canned_msg(MSG_OK);
        return false;
    }

    // Escaping.
    if (!down && ygrd == DNGN_EXIT_DUNGEON && !player_has_orb())
    {
        string prompt = make_stringf("Are you sure you want to leave the "
                                     "Dungeon?%s",
                                     crawl_state.game_is_tutorial() ? "" :
                                     " This will make you lose the game!");
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            mpr("Alright, then stay!");
            return false;
        }
    }

    return true;
}

static void _take_stairs(bool down)
{
    ASSERT(!crawl_state.game_is_arena() && !crawl_state.arena_suspended);

    const dungeon_feature_type ygrd = grd(you.pos());

    const bool shaft = (down && get_trap_type(you.pos()) == TRAP_SHAFT
                             && ygrd != DNGN_UNDISCOVERED_TRAP);

    if (!_can_take_stairs(ygrd, down, shaft))
        return;

    if (!you.attempt_escape()) // false means constricted and don't escape
        return;

    if (!_prompt_stairs(ygrd, down))
        return;

    you.clear_clinging();
    you.stop_constricting_all(true);
    you.stop_being_constricted();

    if (shaft)
        start_delay(DELAY_DESCENDING_STAIRS, 0);
    else
    {
        tag_followers(); // Only those beside us right now can follow.
        start_delay(down ? DELAY_DESCENDING_STAIRS : DELAY_ASCENDING_STAIRS,
                    1 + (you.burden_state > BS_UNENCUMBERED));
    }
}

static void _experience_check()
{
    mprf("You are a level %d %s %s.",
         you.experience_level,
         species_name(you.species).c_str(),
         you.class_name.c_str());
    int perc = get_exp_progress();

    if (you.experience_level < 27)
    {
        mprf("You are %d%% of the way to level %d.", perc,
              you.experience_level + 1);
    }
    else
    {
        mpr("I'm sorry, level 27 is as high as you can go.");
        mpr("With the way you've been playing, I'm surprised you got this far.");
    }

    if (you.species == SP_FELID)
    {
        int xl = you.experience_level;
        // calculate the "real" level
        while (you.experience >= exp_needed(xl + 1))
            xl++;
        int nl = you.max_level;
        // and the next level you'll get a life
        do nl++; while (!will_gain_life(nl));

        // old value was capped at XL27
        perc = (you.experience - exp_needed(xl)) * 100
             / (exp_needed(xl + 1) - exp_needed(xl));
        perc = (nl - xl) * 100 - perc;
        mprf(you.lives < 2 ?
             "You'll get an extra life in %d.%02d levels' worth of XP." :
             "If you died right now, you'd get an extra life in %d.%02d levels' worth of XP.",
             perc / 100, perc % 100);
    }

    handle_real_time();
    msg::stream << "Play time: " << make_time_string(you.real_time)
                << " (" << you.num_turns << " turns)"
                << endl;
#ifdef DEBUG_DIAGNOSTICS
    if (you.gourmand())
    {
        mprf(MSGCH_DIAGNOSTICS, "Gourmand charge: %d",
             you.duration[DUR_GOURMAND]);
    }

    mprf(MSGCH_DIAGNOSTICS, "Turns spent on this level: %d",
         env.turns_on_level);
#endif
}

static void _print_friendly_pickup_setting(bool was_changed)
{
    string now = (was_changed? "now " : "");

    if (you.friendly_pickup == FRIENDLY_PICKUP_NONE)
    {
        mprf("Your intelligent allies are %sforbidden to pick up anything at all.",
             now.c_str());
    }
    else if (you.friendly_pickup == FRIENDLY_PICKUP_FRIEND)
    {
        mprf("Your intelligent allies may %sonly pick up items dropped by allies.",
             now.c_str());
    }
    else if (you.friendly_pickup == FRIENDLY_PICKUP_PLAYER)
    {
        mprf("Your intelligent allies may %sonly pick up items dropped by you "
             "and your allies.", now.c_str());
    }
    else if (you.friendly_pickup == FRIENDLY_PICKUP_ALL)
    {
        mprf("Your intelligent allies may %spick up anything they need.",
             now.c_str());
    }
    else
        mprf(MSGCH_ERROR, "Your allies%s are collecting bugs!", now.c_str());
}

static void _do_look_around()
{
    dist lmove;   // Will be initialised by direction().
    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.just_looking = true;
    args.needs_path = false;
    args.target_prefix = "Here";
    args.may_target_monster = "Move the cursor around to observe a square.";
    direction(lmove, args);
    if (lmove.isValid && lmove.isTarget && !lmove.isCancel
        && !crawl_state.arena_suspended)
    {
        start_travel(lmove.target);
    }
}

static void _do_remove_armour()
{
    if (you.species == SP_FELID)
    {
        mpr("You can't remove your fur, sorry.");
        return;
    }

    if (!form_can_wear())
    {
        mpr("You can't wear or remove anything in your present form.");
        return;
    }

    int index = 0;
    if (armour_prompt("Take off which item?", &index, OPER_TAKEOFF))
        takeoff_armour(index);
}

static void _toggle_friendly_pickup()
{
    // Toggle pickup mode for friendlies.
    _print_friendly_pickup_setting(false);

    mpr("Change to (d)efault, (n)othing, (f)riend-dropped, (p)layer, "
        "or (a)ll? ", MSGCH_PROMPT);

    int type;
    {
        cursor_control con(true);
        type = toalower(getchm(KMC_DEFAULT));
    }

    switch (type)
    {
    case 'd': you.friendly_pickup = Options.default_friendly_pickup; break;
    case 'n': you.friendly_pickup = FRIENDLY_PICKUP_NONE; break;
    case 'f': you.friendly_pickup = FRIENDLY_PICKUP_FRIEND; break;
    case 'p': you.friendly_pickup = FRIENDLY_PICKUP_PLAYER; break;
    case 'a': you.friendly_pickup = FRIENDLY_PICKUP_ALL; break;
    default: canned_msg(MSG_OK); return;
    }

    _print_friendly_pickup_setting(true);
}

static void _toggle_travel_speed()
{
    you.travel_ally_pace = !you.travel_ally_pace;
    if (you.travel_ally_pace)
        mpr("You pace your travel speed to your slowest ally.");
    else
    {
        mpr("You travel at normal speed.");
        you.running.travel_speed = 0;
    }
}

static void _do_rest()
{
    if (you.hunger_state == HS_STARVING && !you_min_hunger())
    {
        mpr("You're too hungry to rest.");
        return;
    }

    if (i_feel_safe())
    {
        if ((you.hp == you.hp_max
                || player_mutation_level(MUT_SLOW_HEALING) == 3
                || (you.species == SP_VAMPIRE
                    && you.hunger_state == HS_STARVING))
            && you.magic_points == you.max_magic_points)
        {
            mpr("You start waiting.");
        }
        else
            mpr("You start resting.");
    }

    _start_running(RDIR_REST, RMODE_REST_DURATION);
}

static void _do_clear_map()
{
    if (Options.show_travel_trail && env.travel_trail.size())
    {
        mpr("Clearing travel trail.");
        clear_travel_trail();
    }
    else
    {
        mpr("Clearing level map.");
        clear_map();
        crawl_view.set_player_at(you.pos());
    }
}

static void _do_display_map()
{
    if (Hints.hints_events[HINT_MAP_VIEW])
        Hints.hints_events[HINT_MAP_VIEW] = false;

#ifdef USE_TILE_LOCAL
    // Since there's no actual overview map, but the functionality
    // exists, give a message to explain what's going on.
    mpr("Move the cursor to view the level map, or type <w>?</w> for "
        "a list of commands.");
    flush_prev_message();
#endif

    level_pos pos;
    const bool travel = show_map(pos, true, true, true);

#ifdef USE_TILE_LOCAL
    mpr("Returning to the game...");
#endif
    if (travel)
        start_translevel_travel(pos);
}

static void _do_cycle_quiver(int dir)
{
    if (you.species == SP_FELID)
    {
        mpr("You can't grasp things well enough to throw them.");
        return;
    }

    const int cur = you.m_quiver->get_fire_item();
    const int next = get_next_fire_item(cur, dir);
#ifdef DEBUG_QUIVER
    mprf(MSGCH_DIAGNOSTICS, "next slot: %d, item: %s", next,
         next == -1 ? "none" : you.inv[next].name(DESC_PLAIN).c_str());
#endif
    if (next != -1)
    {
        // Kind of a hacky way to get quiver to change.
        you.m_quiver->on_item_fired(you.inv[next], true);

        if (next == cur)
            mpr("No other missiles available. Use F to throw any item.");
    }
    else if (cur == -1)
        mpr("No missiles available. Use F to throw any item.");
}

static void _do_list_gold()
{
    if (shopping_list.empty())
        mprf("You have %d gold piece%s.", you.gold, you.gold != 1 ? "s" : "");
    else
        shopping_list.display();
}

// Note that in some actions, you don't want to clear afterwards.
// e.g. list_jewellery, etc.
void process_command(command_type cmd)
{
    apply_berserk_penalty = true;
    switch (cmd)
    {
#ifdef USE_TILE
        // Tiles-specific commands.
    case CMD_EDIT_PLAYER_TILE: tiles.draw_doll_edit(); break;
#endif

        // Movement and running commands.
    case CMD_OPEN_DOOR_UP_RIGHT:   _open_door(1, -1); break;
    case CMD_OPEN_DOOR_UP:         _open_door(0, -1); break;
    case CMD_OPEN_DOOR_UP_LEFT:    _open_door(-1, -1); break;
    case CMD_OPEN_DOOR_RIGHT:      _open_door(1,  0); break;
    case CMD_OPEN_DOOR_DOWN_RIGHT: _open_door(1,  1); break;
    case CMD_OPEN_DOOR_DOWN:       _open_door(0,  1); break;
    case CMD_OPEN_DOOR_DOWN_LEFT:  _open_door(-1,  1); break;
    case CMD_OPEN_DOOR_LEFT:       _open_door(-1,  0); break;

    case CMD_MOVE_DOWN_LEFT:  _move_player(-1,  1); break;
    case CMD_MOVE_DOWN:       _move_player(0,  1); break;
    case CMD_MOVE_UP_RIGHT:   _move_player(1, -1); break;
    case CMD_MOVE_UP:         _move_player(0, -1); break;
    case CMD_MOVE_UP_LEFT:    _move_player(-1, -1); break;
    case CMD_MOVE_LEFT:       _move_player(-1,  0); break;
    case CMD_MOVE_DOWN_RIGHT: _move_player(1,  1); break;
    case CMD_MOVE_RIGHT:      _move_player(1,  0); break;

    case CMD_RUN_DOWN_LEFT: _start_running(RDIR_DOWN_LEFT, RMODE_START); break;
    case CMD_RUN_DOWN:      _start_running(RDIR_DOWN, RMODE_START);      break;
    case CMD_RUN_UP_RIGHT:  _start_running(RDIR_UP_RIGHT, RMODE_START);  break;
    case CMD_RUN_UP:        _start_running(RDIR_UP, RMODE_START);        break;
    case CMD_RUN_UP_LEFT:   _start_running(RDIR_UP_LEFT, RMODE_START);   break;
    case CMD_RUN_LEFT:      _start_running(RDIR_LEFT, RMODE_START);      break;
    case CMD_RUN_DOWN_RIGHT:_start_running(RDIR_DOWN_RIGHT, RMODE_START); break;
    case CMD_RUN_RIGHT:     _start_running(RDIR_RIGHT, RMODE_START);     break;

#ifdef CLUA_BINDINGS
    case CMD_AUTOFIGHT:
        if (!clua.callfn("hit_closest", 0, 0))
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
        break;
    case CMD_AUTOFIGHT_NOMOVE:
        if (!clua.callfn("hit_adjacent", 0, 0))
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
        break;
#endif
    case CMD_REST:            _do_rest(); break;

    case CMD_GO_UPSTAIRS:
    case CMD_GO_DOWNSTAIRS:
        _take_stairs(cmd == CMD_GO_DOWNSTAIRS);
        break;

    case CMD_OPEN_DOOR:       _open_door(0, 0); break;
    case CMD_CLOSE_DOOR:      _close_door(coord_def(0, 0)); break;

    // Repeat commands.
    case CMD_REPEAT_CMD:     _do_cmd_repeat();  break;
    case CMD_PREV_CMD_AGAIN: _do_prev_cmd_again(); break;
    case CMD_MACRO_ADD:      macro_add_query();    break;

    // Toggle commands.
    case CMD_DISABLE_MORE: crawl_state.show_more_prompt = false; break;
    case CMD_ENABLE_MORE:  crawl_state.show_more_prompt = true;  break;

    case CMD_TOGGLE_AUTOPICKUP:
        if (Options.autopickup_on < 1)
            Options.autopickup_on = 1;
        else
            Options.autopickup_on = 0;
        mprf("Autopickup is now %s.", Options.autopickup_on > 0 ? "on" : "off");
        break;

    case CMD_TOGGLE_FRIENDLY_PICKUP:     _toggle_friendly_pickup(); break;
    case CMD_TOGGLE_VIEWPORT_MONSTER_HP: toggle_viewport_monster_hp(); break;
    case CMD_TOGGLE_TRAVEL_SPEED:        _toggle_travel_speed(); break;


        // Map commands.
    case CMD_CLEAR_MAP:       _do_clear_map();   break;
    case CMD_DISPLAY_OVERMAP: display_overview(); break;
    case CMD_DISPLAY_MAP:     _do_display_map(); break;

#ifdef TOUCH_UI
        // zoom commands
    case CMD_ZOOM_IN:   tiles.zoom_dungeon(true); break;
    case CMD_ZOOM_OUT:  tiles.zoom_dungeon(false); break;
#endif

        // Stash commands.
    case CMD_SEARCH_STASHES:
        if (Hints.hints_stashes)
            Hints.hints_stashes = 0;
        StashTrack.search_stashes();
        break;

    case CMD_INSPECT_FLOOR:
        request_autopickup();
        if (player_on_single_stack() && !you.running)
            pickup(true);
        break;
    case CMD_SHOW_TERRAIN: toggle_show_terrain(); break;
    case CMD_ADJUST_INVENTORY: adjust(); break;

    case CMD_MOVE_NOWHERE:
    case CMD_WAIT:
        you.check_clinging(false);
        you.turn_is_over = true;
        break;

    case CMD_PICKUP:
    case CMD_PICKUP_QUANTITY:
        pickup(cmd != CMD_PICKUP);
        break;

        // Action commands.
    case CMD_BUTCHER:              butchery();               break;
    case CMD_CAST_SPELL:           do_cast_spell_cmd(false); break;
    case CMD_DISPLAY_SPELLS:       inspect_spells();         break;
    case CMD_EAT:                  eat_food();               break;
    case CMD_EXAMINE_OBJECT:       examine_object();         break;
    case CMD_FIRE:                 fire_thing();             break;
    case CMD_FORCE_CAST_SPELL:     do_cast_spell_cmd(true);  break;
    case CMD_LOOK_AROUND:          _do_look_around();        break;
    case CMD_PRAY:                 pray();                   break;
    case CMD_QUAFF:                drink();                  break;
    case CMD_READ:                 read_scroll();            break;
    case CMD_REMOVE_ARMOUR:        _do_remove_armour();      break;
    case CMD_REMOVE_JEWELLERY:     remove_ring();            break;
    case CMD_SHOUT:                yell();                   break;
    case CMD_THROW_ITEM_NO_QUIVER: throw_item_no_quiver();   break;
    case CMD_WEAPON_SWAP:          wield_weapon(true);       break;
    case CMD_WEAR_ARMOUR:          wear_armour();            break;
    case CMD_WEAR_JEWELLERY:       puton_ring(-1);           break;
    case CMD_WIELD_WEAPON:         wield_weapon(false);      break;
    case CMD_ZAP_WAND:             zap_wand();               break;

    case CMD_DROP:
        drop();
        break;

    case CMD_DROP_LAST:
        drop_last();
        break;

    case CMD_EVOKE:
        if (!evoke_item())
            flush_input_buffer(FLUSH_ON_FAILURE);
        break;

    case CMD_EVOKE_WIELDED:
        if (!evoke_item(you.equip[EQ_WEAPON]))
            flush_input_buffer(FLUSH_ON_FAILURE);
        break;

    case CMD_MEMORISE_SPELL:
        if (!learn_spell())
            flush_input_buffer(FLUSH_ON_FAILURE);
        break;

    case CMD_USE_ABILITY:
        if (!activate_ability())
            flush_input_buffer(FLUSH_ON_FAILURE);
        break;

        // Informational commands.
    case CMD_DISPLAY_CHARACTER_STATUS: display_char_status();          break;
    case CMD_DISPLAY_COMMANDS:         list_commands(0, true);         break;
    case CMD_DISPLAY_INVENTORY:        get_invent(OSEL_ANY);           break;
    case CMD_DISPLAY_KNOWN_OBJECTS: check_item_knowledge(); redraw_screen(); break;
    case CMD_DISPLAY_MUTATIONS: display_mutations(); redraw_screen();  break;
    case CMD_DISPLAY_RUNES:            display_runes();                break;
    case CMD_DISPLAY_SKILLS:           skill_menu(); redraw_screen();  break;
    case CMD_EXPERIENCE_CHECK:         _experience_check();            break;
    case CMD_FULL_VIEW:                full_describe_view();           break;
    case CMD_INSCRIBE_ITEM:            prompt_inscribe_item();         break;
    case CMD_LIST_ARMOUR:              list_armour();                  break;
    case CMD_LIST_EQUIPMENT:           get_invent(OSEL_EQUIP);         break;
    case CMD_LIST_GOLD:                _do_list_gold();                break;
    case CMD_LIST_JEWELLERY:           list_jewellery();               break;
    case CMD_MAKE_NOTE:                make_user_note();               break;
    case CMD_REPLAY_MESSAGES: replay_messages(); redraw_screen();      break;
    case CMD_RESISTS_SCREEN:           print_overview_screen();        break;

    case CMD_DISPLAY_RELIGION:
    {
#ifdef USE_TILE_WEB
        tiles_crt_control show_as_menu(CRT_MENU, "describe_god");
#endif
        describe_god(you.religion, true);
        redraw_screen();
        break;
    }

    case CMD_READ_MESSAGES:
#ifdef DGL_SIMPLE_MESSAGING
        if (SysEnv.have_messages)
            read_messages();
#endif
        break;

    case CMD_CHARACTER_DUMP:
        if (!dump_char(you.your_name))
            mpr("Char dump unsuccessful! Sorry about that.");
        break;

        // Travel commands.
    case CMD_FIX_WAYPOINT:      travel_cache.add_waypoint(); break;
    case CMD_INTERLEVEL_TRAVEL: do_interlevel_travel();      break;
    case CMD_ANNOTATE_LEVEL:    annotate_level();            break;
    case CMD_EXPLORE:           do_explore_cmd();            break;

        // Mouse commands.
    case CMD_MOUSE_MOVE:
    {
        const coord_def dest = crawl_view.screen2grid(crawl_view.mousep);
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
        if (cme && crawl_view.in_viewport_s(cme.pos))
        {
            const coord_def dest = crawl_view.screen2grid(cme.pos);
            if (cme.left_clicked())
            {
                if (in_bounds(dest))
                    start_travel(dest);
            }
            else if (cme.right_clicked())
            {
                if (you.see_cell(dest))
                    full_describe_square(dest);
                else
                    mpr("You can't see that place.");
            }
        }
        break;
    }


        // Quiver commands.
    case CMD_QUIVER_ITEM:           choose_item_for_quiver(); break;
    case CMD_CYCLE_QUIVER_FORWARD:  _do_cycle_quiver(+1);     break;
    case CMD_CYCLE_QUIVER_BACKWARD: _do_cycle_quiver(-1);     break;

#ifdef WIZARD
    case CMD_WIZARD: _handle_wizard_command(); break;
#endif

        // Game commands.
    case CMD_REDRAW_SCREEN: redraw_screen(); break;

#ifdef USE_UNIX_SIGNALS
    case CMD_SUSPEND_GAME:
        // CTRL-Z suspend behaviour is implemented here,
        // because we want to have CTRL-Y available...
        // and unfortunately they tend to be stuck together.
        clrscr();
#if !defined(USE_TILE_LOCAL) && !defined(TARGET_OS_WINDOWS)
        console_shutdown();
        kill(0, SIGTSTP);
        console_startup();
#endif
        redraw_screen();
        break;
#endif

    case CMD_SAVE_GAME:
        if (yesno("Save game and exit?", true, 'n'))
            save_game(true);
        else
            canned_msg(MSG_OK);
        break;

    case CMD_SAVE_GAME_NOW:
        mpr("Saving game... please wait.");
        save_game(true);
        break;

    case CMD_QUIT:
        if (crawl_state.disables[DIS_CONFIRMATIONS]
            || yes_or_no("Are you sure you want to abandon this character and quit the game"))
        {
            ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_QUITTING);
        }
        else
            canned_msg(MSG_OK);
        break;

    case CMD_NO_CMD:
    default:
        if (crawl_state.game_is_hints())
        {
            string msg = "Unknown command. (For a list of commands type "
                         "<w>?\?<lightgrey>.)";
            mpr(msg);
        }
        else // well, not examine, but...
            mpr("Unknown command.", MSGCH_EXAMINE_FILTER);

        break;
    }
}

static void _prep_input()
{
    you.turn_is_over = false;
    you.time_taken = player_speed();
    you.shield_blocks = 0;              // no blocks this round

    textcolor(LIGHTGREY);

    set_redraw_status(REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK);
    print_stats();

    viewwindow();
    maybe_update_stashes();
    if (check_for_interesting_features() && you.running.is_explore())
            stop_running();


    if (you.seen_portals)
    {
        ASSERT(you.religion == GOD_ASHENZARI);
        if (you.seen_portals == 1)
            mpr("You have a vision of a gate.", MSGCH_GOD);
        else
            mpr("You have a vision of multiple gates.", MSGCH_GOD);

        you.seen_portals = 0;
    }
    if (you.seen_invis)
    {
        maybe_id_ring_see_invis();
        you.seen_invis = false;
    }
}

// Decrement a single duration. Print the message if the duration runs out.
// Returns true if the duration ended.
// At midpoint (defined by get_expiration_threshold() in player.cc)
// print midmsg and decrease duration by midloss (a randomised amount so as
// to make it impossible to know the exact remaining duration for sure).
// NOTE: The maximum possible midloss should be smaller than midpoint,
//       otherwise the duration may end in the same turn the warning
//       message is printed which would be a bit late.
static bool _decrement_a_duration(duration_type dur, int delay,
                                  const char* endmsg = NULL, int midloss = 0,
                                  const char* midmsg = NULL,
                                  msg_channel_type chan = MSGCH_DURATION)
{
    if (you.duration[dur] < 1)
        return false;

    const int midpoint = get_expiration_threshold(dur);

    int old_dur = you.duration[dur];

    you.duration[dur] -= delay;
    if (you.duration[dur] < 0)
        you.duration[dur] = 0;

    // Did we cross the mid point? (No longer likely to hit it exactly) -cao
    if (you.duration[dur] <= midpoint && old_dur > midpoint)
    {
        if (midmsg)
        {
            if (need_expiration_warning(dur))
                mprf(MSGCH_DANGER, "Careful! %s", midmsg);
            else
                mpr(midmsg, chan);
        }
        you.duration[dur] -= midloss * BASELINE_DELAY;
    }

    // allow fall-through in case midloss ended the duration (it shouldn't)
    if (you.duration[dur] == 0)
    {
        if (endmsg)
            mpr(endmsg, chan);
        return true;
    }

    return false;
}

static void _decrement_paralysis(int delay)
{
    _decrement_a_duration(DUR_PARALYSIS_IMMUNITY, delay);

    if (you.duration[DUR_PARALYSIS])
    {
        _decrement_a_duration(DUR_PARALYSIS, delay);

        if (!you.duration[DUR_PARALYSIS] && !you.petrified())
        {
            mpr("You can move again.", MSGCH_DURATION);
            you.redraw_evasion = true;
            you.duration[DUR_PARALYSIS_IMMUNITY] = roll_dice(1, 3)
                                                   * BASELINE_DELAY;
            if (you.props.exists("paralysed_by"))
                you.props.erase("paralysed_by");
        }
    }
}

static void _decrement_petrification(int delay)
{
    if (_decrement_a_duration(DUR_PETRIFIED, delay) && !you.paralysed())
    {
        you.redraw_evasion = true;
        mprf(MSGCH_DURATION, "You turn to %s and can move again.",
             you.form == TRAN_LICH ? "bone" :
             you.form == TRAN_ICE_BEAST ? "ice" :
             "flesh");
    }

    if (you.duration[DUR_PETRIFYING])
    {
        int &dur = you.duration[DUR_PETRIFYING];
        int old_dur = dur;
        if ((dur -= delay) <= 0)
        {
            dur = 0;
            // If we'd kill the player when active flight stops, this will
            // need to pass the killer.  Unlike monsters, almost all flight is
            // magical, inluding tengu, as there's no flapping of wings.  Should
            // we be nasty to dragon and bat forms?  For now, let's not instakill
            // them even if it's inconsistent.
            you.fully_petrify(NULL);
        }
        else if (dur < 15 && old_dur >= 15)
            mpr("Your limbs are stiffening.");
    }
}

//  Perhaps we should write functions like: update_liquid_flames(), etc.
//  Even better, we could have a vector of callback functions (or
//  objects) which get installed at some point.
static void _decrement_durations()
{
    int delay = you.time_taken;

    if (you.gourmand())
    {
        // Innate gourmand is always fully active.
        if (player_mutation_level(MUT_GOURMAND) > 0)
            you.duration[DUR_GOURMAND] = GOURMAND_MAX;
        else if (you.duration[DUR_GOURMAND] < GOURMAND_MAX && coinflip())
            you.duration[DUR_GOURMAND] += delay;
    }
    else
        you.duration[DUR_GOURMAND] = 0;
    
    // Retractable antennae
    if (you.species == SP_FORMICID && !player_wearing_slot(EQ_HELMET))
    {
        if (form_keeps_mutations()
            && you.duration[DUR_ANTENNAE_EXTEND] < ANTENNAE_EXTEND_TIME)
        {
            you.duration[DUR_ANTENNAE_EXTEND] += delay;
            if (you.duration[DUR_ANTENNAE_EXTEND] >= ANTENNAE_EXTEND_TIME)
            {
                you.duration[DUR_ANTENNAE_EXTEND] = ANTENNAE_EXTEND_TIME;
                // sinv comes back.
                if (you.has_antennae() >= 3)
                    autotoggle_autopickup(false);
                mpr("Your antennae are now fully extended.");
            }
            #ifdef USE_TILE
                init_player_doll();
            #endif
        }
    }
    else
    {
        you.duration[DUR_ANTENNAE_EXTEND] = 0;
    }

    if (you.duration[DUR_ICEMAIL_DEPLETED] > 0)
    {
        if (delay > you.duration[DUR_ICEMAIL_DEPLETED])
            you.duration[DUR_ICEMAIL_DEPLETED] = 0;
        else
            you.duration[DUR_ICEMAIL_DEPLETED] -= delay;

        if (!you.duration[DUR_ICEMAIL_DEPLETED])
            mpr("Your icy envelope is fully restored.", MSGCH_DURATION);

        you.redraw_armour_class = true;
    }

    if (you.duration[DUR_DEMONIC_GUARDIAN] > 0)
    {
        if (delay > you.duration[DUR_DEMONIC_GUARDIAN])
            you.duration[DUR_DEMONIC_GUARDIAN] = 0;
        else
            you.duration[DUR_DEMONIC_GUARDIAN] -= delay;
    }

    // Must come before berserk.
    if (_decrement_a_duration(DUR_BUILDING_RAGE, delay))
        go_berserk(false);

    if (you.duration[DUR_LIQUID_FLAMES])
        dec_napalm_player(delay);

    if (_decrement_a_duration(DUR_ICY_ARMOUR, delay,
                              "Your icy armour evaporates.", coinflip(),
                              you.props.exists("melt_armour") ? NULL
                                  : "Your icy armour starts to melt."))
    {
        you.redraw_armour_class = true;
    }

    // Possible reduction of silence radius.
    if (you.duration[DUR_SILENCE])
        invalidate_agrid();
    // and liquefying radius.
    if (you.duration[DUR_LIQUEFYING])
        invalidate_agrid();

    _decrement_a_duration(DUR_SILENCE, delay, "Your hearing returns.");

    _decrement_a_duration(DUR_REPEL_MISSILES, delay,
                          "You feel less protected from missiles.",
                          coinflip(),
                          "Your repel missiles spell is about to expire...");

    _decrement_a_duration(DUR_DEFLECT_MISSILES, delay,
                          "You feel less protected from missiles.",
                          coinflip(),
                          "Your deflect missiles spell is about to expire...");

    if (_decrement_a_duration(DUR_REGENERATION, delay,
                              NULL, coinflip(),
                              "Your skin is crawling a little less now."))
    {
        remove_regen(you.attribute[ATTR_DIVINE_REGENERATION]);
    }

    _decrement_a_duration(DUR_VEHUMET_GIFT, delay);

    _decrement_a_duration(DUR_JELLY_PRAYER, delay, "Your prayer is over.");

    if (you.duration[DUR_DIVINE_SHIELD] > 0)
    {
        if (you.duration[DUR_DIVINE_SHIELD] > 1)
        {
            you.duration[DUR_DIVINE_SHIELD] -= delay;
            if (you.duration[DUR_DIVINE_SHIELD] <= 1)
            {
                you.duration[DUR_DIVINE_SHIELD] = 1;
                mpr("Your divine shield starts to fade.", MSGCH_DURATION);
            }
        }

        if (you.duration[DUR_DIVINE_SHIELD] == 1 && !one_chance_in(3))
        {
            you.redraw_armour_class = true;
            if (--you.attribute[ATTR_DIVINE_SHIELD] == 0)
            {
                you.duration[DUR_DIVINE_SHIELD] = 0;
                mpr("Your divine shield fades away.", MSGCH_DURATION);
            }
        }
    }

    //jmf: More flexible weapon branding code.
    int last_value = you.duration[DUR_WEAPON_BRAND];

    if (last_value > 0)
    {
        you.duration[DUR_WEAPON_BRAND] -= delay;

        if (you.duration[DUR_WEAPON_BRAND] <= 0)
        {
            you.duration[DUR_WEAPON_BRAND] = 0;
            item_def& weapon = *you.weapon();
            const int temp_effect = get_weapon_brand(weapon);

            set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_NORMAL);
            string msg = weapon.name(DESC_YOUR);

            switch (temp_effect)
            {
            case SPWPN_VORPAL:
                if (get_vorpal_type(weapon) == DVORP_SLICING)
                    msg += " seems blunter.";
                else
                    msg += " feels lighter.";
                break;
            case SPWPN_FLAME:
            case SPWPN_FLAMING:
                msg += " goes out.";
                break;
            case SPWPN_FREEZING:
                msg += " stops glowing.";
                break;
            case SPWPN_FROST:
                msg += "'s frost melts away.";
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
                msg += " seems less pained.";
                break;
            case SPWPN_CHAOS:
                msg += " seems more stable.";
                break;
            case SPWPN_ELECTROCUTION:
                msg += " stops emitting sparks.";
                break;
            case SPWPN_HOLY_WRATH:
                msg += "'s light goes out.";
                break;
            case SPWPN_ANTIMAGIC:
                msg += " stops repelling magic.";
                calc_mp();
                break;
            default:
                msg += " seems inexplicably less special.";
                break;
            }

            mpr(msg.c_str(), MSGCH_DURATION);
            you.wield_change = true;
        }
    }

    // FIXME: [ds] Remove this once we've ensured durations can never go < 0?
    if (you.duration[DUR_TRANSFORMATION] <= 0
        && you.form != TRAN_NONE)
    {
        you.duration[DUR_TRANSFORMATION] = 1;
    }

    // Vampire bat transformations are permanent (until ended).
    if (you.species != SP_VAMPIRE || you.form != TRAN_BAT
        || you.duration[DUR_TRANSFORMATION] <= 5 * BASELINE_DELAY)
    {
        if (_decrement_a_duration(DUR_TRANSFORMATION, delay, NULL, random2(3),
                                  "Your transformation is almost over."))
        {
            untransform();
        }
    }

    // Must come after transformation duration.
    _decrement_a_duration(DUR_BREATH_WEAPON, delay,
                          "You have got your breath back.", 0, NULL,
                          MSGCH_RECOVERY);

    _decrement_a_duration(DUR_SWIFTNESS, delay,
                          "You feel sluggish.", coinflip(),
                          "You start to feel a little slower.");
    _decrement_a_duration(DUR_RESISTANCE, delay,
                          "Your resistance to elements expires.", coinflip(),
                          "You start to feel less resistant.");

    if (_decrement_a_duration(DUR_PHASE_SHIFT, delay,
                    "You are firmly grounded in the material plane once more.",
                    coinflip(),
                    "You feel closer to the material plane."))
    {
        you.redraw_evasion = true;
    }

    _decrement_a_duration(DUR_POWERED_BY_DEATH, delay,
                          "You feel less regenerative.");
    if (you.duration[DUR_POWERED_BY_DEATH] > 0)
        handle_pbd_corpses(true);

    _decrement_a_duration(DUR_TELEPATHY, delay, "You feel less empathic.");

    if (_decrement_a_duration(DUR_CONDENSATION_SHIELD, delay,
                              "Your icy shield evaporates.",
                              coinflip(),
                              "Your icy shield starts to melt."))
    {
        you.redraw_armour_class = true;
    }

    if (_decrement_a_duration(DUR_MAGIC_SHIELD, delay,
                              "Your magical shield disappears."))
    {
        you.redraw_armour_class = true;
    }

    if (_decrement_a_duration(DUR_STONESKIN, delay, "Your skin feels tender."))
        you.redraw_armour_class = true;

    if (_decrement_a_duration(DUR_TELEPORT, delay))
    {
        // Only to a new area of the abyss sometimes (for abyss teleports).
        you_teleport_now(true, one_chance_in(5));
        untag_followers();
    }

    _decrement_a_duration(DUR_CONTROL_TELEPORT, delay,
                          "You feel uncertain.", coinflip(),
                          "You start to feel a little uncertain.");

    if (_decrement_a_duration(DUR_DEATH_CHANNEL, delay,
                              "Your unholy channel expires.", coinflip(),
                              "Your unholy channel is weakening."))
    {
        you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = 0;
    }

    _decrement_a_duration(DUR_STEALTH, delay, "You feel less stealthy.");
    _decrement_a_duration(DUR_SLAYING, delay, "You feel less lethal.");

    if (_decrement_a_duration(DUR_INVIS, delay, "You flicker back into view.",
                              coinflip(), "You flicker for a moment."))
    {
        you.attribute[ATTR_INVIS_UNCANCELLABLE] = 0;
    }

    _decrement_a_duration(DUR_BARGAIN, delay, "You feel less charismatic.");
    _decrement_a_duration(DUR_CONF, delay, "You feel less confused.");
    _decrement_a_duration(DUR_LOWERED_MR, delay, "You feel less vulnerable to hostile enchantments.");
    _decrement_a_duration(DUR_SLIMIFY, delay, "You feel less slimy.",
                          coinflip(), "Your slime is starting to congeal.");
    if (_decrement_a_duration(DUR_MISLED, delay,
                              "Your thoughts are your own once more."))
    {
        end_mislead();
    }
    if (_decrement_a_duration(DUR_QUAD_DAMAGE, delay, NULL, 0,
                              "Quad Damage is wearing off."))
    {
        invalidate_agrid(true);
    }
    _decrement_a_duration(DUR_MIRROR_DAMAGE, delay,
                          "Your dark mirror aura disappears.");
    if (_decrement_a_duration(DUR_HEROISM, delay,
                          "You feel like a meek peon again."))
    {
            you.redraw_evasion      = true;
            you.redraw_armour_class = true;
    }
    _decrement_a_duration(DUR_FINESSE, delay, "Your hands slow down.");

    _decrement_a_duration(DUR_CONFUSING_TOUCH, delay,
                          ((string("Your ") + you.hand_name(true)) +
                          " stop glowing.").c_str());

    _decrement_a_duration(DUR_SURE_BLADE, delay,
                          "The bond with your blade fades away.");

    if (_decrement_a_duration(DUR_MESMERISED, delay,
                              "You break out of your daze.",
                              0, NULL, MSGCH_RECOVERY))
    {
        you.clear_beholders();
    }

    if (_decrement_a_duration(DUR_AFRAID, delay,
                              "Your fear fades away.",
                              0, NULL, MSGCH_RECOVERY))
    {
        you.clear_fearmongers();
    }

    dec_slow_player(delay);
    dec_exhaust_player(delay);
    dec_haste_player(delay);

    if (you.duration[DUR_LIQUEFYING] && !you.stand_on_solid_ground())
        you.duration[DUR_LIQUEFYING] = 1;

    if (_decrement_a_duration(DUR_LIQUEFYING, delay,
                              "The ground is no longer liquid beneath you."))
    {
        invalidate_agrid();
    }

    if (_decrement_a_duration(DUR_MIGHT, delay,
                              "You feel a little less mighty now."))
    {
        notify_stat_change(STAT_STR, -5, true, "might running out");
    }

    if (_decrement_a_duration(DUR_AGILITY, delay,
                              "You feel a little less agile now."))
    {
        notify_stat_change(STAT_DEX, -5, true, "agility running out");
    }

    if (_decrement_a_duration(DUR_BRILLIANCE, delay,
                              "You feel a little less clever now."))
    {
        notify_stat_change(STAT_INT, -5, true, "brilliance running out");
    }

    if (you.duration[DUR_BERSERK]
        && (_decrement_a_duration(DUR_BERSERK, delay)
            || you.hunger + 100 <= HUNGER_STARVING + BERSERK_NUTRITION))
    {
        mpr("You are no longer berserk.");
        you.duration[DUR_BERSERK] = 0;

        // Sometimes berserk leaves us physically drained.
        //
        // Chance of passing out:
        //     - mutation gives a large plus in order to try and
        //       avoid the mutation being a "death sentence" to
        //       certain characters.

        if (you.berserk_penalty != NO_BERSERK_PENALTY
            && one_chance_in(10 + player_mutation_level(MUT_BERSERK) * 25))
        {
            // Note the beauty of Trog!  They get an extra save that's at
            // the very least 20% and goes up to 100%.
            if (you.religion == GOD_TROG && x_chance_in_y(you.piety, 150)
                && !player_under_penance())
            {
                mpr("Trog's vigour flows through your veins.");
            }
            else
            {
                mpr("You pass out from exhaustion.", MSGCH_WARN);
                you.increase_duration(DUR_PARALYSIS, roll_dice(1,4));
                you.stop_constricting_all();
            }
        }

        if (!you.duration[DUR_PARALYSIS] && !you.petrified())
            mpr("You are exhausted.", MSGCH_WARN);

        if (you.species == SP_LAVA_ORC)
            mpr("You feel less hot-headed.");

        // This resets from an actual penalty or from NO_BERSERK_PENALTY.
        you.berserk_penalty = 0;

        int dur = 12 + roll_dice(2, 12);
        // For consistency with slow give exhaustion 2 times the nominal
        // duration.
        you.increase_duration(DUR_EXHAUSTED, dur * 2);

        notify_stat_change(STAT_STR, -5, true, "berserk running out");

        // Don't trigger too many hints mode messages.
        const bool hints_slow = Hints.hints_events[HINT_YOU_ENCHANTED];
        Hints.hints_events[HINT_YOU_ENCHANTED] = false;

        slow_player(dur);

        make_hungry(BERSERK_NUTRITION, true);
        you.hunger = max(HUNGER_STARVING - 100, you.hunger);

        // 1KB: No berserk healing.
        you.hp = (you.hp + 1) * 2 / 3;
        calc_hp();

        learned_something_new(HINT_POSTBERSERK);
        Hints.hints_events[HINT_YOU_ENCHANTED] = hints_slow;
        you.redraw_quiver = true; // Can throw again.
    }

    if (_decrement_a_duration(DUR_CORONA, delay) && !you.backlit())
        mpr("You are no longer glowing.", MSGCH_DURATION);

    // Leak piety from the piety pool into actual piety.
    // Note that changes of religious status without corresponding actions
    // (killing monsters, offering items, ...) might be confusing for characters
    // of other religions.
    // For now, though, keep information about what happened hidden.
    if (you.piety < MAX_PIETY && you.duration[DUR_PIETY_POOL] > 0
        && one_chance_in(5))
    {
        you.duration[DUR_PIETY_POOL]--;
        gain_piety(1, 1, true);

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE) || defined(DEBUG_PIETY)
        mpr("Piety increases by 1 due to piety pool.", MSGCH_DIAGNOSTICS);

        if (you.duration[DUR_PIETY_POOL] == 0)
            mpr("Piety pool is now empty.", MSGCH_DIAGNOSTICS);
#endif
    }

    if (you.duration[DUR_DISJUNCTION])
    {
        disjunction();
        _decrement_a_duration(DUR_DISJUNCTION, delay,
                              "The translocation energy dissipates.");
        if (!you.duration[DUR_DISJUNCTION])
            invalidate_agrid(true);
    }

    if (_decrement_a_duration(DUR_TORNADO_COOLDOWN, delay,
                              "The winds around you calm down."))
    {
        remove_tornado_clouds(MID_PLAYER);
    }
    // Should expire before flight.
    if (you.duration[DUR_TORNADO])
    {
        tornado_damage(&you, min(delay, you.duration[DUR_TORNADO]));
        _decrement_a_duration(DUR_TORNADO, delay,
                              "The winds around you start to calm down.");
        if (!you.duration[DUR_TORNADO])
            you.duration[DUR_TORNADO_COOLDOWN] = random_range(25, 35);
    }

    if (you.duration[DUR_FLIGHT])
    {
        if (!you.permanent_flight())
        {
            if (_decrement_a_duration(DUR_FLIGHT, delay,
                                      0,
                                      random2(6),
                                      "You are starting to lose your buoyancy."))
            {
                land_player();
            }
        }
        else if ((you.duration[DUR_FLIGHT] -= delay) <= 0)
        {
            // Just time out potions/spells/miscasts.
            you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 0;
            you.duration[DUR_FLIGHT] = 0;
        }
    }

    if (you.rotting > 0)
    {
        // XXX: Mummies have an ability (albeit an expensive one) that
        // can fix rotted HPs now... it's probably impossible for them
        // to even start rotting right now, but that could be changed. - bwr
        // It's not normal biology, so Cheibriados won't help.
        if (you.species == SP_MUMMY)
            you.rotting = 0;
        else if (x_chance_in_y(you.rotting, 20)
                 && !you.duration[DUR_DEATHS_DOOR])
        {
            mpr("You feel your flesh rotting away.", MSGCH_WARN);
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
        int resilience = 400;

        if (you.religion == GOD_CHEIBRIADOS && you.piety >= piety_breakpoint(0))
            resilience = resilience * 3 / 2;

        // Faster rotting when hungry.
        if (you.hunger_state < HS_SATIATED)
            resilience >>= HS_SATIATED - you.hunger_state;

        if (one_chance_in(resilience))
        {
            dprf("rot rate: 1/%d", resilience);
            mpr("You feel your flesh rotting away.", MSGCH_WARN);
            rot_hp(1);
            if (you.rotting > 0)
                you.rotting--;
        }
    }

    dec_disease_player(delay);

    if (you.duration[DUR_POISONING])
        dec_poison_player();

    if (you.duration[DUR_DEATHS_DOOR])
    {
        if (you.hp > allowed_deaths_door_hp())
        {
            you.hp = allowed_deaths_door_hp();
            you.redraw_hit_points = true;
        }

        if (_decrement_a_duration(DUR_DEATHS_DOOR, delay,
                              "Your life is in your own hands again!",
                              random2(6),
                              "Your time is quickly running out!"))
        {
            you.increase_duration(DUR_EXHAUSTED, roll_dice(1,3));
        }
    }

    if (_decrement_a_duration(DUR_DIVINE_STAMINA, delay))
        zin_remove_divine_stamina();

    if (_decrement_a_duration(DUR_DIVINE_VIGOUR, delay))
        elyvilon_remove_divine_vigour();

    _decrement_a_duration(DUR_REPEL_STAIRS_MOVE, delay);
    _decrement_a_duration(DUR_REPEL_STAIRS_CLIMB, delay);

    _decrement_a_duration(DUR_COLOUR_SMOKE_TRAIL, 1);

    if (_decrement_a_duration(DUR_SCRYING, delay,
                              "Your astral sight fades away."))
    {
        you.xray_vision = false;
    }

    _decrement_a_duration(DUR_LIFESAVING, delay,
                          "Your divine protection fades away.");

    if (_decrement_a_duration(DUR_DARKNESS, delay,
                          "The ambient light returns to normal.")
        || you.haloed())
    {
        if (you.duration[DUR_DARKNESS])
        {
            you.duration[DUR_DARKNESS] = 0;
            mpr("The divine light dispels your darkness!");
        }
        update_vision_range();
    }

    _decrement_a_duration(DUR_SHROUD_OF_GOLUBRIA, delay,
                          "Your shroud unravels.",
                          0,
                          "Your shroud begins to fray at the edges.");

    _decrement_a_duration(DUR_SENTINEL_MARK, delay,
                          "The sentinel's mark upon you fades away.");

    _decrement_a_duration(DUR_WEAK, delay,
                          "Your attacks no longer feel as feeble.");

    _decrement_a_duration(DUR_DIMENSION_ANCHOR, delay,
                          "You are no longer firmly anchored in space.");

    _decrement_a_duration(DUR_SICKENING, delay);

    _decrement_a_duration(DUR_ANTIMAGIC, delay,
                          "You regain control over your magic.");

    _decrement_a_duration(DUR_WATER_HOLD_IMMUNITY, delay);
    if (you.duration[DUR_WATER_HOLD])
        handle_player_drowning(delay);

    if (you.duration[DUR_FLAYED])
    {
        bool near_ghost = false;
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->type == MONS_FLAYED_GHOST && !mi->wont_attack()
                && you.see_cell(mi->pos()))
            {
                near_ghost = true;
                break;
            }
        }
        if (!near_ghost)
        {
            if (_decrement_a_duration(DUR_FLAYED, delay))
                heal_flayed_effect(&you);
        }
        else if (you.duration[DUR_FLAYED] < 80)
            you.duration[DUR_FLAYED] += div_rand_round(50, delay);
    }

    _decrement_a_duration(DUR_RETCHING, delay, "Your fit of retching subsides.");

    if (you.attribute[ATTR_NEXT_RECALL_INDEX] > 0)
        do_recall(delay);

    if (!env.sunlight.empty())
        process_sunlights();
}

static void _check_banished()
{
    if (you.banished)
    {
        ASSERT(brdepth[BRANCH_ABYSS] != -1);
        you.banished = false;
        if (!player_in_branch(BRANCH_ABYSS))
            mpr("You are cast into the Abyss!", MSGCH_BANISHMENT);
        else if (you.depth < brdepth[BRANCH_ABYSS])
            mpr("You are cast deeper into the Abyss!", MSGCH_BANISHMENT);
        else
            mpr("The Abyss bends around you!", MSGCH_BANISHMENT);
        more();
        banished(you.banished_by);
        you.banished_by.clear();
    }
}

static void _check_shafts()
{
    for (int i = 0; i < MAX_TRAPS; ++i)
    {
        trap_def &trap = env.trap[i];

        if (trap.type != TRAP_SHAFT)
            continue;

        ASSERT_IN_BOUNDS(trap.pos);

        handle_items_on_shaft(trap.pos, true);
    }
}

static void _check_sanctuary()
{
    if (env.sanctuary_time <= 0)
        return;

    decrease_sanctuary_radius();
}

// cjo: Handles player hp and mp regeneration. If the counter you.hit_points_regeneration
// is over 100, a loop restores 1 hp and decreases the counter by 100 (so you can regen
// more than 1 hp per turn). If the counter is below 100, it is increased by a variable
// calculated from delay, BASELINE_DELAY, and your regeneration rate. MP regeneration happens
// similarly, but the countup depends on delay, BASELINE_DELAY, and you.max_magic_points
static void _regenerate_hp_and_mp(int delay)
{
    if (crawl_state.disables[DIS_PLAYER_REGEN])
        return;

    // XXX: using an int tmp to fix the fact that hit_points_regeneration
    // is only an unsigned char and is thus likely to overflow. -- bwr
    int tmp = you.hit_points_regeneration;

    if (you.hp < you.hp_max && !you.duration[DUR_DEATHS_DOOR])
    {
        const int base_val = player_regen();
        tmp += div_rand_round(base_val * delay, BASELINE_DELAY);
    }

    while (tmp >= 100)
    {
        // at low mp, "mana link" restores mp in place of hp
        if (you.mutation[MUT_MANA_LINK]
            && !x_chance_in_y(you.magic_points, you.max_magic_points))
        {
            inc_mp(1);
        }
        else // standard hp regeneration
            inc_hp(1);
        tmp -= 100;
    }

    ASSERT_RANGE(tmp, 0, 100);
    you.hit_points_regeneration = tmp;

    // XXX: Don't let DD use guardian spirit for free HP, since their
    // damage shaving is enough. (due, dpeg)
    if (you.spirit_shield() && you.species == SP_DEEP_DWARF)
        return;

    // XXX: Doing the same as the above, although overflow isn't an
    // issue with magic point regeneration, yet. -- bwr
    tmp = you.magic_points_regeneration;

    if (you.magic_points < you.max_magic_points)
    {
        const int base_val = 7 + you.max_magic_points / 2;
        int mp_regen_countup = div_rand_round(base_val * delay, BASELINE_DELAY);
        if (you.mutation[MUT_MANA_REGENERATION])
            mp_regen_countup *= 2;
        tmp += mp_regen_countup;
    }

    while (tmp >= 100)
    {
        inc_mp(1);
        tmp -= 100;
    }

    ASSERT_RANGE(tmp, 0, 100);
    you.magic_points_regeneration = tmp;
}

static void _update_mold_state(const coord_def & pos)
{
    if (coinflip())
    {
        // Doing a weird little state thing with the two mold
        // fprops. 'glowing' mold should turn back to normal after
        // a couple display update (i.e. after the player makes their
        // next move), since we happen to have two bits dedicated to
        // mold now we may as well use them? -cao
        if (env.pgrid(pos) & FPROP_MOLD)
            env.pgrid(pos) &= ~FPROP_MOLD;
        else
        {
            env.pgrid(pos) |= FPROP_MOLD;
            env.pgrid(pos) &= ~FPROP_GLOW_MOLD;
        }
    }
}

static void _update_mold()
{
    env.level_state &= ~LSTATE_GLOW_MOLD; // we'll restore it if any

    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (glowing_mold(*ri))
        {
            _update_mold_state(*ri);
            env.level_state |= LSTATE_GLOW_MOLD;
        }
    }
    for (monster_iterator mon_it; mon_it; ++mon_it)
    {
        if (mon_it->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
        {
            for (radius_iterator rad_it(mon_it->pos(),
                                        2, true, false); rad_it; ++rad_it)
            {
                // A threshold greater than 5, less than 8 on distance
                // matches the blast of a radius 2 explosion.
                int range = distance2(mon_it->pos(), *rad_it);
                if (range < 6 && is_moldy(*rad_it))
                {
                    env.pgrid(*rad_it) |= FPROP_MOLD;
                    env.pgrid(*rad_it) |= FPROP_GLOW_MOLD;
                }
            }
            env.level_state |= LSTATE_GLOW_MOLD;
        }
    }
}

static void _player_reacts()
{
    search_around();

    stealth = check_stealth();

#ifdef DEBUG_STEALTH
    // Too annoying for regular diagnostics.
    mprf(MSGCH_DIAGNOSTICS, "stealth: %d", stealth);
#endif

    if (you.attribute[ATTR_SHADOWS])
        shadow_lantern_effect();

    if (you.species == SP_LAVA_ORC)
        temperature_check();

    if (player_mutation_level(MUT_DEMONIC_GUARDIAN))
        check_demonic_guardian();

    if (you.unrand_reacts != 0)
        unrand_reacts();

    if (you.attribute[ATTR_NOISES])
        noisy_equipment();

    if (one_chance_in(10))
    {
        const int teleportitis_level = player_teleport();
        // this is instantaneous
        if (teleportitis_level > 0 && one_chance_in(100 / teleportitis_level))
        {
            if (teleportitis_level >= 8)
                you_teleport_now(true);
            else
                you_teleport_now(true, false, false, teleportitis_level * 5);
        }
        else if (player_in_branch(BRANCH_ABYSS) && one_chance_in(80)
          && (!map_masked(you.pos(), MMT_VAULT) || one_chance_in(3)))
        {
            mpr("You are suddenly pulled into a different region of the Abyss!",
                MSGCH_BANISHMENT);
            you_teleport_now(false, true); // to new area of the Abyss

            // It's effectively a new level, make a checkpoint save so eventual
            // crashes lose less of the player's progress (and fresh new bad
            // mutations).
            save_game(false);
        }
        else if (you.form == TRAN_WISP)
            random_blink(false);
    }

    actor_apply_cloud(&you);

    slime_wall_damage(&you, you.time_taken);

    // Icy shield and armour melt over lava.
    if (grd(you.pos()) == DNGN_LAVA)
        expose_player_to_element(BEAM_LAVA);

    you.update_beholders();
    you.update_fearmongers();

    _decrement_durations();
    you.handle_constriction();

    // increment constriction durations
    you.accum_has_constricted();

    int capped_time = you.time_taken;
    if (you.walking && capped_time > BASELINE_DELAY)
        capped_time = BASELINE_DELAY;

    int food_use = player_hunger_rate();
    food_use = div_rand_round(food_use * capped_time, BASELINE_DELAY);

    if (food_use > 0 && you.hunger > 0)
    {
        make_hungry(food_use, true);
        if (you.duration[DUR_AMBROSIA])
        {
            if (food_use > you.duration[DUR_AMBROSIA])
                food_use = you.duration[DUR_AMBROSIA];
            you.duration[DUR_AMBROSIA] -= food_use;
            inc_mp(food_use);
        }
    }

    _regenerate_hp_and_mp(capped_time);

    recharge_rods(you.time_taken, false);

    // Reveal adjacent mimics.
    for (adjacent_iterator ai(you.pos(), false); ai; ++ai)
        discover_mimic(*ai);

    // Player stealth check.
    seen_monsters_react();

    update_stat_zero();

    // XOM now ticks from here, to increase his reaction time to tension.
    if (you.religion == GOD_XOM)
        xom_tick();
}

// Ran after monsters and clouds get to act.
static void _player_reacts_to_monsters()
{
    // In case Maurice managed to steal a needed item for example.
    if (!you_are_delayed())
        update_can_train();

    if (you.duration[DUR_FIRE_SHIELD] > 0)
        manage_fire_shield(you.time_taken);

    // penance checked there (as you can have antennae too)
    if (you.has_antennae(true) || you.religion == GOD_ASHENZARI)
        check_antennae_detect();

    if ((you.religion == GOD_ASHENZARI && !player_under_penance())
        || you.mutation[MUT_JELLY_GROWTH])
    {
        detect_items(-1);
    }

    if (you.duration[DUR_TELEPATHY])
        detect_creatures(1 + you.duration[DUR_TELEPATHY] /
                         (2 * BASELINE_DELAY), true);

    // We have to do the messaging here, because a simple wand of flame will
    // call _maybe_melt_player_enchantments twice. It also avoid duplicate
    // messages when melting because of several heating sources.
    string what;
    if (you.props.exists("melt_armour"))
    {
        what = "armour";
        you.props.erase("melt_armour");
    }

    if (you.props.exists("melt_shield"))
    {
        if (what != "")
            what += " and ";
        what += "shield";
        you.props.erase("melt_shield");
    }

    if (what != "")
        mprf(MSGCH_DURATION, "The heat melts your icy %s.", what.c_str());

    handle_starvation();
    _decrement_paralysis(you.time_taken);
    _decrement_petrification(you.time_taken);
    if (_decrement_a_duration(DUR_SLEEP, you.time_taken))
        you.awake();

    // Entering/leaving a suppression aura needs to redraw player stats
    // and generally recalculate some things that aren't generally recalc'd
    // with every step. This is how we detect crossing the threshold.
    if (you.props.exists("exists_if_suppressed") != you.suppressed())
    {
        // HP and MP generally aren't recalculated each step, so we do it now
        calc_hp_artefact();  // different from calc_hp()
        calc_mp();

        // Redraw everything that suppression might affect
        you.redraw_hit_points = true;
        you.redraw_magic_points = true;
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
        you.redraw_stats[STAT_STR] = true;
        you.redraw_stats[STAT_DEX] = true;
        you.redraw_stats[STAT_INT] = true;
        notify_stat_change("suppression");

        if (you.suppressed())
            you.props["exists_if_suppressed"] = true;
        else
           you.props.erase("exists_if_suppressed");
    }
}

static void _update_golubria_traps()
{
    vector<coord_def> traps = find_golubria_on_level();
    for (vector<coord_def>::const_iterator it = traps.begin(); it != traps.end(); ++it)
    {
        trap_def *trap = find_trap(*it);
        if (trap && trap->type == TRAP_GOLUBRIA)
        {
            if (--trap->ammo_qty <= 0)
            {
                if (you.see_cell(*it))
                    mpr("Your passage of Golubria closes with a snap!");
                else
                    mpr("You hear a snapping sound.", MSGCH_SOUND);
                trap->destroy();
                noisy(8, *it);
            }
        }
    }
    if (traps.empty())
        env.level_state &= ~LSTATE_GOLUBRIA;
}

void world_reacts()
{
    // All markers should be activated at this point.
    ASSERT(!env.markers.need_activate());

    fire_final_effects();

    if (crawl_state.viewport_monster_hp)
    {
        crawl_state.viewport_monster_hp = false;
        viewwindow();
    }

    update_monsters_in_view();

    reset_show_terrain();

    crawl_state.clear_mon_acting();

    if (!crawl_state.game_is_arena())
    {
        you.turn_is_over = true;
        religion_turn_end();
        crawl_state.clear_god_acting();
    }

#ifdef USE_TILE
    if (crawl_state.game_is_hints())
    {
        tiles.clear_text_tags(TAG_TUTORIAL);
        tiles.place_cursor(CURSOR_TUTORIAL, NO_CURSOR);
    }
#endif

    _check_banished();
    _check_shafts();
    _check_sanctuary();

    run_environment_effects();

    if (!crawl_state.game_is_arena())
        _player_reacts();

    abyss_morph(you.time_taken);
    apply_noises();
    handle_monsters(true);

    _check_banished();

    ASSERT(you.time_taken >= 0);
    you.elapsed_time += you.time_taken;
    if (you.elapsed_time >= 2*1000*1000*1000)
    {
        // 2B of 1/10 turns.  A 32-bit signed int can hold 2.1B.
        // The worst case of mummy scumming had 92M turns, the second worst
        // merely 8M.  This limit is ~200M turns, with an efficient bot that
        // keeps resting on a fast machine, it takes ~24 hours to hit it
        // on a level with no monsters, at 100% CPU utilization, producing
        // a gigabyte of bzipped ttyrec.
        // We could extend the counters to 64 bits, but in the light of the
        // above, it's an useless exercise.
        mpr("Outside, the world ends.");
        mpr("Sorry, but your quest for the Orb is now rather pointless. "
            "You quit...");
        // Please do not give it a custom ktyp or make it cool in any way
        // whatsoever, because players are insane.  Usually, not being dragged
        // down by sanity is good, but this is not the case here.
        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_QUITTING);
    }

    handle_time();
    manage_clouds();
    if (env.level_state & LSTATE_GLOW_MOLD)
        _update_mold();
    if (env.level_state & LSTATE_GOLUBRIA)
        _update_golubria_traps();

    if (crawl_state.game_is_zotdef() && you.num_turns == 100)
        zotdef_set_wave();

    // Zotdef spawns only in the main dungeon
    if (crawl_state.game_is_zotdef()
        && player_in_branch(root_branch)
        && you.num_turns > 100)
    {
        zotdef_bosses_check();
        for (int i = 0; i < ZOTDEF_SPAWN_SIZE; i++)
        {
            // Reduce critter frequency for first wave
            if (you.num_turns<ZOTDEF_CYCLE_LENGTH && one_chance_in(3))
                continue;

            if ((you.num_turns % ZOTDEF_CYCLE_LENGTH > ZOTDEF_CYCLE_INTERVAL)
                && x_chance_in_y((you.num_turns % ZOTDEF_CYCLE_LENGTH), ZOTDEF_CYCLE_LENGTH*3))
            {
                zotdef_spawn(false);
            }
        }
    }
    if (!crawl_state.game_is_arena())
        _player_reacts_to_monsters();

    viewwindow();

    if (you.cannot_act() && any_messages()
        && crawl_state.repeat_cmd != CMD_WIZARD)
    {
        more();
    }

#if defined(DEBUG_TENSION) || defined(DEBUG_RELIGION)
    if (you.religion != GOD_NO_GOD)
        mprf(MSGCH_DIAGNOSTICS, "TENSION = %d", get_tension());
#endif

    if (you.num_turns != -1)
    {
        // Zotdef: Time only passes in the hall of zot
        if ((!crawl_state.game_is_zotdef() || player_in_branch(root_branch))
            && you.num_turns < INT_MAX)
        {
            you.num_turns++;
        }

        if (env.turns_on_level < INT_MAX)
            env.turns_on_level++;
        record_turn_timestamp();
        update_turn_count();
        msgwin_new_turn();
        crawl_state.lua_calls_no_turn = 0;
        if (crawl_state.game_is_sprint()
            && !(you.num_turns % 256)
            && !you_are_delayed())
        {
            // Resting makes the saving quite random, but meh.
            save_game(false);
        }
    }
}

static command_type _get_next_cmd()
{
#ifdef DGL_SIMPLE_MESSAGING
    check_messages();
#endif

#ifdef DEBUG_DIAGNOSTICS
    // Save hunger at start of round for use with hunger "delta-meter"
    // in output.cc.
    you.old_hunger = you.hunger;
#endif

#ifdef DEBUG_ITEM_SCAN
    debug_item_scan();
#endif
#ifdef DEBUG_MONS_SCAN
    debug_mons_scan();
#endif

    _center_cursor();

    keycode_type keyin = _get_next_keycode();

    handle_real_time();

    if (is_userfunction(keyin))
    {
        run_macro(get_userfunction(keyin).c_str());
        return CMD_NEXT_CMD;
    }

    return _keycode_to_command(keyin);
}

// We handle the synthetic keys, key_to_command() handles the
// real ones.
static command_type _keycode_to_command(keycode_type key)
{
    switch (key)
    {
#ifdef USE_TILE
    case CK_MOUSE_CMD:    return CMD_NEXT_CMD;
#endif

    case KEY_MACRO_DISABLE_MORE: return CMD_DISABLE_MORE;
    case KEY_MACRO_ENABLE_MORE:  return CMD_ENABLE_MORE;

    default:
        return key_to_command(key, KMC_DEFAULT);
    }
}

static keycode_type _get_next_keycode()
{
    keycode_type keyin;

    flush_input_buffer(FLUSH_BEFORE_COMMAND);

    mouse_control mc(MOUSE_MODE_COMMAND);
    keyin = unmangle_direction_keys(getch_with_command_macros());

    // This is the main mesclr() with Option.clear_messages.
    if (!is_synthetic_key(keyin))
        mesclr();

    return keyin;
}

// Check squares adjacent to player for given feature and return how
// many there are.  If there's only one, return the dx and dy.
static int _check_adjacent(dungeon_feature_type feat, coord_def& delta)
{
    int num = 0;

    vector<coord_def> doors;
    for (adjacent_iterator ai(you.pos(), true); ai; ++ai)
    {
        if (grd(*ai) == feat)
        {
            // Specialcase doors to take into account gates.
            if (feat_is_door(feat))
            {
                bool found_door = false;
                for (unsigned int i = 0; i < doors.size(); ++i)
                {
                    if (doors[i] == *ai)
                    {
                        found_door = true;
                        break;
                    }
                }

                // Already included in a gate, skip this door.
                if (found_door)
                    continue;

                // Check if it's part of a gate. If so, remember all its doors.
                set<coord_def> all_door;
                find_connected_identical(*ai, grd(*ai), all_door);
                for (set<coord_def>::const_iterator dc = all_door.begin();
                     dc != all_door.end(); ++dc)
                {
                     doors.push_back(*dc);
                }
            }

            num++;
            delta = *ai - you.pos();
        }
    }

    return num;
}

// Handles some aspects of untrapping. Returns false if the target is a
// closed door that will need to be opened.
static bool _untrap_target(const coord_def move, bool check_confused)
{
    const coord_def target = you.pos() + move;
    monster* mon = monster_at(target);
    if (mon && player_can_hit_monster(mon))
    {
        you.turn_is_over = true;
        fight_melee(&you, mon);

        if (you.berserk_penalty != NO_BERSERK_PENALTY)
            you.berserk_penalty = 0;

        return true;
    }

    if (find_trap(target) && grd(target) != DNGN_UNDISCOVERED_TRAP)
    {
        if (!you.confused())
        {
            if (!form_can_wield())
            {
                mpr("You can't disarm traps in your present form.");
                return true;
            }

            const int cloud = env.cgrid(target);
            if (cloud != EMPTY_CLOUD
                && is_damaging_cloud(env.cloud[ cloud ].type, true))
            {
                mpr("You can't get to that trap right now.");
                return true;
            }
        }

        // If you're confused, you may attempt it and stumble into the trap.
        disarm_trap(target);
        return true;
    }

    const dungeon_feature_type feat = grd(target);
    if (!feat_is_closed_door(feat) || you.confused())
    {
        switch (feat)
        {
        case DNGN_OPEN_DOOR:
            _close_door(move); // for convenience
            return true;
        default:
        {
            bool do_msg = true;

            // Don't waste a turn if feature is solid.
            if (feat_is_solid(feat) && !you.confused())
                return true;
            else
            {
                list<actor*> cleave_targets;
                if (you.weapon() && weapon_skill(*you.weapon()) == SK_AXES
                    && !you.confused())
                {
                    get_all_cleave_targets(&you, target, cleave_targets);
                }

                if (!cleave_targets.empty())
                {
                    targetter_cleave hitfunc(&you, target);
                    if (stop_attack_prompt(hitfunc, "attack"))
                        return true;

                    if (!you.fumbles_attack())
                        attack_cleave_targets(&you, cleave_targets);
                }
                else if (do_msg && !you.fumbles_attack())
                    mpr("You swing at nothing.");
                make_hungry(3, true);
                // Take the usual attack delay.
                melee_attack attk(&you, NULL);
                you.time_taken = attk.calc_attack_delay();
            }
            you.turn_is_over = true;
            return true;
        }
        }
    }

    // Else it's a closed door and needs further handling.
    return false;
}

// Opens doors and may also handle untrapping/attacking, etc.
// If either move_x or move_y are non-zero, the pair carries a specific
// direction for the door to be opened (eg if you type ctrl + dir).
static void _open_door(coord_def move, bool check_confused)
{
    ASSERT(!crawl_state.game_is_arena() && !crawl_state.arena_suspended);

    if (you.attribute[ATTR_HELD])
    {
        free_self_from_net();
        you.turn_is_over = true;
        return;
    }

    // The player used Ctrl + dir or a variant thereof.
    if (!move.origin())
    {
        if (check_confused && you.confused() && !one_chance_in(3))
            move = Compass[random2(8)];
        if (_untrap_target(move, check_confused))
            return;
    }

    // If we get here, the player either hasn't picked a direction yet,
    // or the chosen direction actually contains a closed door.
    if (!player_can_open_doors())
    {
        mpr("You can't open doors in your present form.");
        return;
    }

    dist door_move;

    // The player hasn't picked a direction yet.
    if (move.origin())
    {
        const int num = _check_adjacent(DNGN_CLOSED_DOOR, move)
                        + _check_adjacent(DNGN_RUNED_DOOR, move);

        if (num == 0)
        {
            mpr("There's nothing to open nearby.");
            return;
        }

        // If there's only one door to open, don't ask.
        if (num == 1)
            door_move.delta = move;
        else
        {
            mpr("Which direction?", MSGCH_PROMPT);
            direction_chooser_args args;
            args.restricts = DIR_DIR;
            direction(door_move, args);

            if (!door_move.isValid)
                return;
        }
    }
    else
        door_move.delta = move;

    if (check_confused && you.confused() && !one_chance_in(3))
        door_move.delta = Compass[random2(8)];

    // We got a valid direction.
    const coord_def doorpos = you.pos() + door_move.delta;
    const dungeon_feature_type feat = (in_bounds(doorpos) ? grd(doorpos)
                                                          : DNGN_UNSEEN);
    string door_already_open = "";
    if (in_bounds(doorpos))
    {
        door_already_open = env.markers.property_at(doorpos, MAT_ANY,
                                "door_verb_already_open");
    }

    if (!feat_is_closed_door(feat))
    {
        if (you.confused())
        {
            mpr("You swing at nothing.");
            make_hungry(3, true);
            you.turn_is_over = true;
            return;
        }
        switch (feat)
        {
        // This doesn't ever seem to be triggered.
        case DNGN_OPEN_DOOR:
            if (!door_already_open.empty())
                mpr(door_already_open.c_str());
            else
                mpr("It's already open!");
            break;
        default:
            mpr("There isn't anything that you can open there!");
            break;
        }
        // Don't lose a turn.
        return;
    }

    // Allow doors to be locked.
    const string door_veto_message = env.markers.property_at(doorpos, MAT_ANY,
                                                             "veto_reason");
    if (door_vetoed(doorpos))
    {
        if (door_veto_message.empty())
            mpr("The door is shut tight!");
        else
            mpr(door_veto_message.c_str());

        return;
    }

    if (feat == DNGN_SEALED_DOOR)
    {
        mpr("That door is sealed shut!");
        return;
    }

    // Finally, open the closed door!
    set<coord_def> all_door = connected_doors(doorpos);
    const char *adj, *noun;
    get_door_description(all_door.size(), &adj, &noun);

    const string door_desc_adj  =
        env.markers.property_at(doorpos, MAT_ANY, "door_description_adjective");
    const string door_desc_noun =
        env.markers.property_at(doorpos, MAT_ANY, "door_description_noun");
    if (!door_desc_adj.empty())
        adj = door_desc_adj.c_str();
    if (!door_desc_noun.empty())
        noun = door_desc_noun.c_str();

    if (!(check_confused && you.confused()))
    {
        string door_open_prompt =
            env.markers.property_at(doorpos, MAT_ANY, "door_open_prompt");

        bool ignore_exclude = false;

        if (!door_open_prompt.empty())
        {
            door_open_prompt += " (y/N)";
            if (!yesno(door_open_prompt.c_str(), true, 'n', true, false))
            {
                if (is_exclude_root(doorpos))
                    canned_msg(MSG_OK);
                else
                {
                    if (yesno("Put travel exclusion on door? (Y/n)",
                              true, 'y'))
                    {
                        // Zero radius exclusion right on top of door.
                        set_exclude(doorpos, 0);
                    }
                }
                interrupt_activity(AI_FORCE_INTERRUPT);
                return;
            }
            ignore_exclude = true;
        }

        if (!ignore_exclude && is_exclude_root(doorpos))
        {
            string prompt = make_stringf("This %s%s is marked as excluded! "
                                         "Open it anyway?", adj, noun);

            if (!yesno(prompt.c_str(), true, 'n', true, false))
            {
                canned_msg(MSG_OK);
                interrupt_activity(AI_FORCE_INTERRUPT);
                return;
            }
        }
    }

    int skill = you.dex() + you.skill_rdiv(SK_STEALTH);

    string berserk_open = env.markers.property_at(doorpos, MAT_ANY,
                                                  "door_berserk_verb_open");
    string berserk_adjective = env.markers.property_at(doorpos, MAT_ANY,
                                                       "door_berserk_adjective");
    string door_open_creak = env.markers.property_at(doorpos, MAT_ANY,
                                                     "door_noisy_verb_open");
    string door_airborne = env.markers.property_at(doorpos, MAT_ANY,
                                                   "door_airborne_verb_open");
    string door_open_verb = env.markers.property_at(doorpos, MAT_ANY,
                                                    "door_verb_open");

    if (you.berserk())
    {
        // XXX: Better flavour for larger doors?
        if (silenced(you.pos()))
        {
            if (!berserk_open.empty())
            {
                berserk_open += ".";
                mprf(berserk_open.c_str(), adj, noun);
            }
            else
                mprf("The %s%s flies open!", adj, noun);
        }
        else
        {
            if (!berserk_open.empty())
            {
                if (!berserk_adjective.empty())
                    berserk_open += " " + berserk_adjective;
                else
                    berserk_open += ".";
                mprf(MSGCH_SOUND, berserk_open.c_str(), adj, noun);
            }
            else
                mprf(MSGCH_SOUND, "The %s%s flies open with a bang!", adj, noun);
            noisy(15, you.pos());
        }
    }
    else if (one_chance_in(skill) && !silenced(you.pos()))
    {
        if (!door_open_creak.empty())
            mprf(MSGCH_SOUND, door_open_creak.c_str(), adj, noun);
        else
        {
            mprf(MSGCH_SOUND, "As you open the %s%s, it creaks loudly!",
                 adj, noun);
        }
        noisy(10, you.pos());
    }
    else
    {
        const char* verb;
        if (you.airborne())
        {
            if (!door_airborne.empty())
                verb = door_airborne.c_str();
            else
                verb = "You reach down and open the %s%s.";
        }
        else
        {
            if (!door_open_verb.empty())
               verb = door_open_verb.c_str();
            else
               verb = "You open the %s%s.";
        }

        mprf(verb, adj, noun);
    }

    vector<coord_def> excludes;
    for (set<coord_def>::iterator i = all_door.begin(); i != all_door.end(); ++i)
    {
        const coord_def& dc = *i;
        // Even if some of the door is out of LOS, we want the entire
        // door to be updated.  Hitting this case requires a really big
        // door!
        if (env.map_knowledge(dc).seen())
        {
            env.map_knowledge(dc).set_feature(DNGN_OPEN_DOOR);
#ifdef USE_TILE
            env.tile_bk_bg(dc) = TILE_DNGN_OPEN_DOOR;
#endif
        }
        grd(dc) = DNGN_OPEN_DOOR;
        set_terrain_changed(dc);
        dungeon_events.fire_position_event(DET_DOOR_OPENED, dc);
        if (is_excluded(dc))
            excludes.push_back(dc);
    }

    update_exclusion_los(excludes);
    viewwindow();

    you.turn_is_over = true;
}

static void _close_door(coord_def move)
{
    if (!player_can_open_doors())
    {
        mpr("You can't close doors in your present form.");
        return;
    }

    if (you.attribute[ATTR_HELD])
    {
        mprf("You can't close doors while %s.", held_status());
        return;
    }

    dist door_move;

    // The player hasn't yet told us a direction.
    if (move.origin())
    {
        // If there's only one door to close, don't ask.
        int num = _check_adjacent(DNGN_OPEN_DOOR, move);
        if (num == 0)
        {
            mpr("There's nothing to close nearby.");
            return;
        }
        else if (num == 1)
            door_move.delta = move;
        else
        {
            mpr("Which direction?", MSGCH_PROMPT);
            direction_chooser_args args;
            args.restricts = DIR_DIR;
            direction(door_move, args);

            if (!door_move.isValid)
                return;
        }
    }
    else
        door_move.delta = move;

    if (you.confused() && !one_chance_in(3))
        door_move.delta = Compass[random2(8)];

    if (door_move.delta.origin())
    {
        mpr("You can't close doors on yourself!");
        return;
    }

    const coord_def doorpos = you.pos() + door_move.delta;
    const dungeon_feature_type feat = (in_bounds(doorpos) ? grd(doorpos)
                                                          : DNGN_UNSEEN);

    string berserk_close = env.markers.property_at(doorpos, MAT_ANY,
                                                   "door_berserk_verb_close");
    string berserk_adjective = env.markers.property_at(doorpos, MAT_ANY,
                                                       "door_berserk_adjective");
    string door_close_creak = env.markers.property_at(doorpos, MAT_ANY,
                                                      "door_noisy_verb_close");
    string door_airborne = env.markers.property_at(doorpos, MAT_ANY,
                                                   "door_airborne_verb_close");
    string door_close_verb = env.markers.property_at(doorpos, MAT_ANY,
                                                     "door_verb_close");

    if (feat == DNGN_OPEN_DOOR)
    {
        set<coord_def> all_door;
        find_connected_identical(doorpos, grd(doorpos), all_door);
        const char *adj, *noun;
        get_door_description(all_door.size(), &adj, &noun);
        const string waynoun_str = make_stringf("%sway", noun);
        const char *waynoun = waynoun_str.c_str();

        const string door_desc_adj  =
            env.markers.property_at(doorpos, MAT_ANY,
                                    "door_description_adjective");
        const string door_desc_noun =
            env.markers.property_at(doorpos, MAT_ANY,
                                    "door_description_noun");
        if (!door_desc_adj.empty())
            adj = door_desc_adj.c_str();
        if (!door_desc_noun.empty())
        {
            noun = door_desc_noun.c_str();
            waynoun = noun;
        }

        for (set<coord_def>::const_iterator i = all_door.begin();
             i != all_door.end(); ++i)
        {
            const coord_def& dc = *i;
            if (monster* mon = monster_at(dc))
            {
                // Need to make sure that turn_is_over is set if
                // creature is invisible.
                if (!you.can_see(mon))
                {
                    mprf("Something is blocking the %s!", waynoun);
                    you.turn_is_over = true;
                }
                else
                    mprf("There's a creature in the %s!", waynoun);
                return;
            }

            if (igrd(dc) != NON_ITEM)
            {
                mprf("There's something blocking the %s.", waynoun);
                return;
            }

            if (you.pos() == dc)
            {
                mprf("There's a thick-headed creature in the %s!", waynoun);
                return;
            }
        }

        int skill = you.dex() + you.skill_rdiv(SK_STEALTH);

        if (you.berserk())
        {
            if (silenced(you.pos()))
            {
                if (!berserk_close.empty())
                {
                    berserk_close += ".";
                    mprf(berserk_close.c_str(), adj, noun);
                }
                else
                    mprf("You slam the %s%s shut!", adj, noun);
            }
            else
            {
                if (!berserk_close.empty())
                {
                    if (!berserk_adjective.empty())
                        berserk_close += " " + berserk_adjective;
                    else
                        berserk_close += ".";
                    mprf(MSGCH_SOUND, berserk_close.c_str(), adj, noun);
                }
                else
                    mprf(MSGCH_SOUND, "You slam the %s%s shut with a bang!", adj, noun);
                noisy(15, you.pos());
            }
        }
        else if (one_chance_in(skill) && !silenced(you.pos()))
        {
            if (!door_close_creak.empty())
                mprf(MSGCH_SOUND, door_close_creak.c_str(), adj, noun);
            else
                mprf(MSGCH_SOUND, "As you close the %s%s, it creaks loudly!",
                     adj, noun);
            noisy(10, you.pos());
        }
        else
        {
            const char* verb;
            if (you.airborne())
            {
                if (!door_airborne.empty())
                    verb = door_airborne.c_str();
                else
                    verb = "You reach down and close the %s%s.";
            }
            else
            {
                if (!door_close_verb.empty())
                   verb = door_close_verb.c_str();
                else
                    verb = "You close the %s%s.";
            }

            mprf(verb, adj, noun);
        }

        vector<coord_def> excludes;
        for (set<coord_def>::const_iterator i = all_door.begin();
             i != all_door.end(); ++i)
        {
            const coord_def& dc = *i;
            // Once opened, formerly runed doors become normal doors.
            grd(dc) = DNGN_CLOSED_DOOR;
            set_terrain_changed(dc);
            dungeon_events.fire_position_event(DET_DOOR_CLOSED, dc);

            // Even if some of the door is out of LOS once it's closed
            // (or even if some of it is out of LOS when it's open), we
            // want the entire door to be updated.
            if (env.map_knowledge(dc).seen())
            {
                env.map_knowledge(dc).set_feature(DNGN_CLOSED_DOOR);
#ifdef USE_TILE
                env.tile_bk_bg(dc) = TILE_DNGN_CLOSED_DOOR;
#endif
            }
            if (is_excluded(dc))
                excludes.push_back(dc);
        }

        update_exclusion_los(excludes);
        you.turn_is_over = true;
    }
    else if (you.confused())
        _open_door(door_move.delta);
    else
    {
        switch (feat)
        {
        case DNGN_CLOSED_DOOR:
        case DNGN_RUNED_DOOR:
            mpr("It's already closed!");
            break;
        default:
            mpr("There isn't anything that you can close there!");
            break;
        }
    }
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
static void _do_berserk_no_combat_penalty(void)
{
    // Butchering/eating a corpse will maintain a blood rage.
    const int delay = current_delay_action();
    if (delay == DELAY_BUTCHER || delay == DELAY_EAT)
        return;

    if (you.berserk_penalty == NO_BERSERK_PENALTY)
        return;

    if (you.berserk())
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

        const int berserk_base_delay = BASELINE_DELAY;
        int berserk_delay_penalty = you.berserk_penalty * berserk_base_delay;

        you.duration[DUR_BERSERK] -= berserk_delay_penalty;
        // Don't actually expire berserk until the next turn.
        if (you.duration[DUR_BERSERK] < 1)
            you.duration[DUR_BERSERK] = 1;
    }
    return;
}

// Called when the player moves by walking/running. Also calls attack
// function etc when necessary.
static void _move_player(int move_x, int move_y)
{
    _move_player(coord_def(move_x, move_y));
}

static void _move_player(coord_def move)
{
    ASSERT(!crawl_state.game_is_arena() && !crawl_state.arena_suspended);

    bool attacking = false;
    bool moving = true;         // used to prevent eventual movement (swap)
    bool swap = false;

    if (you.attribute[ATTR_HELD])
    {
        free_self_from_net();
        you.turn_is_over = true;
        return;
    }

    // When confused, sometimes make a random move
    if (you.confused())
    {
        dungeon_feature_type dangerous = DNGN_FLOOR;
        monster *bad_mons = 0;
        string bad_suff, bad_adj;
        for (adjacent_iterator ai(you.pos(), false); ai; ++ai)
        {
            if (is_feat_dangerous(grd(*ai)) && !you.can_cling_to(*ai)
                && (dangerous == DNGN_FLOOR || grd(*ai) == DNGN_LAVA))
            {
                dangerous = grd(*ai);
                break;
            }
            else
            {
                string suffix, adj;
                monster *mons = monster_at(*ai);
                if (mons && bad_attack(mons, adj, suffix))
                {
                    bad_mons = mons;
                    bad_suff = suffix;
                    bad_adj = adj;
                    break;
                }
            }
        }

        if (you.form == TRAN_TREE)
            dangerous = DNGN_FLOOR; // still warn about allies

        if (dangerous != DNGN_FLOOR || bad_mons)
        {
            string prompt = "Are you sure you want to stumble around while "
                            "confused and next to ";

            if (dangerous != DNGN_FLOOR)
                prompt += (dangerous == DNGN_LAVA ? "lava" : "deep water");
            else
            {
                string name = bad_mons->name(DESC_PLAIN);
                if (name.find("the ") == 0)
                    name.erase(0, 4);
                if (bad_adj.find("your") != 0)
                    bad_adj = "the " + bad_adj;
                prompt += bad_adj + name + bad_suff;
            }
            prompt += "?";

            monster* targ = monster_at(you.pos() + move);
            if (targ && !targ->wont_attack() && you.can_see(targ))
                prompt += " (Use ctrl+direction to attack without moving)";

            if (!crawl_state.disables[DIS_CONFIRMATIONS]
                && !yesno(prompt.c_str(), false, 'n'))
            {
                canned_msg(MSG_OK);
                return;
            }
        }

        if (!one_chance_in(3))
        {
            move.x = random2(3) - 1;
            move.y = random2(3) - 1;
            you.reset_prev_move();
        }

        const coord_def& new_targ = you.pos() + move;
        if (!in_bounds(new_targ) || !you.can_pass_through(new_targ))
        {
            you.walking = move.abs();
            you.turn_is_over = true;
            mpr("Ouch!");
            apply_berserk_penalty = true;
            crawl_state.cancel_cmd_repeat();

            return;
        }
    }
    else if (you.form == TRAN_WISP && one_chance_in(10))
    {
        // Full confusion appears to be a pain in the rear, and monster
        // wisps don't attack allies either.  Thus, you can get redirected
        // only into empty places or towards enemies.
        coord_def dir = Compass[random2(8)];
        coord_def targ = you.pos() + dir;
        monster *dude = monster_at(targ);
        if (in_bounds(targ) && (dude? !dude->wont_attack() :
                                      you.can_pass_through(targ)))
        {
            move = dir;
        }
    }

    const coord_def& targ = you.pos() + move;

    // You can't walk out of bounds!
    if (!in_bounds(targ))
        return;

    dungeon_feature_type targ_grid = grd(targ);

    monster* targ_monst = monster_at(targ);
    if (fedhas_passthrough(targ_monst))
    {
        // Moving on a plant takes 1.5 x normal move delay. We
        // will print a message about it but only when moving
        // from open space->plant (hopefully this will cut down
        // on the message spam).
        you.time_taken = div_rand_round(you.time_taken * 3, 2);

        monster* current = monster_at(you.pos());
        if (!current || !fedhas_passthrough(current))
        {
            // Probably need a better message. -cao
            mprf("You walk carefully through the %s.",
                 mons_genus(targ_monst->type) == MONS_FUNGUS ? "fungus"
                                                             : "plants");
        }
        targ_monst = NULL;
    }

    bool targ_pass = you.can_pass_through(targ) && you.form != TRAN_TREE;

    // You can swap places with a friendly or good neutral monster if
    // you're not confused, or if both of you are inside a sanctuary.
    const bool can_swap_places = targ_monst
                                 && !mons_is_stationary(targ_monst)
                                 && (targ_monst->wont_attack()
                                       && !you.confused()
                                     || is_sanctuary(you.pos())
                                        && is_sanctuary(targ));

    // You cannot move away from a mermaid but you CAN fight monsters on
    // neighbouring squares.
    monster* beholder = NULL;
    if (!you.confused())
        beholder = you.get_beholder(targ);

    // You cannot move closer to a fear monger.
    monster *fmonger = NULL;
    if (!you.confused())
        fmonger = you.get_fearmonger(targ);

    if (you.running.check_stop_running())
    {
        // [ds] Do we need this? Shouldn't it be false to start with?
        you.turn_is_over = false;
        return;
    }

    coord_def mon_swap_dest;

    string verb;
    if (you.flight_mode())
        verb = "fly";
    else if (you.is_wall_clinging())
        verb = "cling";
    else if (you.species == SP_NAGA && !form_changed_physiology())
        verb = "slither";
    else
        verb = "walk";

    if (targ_monst && !targ_monst->submerged())
    {
        if (can_swap_places && !beholder && !fmonger)
        {
            if (swap_check(targ_monst, mon_swap_dest))
                swap = true;
            else
                moving = false;
        }
        else if (!can_swap_places) // attack!
        {
            // XXX: Moving into a normal wall does nothing and uses no
            // turns or energy, but moving into a wall which contains
            // an invisible monster attacks the monster, thus allowing
            // the player to figure out which adjacent wall an invis
            // monster is in "for free".

            // Don't allow the player to freely locate invisible monsters
            // with confirmation prompts.
            if (!you.can_see(targ_monst) && !check_moveto(targ, verb))
            {
                stop_running();
                you.turn_is_over = false;
                return;
            }

            you.turn_is_over = true;
            fight_melee(&you, targ_monst);

            // We don't want to create a penalty if there isn't
            // supposed to be one.
            if (you.berserk_penalty != NO_BERSERK_PENALTY)
                you.berserk_penalty = 0;

            attacking = true;
        }
    }
    else if (you.form == TRAN_FUNGUS && moving)
    {
        if (you.made_nervous_by(targ))
        {
            mpr("You're too terrified to move while being watched!");
            stop_running();
            moving = false;
            you.turn_is_over = false;
            return;
        }
    }

    if (you.form == TRAN_JELLY && feat_is_closed_door(targ_grid))
    {
        mpr("You eat the door.");
        nuke_wall(targ);
        targ_pass = true;
        targ_grid = grd(targ);
    }

    if (!attacking && targ_pass && moving && !beholder && !fmonger)
    {
        if (crawl_state.game_is_zotdef() && you.pos() == env.orb_pos)
        {
            // Are you standing on the Orb? If so, are the critters near?
            bool danger = false;
            for (int i = 0; i < MAX_MONSTERS; ++i)
            {
                monster& mon = menv[i];
                if (you.can_see(&mon) && !mon.friendly() &&
                    !mons_is_firewood(&mon) &&
                    (grid_distance(you.pos(), mon.pos()) < 4))
                {
                    danger = true;
                }
            }

            if (danger && !player_has_orb())
            {
                string prompt = "Are you sure you want to leave the Orb unguarded?";
                if (!yesno(prompt.c_str(), false, 'n'))
                {
                    canned_msg(MSG_OK);
                    return;
                }
            }
        }

        if (!you.attempt_escape()) // false means constricted and did not escape
            return;

        if (!you.confused() && !check_moveto(targ, verb))
        {
            stop_running();
            you.turn_is_over = false;
            return;
        }

        if (you.duration[DUR_WATER_HOLD])
        {
            if (you.can_swim())
                mpr("You deftly slip free of the water engulfing you.");
            else //Unless you're a natural swimmer, this takes longer than normal
            {
                mpr("With effort, you pull free of the water engulfing you.");
                you.time_taken = you.time_taken * 3 / 2;
            }
            you.duration[DUR_WATER_HOLD] = 1;
            you.props.erase("water_holder");
        }

        if (swap)
            swap_places(targ_monst, mon_swap_dest);
        else if (you.duration[DUR_COLOUR_SMOKE_TRAIL])
        {
            check_place_cloud(CLOUD_MAGIC_TRAIL, you.pos(),
                random_range(3, 10), &you, 0, ETC_RANDOM);
        }

        if (delay_is_run(current_delay_action()) && env.travel_trail.empty())
            env.travel_trail.push_back(you.pos());
        else if (!delay_is_run(current_delay_action()))
            clear_travel_trail();

        you.time_taken *= player_movement_speed();
        you.time_taken = div_rand_round(you.time_taken, 10);

        if (you.running && you.running.travel_speed)
        {
            you.time_taken = max(you.time_taken,
                                 div_round_up(100, you.running.travel_speed));
        }

#ifdef EUCLIDEAN
        if (move.abs() == 2)
            you.time_taken *= 1.4;
#endif

        // clear constriction data
        you.stop_constricting_all(true);
        you.stop_being_constricted();

        move_player_to_grid(targ, true, false);

        if (delay_is_run(current_delay_action()))
            env.travel_trail.push_back(you.pos());

        you.walking = move.abs();
        you.prev_move = move;
        move.reset();
        you.turn_is_over = true;
        request_autopickup();
    }

    // BCR - Easy doors single move
    if (Options.easy_open && !attacking && feat_is_closed_door(targ_grid))
    {
        _open_door(move.x, move.y, false);
        you.prev_move = move;
    }
    else if (!targ_pass && grd(targ) == DNGN_MALIGN_GATEWAY && !attacking)
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !_prompt_dangerous_portal(grd(targ)))
        {
            return;
        }

        you.prev_move = move;
        move.reset();
        you.turn_is_over = true;

        entered_malign_portal(&you);
        return;
    }
    else if (!targ_pass && !attacking)
    {
        if (you.form == TRAN_TREE)
            canned_msg(MSG_CANNOT_MOVE);
        else if (grd(targ) == DNGN_OPEN_SEA)
            mpr("The ferocious winds and tides of the open sea thwart your progress.");
        else if (grd(targ) == DNGN_LAVA_SEA)
            mpr("The endless sea of lava is not a nice place.");
        else if (feat_is_tree(grd(targ)) && you.religion == GOD_FEDHAS)
            mpr("You cannot walk through the dense trees.");

        stop_running();
        move.reset();
        you.turn_is_over = false;
        crawl_state.cancel_cmd_repeat();
        return;
    }
    else if (beholder && !attacking)
    {
        mprf("You cannot move away from %s!",
            beholder->name(DESC_THE, true).c_str());
        stop_running();
        return;
    }
    else if (fmonger && !attacking)
    {
        mprf("You cannot move closer to %s!",
            fmonger->name(DESC_THE, true).c_str());
        stop_running();
        return;
    }

    if (you.running == RMODE_START)
        you.running = RMODE_CONTINUE;

    if (player_in_branch(BRANCH_ABYSS))
        maybe_shift_abyss_around_player();

    apply_berserk_penalty = !attacking;

    if (!attacking && you.religion == GOD_CHEIBRIADOS && one_chance_in(10)
        && you.run())
    {
        did_god_conduct(DID_HASTY, 1, true);
    }
}

static int _get_num_and_char_keyfun(int &ch)
{
    if (ch == CK_BKSP || isadigit(ch) || (unsigned)ch >= 128)
        return 1;

    return -1;
}

static int _get_num_and_char(const char* prompt, char* buf, int buf_len)
{
    if (prompt != NULL)
        mpr(prompt, MSGCH_PROMPT);

    line_reader reader(buf, buf_len);

    reader.set_keyproc(_get_num_and_char_keyfun);

    return reader.read_line(true);
}

static void _cancel_cmd_repeat()
{
    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();
    flush_input_buffer(FLUSH_REPLAY_SETUP_FAILURE);
}

static command_type _find_command(const keyseq& keys)
{
    // Add it back in to find out the command.
    macro_buf_add(keys, true);
    keycode_type keyin = unmangle_direction_keys(getch_with_command_macros());
    command_type cmd = _keycode_to_command(keyin);
    if (is_userfunction(keyin))
        cmd = CMD_NEXT_CMD;
    flush_input_buffer(FLUSH_REPEAT_SETUP_DONE);
    return cmd;
}

static void _check_cmd_repeat(int last_turn)
{
    if (last_turn == you.num_turns
        && crawl_state.repeat_cmd != CMD_WIZARD
        && (crawl_state.repeat_cmd != CMD_PREV_CMD_AGAIN
            || crawl_state.prev_cmd != CMD_WIZARD))
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
}

static void _run_input_with_keys(const keyseq& keys)
{
    ASSERT(crawl_state.is_replaying_keys());

    const int old_buf_size = get_macro_buf_size();

    macro_buf_add(keys, true);

    while (get_macro_buf_size() > old_buf_size
           && crawl_state.is_replaying_keys())
    {
        _input();
    }

    if (get_macro_buf_size() < old_buf_size)
    {
        mpr("(Key replay stole keys)", MSGCH_ERROR);
        crawl_state.cancel_cmd_all();
    }
}

static void _do_cmd_repeat()
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
    int ch = _get_num_and_char("Number of times to repeat, then command key: ",
                               buf, sizeof(buf));

    if (strlen(buf) == 0)
    {
        mpr("You must enter the number of times for the command to repeat.");
        _cancel_cmd_repeat();
        return;
    }
    int count = atoi(buf);

    if (count <= 0)
    {
        canned_msg(MSG_OK);
        _cancel_cmd_repeat();
        return;
    }

    // User can type space or enter and then the command key, in case
    // they want to repeat a command bound to a number key.
    c_input_reset(true);
    if (ch == ' ' || ch == CK_ENTER)
    {
        mpr("Enter command to be repeated: ", MSGCH_PROMPT);
        // Enable the cursor to read input.
        cursor_control con(true);

        ch = getchm();
    }

    // Read out key sequence.
    keyseq keys;
    do
    {
        keys.push_back(ch);
        ch = macro_buf_get();
    } while (ch != -1);

    const command_type cmd = _find_command(keys);
    command_type real_cmd = cmd;
    if (cmd == CMD_PREV_CMD_AGAIN)
        real_cmd = crawl_state.prev_cmd;

    if (real_cmd != CMD_MOUSE_MOVE)
        c_input_reset(false);

    if (!_cmd_is_repeatable(real_cmd))
    {
        _cancel_cmd_repeat();
        return;
    }

    keyseq repeat_keys;
    int i = 0;
    if (cmd != CMD_PREV_CMD_AGAIN)
    {
        repeat_again_rec.keys.clear();
        macro_buf_add(keys, true);
        while (get_macro_buf_size() > 0)
            _input();
        repeat_keys = crawl_state.prev_cmd_keys;
        i++;
    }
    else
        repeat_keys = crawl_state.prev_cmd_keys;

    crawl_state.repeat_cmd                = real_cmd;
    crawl_state.cmd_repeat_started_unsafe = !i_feel_safe();

    int last_repeat_turn;
    for (; i < count && crawl_state.is_repeating_cmd(); ++i)
    {
        last_repeat_turn = you.num_turns;
        _run_input_with_keys(repeat_keys);
        _check_cmd_repeat(last_repeat_turn);
    }
    crawl_state.prev_repeat_cmd           = cmd;
    crawl_state.prev_cmd_repeat_goal      = count;
    crawl_state.repeat_cmd                = CMD_NO_CMD;
}

static void _do_prev_cmd_again()
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
        repeat_again_rec.clear();
        return;
    }

    if (crawl_state.doing_prev_cmd_again)
    {
        mpr("Trying to re-do re-do command, aborting.", MSGCH_ERROR);
        crawl_state.cancel_cmd_all();
        return;
    }

    crawl_state.doing_prev_cmd_again = true;

    ASSERT(!crawl_state.prev_cmd_keys.empty());

    if (crawl_state.prev_cmd == CMD_REPEAT_CMD)
    {
        // just call the relevant part of _do_repeat_cmd again
        // _do_repeat_cmd_again
        crawl_state.doing_prev_cmd_again = false;
        return;
    }

    _run_input_with_keys(crawl_state.prev_cmd_keys);

    crawl_state.doing_prev_cmd_again = false;
}

static void _update_replay_state()
{
    if (!crawl_state.is_replaying_keys()
        && crawl_state.prev_cmd != CMD_NO_CMD)
    {
        if (!repeat_again_rec.keys.empty())
            crawl_state.prev_cmd_keys = repeat_again_rec.keys;
    }

    if (!is_processing_macro())
        repeat_again_rec.clear();
}


static void _compile_time_asserts()
{
    //jmf: NEW ASSERTS: we ought to do a *lot* of these
    COMPILE_CHECK(NUM_SPECIES < SP_UNKNOWN);
    COMPILE_CHECK(NUM_JOBS < JOB_UNKNOWN);
    COMPILE_CHECK(NUM_MONSTERS < MONS_NO_MONSTER);

    // Make sure there's enough room in you.unique_items to hold all
    // the unrandarts.
    COMPILE_CHECK(NO_UNRANDARTS < MAX_UNRANDARTS);

    // Non-artefact brands and unrandart indexes both go into
    // item.special, so make sure they don't overlap.
    COMPILE_CHECK((int) NUM_SPECIAL_WEAPONS < (int) UNRAND_START);

    // We have space for 32 brands in the bitfield.
    COMPILE_CHECK((int) SP_UNKNOWN_BRAND < 8*sizeof(you.seen_weapon[0]));
    COMPILE_CHECK((int) SP_UNKNOWN_BRAND < 8*sizeof(you.seen_armour[0]));
    COMPILE_CHECK(NUM_SPECIAL_WEAPONS <= SP_UNKNOWN_BRAND);
    COMPILE_CHECK(NUM_SPECIAL_ARMOURS <= SP_UNKNOWN_BRAND);
    COMPILE_CHECK(sizeof(float) == sizeof(int32_t));
    COMPILE_CHECK(sizeof(feature_property_type) <= sizeof(terrain_property_t));
    COMPILE_CHECK(sizeof(level_flag_type) <= sizeof(int32_t));
    // Travel cache, traversable_terrain.
    COMPILE_CHECK(NUM_FEATURES <= 256);
    COMPILE_CHECK(NUM_GODS <= NUM_GODS);
    COMPILE_CHECK(TAG_CHR_FORMAT < 256);
    COMPILE_CHECK(TAG_MAJOR_VERSION < 256);
    COMPILE_CHECK(NUM_TAG_MINORS < 256);
    COMPILE_CHECK(NUM_MONSTERS < 32768); // stored in a 16 bit field,
                                         // with untested signedness
    COMPILE_CHECK(MAX_BRANCH_DEPTH < 256); // 8 bits

    // Also some runtime stuff; I don't know if the order of branches[]
    // needs to match the enum, but it currently does.
    for (int i = 0; i < NUM_BRANCHES; ++i)
        ASSERT(branches[i].id == i || branches[i].id == NUM_BRANCHES);
    ASSERT(DNGN_ALTAR_FIRST_GOD + NUM_GODS - 1 == DNGN_ALTAR_LAST_GOD + 1);
}
