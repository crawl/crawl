/**
 * @file
 * @brief Prompts.
 **/

#include "AppHdr.h"
#include <cmath> // isnan

#include "prompt.h"

#include "clua.h"
#include "delay.h"
#include "libutil.h"
#include "menu.h"
#include "message.h"
#include "options.h"
#include "state.h"
#include "stringutil.h"
#ifdef USE_TILE
#include "rltiles/tiledef-gui.h"
#endif
#include "viewchar.h"
#include "ui.h"

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

    mprf(MSGCH_PROMPT, "%s (Confirm with \"yes\".) ", buf);

    if (cancellable_get_line(buf, sizeof buf))
        return false;
    if (strcasecmp(buf, "yes") != 0)
        return false;

    return true;
}

// jmf: general helper (should be used all over in code)
//      -- idea borrowed from Nethack
int yesno(const char *str, bool allow_lowercase, int default_answer, bool clear_after,
           bool interrupt_delays, bool noprompt,
           const explicit_keymap *map, bool allow_popup, bool ask_always)
{
    if (interrupt_delays && !crawl_state.is_repeating_cmd())
        interrupt_activity(activity_interrupt::force);

    // Allow players to answer prompts via clua.
    // XXX: always not currently supported
    maybe_bool res = clua.callmaybefn("c_answer_prompt", "s", str);
    if (res == MB_TRUE)
        return true;
    if (res == MB_FALSE)
        return false;

    string prompt = make_stringf("%s ", str ? str : "Buggy prompt?");

#ifdef TOUCH_UI
    bool use_popup = true;
#else
    bool use_popup = !crawl_state.need_save || ui::has_layout();
    use_popup = use_popup && str && allow_popup;
#endif

    int flags = MF_SINGLESELECT | MF_ANYPRINTABLE | MF_ALLOW_FORMATTING;
    if (allow_lowercase && use_popup)
        flags |= MF_ARROWS_SELECT;
    Menu pop(flags, "", KMC_CONFIRM);
    MenuEntry *status = nullptr;

    if (use_popup)
    {
        status = new MenuEntry("", MEL_SUBTITLE);
        MenuEntry * const y_me = new MenuEntry("Yes", MEL_ITEM, 1, 'Y');
        MenuEntry * const n_me = new MenuEntry("No", MEL_ITEM, 1, 'N');
        MenuEntry * const a_me = new MenuEntry("Always", MEL_ITEM, 1, 'A');
        y_me->add_tile(tile_def(TILEG_PROMPT_YES));
        n_me->add_tile(tile_def(TILEG_PROMPT_NO));

        MenuEntry *question = new MenuEntry(prompt, MEL_TITLE);
        question->wrap_text();
        pop.set_title(question);
        pop.add_entry(status);
        pop.add_entry(y_me);
        pop.add_entry(n_me);
        if (ask_always)
            pop.add_entry(a_me);
        if (allow_lowercase && default_answer == 'y')
            pop.set_hovered(1);
        else if (allow_lowercase && default_answer == 'n')
            pop.set_hovered(2);
        else if (ask_always && allow_lowercase && default_answer == 'a')
            pop.set_hovered(3);
        pop.on_single_selection = [&pop](const MenuEntry& item)
            {
                if (item.hotkeys.size())
                    return pop.process_key(item.hotkeys[0]);
                return false;
            };
    }
    mouse_control mc(MOUSE_MODE_YESNO);
    while (true)
    {
        int tmp = ESCAPE;
        if (!crawl_state.seen_hups)
        {
            if (use_popup)
            {
                pop.show();
                tmp = pop.getkey();
            }
            else
            {
                if (!noprompt)
                    mprf(MSGCH_PROMPT, "%s", prompt.c_str());

                tmp = ui::getch(KMC_CONFIRM);
            }
        }

        // If no safe answer exists, we still need to abort when a HUP happens.
        // The caller must handle this case, preferably by issuing an uncancel
        // event that can restart when the game restarts -- and ignore the
        // the return value here.
        if (crawl_state.seen_hups && !default_answer)
            return false;

        if (map && map->find(tmp) != map->end())
            tmp = map->find(tmp)->second;

        if (default_answer
            && (tmp == ' ' || key_is_escape(tmp)
                || tmp == '\r' || tmp == '\n' || crawl_state.seen_hups))
        {
            tmp = default_answer;
        }

        if (Options.easy_confirm == easy_confirm_type::all
            || tmp == default_answer
            || Options.easy_confirm == easy_confirm_type::safe
               && allow_lowercase)
        {
            tmp = toupper_safe(tmp);
        }

        if (clear_after)
            clear_messages();

        if (tmp == 'N')
            return 0;
        else if (tmp == 'Y')
            return 1;
        else if (ask_always && tmp == 'A')
            return 2;
        else if (!noprompt)
        {
            bool upper = !allow_lowercase
                         && (tmp == 'n' || tmp == 'y'
                             || (ask_always && tmp == 'a')
                             || crawl_state.game_is_hints_tutorial());
            const string pr = make_stringf("%s%s only, please.",
                                           upper ? "Uppercase " : "",
                                           ask_always ?
                                               "[Y]es, [N]o, or [A]lways" :
                                               "[Y]es or [N]o");
            if (use_popup && status) // redundant, but will quiet a warning
                status->text = pr;
            else
                mprf(MSGCH_PROMPT, "%s", pr.c_str());
        }
    }
}

