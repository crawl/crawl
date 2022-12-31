/**
 * @file
 * @brief Monster-affecting enchantment spells.
 *           Other targeted enchantments are handled in spl-zap.cc.
**/

#include "AppHdr.h"

#include "spl-monench.h"

#include "english.h" // apostrophise
#include "env.h"
#include "message.h"
#include "spl-util.h"
#include "stringutil.h" // make_stringf
#include "terrain.h"

int englaciate(coord_def where, int pow, actor *agent)
{
    actor *victim = actor_at(where);

    if (!victim || victim == agent)
        return 0;

    if (agent->is_monster() && mons_aligned(agent, victim))
        return 0; // don't let monsters hit friendlies

    monster* mons = victim->as_monster();

    if (victim->res_cold() > 0
        || victim->is_stationary())
    {
        if (!mons)
            canned_msg(MSG_YOU_UNAFFECTED);
        else if (!mons_is_firewood(*mons))
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
 *  @returns true if it got backlit (even if it was already).
 */
bool backlight_monster(monster* mons)
{
    const mon_enchant bklt = mons->get_ench(ENCH_CORONA);
    const mon_enchant zin_bklt = mons->get_ench(ENCH_SILVER_CORONA);
    const int lvl = bklt.degree + zin_bklt.degree;

    mons->add_ench(mon_enchant(ENCH_CORONA, 1));

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

    if (!mon.is_stationary()
        && mon.add_ench(mon_enchant(ENCH_SLOW, 0, agent, dur)))
    {
        if (!mon.paralysed() && !mon.petrified()
            && simple_monster_message(mon, " seems to slow down."))
        {
            return true;
        }
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
        hexes.push_back(ENCH_ANTIMAGIC);
    if (res_margin <= 0)
    {
        if (mons_can_be_blinded(mon.type))
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
        if (mon.has_ench(hex))
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

spret cast_vile_clutch(int pow, bolt &beam, bool fail)
{
    spret result = zapping(ZAP_VILE_CLUTCH, pow, beam, true, nullptr, fail);

    if (result == spret::success)
        you.props[VILE_CLUTCH_POWER_KEY].get_int() = pow;

    return result;
}

string mons_simulacrum_immune_reason(const monster *mons)
{
    if (!mons || !you.can_see(*mons))
        return "You can't see anything there.";

    if (mons->has_ench(ENCH_SIMULACRUM))
    {
        return make_stringf("%s's soul is already gripped in ice!",
                            mons->name(DESC_THE).c_str());
    }

    if (!mons_can_be_spectralised(*mons))
        return "You can't make simulacra of that!";

    return "";
}

spret cast_simulacrum(coord_def target, int pow, bool fail)
{
    if (cell_is_solid(target))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return spret::abort;
    }

    monster* mons = monster_at(target);
    const string immune_reason = mons_simulacrum_immune_reason(mons);
    if (!immune_reason.empty())
    {
        mprf("%s", immune_reason.c_str());
        return spret::abort;
    }

    fail_check();
    int dur = 20 + random2(1 + div_rand_round(pow, 10));
    mprf("You freeze %s soul.", apostrophise(mons->name(DESC_THE)).c_str());
    mons->add_ench(mon_enchant(ENCH_SIMULACRUM, 0, &you, dur * BASELINE_DELAY));
    mons->props[SIMULACRUM_POWER_KEY] = pow;
    return spret::success;
}

void grasp_with_roots(actor &caster, actor &target, int turns)
{
    if (target.is_player())
    {
        you.increase_duration(DUR_GRASPING_ROOTS, turns);
        caster.start_constricting(you);
        mprf(MSGCH_WARN, "The grasping roots grab you!");
    }
    else
    {
        auto ench = mon_enchant(ENCH_GRASPING_ROOTS, 0, &caster,
                                turns * BASELINE_DELAY);
        target.as_monster()->add_ench(ench);
    }
}
