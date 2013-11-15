/**
 * @file
 * @brief Pseudo spells triggered by gods and various items.
**/

#include "AppHdr.h"

#include "externs.h"
#include "spl-goditem.h"

#include "artefact.h"
#include "attitude-change.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "decks.h"
#include "env.h"
#include "godconduct.h"
#include "godpassive.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "map_knowledge.h"
#include "mapdef.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tilepick.h"
#include "transform.h"
#include "traps.h"
#include "view.h"

int identify(int power, int item_slot, string *pre_msg)
{
    int id_used = 1;
    int identified = 0;

    // Scrolls of identify *may* produce "extra" identifications.
    if (power == -1 && one_chance_in(5))
        id_used += (coinflip()? 1 : 2);

    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item(
                "Identify which item? (\\ to view known items)",
                MT_INVLIST, OSEL_UNIDENT, true, true, false, 0,
                -1, NULL, OPER_ANY, true);
        }
        if (prompt_failed(item_slot))
            return identified;

        item_def& item(you.inv[item_slot]);

        if (fully_identified(item)
            && (!is_deck(item) || top_card_is_known(item)))
        {
            mpr("Choose an unidentified item, or Esc to abort.");
            if (Options.auto_list)
                more();
            item_slot = -1;
            continue;
        }

        if (pre_msg && identified == 0)
            mpr(pre_msg->c_str());

        set_ident_type(item, ID_KNOWN_TYPE);
        set_ident_flags(item, ISFLAG_IDENT_MASK);

        if (is_deck(item) && !top_card_is_known(item))
            deck_identify_first(item_slot);

        // For scrolls, now id the scroll, unless already known.
        if (power == -1
            && get_ident_type(OBJ_SCROLLS, SCR_IDENTIFY) != ID_KNOWN_TYPE)
        {
            set_ident_type(OBJ_SCROLLS, SCR_IDENTIFY, ID_KNOWN_TYPE);

            const int wpn = you.equip[EQ_WEAPON];
            if (wpn != -1
                && you.inv[wpn].base_type == OBJ_SCROLLS
                && you.inv[wpn].sub_type == SCR_IDENTIFY)
            {
                you.wield_change = true;
            }
        }

        // Output identified item.
        mpr_nocap(item.name(DESC_INVENTORY_EQUIP).c_str());
        if (item_slot == you.equip[EQ_WEAPON])
            you.wield_change = true;

        identified++;

        if (item.base_type == OBJ_JEWELLERY
            && item.sub_type == AMU_INACCURACY
            && item_slot == you.equip[EQ_AMULET]
            && !item_known_cursed(item))
        {
            learned_something_new(HINT_INACCURACY);
        }

        if (Options.auto_list && id_used > identified)
            more();

        // In case we get to try again.
        item_slot = -1;
    }
    while (id_used > identified);
    return identified;
}

static bool _mons_hostile(const monster* mon)
{
    // Needs to be done this way because of friendly/neutral enchantments.
    return !mon->wont_attack() && !mon->neutral();
}

// Check whether it is possible at all to pacify this monster.
// Returns -1, if monster can never be pacified.
// Returns -2, if monster can currently not be pacified (asleep).
// Returns 0, if it's possible to pacify this monster.
int is_pacifiable(const monster* mon)
{
    if (!you_worship(GOD_ELYVILON))
        return -1;

    // I was thinking of jellies when I wrote this, but maybe we shouldn't
    // exclude zombies and such... (jpeg)
    if (mons_intel(mon) <= I_INSECT // no self-awareness
        || mons_is_tentacle_or_tentacle_segment(mon->type)) // body part
    {
        return -1;
    }

    const mon_holy_type holiness = mon->holiness();

    if (!mon->is_holy()
        && holiness != MH_UNDEAD
        && holiness != MH_DEMONIC
        && holiness != MH_NATURAL)
    {
        return -1;
    }

    if (mon->is_stationary()) // not able to leave the level
        return -1;

    if (mon->asleep()) // not aware of what is happening
        return -2;

    return 0;
}

