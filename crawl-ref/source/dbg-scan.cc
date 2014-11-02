/**
 * @file
 * @brief Debugging code to scan the list of items and monsters.
**/

#include "AppHdr.h"

#include "dbg-scan.h"

#include <cmath>
#include <sstream>
#include <errno.h>

#include "artefact.h"
#include "branch.h"
#include "butcher.h"
#include "chardump.h"
#include "coord.h"
#include "coordit.h"
#include "dbg-maps.h"
#include "dbg-util.h"
#include "decks.h"
#include "dungeon.h"
#include "end.h"
#include "env.h"
#include "godabil.h"
#include "initfile.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "mon-util.h"
#include "ng-init.h"
#include "shopping.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "terrain.h"
#include "traps.h"
#include "version.h"

#define DEBUG_ITEM_SCAN
#ifdef DEBUG_ITEM_SCAN
static void _dump_item(const char *name, int num, const item_def &item,
                       PRINTF(3, ));

static void _dump_item(const char *name, int num, const item_def &item,
                       const char *format, ...)
{
#ifdef DEBUG_FATAL
    const msg_channel_type chan = MSGCH_WARN;
#else
    const msg_channel_type chan = MSGCH_ERROR;
#endif

    va_list args;
    va_start(args, format);
    string msg = vmake_stringf(format, args);
    va_end(args);

    mprf(chan, "%s", msg.c_str());
    mprf(chan, "%s", name);

    mprf("    item #%d:  base: %d; sub: %d; plus: %d; plus2: %d; special: %d",
         num, item.base_type, item.sub_type,
         item.plus, item.plus2, item.special);

    mprf("    quant: %d; colour: %d; ident: 0x%08" PRIx32"; ident_type: %d",
         item.quantity, (int)item.colour, item.flags,
         get_ident_type(item));

    mprf("    x: %d; y: %d; link: %d", item.pos.x, item.pos.y, item.link);

#ifdef DEBUG_FATAL
    if (!crawl_state.game_crashed)
        die("%s %s", msg.c_str(), name);
#endif
    crawl_state.cancel_cmd_repeat();
}

//---------------------------------------------------------------
//
// debug_item_scan
//
//---------------------------------------------------------------
void debug_item_scan()
{
    int   i;
    char  name[256];

    FixedBitVector<MAX_ITEMS> visited;

    // First we're going to check all the stacks on the level:
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        // Unlinked temporary items.
        if (*ri == coord_def())
            continue;

        // Looking for infinite stacks (ie more links than items allowed)
        // and for items which have bad coordinates (can't find their stack)
        for (int obj = igrd(*ri); obj != NON_ITEM; obj = mitm[obj].link)
        {
            if (obj < 0 || obj > MAX_ITEMS)
            {
                if (igrd(*ri) == obj)
                {
                    mprf(MSGCH_ERROR, "Igrd has invalid item index %d "
                                      "at (%d, %d)",
                         obj, ri->x, ri->y);
                }
                else
                {
                    mprf(MSGCH_ERROR, "Item in stack at (%d, %d) has "
                                      "invalid link %d",
                         ri->x, ri->y, obj);
                }
                break;
            }

            // Check for invalid (zero quantity) items that are linked in.
            if (!mitm[obj].defined())
            {
                _dump_item(mitm[obj].name(DESC_PLAIN).c_str(), obj, mitm[obj],
                           "Linked invalid item at (%d,%d)!", ri->x, ri->y);
            }

            // Check that item knows what stack it's in.
            if (mitm[obj].pos != *ri)
            {
                _dump_item(mitm[obj].name(DESC_PLAIN).c_str(), obj, mitm[obj],
                           "Item position incorrect at (%d,%d)!", ri->x, ri->y);
            }

            // If we run into a premarked item we're in real trouble,
            // this will also keep this from being an infinite loop.
            if (visited[obj])
            {
                mprf(MSGCH_ERROR,
                     "Potential INFINITE STACK at (%d, %d)", ri->x, ri->y);
                break;
            }
            visited.set(obj);
        }
    }

    // Now scan all the items on the level:
    for (i = 0; i < MAX_ITEMS; ++i)
    {
        if (!mitm[i].defined())
            continue;

        strlcpy(name, mitm[i].name(DESC_PLAIN).c_str(), sizeof(name));

        const monster* mon = mitm[i].holding_monster();

        // Don't check (-1, -1) player items or (-2, -2) monster items
        // (except to make sure that the monster is alive).
        if (mitm[i].pos.origin())
            _dump_item(name, i, mitm[i], "Unlinked temporary item:");
        else if (mon != NULL && mon->type == MONS_NO_MONSTER)
            _dump_item(name, i, mitm[i], "Unlinked item held by dead monster:");
        else if ((mitm[i].pos.x > 0 || mitm[i].pos.y > 0) && !visited[i])
        {
            _dump_item(name, i, mitm[i], "Unlinked item:");

            if (!in_bounds(mitm[i].pos))
            {
                mprf(MSGCH_ERROR, "Item position (%d, %d) is out of bounds",
                     mitm[i].pos.x, mitm[i].pos.y);
            }
            else
            {
                mprf("igrd(%d,%d) = %d",
                     mitm[i].pos.x, mitm[i].pos.y, igrd(mitm[i].pos));
            }

            // Let's check to see if it's an errant monster object:
            for (int j = 0; j < MAX_MONSTERS; ++j)
                for (int k = 0; k < NUM_MONSTER_SLOTS; ++k)
                {
                    if (menv[j].inv[k] == i)
                    {
                        mprf("Held by monster #%d: %s at (%d,%d)",
                             j, menv[j].name(DESC_A, true).c_str(),
                             menv[j].pos().x, menv[j].pos().y);
                    }
                }
        }

        // Current bad items of interest:
        //   -- armour and weapons with large enchantments/illegal special vals
        //
        //   -- items described as questionable (the class 100 bug)
        //
        //   -- eggplant is an illegal throwing weapon
        //
        //   -- items described as buggy (typically adjectives out of range)
        //      (note: covers buggy, bugginess, buggily, whatever else)
        //
        // Theoretically some of these could match random names.
        //
        if (strstr(name, "questionable") != NULL
            || strstr(name, "eggplant") != NULL
            || strstr(name, "buggy") != NULL
            || strstr(name, "buggi") != NULL)
        {
            _dump_item(name, i, mitm[i], "Bad item:");
        }
        else if (abs(mitm[i].plus) > 30 &&
                    (mitm[i].base_type == OBJ_WEAPONS
                     || mitm[i].base_type == OBJ_ARMOUR))
        {
            _dump_item(name, i, mitm[i], "Bad plus:");
        }
        else if (!is_artefact(mitm[i])
                 && (mitm[i].base_type == OBJ_WEAPONS
                     && mitm[i].special >= NUM_SPECIAL_WEAPONS
                     || mitm[i].base_type == OBJ_ARMOUR
                        && mitm[i].special >= NUM_SPECIAL_ARMOURS))
        {
            _dump_item(name, i, mitm[i], "Bad special value:");
        }
        else if (mitm[i].flags & ISFLAG_SUMMONED && in_bounds(mitm[i].pos))
            _dump_item(name, i, mitm[i], "Summoned item on floor:");
    }

    // Quickly scan monsters for "program bug"s.
    for (i = 0; i < MAX_MONSTERS; ++i)
    {
        const monster& mons = menv[i];

        if (mons.type == MONS_NO_MONSTER)
            continue;

        if (mons.name(DESC_PLAIN, true).find("questionable") != string::npos)
        {
            mprf(MSGCH_ERROR, "Program bug detected!");
            mprf(MSGCH_ERROR,
                 "Buggy monster detected: monster #%d; position (%d,%d)",
                 i, mons.pos().x, mons.pos().y);
        }
    }
}
#endif

