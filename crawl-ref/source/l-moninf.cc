/*** Monster information.
 * @module monster
 */
#include "AppHdr.h"

#include "l-libs.h"

#include <algorithm>

#include "cluautil.h"
#include "coord.h"
#include "env.h"
#include "fight.h"
#include "l-defs.h"
#include "libutil.h" // map_find
#include "mon-book.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "ranged-attack.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stringutil.h"
#include "tag-version.h"
#include "throw.h"
#include "transform.h"
#include "math.h" // ceil
#include "spl-zap.h" // calc_spell_power
#include "evoke.h" // wand_mp_cost
#include "describe.h" // describe_info, get_monster_db_desc
#include "directn.h"

#define MONINF_METATABLE "monster.info"

void lua_push_moninf(lua_State *ls, monster_info *mi)
{
    monster_info **miref =
        clua_new_userdata<monster_info *>(ls, MONINF_METATABLE);
    *miref = new monster_info(*mi);
}

#define MONINF(ls, n, var) \
    monster_info *var = *(monster_info **) \
        luaL_checkudata(ls, n, MONINF_METATABLE); \
    if (!var) \
        return 0

#define MIRET1(type, field, cfield) \
    static int moninf_get_##field(lua_State *ls) \
    { \
        MONINF(ls, 1, mi); \
        lua_push##type(ls, mi->cfield); \
        return 1; \
    }

#define MIREG(field) { #field, moninf_get_##field }

/*** Monster information class.
 * @type monster.info
 */
/*** How hurt is this monster?
 * Numeric representation of the level of damage sustained by the monster.
 * Return value ranges from 0 (full HP) to 6 (dead).
 * @treturn boolean
 * @function damage_level
 */
MIRET1(number, damage_level, dam)
/*** Is this monster safe by default?
 * Check if this monster is thought of as safe by crawl internally. Does not
 * check @{Hooks.ch_mon_is_safe}, so this can be used there without causing an
 * infinite loop.
 * @treturn boolean
 * @function is_safe
 */
MIRET1(boolean, is_safe, is(MB_SAFE))
/*** Is this monster firewood.
 * Plants and fungi. Immobile things that give no exp.
 * @treturn boolean
 * @function is_firewood
 */
MIRET1(boolean, is_firewood, is(MB_FIREWOOD))
/*** The monster's current attitude.
 * A numerical value representing the monster's attitude. Possible values:
 *
 * - 0 hostile
 * - 1 neutral
 * - 2 strict neutral (neutral but won't attack the player)
 * - 3 good neutral (neutral but won't attack friendlies)
 * - 4 friendly (created friendly, not charmed)
 *
 * @treturn int
 * @function attitude
 */
MIRET1(number, attitude, attitude)
/*** The monster's threat level.
 * A numeric representation of the the threat level in the monster list.
 *
 * - 0 dark grey threat (trivial)
 * - 1 light grey threat (easy)
 * - 2 yellow threat (dangerous)
 * - 3 red threat (extremely dangerous)
 *
 * @treturn int
 * @function threat
 */
MIRET1(number, threat, threat)
/*** Simple monster name.
 * Returns the name of the monster.
 * @treturn string
 * @see name
 * @function mname
 */
MIRET1(string, mname, mname.c_str())
/*** Monster type enum value as in monster_type.h.
 * @treturn int
 * @function type
 */
MIRET1(number, type, type)
/*** Monster base type as in monster_type.h.
 * @treturn int
 * @function base_type
 */
MIRET1(number, base_type, base_type)
/*** Monster number field.
 * Contains hydra heads or slime size. Meaningless for all others.
 * @treturn int
 * @function number
 */
MIRET1(number, number, number)
/*** Does this monster have a ranged attack we know about?
 * A monster is considered to have a ranged attack if it has any of the
 * following: a reach attack, a throwable missile, a launcher weapon, an
 * attack wand, or an attack spell with a range greater than 1.
 * @treturn boolean
 * @function has_ranged_attack
 */
MIRET1(boolean, has_known_ranged_attack, is(MB_RANGED_ATTACK))
/*** A string describing monster speed.
 * Possible values are: "very slow", "slow", "normal", "fast", "very fast", and
 * "extremely fast".
 * @treturn string
 * @function speed_description
 */
