/*** Information about spells.
 * @module spells
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "religion.h"
#include "spl-util.h"
#include "spl-zap.h"
#include "describe.h"

/*** Is this spell memorised?
 * @tparam string spellname
 * @treturn boolean
 * @function memorised
 */
LUAFN(l_spells_memorised)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(boolean, you.has_spell(spell));
}

/*** What letter is this spell assigned to?
 * @tparam string name
 * @treturn string|nil the spell letter or nil if not memorised
 * @function letter
 */
LUAFN(l_spells_letter)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    if (!you.has_spell(spell))
    {
        lua_pushnil(ls);
        return 1;
    }
    char buf[2];
    buf[0] = get_spell_letter(spell);
    buf[1] = 0;
    PLUARET(string, buf);
}

/*** The level of the named spell.
 * @tparam string name
 * @treturn int
 * @function level
 */
LUAFN(l_spells_level)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_difficulty(spell));
}

/*** The MP cost of the spell.
 * @tparam string name
 * @treturn int
 * @function mana_cost
 */
LUAFN(l_spells_mana_cost)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_mana(spell));
}

/*** The current range of the spell.
 * @tparam string name
 * @treturn int
 * @function range
 */
LUAFN(l_spells_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_range(spell, calc_spell_power(spell)));
}

/*** The maximum range of the spell.
 * @tparam string name
 * @treturn int
 * @function max_range
 */
LUAFN(l_spells_max_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_range(spell, spell_power_cap(spell)));
}

/*** The minimum range of the spell.
 * @tparam string name
 * @treturn int
 * @function min_range
 */
LUAFN(l_spells_min_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_range(spell, 0));
}


/*** If this spell is aimed at (x,y), what path will it actually take?
 * @tparam string spell name
 * @tparam int x coordinate to aim at, in player coordinates
 * @tparam int y coordinate to aim at, in player coordinates
 * @tparam int x coordinate of spell source, in player coordinates (default=0)
 * @tparam int y coordinate of spell source, in player coordinates (default=0)
 * @tparam boolean[opt=false] if true, have the spell aim at the target; if
 *                            false, shoot past it.
 * @treturn table|nil a table of {x,y} of the path the spell will take, in
 *                    player coordinates.
 * Nil is returned if the spell does not follow a path (eg. smite-targeted
 * spells) or if the spell has zero range.
 * @function path
 */
LUAFN(l_spells_path)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    zap_type zap = spell_to_zap(spell);
    int power = calc_spell_power(spell);
    int range = spell_range(spell, power);
    // return nil for non-zap or zero-range spells
    if (range <= 0 || zap >= NUM_ZAPS)
    {
        lua_pushnil(ls);
        return 1;
    }
    coord_def a;
    a.x = luaL_safe_checkint(ls, 2);
    a.y = luaL_safe_checkint(ls, 3);
    coord_def aim = player2grid(a);

    coord_def s;
    s.x = lua_isnumber(ls, 4) ? lua_tonumber(ls, 4) : 0;
    s.y = lua_isnumber(ls, 5) ? lua_tonumber(ls, 5) : 0;
    coord_def src = player2grid(s);

    bolt beam;
    beam.set_agent(&you);
    beam.source = src;
    beam.attitude = ATT_FRIENDLY;
    zappy(zap, power, false, beam);
    beam.set_is_tracer(true);
    beam.range = range;
    beam.target = aim;
    beam.ex_size = 0;
    beam.aimed_at_spot = true;
    if (lua_isboolean(ls, 6))
        beam.aimed_at_spot = lua_toboolean(ls, 6);
    beam.path_taken.clear();
    beam.fire();

    lua_createtable(ls, beam.path_taken.size(), 0);
    int index = 0;
    for (auto g : beam.path_taken)
    {
        coord_def p = grid2player(g);
        lua_createtable(ls, 2, 0);
        lua_pushnumber(ls, p.x);
        lua_rawseti(ls, -2, 1);
        lua_pushnumber(ls, p.y);
        lua_rawseti(ls, -2, 2);
        lua_rawseti(ls, -2, ++index);
    }
    return 1;
}


/*** The failure rate of the spell as a number in [0,100].
 * @tparam string name
 * @treturn int
 * @function fail
 */
LUAFN(l_spells_fail)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, failure_rate_to_int(raw_spell_fail(spell)));
}

/*** The miscast severity of the spell as a number in [0,5].
 * 0: light grey, no chance of damaging miscast
 * 1: white, <= 10% max HP damage
 * 2: yellow, <= 30% max HP damage
 * 3: light red, <= 50% max HP damage
 * 4: red, <= 70% max HP damage
 * 5: magenta, > 70% max HP damage (potentially lethal)
 * @tparam string name
 * @treturn int
 * @function fail_severity
 */
LUAFN(l_spells_fail_severity)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, fail_severity(spell));
}

/*** The current spellpower (as an integer percentage 0-100).
 * @tparam string name
 * @treturn int
 * @function power_perc
 */
LUAFN(l_spells_power_perc)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, spell_power_percent(spell));
}

