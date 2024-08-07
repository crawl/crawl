#include "AppHdr.h"

#include "spl-damage.h"

#include <cfloat>
#include <cmath>

#include "areas.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "env.h"
#include "fight.h"
#include "fineff.h"
#include "fprop.h"
#include "god-conduct.h"
#include "god-passive.h" // passive_t::shoot_through_plants
#include "libutil.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-tentacle.h"
#include "ouch.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-goditem.h" // majin_bo_vampirism
#include "target.h"
#include "terrain.h"
#include "transform.h"

static bool _airtight(coord_def c)
{
    // Broken by 6f473416 -- we should re-allow the wind through grates.

    // return (feat_is_wall(env.grid(c)) || feat_is_opaque(env.grid(c))) && env.grid(c);
    return !feat_is_reachable_past(env.grid(c));
}

/* Explanation of the algorithm:
   http://en.wikipedia.org/wiki/Biconnected_component
   We include everything up to and including the first articulation vertex,
   the center is never considered to be one.
*/
class WindSystem
{
    coord_def org;
    SquareArray<int, POLAR_VORTEX_RADIUS+1> depth;
    SquareArray<bool, POLAR_VORTEX_RADIUS+1> cut, wind;
    int visit(coord_def c, int d, coord_def parent);
    void pass_wind(coord_def c);
public:
    WindSystem(coord_def _org);
    bool has_wind(coord_def c);
};

WindSystem::WindSystem(coord_def _org)
    : org(_org)
{
    depth.init(-1);
    cut.init(false);
    visit(org, 0, coord_def(0,0));
    cut(coord_def(0,0)) = false;
    wind.init(false);
    pass_wind(org);
}

int WindSystem::visit(coord_def c, int d, coord_def parent)
{
    depth(c - org) = d;
    int low = d;
    int sonmax = -1;

    for (adjacent_iterator ai(c); ai; ++ai)
    {
        if ((*ai - org).rdist() > POLAR_VORTEX_RADIUS || _airtight(*ai))
            continue;
        if (depth(*ai - org) == -1)
        {
            int sonlow = visit(*ai, d+1, c);
            low = min(low, sonlow);
            sonmax = max(sonmax, sonlow);
        }
        else if (*ai != parent)
            low = min(low, depth(*ai - org));
    }

    cut(c - org) = (sonmax >= d);
    return low;
}

void WindSystem::pass_wind(coord_def c)
{
    wind(c - org) = true;
    depth(c - org) = -1;
    if (cut(c - org))
        return;

    for (adjacent_iterator ai(c); ai; ++ai)
        if (depth(*ai - org) != -1)
            pass_wind(*ai);
}

bool WindSystem::has_wind(coord_def c)
{
    ASSERT(grid_distance(c, org) <= POLAR_VORTEX_RADIUS); // might say no instead
    return wind(c - org);
}

static void _set_vortex_durations()
{
    int dur = 60;
    you.duration[DUR_VORTEX] = dur;
    if (!get_form()->forbids_flight())
        you.duration[DUR_FLIGHT] = max(dur, you.duration[DUR_FLIGHT]);
}

spret cast_polar_vortex(int powc, bool fail)
{
    targeter_radius hitfunc(&you, LOS_NO_TRANS, POLAR_VORTEX_RADIUS);
    if (stop_attack_prompt(hitfunc, "make a polar vortex",
                [](const actor *act) -> bool {
                    return !act->res_polar_vortex()
                        && (!act->is_monster()
                            || !god_protects(&you, act->as_monster(), true));
                }))
    {
        return spret::abort;
    }

    fail_check();

    mprf("A great freezing vortex %s.",
         (you.airborne() || get_form()->forbids_flight()) ?
         "appears around you" : "appears and lifts you up");

    if (you.fishtail)
        merfolk_stop_swimming();

    you.props[POLAR_VORTEX_KEY].get_int() = you.elapsed_time;
    you.props[VORTEX_POWER_KEY] = powc;
    _set_vortex_durations();

    return spret::success;
}

static bool _mons_is_unmovable(const monster *mons)
{
    // hard to explain uprooted oklobs surviving
    if (mons->is_stationary())
        return true;
    // we'd have to rotate everything
    if (mons_is_tentacle_or_tentacle_segment(mons->type)
        || mons_is_tentacle_head(mons_base_type(*mons)))
    {
        return true;
    }
    return false;
}

static double _get_ang(int x, int y)
{
    if (abs(x) > abs(y))
    {
        if (x > 0)
            return double(y)/double(x);
        else
            return 4 + double(y)/double(x);
    }
    else
    {
        if (y > 0)
            return 2 - double(x)/double(y);
        else
            return -2 - double(x)/double(y);
    }
}

