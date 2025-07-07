/**
 * @file
 * @brief Player kill tracking
**/

#include "AppHdr.h"

#include "kills.h"

#include <algorithm>

#include "clua.h"
#include "describe.h"
#include "english.h"
#include "files.h"
#include "ghost.h"
#include "libutil.h"
#include "l-libs.h"
#include "mon-death.h"
#include "mon-info.h"
#include "monster.h"
#include "options.h"
#include "stringutil.h"
#include "tags.h"
#include "tag-version.h"
#include "travel.h"
#include "unwind.h"
#include "viewchar.h"

#define KILLS_MAJOR_VERSION 4
#define KILLS_MINOR_VERSION 1

static void kill_lua_filltable(vector<kill_exp> &v);

///////////////////////////////////////////////////////////////////////////
// KillMaster
//

static const char *kill_category_names[] =
{
    "you",
    "collateral kills",
    "others",
};

const char *KillMaster::category_name(kill_category kc) const
{
    if (kc >= KC_YOU && kc < KC_NCATEGORIES)
        return kill_category_names[kc];
    return nullptr;
}

bool KillMaster::empty() const
{
    for (int i = 0; i < KC_NCATEGORIES; ++i)
        if (!categorized_kills[i].empty())
            return false;
    return true;
}

void KillMaster::save(writer& outf) const
{
    const auto version = save_version(KILLS_MAJOR_VERSION, KILLS_MINOR_VERSION);
    write_save_version(outf, version);

    for (int i = 0; i < KC_NCATEGORIES; ++i)
        categorized_kills[i].save(outf);
}

void KillMaster::load(reader& inf)
{
    const auto version = get_save_version(inf);
    const auto major = version.major, minor = version.minor;

    if (major != KILLS_MAJOR_VERSION
        || (minor != KILLS_MINOR_VERSION && minor > 0))
    {
        return;
    }

    for (int i = 0; i < KC_NCATEGORIES; ++i)
    {
        categorized_kills[i].load(inf);
        if (!minor)
            break;
    }
}

void KillMaster::record_kill(const monster* mon, int killer, bool ispet)
{
    const kill_category kc =
        YOU_KILL(killer) ? KC_YOU :
        ispet            ? KC_FRIENDLY :
                           KC_OTHER;
    categorized_kills[kc].record_kill(mon);
}

int KillMaster::total_kills() const
{
    int grandtotal = 0;
    for (int i = KC_YOU; i < KC_NCATEGORIES; ++i)
    {
        if (categorized_kills[i].empty())
            continue;

        vector<kill_exp> kills;
        int count = categorized_kills[i].get_kills(kills);
        grandtotal += count;
    }
    return grandtotal;
}

string KillMaster::kill_info() const
{
    if (empty())
        return "";

    string killtext;

    bool needseparator = false;
    int categories = 0;
    int grandtotal = 0;

    Kills catkills[KC_NCATEGORIES];
    for (int i = 0; i < KC_NCATEGORIES; ++i)
    {
        int targ = Options.kill_map[i];
        catkills[targ].merge(categorized_kills[i]);
    }

    for (int i = KC_YOU; i < KC_NCATEGORIES; ++i)
    {
        if (catkills[i].empty())
            continue;

        vector<kill_exp> kills;
        int count = catkills[i].get_kills(kills);
        if (count == 0) // filtered out firewood
            continue;

        categories++;
        grandtotal += count;

        add_kill_info(killtext,
                       kills,
                       count,
                       i == KC_YOU ? nullptr
                                   : category_name((kill_category) i),
                       needseparator);
        needseparator = true;
    }

    string grandt;
    if (categories > 1)
    {
        char buf[200];
        snprintf(buf, sizeof buf,
                "Grand Total: %d creatures vanquished",
                grandtotal);
        grandt = buf;
    }

    bool custom = false;
    unwind_var<int> lthrottle(clua.throttle_unit_lines, 500000);
    // Call the kill dump Lua function with null a, to tell it we're done.
    if (!clua.callfn("c_kill_list", "ss>b", nullptr, grandt.c_str(), &custom)
        || !custom)
    {
        // We can sum up ourselves, if Lua doesn't want to.
        if (categories > 1)
        {
            // Give ourselves a newline first
            killtext += "\n";
            killtext += grandt + "\n";
        }
    }

    return killtext;
}

