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
#include "spl-wpnench.h"
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

// The menu class for using items from either inv or floor.
// Derivative of InvMenu

class UseItemMenu : public InvMenu
{
    bool init_modes();
    bool populate_list(bool check_only=false);
    void populate_menu();
    bool process_key(int key) override;
    void update_sections();
    void clear() override;
    bool examine_index(int i) override;
    bool cycle_mode(bool forward) override;
    string get_keyhelp(bool scrollable) const override;

    bool skip_process_command(int keyin) override
    {
        // override superclass behavior for everything except id menu
        if (keyin == '!' && item_type_filter != OSEL_UNIDENT)
            return false;
        return InvMenu::skip_process_command(keyin);
    }

public:
    UseItemMenu(operation_types oper, int selector, const char* prompt);

    bool display_all;
    bool is_inventory;
    int item_type_filter;
    operation_types oper;

    int last_inv_pos;

    // XX these probably shouldn't be const...
    vector<const item_def*> item_inv;
    vector<const item_def*> item_floor;

    void toggle_display_all();
    void toggle_inv_or_floor();
    void set_hovered(int hovered, bool force=false) override;
    bool cycle_headers(bool=true) override;

    bool empty_check() const;

private:
    MenuEntry *inv_header;
    MenuEntry *floor_header;
    vector<operation_types> available_modes;
};

static string _default_use_title(operation_types oper)
{
    switch (oper)
    {
    case OPER_WIELD:
        return "Wield which item (- for none)?";
    case OPER_WEAR:
        return "Wear which item?";
    case OPER_PUTON:
        return "Put on which piece of jewellery?";
    case OPER_QUAFF:
        if (you.has_mutation(MUT_LONG_TONGUE))
            return "Slurp which item?";
        return "Drink which item?";
    case OPER_READ:
        return "Read which item?";
    default:
        return "buggy";
    }
}

static string _oper_name(operation_types oper)
{
    switch (oper)
    {
    case OPER_WIELD: return "wield";
    case OPER_WEAR:  return "wear";
    case OPER_PUTON: return "put on";
    case OPER_QUAFF: return "quaff";
    case OPER_READ:  return "read";
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
    default:
        return OSEL_ANY; // buggy?
    }
}

static vector<operation_types> _oper_to_mode(operation_types o)
{
    static const vector<operation_types> wearables = {OPER_WIELD, OPER_WEAR, OPER_PUTON};
    static const vector<operation_types> usables = {OPER_READ, OPER_QUAFF};

    // return a copy
    vector<operation_types> result;
    if (find(wearables.begin(), wearables.end(), o) != wearables.end())
        result = wearables;
    else if (find(usables.begin(), usables.end(), o) != usables.end())
        result = usables;

    return result;
}

bool UseItemMenu::init_modes()
{
    available_modes = _oper_to_mode(oper);
    if (available_modes.empty())
        return false;

    unwind_var<operation_types> cur_oper(oper);
    unwind_var<int> cur_filter(item_type_filter);

    erase_if(available_modes, [this](operation_types o) {
        oper = o;
        item_type_filter = _default_osel(oper);
        return !populate_list(true);
    });

    return available_modes.size() > 0;
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

    auto starting_hover = last_hovered;
    item_type_filter = _default_osel(oper);

    clear();
    populate_list();
    populate_menu();
    update_sections();
    set_title(_default_use_title(oper));
    set_hovered(starting_hover);
    // fixup hover in case headings have moved
    if (last_hovered >= 0 && items[last_hovered]->level != MEL_ITEM)
        if (is_set(MF_ARROWS_SELECT))
            cycle_hover();
        else
            set_hovered(-1);
    return true;
}

UseItemMenu::UseItemMenu(operation_types _oper, int item_type=OSEL_ANY,
                                    const char* prompt=nullptr)
    : InvMenu(MF_SINGLESELECT | MF_ARROWS_SELECT | MF_INIT_HOVER | MF_ALLOW_FORMATTING),
                            display_all(false), is_inventory(true),
      item_type_filter(item_type), oper(_oper), last_inv_pos(-1),
      inv_header(nullptr), floor_header(nullptr)
{
    set_tag("use_item");
    menu_action = ACT_EXECUTE;
    if (prompt)
        set_title(prompt);
    else
        set_title(_default_use_title(oper));
    // see `item_is_selected` for more on what can be used for item_type.
    if (item_type_filter == OSEL_ANY)
        item_type_filter = _default_osel(oper);

    init_modes();
    populate_list();
    populate_menu();
    // start hover on wielded item, if there is one, otherwise on -
    if (oper == OPER_WIELD && item_inv.size() > 0 && you.weapon())
        set_hovered(1);
}

bool UseItemMenu::populate_list(bool check_only)
{
    if (check_only && oper == OPER_WIELD)
        return true; // any char can go unarmed

    // Load inv items first
    for (const auto &item : you.inv)
    {
        if (item.defined()
            && (display_all || item_is_selected(item, item_type_filter)))
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
        if (!it->defined() || item_is_stationary(*it) || item_is_orb(*it)
            || item_is_spellbook(*it) || it->base_type == OBJ_GOLD
            || it->base_type == OBJ_RUNES)
        {
            continue;
        }
        if (!display_all && !item_is_selected(*it, item_type_filter))
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
    if (oper != OPER_WIELD && !any_items_of_type(item_type_filter, -1, true))
    {
        mprf(MSGCH_PROMPT, "%s",
            no_selectables_message(item_type_filter).c_str());
        return true;
    }
    return false;
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
    if (item_inv.empty())
        is_inventory = false;
    else if (item_floor.empty())
        is_inventory = true;

    // Entry for unarmed. Hotkey works for either subsection, though selecting
    // it (currently) enables the inv section.
    if (oper == OPER_WIELD)
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
                        // hacky: remove the class hotkey for cases where it
                        // is counterintuitive/useless
                        if (item_type_filter != OSEL_UNIDENT)
                            entry->hotkeys.pop_back();
                        if (item_type_filter == OBJ_SCROLLS)
                            _note_tele_cancel(entry);
                        return entry;
                    });
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
                        // hacky: remove the class hotkey
                        entry->hotkeys.pop_back();
                        if (item_type_filter == OBJ_SCROLLS)
                            _note_tele_cancel(entry);
                        return entry;
                    });
    }
    update_sections();

    if (last_hovered >= 0 && !item_floor.empty() && !item_inv.empty())
    {
        if (is_inventory && last_hovered > last_inv_pos)
            set_hovered(oper == OPER_WIELD ? 1 : 0);
        else if (!is_inventory && last_hovered <= last_inv_pos)
            set_hovered(last_inv_pos + 1);
    }
}

void UseItemMenu::update_sections()
{
    // never disable the unwield button
    int i = oper == OPER_WIELD ? 1 : 0;
    for (; i <= last_inv_pos; i++)
        if (items[i]->level == MEL_ITEM)
            items[i]->set_enabled(is_inventory);
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
                        (oper == OPER_WEAR ? "wear"
                        : oper == OPER_WIELD ? "wield"
                        : "use"));
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

void UseItemMenu::toggle_display_all()
{
    // don't allow identifying already id'd items, etc
    if (oper == OPER_ANY)
        return;
    clear();
    display_all = !display_all;
    populate_list();
    populate_menu();
    update_sections();
    update_more();
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
    if (oper == OPER_WIELD && i == 0)
        return true; // no description implemented
    else if (is_inventory)
        return InvMenu::examine_index(i);
    else // floor item
    {
        auto ie = dynamic_cast<InvEntry *>(items[i]);
        auto desc_tgt = const_cast<item_def*>(ie->item);
        ASSERT(desc_tgt);
        return describe_item(*desc_tgt, nullptr, false);
    }
}

string UseItemMenu::get_keyhelp(bool) const
{
    string r;
    if (oper == OPER_ANY)
        return r; // `*` is disabled for identify, enchant

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
        r += menu_keyhelp_cmd(CMD_MENU_CYCLE_MODE) + " "
            + join_strings(mode_names.begin(), mode_names.end(), "|")
            + "   ";
    }

    // TODO: show cycle mode information, once this feature is more baked
    r += "[<w>*</w>] ";
    r += (display_all ? "show appropriate" : "show all");
    return r;
}

bool UseItemMenu::process_key(int key)
{
    // TODO: should check inscriptions here
    if (isadigit(key)
        || key == '-' && oper == OPER_WIELD)
    {
        lastch = key;
        return false;
    }

    if (key == '*')
    {
        toggle_display_all();
        return true;
    }
    else if (key == '\\')
    {
        check_item_knowledge();
        return true;
    }
    else if (key == ','
        && Options.easy_floor_use && item_floor.size() == 1
        && (is_inventory || item_inv.empty()))
    {
        // TODO: should this go with CMD_MENU_CYCLE_HEADERS instead of `,`?
        lastch = ','; // XX don't use keycode for this
        return false;
    }

    return Menu::process_key(key);
}

