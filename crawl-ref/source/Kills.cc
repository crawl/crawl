/*
 *  File:       Kills.cc
 *  Summary:    Player kill tracking
 *  Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <algorithm>

#include "chardump.h"
#include "describe.h"
#include "mon-util.h"
#include "monstuff.h"
#include "files.h"
#include "ghost.h"
#include "itemname.h"
#include "place.h"
#include "travel.h"
#include "tags.h"
#include "Kills.h"
#include "clua.h"

#define KILLS_MAJOR_VERSION 4
#define KILLS_MINOR_VERSION 1

#ifdef CLUA_BINDINGS
static void kill_lua_filltable(std::vector<kill_exp> &v);
#endif

///////////////////////////////////////////////////////////////////////////
// KillMaster
//

const char *kill_category_names[] =
{
    "you",
    "collateral kills",
    "others",
};

KillMaster::KillMaster()
{
}

KillMaster::KillMaster(const KillMaster &other)
{
    *this = other;
}

KillMaster::~KillMaster()
{
}

const char *KillMaster::category_name(kill_category kc) const
{
    if (kc >= KC_YOU && kc < KC_NCATEGORIES)
        return (kill_category_names[kc]);
    return (NULL);
}

bool KillMaster::empty() const
{
    for (int i = 0; i < KC_NCATEGORIES; ++i)
        if (!categorized_kills[i].empty())
            return (false);
    return (true);
}

void KillMaster::save(writer& outf) const
{
    // Write the version of the kills file
    marshallByte(outf, KILLS_MAJOR_VERSION);
    marshallByte(outf, KILLS_MINOR_VERSION);

    for (int i = 0; i < KC_NCATEGORIES; ++i)
        categorized_kills[i].save(outf);
}

void KillMaster::load(reader& inf)
{
    unsigned char major = unmarshallByte(inf),
                  minor = unmarshallByte(inf);
    if (major != KILLS_MAJOR_VERSION ||
        (minor != KILLS_MINOR_VERSION && minor > 0))
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

void KillMaster::record_kill(const monsters *mon, int killer, bool ispet)
{
    const kill_category kc =
        YOU_KILL(killer)? KC_YOU :
        ispet?            KC_FRIENDLY :
                          KC_OTHER;
    categorized_kills[kc].record_kill(mon);
}

std::string KillMaster::kill_info() const
{
    if (empty())
        return ("");

    std::string killtext;

    bool needseparator = false;
    int categories = 0;
    long grandtotal = 0L;

    Kills catkills[KC_NCATEGORIES];
    for (int i = 0; i < KC_NCATEGORIES; ++i)
    {
        int targ = Options.kill_map[i];
        catkills[targ].merge( categorized_kills[i] );
    }

    for (int i = KC_YOU; i < KC_NCATEGORIES; ++i)
    {
        if (catkills[i].empty())
            continue;

        categories++;
        std::vector<kill_exp> kills;
        long count = catkills[i].get_kills(kills);
        grandtotal += count;

        add_kill_info( killtext,
                       kills,
                       count,
                       i == KC_YOU? NULL :
                                    category_name((kill_category) i),
                       needseparator );
        needseparator = true;
    }

    std::string grandt;
    if (categories > 1)
    {
        char buf[200];
        snprintf(buf, sizeof buf,
                "Grand Total: %ld creatures vanquished",
                grandtotal);
        grandt = buf;
    }

#ifdef CLUA_BINDINGS
    unwind_var<int> lthrottle(clua.throttle_unit_lines, 500000);
    // Call the kill dump Lua function with null a, to tell it we're done.
    if (!clua.callfn("c_kill_list", "ss", NULL, grandt.c_str()))
#endif
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

void KillMaster::add_kill_info(std::string &killtext,
                               std::vector<kill_exp> &kills,
                               long count,
                               const char *category,
                               bool separator) const
{
#ifdef CLUA_BINDINGS
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
    if (!clua.callfn("c_kill_list", 3, 0))
#endif
    {
#ifdef CLUA_BINDINGS
        if (clua.error.length())
        {
            killtext += "Lua error:\n";
            killtext += clua.error + "\n\n";
        }
#endif
        if (separator)
            killtext += "\n";

        killtext += "Vanquished Creatures";
        if (category)
            killtext += std::string(" (") + category + ")";

        killtext += "\n";

        for (int i = 0, sz = kills.size(); i < sz; ++i)
        {
            killtext += "  " + kills[i].desc;
            killtext += "\n";
        }
        {
            char numbuf[100];
            snprintf(numbuf, sizeof numbuf,
                    "%ld creature%s vanquished." "\n", count,
                    count > 1? "s" : "");
            killtext += numbuf;
        }
    }
}

long KillMaster::num_kills(const monsters *mon, kill_category cat) const
{
    return categorized_kills[cat].num_kills(mon);
}

long KillMaster::num_kills(const monsters *mon) const
{
    long total = 0;
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
    ghosts.insert( ghosts.end(), k.ghosts.begin(), k.ghosts.end() );

    // Regular kills are messier to merge.
    for (kill_map::const_iterator i = k.kills.begin();
            i != k.kills.end(); ++i)
    {
        const kill_monster_desc &kmd = i->first;
        kill_def &ki = kills[kmd];
        const kill_def &ko = i->second;
        bool uniq = mons_is_unique(kmd.monnum);
        ki.merge(ko, uniq);
    }
}

void Kills::record_kill(const struct monsters *mon)
{
    // Handle player ghosts separately.
    if (mon->type == MONS_PLAYER_GHOST)
    {
        record_ghost_kill(mon);
        return ;
    }

    // Normal monsters
    // Create a descriptor
    kill_monster_desc descriptor = mon;

    kill_def &k = kills[descriptor];
    if (k.kills)
        k.add_kill(mon, get_packed_place());
    else
        k = kill_def(mon);
}

long Kills::get_kills(std::vector<kill_exp> &all_kills) const
{
    long count = 0;
    kill_map::const_iterator iter = kills.begin();
    for (; iter != kills.end(); ++iter)
    {
        const kill_monster_desc &md = iter->first;
        const kill_def &k = iter->second;
        all_kills.push_back( kill_exp(k, md) );
        count += k.kills;
    }

    ghost_vec::const_iterator gi = ghosts.begin();
    for (; gi != ghosts.end(); ++gi)
    {
        all_kills.push_back( kill_exp(*gi) );
    }
    count += ghosts.size();

    std::sort(all_kills.begin(), all_kills.end());
    return (count);
}

void Kills::save(writer& outf) const
{
    // How many kill records do we have?
    marshallLong(outf, kills.size());

    for ( kill_map::const_iterator iter = kills.begin();
          iter != kills.end(); ++iter)
    {
        iter->first.save(outf);
        iter->second.save(outf);
    }

    // How many ghosts do we have?
    marshallShort(outf, ghosts.size());
    for (ghost_vec::const_iterator iter = ghosts.begin();
         iter != ghosts.end(); ++iter)
    {
        iter->save(outf);
    }
}

void Kills::load(reader& inf)
{
    // How many kill records?
    long kill_count = unmarshallLong(inf);
    kills.clear();
    for (long i = 0; i < kill_count; ++i)
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

void Kills::record_ghost_kill(const struct monsters *mon)
{
    kill_ghost ghostk(mon);
    ghosts.push_back(ghostk);
}

int Kills::num_kills(const monsters *mon) const
{
    kill_monster_desc desc(mon);
    kill_map::const_iterator iter = kills.find(desc);
    int total = (iter == kills.end() ? 0 : iter->second.kills);

    if (desc.modifier == kill_monster_desc::M_SHAPESHIFTER)
    {
        desc.modifier = kill_monster_desc::M_NORMAL;
        iter = kills.find(desc);
        total += (iter == kills.end() ? 0 : iter->second.kills);
    }

    return total;
}

kill_def::kill_def(const struct monsters *mon) : kills(0), exp(0)
{
    exp = exper_value(mon);
    add_kill(mon, get_packed_place());
}

std::string apostrophise(const std::string &name)
{
    if (name.empty())
        return (name);

    const char lastc = name[name.length() - 1];
    return (name + (lastc == 's' ? "'" : "'s"));
}

std::string apostrophise_fixup(const std::string &msg)
{
    if (msg.empty())
        return (msg);

    // XXX: This is rather hackish.
    return (replace_all(msg, "s's", "s'"));
}

// For monster names ending with these suffixes, we pluralise directly without
// attempting to use the "of" rule. For instance:
//
//      moth of wrath           => moths of wrath but
//      moth of wrath zombie    => moth of wrath zombies.
//
// This is not necessary right now, since there are currently no monsters that
// require this special treatment (no monster with 'of' in its name is eligible
// for zombies or skeletons).
static const char *modifier_suffixes[] =
{
    "zombie", "skeleton", "simulacrum", NULL,
};

// For a non-unique monster, prefixes a suitable article if we have only one
// kill, else prefixes a kill count and pluralises the monster name.
static std::string n_names(const std::string &name, int n)
{
    if (n > 1)
    {
        char buf[20];
        snprintf(buf, sizeof buf, "%d ", n);
        return buf + pluralise(name, standard_plural_qualifiers,
                               modifier_suffixes);
    }
    else
        return article_a(name, false);
}

// Returns a string describing the number of times a unique has been killed.
// Currently required only for Boris.
//
static std::string kill_times(int kills)
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
    return std::string(buf);
}

void kill_def::merge(const kill_def &k, bool uniq)
{
    if (!kills)
    {
        *this = k;
    }
    else
    {
        kills += k.kills;
        for (int i = 0, size = k.places.size(); i < size; ++i)
            add_place(k.places[i], uniq);
    }
}

void kill_def::add_kill(const struct monsters *mon, unsigned short place)
{
    kills++;
    add_place(place, mons_is_unique(mon->type));
}

void kill_def::add_place(unsigned short place, bool force)
{
    for (unsigned i = 0; i < places.size(); ++i)
        if (places[i] == place) return;

    if (force || places.size() < PLACE_LIMIT)
        places.push_back(place);
}

std::string kill_def::base_name(const kill_monster_desc &md) const
{
    std::string name;
    if (md.monnum == MONS_PANDEMONIUM_DEMON)
        name = "demon lord";
    else
        name = mons_type_name(md.monnum, DESC_PLAIN);

    switch (md.modifier)
    {
      case kill_monster_desc::M_ZOMBIE:
        name += " zombie";
        break;
      case kill_monster_desc::M_SKELETON:
        name += " skeleton";
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

    switch (md.monnum)
    {
      case MONS_ABOMINATION_LARGE:
        name = "large " + name;
        break;
      case MONS_ABOMINATION_SMALL:
        // Do nothing
        break;
      case MONS_RAKSHASA_FAKE:
        name = "illusory " + name;
        break;
    }
    return name;
}

std::string kill_def::info(const kill_monster_desc &md) const
{
    std::string name = base_name(md);

    if (!mons_is_unique(md.monnum))
    {
        // Pluralise as needed
        name = n_names(name, kills);

        // We brand shapeshifters with the (shapeshifter) qualifier. This
        // has to be done after doing pluralise(), else we get very odd plurals
        // :)
        if (md.modifier == kill_monster_desc::M_SHAPESHIFTER &&
                md.monnum != MONS_SHAPESHIFTER &&
                md.monnum != MONS_GLOWING_SHAPESHIFTER)
            name += " (shapeshifter)";
    }
    else if (kills > 1)
    {
        // Aha! A resurrected unique
        name += kill_times(kills);
    }

    // What places we killed this type of monster
    return append_places(md, name);
}

std::string kill_def::append_places(const kill_monster_desc &md,
                                const std::string &name) const
{
    if (Options.dump_kill_places == KDO_NO_PLACES) return name;

    int nplaces = places.size();
    if ( nplaces == 1 || mons_is_unique(md.monnum)
            || Options.dump_kill_places == KDO_ALL_PLACES )
    {
        std::string augmented = name;
        augmented += " (";
        for (std::vector<unsigned short>::const_iterator iter = places.begin();
                iter != places.end(); ++iter)
        {
            if (iter != places.begin())
                augmented += " ";
            augmented += short_place_name(*iter);
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
    for (std::vector<unsigned short>::const_iterator iter = places.begin();
            iter != places.end(); ++iter)
    {
        marshallShort(outf, *iter);
    }
}

void kill_def::load(reader& inf)
{
    kills = (unsigned short) unmarshallShort(inf);
    exp   = unmarshallShort(inf);

    places.clear();
    short place_count = unmarshallShort(inf);
    for (short i = 0; i < place_count; ++i)
        places.push_back((unsigned short) unmarshallShort(inf));
}

kill_ghost::kill_ghost(const monsters *mon)
{
    exp = exper_value(mon);
    place = get_packed_place();
    ghost_name = mon->ghost->name;

    // Check whether this is really a ghost, since we also have to handle
    // the Pandemonic demons.
    if (mon->type == MONS_PLAYER_GHOST)
        ghost_name = "The ghost of " + get_ghost_description(*mon, true);
}

std::string kill_ghost::info() const
{
    return ghost_name
           + (Options.dump_kill_places != KDO_NO_PLACES?
                " (" + short_place_name(place) + ")" : std::string(""));
}

void kill_ghost::save(writer& outf) const
{
    marshallString4(outf, ghost_name);
    marshallShort(outf, (unsigned short) exp);
    marshallShort(outf, place);
}

void kill_ghost::load(reader& inf)
{
    unmarshallString4(inf, ghost_name);
    exp = unmarshallShort(inf);
    place = (unsigned short) unmarshallShort(inf);
}

kill_monster_desc::kill_monster_desc(const monsters *mon)
{
    monnum = mon->type;
    modifier = M_NORMAL;
    switch (mon->type)
    {
        case MONS_ZOMBIE_LARGE: case MONS_ZOMBIE_SMALL:
            modifier = M_ZOMBIE;
            break;
        case MONS_SKELETON_LARGE: case MONS_SKELETON_SMALL:
            modifier = M_SKELETON;
            break;
        case MONS_SIMULACRUM_LARGE: case MONS_SIMULACRUM_SMALL:
            modifier = M_SIMULACRUM;
            break;
        case MONS_SPECTRAL_THING:
            modifier = M_SPECTRE;
            break;
        default: break;
    }
    if (modifier != M_NORMAL)
        monnum = mon->base_monster;

    if (mons_is_shapeshifter(mon))
        modifier = M_SHAPESHIFTER;

    // XXX: Ugly hack - merge all mimics into one mimic record.
    if (monnum >= MONS_GOLD_MIMIC && monnum <= MONS_POTION_MIMIC)
        monnum = MONS_WEAPON_MIMIC;
}

void kill_monster_desc::save(writer& outf) const
{
    marshallShort(outf, (short) monnum);
    marshallShort(outf, (short) modifier);
}

void kill_monster_desc::load(reader& inf)
{
    monnum = (int) unmarshallShort(inf);
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
        kill_exp *ke = static_cast<kill_exp*>( lua_touserdata(ls, 1) ); \
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
KILLEXP_ACCESS(isghost, boolean,
               monnum == -1 &&
               ke->desc.find("The ghost of") != std::string::npos)
KILLEXP_ACCESS(ispandemon, boolean,
               monnum == -1 &&
               ke->desc.find("The ghost of") == std::string::npos)
KILLEXP_ACCESS(isunique, boolean,
               monnum != -1 && mons_is_unique(ke->monnum))


static int kill_lualc_modifier(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Unexpected argument type");
        return 0;
    }

    kill_exp *ke = static_cast<kill_exp*>( lua_touserdata(ls, 1) );
    if (ke)
    {
        const char *modifier;
        switch (ke->modifier)
        {
        case kill_monster_desc::M_ZOMBIE:
            modifier = "zombie";
            break;
        case kill_monster_desc::M_SKELETON:
            modifier = "skeleton";
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

static int kill_lualc_places(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Unexpected argument type");
        return 0;
    }

    kill_exp *ke = static_cast<kill_exp*>( lua_touserdata(ls, 1) );
    if (ke)
    {
        lua_newtable(ls);
        for (int i = 0, count = ke->places.size(); i < count; ++i)
        {
            lua_pushnumber(ls, ke->places[i]);
            lua_rawseti(ls, -2, i + 1);
        }
        return 1;
    }
    return 0;
}

static int kill_lualc_place_name(lua_State *ls)
{
    int num = luaL_checkint(ls, 1);
    std::string plname = short_place_name(num);
    lua_pushstring(ls, plname.c_str());
    return 1;
}

static bool is_ghost(const kill_exp *ke)
{
    return ke->monnum == -1
        && ke->desc.find("The ghost of") != std::string::npos;
}

static int kill_lualc_holiness(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Unexpected argument type");
        return 0;
    }

    kill_exp *ke = static_cast<kill_exp*>( lua_touserdata(ls, 1) );
    if (ke)
    {
        const char *verdict = "strange";
        if (ke->monnum == -1)
        {
            verdict = is_ghost(ke)? "undead" : "demonic";
        }
        else
        {
            switch (mons_class_holiness(ke->monnum))
            {
            case MH_HOLY:       verdict = "holy"; break;
            case MH_NATURAL:    verdict = "natural"; break;
            case MH_UNDEAD:     verdict = "undead"; break;
            case MH_DEMONIC:    verdict = "demonic"; break;
            case MH_NONLIVING:  verdict = "nonliving"; break;
            case MH_PLANT:      verdict = "plant"; break;
            }
            if (ke->modifier != kill_monster_desc::M_NORMAL
                    && ke->modifier != kill_monster_desc::M_SHAPESHIFTER)
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

    kill_exp *ke = static_cast<kill_exp*>( lua_touserdata(ls, 1) );
    if (ke)
    {
        unsigned char ch = ke->monnum != -1?
                    mons_char(ke->monnum) :
              is_ghost(ke)? 'p' : '&';

        if (ke->monnum == MONS_PROGRAM_BUG)
            ch = ' ';

        switch (ke->modifier)
        {
        case kill_monster_desc::M_ZOMBIE:
        case kill_monster_desc::M_SKELETON:
        case kill_monster_desc::M_SIMULACRUM:
            ch = mons_zombie_size(ke->monnum) == Z_SMALL ? 'z' : 'Z';
            break;
        case kill_monster_desc::M_SPECTRE:
            ch = 'W';
            break;
        default:
            break;
        }

        char s[2];
        s[0] = (char) ch;
        s[1] = 0;
        lua_pushstring(ls, s);
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

    std::string *skill = static_cast<std::string *>( lua_touserdata(ls, -1) );
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

    kill_exp *ke = static_cast<kill_exp*>( lua_touserdata(ls, 1) );
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

        std::string *skill = static_cast<std::string *>(
                                    lua_touserdata(ls, -1) );
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

    unsigned long count = 0;
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

        kill_exp *ke = static_cast<kill_exp*>( lua_touserdata(ls, -1) );
        lua_settop(ls, -2);
        if (ke)
            count += ke->nkills;
    }
    char buf[120];
    *buf = 0;
    if (count)
        snprintf(buf, sizeof buf, "%lu creature%s vanquished.",
                count, count > 1? "s" : "");
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
    { "places",     kill_lualc_places },
    { "place_name", kill_lualc_place_name },
    { "holiness",   kill_lualc_holiness },
    { "symbol",     kill_lualc_symbol },
    { "isghost",    kill_lualc_isghost },
    { "ispandemon", kill_lualc_ispandemon },
    { "isunique",   kill_lualc_isunique },
    { "rawwrite",   kill_lualc_rawwrite },
    { "write",      kill_lualc_write },
    { "summary",    kill_lualc_summary },
    { NULL, NULL }
};

void luaopen_kills(lua_State *ls)
{
    luaL_openlib(ls, "kills", kill_lib, 0);
}

#ifdef CLUA_BINDINGS
static void kill_lua_filltable(std::vector<kill_exp> &v)
{
    lua_State *ls = clua.state();
    lua_newtable(ls);
    for (int i = 0, count = v.size(); i < count; ++i)
    {
        lua_pushlightuserdata(ls, &v[i]);
        lua_rawseti(ls, -2, i + 1);
    }
}
#endif