#define DEBUG_MONS_SCAN
#ifdef DEBUG_MONS_SCAN
static void _announce_level_prob(bool warned)
{
    if (!warned && crawl_state.generating_level)
    {
        mprf(MSGCH_ERROR, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        mprf(MSGCH_ERROR, "mgrd problem occurred during level generation");

        debug_dump_levgen();
    }
}

static bool _inside_vault(const vault_placement& place, const coord_def &pos)
{
    const coord_def delta = pos - place.pos;

    return delta.x >= 0 && delta.y >= 0
           && delta.x < place.size.x && delta.y < place.size.y;
}

static vector<string> _in_vaults(const coord_def &pos)
{
    vector<string> out;

    for (unsigned int i = 0; i < env.level_vaults.size(); ++i)
    {
        const vault_placement &vault = *env.level_vaults[i];
        if (_inside_vault(vault, pos))
            out.push_back(vault.map.name);
    }

    for (unsigned int i = 0; i < Temp_Vaults.size(); ++i)
    {
        const vault_placement &vault = Temp_Vaults[i];
        if (_inside_vault(vault, pos))
            out.push_back(vault.map.name);
    }

    return out;
}

static string _vault_desc(const coord_def pos)
{
    int mi = env.level_map_ids(pos);
    if (mi == INVALID_MAP_INDEX)
        return "";

    string out;

    for (unsigned int i = 0; i < env.level_vaults.size(); ++i)
    {
        const vault_placement &vp = *env.level_vaults[i];
        if (_inside_vault(vp, pos))
        {
            coord_def br = vp.pos + vp.size - 1;
            out += make_stringf(" [vault: %s (%d,%d)-(%d,%d) (%dx%d)]",
                        vp.map_name_at(pos).c_str(),
                        vp.pos.x, vp.pos.y,
                        br.x, br.y,
                        vp.size.x, vp.size.y);
        }
    }

    return out;
}

void debug_mons_scan()
{
    vector<coord_def> bogus_pos;
    vector<int>       bogus_idx;

    bool warned = false;
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            const int mons = mgrd[x][y];
            if (mons == NON_MONSTER)
                continue;

            if (invalid_monster_index(mons))
            {
                mprf(MSGCH_ERROR, "mgrd at (%d, %d) has invalid monster "
                                  "index %d",
                     x, y, mons);
                continue;
            }

            const monster* m = &menv[mons];
            const coord_def pos(x, y);
            if (m->pos() != pos)
            {
                bogus_pos.push_back(pos);
                bogus_idx.push_back(mons);

                _announce_level_prob(warned);
                mprf(MSGCH_WARN,
                     "Bogosity: mgrd at (%d,%d) points at %s, "
                     "but monster is at (%d,%d)",
                     x, y, m->name(DESC_PLAIN, true).c_str(),
                     m->pos().x, m->pos().y);
                if (!m->alive())
                    mprf(MSGCH_WARN, "Additionally, it isn't alive.");
                warned = true;
            }
            else if (!m->alive())
            {
                _announce_level_prob(warned);
                mprf_nocap(MSGCH_ERROR,
                     "mgrd at (%d,%d) points at dead monster %s",
                     x, y, m->name(DESC_PLAIN, true).c_str());
                warned = true;
            }
        }

    ASSERT(you.type == MONS_PLAYER);
    ASSERT(you.mid == MID_PLAYER);

    vector<int> floating_mons;
    bool             is_floating[MAX_MONSTERS];

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        is_floating[i] = false;

        const monster* m = &menv[i];
        if (!m->alive())
            continue;

        ASSERT(m->mid > 0);
        coord_def pos = m->pos();

        if (invalid_monster_type(m->type))
        {
            mprf(MSGCH_ERROR, "Bogus monster type %d at (%d, %d), midx = %d",
                              m->type, pos.x, pos.y, i);
        }

        if (!in_bounds(pos))
        {
            mprf(MSGCH_ERROR, "Out of bounds monster: %s at (%d, %d), "
                              "midx = %d",
                 m->full_name(DESC_PLAIN, true).c_str(),
                 pos.x, pos.y, i);
        }
        else if (mgrd(pos) != i)
        {
            floating_mons.push_back(i);
            is_floating[i] = true;

            _announce_level_prob(warned);
            mprf(MSGCH_WARN, "Floating monster: %s at (%d,%d), midx = %d",
                 m->full_name(DESC_PLAIN, true).c_str(),
                 pos.x, pos.y, i);
            warned = true;
            for (int j = 0; j < MAX_MONSTERS; ++j)
            {
                if (i == j)
                    continue;

                const monster* m2 = &menv[j];

                if (m2->pos() != m->pos())
                    continue;

                string full = m2->full_name(DESC_PLAIN, true);
                if (m2->alive())
                {
                    mprf(MSGCH_WARN, "Also at (%d, %d): %s, midx = %d",
                         pos.x, pos.y, full.c_str(), j);
                }
                else if (m2->type != MONS_NO_MONSTER)
                {
                    mprf(MSGCH_WARN, "Dead mon also at (%d, %d): %s,"
                                     "midx = %d",
                         pos.x, pos.y, full.c_str(), j);
                }
            }
        } // if (mgrd(m->pos()) != i)

        if (feat_is_wall(grd(pos)))
        {
#if defined(DEBUG_FATAL)
            // if we're going to dump, point out the culprit
            env.pgrid(pos) |= FPROP_HIGHLIGHT;
#endif
            mprf(MSGCH_ERROR, "Monster %s in %s at (%d, %d)%s",
                 m->full_name(DESC_PLAIN, true).c_str(),
                 dungeon_feature_name(grd(pos)),
                 pos.x, pos.y,
                 _vault_desc(pos).c_str());
        }

        for (int j = 0; j < NUM_MONSTER_SLOTS; ++j)
        {
            const int idx = m->inv[j];
            if (idx == NON_ITEM)
                continue;

            if (idx < 0 || idx > MAX_ITEMS)
            {
                mprf(MSGCH_ERROR, "Monster %s (%d, %d) has invalid item "
                                  "index %d in slot %d.",
                     m->full_name(DESC_PLAIN, true).c_str(),
                     pos.x, pos.y, idx, j);
                continue;
            }
            item_def &item(mitm[idx]);

            if (!item.defined())
            {
                _announce_level_prob(warned);
                warned = true;
                mprf(MSGCH_WARN, "Monster %s (%d, %d) holding invalid item in "
                                 "slot %d (midx = %d)",
                     m->full_name(DESC_PLAIN, true).c_str(),
                     pos.x, pos.y, j, i);
                continue;
            }

            const monster* holder = item.holding_monster();

            if (holder == NULL)
            {
                _announce_level_prob(warned);
                warned = true;
                _dump_item(item.name(DESC_PLAIN, false, true).c_str(),
                            idx, item,
                           "Monster %s (%d, %d) holding non-monster "
                           "item (midx = %d)",
                           m->full_name(DESC_PLAIN, true).c_str(),
                           pos.x, pos.y, i);
                continue;
            }

            if (holder != m)
            {
                _announce_level_prob(warned);
                warned = true;
                mprf(MSGCH_WARN, "Monster %s (%d, %d) [midx = %d] holding "
                                 "item %s, but item thinks it's held by "
                                 "monster %s (%d, %d) [midx = %d]",
                     m->full_name(DESC_PLAIN, true).c_str(),
                     m->pos().x, m->pos().y, i,
                     item.name(DESC_PLAIN).c_str(),
                     holder->full_name(DESC_PLAIN, true).c_str(),
                     holder->pos().x, holder->pos().y, holder->mindex());

                bool found = false;
                for (int k = 0; k < NUM_MONSTER_SLOTS; ++k)
                {
                    if (holder->inv[k] == idx)
                    {
                        mprf(MSGCH_WARN, "Other monster thinks it's holding the item, too.");
                        found = true;
                        break;
                    }
                }
                if (!found)
                    mprf(MSGCH_WARN, "Other monster isn't holding it, though.");
            } // if (holder != m)
        } // for (int j = 0; j < NUM_MONSTER_SLOTS; j++)

        monster* m1 = monster_by_mid(m->mid);
        if (m1 != m)
        {
            if (!m1)
                die("mid cache bogosity: no monster for %d", m->mid);
            else if (m1->mid == m->mid)
            {
                mprf(MSGCH_ERROR,
                     "Error: monster %s(%d) has same mid as %s(%d) (%d)",
                     m->name(DESC_PLAIN, true).c_str(), m->mindex(),
                     m1->name(DESC_PLAIN, true).c_str(), m1->mindex(), m->mid);
            }
            else
                die("mid cache bogosity: wanted %d got %d", m->mid, m1->mid);
        }

        if (you.constricted_by == m->mid && (!m->constricting
              || m->constricting->find(MID_PLAYER) == m->constricting->end()))
        {
            mprf(MSGCH_ERROR, "Error: constricting[you] entry missing for monster %s(%d)",
                 m->name(DESC_PLAIN, true).c_str(), m->mindex());
        }

        if (m->constricted_by)
        {
            const actor *h = actor_by_mid(m->constricted_by);
            if (!h)
            {
                mprf(MSGCH_ERROR, "Error: constrictor missing for monster %s(%d)",
                     m->name(DESC_PLAIN, true).c_str(), m->mindex());
            }
            if (!h->constricting
                || h->constricting->find(m->mid) == h->constricting->end())
            {
                mprf(MSGCH_ERROR, "Error: constricting[%s(mindex=%d mid=%d)] "
                                  "entry missing for monster %s(mindex=%d mid=%d)",
                     m->name(DESC_PLAIN, true).c_str(), m->mindex(), m->mid,
                     h->name(DESC_PLAIN, true).c_str(), h->mindex(), h->mid);
            }
        }
    } // for (int i = 0; i < MAX_MONSTERS; ++i)

    for (map<mid_t, unsigned short>::const_iterator mc = env.mid_cache.begin();
         mc != env.mid_cache.end(); ++mc)
    {
        unsigned short idx = mc->second;
        ASSERT(!invalid_monster_index(idx));
        if (menv[idx].mid != mc->first)
        {
            monster &m(menv[idx]);
            die("mid cache bogosity: mid %d points to %s mindex=%d mid=%d",
                mc->first, m.name(DESC_PLAIN, true).c_str(), m.mindex(), m.mid);
        }
    }

    if (in_bounds(you.pos()))
        if (const monster* m = monster_at(you.pos()))
            if (!m->submerged() && !fedhas_passthrough(m))
            {
                mprf(MSGCH_ERROR, "Error: player on same spot as monster: %s(%d)",
                      m->name(DESC_PLAIN, true).c_str(), m->mindex());
            }

    // No problems?
    if (!warned)
        return;

    // If this wasn't the result of generating a level then there's nothing
    // more to report.
    if (!crawl_state.generating_level)
    {
        // Force the dev to notice problems. :P
        more();
        return;
    }

    // No vaults to report on?
    if (env.level_vaults.empty() && Temp_Vaults.empty())
    {
        mprf(MSGCH_ERROR, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        // Force the dev to notice problems. :P
        more();
        return;
    }

    mpr("");

    for (unsigned int i = 0; i < floating_mons.size(); ++i)
    {
        const int       idx = floating_mons[i];
        const monster* mon = &menv[idx];
        vector<string> vaults = _in_vaults(mon->pos());

        string str = make_stringf("Floating monster %s (%d, %d)",
                                  mon->name(DESC_PLAIN, true).c_str(),
                                  mon->pos().x, mon->pos().y);

        if (vaults.empty())
            mprf(MSGCH_WARN, "%s not in any vaults.", str.c_str());
        else
        {
            mpr_comma_separated_list(str + " in vault(s) ", vaults,
                                     " and ", ", ", MSGCH_WARN);
        }
    }

    mpr("");

    for (unsigned int i = 0; i < bogus_pos.size(); ++i)
    {
        const coord_def pos = bogus_pos[i];
        const int       idx = bogus_idx[i];
        const monster* mon = &menv[idx];

        string str = make_stringf("Bogus mgrd (%d, %d) pointing to %s", pos.x,
                                  pos.y, mon->name(DESC_PLAIN, true).c_str());

        vector<string> vaults = _in_vaults(pos);

        if (vaults.empty())
            mprf(MSGCH_WARN, "%s not in any vaults.", str.c_str());
        else
        {
            mpr_comma_separated_list(str + " in vault(s) ", vaults,
                                     " and ", ", ", MSGCH_WARN);
        }

        // Don't report on same monster twice.
        if (is_floating[idx])
            continue;

        str    = "Monster pointed to";
        vaults = _in_vaults(mon->pos());

        if (vaults.empty())
            mprf(MSGCH_WARN, "%s not in any vaults.", str.c_str());
        else
        {
            mpr_comma_separated_list(str + " in vault(s) ", vaults,
                                     " and ", ", ", MSGCH_WARN);
        }
    }

    mprf(MSGCH_ERROR, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    // Force the dev to notice problems. :P
    more();
}
#endif

/**
 * Check the map for validity, generating a crash if not.
 *
 * This checks the loaded map to make sure all dungeon features and shops are
 * valid, all branch exits are generated, and all portals generated at fixed
 * levels in the Depths are actually present.
 */
void check_map_validity()
{
#ifdef ASSERTS
    dungeon_feature_type portal = DNGN_UNSEEN;
    if (player_in_branch(BRANCH_DEPTHS))
    {
        if (you.depth == 3)
            portal = DNGN_ENTER_PANDEMONIUM;
        else if (you.depth == 4)
            portal = DNGN_ENTER_ABYSS;
        else if (you.depth == 2)
            portal = DNGN_ENTER_HELL;
    }

    dungeon_feature_type exit = DNGN_UNSEEN;
    if (you.depth == 1 && !player_in_branch(root_branch))
        exit = branches[you.where_are_you].exit_stairs;

    // these may require you to look farther:
    if (exit == DNGN_EXIT_PANDEMONIUM)
        exit = DNGN_TRANSIT_PANDEMONIUM;
    if (exit == DNGN_EXIT_ABYSS)
        exit = DNGN_UNSEEN;

    for (rectangle_iterator ri(0); ri; ++ri)
    {
        dungeon_feature_type feat = grd(*ri);
        if (feat <= DNGN_UNSEEN || feat >= NUM_FEATURES)
            die("invalid feature %d at (%d,%d)", feat, ri->x, ri->y);
        const char *name = dungeon_feature_name(feat);
        ASSERT(name);
        ASSERT(*name); // placeholders get empty names

        find_trap(*ri); // this has all needed asserts already

        if (shop_struct *shop = get_shop(*ri))
            ASSERT_RANGE(shop->type, 0, NUM_SHOPS);

        // border must be impassable
        if (!in_bounds(*ri))
            ASSERT(feat_is_solid(feat));

        if (env.level_map_mask(*ri) & MMT_MIMIC)
            continue;
        // no mimics below

        const dungeon_feature_type orig = orig_terrain(*ri);
        if (feat == portal || orig == portal)
            portal = DNGN_UNSEEN;
        if (feat == exit || orig == exit)
            exit = DNGN_UNSEEN;
    }

    if (portal)
    {
#ifdef DEBUG_DIAGNOSTICS
        dump_map("missing_portal.map", true);
#endif
        die("Portal %s[%d] didn't get generated.", dungeon_feature_name(portal), portal);
    }
    if (exit)
    {
#ifdef DEBUG_DIAGNOSTICS
        dump_map("missing_exit.map", true);
#endif
        die("Exit %s[%d] didn't get generated.", dungeon_feature_name(exit), exit);
    }

    // And just for good measure:
    debug_item_scan();
    debug_mons_scan();
#endif
}

#ifdef DEBUG_DIAGNOSTICS
static FILE *stat_outf;
const static char *stat_out_prefix = "objstat_";
const static char *stat_out_ext = ".txt";
#define STAT_PRECISION 2

enum item_base_type {
    ITEM_FOOD,
    ITEM_GOLD,
    ITEM_SCROLLS,
    ITEM_POTIONS,
    ITEM_WANDS,
    ITEM_WEAPONS,
    ITEM_MISSILES,
    ITEM_STAVES,
    ITEM_ARMOUR,
    ITEM_JEWELLERY,
    ITEM_MISCELLANY,
    ITEM_RODS,
    ITEM_DECKS,
    ITEM_BOOKS,
    ITEM_ARTEBOOKS,
    ITEM_MANUALS,
    NUM_ITEM_BASE_TYPES,
    ITEM_IGNORE = 100,
};

enum antiquity_level {
    ANTIQ_ORDINARY,
    ANTIQ_ARTEFACT,
    ANTIQ_ALL,
};

class item_type
{
public:
    item_type(item_base_type ct, int st) : base_type(ct), sub_type(st)
    {
    }
    item_type(item_def &item);

    item_base_type base_type;
    int sub_type;
};

static level_id all_lev(NUM_BRANCHES, -1);
static map<branch_type, vector<level_id> > stat_branches;
static int num_branches = 0;
static int num_levels = 0;

// item_recs[level_id][item.base_type][item.sub_type][field]
static map<level_id, FixedVector<map<int, map<string, double> >, NUM_ITEM_BASE_TYPES> > item_recs;

// weapon_brands[level_id][item.base_type][item.sub_type][antiquity_level][brand];
// arte_sum is 0 for ordinary, 1 for artefact, or 2 for all
static map<level_id, vector <vector< vector< vector< int> > > > > equip_brands;
static map<level_id, vector< vector< vector< int> > > > armour_brands;
static map<level_id, vector< vector< int> > > missile_brands;

static FixedVector< vector<string>, NUM_ITEM_BASE_TYPES> item_fields;

static const char* equip_brand_fields[] = {"OrdBrandNums", "ArteBrandNums",
                                           "AllBrandNums"};
static const char* missile_brand_field = "BrandNums";

static map<int, int> valid_foods;

static vector<string> monster_fields;
static map<monster_type, int> valid_monsters;
static map<level_id, map<int, map <string, double> > > monster_recs;

static bool _is_valid_food_type(int sub_type)
{
    return sub_type != FOOD_CHUNK && food_type_name(sub_type) != "buggy";
}

static void _init_foods()
{
    int num_foods = 0;
    for (int i = 0; i < NUM_FOODS; i++)
    {
        if (_is_valid_food_type(i))
            valid_foods[i] = num_foods++;
    }
}

static void _init_monsters()
{
    int num_mons = 0;
    for (int i = 0; i < NUM_MONSTERS; i++)
    {
        monster_type mc = static_cast<monster_type>(i);
        if (!mons_class_flag(mc, M_NO_EXP_GAIN)
            && !mons_class_flag(mc, M_CANT_SPAWN))
        {
            valid_monsters[mc] = num_mons++;
        }
    }
    // For the all-monster summary
    valid_monsters[NUM_MONSTERS] = num_mons;
}

static item_base_type _item_base_type(item_def &item)
{
    item_base_type type;
    switch (item.base_type)
    {
    case OBJ_MISCELLANY:
        if (item.sub_type >= MISC_FIRST_DECK && item.sub_type <= MISC_LAST_DECK)
            type = ITEM_DECKS;
        else
            type = ITEM_MISCELLANY;
        break;
    case OBJ_BOOKS:
        if (item.sub_type == BOOK_MANUAL)
            type = ITEM_MANUALS;
        else if (is_artefact(item))
            type = ITEM_ARTEBOOKS;
        else
            type = ITEM_BOOKS;
        break;
    case OBJ_FOOD:
        if (valid_foods.count(item.sub_type))
            type = ITEM_FOOD;
        else
            type = ITEM_IGNORE;
        break;
    case OBJ_GOLD:
        type = ITEM_GOLD;
        break;
    case OBJ_SCROLLS:
        type = ITEM_SCROLLS;
        break;
    case OBJ_POTIONS:
        type = ITEM_POTIONS;
        break;
    case OBJ_WANDS:
        type = ITEM_WANDS;
        break;
    case OBJ_WEAPONS:
        type = ITEM_WEAPONS;
        break;
    case OBJ_MISSILES:
        type = ITEM_MISSILES;
        break;
    case OBJ_STAVES:
        type = ITEM_STAVES;
        break;
    case OBJ_ARMOUR:
        type = ITEM_ARMOUR;
        break;
    case OBJ_JEWELLERY:
        type = ITEM_JEWELLERY;
        break;
    case OBJ_RODS:
        type = ITEM_RODS;
        break;
    default:
        type = ITEM_IGNORE;
        break;
    }
    return type;
}

static object_class_type _item_orig_base_type(item_base_type base_type)
{
    object_class_type type;
    switch (base_type)
    {
    case ITEM_FOOD:
        type = OBJ_FOOD;
        break;
    case ITEM_GOLD:
        type = OBJ_GOLD;
        break;
    case ITEM_SCROLLS:
        type = OBJ_SCROLLS;
        break;
    case ITEM_POTIONS:
        type = OBJ_POTIONS;
        break;
    case ITEM_WANDS:
        type = OBJ_WANDS;
        break;
    case ITEM_WEAPONS:
        type = OBJ_WEAPONS;
        break;
    case ITEM_MISSILES:
        type = OBJ_MISSILES;
        break;
    case ITEM_STAVES:
        type = OBJ_STAVES;
        break;
    case ITEM_ARMOUR:
        type = OBJ_ARMOUR;
        break;
    case ITEM_JEWELLERY:
        type = OBJ_JEWELLERY;
        break;
    case ITEM_RODS:
        type = OBJ_RODS;
        break;
    case ITEM_DECKS:
    case ITEM_MISCELLANY:
        type = OBJ_MISCELLANY;
        break;
    case ITEM_ARTEBOOKS:
    case ITEM_MANUALS:
    case ITEM_BOOKS:
        type = OBJ_BOOKS;
        break;
    default:
        type = OBJ_UNASSIGNED;
        break;
    }
    return type;
}

// Get the actual food subtype
static int _orig_food_subtype(int sub_type)
{
    map<int, int>::const_iterator mi;
    for (mi = valid_foods.begin(); mi != valid_foods.end(); mi++)
    {
        if (mi->second == sub_type)
            return mi->first;
    }
    die("Invalid food subtype");
    return 0;
}

static string _item_class_name(item_base_type base_type)
{
    string name;
    switch (base_type)
    {
    case ITEM_DECKS:
        name = "Decks";
        break;
    case ITEM_ARTEBOOKS:
        name = "Artefact Spellbooks";
        break;
    case ITEM_MANUALS:
        name = "Manuals";
        break;
    default:
        name = item_class_name(_item_orig_base_type(base_type));
    }
    return name;
}

static int _item_orig_sub_type(item_type &item)
{
    int type;
    switch (item.base_type)
    {
    case ITEM_DECKS:
        type = item.sub_type + MISC_FIRST_DECK;
        break;
    case ITEM_MISCELLANY:
        if (item.sub_type >= MISC_FIRST_DECK)
            type = item.sub_type + MISC_LAST_DECK - MISC_FIRST_DECK + 1;
        else
            type = item.sub_type;
        break;
    case ITEM_FOOD:
        type = _orig_food_subtype(item.sub_type);
        break;
    case ITEM_ARTEBOOKS:
        type = item.sub_type + BOOK_RANDART_LEVEL;
        break;
    case ITEM_MANUALS:
        type = BOOK_MANUAL;
        break;
    default:
        type = item.sub_type;
        break;
    }
    return type;
}

static int _item_max_sub_type(item_base_type base_type)
{
    int num = 0;
    switch (base_type)
    {
    case ITEM_FOOD:
        num = valid_foods.size();
        break;
    case ITEM_MISCELLANY:
        // Decks not counted here.
        num = NUM_MISCELLANY - (MISC_LAST_DECK - MISC_FIRST_DECK + 1);
        break;
    case ITEM_DECKS:
        num = MISC_LAST_DECK - MISC_FIRST_DECK + 1;
        break;
    case ITEM_BOOKS:
        num = MAX_RARE_BOOK + 1;
        break;
    case ITEM_ARTEBOOKS:
        num = 2;
        break;
    case ITEM_MANUALS:
        num = 1;
        break;
    default:
        num = get_max_subtype(_item_orig_base_type(base_type));
        break;
    }
    return num;
}

static item_def _dummy_item(item_type &item)
{
    item_def dummy_item;
    dummy_item.base_type = _item_orig_base_type(item.base_type);
    dummy_item.sub_type = _item_orig_sub_type(item);
    // Deck name is reported as buggy if this is not done.
    if (item.base_type == ITEM_DECKS)
    {
        dummy_item.plus = 1;
        dummy_item.special  = DECK_RARITY_COMMON;
        init_deck(dummy_item);
    }
    return dummy_item;
}

static bool _item_has_antiquity(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_WEAPONS:
    case ITEM_ARMOUR:
    case ITEM_JEWELLERY:
        return true;
    default:
        return false;
    }
}

