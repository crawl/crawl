/**
 * @file
 * @brief Damage-dealing spells not already handled elsewhere.
 *           Other targeted spells are covered in spl-zap.cc.
**/

#include "AppHdr.h"

#include "spl-damage.h"
#include "externs.h"

#include "areas.h"
#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "feature.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "ouch.h"
#include "player-equip.h"
#include "shout.h"
#include "spl-util.h"
#include "stuff.h"
#include "target.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "view.h"
#include "viewchar.h"

// This spell has two main advantages over Fireball:
//
// (1) The release is instantaneous, so monsters will not
//     get an action before the player... this allows the
//     player to hit monsters with a double fireball (this
//     is why we only allow one delayed fireball at a time,
//     if you want to allow for more, then the release should
//     take at least some amount of time).
//
//     The casting of this spell still costs a turn.  So
//     casting Delayed Fireball and immediately releasing
//     the fireball is only slightly different from casting
//     a regular Fireball (monsters act in the middle instead
//     of at the end).  This is why we allow for the spell
//     level discount so that Fireball is free with this spell
//     (so that it only costs 7 levels instead of 13 to have
//     both).
//
// (2) When the fireball is released, it is guaranteed to
//     go off... the spell only fails at this point.  This can
//     be a large advantage for characters who have difficulty
//     casting Fireball in their standard equipment.  However,
//     the power level for the actual fireball is determined at
//     release, so if you do swap out your enhancers you'll
//     get a less powerful ball when it's released. - bwr
//
spret_type cast_delayed_fireball(bool fail)
{
    if (you.attribute[ATTR_DELAYED_FIREBALL])
    {
        mpr("You are already charged.");
        return SPRET_ABORT;
    }

    fail_check();
    // Okay, this message is weak but functional. - bwr
    mpr("You feel magically charged.");
    you.attribute[ATTR_DELAYED_FIREBALL] = 1;
    return SPRET_SUCCESS;
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
        source->is_player() ? KILL_YOU_MISSILE : KILL_MON;
    beam.aux_source.clear();
    beam.obvious_effect = false;
    beam.is_beam      = false;
    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.ench_power   = pow;      // used for radius
    beam.hit          = 20 + pow / 10;
    beam.damage       = calc_dice(8, 5 + pow);
}

spret_type cast_fire_storm(int pow, bolt &beam, bool fail)
{
    if (distance2(beam.target, beam.source) > dist_range(beam.range))
    {
        mpr("That is beyond the maximum range.");
        return SPRET_ABORT;
    }

    if (cell_is_solid(beam.target))
    {
        mpr("You can't place the storm on a wall.");
        return SPRET_ABORT;
    }

    fail_check();
    setup_fire_storm(&you, pow, beam);

    bolt tempbeam = beam;
    tempbeam.ex_size = (pow > 76) ? 3 : 2;
    tempbeam.is_tracer = true;

    tempbeam.explode(false);
    if (tempbeam.beam_cancelled)
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    beam.refine_for_explosion();
    beam.explode(false);

    viewwindow();
    return SPRET_SUCCESS;
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
        return false;
    }

    mpr("You call forth a pillar of hellfire!");

    beam.is_tracer = false;
    beam.in_explosion_phase = false;
    beam.explode(true);

    return true;
}

// XXX no friendly check
spret_type cast_chain_lightning(int pow, const actor *caster, bool fail)
{
    fail_check();
    bolt beam;

    // initialise beam structure
    beam.name           = "lightning arc";
    beam.aux_source     = "chain lightning";
    beam.beam_source    = caster->mindex();
    beam.thrower        = caster->is_player() ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
    beam.range          = 8;
    beam.hit            = AUTOMATIC_HIT;
    beam.glyph          = dchar_glyph(DCHAR_FIRED_ZAP);
    beam.flavour        = BEAM_ELECTRICITY;
    beam.obvious_effect = true;
    beam.is_beam        = false;       // since we want to stop at our target
    beam.is_explosion   = false;
    beam.is_tracer      = false;

    if (const monster* mons = caster->as_monster())
        beam.source_name = mons->name(DESC_PLAIN, true);

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

            // Don't arc to things we cannot hit.
            if (beam.ignores_monster(*mi))
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

            if (!cell_see_cell(source, mi->pos(), LOS_SOLID))
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
                // either first target, or new selected target at
                // min_dist == dist.
                target = mi->pos();
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
                && cell_see_cell(source, you.pos(), LOS_SOLID))
            {
                target = you.pos();
            }
        }

        const bool see_source = you.see_cell(source);
        const bool see_targ   = you.see_cell(target);

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

        if (!you.see_cell_no_trans(target))
        {
            // It's no longer in the caster's LOS and influence.
            pow = pow / 2 + 1;
        }

        beam.source = source;
        beam.target = target;
        beam.colour = LIGHTBLUE;
        beam.damage = calc_dice(5, 10 + pow * 2 / 3);

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
    return SPRET_SUCCESS;
}

// Poisonous light passes right through invisible players
// and monsters, and so, they are unaffected by this spell --
// assumes only you can cast this spell (or would want to).
static bool _toxic_radianceable(const actor *act)
{
    if (act->invisible())
        return false;
    // currently monsters are still immune at rPois 1
    return (act->res_poison() < (act->is_player() ? 3 : 1));
}

spret_type cast_toxic_radiance(int pow, bool non_player, bool fail)
{
    targetter_los hitfunc(&you, LOS_NO_TRANS);
    if (!non_player)
    {
        if (stop_attack_prompt(hitfunc, "poison", _toxic_radianceable))
            return SPRET_ABORT;
    }

    fail_check();
    if (non_player)
        mpr("The air is filled with a sickly green light!");
    else
        mpr("You radiate a sickly green light!");

    flash_view(GREEN, &hitfunc);
    more();
    mesclr();
    flash_view(0);

    // Determine whether the player is hit by the radiance. {dlb}
    if (you.duration[DUR_INVIS])
        mpr("The light passes straight through your body.");
    else
    {
        int poison_amount = poison_player(2, "", "toxic radiance");
        if (poison_amount)
            mpr("You feel rather sick.");
    }

    bool sanct = false;
    counted_monster_list affected_monsters;
    // determine which monsters are hit by the radiance: {dlb}
    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        // Trees and translucent walls absorb enough of the radiance for it to be ineffective.
        if (!mi->submerged() && cell_see_cell(you.pos(), mi->pos(), LOS_NO_TRANS))
        {
            // Monsters affected by corona are still invisible in that
            // radiation passes through them without affecting them. Therefore,
            // this check should not be !mons->invisible().
            if (!mi->has_ench(ENCH_INVIS))
            {
                const actor* agent = non_player ? 0 : &you;
                const int levels = 1 + random2(pow / 20);

                if (poison_monster(*mi, agent, levels, false, false))
                {
                    if (is_sanctuary(mi->pos()))
                        sanct = true;
                    affected_monsters.add(*mi);
                }
            }
            else if (you.can_see_invisible())
            {
                // message player re:"miss" where appropriate {dlb}
                mprf("The light passes through %s.",
                     mi->name(DESC_THE).c_str());
            }
        }
    }

    if (!affected_monsters.empty())
    {
        const string message =
            make_stringf("%s %s poisoned.",
                         affected_monsters.describe().c_str(),
                         affected_monsters.count() == 1? "is" : "are");
        if (strwidth(message) < get_number_of_cols() - 2)
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
        // Sanctuary is violated if either you or any victims are in it.
        if (!non_player && (sanct || is_sanctuary(you.pos())))
            remove_sanctuary(true);

    }
    return SPRET_SUCCESS;
}

static bool _refrigerateable(const actor *act)
{
    // Inconsistency: monsters suffer no damage at rC+++, players suffer
    // considerable damage.
    return (act->is_player() || act->res_cold() < 3);
}