void KillMaster::add_kill_info(string &killtext,
                               vector<kill_exp> &kills,
                               int count,
                               const char *category,
                               bool separator) const
{
    // Set a pointer to killtext as a Lua global
    lua_pushlightuserdata(clua.state(), &killtext);
    clua.setregistry("cr_skill");

    // Populate a Lua table with kill_exp structs, in the default order,
    // and leave the table on the top of the Lua stack.
    kill_lua_filltable(kills);

    if (category)
        lua_pushstring(clua, category);
    else
        lua_pushnil(clua);

    lua_pushboolean(clua, separator);

    unwind_var<int> lthrottle(clua.throttle_unit_lines, 500000);
    if (!clua.callfn("c_kill_list", 3, 1)
        || !lua_isboolean(clua, -1) || !lua_toboolean(clua, -1))
    {
        if (!clua.error.empty())
        {
            killtext += "Lua error:\n";
            killtext += clua.error + "\n\n";
        }
        if (separator)
            killtext += "\n";

        killtext += "Vanquished Creatures";
        if (category)
            killtext += string(" (") + category + ")";

        killtext += "\n";

        for (const kill_exp &kill : kills)
            killtext += "  " + kill.desc + "\n";

        killtext += make_stringf("%d creature%s vanquished.\n",
                                 count, count == 1 ? "" : "s");
    }
    lua_pop(clua, 1);
}

int KillMaster::num_kills(const monster* mon, kill_category cat) const
{
    return categorized_kills[cat].num_kills(mon);
}

int KillMaster::num_kills(const monster_info& mon, kill_category cat) const
{
    return categorized_kills[cat].num_kills(mon);
}

int KillMaster::num_kills(const monster* mon) const
{
    int total = 0;
    for (int cat = 0; cat < KC_NCATEGORIES; cat++)
        total += categorized_kills[cat].num_kills(mon);

    return total;
}

int KillMaster::num_kills(const monster_info& mon) const
{
    int total = 0;
    for (int cat = 0; cat < KC_NCATEGORIES; cat++)
        total += categorized_kills[cat].num_kills(mon);

    return total;
}

///////////////////////////////////////////////////////////////////////////

bool Kills::empty() const
{
    return kills.empty() && ghosts.empty();
}

void Kills::merge(const Kills &k)
{
    ghosts.insert(ghosts.end(), k.ghosts.begin(), k.ghosts.end());

    // Regular kills are messier to merge.
    for (const auto &entry : k.kills)
    {
        const kill_monster_desc &kmd = entry.first;
        kill_def &ki = kills[kmd];
        const kill_def &ko = entry.second;
        bool uniq = mons_is_unique(kmd.monnum);
        ki.merge(ko, uniq);
    }
}

void Kills::record_kill(const monster* mon)
{
    // Handle player ghosts separately, but don't handle summoned
    // ghosts at all. {due}
    if (mon->type == MONS_PLAYER_GHOST && !mon->is_summoned())
    {
        record_ghost_kill(mon);
        return ;
    }

    // Normal monsters
    // Create a descriptor
    kill_monster_desc descriptor = mon;

    kill_def &k = kills[descriptor];
    if (k.kills)
        k.add_kill(mon, level_id::current());
    else
        k = kill_def(mon);
}

int Kills::get_kills(vector<kill_exp> &all_kills) const
{
    int count = 0;
    for (const auto &entry : kills)
    {
        const kill_monster_desc &md = entry.first;
        if (mons_class_is_firewood(md.monnum))
            continue;
        const kill_def &k = entry.second;
        all_kills.emplace_back(k, md);
        count += k.kills;
    }

    for (const kill_ghost &gh : ghosts)
        all_kills.emplace_back(gh);
    count += ghosts.size();

    sort(all_kills.begin(), all_kills.end());
    return count;
}

void Kills::save(writer& outf) const
{
    // How many kill records do we have?
    marshallInt(outf, kills.size());

    for (const auto &entry : kills)
    {
        entry.first.save(outf);
        entry.second.save(outf);
    }

    // How many ghosts do we have?
    marshallShort(outf, ghosts.size());
    for (const auto &ghost : ghosts)
        ghost.save(outf);
}

