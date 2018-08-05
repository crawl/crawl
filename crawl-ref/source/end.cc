/**
 * @file
 * @brief Handle cleanup during shutdown.
 **/

#include "AppHdr.h"

#include "end.h"

#include <cerrno>

#include "abyss.h"
#include "chardump.h"
#include "colour.h"
#include "crash.h"
#include "database.h"
#include "describe.h"
#include "dungeon.h"
#include "god-passive.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "los.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "prompt.h"
#include "religion.h"
#include "startup.h"
#include "state.h"
#include "stringutil.h"
#include "view.h"
#include "xom.h"
#include "ui.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

using namespace ui;

/**
 * Should crawl restart on game end, depending on restart options and options
 * (command-line or RC) that bypass the startup menu?
 *
 * @param saved whether the game ended by saving
 */
bool crawl_should_restart(game_exit exit)
{
#ifdef DGAMELAUNCH
    return false;
#else
#ifdef USE_TILE_WEB
    if (is_tiles() && Options.name_bypasses_menu)
        return false;
#endif
    if (exit == game_exit::crash)
        return false;
    if (exit == game_exit::abort || exit == game_exit::unknown)
        return true; // always restart on aborting out of a menu
    bool ret =
        tobool(Options.restart_after_game, !crawl_state.bypassed_startup_menu);
    if (exit == game_exit::save)
        ret = ret && Options.restart_after_save;
    return ret;
#endif
}

void cio_cleanup()
{
    if (!crawl_state.io_inited)
        return;

    console_shutdown();
    crawl_state.io_inited = false;
}

// Clear some globally defined variables.
static void _clear_globals_on_exit()
{
    clear_rays_on_exit();
    clear_zap_info_on_exit();
    destroy_abyss();
}

bool fatal_error_notification(string error_msg)
{
    if (error_msg.empty())
        return false;

    // for local tiles, if there is no available ui, it's possible that wm
    // initialisation has failed and there's nothing that can be done, so we
    // don't try. On other builds, though, it's just probably early in the
    // initialisation process, and cio_init should be fairly safe.
#ifndef USE_TILE_LOCAL
    if (!ui::is_available() && !crawl_state.build_db)
        cio_init(); // this, however, should be fairly safe
#endif

    mprf(MSGCH_ERROR, "%s", error_msg.c_str());

    if (!ui::is_available() || crawl_state.test || crawl_state.script
        || crawl_state.build_db)
    {
        return false;
    }

    // do the linebreak here so webtiles has it, but it's needed below as well
    linebreak_string(error_msg, 79);
#ifdef USE_TILE_WEB
    tiles.send_exit_reason("error", error_msg);
#endif

    // this is a bit heavy to continue past here in the face of a real crash.
    if (crawl_state.seen_hups)
        return false;

#if (!defined(DGAMELAUNCH)) || defined(DGL_PAUSE_AFTER_ERROR)
#ifdef USE_TILE_WEB
    tiles_crt_popup show_as_popup;
    tiles.set_ui_state(UI_CRT);
#endif

    // TODO: better formatting, maybe use a formatted_scroller?
    // Escape '<'.
    // NOTE: This assumes that the error message doesn't contain
    //       any formatting!
    error_msg = string("Fatal error:\n\n<lightred>")
                       + replace_all(error_msg, "<", "<<");
    error_msg += "</lightred>\n\n<cyan>Hit any key to exit, "
                 "ctrl-p for the full log.</cyan>";

    auto prompt_ui =
                make_shared<Text>(formatted_string::parse_string(error_msg));
    bool done = false;
    prompt_ui->on(Widget::slots.event, [&](wm_event ev) {
        if (ev.type == WME_KEYDOWN)
        {
            if (ev.key.keysym.sym == CONTROL('P'))
            {
                done = false;
                replay_messages();
            }
            else
                done = true;
        }
        else
            done = false;
        return done;
    });

    mouse_control mc(MOUSE_MODE_MORE);
    auto popup = make_shared<ui::Popup>(prompt_ui);
    ui::run_layout(move(popup), done);
#endif

    return true;
}

// Used by do_crash_dump() to tell if the crash happened during exit() hooks.
// Not a part of crawl_state, since that's a global C++ instance which is
// free'd by exit() hooks when exit() is called, and we don't want to reference
// free'd memory.
bool CrawlIsExiting = false;
bool CrawlIsCrashing = false;

