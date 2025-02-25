/**
 * @file
 * @brief Functions for making use of inventory items.
**/

#include "AppHdr.h"

#include "item-use.h"

#include "ability.h"
#include "acquire.h"
#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "chardump.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "corpse.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "fight.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "known-items.h"
#include "level-state-type.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "melee-attack.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "orb.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "sound.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "target.h"
#include "terrain.h"
#include "throw.h"
#include "tiles-build-specific.h"
#include "transform.h"
#include "uncancel.h"
#include "unwind.h"
#include "view.h"
#include "xom.h"

static bool _equip_oper(operation_types oper);
static bool _try_unwield_weapons();

// The menu class for using items from either inv or floor.
// Derivative of InvMenu

class UseItemMenu : public InvMenu
{
    void reset(operation_types oper, const char* prompt_override=nullptr);
    bool init_modes();
    bool populate_list(bool check_only=false);
    void populate_menu();
    bool process_key(int key) override;
    void update_sections();
    void clear() override;
    bool examine_index(int i) override;
    bool cycle_mode(bool forward) override;
    void save_hover();
    void restore_hover(bool preserve_pos);
    string get_keyhelp(bool scrollable) const override;
    bool skip_process_command(int keyin) override
    {
        // override superclass behavior for everything except id menu
        if (keyin == '!' && item_type_filter != OSEL_UNIDENT)
            return false;
        return InvMenu::skip_process_command(keyin);
    }

    command_type get_command(int keyin) override
    {
        if (keyin == '?' && item_type_filter != OSEL_UNIDENT)
            return CMD_MENU_EXAMINE;
        return InvMenu::get_command(keyin);
    }

public:
    UseItemMenu(operation_types oper, int selector, const char* prompt);

    bool display_all;
    bool is_inventory;
    int item_type_filter;
    operation_types oper;

    int saved_inv_item;
    int saved_hover;
    int last_inv_pos;

    // XX these probably shouldn't be const...
    vector<const item_def*> item_inv;
    vector<const item_def*> item_floor;

    bool do_easy_floor;

    void toggle_inv_or_floor();
    void set_hovered(int hovered, bool force=false) override;
    bool cycle_headers(bool=true) override;

    bool empty_check() const;
    bool show_unarmed() const
    {
        return oper == OPER_WIELD || oper == OPER_EQUIP;
    }

private:
    MenuEntry *inv_header;
    MenuEntry *floor_header;
    vector<operation_types> available_modes;
};

static string _default_use_title(operation_types oper)
{
    switch (oper)
    {
    case OPER_EQUIP:
        return Options.equip_unequip
            ? "Equip or unequip which item?"
            : "Equip which item?";
    case OPER_WIELD:
        return Options.equip_unequip
            ? "Wield or unwield which item (- for none)?"
            : "Wield which item (- for none)?";
    case OPER_WEAR:
        if (Options.equip_unequip)
            return "Wear or take off which item?";
        return "Wear which item?";
    case OPER_PUTON:
        return Options.equip_unequip
            ? "Put on or remove which piece of jewellery?"
            : "Put on which piece of jewellery?";
    case OPER_QUAFF:
        return "Drink which item?";
    case OPER_READ:
        return "Read which item?";
    case OPER_EVOKE:
        return "Evoke which item?";
    case OPER_TAKEOFF:
        return "Take off which piece of armour?";
    case OPER_REMOVE:
        return "Remove which piece of jewellery?";
    case OPER_UNEQUIP:
        return "Unequip which item?";
    default:
        return "buggy";
    }
}

// XX code dup with invent.cc, _operation_verb
static string _oper_name(operation_types oper)
{
    switch (oper)
    {
    case OPER_EQUIP: return "equip";
    case OPER_WIELD: return "wield";
    case OPER_WEAR:  return "wear";
    case OPER_PUTON: return "put on";
    case OPER_QUAFF: return "quaff";
    case OPER_READ:  return "read";
    case OPER_EVOKE: return "evoke";
    case OPER_TAKEOFF: return "take off";
    case OPER_REMOVE:  return "remove";
    case OPER_UNEQUIP: return "unequip";
    default:
        return "buggy";
    }
}

static int _default_osel(operation_types oper)
{
    switch (oper)
    {
    case OPER_WIELD:
        return OSEL_WIELD; // XX is this different from OBJ_WEAPONS any more?
    case OPER_WEAR:
        return OBJ_ARMOUR;
    case OPER_PUTON:
        return OBJ_JEWELLERY;
    case OPER_QUAFF:
        return OBJ_POTIONS;
    case OPER_READ:
        return OBJ_SCROLLS;
    case OPER_EVOKE:
        return OSEL_EVOKABLE;
    case OPER_EQUIP:
        return OSEL_EQUIPABLE;
    case OPER_TAKEOFF:
        return OSEL_WORN_ARMOUR;
    case OPER_REMOVE:
        return OSEL_WORN_JEWELLERY;
    case OPER_UNEQUIP:
        return OSEL_WORN_EQUIPABLE;
    default:
        return OSEL_ANY; // buggy?
    }
}

static vector<operation_types> _oper_to_mode(operation_types o)
{
    static const vector<operation_types> wearables
        = {OPER_EQUIP, OPER_WIELD, OPER_WEAR, OPER_PUTON};
    static const vector<operation_types> removables
        = {OPER_UNEQUIP, OPER_TAKEOFF, OPER_REMOVE};
    static const vector<operation_types> usables
        = {OPER_READ, OPER_QUAFF, OPER_EVOKE};

    // return a copy
    vector<operation_types> result;
    if (find(wearables.begin(), wearables.end(), o) != wearables.end())
        result = wearables;
    else if (find(usables.begin(), usables.end(), o) != usables.end())
        result = usables;
    else if (find(removables.begin(), removables.end(), o) != removables.end())
        result = removables;

    return result;
}

bool UseItemMenu::init_modes()
{
    available_modes = _oper_to_mode(oper);
    if (available_modes.empty())
        return false;

    {
        unwind_var<operation_types> cur_oper(oper);
        unwind_var<int> cur_filter(item_type_filter);

        erase_if(available_modes, [cur_oper, this](operation_types o) {
            oper = o;
            item_type_filter = _default_osel(oper);
            return !populate_list(true);
        });
    }

    if (available_modes.size() == 0)
        return false;

    // If the originally requested oper did not survive (e.g. because there are
    // no items that match it), abort.
    //
    // (While there may be usable alternate modes, it's more confusing if a
    // player presses a menu button and a different menu appears with no
    // explanation.)
    if (_equip_oper(oper)
        && find(available_modes.begin(), available_modes.end(), oper)
                                                    == available_modes.end())
    {
        return false;
    }

    return true;
}

bool UseItemMenu::cycle_mode(bool forward)
{
    if (available_modes.size() <= 1)
        return false;

    auto it = find(available_modes.begin(), available_modes.end(), oper);
    if (it == available_modes.end())
        return false; // or assert?

    // ugly iterator clock math
    if (forward)
        it = it + 1 == available_modes.end() ? available_modes.begin() : it + 1;
    else
        it = it == available_modes.begin() ? available_modes.end() - 1 : it - 1;

    oper = *it;

    save_hover();
    item_type_filter = _default_osel(oper);

    // always reset display all on mode change
    display_all = false;

    clear();
    populate_list();
    populate_menu();
    update_sections();
    set_title(_default_use_title(oper));
    restore_hover(true);
    return true;
}

class TempUselessnessHighlighter : public MenuHighlighter
{
public:
    int entry_colour(const MenuEntry *entry) const override
    {
        return entry->colour != MENU_ITEM_STOCK_COLOUR ? entry->colour
               : entry->highlight_colour(true);
    }
};

void UseItemMenu::reset(operation_types _oper, const char* prompt_override)
{
    oper = _oper;

    display_all = false;

    clear();
    // init_modes may change oper
    init_modes();
    if (prompt_override)
        set_title(prompt_override);
    else
        set_title(_default_use_title(oper));
    // see `item_is_selected` for more on what can be used for item_type.
    if (item_type_filter == OSEL_ANY)
        item_type_filter = _default_osel(oper);

    populate_list();
    populate_menu();
    // for wield in particular:
    // start hover on wielded item, if there is one, otherwise on -
    if (oper == OPER_WIELD && item_inv.size() > 0 && you.weapon())
        set_hovered(1);
    update_title();
    update_more();
}

UseItemMenu::UseItemMenu(operation_types _oper, int item_type=OSEL_ANY,
                                    const char* prompt=nullptr)
    : InvMenu(MF_SINGLESELECT | MF_ARROWS_SELECT
                | MF_INIT_HOVER | MF_ALLOW_FORMATTING),
        display_all(false), is_inventory(true),
        item_type_filter(item_type), oper(_oper), saved_inv_item(NON_ITEM),
        saved_hover(-1), last_inv_pos(-1), do_easy_floor(false),
        inv_header(nullptr), floor_header(nullptr)
{
    set_tag("use_item");
    set_flags(get_flags() & ~MF_USE_TWO_COLUMNS);
    set_highlighter(new TempUselessnessHighlighter()); // pointer managed by Menu
    menu_action = ACT_EXECUTE;
    reset(oper, prompt);
}

bool UseItemMenu::populate_list(bool check_only)
{
    if (check_only && show_unarmed())
        return true; // any char can go unarmed

    // Load inv items first
    for (const auto &item : you.inv)
    {
        if (item.defined() && item_is_selected(item, item_type_filter))
        {
            if (check_only)
                return true;
            item_inv.push_back(&item);
        }
    }
    // Load floor items...
    vector<const item_def*> floor = const_item_list_on_square(
                                            you.visible_igrd(you.pos()));

    for (const auto *it : floor)
    {
        // ...only stuff that can go into your inventory though
        if (!it->defined() || item_is_stationary(*it) || item_is_spellbook(*it)
            || item_is_collectible(*it) || it->base_type == OBJ_GOLD)
        {
            continue;
        }
        // No evoking wands, etc from the floor!
        if (oper == OPER_EVOKE && it->base_type != OBJ_TALISMANS)
            continue;

        // even with display_all, only show matching floor items.
        if (!item_is_selected(*it, item_type_filter))
            continue;

        if (check_only)
            return true;
        item_floor.push_back(it);
    }

    return !check_only && (item_inv.size() > 0 || item_floor.size() > 0);
}

bool UseItemMenu::empty_check() const
{
    // (if choosing weapons, then bare hands are always a possibility)
    if (show_unarmed())
        return false;
    if (any_items_of_type(item_type_filter, -1, oper != OPER_EVOKE))
        return false;
    // Only talismans can be evoked from the floor.
    if (oper == OPER_EVOKE)
    {
        auto floor_items = item_list_on_square(you.visible_igrd(you.pos()));
        if (any_of(begin(floor_items), end(floor_items),
              [=] (const item_def* item) -> bool
              {
                  return item->defined()
                         && item->base_type == OBJ_TALISMANS
                         && item_is_selected(*item, item_type_filter);
              }))
        {
            return false;
        }
    }

    mprf(MSGCH_PROMPT, "%s",
        no_selectables_message(item_type_filter).c_str());
    return true;
}

