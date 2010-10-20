#include "AppHdr.h"

#include "godpassive.h"

#include "artefact.h"
#include "branch.h"
#include "coord.h"
#include "coordit.h"
#include "defines.h"
#include "describe.h"
#include "env.h"
#include "files.h"
#include "food.h"
#include "fprop.h"
#include "godprayer.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "mon-stuff.h"
#include "options.h"
#include "player.h"
#include "player-stats.h"
#include "religion.h"
#include "spl-book.h"
#include "state.h"
#include "stuff.h"

int che_boost_level()
{
    if (you.religion != GOD_CHEIBRIADOS)
        return (0);

    return (std::min(player_ponderousness(), piety_rank() - 1));
}

int che_boost(che_boost_type bt, int level)
{
    if (level == 0)
        return (0);

    switch (bt)
    {
    case CB_RNEG:
        return (level > 0 ? 1 : 0);
    case CB_RCOLD:
        return (level > 1 ? 1 : 0);
    case CB_RFIRE:
        return (level > 2 ? 1 : 0);
    case CB_STATS:
        return (level * (level + 1)) / 2;
    default:
        return (0);
    }
}

void che_handle_change(che_change_type ct, int diff)
{
    if (you.religion != GOD_CHEIBRIADOS)
        return;

    const std::string typestr = (ct == CB_PIETY ? "piety" : "ponderous");

    // Values after the change.
    const int ponder = player_ponderousness();
    const int prank = piety_rank() - 1;
    const int newlev = std::min(ponder, prank);

    // Reconstruct values before the change.
    int oldponder = ponder;
    int oldprank = prank;
    if (ct == CB_PIETY)
        oldprank -= diff;
    else // ct == CB_PONDEROUS
        oldponder -= diff;
    const int oldlev = std::min(oldponder, oldprank);
    dprf("Che %s %+d: %d/%d -> %d/%d", typestr.c_str(), diff,
         oldponder, oldprank,
         ponder, prank);

    for (int i = 0; i < NUM_BOOSTS; ++i)
    {
        const che_boost_type bt = static_cast<che_boost_type>(i);
        const int boostdiff = che_boost(bt, newlev) - che_boost(bt, oldlev);
        if (boostdiff == 0)
            continue;

        const std::string elemmsg = god_name(you.religion)
                                    + (boostdiff > 0 ? " " : " no longer ")
                                    + "protects you from %s.";
        switch (bt)
        {
        case CB_RNEG:
            mprf(MSGCH_GOD, elemmsg.c_str(), "negative energy");
            break;
        case CB_RCOLD:
            mprf(MSGCH_GOD, elemmsg.c_str(), "cold");
            break;
        case CB_RFIRE:
            mprf(MSGCH_GOD, elemmsg.c_str(), "fire");
            break;
        case CB_STATS:
        {
            mprf(MSGCH_GOD, "%s %s the support of your attributes.",
                 god_name(you.religion).c_str(),
                 boostdiff > 0 ? "raises" : "reduces");
            const std::string reason = "Cheibriados " + typestr + " change";
            notify_stat_change(reason.c_str());
            break;
        }
        case NUM_BOOSTS:
            break;
        }
    }
}

// Eat from one random off-level item stack.
void jiyva_eat_offlevel_items()
{
    // For wizard mode 'J' command
    if (you.religion != GOD_JIYVA)
        return;

    if (crawl_state.game_is_sprint())
        return;

    while (true)
    {
        if (one_chance_in(200))
            break;

        const int branch = random2(NUM_BRANCHES);

        // Choose level based on main dungeon depth so that levels short branches
        // aren't picked more often.
        ASSERT(branches[branch].depth <= branches[BRANCH_MAIN_DUNGEON].depth);
        const int level  = random2(branches[BRANCH_MAIN_DUNGEON].depth) + 1;

        const level_id lid(static_cast<branch_type>(branch), level);

        if (lid == level_id::current() || !is_existing_level(lid))
            continue;

        dprf("Checking %s", lid.describe().c_str());

        level_excursion le;
        le.go_to(lid);
        while (true)
        {
            if (one_chance_in(200))
                break;

            const coord_def p = random_in_bounds();

            if (igrd(p) == NON_ITEM || testbits(env.pgrid(p), FPROP_NO_JIYVA))
                continue;

            for (stack_iterator si(p); si; ++si)
            {
                if (!is_item_jelly_edible(*si) || one_chance_in(4))
                    continue;

                if (one_chance_in(4))
                    break;

                dprf("Eating %s on %s",
                     si->name(DESC_PLAIN).c_str(), lid.describe().c_str());

                // Needs a message now to explain possible hp or mp
                // gain from jiyva_slurp_bonus()
                mpr("You hear a distant slurping noise.");
                sacrifice_item_stack(*si);
                destroy_item(si.link());
            }
            return;
        }
    }
}

