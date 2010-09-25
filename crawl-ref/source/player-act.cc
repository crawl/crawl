/*
 * File:     player-act.cc
 * Summary:  Implementing the actor interface for player.
 */

#include "AppHdr.h"

#include "player.h"

#include "artefact.h"
#include "dgnevent.h"
#include "env.h"
#include "food.h"
#include "goditem.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "misc.h"
#include "monster.h"
#include "state.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "viewgeom.h"

monster_type player::id() const
{
    return (MONS_PLAYER);
}

int player::mindex() const
{
    return (MHITYOU);
}

kill_category player::kill_alignment() const
{
    return (KC_YOU);
}

god_type player::deity() const
{
    return (religion);
}

bool player::alive() const
{
    // Simplistic, but if the player dies the game is over anyway, so
    // nobody can ask further questions.
    return (!crawl_state.game_is_arena());
}

bool player::is_summoned(int* _duration, int* summon_type) const
{
    if (_duration != NULL)
        *_duration = -1;
    if (summon_type != NULL)
        *summon_type = 0;

    return (false);
}

void player::moveto(const coord_def &c)
{
    if (c != pos())
        clear_trapping_net();

    crawl_view.set_player_at(c);
    set_position(c);
}

bool player::move_to_pos(const coord_def &c)
{
    actor *target = actor_at(c);
    if (!target || target->submerged())
    {
        moveto(c);
        return true;
    }
    return false;
}

void player::apply_location_effects(const coord_def &oldpos,
                                    killer_type killer,
                                    int killernum)
{
    moveto_location_effects(env.grid(oldpos));
}

void player::set_position(const coord_def &c)
{
    ASSERT(!crawl_state.game_is_arena());

    const bool real_move = (c != pos());
    actor::set_position(c);

    if (real_move)
    {
        reset_prev_move();
        dungeon_events.fire_position_event(DET_PLAYER_MOVED, c);
    }
}

bool player::swimming() const
{
    return in_water() && can_swim();
}

bool player::submerged() const
{
    return (false);
}

bool player::floundering() const
{
    return in_water() && !can_swim() && !extra_balanced();
}

bool player::extra_balanced() const
{
    return (species == SP_NAGA && !transform_changed_physiology());
}

int player::get_experience_level() const
{
    return (experience_level);
}

bool player::can_pass_through_feat(dungeon_feature_type grid) const
{
    return !feat_is_solid(grid);
}

bool player::is_habitable_feat(dungeon_feature_type actual_grid) const
{
    if (!can_pass_through_feat(actual_grid))
        return (false);

    if (airborne())
        return (true);

    if (actual_grid == DNGN_LAVA
        || actual_grid == DNGN_DEEP_WATER && !can_swim())
    {
        return (false);
    }

    return (true);
}

size_type player::body_size(size_part_type psize, bool base) const
{
    if (base)
        return species_size(species, psize);
    else
    {
        size_type tf_size = transform_size(psize);
        return (tf_size == SIZE_CHARACTER ? species_size(species, psize)
                                          : tf_size);
    }
}

int player::body_weight(bool base) const
{
    int weight = actor::body_weight(base);

    if (base)
        return (weight);

    switch (attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_STATUE:
        weight *= 2;
        break;
    case TRAN_LICH:
        weight /= 2;
        break;
    default:
        break;
    }

    return (weight);
}

int player::total_weight() const
{
    return (body_weight() + burden);
}

