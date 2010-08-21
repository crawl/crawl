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
#include "coordit.h"
#include "debug.h"
#include "effects.h"
#include "env.h"
#include "food.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-place.h"
#include "options.h"
#include "religion.h"
#include "shout.h"
#include "spells1.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
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

static void _maybe_mark_was_cursed(item_def &item)
{
    if (Options.autoinscribe_cursed
        && item.inscription.find("was cursed") == std::string::npos
        && !item_ident(item, ISFLAG_SEEN_CURSED)
        && !item_ident(item, ISFLAG_IDENT_MASK))
    {
        add_inscription(item, "was cursed");
    }
    do_uncurse_item(item);
}

bool remove_curse(bool suppress_msg)
{
    bool success = false;

    // Only cursed *weapons* in hand count as cursed. - bwr
    if (you.weapon()
        && you.weapon()->base_type == OBJ_WEAPONS
        && you.weapon()->cursed())
    {
        // Also sets wield_change.
        _maybe_mark_was_cursed(*you.weapon());
        success = true;
    }

    // Everything else uses the same paradigm - are we certain?
    // What of artefact rings and amulets? {dlb}:
    for (int i = EQ_WEAPON + 1; i < NUM_EQUIP; i++)
    {
        // Melded equipment can also get uncursed this way.
        if (you.equip[i] != -1 && you.inv[you.equip[i]].cursed())
        {
            _maybe_mark_was_cursed(you.inv[you.equip[i]]);
            success = true;
        }
    }

    if (!suppress_msg)
    {
        if (success)
            mpr("You feel as if something is helping you.");
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return (success);
}

bool detect_curse(int scroll, bool suppress_msg)
{
    int item_count = 0;
    int found_item = NON_ITEM;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def& item = you.inv[i];

        if (!item.defined())
            continue;

        if (item_count <= 1)
        {
            item_count += item.quantity;
            if (i == scroll)
                item_count--;
            if (item_count > 0)
                found_item = i;
        }

        if (item.base_type == OBJ_WEAPONS
            || item.base_type == OBJ_ARMOUR
            || item.base_type == OBJ_JEWELLERY)
        {
            set_ident_flags(item, ISFLAG_KNOW_CURSE);
        }
    }

    // Not carrying any items -> don't id the scroll.
    if (!item_count)
        return (false);

    ASSERT(found_item != NON_ITEM);

    if (!suppress_msg)
    {
        if (item_count == 1)
        {
            // If you're carrying just one item, mention it explicitly.
            item_def item = you.inv[found_item];

            // If the carried item is just the stack of scrolls,
            // decrease quantity by one to make up for the scroll just read.
            if (found_item == scroll)
                item.quantity--;

            mprf("%s softly glows as it is inspected for curses.",
                 item.name(DESC_CAP_YOUR).c_str());
        }
        else
            mpr("Your items softly glow as they are inspected for curses.");
    }

    return (true);
}

bool cast_smiting(int pow, const coord_def& where)
{
    monsters *m = monster_at(where);

    if (m == NULL)
    {
        mpr("There's nothing there!");
        // Counts as a real cast, due to victory-dancing and
        // invisible/submerged monsters.
        return (true);
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool success = !stop_attack_prompt(m, false, you.pos());

    if (success)
    {
        set_attack_conducts(conducts, m);

        mprf("You smite %s!", m->name(DESC_NOCAP_THE).c_str());

        behaviour_event(m, ME_ANNOY, MHITYOU);
        if (mons_is_mimic(m->type))
            mimic_alert(m);
    }

    enable_attack_conducts(conducts);

    if (success)
    {
        // Maxes out at around 40 damage at 27 Invocations, which is
        // plenty in my book (the old max damage was around 70,
        // which seems excessive).
        m->hurt(&you, 7 + (random2(pow) * 33 / 191));
        if (m->alive())
            print_wounds(m);
    }

    return (success);
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

static bool _do_imprison(int pow, const coord_def& where, bool force_full)
{
    // power guidelines:
    // powc is roughly 50 at Evoc 10 with no godly assistance, ranging
    // up to 300 or so with godly assistance or end-level, and 1200
    // as more or less the theoretical maximum.
    int number_built = 0;

    const dungeon_feature_type safe_tiles[] = {
        DNGN_SHALLOW_WATER, DNGN_FLOOR, DNGN_FLOOR_SPECIAL, DNGN_OPEN_DOOR
    };

    bool proceed;

    if (force_full)
    {
        bool success = true;

        for (adjacent_iterator ai(where); ai; ++ai)
        {
            // The tile is occupied.
            if (actor_at(*ai))
            {
                success = false;
                break;
            }

            // Make sure we have a legitimate tile.
            proceed = false;
            for (unsigned int i = 0; i < ARRAYSZ(safe_tiles) && !proceed; ++i)
                if (grd(*ai) == safe_tiles[i] || feat_is_trap(grd(*ai)))
                    proceed = true;

            if (!proceed && grd(*ai) > DNGN_MAX_NONREACH)
            {
                success = false;
                break;
            }
        }

        if (!success)
        {
            mpr("Half-formed walls emerge from the floor, then retract.");
            return (false);
        }
    }

    for (adjacent_iterator ai(where); ai; ++ai)
    {
        // This is where power comes in.
        if (!force_full && one_chance_in(pow / 5))
            continue;

        // The tile is occupied.
        if (actor_at(*ai))
            continue;

        // Make sure we have a legitimate tile.
        proceed = false;
        for (unsigned int i = 0; i < ARRAYSZ(safe_tiles) && !proceed; ++i)
            if (grd(*ai) == safe_tiles[i] || feat_is_trap(grd(*ai)))
                proceed = true;

        if (proceed)
        {
            // All items are moved inside.
            if (igrd(*ai) != NON_ITEM)
                move_items(*ai, where);

            // All clouds are destroyed.
            if (env.cgrid(*ai) != EMPTY_CLOUD)
                delete_cloud(env.cgrid(*ai));

            // All traps are destroyed.
            if (trap_def *ptrap = find_trap(*ai))
                ptrap->destroy();

            // Actually place the wall.
            grd(*ai) = DNGN_ROCK_WALL;
            set_terrain_changed(*ai);
            number_built++;
        }
    }

    if (number_built > 0)
    {
        mpr("Walls emerge from the floor!");
        you.update_beholders();
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (number_built > 0);
}

bool entomb(int pow)
{
    return (_do_imprison(pow, you.pos(), false));
}

bool cast_imprison(int pow, monsters *monster, int source)
{
    if (_do_imprison(pow, monster->pos(), true))
    {
        const int tomb_duration = BASELINE_DELAY
            * pow;
        env.markers.add(new map_tomb_marker(monster->pos(),
                                            tomb_duration,
                                            source,
                                            monster->mindex()));
        return (true);
    }

    return (false);
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