// Check whether this monster might be pacified.
// Returns 0, if monster can be pacified but the attempt failed.
// Returns 1, if monster is pacified.
// Returns -1, if monster can never be pacified.
// Returns -2, if monster can currently not be pacified (asleep).
// Returns -3, if monster can be pacified but the attempt narrowly failed.
// Returns -4, if monster can currently not be pacified (too much hp).
static int _can_pacify_monster(const monster* mon, const int healed,
                               const int max_healed)
{

    int pacifiable = is_pacifiable(mon);
    if (pacifiable < 0)
        return pacifiable;

    if (healed < 1)
        return 0;

    const int factor = (mons_intel(mon) <= I_ANIMAL)       ? 3 : // animals
                       (is_player_same_genus(mon->type))   ? 2   // same genus
                                                           : 1;  // other

    int divisor = 3;

    const mon_holy_type holiness = mon->holiness();
    if (mon->is_holy())
        divisor--;
    else if (holiness == MH_UNDEAD)
        divisor++;
    else if (holiness == MH_DEMONIC)
        divisor += 2;

    if (mon->max_hit_points > factor * ((you.skill(SK_INVOCATIONS, max_healed)
                                         + max_healed) / divisor))
    {
        return -4;
    }

    int random_factor = random2((you.skill(SK_INVOCATIONS, healed) + healed)
                                / divisor);

    dprf("pacifying %s? max hp: %d, factor: %d, Inv: %d, healed: %d, rnd: %d",
         mon->name(DESC_PLAIN).c_str(), mon->max_hit_points, factor,
         you.skill(SK_INVOCATIONS), healed, random_factor);

    if (mon->max_hit_points < factor * random_factor)
        return 1;
    if (mon->max_hit_points < factor * random_factor * 1.15)
        return -3;

    return 0;
}

static vector<string> _desc_mindless(const monster_info& mi)
{
    vector<string> descs;
    if (mi.intel() <= I_INSECT)
        descs.push_back("mindless");
    return descs;
}

// Returns: 1 -- success, 0 -- failure, -1 -- cancel
static int _healing_spell(int healed, int max_healed, bool divine_ability,
                          const coord_def& where, bool not_self,
                          targ_mode_type mode)
{
    ASSERT(healed >= 1);

    bolt beam;
    dist spd;

    if (where.origin())
    {
        spd.isValid = spell_direction(spd, beam, DIR_TARGET,
                                      mode != TARG_NUM_MODES ? mode :
                                      you_worship(GOD_ELYVILON) ?
                                            TARG_ANY : TARG_FRIEND,
                                      LOS_RADIUS, false, true, true, "Heal",
                                      NULL, false, NULL, _desc_mindless);
    }
    else
    {
        spd.target  = where;
        spd.isValid = in_bounds(spd.target);
    }

    if (!spd.isValid)
        return -1;

    if (spd.target == you.pos())
    {
        if (not_self)
        {
            mpr("You can only heal others!");
            return -1;
        }

        mpr("You are healed.");
        inc_hp(healed);
        return 1;
    }

    monster* mons = monster_at(spd.target);
    if (!mons)
    {
        canned_msg(MSG_NOTHING_THERE);
        // This isn't a cancel, to avoid leaking invisible monster
        // locations.
        return 0;
    }

    const int can_pacify  = _can_pacify_monster(mons, healed, max_healed);
    const bool is_hostile = _mons_hostile(mons);

    // Don't divinely heal a monster you can't pacify.
    if (divine_ability && is_hostile
        && you_worship(GOD_ELYVILON)
        && can_pacify <= 0)
    {
        if (can_pacify == 0)
        {
            mprf("The light of Elyvilon fails to reach %s.",
                 mons->name(DESC_THE).c_str());
            return 0;
        }
        else if (can_pacify == -3)
        {
            mprf("The light of Elyvilon almost touches upon %s.",
                 mons->name(DESC_THE).c_str());
            return 0;
        }
        else if (can_pacify == -4)
        {
            mprf("%s is completely unfazed by your meager offer of peace.",
                 mons->name(DESC_THE).c_str());
            return 0;
        }
        else
        {
            if (can_pacify == -2)
            {
                mprf("You cannot pacify this monster while %s is sleeping!",
                     mons->pronoun(PRONOUN_SUBJECTIVE).c_str());
            }
            else
                mpr("You cannot pacify this monster!");
            return -1;
        }
    }

    bool did_something = false;

    if (you_worship(GOD_ELYVILON)
        && can_pacify == 1
        && is_hostile)
    {
        did_something = true;

        const bool is_holy     = mons->is_holy();
        const bool is_summoned = mons->is_summoned();

        int pgain = 0;
        if (!is_holy && !is_summoned && you.piety < MAX_PIETY)
        {
            pgain = random2(mons->max_hit_points / (2 + you.piety / 20));
            if (!pgain) // Always give a 50% chance of gaining a little piety.
                pgain = coinflip();
        }

        // The feedback no longer tells you if you gained any piety this time,
        // it tells you merely the general rate.
        if (random2(1 + pgain))
            simple_god_message(" approves of your offer of peace.");
        else
            mpr("Elyvilon supports your offer of peace.");

        if (is_holy)
            good_god_holy_attitude_change(mons);
        else
        {
            simple_monster_message(mons, " turns neutral.");
            record_monster_defeat(mons, KILL_PACIFIED);
            mons_pacify(mons, ATT_NEUTRAL);

            // Give a small piety return.
            gain_piety(pgain);
        }
    }

    if (mons->heal(healed))
    {
        did_something = true;
        mprf("You heal %s.", mons->name(DESC_THE).c_str());

        if (mons->hit_points == mons->max_hit_points)
            simple_monster_message(mons, " is completely healed.");
        else
            print_wounds(mons);
    }

    if (!did_something)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return 0;
    }

    return 1;
}