int player::damage_type(int)
{
    if (const item_def* wp = weapon())
        return (get_vorpal_type(*wp));
    else if (attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
        return (DVORP_SLICING);
    else if (has_usable_claws())
        return (DVORP_CLAWING);

    return (DVORP_CRUSHING);
}

int player::damage_brand(int)
{
    int ret = SPWPN_NORMAL;
    const int wpn = equip[EQ_WEAPON];

    if (wpn != -1)
    {
        if (!is_range_weapon(inv[wpn]))
            ret = get_weapon_brand(inv[wpn]);
    }
    else if (duration[DUR_CONFUSING_TOUCH])
        ret = SPWPN_CONFUSE;
    else
    {
        switch (attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_SPIDER:
            ret = SPWPN_VENOM;
            break;

        case TRAN_ICE_BEAST:
            ret = SPWPN_FREEZING;
            break;

        case TRAN_LICH:
            ret = SPWPN_DRAINING;
            break;

        case TRAN_BAT:
            if (species == SP_VAMPIRE && one_chance_in(8))
                ret = SPWPN_VAMPIRICISM;
            break;

        default:
            break;
        }
    }

    return (ret);
}

// Returns the item in the given equipment slot, NULL if the slot is empty.
// eq must be in [EQ_WEAPON, EQ_AMULET], or bad things will happen.
item_def *player::slot_item(equipment_type eq, bool include_melded)
{
    ASSERT(eq >= EQ_WEAPON && eq <= EQ_AMULET);

    const int item = equip[eq];
    if (item == -1 || !include_melded && melded[eq])
        return (NULL);
    return (&inv[item]);
}

// const variant of the above...
const item_def *player::slot_item(equipment_type eq, bool include_melded) const
{
    ASSERT(eq >= EQ_WEAPON && eq <= EQ_AMULET);

    const int item = equip[eq];
    if (item == -1 || !include_melded && melded[eq])
        return (NULL);
    return (&inv[item]);
}

// Returns the item in the player's weapon slot.
item_def *player::weapon(int /* which_attack */)
{
    return (slot_item(EQ_WEAPON, false));
}

bool player::can_wield(const item_def& item, bool ignore_curse,
                       bool ignore_brand, bool ignore_shield,
                       bool ignore_transform) const
{
    if (equip[EQ_WEAPON] != -1 && !ignore_curse)
    {
        if (inv[equip[EQ_WEAPON]].cursed())
            return (false);
    }

    // Unassigned means unarmed combat.
    const bool two_handed = item.base_type == OBJ_UNASSIGNED
                            || hands_reqd(item, body_size()) == HANDS_TWO;

    if (two_handed && !ignore_shield && player_wearing_slot(EQ_SHIELD))
        return (false);

    return could_wield(item, ignore_brand, ignore_transform);
}

bool player::could_wield(const item_def &item, bool ignore_brand,
                         bool /* ignore_transform */) const
{
    if (species == SP_CAT)
        return (false);
    if (body_size(PSIZE_TORSO) < SIZE_LARGE && item_mass(item) >= 300)
        return (false);

    // Small species wielding large weapons...
    if (body_size(PSIZE_BODY) < SIZE_MEDIUM
        && !check_weapon_wieldable_size(item, body_size(PSIZE_BODY)))
    {
        return (false);
    }

    if (!ignore_brand)
    {
        if (undead_or_demonic() && is_holy_item(item))
            return (false);
    }

    return (true);
}

// Returns the shield the player is wearing, or NULL if none.
item_def *player::shield()
{
    return (slot_item(EQ_SHIELD, false));
}

void player::make_hungry(int hunger_increase, bool silent)
{
    if (hunger_increase > 0)
        ::make_hungry(hunger_increase, silent);
    else if (hunger_increase < 0)
        ::lessen_hunger(-hunger_increase, silent);
}

static std::string _pronoun_you(description_level_type desc)
{
    switch (desc)
    {
    case DESC_NONE:
        return "";
    case DESC_CAP_A: case DESC_CAP_THE:
        return "You";
    case DESC_NOCAP_A: case DESC_NOCAP_THE:
    default:
        return "you";
    case DESC_CAP_YOUR:
        return "Your";
    case DESC_NOCAP_YOUR:
    case DESC_NOCAP_ITS:
        return "your";
    }
}

std::string player::name(description_level_type type, bool) const
{
    return (_pronoun_you(type));
}

std::string player::pronoun(pronoun_type pro, bool) const
{
    switch (pro)
    {
    default:
    case PRONOUN_CAP:               return "You";
    case PRONOUN_NOCAP:             return "you";
    case PRONOUN_CAP_POSSESSIVE:    return "Your";
    case PRONOUN_NOCAP_POSSESSIVE:  return "your";
    case PRONOUN_REFLEXIVE:         return "yourself";
    case PRONOUN_OBJECTIVE:         return "you";
    }
}

std::string player::conj_verb(const std::string &verb) const
{
    return (verb);
}

std::string player::hand_name(bool plural, bool *can_plural) const
{
    if (can_plural != NULL)
        *can_plural = true;
    return your_hand(plural);
}

std::string player::foot_name(bool plural, bool *can_plural) const
{
    bool _can_plural;
    if (can_plural == NULL)
        can_plural = &_can_plural;
    *can_plural = true;

    std::string str;

    if (attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER)
        str = "hind leg";
    else if (!transform_changed_physiology())
    {
        if (player_mutation_level(MUT_HOOVES) >= 3)
            str = "hoof";
        else if (player_mutation_level(MUT_TALONS) >= 3)
            str = "talon";
        else if (species == SP_NAGA)
        {
            str         = "underbelly";
            *can_plural = false;
        }
        else if (species == SP_MERFOLK && swimming())
        {
            str         = "tail";
            *can_plural = false;
        }
    }

    if (str.empty())
        return (plural ? "feet" : "foot");

    if (plural && *can_plural)
        str = pluralise(str);

    return str;
}

std::string player::arm_name(bool plural, bool *can_plural) const
{
    if (transform_changed_physiology())
        return hand_name(plural, can_plural);

    if (can_plural != NULL)
        *can_plural = true;

    std::string str = "arm";

    if (player_genus(GENPC_DRACONIAN) || species == SP_NAGA)
        str = "scaled arm";
    else if (species == SP_KENKU)
        str = "feathered arm";
    else if (species == SP_MUMMY)
        str = "bandage-wrapped arm";

    if (plural)
        str = pluralise(str);

    return (str);
}

bool player::fumbles_attack(bool verbose)
{
    // Fumbling in shallow water.
    if (floundering())
    {
        if (x_chance_in_y(4, dex()) || one_chance_in(5))
        {
            if (verbose)
                mpr("Unstable footing causes you to fumble your attack.");
            return (true);
        }
    }
    return (false);
}

bool player::cannot_fight() const
{
    return (false);
}

// If you have a randart equipped that has the ARTP_ANGRY property,
// there's a 1/20 chance of it becoming activated whenever you
// attack a monster. (Same as the berserk mutation at level 1.)
// The probabilities for actually going berserk are cumulative!
static bool _equipment_make_berserk()
{
    for (int eq = EQ_WEAPON; eq < NUM_EQUIP; eq++)
    {
        const item_def *item = you.slot_item((equipment_type) eq, false);
        if (!item)
            continue;

        if (!is_artefact(*item))
            continue;

        if (artefact_wpn_property(*item, ARTP_ANGRY) && one_chance_in(20))
            return (true);
    }

    // nothing found
    return (false);
}

void player::attacking(actor *other)
{
    ASSERT(!crawl_state.game_is_arena());

    if (other && other->atype() == ACT_MONSTER)
    {
        const monster* mon = other->as_monster();
        if (!mon->friendly() && !mon->neutral())
            pet_target = mon->mindex();
    }

    if (player_mutation_level(MUT_BERSERK)
            && x_chance_in_y(player_mutation_level(MUT_BERSERK) * 10 - 5, 100)
        || _equipment_make_berserk())
    {
        go_berserk(false);
    }
}

void player::go_berserk(bool intentional, bool potion)
{
    ::go_berserk(intentional, potion);
}

bool player::can_go_berserk() const
{
    return (can_go_berserk(false));
}

bool player::can_go_berserk(bool intentional, bool potion) const
{
    const bool verbose = intentional || potion;

    if (berserk())
    {
        if (verbose)
            mpr("You're already berserk!");
        // or else you won't notice -- no message here.
        return (false);
    }

    if (duration[DUR_EXHAUSTED])
    {
        if (verbose)
            mpr("You're too exhausted to go berserk.");
        // or else they won't notice -- no message here
        return (false);
    }

    if (duration[DUR_DEATHS_DOOR])
    {
        if (verbose)
            mpr("Your body is effectively dead; that's not a shape for a blood rage.");
        return (false);
    }

    if (beheld())
    {
        if (verbose)
            mpr("You are too mesmerised to rage.");
        // or else they won't notice -- no message here
        return (false);
    }

    if (afraid())
    {
        if (verbose)
            mpr("You are too terrified to rage.");

        return (false);
    }

    if (is_undead
        && (is_undead != US_SEMI_UNDEAD || hunger_state <= HS_SATIATED))
    {
        if (verbose)
            mpr("You cannot raise a blood rage in your lifeless body.");

        // or else you won't notice -- no message here
        return (false);
    }

    // Stasis, but only for identified amulets; unided amulets will
    // trigger when the player attempts to activate berserk,
    // auto-iding at that point, but also killing the berserk and
    // wasting a turn.
    if (wearing_amulet(AMU_STASIS, false))
    {
        if (verbose)
        {
            const item_def *amulet = you.slot_item(EQ_AMULET, false);
            mprf("You cannot go berserk with %s on.",
                 amulet? amulet->name(DESC_NOCAP_YOUR).c_str() : "your amulet");
        }
        return (false);
    }

    if (!intentional && !potion && player_mental_clarity(true))
    {
        if (verbose)
        {
            mpr("You're too calm and focused to rage.");
            item_def *amu;
            if (!player_mental_clarity(false) && wearing_amulet(AMU_CLARITY)
                && (amu = &you.inv[you.equip[EQ_AMULET]]) && !item_type_known(*amu))
            {
                set_ident_type(amu->base_type, amu->sub_type, ID_KNOWN_TYPE);
                set_ident_flags(*amu, ISFLAG_KNOW_PROPERTIES);
                mprf("You are wearing: %s",
                     amu->name(DESC_INVENTORY_EQUIP).c_str());
            }
        }

        return (false);
    }

    return (true);
}

bool player::berserk() const
{
    return (duration[DUR_BERSERKER]);
}
