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
#include "butcher.h"
#include "cio.h"
#include "colour.h"
#include "decks.h"
#include "describe.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h"
#include "files.h"
#include "food.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "notes.h"
#include "output.h"
#include "place.h"
#include "player.h"
#include "prompt.h"
#include "rot.h"
#include "spl-book.h"
#include "stash.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
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
    ret += 6 * prop[ ARTP_AC ]
            + 6 * prop[ ARTP_EVASION ]
            + 4 * prop[ ARTP_SHIELDING ]
            + 6 * prop[ ARTP_SLAYING ]
            + 3 * prop[ ARTP_STRENGTH ]
            + 3 * prop[ ARTP_INTELLIGENCE ]
            + 3 * prop[ ARTP_DEXTERITY ]
            + 4 * prop[ ARTP_HP ]
            + 3 * prop[ ARTP_MAGICAL_POWER ];

    // These resistances have meaningful levels
    if (prop[ ARTP_FIRE ] > 0)
        ret += 5 + 5 * (prop[ ARTP_FIRE ] * prop[ ARTP_FIRE ]);
    else if (prop[ ARTP_FIRE ] < 0)
        ret -= 10;

    if (prop[ ARTP_COLD ] > 0)
        ret += 5 + 5 * (prop[ ARTP_COLD ] * prop[ ARTP_COLD ]);
    else if (prop[ ARTP_COLD ] < 0)
        ret -= 10;

    if (prop[ ARTP_MAGIC_RESISTANCE ] > 0)
        ret += 4 + 4 * prop[ ARTP_MAGIC_RESISTANCE ];
    else if (prop[ ARTP_MAGIC_RESISTANCE ] < 0)
        ret -= 6;

    if (prop[ ARTP_NEGATIVE_ENERGY ] > 0)
        ret += 3 + 3 * (prop[ARTP_NEGATIVE_ENERGY] * prop[ARTP_NEGATIVE_ENERGY]);

    // Discount Stlth-, charge for Stlth+
    ret += 2 * prop[ARTP_STEALTH];
    // Stlth+ costs more than Stlth- cheapens
    if (prop[ARTP_STEALTH] > 0)
        ret += 2 * prop[ARTP_STEALTH];

    // only one meaningful level:
    if (prop[ ARTP_POISON ])
        ret += 6;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_ELECTRICITY ])
        ret += 10;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_RCORR ])
        ret += 8;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_RMUT ])
        ret += 8;

    if (prop[ ARTP_SEE_INVISIBLE ])
        ret += 6;

    // abilities:
    if (prop[ ARTP_FLY ])
        ret += 3;

    if (prop[ ARTP_BLINK ])
        ret += 10;

    if (prop[ ARTP_BERSERK ])
        ret += 5;

    if (prop[ ARTP_INVISIBLE ])
        ret += 10;

    if (prop[ ARTP_ANGRY ])
        ret -= 3;

    if (prop[ ARTP_CAUSE_TELEPORTATION ])
        ret -= 3;

    if (prop[ ARTP_NOISE ])
        ret -= 5;

    if (prop[ ARTP_PREVENT_TELEPORTATION ])
        ret -= 8;

    if (prop[ ARTP_PREVENT_SPELLCASTING ])
        ret -= 10;

    if (prop[ ARTP_CONTAM ])
        ret -= 8;

    if (prop[ ARTP_CORRODE ])
        ret -= 8;

    if (prop[ ARTP_DRAIN ])
        ret -= 8;

    if (prop[ ARTP_SLOW ])
        ret -= 8;

    if (prop[ ARTP_FRAGILE ])
        ret -= 8;

    if (prop[ ARTP_RMSL ])
        ret += 20;

    if (prop[ ARTP_CLARITY ])
        ret += 20;

    return (ret > 0) ? ret : 0;
}