item_type::item_type(item_def &item)
{
    base_type = _item_base_type(item);
    if (base_type == ITEM_DECKS)
        sub_type = item.sub_type - MISC_FIRST_DECK;
    else if (base_type == ITEM_MISCELLANY && item.sub_type > MISC_LAST_DECK)
        sub_type = item.sub_type - (MISC_LAST_DECK - MISC_FIRST_DECK + 1);
    else if (base_type == ITEM_FOOD)
        sub_type = valid_foods[item.sub_type];
    else if (base_type == ITEM_ARTEBOOKS)
        sub_type = item.sub_type - BOOK_RANDART_LEVEL;
    else if (base_type == ITEM_MANUALS)
        sub_type = 0;
    else
        sub_type = item.sub_type;
}

// This could be done without the macros in C++11, where std::vector has a
// constructor that takes an initializer list.
#define INIT_VEC(vec, ...) do                                  \
    {                                                          \
        vector<string> &x = (vec);                             \
        static const char *const ary[] = { __VA_ARGS__ };      \
        x.insert(x.begin(), ary, ary + ARRAYSZ(ary));          \
    } while (0)

#define ITEM_FIELDS(name, ...) INIT_VEC(item_fields[ITEM_ ## name], __VA_ARGS__)

static void _init_fields()
{
    ITEM_FIELDS(SCROLLS,    "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
                            "NumPiles", "PileQuant");
    ITEM_FIELDS(POTIONS,    "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
                            "NumPiles", "PileQuant");
    ITEM_FIELDS(FOOD,       "Num", "NumMin", "NumMax", "NumSD", "NumPiles",
                            "PileQuant", "TotalNormNutr", "TotalCarnNutr",
                            "TotalHerbNutr");
    ITEM_FIELDS(GOLD,       "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
                            "NumPiles", "PileQuant");
    ITEM_FIELDS(WANDS,      "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons", "WandCharges");
    ITEM_FIELDS(WEAPONS,    "OrdNum", "ArteNum", "AllNum", "AllNumMin",
                            "AllNumMax", "AllNumSD", "OrdEnch", "ArteEnch",
                            "AllEnch", "OrdNumCursed", "ArteNumCursed",
                            "AllNumCursed", "OrdNumBranded", "OrdNumHeldMons",
                            "ArteNumHeldMons", "AllNumHeldMons");
    ITEM_FIELDS(STAVES,     "Num", "NumMin", "NumMax", "NumSD", "NumCursed",
                            "NumHeldMons");
    ITEM_FIELDS(ARMOUR,     "OrdNum", "ArteNum", "AllNum", "AllNumMin",
                            "AllNumMax", "AllNumSD", "OrdEnch", "ArteEnch",
                            "AllEnch", "OrdNumCursed", "ArteNumCursed",
                            "AllNumCursed", "OrdNumBranded", "OrdNumHeldMons",
                            "ArteNumHeldMons", "AllNumHeldMons");
    ITEM_FIELDS(JEWELLERY,  "OrdNum", "ArteNum", "AllNum", "AllNumMin",
                            "AllNumMax", "AllNumSD", "OrdNumCursed",
                            "ArteNumCursed", "AllNumCursed", "OrdNumHeldMons",
                            "ArteNumHeldMons", "AllNumHeldMons", "OrdEnch",
                            "ArteEnch", "AllEnch");
    ITEM_FIELDS(RODS,       "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
                            "RodMana", "RodRecharge", "NumCursed");
    ITEM_FIELDS(MISSILES,   "Num", "NumMin", "NumMax", "NumSD", "NumHeldMons",
                            "NumBranded", "NumPiles", "PileQuant");
    ITEM_FIELDS(MISCELLANY, "Num", "NumMin", "NumMax", "NumSD", "MiscPlus");
    ITEM_FIELDS(DECKS,      "PlainNum", "OrnateNum", "LegendaryNum", "AllNum",
                            "AllNumMin", "AllNumMax", "AllNumSD",
                            "AllDeckCards");
    ITEM_FIELDS(BOOKS,      "Num", "NumMin", "NumMax", "NumSD");
    ITEM_FIELDS(ARTEBOOKS,  "Num", "NumMin", "NumMax", "NumSD");
    ITEM_FIELDS(MANUALS,    "Num", "NumMin", "NumMax", "NumSD");

    INIT_VEC(monster_fields, "Num", "NumMin", "NumMax", "NumSD", "MonsHD",
                             "MonsHP", "MonsXP", "TotalXP", "MonsNumChunks",
                             "TotalNutr");
}

