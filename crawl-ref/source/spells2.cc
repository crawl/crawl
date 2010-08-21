/*
 *  File:       spells2.cc
 *  Summary:    Implementations of some additional spells.
 *              Mostly Necromancy and Summoning.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "spells2.h"
#include "externs.h"

#include "artefact.h"
#include "beam.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "effects.h"
#include "env.h"
#include "godconduct.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "map_knowledge.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "ouch.h"
#include "quiver.h"
#include "shout.h"
#include "stuff.h"
#include "traps.h"
#include "view.h"

int detect_traps(int pow)
{
    pow = std::min(50, pow);

    // Trap detection moved to traps.cc.  -am

    const int range = 8 + random2(8) + pow;
    return reveal_traps(range);
}

int detect_items(int pow)
{
    int items_found = 0;
    const int map_radius = 8 + random2(8) + pow;

    for (radius_iterator ri(you.pos(), map_radius, C_SQUARE); ri; ++ri)
    {
        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus item free).
        if (env.map_knowledge(*ri).changed())
            continue;

        if (igrd(*ri) != NON_ITEM
            && !env.map_knowledge(*ri).item())
        {
            items_found++;
            env.map_knowledge(*ri).set_detected_item();
        }
    }

    return (items_found);
}

static void _fuzz_detect_creatures(int pow, int *fuzz_radius, int *fuzz_chance)
{
    dprf("dc_fuzz: Power is %d", pow);

    pow = std::max(1, pow);

    *fuzz_radius = pow >= 50 ? 1 : 2;

    // Fuzz chance starts off at 100% and declines to a low of 10% for
    // obscenely powerful castings (pow caps around the 60 mark).
    *fuzz_chance = 100 - 90 * (pow - 1) / 59;
    if (*fuzz_chance < 10)
        *fuzz_chance = 10;
}

static bool _mark_detected_creature(coord_def where, const monsters *mon,
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

            // If the player would be able to see a monster at this location
            // don't place it there.
            if (you.see_cell(place))
                continue;

            // Try not to overwrite another detected monster.
            if (env.map_knowledge(place).detected_monster())
                continue;

            // Don't print monsters on terrain they cannot pass through,
            // not even if said terrain has since changed.
            if (map_bounds(place) && !env.map_knowledge(place).changed()
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

    return (found_good);
}

int detect_creatures(int pow, bool telepathic)
{
    int fuzz_radius = 0, fuzz_chance = 0;
    if (!telepathic)
        _fuzz_detect_creatures(pow, &fuzz_radius, &fuzz_chance);

    int creatures_found = 0;
    const int map_radius = 8 + random2(8) + pow;

    // Clear the map so detect creatures is more useful and the detection
    // fuzz is harder to analyse by averaging.
    if (!telepathic)
        clear_map(false);

    for (radius_iterator ri(you.pos(), map_radius, C_SQUARE); ri; ++ri)
    {
        if (monsters *mon = monster_at(*ri))
        {
            // If you can see the monster, don't "detect" it elsewhere.
            if (!mons_near(mon) || !mon->visible_to(&you))
            {
                creatures_found++;
                _mark_detected_creature(*ri, mon, fuzz_chance, fuzz_radius);
            }

            // Assuming that highly intelligent spellcasters can
            // detect scrying. -- bwr
            if (mons_intel(mon) == I_HIGH
                && mons_class_flag(mon->type, M_SPELLCASTER))
            {
                behaviour_event(mon, ME_DISTURB, MHITYOU, you.pos());
            }
        }
    }

    return (creatures_found);
}

// We need to know what brands equate with what missile brands to know if
// we should disallow temporary branding or not.
static special_missile_type _convert_to_missile(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_NORMAL: return SPMSL_NORMAL;
    case SPWPN_FLAME: // deliberate fall through
    case SPWPN_FLAMING: return SPMSL_FLAME;
    case SPWPN_FROST: // deliberate fall through
    case SPWPN_FREEZING: return SPMSL_FROST;
    case SPWPN_VENOM: return SPMSL_POISONED;
    case SPWPN_CHAOS: return SPMSL_CHAOS;
    case SPWPN_RETURNING: return SPMSL_RETURNING;
    default: return SPMSL_NORMAL; // there are no equivalents for the rest
                                  // of the ammo brands.
    }
}

// Some launchers need to convert different brands.
static brand_type _convert_to_launcher(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_FREEZING: return SPWPN_FROST; case SPWPN_FLAMING: return SPWPN_FLAME;
    default: return (which_brand);
    }
}

static bool _ok_for_launchers(brand_type which_brand)
{
    switch (which_brand)
    {
    case SPWPN_NORMAL:
    case SPWPN_FREEZING:
    case SPWPN_FROST:
    case SPWPN_FLAMING:
    case SPWPN_FLAME:
    case SPWPN_VENOM:
    //case SPWPN_PAIN: -- no pain missile type yet
    case SPWPN_RETURNING:
    case SPWPN_CHAOS:
        return (true);
    default:
        return (false);
    }
}

bool brand_weapon(brand_type which_brand, int power)
{
    if (!you.weapon())
        return (false);

    bool temp_brand = you.duration[DUR_WEAPON_BRAND];
    item_def& weapon = *you.weapon();

    // Can't brand non-weapons, but can brand some launchers (see later).
    if (weapon.base_type != OBJ_WEAPONS)
        return (false);

    // But not blowguns.
    if (weapon.sub_type == WPN_BLOWGUN)
        return (false);

    // Can't brand artefacts.
    if (is_artefact(weapon))
        return (false);

    // Can't brand already-branded items.
    if (!temp_brand && get_weapon_brand(weapon) != SPWPN_NORMAL)
        return (false);

    // Some brandings are restricted to certain damage types.
    const int wpn_type = get_vorpal_type(weapon);
    if (which_brand == SPWPN_VENOM && wpn_type == DVORP_CRUSHING
        || which_brand == SPWPN_VORPAL && wpn_type != DVORP_SLICING
                                       && wpn_type != DVORP_STABBING
        || which_brand == SPWPN_DUMMY_CRUSHING && wpn_type != DVORP_CRUSHING)
    {
        return (false);
    }

    // Can only brand launchers with sensible brands.
    if (is_range_weapon(weapon))
    {
        // If the new missile type wouldn't match the launcher, say no.
        missile_type missile = fires_ammo_type(weapon);

        // XXX: To deal with the fact that is_missile_brand_ok will be
        // unhappy if we attempt to brand stones, tell it we're using
        // sling bullets instead.
        if (weapon.sub_type == WPN_SLING)
            missile = MI_SLING_BULLET;

        if (!is_missile_brand_ok(missile, _convert_to_missile(which_brand), true))
            return (false);

        // If the brand isn't appropriate for that launcher, also say no.
        if (!_ok_for_launchers(which_brand))
            return (false);

        // Otherwise, convert to the correct brand type, most specifically (but
        // not necessarily only) flaming -> flame, freezing -> frost.
        which_brand = _convert_to_launcher(which_brand);
    }

    // Allow rebranding a temporarily-branded item to a different brand.
    if (temp_brand && (get_weapon_brand(weapon) != which_brand))
    {
        you.duration[DUR_WEAPON_BRAND] = 0;
        set_item_ego_type(weapon, OBJ_WEAPONS, SPWPN_NORMAL);
        temp_brand = false;
    }

    std::string msg = weapon.name(DESC_CAP_YOUR);

    bool emit_special_message = !temp_brand;
    int duration_affected = 0;
    switch (which_brand)
    {
    case SPWPN_FLAME:
    case SPWPN_FLAMING:
        msg += " bursts into flame!";
        duration_affected = 7;
        break;

    case SPWPN_FROST:
        msg += " frosts over!";
        duration_affected = 7;
        break;

    case SPWPN_FREEZING:
        msg += " glows blue.";
        duration_affected = 7;
        break;

    case SPWPN_VENOM:
        msg += " starts dripping with poison.";
        duration_affected = 15;
        break;

    case SPWPN_DRAINING:
        msg += " crackles with unholy energy.";
        duration_affected = 12;
        break;

    case SPWPN_VORPAL:
        msg += " glows silver and looks extremely sharp.";
        duration_affected = 10;
        break;

    case SPWPN_DISTORTION:      //jmf: Added for Warp Weapon.
        msg += " seems to ";
        msg += random_choose_string("twist", "bend", "vibrate",
                                    "flex", "wobble", "twang", NULL);
        msg += (coinflip() ? " oddly." : " strangely.");
        duration_affected = 5;

        // [dshaligram] Clamping power to 2.
        power = 2;
        break;

    case SPWPN_PAIN:
        // Well, in theory, we could be silenced, but then how are
        // we casting the brand spell?
        msg += " shrieks in agony.";
        noisy(15, you.pos());
        duration_affected = 8;
        // We must repeat the special message here (as there's a side effect.)
        emit_special_message = true;
        break;

    case SPWPN_DUMMY_CRUSHING:  //jmf: Added for Maxwell's Silver Hammer.
        which_brand = SPWPN_VORPAL;
        msg += " glows silver and feels heavier.";
        duration_affected = 7;
        break;

    case SPWPN_RETURNING:
        msg += " wiggles in your hand.";
        duration_affected = 5;
        break;

    default:
        break;
    }

    if (!temp_brand)
    {
        set_item_ego_type(weapon, OBJ_WEAPONS, which_brand);
        you.wield_change = true;
    }

    if (emit_special_message)
        mpr(msg.c_str());
    else
        mprf("%s flashes.", weapon.name(DESC_CAP_YOUR).c_str());

    you.increase_duration(DUR_WEAPON_BRAND,
                          duration_affected + roll_dice(2, power), 50);

    return (true);
}

typedef std::pair<const monsters*,int> counted_monster;
typedef std::vector<counted_monster> counted_monster_list;
static void _record_monster_by_name(counted_monster_list &list,
                                    const monsters *mons)
{
    const std::string name = mons->name(DESC_PLAIN);
    for (counted_monster_list::iterator i = list.begin(); i != list.end(); ++i)
    {
        if (i->first->name(DESC_PLAIN) == name)
        {
            i->second++;
            return;
        }
    }
    list.push_back(counted_monster(mons, 1));
}

static int _monster_count(const counted_monster_list &list)
{
    int nmons = 0;
    for (counted_monster_list::const_iterator i = list.begin();
         i != list.end(); ++i)
    {
        nmons += i->second;
    }
    return (nmons);
}

static std::string _describe_monsters(const counted_monster_list &list)
{
    std::ostringstream out;

    description_level_type desc = DESC_CAP_THE;
    for (counted_monster_list::const_iterator i = list.begin();
         i != list.end(); desc = DESC_NOCAP_THE)
    {
        const counted_monster &cm(*i);
        if (i != list.begin())
        {
            ++i;
            out << (i == list.end() ? " and " : ", ");
        }
        else
            ++i;

        const std::string name =
            cm.second > 1 ? pluralise(cm.first->name(desc))
                          : cm.first->name(desc);
        out << name;
    }
    return (out.str());
}

// Poisonous light passes right through invisible players
// and monsters, and so, they are unaffected by this spell --
// assumes only you can cast this spell (or would want to).
void cast_toxic_radiance(bool non_player)
{
    if (non_player)
        mpr("The air is filled with a sickly green light!");
    else
        mpr("You radiate a sickly green light!");

    flash_view(GREEN);
    more();
    mesclr();

    // Determine whether the player is hit by the radiance. {dlb}
    if (you.duration[DUR_INVIS])
    {
        mpr("The light passes straight through your body.");
    }
    else if (!player_res_poison())
    {
        mpr("You feel rather sick.");
        poison_player(2, "", "toxic radiance");
    }

    counted_monster_list affected_monsters;
    // determine which monsters are hit by the radiance: {dlb}
    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        if (!mi->submerged())
        {
            // Monsters affected by corona are still invisible in that
            // radiation passes through them without affecting them. Therefore,
            // this check should not be !monster->invisible().
            if (!mi->has_ench(ENCH_INVIS))
            {
                kill_category kc = KC_YOU;
                if (non_player)
                    kc = KC_OTHER;
                bool affected =
                    poison_monster(*mi, kc, 1, false, false);

                if (coinflip() && poison_monster(*mi, kc, false, false))
                    affected = true;

                if (affected)
                    _record_monster_by_name(affected_monsters, *mi);
            }
            else if (you.can_see_invisible())
            {
                // message player re:"miss" where appropriate {dlb}
                mprf("The light passes through %s.",
                     mi->name(DESC_NOCAP_THE).c_str());
            }
        }
    }

    if (!affected_monsters.empty())
    {
        const std::string message =
            make_stringf("%s %s poisoned.",
                         _describe_monsters(affected_monsters).c_str(),
                         _monster_count(affected_monsters) == 1? "is" : "are");
        if (static_cast<int>(message.length()) < get_number_of_cols() - 2)
            mpr(message.c_str());
        else
        {
            // Exclamation mark to suggest that a lot of creatures were
            // affected.
            if (non_player)
                mpr("Nearby monsters are poisoned!");
            else
                mpr("The monsters around you are poisoned!");
        }
    }
}

void cast_refrigeration(int pow, bool non_player)
{
    if (non_player)
        mpr("Something drains the heat from around you.");
    else
        mpr("The heat is drained from your surroundings.");

    flash_view(LIGHTCYAN);
    more();
    mesclr();

    // Handle the player.
    const dice_def dam_dice(3, 5 + pow / 10);
    const int hurted = check_your_resists(dam_dice.roll(), BEAM_COLD,
            "refrigeration");

    if (hurted > 0)
    {
        mpr("You feel very cold.");
        ouch(hurted, NON_MONSTER, KILLED_BY_FREEZING);

        // Note: this used to be 12!... and it was also applied even if
        // the player didn't take damage from the cold, so we're being
        // a lot nicer now.  -- bwr
        expose_player_to_element(BEAM_COLD, 5);
    }

    // Now do the monsters.

    // First build the message.
    counted_monster_list affected_monsters;

    for (monster_iterator mi(&you); mi; ++mi)
        _record_monster_by_name(affected_monsters, *mi);

    if (!affected_monsters.empty())
    {
        const std::string message =
            make_stringf("%s %s frozen.",
                         _describe_monsters(affected_monsters).c_str(),
                         _monster_count(affected_monsters) == 1? "is" : "are");
        if (static_cast<int>(message.length()) < get_number_of_cols() - 2)
            mpr(message.c_str());
        else
        {
            // Exclamation mark to suggest that a lot of creatures were
            // affected.
            mpr("The monsters around you are frozen!");
        }
    }

    // Now damage the creatures.

    // Set up the cold attack.
    bolt beam;
    beam.flavour = BEAM_COLD;
    beam.thrower = KILL_YOU;

    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        // Note that we *do* hurt monsters which you can't see
        // (submerged, invisible) even though you get no information
        // about it.

        // Calculate damage and apply.
        int hurt = mons_adjust_flavoured(*mi, beam, dam_dice.roll());
        if (non_player)
            mi->hurt(NULL, hurt, BEAM_COLD);
        else
            mi->hurt(&you, hurt, BEAM_COLD);

        // Cold-blooded creatures can be slowed.
        if (mi->alive()
            && mons_class_flag(mi->type, M_COLD_BLOOD)
            && coinflip())
        {
            mi->add_ench(ENCH_SLOW);
        }
    }
}

bool vampiric_drain(int pow, monsters *monster)
{
    if (monster == NULL || monster->submerged())
    {
        mpr("There isn't anything there!");
        // Cost to disallow freely locating invisible monsters.
        return (true);
    }

    if (monster->observable() && monster->undead_or_demonic())
    {
        mpr("Draining that being is not a good idea.");
        return (false);
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool success = !stop_attack_prompt(monster, false, you.pos());

    if (success)
    {
        set_attack_conducts(conducts, monster);

        behaviour_event(monster, ME_WHACK, MHITYOU, you.pos());
    }

    enable_attack_conducts(conducts);

    if (!success)
        return (false);
        
    if (!monster->alive())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (true);
    }

    // Monster might be invisible or player misled.
    if (monster->undead_or_demonic())
    {
        mpr("Aaaarggghhhhh!");
        dec_hp(random2avg(39, 2) + 10, false, "vampiric drain backlash");
        return (true);
    }

    if (monster->holiness() != MH_NATURAL
        || monster->res_negative_energy())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (true);
    }

    // The practical maximum of this is about 25 (pow @ 100). - bwr
    int hp_gain = 3 + random2avg(9, 2) + random2(pow) / 7;

        hp_gain = std::min(monster->hit_points, hp_gain);
        hp_gain = std::min(you.hp_max - you.hp, hp_gain);

    if (!hp_gain)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (true);
    }

    const bool mons_was_summoned = monster->is_summoned();

    monster->hurt(&you, hp_gain);

    if (monster->alive())
        print_wounds(monster);

    hp_gain /= 2;

    if (hp_gain && !mons_was_summoned)
    {
        mpr("You feel life coursing into your body.");
        inc_hp(hp_gain, false);
    }

    return (true);
}

bool burn_freeze(int pow, beam_type flavour, monsters *monster)
{
    pow = std::min(25, pow);

    if (monster == NULL || monster->submerged())
    {
        mpr("There isn't anything close enough!");
        // If there's no monster there, you still pay the costs in
        // order to prevent locating invisible monsters.
        return (true);
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool success = !stop_attack_prompt(monster, false, you.pos());

    if (success)
    {
        set_attack_conducts(conducts, monster);

        mprf("You %s %s.",
             (flavour == BEAM_FIRE)        ? "burn" :
             (flavour == BEAM_COLD)        ? "freeze" :
             (flavour == BEAM_MISSILE)     ? "crush" :
             (flavour == BEAM_ELECTRICITY) ? "zap"
                                           : "______",
             monster->name(DESC_NOCAP_THE).c_str());

        behaviour_event(monster, ME_ANNOY, MHITYOU);
    }

    enable_attack_conducts(conducts);

    if (!success)
        return (false);

    bolt beam;
    beam.flavour = flavour;
    beam.thrower = KILL_YOU;

    const int orig_hurted = roll_dice(1, 3 + pow / 3);
    int hurted = mons_adjust_flavoured(monster, beam, orig_hurted);
    monster->hurt(&you, hurted);

    if (monster->alive())
    {
        monster->expose_to_element(flavour, orig_hurted);
        print_wounds(monster);

        if (flavour == BEAM_COLD)
        {
            const int cold_res = monster->res_cold();
            if (cold_res <= 0)
            {
                const int stun = (1 - cold_res) * random2(2 + pow/5);
                monster->speed_increment -= stun;
            }
        }
    }

    return (true);
}
