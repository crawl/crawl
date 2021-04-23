/**
 * @file
 * @brief Damage-dealing spells not already handled elsewhere.
 *           Other targeted spells are covered in spl-zap.cc.
**/

#include "AppHdr.h"


#include "spl-damage.h"

#include <functional>

#include "act-iter.h"
#include "areas.h"
#include "attack.h"
#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "fineff.h"
#include "fprop.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "invent.h"
#include "items.h"
#include "level-state-type.h"
#include "los.h"
#include "losglobal.h"
#include "macro.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "ouch.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-goditem.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "spl-zap.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "transform.h"
#include "unicode.h"
#include "viewchar.h"
#include "view.h"
#include "xp-evoker-data.h" // for thunderbolt

void setup_fire_storm(const actor *source, int pow, bolt &beam)
{
    zappy(ZAP_FIRE_STORM, pow, source->is_monster(), beam);
    beam.ex_size      = 2 + (random2(1000) < pow);
    beam.source_id    = source->mid;
    // XXX: Should this be KILL_MON_MISSILE?
    beam.thrower      =
        source->is_player() ? KILL_YOU_MISSILE : KILL_MON;
    beam.aux_source.clear();
    beam.is_tracer    = false;
    beam.origin_spell = SPELL_FIRE_STORM;
}

spret cast_fire_storm(int pow, bolt &beam, bool fail)
{
    if (grid_distance(beam.target, beam.source) > beam.range)
    {
        mpr("That is beyond the maximum range.");
        return spret::abort;
    }

    if (cell_is_solid(beam.target))
    {
        const char *feat = feat_type_name(env.grid(beam.target));
        mprf("You can't place the storm on %s.", article_a(feat).c_str());
        return spret::abort;
    }

    setup_fire_storm(&you, pow, beam);

    bolt tempbeam = beam;
    tempbeam.ex_size = (pow > 76) ? 3 : 2;
    tempbeam.is_tracer = true;

    tempbeam.explode(false);
    if (tempbeam.beam_cancelled)
        return spret::abort;

    fail_check();

    beam.apply_beam_conducts();
    beam.refine_for_explosion();
    beam.explode(false);

    viewwindow();
    update_screen();
    return spret::success;
}

// No setup/cast split here as monster damnation is completely different.
// XXX make this not true
bool cast_smitey_damnation(int pow, bolt &beam)
{
    beam.name              = "damnation";
    beam.aux_source        = "damnation";
    beam.ex_size           = 1;
    beam.flavour           = BEAM_DAMNATION;
    beam.real_flavour      = beam.flavour;
    beam.glyph             = dchar_glyph(DCHAR_FIRED_BURST);
    beam.colour            = LIGHTRED;
    beam.source_id         = MID_PLAYER;
    beam.thrower           = KILL_YOU;
    beam.obvious_effect    = false;
    beam.pierce            = false;
    beam.is_explosion      = true;
    beam.ench_power        = pow;      // used for radius
    beam.hit               = 20 + pow / 10;
    beam.damage            = calc_dice(6, 30 + pow);
    beam.attitude          = ATT_FRIENDLY;
    beam.friend_info.count = 0;
    beam.is_tracer         = true;

    beam.explode(false);

    if (beam.beam_cancelled)
    {
        canned_msg(MSG_OK);
        return false;
    }

    mpr("You call forth a pillar of damnation!");

    beam.is_tracer = false;
    beam.in_explosion_phase = false;
    beam.explode(true);

    return true;
}

string desc_chain_lightning_dam(int pow)
{
    // Damage is 5d(9.2 + pow / 30), but if lots of targets are around
    // it can hit the player precisely once at very low (e.g. 1) power
    // and deal 5 damage.
    int min = 5;

    // Max damage per bounce is 46 + pow / 6; in the worst case every other
    // bounce hits the player, losing 8 pow on the bounce away and 8 on the
    // bounce back for a total of 16; thus, for n bounces, it's:
    // (46 + pow/6) * n less 16/6 times the (n - 1)th triangular number.
    int n = (pow + 15) / 16;
    int max = (46 + (pow / 6)) * n - 4 * n * (n - 1) / 3;

    return make_stringf("%d-%d", min, max);
}

// XXX no friendly check
spret cast_chain_spell(spell_type spell_cast, int pow,
                            const actor *caster, bool fail)
{
    fail_check();
    bolt beam;

    // initialise beam structure
    switch (spell_cast)
    {
        case SPELL_CHAIN_LIGHTNING:
            beam.name           = "lightning arc";
            beam.aux_source     = "chain lightning";
            beam.glyph          = dchar_glyph(DCHAR_FIRED_ZAP);
            beam.flavour        = BEAM_ELECTRICITY;
            break;
        case SPELL_CHAIN_OF_CHAOS:
            beam.name           = "arc of chaos";
            beam.aux_source     = "chain of chaos";
            beam.glyph          = dchar_glyph(DCHAR_FIRED_ZAP);
            beam.flavour        = BEAM_CHAOS;
            break;
        default:
            die("buggy chain spell %d cast", spell_cast);
            break;
    }
    beam.source_id      = caster->mid;
    beam.thrower        = caster->is_player() ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
    beam.range          = 8;
    beam.hit            = AUTOMATIC_HIT;
    beam.obvious_effect = true;
    beam.pierce         = false;       // since we want to stop at our target
    beam.is_explosion   = false;
    beam.is_tracer      = false;
    beam.origin_spell   = spell_cast;

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
        int min_dist = LOS_DEFAULT_RANGE - 1;

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

            if (!cell_see_cell(source, mi->pos(), LOS_SOLID)
                || !cell_see_cell(caster->pos(), mi->pos(), LOS_SOLID_SEE))
            {
                continue;
            }

            // check for actors along the arc path
            ray_def ray;
            if (!find_ray(source, mi->pos(), ray, opc_solid))
                continue;

            while (ray.advance())
                if (actor_at(ray.pos()))
                    break;

            if (ray.pos() != mi->pos())
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
                mprf("The %s grounds out.", beam.name.c_str());

            break;
        }

        // Trying to limit message spamming here so we'll only mention
        // the thunder at the start or when it's out of LoS.
        switch (spell_cast)
        {
            case SPELL_CHAIN_LIGHTNING:
            {
                const char* msg = "You hear a mighty clap of thunder!";
                noisy(spell_effect_noise(SPELL_CHAIN_LIGHTNING), source,
                      (first || !see_source) ? msg : nullptr);
                break;
            }
            case SPELL_CHAIN_OF_CHAOS:
                if (first && see_source)
                    mpr("A swirling arc of seething chaos appears!");
                break;
            default:
                break;
        }
        first = false;

        if (see_source && !see_targ)
            mprf("The %s arcs out of your line of sight!", beam.name.c_str());
        else if (!see_source && see_targ)
            mprf("The %s suddenly appears!", beam.name.c_str());

        beam.source = source;
        beam.target = target;
        switch (spell_cast)
        {
            case SPELL_CHAIN_LIGHTNING:
                beam.colour = LIGHTBLUE;
                beam.damage = caster->is_player()
                    ? calc_dice(5, 10 + pow * 2 / 3)
                    : calc_dice(5, 46 + pow / 6);
                break;
            case SPELL_CHAIN_OF_CHAOS:
                beam.colour       = ETC_RANDOM;
                beam.ench_power   = pow;
                beam.damage       = calc_dice(3, 5 + pow / 6);
                beam.real_flavour = BEAM_CHAOS;
                beam.flavour      = BEAM_CHAOS;
            default:
                break;
        }

        // Be kinder to the caster.
        if (target == caster->pos())
        {
            // This should not hit the caster, too scary as a player effect and
            // too kind to the player as a monster effect.
            if (spell_cast == SPELL_CHAIN_OF_CHAOS)
            {
                beam.real_flavour = BEAM_VISUAL;
                beam.flavour      = BEAM_VISUAL;
            }

            // Reduce damage when the spell arcs to the caster.
            beam.damage.num = max(1, beam.damage.num / 2);
            beam.damage.size = max(3, beam.damage.size / 2);
        }
        beam.fire();
    }

    return spret::success;
}

/*
 * Handle the application of damage from a player spell that doesn't apply these
 * through struct bolt. This can apply god conducts and handles any necessary
 * death cleanup.
 * @param mon          The monster.
 * @param damage       The damage to apply, if any. Regardless of damage done,
 *                     the monster will have death cleanup applied via
 *                     monster_die() if it's now dead.
 * @param flavour      The beam flavour of damage.
 * @param god_conducts If true, apply any god conducts, in which case the
 *                     monster must be alive. Some callers need to apply
 *                     effects prior to damage that might kill the monster,
 *                     hence handle conducts on their own.
*/
static void _player_hurt_monster(monster &mon, int damage, beam_type flavour,
                                 bool god_conducts = true)
{
    ASSERT(mon.alive() || !god_conducts);

    if (god_conducts && you.deity() == GOD_FEDHAS && fedhas_neutralises(mon))
    {
        simple_god_message(" protects your plant from harm.", GOD_FEDHAS);
        return;
    }

    god_conduct_trigger conducts[3];
    if (god_conducts)
        set_attack_conducts(conducts, mon, you.can_see(mon));

    if (damage)
    {
        majin_bo_vampirism(mon, min(damage, mon.stat_hp()));
        mon.hurt(&you, damage, flavour, KILLED_BY_BEAM);
    }

    if (mon.alive())
    {
        behaviour_event(&mon, ME_WHACK, &you);

        if (damage && you.can_see(mon))
            print_wounds(mon);
    }
    // monster::hurt() wasn't called, so we do death cleanup.
    else if (!damage)
        monster_die(mon, KILL_YOU, NON_MONSTER);
}

static bool _drain_lifeable(const actor* agent, const actor* act)
{
    if (act->res_negative_energy() >= 3)
        return false;

    if (!agent)
        return true;

    const monster* mons = agent->as_monster();
    const monster* m = act->as_monster();

    return !(agent->is_player() && act->wont_attack()
             || mons && act->is_player() && mons->wont_attack()
             || mons && m && mons_atts_aligned(mons->attitude, m->attitude));
}

static void _los_spell_pre_damage_monsters(const actor* agent,
                                           vector<monster *> affected_monsters,
                                           const char *verb)
{
    // Filter out affected monsters that we don't know for sure are there
    vector<monster*> seen_monsters;
    for (monster *mon : affected_monsters)
        if (you.can_see(*mon))
            seen_monsters.push_back(mon);

    if (!seen_monsters.empty())
    {
        counted_monster_list mons_list =
            counted_monster_list(seen_monsters);
        const string message = make_stringf("%s %s %s.",
                mons_list.describe(DESC_THE).c_str(),
                conjugate_verb("be", mons_list.count() > 1).c_str(), verb);
        if (strwidth(message) < get_number_of_cols() - 2)
            mpr(message);
        else
        {
            // Exclamation mark to suggest that a lot of creatures were
            // affected.
            mprf("The monsters around %s are %s!",
                agent && agent->is_monster() && you.can_see(*agent)
                ? agent->as_monster()->name(DESC_THE).c_str()
                : "you", verb);
        }
    }
}

static int _los_spell_damage_player(const actor* agent, bolt &beam,
                                    bool actual)
{
    // No self damage from ozo's, but we do get -Potion
    if (actual && agent->is_player()
        && beam.origin_spell == SPELL_OZOCUBUS_REFRIGERATION)
    {
        mpr("You feel very cold.");
        you.increase_duration(DUR_NO_POTIONS, 7 + random2(9), 15);
        return 0;
    }

    int hurted = actual ? beam.damage.roll()
                        // Monsters use the average for foe calculations.
                        : (1 + beam.damage.num * beam.damage.size) / 2;
    hurted = check_your_resists(hurted, beam.flavour, beam.name, 0,
            // Drain life doesn't apply drain effects.
            actual && beam.origin_spell != SPELL_DRAIN_LIFE);
    if (actual && hurted > 0)
    {

        if (agent && !agent->is_player())
        {
            ouch(hurted, KILLED_BY_BEAM, agent->mid,
                 make_stringf("by %s", beam.name.c_str()).c_str(), true,
                 agent->as_monster()->name(DESC_A).c_str());
            you.expose_to_element(beam.flavour, 5);
        }
    }

    return hurted;
}

static int _los_spell_damage_monster(const actor* agent, monster &target,
                                     bolt &beam, bool actual)
{

    beam.thrower = (agent && agent->is_player()) ? KILL_YOU :
                    agent                        ? KILL_MON
                                                 : KILL_MISC;

    // Set conducts here. The monster needs to be alive when this is done, and
    // mons_adjust_flavoured() could kill it.
    god_conduct_trigger conducts[3];
    if (YOU_KILL(beam.thrower))
        set_attack_conducts(conducts, target, you.can_see(target));

    int hurted = actual ? beam.damage.roll()
                        // Monsters use the average for foe calculations.
                        : (1 + beam.damage.num * beam.damage.size) / 2;
    hurted = mons_adjust_flavoured(&target, beam, hurted,
                 // Drain life doesn't apply drain effects.
                 actual && beam.origin_spell != SPELL_DRAIN_LIFE);
    dprf("damage done: %d", hurted);

    if (actual)
    {
        if (YOU_KILL(beam.thrower))
            _player_hurt_monster(target, hurted, beam.flavour, false);
        else if (hurted)
            target.hurt(agent, hurted, beam.flavour);

        // Cold-blooded creatures can be slowed.
        if (beam.origin_spell == SPELL_OZOCUBUS_REFRIGERATION
            && target.alive())
        {
            target.expose_to_element(beam.flavour, 5);
        }
    }

    // So that summons don't restore HP.
    if (beam.origin_spell == SPELL_DRAIN_LIFE && target.is_summoned())
        return 0;

    return hurted;
}


