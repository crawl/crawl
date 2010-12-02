/*
 *  File:       stuff.cc
 *  Summary:    Misc stuff.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "stuff.h"

#include "areas.h"
#include "beam.h"
#include "cio.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "libutil.h"
#include "los.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-place.h"
#include "terrain.h"
#include "state.h"
#include "travel.h"
#include "view.h"
#include "viewchar.h"
#include "viewgeom.h"

#include <cstdarg>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <stack>

#ifdef UNIX
 #ifndef USE_TILE
 #include "libunix.h"
 #endif
#endif

#include "branch.h"
#include "delay.h"
#include "externs.h"
#include "options.h"
#include "items.h"
#include "macro.h"
#include "misc.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "religion.h"
#include "hints.h"
#include "view.h"

stack_iterator::stack_iterator(const coord_def& pos, bool accessible)
{
    cur_link = accessible ? you.visible_igrd(pos) : igrd(pos);
    if (cur_link != NON_ITEM)
        next_link = mitm[cur_link].link;
    else
        next_link = NON_ITEM;
}

stack_iterator::stack_iterator(int start_link)
{
    cur_link = start_link;
    if (cur_link != NON_ITEM)
        next_link = mitm[cur_link].link;
    else
        next_link = NON_ITEM;
}

stack_iterator::operator bool() const
{
    return (cur_link != NON_ITEM);
}

item_def& stack_iterator::operator*() const
{
    ASSERT(cur_link != NON_ITEM);
    return mitm[cur_link];
}

item_def* stack_iterator::operator->() const
{
    ASSERT(cur_link != NON_ITEM);
    return &mitm[cur_link];
}

int stack_iterator::link() const
{
    return cur_link;
}

const stack_iterator& stack_iterator::operator ++ ()
{
    cur_link = next_link;
    if (cur_link != NON_ITEM)
        next_link = mitm[cur_link].link;
    return *this;
}

stack_iterator stack_iterator::operator++(int dummy)
{
    const stack_iterator copy = *this;
    ++(*this);
    return copy;
}

// Crude, but functional.
std::string make_time_string(time_t abs_time, bool terse)
{
    const int days  = abs_time / 86400;
    const int hours = (abs_time % 86400) / 3600;
    const int mins  = (abs_time % 3600) / 60;
    const int secs  = abs_time % 60;

    std::ostringstream buff;
    buff << std::setfill('0');

    if (days > 0)
    {
        if (terse)
            buff << days << ", ";
        else
            buff << days << (days > 1 ? " days" : "day");
    }

    buff << std::setw(2) << hours << ':'
         << std::setw(2) << mins << ':'
         << std::setw(2) << secs;
    return buff.str();
}

std::string make_file_time(time_t when)
{
    if (tm *loc = TIME_FN(&when))
    {
        return make_stringf("%04d%02d%02d-%02d%02d%02d",
                 loc->tm_year + 1900,
                 loc->tm_mon + 1,
                 loc->tm_mday,
                 loc->tm_hour,
                 loc->tm_min,
                 loc->tm_sec);
    }
    return ("");
}

void set_redraw_status(uint64_t flags)
{
    you.redraw_status_flags |= flags;
}

static bool _is_religious_follower(const monster* mon)
{
    return ((you.religion == GOD_YREDELEMNUL || you.religion == GOD_BEOGH)
            && is_follower(mon));
}

static bool _tag_follower_at(const coord_def &pos, bool &real_follower)
{
    if (!in_bounds(pos) || pos == you.pos())
        return (false);

    monster* fmenv = monster_at(pos);
    if (fmenv == NULL)
        return (false);

    if (!fmenv->alive()
        || fmenv->speed_increment < 50
        || fmenv->incapacitated()
        || mons_is_stationary(fmenv))
    {
        return (false);
    }

    if (!monster_habitable_grid(fmenv, DNGN_FLOOR))
        return (false);

    // Only non-wandering friendly monsters or those actively
    // seeking the player will follow up/down stairs.
    if (!fmenv->friendly()
          && (!mons_is_seeking(fmenv) || fmenv->foe != MHITYOU)
        || fmenv->foe == MHITNOT)
    {
        return (false);
    }

    // Monsters that are not directly adjacent are subject to more
    // stringent checks.
    if ((pos - you.pos()).abs() > 2)
    {
        if (!fmenv->friendly())
            return (false);

        // Undead will follow Yredelemnul worshippers, and orcs will
        // follow Beogh worshippers.
        if (!_is_religious_follower(fmenv))
            return (false);
    }

    // Monsters that can't use stairs can still be marked as followers
    // (though they'll be ignored for transit), so any adjacent real
    // follower can follow through. (jpeg)
    if (!mons_can_use_stairs(fmenv))
    {
        if (_is_religious_follower(fmenv))
        {
            fmenv->flags |= MF_TAKING_STAIRS;
            return (true);
        }
        return (false);
    }

    real_follower = true;

    // Monster is chasing player through stairs.
    fmenv->flags |= MF_TAKING_STAIRS;

    // Clear patrolling/travel markers.
    fmenv->patrol_point.reset();
    fmenv->travel_path.clear();
    fmenv->travel_target = MTRAV_NONE;

    dprf("%s is marked for following.",
         fmenv->name(DESC_CAP_THE, true).c_str());

    return (true);
}

static int follower_tag_radius2()
{
    // If only friendlies are adjacent, we set a max radius of 6, otherwise
    // only adjacent friendlies may follow.
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (const monster* mon = monster_at(*ai))
            if (!mon->friendly())
                return (2);
    }

    return (6 * 6);
}

void tag_followers()
{
    const int radius2 = follower_tag_radius2();
    int n_followers = 18;

    std::vector<coord_def> places[2];
    int place_set = 0;

    places[place_set].push_back(you.pos());
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    while (!places[place_set].empty())
    {
        for (int i = 0, size = places[place_set].size(); i < size; ++i)
        {
            const coord_def &p = places[place_set][i];
            for (adjacent_iterator ai(p); ai; ++ai)
            {
                if ((*ai - you.pos()).abs() > radius2
                    || travel_point_distance[ai->x][ai->y])
                {
                    continue;
                }
                travel_point_distance[ai->x][ai->y] = 1;

                bool real_follower = false;
                if (_tag_follower_at(*ai, real_follower))
                {
                    // If we've run out of our follower allowance, bail.
                    if (real_follower && --n_followers <= 0)
                        return;
                    places[!place_set].push_back(*ai);
                }
            }
        }
        places[place_set].clear();
        place_set = !place_set;
    }
}

void untag_followers()
{
    for (int m = 0; m < MAX_MONSTERS; ++m)
        menv[m].flags &= (~MF_TAKING_STAIRS);
}

unsigned char get_ch()
{
    mouse_control mc(MOUSE_MODE_MORE);
    unsigned char gotched = getchm();

    if (gotched == 0)
        gotched = getchm();

    return gotched;
}

void cio_init()
{
    crawl_state.io_inited = true;

#if defined(UNIX) && !defined(USE_TILE)
    unixcurses_startup();
#endif

#if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE)
    init_libw32c();
#endif

#ifdef TARGET_OS_DOS
    init_libdos();
#endif

    set_cursor_enabled(false);

    crawl_view.init_geometry();

#ifdef USE_TILE
    tiles.resize();
#endif
}

void cio_cleanup()
{
    if (!crawl_state.io_inited)
        return;

#if defined(USE_TILE)
    tiles.shutdown();
#elif defined(UNIX)
    unixcurses_shutdown();
#endif

#if defined(TARGET_OS_WINDOWS) && !defined(USE_TILE)
    deinit_libw32c();
#endif

    msg::deinitialise_mpr_streams();
    clear_globals_on_exit();
    crawl_state.io_inited = false;
}

// Clear some globally defined variables.
void clear_globals_on_exit()
{
    clear_rays_on_exit();
    clear_zap_info_on_exit();
    clear_colours_on_exit();
    dgn_clear_vault_placements(env.level_vaults);
}

// Used by do_crash_dump() to tell if the crash happened during exit() hooks.
// Not a part of crawl_state, since that's a global C++ instance which is
// free'd by exit() hooks when exit() is called, and we don't want to reference
// free'd memory.
bool CrawlIsExiting = false;
bool CrawlIsCrashing = false;

void end(int exit_code, bool print_error, const char *format, ...)
{
    std::string error = print_error? strerror(errno) : "";

    cio_cleanup();
    databaseSystemShutdown();

    if (format)
    {
        va_list arg;
        va_start(arg, format);
        char buffer[1024];
        vsnprintf(buffer, sizeof buffer, format, arg);
        va_end(arg);

        if (error.empty())
            error = std::string(buffer);
        else
            error = std::string(buffer) + ": " + error;
    }

    if (!error.empty())
    {
        if (error[error.length() - 1] != '\n')
            error += "\n";

        fprintf(stderr, "%s", error.c_str());
        error.clear();
    }

#if (defined(TARGET_OS_WINDOWS) && !defined(USE_TILE)) \
     || defined(TARGET_OS_DOS) \
     || defined(DGL_PAUSE_AFTER_ERROR)
    if (exit_code && !crawl_state.game_is_arena()
        && !crawl_state.seen_hups && !crawl_state.test)
    {
        fprintf(stderr, "Hit Enter to continue...\n");
        getchar();
    }
#endif

    CrawlIsExiting = true;
    if (exit_code)
        CrawlIsCrashing = true;
    exit(exit_code);
}

void game_ended()
{
    if (!crawl_state.seen_hups)
        throw game_ended_condition();
    else
        end(0);
}

void game_ended_with_error(const std::string &message)
{
    if (crawl_state.seen_hups)
        end(1);

    if (Options.restart_after_game)
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
        game_ended();
    }
    else
    {
        end(1, false, "%s", message.c_str());
    }
}

// Print error message on the screen.
// Ugly, but better than not showing anything at all. (jpeg)
void print_error_screen(const char *message, ...)
{
    // Get complete error message.
    std::string error_msg;
    {
        va_list arg;
        va_start(arg, message);
        char buffer[1024];
        vsnprintf(buffer, sizeof buffer, message, arg);
        va_end(arg);

        error_msg = std::string(buffer);
    }
    if (error_msg.empty())
        return;

    // Escape '<'.
    // NOTE: This assumes that the error message doesn't contain
    //       any formatting!
    error_msg = replace_all(error_msg, "<", "<<");

    error_msg += "\n\n\nHit any key to exit...\n";

    // Break message into correctly sized lines.
    int width = 80;
#ifdef USE_TILE
    width = crawl_view.msgsz.x;
#else
    width = std::min(80, get_number_of_cols());
#endif
    linebreak_string2(error_msg, width);

    // And finally output the message.
    clrscr();
    formatted_string::parse_string(error_msg, false).display();
    getchm();
}

void redraw_screen(void)
{
    if (!crawl_state.need_save)
    {
        // If the game hasn't started, don't do much.
        clrscr();
        return;
    }

    draw_border();

    you.redraw_hit_points   = true;
    you.redraw_magic_points = true;
    you.redraw_stats.init(true);
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_experience   = true;
    you.wield_change        = true;
    you.redraw_quiver       = true;

    set_redraw_status(
        REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK);

    print_stats();

    bool note_status = notes_are_active();
    activate_notes(false);
    print_stats_level();
#ifdef DGL_SIMPLE_MESSAGING
    update_message_status();
#endif
    update_turn_count();
    activate_notes(note_status);

    viewwindow();

    // Display the message window at the end because it places
    // the cursor behind possible prompts.
    display_message_window();
}

// STEPDOWN FUNCTION to replace conditional chains in spells2.cc 12jan2000 {dlb}
// it is a bit more extensible and optimises the logical structure, as well
// usage: cast_summon_swarm() cast_haunt() cast_summon_scorpions()
//        cast_summon_horrible_things()
// ex(1): stepdown_value (foo, 2, 2, 6, 8) replaces the following block:
//

/*
   if (foo > 2)
     foo = (foo - 2) / 2 + 2;
   if (foo > 4)
     foo = (foo - 4) / 2 + 4;
   if (foo > 6)
     foo = (foo - 6) / 2 + 6;
   if (foo > 8)
     foo = 8;
 */