// Returns: 1 -- success, 0 -- failure, -1 -- cancel
int cast_healing(int pow, int max_pow, bool divine_ability,
                 const coord_def& where, bool not_self, targ_mode_type mode)
{
    pow = min(50, pow);
    max_pow = min(50, max_pow);
    return _healing_spell(pow + roll_dice(2, pow) - 2, (3 * max_pow) - 2,
                          divine_ability, where, not_self, mode);
}

// Antimagic is sort of an anti-extension... it sets a lot of magical
// durations to 1 so it's very nasty at times (and potentially lethal,
// that's why we reduce flight to 2, so that the player has a chance
// to stop insta-death... sure the others could lead to death, but that's
// not as direct as falling into deep water) -- bwr
void antimagic()
{
    duration_type dur_list[] =
    {
        DUR_INVIS, DUR_CONF, DUR_PARALYSIS, DUR_HASTE, DUR_MIGHT, DUR_AGILITY,
        DUR_BRILLIANCE, DUR_CONFUSING_TOUCH, DUR_SURE_BLADE, DUR_CORONA,
        DUR_FIRE_SHIELD, DUR_ICY_ARMOUR, DUR_REPEL_MISSILES,
        DUR_SWIFTNESS, DUR_CONTROL_TELEPORT,
        DUR_TRANSFORMATION, DUR_DEATH_CHANNEL, DUR_DEFLECT_MISSILES,
        DUR_PHASE_SHIFT, DUR_WEAPON_BRAND, DUR_SILENCE,
        DUR_CONDENSATION_SHIELD, DUR_STONESKIN, DUR_RESISTANCE,
        DUR_SLAYING, DUR_STEALTH,
        DUR_MAGIC_SHIELD, DUR_PETRIFIED, DUR_LIQUEFYING, DUR_DARKNESS,
        DUR_SHROUD_OF_GOLUBRIA, DUR_DISJUNCTION, DUR_SENTINEL_MARK,
        DUR_ANTIMAGIC /*!*/, DUR_REGENERATION,
    };

    bool need_msg = false;

    if (!you.permanent_flight()
        && you.duration[DUR_FLIGHT] > 11)
    {
        you.duration[DUR_FLIGHT] = 11;
        need_msg = true;
    }

    if (you.duration[DUR_TELEPORT] > 0)
    {
        you.duration[DUR_TELEPORT] = 0;
        mpr("You feel strangely stable.", MSGCH_DURATION);
    }

    if (you.duration[DUR_PETRIFYING] > 0)
    {
        you.duration[DUR_PETRIFYING] = 0;
        mpr("You feel limber!", MSGCH_DURATION);
    }

    if (you.attribute[ATTR_DELAYED_FIREBALL])
    {
        you.attribute[ATTR_DELAYED_FIREBALL] = 0;
        mpr("Your charged fireball dissipates.", MSGCH_DURATION);
    }

    // Post-berserk slowing isn't magic, so don't remove that.
    if (you.duration[DUR_SLOW] > you.duration[DUR_EXHAUSTED])
        you.duration[DUR_SLOW] = max(you.duration[DUR_EXHAUSTED], 1);

    for (unsigned int i = 0; i < ARRAYSZ(dur_list); ++i)
    {
        if (you.duration[dur_list[i]] > 1)
        {
            you.duration[dur_list[i]] = 1;
            need_msg = true;
        }
    }

    bool danger = need_expiration_warning(you.pos());

    if (need_msg)
    {
        mprf(danger ? MSGCH_DANGER : MSGCH_WARN,
             "%sYour magical effects are unravelling.",
             danger ? "Careful! " : "");
    }

    contaminate_player(-1 * (1000 + random2(4000)));
}

