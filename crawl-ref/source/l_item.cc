/**
 * @file
 * @brief functions for managed Lua item manipulation
**/

#include "AppHdr.h"

#include "l_libs.h"

#include <algorithm>
#include <sstream>

#include "adjust.h"
#include "artefact.h"
#include "cluautil.h"
#include "colour.h"
#include "coord.h"
#include "enum.h"
#include "env.h"
#include "food.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "l_defs.h"
#include "libutil.h"
#include "mon-util.h"
#include "output.h"
#include "player.h"
#include "prompt.h"
#include "shopping.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "stash.h"
#include "stringutil.h"
#include "throw.h"

/////////////////////////////////////////////////////////////////////
// Item handling

struct item_wrapper
{
    item_def *item;
    bool temp; // Does item need to be freed when the wrapper is GCed?
    int turn;

    bool valid() const { return turn == you.num_turns; }
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
    if (CLua::get_vm(ls).managed_vm && !iwrap->valid())
        luaL_error(ls, "Invalid item");
    return iwrap->item;
}

void lua_push_floor_items(lua_State *ls, int link)
{
    lua_newtable(ls);
    int index = 0;
    for (; link != NON_ITEM; link = mitm[link].link)
    {
        clua_push_item(ls, &mitm[link]);
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
    static int l_item_##name(lua_State *ls, item_def *item,             \
                             const char *attr)                         \

#define IDEFN(name, closure)                    \
    static int l_item_##name(lua_State *ls, item_def *item, const char *attrs) \
    {                                                                   \
        clua_push_item(ls, item);                                            \
        lua_pushcclosure(ls, l_item_##closure, 1);                      \
        return 1;                                                     \
    }

#define ITEM(name, ndx) \
    item_def *name = clua_get_item(ls, ndx)

#define UDATA_ITEM(name) ITEM(name, lua_upvalueindex(1))

static int l_item_do_wield(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;

    UDATA_ITEM(item);

    int slot = -1;
    if (item && item->defined() && in_inventory(*item))
        slot = item->link;
    bool res = wield_weapon(true, slot);
    lua_pushboolean(ls, res);
    return 1;
}

IDEFN(wield, do_wield)

static int l_item_do_wear(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item))
        return 0;

    bool success = do_wear_armour(item->link, false);
    lua_pushboolean(ls, success);
    return 1;
}

IDEFN(wear, do_wear)

static int l_item_do_puton(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item))
        return 0;

    lua_pushboolean(ls, puton_ring(item->link));
    return 1;
}

IDEFN(puton, do_puton)

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

    int eq = get_equip_slot(item);
    if (eq < EQ_FIRST_EQUIP || eq >= NUM_EQUIP)
    {
        mpr("Item is not equipped");
        return 0;
    }

    bool result = false;
    if (eq == EQ_WEAPON)
        result = wield_weapon(true, SLOT_BARE_HANDS);
    else if (eq >= EQ_FIRST_JEWELLERY && eq <= EQ_LAST_JEWELLERY)
        result = remove_ring(item->link);
    else
        result = takeoff_armour(item->link);
    lua_pushboolean(ls, result);
    return 1;
}

IDEFN(remove, do_remove)

static int l_item_do_drop(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item))
        return 0;

    int eq = get_equip_slot(item);
    if (eq >= EQ_FIRST_EQUIP && eq < NUM_EQUIP)
    {
        lua_pushboolean(ls, false);
        lua_pushstring(ls, "Can't drop worn items");
        return 2;
    }

    int qty = item->quantity;
    if (lua_isnumber(ls, 1))
    {
        int q = luaL_checkint(ls, 1);
        if (q >= 1 && q <= item->quantity)
            qty = q;
    }
    lua_pushboolean(ls, drop_item(item->link, qty));
    return 1;
}

IDEFN(drop, do_drop)

