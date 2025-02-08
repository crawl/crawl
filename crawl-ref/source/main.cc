/**
 * @file
 * @brief Main entry point, event loop, and some initialization functions
**/

#include "AppHdr.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <utility> // pair
#include <vector>
#include <fcntl.h>
#if defined(UNIX) || defined(TARGET_COMPILER_MINGW)
# include <unistd.h>
#endif

#ifndef TARGET_OS_WINDOWS
# include <langinfo.h>
#endif
#ifdef USE_UNIX_SIGNALS
#include <csignal>
#endif

#include "ability.h"
#include "abyss.h"
#include "act-iter.h"
#include "adjust.h"
#include "areas.h"
#include "arena.h"
#include "artefact.h"
#include "beam.h"
#include "branch.h"
#include "chardump.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "colour.h"
#include "command.h"
#include "coord.h"
#include "corpse.h"
#include "crash.h"
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
#include "directn.h"
#include "dlua.h"
#include "dungeon.h"
#include "end.h"
#include "env.h"
#include "tile-env.h"
#include "errors.h"
#include "evoke.h"
#include "fight.h"
#include "files.h"
#include "fineff.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "god-prayer.h"
#include "hints.h"
#include "hiscores.h"
#include "initfile.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "jobs.h"
#include "known-items.h"
#include "level-state-type.h"
#include "libutil.h"
#include "lookup-help.h"
#include "luaterp.h"
#include "macro.h"
#include "makeitem.h"
#include "map-knowledge.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-transit.h"
#include "mon-util.h"
#include "mutation.h"
#include "movement.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "player-reacts.h"
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
#include "spl-monench.h"
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
 #include "rltiles/tiledef-dngn.h"
#endif
#include "tilepick.h"
#include "timed-effects.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "uncancel.h"
#include "version.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "view.h"
#include "viewmap.h"
#ifdef __ANDROID__
#include "syscalls.h"
#endif
#include "wiz-you.h" // FREEZE_TIME_KEY
#include "wizard.h" // handle_wizard_command() and enter_explore_mode()
#include "xom.h" // XOM_CLOUD_TRAIL_TYPE_KEY
#include "zot.h"

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
crawl_tile_environment tile_env;

player you;

game_state crawl_state;

void world_reacts();

static key_recorder repeat_again_rec;

