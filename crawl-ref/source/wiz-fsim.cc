/**
 * @file
 * @brief Fight simualtion wizard functions.
**/

#include "AppHdr.h"
#include "bitary.h"

#include "wiz-fsim.h"

#include <errno.h>

#include "beam.h"
#include "coordit.h"
#include "dbg-util.h"
#include "directn.h"
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
#include "state.h"
#include "stuff.h"
#include "throw.h"
#include "unwind.h"
#include "version.h"
#include "wiz-you.h"

#ifdef WIZARD

fight_data null_fight = {0.0, 0, 0, 0.0, 0, 0.0, 0.0};
typedef map<skill_type, int8_t> skill_map;
typedef map<skill_type, int8_t>::iterator skill_map_iterator;

static const char* _title_line =
    "AvHitDam | MaxDam | Accuracy | AvDam | AvTime | AvSpeed | AvEffDam"; // 64 columns

static const string _fight_string(fight_data fdata)
{
    return make_stringf("   %5.1f |    %3d |     %3d%% |"
                        " %5.1f |   %3d  | %5.2f |    %5.1f",
                        fdata.av_hit_dam, fdata.max_dam, fdata.accuracy,
                        fdata.av_dam, fdata.av_time, fdata.av_speed,
                        fdata.av_eff_dam);
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

static string _equipped_weapon_name()
{
    const int weapon = you.equip[EQ_WEAPON];
    const item_def * iweap = weapon != -1 ? &you.inv[weapon] : NULL;
    const int missile = you.m_quiver->get_fire_item();

    if (iweap)
    {
        string item_buf = iweap->name(DESC_PLAIN, true);
        // If it's a ranged weapon, add the description of the missile
        if (is_range_weapon(*iweap) && missile < ENDOFPACK && missile >= 0)
                item_buf += " with " + you.inv[missile].name(DESC_PLAIN);
        return "Wielding: " + item_buf;
    }

    if (missile != -1)
        return "Quivering: " + you.inv[missile].name(DESC_PLAIN);

    return "Unarmed";
}

static string _time_string()
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
    return "";
}

static void _write_version(FILE * o)
{
    fprintf(o, CRAWL " version %s\n", Version::Long);
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
    fprintf(o, "%s, Skill: %s\n",
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

static bool _fsim_kit_equip(const string &kit)
{
    string::size_type ammo_div = kit.find("/");
    string weapon = kit;
    string missile;
    if (ammo_div != string::npos)
    {
        weapon = kit.substr(0, ammo_div);
        missile = kit.substr(ammo_div + 1);
        trim_string(weapon);
        trim_string(missile);
    }

    if (!weapon.empty())
    {
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if (!you.inv[i].defined())
                continue;

            if (you.inv[i].name(DESC_PLAIN).find(weapon) != string::npos)
            {
                if (i != you.equip[EQ_WEAPON])
                {
                    wield_weapon(true, i, false);
                    if (i != you.equip[EQ_WEAPON])
                        return false;
                }
                break;
            }
        }
    }
    else if (you.weapon())
        unwield_item(false);

    if (!missile.empty())
    {
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if (!you.inv[i].defined())
                continue;

            if (you.inv[i].name(DESC_PLAIN).find(missile) != string::npos)
            {
                quiver_item(i);
                break;
            }
        }
    }

    return true;
}