MIRET1(string, speed_description, speed_description().c_str())
/*** The monster's x coordinate in player centered coordinates.
 * @treturn int
 * @function x_pos
 */
MIRET1(number, x_pos, pos.x - you.pos().x)
/*** The monster's y coordinate in player centered coordinates.
 * @treturn int
 * @function y_pos
 */
MIRET1(number, y_pos, pos.y - you.pos().y)

/*** Monster glyph colour.
 * Return is a crawl colour number.
 * @treturn int
 * @function colour
 */
static int moninf_get_colour(lua_State *ls)
{
    MONINF(ls, 1, mi);
    lua_pushnumber(ls, mi->colour());
    return 1;
}

/*** The x,y coordinates of the monster in player centered coordinates.
 * @treturn int
 * @treturn int
 * @function pos
 */
static int moninf_get_pos(lua_State *ls)
{
    MONINF(ls, 1, mi);
    lua_pushnumber(ls, mi->pos.x - you.pos().x);
    lua_pushnumber(ls, mi->pos.y - you.pos().y);
    return 2;
}

#define MIRES1(field, resist) \
    static int moninf_get_##field(lua_State *ls) \
    { \
        MONINF(ls, 1, mi); \
        lua_pushnumber(ls, get_resist(mi->resists(), resist)); \
        return 1; \
    }

// Named for consistency with the player resists.
/*** Does the monster resist poison?
 * Returns a value ranging from -1 (vulnerable) to 3 (immune).
 * @treturn int resistance level
 * @function res_poison
 */
MIRES1(res_poison, MR_RES_POISON)
/*** Does the monster resist fire?
 * Returns a value ranging from -1 (vulnerable) to 3 (immune).
 * @treturn int resistance level
 * @function res_fire
 */
MIRES1(res_fire, MR_RES_FIRE)
/*** Does the monster resist cold?
 * Returns a value ranging from -1 (vulnerable) to 3 (immune).
 * @treturn int resistance level
 * @function res_cold
 */
MIRES1(res_cold, MR_RES_COLD)
/*** Does the monster resist negative energy?
 * Returns a value ranging from -1 (vulnerable) to 3 (immune).
 * @treturn int resistance level
 * @function res_draining
 */
MIRES1(res_draining, MR_RES_NEG)
/*** Does the monster resist electricity?
 * Returns a value ranging from -1 (vulnerable) to 3 (immune).
 * @treturn int resistance level
 * @function res_shock
 */
MIRES1(res_shock, MR_RES_ELEC)
/*** Does the monster resist corrosion?
 * Returns a value ranging from -1 (vulnerable) to 3 (immune).
 * @treturn int resistance level
 * @function res_corr
 */
MIRES1(res_corr, MR_RES_ACID)
/*** Can the monster currently be frenzied?
 * Is it possible to affect the monster with the discord spell or a datura
 * dart?
 * @treturn boolean
 * @function can_go_frenzy
 */
MIRET1(boolean, can_go_frenzy, can_go_frenzy)

/*** The monster's max HP given in its description.
 * @treturn string describing the max HP (usually "about X").
 * @function max_hp
 */
static int moninf_get_max_hp(lua_State *ls)
{
    MONINF(ls, 1, mi);
    lua_pushstring(ls, mi->get_max_hp_desc().c_str());
    return 1;
}

/*** The monster's WL level, in "pips" (number of +'s shown on its description).
 * Returns a value ranging from 0 to 125 (immune).
 * @treturn int WL level
 * @function wl
 */
static int moninf_get_wl(lua_State *ls)
{
    MONINF(ls, 1, mi);
    lua_pushnumber(ls, ceil(1.0*mi->willpower()/WL_PIP));
    return 1;
}

/*** Your probability of defeating the monster's WL with a given spell or zap.
 * Returns a value ranging from 0 (no chance) to 100 (guaranteed success).
 *    Returns nil if WL does not apply or the spell can't be cast.
 * @tparam string spell name
 * @tparam[opt] boolean true if this spell is evoked rather than cast;
 *    defaults to false
 * @treturn int|string|nil percent chance of success (0-100);
 *     returns "infinite will" if monster is immune;
 *     returns nil if WL does not apply.
 * @function defeat_wl
 */
