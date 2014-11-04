/**
 * @file
 * @brief Main entry point, event loop, and some initialization functions
**/

#include "AppHdr.h"

#include <algorithm>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef TARGET_OS_WINDOWS
# ifndef __ANDROID__
#  include <langinfo.h>
# endif
#endif
#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

#include "ability.h"
#include "abyss.h"
#include "acquire.h"
#include "act-iter.h"
#include "areas.h"
#include "arena.h"
#include "artefact.h"
#include "art-enum.h"
#include "beam.h"
#include "bloodspatter.h"
#include "branch.h"
#include "butcher.h"
#include "chardump.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "colour.h"
#include "command.h"
#include "coord.h"
#include "coordit.h"
#include "crash.h"
#include "dactions.h"
#include "database.h"
#include "dbg-scan.h"
#include "dbg-util.h"
#include "delay.h"
#include "describe-god.h"
#include "describe.h"
#ifdef DGL_SIMPLE_MESSAGING
#include "dgl-message.h"
#endif
#include "dgn-overview.h"
#include "dgn-shoals.h"
#include "dlua.h"
#include "dungeon.h"
#include "effects.h"
#include "end.h"
#include "env.h"
#include "errors.h"
#include "evoke.h"
#include "exercise.h"
#include "fight.h"
#include "files.h"
#include "fineff.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "godcompanions.h"
#include "godconduct.h"
#include "goditem.h"
#include "godpassive.h"
#include "godprayer.h"
#include "hints.h"
#include "hiscores.h"
#include "initfile.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "luaterp.h"
#include "macro.h"
#include "makeitem.h"
#include "map_knowledge.h"
#include "mapmark.h"
#include "maps.h"
#include "melee_attack.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-transit.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player-equip.h"
#include "player.h"
#include "player-reacts.h"
#include "player-stats.h"
#include "prompt.h"
#include "quiver.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "skills.h"
#include "species.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-other.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "stairs.h"
#include "startup.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "tags.h"
#include "target.h"
#include "terrain.h"
#include "throw.h"
#ifdef USE_TILE
 #include "tiledef-dngn.h"
 #include "tilepick.h"
#endif
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "uncancel.h"
#include "version.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "view.h"
#include "viewmap.h"
#include "wiz-dgn.h"
#include "wiz-dump.h"
#include "wiz-fsim.h"
#include "wiz-item.h"
#include "wiz-mon.h"
#include "wiz-you.h"
#include "xom.h"
#include "zotdef.h"

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

int stealth;               // externed in shout.cc and player_reacts.cc

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

static void _do_berserk_no_combat_penalty();
static void _do_searing_ray();
static void _input();
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
static void _set_removed_types_as_identified();

static void _compile_time_asserts();

#ifdef WIZARD
static void _handle_wizard_command();
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
        catch (short_read_exception &E)
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

    _set_removed_types_as_identified();

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
#ifdef DGAMELAUNCH
    puts("  -no-throttle          disable throttling of user Lua scripts");
#else
    puts("  -throttle             enable throttling of user Lua scripts");
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
    puts("  -test               run all test cases in test/ except test/big/");
    puts("  -test foo,bar       run only tests \"foo\" and \"bar\"");
    puts("  -test list          list available tests");
    puts("  -script <name>      run script matching <name> in ./scripts");
    puts("  -mapstat [<levels>] run map stats on the given range of levels");
    puts("      Defaults to entire dungeon; level ranges follow des DEPTH "
         "syntax.");
    puts("      Examples: '-mapstat D,Depths' and '-mapstat Snake:1-4,Spider:1-4,Orc'");
    puts("  -objstat [<levels>] run monster and item stats on the given range "
         "of levels");
    puts("      Defaults to entire dungeon; same level syntax as -mapstat.");
    puts("  -iters <num>        For -mapstat and -objstat, set the number of "
         "iterations");
#endif
    puts("");
    puts("Miscellaneous options:");
    puts("  -dump-maps       write map Lua to stderr when parsing .des files");
#ifndef TARGET_OS_WINDOWS
    puts("  -gdb/-no-gdb     produce gdb backtrace when a crash happens (default:on)");
#endif

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

/**
 * The suffix to apply to welcome_spam when looking up an entry name from the
 * database.
 */
