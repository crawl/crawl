/**
 * @file
 * @brief Prompts.
 **/

#pragma once

#include "menu.h"

namespace ui
{
    int error(string err, string title="Error!", bool force_popup=false);
    int message(string msg,
        string title="", string prompt="<cyan>Hit any key to continue...</cyan>",
        bool format_msg=false);
}

bool yes_or_no(PRINTF(0, ));
typedef map<int, int> explicit_keymap;
int yesno(const char * str, bool allow_lowercase, int default_answer,
          bool clear_after = true, bool interrupt_delays = true,
          bool noprompt = false,
          const explicit_keymap *map = nullptr,
          bool allow_popup = true,
          bool ask_always = false);

int prompt_for_quantity(const char *prompt);
int prompt_for_int(const char *prompt, bool nonneg, const string &prefill = "");
double prompt_for_float(const char* prompt);

char index_to_letter(int the_index);

int letter_to_index(int the_letter);


// PromptMenu is an implementation of Menu for use in the message pane. The
// main feature is that, if the menu is small enough to fit as a regular prompt,
// it displays it that way, otherwise, it uses a regular Menu popup. It is
// currently pretty minimalistic in that, when in prompt mode, a lot of the
// more involved Menu features are just not supported.
class PromptMenu : public Menu
{
public:
    PromptMenu(int _flags = MF_SINGLESELECT | MF_ARROWS_SELECT
                            | MF_INIT_HOVER | MF_GRID_LAYOUT)
        : Menu(_flags), columns(0), col_width(0), in_prompt_mode(false)
    {
        // no other mode is supported
        ASSERT(is_set(MF_SINGLESELECT));
    }

    bool process_key(int keyin) override;
    void update_menu(bool update_entries=false) override;

    virtual vector<MenuEntry *> show(bool reuse_selections = false) override;
    virtual vector<MenuEntry *> show_in_msgpane();

    void update_columns();
    void build_prompt_menu();

    bool fits_in_mpane() const;

protected:
    vector<formatted_string> menu_text;
    int columns;
    int col_width;
    bool in_prompt_mode;

};

#ifdef WIZARD
// If _symbol is 0, a copy in a WizardMenu will be given the next unused symbol.
// symbol in the menu's copy.
class WizardEntry : public MenuEntry
{
public:
    WizardEntry(int _symbol, string _text, int _output)
    : MenuEntry(_text, MEL_ITEM, 1, _symbol), output(_output)
    { indent_no_hotkeys = true; }
    WizardEntry(int _symbol, string _text) // Return the symbol from run().
    : WizardEntry(_symbol, _text, _symbol) { }
    WizardEntry(string _text, int _output) // Find a symbol later.
    : WizardEntry(0, _text, _output) { }

    int output;
};

class WizardMenu : private PromptMenu
{
public:
    WizardMenu(string _title, vector<WizardEntry> &_options, int _default=127);
    int run(bool to_bool = false);

    int result() const
    {
        return last_result;
    }
    bool success() const
    {
        return last_success;
    }
protected:
    void run_aux();
    int index_to_symbol(int index) const;
    int next_unused_symbol();
    bool skip_process_command(int keyin) override;

    unsigned last_symbol_index = 0;
    int last_result = 0;
    bool last_success = false;

    int default_value;
};
#endif