/**
 * Prompt the user for a quantity of things.
 *
 * @param prompt the message to be used before the prompt.
 * @return -1 if <enter> or ';' are pressed (meaning all);
 *         0 if the user escaped;
 *         the number chosen otherwise.
 */
int prompt_for_quantity(const char *prompt)
{
    msgwin_prompt(prompt);

    int ch = getch_ck();
    if (ch == CK_ENTER || ch == ';')
        return -1;
    else if (ch == CK_ESCAPE || ch == CK_REDRAW)
        return 0;

    const string prefill = string(1, ch);
    return prompt_for_int("", false, prefill);
}

/**
 * Returns an integer, with a failure state.
 *
 * @param prompt the message to be used before the prompt.
 * @param nonneg if true, the failure sentinel is -1;
 *               if false, the sentinel is 0.
 & @param prefill a prefill to use for the message box, if any.
 * @return the chosen number, or the chosen sentinel value.
 */
int prompt_for_int(const char *prompt, bool nonneg, const string &prefill)
{
    char specs[80];

    int getline_ret = msgwin_get_line(prompt, specs, sizeof(specs), nullptr,
                                            prefill);

    if (specs[0] == '\0' || getline_ret == CK_ESCAPE)
        return nonneg ? -1 : 0;

    char *end;
    int   ret = strtol(specs, &end, 10);

    if (ret < 0 && nonneg || ret == 0 && end == specs)
        ret = (nonneg ? -1 : 0);

    return ret;
}

double prompt_for_float(const char* prompt)
{
    char specs[80];

    int getline_ret = msgwin_get_line(prompt, specs, sizeof(specs));

    if (specs[0] == '\0' || getline_ret == CK_ESCAPE)
        return -1;

    char *end;
    double ret = strtod(specs, &end);

    if (ret == 0 && end == specs || std::isnan(ret))
        ret = -1;

    return ret;
}


char index_to_letter(int the_index)
{
    ASSERT_RANGE(the_index, 0, ENDOFPACK);
    return the_index + ((the_index < 26) ? 'a' : ('A' - 26));
}

int letter_to_index(int the_letter)
{
    if (the_letter >= 'a' && the_letter <= 'z')
        return the_letter - 'a'; // returns range [0-25] {dlb}
    else if (the_letter >= 'A' && the_letter <= 'Z')
        return the_letter - 'A' + 26; // returns range [26-51] {dlb}

    die("slot not a letter: %s (%d)", the_letter ?
        stringize_glyph(the_letter).c_str() : "null", the_letter);
}
