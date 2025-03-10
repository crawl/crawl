/**
 * @file
 * @brief Shop keeper functions.
**/

#include "AppHdr.h"

#include "shopping.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "artefact.h"
#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "describe.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h"
#include "files.h"
#include "god-wrath.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "menu.h"
#include "message.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "prompt.h"
#include "spl-book.h"
#include "stash.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#ifdef USE_TILE_LOCAL
#include "tilereg-crt.h"
#endif
#include "travel.h"
#include "unicode.h"
#include "unwind.h"

ShoppingList shopping_list;

static int _shop_get_item_value(const item_def& item, int greed, bool id)
{
    int result = (greed * item_value(item, id) / 10);

    return max(result, 1);
}

int item_price(const item_def& item, const shop_struct& shop)
{
    return _shop_get_item_value(item, shop.greed, shoptype_identifies_stock(shop.type));
}

// This probably still needs some work. Rings used to be the only
// artefacts which had a change in price, and that value corresponds
// to returning 50 from this function. Good artefacts will probably
// be returning just over 30 right now. Note that this isn't used
// as a multiple, its used in the old ring way: 7 * ret is added to
// the price of the artefact. -- bwr
int artefact_value(const item_def &item)
{
    ASSERT(is_artefact(item));

    int ret = 10;
    artefact_properties_t prop;
    artefact_properties(item, prop);

    // Brands are already accounted for via existing ego checks

    // This should probably be more complex... but this isn't so bad:
    ret += 6 * prop[ARTP_AC]
           + 6 * prop[ARTP_EVASION]
           + 5 * prop[ARTP_SHIELDING]
           + 6 * prop[ARTP_SLAYING]
           + 3 * prop[ARTP_STRENGTH]
           + 3 * prop[ARTP_INTELLIGENCE]
           + 3 * prop[ARTP_DEXTERITY]
           + 4 * prop[ARTP_HP]
           + 3 * prop[ARTP_MAGICAL_POWER];

    // These resistances have meaningful levels
    if (prop[ARTP_FIRE] > 0)
        ret += 5 + 5 * (prop[ARTP_FIRE] * prop[ARTP_FIRE]);
    else if (prop[ARTP_FIRE] < 0)
        ret -= 10;

    if (prop[ARTP_COLD] > 0)
        ret += 5 + 5 * (prop[ARTP_COLD] * prop[ARTP_COLD]);
    else if (prop[ARTP_COLD] < 0)
        ret -= 10;

    if (prop[ARTP_WILLPOWER] > 0)
        ret += 4 + 4 * (prop[ARTP_WILLPOWER] * prop[ARTP_WILLPOWER]);
    else if (prop[ARTP_WILLPOWER] < 0)
        ret -= 6;

    if (prop[ARTP_NEGATIVE_ENERGY] > 0)
        ret += 3 + 3 * (prop[ARTP_NEGATIVE_ENERGY] * prop[ARTP_NEGATIVE_ENERGY]);

    // Discount Stlth-, charge for Stlth+
    ret += 2 * prop[ARTP_STEALTH];
    // Stlth+ costs more than Stlth- cheapens
    if (prop[ARTP_STEALTH] > 0)
        ret += 2 * prop[ARTP_STEALTH];

    // only one meaningful level:
    if (prop[ARTP_POISON])
        ret += 6;

    // only one meaningful level (hard to get):
    if (prop[ARTP_ELECTRICITY])
        ret += 10;

    // only one meaningful level (hard to get):
    if (prop[ARTP_RCORR])
        ret += 8;

    // only one meaningful level (hard to get):
    if (prop[ARTP_RMUT])
        ret += 8;

    if (prop[ARTP_SEE_INVISIBLE])
        ret += 6;

    // abilities:
    if (prop[ARTP_FLY])
        ret += 3;

    if (prop[ARTP_BLINK])
        ret += 10;

    if (prop[ARTP_INVISIBLE])
        ret += 10;

    if (prop[ARTP_ANGRY])
        ret -= 3;

    if (prop[ARTP_NOISE])
        ret -= 5;

    if (prop[ARTP_PREVENT_TELEPORTATION])
        ret -= 10;

    if (prop[ARTP_PREVENT_SPELLCASTING])
        ret -= 10;

    if (prop[ARTP_CONTAM])
        ret -= 8;

    if (prop[ARTP_CORRODE])
        ret -= 8;

    if (prop[ARTP_DRAIN])
        ret -= 8;

    if (prop[ARTP_SLOW])
        ret -= 8;

    if (prop[ARTP_SILENCE])
        ret -= 8;

    if (prop[ARTP_FRAGILE])
        ret -= 8;

    if (prop[ARTP_RMSL])
        ret += 20;

    if (prop[ARTP_CLARITY])
        ret += 20;

    if (prop[ARTP_ARCHMAGI])
        ret += 20;

    // Yuck!
    for (int i = ARTP_ENHANCE_CONJ; i <= ARTP_ENHANCE_ALCHEMY; ++i)
        if (prop[i])
            ret += 8;

    if (prop[ARTP_ENHANCE_FORGECRAFT])
        ret += 8;

    return (ret > 0) ? ret : 0;
}

bool have_voucher()
{
    if (you.attribute[ATTR_VOUCHER] > 0)
            return true;
    return false;
}