static string _welcome_spam_suffix()
{
    if (crawl_state.game_is_hints())
        return " Hints";

    const string type = crawl_state.game_type_name();
    if (!type.empty())
        return " " + type;
    if (today_is_halloween())
        return " Halloween";
    return "";
}

// A one-liner upon game start to mention the orb.
static void _announce_goal_message()
{
    const string type = _welcome_spam_suffix();
    mprf(MSGCH_PLAIN, "<yellow>%s</yellow>",
         getMiscString("welcome_spam" + type).c_str());
}

static void _god_greeting_message(bool game_start)
{
    if (you_worship(GOD_NO_GOD))
        return;

    string msg, result;

    if (brdepth[BRANCH_ABYSS] == -1 && you_worship(GOD_LUGONU))
        ;
    else if (game_start)
        msg = " newgame";
    else if (you_worship(GOD_XOM) && you.gift_timeout <= 1)
        msg = " bored";
    else if (player_under_penance())
        msg = " penance";

    if (!msg.empty() && !(result = getSpeakString(god_name(you.religion) + msg)).empty())
        god_speaks(you.religion, result.c_str());
    else if (!(result = getSpeakString(god_name(you.religion) + " welcome")).empty())
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
    mark_milestone("begin", "began the quest for the Orb.");

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

// required so that maybe_identify_base_type works correctly
static void _set_removed_types_as_identified()
{
#if TAG_MAJOR_VERSION == 34
    you.type_ids[OBJ_JEWELLERY][AMU_CONTROLLED_FLIGHT] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_JEWELLERY][AMU_CONSERVATION] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_STAVES][STAFF_ENCHANTMENT] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_STAVES][STAFF_CHANNELING] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_POTIONS][POT_GAIN_STRENGTH] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_POTIONS][POT_GAIN_DEXTERITY] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_POTIONS][POT_GAIN_INTELLIGENCE] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_POTIONS][POT_WATER] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_POTIONS][POT_STRONG_POISON] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_SCROLLS][SCR_ENCHANT_WEAPON_II] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_SCROLLS][SCR_ENCHANT_WEAPON_III] = ID_KNOWN_TYPE;
#endif
    // not generated, but the enum value is still used
    you.type_ids[OBJ_POTIONS][POT_SLOWING] = ID_KNOWN_TYPE;
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

    case 'a': acquirement(OBJ_RANDOM, AQ_WIZMODE); break;
    case 'A': wizard_set_all_skills(); break;
    case CONTROL('A'):
        if (player_in_branch(BRANCH_ABYSS))
            wizard_set_abyss();
        else
            mpr("You can only abyss_teleport() inside the Abyss.");
        break;

    case 'b': blink(1000, true, true); break;
    case 'B': you.teleport(true, true); break;
    case CONTROL('B'):
        if (!player_in_branch(BRANCH_ABYSS))
            banished("wizard command");
        else
            down_stairs(DNGN_EXIT_ABYSS);
        break;

    case 'c': wizard_draw_card(); break;
    case 'C': wizard_uncurse_item(); break;
    case CONTROL('C'): die("Intentional crash");

    case 'd': wizard_level_travel(true); break;
    case 'D': wizard_detect_creatures(); break;
    case CONTROL('D'): wizard_edit_durations(); break;

    case 'e': wizard_set_hunger_state(); break;
    // case 'E': break;
    case CONTROL('E'): debug_dump_levgen(); break;

    case 'f': wizard_quick_fsim(); break;
    case 'F': wizard_fight_sim(false); break;
    case CONTROL('F'): wizard_fight_sim(true); break;

    case 'g': wizard_exercise_skill(); break;
    case 'G': wizard_dismiss_all_monsters(); break;
#ifdef DEBUG_BONES
    case CONTROL('G'): debug_ghosts(); break;