spret_type cast_refrigeration(int pow, bool non_player, bool freeze_potions,
                              bool fail)
{
    targetter_los hitfunc(&you, LOS_SOLID);
    {
        if (stop_attack_prompt(hitfunc, "harm",  _refrigerateable))
            return SPRET_ABORT;
    }

    fail_check();
    if (non_player)
        mpr("Something drains the heat from around you.");
    else
        mpr("The heat is drained from your surroundings.");

    flash_view(LIGHTCYAN, &hitfunc);
    more();
    mesclr();
    flash_view(0);

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
        expose_player_to_element(BEAM_COLD, 5, freeze_potions);
    }

    // Now do the monsters.

    // First build the message.
    counted_monster_list affected_monsters;

    for (monster_iterator mi(&you); mi; ++mi)
    {
        // not just you.can_see (Scry), and not cold-immune monsters
        if (cell_see_cell(you.pos(), mi->pos(), LOS_SOLID)
            && _refrigerateable(*mi))
        {
            affected_monsters.add(*mi);
        }
    }

    if (!affected_monsters.empty())
    {
        const string message =
            make_stringf("%s %s frozen.",
                         affected_monsters.describe().c_str(),
                         affected_monsters.count() == 1? "is" : "are");
        if (strwidth(message) < get_number_of_cols() - 2)
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

        // ... but not ones you see only via Scrying, and not cold-immune ones
        if (!cell_see_cell(you.pos(), mi->pos(), LOS_SOLID)
            || !_refrigerateable(*mi))
        {
            continue;
        }

        // Calculate damage and apply.
        int hurt = mons_adjust_flavoured(*mi, beam, dam_dice.roll());
        dprf("damage done: %d", hurt);
        if (non_player)
            mi->hurt(NULL, hurt, BEAM_COLD);
        else
            mi->hurt(&you, hurt, BEAM_COLD);

        if (!non_player && (is_sanctuary(you.pos()) || is_sanctuary(mi->pos())))
            remove_sanctuary(true);

        // Cold-blooded creatures can be slowed.
        if (mi->alive())
            mi->expose_to_element(BEAM_COLD, 5);
    }
    return SPRET_SUCCESS;
}

// Screaming Sword
void sonic_damage(bool scream)
{
    // First build the message.
    counted_monster_list affected_monsters;

    for (monster_iterator mi(&you); mi; ++mi)
        if (cell_see_cell(you.pos(), mi->pos(), LOS_SOLID)
            && !silenced(mi->pos()))
        {
            affected_monsters.add(*mi);
        }

    /* dpeg sez:
       * damage applied to everyone but the wielder (reasoning: the sword
         does not like competitors, so no allies)
       -- so sorry, Beoghites, Jiyvaites and the rest.
    */

    if (!affected_monsters.empty())
    {
        const string message =
            make_stringf("%s %s hurt by the noise.",
                         affected_monsters.describe().c_str(),
                         affected_monsters.count() == 1? "is" : "are");
        if (strwidth(message) < get_number_of_cols() - 2)
            mpr(message.c_str());
        else
        {
            // Exclamation mark to suggest that a lot of creatures were
            // affected.
            mpr("The monsters around you reel from the noise!");
        }
    }

    // Now damage the creatures.
    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        if (!cell_see_cell(you.pos(), mi->pos(), LOS_SOLID)
            || silenced(mi->pos()))
        {
            continue;
        }
        int hurt = (random2(2) + 1) * (random2(2) + 1) * (random2(3) + 1)
                 + (random2(3) + 1) + 1;
        if (scream)
            hurt = max(hurt * 2, 16);
        int cap = scream ? mi->max_hit_points / 2 : mi->max_hit_points * 3 / 10;
        hurt = min(hurt, max(cap, 1));
        // not so much damage if you're a n00b
        hurt = div_rand_round(hurt * you.experience_level, 27);
        /* per dpeg:
         * damage is universal (well, only to those who can hear, but not sure
           we can determine that in-game), i.e. smiting, no resists
        */
        dprf("damage done: %d", hurt);
        mi->hurt(&you, hurt);

        if (is_sanctuary(you.pos()) || is_sanctuary(mi->pos()))
            remove_sanctuary(true);
    }
}

spret_type vampiric_drain(int pow, monster* mons, bool fail)
{
    if (mons == NULL || mons->submerged())
    {
        fail_check();
        canned_msg(MSG_NOTHING_CLOSE_ENOUGH);
        // Cost to disallow freely locating invisible/submerged
        // monsters.
        return SPRET_SUCCESS;
    }

    if (mons->observable() && mons->holiness() != MH_NATURAL)
    {
        mpr("Draining that being is not a good idea.");
        return SPRET_ABORT;
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool abort = stop_attack_prompt(mons, false, you.pos());

    if (!abort && !fail)
    {
        set_attack_conducts(conducts, mons);

        behaviour_event(mons, ME_WHACK, &you, you.pos());
    }

    enable_attack_conducts(conducts);

    if (abort)
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    fail_check();

    if (!mons->alive())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    // Monster might be invisible or player misled.
    if (mons->holiness() == MH_UNDEAD || mons->holiness() == MH_DEMONIC)
    {
        mpr("Aaaarggghhhhh!");
        dec_hp(random2avg(39, 2) + 10, false, "vampiric drain backlash");
        return SPRET_SUCCESS;
    }

    if (mons->holiness() != MH_NATURAL || mons->res_negative_energy())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    // The practical maximum of this is about 25 (pow @ 100). - bwr
    int hp_gain = 3 + random2avg(9, 2) + random2(pow) / 7;

    hp_gain = min(mons->hit_points, hp_gain);
    hp_gain = min(you.hp_max - you.hp, hp_gain);

    if (!hp_gain)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_SUCCESS;
    }

    const bool mons_was_summoned = mons->is_summoned();

    mons->hurt(&you, hp_gain);

    if (mons->alive())
        print_wounds(mons);

    hp_gain /= 2;

    if (hp_gain && !mons_was_summoned && !you.duration[DUR_DEATHS_DOOR])
    {
        mpr("You feel life coursing into your body.");
        inc_hp(hp_gain);
    }

    return SPRET_SUCCESS;
}