unsigned int item_value(item_def item, bool ident)
{
    // Note that we pass item in by value, since we want a local
    // copy to mangle as necessary.
    item.flags = (ident) ? (item.flags | ISFLAG_IDENTIFIED) : (item.flags);

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);
        if (entry->value != 0)
            return entry->value;
    }

    int valued = 0;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        valued += weapon_base_price((weapon_type)item.sub_type);

        if (item.is_identified())
        {
            switch (get_weapon_brand(item))
            {
            case SPWPN_NORMAL:
            default:            // randart
                valued *= 10;
                break;

            case SPWPN_SPEED:
            case SPWPN_VAMPIRISM:
            case SPWPN_ANTIMAGIC:
                valued *= 30;
                break;

            case SPWPN_DISTORTION:
            case SPWPN_ELECTROCUTION:
            case SPWPN_PAIN:
            case SPWPN_ACID: // Unrand-only.
            case SPWPN_FOUL_FLAME: // Unrand only.
            case SPWPN_PENETRATION: // Unrand-only.
            case SPWPN_SPECTRAL:
                valued *= 25;
                break;

            case SPWPN_CHAOS:
            case SPWPN_DRAINING:
            case SPWPN_FLAMING:
            case SPWPN_FREEZING:
            case SPWPN_HEAVY:
            case SPWPN_HOLY_WRATH:
                valued *= 18;
                break;

            case SPWPN_PROTECTION:
            case SPWPN_VENOM:
                valued *= 12;
                break;
            }

            valued /= 10;
        }

        if (item.is_identified())
            valued += 50 * item.plus;

        if (is_artefact(item))
        {
            if (item.is_identified())
                valued += (7 * artefact_value(item));
            else
                valued += 50;
        }
        else if (item.is_identified()
                 && get_equip_desc(item) != 0) // ???
        {
            valued += 20;
        }
        else if (!item.is_identified() && (get_equip_desc(item) != 0))
            valued += 30; // un-id'd "glowing" - arbitrary added cost

        break;

    case OBJ_MISSILES:          // ammunition
        valued += missile_base_price((missile_type)item.sub_type);

        if (item.is_identified())
        {
            switch (get_ammo_brand(item))
            {
            case SPMSL_NORMAL:
            default:
                break;

            case SPMSL_CHAOS:
            case SPMSL_CURARE:
                valued *= 4;
                break;

            case SPMSL_BLINDING:
            case SPMSL_FRENZY:
            case SPMSL_SILVER:
            case SPMSL_DISPERSAL:
            case SPMSL_DISJUNCTION:
#if TAG_MAJOR_VERSION == 34
            case SPMSL_PARALYSIS:
            case SPMSL_PENETRATION:
            case SPMSL_STEEL:
            case SPMSL_FLAME:
            case SPMSL_FROST:
            case SPMSL_SLEEP:
            case SPMSL_CONFUSION:
                valued *= 3;
                break;
#endif

            case SPMSL_POISONED:
#if TAG_MAJOR_VERSION == 34
            case SPMSL_RETURNING:
            case SPMSL_EXPLODING:
            case SPMSL_SLOW:
            case SPMSL_SICKNESS:
#endif
                valued *= 2;
                break;
            }
        }
        break;

    case OBJ_ARMOUR:
        valued += armour_base_price((armour_type)item.sub_type);

        if (item.is_identified())
        {
            const int sparm = get_armour_ego_type(item);
            switch (sparm)
            {
            case SPARM_ARCHMAGI:
            case SPARM_RESISTANCE:
                valued += 250;
                break;

            case SPARM_COLD_RESISTANCE:
            case SPARM_DEXTERITY:
            case SPARM_FIRE_RESISTANCE:
            case SPARM_SEE_INVISIBLE:
            case SPARM_INTELLIGENCE:
            case SPARM_FLYING:
            case SPARM_STEALTH:
            case SPARM_STRENGTH:
            case SPARM_INVISIBILITY:
            case SPARM_WILLPOWER:
            case SPARM_PROTECTION:
            case SPARM_HURLING:
            case SPARM_REPULSION:
            case SPARM_PRESERVATION:
            case SPARM_SHADOWS:
            case SPARM_RAMPAGING:
            case SPARM_INFUSION:
            case SPARM_LIGHT:
            case SPARM_ENERGY:
                valued += 50;
                break;

            case SPARM_POSITIVE_ENERGY:
            case SPARM_POISON_RESISTANCE:
            case SPARM_REFLECTION:
            case SPARM_SPIRIT_SHIELD:
            case SPARM_HARM:
            case SPARM_RAGE:
            case SPARM_MAYHEM:
            case SPARM_GUILE:
                valued += 20;
                break;

            case SPARM_PONDEROUSNESS:
                valued -= 250;
                break;
            }
        }

        if (item.is_identified())
            valued += 50 * item.plus;

        if (is_artefact(item))
        {
            if (item.is_identified())
                valued += (7 * artefact_value(item));
            else
                valued += 50;
        }
        else if (item.is_identified() && get_equip_desc(item) != 0)
            valued += 20;  // ???
        else if (!item.is_identified() && (get_equip_desc(item) != 0))
            valued += 30; // un-id'd "glowing" - arbitrary added cost

        break;

    case OBJ_WANDS:
        if (!item.is_identified())
            valued += 40;
        else
        {
            switch (item.sub_type)
            {
            case WAND_ACID:
            case WAND_LIGHT:
            case WAND_QUICKSILVER:
            case WAND_DIGGING:
                valued += 42 * item.plus;
                break;

            case WAND_ICEBLAST:
            case WAND_ROOTS:
            case WAND_WARPING:
            case WAND_CHARMING:
            case WAND_PARALYSIS:
                valued += 24 * item.plus;
                break;

            case WAND_POLYMORPH:
            case WAND_MINDBURST:
                valued += 14 * item.plus;
                break;

            case WAND_FLAME:
                valued += 7 * item.plus;
                break;

            default:
                valued += 4 * item.plus;
                break;
            }
        }
        break;

    case OBJ_POTIONS:
        if (!item.is_identified())
            valued += 9;
        else
        {
            switch (item.sub_type)
            {
            case POT_EXPERIENCE:
                valued += 500;
                break;

            case POT_RESISTANCE:
            case POT_HASTE:
                valued += 100;
                break;

            case POT_MAGIC:
            case POT_INVISIBILITY:
            case POT_CANCELLATION:
            case POT_AMBROSIA:
            case POT_MUTATION:
                valued += 80;
                break;

            case POT_BERSERK_RAGE:
            case POT_HEAL_WOUNDS:
            case POT_ENLIGHTENMENT:
                valued += 50;
                break;

            case POT_MIGHT:
            case POT_BRILLIANCE:
                valued += 40;
                break;

            case POT_CURING:
            case POT_LIGNIFY:
            case POT_ATTRACTION:
                valued += 30;
                break;

            case POT_MOONSHINE:
                valued += 10;
                break;

            CASE_REMOVED_POTIONS(item.sub_type)
            }
        }
        break;

    case OBJ_SCROLLS:
        if (!item.is_identified())
            valued += 10;
        else
        {
            switch (item.sub_type)
            {
            case SCR_ACQUIREMENT:
                valued += 520;
                break;

            case SCR_SUMMONING:
            case SCR_TORMENT:
            case SCR_SILENCE:
            case SCR_BRAND_WEAPON:
                valued += 95;
                break;

            case SCR_BLINKING:
            case SCR_ENCHANT_ARMOUR:
            case SCR_ENCHANT_WEAPON:
            case SCR_REVELATION:
                valued += 75;
                break;

            case SCR_AMNESIA:
            case SCR_FEAR:
            case SCR_IMMOLATION:
            case SCR_POISON:
            case SCR_VULNERABILITY:
            case SCR_FOG:
            case SCR_BUTTERFLIES:
                valued += 40;
                break;

            case SCR_TELEPORTATION:
                valued += 30;
                break;

            case SCR_IDENTIFY:
#if TAG_MAJOR_VERSION == 34
            case SCR_CURSE_ARMOUR:
            case SCR_CURSE_WEAPON:
            case SCR_CURSE_JEWELLERY:
            case SCR_HOLY_WORD:
#endif
                valued += 20;
                break;

            case SCR_NOISE:
                valued += 10;
                break;
            }
        }
        break;

    case OBJ_JEWELLERY:
        if (!item.is_identified())
            valued += 50;
        else
        {
            // Variable-strength rings.
            if (jewellery_type_has_plusses(item.sub_type))
            {
                // Formula: price = 5n(n+1)
                // n is the power. (The base variable is equal to n.)
                int base = 0;

                switch (item.sub_type)
                {
                case RING_SLAYING:
                case RING_PROTECTION:
                    base = 2 * item.plus;
                    break;
                case RING_EVASION:
                    base = 8 * item.plus / 5;
                    break;
                case RING_STRENGTH:
                case RING_DEXTERITY:
                case RING_INTELLIGENCE:
                    base = item.plus * 4 / 3;
                    break;
                default:
                    break;
                }

                if (base <= 0)
                    valued += 25 * base;
                else
                    valued += 5 * base * (base + 1);
            }
            else
            {
                switch (item.sub_type)
                {
                case AMU_FAITH:
                    valued += 400;
                    break;

                case RING_WIZARDRY:
                case AMU_REGENERATION:
                case AMU_GUARDIAN_SPIRIT:
                case AMU_MANA_REGENERATION:
                case AMU_ACROBAT:
                case AMU_REFLECTION:
                    valued += 300;
                    break;

                case RING_FIRE:
                case RING_ICE:
                case RING_PROTECTION_FROM_COLD:
                case RING_PROTECTION_FROM_FIRE:
                case RING_WILLPOWER:
                    valued += 250;
                    break;

                case RING_MAGICAL_POWER:
                case RING_POSITIVE_ENERGY:
                case RING_POISON_RESISTANCE:
                case RING_RESIST_CORROSION:
                    valued += 200;
                    break;

                case RING_STEALTH:
                case RING_FLIGHT:
                    valued += 175;
                    break;

                case RING_SEE_INVISIBLE:
                    valued += 150;
                    break;

                case AMU_NOTHING:
                    valued += 75;
                    break;
                }
            }

            if (is_artefact(item))
            {
                // in this branch we're guaranteed to know
                // the item type!
                if (valued < 0)
                    valued = (artefact_value(item) - 5) * 7;
                else
                    valued += artefact_value(item) * 7;
            }

            // Hard minimum, as it's worth 20 to ID a ring.
            valued = max(20, valued);
        }
        break;

    case OBJ_MISCELLANY:
        switch (item.sub_type)
        {
        case MISC_HORN_OF_GERYON:
        case MISC_ZIGGURAT:
        case MISC_SHOP_VOUCHER:
            valued += 5000;
            break;

        case MISC_PHIAL_OF_FLOODS:
        case MISC_TIN_OF_TREMORSTONES:
        case MISC_BOX_OF_BEASTS:
        case MISC_SACK_OF_SPIDERS:
        case MISC_CONDENSER_VANE:
        case MISC_PHANTOM_MIRROR:
        case MISC_LIGHTNING_ROD:
        case MISC_GRAVITAMBOURINE:
            valued += 400;
            break;

        default:
            valued += 200;
        }
        break;

    case OBJ_TALISMANS:
        // These are all pretty arbitrary.
        switch (item.sub_type)
        {
        case TALISMAN_DEATH:
        case TALISMAN_STORM:
            valued += 800;
            break;

        case TALISMAN_DRAGON:
        case TALISMAN_STATUE:
        case TALISMAN_VAMPIRE:
            valued += 600;
            break;

        case TALISMAN_MAW:
        case TALISMAN_SERPENT:
        case TALISMAN_BLADE:
            valued += 300;
            break;

        case TALISMAN_FLUX:
            valued += 250;
            break;

        case TALISMAN_BEAST:
        default:
            valued += 200;
            break;
        }
        if (is_artefact(item))
        {
            // XXX placeholder
            if (item.is_identified())
                valued += artefact_value(item) * (valued / 10);
            else
                valued += valued / 16;
        }

        break;

    case OBJ_BOOKS:
    {
        valued = 0;
        const book_type book = static_cast<book_type>(item.sub_type);
        if (book == BOOK_MANUAL)
            return 800;
#if TAG_MAJOR_VERSION == 34
        if (book == BOOK_BUGGY_DESTRUCTION)
            break;
#endif
        int levels = 0;
        const vector<spell_type> spells = spells_in_book(item);
        for (spell_type spell : spells)
            levels += spell_difficulty(spell);
        // Level 9 spells are worth 4x level 1 spells.
        valued += levels * 20 + spells.size() * 20;
        break;
    }

    case OBJ_STAVES:
        valued = item.is_identified() ? 250 : 120;
        if (is_artefact(item))
        {
            // XX placeholder
            if (item.is_identified())
                valued += (7 * artefact_value(item));
            else
                valued += 50;
        }
        break;

    case OBJ_ORBS:
        valued = 250000;
        break;

    case OBJ_RUNES:
    case OBJ_GEMS:
        valued = 10000;
        break;

    default:
        break;
    }                           // end switch

    if (valued < 1)
        valued = 1;

    valued = stepdown_value(valued, 1000, 1000, 10000, 10000);

    return item.quantity * valued;
}