// Clockwise, around the compass from north (same order as run_dir_type)
const struct coord_def Compass[9] =
{
    // kuln
    {0, -1}, {1, -1}, {1, 0}, {1, 1},
    // jbhy
    {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
    // .
    {0, 0}
};

// Functions in main module
static void _launch_game_loop();
NORETURN static void _launch_game();

static void _do_berserk_no_combat_penalty();

static void _input();

static void _safe_move_player(coord_def move);
static void _swing_at_target(coord_def move);

static void _start_running(int dir, int mode);

static void _prep_input();
static command_type _get_next_cmd();
static keycode_type _get_next_keycode();
static command_type _keycode_to_command(keycode_type key);
static void _do_cmd_repeat();
static void _do_prev_cmd_again();
static void _update_replay_state();

static void _show_commandline_options_help();
static void _announce_goal_message();
static void _god_greeting_message(bool game_start);
static void _take_starting_note();
static void _startup_hints_mode();
static void _set_removed_types_as_identified();

static void _startup_asserts()
{
    for (int i = 0; i < NUM_BRANCHES; ++i)
        ASSERT(branches[i].id == i || branches[i].id == NUM_BRANCHES);
}

//
//  It all starts here. Some initialisations are run first, then straight
//  to new_game and then input.
//
#ifdef USE_SDL
# include <SDL_main.h>
# if defined(__GNUC__) && !defined(__clang__)
// SDL plays nasty tricks with main() (actually, _SDL_main()), which for
// Windows builds somehow fail with -fwhole-program. Thus, exempt SDL_main()
// from this treatment.
__attribute__((externally_visible))
# endif
#endif
int main(int argc, char *argv[])
{
#ifdef DGAMELAUNCH
    // avoid commas instead of dots, etc, on CDO
    setlocale(LC_CTYPE, "");
#else
    setlocale(LC_ALL, "");
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
    // do this explicitly so that static initialization order woes can be
    // ignored.
    msg::force_stderr echo(maybe_bool::maybe);

    init_crash_handler();

    _startup_asserts();

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

    // make sure all the expected data directories exist
    validate_basedirs();

    {
        // Read the init file -- first pass. This pass ignores lua. It'll get
        // reread with lua on starting a game.
#ifdef USE_TILE_WEB
        // on webtiles, prevent echoing a player's rc errors to the webtiles
        // log. At this point, io is not initialized so for other builds we
        // do want to echo, in case things go extremely wrong. For dgl builds,
        // players will see the error in their log anyways. (Regular webtiles
        // actually gets a popup, but this is rarely used except for dev work)

        // TODO: would be simpler to just never echo? Do other builds really
        // need this outside of debugging contexts?
        msg::force_stderr suppress_log_stderr(false);
#endif
        read_init_file();
    }

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

#ifdef USE_TILE_LOCAL
    // Hook up text colour redefinitions
    for (auto col : Options.custom_text_colours)
        term_colours[col.colour_index] = col.colour_def;
#endif

    _launch_game_loop();
    if (crawl_state.last_game_exit.message.size())
        end(0, false, "%s\n", crawl_state.last_game_exit.message.c_str());
    else
        end(0);

    return 0;
}

static void _reset_game()
{
    clrscr();
    // Unset by death, but not by saving with restart_after_save.
    crawl_state.reset_game();
    clear_message_store();
    macro_clear_buffers();
    crawl_state.show_more_prompt = true;
    the_lost_ones.clear();
    shopping_list = ShoppingList();
    you = player();
    reset_hud();
    StashTrack = StashTracker();
    travel_cache = TravelCache();
    // TODO: hint state needs seem work
    Hints.hints_events.init(false);
    clear_level_target();
    overview_clear();
    clear_message_window();
    note_list.clear();
    msg::deinitialise_mpr_streams();
    quiver::reset_state();

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
        catch (const game_ended_condition &ge)
        {
            game_ended = true;
            crawl_state.last_game_exit = ge;
            _reset_game();

            // Don't re-enter the Sprint menu with restart_after_save, as
            // that would reload the just-saved game immediately.
            if (ge.exit_reason == game_exit::save)
                crawl_state.last_type = GAME_TYPE_UNSPECIFIED;
        }
        catch (const ext_fail_exception &fe)
        {
            end(1, false, "%s", fe.what());
        }
        catch (const short_read_exception&)
        {
            end(1, false, "Error: truncation inside the save file.\n");
        }
    } while (crawl_should_restart(crawl_state.last_game_exit.exit_reason)
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

    Version::record(you.prev_save_version);

    if (!crawl_state.game_is_tutorial())
    {
        msg::stream << "<yellow>Welcome" << (game_start? "" : " back") << ", "
                    << you.your_name << " the "
                    << species::name(you.species)
                    << " " << get_job_name(you.char_class) << ".</yellow>";
        // TODO: seeded sprint?
        if (crawl_state.type == GAME_TYPE_CUSTOM_SEED)
            msg::stream << endl << "<white>" << seed_description() << "</white>";
        else
        {
            const string type_name = crawl_state.game_type_name();
            if (!type_name.empty())
                msg::stream << "<white> [" << type_name << "]</white>";
        }
        msg::stream << endl;
    }

#ifdef USE_TILE
    viewwindow();
    update_screen();
#endif

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
    if (in_headless_mode())
    {
        // TODO: derive exact list from initfile.cc
#ifdef USE_TILE_LOCAL
        puts("Headless crawl tiles build: requires -objstat or -mapstat.");
#else
        puts("Headless crawl: requires -test, -script, -objstat, -builddb, or other similar parameter.");
        puts("A single argument will be interpreted as a script name.");
#endif
        puts("");
    }

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
#ifdef WIZARD
    puts("  -wizard               allow access to wizard mode");
    puts("  -explore              allow access to explore mode");
#endif
#ifdef DGAMELAUNCH
    puts("  -no-throttle          disable throttling of user Lua scripts");
#else
    puts("  -throttle             enable throttling of user Lua scripts");
    puts("  -lua-max-memory       max memory in MB allowed for user Lua scripts");
    puts("  -seed <number>        specify a game seed to use when creating a new game");
#endif

    puts("");

    puts("Command line options override init file options, which override");
    puts("environment options (CRAWL_NAME, CRAWL_DIR, CRAWL_RC).");
    puts("");
    puts("  -extra-opt-first optname=optval");
    puts("  -extra-opt-last  optname=optval");
    puts("");
    puts("Acts as if 'optname=optval' was at the top or bottom of the init");
    puts("file. Can be used multiple times.");
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
    puts("Diagnostic options:");
    puts("  -test               run all test cases in test/ except test/big/");
    puts("  -test foo,bar       run only tests \"foo\" and \"bar\"");
    puts("  -test list          list available tests");
#endif
    // XX should this really be advertised outside of debug builds?
    puts("  -headless           force headless mode (no pty)");
    puts("  -script <name>      run script matching <name> in ./scripts");
#ifdef DEBUG_STATISTICS
#ifndef DEBUG_DIAGNOSTICS
    puts("");
    puts("Diagnostic options:");
#endif
    puts("  -mapstat [<levels>] run map stats on the given range of levels");
    puts("      Defaults to entire dungeon; level ranges follow des DEPTH "
         "syntax.");
    puts("      Examples: '-mapstat D,Depths' and '-mapstat Snake:1-4,Spider:1-4,Orc'");
    puts("  -dump-disconnect    In mapstat when a disconnected level is "
         "generated, dump");
    puts("      map to map.dump and exit");
    puts("  -objstat [<levels>] run monster and item stats on the given range "
         "of levels");
    puts("      Defaults to entire dungeon; same level syntax as -mapstat.");
    puts("  -iters <num>        For -mapstat and -objstat, set the number of "
         "iterations");
    puts("  -force-map <map>    For -mapstat and -objstat, always choose the "
         "      given map on every level.");
#endif
    puts("");
    puts("Miscellaneous options:");
    puts("  -builddb         don't start the game; rebuild the .des cache and exit");
    puts("  -reset-cache     force a full rebuild of the .des cache");
    puts("  -dump-maps       write map Lua to stderr when parsing .des files");
#ifndef TARGET_OS_WINDOWS
    puts("  -gdb/-no-gdb     produce gdb backtrace when a crash happens (default:on)");
#endif
    puts("  -playable-json   list playable species, jobs, and character combos.");
    puts("  -branches-json   list branch data.");
    puts("  -no-player-bones do not write player's info to bones files.");

#if defined(TARGET_OS_WINDOWS) && defined(USE_TILE_LOCAL)
    text_popup(help, L"Dungeon Crawl command line help");
#endif
}

static string _wanderer_equip_str()
{
    return "the following items: "
        + comma_separated_fn(begin(you.inv), end(you.inv),
                         [] (const item_def &item) -> string
                         {
                             return item.name(DESC_A, false, true);
                         }, ", ", ", ", mem_fn(&item_def::defined));
}

static string _wanderer_spell_str()
{
    return comma_separated_fn(begin(you.spells), end(you.spells),
                              [] (const spell_type spell) -> string
                              {
                                  return spell_title(spell);
                              },
                              ", ", ", ",
                              // Don't include empty spell slots
                              [] (const spell_type spell) -> bool
                              {
                                  return spell != SPELL_NO_SPELL;
                              });
}

static string _get_equip_str()
{
    if (inv_count() == 0)
        return "";
    if (you.char_class == JOB_WANDERER)
        return _wanderer_equip_str();
    return "";
}

static void _djinn_announce_spells()
{
    const string equip_str =_get_equip_str();
    const string spell_str = you.spell_no ?
                                "the following spells memorised: " + _wanderer_spell_str() :
                                "";
    if (spell_str.empty() && equip_str.empty())
        return;

    const string spacer = spell_str.empty() || equip_str.empty() ? "" : "; and ";
    mprf("You begin with %s%s%s.", equip_str.c_str(), spacer.c_str(), spell_str.c_str());

    take_note(Note(NOTE_MESSAGE, 0, 0, you.your_name + " set off with " +
                                       equip_str + spell_str + "."));
}

// Announce to the message log and make a note of the player's starting items,
// spells and spell library
static void _wanderer_note_equipment()
{
    const string equip_str = inv_count() > 0 ? _wanderer_equip_str() : "";

    const string eq_spacer = equip_str.empty() ? "" : "; and ";

    // Wanderers start with at most 1 spell memorised.
    const string spell_str =
        !you.spell_no ? "" :
        eq_spacer + "the following spell memorised: "
        + _wanderer_spell_str();

    const string spell_spacer = spell_str.empty() && equip_str.empty() ? "" : "; and ";

    auto const library = get_sorted_spell_list(true, true);
    const string library_str =
        !library.size() ? "" :
        spell_spacer + "the following spells available to memorise: "
        + comma_separated_fn(library.begin(), library.end(),
                             [] (const spell_type spell) -> string
                             {
                                 return spell_title(spell);
                             }, ", ", ", ");

    // Announce the starting equipment and spells, because it is otherwise
    // not obvious if the player has any spells.
    mprf("You begin with %s%s%s.", equip_str.c_str(),
         spell_str.c_str(), library_str.c_str());

    const string combined_str = you.your_name + " set off with "
                                + equip_str + spell_str + library_str + ".";
    take_note(Note(NOTE_MESSAGE, 0, 0, combined_str));
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
    notestr << you.your_name << " the "
            << species::name(you.species) << " "
            << get_job_name(you.char_class)
            << " began the quest for the Orb.";
    take_note(Note(NOTE_MESSAGE, 0, 0, notestr.str()));
    mark_milestone("begin", "began the quest for the Orb.");

    notestr.str("");
    notestr.clear();

#ifdef WIZARD
    if (you.wizard || you.suppress_wizard)
    {
        notestr << "You started the game in wizard mode.";
        take_note(Note(NOTE_MESSAGE, 0, 0, notestr.str()));

        notestr.str("");
        notestr.clear();
    }
#endif

    if (you.has_mutation(MUT_INNATE_CASTER))
        _djinn_announce_spells();
    else if (you.char_class == JOB_WANDERER)
        _wanderer_note_equipment();

    notestr << "HP: " << you.hp << "/" << you.hp_max
            << " MP: " << you.magic_points << "/" << you.max_magic_points;

    take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0,
                   notestr.str().c_str()));
}

static void _startup_hints_mode()
{
    // Don't allow triggering at game start.
    Hints.hints_just_triggered = true;
    hints_starting_screen();
}

// required so that maybe_identify_base_type works correctly
static void _set_removed_types_as_identified()
{
    for (auto entry : removed_items)
        if (item_type_has_ids(entry.first))
            you.type_ids(entry) = true;
}