int detect_traps(int pow)
{
    pow = min(50, pow);

    // Trap detection moved to traps.cc. -am
    const int range = 8 + random2(8) + pow;
    return reveal_traps(range);
}

// pow -1 for passive
int detect_items(int pow)
{
    int items_found = 0;
    int map_radius;
    if (pow >= 0)
        map_radius = 8 + random2(8) + pow;
    else
    {
        if (you_worship(GOD_ASHENZARI))
        {
            map_radius = min(you.piety / 20, LOS_RADIUS);
            if (map_radius <= 0)
                return 0;
        }
        else // MUT_JELLY_GROWTH
            map_radius = 6;
    }

    for (radius_iterator ri(you.pos(), map_radius, C_ROUND); ri; ++ri)
    {
        // Don't you love the 0,5 shop hack?
        if (!in_bounds(*ri))
            continue;

        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus item free).
        if (pow != -1 && env.map_knowledge(*ri).changed())
            continue;

        if (igrd(*ri) != NON_ITEM
            && !env.map_knowledge(*ri).item())
        {
            items_found++;
            env.map_knowledge(*ri).set_detected_item();
        }
    }

    return items_found;
}

static void _fuzz_detect_creatures(int pow, int *fuzz_radius, int *fuzz_chance)
{
    dprf("dc_fuzz: Power is %d", pow);

    pow = max(1, pow);

    *fuzz_radius = pow >= 50 ? 1 : 2;

    // Fuzz chance starts off at 100% and declines to a low of 10% for
    // obscenely powerful castings (pow caps around the 60 mark).
    *fuzz_chance = 100 - 90 * (pow - 1) / 59;
    if (*fuzz_chance < 10)
        *fuzz_chance = 10;
}

static bool _mark_detected_creature(coord_def where, monster* mon,
                                    int fuzz_chance, int fuzz_radius)
{
    bool found_good = false;

    if (fuzz_radius && x_chance_in_y(fuzz_chance, 100))
    {
        const int fuzz_diam = 2 * fuzz_radius + 1;

        coord_def place;
        // Try five times to find a valid placement, else we attempt to place
        // the monster where it really is (and may fail).
        for (int itry = 0; itry < 5; ++itry)
        {
            place.set(where.x + random2(fuzz_diam) - fuzz_radius,
                      where.y + random2(fuzz_diam) - fuzz_radius);

            if (!map_bounds(place))
                continue;

            // If the player would be able to see a monster at this location
            // don't place it there.
            if (you.see_cell(place))
                continue;

            // Try not to overwrite another detected monster.
            if (env.map_knowledge(place).detected_monster())
                continue;

            // Don't print monsters on terrain they cannot pass through,
            // not even if said terrain has since changed.
            if (!env.map_knowledge(place).changed()
                && mon->can_pass_through_feat(grd(place)))
            {
                found_good = true;
                break;
            }
        }

        if (found_good)
            where = place;
    }

    env.map_knowledge(where).set_detected_monster(mons_detected_base(mon->type));

    return found_good;
}