bool is_worthless_consumable(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_POTIONS:
        switch (item.sub_type)
        {
        case POT_MOONSHINE:
            return true;
        default:
            return false;
        CASE_REMOVED_POTIONS(item.sub_type)
        }
    case OBJ_SCROLLS:
        return item.sub_type == SCR_NOISE;

    // Only consumables are worthless.
    default:
        return false;
    }
}

static int _count_identical(const vector<item_def>& stock, const item_def& item)
{
    int count = 0;
    for (const item_def& other : stock)
        if (ShoppingList::items_are_same(item, other))
            count++;

    return count;
}

/** Buy an item from a shop!
 *
 *  @param shop  the shop to purchase from.
 *  @param pos   where the shop is located
 *  @param index the index of the item to buy in shop.stock
 *  @param cost  the price to deduct for this item
 *  @param voucher use a voucher instead of paying the item cost
 *  @returns true if it went in your inventory, false otherwise.
 */
static bool _purchase(shop_struct& shop, const level_pos& pos, int index,
                      int cost, bool voucher)
{
    item_def item = shop.stock[index]; // intentional copy
    shop.stock.erase(shop.stock.begin() + index);

    // Remove from shopping list if it's unique
    // (i.e., if the shop has multiple scrolls of
    // identify, don't remove the other scrolls
    // from the shopping list if there's any
    // left).
    if (shopping_list.is_on_list(item, &pos)
        && _count_identical(shop.stock, item) == 0)
    {
        shopping_list.del_thing(item, &pos);
    }

    // Take a note of the purchase.
    take_note(Note(NOTE_BUY_ITEM, cost, 0,
                   item.name(DESC_A).c_str()));

    // But take no further similar notes.
    item.flags |= ISFLAG_NOTED_GET;

    if (item.is_identified())
        item.flags |= ISFLAG_NOTED_ID;

    // Record milestones for purchasing especially notable items (runes,
    // gems, the Orb).
    milestone_check(item);

    if (!voucher)
        you.del_gold(cost);
    else
        you.attribute[ATTR_VOUCHER]--;

    you.attribute[ATTR_PURCHASES] += cost;

    origin_purchased(item);

    // Unidentified potions/scrolls are the only shop items that don't become
    // auto-ID'd when the player purchases them. (But identified potions/scrolls
    // should still be re-identified, so the player can potentially learn their
    // type.)
    if ((item.base_type != OBJ_POTIONS && item.base_type != OBJ_SCROLLS)
         || item.is_identified())

    {
        // (Re-)identify the item. This also takes the ID note if necessary.
        item.flags &= ~ISFLAG_IDENTIFIED;
        identify_item(item);
    }

    // Shopkeepers will place goods you can't carry outside the shop.
    if (item_is_stationary(item)
        || !move_item_to_inv(item))
    {
        copy_item_to_grid(item, shop.pos);
        return false;
    }
    return true;
}

enum shopping_order
{
    ORDER_TYPE,
    ORDER_DEFAULT = ORDER_TYPE,
    ORDER_PRICE,
    ORDER_ALPHABETICAL,
    NUM_ORDERS
};

static const char * const shopping_order_names[NUM_ORDERS] =
{
    "type", "price", "name"
};

static shopping_order operator++(shopping_order &x)
{
    x = static_cast<shopping_order>(x + 1);
    if (x == NUM_ORDERS)
        x = ORDER_DEFAULT;
    return x;
}

class ShopMenu : public InvMenu
{
    friend class ShopEntry;

    shop_struct& shop;
    shopping_order order = ORDER_DEFAULT;
    level_pos pos;
    bool can_purchase;

    int outside_items;
    vector<int> bought_indices;

    int selected_cost(bool use_shopping_list=false) const;
    int max_cost() const;

    void init_entries();
    void update_help();
    void resort();
    void purchase_selected();

    bool cycle_mode(bool) override;

    virtual bool process_key(int keyin) override;
    bool process_command(command_type cmd) override;

protected:
    void select_item_index(int idx, int qty = MENU_SELECT_INVERT) override;
    bool examine_index(int i) override;
    bool skip_process_command(int keyin) override;

public:
    bool bought_something = false;

    ShopMenu(shop_struct& _shop, const level_pos& _pos, bool _can_purchase);
};

class ShopEntry : public InvEntry
{
    ShopMenu& menu;

    string get_text() const override
    {
        const int total_cost = menu.selected_cost();
        const bool on_list = shopping_list.is_on_list(*item, &menu.pos);
        // Colour stock as follows:
        //  * lightcyan, if on the shopping list and not selected.
        //  * lightred, if you can't buy all you selected.
        //  * lightgreen, if this item is purchasable along with your selections
        //  * red, if this item is not purchasable even by itself.
        //  * yellow, if this item would be purchasable if you deselected
        //            something else.

        // Is this too complicated? (jpeg)
        const colour_t keycol =
            !selected() && on_list              ? LIGHTCYAN :
            selected() && total_cost > you.gold ? LIGHTRED  :
            cost <= you.gold - total_cost       ? LIGHTGREEN :
            cost > you.gold                     ? RED :
                                                  YELLOW;
        const string keystr = colour_to_str(keycol);
        const string itemstr =
            colour_to_str(menu_colour(text, item_prefix(*item, false), tag, false));
        return make_stringf(" <%s>%c %c </%s><%s>%4d gold   %s%s</%s>",
                            keystr.c_str(),
                            hotkeys[0],
                            selected() ? '+' : on_list ? '$' : '-',
                            keystr.c_str(),
                            itemstr.c_str(),
                            cost,
                            text.c_str(),
                            shop_item_unknown(*item) ? " (unknown)" : "",
                            itemstr.c_str());
    }

    virtual void select(int qty = -1) override
    {
        if (shopping_list.is_on_list(*item, &menu.pos) && qty != 0)
            shopping_list.del_thing(*item, &menu.pos);

        InvEntry::select(qty);
    }
public:
    ShopEntry(const item_def& i, ShopMenu& m)
        : InvEntry(i),
          menu(m), cost(item_price(i, m.shop))
    {
        show_background = false;
    }
    const int cost;
};

// XX why is this MF_QUIET_SELECT?
ShopMenu::ShopMenu(shop_struct& _shop, const level_pos& _pos, bool _can_purchase)
    : InvMenu(MF_MULTISELECT | MF_QUIET_SELECT
                | MF_ALLOW_FORMATTING | MF_INIT_HOVER),
      shop(_shop),
      pos(_pos),
      can_purchase(_can_purchase)
{
    menu_action = can_purchase ? ACT_EXECUTE : ACT_EXAMINE;
    set_flags(get_flags() & ~MF_USE_TWO_COLUMNS);

    set_tag("shop");

    init_entries();
    resort();

    outside_items = 0;
    bought_indices = {};
    update_help();

    set_title("Welcome to " + shop_name(shop) + "! What would you "
              "like to do?");
}

void ShopMenu::init_entries()
{
    menu_letter ckey = 'a';
    for (item_def& item : shop.stock)
    {
        auto newentry = make_unique<ShopEntry>(item, *this);
        newentry->hotkeys.clear();
        newentry->add_hotkey(ckey++);
        add_entry(std::move(newentry));
    }
}

int ShopMenu::selected_cost(bool use_shopping_list) const
{
    int cost = 0;
    for (auto item : selected_entries())
        cost += dynamic_cast<ShopEntry*>(item)->cost;
    if (use_shopping_list && cost == 0)
    {
        for (auto item : items)
        {
            auto e = dynamic_cast<ShopEntry*>(item);
            if (shopping_list.is_on_list(*e->item, &pos))
                cost += e->cost;
        }
    }
    return cost;
}

int ShopMenu::max_cost() const
{
    int cost = 0;
    for (auto item : selected_entries())
        cost = max(cost, dynamic_cast<ShopEntry*>(item)->cost);

    return cost;
}