unsigned int item_value(item_def item, bool ident)
{
    // Note that we pass item in by value, since we want a local
    // copy to mangle as necessary.
    item.flags = (ident) ? (item.flags | ISFLAG_IDENT_MASK) : (item.flags);

    if (is_unrandom_artefact(item)
        && item_ident(item, ISFLAG_KNOW_PROPERTIES))
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

        if (item_type_known(item))
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
            case SPWPN_PENETRATION: // Unrand-only.
                valued *= 25;
                break;

            case SPWPN_CHAOS:
            case SPWPN_DRAINING:
            case SPWPN_FLAMING:
            case SPWPN_FREEZING:
            case SPWPN_HOLY_WRATH:
                valued *= 18;
                break;

            case SPWPN_VORPAL:
                valued *= 15;
                break;

            case SPWPN_PROTECTION:
            case SPWPN_VENOM:
                valued *= 12;
                break;
            }

            valued /= 10;
        }

        if (item_ident(item, ISFLAG_KNOW_PLUSES))
            valued += 50 * item.plus;

        if (is_artefact(item))
        {
            if (item_type_known(item))
                valued += (7 * artefact_value(item));
            else
                valued += 50;
        }
        else if (item_type_known(item)
                 && get_equip_desc(item) != 0) // ???
        {
            valued += 20;
        }
        else if (!(item.flags & ISFLAG_IDENT_MASK)
                 && (get_equip_desc(item) != 0))
        {
            valued += 30; // un-id'd "glowing" - arbitrary added cost
        }

        if (item_known_cursed(item))
            valued -= 30;

        break;

    case OBJ_MISSILES:          // ammunition
        valued += missile_base_price((missile_type)item.sub_type);

        if (item_type_known(item))
        {
            switch (get_ammo_brand(item))
            {
            case SPMSL_NORMAL:
            default:
                valued *= 10;
                break;

            case SPMSL_CHAOS:
                valued *= 40;
                break;

            case SPMSL_CURARE:
            case SPMSL_PARALYSIS:
            case SPMSL_PENETRATION:
            case SPMSL_SILVER:
            case SPMSL_STEEL:
            case SPMSL_DISPERSAL:
                valued *= 30;
                break;

#if TAG_MAJOR_VERSION == 34
            case SPMSL_FLAME:
            case SPMSL_FROST:
#endif
            case SPMSL_SLEEP:
            case SPMSL_CONFUSION:
                valued *= 25;
                break;

            case SPMSL_EXPLODING:
            case SPMSL_POISONED:
#if TAG_MAJOR_VERSION == 34
            case SPMSL_SLOW:
            case SPMSL_SICKNESS:
#endif
            case SPMSL_FRENZY:
                valued *= 20;
                break;
            }

            valued /= 10;
        }
        break;

    case OBJ_ARMOUR:
        valued += armour_base_price((armour_type)item.sub_type);

        if (item_type_known(item))
        {
            const int sparm = get_armour_ego_type(item);
            switch (sparm)
            {
            case SPARM_RUNNING:
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
            case SPARM_MAGIC_RESISTANCE:
            case SPARM_PROTECTION:
            case SPARM_ARCHERY:
            case SPARM_REPULSION:
                valued += 50;
                break;

            case SPARM_POSITIVE_ENERGY:
            case SPARM_POISON_RESISTANCE:
            case SPARM_REFLECTION:
            case SPARM_SPIRIT_SHIELD:
                valued += 20;
                break;

            case SPARM_PONDEROUSNESS:
                valued -= 250;
                break;
            }
        }

        if (item_ident(item, ISFLAG_KNOW_PLUSES))
            valued += 50 * item.plus;

        if (is_artefact(item))
        {
            if (item_type_known(item))
                valued += (7 * artefact_value(item));
            else
                valued += 50;
        }
        else if (item_type_known(item) && get_equip_desc(item) != 0)
            valued += 20;  // ???
        else if (!(item.flags & ISFLAG_IDENT_MASK)
                 && (get_equip_desc(item) != 0))
        {
            valued += 30; // un-id'd "glowing" - arbitrary added cost
        }

        if (item_known_cursed(item))
            valued -= 30;

        break;

    case OBJ_WANDS:
        if (!item_type_known(item))
            valued += 40;
        else
        {
            // true if the wand is of a good type, a type with significant
            // inherent value even when empty. Good wands are less expensive
            // per charge.
            bool good = false;
            switch (item.sub_type)
            {
            case WAND_CLOUDS:
            case WAND_SCATTERSHOT:
                valued += 120;
                good = true;
                break;

            case WAND_ACID:
            case WAND_DIGGING:
                valued += 80;
                good = true;
                break;

            case WAND_ICEBLAST:
            case WAND_LIGHTNING:
            case WAND_DISINTEGRATION:
                valued += 40;
                good = true;
                break;

            case WAND_ENSLAVEMENT:
            case WAND_POLYMORPH:
            case WAND_PARALYSIS:
                valued += 20;
                break;

            case WAND_CONFUSION:
                valued += 15;
                break;

            case WAND_FLAME:
            case WAND_RANDOM_EFFECTS:
                valued += 10;
                break;

            default:
                valued += 6;
                break;
            }

            if (item_ident(item, ISFLAG_KNOW_PLUSES))
            {
                if (good) valued += (valued * item.plus) / 4;
                else      valued += (valued * item.plus) / 2;
            }
        }
        break;

    case OBJ_POTIONS:
        if (!item_type_known(item))
            valued += 9;
        else
        {
            switch (item.sub_type)
            {
            case POT_EXPERIENCE:
                valued += 500;
                break;

#if TAG_MAJOR_VERSION == 34
            case POT_GAIN_DEXTERITY:
            case POT_GAIN_INTELLIGENCE:
            case POT_GAIN_STRENGTH:
            case POT_BENEFICIAL_MUTATION:
                valued += 350;
                break;

            case POT_CURE_MUTATION:
                valued += 250;
                break;
#endif

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
#if TAG_MAJOR_VERSION == 34
            case POT_RESTORE_ABILITIES:
#endif
                valued += 50;
                break;

            case POT_MIGHT:
            case POT_AGILITY:
            case POT_BRILLIANCE:
                valued += 40;
                break;

            case POT_CURING:
            case POT_LIGNIFY:
            case POT_FLIGHT:
                valued += 30;
                break;

#if TAG_MAJOR_VERSION == 34
            case POT_POISON:
            case POT_STRONG_POISON:
            case POT_PORRIDGE:
            case POT_SLOWING:
            case POT_DECAY:
#endif
            case POT_BLOOD:
            case POT_DEGENERATION:
                valued += 10;
                break;

#if TAG_MAJOR_VERSION == 34
            case POT_BLOOD_COAGULATED:
                valued += 5;
                break;
#endif
            }
        }
        break;

    case OBJ_FOOD:
        switch (item.sub_type)
        {
        case FOOD_MEAT_RATION:
        case FOOD_BREAD_RATION:
            valued = 50;
            break;

        case FOOD_ROYAL_JELLY:
            valued = 20;
            break;

        case FOOD_FRUIT:
            valued = 15;
            break;

        case FOOD_CHUNK:
        default:
            break;
        }
        break;

    case OBJ_CORPSES:
        valued = max_corpse_chunks(item.mon_type) * 5;

    case OBJ_SCROLLS:
        if (!item_type_known(item))
            valued += 10;
        else
        {
            switch (item.sub_type)
            {
            case SCR_ACQUIREMENT:
                valued += 520;
                break;

            case SCR_BRAND_WEAPON:
                valued += 200;
                break;

            case SCR_RECHARGING:
            case SCR_SUMMONING:
                valued += 95;
                break;

            case SCR_BLINKING:
            case SCR_ENCHANT_ARMOUR:
            case SCR_ENCHANT_WEAPON:
            case SCR_TORMENT:
            case SCR_HOLY_WORD:
            case SCR_SILENCE:
            case SCR_VULNERABILITY:
                valued += 75;
                break;

            case SCR_AMNESIA:
            case SCR_FEAR:
            case SCR_IMMOLATION:
            case SCR_MAGIC_MAPPING:
                valued += 35;
                break;

            case SCR_REMOVE_CURSE:
            case SCR_TELEPORTATION:
                valued += 30;
                break;

            case SCR_FOG:
            case SCR_IDENTIFY:
#if TAG_MAJOR_VERSION == 34
            case SCR_CURSE_ARMOUR:
            case SCR_CURSE_WEAPON:
            case SCR_CURSE_JEWELLERY:
#endif
                valued += 20;
                break;

            case SCR_NOISE:
            case SCR_RANDOM_USELESSNESS:
                valued += 10;
                break;
            }
        }
        break;

    case OBJ_JEWELLERY:
        if (item_known_cursed(item))
            valued -= 30;

        if (!item_type_known(item))
            valued += 50;
        else
        {
            // Variable-strength rings.
            if (item_ident(item, ISFLAG_KNOW_PLUSES)
                && (item.sub_type == RING_PROTECTION
                    || item.sub_type == RING_STRENGTH
                    || item.sub_type == RING_EVASION
                    || item.sub_type == RING_DEXTERITY
                    || item.sub_type == RING_INTELLIGENCE
                    || item.sub_type == RING_SLAYING
                    || item.sub_type == AMU_REFLECTION))
            {
                // Formula: price = kn(n+1) / 2, where k depends on the subtype,
                // n is the power. (The base variable is equal to 2n.)
                int base = 0;
                int coefficient = 0;
                if (item.sub_type == RING_SLAYING)
                    base = 3 * item.plus;
                else
                    base = 2 * item.plus;

                switch (item.sub_type)
                {
                case RING_SLAYING:
                case RING_PROTECTION:
                case RING_EVASION:
                    coefficient = 40;
                    break;
                case RING_STRENGTH:
                case RING_DEXTERITY:
                case RING_INTELLIGENCE:
                case AMU_REFLECTION:
                    coefficient = 30;
                    break;
                default:
                    break;
                }

                if (base <= 0)
                    valued += 25 * base;
                else
                    valued += (coefficient * base * (base + 1)) / 8;
            }
            else
            {
                switch (item.sub_type)
                {
                case AMU_FAITH:
                case AMU_RAGE:
                    valued += 400;
                    break;

                case RING_WIZARDRY:
                case AMU_REGENERATION:
                case AMU_GUARDIAN_SPIRIT:
                case AMU_THE_GOURMAND:
                case AMU_HARM:
                case AMU_MANA_REGENERATION:
                    valued += 300;
                    break;

                case RING_FIRE:
                case RING_ICE:
                case RING_PROTECTION_FROM_COLD:
                case RING_PROTECTION_FROM_FIRE:
                case RING_PROTECTION_FROM_MAGIC:
                    valued += 250;
                    break;

                case RING_MAGICAL_POWER:
                case RING_LIFE_PROTECTION:
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

                case RING_LOUDNESS:
                case RING_TELEPORTATION:
                case AMU_NOTHING:
                    valued += 75;
                    break;

                case AMU_INACCURACY:
                    valued -= 300;
                    break;
                    // got to do delusion!
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
            valued += 5000;
            break;

        case MISC_FAN_OF_GALES:
        case MISC_PHIAL_OF_FLOODS:
        case MISC_LAMP_OF_FIRE:
        case MISC_LIGHTNING_ROD:
            valued += 400;
            break;

        case MISC_PHANTOM_MIRROR:
            valued += 300;
            break;

        case MISC_BOX_OF_BEASTS:
        case MISC_SACK_OF_SPIDERS:
            valued += 200;
            break;

        default:
            if (is_deck(item))
                valued += 80 + item.deck_rarity * 60;
            else
                valued += 200;
        }
        break;

    case OBJ_BOOKS:
    {
        valued = 150;
        const book_type book = static_cast<book_type>(item.sub_type);
#if TAG_MAJOR_VERSION == 34
        if (book == BOOK_BUGGY_DESTRUCTION)
            break;
#endif

        if (item_type_known(item))
        {
            double rarity = 0;
            if (is_random_artefact(item))
            {
                const vector<spell_type>& spells = spells_in_book(item);

                int rarest = 0;
                for (spell_type spell : spells)
                {
                    rarity += spell_rarity(spell);
                    if (spell_rarity(spell) > rarest)
                        rarest = spell_rarity(spell);
                }
                rarity += rarest * 2;
                rarity /= spells.size();

                // Surcharge for large books.
                if (spells.size() > 6)
                    rarity *= spells.size() / 6;

            }
            else
                rarity = book_rarity(book);

            valued += (int)(rarity * 50.0);
        }
        break;
    }

    case OBJ_STAVES:
        valued = item_type_known(item) ? 250 : 120;
        break;

    case OBJ_ORBS:
        valued = 250000;
        break;

    case OBJ_RUNES:
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
        // Blood potions are worthless because they are easy to make.
        case POT_BLOOD:
#if TAG_MAJOR_VERSION == 34
        case POT_BLOOD_COAGULATED:
        case POT_SLOWING:
        case POT_DECAY:
        case POT_POISON:
#endif
        case POT_DEGENERATION:
            return true;
        default:
            return false;
        }
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case SCR_CURSE_ARMOUR:
        case SCR_CURSE_WEAPON:
        case SCR_CURSE_JEWELLERY:
#endif
        case SCR_NOISE:
        case SCR_RANDOM_USELESSNESS:
            return true;
        default:
            return false;
        }

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
 *  @returns true if it went in your inventory, false otherwise.
 */