int detect_creatures(int pow, bool telepathic)
{
    int fuzz_radius = 0, fuzz_chance = 0;
    if (!telepathic)
        _fuzz_detect_creatures(pow, &fuzz_radius, &fuzz_chance);

    int creatures_found = 0;
    const int map_radius = 10 + random2(8) + pow;

    // Clear the map so detect creatures is more useful and the detection
    // fuzz is harder to analyse by averaging.
    clear_map(false);

    for (radius_iterator ri(you.pos(), map_radius, C_ROUND); ri; ++ri)
    {
        discover_mimic(*ri, false);
        if (monster* mon = monster_at(*ri))
        {
            // If you can see the monster, don't "detect" it elsewhere.
            if (!mons_near(mon) || !mon->visible_to(&you))
            {
                creatures_found++;
                _mark_detected_creature(*ri, mon, fuzz_chance, fuzz_radius);
            }
        }
    }

    return creatures_found;
}

static bool _selectively_remove_curse(string *pre_msg)
{
    bool used = false;

    while (1)
    {
        if (!any_items_to_select(OSEL_CURSED_WORN, false) && used)
        {
            mpr("You have uncursed all your worn items.");
            return used;
        }

        int item_slot = prompt_invent_item("Uncurse which item?", MT_INVLIST,
                                           OSEL_CURSED_WORN, true, true, false);
        if (prompt_failed(item_slot))
            return used;

        item_def& item(you.inv[item_slot]);

        if (!item.cursed()
            || !item_is_equipped(item)
            || &item == you.weapon() && !is_weapon(item))
        {
            mpr("Choose a cursed equipped item, or Esc to abort.");
            if (Options.auto_list)
                more();
            continue;
        }

        if (!used && pre_msg)
            mpr(*pre_msg);

        do_uncurse_item(item, true, false, false);
        used = true;
    }
}

bool remove_curse(bool alreadyknown, string *pre_msg)
{
    if (you_worship(GOD_ASHENZARI) && alreadyknown)
    {
        if (_selectively_remove_curse(pre_msg))
        {
            ash_check_bondage();
            return true;
        }
        else
            return false;
    }

    bool success = false;

    // Only cursed *weapons* in hand count as cursed. - bwr
    if (you.weapon() && is_weapon(*you.weapon())
        && you.weapon()->cursed())
    {
        // Also sets wield_change.
        do_uncurse_item(*you.weapon());
        success = true;
    }

    // Everything else uses the same paradigm - are we certain?
    // What of artefact rings and amulets? {dlb}:
    for (int i = EQ_WEAPON + 1; i < NUM_EQUIP; i++)
    {
        // Melded equipment can also get uncursed this way.
        if (you.equip[i] != -1 && you.inv[you.equip[i]].cursed())
        {
            do_uncurse_item(you.inv[you.equip[i]]);
            success = true;
        }
    }

    if (success)
    {
        if (pre_msg)
            mpr(*pre_msg);
        mpr("You feel as if something is helping you.");
        learned_something_new(HINT_REMOVED_CURSE);
    }
    else if (alreadyknown)
        mpr("None of your equipped items are cursed.", MSGCH_PROMPT);
    else
    {
        if (pre_msg)
            mpr(*pre_msg);
        canned_msg(MSG_NOTHING_HAPPENS);
    }

    return success;
}

static bool _selectively_curse_item(bool armour, string *pre_msg)
{
    while (1)
    {
        int item_slot = prompt_invent_item("Curse which item?", MT_INVLIST,
                                           armour ? OSEL_UNCURSED_WORN_ARMOUR
                                                  : OSEL_UNCURSED_WORN_JEWELLERY,
                                           true, true, false);
        if (prompt_failed(item_slot))
            return false;

        item_def& item(you.inv[item_slot]);

        if (item.cursed()
            || !item_is_equipped(item)
            || armour && item.base_type != OBJ_ARMOUR
            || !armour && item.base_type != OBJ_JEWELLERY)
        {
            mprf("Choose an uncursed equipped piece of %s, or Esc to abort.",
                 armour ? "armour" : "jewellery");
            if (Options.auto_list)
                more();
            continue;
        }

        if (pre_msg)
            mpr(*pre_msg);
        do_curse_item(item, false);
        return true;
    }
}