static void _init_stats()
{
    map<branch_type, vector<level_id> >::const_iterator bi;
    for (bi = stat_branches.begin(); bi != stat_branches.end(); bi++)
    {
        for (unsigned int l = 0; l <= bi->second.size(); l++)
        {
            level_id lev;
            if (l == bi->second.size())
            {
                if (bi->first == NUM_BRANCHES)
                    continue;
                lev.branch = bi->first;
                lev.depth = -1;
            }
            else
                lev = (bi->second)[l];
            for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
            {
                item_base_type base_type = static_cast<item_base_type>(i);
                string min_field = _item_has_antiquity(base_type)
                    ? "AllNumMin" : "NumMin";
                string max_field = _item_has_antiquity(base_type)
                    ? "AllNumMax" : "NumMax";
                for (int  j = 0; j <= _item_max_sub_type(base_type); j++)
                {
                    for (unsigned int k = 0; k < item_fields[i].size(); k++)
                        item_recs[lev][i][j][item_fields[i][k]] = 0;
                    // For determining the NumSD, NumMin and NumMax fields.
                    item_recs[lev][i][j]["NumForIter"] = 0;
                    item_recs[lev][i][j][min_field] = INFINITY;
                    item_recs[lev][i][j][max_field] = -1;
                }
            }
            equip_brands[lev] = vector< vector< vector< vector<int> > > >();
            equip_brands[lev].push_back(vector< vector< vector<int> > >());
            for (int i = 0; i <= NUM_WEAPONS; i++)
            {
                equip_brands[lev][0].push_back(vector< vector<int> >());
                for (int j = 0; j < 3; j++)
                {
                    equip_brands[lev][0][i].push_back(vector<int>());
                    for (int k = 0; k < NUM_SPECIAL_WEAPONS; k++)
                        equip_brands[lev][0][i][j].push_back(0);
                }
            }

            equip_brands[lev].push_back(vector< vector< vector<int> > >());
            for (int i = 0; i <= NUM_ARMOURS; i++)
            {
                equip_brands[lev][1].push_back(vector< vector<int> >());
                for (int j = 0; j < 3; j++)
                {
                    equip_brands[lev][1][i].push_back(vector<int>());
                    for (int k = 0; k < NUM_SPECIAL_ARMOURS; k++)
                        equip_brands[lev][1][i][j].push_back(0);
                }
            }

            missile_brands[lev] = vector< vector<int> >();
            for (int i = 0; i <= NUM_MISSILES; i++)
            {
                missile_brands[lev].push_back(vector<int>());
                for (int j = 0; j < NUM_SPECIAL_ARMOURS; j++)
                    missile_brands[lev][i].push_back(0);
            }

            map<monster_type, int>::const_iterator mi;
            for (mi = valid_monsters.begin(); mi != valid_monsters.end();
                 mi++)
            {
                for (unsigned int i = 0; i < monster_fields.size(); i++)
                    monster_recs[lev][mi->second][monster_fields[i]] = 0;
                // For determining the NumMin and NumMax fields.
                monster_recs[lev][mi->second]["NumForIter"] = 0;
                monster_recs[lev][mi->second]["NumMin"] = INFINITY;
                monster_recs[lev][mi->second]["NumMax"] = -1;
            }
        }
    }
}

