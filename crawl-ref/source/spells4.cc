/*
 *  File:       spells4.cc
 *  Summary:    new spells
 *  Written by: Copyleft Josh Fishman 1999-2000, All Rights Preserved
 */

#include "AppHdr.h"
#include "externs.h"

#include "areas.h"
#include "coord.h"
#include "delay.h"
#include "env.h"
#include "godconduct.h"
#include "hints.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "mon-place.h"
#include "player-stats.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"

// Cast_phase_shift: raises evasion (by 8 currently) via Translocations.
void cast_phase_shift(int pow)
{
    if (!you.duration[DUR_PHASE_SHIFT])
        mpr("You feel the strange sensation of being on two planes at once.");
    else
        mpr("You feel the material plane grow further away.");

    you.increase_duration(DUR_PHASE_SHIFT, 5 + random2(pow), 30);
    you.redraw_evasion = true;
}

void cast_see_invisible(int pow)
{
    if (you.can_see_invisible())
        mpr("You feel as though your vision will be sharpened longer.");
    else
    {
        mpr("Your vision seems to sharpen.");

        // We might have to turn autopickup back on again.
        // TODO: Once the spell times out we might want to check all monsters
        //       in LOS for invisibility and turn autopickup off again, if
        //       needed.
        autotoggle_autopickup(false);
    }

    // No message if you already are under the spell.
    you.increase_duration(DUR_SEE_INVISIBLE, 10 + random2(2 + pow/2), 100);
}

static int _sleep_monsters(coord_def where, int pow, int, actor *)
{
    monsters *monster = monster_at(where);
    if (!monster)
        return (0);

    if (!monster->can_hibernate(true))
        return (0);

    if (monster->check_res_magic(pow))
        return (0);

    const int res = monster->res_cold();
    if (res > 0 && one_chance_in(std::max(4 - res, 1)))
        return (0);

    if (monster->has_ench(ENCH_SLEEP_WARY) && !one_chance_in(3))
        return (0);

    monster->hibernate();
    monster->expose_to_element(BEAM_COLD, 2);
    return (1);
}

void cast_mass_sleep(int pow)
{
    apply_area_visible(_sleep_monsters, pow);
}

// This is a hack until we set an is_beast flag in the monster data
// (which we might never do, this is sort of minor.)
// It's a list of monster types which can be affected by beast taming.
static bool _is_domesticated_animal(int type)
{
    const monster_type types[] = {
        MONS_GIANT_BAT, MONS_HOUND, MONS_JACKAL, MONS_RAT,
        MONS_YAK, MONS_WYVERN, MONS_HIPPOGRIFF, MONS_GRIFFON,
        MONS_DEATH_YAK, MONS_WAR_DOG, MONS_GREY_RAT,
        MONS_GREEN_RAT, MONS_ORANGE_RAT, MONS_SHEEP,
        MONS_HOG, MONS_GIANT_FROG, MONS_GIANT_TOAD,
        MONS_SPINY_FROG, MONS_BLINK_FROG, MONS_WOLF, MONS_WARG,
        MONS_BEAR, MONS_GRIZZLY_BEAR, MONS_POLAR_BEAR, MONS_BLACK_BEAR
    };

    for (unsigned int i = 0; i < ARRAYSZ(types); ++i)
        if (types[i] == type)
            return (true);

    return (false);
}

static int _tame_beast_monsters(coord_def where, int pow, int, actor *)
{
    monsters *monster = monster_at(where);
    if (monster == NULL)
        return 0;

    if (!_is_domesticated_animal(monster->type) || monster->friendly()
        || player_will_anger_monster(monster))
    {
        return 0;
    }

    // 50% bonus for dogs
    if (monster->type == MONS_HOUND || monster->type == MONS_WAR_DOG )
        pow += (pow / 2);

    if (you.species == SP_HILL_ORC && monster->type == MONS_WARG)
        pow += (pow / 2);

    if (monster->check_res_magic(pow))
        return 0;

    simple_monster_message(monster, " is tamed!");

    if (random2(100) < random2(pow / 10))
        monster->attitude = ATT_FRIENDLY;  // permanent
    else
        monster->add_ench(ENCH_CHARM);     // temporary
    mons_att_changed(monster);

    return 1;
}

void cast_tame_beasts(int pow)
{
    apply_area_visible(_tame_beast_monsters, pow);
}

void cast_silence(int pow)
{
    if (!you.attribute[ATTR_WAS_SILENCED])
        mpr("A profound silence engulfs you.");

    you.attribute[ATTR_WAS_SILENCED] = 1;

    you.increase_duration(DUR_SILENCE, 10 + random2avg(pow, 2), 100);
    invalidate_agrid(true);

    if (you.beheld())
        you.update_beholders();

    learned_something_new(HINT_YOU_SILENCE);
}

static bool _feat_is_passwallable(dungeon_feature_type feat)
{
    // Irony: you can passwall through a secret door but not a door.
    // Worked stone walls are out, they're not diggable and
    // are used for impassable walls...
    switch (feat)
    {
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_CLEAR_ROCK_WALL:
    case DNGN_SECRET_DOOR:
        return (true);
    default:
        return (false);
    }
}