bool curse_item(bool armour, bool alreadyknown, string *pre_msg)
{
    // make sure there's something to curse first
    int count = 0;
    int affected = EQ_WEAPON;
    int min_type, max_type;
    if (armour)
        min_type = EQ_MIN_ARMOUR, max_type = EQ_MAX_ARMOUR;
    else
        min_type = EQ_LEFT_RING, max_type = EQ_RING_AMULET;
    for (int i = min_type; i <= max_type; i++)
    {
        if (you.equip[i] != -1 && !you.inv[you.equip[i]].cursed())
        {
            count++;
            if (one_chance_in(count))
                affected = i;
        }
    }

    if (affected == EQ_WEAPON)
    {
        if (you_worship(GOD_ASHENZARI) && alreadyknown)
        {
            mprf(MSGCH_PROMPT, "You aren't wearing any piece of uncursed %s.",
                 armour ? "armour" : "jewellery");
        }
        else
        {
            if (pre_msg)
                mpr(*pre_msg);
            canned_msg(MSG_NOTHING_HAPPENS);
        }

        return false;
    }

    if (you_worship(GOD_ASHENZARI) && alreadyknown)
        return _selectively_curse_item(armour, pre_msg);

    if (pre_msg)
        mpr(*pre_msg);
    // Make the name before we curse it.
    do_curse_item(you.inv[you.equip[affected]], false);
    learned_something_new(HINT_YOU_CURSED);
    return true;
}

