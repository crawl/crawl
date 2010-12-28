#include "AppHdr.h"

#include "spl-damage.h"

#include "cloud.h"
#include "coordit.h"
#include "env.h"
#include "godconduct.h"
#include "los.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "ouch.h"
#include "player.h"
#include "spl-cast.h"
#include "stuff.h"
#include "terrain.h"

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
         you.is_levitating() ? "appears around you"
                             : "appears and lifts you up");

    if (you.fishtail)
        merfolk_stop_swimming();

    _set_tornado_durations(powc);
    burden_change();

    return true;
}

void tornado_damage(int dur)
{
    if (dur <= 0)
        return;

    // Not stored so unwielding that staff will reduce damage.
    int pow = div_rand_round(calc_spell_power(SPELL_TORNADO, true) * dur, 10);
    dprf("Doing tornado, dur %d, effective power %d", dur, pow);
    const coord_def org = you.pos();

    std::stack<actor*>    move_act;

    distance_iterator count_i(org, false);
    distance_iterator dam_i(org, true);
    for (int r = 1; r <= TORNADO_RADIUS; r++)
    {
        int cnt_open = 0;
        int cnt_all  = 0;
        while (count_i && count_i.radius() == r)
        {
            if (!feat_is_solid(grd(*count_i)))
                cnt_open++;
            cnt_all++;
            count_i++;
        }
        pow = pow * cnt_open / cnt_all;
        dprf("at dist %d pow is %d", r, pow);
        if (!pow)
            break;

        std::vector<coord_def> clouds;
        for (; dam_i && dam_i.radius() == r; dam_i++)
        {
            if (grd(*dam_i) == DNGN_TREE && one_chance_in(20))
            {
                grd(*dam_i) = DNGN_FLOOR;
                set_terrain_changed(*dam_i);
                if (you.see_cell(*dam_i))
                    mpr("A tree falls to the hurricane!");
                did_god_conduct(DID_KILL_PLANT, 1);
            }

            if (feat_is_solid(grd(*dam_i)))
                continue;

            if (actor* victim = actor_at(*dam_i))
            {
                if (victim->submerged())
                    continue;

                if (!victim->res_wind())
                {
                    if (victim->atype() == ACT_MONSTER)
                    {
                        // levitate the monster so you get only one attempt at
                        // tossing them into water/lava
                        monster *mon = victim->as_monster();
                        mon_enchant ench(ENCH_LEVITATION, 0, KC_YOU, 20);
                        if (mon->has_ench(ENCH_LEVITATION))
                            mon->update_ench(ench);
                        else
                            mon->add_ench(ench);
                        behaviour_event(mon, ME_ANNOY, MHITYOU);
                        if (mons_is_mimic(mon->type))
                            mimic_alert(mon);
                    }
                    int dmg = roll_dice(6, pow) / 8;
                    dprf("damage done: %d", dmg);
                    victim->hurt(&you, dmg);
                }

                if (victim->alive())
                    move_act.push(victim);
            }
            if ((env.cgrid(*dam_i) == EMPTY_CLOUD
                || env.cloud[env.cgrid(*dam_i)].type == CLOUD_TORNADO)
                && x_chance_in_y(pow, 20))
            {
                place_cloud(CLOUD_TORNADO, *dam_i, 2 + random2(2), &you);
            }
            clouds.push_back(*dam_i);
            swap_clouds(clouds[random2(clouds.size())], *dam_i);
        }
    }
}

void cancel_tornado()
{
    if (!you.duration[DUR_TORNADO])
        return;

    dprf("Aborting tornado.");
    if (you.duration[DUR_TORNADO] == you.duration[DUR_LEVITATION]
        && !you.permanent_levitation() && !you.permanent_flight())
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