void Kills::load(reader& inf)
{
    // How many kill records?
    int kill_count = unmarshallInt(inf);
    kills.clear();
    for (int i = 0; i < kill_count; ++i)
    {
        kill_monster_desc md;
        md.load(inf);
        kills[md].load(inf);
    }

    short ghost_count = unmarshallShort(inf);
    ghosts.clear();
    for (short i = 0; i < ghost_count; ++i)
    {
        kill_ghost kg;
        kg.load(inf);
        ghosts.push_back(kg);
    }
}

void Kills::record_ghost_kill(const monster* mon)
{
    // We should never get to this point, but just in case... {due}
    if (mon->is_summoned())
        return;
    ghosts.emplace_back(mon);
}

int Kills::num_kills(const monster* mon) const
{
    kill_monster_desc desc(mon);
    return num_kills(desc);
}

int Kills::num_kills(const monster_info& mon) const
{
    kill_monster_desc desc(mon);
    return num_kills(desc);
}

int Kills::num_kills(kill_monster_desc desc) const
{
    auto iter = kills.find(desc);
    int total = (iter == kills.end() ? 0 : iter->second.kills);

    if (desc.modifier == kill_monster_desc::M_SHAPESHIFTER)
    {
        desc.modifier = kill_monster_desc::M_NORMAL;
        iter = kills.find(desc);
        total += (iter == kills.end() ? 0 : iter->second.kills);
    }

    return total;
}

kill_def::kill_def(const monster* mon) : kills(0), exp(0)
{
    exp = exp_value(*mon);
    add_kill(mon, level_id::current());
}

// For a non-unique monster, prefixes a suitable article if we have only one
// kill, else prefixes a kill count and pluralises the monster name.
static string n_names(const string &name, int n)
{
    if (n > 1)
    {
        char buf[20];
        snprintf(buf, sizeof buf, "%d ", n);
        return buf + pluralise_monster(name);
    }
    else
        return article_a(name, false);
}

// Returns a string describing the number of times a unique has been killed.
// Currently required only for Boris.
//
static string kill_times(int kills)
{
    char buf[50];
    switch (kills)
    {
    case 1:
        strcpy(buf, " (once)");
        break;
    case 2:
        strcpy(buf, " (twice)");
        break;
    case 3:
        strcpy(buf, " (thrice)");
        break;
    default:
        snprintf(buf, sizeof buf, " (%d times)", kills);
        break;
    }
    return string(buf);
}

void kill_def::merge(const kill_def &k, bool uniq)
{
    if (!kills)
        *this = k;
    else
    {
        kills += k.kills;
        for (level_id lvl : k.places)
            add_place(lvl, uniq);
    }
}

void kill_def::add_kill(const monster* mon, level_id place)
{
    kills++;
    // They're only unique if they aren't summoned.
    add_place(place, mons_is_unique(mon->type) && !mon->is_summoned());
}

void kill_def::add_place(level_id place, bool force)
{
    if (find(begin(places), end(places), place) != end(places))
        return; // Already there

    if (force || places.size() < PLACE_LIMIT)
        places.push_back(place);
}

string kill_def::base_name(const kill_monster_desc &md) const
{
    string name;
    if (md.monnum == MONS_PANDEMONIUM_LORD)
        name = "pandemonium lord";
    else
        name = mons_type_name(md.monnum, DESC_PLAIN);

    switch (md.modifier)
    {
    case kill_monster_desc::M_ZOMBIE:
        name += " zombie";
        break;
    case kill_monster_desc::M_DRAUGR:
        name += " draugr";
        break;
    case kill_monster_desc::M_SIMULACRUM:
        name += " simulacrum";
        break;
    case kill_monster_desc::M_SPECTRE:
        name = "spectral " + name;
        break;
    default:
        // Silence compiler warning about not handling M_NORMAL and
        // M_SHAPESHIFTER
        break;
    }

    return name;
}

string kill_def::info(const kill_monster_desc &md) const
{
    string name = base_name(md);

    if (!mons_is_unique(md.monnum))
    {
        // Pluralise as needed.
        name = n_names(name, kills);

        // We brand shapeshifters with the (shapeshifter) qualifier.
        // This has to be done after doing pluralise(), else we get very
        // odd plurals :)
        if (md.modifier == kill_monster_desc::M_SHAPESHIFTER
            && md.monnum != MONS_SHAPESHIFTER
            && md.monnum != MONS_GLOWING_SHAPESHIFTER)
        {
            name += " (shapeshifter)";
        }
    }
    else if (kills > 1)
    {
        // Aha! A resurrected unique
        name += kill_times(kills);
    }

    // What places we killed this type of monster
    return append_places(md, name);
}