static bool _do_imprison(int pow, const coord_def& where, bool zin)
{
    // power guidelines:
    // powc is roughly 50 at Evoc 10 with no godly assistance, ranging
    // up to 300 or so with godly assistance or end-level, and 1200
    // as more or less the theoretical maximum.
    int number_built = 0;

    const dungeon_feature_type safe_tiles[] =
    {
        DNGN_SHALLOW_WATER, DNGN_FLOOR, DNGN_OPEN_DOOR
    };

    bool proceed;
    monster *mon;
    string targname;

    if (zin)
    {
        // We need to get this now because we won't be able to see
        // the monster once the walls go up!
        mon = monster_at(where);
        targname = mon->name(DESC_THE);
        bool success = true;
        bool none_vis = true;

        vector<coord_def> veto_spots(8);
        for (adjacent_iterator ai(where); ai; ++ai)
            veto_spots.push_back(*ai);

        // Check that any adjacent creatures can be pushed out of the way.
        for (adjacent_iterator ai(where); ai; ++ai)
        {
            // The tile is occupied.
            if (actor *act = actor_at(*ai))
            {
                // Can't push ourselves.
                coord_def newpos;
                if (act->is_player()
                    || !get_push_space(*ai, newpos, act, true, &veto_spots))
                {
                    success = false;
                    if (you.can_see(act))
                        none_vis = false;
                    break;
                }
                else
                    veto_spots.push_back(newpos);
            }

            // Make sure we have a legitimate tile.
            proceed = false;
            for (unsigned int i = 0; i < ARRAYSZ(safe_tiles) && !proceed; ++i)
            {
                if (cell_is_solid(*ai) && !feat_is_opaque(grd(*ai)))
                {
                    success = false;
                    none_vis = false;
                    break;
                }
            }
        }

        if (!success)
        {
            mprf(none_vis ? "You briefly glimpse something next to %s."
                        : "You need more space to imprison %s.",
                targname.c_str());
            return false;
        }
    }

    for (adjacent_iterator ai(where); ai; ++ai)
    {
        // This is where power comes in.
        if (!zin && one_chance_in(pow / 3))
            continue;

        // The tile is occupied.
        if (zin)
        {
            if (actor* act = actor_at(*ai))
            {
                coord_def newpos;
                get_push_space(*ai, newpos, act, true);
                act->move_to_pos(newpos);
            }
        }

        // Make sure we have a legitimate tile.
        proceed = false;
        if (!zin && !monster_at(*ai))
        {
            for (unsigned int i = 0; i < ARRAYSZ(safe_tiles) && !proceed; ++i)
                if (grd(*ai) == safe_tiles[i] || feat_is_trap(grd(*ai), true))
                    proceed = true;
        }
        else if (zin && !cell_is_solid(*ai))
            proceed = true;

        if (proceed)
        {
            // All items are moved aside.
            if (igrd(*ai) != NON_ITEM)
            {
                coord_def newpos;
                get_push_space(*ai, newpos, NULL, true);
                move_items(*ai, newpos);
            }

            // All clouds are destroyed.
            if (env.cgrid(*ai) != EMPTY_CLOUD)
                delete_cloud(env.cgrid(*ai));

            // All traps are destroyed.
            if (trap_def *ptrap = find_trap(*ai))
            {
                ptrap->destroy();
                grd(*ai) = DNGN_FLOOR;
            }

            // Actually place the wall.
            if (zin)
            {
                map_wiz_props_marker *marker = new map_wiz_props_marker(*ai);
                marker->set_property("feature_description", "a gleaming silver wall");
                env.markers.add(marker);

                temp_change_terrain(*ai, DNGN_METAL_WALL, INFINITE_DURATION,
                                    TERRAIN_CHANGE_IMPRISON);

                // Make the walls silver.
                env.grid_colours(*ai) = WHITE;
                env.tile_flv(*ai).feat_idx =
                        store_tilename_get_index("dngn_silver_wall");
                env.tile_flv(*ai).feat = TILE_DNGN_SILVER_WALL;
                if (env.map_knowledge(*ai).seen())
                {
                    env.map_knowledge(*ai).set_feature(DNGN_METAL_WALL);
                    env.map_knowledge(*ai).clear_item();
#ifdef USE_TILE
                    env.tile_bk_bg(*ai) = TILE_DNGN_SILVER_WALL;
                    env.tile_bk_fg(*ai) = 0;
#endif
                }
            }
            // Tomb card
            else
            {
                temp_change_terrain(*ai, DNGN_ROCK_WALL, INFINITE_DURATION,
                                    TERRAIN_CHANGE_TOMB);
            }

            number_built++;
        }
    }

    if (number_built > 0)
    {
        if (zin)
        {
            mprf("Zin imprisons %s with walls of pure silver!",
                 targname.c_str());
        }
        else
            mpr("Walls emerge from the floor!");

        you.update_beholders();
        you.update_fearmongers();
        env.markers.clear_need_activate();
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return number_built > 0;
}

bool entomb(int pow)
{
    // Zotdef - turned off
    if (crawl_state.game_is_zotdef())
    {
        mpr("The dungeon rumbles ominously, and rocks fall from the ceiling!");
        return false;
    }
    if (_do_imprison(pow, you.pos(), false))
    {
        const int tomb_duration = BASELINE_DELAY * pow;
        env.markers.add(new map_tomb_marker(you.pos(),
                                            tomb_duration,
                                            you.mindex(),
                                            you.mindex()));
        env.markers.clear_need_activate(); // doesn't need activation
        return true;
    }

    return false;
}

bool cast_imprison(int pow, monster* mons, int source)
{
    if (_do_imprison(pow, mons->pos(), true))
    {
        const int tomb_duration = BASELINE_DELAY * pow;
        env.markers.add(new map_tomb_marker(mons->pos(),
                                            tomb_duration,
                                            source,
                                            mons->mindex()));
        env.markers.clear_need_activate(); // doesn't need activation
        return true;
    }

    return false;
}

bool cast_smiting(int pow, monster* mons)
{
    if (mons == NULL || mons->submerged())
    {
        canned_msg(MSG_NOTHING_THERE);
        // Counts as a real cast, due to victory-dancing and
        // invisible/submerged monsters.
        return true;
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool success = !stop_attack_prompt(mons, false, you.pos());

    if (success)
    {
        set_attack_conducts(conducts, mons);

        mprf("You smite %s!", mons->name(DESC_THE).c_str());

        behaviour_event(mons, ME_ANNOY, &you);
    }

    enable_attack_conducts(conducts);

    if (success)
    {
        // Maxes out at around 40 damage at 27 Invocations, which is
        // plenty in my book (the old max damage was around 70, which
        // seems excessive).
        mons->hurt(&you, 7 + (random2(pow) * 33 / 191));
        if (mons->alive())
            print_wounds(mons);
    }

    return success;
}