/**
 * Prompt use of an item from either player inventory or the floor.
 *
 * This function generates a menu containing type_expect items based on the
 * object_class_type to be acted on by another function. First it will list
 * items in inventory, then items on the floor. If the prompt is cancelled,
 * false is returned. If something is successfully choosen, then true is
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
operation_types use_an_item(item_def *&target, operation_types oper, int item_type,
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

        // Handle inscribed item keys
        if (isadigit(keyin))
        {
            // select by inscription
            // This allows you to select stuff by inscription that is not on the
            // screen, but only if you couldn't by default use it for that
            // operation anyway. It's a bit weird, but it does save a '*'
            // keypress for bread-swingers.
            tmp_tgt = digit_inscription_to_item(keyin, oper);
            if (tmp_tgt)
                choice_made = true;
        }
        else if (keyin == ',')
        {
            // first floor item
            ASSERT(!menu.item_floor.empty());
            choice_made = true;
            tmp_tgt = const_cast<item_def*>(menu.item_floor[0]);
        }
        else if (keyin == '-' && menu.oper == OPER_WIELD)
        {
            choice_made = true;
            tmp_tgt = nullptr;
        }
        else if (!sel.empty())
        {
            ASSERT(sel.size() == 1);

            choice_made = true;
            auto ie = dynamic_cast<InvEntry *>(sel[0]);
            tmp_tgt = const_cast<item_def*>(ie->item);
        }

        redraw_screen();
        update_screen();
        // For weapons, armour, and jewellery this is handled in wield_weapon,
        // wear_armour, and _puton_item after selection
        if (menu.oper != OPER_WIELD && menu.oper != OPER_WEAR
            && menu.oper != OPER_PUTON
            && choice_made && tmp_tgt
            && !check_warning_inscriptions(*tmp_tgt, oper))
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

    ASSERT(!choice_made || target || menu.oper == OPER_WIELD);
    if (choice_made)
        return menu.oper;
    else
        return OPER_NONE;
}

static bool _safe_to_remove_or_wear(const item_def &item, const item_def
                                    *old_item, bool remove, bool quiet = false);
static bool _safe_to_remove_or_wear(const item_def &item,
                                    bool remove, bool quiet = false);

// Rather messy - we've gathered all the can't-wield logic from wield_weapon()
// here.
bool can_wield(const item_def *weapon, bool say_reason,
               bool ignore_temporary_disability, bool unwield, bool only_known)
{
#define SAY(x) {if (say_reason) { x; }}
    if (you.melded[EQ_WEAPON] && unwield)
    {
        SAY(mpr("Your weapon is melded into your body!"));
        return false;
    }

    if (!ignore_temporary_disability && !form_can_wield(you.form))
    {
        SAY(mpr("You can't wield anything in your present form."));
        return false;
    }

    if (!ignore_temporary_disability
        && player_equip_unrand(UNRAND_DEMON_AXE)
        && you.beheld())
    {
        SAY(mpr("Your thirst for blood prevents you from unwielding your "
                "weapon!"));
        return false;
    }

    if (!ignore_temporary_disability
        && you.weapon()
        && is_weapon(*you.weapon())
        && you.weapon()->cursed())
    {
        SAY(mprf("You can't unwield your weapon%s!",
                 !unwield ? " to draw a new one" : ""));
        return false;
    }

    // If we don't have an actual weapon to check, return now.
    if (!weapon)
        return true;

    if (you.get_mutation_level(MUT_MISSING_HAND)
            && you.hands_reqd(*weapon) == HANDS_TWO)
    {
        SAY(mprf("You can't wield that without your missing %s.",
            species::arm_name(you.species).c_str()));
        return false;
    }

    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; i++)
    {
        if (you.equip[i] != -1 && &you.inv[you.equip[i]] == weapon)
        {
            SAY(mpr("You are wearing that object!"));
            return false;
        }
    }

    if (!you.could_wield(*weapon, true, true, !say_reason))
        return false;

    // All non-weapons only need a shield check.
    if (weapon->base_type != OBJ_WEAPONS)
    {
        if (!ignore_temporary_disability && is_shield_incompatible(*weapon))
        {
            SAY(mpr("You can't wield that with only one hand."));
            return false;
        }
        else
            return true;
    }

    bool id_brand = false;

    if (you.undead_or_demonic() && is_holy_item(*weapon)
        && (item_type_known(*weapon) || !only_known))
    {
        if (say_reason)
        {
            mpr("This weapon is holy and will not allow you to wield it.");
            id_brand = true;
        }
        else
            return false;
    }
    if (id_brand)
    {
        auto wwpn = const_cast<item_def*>(weapon);
        if (!is_artefact(*weapon) && !is_blessed(*weapon)
            && !item_type_known(*weapon))
        {
            set_ident_flags(*wwpn, ISFLAG_KNOW_TYPE);
            if (in_inventory(*weapon))
                mprf_nocap("%s", weapon->name(DESC_INVENTORY_EQUIP).c_str());
        }
        else if (is_artefact(*weapon) && !item_type_known(*weapon))
            artefact_learn_prop(*wwpn, ARTP_BRAND);
        return false;
    }

    if (!ignore_temporary_disability && is_shield_incompatible(*weapon))
    {
        if (you.has_mutation(MUT_QUADRUMANOUS) && say_reason)
            mpr("You can't wield that with only one hand-pair.");
        else
            SAY(mpr("You can't wield that with only one hand."));
        return false;
    }

    // We can wield this weapon. Phew!
    return true;

#undef SAY
}

/**
 * Helper function for wield_weapon, wear_armour, and puton_ring
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
 * Helper function for wield_weapon, wear_armour, and puton_ring
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
 * Helper function for wield_weapon, wear_armour, and puton_ring
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

static bool _do_wield_weapon(item_def *to_wield, bool show_weff_messages=true,
                  bool show_unwield_msg=true, bool show_wield_msg=true,
                  bool adjust_time_taken=true);
static bool _do_wear_armour(item_def *to_wear);


static bool _equip_item(item_def *to_equip, operation_types o)
{
    // TODO: refactor this
    if (to_equip == nullptr && o == OPER_WIELD)
        return wield_weapon(true, SLOT_BARE_HANDS);

    ASSERT(to_equip);

    if (o == OPER_WIELD)
        return _do_wield_weapon(to_equip);
    else if (o == OPER_WEAR)
        return _do_wear_armour(to_equip);
    else if (o == OPER_PUTON)
        return puton_ring(*to_equip);
    else
        return false; // or ASSERT?
}

/**
 * @param auto_wield false if this was initiated by the wield weapon command (w)
 *      true otherwise (e.g. switching between ranged and melee with the
 *      auto_switch option)
 * @param slot Index into inventory of item to equip. Or one of following
 *     special values:
 *      - -1 (default): meaning no particular weapon. We'll either prompt for a
 *        choice of weapon (if auto_wield is false) or choose one by default.
 *      - SLOT_BARE_HANDS: equip nothing (unwielding current weapon, if any)
 */
bool wield_weapon(bool auto_wield, int slot, bool show_weff_messages,
                  bool show_unwield_msg, bool show_wield_msg,
                  bool adjust_time_taken)
{
    // Abort immediately if there's some condition that could prevent wielding
    // weapons.
    if (!can_wield(nullptr, true, false, slot == SLOT_BARE_HANDS))
        return false;

    item_def *to_wield = &you.inv[0]; // default is 'a'
        // we'll set this to nullptr to indicate bare hands

    if (auto_wield)
    {
        if (to_wield == you.weapon()
            || you.equip[EQ_WEAPON] == -1 && !item_is_wieldable(*to_wield))
        {
            to_wield = &you.inv[1];      // backup is 'b'
        }

        if (slot != -1)         // allow external override
        {
            if (slot == SLOT_BARE_HANDS)
                to_wield = nullptr;
            else
                to_wield = &you.inv[slot];
        }
    }

    if (to_wield)
    {
    // Prompt if not using the auto swap command
        if (!auto_wield)
        {
            operation_types o = use_an_item(to_wield, OPER_WIELD);
            if (o == OPER_NONE)
                return false;
            else if (o != OPER_WIELD)
                return _equip_item(to_wield, o);
        }

    // If autowielding and the swap slot has a bad or invalid item in it, the
    // swap will be to bare hands.
        else if (!to_wield->defined() || !item_is_wieldable(*to_wield))
            to_wield = nullptr;
    }
    return _do_wield_weapon(to_wield, show_weff_messages, show_unwield_msg,
                                            show_wield_msg, adjust_time_taken);
}