static bool _purchase(shop_struct& shop, const level_pos& pos, int index)
{
    item_def item = shop.stock[index]; // intentional copy
    const int cost = item_price(item, shop);
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

    if (fully_identified(item))
        item.flags |= ISFLAG_NOTED_ID;

    you.del_gold(cost);

    you.attribute[ATTR_PURCHASES] += cost;

    origin_purchased(item);

    if (shoptype_identifies_stock(shop.type))
    {
        // Identify the item and its type.
        // This also takes the ID note if necessary.
        set_ident_type(item, true);
        set_ident_flags(item, ISFLAG_IDENT_MASK);
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

static string _hyphenated_letters(int how_many, char first)
{
    string s = "<w>";
    s += first;
    s += "</w>";
    if (how_many > 1)
    {
        s += "-<w>";
        s += first + how_many - 1;
        s += "</w>";
    }
    return s;
}

enum shopping_order
{
    ORDER_DEFAULT,
    ORDER_PRICE,
    ORDER_ALPHABETICAL,
    ORDER_TYPE,
    NUM_ORDERS
};

static const char * const shopping_order_names[NUM_ORDERS] =
{
    "default", "price", "name", "type"
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
    bool looking = false;
    shopping_order order = ORDER_DEFAULT;
    level_pos pos;
    bool can_purchase;

    int selected_cost() const;

    void init_entries();
    void update_help();
    void resort();
    void purchase_selected();

    virtual bool process_key(int keyin) override;
    virtual void draw_menu() override;
    // Selecting one item may remove another from the shopping list, or change
    // the colour others need to have, so we can't be lazy with redrawing
    // elements.
    virtual bool always_redraw() const override { return true; }

public:
    bool bought_something = false;

    ShopMenu(shop_struct& _shop, const level_pos& _pos, bool _can_purchase);
};

class ShopEntry : public InvEntry
{
    ShopMenu& menu;

    string get_text(bool need_cursor = false) const override
    {
        need_cursor = need_cursor && show_cursor;
        const int cost = item_price(*item, menu.shop);
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
            colour_to_str(menu_colour(text, item_prefix(*item), tag));
        return make_stringf(" <%s>%c%c%c%c</%s><%s>%4d gold   %s%s</%s>",
                            keystr.c_str(),
                            hotkeys[0],
                            need_cursor ? '[' : ' ',
                            selected() ? '+' : on_list ? '$' : '-',
                            need_cursor ? ']' : ' ',
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
          menu(m)
    {
        show_background = false;
    }
};

ShopMenu::ShopMenu(shop_struct& _shop, const level_pos& _pos, bool _can_purchase)
    : InvMenu(MF_MULTISELECT | MF_NO_SELECT_QTY | MF_QUIET_SELECT
               | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING),
      shop(_shop),
      pos(_pos),
      can_purchase(_can_purchase)
{
    if (!can_purchase)
        looking = true;

    set_tag("shop");

    init_entries();

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
        add_entry(move(newentry));
    }
}

void ShopMenu::draw_menu()
{
    // Unlike other menus, selecting one item in the shopping menu may change
    // other ones (because of the colour scheme). Keypresses also need to
    // update the more, which is hijacked for use as help text.
#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("msg", "update_menu");
    tiles.json_write_string("more", more.to_colour_string());
    tiles.json_write_int("total_items", items.size());
    tiles.json_close_object();
    tiles.finish_message();
    if (items.size() > 0)
        webtiles_update_items(0, items.size() - 1);
#endif

    InvMenu::draw_menu();
}

int ShopMenu::selected_cost() const
{
    int cost = 0;
    for (auto item : selected_entries())
        cost += item_price(*dynamic_cast<ShopEntry*>(item)->item, shop);
    return cost;
}

void ShopMenu::update_help()
{
    string top_line = make_stringf("<yellow>You have %d gold piece%s.",
                                   you.gold,
                                   you.gold != 1 ? "s" : "");
    const int total_cost = selected_cost();
    if (total_cost > you.gold)
    {
        top_line += "<lightred>";
        top_line +=
            make_stringf(" You are short %d gold piece%s for the purchase.",
                         total_cost - you.gold,
                         (total_cost - you.gold != 1) ? "s" : "");
        top_line += "</lightred>";
    }
    else if (total_cost)
    {
        top_line +=
            make_stringf(" After the purchase, you will have %d gold piece%s.",
                         you.gold - total_cost,
                         (you.gold - total_cost != 1) ? "s" : "");
    }
    top_line += "</yellow>\n";

    set_more(formatted_string::parse_string(top_line + make_stringf(
        //You have 0 gold pieces.
        //[Esc/R-Click] exit  [!] buy|examine items  [a-i] select item for purchase
        //[/] sort (default)  [Enter] make purchase  [A-I] put item on shopping list
#if defined(USE_TILE) && !defined(TOUCH_UI)
        "[<w>Esc</w>/<w>R-Click</w>] exit  "
#else
        //               "/R-Click"
        "[<w>Esc</w>] exit          "
#endif
        "%s  [%s] %s\n"
        "[<w>/</w>] sort (%s)%s  %s  [%s] put item on shopping list",
        !can_purchase ? " " " "  "  " "       "  "          " :
        looking       ? "[<w>!</w>] buy|<w>examine</w> items" :
                        "[<w>!</w>] <w>buy</w>|examine items",
        _hyphenated_letters(item_count(), 'a').c_str(),
        looking ? "examine item" : "select item for purchase",
        shopping_order_names[order],
        // strwidth("default")
        string(7 - strwidth(shopping_order_names[order]), ' ').c_str(),
        !can_purchase ? " " "     "  "               " :
                        "[<w>Enter</w>] make purchase",
        _hyphenated_letters(item_count(), 'A').c_str())));
}

void ShopMenu::purchase_selected()
{
    bool buying_from_list = false;
    vector<MenuEntry*> selected = selected_entries();
    int cost = selected_cost();
    if (selected.empty())
    {
        ASSERT(cost == 0);
        buying_from_list = true;
        for (auto item : items)
        {
            const item_def& it = *dynamic_cast<ShopEntry*>(item)->item;
            if (shopping_list.is_on_list(it, &pos))
            {
                selected.push_back(item);
                cost += item_price(it, shop);
            }
        }
    }
    if (selected.empty())
        return;
    const string col = colour_to_str(channel_to_colour(MSGCH_PROMPT));
    update_help();
    const formatted_string old_more = more;
    if (cost > you.gold)
    {
        more = formatted_string::parse_string(make_stringf(
                   "<%s>You don't have enough money.</%s>\n",
                   col.c_str(),
                   col.c_str()));
        more += old_more;
        draw_menu();
        return;
    }
    more = formatted_string::parse_string(make_stringf(
               "<%s>Purchase items%s for %d gold? (y/N)</%s>\n",
               col.c_str(),
               buying_from_list ? " in shopping list" : "",
               cost,
               col.c_str()));
    more += old_more;
    draw_menu();
    if (!yesno(nullptr, true, 'n', false, false, true))
    {
        more = old_more;
        draw_menu();
        return;
    }
    sort(begin(selected), end(selected),
         [](MenuEntry* a, MenuEntry* b)
         {
             return a->data > b->data;
         });
    vector<int> bought_indices;
    int outside_items = 0;

    // Store last_pickup in case we need to restore it.
    // Then clear it to fill with items purchased.
    map<int,int> tmp_l_p = you.last_pickup;
    you.last_pickup.clear();

    // Will iterate backwards through the shop (because of the earlier sort).
    // This means we can erase() from shop.stock (since it only invalidates
    // pointers to later elements), but nothing else.
    for (auto entry : selected)
    {
        const int i = static_cast<item_def*>(entry->data) - shop.stock.data();
        item_def& item(shop.stock[i]);
        // Can happen if the price changes due to id status
        if (item_price(item, shop) > you.gold)
            continue;
        const int quant = item.quantity;

        if (!_purchase(shop, pos, i))
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
    deleteAll(items);
    init_entries();
    resort();

    if (outside_items)
    {
        more = formatted_string::parse_string(make_stringf(
            "<%s>I'll put %s outside for you.</%s>\n",
            col.c_str(),
            bought_indices.size() == 1             ? "it" :
      (int) bought_indices.size() == outside_items ? "them"
                                                   : "some of them",
            col.c_str()));
        more += old_more;
    }
    else
        update_help();

    draw_menu();
}

// Doesn't handle redrawing itself.
void ShopMenu::resort()
{
    switch (order)
    {
    case ORDER_DEFAULT:
        sort(begin(items), end(items),
             [](MenuEntry* a, MenuEntry* b)
             {
                 return a->data < b->data;
             });
        break;
    case ORDER_PRICE:
        sort(begin(items), end(items),
             [this](MenuEntry* a, MenuEntry* b)
             {
                 return item_price(*dynamic_cast<ShopEntry*>(a)->item, shop)
                        < item_price(*dynamic_cast<ShopEntry*>(b)->item, shop);
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
    case ORDER_TYPE:
        sort(begin(items), end(items),
             [this](MenuEntry* a, MenuEntry* b) -> bool
             {
                 const auto ai = dynamic_cast<ShopEntry*>(a)->item;
                 const auto bi = dynamic_cast<ShopEntry*>(b)->item;
                 if (ai->base_type == bi->base_type)
                     return ai->sub_type < bi->sub_type;
                 else
                     return ai->base_type < bi->base_type;
             });
        break;
    case NUM_ORDERS:
        die("invalid ordering");
    }
    for (size_t i = 0; i < items.size(); ++i)
        items[i]->hotkeys[0] = index_to_letter(i);
}

bool ShopMenu::process_key(int keyin)
{
    switch (keyin)
    {
    case '!':
    case '?':
        if (can_purchase)
        {
            looking = !looking;
            update_help();
            draw_menu();
        }
        return true;
    case ' ':
    case CK_MOUSE_CLICK:
    case CK_ENTER:
        if (can_purchase)
            purchase_selected();
        return true;
    case '$':
    {
        const vector<MenuEntry*> selected = selected_entries();
        if (!selected.empty())
        {
            // Move selected to shopping list.
            for (auto entry : selected)
            {
                const item_def& item = *dynamic_cast<ShopEntry*>(entry)->item;
                entry->selected_qty = 0;
                if (!shopping_list.is_on_list(item, &pos))
                    shopping_list.add_thing(item, item_price(item, shop), &pos);
            }
        }
        else
            // Move shoplist to selection.
            for (auto entry : items)
                if (shopping_list.is_on_list(*dynamic_cast<ShopEntry*>(entry)->item, &pos))
                    entry->select(-2);
        // Move shoplist to selection.
        draw_menu();
        return true;
    }
    case '/':
        ++order;
        resort();
        update_help();
        draw_menu();
        return true;
    default:
        break;
    }

    if (keyin - 'a' >= 0 && keyin - 'a' < (int)items.size() && looking)
    {
        item_def& item(*const_cast<item_def*>(dynamic_cast<ShopEntry*>(
            items[letter_to_index(keyin)])->item));
        // A hack to make the description more useful.
        // In theory, the user could kill the process at this
        // point and end up with valid ID for the item.
        // That's not very useful, though, because it doesn't set
        // type-ID and once you can access the item (by buying it)
        // you have its full ID anyway. Worst case, it won't get
        // noted when you buy it.
        {
            unwind_var<iflags_t> old_flags(item.flags);
            if (shoptype_identifies_stock(shop.type))
            {
                item.flags |= (ISFLAG_IDENT_MASK | ISFLAG_NOTED_ID
                               | ISFLAG_NOTED_GET);
            }
            describe_item(item);
        }
        draw_menu();
        return true;
    }
    else if (keyin - 'A' >= 0 && keyin - 'A' < (int)items.size())
    {
        const auto index = letter_to_index(keyin) % 26;
        auto entry = dynamic_cast<ShopEntry*>(items[index]);
        entry->selected_qty = 0;
        const item_def& item(*entry->item);
        if (shopping_list.is_on_list(item, &pos))
            shopping_list.del_thing(item, &pos);
        else
            shopping_list.add_thing(item, item_price(item, shop), &pos);
        // not draw_item since other items may enter/leave shopping list
        draw_menu();
        return true;
    }

    auto old_selected = selected_entries();
    bool ret = InvMenu::process_key(keyin);
    if (old_selected != selected_entries())
    {
        // Update the footer to display the new $$$ info.
        update_help();
        draw_menu();
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
    redraw_screen();
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
        grd(p) = DNGN_ABANDONED_SHOP;
        unnotice_feature(level_pos(level_id::current(), p));
    }
}

shop_struct *shop_at(const coord_def& where)
{
    if (grd(where) != DNGN_ENTER_SHOP)
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
        case SHOP_EVOKABLES:
            return "Gadget";
        case SHOP_BOOK:
            return "Book";
        case SHOP_FOOD:
            return "Food";
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

bool shop_item_unknown(const item_def &item)
{
    return item_type_has_ids(item.base_type)
           && item_type_known(item)
           && !get_ident_type(item)
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
    "gadget",
    "book",
    "food",
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

void list_shop_types()
{
    mpr_nojoin(MSGCH_PLAIN, "Available shop types: ");
    for (const char *type : shop_types)
        mprf_nocap("%s", type);
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

#define SETUP_THING()                             \
    CrawlHashTable *thing = new CrawlHashTable();  \
    (*thing)[SHOPPING_THING_COST_KEY] = cost; \
    (*thing)[SHOPPING_THING_POS_KEY]  = pos;

bool ShoppingList::add_thing(const item_def &item, int cost,
                             const level_pos* _pos)
{
    ASSERT(item.defined());
    ASSERT(cost > 0);

    SETUP_POS();

    if (find_thing(item, pos) != -1)
    {
        mprf(MSGCH_ERROR, "%s is already on the shopping list.",
             item.name(DESC_THE).c_str());
        return false;
    }

    SETUP_THING();
    (*thing)[SHOPPING_THING_ITEM_KEY] = item;
    list->push_back(*thing);
    refresh();

    return true;
}

bool ShoppingList::add_thing(string desc, string buy_verb, int cost,
                             const level_pos* _pos)
{
    ASSERT(!desc.empty());
    ASSERT(!buy_verb.empty());
    ASSERT(cost > 0);

    SETUP_POS();

    if (find_thing(desc, pos) != -1)
    {
        mprf(MSGCH_ERROR, "%s is already on the shopping list.",
             desc.c_str());
        return false;
    }

    SETUP_THING();
    (*thing)[SHOPPING_THING_DESC_KEY] = desc;
    (*thing)[SHOPPING_THING_VERB_KEY] = buy_verb;
    list->push_back(*thing);
    refresh();

    return true;
}

#undef SETUP_THING

bool ShoppingList::is_on_list(const item_def &item, const level_pos* _pos) const
{
    SETUP_POS();

    return find_thing(item, pos) != -1;
}

bool ShoppingList::is_on_list(string desc, const level_pos* _pos) const
{
    SETUP_POS();

    return find_thing(desc, pos) != -1;
}

void ShoppingList::del_thing_at_index(int idx)
{
    ASSERT_RANGE(idx, 0, list->size());
    list->erase(idx);
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

    int idx = find_thing(item, pos);

    if (idx == -1)
    {
        mprf(MSGCH_ERROR, "%s isn't on shopping list, can't delete it.",
             item.name(DESC_THE).c_str());
        return false;
    }

    del_thing_at_index(idx);
    return true;
}

bool ShoppingList::del_thing(string desc, const level_pos* _pos)
{
    SETUP_POS();

    int idx = find_thing(desc, pos);

    if (idx == -1)
    {
        mprf(MSGCH_ERROR, "%s isn't on shopping list, can't delete it.",
             desc.c_str());
        return false;
    }

    del_thing_at_index(idx);
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
        // Only these are really interchangeable.
        break;
    case OBJ_MISCELLANY:
        // ... and a few of these.
        switch (item.sub_type)
        {
            case MISC_CRYSTAL_BALL_OF_ENERGY:
                break;
            default:
                if (!is_xp_evoker(item))
                    return 0;
        }
        break;
    default:
        return 0;
    }

    if (!item_type_known(item) || is_artefact(item))
        return 0;

    // Ignore stat-modification rings which reduce a stat, since they're
    // worthless.
    if (item.base_type == OBJ_JEWELLERY && item.plus < 0)
        return 0;

    // Manuals are consumable, and interesting enough to keep on list.
    if (item.is_type(OBJ_BOOKS, BOOK_MANUAL))
        return 0;

    // Item is already on shopping-list.
    const bool on_list = find_thing(item, level_pos::current()) != -1;

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

        if (!item_type_known(list_item) || is_artefact(list_item))
            continue;

        // Don't prompt to remove rings with strictly better pluses
        // than the new one. Also, don't prompt to remove rings with
        // known pluses when the new ring's pluses are unknown.
        if (item.base_type == OBJ_JEWELLERY)
        {
            const bool has_plus = jewellery_has_pluses(item);
            const int delta_p = item.plus - list_item.plus;
            if (has_plus
                && item_ident(list_item, ISFLAG_KNOW_PLUSES)
                && (!item_ident(item, ISFLAG_KNOW_PLUSES)
                     || delta_p < 0))
            {
                continue;
            }
        }

        // Don't prompt to remove known manuals when the new one is unknown
        // or for a different skill.
        if (item.is_type(OBJ_BOOKS, BOOK_MANUAL)
            && item_type_known(list_item)
            && (!item_type_known(item) || item.plus != list_item.plus))
        {
            continue;
        }

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

void ShoppingList::item_type_identified(object_class_type base_type,
                                        int sub_type)
{
    // Dead men can't update their shopping lists.
    if (!crawl_state.need_save)
        return;

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

void ShoppingList::remove_dead_shops()
{
    // Only restore the excursion at the very end.
    level_excursion le;

    set<level_pos> shops_to_remove;

    for (CrawlHashTable &thing : *list)
    {
        const level_pos place = thing_pos(thing);
        le.go_to(place.id); // thereby running DACT_REMOVE_GOZAG_SHOPS
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
        return; // Shopping list is unitialized and uneeded.
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
    ShoppingListMenu()
#ifdef USE_TILE_LOCAL
        : Menu(MF_MULTISELECT,"",false)
#else
        : Menu()
#endif
    { }

protected:
    void draw_title();
};

void ShoppingListMenu::draw_title()
{
    if (title)
    {
        const int total_cost = you.props[SHOPPING_LIST_COST_KEY];

        cgotoxy(1, 1);
        formatted_string fs = formatted_string(title->colour);
        fs.cprintf("%d %s%s, total %d gold",
                   title->quantity, title->text.c_str(),
                   title->quantity > 1? "s" : "",
                   total_cost);
        fs.display();

#ifdef USE_TILE_WEB
        webtiles_set_title(fs);
#endif
        string s = "<lightgrey>  [<w>a-z</w>] ";

        switch (menu_action)
        {
        case ACT_EXECUTE:
            s += "<w>travel</w>|examine|delete";
            break;
        case ACT_EXAMINE:
            s += "travel|<w>examine</w>|delete";
            break;
        default:
            s += "travel|examine|<w>delete</w>";
            break;
        }

        s += "  [<w>?</w>/<w>!</w>] change action</lightgrey>";

        draw_title_suffix(formatted_string::parse_string(s), false);
    }
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

        if (cost > you.gold)
            me->colour = DARKGREY;
        else if (thing_is_item(thing))
        {
            // Colour shopping list item according to menu colours.
            const item_def &item = get_thing_item(thing);

            const string colprf = item_prefix(item);
            const int col = menu_colour(item.name(DESC_A),
                                        colprf, "shop");

            if (col != -1)
                me->colour = col;
        }

        shopmenu.add_entry(me);
        ++hotkey;
    }
}

void ShoppingList::display()
{
    if (list->empty())
        return;

    ShoppingListMenu shopmenu;
    shopmenu.set_tag("shop");
    shopmenu.menu_action  = Menu::ACT_EXECUTE;
    shopmenu.action_cycle = Menu::CYCLE_CYCLE;
    string title          = "thing";

    MenuEntry *mtitle = new MenuEntry(title, MEL_TITLE);
    shopmenu.set_title(mtitle);

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (list->size() > 52
        && (shopmenu.maxpagesize() > 52 || shopmenu.maxpagesize() == 0))
    {
        shopmenu.set_maxpagesize(52);
    }

    string more_str = make_stringf("<yellow>You have %d gp</yellow>", you.gold);
    shopmenu.set_more(formatted_string::parse_string(more_str));

    shopmenu.set_flags(MF_SINGLESELECT | MF_ALWAYS_SHOW_MORE
                        | MF_ALLOW_FORMATTING);

    fill_out_menu(shopmenu);

    vector<MenuEntry*> sel;
    while (true)
    {
        // Abuse of the quantity field.
        mtitle->quantity = list->size();

        redraw_screen();
        sel = shopmenu.show();

        if (sel.empty())
            break;

        const CrawlHashTable* thing =
            static_cast<const CrawlHashTable *>(sel[0]->data);

        const bool is_item = thing_is_item(*thing);

        if (shopmenu.menu_action == Menu::ACT_EXECUTE)
        {
            int cost = thing_cost(*thing);

            if (cost > you.gold)
            {
                string prompt =
                   make_stringf("You cannot afford %s; travel there "
                                "anyway? (y/N)",
                                describe_thing(*thing, DESC_A).c_str());
                clrscr();
                if (!yesno(prompt.c_str(), true, 'n'))
                    continue;
            }

            const level_pos lp(thing_pos(*thing));
            start_translevel_travel(lp);
            break;
        }
        else if (shopmenu.menu_action == Menu::ACT_EXAMINE)
        {
            clrscr();
            if (is_item)
            {
                const item_def &item = get_thing_item(*thing);
                describe_item(const_cast<item_def&>(item));
            }
            else // not an item, so we only stored a description.
            {
                // HACK: Assume it's some kind of portal vault.
                const string info = make_stringf(
                             "%s with an entry fee of %d gold pieces.",
                             describe_thing(*thing, DESC_A).c_str(),
                             (int) thing_cost(*thing));

                print_description(info.c_str());
                getchm();
            }
        }
        else if (shopmenu.menu_action == Menu::ACT_MISC)
        {
            const int index = shopmenu.get_entry_index(sel[0]);
            if (index == -1)
            {
                mprf(MSGCH_ERROR, "ERROR: Unable to delete thing from shopping list!");
                more();
                continue;
            }

            del_thing_at_index(index);
            if (list->empty())
            {
                mpr("Your shopping list is now empty.");
                break;
            }

            shopmenu.clear();
            fill_out_menu(shopmenu);
        }
        else
            die("Invalid menu action type");
    }
    redraw_screen();
}

static bool _compare_shopping_things(const CrawlStoreValue& a,
                                     const CrawlStoreValue& b)
{
    const CrawlHashTable& hash_a = a.get_table();
    const CrawlHashTable& hash_b = b.get_table();

    const int a_cost = hash_a[SHOPPING_THING_COST_KEY];
    const int b_cost = hash_b[SHOPPING_THING_COST_KEY];

    return a_cost < b_cost;
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

int ShoppingList::find_thing(const item_def &item,
                             const level_pos &pos) const
{
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];
        const level_pos       _pos  = thing_pos(thing);

        if (pos != _pos)
            continue;

        if (!thing_is_item(thing))
            continue;

        const item_def &_item = get_thing_item(thing);

        if (item_name_simple(item) == item_name_simple(_item))
            return i;
    }

    return -1;
}

int ShoppingList::find_thing(const string &desc, const level_pos &pos) const
{
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];
        const level_pos       _pos  = thing_pos(thing);

        if (pos != _pos)
            continue;

        if (thing_is_item(thing))
            continue;

        if (desc == name_thing(thing))
            return i;
    }

    return -1;
}

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
    const level_pos pos = thing_pos(thing);

    string desc = name_thing(thing, descrip) + " on ";

    if (pos.id == level_id::current())
        desc += "this level";
    else
        desc += pos.id.describe();

    return desc;
}

// Item name without curse-status or inscription.
string ShoppingList::item_name_simple(const item_def& item, bool ident)
{
    return item.name(DESC_PLAIN, false, ident, false, false,
                     ISFLAG_KNOW_CURSE);
}