static spret _cast_los_attack_spell(spell_type spell, int pow,
                                         const actor* agent, bool actual,
                                         bool fail, int* damage_done)
{
    const monster* mons = agent ? agent->as_monster() : nullptr;

    const zap_type zap = spell_to_zap(spell);
    if (zap == NUM_ZAPS)
        return spret::abort;

    bolt beam;
    zappy(zap, pow, mons, beam);
    beam.source_id = agent ? agent->mid : MID_NOBODY;
    beam.foe_ratio = 80;

    const char *player_msg = nullptr, *global_msg = nullptr,
               *mons_vis_msg = nullptr, *mons_invis_msg = nullptr,
               *verb = nullptr, *prompt_verb = nullptr;
    bool (*vulnerable)(const actor *, const actor *) = nullptr;

    switch (spell)
    {
        case SPELL_OZOCUBUS_REFRIGERATION:
            player_msg = "The heat is drained from your surroundings.";
            global_msg = "Something drains the heat from around you.";
            mons_vis_msg = " drains the heat from the surrounding"
                           " environment!";
            mons_invis_msg = "The ambient heat is drained!";
            verb = "frozen";
            prompt_verb = "refrigerate";
            vulnerable = [](const actor *caster, const actor *act) {
                return act->is_player() || act->res_cold() < 3
                       && !(caster->deity() == GOD_FEDHAS
                            && fedhas_protects(act->as_monster()));
            };
            break;

        case SPELL_DRAIN_LIFE:
            player_msg = "You draw life from your surroundings.";
            global_msg = "Something draws the life force from your"
                         " surroundings.";
            mons_vis_msg = " draws from the surrounding life force!";
            mons_invis_msg = "The surrounding life force dissipates!";
            verb = "drained of life";
            prompt_verb = "drain life";
            vulnerable = &_drain_lifeable;
            break;

        case SPELL_SONIC_WAVE:
            player_msg = "You send a blast of sound all around you.";
            global_msg = "Something sends a blast of sound all around you.";
            mons_vis_msg = " sends a blast of sound all around you!";
            mons_invis_msg = "Sound blasts the surrounding area!";
            verb = "blasted";
            // prompt_verb = "sing" The singing sword prompts in melee-attack
            vulnerable = [](const actor *caster, const actor *act) {
                return act != caster
                       && !(caster->deity() == GOD_FEDHAS
                            && fedhas_protects(act->as_monster()));
            };
            break;

        default:
            return spret::abort;
    }

    auto vul_hitfunc = [vulnerable](const actor *act) -> bool
    {
        return (*vulnerable)(&you, act);
    };

    if (agent && agent->is_player())
    {
        ASSERT(actual);

        targeter_radius hitfunc(&you, LOS_NO_TRANS);
        // Singing Sword's spell shouldn't give a prompt at this time.
        if (spell != SPELL_SONIC_WAVE)
        {
            if (stop_attack_prompt(hitfunc, prompt_verb, vul_hitfunc))
                return spret::abort;

            fail_check();
        }

        mpr(player_msg);
        flash_view_delay(UA_PLAYER, beam.colour, 300, &hitfunc);
    }
    else if (actual)
    {
        if (!agent)
            mpr(global_msg);
        else if (you.can_see(*agent))
            simple_monster_message(*mons, mons_vis_msg);
        else if (you.see_cell(agent->pos()))
            mpr(mons_invis_msg);

        if (!agent || you.see_cell(agent->pos()))
            flash_view_delay(UA_MONSTER, beam.colour, 300);
    }

    bool affects_you = false;
    vector<monster *> affected_monsters;

    for (actor_near_iterator ai((agent ? agent : &you)->pos(), LOS_NO_TRANS);
         ai; ++ai)
    {
        if ((*vulnerable)(agent, *ai))
        {
            if (ai->is_player())
                affects_you = true;
            else if (*ai != agent)
                affected_monsters.push_back(ai->as_monster());
        }
    }

    const int avg_damage = (1 + beam.damage.num * beam.damage.size) / 2;
    int total_damage = 0;
    // XXX: This ordering is kind of broken; it's to preserve the message
    // order from the original behaviour in the case of refrigerate.
    if (affects_you)
    {
        total_damage = _los_spell_damage_player(agent, beam, actual);
        if (!actual && mons)
        {
            if (mons->wont_attack())
            {
                beam.friend_info.count++;
                beam.friend_info.power +=
                    (you.get_experience_level() * total_damage / avg_damage);
            }
            else
            {
                beam.foe_info.count++;
                beam.foe_info.power +=
                    (you.get_experience_level() * total_damage / avg_damage);
            }
        }
    }

    if (actual && !affected_monsters.empty())
        _los_spell_pre_damage_monsters(agent, affected_monsters, verb);

    for (auto m : affected_monsters)
    {
        // Watch out for invalidation. Example: Ozocubu's refrigeration on
        // a bunch of ballistomycete spores that blow each other up.
        if (!m->alive())
            continue;

        int this_damage = _los_spell_damage_monster(agent, *m, beam, actual);
        total_damage += this_damage;

        if (!actual && mons)
        {
            if (mons_atts_aligned(m->attitude, mons->attitude))
            {
                beam.friend_info.count++;
                beam.friend_info.power +=
                    (m->get_hit_dice() * this_damage / avg_damage);
            }
            else
            {
                beam.foe_info.count++;
                beam.foe_info.power +=
                    (m->get_hit_dice() * this_damage / avg_damage);
            }
        }
    }

    if (damage_done)
        *damage_done = total_damage;

    if (actual)
        return spret::success;
    return mons_should_fire(beam) ? spret::success : spret::abort;
}

spret trace_los_attack_spell(spell_type spell, int pow, const actor* agent)
{
    return _cast_los_attack_spell(spell, pow, agent, false, false, nullptr);
}

spret fire_los_attack_spell(spell_type spell, int pow, const actor* agent,
                            bool fail, int* damage_done)
{
    return _cast_los_attack_spell(spell, pow, agent, true, fail, damage_done);
}

dice_def freeze_damage(int pow)
{
    return dice_def(1, 3 + pow / 3);
}

spret cast_freeze(int pow, monster* mons, bool fail)
{
    pow = min(25, pow);

    if (!mons || mons->submerged())
    {
        fail_check();
        canned_msg(MSG_NOTHING_CLOSE_ENOUGH);
        // If there's no monster there, you still pay the costs in
        // order to prevent locating invisible/submerged monsters.
        return spret::success;
    }

    if (stop_attack_prompt(mons, false, you.pos()))
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }

    fail_check();

    // Set conducts here. The monster needs to be alive when this is done, and
    // mons_adjust_flavoured() could kill it.
    god_conduct_trigger conducts[3];
    set_attack_conducts(conducts, *mons);

    bolt beam;
    beam.flavour = BEAM_COLD;
    beam.thrower = KILL_YOU;

    const int orig_hurted = freeze_damage(pow).roll();
    // calculate the resist adjustment to punctuate
    int hurted = mons_adjust_flavoured(mons, beam, orig_hurted, false);
    mprf("You freeze %s%s%s",
         mons->name(DESC_THE).c_str(),
         hurted ? "" : " but do no damage",
         attack_strength_punctuation(hurted).c_str());

    // print the resist message and expose to the cold
    mons_adjust_flavoured(mons, beam, orig_hurted);

    _player_hurt_monster(*mons, hurted, beam.flavour, false);

    if (mons->alive())
        mons->expose_to_element(BEAM_COLD, orig_hurted);

    return spret::success;
}

