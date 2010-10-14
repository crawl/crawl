/*
 * File:     l_items.cc
 * Summary:  functions for managed Lua item manipulation
 */

#include "AppHdr.h"

#include <sstream>

#include "cluautil.h"
#include "l_libs.h"

#include "artefact.h"
#include "colour.h"
#include "command.h"
#include "env.h"
#include "enum.h"
#include "invent.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "output.h"
#include "player.h"
#include "skills2.h"
#include "spl-summoning.h"
#include "stuff.h"

/////////////////////////////////////////////////////////////////////
// Item handling

struct item_wrapper
{
    item_def *item;
    long turn;

    bool valid() const { return turn == you.num_turns; }
};

void clua_push_item(lua_State *ls, item_def *item)
{

    item_wrapper *iw = clua_new_userdata<item_wrapper>(ls, ITEM_METATABLE);
    iw->item = item;
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

void lua_push_inv_items(lua_State *ls = NULL)
{
    if (!ls)
        ls = clua.state();
    lua_newtable(ls);
    int index = 0;
    for (unsigned slot = 0; slot < ENDOFPACK; ++slot)
    {
        if (you.inv[slot].defined())
        {
            clua_push_item(ls, &you.inv[slot]);
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
        return (1);                                                     \
    }

#define ITEM(name, ndx) \
    item_def *name = clua_get_item(ls, ndx)

#define UDATA_ITEM(name) ITEM(name, lua_upvalueindex(1))

static int l_item_do_wield(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

    UDATA_ITEM(item);

    int slot = -1;
    if (item && item->defined() && in_inventory(*item))
        slot = item->link;
    bool res = wield_weapon(true, slot);
    lua_pushboolean(ls, res);
    return (1);
}

IDEFN(wield, do_wield)

static int l_item_do_wear(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item))
        return (0);

    bool success = do_wear_armour(item->link, false);
    lua_pushboolean(ls, success);
    return (1);
}

IDEFN(wear, do_wear)

static int l_item_do_puton(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item))
        return (0);

    lua_pushboolean(ls, puton_ring(item->link));
    return (1);
}

IDEFN(puton, do_puton)

static int l_item_do_remove(lua_State *ls)
{
    if (you.turn_is_over)
    {
        mpr("Turn is over");
        return (0);
    }

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item))
    {
        mpr("Bad item");
        return (0);
    }

    int eq = get_equip_slot(item);
    if (eq < 0 || eq >= NUM_EQUIP)
    {
        mpr("Item is not equipped");
        return (0);
    }

    bool result = false;
    if (eq == EQ_WEAPON)
        result = wield_weapon(true, -1);
    else if (eq == EQ_LEFT_RING || eq == EQ_RIGHT_RING || eq == EQ_AMULET)
        result = remove_ring(item->link);
    else
        result = takeoff_armour(item->link);
    lua_pushboolean(ls, result);
    return (1);
}

IDEFN(remove, do_remove)

static int l_item_do_drop(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

    UDATA_ITEM(item);

    if (!item || !in_inventory(*item))
        return (0);

    int eq = get_equip_slot(item);
    if (eq >= 0 && eq < NUM_EQUIP)
    {
        lua_pushboolean(ls, false);
        lua_pushstring(ls, "Can't drop worn items");
        return (2);
    }

    int qty = item->quantity;
    if (lua_isnumber(ls, 1))
    {
        int q = luaL_checkint(ls, 1);
        if (q >= 1 && q <= item->quantity)
            qty = q;
    }
    lua_pushboolean(ls, drop_item(item->link, qty));
    return (1);
}

IDEFN(drop, do_drop)

IDEF(equipped)
{
    if (!item || !in_inventory(*item))
        lua_pushboolean(ls, false);

    int eq = get_equip_slot(item);
    if (eq < 0 || eq >= NUM_EQUIP)
        lua_pushboolean(ls, false);
    else
        lua_pushboolean(ls, true);

    return (1);
}