string kill_def::append_places(const kill_monster_desc &md,
                               const string &name) const
{
    if (Options.dump_kill_places == KDO_NO_PLACES)
        return name;

    size_t nplaces = places.size();
    if (nplaces == 1 || mons_is_unique(md.monnum)
            || Options.dump_kill_places == KDO_ALL_PLACES)
    {
        string augmented = name;
        augmented += " (";
        for (auto iter = places.begin(); iter != places.end(); ++iter)
        {
            if (iter != places.begin())
                augmented += " ";
            augmented += iter->describe();
        }
        augmented += ")";
        return augmented;
    }
    return name;
}

void kill_def::save(writer& outf) const
{
    marshallShort(outf, kills);
    marshallShort(outf, exp);

    marshallShort(outf, places.size());
    for (auto lvl : places)
        lvl.save(outf);
}

void kill_def::load(reader& inf)
{
    kills = (unsigned short) unmarshallShort(inf);
    exp   = unmarshallShort(inf);

    places.clear();
    short place_count = unmarshallShort(inf);
    for (short i = 0; i < place_count; ++i)
    {
#if TAG_MAJOR_VERSION == 34
        if (inf.getMinorVersion() < TAG_MINOR_PLACE_UNPACK)
        {
            places.push_back(level_id::from_packed_place(
                                (unsigned short) unmarshallShort(inf)));
        }
        else
        {
#endif
        level_id tmp;
        tmp.load(inf);
        places.push_back(tmp);
#if TAG_MAJOR_VERSION == 34
        }
#endif
    }
}

kill_ghost::kill_ghost(const monster* mon)
{
    exp = exp_value(*mon);
    place = level_id::current();
    ghost_name = mon->ghost->name;

    // Check whether this is really a ghost, since we also have to handle
    // the Pandemonic demons.
    if (mon->type == MONS_PLAYER_GHOST && !mon->is_summoned())
    {
        monster_info mi(mon);
        ghost_name = "The ghost of " + get_ghost_description(mi, true);
    }
}

string kill_ghost::info() const
{
    return ghost_name
           + (Options.dump_kill_places != KDO_NO_PLACES ?
                " (" + place.describe() + ")" :
                string(""));
}

void kill_ghost::save(writer& outf) const
{
    marshallString4(outf, ghost_name);
    marshallShort(outf, (unsigned short) exp);
    place.save(outf);
}

void kill_ghost::load(reader& inf)
{
    unmarshallString4(inf, ghost_name);
    exp = unmarshallShort(inf);
#if TAG_MAJOR_VERSION == 34
    if (inf.getMinorVersion() < TAG_MINOR_PLACE_UNPACK)
        place = level_id::from_packed_place((unsigned short) unmarshallShort(inf));
    else
#endif
    place.load(inf);
}

kill_monster_desc::kill_monster_desc(const monster* mon)
{
    monnum = mon->type;
    modifier = M_NORMAL;
    switch (mon->type)
    {
        case MONS_ZOMBIE:
            modifier = M_ZOMBIE;
            break;
        case MONS_DRAUGR:
            modifier = M_DRAUGR;
            break;
        case MONS_SIMULACRUM:
            modifier = M_SIMULACRUM;
            break;
        case MONS_BOUND_SOUL:
        case MONS_SPECTRAL_THING:
            modifier = M_SPECTRE;
            break;
        default: break;
    }
    if (modifier != M_NORMAL)
        monnum = mons_species(mon->base_monster);

    if (mon->is_shapeshifter())
        modifier = M_SHAPESHIFTER;
}

// TODO: de-duplicate with the monster * version?
kill_monster_desc::kill_monster_desc(const monster_info& mon)
{
    monnum = mon.type;
    modifier = M_NORMAL;
    switch (mon.type)
    {
        case MONS_ZOMBIE:
            modifier = M_ZOMBIE;
            break;
        case MONS_DRAUGR:
            modifier = M_DRAUGR;
            break;
        case MONS_SIMULACRUM:
            modifier = M_SIMULACRUM;
            break;
        case MONS_BOUND_SOUL:
        case MONS_SPECTRAL_THING:
            modifier = M_SPECTRE;
            break;
        default: break;
    }
    if (modifier != M_NORMAL)
        monnum = mon.base_type;

    if (mon.is(MB_SHAPESHIFTER))
        modifier = M_SHAPESHIFTER;
}