static coord_def _rotate(coord_def org, coord_def from,
                         vector<coord_def> &avail, int rdur)
{
    if (avail.empty())
        return from;

    coord_def best = from;
    double hiscore = DBL_MAX;

    double dist0 = (from - org).rdist();
    double ang0 = _get_ang(from.x - org.x, from.y - org.y) - rdur * 0.01 * 4 / 3;
    for (coord_def pos : avail)
    {
        double dist = (pos - org).rdist();
        double distdiff = fabs(dist - dist0);
        double ang = _get_ang(pos.x - org.x, pos.y - org.y);
        double angdiff = min(fabs(ang - ang0), fabs(ang - ang0 - 8));

        double score = distdiff + angdiff * 3 / 2;
        if (score < hiscore)
            best = pos, hiscore = score;
    }

    // must find _something_, the original space might be already taken
    ASSERT(hiscore != DBL_MAX);

    return best;
}

static int _rdam(int rage)
{
    // integral of damage done until given age-radius
    if (rage <= 0)
        return 0;
    else if (rage < 10)
        return sqr(rage) / 2;
    else
        return rage * 10 - 50;
}

static int _vortex_age(const actor *caster)
{
    const string name = POLAR_VORTEX_KEY;
    if (caster->props.exists(name.c_str()))
        return you.elapsed_time - caster->props[name.c_str()].get_int();
    return 100; // for permanent vortices
}

// time needed to reach the given radius
static int _age_needed(int r)
{
    if (r <= 0)
        return 0;
    if (r > POLAR_VORTEX_RADIUS)
        return INT_MAX;
    return sqr(r) * 7 / 5;
}

dice_def polar_vortex_dice(int pow, bool random)
{
    if (random)
        return dice_def(12, div_rand_round(pow, 15));
    return dice_def(12, pow / 15);
}