void ShopMenu::update_help()
{
    // TODO: convert to a regular keyhelp, make less painful

    //You have 2000 gold pieces. After the purchase, you will have 1802 gold pieces.
    //[Esc] exit          [Tab] buy|examine items     [a-j] mark item for purchase
    //[/] sort (type)     [Enter] buy marked items    [A-J] put item on shopping list
    string top_line = make_stringf("<yellow>You have %d gold piece%s.",
                                   you.gold,
                                   you.gold != 1 ? "s" : "");
    const int total_cost = !can_purchase ? 0 : selected_cost(true);
    if (total_cost > you.gold)
    {
        int max = max_cost();
        if (have_voucher() && total_cost - max <= you.gold)
        {
            top_line += "<lightred>";
            top_line +=
                make_stringf(" Purchasing will use your shop voucher and %d gold piece%s.",
                             total_cost - max,
                             (total_cost - max != 1) ? "s" : "");
            top_line += "</lightred>";
        }
        else
        {
            top_line += "<lightred>";
            top_line +=
                make_stringf(" You are short %d gold piece%s for the purchase.",
                             total_cost - you.gold,
                             (total_cost - you.gold != 1) ? "s" : "");
            top_line += "</lightred>";
        }
    }
    else if (total_cost)
    {
        top_line +=
            make_stringf(" After the purchase, you will have %d gold piece%s.",
                         you.gold - total_cost,
                         (you.gold - total_cost != 1) ? "s" : "");
    }
    top_line += "</yellow>";

    // Ensure length >= 80ch, which prevents the local tiles menu from resizing
    // as the player selects/deselects entries. Blegh..
    int top_line_width = strwidth(formatted_string::parse_string(top_line).tostring());
    top_line += string(max(0, 80 - top_line_width), ' ') + '\n';

    const bool no_selection = selected_entries().empty();
    const bool from_shopping_list = no_selection && total_cost > 0;
    const bool no_action = menu_action == ACT_EXECUTE
                                && (!can_purchase ||
                                    no_selection && !from_shopping_list)
                || menu_action == ACT_EXAMINE && !is_set(MF_ARROWS_SELECT);
    const string action_key = no_action ? "       " // max: "[Enter]"
        : menu_keyhelp_cmd(CMD_MENU_ACCEPT_SELECTION);
    const string action_desc = action_key +
            (no_action                   ? "                  "
            : menu_action == ACT_EXAMINE ? " describe         "
            : from_shopping_list         ? " buy shopping list"
                                         : " buy marked items ");

    // XX swap shopping list / select by letter?
    const string mode_desc = !can_purchase ? ""
        : menu_keyhelp_cmd(CMD_MENU_CYCLE_MODE)
            + (menu_action == ACT_EXECUTE ? " <w>buy</w>|examine items"
                                          : " buy|<w>examine</w> items");
    string m = menu_keyhelp_cmd(CMD_MENU_EXIT)+ " exit          ";
    m += mode_desc;
    m = pad_more_with(m, make_stringf("%s %s",
        hyphenated_hotkey_letters(item_count(), 'a').c_str(),
        menu_action == ACT_EXECUTE ? "mark item for purchase   "
                                   : "examine item             "));
    m += make_stringf("\n[<w>/</w>] sort (%s)%s  %s",
        shopping_order_names[order],
        string(7 - strwidth(shopping_order_names[order]), ' ').c_str(),
        action_desc.c_str());

    m = pad_more_with(m, hyphenated_hotkey_letters(item_count(), 'A')
                                  + " put item on shopping list");


    const string col = colour_to_str(channel_to_colour(MSGCH_PROMPT));
    if (outside_items)
    {
        const formatted_string outside = formatted_string::parse_string(make_stringf(
            "<%s>I'll put %s outside for you.</%s>\n",
            col.c_str(),
            bought_indices.size() == 1             ? "it" :
      (int) bought_indices.size() == outside_items ? "them"
                                                   : "some of them",
            col.c_str()));
        set_more(outside + formatted_string::parse_string(top_line + m));
    }
    else
        set_more(formatted_string::parse_string(top_line + m));

    // set_more(formatted_string::parse_string(top_line
    //     + make_stringf(


    //     "%s exit  "
    //     "%s      %s %s\n"
    //     "[<w>/</w>] sort (%s)%s  %s  %s put item on shopping list",
    //     menu_keyhelp_cmd(CMD_MENU_EXIT).c_str(),
    //     !can_purchase ?              " " " "  "  " "       "  "          " :
    //     menu_action == ACT_EXECUTE ? "[<w>!</w>] <w>buy</w>|examine items" :
    //                                  "[<w>!</w>] buy|<w>examine</w> items",
    //     hyphenated_hotkey_letters(item_count(), 'a').c_str(),
    //     menu_action == ACT_EXECUTE ? "mark item for purchase" : "examine item",
    //     shopping_order_names[order],
    //     // strwidth("default")
    //     string(7 - strwidth(shopping_order_names[order]), ' ').c_str(),
    //     action_desc.c_str(),
    //     hyphenated_hotkey_letters(item_count(), 'A').c_str())));
}

void ShopMenu::purchase_selected()
{
    no_excursions sanity;

    bool buying_from_list = false;
    vector<MenuEntry*> selected = selected_entries();
    int cost = selected_cost();
    if (selected.empty())
    {
        ASSERT(cost == 0);
        buying_from_list = true;
        for (auto item : items)
        {
            auto e = dynamic_cast<ShopEntry*>(item);
            if (shopping_list.is_on_list(*e->item, &pos))
            {
                selected.push_back(item);
                cost += e->cost;
            }
        }
    }
    if (selected.empty())
        return;
    const string col = colour_to_str(channel_to_colour(MSGCH_PROMPT));
    update_help();
    const formatted_string old_more = more;
    const bool too_expensive = (cost > you.gold);
    if (too_expensive)
    {
        if (!have_voucher() || cost - max_cost() > you.gold)
        {
            more = formatted_string::parse_string(make_stringf(
                    "<%s>You don't have enough money.</%s>\n",
                    col.c_str(),
                    col.c_str()));
            more += old_more;
            update_more();
            return;
        }
    }
    more = formatted_string::parse_string(make_stringf(
               "<%s>Purchase items%s for %d gold? %s (%s/N)</%s>\n",
               col.c_str(),
               buying_from_list ? " in shopping list" : "",
               cost,
               too_expensive ? "This will use your shop voucher." : "",
               Options.easy_confirm == easy_confirm_type::none ? "Y" : "y",
               col.c_str()));
    more += old_more;
    update_more();
    if (!yesno(nullptr, true, 'n', false, false, true))
    {
        more = old_more;
        update_more();
        return;
    }
    sort(begin(selected), end(selected),
         [](MenuEntry* a, MenuEntry* b)
         {
             return a->data > b->data;
         });
    bought_indices = {};
    outside_items = 0;

    // Store last_pickup in case we need to restore it.
    // Then clear it to fill with items purchased.
    map<int,int> tmp_l_p = you.last_pickup;
    you.last_pickup.clear();

    bool use_voucher = false;
    int voucher_value = too_expensive ? max_cost() : 0;

    // Will iterate backwards through the shop (because of the earlier sort).
    // This means we can erase() from shop.stock (since it only invalidates
    // pointers to later elements), but nothing else.
    for (auto entry : selected)
    {
        const int i = static_cast<item_def*>(entry->data) - shop.stock.data();
        item_def& item(shop.stock[i]);
        const int price = dynamic_cast<ShopEntry*>(entry)->cost;
        // Can happen if the price changes due to id status
        if (price > you.gold && price != voucher_value)
            continue;

        use_voucher = price == voucher_value;
        if (use_voucher)
            voucher_value = 0;

        const int quant = item.quantity;

        if (!_purchase(shop, pos, i, price, use_voucher))
        {
            // The purchased item didn't fit into your
            // knapsack.
            outside_items += quant;
        }

        bought_indices.push_back(i);
        bought_something = true;
    }

    if (you.last_pickup.empty())
        you.last_pickup = tmp_l_p;

    // Since the old ShopEntrys may now point to past the end of shop.stock (or
    // just the wrong place in general) nuke the whole thing and start over.
    clear();
    init_entries();
    resort();

    update_help();
    update_menu(true);
}

// Doesn't handle redrawing itself.
void ShopMenu::resort()
{
    switch (order)
    {
    case ORDER_TYPE:
    {
        const bool id = shoptype_identifies_stock(shop.type);
        // Using a map to sort reduces the number of item->name() calls.
        multimap<const string, MenuEntry *> list;
        for (const auto entry : items)
        {
            const auto item = dynamic_cast<ShopEntry*>(entry)->item;
            if (is_artefact(*item) && item->is_identified())
                list.insert({item->name(DESC_QUALNAME, false, id), entry});
            else
            {
                list.insert({item->name(DESC_DBNAME, false, id) + " "
                     + item->name(DESC_PLAIN, false, id), entry});
            }
        }
        items.clear();
        for (auto &entry : list)
            items.push_back(entry.second);
        break;
    }
    case ORDER_PRICE:
        sort(begin(items), end(items),
             [](MenuEntry* a, MenuEntry* b)
             {
                 return dynamic_cast<ShopEntry*>(a)->cost
                        < dynamic_cast<ShopEntry*>(b)->cost;
             });
        break;
    case ORDER_ALPHABETICAL:
        sort(begin(items), end(items),
             [this](MenuEntry* a, MenuEntry* b) -> bool
             {
                 const bool id = shoptype_identifies_stock(shop.type);
                 return dynamic_cast<ShopEntry*>(a)->item->name(DESC_PLAIN, false, id)
                        < dynamic_cast<ShopEntry*>(b)->item->name(DESC_PLAIN, false, id);
             });
        break;
    case NUM_ORDERS:
        die("invalid ordering");
    }
    for (size_t i = 0; i < items.size(); ++i)
        items[i]->hotkeys[0] = index_to_letter(i);
}

