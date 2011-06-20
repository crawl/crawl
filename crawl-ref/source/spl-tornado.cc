#include "AppHdr.h"

#include "spl-damage.h"

#include "areas.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fineff.h"
#include "godconduct.h"
#include "los.h"
#include "math.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "ouch.h"
#include "player.h"
#include "shout.h"
#include "spl-cast.h"
#include "stuff.h"
#include "terrain.h"

static bool _airtight(coord_def c)
{
    return grd(c) <= DNGN_MAXWALL;
}

/* Explanation of the algorithm:
   http://en.wikipedia.org/wiki/Biconnected_component
   We include everything up to and including the first articulation vertex,
   the center is never considered to be one.
*/
class WindSystem
{
    coord_def org;
    SquareArray<int, TORNADO_RADIUS+1> depth;
    SquareArray<bool, TORNADO_RADIUS+1> cut, wind;
    int visit(coord_def c, int d, coord_def parent);
    void pass_wind(coord_def c);
public:
    WindSystem(coord_def _org);
    bool has_wind(coord_def c);
};

WindSystem::WindSystem(coord_def _org)
{
    org = _org;
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
        if ((*ai - org).abs() > dist_range(TORNADO_RADIUS) || _airtight(*ai))
            continue;
        if (depth(*ai - org) == -1)
        {
            int sonlow = visit(*ai, d+1, c);
            low = std::min(low, sonlow);
            sonmax = std::max(sonmax, sonlow);
        }
        else if (*ai != parent)
            low = std::min(low, depth(*ai - org));
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
    ASSERT(grid_distance(c, org) <= TORNADO_RADIUS); // might say no instead
    return wind(c - org);
}

static void _set_tornado_durations(int powc)
{
    int dur = 40 + powc / 6;
    you.duration[DUR_TORNADO] = dur;
    you.duration[DUR_LEVITATION] =
        std::max(dur, you.duration[DUR_LEVITATION]);
    you.duration[DUR_CONTROLLED_FLIGHT] =
        std::max(dur, you.duration[DUR_CONTROLLED_FLIGHT]);
    you.attribute[ATTR_LEV_UNCANCELLABLE] = 1;
}

bool cast_tornado(int powc)
{
    if (you.duration[DUR_TORNADO])
    {
        _set_tornado_durations(powc);
        mpr("The winds around you grow in strength.");
        return true;
    }

    bool friendlies = false;
    for (radius_iterator ri(you.pos(), TORNADO_RADIUS, C_ROUND); ri; ++ri)
    {
        const monster_info* m = env.map_knowledge(*ri).monsterinfo();
        if (!m)
            continue;
        if (mons_att_wont_attack(m->attitude)
            && mons_class_res_wind(m->type) <= 0
            && !mons_is_projectile(m->type))
        {
            friendlies = true;
        }
    }

    if (friendlies
        && !yesno("There are friendlies around, are you sure you want to hurt them?",
                  true, 'n'))
    {
        return false;
    }

    mprf("A great vortex of raging winds %s.",
         you.airborne() ? "appears around you"
                        : "appears and lifts you up");

    if (you.fishtail)
        merfolk_stop_swimming();

    _set_tornado_durations(powc);
    burden_change();

    return true;
}

static bool _mons_is_unmovable(const monster *mons)
{
    // hard to explain uprooted oklobs surviving
    if (mons_is_stationary(mons))
        return true;
    // we'd have to rotate everything
    if (mons_is_tentacle(mons->type) || mons_base_type(mons) == MONS_KRAKEN)
        return true;
    return false;
}

static coord_def _rotate(coord_def org, coord_def from,
                         std::vector<coord_def> &avail, int dur)
{
    if (avail.empty())
        return from;

    coord_def best;
    double hiscore = 1e38;

    double dist0 = sqrt((from - org).abs());
    double ang0 = atan2(from.x - org.x, from.y - org.y) + dur * 0.1;
    if (ang0 > PI)
        ang0 -= 2 * PI;
    for (unsigned int i = 0; i < avail.size(); i++)
    {
        double dist = sqrt((avail[i] - org).abs());
        double distdiff = fabs(dist - dist0);
        double ang = atan2(avail[i].x - org.x, avail[i].y - org.y);
        double angdiff = std::min(fabs(ang - ang0), fabs(ang - ang0 + 2 * PI));

        // Let's give 1 rad =~ 57⁰ of mismatch the same weight as changing
        // radius by 1.  In general, infelicities in radius have bigger balance
        // effect than those in angle.
        double score = distdiff + angdiff;
        if (score < hiscore)
            best = avail[i], hiscore = score;
    }

    return best;
}

