/**
 * @file
 * @brief Debugging code to scan the list of items and monsters.
**/

#include "AppHdr.h"

#include "dbg-scan.h"

#include "artefact.h"
#include "branch.h"
#include "chardump.h"
#include "coord.h"
#include "coordit.h"
#include "dbg-util.h"
#include "dungeon.h"
#include "env.h"
#include "itemname.h"
#include "libutil.h"
#include "message.h"
#include "mon-util.h"
#include "shopping.h"
#include "state.h"
#include "terrain.h"
#include "traps.h"

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

    mpr(msg.c_str(), chan);
    mpr(name, chan);

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
void debug_item_scan(void)
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
    if (!warned && Generating_Level)
    {
        mpr("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", MSGCH_ERROR);
        mpr("mgrd problem occurred during level generation", MSGCH_ERROR);

        debug_dump_levgen();
    }
}

static bool _inside_vault(const vault_placement& place, const coord_def &pos)
{
    const coord_def delta = pos - place.pos;

    return (delta.x >= 0 && delta.y >= 0
            && delta.x < place.size.x && delta.y < place.size.y);
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
                    mpr("Additionally, it isn't alive.", MSGCH_WARN);
                warned = true;
            }
            else if (!m->alive())
            {
                _announce_level_prob(warned);
                mprf_nocap(MSGCH_WARN,
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
                        mpr("Other monster thinks it's holding the item, too.",
                            MSGCH_WARN);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    mpr("Other monster isn't holding it, though.", MSGCH_WARN);
            } // if (holder != m)
        } // for (int j = 0; j < NUM_MONSTER_SLOTS; j++)

        monster* m1 = monster_by_mid(m->mid);
        if (m1 != m)
        {
            if (m1 && m1->mid == m->mid)
            {
                mprf(MSGCH_ERROR,
                     "Error: monster %s(%d) has same mid as %s(%d) (%d)",
                     m->name(DESC_PLAIN, true).c_str(), m->mindex(),
                     m1->name(DESC_PLAIN, true).c_str(), m1->mindex(), m->mid);
            }
            else
                ASSERT(monster_by_mid(m->mid) == m);
        }

        if (m->constricted_by && !monster_by_mid(m->constricted_by))
        {
            mprf(MSGCH_ERROR, "Error: constrictor missing for monster %s(%d)",
                 m->name(DESC_PLAIN, true).c_str(), m->mindex());
        }
    } // for (int i = 0; i < MAX_MONSTERS; ++i)

    for (map<mid_t, unsigned short>::const_iterator mc = env.mid_cache.begin();
         mc != env.mid_cache.end(); ++mc)
    {
        unsigned short idx = mc->second;
        ASSERT(!invalid_monster_index(idx));
        ASSERT(menv[idx].mid == mc->first);
    }

    // No problems?
    if (!warned)
        return;

    // If this wasn't the result of generating a level then there's nothing
    // more to report.
    if (!Generating_Level)
    {
        // Force the dev to notice problems. :P
        more();
        return;
    }

    // No vaults to report on?
    if (env.level_vaults.empty() && Temp_Vaults.empty())
    {
        mpr("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", MSGCH_ERROR);
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

    mpr("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", MSGCH_ERROR);
    // Force the dev to notice problems. :P
    more();
}
#endif

void check_map_validity()
{
#ifdef ASSERTS
    dungeon_feature_type portal = DNGN_UNSEEN;
    if (you.where_are_you == BRANCH_MAIN_DUNGEON)
    {
        if (you.depth == 24)
            portal = DNGN_ENTER_PANDEMONIUM;
        else if (you.depth == 25)
            portal = DNGN_ENTER_ABYSS;
        else if (you.depth == 21)
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

        if (feat == portal)
            portal = DNGN_UNSEEN;
        if (feat == exit)
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
