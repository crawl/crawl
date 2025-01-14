/*** Item handling functions and type.
 * @module items
 */
#include "AppHdr.h"

#include "l-libs.h"

#include <algorithm>
#include <sstream>

#include "adjust.h"
#include "acquire.h"
#include "artefact.h"
#include "art-enum.h"
#include "cluautil.h"
#include "colour.h"
#include "coord.h"
#include "describe.h"
#include "env.h"
#include "evoke.h" // evoke_damage_string()
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "mapdef.h" // item_spec
#include "mon-util.h"
#include "mpr.h"
#include "output.h"
#include "player.h"
#include "player-equip.h"
#include "prompt.h"
#include "shopping.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stash.h"
#include "stringutil.h"
#include "throw.h"

struct item_wrapper
{
    item_def *item;
    bool temp; // whether `item` is being memory managed by this object or
               // elsewhere; if true, will be deleted on gc.
    int turn;

    bool valid(lua_State *ls) const
    {
        // TODO: under what circumstances will dlua actually need to deal with
        // wrapped items that were created on a different turn?
        return item && (!CLua::get_vm(ls).managed_vm || turn == you.num_turns);
    }
};

void clua_push_item(lua_State *ls, item_def *item)
{
    item_wrapper *iw = clua_new_userdata<item_wrapper>(ls, ITEM_METATABLE);
    iw->item = item;
    iw->temp = false;
    iw->turn = you.num_turns;
}

// Push a (wrapped) temporary item_def. A copy of the item will be allocated,
// then deleted when the wrapper is GCed.
static void _clua_push_item_temp(lua_State *ls, const item_def &item)
{
    item_wrapper *iw = clua_new_userdata<item_wrapper>(ls, ITEM_METATABLE);
    iw->item = new item_def(item);
    iw->temp = true;
    iw->turn = you.num_turns;
}

item_def *clua_get_item(lua_State *ls, int ndx)
{
    item_wrapper *iwrap =
        clua_get_userdata<item_wrapper>(ls, ITEM_METATABLE, ndx);
    if (!iwrap->valid(ls))
        luaL_error(ls, "Invalid item");
    return iwrap->item;
}

void lua_push_floor_items(lua_State *ls, int link)
{
    lua_newtable(ls);
    int index = 0;
    for (; link != NON_ITEM; link = env.item[link].link)
    {
        clua_push_item(ls, &env.item[link]);
        lua_rawseti(ls, -2, ++index);
    }
}

static void _lua_push_inv_items(lua_State *ls = nullptr)
{
    if (!ls)
        ls = clua.state();
    lua_newtable(ls);
    int index = 0;
    for (item_def &item : you.inv)
    {
        if (item.defined())
        {
            clua_push_item(ls, &item);
            lua_rawseti(ls, -2, ++index);
        }
    }
}

#define IDEF(name)                                                      \
    static int l_item_##name(lua_State *ls, item_def *item)             \

#define IDEFN(name, closure)                    \
    static int l_item_##name(lua_State *ls, item_def *item) \
    {                                                                   \
        clua_push_item(ls, item);                                            \
        lua_pushcclosure(ls, l_item_##closure, 1);                      \
        return 1;                                                     \
    }

#define ITEM(name, ndx) \
    item_def *name = clua_get_item(ls, ndx)

#define UDATA_ITEM(name) ITEM(name, lua_upvalueindex(1))

/*** Information and interaction with a single item.
 * @type Item
 */

static int l_item_do_equip(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item) || !is_equippable_item(*item))
        return 0;

    bool success = try_equip_item(*item);
    lua_pushboolean(ls, success);
    return 1;
}

/*** Equip this item.
 * @treturn boolean successfully equipped
 * @function equip
 */
IDEFN(equip, do_equip)

static int l_item_do_remove(lua_State *ls)
{
    if (you.turn_is_over)
    {
        mpr("Turn is over");
        return 0;
    }

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item))
    {
        mpr("Bad item");
        return 0;
    }

    if (!item_is_equipped(*item))
    {
        mpr("Item is not equipped");
        return 0;
    }

    bool result = try_unequip_item(*item);
    lua_pushboolean(ls, result);
    return 1;
}

/*** Remove this item from our body.
 * @treturn boolean|nil successfully removed something; nil if nothing to
 * remove
 * @function remove
 */
IDEFN(remove, do_remove)

static int l_item_do_drop(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item))
        return 0;

    if (item_is_equipped(*item))
    {
        lua_pushboolean(ls, false);
        lua_pushstring(ls, "Can't drop equipped items");
        return 2;
    }

    int qty = item->quantity;
    if (lua_isnumber(ls, 1))
    {
        int q = luaL_safe_checkint(ls, 1);
        if (q >= 1 && q <= item->quantity)
            qty = q;
    }
    lua_pushboolean(ls, drop_item(item->link, qty));
    return 1;
}

/*** Drop this.
 * Optionally specify how many for partially dropping a stack.
 * @tparam[opt] int qty
 * @treturn boolean successfully dropped
 * @function drop
 */
IDEFN(drop, do_drop)

/*** Is this equipped?
 * @field equipped boolean
 */
IDEF(equipped)
{
    lua_pushboolean(ls, item_is_equipped(*item));

    return 1;
}