// fight simulator internals
static monster* _init_fsim()
{
    monster * mon = NULL;
    monster_type mtype = get_monster_by_name(Options.fsim_mons, true);

    if (mtype == MONS_PROGRAM_BUG && monster_nearby())
    {
        // get a monster via targetting.
        dist moves;
        direction_chooser_args args;
        args.needs_path = false;
        args.top_prompt = "Select a monster, or hit Escape to use default.";
        direction(moves, args);
        if (monster_at(moves.target))
        {
            mon = clone_mons(monster_at(moves.target), true);
            if (mon)
                mon->flags |= MF_HARD_RESET;
        }
    }

    if (!mon)
    {
        if (mtype == MONS_PROGRAM_BUG)
        {
            char specs[100];
            mprf(MSGCH_PROMPT, "Enter monster name (or MONS spec): ");
            if (cancellable_get_line_autohist(specs, sizeof specs) || !*specs)
            {
                canned_msg(MSG_OK);
                return NULL;
            }
            mtype = get_monster_by_name(specs, true);

            // Wizmode users should be able to conjure up uniques even if they
            // were already created.
            if (mons_is_unique(mtype) && you.unique_creatures[mtype])
                you.unique_creatures.set(mtype, false);
        }

        mgen_data temp = mgen_data::hostile_at(mtype, "fightsim", false, 0, 0,
                                               you.pos(), MG_DONT_COME);

        temp.extra_flags |= MF_HARD_RESET;
        mon = create_monster(temp);
        if (!mon)
        {
            mpr("Failed to create monster.");
            return NULL;
        }
    }

    // move the monster next to the player
    // this probably works best in the arena, or at least somewhere
    // where there's no water or anything weird to interfere
    if (!adjacent(mon->pos(), you.pos()))
    {
        for (adjacent_iterator ai(you.pos()); ai; ++ai)
            if (mon->move_to_pos(*ai))
                break;
    }

    if (!adjacent(mon->pos(), you.pos()))
    {
        monster_die(mon, KILL_DISMISSED, NON_MONSTER);
        mpr("Could not put monster adjacent to player.");
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
    unwind_var<FixedBitVector<NUM_DISABLEMENTS> > disabilities(crawl_state.disables);
    crawl_state.disables.set(DIS_DEATH);
    crawl_state.disables.set(DIS_DELAY);

    no_messages mx;
    const int hunger = you.hunger;

    const coord_def start_pos = mon.pos();

    if (!defend) // you're the attacker
    {
        for (int i = 0; i < iter_limit; i++)
        {
            // This sets mgrid(mons.pos()) to NON_MONSTER
            mon = orig;
            // Re-place the monster if it e.g. blinked away.
            mon.move_to_pos(start_pos);
            mon.hit_points = mon.max_hit_points;
            mon.shield_blocks = 0;
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
                //Don't leave stacks of ammo
                you.inv[missile].flags |= ISFLAG_SUMMONED;
                beam.target = mon.pos();
                beam.animate = false;
                beam.dont_stop_player = true;
                if (throw_it(beam, missile, false, DEBUG_COOKIE))
                    hits++;
                you.inv[missile].flags &= ~ISFLAG_SUMMONED;
            }
            else // otherwise, melee combat
            {
                bool did_hit = false;
                fight_melee(&you, &mon, &did_hit, true);
                if (did_hit)
                    hits++;
            }
            you.hunger = hunger;
            time_taken += you.time_taken * 10;

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
            bool did_hit = false;
            you.shield_blocks = 0; // no blocks this round
            fight_melee(&mon, &you, &did_hit, true);

            time_taken += 1000 / (mon.speed ? mon.speed : 10);

            int damage = you.hp_max - you.hp;
            if (did_hit)
                hits++;
            cumulative_damage += damage;
            if (damage > fdata.max_dam)
                fdata.max_dam = damage;

            // Re-place the monster if it e.g. blinked away.
            mon.move_to_pos(start_pos);
        }
        you.hp = yhp;
        you.hp_max = ymhp;
    }

    fdata.av_hit_dam = hits ? double(cumulative_damage) / hits : 0.0;
    fdata.accuracy = 100 * hits / iter_limit;
    fdata.av_dam = double(cumulative_damage) / iter_limit;
    fdata.av_time = double(time_taken) / iter_limit + 0.5; // round to nearest
    fdata.av_speed = double(iter_limit) * 100 / time_taken;
    fdata.av_eff_dam = fdata.av_dam * 100 / fdata.av_time;

    return fdata;
}

// this is the skeletal simulator call, and the one that's easily accessed
void wizard_quick_fsim()
{
    // we could declare this in the fight calls, but i'm worried that
    // the actual monsters that are made will be slightly different,
    // so it's safer to do it here.
    monster *mon = _init_fsim();
    if (!mon)
        return;

    const int iter_limit = Options.fsim_rounds;
    fight_data fdata = _get_fight_data(*mon, iter_limit, false);
    mprf("           %s\nAttacking: %s", _title_line,
         _fight_string(fdata).c_str());

    fdata = _get_fight_data(*mon, iter_limit, true);
    mprf("Defending: %s", _fight_string(fdata).c_str());

    _uninit_fsim(mon);
    return;
}

static string _init_scale(skill_map &scale, bool &xl_mode)
{
    string ret;

    for (int i = 0, size = Options.fsim_scale.size(); i < size; ++i)
    {
        string sk_str = lowercase_string(Options.fsim_scale[i]);
        if (sk_str == "xl")
        {
            xl_mode = true;
            ret = "XL";
            continue;
        }

        int divider = 1;
        skill_type sk;

        string::size_type sep = sk_str.find("/");
        if (sep == string::npos)
            sep = sk_str.find(":");
        if (sep != string::npos)
        {
            string divider_str = sk_str.substr(sep + 1);
            sk_str = sk_str.substr(0, sep);
            trim_string(sk_str);
            trim_string(divider_str);
            divider = atoi(divider_str.c_str());
        }

        if (sk_str == "weapon")
            sk = _equipped_skill();
        else
            sk = skill_from_name(sk_str.c_str());

        scale[sk] = divider;
        if (divider == 1 && ret.empty())
            ret = skill_name(sk);
    }

    if (xl_mode)
    {
        you.training.init(0);
        for (skill_map_iterator it = scale.begin(); it != scale.end(); ++it)
            you.training[it->first] = it->second;
    }

    return ret;
}