static int moninf_get_defeat_wl(lua_State *ls)
{
    MONINF(ls, 1, mi);
    spell_type spell = spell_by_name(luaL_checkstring(ls, 2), false);
    bool is_evoked = lua_isboolean(ls, 3) ? lua_toboolean(ls, 3) : false;
    int power = is_evoked ?
        (15 + you.skill(SK_EVOCATIONS, 7) / 2) * (wand_mp_cost() + 9) / 9 :
        calc_spell_power(spell);
    spell_flags flags = get_spell_flags(spell);
    bool wl_check = testbits(flags, spflag::WL_check)
        && testbits(flags, spflag::dir_or_target)
        && !testbits(flags, spflag::helpful);
    if (power <= 0 || !wl_check)
    {
        lua_pushnil(ls);
        return 1;
    }
    int wl = mi->willpower();
    if (wl == WILL_INVULN)
    {
        lua_pushstring(ls, "infinite will");
        return 1;
    }
    zap_type zap = spell_to_zap(spell);
    int eff_power = zap == NUM_ZAPS ? power : zap_ench_power(zap, power, false);
    int success = hex_success_chance(wl, eff_power, 100);
    lua_pushnumber(ls, success);
    return 1;
}

/*** The monster's AC value, in "pips" (number of +'s shown on its description).
 * Returns a value ranging from 0 to 5 (highest).
 * @treturn int AC level
 * @function ac
 */
static int moninf_get_ac(lua_State *ls)
{
    MONINF(ls, 1, mi);
    lua_pushnumber(ls, ceil(mi->ac/5.0));
    return 1;
}
/*** The monster's EV value, in "pips" (number of +'s shown on its description).
 * Returns a value ranging from 0 to 5 (highest).
 * @treturn int evasion level
 * @function ev
 */
static int moninf_get_ev(lua_State *ls)
{
    MONINF(ls, 1, mi);
    int value = mi->ev;
    if (!value && mi->base_ev != INT_MAX)
        value = mi->base_ev;
    lua_pushnumber(ls, ceil(value/5.0));
    return 1;
}

/*** The string displayed if you target this monster.
 * @treturn string targeting description of the monster
 *   (such as "Sigmund, wielding a +0 scythe and wearing a +0 robe")
 * @function target_desc
 */
static int moninf_get_target_desc(lua_State *ls)
{
    MONINF(ls, 1, mi);
    coord_def mp(mi->pos.x, mi->pos.y);
    dist moves;
    moves.target = mp;
    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.just_looking = true;
    args.needs_path = false;
    lua_pushstring(ls, direction_chooser(moves, args).target_description().c_str());
    return 1;
}

/*** Returns the string displayed in xv for your current weapon hit chance.
 * @treturn string (such as "about 82% to hit with your dagger")
 * @function target_weapon
 */
static int moninf_get_target_weapon(lua_State *ls)
{
    MONINF(ls, 1, mi);
    ostringstream result;
    describe_to_hit(*mi, result, you.weapon(), true);
    lua_pushstring(ls, result.str().c_str());
    return 1;
}

/*** Returns the string displayed if you target this monster with a spell.
 * @tparam string spell name
 * @treturn string (such as "74% to hit")
 * @function target_spell
 */
static int moninf_get_target_spell(lua_State *ls)
{
    MONINF(ls, 1, mi);
    spell_type spell = spell_by_name(luaL_checkstring(ls, 2), false);
    string desc = target_spell_desc(*mi, spell);
    lua_pushstring(ls, desc.c_str());
    return 1;
}

/*** Returns the string displayed if you target this monster with a thrown item.
 * @tparam item object to be thrown
 * @treturn string (such as "about 45% to hit")
 * @function target_throw
 */
static int moninf_get_target_throw(lua_State *ls)
{
    MONINF(ls, 1, mi);
    item_def *item = *(item_def **) luaL_checkudata(ls, 2, ITEM_METATABLE);
    ostringstream result;
    describe_to_hit(*mi, result, item);
    lua_pushstring(ls, result.str().c_str());
    return 1;
}