static int l_item_do_class(lua_State *ls)
{
    UDATA_ITEM(item);

    if (item)
    {
        bool terse = false;
        if (lua_isboolean(ls, 1))
            terse = lua_toboolean(ls, 1);

        string s = item_class_name(item->base_type, terse);
        lua_pushstring(ls, s.c_str());
    }
    else
        lua_pushnil(ls);
    return 1;
}

/*** What is the item class?
 * @tparam[opt=false] boolean terse
 * @treturn string
 * @function class
 */
IDEFN(class, do_class)

static int l_item_do_subtype(lua_State *ls)
{
    UDATA_ITEM(item);

    if (!item)
    {
        lua_pushnil(ls);
        return 1;
    }

    const char *s = nullptr;
    string saved;

    // Special-case OBJ_ARMOUR behavior to maintain compatibility with
    // existing scripts.
    if (item->base_type == OBJ_ARMOUR)
    {
        equipment_slot slot = get_armour_slot(*item);
        s = (slot == SLOT_BODY_ARMOUR ? "body"
                                      : lowercase_string(equip_slot_name(slot)).c_str());
    }
    else if (item->is_identified() || item->base_type == OBJ_WEAPONS)
    {
        // must keep around the string until we call lua_pushstring
        saved = sub_type_string(*item);
        s = saved.c_str();
    }

    if (s)
        lua_pushstring(ls, s);
    else
        lua_pushnil(ls);

    return 1;
}

/*** What is the subtype?
 * @treturn string|nil the item's subtype, if any
 * @function subtype
 */
IDEFN(subtype, do_subtype)

static int l_item_do_is_shield(lua_State *ls)
{
    UDATA_ITEM(item);

    if (!item)
    {
        lua_pushnil(ls);
        return 1;
    }

    lua_pushboolean(ls, is_shield(item));

    return 1;
}

/*** Is this item a shield?
 * @treturn bool true, if this item is a shield
 */
IDEFN(is_shield, do_is_shield)


static int l_item_do_ego(lua_State *ls)
{
    UDATA_ITEM(item);
    if (!item)
    {
        lua_pushnil(ls);
        return 1;
    }

    bool terse = false;
    if (lua_isboolean(ls, 1))
        terse = lua_toboolean(ls, 1);

    if (item->is_identified() || item->base_type == OBJ_MISSILES)
    {
        const string s = ego_type_string(*item, terse);
        if (!s.empty())
        {
            lua_pushstring(ls, s.c_str());
            return 1;
        }
    }

    lua_pushnil(ls);
    return 1;
}

/*** What is the ego?
 * @tparam[opt=false] boolean terse
 * @treturn string|nil the item's ego, if any
 * @function ego
 */
IDEFN(ego, do_ego)

/*** Is this item cursed?
 * @field cursed boolean
 */
IDEF(cursed)
{
    lua_pushboolean(ls, item && item->cursed());
    return 1;
}

static string _item_name(lua_State *ls, item_def* item)
{
    description_level_type ndesc = DESC_PLAIN;
    if (lua_isstring(ls, 1))
        ndesc = description_type_by_name(lua_tostring(ls, 1));
    else if (lua_isnumber(ls, 1))
        ndesc = static_cast<description_level_type>(luaL_safe_checkint(ls, 1));
    const bool terse = lua_toboolean(ls, 2);
    return item->name(ndesc, terse);
}

static int l_item_do_name(lua_State *ls)
{
    UDATA_ITEM(item);

    if (item)
        lua_pushstring(ls, _item_name(ls, item).c_str());
    else
        lua_pushnil(ls);
    return 1;
}

/*** Get this item's name.
 * For the levels of item descriptions see @{crawl.grammar}
 * @tparam[opt="plain"] string desc description type
 * @tparam[opt="false"] boolean terse
 * @treturn string
 * @function name
 */
IDEFN(name, do_name)

static int l_item_do_name_coloured(lua_State *ls)
{
    UDATA_ITEM(item);

    if (item)
    {
        string name   = _item_name(ls, item);
        int    col    = menu_colour(name, item_prefix(*item), "item", false);
        string colstr = colour_to_str(col);

        ostringstream out;

        out << "<" << colstr << ">" << name << "</" << colstr << ">";

        lua_pushstring(ls, out.str().c_str());
    }
    else
        lua_pushnil(ls);
    return 1;
}

/*** Get this item's name as a colour-formatted string.
 * Adds item colouring suitable for @{crawl.formatted_mpr}.
 * @tparam[opt="plain"] string desc description type
 * @tparam[opt="false"] boolean terse
 * @treturn string
 * @function name_coloured
 */
IDEFN(name_coloured, do_name_coloured)

static int l_item_do_stacks(lua_State *ls)
{
    UDATA_ITEM(first);

    if (!first)
        lua_pushnil(ls);
    else if (lua_gettop(ls) == 0 || lua_isnil(ls, 1))
    {
        const bool any_stack =
            is_stackable_item(*first)
            && any_of(begin(you.inv), end(you.inv),
                      [first] (const item_def &item) -> bool
                      {
                          return items_stack(*first, item);
                      })
         || first->base_type == OBJ_WANDS
            && any_of(begin(you.inv), end(you.inv),
                      [first] (const item_def &item) -> bool
                      {
                          return item.is_type(OBJ_WANDS, first->sub_type);
                      });
        lua_pushboolean(ls, any_stack);
    }
    else if (ITEM(second, 1))
        lua_pushboolean(ls, items_stack(*first, *second));
    else
        lua_pushnil(ls);
    return 1;
}

