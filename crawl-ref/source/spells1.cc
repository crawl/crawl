/*
 *  File:       spells1.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "spells1.h"
#include "externs.h"

#include "beam.h"
#include "coord.h"
#include "effects.h"
#include "env.h"
#include "godconduct.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "los.h"
#include "message.h"
#include "misc.h"
#include "mon-iter.h"
#include "religion.h"
#include "shout.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"
#include "viewchar.h"

bool fireball(int pow, bolt &beam)
{
    return (zapping(ZAP_FIREBALL, pow, beam, true));
}

void setup_fire_storm(const actor *source, int pow, bolt &beam)
{
    beam.name         = "great blast of fire";
    beam.ex_size      = 2 + (random2(pow) > 75);
    beam.flavour      = BEAM_LAVA;
    beam.real_flavour = beam.flavour;
    beam.glyph        = dchar_glyph(DCHAR_FIRED_ZAP);
    beam.colour       = RED;
    beam.beam_source  = source->mindex();
    // XXX: Should this be KILL_MON_MISSILE?
    beam.thrower      =
        source->atype() == ACT_PLAYER ? KILL_YOU_MISSILE : KILL_MON;
    beam.aux_source.clear();
    beam.obvious_effect = false;
    beam.is_beam      = false;
    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.ench_power   = pow;      // used for radius
    beam.hit          = 20 + pow / 10;
    beam.damage       = calc_dice(8, 5 + pow);
}

bool cast_fire_storm(int pow, bolt &beam)
{
    if (grid_distance(beam.target, beam.source) > beam.range)
        return (false);

    setup_fire_storm(&you, pow, beam);

    mpr("A raging storm of fire appears!");

    beam.explode(false);

    viewwindow();
    return (true);
}

// No setup/cast split here as monster hellfire is completely different.
// Sad, but needed to maintain balance - monster hellfirers get asymmetric
// torment too.
bool cast_hellfire_burst(int pow, bolt &beam)
{
    beam.name              = "burst of hellfire";
    beam.aux_source        = "burst of hellfire";
    beam.ex_size           = 1;
    beam.flavour           = BEAM_HELLFIRE;
    beam.real_flavour      = beam.flavour;
    beam.glyph             = dchar_glyph(DCHAR_FIRED_BURST);
    beam.colour            = RED;
    beam.beam_source       = MHITYOU;
    beam.thrower           = KILL_YOU;
    beam.obvious_effect    = false;
    beam.is_beam           = false;
    beam.is_explosion      = true;
    beam.ench_power        = pow;      // used for radius
    beam.hit               = 20 + pow / 10;
    beam.damage            = calc_dice(6, 30 + pow);
    beam.can_see_invis     = you.can_see_invisible();
    beam.smart_monster     = true;
    beam.attitude          = ATT_FRIENDLY;
    beam.friend_info.count = 0;
    beam.is_tracer         = true;

    beam.explode(false);

    if (beam.beam_cancelled)
    {
        canned_msg(MSG_OK);
        return (false);
    }

    mpr("You call forth a pillar of hellfire!");

    beam.is_tracer = false;
    beam.in_explosion_phase = false;
    beam.explode(true);

    return (true);
}

static bool _lightning_los(const coord_def& source, const coord_def& target)
{
    // XXX: currently bounded by circular LOS radius;
    // XXX: adapt opacity -- allow passing clouds.
    return (exists_ray(source, target, opc_solid,
                       circle_def(LOS_MAX_RADIUS, C_ROUND)));
}

void cast_chain_lightning(int pow, const actor *caster)
{
    bolt beam;

    // initialise beam structure
    beam.name           = "lightning arc";
    beam.aux_source     = "chain lightning";
    beam.beam_source    = caster->mindex();
    beam.thrower        = (caster == &you) ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
    beam.range          = 8;
    beam.hit            = AUTOMATIC_HIT;
    beam.glyph          = dchar_glyph(DCHAR_FIRED_ZAP);
    beam.flavour        = BEAM_ELECTRICITY;
    beam.obvious_effect = true;
    beam.is_beam        = false;       // since we want to stop at our target
    beam.is_explosion   = false;
    beam.is_tracer      = false;

    if (const monsters *monster = caster->as_monster())
        beam.source_name = monster->name(DESC_PLAIN, true);

    bool first = true;
    coord_def source, target;

    for (source = caster->pos(); pow > 0;
         pow -= 8 + random2(13), source = target)
    {
        // infinity as far as this spell is concerned
        // (Range - 1) is used because the distance is randomised and
        // may be shifted by one.
        int min_dist = MONSTER_LOS_RANGE - 1;

        int dist;
        int count = 0;

        target.x = -1;
        target.y = -1;

        for (monster_iterator mi; mi; ++mi)
        {
            if (invalid_monster(*mi))
                continue;

            dist = grid_distance(source, mi->pos());

            // check for the source of this arc
            if (!dist)
                continue;

            // randomise distance (arcs don't care about a couple of feet)
            dist += (random2(3) - 1);

            // always ignore targets further than current one
            if (dist > min_dist)
                continue;

            if (!_lightning_los(source, mi->pos()))
                continue;

            count++;

            if (dist < min_dist)
            {
                // switch to looking for closer targets (but not always)
                if (!one_chance_in(10))
                {
                    min_dist = dist;
                    target = mi->pos();
                    count = 0;
                }
            }
            else if (target.x == -1 || one_chance_in(count))
            {
                // either first target, or new selected target at min_dist
                target = mi->pos();

                // need to set min_dist for first target case
                dist = std::max(dist, min_dist);
            }
        }

        // now check if the player is a target
        dist = grid_distance(source, you.pos());

        if (dist)       // i.e., player was not the source
        {
            // distance randomised (as above)
            dist += (random2(3) - 1);

            // select player if only, closest, or randomly selected
            if ((target.x == -1
                    || dist < min_dist
                    || (dist == min_dist && one_chance_in(count + 1)))
                && _lightning_los(source, you.pos()))
            {
                target = you.pos();
            }
        }

        if (caster == &you)
        {
            monsters *monster = monster_at(target);
            if (monster)
            {
                if (stop_attack_prompt(monster, false, you.pos()))
                    return;
            }
        }

        const bool see_source = you.see_cell( source );
        const bool see_targ   = you.see_cell( target );

        if (target.x == -1)
        {
            if (see_source)
                mpr("The lightning grounds out.");

            break;
        }

        // Trying to limit message spamming here so we'll only mention
        // the thunder at the start or when it's out of LoS.
        const char* msg = "You hear a mighty clap of thunder!";
        noisy(25, source, (first || !see_source) ? msg : NULL);
        first = false;

        if (see_source && !see_targ)
            mpr("The lightning arcs out of your line of sight!");
        else if (!see_source && see_targ)
            mpr("The lightning arc suddenly appears!");

        if (!you.see_cell_no_trans( target ))
        {
            // It's no longer in the caster's LOS and influence.
            pow = pow / 2 + 1;
        }

        beam.source = source;
        beam.target = target;
        beam.colour = LIGHTBLUE;
        beam.damage = calc_dice(5, 12 + pow * 2 / 3);

        // Be kinder to the caster.
        if (target == caster->pos())
        {
            if (!(beam.damage.num /= 2))
                beam.damage.num = 1;
            if ((beam.damage.size /= 2) < 3)
                beam.damage.size = 3;
        }
        beam.fire();
    }

    more();
}

void cast_cure_poison(int pow)
{
    if (you.duration[DUR_POISONING] > 0)
        reduce_poison_player(2 + random2(pow) + random2(3));
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

int allowed_deaths_door_hp(void)
{
    int hp = you.skills[SK_NECROMANCY] / 2;

    if (you.religion == GOD_KIKUBAAQUDGHA && !player_under_penance())
        hp += you.piety / 15;

    return (hp);
}

void cast_deaths_door(int pow)
{
    if (you.is_undead)
        mpr("You're already dead!");
    else if (you.duration[DUR_DEATHS_DOOR])
        mpr("Your appeal for an extension has been denied.");
    else
    {
        mpr("You feel invincible!");
        mpr("You seem to hear sand running through an hourglass...",
            MSGCH_SOUND);

        set_hp( allowed_deaths_door_hp(), false );
        deflate_hp( you.hp_max, false );

        you.set_duration(DUR_DEATHS_DOOR, 10 + random2avg(13, 3)
                                           + (random2(pow) / 10));

        if (you.duration[DUR_DEATHS_DOOR] > 25 * BASELINE_DELAY)
            you.duration[DUR_DEATHS_DOOR] = (23 + random2(5)) * BASELINE_DELAY;
    }

    return;
}

void abjuration(int pow)
{
    mpr("Send 'em back where they came from!");

    // Scale power into something comparable to summon lifetime.
    const int abjdur = pow * 12;

    for (monster_iterator mon(you.get_los()); mon; ++mon)
    {
        if (mon->wont_attack())
            continue;

        int duration;
        if (mon->is_summoned(&duration))
        {
            int sockage = std::max(fuzz_value(abjdur, 60, 30), 40);
            dprf("%s abj: dur: %d, abj: %d",
                 mon->name(DESC_PLAIN).c_str(), duration, sockage);

            bool shielded = false;
            // TSO and Trog's abjuration protection.
            if (mons_is_god_gift(*mon, GOD_SHINING_ONE))
            {
                sockage = sockage * (30 - mon->hit_dice) / 45;
                if (sockage < duration)
                {
                    simple_god_message(" protects a fellow warrior from your evil magic!",
                                       GOD_SHINING_ONE);
                    shielded = true;
                }
            }
            else if (mons_is_god_gift(*mon, GOD_TROG))
            {
                sockage = sockage * 8 / 15;
                if (sockage < duration)
                {
                    simple_god_message(" shields an ally from your puny magic!",
                                       GOD_TROG);
                    shielded = true;
                }
            }

            mon_enchant abj = mon->get_ench(ENCH_ABJ);
            if (!mon->lose_ench_duration(abj, sockage) && !shielded)
                simple_monster_message(*mon, " shudders.");
        }
    }
}

static bool _know_spell(spell_type spell)
{
    if (spell == NUM_SPELLS)
        return (false);

    if (!you.has_spell(spell))
    {
        mprf("You don't know how to extend %s.", spell_title(spell));
        return (false);
    }

    int fail = spell_fail(spell);
    dprf("fail = %d", fail);

    if (fail > random2(50) + 50)
    {
        mprf("Your knowledge of %s fails you.", spell_title(spell));
        return (false);
    }

    return (true);
}

static spell_type _brand_spell()
{
    const item_def *wpn = you.weapon();

    if (!wpn)
        return NUM_SPELLS;

    const int wpn_type = get_vorpal_type(*wpn);
    switch(get_weapon_brand(*wpn))
    {
        case SPWPN_FLAMING:
        case SPWPN_FLAME:
            return SPELL_FIRE_BRAND;
        case SPWPN_FREEZING:
        case SPWPN_FROST:
            return SPELL_FREEZING_AURA;
        case SPWPN_VORPAL:
            if (wpn_type == DVORP_SLICING)
                return SPELL_TUKIMAS_VORPAL_BLADE;
            if (wpn_type == DVORP_CRUSHING)
                return SPELL_MAXWELLS_SILVER_HAMMER;
            return NUM_SPELLS;
        case SPWPN_VENOM:
            if (wpn_type == DVORP_CRUSHING)
                return NUM_SPELLS;
            return SPELL_POISON_WEAPON;
        case SPWPN_PAIN:
            return SPELL_EXCRUCIATING_WOUNDS;
        case SPWPN_DRAINING:
            return SPELL_LETHAL_INFUSION;
        case SPWPN_DISTORTION:
            return SPELL_WARP_BRAND;
        default:
            return NUM_SPELLS;
    }
}

static spell_type _transform_spell()
{
    switch(you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_BLADE_HANDS:
        return SPELL_BLADE_HANDS;
    case TRAN_SPIDER:
        return SPELL_SPIDER_FORM;
    case TRAN_STATUE:
        return SPELL_STATUE_FORM;
    case TRAN_ICE_BEAST:
        return SPELL_ICE_FORM;
    case TRAN_DRAGON:
        return SPELL_DRAGON_FORM;
    case TRAN_LICH:
        return SPELL_NECROMUTATION;
    case TRAN_BAT:
        return NUM_SPELLS; // no spell
    case TRAN_PIG:
        return SPELL_PORKALATOR; // Kirke/Nemelex only
    default:
        return NUM_SPELLS;
    }
}

void extension(int pow)
{
    int contamination = random2(2);

    if (you.duration[DUR_HASTE] && _know_spell(SPELL_HASTE))
    {
        potion_effect(POT_SPEED, pow);
        contamination++;
    }

    if (you.duration[DUR_SLOW] && _know_spell(SPELL_SLOW)) // heh heh
        potion_effect(POT_SLOWING, pow);

/*  Removed.
    if (you.duration[DUR_MIGHT])
    {
        potion_effect(POT_MIGHT, pow);
        contamination++;
    }

    if (you.duration[DUR_BRILLIANCE])
    {
        potion_effect(POT_BRILLIANCE, pow);
        contamination++;
    }

    if (you.duration[DUR_AGILITY])
    {
        potion_effect(POT_AGILITY, pow);
        contamination++;
    }
*/

    if (you.duration[DUR_LEVITATION] && !you.duration[DUR_CONTROLLED_FLIGHT]
        && _know_spell(SPELL_LEVITATION))
    {
        levitate_player(pow);
    }

    if (you.duration[DUR_INVIS] && _know_spell(SPELL_INVISIBILITY))
    {
        potion_effect(POT_INVISIBILITY, pow);
        contamination++;
    }

    if (you.duration[DUR_ICY_ARMOUR] && _know_spell(SPELL_OZOCUBUS_ARMOUR))
        ice_armour(pow, true);

    if (you.duration[DUR_REPEL_MISSILES] && _know_spell(SPELL_REPEL_MISSILES))
        missile_prot(pow);

    if (you.duration[DUR_REGENERATION] && _know_spell(SPELL_REGENERATION)
        && you.attribute[ATTR_TRANSFORMATION] != TRAN_LICH)
    {
        cast_regen(pow);
    }

    if (you.duration[DUR_DEFLECT_MISSILES]
        && _know_spell(SPELL_DEFLECT_MISSILES))
    {
        deflection(pow);
    }

    if (you.duration[DUR_FIRE_SHIELD] && _know_spell(SPELL_RING_OF_FLAMES))
    {
        you.increase_duration(DUR_FIRE_SHIELD, random2(pow / 20), 50);
        mpr("Your ring of flames roars with new vigour!");
    }

    if (!you.duration[DUR_WEAPON_BRAND] < 1 && _know_spell(_brand_spell()))
    {
        you.increase_duration(DUR_WEAPON_BRAND, 5 + random2(8), 80);
    }

    if (you.duration[DUR_SWIFTNESS] && _know_spell(SPELL_SWIFTNESS))
        cast_swiftness(pow);

    if (you.duration[DUR_INSULATION] && _know_spell(SPELL_INSULATION))
        cast_insulation(pow);

    // Xom only currently.
    if (you.duration[DUR_STONEMAIL] && _know_spell(SPELL_STONEMAIL))
        stone_scales(pow);

    if (you.duration[DUR_CONTROLLED_FLIGHT] && _know_spell(SPELL_FLY))
        cast_fly(pow);

    if (you.duration[DUR_CONTROL_TELEPORT]
        && _know_spell(SPELL_CONTROL_TELEPORT))
    {
        cast_teleport_control(pow);
    }

    if (you.duration[DUR_RESIST_POISON] && _know_spell(SPELL_RESIST_POISON))
        cast_resist_poison(pow);

    if (you.duration[DUR_TRANSFORMATION] && _know_spell(_transform_spell()))
    {
        mpr("Your transformation has been extended.");
        you.increase_duration(DUR_TRANSFORMATION, random2(pow), 100,
                              "Your transformation has been extended.");

        // Give a warning if it won't last long enough for the
        // timeout messages.
        transformation_expiration_warning();
    }

    if (you.duration[DUR_STONESKIN] && _know_spell(SPELL_STONESKIN))
        cast_stoneskin(pow);

    if (you.duration[DUR_PHASE_SHIFT] && _know_spell(SPELL_PHASE_SHIFT))
        cast_phase_shift(pow);

    if (you.duration[DUR_SEE_INVISIBLE] && _know_spell(SPELL_SEE_INVISIBLE))
        cast_see_invisible(pow);

    if (you.duration[DUR_SILENCE] && _know_spell(SPELL_SILENCE))
        cast_silence(pow);    //how precisely did you cast extension?

    if (you.duration[DUR_CONDENSATION_SHIELD]
        && _know_spell(SPELL_CONDENSATION_SHIELD))
    {
        cast_condensation_shield(pow);
    }

    if (you.duration[DUR_DEATH_CHANNEL] && _know_spell(SPELL_DEATH_CHANNEL))
        cast_death_channel(pow);

    if (you.duration[DUR_DEATHS_DOOR] && _know_spell(SPELL_DEATHS_DOOR))
        cast_deaths_door(pow);   // just for the fail message

    if (contamination)
        contaminate_player( contamination, true );
}

