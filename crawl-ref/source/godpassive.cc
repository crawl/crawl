#include "AppHdr.h"
#include <math.h>

#include "godpassive.h"

#include "art-enum.h"
#include "artefact.h"
#include "branch.h"
#include "coord.h"
#include "coordit.h"
#include "defines.h"
#include "env.h"
#include "files.h"
#include "food.h"
#include "fprop.h"
#include "goditem.h"
#include "godprayer.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "mon-stuff.h"
#include "player.h"
#include "religion.h"
#include "skills2.h"
#include "state.h"

int chei_stat_boost(int piety)
{
    if (!you_worship(GOD_CHEIBRIADOS) || you.penance[GOD_CHEIBRIADOS])
        return 0;
    if (piety < piety_breakpoint(0))  // Since you've already begun to slow down.
        return 1;
    if (piety >= piety_breakpoint(5))
        return 15;
    return ((piety - 10) / 10);
}

// Eat from one random off-level item stack.
void jiyva_eat_offlevel_items()
{
    // For wizard mode 'J' command
    if (!you_worship(GOD_JIYVA))
        return;

    if (crawl_state.game_is_sprint())
        return;

    while (true)
    {
        if (one_chance_in(200))
            break;

        const int branch = random2(NUM_BRANCHES);
        int js = JS_NONE;

        // Choose level based on main dungeon depth so that levels of
        // short branches aren't picked more often.
        ASSERT(brdepth[branch] <= MAX_BRANCH_DEPTH);
        const int level = random2(MAX_BRANCH_DEPTH) + 1;

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
                sacrifice_item_stack(*si, &js);
                item_was_destroyed(*si);
                destroy_item(si.link());
                jiyva_slurp_message(js);
            }
            return;
        }
    }
}

void jiyva_slurp_bonus(int item_value, int *js)
{
    if (player_under_penance(GOD_JIYVA))
        return;

    if (you.piety >= piety_breakpoint(1)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && you.is_undead != US_UNDEAD)
    {
        //same as a sultana
        lessen_hunger(70, true);
        *js |= JS_FOOD;
    }

    if (you.piety >= piety_breakpoint(3)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && you.magic_points < you.max_magic_points)
    {
        inc_mp(max(random2(item_value), 1));
        *js |= JS_MP;
    }

    if (you.piety >= piety_breakpoint(4)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && you.hp < you.hp_max
        && !you.duration[DUR_DEATHS_DOOR])
    {
        inc_hp(max(random2(item_value), 1));
        *js |= JS_HP;
    }
}

void jiyva_slurp_message(int js)
{
    if (js != JS_NONE)
    {
        if (js & JS_FOOD)
            mpr("You feel a little less hungry.");
        if (js & JS_MP)
            mpr("You feel your power returning.");
        if (js & JS_HP)
            mpr("You feel a little better.");
    }
}

void ash_init_bondage(player *y)
{
    y->bondage_level = 0;
    for (int i = ET_WEAPON; i < NUM_ET; ++i)
        y->bondage[i] = 0;
}

static bool _two_handed()
{
    const item_def* wpn = you.slot_item(EQ_WEAPON, true);
    if (!wpn)
        return false;

    hands_reqd_type wep_type = you.hands_reqd(*wpn);
    return wep_type == HANDS_TWO;
}

