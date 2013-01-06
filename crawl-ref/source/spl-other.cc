/**
 * @file
 * @brief Non-enchantment spells that didn't fit anywhere else.
 *           Mostly Transmutations.
**/

#include "AppHdr.h"

#include "spl-other.h"
#include "externs.h"

#include "coord.h"
#include "delay.h"
#include "env.h"
#include "food.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-place.h"
#include "mon-util.h"
#include "player.h"
#include "player-stats.h"
#include "potion.h"
#include "religion.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"

spret_type cast_cure_poison(int pow, bool fail)
{
    if (!you.duration[DUR_POISONING])
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_ABORT;
    }

    fail_check();
    reduce_poison_player(2 + random2(pow) + random2(3));
    return SPRET_SUCCESS;
}

spret_type cast_sublimation_of_blood(int pow, bool fail)
{
    bool success = false;

    int wielded = you.equip[EQ_WEAPON];

    if (wielded != -1)
    {
        if (you.inv[wielded].base_type == OBJ_FOOD
            && you.inv[wielded].sub_type == FOOD_CHUNK)
        {
            fail_check();
            success = true;

            mpr("The chunk of flesh you are holding crumbles to dust.");

            mpr("A flood of magical energy pours into your mind!");

            inc_mp(5 + random2(2 + pow / 15));

            dec_inv_item_quantity(wielded, 1);

            if (mons_genus(you.inv[wielded].mon_type) == MONS_ORC)
                did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
            if (mons_class_holiness(you.inv[wielded].mon_type) == MH_HOLY)
                did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2);
        }
        else if (is_blood_potion(you.inv[wielded])
                 && item_type_known(you.inv[wielded]))
        {
            fail_check();
            success = true;

            mprf("The blood within %s froths and boils.",
                 you.inv[wielded].quantity > 1 ? "one of your flasks"
                                               : "the flask you are holding");

            mpr("A flood of magical energy pours into your mind!");

            inc_mp(5 + random2(2 + pow / 15));

            remove_oldest_blood_potion(you.inv[wielded]);
            dec_inv_item_quantity(wielded, 1);
        }
        else
            wielded = -1;
    }

    if (wielded == -1)
    {
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("A conflicting enchantment prevents the spell from "
                "coming into effect.");
        }
        else if (!you.can_bleed(false))
        {
            mpr("You don't have enough blood to draw power from your "
                "own body.");
        }
        else if (!enough_hp(2, true))
            mpr("Your attempt to draw power from your own body fails.");
        else
        {
            int food = 0;

            while (you.magic_points < you.max_magic_points && you.hp > 1
                   && (you.is_undead != US_SEMI_UNDEAD
                       || you.hunger - food >= 7000))
            {
                fail_check();
                success = true;

                inc_mp(1);
                dec_hp(1, false);

                if (you.is_undead == US_SEMI_UNDEAD)
                    food += 15;

                for (int loopy = 0; loopy < (you.hp > 1 ? 3 : 0); ++loopy)
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
    }

    return (success ? SPRET_SUCCESS : SPRET_ABORT);
}

spret_type cast_death_channel(int pow, god_type god, bool fail)
{
    if (you.duration[DUR_DEATH_CHANNEL] >= 30 * BASELINE_DELAY)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_ABORT;
    }

    fail_check();
    mpr("Malign forces permeate your being, awaiting release.");

    you.increase_duration(DUR_DEATH_CHANNEL, 15 + random2(1 + pow/3), 100);

    if (god != GOD_NO_GOD)
        you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = static_cast<int>(god);

    return SPRET_SUCCESS;
}

spret_type cast_recall(bool fail)
{
    fail_check();
    recall(0);
    return SPRET_SUCCESS;
}

// Type recalled:
// 0 = anything
// 1 = undead only (Yred religion ability)
// 2 = orcs only (Beogh religion ability)
bool recall(int type_recalled)
{
    int loopy          = 0;      // general purpose looping variable {dlb}
    bool success       = false;  // more accurately: "apparent success" {dlb}
    int start_count    = 0;
    int step_value     = 1;
    int end_count      = (MAX_MONSTERS - 1);

    monster* mons = NULL;

    // someone really had to make life difficult {dlb}:
    // sometimes goes through monster list backwards
    if (coinflip())
    {
        start_count = (MAX_MONSTERS - 1);
        end_count   = 0;
        step_value  = -1;
    }

    for (loopy = start_count; loopy != end_count + step_value;
         loopy += step_value)
    {
        mons = &menv[loopy];

        if (mons->type == MONS_NO_MONSTER)
            continue;

        if (!mons->friendly())
            continue;

        if (mons_class_is_stationary(mons->type)
            || mons_is_conjured(mons->type))
        {
            continue;
        }

        if (!monster_habitable_grid(mons, DNGN_FLOOR))
            continue;

        if (type_recalled == 1) // undead
        {
            if (mons->holiness() != MH_UNDEAD)
                continue;
        }
        else if (type_recalled == 2) // Beogh
        {
            if (!is_orcish_follower(mons))
                continue;
        }

        coord_def empty;
        if (empty_surrounds(you.pos(), DNGN_FLOOR, 3, false, empty)
            && mons->move_to_pos(empty))
        {
            // only informed if monsters recalled are visible {dlb}:
            if (simple_monster_message(mons, " is recalled."))
                success = true;
        }
        else
            break;              // no more room to place monsters {dlb}
    }

    if (!success)
        mpr("Nothing appears to have answered your call.");

    return success;
}

// Cast_phase_shift: raises evasion (by 8 currently) via Translocations.
spret_type cast_phase_shift(int pow, bool fail)
{
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
    int shallow = 1 + you.skill(SK_EARTH_MAGIC) / 8;
    int range = shallow + random2(pow) / 25;
    int maxrange = shallow + pow / 25;

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
    if (!in_bounds(dest))
        mpr("You sense an overwhelming volume of rock.");
    else if (feat_is_solid(grd(dest)))
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
    if (mons == NULL
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
        potion_effect(POT_CONFUSION, 10 + (100 - pow) / 10);

    if (one_chance_in(20)
        && lose_stat(STAT_INT, 1 + random2(3), false,
                      "casting intoxication"))
    {
        mpr("Your head spins!");
    }

    apply_area_visible(_intoxicate_monsters, pow);
    return SPRET_SUCCESS;
}

void remove_condensation_shield()
{
    mpr("Your icy shield evaporates.", MSGCH_DURATION);
    you.duration[DUR_CONDENSATION_SHIELD] = 0;
    you.redraw_armour_class = true;
}

spret_type cast_condensation_shield(int pow, bool fail)
{
    if (you.shield() || you.duration[DUR_FIRE_SHIELD])
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return SPRET_ABORT;
    }

    fail_check();

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        mpr("The disc of vapour around you crackles some more.");
    else
        mpr("A crackling disc of dense vapour forms in the air!");
    you.increase_duration(DUR_CONDENSATION_SHIELD, 15 + random2(pow), 40);
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
        mpr("This spell conflicts with another spell still in effect.");
        return SPRET_ABORT;
    }

    fail_check();

    if (you.duration[DUR_STONESKIN])
        mpr("Your skin feels harder.");
    else
    {
        if (you.form == TRAN_STATUE)
            mpr("Your stone body feels more resilient.");
        else
            mpr("Your skin hardens.");

        you.redraw_armour_class = true;
    }

    you.increase_duration(DUR_STONESKIN, 10 + random2(pow) + random2(pow), 50);

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