spret_type cast_freeze(int pow, monster* mons, bool fail)
{
    pow = min(25, pow);

    if (mons == NULL || mons->submerged())
    {
        fail_check();
        canned_msg(MSG_NOTHING_CLOSE_ENOUGH);
        // If there's no monster there, you still pay the costs in
        // order to prevent locating invisible/submerged monsters.
        return SPRET_SUCCESS;
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool abort = stop_attack_prompt(mons, false, you.pos());

    if (!abort && !fail)
    {
        set_attack_conducts(conducts, mons);

        mprf("You freeze %s.", mons->name(DESC_THE).c_str());

        behaviour_event(mons, ME_ANNOY, &you);
    }

    enable_attack_conducts(conducts);

    if (abort)
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    fail_check();

    bolt beam;
    beam.flavour = BEAM_COLD;
    beam.thrower = KILL_YOU;

    const int orig_hurted = roll_dice(1, 3 + pow / 3);
    int hurted = mons_adjust_flavoured(mons, beam, orig_hurted);
    mons->hurt(&you, hurted);

    if (mons->alive())
    {
        mons->expose_to_element(BEAM_COLD, orig_hurted);
        print_wounds(mons);

        const int cold_res = mons->res_cold();
        if (cold_res <= 0)
        {
            const int stun = (1 - cold_res) * random2(2 + pow/5);
            mons->speed_increment -= stun;
        }
    }

    return SPRET_SUCCESS;
}

spret_type cast_airstrike(int pow, const dist &beam, bool fail)
{
    monster* mons = monster_at(beam.target);
    if (!mons
        || mons->submerged()
        || (cell_is_solid(beam.target) && mons_wall_shielded(mons)))
    {
        fail_check();
        canned_msg(MSG_SPELL_FIZZLES);
        return SPRET_SUCCESS; // still losing a turn
    }

    if (mons->res_wind() > 0)
    {
        if (mons->observable())
        {
            mprf("But air would do no harm to %s!",
                 mons->name(DESC_THE).c_str());
            return SPRET_ABORT;
        }

        fail_check();
        mprf("The air twists arounds and harmlessly tosses %s around.",
             mons->name(DESC_THE).c_str());
        // Bailing out early, no need to upset the gods or the target.
        return SPRET_SUCCESS; // you still did discover the invisible monster
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (stop_attack_prompt(mons, false, you.pos()))
        return SPRET_ABORT;

    fail_check();
    set_attack_conducts(conducts, mons);

    mprf("The air twists around and strikes %s!",
         mons->name(DESC_THE).c_str());
    noisy(4, beam.target);

    behaviour_event(mons, ME_ANNOY, &you);

    enable_attack_conducts(conducts);

    int hurted = 8 + random2(random2(4) + (random2(pow) / 6)
                   + (random2(pow) / 7));

    bolt pbeam;
    pbeam.flavour = BEAM_AIR;
    hurted = mons->apply_ac(mons->beam_resists(pbeam, hurted, false));

    mons->hurt(&you, hurted);
    if (mons->alive())
        print_wounds(mons);

    return SPRET_SUCCESS;
}

// Just to avoid typing this over and over.
// Returns true if monster died. -- bwr
static bool _player_hurt_monster(monster& m, int damage,
                                 beam_type flavour = BEAM_MISSILE)
{
    if (damage > 0)
    {
        m.hurt(&you, damage, flavour, false);

        if (m.alive())
        {
            print_wounds(&m);
            behaviour_event(&m, ME_WHACK, &you);
        }
        else
        {
            monster_die(&m, KILL_YOU, NON_MONSTER);
            return true;
        }
    }

    return false;
}

// Here begin the actual spells:
static int _shatter_mon_dice(const monster *mon)
{
    if (!mon)
        return 0;

    // Removed a lot of silly monsters down here... people, just because
    // it says ice, rock, or iron in the name doesn't mean it's actually
    // made out of the substance. - bwr
    switch (mon->type)
    {
    // Double damage to stone, metal and crystal.
    case MONS_EARTH_ELEMENTAL:
    case MONS_CLAY_GOLEM:
    case MONS_STONE_GOLEM:
    case MONS_STATUE:
    case MONS_GARGOYLE:
    case MONS_IRON_ELEMENTAL:
    case MONS_IRON_GOLEM:
    case MONS_METAL_GARGOYLE:
    case MONS_CRYSTAL_GOLEM:
    case MONS_SILVER_STATUE:
    case MONS_ORANGE_STATUE:
    case MONS_ROXANNE:
        return 6;

    // 1/3 damage to liquids.
    case MONS_JELLYFISH:
    case MONS_WATER_ELEMENTAL:
        return 1;

    default:
        const bool petrifying = mon->petrifying();
        const bool petrified = mon->petrified();

        // Extra damage to petrifying/petrified things.
        // Undo the damage reduction as well; base damage is 4 : 6.
        if (petrifying || petrified)
            return petrifying ? 6 : 12;
        // No damage to insubstantials.
        else if (mon->is_insubstantial())
            return 0;
        // 1/3 damage to fliers and slimes.
        else if (mons_flies(mon) || mons_is_slime(mon))
            return 1;
        // 3/2 damage to ice.
        else if (mon->is_icy())
            return 4;
        // Double damage to bone.
        else if (mon->is_skeletal())
            return 6;
        // Normal damage to everything else.
        else
            return 3;
    }
}

static int _shatter_monsters(coord_def where, int pow, actor *agent)
{
    dice_def dam_dice(0, 5 + pow / 3); // Number of dice set below.
    monster* mon = monster_at(where);

    if (mon == NULL || mon == agent)
        return 0;

    dam_dice.num = _shatter_mon_dice(mon);
    int damage = max(0, dam_dice.roll() - random2(mon->armour_class()));

    if (damage <= 0)
        return 0;

    if (agent->is_player())
    {
        _player_hurt_monster(*mon, damage);

        if (is_sanctuary(you.pos()) || is_sanctuary(mon->pos()))
            remove_sanctuary(true);
    }
    else
        mon->hurt(agent, damage);

    return damage;
}

static int _shatter_items(coord_def where, int pow, actor *)
{
    UNUSED(pow);

    int broke_stuff = 0;

    for (stack_iterator si(where); si; ++si)
    {
        if (si->base_type == OBJ_POTIONS)
        {
            for (int j = 0; j < si->quantity; ++j)
            {
                if (one_chance_in(10))
                {
                    broke_stuff++;
                    if (!dec_mitm_item_quantity(si->index(), 1)
                        && is_blood_potion(*si))
                    {
                       remove_oldest_blood_potion(*si);
                    }
                }
            }
        }
    }

    if (broke_stuff)
    {
        if (player_can_hear(where))
            mpr("You hear glass break.", MSGCH_SOUND);

        return 1;
    }

    return 0;
}

static int _shatter_walls(coord_def where, int pow, actor *agent)
{
    int chance = 0;

    // if not in-bounds then we can't really shatter it -- bwr
    if (!in_bounds(where))
        return 0;

    if (env.markers.property_at(where, MAT_ANY, "veto_shatter") == "veto")
        return 0;

    const dungeon_feature_type grid = grd(where);

    switch (grid)
    {
    case DNGN_CLOSED_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_OPEN_DOOR:
    case DNGN_SEALED_DOOR:
        if (you.see_cell(where))
            mpr("A door shatters!");
        chance = 100;
        break;

    case DNGN_GRATE:
        if (you.see_cell(where))
            mpr("An iron grate is ripped into pieces!");
        chance = 100;
        break;

    case DNGN_METAL_WALL:
        chance = pow / 10;
        break;

    case DNGN_ORCISH_IDOL:
    case DNGN_GRANITE_STATUE:
        chance = 50;
        break;

    case DNGN_CLEAR_STONE_WALL:
    case DNGN_STONE_WALL:
        chance = pow / 6;
        break;

    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
        chance = pow / 4;
        break;

    case DNGN_GREEN_CRYSTAL_WALL:
        chance = 50;
        break;

    case DNGN_TREE:
    case DNGN_MANGROVE:
        chance = 33;
        break;

    default:
        break;
    }

    if (x_chance_in_y(chance, 100))
    {
        noisy(30, where);

        nuke_wall(where);

        if (agent->is_player() && grid == DNGN_ORCISH_IDOL)
            did_god_conduct(DID_DESTROY_ORCISH_IDOL, 8);
        if (agent->is_player() && feat_is_tree(grid))
            did_god_conduct(DID_KILL_PLANT, 1);

        return 1;
    }

    return 0;
}

static bool _shatterable(const actor *act)
{
    if (act->is_player())
        return true; // no player ghostlies... at least user-controllable ones
    return _shatter_mon_dice(act->as_monster());
}

spret_type cast_shatter(int pow, bool fail)
{
    {
        int r_min = 3 + you.skill(SK_EARTH_MAGIC) / 5;
        targetter_los hitfunc(&you, LOS_ARENA, r_min, min(r_min + 1, 8));
        if (stop_attack_prompt(hitfunc, "harm", _shatterable))
            return SPRET_ABORT;
    }

    fail_check();
    const bool silence = silenced(you.pos());

    if (silence)
        mpr("The dungeon shakes!");
    else
    {
        noisy(30, you.pos());
        mpr("The dungeon rumbles!", MSGCH_SOUND);
    }

    int rad = 3 + you.skill_rdiv(SK_EARTH_MAGIC, 1, 5);

    int dest = 0;
    for (distance_iterator di(you.pos(), true, true, rad); di; ++di)
    {
        // goes from the center out, so newly dug walls recurse
        if (!cell_see_cell(you.pos(), *di, LOS_SOLID))
            continue;

        _shatter_items(*di, pow, &you);
        _shatter_monsters(*di, pow, &you);
        dest += _shatter_walls(*di, pow, &you);
    }

    if (dest && !silence)
        mpr("Ka-crash!", MSGCH_SOUND);

    return SPRET_SUCCESS;
}

static int _shatter_player_dice()
{
    if (you.petrified())
        return 12; // reduced later
    else if (you.petrifying())
        return 6;  // reduced later
    // Same order as for monsters -- petrified flyers get hit hard, skeletal
    // flyers get no extra damage.
    else if (you.airborne())
        return 1;
    else if (you.form == TRAN_STATUE)
        return 6;
    else if (you.form == TRAN_ICE_BEAST)
        return 4;
    else
        return 3;
}

static int _shatter_player(int pow, actor *wielder, bool devastator = false)
{
    if (wielder->is_player())
        return 0;

    dice_def dam_dice(_shatter_player_dice(), 5 + pow / 3);

    int damage = max(0, dam_dice.roll() - random2(you.armour_class()));

    if (damage > 0)
    {
        mpr(damage > 15 ? "You shudder from the earth-shattering force."
                        : "You shudder.");
        if (devastator)
            ouch(damage, wielder->mindex(), KILLED_BY_MONSTER);
        else
            ouch(damage, wielder->mindex(), KILLED_BY_BEAM, "by Shatter");
    }

    return damage;
}

bool mons_shatter(monster* caster, bool actual)
{
    const bool silence = silenced(caster->pos());
    int foes = 0;

    if (actual)
    {
        if (silence)
            mprf("The dungeon shakes around %s!",
                 caster->name(DESC_THE).c_str());
        else
        {
            noisy(30, caster->pos(), caster->mindex());
            mprf(MSGCH_SOUND, "The dungeon rumbles around %s!",
                 caster->name(DESC_THE).c_str());
        }
    }

    int pow = 5 + div_rand_round(caster->hit_dice * 9, 2);
    int rad = 3 + div_rand_round(caster->hit_dice, 5);

    int dest = 0;
    for (distance_iterator di(caster->pos(), true, true, rad); di; ++di)
    {
        // goes from the center out, so newly dug walls recurse
        if (!cell_see_cell(caster->pos(), *di, LOS_SOLID))
            continue;

        if (actual)
        {
            _shatter_items(*di, pow, caster);
            _shatter_monsters(*di, pow, caster);
            if (*di == you.pos())
                _shatter_player(pow, caster);
            dest += _shatter_walls(*di, pow, caster);
        }
        else
        {
            if (you.pos() == *di)
                foes -= _shatter_player_dice();
            if (const monster *victim = monster_at(*di))
            {
                dprf("[%s]", victim->name(DESC_PLAIN, true).c_str());
                foes += _shatter_mon_dice(victim)
                     * (victim->wont_attack() ? -1 : 1);
            }
        }
    }

    if (dest && !silence)
        mpr("Ka-crash!", MSGCH_SOUND);

    if (!caster->wont_attack())
        foes *= -1;

    if (!actual)
        dprf("Shatter foe HD: %d", foes);

    return (foes > 0); // doesn't matter if actual
}

void shillelagh(actor *wielder, coord_def where, int pow)
{
    bolt beam;
    beam.name = "shillelagh";
    beam.flavour = BEAM_VISUAL;
    beam.set_agent(wielder);
    beam.colour = BROWN;
    beam.glyph = dchar_glyph(DCHAR_EXPLOSION);
    beam.range = 1;
    beam.ex_size = 1;
    beam.is_explosion = true;
    beam.source = wielder->pos();
    beam.target = where;
    beam.hit = AUTOMATIC_HIT;
    beam.loudness = 7;
    beam.explode();

    counted_monster_list affected_monsters;
    for (adjacent_iterator ai(where, false); ai; ++ai)
    {
        monster *mon = monster_at(*ai);
        if (!mon || !mon->alive() || mon->submerged()
            || mon->is_insubstantial() || !you.can_see(mon)
            || mon == wielder)
        {
            continue;
        }
        affected_monsters.add(mon);
    }
    if (!affected_monsters.empty())
    {
        const string message =
            make_stringf("%s shudder%s.",
                         affected_monsters.describe().c_str(),
                         affected_monsters.count() == 1? "s" : "");
        if (strwidth(message) < get_number_of_cols() - 2)
            mpr(message.c_str());
        else
            mpr("There is a shattering impact!");
    }

    // need to do this again to do the actual damage
    for (adjacent_iterator ai(where, false); ai; ++ai)
        _shatter_monsters(*ai, pow * 3 / 2, wielder);

    if ((you.pos() - wielder->pos()).abs() <= 2 && in_bounds(you.pos()))
        _shatter_player(pow, wielder, true);
}

static int _ignite_poison_affect_item(item_def& item, bool in_inv)
{
    int strength = 0;

    // Poison branding becomes fire branding.
    // don't affect non-wielded weapons, they don't start dripping poison until
    // you wield them. -doy
    if (&item == you.weapon()
        && you.duration[DUR_WEAPON_BRAND]
        && get_weapon_brand(item) == SPWPN_VENOM)
    {
        if (set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FLAMING))
        {
            mprf("%s bursts into flame!",
                 item.name(DESC_YOUR).c_str());

            you.wield_change = true;

            int increase = 1 + you.duration[DUR_WEAPON_BRAND]
                               /(2 * BASELINE_DELAY);

            you.increase_duration(DUR_WEAPON_BRAND, increase, 80);
        }

        // and don't destroy it
        return 0;
    }
    else if (item.base_type == OBJ_MISSILES && item.special == SPMSL_POISONED)
    {
        // Burn poison ammo.
        strength = item.quantity;
    }
    else if (item.base_type == OBJ_POTIONS)
    {
        // Burn poisonous potions.
        switch (item.sub_type)
        {
        case POT_STRONG_POISON:
            strength = 20 * item.quantity;
            break;
        case POT_DEGENERATION:
        case POT_POISON:
            strength = 10 * item.quantity;
            break;
        default:
            break;
        }
    }
    else if (item.base_type == OBJ_CORPSES &&
             item.sub_type == CORPSE_BODY &&
             chunk_is_poisonous(mons_corpse_effect(item.mon_type)))
    {
        strength = mons_weight(item.mon_type) / 25;
    }
    else if (item.base_type == OBJ_FOOD &&
             item.sub_type == FOOD_CHUNK &&
             chunk_is_poisonous(mons_corpse_effect(item.mon_type)))
    {
        strength += 30 * item.quantity;
    }

    if (strength)
    {
        if (in_inv)
        {
            if (item.base_type == OBJ_POTIONS)
            {
                mprf("%s explode%s!",
                     item.name(DESC_PLAIN).c_str(),
                     item.quantity == 1 ? "s" : "");
            }
            else
            {
                mprf("Your %s burn%s!",
                     item.name(DESC_PLAIN).c_str(),
                     item.quantity == 1 ? "s" : "");
            }
        }

        if (item.base_type == OBJ_CORPSES &&
            item.sub_type == CORPSE_BODY &&
            mons_skeleton(item.mon_type))
        {
            turn_corpse_into_skeleton(item);
        }
        else
        {
            if (in_inv && &item == you.weapon())
            {
                unwield_item();
                canned_msg(MSG_EMPTY_HANDED_NOW);
            }
            item_was_destroyed(item);
            if (in_inv)
                destroy_item(item);
            else
                destroy_item(item.index());
        }
    }

    return strength;
}

static int _ignite_poison_objects(coord_def where, int pow, int, actor *actor)
{
    UNUSED(pow);

    int strength = 0;

    for (stack_iterator si(where); si; ++si)
        strength += _ignite_poison_affect_item(*si, false);

    if (strength > 0)
    {
        place_cloud(CLOUD_FIRE, where,
                    strength + roll_dice(3, strength / 4), actor);
    }

    return strength;
}

static int _ignite_poison_clouds(coord_def where, int pow, int, actor *actor)
{
    UNUSED(pow);

    const int i = env.cgrid(where);
    if (i != EMPTY_CLOUD)
    {
        cloud_struct& cloud = env.cloud[i];

        if (cloud.type == CLOUD_MEPHITIC)
        {
            cloud.decay /= 2;

            if (cloud.decay < 1)
                cloud.decay = 1;
        }
        else if (cloud.type != CLOUD_POISON)
            return false;

        cloud.type = CLOUD_FIRE;
        cloud.whose = actor->kill_alignment();
        cloud.killer = actor->is_player() ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
        cloud.source = actor->mid;
        return true;
    }

    return false;
}

static int _ignite_poison_monsters(coord_def where, int pow, int, actor *actor)
{
    bolt beam;
    beam.flavour = BEAM_FIRE;   // This is dumb, only used for adjust!

    dice_def dam_dice(0, 5 + pow/7);  // Dice added below if applicable.

    // If a monster casts Ignite Poison, it can't hit itself.
    // This doesn't apply to the other functions: it can ignite
    // clouds or items where it's standing!

    monster* mon = monster_at(where);
    if (mon == NULL || mon == actor)
        return 0;

    // Monsters which have poison corpses or poisonous attacks.
    if (mons_is_poisoner(mon))
        dam_dice.num = 3;

    // Monsters which are poisoned:
    int strength = 0;

    // First check for player poison.
    mon_enchant ench = mon->get_ench(ENCH_POISON);
    if (ench.ench != ENCH_NONE)
        strength += ench.degree;

    // Strength is now the sum of both poison types
    // (although only one should actually be present at a given time).
    dam_dice.num += strength;

    int damage = dam_dice.roll();
    if (damage > 0)
    {
        damage = mons_adjust_flavoured(mon, beam, damage);
        simple_monster_message(mon, " seems to burn from within!");

        dprf("Dice: %dd%d; Damage: %d", dam_dice.num, dam_dice.size, damage);

        mon->hurt(actor, damage);

        if (mon->alive())
        {
            behaviour_event(mon, ME_WHACK, actor);

            // Monster survived, remove any poison.
            mon->del_ench(ENCH_POISON);
            print_wounds(mon);
        }
        else
        {
            monster_die(mon,
                        actor->is_player() ? KILL_YOU : KILL_MON,
                        actor->mindex());
        }

        return 1;
    }

    return 0;
}

// The self effects of Ignite Poison are beautiful and
// shouldn't be thrown out. Let's save them for a monster
// version of the spell!

static int _ignite_poison_player(coord_def where, int pow, int, actor *actor)
{
    if (actor->is_player() || where != you.pos())
        return 0;

    int totalstrength = 0;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item = you.inv[i];
        if (!item.defined())
            continue;

        totalstrength += _ignite_poison_affect_item(item, true);
    }

    if (totalstrength)
    {
        place_cloud(
            CLOUD_FIRE, you.pos(),
            random2(totalstrength / 4 + 1) + random2(totalstrength / 4 + 1) +
            random2(totalstrength / 4 + 1) + random2(totalstrength / 4 + 1) + 1,
            actor);
    }

    int damage = 0;
    // Player is poisonous.
    if (player_mutation_level(MUT_SPIT_POISON)
        || player_mutation_level(MUT_STINGER)
        || you.form == TRAN_SPIDER // poison attack
        || (!form_changed_physiology()
            && (you.species == SP_GREEN_DRACONIAN       // poison breath
                || you.species == SP_KOBOLD             // poisonous corpse
                || you.species == SP_NAGA)))            // spit poison
    {
        damage = roll_dice(3, 5 + pow / 7);
    }

    // Player is poisoned.
    damage += roll_dice(you.duration[DUR_POISONING], 6);

    if (damage)
    {
        const int resist = player_res_fire();
        if (resist > 0)
        {
            mpr("You feel like your blood is boiling!");
            damage /= 3;
        }
        else if (resist < 0)
        {
            mpr("The poison in your system burns terribly!");
            damage *= 3;
        }
        else
            mpr("The poison in your system burns!");

        ouch(damage, actor->as_monster()->mindex(), KILLED_BY_MONSTER,
             actor->as_monster()->name(DESC_A).c_str());

        if (you.duration[DUR_POISONING] > 0)
        {
            mpr("You feel that the poison has left your system.");
            you.duration[DUR_POISONING] = 0;
        }
    }

    if (damage || totalstrength)
        return 1;
    else
        return 0;
}

