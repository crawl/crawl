/**
 * @file
 * @brief Monster-affecting enchantment spells.
 *           Other targeted enchantments are handled in spl-zap.cc.
**/

#include "AppHdr.h"

#include "spl-monench.h"

#include "actor.h"
#include "beam.h"
#include "coordit.h"
#include "directn.h"
#include "english.h" // apostrophise
#include "env.h"
#include "fight.h"
#include "losglobal.h"
#include "melee-attack.h"
#include "message.h"
#include "mon-tentacle.h"
#include "spl-util.h"
#include "stringutil.h" // make_stringf
#include "terrain.h"
#include "transform.h"
#include "rltiles/tiledef-main.h"
#include "view.h"
#include "viewchar.h"

int englaciate(coord_def where, int pow, actor *agent)
{
    actor *victim = actor_at(where);

    if (!victim || victim == agent)
        return 0;

    if (agent->is_monster() && mons_aligned(agent, victim))
        return 0; // don't let monsters hit friendlies

    monster* mons = victim->as_monster();

    // Skip some ineligable monster categories
    if (victim->is_peripheral() || never_harm_monster(agent, mons))
        return 0;

    if (victim->res_cold() > 0)
    {
        if (!mons)
            canned_msg(MSG_YOU_UNAFFECTED);
        else
            simple_monster_message(*mons, " is unaffected.");
        return 0;
    }

    int duration = div_rand_round(roll_dice(3, 1 + pow), 6)
                    - div_rand_round(victim->get_hit_dice() - 1, 2);

    if (duration <= 0)
    {
        if (!mons)
            canned_msg(MSG_YOU_RESIST);
        else
            simple_monster_message(*mons, " resists.");
        return 0;
    }

    if ((!mons && you.get_mutation_level(MUT_COLD_BLOODED))
        || (mons && mons_class_flag(mons->type, M_COLD_BLOOD)))
    {
        duration *= 2;
    }

    // Guarantee a minimum duration if not fully resisted.
    duration = max(duration, 2 + random2(4));

    if (!mons)
        return slow_player(duration);

    return do_slow_monster(*mons, agent, duration * BASELINE_DELAY);
}

spret cast_englaciation(int pow, bool fail)
{
    fail_check();
    mpr("You radiate an aura of cold.");
    apply_area_visible([pow] (coord_def where) {
        return englaciate(where, pow, &you);
    }, you.pos());
    return spret::success;
}

/** Corona a monster.
 *
 *  @param mons the monster to get a backlight.
 *  @param source The actor responsible for this.
 *  @returns true if it got backlit (even if it was already).
 */
bool backlight_monster(monster* mons, const actor* source)
{
    const mon_enchant bklt = mons->get_ench(ENCH_CORONA);
    const mon_enchant zin_bklt = mons->get_ench(ENCH_SILVER_CORONA);
    const int lvl = bklt.degree + zin_bklt.degree;

    mons->add_ench(mon_enchant(ENCH_CORONA, 1, source));

    if (lvl == 0)
        simple_monster_message(*mons, " is outlined in light.");
    else if (lvl == 4)
        simple_monster_message(*mons, " glows brighter for a moment.");
    else
        simple_monster_message(*mons, " glows brighter.");

    return true;
}

bool do_slow_monster(monster& mon, const actor* agent, int dur)
{
    if (mon.stasis())
        return true;

    if (mon.add_ench(mon_enchant(ENCH_SLOW, 0, agent, dur)))
    {
        if (!mon.paralysed() && !mon.petrified()
            && simple_monster_message(mon, " seems to slow down."))
        {
            return true;
        }
    }

    return false;
}

bool silence_monster(monster& mon, const actor* agent, int dur)
{
    if (mon.add_ench(mon_enchant(ENCH_MUTE, 0, agent, dur)))
    {
        simple_monster_message(mon, " loses the ability to speak.");
        return true;
    }

    return false;
}