bool ShopMenu::examine_index(int i)
{
    ASSERT(i < static_cast<int>(items.size()));
    // A hack to make the description more useful.
    // The default copy constructor is non-const for item_def,
    // so we need this violation of const hygiene to tweak the flags
    // to make the description more useful. The flags are copied by
    // value by the default copy constructor so this is safe.
    item_def& item(*const_cast<item_def*>(dynamic_cast<ShopEntry*>(
        items[i])->item));
    if (shoptype_identifies_stock(shop.type))
    {
        item.flags |= (ISFLAG_IDENTIFIED | ISFLAG_NOTED_ID
                       | ISFLAG_NOTED_GET);
    }
    describe_item_popup(item);
    return true;
}

bool ShopMenu::cycle_mode(bool)
{
    if (can_purchase)
    {
        if (menu_action == ACT_EXECUTE)
            menu_action = ACT_EXAMINE;
        else
            menu_action = ACT_EXECUTE;
        update_help();
        update_more();
        return true;
    }
    return false;
}

void ShopMenu::select_item_index(int idx, int qty)
{
    if (!can_purchase)
        return; // do nothing
    InvMenu::select_item_index(idx, qty);
}

bool ShopMenu::process_command(command_type cmd)
{
    switch (cmd)
    {
    case CMD_MENU_ACCEPT_SELECTION:
        if (can_purchase)
            purchase_selected();
        return true;
    default:
        break;
    }
    return InvMenu::process_command(cmd);
}

bool ShopMenu::skip_process_command(int keyin)
{
    // Bypass InvMenu::skip_process_command, which disables ! and ?
    return Menu::skip_process_command(keyin);
}

bool ShopMenu::process_key(int keyin)
{
    switch (keyin)
    {
    case '$':
    {
        // XX maybe add highlighted item if no selection?
        const vector<MenuEntry*> selected = selected_entries();
        if (!selected.empty())
        {
            // Move selected to shopping list.
            for (auto entry : selected)
            {
                const item_def& item = *dynamic_cast<ShopEntry*>(entry)->item;
                auto cost = dynamic_cast<ShopEntry*>(entry)->cost;
                entry->selected_qty = 0;
                if (!shopping_list.is_on_list(item, &pos))
                    shopping_list.add_thing(item, cost, &pos);
            }
        }
        else if (can_purchase)
        {
            // Move shoplist to selection.
            for (auto entry : items)
                if (shopping_list.is_on_list(*dynamic_cast<ShopEntry*>(entry)->item, &pos))
                    entry->select(-2);
        }
        // Move shoplist to selection.
        update_help();
        update_menu(true);
        return true;
    }
    case '/':
        ++order;
        resort();
        update_help();
        update_menu(true);
        return true;
    default:
        break;
    }

    if (keyin - 'A' >= 0 && keyin - 'A' < (int)items.size())
    {
        const auto index = letter_to_index(keyin) % 26;
        auto entry = dynamic_cast<ShopEntry*>(items[index]);
        entry->selected_qty = 0;
        const item_def& item(*entry->item);
        if (shopping_list.is_on_list(item, &pos))
            shopping_list.del_thing(item, &pos);
        else
            shopping_list.add_thing(item, entry->cost, &pos);
        update_help();
        update_menu(true);
        return true;
    }

    auto old_selected = selected_entries();
    bool ret = InvMenu::process_key(keyin);
    if (old_selected != selected_entries())
    {
        // Update the footer to display the new $$$ info.
        update_help();
        update_menu(true);
        // Next time, dismiss any message about leaving items outside.
        outside_items = 0;
    }
    return ret;
}

void shop()
{
    if (!shop_at(you.pos()))
    {
        mprf(MSGCH_ERROR, "Help! Non-existent shop.");
        return;
    }

    shop_struct& shop = *shop_at(you.pos());
    const string shopname = shop_name(shop);

    // Quick out, if no inventory
    if (shop.stock.empty())
    {
        mprf("%s appears to be closed.", shopname.c_str());
        destroy_shop_at(you.pos());
        return;
    }

    bool culled = false;
    for (const auto& item : shop.stock)
    {
        const int cost = item_price(item, shop);
        culled |= shopping_list.cull_identical_items(item, cost);
    }
    if (culled)
        more(); // make sure all messages appear before menu

    ShopMenu menu(shop, level_pos::current(), true);
    menu.show();

    StashTrack.get_shop(shop.pos) = ShopInfo(shop);
    bool any_on_list = any_of(begin(shop.stock), end(shop.stock),
                              [](const item_def& item)
                              {
                                  return shopping_list.is_on_list(item);
                              });

    // If the shop is now empty, erase it from the overview.
    if (shop.stock.empty())
        destroy_shop_at(you.pos());
    // finally it is safe to catch up on any off-level id stuff that is needed
    shopping_list.do_excursion_work();
    redraw_screen();
    update_screen();
    if (menu.bought_something)
        mprf("Thank you for shopping at %s!", shopname.c_str());
    if (any_on_list)
        mpr("You can access your shopping list by pressing '$'.");
}

void shop(shop_struct& shop, const level_pos& pos)
{
    ASSERT(shop.pos == pos.pos);
    ShopMenu(shop, pos, false).show();
}

void destroy_shop_at(coord_def p)
{
    if (shop_at(p))
    {
        env.shop.erase(p);
        env.grid(p) = DNGN_ABANDONED_SHOP;
        unnotice_feature(level_pos(level_id::current(), p));
    }
}

shop_struct *shop_at(const coord_def& where)
{
    if (env.grid(where) != DNGN_ENTER_SHOP)
        return nullptr;

    auto it = env.shop.find(where);
    ASSERT(it != env.shop.end());
    ASSERT(it->second.pos == where);
    ASSERT(it->second.type != SHOP_UNASSIGNED);

    return &it->second;
}

string shop_type_name(shop_type type)
{
    switch (type)
    {
        case SHOP_WEAPON_ANTIQUE:
            return "Antique Weapon";
        case SHOP_ARMOUR_ANTIQUE:
            return "Antique Armour";
        case SHOP_WEAPON:
            return "Weapon";
        case SHOP_ARMOUR:
            return "Armour";
        case SHOP_JEWELLERY:
            return "Jewellery";
        case SHOP_BOOK:
            return "Book";
#if TAG_MAJOR_VERSION == 34
        case SHOP_EVOKABLES:
            return "Gadget";
        case SHOP_FOOD:
            return "Removed Food";
#endif
        case SHOP_SCROLL:
            return "Magic Scroll";
        case SHOP_GENERAL_ANTIQUE:
            return "Assorted Antiques";
        case SHOP_DISTILLERY:
            return "Distillery";
        case SHOP_GENERAL:
            return "General Store";
        default:
            return "Bug";
    }
}

static const char *_shop_type_suffix(shop_type type, const coord_def &where)
{
    if (type == SHOP_GENERAL
        || type == SHOP_GENERAL_ANTIQUE
        || type == SHOP_DISTILLERY)
    {
        return "";
    }

    static const char * const suffixnames[] =
    {
        "Shoppe", "Boutique", "Emporium", "Shop"
    };
    return suffixnames[(where.x + where.y) % ARRAYSZ(suffixnames)];
}

string shop_name(const shop_struct& shop)
{
    const shop_type type = shop.type;

    string sh_name = "";

#if TAG_MAJOR_VERSION == 34
    // xref ShopInfo::load
    if (shop.shop_name == " ")
        return shop.shop_type_name;
#endif
    if (!shop.shop_name.empty())
        sh_name += apostrophise(shop.shop_name) + " ";
    else
    {
        uint32_t seed = static_cast<uint32_t>(shop.keeper_name[0])
            | (static_cast<uint32_t>(shop.keeper_name[1]) << 8)
            | (static_cast<uint32_t>(shop.keeper_name[1]) << 16);

        sh_name += apostrophise(make_name(seed)) + " ";
    }

    if (!shop.shop_type_name.empty())
        sh_name += shop.shop_type_name;
    else
        sh_name += shop_type_name(type);

    if (!shop.shop_suffix_name.empty())
        sh_name += " " + shop.shop_suffix_name;
    else
    {
        string sh_suffix = _shop_type_suffix(type, shop.pos);
        if (!sh_suffix.empty())
            sh_name += " " + sh_suffix;
    }

    return sh_name;
}

bool is_shop_item(const item_def &item)
{
    return item.link == ITEM_IN_SHOP;
}

bool shoptype_identifies_stock(shop_type type)
{
    return type != SHOP_WEAPON_ANTIQUE
           && type != SHOP_ARMOUR_ANTIQUE
           && type != SHOP_GENERAL_ANTIQUE;
}