static bool maybe_abort_ignite()
{
    // Fire cloud immunity.
    if (you.duration[DUR_FIRE_SHIELD] || you.mutation[MUT_IGNITE_BLOOD])
        return false;

    string prompt = "You are standing ";

    const int i = env.cgrid(you.pos());
    if (i != EMPTY_CLOUD)
    {
        cloud_struct& cloud = env.cloud[i];

        if (cloud.type == CLOUD_MEPHITIC || cloud.type == CLOUD_POISON)
        {
            prompt += "in a cloud of ";
            prompt += cloud_type_name(cloud.type, true);
            prompt += "! Ignite poison anyway?";
            return (!yesno(prompt.c_str(), false, 'n'));
        }
    }

    // Now check for items at player position.
    item_def item;
    for (stack_iterator si(you.pos()); si; ++si)
    {
        item = *si;

        if (item.base_type == OBJ_MISSILES && item.special == SPMSL_POISONED)
        {
            prompt += "over ";
            prompt += (item.quantity == 1 ? "a " : "") + (item.name(DESC_PLAIN));
            prompt += "! Ignite poison anyway?";
            return (!yesno(prompt.c_str(), false, 'n'));
        }
        else if (item.base_type == OBJ_POTIONS && item_type_known(item))
        {
            switch (item.sub_type)
            {
            case POT_STRONG_POISON:
            case POT_DEGENERATION:
            case POT_POISON:
                prompt += "over ";
                prompt += (item.quantity == 1 ? "a " : "") + (item.name(DESC_PLAIN));
                prompt += "! Ignite poison anyway?";
                return (!yesno(prompt.c_str(), false, 'n'));
            default:
                break;
            }
        }
        else if (item.base_type == OBJ_CORPSES
                 && item.sub_type == CORPSE_BODY
                 && chunk_is_poisonous(mons_corpse_effect(item.mon_type)))
        {
            prompt += "over ";
            prompt += (item.quantity == 1 ? "a " : "") + (item.name(DESC_PLAIN));
            prompt += "! Ignite poison anyway?";
            return (!yesno(prompt.c_str(), false, 'n'));
        }
        else if (item.base_type == OBJ_FOOD &&
                 item.sub_type == FOOD_CHUNK &&
                 chunk_is_poisonous(mons_corpse_effect(item.mon_type)))
        {
            prompt += "over ";
            prompt += (item.quantity == 1 ? "a " : "") + (item.name(DESC_PLAIN));
            prompt += "! Ignite poison anyway?";
            return (!yesno(prompt.c_str(), false, 'n'));
        }
    }

    return false;
}