void ash_check_bondage(bool msg)
{
    if (!you_worship(GOD_ASHENZARI))
        return;

    int cursed[NUM_ET] = {0}, slots[NUM_ET] = {0};

    for (int i = EQ_WEAPON; i < NUM_EQUIP; i++)
    {
        eq_type s;
        if (i == EQ_WEAPON)
            s = ET_WEAPON;
        else if (i == EQ_SHIELD)
            s = ET_SHIELD;
        else if (i <= EQ_MAX_ARMOUR)
            s = ET_ARMOUR;
        // Octopodes don't count these slots:
        else if (you.species == SP_OCTOPODE &&
                 (i == EQ_LEFT_RING || i == EQ_RIGHT_RING))
            continue;
        // *Only* octopodes count these slots:
        else if (you.species != SP_OCTOPODE && i > EQ_AMULET)
            continue;
        else
            s = ET_JEWELS;

        // transformed away slots are still considered to be possibly bound
        if (you_can_wear(i, true))
        {
            slots[s]++;
            if (you.equip[i] != -1)
            {
                const item_def& item = you.inv[you.equip[i]];
                if (item.cursed() && (i != EQ_WEAPON || is_weapon(item)))
                {
                    if (s == ET_WEAPON && _two_handed())
                    {
                        cursed[ET_WEAPON] = 3;
                        cursed[ET_SHIELD] = 3;
                    }
                    else
                    {
                        cursed[s]++;
                        if (i == EQ_BODY_ARMOUR && is_unrandom_artefact(item)
                            && item.special == UNRAND_LEAR)
                        {
                            cursed[s] += 3;
                        }
                    }
                }
            }
        }
    }

    int8_t new_bondage[NUM_ET];
    int old_level = you.bondage_level;
    for (int s = ET_WEAPON; s < NUM_ET; s++)
    {
        if (slots[s] == 0)
            new_bondage[s] = -1;
        // That's only for 2 handed weapons.
        else if (cursed[s] > slots[s])
            new_bondage[s] = 3;
        else if (cursed[s] == slots[s])
            new_bondage[s] = 2;
        else if (cursed[s] > slots[s] / 2)
            new_bondage[s] = 1;
        else
            new_bondage[s] = 0;
    }

    you.bondage_level = 0;
    // kittehs don't obey hoomie rules!
    if (you.species == SP_FELID)
    {
        for (int i = EQ_LEFT_RING; i < NUM_EQUIP; ++i)
            if (you.equip[i] != -1 && you.inv[you.equip[i]].cursed())
                ++you.bondage_level;
    }
    else
        for (int i = ET_WEAPON; i < NUM_ET; ++i)
            if (new_bondage[i] > 0)
                ++you.bondage_level;

    int flags = 0;
    if (msg)
    {
        for (int s = ET_WEAPON; s < NUM_ET; s++)
            if (new_bondage[s] != you.bondage[s])
                flags |= 1 << s;
    }

    you.skill_boost.clear();
    for (int s = ET_WEAPON; s < NUM_ET; s++)
    {
        you.bondage[s] = new_bondage[s];
        map<skill_type, int8_t> boosted_skills = ash_get_boosted_skills(eq_type(s));
        for (map<skill_type, int8_t>::iterator it = boosted_skills.begin();
             it != boosted_skills.end(); ++it)
        {
            you.skill_boost[it->first] += it->second;
            if (you.skill_boost[it->first] > 3)
                you.skill_boost[it->first] = 3;
        }

    }

    if (msg)
    {
        string desc = ash_describe_bondage(flags, you.bondage_level != old_level);
        if (!desc.empty())
            mpr(desc, MSGCH_GOD);
    }
}