// Returns whether the type of an identified item in a shop is otherwise
// unknown to the player (and it should be marked "(unknown)").
bool shop_item_unknown(const item_def &item)
{
    return item_type_has_ids(item.base_type)
           && !item_type_known(item)
           && item.is_identified()
           && !is_artefact(item);
}

static const char *shop_types[] =
{
    "weapon",
    "armour",
    "antique weapon",
    "antique armour",
    "antiques",
    "jewellery",
#if TAG_MAJOR_VERSION == 34
    "removed gadget",
#endif
    "book",
#if TAG_MAJOR_VERSION == 34
    "removed food",
#endif
    "distillery",
    "scroll",
    "general",
};

/** What shop type is this?
 *
 *  @param s the shop type, in a string.
 *  @returns the corresponding enum, or SHOP_UNASSIGNED if none.
 */
shop_type str_to_shoptype(const string &s)
{
#if TAG_MAJOR_VERSION == 34
    if (s == "removed gadget" || s == "removed food")
        return SHOP_UNASSIGNED;
#endif
    if (s == "random" || s == "any")
        return SHOP_RANDOM;

    for (size_t i = 0; i < ARRAYSZ(shop_types); ++i)
        if (s == shop_types[i])
            return static_cast<shop_type>(i);

    return SHOP_UNASSIGNED;
}

const char *shoptype_to_str(shop_type type)
{
    return shop_types[type];
}

////////////////////////////////////////////////////////////////////////

// TODO:
//   * Warn if buying something not on the shopping list would put
//     something on shopping list out of your reach.

#define SHOPPING_LIST_KEY       "shopping_list_key"
#define SHOPPING_LIST_COST_KEY  "shopping_list_cost_key"
#define SHOPPING_THING_COST_KEY "cost_key"
#define SHOPPING_THING_ITEM_KEY "item_key"
#define SHOPPING_THING_DESC_KEY "desc_key"
#define SHOPPING_THING_VERB_KEY "verb_key"
#define SHOPPING_THING_POS_KEY  "pos_key"

ShoppingList::ShoppingList()
    : list(nullptr), min_unbuyable_cost(0), min_unbuyable_idx(0),
      max_buyable_cost(0), max_buyable_idx(0)
{
}

#define SETUP_POS()                 \
    ASSERT(list); \
    level_pos pos;                  \
    if (_pos != nullptr)            \
        pos = *_pos;                \
    else                            \
        pos = level_pos::current(); \
    ASSERT(pos.is_valid());

bool ShoppingList::add_thing(const item_def &item, int cost,
                             const level_pos* _pos)
{
    ASSERT(item.defined());
    ASSERT(cost > 0);

    SETUP_POS();

    if (!find_thing(item, pos).empty()) // TODO: this check isn't working?
    {
        ui::error(make_stringf("%s is already on the shopping list.",
             item.name(DESC_THE).c_str()));
        return false;
    }

    CrawlHashTable *thing = new CrawlHashTable();
    (*thing)[SHOPPING_THING_COST_KEY] = cost;
    (*thing)[SHOPPING_THING_POS_KEY]  = pos;
    (*thing)[SHOPPING_THING_ITEM_KEY] = item;
    list->push_back(*thing);
    refresh();

    return true;
}

bool ShoppingList::is_on_list(const item_def &item, const level_pos* _pos) const
{
    SETUP_POS();

    return !find_thing(item, pos).empty();
}

bool ShoppingList::is_on_list(string desc, const level_pos* _pos) const
{
    SETUP_POS();

    return !find_thing(desc, pos).empty();
}

void ShoppingList::del_thing_at_index(int idx)
{
    ASSERT_RANGE(idx, 0, list->size());
    list->erase(idx);
    refresh();
}

template <typename C>
void ShoppingList::del_thing_at_indices(C const &idxs)
{
    set<int,greater<int>> indices(idxs.begin(), idxs.end());

    for (auto idx : indices)
    {
        ASSERT_RANGE(idx, 0, list->size());
        list->erase(idx);
    }
    refresh();
}

void ShoppingList::del_things_from(const level_id &lid)
{
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        if (thing_pos(thing).is_on(lid))
            list->erase(i--);
    }
    refresh();
}

bool ShoppingList::del_thing(const item_def &item,
                             const level_pos* _pos)
{
    SETUP_POS();

    auto indices = find_thing(item, pos);

    if (indices.empty())
    {
        ui::error(make_stringf("%s isn't on shopping list, can't delete it.",
             item.name(DESC_THE).c_str()));
        return false;
    }

    del_thing_at_indices(indices);
    return true;
}

bool ShoppingList::del_thing(string desc, const level_pos* _pos)
{
    SETUP_POS();

    auto indices = find_thing(desc, pos);

    if (indices.empty())
    {
        ui::error(make_stringf("%s isn't on shopping list, can't delete it.",
             desc.c_str()));
        return false;
    }

    del_thing_at_indices(indices);
    return true;
}

#undef SETUP_POS

#define REMOVE_PROMPTED_KEY  "remove_prompted_key"
#define REPLACE_PROMPTED_KEY "replace_prompted_key"

// TODO:
//
// * If you get a randart which lets you turn invisible, then remove
//   any ordinary rings of invisibility from the shopping list.
//
// * If you collected enough spellbooks that all the spells in a
//   shopping list book are covered, then auto-remove it.
bool ShoppingList::cull_identical_items(const item_def& item, int cost)
{
    // Dead men can't update their shopping lists.
    if (!crawl_state.need_save)
        return 0;

    // Can't put items in Bazaar shops in the shopping list, so
    // don't bother transferring shopping list items to Bazaar shops.
    // TODO: temporary shoplists
    if (cost != -1 && !is_connected_branch(you.where_are_you))
        return 0;

    switch (item.base_type)
    {
    case OBJ_JEWELLERY:
    case OBJ_BOOKS:
    case OBJ_STAVES:
    case OBJ_TALISMANS:
        // Only these are really interchangeable.
        break;
    case OBJ_MISCELLANY:
        // Evokers are useless to purchase at max charge, but useful otherwise.
        if (!is_xp_evoker(item) || evoker_plus(item.sub_type) != MAX_EVOKER_ENCHANT)
            return 0;
        break;
    default:
        return 0;
    }

    if (!item.is_identified() || is_artefact(item))
        return 0;

    // Ignore stat-modification rings which reduce a stat, since they're
    // worthless.
    if (item.base_type == OBJ_JEWELLERY && item.plus < 0)
        return 0;

    // Manuals are consumable, and interesting enough to keep on list.
    if (item.is_type(OBJ_BOOKS, BOOK_MANUAL))
        return 0;

    // Item is already on shopping-list.
    const bool on_list = !find_thing(item, level_pos::current()).empty();

    const bool do_prompt = item.base_type == OBJ_JEWELLERY
                           && !jewellery_is_amulet(item)
                           && ring_has_stackable_effect(item);

    bool add_item = false;

    typedef pair<item_def, level_pos> list_pair;
    vector<list_pair> to_del;

    // NOTE: Don't modify the shopping list while iterating over it.
    for (CrawlHashTable &thing : *list)
    {
        if (!thing_is_item(thing))
            continue;

        const item_def& list_item = get_thing_item(thing);

        if (list_item.base_type != item.base_type
            || list_item.sub_type != item.sub_type)
        {
            continue;
        }

        if (!item.is_identified() || is_artefact(list_item))
            continue;

        // Don't prompt to remove rings with strictly better pluses
        // than the new one. Also, don't prompt to remove rings with
        // known pluses when the new ring's pluses are unknown.
        if (item.base_type == OBJ_JEWELLERY)
        {
            const bool has_plus = jewellery_has_pluses(item);
            const int delta_p = item.plus - list_item.plus;
            if (has_plus
                && list_item.is_identified()
                && (!item.is_identified() || delta_p < 0))
            {
                continue;
            }
        }

        // Don't prompt to remove known manuals when the new one is for a
        // different skill.
        if (item.is_type(OBJ_BOOKS, BOOK_MANUAL) && item.plus != list_item.plus)
            continue;

        list_pair listed(list_item, thing_pos(thing));

        // cost = -1, we just found a shop item which is cheaper than
        // one on the shopping list.
        if (cost != -1)
        {
            int list_cost = thing_cost(thing);

            if (cost >= list_cost)
                continue;

            // Only prompt once.
            if (thing.exists(REPLACE_PROMPTED_KEY))
                continue;
            thing[REPLACE_PROMPTED_KEY] = (bool) true;

            string prompt =
                make_stringf("Shopping list: replace %dgp %s with cheaper "
                             "one? (Y/n)", list_cost,
                             describe_thing(thing).c_str());

            if (yesno(prompt.c_str(), true, 'y', false))
            {
                add_item = true;
                to_del.push_back(listed);
            }
            continue;
        }

        // cost == -1, we just got an item which is on the shopping list.
        if (do_prompt)
        {
            // Only prompt once.
            if (thing.exists(REMOVE_PROMPTED_KEY))
                continue;
            thing[REMOVE_PROMPTED_KEY] = (bool) true;

            string prompt = make_stringf("Shopping list: remove %s? (Y/n)",
                                         describe_thing(thing, DESC_A).c_str());

            if (yesno(prompt.c_str(), true, 'y', false))
            {
                to_del.push_back(listed);
                mprf("Shopping list: removing %s",
                     describe_thing(thing, DESC_A).c_str());
            }
            else
                canned_msg(MSG_OK);
        }
        else
        {
            mprf("Shopping list: removing %s",
                 describe_thing(thing, DESC_A).c_str());
            to_del.push_back(listed);
        }
    }

    for (list_pair &entry : to_del)
        del_thing(entry.first, &entry.second);

    if (add_item && !on_list)
        add_thing(item, cost);

    return to_del.size();
}

