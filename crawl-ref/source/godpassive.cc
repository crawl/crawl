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
#include "libutil.h"
#include "math.h"
#include "mon-stuff.h"
#include "options.h"
#include "player.h"
#include "player-stats.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "state.h"
#include "stuff.h"

int che_boost_level()
{
    if (you.religion != GOD_CHEIBRIADOS || you.penance[GOD_CHEIBRIADOS])
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
        int js = JS_NONE;

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
                sacrifice_item_stack(*si, &js);
                destroy_item(si.link());
                jiyva_slurp_message(js);
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

void ash_init_bondage()
{
    you.bondage_level = 0;
    for (int i = ET_WEAPON; i < NUM_ET; ++i)
        you.bondage[i] = 0;
}

static bool _two_handed()
{
    const item_def* wpn = you.slot_item(EQ_WEAPON, true);
    if (!wpn)
        return false;

    hands_reqd_type wep_type = hands_reqd(*wpn, you.body_size());
    return wep_type == HANDS_TWO || wep_type == HANDS_HALF
                                    && you.has_usable_offhand();
}

void ash_check_bondage(bool msg)
{
    if (you.religion != GOD_ASHENZARI)
        return;

    int cursed[NUM_ET] = {0}, slots[NUM_ET] = {0};

    for (int i = EQ_WEAPON; i < NUM_EQUIP; i++)
    {
        eq_type s;
        if (i == EQ_WEAPON)
            s = ET_WEAPON;
        else if (i == EQ_SHIELD)
            s= ET_SHIELD;
        else if (i <= EQ_MAX_ARMOUR)
            s = ET_ARMOUR;
        else
            s = ET_JEWELS;

        // transformed away slots are still considered to be possibly bound
        if (you_can_wear(i, true))
        {
            slots[s]++;
            if (you.equip[i] != -1)
            {
                const item_def& item = you.inv[you.equip[i]];
                if (item.cursed()
                    && (i != EQ_WEAPON
                        || item.base_type == OBJ_WEAPONS
                        || item.base_type == OBJ_STAVES))
                {
                    if (s == ET_WEAPON && _two_handed())
                    {
                        cursed[ET_WEAPON] = 3;
                        cursed[ET_SHIELD] = 3;
                    }
                    else
                        cursed[s]++;
                }
            }
        }
    }

    char new_bondage[NUM_ET];
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
    if (you.species == SP_CAT)
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
        for (int s = ET_WEAPON; s < NUM_ET; s++)
            if (new_bondage[s] != you.bondage[s])
                flags |= 1 << s;

    you.skill_boost.clear();
    for (int s = ET_WEAPON; s < NUM_ET; s++)
    {
        you.bondage[s] = new_bondage[s];
        std::map<skill_type, char> boosted_skills = ash_get_boosted_skills(eq_type(s));
        for (std::map<skill_type, char>::iterator it = boosted_skills.begin();
             it != boosted_skills.end(); it++)
        {
            you.skill_boost[it->first] += it->second;
            if (you.skill_boost[it->first] > 3)
                you.skill_boost[it->first] = 3;
        }

    }

    if (msg)
    {
        mpr(ash_describe_bondage(flags, you.bondage_level != old_level),
            MSGCH_GOD);
    }
}

std::string ash_describe_bondage(int flags, bool level)
{
    std::string desc;
    if (flags & ETF_WEAPON && flags & ETF_SHIELD
        && you.bondage[ET_WEAPON] != -1)
    {
        if (you.bondage[ET_WEAPON] == you.bondage[ET_SHIELD])
        {
            desc = make_stringf("Your hands are %sbound. ",
                                you.bondage[ET_WEAPON] ? "" : "not ");
        }
        else
        {
            desc = make_stringf("Your %s hand is bound but not your %s hand. ",
                                you.bondage[ET_WEAPON] ? "weapon" : "shield",
                                you.bondage[ET_WEAPON] ? "shield" : "weapon");
        }
    }
    else if (flags & ETF_WEAPON && you.bondage[ET_WEAPON] != -1)
    {
        desc = make_stringf("Your weapon hand is %sbound. ",
                            you.bondage[ET_WEAPON] ? "" : "not ");
    }
    else if (flags & ETF_SHIELD && you.bondage[ET_SHIELD] != -1)
    {
        desc = make_stringf("Your shield hand is %sbound. ",
                            you.bondage[ET_SHIELD] ? "" : "not ");
    }

    if (flags & ETF_ARMOUR && flags & ETF_JEWELS
        && you.bondage[ET_ARMOUR] == you.bondage[ET_JEWELS]
        && you.bondage[ET_ARMOUR] != -1)
    {
        desc += make_stringf("You are %s bound in armour and magic. ",
                             you.bondage[ET_ARMOUR] == 0 ? "not" :
                             you.bondage[ET_ARMOUR] == 1 ? "partially"
                                                         : "fully");
    }
    else
    {
        if (flags & ETF_ARMOUR && you.bondage[ET_ARMOUR] != -1)
            desc += make_stringf("You are %s bound in armour. ",
                                 you.bondage[ET_ARMOUR] == 0 ? "not" :
                                 you.bondage[ET_ARMOUR] == 1 ? "partially"
                                                             : "fully");

        if (flags & ETF_JEWELS && you.bondage[ET_JEWELS] != -1)
            desc += make_stringf("You are %s bound in magic. ",
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
        return worn->base_type == OBJ_WEAPONS || worn->base_type == OBJ_STAVES;
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
    case AMU_THE_GOURMAND:
        return (you.species != SP_MUMMY
                && player_mutation_level(MUT_HERBIVOROUS) < 3);
    case RING_INVISIBILITY:
    case RING_TELEPORTATION:
    case RING_MAGICAL_POWER:
    case RING_LEVITATION:
    case AMU_RAGE:
    case AMU_GUARDIAN_SPIRIT:
        return true;
    default:
        return false;
    }
}

bool ash_id_item(const coord_def p)
{
    if (const monster* mons = monster_at(p))
        if (mons_is_unknown_mimic(mons) && mons_is_item_mimic(mons->type))
            return ash_id_item(get_mimic_item(mons));

    if (you.visible_igrd(p) != NON_ITEM)
        return ash_id_item(mitm[you.visible_igrd(p)]);

    return false;
}

bool ash_id_item(item_def& item, bool silent)
{
    if (you.religion != GOD_ASHENZARI)
        return false;

    if (item.base_type == OBJ_JEWELLERY && item_needs_autopickup(item))
        item.props["needs_autopickup"] = true;

    iflags_t old_ided = item.flags & ISFLAG_IDENT_MASK;
    iflags_t ided = ISFLAG_KNOW_CURSE;

    if (item.base_type == OBJ_WEAPONS
        || item.base_type == OBJ_ARMOUR
        || item.base_type == OBJ_STAVES)
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

        if (item.props.exists("needs_autopickup") && is_useless_item(item))
            item.props.erase("needs_autopickup");

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

static bool _is_offensive_wand(const item_def& item)
{
    switch (item.sub_type)
    {
    // Monsters don't use those, so no need to warn the player about them.
    case WAND_ENSLAVEMENT:
    case WAND_RANDOM_EFFECTS:
    case WAND_DIGGING:

    // Monsters will use them on themselves.
    case WAND_HASTING:
    case WAND_HEALING:
    case WAND_INVISIBILITY:
        return false;

    case WAND_FLAME:
    case WAND_FROST:
    case WAND_SLOWING:
    case WAND_MAGIC_DARTS:
    case WAND_PARALYSIS:
    case WAND_FIRE:
    case WAND_COLD:
    case WAND_CONFUSION:
    case WAND_FIREBALL:
    case WAND_TELEPORTATION:
    case WAND_LIGHTNING:
    case WAND_POLYMORPH_OTHER:
    case WAND_DRAINING:
    case WAND_DISINTEGRATION:
        return true;
    }
    return false;
}

void ash_id_monster_equipment(monster* mon)
{
    if (you.religion != GOD_ASHENZARI)
        return;

    bool id = false;

    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (mon->inv[i] == NON_ITEM)
            continue;

        item_def &item = mitm[mon->inv[i]];
        if ((i != MSLOT_WAND || !_is_offensive_wand(item))
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

monster_type ash_monster_tier(const monster *mon)
{
    double factor = sqrt(exp_needed(you.experience_level) / 30.0);
    int tension = exper_value(mon) / (1 + factor);

    if (mon->friendly())
        return MONS_SENSED_FRIENDLY;
    else if (tension <= 0)
        // Conjurators use melee to conserve mana, MDFis switch plates...
        return MONS_SENSED_TRIVIAL;
    else if (tension <= 5)
        // An easy fight but not ignorable.
        return MONS_SENSED_EASY;
    else if (tension <= 32)
        // Hard but reasonable.
        return MONS_SENSED_TOUGH;
    else
        // Check all wands/jewels several times, wear brown pants...
        return MONS_SENSED_NASTY;
}

std::map<skill_type, char> ash_get_boosted_skills(eq_type type)
{
    const int bondage = you.bondage[type];
    std::map<skill_type, char> boost;
    if (!bondage)
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
        if(wpn->base_type == OBJ_WEAPONS)
        {
            boost[is_range_weapon(*wpn) ? range_skill(*wpn)
                                        : weapon_skill(*wpn)] = bondage;
        }

        // Those staves don't benefit from evocation.
        //Boost spellcasting instead.
        if (item_is_staff(*wpn) && (wpn->sub_type == STAFF_POWER
                                    || wpn->sub_type == STAFF_CONJURATION
                                    || wpn->sub_type == STAFF_ENCHANTMENT
                                    || wpn->sub_type == STAFF_ENERGY
                                    || wpn->sub_type == STAFF_WIZARDRY))
        {
            boost[SK_SPELLCASTING] = 2;
        }
        // Other staves use evocation.
        else if (item_is_staff(*wpn))
        {
            boost[SK_EVOCATIONS] = 1;
            boost[SK_STAVES] = 1;

        }
        else if (item_is_rod(*wpn))
            boost[SK_EVOCATIONS] = 2;

        break;

    case (ET_SHIELD):
        if (bondage == 2)
            boost[SK_SHIELDS] = 1;
        break;

    // Bonus for bounded armour depends on body armour type.
    case (ET_ARMOUR):
        if (evp < 2)
        {
            boost[SK_STEALTH] = bondage;
            boost[SK_DODGING] = bondage;
        }
        else if (evp < 4)
        {
            boost[SK_DODGING] = bondage;
            boost[SK_ARMOUR] = bondage;
        }
        else
            boost[SK_ARMOUR] = bondage + 1;
        break;

    // Boost all spell schools and evoc (to give some appeal to melee).
    case (ET_JEWELS):
        for (int i = SK_CONJURATIONS; i <= SK_POISON_MAGIC; ++i)
            boost[skill_type(i)] = bondage;
        boost[SK_EVOCATIONS] = bondage;
        break;

    default:
        die("Unknown equipment type.");
    }

    return boost;
}

int ash_skill_boost(skill_type sk)
{
    const int level = you.skills[sk];
    if (!level || !you.skill_boost[sk] || piety_rank() <= 2)
        return level;

    // 1 = low bonus    -> factor = 1
    // 2 = medium bonus -> factor = 1.25
    // 3 = high bonus   -> factor = 1.5
    const float piety_factor = (you.skill_boost[sk] + 3) / 4.0;
    const int base = std::min(piety_rank() - 1, you.bondage_level + 2);
    const int bonus = std::max<int>(0, base * piety_factor - level / 4.0);

    return std::min(level + bonus, 27);
}