/*** Returns the string displayed if you target this monster with an evocable.
 * @tparam item object to be evoked
 * @treturn string (such as "about 45% to hit")
 * @function target_evoke
 */
static int moninf_get_target_evoke(lua_State *ls)
{
    MONINF(ls, 1, mi);
    item_def *item = *(item_def **) luaL_checkudata(ls, 2, ITEM_METATABLE);
    if (!item)
    {
        lua_pushnil(ls);
        return 1;
    }

    string desc = target_evoke_desc(*mi, *item);
    lua_pushstring(ls, desc.c_str());
    return 1;
}

/*** Get the monster's holiness.
 * If passed a holiness, returns a boolean test of whether the monster has the
 * given holiness. Otherwise returns a string describing the monster's
 * holiness.
 *
 * Possible holinesses: "holy", "natural", "undead", "demonic",
 * "nonliving", "plant", "evil". Evil is a pseudo-holiness given to natural,
 * nonliving, or plant monsters that are hated by the good gods for the spells
 * they cast or gods they worship.
 * @tparam[opt] string holiness
 * @treturn string|boolean
 * @function holiness
 */
LUAFN(moninf_get_holiness)
{
    MONINF(ls, 1, mi);

    if (lua_gettop(ls) >= 2)
    {
        string holi = luaL_checkstring(ls, 2);
        lowercase(holi);
        mon_holy_type arg = holiness_by_name(holi);
        if (arg == MH_NONE)
        {
            luaL_argerror(ls, 2, (string("no such holiness: '")
                                  + holi + "'").c_str());
            return 0;
        }
        else
            PLUARET(boolean, bool(mi->holi & arg));
    }
    else
        PLUARET(string, holiness_description(mi->holi).c_str());
}

/*** Get the monster's intelligence.
 * Returns a string describing the intelligence level of the monster. Possible
 * descriptions: "Mindless", "Animal", or "Human"
 * @treturn string
 * @function intelligence
 */
LUAFN(moninf_get_intelligence)
{
    MONINF(ls, 1, mi);
    PLUARET(string, intelligence_description(mi->intel()));
}

/*** Get the monster's average depth of (random) generation in the current branch
 * Returns -1 if the monster is not generated in this branch. Does not handle
 * fish or zombies.
 * @treturn number
 * @function avg_local_depth
 */
LUAFN(moninf_get_avg_local_depth)
{
    MONINF(ls, 1, mi);
    PLUARET(number, monster_pop_depth_avg(you.where_are_you, mi->type));
}

/*** Get the monster's probability of randomly generating on the current floor
 * This can be used to estimate difficulty, but keep in mind that it is highly
 * dependent on the branch's generation table.
 * Returns -1 if the monster is not generated in this branch. Does not handle
 * fish or zombies.
 * @treturn number
 * @function avg_local_prob
 */
LUAFN(moninf_get_avg_local_prob)
{
    MONINF(ls, 1, mi);
    PLUARET(number, monster_probability(level_id::current(), mi->type));
}


// const char* here would save a tiny bit of memory, but every map
// for an unique pair of types costs 35KB of code. We have
// map<string, int> elsewhere.
static map<string, int> mi_flags;
static void _init_mi_flags()
{
    int f = 0;
#define MI_FLAG(x) mi_flags[x] = f++;
#include "mi-enum.h"
#undef MI_FLAG
}

/*** Test a monster flag.
 * Check if a monster has a flag set. See `mi-flag.h` for a list of flags.
 * @tparam string flagname
 * @treturn boolean
 * @function is
 */
LUAFN(moninf_get_is)
{
    MONINF(ls, 1, mi);
    int num = -1;
    if (lua_isnumber(ls, 2)) // legacy scripts
        num = lua_tonumber(ls, 2);
    else
    {
        if (mi_flags.empty())
            _init_mi_flags();
        string flag = luaL_checkstring(ls, 2);
        if (int *flagnum = map_find(mi_flags, lowercase(flag)))
            num = *flagnum;
        else
        {
            luaL_argerror(ls, 2, (string("no such moninf flag: '")
                                  + flag + "'").c_str());
            return 0;
        }
    }
    if (num < 0 || num >= NUM_MB_FLAGS)
    {
        luaL_argerror(ls, 2, "mb:is() out of bounds");
        return 0;
    }
    lua_pushboolean(ls, mi->is(num));
    return 1;
}