static void _record_item_stat(level_id &lev, item_type &item, string field,
                          double value)
{
    int class_sum = _item_max_sub_type(item.base_type);
    level_id br_lev(lev.branch, -1);

    item_recs[lev][item.base_type][item.sub_type][field] += value;
    item_recs[lev][item.base_type][class_sum][field] += value;
    item_recs[br_lev][item.base_type][item.sub_type][field] += value;
    item_recs[br_lev][item.base_type][class_sum][field] += value;
    item_recs[all_lev][item.base_type][item.sub_type][field] += value;
    item_recs[all_lev][item.base_type][class_sum][field] += value;
}

static void _record_brand(level_id &lev, item_type &item, int quantity,
                             bool is_arte, int brand)
{
    ASSERT(item.base_type == ITEM_WEAPONS || item.base_type == ITEM_ARMOUR
           || item.base_type == ITEM_MISSILES);
    int cst = _item_max_sub_type(item.base_type);
    int antiq = is_arte ? ANTIQ_ARTEFACT : ANTIQ_ORDINARY;
    bool is_equip = item.base_type == ITEM_WEAPONS
        || item.base_type == ITEM_ARMOUR;
    int bt = item.base_type == ITEM_WEAPONS ? 0 : 1;
    int st = item.sub_type;
    level_id br_lev(lev.branch, -1);

    if (is_equip)
    {
        equip_brands[lev][bt][st][antiq][brand] += quantity;
        equip_brands[lev][bt][st][ANTIQ_ALL][brand] += quantity;
        equip_brands[lev][bt][cst][antiq][brand] += quantity;
        equip_brands[lev][bt][cst][ANTIQ_ALL][brand] += quantity;

        equip_brands[br_lev][bt][st][antiq][brand] += quantity;
        equip_brands[br_lev][bt][st][ANTIQ_ALL][brand] += quantity;
        equip_brands[br_lev][bt][cst][antiq][brand] += quantity;
        equip_brands[br_lev][bt][cst][ANTIQ_ALL][brand] += quantity;

        equip_brands[all_lev][bt][st][antiq][brand] += quantity;
        equip_brands[all_lev][bt][st][ANTIQ_ALL][brand] += quantity;
        equip_brands[all_lev][bt][cst][antiq][brand] += quantity;
        equip_brands[all_lev][bt][cst][ANTIQ_ALL][brand] += quantity;
    }
    else
    {
        missile_brands[lev][st][brand] += quantity;
        missile_brands[lev][cst][brand] += quantity;
        missile_brands[br_lev][st][brand] += quantity;
        missile_brands[br_lev][cst][brand] += quantity;
        missile_brands[all_lev][st][brand] += quantity;
        missile_brands[all_lev][cst][brand] += quantity;
    }
}