spret_type cast_ignite_poison(int pow, bool fail)
{
    if (maybe_abort_ignite())
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    fail_check();
    targetter_los hitfunc(&you, LOS_NO_TRANS);
    flash_view(RED, &hitfunc);

    mpr("You ignite the poison in your surroundings!");

    apply_area_visible(_ignite_poison_clouds, pow, &you);
    apply_area_visible(_ignite_poison_objects, pow, &you);
    apply_area_visible(_ignite_poison_monsters, pow, &you);
// Not currently relevant - nothing will ever happen as long as
// the actor is &you.
    apply_area_visible(_ignite_poison_player, pow, &you);

#ifndef USE_TILE_LOCAL
    delay(100); // show a brief flash
#endif
    flash_view(0);
    return SPRET_SUCCESS;
}

static int _discharge_monsters(coord_def where, int pow, int, actor *)
{
    monster* mons = monster_at(where);
    int damage = 0;

    bolt beam;
    beam.flavour = BEAM_ELECTRICITY; // used for mons_adjust_flavoured

    dprf("Static discharge on (%d,%d) pow: %d", where.x, where.y, pow);
    if (where == you.pos())
    {
        mpr("You are struck by lightning.");
        damage = 1 + random2(3 + pow / 15);
        dprf("You: static discharge damage: %d", damage);
        damage = check_your_resists(damage, BEAM_ELECTRICITY,
                                    "static discharge");
        if (you.airborne())
            damage /= 2;
        ouch(damage, NON_MONSTER, KILLED_BY_WILD_MAGIC, "static electricity");
    }
    else if (mons == NULL)
        return 0;
    else if (mons->res_elec() > 0 || mons_flies(mons))
        return 0;
    else
    {
        damage = 3 + random2(5 + pow / 10 + (random2(pow) / 10));
        dprf("%s: static discharge damage: %d",
             mons->name(DESC_PLAIN, true).c_str(), damage);
        damage = mons_adjust_flavoured(mons, beam, damage);

        if (damage)
        {
            mprf("%s is struck by lightning.",
                 mons->name(DESC_THE).c_str());
            _player_hurt_monster(*mons, damage);
        }
    }

    // Recursion to give us chain-lightning -- bwr
    // Low power slight chance added for low power characters -- bwr
    if ((pow >= 10 && !one_chance_in(4)) || (pow >= 3 && one_chance_in(10)))
    {
        mpr("The lightning arcs!");
        pow /= (coinflip() ? 2 : 3);
        damage += apply_random_around_square(_discharge_monsters, where,
                                             true, pow, 1);
    }
    else if (damage > 0)
    {
        // Only printed if we did damage, so that the messages in
        // cast_discharge() are clean. -- bwr
        mpr("The lightning grounds out.");
    }

    return damage;
}