static int l_item_do_class (lua_State *ls)
{
    UDATA_ITEM(item);

    if (item)
    {
        bool terse = false;
        if (lua_isboolean(ls, 1))
            terse = lua_toboolean(ls, 1);

        std::string s = item_class_name(item->base_type, terse);
        lua_pushstring(ls, s.c_str());
    }
    else
        lua_pushnil(ls);
    return (1);
}

IDEFN(class, do_class)

// FIXME: Fold this back into itemname.cc.
static const char *ring_types[] =
{
    "regeneration",
    "protection",
    "protection from fire",
    "poison resistance",
    "protection from cold",
    "strength",
    "slaying",
    "see invisible",
    "invisibility",
    "hunger",
    "teleportation",
    "evasion",
    "sustain abilities",
    "sustenance",
    "dexterity",
    "intelligence",
    "wizardry",
    "magical power",
    "levitation",
    "life protection",
    "protection from magic",
    "fire",
    "ice",
    "teleport control"
};

static const char *amulet_types[] =
{
    "rage", "resist slowing", "clarity", "warding", "resist corrosion",
    "gourmand", "conservation", "controlled flight", "inaccuracy",
    "resist mutation"
};

IDEF(subtype)
{
    if (item)
    {
        if (item_type_known(*item))
        {
            const char *s = NULL;
            if (item->base_type == OBJ_JEWELLERY)
            {
                if (jewellery_is_amulet(*item))
                    s = amulet_types[ item->sub_type - AMU_RAGE ];
                else
                    s = ring_types[item->sub_type];
            }
            else if (item->base_type == OBJ_POTIONS)
            {
                if (item->sub_type == POT_BLOOD)
                   s = "blood";
                else if (item->sub_type == POT_BLOOD_COAGULATED)
                   s = "coagulated blood";
                else if (item->sub_type == POT_WATER)
                   s = "water";
                else if (item->sub_type == POT_PORRIDGE)
                   s = "porridge";
                else if (item->sub_type == POT_BERSERK_RAGE)
                   s = "berserk";
                else if (item->sub_type == POT_GAIN_STRENGTH
                         || item->sub_type == POT_GAIN_DEXTERITY
                         || item->sub_type == POT_GAIN_INTELLIGENCE)
                {
                   s = "gain ability";
                }
                else if (item->sub_type == POT_CURE_MUTATION)
                   s = "cure mutation";
            }
            else if (item->base_type == OBJ_BOOKS)
            {
                if (item->sub_type == BOOK_MANUAL)
                    s = "manual";
                else
                    s = "spellbook";
            }

            if (s)
                lua_pushstring(ls, s);
            else
                lua_pushnil(ls);

            lua_pushnumber(ls, item->sub_type);
            return (2);
        }
    }

    lua_pushnil(ls);
    lua_pushnil(ls);
    return (2);
}

IDEF(cursed)
{
    bool cursed = item && item_ident(*item, ISFLAG_KNOW_CURSE)
                       && item->cursed();
    lua_pushboolean(ls, cursed);
    return (1);
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
    return (2);
}

static std::string _item_name(lua_State *ls, item_def* item)
{
    description_level_type ndesc = DESC_PLAIN;
    if (lua_isstring(ls, 1))
        ndesc = description_type_by_name(lua_tostring(ls, 1));
    else if (lua_isnumber(ls, 1))
        ndesc = static_cast<description_level_type>(luaL_checkint(ls, 1));
    bool terse = lua_toboolean(ls, 2);
    return (item->name(ndesc, terse));
}

static int l_item_do_name(lua_State *ls)
{
    UDATA_ITEM(item);

    if (item)
    {
        std::string name = _item_name(ls, item);
        lua_pushstring(ls, name.c_str());
    }
    else
        lua_pushnil(ls);
    return (1);
}

IDEFN(name, do_name)

