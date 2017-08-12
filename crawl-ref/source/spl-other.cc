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
#include "god-companions.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-place.h"
#include "mon-util.h"
#include "place.h"
#include "player-stats.h"
#include "potion.h"
#include "religion.h"
#include "spl-util.h"
#include "terrain.h"

spret_type cast_sublimation_of_blood(int pow, bool fail)
{
    bool success = false;

    if (you.duration[DUR_DEATHS_DOOR])
        mpr("당신은 죽음의 문턱에 머물러 있는 동안에는 당신의 원기를 이끌어낼 수 없다.");
    else if (!you.can_bleed())
    {
        if (you.species == SP_VAMPIRE)
            mpr("당신의 원기를 이끌어 내기엔 당신의 몸에 피가 부족하다.");
        else
            mpr("당신의 몸엔 피가 하나도 없다.");
    }
    else if (!enough_hp(2, true))
        mpr("당신의 몸에서 힘을 끌어내려한 시도는 실패로 돌아갔다.");
    else
    {
        // Take at most 90% of currhp.
        const int minhp = max(div_rand_round(you.hp, 10), 1);

        while (you.magic_points < you.max_magic_points && you.hp > minhp)
        {
            fail_check();
            success = true;

            inc_mp(1);
            dec_hp(1, false);

            for (int i = 0; i < (you.hp > minhp ? 3 : 0); ++i)
                if (x_chance_in_y(6, pow))
                    dec_hp(1, false);

            if (x_chance_in_y(6, pow))
                break;
        }
        if (success)
            mpr("당신은 자신의 몸에서 마법적 원기를 이끌어냈다!");
        else
            mpr("당신의 몸에서 힘을 끌어내려한 시도는 실패로 돌아갔다.");
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
    mpr("악의에 찬 힘이 당신에게 스며들어, 그 방출을 기다리고 있다.");

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
        if (!mons_is_recallable(&you, **mi))
            continue;

        if (type == RECALL_YRED)
        {
            if (!(mi->holiness() & MH_UNDEAD))
                continue;
        }
        else if (type == RECALL_BEOGH)
        {
            if (!is_orcish_follower(**mi))
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
        mpr("당신은 동료를 부르기 시작했다.");
    }
    else
        mpr("아무 것도 당신의 부름에 답하지 않았다.");
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
    if (foe && !you.can_see(*foe))
        mons->foe = MHITYOU;
}

// Attempt to recall a single monster by mid, which might be either on or off
// our current level. Returns whether this monster was successfully recalled.
bool try_recall(mid_t mid)
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
            if (find_habitable_spot_near(you.pos(), mons_base_type(*mons), 3, false, empty)
                && mons->move_to_pos(empty))
            {
                recall_orders(mons);
                simple_monster_message(*mons, " is recalled.");
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
        if (try_recall(mid))
        {
            time -= you.attribute[ATTR_NEXT_RECALL_TIME];
            you.attribute[ATTR_NEXT_RECALL_TIME] = 3 + random2(4);
        }
        if ((unsigned int)you.attribute[ATTR_NEXT_RECALL_INDEX] >
             you.recall_list.size())
        {
            end_recall();
            mpr("당신은 동료를 부르는 것을 마쳤다.");
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
    coord_def dest;
    for (dest = you.pos() + delta;
         in_bounds(dest) && _feat_is_passwallable(grd(dest));
         dest += delta)
    {}

    int walls = (dest - you.pos()).rdist() - 1;
    if (walls == 0)
    {
        mpr("그 벽은 통과할 수 없다.");
        return SPRET_ABORT;
    }

    fail_check();

    // Below here, failing to cast yields information to the
    // player, so we don't make the spell abort (return SPRET_SUCCESS).
    const monster *mon = monster_at(dest);
    if (!in_bounds(dest))
        mpr("당신은 압도적인 바위의 크기를 느꼈다.");
    else if (cell_is_solid(dest) || (mon && mon->is_stationary()))
        mpr("무언가가 바위로 당신의 길을 막고 있다.");
    else if (walls > spell_range(SPELL_PASSWALL, pow))
        mpr("이 바위는 정말 깊게 묻혀 있는 것 같다.");
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
            start_delay<PasswallDelay>(1 + walls, dest);
        }
    }
    return SPRET_SUCCESS;
}

static int _intoxicate_monsters(coord_def where, int pow)
{
    monster* mons = monster_at(where);
    if (mons == nullptr
        || mons_intel(*mons) < I_HUMAN
        || !(mons->holiness() & MH_NATURAL)
        || mons->check_clarity(false)
        || monster_resists_this_poison(*mons))
    {
        return 0;
    }

    if (x_chance_in_y(40 + pow/3, 100))
    {
        mons->add_ench(mon_enchant(ENCH_CONFUSION, 0, &you));
        simple_monster_message(*mons, " looks rather confused.");
        return 1;
    }
    return 0;
}

spret_type cast_intoxicate(int pow, bool fail)
{
    fail_check();
    mpr("당신은 동료를 해독하려 시도했다!");
    int count = apply_area_visible([pow] (coord_def where) {
        return _intoxicate_monsters(where, pow);
    }, you.pos());
    if (count > 0)
    {
        if (x_chance_in_y(60 - pow/3, 100))
        {
            mprf(MSGCH_WARN, "The world spins around you!");
            you.increase_duration(DUR_VERTIGO, 4 + random2(20 + (100 - pow) / 10));
            you.redraw_evasion = true;
        }
    }

    return SPRET_SUCCESS;
}

spret_type cast_darkness(int pow, bool fail)
{
    fail_check();
    if (you.duration[DUR_DARKNESS])
        mprf(MSGCH_DURATION, "It gets a bit darker.");
    else
        mprf(MSGCH_DURATION, "It gets dark.");
    you.increase_duration(DUR_DARKNESS, 15 + random2(1 + pow/3), 100);
    update_vision_range();

    return SPRET_SUCCESS;
}
