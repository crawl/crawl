/**
 * @file
 * @brief Functions for inventory related commands.
**/

#ifndef INVENT_H
#define INVENT_H

#include <cstddef>
#include <vector>

#include "enum.h"
#include "itemname.h"
#include "itemprop-enum.h"
#include "menu.h"

enum object_selector
{
    OSEL_ANY                     =  -1,
    OSEL_WIELD                   =  -2,
    OSEL_UNIDENT                 =  -3,
    OSEL_EQUIP                   =  -4,
    OSEL_RECHARGE                =  -5,
    OSEL_ENCH_ARM                =  -6,
//  OSEL_VAMP_EAT                =  -7,
    OSEL_DRAW_DECK               =  -8,
    OSEL_THROWABLE               =  -9,
    OSEL_EVOKABLE                = -10,
    OSEL_WORN_ARMOUR             = -11,
    OSEL_FRUIT                   = -12,
    OSEL_CURSED_WORN             = -13,
    OSEL_UNCURSED_WORN_ARMOUR    = -14,
    OSEL_UNCURSED_WORN_JEWELLERY = -15,
    OSEL_BRANDABLE_WEAPON        = -16,
    OSEL_ENCHANTABLE_WEAPON      = -17,
    OSEL_BLESSABLE_WEAPON        = -18,
};

#define PROMPT_ABORT         -1
#define PROMPT_GOT_SPECIAL   -2
#define PROMPT_NOTHING       -3
#define PROMPT_INAPPROPRIATE -4

#define SLOT_BARE_HANDS      PROMPT_GOT_SPECIAL

extern FixedVector<int, NUM_OBJECT_CLASSES> inv_order;

struct SelItem
{
    int slot;
    int quantity;
    const item_def *item;

    SelItem() : slot(0), quantity(0), item(nullptr) { }
    SelItem(int s, int q, const item_def *it = nullptr)
        : slot(s), quantity(q), item(it)
    {
    }
};

typedef string (*invtitle_annotator)(const Menu *m, const string &oldtitle);

class InvTitle : public MenuEntry
{
public:
    InvTitle(Menu *mn, const string &title, invtitle_annotator tfn);

    string get_text(const bool = false) const;

private:
    Menu *m;
    invtitle_annotator titlefn;
};

class InvEntry : public MenuEntry
{
private:
    static bool show_prices;
    static bool show_glyph;

    mutable string basename;
    mutable string qualname;
    mutable string dbname;

protected:
    static bool show_cursor;
    // Should we show the floor tile, etc?
    bool show_background;

public:
    const item_def *item;

    InvEntry(const item_def &i, bool show_bg = false);
    string get_text(const bool need_cursor = false) const;
    void set_show_glyph(bool doshow);
    static void set_show_cursor(bool doshow);

    const string &get_basename() const;
    const string &get_qualname() const;
    const string &get_fullname() const;
    const string &get_dbname() const;
    bool         is_item_cursed() const;
    bool         is_item_glowing() const;
    bool         is_item_ego() const;
    bool         is_item_art() const;
    bool         is_item_equipped() const;

    virtual int highlight_colour() const
    {
        return menu_colour(get_text(), item_prefix(*item), tag);
    }

    virtual void select(int qty = -1);

    virtual string get_filter_text() const;

#ifdef USE_TILE
    virtual bool get_tiles(vector<tile_def>& tiles) const;
#endif

private:
    void add_class_hotkeys(const item_def &i);
};

class InvMenu : public Menu
{
public:
    InvMenu(int mflags = MF_MULTISELECT);

public:
    unsigned char getkey() const;

    void set_preselect(const vector<SelItem> *pre);
    void set_type(menu_type t);

    // Sets function to annotate the title with meta-information if needed.
    // If you set this, do so *before* calling set_title, or it won't take
    // effect.
    void set_title_annotator(invtitle_annotator fn);

    void set_title(MenuEntry *title, bool first = true);
    void set_title(const string &s);

    // Loads items into the menu. If "procfn" is provided, it'll be called
    // for each MenuEntry added.
    // NOTE: Does not set menu title, ever! You *must* set the title explicitly
    menu_letter load_items(const vector<const item_def*> &items,
                           MenuEntry *(*procfn)(MenuEntry *me) = nullptr,
                           menu_letter ckey = 'a', bool sort = true);

    // Loads items from the player's inventory into the menu, and sets the
    // title to the stock title. If "procfn" is provided, it'll be called for
    // each MenuEntry added, *excluding the title*.
    void load_inv_items(int item_selector = OSEL_ANY, int excluded_slot = -1,
                        MenuEntry *(*procfn)(MenuEntry *me) = nullptr);

    vector<SelItem> get_selitems() const;

    // Returns vector of item_def pointers to each item_def in the given
    // vector. Note: make sure the original vector stays around for the lifetime
    // of the use of the item pointers, or mayhem results!
    static vector<const item_def*> xlat_itemvect(const vector<item_def> &);
    const menu_sort_condition *find_menu_sort_condition() const;
    void sort_menu(vector<InvEntry*> &items, const menu_sort_condition *cond);

protected:
    bool process_key(int key);
    void do_preselect(InvEntry *ie);
    virtual bool is_selectable(int index) const;
    virtual bool allow_easy_exit() const;

protected:
    menu_type type;
    const vector<SelItem> *pre_select;

    invtitle_annotator title_annotate;
    string temp_title;
};

void get_class_hotkeys(const int type, vector<char> &glyphs);

bool is_item_selected(const item_def &item, int selector);
bool any_items_of_type(int type_expect, int excluded_slot = -1);
string no_selectables_message(int item_selector);

string slot_description();

int prompt_invent_item(const char *prompt,
                       menu_type type,
                       int type_expect,
                       bool must_exist = true,
                       bool auto_list = true,
                       bool allow_easy_quit = true,
                       const char other_valid_char = '\0',
                       int excluded_slot = -1,
                       int *const count = nullptr,
                       operation_types oper = OPER_ANY,
                       bool allow_list_known = false,
                       bool do_warning = true);

vector<SelItem> select_items(
                        const vector<const item_def*> &items,
                        const char *title, bool noselect = false,
                        menu_type mtype = MT_PICKUP,
                        invtitle_annotator titlefn = nullptr);

vector<SelItem> prompt_invent_items(
                        const char *prompt,
                        menu_type type,
                        int type_expect,
                        invtitle_annotator titlefn = nullptr,
                        bool auto_list = true,
                        bool allow_easy_quit = true,
                        const char other_valid_char = '\0',
                        vector<text_pattern> *filter = nullptr,
                        Menu::selitem_tfn fn = nullptr,
                        const vector<SelItem> *pre_select = nullptr);

unsigned char get_invent(int invent_type, bool redraw = true);

bool in_inventory(const item_def &i);
void identify_inventory();

const char *item_class_name(int type, bool terse = false);
const char *item_slot_name(equipment_type type);

bool check_old_item_warning(const item_def& item, operation_types oper);
bool check_warning_inscriptions(const item_def& item, operation_types oper);

void init_item_sort_comparators(item_sort_comparators &list,
                                const string &set);

bool prompt_failed(int retval);

void list_charging_evokers(FixedVector<item_def*, NUM_MISCELLANY> &evokers);

bool item_is_wieldable(const item_def &item);
bool item_is_evokable(const item_def &item, bool reach = true,
                      bool known = false, bool all_wands = false,
                      bool msg = false, bool equip = true);
bool nasty_stasis(const item_def &item, operation_types oper);
bool needs_handle_warning(const item_def &item, operation_types oper,
                          bool &penance);
#endif
