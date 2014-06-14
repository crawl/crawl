/**
 * @file
 * @brief Debugging code to scan the list of items and monsters.
**/

#include "AppHdr.h"
#include "dbg-scan.h"

#include <sstream>

#include "artefact.h"
#include "branch.h"
#include "chardump.h"
#include "coord.h"
#include "coordit.h"
#include "dbg-maps.h"
#include "dbg-util.h"
#include "decks.h"
#include "dungeon.h"
#include "env.h"
#include "godabil.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "initfile.h"
#include "invent.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "ng-init.h"
#include "shopping.h"
#include "state.h"
#include "stuff.h"
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
        else if ((mitm[i].base_type == OBJ_WEAPONS
                    && (abs(mitm[i].plus) > 30
                        || abs(mitm[i].plus2) > 30
                        || !is_artefact(mitm[i])
                           && mitm[i].special >= NUM_SPECIAL_WEAPONS))

                 || (mitm[i].base_type == OBJ_ARMOUR
                     && (abs(mitm[i].plus) > 30
                         || !is_artefact(mitm[i])
                            && mitm[i].special >= NUM_SPECIAL_ARMOURS)))
        {
            _dump_item(name, i, mitm[i], "Bad plus or special value:");
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
    if (you.where_are_you == BRANCH_DEPTHS)
    {
        if (you.depth == 3)
            portal = DNGN_ENTER_PANDEMONIUM;
        else if (you.depth == 4)
            portal = DNGN_ENTER_ABYSS;
        else if (you.depth == 2)
            portal = DNGN_ENTER_HELL;
    }

    dungeon_feature_type exit = DNGN_UNSEEN;
    if (you.depth == 1 && you.where_are_you != root_branch)
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

static FILE *og_outf;
const static char *og_out_file = "objects.txt";
#define OG_STAT_PRECISION 3
static char og_stat_fmt[10] = "";

enum og_class_type {
    OG_FOOD,
    OG_GOLD,
    OG_SCROLLS,
    OG_POTIONS,
    OG_WANDS,
    OG_WEAPONS,
    OG_MISSILES,
    OG_STAVES,
    OG_ARMOUR,
    OG_JEWELLERY,
    OG_MISCELLANY,
    OG_RODS,
    OG_DECKS,
    OG_BOOKS,
    OG_ARTEBOOKS,
    OG_MANUALS,
    NUM_OG_CLASSES,
    OG_UNUSED = 100,
};

enum og_summary_level {
    SUMMARY_ORD,
    SUMMARY_ARTE,
    SUMMARY_ALL,
};

static vector<level_id> og_levels;
static map<branch_type, int> og_branches;

// og_items[level_id][item.base_type][item.sub_type][field]
static map<level_id, FixedVector<map<int, map<string, double> >, NUM_OG_CLASSES> > og_items;

// og_weapon_brands[level_id][item.sub_type][type_ind][brand];
// type_ind is 0 for ordinary, 1 for artefact, or 2 for all
static map<level_id, vector< vector< vector< int> > > > og_weapon_brands;
static map<level_id, vector< vector< vector< int> > > > og_armour_brands;
static map<level_id, vector< vector< int> > > og_missile_brands;

static FixedVector< vector<string>, NUM_OG_CLASSES> og_item_fields;

static const char* og_equip_brand_fields[] = {"OrdBrandNums",
                                              "ArteBrandNums",
                                              "AllBrandNums"};
static const char* og_missile_brand_field = "BrandNums";


static map<int, int> og_valid_foods;

static vector<string> og_monster_fields;
static map<monster_type, int> og_valid_monsters;
static map<level_id, map<int, map <string, double> > > og_monsters;

static bool _og_is_valid_food_type(int sub_type)
{
    return sub_type != FOOD_CHUNK && food_type_name(sub_type) != "buggy";
}

static void _og_init_foods()
{
    int num_foods = 0;
    for (int i = 0; i < NUM_FOODS; i++)
    {
        if (_og_is_valid_food_type(i))
            og_valid_foods[i] = num_foods++;
    }
}

static void _og_init_monsters()
{
    int num_mons = 0;
    for (int i = 0; i < NUM_MONSTERS; i++)
    {
        monster_type mc = static_cast<monster_type>(i);
        if (!mons_class_flag(mc, M_NO_EXP_GAIN)
            && !mons_class_flag(mc, M_CANT_SPAWN))
        {
            og_valid_monsters[mc] = num_mons++;
        }
    }
    // For the all-monster summary
    og_valid_monsters[NUM_MONSTERS] = num_mons;
}

static og_class_type _og_base_type(item_def &item)
{
    og_class_type type;
    switch (item.base_type)
    {
    case OBJ_MISCELLANY:
        if (item.sub_type >= MISC_FIRST_DECK && item.sub_type <= MISC_LAST_DECK)
            type = OG_DECKS;
        else
            type = OG_MISCELLANY;
        break;
    case OBJ_BOOKS:
        if (item.sub_type == BOOK_MANUAL)
            type = OG_MANUALS;
        else if (is_artefact(item))
            type = OG_ARTEBOOKS;
        else
            type = OG_BOOKS;
        break;
    case OBJ_FOOD:
        if (og_valid_foods.count(item.sub_type))
            type = OG_FOOD;
        else
            type = OG_UNUSED;
        break;
    case OBJ_GOLD:
        type = OG_GOLD;
        break;
    case OBJ_SCROLLS:
        type = OG_SCROLLS;
        break;
    case OBJ_POTIONS:
        type = OG_POTIONS;
        break;
    case OBJ_WANDS:
        type = OG_WANDS;
        break;
    case OBJ_WEAPONS:
        type = OG_WEAPONS;
        break;
    case OBJ_MISSILES:
        type = OG_MISSILES;
        break;
    case OBJ_STAVES:
        type = OG_STAVES;
        break;
    case OBJ_ARMOUR:
        type = OG_ARMOUR;
        break;
    case OBJ_JEWELLERY:
        type = OG_JEWELLERY;
        break;
    case OBJ_RODS:
        type = OG_RODS;
        break;
    default:
        type = OG_UNUSED;
        break;
    }
    return type;
}

static object_class_type _og_orig_base_type(og_class_type item_type)
{
    object_class_type type;
    switch (item_type)
    {
    case OG_FOOD:
        type = OBJ_FOOD;
        break;
    case OG_GOLD:
        type = OBJ_GOLD;
        break;
    case OG_SCROLLS:
        type = OBJ_SCROLLS;
        break;
    case OG_POTIONS:
        type = OBJ_POTIONS;
        break;
    case OG_WANDS:
        type = OBJ_WANDS;
        break;
    case OG_WEAPONS:
        type = OBJ_WEAPONS;
        break;
    case OG_MISSILES:
        type = OBJ_MISSILES;
        break;
    case OG_STAVES:
        type = OBJ_STAVES;
        break;
    case OG_ARMOUR:
        type = OBJ_ARMOUR;
        break;
    case OG_JEWELLERY:
        type = OBJ_JEWELLERY;
        break;
    case OG_RODS:
        type = OBJ_RODS;
        break;
    case OG_DECKS:
    case OG_MISCELLANY:
        type = OBJ_MISCELLANY;
        break;
    case OG_ARTEBOOKS:
    case OG_MANUALS:
    case OG_BOOKS:
        type = OBJ_BOOKS;
        break;
    default:
        type = OBJ_UNASSIGNED;
        break;
    }
    return type;
}

// Get the actual food subtype
static int _og_orig_food_subtype(int sub_type)
{
    map<int, int>::const_iterator mi;
    for (mi = og_valid_foods.begin(); mi != og_valid_foods.end(); mi++)
    {
        if (mi->second == sub_type)
            return mi->first;
    }
    die("Invalid food subtype");
    return 0;
}

static int _og_orig_sub_type(og_class_type item_type, int sub_type)
{
    int type;
    if (item_type == OG_DECKS)
        type = sub_type + MISC_FIRST_DECK;
    else if (item_type == OG_MISCELLANY && sub_type >= MISC_FIRST_DECK)
        type = sub_type + MISC_LAST_DECK - MISC_FIRST_DECK + 1;
    else if (item_type == OG_FOOD)
        type = _og_orig_food_subtype(sub_type);
    else if (item_type == OG_ARTEBOOKS)
        type = sub_type + BOOK_RANDART_LEVEL;
    else if (item_type == OG_MANUALS)
        type = BOOK_MANUAL;
    else
        type = sub_type;
    return type;
}

static int _og_get_max_subtype(og_class_type item_type)
{
    int num = 0;
    switch (item_type)
    {
    case OG_FOOD:
        num = og_valid_foods.size();
        break;
    case OG_MISCELLANY:
        // Decks not counted here.
        num = NUM_MISCELLANY - (MISC_LAST_DECK - MISC_FIRST_DECK + 1);
        break;
    case OG_DECKS:
        num = MISC_LAST_DECK - MISC_FIRST_DECK + 1;
        break;
    case OG_BOOKS:
        num = MAX_RARE_BOOK;
        break;
    case OG_ARTEBOOKS:
        num = 2;
        break;
    case OG_MANUALS:
        num = 1;
        break;
    default:
        num = get_max_subtype(_og_orig_base_type(item_type));
        break;
    }
    return num;
}

static item_def _og_dummy_item(og_class_type base_type, int sub_type)
{
    item_def item;
    item.base_type = _og_orig_base_type(base_type);
    item.sub_type = _og_orig_sub_type(base_type, sub_type);
    // Deck name is reported as buggy if this is not done.
    if (base_type == OG_DECKS)
    {
        item.plus = 1;
        item.special  = DECK_RARITY_COMMON;
        init_deck(item);
    }
    return item;
}

static string _og_class_name(og_class_type item_type)
{
    string name;
    switch (item_type)
    {
    case OG_DECKS:
        name = "Decks";
        break;
    case OG_ARTEBOOKS:
        name = "Artefact Spellbooks";
        break;
    case OG_MANUALS:
        name = "Manuals";
        break;
    default:
        name = item_class_name(_og_orig_base_type(item_type));
    }
    return name;
}

class og_item
{
public:
    og_item(item_def &item);
    og_class_type base_type;
    int sub_type;
};

og_item::og_item(item_def &item)
{
    base_type = _og_base_type(item);
    if (base_type == OG_DECKS)
        sub_type = item.sub_type - MISC_FIRST_DECK;
    else if (base_type == OG_MISCELLANY && item.sub_type > MISC_LAST_DECK)
        sub_type = item.sub_type - (MISC_LAST_DECK - MISC_FIRST_DECK + 1);
    else if (base_type == OG_FOOD)
        sub_type = og_valid_foods[item.sub_type];
    else if (base_type == OG_ARTEBOOKS)
        sub_type = item.sub_type - BOOK_RANDART_LEVEL;
    else if (base_type == OG_MANUALS)
        sub_type = 0;
    else
        sub_type = item.sub_type;
}

static void _og_init_fields()
{
#define OG_SET_FIELDS(x, name, ...)                                 \
    string name[] = { __VA_ARGS__ };                                \
    x.insert(x.begin(), name, name + sizeof(name) / sizeof(string))

    OG_SET_FIELDS(og_item_fields[OG_SCROLLS], scrolls,
                  "Num", "NumPiles", "PileQuant");
    OG_SET_FIELDS(og_item_fields[OG_POTIONS], potions,
                  "Num", "NumPiles", "PileQuant");
    OG_SET_FIELDS(og_item_fields[OG_FOOD], food,
                  "Num", "NumPiles", "PileQuant", "TotalNormNutr",
                  "TotalCarnNutr", "TotalHerbNutr");
    OG_SET_FIELDS(og_item_fields[OG_GOLD], gold,
                  "Num", "NumPiles", "PileQuant");
    OG_SET_FIELDS(og_item_fields[OG_WANDS], wands,
                  "Num", "WandCharges");
    OG_SET_FIELDS(og_item_fields[OG_WEAPONS], weapons,
                  "OrdNum", "ArteNum", "AllNum", "OrdEnch",
                  "ArteEnch", "AllEnch", "OrdNumCursed",
                  "ArteNumCursed", "AllNumCursed", "OrdNumBranded");
    OG_SET_FIELDS(og_item_fields[OG_STAVES], staves,
                  "Num", "NumCursed");
    OG_SET_FIELDS(og_item_fields[OG_ARMOUR], armour,
                  "OrdNum", "ArteNum", "AllNum", "OrdEnch",
                  "ArteEnch", "AllEnch", "OrdNumCursed",
                  "ArteNumCursed", "AllNumCursed",
                  "OrdNumBranded");
    OG_SET_FIELDS(og_item_fields[OG_JEWELLERY], jewellery,
                  "OrdNum", "ArteNum", "AllNum", "OrdNumCursed",
                  "ArteNumCursed", "AllNumCursed", "OrdEnch",
                  "ArteEnch", "AllEnch");
    OG_SET_FIELDS(og_item_fields[OG_RODS], rods,
                  "Num", "RodRecharge", "RodMana", "NumCursed");
    OG_SET_FIELDS(og_item_fields[OG_MISSILES], missiles,
                  "Num", "NumBranded", "NumPiles", "PileQuant");
    OG_SET_FIELDS(og_item_fields[OG_MISCELLANY], misc,
                  "Num", "MiscPlus", "MiscPlus2");
    OG_SET_FIELDS(og_item_fields[OG_DECKS], decks,
                  "PlainNum", "OrnateNum", "LegendaryNum",
                  "AllNum", "AllDeckCards");
    OG_SET_FIELDS(og_item_fields[OG_BOOKS], books, "Num");
    OG_SET_FIELDS(og_item_fields[OG_ARTEBOOKS], artebooks, "Num");
    OG_SET_FIELDS(og_item_fields[OG_MANUALS], manuals, "Num");
    OG_SET_FIELDS(og_monster_fields, monsters, "Num", "MonsHD", "MonsHP",
                  "MonsXP", "MonsNumChunks", "TotalXP", "TotalNormNutr",
                  "TotalGourmNutr");
}


static void _og_init_stats()
{
    vector<level_id>::const_iterator li;
    for (li = og_levels.begin(); li != og_levels.end(); li++)
    {
        for (int i = 0; i < NUM_OG_CLASSES; i++)
        {
            og_class_type item_type = static_cast<og_class_type>(i);
            for (int  j = 0; j <= _og_get_max_subtype(item_type); j++)
            {
                for (unsigned int k = 0; k < og_item_fields[i].size(); k++)
                    og_items[*li][i][j][og_item_fields[i][k]] = 0;
            }
        }
        og_weapon_brands[*li] = vector< vector< vector<int> > >();
        for (int i = 0; i <= NUM_WEAPONS; i++)
        {
            og_weapon_brands[*li].push_back(vector< vector<int> >());
            for (int j = 0; j < 3; j++)
            {
                og_weapon_brands[*li][i].push_back(vector<int>());
                for (int k = 0; k < NUM_SPECIAL_WEAPONS; k++)
                    og_weapon_brands[*li][i][j].push_back(0);
            }
        }
        og_armour_brands[*li] = vector< vector< vector<int> > >();
        for (int i = 0; i <= NUM_ARMOURS; i++)
        {
            og_armour_brands[*li].push_back(vector< vector<int> >());
            for (int j = 0; j < 3; j++)
            {
                og_armour_brands[*li][i].push_back(vector<int>());
                for (int k = 0; k < NUM_SPECIAL_ARMOURS; k++)
                    og_armour_brands[*li][i][j].push_back(0);
            }
        }
        og_missile_brands[*li] = vector< vector<int> >();
        for (int i = 0; i <= NUM_MISSILES; i++)
        {
            og_missile_brands[*li].push_back(vector<int>());
            for (int j = 0; j < NUM_SPECIAL_ARMOURS; j++)
                og_missile_brands[*li][i].push_back(0);
        }

        map<monster_type, int>::const_iterator mi;
        for (mi = og_valid_monsters.begin(); mi != og_valid_monsters.end();
             mi++)
        {
            for (unsigned int i = 0; i < og_monster_fields.size(); i++)
                og_monsters[*li][mi->second][og_monster_fields[i]] = 0;
        }

    }
}

void objgen_report_item(item_def &item)
{
    level_id cur_lev = level_id::current();
    og_item ogi(item);

    if (ogi.base_type == OG_UNUSED)
        return;

    bool is_arte = is_artefact(item);
    bool is_weapon = ogi.base_type == OG_WEAPONS;
    bool is_armour = ogi.base_type == OG_ARMOUR;
    bool is_missile = ogi.base_type == OG_MISSILES;
    int brand = -1;
    int class_sum_ind = _og_get_max_subtype(ogi.base_type);
    og_summary_level sum_ind = is_arte ? SUMMARY_ARTE : SUMMARY_ORD;
    string num_f = is_arte ? "ArteNum" : "OrdNum";
    string cursed_f = is_arte ? "ArteNumCursed" : "OrdNumCursed";
    string ench_f = is_arte ? "ArteEnch" : "OrdEnch";
    map<string, double> &item_stats =
        og_items[cur_lev][ogi.base_type][ogi.sub_type];
    map<string, double> &sum_stats =
        og_items[cur_lev][ogi.base_type][class_sum_ind];

    if (is_weapon)
        brand = get_weapon_brand(item);
    else if (is_armour)
        brand = get_armour_ego_type(item);
    else if (is_missile)
        brand = get_ammo_brand(item);

    // The Some averages are calculated after all items are tallied.
    switch (ogi.base_type)
    {
    case OG_MISSILES:
        og_missile_brands[cur_lev][ogi.sub_type][brand] += item.quantity;
        og_missile_brands[cur_lev][class_sum_ind][brand] += item.quantity;
        item_stats["NumBranded"] += brand > 0 ? item.quantity : 0;
        sum_stats["NumBranded"] += brand > 0 ? item.quantity : 0;
        //deliberate fallthrough
    case OG_SCROLLS:
    case OG_POTIONS:
    case OG_GOLD:
        item_stats["Num"] += item.quantity;
        sum_stats["Num"] += item.quantity;
        item_stats["NumPiles"] += 1;
        sum_stats["NumPiles"] += 1;
        break;
    case OG_FOOD:
        item_stats["Num"] += item.quantity;
        sum_stats["Num"] += item.quantity;
        item_stats["NumPiles"] += 1;
        sum_stats["NumPiles"] += 1;
        item_stats["TotalNormNutr"] += food_value(item);
        sum_stats["TotalNormNutr"] += food_value(item);
        you.mutation[MUT_CARNIVOROUS] = 3;
        item_stats["TotalCarnNutr"] += food_value(item)
            * food_is_meaty(item.sub_type);
        sum_stats["TotalCarnNutr"] += food_value(item)
            * food_is_meaty(item.sub_type);
        you.mutation[MUT_CARNIVOROUS] = 0;
        you.mutation[MUT_HERBIVOROUS] = 3;
        item_stats["TotalHerbNutr"] += food_value(item)
            * food_is_veggie(item.sub_type);
        sum_stats["TotalHerbNutr"] += food_value(item)
            * food_is_veggie(item.sub_type);
        you.mutation[MUT_HERBIVOROUS] = 0;
        break;
    case OG_WANDS:
        item_stats["Num"] += 1;
        sum_stats["Num"] += 1;
        item_stats["WandCharges"] += item.plus;
        sum_stats["WandCharges"] += item.plus;
        break;
    case OG_WEAPONS:
    case OG_ARMOUR:
        item_stats[num_f] += 1;
        sum_stats[num_f] += 1;
        item_stats[cursed_f] += item.cursed();
        sum_stats[cursed_f] += item.cursed();
        item_stats[ench_f] += item.plus;
        sum_stats[ench_f] += item.plus;
        item_stats["OrdNumBranded"] += !is_arte && brand > 0;
        sum_stats["OrdNumBranded"] += !is_arte && brand > 0;
        item_stats["AllNum"] += 1;
        sum_stats["AllNum"] += 1;
        item_stats["AllNumCursed"] += item.cursed();
        sum_stats["AllNumCursed"] += item.cursed();
        item_stats["AllEnch"] += item.plus;
        sum_stats["AllEnch"] += item.plus;
        if (is_weapon)
        {
            og_weapon_brands[cur_lev][ogi.sub_type][sum_ind][brand] += 1;
            og_weapon_brands[cur_lev][class_sum_ind][sum_ind][brand] += 1;
            og_weapon_brands[cur_lev][ogi.sub_type][SUMMARY_ALL][brand] += 1;
            og_weapon_brands[cur_lev][class_sum_ind][SUMMARY_ALL][brand] += 1;
        }
        else
        {
            og_armour_brands[cur_lev][ogi.sub_type][sum_ind][brand] += 1;
            og_armour_brands[cur_lev][class_sum_ind][sum_ind][brand] += 1;
            og_armour_brands[cur_lev][ogi.sub_type][SUMMARY_ALL][brand] += 1;
            og_armour_brands[cur_lev][class_sum_ind][SUMMARY_ALL][brand] += 1;
        }
        break;
    case OG_STAVES:
        item_stats["Num"] += 1;
        sum_stats["Num"] += 1;
        item_stats["NumCursed"] += item.cursed();
        sum_stats["NumCursed"] += item.cursed();
        break;
    case OG_JEWELLERY:
        item_stats[num_f] += 1;
        sum_stats[num_f] += 1;
        item_stats[cursed_f] += item.cursed();
        sum_stats[cursed_f] += item.cursed();
        item_stats[ench_f] += item.plus;
        sum_stats[ench_f] += item.plus;
        item_stats["AllNum"] += 1;
        sum_stats["AllNum"] += 1;
        item_stats["AllNumCursed"] += item.cursed();
        sum_stats["AllNumCursed"] += item.cursed();
        item_stats["AllEnch"] += item.plus;
        sum_stats["AllEnch"] += item.plus;
        break;
    case OG_RODS:
        item_stats["Num"] += 1;
        sum_stats["Num"] += 1;
        item_stats["RodRecharge"] += item.plus2 / ROD_CHARGE_MULT;
        sum_stats["RodRecharge"] += item.plus2 / ROD_CHARGE_MULT;
        item_stats["RodMana"] += item.special;
        sum_stats["RodMana"] += item.special;
        item_stats["NumCursed"] += item.cursed();
        sum_stats["NumCursed"] += item.cursed();
        break;
    case OG_MISCELLANY:
        item_stats["Num"] += 1;
        sum_stats["Num"] += 1;
        item_stats["MiscPlus1"] += item.plus;
        sum_stats["MiscPlus1"] += item.plus;
        item_stats["MiscPlus2"] += item.plus2;
        sum_stats["MiscPlus2"] += item.plus2;
        break;
    case OG_DECKS:
        switch (deck_rarity(item))
        {
        case DECK_RARITY_COMMON:
            item_stats["PlainNum"] += 1;
            sum_stats["PlainNum"] += 1;
            break;
        case DECK_RARITY_RARE:
            item_stats["OrnateNum"] += 1;
            sum_stats["OrnateNum"] += 1;
            break;
        case DECK_RARITY_LEGENDARY:
            item_stats["LegendaryNum"] += 1;
            sum_stats["LegendaryNum"] += 1;
            break;
        default:
            break;
        }
        item_stats["AllNum"] += 1;
        sum_stats["AllNum"] += 1;
        item_stats["AllDeckCards"] += item.plus;
        sum_stats["AllDeckCards"] += item.plus;
        break;
    case OG_BOOKS:
    case OG_MANUALS:
    case OG_ARTEBOOKS:
        item_stats["Num"] += 1;
        sum_stats["Num"] += 1;
        break;
    default:
        die("Buggy og_class_type");
        break;
    }
}

void objgen_report_monster(monster *mons)
{
    level_id cur_lev = level_id::current();
    if (!og_valid_monsters.count(mons->type))
        return;

    int mons_id = og_valid_monsters[mons->type];
    int sum_ind = og_valid_monsters[NUM_MONSTERS];
    map<string, double> &mons_stats = og_monsters[cur_lev][mons_id];
    map<string, double> &sum_stats = og_monsters[cur_lev][sum_ind];
    corpse_effect_type chunk_effect = mons_corpse_effect(mons->type);

    mons_stats["Num"] += 1;
    sum_stats["Num"] += 1;
    mons_stats["MonsXP"] += exper_value(mons);
    sum_stats["MonsXP"] += exper_value(mons);
    mons_stats["TotalXP"] += exper_value(mons);
    sum_stats["TotalXP"] += exper_value(mons);
    mons_stats["MonsHP"] += mons->max_hit_points;
    sum_stats["MonsHP"] += mons->max_hit_points;
    mons_stats["MonsHD"] += mons->hit_dice;
    sum_stats["MonsHD"] += mons->hit_dice;
    // copied from turn_corpse_into_chunks()
    double avg_chunks = (1 + stepdown_value(get_max_corpse_chunks(mons->type),
                                            4, 4, 12, 12)) / 2.0;
    mons_stats["MonsNumChunks"] += avg_chunks;
    sum_stats["MonsNumChunks"] += avg_chunks;
    bool is_contam = chunk_effect == CE_POISON_CONTAM
        || chunk_effect == CE_CONTAMINATED;
    bool is_clean = chunk_effect == CE_CLEAN || chunk_effect == CE_POISONOUS;
    if (is_clean)
    {
        mons_stats["TotalNormNutr"] += avg_chunks * CHUNK_BASE_NUTRITION;
        sum_stats["TotalNormNutr"] += avg_chunks * CHUNK_BASE_NUTRITION;
    }
    if (is_contam)
    {
        mons_stats["TotalNormNutr"] += avg_chunks * CHUNK_BASE_NUTRITION / 2.0;
        sum_stats["TotalNormNutr"] += avg_chunks * CHUNK_BASE_NUTRITION / 2.0;
    }
    if (is_clean || is_contam)
    {
        mons_stats["TotalGourmNutr"] += avg_chunks * CHUNK_BASE_NUTRITION;
        sum_stats["TotalGourmNutr"] += avg_chunks * CHUNK_BASE_NUTRITION;
    }
}

static void _og_write_level_headers(int num_fields)
{
    int level_count = 0;
    fprintf(og_outf, "Place");
    vector<level_id>::const_iterator li;
    for (li = og_levels.begin(); li != og_levels.end(); li++)
    {
        fprintf(og_outf, "\t%s", li->describe().c_str());
        for (int i = 0; i < num_fields - 1; i++)
            fprintf(og_outf, "\t");
        if (++level_count == og_branches[li->branch])
        {
            if (level_count == 1)
            {
                level_count = 0;
                continue;
            }

            fprintf(og_outf, "\t%s", branches[li->branch].abbrevname);
            for (int i = 0; i < num_fields - 1; i++)
                fprintf(og_outf, "\t");
            level_count = 0;
        }
    }
    if (og_branches.size() > 1)
    {
        fprintf(og_outf, "\tAllLevels");
        for (int i = 0; i < num_fields - 1; i++)
            fprintf(og_outf, "\t");
    }
    fprintf(og_outf, "\n");
}

static void _og_write_stat_headers(vector<string> fields)
{
    int level_count = 0;
    fprintf(og_outf, "Property");
    vector<level_id>::const_iterator li;
    for (li = og_levels.begin(); li != og_levels.end(); li++)
    {
        for (unsigned int i = 0; i < fields.size(); i++)
            fprintf(og_outf, "\t%s", fields[i].c_str());

        // Branch-level headers
        if (++level_count == og_branches[li->branch])
        {
            if (level_count == 1)
            {
                level_count = 0;
                continue;
            }

            for (unsigned int i = 0; i < fields.size(); i++)
                fprintf(og_outf, "\t%s", fields[i].c_str());
            level_count = 0;
        }
    }
    if (og_branches.size() > 1)
    {
        for (unsigned int i = 0; i < fields.size(); i++)
            fprintf(og_outf, "\t%s", fields[i].c_str());
    }
    fprintf(og_outf, "\n");
}

static void _og_write_stat(map<string, double> &item_stats, string field)
{
    ostringstream output;
    double value = 0;

    output.precision(OG_STAT_PRECISION);
    if (field == "PileQuant")
        value = item_stats["Num"] / item_stats["NumPiles"];
    else if (field == "WandCharges"
             || field == "RodMana"
             || field == "RodRecharge"
             || field == "MiscPlus"
             || field == "MiscPlus2"
             || field == "MonsHD"
             || field == "MonsHP"
             || field == "MonsXP"
             || field == "MonsNumChunks")
    {
        value = item_stats[field] / item_stats["Num"];
    }
    else if (field == "AllEnch" || field == "AllDeckCards")
        value = item_stats[field] / item_stats["AllNum"];
    else if (field == "ArteEnch")
        value = item_stats[field] / item_stats["ArteNum"];
    else if (field == "OrdEnch")
        value = item_stats[field] / item_stats["OrdNum"];
    else
        value = item_stats[field] / SysEnv.map_gen_iters;
    output << "\t" << value;
    fprintf(og_outf, "%s", output.str().c_str());
}

static string _og_brand_name(og_class_type base_type, int sub_type, int brand)
 {
     string brand_name = "";
     item_def item = _og_dummy_item(base_type, sub_type);

     if (!brand)
            brand_name = "none";
     else
     {
         item.special = brand;
         switch (base_type)
            {
            case OG_WEAPONS:
                brand_name = weapon_brand_name(item, true);
                break;
            case OG_ARMOUR:
                brand_name = armour_ego_name(item, true);
                break;
            case OG_MISSILES:
                brand_name = missile_brand_name(item, MBN_TERSE);
                break;
            default:
                break;
            }
     }
     return brand_name;
 }

static void _og_write_brand_stats(vector<int> &brand_stats,
                                  og_class_type item_type, int sub_type,
                                  vector<int> *branch_stats = NULL,
                                  vector<int> *total_stats = NULL)
{
    ASSERT(item_type == OG_WEAPONS || item_type == OG_ARMOUR
           || item_type == OG_MISSILES);
    item_def dummy_item = _og_dummy_item(item_type, sub_type);
    unsigned int num_brands = brand_stats.size();
    bool first_brand = true;
    ostringstream brand_summary;

    brand_summary.precision(OG_STAT_PRECISION);
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

        brand_name = _og_brand_name(item_type, sub_type, i);
        brand_summary << brand_name.c_str() << "(" << value << ")";
        if (branch_stats)
            (*branch_stats)[i] += brand_stats[i];
        if (total_stats)
            (*total_stats)[i] += brand_stats[i];
    }
    fprintf(og_outf, "\t%s", brand_summary.str().c_str());
}