static int l_item_do_name_coloured(lua_State *ls)
{
    UDATA_ITEM(item);

    if (item)
    {
        std::string name   = _item_name(ls, item);
        int         col    = menu_colour(name, menu_colour_item_prefix(*item));
        std::string colstr = colour_to_str(col);

        std::ostringstream out;

        out << "<" << colstr << ">" << name << "</" << colstr << ">";

        lua_pushstring(ls, out.str().c_str());
    }
    else
        lua_pushnil(ls);
    return (1);
}

IDEFN(name_coloured, do_name_coloured)

IDEF(quantity)
{
    PLUARET(number, item? item->quantity : 0);
}

IDEF(slot)
{
    if (item)
    {
        int slot = in_inventory(*item) ? item->link
                                       : letter_to_index(item->slot);
        lua_pushnumber(ls, slot);
    }
    else
        lua_pushnil(ls);
    return (1);
}

IDEF(ininventory)
{
    PLUARET(boolean, item && in_inventory(*item));
}

IDEF(equip_type)
{
    if (!item || !item->defined())
        return (0);

    equipment_type eq = EQ_NONE;

    if (item->base_type == OBJ_WEAPONS || item->base_type == OBJ_STAVES)
        eq = EQ_WEAPON;
    else if (item->base_type == OBJ_ARMOUR)
        eq = get_armour_slot(*item);
    else if (item->base_type == OBJ_JEWELLERY)
        eq = item->sub_type >= AMU_RAGE? EQ_AMULET : EQ_RINGS;

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
    return (2);
}

IDEF(weap_skill)
{
    if (!item || !item->defined())
        return (0);

    int skill = range_skill(*item);
    if (skill == SK_THROWING)
        skill = weapon_skill(*item);
    if (skill == SK_FIGHTING)
        return (0);

    lua_pushstring(ls, skill_name(skill));
    lua_pushnumber(ls, skill);
    return (2);
}

IDEF(dropped)
{
    if (!item || !item->defined())
        return (0);

    lua_pushboolean(ls, item->flags & ISFLAG_DROPPED);

    return (1);
}

IDEF(can_cut_meat)
{
    if (!item || !item->defined())
        return (0);

    lua_pushboolean(ls, can_cut_meat(*item));

    return (1);
}

IDEF(artefact)
{
    if (!item || !item->defined())
        return (0);

    lua_pushboolean(ls, is_artefact(*item));

    return (1);
}

IDEF(branded)
{
    if (!item || !item->defined() || !item_type_known(*item))
        return (0);

    bool branded = false;
    switch (item->base_type)
    {
    case OBJ_WEAPONS:
        branded = get_weapon_brand(*item) != SPWPN_NORMAL;
        break;
    case OBJ_ARMOUR:
        branded = get_armour_ego_type(*item) != SPARM_NORMAL;
        break;
    case OBJ_MISSILES:
        branded = get_ammo_brand(*item) != SPMSL_NORMAL;
        break;
    default:
        break;
    }
    lua_pushboolean(ls, branded);
    return (1);
}

IDEF(snakable)
{
    if (!item || !item->defined())
        return (0);

    lua_pushboolean(ls, item_is_snakable(*item));

    return (1);
}

// DLUA-only functions
static int l_item_do_pluses (lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined() || !item_ident(*item, ISFLAG_KNOW_PLUSES))
    {
        lua_pushboolean(ls, false);
        return (1);
    }

    lua_pushnumber(ls, item->plus);
    // XXX: May cause issues on items that don't use plus2, ie ammunition.
    lua_pushnumber(ls, item->plus2);

    return (2);
}

IDEFN(pluses, do_pluses)

static int l_item_do_destroy (lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined())
    {
        lua_pushboolean(ls, false);
        return (0);
    }

    item_was_destroyed(*item);
    destroy_item(item->index());

    lua_pushboolean(ls, true);
    return (1);
}

IDEFN(destroy, do_destroy)