string ash_describe_bondage(int flags, bool level)
{
    string desc;
    if (flags & ETF_WEAPON && flags & ETF_SHIELD
        && you.bondage[ET_WEAPON] != -1)
    {
        if (you.bondage[ET_WEAPON] == you.bondage[ET_SHIELD])
        {
            desc = make_stringf("Your %s are %sbound.\n",
                                you.hand_name(true).c_str(),
                                you.bondage[ET_WEAPON] ? "" : "not ");
        }
        else
        {
            desc = make_stringf("Your %s %s is bound but not your %s %s.\n",
                                you.bondage[ET_WEAPON] ? "weapon" : "shield",
                                you.hand_name(false).c_str(),
                                you.bondage[ET_WEAPON] ? "shield" : "weapon",
                                you.hand_name(false).c_str());
        }
    }
    else if (flags & ETF_WEAPON && you.bondage[ET_WEAPON] != -1)
    {
        desc = make_stringf("Your weapon %s is %sbound.\n",
                            you.hand_name(false).c_str(),
                            you.bondage[ET_WEAPON] ? "" : "not ");
    }
    else if (flags & ETF_SHIELD && you.bondage[ET_SHIELD] != -1)
    {
        desc = make_stringf("Your shield %s is %sbound.\n",
                            you.hand_name(false).c_str(),
                            you.bondage[ET_SHIELD] ? "" : "not ");
    }

    if (flags & ETF_ARMOUR && flags & ETF_JEWELS
        && you.bondage[ET_ARMOUR] == you.bondage[ET_JEWELS]
        && you.bondage[ET_ARMOUR] != -1)
    {
        desc += make_stringf("You are %s bound in armour and magic.\n",
                             you.bondage[ET_ARMOUR] == 0 ? "not" :
                             you.bondage[ET_ARMOUR] == 1 ? "partially"
                                                         : "fully");
    }
    else
    {
        if (flags & ETF_ARMOUR && you.bondage[ET_ARMOUR] != -1)
            desc += make_stringf("You are %s bound in armour.\n",
                                 you.bondage[ET_ARMOUR] == 0 ? "not" :
                                 you.bondage[ET_ARMOUR] == 1 ? "partially"
                                                             : "fully");

        if (flags & ETF_JEWELS && you.bondage[ET_JEWELS] != -1)
            desc += make_stringf("You are %s bound in magic.\n",
                                 you.bondage[ET_JEWELS] == 0 ? "not" :
                                 you.bondage[ET_JEWELS] == 1 ? "partially"
                                                             : "fully");
    }

    if (level)
    {
        desc += make_stringf("You are %s bound.",
                             you.bondage_level == 0 ? "not" :
                             you.bondage_level == 1 ? "slightly" :
                             you.bondage_level == 2 ? "moderately" :
                             you.bondage_level == 3 ? "seriously" :
                             you.bondage_level == 4 ? "fully"
                                                    : "buggily");
    }

    return trim_string(desc);
}

static bool _is_slot_cursed(equipment_type eq)
{
    const item_def *worn = you.slot_item(eq, true);
    if (!worn || !worn->cursed())
        return false;

    if (eq == EQ_WEAPON)
        return is_weapon(*worn);
    return true;
}

static bool _jewel_auto_id(const item_def& item)
{
    if (item.base_type != OBJ_JEWELLERY)
        return false;

    // Yay, such lists tend to get out of sync very fast...
    // Fortunately, this one doesn't have to be too accurate.
    switch (item.sub_type)
    {
    case RING_REGENERATION:
        return player_mutation_level(MUT_SLOW_HEALING) < 3;
    case RING_PROTECTION:
    case RING_EVASION:
    case RING_STRENGTH:
    case RING_DEXTERITY:
    case RING_INTELLIGENCE:
        return !!item.plus;
    case AMU_FAITH:
        return (!you_worship(GOD_NO_GOD));
    case AMU_THE_GOURMAND:
        return you.species != SP_MUMMY
               && player_mutation_level(MUT_HERBIVOROUS) < 3;
    case RING_INVISIBILITY:
    case RING_TELEPORTATION:
    case RING_TELEPORT_CONTROL:
    case RING_MAGICAL_POWER:
    case RING_FLIGHT:
    case RING_ICE:
    case RING_FIRE:
    case RING_WIZARDRY:
    case AMU_RAGE:
    case AMU_GUARDIAN_SPIRIT:
        return true;
    default:
        return false;
    }
}