bool cast_passwall(const coord_def& delta, int pow)
{
    int shallow = 1 + (you.skills[SK_EARTH_MAGIC] / 8);
    int range = shallow + random2(pow) / 25;
    int maxrange = shallow + pow / 25;

    coord_def dest;
    for (dest = you.pos() + delta;
         in_bounds(dest) && _feat_is_passwallable(grd(dest));
         dest += delta) ;

    int walls = (dest - you.pos()).rdist() - 1;
    if (walls == 0)
    {
        mpr("That's not a passable wall.");
        return (false);
    }

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
        // Passwall delay is reduced, and the delay cannot be interrupted.
        start_delay(DELAY_PASSWALL, 1 + walls, dest.x, dest.y);
    }
    return (true);
}

static int _intoxicate_monsters(coord_def where, int pow, int, actor *)
{
    UNUSED( pow );

    monsters *monster = monster_at(where);
    if (monster == NULL
        || mons_intel(monster) < I_NORMAL
        || monster->holiness() != MH_NATURAL
        || monster->res_poison() > 0)
    {
        return 0;
    }

    monster->add_ench(mon_enchant(ENCH_CONFUSION, 0, KC_YOU));
    return 1;
}

void cast_intoxicate(int pow)
{
    potion_effect( POT_CONFUSION, 10 + (100 - pow) / 10);

    if (one_chance_in(20)
        && lose_stat( STAT_INT, 1 + random2(3), false,
                      "casting intoxication"))
    {
        mpr("Your head spins!");
    }

    apply_area_visible(_intoxicate_monsters, pow);
}

bool backlight_monsters(coord_def where, int pow, int garbage)
{
    UNUSED(pow);
    UNUSED(garbage);

    monsters *monster = monster_at(where);
    if (monster == NULL)
        return (false);

    // Already glowing.
    if (mons_class_flag(monster->type, M_GLOWS_LIGHT)
        || mons_class_flag(monster->type, M_GLOWS_RADIATION))
    {
        return (false);
    }

    mon_enchant bklt = monster->get_ench(ENCH_CORONA);
    const int lvl = bklt.degree;

    // This enchantment overrides invisibility (neat).
    if (monster->has_ench(ENCH_INVIS))
    {
        if (!monster->has_ench(ENCH_CORONA))
        {
            monster->add_ench(
                mon_enchant(ENCH_CORONA, 1, KC_OTHER, random_range(30, 50)));
            simple_monster_message(monster, " is lined in light.");
        }
        return (true);
    }

    monster->add_ench(mon_enchant(ENCH_CORONA, 1));

    if (lvl == 0)
        simple_monster_message(monster, " is outlined in light.");
    else if (lvl == 4)
        simple_monster_message(monster, " glows brighter for a moment.");
    else
        simple_monster_message(monster, " glows brighter.");

    return (true);
}