void jiyva_slurp_bonus(int item_value, int *js)
{
    if (you.penance[GOD_JIYVA])
        return;

    if (you.piety >= piety_breakpoint(1)
        && x_chance_in_y(you.piety, MAX_PIETY))
    {
        //same as a sultana
        lessen_hunger(70, true);
        *js |= JS_FOOD;
    }

    if (you.piety >= piety_breakpoint(3)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && you.magic_points < you.max_magic_points)
    {
         inc_mp(std::max(random2(item_value), 1), false);
         *js |= JS_MP;
     }

    if (you.piety >= piety_breakpoint(4)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && you.hp < you.hp_max)
    {
         inc_hp(std::max(random2(item_value), 1), false);
         *js |= JS_HP;
     }
}

enum eq_type
{
    ET_WEAPON,
    ET_ARMOUR,
    ET_JEWELS,
    NUM_ET
};

int ash_bondage_level(int type_only)
{
    if (you.religion != GOD_ASHENZARI)
        return (0);

    int cursed[NUM_ET] = {0}, slots[NUM_ET] = {0};

    for (int i = EQ_WEAPON; i < NUM_EQUIP; i++)
    {
        eq_type s;
        if (i == EQ_WEAPON)
            s = ET_WEAPON;
        else if (i <= EQ_MAX_ARMOUR)
            s = ET_ARMOUR;
        else
            s = ET_JEWELS;

        // kittehs don't obey hoomie rules!
        if (you.species == SP_CAT)
        {
            if (i >= EQ_LEFT_RING)
                s = (eq_type)(i - EQ_LEFT_RING);
            else
                ASSERT(!you_can_wear(i, true));
        }

        // transformed away slots are still considered to be possibly bound
        if (you.equip[i] != -1)
        {
            slots[s]++;
            const item_def& item = you.inv[you.equip[i]];
            if (item.cursed()
                && (i != EQ_WEAPON
                    || item.base_type == OBJ_WEAPONS
                    || item.base_type == OBJ_STAVES))
            {
                cursed[s]++;
            }
        }
    }

    int bonus = 0;
    for (int s = ET_WEAPON; s < NUM_ET; s++)
    {
        if (type_only && s+1 != type_only)
            continue;
        if (cursed[s] > slots[s] / 2)
            bonus++;
    }
    return bonus;
}

void ash_check_bondage()
{
    int new_level = ash_bondage_level();
    if (new_level == you.bondage_level)
        return;

    if (new_level > you.bondage_level)
        mprf(MSGCH_GOD, "You feel %s bound.",
             (new_level == 1) ? "slightly" :
             (new_level == 2) ? "seriously" :
             (new_level == 3) ? "completely" :
                                "buggily");
    else
        mprf(MSGCH_GOD, "You feel less bound.");
    you.bondage_level = new_level;
}

static bool _is_slot_cursed(equipment_type eq)
{
    const item_def *worn = you.slot_item(eq, true);
    if (!worn || !worn->cursed())
        return false;

    if (eq == EQ_WEAPON)
        return worn->base_type == OBJ_WEAPONS || worn->base_type == OBJ_STAVES;
    return true;
}

static bool _jewel_auto_id(const item_def& item)
{
    if (item.base_type != OBJ_JEWELLERY)
        return false;

    // Yay, such lists tend to get out of sync very fast...
    // Fortunately, this one doesn't have to be too accurate.
    switch(item.sub_type)
    {
    case RING_REGENERATION:
        return (player_mutation_level(MUT_SLOW_HEALING) < 3);
    case RING_PROTECTION:
    case RING_EVASION:
    case RING_STRENGTH:
    case RING_DEXTERITY:
    case RING_INTELLIGENCE:
        return !!item.plus;
    case AMU_FAITH:
        return (you.religion != GOD_NO_GOD);
    case RING_WIZARDRY:
        return !!player_spell_skills();
    case RING_INVISIBILITY:
    case RING_TELEPORTATION:
    case RING_MAGICAL_POWER:
    case RING_LEVITATION:
    case RING_CHARM:
    case AMU_RAGE:
    case AMU_GUARDIAN_SPIRIT:
        return true;
    default:
        return false;
    }
}