void tornado_damage(actor *caster, int dur)
{
    if (!dur)
        return;

    int pow;
    // Not stored so unwielding that staff will reduce damage.
    if (caster->atype() == ACT_PLAYER)
        pow = calc_spell_power(SPELL_TORNADO, true);
    else
        pow = caster->as_monster()->hit_dice * 4;
    dprf("Doing tornado, dur %d, effective power %d", dur, pow);
    const coord_def org = caster->pos();
    noisy(25, org, caster->mindex());
    WindSystem winds(org);

    std::vector<actor*>        move_act;   // victims to move
    std::vector<coord_def>     move_avail; // legal destinations
    std::map<mid_t, coord_def> move_dest;  // chosen destination
    int cnt_open = 0;
    int cnt_all  = 0;

    distance_iterator count_i(org, false);
    distance_iterator dam_i(org, true);
    for (int r = 1; r <= TORNADO_RADIUS; r++)
    {
        while (count_i && count_i.radius() == r)
        {
            if (winds.has_wind(*count_i))
                cnt_open++;
            cnt_all++;
            count_i++;
        }
        int rpow = pow * cnt_open / cnt_all;
        dprf("at dist %d pow is %d", r, rpow);
        if (!rpow)
            break;

        std::vector<coord_def> clouds;
        for (; dam_i && dam_i.radius() == r; dam_i++)
        {
            if (feat_is_tree(grd(*dam_i)) && dur > 0 && one_chance_in(20))
            {
                grd(*dam_i) = DNGN_FLOOR;
                set_terrain_changed(*dam_i);
                if (you.see_cell(*dam_i))
                    mpr("A tree falls to the hurricane!");
                if (caster == &you)
                    did_god_conduct(DID_KILL_PLANT, 1);
            }

            if (!winds.has_wind(*dam_i))
                continue;

            bool leda = false; // squares with ledaed enemies are no-go
            if (actor* victim = actor_at(*dam_i))
            {
                if (victim->submerged())
                {
                    leda = true; // and with fish, too
                    continue;
                }

                leda = liquefied(victim->pos()) && victim->ground_level()
                    || victim->atype() == ACT_MONSTER
                       && _mons_is_unmovable(victim->as_monster());
                if (!victim->res_wind())
                {
                    if (victim->atype() == ACT_MONSTER)
                    {
                        monster *mon = victim->as_monster();
                        if (!leda)
                        {
                            // levitate the monster so you get only one attempt
                            // at tossing them into water/lava
                            mon_enchant ench(ENCH_LEVITATION, 0,
                                             caster, 20);
                            if (mon->has_ench(ENCH_LEVITATION))
                                mon->update_ench(ench);
                            else
                                mon->add_ench(ench);
                        }
                        behaviour_event(mon, ME_ANNOY, caster->mindex());
                        if (mons_is_mimic(mon->type))
                            mimic_alert(mon);
                    }
                    else if (!leda)
                    {
                        bool standing = !you.airborne();
                        if (standing)
                            mpr("The vortex of raging winds lifts you up.");
                        you.attribute[ATTR_LEV_UNCANCELLABLE] = 1;
                        you.duration[DUR_LEVITATION]
                            = std::max(you.duration[DUR_LEVITATION], 20);
                        if (standing)
                            float_player(false);
                    }
                    int dmg = roll_dice(6, rpow) / 15
                              - random2(victim->armour_class() / 3 + 1);
                    if (dur > 0)
                        dmg = div_rand_round(dmg * dur, 10);
                    else if (dur < 0)
                        dmg = 0;
                    dprf("damage done: %d", dmg);
                    if (victim->atype() == ACT_PLAYER)
                        ouch(dmg, caster->mindex(), KILLED_BY_BEAM,
                             "tornado");
                    else
                        victim->hurt(caster, dmg);
                }

                if (victim->alive() && !leda)
                    move_act.push_back(victim);
            }
            if ((env.cgrid(*dam_i) == EMPTY_CLOUD
                || env.cloud[env.cgrid(*dam_i)].type == CLOUD_TORNADO)
                && x_chance_in_y(rpow, 20))
            {
                place_cloud(CLOUD_TORNADO, *dam_i, 2 + random2(2), caster);
            }
            clouds.push_back(*dam_i);
            swap_clouds(clouds[random2(clouds.size())], *dam_i);

            if (!leda)
                move_avail.push_back(*dam_i);
        }
    }

    if (dur <= 0)
        return;

    for (unsigned int i = 0; i < move_act.size(); i++)
    {
        coord_def dest = _rotate(org, move_act[i]->pos(), move_avail, dur);
        for (unsigned int j = 0; j < move_avail.size(); j++)
            if (move_avail[j] == dest)
            {
                // Only one monster per destination.
                move_avail[j] = move_avail[move_avail.size() - 1];
                move_avail.pop_back();
                break;
            }
        move_dest[move_act[i]->mid] = dest;
    }
    for (unsigned int i = 0; i < move_act.size(); i++)
        if (move_act[i]->alive()) // shouldn't ever change...
        {
            // Temporarily move to (0,0) to allow permutations.
            if (mgrd(move_act[i]->pos()) == move_act[i]->mindex())
                mgrd(move_act[i]->pos()) = NON_MONSTER;
            move_act[i]->moveto(coord_def());
        }
    for (unsigned int i = 0; i < move_act.size(); i++)
        if (move_act[i]->alive())
        {
            coord_def newpos = move_dest[move_act[i]->mid];
            ASSERT(!actor_at(newpos));
            move_act[i]->move_to_pos(newpos);
            ASSERT(move_act[i]->pos() == newpos);
        }

    if (caster == &you)
        fire_final_effects();
}

void cancel_tornado()
{
    if (!you.duration[DUR_TORNADO])
        return;

    dprf("Aborting tornado.");
    if (you.duration[DUR_TORNADO] == you.duration[DUR_LEVITATION])
    {
        you.duration[DUR_LEVITATION] = 0;
        you.duration[DUR_CONTROLLED_FLIGHT] = 0;
        you.attribute[ATTR_LEV_UNCANCELLABLE] = 0;
        burden_change();
        // NO checking for water, since this is called only during level
        // change, and being, say, banished from above water shouldn't
        // kill you.
    }
    you.duration[DUR_TORNADO] = 0;
}