void kill_monster_desc::save(writer& outf) const
{
    marshallShort(outf, (short) monnum);
    marshallShort(outf, (short) modifier);
}

void kill_monster_desc::load(reader& inf)
{
    monnum = static_cast<monster_type>(unmarshallShort(inf));
    modifier = (name_modifier) unmarshallShort(inf);
}

///////////////////////////////////////////////////////////////////////////
// Kill Lua interface
//

#define KILLEXP_ACCESS(name, type, field) \
    static int kill_lualc_##name(lua_State *ls) \
    { \
        if (!lua_islightuserdata(ls, 1)) \
        { \
            luaL_argerror(ls, 1, "Unexpected argument type"); \
            return 0; \
        } \
          \
        kill_exp *ke = static_cast<kill_exp*>(lua_touserdata(ls, 1)); \
        if (ke) \
        { \
            lua_push##type(ls, ke->field); \
            return 1; \
        } \
        return 0; \
    }

KILLEXP_ACCESS(nkills, number, nkills)
KILLEXP_ACCESS(exp, number, exp)
KILLEXP_ACCESS(base_name, string, base_name.c_str())
KILLEXP_ACCESS(desc, string, desc.c_str())
KILLEXP_ACCESS(monnum, number, monnum)
KILLEXP_ACCESS(isghost, boolean, monnum == MONS_PLAYER_GHOST)
KILLEXP_ACCESS(ispandemon, boolean, monnum == MONS_PANDEMONIUM_LORD)

static int kill_lualc_modifier(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Unexpected argument type");
        return 0;
    }

    kill_exp *ke = static_cast<kill_exp*>(lua_touserdata(ls, 1));
    if (ke)
    {
        const char *modifier;
        switch (ke->modifier)
        {
        case kill_monster_desc::M_ZOMBIE:
            modifier = "zombie";
            break;
        case kill_monster_desc::M_DRAUGR:
            modifier = "draugr";
            break;
        case kill_monster_desc::M_SIMULACRUM:
            modifier = "simulacrum";
            break;
        case kill_monster_desc::M_SPECTRE:
            modifier = "spectre";
            break;
        case kill_monster_desc::M_SHAPESHIFTER:
            modifier = "shapeshifter";
            break;
        default:
            modifier = "";
            break;
        }
        lua_pushstring(ls, modifier);
        return 1;
    }
    return 0;
}

static int kill_lualc_isunique(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Unexpected argument type");
        return 0;
    }

    kill_exp *ke = static_cast<kill_exp*>(lua_touserdata(ls, 1));
    if (ke)
    {
        lua_pushboolean(ls, mons_is_unique(ke->monnum));
        return 1;
    }
    return 0;
}

static int kill_lualc_holiness(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Unexpected argument type");
        return 0;
    }

    kill_exp *ke = static_cast<kill_exp*>(lua_touserdata(ls, 1));
    if (ke)
    {
        const char *verdict = "strange";
        mon_holy_type holi = mons_class_holiness(ke->monnum);
        if (holi & MH_HOLY)
            verdict = "holy";
        else if (holi & MH_NATURAL)
            verdict = "natural";
        else if (holi & MH_UNDEAD)
            verdict = "undead";
        else if (holi & MH_DEMONIC)
            verdict = "demonic";
        else if (holi & MH_NONLIVING)
            verdict = "nonliving";
        else if (holi & MH_PLANT)
            verdict = "plant";
        if (ke->modifier != kill_monster_desc::M_NORMAL
            && ke->modifier != kill_monster_desc::M_SHAPESHIFTER)
        {
            verdict = "undead";
        }
        lua_pushstring(ls, verdict);
        return 1;
    }
    return 0;
}