static string _og_item_name(og_class_type item_type, int sub_type)
{

    string name = "";
    if (item_type == OG_MANUALS)
        name = "Manual";
    else if (item_type == OG_ARTEBOOKS)
    {
        name = _og_orig_sub_type(item_type, sub_type) == BOOK_RANDART_LEVEL
            ? "Level Artefact Book" : "Theme Artefact Book";
    }
    else if (sub_type == _og_get_max_subtype(item_type))
        name = "All " + item_class_name(_og_orig_base_type(item_type));
    else
    {
        item_def dummy_item = _og_dummy_item(item_type, sub_type);
        name = dummy_item.name(DESC_DBNAME, true, true);
    }
    return name;
}

static void _og_write_item_stats(og_class_type item_type, int sub_type)
{
    map <string, double> branch_stats, total_stats;
    bool is_brand_equip = item_type == OG_WEAPONS || item_type == OG_ARMOUR;
    int num_brands = item_type == OG_WEAPONS ? NUM_SPECIAL_WEAPONS
        : item_type == OG_ARMOUR ? NUM_SPECIAL_ARMOURS
        : item_type == OG_MISSILES ? NUM_SPECIAL_MISSILES : 1;
    vector < vector<int> > branch_brand_stats(3, vector<int>(num_brands));
    vector < vector<int> > total_brand_stats(3, vector<int>(num_brands));
    int level_count = 0;
    vector <string> fields = og_item_fields[item_type];

    fprintf(og_outf, "%s", _og_item_name(item_type, sub_type).c_str());
    vector<level_id>::const_iterator li;

    for (unsigned int i = 0; i < fields.size(); i++)
        total_stats[fields[i]] = 0;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < num_brands; j++)
            total_brand_stats[i][j] = 0;
    }
    for (li = og_levels.begin(); li != og_levels.end(); li++)
    {
        if (!level_count)
        {
            for (unsigned int i = 0; i < fields.size(); i++)
                branch_stats[fields[i]] = 0;
            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < num_brands; j++)
                    branch_brand_stats[i][j] = 0;
            }
        }

        map <string, double> &item_stats = og_items[*li][item_type][sub_type];
        for (unsigned int i = 0; i < fields.size(); i++)
        {
            branch_stats[fields[i]] += item_stats[fields[i]];
            total_stats[fields[i]] += item_stats[fields[i]];
            _og_write_stat(item_stats, fields[i]);
        }
        vector<int> brand_stats;
        if (is_brand_equip)
        {
            for (int j = 0; j < 3; j++)
            {
                if (item_type == OG_WEAPONS)
                    brand_stats = og_weapon_brands[*li][sub_type][j];
                else
                    brand_stats = og_armour_brands[*li][sub_type][j];
                _og_write_brand_stats(brand_stats, item_type, sub_type,
                                      &branch_brand_stats[j],
                                      &total_brand_stats[j]);
            }
        }
        else if (item_type == OG_MISSILES)
        {
            brand_stats = og_missile_brands[*li][sub_type];
            _og_write_brand_stats(brand_stats, item_type, sub_type,
                                  &branch_brand_stats[0],
                                  &total_brand_stats[0]);
        }

        if (++level_count == og_branches[li->branch])
        {
            if (level_count == 1)
            {
                level_count = 0;
                continue;
            }

            for (unsigned int i = 0; i < fields.size(); i++)
                _og_write_stat(branch_stats, fields[i]);

            if (is_brand_equip)
            {
                for (int j = 0; j < 3; j++)
                {
                    _og_write_brand_stats(branch_brand_stats[j], item_type,
                                          sub_type);
                }
            }
            else if (item_type == OG_MISSILES)
            {
                _og_write_brand_stats(branch_brand_stats[0], item_type,
                                      sub_type);
            }
            level_count = 0;
        }
    }
    // Only do grand summary if there are multiple branches.
    if (og_branches.size() > 1)
    {
        for (unsigned int i = 0; i < fields.size(); i++)
            _og_write_stat(total_stats, fields[i]);

        if (is_brand_equip)
        {
            for (int j = 0; j < 3; j++)
                _og_write_brand_stats(total_brand_stats[j], item_type, sub_type);
        }
        else if (item_type == OG_MISSILES)
            _og_write_brand_stats(total_brand_stats[0], item_type, sub_type);
    }
    fprintf(og_outf, "\n");
}