static bool _do_wield_weapon(item_def *to_wield, bool show_weff_messages,
                  bool show_unwield_msg, bool show_wield_msg,
                  bool adjust_time_taken)
{
    // Reset the warning counter. We do this before the rewield check to
    // provide a (slightly hacky) way to let players reset this without
    // unwielding. (TODO: better ui?)
    you.received_weapon_warning = false;

    if (to_wield && to_wield->pos != ITEM_IN_INVENTORY
        && !_can_move_item_from_floor_to_inv(*to_wield)) // does messaging
    {
        return false;
    }

    if (to_wield && to_wield == you.weapon())
    {
        if (Options.equip_unequip)
            to_wield = nullptr;
        else
        {
            mpr("You are already wielding that!");
            return true;
        }
    }

    if (!to_wield)
    {
        if (const item_def* wpn = you.weapon())
        {
            bool penance = false;
            // Can we safely unwield this item?
            if (needs_handle_warning(*wpn, OPER_WIELD, penance))
            {
                string prompt =
                    "Really unwield " + wpn->name(DESC_INVENTORY) + "?";
                if (penance)
                    prompt += " This could place you under penance!";

                if (!yesno(prompt.c_str(), false, 'n'))
                {
                    canned_msg(MSG_OK);
                    return false;
                }
            }

            // check if you'd get stat-zeroed
            if (!_safe_to_remove_or_wear(*wpn, true))
                return false;

            if (!unwield_item(show_weff_messages))
                return false;

            if (show_unwield_msg)
            {
#ifdef USE_SOUND
                parse_sound(WIELD_NOTHING_SOUND);
#endif
                canned_msg(MSG_EMPTY_HANDED_NOW);
            }

            // Switching to bare hands is extra fast.
            you.turn_is_over = true;
            if (adjust_time_taken)
            {
                you.time_taken *= 3;
                you.time_taken /= 10;
            }
        }
        else
            canned_msg(MSG_EMPTY_HANDED_ALREADY);

        return true;
    }

    // By now we're sure we're swapping to a real weapon, not bare hands

    item_def& new_wpn = *to_wield;

    // Switching to a launcher while berserk is likely a mistake.
    if (you.berserk() && is_range_weapon(new_wpn))
    {
        string prompt = "You can't shoot while berserk! Really wield " +
                        new_wpn.name(DESC_INVENTORY) + "?";
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    // Ensure wieldable
    if (!can_wield(&new_wpn, true))
        return false;

    // Really ensure wieldable, even unknown brand
    if (!can_wield(&new_wpn, true, false, false, false))
        return false;

    // At this point, we know it's possible to equip this item. However, there
    // might be reasons it's not advisable.
    if (!check_warning_inscriptions(new_wpn, OPER_WIELD))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (!_safe_to_remove_or_wear(new_wpn, you.weapon(), false))
        return false;

    // Unwield any old weapon.
    if (you.weapon())
    {
        if (unwield_item(show_weff_messages))
        {
            // Enable skills so they can be re-disabled later
            update_can_currently_train();
        }
        else
            return false;
    }

    const unsigned int old_talents = your_talents(false).size();

    // If it's on the ground, pick it up. Once it's picked up, there should be
    // no aborting, lest we introduce a way to instantly pick things up
    // NB we already made sure there was space for the item
    int item_slot = _get_item_slot_maybe_with_move(new_wpn);

    // At this point new_wpn is potentially not the right thing anymore (the
    // thing actually in the player's inventory), that is, in the case where the
    // player chose something from the floor. So use item_slot from here on.

    // Go ahead and wield the weapon.
    equip_item(EQ_WEAPON, item_slot, show_weff_messages);

    if (show_wield_msg)
    {
#ifdef USE_SOUND
        parse_sound(WIELD_WEAPON_SOUND);
#endif
        mprf_nocap("%s", you.inv[item_slot].name(DESC_INVENTORY_EQUIP).c_str());
    }

    check_item_hint(you.inv[item_slot], old_talents);

    // Time calculations.
    if (adjust_time_taken)
        you.time_taken /= 2;

    you.wield_change  = true;
    quiver::on_weapon_changed();
    you.turn_is_over  = true;

    return true;
}

bool item_is_worn(int inv_slot)
{
    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; ++i)
        if (inv_slot == you.equip[i])
            return true;

    return false;
}

/**
 * Prompt user for carried armour. Will warn user if wearing/removing the armour is dangerous.
 *
 * @param mesg Title for the prompt
 * @param index[out] the inventory slot of the item chosen; not initialised
 *                   if a valid item was not chosen.
 * @param oper if equal to OPER_TAKEOFF, only show items relevant to the 'T'
 *             command.
 * @return whether a valid armour item was chosen.
 */
bool armour_prompt(const string & mesg, int *index, operation_types oper)
{
    ASSERT(index != nullptr);

    if (you.berserk())
        canned_msg(MSG_TOO_BERSERK);
    else
    {
        int selector = OBJ_ARMOUR;
        if (oper == OPER_TAKEOFF && !Options.equip_unequip)
            selector = OSEL_WORN_ARMOUR;
        int slot = prompt_invent_item(mesg.c_str(), menu_type::invlist,
                                      selector, oper);

        if (!prompt_failed(slot))
        {
            *index = slot;
            return true;
        }
    }

    return false;
}

static const int ARMOUR_EQUIP_DELAY = 5;

// If you can't wear a bardings, why not? (If you can, return "".)
static string _cant_wear_barding_reason(bool ignore_temporary)
{
    if (!you.wear_barding())
        return "You can't wear that!";

    if (!ignore_temporary && player_is_shapechanged())
        return "You can wear that only in your normal form.";

    return "";
}

/**
 * Can you wear this item of armour currently?
 *
 * Ignores whether or not an item is equipped in its slot already.
 * If the item is Lear's hauberk, some of this comment may be incorrect.
 *
 * @param item The item. Only the base_type and sub_type really should get
 *             checked, since you_can_wear passes in a dummy item.
 * @param verbose Whether to print a message about your inability to wear item.
 * @param ignore_temporary Whether to take into account forms/fishtail/2handers.
 *                         Note that no matter what this is set to, all
 *                         mutations will be taken into account, except for
 *                         ones from Beastly Appendage, which are only checked
 *                         if this is false.
 */
bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary)
{
    const object_class_type base_type = item.base_type;
    if (base_type != OBJ_ARMOUR || you.has_mutation(MUT_NO_ARMOUR))
    {
        if (verbose)
            mpr("You can't wear that!");

        return false;
    }

    const int sub_type = item.sub_type;
    const equipment_type slot = get_armour_slot(item);

    if (species::bans_eq(you.species, slot))
    {
        if (verbose)
        {
            // *if* the species bans body armour, then blame it on wings.
            // (But don't unconditionally ban armour with this mut.)
            // XX turn this mut into a two-level mutation?
            if (slot == EQ_BODY_ARMOUR
                && species::mutation_level(you.species, MUT_BIG_WINGS))
            {
                mprf("Your wings%s won't fit in that.",
                    you.has_mutation(MUT_BIG_WINGS)
                        ? "" : ", even vestigial as they are,");
            }
            else
                mpr("You can't wear that!");
        }
        return false;
    }

    if (sub_type == ARM_BARDING)
    {
        const string reason = _cant_wear_barding_reason(ignore_temporary);
        if (reason == "")
            return true;
        if (verbose)
            mpr(reason);
        return false;
    }

    if (you.get_mutation_level(MUT_MISSING_HAND) && is_offhand(item))
    {
        if (verbose)
        {
            if (you.has_innate_mutation(MUT_TENTACLE_ARMS))
                mpr("You need the rest of your tentacles for walking.");
            else
                mprf("You'd need another %s to do that!", you.hand_name(false).c_str());
        }
        return false;
    }

    if (!ignore_temporary && you.weapon()
        && is_offhand(item)
        && is_shield_incompatible(*you.weapon(), &item))
    {
        if (verbose)
        {
            if (you.has_innate_mutation(MUT_TENTACLE_ARMS))
                mpr("You need the rest of your tentacles for walking.");
            else if (you.has_mutation(MUT_QUADRUMANOUS))
                mpr("You'd need three hand-pairs to do that!");
            else
            {
                // Singular hand should have already been handled above.
                mprf("You'd need three %s to do that!",
                     you.hand_name(true).c_str());
            }
        }
        return false;
    }

    const string mut_block = mut_blocks_item_reason(item, false);
    if (!mut_block.empty())
    {
        if (verbose)
            mprf("%s", mut_block.c_str());
        return false;
    }

    // Lear's hauberk covers also head, hands and legs.
    if (is_unrandom_artefact(item, UNRAND_LEAR))
    {
        if (you.wear_barding())
        {
            if (verbose)
                mpr("The hauberk won't fit over your tail.");
            return false;
        }

        if (!player_has_feet(!ignore_temporary))
        {
            if (verbose)
                mpr("You have no feet.");
            return false;
        }

        if (!ignore_temporary)
        {
            for (int s = EQ_HELMET; s <= EQ_BOOTS; s++)
            {
                // No strange race can wear this.
                const string parts[] = { "head", you.hand_name(true),
                                         you.foot_name(true) };
                COMPILE_CHECK(ARRAYSZ(parts) == EQ_BOOTS - EQ_HELMET + 1);

                // Auto-disrobing would be nice.
                if (you.equip[s] != -1)
                {
                    if (verbose)
                    {
                        mprf("You'd need your %s free.",
                             parts[s - EQ_HELMET].c_str());
                    }
                    return false;
                }

                if (!get_form()->slot_available(s))
                {
                    if (verbose)
                    {
                        mprf("The hauberk won't fit your %s.",
                             parts[s - EQ_HELMET].c_str());
                    }
                    return false;
                }
            }
        }
    }
    else if (slot >= EQ_HELMET && slot <= EQ_BOOTS
             && !ignore_temporary
             && player_equip_unrand(UNRAND_LEAR, true))
    {
        // The explanation is iffy for loose headgear, especially crowns:
        // kings loved hooded hauberks, according to portraits.
        if (verbose)
            mpr("You can't wear this over your hauberk.");
        return false;
    }

    size_type player_size = you.body_size(PSIZE_TORSO, ignore_temporary);
    int bad_size = fit_armour_size(item, player_size);
#if TAG_MAJOR_VERSION == 34
    if (is_unrandom_artefact(item, UNRAND_TALOS))
    {
        // adjust bad_size for the oversized plate armour
        // negative means levels too small, positive means levels too large
        bad_size = SIZE_LARGE - player_size;
    }
#endif

    if (bad_size)
    {
        if (verbose)
        {
            mprf("This armour is too %s for you!",
                 (bad_size > 0) ? "big" : "small");
        }

        return false;
    }

    if (sub_type == ARM_BOOTS)
    {
        if (you.wear_barding())
        {
            if (verbose)
            {
                if (you.has_mutation(MUT_CONSTRICTING_TAIL))
                    mpr("You have no legs!");
                else
                    mpr("Boots don't fit your feet!"); // armataur
            }
            return false;
        }

        if (!ignore_temporary && you.fishtail)
        {
            if (verbose)
                mpr("You don't currently have feet!");
            return false;
        }
    }

    if (is_hard_helmet(item))
    {
        if (species::is_draconian(you.species))
        {
            if (verbose)
                mpr("You can't wear that with your reptilian head.");
            return false;
        }

        if (you.species == SP_OCTOPODE)
        {
            if (verbose)
                mpr("You can't wear that!");
            return false;
        }
    }

    // Can't just use Form::slot_available because of shroom caps.
    if (!ignore_temporary && !get_form()->can_wear_item(item))
    {
        if (verbose)
            mpr("You can't wear that in your present form.");
        return false;
    }

    return true;
}