#endif

    case 'h': wizard_heal(false); break;
    case 'H': wizard_heal(true); break;
    // case CONTROL('H'): break;

    case 'i': wizard_identify_pack(); break;
    case 'I': wizard_unidentify_pack(); break;
    case CONTROL('I'): debug_item_statistics(); break;

    // case 'j': break;
    case 'J': jiyva_eat_offlevel_items(); break;
    // case CONTROL('J'): break;

    case 'k':
        if (player_in_branch(BRANCH_LABYRINTH))
            change_labyrinth(true);
        else
            mpr("This only makes sense in a labyrinth!");
        break;
    // case 'K': break;
    case CONTROL('K'): wizard_clear_used_vaults(); break;

    case 'l': wizard_set_xl(); break;
    case 'L': debug_place_map(false); break;
    // case CONTROL('L'): break;

    case 'm': wizard_create_spec_monster_name(); break;
    case 'M': wizard_create_spec_monster(); break;
    // case CONTROL('M'): break; // XXX do not use, menu command

    case 'n': you.set_gold(0); break;
    // case 'N': break;
    // case CONTROL('N'): break;

    case 'o': wizard_create_spec_object(); break;
    case 'O': debug_test_explore(); break;
    // case CONTROL('O'): break;

    case 'p': wizard_transform(); break;
    case 'P': debug_place_map(true); break;
    case CONTROL('P'): wizard_list_props(); break;

    // case 'q': break;
    // case 'Q': break;
    case CONTROL('Q'): wizard_toggle_dprf(); break;

    case 'r': wizard_change_species(); break;
    case 'R': wizard_spawn_control(); break;
    case CONTROL('R'): wizard_recreate_level(); break;

    case 's':
        you.exp_available += HIGH_EXP_POOL;
        level_change();
        you.redraw_experience = true;
        break;
    case 'S': wizard_set_skill_level(); break;
    case CONTROL('S'): wizard_abyss_speed(); break;

    case 't': wizard_tweak_object(); break;
    case 'T': debug_make_trap(); break;
    case CONTROL('T'): debug_terp_dlua(); break;

    case 'u': wizard_level_travel(false); break;
    // case 'U': break;
    case CONTROL('U'): debug_terp_dlua(clua); break;

    case 'v': wizard_value_artefact(); break;
    case 'V': wizard_toggle_xray_vision(); break;
    // case CONTROL('V'): break;

    case 'w': wizard_god_mollify(); break;
    case 'W': wizard_god_wrath(); break;
    case CONTROL('W'): wizard_mod_tide(); break;

    case 'x':
        you.experience = 1 + exp_needed(1 + you.experience_level);
        level_change();
        break;
    case 'X':
    {
        int result = 0;
        do
        {
            if (you_worship(GOD_XOM))
                result = xom_acts(abs(you.piety - HALF_MAX_PIETY));
            else
                result = xom_acts(coinflip(), random_range(0, HALF_MAX_PIETY));
        }
        while (result == 0);
        break;
    }
    case CONTROL('X'): debug_xom_effects(); break;

    case 'y': wizard_identify_all_items(); break;
    case 'Y': wizard_unidentify_all_items(); break;
    // case CONTROL('Y'): break;

    case 'z': wizard_cast_spec_spell(); break;
    // case 'Z': break;
    case CONTROL('Z'):
        if (crawl_state.game_is_zotdef())
        {
            you.zot_points = 1000000;
            you.redraw_experience = true;
        }
        else
            mpr("But you're not in Zot Defence!");
        break;

    case '!': wizard_memorise_spec_spell(); break;
    case '@': wizard_set_stats(); break;
    case '#': wizard_load_dump_file(); break;
    case '$': you.add_gold(1000); break;
    case '%': wizard_create_spec_object_by_name(); break;
    case '^': wizard_set_piety(); break;
    case '&': wizard_list_companions(); break;
    // case '*': break; // XXX do not use, this is the alternate control prefix
    case '(': wizard_create_feature(); break;
    // case ')': break;

    // case '`': break;
    case '~': wizard_interlevel_travel(); break;

    case '-': wizard_get_god_gift(); break;
    case '_': wizard_join_religion(); break;

    case '=':
        mprf("Cost level: %d  Total experience: %d  Next cost level: %d Skill cost: %d",
              you.skill_cost_level, you.total_experience,
              skill_cost_needed(you.skill_cost_level + 1),
              calc_skill_cost(you.skill_cost_level));
        break;
    case '+': wizard_make_object_randart(); break;

    // case '[': break;
    case '{': wizard_map_level(); break;

    case ']':
        if (!wizard_add_mutation())
            mpr("Failure to give mutation.");
        break;
    case '}': wizard_reveal_traps(); break;

    case '\\': debug_make_shop(); break;
    case '|': wizard_create_all_artefacts(); break;

    case ';': wizard_list_levels(); break;
    case ':': wizard_list_branches(); break;

    case '\'': wizard_list_items(); break;
    case '"': debug_list_monsters(); break;

    case ',': wizard_place_stairs(true); break;
    // case '>': break; // XXX do not use, menu command

    case '.': wizard_place_stairs(false); break;
    // case '<': break; // XXX do not use, menu command

    // case '/': break;

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