spret cast_airstrike(int pow, const dist &beam, bool fail)
{
    if (cell_is_solid(beam.target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    monster* mons = monster_at(beam.target);
    if (!mons || mons->submerged())
    {
        fail_check();
        canned_msg(MSG_SPELL_FIZZLES);
        return spret::success; // still losing a turn
    }

    if (!(have_passive(passive_t::shoot_through_plants)
          && fedhas_protects(mons))
        && stop_attack_prompt(mons, false, you.pos()))
    {
        return spret::abort;
    }

    fail_check();

    noisy(spell_effect_noise(SPELL_AIRSTRIKE), beam.target);

    bolt pbeam;
    pbeam.flavour = BEAM_AIR;

    int empty_space = 0;
    for (adjacent_iterator ai(beam.target); ai; ++ai)
        if (!monster_at(*ai) && !cell_is_solid(*ai))
            empty_space++;

    empty_space = max(3, empty_space);

    int hurted = 5 + empty_space + random2avg(2 + div_rand_round(pow, 7), 2);
#ifdef DEBUG_DIAGNOSTICS
    const int preac = hurted;
#endif
    hurted = mons->apply_ac(mons->beam_resists(pbeam, hurted, false));
    dprf("preac: %d, postac: %d", preac, hurted);

    mprf("The air twists around and strikes %s%s%s",
         mons->name(DESC_THE).c_str(),
         hurted ? "" : " but does no damage",
         attack_strength_punctuation(hurted).c_str());
    _player_hurt_monster(*mons, hurted, pbeam.flavour);

    return spret::success;
}

dice_def base_fragmentation_damage(int pow)
{
    return dice_def(3, 5 + pow / 5);
}

enum class frag_damage_type
{
    rock, // default
    metal, // extra damage
    crystal, // extra damage & radius
    ice, // BEAM_ICE, not BEAM_FRAG
    player_gargoyle, // weaker, because (?)
};

struct frag_effect
{
    frag_damage_type damage;
    colour_t colour;
    string name;
    const char* terrain_name;
    bool direct;
    bool hit_centre;
};

// Initializes the provided frag_effect with the appropriate Lee's Rapid
// Deconstruction explosion for blowing up the player. Returns true iff the
// player can be deconstructed.
static bool _init_frag_player(frag_effect &effect)
{
    if (you.form == transformation::statue || you.species == SP_GARGOYLE)
    {
        effect.name       = "blast of rock fragments";
        effect.colour     = BROWN;
        if (you.form != transformation::statue)
            effect.damage = frag_damage_type::player_gargoyle;
        return true;
    }
    if (you.petrified() || you.petrifying())
    {
        effect.name       = "blast of petrified fragments";
        effect.colour     = mons_class_colour(player_mons(true));
        return true;
    }
    if (you.form == transformation::ice_beast)
    {
        effect.name       = "icy blast";
        effect.colour     = WHITE;
        effect.damage     = frag_damage_type::ice;
        return true;
    }
    return false;
}

struct monster_frag
{
    const char* type;
    colour_t colour;
    frag_damage_type damage;
};

static const map<monster_type, monster_frag> fraggable_monsters = {
    { MONS_TOENAIL_GOLEM,     { "toenail", RED } },
    // I made saltlings not have a big crystal explosion for balance reasons -
    // there are so many of them, it seems wrong to have them be so harmful to
    // their own allies. This could be wrong!
    { MONS_SALTLING,          { "salt crystal", WHITE } },
    { MONS_EARTH_ELEMENTAL,   { "rock", BROWN } },
    { MONS_ROCKSLIME,         { "rock", BROWN } },
    { MONS_USHABTI,           { "rock", BROWN } },
    { MONS_STATUE,            { "rock", BROWN } },
    { MONS_GARGOYLE,          { "rock", BROWN } },
    { MONS_IRON_ELEMENTAL,    { "metal", CYAN, frag_damage_type::metal } },
    { MONS_IRON_GOLEM,        { "metal", CYAN, frag_damage_type::metal } },
    { MONS_PEACEKEEPER,       { "metal", CYAN, frag_damage_type::metal } },
    { MONS_WAR_GARGOYLE,      { "metal", CYAN, frag_damage_type::metal } },
    { MONS_CRYSTAL_GUARDIAN,  { "crystal", DARKGREY,
                                frag_damage_type::crystal } },
    { MONS_ORANGE_STATUE,     { "orange crystal", LIGHTRED,
                                frag_damage_type::crystal } },
    { MONS_OBSIDIAN_STATUE,   { "obsidian", GREEN,
                                frag_damage_type::crystal } },
    { MONS_ROXANNE,           { "sapphire", BLUE, frag_damage_type::crystal } },
};

// Initializes the provided frag_effect with the appropriate Lee's Rapid
// Deconstruction explosion for blowing up the given monster. Return true iff
// that monster can be deconstructed.
static bool _init_frag_monster(frag_effect &effect, const monster &mon)
{
    auto frag_f = fraggable_monsters.find(mon.type);
    if (frag_f != fraggable_monsters.end())
    {
        const monster_frag &frag = frag_f->second;
        effect.damage = frag.damage;
        const bool crystal = frag.damage == frag_damage_type::crystal;
        effect.name = make_stringf("blast of %s %s", frag.type,
                                   crystal ? "shards" : "fragments");
        effect.colour = frag.colour;
        return true;
    }

    // Petrifying or petrified monsters can be exploded.
    if (mon.petrified() || mon.petrifying())
    {
        monster_info minfo(&mon);
        effect.name       = "blast of petrified fragments";
        effect.colour     = minfo.colour();
        return true;
    }
    if (mon.is_icy()) // blast of ice
    {
        effect.name       = "icy blast";
        effect.colour     = WHITE;
        effect.damage     = frag_damage_type::ice;
        return true;
    }
    if (mon.is_skeletal()) // blast of bone
    {
        effect.name   = "blast of bone shards";
        effect.colour = LIGHTGREY;
        return true;
    }
    // Targeted monster not shatterable.
    return false;
}

struct feature_frag
{
    const char* type;
    const char* what;
    frag_damage_type damage;
};

static const map<dungeon_feature_type, feature_frag> fraggable_terrain = {
    // Stone and rock terrain
    { DNGN_ROCK_WALL, { "rock", "wall" } },
    { DNGN_SLIMY_WALL, { "rock", "wall" } },
    { DNGN_STONE_WALL, { "rock", "wall" } },
    { DNGN_CLEAR_ROCK_WALL, { "rock", "wall" } },
    { DNGN_CLEAR_STONE_WALL, { "rock", "wall" } },
    { DNGN_ORCISH_IDOL, { "rock", "stone idol" } },
    { DNGN_GRANITE_STATUE, { "rock", "statue" } },
    { DNGN_PETRIFIED_TREE, { "rock", "petrified wood" } },
    // Stone arches and doors
    { DNGN_OPEN_DOOR, { "rock", "stone door frame" } },
    { DNGN_OPEN_CLEAR_DOOR, { "rock", "stone door frame" } },
    { DNGN_CLOSED_DOOR, { "rock", "stone door frame" } },
    { DNGN_CLOSED_CLEAR_DOOR, { "rock", "stone door frame" } },
    { DNGN_RUNED_DOOR, { "rock", "stone door frame" } },
    { DNGN_RUNED_CLEAR_DOOR, { "rock", "stone door frame" } },
    { DNGN_SEALED_DOOR, { "rock", "stone door frame" } },
    { DNGN_SEALED_CLEAR_DOOR, { "rock", "stone door frame" } },
    { DNGN_STONE_ARCH, { "rock", "stone arch" } },
    // Metal -- small but nasty explosion
    { DNGN_METAL_WALL, { "metal", "metal wall", frag_damage_type::metal } },
    { DNGN_GRATE, { "metal", "iron grate", frag_damage_type::metal } },
    // Crystal -- large & nasty explosion
    { DNGN_CRYSTAL_WALL, { "crystal", "crystal wall",
                           frag_damage_type::crystal } },
};

// Initializes the provided frag_effect with the appropriate Lee's Rapid
// Deconstruction explosion for the given target. Return true iff the target
// can be deconstructed.
static bool _init_frag_grid(frag_effect &effect,
                            coord_def target, const char **what)
{
    const dungeon_feature_type grid = env.grid(target);

    auto frag_f = fraggable_terrain.find(grid);
    if (frag_f == fraggable_terrain.end())
        return false;
    const feature_frag &frag = frag_f->second;

    effect.damage = frag.damage;
    const bool crystal = frag.damage == frag_damage_type::crystal;
    effect.name = make_stringf("blast of %s %s", frag.type,
                               crystal ? "shards" : "fragments");
    if (what)
        *what = frag.what;

    if (!feat_is_solid(grid))
        effect.hit_centre = true; // to hit monsters standing on doors

   // If it was recoloured, use that colour instead.
   if (env.grid_colours(target))
       effect.colour = env.grid_colours(target);
   else
   {
       effect.colour = element_colour(get_feature_def(grid).colour(),
                                     false, target);
   }
    return true;
}

// Initializes the provided frag_effect with the appropriate Lee's Rapid
// Deconstruction explosion for the given target.
static bool _init_frag_effect(frag_effect &effect, const actor &caster,
                              coord_def target, const char **what)
{
    if (target == you.pos() && _init_frag_player(effect))
    {
        effect.direct = true;
        return true;
    }

    const actor* victim = actor_at(target);
    if (victim && victim->alive() && victim->is_monster()
        && caster.can_see(*victim)
        && _init_frag_monster(effect, *victim->as_monster()))
    {
        return true;
    }

    return _init_frag_grid(effect, target, what);
}

bool setup_fragmentation_beam(bolt &beam, int pow, const actor *caster,
                              const coord_def target, bool quiet,
                              const char **what, bool &hole)
{
    beam.glyph       = dchar_glyph(DCHAR_FIRED_BURST);
    beam.source_id   = caster->mid;
    beam.thrower     = caster->is_player() ? KILL_YOU : KILL_MON;
    beam.source      = you.pos();
    beam.hit         = AUTOMATIC_HIT;

    frag_effect effect = {};
    if (!_init_frag_effect(effect, *caster, target, what))
    {
        // Couldn't find a monster or wall to shatter - abort casting!
        if (caster->is_player() && !quiet)
            mpr("You can't deconstruct that!");
        return false;
    }

    beam.colour = effect.colour;
    beam.name = effect.name;
    if (effect.direct) // cast on the player, not next to them
        beam.aux_source = "by Lee's Rapid Deconstruction";
    else
        beam.aux_source = effect.name;

    beam.damage  = base_fragmentation_damage(pow);
    beam.flavour = BEAM_FRAG;
    beam.ex_size = 1;
    switch (effect.damage)
    {
        case frag_damage_type::rock:
        default:
            break;
        case frag_damage_type::metal:
            beam.damage.num++;
            break;
        case frag_damage_type::crystal:
            beam.damage.num++;
            beam.ex_size++;
            break;
        case frag_damage_type::ice:
            beam.flavour = BEAM_ICE;
            break;
        case frag_damage_type::player_gargoyle:
            beam.damage.num--;
            break;
    }

    if (effect.hit_centre)
        hole = false;

    beam.source_name = caster->name(DESC_PLAIN, true);
    beam.target = target;

    return true;
}

spret cast_fragmentation(int pow, const actor *caster,
                              const coord_def target, bool fail)
{
    bool hole                = true;
    const char *what         = nullptr;

    bolt beam;

    if (!setup_fragmentation_beam(beam, pow, caster, target, false, &what,
                                  hole))
    {
        return spret::abort;
    }

    if (caster->is_player())
    {
        bolt tempbeam;
        bool temp;
        setup_fragmentation_beam(tempbeam, pow, caster, target, true, nullptr,
                                 temp);
        tempbeam.is_tracer = true;
        tempbeam.explode(false);
        if (tempbeam.beam_cancelled)
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
    }

    fail_check();

    if (what != nullptr) // Terrain explodes.
    {
        if (you.see_cell(target))
            mprf("The %s shatters!", what);
    }
    else if (target == you.pos()) // You explode.
    {
        const int dam = beam.damage.roll();
        mprf("You shatter%s", attack_strength_punctuation(dam).c_str());

        ouch(dam, KILLED_BY_BEAM, caster->mid,
             "by Lee's Rapid Deconstruction", true,
             caster->is_player() ? "you"
                                 : caster->name(DESC_A).c_str());
    }
    else // Monster explodes.
    {
        // Checks by setup_fragmentation_beam() must guarantee that we have an
        // alive monster.
        monster* mon = monster_at(target);
        ASSERT(mon);

        const int dam = beam.damage.roll();
        if (you.see_cell(target))
        {
            mprf("%s shatters%s", mon->name(DESC_THE).c_str(),
                 attack_strength_punctuation(dam).c_str());
        }

        if (caster->is_player())
            _player_hurt_monster(*mon, dam, BEAM_MINDBURST);
        else if (dam)
            mon->hurt(caster, dam, BEAM_MINDBURST);
    }

    beam.explode(true, hole);

    return spret::success;
}

static int _shatter_mon_dice(const monster *mon)
{
    const int DEFAULT_DICE = 3;
    if (!mon)
        return DEFAULT_DICE;

    // Double damage to stone, metal and crystal - the same as the list of
    // monsters affected by LRD.
    if (map_find(fraggable_monsters, mon->type))
        return DEFAULT_DICE * 2;
    if (mon->is_insubstantial())
        return 1;
    if (mon->petrifying() || mon->petrified()
        || mon->is_skeletal() || mon->is_icy())
    {
        return DEFAULT_DICE * 2;
    }
    else if (mon->airborne() || mons_is_slime(*mon))
        return 1;
    // Normal damage to everything else.
    else
        return DEFAULT_DICE;
}

dice_def shatter_damage(int pow, monster *mon)
{
    return dice_def(_shatter_mon_dice(mon), 5 + pow / 3);
}

static int _shatter_monsters(coord_def where, int pow, actor *agent)
{
    monster* mon = monster_at(where);

    if (!mon || !mon->alive() || mon == agent)
        return 0;

    const dice_def dam_dice = shatter_damage(pow, mon);
    int damage = max(0, dam_dice.roll() - random2(mon->armour_class()));

    if (agent->is_player())
        _player_hurt_monster(*mon, damage, BEAM_MMISSILE);
    else if (damage)
        mon->hurt(agent, damage);

    return damage;
}

static int _shatter_walls(coord_def where, int /*pow*/, actor *agent)
{
    int chance = 0;

    // if not in-bounds then we can't really shatter it -- bwr
    if (!in_bounds(where))
        return 0;

    if (env.markers.property_at(where, MAT_ANY, "veto_destroy") == "veto")
        return 0;

    const dungeon_feature_type grid = env.grid(where);

    switch (grid)
    {
    case DNGN_CLOSED_DOOR:
    case DNGN_CLOSED_CLEAR_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_RUNED_CLEAR_DOOR:
    case DNGN_OPEN_DOOR:
    case DNGN_OPEN_CLEAR_DOOR:
    case DNGN_SEALED_DOOR:
    case DNGN_SEALED_CLEAR_DOOR:
        if (you.see_cell(where))
            mpr("A door shatters!");
        chance = 100;
        break;

    case DNGN_GRATE:
        if (you.see_cell(where))
            mpr("An iron grate is ripped into pieces!");
        chance = 100;
        break;

    case DNGN_ORCISH_IDOL:
    case DNGN_GRANITE_STATUE:
        chance = 100;
        break;

    case DNGN_METAL_WALL:
        chance = 15;
        break;

    case DNGN_CLEAR_STONE_WALL:
    case DNGN_STONE_WALL:
        chance = 25;
        break;

    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_CRYSTAL_WALL:
    case DNGN_TREE:
    case DNGN_MANGROVE:
    case DNGN_DEMONIC_TREE:
    case DNGN_PETRIFIED_TREE:
        chance = 33;
        break;

    default:
        break;
    }

    if (agent->deity() == GOD_FEDHAS && feat_is_tree(grid))
        return 0;

    if (x_chance_in_y(chance, 100))
    {
        noisy(spell_effect_noise(SPELL_SHATTER), where);

        destroy_wall(where);

        return 1;
    }

    return 0;
}

static int _shatter_player_dice()
{
    if (you.is_insubstantial())
        return 1;
    if (you.petrified() || you.petrifying())
        return 6; // reduced later by petrification's damage reduction
    else if (you.form == transformation::statue
             || you.form == transformation::ice_beast
             || you.species == SP_GARGOYLE)
        return 6;
    else if (you.airborne())
        return 1;
    else
        return 3;
}

/**
 * Is this a valid target for shatter?
 *
 * @param act     The actor being considered
 * @return        Whether the actor will take damage from shatter.
 */
static bool _shatterable(const actor *act)
{
    if (act->is_player())
        return _shatter_player_dice();
    return _shatter_mon_dice(act->as_monster());
}

spret cast_shatter(int pow, bool fail)
{
    targeter_radius hitfunc(&you, LOS_ARENA);
    auto vulnerable = [](const actor *act) -> bool
    {
        return !act->is_player()
               && !(have_passive(passive_t::shoot_through_plants)
                    && fedhas_protects(act->as_monster()))
               && _shatterable(act);
    };
    if (stop_attack_prompt(hitfunc, "attack", vulnerable))
        return spret::abort;

    fail_check();
    const bool silence = silenced(you.pos());

    if (silence)
        mpr("The dungeon shakes!");
    else
    {
        noisy(spell_effect_noise(SPELL_SHATTER), you.pos());
        mprf(MSGCH_SOUND, "The dungeon rumbles!");
    }

    run_animation(ANIMATION_SHAKE_VIEWPORT, UA_PLAYER);

    int dest = 0;
    for (distance_iterator di(you.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        // goes from the center out, so newly dug walls recurse
        if (!cell_see_cell(you.pos(), *di, LOS_SOLID))
            continue;

        _shatter_monsters(*di, pow, &you);
        dest += _shatter_walls(*di, pow, &you);
    }

    if (dest && !silence)
        mprf(MSGCH_SOUND, "Ka-crash!");

    return spret::success;
}

static int _shatter_player(int pow, actor *wielder, bool devastator = false)
{
    if (wielder->is_player())
        return 0;

    dice_def dam_dice(_shatter_player_dice(), 5 + pow / 3);

    int damage = max(0, dam_dice.roll() - random2(you.armour_class()));

    if (damage > 0)
    {
        mprf(damage > 15 ? "You shudder from the earth-shattering force%s"
                        : "You shudder%s",
             attack_strength_punctuation(damage).c_str());
        if (devastator)
            ouch(damage, KILLED_BY_MONSTER, wielder->mid);
        else
            ouch(damage, KILLED_BY_BEAM, wielder->mid, "by Shatter");
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
        {
            mprf("The dungeon shakes around %s!",
                 caster->name(DESC_THE).c_str());
        }
        else
        {
            noisy(spell_effect_noise(SPELL_SHATTER), caster->pos(), caster->mid);
            mprf(MSGCH_SOUND, "The dungeon rumbles around %s!",
                 caster->name(DESC_THE).c_str());
        }
    }

    int pow = 5 + div_rand_round(caster->get_hit_dice() * 9, 2);

    int dest = 0;
    for (distance_iterator di(caster->pos(), true, true, LOS_RADIUS); di; ++di)
    {
        // goes from the center out, so newly dug walls recurse
        if (!cell_see_cell(caster->pos(), *di, LOS_SOLID))
            continue;

        if (actual)
        {
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
        mprf(MSGCH_SOUND, "Ka-crash!");

    if (actual)
        run_animation(ANIMATION_SHAKE_VIEWPORT, UA_MONSTER);

    if (!caster->wont_attack())
        foes *= -1;

    if (!actual)
        dprf("Shatter foe HD: %d", foes);

    return foes > 0; // doesn't matter if actual
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
            || mon->is_insubstantial() || !you.can_see(*mon)
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
            mpr(message);
        else
            mpr("There is a shattering impact!");
    }

    // need to do this again to do the actual damage
    for (adjacent_iterator ai(where, false); ai; ++ai)
        _shatter_monsters(*ai, pow * 3 / 2, wielder);

    if ((you.pos() - wielder->pos()).rdist() <= 1 && in_bounds(you.pos()))
        _shatter_player(pow, wielder, true);
}

dice_def irradiate_damage(int pow, bool random)
{
    const int dice = 3;
    const int max_dam = 40 + (random ? div_rand_round(pow, 2) : pow / 2);
    return calc_dice(dice, max_dam);
}

/**
 * Irradiate the given cell. (Per the spell.)
 *
 * @param where     The cell in question.
 * @param pow       The power with which the spell is being cast.
 * @param agent     The agent (player or monster) doing the irradiating.
 */
static int _irradiate_cell(coord_def where, int pow, actor *agent)
{
    monster *mons = monster_at(where);
    if (!mons || !mons->alive())
        return 0; // XXX: handle damaging the player for mons casts...?

    const dice_def dam_dice = irradiate_damage(pow);
    const int base_dam = dam_dice.roll();
    const int dam = mons->apply_ac(base_dam);
    mprf("%s is blasted with magical radiation%s",
         mons->name(DESC_THE).c_str(),
         attack_strength_punctuation(dam).c_str());

    if (agent->deity() == GOD_FEDHAS && fedhas_protects(mons))
    {
        simple_god_message(
                    make_stringf(" protects %s plant from harm.",
                        agent->is_player() ? "your" : "a").c_str(),
                    GOD_FEDHAS);
        return 0;
    }

    if (agent->is_player())
        _player_hurt_monster(*mons, dam, BEAM_MMISSILE);
    else if (dam)
        mons->hurt(agent, dam, BEAM_MMISSILE);

    if (mons->alive())
        mons->malmutate("");

    return dam;
}

/**
 * Attempt to cast the spell "Irradiate", damaging & deforming enemies around
 * the player.
 *
 * @param pow   The power at which the spell is being cast.
 * @param who   The actor doing the irradiating.
 * @param fail  Whether the player has failed to cast the spell.
 * @return      spret::abort if the player changed their mind about casting after
 *              realizing they would hit an ally; spret::fail if they failed the
 *              cast chance; spret::success otherwise.
 */
spret cast_irradiate(int powc, actor* who, bool fail)
{
    targeter_radius hitfunc(who, LOS_NO_TRANS, 1, 0, 1);
    auto vulnerable = [who](const actor *act) -> bool
    {
        return !act->is_player()
               && !(who->deity() == GOD_FEDHAS
                    && fedhas_protects(act->as_monster()));
    };

    if (stop_attack_prompt(hitfunc, "irradiate", vulnerable))
        return spret::abort;

    fail_check();

    ASSERT(who);
    if (who->is_player())
        mpr("You erupt in a fountain of uncontrolled magic!");
    else
    {
        simple_monster_message(*who->as_monster(),
                               " erupts in a fountain of uncontrolled magic!");
    }

    bolt beam;
    beam.name = "irradiate";
    beam.flavour = BEAM_VISUAL;
    beam.set_agent(&you);
    beam.colour = ETC_MUTAGENIC;
    beam.glyph = dchar_glyph(DCHAR_EXPLOSION);
    beam.range = 1;
    beam.ex_size = 1;
    beam.is_explosion = true;
    beam.explode_delay = beam.explode_delay * 3 / 2;
    beam.source = you.pos();
    beam.target = you.pos();
    beam.hit = AUTOMATIC_HIT;
    beam.explode(true, true);

    apply_random_around_square([powc, who] (coord_def where) {
        return _irradiate_cell(where, powc, who);
    }, who->pos(), true, 8);

    if (who->is_player())
        contaminate_player(1000 + random2(500));
    return spret::success;
}

// How much work can we consider we'll have done by igniting a cloud here?
// Considers a cloud under a susceptible ally bad, a cloud under a a susceptible
// enemy good, and other clouds relatively unimportant.
static int _ignite_tracer_cloud_value(coord_def where, actor *agent)
{
    actor* act = actor_at(where);
    if (act)
    {
        const int dam = actor_cloud_immune(*act, CLOUD_FIRE)
                        ? 0
                        : resist_adjust_damage(act, BEAM_FIRE, 40);

        if (agent->deity() == GOD_FEDHAS && agent->is_player()
            && fedhas_protects(act->as_monster()))
        {
            return 0;
        }

        return mons_aligned(act, agent) ? -dam : dam;
    }
    // We've done something, but its value is indeterminate
    else
        return 1;
}

/**
 * Place flame clouds over toxic bogs, by the power of Ignite Poison.
 *
 * @param where     The tile in question.
 * @param pow       The power with which Ignite Poison is being cast.
 *                  If -1, this indicates the spell is a test-run 'tracer'.
 * @param agent     The caster of Ignite Poison.
 * @return          If we're just running a tracer, return the expected 'value'
 *                  of creating fire clouds in the given location (could be
 *                  negative if there are allies there).
 *                  If it's not a tracer, return 1 if a flame cloud is created
 *                  and 0 otherwise.
 */
static int _ignite_poison_bog(coord_def where, int pow, actor *agent)
{
    const bool tracer = (pow == -1);  // Only testing damage, not dealing it

    if (env.grid(where) != DNGN_TOXIC_BOG)
        return false;

    if (tracer)
    {
        const int value = _ignite_tracer_cloud_value(where, agent);
        // Player doesn't care about magnitude.
        return agent && agent->is_player() ? sgn(value) : value;
    }

    // Tone down bog clouds
    if (!one_chance_in(4))
        return false;

    place_cloud(CLOUD_FIRE, where, 2 + random2(pow / 30), agent);

    return true;
}

/**
 * Turn poisonous clouds in the given tile into flame clouds, by the power of
 * Ignite Poison.
 *
 * @param where     The tile in question.
 * @param pow       The power with which Ignite Poison is being cast.
 *                  If -1, this indicates the spell is a test-run 'tracer'.
 * @param agent     The caster of Ignite Poison.
 * @return          If we're just running a tracer, return the expected 'value'
 *                  of creating fire clouds in the given location (could be
 *                  negative if there are allies there).
 *                  If it's not a tracer, return 1 if a flame cloud is created
 *                  and 0 otherwise.
 */
static int _ignite_poison_clouds(coord_def where, int pow, actor *agent)
{
    const bool tracer = (pow == -1);  // Only testing damage, not dealing it

    cloud_struct* cloud = cloud_at(where);
    if (!cloud)
        return false;

    if (cloud->type != CLOUD_MEPHITIC && cloud->type != CLOUD_POISON)
        return false;

    if (tracer)
    {
        const int value = _ignite_tracer_cloud_value(where, agent);
        // Player doesn't care about magnitude.
        return agent && agent->is_player() ? sgn(value) : value;
    }

    cloud->type = CLOUD_FIRE;
    cloud->decay = 30 + random2(20 + pow); // from 3-5 turns to 3-15 turns
    cloud->whose = agent->kill_alignment();
    cloud->killer = agent->is_player() ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
    cloud->source = agent->mid;
    return true;
}

/**
 * Burn poisoned monsters in the given tile, removing their poison state &
 * damaging them.
 *
 * @param where     The tile in question.
 * @param pow       The power with which Ignite Poison is being cast.
 *                  If -1, this indicates the spell is a test-run 'tracer'.
 * @param agent     The caster of Ignite Poison.
 * @return          If we're just running a tracer, return the expected damage
 *                  of burning the monster in the given location (could be
 *                  negative if there are allies there).
 *                  If it's not a tracer, return 1 if damage is caused & 0
 *                  otherwise.
 */
static int _ignite_poison_monsters(coord_def where, int pow, actor *agent)
{
    bolt beam;
    beam.flavour = BEAM_FIRE;   // This is dumb, only used for adjust!

    const bool tracer = (pow == -1);  // Only testing damage, not dealing it
    if (tracer)                       // Give some fake damage to test resists
        pow = 100;

    // If a monster casts Ignite Poison, it can't hit itself.
    // This doesn't apply to the other functions: it can ignite
    // clouds where it's standing!

    monster* mon = monster_at(where);
    if (invalid_monster(mon) || mon == agent)
        return 0;

    // how poisoned is the victim?
    const mon_enchant ench = mon->get_ench(ENCH_POISON);
    const int pois_str = ench.ench == ENCH_NONE ? 0 : ench.degree;

    // poison currently does roughly 6 damage per degree (over its duration)
    // do roughly 2x to 3x that much, scaling with spellpower
    const dice_def dam_dice(pois_str * 2, 12 + div_rand_round(pow * 6, 100));

    const int base_dam = dam_dice.roll();
    const int damage = mons_adjust_flavoured(mon, beam, base_dam, false);
    if (damage <= 0)
        return 0;

    if (agent && agent->deity() == GOD_FEDHAS && fedhas_protects(mon))
    {
        if (!tracer)
        {
            simple_god_message(
                        make_stringf(" protects %s plant from harm.",
                            agent->is_player() ? "your" : "a").c_str(),
                        GOD_FEDHAS);
        }
        return 0;
    }

    mon->expose_to_element(BEAM_FIRE, damage);

    if (tracer)
    {
        // players don't care about magnitude, just care if enemies exist
        if (agent && agent->is_player())
            return mons_aligned(mon, agent) ? -1 : 1;
        return mons_aligned(mon, agent) ? -1 * damage : damage;
    }

    if (you.see_cell(mon->pos()))
    {
        mprf("%s seems to burn from within%s",
             mon->name(DESC_THE).c_str(),
             attack_strength_punctuation(damage).c_str());
    }

    dprf("Dice: %dd%d; Damage: %d", dam_dice.num, dam_dice.size, damage);

    mon->hurt(agent, damage);

    if (mon->alive())
    {
        behaviour_event(mon, ME_WHACK, agent);

        // Monster survived, remove any poison.
        mon->del_ench(ENCH_POISON, true); // suppress spam
        print_wounds(*mon);
    }

    return 1;
}

/**
 * Burn poisoned players in the given tile, removing their poison state &
 * damaging them.
 *
 * @param where     The tile in question.
 * @param pow       The power with which Ignite Poison is being cast.
 *                  If -1, this indicates the spell is a test-run 'tracer'.
 * @param agent     The caster of Ignite Poison.
 * @return          If we're just running a tracer, return the expected damage
 *                  of burning the player in the given location (could be
 *                  negative if the player is an ally).
 *                  If it's not a tracer, return 1 if damage is caused & 0
 *                  otherwise.
 */

static int _ignite_poison_player(coord_def where, int pow, actor *agent)
{
    if (agent->is_player() || where != you.pos())
        return 0;

    const bool tracer = (pow == -1);  // Only testing damage, not dealing it
    if (tracer)                       // Give some fake damage to test resists
        pow = 100;

    // Step down heavily beyond light poisoning (or we could easily one-shot a heavily poisoned character)
    const int pois_str = stepdown((double)you.duration[DUR_POISONING] / 5000,
                                  2.25);
    if (!pois_str)
        return 0;

    const int base_dam = roll_dice(pois_str, 5 + pow/7);
    const int damage = resist_adjust_damage(&you, BEAM_FIRE, base_dam);

    if (tracer)
        return mons_aligned(&you, agent) ? -1 * damage : damage;

    const int resist = player_res_fire();
    if (resist > 0)
        mpr("You feel like your blood is boiling!");
    else if (resist < 0)
        mpr("The poison in your system burns terribly!");
    else
        mpr("The poison in your system burns!");

    ouch(damage, KILLED_BY_BEAM, agent->mid,
         "by burning poison", you.can_see(*agent),
         agent->as_monster()->name(DESC_A, true).c_str());
    if (damage > 0)
        you.expose_to_element(BEAM_FIRE, 2);

    mprf(MSGCH_RECOVERY, "You are no longer poisoned.");
    you.duration[DUR_POISONING] = 0;

    return damage ? 1 : 0;
}

/**
 * Would casting Ignite Poison possibly harm one of the player's allies in the
 * given cell?
 *
 * @param  where    The cell in question.
 * @return          1 if there's potential harm, 0 otherwise.
 */
static int _ignite_ally_harm(const coord_def &where)
{
    if (where == you.pos())
        return 0; // you're not your own ally!
    // (prevents issues with duplicate prompts when standing in an igniteable
    // cloud)

    return (_ignite_poison_clouds(where, -1, &you) < 0)   ? 1 :
           (_ignite_poison_monsters(where, -1, &you) < 0) ? 1 :
           (_ignite_poison_bog(where, -1, &you) < 0)      ? 1 :
            0;
}

/**
 * Let the player choose to abort a casting of ignite poison, if it seems
 * like a bad idea. (If they'd ignite themself.)
 *
 * @return      Whether the player chose to abort the casting.
 */
static bool maybe_abort_ignite()
{
    string prompt = "You are standing ";

    // XXX XXX XXX major code duplication (ChrisOelmueller)
    if (const cloud_struct* cloud = cloud_at(you.pos()))
    {
        if ((cloud->type == CLOUD_MEPHITIC || cloud->type == CLOUD_POISON)
            && !actor_cloud_immune(you, CLOUD_FIRE))
        {
            prompt += "in a cloud of ";
            prompt += cloud->cloud_name(true);
            prompt += "! Ignite poison anyway?";
            if (!yesno(prompt.c_str(), false, 'n'))
                return true;
        }
    }

    if (apply_area_visible(_ignite_ally_harm, you.pos()) > 0)
    {
        return !yesno("You might harm nearby allies! Ignite poison anyway?",
                      false, 'n');
    }

    return false;
}

/**
 * Does Ignite Poison affect the given creature?
 *
 * @param act       The creature in question.
 * @return          Whether Ignite Poison can directly damage the given
 *                  creature (not counting clouds).
 */
bool ignite_poison_affects(const actor* act)
{
    if (act->is_player())
        return you.duration[DUR_POISONING];
    return act->as_monster()->has_ench(ENCH_POISON);
}

/**
 * Does Ignite Poison do something to this cell?
 *
 * @param where       Where to look
 * @param agent     Who's casting
 * @return          If this cell will be affected
 */
bool ignite_poison_affects_cell(const coord_def where, actor* agent)
{
    return _ignite_poison_clouds(where, -1, agent)
         + _ignite_poison_monsters(where, -1, agent)
         + _ignite_poison_player(where, -1, agent)
         + _ignite_poison_bog(where, -1, agent) != 0;
}

/**
 * Cast the spell Ignite Poison, burning poisoned creatures and poisonous
 * clouds in LOS.
 *
 * @param agent         The spell's caster.
 * @param pow           The power with which the spell is being cast.
 * @param fail          If it's a player spell, whether the spell fail chance
 *                      was hit (whether the spell will fail as soon as the
 *                      player chooses not to abort the casting)
 * @param mon_tracer    Whether the 'casting' is just a tracer (a check to see
 *                      if it's worth actually casting)
 * @return              If it's a tracer, spret::success if the spell should
 *                      be cast & spret::abort otherwise.
 *                      If it's a real spell, spret::abort if the player chose
 *                      to abort the spell, spret::fail if they failed the cast
 *                      chance, and spret::success otherwise.
 */
spret cast_ignite_poison(actor* agent, int pow, bool fail, bool tracer)
{
    if (tracer)
    {
        // Estimate how much useful effect we'd get if we cast the spell now
        const int work = apply_area_visible([agent] (coord_def where) {
            return _ignite_poison_clouds(where, -1, agent)
                 + _ignite_poison_monsters(where, -1, agent)
                 + _ignite_poison_player(where, -1, agent)
                 + _ignite_poison_bog(where, -1, agent);
        }, agent->pos());

        return work > 0 ? spret::success : spret::abort;
    }

    if (agent->is_player())
    {
        if (maybe_abort_ignite())
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
        fail_check();
    }

    targeter_radius hitfunc(agent, LOS_NO_TRANS);
    flash_view_delay(
        agent->is_player()
            ? UA_PLAYER
            : UA_MONSTER,
        RED, 100, &hitfunc);

    mprf("%s %s the poison in %s surroundings!", agent->name(DESC_THE).c_str(),
         agent->conj_verb("ignite").c_str(),
         agent->pronoun(PRONOUN_POSSESSIVE).c_str());

    // this could conceivably cause crashes if the player dies midway through
    // maybe split it up...?
    apply_area_visible([pow, agent] (coord_def where) {
        _ignite_poison_clouds(where, pow, agent);
        _ignite_poison_monsters(where, pow, agent);
        _ignite_poison_bog(where, pow, agent);
        // Only relevant if a monster is casting this spell
        // (never hurts the caster)
        _ignite_poison_player(where, pow, agent);
        return 0; // ignored
    }, agent->pos());

    return spret::success;
}

static void _ignition_square(const actor */*agent*/, bolt beam, coord_def square, bool center)
{
    // HACK: bypass visual effect
    beam.target = square;
    beam.in_explosion_phase = true;
    beam.explosion_affect_cell(square);
    if (center)
        noisy(spell_effect_noise(SPELL_IGNITION),square);
}

vector<coord_def> get_ignition_blast_sources(const actor *agent, bool tracer)
{
    // Ignition affects squares that had hostile monsters on them at the time
    // of casting. This way nothing bad happens when monsters die halfway
    // through the spell.
    vector<coord_def> blast_sources;
    if (!agent)
        return blast_sources;

    for (actor_near_iterator ai(agent->pos(), LOS_NO_TRANS);
         ai; ++ai)
    {
        if (ai->is_monster()
            && !ai->as_monster()->wont_attack()
            && !mons_is_firewood(*ai->as_monster())
            && !mons_is_tentacle_segment(ai->as_monster()->type)
            && (!tracer || agent->can_see(**ai)))
        {
            blast_sources.push_back(ai->position);
        }
    }
    return blast_sources;
}

spret cast_ignition(const actor *agent, int pow, bool fail)
{
    ASSERT(agent->is_player());

    fail_check();

    //targeter_radius hitfunc(agent, LOS_NO_TRANS);

    // Ignition affects squares that had hostile monsters on them at the time
    // of casting. This way nothing bad happens when monsters die halfway
    // through the spell.
    vector<coord_def> blast_sources = get_ignition_blast_sources(agent);

    if (blast_sources.empty())
        canned_msg(MSG_NOTHING_HAPPENS);
    else
    {
        mpr("The air bursts into flame!");

        vector<coord_def> blast_adjacents;

        // Used to draw explosion cells
        bolt beam_visual;
        beam_visual.set_agent(agent);
        beam_visual.flavour       = BEAM_VISUAL;
        // XXX: why is this different from fireball?
        beam_visual.glyph         = dchar_glyph(DCHAR_FIRED_BURST);
        beam_visual.colour        = RED;
        beam_visual.ex_size       = 1;
        beam_visual.is_explosion  = true;

        // Used to deal damage; invisible
        bolt beam_actual;
        zappy(ZAP_IGNITION, pow, false, beam_actual);
        beam_actual.set_agent(agent);
        beam_actual.ex_size       = 0;
        beam_actual.apply_beam_conducts();

#ifdef DEBUG_DIAGNOSTICS
        dprf(DIAG_BEAM, "ignition dam=%dd%d",
             beam_actual.damage.num, beam_actual.damage.size);
#endif

        // Fake "shaped" radius 1 explosions (skipping squares with friends).
        for (coord_def pos : blast_sources)
        {
            for (adjacent_iterator ai(pos); ai; ++ai)
            {
                if (cell_is_solid(*ai)
                    && (!beam_actual.can_affect_wall(*ai)
                        || you_worship(GOD_FEDHAS)))
                {
                    continue;
                }

                actor *act = actor_at(*ai);

                // Friendly creature, don't blast this square.
                if (act && (act == agent
                            || (act->is_monster()
                                && act->as_monster()->wont_attack())))
                {
                    continue;
                }

                blast_adjacents.push_back(*ai);
                if (Options.use_animations & UA_BEAM)
                    beam_visual.explosion_draw_cell(*ai);
            }
            if (Options.use_animations & UA_BEAM)
                beam_visual.explosion_draw_cell(pos);
        }
        if (Options.use_animations & UA_BEAM)
        {
            viewwindow(false);
            update_screen();
            scaled_delay(50);
        }

        // Real explosions on each individual square.
        for (coord_def pos : blast_sources)
            _ignition_square(agent, beam_actual, pos, true);
        for (coord_def pos : blast_adjacents)
            _ignition_square(agent, beam_actual, pos, false);
    }

    return spret::success;
}

static int _discharge_monsters(const coord_def &where, int pow,
                               const actor &agent)
{
    actor* victim = actor_at(where);

    if (!victim || !victim->alive())
        return 0;

    int damage = (&agent == victim) ? 1 + random2(3 + pow / 15)
                                    : 3 + random2(5 + pow / 10
                                                  + (random2(pow) / 10));

    bolt beam;
    beam.flavour    = BEAM_ELECTRICITY; // used for mons_adjust_flavoured
    beam.glyph      = dchar_glyph(DCHAR_FIRED_ZAP);
    beam.colour     = LIGHTBLUE;
#ifdef USE_TILE
    beam.tile_beam  = -1;
#endif
    beam.draw_delay = 0;

    dprf("Static discharge on (%d,%d) pow: %d", where.x, where.y, pow);
    if ((Options.use_animations & UA_BEAM)
        && (victim->is_player() || victim->res_elec() <= 0))
    {
        beam.draw(where);
    }

    if (victim->is_player())
    {
        damage = 1 + random2(3 + pow / 15);
        dprf("You: static discharge damage: %d", damage);
        damage = check_your_resists(damage, BEAM_ELECTRICITY,
                                    "static discharge");
        mprf("You are struck by an arc of lightning%s",
             attack_strength_punctuation(damage).c_str());
        ouch(damage, KILLED_BY_BEAM, agent.mid, "by static electricity", true,
             agent.is_player() ? "you" : agent.name(DESC_A).c_str());
        if (damage > 0)
            victim->expose_to_element(BEAM_ELECTRICITY, 2);
    }
    // rEelec monsters don't allow arcs to continue.
    else if (victim->res_elec() > 0)
        return 0;
    else if (agent.deity() == GOD_FEDHAS
             && fedhas_protects(victim->as_monster()))
    {
        simple_god_message(
                    make_stringf(" protects %s plant from harm.",
                        agent.is_player() ? "your" : "a").c_str(),
                    GOD_FEDHAS);
        return 0;
    }
    else
    {
        monster* mons = victim->as_monster();

        // We need to initialize these before the monster has died.
        god_conduct_trigger conducts[3];
        if (agent.is_player())
            set_attack_conducts(conducts, *mons, you.can_see(*mons));

        dprf("%s: static discharge damage: %d",
             mons->name(DESC_PLAIN, true).c_str(), damage);
        damage = mons_adjust_flavoured(mons, beam, damage, false);
        mprf("%s is struck by an arc of lightning%s",
                mons->name(DESC_THE).c_str(),
                attack_strength_punctuation(damage).c_str());
        damage = mons_adjust_flavoured(mons, beam, damage);

        if (agent.is_player())
            _player_hurt_monster(*mons, damage, beam.flavour, false);
        else if (damage)
            mons->hurt(agent.as_monster(), damage);
    }

    // Recursion to give us chain-lightning -- bwr
    // Low power slight chance added for low power characters -- bwr
    if ((pow >= 10 && !one_chance_in(4)) || (pow >= 3 && one_chance_in(10)))
    {
        pow /= random_range(2, 3);
        damage += apply_random_around_square([pow, &agent] (coord_def where2) {
            return _discharge_monsters(where2, pow, agent);
        }, where, true, 1);
    }
    else if (damage > 0)
    {
        // Only printed if we did damage, so that the messages in
        // cast_discharge() are clean. -- bwr
        mpr("The lightning grounds out.");
    }

    return damage;
}

bool safe_discharge(coord_def where, vector<const actor *> &exclude)
{
    for (adjacent_iterator ai(where); ai; ++ai)
    {
        const actor *act = actor_at(*ai);
        if (!act)
            continue;

        if (find(exclude.begin(), exclude.end(), act) == exclude.end())
        {
            if (act->is_monster())
            {
                // Harmless to these monsters, so don't prompt about them.
                if (act->res_elec() > 0
                    || you.deity() == GOD_FEDHAS
                       && fedhas_protects(act->as_monster()))
                {
                    continue;
                }

                if (stop_attack_prompt(act->as_monster(), false, where))
                    return false;
            }
            // Don't prompt for the player, but always continue arcing.

            exclude.push_back(act);
            if (!safe_discharge(act->pos(), exclude))
                return false;
        }
    }

    return true;
}

spret cast_discharge(int pow, const actor &agent, bool fail, bool prompt)
{
    vector<const actor *> exclude;
    if (agent.is_player() && prompt && !safe_discharge(you.pos(), exclude))
        return spret::abort;

    fail_check();

    const int num_targs = 1 + random2(random_range(1, 3) + pow / 20);
    const int dam =
        apply_random_around_square([pow, &agent] (coord_def target) {
            return _discharge_monsters(target, pow, agent);
        }, agent.pos(), true, num_targs);

    dprf("Arcs: %d Damage: %d", num_targs, dam);

    if (dam > 0)
    {
        if (Options.use_animations & UA_BEAM)
            scaled_delay(100);
    }
    else
    {
        if (coinflip())
            mpr("The air crackles with electrical energy.");
        else
        {
            const bool plural = coinflip();
            mprf("%s blue arc%s ground%s harmlessly.",
                 plural ? "Some" : "A",
                 plural ? "s" : "",
                 plural ? " themselves" : "s itself");
        }
    }
    return spret::success;
}

pair<int, item_def *> sandblast_find_ammo()
{
    item_def *stone = nullptr;
    int num_stones = 0;
    for (item_def& i : you.inv)
    {
        if (i.is_type(OBJ_MISSILES, MI_STONE)
            && check_warning_inscriptions(i, OPER_DESTROY))
        {
            num_stones += i.quantity;
            stone = &i;
        }
    }
    return make_pair(num_stones, stone);
}

spret cast_sandblast(int pow, bolt &beam, bool fail)
{
    auto ammo = sandblast_find_ammo();

    if (ammo.first == 0 || !ammo.second)
    {
        mpr("You don't have any stones to cast with.");
        return spret::abort;
    }

    zap_type zap = ZAP_SANDBLAST;
    const spret ret = zapping(zap, pow, beam, true, nullptr, fail);

    if (ret == spret::success)
    {
        if (dec_inv_item_quantity(letter_to_index(ammo.second->slot), 1))
            mpr("You now have no stones remaining.");
        else if (!you.quiver_action.spell_is_quivered(SPELL_SANDBLAST))
            mprf_nocap("%s", ammo.second->name(DESC_INVENTORY).c_str());
    }

    return ret;
}

static bool _elec_not_immune(const actor *act)
{
    return act->res_elec() < 3 && !(you_worship(GOD_FEDHAS)
                                    && fedhas_protects(act->as_monster()));
}

coord_def get_thunderbolt_last_aim(actor *caster)
{
    const int &last_turn = caster->props[THUNDERBOLT_LAST_KEY].get_int();
    const coord_def &last_aim = caster->props[THUNDERBOLT_AIM_KEY].get_coord();

    // check against you.pos() in case the player has moved instantaneously,
    // via mesmerise, wjc, etc. In principle, this should probably also
    // record and check the player's location on their last cast.
    if (last_turn && last_turn + 1 == you.num_turns && last_aim != you.pos())
        return last_aim;
    else
        return coord_def();
}

static void _set_thundervolt_last_aim(actor *caster, coord_def aim)
{
    int &last_turn = caster->props[THUNDERBOLT_LAST_KEY].get_int();
    coord_def &last_aim = caster->props[THUNDERBOLT_AIM_KEY].get_coord();

    last_turn = you.num_turns;
    last_aim = aim;
}

spret cast_thunderbolt(actor *caster, int pow, coord_def aim, bool fail)
{
    int &charges = caster->props[THUNDERBOLT_CHARGES_KEY].get_int();
    ASSERT(charges <= LIGHTNING_MAX_CHARGE);

    coord_def prev = get_thunderbolt_last_aim(caster);
    if (!in_bounds(prev))
        charges = 0;

    targeter_thunderbolt hitfunc(caster, spell_range(SPELL_THUNDERBOLT, pow),
                                 prev);
    hitfunc.set_aim(aim);

    if (caster->is_player()
        && stop_attack_prompt(hitfunc, "zap", _elec_not_immune))
    {
        return spret::abort;
    }

    fail_check();

    const int juice
        = (spell_mana(SPELL_THUNDERBOLT, false) + charges)
          * LIGHTNING_CHARGE_MULT;

    dprf("juice: %d", juice);

    bolt beam;
    beam.name              = "thunderbolt";
    beam.aux_source        = "lightning rod";
    beam.origin_spell      = SPELL_THUNDERBOLT;
    beam.flavour           = BEAM_ELECTRICITY;
    beam.glyph             = dchar_glyph(DCHAR_FIRED_BURST);
    beam.colour            = LIGHTCYAN;
    beam.range             = 1;
    beam.hit               = AUTOMATIC_HIT;
    beam.ac_rule           = ac_type::proportional;
    beam.set_agent(caster);
#ifdef USE_TILE
    beam.tile_beam = -1;
#endif
    beam.draw_delay = 0;

    if (Options.use_animations & UA_BEAM)
    {
        for (const auto &entry : hitfunc.zapped)
        {
            if (entry.second <= 0)
                continue;

            beam.draw(entry.first);
        }

        scaled_delay(200);
    }

    beam.glyph = 0; // FIXME: a hack to avoid "appears out of thin air"

    for (const auto &entry : hitfunc.zapped)
    {
        if (entry.second <= 0)
            continue;

        // beams are incredibly spammy in debug mode
        if (!actor_at(entry.first))
            continue;

        int arc = hitfunc.arc_length[entry.first.distance_from(hitfunc.origin)];
        ASSERT(arc > 0);
        dprf("at distance %d, arc length is %d",
             entry.first.distance_from(hitfunc.origin), arc);
        beam.source = beam.target = entry.first;
        beam.source.x -= sgn(beam.source.x - hitfunc.origin.x);
        beam.source.y -= sgn(beam.source.y - hitfunc.origin.y);
        beam.damage = dice_def(div_rand_round(juice, LIGHTNING_CHARGE_MULT),
                               div_rand_round(30 + pow / 6, arc + 2));
        beam.fire();
    }

    _set_thundervolt_last_aim(caster, aim);

    if (charges < LIGHTNING_MAX_CHARGE)
        charges++;

    return spret::success;
}

// Find an enemy who would suffer from Awaken Forest.
actor* forest_near_enemy(const actor *mon)
{
    const coord_def pos = mon->pos();

    for (radius_iterator ri(pos, LOS_NO_TRANS); ri; ++ri)
    {
        actor* foe = actor_at(*ri);
        if (!foe || mons_aligned(foe, mon))
            continue;

        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (feat_is_tree(env.grid(*ai)) && cell_see_cell(pos, *ai, LOS_DEFAULT))
                return foe;
    }

    return nullptr;
}

// Print a message only if you can see any affected trees.
void forest_message(const coord_def pos, const string &msg, msg_channel_type ch)
{
    for (radius_iterator ri(pos, LOS_DEFAULT); ri; ++ri)
        if (feat_is_tree(env.grid(*ri))
            && cell_see_cell(you.pos(), *ri, LOS_DEFAULT))
        {
            mprf(ch, "%s", msg.c_str());
            return;
        }
}

void forest_damage(const actor *mon)
{
    const coord_def pos = mon->pos();
    const int hd = mon->get_hit_dice();

    if (one_chance_in(4))
    {
        forest_message(pos, random_choose(
            "The trees move their gnarly branches around.",
            "You feel roots moving beneath the ground.",
            "Branches wave dangerously above you.",
            "Trunks creak and shift.",
            "Tree limbs sway around you."), MSGCH_TALK_VISUAL);
    }

    for (radius_iterator ri(pos, LOS_NO_TRANS); ri; ++ri)
    {
        actor* foe = actor_at(*ri);
        if (!foe || mons_aligned(foe, mon))
            continue;

        if (is_sanctuary(foe->pos()))
            continue;

        for (adjacent_iterator ai(*ri); ai; ++ai)
            if (feat_is_tree(env.grid(*ai)) && cell_see_cell(pos, *ai, LOS_NO_TRANS))
            {
                int dmg = 0;
                string msg;

                if (!apply_chunked_AC(1, foe->evasion(ev_ignore::none, mon)))
                {
                    msg = random_choose(
                            "@foe@ @is@ waved at by a branch",
                            "A tree reaches out but misses @foe@",
                            "A root lunges up near @foe@");
                }
                else if (!(dmg = foe->apply_ac(hd + random2(hd), hd * 2 - 1,
                                               ac_type::proportional)))
                {
                    msg = random_choose(
                            "@foe@ @is@ scraped by a branch",
                            "A tree reaches out and scrapes @foe@",
                            "A root barely touches @foe@ from below");
                    if (foe->is_monster())
                        behaviour_event(foe->as_monster(), ME_WHACK);
                }
                else
                {
                    msg = random_choose(
                        "@foe@ @is@ hit by a branch",
                        "A tree reaches out and hits @foe@",
                        "A root smacks @foe@ from below");
                    if (foe->is_monster())
                        behaviour_event(foe->as_monster(), ME_WHACK);
                }

                msg = replace_all(replace_all(msg,
                    "@foe@", foe->name(DESC_THE)),
                    "@is@", foe->conj_verb("be"))
                    + attack_strength_punctuation(dmg);
                if (you.see_cell(foe->pos()))
                    mpr(msg);

                if (dmg <= 0)
                    break;

                foe->hurt(mon, dmg, BEAM_MISSILE, KILLED_BY_BEAM, "",
                          "by angry trees");

                break;
            }
    }
}

int dazzle_chance_numerator(int hd)
{
    return 95 - hd * 4;
}

int dazzle_chance_denom(int pow)
{
    return 150 - pow;
}

bool dazzle_monster(monster * mons, int pow)
{
    if (!mons || !mons_can_be_dazzled(mons->type))
        return false;

    const int numerator = dazzle_chance_numerator(mons->get_hit_dice());
    if (x_chance_in_y(numerator, dazzle_chance_denom(pow)))
    {
        mons->add_ench(mon_enchant(ENCH_BLIND, 1, &you,
                       random_range(4, 8) * BASELINE_DELAY));
        return true;
    }

    return false;
}

spret cast_dazzling_flash(int pow, bool fail, bool tracer)
{
    int range = spell_range(SPELL_DAZZLING_FLASH, pow);
    auto hitfunc = find_spell_targeter(SPELL_DAZZLING_FLASH, pow, range);
    bool (*vulnerable) (const actor *) = [](const actor * act) -> bool
    {
        // No fedhas checks needed, plants can't be dazzled
        return act->is_monster()
               && mons_can_be_dazzled(act->as_monster()->type);
    };

    if (tracer)
    {
        for (radius_iterator ri(you.pos(), range, C_SQUARE, LOS_SOLID_SEE, true); ri; ++ri)
        {
            if (!in_bounds(*ri))
                continue;

            const monster* mon = monster_at(*ri);

            if (!mon || !you.can_see(*mon))
                continue;

            if (!mon->friendly() && (*vulnerable)(mon))
                return spret::success;
        }

        return spret::abort;
    }


    // [eb] the simulationist in me wants to use LOS_DEFAULT
    // and let this blind through glass
    if (stop_attack_prompt(*hitfunc, "dazzle", vulnerable))
        return spret::abort;

    fail_check();

    bolt beam;
    beam.name = "energy";
    beam.flavour = BEAM_VISUAL;
    beam.origin_spell = SPELL_DAZZLING_FLASH;
    beam.set_agent(&you);
    beam.colour = WHITE;
    beam.glyph = dchar_glyph(DCHAR_EXPLOSION);
    beam.range = range;
    beam.ex_size = range;
    beam.is_explosion = true;
    beam.source = you.pos();
    beam.target = you.pos();
    beam.hit = AUTOMATIC_HIT;
    beam.loudness = 0;
    beam.explode(true, true);

    for (radius_iterator ri(you.pos(), range, C_SQUARE, LOS_SOLID_SEE, true);
         ri; ++ri)
    {
        monster* mons = monster_at(*ri);
        if (mons && dazzle_monster(mons, pow))
            simple_monster_message(*mons, " is dazzled.");
    }

    return spret::success;
}

static bool _toxic_can_affect(const actor *act)
{
    if (act->is_monster() && act->as_monster()->submerged())
        return false;

    // currently monsters are still immune at rPois 1
    return act->res_poison() < (act->is_player() ? 3 : 1);
}

spret cast_toxic_radiance(actor *agent, int pow, bool fail, bool mon_tracer)
{
    if (agent->is_player())
    {
        targeter_radius hitfunc(&you, LOS_NO_TRANS);
        if (stop_attack_prompt(hitfunc, "poison", _toxic_can_affect))
            return spret::abort;

        fail_check();

        if (!you.duration[DUR_TOXIC_RADIANCE])
            mpr("You begin to radiate toxic energy.");
        else
            mpr("Your toxic radiance grows in intensity.");

        you.increase_duration(DUR_TOXIC_RADIANCE, 2 + random2(pow/20), 15);
        toxic_radiance_effect(&you, 10, true);

        flash_view_delay(UA_PLAYER, GREEN, 300, &hitfunc);

        return spret::success;
    }
    else if (mon_tracer)
    {
        for (actor_near_iterator ai(agent->pos(), LOS_NO_TRANS); ai; ++ai)
        {
            if (!_toxic_can_affect(*ai) || mons_aligned(agent, *ai))
                continue;
            else
                return spret::success;
        }

        // Didn't find any susceptible targets
        return spret::abort;
    }
    else
    {
        monster* mon_agent = agent->as_monster();
        simple_monster_message(*mon_agent,
                               " begins to radiate toxic energy.");

        mon_agent->add_ench(mon_enchant(ENCH_TOXIC_RADIANCE, 1, mon_agent,
                                        (4 + random2avg(pow/15, 2)) * BASELINE_DELAY));
        toxic_radiance_effect(agent, 10);

        targeter_radius hitfunc(mon_agent, LOS_NO_TRANS);
        flash_view_delay(UA_MONSTER, GREEN, 300, &hitfunc);

        return spret::success;
    }
}

/*
 * Attempt to poison all monsters in line of sight of the caster.
 *
 * @param agent   The caster.
 * @param mult    A number to multiply the damage by.
 *                This is the time taken for the player's action in auts,
 *                or 10 if the spell was cast this turn.
 * @param on_cast Whether the spell was cast this turn. This only matters
 *                if the player cast the spell. If true, we trigger conducts
 *                if the player hurts allies; if false, we don't, to avoid
 *                the player being accidentally put under penance.
 *                Defaults to false.
 */
void toxic_radiance_effect(actor* agent, int mult, bool on_cast)
{
    int pow;
    if (agent->is_player())
        pow = calc_spell_power(SPELL_OLGREBS_TOXIC_RADIANCE, true);
    else
        pow = agent->as_monster()->get_hit_dice() * 8;

    for (actor_near_iterator ai(agent->pos(), LOS_NO_TRANS); ai; ++ai)
    {
        if (!_toxic_can_affect(*ai))
            continue;

        // Monsters can skip hurting friendlies
        if (agent->is_monster() && mons_aligned(agent, *ai))
            continue;

        int dam = roll_dice(1, 1 + pow / 20) * div_rand_round(mult, BASELINE_DELAY);
        dam = resist_adjust_damage(*ai, BEAM_POISON, dam);

        if (ai->is_player())
        {
            // We're affected only if we're not the agent.
            if (!agent->is_player())
            {
                ouch(dam, KILLED_BY_BEAM, agent->mid,
                    "by Olgreb's Toxic Radiance", true,
                    agent->as_monster()->name(DESC_A).c_str());

                poison_player(roll_dice(2, 3), agent->name(DESC_A),
                              "toxic radiance", false);
            }
        }
        else
        {
            god_conduct_trigger conducts[3];

            // Only trigger conducts on the turn the player casts the spell
            // (see PR #999).
            if (on_cast && agent->is_player())
                set_attack_conducts(conducts, *ai->as_monster());

            ai->hurt(agent, dam, BEAM_POISON);

            if (ai->alive())
            {
                behaviour_event(ai->as_monster(), ME_ANNOY, agent,
                                agent->pos());
                int q = mult / BASELINE_DELAY;
                int levels = roll_dice(q, 2) - q + (roll_dice(1, 20) <= (mult % BASELINE_DELAY));
                if (!ai->as_monster()->has_ench(ENCH_POISON)) // Always apply poison to an unpoisoned enemy
                    levels = max(levels, 1);
                poison_monster(ai->as_monster(), agent, levels);
            }
        }
    }
}

spret cast_searing_ray(int pow, bolt &beam, bool fail)
{
    const spret ret = zapping(ZAP_SEARING_RAY, pow, beam, true, nullptr,
                                   fail);

    if (ret == spret::success)
    {
        monster * mons = monster_at(beam.target);
        // Special value, used to avoid terminating ray immediately, since we
        // took a non-wait action on this turn (ie: casting it)
        you.attribute[ATTR_SEARING_RAY] = -1;
        you.props["searing_ray_aimed_at_spot"].get_bool() = beam.aimed_at_spot
                                                            || !mons;
        you.props["searing_ray_target"].get_coord() = beam.target;

        if (mons)
            you.props["searing_ray_mid"].get_int() = mons->mid;

        string msg = "(Press <w>%</w> to maintain the ray.)";
        insert_commands(msg, { CMD_WAIT });
        mpr(msg);
    }

    return ret;
}

void handle_searing_ray()
{
    ASSERT_RANGE(you.attribute[ATTR_SEARING_RAY], 1, 4);

    // All of these effects interrupt a channeled ray
    if (you.confused() || you.berserk())
    {
        end_searing_ray();
        return;
    }

    if (!enough_mp(1, true))
    {
        mpr("Without enough magic to sustain it, your searing ray dissipates.");
        end_searing_ray();
        return;
    }

    const zap_type zap = zap_type(ZAP_SEARING_RAY);
    const int pow = calc_spell_power(SPELL_SEARING_RAY, true);

    if (!you.props["searing_ray_aimed_at_spot"].get_bool())
    {
        monster* mons = nullptr;
        mons = monster_by_mid(you.props["searing_ray_mid"].get_int());
        // homing targeting, save the target location in case it dies or
        // disappears
        if (mons && mons->alive() && you.can_see(*mons))
            you.props["searing_ray_target"].get_coord() = mons->pos();
        else
            you.props["searing_ray_aimed_at_spot"] = true;
    }

    bolt beam;
    beam.thrower = KILL_YOU_MISSILE;
    beam.range   = calc_spell_range(SPELL_SEARING_RAY, pow);
    beam.source  = you.pos();
    beam.target  = you.props["searing_ray_target"].get_coord();

    // If friendlies have moved into the beam path, give a chance to abort
    if (!player_tracer(zap, pow, beam))
    {
        mpr("You stop channeling your searing ray.");
        end_searing_ray();
        return;
    }

    zappy(zap, pow, false, beam);

    aim_battlesphere(&you, SPELL_SEARING_RAY);
    beam.fire();
    trigger_battlesphere(&you);

    pay_mp(1);
    finalize_mp_cost();

    if (++you.attribute[ATTR_SEARING_RAY] > 3)
    {
        mpr("You finish channeling your searing ray.");
        end_searing_ray();
    }
}

void end_searing_ray()
{
    you.attribute[ATTR_SEARING_RAY] = 0;
    you.props.erase("searing_ray_target");
    you.props.erase("searing_ray_aimed_at_spot");
}

/**
 * Can a casting of Glaciate by the player injure the given creature?
 *
 * @param victim        The potential victim.
 * @return              Whether Glaciate can harm that victim.
 *                      (False for IOODs or friendly battlespheres.)
 */
static bool _player_glaciate_affects(const actor *victim)
{
    // TODO: deduplicate this with beam::ignores
    if (!victim)
        return false;

    const monster* mon = victim->as_monster();
    if (!mon) // player
        return true;

    return !mons_is_projectile(*mon)
            && (!mons_is_avatar(mon->type) || !mons_aligned(&you, mon));
}

dice_def glaciate_damage(int pow, int eff_range)
{
    // At or within range 3, this is equivalent to the old Ice Storm damage.
    return calc_dice(10, (54 + 3 * pow / 2) / eff_range);
}

spret cast_glaciate(actor *caster, int pow, coord_def aim, bool fail)
{
    const int range = spell_range(SPELL_GLACIATE, pow);
    targeter_cone hitfunc(caster, range);
    hitfunc.set_aim(aim);

    if (caster->is_player()
        && stop_attack_prompt(hitfunc, "glaciate", _player_glaciate_affects))
    {
        return spret::abort;
    }

    fail_check();

    bolt beam;
    beam.name              = "great icy blast";
    beam.aux_source        = "great icy blast";
    beam.flavour           = BEAM_ICE;
    beam.glyph             = dchar_glyph(DCHAR_EXPLOSION);
    beam.colour            = WHITE;
    beam.range             = 1;
    beam.hit               = AUTOMATIC_HIT;
    beam.source_id         = caster->mid;
    beam.hit_verb          = "engulfs";
    beam.origin_spell      = SPELL_GLACIATE;
    beam.set_agent(caster);
#ifdef USE_TILE
    beam.tile_beam = -1;
#endif
    beam.draw_delay = 0;

    if (Options.use_animations & UA_BEAM)
    {
        for (int i = 1; i <= range; i++)
        {
            for (const auto &entry : hitfunc.sweep[i])
            {
                if (entry.second <= 0)
                    continue;

                beam.draw(entry.first);
            }
            scaled_delay(25);
        }

        scaled_delay(100);
    }

    if (you.can_see(*caster) || caster->is_player())
    {
        mprf("%s %s a mighty blast of ice!",
             caster->name(DESC_THE).c_str(),
             caster->conj_verb("conjure").c_str());
    }

    beam.glyph = 0;

    for (int i = 1; i <= range; i++)
    {
        for (const auto &entry : hitfunc.sweep[i])
        {
            if (entry.second <= 0)
                continue;

            const int eff_range = max(3, (6 * i / LOS_DEFAULT_RANGE));

            beam.damage = glaciate_damage(pow, eff_range);

            if (actor_at(entry.first))
            {
                beam.source = beam.target = entry.first;
                beam.source.x -= sgn(beam.source.x - hitfunc.origin.x);
                beam.source.y -= sgn(beam.source.y - hitfunc.origin.y);
                beam.fire();
            }
            place_cloud(CLOUD_COLD, entry.first,
                        (18 + random2avg(45,2)) / eff_range, caster);
        }
    }

    noisy(spell_effect_noise(SPELL_GLACIATE), hitfunc.origin);

    return spret::success;
}

spret cast_starburst(int pow, bool fail, bool tracer)
{
    int range = spell_range(SPELL_STARBURST, pow);

    vector<coord_def> offsets = { coord_def(range, 0),
                                coord_def(range, range),
                                coord_def(0, range),
                                coord_def(-range, range),
                                coord_def(-range, 0),
                                coord_def(-range, -range),
                                coord_def(0, -range),
                                coord_def(range, -range) };

    bolt beam;
    beam.range        = range;
    beam.source       = you.pos();
    beam.source_id    = MID_PLAYER;
    beam.is_tracer    = tracer;
    beam.is_targeting = tracer;
    beam.dont_stop_player = true;
    beam.friend_info.dont_stop = true;
    beam.foe_info.dont_stop = true;
    beam.attitude = ATT_FRIENDLY;
    beam.thrower      = KILL_YOU;
    beam.origin_spell = SPELL_STARBURST;
    beam.draw_delay   = 5;
    zappy(ZAP_BOLT_OF_FIRE, pow, false, beam);

    for (const coord_def & offset : offsets)
    {
        beam.target = you.pos() + offset;
        if (!tracer && !player_tracer(ZAP_BOLT_OF_FIRE, pow, beam))
            return spret::abort;

        if (tracer)
        {
            beam.fire();
            // something to hit
            if (beam.foe_info.count > 0)
                return spret::success;
        }
    }

    if (tracer)
        return spret::abort;

    fail_check();

    // Randomize for nice animations
    shuffle_array(offsets);
    for (auto & offset : offsets)
    {
        beam.target = you.pos() + offset;
        beam.fire();
    }

    return spret::success;
}

void foxfire_attack(const monster *foxfire, const actor *target)
{
    actor * summoner = actor_by_mid(foxfire->summoner);
    if (!summoner || !summoner->alive())
    {
        // perish as your master once did
        return;
    }

    // Don't allow foxfires that have wandered off to attack before dissapating
    if (summoner && !(summoner->can_see(*foxfire)
                      && summoner->see_cell(target->pos())))
    {
        return;
    }

    bolt beam;
    beam.thrower = (foxfire && foxfire->friendly()) ? KILL_YOU :
                   (foxfire)                       ? KILL_MON
                                                  : KILL_MISC;
    beam.range       = 1;
    beam.source      = foxfire->pos();
    beam.source_id   = foxfire->summoner;
    beam.source_name = summoner->name(DESC_PLAIN, true);
    zappy(ZAP_FOXFIRE, foxfire->get_hit_dice(), !foxfire->friendly(), beam);
    beam.aux_source  = beam.name;
    beam.target      = target->pos();
    beam.fire();
}

/**
 * Hailstorm the given cell. (Per the spell.)
 *
 * @param where     The cell in question.
 * @param pow       The power with which the spell is being cast.
 * @param agent     The agent (player or monster) doing the hailstorming.
 */
static void _hailstorm_cell(coord_def where, int pow, actor *agent)
{
    bolt beam;
    zappy(ZAP_HAILSTORM, pow, agent->is_monster(), beam);
    beam.thrower    = agent->is_player() ? KILL_YOU : KILL_MON;
    beam.source_id  = agent->mid;
    beam.attitude   = agent->temp_attitude();
#ifdef USE_TILE
    beam.tile_beam  = -1;
#endif
    beam.draw_delay = 10;
    beam.source     = where;
    beam.target     = where;
    beam.hit_verb   = "pelts";

    beam.fire();
}

spret cast_hailstorm(int pow, bool fail, bool tracer)
{
    const int range = calc_spell_range(SPELL_HAILSTORM, pow);
    // used only for vulnerability check, not for the actual targeting
    auto hitfunc = find_spell_targeter(SPELL_HAILSTORM, pow, range);
    bool (*vulnerable) (const actor *) = [](const actor * act) -> bool
    {
      // actor guaranteed to be monster from usage,
      // but we'll verify it as a matter of good hygiene.
        const monster* mon = act->as_monster();
        return mon && !mons_is_firewood(*mon)
            && !(you_worship(GOD_FEDHAS) && fedhas_protects(mon))
            && !mons_is_projectile(*mon)
            && !(mons_is_avatar(mon->type) && mons_aligned(&you, mon))
            && !testbits(mon->flags, MF_DEMONIC_GUARDIAN);
    };

    if (tracer)
    {
        for (radius_iterator ri(you.pos(), range, C_SQUARE, LOS_NO_TRANS, true);
             ri; ++ri)
        {
            if (grid_distance(you.pos(), *ri) == 1 || !in_bounds(*ri))
                continue;

            const monster* mon = monster_at(*ri);

            if (!mon || !you.can_see(*mon))
                continue;

            if (!mon->friendly() && (*vulnerable)(mon))
                return spret::success;
        }

        return spret::abort;
    }

    if (stop_attack_prompt(*hitfunc, "hailstorm", vulnerable))
        return spret::abort;

    fail_check();

    mpr("A cannonade of hail descends around you!");

    for (radius_iterator ri(you.pos(), range, C_SQUARE, LOS_NO_TRANS, true);
         ri; ++ri)
    {
        if (grid_distance(you.pos(), *ri) == 1 || !in_bounds(*ri))
            continue;

        _hailstorm_cell(*ri, pow, &you);
    }

    return spret::success;
}

static void _imb_actor(actor * act, int pow, coord_def source)
{
    bolt beam;
    zappy(ZAP_MYSTIC_BLAST, pow, false, beam);
    beam.source          = source;
    beam.thrower         = KILL_YOU;
    beam.source_id       = MID_PLAYER;
    beam.range           = LOS_RADIUS;
    beam.ench_power      = pow;
    beam.aimed_at_spot   = true;

    beam.target          = act->pos();

    beam.flavour          = BEAM_VISUAL;
    beam.affects_nothing = true;
    beam.fire();

    zappy(ZAP_MYSTIC_BLAST, pow, false, beam);
    beam.affects_nothing = false;

    beam.affect_actor(act);
}

spret cast_imb(int pow, bool fail)
{
    int range = spell_range(SPELL_ISKENDERUNS_MYSTIC_BLAST, pow);
    auto hitfunc = find_spell_targeter(SPELL_ISKENDERUNS_MYSTIC_BLAST, pow, range);

    bool (*vulnerable) (const actor *) = [](const actor * act) -> bool
    {
        return !(act->is_monster()
                 && (mons_is_conjured(act->as_monster()->type)
                     || (have_passive(passive_t::shoot_through_plants)
                         && fedhas_protects(act->as_monster())))) ;
    };

    if (stop_attack_prompt(*hitfunc, "blast", vulnerable))
        return spret::abort;

    fail_check();

    mpr("You erupt in a blast of force!");

    vector<actor *> act_list;
    // knock back into dispersal could move the player, so save the current pos
    coord_def source = you.pos();

    for (actor_near_iterator ai(source, LOS_SOLID_SEE); ai; ++ai)
    {
        if (ai->pos().distance_from(you.pos()) > range
            || ai->pos() == you.pos() // so it's never aimed_at_feet
            || mons_is_conjured(ai->as_monster()->type)) // skip prisms &c.
        {
            continue;
        }

        act_list.push_back(*ai);
    }

    far_to_near_sorter sorter = { source };
    sort(act_list.begin(), act_list.end(), sorter);

    for (actor *act : act_list)
        if (cell_see_cell(source, act->pos(), LOS_SOLID_SEE)) // sanity check vs dispersal
            _imb_actor(act, pow, source);

    return spret::success;
}

void actor_apply_toxic_bog(actor * act)
{
    if (env.grid(act->pos()) != DNGN_TOXIC_BOG)
        return;

    if (!act->ground_level())
        return;

    const bool player = act->is_player();
    monster *mons = !player ? act->as_monster() : nullptr;

    if (mons && mons->type == MONS_FENSTRIDER_WITCH)
        return; // stilting above the muck!

    actor *oppressor = nullptr;

    for (map_marker *marker : env.markers.get_markers_at(act->pos()))
    {
        if (marker->get_type() == MAT_TERRAIN_CHANGE)
        {
            map_terrain_change_marker* tmarker =
                    dynamic_cast<map_terrain_change_marker*>(marker);
            if (tmarker->change_type == TERRAIN_CHANGE_BOG)
                oppressor = actor_by_mid(tmarker->mon_num);
        }
    }

    const int base_damage = dice_def(4, 6).roll();
    const int damage = resist_adjust_damage(act, BEAM_POISON_ARROW, base_damage);
    const int resist = base_damage - damage;

    const int final_damage = timescale_damage(act, damage);

    if (player && final_damage > 0)
    {
        mprf("You fester in the toxic bog%s",
                attack_strength_punctuation(final_damage).c_str());
    }
    else if (final_damage > 0)
    {
        behaviour_event(mons, ME_DISTURB, 0, act->pos());
        mprf("%s festers in the toxic bog%s",
                mons->name(DESC_THE).c_str(),
                attack_strength_punctuation(final_damage).c_str());
    }

    if (final_damage > 0 && resist > 0)
    {
        if (player)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);

        act->poison(oppressor, 7, true);
    }
    else if (final_damage > 0)
        act->poison(oppressor, 21, true);

    if (final_damage)
    {

        const string oppr_name =
            oppressor ? " "+apostrophise(oppressor->name(DESC_THE))
                      : "";
        dprf("%s %s %d damage from%s toxic bog.",
             act->name(DESC_THE).c_str(),
             act->conj_verb("take").c_str(),
             final_damage,
             oppr_name.c_str());

        act->hurt(oppressor, final_damage, BEAM_MISSILE,
                  KILLED_BY_POISON, "", "toxic bog");
    }
}

vector<coord_def> find_ramparts_walls(const coord_def &center)
{
    vector<coord_def> wall_locs;
    for (radius_iterator ri(center,
            spell_range(SPELL_FROZEN_RAMPARTS, -1, false), C_SQUARE,
                                                        LOS_NO_TRANS, true);
        ri; ++ri)
    {
        const auto feat = env.grid(*ri);
        if (feat_is_wall(feat))
            wall_locs.push_back(*ri);
    }
    return wall_locs;
}

/**
 * Cast Frozen Ramparts
 *
 * @param caster The caster.
 * @param pow    The spell power.
 * @param fail   Did this spell miscast? If true, abort the cast.
 * @return       spret::fail if one could be found but we miscast, and
 *               spret::success if the spell was successfully cast.
*/
spret cast_frozen_ramparts(int pow, bool fail)
{
    vector<coord_def> wall_locs = find_ramparts_walls(you.pos());

    if (wall_locs.empty())
    {
        mpr("There are no walls around you to affect.");
        return spret::abort;
    }

    fail_check();

    for (auto pos: wall_locs)
    {
        if (in_bounds(pos))
            noisy(spell_effect_noise(SPELL_FROZEN_RAMPARTS), pos);
        env.pgrid(pos) |= FPROP_ICY;
    }

    env.level_state |= LSTATE_ICY_WALL;
    you.props[FROZEN_RAMPARTS_KEY] = you.pos();

    mpr("The walls around you are covered in ice.");
    you.duration[DUR_FROZEN_RAMPARTS] = random_range(40 + pow,
                                                     80 + pow * 3 / 2);
    return spret::success;
}

void end_frozen_ramparts()
{
    if (!you.props.exists(FROZEN_RAMPARTS_KEY))
        return;

    const auto &pos = you.props[FROZEN_RAMPARTS_KEY].get_coord();
    ASSERT(in_bounds(pos));

    for (distance_iterator di(pos, false, false,
                spell_range(SPELL_FROZEN_RAMPARTS, -1, false)); di; di++)
    {
        env.pgrid(*di) &= ~FPROP_ICY;
        env.map_knowledge(*di).flags &= ~MAP_ICY;
    }

    you.props.erase(FROZEN_RAMPARTS_KEY);

    env.level_state &= ~LSTATE_ICY_WALL;
}

dice_def ramparts_damage(int pow, bool random)
{
    int size = 2 + pow / 5;
    if (random)
        size = 2 + div_rand_round(pow, 5);
    return dice_def(1, size);
}

static bool _abszero_target_check(monster &m)
{
    return you.see_cell_no_trans(m.pos())
            && !m.wont_attack()
            && !mons_is_firewood(m)
            && !mons_is_conjured(m.type);
}

// returns the closest target to the player, choosing randomly if there are more
// than one (see `fair` argument to distance_iterator).
static monster* _find_abszero_target(int radius, bool tracer)
{
    for (distance_iterator di(you.pos(), !tracer, true, radius); di; ++di)
    {
        monster *mon = monster_at(*di);
        if (mon && _abszero_target_check(*mon)
            && (!tracer || you.can_see(*mon)))
        {
            return mon;
        }
    }

    return nullptr;
}

// find all possible targets at the closest distance; used for targeting
vector<monster *> find_abszero_possibles(int radius)
{
    vector<monster *> result;
    monster *seed = _find_abszero_target(radius, true);
    if (seed)
    {
        const int distance = max(abs(you.pos().x - seed->pos().x),
                                        abs(you.pos().y - seed->pos().y));
        dprf("searching at rad %d, initial radius %d", distance, radius);
        for (distance_iterator di(you.pos(), true, true, distance); di; ++di)
        {
            monster *mon = monster_at(*di);
            if (mon && _abszero_target_check(*mon) && you.can_see(*mon))
                result.push_back(mon);
        }
    }
    return result;
}

spret cast_absolute_zero(int pow, bool fail, bool tracer)
{
    monster* const mon = _find_abszero_target(
            spell_range(SPELL_ABSOLUTE_ZERO, pow), tracer);

    if (tracer)
    {
        if (!mon || !you.can_see(*mon))
            return spret::abort;
        else
            return spret::success;
    }

    if (mon && you.can_see(*mon) && stop_attack_prompt(mon, false, mon->pos()))
        return spret::abort;

    fail_check();

    if (!mon)
        canned_msg(MSG_NOTHING_HAPPENS);
    else
    {
        targeter_radius hitfunc(&you, LOS_NO_TRANS);
        flash_view_delay(UA_PLAYER, LIGHTCYAN, 100, &hitfunc);

        god_conduct_trigger conducts[3];
        set_attack_conducts(conducts, *mon, you.can_see(*mon));

        if (mon->type == MONS_ROYAL_JELLY && !mon->is_summoned())
        {
            // need to do this here, because react_to_damage is never called
            mprf("A cloud of jellies burst out of %s as it chills to"
                 " absolute zero!", mon->name(DESC_THE, false).c_str());
            trj_spawn_fineff::schedule(&you, mon, mon->pos(), mon->hit_points);
        }
        else
        {
            mprf("You chill %s to absolute zero!",
                 you.can_see(*mon) ? mon->name(DESC_THE).c_str() : "something");
        }

        const coord_def pos = mon->pos();
        glaciate_freeze(mon, KILL_YOU, actor_to_death_source(&you));
        noisy(spell_effect_noise(SPELL_ABSOLUTE_ZERO), pos, you.mid);
    }

    return spret::success;
}

vector<coord_def> find_bog_locations(const coord_def &center, int pow)
{
    vector<coord_def> bog_locs;
    const int radius = spell_range(SPELL_NOXIOUS_BOG, pow, false);

    for (radius_iterator ri(center, radius, C_SQUARE, LOS_NO_TRANS, true); ri;
            ri++)
    {
        if (!feat_has_solid_floor(env.grid(*ri)))
            continue;

        // If a candidate cell is next to a solid feature, we can't bog it.
        // Additionally, if it's next to a cell we can't currently see, we
        // can't bog it, regardless of what the cell contains. Don't want to
        // leak information about out-of-los cells.
        bool valid = true;
        for (adjacent_iterator ai(*ri); ai; ai++)
        {
            if (!you.see_cell(*ai) || feat_is_solid(env.grid(*ai)))
            {
                valid = false;
                break;
            }
        }
        if (valid)
            bog_locs.push_back(*ri);
    }

    return bog_locs;
}
spret cast_noxious_bog(int pow, bool fail)
{
    vector <coord_def> bog_locs = find_bog_locations(you.pos(), pow);
    if (bog_locs.empty())
    {
        mpr("There are no places for you to create a bog.");
        return spret::abort;
    }

    fail_check();

    const int turns = 5 + random2(pow / 10);
    you.increase_duration(DUR_NOXIOUS_BOG, turns);

    for (auto pos : bog_locs)
    {
        temp_change_terrain(pos, DNGN_TOXIC_BOG, turns * BASELINE_DELAY,
                TERRAIN_CHANGE_BOG, you.as_monster());
    }

    flash_view_delay(UA_PLAYER, LIGHTGREEN, 100);
    mpr("You spew toxic sludge!");

    return spret::success;
}