static bool _can_takeoff_armour(int item);

// Like can_wear_armour, but also takes into account currently worn equipment.
// e.g. you may be able to *wear* that robe, but you can't equip it if your
// currently worn armour is cursed, or melded.
// precondition: item is not already worn
static bool _can_equip_armour(const item_def &item)
{
    const object_class_type base_type = item.base_type;
    if (base_type != OBJ_ARMOUR)
    {
        mpr("You can't wear that.");
        return false;
    }

    const equipment_type slot = get_armour_slot(item);
    const int equipped = you.equip[slot];
    if (equipped != -1 && !_can_takeoff_armour(equipped))
        return false;
    return can_wear_armour(item, true, false);
}

// Try to equip the armour in the given inventory slot (or, if slot is -1,
// prompt for a choice of item, then try to wear it).
bool wear_armour(int item)
{
    // Before (possibly) prompting for which item to wear, check for some
    // conditions that would make it impossible to wear any type of armour.
    // TODO: perhaps also worth checking here whether all available armour slots
    // are cursed. Same with jewellery.
    if (you.has_mutation(MUT_NO_ARMOUR))
    {
        mpr("You can't wear anything.");
        return false;
    }

    if (!form_can_wear())
    {
        mpr("You can't wear anything in your present form.");
        return false;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    item_def *to_wear = nullptr;

    if (item == -1)
    {
        auto o = use_an_item(to_wear, OPER_WEAR);
        if (o == OPER_NONE)
            return false;
        else if (o != OPER_WEAR)
            return _equip_item(to_wear, o);
    }
    else
        to_wear = &you.inv[item];

    return _do_wear_armour(to_wear);
}

static bool _do_wear_armour(item_def *to_wear)
{
    ASSERT(to_wear);
    // First, let's check for any conditions that would make it impossible to
    // equip the given item
    if (!to_wear->defined())
    {
        mpr("You don't have any such object.");
        return false;
    }

    if (to_wear == you.weapon())
    {
        mpr("You are wielding that object!");
        return false;
    }

    if (to_wear->pos != ITEM_IN_INVENTORY
        && !_can_move_item_from_floor_to_inv(*to_wear)) // does messaging
    {
        return false;
    }

    if (to_wear->pos == ITEM_IN_INVENTORY && item_is_worn(to_wear->link))
    {
        if (Options.equip_unequip)
            return takeoff_armour(to_wear->link);
        else
        {
            mpr("You're already wearing that object!");
            return false;
        }
    }

    if (!_can_equip_armour(*to_wear))
        return false;

    // At this point, we know it's possible to equip this item. However, there
    // might be reasons it's not advisable. Warn about any dangerous
    // inscriptions, giving the player an opportunity to bail out.
    if (!check_warning_inscriptions(*to_wear, OPER_WEAR))
    {
        canned_msg(MSG_OK);
        return false;
    }

    bool swapping = false;
    const equipment_type slot = get_armour_slot(*to_wear);

    if (!_safe_to_remove_or_wear(*to_wear, you.slot_item(slot), false))
        return false;

    if ((slot == EQ_CLOAK
           || slot == EQ_HELMET
           || slot == EQ_GLOVES
           || slot == EQ_BOOTS
           || slot == EQ_SHIELD
           || slot == EQ_BODY_ARMOUR)
        && you.equip[slot] != -1)
    {
        if (!takeoff_armour(you.equip[slot], true))
            return false;
        swapping = true;
    }

    you.turn_is_over = true;

    // If it's on the ground, pick it up. Once it's picked up, there should be
    // no aborting
    // NB we already made sure there was space for the item
    int item_slot = _get_item_slot_maybe_with_move(*to_wear);

    start_delay<EquipOnDelay>(ARMOUR_EQUIP_DELAY - (swapping ? 0 : 1),
                              you.inv[item_slot]);

    return true;
}

static bool _can_generically_takeoff_armour()
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (you.has_mutation(MUT_NO_ARMOUR))
    {
        mprf("You can't remove your %s, sorry.",
             species::skin_name(you.species).c_str());
        return false;
    }

    if (!form_can_wear())
    {
        mpr("You can't wear or remove anything in your present form.");
        return false;
    }

    return true;
}

static bool _can_takeoff_armour(int item)
{
    if (!_can_generically_takeoff_armour())
        return false;

    item_def& invitem = you.inv[item];

    if (invitem.base_type != OBJ_ARMOUR)
    {
        mpr("You couldn't even wear that if you tried!");
        return false;
    }

    const equipment_type slot = get_armour_slot(invitem);
    if (item == you.equip[slot] && you.melded[slot])
    {
        mprf("%s is melded into your body!",
             invitem.name(DESC_YOUR).c_str());
        return false;
    }

    if (!item_is_worn(item))
    {
        mpr("You aren't wearing that object!");
        return false;
    }

    // If we get here, we're wearing the item.
    if (invitem.cursed())
    {
        mprf("%s is stuck to your body!", invitem.name(DESC_YOUR).c_str());
        return false;
    }
    return true;
}

/**
 * Takes off a given piece of armour.
 * @param item The item to remove. If -1, the player will be prompted.
 * @param noask Whether to prompt if removing the item would cause stat-zero.
 * @return True if the item was removed.
 */
bool takeoff_armour(int item, bool noask)
{
    // We want to check non-item depedent stuff before prompting for the actual item
    if (!_can_generically_takeoff_armour())
        return false;

    if (item == -1
        && !armour_prompt("Take off which item?", &item, OPER_TAKEOFF))
    {
        return false;
    }

    if (!_can_takeoff_armour(item))
        return false;

    item_def& invitem = you.inv[item];

    // It's possible to take this thing off, but if it would drop a stat
    // below 0, we should get confirmation.
    if (!noask && !_safe_to_remove_or_wear(invitem, true))
        return false;

    you.turn_is_over = true;
    start_delay<EquipOffDelay>(ARMOUR_EQUIP_DELAY - 1, invitem);

    return true;
}

// Returns a list of possible ring slots.
static vector<equipment_type> _current_ring_types()
{
    vector<equipment_type> ret = species::ring_slots(you.species,
                                        you.has_mutation(MUT_MISSING_HAND));

    if (player_equip_unrand(UNRAND_FINGER_AMULET))
        ret.push_back(EQ_RING_AMULET);

    erase_if(ret, [](const equipment_type &e)
        {
            return !get_form()->slot_available(e);
        });
    return ret;
}

static vector<equipment_type> _current_jewellery_types()
{
    vector<equipment_type> ret = _current_ring_types();
    ret.push_back(EQ_AMULET);
    return ret;
}

static char _ring_slot_key(equipment_type slot)
{
    switch (slot)
    {
    case EQ_LEFT_RING:      return '<';
    case EQ_RIGHT_RING:     return '>';
    case EQ_RING_AMULET:    return '^';
    case EQ_RING_ONE:       return '1';
    case EQ_RING_TWO:       return '2';
    case EQ_RING_THREE:     return '3';
    case EQ_RING_FOUR:      return '4';
    case EQ_RING_FIVE:      return '5';
    case EQ_RING_SIX:       return '6';
    case EQ_RING_SEVEN:     return '7';
    case EQ_RING_EIGHT:     return '8';
    default:
        die("Invalid ring slot");
    }
}

static int _prompt_ring_to_remove()
{
    const vector<equipment_type> ring_types = _current_ring_types();
    vector<char> slot_chars;
    vector<item_def*> rings;
    for (auto eq : ring_types)
    {
        rings.push_back(you.slot_item(eq, true));
        ASSERT(rings.back());
        slot_chars.push_back(index_to_letter(rings.back()->link));
    }

    if (slot_chars.size() + 2 > msgwin_lines() || ui::has_layout())
    {
        // force a menu rather than a more().
        return EQ_NONE;
    }

    clear_messages();

    mprf(MSGCH_PROMPT,
         "You're wearing all the rings you can. Remove which one?");
    mprf(MSGCH_PROMPT, "(<w>?</w> for menu, <w>Esc</w> to cancel)");

    // FIXME: Needs TOUCH_UI version

    for (size_t i = 0; i < rings.size(); i++)
    {
        string m = "<w>";
        const char key = _ring_slot_key(ring_types[i]);
        m += key;
        if (key == '<')
            m += '<';

        m += "</w> or " + rings[i]->name(DESC_INVENTORY);
        mprf_nocap("%s", m.c_str());
    }
    flush_prev_message();

    // Deactivate choice from tile inventory.
    // FIXME: We need to be able to get the choice (item letter)n
    //        *without* the choice taking action by itself!
    int eqslot = EQ_NONE;

    mouse_control mc(MOUSE_MODE_PROMPT);
    int c;
    do
    {
        c = getchm();
        for (size_t i = 0; i < slot_chars.size(); i++)
        {
            if (c == slot_chars[i]
                || c == _ring_slot_key(ring_types[i]))
            {
                eqslot = ring_types[i];
                c = ' ';
                break;
            }
        }
    } while (!key_is_escape(c) && c != ' ' && c != '?');

    clear_messages();

    if (c == '?')
        return EQ_NONE;
    else if (key_is_escape(c) || eqslot == EQ_NONE)
        return -2;

    return you.equip[eqslot];
}