IDEFN(stacks, do_stacks)

/*** How many do we have in a stack?
 * @field quantity int
 */
IDEF(quantity)
{
    PLUARET(number, item? item->quantity : 0);
}

/*** This item's inventory slot.
 * @field slot index or nil
 */
IDEF(slot)
{
    if (item && in_inventory(*item))
        lua_pushnumber(ls, item->link);
    else
        lua_pushnil(ls);
    return 1;
}

/*** Is this item in the inventory?
 * @field ininventory boolean
 */
IDEF(ininventory)
{
    PLUARET(boolean, item && in_inventory(*item));
}

/*** The default slot type this item goes in.
 * @field equip_type int
 */
IDEF(equip_type)
{
    if (!item || !item->defined())
        return 0;

    equipment_slot eq = get_item_slot(*item);

    if (eq != SLOT_UNUSED)
        lua_pushnumber(ls, eq);
    else
        lua_pushnil(ls);
    return 1;
}

/*** The weapon skill this item requires.
 * @field weap_skill string|nil nil for non-weapons
 */
IDEF(weap_skill)
{
    if (!item || !item->defined())
        return 0;

    const skill_type skill = item_attack_skill(*item);
    if (skill == SK_FIGHTING)
        return 0;

    if (is_unrandom_artefact(*item, UNRAND_LOCHABER_AXE))
    {
        lua_pushstring(ls, make_stringf("%s,%s",
                                        skill_name(SK_POLEARMS),
                                        skill_name(SK_AXES)).c_str());
    }
    else
        lua_pushstring(ls, skill_name(skill));
    lua_pushnumber(ls, skill);
    return 2;
}

/*** The reach range of this item.
 * @field reach_range int
 */
IDEF(reach_range)
{
    if (!item || !item->defined())
        return 0;

    reach_type rt = weapon_reach(*item);
    lua_pushnumber(ls, rt);
    return 1;
}

/*** Is this a ranged weapon?
 * @field is_ranged boolean
 */
IDEF(is_ranged)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_range_weapon(*item));

    return 1;
}

/*** Can this be thrown effectively?
 * @field is_throwable boolean
 */
IDEF(is_throwable)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_throwable(&you, *item));

    return 1;
}

/*** Is this an elemental evoker?
 * @field is_xp_evoker boolean
 */
IDEF(is_xp_evoker)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_xp_evoker(*item));

    return 1;
}

/*** Did we drop this?
 * @field dropped boolean
 */
IDEF(dropped)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item->flags & ISFLAG_DROPPED);

    return 1;
}

/*** Is this item currently melded to us?
 * @field is_melded boolean
 */
IDEF(is_melded)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item_is_melded(*item));

    return 1;
}

/*** Is this item a weapon?
 * @field is_weapon boolean
 */
IDEF(is_weapon)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_weapon(*item));

    return 1;
}

/*** Is this a corpse?
 * @field is_corpse boolean
 */
IDEF(is_corpse)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item->is_type(OBJ_CORPSES, CORPSE_BODY));

    return 1;
}

/*** Is this useless?
 * @field is_useless boolean
 */
IDEF(is_useless)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_useless_item(*item));

    return 1;
}

/*** Is this an artefact?
 * @field artefact boolean
 */
IDEF(artefact)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_artefact(*item));

    return 1;
}

/*** Is this branded?
 * @field branded
 */
IDEF(branded)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item->is_identified() ? item_is_branded(*item)
                                : item->flags & ISFLAG_COSMETIC_MASK);
    return 1;
}

IDEF(hands)
{
    if (!item || !item->defined() || !is_weapon(*item))
        return 0;

    int hands = you.hands_reqd(*item) == HANDS_TWO ? 2 : 1;
    lua_pushnumber(ls, hands);

    return 1;
}

/*** Is this a god gift?
 * @field god_gift boolean
 */
IDEF(god_gift)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, origin_as_god_gift(*item) != GOD_NO_GOD);

    return 1;
}

/*** Is this fully identified?
 * @field is_identified boolean
 */
IDEF(is_identified)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item->is_identified());

    return 1;
}

/*** Item plus.
 * @field plus int
 */
IDEF(plus)
{
    if (!item || !item->defined())
        return 0;

    if (item->is_identified()
        && (item->base_type == OBJ_WEAPONS || item->base_type == OBJ_ARMOUR
            || item->base_type == OBJ_WANDS
            || item->base_type == OBJ_JEWELLERY
               && (item->sub_type == RING_PROTECTION
                   || item->sub_type == RING_STRENGTH
                   || item->sub_type == RING_SLAYING
                   || item->sub_type == RING_EVASION
                   || item->sub_type == RING_DEXTERITY
                   || item->sub_type == RING_INTELLIGENCE
                   || item->sub_type == AMU_REFLECTION)))
    {
        lua_pushnumber(ls, item->plus);
    }
    else
        lua_pushnil(ls);

    return 1;
}

/*** Is this item enchantable?
 * @field is_enchantable boolean
 */