static bool _safe_discharge(coord_def where, vector<const monster *> &exclude)
{
    for (adjacent_iterator ai(where); ai; ++ai)
    {
        const monster *mon = monster_at(*ai);
        if (!mon)
            continue;

        if (find(exclude.begin(), exclude.end(), mon) == exclude.end())
        {
            // Harmless to these monsters, so don't prompt about hitting them
            if (mon->res_elec() > 0 || mons_flies(mon))
                continue;

            if (stop_attack_prompt(mon, false, where))
                return false;

            exclude.push_back(mon);
            if (!_safe_discharge(mon->pos(), exclude))
                return false;
        }
    }

    return true;
}

spret_type cast_discharge(int pow, bool fail)
{
    fail_check();

    vector<const monster *> exclude;
    if (!_safe_discharge(you.pos(), exclude))
        return SPRET_ABORT;

    const int num_targs = 1 + random2(random_range(1, 3) + pow / 20);
    const int dam = apply_random_around_square(_discharge_monsters, you.pos(),
                                               true, pow, num_targs);

    dprf("Arcs: %d Damage: %d", num_targs, dam);

    if (dam == 0)
    {
        if (coinflip())
            mpr("The air around you crackles with electrical energy.");
        else
        {
            const bool plural = coinflip();
            mprf("%s blue arc%s ground%s harmlessly %s you.",
                 plural ? "Some" : "A",
                 plural ? "s" : "",
                 plural ? " themselves" : "s itself",
                 plural ? "around" : (coinflip() ? "beside" :
                                      coinflip() ? "behind" : "before"));
        }
    }
    return SPRET_SUCCESS;
}

static int _disperse_monster(monster* mon, int pow)
{
    if (!mon)
        return 0;

    if (mon->no_tele())
        return 0;

    if (mon->check_res_magic(pow) > 0)
    {
        monster_blink(mon);
        return 1;
    }
    else
    {
        monster_teleport(mon, true);
        return 1;
    }

    return 0;
}

spret_type cast_dispersal(int pow, bool fail)
{
    fail_check();
    if (!apply_monsters_around_square(_disperse_monster, you.pos(), pow))
        mpr("The air shimmers briefly around you.");

    return SPRET_SUCCESS;
}

bool setup_fragmentation_beam(bolt &beam, int pow, const actor *caster,
                              const coord_def target, bool allow_random,
                              bool get_max_distance, bool quiet,
                              const char **what, bool &destroy_wall, bool &hole)
{
    beam.flavour     = BEAM_FRAG;
    beam.glyph       = dchar_glyph(DCHAR_FIRED_BURST);
    beam.beam_source = caster->mindex();
    beam.thrower     = caster->is_player() ? KILL_YOU : KILL_MON;
    beam.ex_size     = 1;
    beam.source      = you.pos();
    beam.hit         = AUTOMATIC_HIT;

    beam.source_name = caster->name(DESC_PLAIN, true);
    beam.aux_source = "by Lee's Rapid Deconstruction"; // for direct attack

    beam.target = target;

    // Number of dice vary... 3 is easy/common, but it can get as high as 6.
    beam.damage = dice_def(0, 5 + pow / 5);

    monster* mon = monster_at(target);
    const dungeon_feature_type grid = grd(target);

    if (target == you.pos())
    {
        const bool petrifying = you.petrifying();
        const bool petrified = you.petrified();

        if (you.form == TRAN_STATUE)
        {
            beam.ex_size    = 2;
            beam.name       = "blast of rock fragments";
            beam.colour     = BROWN;
            beam.damage.num = 3;
            return true;
        }
        else if (petrifying || petrified)
        {
            beam.ex_size    = petrifying ? 1 : 2;
            beam.name       = "blast of petrified fragments";
            beam.colour     = mons_class_colour(player_mons(true));
            beam.damage.num = petrifying ? 2 : 3;
            return true;
        }
        else if (you.form == TRAN_ICE_BEAST) // blast of ice
        {
            beam.name       = "icy blast";
            beam.colour     = WHITE;
            beam.damage.num = 2;
            beam.flavour    = BEAM_ICE;
            return true;
        }

        goto do_terrain;
    }

    // Set up the explosion if there's a visible monster.
    if (mon && (caster->is_monster() || (you.can_see(mon))))
    {
        switch (mon->type)
        {
        case MONS_TOENAIL_GOLEM:
            beam.damage.num = 2;
            beam.name       = "blast of toenail fragments";
            beam.colour     = RED;
            break;

        case MONS_IRON_ELEMENTAL:
        case MONS_IRON_GOLEM:
        case MONS_METAL_GARGOYLE:
            beam.name       = "blast of metal fragments";
            beam.colour     = CYAN;
            beam.damage.num = 4;
            break;

        case MONS_EARTH_ELEMENTAL:
        case MONS_CLAY_GOLEM:
        case MONS_STONE_GOLEM:
        case MONS_STATUE:
        case MONS_GARGOYLE:
            beam.ex_size    = 2;
            beam.name       = "blast of rock fragments";
            beam.colour     = BROWN;
            beam.damage.num = 3;
            break;

        case MONS_SILVER_STATUE:
        case MONS_ORANGE_STATUE:
        case MONS_CRYSTAL_GOLEM:
        case MONS_ROXANNE:
            beam.ex_size    = 2;
            beam.damage.num = 4;
            if (mon->type == MONS_SILVER_STATUE)
            {
                beam.name       = "blast of silver fragments";
                beam.colour     = WHITE;
            }
            else if (mon->type == MONS_ORANGE_STATUE)
            {
                beam.name       = "blast of orange crystal shards";
                beam.colour     = LIGHTRED;
            }
            else if (mon->type == MONS_CRYSTAL_GOLEM)
            {
                beam.name       = "blast of crystal shards";
                beam.colour     = GREEN;
            }
            else
            {
                beam.name       = "blast of sapphire shards";
                beam.colour     = BLUE;
            }
            break;

        default:
            const bool petrifying = mon->petrifying();
            const bool petrified = mon->petrified();

            // Petrifying or petrified monsters can be exploded.
            if (petrifying || petrified)
            {
                beam.ex_size    = petrifying ? 1 : 2;
                beam.name       = "blast of petrified fragments";
                beam.colour     = mons_class_colour(mon->type);
                beam.damage.num = petrifying ? 2 : 3;
                break;
            }
            else if (mon->is_icy()) // blast of ice
            {
                beam.name       = "icy blast";
                beam.colour     = WHITE;
                beam.damage.num = 2;
                beam.flavour    = BEAM_ICE;
                break;
            }
            else if (mon->is_skeletal()) // blast of bone
            {
                beam.name   = "blast of bone shards";
                beam.colour = LIGHTGREY;
                beam.damage.num = 2;
                break;
            }
            // Targeted monster not shatterable, try the terrain instead.
            goto do_terrain;
        }

        beam.aux_source = beam.name;

        // Got a target, let's blow it up.
        return true;
    }

  do_terrain:

    if (env.markers.property_at(target, MAT_ANY,
                                "veto_fragmentation") == "veto")
    {
        if (caster->is_player() && !quiet)
        {
            mprf("%s seems to be unnaturally hard.",
                 feature_description_at(target, false, DESC_THE, false).c_str());
        }
        return false;
    }

    switch (grid)
    {
    // Stone and rock terrain
    case DNGN_ORCISH_IDOL:
        if (!caster->is_player())
            return false; // don't let monsters blow up orcish idols

        if (what && (*what == NULL))
            *what = "stone idol";
        // fall-through
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_STONE_WALL:
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_CLEAR_STONE_WALL:
        if (what && (*what == NULL))
            *what = "wall";
        // fall-through
    case DNGN_GRANITE_STATUE:   // normal rock -- big explosion
        if (what && (*what == NULL))
            *what = "statue";

        beam.name       = "blast of rock fragments";
        beam.damage.num = 3;

        if ((grid == DNGN_ORCISH_IDOL
             || grid == DNGN_GRANITE_STATUE
             || grid == DNGN_GRATE
             || pow >= 40 && grid == DNGN_ROCK_WALL
                 && (allow_random && one_chance_in(3)
                     || !allow_random && get_max_distance)
             || pow >= 40 && grid == DNGN_CLEAR_ROCK_WALL
                 && (allow_random && one_chance_in(3)
                     || !allow_random && get_max_distance)
             || pow >= 60 && grid == DNGN_STONE_WALL
                 && (allow_random && one_chance_in(10)
                     || !allow_random && get_max_distance)
             || pow >= 60 && grid == DNGN_CLEAR_STONE_WALL
                 && (allow_random && one_chance_in(10)
                     || !allow_random && get_max_distance)))
        {
            beam.ex_size = 2;
            destroy_wall = true;
        }
        break;

    // Metal -- small but nasty explosion
    case DNGN_METAL_WALL:
        if (what)
            *what = "metal wall";
        // fall through
    case DNGN_GRATE:
        if (what && (*what == NULL))
            *what = "iron grate";
        beam.name       = "blast of metal fragments";
        beam.damage.num = 4;

        if (pow >= 80 && (allow_random && x_chance_in_y(pow / 5, 500)
                          || !allow_random && get_max_distance)
            || grid == DNGN_GRATE)
        {
            beam.damage.num += 2;
            destroy_wall     = true;
        }
        break;

    // Crystal
    case DNGN_GREEN_CRYSTAL_WALL:       // crystal -- large & nasty explosion
        if (what)
            *what = "crystal wall";
        beam.ex_size    = 2;
        beam.name       = "blast of crystal shards";
        beam.damage.num = 4;

        if (allow_random && coinflip()
            || !allow_random && get_max_distance)
        {
            beam.ex_size = 3;
            destroy_wall = true;
        }
        break;

    // Stone doors and arches

    case DNGN_OPEN_DOOR:
    case DNGN_CLOSED_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_SEALED_DOOR:
        // Doors always blow up, stone arches never do (would cause problems).
        if (what)
            *what = "door";
        destroy_wall = true;

        // fall-through
    case DNGN_STONE_ARCH:          // Floor -- small explosion.
        if (what && (*what == NULL))
            *what = "stone arch";
        hole            = false;  // to hit monsters standing on doors
        beam.name       = "blast of rock fragments";
        beam.damage.num = 2;
        break;

    default:
        // Couldn't find a monster or wall to shatter - abort casting!
        if (caster->is_player() && !quiet)
            mpr("You can't deconstruct that!");
        return false;
    }

    // If it was recoloured, use that colour instead.
    if (env.grid_colours(target))
        beam.colour = env.grid_colours(target);
    else
    {
        beam.colour = element_colour(get_feature_def(grid).colour,
                                     false, target);
    }

    beam.aux_source = beam.name;

    return true;
}