static void _note_tele_cancel(MenuEntry* entry)
{
    auto ie = dynamic_cast<InvEntry *>(entry);
    if (ie && ie->item
        && ie->item->base_type == OBJ_SCROLLS
        && ie->item->sub_type == SCR_TELEPORTATION
        && you.duration[DUR_TELEPORT])
    {
        ie->text += " (cancels current teleport)";
    }
}

void UseItemMenu::populate_menu()
{
    const bool use_category_selection = item_type_filter == OSEL_UNIDENT
                                        || _equip_oper(oper);
    if (_equip_oper(oper))
        set_flags(get_flags() | MF_SECONDARY_SCROLL);

    if (item_inv.empty())
        is_inventory = false;
    else if (item_floor.empty())
        is_inventory = true;

    // Entry for unarmed. Hotkey works for either subsection, though selecting
    // it (currently) enables the inv section.
    if (show_unarmed())
    {
        string hands_string = you.unarmed_attack_name("Unarmed");
        if (!you.weapon())
            hands_string += " (current attack)";

        MenuEntry *hands = new MenuEntry(hands_string, MEL_ITEM);
        if (!you.weapon())
            hands->colour = LIGHTGREEN;
        hands->add_hotkey('-');
        hands->on_select = [this](const MenuEntry&)
            {
                lastch = '-';
                return false;
            };
        add_entry(hands);
    }

    if (!item_inv.empty())
    {
        // Only clarify that these are inventory items if there are also floor
        // items.
        if (!item_floor.empty())
        {
            inv_header = new MenuEntry("Inventory Items", MEL_TITLE);
            inv_header->colour = LIGHTCYAN;
            add_entry(inv_header);
        }
        load_items(item_inv,
                    [&](MenuEntry* entry) -> MenuEntry*
                    {
                        if (item_type_filter == OBJ_SCROLLS)
                            _note_tele_cancel(entry);
                        return entry;
                    }, 'a', true, use_category_selection);
    }
    last_inv_pos = items.size() - 1;

    if (!item_floor.empty())
    {
#ifndef USE_TILE
        // vertical padding for console
        if (!item_inv.empty())
            add_entry(new MenuEntry("", MEL_TITLE));
#endif
        // Load floor items to menu. Always add a subtitle, even if there are
        // no inv items.
        floor_header = new MenuEntry("Floor Items", MEL_TITLE);
        floor_header->colour = LIGHTCYAN;
        add_entry(floor_header);

        load_items(item_floor,
                    [&](MenuEntry* entry) -> MenuEntry*
                    {
                        if (item_type_filter == OBJ_SCROLLS)
                            _note_tele_cancel(entry);
                        return entry;
                    }, 'a', true, false);
    }
    update_sections();

    if (last_hovered >= 0 && !item_floor.empty() && !item_inv.empty())
    {
        if (is_inventory && last_hovered > last_inv_pos)
            set_hovered(show_unarmed() ? 1 : 0);
        else if (!is_inventory && last_hovered <= last_inv_pos)
            set_hovered(last_inv_pos + 1);
    }
}

static bool _enable_equip_check(operation_types o, MenuEntry *entry)
{
    auto ie = dynamic_cast<InvEntry *>(entry);
    if (o != OPER_EQUIP && o != OPER_WIELD && o != OPER_PUTON && o != OPER_WEAR)
        return true;
    return !ie || !ie->item || !item_is_equipped(*(ie->item))
           || (item_is_equipped(*(ie->item)) && Options.equip_unequip);
}

static bool _disable_item(const MenuEntry &)
{
    // XX would be nice to show an informative message for this case
    return true;
}

static item_def *_entry_to_item(MenuEntry *me)
{
    if (!me)
        return nullptr;
    auto ie = dynamic_cast<InvEntry *>(me);
    if (!ie)
        return nullptr;
    auto tgt = const_cast<item_def*>(ie->item);
    return tgt;
}

void UseItemMenu::save_hover()
{
    // when switch modes, we want to keep hover on the currently selected item
    // even if it moves around, or failing that, in a similar position in the
    // menu. This code accomplishes that.
    saved_hover = last_hovered;
    if (last_hovered < 0)
    {
        saved_inv_item = NON_ITEM;
        return;
    }

    auto tgt = _entry_to_item(items[last_hovered]);
    if (!tgt || !in_inventory(*tgt))
    {
        saved_inv_item = NON_ITEM;
        return;
    }
    saved_inv_item = tgt->link;
}

void UseItemMenu::restore_hover(bool preserve_pos)
{
    // if there is a saved hovered item, look for that first
    if (saved_inv_item != NON_ITEM)
        for (int i = 0; i < static_cast<int>(items.size()); i++)
            if (auto tgt = _entry_to_item(items[i]))
                if (tgt->link == saved_inv_item)
                {
                    set_hovered(i);
                    saved_inv_item = NON_ITEM;
                    saved_hover = -1;
                    return;
                }

    // nothing found
    saved_inv_item = NON_ITEM;
    const bool use_hover = is_set(MF_ARROWS_SELECT);
    if (!preserve_pos || !use_hover)
        set_hovered(use_hover ? 0 : -1);
    else
    {
        // if preserve_pos is set, try to keep an existing saved hover position
        set_hovered(saved_hover);
    }
    saved_hover = -1;

    // fixup hover in case headings have moved
    if (last_hovered >= 0 && items[last_hovered]->level != MEL_ITEM)
        cycle_hover();
}

void UseItemMenu::update_sections()
{
    // disable unarmed on equip menus if player is unarmed
    if (show_unarmed() && !you.weapon())
        items[0]->on_select = _disable_item;
    int i = show_unarmed() ? 1 : 0;
    for (; i <= last_inv_pos; i++)
        if (items[i]->level == MEL_ITEM)
        {
            items[i]->set_enabled(is_inventory);
            if (is_inventory && !_enable_equip_check(oper, items[i]))
                items[i]->on_select = _disable_item;
        }
    for (; i < static_cast<int>(items.size()); i++)
        if (items[i]->level == MEL_ITEM)
            items[i]->set_enabled(!is_inventory);
    const string cycle_hint = make_stringf("<lightgray> (%s to select)</lightgray>",
            menu_keyhelp_cmd(CMD_MENU_CYCLE_HEADERS).c_str());

    // a `,` will trigger quick activation, rather than cycle headers
    const bool easy_floor = Options.easy_floor_use && item_floor.size() == 1
        && (is_inventory || !inv_header);
    if (inv_header)
    {
        inv_header->text = "Inventory Items";
        if (!is_inventory)
            inv_header->text += cycle_hint;
    }

    if (floor_header)
    {
        floor_header->text = "Floor Items";
        if (easy_floor)
        {
            floor_header->text += make_stringf(" (%s to %s)",
                    menu_keyhelp_cmd(CMD_MENU_CYCLE_HEADERS).c_str(),
                        _oper_name(oper).c_str());
        }
        else if (is_inventory)
            floor_header->text += cycle_hint;
    }

    update_menu(true);
}

void UseItemMenu::clear()
{
    item_inv.clear();
    item_floor.clear();
    inv_header = nullptr;
    floor_header = nullptr;
    last_inv_pos = -1;
    Menu::clear();
}

void UseItemMenu::toggle_inv_or_floor()
{
    if (item_inv.empty() || item_floor.empty())
        return;
    is_inventory = !is_inventory;
    update_sections();
}

// only two sections, and we want to skip inv menu subtitles, so it's simplest
// just to write custom code here
bool UseItemMenu::cycle_headers(bool)
{
    if (item_inv.empty() || item_floor.empty())
        return false;
    if (!is_set(MF_ARROWS_SELECT))
        toggle_inv_or_floor();
    else if (is_inventory)
        set_hovered(last_inv_pos + 1);
    else
        set_hovered(0);
    // XX this skips `unarmed`, should it?
    cycle_hover(); // hover is guaranteed to be a header, get to a selectable item
#ifdef USE_TILE_WEB
    // cycle_headers doesn't currently have a client-side
    // implementation, so force-send the server-side scroll
    webtiles_update_scroll_pos(true);
#endif
    return true;
}

void UseItemMenu::set_hovered(int hovered, bool force)
{
    // need to be a little bit careful about recursion potential here:
    // update_menu calls set_hovered to sanitize low level UI state.
    const bool skip_toggle = hovered == last_hovered;
    InvMenu::set_hovered(hovered, force);
    // keep inv vs floor in sync
    if (!skip_toggle && last_hovered >= 0
        && !item_floor.empty() && !item_inv.empty())
    {
        if (is_inventory && last_hovered > last_inv_pos
            || !is_inventory && last_hovered <= last_inv_pos)
        {
            toggle_inv_or_floor();
        }
    }
}

bool UseItemMenu::examine_index(int i)
{
    if (show_unarmed() && i == 0)
        return true; // no description implemented
    else if (is_inventory)
        return InvMenu::examine_index(i);
    else // floor item
    {
        auto desc_tgt = _entry_to_item(items[i]);
        ASSERT(desc_tgt);
        return describe_item(*desc_tgt, nullptr, false);
    }
}

string UseItemMenu::get_keyhelp(bool) const
{
    string r;

    // XX the logic here is getting convoluted, if only these were widgets...
    const string desc_key = is_set(MF_ARROWS_SELECT)
                                    ? "[<w>?</w>] describe selected" : "";
    string full;
    string eu_modes;
    string modes;
    // OPER_ANY menus are identify, enchant, brand; they disable most of these
    // key shortcuts
    if (oper != OPER_ANY)
    {
        if (_equip_oper(oper))
        {
            eu_modes = "[<w>tab</w>] ";
            eu_modes += (generalize_oper(oper) == OPER_EQUIP)
                ? "<w>equip</w>|unequip  " : "equip|<w>unequip</w>  ";
        }

        if (available_modes.size() > 1)
        {
            vector<string> mode_names;
            for (const auto &o : available_modes)
            {
                string n = _oper_name(o);
                if (o == oper)
                    n = "<w>" + n + "</w>";
                mode_names.push_back(n);
            }
            modes = menu_keyhelp_cmd(CMD_MENU_CYCLE_MODE) + " "
                + join_strings(mode_names.begin(), mode_names.end(), "|")
                + "   ";
        }
    }
    if (eu_modes.size() && desc_key.size() && modes.size() && full.size())
    {
        // too much stuff to fit in one row
        r = pad_more_with(full, desc_key) + "\n";
        r += pad_more_with(modes, eu_modes);
        return r;
    }
    else
    {
        // if both of these are shown, always keep them in a stable position
        if (eu_modes.size() && desc_key.size())
            r = pad_more_with("", desc_key);

        // annoying: I cannot get more height changes to work correctly. So as
        // a workaround, this will produce a two line more, potentially
        // with one blank line (except on menus where mode never changes).
        if (oper != OPER_ANY)
            r += "\n";
        r += modes + full;
        if (eu_modes.size() && desc_key.size())
            return pad_more_with(r, eu_modes); // desc_key on prev line
        else // at most one of these is shown
            return pad_more_with(r, desc_key.size() ? desc_key : eu_modes);
    }
}