// Calculate the stat bonus from an item.
// XXX: This needs to match _stat_modifier() and get_item_description().
static void _item_stat_bonus(const item_def &item, int &prop_str,
                             int &prop_dex, int &prop_int, bool remove)
{
    prop_str = prop_dex = prop_int = 0;

    if (item.base_type == OBJ_JEWELLERY
        && item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        switch (item.sub_type)
        {
        case RING_STRENGTH:
            if (item.plus != 0)
                prop_str = item.plus;
            break;
        case RING_DEXTERITY:
            if (item.plus != 0)
                prop_dex = item.plus;
            break;
        case RING_INTELLIGENCE:
            if (item.plus != 0)
                prop_int = item.plus;
            break;
        default:
            break;
        }
    }
    else if (item.base_type == OBJ_ARMOUR && item_type_known(item))
    {
        switch (item.brand)
        {
        case SPARM_STRENGTH:
            prop_str = 3;
            break;
        case SPARM_INTELLIGENCE:
            prop_int = 3;
            break;
        case SPARM_DEXTERITY:
            prop_dex = 3;
            break;
        default:
            break;
        }
    }

    if (is_artefact(item))
    {
        prop_str += artefact_known_property(item, ARTP_STRENGTH);
        prop_int += artefact_known_property(item, ARTP_INTELLIGENCE);
        prop_dex += artefact_known_property(item, ARTP_DEXTERITY);
    }

    if (!remove)
    {
        prop_str *= -1;
        prop_int *= -1;
        prop_dex *= -1;
    }
}

enum class afsz
{
    go, // asked the player, who said to to proceed.
    stop, // asked the player (or didn't because of "quiet"), who said to stop.
    noask // no <1 stats, so not asked.
};

static afsz _abort_for_stat_zero(const item_def &item, int prop_str,
                                 int prop_dex, int prop_int,  bool remove,
                                 bool quiet)
{
    stat_type red_stat = NUM_STATS;
    if (prop_str >= you.strength() && you.strength() > 0)
        red_stat = STAT_STR;
    else if (prop_int >= you.intel() && you.intel() > 0)
        red_stat = STAT_INT;
    else if (prop_dex >= you.dex() && you.dex() > 0)
        red_stat = STAT_DEX;

    if (red_stat == NUM_STATS)
        return afsz::noask;

    if (quiet)
        return afsz::stop;

    string verb = "";
    if (remove)
    {
        if (item.base_type == OBJ_WEAPONS)
            verb = "Unwield";
        else
            verb = "Remov"; // -ing, not a typo
    }
    else
    {
        if (item.base_type == OBJ_WEAPONS)
            verb = "Wield";
        else
            verb = "Wear";
    }

    string prompt = make_stringf("%sing this item will reduce your %s to zero "
                                 "or below. Continue?", verb.c_str(),
                                 stat_desc(red_stat, SD_NAME));
    if (!yesno(prompt.c_str(), true, 'n', true, false))
    {
        canned_msg(MSG_OK);
        return afsz::stop;
    }
    return afsz::go;
}

// Checks whether a to-be-worn or to-be-removed item affects
// character stats and whether wearing/removing it could be fatal.
// If so, warns the player, or just returns false if quiet is true.
static bool _safe_to_remove_or_wear(const item_def &item, const item_def
                                    *old_item, bool remove, bool quiet)
{
    if (remove && !safe_to_remove(item, quiet))
        return false;

    int str1 = 0, dex1 = 0, int1 = 0, str2 = 0, dex2 = 0, int2 = 0;
    afsz asked = afsz::noask;
    if (!remove && old_item)
    {
        _item_stat_bonus(*old_item, str1, dex1, int1, true);
        asked = _abort_for_stat_zero(item, str1, dex1, int1, true, quiet);
        if (afsz::stop == asked)
            return false;
    }
    _item_stat_bonus(item, str2, dex2, int2, remove);
    return afsz::go == asked
        || afsz::stop != _abort_for_stat_zero(item, str1+str2, dex1+dex2,
                                              int1+int2, remove, quiet);
}

static bool _safe_to_remove_or_wear(const item_def &item, bool remove,
                                    bool quiet)
{
    return _safe_to_remove_or_wear(item, nullptr, remove, quiet);
}

// Checks whether removing an item would cause flight to end and the
// player to fall to their death.
bool safe_to_remove(const item_def &item, bool quiet)
{
    item_def inf = get_item_known_info(item);

    const bool grants_flight =
         inf.is_type(OBJ_JEWELLERY, RING_FLIGHT)
         || inf.base_type == OBJ_ARMOUR && inf.brand == SPARM_FLYING
         || is_artefact(inf)
            && artefact_known_property(inf, ARTP_FLY);

    // assumes item can't grant flight twice
    const bool removing_ends_flight = you.airborne()
                                        && !you.permanent_flight(false)
                                        && you.equip_flight() == 1;

    const dungeon_feature_type feat = env.grid(you.pos());

    if (grants_flight && removing_ends_flight
        && is_feat_dangerous(feat, false, true))
    {
        if (!quiet)
            mpr("Losing flight right now would be extremely painful!");
        return false;
    }

    return true;
}

// Assumptions:
// item is an item in inventory or on the floor where the player is standing
// EQ_LEFT_RING and EQ_RIGHT_RING are both occupied, and item is not
// in one of those slots.
//
// Does not do amulets.
static bool _swap_rings(item_def& to_puton)
{
    vector<equipment_type> ring_types = _current_ring_types();
    const int num_rings = ring_types.size();
    int unwanted = 0;
    int last_inscribed = 0;
    int cursed = 0;
    int inscribed = 0;
    int melded = 0; // Both melded rings and unavailable slots.
    int available = 0;
    bool all_same = true;
    item_def* first_ring = nullptr;
    for (auto eq : ring_types)
    {
        item_def* ring = you.slot_item(eq, true);
        if (!you_can_wear(eq, true) || you.melded[eq])
            melded++;
        else if (ring != nullptr)
        {
            if (first_ring == nullptr)
                first_ring = ring;
            else if (all_same)
            {
                if (ring->sub_type != first_ring->sub_type
                    || ring->plus  != first_ring->plus
                    || is_artefact(*ring) || is_artefact(*first_ring))
                {
                    all_same = false;
                }
            }

            if (ring->cursed())
                cursed++;
            else if (strstr(ring->inscription.c_str(), "=R"))
            {
                inscribed++;
                last_inscribed = you.equip[eq];
            }
            else
            {
                available++;
                unwanted = you.equip[eq];
            }
        }
    }

    // If the only swappable rings are inscribed =R, go ahead and use them.
    if (available == 0 && inscribed > 0)
    {
        available += inscribed;
        unwanted = last_inscribed;
    }

    // We can't put a ring on, because we're wearing all cursed ones.
    if (melded == num_rings)
    {
        // Shouldn't happen, because hogs and bats can't put on jewellery at
        // all and thus won't get this far.
        mpr("You can't wear that in your present form.");
        return false;
    }
    else if (available == 0)
    {
        mprf("You're already wearing %s cursed ring%s!%s",
             number_in_words(cursed).c_str(),
             (cursed == 1 ? "" : "s"),
             (cursed > 2 ? " Isn't that enough for you?" : ""));
        return false;
    }
    // The simple case - only one available ring.
    // If the jewellery_prompt option is true, always allow choosing the
    // ring slot (even if we still have empty slots).
    else if (available == 1 && !Options.jewellery_prompt)
    {
        if (!_safe_to_remove_or_wear(to_puton, &you.inv[unwanted], false))
            return false;

        if (!remove_ring(unwanted, false, true))
            return false;
    }
    // We can't put a ring on without swapping - because we found
    // multiple available rings.
    else
    {
        // Don't prompt if all the rings are the same.
        if (!all_same || Options.jewellery_prompt)
            unwanted = _prompt_ring_to_remove();

        if (unwanted == EQ_NONE)
        {
            // do this here rather than in remove_ring so that the custom
            // message is visible.
            unwanted = prompt_invent_item(
                    "You're wearing all the rings you can. Remove which one?",
                    menu_type::invlist, OSEL_UNCURSED_WORN_RINGS, OPER_REMOVE,
                    invprompt_flag::no_warning | invprompt_flag::hide_known);
        }

        // Cancelled:
        if (unwanted < 0)
        {
            canned_msg(MSG_OK);
            return false;
        }

        if (!_safe_to_remove_or_wear(to_puton, &you.inv[unwanted], false))
            return false;

        if (!remove_ring(unwanted, false, true))
            return false;
    }

    // Put on the new ring.
    start_delay<JewelleryOnDelay>(1, to_puton);

    return true;
}

static equipment_type _choose_ring_slot()
{
    clear_messages();

    mprf(MSGCH_PROMPT,
         "Put ring on which %s? (<w>Esc</w> to cancel)", you.hand_name(false).c_str());

    const vector<equipment_type> slots = _current_ring_types();
    for (auto eq : slots)
    {
        string msg = "<w>";
        const char key = _ring_slot_key(eq);
        msg += key;
        if (key == '<')
            msg += '<';

        item_def* ring = you.slot_item(eq, true);
        if (ring)
            msg += "</w> or " + ring->name(DESC_INVENTORY);
        else
            msg += "</w> - no ring";

        if (eq == EQ_LEFT_RING)
            msg += " (left)";
        else if (eq == EQ_RIGHT_RING)
            msg += " (right)";
        else if (eq == EQ_RING_AMULET)
            msg += " (amulet)";
        mprf_nocap("%s", msg.c_str());
    }
    flush_prev_message();

    equipment_type eqslot = EQ_NONE;
    mouse_control mc(MOUSE_MODE_PROMPT);
    int c;
    do
    {
        c = getchm();
        for (auto eq : slots)
        {
            if (c == _ring_slot_key(eq)
                || (you.slot_item(eq, true)
                    && c == index_to_letter(you.slot_item(eq, true)->link)))
            {
                eqslot = eq;
                c = ' ';
                break;
            }
        }
    } while (!key_is_escape(c) && c != ' ');

    clear_messages();

    return eqslot;
}