bool god_id_item(item_def& item, bool silent)
{
    iflags_t old_ided = item.flags & ISFLAG_IDENT_MASK;
    iflags_t ided = 0;

    if (you_worship(GOD_ASHENZARI))
    {
        // Don't identify runes or the orb, since this has no gameplay purpose
        // and might mess up other things.
        if (item_is_rune(item) || item_is_orb(item))
            return false;

        ided = ISFLAG_KNOW_CURSE;

        if ((item.base_type == OBJ_JEWELLERY || item.base_type == OBJ_STAVES)
            && item_needs_autopickup(item))
        {
            item.props["needs_autopickup"] = true;
        }

        if (is_weapon(item) || item.base_type == OBJ_ARMOUR)
            ided |= ISFLAG_KNOW_PROPERTIES | ISFLAG_KNOW_TYPE;

        if (_jewel_auto_id(item))
            ided |= ISFLAG_IDENT_MASK;

        if (item.base_type == OBJ_ARMOUR
            && you.piety >= piety_breakpoint(0)
            && _is_slot_cursed(get_armour_slot(item)))
        {
            // Armour would id the pluses when worn, unlike weapons.
            ided |= ISFLAG_KNOW_PLUSES;
        }

        if (is_weapon(item)
            && you.piety >= piety_breakpoint(1)
            && _is_slot_cursed(EQ_WEAPON))
        {
            ided |= ISFLAG_KNOW_PLUSES;
        }

        if (you.species != SP_OCTOPODE && item.base_type == OBJ_JEWELLERY
            && you.piety >= piety_breakpoint(1)
            && (jewellery_is_amulet(item) ?
                 _is_slot_cursed(EQ_AMULET) :
                 (_is_slot_cursed(EQ_LEFT_RING) && _is_slot_cursed(EQ_RIGHT_RING))
             ))
        {
            ided |= ISFLAG_IDENT_MASK;
        }
        else if (you.species == SP_OCTOPODE && item.base_type == OBJ_JEWELLERY
            && you.piety >= piety_breakpoint(1)
            && (jewellery_is_amulet(item) ?
                 _is_slot_cursed(EQ_AMULET) :
                 (_is_slot_cursed(EQ_RING_ONE) && _is_slot_cursed(EQ_RING_TWO) &&
                  _is_slot_cursed(EQ_RING_THREE) && _is_slot_cursed(EQ_RING_FOUR) &&
                  _is_slot_cursed(EQ_RING_FIVE) && _is_slot_cursed(EQ_RING_SIX) &&
                  _is_slot_cursed(EQ_RING_SEVEN) && _is_slot_cursed(EQ_RING_EIGHT))
             ))
        {
            ided |= ISFLAG_IDENT_MASK;
        }
    }
    else if (you_worship(GOD_ELYVILON))
    {
        if ((item.base_type == OBJ_STAVES || item.base_type == OBJ_RODS)
            && (is_evil_item(item) || is_unholy_item(item)))
        {
            // staff of death, evil rods
            ided |= ISFLAG_KNOW_TYPE;
        }

        // Don't use is_{evil,unholy}_item() for weapons -- on demonic weapons
        // the brand is irrelevant, unrands may have an innocuous brand; let's
        // still show evil brands on unholy weapons for consistency even if this
        // gives more information than absolutely needed.
        brand_type brand = get_weapon_brand(item);
        if (brand == SPWPN_DRAINING || brand == SPWPN_PAIN
            || brand == SPWPN_VAMPIRICISM || brand == SPWPN_REAPING)
        {
            ided |= ISFLAG_KNOW_TYPE;
        }
    }

    if (ided & ~old_ided)
    {
        if (ided & ISFLAG_KNOW_TYPE)
            set_ident_type(item, ID_KNOWN_TYPE);
        set_ident_flags(item, ided);

        if (item.props.exists("needs_autopickup") && is_useless_item(item))
            item.props.erase("needs_autopickup");

        if (&item == you.weapon())
            you.wield_change = true;

        if (!silent)
            mpr_nocap(item.name(DESC_INVENTORY_EQUIP).c_str());

        seen_item(item);
        return true;
    }

    // nothing new
    return false;
}

void ash_id_monster_equipment(monster* mon)
{
    if (!you_worship(GOD_ASHENZARI))
        return;

    bool id = false;

    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (mon->inv[i] == NON_ITEM)
            continue;

        item_def &item = mitm[mon->inv[i]];
        if ((i != MSLOT_WAND || !is_offensive_wand(item))
            && !item_is_branded(item))
        {
            continue;
        }

        if (x_chance_in_y(you.bondage_level, 4))
        {
            if (i == MSLOT_WAND)
            {
                set_ident_type(OBJ_WANDS, item.sub_type, ID_KNOWN_TYPE);
                mon->props["wand_known"] = true;
            }
            else
                set_ident_flags(item, ISFLAG_KNOW_TYPE);

            id = true;
        }
    }

    if (id)
        mon->props["ash_id"] = true;
}

