/*
 *  File:       dbg-scan.cc
 *  Summary:    Debugging code to scan the list of items and monsters.
 *  Written by: Linley Henzell and Jesse Jones
 */

#include "AppHdr.h"

#include "dbg-scan.h"

#include "artefact.h"
#include "coord.h"
#include "coordit.h"
#include "dbg-util.h"
#include "dungeon.h"
#include "env.h"
#include "itemname.h"
#include "libutil.h"
#include "message.h"
#include "mon-util.h"
#include "state.h"

#define DEBUG_ITEM_SCAN
#ifdef DEBUG_ITEM_SCAN
static void _dump_item( const char *name, int num, const item_def &item )
{
    mpr(name, MSGCH_ERROR);

    mprf("    item #%d:  base: %d; sub: %d; plus: %d; plus2: %d; special: %d",
         num, item.base_type, item.sub_type,
         item.plus, item.plus2, item.special );

    mprf("    quant: %d; colour: %d; ident: 0x%08"PRIx64"; ident_type: %d",
         item.quantity, item.colour, item.flags,
         get_ident_type( item ) );

    mprf("    x: %d; y: %d; link: %d", item.pos.x, item.pos.y, item.link );

    crawl_state.cancel_cmd_repeat();
}

//---------------------------------------------------------------
//
// debug_item_scan
//
//---------------------------------------------------------------
void debug_item_scan( void )
{
    int   i;
    char  name[256];

    FixedVector<bool, MAX_ITEMS> visited;
    visited.init(false);

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
                    mprf(MSGCH_ERROR, "Item in stack at (%d, %d) has ",
                                      "invalid link %d",
                         ri->x, ri->y, obj);
                }
                break;
            }

            // Check for invalid (zero quantity) items that are linked in.
            if (!mitm[obj].defined())
            {
                mprf(MSGCH_ERROR, "Linked invalid item at (%d,%d)!",
                     ri->x, ri->y);
                _dump_item( mitm[obj].name(DESC_PLAIN).c_str(), obj, mitm[obj] );
            }

            // Check that item knows what stack it's in.
            if (mitm[obj].pos != *ri)
            {
                mprf(MSGCH_ERROR,"Item position incorrect at (%d,%d)!",
                     ri->x, ri->y);
                _dump_item( mitm[obj].name(DESC_PLAIN).c_str(),
                            obj, mitm[obj] );
            }

            // If we run into a premarked item we're in real trouble,
            // this will also keep this from being an infinite loop.
            if (visited[obj])
            {
                mprf(MSGCH_ERROR,
                     "Potential INFINITE STACK at (%d, %d)", ri->x, ri->y);
                break;
            }
            visited[obj] = true;
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
        {
            mpr("Unlinked temporary item:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
        else if (mon != NULL && mon->type == MONS_NO_MONSTER)
        {
            mpr("Unlinked item held by dead monster:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
        else if ((mitm[i].pos.x > 0 || mitm[i].pos.y > 0) && !visited[i])
        {
            mpr("Unlinked item:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );

            if (!in_bounds(mitm[i].pos))
            {
                mprf(MSGCH_ERROR, "Item position (%d, %d) is out of bounds",
                     mitm[i].pos.x, mitm[i].pos.y);
            }
            else
            {
                mprf("igrd(%d,%d) = %d",
                     mitm[i].pos.x, mitm[i].pos.y, igrd( mitm[i].pos ));
            }

            // Let's check to see if it's an errant monster object:
            for (int j = 0; j < MAX_MONSTERS; ++j)
                for (int k = 0; k < NUM_MONSTER_SLOTS; ++k)
                {
                    if (menv[j].inv[k] == i)
                    {
                        mprf("Held by monster #%d: %s at (%d,%d)",
                             j, menv[j].name(DESC_CAP_A, true).c_str(),
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
        //   -- bola is an illegal fixed artefact
        //
        //   -- items described as buggy (typically adjectives out of range)
        //      (note: covers buggy, bugginess, buggily, whatever else)
        //
        if (strstr( name, "questionable" ) != NULL
            || strstr( name, "eggplant" ) != NULL
            || strstr( name, "bola" ) != NULL
            || strstr( name, "bugg" ) != NULL)
        {
            mpr("Bad item:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
        else if ((mitm[i].base_type == OBJ_WEAPONS
                    && (abs(mitm[i].plus) > 30
                        || abs(mitm[i].plus2) > 30
                        || !is_artefact( mitm[i] )
                           && mitm[i].special >= NUM_SPECIAL_WEAPONS))

                 || (mitm[i].base_type == OBJ_MISSILES
                     && (abs(mitm[i].plus) > 25
                         || !is_artefact( mitm[i] )
                            && mitm[i].special >= NUM_SPECIAL_MISSILES))

                 || (mitm[i].base_type == OBJ_ARMOUR
                     && (abs(mitm[i].plus) > 25
                         || !is_artefact( mitm[i] )
                            && mitm[i].special >= NUM_SPECIAL_ARMOURS)))
        {
            mpr("Bad plus or special value:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
        else if (mitm[i].flags & ISFLAG_SUMMONED
                 && in_bounds(mitm[i].pos))
        {
            mpr("Summoned item on floor:", MSGCH_ERROR);
            _dump_item( name, i, mitm[i] );
        }
    }

    // Quickly scan monsters for "program bug"s.
    for (i = 0; i < MAX_MONSTERS; ++i)
    {
        const monster& mons = menv[i];

        if (mons.type == MONS_NO_MONSTER)
            continue;

        if (mons.name(DESC_PLAIN, true).find("questionable") !=
            std::string::npos)
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

static std::vector<std::string> _in_vaults(const coord_def &pos)
{
    std::vector<std::string> out;

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

    return (out);
}

void debug_mons_scan()
{
    std::vector<coord_def> bogus_pos;
    std::vector<int>       bogus_idx;

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
                mprf(MSGCH_WARN,
                     "mgrd at (%d,%d) points at dead monster %s",
                     x, y, m->name(DESC_PLAIN, true).c_str());
                warned = true;
            }
        }

    std::vector<int> floating_mons;
    bool             is_floating[MAX_MONSTERS];

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        is_floating[i] = false;

        const monster* m = &menv[i];
        if (!m->alive())
            continue;

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

                std::string full = m2->full_name(DESC_PLAIN, true);
                if (m2->alive())
                {
                    mprf(MSGCH_WARN, "Also at (%d, %d): %s, midx = %d",
                         pos.x, pos.y, full.c_str(), j);
                }
                else if (m2->type != -1)
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
                mprf(MSGCH_WARN, "Monster %s (%d, %d) holding non-monster "
                                 "item (midx = %d)",
                     m->full_name(DESC_PLAIN, true).c_str(),
                     pos.x, pos.y, i);
                _dump_item( item.name(DESC_PLAIN, false, true).c_str(),
                            idx, item );
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
    } // for (int i = 0; i < MAX_MONSTERS; ++i)

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
        std::vector<std::string> vaults = _in_vaults(mon->pos());

        std::string str =
            make_stringf("Floating monster %s (%d, %d)",
                         mon->name(DESC_PLAIN, true).c_str(),
                         mon->pos().x, mon->pos().y);

        if (vaults.size() == 0)
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

        std::string str =
            make_stringf("Bogus mgrd (%d, %d) pointing to %s",
                         pos.x, pos.y, mon->name(DESC_PLAIN, true).c_str());

        std::vector<std::string> vaults = _in_vaults(pos);

        if (vaults.size() == 0)
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

        if (vaults.size() == 0)
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