void ShoppingList::do_excursion_work()
{
    ASSERT(level_excursions_allowed());
    // this is not (currently) automatically called, so be sure to call it
    // manually if you trigger item_type_identified at a time where
    // excursions are disabled. (XX autocall every once in a while?)
    for (auto &p : need_excursions)
        item_type_identified(p.first, p.second);
    need_excursions.clear();
}

void ShoppingList::item_type_identified(object_class_type base_type,
                                        int sub_type)
{
    // Dead men can't update their shopping lists.
    if (!crawl_state.need_save)
        return;

    if (!level_excursions_allowed())
    {
        need_excursions.emplace_back(base_type, sub_type);
        return;
    }

    // Only restore the excursion at the very end.
    level_excursion le;

#if TAG_MAJOR_VERSION == 34
    // Handle removed Gozag shops from old saves. Only do this once:
    // future Gozag abandonment will call remove_dead_shops itself.
    if (!you.props.exists(REMOVED_DEAD_SHOPS_KEY))
    {
        remove_dead_shops();
        you.props[REMOVED_DEAD_SHOPS_KEY] = true;
    }
#endif

    for (CrawlHashTable &thing : *list)
    {
        if (!thing_is_item(thing))
            continue;

        const item_def& item = get_thing_item(thing);

        if (item.base_type != base_type || item.sub_type != sub_type)
            continue;

        const level_pos place = thing_pos(thing);

        le.go_to(place.id);
        const shop_struct *shop = shop_at(place.pos);
        ASSERT(shop);
        if (shoptype_identifies_stock(shop->type))
            continue;

        thing[SHOPPING_THING_COST_KEY] =
            _shop_get_item_value(item, shop->greed, false);
    }

    // Prices could have changed.
    refresh();
}

void ShoppingList::spells_added_to_library(const vector<spell_type>& spells, bool quiet)
{
    if (!list) /* let's not make book starts crash instantly... */
        return;

    unordered_set<int> indices_to_del;
    for (CrawlHashTable &thing : *list)
    {
        if (!thing_is_item(thing)) // ???
            continue;
        const item_def& book = get_thing_item(thing);
        if (book.base_type != OBJ_BOOKS || book.sub_type == BOOK_MANUAL)
            continue;

        const auto item_spells = spells_in_book(book);
        if (any_of(item_spells.begin(), item_spells.end(), [&spells](const spell_type st) {
                    return find(spells.begin(), spells.end(), st) != spells.end();
                }) && is_useless_item(book, false))
        {
            level_pos pos = thing_pos(thing); // TODO: unreliable?
            auto thing_indices = find_thing(get_thing_item(thing), pos);
            indices_to_del.insert(thing_indices.begin(), thing_indices.end());
        }
    }
    if (!quiet)
    {
        for (auto idx : indices_to_del)
        {
            ASSERT_RANGE(idx, 0, list->size());
            mprf("Shopping list: removing %s",
                describe_thing((*list)[idx], DESC_A).c_str());
        }
    }
    del_thing_at_indices(indices_to_del);
}

void ShoppingList::remove_dead_shops()
{
    // Only restore the excursion at the very end.
    level_excursion le;

    // This is potentially a lot of excursions, it might be cleaner to do this
    // by annotating the shopping list directly
    set<level_pos> shops_to_remove;
    set<level_id> levels_seen;

    for (CrawlHashTable &thing : *list)
    {
        const level_pos place = thing_pos(thing);
        le.go_to(place.id);
        if (!levels_seen.count(place.id))
        {
            // Alternatively, this could call catchup_dactions. But that might
            // have other side effects.
            gozag_abandon_shops_on_level();
            levels_seen.insert(place.id);
        }
        const shop_struct *shop = shop_at(place.pos);

        if (!shop)
            shops_to_remove.insert(place);
    }

    for (auto pos : shops_to_remove)
        forget_pos(pos);

    // Prices could have changed.
    refresh();
}

vector<shoplist_entry> ShoppingList::entries()
{
    ASSERT(list);

    vector<shoplist_entry> list_entries;
    for (const CrawlHashTable &entry : *list)
    {
        list_entries.push_back(
            make_pair(name_thing(entry), thing_cost(entry))
        );
    }

    return list_entries;
}

int ShoppingList::size() const
{
    ASSERT(list);

    return list->size();
}

bool ShoppingList::items_are_same(const item_def& item_a,
                                  const item_def& item_b)
{
    return item_name_simple(item_a) == item_name_simple(item_b);
}

void ShoppingList::move_things(const coord_def &_src, const coord_def &_dst)
{
    if (crawl_state.map_stat_gen
        || crawl_state.obj_stat_gen
        || crawl_state.test)
    {
        return; // Shopping list is initialized and unneeded.
    }

    const level_pos src(level_id::current(), _src);
    const level_pos dst(level_id::current(), _dst);

    for (CrawlHashTable &thing : *list)
        if (thing_pos(thing) == src)
            thing[SHOPPING_THING_POS_KEY] = dst;
}

void ShoppingList::forget_pos(const level_pos &pos)
{
    if (!crawl_state.need_save)
        return; // Shopping list is uninitialized and unneeded.

    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        if (thing_pos(thing) == pos)
        {
            list->erase(i);
            i--;
        }
    }

    // Maybe we just forgot about a shop.
    refresh();
}

void ShoppingList::gold_changed(int old_amount, int new_amount)
{
    ASSERT(list);

    if (new_amount > old_amount && new_amount >= min_unbuyable_cost)
    {
        ASSERT(min_unbuyable_idx < list->size());

        vector<string> descs;
        for (unsigned int i = min_unbuyable_idx; i < list->size(); i++)
        {
            const CrawlHashTable &thing = (*list)[i];
            const int            cost   = thing_cost(thing);

            if (cost > new_amount)
            {
                ASSERT(i > (unsigned int) min_unbuyable_idx);
                break;
            }

            string desc;

            if (thing.exists(SHOPPING_THING_VERB_KEY))
                desc += thing[SHOPPING_THING_VERB_KEY].get_string();
            else
                desc = "buy";
            desc += " ";

            desc += describe_thing(thing, DESC_A);

            descs.push_back(desc);
        }
        ASSERT(!descs.empty());

        mpr_comma_separated_list("You now have enough gold to ", descs,
                                 ", or ");
        mpr("You can access your shopping list by pressing '$'.");

        // Our gold has changed, maybe we can buy different things now.
        refresh();
    }
    else if (new_amount < old_amount && new_amount < max_buyable_cost)
    {
        // As above.
        refresh();
    }
}

class ShoppingListMenu : public Menu
{
public:
    ShoppingListMenu(ShoppingList &_list)
        : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING | MF_ARROWS_SELECT
            | MF_INIT_HOVER),
        view_only(false), list(_list)
    {}

    bool view_only;
    ShoppingList &list;

    string get_keyhelp(bool) const override
    {
        string s = make_stringf("<yellow>You have %d gold pieces</yellow>\n"
                                "<lightgrey>", you.gold);

        if (view_only)
            s += "Choose to examine item  ";
        else
        {
            s += menu_keyhelp_cmd(CMD_MENU_CYCLE_MODE);
            switch (menu_action)
            {
            case ACT_EXECUTE:
                s += " <w>travel</w>|examine|delete";
                break;
            case ACT_EXAMINE:
                s += " travel|<w>examine</w>|delete";
                break;
            default:
                s += " travel|examine|<w>delete</w>";
                break;
            }

            s += " items    ";
        }
        if (items.size() > 0)
        {
            // could be dropped if there's ever more interesting hotkeys
            s += hyphenated_hotkey_letters(items.size(), 'a')
                    + " choose item</lightgray>";
        }
        return pad_more_with_esc(s);
    }

    friend class ShoppingList;

protected:
    virtual formatted_string calc_title() override;
    bool examine_index(int i) override;
};

formatted_string ShoppingListMenu::calc_title()
{
    formatted_string fs;
    const int total_cost = you.props[SHOPPING_LIST_COST_KEY];

    fs.textcolour(title->colour);
    fs.cprintf("Shopping list: %d %s%s, total cost %d gold pieces",
                title->quantity, title->text.c_str(),
                title->quantity > 1 ? "s" : "",
                total_cost);

    return fs;
}

/**
 * Describe the location of a given shopping list entry.
 *
 * @param thing     A shopping list entry.
 * @return          Something like [Orc:4], probably.
 */