void remove_ice_armour()
{
    mpr("Your icy armour melts away.", MSGCH_DURATION);
    you.redraw_armour_class = true;
    you.duration[DUR_ICY_ARMOUR] = 0;
}

void ice_armour(int pow, bool extending)
{
    if (!player_effectively_in_light_armour())
    {
        if (!extending)
            mpr("You are wearing too much armour.");

        return;
    }

    if (you.duration[DUR_STONEMAIL] || you.duration[DUR_STONESKIN])
    {
        if (!extending)
            mpr("The spell conflicts with another spell still in effect.");

        return;
    }

    if (you.duration[DUR_ICY_ARMOUR])
        mpr( "Your icy armour thickens." );
    else
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST)
            mpr( "Your icy body feels more resilient." );
        else
            mpr( "A film of ice covers your body!" );

        you.redraw_armour_class = true;
    }

    you.increase_duration(DUR_ICY_ARMOUR, 20 + random2(pow) + random2(pow), 50,
                          NULL);
}

void stone_scales(int pow)
{
    if (you.duration[DUR_ICY_ARMOUR] || you.duration[DUR_STONESKIN])
    {
        mpr("The spell conflicts with another spell still in effect.");
        return;
    }

    if (you.duration[DUR_STONEMAIL])
        mpr("Your scaly armour looks firmer.");
    else
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_STATUE)
            mpr( "Your stone body feels more resilient." );
        else
            mpr( "A set of stone scales covers your body!" );

        you.redraw_evasion = true;
        you.redraw_armour_class = true;
    }

    you.increase_duration(DUR_STONEMAIL, 20 + random2(pow) + random2(pow), 100,
                          NULL);
    burden_change();
}