bool enfeeble_monster(monster &mon, int pow)
{
    const int res_margin = mon.check_willpower(&you, pow);
    vector<enchant_type> hexes;

    if (mons_has_attacks(mon))
        hexes.push_back(ENCH_WEAK);
    if (mon.antimagic_susceptible())
        hexes.push_back(ENCH_DIMINISHED_SPELLS);
    if (res_margin <= 0)
    {
        hexes.push_back(ENCH_BLIND);
        hexes.push_back(ENCH_DAZED);
    }

    // Resisted the upgraded effects, and immune to the irresistible effects.
    if (hexes.empty())
    {
        return simple_monster_message(mon,
                   mon.resist_margin_phrase(res_margin).c_str());
    }

    const int max_extra_dur = div_rand_round(pow, 40);
    const int dur = 5 + random2avg(max_extra_dur, 3);

    for (auto hex : hexes)
    {
        if (hex == ENCH_DAZED)
            mon.daze(random_range(3, 5));
        else if (mon.has_ench(hex))
        {
            mon_enchant ench = mon.get_ench(hex);
            ench.duration = max(dur * BASELINE_DELAY, ench.duration);
            mon.update_ench(ench);
        }
        else
            mon.add_ench(mon_enchant(hex, 0, &you, dur * BASELINE_DELAY));
    }

    if (res_margin > 0)
        simple_monster_message(mon, " partially resists.");

    return simple_monster_message(mon, " is enfeebled!");
}

bool enfeeble_player(actor *source, int pow)
{
    const int res_margin = you.check_willpower(source, pow);
    vector<duration_type> hexes;

    hexes.push_back(DUR_WEAK);
    if (you.has_any_spells())
        hexes.push_back(DUR_DIMINISHED_SPELLS);
    if (res_margin <= 0)
    {
        hexes.push_back(DUR_BLIND);
        hexes.push_back(DUR_DAZED);
    }

    // Resisted the upgraded effects, and immune to the irresistible effects.
    if (hexes.empty())
    {
        canned_msg(MSG_YOU_UNAFFECTED);
        return false;
    }

    const int dur = random_range(7, 12);
    for (auto hex : hexes)
    {
        if (hex == DUR_DAZED)
            you.daze(random_range(2, 4));
        else if (hex == DUR_BLIND && you.duration[DUR_BLIND] < dur * BASELINE_DELAY)
        {
            you.duration[DUR_BLIND] = 0;
            blind_player(dur);
        }
        else
            you.duration[hex] = max(dur * BASELINE_DELAY, you.duration[hex]);
    }

    if (res_margin > 0)
        canned_msg(MSG_YOU_PARTIALLY_RESIST);

    mpr("You are enfeebled!");
    return true;
}

spret cast_vile_clutch(int pow, bolt &beam, bool fail)
{
    spret result = zapping(ZAP_VILE_CLUTCH, pow, beam, true, nullptr, fail);

    if (result == spret::success)
        you.props[VILE_CLUTCH_POWER_KEY].get_int() = pow;

    return result;
}

bool start_ranged_constriction(actor& caster, actor& target, int duration,
                               constrict_type type)
{
    if (!caster.can_constrict(target, type))
        return false;

    if (target.is_player())
    {
        if (type == CONSTRICT_ROOTS)
        {
            you.increase_duration(DUR_GRASPING_ROOTS, duration);
            mprf(MSGCH_WARN, "The grasping roots grab you!");
        }
        else if (type == CONSTRICT_BVC)
        {
            you.increase_duration(DUR_VILE_CLUTCH, duration);
            mprf(MSGCH_WARN, "Zombie hands grab you from below!");
        }
        caster.start_constricting(you);
    }
    else
    {
        enchant_type etype = (type == CONSTRICT_ROOTS ? ENCH_GRASPING_ROOTS
                                                      : ENCH_VILE_CLUTCH);
        auto ench = mon_enchant(etype, 0, &caster, duration * BASELINE_DELAY);
        target.as_monster()->add_ench(ench);
    }

    return true;
}

dice_def rimeblight_dot_damage(int pow, bool random)
{
    if (random)
        return dice_def(2, 4 + div_rand_round(pow, 17));
    else
        return dice_def(2, 4 + pow / 17);
}

string describe_rimeblight_damage(int pow, bool terse)
{
    dice_def dot_damage = rimeblight_dot_damage(pow, false);
    dice_def shards_damage = zap_damage(ZAP_RIMEBLIGHT_SHARDS, pow, false, false);

    if (terse)
    {
        return make_stringf("%dd%d/%dd%d", dot_damage.num, dot_damage.size,
                                           shards_damage.num, shards_damage.size);
    }

    return make_stringf("%dd%d (primary target), %dd%d (explosion)",
                        dot_damage.num, dot_damage.size,
                        shards_damage.num, shards_damage.size);
}

