/**
 * @file
 * @brief Fight simualtion wizard functions.
**/

#include "AppHdr.h"

#include "wiz-fsim.h"

#include <errno.h>

#include "beam.h"
#include "coordit.h"
#include "dbg-util.h"
#include "env.h"
#include "fight.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "message.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "monster.h"
#include "mon-clone.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "player-equip.h"
#include "skills.h"
#include "skills2.h"
#include "species.h"
#include "stuff.h"

#ifdef WIZARD

fight_data null_fight = {0.0,0,0,0.0,0.0,0.0};

// strings and things
static const std::string _dummy_string =
    "AvHitDam | MaxDam | Accuracy | AvDam | AvTime | AvEffDam"; // 55 columns

static std::string _fight_string(fight_data fdata)
{
    return make_stringf("   %5.1f |    %3d |     %3d%% |"
                        " %5.1f |  %5.1f |    %5.1f",
                        fdata.av_hit_dam, fdata.max_dam, fdata.accuracy,
                        fdata.av_dam, fdata.av_time, fdata.av_eff_dam);
}

static skill_type _equipped_skill()
{
    const int weapon = you.equip[EQ_WEAPON];
    const item_def * iweap = weapon != -1 ? &you.inv[weapon] : NULL;
    const int missile = you.m_quiver->get_fire_item();

    if (iweap && iweap->base_type == OBJ_WEAPONS)
    {
        if (is_range_weapon(*iweap))
            return range_skill(*iweap);
        return weapon_skill(*iweap);
    }

    if (missile != -1)
        return range_skill(you.inv[missile]);

    return SK_UNARMED_COMBAT;
}

static std::string _equipped_weapon_name()
{
    const int weapon = you.equip[EQ_WEAPON];
    const item_def * iweap = weapon != -1 ? &you.inv[weapon] : NULL;
    const int missile = you.m_quiver->get_fire_item();

    if (iweap)
    {
        std::string item_buf = iweap->name(DESC_PLAIN, true);
        // If it's a ranged weapon, add the description of the missile
        if (is_range_weapon(*iweap) && missile < ENDOFPACK && missile >= 0)
                item_buf += " with " + you.inv[missile].name(DESC_PLAIN);
        return item_buf;
    }

    if (missile != -1)
        return you.inv[missile].name(DESC_PLAIN);

    return "unarmed";
}

static std::string _time_string()
{
    time_t curr_time = time(NULL);
    struct tm *ltime = TIME_FN(&curr_time);
    if (ltime)
    {
        return make_stringf("%4d/%02d/%02d/%2d:%02d:%02d",
                 ltime->tm_year + 1900,
                 ltime->tm_mon  + 1,
                 ltime->tm_mday,
                 ltime->tm_hour,
                 ltime->tm_min,
                 ltime->tm_sec);
    }
    return ("");
}

static void _write_version(FILE * o)
{
    fprintf(o, CRAWL " version %s\n", Version::Long().c_str());
}

static void _write_matchup(FILE * o, monster &mon, bool defend, int iter_limit)
{
    fprintf(o, "%s: %s %s vs. %s (%d rounds) (%s)\n",
            defend ? "Defense" : "Attack",
            species_name(you.species).c_str(),
            you.class_name.c_str(),
            mon.name(DESC_PLAIN, true).c_str(),
            iter_limit,
            _time_string().c_str());
}

static void _write_you(FILE * o)
{
    fprintf(o, "%s %s: XL %d   Str %d   Int %d   Dex %d\n",
            species_name(you.species).c_str(),
            you.class_name.c_str(),
            you.experience_level,
            you.strength(),
            you.intel(),
            you.dex());
}

static void _write_weapon(FILE * o)
{
    fprintf(o, "Wielding: %s, Skill: %s\n",
            _equipped_weapon_name().c_str(),
            skill_name(_equipped_skill()));
}

static void _write_mon(FILE * o, monster &mon)
{
    fprintf(o, "%s: HD %d   AC %d   EV %d\n",
            mon.name(DESC_PLAIN, true).c_str(),
            mon.hit_dice,
            mon.ac,
            mon.ev);
}