IDEF(is_enchantable)
{
    if (!item || !item->defined())
        return 0;

    if (is_artefact(*item)
        || item->base_type != OBJ_WEAPONS && item->base_type != OBJ_ARMOUR)
    {
        lua_pushboolean(ls, false);
    }
    // We assume unidentified non-artefact items are enchantable.
    else if (!item->is_identified())
        lua_pushboolean(ls, true);
    else if (item->base_type == OBJ_WEAPONS)
        lua_pushboolean(ls, is_enchantable_weapon(*item));
    else
        lua_pushboolean(ls, is_enchantable_armour(*item));

    return 1;
}

IDEF(plus2)
{
    if (!item || !item->defined())
        return 0;

    lua_pushnil(ls);

    return 1;
}

/*** What spells are in this?
 * @field spells array of spell names
 */
IDEF(spells)
{
    if (!item || !item->defined() || !item->has_spells())
        return 0;

    int index = 0;
    lua_newtable(ls);

    for (spell_type stype : spells_in_book(*item))
    {
        lua_pushstring(ls, spell_title(stype));
        lua_rawseti(ls, -2, ++index);
    }

    return 1;
}

/*** Artefact properties
 * Full list in `artefact-prop-type.h`
 * @field artprops table artefact property table
 */
IDEF(artprops)
{
    if (!item || !item->defined() || !is_artefact(*item)
        || !item->is_identified()
        || item->base_type == OBJ_BOOKS)
    {
        return 0;
    }

    lua_newtable(ls);

    for (int i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        int value = artefact_property(*item, (artefact_prop_type)i);
        if (value)
        {
            lua_pushstring(ls, artp_name((artefact_prop_type)i));
            lua_pushnumber(ls, value);
            lua_settable(ls, -3);
        }
    }

    return 1;
}

/*** Item base damage.
 * @field damage int
 */
IDEF(damage)
{
    if (!item || !item->defined())
        return 0;

    if (is_weapon(*item)
        || item->base_type == OBJ_MISSILES)
    {
        lua_pushnumber(ls, property(*item, PWPN_DAMAGE));
    }
    else
        lua_pushnil(ls);

    return 1;
}

static int l_item_do_damage_rating(lua_State *ls)
{
    UDATA_ITEM(item);

    if (!item || !item->defined())
        return 0;


    if (is_weapon(*item)
        || item->base_type == OBJ_MISSILES)
    {
        int rating = 0;
        string rating_desc = damage_rating(item, &rating);
        lua_pushnumber(ls, rating);
        lua_pushstring(ls, rating_desc.c_str());
    }
    else
    {
        lua_pushnil(ls);
        lua_pushnil(ls);
    }

    return 2;
}

/*** Item damage rating.
 * @treturn number The item's damage rating.
 * @treturn string The item's full damage rating string.
 * @function damage_rating
 */
IDEFN(damage_rating, do_damage_rating)

/*** Item evoke damage.
 * @field evoke_damage string The evokable item's damage string.
 */
IDEF(evoke_damage)
{
    if (!item || !item->defined())
        return 0;

    const string damage_str = evoke_damage_string(*item);
    lua_pushstring(ls, damage_str.c_str());
    return 1;
}

/*** Item base accuracy.
 * @field accuracy int
 */
IDEF(accuracy)
{
    if (!item || !item->defined())
        return 0;

    if (is_weapon(*item))
        lua_pushnumber(ls, property(*item, PWPN_HIT));
    else
        lua_pushnil(ls);

    return 1;
}

/*** Item base delay.
 * @field delay int
 */
IDEF(delay)
{
    if (!item || !item->defined())
        return 0;

    if (is_weapon(*item))
        lua_pushnumber(ls, property(*item, PWPN_SPEED));
    else
        lua_pushnil(ls);

    return 1;
}

/*** Item base AC.
 * @field ac int
 */
IDEF(ac)
{
    if (!item || !item->defined())
        return 0;

    if (item->base_type == OBJ_ARMOUR)
        lua_pushnumber(ls, property(*item, PARM_AC));
    else
        lua_pushnil(ls);

    return 1;
}

/*** Item encumbrance rating.
 * @field encumbrance int
 */
IDEF(encumbrance)
{
    if (!item || !item->defined())
        return 0;

    if (item->base_type == OBJ_ARMOUR)
        lua_pushnumber(ls, -property(*item, PARM_EVASION) / 10);
    else
        lua_pushnil(ls);

    return 1;
}

/*** Item is for sale.
 * @field is_in_shop boolean
 */
IDEF(is_in_shop)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_shop_item(*item));

    return 1;
}

/*** Item inscription string.
 * @field inscription string
 */
IDEF(inscription)
{
    if (!item || !item->defined())
        return 0;

    lua_pushstring(ls, item->inscription.c_str());

    return 1;
}

/*** Item description string, as displayed in the game UI.
 * @field description string
 */
IDEF(description)
{
    if (!item || !item->defined())
        return 0;

    lua_pushstring(ls, get_item_description(*item).c_str());

    return 1;
}


// DLUA-only functions
static int l_item_do_pluses(lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined() || !item->is_identified())
    {
        lua_pushboolean(ls, false);
        return 1;
    }

    lua_pushnumber(ls, item->plus);

    return 1;
}

IDEFN(pluses, do_pluses)

static int l_item_do_destroy(lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined())
    {
        lua_pushboolean(ls, false);
        return 0;
    }

    item_was_destroyed(*item);
    destroy_item(item->index());

    lua_pushboolean(ls, true);
    return 1;
}

