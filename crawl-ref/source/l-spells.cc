/*** Information about spells.
 * @module spells
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "coord.h"
#include "describe.h"
#include "env.h"
#include "god-conduct.h"
#include "los.h"
#include "losparam.h"
#include "map-knowledge.h"
#include "quiver.h"
#include "ray.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "terrain.h"

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
    PLUARET(integer, spell_difficulty(spell));
}

/*** The MP cost of the spell.
 * @tparam string name
 * @treturn int
 * @function mana_cost
 */
LUAFN(l_spells_mana_cost)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(integer, spell_mana(spell));
}

/*** The current range of the spell.
 * @tparam string name
 * @treturn int
 * @function range
 */
LUAFN(l_spells_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(integer, spell_range(spell, &you));
}

/*** The maximum range of the spell.
 * @tparam string name
 * @treturn int
 * @function max_range
 */
LUAFN(l_spells_max_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(integer, calc_spell_range(spell, spell_power_cap(spell), true));
}

/*** The minimum range of the spell.
 * @tparam string name
 * @treturn int
 * @function min_range
 */
LUAFN(l_spells_min_range)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(integer, calc_spell_range(spell, 0, true));
}

/*** What path would a beam aimed at (x,y) take?
 *
 * This uses player knowledge, assuming that unknown squares are empty. The
 * beam will exclude the source. If it stops at a monster, it will include that
 * monster position. If it stops at a wall, it will not include it (unless it
 * has a monster in it and the beam is non-digging).
 *
 * @tparam int x coordinate of the ray destination, in player coordinates
 * @tparam int y coordinate of the ray destination, in player coordinates
 * @tparam[opt=0] int x_src coordinate of ray source, in player coordinates
 * @tparam[opt=0] int y_src coordinate of ray source, in player coordinates
 * @tparam[opt=0] int type The type of beam to use: 0 for non-piercing, 1 for
 * actor piercing, or 2 for digging.
 * @tparam[opt=true] boolean at_target If true, have the beam path never go
 * past the target. Otherwise, if the beam isn't blocked, it can continue past
 * the target up to the max range of player LOS radius.
 * @treturn table a table of {x,y} of the path the ray will take, in player
 * coordinates. Empty if the source and destination are either equal or farther
 * apart than player LOS radius. Also empty if either source or destination is
 * outside of known map bounds or is unknown.
 * @function beam_path
 */
LUAFN(l_spells_beam_path)
{
    coord_def a;
    a.x = luaL_safe_checkint(ls, 1);
    a.y = luaL_safe_checkint(ls, 2);
    coord_def aim = player2grid(a);

    coord_def s;
    s.x = lua_isnumber(ls, 3) ? lua_tointeger(ls, 3) : 0;
    s.y = lua_isnumber(ls, 4) ? lua_tointeger(ls, 4) : 0;
    coord_def src = player2grid(s);

    int type = 0;
    if (lua_isnumber(ls, 5))
    {
        int t = lua_tointeger(ls, 5);
        if (t >= 0 && t <= 2)
            type = t;
    }
    bool piercing = false, digging = false;
    if (type == 1)
        piercing = true;
    else if (type == 2)
    {
        piercing = true;
        digging = true;
    }

    bool at_target = true;
    if (lua_isboolean(ls, 6))
        at_target = lua_toboolean(ls, 6);

    const int max_range = get_los_radius();
    lua_newtable(ls);
    if (src == aim
        || src.distance_from(aim) > max_range
        || !in_known_map_bounds(aim)
        || !in_known_map_bounds(src)
        || !env.map_knowledge(src).known()
        || !env.map_knowledge(aim).known())
    {
        return 1;
    }

    // mirrors bolt::choose_ray with player knowledge.
    ray_def ray;
    if (!find_ray(src, aim, ray, opc_map_solid_see)
        && !find_ray(src, aim, ray, opc_map_default))
    {
        fallback_ray(src, aim, ray);
    }
    ray.advance();

    int index = 0;
    while (in_known_map_bounds(ray.pos()) && index < max_range)
    {
        const coord_def p = ray.pos();
        const map_cell &cell = env.map_knowledge(p);
        const dungeon_feature_type feat =
            cell.known() ? cell.feat() : DNGN_UNSEEN;
        const bool blocked = feat != DNGN_UNSEEN
            && feat_is_solid(feat)
            && (!digging || !feat_is_diggable(feat));
        const bool known_mons = cell.monsterinfo() != nullptr;
        const bool known_actor = known_mons || p == you.pos();
        const bool wall_mons = blocked && known_mons && !digging;

        // Terminate before a solid square, unless there's a monster in it.
        if (blocked && !wall_mons)
            break;

        const coord_def pp = grid2player(p);
        lua_createtable(ls, 2, 0);
        lua_pushinteger(ls, pp.x);
        lua_rawseti(ls, -2, 1);
        lua_pushinteger(ls, pp.y);
        lua_rawseti(ls, -2, 2);
        lua_rawseti(ls, -2, ++index);

        // Add a monster (or player) square but go no further.
        if (wall_mons || !piercing && known_actor)
            break;

        if (at_target && p == aim)
            break;

        ray.advance();
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
    PLUARET(integer, failure_rate_to_int(raw_spell_fail(spell)));
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
    PLUARET(integer, fail_severity(spell));
}

/*** The current spellpower (as an integer percentage 0-100).
 * @tparam string name
 * @treturn int
 * @function power_perc
 */
LUAFN(l_spells_power_perc)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(integer, spell_power_percent(spell));
}

/*** The current spellpower (in bars).
 * @tparam string name
 * @treturn int
 * @function power
 */
LUAFN(l_spells_power)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(integer, power_to_barcount(calc_spell_power(spell)));
}

/*** The maximum spellpower (in bars).
 * @tparam string name
 * @treturn int
 * @function max_power
 */
LUAFN(l_spells_max_power)
{
    spell_type spell = spell_by_name(luaL_checkstring(ls, 1), false);
    PLUARET(integer, power_to_barcount(spell_power_cap(spell)));
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
    PLUARET(boolean, god_forbids_spell(spell, god));
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


static const struct luaL_Reg spells_clib[] =
{
    { "memorised"     , l_spells_memorised },
    { "letter"        , l_spells_letter },
    { "beam_path"     , l_spells_beam_path },
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
    lua_newtable(ls);
    luaL_setfuncs(ls, spells_clib, 0);
    lua_setglobal(ls, "spells");
}