IDEF(equipped)
{
    if (!item || !in_inventory(*item))
        lua_pushboolean(ls, false);

    int eq = get_equip_slot(item);
    if (eq < EQ_FIRST_EQUIP || eq >= NUM_EQUIP)
        lua_pushboolean(ls, false);
    else
        lua_pushboolean(ls, true);

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
        s = item_slot_name(get_armour_slot(*item));
    else if (item_type_known(*item))
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

IDEFN(subtype, do_subtype)

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

    if (item_type_known(*item) || item->base_type == OBJ_MISSILES)
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

IDEFN(ego, do_ego)

IDEF(cursed)
{
    bool cursed = item && item_ident(*item, ISFLAG_KNOW_CURSE)
                       && item->cursed();
    lua_pushboolean(ls, cursed);
    return 1;
}

IDEF(worn)
{
    int worn = get_equip_slot(item);
    if (worn != -1)
        lua_pushnumber(ls, worn);
    else
        lua_pushnil(ls);
    if (worn != -1)
        lua_pushstring(ls, equip_slot_to_name(worn));
    else
        lua_pushnil(ls);
    return 2;
}

static string _item_name(lua_State *ls, item_def* item)
{
    description_level_type ndesc = DESC_PLAIN;
    if (lua_isstring(ls, 1))
        ndesc = description_type_by_name(lua_tostring(ls, 1));
    else if (lua_isnumber(ls, 1))
        ndesc = static_cast<description_level_type>(luaL_checkint(ls, 1));
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

IDEFN(name, do_name)

static int l_item_do_name_coloured(lua_State *ls)
{
    UDATA_ITEM(item);

    if (item)
    {
        string name   = _item_name(ls, item);
        int    col    = menu_colour(name, item_prefix(*item));
        string colstr = colour_to_str(col);

        ostringstream out;

        out << "<" << colstr << ">" << name << "</" << colstr << ">";

        lua_pushstring(ls, out.str().c_str());
    }
    else
        lua_pushnil(ls);
    return 1;
}

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
                      [&] (const item_def &item) -> bool
                      {
                          return items_stack(*first, item);
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

IDEF(quantity)
{
    PLUARET(number, item? item->quantity : 0);
}

IDEF(slot)
{
    if (item && in_inventory(*item))
        lua_pushnumber(ls, item->link);
    else
        lua_pushnil(ls);
    return 1;
}

IDEF(ininventory)
{
    PLUARET(boolean, item && in_inventory(*item));
}

IDEF(equip_type)
{
    if (!item || !item->defined())
        return 0;

    equipment_type eq = EQ_NONE;

    if (is_weapon(*item))
        eq = EQ_WEAPON;
    else if (item->base_type == OBJ_ARMOUR)
        eq = get_armour_slot(*item);
    else if (item->base_type == OBJ_JEWELLERY)
        eq = item->sub_type >= AMU_RAGE ? EQ_AMULET : EQ_RINGS;

    if (eq != EQ_NONE)
    {
        lua_pushnumber(ls, eq);
        lua_pushstring(ls, equip_slot_to_name(eq));
    }
    else
    {
        lua_pushnil(ls);
        lua_pushnil(ls);
    }
    return 2;
}

IDEF(weap_skill)
{
    if (!item || !item->defined())
        return 0;

    const skill_type skill = item_attack_skill(*item);
    if (skill == SK_FIGHTING)
        return 0;

    lua_pushstring(ls, skill_name(skill));
    lua_pushnumber(ls, skill);
    return 2;
}

IDEF(reach_range)
{
    if (!item || !item->defined())
        return 0;

    reach_type rt = weapon_reach(*item);
    lua_pushnumber(ls, rt);
    return 1;
}

IDEF(is_ranged)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_range_weapon(*item));

    return 1;
}

IDEF(is_throwable)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_throwable(&you, *item));

    return 1;
}

IDEF(dropped)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item->flags & ISFLAG_DROPPED);

    return 1;
}

IDEF(is_melded)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item_is_melded(*item));

    return 1;
}

IDEF(is_corpse)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item->is_type(OBJ_CORPSES, CORPSE_BODY));

    return 1;
}

IDEF(is_skeleton)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item->is_type(OBJ_CORPSES, CORPSE_SKELETON));

    return 1;
}

IDEF(has_skeleton)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item->is_type(OBJ_CORPSES, CORPSE_BODY)
                         && mons_skeleton(item->mon_type)
                        || item->is_type(OBJ_CORPSES, CORPSE_SKELETON));

    return 1;
}

IDEF(can_zombify)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item->is_type(OBJ_CORPSES, CORPSE_BODY)
                        && mons_zombifiable(item->mon_type));

    return 1;
}

IDEF(is_preferred_food)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_preferred_food(*item));

    return 1;
}

IDEF(is_bad_food)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_bad_food(*item));

    return 1;
}

IDEF(is_useless)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_useless_item(*item));

    return 1;
}

IDEF(artefact)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_artefact(*item));

    return 1;
}

IDEF(branded)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, item_is_branded(*item)
                        || item->flags & ISFLAG_COSMETIC_MASK
                           && !item_type_known(*item));
    return 1;
}

IDEF(hands)
{
    if (!item || !item->defined())
        return 0;

    int hands = you.hands_reqd(*item) == HANDS_TWO ? 2 : 1;
    lua_pushnumber(ls, hands);

    return 1;
}

IDEF(god_gift)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, origin_as_god_gift(*item) != GOD_NO_GOD);

    return 1;
}

