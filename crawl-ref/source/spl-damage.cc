/**
 * @file
 * @brief Damage-dealing spells not already handled elsewhere.
 *           Other targeted spells are covered in spl-zap.cc.
**/

#include "AppHdr.h"


#include "spl-damage.h"

#include <cmath>
#include <functional>

#include "act-iter.h"
#include "areas.h"
#include "attack.h"
#include "beam.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "delay.h"
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
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-tentacle.h"
#include "mutation.h"
#include "ouch.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-clouds.h" // big_cloud
#include "spl-goditem.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "spl-zap.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "tilepick.h"
#include "transform.h"
#include "traps.h"
#include "unicode.h"
#include "viewchar.h"
#include "view.h"
#include "xp-evoker-data.h" // for thunderbolt

#define SEARING_RAY_AIM_SPOT_KEY "searing_ray_aimed_at_spot"
#define SEARING_RAY_TARGET_KEY "searing_ray_target"
#define SEARING_RAY_MID_KEY "searing_ray_mid"
#define SEARING_RAY_POWER_KEY "searing_ray_power"

static bool _act_worth_targeting(const actor &caster, const actor &a)
{
    if (!caster.see_cell_no_trans(a.pos()))
        return false;
    if (a.is_player())
        return true;
    const monster &m = *a.as_monster();
    if (m.is_peripheral())
        return false;
    if (!caster.is_player())
        return true;
    return !god_protects(&you, m, true)
           && !testbits(m.flags, MF_DEMONIC_GUARDIAN);
}

// returns the closest target to the caster, choosing randomly if there are more
// than one (see `fair` argument to distance_iterator).
static actor* _find_closest_target(const actor &caster, int radius, bool tracer)
{
    for (distance_iterator di(caster.pos(), !tracer, true, radius); di; ++di)
    {
        actor *act = actor_at(*di);
        if (act && _act_worth_targeting(caster, *act)
            && cell_see_cell(caster.pos(), *di, LOS_SOLID)
            && (!tracer || caster.can_see(*act)))
        {
            return act;
        }
    }

    return nullptr;
}

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
    tempbeam.ex_size = (pow > 0) ? 3 : 2;
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

struct arc_victim
{
    coord_def source;
    actor *victim;
};

static const int ARC_DIST = 3;

static void _chain_lightning_to(const actor &caster, int power,
                                vector<arc_victim> &victims,
                                set<actor*> &seen_set, int arcs,
                                int initial_range = 0)
{
    bolt beam;
    zappy(ZAP_CHAIN_LIGHTNING, power, caster.is_monster(), beam);
    // Initial arc scales with range.
    if (initial_range > 0)
    {
        beam.damage.size =
            beam.damage.size * pow(0.6, (initial_range - 1) / 3.0);
    }
    // Further arcs scale a fixed amount per arc.
    for (int i = 0; i < arcs; i++)
        beam.damage.size = beam.damage.size * 3 / 5;
    const int dam_size = beam.damage.size;

    vector<arc_victim> new_victims;
    for (arc_victim &arc : victims)
    {
        beam.source = arc.source;
        beam.target = arc.victim->pos();
        beam.range = grid_distance(beam.source, beam.target);
        beam.source_id      = caster.mid;
        beam.thrower        = caster.is_player() ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
        beam.origin_spell   = SPELL_CHAIN_LIGHTNING;
        beam.aux_source     = "arc of chain lightning";
        // Reduce damage to the caster.
        beam.damage.size = dam_size / (arc.victim == &caster ? 6 : 1);
        beam.draw_delay = 0;
        beam.redraw_per_cell = false;
        beam.obvious_effect = true;
        beam.fire();

        // arc!
        for (distance_iterator di(beam.target, false, true, ARC_DIST); di; ++di)
        {
            actor *new_victim = actor_at(*di);
            if (new_victim
                && seen_set.find(new_victim) == seen_set.end()
                && cell_see_cell(*di, beam.target, LOS_SOLID)
                && _act_worth_targeting(caster, *new_victim))
            {
                new_victims.push_back(arc_victim{beam.target, new_victim});
                seen_set.insert(new_victim);
            }
        }
    }

    if (beam.animate)
        animation_delay(200, true);

    if (!new_victims.empty())
        _chain_lightning_to(caster, power, new_victims, seen_set, arcs + 1);
}

// Assuming the player casts Chain Lightning, who *might* get hit?
// Doesn't include monsters the player can't see.
vector<coord_def> chain_lightning_targets()
{
    vector<coord_def> targets;
    actor *seed = _find_closest_target(you, LOS_RADIUS, true);
    if (!seed)
        return targets;

    set<actor*> seen;
    vector<coord_def> to_check;
    seen.insert(seed);
    to_check.push_back(seed->pos());
    targets.push_back(seed->pos());
    while (!to_check.empty())
    {
        const coord_def arc_source = to_check.back();
        to_check.pop_back();
        for (distance_iterator di(arc_source, true, true, ARC_DIST); di; ++di)
        {
            actor *new_victim = actor_at(*di);
            if (new_victim
                && you.can_see(*new_victim)
                && seen.find(new_victim) == seen.end()
                && cell_see_cell(*di, you.pos(), LOS_SOLID)
                && _act_worth_targeting(you, *new_victim))
            {
                to_check.push_back(new_victim->pos());
                targets.push_back(new_victim->pos());
                seen.insert(new_victim);
            }
        }
    }

    return targets;
}

spret cast_chain_lightning(int pow, const actor &caster, bool fail)
{
    if (caster.is_player()
        && warn_about_bad_targets(SPELL_CHAIN_LIGHTNING,
                                  chain_lightning_targets()))
    {
        return spret::abort;
    }
    // NOTE: it's possible to hit something not in this list by arcing
    // chain lightning through an invisible enemy through an ally. Oh well...

    fail_check();

    actor* act = _find_closest_target(caster, LOS_RADIUS, false);
    if (!act)
    {
        if (caster.is_player())
            canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    if (you.can_see(caster))
    {
        mprf("Lightning arcs from %s %s!",
             apostrophise(caster.name(DESC_PLAIN)).c_str(),
             caster.hand_name(true).c_str());
    }

    vector<arc_victim> victims;
    victims.push_back(arc_victim{caster.pos(), act});
    set<actor*> seen_set;
    seen_set.insert(act);
    _chain_lightning_to(caster, pow, victims, seen_set, 0,
                        grid_distance(caster.pos(), act->pos()));

    return spret::success;
}

// XXX no friendly check
spret cast_chain_spell(spell_type spell_cast, int pow,
                            const actor *caster, bool fail)
{
    fail_check();
    bolt beam;

    // initialise beam structure
    beam.name           = "arc of chaos";
    beam.aux_source     = "chain of chaos";
    beam.glyph          = dchar_glyph(DCHAR_FIRED_ZAP);
    beam.flavour        = BEAM_CHAOS;
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
        // the chaos at the start or when it's out of LoS.
        if (first && see_source)
            mpr("A swirling arc of seething chaos appears!");
        first = false;

        if (see_source && !see_targ)
            mprf("The %s arcs out of your line of sight!", beam.name.c_str());
        else if (!see_source && see_targ)
            mprf("The %s suddenly appears!", beam.name.c_str());

        beam.source = source;
        beam.target = target;
        beam.colour       = ETC_RANDOM;
        beam.ench_power   = pow;
        beam.damage       = calc_dice(3, 5 + pow / 6);
        beam.real_flavour = BEAM_CHAOS;
        beam.flavour      = BEAM_CHAOS;

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
                                 bool god_conducts = true, bool quiet = false)
{
    ASSERT(mon.alive() || !god_conducts);

    if (god_conducts && god_protects(mon, false))
        return;

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

        if (damage && you.can_see(mon) && !quiet)
            print_wounds(mon);
    }
}

static bool _drain_lifeable(const actor* agent, const actor* act)
{
    if (!actor_is_susceptible_to_vampirism(*act)
        || act->res_negative_energy() >= 3)
    {
        return false;
    }

    if (!agent)
        return true;

    const monster* mons = agent->as_monster();
    const monster* m = act->as_monster();

    return !(agent->is_player() && act->wont_attack()
             || mons && act->is_player() && mons->wont_attack()
             || mons && m && mons_aligned(mons, m));
}