spret_type cast_fragmentation(int pow, const actor *caster,
                              const coord_def target, bool fail)
{
    bool destroy_wall = false;
    bool hole         = true;
    const char *what  = NULL;
    const dungeon_feature_type grid = grd(target);

    bolt beam;

    if (!setup_fragmentation_beam(beam, pow, caster, target, true, false,
                                  false, &what, destroy_wall, hole))
    {
        return SPRET_ABORT;
    }

    if (caster->is_player())
    {
        if (grid == DNGN_ORCISH_IDOL)
        {
            if (!yesno("Really insult Beogh by defacing this idol?",
                       false, 'n'))
            {
                canned_msg(MSG_OK);
                return SPRET_ABORT;
            }
        }

        bolt tempbeam;
        bool temp;
        setup_fragmentation_beam(tempbeam, pow, caster, target, false, true,
                                 true, NULL, temp, temp);
        tempbeam.is_tracer = true;
        tempbeam.explode(false);
        if (tempbeam.beam_cancelled)
        {
            canned_msg(MSG_OK);
            return SPRET_ABORT;
        }
    }

    monster* mon = monster_at(target);

    fail_check();

    if (what != NULL) // Terrain explodes.
    {
        mprf("The %s shatters!", what);
        if (destroy_wall)
            nuke_wall(target);
    }
    else if (target == you.pos()) // You explode.
    {
        mpr("You shatter!");

        ouch(beam.damage.roll(), caster->mindex(), KILLED_BY_BEAM,
             "by Lee's Rapid Deconstruction", true,
             caster->is_player() ? "themself"
                                 : caster->name(DESC_A).c_str());
    }
    else // Monster explodes.
    {
        mprf("%s shatters!", mon->name(DESC_THE).c_str());

        if ((mons_is_statue(mon->type) || mon->is_skeletal())
             && x_chance_in_y(pow / 5, 50)) // potential insta-kill
        {
            monster_die(mon,
                        caster->is_player() ? KILL_YOU
                                            : KILL_MON,
                        NON_MONSTER);
            beam.damage.num += 2;
        }
        else if (caster->is_player())
        {
            if (_player_hurt_monster(*mon, beam.damage.roll(),
                                     BEAM_DISINTEGRATION))
            {
                beam.damage.num += 2;
            }
        }
        else
        {
            mon->hurt(caster, beam.damage.roll(), BEAM_DISINTEGRATION);
            if (!mon->alive())
                beam.damage.num += 2;
        }
    }

    beam.explode(true, hole);

    // Monsters shouldn't be able to blow up idols,
    // but this check is here just in case...
    if (caster->is_player() && grid == DNGN_ORCISH_IDOL)
        did_god_conduct(DID_DESTROY_ORCISH_IDOL, 8);

    return SPRET_SUCCESS;
}

int wielding_rocks()
{
    const item_def* wpn = you.weapon();
    if (!wpn || wpn->base_type != OBJ_MISSILES)
        return 0;
    else if (wpn->sub_type == MI_STONE)
        return 1;
    else if (wpn->sub_type == MI_LARGE_ROCK)
        return 2;
    else
        return 0;
}

spret_type cast_sandblast(int pow, bolt &beam, bool fail)
{
    zap_type zap = ZAP_SMALL_SANDBLAST;
    switch (wielding_rocks())
    {
    case 1:
        zap = ZAP_SANDBLAST;
        break;
    case 2:
        zap = ZAP_LARGE_SANDBLAST;
        break;
    default:
        break;
    }

    const spret_type ret = zapping(zap, pow, beam, true, NULL, fail);

    if (ret == SPRET_SUCCESS && zap != ZAP_SMALL_SANDBLAST)
        dec_inv_item_quantity(you.equip[EQ_WEAPON], 1);

    return ret;
}

static bool _elec_not_immune(const actor *act)
{
    return act->res_elec() < 3;
}

spret_type cast_thunderbolt(actor *caster, int pow, coord_def aim, bool fail)
{
    coord_def prev;
    if (caster->props.exists("thunderbolt_last")
        && caster->props["thunderbolt_last"].get_int() + 1 == you.num_turns)
    {
        prev = caster->props["thunderbolt_aim"].get_coord();
    }

    targetter_thunderbolt hitfunc(caster, spell_range(SPELL_THUNDERBOLT, pow),
                                  prev);
    hitfunc.set_aim(aim);

    if (caster->is_player())
    {
        if (stop_attack_prompt(hitfunc, "zap", _elec_not_immune))
            return SPRET_ABORT;
    }

    fail_check();

    int juice = prev.origin() ? 2 * ROD_CHARGE_MULT
                              : caster->props["thunderbolt_mana"].get_int();
    bolt beam;
    beam.name              = "lightning";
    beam.aux_source        = "rod of lightning";
    beam.flavour           = BEAM_ELECTRICITY;
    beam.glyph             = dchar_glyph(DCHAR_FIRED_BURST);
    beam.colour            = LIGHTCYAN;
    beam.range             = 1;
    // Dodging a horizontal arc is nearly impossible: you'd have to fall prone
    // or jump high.
    beam.hit               = prev.origin() ? 10 + pow / 20 : 1000;
    beam.ac_rule           = AC_PROPORTIONAL;
    beam.set_agent(caster);
#ifdef USE_TILE
    beam.tile_beam = -1;
#endif
    beam.draw_delay = 0;

    for (map<coord_def, aff_type>::const_iterator p = hitfunc.zapped.begin();
         p != hitfunc.zapped.end(); ++p)
    {
        if (p->second <= 0)
            continue;

        beam.draw(p->first);
    }

    delay(200);

    beam.glyph = 0; // FIXME: a hack to avoid "appears out of thin air"

    for (map<coord_def, aff_type>::const_iterator p = hitfunc.zapped.begin();
         p != hitfunc.zapped.end(); ++p)
    {
        if (p->second <= 0)
            continue;

        // beams are incredibly spammy in debug mode
        if (!actor_at(p->first))
            continue;

        int arc = hitfunc.arc_length[p->first.range(hitfunc.origin)];
        ASSERT(arc > 0);
        dprf("at distance %d, arc length is %d", p->first.range(hitfunc.origin),
                                                 arc);
        beam.source = beam.target = p->first;
        beam.source.x -= sgn(beam.source.x - hitfunc.origin.x);
        beam.source.y -= sgn(beam.source.y - hitfunc.origin.y);
        beam.damage = dice_def(div_rand_round(juice, ROD_CHARGE_MULT),
                               div_rand_round(30 + pow / 6, arc + 2));
        beam.fire();
    }

    caster->props["thunderbolt_last"].get_int() = you.num_turns;
    caster->props["thunderbolt_aim"].get_coord() = aim;

    noisy(15 + div_rand_round(juice, ROD_CHARGE_MULT), hitfunc.origin);

    return SPRET_SUCCESS;
}