// Is it possible to put on the given item in a jewellery slot?
// Preconditions:
// - item is not already equipped in a jewellery slot
static bool _can_puton_jewellery(const item_def &item)
{
    // TODO: between this function, _puton_item, _swap_rings, and remove_ring,
    // there's a bit of duplicated work, and sep. of concerns not clear
    if (&item == you.weapon())
    {
        mpr("You are wielding that object.");
        return false;
    }

    if (item.base_type != OBJ_JEWELLERY)
    {
        mpr("You can only put on jewellery.");
        return false;
    }

    return true;
}

static bool _can_puton_ring(const item_def &item)
{
    if (!_can_puton_jewellery(item))
        return false;
    if (!you_can_wear(EQ_RINGS, true))
    {
        mpr("You can't wear that in your present form.");
        return false;
    }

    const vector<equipment_type> slots = _current_ring_types();
    int melded = 0;
    int cursed = 0;
    for (auto eq : slots)
    {
        if (!you_can_wear(eq, true) || you.melded[eq])
        {
            melded++;
            continue;
        }
        int existing = you.equip[eq];
        if (existing != -1 && you.inv[existing].cursed())
            cursed++;
        else
            // We found an available slot. We're done.
            return true;
    }
    // If we got this far, there are no available slots.
    if (melded == (int)slots.size())
        mpr("You can't wear that in your present form.");
    else
        mprf("You're already wearing %s cursed ring%s!%s",
             number_in_words(cursed).c_str(),
             (cursed == 1 ? "" : "s"),
             (cursed > 2 ? " Isn't that enough for you?" : ""));
    return false;
}

static bool _can_puton_amulet(const item_def &item)
{
    if (!_can_puton_jewellery(item))
        return false;

    if (!you_can_wear(EQ_AMULET, true))
    {
        mpr("You can't wear that in your present form.");
        return false;
    }

    const int existing = you.equip[EQ_AMULET];
    if (existing != -1 && you.inv[existing].cursed())
    {
        mprf("%s is stuck to you!",
             you.inv[existing].name(DESC_YOUR).c_str());
        return false;
    }

    return true;
}

/**
 * Handles the case of putting on a ring where the ring was an amulet; called by puton_ring.
 * @param item The amulet we're putting on.
 * @param check_for_inscriptions Whether or not to prompt the player when there are side effects to wearing the amulet.
 * @return True if the item was put on /or/ taken off.
 */
static bool _puton_amulet(item_def &item,
                          bool check_for_inscriptions)
{
    if (&item == you.slot_item(EQ_AMULET, true))
    {
        // "Putting on" an equipped item means taking it off.
        if (Options.equip_unequip)
            return remove_ring(item.link);
        mpr("You're already wearing that amulet!");
        return false;
    }

    if (!_can_puton_amulet(item))
        return false;

    // It looks to be possible to equip this item. Before going any further,
    // we should prompt the user with any warnings that come with trying to
    // put it on, except when they have already been prompted with them
    // from switching rings.
    if (check_for_inscriptions && !check_warning_inscriptions(item, OPER_PUTON))
    {
        canned_msg(MSG_OK);
        return false;
    }

    item_def *old_amu = you.slot_item(EQ_AMULET, true);

    // Check for stat loss.
    if (!_safe_to_remove_or_wear(item, old_amu, false))
        return false;

    // Remove the previous one.
    if (old_amu && !remove_ring(old_amu->link, true, true))
        return false;

    // puton_ring already confirmed there's room for it
    int item_slot = _get_item_slot_maybe_with_move(item);

    start_delay<EquipOnDelay>(ARMOUR_EQUIP_DELAY, you.inv[item_slot]);
    return true;
}

// Put on a particular ring.
static bool _puton_ring(item_def &item, bool prompt_slot,
                        bool check_for_inscriptions, bool noask)
{
    vector<equipment_type> current_jewellery = _current_ring_types();

    for (auto eq : current_jewellery)
    {
        if (&item != you.slot_item(eq, true))
            continue;
        // "Putting on" an equipped item means taking it off.
        if (Options.equip_unequip)
            return remove_ring(item.link);
        mpr("You're already wearing that ring!");
        return false;
    }

    if (!_can_puton_ring(item))
        return false;

    // It looks to be possible to equip this item. Before going any further,
    // we should prompt the user with any warnings that come with trying to
    // put it on, except when they have already been prompted with them
    // from switching rings.
    if (check_for_inscriptions && !check_warning_inscriptions(item, OPER_PUTON))
    {
        canned_msg(MSG_OK);
        return false;
    }

    const vector<equipment_type> ring_types = _current_ring_types();

    // Check whether there are any unused ring slots
    bool need_swap = true;
    for (auto eq : ring_types)
    {
        if (!you.slot_item(eq, true))
        {
            need_swap = false;
            break;
        }
    }

    // No unused ring slots. Swap out a worn ring for the new one.
    if (need_swap)
        return _swap_rings(item);

    // At this point, we know there's an empty slot for the ring/amulet we're
    // trying to equip.

    // Check for stat loss.
    if (!noask && !_safe_to_remove_or_wear(item, false))
        return false;

    equipment_type hand_used = EQ_NONE;
    if (prompt_slot)
    {
        // Prompt for a slot, even if we have empty ring slots.
        hand_used = _choose_ring_slot();

        if (hand_used == EQ_NONE)
        {
            canned_msg(MSG_OK);
            return false;
        }
        // Allow swapping out a ring.
        else if (you.slot_item(hand_used, true))
        {
            if (!noask && !_safe_to_remove_or_wear(item,
                you.slot_item(hand_used, true), false))
            {
                return false;
            }

            if (!remove_ring(you.equip[hand_used], false, true))
                return false;

            start_delay<JewelleryOnDelay>(1, item);
            return true;
        }
    }
    else
    {
        for (auto eq : ring_types)
        {
            if (!you.slot_item(eq, true))
            {
                hand_used = eq;
                break;
            }
        }
    }

    const unsigned int old_talents = your_talents(false).size();

    // Actually equip the item.
    int item_slot = _get_item_slot_maybe_with_move(item);
    equip_item(hand_used, item_slot);

    check_item_hint(you.inv[item_slot], old_talents);

    // Putting on jewellery is fast.
    you.time_taken /= 2;
    you.turn_is_over = true;

    return true;
}

/**
 * Try to put on or take off a ring or amulet. Most of the work is in _puton_item.
 *
 * @param allow_prompt If we prompt for a slot if we need to choose.
 * @param noask Whether or not to prompt a warning when this action will cause
 *              stat-zero effects.
 * @return True if the item was put on /or/ taken off.
 */
bool puton_ring(item_def &to_puton, bool allow_prompt,
                bool check_for_inscriptions, bool noask)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }
    if (!to_puton.defined())
    {
        mpr("You don't have any such object.");
        return false;
    }

    if (to_puton.pos != ITEM_IN_INVENTORY
        && !_can_move_item_from_floor_to_inv(to_puton))
    {
        return false;
    }

    // item type checking is a bit convoluted here; we can't yet meet the
    // conditions for _can_puton_jewellery (called in _puton_ring) but
    // jewellery_is_amulet is only well-defined if it is passed jewellery,
    // and ASSERTs accordingly
    if (to_puton.base_type == OBJ_JEWELLERY && jewellery_is_amulet(to_puton))
        return _puton_amulet(to_puton, check_for_inscriptions);
    const bool prompt = allow_prompt && Options.jewellery_prompt;
    return _puton_ring(to_puton, prompt, check_for_inscriptions, noask);
}

// Wraps version of puton_ring with item_def param. If slot is -1, prompt for
// which item to put on; otherwise, pass on the item in inventory slot
bool puton_ring(int slot, bool allow_prompt, bool check_for_inscriptions,
                bool noask)
{
    item_def *to_puton_ptr = nullptr;
    if (slot == -1)
    {
        auto o = use_an_item(to_puton_ptr, OPER_PUTON);
        if (o == OPER_NONE)
            return false;
        else if (o != OPER_PUTON)
            return _equip_item(to_puton_ptr, o);
    }
    else
        to_puton_ptr = &you.inv[slot];

    return puton_ring(*to_puton_ptr, allow_prompt, check_for_inscriptions,
                      noask);
}

/**
 * Remove the ring or amulet at the given inventory slot, prompting
 * if slot is -1 (default).
 *
 * @param slot The slot to remove the ring from, or -1 if the player should
 *             be prompted.
 * @param announce If the item name should be used in the "It's cursed" message
 * @param noask Suppresses the "stat zero" prompt
 * @returns true if the ring was removed
 */