static void _handle_wizard_command()
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
        mprf(MSGCH_WARN, "WARNING: ABOUT TO ENTER WIZARD MODE!");

#ifndef SCORE_WIZARD_CHARACTERS
        mprf(MSGCH_WARN, "If you continue, your game will not be scored!");
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
        mprf(MSGCH_PROMPT, "Enter Wizard Command (? - help): ");
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
        case ']':
        case '^':
        case '%':
        case 'o':
        case 'z':
        case CONTROL('Z'):
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
            && (!you_worship(GOD_JIYVA) || you.penance[GOD_JIYVA]))
        {
            if (dir == RDIR_REST)
                mprf(MSGCH_WARN, "You're standing next to a slime covered wall!");
            else
                mprf(MSGCH_WARN, "You're about to run into the slime covered wall!");
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
    case CMD_FIX_WAYPOINT:
    case CMD_CLEAR_MAP:
    case CMD_INSCRIBE_ITEM:
    case CMD_MAKE_NOTE:
    case CMD_CYCLE_QUIVER_FORWARD:
#ifdef USE_TILE
    case CMD_EDIT_PLAYER_TILE:
#endif
    case CMD_LUA_CONSOLE:
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

    // Unhandled things that should have caused death.
    ASSERT(you.hp > 0);

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
        end_searing_ray();
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
            {
                if (!clua.callfn("ready", 0, 0) && !clua.error.empty())
                    mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
            }
        }

#ifdef WATCHDOG
        // We're not in an infinite loop, reset the timer.
        watchdog();