IDEFN(destroy, do_destroy)

static int l_item_do_dec_quantity(lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined())
    {
        lua_pushboolean(ls, false);
        return 1;
    }

    // The quantity to reduce by.
    int quantity = luaL_safe_checkint(ls, 1);

    bool destroyed = false;

    if (in_inventory(*item))
        destroyed = dec_inv_item_quantity(item->link, quantity);
    else
        destroyed = dec_mitm_item_quantity(item->index(), quantity);

    lua_pushboolean(ls, destroyed);
    return 1;
}

IDEFN(dec_quantity, do_dec_quantity)

static int l_item_do_inc_quantity(lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined())
    {
        lua_pushboolean(ls, false);
        return 1;
    }

    // The quantity to increase by.
    int quantity = luaL_safe_checkint(ls, 1);

    if (in_inventory(*item))
        inc_inv_item_quantity(item->link, quantity);
    else
        inc_mitm_item_quantity(item->index(), quantity);

    return 0;
}

IDEFN(inc_quantity, do_inc_quantity)

// Some dLua convenience functions.
IDEF(base_type)
{
    ASSERT_DLUA;

    lua_pushstring(ls, base_type_string(*item));
    return 1;
}

IDEF(sub_type)
{
    ASSERT_DLUA;

    lua_pushstring(ls, sub_type_string(*item).c_str());
    return 1;
}

IDEF(ego_type)
{
    if (CLua::get_vm(ls).managed_vm && !item->is_identified()
        && item->base_type != OBJ_MISSILES)
    {
        lua_pushstring(ls, "unknown");
        return 1;
    }

    lua_pushstring(ls, ego_type_string(*item).c_str());
    return 1;
}

IDEF(ego_type_terse)
{
    if (CLua::get_vm(ls).managed_vm && !item->is_identified()
        && item->base_type != OBJ_MISSILES)
    {
        lua_pushstring(ls, "unknown");
        return 1;
    }

    lua_pushstring(ls, ego_type_string(*item, true).c_str());
    return 1;
}

IDEF(artefact_name)
{
    ASSERT_DLUA;

    if (is_artefact(*item))
        lua_pushstring(ls, get_artefact_name(*item, true).c_str());
    else
        lua_pushnil(ls);

    return 1;
}

IDEF(is_cursed)
{
    ASSERT_DLUA;

    bool cursed = item->cursed();

    lua_pushboolean(ls, cursed);
    return 1;
}

/***
 * @section end
 */

/*** Get the current inventory.
 * @treturn array An array of @{Item} objects for the current inventory
 * @function inventory
 */
static int l_item_inventory(lua_State *ls)
{
    _lua_push_inv_items(ls);
    return 1;
}

/*** Get the inventory letter of a slot number.
 * @tparam int idx
 * @treturn string
 * @function index_to_letter
 */
static int l_item_index_to_letter(lua_State *ls)
{
    int index = luaL_safe_checkint(ls, 1);
    char sletter[2] = "?";
    if (index >= 0 && index <= ENDOFPACK)
        *sletter = index_to_letter(index);
    lua_pushstring(ls, sletter);
    return 1;
}

/*** Get the index of an inventory letter.
 * @tparam string let
 * @treturn int
 * @function letter_to_index
 */
static int l_item_letter_to_index(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s || !*s || s[1])
        return 0;
    lua_pushnumber(ls, isaalpha(*s) ? letter_to_index(*s) : -1);
    return 1;
}

/*** Swap item slots.
 * Requires item indexes, use @{letter_to_index} if you want to swap letters.
 * @tparam int idx1
 * @tparam int idx2
 * @function swap_slots
 */
static int l_item_swap_slots(lua_State *ls)
{
    int slot1 = luaL_safe_checkint(ls, 1),
        slot2 = luaL_safe_checkint(ls, 2);
    bool verbose = lua_toboolean(ls, 3);
    if (slot1 < 0 || slot1 >= ENDOFPACK
        || slot2 < 0 || slot2 >= ENDOFPACK
        || slot1 == slot2 || !you.inv[slot1].defined())
    {
        return 0;
    }

    swap_inv_slots(slot1, slot2, verbose);

    return 0;
}

static item_def *dmx_get_item(lua_State *ls, int ndx, int subndx)
{
    if (lua_istable(ls, ndx))
    {
        lua_rawgeti(ls, ndx, subndx);
        ITEM(item, -1);
        lua_pop(ls, 1);
        return item;
    }
    ITEM(item, ndx);
    return item;
}

static int dmx_get_qty(lua_State *ls, int ndx, int subndx)
{
    int qty = -1;
    if (lua_istable(ls, ndx))
    {
        lua_rawgeti(ls, ndx, subndx);
        if (lua_isnumber(ls, -1))
            qty = luaL_safe_checkint(ls, -1);
        lua_pop(ls, 1);
    }
    else if (lua_isnumber(ls, ndx))
        qty = luaL_safe_checkint(ls, ndx);
    return qty;
}

static bool l_item_pickup2(item_def *item, int qty)
{
    if (!item || in_inventory(*item))
        return false;

    int floor_link = item_on_floor(*item, you.pos());
    if (floor_link == NON_ITEM)
        return false;

    return pickup_single_item(floor_link, qty);
}