void missile_prot(int pow)
{
    you.increase_duration(DUR_REPEL_MISSILES, 8 + roll_dice( 2, pow ), 100,
                          "You feel protected from missiles.");
}

void deflection(int pow)
{
    you.increase_duration(DUR_DEFLECT_MISSILES, 15 + random2(pow), 100,
                          "You feel very safe from missiles.");
}

void remove_regen(bool divine_ability)
{
    mpr("Your skin stops crawling.", MSGCH_DURATION);
    you.duration[DUR_REGENERATION] = 0;
    if (divine_ability)
    {
        mpr("You feel less resistant to magic.", MSGCH_DURATION);
        you.attribute[ATTR_DIVINE_REGENERATION] = 0;
    }
}

void cast_regen(int pow, bool divine_ability)
{
    you.increase_duration(DUR_REGENERATION, 5 + roll_dice(2, pow / 3 + 1), 100,
                          "Your skin crawls.");

    if (divine_ability)
    {
        mpr("You feel resistant to magic.");
        you.attribute[ATTR_DIVINE_REGENERATION] = 1;
    }
}

void cast_berserk(void)
{
    go_berserk(true);
}

void cast_swiftness(int power)
{
    if (you.in_water())
    {
        mpr("The water foams!");
        return;
    }

    if (!you.duration[DUR_SWIFTNESS] && player_movement_speed() <= 6)
    {
        mpr( "You can't move any more quickly." );
        return;
    }

    // [dshaligram] Removed the on-your-feet bit.  Sounds odd when
    // you're levitating, for instance.
    you.increase_duration(DUR_SWIFTNESS, 20 + random2(power), 100,
                          "You feel quick.");
    did_god_conduct(DID_HASTY, 8, true);
}