bool ash_id_item(item_def& item, bool silent)
{
    if (you.religion != GOD_ASHENZARI)
        return false;

    iflags_t old_ided = item.flags & ISFLAG_IDENT_MASK;
    iflags_t ided = ISFLAG_KNOW_CURSE;

    if (item.base_type == OBJ_WEAPONS
        || item.base_type == OBJ_ARMOUR
        || item_is_rod(item))
    {
        ided |= ISFLAG_KNOW_PROPERTIES | ISFLAG_KNOW_TYPE;
    }

    if (_jewel_auto_id(item))
    {
        ided |= ISFLAG_EQ_JEWELLERY_MASK;
    }

    if (item.base_type == OBJ_ARMOUR
        && you.piety >= piety_breakpoint(0)
        && _is_slot_cursed(get_armour_slot(item)))
    {
        // Armour would id the pluses when worn, unlike weapons.
        ided |= ISFLAG_KNOW_PLUSES;
    }

    if ((item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES)
        && you.piety >= piety_breakpoint(1)
        && _is_slot_cursed(EQ_WEAPON))
    {
        ided |= ISFLAG_KNOW_PLUSES;
    }

    if (item.base_type == OBJ_JEWELLERY
        && you.piety >= piety_breakpoint(1)
        && (jewellery_is_amulet(item) ?
             _is_slot_cursed(EQ_AMULET) :
             (_is_slot_cursed(EQ_LEFT_RING) && _is_slot_cursed(EQ_RIGHT_RING))
         ))
    {
        ided |= ISFLAG_EQ_JEWELLERY_MASK;
    }

    if (ided & ~old_ided)
    {
        if (ided & ISFLAG_KNOW_TYPE)
            set_ident_type(item, ID_KNOWN_TYPE);
        set_ident_flags(item, ided);
        if (Options.autoinscribe_artefacts && is_artefact(item))
            add_autoinscription(item, artefact_auto_inscription(item));

        if (&item == you.weapon())
            you.wield_change = true;

        if (!silent)
            mpr(item.name(DESC_INVENTORY_EQUIP).c_str());

        seen_item(item);
        return true;
    }

    // nothing new
    return false;
}

void ash_id_inventory()
{
    if (you.religion != GOD_ASHENZARI)
        return;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def& item = you.inv[i];
        if (item.defined())
            ash_id_item(item, false);
    }
}

static bool is_ash_portal(dungeon_feature_type feat)
{
    switch(feat)
    {
    case DNGN_ENTER_HELL:
    case DNGN_ENTER_LABYRINTH:
    case DNGN_ENTER_ABYSS: // for completeness/Pan
    case DNGN_EXIT_ABYSS:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_EXIT_PANDEMONIUM:
    // DNGN_TRANSIT_PANDEMONIUM is too mundane
    case DNGN_ENTER_PORTAL_VAULT:
        return true;
    default:
        return false;
    }
}

// Yay for rectangle_iterator and radius_iterator not sharing a base type
static bool _check_portal(coord_def where)
{
    const dungeon_feature_type feat = grd(where);
    if (feat != env.map_knowledge(where).feat() && is_ash_portal(feat))
    {
        env.map_knowledge(where).set_feature(feat);
        env.map_knowledge(where).flags |= MAP_MAGIC_MAPPED_FLAG;

        if (!testbits(env.pgrid(where), FPROP_SEEN_OR_NOEXP))
        {
            env.pgrid(where) |= FPROP_SEEN_OR_NOEXP;
            if (!you.see_cell(where))
                return true;
        }
    }
    return false;
}

int ash_detect_portals(bool all)
{
    if (you.religion != GOD_ASHENZARI)
        return 0;

    int portals_found = 0;
    const int map_radius = LOS_RADIUS + 1;

    if (all)
    {
        for (rectangle_iterator ri(0); ri; ++ri)
        {
            if (_check_portal(*ri))
                portals_found++;
        }
    }
    else
    {
        for (radius_iterator ri(you.pos(), map_radius, C_ROUND); ri; ++ri)
        {
            if (_check_portal(*ri))
                portals_found++;
        }
    }

    you.seen_portals += portals_found;
    return (portals_found);
}
