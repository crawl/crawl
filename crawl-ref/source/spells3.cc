/*
 *  File:       spells3.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "spells3.h"
#include "externs.h"

#include "areas.h"
#include "beam.h"
#include "cloud.h"
#include "coord.h"
#include "debug.h"
#include "effects.h"
#include "env.h"
#include "food.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-place.h"
#include "religion.h"
#include "shout.h"
#include "spells1.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "travel.h"
#include "viewmap.h"

bool cast_selective_amnesia(bool force)
{
    if (you.spell_no == 0)
    {
        canned_msg(MSG_NO_SPELLS);
        return (false);
    }

    int keyin = 0;

    // Pick a spell to forget.
    while (true)
    {
        mpr("Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT);
        keyin = get_ch();

        if (key_is_escape(keyin))
            return (false);

        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells(false);
            redraw_screen();
        }

        if (!isaalpha(keyin))
            mesclr();
        else
            break;
    }

    const spell_type spell = get_spell_by_letter(keyin);
    const int slot = get_spell_slot_by_letter(keyin);

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        return (false);
    }

    if (!force
        && random2(you.skills[SK_SPELLCASTING])
           < random2(spell_difficulty(spell)))
    {
        mpr("Oops! This spell sure is a blunt instrument.");
        forget_map(20 + random2(50));
    }
    else
    {
        const int ep_gain = spell_mana(spell);
        del_spell_from_memory_by_slot(slot);

        if (ep_gain > 0)
        {
            inc_mp(ep_gain, false);
            mpr("The spell releases its latent energy back to you as "
                "it unravels.");
        }
    }

    return (true);
}

int airstrike(int pow, const dist &beam)
{
    bool success = false;

    monsters *monster = monster_at(beam.target);

    if (monster == NULL)
        canned_msg(MSG_SPELL_FIZZLES);
    else
    {
        god_conduct_trigger conducts[3];
        disable_attack_conducts(conducts);

        success = !stop_attack_prompt(monster, false, you.pos());

        if (success)
        {
            set_attack_conducts(conducts, monster);

            mprf("The air twists around and strikes %s!",
                 monster->name(DESC_NOCAP_THE).c_str());

            behaviour_event(monster, ME_ANNOY, MHITYOU);
            if (mons_is_mimic(monster->type))
                mimic_alert(monster);
        }

        enable_attack_conducts(conducts);

        if (success)
        {
            int hurted = 8 + random2(random2(4) + (random2(pow) / 6)
                           + (random2(pow) / 7));

            if (mons_flies(monster))
            {
                hurted *= 3;
                hurted /= 2;
            }

            hurted -= random2(1 + monster->ac);

            hurted = std::max(0, hurted);

            monster->hurt(&you, hurted);
            if (monster->alive())
                print_wounds(monster);
        }
    }

    return (success);
}

bool cast_bone_shards(int power, bolt &beam)
{
    if (!you.weapon() || you.weapon()->base_type != OBJ_CORPSES)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return (false);
    }

    bool success = false;

    const bool was_orc = (mons_species(you.weapon()->plus) == MONS_ORC);

    if (you.weapon()->sub_type != CORPSE_SKELETON)
    {
        mpr("The corpse collapses into a pulpy mess.");

        dec_inv_item_quantity(you.equip[EQ_WEAPON], 1);

        if (was_orc)
            did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
    }
    else
    {
        // Practical max of 100 * 15 + 3000 = 4500.
        // Actual max of    200 * 15 + 3000 = 6000.
        power *= 15;
        power += mons_weight(you.weapon()->plus);

        if (!player_tracer(ZAP_BONE_SHARDS, power, beam))
            return (false);

        mpr("The skeleton explodes into sharp fragments of bone!");

        dec_inv_item_quantity(you.equip[EQ_WEAPON], 1);

        if (was_orc)
            did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);

        zapping(ZAP_BONE_SHARDS, power, beam);

        success = true;
    }

    return (success);
}

bool cast_sublimation_of_blood(int pow)
{
    bool success = false;

    int wielded = you.equip[EQ_WEAPON];

    if (wielded != -1)
    {
        if (you.inv[wielded].base_type == OBJ_FOOD
            && you.inv[wielded].sub_type == FOOD_CHUNK)
        {
            mpr("The chunk of flesh you are holding crumbles to dust.");

            mpr("A flood of magical energy pours into your mind!");

            inc_mp(7 + random2(7), false);

            dec_inv_item_quantity(wielded, 1);

            if (mons_species(you.inv[wielded].plus) == MONS_ORC)
                did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
        }
        else if (is_blood_potion(you.inv[wielded]))
        {
            mprf("The blood within %s froths and boils.",
                 you.inv[wielded].quantity > 1 ? "one of your flasks"
                                               : "the flask you are holding");

            mpr("A flood of magical energy pours into your mind!");

            inc_mp(7 + random2(7), false);

            split_potions_into_decay(wielded, 1, false);
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
        else if (you.species == SP_VAMPIRE && you.hunger_state <= HS_SATIATED)
        {
            mpr("You don't have enough blood to draw power from your "
                "own body.");
        }
        else if (!enough_hp(2, true))
             mpr("Your attempt to draw power from your own body fails.");
        else
        {
            // For vampires.
            int food = 0;

            mpr("You draw magical energy from your own body!");

            while (you.magic_points < you.max_magic_points && you.hp > 1
                   && (you.species != SP_VAMPIRE || you.hunger - food >= 7000))
            {
                success = true;

                inc_mp(1, false);
                dec_hp(1, false);

                if (you.species == SP_VAMPIRE)
                    food += 15;

                for (int loopy = 0; loopy < (you.hp > 1 ? 3 : 0); ++loopy)
                    if (x_chance_in_y(6, pow))
                        dec_hp(1, false);

                if (x_chance_in_y(6, pow))
                    break;
            }

            make_hungry(food, false);
        }
    }

    return (success);
}

bool cast_death_channel(int pow, god_type god)
{
    bool success = false;

    if (you.duration[DUR_DEATH_CHANNEL] < 30 * BASELINE_DELAY)
    {
        success = true;

        mpr("Malign forces permeate your being, awaiting release.");

        you.increase_duration(DUR_DEATH_CHANNEL, 15 + random2(1 + pow/3), 100);

        if (god != GOD_NO_GOD)
            you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = static_cast<int>(god);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

bool project_noise()
{
    bool success = false;

    coord_def pos(1, 0);
    level_pos lpos;

    mpr("Choose the noise's source (press '.' or delete to select).");
    more();

    // Might abort with SIG_HUP despite !allow_esc.
    if (!show_map(lpos, false, false, false))
        lpos = level_pos::current();
    pos = lpos.pos;

    redraw_screen();

    dprf("Target square (%d,%d)", pos.x, pos.y);

    if (!in_bounds(pos) || !silenced(pos))
    {
        if (in_bounds(pos) && !feat_is_solid(grd(pos)))
        {
            noisy(30, pos);
            success = true;
        }

        if (!silenced(you.pos()))
        {
            if (success)
            {
                mprf(MSGCH_SOUND, "You hear a %svoice call your name.",
                     (!you.see_cell(pos) ? "distant " : ""));
            }
            else
                mprf(MSGCH_SOUND, "You hear a dull thud.");
        }
    }

    return (success);
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

    monsters *monster = NULL;

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
        monster = &menv[loopy];

        if (monster->type == MONS_NO_MONSTER)
            continue;

        if (!monster->friendly())
            continue;

        if (mons_class_is_stationary(monster->type)
            || mons_is_conjured(monster->type))
        {
            continue;
        }

        if (!monster_habitable_grid(monster, DNGN_FLOOR))
            continue;

        if (type_recalled == 1) // undead
        {
            if (monster->holiness() != MH_UNDEAD)
                continue;
        }
        else if (type_recalled == 2) // Beogh
        {
            if (!is_orcish_follower(monster))
                continue;
        }

        coord_def empty;
        if (empty_surrounds(you.pos(), DNGN_FLOOR, 3, false, empty)
            && monster->move_to_pos(empty))
        {
            // only informed if monsters recalled are visible {dlb}:
            if (simple_monster_message(monster, " is recalled."))
                success = true;
        }
        else
            break;              // no more room to place monsters {dlb}
    }

    if (!success)
        mpr("Nothing appears to have answered your call.");

    return (success);
}