/*** Pick up items.
 * Accepts either a single Item object, and optionally a quantity (for picking
 * up part of a stack), defaulting to the whole stack, or a table of (Item,qty)
 * pairs.
 *
 * In the first usage returns how many of the single Item were picked up. In
 * the second usage returns how many keys were successfully picked up.
 * @tparam Item|table what
 * @tparam[opt] int qty how many; defaults to the whole stack
 * @treturn int
 * @function pickup
 */
static int l_item_pickup(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;

    if (lua_isuserdata(ls, 1))
    {
        ITEM(item, 1);
        int qty = item->quantity;
        if (lua_isnumber(ls, 2))
            qty = luaL_safe_checkint(ls, 2);

        if (l_item_pickup2(item, qty))
            lua_pushnumber(ls, 1);
        else
            lua_pushnil(ls);
        return 1;
    }
    else if (lua_istable(ls, 1))
    {
        int dropped = 0;
        for (int i = 1; ; ++i)
        {
            lua_rawgeti(ls, 1, i);
            item_def *item = dmx_get_item(ls, -1, 1);
            int qty = dmx_get_qty(ls, -1, 2);
            lua_pop(ls, 1);

            if (l_item_pickup2(item, qty))
                dropped++;
            else
            {
                // Yes, we bail out on first failure.
                break;
            }
        }
        if (dropped)
            lua_pushnumber(ls, dropped);
        else
            lua_pushnil(ls);
        return 1;
    }
    return 0;
}

/*** Get the Item in a given equipment slot.
 * Takes either a slot name or a slot number.
 * @tparam string|int Slot type name or index
 * @tparam int[opt] Index of item in that slot type (ie: ring #2). Default 1.
 * @treturn Item|nil returns nil for nothing equipped or invalid slot
 * @function equipped_at
 */
static int l_item_equipped_at(lua_State *ls)
{
    int slot = -1;
    if (lua_isnumber(ls, 1))
        slot = luaL_safe_checkint(ls, 1);
    else if (lua_isstring(ls, 1))
    {
        const char *slotname = lua_tostring(ls, 1);
        if (!slotname)
            return 0;
        slot = equip_slot_by_name(slotname);
    }

    int index = 0;
    if (lua_isnumber(ls, 2))
        index = luaL_safe_checkint(ls, 2) - 1;

    if (slot < SLOT_WEAPON || slot >= NUM_EQUIP_SLOTS || index < 0)
        return 0;

    vector<item_def*> eq = you.equipment.get_slot_items(static_cast<equipment_slot>(slot), true);
    if ((int)eq.size() <= index)
        lua_pushnil(ls);
    else
        clua_push_item(ls, eq[index]);

    return 1;
}

/*** Get the ammo Item we should fire by default.
 * @treturn Item|nil returns nil if something other than ammo is quivered
 * @function fired_item
 */
static int l_item_fired_item(lua_State *ls)
{
    const auto a = quiver::get_secondary_action();
    if (!a->is_valid() || !a->is_enabled())
        return 0;

    const int q = a->get_item();

    if (q < 0 || q >= ENDOFPACK)
        return 0;

    clua_push_item(ls, &you.inv[q]);

    return 1;
}

/*** What item is in this slot?
 * Uses numeric slot index, use @{letter_to_index} to look up the index from
 * the number.
 * @tparam int idx slot index
 * @treturn Item|nil the item in that slot, nil if none
 * @function inslot
 */
static int l_item_inslot(lua_State *ls)
{
    int index = luaL_safe_checkint(ls, 1);
    if (index >= 0 && index < 52 && you.inv[index].defined())
        clua_push_item(ls, &you.inv[index]);
    else
        lua_pushnil(ls);
    return 1;
}

/*** List the seen items at a grid cell.
 * This function uses player centered coordinates.
 * @tparam int x
 * @tparam int y
 * @treturn array An array of @{Item} objects
 * @function get_items_at
 */
static int l_item_get_items_at(lua_State *ls)
{
    coord_def s;
    s.x = luaL_safe_checkint(ls, 1);
    s.y = luaL_safe_checkint(ls, 2);
    coord_def p = player2grid(s);

    if (!map_bounds(p))
        return 0;

    const auto item = env.map_knowledge(p).item();
    // XXX: Sensed items like those from Gnoll's strong nose mutation aren't
    // fully defined. These currently shouldn't be returned to clua, since many
    // methods like :name() don't work properly. Ideally they would have a
    // valid representation we could return.
    if (!item || !item->defined())
        return 0;

    lua_newtable(ls);

    const vector<item_def> items = item_list_in_stash(p);
    int index = 0;
    for (const auto &it : items)
    {
        _clua_push_item_temp(ls, it);
        lua_rawseti(ls, -2, ++index);
    }

    // The item is defined in our map knowledge, but not in our stash, hence
    // it's a sensed item like the Abyssal rune.
    if (!index)
    {
        _clua_push_item_temp(ls, *item);
        lua_rawseti(ls, -2, ++index);
    }

    return 1;
}

