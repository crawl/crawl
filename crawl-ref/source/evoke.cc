/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/

#include "AppHdr.h"

#include "evoke.h"

#include <algorithm>
#include <cstdlib>
#include <string.h>

#include "externs.h"

#include "areas.h"
#include "artefact.h"
#include "cloud.h"
#include "coordit.h"
#include "decks.h"
#include "effects.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "food.h"
#include "invent.h"
#include "items.h"
#include "item_use.h"
#include "itemprop.h"
#include "libutil.h"
#include "losglobal.h"
#include "mapmark.h"
#include "melee_attack.h"
#include "message.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "misc.h"
#include "player-stats.h"
#include "godconduct.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-clouds.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "target.h"
#include "terrain.h"
#include "view.h"
#include "xom.h"

#ifdef USE_TILE
#include "tilepick.h"
#endif

void shadow_lantern_effect()
{
    // Currently only mummies and lich form get more shadows.
    if (x_chance_in_y(player_spec_death() + 1, 8))
    {
        create_monster(mgen_data(MONS_SHADOW, BEH_FRIENDLY, &you, 2, 0,
                                 you.pos(), MHITYOU));

        did_god_conduct(DID_NECROMANCY, 1);
    }
}

extern bool apply_berserk_penalty;

static bool _reaching_weapon_attack(const item_def& wpn)
{
    if (you.caught())
    {
        mprf("You cannot attack while %s.", held_status());
        return false;
    }

    bool targ_mid = false;
    dist beam;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_HOSTILE;
    args.range = 2;
    args.top_prompt = "Attack whom?";
    args.cancel_at_self = true;
    targetter_reach hitfunc(&you, REACH_TWO);
    args.hitfunc = &hitfunc;

    direction(beam, args);

    if (!beam.isValid)
    {
        if (beam.isCancel)
            canned_msg(MSG_OK);
        return false;
    }

    if (beam.isMe())
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return false;
    }

    if (you.confused())
        beam.confusion_fuzz(2);

    const coord_def delta = beam.target - you.pos();
    const int x_distance  = abs(delta.x);
    const int y_distance  = abs(delta.y);
    monster* mons = monster_at(beam.target);
    // don't allow targetting of submerged monsters (includes trapdoor spiders)
    if (mons && mons->submerged())
        mons = NULL;

    const int x_first_middle = you.pos().x + (delta.x)/2;
    const int y_first_middle = you.pos().y + (delta.y)/2;
    const int x_second_middle = beam.target.x - (delta.x)/2;
    const int y_second_middle = beam.target.y - (delta.y)/2;
    const coord_def first_middle(x_first_middle, y_first_middle);
    const coord_def second_middle(x_second_middle, y_second_middle);

    if (x_distance > 2 || y_distance > 2)
    {
        mpr("Your weapon cannot reach that far!");
        return false; // Shouldn't happen with confused swings
    }

    // Calculate attack delay now in case we have to apply it.
    melee_attack attk(&you, NULL);
    const int attack_delay = attk.calc_attack_delay();

    if (!feat_is_reachable_past(grd(first_middle))
        && !feat_is_reachable_past(grd(second_middle)))
    {
        // Might also be a granite statue/orcish idol which you
        // can reach _past_.
        if (you.confused())
        {
            mpr("You swing wildly and hit a wall.");
            you.time_taken = attack_delay;
            make_hungry(3, true);
            return true;
        }
        else
        {
            mpr("There's a wall in the way.");
            return false;
        }
    }

    // Failing to hit someone due to a friend blocking is infuriating,
    // shadow-boxing empty space is not (and would be abusable to wait
    // with no penalty).
    if (mons)
        apply_berserk_penalty = false;

    // Choose one of the two middle squares (which might be the same).
    const coord_def middle =
        (!feat_is_reachable_past(grd(first_middle)) ? second_middle :
         (!feat_is_reachable_past(grd(second_middle)) ? first_middle :
          (coinflip() ? first_middle : second_middle)));

    // Check for a monster in the way. If there is one, it blocks the reaching
    // attack 50% of the time, and the attack tries to hit it if it is hostile.

    // If we're attacking more than a space away...
    if (x_distance > 1 || y_distance > 1)
    {
        bool success = true;
        monster *midmons;
        if ((midmons = monster_at(middle))
            && !midmons->submerged())
        {
            // This chance should possibly depend on your skill with
            // the weapon.
            if (coinflip())
            {
                success = false;
                beam.target = middle;
                mons = midmons;
                targ_mid = true;
                if (mons->wont_attack())
                {
                    // Let's assume friendlies cooperate.
                    mpr("You could not reach far enough!");
                    you.time_taken = attack_delay;
                    make_hungry(3, true);
                    return true;
                }
            }
        }
        if (success)
            mpr("You reach to attack!");
        else
        {
            mprf("%s is in the way.",
                 mons->observable() ? mons->name(DESC_THE).c_str()
                                    : "Something you can't see");
        }
    }

    if (mons == NULL)
    {
        // Must return true, otherwise you get a free discovery
        // of invisible monsters.

        if (you.confused())
        {
            mprf("You swing wildly%s", beam.isMe() ?
                                       " and almost hit yourself!" : ".");
        }
        else
            mpr("You attack empty space.");
        you.time_taken = attack_delay;
        make_hungry(3, true);
        return true;
    }
    else if (!fight_melee(&you, mons))
    {
        if (targ_mid)
        {
            // turn_is_over may have been reset to false by fight_melee, but
            // a failed attempt to reach further should not be free; instead,
            // charge the same as a successful attempt.
            you.time_taken = attack_delay;
            make_hungry(3, true);
            you.turn_is_over = true;
        }
        else
        {
            canned_msg(MSG_OK);
            return false;
        }
    }

    return true;
}