static int kill_lualc_symbol(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Unexpected argument type");
        return 0;
    }

    kill_exp *ke = static_cast<kill_exp*>(lua_touserdata(ls, 1));
    if (ke)
    {
        char32_t ch = mons_char(ke->monnum);

        if (ke->monnum == MONS_PROGRAM_BUG)
            ch = ' ';

        switch (ke->modifier)
        {
        case kill_monster_desc::M_ZOMBIE:
        case kill_monster_desc::M_DRAUGR:
            ch = mons_char(MONS_DRAUGR);
            break;
        case kill_monster_desc::M_SIMULACRUM:
            ch = mons_char(MONS_SIMULACRUM);
            break;
        case kill_monster_desc::M_SPECTRE:
            ch = mons_char(MONS_SPECTRAL_THING);
            break;
        default:
            break;
        }

        lua_pushstring(ls, stringize_glyph(ch).c_str());
        return 1;
    }
    return 0;
}

static int kill_lualc_rawwrite(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    lua_pushstring(ls, "cr_skill");
    lua_gettable(ls, LUA_REGISTRYINDEX);
    if (!lua_islightuserdata(ls, -1))
    {
        lua_settop(ls, -2);
        fprintf(stderr, "Can't find kill string?\n");
        return 0;
    }

    string *skill = static_cast<string *>(lua_touserdata(ls, -1));
    // Pop the userdata off the stack.
    lua_settop(ls, -2);

    *skill += s;
    *skill += "\n";

    return 0;
}

static int kill_lualc_write(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Unexpected argument type");
        return 0;
    }

    kill_exp *ke = static_cast<kill_exp*>(lua_touserdata(ls, 1));
    if (ke)
    {
        lua_pushstring(ls, "cr_skill");
        lua_gettable(ls, LUA_REGISTRYINDEX);
        if (!lua_islightuserdata(ls, -1))
        {
            lua_settop(ls,-2);
            fprintf(stderr, "Can't find kill string?\n");
            return 0;
        }

        string *skill = static_cast<string *>(lua_touserdata(ls, -1));
        // Pop the userdata off the stack.
        lua_settop(ls, -2);

        // Write kill description and a newline.
        *skill += ke->desc + "\n";
    }
    return 0;
}

static int kill_lualc_summary(lua_State *ls)
{
    if (!lua_istable(ls, 1))
    {
        luaL_argerror(ls, 1, "Unexpected argument type, wanted table");
        return 0;
    }

    unsigned int count = 0;
    for (int i = 1; ; ++i)
    {
        lua_rawgeti(ls, 1, i);
        if (lua_isnil(ls, -1))
        {
            lua_settop(ls, -2);
            break;
        }

        if (!lua_islightuserdata(ls, -1))
        {
            luaL_argerror(ls, 1, "Unexpected argument type");
            return 0;
        }

        kill_exp *ke = static_cast<kill_exp*>(lua_touserdata(ls, -1));
        lua_settop(ls, -2);
        if (ke)
            count += ke->nkills;
    }
    char buf[120];
    *buf = 0;
    if (count)
    {
        snprintf(buf, sizeof buf, "%u creature%s vanquished.",
                count, count > 1? "s" : "");
    }
    lua_pushstring(ls, buf);
    return 1;
}

static const struct luaL_reg kill_lib[] =
{
    { "nkills",     kill_lualc_nkills },
    { "exp"   ,     kill_lualc_exp },
    { "base_name",  kill_lualc_base_name },
    { "desc",       kill_lualc_desc },
    { "monnum",     kill_lualc_monnum },
    { "modifier",   kill_lualc_modifier },
    { "holiness",   kill_lualc_holiness },
    { "symbol",     kill_lualc_symbol },
    { "isghost",    kill_lualc_isghost },
    { "ispandemon", kill_lualc_ispandemon },
    { "isunique",   kill_lualc_isunique },
    { "rawwrite",   kill_lualc_rawwrite },
    { "write",      kill_lualc_write },
    { "summary",    kill_lualc_summary },
    { nullptr, nullptr }
};

void cluaopen_kills(lua_State *ls)
{
    luaL_openlib(ls, "kills", kill_lib, 0);
}

static void kill_lua_filltable(vector<kill_exp> &v)
{
    lua_State *ls = clua.state();
    lua_newtable(ls);
    for (int i = 0, count = v.size(); i < count; ++i)
    {
        lua_pushlightuserdata(ls, &v[i]);
        lua_rawseti(ls, -2, i + 1);
    }
}