int lua_push_shop_items_at(lua_State *ls, const coord_def &s)
{
    // also used in l-dgnit.cc
    shop_struct *shop = shop_at(s);
    if (!shop)
        return 0;
    shopping_list.refresh(); // prevent crash if called during tests

    lua_newtable(ls);

    const vector<item_def> items = shop->stock;
    int index = 0;
    for (const auto &item : items)
    {
        lua_newtable(ls);
        _clua_push_item_temp(ls, item);
        lua_rawseti(ls, -2, 1);
        lua_pushnumber(ls, item_price(item, *shop));
        lua_rawseti(ls, -2, 2);
        lua_pushboolean(ls, shopping_list.is_on_list(item));
        lua_rawseti(ls, -2, 3);
        lua_rawseti(ls, -2, ++index);
    }

    return 1;
}

/*** See what a shop has for sale.
 * Only works when standing at a shop.
 * @treturn array|nil An array of @{Item} objects or nil if not on a shop
 * @function shop_inventory
 */
static int l_item_shop_inventory(lua_State *ls)
{
    return lua_push_shop_items_at(ls, you.pos());
}

/*** Look at the shopping list.
 * @treturn array|nil Array of @{Item}s on the shopping list or nil if the
 * shopping list is empty
 * @function shopping_list
 */
static int l_item_shopping_list(lua_State *ls)
{
    if (shopping_list.empty())
        return 0;

    lua_newtable(ls);

    const vector<shoplist_entry> items = shopping_list.entries();
    int index = 0;
    for (const auto &item : items)
    {
        lua_newtable(ls);
        lua_pushstring(ls, item.first.c_str());
        lua_rawseti(ls, -2, 1);
        lua_pushnumber(ls, item.second);
        lua_rawseti(ls, -2, 2);
        lua_rawseti(ls, -2, ++index);
    }

    return 1;
}

/*** See the items offered by acquirement.
 * Only works when the acquirement menu is active.
 * @tparam int An integer indicating the type of acquirement:
 *                 1: The normal acquirement menu.
 *                 2: Okawaru weapon gift acquirement.
 *                 3: Okawaru armour gift acquirement.
 *                 4: Coglin gizmo acquirement.
 * @treturn array|nil An array of @{Item} objects or nil if not acquiring.
 * @function acquirement_items
 */
static int l_item_acquirement_items(lua_State *ls)
{

    const int acquire_type = luaL_safe_checkint(ls, 1);
    string acquire_key;
    if (acquire_type <= 1)
        acquire_key = ACQUIRE_ITEMS_KEY;
    else if (acquire_type == 2)
        acquire_key = OKAWARU_WEAPONS_KEY;
    else if (acquire_type == 3)
        acquire_key = OKAWARU_ARMOUR_KEY;
    else if (acquire_type == 4)
        acquire_key = COGLIN_GIZMO_KEY;
    else
        return 0;

    if (!you.props.exists(acquire_key))
        return 0;

    auto &acq_items = you.props[acquire_key].get_vector();

    lua_newtable(ls);

    int index = 0;
    for (const item_def &item : acq_items)
    {
        _clua_push_item_temp(ls, item);
        lua_rawseti(ls, -2, ++index);
    }

    return 1;
}

/*** Fire an item in inventory at a target, either an evokable or a throwable
 * ammo. This will work for launcher ammo, but only if the launcher is wielded.
 * Some evokables (e.g. artefact weapons) may also need to be wielded.
 *
 * @tparam number the item's slot
 * @tparam[opt=0] number x coordinate
 * @tparam[opt=0] number y coordinate
 * @tparam[opt=false] boolean if true, aim at the target; if false, shoot past it
 * @tparam[opt=false] boolean whether to allow fumble throwing of non-activatable items
 * @treturn boolean whether an action took place
 * @function fire
 */
static int l_item_fire(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;
    const int slot = luaL_safe_checkint(ls, 1);

    if (slot < 0 || slot > ENDOFPACK || !you.inv[slot].defined())
    {
        luaL_argerror(ls, 1,
                        make_stringf("Invalid item slot: %d", slot).c_str());
        return 0;
    }
    PLAYERCOORDS(c, 2, 3);
    dist target;
    target.target = c;
    target.isEndpoint = lua_toboolean(ls, 4); // can be nil
    const bool force = lua_toboolean(ls, 5); // can be nil
    quiver::slot_to_action(slot, force)->trigger(target);
    PLUARET(boolean, you.turn_is_over);
}

LUAFN(l_item_excluded_from_set)
{
    ASSERT_DLUA;

    const string &specifier = luaL_checkstring(ls, 1);
    item_list il;
    item_spec parsed_spec;
    if (!il.parse_single_spec(parsed_spec, specifier))
    {
        luaL_error(ls, make_stringf("Invalid item spec '%s'.",
                                    specifier.c_str()).c_str());
        return 0;
    }
    lua_pushboolean(ls, item_excluded_from_set(parsed_spec.base_type,
                                               parsed_spec.sub_type));
    return 1;
}

LUAFN(l_item_for_set)
{
    ASSERT_DLUA;

    const string &setname = luaL_checkstring(ls, 1);
    const item_set_type iset = item_set_by_name(setname);
    if (iset == NUM_ITEM_SET_TYPES)
    {
        luaL_error(ls, make_stringf("Invalid item set name '%s'.",
                                    setname.c_str()).c_str());
        return 0;
    }

    lua_pushstring(ls, item_name_for_set(iset).c_str());
    return 1;
}

static bool _item_pickable(object_class_type base, int sub)
{
    return !item_excluded_from_set(base, sub);
}