bool remove_ring(int slot, bool announce, bool noask)
{
    equipment_type hand_used = EQ_NONE;
    bool has_jewellery = false;
    bool has_melded = false;
    const vector<equipment_type> jewellery_slots = _current_jewellery_types();

    for (auto eq : jewellery_slots)
    {
        if (you.slot_item(eq))
        {
            if (has_jewellery || Options.jewellery_prompt)
            {
                // At least one other piece, which means we'll have to ask
                hand_used = EQ_NONE;
            }
            else
                hand_used = eq;

            has_jewellery = true;
        }
        else if (you.melded[eq])
            has_melded = true;
    }

    if (!has_jewellery)
    {
        if (has_melded)
            mpr("You aren't wearing any unmelded rings or amulets.");
        else
            mpr("You aren't wearing any rings or amulets.");

        return false;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    // If more than one equipment slot had jewellery, we need to figure out
    // which one to remove from.
    if (hand_used == EQ_NONE)
    {
        const int equipn =
            (slot == -1) ? prompt_invent_item("Remove which piece of jewellery?",
                                              menu_type::invlist,
                                              OBJ_JEWELLERY,
                                              OPER_REMOVE,
                                              invprompt_flag::no_warning
                                                  | invprompt_flag::hide_known)
                         : slot;

        if (prompt_failed(equipn))
            return false;

        hand_used = item_equip_slot(you.inv[equipn]);
        if (hand_used == EQ_NONE)
        {
            mpr("You aren't wearing that.");
            return false;
        }
        else if (you.inv[equipn].base_type != OBJ_JEWELLERY)
        {
            mpr("That isn't a piece of jewellery.");
            return false;
        }
    }

    if (you.equip[hand_used] == -1)
    {
        mpr("I don't think you really meant that.");
        return false;
    }
    else if (you.melded[hand_used])
    {
        mpr("You can't take that off while it's melded.");
        return false;
    }
    else if (hand_used == EQ_AMULET
        && you.equip[EQ_RING_AMULET] != -1)
    {
        // This can be removed in the future if more ring amulets are added.
        ASSERT(player_equip_unrand(UNRAND_FINGER_AMULET));

        mpr("The amulet cannot be taken off without first removing the ring!");
        return false;
    }

    if (!check_warning_inscriptions(you.inv[you.equip[hand_used]],
                                    OPER_REMOVE))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (you.inv[you.equip[hand_used]].cursed())
    {
        if (announce)
        {
            mprf("%s is stuck to you!",
                 you.inv[you.equip[hand_used]].name(DESC_YOUR).c_str());
        }
        else
            mpr("It's stuck to you!");

        return false;
    }

    const int removed_ring_slot = you.equip[hand_used];
    item_def &invitem = you.inv[removed_ring_slot];
    if (!noask && !_safe_to_remove_or_wear(invitem, true))
        return false;

    // Remove the ring.
    you.turn_is_over = true;
    if (hand_used == EQ_AMULET)
    {
        start_delay<EquipOffDelay>(ARMOUR_EQUIP_DELAY - 1, invitem);
        return true;
    }

#ifdef USE_SOUND
    parse_sound(REMOVE_JEWELLERY_SOUND);
#endif
    mprf("You remove %s.", invitem.name(DESC_YOUR).c_str());
#ifdef USE_TILE_LOCAL
    const unsigned int old_talents = your_talents(false).size();
#endif
    unequip_item(hand_used);
#ifdef USE_TILE_LOCAL
    if (your_talents(false).size() != old_talents)
    {
        tiles.layout_statcol();
        redraw_screen();
        update_screen();
    }
#endif

    you.time_taken /= 2;

    return true;
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

void drink(item_def* potion)
{
    if (!you.can_drink(false))
    {
        mpr("You can't drink.");
        return;
    }
    else if (!you.can_drink(true))
    {
        mpr("You cannot drink potions in your current state!");
        return;
    }
    else if (player_in_branch(BRANCH_COCYTUS))
    {
        mpr("It's too cold; everything's frozen solid!");
        return;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (!potion)
    {
        if (use_an_item(potion, OPER_QUAFF) == OPER_NONE)
            return;
        ASSERT(potion);
        if (potion->base_type == OBJ_SCROLLS)
        {
            read(potion);
            return;
        }
    }

    if (potion->base_type != OBJ_POTIONS)
    {
        mpr("You can't drink that!");
        return;
    }

    const bool alreadyknown = item_type_known(*potion);

    if (alreadyknown && is_bad_item(*potion))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return;
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
        return;
    }

    // The "> 1" part is to reduce the amount of times that Xom is
    // stimulated when you are a low-level 1 trying your first unknown
    // potions on monsters.
    const bool dangerous = (player_in_a_dangerous_place()
                            && you.experience_level > 1);

    if (player_under_penance(GOD_GOZAG) && one_chance_in(3))
    {
        simple_god_message(" petitions for your drink to fail.", GOD_GOZAG);
        you.turn_is_over = true;
        return;
    }

    // Check for Delatra's gloves before potentially melding them.
    bool heal_on_id = player_equip_unrand(UNRAND_DELATRAS_GLOVES);

    if (!quaff_potion(*potion))
        return;

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
}

// XXX: there's probably a nicer way of doing this.
bool god_hates_brand(const int brand)
{
    if (is_good_god(you.religion)
        && (brand == SPWPN_DRAINING
            || brand == SPWPN_VAMPIRISM
            || brand == SPWPN_CHAOS
            || brand == SPWPN_PAIN))
    {
        return true;
    }

    if (you_worship(GOD_CHEIBRIADOS) && (brand == SPWPN_CHAOS
                                         || brand == SPWPN_SPEED))
    {
        return true;
    }

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
                                               3, SPWPN_VENOM,
                                               3, SPWPN_VORPAL,
                                               1, SPWPN_ELECTROCUTION,
                                               1, SPWPN_CHAOS);
        }
        else
        {
            new_brand = random_choose_weighted(2, SPWPN_FLAMING,
                                               2, SPWPN_FREEZING,
                                               2, SPWPN_VORPAL,
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
    case SPWPN_VORPAL:
        flash_colour = YELLOW;
        mprf("%s emits a brilliant flash of light!",itname.c_str());
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
        // Message would spoil this even if we didn't identify.
        set_ident_flags(wpn, ISFLAG_KNOW_TYPE);
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

    auto success = use_an_item(target, OPER_ANY, selector, prompt,
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
    item_def* weapon = _scroll_choose_weapon(alreadyknown, pre_msg,
                                             SCR_BRAND_WEAPON);
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

    if (is_weapon(wpn)
        && !is_artefact(wpn)
        && wpn.base_type == OBJ_WEAPONS
        && wpn.plus < MAX_WPN_ENCHANT)
    {
        wpn.plus++;
        success = true;
        if (!quiet)
            mprf("%s glows red for a moment.", iname.c_str());
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
    item_def* itemp = _choose_target_item_for_scroll(alreadyknown, OSEL_UNIDENT,
                       "Identify which item? (\\ to view known items)");

    if (!itemp)
        return !alreadyknown;

    item_def& item = *itemp;
    if (alreadyknown)
        mpr(pre_msg);

    set_ident_type(item, true);
    set_ident_flags(item, ISFLAG_IDENT_MASK);

    // Output identified item.
    mprf_nocap("%s", menu_colour_item_name(item, DESC_INVENTORY_EQUIP).c_str());
    if (in_inventory(item))
    {
        if (item.link == you.equip[EQ_WEAPON])
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
    item_def* weapon = _scroll_choose_weapon(alreadyknown, pre_msg,
                                             SCR_ENCHANT_WEAPON);
    if (!weapon)
        return !alreadyknown;

    enchant_weapon(*weapon, false);
    return true;
}

bool enchant_armour(int &ac_change, bool quiet, item_def &arm)
{
    ASSERT(arm.defined());
    ASSERT(arm.base_type == OBJ_ARMOUR);

    ac_change = 0;

    // Cannot be enchanted.
    if (!is_enchantable_armour(arm))
    {
        if (!quiet)
            canned_msg(MSG_NOTHING_HAPPENS);
        return false;
    }

    // Output message before changing enchantment and curse status.
    if (!quiet)
    {
        const bool plural = armour_is_hide(arm)
                            && arm.sub_type != ARM_TROLL_LEATHER_ARMOUR;
        mprf("%s %s green for a moment.",
             _item_name(arm).c_str(),
             conjugate_verb("glow", plural).c_str());
    }

    arm.plus++;
    ac_change++;

    return true;
}

static int _handle_enchant_armour(bool alreadyknown, const string &pre_msg)
{
    item_def* target = _choose_target_item_for_scroll(alreadyknown, OSEL_ENCHANTABLE_ARMOUR,
                                                      "Enchant which item?");

    if (!target)
        return alreadyknown ? -1 : 0;

    // Okay, we may actually (attempt to) enchant something.
    if (alreadyknown)
        mpr(pre_msg);

    int ac_change;
    bool result = enchant_armour(ac_change, false, *target);

    if (ac_change)
        you.redraw_armour_class = true;

    return result ? 1 : 0;
}

static void _vulnerability_scroll()
{
    const int dur = 30 + random2(20);
    mon_enchant lowered_wl(ENCH_LOWERED_WL, 1, &you, dur * BASELINE_DELAY);

    // Go over all creatures in LOS.
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            // If relevant, monsters have their WL halved.
            if (!mons_invuln_will(*mon))
                mon->add_ench(lowered_wl);

            // Annoying but not enough to turn friendlies against you.
            if (!mon->wont_attack())
                behaviour_event(mon, ME_ANNOY, &you);
        }
    }

    you.set_duration(DUR_LOWERED_WL, dur, 0,
                     "Magic quickly surges around you.");
}

static bool _is_cancellable_scroll(scroll_type scroll)
{
    return scroll == SCR_IDENTIFY
           || scroll == SCR_BLINKING
           || scroll == SCR_ENCHANT_ARMOUR
           || scroll == SCR_AMNESIA
#if TAG_MAJOR_VERSION == 34
           || scroll == SCR_CURSE_ARMOUR
           || scroll == SCR_CURSE_JEWELLERY
           || scroll == SCR_RECHARGING
#endif
           || scroll == SCR_BRAND_WEAPON
           || scroll == SCR_ENCHANT_WEAPON
           || scroll == SCR_MAGIC_MAPPING
           || scroll == SCR_ACQUIREMENT
           || scroll == SCR_POISON;
}

/**
 * If the player has no items matching the given selector, give an appropriate
 * response to print. Otherwise, if they do have such items, return the empty
 * string.
 */
static string _no_items_reason(object_selector type, bool check_floor = false)
{
    if (!any_items_of_type(type, -1, check_floor))
        return no_selectables_message(type);
    return "";
}

/**
 * If the player is unable to (r)ead the item in the given slot, return the
 * reason why. Otherwise (if they are able to read it), returns "", the empty
 * string. If item is nullptr, do only general reading checks.
 */
string cannot_read_item_reason(const item_def *item)
{
    // general checks
    if (player_in_branch(BRANCH_GEHENNA))
        return "You cannot see clearly; the smoke and ash is too thick!";

    if (you.berserk())
        return "You are too berserk!";

    if (you.confused())
        return "You are too confused!";

    // no reading while threatened (Ru/random mutation)
    if (you.duration[DUR_NO_SCROLLS] || you.duration[DUR_BRAINLESS])
        return "You cannot read scrolls in your current state!";

    if (silenced(you.pos()))
        return "Magic scrolls do not work when you're silenced!";

    // water elementals
    if (you.duration[DUR_WATER_HOLD] && !you.res_water_drowning())
        return "You cannot read scrolls while unable to breathe!";

    if (!item)
        return "";

    // item-specific checks

    // still possible to use * at the `r` prompt. (Why do we allow this now?)
    if (item->base_type != OBJ_SCROLLS)
        return "You can't read that!";

    // don't waste the player's time reading known scrolls in situations where
    // they'd be useless

    if (!item_type_known(*item))
        return "";

    switch (item->sub_type)
    {
        case SCR_BLINKING:
        case SCR_TELEPORTATION:
            return you.no_tele_reason(item->sub_type == SCR_BLINKING);

        case SCR_AMNESIA:
            if (you.spell_no == 0)
                return "You have no spells to forget!";
            return "";

        case SCR_ENCHANT_ARMOUR:
            return _no_items_reason(OSEL_ENCHANTABLE_ARMOUR, true);

        case SCR_ENCHANT_WEAPON:
            return _no_items_reason(OSEL_ENCHANTABLE_WEAPON, true);

        case SCR_IDENTIFY:
            return _no_items_reason(OSEL_UNIDENT, true);

        case SCR_SUMMONING:
        case SCR_BUTTERFLIES:
            if (you.allies_forbidden())
                return "You cannot coerce anything to answer your summons.";
            return "";

#if TAG_MAJOR_VERSION == 34
        case SCR_CURSE_WEAPON:
            if (!you.weapon())
                return "This scroll only affects a wielded weapon!";

            // assumption: wielded weapons always have their curse & brand known
            if (you.weapon()->cursed())
                return "Your weapon is already cursed!";

            if (get_weapon_brand(*you.weapon()) == SPWPN_HOLY_WRATH)
                return "Holy weapons cannot be cursed!";
            return "";

        case SCR_CURSE_ARMOUR:
            return _no_items_reason(OSEL_UNCURSED_WORN_ARMOUR);

        case SCR_CURSE_JEWELLERY:
            return _no_items_reason(OSEL_UNCURSED_WORN_JEWELLERY);
#endif

        default:
            return "";
    }
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
    return m.alive() && scr == SCR_TORMENT && !m.res_torment();
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
void read(item_def* scroll, dist *target)
{
    string failure_reason = cannot_read_item_reason(scroll);

    if (!scroll && failure_reason.empty())
    {
        // player can currently read, but no scroll was provided
        if (use_an_item(scroll, OPER_READ) == OPER_NONE)
            return;
        ASSERT(scroll);
        if (scroll->base_type == OBJ_POTIONS)
        {
            drink(scroll);
            return;
        }
        failure_reason = cannot_read_item_reason(scroll);
    }

    if (!failure_reason.empty())
    {
        mprf(MSGCH_PROMPT, "%s", failure_reason.c_str());
        return;
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
            return;
        }
        else if (penance && !yesno(penance_prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return;
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
            return;
        }
        else if (!bad_item && !hostile_check && !yesno(make_stringf(
            "You can't see any enemies this would affect, really %s?",
                                        verb_object.c_str()).c_str(),
                                                false, 'n'))
        {
            // is this too nanny dev?
            canned_msg(MSG_OK);
            return;
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
        return;
    }

    // For cancellable scrolls leave printing this message to their
    // respective functions.
    const string pre_succ_msg =
            make_stringf("As you read the %s, it crumbles to dust.",
                          scroll->name(DESC_QUALNAME).c_str());
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
        set_ident_type(*scroll, true);

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
        if (alreadyknown && (env.level_state & LSTATE_STILL_WINDS))
        {
            mpr("The air is too still for clouds to form.");
            cancel_scroll = true;
            break;
        }
        mpr("The scroll dissolves into smoke.");
        auto smoke = random_smoke_type();
        big_cloud(smoke, &you, you.pos(), 50, 8 + random2(8));
        break;
    }

    case SCR_MAGIC_MAPPING:
        if (alreadyknown && !is_map_persistent())
        {
            cancel_scroll = true;
            mpr("It would have no effect in this place.");
            break;
        }
        mpr(pre_succ_msg);
        magic_mapping(500, 100, false);
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
            set_ident_type(*scroll, true);
        }
        cancel_scroll = !_identify(alreadyknown, pre_succ_msg, link);
        break;

    case SCR_ENCHANT_ARMOUR:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of enchant armour.");
            // included in default force_more_message (to show it before menu)
        }
        cancel_scroll =
            (_handle_enchant_armour(alreadyknown, pre_succ_msg) == -1);
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

    set_ident_type(*scroll, true);
    set_ident_flags(*scroll, ISFLAG_KNOW_TYPE); // for notes

    string scroll_name = scroll->name(DESC_QUALNAME);

    if (!cancel_scroll)
    {
        if (in_inventory(*scroll))
            dec_inv_item_quantity(link, 1);
        else
            dec_mitm_item_quantity(scroll->index(), 1);
        count_action(CACT_USE, OBJ_SCROLLS);
        if (you.has_mutation(MUT_AWKWARD_TONGUE))
            you.time_taken = div_rand_round(you.time_taken * 3, 2);
    }

    if (!alreadyknown
        && which_scroll != SCR_BRAND_WEAPON
        && which_scroll != SCR_ENCHANT_WEAPON
        && which_scroll != SCR_IDENTIFY
        && which_scroll != SCR_ENCHANT_ARMOUR
#if TAG_MAJOR_VERSION == 34
        && which_scroll != SCR_RECHARGING
#endif
        && which_scroll != SCR_AMNESIA
        && which_scroll != SCR_ACQUIREMENT)
    {
        mprf("It %s a %s.",
             scroll->quantity < prev_quantity ? "was" : "is",
             scroll_name.c_str());
    }

    if (!alreadyknown)
    {
        if (player_equip_unrand(UNRAND_DELATRAS_GLOVES))
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

    if (!alreadyknown)
        auto_assign_item_slot(*scroll);

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

    if (part)
    {
        pickup_menu(idx);
        return;
    }
    pickup_single_item(idx, -1);
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

void tile_item_use_secondary(int idx)
{
    const item_def item = you.inv[idx];

    // TODO: add quiver stuff here?
    if (you.equip[EQ_WEAPON] == idx)
        wield_weapon(true, SLOT_BARE_HANDS);
    else if (item_is_wieldable(item))
    {
        // secondary wield for several spells and such
        wield_weapon(true, idx); // wield
    }
}

void tile_item_use(int idx)
{
    const item_def item = you.inv[idx];

    // Equipped?
    bool equipped = false;
    bool equipped_weapon = false;
    for (unsigned int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; i++)
    {
        if (you.equip[i] == idx)
        {
            equipped = true;
            if (i == EQ_WEAPON)
                equipped_weapon = true;
            break;
        }
    }

    // Special case for folks who are wielding something
    // that they shouldn't be wielding.
    // Note that this is only a problem for equipables
    // (otherwise it would only waste a turn)
    if (you.equip[EQ_WEAPON] == idx
        && (item.base_type == OBJ_ARMOUR
            || item.base_type == OBJ_JEWELLERY))
    {
        wield_weapon(true, SLOT_BARE_HANDS);
        return;
    }

    const int type = item.base_type;

    // Use it
    switch (type)
    {
    case OBJ_WEAPONS:
    case OBJ_STAVES:
    case OBJ_MISCELLANY:
    case OBJ_WANDS:
        // Wield any unwielded item of these types.
        if (!equipped && item_is_wieldable(item))
        {
            wield_weapon(true, idx);
            return;
        }
        // Evoke misc. items or wands.
        if (item_is_evokable(item, false))
        {
            evoke_item(idx);
            return;
        }
        // Unwield wielded items.
        if (equipped)
            wield_weapon(true, SLOT_BARE_HANDS);
        return;

    case OBJ_MISSILES:
        if (check_warning_inscriptions(item, OPER_FIRE))
            quiver::slot_to_action(idx)->trigger(); // TODO: anything more interesting?
        return;

    case OBJ_ARMOUR:
        if (equipped && !equipped_weapon)
            takeoff_armour(idx);
        else
            wear_armour(idx);
        return;

    case OBJ_SCROLLS:
        if (check_warning_inscriptions(item, OPER_READ))
            read(&you.inv[idx]);
        return;

    case OBJ_JEWELLERY:
        if (equipped && !equipped_weapon)
            remove_ring(idx);
        else
            puton_ring(idx);
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