bool maybe_spread_rimeblight(monster& victim, int power)
{
    if (!victim.has_ench(ENCH_RIMEBLIGHT)
        && !victim.is_peripheral()
        && !never_harm_monster(&you, victim)
        && you.see_cell_no_trans(victim.pos()))
    {
        apply_rimeblight(victim, power);
        return true;
    }

    return false;
}

bool apply_rimeblight(monster& victim, int power, bool quiet)
{
    if (victim.has_ench(ENCH_RIMEBLIGHT))
        return false;

    int duration = (random_range(8, 12) + div_rand_round(power, 30))
                    * BASELINE_DELAY;
    victim.add_ench(mon_enchant(ENCH_RIMEBLIGHT, 0, &you, duration));
    victim.props[RIMEBLIGHT_POWER_KEY] = power;
    victim.props[RIMEBLIGHT_TICKS_KEY] = random_range(0, 2);

    if (!quiet)
        simple_monster_message(victim, " is afflicted with rimeblight.");

    return true;
}

void do_rimeblight_explosion(coord_def pos, int power, int size)
{
    bolt shards;
    zappy(ZAP_RIMEBLIGHT_SHARDS, power, false, shards);
    shards.ex_size = size;
    shards.source_id     = MID_PLAYER;
    shards.thrower       = KILL_YOU_MISSILE;
    shards.origin_spell  = SPELL_RIMEBLIGHT;
    shards.target        = pos;
    shards.source        = pos;
    shards.hit_verb      = "hits";
    shards.aimed_at_spot = true;
    shards.explode();
}

void tick_rimeblight(monster& victim)
{
    const int pow = victim.props[RIMEBLIGHT_POWER_KEY].get_int();
    int ticks = victim.props[RIMEBLIGHT_TICKS_KEY].get_int();

    // Determine chance to explode with ice (rises over time)
    // Never happens below 3, always happens at 4, random chance beyond that
    if ((ticks == 4 || ticks > 4 && x_chance_in_y(ticks, ticks + 16))
        && you.see_cell_no_trans(victim.pos()))
    {
        mprf("Shards of ice erupt from %s body!", apostrophise(victim.name(DESC_THE)).c_str());
        do_rimeblight_explosion(victim.pos(), pow, 1);
    }

    // Injury bond or some other effects may have killed us by now
    if (!victim.alive())
        return;

    // Apply direct AC-ignoring cold damage
    int dmg = rimeblight_dot_damage(pow).roll();
    dmg = resist_adjust_damage(&victim, BEAM_COLD, dmg);
    victim.hurt(&you, dmg, BEAM_COLD, KILLED_BY_FREEZING);

    // Increment how long rimeblight has been active
    if (victim.alive())
        victim.props[RIMEBLIGHT_TICKS_KEY] = (++ticks);
}