void cast_fly(int power)
{
    const int dur_change = 25 + random2(power) + random2(power);
    const bool was_levitating = you.airborne();

    you.increase_duration(DUR_LEVITATION, dur_change, 100);
    you.increase_duration(DUR_CONTROLLED_FLIGHT, dur_change, 100);

    burden_change();

    if (!was_levitating)
    {
        if (you.light_flight())
            mpr("You swoop lightly up into the air.");
        else
            mpr("You fly up into the air.");

        // Merfolk boots unmeld if flight takes us out of water.
        if (you.species == SP_MERFOLK && feat_is_water(grd(you.pos())))
            unmeld_one_equip(EQ_BOOTS);
    }
    else
        mpr("You feel more buoyant.");
}

void cast_insulation(int power)
{
    you.increase_duration(DUR_INSULATION, 10 + random2(power), 100,
                          "You feel insulated.");
}

void cast_resist_poison(int power)
{
    you.increase_duration(DUR_RESIST_POISON, 10 + random2(power), 100,
                          "You feel resistant to poison.");
}

void cast_teleport_control(int power)
{
    you.increase_duration(DUR_CONTROL_TELEPORT, 10 + random2(power), 50,
                          "You feel in control.");
}

void cast_confusing_touch(int power)
{
    msg::stream << "Your " << your_hand(true) << " begin to glow "
                << (you.duration[DUR_CONFUSING_TOUCH] ? "brighter" : "red")
                << "." << std::endl;

    you.increase_duration(DUR_CONFUSING_TOUCH, 5 + (random2(power) / 5),
                          50, NULL);

}

bool cast_sure_blade(int power)
{
    bool success = false;

    if (!you.weapon())
        mpr("You aren't wielding a weapon!");
    else if (weapon_skill(you.weapon()->base_type,
                          you.weapon()->sub_type) != SK_SHORT_BLADES)
    {
        mpr("You cannot bond with this weapon.");
    }
    else
    {
        if (!you.duration[DUR_SURE_BLADE])
            mpr("You become one with your weapon.");
        else if (you.duration[DUR_SURE_BLADE] < 25 * BASELINE_DELAY)
            mpr("Your bond becomes stronger.");

        you.increase_duration(DUR_SURE_BLADE, 8 + (random2(power) / 10),
                              25, NULL);
        success = true;
    }

    return (success);
}