/*** Get the monster's flags.
 * Returns all flags set for the moster, as a list of flag names.
 * @treturn array
 * @function flags
 */
LUAFN(moninf_get_flags)
{
    if (mi_flags.empty())
        _init_mi_flags();
    MONINF(ls, 1, mi);
    lua_newtable(ls);
    int index = 0;
    for (std::map<string,int>::iterator it = mi_flags.begin(); it != mi_flags.end(); ++it)
        if (mi->is(it->second))
        {
            lua_pushstring(ls, it->first.c_str());
            lua_rawseti(ls, -2, ++index);
        }
    return 1;
}

/*** Get the monster's spells.
 * Returns the monster's spellbook. The spellbook is given
 * as a list of spell names.
 * @treturn array
 * @function spells
 */
LUAFN(moninf_get_spells)
{
    MONINF(ls, 1, mi);

    if (!mi->has_spells())
    {
        lua_newtable(ls);
        return 1;
    }

    const vector<mon_spell_slot> &unique_slots = get_unique_spells(*mi);
    vector<string> spell_titles;

    bool abjuration = false;
    for (const auto& slot : unique_slots)
    {
        spell_titles.emplace_back(spell_title(slot.spell));

        // XXX: Probably get_unique_spells() could just do this for us.
        if (get_spell_flags(slot.spell) & spflag::mons_abjure)
            abjuration = true;
    }

    if (abjuration)
        spell_titles.emplace_back(spell_title(SPELL_ABJURATION));

    clua_stringtable(ls, spell_titles);
    return 1;
}

/*** What quality of stab can you get on this monster?
 * The return value is a number representing the percentage of a top-tier stab
 * you can currently get by attacking the monster. Possible values are:
 *
 * - 1.0 Sleep, petrified, and paralysis stabs.
 * - 0.25 Net, web, petrifying, confusion, fear, invisibility, and distraction
 *   stabs
 * - 0.0 No stab bonus.
 *
 * @treturn number
 * @function stabbability
 */
LUAFN(moninf_get_stabbability)
{
    MONINF(ls, 1, mi);
    if (mi->is(MB_STABBABLE))
        lua_pushnumber(ls, 1.0);
    else if (mi->is(MB_MAYBE_STABBABLE))
        lua_pushnumber(ls, 0.25);
    else
        lua_pushnumber(ls, 0);

    return 1;
}

/*** Is the monster caught in something?
 * Tests for nets or webs.
 * @treturn boolean
 * @function is_caught
 */
LUAFN(moninf_get_is_caught)
{
    MONINF(ls, 1, mi);
    lua_pushboolean(ls, mi->is(MB_CAUGHT) || mi->is(MB_WEBBED));
    return 1;
}

/*** Is the monster constricted?
 * @treturn boolean
 * @function is_constricted
 */
LUAFN(moninf_get_is_constricted)
{
    MONINF(ls, 1, mi);
    lua_pushboolean(ls, (mi->constrictor_name.find("constricted by") == 0)
                     || (mi->constrictor_name.find("held by") == 0));
    return 1;
}

/*** Is the monster constricting something?
 * @treturn boolean
 * @function is_constricting
 */
LUAFN(moninf_get_is_constricting)
{
    MONINF(ls, 1, mi);
    lua_pushboolean(ls, !mi->constricting_name.empty());
    return 1;
}

/*** Is the monster constricting you in particular?
 * @treturn boolean
 * @function is_constricting_you
 */
LUAFN(moninf_get_is_constricting_you)
{
    MONINF(ls, 1, mi);
    if (!you.is_constricted())
    {
        lua_pushboolean(ls, false);
        return 1;
    }

    // yay the interface
    lua_pushboolean(ls, (find(mi->constricting_name.begin(),
                              mi->constricting_name.end(), "constricting you")
                         != mi->constricting_name.end())
                     || (find(mi->constricting_name.begin(),
                              mi->constricting_name.end(), "holding you")
                         != mi->constricting_name.end()));
    return 1;
}