static bool _evoke_horn_of_geryon(item_def &item)
{
    // Note: This assumes that the Vestibule has not been changed.
    bool rc = false;

    if (silenced(you.pos()))
    {
        mpr("You can't produce a sound!");
        return false;
    }
    else if (player_in_branch(BRANCH_VESTIBULE_OF_HELL))
    {
        mpr("You produce a weird and mournful sound.");

        if (you.char_direction == GDT_ASCENDING)
        {
            mpr("But nothing happens...");
            return false;
        }

        for (int count_x = 0; count_x < GXM; count_x++)
            for (int count_y = 0; count_y < GYM; count_y++)
            {
                if (grd[count_x][count_y] == DNGN_STONE_ARCH)
                {
                    rc = true;

                    map_marker *marker =
                        env.markers.find(coord_def(count_x, count_y),
                                         MAT_FEATURE);

                    if (marker)
                    {
                        map_feature_marker *featm =
                            dynamic_cast<map_feature_marker*>(marker);
                        // [ds] Ensure we're activating the correct feature
                        // markers. Feature markers are also used for other
                        // things, notably to indicate the return point from
                        // a labyrinth or portal vault.
                        switch (featm->feat)
                        {
                        case DNGN_ENTER_COCYTUS:
                        case DNGN_ENTER_DIS:
                        case DNGN_ENTER_GEHENNA:
                        case DNGN_ENTER_TARTARUS:
                            grd[count_x][count_y] = featm->feat;
                            env.markers.remove(marker);
                            item.plus2++;
                            break;
                        default:
                            break;
                        }
                    }
                }
            }

        if (rc)
            mpr("Your way has been unbarred.");
    }
    else
    {
        mpr("You produce a hideous howling noise!", MSGCH_SOUND);
        create_monster(
            mgen_data::hostile_at(MONS_HELL_BEAST, "the horn of Geryon",
                true, 4, 0, you.pos()));
    }
    return rc;
}