bool UseItemMenu::process_key(int key)
{
    // TODO: should check inscriptions here
    if (isadigit(key)
        || key == '-' && show_unarmed())
    {
        lastch = key;
        return false;
    }

    if (key == '\\')
    {
        check_item_knowledge();
        return true;
    }
    else if (key == ','
        && Options.easy_floor_use && item_floor.size() == 1
        && (is_inventory || item_inv.empty()))
    {
        // handle easy_floor_use outside of the menu loop
        lastch = ','; // sanity only
        do_easy_floor = true;
        return false;
    }
    else if (key == CK_TAB && _equip_oper(oper))
    {
        item_type_filter = OSEL_ANY;
        save_hover();
        switch (oper)
        {
        case OPER_EQUIP:
        case OPER_WIELD:   reset(OPER_UNEQUIP); break;
        case OPER_WEAR:    reset(OPER_TAKEOFF); break;
        case OPER_PUTON:   reset(OPER_REMOVE); break;
        case OPER_UNEQUIP: reset(OPER_EQUIP); break;
        case OPER_REMOVE:  reset(OPER_PUTON); break;
        case OPER_TAKEOFF: reset(OPER_WEAR); break;
        default: break;
        }
        restore_hover(false);
        return true;
    }

    return Menu::process_key(key);
}

static operation_types _item_type_to_oper(object_class_type type)
{
    switch (type)
    {
        case OBJ_WANDS:
        case OBJ_TALISMANS:
        case OBJ_MISCELLANY: return OPER_EVOKE;
        case OBJ_POTIONS:    return OPER_QUAFF;
        case OBJ_SCROLLS:    return OPER_READ;
        case OBJ_ARMOUR:     return OPER_WEAR;
        case OBJ_WEAPONS:
        case OBJ_STAVES:     return OPER_WIELD;
        case OBJ_JEWELLERY:  return OPER_PUTON;
        default:             return OPER_NONE;
    }
}

static operation_types _item_type_to_remove_oper(object_class_type type)
{
    switch (type)
    {
    case OBJ_ARMOUR:    return OPER_TAKEOFF;
    case OBJ_WEAPONS:
    case OBJ_STAVES:    return OPER_WIELD;
    case OBJ_JEWELLERY: return OPER_REMOVE;
    default:            return OPER_NONE;
    }
}

static operation_types _item_to_oper(item_def *target)
{
    if (!target)
        return OPER_WIELD; // unwield
    else
        return _item_type_to_oper(target->base_type);
}

static operation_types _item_to_removal(item_def *target)
{
    if (!target)
        return OPER_NONE;
    else
        return _item_type_to_remove_oper(target->base_type);
}

static bool _equip_oper(operation_types oper)
{
    const auto gen = generalize_oper(oper);
    return gen == OPER_EQUIP || gen == OPER_UNEQUIP;
}

string item_equip_verb(const item_def& item)
{
    return _oper_name(_item_type_to_oper(item.base_type));
}

string item_unequip_verb(const item_def& item)
{
    const operation_types oper = _item_type_to_remove_oper(item.base_type);
    if (oper == OPER_WIELD)
        return "unwield";
    else
        return _oper_name(oper);
}

static bool _can_generically_use_armour(bool wear=true)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (you.has_mutation(MUT_NO_ARMOUR))
    {
        if (wear)
            mprf(MSGCH_PROMPT, "You can't wear anything.");
        else
        {
            mprf(MSGCH_PROMPT, "You can't remove your %s, sorry.",
                species::skin_name(you.species).c_str());
        }
        return false;
    }

    if (!form_can_wear())
    {
        mprf(MSGCH_PROMPT, "You can't %s anything in your present form.",
            wear ? "wear" : "remove");
        return false;
    }

    return true;
}

static bool _can_wield_anything()
{
    item_def dummy_weapon;
    dummy_weapon.base_type = OBJ_WEAPONS;
    string veto_reason;
    bool ret = can_equip_item(dummy_weapon, true, &veto_reason);
    if (!veto_reason.empty())
        mprf(MSGCH_PROMPT, "%s", veto_reason.c_str());

    return ret;
}

static bool _can_generically_use(operation_types oper)
{
    // check preconditions
    // XX should this be part of the menu?
    string err;
    switch (oper)
    {
    case OPER_QUAFF:
        err = cannot_drink_item_reason();
        break;
    case OPER_READ:
        err = cannot_read_item_reason();
        break;
    case OPER_EVOKE:
        err = cannot_evoke_item_reason();
        break;
    case OPER_WEAR:
        if (!_can_generically_use_armour())
            return false;
        break;
    case OPER_WIELD:
        if (!_can_wield_anything())
            return false;
        break;
    case OPER_REMOVE:
        // TODO: maybe add a worn jewellery check here?
        // intentional fallthrough
    case OPER_PUTON:
        if (you.berserk())
        {
            canned_msg(MSG_TOO_BERSERK);
            return false;
        }
        // can't differentiate between these two at this point
        if (!you_can_wear(SLOT_RING, true) && !you_can_wear(SLOT_AMULET, true))
        {
            mprf(MSGCH_PROMPT, "You can't %s jewellery%s.",
                oper == OPER_PUTON ? "wear" : "remove",
                you.has_mutation(MUT_NO_JEWELLERY) ? "" :  " in your present form");
            return false;
        }
        break;
    case OPER_TAKEOFF:
        if (!_can_generically_use_armour(false))
            return false;
        break;
    default:
        break;
    }

    if (!err.empty())
    {
        mprf(MSGCH_PROMPT, "%s", err.c_str());
        return false;
    }
    return true;
}

static bool _evoke_item(item_def &i)
{
    ASSERT(i.defined());
    if (i.base_type != OBJ_TALISMANS && !in_inventory(i))
    {
        mprf(MSGCH_PROMPT, "You aren't carrying that!");
        return false;
    }

    return evoke_item(i);
}

bool use_an_item(operation_types oper, item_def *target)
{
    if (!_can_generically_use(oper))
        return false;

    // if the player is wearing one piece of jewellery, under some circumstances
    // autoselect instead of prompt.
    if (!target && oper == OPER_REMOVE && !Options.jewellery_prompt)
    {
        vector<item_def*> jewellery = you.equipment.get_slot_items(SLOT_ALL_JEWELLERY);
        if (jewellery.size() == 1)
            target = jewellery[0];
    }

    if (!target)
        oper = use_an_item_menu(target, oper);

    if (oper == OPER_NONE)
        return false; // abort menu

    if (oper == OPER_EQUIP)
        oper = _item_to_oper(target);
    else if (oper == OPER_UNEQUIP)
    {
        oper = _item_to_removal(target);
        if (oper == OPER_WIELD)
            target = nullptr; // unwield
    }

    ASSERT(oper == OPER_WIELD || target);
    // now we have an item and a specific oper: what to do with the item?
    switch (oper)
    {
    case OPER_QUAFF:
        return drink(target);
    case OPER_READ:
        return read(target);
    case OPER_EVOKE:
        return _evoke_item(*target);
    case OPER_WIELD:
        if (!target)
            return _try_unwield_weapons();
        // Deliberate fallthrough
    case OPER_WEAR:
    case OPER_PUTON:
        return try_equip_item(*target);
    case OPER_REMOVE:
    case OPER_TAKEOFF: // fallthrough
        return try_unequip_item(*target);

    default:
        return false; // or ASSERT?
    }
}

/**
 * Helper function for try_equip_item
 * @param  item    item on floor (where the player is standing)
 * @param  quiet   print message or not
 * @return boolean can the player move the item into their inventory, or are
 *                 they out of space?
 */
static bool _can_move_item_from_floor_to_inv(const item_def &item)
{
    if (inv_count() < ENDOFPACK)
        return true;
    if (!is_stackable_item(item))
    {
        mpr("You can't carry that many items.");
        return false;
    }
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (items_stack(you.inv[i], item))
            return true;
    }
    mpr("You can't carry that many items.");
    return false;
}

/**
 * Helper function for try_equip_item
 * @param  to_get item on floor (where the player is standing) to move into
                  inventory
 * @return int -1 if failure due to already full inventory; otherwise, index in
 *             you.inv where the item ended up
 */
static int _move_item_from_floor_to_inv(const item_def &to_get)
{
    map<int,int> tmp_l_p = you.last_pickup; // if we need to restore
    you.last_pickup.clear();

    if (!move_item_to_inv(to_get.index(), to_get.quantity, true))
    {
        mpr("You can't carry that many items.");
        you.last_pickup = tmp_l_p;
    }
    // Get the slot of the last thing picked up
    // TODO: this is a bit hacky---perhaps it's worth making a function like
    // move_item_to_inv that returns the slot the item moved into
    else
    {
        ASSERT(you.last_pickup.size() == 1); // Sanity check...
        return you.last_pickup.begin()->first;
    }
    return -1;
}

/**
 * Helper function for try_equip_item
 * @param  item  item on floor (where the player is standing) or in inventory
 * @return ret index in you.inv where the item is (either because it was already
 *               there or just got moved there), or -1 if we tried and failed to
 *               move the item into inventory
 */
static int _get_item_slot_maybe_with_move(const item_def &item)
{
    int ret = item.pos == ITEM_IN_INVENTORY
        ? item.link : _move_item_from_floor_to_inv(item);
    return ret;
}

static char _item_swap_keys[] =
{'<', '>', '^', '1', '2', '3', '4', '5', '6', '7', '8'};

static item_def* _item_swap_menu(const vector<item_def*>& candidates)
{
    // Mark all items we want to display
    for (item_def* item : candidates)
        item->flags |= ISFLAG_MARKED_FOR_MENU;

    int ret = prompt_invent_item(
                "To do this, you must remove one of the following items:",
                menu_type::invlist, OSEL_MARKED_ITEMS, OPER_UNEQUIP,
                invprompt_flag::no_warning | invprompt_flag::hide_known);

    // Immediatey unmark all items
    for (item_def* item : candidates)
        item->flags &= ~ISFLAG_MARKED_FOR_MENU;

    // Cancelled:
    if (ret < 0)
    {
        canned_msg(MSG_OK);
        return nullptr;
    }

    // Now map the result of prompt_invent_item into an index into candidates
    // (which is what our calling function expects).
    for (size_t i = 0; i < candidates.size(); ++i)
        if (candidates[i]->link == ret)
            return candidates[i];

    return nullptr;
}

static item_def* _item_swap_prompt(const vector<item_def*>& candidates)
{
    // Default to a menu for larger choices
    if (candidates.size() > 3 || ui::has_layout())
        return _item_swap_menu(candidates);

    vector<char> slot_chars;
    for (auto item : candidates)
        slot_chars.push_back(index_to_letter(item->link));

    clear_messages();

    mprf(MSGCH_PROMPT,
         "To do this, you must remove one of the following items:");
    mprf(MSGCH_PROMPT, "(<w>?</w> for menu, <w>Esc</w> to cancel)");

    for (size_t i = 0; i < candidates.size(); i++)
    {
        string m = "<w>";
        const char key = _item_swap_keys[i];
        m += key;
        if (key == '<')
            m += '<';

        m += "</w> or " + candidates[i]->name(DESC_INVENTORY);
        mprf_nocap("%s", m.c_str());
    }
    flush_prev_message();

    int chosen = -1;
    mouse_control mc(MOUSE_MODE_PROMPT);
    int c;
    do
    {
        c = getchm();
        for (size_t i = 0; i < slot_chars.size(); i++)
        {
            if (c == slot_chars[i]
                || c == _item_swap_keys[i])
            {
                chosen = i;
                break;
            }
        }
    } while (!key_is_escape(c) && c != ' ' && c != '?' && chosen < 0);

    clear_messages();

    if (c == '?')
        return _item_swap_menu(candidates);
    else if (key_is_escape(c) || c == ' ')
        canned_msg(MSG_OK);

    if (chosen >= 0)
        return candidates[chosen];
    else
        return nullptr;
}