// Find an enemy who would suffer from Awaken Forest.
actor* forest_near_enemy(const actor *mon)
{
    const coord_def pos = mon->pos();

    for (radius_iterator ri(pos, LOS_RADIUS); ri; ++ri)
    {
        actor* foe = actor_at(*ri);
        if (!foe || mons_aligned(foe, mon))
            continue;

        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (feat_is_tree(grd(*ai)) && cell_see_cell(pos, *ai, LOS_DEFAULT))
                return foe;
    }

    return NULL;
}

// Print a message only if you can see any affected trees.
void forest_message(const coord_def pos, const string &msg, msg_channel_type ch)
{
    for (radius_iterator ri(pos, LOS_RADIUS); ri; ++ri)
        if (feat_is_tree(grd(*ri))
            && cell_see_cell(you.pos(), *ri, LOS_DEFAULT)
            && cell_see_cell(pos, *ri, LOS_DEFAULT))
        {
            mpr(msg, ch);
            return;
        }
}

void forest_damage(const actor *mon)
{
    const coord_def pos = mon->pos();
    const int hd = mon->get_experience_level();

    if (one_chance_in(4))
        forest_message(pos, random_choose(
            "The trees move their gnarly branches around.",
            "You feel roots moving beneath the ground.",
            "Branches wave dangerously above you.",
            "Trunks creak and shift.",
            "Tree limbs sway around you.",
            0), MSGCH_TALK_VISUAL);

    for (radius_iterator ri(pos, LOS_RADIUS); ri; ++ri)
    {
        actor* foe = actor_at(*ri);
        if (!foe || mons_aligned(foe, mon))
            continue;

        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (feat_is_tree(grd(*ai)) && cell_see_cell(pos, *ai, LOS_DEFAULT))
            {
                int evnp = foe->melee_evasion(mon, EV_IGNORE_PHASESHIFT);
                int dmg = 0;
                string msg;

                if (!apply_chunked_AC(1, evnp))
                {
                    msg = random_choose(
                            "@foe@ @is@ waved at by a branch.",
                            "A tree reaches out but misses @foe@.",
                            "A root lunges up near @foe@.",
                            0);
                }
                else if (!apply_chunked_AC(1, foe->melee_evasion(mon) - evnp))
                {
                    msg = random_choose(
                            "A branch passes through @foe@!",
                            "A tree reaches out and and passes through @foe@!",
                            "A root lunges and passes through @foe@ from below.",
                            0);
                }
                else if (!(dmg = foe->apply_ac(div_rand_round(hd, 2) + random2(hd),
                                               (hd * 3 - 1) / 2, AC_PROPORTIONAL)))
                {
                    msg = random_choose(
                            "@foe@ @is@ scraped by a branch!",
                            "A tree reaches out and scrapes @foe@!",
                            "A root barely touches @foe@ from below.",
                            0);
                }
                else
                {
                    msg = random_choose(
                        "@foe@ @is@ hit by a branch!",
                        "A tree reaches out and hits @foe@!",
                        "A root smacks @foe@ from below.",
                        0);
                }

                msg = replace_all(replace_all(msg,
                    // "it" looks butt-ugly here...
                    "@foe@", foe->visible_to(&you) ? foe->name(DESC_THE)
                                                   : "something"),
                    "@is@", foe->is_player() ? "are" : "is");
                if (you.see_cell(foe->pos()))
                    mpr(msg.c_str());

                if (dmg <= 0)
                    break;

                if (foe->is_player())
                {
                    ouch(dmg, mon->mindex(), KILLED_BY_BEAM,
                         "angry trees", true);
                }
                else
                    foe->hurt(mon, dmg);

                break;
            }
    }
}

vector<bolt> get_spray_rays(const actor *caster, coord_def aim, int range, int max_rays)
{
    coord_def aim_dir = (caster->pos() - aim).sgn();

    int num_targets = 0;
    vector<bolt> beams;
    int range2 = dist_range(range);

    bolt base_beam;

    base_beam.set_agent(const_cast<actor *>(caster));
    base_beam.attitude = ATT_FRIENDLY;
    base_beam.is_tracer = true;
    base_beam.is_targetting = true;
    base_beam.range = range;
    base_beam.source = caster->pos();
    base_beam.target = aim;

    bolt center_beam = base_beam;
    center_beam.hit = AUTOMATIC_HIT;
    center_beam.fire();
    center_beam.target = center_beam.path_taken.back();
    center_beam.hit = 1;
    center_beam.fire();
    center_beam.is_tracer = false;
    beams.push_back(center_beam);

    for (distance_iterator di(aim, false, false, 3); di; ++di)
    {
        if (monster_at(*di))
        {
            coord_def delta = caster->pos() - *di;

            //Don't aim secondary rays at friendlies
            if ((caster->is_player() ? monster_at(*di)->attitude != ATT_HOSTILE
                    : monster_at(*di)->attitude != ATT_FRIENDLY))
                continue;

            if (!caster->can_see(monster_at(*di)))
                continue;

            //Don't try to aim at a target if it's out of range
            if (delta.abs() > range2)
                continue;

            //Don't try to aim at targets in the opposite direction of main aim
            if (abs(aim_dir.x - delta.sgn().x) + abs(aim_dir.y - delta.sgn().y) >= 2)
                continue;

            //Test if this beam stops at a location used by any prior beam
            bolt testbeam = base_beam;
            testbeam.target = *di;
            testbeam.hit = AUTOMATIC_HIT;
            testbeam.fire();
            bool duplicate = false;

            for (unsigned int i = 0; i < beams.size(); ++i)
            {
                if (testbeam.path_taken.back() == beams[i].target)
                {
                    duplicate = true;
                    continue;
                }
            }
            if (!duplicate)
            {
                bolt tempbeam = base_beam;
                tempbeam.target = testbeam.path_taken.back();
                tempbeam.fire();
                tempbeam.is_tracer = false;
                base_beam.is_targetting = false;
                beams.push_back(tempbeam);
                num_targets++;
            }

            if (num_targets == max_rays - 1)
              break;
        }
    }

    return beams;
}

static bool _dazzle_can_hit(const actor *act)
{
    if (act->is_monster())
    {
        const monster* mons = act->as_monster();
        bolt testbeam;
        testbeam.thrower = KILL_YOU;
        zappy(ZAP_DAZZLING_SPRAY, 100, testbeam);

        return (mons->type != MONS_BATTLESPHERE
                && mons->type != MONS_ORB_OF_DESTRUCTION
                && mons_species(mons->type) != MONS_BUSH
                && !fedhas_shoot_through(testbeam, mons));
    }
    else
        return false;
}

spret_type cast_dazzling_spray(actor *caster, int pow, coord_def aim, bool fail)
{
    int range = spell_range(SPELL_DAZZLING_SPRAY, pow);

    targetter_spray hitfunc(&you, range, ZAP_DAZZLING_SPRAY);

    hitfunc.set_aim(aim);

    if (caster->is_player())
    {
        if (stop_attack_prompt(hitfunc, "fire towards", _dazzle_can_hit))
            return SPRET_ABORT;
    }

    fail_check();

    vector<bolt> beams = get_spray_rays(caster, aim, range, 3);

    if (beams.size() == 0)
    {
        mpr("You can't see any targets in that direction!");
        return SPRET_ABORT;
    }

    for (unsigned int i = 0; i < beams.size(); ++i)
    {
        zappy(ZAP_DAZZLING_SPRAY, pow, beams[i]);
        beams[i].fire();
    }

    return SPRET_SUCCESS;
}