static bool _efreet_flask(int slot)
{
    bool friendly = x_chance_in_y(300 + you.skill(SK_EVOCATIONS, 10), 600);

    mpr("You open the flask...");

    monster *mons =
        create_monster(
            mgen_data(MONS_EFREET,
                      friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                      &you, 0, 0, you.pos(),
                      MHITYOU, MG_FORCE_BEH));

    if (mons)
    {
        mpr("...and a huge efreet comes out.");

        if (player_angers_monster(mons))
            friendly = false;

        if (silenced(you.pos()))
        {
            mpr(friendly ? "It nods graciously at you."
                         : "It snaps in your direction!", MSGCH_TALK_VISUAL);
        }
        else
        {
            mpr(friendly ? "\"Thank you for releasing me!\""
                         : "It howls insanely!", MSGCH_TALK);
        }
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    dec_inv_item_quantity(slot, 1);

    return true;
}

static bool _check_crystal_ball()
{
    if (you.intel() <= 1)
    {
        mpr("You lack the intelligence to focus on the shapes in the ball.");
        return false;
    }

    if (you.confused())
    {
        mpr("You are unable to concentrate on the shapes in the ball.");
        return false;
    }

    if (you.magic_points == you.max_magic_points)
    {
        mpr("With no energy to recover, the crystal ball of energy is "
            "presently useless to you.");
        return false;
    }

    if (you.skill(SK_EVOCATIONS) < 2)
    {
        mpr("You lack the skill to use this item.");
        return false;
    }

    return true;
}

bool disc_of_storms(bool drac_breath)
{
    const int fail_rate = 30 - you.skill(SK_EVOCATIONS);
    bool rc = false;

    if (x_chance_in_y(fail_rate, 100) && !drac_breath)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (x_chance_in_y(fail_rate, 100) && !drac_breath)
        mpr("The disc glows for a moment, then fades.");
    else if (x_chance_in_y(fail_rate, 100) && !drac_breath)
        mpr("Little bolts of electricity crackle over the disc.");
    else
    {
        if (!drac_breath)
            mpr("The disc erupts in an explosion of electricity!");
        rc = true;

        const int disc_count = (drac_breath) ? roll_dice(2, 1 + you.experience_level / 7) :
            roll_dice(2, 1 + you.skill_rdiv(SK_EVOCATIONS, 1, 7));

        for (int i = 0; i < disc_count; ++i)
        {
            bolt beam;
            const zap_type types[] = { ZAP_LIGHTNING_BOLT, ZAP_SHOCK,
                                       ZAP_ORB_OF_ELECTRICITY };

            const zap_type which_zap = RANDOM_ELEMENT(types);

            // range has no tracer, so randomness is ok
            beam.range = (drac_breath) ? you.experience_level / 3 + 5 :
                you.skill_rdiv(SK_EVOCATIONS, 1, 3) + 5; // 5--14
            beam.source = you.pos();
            beam.target = you.pos() + coord_def(random2(13)-6, random2(13)-6);
            int power = (drac_breath) ? 25 + you.experience_level : 30
                                           + you.skill(SK_EVOCATIONS, 2);
            // Non-controlleable, so no player tracer.
            zapping(which_zap, power, beam);

        }

        if (!drac_breath)
        {
            for (radius_iterator ri(you.pos(), LOS_RADIUS, false); ri; ++ri)
            {
                if (!in_bounds(*ri) || grd(*ri) < DNGN_MAXWALL)
                    continue;

                if (one_chance_in(60 - you.skill(SK_EVOCATIONS)))
                    place_cloud(CLOUD_RAIN, *ri,
                                random2(you.skill(SK_EVOCATIONS)), &you);
            }
        }
    }
    return rc;
}

void tome_of_power(int slot)
{
    if (you.form == TRAN_WISP)
    {
        crawl_state.zero_turns_taken();
        return mpr("You can't handle books in this form.");
    }

    const int powc = 5 + you.skill(SK_EVOCATIONS)
                       + roll_dice(5, you.skill(SK_EVOCATIONS));

    msg::stream << "The book opens to a page covered in "
                << weird_writing() << '.' << endl;

    you.turn_is_over = true;

    if (player_mutation_level(MUT_BLURRY_VISION) > 0
        && x_chance_in_y(player_mutation_level(MUT_BLURRY_VISION), 4))
    {
        mpr("The page is too blurry for you to read.");
        return;
    }

    mpr("You find yourself reciting the magical words!");
    practise(EX_WILL_READ_TOME);
    count_action(CACT_EVOKE, EVOC_MISC);

    if (x_chance_in_y(7, 50))
    {
        mpr("A cloud of weird smoke pours from the book's pages!");
        big_cloud(random_smoke_type(), &you, you.pos(), 20, 10 + random2(8));
        xom_is_stimulated(12);
    }
    else if (x_chance_in_y(2, 43))
    {
        mpr("A cloud of choking fumes pours from the book's pages!");
        big_cloud(CLOUD_POISON, &you, you.pos(), 20, 7 + random2(5));
        xom_is_stimulated(50);
    }
    else if (x_chance_in_y(2, 41))
    {
        mpr("A cloud of freezing gas pours from the book's pages!");
        big_cloud(CLOUD_COLD, &you, you.pos(), 20, 8 + random2(5));
        xom_is_stimulated(50);
    }
    else if (x_chance_in_y(3, 39))
    {
        if (one_chance_in(5))
        {
            mpr("The book disappears in a mighty explosion!");
            dec_inv_item_quantity(slot, 1);
        }

        immolation(15, IMMOLATION_TOME, you.pos(), false, &you);

        xom_is_stimulated(200);
    }
    else if (one_chance_in(36))
    {
        if (create_monster(
                mgen_data::hostile_at(MONS_ABOMINATION_SMALL,
                    "a tome of Destruction",
                    true, 6, 0, you.pos())))
        {
            mpr("A horrible Thing appears!");
            mpr("It doesn't look too friendly.");
        }
        xom_is_stimulated(200);
    }
    else
    {
        viewwindow();

        const int temp_rand =
            min(25, random2(23)
                    + random2(you.skill_rdiv(SK_EVOCATIONS, 1, 3)));

        const spell_type spell_casted =
            ((temp_rand > 24) ? SPELL_LEHUDIBS_CRYSTAL_SPEAR :
             (temp_rand > 21) ? SPELL_BOLT_OF_FIRE :
             (temp_rand > 18) ? SPELL_BOLT_OF_COLD :
             (temp_rand > 16) ? SPELL_LIGHTNING_BOLT :
             (temp_rand > 10) ? SPELL_FIREBALL :
             (temp_rand >  9) ? SPELL_VENOM_BOLT :
             (temp_rand >  8) ? SPELL_BOLT_OF_DRAINING :
             (temp_rand >  7) ? SPELL_BOLT_OF_INACCURACY :
             (temp_rand >  6) ? SPELL_STICKY_FLAME_RANGE :
             (temp_rand >  5) ? SPELL_TELEPORT_SELF :
             (temp_rand >  4) ? SPELL_DAZZLING_SPRAY :
             (temp_rand >  3) ? SPELL_POLYMORPH :
             (temp_rand >  2) ? SPELL_MEPHITIC_CLOUD :
             (temp_rand >  1) ? SPELL_THROW_FLAME :
             (temp_rand >  0) ? SPELL_THROW_FROST
                              : SPELL_MAGIC_DART);

        your_spells(spell_casted, powc, false);
    }
}

void stop_studying_manual(bool finish)
{
    const skill_type sk = you.manual_skill;
    if (finish)
    {
        mprf("You have finished your manual of %s and toss it away.",
             skill_name(sk));
        dec_inv_item_quantity(you.manual_index, 1);
    }
    else
        mprf("You stop studying %s.", skill_name(sk));

    you.manual_skill = SK_NONE;
    you.manual_index = -1;
    if (training_restricted(sk))
        you.stop_train.insert(sk);
}

void skill_manual(int slot)
{
    item_def& manual(you.inv[slot]);
    const bool known = item_type_known(manual);
    if (!known)
        set_ident_flags(manual, ISFLAG_KNOW_TYPE);
    const skill_type skill = static_cast<skill_type>(manual.plus);

    if (is_useless_skill(skill) || you.skills[skill] >= 27)
    {
        if (!known)
            mprf("This is a manual of %s.", skill_name(skill));
        mpr("You have no use for it.");
        return;
    }

    if (skill == you.manual_skill)
    {
        stop_studying_manual();
        you.turn_is_over = true;
        return;
    }

    if (!known)
    {
        string prompt = make_stringf("This is a manual of %s. Do you want "
                                     "to study it?", skill_name(skill));
        if (!yesno(prompt.c_str(), true, 'n'))
        {
            canned_msg(MSG_OK);
            return;
        }
    }

    if (!is_invalid_skill(you.manual_skill))
        stop_studying_manual();

    mprf("You start studying %s.", skill_name(skill));
    you.manual_skill = skill;
    you.manual_index = slot;
    you.start_train.insert(skill);
    you.turn_is_over = true;
}

static bool _box_of_beasts(item_def &box)
{
    bool success = false;

    mpr("You open the lid...");

    if (x_chance_in_y(60 + you.skill(SK_EVOCATIONS), 100))
    {
        const monster_type beasts[] = {
            MONS_BAT,       MONS_HOUND,     MONS_JACKAL,
            MONS_RAT,       MONS_ICE_BEAST, MONS_ADDER,
            MONS_YAK,       MONS_BUTTERFLY, MONS_WATER_MOCCASIN,
            MONS_CROCODILE, MONS_HELL_HOUND
        };

        monster_type mon = MONS_NO_MONSTER;

        // If you worship a good god, don't summon an unholy beast (in
        // this case, the hell hound).
        do
            mon = RANDOM_ELEMENT(beasts);
        while (player_will_anger_monster(mon));

        const bool friendly = !x_chance_in_y(100,
                                   you.skill(SK_EVOCATIONS, 100) + 500);

        if (create_monster(
                mgen_data(mon,
                          friendly ? BEH_FRIENDLY : BEH_HOSTILE, &you,
                          2 + random2(4), 0,
                          you.pos(),
                          MHITYOU)))
        {
            success = true;

            mpr("...and something leaps out!");
            xom_is_stimulated(10);
        }
    }
    else
    {
        if (!one_chance_in(6))
            mpr("...but nothing happens.");
        else
        {
            mpr("...but the box appears empty, and falls apart.");
            ASSERT(in_inventory(box));
            dec_inv_item_quantity(box.link, 1);
        }
    }

    return success;
}

static bool _ball_of_energy(void)
{
    bool ret = false;

    mpr("You gaze into the crystal ball.");

    int use = random2(you.skill(SK_EVOCATIONS, 6));

    if (use < 2)
        lose_stat(STAT_INT, 1 + random2avg(7, 2), false, "using a ball of energy");
    else if (use < 5 && enough_mp(1, true))
    {
        mpr("You feel your power drain away!");
        dec_mp(you.magic_points);
    }
    else if (use < 10)
        confuse_player(10 + random2(10));
    else
    {
        int proportional = (you.magic_points * 100) / you.max_magic_points;

        if (random2avg(77 - you.skill(SK_EVOCATIONS, 2), 4) > proportional
            || one_chance_in(25))
        {
            mpr("You feel your power drain away!");
            dec_mp(you.magic_points);
        }
        else
        {
            mpr("You are suffused with power!");
            inc_mp(5 + random2avg(you.skill(SK_EVOCATIONS), 2));

            ret = true;
        }
    }

    return ret;
}

static vector<coord_def> _get_jitter_path(coord_def source, coord_def target,
                                          bool jitter_start,
                                          bolt &beam1, bolt &beam2)
{
    const int NUM_TRIES = 10;
    const int RANGE = 8;

    bolt trace_beam;
    trace_beam.source = source;
    trace_beam.target = target;
    trace_beam.aimed_at_spot = false;
    trace_beam.is_tracer = true;
    trace_beam.range = RANGE;
    trace_beam.fire();

    coord_def aim_dir = (source - target).sgn();

    if (trace_beam.path_taken.back() != source)
        target = trace_beam.path_taken.back();

    if (jitter_start)
    {
        for (int n = 0; n < NUM_TRIES; ++n)
        {
            coord_def jitter = target + coord_def(random_range(-2, 2), random_range(-2, 2));
            if (jitter == target || jitter == source || feat_is_solid(grd(jitter)))
                continue;

            trace_beam.target = jitter;
            trace_beam.fire();

            coord_def delta = source - trace_beam.path_taken.back();
            //Don't try to aim at targets in the opposite direction of main aim
            if ((abs(aim_dir.x - delta.sgn().x) + abs(aim_dir.y - delta.sgn().y) >= 2)
                 && !delta.origin())
                continue;

            target = trace_beam.path_taken.back();
            break;
        }
    }

    vector<coord_def> path = trace_beam.path_taken;
    unsigned int mid_i = (path.size() / 2);
    coord_def mid = path[mid_i];

    for (int n = 0; n < NUM_TRIES; ++n)
    {
        coord_def jitter = mid + coord_def(random_range(-3, 3), random_range(-3, 3));
        if (jitter == mid || jitter.distance_from(mid) < 2 || jitter == source
            || feat_is_solid(grd(jitter))
            || !cell_see_cell(source, jitter, LOS_NO_TRANS)
            || !cell_see_cell(target, jitter, LOS_NO_TRANS))
            continue;

        trace_beam.aimed_at_feet = false;
        trace_beam.source = jitter;
        trace_beam.target = target;
        trace_beam.fire();

        coord_def delta1 = source - jitter;
        coord_def delta2 = source - trace_beam.path_taken.back();

        //Don't try to aim at targets in the opposite direction of main aim
        if (abs(aim_dir.x - delta1.sgn().x) + abs(aim_dir.y - delta1.sgn().y) >= 2
            || abs(aim_dir.x - delta2.sgn().x) + abs(aim_dir.y - delta2.sgn().y) >= 2)
        {
            continue;
        }

        // Don't make l-turns
        coord_def delta = jitter-target;
        if (!delta.x || !delta.y)
            continue;

        bool match = false;
        for (unsigned int i = 0; i < path.size(); ++i)
        {
            if (path[i] == jitter)
            {
                match = true;
                break;
            }
        }
        if (match)
            continue;

        mid = jitter;
        break;
    }

    beam1.source = source;
    beam1.target = mid;
    beam1.range = RANGE;
    beam1.aimed_at_spot = true;
    beam1.is_tracer = true;
    beam1.fire();
    beam1.is_tracer = false;

    beam2.source = mid;
    beam2.target = target;
    beam2.range = max(int(RANGE - beam1.path_taken.size()), mid.distance_from(target));
    beam2.is_tracer = true;
    beam2.fire();
    beam2.is_tracer = false;

    vector<coord_def> newpath;
    newpath.insert(newpath.end(), beam1.path_taken.begin(), beam1.path_taken.end());
    newpath.insert(newpath.end(), beam2.path_taken.begin(), beam2.path_taken.end());

    return newpath;
}

static bool _check_path_overlap(const vector<coord_def> &path1,
                                const vector<coord_def> &path2, int match_len)
{
    int max_len = min(path1.size(), path2.size());
    match_len = min(match_len, max_len-1);

    // Check for overlap with previous path
    int matchs = 0;
    for (int i = 0; i < max_len; ++i)
    {
        if (path1[i] == path2[i])
            ++matchs;
        else
            matchs = 0;

        if (matchs >= match_len)
            return true;
    }

    return false;
}

static bool _fill_flame_trails(coord_def source, coord_def target,
                               vector<bolt> &beams, vector<coord_def> &elementals,
                               int num)
{
    const int NUM_TRIES = 10;
    vector<vector<coord_def> > paths;
    for (int n = 0; n < num; ++n)
    {
        int tries = 0;
        vector<coord_def> path;
        bolt beam1, beam2;
        while (++tries <= NUM_TRIES && path.empty())
        {
            path = _get_jitter_path(source, target, !paths.empty(), beam1, beam2);
            for (unsigned int i = 0; i < paths.size(); ++i)
            {
                if (_check_path_overlap(path, paths[i], 3))
                {
                    path.clear();
                    beam1 = bolt();
                    beam2 = bolt();
                    break;
                }
            }
        }

        if (!path.empty())
        {
            paths.push_back(path);
            beams.push_back(beam1);
            beams.push_back(beam2);
            if (path.size() > 3)
                elementals.push_back(path.back());
        }
    }

    return (!paths.empty());
}

static bool _lamp_of_fire()
{
    bolt base_beam;
    dist target;

    const int pow = 12 + you.skill(SK_EVOCATIONS, 2);
    if (spell_direction(target, base_beam, DIR_TARGET, TARG_ANY, 8,
                        true, true, false, NULL,
                        "Aim the lamp in which direction?", true, NULL))
    {
        mpr("The flames dance!");

        vector<bolt> beams;
        vector<coord_def> elementals;
        int num_trails = 1;
        if (you.skill(SK_EVOCATIONS, 10) + random2(60) > 80)
            ++num_trails;
        if (you.skill(SK_EVOCATIONS, 10) + random2(60) > 130)
            ++num_trails;

        _fill_flame_trails(you.pos(), target.target, beams, elementals, num_trails);

        for (unsigned int n = 0; n < beams.size(); ++n)
        {
            if (beams[n].source == beams[n].target)
                continue;

            beams[n].flavour     = BEAM_FIRE;
            beams[n].colour      = RED;
            beams[n].beam_source = MHITYOU;
            beams[n].thrower     = KILL_YOU;
            beams[n].is_beam     = true;
            beams[n].name        = "trail of fire";
            beams[n].hit         = 10 + (pow/8);
            beams[n].damage      = dice_def(2, 4 + pow/4);
            beams[n].ench_power  = (pow/8);
            beams[n].fire();
        }

        for (unsigned int n = 0; n < elementals.size(); ++n)
        {
            mgen_data mg(MONS_FIRE_ELEMENTAL, BEH_FRIENDLY, &you, 3,
                         SPELL_NO_SPELL, elementals[n], 0,
                         MG_FORCE_BEH | MG_FORCE_PLACE, GOD_NO_GOD,
                         MONS_FIRE_ELEMENTAL, 0, BLACK, PROX_CLOSE_TO_PLAYER);
            mg.hd = 6 + (pow/14);
            create_monster(mg);
        }
    }

    return true;
}

bool evoke_item(int slot)
{
    if (you.form == TRAN_WISP)
        return mpr("You cannot handle anything in this form."), false;

    if (you.berserk() && (slot == -1
                       || slot != you.equip[EQ_WEAPON]
                       || weapon_reach(*you.weapon()) <= 2))
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (slot == -1)
    {
        slot = prompt_invent_item("Evoke which item? (* to show all)",
                                   MT_INVLIST,
                                   OSEL_EVOKABLE, true, true, true, 0, -1,
                                   NULL, OPER_EVOKE);

        if (prompt_failed(slot))
            return false;
    }
    else if (!check_warning_inscriptions(you.inv[slot], OPER_EVOKE))
        return false;

    ASSERT(slot >= 0);

#ifdef ASSERTS // Used only by an assert
    const bool wielded = (you.equip[EQ_WEAPON] == slot);
#endif /* DEBUG */

    item_def& item = you.inv[slot];
    // Also handles messages.
    if (!item_is_evokable(item, true, false, false, true))
        return false;

    if (you.suppressed() && weapon_reach(item) <= 2)
    {
        canned_msg(MSG_EVOCATION_SUPPRESSED);
        return false;
    }

    int pract = 0; // By how much Evocations is practised.
    bool did_work   = false;  // Used for default "nothing happens" message.
    bool unevokable = false;

    const unrandart_entry *entry = is_unrandom_artefact(item)
        ? get_unrand_entry(item.special) : NULL;

    if (entry && entry->evoke_func)
    {
        ASSERT(item_is_equipped(item));

        if (entry->evoke_func(&item, &pract, &did_work, &unevokable))
        {
            if (!unevokable)
                count_action(CACT_EVOKE, EVOC_MISC);
            return did_work;
        }
    }
    else switch (item.base_type)
    {
    case OBJ_WANDS:
        zap_wand(slot);
        return true;

    case OBJ_WEAPONS:
        ASSERT(wielded);

        if (weapon_reach(item) > 2)
        {
            if (_reaching_weapon_attack(item))
            {
                pract    = 0;
                did_work = true;
            }
            else
                return false;
        }
        else
            unevokable = true;
        break;

    case OBJ_RODS:
        ASSERT(wielded);

        if (you.confused())
        {
            canned_msg(MSG_TOO_CONFUSED);
            return false;
        }

        pract = rod_spell(slot);
        // [ds] Early exit, no turns are lost.
        if (pract == -1)
            return false;

        did_work = true;  // rod_spell() will handle messages
        count_action(CACT_EVOKE, EVOC_ROD);
        break;

    case OBJ_STAVES:
        ASSERT(wielded);
        if (item.sub_type != STAFF_ENERGY)
        {
            unevokable = true;
            break;
        }

        if (you.confused())
        {
            canned_msg(MSG_TOO_CONFUSED);
            return false;
        }

        if (!you.is_undead && !you_foodless()
            && you.hunger_state == HS_STARVING)
        {
            canned_msg(MSG_TOO_HUNGRY);
            return false;
        }
        else if (you.magic_points >= you.max_magic_points)
        {
            mpr("Your reserves of magic are already full.");
            return false;
        }
        else if (x_chance_in_y(you.skill(SK_EVOCATIONS, 100) + 1100, 4000))
        {
            mpr("You channel some magical energy.");
            inc_mp(1 + random2(3));
            make_hungry(50, false, true);
            pract = 1;
            did_work = true;
            count_action(CACT_EVOKE, EVOC_MISC);
        }
        break;

    case OBJ_MISCELLANY:
        did_work = true; // easier to do it this way for misc items

        if (is_deck(item))
        {
            ASSERT(wielded);

            evoke_deck(item);
            pract = 1;
            count_action(CACT_EVOKE, EVOC_DECK);
            break;
        }

        switch (item.sub_type)
        {
        case MISC_BOTTLED_EFREET:
            if (_efreet_flask(slot))
                pract = 2;
            break;

        case MISC_AIR_ELEMENTAL_FAN:
            if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100), 3000))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                cast_summon_elemental(100, GOD_NO_GOD, MONS_AIR_ELEMENTAL, 4, 3);
                pract = (one_chance_in(5) ? 1 : 0);
            }
            break;

        case MISC_LAMP_OF_FIRE:

            _lamp_of_fire();

            break;

        case MISC_STONE_OF_EARTH_ELEMENTALS:
            if (!x_chance_in_y(you.skill(SK_EVOCATIONS, 100), 3000))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                cast_summon_elemental(100, GOD_NO_GOD, MONS_EARTH_ELEMENTAL, 4, 3);
                pract = (one_chance_in(5) ? 1 : 0);
            }
            break;

        case MISC_HORN_OF_GERYON:
            if (_evoke_horn_of_geryon(item))
                pract = 1;
            break;

        case MISC_BOX_OF_BEASTS:
            if (_box_of_beasts(item))
                pract = 1;
            break;

        case MISC_CRYSTAL_BALL_OF_ENERGY:
            if (!_check_crystal_ball())
                unevokable = true;
            else if (_ball_of_energy())
                pract = 1;
            break;

        case MISC_DISC_OF_STORMS:
            if (disc_of_storms())
                pract = (coinflip() ? 2 : 1);
            break;

        case MISC_QUAD_DAMAGE:
            mpr("QUAD DAMAGE!");
            you.duration[DUR_QUAD_DAMAGE] = 30 * BASELINE_DELAY;
            ASSERT(in_inventory(item));
            dec_inv_item_quantity(item.link, 1);
            invalidate_agrid(true);
            break;

        default:
            did_work = false;
            unevokable = true;
            break;
        }
        if (did_work)
            count_action(CACT_EVOKE, EVOC_MISC);
        break;

    default:
        unevokable = true;
        break;
    }

    if (!did_work)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (pract > 0)
        practise(EX_DID_EVOKE_ITEM, pract);

    if (!unevokable)
        you.turn_is_over = true;
    else
        crawl_state.zero_turns_taken();

    return did_work;
}