//
// ex(2): bar = stepdown_value(bar, 2, 2, 6, -1) replaces the following block:
//

/*
   if (bar > 2)
     bar = (bar - 2) / 2 + 2;
   if (bar > 4)
     bar = (bar - 4) / 2 + 4;
   if (bar > 6)
     bar = (bar - 6) / 2 + 6;
 */

// I hope this permits easier/more experimentation with value stepdowns
// in the code.  It really needs to be rewritten to accept arbitrary
// (unevenly spaced) steppings.
int stepdown_value(int base_value, int stepping, int first_step,
                   int last_step, int ceiling_value)
{
    int return_value = base_value;

    // values up to the first "step" returned unchanged:
    if (return_value <= first_step)
        return return_value;

    for (int this_step = first_step; this_step <= last_step;
         this_step += stepping)
    {
        if (return_value > this_step)
            return_value = ((return_value - this_step) / 2) + this_step;
        else
            break;              // exit loop iff value fully "stepped down"
    }

    // "no final ceiling" == -1
    if (ceiling_value != -1 && return_value > ceiling_value)
        return ceiling_value;   // highest value to return is "ceiling"
    else
        return return_value;    // otherwise, value returned "as is"

}

int skill_bump(int skill)
{
    return ((you.skills[skill] < 3) ? you.skills[skill] * 2
                                    : you.skills[skill] + 3);
}

