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
#include "files.h"
#include "god-passive.h"
#include "ghost.h"
#include "hints.h"
#include "initfile.h"
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
#include "tiledef-feat.h"

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
    if (!ui::is_available() && !msgwin_errors_to_stderr())
        cio_init(); // this, however, should be fairly safe
#endif

    mprf(MSGCH_ERROR, "%s", error_msg.c_str());

    if (!ui::is_available() || msgwin_errors_to_stderr())
        return false;

    // do the linebreak here so webtiles has it, but it's needed below as well
    linebreak_string(error_msg, 79);
#ifdef USE_TILE_WEB
    tiles.send_exit_reason("error", error_msg);
#endif

    // this is a bit heavy to continue past here in the face of a real crash.
    if (crawl_state.seen_hups)
        return false;

#if (!defined(DGAMELAUNCH)) || defined(DGL_PAUSE_AFTER_ERROR)
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

class HiscoreScroller : public Scroller
{
public:
    virtual void _allocate_region();
    int scroll_target = 0;
};

void HiscoreScroller::_allocate_region()
{
    m_scroll = scroll_target - m_region.height/2;
    Scroller::_allocate_region();
}

NORETURN void end_game(scorefile_entry &se)
{
    //Update states
    crawl_state.need_save       = false;
    crawl_state.updating_scores = true;

    const kill_method_type death_type = (kill_method_type) se.get_death_type();

    const bool non_death = death_type == KILLED_BY_QUITTING
                        || death_type == KILLED_BY_WINNING
                        || death_type == KILLED_BY_LEAVING;

    int hiscore_index = -1;
#ifndef SCORE_WIZARD_CHARACTERS
    if (!you.wizard && !you.explore)
#endif
    {
        // Add this highscore to the score file.
        hiscore_index = hiscores_new_entry(se);
        logfile_new_entry(se);
    }
#ifndef SCORE_WIZARD_CHARACTERS
    else
        hiscores_read_to_memory();
#endif

    // Never generate bones files of wizard or tutorial characters -- bwr
    if (!non_death && !crawl_state.game_is_tutorial() && !you.wizard)
        save_ghosts(ghost_demon::find_ghosts());

    for (auto &item : you.inv)
        if (item.defined() && item_type_unknown(item))
            add_inscription(item, "unknown");

    identify_inventory();

    _delete_files();

    // death message
    if (!non_death)
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

#if TAG_MAJOR_VERSION == 34
        case GOD_PAKELLAS:
        {
            const string result = getSpeakString("Pakellas death");
            god_speaks(GOD_PAKELLAS, result.c_str());
            break;
        }
#endif

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

#ifndef DISABLE_STICKY_STARTUP_OPTIONS
    // TODO: update all sticky prefs based on the dead char? Right now this
    // would lose weapon choice, and random select, as far as I can tell.
    save_seed_pref();
#endif

    if (!crawl_state.seen_hups)
        more();

    if (!crawl_state.disables[DIS_CONFIRMATIONS])
        display_inventory();

    clua.save_persist();

    // Prompt for saving macros.
    if (crawl_state.unsaved_macros && yesno("Save macros?", true, 'n'))
        macro_save();

    auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
    tile_def death_tile(TILEG_ERROR, TEX_GUI);
    if (death_type == KILLED_BY_LEAVING || death_type == KILLED_BY_WINNING)
        death_tile = tile_def(TILE_DNGN_EXIT_DUNGEON, TEX_FEAT);
    else
        death_tile = tile_def(TILE_DNGN_GRAVESTONE+1, TEX_FEAT);

    auto tile = make_shared<Image>(death_tile);
    tile->set_margin_for_sdl(0, 10, 0, 0);
    title_hbox->add_child(move(tile));
#endif
    string goodbye_title = make_stringf("Goodbye, %s.", you.your_name.c_str());
    title_hbox->add_child(make_shared<Text>(goodbye_title));
    title_hbox->align_cross = Widget::CENTER;
    title_hbox->set_margin_for_sdl(0, 0, 20, 0);
    title_hbox->set_margin_for_crt(0, 0, 1, 0);

    auto vbox = make_shared<Box>(Box::VERT);
    vbox->add_child(move(title_hbox));

    string goodbye_msg;
    goodbye_msg += "    "; // Space padding where # would go in list format

    string hiscore = hiscores_format_single_long(se, true);

    goodbye_msg += hiscore;

    goodbye_msg += make_stringf("\nBest Crawlers - %s\n",
            crawl_state.game_type_name().c_str());

#ifdef USE_TILE_LOCAL
        const int line_height = tiles.get_crt_font()->char_height();
#else
        const int line_height = 1;
#endif

    int start;
    int num_lines = 100;
    string hiscores = hiscores_print_list(num_lines, SCORE_TERSE, hiscore_index, start);
    auto scroller = make_shared<HiscoreScroller>();
    auto hiscores_txt = make_shared<Text>(formatted_string::parse_string(hiscores));
    scroller->set_child(hiscores_txt);
    scroller->set_scrollbar_visible(false);
    scroller->scroll_target = (hiscore_index - start)*line_height + (line_height/2);

    mouse_control mc(MOUSE_MODE_MORE);

    auto goodbye_txt = make_shared<Text>(formatted_string::parse_string(goodbye_msg));
    vbox->add_child(goodbye_txt);
    vbox->add_child(scroller);

#ifndef DGAMELAUNCH
    string morgue_dir = make_stringf("\nYou can find your morgue file in the '%s' directory.",
            morgue_directory().c_str());
    vbox->add_child(make_shared<Text>(formatted_string::parse_string(morgue_dir)));
#endif

    auto popup = make_shared<ui::Popup>(move(vbox));
    bool done = false;
    popup->on(Widget::slots.event, [&](wm_event ev)  {
        return done = ev.type == WME_KEYDOWN;
    });

    if (!crawl_state.seen_hups && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_open_object("tile");
    tiles.json_write_int("t", death_tile.tile);
    tiles.json_write_int("tex", death_tile.tex);
    tiles.json_close_object();
    tiles.json_write_string("title", goodbye_title);
    tiles.json_write_string("body", goodbye_msg
            + hiscores_print_list(11, SCORE_TERSE, hiscore_index, start));
    tiles.push_ui_layout("game-over", 0);
#endif

        ui::run_layout(move(popup), done);

#ifdef USE_TILE_WEB
    tiles.pop_ui_layout();
#endif
    }

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
#ifdef USE_TILE_WEB
    else if (exit == game_exit::abort)
        tiles.send_exit_reason("cancel", message);
#endif
    throw game_ended_condition(exit, message);
}

// note: this *will not* print a crash dump, and so should probably be avoided.
NORETURN void game_ended_with_error(const string &message)
{
    game_ended(game_exit::crash, message);
}
