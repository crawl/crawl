/**
 * @file
 * @brief Prompts.
 **/

#include "AppHdr.h"
#include <climits> // UCHAR_MAX
#include <cmath> // isnan

#include "prompt.h"

#include "clua.h"
#include "delay.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "options.h"
#include "scroller.h"
#include "state.h"
#include "stringutil.h"
#ifdef USE_TILE
#include "rltiles/tiledef-gui.h"
#endif
#include "viewchar.h"
#include "ui.h"

namespace ui
{
    class message_scroller : public formatted_scroller
    {
    public:
        message_scroller()
            : formatted_scroller(FS_EASY_EXIT)
        { }

        message_scroller(string text, string title="", string prompt="")
            : message_scroller()
        {
            if (title.size())
                set_title(formatted_string::parse_string(title));

            if (prompt.size())
                set_more(formatted_string::parse_string(prompt));

            add_formatted_string(formatted_string::parse_string(text));
        }

    protected:
        maybe_bool process_key(int ch) override
        {
            // general keycode that is not hinted by default; it is mostly
            // useful for debugging. If you want it to be apparent to players,
            // just set a different `more`. (See e.g. the arena call to
            // the wrapper functions for an example.)
            if (ch == CONTROL('P'))
            {
                if (crawl_state.game_started)
                    replay_messages();
                else
                    replay_messages_during_startup(); // show a title; less cryptic for this case
                return true;
            }
            return formatted_scroller::process_key(ch);
        }
    };

    // Convenience wrapper for `message_scroller`.
    // somewhat general popup code for short messages.
    int message(string msg, string title, string prompt, bool format_msg)
    {
        // XX could this just always accept a format string msg?
        message_scroller ms(
            format_msg ? msg : replace_all(msg, "<", "<<"),
            title, prompt);
        ms.show();
        return ms.get_lastch();
    }

    // slightly smarter error msging than just `mprf(MSGCH_ERROR, ...)`: if
    // there are ui elements on the stack, show a popup on top of them; either
    // way, log the error to MSGCH_ERROR.
    int error(string err, string title, bool force_popup)
    {
        // unlike `message` above, does not accept color formatting in title,
        // so that it can auto-wrap it in an angry red
        // XX direct errorf version of this fn?
        mprf(MSGCH_ERROR, "%s", err.c_str());
        // assumes UI init! See end.cc::fatal_error_notification for a wrapped
        // version that does not.
        // XX unify the two?
        if (force_popup || has_layout())
        {
            if (title.size())
            {
                title = string("<lightred>")
                        + replace_all(title, "<", "<<")
                        + "</lightred>";
            }
            return message(err, title);
        }
        return 0; // no popup used
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
    if (res.is_bool())
        return bool(res);

    string prompt = make_stringf("%s ", str ? str : "Buggy prompt?");

    bool use_popup = !crawl_state.need_save // not in game, no message pane
        || ui::has_layout() // already in a popup; no choice but to use another one
        || Options.prompt_menu; // options want us to use a popup
    use_popup = use_popup && str && allow_popup;

    // MF_ANYPRINTABLE is here because we are running a loop manually
    // XX don't do this
    int flags = MF_SINGLESELECT | MF_ANYPRINTABLE | MF_ALLOW_FORMATTING;
    if (use_popup)
        flags |= MF_ARROWS_SELECT;
    Menu pop(flags, "prompt", KMC_CONFIRM);

    bool enter_defaults = false;

    // don't require the caller to match case with their other settings
    if (default_answer)
    {
        default_answer = toupper_safe(default_answer);
        // if arrow selection is disabled, enter/space activates the default
        // XX what about space with arrow selection? by default this scrolls
        enter_defaults = use_popup && !pop.is_set(MF_ARROWS_SELECT);

        if (enter_defaults || !use_popup)
        {
            // XX a less hacky way, perhaps one that doesn't refer to specific
            // keycodes. (CMD_MENU_DEFAULT?)
            pop.f_keyfilter = [default_answer](int keyin)
                {
                    // XX space for the default doesn't work in a webtiles
                    // popup
                    if (keyin == ' ' || keyin == '\r' || keyin == '\n')
                        keyin = default_answer;
                    return keyin;
                };
        }

    }

    if (use_popup)
    {
        MenuEntry * const y_me = new MenuEntry("Yes", MEL_ITEM, 1, 'Y');
        y_me->add_hotkey('y');
        MenuEntry * const n_me = new MenuEntry("No", MEL_ITEM, 1, 'N');
        n_me->add_hotkey('n');
        MenuEntry * const a_me = new MenuEntry("Always", MEL_ITEM, 1, 'A');
        a_me->add_hotkey('a');
        y_me->add_tile(tile_def(TILEG_PROMPT_YES));
        n_me->add_tile(tile_def(TILEG_PROMPT_NO));

        MenuEntry *question = new MenuEntry(prompt, MEL_TITLE);
        question->wrap_text();
        pop.set_title(question);
        pop.add_entry(y_me);
        pop.add_entry(n_me);
        if (ask_always)
            pop.add_entry(a_me);
        if (default_answer == 'Y')
            pop.set_hovered(1);
        else if (default_answer == 'N')
            pop.set_hovered(2);
        else if (ask_always && default_answer == 'A')
            pop.set_hovered(3);
        else
            enter_defaults = false; // no valid default

        if (enter_defaults)
        {
            pop.set_more(make_stringf("<white>[enter]</white>: %s",
                default_answer == 'Y' ? "Yes"
                : default_answer == 'N' ? "No"
                : (ask_always && default_answer == 'A') ? "Always"
                : "bugs"));
        }
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
                auto answer = pop.selected_entries();
                if (answer.size() && answer[0]->hotkeys.size())
                    tmp = answer[0]->hotkeys[0]; // uppercase version

                // sub in any alpha char if that's what the player typed, for
                // error messaging
                int actual_key = pop.getkey();

                // if the typed key is out of alpha char range, leave it as
                // ESCAPE to avoid a crash
                if (actual_key != EOF && actual_key > UCHAR_MAX)
                    actual_key = tmp;

                if (isalpha(actual_key) && actual_key != tmp)
                    tmp = actual_key;
                // otherwise, leave as ESCAPE
            }
            else
            {
                if (!noprompt)
                    mprf(MSGCH_PROMPT, "%s", prompt.c_str());

                tmp = ui::getch(KMC_CONFIRM);
                if (pop.f_keyfilter)
                    tmp = pop.f_keyfilter(tmp);
            }
        }