LUAFN(l_item_pickable)
{
    ASSERT_DLUA;

    const string &specifier = luaL_checkstring(ls, 1);
    item_list il;
    item_spec parsed_spec;
    if (!il.parse_single_spec(parsed_spec, specifier))
    {
        luaL_error(ls, make_stringf("Invalid item spec '%s'.",
                                    specifier.c_str()).c_str());
        return 0;
    }
    lua_pushboolean(ls, _item_pickable(parsed_spec.base_type,
                                       parsed_spec.sub_type));
    return 1;
}


struct ItemAccessor
{
    const char *attribute;
    int (*accessor)(lua_State *ls, item_def *item);
};

static ItemAccessor item_attrs[] =
{
    { "artefact",          l_item_artefact },
    { "branded",           l_item_branded },
    { "god_gift",          l_item_god_gift },
    { "is_identified",     l_item_is_identified },
    { PLUS_KEY,            l_item_plus },
    { "is_enchantable",    l_item_is_enchantable },
    { "plus2",             l_item_plus2 },
    { "class",             l_item_class },
    { "subtype",           l_item_subtype },
    { "ego",               l_item_ego },
    { "cursed",            l_item_cursed },
    { "name",              l_item_name },
    { "name_coloured",     l_item_name_coloured },
    { "stacks",            l_item_stacks },
    { "quantity",          l_item_quantity },
    { "slot",              l_item_slot },
    { "ininventory",       l_item_ininventory },
    { "equip",             l_item_equip },
    { "remove",            l_item_remove },
    { "drop",              l_item_drop },
    { "equipped",          l_item_equipped },
    { "equip_type",        l_item_equip_type },
    { "weap_skill",        l_item_weap_skill },
    { "reach_range",       l_item_reach_range },
    { "is_ranged",         l_item_is_ranged },
    { "is_throwable",      l_item_is_throwable },
    { "is_xp_evoker",      l_item_is_xp_evoker },
    { "dropped",           l_item_dropped },
    { "is_weapon",         l_item_is_weapon },
    { "is_shield",         l_item_is_shield },
    { "is_melded",         l_item_is_melded },
    { "is_corpse",         l_item_is_corpse },
    { "is_useless",        l_item_is_useless },
    { "spells",            l_item_spells },
    { "artprops",          l_item_artprops },
    { "damage",            l_item_damage },
    { "damage_rating",     l_item_damage_rating },
    { "evoke_damage",      l_item_evoke_damage },
    { "accuracy",          l_item_accuracy },
    { "delay",             l_item_delay },
    { "ac",                l_item_ac },
    { "encumbrance",       l_item_encumbrance },
    { "is_in_shop",        l_item_is_in_shop },
    { "inscription",       l_item_inscription },
    { "description",       l_item_description },

    // dlua only past this point
    { "pluses",            l_item_pluses },
    { "destroy",           l_item_destroy },
    { "dec_quantity",      l_item_dec_quantity },
    { "inc_quantity",      l_item_inc_quantity },
    { "base_type",         l_item_base_type },
    { "sub_type",          l_item_sub_type },
    { "ego_type",          l_item_ego_type },
    { "ego_type_terse",    l_item_ego_type_terse },
    { "artefact_name",     l_item_artefact_name },
    { "is_cursed",         l_item_is_cursed },
    { "hands",             l_item_hands },
};

static int item_get(lua_State *ls)
{
    ITEM(iw, 1);
    if (!iw)
        return 0;

    const char *attr = luaL_checkstring(ls, 2);
    if (!attr)
        return 0;

    for (const ItemAccessor &ia : item_attrs)
        if (!strcmp(attr, ia.attribute))
            return ia.accessor(ls, iw);

    return 0;
}

static const struct luaL_reg item_lib[] =
{
    { "inventory",         l_item_inventory },
    { "letter_to_index",   l_item_letter_to_index },
    { "index_to_letter",   l_item_index_to_letter },
    { "swap_slots",        l_item_swap_slots },
    { "pickup",            l_item_pickup },
    { "equipped_at",       l_item_equipped_at },
    { "fired_item",        l_item_fired_item },
    { "inslot",            l_item_inslot },
    { "get_items_at",      l_item_get_items_at },
    { "shop_inventory",    l_item_shop_inventory },
    { "shopping_list",     l_item_shopping_list },
    { "acquirement_items", l_item_acquirement_items },
    { "fire",              l_item_fire },

    { "excluded_from_set", l_item_excluded_from_set },
    { "item_for_set",      l_item_for_set },
    { "pickable",          l_item_pickable },
    { nullptr, nullptr },
};

static int _delete_wrapped_item(lua_State *ls)
{
    item_wrapper *iw = static_cast<item_wrapper*>(lua_touserdata(ls, 1));
    if (iw && iw->temp && iw->item)
    {
        delete iw->item;
        iw->item = nullptr;
    }
    return 0;
}

void cluaopen_item(lua_State *ls)
{
    luaL_newmetatable(ls, ITEM_METATABLE);
    lua_pushstring(ls, "__index");
    lua_pushcfunction(ls, item_get);
    lua_settable(ls, -3);

    lua_pushstring(ls, "__gc");
    lua_pushcfunction(ls, _delete_wrapped_item);
    lua_settable(ls, -3);

    // Pop the metatable off the stack.
    lua_pop(ls, 1);

    luaL_openlib(ls, "items", item_lib, 0);
}