// fight simulator internals
static monster* _init_fsim()
{
    monster * mon = NULL;
    monster_type mtype = get_monster_by_name(Options.fsim_mons);

    if(mtype == MONS_PROGRAM_BUG && monster_nearby())
    {
        // get a monster via targetting.
        dist moves;
        direction_chooser_args args;
        args.needs_path = false;
        args.top_prompt = "Select a monster, or hit Escape to use default.";
        direction(moves, args);
        if (monster_at(moves.target))
            mon = clone_mons(monster_at(moves.target), true);
    }

    if (!mon)
    {
        if (mtype == MONS_PROGRAM_BUG)
        {
            char specs[100];
            mpr("Enter monster name (or MONS spec): ", MSGCH_PROMPT);
            if (cancelable_get_line_autohist(specs, sizeof specs) || !*specs)
            {
                canned_msg(MSG_OK);
                return NULL;
            }
            mtype = get_monster_by_name(specs);
        }

        mon = create_monster(
            mgen_data::hostile_at(mtype, "fightsim", false, 0, 0, you.pos(),
                                  MG_DONT_COME));
        if (!mon)
        {
            mprf("Failed to create monster.");
            return NULL;
        }
    }

    // move the monster next to the player
    // this probably works best in the arena, or at least somewhere
    // where there's no water or anything weird to interfere
    if (!adjacent(mon->pos(), you.pos()))
        for (adjacent_iterator ai(you.pos()); ai; ++ai)
            if (mon->move_to_pos(*ai))
                break;

    if (!adjacent(mon->pos(), you.pos()))
    {
        monster_die(mon, KILL_DISMISSED, NON_MONSTER);
        mprf("Could not put monster adjacent to player.");
        return 0;
    }

    // prevent distracted stabbing
    mon->foe = MHITYOU;
    // this line is actually kind of important for distortion now
    mon->hit_points = mon->max_hit_points = MAX_MONSTER_HP;
    mon->behaviour = BEH_SEEK;

    return mon;
}

static void _uninit_fsim(monster *mon)
{
    monster_die(mon, KILL_DISMISSED, NON_MONSTER);
    reset_training();
}

static fight_data _get_fight_data(monster &mon, int iter_limit, bool defend)
{
    const monster orig = mon;
    unsigned int cumulative_damage = 0;
    unsigned int time_taken = 0;
    int hits = 0;
    fight_data fdata;
    fdata.max_dam = 0;

    const int weapon = you.equip[EQ_WEAPON];
    const item_def *iweap = weapon != -1 ? &you.inv[weapon] : NULL;
    const int missile = you.m_quiver->get_fire_item();

    // now make sure the player is ready
    you.exp_available = 0;
    const int yhp  = you.hp;
    const int ymhp = you.hp_max;

    // disable death and delay, but make sure that these values
    // get reset when the function call ends
    unwind_var<FixedBitArray<NUM_DISABLEMENTS> > disabilities(crawl_state.disables);
    crawl_state.disables.set(DIS_DEATH);
    crawl_state.disables.set(DIS_DELAY);

    no_messages mx;
    const int hunger = you.hunger;

    if(!defend) // you're the attacker
    {
        for (int i = 0; i < iter_limit; i++)
        {
            mon = orig;
            mon.hit_points = mon.max_hit_points;
            mon.move_to_pos(mon.pos());
            you.time_taken = player_speed();

            // first, ranged weapons. note: this includes
            // being empty-handed but having a missile quivered
            if ((iweap && iweap->base_type == OBJ_WEAPONS &&
                        is_range_weapon(*iweap))
                || (!iweap && (missile != -1)))
            {
                bolt beam;
                // throw_it() will decrease quantity by 1
                inc_inv_item_quantity(missile, 1);
                beam.target = mon.pos();
                beam.animate = false;
                if (throw_it(beam, missile, false, DEBUG_COOKIE))
                    hits++;
            }
            else // otherwise, melee combat
            {
                bool did_hit = false;
                fight_melee(&you, &mon, &did_hit);
                if (did_hit)
                    hits++;
            }
            you.hunger = hunger;
            time_taken += you.time_taken;

            int damage = (mon.max_hit_points - mon.hit_points);
            cumulative_damage += damage;
            if (damage > fdata.max_dam)
                fdata.max_dam = damage;
        }
    }
    else // you're defending
    {
        for (int i = 0; i < iter_limit; i++)
        {
            you.hp = you.hp_max = 999; // again, arbitrary
            fight_melee(&mon, &you);

            time_taken += (100 / mon.speed);

            int damage = you.hp_max - you.hp;
            if (damage)
                hits++;
            cumulative_damage += damage;
            if (damage > fdata.max_dam)
                fdata.max_dam = damage;
        }
        you.hp = yhp;
        you.hp_max = ymhp;
    }

    fdata.av_hit_dam = hits ? double(cumulative_damage) / hits : 0.0;
    fdata.accuracy = 100 * hits / iter_limit;
    fdata.av_dam = double(cumulative_damage) / iter_limit;
    fdata.av_time = double(time_taken) / double(iter_limit) / 10.0;
    fdata.av_eff_dam = fdata.av_dam / fdata.av_time;

    return fdata;
}