/*** The current spellpower (in bars).
 * @tparam string name
 * @treturn int
 * @function power
 */
LUAFN(l_spells_power)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, power_to_barcount(calc_spell_power(spell)));
}

/*** The maximum spellpower (in bars).
 * @tparam string name
 * @treturn int
 * @function max_power
 */
LUAFN(l_spells_max_power)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(number, power_to_barcount(spell_power_cap(spell)));
}

/*** Does this spell take a direction or target?
 * @tparam string name
 * @treturn boolean
 * @function dir_or_target
 */
LUAFN(l_spells_dir_or_target)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    const spell_flags flags = get_spell_flags(spell);
    PLUARET(boolean, bool(flags & spflag::dir_or_target));
}

/*** Is this spell targetable?
 * @tparam string name
 * @treturn boolean
 * @function target
 */
LUAFN(l_spells_target)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    const spell_flags flags = get_spell_flags(spell);
    PLUARET(boolean, bool(flags & spflag::target));
}

/*** Is this spell castable in a direction?
 * @tparam string name
 * @treturn boolean
 * @function dir
 */
LUAFN(l_spells_dir)
{
    PLUARET(boolean, false);
}

/*** Can this spell target objects?
 * @tparam string name
 * @treturn boolean
 * @function targ_obj
 */
LUAFN(l_spells_targ_obj)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    const spell_flags flags = get_spell_flags(spell);
    PLUARET(boolean, bool(flags & spflag::obj));
}

/*** Does our god like this spell?
 * @tparam string name
 * @treturn boolean
 * @function god_likes
 */
LUAFN(l_spells_god_likes)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    god_type god = you.religion;
    if (lua_gettop(ls) > 1)
    {
        const char *godname = luaL_checkstring(ls, 2);
        god = str_to_god(godname);
    }
    PLUARET(boolean, god_likes_spell(spell, god));
}

/*** Does our god hate this spell?
 * Casting this will result in pennance or excommunication.
 * @tparam string name
 * @treturn boolean
 * @function god_hates
 */
LUAFN(l_spells_god_hates)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    god_type god = you.religion;
    if (lua_gettop(ls) > 1)
    {
        const char *godname = luaL_checkstring(ls, 2);
        god = str_to_god(godname);
    }
    PLUARET(boolean, god_hates_spell(spell, god));
}

/*** Cast a spell at a target. If the target is not provided, enters interactive
 * targeting.
 *
 * @tparam string spell name
 * @tparam[opt=0] number x coordinate
 * @tparam[opt=0] number y coordinate
 * @tparam[opt=false] boolean if true, aim at the target; if false, shoot past it
 * @treturn boolean whether an action took place
 * @function cast
 */
static int l_spells_cast(lua_State *ls)
{
    if (you.turn_is_over)
        return 0;
    const string spell_name = luaL_checkstring(ls, 1);
    spell_type spell = spell_by_name(spell_name, false);
    if (!is_valid_spell(spell))
    {
        luaL_argerror(ls, 1, ("Invalid spell: " + spell_name).c_str());
        return 0;
    }
    PLAYERCOORDS(c, 2, 3);
    dist target;
    target.target = c;
    target.isEndpoint = lua_toboolean(ls, 4); // can be nil
    quiver::spell_to_action(spell)->trigger(target);
    PLUARET(boolean, you.turn_is_over);
}

/*** Describe a spell.
 * Provide the complete text description of a spell as displayed in the
 * game UI
 * @tparam string spell name
 * @treturn string description
 * @function describe
 */
static int l_spells_describe(lua_State *ls)
{
    const string spell_name = luaL_checkstring(ls, 1);
    spell_type spell = spell_by_name(spell_name, false);
    if (!is_valid_spell(spell))
    {
        luaL_argerror(ls, 1, ("Invalid spell: " + spell_name).c_str());
        return 0;
    }
    PLUARET(string, player_spell_desc(spell).c_str());
}


static const struct luaL_reg spells_clib[] =
{
    { "memorised"     , l_spells_memorised },
    { "letter"        , l_spells_letter },
    { "path"          , l_spells_path },
    { "level"         , l_spells_level },
    { "mana_cost"     , l_spells_mana_cost },
    { "range"         , l_spells_range },
    { "max_range"     , l_spells_max_range },
    { "min_range"     , l_spells_min_range },
    { "fail"          , l_spells_fail },
    { "fail_severity" , l_spells_fail_severity },
    { "power"         , l_spells_power },
    { "max_power"     , l_spells_max_power },
    { "power_perc"    , l_spells_power_perc },
    { "dir_or_target" , l_spells_dir_or_target },
    { "target"        , l_spells_target },
    { "dir"           , l_spells_dir },
    { "targ_obj"      , l_spells_targ_obj },
    { "god_likes"     , l_spells_god_likes },
    { "god_hates"     , l_spells_god_hates },
    { "cast"          , l_spells_cast },
    { "describe"      , l_spells_describe },
    { nullptr, nullptr }
};

void cluaopen_spells(lua_State *ls)
{
    luaL_openlib(ls, "spells", spells_clib, 0);
}