#endif

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

        _do_searing_ray();

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
    if (you.is_stationary())
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
            else if (ftype == DNGN_SEALED_STAIRS_UP
                     || ftype == DNGN_SEALED_STAIRS_DOWN )
            {
                mpr("A magical barricade bars your way!");
            }
            else if (down)
                mpr("You can't go down here!");
            else
                mpr("You can't go up here!");
            return false;
        }
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
        && ygrd != DNGN_EXIT_THROUGH_ABYSS)
    {
        return true;
    }

    item_def* rune = find_floor_item(OBJ_MISCELLANY, MISC_RUNE_OF_ZOT);
    if (rune && item_is_unique_rune(*rune))
    {
        return yesno("An item of great power still resides in this realm, "
                     "and once you leave you can never return. "
                     "Are you sure you want to leave?", false, 'n');
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
    if (ygrd == DNGN_EXIT_ZIGGURAT
        && you.depth < brdepth[BRANCH_ZIGGURAT])
    {
        // "unsafe", as often you bail at single-digit hp and a wasted turn to
        // an overeager prompt cancellation might be nasty.
        if (!yesno("Are you sure you want to leave this ziggurat?", false, 'n'))
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
        string prompt = make_stringf("Are you sure you want to leave %s?%s",
                                     branches[root_branch].longname,
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
    ASSERT(!crawl_state.game_is_arena());
    ASSERT(!crawl_state.arena_suspended);

    const dungeon_feature_type ygrd = grd(you.pos());

    const bool shaft = (down && get_trap_type(you.pos()) == TRAP_SHAFT
                             && ygrd != DNGN_UNDISCOVERED_TRAP);

    if (!_can_take_stairs(ygrd, down, shaft))
        return;

    if (!_prompt_stairs(ygrd, down))
        return;

    if (!you.attempt_escape()) // false means constricted and don't escape
        return;

    you.stop_constricting_all(true);
    you.stop_being_constricted();

    if (shaft)
        start_delay(DELAY_DESCENDING_STAIRS, 0);
    else
    {
        tag_followers(); // Only those beside us right now can follow.
        start_delay(down ? DELAY_DESCENDING_STAIRS : DELAY_ASCENDING_STAIRS, 1);
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
        if (!clua.callfn("hit_closest_nomove", 0, 0))
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

    case CMD_TOGGLE_VIEWPORT_MONSTER_HP: toggle_viewport_monster_hp(); break;
    case CMD_TOGGLE_VIEWPORT_WEAPONS: toggle_viewport_weapons(); break;
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
        you.turn_is_over = true;
        extract_manticore_spikes("You carefully extract the manticore spikes "
                                 "from your body.");
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
    case CMD_FIRE:                 fire_thing();             break;
    case CMD_FORCE_CAST_SPELL:     do_cast_spell_cmd(true);  break;
    case CMD_LOOK_AROUND:          do_look_around();         break;
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
    case CMD_FORCE_EVOKE_WIELDED:
        if (!evoke_item(you.equip[EQ_WEAPON], cmd != CMD_FORCE_EVOKE_WIELDED))
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
#ifdef USE_TILE_WEB
        else
            tiles.send_dump_info("command", you.your_name);
#endif
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
    {
        const char * const prompt
            = (Options.restart_after_game && Options.restart_after_save)
              ? "Save game and return to main menu?"
              : "Save game and exit?";
        if (yesno(prompt, true, 'n'))
            save_game(true);
        else
            canned_msg(MSG_OK);
        break;
    }

    case CMD_SAVE_GAME_NOW:
        mpr("Saving game... please wait.");
        save_game(true);
        break;

    case CMD_QUIT:
        if (crawl_state.disables[DIS_CONFIRMATIONS]
            || yes_or_no("Are you sure you want to abandon this character and quit the game?"))
        {
            ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_QUITTING);
        }
        else
            canned_msg(MSG_OK);
        break;

    case CMD_LUA_CONSOLE:
        debug_terp_dlua(clua);
        break;

    case CMD_NO_CMD:
    default:
        // The backslash in ?\? is there so it doesn't start a trigraph.
        if (crawl_state.game_is_hints())
            mpr("Unknown command. (For a list of commands type <w>?\?</w>.)");
        else // well, not examine, but...
            mprf(MSGCH_EXAMINE_FILTER, "Unknown command.");

        break;
    }
}

static void _prep_input()
{
    you.turn_is_over = false;
    you.time_taken = player_speed();
    you.shield_blocks = 0;              // no blocks this round

    textcolour(LIGHTGREY);

    set_redraw_status(REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK);
    print_stats();

    viewwindow();
    maybe_update_stashes();
    if (check_for_interesting_features() && you.running.is_explore())
            stop_running();

    if (you.seen_portals)
    {
        ASSERT(you_worship(GOD_ASHENZARI));
        if (you.seen_portals == 1)
            mprf(MSGCH_GOD, "You have a vision of a gate.");
        else
            mprf(MSGCH_GOD, "You have a vision of multiple gates.");

        you.seen_portals = 0;
    }
    if (you.props.exists(GOZAG_ANNOUNCE_SHOP_KEY))
    {
        mpr_nojoin(MSGCH_GOD,
                   you.props[GOZAG_ANNOUNCE_SHOP_KEY].get_string().c_str());
        you.props.erase(GOZAG_ANNOUNCE_SHOP_KEY);
    }
    if (you.seen_invis)
        you.seen_invis = false;
}

static void _check_banished()
{
    if (you.banished)
    {
        ASSERT(brdepth[BRANCH_ABYSS] != -1);
        you.banished = false;
        if (!player_in_branch(BRANCH_ABYSS))
            mprf(MSGCH_BANISHMENT, "You are cast into the Abyss!");
        else if (you.depth < brdepth[BRANCH_ABYSS])
            mprf(MSGCH_BANISHMENT, "You are cast deeper into the Abyss!");
        else
            mprf(MSGCH_BANISHMENT, "The Abyss bends around you!");
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
            for (radius_iterator rad_it(mon_it->pos(), 2, C_ROUND);
                 rad_it; ++rad_it)
            {
                // Matche the blast of a radius 2 explosion.
                env.pgrid(*rad_it) |= FPROP_MOLD;
                env.pgrid(*rad_it) |= FPROP_GLOW_MOLD;
            }
            env.level_state |= LSTATE_GLOW_MOLD;
        }
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
                    mprf(MSGCH_SOUND, "You hear a snapping sound.");
                trap->destroy();
                noisy(spell_effect_noise(SPELL_GOLUBRIAS_PASSAGE), *it);
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

    if (crawl_state.viewport_monster_hp || crawl_state.viewport_weapons)
    {
        crawl_state.viewport_monster_hp = false;
        crawl_state.viewport_weapons = false;
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
        player_reacts();

    abyss_morph();
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

            if (you.num_turns % ZOTDEF_CYCLE_LENGTH > ZOTDEF_CYCLE_INTERVAL
                && x_chance_in_y(you.num_turns % ZOTDEF_CYCLE_LENGTH, ZOTDEF_CYCLE_LENGTH*3))
            {
                zotdef_spawn(false);
            }
        }
    }
    if (!crawl_state.game_is_arena())
        player_reacts_to_monsters();

    viewwindow();

    if (you.cannot_act() && any_messages()
        && crawl_state.repeat_cmd != CMD_WIZARD)
    {
        more();
    }

#if defined(DEBUG_TENSION) || defined(DEBUG_RELIGION)
    if (!you_worship(GOD_NO_GOD))
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
            && !you_are_delayed()
            && !crawl_state.disables[DIS_SAVE_CHECKPOINTS])
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

    // This is the main clear_messages() with Option.clear_messages.
    if (!is_synthetic_key(keyin))
        clear_messages();

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
                find_connected_identical(*ai, all_door);
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
        apply_berserk_penalty = false;

        return true;
    }

    if (find_trap(target) && grd(target) != DNGN_UNDISCOVERED_TRAP)
    {
        if (!form_can_wield())
        {
            mpr("You can't disarm traps in your present form.");
            return true;
        }
        else if (!you.confused())
        {
            const int cloud = env.cgrid(target);
            if (cloud != EMPTY_CLOUD
                && is_damaging_cloud(env.cloud[cloud].type, true)
                && !actor_cloud_immune(&you, env.cloud[cloud]))
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
                const skill_type wpn_skl = you.weapon() ?
                                                item_attack_skill(*you.weapon()) :
                                                SK_UNARMED_COMBAT;
                if (actor_can_cleave(you, wpn_skl))
                    get_all_cleave_targets(&you, target, cleave_targets);

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
                you.time_taken = you.attack_delay(you.weapon());
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
    ASSERT(!crawl_state.game_is_arena());
    ASSERT(!crawl_state.arena_suspended);

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
            mprf(MSGCH_PROMPT, "Which direction?");
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

    player_open_door(doorpos, check_confused);

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
            mprf(MSGCH_PROMPT, "Which direction?");
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
        find_connected_identical(doorpos, all_door);
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
static void _do_berserk_no_combat_penalty()
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
            mprf(MSGCH_DURATION, "You feel a strong urge to attack something.");
            break;
        case 4:
            mprf(MSGCH_DURATION, "You feel your anger subside.");
            break;
        case 6:
            mprf(MSGCH_DURATION, "Your blood rage is quickly leaving you.");
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

// Fire the next searing ray stage if we have taken no other action this turn,
// otherwise cancel
static void _do_searing_ray()
{
    if (you.attribute[ATTR_SEARING_RAY] == 0)
        return;

    // Convert prepping value into stage one value (so it can fire next turn)
    if (you.attribute[ATTR_SEARING_RAY] == -1)
    {
        you.attribute[ATTR_SEARING_RAY] = 1;
        return;
    }

    if (crawl_state.prev_cmd == CMD_WAIT
        || crawl_state.prev_cmd == CMD_MOVE_NOWHERE)
    {
        handle_searing_ray();
    }
    else
        end_searing_ray();
}

// Called when the player moves by walking/running. Also calls attack
// function etc when necessary.
static void _move_player(int move_x, int move_y)
{
    _move_player(coord_def(move_x, move_y));
}

// Swap monster to this location.  Player is swapped elsewhere.
static void _swap_places(monster* mons, const coord_def &loc)
{
    ASSERT(map_bounds(loc));
    ASSERT(monster_habitable_grid(mons, grd(loc)));

    if (monster_at(loc))
    {
        if (mons->type == MONS_WANDERING_MUSHROOM
            && monster_at(loc)->type == MONS_TOADSTOOL)
        {
            monster_swaps_places(mons, loc - mons->pos());
            return;
        }
        else
        {
            mpr("Something prevents you from swapping places.");
            return;
        }
    }

    mpr("You swap places.");

    mgrd(mons->pos()) = NON_MONSTER;

    const coord_def old_loc = mons->pos();
    mons->moveto(loc);
    mons->apply_location_effects(old_loc);

    if (mons->alive())
        mgrd(mons->pos()) = mons->mindex();

    return;
}

static void _move_player(coord_def move)
{
    ASSERT(!crawl_state.game_is_arena() && !crawl_state.arena_suspended);

    bool attacking = false;
    bool moving = true;         // used to prevent eventual movement (swap)
    bool swap = false;

    int additional_time_taken = 0; // Extra time independant of movement speed

    ASSERT(!in_bounds(you.pos()) || !cell_is_solid(you.pos())
           || you.wizmode_teleported_into_rock);

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
        bool penance = false;
        for (adjacent_iterator ai(you.pos(), false); ai; ++ai)
        {
            if (is_feat_dangerous(grd(*ai))
                && (dangerous == DNGN_FLOOR || grd(*ai) == DNGN_LAVA))
            {
                dangerous = grd(*ai);
                break;
            }
            else
            {
                string suffix, adj;
                monster *mons = monster_at(*ai);
                if (mons
                    && !fedhas_passthrough(mons)
                    && bad_attack(mons, adj, suffix, penance))
                {
                    bad_mons = mons;
                    bad_suff = suffix;
                    bad_adj = adj;
                    if (penance)
                        break;
                }
            }
        }

        if (you.is_stationary())
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

            if (penance)
                prompt += " This could place you under penance!";

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

        if (you.is_stationary())
        {
            // Don't choose a random location to try to attack into - allows
            // abuse, since trying to move (not attack) takes no time, and
            // shouldn't. Just force confused trees to use ctrl.
            mpr("You cannot move. (Use ctrl+direction to attack without "
                "moving)");
            return;
        }

        if (!one_chance_in(3))
        {
            move.x = random2(3) - 1;
            move.y = random2(3) - 1;
            you.reset_prev_move();
            if (move.origin())
                mpr("You're too confused to move!");
        }

        const coord_def& new_targ = you.pos() + move;
        if (!in_bounds(new_targ) || !you.can_pass_through(new_targ))
        {
            you.walking = move.abs();
            you.turn_is_over = true;
            if (you.digging) // no actual damage
            {
                mprf("Your mandibles retract as you bump into %s",
                     feature_description_at(new_targ, false,
                                            DESC_THE).c_str());
                you.digging = false;
            }
            else
            {
                mprf("You bump into %s",
                     feature_description_at(new_targ, false,
                                            DESC_THE).c_str());
            }
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
    {
        // Why isn't the border permarock?
        if (you.digging)
            mpr("This wall is too hard to dig through.");
        return;
    }

    const dungeon_feature_type targ_grid = grd(targ);

    const string walkverb = you.flight_mode()           ? "fly"
                          : you.form == TRAN_SPIDER     ? "crawl"
                          : (you.species == SP_NAGA
                             && form_keeps_mutations()) ? "slither"
                                                        : "walk";

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
            mprf("You %s carefully through the %s.", walkverb.c_str(),
                 mons_genus(targ_monst->type) == MONS_FUNGUS ? "fungus"
                                                             : "plants");
        }
        targ_monst = NULL;
    }

    bool targ_pass = you.can_pass_through(targ) && !you.is_stationary();

    if (you.digging)
    {
        if (you.hunger_state == HS_STARVING && you.undead_state() == US_ALIVE)
        {
            you.digging = false;
            canned_msg(MSG_TOO_HUNGRY);
        }
        else if (grd(targ) == DNGN_ROCK_WALL
                 || grd(targ) == DNGN_CLEAR_ROCK_WALL
                 || grd(targ) == DNGN_GRATE)
        {
            targ_pass = true;
        }
        else // moving or attacking ends dig
        {
            you.digging = false;
            if (feat_is_solid(grd(targ)))
                mpr("You can't dig through that.");
            else
                mpr("You retract your mandibles.");
        }
    }

    // You can swap places with a friendly or good neutral monster if
    // you're not confused, or if both of you are inside a sanctuary.
    const bool can_swap_places = targ_monst
                                 && !targ_monst->is_stationary()
                                 && (targ_monst->wont_attack()
                                       && !you.confused()
                                     || is_sanctuary(you.pos())
                                        && is_sanctuary(targ));

    // You cannot move away from a siren but you CAN fight monsters on
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

    if (targ_monst && !targ_monst->submerged())
    {
        if (can_swap_places && !beholder && !fmonger)
        {
            if (swap_check(targ_monst, mon_swap_dest))
                swap = true;
            else
            {
                stop_running();
                moving = false;
            }
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
            if (!you.can_see(targ_monst) && !check_moveto(targ, walkverb))
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

        if (!you.confused() && !check_moveto(targ, walkverb))
        {
            stop_running();
            you.turn_is_over = false;
            return;
        }

        if (you.duration[DUR_BARBS] && !you.props.exists(BARBS_MOVE_KEY))
        {
            string prompt = "The barbs in your skin will harm you if you move."
                            " Continue?";
            if (!yesno(prompt.c_str(), false, 'n'))
            {
                canned_msg(MSG_OK);
                you.turn_is_over = false;
                return;
            }

            you.props[BARBS_MOVE_KEY] = true;
        }

        if (!you.attempt_escape()) // false means constricted and did not escape
            return;

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

        if (you.digging)
        {
            mprf("You dig through %s.", feature_description_at(targ, false,
                 DESC_THE, false).c_str());
            destroy_wall(targ);
            noisy(6, you.pos());
            make_hungry(50, true);
            additional_time_taken += BASELINE_DELAY / 5;
        }

        if (swap)
            _swap_places(targ_monst, mon_swap_dest);
        else if (you.duration[DUR_COLOUR_SMOKE_TRAIL])
        {
            check_place_cloud(CLOUD_MAGIC_TRAIL, you.pos(),
                random_range(3, 10), &you, 0, ETC_RANDOM);
        }

        if (delay_is_run(current_delay_action()) && env.travel_trail.empty())
            env.travel_trail.push_back(you.pos());
        else if (!delay_is_run(current_delay_action()))
            clear_travel_trail();

        // clear constriction data
        you.stop_constricting_all(true);
        you.stop_being_constricted();

        // Don't trigger traps when confusion causes no move.
        if (you.pos() != targ)
            move_player_to_grid(targ, true);

        if (you.duration[DUR_BARBS])
        {
            mprf(MSGCH_WARN,"The barbed spikes dig painfully into your body "
            "as you move.");
            ouch(roll_dice(2, you.attribute[ATTR_BARBS_POW]), NON_MONSTER,
                 KILLED_BY_BARBS);
            bleed_onto_floor(you.pos(), MONS_PLAYER, 2, false);

            // Sometimes decrease duration even when we move.
            if (one_chance_in(3))
                extract_manticore_spikes("The manticore spikes snap loose.");
        }

        if (delay_is_run(current_delay_action()))
            env.travel_trail.push_back(you.pos());

        you.time_taken *= player_movement_speed();
        you.time_taken = div_rand_round(you.time_taken, 10);
        you.time_taken += additional_time_taken;

        if (you.running && you.running.travel_speed)
        {
            you.time_taken = max(you.time_taken,
                                 div_round_up(100, you.running.travel_speed));
        }

#ifdef EUCLIDEAN
        if (move.abs() == 2)
            you.time_taken *= 1.4;
#endif

        you.walking = move.abs();
        you.prev_move = move;
        move.reset();
        you.turn_is_over = true;
        request_autopickup();
    }

    // BCR - Easy doors single move
    if ((Options.travel_open_doors || !you.running) && !attacking && feat_is_closed_door(targ_grid))
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
        if (you.is_stationary())
            canned_msg(MSG_CANNOT_MOVE);
        else if (grd(targ) == DNGN_OPEN_SEA)
            mpr("The ferocious winds and tides of the open sea thwart your progress.");
        else if (grd(targ) == DNGN_LAVA_SEA)
            mpr("The endless sea of lava is not a nice place.");
        else if (feat_is_tree(grd(targ)) && you_worship(GOD_FEDHAS))
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

    if (!attacking && you_worship(GOD_CHEIBRIADOS) && one_chance_in(10)
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
        mprf(MSGCH_PROMPT, "%s", prompt);

    line_reader reader(buf, buf_len);

    reader.set_keyproc(_get_num_and_char_keyfun);
#ifdef USE_TILE_WEB
    reader.set_tag("repeat");
#endif

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
        mprf(MSGCH_ERROR, "(Key replay stole keys)");
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
        mprf(MSGCH_PROMPT, "Enter command to be repeated: ");
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
        mprf(MSGCH_ERROR, "Trying to re-do re-do command, aborting.");
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
    COMPILE_CHECK(NUM_UNRANDARTS < MAX_UNRANDARTS);

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
}