static int l_item_do_dec_quantity (lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined())
    {
        lua_pushboolean(ls, false);
        return (1);
    }

    // The quantity to reduce by.
    int quantity = luaL_checkint(ls, 1);

    bool destroyed = false;

    if (in_inventory(*item))
        destroyed = dec_inv_item_quantity(item->link, quantity);
    else
        destroyed = dec_mitm_item_quantity(item->index(), quantity);

    lua_pushboolean(ls, destroyed);
    return (1);
}

IDEFN(dec_quantity, do_dec_quantity)

static int l_item_do_inc_quantity (lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined())
    {
        lua_pushboolean(ls, false);
        return (1);
    }

    // The quantity to increase by.
    int quantity = luaL_checkint(ls, 1);

    if (in_inventory(*item))
        inc_inv_item_quantity(item->link, quantity);
    else
        inc_mitm_item_quantity(item->index(), quantity);

    return (0);
}

IDEFN(inc_quantity, do_inc_quantity)

unsigned long str_to_item_status_flags (std::string flag)
{
    unsigned long flags = 0;
    if (flag.find("curse") != std::string::npos)
        flags &= ISFLAG_KNOW_CURSE;
    // type is dealt with using item_type_known.
    //if (flag.find("type") != std::string::npos)
    //    flags &= ISFLAG_KNOW_TYPE;
    if (flag.find("pluses") != std::string::npos)
        flags &= ISFLAG_KNOW_PLUSES;
    if (flag.find("properties") != std::string::npos)
        flags &= ISFLAG_KNOW_PROPERTIES;
    if (flag == "any")
        flags = ISFLAG_IDENT_MASK;

    return (flags);
}

static int l_item_do_identified (lua_State *ls)
{
    ASSERT_DLUA;

    UDATA_ITEM(item);

    if (!item || !item->defined())
    {
        lua_pushnil(ls);
        return (1);
    }

    bool known_status = false;
    if (lua_isstring(ls, 1))
    {
        std::string flags = luaL_checkstring(ls, 1);
        if (trimmed_string(flags).empty())
            known_status = item_ident(*item, ISFLAG_IDENT_MASK);
        else
        {
            const bool check_type = strip_tag(flags, "type");
            const unsigned long item_flags = str_to_item_status_flags(flags);
            known_status = ((item_flags || check_type)
                            && (!item_flags || item_ident(*item, item_flags))
                            && (!check_type || item_type_known(*item)));
        }
    }
    else
    {
        known_status = item_ident(*item, ISFLAG_IDENT_MASK);
    }

    lua_pushboolean(ls, known_status);
    return (1);
}

IDEFN(identified, do_identified)

// Some dLua convenience functions.
IDEF(base_type)
{
    ASSERT_DLUA;

    lua_pushstring(ls, base_type_string(*item).c_str());
    return (1);
}

IDEF(sub_type)
{
    ASSERT_DLUA;

    lua_pushstring(ls, sub_type_string(*item).c_str());
    return (1);
}

IDEF(ego_type)
{
    if (CLua::get_vm(ls).managed_vm && !item_ident(*item, ISFLAG_KNOW_TYPE))
    {
        lua_pushstring(ls, "unknown");
        return (1);
    }

    lua_pushstring(ls, ego_type_string(*item).c_str());
    return (1);
}

IDEF(artefact_name)
{
    ASSERT_DLUA;

    if (is_artefact(*item))
        lua_pushstring(ls, get_artefact_name(*item, true).c_str());
    else
        lua_pushnil(ls);

    return (1);
}

IDEF(is_cursed)
{
    ASSERT_DLUA;

    bool cursed = item->cursed();

    lua_pushboolean(ls, cursed);
    return (1);
}

// Library functions below
static int l_item_inventory(lua_State *ls)
{
    lua_push_inv_items(ls);
    return (1);
}

static int l_item_index_to_letter(lua_State *ls)
{
    int index = luaL_checkint(ls, 1);
    char sletter[2] = "?";
    if (index >= 0 && index <= ENDOFPACK)
        *sletter = index_to_letter(index);
    lua_pushstring(ls, sletter);
    return (1);
}