spret cast_sign_of_ruin(actor& caster, coord_def target, int duration, bool check_only)
{
    vector<actor*> targets;

    // Gather targets (returning early if we're just checking if there are any)
    for (radius_iterator ri(target, 2, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        actor* act = actor_at(*ri);
        if (!act)
            continue;

        if (!mons_aligned(&caster, act))
        {
            if (act->is_player() && !you.duration[DUR_SIGN_OF_RUIN]
                || act->is_monster() && !act->as_monster()->has_ench(ENCH_SIGN_OF_RUIN))
            {
                if (check_only)
                    return spret::success;
                else
                    targets.push_back(act);
            }
        }
    }

    // No targets were found
    if (check_only)
        return spret::abort;

    // Show animation
    draw_ring_animation(target, 2, DARKGRAY, RED);

    // Apply signs
    for (actor* act : targets)
    {
        if (act->is_player())
        {
            mprf(MSGCH_WARN, "The sign of ruin forms upon you!");
            you.duration[DUR_SIGN_OF_RUIN] = random_range(duration, duration * 3 / 2);
        }
        else
        {
            if (you.can_see(*act))
                mprf("The sign of ruin forms upon %s!", act->name(DESC_THE).c_str());

            act->as_monster()->add_ench(mon_enchant(ENCH_SIGN_OF_RUIN, 1, &caster,
                                                    random_range(duration, duration * 3 / 2)));
        }
    }

    return spret::success;
}

spret cast_percussive_tempering(const actor& caster, monster& target, int power,
                                bool fail)
{
    fail_check();

    if (you.can_see(target))
    {
        mprf("A magical hammer augments %s in a flurry of sparks and slag.",
             target.name(DESC_THE).c_str());
    }

    flash_tile(target.pos(), WHITE, 0, TILE_BOLT_PERCUSSIVE_TEMPERING);

    bolt shockwave;
    shockwave.set_agent(&caster);
    shockwave.attitude = caster.temp_attitude();
    shockwave.source = target.pos();
    shockwave.target = target.pos();
    shockwave.is_explosion = true;
    shockwave.ex_size = 1;
    shockwave.origin_spell = SPELL_PERCUSSIVE_TEMPERING;
    zappy(ZAP_PERCUSSIVE_TEMPERING, power, true, shockwave);
    shockwave.explode(true, true);

    target.heal(roll_dice(3, 10));
    target.add_ench(mon_enchant(ENCH_TEMPERED, 0, &caster, random_range(70, 100)));

    // Give a small bit of extra duration if we're about to time out, just to
    // avoid the sad feeling of buffing a monster who immediately vanishes.
    if (target.has_ench(ENCH_SUMMON_TIMER))
    {
        mon_enchant dur = target.get_ench(ENCH_SUMMON_TIMER);
        if (dur.duration < 50)
        {
            dur.duration += random_range(30, 50);
            target.update_ench(dur);
        }
    }

    return spret::success;
}

bool is_valid_tempering_target(const monster& mon, const actor& caster)
{
    if (!mon.was_created_by(caster) || mon.has_ench(ENCH_TEMPERED))
        return false;

    mon_enchant summ = mon.get_ench(ENCH_SUMMON);
    if (summ.degree > 0)
    {
        const spell_type spell = (spell_type)summ.degree;
        // XXX: Bomblets are marked as created by the player cast of Monarch
        //      Bomb in order to track all detontation targets, but should *not*
        //      count as valid targets for Percussive Tempering despite this.
        if (!!(get_spell_disciplines(spell) & spschool::forgecraft))
            return mon.type != MONS_BOMBLET && mon.type != MONS_BLAZEHEART_CORE;
    }

    return false;
}

// Perform a forcible attack at a weighted random space around this actor.
// Spaces without an actor are 1/7th as likely to be chosen as one with an
// actor (so if you are adjacent to a single monster, you have a 50% chance to
// attack them and a 50% chance to whiff).
//
// Actor friendliness doesn't matter - you are just as likely to attack allies
// as enemies, though gods won't penance you for this action since it's not your
// fault. The allies themselves may not be so generous!
void do_vexed_attack(actor& attacker, bool always_hit_ally)
{
    const bool has_attacks = attacker.is_player() ? true
                                : mons_has_attacks(*attacker.as_monster(), true);

    vector<coord_def> empty_space;
    vector<actor*> targs;

    for (adjacent_iterator ai(attacker.pos()); ai; ++ai)
    {
        if (actor* targ = actor_at(*ai))
        {
            if (!always_hit_ally || mons_aligned(&attacker, targ))
                targs.push_back(targ);
        }
        else if (!always_hit_ally)
            empty_space.push_back(*ai);
    }

    // If we've been told to attack an ally, but none are around, just hit the floor.
    if (always_hit_ally && targs.empty())
        empty_space.push_back(attacker.pos());

    // Decide whether to attack empty space or an actor
    const int total_weight = empty_space.size() + targs.size() * 7;
    if (x_chance_in_y(empty_space.size(), total_weight))
    {
        coord_def pos = empty_space[random2(empty_space.size())];
        string targ_desc = (attacker.airborne() && feat_has_solid_floor(env.grid(pos))
                            && coinflip()) ? "the ceiling"
                            : feature_description_at(pos, false, DESC_THE);
        if (you.can_see(attacker))
        {
            mprf("%s %s %s!",
                    attacker.name(DESC_THE).c_str(),
                    has_attacks ? attacker.is_monster() ? "attacks" : "attack"
                                : "glares at",
                    targ_desc.c_str());
        }

        if (attacker.is_monster())
            attacker.as_monster()->lose_energy(EUT_ATTACK);
    }
    else
    {
        ASSERT(!targs.empty());
        actor* victim = targs[random2(targs.size())];
        if (has_attacks)
        {
            melee_attack atk(&attacker, victim);
            // The player is deliberately allowed to attack their allies.
            atk.never_prompt = true;
            atk.launch_attack_set();
        }
        else
        {
            if (you.can_see(attacker))
            {
                mprf("%s glares at %s!",
                        attacker.name(DESC_THE).c_str(),
                        victim->name(DESC_THE).c_str());
            }
            attacker.as_monster()->lose_energy(EUT_ATTACK);
        }
    }
}

static int _gloom_chance_numerator(int hd)
{
    return 95 - hd * 4;
}

static int _gloom_chance_denom(int pow)
{
    return 150 - pow;
}

int gloom_success_chance(int power, int target_hd)
{
    const int numerator = _gloom_chance_numerator(target_hd);
    const int denom = _gloom_chance_denom(power);
    return max(100 * numerator / denom, 0);
}

static bool _gloom_affect_target(actor *victim, const actor *agent, int pow)
{
    if (victim->res_blind())
        return false;

    if (victim->is_monster())
    {
        auto mons = victim->as_monster();
        const int numerator = _gloom_chance_numerator(mons->get_hit_dice());
        if (x_chance_in_y(numerator, _gloom_chance_denom(pow)))
        {
            mons->add_ench(mon_enchant(ENCH_BLIND, 1, agent,
                        random_range(4, 8) * BASELINE_DELAY));
            return true;
        }
    }
    else
    {
        const int numerator = _gloom_chance_numerator(you.experience_level / 2);
        if (x_chance_in_y(numerator, _gloom_chance_denom(pow)))
        {
            blind_player(random_range(6, 12));
            return true;
        }
    }

    return false;
}

spret cast_gloom(const actor *caster, int pow, bool fail, bool tracer)
{
    int range = spell_range(SPELL_GLOOM, pow, caster->is_player());
    auto vulnerable = [caster](const actor *act) -> bool
    {
        // No fedhas checks needed, plants can't be blinded by this.
        if (act->res_blind())
            return false;

        // For monster casting, only affect enemies, to make it easier to use
        // (Hopefully this is not somehow abusable)
        return caster->is_player() || !mons_aligned(caster, act);
    };

    if (tracer)
    {
         // XX: LOS_NO_TRANS ?
        for (radius_iterator ri(caster->pos(), range, C_SQUARE, LOS_SOLID_SEE, true); ri; ++ri)
        {
            if (!in_bounds(*ri))
                continue;

            const actor* victim = actor_at(*ri);

            if (!victim || !caster->can_see(*victim) || !vulnerable(victim))
                continue;

            if (!mons_aligned(caster, victim))
            {
                // If this is a player tracer, consider blind monsters to be
                // valid targets (since you *could* stand more duration on them.)
                if (caster->is_player())
                    return spret::success;
                // But for monsters, it's generally a waste of time to reblind
                // things which are already blind, so don't bother.
                else
                {
                    if ((victim->is_monster() && !victim->as_monster()->has_ench(ENCH_BLIND))
                        || (victim->is_player() && !you.duration[DUR_BLIND]))
                    {
                        return spret::success;
                    }
                }
            }
        }

        return spret::abort;
    }

    if (caster->is_player())
    {
        auto hitfunc = find_spell_targeter(SPELL_GLOOM, pow, range);
        if (stop_attack_prompt(*hitfunc, "blind", vulnerable))
            return spret::abort;
    }

    fail_check();

    bolt beam;
    beam.name = "gloom";
    beam.flavour = BEAM_VISUAL;
    beam.origin_spell = SPELL_GLOOM;
    beam.set_agent(caster);
    beam.colour = DARKGRAY;
    beam.glyph = dchar_glyph(DCHAR_EXPLOSION);
    beam.range = range;
    beam.ex_size = range;
    beam.is_explosion = true;
    beam.source = caster->pos();
    beam.target = caster->pos();
    beam.hit = AUTOMATIC_HIT;
    beam.loudness = 0;
    beam.tile_explode = TILE_BOLT_GLOOM;
    beam.explode(true, true);

    for (radius_iterator ri(caster->pos(), range, C_SQUARE, LOS_SOLID_SEE, true);
         ri; ++ri)
    {
        actor* victim = actor_at(*ri);
        if (victim && vulnerable(victim) && _gloom_affect_target(victim, caster, pow))
        {
            if (victim->is_monster())
                simple_monster_message(*victim->as_monster(), " is enveloped in gloom.");
            // Player already got a message from blind_player
            // XX: Should it be a slightly different message? Show something if resisted?
        }
    }

    return spret::success;
}