static bool is_ash_portal(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_ENTER_HELL:
    case DNGN_ENTER_LABYRINTH:
    case DNGN_ENTER_ABYSS: // for completeness
    case DNGN_EXIT_THROUGH_ABYSS:
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
    if (!you_worship(GOD_ASHENZARI))
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
    return portals_found;
}

monster_type ash_monster_tier(const monster *mon)
{
    return monster_type(MONS_SENSED_TRIVIAL + monster_info(mon).threat);
}

map<skill_type, int8_t> ash_get_boosted_skills(eq_type type)
{
    const int bondage = you.bondage[type];
    map<skill_type, int8_t> boost;
    if (bondage <= 0)
        return boost;

    // Include melded.
    const item_def* wpn = you.slot_item(EQ_WEAPON, true);
    const item_def* armour = you.slot_item(EQ_BODY_ARMOUR, true);
    const int evp = armour ? -property(*armour, PARM_EVASION) : 0;
    switch (type)
    {
    case (ET_WEAPON):
        ASSERT(wpn);

        // Boost weapon skill.
        if (wpn->base_type == OBJ_WEAPONS)
        {
            boost[is_range_weapon(*wpn) ? range_skill(*wpn)
                                        : weapon_skill(*wpn)] = bondage;
        }

        // Those staves don't benefit from evocation.
        // Boost spellcasting instead.
        if (wpn->base_type == OBJ_STAVES
            && (wpn->sub_type == STAFF_POWER
                || wpn->sub_type == STAFF_CONJURATION
                || wpn->sub_type == STAFF_ENERGY
                || wpn->sub_type == STAFF_WIZARDRY))
        {
            boost[SK_SPELLCASTING] = 2;
        }
        // Other staves use evocation.
        else if (wpn->base_type == OBJ_STAVES)
        {
            boost[SK_EVOCATIONS] = 1;
            boost[SK_STAVES] = 1;

        }
        else if (wpn->base_type == OBJ_RODS)
            boost[SK_EVOCATIONS] = 2;

        break;

    case (ET_SHIELD):
        if (bondage == 2)
            boost[SK_SHIELDS] = 1;
        break;

    // Bonus for bounded armour depends on body armour type.
    case (ET_ARMOUR):
        if (evp < 6)
        {
            boost[SK_STEALTH] = bondage;
            boost[SK_DODGING] = bondage;
        }
        else if (evp < 12)
        {
            boost[SK_DODGING] = bondage;
            boost[SK_ARMOUR] = bondage;
        }
        else
            boost[SK_ARMOUR] = bondage + 1;
        break;

    // Boost all spell schools and evoc (to give some appeal to melee).
    case (ET_JEWELS):
        for (int i = SK_FIRST_MAGIC_SCHOOL; i <= SK_LAST_MAGIC; ++i)
            boost[skill_type(i)] = bondage;
        boost[SK_EVOCATIONS] = bondage;
        break;

    default:
        die("Unknown equipment type.");
    }

    return boost;
}

int ash_skill_boost(skill_type sk, int scale)
{
    // It gives a bonus to skill points. The formula is:
    // factor * piety_rank * skill_level
    // low bonus    -> factor = 3
    // medium bonus -> factor = 5
    // high bonus   -> factor = 7

    unsigned int skill_points = you.skill_points[sk];
    skill_points += (you.skill_boost[sk] * 2 + 1) * piety_rank()
                    * you.skill(sk, 10, true) * species_apt_factor(sk);

    int level = you.skills[sk];
    while (level < 27 && skill_points >= skill_exp_needed(level + 1, sk))
        ++level;

    level = level * scale + get_skill_progress(sk, level, skill_points, scale);

    return min(level, 27 * scale);
}