static void _og_write_monster_stats(int mons_id, monster_type mons_type)
{
    map <string, double> branch_stats, total_stats;
    int level_count = 0;
    vector <string> fields = og_monster_fields;

    if (mons_id == og_valid_monsters[NUM_MONSTERS])
        fprintf(og_outf, "All Monsters");
    else
        fprintf(og_outf, "%s", mons_type_name(mons_type, DESC_PLAIN).c_str());
    vector<level_id>::const_iterator li;

    for (unsigned int i = 0; i < fields.size(); i++)
        total_stats[fields[i]] = 0;
    for (li = og_levels.begin(); li != og_levels.end(); li++)
    {
        if (!level_count)
        {
            for (unsigned int i = 0; i < fields.size(); i++)
                branch_stats[fields[i]] = 0;
        }

        map <string, double> &mons_stats = og_monsters[*li][mons_id];
        for (unsigned int i = 0; i < fields.size(); i++)
        {
            branch_stats[fields[i]] += mons_stats[fields[i]];
            total_stats[fields[i]] += mons_stats[fields[i]];
            _og_write_stat(mons_stats, fields[i]);
        }

        if (++level_count == og_branches[li->branch])
        {
            if (level_count == 1)
            {
                level_count = 0;
                continue;
            }

            for (unsigned int i = 0; i < fields.size(); i++)
                _og_write_stat(branch_stats, fields[i]);

            level_count = 0;
        }
    }
    // Only do grand summary if there are multiple branches.
    if (og_branches.size() > 1)
    {
        for (unsigned int i = 0; i < fields.size(); i++)
            _og_write_stat(total_stats, fields[i]);
    }
    fprintf(og_outf, "\n");
}