static void _fsim_simple_scale(FILE * o, monster* mon, bool defense)
{
    skill_map scale;
    bool xl_mode = false;
    string col_name;

    if (Options.fsim_scale.empty())
    {
        skill_type sk = defense ? SK_ARMOUR : _equipped_skill();
        scale[sk] = 1;
        col_name = skill_name(sk);
    }
    else
        col_name = _init_scale(scale, xl_mode);

    const char* title = make_stringf("%10.10s | %s", col_name.c_str(),
                                     _title_line).c_str();
    fprintf(o, "%s\n", title);
    mpr(title);

    const int iter_limit = Options.fsim_rounds;
    for (int i = xl_mode ? 1 : 0; i <= 27; i++)
    {
        mesclr();

        if (xl_mode)
            set_xl(i, true);
        else
            for (skill_map_iterator it = scale.begin(); it != scale.end(); ++it)
                set_skill_level(it->first, i / it->second);

        fight_data fdata = _get_fight_data(*mon, iter_limit, defense);
        const string line = make_stringf("        %2d | %s", i,
                                         _fight_string(fdata).c_str());
        mpr(line);
        fprintf(o, "%s\n", line.c_str());
        fflush(o);

        // kill the loop if the user hits escape
        if (kbhit() && getchk() == 27)
        {
            mpr("Cancelling simulation.\n");
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
    for (int y = 1; y <= 27; y += 2)
        fprintf(o,"   %2d", y);

    fprintf(o,"\n");

    const int iter_limit = Options.fsim_rounds;
    for (int y = 1; y <= 27; y += 2)
    {
        fprintf(o, "%2d", y);
        for (int x = 1; x <= 27; x += 2)
        {
            mesclr();
            set_skill_level(skx, x);
            set_skill_level(sky, y);
            fight_data fdata = _get_fight_data(*mon, iter_limit, defense);
            mprf("%s %d, %s %d: %d", skill_name(skx), x, skill_name(sky), y,
                 int(fdata.av_eff_dam));
            fprintf(o,"%5.1f", fdata.av_eff_dam);
            fflush(o);

            // kill the loop if the user hits escape
            if (kbhit() && getchk() == 27)
            {
                mpr("Cancelling simulation.\n");
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
    if (!mon)
        return;

    bool defense = false;
    const char * fightstat = "fight.stat";

    FILE * o = fopen(fightstat, "a");
    if (!o)
    {
        mprf(MSGCH_ERROR, "Can't write %s: %s", fightstat, strerror(errno));
        _uninit_fsim(mon);
        return;
    }

    if (Options.fsim_mode.find("defen") != string::npos)
        defense = true;
    else if (Options.fsim_mode.find("attack") != string::npos
             || Options.fsim_mode.find("offen") != string::npos)
    {
        defense = false;
    }
    else
    {
        mprf(MSGCH_PROMPT, "(A)ttack or (D)efense?");

        switch (toalower(getchk()))
        {
        case 'a':
        case 'A':
            defense = false;
            break;
        case 'd':
        case 'D':
            defense = true;
            break;
        default:
            canned_msg(MSG_OK);
            _uninit_fsim(mon);
            return;
        }
    }

    _write_version(o);
    _write_matchup(o, *mon, defense, Options.fsim_rounds);
    _write_you(o);
    _write_weapon(o);
    _write_mon(o, *mon);
    fprintf(o,"\n");

    skill_state skill_backup;
    skill_backup.save();
    int xl = you.experience_level;

    // disable death and delay, but make sure that these values
    // get reset when the function call ends
    unwind_var<FixedBitVector<NUM_DISABLEMENTS> > disabilities(crawl_state.disables);
    crawl_state.disables.set(DIS_DEATH);
    crawl_state.disables.set(DIS_DELAY);

    void (*fsim_proc)(FILE * o, monster* mon, bool defense) = NULL;
    fsim_proc = double_scale ? _fsim_double_scale : _fsim_simple_scale;

    if (Options.fsim_kit.empty())
        fsim_proc(o, mon, defense);
    else
        for (int i = 0, size = Options.fsim_kit.size(); i < size; ++i)
        {
            if (_fsim_kit_equip(Options.fsim_kit[i]))
            {
                _write_weapon(o);
                fsim_proc(o, mon, defense);
                fprintf(o, "\n");
            }
            else
            {
                mprf("Aborting sim on %s", Options.fsim_kit[i].c_str());
                break;
            }
        }

    fprintf(o, "-----------------------------------\n\n");
    fclose(o);

    skill_backup.restore_levels();
    skill_backup.restore_training();
    if (you.experience_level != xl)
        set_xl(xl, false);

    _uninit_fsim(mon);
    mpr("Done.");
}

#endif