static bool _item_track_piles(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_GOLD:
    case ITEM_POTIONS:
    case ITEM_SCROLLS:
    case ITEM_FOOD:
    case ITEM_MISSILES:
        return true;
    default:
        return false;
    }
}

static bool _item_track_curse(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_WEAPONS:
    case ITEM_STAVES:
    case ITEM_ARMOUR:
    case ITEM_JEWELLERY:
    case ITEM_RODS:
        return true;
    default:
        return false;
    }
}

static bool _item_track_plus(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_WEAPONS:
    case ITEM_ARMOUR:
    case ITEM_JEWELLERY:
    case ITEM_WANDS:
    case ITEM_MISCELLANY:
    case ITEM_DECKS:
        return true;
    default:
        return false;
    }
}

static bool _item_track_brand(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_WEAPONS:
    case ITEM_ARMOUR:
    case ITEM_MISSILES:
        return true;
    default:
        return false;
    }
}

static bool _item_track_monster(item_base_type base_type)
{
    switch (base_type)
    {
    case ITEM_GOLD:
    case ITEM_POTIONS:
    case ITEM_SCROLLS:
    case ITEM_WANDS:
    case ITEM_WEAPONS:
    case ITEM_STAVES:
    case ITEM_ARMOUR:
    case ITEM_RODS:
    case ITEM_MISSILES:
    case ITEM_JEWELLERY:
        return true;
    default:
        return false;
    }
}

void objstat_record_item(item_def &item)
{
    level_id cur_lev = level_id::current();
    item_type itype(item);
    bool is_arte = is_artefact(item);
    int brand = -1;
    bool has_antiq = _item_has_antiquity(itype.base_type);
    string all_num_f = has_antiq ? "AllNum" : "Num";
    string antiq_num_f = is_arte ? "ArteNum" : "OrdNum";
    string all_cursed_f = has_antiq ? "AllNumCursed" : "NumCursed";
    string antiq_cursed_f = is_arte ? "ArteNumCursed" : "OrdNumCursed";
    string all_num_hm_f = has_antiq ? "AllNumHeldMons" : "NumHeldMons";
    string antiq_num_hm_f = is_arte ? "ArteNumHeldMons" : "OrdNumHeldMons";
    string all_plus_f = "AllEnch";
    string antiq_plus_f = is_arte ? "ArteEnch" : "OrdEnch";
    string num_brand_f = has_antiq ? "OrdNumBranded" : "NumBranded";

    // Just in case, don't count mimics as items; these are converted
    // explicitely in mg_do_build_level().
    if (item.flags & ISFLAG_MIMIC || itype.base_type == ITEM_IGNORE)
        return;

    // Some type-specific calculations.
    switch (itype.base_type)
    {
    case ITEM_MISSILES:
        brand = get_ammo_brand(item);
        break;
    case ITEM_FOOD:
        _record_item_stat(cur_lev, itype, "TotalNormNutr", food_value(item));
        // Set these dietary mutations so we can get accurate nutrition.
        you.mutation[MUT_CARNIVOROUS] = 3;
        _record_item_stat(cur_lev, itype, "TotalCarnNutr",
                             food_value(item) * food_is_meaty(item.sub_type));
        you.mutation[MUT_CARNIVOROUS] = 0;
        you.mutation[MUT_HERBIVOROUS] = 3;
        _record_item_stat(cur_lev, itype, "TotalHerbNutr",
                             food_value(item) * food_is_veggie(item.sub_type));
        you.mutation[MUT_HERBIVOROUS] = 0;
        break;
    case ITEM_WEAPONS:
        brand = get_weapon_brand(item);
        break;
    case ITEM_ARMOUR:
        brand = get_armour_ego_type(item);
        break;
    case ITEM_WANDS:
        all_plus_f = "WandCharges";
        break;
    case ITEM_RODS:
        _record_item_stat(cur_lev, itype, "RodMana",
                             item.charge_cap / ROD_CHARGE_MULT);
        _record_item_stat(cur_lev, itype, "RodRecharge", item.special);
        break;
    case ITEM_MISCELLANY:
        all_plus_f = "MiscPlus";
        break;
    case ITEM_DECKS:
        switch (item.deck_rarity)
        {
        case DECK_RARITY_COMMON:
            _record_item_stat(cur_lev, itype, "PlainNum", 1);
            break;
        case DECK_RARITY_RARE:
            _record_item_stat(cur_lev, itype, "OrnateNum", 1);
            break;
        case DECK_RARITY_LEGENDARY:
            _record_item_stat(cur_lev, itype, "LegendaryNum", 1);
            break;
        default:
            break;
        }
        all_plus_f = "AllDeckCards";
        break;
    default:
        break;
    }
    if (_item_track_piles(itype.base_type))
        _record_item_stat(cur_lev, itype, "NumPiles", 1);
    if (_item_track_curse(itype.base_type) && item.cursed())
    {
        if (has_antiq)
            _record_item_stat(cur_lev, itype, antiq_cursed_f, 1);
        _record_item_stat(cur_lev, itype, all_cursed_f, 1);
    }
    if (_item_track_plus(itype.base_type))
    {
        if (has_antiq)
            _record_item_stat(cur_lev, itype, antiq_plus_f, item.plus);
        _record_item_stat(cur_lev, itype, all_plus_f, item.plus);
    }

    if (_item_track_brand(itype.base_type))
    {
        _record_brand(cur_lev, itype, item.quantity, is_arte, brand);
        if (!is_arte && brand > 0)
            _record_item_stat(cur_lev, itype, num_brand_f, item.quantity);
    }
    if (_item_track_monster(itype.base_type) && item.holding_monster())
    {
        if (has_antiq)
            _record_item_stat(cur_lev, itype, antiq_num_hm_f, item.quantity);
        _record_item_stat(cur_lev, itype, all_num_hm_f, item.quantity);
    }
    if (has_antiq)
        _record_item_stat(cur_lev, itype, antiq_num_f, item.quantity);
    _record_item_stat(cur_lev, itype, all_num_f, item.quantity);
    _record_item_stat(cur_lev, itype, "NumForIter", item.quantity);
}

static void _record_monster_stat(level_id &lev, int mons_ind, string field,
                                 double value)
{
    level_id br_lev(lev.branch, -1);
    int sum_ind = valid_monsters[NUM_MONSTERS];

    monster_recs[lev][mons_ind][field] += value;
    monster_recs[lev][sum_ind][field] += value;
    monster_recs[br_lev][mons_ind][field] += value;
    monster_recs[br_lev][sum_ind][field] += value;
    monster_recs[all_lev][mons_ind][field] += value;
    monster_recs[all_lev][sum_ind][field] += value;
}