// this is the skeletal simulator call, and the one that's easily accessed
void wizard_quick_fsim()
{
    // we could declare this in the fight calls, but i'm worried that
    // the actual monsters that are made will be slightly different,
    // so it's safer to do it here.
    monster *mon = _init_fsim();
    if(!mon)
        return;

    const int iter_limit = Options.fsim_rounds;
    fight_data fdata = _get_fight_data(*mon, iter_limit, false);
    mprf("           %s\n"
         "Attacking: %s",
        _dummy_string.c_str(),
        _fight_string(fdata).c_str());

    fdata = _get_fight_data(*mon, iter_limit, true);
    mprf("Defending: %s",
        _fight_string(fdata).c_str());

    _uninit_fsim(mon);
    return;
}

static void _fsim_simple_scale(FILE * o, monster* mon, bool defense)
{
    skill_type sk = defense ? SK_ARMOUR : _equipped_skill();
    fprintf(o, "%10.10s | %s\n",
            skill_name(sk),
            _dummy_string.c_str());

    const int iter_limit = Options.fsim_rounds;
    for(int i = 0; i <= 27; i++)
    {
        mesclr();
        mprf("Calculating average damage at %s %d...",
             skill_name(sk), i);

        set_skill_level(sk, i);
        fight_data fdata = _get_fight_data(*mon, iter_limit, defense);
        fprintf(o, "        %2d | %s\n", i, _fight_string(fdata).c_str());
        fflush(o);

        // kill the loop if the user hits escape
        if (kbhit() && getchk() == 27)
        {
            mprf("Cancelling simulation.\n");
            fprintf(o, "Simulation cancelled!\n\n");
            break;
        }
    }
}

static void _fsim_double_scale(FILE * o, monster* mon, bool defense)
{
    skill_type skx, sky;
    if (defense)
    {
        skx = SK_ARMOUR;
        sky = SK_DODGING;
    }
    else
    {
        skx = SK_FIGHTING;
        sky = _equipped_skill();
    }

    fprintf(o, "%s(x) vs %s(y)\n", skill_name(skx), skill_name(sky));
    fprintf(o, "  ");
    for(int y=0; y<27; y+=2)
        fprintf(o,"   %2d", y);

    fprintf(o,"\n");

    const int iter_limit = Options.fsim_rounds;
    for(int y=0; y<27; y+=2)
    {
        fprintf(o, "%2d", y);
        for(int x=0; x<27; x+=2)
        {
            mesclr();
            mprf("%s %d, %s %d...", skill_name(skx), x, skill_name(sky), y);
            set_skill_level(skx, x);
            set_skill_level(sky, y);
            fight_data fdata = _get_fight_data(*mon, iter_limit, defense);
            fprintf(o,"%5.1f", fdata.av_eff_dam);
            fflush(o);

            // kill the loop if the user hits escape
            if (kbhit() && getchk() == 27)
            {
                mprf("Cancelling simulation.\n");
                fprintf(o, "\nSimulation cancelled!\n\n");
                return;
            }
        }
        fprintf(o,"\n");
    }
}

void wizard_fight_sim(bool double_scale)
{
    monster * mon = _init_fsim();
    if(!mon)
        return;

    bool defense = false;
    const char * fightstat = "fight.stat";

    FILE * o = fopen(fightstat, "a");
    if (!o)
    {
        mprf(MSGCH_ERROR, "Can't write %s: %s", fightstat, strerror(errno));
        return;
    }

    _write_version(o);
    _write_matchup(o, *mon, defense, Options.fsim_rounds);
    _write_you(o);
    _write_weapon(o);
    _write_mon(o, *mon);
    fprintf(o,"\n");

    // this block of stuff is useful whenever you're going to change skill data
    // but don't want it to be saved (no guarantees that everything will be
    // the same, but things should still be playable). it resets your skills
    // after this function ends (during the destructor).
    unwind_var<FixedVector<uint8_t, NUM_SKILLS> > skills(you.skills);
    unwind_var<FixedVector<unsigned int, NUM_SKILLS> > skill_points(you.skill_points);
    unwind_var<std::list<skill_type> > exercises(you.exercises);
    unwind_var<std::list<skill_type> > exercises_all(you.exercises_all);
    unwind_var<FixedVector<int8_t, NUM_STATS> > stats(you.base_stats);
    unwind_var<int> xp(you.experience_level);

    // disable death and delay, but make sure that these values
    // get reset when the function call ends
    unwind_var<FixedBitArray<NUM_DISABLEMENTS> > disabilities(crawl_state.disables);
    crawl_state.disables.set(DIS_DEATH);
    crawl_state.disables.set(DIS_DELAY);

    if (double_scale)
        _fsim_double_scale(o, mon, defense);
    else
        _fsim_simple_scale(o, mon, defense);

    fprintf(o, "-----------------------------------\n\n");
    fclose(o);
    _uninit_fsim(mon);
    mprf("Done.");
}

#endif
