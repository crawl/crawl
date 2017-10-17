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
#include "state.h"
#include "stringutil.h"
#include "view.h"
#include "xom.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

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

#if (defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)) \
|| defined(DGL_PAUSE_AFTER_ERROR)
// Print error message on the screen.
// Ugly, but better than not showing anything at all. (jpeg)
static bool _print_error_screen(const char *message, ...)
{
    if (!crawl_state.io_inited || crawl_state.seen_hups)
        return false;

    // Get complete error message.
    string error_msg;
    {
        va_list arg;
        va_start(arg, message);
        char buffer[1024];
        vsnprintf(buffer, sizeof buffer, message, arg);
        va_end(arg);

        error_msg = string(buffer);
    }
    if (error_msg.empty())
        return false;

    // Escape '<'.
    // NOTE: This assumes that the error message doesn't contain
    //       any formatting!
    error_msg = replace_all(error_msg, "<", "<<");

    error_msg += "\n\n\nHit any key to exit...\n";

    // Break message into correctly sized lines.
    int width = 80;
#ifdef USE_TILE_LOCAL
    width = crawl_view.msgsz.x;
#else
    width = min(80, get_number_of_cols());
#endif
    linebreak_string(error_msg, width);

    // And finally output the message.
    clrscr();
    formatted_string::parse_string(error_msg).display();
    getchm();
    return true;
}
#endif

// Used by do_crash_dump() to tell if the crash happened during exit() hooks.
// Not a part of crawl_state, since that's a global C++ instance which is
// free'd by exit() hooks when exit() is called, and we don't want to reference
// free'd memory.
bool CrawlIsExiting = false;
bool CrawlIsCrashing = false;

NORETURN void end(int exit_code, bool print_error, const char *format, ...)
{
    bool need_pause = true;
    disable_other_crashes();

    // Let "error" go out of scope for valgrind's sake.
    {
        string error = print_error? strerror(errno) : "";
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

#if (defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)) \
|| defined(DGL_PAUSE_AFTER_ERROR)
        if (exit_code && !error.empty())
        {
            if (_print_error_screen("%s", error.c_str()))
                need_pause = false;
        }
#endif

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
            fprintf(stderr, "%s", error.c_str());
            error.clear();
        }
    }

#if (defined(TARGET_OS_WINDOWS) && !defined(USE_TILE_LOCAL)) \
|| defined(DGL_PAUSE_AFTER_ERROR)
    if (need_pause && exit_code && !crawl_state.game_is_arena()
        && !crawl_state.seen_hups && !crawl_state.test)
    {
        fprintf(stderr, "Hit Enter to continue...\n");
        getchar();
    }
#else
    UNUSED(need_pause);
#endif

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
        clrscr();
        linebreak_string(text, get_number_of_cols());
        display_tagged_block(text);

        if (!crawl_state.seen_hups)
            get_ch();
    }

    game_ended(game_exit::abort); // TODO: is this the right exit condition?
}

game_exit kill_method_to_exit(kill_method_type kill)
{
    switch (kill)
    {
        case KILLED_BY_QUITTING: return game_exit::quit;
        case KILLED_BY_WINNING:  return game_exit::win;
        case KILLED_BY_LEAVING:  return game_exit::leave;
        default:                 return game_exit::death;
    }
}

string exit_type_to_string(game_exit e)
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
    {
        mpr("Char dump unsuccessful! Sorry about that.");
        if (!crawl_state.seen_hups)
            more();
        clrscr();
    }
#ifdef USE_TILE_WEB
    else
        tiles.send_dump_info("morgue", fname);
#endif

#if defined(DGL_WHEREIS) || defined(USE_TILE_WEB)
    string reason = exit_type_to_string(kill_method_to_exit(death_type));

#ifdef DGL_WHEREIS
    whereis_record(reason.c_str());
#endif
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

    clrscr();
    cprintf("Goodbye, %s.", you.your_name.c_str());
    cprintf("\n\n    "); // Space padding where # would go in list format

    string hiscore = hiscores_format_single_long(se, true);

    const int lines = count_occurrences(hiscore, "\n") + 1;

    cprintf("%s", hiscore.c_str());

    cprintf("\nBest Crawlers - %s\n",
            crawl_state.game_type_name().c_str());

    // "- 5" gives us an extra line in case the description wraps on a line.
    hiscores_print_list(get_number_of_lines() - lines - 5, SCORE_TERSE,
                        hiscore_index);

#ifndef DGAMELAUNCH
    cprintf("\nYou can find your morgue file in the '%s' directory.",
            morgue_directory().c_str());
#endif

    // just to pause, actual value returned does not matter {dlb}
    if (!crawl_state.seen_hups && !crawl_state.disables[DIS_CONFIRMATIONS])
        get_ch();

#ifdef USE_TILE_WEB
    tiles.send_exit_reason(reason, hiscore);
#endif

    game_ended(kill_method_to_exit(death_type));
}

NORETURN void game_ended(game_exit exit)
{
    if (crawl_state.marked_as_won &&
        (exit == game_exit::death || exit == game_exit::leave))
    {
        // used in tutorials
        exit = game_exit::win;
    }
    if (!crawl_state.seen_hups)
        throw game_ended_condition(exit);
    else
        end(0);
}

NORETURN void game_ended_with_error(const string &message)
{
    if (crawl_state.seen_hups)
        end(1);

#ifdef USE_TILE_WEB
    tiles.send_exit_reason("error", message);
#endif

    if (crawl_should_restart(game_exit::crash))
    {
        if (crawl_state.io_inited)
        {
            mprf(MSGCH_ERROR, "%s", message.c_str());
            more();
        }
        else
        {
            fprintf(stderr, "%s\nHit Enter to continue...\n", message.c_str());
            getchar();
        }
        game_ended(game_exit::crash);
    }
    else
        end(1, false, "%s", message.c_str());
}