static int _los_spell_damage_actor(const actor* agent, actor &target,
                                   bolt &beam, bool actual, bool ignore_ac)
{

    beam.thrower = (agent && agent->is_player()) ? KILL_YOU :
                    agent                        ? KILL_MON
                                                 : KILL_NON_ACTOR;

    // Set conducts here. The monster needs to be alive when this is done, and
    // mons_adjust_flavoured() could kill it.
    monster *mon_targ = target.as_monster();
    god_conduct_trigger conducts[3];
    if (actual && YOU_KILL(beam.thrower) && mon_targ)
        set_attack_conducts(conducts, *mon_targ, you.can_see(target));

    int hurted;
    if (actual)
    {
        hurted = beam.damage.roll();
        if (!ignore_ac)
            hurted = max(0, target.apply_ac(hurted));
    }
    else
    {
        // Tracers use the average for damage calculations.
        hurted = (1 + beam.damage.num * beam.damage.size) / 2;
        if (!ignore_ac)
            hurted = max(0, hurted - target.armour_class() / 2);
    }

    const bool doFlavour = actual && beam.origin_spell != SPELL_DRAIN_LIFE;
    if (mon_targ)
        hurted = mons_adjust_flavoured(mon_targ, beam, hurted, doFlavour);
    else
    {
        hurted = check_your_resists(hurted, beam.flavour, beam.name, 0, doFlavour);
        if (agent->real_attitude() == ATT_MARIONETTE)
            hurted = 0;
    }
    dprf("damage done: %d", hurted);

    if (actual)
    {
        if (YOU_KILL(beam.thrower) && mon_targ)
        {
            _player_hurt_monster(*mon_targ, hurted, beam.flavour, false);
            if (target.alive())
                you.pet_target = target.mindex();
        }
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

/**
 * Returns the number of allied monsters adjacent to the given position which
 * other monsters can huddle against. (Reducing damage taken from Refrigeration.)
 */
int adjacent_huddlers(coord_def pos, bool only_in_sight)
{
    int adj_count = 0;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        const actor* act = actor_at(*ai);
        if (!act || !act->is_monster())
            continue;

        // For the targeter, only display huddlers the player is aware of.
        if (only_in_sight && !you.can_see(*act))
            continue;

        if (!act->is_firewood() && mons_aligned(act, actor_at(pos)))
            ++adj_count;
    }
    return adj_count;
}

static int _ozo_adj_dam(int base_dam, int adj_actors, bool actual)
{
    switch (adj_actors)
    {
    case 0:
        return base_dam;
    case 1:
        if (actual)
            return div_rand_round(7 * base_dam, 10);
        return 7 * base_dam / 10;
    default:
        if (actual)
            return div_rand_round(2 * base_dam, 5);
        return base_dam * 2 / 5;
    }
}

static spret _cast_los_attack_spell(spell_type spell, int pow,
                                         const actor* agent, bool actual,
                                         bool fail, int* damage_done)
{
    const bool player_caster = agent && agent->is_player();
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
               *prompt_verb = nullptr;
    bool (*vulnerable)(const actor *, const actor *) = nullptr;

    switch (spell)
    {
        case SPELL_OZOCUBUS_REFRIGERATION:
            player_msg = "The heat is drained from your surroundings.";
            global_msg = "Something drains the heat from around you.";
            mons_vis_msg = " drains the heat from the surrounding"
                           " environment!";
            mons_invis_msg = "The ambient heat is drained!";
            prompt_verb = "refrigerate";
            vulnerable = [](const actor *caster, const actor *act) {
                return act != caster
                       // Players don't get immunity with rC+++.
                       && (act->is_player() || act->res_cold() < 3)
                       && !god_protects(caster, act->as_monster());
            };
            break;

        case SPELL_DRAIN_LIFE:
            player_msg = "You draw life from your surroundings.";
            global_msg = "Something draws the life force from your"
                         " surroundings.";
            mons_vis_msg = " draws from the surrounding life force!";
            mons_invis_msg = "The surrounding life force dissipates!";
            prompt_verb = "drain life";
            vulnerable = &_drain_lifeable;
            break;

        case SPELL_SONIC_WAVE:
            player_msg = "You send a blast of sound all around you.";
            global_msg = "Something sends a blast of sound all around you.";
            mons_vis_msg = " sends a blast of sound all around you!";
            mons_invis_msg = "Sound blasts the surrounding area!";
            // prompt_verb = "sing" The singing sword prompts in melee-attack
            vulnerable = [](const actor *caster, const actor *act) {
                return act != caster
                       && !god_protects(caster, act->as_monster());
            };
            break;

        default:
            return spret::abort;
    }

    auto vul_hitfunc = [vulnerable](const actor *act) -> bool
    {
        return (*vulnerable)(&you, act);
    };

    if (actual)
    {
        if (player_caster)
        {
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
        else
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
    }

    vector<actor *> affected_actors;
    map<actor*, int> ozo_adj_count;

    for (actor_near_iterator ai((agent ? agent : &you)->pos(), LOS_NO_TRANS);
         ai; ++ai)
    {
        if (!actual && !agent->can_see(**ai))
            continue;
        if (!(*vulnerable)(agent, *ai))
            continue;

        if ((*vulnerable)(agent, *ai))
            affected_actors.push_back(*ai);

        // For perf, don't count when running tracers.
        if (spell == SPELL_OZOCUBUS_REFRIGERATION)
            ozo_adj_count[*ai] = actual ? adjacent_huddlers(ai->pos()) : 0;
    }

    const int avg_damage = (1 + beam.damage.num * beam.damage.size) / 2;
    int total_damage = 0;
    const int base_dam_size = beam.damage.size;

    for (auto a : affected_actors)
    {
        // Watch out for invalidation. Example: Ozocubu's refrigeration on
        // a bunch of ballistomycete spores that blow each other up.
        if (!a->alive())
            continue;

        if (spell == SPELL_OZOCUBUS_REFRIGERATION)
            beam.damage.size = _ozo_adj_dam(base_dam_size, ozo_adj_count[a], actual);

        int this_damage = _los_spell_damage_actor(agent, *a, beam, actual,
                                                    spell == SPELL_DRAIN_LIFE);
        total_damage += this_damage;

        if (actual || !this_damage)
            continue;

        if (a->is_monster() && !mons_is_threatening(*a->as_monster()))
            continue;

        if (!agent || !mons_aligned(a, agent))
        {
            beam.foe_info.count++;
            beam.foe_info.power +=
                (a->get_hit_dice() * this_damage / avg_damage);
        }
        else
        {
            beam.friend_info.count++;
            beam.friend_info.power +=
                (a->get_hit_dice() * this_damage / avg_damage);
        }
    }

    if (damage_done)
        *damage_done = total_damage;

    if (actual)
        return spret::success;

    if (player_caster)
        return beam.foe_info.count ? spret::success : spret::abort;
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

dice_def freeze_damage(int pow, bool random)
{
    return dice_def(1, random ? 3 + div_rand_round(pow * 3, 10)
                              : 3 + pow * 3 / 10);
}

spret cast_freeze(int pow, monster* mons, bool fail)
{
    pow = min(25, pow);

    if (!mons)
    {
        fail_check();
        canned_msg(MSG_NOTHING_CLOSE_ENOUGH);
        // If there's no monster there, you still pay the costs in
        // order to prevent locating invisible monsters.
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

    const int orig_hurted = freeze_damage(pow, true).roll();
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
    {
        mons->expose_to_element(BEAM_COLD, orig_hurted);
        you.pet_target = mons->mindex();
    }

    return spret::success;
}

// For airstrike purposes, how much empty space is there around
// the given target?
int airstrike_space_around(coord_def target, bool count_unseen)
{
    int empty_space = 0;
    for (adjacent_iterator ai(target); ai; ++ai)
    {
        if (!count_unseen && !env.map_knowledge(*ai).seen())
            continue;

        const auto feat = count_unseen ? env.grid(*ai)
                                       : env.map_knowledge(*ai).feat();
        if (feat_is_solid(feat))
            continue;

        if (count_unseen)
        {
            if (!actor_at(*ai))
                ++empty_space;
        }
        else if (you.pos() != *ai && !env.map_knowledge(*ai).monsterinfo())
            ++empty_space;
    }

    return empty_space;
}

spret cast_airstrike(int pow, coord_def target, bool fail)
{
    if (cell_is_solid(target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    monster* mons = monster_at(target);
    if (!mons)
    {
        fail_check();
        canned_msg(MSG_SPELL_FIZZLES);
        return spret::success; // still losing a turn
    }

    if (!god_protects(*mons)
        && stop_attack_prompt(mons, false, you.pos()))
    {
        return spret::abort;
    }

    fail_check();

    noisy(spell_effect_noise(SPELL_AIRSTRIKE), target);

    bolt pbeam;
    pbeam.flavour = BEAM_AIR;

    const int empty_space = airstrike_space_around(target, true);

    dice_def to_roll = base_airstrike_damage(pow, true);
    to_roll.size += empty_space * AIRSTRIKE_PER_SPACE_BONUS;
    int hurted = to_roll.roll();
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

    if (mons->alive())
        you.pet_target = mons->mindex();

    return spret::success;
}

// maximum damage before accounting for empty space
// used for damage display
dice_def base_airstrike_damage(int pow, bool random)
{
    if (random)
        return dice_def(2, div_rand_round(pow, 14));
    return dice_def(2, (pow + 13) / 14);
}

string describe_airstrike_dam(dice_def dice)
{
    return make_stringf("%dd(%d-%d)", dice.num, dice.size,
                        dice.size + MAX_AIRSTRIKE_BONUS);
}

string describe_resonance_strike_dam(dice_def dice)
{
    return make_stringf("(%d-%d)d%d", dice.num, dice.num + 4, dice.size);
}

spret cast_momentum_strike(int pow, coord_def target, bool fail)
{
    if (cell_is_solid(target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    monster* mons = monster_at(target);
    if (mons
        && !god_protects(*mons)
        && you.can_see(*mons)
        && stop_attack_prompt(mons, false, you.pos()))
    {
        return spret::abort;
    }

    fail_check();

    bolt beam;
    zappy(ZAP_MOMENTUM_STRIKE, pow, false, beam);
    beam.source_id    = MID_PLAYER;
    beam.thrower      = KILL_YOU;
    beam.attitude     = ATT_FRIENDLY;
    beam.origin_spell = SPELL_MOMENTUM_STRIKE;
    beam.source       = beam.target = target;
    beam.fire();

    if (!beam.foe_info.hurt && !beam.friend_info.hurt) // miss!
    {
        if (!mons || !you.can_see(*mons))
            mpr("The momentum dissipates harmlessly.");
        return spret::success;
    }

    const int dur = random_range(5, 8);
    if (you.duration[DUR_NO_MOMENTUM] < dur)
    {
        you.set_duration(DUR_NO_MOMENTUM, dur, dur,
                         "You are unable to move.");
    }

    return spret::success;
}

static ai_action::goodness _fire_permafrost_at(const actor &agent, int pow,
                                               coord_def target, bool tracer)
{
    const bool mon = agent.is_monster();

    bolt beam;
    beam.is_tracer    = tracer;
    beam.set_agent(&agent);
    beam.attitude     = mon ? mons_attitude(*agent.as_monster()) : ATT_FRIENDLY;
    beam.foe_ratio    = 80; // default
    beam.origin_spell = SPELL_PERMAFROST_ERUPTION;
    beam.source = beam.target = target;
    zappy(ZAP_PERMAFROST_ERUPTION_EARTH, pow, mon, beam);
    beam.fire();
    const ai_action::goodness earth_good = beam.good_to_fire();

    zappy(ZAP_PERMAFROST_ERUPTION_COLD, pow, mon, beam);
    beam.ex_size       = 1;
    beam.apply_beam_conducts();
    beam.refine_for_explosion();
    beam.explode();
    const ai_action::goodness ice_good = beam.good_to_fire();

    if (earth_good == ai_action::bad() || ice_good == ai_action::bad())
        return ai_action::bad();
    if (earth_good == ai_action::good() || ice_good == ai_action::good())
        return ai_action::good();
    return ai_action::neutral();
}

bool mons_should_fire_permafrost(int pow, const actor &agent)
{
    set<coord_def> targets = permafrost_targets(agent, pow, false);
    bool ever_good = false;
    for (auto target : targets)
    {
        const ai_action::goodness result = _fire_permafrost_at(agent, pow, target, true);
        if (result == ai_action::bad())
            return false; // be very careful!
        if (result == ai_action::good())
            ever_good = true;
    }
    return ever_good;
}

set<coord_def> permafrost_targets(const actor &caster, int pow, bool actual)
{
    set<coord_def> targets;

    const int range = spell_range(SPELL_PERMAFROST_ERUPTION, pow);
    vector<coord_def> all_hostiles = find_near_hostiles(range, actual, caster);
    if (all_hostiles.empty())
        return targets;

    set<coord_def> hostile_locs(all_hostiles.begin(), all_hostiles.end());
    int best_foes = 0;
    const coord_def src = caster.pos();

    for (coord_def t : all_hostiles)
    {
        if (grid_distance(t, src) < 2) // don't blow up the caster
            continue;

        int foes = 0; // not counting the centre
        for (adjacent_iterator ai(t); ai; ++ai)
            foes += hostile_locs.count(*ai); // ie 1 if present
        if (foes < best_foes)
            continue;

        if (foes > best_foes)
        {
            best_foes = foes;
            targets.clear();
        }
        targets.insert(t);
    }

    return targets;
}

spret cast_permafrost_eruption(actor &caster, int pow, bool fail)
{
    set<coord_def> maybe_targets = permafrost_targets(caster, pow, true);
    if (caster.is_player())
    {
        set<coord_def> maybe_victims(maybe_targets.begin(), maybe_targets.end());
        for (coord_def t : maybe_targets)
            for (adjacent_iterator ai(t); ai; ++ai)
                if (caster.see_cell(*ai)
                    && monster_at(*ai) && monster_at(*ai)->res_cold() < 3)
                {
                    maybe_victims.insert(*ai);
                }

        vector<coord_def> mvv(maybe_victims.begin(), maybe_victims.end());
        if (warn_about_bad_targets(SPELL_PERMAFROST_ERUPTION, mvv))
            return spret::abort;
    }

    fail_check();

    if (maybe_targets.empty())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }


    const coord_def targ = *random_iterator(maybe_targets);
    mpr("Bitter cold erupts, blasting rock from the ceiling!");

    _fire_permafrost_at(caster, pow, targ, false);

    return spret::success;
}

dice_def base_fragmentation_damage(int pow, bool random)
{
    return dice_def(3, random ? 4 + div_rand_round(pow, 5)
                              : 4 + pow / 5);
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
    { MONS_PILE_OF_DEBRIS,    { "stone", LIGHTGRAY } },
    { MONS_EARTH_ELEMENTAL,   { "rock", BROWN } },
    { MONS_ROCKSLIME,         { "rock", BROWN } },
    { MONS_BOULDER,           { "rock", BROWN } },
    { MONS_USHABTI,           { "rock", BROWN } },
    { MONS_STATUE,            { "rock", BROWN } },
    { MONS_GARGOYLE,          { "rock", BROWN } },
    { MONS_VV,                { "rock", BROWN } },
    { MONS_HELLFIRE_MORTAR,   { "rock", BROWN } },
    { MONS_IRON_ELEMENTAL,    { "metal", CYAN, frag_damage_type::metal } },
    { MONS_IRON_GOLEM,        { "metal", CYAN, frag_damage_type::metal } },
    { MONS_PEACEKEEPER,       { "metal", CYAN, frag_damage_type::metal } },
    { MONS_WAR_GARGOYLE,      { "metal", CYAN, frag_damage_type::metal } },
    { MONS_CLOCKWORK_BEE,     { "metal", CYAN, frag_damage_type::metal } },
    { MONS_CLOCKWORK_BEE_INACTIVE, { "metal", CYAN, frag_damage_type::metal } },
    { MONS_LIGHTNING_SPIRE,   { "metal", CYAN, frag_damage_type::metal } },
    { MONS_BLAZEHEART_GOLEM,  { "metal", CYAN, frag_damage_type::metal } },
    { MONS_WALKING_ALEMBIC,   { "metal", CYAN, frag_damage_type::metal } },
    { MONS_MONARCH_BOMB,      { "metal", CYAN, frag_damage_type::metal } },
    { MONS_BOMBLET,           { "metal", CYAN, frag_damage_type::metal } },
    { MONS_PHALANX_BEETLE,    { "metal", CYAN, frag_damage_type::metal } },
    { MONS_SPELLSPARK_SERVITOR, { "metal", CYAN, frag_damage_type::metal } },
    { MONS_PLATINUM_PARAGON,  { "platinum", CYAN, frag_damage_type::metal } },
    { MONS_GLASS_EYE,         { "glass", LIGHTCYAN,
                                frag_damage_type::crystal } },
    { MONS_CRYSTAL_GUARDIAN,  { "crystal", GREEN,
                                frag_damage_type::crystal } },
    { MONS_CRYSTAL_ECHIDNA,   { "crystal", GREEN,
                                frag_damage_type::crystal } },
    { MONS_ORANGE_STATUE,     { "orange crystal", LIGHTRED,
                                frag_damage_type::crystal } },
    { MONS_OBSIDIAN_BAT,      { "obsidian", MAGENTA,
                                frag_damage_type::crystal } },
    { MONS_OBSIDIAN_STATUE,   { "obsidian", MAGENTA,
                                frag_damage_type::crystal } },
    { MONS_DIAMOND_SAWBLADE,  { "diamond", WHITE,
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
    { DNGN_PERMAROCK_WALL, { "rock", "wall" } },
    { DNGN_CLEAR_ROCK_WALL, { "rock", "wall" } },
    { DNGN_CLEAR_STONE_WALL, { "rock", "wall" } },
    { DNGN_CLEAR_PERMAROCK_WALL, { "rock", "wall" } },
    { DNGN_ORCISH_IDOL, { "rock", "stone idol" } },
    { DNGN_GRANITE_STATUE, { "rock", "statue" } },
    { DNGN_PETRIFIED_TREE, { "rock", "petrified wood" } },
    { DNGN_SPIKE_LAUNCHER, { "rock", "spike launcher" } },
    // Stone arches and doors
    { DNGN_OPEN_DOOR, { "rock", "stone door frame" } },
    { DNGN_OPEN_CLEAR_DOOR, { "rock", "stone door frame" } },
    { DNGN_CLOSED_DOOR, { "rock", "stone door frame" } },
    { DNGN_CLOSED_CLEAR_DOOR, { "rock", "stone door frame" } },
    { DNGN_RUNED_DOOR, { "rock", "stone door frame" } },
    { DNGN_RUNED_CLEAR_DOOR, { "rock", "stone door frame" } },
    { DNGN_SEALED_DOOR, { "rock", "stone door frame" } },
    { DNGN_SEALED_CLEAR_DOOR, { "rock", "stone door frame" } },
    { DNGN_BROKEN_DOOR, { "rock", "stone door frame" } },
    { DNGN_BROKEN_CLEAR_DOOR, { "rock", "stone door frame" } },
    { DNGN_STONE_ARCH, { "rock", "stone arch" } },
    // Metal -- small but nasty explosion
    { DNGN_METAL_STATUE, { "metal", "metal statue", frag_damage_type::metal } },
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
    beam.glyph        = dchar_glyph(DCHAR_FIRED_BURST);
    beam.source_id    = caster->mid;
    beam.thrower      = caster->is_player() ? KILL_YOU : KILL_MON;
    beam.source       = you.pos();
    beam.origin_spell = SPELL_LRD;
    beam.hit          = AUTOMATIC_HIT;

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

    beam.damage  = base_fragmentation_damage(pow, caster->is_player());
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
            return spret::abort;
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
        {
            _player_hurt_monster(*mon, dam, BEAM_MINDBURST);
            if (mon->alive())
                you.pet_target = mon->mindex();
        }
        else if (dam)
            mon->hurt(caster, dam, BEAM_MINDBURST);
    }

    beam.explode(true, hole);

    return spret::success;
}

static int _shatter_mon_dice(const monster *mon)
{
    if (!mon)
        return DEFAULT_SHATTER_DICE;

    // Double damage to stone, metal and crystal - the same as the list of
    // monsters affected by LRD.
    if (map_find(fraggable_monsters, mon->type))
        return DEFAULT_SHATTER_DICE * 2;
    if (mon->is_insubstantial())
        return 1;
    if (mon->petrifying() || mon->petrified()
        || mon->is_skeletal() || mon->is_icy())
    {
        return DEFAULT_SHATTER_DICE * 2;
    }
    else if (mon->airborne() || mon->is_amorphous())
        return 1;
    // Normal damage to everything else.
    else
        return DEFAULT_SHATTER_DICE;
}

dice_def shatter_damage(int pow, monster *mon, bool random)
{
    return dice_def(_shatter_mon_dice(mon), random ? 5 + div_rand_round(pow, 3)
                                                   : 5 + pow / 3);
}

static int _shatter_monsters(coord_def where, int pow, actor *agent)
{
    monster* mon = monster_at(where);

    if (!mon || !mon->alive() || mon == agent
        || always_shoot_through_monster(agent, *mon))
    {
        return 0;
    }

    const dice_def dam_dice = shatter_damage(pow, mon);
    int damage = max(0, dam_dice.roll() - random2(1 + mon->armour_class()));

    if (agent->is_player())
        _player_hurt_monster(*mon, damage, BEAM_MMISSILE);
    else if (damage)
        mon->hurt(agent, damage);

    return damage;
}

static const map<dungeon_feature_type, int> terrain_shatter_chances = {
    { DNGN_CLOSED_DOOR,     100 }, // also applies to all other door types
    { DNGN_GRATE,           100 },
    { DNGN_ORCISH_IDOL,     100 },
    { DNGN_GRANITE_STATUE,  100 },
    { DNGN_CLEAR_ROCK_WALL,  33 },
    { DNGN_ROCK_WALL,        33 },
    { DNGN_SLIMY_WALL,       33 },
    { DNGN_CRYSTAL_WALL,     33 },
    { DNGN_TREE,             33 }, // also applies to all other types of tree
    { DNGN_CLEAR_STONE_WALL, 25 },
    { DNGN_STONE_WALL,       25 },
    { DNGN_METAL_STATUE,     15 },
    { DNGN_METAL_WALL,       15 },
};

// Returns a percentage chance of the given wall being shattered by the given
// agent casting Shatter, where 100 is guaranteed.
int terrain_shatter_chance(coord_def where, const actor &agent)
{
    // if not in-bounds then we can't really shatter it -- bwr
    if (!in_bounds(where))
        return 0;

    if (env.markers.property_at(where, MAT_ANY, "veto_destroy") == "veto")
        return 0;

    dungeon_feature_type feat = env.grid(where);
    if (feat_is_tree(feat))
    {
        if (agent.deity() == GOD_FEDHAS)
            return 0;
        feat = DNGN_TREE;
    }
    else if (feat_is_door(feat))
        feat = DNGN_CLOSED_DOOR;

    auto feat_chance = terrain_shatter_chances.find(feat);
    if (feat_chance == terrain_shatter_chances.end())
        return 0;
    return feat_chance->second;
}

static int _shatter_walls(coord_def where, actor *agent)
{
    const int chance = terrain_shatter_chance(where, *agent);

    if (!x_chance_in_y(chance, 100))
        return 0;

    if (you.see_cell(where))
    {
        const dungeon_feature_type feat = env.grid(where);
        if (feat_is_door(feat))
            mpr("A door shatters!");
        else if (feat == DNGN_GRATE)
            mpr("An iron grate is ripped into pieces!");
    }

    noisy(spell_effect_noise(SPELL_SHATTER), where);
    destroy_wall(where);
    return 1;
}

static int _shatter_player_dice()
{
    if (you.is_insubstantial())
        return 1;
    if (you.petrified() || you.petrifying())
        return 6; // reduced later by petrification's damage reduction
    else if (you.form == transformation::statue || you.species == SP_GARGOYLE)
        return 6;
    else if (you.airborne() || you.is_amorphous())
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
               && !god_protects(*act->as_monster())
               && !always_shoot_through_monster(&you, *act->as_monster())
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
        dest += _shatter_walls(*di, &you);
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
            dest += _shatter_walls(*di, caster);
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
    beam.explode();

    // We need to do this separately from the beam, since explosions reset
    // noise values & visual beams don't cause noise regardless.
    noisy(7, where);

    counted_monster_list affected_monsters;
    for (adjacent_iterator ai(where, false); ai; ++ai)
    {
        monster *mon = monster_at(*ai);
        if (!mon || !mon->alive()
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

dice_def scorch_damage(int pow, bool random)
{
    if (random)
    {
        const int max_dam = 10 + div_rand_round(pow, 6);
        return calc_dice(2, max_dam);
    }
    return dice_def(2, (10 + pow / 6) / 2);
}

spret cast_scorch(const actor& agent, int pow, bool fail)
{
    fail_check();

    const int range = spell_range(SPELL_SCORCH, pow);
    auto targeter = make_unique<targeter_scorch>(agent, range, true);
    actor *targ = nullptr;
    int seen = 0;
    for (auto ti = targeter->affected_iterator(AFF_MAYBE); ti; ++ti)
        if (one_chance_in(++seen))
            targ = actor_at(*ti);

    if (!targ)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    const int base_dam = scorch_damage(pow, true).roll();
    const int post_ac_dam = max(0, targ->apply_ac(base_dam));

    mprf("Flames lash %s%s.", targ->name(DESC_THE).c_str(),
         post_ac_dam ? "" : " but do no damage");
    const coord_def p = targ->pos();
    noisy(spell_effect_noise(SPELL_SCORCH), p);

    bolt beam;
    beam.flavour = BEAM_FIRE;

    const int damage = (targ->is_monster())
                        ? mons_adjust_flavoured(targ->as_monster(), beam, post_ac_dam)
                        : check_your_resists(post_ac_dam, BEAM_FIRE, "scorch", &beam);

    if (agent.is_player())
        _player_hurt_monster(*targ->as_monster(), damage, beam.flavour);
    else
        targ->hurt(&agent, damage, BEAM_FIRE);

    // XXX: interact with clouds of cold?
    // XXX: dedup with beam::affect_place_clouds()?
    if (feat_is_water(env.grid(p)) && !cloud_at(p))
        place_cloud(CLOUD_STEAM, p, 2 + random2(5), &agent, 11);

    if (!targ->alive())
    {
        flash_tile(p, RED);
        return spret::success;
    }

    if (agent.is_player())
        you.pet_target = targ->mindex();

    if (damage > 0)
    {
        const int dur = 3 + div_rand_round(damage, 3);
        if (targ->is_monster())
        {
            monster* mon = targ->as_monster();
            if (you.can_see(*mon) && !mon->has_ench(ENCH_FIRE_VULN))
            {
                mprf("%s fire resistance burns away.",
                    mon->name(DESC_ITS).c_str());
            }
            mon->add_ench(mon_enchant(ENCH_FIRE_VULN, 1, &agent,
                                      dur * BASELINE_DELAY));
        }
        else
        {
            mprf(MSGCH_DANGER, "Your fire resistance burns away!");
            you.duration[DUR_FIRE_VULN] += dur * 3 / 2;
        }
    }

    flash_tile(targ->pos(), RED);
    return spret::success;
}

/// Scorch's target selection (see targeter_scorch)
vector<coord_def> find_near_hostiles(int range, bool affect_invis, const actor& agent)
{
    vector<coord_def> hostiles;
    for (radius_iterator ri(agent.pos(), range, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        actor *act = actor_at(*ri);
        if (act
            && !mons_aligned(&agent, act)
            && _act_worth_targeting(agent, *act)
            && (affect_invis || agent.can_see(*act)))
        {
            hostiles.push_back(*ri);
        }
    }
    return hostiles;
}

dice_def irradiate_damage(int pow, bool random)
{
    const int dice = 3;
    const int max_dam = 35 + (random ? div_rand_round(pow, 2) : pow / 2);
    return calc_dice(dice, max_dam, random);
}

/**
 * Irradiate the given cell. (Per the spell.)
 *
 * @param where     The cell in question.
 * @param pow       The power with which the spell is being cast.
 * @param agent     The agent (player or monster) doing the irradiating.
 */
static int _irradiate_cell(coord_def where, int pow, const actor &agent)
{
    actor *act = actor_at(where);
    if (!act || !act->alive()
        || act->is_monster() && always_shoot_through_monster(&agent, *act->as_monster()))
    {
        return 0;
    }

    const bool hitting_player = act->is_player();

    const dice_def dam_dice = irradiate_damage(pow);
    const int base_dam = dam_dice.roll();
    const int dam = act->apply_ac(base_dam);

    if (god_protects(&agent, act->as_monster(), false))
        return 0;

    mprf("%s %s blasted with magical radiation%s",
         act->name(DESC_THE).c_str(),
         conjugate_verb("are", hitting_player).c_str(),
         attack_strength_punctuation(dam).c_str());

    if (agent.is_player())
        _player_hurt_monster(*act->as_monster(), dam, BEAM_MMISSILE);
    else if (dam)
        act->hurt(&agent, dam, BEAM_MMISSILE);

    if (act->alive())
    {
        // be nice and "only" contaminate the player a lot
        if (hitting_player)
            contaminate_player(2000 + random2(1000));
        else if (coinflip())
            act->malmutate("");
    }

    return dam;
}

/**
 * Attempt to cast the spell "Irradiate", damaging & deforming enemies around
 * the player.
 *
 * @param pow   The power at which the spell is being cast.
 * @param caster   The actor doing the irradiating.
 * @param fail  Whether the player has failed to cast the spell.
 * @return      spret::abort if the player changed their mind about casting after
 *              realizing they would hit an ally; spret::fail if they failed the
 *              cast chance; spret::success otherwise.
 */
spret cast_irradiate(int powc, actor &caster, bool fail)
{
    targeter_radius hitfunc(&caster, LOS_NO_TRANS, 1, 0, 1);
    auto vulnerable = [&caster](const actor *act) -> bool
    {
        return !act->is_player()
               && !always_shoot_through_monster(&caster, *act->as_monster())
               && !god_protects(&caster, *act->as_monster());
    };

    if (caster.is_player() && stop_attack_prompt(hitfunc, "irradiate", vulnerable))
        return spret::abort;

    fail_check();

    if (caster.is_player())
        mpr("You erupt in a fountain of uncontrolled magic!");
    else
    {
        simple_monster_message(*caster.as_monster(),
                               " erupts in a fountain of uncontrolled magic!");
    }

    bolt beam;
    beam.name = "irradiate";
    beam.flavour = BEAM_VISUAL;
    beam.set_agent(&caster);
    beam.colour = ETC_MUTAGENIC;
    beam.glyph = dchar_glyph(DCHAR_EXPLOSION);
    beam.range = 1;
    beam.ex_size = 1;
    beam.is_explosion = true;
    beam.explode_delay = beam.explode_delay * 3 / 2;
    beam.source = caster.pos();
    beam.target = caster.pos();
    beam.hit = AUTOMATIC_HIT;
    beam.explode(true, true);

    apply_random_around_square([powc, &caster] (coord_def where) {
        return _irradiate_cell(where, powc, caster);
    }, caster.pos(), true, 8);

    if (caster.is_player())
        contaminate_player(1250 + random2(750));
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

        if (god_protects(agent, act->as_monster()))
            return 0;

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

    place_cloud(CLOUD_FIRE, where, 2 + random2(1 + div_rand_round(pow, 30)), agent);

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
    if (invalid_monster(mon) || mon == agent || tracer && !agent->can_see(*mon))
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

    if (god_protects(agent, *mon, tracer))
        return 0;

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

    if (agent && agent->is_player())
        _player_hurt_monster(*mon, damage, BEAM_MMISSILE);
    else
        mon->hurt(agent, damage);

    if (mon->alive())
    {
        // Monster survived, remove any poison.
        mon->del_ench(ENCH_POISON, true); // suppress spam
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
        if (!ai->wont_attack()
            && !ai->is_firewood()
            && !mons_is_tentacle_segment(ai->type)
            && !mons_is_projectile(*ai->as_monster())
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
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

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
        animation_delay(50, true);

    // Real explosions on each individual square.
    for (coord_def pos : blast_sources)
        _ignition_square(agent, beam_actual, pos, true);
    for (coord_def pos : blast_adjacents)
        _ignition_square(agent, beam_actual, pos, false);

    return spret::success;
}

static const int DISCHARGE_POWER_DIV = 12;

static int _discharge_monsters(const coord_def &where, int pow,
                               const actor &agent, int remaining)
{
    actor* victim = actor_at(where);

    if (!victim || !victim->alive())
        return 0;

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

    int damage = FLAT_DISCHARGE_ARC_DAMAGE
                 + random2(3 + div_rand_round(pow, DISCHARGE_POWER_DIV));

    // Reduced damage when arcing back to the caster.
    if (&agent == victim)
        damage = div_rand_round(damage, 2);

    damage = max(0, victim->apply_ac(damage, 0, ac_type::half));

    if (victim->is_player())
    {
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
    // Elec immune monsters don't allow arcs to continue.
    else if (victim->res_elec() >= 3)
        return 0;
    else if (god_protects(&agent, *victim->as_monster(), false))
        return 0;
    else
    {
        monster* mons = victim->as_monster();

        // We need to initialize these before the monster has died.
        god_conduct_trigger conducts[3];
        if (agent.is_player())
            set_attack_conducts(conducts, *mons, you.can_see(*mons));

        dprf("%s: static discharge damage: %d",
             mons->name(DESC_PLAIN, true).c_str(), damage);
        mprf("%s is struck by an arc of lightning%s",
                mons->name(DESC_THE).c_str(),
                attack_strength_punctuation(damage).c_str());
        damage = mons_adjust_flavoured(mons, beam, damage);

        if (agent.is_player())
            _player_hurt_monster(*mons, damage, beam.flavour, false);
        else if (damage)
            mons->hurt(agent.as_monster(), damage);
    }

    // Recursion to give us chain-lightning
    if (remaining > 0)
    {
        damage += apply_random_around_square([pow, &agent, remaining]
                                            (coord_def where2) {
            return _discharge_monsters(where2, pow, agent, remaining - 1);
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

bool safe_discharge(coord_def where, vector<const actor *> &exclude, bool check_only)
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
                if (act->res_elec() >= 3 || god_protects(*act->as_monster()))
                    continue;

                if (stop_attack_prompt(act->as_monster(), false, where, nullptr,
                                       coord_def(), check_only))
                {
                    return false;
                }
            }
            // Don't prompt for the player, but always continue arcing.

            exclude.push_back(act);
            if (!safe_discharge(act->pos(), exclude, check_only))
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

    const int num_targs = 1 + random2(2 + div_rand_round(pow, 25));
    const int dam =
        apply_random_around_square([pow, &agent] (coord_def target) {
            return _discharge_monsters(target, pow, agent,
                        1 + random2(2 + div_rand_round(pow, 25)));
        }, agent.pos(), true, num_targs);

    dprf("Arcs: %d Damage: %d", num_targs, dam);

    if (dam > 0)
    {
        if (Options.use_animations & UA_BEAM)
            animation_delay(100, Options.reduce_animations);
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

int discharge_max_damage(int pow)
{
    return FLAT_DISCHARGE_ARC_DAMAGE
           + (pow + DISCHARGE_POWER_DIV - 1) / DISCHARGE_POWER_DIV;
}

dice_def arcjolt_damage(int pow, bool random)
{
    return dice_def(1, random ? 10 + div_rand_round(pow, 2) : 10 + pow / 2);
}

static vector<coord_def> _get_chain_targets(const actor &agent,
                                            vector<coord_def>& seed_points, bool actual)
{
    set<coord_def> seen;
    vector<coord_def> targets;

    for (auto point : seed_points)
        seen.insert(point);

    while (!seed_points.empty())
    {
        vector<coord_def> next_frontier;
        for (coord_def p : seed_points)
        {
            actor* act = actor_at(p);
            const bool seen_act = act && (actual || agent.can_see(*act));
            if (!seen_act
                || act == &agent
                || act->res_elec() >= 3)
            {
                continue;
            }

            const monster* mon = act->as_monster();
            if (mon && (mons_is_projectile(*mon) || god_protects(&agent, *mon)))
                continue;

            targets.push_back(p);

            for (adjacent_iterator ai(p); ai; ++ai)
            {
                if (!seen.count(*ai) && agent.see_cell(*ai))
                {
                    seen.insert(*ai);
                    next_frontier.push_back(*ai);
                }
            }
        }
        seed_points = next_frontier;
    }
    return targets;
}

static void _do_chain_jolt(const actor& agent, vector<coord_def>& targets, dice_def damage)
{
    bolt beam;
    beam.flavour = BEAM_ELECTRICITY;
    beam.thrower = agent.is_player() ? KILL_YOU : KILL_MON;
    beam.glyph      = dchar_glyph(DCHAR_FIRED_ZAP);
    beam.colour     = LIGHTBLUE;
#ifdef USE_TILE
    beam.tile_beam  = -1;
#endif
    beam.draw_delay = 10;

    // Do the full animation first, so it doesn't get interrupted mid-way by messages
    if (Options.use_animations & UA_BEAM)
    {
        for (coord_def t : targets)
                beam.draw(t);
    }

    for (coord_def t : targets)
    {
        actor *act = actor_at(t);
        if (!act || !act->alive()) // may have died midway through casting
            continue;

        monster* mon = act->as_monster();
        const int rolled_dam = damage.roll();
        const int post_ac_dam = max(0, act->apply_ac(rolled_dam, 0,
                                                     ac_type::half));
        const int post_resist_dam = mon ? mons_adjust_flavoured(mon, beam,
                                                                post_ac_dam)
                                        : check_your_resists(post_ac_dam,
                                                             beam.flavour,
                                                             "arcjolt");

        if (act->is_player())
        {
            mprf("You are struck by an electric surge%s",
                attack_strength_punctuation(post_resist_dam).c_str());
        }
        else
        {
            mprf("%s is struck by an electric surge%s",
                    mon->name(DESC_THE).c_str(),
                    attack_strength_punctuation(post_resist_dam).c_str());
        }

        if (post_resist_dam)
        {
            if (agent.is_player())
                _player_hurt_monster(*mon, post_resist_dam, beam.flavour);
            else
                act->hurt(&agent, post_resist_dam);
        }
        act->expose_to_element(beam.flavour, post_resist_dam);
        noisy(spell_effect_noise(SPELL_ARCJOLT), t);
    }
    if (Options.use_animations & UA_BEAM)
        animation_delay(100, Options.reduce_animations);
}

vector<coord_def> galvanic_targets(const actor &agent, coord_def pos, bool actual)
{
    vector<coord_def> to_check;
    to_check.push_back(pos);

    return _get_chain_targets(agent, to_check, actual);
}

vector<coord_def> arcjolt_targets(const actor &agent, bool actual)
{
    vector<coord_def> to_check;
    to_check.push_back(agent.pos());

    for (radius_iterator ri(agent.pos(), 2, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
        to_check.push_back(*ri);

    return _get_chain_targets(agent, to_check, actual);
}

spret cast_arcjolt(int pow, const actor &agent, bool fail)
{
    if (agent.is_player()
        && warn_about_bad_targets(SPELL_ARCJOLT,
                                   arcjolt_targets(agent, false)))
    {
            return spret::abort;
    }
    // NOTE: it's possible to hit something not in this list by
    // arcing through an invisible enemy into an ally. Oh well...

    fail_check();

    if (agent.is_player())
        mpr("Electricity surges outward!");
    else
    {
        simple_monster_message(*agent.as_monster(),
                               " emits a burst of electricity!");
    }

    auto targets = arcjolt_targets(agent, true);

    _do_chain_jolt(agent, targets, arcjolt_damage(pow, true));

    return spret::success;
}

void do_galvanic_jolt(const actor& agent, coord_def pos, dice_def damage)
{
    auto targets = galvanic_targets(agent, pos, true);
    _do_chain_jolt(agent, targets, damage);
}

static bool _plasma_targetable(const actor &agent, monster &m, bool actual)
{
    if (!_act_worth_targeting(agent, m))
        return false;

    if (mons_aligned(&agent, &m))
        return false;

    return actual || agent.can_see(m);
}

vector<coord_def> plasma_beam_paths(coord_def source, const vector<coord_def> &targets)
{
    set<coord_def> intermediates;
    for (coord_def c : targets)
    {
        // any monster along the path could get hit, so we need to add them
        // for targeter warnings
        ray_def ray;
        if (!find_ray(source, c, ray, opc_solid))
            continue;

        while (ray.advance())
        {
            intermediates.insert(ray.pos());

            if (ray.pos() == c)
                break;
        }
    }

    vector<coord_def> paths;
    for (coord_def c : intermediates)
        paths.push_back(c);
    return paths;
}

vector<coord_def> plasma_beam_targets(const actor &agent, int pow, bool actual)
{
    const int range = spell_range(SPELL_PLASMA_BEAM, pow);
    int maxdist = 0;
    vector<actor*> target_actors;
    vector <coord_def> targets;
    coord_def source = agent.pos();

    // find the "actual" range of the spell
    for (monster_near_iterator mi(source, LOS_SOLID_SEE); mi; ++mi)
    {
        if (!_plasma_targetable(agent, **mi, actual))
            continue;

        int dist = grid_distance(source, mi->pos());
        if (dist > maxdist && dist <= range)
            maxdist = dist;
    }

    if (agent.is_monster()
        && !agent.as_monster()->wont_attack()
        && cell_see_cell(source, you.pos(), LOS_SOLID_SEE)
        && (actual || agent.can_see(you)))
    {
        int dist = grid_distance(source, you.pos());
        if (dist > maxdist && dist <= range)
        {
            maxdist = dist;
            target_actors.push_back(&you);
        }
    }

    // nothing in range
    if (maxdist == 0)
        return targets;

    for (monster_near_iterator mi(source, LOS_SOLID_SEE); mi; ++mi)
    {
        // look only at the maximum found range
        int dist = grid_distance(source, mi->pos());
        if (dist != maxdist)
            continue;

        if (!_plasma_targetable(agent, **mi, actual))
            continue;

        target_actors.push_back(*mi);
    }

    for (actor *a : target_actors)
        targets.push_back(a->pos());
    return targets;
}

static ai_action::goodness _fire_plasma_beam_at(const actor &agent, int pow,
                                                coord_def target, bool tracer)
{
    int range = grid_distance(agent.pos(), target);
    const bool mon = agent.is_monster();

    // lightning beam
    bolt beam;
    beam.range        = range;
    beam.name         = "plasma beam";
    beam.source       = agent.pos();
    beam.target       = target;
    beam.set_agent(&agent);
    beam.attitude     = mon ? mons_attitude(*agent.as_monster()) : ATT_FRIENDLY;
    beam.origin_spell = SPELL_PLASMA_BEAM;
    beam.draw_delay   = 5;
    beam.foe_ratio    = 80; // default
    beam.is_tracer    = tracer;
    zappy(ZAP_PLASMA_LIGHTNING, pow, mon, beam);
    beam.fire();
    const ai_action::goodness fire_good = beam.good_to_fire();

    // fire beam
    beam.flavour = BEAM_FIRE;
    beam.name    = "fiery plasma";
    beam.glyph   = dchar_glyph(DCHAR_FIRED_ZAP);
    zappy(ZAP_PLASMA, pow, mon, beam);
    beam.fire();
    const ai_action::goodness light_good = beam.good_to_fire();

    if (fire_good == ai_action::bad() || light_good == ai_action::bad())
        return ai_action::bad();
    if (light_good == ai_action::good() || light_good == ai_action::good())
        return ai_action::good();
    return ai_action::neutral();
}

bool mons_should_fire_plasma(int pow, const actor &agent)
{
    vector<coord_def> targets = plasma_beam_targets(agent, pow, false);
    bool ever_good = false;
    for (auto target : targets)
    {
        const ai_action::goodness result = _fire_plasma_beam_at(agent, pow, target, true);
        if (result == ai_action::bad())
            return false; // be very careful!
        if (result == ai_action::good())
            ever_good = true;
    }
    return ever_good;
}

spret cast_plasma_beam(int pow, const actor &agent, bool fail)
{
    if (agent.is_player())
    {
        vector<coord_def> known_targs = plasma_beam_targets(agent, pow, false);
        if (warn_about_bad_targets(SPELL_PLASMA_BEAM,
                                    plasma_beam_paths(you.pos(), known_targs),
            [](const monster& m) { return m.res_fire() == 3 && m.res_elec() == 3 ;}))
        {
            return spret::abort;
        }
    }

    fail_check();

    vector<coord_def> targets = plasma_beam_targets(agent, pow, true);

    if (targets.empty())
    {
        if (you.can_see(agent))
            canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }
    auto target = *random_iterator(targets);
    _fire_plasma_beam_at(agent, pow, target, false);
    return spret::success;
}

static bool _elec_not_immune(const actor *act)
{
    return act->res_elec() < 3 && !god_protects(act->as_monster());
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

static void _set_thunderbolt_last_aim(actor *caster, coord_def aim)
{
    int &last_turn = caster->props[THUNDERBOLT_LAST_KEY].get_int();
    coord_def &last_aim = caster->props[THUNDERBOLT_AIM_KEY].get_coord();

    last_turn = you.num_turns;
    last_aim = aim;
}

dice_def thunderbolt_damage(int power, int arc)
{
    const int &charges = you.props[THUNDERBOLT_CHARGES_KEY].get_int();
    ASSERT(charges <= LIGHTNING_MAX_CHARGE);

    int charge_boost = 0;
    if (in_bounds(get_thunderbolt_last_aim(&you)))
        charge_boost = charges;

    const int dice = div_rand_round(
        (spell_mana(SPELL_THUNDERBOLT, false) + charge_boost)
            * LIGHTNING_CHARGE_MULT,
        LIGHTNING_CHARGE_MULT);
    return dice_def(dice, div_rand_round(45 + power / 4, arc + 2));
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

    bolt beam;
    beam.name              = "thunderbolt";
    beam.aux_source        = "lightning rod";
    beam.origin_spell      = SPELL_THUNDERBOLT;
    beam.flavour           = BEAM_ELECTRICITY;
    beam.glyph             = dchar_glyph(DCHAR_FIRED_BURST);
    beam.colour            = LIGHTCYAN;
    beam.range             = 1;
    beam.hit               = AUTOMATIC_HIT;
    beam.ac_rule           = ac_type::half;
    beam.loudness          = spell_effect_noise(SPELL_THUNDERBOLT);
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

        animation_delay(200, Options.reduce_animations);
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
        beam.damage = thunderbolt_damage(pow, arc);
        beam.fire();
    }

    noisy(spell_effect_noise(SPELL_THUNDERBOLT), caster->pos());

    _set_thunderbolt_last_aim(caster, aim);

    if (charges < LIGHTNING_MAX_CHARGE)
        charges++;

    return spret::success;
}

// Find an enemy who might suffer from Awaken Forest.
// (Search not just for enemies adjacent to trees, but also within one tile of a
// tree, since that usually implies that the trees are close enough to constrain
// future movement meaningfully.)
actor* forest_near_enemy(const actor *mon)
{
    const coord_def pos = mon->pos();

    for (radius_iterator ri(pos, LOS_NO_TRANS); ri; ++ri)
    {
        actor* foe = actor_at(*ri);
        if (!foe || mons_aligned(foe, mon))
            continue;

        for (distance_iterator di(*ri, false, true, 2); di; ++di)
            if (feat_is_tree(env.grid(*di)) && cell_see_cell(*di, *ri, LOS_NO_TRANS))
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

                if (!apply_chunked_AC(1, foe->evasion(false, mon)))
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

static bool _can_be_dazzled(const actor *victim)
{
    return victim->can_be_dazzled();
}

bool dazzle_target(actor *victim, const actor *agent, int pow)
{
    if (!_can_be_dazzled(victim))
        return false;

    if (victim->is_monster())
    {
        auto mons = victim->as_monster();
        const int numerator = dazzle_chance_numerator(mons->get_hit_dice());
        if (x_chance_in_y(numerator, dazzle_chance_denom(pow)))
        {
            mons->add_ench(mon_enchant(ENCH_BLIND, 1, agent,
                        random_range(4, 8) * BASELINE_DELAY));
            return true;
        }
    }
    else
    {
        // Dazzling player: similar logic to monster dazzling but it's maybe
        // not the best way to handle this. Dividing XL by 2 so there is still
        // decent chance at XL27 that the player can be affected.
        const int numerator = dazzle_chance_numerator(you.experience_level / 2);
        if (you.can_be_dazzled()
            && x_chance_in_y(numerator, dazzle_chance_denom(pow)))
        {
            blind_player(random_range(6, 12));
            return true;
        }
    }

    return false;
}

spret cast_dazzling_flash(const actor *caster, int pow, bool fail, bool tracer)
{
    int range = spell_range(SPELL_DAZZLING_FLASH, pow, caster->is_player());
    auto vulnerable = [caster](const actor *act) -> bool
    {
        // No fedhas checks needed, plants can't be dazzled.
        if (!_can_be_dazzled(act))
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

            // XX: monster tracer will need to check for player at
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
        auto hitfunc = find_spell_targeter(SPELL_DAZZLING_FLASH, pow, range);

        // [eb] the simulationist in me wants to use LOS_DEFAULT
        // and let this blind through glass
        if (stop_attack_prompt(*hitfunc, "dazzle", vulnerable))
            return spret::abort;
    }

    fail_check();

    bolt beam;
    beam.name = "energy";
    beam.flavour = BEAM_VISUAL;
    beam.origin_spell = SPELL_DAZZLING_FLASH;
    beam.set_agent(caster);
    beam.colour = WHITE;
    beam.glyph = dchar_glyph(DCHAR_EXPLOSION);
    beam.range = range;
    beam.ex_size = range;
    beam.is_explosion = true;
    beam.source = caster->pos();
    beam.target = caster->pos();
    beam.hit = AUTOMATIC_HIT;
    beam.loudness = 0;
    beam.explode(true, true);

    for (radius_iterator ri(caster->pos(), range, C_SQUARE, LOS_SOLID_SEE, true);
         ri; ++ri)
    {
        actor* victim = actor_at(*ri);
        if (victim && vulnerable(victim) && dazzle_target(victim, caster, pow))
        {
            if (victim->is_monster())
                simple_monster_message(*victim->as_monster(), " is dazzled.");
            // Player already got a message from blind_player
            // XX: Should it be a slightly different message? Show something if resisted?
        }
    }

    return spret::success;
}

static bool _toxic_can_affect(const actor *act)
{
    // currently monsters are still immune at rPois 1
    return act->res_poison() < (act->is_player() ? 3 : 1);
}

spret cast_toxic_radiance(actor *agent, int pow, bool fail, bool mon_tracer)
{
    // XXX: This must come before the player check, due to Marionette.
    if (mon_tracer)
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
    else if (agent->is_player())
    {
        targeter_radius hitfunc(&you, LOS_NO_TRANS);
        if (stop_attack_prompt(hitfunc, "poison", _toxic_can_affect))
            return spret::abort;

        fail_check();

        if (!you.duration[DUR_TOXIC_RADIANCE])
            mpr("You begin to radiate toxic energy.");
        else
            mpr("Your toxic radiance grows in intensity.");

        you.increase_duration(DUR_TOXIC_RADIANCE,
                              2 + random2(1 + div_rand_round(pow, 25)), 15);
        you.props[TOXIC_RADIANCE_POWER_KEY].get_int() = pow;
        toxic_radiance_effect(&you, 10, true);

        flash_view_delay(UA_PLAYER, GREEN, 300, &hitfunc);

        return spret::success;
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
        pow = you.props[TOXIC_RADIANCE_POWER_KEY].get_int();
    else
        pow = agent->as_monster()->get_hit_dice() * 12;

    for (actor_near_iterator ai(agent->pos(), LOS_NO_TRANS); ai; ++ai)
    {
        if (!_toxic_can_affect(*ai))
            continue;

        // Monsters can skip hurting friendlies
        if (agent->is_monster() && mons_aligned(agent, *ai))
            continue;

        int dam = roll_dice(1, 1 + div_rand_round(pow, 20))
                            * div_rand_round(mult, BASELINE_DELAY);
        dam = resist_adjust_damage(*ai, BEAM_POISON, dam);

        if (ai->is_player())
        {
            // We're affected only if we're not the agent.
            if (!agent->is_player())
            {
                ouch(dam, KILLED_BY_BEAM, agent->mid,
                    "by Olgreb's Toxic Radiance", true,
                    agent->as_monster()->name(DESC_A).c_str());

                int poison = roll_dice(2, 3 + div_rand_round(pow, 24));

                // rPois = 1/3 poison each tick instead of a 1/3 chance
                // of full poison each tick. Looks smoother.
                if (you.res_poison() > 0)
                    poison /= 3;

                poison_player(poison, agent->name(DESC_A),
                              "toxic radiance", true);
            }
        }
        else
        {
            god_conduct_trigger conducts[3];

            // Only trigger conducts on the turn the player casts the spell
            // (see PR #999).
            if (on_cast && agent->is_player())
                set_attack_conducts(conducts, *ai->as_monster());

            if (agent->is_player())
            {
                _player_hurt_monster(*ai->as_monster(), dam, BEAM_POISON,
                                     false, true);
            }
            else
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

static void _setup_unravelling(bolt &beam, int pow, coord_def target)
{
    zappy(ZAP_UNRAVELLING, pow, false, beam);
    beam.set_agent(&you);
    beam.source = target;
    beam.target = target;
    beam.ex_size = 1;
}

spret cast_unravelling(coord_def target, int pow, bool fail)
{
    if (cell_is_solid(target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    const actor* victim = actor_at(target);
    if ((!victim || !you.can_see(*victim))
        && !yesno("You can't see anything there. Cast anyway?", false, 'n'))
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }

    if (victim && (victim->is_player() && !player_is_debuffable()
                   || victim->is_monster() && you.can_see(*victim)
                      && !monster_can_be_unravelled(*victim->as_monster())))
    {
        mprf("%s %s no magical effects to unravel.",
             victim->name(DESC_THE).c_str(),
             victim->conj_verb("have").c_str());
        return spret::abort;
    }

    targeter_unravelling hitfunc;
    hitfunc.set_aim(target);
    auto vulnerable = [](const actor *act) -> bool
    {
        return !god_protects(act->as_monster());
    };

    if (hitfunc.is_affected(you.pos()) >= AFF_MAYBE
        && !yesno("The unravelling is likely to hit you. Continue anyway?",
                  false, 'n'))
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }

    if (stop_attack_prompt(hitfunc, "unravel", vulnerable))
        return spret::abort;

    fail_check();

    if (!victim)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return spret::success;
    }

    bolt beam;
    _setup_unravelling(beam, pow, target);
    beam.fire();

    return spret::success;
}

// XXX: this should take a monster_info.
string mons_inner_flame_immune_reason(const monster *mons)
{
    if (!mons || !you.can_see(*mons))
        return "You can't see anything there.";

    if (mons->has_ench(ENCH_INNER_FLAME))
    {
        return make_stringf("%s is already burning with an inner flame!",
                            mons->name(DESC_THE).c_str());
    }

    if (mons->willpower() == WILL_INVULN)
    {
        return make_stringf("%s has infinite will and cannot be affected.",
                            mons->name(DESC_THE).c_str());
    }

    return "";
}

spret cast_inner_flame(coord_def target, int pow, bool fail)
{
    if (cell_is_solid(target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    const monster* mons = monster_at(target);
    const string immune_reason = mons_inner_flame_immune_reason(mons);
    if (!immune_reason.empty())
    {
        mpr(immune_reason);
        return spret::abort;
    }

    bolt beam;
    beam.source = mons->pos();
    beam.target = mons->pos();
    beam.set_agent(&you);

    return zapping(ZAP_INNER_FLAME, pow, beam, false, nullptr, fail);
}

int get_mercury_weaken_chance(int victim_hd, int pow)
{
    return max(0, 100 - max(0, (victim_hd * 12 - pow * 3 / 2 - 17) * 115 / 100));
}

spret cast_mercury_vapours(int pow, const coord_def target, bool fail)
{
    if (cell_is_solid(target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    monster* mons = monster_at(target);
    if (mons && you.can_see(*mons) && !god_protects(&you, *mons)
        && mons->res_poison() <= 0
        && stop_attack_prompt(mons, false, you.pos()))
    {
        return spret::abort;
    }
    else if (you.pos() == target && you.res_poison() <= 0
             && !yesno("Really target yourself?", false, 'n'))
    {
        return spret::abort;
    }

    fail_check();

    if (mons && you.can_see(*mons))
        mprf("Fumes of mercury billow around %s!", mons->name(DESC_THE).c_str());
    else if (target == you.pos())
        mpr("Fumes of mercury billow around yourself!");
    else
        mpr("Fumes of mercury billow through the air!");

    // Attempt to poison the central monster, if there is one.
    if (mons && mons->res_poison() <= 0 && !god_protects(&you, *mons))
    {
        // Be a little more generous with poisoning unpoisoned monsters.
        int amount = max(1, div_rand_round(pow, 25));
        if (!mons->has_ench(ENCH_POISON))
            ++amount;

        poison_monster(mons, &you, amount);

        behaviour_event(mons, ME_WHACK, &you);
        if (mons->alive())
            you.pet_target = mons->mindex();
    }
    // Trying to cast on self - presumably for the AoE weak.
    else if (you.pos() == target && you.res_poison() <= 0)
        you.poison(&you, roll_dice(2, 6));

    // Now attempt to weaken all monsters adjacent to the target
    for (adjacent_iterator ai(target, false); ai; ++ai)
    {
        actor* actor = actor_at(*ai);
        if (!actor || god_protects(&you, actor->as_monster()))
            continue;

        int chance = get_mercury_weaken_chance(actor->get_hit_dice(), pow);

        // Adjacent targets have moderately less chance of weakness than the
        // central target.
        if (*ai != target)
            chance = chance * 2 / 3;

        if (actor && x_chance_in_y(chance, 100))
            actor->weaken(&you, 5);
    }

    return spret::success;
}

static bool _prep_flame_wave(bolt &beam, int pow, int lvl, bool skip_tracer = false)
{
    zappy(ZAP_FLAME_WAVE, pow, false, beam);
    beam.set_agent(&you);
    beam.origin_spell = SPELL_FLAME_WAVE;
    beam.ex_size       = lvl;
    beam.source = beam.target = you.pos();

    if (!skip_tracer)
    {
        bolt tracer_beam = beam;
        tracer_beam.is_tracer = true;
        tracer_beam.explode(false, true);
        return !tracer_beam.beam_cancelled;
    }

    return true;
}

spret cast_flame_wave(int pow, bool fail)
{
    bolt beam;
    if (!_prep_flame_wave(beam, pow, 1))
        return spret::abort;

    fail_check();

    you.props[FLAME_WAVE_POWER_KEY].get_int() = pow;
    start_channelling_spell(SPELL_FLAME_WAVE, "intensify the flame waves");

    return spret::success;
}

void handle_flame_wave(int lvl)
{
    const int pow = you.props[FLAME_WAVE_POWER_KEY].get_int();
    bolt beam;
    if (!_prep_flame_wave(beam, pow, lvl, lvl == 1))
    {
        stop_channelling_spells();
        return;
    }

    beam.apply_beam_conducts();
    beam.refine_for_explosion();
    beam.explode(true, true);
    trigger_battlesphere(&you);

    if (lvl > 1)
    {
        pay_mp(1);
        finalize_mp_cost();
    }

    if (lvl >= spell_range(SPELL_FLAME_WAVE, pow))
    {
        mpr("Your wave of flame reaches its maximum intensity and dissipates.");
        stop_channelling_spells(true);
    }
}

spret cast_searing_ray(actor& agent, int pow, bolt &beam, bool fail)
{
    if (agent.is_player())
    {
        if (!player_tracer(ZAP_SEARING_RAY, pow, beam))
            return spret::abort;

        fail_check();
    }
    else
    {
        zappy(ZAP_SEARING_RAY, pow, false, beam);
        beam.fire();
    }

    actor* targ = actor_at(beam.target);
    agent.props[SEARING_RAY_AIM_SPOT_KEY].get_bool() = beam.aimed_at_spot
                                                        || !targ;
    agent.props[SEARING_RAY_TARGET_KEY].get_coord() = beam.target;
    agent.props[SEARING_RAY_POWER_KEY].get_int() = pow;

    if (targ)
        agent.props[SEARING_RAY_MID_KEY].get_int() = targ->mid;
    else
        agent.props.erase(SEARING_RAY_MID_KEY);

    if (agent.is_player())
        start_channelling_spell(SPELL_SEARING_RAY, "maintain the ray");
    else
    {
        int dur = min(3 + pow / 60, 5);
        mon_enchant ench(ENCH_CHANNEL_SEARING_RAY, 0, &agent, dur);
        agent.as_monster()->add_ench(ench);
    }

    return spret::success;
}

// Returns true if the searing ray still fired this turn
bool handle_searing_ray(actor& agent, int turn)
{
    const zap_type zap = zap_type(ZAP_SEARING_RAY);
    const int pow = agent.props[SEARING_RAY_POWER_KEY].get_int();

    if (!agent.props[SEARING_RAY_AIM_SPOT_KEY].get_bool())
    {
        actor* targ = actor_by_mid(agent.props[SEARING_RAY_MID_KEY].get_int());
        // homing targeting, save the target location in case it dies or
        // disappears
        if (targ && targ->alive() && agent.can_see(*targ))
            agent.props[SEARING_RAY_TARGET_KEY].get_coord() = targ->pos();
        else
            agent.props[SEARING_RAY_AIM_SPOT_KEY] = true;
    }

    bolt beam;
    beam.thrower = agent.is_player() ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
    beam.range   = calc_spell_range(SPELL_SEARING_RAY, pow);
    beam.source  = agent.pos();
    beam.target  = agent.props[SEARING_RAY_TARGET_KEY].get_coord();

    // If friendlies have moved into the beam path, give a chance to abort
    if (agent.is_player() && !player_tracer(zap, pow, beam))
    {
        stop_channelling_spells();
        return false;
    }

    zappy(zap, pow, false, beam);

    // If a channelling monster no longer has a good shot, cancel it and let
    // it do something else.
    if (agent.is_monster())
    {
        fire_tracer(agent.as_monster(), beam);
        if (!mons_should_fire(beam))
        {
            simple_monster_message(*agent.as_monster(), " stops channelling.");
            agent.as_monster()->del_ench(ENCH_CHANNEL_SEARING_RAY);
            return false;
        }
    }

    beam.fire();
    trigger_battlesphere(&agent);

    if (agent.is_player())
    {
        pay_mp(1);
        finalize_mp_cost();

        if (turn > 3)
        {
            mpr("You finish channelling your searing ray.");
            stop_channelling_spells(true);
        }
    }
    else
    {
        monster* mons = agent.as_monster();

        mon_enchant me = mons->get_ench(ENCH_CHANNEL_SEARING_RAY);
        mons->lose_ench_duration(me, 1);
        if (!mons->has_ench(ENCH_CHANNEL_SEARING_RAY))
            simple_monster_message(*mons, " finishes channelling their searing ray.");
    }

    return true;
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

            // Why doesn't this set beam.draw_delay instead?
            if (!Options.reduce_animations)
                scaled_delay(25); // beam.draw should trigger a redraw already?
        }

        if (Options.reduce_animations)
            animation_delay(100, true);
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
        // If the player has already answered yes to shooting at at least one
        // friendly, don't make them answer the question again for friendlies
        // in every other direction.
        else if (!tracer && beam.friend_info.dont_stop)
            break;

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

static string _get_jinxsprite_message(const monster& victim)
{
    string msg;

    // This is *extremely* ripe ground for varied comical messages. Tie a snake's tail
    // in a knot, unravel a mummy's bandages, tell Edmund he'll never be as scary as his brother.

    if (get_mon_shape(victim.type) == MON_SHAPE_SNAKE && coinflip())
    {
        msg = make_stringf("ties %s tail in a knot.",
                        victim.name(DESC_ITS).c_str());
    }
    else if (mon_shape_is_humanoid(get_mon_shape(victim.type)) && coinflip())
    {
        if (victim.inv[MSLOT_WEAPON] != NON_ITEM && coinflip())
        {
            return make_stringf("bonks %s with %s %s.",
                                victim.name(DESC_THE).c_str(),
                                victim.pronoun(PRONOUN_POSSESSIVE).c_str(),
                                env.item[victim.inv[MSLOT_WEAPON]].name(DESC_PLAIN).c_str());
        }
        else if (one_chance_in(3))
        {
            return make_stringf("insults %s ancestors.",
                                victim.name(DESC_ITS).c_str());
        }
        else
        {
            return make_stringf("doodles on %s face.",
                                victim.name(DESC_ITS).c_str());
        }
    }
    else
    {
        switch (random2(2))
        {
            case 0:
            return make_stringf("makes %s trip over %s own %s.",
                                victim.name(DESC_THE).c_str(),
                                victim.pronoun(PRONOUN_POSSESSIVE).c_str(),
                                victim.foot_name(true).c_str());

            case 1:
            return make_stringf("smacks %s with %s own %s.",
                                victim.name(DESC_THE).c_str(),
                                victim.pronoun(PRONOUN_POSSESSIVE).c_str(),
                                victim.hand_name(false).c_str());

            case 2:
            return make_stringf("does a pirouette on top of %s.",
                                victim.name(DESC_THE).c_str());
        }
    }

    return msg;
}

void attempt_jinxbite_hit(actor& victim)
{
    if (!victim.is_monster() || victim.wont_attack())
        return;

    // Test victim will to see if we should trigger.
    // (Return silently if we don't pass the check.)
    const int pow = calc_spell_power(SPELL_JINXBITE);
    if (victim.check_willpower(&you, pow) > 0)
        return;

    // Show brief animation when we successfully trigger. (Helps sell to the
    // player that this is a Will check, also.)
    flash_tile(victim.pos(), LIGHTBLUE);

    // XXX TODO: move this out and display it
    const int dmg = roll_dice(2, 2 + div_rand_round(pow, 25));

    monster* mons = victim.as_monster();
    const int drain_dur = random_range(3 * BASELINE_DELAY, 5 * BASELINE_DELAY);
    mons->add_ench(mon_enchant(ENCH_DRAINED, 2, &you, drain_dur));

    if (you.can_see(victim))
    {
        mprf("A giggling sprite leaps out and %s",
                _get_jinxsprite_message(*mons).c_str());
    }

    _player_hurt_monster(*mons, dmg, BEAM_MAGIC);

    // Drain some duration every time we spawn a sprite.
    you.duration[DUR_JINXBITE] -= 40;
    if (you.duration[DUR_JINXBITE] < 1)
        you.duration[DUR_JINXBITE] = 1;
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
    beam.thrower = (foxfire && foxfire->summoner == MID_PLAYER) ? KILL_YOU :
                        (foxfire)   ? KILL_MON
                                    : KILL_NON_ACTOR;
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
    beam.draw_delay = 0;
    beam.redraw_per_cell = false;
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
        return mon && !mon->is_firewood()
            && !god_protects(*mon)
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

    if (Options.use_animations & UA_BEAM)
        animation_delay(200, true);

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
                 && (always_shoot_through_monster(&you, *act->as_monster())
                     || god_protects(*act->as_monster())));
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
        if (ai->pos().distance_from(you.pos()) > range || ai->pos() == you.pos())
            continue;

        act_list.push_back(*ai);
    }

    far_to_near_sorter sorter = { source };
    sort(act_list.begin(), act_list.end(), sorter);

    for (actor *act : act_list)
        if (cell_see_cell(source, act->pos(), LOS_SOLID_SEE)) // sanity check vs dispersal
            _imb_actor(act, pow, source);

    return spret::success;
}

dice_def toxic_bog_damage()
{
    return dice_def(4, 6);
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

    if (player && you.duration[DUR_NOXIOUS_BOG])
        return;

    actor *oppressor = nullptr;

    for (map_marker *marker : env.markers.get_markers_at(act->pos()))
    {
        if (marker->get_type() == MAT_TERRAIN_CHANGE)
        {
            map_terrain_change_marker* tmarker =
                    dynamic_cast<map_terrain_change_marker*>(marker);
            const auto ct = tmarker->change_type;
            if (ct == TERRAIN_CHANGE_BOG || ct == TERRAIN_CHANGE_FLOOD)
                oppressor = actor_by_mid(tmarker->mon_num);
        }
    }

    const int base_damage = toxic_bog_damage().roll();
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
                  KILLED_BY_BEAM, "", "toxic bog");
    }
}

vector<coord_def> find_ramparts_walls()
{
    vector<coord_def> wall_locs;
    for (radius_iterator ri(you.pos(),
            spell_range(SPELL_FROZEN_RAMPARTS, -1), C_SQUARE,
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
 * @param pow    The spell power.
 * @param fail   Did this spell miscast? If true, abort the cast.
 * @return       spret::abort if no affectable walls could be found,
 *               spret::fail if one could be found but we miscast, and
 *               spret::success if the spell was successfully cast.
*/
spret cast_frozen_ramparts(int pow, bool fail)
{
    vector<coord_def> wall_locs = find_ramparts_walls();

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
    you.props[FROZEN_RAMPARTS_POWER_KEY].get_int() = pow;

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
                spell_range(SPELL_FROZEN_RAMPARTS, -1)); di; di++)
    {
        env.pgrid(*di) &= ~FPROP_ICY;
        env.map_knowledge(*di).flags &= ~MAP_ICY;
    }

    you.props.erase(FROZEN_RAMPARTS_KEY);

    env.level_state &= ~LSTATE_ICY_WALL;
}

dice_def ramparts_damage(int pow, bool random)
{
    int size = 1 + pow * 3 / 10;
    if (random)
        size = 1 + div_rand_round(pow * 3, 10);
    return dice_def(1, size);
}

static bool _maxwells_target_check(const monster &m)
{
    return _act_worth_targeting(you, m)
            && !m.wont_attack();
}

// returns the closest target to the player, choosing randomly if there are more
// than one (see `fair` argument to distance_iterator).
static monster* _find_maxwells_target(bool tracer)
{
    for (distance_iterator di(you.pos(), !tracer, true, LOS_RADIUS); di; ++di)
    {
        monster *mon = monster_at(*di);
        if (mon && _maxwells_target_check(*mon)
            && (!tracer || you.can_see(*mon)))
        {
            return mon;
        }
    }

    return nullptr;
}

// find all possible targets at the closest distance; used for targeting
vector<monster *> find_maxwells_possibles()
{
    vector<monster *> result;
    monster *seed = _find_maxwells_target(true);
    if (!seed)
        return result;

    const int distance = grid_distance(you.pos(), seed->pos());
    for (distance_iterator di(you.pos(), true, true, distance); di; ++di)
    {
        monster *mon = monster_at(*di);
        if (mon && _maxwells_target_check(*mon) && you.can_see(*mon))
            result.push_back(mon);
    }
    return result;
}

spret cast_maxwells_coupling(int pow, bool fail, bool tracer)
{
    monster* const mon = _find_maxwells_target(tracer);

    if (tracer)
    {
        if (!mon || !you.can_see(*mon))
            return spret::abort;
        else
            return spret::success;
    }

    fail_check();

    mpr("You begin accumulating electric charge.");

    you.props[COUPLING_TIME_KEY] =
       you.elapsed_time + (30 + div_rand_round(random2((200 - pow) * 40), 200));

    start_channelling_spell(SPELL_MAXWELLS_COUPLING, "continue charging", false);

    return spret::success;
}

static void _discharge_maxwells_coupling()
{
    monster* const mon = _find_maxwells_target(false);

    if (!mon)
    {
        mpr("Your charge dissipates without a target.");
        return;
    }

    targeter_radius hitfunc(&you, LOS_NO_TRANS);
    flash_view_delay(UA_PLAYER, LIGHTCYAN, 100, &hitfunc);

    god_conduct_trigger conducts[3];
    set_attack_conducts(conducts, *mon, you.can_see(*mon));

    string attack_punctuation = attack_strength_punctuation(mon->hit_points);

    if (mon->type == MONS_ROYAL_JELLY && !mon->is_summoned())
    {
        // need to do this here, because react_to_damage is never called
        mprf("A cloud of jellies burst out of %s as the current"
             " ripples through it%s", mon->name(DESC_THE).c_str(), attack_punctuation.c_str());
        trj_spawn_fineff::schedule(&you, mon, mon->pos(), mon->hit_points);
    }
    else
        mprf("The electricity discharges through %s%s", mon->name(DESC_THE).c_str(), attack_punctuation.c_str());

    // XX the messaging and corpse logic here would be better handled in
    // monster_die, so that various special cases (e.g. dancing weapons in
    // the abyss) can get clear but non-redundant handling
    const coord_def pos = mon->pos();
    const bool goldify = mons_will_goldify(*mon);
    if (goldify)
        simple_monster_message(*mon, " vaporises and condenses as gold!");
    else
    {
        simple_monster_message(*mon, " vaporises in an electric haze!");
        big_cloud(CLOUD_ELECTRICITY, &you, pos, random_range(4, 8), random_range(8, 12));
    }

    item_def* corpse = monster_die(*mon, KILL_YOU,
                                    actor_to_death_source(&you));
    if (corpse && !goldify)
        destroy_item(corpse->index());


    noisy(spell_effect_noise(SPELL_MAXWELLS_COUPLING), pos, you.mid);
}

void handle_maxwells_coupling()
{
    const int discharge_time = you.props[COUPLING_TIME_KEY].get_int();

    if (you.elapsed_time >= discharge_time)
    {
        you.props.erase(COUPLING_TIME_KEY);
        _discharge_maxwells_coupling();
        stop_channelling_spells(true);
        return;
    }

    mpr("You feel charge building up...");
}

vector<coord_def> find_bog_locations(const coord_def &center, int pow)
{
    vector<coord_def> bog_locs;
    const int radius = spell_range(SPELL_NOXIOUS_BOG, pow);

    for (radius_iterator ri(center, radius, C_SQUARE, LOS_NO_TRANS); ri; ri++)
    {
        if (!feat_has_solid_floor(env.grid(*ri)))
            continue;

        // If a candidate cell is next to more than two solid features, we can't
        // bog it. Cells we can't currently see are also considered solid,
        // regardless of what the cell contains. Don't want to leak information
        // about out-of-los cells.
        int walls = 0;
        for (adjacent_iterator ai(*ri); ai; ai++)
        {
            if (!you.see_cell(*ai) || feat_is_solid(env.grid(*ai)))
                walls++;
        }
        if (walls <= 2)
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

    const int turns = 4 + random2(2 + div_rand_round(pow, 10));
    you.increase_duration(DUR_NOXIOUS_BOG, turns);

    for (auto pos : bog_locs)
    {
        temp_change_terrain(pos, DNGN_TOXIC_BOG, turns * BASELINE_DELAY,
                TERRAIN_CHANGE_BOG, MID_PLAYER);
    }

    flash_view_delay(UA_PLAYER, LIGHTGREEN, 100);
    mpr("You spew toxic sludge!");

    return spret::success;
}

int siphon_essence_range() { return 2; }

bool siphon_essence_affects(const monster &m)
{
    return !m.wont_attack()
        && !m.res_torment()
        && !m.is_peripheral();
}

dice_def boulder_damage(int pow, bool random)
{
    if (random)
        return dice_def(2, 4 + div_rand_round(pow, 10));
    return dice_def(2, 4 + pow / 10);
}

void do_boulder_impact(monster& boulder, actor& victim, bool quiet)
{
    if (you.can_see(boulder))
    {
        if (!quiet)
        {
            mprf("%s barrels into %s!",
                    boulder.name(DESC_THE).c_str(),
                    victim.name(DESC_THE).c_str());
        }
    }

    const int pow = boulder.props[BOULDER_POWER_KEY].get_int();
    int dam = boulder_damage(pow, true).roll();
    dam = victim.apply_ac(dam);

    if (victim.is_player())
        ouch(dam, KILLED_BY_ROLLING, boulder.mid);
    else
        _player_hurt_monster(*victim.as_monster(), dam, BEAM_MISSILE);
}

dice_def electrolunge_damage(int pow)
{
    return dice_def(2, pow / 6);
}

int get_warp_space_chance(int pow)
{
    return min(90, 35 + pow);
}

dice_def default_collision_damage(int pow, bool random)
{
    return dice_def(2, random ? 1 + div_rand_round(pow, 10) : 1 + pow / 10);
}

string describe_collision_dam(dice_def dice)
{
    return make_stringf("%dd%d / collision", dice.num, dice.size);
}

vector<coord_def> get_magnavolt_targets()
{
    vector<coord_def> targets;
    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        // Don't zap at ourselves, even if we're standing in a cloud
        if (*ri == you.pos())
            continue;

        if (monster* mon = monster_at(*ri))
        {
            if (mon->has_ench(ENCH_MAGNETISED))
                targets.push_back(mon->pos());
        }

        if (cloud_type_at(*ri) == CLOUD_MAGNETISED_DUST)
            targets.push_back(*ri);
    }

    return targets;
}

vector<coord_def> get_magnavolt_beam_paths(vector<coord_def>& targets)
{
    vector<coord_def> paths;
    for (coord_def target : targets)
    {
        ray_def ray;
        if (find_ray(you.pos(), target, ray, opc_solid))
        {
            while (ray.advance() && ray.pos() != target)
                paths.push_back(ray.pos());
        }
    }

    return paths;
}

spret cast_magnavolt(coord_def target, int pow, bool fail)
{
    // Calculate paths for all existing magnetized enemies and the one we plan
    // to magnetize now.
    vector<coord_def> targets = get_magnavolt_targets();
    // Add the new target only if it isn't already here, or we'll zap it twice.
    if (find(targets.begin(), targets.end(), target) == targets.end())
        targets.push_back(target);
    vector<coord_def> paths = get_magnavolt_beam_paths(targets);

    if (warn_about_bad_targets(SPELL_MAGNAVOLT, paths,
            [](const monster& m) { return m.res_elec() == 3 ;}))
    {
        return spret::abort;
    }

    fail_check();

    // First apply the debuff to the targeted enemy.
    monster* mon = monster_at(target);

    if (!mon->has_ench(ENCH_MAGNETISED))
        mprf("Magnetic shrapnel attaches itself to %s.", mon->name(DESC_THE).c_str());

    mon->add_ench(mon_enchant(ENCH_MAGNETISED, 1, &you,
                              random_range(5, 8) * BASELINE_DELAY));

    // Then zap all magnetized enemies.
    mpr("Electricity arcs towards the magnetite!");
    for (unsigned int i = 0; i < targets.size(); ++i)
    {
        bolt volt;
        zappy(ZAP_MAGNAVOLT, pow, false, volt);
        volt.source = you.pos();
        volt.target = targets[i];
        volt.aimed_at_spot = true;
        volt.range = LOS_RADIUS;
        volt.thrower = KILL_YOU_MISSILE;
        volt.fire();
    }

    return spret::success;
}

spret cast_fulsome_fusillade(int pow, bool fail)
{
    targeter_radius hitfunc(&you, LOS_ARENA);
    auto vulnerable = [](const actor *act) -> bool
    {
        return !act->is_player()
               && !god_protects(*act->as_monster());
    };
    if (stop_attack_prompt(hitfunc, "hurl reagents", vulnerable))
        return spret::abort;

    fail_check();

    mpr("You conjure up an array of volatile reagents!");

    you.duration[DUR_FUSILLADE] = 5;
    you.props[FUSILLADE_POWER_KEY] = pow;

    return spret::success;
}

static vector<beam_type> concoction_flavours =
{
    BEAM_FIRE,
    BEAM_COLD,
    BEAM_POISON_ARROW,
    BEAM_ELECTRICITY,
};

static map<beam_type, string> concoction_description =
{
    { BEAM_FIRE, "fiery phlogiston" },
    { BEAM_COLD, "frigid brine" },
    { BEAM_POISON_ARROW, "noxious sulphur" },
    { BEAM_ELECTRICITY, "flickering plasma" },
    { BEAM_MMISSILE, "unstable reaction" },
};

static map<beam_type, colour_t> concoction_colour =
{
    { BEAM_FIRE, RED },
    { BEAM_COLD, BLUE },
    { BEAM_POISON_ARROW, GREEN },
    { BEAM_ELECTRICITY, LIGHTCYAN },
    // This is for the multi-element reaction
    { BEAM_MMISSILE, YELLOW },
};

static vector<pair<beam_type, int>> reaction_effects =
{
    { BEAM_NONE, 200 },
    { BEAM_MALMUTATE, 30 },
    { BEAM_SLOW, 30 },
    { BEAM_WEAKNESS, 30 },
    { BEAM_CONFUSION, 20 },
    { BEAM_ENSNARE, 20 },
    { BEAM_PETRIFY, 10 },
};

static void _show_fusillade_explosion(map<coord_def, beam_type>& hit_map,
                                      vector<coord_def>& exp_map,
                                      bool quick_anim)
{
    if (!(Options.use_animations & UA_BEAM))
        return;

    for (unsigned int j = 0; j < exp_map.size(); ++j)
    {
        const coord_def pos = exp_map[j];
        if (you.see_cell(pos))
        {
            colour_t colour = concoction_colour[hit_map[pos]];
            flash_tile(pos, concoction_colour[hit_map[pos]], 0,
                       colour == YELLOW ? int{TILE_BOLT_IRRADIATE} : 0);
        }
    }

    animation_delay(quick_anim ? 0 : 50, true);
}

static void _do_fusillade_hit(monster* mon, int power, beam_type flavour)
{
    bolt exp;

    zappy(ZAP_FULSOME_FUSILLADE, power, false, exp);

    // Unstable reactions do more damage
    if (flavour == BEAM_MMISSILE)
        exp.damage.num = 5;

    exp.source_id     = MID_PLAYER;
    exp.thrower       = KILL_YOU_MISSILE;
    exp.origin_spell  = SPELL_FULSOME_FUSILLADE;
    exp.target        = mon->pos();
    exp.source        = mon->pos();
    exp.hit_verb      = "engulfs";
    exp.aimed_at_spot = true;
    exp.flavour       = flavour;
    exp.name          = concoction_description[flavour];
    exp.aux_source    = concoction_description[flavour];
    exp.colour        = concoction_colour[flavour];
    exp.fire();

    if (flavour == BEAM_MMISSILE && mon && mon->alive())
    {
        // Do reaction here
        beam_type effect = *random_choose_weighted(reaction_effects);

        switch (flavour)
        {
            // Do nothing
            case BEAM_NONE:
                break;

            case BEAM_ENSNARE:
                ensnare(mon);
                break;

            default:
                enchant_actor_with_flavour(mon, &you, effect, power / 20);
        }
    }
}

static void _calc_fusillade_explosion(coord_def center, beam_type flavour,
                                      map<coord_def, beam_type>& hit_map,
                                      vector<coord_def>& exp_map,
                                      bool quick_anim = false)
{
    for (adjacent_iterator ai(center, false); ai; ++ai)
    {
        if (feat_is_solid(env.grid(*ai)))
            continue;

        exp_map.push_back(*ai);

        // Apply the explosion flavour at all affected tiles, but an unstable
        // reaction at any tile which has already been hit this turn.
        if (hit_map.count(*ai))
            hit_map[*ai] = BEAM_MMISSILE;
        else
            hit_map[*ai] = flavour;
    }

    noisy(15, center, MID_PLAYER);

    _show_fusillade_explosion(hit_map, exp_map, quick_anim);
}

void fire_fusillade()
{
    // The first volley is included in the casting cost. The rest drain additional mp.
    if (you.duration[DUR_FUSILLADE] != 5)
    {
        if (!enough_mp(2, true))
        {
            mpr("Your magical reserves are too exhausted to conjure more reagents.");
            you.duration[DUR_FUSILLADE] = 0;
            return;
        }
        pay_mp(2);
        finalize_mp_cost();
    }

    mpr("Flasks of reagents rain from above!");

    int pow = you.props[FUSILLADE_POWER_KEY].get_int();

    map<coord_def, beam_type> hit_map; // Map of which flavour to apply on which tile
    vector<coord_def> exp_map[3];      // Individual explosions, for animation

    // Determine which monsters we're hitting
    vector<monster*> targs;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mi->wont_attack() && !mi->is_firewood() && you.can_see(**mi)
            && grid_distance(mi->pos(), you.pos()) > 1)
        {
            targs.push_back(*mi);
        }
    }

    shuffle_array(targs);
    shuffle_array(concoction_flavours);

    // Aim up to 3 shots at random valid enemies
    int num_shots = 3;
    for (unsigned int i = 0; i < targs.size() && num_shots > 0; ++i)
    {
        _calc_fusillade_explosion(targs[i]->pos(), concoction_flavours[3 - num_shots],
                                  hit_map, exp_map[3 - num_shots]);
        num_shots--;
    }

    // Animate more quickly if nothing is around
    bool quick_anim = num_shots == 3;

    // If we still have shots left, aim them at random empty spaces
    if (num_shots > 0)
    {
        vector<coord_def> empty_spot;
        for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
        {
            if (grid_distance(you.pos(), *ri) > 1 && !feat_is_solid(env.grid(*ri))
                && !monster_at(*ri))
            {
                empty_spot.push_back(*ri);
            }
        }

        shuffle_array(empty_spot);

        for (unsigned int i = 0; i < empty_spot.size() && num_shots > 0; ++i)
        {
            _calc_fusillade_explosion(empty_spot[i], concoction_flavours[3 - num_shots],
                                      hit_map, exp_map[3 - num_shots], quick_anim);
            num_shots--;
        }
    }

    if (Options.use_animations & UA_BEAM)
        animation_delay(quick_anim ? 50 : 100, false);

    // Finally, actually apply damage to enemies
    map<coord_def, beam_type>::iterator it;
    for (it = hit_map.begin(); it != hit_map.end(); it++)
    {
        monster* mon = monster_at(it->first);
        if (mon)
            _do_fusillade_hit(mon, pow, it->second);
    }

    you.duration[DUR_FUSILLADE] -= 1;

    if (!you.duration[DUR_FUSILLADE])
        mprf(MSGCH_DURATION, "Your rain of reagents ends.");
}

spret cast_grave_claw(actor& caster, coord_def targ, int pow, bool fail)
{
    actor* act = actor_at(targ);

    if (caster.is_player() && act)
    {
        if (stop_attack_prompt(act->as_monster(), false, you.pos()))
            return spret::abort;
    }

    fail_check();

    if (caster.is_player())
    {
        mpr("You unleash the spiteful dead!");
        you.props[GRAVE_CLAW_CHARGES_KEY].get_int()--;
    }

    flash_tile(targ, WHITE);

    bolt beam;
    beam.set_agent(&caster);
    beam.attitude = caster.is_player() ? ATT_FRIENDLY
                                       : mons_attitude(*caster.as_monster());
    beam.origin_spell = SPELL_GRAVE_CLAW;
    beam.source = beam.target = targ;
    zappy(ZAP_GRAVE_CLAW, pow, caster.is_monster(), beam);
    beam.hit_verb = "skewer";
    beam.fire();

    if (caster.is_player())
    {
        if (you.props[GRAVE_CLAW_CHARGES_KEY].get_int() == 0)
            mprf(MSGCH_DURATION, "The last of your harvested death is exhausted.");
    }

    return spret::success;
}

void gain_grave_claw_soul(bool silent, bool wizard)
{
    int& charges = you.props[GRAVE_CLAW_CHARGES_KEY].get_int();

    // Don't gain charge if we're already full.
    if (charges == GRAVE_CLAW_MAX_CHARGES)
        return;

    if (--you.duration[DUR_GRAVE_CLAW_RECHARGE] <= 0 || wizard)
    {
        // Set recharge to a random 4-6 kills.
        you.duration[DUR_GRAVE_CLAW_RECHARGE] = random_range(4, 6);
        charges = wizard ? GRAVE_CLAW_MAX_CHARGES: charges + 1;

        if (silent)
            return;

        if (charges == GRAVE_CLAW_MAX_CHARGES)
        {
            mprf(MSGCH_DURATION, "You have harvested as much death for "
                                 "Grave Claw as you can hold at once.");
        }
        else
        {
            mprf(MSGCH_DURATION, "You have harvested enough death to cast "
                                 "Grave Claw an additional time.");
        }
    }
}

spret cast_fortress_blast(actor& caster, int pow, bool fail)
{
    fail_check();

    if (caster.is_player())
    {
        const int delay = 70 - div_rand_round(pow * 10, 25);
        you.duration[DUR_FORTRESS_BLAST_TIMER] = delay;
        mprf("You steady yourself in place and focus your resilience into a mighty blast.");
    }
    // Todo: Add monster implementation here

    caster.props[FORTRESS_BLAST_POS_KEY] = caster.pos();

    return spret::success;
}

void unleash_fortress_blast(actor& caster)
{
    // Hits power cap at 70 AC
    int power = min(200, (int)pow(caster.armour_class(), 1.343) * 2 / 3);

    if (caster.is_player())
    {
        string desc = "a";
        if (power < 30)
            desc = "a weak";
        else if (power > 180)
            desc = "an overpowering";
        else if (power > 135)
            desc = "a very strong";
        else if (power > 80)
            desc = "a strong";
        mprf(MSGCH_DURATION, "You unleash %s concussive blast!", desc.c_str());
    }

    bolt blast;
    zappy(ZAP_FORTRESS_BLAST, power, caster.is_monster(), blast);
    blast.set_agent(&caster);
    blast.attitude = caster.temp_attitude();
    blast.source = caster.pos();
    blast.target = caster.pos();
    blast.origin_spell = SPELL_FORTRESS_BLAST;
    blast.is_explosion = true;
    blast.ex_size = spell_range(SPELL_FORTRESS_BLAST, 0);

    blast.explode(true, true);
}

// For UI purposes. Non-random.
dice_def fortress_blast_damage(int AC, bool is_monster)
{
    int power = min(200, (int)pow(AC, 1.343) * 2 / 3);
    return zap_damage(ZAP_FORTRESS_BLAST, power, is_monster, false);
}