string ShoppingList::describe_thing_pos(const CrawlHashTable &thing)
{
    return make_stringf("[%s]", thing_pos(thing).id.describe().c_str());
}

void ShoppingList::fill_out_menu(Menu& shopmenu)
{
    menu_letter hotkey;
    int longest = 0;
    // How much space does the longest entry need for proper alignment?
    for (const CrawlHashTable &thing : *list)
        longest = max(longest, strwidth(describe_thing_pos(thing)));

    for (CrawlHashTable &thing : *list)
    {
        const int cost = thing_cost(thing);
        const bool unknown = thing_is_item(thing)
                             && shop_item_unknown(get_thing_item(thing));

        const string etitle =
            make_stringf(
                "%*s%5d gold  %s%s",
                longest,
                describe_thing_pos(thing).c_str(),
                cost,
                name_thing(thing, DESC_A).c_str(),
                unknown ? " (unknown)" : "");

        MenuEntry *me = new MenuEntry(etitle, MEL_ITEM, 1, hotkey);
        me->data = &thing;

        if (thing_is_item(thing))
        {
            // Colour shopping list item according to menu colours.
            const item_def &item = get_thing_item(thing);

            const string colprf = item_prefix(item, false);
            const int col = menu_colour(item.name(DESC_A),
                                        colprf, "shop", false);

            vector<tile_def> item_tiles;
            get_tiles_for_item(item, item_tiles, true);
            for (const auto &tile : item_tiles)
                me->add_tile(tile);

            if (col != -1)
                me->colour = col;
        }
        if (cost > you.gold)
            me->colour = DARKGREY;

        shopmenu.add_entry(me);
        ++hotkey;
    }
}

bool ShoppingListMenu::examine_index(int i)
{
    ASSERT(i >= 0 && i < static_cast<int>(items.size()));
    const CrawlHashTable* thing =
                        static_cast<const CrawlHashTable *>(items[i]->data);
    const bool is_item = list.thing_is_item(*thing);

    if (is_item)
    {
        const item_def &item = list.get_thing_item(*thing);
        describe_item_popup(item);
    }
    else // not an item, so we only stored a description.
    {
        // HACK: Assume it's some kind of portal vault.
        const string info = make_stringf(
                     "%s with an entry fee of %d gold pieces.",
                     list.describe_thing(*thing, DESC_A).c_str(),
                     (int) list.thing_cost(*thing));
        show_description(info.c_str());
    }
    return true;
}

void ShoppingList::display(bool view_only)
{
    if (list->empty())
        return;

    ShoppingListMenu shopmenu(*this);
    shopmenu.view_only = view_only;
    shopmenu.set_tag("shop");
    shopmenu.menu_action  = view_only ? Menu::ACT_EXAMINE : Menu::ACT_EXECUTE;
    shopmenu.action_cycle = view_only ? Menu::CYCLE_NONE : Menu::CYCLE_CYCLE;
    string title          = "item";

    MenuEntry *mtitle = new MenuEntry(title, MEL_TITLE);
    mtitle->quantity = list->size();
    shopmenu.set_title(mtitle);

    fill_out_menu(shopmenu);

    shopmenu.on_single_selection =
        [this, &shopmenu, &mtitle](const MenuEntry& sel)
    {
        const CrawlHashTable* thing =
            static_cast<const CrawlHashTable *>(sel.data);

        if (shopmenu.menu_action == Menu::ACT_EXECUTE)
        {
            int cost = thing_cost(*thing);

            if (cost > you.gold)
            {
                string prompt =
                   make_stringf("Travel to: %s\nYou cannot afford this item; travel there "
                                "anyway? (y/N)",
                                describe_thing(*thing, DESC_A).c_str());
                clrscr();
                if (!yesno(prompt.c_str(), true, 'n'))
                    return true;
            }

            const level_pos lp(thing_pos(*thing));
            start_translevel_travel(lp);
            return false;
        }
        else if (shopmenu.menu_action == Menu::ACT_MISC)
        {
            const int index = shopmenu.get_entry_index(&sel);
            if (index == -1)
            {
                ui::error("ERROR: Unable to delete thing from shopping list!");
                return true;
            }

            del_thing_at_index(index);
            mtitle->quantity = this->list->size();
            shopmenu.set_title(mtitle);

            if (this->list->empty())
            {
                mpr("Your shopping list is now empty.");
                return false;
            }

            shopmenu.clear();
            fill_out_menu(shopmenu);
            shopmenu.update_more();
            shopmenu.update_menu(true);
        }
        else
            die("Invalid menu action type");
        return true;
    };

    shopmenu.show();
    redraw_screen();
    update_screen();
}

static bool _compare_shopping_things(const CrawlStoreValue& a,
                                     const CrawlStoreValue& b)
{
    const CrawlHashTable& hash_a = a.get_table();
    const CrawlHashTable& hash_b = b.get_table();

    const int a_cost = hash_a[SHOPPING_THING_COST_KEY];
    const int b_cost = hash_b[SHOPPING_THING_COST_KEY];

    const level_id id_a = hash_a[SHOPPING_THING_POS_KEY].get_level_pos().id;
    const level_id id_b = hash_b[SHOPPING_THING_POS_KEY].get_level_pos().id;

    // Put Bazaar items at the top of the shopping list.
    if (!player_in_branch(BRANCH_BAZAAR) || id_a.branch == id_b.branch)
        return a_cost < b_cost;
    else
        return id_a.branch == BRANCH_BAZAAR;
}

// Reset max_buyable and min_unbuyable info. Call this anytime any of the
// player's gold, the shopping list, and the prices of the items on it
// change.
void ShoppingList::refresh()
{
    if (!you.props.exists(SHOPPING_LIST_KEY))
        you.props[SHOPPING_LIST_KEY].new_vector(SV_HASH, SFLAG_CONST_TYPE);
    list = &you.props[SHOPPING_LIST_KEY].get_vector();

    stable_sort(list->begin(), list->end(), _compare_shopping_things);

    min_unbuyable_cost = INT_MAX;
    min_unbuyable_idx  = -1;
    max_buyable_cost   = -1;
    max_buyable_idx    = -1;

    int total_cost = 0;

    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        const int cost = thing_cost(thing);

        if (cost <= you.gold)
        {
            max_buyable_cost = cost;
            max_buyable_idx  = i;
        }
        else if (min_unbuyable_idx == -1)
        {
            min_unbuyable_cost = cost;
            min_unbuyable_idx  = i;
        }
        total_cost += cost;
    }
    you.props[SHOPPING_LIST_COST_KEY].get_int() = total_cost;
}

unordered_set<int> ShoppingList::find_thing(const item_def &item,
                             const level_pos &pos) const
{
    unordered_set<int> result;
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];
        const level_pos       _pos  = thing_pos(thing);

        if (pos != _pos) // TODO: using thing_pos above seems to make this unreliable?
            continue;

        if (!thing_is_item(thing))
            continue;

        const item_def &_item = get_thing_item(thing);

        if (item_name_simple(item) == item_name_simple(_item))
            result.insert(i);
    }

    return result;
}

unordered_set<int> ShoppingList::find_thing(const string &desc, const level_pos &pos) const
{
    unordered_set<int> result;
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];
        const level_pos       _pos  = thing_pos(thing);

        if (pos != _pos)
            continue;

        if (thing_is_item(thing))
            continue;

        if (desc == name_thing(thing))
            result.insert(i);
    }

    return result;
}

// XX these don't need to be member functions
bool ShoppingList::thing_is_item(const CrawlHashTable& thing)
{
    return thing.exists(SHOPPING_THING_ITEM_KEY);
}

const item_def& ShoppingList::get_thing_item(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_ITEM_KEY));

    const item_def &item = thing[SHOPPING_THING_ITEM_KEY].get_item();
    ASSERT(item.defined());

    return item;
}

string ShoppingList::get_thing_desc(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_DESC_KEY));

    string desc = thing[SHOPPING_THING_DESC_KEY].get_string();
    return desc;
}

int ShoppingList::thing_cost(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_COST_KEY));
    return thing[SHOPPING_THING_COST_KEY].get_int();
}

level_pos ShoppingList::thing_pos(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_POS_KEY));
    return thing[SHOPPING_THING_POS_KEY].get_level_pos();
}

string ShoppingList::name_thing(const CrawlHashTable& thing,
                                description_level_type descrip)
{
    if (thing_is_item(thing))
    {
        const item_def &item = get_thing_item(thing);
        return item.name(descrip);
    }
    else
    {
        string desc = get_thing_desc(thing);
        return apply_description(descrip, desc);
    }
}

string ShoppingList::describe_thing(const CrawlHashTable& thing,
                                    description_level_type descrip)
{
    string desc = name_thing(thing, descrip) + " on ";

    const level_pos pos = thing_pos(thing);
    if (pos.id == level_id::current())
        desc += "this level";
    else
        desc += pos.id.describe();

    return desc;
}

// Item name without inscription.
string ShoppingList::item_name_simple(const item_def& item, bool ident)
{
    return item.name(DESC_PLAIN, false, ident, false, false);
}