        // If no safe answer exists, we still need to abort when a HUP happens.
        // The caller must handle this case, preferably by issuing an uncancel
        // event that can restart when the game restarts -- and ignore the
        // return value here.
        if (crawl_state.seen_hups && !default_answer)
            return false;

        if (map && map->find(tmp) != map->end())
            tmp = map->find(tmp)->second;

        // XX is the esc case always right for use_popup = true?
        if (default_answer && (key_is_escape(tmp) || crawl_state.seen_hups))
            tmp = default_answer;

        if (Options.easy_confirm == easy_confirm_type::all
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
            const string pr = make_stringf("<lightred>%s%s only, please.</lightred>",
                                           upper ? "Uppercase " : "",
                                           ask_always ?
                                               "[Y]es, [N]o, or [A]lways" :
                                               "[Y]es or [N]o");
            if (use_popup)
                pop.set_more(pr); // replaces keyhint if it's there
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

bool PromptMenu::process_key(int keyin)
{
    if (ui_is_initialized())
        return Menu::process_key(keyin);

    // otherwise, we are at a prompt. TODO: should this code try to reuse
    // anything from the superclass? This is extremely minimalistic
    // compared to what the superclass does
    lastch = keyin;
    if (ui::key_exits_popup(lastch, false))
    {
        deselect_all();
        return false;
    }

    const int index = hotkey_to_index(lastch, false);
    if (index >= 0)
    {
        select_index(index, MENU_SELECT_ALL);
        return process_selection();
    }
    return true;
}

static string _prompt_text(const MenuEntry &me)
{
    // XX code dup with MenuEntry::get_text, _get_text_preface
    // I couldn't come up with a great way of generalizing the superclass
    // get_text, at least one that didn't involve a lot of unnecessary changes
    // to other menu subclasses. So we get the brute force version for now.
    if (me.level == MEL_ITEM && me.hotkeys_count())
        return make_stringf("(%s) %s", keycode_to_name(me.hotkeys[0]).c_str(), me.text.c_str());
    else if (me.level == MEL_ITEM && me.indent_no_hotkeys)
        return make_stringf("    %s", me.text.c_str());
    else
        return me.text;
}

void PromptMenu::update_columns()
{
    // the 100 here is somewhat heuristic; on a fairly hires monitor, using
    // the full local tiles line width seems way too wide, and 80 is quite
    // narrow. 100 seems like an ok compromise that also allows the entire
    // &~ menu to show up.
    const int max_line_width = min(100, static_cast<int>(msgwin_line_length()));
    if (items.size() == 0)
    {
        // prevent weird values with no menu items
        columns = 1;
        col_width = max_line_width;
        return;
    }
    int max_width = 0;
    for (MenuEntry *item : items)
        max_width = max(max_width, static_cast<int>(_prompt_text(*item).size()));
    // the + 2 here is to allow at least 2 spaces between cols.
    // currently no limit on columns, maybe it shouldn't be more than 6 or so?
    columns = max(1, max_line_width / (max_width + 2));
    col_width = max_line_width / columns;
}

void PromptMenu::build_prompt_menu()
{
    // rebuild the menu text to be shown at a prompt
    menu_text.clear();
    update_columns();
    int c = 0;
    formatted_string line;
    for (MenuEntry *item : items)
    {
        if (item->level != MEL_ITEM && c != 0)
        {
            menu_text.push_back(line);
            line.clear();
        }

        line.textcolour(item_colour(item));
        // TODO: support MF_ALLOW_FORMATTING
        line.cprintf("%-*s",
            col_width,
            _prompt_text(*item).c_str()); // _prompt_text handles the hotkey

        c++;
        if (c >= columns || item->level != MEL_ITEM)
        {
            c = 0;
            menu_text.push_back(line);
            line.clear();
        }
    }
    if (!line.empty())
        menu_text.push_back(line);
}

void PromptMenu::update_menu(bool update_entries)
{
    build_prompt_menu();
    Menu::update_menu(update_entries);
}

bool PromptMenu::fits_in_mpane() const
{
    // check is relative to current column settings...
    // handle rounding properly:
    const int rows = (items.size() + columns - 1) / columns;
    return rows < static_cast<int>(msgwin_lines());
}

vector<MenuEntry *> PromptMenu::show(bool reuse_selections)
{
    // if options allow, the menu items fit in the msgpane, and there is no
    // currently running ui layout, show it in the message pane. Otherwise,
    // use a popup. Prompting in the message pain is more classic, and is the
    // default on console, but the popup is more consistent and is otherwise
    // the default. (This is all a bit janky, but message pain prompts
    // historically are way more janky than this; and I don't want to remove
    // them because of their classic feel.)
    update_columns();
    in_prompt_mode = !Options.prompt_menu && !ui::has_layout() && fits_in_mpane();

    if (in_prompt_mode)
        return show_in_msgpane();
    else
        return Menu::show(reuse_selections);
}

vector<MenuEntry *> PromptMenu::show_in_msgpane()
{
    clear_messages();
    msgwin_temporary_mode temp;
    build_prompt_menu(); // could just rebuild it on every loop...
    while (true)
    {
        if (!fits_in_mpane())
        {
            // build_prompt_menu updates the column widths, so if the menu
            // gets updated by process_key, this may trigger on successive
            // loops. Once we are in menu mode, no going back to prompt mode.
            in_prompt_mode = false;
            return Menu::show();
        }
        msgwin_clear_temporary();
        const auto *t_entry = get_cur_title();
        // currently ignores entry color for the title, assuming that the
        // default prompt color is appropriate...
        const string prompt_text = t_entry ? t_entry->text : "? ";

        // We do this line-by-line so that webtiles formatting comes out
        // correctly. (Unfortunately, webtiles does not add a prefix to every
        // line of a multi-line mpr, whereas console/tiles builds do.)
        for (const auto &line : menu_text)
            formatted_mpr(line, MSGCH_PROMPT);
        mprf(MSGCH_PROMPT, "%s", prompt_text.c_str());

        int key = get_ch();
        if (remap_numpad)
            key = numpad_to_regular(key, true);

        if (!process_key(key))
            break;
    }
    get_selected(&sel);
    return sel;
}

#ifdef WIZARD

WizardMenu::WizardMenu(string _title, vector<WizardEntry> &_options,
                       int _default)
    : PromptMenu(MF_SINGLESELECT | MF_ARROWS_SELECT | MF_INIT_HOVER
                 | MF_GRID_LAYOUT | MF_ANYPRINTABLE), default_value(_default)
{
    set_title(new MenuEntry(_title, MEL_TITLE));
    for (auto &o : _options)
    {
        auto p = new WizardEntry(o); // Menu expects new pointers.
        if (!p->hotkeys_count())
            p->add_hotkey(next_unused_symbol()); // Give everything a hotkey.
        add_entry(p);
    }
}

int WizardMenu::run(bool to_bool)
{
    run_aux();
    return to_bool ? last_success : last_result;
}

// Display the menu. If something is selected, set success and return the
// selected thing. If not, clear success and return default_value.
void WizardMenu::run_aux()
{
    show();
    last_success = !sel.empty();
    if (last_success)
        last_result = (dynamic_cast<WizardEntry*>(sel[0]))->output;
    else
    {
        canned_msg(MSG_OK);
        last_result = default_value;
    }
}

int WizardMenu::index_to_symbol(int index) const
{
    if (index < 52)
        return index_to_letter(index);
    else if (last_symbol_index < 69)
        return '0'+last_symbol_index-52;
    else
        return 0; // 0 no hotkey, cursor keys only.
}

int WizardMenu::next_unused_symbol()
{
    while (true)
    {
        int sym = index_to_symbol(last_symbol_index++);
        if (!sym || -1 == hotkey_to_index(sym, true))
            return sym;
    }
}

bool WizardMenu::skip_process_command(int keyin)
{
    if (keyin == '!' || keyin == '?')
        return true; // potions and scrolls in item creation
    return Menu::skip_process_command(keyin);
}

#endif // defined(WIZARD)