NORETURN void end(int exit_code, bool print_error, const char *format, ...)
{
    disable_other_crashes();

    // Let "error" go out of scope for valgrind's sake.
    {
        string error = print_error ? strerror(errno) : "";
        if (format)
        {
            va_list arg;
            va_start(arg, format);
            char buffer[1024];
            vsnprintf(buffer, sizeof buffer, format, arg);
            va_end(arg);

            if (error.empty())
                error = string(buffer);
            else
                error = string(buffer) + ": " + error;

            if (!error.empty() && error[error.length() - 1] != '\n')
                error += "\n";
        }

        if (exit_code)
            fatal_error_notification(error);

#ifdef USE_TILE_WEB
        tiles.shutdown();
#endif

        cio_cleanup();
        msg::deinitialise_mpr_streams();
        _clear_globals_on_exit();
        databaseSystemShutdown();
#ifdef DEBUG_PROPS
        dump_prop_accesses();
#endif

        if (!error.empty())
        {
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "Crawl", "%s", error.c_str());
#endif
            error.clear();
        }
    }

    CrawlIsExiting = true;
    if (exit_code)
        CrawlIsCrashing = true;

#ifdef DEBUG_GLOBALS
    delete real_env;         real_env = 0;
    delete real_crawl_state; real_crawl_state = 0;
    delete real_dlua;        real_dlua = 0;
    delete real_clua;        real_clua = 0;
    delete real_you;         real_you = 0;
    delete real_Options;     real_Options = 0;
#endif

    exit(exit_code);
}

// Delete save files on game end.
static void _delete_files()
{
    crawl_state.need_save = false;
    you.save->unlink();
    delete you.save;
    you.save = 0;
}

NORETURN void screen_end_game(string text)
{
#ifdef USE_TILE_WEB
    tiles.send_exit_reason("quit");
#endif
    crawl_state.cancel_cmd_all();
    _delete_files();

    if (!text.empty())
    {
        auto prompt_ui = make_shared<Text>(
                formatted_string::parse_string(text));
        bool done = false;
        prompt_ui->on(Widget::slots.event, [&](wm_event ev)  {
            return done = ev.type == WME_KEYDOWN;
        });

        mouse_control mc(MOUSE_MODE_MORE);
        auto popup = make_shared<ui::Popup>(prompt_ui);
        ui::run_layout(move(popup), done);
    }

    game_ended(game_exit::abort); // TODO: is this the right exit condition?
}

static game_exit _kill_method_to_exit(kill_method_type kill)
{
    switch (kill)
    {
        case KILLED_BY_QUITTING: return game_exit::quit;
        case KILLED_BY_WINNING:  return game_exit::win;
        case KILLED_BY_LEAVING:  return game_exit::leave;
        default:                 return game_exit::death;
    }
}

static string _exit_type_to_string(game_exit e)
{
    // some of these may be used by webtiles, check before editing
    switch (e)
    {
        case game_exit::unknown: return "unknown";
        case game_exit::win:     return "won";
        case game_exit::leave:   return "bailed out";
        case game_exit::quit:    return "quit";
        case game_exit::death:   return "dead";
        case game_exit::save:    return "save";
        case game_exit::abort:   return "abort";
        case game_exit::crash:   return "crash";
    }
    return "BUGGY EXIT TYPE";
}