/*** Can this monster be constricted?
 * @treturn boolean
 * @function can_be_constricted
 */
LUAFN(moninf_get_can_be_constricted)
{
    MONINF(ls, 1, mi);
    if (!mi->constrictor_name.empty()
        || !form_keeps_mutations()
        || (you.get_mutation_level(MUT_CONSTRICTING_TAIL) < 2
                || you.is_constricting())
            && (you.has_mutation(MUT_TENTACLE_ARMS)
                || !you.has_usable_tentacle()))
    {
        lua_pushboolean(ls, false);
    }
    else
    {
        monster dummy;
        dummy.type = mi->type;
        dummy.base_monster = mi->base_type;
        lua_pushboolean(ls, !dummy.res_constrict());
    }
    return 1;
}

/*** Can this monster traverse a particular cell?
 * Does not address doors or traps; returns true if the monster can occupy the cell.
 * Uses player coordinates
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function can_traverse
 */
LUAFN(moninf_get_can_traverse)
{
    MONINF(ls, 1, mi);
    PLAYERCOORDS(p, 2, 3)
    lua_pushboolean(ls,
        map_bounds(p)
        && monster_habitable_feat(mi->type, env.map_knowledge(p).feat()));
    return 1;
}

/*** How far can the monster reach with their melee weapon?
 * @treturn int
 * @function reach_range
 */
LUAFN(moninf_get_reach_range)
{
    MONINF(ls, 1, mi);

    lua_pushnumber(ls, mi->reach_range());
    return 1;
}

/*** Is this monster a unique?
 * @treturn boolean
 * @function is_unique
 */
LUAFN(moninf_get_is_unique)
{
    MONINF(ls, 1, mi);
    lua_pushboolean(ls, mons_is_unique(mi->type));
    return 1;
}

/*** Can this monster move?
 * @treturn boolean
 * @function is_stationary
 */
LUAFN(moninf_get_is_stationary)
{
    MONINF(ls, 1, mi);
    lua_pushboolean(ls, mons_class_is_stationary(mi->type));
    return 1;
}

/*** Can this monster use doors?
 * @treturn boolean
 * @function can_use_doors
 */
LUAFN(moninf_get_can_use_doors)
{
    MONINF(ls, 1, mi);
    lua_pushboolean(ls, mons_class_itemuse(mi->type) >= MONUSE_OPEN_DOORS);
    return 1;
}

/*** Get a string describing how injured this monster is.
 * @treturn string
 * @function damage_desc
 */
LUAFN(moninf_get_damage_desc)
{
    MONINF(ls, 1, mi);
    string s = mi->damage_desc();
    lua_pushstring(ls, s.c_str());
    return 1;
}

/*** A description of this monster.
 * @tparam[opt] boolean set true to get the description information body
 *     displayed when examining the monster; if false (default) returns
 *     a short description.
 * @treturn string
 * @function desc
 */
LUAFN(moninf_get_desc)
{
    MONINF(ls, 1, mi);
    if (lua_isboolean(ls, 2) && lua_toboolean(ls, 2))
    {
        // full description
        describe_info inf;
        bool has_stat_desc;
        get_monster_db_desc(*mi, inf, has_stat_desc);
        lua_pushstring(ls, inf.body.str().c_str());
    }
    else
    {
        // short description
        string desc;
        int col;
        mi->to_string(1, desc, col);
        lua_pushstring(ls, desc.c_str());
    }
    return 1;
}

/*** What statuses is this monster under?
 * If passed a string parameter, returns a boolean indicating if the monster
 * has that status. Otherwise returns a comma separated string with all the
 * statuses the monster has.
 * @tparam[opt] string statusname
 * @treturn string|boolean
 * @function status
 */
LUAFN(moninf_get_status)
{
    MONINF(ls, 1, mi);
    const char* which = nullptr;
    if (lua_gettop(ls) >= 2)
        which = luaL_checkstring(ls, 2);

    vector<string> status = mi->attributes();
    if (!which)
    {
        PLUARET(string, comma_separated_line(status.begin(),
                                             status.end(), ", ").c_str());
    }
    for (const auto &st : status)
        if (st == which)
            PLUARET(boolean, true);

    PLUARET(boolean, false);
}

