/*
 *  File:       spells1.cc
 *  Summary:    Implementations of some additional spells.
 *              Mostly Translocations.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "spells1.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include "externs.h"

#include "abyss.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "describe.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "invent.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "los.h"
#include "message.h"
#include "misc.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "player-stats.h"
#include "religion.h"
#include "godconduct.h"
#include "skills2.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"

static bool _abyss_blocks_teleport(bool cblink)
{
    // Lugonu worshippers get their perks.
    if (you.religion == GOD_LUGONU)
        return (false);

    // Controlled Blink (the spell) works quite reliably in the Abyss.
    return (cblink ? one_chance_in(3) : !one_chance_in(3));
}

// If wizard_blink is set, all restriction are ignored (except for
// a monster being at the target spot), and the player gains no
// contamination.
int blink(int pow, bool high_level_controlled_blink, bool wizard_blink)
{
    ASSERT(!crawl_state.game_is_arena());

    dist beam;

    if (crawl_state.is_repeating_cmd())
    {
        crawl_state.cant_cmd_repeat("You can't repeat controlled blinks.");
        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return (1);
    }

    // yes, there is a logic to this ordering {dlb}:
    if (item_blocks_teleport(true, true) && !wizard_blink)
        canned_msg(MSG_STRANGE_STASIS);
    else if (you.level_type == LEVEL_ABYSS
             && _abyss_blocks_teleport(high_level_controlled_blink)
             && !wizard_blink)
    {
        mpr("The power of the Abyss keeps you in your place!");
    }
    else if (you.confused() && !wizard_blink)
        random_blink(false);
    else if (!allow_control_teleport(true) && !wizard_blink)
    {
        mpr("A powerful magic interferes with your control of the blink.");
        if (high_level_controlled_blink)
            return (cast_semi_controlled_blink(pow));
        random_blink(false);
    }
    else
    {
        // query for location {dlb}:
        while (!crawl_state.seen_hups)
        {
            direction_chooser_args args;
            args.restricts = DIR_TARGET;
            args.needs_path = false;
            args.may_target_monster = false;
            args.top_prompt = "Blink to where?";
            direction(beam, args);

            if (!beam.isValid || beam.target == you.pos())
            {
                if (!wizard_blink
                    && !yesno("Are you sure you want to cancel this blink?",
                              false, 'n'))
                {
                    mesclr();
                    continue;
                }
                canned_msg(MSG_OK);
                return (-1);         // early return {dlb}
            }

            monsters* beholder = you.get_beholder(beam.target);
            if (!wizard_blink && beholder)
            {
                mprf("You cannot blink away from %s!",
                    beholder->name(DESC_NOCAP_THE, true).c_str());
                continue;
            }

            if (grd(beam.target) == DNGN_OPEN_SEA)
            {
                mesclr();
                mpr("You can't blink into the sea!");
            }
            else if (you.see_cell_no_trans(beam.target))
            {
                // Grid in los, no problem.
                break;
            }
            else if (you.trans_wall_blocking( beam.target ))
            {
                // Wizard blink can move past translucent walls.
                if (wizard_blink)
                    break;

                mesclr();
                mpr("You can't blink through translucent walls.");
            }
            else
            {
                mesclr();
                mpr("You can only blink to visible locations.");
            }
        }

        // Allow wizard blink to send player into walls, in case the
        // user wants to alter that grid to something else.
        if (wizard_blink && feat_is_solid(grd(beam.target)))
            grd(beam.target) = DNGN_FLOOR;

        if (feat_is_solid(grd(beam.target)) || monster_at(beam.target))
        {
            mpr("Oops! Maybe something was there already.");
            random_blink(false);
        }
        else if (you.level_type == LEVEL_ABYSS && !wizard_blink)
        {
            abyss_teleport( false );
            if (you.pet_target != MHITYOU)
                you.pet_target = MHITNOT;
        }
        else
        {
            // Leave a purple cloud.
            place_cloud(CLOUD_TLOC_ENERGY, you.pos(), 1 + random2(3), KC_YOU);
            move_player_to_grid(beam.target, false, true, true);

            // Controlling teleport contaminates the player. -- bwr
            if (!wizard_blink)
                contaminate_player( 1, true );


            if (!wizard_blink && you.duration[DUR_CONDENSATION_SHIELD] > 0)
                remove_condensation_shield();
        }
    }

    crawl_state.cancel_cmd_again();
    crawl_state.cancel_cmd_repeat();

    return (1);
}

void random_blink(bool allow_partial_control, bool override_abyss)
{
    ASSERT(!crawl_state.game_is_arena());

    bool success = false;
    coord_def target;

    if (item_blocks_teleport(true, true))
        canned_msg(MSG_STRANGE_STASIS);
    else if (you.level_type == LEVEL_ABYSS
             && !override_abyss && !one_chance_in(3))
    {
        mpr("The power of the Abyss keeps you in your place!");
    }
    // First try to find a random square not adjacent to the player,
    // then one adjacent if that fails.
    else if (!random_near_space(you.pos(), target)
             && !random_near_space(you.pos(), target, true))
    {
        mpr("You feel jittery for a moment.");
    }

#ifdef USE_SEMI_CONTROLLED_BLINK
    //jmf: Add back control, but effect is cast_semi_controlled_blink(pow).
    else if (player_control_teleport() && !you.confused()
             && allow_partial_control && allow_control_teleport())
    {
        mpr("You may select the general direction of your translocation.");
        cast_semi_controlled_blink(100);
        maybe_id_ring_TC();
        success = true;
    }
#endif
    else
    {
        // Going to assume that move_player_to_grid() works.  (It should
        // because terrain type, etc. was already checked.)  This could
        // result in awkward messaging if it cancels for some reason,
        // but it's probably better than getting the blink message after
        // any Mf transform messages all the time. -cao
        canned_msg(MSG_YOU_BLINK);
        coord_def origin = you.pos();
        success = move_player_to_grid(target, false, true, true);
        if (success)
        {
            // Leave a purple cloud.
            place_cloud(CLOUD_TLOC_ENERGY, origin, 1 + random2(3), KC_YOU);

            if (you.level_type == LEVEL_ABYSS)
            {
                abyss_teleport(false);
                if (you.pet_target != MHITYOU)
                    you.pet_target = MHITNOT;
            }
        }
    }

    if (success && you.duration[DUR_CONDENSATION_SHIELD] > 0)
        remove_condensation_shield();
}

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

    viewwindow(false);
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

bool _lightning_los(const coord_def& source, const coord_def& target)
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

void identify(int power, int item_slot)
{
    int id_used = 1;

    // Scrolls of identify *may* produce "extra" identifications.
    if (power == -1 && one_chance_in(5))
        id_used += (coinflip()? 1 : 2);

    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Identify which item?", MT_INVLIST,
                                           OSEL_UNIDENT, true, true, false);
        }
        if (prompt_failed(item_slot))
            return;

        item_def& item(you.inv[item_slot]);

        if (fully_identified(item))
        {
            mpr("Choose an unidentified item, or Esc to abort.");
            if (Options.auto_list)
                more();
            item_slot = -1;
            continue;
        }

        set_ident_type(item, ID_KNOWN_TYPE);
        set_ident_flags(item, ISFLAG_IDENT_MASK);
        if (Options.autoinscribe_artefacts && is_artefact(item))
            add_autoinscription( item, artefact_auto_inscription(item));

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
        mpr(item.name(DESC_INVENTORY_EQUIP).c_str());
        if (item_slot == you.equip[EQ_WEAPON])
            you.wield_change = true;

        id_used--;

        if (Options.auto_list && id_used > 0)
            more();

        // In case we get to try again.
        item_slot = -1;
    }
    while (id_used > 0);
}

// Returns whether the spell was actually cast.
bool conjure_flame(int pow, const coord_def& where)
{
    // FIXME: This would be better handled by a flag to enforce max range.
    if (grid_distance(where, you.pos()) > spell_range(SPELL_CONJURE_FLAME,
                                                      pow, true)
        || !in_bounds(where))
    {
        mpr("That's too far away.");
        return (false);
    }

    if (you.trans_wall_blocking(where))
    {
        mpr("A translucent wall is in the way.");
        return (false);
    }

    if (cell_is_solid(where))
    {
        if (grd(where) == DNGN_WAX_WALL)
            mpr("The flames aren't hot enough to melt wax walls!");
        else
            mpr("You can't ignite solid rock!");
        return (false);
    }

    const int cloud = env.cgrid(where);

    if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE)
    {
        mpr("There's already a cloud there!");
        return (false);
    }

    // Note that self-targeting is handled by SPFLAG_NOT_SELF.
    monsters *monster = monster_at(where);
    if (monster)
    {
        if (you.can_see(monster))
        {
            mpr("You can't place the cloud on a creature.");
            return (false);
        }
        else
        {
            // FIXME: maybe should do _paranoid_option_disable() here?
            mpr("You see a ghostly outline there, and the spell fizzles.");
            return (true);      // Don't give free detection!
        }
    }

    if (cloud != EMPTY_CLOUD)
    {
        // Reinforce the cloud - but not too much.
        // It must be a fire cloud from a previous test.
        mpr("The fire roars with new energy!");
        const int extra_dur = 2 + std::min(random2(pow) / 2, 20);
        env.cloud[cloud].decay += extra_dur * 5;
        env.cloud[cloud].set_whose(KC_YOU);
    }
    else
    {
        const int durat = std::min(5 + (random2(pow)/2) + (random2(pow)/2), 23);
        place_cloud(CLOUD_FIRE, where, durat, KC_YOU);
    }

    return (true);
}

// Assumes beem.range has already been set. -cao
bool stinking_cloud( int pow, bolt &beem )
{
    beem.name        = "stinking cloud";
    beem.colour      = GREEN;
    beem.damage      = dice_def( 1, 0 );
    beem.hit         = 20;
    beem.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
    beem.flavour     = BEAM_POTION_STINKING_CLOUD;
    beem.ench_power  = pow;
    beem.beam_source = MHITYOU;
    beem.thrower     = KILL_YOU;
    beem.is_beam     = false;
    beem.is_explosion = true;
    beem.aux_source.clear();

    // Don't bother tracing if you're targeting yourself.
    if (beem.target != you.pos())
    {
        // Fire tracer.
        beem.source            = you.pos();
        beem.can_see_invis     = you.can_see_invisible();
        beem.smart_monster     = true;
        beem.attitude          = ATT_FRIENDLY;
        beem.friend_info.count = 0;
        beem.is_tracer         = true;
        beem.fire();

        if (beem.beam_cancelled)
        {
            // We don't want to fire through friendlies.
            canned_msg(MSG_OK);
            return (false);
        }
    }

    // Really fire.
    beem.is_tracer = false;
    beem.fire();

    return (true);
}

int cast_big_c(int pow, cloud_type cty, kill_category whose, bolt &beam)
{
    big_cloud( cty, whose, beam.target, pow, 8 + random2(3), -1 );
    return (1);
}

void big_cloud(cloud_type cl_type, kill_category whose,
               const coord_def& where, int pow, int size, int spread_rate,
               int colour, std::string name, std::string tile)
{
    big_cloud(cl_type, whose, cloud_struct::whose_to_killer(whose),
              where, pow, size, spread_rate, colour, name, tile);
}

void big_cloud(cloud_type cl_type, killer_type killer,
               const coord_def& where, int pow, int size, int spread_rate,
               int colour, std::string name, std::string tile)
{
    big_cloud(cl_type, cloud_struct::killer_to_whose(killer), killer,
              where, pow, size, spread_rate, colour, name, tile);
}

void big_cloud(cloud_type cl_type, kill_category whose, killer_type killer,
               const coord_def& where, int pow, int size, int spread_rate,
               int colour, std::string name, std::string tile)
{
    apply_area_cloud(make_a_normal_cloud, where, pow, size,
                     cl_type, whose, killer, spread_rate, colour, name, tile);
}

static bool _mons_hostile(const monsters *mon)
{
    // Needs to be done this way because of friendly/neutral enchantments.
    return (!mon->wont_attack() && !mon->neutral());
}

static bool _can_pacify_monster(const monsters *mon, const int healed)
{
    if (you.religion != GOD_ELYVILON)
        return (false);

    if (healed < 1)
        return (false);

    // I was thinking of jellies when I wrote this, but maybe we shouldn't
    // exclude zombies and such... (jpeg)
    if (mons_intel(mon) <= I_PLANT // no self-awareness
        || mon->type == MONS_KRAKEN_TENTACLE) // body part
    {
        return (false);
    }

    const mon_holy_type holiness = mon->holiness();

    if (!mon->is_holy()
        && holiness != MH_UNDEAD
        && holiness != MH_DEMONIC
        && holiness != MH_NATURAL)
    {
        return (false);
    }

    if (mons_is_stationary(mon)) // not able to leave the level
        return (false);

    if (mon->asleep()) // not aware of what is happening
        return (false);

    const int factor = (mons_intel(mon) <= I_ANIMAL)       ? 3 : // animals
                       (is_player_same_species(mon->type)) ? 2   // same species
                                                           : 1;  // other

    int divisor = 3;

    if (mon->is_holy())
        divisor--;
    else if (holiness == MH_UNDEAD)
        divisor++;
    else if (holiness == MH_DEMONIC)
        divisor += 2;

    const int random_factor = random2((you.skills[SK_INVOCATIONS] + 1) *
                                      healed / divisor);

    dprf("pacifying %s? max hp: %d, factor: %d, Inv: %d, healed: %d, rnd: %d",
         mon->name(DESC_PLAIN).c_str(), mon->max_hit_points, factor,
         you.skills[SK_INVOCATIONS], healed, random_factor);

    if (mon->max_hit_points < factor * random_factor)
        return (true);

    return (false);
}

// Returns: 1 -- success, 0 -- failure, -1 -- cancel
static int _healing_spell(int healed, bool divine_ability,
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
                                      you.religion == GOD_ELYVILON ?
                                            TARG_ANY : TARG_FRIEND,
                                      LOS_RADIUS,
                                      false, true, true, "Heal", NULL);
    }
    else
    {
        spd.target  = where;
        spd.isValid = in_bounds(spd.target);
    }

    if (!spd.isValid)
        return (-1);

    if (spd.target == you.pos())
    {
        if (not_self)
        {
            mpr("You can only heal others!");
            return (-1);
        }

        mpr("You are healed.");
        inc_hp(healed, false);
        return (1);
    }

    monsters* monster = monster_at(spd.target);
    if (!monster)
    {
        mpr("There isn't anything there!");
        // This isn't a cancel, to avoid leaking invisible monster
        // locations.
        return (0);
    }

    const bool can_pacify = _can_pacify_monster(monster, healed);
    const bool is_hostile = _mons_hostile(monster);

    // Don't divinely heal a monster you can't pacify.
    if (divine_ability
        && you.religion == GOD_ELYVILON
        && !can_pacify)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (0);
    }

    bool did_something = false;

    if (you.religion == GOD_ELYVILON
        && can_pacify && is_hostile)
    {
        did_something = true;

        const bool is_holy     = monster->is_holy();
        const bool is_summoned = monster->is_summoned();

        int pgain = 0;
        if (!is_holy && !is_summoned && you.piety < MAX_PIETY)
        {
            pgain = random2(1 + random2(monster->max_hit_points /
                            (2 + you.piety / 20)));
        }

        if (pgain > 0)
            simple_god_message(" approves of your offer of peace.");
        else
            mpr("Elyvilon supports your offer of peace.");

        if (is_holy)
            good_god_holy_attitude_change(monster);
        else
        {
            simple_monster_message(monster, " turns neutral.");
            mons_pacify(monster, ATT_NEUTRAL);

            // Give a small piety return.
            if (pgain > 0)
                gain_piety(pgain);
        }
    }

    if (monster->heal(healed))
    {
        did_something = true;
        mprf("You heal %s.", monster->name(DESC_NOCAP_THE).c_str());

        if (monster->hit_points == monster->max_hit_points)
            simple_monster_message(monster, " is completely healed.");
        else
            print_wounds(monster);

        if (you.religion == GOD_ELYVILON && !is_hostile)
        {
            int pgain = 0;
            if (one_chance_in(8) && you.piety < MAX_PIETY)
                pgain = 1;

            if (pgain > 0)
            {
                simple_god_message(" approves of your healing of a fellow "
                                   "creature.");
            }
            else
            {
                mpr("Elyvilon appreciates your healing of a fellow "
                    "creature.");
            }

            // Give a small piety return.
            if (pgain > 0)
                gain_piety(pgain);
        }
    }

    if (!did_something)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (0);
    }

    return (1);
}

// Returns: 1 -- success, 0 -- failure, -1 -- cancel
int cast_healing(int pow, bool divine_ability, const coord_def& where,
                 bool not_self, targ_mode_type mode)
{
    pow = std::min(50, pow);
    return (_healing_spell(pow + roll_dice(2, pow) - 2, divine_ability, where,
                           not_self, mode));
}

void remove_divine_vigour()
{
    mpr("Your divine vigour fades away.", MSGCH_DURATION);
    you.duration[DUR_DIVINE_VIGOUR] = 0;
    you.attribute[ATTR_DIVINE_VIGOUR] = 0;
    calc_hp();
    calc_mp();
}

bool cast_divine_vigour()
{
    bool success = false;

    if (!you.duration[DUR_DIVINE_VIGOUR])
    {
        mprf("%s grants you divine vigour.",
             god_name(you.religion).c_str());

        const int vigour_amt = 1 + (you.skills[SK_INVOCATIONS]/6);
        const int old_hp_max = you.hp_max;
        const int old_mp_max = you.max_magic_points;
        you.attribute[ATTR_DIVINE_VIGOUR] = vigour_amt;
        you.set_duration(DUR_DIVINE_VIGOUR,
                         40 + (you.skills[SK_INVOCATIONS]*5)/2);

        calc_hp();
        inc_hp(you.hp_max - old_hp_max, false);
        calc_mp();
        inc_mp(you.max_magic_points - old_mp_max, false);

        success = true;
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

void remove_divine_stamina()
{
    mpr("Your divine stamina fades away.", MSGCH_DURATION);
    notify_stat_change(STAT_STR, -you.attribute[ATTR_DIVINE_STAMINA],
                true, "Zin's divine stamina running out");
    notify_stat_change(STAT_INT, -you.attribute[ATTR_DIVINE_STAMINA],
                true, "Zin's divine stamina running out");
    notify_stat_change(STAT_DEX, -you.attribute[ATTR_DIVINE_STAMINA],
                true, "Zin's divine stamina running out");
    you.duration[DUR_DIVINE_STAMINA] = 0;
    you.attribute[ATTR_DIVINE_STAMINA] = 0;
}

static bool _kill_duration(duration_type dur)
{
    const bool rc = (you.duration[dur] > 0);
    you.duration[dur] = 0;
    return (rc);
}

bool cast_vitalisation()
{
    bool success = false;
    int type = 0;

    // Remove negative afflictions.
    if (you.disease || you.rotting || you.confused()
        || you.duration[DUR_PARALYSIS] || you.duration[DUR_POISONING]
        || you.petrified())
    {
        do
        {
            switch (random2(6))
            {
            case 0:
                if (you.disease)
                {
                    success = true;
                    you.disease = 0;
                }
                break;
            case 1:
                if (you.rotting)
                {
                    success = true;
                    you.rotting = 0;
                }
                break;
            case 2:
                success = _kill_duration(DUR_CONF);
                break;
            case 3:
                success = _kill_duration(DUR_PARALYSIS);
                break;
            case 4:
                success = _kill_duration(DUR_POISONING);
                break;
            case 5:
                success = _kill_duration(DUR_PETRIFIED);
                break;
            }
        }
        while (!success);
    }
    // Restore stats.
    else if (you.strength() < you.max_strength()
             || you.intel() < you.max_intel()
             || you.dex() < you.max_dex())
    {
        type = 1;
        while (!restore_stat(STAT_RANDOM, 0, true))
            ;
        success = true;
    }
    else
    {
        // Add divine stamina.
        if (!you.duration[DUR_DIVINE_STAMINA])
        {
            success = true;
            type = 2;

            mprf("%s grants you divine stamina.",
                 god_name(you.religion).c_str());

            const int stamina_amt = 3;
            you.attribute[ATTR_DIVINE_STAMINA] = stamina_amt;
            you.set_duration(DUR_DIVINE_STAMINA,
                             40 + (you.skills[SK_INVOCATIONS]*5)/2);

            notify_stat_change(STAT_STR, stamina_amt, true, "");
            notify_stat_change(STAT_INT, stamina_amt, true, "");
            notify_stat_change(STAT_DEX, stamina_amt, true, "");
        }
    }

    // If vitalisation has succeeded, display an appropriate message.
    if (success)
    {
        mprf("You feel %s.", (type == 0) ? "better" :
                             (type == 1) ? "renewed"
                                         : "powerful");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

bool cast_revivification(int pow)
{
    bool success = false;

    if (you.hp == you.hp_max)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (you.hp_max < 21)
        mpr("You lack the resilience to cast this spell.");
    else
    {
        mpr("Your body is healed in an amazingly painful way.");

        int loss = 2;
        for (int i = 0; i < 9; ++i)
            if (x_chance_in_y(8, pow))
                loss++;

        dec_max_hp(loss);
        set_hp(you.hp_max, false);
        success = true;
    }

    return (success);
}

void cast_cure_poison(int pow)
{
    if (you.duration[DUR_POISONING] > 0)
        reduce_poison_player(2 + random2(pow) + random2(3));
    else
        canned_msg(MSG_NOTHING_HAPPENS);
}

void purification(void)
{
    mpr("You feel purified!");

    you.disease = 0;
    you.rotting = 0;
    you.duration[DUR_POISONING] = 0;
    you.duration[DUR_CONF] = 0;
    you.duration[DUR_SLOW] = 0;
    you.duration[DUR_PARALYSIS] = 0;          // can't currently happen -- bwr
    you.duration[DUR_PETRIFIED] = 0;
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

// Antimagic is sort of an anti-extension... it sets a lot of magical
// durations to 1 so it's very nasty at times (and potentially lethal,
// that's why we reduce levitation to 2, so that the player has a chance
// to stop insta-death... sure the others could lead to death, but that's
// not as direct as falling into deep water) -- bwr
void antimagic()
{
    duration_type dur_list[] = {
        DUR_INVIS, DUR_CONF, DUR_PARALYSIS, DUR_SLOW, DUR_HASTE,
        DUR_MIGHT, DUR_AGILITY, DUR_BRILLIANCE, DUR_FIRE_SHIELD, DUR_ICY_ARMOUR, DUR_REPEL_MISSILES,
        DUR_REGENERATION, DUR_SWIFTNESS, DUR_STONEMAIL, DUR_CONTROL_TELEPORT,
        DUR_TRANSFORMATION, DUR_DEATH_CHANNEL, DUR_DEFLECT_MISSILES,
        DUR_PHASE_SHIFT, DUR_SEE_INVISIBLE, DUR_WEAPON_BRAND, DUR_SILENCE,
        DUR_CONDENSATION_SHIELD, DUR_STONESKIN, DUR_BARGAIN,
        DUR_INSULATION, DUR_RESIST_POISON, DUR_RESIST_FIRE, DUR_RESIST_COLD,
        DUR_SLAYING, DUR_STEALTH, DUR_MAGIC_SHIELD, DUR_SAGE, DUR_PETRIFIED
    };

    if (!you.permanent_levitation() && !you.permanent_flight()
        && you.duration[DUR_LEVITATION] > 2)
    {
        you.duration[DUR_LEVITATION] = 2;
    }

    if (!you.permanent_flight() && you.duration[DUR_CONTROLLED_FLIGHT] > 1)
        you.duration[DUR_CONTROLLED_FLIGHT] = 1;

    for (unsigned int i = 0; i < ARRAYSZ(dur_list); ++i)
        if (you.duration[dur_list[i]] > 1)
            you.duration[dur_list[i]] = 1;

    contaminate_player(-1 * (1 + random2(5)));
}

void extension(int pow)
{
    int contamination = random2(2);

    if (you.duration[DUR_HASTE])
    {
        potion_effect(POT_SPEED, pow);
        contamination++;
    }

    if (you.duration[DUR_SLOW])
        potion_effect(POT_SLOWING, pow);

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

    if (you.duration[DUR_LEVITATION] && !you.duration[DUR_CONTROLLED_FLIGHT])
        potion_effect(POT_LEVITATION, pow);

    if (you.duration[DUR_INVIS])
    {
        potion_effect(POT_INVISIBILITY, pow);
        contamination++;
    }

    if (you.duration[DUR_ICY_ARMOUR])
        ice_armour(pow, true);

    if (you.duration[DUR_REPEL_MISSILES])
        missile_prot(pow);

    if (you.duration[DUR_REGENERATION])
        cast_regen(pow);

    if (you.duration[DUR_DEFLECT_MISSILES])
        deflection(pow);

    if (you.duration[DUR_FIRE_SHIELD])
    {
        you.increase_duration(DUR_FIRE_SHIELD, random2(pow / 20), 50);
        mpr("Your ring of flames roars with new vigour!");
    }

    if ( !you.duration[DUR_WEAPON_BRAND] < 1)
    {
        you.increase_duration(DUR_WEAPON_BRAND, 5 + random2(8), 80);
    }

    if (you.duration[DUR_SWIFTNESS])
        cast_swiftness(pow);

    if (you.duration[DUR_INSULATION])
        cast_insulation(pow);

    if (you.duration[DUR_STONEMAIL])
        stone_scales(pow);

    if (you.duration[DUR_CONTROLLED_FLIGHT])
        cast_fly(pow);

    if (you.duration[DUR_CONTROL_TELEPORT])
        cast_teleport_control(pow);

    if (you.duration[DUR_RESIST_POISON])
        cast_resist_poison(pow);

    if (you.duration[DUR_TRANSFORMATION]
        && (you.species != SP_VAMPIRE
            || you.attribute[ATTR_TRANSFORMATION] != TRAN_BAT))
    {
        mpr("Your transformation has been extended.");
        you.increase_duration(DUR_TRANSFORMATION, random2(pow), 100,
                              "Your transformation has been extended.");

        // Give a warning if it won't last long enough for the
        // timeout messages.
        transformation_expiration_warning();
    }

    //jmf: added following
    if (you.duration[DUR_STONESKIN])
        cast_stoneskin(pow);

    if (you.duration[DUR_PHASE_SHIFT])
        cast_phase_shift(pow);

    if (you.duration[DUR_SEE_INVISIBLE])
        cast_see_invisible(pow);

    if (you.duration[DUR_SILENCE])   //how precisely did you cast extension?
        cast_silence(pow);

    if (you.duration[DUR_CONDENSATION_SHIELD])
        cast_condensation_shield(pow);

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

void cast_ring_of_flames(int power)
{
    // You shouldn't be able to cast this in the rain. {due}
    if (in_what_cloud(CLOUD_RAIN))
    {
        mpr("Your spell sizzles in the rain.");
        return;
    }
    you.increase_duration(DUR_FIRE_SHIELD,
                          5 + (power / 10) + (random2(power) / 5), 50,
                          "The air around you leaps into flame!");
    manage_fire_shield(1);
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

void manage_fire_shield(int delay)
{
    ASSERT(you.duration[DUR_FIRE_SHIELD]);

    int old_dur = you.duration[DUR_FIRE_SHIELD];

    you.duration[DUR_FIRE_SHIELD]-= delay;
    if(you.duration[DUR_FIRE_SHIELD] < 0)
        you.duration[DUR_FIRE_SHIELD] = 0;

    if (!you.duration[DUR_FIRE_SHIELD])
    {
        mpr("Your ring of flames gutters out.", MSGCH_DURATION);
        return;
    }

    int threshold = get_expiration_threshold(DUR_FIRE_SHIELD);


    if (old_dur > threshold && you.duration[DUR_FIRE_SHIELD] < threshold)
        mpr("Your ring of flames is guttering out.", MSGCH_WARN);

    // Place fire clouds all around you
    for ( adjacent_iterator ai(you.pos()); ai; ++ai )
        if (!feat_is_solid(grd(*ai)) && env.cgrid(*ai) == EMPTY_CLOUD)
            place_cloud( CLOUD_FIRE, *ai, 1 + random2(6), KC_YOU );
}