void objstat_record_monster(monster *mons)
{
    if (!valid_monsters.count(mons->type))
        return;

    int mons_ind = valid_monsters[mons->type];
    corpse_effect_type chunk_effect = mons_corpse_effect(mons->type);
    bool is_clean = chunk_effect == CE_CLEAN || chunk_effect == CE_POISONOUS;
    level_id lev = level_id::current();

    _record_monster_stat(lev, mons_ind, "Num", 1);
    _record_monster_stat(lev, mons_ind, "NumForIter", 1);
    _record_monster_stat(lev, mons_ind, "MonsXP", exper_value(mons));
    _record_monster_stat(lev, mons_ind, "TotalXP", exper_value(mons));
    _record_monster_stat(lev, mons_ind, "MonsHP", mons->max_hit_points);
    _record_monster_stat(lev, mons_ind, "MonsHD", mons->get_experience_level());
    // Record chunks/nutrition if monster leaves a corpse.
    if (chunk_effect != CE_NOCORPSE && mons_weight(mons->type))
    {
        // copied from turn_corpse_into_chunks()
        double chunks = (1 + stepdown_value(get_max_corpse_chunks(mons->type),
                                            4, 4, 12, 12)) / 2.0;
        _record_monster_stat(lev, mons_ind, "MonsNumChunks", chunks);
        if (is_clean)
        {
            _record_monster_stat(lev, mons_ind, "TotalNutr",
                                    chunks * CHUNK_BASE_NUTRITION);
        }
    }
}

void objstat_iteration_stats()
{
    map<branch_type, vector<level_id> >::const_iterator bi;
    for (bi = stat_branches.begin(); bi != stat_branches.end(); bi++)
    {
        for (unsigned int l = 0; l <= bi->second.size(); l++)
        {
            level_id lev;
            if (l == bi->second.size())
            {
                if (bi->first == NUM_BRANCHES)
                    continue;
                lev.branch = bi->first;
                lev.depth = -1;
            }
            else
                lev = (bi->second)[l];

            for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
            {
                item_base_type base_type = static_cast<item_base_type>(i);
                for (int  j = 0; j <= _item_max_sub_type(base_type); j++)
                {
                    bool use_all = _item_has_antiquity(base_type)
                        || base_type == ITEM_DECKS;
                    string min_f = use_all ? "AllNumMin" : "NumMin";
                    string max_f = use_all ? "AllNumMax" : "NumMax";
                    string sd_f = use_all ? "AllNumSD" : "NumSD";
                    map<string, double> &stats = item_recs[lev][i][j];
                    if (stats["NumForIter"] > stats[max_f])
                        stats[max_f] = stats["NumForIter"];
                    if (stats["NumForIter"] < stats[min_f])
                        stats[min_f] = stats["NumForIter"];
                    stats[sd_f] += stats["NumForIter"] * stats["NumForIter"];
                    stats["NumForIter"] = 0;
                }
            }
            map<monster_type, int>::const_iterator mi;
            for (mi = valid_monsters.begin(); mi != valid_monsters.end();
                 mi++)
            {
                map<string, double> &stats = monster_recs[lev][mi->second];
                if (stats["NumForIter"] > stats["NumMax"])
                    stats["NumMax"] = stats["NumForIter"];
                if (stats["NumForIter"] < stats["NumMin"])
                    stats["NumMin"] = stats["NumForIter"];
                stats["NumSD"] += stats["NumForIter"] * stats["NumForIter"];
                stats["NumForIter"] = 0;
            }
        }
    }
}

static void _write_level_headers(branch_type br, int num_fields)
{
    unsigned int level_count = 0;
    vector<level_id> &levels = stat_branches[br];
    vector<level_id>::const_iterator li;

    fprintf(stat_outf, "Place");
    for (li = levels.begin(); li != levels.end(); li++)
    {
        if (br == NUM_BRANCHES)
            fprintf(stat_outf, "\tAllLevels");
        else
            fprintf(stat_outf, "\t%s", li->describe().c_str());

        for (int i = 0; i < num_fields - 1; i++)
            fprintf(stat_outf, "\t");
        if (++level_count == levels.size() && level_count > 1)
        {
            fprintf(stat_outf, "\t%s", branches[li->branch].abbrevname);
            for (int i = 0; i < num_fields - 1; i++)
                fprintf(stat_outf, "\t");
        }
    }
    fprintf(stat_outf, "\n");
}

static void _write_stat_headers(branch_type br, vector<string> fields)
{
    unsigned int level_count = 0;
    vector<level_id> &levels = stat_branches[br];
    vector<level_id>::const_iterator li;

    fprintf(stat_outf, "Property");
    for (li = levels.begin(); li != levels.end(); li++)
    {
        for (unsigned int i = 0; i < fields.size(); i++)
            fprintf(stat_outf, "\t%s", fields[i].c_str());

        if (++level_count == levels.size() && level_count > 1)
        {
            for (unsigned int i = 0; i < fields.size(); i++)
                fprintf(stat_outf, "\t%s", fields[i].c_str());
        }
    }
    fprintf(stat_outf, "\n");
}

static void _write_stat(map<string, double> &stats, string field)
{
    ostringstream output;
    double value = 0;

    output.precision(STAT_PRECISION);
    output.setf(ios_base::fixed);
    if (field == "PileQuant")
        value = stats["Num"] / stats["NumPiles"];
    else if (field == "WandCharges"
             || field == "RodMana"
             || field == "RodRecharge"
             || field == "MiscPlus"
             || field == "MonsHD"
             || field == "MonsHP"
             || field == "MonsXP"
             || field == "MonsNumChunks")
    {
        value = stats[field] / stats["Num"];
    }
    else if (field == "AllEnch" || field == "AllDeckCards")
        value = stats[field] / stats["AllNum"];
    else if (field == "ArteEnch")
        value = stats[field] / stats["ArteNum"];
    else if (field == "OrdEnch")
        value = stats[field] / stats["OrdNum"];
    else if (field == "NumSD" || field == "AllNumSD")
    {
        string num_f = field == "NumSD" ? "Num" : "AllNum";
        if (SysEnv.map_gen_iters == 1)
            value = 0;
        else
        {
            double mean = stats[num_f] / SysEnv.map_gen_iters;
            value = sqrt((SysEnv.map_gen_iters / (SysEnv.map_gen_iters - 1.0))
                         * (stats[field] / SysEnv.map_gen_iters - mean * mean));
        }
    }
    else if (field == "NumMin" || field == "AllNumMin" || field == "NumMax"
             || field == "AllNumMax")
    {
        value = stats[field];
    }
    else
        value = stats[field] / SysEnv.map_gen_iters;
    output << "\t" << value;
    fprintf(stat_outf, "%s", output.str().c_str());
}

static string _brand_name(item_type &item, int brand)
 {
     string brand_name = "";
     item_def dummy_item = _dummy_item(item);

     if (!brand)
            brand_name = "none";
     else
     {
         dummy_item.special = brand;
         switch (item.base_type)
            {
            case ITEM_WEAPONS:
                brand_name = weapon_brand_name(dummy_item, true);
                break;
            case ITEM_ARMOUR:
                brand_name = armour_ego_name(dummy_item, true);
                break;
            case ITEM_MISSILES:
                brand_name = missile_brand_name(dummy_item, MBN_TERSE);
                break;
            default:
                break;
            }
     }
     return brand_name;
 }

static void _write_brand_stats(vector<int> &brand_stats, item_type &item)
{
    ASSERT(item.base_type == ITEM_WEAPONS || item.base_type == ITEM_ARMOUR
           || item.base_type == ITEM_MISSILES);
    item_def dummy_item = _dummy_item(item);
    unsigned int num_brands = brand_stats.size();
    bool first_brand = true;
    ostringstream brand_summary;

    brand_summary.setf(ios_base::fixed);
    brand_summary.precision(STAT_PRECISION);
    for (unsigned int i = 0; i < num_brands; i++)
    {
        string brand_name = "";
        double value = (double) brand_stats[i] / SysEnv.map_gen_iters;
        if (brand_stats[i] == 0)
            continue;

        if (first_brand)
            first_brand = false;
        else
            brand_summary << ",";

        brand_name = _brand_name(item, i);
        brand_summary << brand_name.c_str() << "(" << value << ")";
    }
    fprintf(stat_outf, "\t%s", brand_summary.str().c_str());
}