// This gives (default div = 20, shift = 3):
// - shift/div% @ stat_level = 0; (default 3/20 = 15%, or 20% at stat 1)
// - even (100%) @ stat_level = div - shift; (default 17)
// - 1/div% per stat_level (default 1/20 = 5%)
int stat_mult(int stat_level, int value, int div, int shift)
{
    return (((stat_level + shift) * value) / ((div > 1) ? div : 1));
}

int div_round_up(int num, int den)
{
    return (num / den + (num % den != 0));
}

void canned_msg(canned_message_type which_message)
{
    switch (which_message)
    {
    case MSG_SOMETHING_APPEARS:
        mprf("Something appears %s!",
             (you.species == SP_NAGA || player_mutation_level(MUT_HOOVES))
                 ? "before you" : "at your feet");
        break;
    case MSG_NOTHING_HAPPENS:
        mpr("Nothing appears to happen.");
        break;
    case MSG_YOU_UNAFFECTED:
        mpr("You are unaffected.");
        break;
    case MSG_YOU_RESIST:
        mpr("You resist.");
        learned_something_new(HINT_YOU_RESIST);
        break;
    case MSG_YOU_PARTIALLY_RESIST:
        mpr("You partially resist.");
        break;
    case MSG_TOO_BERSERK:
        mpr("You are too berserk!");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_PRESENT_FORM:
        mpr("You can't do that in your present form.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_NOTHING_CARRIED:
        mpr("You aren't carrying anything.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_CANNOT_DO_YET:
        mpr("You can't do that yet.");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_OK:
        mpr("Okay, then.", MSGCH_PROMPT);
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_UNTHINKING_ACT:
        mpr("Why would you want to do that?");
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_SPELL_FIZZLES:
        mpr("The spell fizzles.");
        break;
    case MSG_HUH:
        mpr("Huh?", MSGCH_EXAMINE_FILTER);
        crawl_state.cancel_cmd_repeat();
        break;
    case MSG_EMPTY_HANDED:
        if (you.species == SP_CAT)
            mpr("Your mouth is now empty.");
        else
            mpr("You are now empty-handed.");
        break;
    case MSG_YOU_BLINK:
        mpr("You blink.");
        break;
    case MSG_STRANGE_STASIS:
        mpr("You feel a strange sense of stasis.");
        break;
    case MSG_NO_SPELLS:
        mpr("You don't know any spells.");
        break;
    case MSG_MANA_INCREASE:
        mpr("You feel your mana capacity increase.");
        break;
    case MSG_MANA_DECREASE:
        mpr("You feel your mana capacity decrease.");
        break;
    case MSG_TOO_HUNGRY:
        mpr("You're too hungry.");
        break;
    }
}

// Like yesno, but requires a full typed answer.
// Unlike yesno, prompt should have no trailing space.
// Returns true if the user typed "yes", false if something else or cancel.
bool yes_or_no(const char* fmt, ...)
{
    char buf[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    buf[sizeof(buf)-1] = 0;

    mprf(MSGCH_PROMPT, "%s? (Confirm with \"yes\".) ", buf);

    if (cancelable_get_line(buf, sizeof buf))
        return (false);
    if (strcasecmp(buf, "yes") != 0)
        return (false);

    return (true);
}

// jmf: general helper (should be used all over in code)
//      -- idea borrowed from Nethack
bool yesno(const char *str, bool safe, int safeanswer, bool clear_after,
           bool interrupt_delays, bool noprompt,
           const explicit_keymap *map, GotoRegion region)
{
    bool message = (region == GOTO_MSG);
    if (interrupt_delays && !crawl_state.is_repeating_cmd())
        interrupt_activity(AI_FORCE_INTERRUPT);

    std::string prompt = make_stringf("%s ", str ? str : "Buggy prompt?");

    mouse_control mc(MOUSE_MODE_MORE);
    while (true)
    {
        if (!noprompt)
        {
            if (message)
                mpr(prompt.c_str(), MSGCH_PROMPT);
            else
                cprintf("%s", prompt.c_str());
        }

        int tmp = getchm(KMC_CONFIRM);

#if defined(USE_UNIX_SIGNALS) && defined(SIGHUP_SAVE) && defined(USE_CURSES)
        // Prevent infinite loop if Curses HUP signal handling happens;
        // if there is no safe answer, then just save-and-exit immediately,
        // since there's no way to know if it would be better to return
        // true or false.
        if (crawl_state.seen_hups && !safeanswer)
            sighup_save_and_exit();
#endif

        if (map && map->find(tmp) != map->end())
            tmp = map->find(tmp)->second;

        if (safeanswer
            && (tmp == ' ' || key_is_escape(tmp)
                || tmp == '\r' || tmp == '\n' || crawl_state.seen_hups))
        {
            tmp = safeanswer;
        }

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || Options.easy_confirm == CONFIRM_SAFE_EASY && safe)
        {
            tmp = toupper(tmp);
        }

        if (clear_after && message)
            mesclr();

        if (tmp == 'N')
            return (false);
        else if (tmp == 'Y')
            return (true);
        else if (!noprompt)
        {
            const std::string pr = "[Y]es or [N]o only, please.";
            if (message)
                mpr(pr);
            else
                cprintf(("\n" + pr + "\n").c_str());
        }
    }
}

static std::string _list_alternative_yes(char yes1, char yes2,
                                         bool lowered = false,
                                         bool brackets = false)
{
    std::string help = "";
    bool print_yes = false;
    if (yes1 != 'Y')
    {
        if (lowered)
            help += tolower(yes1);
        else
            help += yes1;
        print_yes = true;
    }

    if (yes2 != 'Y' && yes2 != yes1)
    {
        if (print_yes)
            help += "/";

        if (lowered)
            help += tolower(yes2);
        else
            help += yes2;
        print_yes = true;
    }

    if (print_yes)
    {
        if (brackets)
            help = " (" + help + ")";
        else
            help = "/" + help;
    }

    return help;
}

static std::string _list_allowed_keys(char yes1, char yes2,
                                      bool lowered = false,
                                      bool allow_all = false)
{
    std::string result = " [";
                result += (lowered ? "y" : "Y");
                result += _list_alternative_yes(yes1, yes2, lowered);
                if (allow_all)
                    result += (lowered? "/a" : "/A");
                result += (lowered ? "/n/q" : "/N/Q");
                result += "]";

    return (result);
}

// Like yesno(), but returns 0 for no, 1 for yes, and -1 for quit.
// alt_yes and alt_yes2 allow up to two synonyms for 'Y'.
// FIXME: This function is shaping up to be a monster. Help!
int yesnoquit(const char* str, bool safe, int safeanswer, bool allow_all,
               bool clear_after, char alt_yes, char alt_yes2)
{
    if (!crawl_state.is_repeating_cmd())
        interrupt_activity(AI_FORCE_INTERRUPT);

    mouse_control mc(MOUSE_MODE_MORE);

    std::string prompt =
        make_stringf("%s%s ", str ? str : "Buggy prompt?",
                     _list_allowed_keys(alt_yes, alt_yes2,
                                        safe, allow_all).c_str());
    while (true)
    {
        mpr(prompt.c_str(), MSGCH_PROMPT);

        int tmp = getchm(KMC_CONFIRM);

        if (key_is_escape(tmp) || tmp == 'q' || tmp == 'Q'
            || crawl_state.seen_hups)
        {
            return -1;
        }

        if ((tmp == ' ' || tmp == '\r' || tmp == '\n') && safeanswer)
            tmp = safeanswer;

        if (Options.easy_confirm == CONFIRM_ALL_EASY
            || tmp == safeanswer
            || safe && Options.easy_confirm == CONFIRM_SAFE_EASY)
        {
            tmp = toupper(tmp);
        }

        if (clear_after)
            mesclr();

        if (tmp == 'N')
            return 0;
        else if (tmp == 'Y' || tmp == alt_yes || tmp == alt_yes2)
            return 1;
        else if (allow_all)
        {
            if (tmp == 'A')
                return 2;
            else
                mprf("Choose [Y]es%s, [N]o, [Q]uit, or [A]ll!",
                     _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
        }
        else
        {
            mprf("[Y]es%s, [N]o or [Q]uit only, please.",
                 _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
        }
    }
}

bool player_can_hear(const coord_def& p, int hear_distance)
{
    return (!silenced(p)
            && !silenced(you.pos())
            && you.pos().distance_from(p) <= hear_distance);
}

char index_to_letter(int the_index)
{
    return (the_index + ((the_index < 26) ? 'a' : ('A' - 26)));
}

int letter_to_index(int the_letter)
{
    if (the_letter >= 'a' && the_letter <= 'z')
        // returns range [0-25] {dlb}
        the_letter -= 'a';
    else if (the_letter >= 'A' && the_letter <= 'Z')
        // returns range [26-51] {dlb}
        the_letter -= ('A' - 26);

    return (the_letter);
}

// Returns 0 if the point is not near stairs.
// Returns 1 if the point is near unoccupied stairs.
// Returns 2 if the point is near player-occupied stairs.
int near_stairs(const coord_def &p, int max_dist,
                dungeon_char_type &stair_type, branch_type &branch)
{
    for (radius_iterator ri(p, max_dist, true, false); ri; ++ri)
    {
        const dungeon_feature_type feat = grd(*ri);
        if (feat_is_stair(feat))
        {
            // Shouldn't happen for escape hatches.
            if (feat_is_escape_hatch(feat))
                continue;

            stair_type = get_feature_dchar(feat);

            // Is it a branch stair?
            for (int i = 0; i < NUM_BRANCHES; ++i)
            {
                if (branches[i].entry_stairs == feat)
                {
                    branch = branches[i].id;
                    break;
                }
                else if (branches[i].exit_stairs == feat)
                {
                    branch = branches[i].parent_branch;
                    break;
                }
            }
            return (*ri == you.pos()) ? 2 : 1;
        }
    }

    return (false);
}

// Does the equivalent of KILL_RESET on all monsters in LOS. Should only be
// applied to new games.
void zap_los_monsters(bool items_also)
{
    // Not using player LOS since clouds might temporarily
    // block monsters.
    los_def los(you.pos(), opc_fullyopaque);
    los.update();

    for (radius_iterator ri(&los); ri; ++ri)
    {
        if (items_also)
        {
            int item = igrd(*ri);

            if (item != NON_ITEM && mitm[item].defined())
                destroy_item(item);
        }

        // If we ever allow starting with a friendly monster,
        // we'll have to check here.
        monster* mon = monster_at(*ri);
        if (mon == NULL || mons_class_flag(mon->type, M_NO_EXP_GAIN))
            continue;

        dprf("Dismissing %s",
             mon->name(DESC_PLAIN, true).c_str());

        // Do a hard reset so the monster's items will be discarded.
        mon->flags |= MF_HARD_RESET;
        // Do a silent, wizard-mode monster_die() just to be extra sure the
        // player sees nothing.
        monster_die(mon, KILL_DISMISSED, NON_MONSTER, true, true);
    }
}

int random_rod_subtype()
{
    return STAFF_FIRST_ROD + random2(NUM_STAVES - STAFF_FIRST_ROD);
}

maybe_bool frombool(bool b)
{
    return (b ? B_TRUE : B_FALSE);
}

bool tobool(maybe_bool mb)
{
    ASSERT (mb != B_MAYBE);
    return (mb == B_TRUE);
}

bool tobool(maybe_bool mb, bool def)
{
    switch (mb)
    {
    case B_TRUE:
        return (true);
    case B_FALSE:
        return (false);
    case B_MAYBE:
    default:
        return (def);
    }
}

coord_def get_random_stair()
{
    std::vector<coord_def> st;
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const dungeon_feature_type feat = grd(*ri);
        if (feat_is_travelable_stair(feat) && !feat_is_escape_hatch(feat))
        {
            st.push_back(*ri);
        }
    }
    if (!st.size())
        return coord_def();        // sanity check: shouldn't happen
    return st[random2(st.size())];
}
