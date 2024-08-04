/**
 * @file
 * @brief Debugging code to scan the list of items and monsters.
**/

#include "AppHdr.h"

#include "dbg-scan.h"

#include <cerrno>
#include <cmath>
#include <sstream>

#include "artefact.h"
#include "branch.h"
#include "chardump.h"
#include "coord.h"
#include "coordit.h"
#include "dbg-util.h"
#include "dungeon.h"
#include "end.h"
#include "env.h"
#include "god-abil.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "mon-util.h"
#include "shopping.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "terrain.h"
#include "traps.h"
#ifdef WIZARD
#include "wiz-dgn.h"
#endif

#ifdef DEBUG_ITEM_SCAN

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
        for (int obj = env.igrid(*ri); obj != NON_ITEM; obj = env.item[obj].link)
        {
            if (obj < 0 || obj > MAX_ITEMS)
            {
                if (env.igrid(*ri) == obj)
                {
                    mprf(MSGCH_ERROR, "env.igrid has invalid item index %d "
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
            if (!env.item[obj].defined())
            {
                debug_dump_item(env.item[obj].name(DESC_PLAIN).c_str(), obj, env.item[obj],
                           "Linked invalid item at (%d,%d)!", ri->x, ri->y);
            }

            // Check that item knows what stack it's in.
            if (env.item[obj].pos != *ri)
            {
                debug_dump_item(env.item[obj].name(DESC_PLAIN).c_str(), obj, env.item[obj],
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
        if (!env.item[i].defined())
            continue;

        strlcpy(name, env.item[i].name(DESC_PLAIN).c_str(), sizeof(name));

        const monster* mon = env.item[i].holding_monster();

        // Don't check (-1, -1) player items or (-2, -2) monster items
        // (except to make sure that the monster is alive).
        if (env.item[i].pos.origin())
            debug_dump_item(name, i, env.item[i], "Unlinked temporary item:");
        else if (mon != nullptr && mon->type == MONS_NO_MONSTER)
            debug_dump_item(name, i, env.item[i], "Unlinked item held by dead monster:");
        else if ((env.item[i].pos.x > 0 || env.item[i].pos.y > 0) && !visited[i])
        {
            debug_dump_item(name, i, env.item[i], "Unlinked item:");

            if (!in_bounds(env.item[i].pos))
            {
                mprf(MSGCH_ERROR, "Item position (%d, %d) is out of bounds",
                     env.item[i].pos.x, env.item[i].pos.y);
            }
            else
            {
                mprf("env.igrid(%d,%d) = %d",
                     env.item[i].pos.x, env.item[i].pos.y, env.igrid(env.item[i].pos));
            }

            // Let's check to see if it's an errant monster object:
            for (int j = 0; j < MAX_MONSTERS; ++j)
            {
                monster& mons(env.mons[j]);
                for (mon_inv_iterator ii(mons); ii; ++ii)
                {
                    if (ii->index() == i)
                    {
                        mprf("Held by monster #%d: %s at (%d,%d)",
                             j, mons.name(DESC_A, true).c_str(),
                             mons.pos().x, mons.pos().y);
                    }
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
        if (strstr(name, "questionable") != nullptr
            || strstr(name, "eggplant") != nullptr
            || strstr(name, "buggy") != nullptr
            || strstr(name, "buggi") != nullptr)
        {
            debug_dump_item(name, i, env.item[i], "Bad item:");
        }
        else if (abs(env.item[i].plus) > 30 &&
                    (env.item[i].base_type == OBJ_WEAPONS
                     || env.item[i].base_type == OBJ_ARMOUR))
        {
            debug_dump_item(name, i, env.item[i], "Bad plus:");
        }
        else if (!is_artefact(env.item[i])
                 && (env.item[i].base_type == OBJ_WEAPONS
                        && env.item[i].brand >= NUM_SPECIAL_WEAPONS
                     || env.item[i].base_type == OBJ_ARMOUR
                        && env.item[i].brand >= NUM_SPECIAL_ARMOURS))
        {
            debug_dump_item(name, i, env.item[i], "Bad special value:");
        }
        else if (env.item[i].flags & ISFLAG_SUMMONED && in_bounds(env.item[i].pos))
            debug_dump_item(name, i, env.item[i], "Summoned item on floor:");
    }

    // Quickly scan monsters for "program bug"s.
    for (i = 0; i < MAX_MONSTERS; ++i)
    {
        const monster& mons = env.mons[i];

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
        mprf(MSGCH_ERROR, "env.mgrid problem occurred during level generation");

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

    for (auto &vault : env.level_vaults)
        if (_inside_vault(*vault, pos))
            out.push_back(vault->map.name);

    for (const vault_placement &vault : Temp_Vaults)
        if (_inside_vault(vault, pos))
            out.push_back(vault.map.name);

    return out;
}

static string _vault_desc(const coord_def pos)
{
    int mi = env.level_map_ids(pos);
    if (mi == INVALID_MAP_INDEX)
        return "";

    string out;

    for (auto &vault : env.level_vaults)
    {
        if (_inside_vault(*vault, pos))
        {
            coord_def br = vault->pos + vault->size - 1;
            out += make_stringf(" [vault: %s (%d,%d)-(%d,%d) (%dx%d)]",
                        vault->map_name_at(pos).c_str(),
                        vault->pos.x, vault->pos.y,
                        br.x, br.y,
                        vault->size.x, vault->size.y);
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
            const int mons = env.mgrid[x][y];
            if (mons == NON_MONSTER)
                continue;

            if (invalid_monster_index(mons))
            {
                mprf(MSGCH_ERROR, "env.mgrid at (%d, %d) has invalid monster "
                                  "index %d",
                     x, y, mons);
                continue;
            }

            const monster* m = &env.mons[mons];
            const coord_def pos(x, y);
            if (m->pos() != pos)
            {
                bogus_pos.push_back(pos);
                bogus_idx.push_back(mons);

                _announce_level_prob(warned);
                mprf(MSGCH_WARN,
                     "Bogosity: env.mgrid at (%d,%d) points at %s, "
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
                     "env.mgrid at (%d,%d) points at dead monster %s",
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

        const monster* m = &env.mons[i];
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
                 m->full_name(DESC_PLAIN).c_str(),
                 pos.x, pos.y, i);
        }
        else if (env.mgrid(pos) != i)
        {
            floating_mons.push_back(i);
            is_floating[i] = true;

            _announce_level_prob(warned);
            mprf(MSGCH_WARN, "Floating monster: %s at (%d,%d), midx = %d",
                 m->full_name(DESC_PLAIN).c_str(),
                 pos.x, pos.y, i);
            warned = true;
            for (int j = 0; j < MAX_MONSTERS; ++j)
            {
                if (i == j)
                    continue;

                const monster* m2 = &env.mons[j];

                if (m2->pos() != m->pos())
                    continue;

                string full = m2->full_name(DESC_PLAIN);
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
        } // if (env.mgrid(m->pos()) != i)

        if (feat_is_wall(env.grid(pos)))
        {
#if defined(DEBUG_FATAL)
            // if we're going to dump, point out the culprit
            env.pgrid(pos) |= FPROP_HIGHLIGHT;
#endif
            mprf(MSGCH_ERROR, "Monster %s in %s at (%d, %d)%s",
                 m->full_name(DESC_PLAIN).c_str(),
                 dungeon_feature_name(env.grid(pos)),
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
                     m->full_name(DESC_PLAIN).c_str(),
                     pos.x, pos.y, idx, j);
                continue;
            }
            item_def &item(env.item[idx]);

            if (!item.defined())
            {
                _announce_level_prob(warned);
                warned = true;
                mprf(MSGCH_WARN, "Monster %s (%d, %d) holding invalid item in "
                                 "slot %d (midx = %d)",
                     m->full_name(DESC_PLAIN).c_str(),
                     pos.x, pos.y, j, i);
                continue;
            }

            const monster* holder = item.holding_monster();

            if (holder == nullptr)
            {
                _announce_level_prob(warned);
                warned = true;
                debug_dump_item(item.name(DESC_PLAIN, false, true).c_str(),
                            idx, item,
                           "Monster %s (%d, %d) holding non-monster "
                           "item (midx = %d)",
                           m->full_name(DESC_PLAIN).c_str(),
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
                     m->full_name(DESC_PLAIN).c_str(),
                     m->pos().x, m->pos().y, i,
                     item.name(DESC_PLAIN).c_str(),
                     holder->full_name(DESC_PLAIN).c_str(),
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

        if (you.constricted_by == m->mid && !m->is_constricting(you))
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
            else if (!h->is_constricting(*m))
            {
                mprf(MSGCH_ERROR, "Error: constricting[%s(mindex=%d mid=%d)] "
                                  "entry missing for monster %s(mindex=%d mid=%d)",
                     m->name(DESC_PLAIN, true).c_str(), m->mindex(), m->mid,
                     h->name(DESC_PLAIN, true).c_str(), h->mindex(), h->mid);
            }
        }
    } // for (int i = 0; i < MAX_MONSTERS; ++i)

    for (const auto &entry : env.mid_cache)
    {
        unsigned short idx = entry.second;
        ASSERT(!invalid_monster_index(idx));
        if (env.mons[idx].mid != entry.first)
        {
            monster &m(env.mons[idx]);
            die("mid cache bogosity: mid %d points to %s mindex=%d mid=%d",
                entry.first, m.name(DESC_PLAIN, true).c_str(), m.mindex(),
                m.mid);
        }
    }

    if (in_bounds(you.pos()))
        if (const monster* m = monster_at(you.pos()))
            if (!fedhas_passthrough(m))
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

    for (int idx : floating_mons)
    {
        const monster* mon = &env.mons[idx];
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
        const monster* mon = &env.mons[idx];

        string str = make_stringf("Bogus env.mgrid (%d, %d) pointing to %s", pos.x,
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
#if defined(ASSERTS) && !defined(DEBUG_VETO_RESUME)
    dungeon_feature_type portal = DNGN_UNSEEN;
    if (player_in_branch(BRANCH_DEPTHS))
    {
        // TODO: centralize these numbers
        // crosscheck with _add_missing_branches when changing
        if (you.depth == 1)
            portal = DNGN_ENTER_HELL;
        else if (you.depth == 2)
            portal = DNGN_ENTER_PANDEMONIUM;
        else if (you.depth == 3)
            portal = DNGN_ENTER_ABYSS;
    }

    dungeon_feature_type exit = DNGN_UNSEEN;
    if (you.depth == 1 && !player_in_branch(root_branch))
        exit = branches[you.where_are_you].exit_stairs;

    // these may require you to look farther:
    if (exit == DNGN_EXIT_PANDEMONIUM)
        exit = DNGN_TRANSIT_PANDEMONIUM;
    if (exit == DNGN_EXIT_ABYSS || exit == DNGN_EXIT_CRUCIBLE)
        exit = DNGN_UNSEEN;

    for (rectangle_iterator ri(0); ri; ++ri)
    {
        dungeon_feature_type feat = env.grid(*ri);
        if (feat <= DNGN_UNSEEN || feat >= NUM_FEATURES)
            die("invalid feature %d at (%d,%d)", feat, ri->x, ri->y);
        const char *name = dungeon_feature_name(feat);
        ASSERT(name);
        ASSERT(*name); // placeholders get empty names

        trap_at(*ri); // this has all needed asserts already

        if (shop_struct *shop = shop_at(*ri))
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
