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
#include "god-abil.h"
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
#include "tag-version.h"
#include "tilepick.h"
#include "view.h"
#include "xom.h"
#include "ui.h"
#include "rltiles/tiledef-feat.h"

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
    UNUSED(exit);
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
        Options.restart_after_game.to_bool(!crawl_state.bypassed_startup_menu);
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

// game is dying / crashing, potentially quite early in the startup process.
// do our best to provide this information to the user somehow.
bool fatal_error_notification(string error_msg)
{
    if (error_msg.empty())
        return false;

    // for local tiles, if there is no available ui, it's possible that wm
    // initialisation has failed and there's nothing that can be done, so we
    // don't try. On other builds, though, it's just probably early in the
    // initialisation process, and cio_init should be fairly safe.
#ifndef USE_TILE_LOCAL
    if (!ui::is_available() && !msg::uses_stderr(MSGCH_ERROR))
        cio_init();
#endif

    const bool ui_available =
#if (!defined(DGAMELAUNCH)) || defined(DGL_PAUSE_AFTER_ERROR)
        // possibly changed by the cio_init call
        ui::is_available() && !msg::uses_stderr(MSGCH_ERROR);
#else
        false;
#endif

    if (!ui_available || crawl_state.seen_hups)
    {
        // in these cases we don't want to attempt the ui popup for the error
        // message, just print the error to the log. In the non-crash case,
        // this will also echo to stderr.
        mprf(MSGCH_ERROR, "%s", error_msg.c_str());
        return false;
    }

    // do the linebreak here so webtiles has it, but it's needed below as well
    linebreak_string(error_msg, 79);
#ifdef USE_TILE_WEB
    tiles.send_exit_reason("error", error_msg);
#endif

    ui::error(error_msg, "Fatal error:", true);
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
// Non-static for catch2-tests.
void delete_files()
{
    crawl_state.need_save = false;
    you.save->unlink();
    delete you.save;
    you.save = 0;
}

NORETURN void screen_end_game(string text, game_exit exit)
{
#ifdef USE_TILE_WEB
    tiles.send_exit_reason("quit");
#endif
    crawl_state.cancel_cmd_all();
    delete_files();

    if (!text.empty())
        ui::message(text, "", "<cyan>Hit any key to exit...</cyan>", true);

    game_ended(exit); // TODO: is this the right exit condition?
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
        if (item.defined() && !item.is_identified())
            add_inscription(item, "unknown");

    identify_inventory();

    delete_files();

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

    update_whereis(reason.c_str());
#else
    UNUSED(_exit_type_to_string);
#endif

    save_game_prefs();

    if (!crawl_state.seen_hups)
        more();

    if (!crawl_state.disables[DIS_CONFIRMATIONS])
        display_inventory();

    clua.save_persist();

    if (crawl_state.unsaved_macros)
        macro_save();

    auto title_hbox = make_shared<Box>(Widget::HORZ);
#ifdef USE_TILE
    tile_def death_tile(TILEG_ERROR);
    if (death_type == KILLED_BY_LEAVING || death_type == KILLED_BY_WINNING)
        death_tile = tile_def(TILE_DNGN_EXIT_DUNGEON);
    else
        death_tile = tile_def(TILE_DNGN_GRAVESTONE+1);

    auto tile = make_shared<Image>(death_tile);
    tile->set_margin_for_sdl(0, 10, 0, 0);
    title_hbox->add_child(std::move(tile));
#endif
    string goodbye_title = make_stringf("Goodbye, %s.", you.your_name.c_str());
    title_hbox->add_child(make_shared<Text>(goodbye_title));
    title_hbox->set_cross_alignment(Widget::CENTER);
    title_hbox->set_margin_for_sdl(0, 0, 20, 0);
    title_hbox->set_margin_for_crt(0, 0, 1, 0);

    auto vbox = make_shared<Box>(Box::VERT);
    vbox->add_child(std::move(title_hbox));

    string goodbye_msg;
    goodbye_msg += "    "; // Space padding where # would go in list format

    string hiscore = hiscores_format_single_long(se, true);

    goodbye_msg += hiscore;

    goodbye_msg += make_stringf("\nBest Crawlers - %s\n",
            crawl_state.game_type_name().c_str());

#ifdef USE_TILE_LOCAL
        const int line_height = in_headless_mode() ? 1 : tiles.get_crt_font()->char_height();
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
# ifndef __ANDROID__
    string morgue_dir = make_stringf("\nYou can find your morgue file in the '%s' directory.",
# else
    string morgue_dir = make_stringf("\nYou can find your morgue file in the \n'%s'\n directory.",
# endif
            morgue_directory().c_str());
    vbox->add_child(make_shared<Text>(formatted_string::parse_string(morgue_dir)));
#endif

    auto popup = make_shared<ui::Popup>(std::move(vbox));
    bool done = false;
    popup->on_keydown_event([&](const KeyEvent&) { return done = true; });

    if (!crawl_state.seen_hups && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_open_object("tile");
    tiles.json_write_int("t", death_tile.tile);
    tiles.json_write_int("tex", get_tile_texture(death_tile.tile));
    tiles.json_close_object();
    tiles.json_write_string("title", goodbye_title);
    tiles.json_write_string("body", goodbye_msg
            + hiscores_print_list(11, SCORE_TERSE, hiscore_index, start));
    tiles.push_ui_layout("game-over", 0);
    popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

        ui::run_layout(std::move(popup), done);
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
            tiles.send_exit_reason(crawl_state.seen_hups
                                        ? "disconnect" : "error", message);
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