static int l_item_letter_to_index(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s || !*s || s[1])
        return (0);
    lua_pushnumber(ls, letter_to_index(*s));
    return (1);
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
        return (0);
    }

    swap_inv_slots(slot1, slot2, verbose);

    return (0);
}

static item_def *dmx_get_item(lua_State *ls, int ndx, int subndx)
{
    if (lua_istable(ls, ndx))
    {
        lua_rawgeti(ls, ndx, subndx);
        ITEM(item, -1);
        lua_pop(ls, 1);
        return (item);
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
    {
        qty = luaL_checkint(ls, ndx);
    }
    return (qty);
}

static bool l_item_pickup2(item_def *item, int qty)
{
    if (!item || in_inventory(*item))
        return (false);

    int floor_link = item_on_floor(*item, you.pos());
    if (floor_link == NON_ITEM)
        return (false);

    return pickup_single_item(floor_link, qty);
}

static int l_item_pickup(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

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
        return (1);
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
        return (1);
    }
    return (0);
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
            return (0);
        eq = equip_name_to_slot(eqname);
    }

    if (eq < 0 || eq >= NUM_EQUIP)
        return (0);

    if (you.equip[eq] != -1)
        clua_push_item(ls, &you.inv[you.equip[eq]]);
    else
        lua_pushnil(ls);

    return (1);
}

static int l_item_inslot(lua_State *ls)
{
    int index = luaL_checkint(ls, 1);
    if (index >= 0 && index < 52 && you.inv[index].defined())
        clua_push_item(ls, &you.inv[index]);
    else
        lua_pushnil(ls);
    return (1);
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
    { "snakable",          l_item_snakable },
    { "class",             l_item_class },
    { "subtype",           l_item_subtype },
    { "cursed",            l_item_cursed },
    { "worn",              l_item_worn },
    { "name",              l_item_name },
    { "name_coloured",     l_item_name_coloured },
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
    { "dropped",           l_item_dropped },
    { "can_cut_meat",      l_item_can_cut_meat },
    { "pluses",            l_item_pluses },
    { "destroy",           l_item_destroy },
    { "dec_quantity",      l_item_dec_quantity },
    { "inc_quantity",      l_item_inc_quantity },
    { "identified",        l_item_identified },
    { "base_type",         l_item_base_type },
    { "sub_type",          l_item_sub_type },
    { "ego_type",          l_item_ego_type },
    { "artefact_name",     l_item_artefact_name },
    { "is_cursed",         l_item_is_cursed },
};

static int item_get(lua_State *ls)
{
    ITEM(iw, 1);
    if (!iw)
        return (0);

    const char *attr = luaL_checkstring(ls, 2);
    if (!attr)
        return (0);

    for (unsigned i = 0; i < sizeof(item_attrs) / sizeof(item_attrs[0]); ++i)
        if (!strcmp(attr, item_attrs[i].attribute))
            return (item_attrs[i].accessor(ls, iw, attr));

    return (0);
}

static const struct luaL_reg item_lib[] =
{
    { "inventory",         l_item_inventory },
    { "letter_to_index",   l_item_letter_to_index },
    { "index_to_letter",   l_item_index_to_letter },
    { "swap_slots",        l_item_swap_slots },
    { "pickup",            l_item_pickup },
    { "equipped_at",       l_item_equipped_at },
    { "inslot",            l_item_inslot },
    { NULL, NULL },
};

void cluaopen_item(lua_State *ls)
{
    luaL_newmetatable(ls, ITEM_METATABLE);
    lua_pushstring(ls, "__index");
    lua_pushcfunction(ls, item_get);
    lua_settable(ls, -3);

    // Pop the metatable off the stack.
    lua_pop(ls, 1);

    luaL_openlib(ls, "items", item_lib, 0);
}