static void _og_write_object_stats()
{
    string range_desc = "";
    if (SysEnv.map_gen_range.get())
        range_desc = SysEnv.map_gen_range->describe();
    else
        range_desc = "Entire Dungeon";
    og_outf = fopen(og_out_file, "w");
    fprintf(og_outf, "Object Generation Stats\n"
            "Number of iterations: %d\n"
            "Levels generated: %s\n"
            "Version: %s\n", SysEnv.map_gen_iters, range_desc.c_str(),
            Version::Long);
    fprintf(og_outf, "Item Generation Stats\n");
    for (int i = 0; i < NUM_OG_CLASSES; i++)
    {
        og_class_type item_type = static_cast<og_class_type>(i);
        int num_types = _og_get_max_subtype(item_type);
        fprintf(og_outf, "\n%s\n",
                _og_class_name(item_type).c_str());
        vector<string> fields = og_item_fields[item_type];
        if (item_type == OG_WEAPONS || item_type == OG_ARMOUR)
        {
            for (int j = 0; j < 3; j++)
                fields.push_back(og_equip_brand_fields[j]);
        }
        else if (item_type == OG_MISSILES)
            fields.push_back(og_missile_brand_field);

        _og_write_level_headers(fields.size());
        _og_write_stat_headers(fields);
        for (int j = 0; j <= num_types; j++)
            _og_write_item_stats(item_type, j);
    }
    fprintf(og_outf, "Monster Generation Stats\n");
    _og_write_level_headers(og_monster_fields.size());
    _og_write_stat_headers(og_monster_fields);
    map<monster_type, int>::const_iterator mi;
    for (mi = og_valid_monsters.begin(); mi != og_valid_monsters.end(); mi++)
        _og_write_monster_stats(mi->second, mi->first);
    fclose(og_outf);
}

void objgen_generate_stats()
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

    sprintf(og_stat_fmt, "%%1.%df", OG_STAT_PRECISION);
    // Populate a vector of the levels ids we've made
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        if (brdepth[i] == -1)
            continue;

        const branch_type br = static_cast<branch_type>(i);
#if TAG_MAJOR_VERSION == 34
        // Don't want to include Forest since it doesn't generate
        if (br == BRANCH_FOREST)
            continue;
#endif
        for (int dep = 1; dep <= brdepth[i]; ++dep)
        {
            const level_id lid(br, dep);
            if (SysEnv.map_gen_range.get()
                && !SysEnv.map_gen_range->is_usable_in(lid))
            {
                continue;
            }
            if (!og_branches.count(br))
                og_branches[br] = 1;
            else
                og_branches[br] += 1;
            og_levels.push_back(lid);
        }
    }
    _og_init_fields();
    _og_init_foods();
    _og_init_monsters();
    _og_init_stats();
    mapstat_build_levels(SysEnv.map_gen_iters);
    _og_write_object_stats();
}

#endif // DEBUG_DIAGNOSTICS