IDEF(fully_identified)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, fully_identified(*item));

    return 1;
}

IDEF(plus)
{
    if (!item || !item->defined())
        return 0;

    if (item_ident(*item, ISFLAG_KNOW_PLUSES)
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

IDEF(plus2)
{
    if (!item || !item->defined())
        return 0;

    lua_pushnil(ls);

    return 1;
}

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

IDEF(artprops)
{
    if (!item || !item->defined() || !is_artefact(*item)
        || !item_ident(*item, ISFLAG_KNOW_PROPERTIES))
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

IDEF(is_in_shop)
{
    if (!item || !item->defined())
        return 0;

    lua_pushboolean(ls, is_shop_item(*item));

    return 1;
}

IDEF(inscription)
{
    if (!item || !item->defined())
        return 0;

    lua_pushstring(ls, item->inscription.c_str());

    return 1;
}

// DLUA-only functions
static int l_item_do_pluses(lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined() || !item_ident(*item, ISFLAG_KNOW_PLUSES))
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
    int quantity = luaL_checkint(ls, 1);

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
    int quantity = luaL_checkint(ls, 1);

    if (in_inventory(*item))
        inc_inv_item_quantity(item->link, quantity);
    else
        inc_mitm_item_quantity(item->index(), quantity);

    return 0;
}

IDEFN(inc_quantity, do_inc_quantity)

static iflags_t _str_to_item_status_flags(string flag)
{
    iflags_t flags = 0;
    if (flag.find("curse") != string::npos)
        flags &= ISFLAG_KNOW_CURSE;
    // type is dealt with using item_type_known.
    //if (flag.find("type") != string::npos)
    //    flags &= ISFLAG_KNOW_TYPE;
    if (flag.find("pluses") != string::npos)
        flags &= ISFLAG_KNOW_PLUSES;
    if (flag.find("properties") != string::npos)
        flags &= ISFLAG_KNOW_PROPERTIES;
    if (flag == "any")
        flags = ISFLAG_IDENT_MASK;

    return flags;
}

static int l_item_do_identified(lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined())
    {
        lua_pushnil(ls);
        return 1;
    }

    bool known_status = false;
    if (lua_isstring(ls, 1))
    {
        string flags = luaL_checkstring(ls, 1);
        if (trimmed_string(flags).empty())
            known_status = item_ident(*item, ISFLAG_IDENT_MASK);
        else
        {
            const bool check_type = strip_tag(flags, "type");
            iflags_t item_flags = _str_to_item_status_flags(flags);
            known_status = ((item_flags || check_type)
                            && (!item_flags || item_ident(*item, item_flags))
                            && (!check_type || item_type_known(*item)));
        }
    }
    else
        known_status = item_ident(*item, ISFLAG_IDENT_MASK);

    lua_pushboolean(ls, known_status);
    return 1;
}

IDEFN(identified, do_identified)

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
    if (CLua::get_vm(ls).managed_vm && !item_type_known(*item)
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
    if (CLua::get_vm(ls).managed_vm && !item_type_known(*item)
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

// Library functions below
static int l_item_inventory(lua_State *ls)
{
    _lua_push_inv_items(ls);
    return 1;
}

static int l_item_index_to_letter(lua_State *ls)
{
    int index = luaL_checkint(ls, 1);
    char sletter[2] = "?";
    if (index >= 0 && index <= ENDOFPACK)
        *sletter = index_to_letter(index);
    lua_pushstring(ls, sletter);
    return 1;
}

static int l_item_letter_to_index(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s || !*s || s[1])
        return 0;
    lua_pushnumber(ls, isaalpha(*s) ? letter_to_index(*s) : -1);
    return 1;
}

static int l_item_swap_slots(lua_State *ls)
{
    int slot1 = luaL_checkint(ls, 1),
        slot2 = luaL_checkint(ls, 2);
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
            qty = luaL_checkint(ls, -1);
        lua_pop(ls, 1);
    }
    else if (lua_isnumber(ls, ndx))
        qty = luaL_checkint(ls, ndx);
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

static int l_item_pickup(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;

    if (lua_isuserdata(ls, 1))
    {
        ITEM(item, 1);
        int qty = item->quantity;
        if (lua_isnumber(ls, 2))
            qty = luaL_checkint(ls, 2);

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

// Returns item equipped in a slot defined in an argument.
static int l_item_equipped_at(lua_State *ls)
{
    int eq = -1;
    if (lua_isnumber(ls, 1))
        eq = luaL_checkint(ls, 1);
    else if (lua_isstring(ls, 1))
    {
        const char *eqname = lua_tostring(ls, 1);
        if (!eqname)
            return 0;
        eq = equip_name_to_slot(eqname);
    }

    if (eq < EQ_FIRST_EQUIP || eq >= NUM_EQUIP)
        return 0;

    if (you.equip[eq] != -1)
        clua_push_item(ls, &you.inv[you.equip[eq]]);
    else
        lua_pushnil(ls);

    return 1;
}

static int l_item_fired_item(lua_State *ls)
{
    int q = you.m_quiver.get_fire_item();

    if (q < 0 || q >= ENDOFPACK)
        return 0;

    if (q != -1 && !fire_warn_if_impossible(true))
        clua_push_item(ls, &you.inv[q]);
    else
        lua_pushnil(ls);

    return 1;
}

static int l_item_inslot(lua_State *ls)
{
    int index = luaL_checkint(ls, 1);
    if (index >= 0 && index < 52 && you.inv[index].defined())
        clua_push_item(ls, &you.inv[index]);
    else
        lua_pushnil(ls);
    return 1;
}

static int l_item_get_items_at(lua_State *ls)
{
    coord_def s;
    s.x = luaL_checkint(ls, 1);
    s.y = luaL_checkint(ls, 2);
    coord_def p = player2grid(s);
    if (!map_bounds(p))
        return 0;

    item_def* top = env.map_knowledge(p).item();
    if (!top || !top->defined())
        return 0;

    lua_newtable(ls);

    const vector<item_def> items = item_list_in_stash(p);
    int index = 0;
    for (const auto &item : items)
    {
        _clua_push_item_temp(ls, item);
        lua_rawseti(ls, -2, ++index);
    }

    return 1;
}

static int l_item_shop_inventory(lua_State *ls)
{
    shop_struct *shop = shop_at(you.pos());
    if (!shop)
        return 0;

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

struct ItemAccessor
{
    const char *attribute;
    int (*accessor)(lua_State *ls, item_def *item, const char *attr);
};

static ItemAccessor item_attrs[] =
{
    { "artefact",          l_item_artefact },
    { "branded",           l_item_branded },
    { "god_gift",          l_item_god_gift },
    { "fully_identified",  l_item_fully_identified },
    { "plus",              l_item_plus },
    { "plus2",             l_item_plus2 },
    { "class",             l_item_class },
    { "subtype",           l_item_subtype },
    { "ego",               l_item_ego },
    { "cursed",            l_item_cursed },
    { "worn",              l_item_worn },
    { "name",              l_item_name },
    { "name_coloured",     l_item_name_coloured },
    { "stacks",            l_item_stacks },
    { "quantity",          l_item_quantity },
    { "slot",              l_item_slot },
    { "ininventory",       l_item_ininventory },
    { "wield",             l_item_wield },
    { "wear",              l_item_wear },
    { "puton",             l_item_puton },
    { "remove",            l_item_remove },
    { "drop",              l_item_drop },
    { "equipped",          l_item_equipped },
    { "equip_type",        l_item_equip_type },
    { "weap_skill",        l_item_weap_skill },
    { "reach_range",       l_item_reach_range },
    { "is_ranged",         l_item_is_ranged },
    { "is_throwable",      l_item_is_throwable },
    { "dropped",           l_item_dropped },
    { "is_melded",         l_item_is_melded },
    { "is_skeleton",       l_item_is_skeleton },
    { "is_corpse",         l_item_is_corpse },
    { "has_skeleton",      l_item_has_skeleton },
    { "can_zombify",       l_item_can_zombify },
    { "is_preferred_food", l_item_is_preferred_food },
    { "is_bad_food",       l_item_is_bad_food },
    { "is_useless",        l_item_is_useless },
    { "spells",            l_item_spells },
    { "artprops",          l_item_artprops },
    { "damage",            l_item_damage },
    { "accuracy",          l_item_accuracy },
    { "delay",             l_item_delay },
    { "ac",                l_item_ac },
    { "encumbrance",       l_item_encumbrance },
    { "is_in_shop",        l_item_is_in_shop },
    { "inscription",       l_item_inscription },

    // dlua only past this point
    { "pluses",            l_item_pluses },
    { "destroy",           l_item_destroy },
    { "dec_quantity",      l_item_dec_quantity },
    { "inc_quantity",      l_item_inc_quantity },
    { "identified",        l_item_identified },
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
            return ia.accessor(ls, iw, attr);

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
    { nullptr, nullptr },
};

static int _delete_wrapped_item(lua_State *ls)
{
    item_wrapper *iw = static_cast<item_wrapper*>(lua_touserdata(ls, 1));
    if (iw && iw->temp)
        delete iw->item;
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
