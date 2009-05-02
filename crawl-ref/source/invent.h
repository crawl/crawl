/*
 *  File:       invent.h
 *  Summary:    Functions for inventory related commands.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef INVENT_H
#define INVENT_H

#include <stddef.h>
#include <vector>
#include "menu.h"
#include "enum.h"
#include "itemname.h"

enum object_selector
{
    OSEL_ANY         =  -1,
    OSEL_WIELD       =  -2,
    OSEL_UNIDENT     =  -3,
    OSEL_EQUIP       =  -4,
    OSEL_MEMORISE    =  -5,
    OSEL_RECHARGE    =  -6,
    OSEL_ENCH_ARM    =  -7,
    OSEL_VAMP_EAT    =  -8,
    OSEL_DRAW_DECK   =  -9,
    OSEL_THROWABLE   = -10,
    OSEL_BUTCHERY    = -11,
    OSEL_EVOKABLE    = -12,
    OSEL_WORN_ARMOUR = -13
};

#define PROMPT_ABORT        -1
#define PROMPT_GOT_SPECIAL  -2
#define PROMPT_NOTHING      -3

struct SelItem
{
    int slot;
    int quantity;
    const item_def *item;

    SelItem() : slot(0), quantity(0), item(NULL) { }
    SelItem( int s, int q, const item_def *it = NULL )
        : slot(s), quantity(q), item(it)
    {
    }
};

typedef std::string (*invtitle_annotator)(
            const Menu *m, const std::string &oldtitle);

struct InvTitle : public MenuEntry
{
    Menu *m;
    invtitle_annotator titlefn;

    InvTitle( Menu *mn, const std::string &title,
              invtitle_annotator tfn );

    std::string get_text() const;
};

class InvShowPrices;
class InvEntry : public MenuEntry
{
private:
    static bool show_prices;
    static bool show_glyph;
    static void set_show_prices(bool doshow);

    mutable std::string basename;
    mutable std::string qualname;

    friend class InvShowPrices;

public:
    const item_def *item;

    InvEntry( const item_def &i );
    std::string get_text() const;
    void set_show_glyph(bool doshow);

    const std::string &get_basename() const;
    const std::string &get_qualname() const;
    const std::string &get_fullname() const;
    const bool        is_item_cursed() const;
    const bool        is_item_glowing() const;
    const bool        is_item_ego() const;
    const bool        is_item_art() const;
    const bool        is_item_equipped() const;
    const int         item_freshness() const;

    virtual int highlight_colour() const
    {
        return menu_colour(get_text(),
                           menu_colour_item_prefix( *( (item_def*) item) ),
                           tag );
    }

    virtual void select( int qty = -1 );

    virtual std::string get_filter_text() const;

#ifdef USE_TILE
    virtual bool get_tiles(std::vector<tile_def>& tiles) const;
#endif

private:
    void add_class_hotkeys(const item_def &i);
};

class InvShowPrices
{
public:
    InvShowPrices(bool doshow = true);
    ~InvShowPrices();
};

class InvMenu : public Menu
{
public:
    InvMenu(int mflags = MF_MULTISELECT);

public:
    unsigned char getkey() const;

    void set_preselect(const std::vector<SelItem> *pre);
    void set_type(menu_type t);

    // Sets function to annotate the title with meta-information if needed.
    // If you set this, do so *before* calling set_title, or it won't take
    // effect.
    void set_title_annotator(invtitle_annotator fn);

    void set_title(MenuEntry *title, bool first = true);
    void set_title(const std::string &s);

    // Loads items into the menu. If "procfn" is provided, it'll be called
    // for each MenuEntry added.
    // NOTE: Does not set menu title, ever! You *must* set the title explicitly
    void load_items(const std::vector<const item_def*> &items,
                    MenuEntry *(*procfn)(MenuEntry *me) = NULL);

    // Loads items from the player's inventory into the menu, and sets the
    // title to the stock title. If "procfn" is provided, it'll be called for
    // each MenuEntry added, *excluding the title*.
    void load_inv_items(int item_selector = OSEL_ANY, int excluded_slot = -1,
                        MenuEntry *(*procfn)(MenuEntry *me) = NULL);

    std::vector<SelItem> get_selitems() const;

    // Returns vector of item_def pointers to each item_def in the given
    // vector. Note: make sure the original vector stays around for the lifetime
    // of the use of the item pointers, or mayhem results!
    static std::vector<const item_def*> xlat_itemvect(
            const std::vector<item_def> &);
    const menu_sort_condition *find_menu_sort_condition() const;
    void sort_menu(std::vector<InvEntry*> &items,
                   const menu_sort_condition *cond);

protected:
    bool process_key(int key);
    void do_preselect(InvEntry *ie);
    virtual bool is_selectable(int index) const;

protected:
    menu_type type;
    const std::vector<SelItem> *pre_select;

    invtitle_annotator title_annotate;
};


int prompt_invent_item( const char *prompt,
                        menu_type type,
                        int type_expect,
                        bool must_exist = true,
                        bool allow_auto_list = true,
                        bool allow_easy_quit = true,
                        const char other_valid_char = '\0',
                        int excluded_slot = -1,
                        int *const count = NULL,
                        operation_types oper = OPER_ANY,
                        bool allow_list_known = false );

std::vector<SelItem> select_items(
                        const std::vector<const item_def*> &items,
                        const char *title, bool noselect = false,
                        menu_type mtype = MT_PICKUP );

std::vector<SelItem> prompt_invent_items(
                        const char *prompt,
                        menu_type type,
                        int type_expect,
                        invtitle_annotator titlefn = NULL,
                        bool allow_auto_list = true,
                        bool allow_easy_quit = true,
                        const char other_valid_char = '\0',
                        std::vector<text_pattern> *filter = NULL,
                        Menu::selitem_tfn fn = NULL,
                        const std::vector<SelItem> *pre_select = NULL );


unsigned char invent_select(
                   // Use NULL for stock Inventory title
                   const char *title = NULL,
                   // MT_DROP allows the multidrop toggle
                   menu_type type = MT_INVLIST,
                   int item_selector = OSEL_ANY,
                   int excluded_slot = -1,
                   int menu_select_flags = MF_NOSELECT,
                   invtitle_annotator titlefn = NULL,
                   std::vector<SelItem> *sels = NULL,
                   std::vector<text_pattern> *filter = NULL,
                   Menu::selitem_tfn fn = NULL,
                   const std::vector<SelItem> *pre_select = NULL );

void browse_inventory( bool show_price );
unsigned char get_invent(int invent_type);

bool in_inventory(const item_def &i);

std::string item_class_name(int type, bool terse = false);

bool check_old_item_warning(const item_def& item, operation_types oper);
bool check_warning_inscriptions(const item_def& item, operation_types oper);
bool has_warning_inscription(const item_def& item, operation_types oper);

void init_item_sort_comparators(item_sort_comparators &list,
                                const std::string &set);

bool prompt_failed(int retval, std::string msg = "");

bool item_is_evokable(const item_def &item, bool known = false,
                      bool msg = false);

#endif