// The intent of this spell isn't to produce helpful potions
// for drinking, but rather to provide ammo for the Evaporate
// spell out of corpses, thus potentially making it useful.
// Producing helpful potions would break game balance here...
// and producing more than one potion from a corpse, or not
// using up the corpse might also lead to game balance problems. - bwr
bool cast_fulsome_distillation(int pow, bool check_range)
{
    int num_corpses = 0;
    item_def *corpse = corpse_at(you.pos(), &num_corpses);
    if (num_corpses && you.flight_mode() == FL_LEVITATE)
        num_corpses = -1;

    // If there is only one corpse, distill it; otherwise, ask the player
    // which corpse to use.
    switch (num_corpses)
    {
        case 0: case -1:
            // Allow using Z to victory dance fulsome.
            if (!check_range)
            {
                mpr("The spell fizzles.");
                return (true);
            }

            if (num_corpses == -1)
                mpr("You can't reach the corpse!");
            else
                mpr("There aren't any corpses here.");
            return (false);
        case 1:
            // Use the only corpse available without prompting.
            break;
        default:
            // Search items at the player's location for corpses.
            // The last corpse detected earlier is irrelevant.
            corpse = NULL;
            for (stack_iterator si(you.pos(), true); si; ++si)
            {
                if (item_is_corpse(*si))
                {
                    const std::string corpsedesc =
                        get_menu_colour_prefix_tags(*si, DESC_NOCAP_THE);
                    const std::string prompt =
                        make_stringf("Distill a potion from %s?",
                                     corpsedesc.c_str());

                    if (yesno(prompt.c_str(), true, 0, false))
                    {
                        corpse = &*si;
                        break;
                    }
                }
            }
    }

    if (!corpse)
    {
        canned_msg(MSG_OK);
        return (false);
    }

    potion_type pot_type = POT_WATER;

    switch (mons_corpse_effect(corpse->plus))
    {
    case CE_CLEAN:
        pot_type = POT_WATER;
        break;

    case CE_CONTAMINATED:
        pot_type = (mons_weight(corpse->plus) >= 900)
            ? POT_DEGENERATION : POT_CONFUSION;
        break;

    case CE_POISONOUS:
        pot_type = POT_POISON;
        break;

    case CE_MUTAGEN_RANDOM:
    case CE_MUTAGEN_GOOD:   // unused
    case CE_RANDOM:         // unused
        pot_type = POT_MUTATION;
        break;

    case CE_MUTAGEN_BAD:    // unused
    case CE_ROTTEN:         // actually this only occurs via mangling
    case CE_HCL:            // necrophage
        pot_type = POT_DECAY;
        break;

    case CE_NOCORPSE:       // shouldn't occur
    default:
        break;
    }

    switch (corpse->plus)
    {
    case MONS_RED_WASP:              // paralysis attack
        pot_type = POT_PARALYSIS;
        break;

    case MONS_YELLOW_WASP:           // slowing attack
        pot_type = POT_SLOWING;
        break;

    default:
        break;
    }

    struct monsterentry* smc = get_monster_data(corpse->plus);

    for (int nattk = 0; nattk < 4; ++nattk)
    {
        if (smc->attack[nattk].flavour == AF_POISON_MEDIUM
            || smc->attack[nattk].flavour == AF_POISON_STRONG
            || smc->attack[nattk].flavour == AF_POISON_STR
            || smc->attack[nattk].flavour == AF_POISON_INT
            || smc->attack[nattk].flavour == AF_POISON_DEX
            || smc->attack[nattk].flavour == AF_POISON_STAT)
        {
            pot_type = POT_STRONG_POISON;
        }
    }

    const bool was_orc = (mons_species(corpse->plus) == MONS_ORC);

    // We borrow the corpse's object to make our potion.
    corpse->base_type = OBJ_POTIONS;
    corpse->sub_type  = pot_type;
    corpse->quantity  = 1;
    corpse->plus      = 0;
    corpse->plus2     = 0;
    corpse->flags     = 0;
    corpse->inscription.clear();
    item_colour(*corpse); // sets special as well

    // Always identify said potion.
    set_ident_type(*corpse, ID_KNOWN_TYPE);

    mprf("You extract %s from the corpse.",
         corpse->name(DESC_NOCAP_A).c_str());

    // Try to move the potion to the player (for convenience).
    if (move_item_to_player(corpse->index(), 1) != 1)
        mpr("Unfortunately, you can't carry it right now!");

    if (was_orc)
        did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);

    return (true);
}

void remove_condensation_shield()
{
    mpr("Your icy shield evaporates.", MSGCH_DURATION);
    you.duration[DUR_CONDENSATION_SHIELD] = 0;
    you.redraw_armour_class = true;
}

void cast_condensation_shield(int pow)
{
    if (you.shield() || you.duration[DUR_FIRE_SHIELD])
        canned_msg(MSG_SPELL_FIZZLES);
    else
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        {
            mpr("The disc of vapour around you crackles some more.");
            you.increase_duration(DUR_CONDENSATION_SHIELD,
                                  5 + roll_dice(2,3), 30);
        }
        else
        {
            mpr("A crackling disc of dense vapour forms in the air!");
            you.increase_duration(DUR_CONDENSATION_SHIELD,
                                  10 + roll_dice(2, pow / 5), 30);
            you.redraw_armour_class = true;
        }
    }
}

void cast_stoneskin(int pow)
{
    if (you.is_undead
        && (you.species != SP_VAMPIRE || you.hunger_state < HS_SATIATED))
    {
        mpr("This spell does not affect your undead flesh.");
        return;
    }

    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE
        && you.attribute[ATTR_TRANSFORMATION] != TRAN_STATUE
        && you.attribute[ATTR_TRANSFORMATION] != TRAN_BLADE_HANDS)
    {
        mpr("This spell does not affect your current form.");
        return;
    }

    if (you.duration[DUR_STONEMAIL] || you.duration[DUR_ICY_ARMOUR])
    {
        mpr("This spell conflicts with another spell still in effect.");
        return;
    }

    if (you.duration[DUR_STONESKIN])
        mpr( "Your skin feels harder." );
    else
    {
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_STATUE)
            mpr( "Your stone body feels more resilient." );
        else
            mpr( "Your skin hardens." );

        you.redraw_armour_class = true;
    }

    you.increase_duration(DUR_STONESKIN, 10 + random2(pow) + random2(pow), 50);
}

bool do_slow_monster(monsters* mon, kill_category whose_kill)
{
    // Try to remove haste, if monster is hasted.
    if (mon->del_ench(ENCH_HASTE, true))
    {
        if (simple_monster_message(mon, " is no longer moving quickly."))
            return (true);
    }

    // Not hasted, slow it.
    if (!mon->has_ench(ENCH_SLOW)
        && !mons_is_stationary(mon)
        && mon->add_ench(mon_enchant(ENCH_SLOW, 0, whose_kill)))
    {
        if (!mon->paralysed() && !mon->petrified()
            && simple_monster_message(mon, " seems to slow down."))
        {
            return (true);
        }
    }

    return (false);
}
