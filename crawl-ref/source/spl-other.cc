/**
 * @file
 * @brief Non-enchantment spells that didn't fit anywhere else.
 *           Mostly Transmutations.
**/

#include "AppHdr.h"

#include "spl-other.h"

#include "act-iter.h"
#include "delay.h"
#include "env.h"
#include "food.h"
#include "godcompanions.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-place.h"
#include "place.h"
#include "player-stats.h"
#include "potion.h"
#include "religion.h"
#include "spl-util.h"
#include "terrain.h"

spret_type cast_cure_poison(int pow, bool fail)
{
    if (!you.duration[DUR_POISONING])
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_ABORT;
    }

    fail_check();
    reduce_player_poison((15 + roll_dice(3, pow / 2)) * 1000);

    // A message is already printed if we removed all of the poison
    if (you.duration[DUR_POISONING])
        mpr("The poison in your system diminishes.");

    return SPRET_SUCCESS;
}

spret_type cast_sublimation_of_blood(int pow, bool fail)
{
    bool success = false;

    if (you.duration[DUR_DEATHS_DOOR])
        mpr("You can't draw power from your own body while in Death's door.");
    else if (!you.can_bleed())
    {
        if (you.species == SP_VAMPIRE)
            mpr("You don't have enough blood to draw power from your own body.");
        else
            mpr("Your body is bloodless.");
    }
    else if (!enough_hp(2, true))
        mpr("Your attempt to draw power from your own body fails.");
    else
    {
        int food = 0;
        // Take at most 90% of currhp.
        const int minhp = max(div_rand_round(you.hp, 10), 1);

        while (you.magic_points < you.max_magic_points && you.hp > minhp
               && (you.undead_state() != US_SEMI_UNDEAD
                   || you.hunger - food >= HUNGER_SATIATED))
        {
            fail_check();
            success = true;

            inc_mp(1);
            dec_hp(1, false);

            if (you.undead_state() == US_SEMI_UNDEAD)
                food += 15;

            for (int i = 0; i < (you.hp > minhp ? 3 : 0); ++i)
                if (x_chance_in_y(6, pow))
                    dec_hp(1, false);

            if (x_chance_in_y(6, pow))
                break;
        }
        if (success)
            mpr("You draw magical energy from your own body!");
        else
            mpr("Your attempt to draw power from your own body fails.");

        make_hungry(food, false);
    }

    return success ? SPRET_SUCCESS : SPRET_ABORT;
}

spret_type cast_death_channel(int pow, god_type god, bool fail)
{
    if (you.duration[DUR_DEATH_CHANNEL] >= 60 * BASELINE_DELAY)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_ABORT;
    }

    fail_check();
    mpr("Malign forces permeate your being, awaiting release.");

    you.increase_duration(DUR_DEATH_CHANNEL, 30 + random2(1 + 2*pow/3), 200);

    if (god != GOD_NO_GOD)
        you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = static_cast<int>(god);

    return SPRET_SUCCESS;
}

spret_type cast_recall(bool fail)
{
    fail_check();
    start_recall(RECALL_SPELL);
    return SPRET_SUCCESS;
}

void start_recall(recall_t type)
{
    // Assemble the recall list.
    typedef pair<mid_t, int> mid_hd;
    vector<mid_hd> rlist;

    you.recall_list.clear();
    for (monster_iterator mi; mi; ++mi)
    {
        if (!mons_is_recallable(&you, *mi))
            continue;

        if (type == RECALL_YRED)
        {
            if (mi->holiness() != MH_UNDEAD)
                continue;
        }
        else if (type == RECALL_BEOGH)
        {
            if (!is_orcish_follower(*mi))
                continue;
        }

        mid_hd m(mi->mid, mi->get_experience_level());
        rlist.push_back(m);
    }

    if (type != RECALL_SPELL && branch_allows_followers(you.where_are_you))
        populate_offlevel_recall_list(rlist);

    if (!rlist.empty())
    {
        // Sort the recall list roughly
        for (mid_hd &entry : rlist)
            entry.second += random2(10);
        sort(rlist.begin(), rlist.end(), greater_second<mid_hd>());

        you.recall_list.clear();
        for (mid_hd &entry : rlist)
            you.recall_list.push_back(entry.first);

        you.attribute[ATTR_NEXT_RECALL_INDEX] = 1;
        you.attribute[ATTR_NEXT_RECALL_TIME] = 0;
        mpr("You begin recalling your allies.");
    }
    else
        mpr("Nothing appears to have answered your call.");
}