NORETURN void end_game(scorefile_entry &se, int hiscore_index)
{
    for (auto &item : you.inv)
        if (item.defined() && item_type_unknown(item))
            add_inscription(item, "unknown");

    identify_inventory();

    _delete_files();

    kill_method_type death_type = (kill_method_type) se.get_death_type();

    // death message
    if (death_type != KILLED_BY_LEAVING && death_type != KILLED_BY_QUITTING
        && death_type != KILLED_BY_WINNING)
    {
        canned_msg(MSG_YOU_DIE);
        xom_death_message(death_type);

        switch (you.religion)
        {
        case GOD_FEDHAS:
            simple_god_message(" appreciates your contribution to the "
                               "ecosystem.");
            break;

        case GOD_NEMELEX_XOBEH:
            nemelex_death_message();
            break;

        case GOD_KIKUBAAQUDGHA:
        {
            const mon_holy_type holi = you.holiness();

            if (holi & (MH_NONLIVING | MH_UNDEAD))
            {
                simple_god_message(" rasps: \"You have failed me! "
                                   "Welcome... oblivion!\"");
            }
            else
            {
                simple_god_message(" rasps: \"You have failed me! "
                                   "Welcome... death!\"");
            }
            break;
        }

        case GOD_YREDELEMNUL:
            if (you.undead_state() != US_ALIVE)
                simple_god_message(" claims you as an undead slave.");
            else if (death_type != KILLED_BY_DISINT
                  && death_type != KILLED_BY_LAVA)
            {
                mprf(MSGCH_GOD, "Your body rises from the dead as a mindless "
                     "zombie.");
            }
            // No message if you're not undead and your corpse is lost.
            break;

        case GOD_BEOGH:
            if (actor* killer = se.killer())
            {
                if (killer->is_monster() && killer->deity() == GOD_BEOGH)
                {
                    const string msg = " appreciates "
                        + killer->name(DESC_ITS)
                        + " killing of a heretic priest.";
                    simple_god_message(msg.c_str());
                }
            }
            break;

        case GOD_PAKELLAS:
        {
            const string result = getSpeakString("Pakellas death");
            god_speaks(GOD_PAKELLAS, result.c_str());
            break;
        }

        default:
            if (will_have_passive(passive_t::goldify_corpses)
                && death_type != KILLED_BY_DISINT
                && death_type != KILLED_BY_LAVA)
            {
                mprf(MSGCH_GOD, "Your body crumbles into a pile of gold.");
            }
            break;
        }

        flush_prev_message();
        viewwindow(); // don't do for leaving/winning characters

        if (crawl_state.game_is_hints())
            hints_death_screen();
    }

    string fname = morgue_name(you.your_name, se.get_death_time());
    if (!dump_char(fname, true, true, &se))
        mpr("Char dump unsuccessful! Sorry about that.");
#ifdef USE_TILE_WEB
    else
        tiles.send_dump_info("morgue", fname);
#endif

    const game_exit exit_reason = _kill_method_to_exit(death_type);
#if defined(DGL_WHEREIS) || defined(USE_TILE_WEB)
    const string reason = _exit_type_to_string(exit_reason);

# ifdef DGL_WHEREIS
    whereis_record(reason.c_str());
# endif
#else
    UNUSED(_exit_type_to_string);
#endif

    if (!crawl_state.seen_hups)
        more();

    if (!crawl_state.disables[DIS_CONFIRMATIONS])
        display_inventory();
    textcolour(LIGHTGREY);

    clua.save_persist();

    // Prompt for saving macros.
    if (crawl_state.unsaved_macros && yesno("Save macros?", true, 'n'))
        macro_save();

#ifdef USE_TILE_WEB
    tiles_crt_popup show_as_popup;
#endif

    string goodbye_msg;
    goodbye_msg += make_stringf("Goodbye, %s.", you.your_name.c_str());
    goodbye_msg += "\n\n    "; // Space padding where # would go in list format

    string hiscore = hiscores_format_single_long(se, true);

    const int lines = count_occurrences(hiscore, "\n") + 1;

    goodbye_msg += hiscore;

    goodbye_msg += make_stringf("\nBest Crawlers - %s\n",
            crawl_state.game_type_name().c_str());

    // "- 5" gives us an extra line in case the description wraps on a line.
    goodbye_msg += hiscores_print_list(24 - lines - 5, SCORE_TERSE,
                        hiscore_index);

#ifndef DGAMELAUNCH
    goodbye_msg += make_stringf("\nYou can find your morgue file in the '%s' directory.",
            morgue_directory().c_str());
#endif

    auto prompt_ui = make_shared<Text>(formatted_string::parse_string(goodbye_msg));
    bool done = false;
    prompt_ui->on(Widget::slots.event, [&](wm_event ev)  {
        return done = ev.type == WME_KEYDOWN;
    });

    mouse_control mc(MOUSE_MODE_MORE);
    auto popup = make_shared<ui::Popup>(prompt_ui);

    if (!crawl_state.seen_hups && !crawl_state.disables[DIS_CONFIRMATIONS])
        ui::run_layout(move(popup), done);

#ifdef USE_TILE_WEB
    tiles.send_exit_reason(reason, hiscore);
#endif

    game_ended(exit_reason);
}

NORETURN void game_ended(game_exit exit, const string &message)
{
    if (crawl_state.marked_as_won &&
        (exit == game_exit::death || exit == game_exit::leave))
    {
        // used in tutorials
        exit = game_exit::win;
    }
    if (crawl_state.seen_hups ||
        (exit == game_exit::crash && !crawl_should_restart(game_exit::crash)))
    {
        const int retval = exit == game_exit::crash ? 1 : 0;
        if (message.size() > 0)
        {
#ifdef USE_TILE_WEB
            tiles.send_exit_reason("error", message);
#endif
            end(retval, false, "%s\n", message.c_str());
        }
        else
            end(retval);
    }
    throw game_ended_condition(exit, message);
}

// note: this *will not* print a crash dump, and so should probably be avoided.
NORETURN void game_ended_with_error(const string &message)
{
    game_ended(game_exit::crash, message);
}