static string _item_name(item_type &item)
{

    string name = "";

    if (item.sub_type == _item_max_sub_type(item.base_type))
        name = "All " + _item_class_name(item.base_type);
    else if (item.base_type == ITEM_MANUALS)
        name = "Manual";
    else if (item.base_type == ITEM_ARTEBOOKS)
    {
        int orig_type = _item_orig_sub_type(item);
        if (orig_type == BOOK_RANDART_LEVEL)
            name = "Level Artefact Book";
        else
            name = "Theme Artefact Book";
    }
    else
    {
        item_def dummy_item = _dummy_item(item);
        name = dummy_item.name(DESC_DBNAME, true, true);
    }
    return name;
}

static void _write_item_stats(branch_type br, item_type &item)
{
    bool is_brand_equip = item.base_type == ITEM_WEAPONS
        || item.base_type == ITEM_ARMOUR;
    int equip_ind = is_brand_equip
        ? (item.base_type == ITEM_WEAPONS ? 0 : 1) : -1;
    unsigned int level_count = 0;
    vector <string> fields = item_fields[item.base_type];
    vector<level_id>::const_iterator li;

    fprintf(stat_outf, "%s", _item_name(item).c_str());
    for (li = stat_branches[br].begin(); li != stat_branches[br].end(); li++)
    {
        map <string, double> &item_stats =
            item_recs[*li][item.base_type][item.sub_type];
        for (unsigned int i = 0; i < fields.size(); i++)
            _write_stat(item_stats, fields[i]);

        if (is_brand_equip)
        {
            vector< vector<int> > &brand_stats =
                equip_brands[*li][equip_ind][item.sub_type];
            for (int j = 0; j < 3; j++)
                _write_brand_stats(brand_stats[j], item);
        }
        else if (item.base_type == ITEM_MISSILES)
            _write_brand_stats(missile_brands[*li][item.sub_type], item);

        if (++level_count == stat_branches[li->branch].size() && level_count > 1)
        {
            level_id br_lev(li->branch, -1);
            map <string, double> &branch_stats =
                item_recs[br_lev][item.base_type][item.sub_type];

            for (unsigned int i = 0; i < fields.size(); i++)
                _write_stat(branch_stats, fields[i]);

            if (is_brand_equip)
            {
                vector< vector<int> > &branch_brand_stats =
                    equip_brands[br_lev][equip_ind][item.sub_type];
                for (int j = 0; j < 3; j++)
                    _write_brand_stats(branch_brand_stats[j], item);
            }
            else if (item.base_type == ITEM_MISSILES)
            {
                _write_brand_stats(missile_brands[br_lev][item.sub_type],
                                      item);
            }
        }
    }
    fprintf(stat_outf, "\n");
}

static void _write_monster_stats(branch_type br, monster_type mons_type,
                                    int mons_ind)
{
    unsigned int level_count = 0;
    vector <string> fields = monster_fields;
    vector<level_id>::const_iterator li;

    if (mons_ind == valid_monsters[NUM_MONSTERS])
        fprintf(stat_outf, "All Monsters");
    else
        fprintf(stat_outf, "%s", mons_type_name(mons_type, DESC_PLAIN).c_str());
    for (li = stat_branches[br].begin(); li != stat_branches[br].end(); li++)
    {
        for (unsigned int i = 0; i < fields.size(); i++)
            _write_stat(monster_recs[*li][mons_ind], fields[i]);

        if (++level_count == stat_branches[li->branch].size() && level_count > 1)
        {
            level_id br_lev(li->branch, -1);
            for (unsigned int i = 0; i < fields.size(); i++)
                _write_stat(monster_recs[br_lev][mons_ind], fields[i]);
        }
    }
    fprintf(stat_outf, "\n");
}

static void _write_branch_stats(branch_type br)
{
    fprintf(stat_outf, "\n\nItem Generation Stats:");
    for (int i = 0; i < NUM_ITEM_BASE_TYPES; i++)
    {
        item_base_type base_type = static_cast<item_base_type>(i);
        int num_types = _item_max_sub_type(base_type);
        vector<string> fields = item_fields[base_type];

        fprintf(stat_outf, "\n%s\n", _item_class_name(base_type).c_str());
        if (base_type == ITEM_WEAPONS || base_type == ITEM_ARMOUR)
        {
            for (int j = 0; j < 3; j++)
                fields.push_back(equip_brand_fields[j]);
        }
        else if (base_type == ITEM_MISSILES)
            fields.push_back(missile_brand_field);

        _write_level_headers(br, fields.size());
        _write_stat_headers(br, fields);
        for (int j = 0; j <= num_types; j++)
        {
            item_type item(base_type, j);
            _write_item_stats(br, item);
        }
    }
    fprintf(stat_outf, "\n\nMonster Generation Stats:\n");
    _write_level_headers(br, monster_fields.size());
    _write_stat_headers(br, monster_fields);
    map<monster_type, int>::const_iterator mi;
    for (mi = valid_monsters.begin(); mi != valid_monsters.end(); mi++)
        _write_monster_stats(br, mi->first, mi->second);
}

static void _write_object_stats()
{
    string all_desc = "";
    map<branch_type, vector<level_id> >::const_iterator bi;

    for (bi = stat_branches.begin(); bi != stat_branches.end(); bi++)
    {
        string branch_name;
        if (bi->first == NUM_BRANCHES)
        {
            if (num_branches == 1)
                continue;
            branch_name = "AllLevels";
            if (SysEnv.map_gen_range.get())
                all_desc = SysEnv.map_gen_range.get()->describe();
            else
                all_desc = "All Branches";
            all_desc = "Levels included in AllLevels: " + all_desc + "\n";
        }
        else
            branch_name = branches[bi->first].abbrevname;
        ostringstream out_file;
        out_file << stat_out_prefix << branch_name << stat_out_ext;
        stat_outf = fopen(out_file.str().c_str(), "w");
        if (!stat_outf)
        {
            fprintf(stderr, "Unable to open objstat output file: %s\n"
                    "Error: %s", out_file.str().c_str(), strerror(errno));
            end(1);
        }
        fprintf(stat_outf, "Object Generation Stats\n"
                "Number of iterations: %d\n"
                "Number of branches: %d\n"
                "%s"
                "Number of levels: %d\n"
                "Version: %s\n", SysEnv.map_gen_iters, num_branches,
                all_desc.c_str(), num_levels, Version::Long);
        _write_branch_stats(bi->first);
        fclose(stat_outf);
        fprintf(stdout, "Wrote statistics for branch %s to %s.\n",
                branch_name.c_str(), out_file.str().c_str());
    }
}

void objstat_generate_stats()
{
    // Warn assertions about possible oddities like the artefact list being
    // cleared.
    you.wizard = true;
    // Let "acquire foo" have skill aptitudes to work with.
    you.species = SP_HUMAN;

    initialise_item_descriptions();
    initialise_branch_depths();
    // We have to run map preludes ourselves.
    run_map_global_preludes();
    run_map_local_preludes();

    // Populate a vector of the levels ids we've made
    for (branch_iterator it; it; ++it)
    {
        if (brdepth[it->id] == -1)
            continue;

        const branch_type br = it->id;
#if TAG_MAJOR_VERSION == 34
        // Don't want to include Forest since it doesn't generate
        if (br == BRANCH_FOREST)
            continue;
#endif
        vector<level_id> levels;
        for (int dep = 1; dep <= brdepth[br]; ++dep)
        {
            const level_id lid(br, dep);
            if (SysEnv.map_gen_range.get()
                && !SysEnv.map_gen_range->is_usable_in(lid))
            {
                continue;
            }
            levels.push_back(lid);
            ++num_levels;
        }
        if (levels.size())
        {
            stat_branches[br] = levels;
            ++num_branches;
        }
    }
    // This represents the AllLevels summary.
    stat_branches[NUM_BRANCHES] = vector<level_id>();
    stat_branches[NUM_BRANCHES].push_back(level_id(NUM_BRANCHES, -1));
    fprintf(stdout, "Generating object statistics for %d iteration(s) of %d "
            "level(s) over %d branch(es).\n", SysEnv.map_gen_iters,
            num_levels, num_branches);
    _init_fields();
    _init_foods();
    _init_monsters();
    _init_stats();
    if (mapstat_build_levels())
        _write_object_stats();
}
#endif // DEBUG_DIAGNOSTICS