// Remind a recalled ally (or one skipped due to proximity) not to run
// away or wander off.
void recall_orders(monster *mons)
{
    // FIXME: is this okay for berserk monsters? We still want them to
    // stick around...

    // Don't patrol
    mons->patrol_point = coord_def(0, 0);

    // Don't wander
    mons->behaviour = BEH_SEEK;

    // Don't pursue distant enemies
    const actor *foe = mons->get_foe();
    if (foe && !you.can_see(foe))
        mons->foe = MHITYOU;
}

// Attempt to recall a single monster by mid, which might be either on or off
// our current level. Returns whether this monster was successfully recalled.
static bool _try_recall(mid_t mid)
{
    monster* mons = monster_by_mid(mid);
    // Either it's dead or off-level.
    if (!mons)
        return recall_offlevel_ally(mid);
    else if (mons->alive())
    {
        // Don't recall monsters that are already close to the player
        if (mons->pos().distance_from(you.pos()) < 3
            && mons->see_cell_no_trans(you.pos()))
        {
            recall_orders(mons);
            return false;
        }
        else
        {
            coord_def empty;
            if (find_habitable_spot_near(you.pos(), mons_base_type(mons), 3, false, empty)
                && mons->move_to_pos(empty))
            {
                recall_orders(mons);
                simple_monster_message(mons, " is recalled.");
                mons->apply_location_effects(mons->pos());
                // mons may have been killed, shafted, etc,
                // but they were still recalled!
                return true;
            }
        }
    }

    return false;
}

// Attempt to recall a number of allies proportional to how much time
// has passed. Once the list has been fully processed, terminate the
// status.
void do_recall(int time)
{
    while (time > you.attribute[ATTR_NEXT_RECALL_TIME])
    {
        // Try to recall an ally.
        mid_t mid = you.recall_list[you.attribute[ATTR_NEXT_RECALL_INDEX]-1];
        you.attribute[ATTR_NEXT_RECALL_INDEX]++;
        if (_try_recall(mid))
        {
            time -= you.attribute[ATTR_NEXT_RECALL_TIME];
            you.attribute[ATTR_NEXT_RECALL_TIME] = 3 + random2(4);
        }
        if ((unsigned int)you.attribute[ATTR_NEXT_RECALL_INDEX] >
             you.recall_list.size())
        {
            end_recall();
            mpr("You finish recalling your allies.");
            return;
        }
    }

    you.attribute[ATTR_NEXT_RECALL_TIME] -= time;
    return;
}

void end_recall()
{
    you.attribute[ATTR_NEXT_RECALL_INDEX] = 0;
    you.attribute[ATTR_NEXT_RECALL_TIME] = 0;
    you.recall_list.clear();
}

// Cast_phase_shift: raises evasion (by 8 currently) via Translocations.
spret_type cast_phase_shift(int pow, bool fail)
{
    if (you.duration[DUR_DIMENSION_ANCHOR])
    {
        mpr("You are anchored firmly to the material plane!");
        return SPRET_ABORT;
    }

    fail_check();
    if (!you.duration[DUR_PHASE_SHIFT])
        mpr("You feel the strange sensation of being on two planes at once.");
    else
        mpr("You feel the material plane grow further away.");

    you.increase_duration(DUR_PHASE_SHIFT, 5 + random2(pow), 30);
    you.redraw_evasion = true;
    return SPRET_SUCCESS;
}

static bool _feat_is_passwallable(dungeon_feature_type feat)
{
    // Worked stone walls are out, they're not diggable and
    // are used for impassable walls...
    switch (feat)
    {
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_CLEAR_ROCK_WALL:
        return true;
    default:
        return false;
    }
}

spret_type cast_passwall(const coord_def& delta, int pow, bool fail)
{
    int shallow = 1 + min(pow / 30, 3);      // minimum penetrable depth
    int range = shallow + random2(pow) / 25; // penetrable depth for this cast
    int maxrange = shallow + pow / 25;       // max penetrable depth

    coord_def dest;
    for (dest = you.pos() + delta;
         in_bounds(dest) && _feat_is_passwallable(grd(dest));
         dest += delta)
    {}

    int walls = (dest - you.pos()).rdist() - 1;
    if (walls == 0)
    {
        mpr("That's not a passable wall.");
        return SPRET_ABORT;
    }

    fail_check();

    // Below here, failing to cast yields information to the
    // player, so we don't make the spell abort (return true).
    monster *mon = monster_at(dest);
    if (!in_bounds(dest))
        mpr("You sense an overwhelming volume of rock.");
    else if (cell_is_solid(dest) || (mon && mon->is_stationary()))
        mpr("Something is blocking your path through the rock.");
    else if (walls > maxrange)
        mpr("This rock feels extremely deep.");
    else if (walls > range)
        mpr("You fail to penetrate the rock.");
    else
    {
        string msg;
        if (grd(dest) == DNGN_DEEP_WATER)
            msg = "You sense a large body of water on the other side of the rock.";
        else if (grd(dest) == DNGN_LAVA)
            msg = "You sense an intense heat on the other side of the rock.";

        if (check_moveto(dest, "passwall", msg))
        {
            // Passwall delay is reduced, and the delay cannot be interrupted.
            start_delay(DELAY_PASSWALL, 1 + walls, dest.x, dest.y);
        }
    }
    return SPRET_SUCCESS;
}