// Set up the running variables for the current run.
static void _start_running(int dir, int mode)
{
    if (Hints.hints_events[HINT_SHIFT_RUN] && mode == RMODE_START)
        Hints.hints_events[HINT_SHIFT_RUN] = false;

    if (!i_feel_safe(true)
        || !can_rest_here(true)
            && (mode == RMODE_REST_DURATION || mode == RMODE_WAIT_DURATION))
    {
        return;
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
    case CMD_LOOKUP_HELP:
        mpr("You can't repeat informational commands.");
        return false;

    // Multi-turn commands
    case CMD_REST:
    case CMD_PICKUP:
    case CMD_DROP:
    case CMD_DROP_LAST:
    case CMD_GO_UPSTAIRS:
    case CMD_GO_DOWNSTAIRS:
    case CMD_WIELD_WEAPON:
    case CMD_WEAPON_SWAP:
    case CMD_WEAR_JEWELLERY:
    case CMD_REMOVE_JEWELLERY:
    case CMD_MEMORISE_SPELL:
    case CMD_EXPLORE:
    case CMD_INTERLEVEL_TRAVEL:
    case CMD_UNEQUIP:
    case CMD_REMOVE_ARMOUR:
    case CMD_EQUIP:
    case CMD_WEAR_ARMOUR:
        mpr("You can't repeat multi-turn commands.");
        return false;

    // Miscellaneous non-repeatable commands.
    case CMD_TOGGLE_AUTOPICKUP:
    case CMD_TOGGLE_SOUND:
    case CMD_ADJUST_INVENTORY:
    case CMD_QUIVER_ITEM:
    case CMD_REPLAY_MESSAGES:
    case CMD_REDRAW_SCREEN:
    case CMD_MACRO_ADD:
    case CMD_MACRO_MENU:
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

    case CMD_WAIT:
    case CMD_SAFE_WAIT:
    case CMD_SAFE_MOVE_LEFT:
    case CMD_SAFE_MOVE_DOWN:
    case CMD_SAFE_MOVE_UP:
    case CMD_SAFE_MOVE_RIGHT:
    case CMD_SAFE_MOVE_UP_LEFT:
    case CMD_SAFE_MOVE_DOWN_LEFT:
    case CMD_SAFE_MOVE_UP_RIGHT:
    case CMD_SAFE_MOVE_DOWN_RIGHT:
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
            return yesno("Really repeat movement command while danger "
                         "is nearby?", false, 'n');
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

static void _center_cursor()
{
#ifndef USE_TILE_LOCAL
    const coord_def cwhere = crawl_view.grid2view(you.pos());
    cgotoxy(cwhere.x, cwhere.y, GOTO_DNGN);
#endif
}

// We have to refresh the SH display if the player's incapacitated state
// changes (getting confused/paralysed/etc. sets SH to 0, recovering
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

static void _update_place_stats()
{
    if (you.num_turns == -1)
        return;

    // not strictly stored as part of PlaceInfo, but this is a natural place
    // to do this update.
    CrawlHashTable &time_tracking = you.props[TIME_PER_LEVEL_KEY].get_table();
    int &cur_value = time_tracking[level_id::current().describe()].get_int();
    cur_value += you.time_taken;

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

        if (you_are_delayed()
            && current_delay()->is_resting())
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

        if (!you_are_delayed()
            || !current_delay()->is_resting())
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

    // Currently only set if Xom accidentally kills the player.
    you.reset_escaped_death();

    reset_damage_counters();

    if (you.pending_revival)
    {
        revive();
        bring_to_safety();
        redraw_screen();
        update_screen();
    }

    if (you.props.exists(DREAMSHARD_KEY))
        you.props.erase(DREAMSHARD_KEY);
    crawl_state.potential_pursuers.clear();

    apply_exp();

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

    if (you.duration[DUR_VEXED])
        do_vexed_attack(you);

    if (you.cannot_act() || you.duration[DUR_VEXED])
    {
        if (crawl_state.repeat_cmd != CMD_WIZARD)
        {
            crawl_state.cancel_cmd_repeat("Cannot control self, cancelling command "
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

    if (you_are_delayed()
        && !dynamic_cast<MacroProcessKeyDelay*>(current_delay().get()))
    {
        stop_channelling_spells();
        handle_delay();

        // Some delays set you.turn_is_over.

        bool time_is_frozen = false;

#ifdef WIZARD
        if (you.props.exists(FREEZE_TIME_KEY))
            time_is_frozen = true;
#endif

        // Some delays reset you.time_taken.
        if (!time_is_frozen && (you.time_taken || you.turn_is_over))
        {
            if (you.berserk())
                _do_berserk_no_combat_penalty();
            world_reacts();
        }

        if (!you_are_delayed())
            update_can_currently_train();

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
        // Lua stack must be empty. Unless there's a leak.
        ASSERT(lua_gettop(clua.state()) == 0);

        if (!has_pending_input() && !kbhit())
        {
            if (++crawl_state.lua_calls_no_turn > 1000)
                mprf(MSGCH_ERROR, "Infinite lua loop detected, aborting.");
            else if (!crawl_state.lua_ready_throttled)
            {
                if (!clua.callfn("ready", 0, 0) && !clua.error.empty())
                {
                    // if ready() has been killed once, it is considered
                    // buggy and should not run again. Note: the sequencing is
                    // important here, because mprs trigger the c_message hook,
                    // so this state variable needs to be checked immediately.
                    if (crawl_state.lua_script_killed)
                        crawl_state.lua_ready_throttled = true;

                    mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
                    if (crawl_state.lua_ready_throttled)
                    {
                        mprf(MSGCH_ERROR, "Banning ready() after %d throttles",
                            CLua::MAX_THROTTLE_SLEEPS);
                    }

                }
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
        const command_type cmd = you.turn_is_over ? CMD_NO_CMD : _get_next_cmd();

        if (crawl_state.seen_hups)
            save_game(true, "Game saved, see you later!");

        crawl_state.waiting_for_command = false;

        if (!crawl_state.is_replaying_keys())
            you.elapsed_time_at_last_input = you.elapsed_time;

        // We need to capture this value, since crawl_state.prev_cmd is updated
        // to be equal to the current command before process_command runs.
        // This means that certain values CMD_PREV_CMD_AGAIN will be kept
        // verbatim; the alternative would require keeping some more state. But
        // it's not too important for the current use.
        const command_type real_prev_cmd = crawl_state.prev_cmd;

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
            ::process_command(cmd, real_prev_cmd);

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

#ifdef WIZARD
    if (you.props.exists(FREEZE_TIME_KEY))
        you.turn_is_over = false;
#endif

    if (you.turn_is_over)
    {
        if (you.apply_berserk_penalty)
            _do_berserk_no_combat_penalty();

        handle_channelled_spell();

        world_reacts();
    }
    else
    {
        // Make sure to do a full view update even when the turn isn't over.
        // This else will be triggered by instantaneous actions, such as
        // Chei's temporal distortion.
        viewwindow();
        update_screen();
    }

    update_can_currently_train();

    _update_replay_state();

    crawl_state.clear_god_acting();
}

static bool _can_take_stairs(dungeon_feature_type ftype, bool down,
                             bool known_shaft)
{
    // Up and down both work for shops, portals, and altars.
    if (ftype == DNGN_ENTER_SHOP || feat_is_altar(ftype))
    {
        if (crawl_state.doing_prev_cmd_again)
        {
            mprf("You can't repeat %s actions.",
                ftype == DNGN_ENTER_SHOP ? "shop" : "altar");
            crawl_state.cancel_cmd_all();
        }
        else if (you.berserk())
            canned_msg(MSG_TOO_BERSERK);
        else if (ftype == DNGN_ENTER_SHOP) // don't convert to capitalism
            shop();
        else
            try_god_conversion(feat_altar_god(ftype));
        // Even though we may have "succeeded", return false so we don't keep
        // trying to go downstairs.
        return false;
    }

    // Immobile
    if (!you.is_motile())
    {
        canned_msg(MSG_CANNOT_MOVE);
        return false;
    }
    else if (you.duration[DUR_VAINGLORY])
    {
        mpr("It simply wouldn't do to leave so soon after announcing yourself.");
        return false;
    }

    // ATTR_HELD is intentionally not tested here, it's handled in _take_stairs()

    // Bidirectional, but not actually a portal - allowed while mesmerised, but
    // not when otherwise unable to move.
    if (ftype == DNGN_PASSAGE_OF_GOLUBRIA || ftype == DNGN_TRANSPORTER)
        return true;

    // Mesmerised
    if (you.beheld() && !you.confused())
    {
        const monster* beholder = you.get_any_beholder();
        mprf("You cannot move away from %s!",
             beholder->name(DESC_THE, true).c_str());
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

    // Rune locks
    switch (ftype)
    {
    case DNGN_EXIT_VAULTS:
        if (runes_in_pack() < 1)
        {
            mpr("You need a rune to leave the Vaults.");
            return false;
        }
        break;
    case DNGN_ENTER_ZOT:
        if (runes_in_pack() < 3 && !crawl_state.game_is_descent())
        {
            mpr("You need at least three runes to enter the Realm of Zot.");
            return false;
        }
        break;
    default:
        break;
    }

    return true;
}

static bool _marker_vetoes_stair()
{
    return marker_vetoes_operation("veto_stair");
}

static bool _prompt_unique_pan_rune(dungeon_feature_type ygrd)
{
    if (ygrd != DNGN_TRANSIT_PANDEMONIUM
        && ygrd != DNGN_EXIT_PANDEMONIUM
        && ygrd != DNGN_EXIT_THROUGH_ABYSS)
    {
        return true;
    }

    item_def* rune = find_floor_item(OBJ_RUNES);
    if (rune && item_is_unique_rune(*rune))
    {
        return yes_or_no("A rune of Zot still resides in this realm, "
                         "and once you leave you can never return. "
                         "Are you sure you want to leave?");
    }
    return true;
}

static bool _prompt_stairs(dungeon_feature_type ygrd, bool down, bool shaft)
{
    // Certain portal types always carry warnings.
    if (!prompt_dangerous_portal(ygrd))
    {
        canned_msg(MSG_OK);
        return false;
    }

    // Descent mode prompts for "atypical" branch order that skips content
    if (crawl_state.game_is_descent() && !prompt_descent_shortcut(ygrd))
    {
        canned_msg(MSG_OK);
        return false;
    }

    // Does the next level have a warning annotation, or would you be bezotted
    // there?
    if (!check_next_floor_warning())
        return false;

    // Prompt for entering excluded transporters.
    if (ygrd == DNGN_TRANSPORTER && is_exclude_root(you.pos()))
    {
        mprf(MSGCH_WARN, "This transporter is marked as excluded!");
        if (!yesno("Enter transporter anyway?", true, 'n', true, false))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

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

    // Leaving ziggurat figurines behind.
    if (ygrd == DNGN_EXIT_ZIGGURAT
        && you.depth == brdepth[BRANCH_ZIGGURAT]
        && find_floor_item(OBJ_MISCELLANY, MISC_ZIGGURAT))
    {
        if (!yesno("Really leave the ziggurat figurine behind?", false, 'n'))
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

    if (!down && player_in_branch(BRANCH_ZOT)
        && you.depth == brdepth[BRANCH_ZOT]
        && you.chapter == CHAPTER_ANGERED_PANDEMONIUM)
    {
        if (!yesno("Really leave the Orb behind?", false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
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

    if (ygrd != DNGN_TRANSPORTER && ygrd != DNGN_PASSAGE_OF_GOLUBRIA
        && beogh_cancel_leaving_floor())
    {
        return false;
    }

    if (Options.warn_hatches)
    {
        if (feat_is_escape_hatch(ygrd))
        {
            if (!yesno("Really go through this one-way escape hatch?", true, 'n'))
            {
                canned_msg(MSG_OK);
                return false;
            }
        }

        if (down && shaft) // voluntary shaft usage
        {
            if (!yesno("Really dive through this shaft in the floor?", true, 'n'))
            {
                canned_msg(MSG_OK);
                return false;
            }
        }
    }

    if (down && ygrd == DNGN_ENTER_VAULTS && !runes_in_pack())
    {
        if (!yes_or_no("You cannot leave the Vaults without holding a Rune of "
                       "Zot, and the runes within are jealously guarded."
                       " Continue?"))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    return true;
}

static void _take_transporter()
{
    const coord_def old_pos = you.pos();
    coord_def dest = get_transporter_dest(you.pos());

    ASSERT(dest != old_pos);

    if (dest == INVALID_COORD || !you.is_habitable(dest))
    {
        mpr("The transporter is blocked on the other side!");
        return;
    }

    monster *mon = monster_at(dest);
    if (mon)
    {
        // Generally the monster won't fail to relocate unless the destination
        // is in a very poor location. If this somehow becomes both common and
        // important to prevent, this can be changed to swap the player and the
        // monsters. -gammafunk
        if (!mon->find_home_near_place(dest))
        {
            mpr("The transporter is blocked by a creature on the other side!");
            return;
        }
    }

    if (you.move_to_pos(dest, true))
        you.turn_is_over = true;

    if (you.turn_is_over)
    {
        place_cloud(CLOUD_TLOC_ENERGY, old_pos, 1 + random2(3), &you);
        transport_followers_from(old_pos);
        if (is_unknown_transporter(old_pos))
        {
            LevelInfo *li = travel_cache.find_level_info(level_id::current());
            ASSERT(li);
            li->update_transporter(old_pos, you.pos());
            explored_tracked_feature(DNGN_TRANSPORTER);
        }
        cancel_polar_vortex();
        mpr("You enter the transporter and appear at another place.");
        id_floor_items();
    }
}

static void _take_stairs(bool down)
{
    ASSERT(!crawl_state.game_is_arena());
    ASSERT(!crawl_state.arena_suspended);

    const dungeon_feature_type ygrd = env.grid(you.pos());

    const bool shaft = (down && get_trap_type(you.pos()) == TRAP_SHAFT);

    if (!_can_take_stairs(ygrd, down, shaft))
        return;

    if (you.attribute[ATTR_HELD])
    {
        free_self_from_net();
        you.turn_is_over = true;
        return;
    }

    if (!(!cancel_harmful_move()
          && _prompt_stairs(ygrd, down, shaft)
          && you.attempt_escape())) // false means constricted and don't escape
    {
        return;
    }

    you.stop_constricting_all(true);
    you.stop_being_constricted();

    if (shaft)
        start_delay<DescendingStairsDelay>(0);
    else if (ygrd == DNGN_TRANSPORTER)
        _take_transporter();
    else if (get_trap_type(you.pos()) == TRAP_GOLUBRIA)
    {
        coord_def old_pos = you.pos();
        bool trap_triggered = you.handle_trap();
        // only returns false if no trap was found, which shouldn't happen
        ASSERT(trap_triggered);
        you.turn_is_over = (you.pos() != old_pos);
        if (you.turn_is_over)
            id_floor_items();
    }
    else
    {
        tag_followers(); // Only those beside us right now can follow.
        if (down)
            start_delay<DescendingStairsDelay>(1);
        else if (crawl_state.game_is_descent())
            up_stairs();
        else
            start_delay<AscendingStairsDelay>(1);
        id_floor_items();
    }
}

static void _experience_check()
{
    mprf("You are a level %d %s %s.",
         you.experience_level,
         species::name(you.species).c_str(),
         get_job_name(you.char_class));
    int perc = get_exp_progress();

    if (you.experience_level < you.get_max_xl())
    {
        mprf("You are %d%% of the way to level %d.", perc,
              you.experience_level + 1);
    }
    else
    {
        mprf("I'm sorry, level %d is as high as you can go.", you.get_max_xl());
        mpr("With the way you've been playing, I'm surprised you got this far.");
    }

    if (you.has_mutation(MUT_MULTILIVED))
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
        you.lives < 2 ?
             mprf("You'll get an extra life in %d.%02d levels' worth of XP.", perc / 100, perc % 100) :
             mprf("If you died right now, you'd get an extra life in %d.%02d levels' worth of XP.",
             perc / 100 , perc % 100);
    }

    handle_real_time();
    msg::stream << "Play time: " << make_time_string(you.real_time())
                << " (" << you.num_turns << " turns)."
                << endl;

    if (!crawl_state.game_is_sprint())
    {
        if (zot_immune())
            msg::stream << "You are forever immune to Zot's power.";
        else if (player_in_branch(BRANCH_ABYSS))
            msg::stream << "You have unlimited time to explore this branch.";
        else
        {
            msg::stream << "Zot will find you in " << turns_until_zot()
                        << " turns if you stay in this branch and explore no"
                        << " new floors.";
        }
        msg::stream << endl << gem_status();
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Turns spent on this level: %d",
         env.turns_on_level);
#endif
}

static void _do_rest()
{

#ifdef WIZARD
    if (you.props.exists(FREEZE_TIME_KEY))
    {
        mprf(MSGCH_WARN, "Cannot rest while time is frozen.");
        return;
    }
#endif

    if (should_fear_zot() && !yesno("Really rest while Zot is near?", false, 'n'))
    {
        canned_msg(MSG_OK);
        return;
    }

    if (i_feel_safe() && can_rest_here())
    {
        if (you.is_sufficiently_rested(true) && ancestor_full_hp())
        {
            mpr("You start waiting.");
            _start_running(RDIR_REST, RMODE_WAIT_DURATION);
            return;
        }
        else
            mpr("You start resting.");
    }
    // intentional fallthrough for else case! Messaging is handled in
    // _start_running, update the corresponding conditional there if you
    // change this one.

    _start_running(RDIR_REST, RMODE_REST_DURATION);
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
    const bool travel = show_map(pos, true, true);

#ifdef USE_TILE_LOCAL
    mpr("Returning to the game...");
#endif
    if (travel)
        start_translevel_travel(pos);
}

static void _do_cycle_quiver(int dir)
{
    const bool changed = you.quiver_action.cycle(dir);
    quiver::set_needs_redraw();

    const bool valid = you.quiver_action.get()->is_valid();

    if (!changed || !valid)
    {
        // `others`: there are quiverable but uncyclable actions.
        // Things could be excluded from cycling via inscriptions, custom
        // fire_order, or setting fire_items_start, and still available from
        // the menu. This messaging still excludes stuff that requires
        // force-quivering, e.g. zigfigs
        const bool others = !valid && quiver::anything_to_quiver();
        mprf("No %squiver actions available for cycling.%s",
            valid ? "other " : "",
            others ? " Use [<white>Q</white>] to select from all actions."
                   : "");
    }
}

static void _do_list_gold()
{
    if (shopping_list.empty())
    {
        mprf("You have %d gold piece%s.", you.gold, you.gold != 1 ? "s" : "");
        int vouchers = you.attribute[ATTR_VOUCHER];
        if (vouchers > 0)
            mprf("You also have %d voucher%s.", vouchers, vouchers > 1 ? "s" : "");
    }
    else
        shopping_list.display();
}

static bool _check_recklessness(command_type prev_cmd)
{
    if (Options.autofight_warning > 0
        && !is_processing_macro()
        && you.real_time_delta
                        <= chrono::milliseconds(Options.autofight_warning)
        && (prev_cmd == CMD_AUTOFIGHT
            || prev_cmd == CMD_AUTOFIGHT_NOMOVE
            || prev_cmd == CMD_AUTOFIRE))
    {
        mprf(MSGCH_DANGER, "You should not fight recklessly!");
        return true;
    }
    return false;
}

static const string _autofight_lua_fn(command_type cmd)
{
    switch (cmd)
    {
    case CMD_AUTOFIRE:
        return "fire_closest";
    case CMD_AUTOFIGHT_NOMOVE:
        return "hit_closest_nomove";
    case CMD_AUTOFIGHT:
        return "hit_closest";
    default:
        die("Unknown autofight command");
    }
}

static void _handle_autofight(command_type cmd, command_type prev_cmd)
{
    // This will lead to recklessness messages taking priority over disabled
    // quiver messages -- is this good?
    if (_check_recklessness(prev_cmd))
        return;

    if (cmd == CMD_AUTOFIRE)
    {
        auto a = quiver::get_secondary_action();
        if (!a || !a->is_valid())
        {
            mpr("Nothing quivered!"); // Can this happen?
            return;
        }

        const bool secondary_enabled = a->is_enabled();

        // Some quiver actions need to be triggered directly. Disabled quiver
        // actions are also triggered here for messaging purposes -- the errors
        // are more informative than what you'd get through autofight.
        // TODO: this probably breaks autofight_wait for these cases?
        if (!a->use_autofight_targeting() || !secondary_enabled)
        {
            dist target;
            // TODO is there a better way to handle this division of labor??
            if (!clua.callfn("get_af_fire_stop", ">b", &target.isEndpoint))
            {
                if (!clua.error.empty())
                    mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
                // just continue with the default value in this case
            }
            // set this so that it prints appropriate error messages for
            // autofiring
            if (!secondary_enabled)
                target.find_target = true;
            a->trigger(target);
            return;
        }

        // quiver actions that aren't called directly above are triggered via
        // autofight code below
    }

    const string fnname = _autofight_lua_fn(cmd);

    if (!clua.callfn(fnname.c_str(), 0, 0))
        mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
}

class GameMenu : public Menu
{
// this could be easily generalized for other menus that select among commands
// if it's ever needed
public:
    class CmdMenuEntry : public MenuEntry
    {
    public:
        CmdMenuEntry(string label, MenuEntryLevel _level, int hotk=0,
                                                command_type _cmd=CMD_NO_CMD,
                                                bool _uses_popup=true)
            : MenuEntry(label, _level, 1, hotk), cmd(_cmd),
              uses_popup(_uses_popup)
        {
            if (tileidx_command(cmd) != TILEG_TODO)
                add_tile(tileidx_command(cmd));
        }

        command_type cmd;
        bool uses_popup;
    };

    command_type cmd;
    GameMenu()
        : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING
                | MF_ARROWS_SELECT | MF_WRAP | MF_INIT_HOVER
#ifdef USE_TILE_LOCAL
                | MF_SPECIAL_MINUS // doll editor (why?)
#endif
                ),
          cmd(CMD_NO_CMD)
    {
        set_tag("game_menu");
        action_cycle = Menu::CYCLE_NONE;
        menu_action  = Menu::ACT_EXECUTE;
        set_title(new MenuEntry(
            string("<w>" CRAWL " ") + Version::Long + "</w>",
            MEL_TITLE));
        on_single_selection = [this](const MenuEntry& item)
        {
            const CmdMenuEntry *c = dynamic_cast<const CmdMenuEntry *>(&item);
            if (c)
            {
                if (c->uses_popup)
                {
                    // recurse
                    if (c->cmd != CMD_NO_CMD)
                        ::process_command(c->cmd, CMD_GAME_MENU);
                    return true;
                }
                // otherwise, exit menu and process in the main process_command call
                cmd = c->cmd;
                return false;
            }
            return true;
        };
    }

    bool skip_process_command(int keyin) override
    {
        if (keyin == '?')
            return true; // hotkeyed
        return Menu::skip_process_command(keyin);
    }

    void fill_entries()
    {
        clear();
        add_entry(new CmdMenuEntry("", MEL_SUBTITLE));
        add_entry(new CmdMenuEntry("Return to game", MEL_ITEM, CK_ESCAPE,
            CMD_NO_CMD, false));
        items[1]->add_tile(tileidx_command(CMD_GAME_MENU));
        // n.b. CMD_SAVE_GAME_NOW crashes on returning to the main menu if we
        // don't exit out of this popup now, not sure why
        add_entry(new CmdMenuEntry(
            (crawl_should_restart(game_exit::save)
                            ? "Save and return to main menu"
                            : "Save and exit"),
            MEL_ITEM, 'S', CMD_SAVE_GAME_NOW, false));
        add_entry(new CmdMenuEntry("Generate and view character dump",
            MEL_ITEM, '#', CMD_SHOW_CHARACTER_DUMP));
#ifdef USE_TILE_LOCAL
        add_entry(new CmdMenuEntry("Edit player tile",
            MEL_ITEM, '-', CMD_EDIT_PLAYER_TILE));
#endif
        add_entry(new CmdMenuEntry("Edit macros",
            MEL_ITEM, '~', CMD_MACRO_MENU));
        add_entry(new CmdMenuEntry("Help and manual",
            MEL_ITEM, '?', CMD_DISPLAY_COMMANDS));
        add_entry(new CmdMenuEntry("Lookup info",
            MEL_ITEM, '/', CMD_LOOKUP_HELP));
#ifdef TARGET_OS_MACOSX
        add_entry(new CmdMenuEntry("Show options file in finder",
            MEL_ITEM, 'O', CMD_REVEAL_OPTIONS));
#endif
#ifdef __ANDROID__
        add_entry(new CmdMenuEntry("Toggle on-screen keyboard",
            MEL_ITEM, CK_F12, CMD_TOGGLE_KEYBOARD));
#endif
        add_entry(new CmdMenuEntry("", MEL_SUBTITLE));
        add_entry(new CmdMenuEntry(
                            "Quit and <lightred>abandon character</lightred>",
            MEL_ITEM, 'Q', CMD_QUIT, false));
    }

    vector<MenuEntry *> show(bool reuse_selections = false) override
    {
        fill_entries();
        return Menu::show(reuse_selections);
    }
};

// Note that in some actions, you don't want to clear afterwards.
// e.g. list_jewellery, etc.
// calling this directly will not record the command for later replay; if you
// want to ensure that it's recorded, see macro.cc:process_command_on_record.
void process_command(command_type cmd, command_type prev_cmd)
{
    you.apply_berserk_penalty = true;

    if (cmd == CMD_GAME_MENU)
    {
        GameMenu m;
        m.show();
        if (m.cmd == CMD_NO_CMD)
            return;
        cmd = m.cmd;
    }

    switch (cmd)
    {
#ifdef USE_TILE
        // Tiles-specific commands.
    case CMD_EDIT_PLAYER_TILE: tiles.draw_doll_edit(); break;
#endif

        // Movement and running commands.
    case CMD_ATTACK_DOWN_LEFT:  _swing_at_target({-1,  1}); break;
    case CMD_ATTACK_DOWN:       _swing_at_target({ 0,  1}); break;
    case CMD_ATTACK_UP_RIGHT:   _swing_at_target({ 1, -1}); break;
    case CMD_ATTACK_UP:         _swing_at_target({ 0, -1}); break;
    case CMD_ATTACK_UP_LEFT:    _swing_at_target({-1, -1}); break;
    case CMD_ATTACK_LEFT:       _swing_at_target({-1,  0}); break;
    case CMD_ATTACK_DOWN_RIGHT: _swing_at_target({ 1,  1}); break;
    case CMD_ATTACK_RIGHT:      _swing_at_target({ 1,  0}); break;

    case CMD_MOVE_DOWN_LEFT:  move_player_action({-1,  1}); break;
    case CMD_MOVE_DOWN:       move_player_action({ 0,  1}); break;
    case CMD_MOVE_UP_RIGHT:   move_player_action({ 1, -1}); break;
    case CMD_MOVE_UP:         move_player_action({ 0, -1}); break;
    case CMD_MOVE_UP_LEFT:    move_player_action({-1, -1}); break;
    case CMD_MOVE_LEFT:       move_player_action({-1,  0}); break;
    case CMD_MOVE_DOWN_RIGHT: move_player_action({ 1,  1}); break;
    case CMD_MOVE_RIGHT:      move_player_action({ 1,  0}); break;

    case CMD_SAFE_MOVE_DOWN_LEFT:  _safe_move_player({-1,  1}); break;
    case CMD_SAFE_MOVE_DOWN:       _safe_move_player({ 0,  1}); break;
    case CMD_SAFE_MOVE_UP_RIGHT:   _safe_move_player({ 1, -1}); break;
    case CMD_SAFE_MOVE_UP:         _safe_move_player({ 0, -1}); break;
    case CMD_SAFE_MOVE_UP_LEFT:    _safe_move_player({-1, -1}); break;
    case CMD_SAFE_MOVE_LEFT:       _safe_move_player({-1,  0}); break;
    case CMD_SAFE_MOVE_DOWN_RIGHT: _safe_move_player({ 1,  1}); break;
    case CMD_SAFE_MOVE_RIGHT:      _safe_move_player({ 1,  0}); break;

    case CMD_CLOSE_DOOR_DOWN_LEFT:  close_door_action({-1,  1}); break;
    case CMD_CLOSE_DOOR_DOWN:       close_door_action({ 0,  1}); break;
    case CMD_CLOSE_DOOR_UP_RIGHT:   close_door_action({ 1, -1}); break;
    case CMD_CLOSE_DOOR_UP:         close_door_action({ 0, -1}); break;
    case CMD_CLOSE_DOOR_UP_LEFT:    close_door_action({-1, -1}); break;
    case CMD_CLOSE_DOOR_LEFT:       close_door_action({-1,  0}); break;
    case CMD_CLOSE_DOOR_DOWN_RIGHT: close_door_action({ 1,  1}); break;
    case CMD_CLOSE_DOOR_RIGHT:      close_door_action({ 1,  0}); break;
    case CMD_CLOSE_DOOR:            close_door_action({ 0,  0}); break;

    case CMD_RUN_DOWN_LEFT: _start_running(RDIR_DOWN_LEFT, RMODE_START); break;
    case CMD_RUN_DOWN:      _start_running(RDIR_DOWN, RMODE_START);      break;
    case CMD_RUN_UP_RIGHT:  _start_running(RDIR_UP_RIGHT, RMODE_START);  break;
    case CMD_RUN_UP:        _start_running(RDIR_UP, RMODE_START);        break;
    case CMD_RUN_UP_LEFT:   _start_running(RDIR_UP_LEFT, RMODE_START);   break;
    case CMD_RUN_LEFT:      _start_running(RDIR_LEFT, RMODE_START);      break;
    case CMD_RUN_DOWN_RIGHT:_start_running(RDIR_DOWN_RIGHT, RMODE_START); break;
    case CMD_RUN_RIGHT:     _start_running(RDIR_RIGHT, RMODE_START);     break;

    case CMD_AUTOFIRE:
    case CMD_AUTOFIGHT:
    case CMD_AUTOFIGHT_NOMOVE:
        _handle_autofight(cmd, prev_cmd);
        break;

    case CMD_REST:           _do_rest(); break;

    case CMD_GO_UPSTAIRS:
    case CMD_GO_DOWNSTAIRS:
        _take_stairs(cmd == CMD_GO_DOWNSTAIRS);
        break;

    case CMD_OPEN_DOOR:      open_door_action(); break;

    // Repeat commands.
    case CMD_REPEAT_CMD:     _do_cmd_repeat();  break;
    case CMD_PREV_CMD_AGAIN: _do_prev_cmd_again(); break;
    case CMD_MACRO_ADD:      macro_quick_add();    break;
    case CMD_MACRO_MENU:     macro_menu();    break;

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

#ifdef USE_SOUND
    case CMD_TOGGLE_SOUND:
        Options.sounds_on = !Options.sounds_on;
        mprf("Sound effects are now %s.", Options.sounds_on ? "on" : "off");
        break;
#endif

        // Map commands.
    case CMD_CLEAR_MAP:       clear_map_or_travel_trail(); break;
    case CMD_DISPLAY_OVERMAP: display_overview(); break;
    case CMD_DISPLAY_MAP:     _do_display_map(); break;

#ifdef USE_TILE
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
        if (player_on_single_stack() && !you.running)
            pickup(true);
        else
            // Forced autopickup if CMD_INSPECT_FLOOR is used twice in a row
            autopickup(prev_cmd == CMD_INSPECT_FLOOR);
        break;
    case CMD_SHOW_TERRAIN: toggle_show_terrain(); break;
    case CMD_ADJUST_INVENTORY: adjust(); break;

    case CMD_SAFE_WAIT:
        if (!i_feel_safe(true) && can_rest_here(true))
            break;
        // else fall-through
    case CMD_WAIT:
        update_acrobat_status();
        you.turn_is_over = true;
        break;

    case CMD_PICKUP:
    case CMD_PICKUP_QUANTITY:
        pickup(cmd != CMD_PICKUP);
        break;

        // Action commands.
    case CMD_CAST_SPELL:           do_cast_spell_cmd(false); break;
    case CMD_DISPLAY_SPELLS:       inspect_spells();         break;
    case CMD_FIRE:                 you.quiver_action.target(); break;
    case CMD_FORCE_CAST_SPELL:     do_cast_spell_cmd(true);  break;
    case CMD_LOOK_AROUND:          do_look_around();         break;
    case CMD_QUAFF:                use_an_item(OPER_QUAFF);  break;
    case CMD_READ:                 use_an_item(OPER_READ);   break;
    case CMD_UNEQUIP:              use_an_item(OPER_UNEQUIP); break;
    case CMD_REMOVE_ARMOUR:        use_an_item(OPER_TAKEOFF); break;
    case CMD_REMOVE_JEWELLERY:     use_an_item(OPER_REMOVE); break;
    case CMD_SHOUT:                issue_orders();           break;
    case CMD_FIRE_ITEM_NO_QUIVER:  fire_item_no_quiver();    break;
    case CMD_WEAPON_SWAP:          auto_wield();             break;
    case CMD_EQUIP:                use_an_item(OPER_EQUIP);  break;
    case CMD_WEAR_ARMOUR:          use_an_item(OPER_WEAR);   break;
    case CMD_WEAR_JEWELLERY:       use_an_item(OPER_PUTON);  break;
    case CMD_WIELD_WEAPON:         use_an_item(OPER_WIELD);  break;
    case CMD_EVOKE:                use_an_item(OPER_EVOKE);  break;
    case CMD_ZAP_WAND:             zap_wand();               break;
    case CMD_DROP:
        drop();
        break;

    case CMD_DROP_LAST:
        drop_last();
        break;

    case CMD_PRIMARY_ATTACK:
        quiver::get_primary_action()->trigger();
        if (!you.turn_is_over)
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
    case CMD_DISPLAY_COMMANDS:
        show_help();
        redraw_screen();
        update_screen();
        break;
    case CMD_DISPLAY_INVENTORY:        display_inventory();            break;
    case CMD_DISPLAY_KNOWN_OBJECTS:
        check_item_knowledge();
        redraw_screen();
        update_screen();
        break;
    case CMD_DISPLAY_MUTATIONS:
        display_mutations();
        redraw_screen();
        update_screen();
        break;
    case CMD_DISPLAY_RUNES:
        display_runes();
        redraw_screen();
        update_screen();
        break;
    case CMD_DISPLAY_SKILLS:
        skill_menu();
        redraw_screen();
        update_screen();
        break;
    case CMD_EXPERIENCE_CHECK:         _experience_check();            break;
    case CMD_FULL_VIEW:                full_describe_view();           break;
    case CMD_INSCRIBE_ITEM:            prompt_inscribe_item();         break;
    case CMD_LIST_ARMOUR:              list_armour();                  break;
    case CMD_LIST_GOLD:                _do_list_gold();                break;
    case CMD_LIST_JEWELLERY:           list_jewellery();               break;
    case CMD_MAKE_NOTE:                make_user_note();               break;
    case CMD_REPLAY_MESSAGES:
        replay_messages();
        redraw_screen();
        update_screen();
        break;
    case CMD_RESISTS_SCREEN:           print_overview_screen();        break;
    case CMD_LOOKUP_HELP:
        keyhelp_query_descriptions(prev_cmd == CMD_GAME_MENU
                                                ? prev_cmd : CMD_NO_CMD);
        break;

    case CMD_DISPLAY_RELIGION:
    {
        describe_god(you.religion);
        redraw_screen();
        update_screen();
        break;
    }

    case CMD_READ_MESSAGES:
#ifdef DGL_SIMPLE_MESSAGING
        if (SysEnv.have_messages)
            read_messages();
#endif
        break;

#ifdef TARGET_OS_MACOSX
    case CMD_REVEAL_OPTIONS:
        // TODO: implement for other OSs
        // TODO: add a way of triggering this from the main menu
        system(make_stringf("/usr/bin/open -R '%s'",
                                            Options.filename.c_str()).c_str());
        break;
#endif
    case CMD_SHOW_CHARACTER_DUMP:
    case CMD_CHARACTER_DUMP:
        if (!dump_char(you.your_name))
            mpr("Char dump unsuccessful! Sorry about that.");
#ifdef USE_TILE_WEB
        else
            tiles.send_dump_info("command", you.your_name);
#endif
        if (cmd == CMD_SHOW_CHARACTER_DUMP)
            display_char_dump();
        break;

        // Travel commands.
    case CMD_FIX_WAYPOINT:      travel_cache.add_waypoint(); break;
    case CMD_INTERLEVEL_TRAVEL: do_interlevel_travel();      break;
    case CMD_ANNOTATE_LEVEL:    do_annotate();               break;
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
                    canned_msg(MSG_CANNOT_SEE);
            }
        }
        break;
    }

        // Quiver commands.
    case CMD_QUIVER_ITEM:           quiver::choose(you.quiver_action); break;
    case CMD_CYCLE_QUIVER_FORWARD:  _do_cycle_quiver(+1);     break;
    case CMD_CYCLE_QUIVER_BACKWARD: _do_cycle_quiver(-1);     break;
    case CMD_SWAP_QUIVER_RECENT:
    {
        auto a = you.quiver_action.find_last_valid();
        if (a)
            you.quiver_action.set(a);
        quiver::set_needs_redraw();
        break;
    }

#ifdef WIZARD
    case CMD_WIZARD: handle_wizard_command(); break;
    case CMD_EXPLORE_MODE: enter_explore_mode(); break;
#endif

        // Game commands.
    case CMD_REDRAW_SCREEN:
        redraw_screen();
        update_screen();
        break;

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
        update_screen();
        break;
#endif

    case CMD_SAVE_GAME:
    {
        const char * const prompt
            = (crawl_should_restart(game_exit::save))
              ? "Save game and return to main menu?"
              : "Save game and exit?";
        explicit_keymap map;
        map['S'] = 'y';
        if (yesno(prompt, true, 'n', true, true, false, &map))
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
    {
        // TODO: msg whether this will start a new game? not very important
        if (crawl_state.disables[DIS_CONFIRMATIONS]
            || yes_or_no("Are you sure you want to abandon this character%s?",
                Options.newgame_after_quit ? "" : // hard to predict this case
                (crawl_should_restart(game_exit::quit)
                                            ? " and return to the main menu"
                                            : " and quit the game")))
        {
            ouch(INSTANT_DEATH, KILLED_BY_QUITTING);
        }
        else
            canned_msg(MSG_OK);
        break;
    }

    case CMD_LUA_CONSOLE:
        debug_terp_dlua(clua);
        break;

#ifdef __ANDROID__
    case CMD_TOGGLE_TAB_ICONS:
        tiles.toggle_tab_icons();
        break;

    case CMD_TOGGLE_KEYBOARD:
        jni_keyboard_control(true);
        break;
#endif

    case CMD_NO_CMD:
    default:
        // The backslash in ?\? is there so it doesn't start a trigraph.
        if (crawl_state.game_is_hints())
            mpr("Unknown command. (For a list of commands type <w>?\?</w>.)");
        else // well, not examine, but...
            mprf(MSGCH_EXAMINE_FILTER, "Unknown command.");

        if (feat_is_altar(env.grid(you.pos())))
        {
            string msg = "Press <w>%</w> or <w>%</w> to pray at altars.";
            insert_commands(msg, { CMD_GO_UPSTAIRS, CMD_GO_DOWNSTAIRS });
            mpr(msg);
        }

        break;
    }

}

static void _prep_input()
{
    you.turn_is_over = false;
    you.time_taken = player_speed();
    you.shield_blocks = 0;              // no blocks this round

    you.redraw_status_lights = true;
    if (you.running == 0)
    {
        you.quiver_action.set_needs_redraw();
        you.refresh_rampage_hints();
    }
    print_stats();
    update_screen();

    viewwindow();
    update_screen(); // ???
    maybe_update_stashes();
    if (check_for_interesting_features() && you.running.is_explore())
        stop_running();

    if (you.seen_portals)
    {
        ASSERT(have_passive(passive_t::detect_portals));
        if (you.seen_portals == 1)
            mprf(MSGCH_GOD, "You have a vision of a gate.");
        else
            mprf(MSGCH_GOD, "You have a vision of multiple gates.");

        you.seen_portals = 0;
    }
}

static void _check_banished()
{
    if (you.banished)
    {
        you.banished = false;
        ASSERT(brdepth[BRANCH_ABYSS] != -1);
        if (!player_in_branch(BRANCH_ABYSS))
            mprf(MSGCH_BANISHMENT, "You are cast into the Abyss!");
        else if (you.depth < brdepth[BRANCH_ABYSS])
            mprf(MSGCH_BANISHMENT, "You are cast deeper into the Abyss!");
        else
            mprf(MSGCH_BANISHMENT, "The Abyss bends around you!");
        // these are included in default force_more_message
        banished(you.banished_by, you.banished_power);
    }
}

static void _check_sanctuary()
{
    if (env.sanctuary_time <= 0)
        return;

    decrease_sanctuary_radius();
}

static void _check_trapped()
{
    if (you.trapped)
    {
        do_trap_effects();
        you.trapped = false;
    }
}

static void _update_golubria_traps(int dur)
{
    vector<coord_def> traps = find_golubria_on_level();
    for (auto c : traps)
    {
        trap_def *trap = trap_at(c);
        if (trap && trap->type == TRAP_GOLUBRIA)
        {
            trap->ammo_qty -= div_rand_round(dur, BASELINE_DELAY);
            if (trap->ammo_qty <= 0)
            {
                if (you.see_cell(c))
                    mpr("Your passage of Golubria closes with a snap!");
                else
                    mprf(MSGCH_SOUND, "You hear a snapping sound.");
                trap->destroy();
                noisy(spell_effect_noise(SPELL_GOLUBRIAS_PASSAGE), c);
            }
        }
    }
    if (traps.empty())
        env.level_state &= ~LSTATE_GOLUBRIA;
}

static void _update_still_winds()
{
    for (monster_iterator mon_it; mon_it; ++mon_it)
        if (mon_it->has_ench(ENCH_STILL_WINDS))
            return;
    // still winds somehow ended without the flag being unset. fix it
    end_still_winds();
}

void world_reacts()
{
    // All markers should be activated at this point.
    ASSERT(!env.markers.need_activate());

    you.rampage_hints.clear(); // only draw on your turn

    fire_final_effects();

    if (crawl_state.viewport_monster_hp || crawl_state.viewport_weapons)
    {
        crawl_state.viewport_monster_hp = false;
        crawl_state.viewport_weapons = false;
        viewwindow();
        update_screen();
    }

    // prevent monsters wandering into view and picking up an item before
    // our next prep_input
    maybe_update_stashes();
    update_monsters_in_view();

    reset_show_terrain();

    crawl_state.clear_mon_acting();

    if (you.time_taken)
        descent_crumble_stairs();

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
    _check_sanctuary();
    _check_trapped();
    check_spectral_weapon(you);

    run_environment_effects();

    if (!crawl_state.game_is_arena())
        player_reacts();

    abyss_morph();
    apply_noises();
    handle_monsters(true);

    // Monsters can schedule final effects, too!
    // (mostly by exploding)
    fire_final_effects();

    _check_banished();

    ASSERT(you.time_taken >= 0);
    you.elapsed_time += you.time_taken;
    if (you.elapsed_time >= 2*1000*1000*1000)
    {
        // 2B of 1/10 turns. A 32-bit signed int can hold 2.1B.
        // The worst case of mummy scumming had 92M turns, the second worst
        // merely 8M. This limit is ~200M turns, with an efficient bot that
        // keeps resting on a fast machine, it takes ~24 hours to hit it
        // on a level with no monsters, at 100% CPU utilization, producing
        // a gigabyte of bzipped ttyrec.
        // We could extend the counters to 64 bits, but in the light of the
        // above, it's an useless exercise.
        mpr("Outside, the world ends.");
        mpr("Sorry, but your quest for the Orb is now rather pointless. "
            "You quit...");
        // Please do not give it a custom ktyp or make it cool in any way
        // whatsoever, because players are insane. Usually, not being dragged
        // down by sanity is good, but this is not the case here.
        ouch(INSTANT_DEATH, KILLED_BY_QUITTING);
    }

    handle_time();
    manage_clouds();
    if (env.level_state & LSTATE_GOLUBRIA)
        _update_golubria_traps(you.time_taken);
    if (env.level_state & LSTATE_STILL_WINDS)
        _update_still_winds();
    if (!crawl_state.game_is_arena())
        player_reacts_to_monsters();

    clear_monster_flags();

    add_auto_excludes();

    viewwindow();
    update_screen();

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
        if (you.num_turns < INT_MAX)
            you.num_turns++;

        _update_place_stats();

        if (env.turns_on_level < INT_MAX)
            env.turns_on_level++;
        record_turn_timestamp();
        update_turn_count();
        msgwin_new_turn();
        crawl_state.lua_calls_no_turn = 0;
        if ((crawl_state.game_is_sprint() && !(you.num_turns % 256)
                || crawl_state.save_after_turn)
            && !you_are_delayed()
            && !crawl_state.disables[DIS_SAVE_CHECKPOINTS])
        {
            // Resting makes the saving quite random, but meh.
            crawl_state.save_after_turn = false;
            save_level(level_id::current());
            save_game(false);
        }
    }
    // End of a turn.
    //
    // `los_noise_last_turn` is the value for display -- it needs to persist
    // for any calls to print_stats during the next turn. Meanwhile, reset
    // the loudest noise tracking for the next world_reacts cycle.
    you.los_noise_last_turn = you.los_noise_level;
    you.los_noise_level = 0;
}

static command_type _get_next_cmd()
{
#ifdef DGL_SIMPLE_MESSAGING
    check_messages();
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
#ifndef USE_TILE_LOCAL
    // ignore all input if the terminal is too small
    if (crawl_state.smallterm)
        return CMD_NEXT_CMD;
#endif

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
    keycode_type keyin = 0;

    flush_input_buffer(FLUSH_BEFORE_COMMAND);

    mouse_control mc(MOUSE_MODE_COMMAND);
    for (;;)
    {
        keyin = unmangle_direction_keys(getch_with_command_macros());
        if (keyin == CK_REDRAW)
        {
            redraw_screen();
            update_screen();
        }
        else
            break;
    }

    // This is the main clear_messages() with Option.clear_messages.
    if (!is_synthetic_key(keyin))
        clear_messages();

    return keyin;
}

static void _swing_at_target(coord_def move)
{
    dist target;
    target.target = you.pos() + move;

    if (never_harm_monster(&you, monster_at(target.target), true))
        return;

    // Don't warn the player "too injured to fight recklessly" when they
    // explicitly request an attack.
    unwind_bool autofight_ok(crawl_state.skip_autofight_check, true);
    // this lets ranged weapons work via this command also -- good or bad?
    quiver::get_primary_action()->trigger(target);
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
    if (you.berserk())
    {
        you.berserk_penalty++;

        switch (you.berserk_penalty)
        {
        case 2:
            mprf(MSGCH_DURATION, "You feel a strong urge to attack something.");
            break;
        case 4:
            mprf(MSGCH_DURATION, "You feel your anger nearly subside.");
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

static void _safe_move_player(coord_def move)
{
    if (!i_feel_safe(true)) {
        // Clear out other queued up commands.
        macro_clear_buffers();
        return;
    }
    move_player_action(move);
}

static int _get_num_and_char(const char* prompt, char* buf, int buf_len)
{
    if (prompt != nullptr)
        msgwin_prompt(prompt);
    auto ret = cancellable_get_line(buf, buf_len, nullptr, keyfun_num_and_char, "", "repeat");
    msgwin_reply(buf);
    return ret;
}

static void _cancel_cmd_repeat()
{
    // need to force reset these so that history for again is consistent, even
    // if a repeat didn't get started.
    crawl_state.cancel_cmd_again("", true);
    crawl_state.cancel_cmd_repeat("", true);
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
        // should be prevented in cmd_is_repeatable(). If
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
#ifdef DGAMELAUNCH
        if (i >= 100)
            usleep(500000);
#endif
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

    if (crawl_state.prev_cmd == CMD_NO_CMD || crawl_state.prev_cmd_keys.empty())
    {
        crawl_state.cancel_cmd_again("No previous command to re-do.", true);
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