void polar_vortex_damage(actor *caster, int dur)
{
    if (!dur)
        return;

    int pow;
    const int max_radius = min(POLAR_VORTEX_RADIUS, (int)you.current_vision);

    if (caster->is_player())
        pow = you.props[VORTEX_POWER_KEY].get_int();
    else
        // XXX TODO: use the normal spellpower calc functions
        pow = caster->as_monster()->get_hit_dice() * 4;
    const coord_def org = caster->pos();
    int noise = 0;
    WindSystem winds(org);

    const coord_def old_player_pos = you.pos();
    coord_def new_player_pos = old_player_pos;

    int age = _vortex_age(caster);
    ASSERT(age >= 0);

    vector<coord_def>     move_avail; // legal destinations
    map<mid_t, coord_def> move_dest;  // chosen destination
    int rdurs[POLAR_VORTEX_RADIUS + 1];    // durations at radii
    int cnt_open = 0;
    int cnt_all  = 0;

    distance_iterator count_i(org, false);
    distance_iterator dam_i(org, true);
    for (int r = 1; r <= max_radius; r++)
    {
        while (count_i && count_i.radius() == r)
        {
            if (winds.has_wind(*count_i))
                cnt_open++;
            ++cnt_all;
            ++count_i;
        }
        // effective age at radius r
        int rage = age - _age_needed(r);
        /* Not just "portion of time affected":
                          **
                        **
                  ----++----
                    **......
                  **........
           here, damage done is 3/4, not 1/2.
        */
        // effective duration at the radius
        int rdur = _rdam(rage + abs(dur)) - _rdam(rage);
        rdurs[r] = rdur;
        // power at the radius
        int rpow = div_rand_round(pow * cnt_open * rdur, cnt_all * 100);
        if (!rpow)
            break;

        noise = max(div_rand_round(r * rdur * 3, 100), noise);

        vector<coord_def> clouds;
        for (; dam_i && dam_i.radius() == r; ++dam_i)
        {
            bool veto =
                env.markers.property_at(*dam_i, MAT_ANY, "veto_destroy") == "veto";

            if (feat_is_tree(env.grid(*dam_i))
                && !have_passive(passive_t::shoot_through_plants)
                && !is_temp_terrain(*dam_i)
                && !veto
                && dur > 0
                && bernoulli(rdur * 0.01, 0.05)) // 5% chance per 10 aut
            {
                env.grid(*dam_i) = DNGN_FLOOR;
                set_terrain_changed(*dam_i);
                if (you.see_cell(*dam_i))
                    mpr("A tree falls to the furious winds!");
            }

            if (!winds.has_wind(*dam_i))
                continue;

            bool leda = false; // squares with ledaed enemies are no-go
            if (actor* victim = actor_at(*dam_i))
            {
                if (victim->is_player() && monster_at(*dam_i))
                {
                    // A far-fetched case: you're using Fedhas' passthrough
                    // or standing on a submerged air elemental, there are
                    // no free spots, and a monster vortex rotates you.
                    // Plants don't get uprooted, so the logic would be
                    // really complex. Let's not go there.
                    continue;
                }
                if (victim->is_player() && get_form()->forbids_flight())
                    continue;

                leda = victim->liquefied_ground()
                       || victim->is_monster()
                          && _mons_is_unmovable(victim->as_monster());
                if (!victim->res_polar_vortex())
                {
                    if (victim->is_monster())
                    {
                        monster *mon = victim->as_monster();
                        if (!leda)
                        {
                            // fly the monster so you get only one attempt
                            // at tossing them into water/lava
                            mon_enchant ench(ENCH_FLIGHT, 0, caster, 20);
                            if (mon->has_ench(ENCH_FLIGHT))
                                mon->update_ench(ench);
                            else
                                mon->add_ench(ench);
                        }
                        behaviour_event(mon, ME_ANNOY, caster);
                    }
                    else if (!leda)
                    {
                        bool standing = !you.airborne();
                        if (standing)
                            mpr("The freezing vortex lifts you up.");
                        you.duration[DUR_FLIGHT]
                            = max(you.duration[DUR_FLIGHT], 20);
                        if (standing)
                            float_player();
                    }

                    // alive check here in case the annoy event above dismissed
                    // the victim.
                    if (dur > 0 && victim->alive()
                        && (!caster->is_player()
                            || !victim->is_monster()
                            || !god_protects(caster, victim->as_monster(), true)))
                    {
                        const int base_dmg = polar_vortex_dice(rpow, true).roll();
                        const int post_res_dmg
                            = resist_adjust_damage(victim, BEAM_ICE, base_dmg);
                        const int post_ac_dmg
                            = victim->apply_ac(post_res_dmg, 0, ac_type::proportional);
                        dprf("damage done: %d", post_ac_dmg);
                        if (caster->is_player())
                            majin_bo_vampirism(*victim->as_monster(), post_ac_dmg);
                        victim->hurt(caster, post_ac_dmg, BEAM_ICE, KILLED_BY_BEAM,
                                     "", "vortex");
                    }
                }

                if (victim->alive()
                    && !leda
                    && dur > 0
                    && !victim->resists_dislodge("being moved by the vortex"))
                {
                    move_dest[victim->mid] = victim->pos();
                }
            }

            if (cell_is_solid(*dam_i))
                continue;

            if ((!cloud_at(*dam_i) || cloud_at(*dam_i)->type == CLOUD_VORTEX)
                && x_chance_in_y(rpow, 20))
            {
                place_cloud(CLOUD_VORTEX, *dam_i, 2 + random2(2), caster);
            }
            clouds.push_back(*dam_i);
            swap_clouds(clouds[random2(clouds.size())], *dam_i);

            if (!leda)
                move_avail.push_back(*dam_i);
        }
    }

    noisy(noise, org, caster->mid);

    if (dur <= 0)
        return;

    // Gather actors who are to be moved.
    for (auto &entry : move_dest)
        if (actor* act = actor_by_mid(entry.first)) // should still be alive...
        {
            ASSERT(entry.second == act->pos());

            // Temporarily move to (0,0) to allow permutations.
            if (env.mgrid(act->pos()) == act->mindex())
                env.mgrid(act->pos()) = NON_MONSTER;
            act->moveto(coord_def());
            if (act->is_player())
                stop_delay(true);
        }

    // Need to check available positions again, as the damage call could
    // have spawned something new (like Royal Jelly spawns).
    erase_if(move_avail, actor_at);

    // Calculate destinations.
    for (auto &entry : move_dest)
    {
        const int r = entry.second.distance_from(org);
        coord_def dest = _rotate(org, entry.second, move_avail, rdurs[r]);
        // Only one monster per destination.
        erase_if(move_avail, [&dest](const coord_def& p) { return p == dest; });
        entry.second = dest;
    }

    // Actually move actors into place.
    for (auto &entry : move_dest)
        if (actor* act = actor_by_mid(entry.first)) // should still be alive...
        {
            const coord_def newpos = entry.second;
            ASSERT(!actor_at(newpos));
            act->move_to_pos(newpos);
            ASSERT(act->pos() == newpos);

            if (act->is_player())
                new_player_pos = newpos;
        }

    if (caster->is_player())
        fire_final_effects();
    else
    {
        if (new_player_pos != old_player_pos
            && !need_expiration_warning(old_player_pos)
            && need_expiration_warning(new_player_pos))
        {
            mprf(MSGCH_DANGER, "Careful! You are now flying above %s.",
                 feature_description_at(new_player_pos, false, DESC_PLAIN)
                     .c_str());
        }
    }
}

void cancel_polar_vortex(bool tloc)
{
    if (!you.duration[DUR_VORTEX])
        return;

    dprf("Aborting vortex.");
    if (you.duration[DUR_VORTEX] == you.duration[DUR_FLIGHT])
    {
        if (tloc)
        {
            // it'd be better to abort flight instantly, but let's first
            // make damn sure all ways of translocating are prevented from
            // landing you in water. Insta-kill due to an arrow of dispersal
            // is not nice.
            you.duration[DUR_FLIGHT] = min(20, you.duration[DUR_FLIGHT]);
        }
        else
        {
            // Vortex ended by using something stairslike, so the destination
            // is safe
            you.duration[DUR_FLIGHT] = 0;
        }
    }
    you.duration[DUR_VORTEX] = 0;
    you.duration[DUR_VORTEX_COOLDOWN] = random_range(35, 45);
}