static int _intoxicate_monsters(coord_def where, int pow, int, actor *)
{
    monster* mons = monster_at(where);
    if (mons == nullptr
        || mons_intel(mons) < I_NORMAL
        || mons->holiness() != MH_NATURAL
        || mons->res_poison() > 0)
    {
        return 0;
    }

    if (x_chance_in_y(40 + pow/3, 100))
    {
        if (mons->check_clarity(false))
            return 1;
        mons->add_ench(mon_enchant(ENCH_CONFUSION, 0, &you));
        simple_monster_message(mons, " looks rather confused.");
        return 1;
    }
    return 0;
}

spret_type cast_intoxicate(int pow, bool fail)
{
    fail_check();
    mpr("You radiate an intoxicating aura.");
    if (x_chance_in_y(60 - pow/3, 100))
        confuse_player(3+random2(10 + (100 - pow) / 10));

    if (one_chance_in(20)
        && lose_stat(STAT_INT, 1 + random2(3), false,
                      "casting intoxication"))
    {
        mpr("Your head spins!");
    }

    apply_area_visible(_intoxicate_monsters, pow, &you);
    return SPRET_SUCCESS;
}

void remove_condensation_shield()
{
    mprf(MSGCH_DURATION, "Your icy shield evaporates.");
    you.duration[DUR_CONDENSATION_SHIELD] = 0;
    you.redraw_armour_class = true;
}

spret_type cast_condensation_shield(int pow, bool fail)
{
    if (you.shield())
    {
        mpr("You can't cast this spell while wearing a shield.");
        return SPRET_ABORT;
    }

    if (you.duration[DUR_FIRE_SHIELD])
    {
        mpr("Your ring of flames would instantly melt the ice.");
        return SPRET_ABORT;
    }

    fail_check();

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        mpr("The disc of vapour around you crackles some more.");
    else
        mpr("A crackling disc of dense vapour forms in the air!");
    you.increase_duration(DUR_CONDENSATION_SHIELD, 15 + random2(pow), 40);
    you.props[CONDENSATION_SHIELD_KEY] = pow;
    you.redraw_armour_class = true;

    return SPRET_SUCCESS;
}

spret_type cast_stoneskin(int pow, bool fail)
{
    if (you.form != TRAN_NONE
        && you.form != TRAN_APPENDAGE
        && you.form != TRAN_STATUE
        && you.form != TRAN_BLADE_HANDS)
    {
        mpr("This spell does not affect your current form.");
        return SPRET_ABORT;
    }

    if (you.duration[DUR_ICY_ARMOUR])
    {
        mpr("Turning your skin into stone would shatter your icy armour.");
        return SPRET_ABORT;
    }

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC)
    {
        // We can't get here from normal casting, and probably don't want
        // a message from the Helm card.
        // mpr("Your skin is naturally stony.");
        return SPRET_ABORT;
    }
#endif

    fail_check();

    if (you.duration[DUR_STONESKIN])
        mpr("Your skin feels harder.");
    else if (you.form == TRAN_STATUE)
        mpr("Your stone body feels more resilient.");
    else
        mpr("Your skin hardens.");

    if (you.attribute[ATTR_BONE_ARMOUR] > 0)
    {
        you.attribute[ATTR_BONE_ARMOUR] = 0;
        mpr("Your corpse armour falls away.");
    }

    you.increase_duration(DUR_STONESKIN, 10 + random2(pow) + random2(pow), 50);
    you.props[STONESKIN_KEY] = pow;
    you.redraw_armour_class = true;

    return SPRET_SUCCESS;
}

spret_type cast_darkness(int pow, bool fail)
{
    if (you.haloed())
    {
        mpr("It would have no effect in that bright light!");
        return SPRET_ABORT;
    }

    fail_check();
    if (you.duration[DUR_DARKNESS])
        mprf(MSGCH_DURATION, "It gets a bit darker.");
    else
        mprf(MSGCH_DURATION, "It gets dark.");
    you.increase_duration(DUR_DARKNESS, 15 + random2(1 + pow/3), 100);
    update_vision_range();

    return SPRET_SUCCESS;
}
