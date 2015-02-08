/**
 * @file
 * @brief Prompts.
 **/

#include "AppHdr.h"

#include "prompt.h"

#include "delay.h"
#include "libutil.h"
#include "macro.h"
#ifdef TOUCH_UI
#include "menu.h"
#endif
#include "message.h"
#include "options.h"
#include "state.h"
#include "stringutil.h"
#ifdef TOUCH_UI
#include "tiledef-gui.h"
#endif
#include "viewchar.h"

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
bool yesno(const char *str, bool safe, int safeanswer, bool clear_after,
           bool interrupt_delays, bool noprompt,
           const explicit_keymap *map, GotoRegion region)
{
    bool message = (region == GOTO_MSG);
    if (interrupt_delays && !crawl_state.is_repeating_cmd())
        interrupt_activity(AI_FORCE_INTERRUPT);

    string prompt = make_stringf("%s ", str ? str : "Buggy prompt?");

#ifdef TOUCH_UI
    Popup *pop = new Popup(prompt);
    MenuEntry *status = new MenuEntry("", MEL_SUBTITLE);
    pop->push_entry(new MenuEntry(prompt, MEL_TITLE));
    pop->push_entry(status);
    MenuEntry *me = new MenuEntry("Yes", MEL_ITEM, 0, 'Y', false);
    me->add_tile(tile_def(TILEG_PROMPT_YES, TEX_GUI));
    pop->push_entry(me);
    me = new MenuEntry("No", MEL_ITEM, 0, 'N', false);
    me->add_tile(tile_def(TILEG_PROMPT_NO, TEX_GUI));
    pop->push_entry(me);
#endif
    mouse_control mc(MOUSE_MODE_YESNO);
    while (true)
    {
        int tmp = ESCAPE;
        if (!crawl_state.seen_hups)
        {
#ifdef TOUCH_UI
            tmp = pop->pop();
#else
            if (!noprompt)
            {
                if (message)
                    mprf(MSGCH_PROMPT, "%s", prompt.c_str());
                else
                    cprintf("%s", prompt.c_str());
            }

            tmp = getchm(KMC_CONFIRM);
#endif
        }

        // If no safe answer exists, we still need to abort when a HUP happens.
        // The caller must handle this case, preferably by issuing an uncancel
        // event that can restart when the game restarts -- and ignore the
        // the return value here.
        if (crawl_state.seen_hups && !safeanswer)
            return false;

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
            clear_messages();

        if (tmp == 'N')
            return false;
        else if (tmp == 'Y')
            return true;
        else if (!noprompt)
        {
            bool upper = !safe && (tmp == 'n' || tmp == 'y'
                                   || crawl_state.game_is_hints_tutorial());
            const string pr = make_stringf("%s[Y]es or [N]o only, please.",
                                           upper ? "Uppercase " : "");
#ifdef TOUCH_UI
            status->text = pr;
#else
            if (message)
                mpr(pr);
            else
                cprintf("%s\n", pr.c_str());
#endif
        }
    }
}

static string _list_alternative_yes(char yes1, char yes2, bool lowered = false,
                                    bool brackets = false)
{
    string help = "";
    bool print_yes = false;
    if (yes1 != 'Y')
    {
        if (lowered)
            help += toalower(yes1);
        else
            help += yes1;
        print_yes = true;
    }

    if (yes2 != 'Y' && yes2 != yes1)
    {
        if (print_yes)
            help += "/";

        if (lowered)
            help += toalower(yes2);
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

static string _list_allowed_keys(char yes1, char yes2, bool lowered = false,
                                 bool allow_all = false)
{
    string result = " [";
    result += (lowered ? "(y)es" : "(Y)es");
    result += _list_alternative_yes(yes1, yes2, lowered);
    if (allow_all)
        result += (lowered? "/(a)ll" : "/(A)ll");
    result += (lowered ? "/(n)o/(q)uit" : "/(N)o/(Q)uit");
    result += "]";

    return result;
}

// Like yesno(), but returns 0 for no, 1 for yes, and -1 for quit.
// alt_yes and alt_yes2 allow up to two synonyms for 'Y'.
// FIXME: This function is shaping up to be a monster. Help!
int yesnoquit(const char* str, bool safe, int safeanswer, bool allow_all,
              bool clear_after, char alt_yes, char alt_yes2)
{
    if (!crawl_state.is_repeating_cmd())
        interrupt_activity(AI_FORCE_INTERRUPT);

    mouse_control mc(MOUSE_MODE_YESNO);

    string prompt =
    make_stringf("%s%s ", str ? str : "Buggy prompt?",
                 _list_allowed_keys(alt_yes, alt_yes2,
                                    safe, allow_all).c_str());
    while (true)
    {
        mprf(MSGCH_PROMPT, "%s", prompt.c_str());

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
            clear_messages();

        if (tmp == 'N')
            return 0;
        else if (tmp == 'Y' || tmp == alt_yes || tmp == alt_yes2)
            return 1;
        else if (allow_all)
        {
            if (tmp == 'A')
                return 2;
            else
            {
                bool upper = !safe && (tmp == 'n' || tmp == 'y' || tmp == 'a'
                                       || crawl_state.game_is_hints_tutorial());
                mprf("Choose %s[Y]es%s, [N]o, [Q]uit, or [A]ll!",
                     upper ? "uppercase " : "",
                     _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
            }
        }
        else
        {
            bool upper = !safe && (tmp == 'n' || tmp == 'y'
                                   || crawl_state.game_is_hints_tutorial());
            mprf("%s[Y]es%s, [N]o or [Q]uit only, please.",
                 upper ? "Uppercase " : "",
                 _list_alternative_yes(alt_yes, alt_yes2, false, true).c_str());
        }
    }
}

//---------------------------------------------------------------
//
// prompt_for_quantity
//
// Returns -1 if ; or enter is pressed (pickup all).
// Else, returns quantity.
//---------------------------------------------------------------
int prompt_for_quantity(const char *prompt)
{
    msgwin_prompt(prompt);

    int ch = getch_ck();
    if (ch == CK_ENTER || ch == ';')
        return -1;
    else if (ch == CK_ESCAPE || ch == CK_REDRAW)
        return 0;

    macro_buf_add(ch);
    return prompt_for_int("", false);
}

//---------------------------------------------------------------
//
// prompt_for_int
//
// If nonneg, then it returns a non-negative number or -1 on fail
// If !nonneg, then it returns an integer, and 0 on fail
//
//---------------------------------------------------------------
int prompt_for_int(const char *prompt, bool nonneg)
{
    char specs[80];

    msgwin_get_line(prompt, specs, sizeof(specs));

    if (specs[0] == '\0')
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

    msgwin_get_line(prompt, specs, sizeof(specs));

    if (specs[0] == '\0')
        return -1;

    char *end;
    double ret = strtod(specs, &end);

    if (ret == 0 && end == specs)
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