/*** The monster's full name.
 * Includes any vault defined names, the uniques name, name changes induced by
 * polymorph, &c.
 * @treturn string
 * @see mname
 * @function name
 */
LUAFN(moninf_get_name)
{
    MONINF(ls, 1, mi);
    string s = mi->full_name();
    lua_pushstring(ls, s.c_str());
    return 1;
}

/*
 * The x,y coordinates of the monster that summoned this monster, in player
 * centered coordinates. If the monster was not summoned by another monster
 * that's currently in LOS, return nil.
 * @treturn int
 * @treturn int
 * @function pos
 */
LUAFN(moninf_get_summoner_pos)
{
    MONINF(ls, 1, mi);

    const auto *summoner = mi->get_known_summoner();
    if (summoner)
    {
        lua_pushnumber(ls, summoner->pos().x - you.pos().x);
        lua_pushnumber(ls, summoner->pos().y - you.pos().y);
        return 2;
    }
    else
    {
        lua_pushnil(ls);
        return 1;
    }
}

static const struct luaL_reg moninf_lib[] =
{
    MIREG(type),
    MIREG(base_type),
    MIREG(number),
    MIREG(colour),
    MIREG(mname),
    MIREG(is),
    MIREG(flags),
    MIREG(is_safe),
    MIREG(is_firewood),
    MIREG(stabbability),
    MIREG(holiness),
    MIREG(intelligence),
    MIREG(attitude),
    MIREG(threat),
    MIREG(is_caught),
    MIREG(is_constricted),
    MIREG(is_constricting),
    MIREG(is_constricting_you),
    MIREG(can_be_constricted),
    MIREG(can_traverse),
    MIREG(reach_range),
    MIREG(is_unique),
    MIREG(is_stationary),
    MIREG(can_use_doors),
    MIREG(damage_level),
    MIREG(damage_desc),
    MIREG(desc),
    MIREG(status),
    MIREG(name),
    MIREG(has_known_ranged_attack),
    MIREG(speed_description),
    MIREG(spells),
    MIREG(res_poison),
    MIREG(res_fire),
    MIREG(res_cold),
    MIREG(res_draining),
    MIREG(res_shock),
    MIREG(res_corr),
    MIREG(can_go_frenzy),
    MIREG(max_hp),
    MIREG(wl),
    MIREG(defeat_wl),
    MIREG(ac),
    MIREG(ev),
    MIREG(target_desc),
    MIREG(target_weapon),
    MIREG(target_spell),
    MIREG(target_throw),
    MIREG(target_evoke),
    MIREG(x_pos),
    MIREG(y_pos),
    MIREG(pos),
    MIREG(summoner_pos),
    MIREG(avg_local_depth),
    MIREG(avg_local_prob),

    { nullptr, nullptr }
};
/*** @section end
 */

// XXX: unify with directn.cc/h
// This uses relative coordinates with origin the player.
bool in_show_bounds(const coord_def &s)
{
    return s.rdist() <= ENV_SHOW_OFFSET;
}

/*** Get information about a monster at a cell.
 * Returns a monster.info object of a monster at the specified coordinates.
 * Uses player coordinates
 * @tparam int x
 * @tparam int y
 * @treturn monster.info|nil
 * @function get_monster_at
 */
LUAFN(mi_get_monster_at)
{
    COORDSHOW(s, 1, 2)
    coord_def p = player2grid(s);
    if (!you.see_cell(p))
        return 0;
    if (env.mgrid(p) == NON_MONSTER)
        return 0;
    monster* m = &env.mons[env.mgrid(p)];
    if (!m->visible_to(&you))
        return 0;
    monster_info mi(m);
    lua_push_moninf(ls, &mi);
    return 1;
}

static const struct luaL_reg mon_lib[] =
{
    { "get_monster_at", mi_get_monster_at },

    { nullptr, nullptr }
};

void cluaopen_moninf(lua_State *ls)
{
    clua_register_metatable(ls, MONINF_METATABLE, moninf_lib,
                            lua_object_gc<monster_info>);
    luaL_openlib(ls, "monster", mon_lib, 0);
}