/**
 * Potentially prompt the player about a multitude of things related to changing
 * their current gear. This includes inscriptions, god disapproval, Drain^ and
 * similar mods, as well as losing flight over dangerous terrain
 *
 * @param to_remove  A list of items to be removed (which will be removed in
 *                   sequence from back to front, before any new item is equipped).
 * @param to_equip   The item slated to be equipped. May be nullptr if no item
 *                   is being equipped.
 *
 * @return True, if the changing gear should continue (either because there were
 *         no warnings, or the player chose to accept them). False, if we should
 *         abort the process.
 */
bool warn_about_changing_gear(const vector<item_def*>& to_remove, item_def* to_equip)
{
    // Switching to a launcher while berserk is likely a mistake.
    if (to_equip && you.berserk() && is_range_weapon(*to_equip))
    {
        string prompt = "You can't shoot while berserk! Really wield " +
                        to_equip->name(DESC_INVENTORY) + "?";
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    // First, check warnings for equipping this item
    if (to_equip && !check_warning_inscriptions(*to_equip, OPER_EQUIP))
    {
        canned_msg(MSG_OK);
        return false;
    }

    for (const item_def* item : to_remove)
    {
        if (!maybe_warn_about_removing(*item))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    // Check whether removing any of this sequence of items would cause us to
    // lose flight over dangerous terrain.
    if (is_feat_dangerous(env.grid(you.pos()), false, !you.permanent_flight(false))
        && (!you.duration[DUR_FLIGHT] || dur_expiring(DUR_FLIGHT)))
    {
        const int equipped_flight = you.equip_flight();
        int removed_flight = 0;
        for (item_def* item : to_remove)
        {
            if (item_grants_flight(*item))
            {
                if (++removed_flight >= equipped_flight)
                {
                    mprf(MSGCH_PROMPT, "Removing %s right now would cause you to %s!",
                            item->name(DESC_YOUR).c_str(),
                            env.grid(you.pos()) == DNGN_DEEP_WATER ? "drown" : "burn");
                    return false;
                }
            }
        }
    }

    return true;
}

bool try_equip_item(item_def& item)
{
    string reason;
    if (!can_equip_item(item, true, &reason))
    {
        mprf(MSGCH_PROMPT, "%s", reason.c_str());
        return false;
    }

    // If we're attempting to equip an item on the floor, test if we have room
    // to even pick it up, first
    if (item.pos != ITEM_IN_INVENTORY && !_can_move_item_from_floor_to_inv(item))
        return false;
    else if (item_is_equipped(item))
    {
        if (Options.equip_unequip)
            return try_unequip_item(item);
        else
            return false;
    }

    vector<vector<item_def*>> removal_candidates;  // Items eligible to be removed
    vector<item_def*> to_remove;                   // Queue of items chosen to remove
    equipment_slot slot = you.equipment.find_slot_to_equip_item(item, removal_candidates);

    // If there is nowhere to put it and nothing the player can do to change
    // this, abort immediately. (If a cursed item was the cause, a message was
    // already printed by find_slot_to_equip_item()
    if (slot == SLOT_UNUSED && removal_candidates.empty())
        return false;

    // Otherwise, maybe look into removing items to make room.
    if (!removal_candidates.empty())
    {
        for (size_t i = 0; i < removal_candidates.size(); ++i)
        {
            vector<item_def*>& candidates = removal_candidates[i];

            // If one of the candidates has already been removed to free
            // another slot, we don't need to remove anything to free this slot
            bool slot_freed_when_freeing_other_slot = false;
            for (const item_def* removed_item : to_remove)
            {
                for (const item_def* candidate : candidates)
                {
                    if (candidate == removed_item)
                    {
                        slot_freed_when_freeing_other_slot = true;
                        break;
                    }
                }
            }
            if (slot_freed_when_freeing_other_slot)
                continue;

            // Check if some number of items are inscribed with {=R} (but less
            // than all of them), and remove them from the candidates.
            int num_inscribed = 0;
            for (int j = candidates.size() - 1; j >= 0; --j)
            {
                if (strstr(candidates[j]->inscription.c_str(), "=R"))
                    ++num_inscribed;
            }
            if (num_inscribed > 0 && num_inscribed < (int)candidates.size())
            {
                for (int j = candidates.size() - 1; j >= 0; --j)
                {
                    if (strstr(candidates[j]->inscription.c_str(), "=R"))
                    {
                        candidates[j] = candidates.back();
                        candidates.pop_back();
                    }
                }
            }

            if (candidates.size() == 1)
            {
                if (item.base_type == OBJ_JEWELLERY && Options.jewellery_prompt)
                {
                    item_def* ret = _item_swap_prompt(candidates);

                    if (ret)
                        to_remove.push_back(ret);
                    else
                        return false;
                }
                else
                    to_remove.push_back(candidates[0]);
            }
            else
            {
                // Save which item was selected to remove.
                if (item_def* ret = _item_swap_prompt(candidates))
                    to_remove.push_back(ret);
                // User cancelled:
                else
                    return false;
            }

            // If find_slot_to_equip_item didn't give us a more specific slot,
            // the first chosen slot to remove must be the 'primary' slot to put
            // this item in, so save it.
            if (slot == SLOT_UNUSED)
                slot = you.equipment.find_compatible_occupied_slot(*to_remove.back(), item);

            // Handle trying to remove items that may themselves require other
            // items to be removed, due to losing slots (ie: Macabre Finger)
            if (!handle_chain_removal(to_remove, true))
                return false;
        }
    }

    // Now that we have determined it is *possible* to remove enough items to
    // equip this item, and the player has made any selections they wished,
    // let's test whether we need to give any warning prompts.
    //
    // Note: What we pass to this method may be a link to a temporary item copy
    //       in our inventory, if the item we are testing is currently on the
    //       foor.
    if (!warn_about_changing_gear(to_remove, &item))
        return false;

    // Now do the actual removal and equipping.

    // Pick up item, if we need to.
    item_def& real_item = you.inv[_get_item_slot_maybe_with_move(item)];
    do_equipment_change(&real_item, slot, to_remove);

    return true;
}

static bool _is_slow_equip(const item_def& item)
{
    if (item.base_type == OBJ_JEWELLERY)
        return jewellery_is_amulet(item.sub_type);
    else if (item.base_type == OBJ_ARMOUR)
        return true;
    else if (is_weapon(item))
        return you.has_mutation(MUT_SLOW_WIELD);

    // Probably nothing reaches this?
    return true;
}

/**
 * Handles the removal of items that are giving equipment slots which are
 * currently filled (and thus must also have something removed from them at
 * the same time). And those items removed may *also* be giving equipment slots,
 * etc, etc.
 *
  * @param to_remove[in/out] List of items selected to remove. Begins with the
  *                          last item on the list when passed it, and other
  *                          items that shoud be removed will be added to this
  *                          list.
 * @param interactive    Whether the player gets to select which item to remove
 *                       at each step (because they are manually removing
 *                       something). Otherwise, items are selected automatically
 *                       (generally to meld them).
 *
 * @return True if removal may proceed. False if removal is impossible (or the
 *         player chose to cancel).
 */
bool handle_chain_removal(vector<item_def*>& to_remove, bool interactive)
{
    if (to_remove.empty())
        return true;

    for (size_t n = 0; n < to_remove.size(); ++n)
    {
        item_def& item = *to_remove[n];
        if (!item_gives_equip_slots(item))
            continue;

        vector<item_def*> chain_remove;
        int chain_remove_num = you.equipment.needs_chain_removal(item, chain_remove, !interactive);
        if (chain_remove_num > 0)
        {
            if ((int)chain_remove.size() < chain_remove_num)
            {
                mprf(MSGCH_PROMPT, "A cursed item is preventing you from removing %s.",
                        item.name(DESC_INVENTORY).c_str());
                return false;
            }
            // We must remove all of the items found.
            else if ((int)chain_remove.size() == chain_remove_num)
            {
                for (item_def* _item : chain_remove)
                    to_remove.push_back(_item);
            }
            // We must remove some of the items found; let the player decide which.
            else if (interactive)
            {
                for (int i = 0; i < chain_remove_num;  ++i)
                {
                    if (item_def* ret = _item_swap_prompt(chain_remove))
                    {
                        to_remove.push_back(ret);

                        // Remove choice from list of choices, in case we need
                        // to make another choice.
                        for (size_t j = 0; j < chain_remove.size(); ++j)
                        {
                            if (chain_remove[j] == ret)
                            {
                                chain_remove[j] = chain_remove.back();
                                chain_remove.pop_back();
                                break;
                            }
                        }
                    }
                    // User cancelled:
                    else
                        return false;
                }
            }
            // Non-interactive: remove items from the end, as needed
            else
            {
                for (int i = 0; i < chain_remove_num;  ++i)
                    to_remove.push_back(chain_remove[chain_remove.size() - i - 1]);
            }
        }
        else
            break;
    }

    return true;
}

/**
 * Performs the actual changing of gear (or starting the delays to do so).
 *
 * Assumes that all items we've been given are valid to perform these operations
 * on.
 *
 * @param to_equip     An item to equip (possibly nullptr, if we're not putting
 *                     anything new on).
 * @param equip_slot   The slot to equip the new item in (if we're equipping one)
 * @param to_remove    A vector of items to remove (possibly empty, if we're not
 *                     removing anything).
 */
void do_equipment_change(item_def* to_equip, equipment_slot equip_slot,
                         vector<item_def*> to_remove)
{
    const bool is_multi = (to_equip != nullptr && !to_remove.empty())
                            || to_remove.size() > 1;
    bool all_fast = true;

    // Removals happen first
    if (!to_remove.empty())
    {
        // We remove items in reverse of the order they are selected, to account
        // for chain removals (ie: removing our third ring before removing the
        // Macabre Finger)
        for (int i = to_remove.size() - 1; i >= 0; --i)
        {
            item_def* item = to_remove[i];
            if (_is_slow_equip(*item))
            {
                start_delay<EquipOffDelay>(ARMOUR_EQUIP_DELAY, *item);
                all_fast = false;
            }
            // If this removal is queued after another removal, it needs to use
            // a delay. (This means it takes 10 aut instead of 5, but this should
            // matter so rarely that I'm not sure it's worth fixing?)
            else if (!all_fast)
                start_delay<EquipOffDelay>(1, *item);
            else
            {
                mprf("You %s %s.", item_unequip_verb(*item).c_str(),
                                   item->name(DESC_YOUR).c_str());
                unequip_item(*item);
            }
        }
    }

    if (to_equip)
    {
        if (_is_slow_equip(*to_equip))
        {
            start_delay<EquipOnDelay>(ARMOUR_EQUIP_DELAY, *to_equip, equip_slot);
            all_fast = false;
        }
        else if (!all_fast)
            start_delay<EquipOnDelay>(1, *to_equip, equip_slot);
        else
            equip_item(equip_slot, to_equip->link);
    }

    // If we did only a single fast equip action, it only takes half a turn.
    if (all_fast && !is_multi)
        you.time_taken /= 2;
    // If we did slow actions, no time should pass immediately (since time
    // passing will be handled by the delays).
    else if (!all_fast)
        you.time_taken = 0;

    you.turn_is_over = true;
}

bool try_unequip_item(item_def& item)
{
    if (item_is_melded(item))
    {
        mprf(MSGCH_PROMPT, "%s is melded into your body!",
                           item.name(DESC_YOUR).c_str());
        return false;
    }

    if (item.cursed())
    {
        mprf(MSGCH_PROMPT, "%s is stuck to your body!",
                            item.name(DESC_YOUR).c_str());
        return false;
    }

    if (is_unrandom_artefact(item, UNRAND_DEMON_AXE))
    {
        mprf(MSGCH_PROMPT, "Your thirst for blood prevents you from unwielding "
                           "your weapon!");
        return false;
    }

    if (you.duration[DUR_VAINGLORY] && is_unrandom_artefact(item, UNRAND_VAINGLORY))
    {
        mprf(MSGCH_PROMPT, "It would be unfitting for someone so glorious to "
                           "remove their crown in front of an audience.");
        return false;
    }

    vector<item_def*> to_remove = {&item};

    // Handle trying to remove items that may themselves require other
    // items to be removed, due to losing slots (ie: Macabre Finger)
    if (!handle_chain_removal(to_remove, true))
        return false;

    if (!warn_about_changing_gear(to_remove))
        return false;

    do_equipment_change(nullptr, SLOT_UNUSED, to_remove);

    return true;
}

// Attempt to unwield all weapons the player is currently wielding.
//
// Return true if we actually did so.
static bool _try_unwield_weapons()
{
    vector<item_def*> weapons = you.equipment.get_slot_items(SLOT_WEAPON);
    if (weapons.empty())
    {
        canned_msg(MSG_EMPTY_HANDED_ALREADY);
        return false;
    }

    for (item_def* item : weapons)
    {
        if (item->cursed())
        {
            mprf(MSGCH_PROMPT, "%s is stuck to your body!",
                                    item->name(DESC_YOUR).c_str());
            return false;
        }
    }

    if (!warn_about_changing_gear(weapons))
        return false;

    do_equipment_change(nullptr, SLOT_UNUSED, weapons);

    return true;
}

/**
 * Prompt use of an item from either player inventory or the floor.
 *
 * This function generates a menu containing type_expect items based on the
 * object_class_type to be acted on by another function. First it will list
 * items in inventory, then items on the floor. If the prompt is cancelled,
 * false is returned. If something is successfully chosen, then true is
 * returned, and at function exit the parameter target points to the object the
 * player chose or to nullptr if the player chose to wield bare hands (this is
 * only possible if oper is OPER_WIELD).
 *
 * @param target A pointer by reference to indicate the object selected.
 * @param item_type The object_class_type or OSEL_* of items to list.
 * @param oper The operation being done to the selected item.
 * @param prompt The prompt on the menu title
 * @param allowcancel If the user tries to cancel out of the prompt, run this
 *                    function. If it returns false, continue the prompt rather
 *                    than returning null.
 *
 * @return an operation to apply to the chosen item, OPER_NONE if the menu
 *         was aborted
 */
operation_types use_an_item_menu(item_def *&target, operation_types oper, int item_type,
                      const char* prompt, function<bool ()> allowcancel)
{
    UseItemMenu menu(oper, item_type, prompt);

    // First bail if there's nothing appropriate to choose in inv or on floor
    if (menu.empty_check())
        return OPER_NONE;

    bool choice_made = false;
    item_def *tmp_tgt = nullptr; // We'll change target only if the player
                                 // actually chooses

    // XX fully let the menu run its input loop
    // right now this exists because of the cancel check, used only for scrolls'
    // "really abort and waste a scroll" prompt.
    while (true)
    {
        vector<MenuEntry*> sel = menu.show(true);
        int keyin = menu.getkey();

        if (menu.do_easy_floor)
        {
            // handle an Options.easy_floor_use selection
            ASSERT(!menu.item_floor.empty());
            choice_made = true;
            tmp_tgt = const_cast<item_def*>(menu.item_floor[0]);
        }
        else if (isadigit(keyin))
        {
            // select by inscription
            tmp_tgt = digit_inscription_to_item(keyin, oper);
            if (tmp_tgt)
                choice_made = true;
        }
        else if (keyin == '-' && menu.show_unarmed())
        {
            choice_made = true;
            tmp_tgt = nullptr;
        }
        else if (!sel.empty())
        {
            ASSERT(sel.size() == 1);

            choice_made = true;
            tmp_tgt = _entry_to_item(sel[0]);
        }

        redraw_screen();
        update_screen();

        // equip cases are handled in followup calls
        // TODO: cleanup
        if (!_equip_oper(menu.oper)
            && choice_made && tmp_tgt
            && !check_warning_inscriptions(*tmp_tgt, menu.oper))
        {
            choice_made = false;
        }

        if (!choice_made)
        {
            if (!allowcancel())
                continue;
            prompt_failed(PROMPT_ABORT);
        }
        break;
    }

    if (choice_made)
        target = tmp_tgt;

    ASSERT(!choice_made || target || menu.show_unarmed());

    return choice_made ? menu.oper : OPER_NONE;
}

bool auto_wield()
{
    // Abort immediately if there's some condition that could prevent wielding
    // weapons.
    if (!_can_wield_anything())
        return false;

    item_def *to_wield = &you.inv[0]; // default is 'a'
    item_def *cur_wpn = you.weapon();

    if (to_wield == cur_wpn || !cur_wpn && !item_is_wieldable(*to_wield))
        to_wield = &you.inv[1];      // backup is 'b'

    // Don't try to equip a robe or something else that might be in this slot.
    if (!item_is_wieldable(*to_wield))
    {
        if (cur_wpn)
            try_unequip_item(*cur_wpn);
        else
            canned_msg(MSG_EMPTY_HANDED_ALREADY);
        return false;
    }

    return try_equip_item(*to_wield);
}

bool item_is_worn(int inv_slot)
{
    return item_is_equipped(you.inv[inv_slot]);
}

void prompt_inscribe_item()
{
    if (inv_count() < 1)
    {
        mpr("You don't have anything to inscribe.");
        return;
    }

    int item_slot = prompt_invent_item("Inscribe which item?",
                                       menu_type::invlist, OSEL_ANY);

    if (prompt_failed(item_slot))
        return;

    inscribe_item(you.inv[item_slot]);
}

bool has_drunken_brawl_targets()
{
    list<actor*> targets;
    get_cleave_targets(you, coord_def(), targets, -1, true);
    return !targets.empty();
}

// Perform a melee attack against every adjacent hostile target, and print a
// special message if there are any.
bool oni_drunken_swing()
{
    // Use the same logic for target-picking that cleaving does
    list<actor*> targets;
    get_cleave_targets(you, coord_def(), targets, -1, true);

    // Test that we have at least one valid non-prompting attack
    bool valid_swing = false;
    for (actor* victim : targets)
    {
        melee_attack attk(&you, victim);
        if (!attk.would_prompt_player())
            valid_swing = true;
    }

    if (!valid_swing)
        return false;

    if (!targets.empty())
    {
        if (you.weapon())
        {
            mprf("You take a swig of the potion and twirl %s.",
                 you.weapon()->name(DESC_YOUR).c_str());
        }
        else
            mpr("You take a swig of the potion and flex your muscles.");

        for (actor* victim : targets)
        {
            melee_attack attk(&you, victim);
            attk.never_cleave = true;

            if (!attk.would_prompt_player())
                attk.launch_attack_set();
        }

        return true;
    }

    return false;
}

bool drink(item_def* potion)
{
    string r = cannot_drink_item_reason(potion, true, true);
    if (!r.empty())
    {
        mpr(r);
        return false;
    }
    ASSERT(potion);

    const bool alreadyknown = item_type_known(*potion);

    if (alreadyknown && is_bad_item(*potion))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return false;
    }

    bool penance = god_hates_item(*potion);
    string prompt = make_stringf("Really quaff the %s?%s",
                                 potion->name(DESC_DBNAME).c_str(),
                                 penance ? " This action would place"
                                           " you under penance!" : "");
    if (alreadyknown && (is_dangerous_item(*potion, true) || penance)
        && Options.bad_item_prompt
        && !yesno(prompt.c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    const bool nearby_mons = there_are_monsters_nearby(true, true, false);
    if (you.unrand_equipped(UNRAND_VICTORY, true)
        && !you.props.exists(VICTORY_CONDUCT_KEY))
    {
        item_def *item = you.equipment.get_first_slot_item(SLOT_BODY_ARMOUR, true);
        string unrand_prompt = make_stringf("Really quaff with monsters nearby "
                                            "while wearing %s?",
                                            item->name(DESC_THE, false, true,
                                                       false).c_str());

        if (item->props[VICTORY_STAT_KEY].get_int() > 0
            && nearby_mons
            && !yesno(unrand_prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    // The "> 1" part is to reduce the amount of times that Xom is
    // stimulated when you are a low-level 1 trying your first unknown
    // potions on monsters.
    const bool dangerous = (player_in_a_dangerous_place()
                            && you.experience_level > 1);

    if (player_under_penance(GOD_GOZAG) && one_chance_in(3))
    {
        simple_god_message(" petitions for your drink to fail.", false,
                           GOD_GOZAG);
        you.turn_is_over = true;
        return false;
    }

    // Drunken master, swing!
    // We do this *before* actually drinking the potion for nicer messaging.
    if (you.has_mutation(MUT_DRUNKEN_BRAWLING)
        && oni_likes_potion(static_cast<potion_type>(potion->sub_type)))
    {
        oni_drunken_swing();
    }

    // Check for Delatra's gloves before potentially melding them.
    bool heal_on_id = you.unrand_equipped(UNRAND_DELATRAS_GLOVES);

    if (!quaff_potion(*potion))
        return false;

    if (!alreadyknown)
    {
        if (heal_on_id)
        {
            mpr("The energy of discovery flows from your fingertips!");
            potionlike_effect(POT_HEAL_WOUNDS, 40);
        }

        if (dangerous)
        {
            // Xom loves it when you drink an unknown potion and there is
            // a dangerous monster nearby...
            xom_is_stimulated(200);
        }
    }

    // Drinking with hostile visible mons nearby resets unrand "Victory" stats.
    if (you.unrand_equipped(UNRAND_VICTORY, true) && nearby_mons)
        you.props[VICTORY_CONDUCT_KEY] = true;

    // We'll need this later, after destroying the item.
    const bool was_exp = potion->sub_type == POT_EXPERIENCE;
    if (in_inventory(*potion))
    {
        dec_inv_item_quantity(potion->link, 1);
        auto_assign_item_slot(*potion);
    }
    else
        dec_mitm_item_quantity(potion->index(), 1);
    count_action(CACT_USE, OBJ_POTIONS);
    you.turn_is_over = true;

    // This got deferred from PotionExperience::effect to prevent SIGHUP abuse.
    if (was_exp)
        level_change();
    return true;
}

// XXX: there's probably a nicer way of doing this.
// Conducts, maybe?
bool god_hates_brand(const int brand)
{
    if (is_good_god(you.religion) && is_evil_brand(brand))
        return true;

    if (you_worship(GOD_ZIN) && is_chaotic_brand(brand))
        return true;

    if (you_worship(GOD_CHEIBRIADOS) && is_hasty_brand(brand))
        return true;

    if (you_worship(GOD_YREDELEMNUL) && brand == SPWPN_HOLY_WRATH)
        return true;

    return false;
}

static void _rebrand_weapon(item_def& wpn)
{
    const brand_type old_brand = get_weapon_brand(wpn);
    monster * spect = find_spectral_weapon(&you);
    if (&wpn == you.weapon() && old_brand == SPWPN_SPECTRAL && spect)
        end_spectral_weapon(spect, false);
    brand_type new_brand = old_brand;

    // now try and find an appropriate brand
    while (old_brand == new_brand || god_hates_brand(new_brand))
    {
        if (is_range_weapon(wpn))
        {
            new_brand = random_choose_weighted(3, SPWPN_FLAMING,
                                               3, SPWPN_FREEZING,
                                               3, SPWPN_DRAINING,
                                               3, SPWPN_HEAVY,
                                               1, SPWPN_ELECTROCUTION,
                                               1, SPWPN_CHAOS);
        }
        else
        {
            new_brand = random_choose_weighted(2, SPWPN_FLAMING,
                                               2, SPWPN_FREEZING,
                                               2, SPWPN_HEAVY,
                                               2, SPWPN_VENOM,
                                               2, SPWPN_PROTECTION,
                                               1, SPWPN_DRAINING,
                                               1, SPWPN_ELECTROCUTION,
                                               1, SPWPN_SPECTRAL,
                                               1, SPWPN_VAMPIRISM,
                                               1, SPWPN_CHAOS);
        }
    }

    set_item_ego_type(wpn, OBJ_WEAPONS, new_brand);
    convert2bad(wpn);
}

static string _item_name(item_def &item)
{
    return item.name(in_inventory(item) ? DESC_YOUR : DESC_THE);
}

static void _brand_weapon(item_def &wpn)
{
    you.wield_change = true;

    const string itname = _item_name(wpn);

    _rebrand_weapon(wpn);

    bool success = true;
    colour_t flash_colour = BLACK;

    switch (get_weapon_brand(wpn))
    {
    case SPWPN_HEAVY:
        flash_colour = BROWN;
        mprf("%s becomes incredibly heavy!",itname.c_str());
        break;

    case SPWPN_PROTECTION:
        flash_colour = YELLOW;
        mprf("%s projects an invisible shield of force!",itname.c_str());
        break;

    case SPWPN_FLAMING:
        flash_colour = RED;
        mprf("%s is engulfed in flames!", itname.c_str());
        break;

    case SPWPN_FREEZING:
        flash_colour = LIGHTCYAN;
        mprf("%s is covered with a thin layer of ice!", itname.c_str());
        break;

    case SPWPN_DRAINING:
        flash_colour = DARKGREY;
        mprf("%s craves living souls!", itname.c_str());
        break;

    case SPWPN_VAMPIRISM:
        flash_colour = DARKGREY;
        mprf("%s thirsts for the lives of mortals!", itname.c_str());
        break;

    case SPWPN_VENOM:
        flash_colour = GREEN;
        mprf("%s drips with poison.", itname.c_str());
        break;

    case SPWPN_ELECTROCUTION:
        flash_colour = LIGHTCYAN;
        mprf("%s crackles with electricity.", itname.c_str());
        break;

    case SPWPN_CHAOS:
        flash_colour = random_colour();
        mprf("%s erupts in a glittering mayhem of colour.", itname.c_str());
        break;

    case SPWPN_ACID:
        flash_colour = ETC_SLIME;
        mprf("%s oozes corrosive slime.", itname.c_str());
        break;

    case SPWPN_SPECTRAL:
        flash_colour = BLUE;
        mprf("%s acquires a faint afterimage.", itname.c_str());
        break;

    default:
        success = false;
        break;
    }

    if (success)
    {
        item_set_appearance(wpn);
        mprf_nocap("%s", wpn.name(DESC_INVENTORY_EQUIP).c_str());
        // Might be rebranding to/from protection or evasion.
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
        // Might be removing antimagic.
        calc_mp();
        flash_view_delay(UA_PLAYER, flash_colour, 300);
    }
    return;
}

static item_def* _choose_target_item_for_scroll(bool scroll_known, object_selector selector,
                                                const char* prompt)
{
    item_def *target = nullptr;

    auto success = use_an_item_menu(target, OPER_ANY, selector, prompt,
                       [=]()
                       {
                           if (scroll_known
                               || crawl_state.seen_hups
                               || yesno("Really abort (and waste the scroll)?", false, 0))
                           {
                               return true;
                           }
                           return false;
                       });
    return success != OPER_NONE ? target : nullptr;
}

static object_selector _enchant_selector(scroll_type scroll)
{
    if (scroll == SCR_BRAND_WEAPON)
        return OSEL_BRANDABLE_WEAPON;
    else if (scroll == SCR_ENCHANT_WEAPON)
        return OSEL_ENCHANTABLE_WEAPON;
    die("Invalid scroll type %d for _enchant_selector", (int)scroll);
}

// Returns nullptr if no weapon was chosen.
static item_def* _scroll_choose_weapon(bool alreadyknown, const string &pre_msg,
                                       scroll_type scroll)
{
    const bool branding = scroll == SCR_BRAND_WEAPON;

    item_def* target = _choose_target_item_for_scroll(alreadyknown, _enchant_selector(scroll),
                                                      branding ? "Brand which weapon?"
                                                               : "Enchant which weapon?");
    if (!target)
        return target;

    if (alreadyknown)
        mpr(pre_msg);

    return target;
}

// Returns true if the scroll is used up.
static bool _handle_brand_weapon(bool alreadyknown, const string &pre_msg)
{
    item_def* weapon = nullptr;
    string letter = "";
    if (!clua.callfn("c_choose_brand_weapon", ">s", &letter))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (isalpha(letter.c_str()[0]))
    {
        item_def &item = you.inv[letter_to_index(letter.c_str()[0])];
        if (item.defined() && is_brandable_weapon(item, true))
            weapon = &item;
    }

    if (!weapon)
        weapon = _scroll_choose_weapon(alreadyknown, pre_msg, SCR_BRAND_WEAPON);

    if (!weapon)
        return !alreadyknown;

    _brand_weapon(*weapon);
    return true;
}

bool enchant_weapon(item_def &wpn, bool quiet)
{
    bool success = false;

    // Get item name now before changing enchantment.
    string iname = _item_name(wpn);

    if (is_enchantable_weapon(wpn))
    {
        wpn.plus++;
        success = true;
        if (!quiet)
        {
            const char* dur = wpn.plus < MAX_WPN_ENCHANT ? "moment" : "while";
            mprf("%s glows red for a %s.", iname.c_str(), dur);
        }
    }

    if (!success && !quiet)
        canned_msg(MSG_NOTHING_HAPPENS);

    if (success)
        you.wield_change = true;

    return success;
}

/**
 * Prompt for an item to identify (either in the player's inventory or in
 * the ground), and then, if one is chosen, identify it.
 *
 * @param alreadyknown  Did we know that this was an ID scroll before we
 *                      started reading it?
 * @param pre_msg       'As you read the scroll of foo, it crumbles to dust.'
 * @param link[in,out]  The location of the ID scroll in the player's inventory
 *                      or, if it's on the floor, -1.
 *                      auto_assign_item_slot() may require us to update this.
 * @return  true if the scroll is used up. (That is, whether it was used or
 *          whether it was previously unknown (& thus uncancellable).)
 */
static bool _identify(bool alreadyknown, const string &pre_msg, int &link)
{
    item_def* itemp = nullptr;
    string letter = "";
    if (!clua.callfn("c_choose_identify", ">s", &letter))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (isalpha(letter.c_str()[0]))
    {
        item_def &item = you.inv[letter_to_index(letter.c_str()[0])];
        if (item.defined() && !item.is_identified())
            itemp = &item;
    }

    if (!itemp)
    {
        itemp = _choose_target_item_for_scroll(alreadyknown, OSEL_UNIDENT,
            "Identify which item? (\\ to view known items)");
    }

    if (!itemp)
        return !alreadyknown;

    item_def& item = *itemp;
    if (alreadyknown)
        mpr(pre_msg);

    identify_item(item);

    // Output identified item.
    mprf_nocap("%s", menu_colour_item_name(item, DESC_INVENTORY_EQUIP).c_str());
    if (in_inventory(item))
    {
        if (item.base_type == OBJ_WEAPONS && item_is_equipped(item))
            you.wield_change = true;

        const int target_link = item.link;
        item_def* moved_target = auto_assign_item_slot(item);
        if (moved_target != nullptr && moved_target->link == link)
        {
            // auto-swapped ID'd item with scrolls being used to ID it
            // correct input 'link' to the new location of the ID scroll stack
            // so that we decrement *it* instead of the ID'd item (10663)
            ASSERT(you.inv[target_link].defined());
            ASSERT(you.inv[target_link].is_type(OBJ_SCROLLS, SCR_IDENTIFY));
            link = target_link;
        }
    }
    return true;
}

static bool _handle_enchant_weapon(bool alreadyknown, const string &pre_msg)
{
    item_def* weapon = nullptr;
    string letter = "";
    if (!clua.callfn("c_choose_enchant_weapon", ">s", &letter))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (isalpha(letter.c_str()[0]))
    {
        item_def &item = you.inv[letter_to_index(letter.c_str()[0])];
        if (item.defined() && is_enchantable_weapon(item, true))
            weapon = &item;
    }

    if (!weapon)
    {
        weapon = _scroll_choose_weapon(alreadyknown, pre_msg,
                                       SCR_ENCHANT_WEAPON);
    }

    if (!weapon)
        return !alreadyknown;

    const bool success = enchant_weapon(*weapon, false);
    if (success && weapon->plus == MAX_WPN_ENCHANT)
    {
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
    }
    return true;
}

bool enchant_armour(item_def &arm, bool quiet)
{
    ASSERT(arm.defined());
    ASSERT(arm.base_type == OBJ_ARMOUR);

    // Cannot be enchanted.
    if (!is_enchantable_armour(arm))
    {
        if (!quiet)
            canned_msg(MSG_NOTHING_HAPPENS);
        return false;
    }

    string name = _item_name(arm);

    ++arm.plus;

    if (!quiet)
    {
        const bool plural = armour_is_hide(arm)
                            && arm.sub_type != ARM_TROLL_LEATHER_ARMOUR;
        string glow = conjugate_verb("glow", plural);
        const char* dur = is_enchantable_armour(arm) ? "moment" : "while";
        mprf("%s %s green for a %s.", name.c_str(), glow.c_str(), dur);
    }

    return true;
}

/// Returns whether the scroll is used up.
static bool _handle_enchant_armour(bool alreadyknown, const string &pre_msg)
{
    item_def* target= nullptr;
    string letter = "";
    if (!clua.callfn("c_choose_enchant_armour", ">s", &letter))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (isalpha(letter.c_str()[0]))
    {
        item_def &item = you.inv[letter_to_index(letter.c_str()[0])];
        if (item.defined() && is_enchantable_armour(item, true))
            target = &item;
    }

    if (!target)
    {
        target = _choose_target_item_for_scroll(alreadyknown,
            OSEL_ENCHANTABLE_ARMOUR, "Enchant which item?");
    }

    if (!target)
        return !alreadyknown;

    // Okay, we may actually (attempt to) enchant something.
    if (alreadyknown)
        mpr(pre_msg);

    const bool success = enchant_armour(*target, false);
    if (!success)
        return true;

    you.redraw_armour_class = true;
    if (!is_enchantable_armour(*target))
    {
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
    }

    return true;
}

static void _vulnerability_scroll()
{
    const int dur = 30 + random2(20);

    // Go over all creatures in LOS.
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            mon->strip_willpower(&you, dur, true);

            // Annoying but not enough to turn friendlies against you.
            if (!mon->wont_attack())
                behaviour_event(mon, ME_ANNOY, &you);
        }
    }

    you.strip_willpower(&you, dur, true);
    mpr("A wave of despondency washes over your surroundings.");
}

static bool _is_cancellable_scroll(scroll_type scroll)
{
    return scroll == SCR_IDENTIFY
           || scroll == SCR_BLINKING
           || scroll == SCR_ENCHANT_ARMOUR
           || scroll == SCR_AMNESIA
           || scroll == SCR_BRAND_WEAPON
           || scroll == SCR_ENCHANT_WEAPON
           || scroll == SCR_ACQUIREMENT
           || scroll == SCR_POISON;
}

/**
 * Check if a particular scroll type would hurt a monster.
 *
 * @param scr           Scroll type in question
 * @param m             Actor as a potential victim to the scroll
 * @return  true if the provided scroll type is harmful to the actor.
 */
static bool _scroll_will_harm(const scroll_type scr, const actor &m)
{
    return m.alive() && scr == SCR_TORMENT
        && !m.res_torment()
        && !never_harm_monster(&you, m.as_monster());
}

static vector<string> _desc_finite_wl(const monster_info& mi)
{
    vector<string> r;
    const int wl = mi.willpower();
    if (wl == WILL_INVULN)
        r.push_back("infinite will");
    else
        r.push_back("susceptible");
    return r;
}

static vector<string> _desc_res_torment(const monster_info& mi)
{
    if (mi.resists() & (MR_RES_TORMENT))
        return { "not susceptible" };
    else
        return { "susceptible" };
}

class targeter_finite_will : public targeter_multimonster
{
public:
    targeter_finite_will() : targeter_multimonster(&you)
    { }

    bool affects_monster(const monster_info& mon)
    {
        return mon.willpower() != WILL_INVULN;
    }
};

class targeter_torment : public targeter_multimonster
{
public:
    targeter_torment() : targeter_multimonster(&you)
    { }

    bool affects_monster(const monster_info& mon)
    {
        // TODO: if this is ever used for the pain card it will need an
        // override for the player in `is_affected`
        return !bool(mon.resists() & MR_RES_TORMENT);
    }
};

class targeter_silence : public targeter_maybe_radius
{
    targeter_radius inner_rad;
public:
    targeter_silence(int r1, int r2)
        : targeter_maybe_radius(&you, LOS_DEFAULT, r2),
          inner_rad(&you, LOS_DEFAULT, r1)
    {
    }

    aff_type is_affected(coord_def loc) override
    {
        if (inner_rad.is_affected(loc) == AFF_YES)
            return AFF_YES;
        else
            return targeter_maybe_radius::is_affected(loc);
    }
};

class targeter_poison_scroll : public targeter_radius
{
public:
    targeter_poison_scroll();
    aff_type is_affected(coord_def loc) override;
};

targeter_poison_scroll::targeter_poison_scroll()
    : targeter_radius(&you, LOS_NO_TRANS)
{ }

aff_type targeter_poison_scroll::is_affected(coord_def loc)
{
    const aff_type base_aff = targeter_radius::is_affected(loc);
    if (base_aff == AFF_NO)
        return AFF_NO;
    if (cell_is_solid(loc) || cloud_type_at(loc) != CLOUD_NONE)
        return AFF_NO;
    const actor* act = actor_at(loc);
    if (act != nullptr && you.can_see(*act))
        return AFF_NO;
    return AFF_YES;
}

// TODO: why do I have to do this
class scroll_targeting_behaviour : public targeting_behaviour
{
public:
    scroll_targeting_behaviour() : targeting_behaviour(false)
    {
    }

    bool targeted() override
    {
        return false;
    }
};

static unique_ptr<targeter> _get_scroll_targeter(scroll_type which_scroll)
{
    // blinking handled elsewhere
    switch (which_scroll)
    {
    case SCR_FEAR:
        return find_spell_targeter(SPELL_CAUSE_FEAR, 1000, LOS_RADIUS);
    case SCR_BUTTERFLIES: // close enough...
    case SCR_SUMMONING:
        // TODO: shadow creatures targeter doesn't handle band placement very
        // well, and this is more obvious with the scroll
        return find_spell_targeter(SPELL_SHADOW_CREATURES, 1000, LOS_RADIUS);
    case SCR_VULNERABILITY:
    case SCR_IMMOLATION:
        return make_unique<targeter_finite_will>();
    case SCR_SILENCE:
        return make_unique<targeter_silence>(2, 4); // TODO: calculate from power (or simplify the calc)
    case SCR_TORMENT:
        return make_unique<targeter_torment>();
    case SCR_POISON:
        return make_unique<targeter_poison_scroll>();
    default:
        return nullptr;
    }
}

static bool _scroll_targeting_check(scroll_type scroll, dist *target)
{
    // TODO: restructure so that spell scrolls call your_spells, and targeting
    // is handled automatically for such scrolls
    if (target && target->needs_targeting())
    {
        direction_chooser_args args;
        unique_ptr<targeter> hitfunc = _get_scroll_targeter(scroll);
        if (!hitfunc)
            return true; // sanity check
        if (scroll == SCR_FEAR)
        {
            // TODO: can't these be rolled in with the targeter somehow? Will
            // a fear (etc) targeter ever not have this?
            args.get_desc_func = targeter_addl_desc(SPELL_CAUSE_FEAR, 1000,
                get_spell_flags(SPELL_CAUSE_FEAR), hitfunc.get());
        }
        else if (scroll == SCR_VULNERABILITY || scroll == SCR_IMMOLATION)
            args.get_desc_func = _desc_finite_wl;
        else if (scroll == SCR_TORMENT)
            args.get_desc_func = _desc_res_torment;

        args.mode = TARG_ANY;
        args.self = confirm_prompt_type::cancel;
        args.hitfunc = hitfunc.get();
        scroll_targeting_behaviour beh;
        args.behaviour = &beh;

        direction(*target, args);
        return target->isValid && !target->isCancel;
    }
    else
        return true;
}

bool scroll_has_targeter(scroll_type which_scroll)
{
    switch (which_scroll)
    {
    case SCR_BLINKING:
    case SCR_FEAR:
    case SCR_SUMMONING:
    case SCR_BUTTERFLIES:
    case SCR_VULNERABILITY:
    case SCR_IMMOLATION:
    case SCR_POISON:
    case SCR_SILENCE:
    case SCR_TORMENT:
        return true;
    default:
        return false;
    }
}

// If needed, check whether there are hostiles in range that would be affected.
// True means that the scroll either doesn't have such a check, or that there
// are relevant enemies in range. False means that the check failed.
bool scroll_hostile_check(scroll_type which_scroll)
{
    // cf spell_no_hostile_in_range
    if (!in_bounds(you.pos()) || !you.on_current_level)
        return false;

    // no hostile check
    if (which_scroll == SCR_SUMMONING
        || which_scroll == SCR_BLINKING
        || which_scroll == SCR_BUTTERFLIES)
    {
        return true;
    }

    unique_ptr<targeter> hitfunc = _get_scroll_targeter(which_scroll);
    if (!hitfunc)
        return true; // no checks for these

    // all scrolls that have a targeter are los radius
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS, true);
         ri; ++ri)
    {
        const monster* mon = monster_at(*ri);
        if (!mon
            || !mon->visible_to(&you)
            // Plants/fungi don't count.
            || (!mons_is_threatening(*mon) || mon->wont_attack())
                && !mons_class_is_test(mon->type))
        {
            continue;
        }

        if (which_scroll == SCR_POISON)
        {
            monster_info mi(mon);
            if (get_resist(mi.mresists, MR_RES_POISON) <= 0)
                return true;
        }

        if (hitfunc->valid_aim(*ri) && hitfunc->is_affected(*ri) != AFF_NO)
            return true;
    }
    // nothing found: check fails
    return false;
}

/**
 * Read the provided scroll.
 *
 * Check to see if the player can read the provided scroll, and if so,
 * reads it. If no scroll is provided, prompt for a scroll.
 *
 * @param scroll The scroll to be read.
 * @param target targeting information, used by the quiver API
 */
bool read(item_def* scroll, dist *target)
{
    string failure_reason = cannot_read_item_reason(scroll);
    if (!failure_reason.empty())
    {
        mpr(failure_reason);
        return false;
    }
    ASSERT(scroll);

    const scroll_type which_scroll = static_cast<scroll_type>(scroll->sub_type);
    // Handle player cancels before we waste time
    if (item_type_known(*scroll))
    {
        const bool hostile_check = scroll_hostile_check(which_scroll);
        bool penance = god_hates_item(*scroll);
        string verb_object = "read the " + scroll->name(DESC_DBNAME);

        string penance_prompt = make_stringf("Really %s? This action would"
                                             " place you under penance%s!",
                                             verb_object.c_str(),
                                             hostile_check ? ""
                    : " and you can't even see any enemies this would affect");

        targeter_radius hitfunc(&you, LOS_NO_TRANS);

        const bool bad_item = (is_dangerous_item(*scroll, true)
                                    || is_bad_item(*scroll))
                            && Options.bad_item_prompt;

        if (stop_attack_prompt(hitfunc, verb_object.c_str(),
                               [which_scroll] (const actor* m)
                               {
                                   return _scroll_will_harm(which_scroll, *m);
                               },
                               nullptr, nullptr))
        {
            return false;
        }
        else if (penance && !yesno(penance_prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
        else if (bad_item
                 && !yesno(make_stringf("Really %s?%s",
                                        verb_object.c_str(),
                                        hostile_check ? ""
                        : " You can't even see any enemies this would affect."
                                        ).c_str(),
                           false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
        else if (!bad_item && !hostile_check && !yesno(make_stringf(
            "You can't see any enemies this would affect, really %s?",
                                        verb_object.c_str()).c_str(),
                                                false, 'n'))
        {
            // is this too nanny dev?
            canned_msg(MSG_OK);
            return false;
        }
    }

    const bool nearby_mons = there_are_monsters_nearby(true, true, false);
    if (you.unrand_equipped(UNRAND_VICTORY, true)
        && !you.props.exists(VICTORY_CONDUCT_KEY))
    {
        item_def *item = you.equipment.get_first_slot_item(SLOT_BODY_ARMOUR, true);
        string unrand_prompt = make_stringf("Really read with monsters nearby "
                                            "while wearing %s?",
                                            item->name(DESC_THE, false, true,
                                                       false).c_str());

        if (item->props[VICTORY_STAT_KEY].get_int() > 0
            && nearby_mons
            && !yesno(unrand_prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    // Ok - now we FINALLY get to read a scroll !!! {dlb}
    you.turn_is_over = true;

    const int prev_quantity = scroll->quantity;
    int link = in_inventory(*scroll) ? scroll->link : -1;
    const bool alreadyknown = item_type_known(*scroll);

    if (alreadyknown
        && scroll_has_targeter(which_scroll)
        && which_scroll != SCR_BLINKING // blinking calls its own targeter
        && !_scroll_targeting_check(which_scroll, target))
    {
        // a targeter can't be used for unid'd or uncancellable scrolls, so
        // we can skip the rest of the function
        you.turn_is_over = false;
        return false;
    }

    // For cancellable scrolls leave printing this message to their
    // respective functions.
    const string pre_succ_msg =
            make_stringf("As you read the %s, it %s.",
                          scroll->name(DESC_QUALNAME).c_str(),
                         which_scroll == SCR_FOG ? "dissolves into smoke" : "crumbles to dust");
    if (!_is_cancellable_scroll(which_scroll))
    {
        mpr(pre_succ_msg);
        // Actual removal of scroll done afterwards. -- bwr
    }

    const bool dangerous = player_in_a_dangerous_place();

    // ... but some scrolls may still be cancelled afterwards.
    bool cancel_scroll = false;
    bool bad_effect = false; // for Xom: result is bad (or at least dangerous)

    switch (which_scroll)
    {
    case SCR_BLINKING:
    {
        const string reason = you.no_tele_reason(true);
        if (!reason.empty())
        {
            mpr(pre_succ_msg);
            mpr(reason);
            break;
        }

        cancel_scroll = (controlled_blink(alreadyknown, target)
                         == spret::abort) && alreadyknown;

        if (!cancel_scroll)
            mpr(pre_succ_msg); // ordering is iffy but w/e
    }
        break;

    case SCR_TELEPORTATION:
        you_teleport();
        break;

    case SCR_ACQUIREMENT:
        if (!alreadyknown)
            mpr("This is a scroll of acquirement!");

        // included in default force_more_message
        // Identify it early in case the player checks the '\' screen.
        identify_item(*scroll);

        if (feat_eliminates_items(env.grid(you.pos())))
        {
            mpr("Anything you acquired here would fall and be lost!");
            cancel_scroll = true;
            break;
        }

        cancel_scroll = !acquirement_menu();
        break;

    case SCR_FEAR:
        mpr("You assume a fearsome visage.");
        mass_enchantment(ENCH_FEAR, 1000);
        break;

    case SCR_NOISE:
        noisy(25, you.pos(), "You hear a loud clanging noise!");
        break;

    case SCR_SUMMONING:
        cancel_scroll = summon_shadow_creatures() == spret::abort
                        && alreadyknown;
        break;

    case SCR_BUTTERFLIES:
        cancel_scroll = summon_butterflies() == spret::abort && alreadyknown;
        break;

    case SCR_FOG:
    {
        auto smoke = random_smoke_type();
        big_cloud(smoke, &you, you.pos(), 50, 8 + random2(8));
        break;
    }

    case SCR_REVELATION:
        magic_mapping(GDM, 100, false, false, false, false, false);
        you.duration[DUR_REVELATION] = you.time_taken + 1;
        break;

    case SCR_TORMENT:
        torment(&you, TORMENT_SCROLL, you.pos());

        // This is only naughty if you know you're doing it.
        did_god_conduct(DID_EVIL, 10, item_type_known(*scroll));
        bad_effect = !you.res_torment();
        break;

    case SCR_IMMOLATION:
    {
        bool had_effect = false;
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (mons_invuln_will(**mi))
                continue;

            // Jivya is a killjoy when it comes to slime bombing.
            if (have_passive(passive_t::neutral_slimes) && mons_is_slime(**mi))
                continue;

            if (mi->add_ench(mon_enchant(ENCH_INNER_FLAME, 0, &you)))
                had_effect = true;
        }

        if (had_effect)
            mpr("The creatures around you are filled with an inner flame!");
        else
            mpr("The air around you briefly surges with heat, but it dissipates.");

        bad_effect = true;
        break;
    }

    case SCR_POISON:
    {
        const spret result = scroll_of_poison(!alreadyknown);
        cancel_scroll = result == spret::abort;
        if (!cancel_scroll)
            mpr(pre_succ_msg);
        // amusing to Xom, at least
        bad_effect = result == spret::success && !player_res_poison();
        break;
    }

    case SCR_ENCHANT_WEAPON:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of enchant weapon.");
            // included in default force_more_message (to show it before menu)
        }

        cancel_scroll = !_handle_enchant_weapon(alreadyknown, pre_succ_msg);
        break;

    case SCR_BRAND_WEAPON:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of brand weapon.");
            // included in default force_more_message (to show it before menu)
        }

        cancel_scroll = !_handle_brand_weapon(alreadyknown, pre_succ_msg);
        break;

    case SCR_IDENTIFY:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of identify.");
            // included in default force_more_message (to show it before menu)
            // Do this here so it doesn't turn up in the ID menu.
            identify_item(*scroll);
        }
        {
            const int old_link = link;
            cancel_scroll = !_identify(alreadyknown, pre_succ_msg, link);
            // If we auto-swapped the ID'd item with the stack of ID scrolls,
            // update the pointer to the new place in inventory for the ?ID.
            if (link != old_link)
                scroll = &you.inv[link];
        }
        break;

    case SCR_ENCHANT_ARMOUR:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of enchant armour.");
            // included in default force_more_message (to show it before menu)
        }
        cancel_scroll = !_handle_enchant_armour(alreadyknown, pre_succ_msg);
        break;
#if TAG_MAJOR_VERSION == 34
    case SCR_CURSE_WEAPON:
    case SCR_CURSE_ARMOUR:
    case SCR_CURSE_JEWELLERY:
    case SCR_RECHARGING:
    case SCR_RANDOM_USELESSNESS:
    case SCR_HOLY_WORD:
    {
        mpr("This item has been removed, sorry!");
        cancel_scroll = true;
        break;
    }
#endif

    case SCR_SILENCE:
        cast_silence(30);
        break;

    case SCR_VULNERABILITY:
        _vulnerability_scroll();
        break;

    case SCR_AMNESIA:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of amnesia.");
            // included in default force_more_message (to show it before menu)
        }
        if (you.spell_no == 0 || you.has_mutation(MUT_INNATE_CASTER))
        {
            mpr("You feel forgetful for a moment.");
            break;
        }
        bool done;
        bool aborted;
        do
        {
            aborted = cast_selective_amnesia() == -1;
            done = !aborted
                   || alreadyknown
                   || crawl_state.seen_hups
                   || yesno("Really abort (and waste the scroll)?",
                            false, 0);
            cancel_scroll = aborted && alreadyknown;
        } while (!done);
        if (aborted)
            canned_msg(MSG_OK);
        break;

    default:
        mpr("Read a buggy scroll, please report this.");
        break;
    }

    if (cancel_scroll)
        you.turn_is_over = false;

    identify_item(*scroll);

    string scroll_name = scroll->name(DESC_QUALNAME);

    if (!cancel_scroll)
    {
        if (in_inventory(*scroll))
            dec_inv_item_quantity(link, 1);
        else
            dec_mitm_item_quantity(scroll->index(), 1);

        count_action(CACT_USE, OBJ_SCROLLS);
    }

    if (!alreadyknown
        && which_scroll != SCR_BRAND_WEAPON
        && which_scroll != SCR_ENCHANT_WEAPON
        && which_scroll != SCR_IDENTIFY
        && which_scroll != SCR_ENCHANT_ARMOUR
        && which_scroll != SCR_AMNESIA
        && which_scroll != SCR_ACQUIREMENT)
    {
        mprf("It %s %s.",
             scroll->quantity < prev_quantity ? "was" : "is",
             article_a(scroll_name).c_str());
    }

    if (!alreadyknown)
    {
        if (you.unrand_equipped(UNRAND_DELATRAS_GLOVES))
        {
            mpr("The energy of discovery flows from your fingertips!");
            potionlike_effect(POT_MAGIC, 40);
        }

        if (dangerous)
        {
            // Xom loves it when you read an unknown scroll and there is a
            // dangerous monster nearby... (though not as much as potions
            // since there are no *really* bad scrolls, merely useless ones).
            xom_is_stimulated(bad_effect ? 100 : 50);
        }
    }

    // Reading with hostile visible mons nearby resets unrand "Victory" stats.
    if (you.unrand_equipped(UNRAND_VICTORY, true)
        && nearby_mons
        && !cancel_scroll)
    {
        you.props[VICTORY_CONDUCT_KEY] = true;
    }

    if (!alreadyknown)
        auto_assign_item_slot(*scroll);
    return true;
}

#ifdef USE_TILE
// Interactive menu for item drop/use.

void tile_item_pickup(int idx, bool part)
{
    if (item_is_stationary(env.item[idx]))
    {
        mpr("You can't pick that up.");
        return;
    }

    int quantity = env.item[idx].quantity;
    if (part && quantity > 1)
    {
        quantity = prompt_for_int("Pick up how many? ", true);
        if (quantity < 1)
        {
            canned_msg(MSG_OK);
            return;
        }
        if (quantity > env.item[idx].quantity)
            quantity = env.item[idx].quantity;
    }
    pickup_single_item(idx, quantity);
}

void tile_item_drop(int idx, bool partdrop)
{
    if (!check_warning_inscriptions(you.inv[idx], OPER_DROP))
    {
        canned_msg(MSG_OK);
        return;
    }
    int quantity = you.inv[idx].quantity;
    if (partdrop && quantity > 1)
    {
        quantity = prompt_for_int("Drop how many? ", true);
        if (quantity < 1)
        {
            canned_msg(MSG_OK);
            return;
        }
        if (quantity > you.inv[idx].quantity)
            quantity = you.inv[idx].quantity;
    }
    drop_item(idx, quantity);
}

void tile_item_use(int idx)
{
    item_def &item = you.inv[idx];

    // Equipped?
    const bool equipped = item_is_equipped(item);

    if (item_ever_evokable(item))
    {
        if (check_warning_inscriptions(item, OPER_EVOKE))
            evoke_item(item);
        return;
    }

    const int type = item.base_type;

    // Use it
    switch (type)
    {

    case OBJ_MISSILES:
        if (check_warning_inscriptions(item, OPER_FIRE))
            quiver::slot_to_action(idx)->trigger(); // TODO: anything more interesting?
        return;

    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
        if (equipped)
            try_unequip_item(item);
        else
            try_equip_item(item);
        return;

    case OBJ_SCROLLS:
        if (check_warning_inscriptions(item, OPER_READ))
            read(&you.inv[idx]);
        return;

    case OBJ_POTIONS:
        if (check_warning_inscriptions(item, OPER_QUAFF))
            drink(&you.inv[idx]);
        return;

    default:
        return;
    }
}
#endif
