/**
 * @file
 * @brief Implementing the actor interface for player.
**/

#include "AppHdr.h"

#include "player.h"

#include <math.h>

#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "coordit.h"
#include "dgnevent.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "food.h"
#include "godconduct.h"
#include "goditem.h"
#include "hints.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "message.h"
#include "player-stats.h"
#include "religion.h"
#include "spl-damage.h"
#include "state.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "viewgeom.h"

int player::mindex() const
{
    return MHITYOU;
}

kill_category player::kill_alignment() const
{
    return KC_YOU;
}

god_type player::deity() const
{
    return religion;
}

bool player::alive() const
{
    // Simplistic, but if the player dies the game is over anyway, so
    // nobody can ask further questions.
    return !crawl_state.game_is_arena();
}

bool player::is_summoned(int* _duration, int* summon_type) const
{
    if (_duration != NULL)
        *_duration = -1;
    if (summon_type != NULL)
        *summon_type = 0;

    return false;
}

void player::moveto(const coord_def &c, bool clear_net)
{
    if (clear_net && c != pos())
        clear_trapping_net();

    crawl_view.set_player_at(c);
    set_position(c);

    clear_far_constrictions();
    end_searing_ray();
}

bool player::move_to_pos(const coord_def &c, bool clear_net)
{
    actor *target = actor_at(c);
    if (!target || target->submerged())
    {
        moveto(c, clear_net);
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

        if (duration[DUR_QUAD_DAMAGE])
            invalidate_agrid(true);

        if (player_has_orb())
        {
            env.orb_pos = c;
            invalidate_agrid(true);
        }

        dungeon_events.fire_position_event(DET_PLAYER_MOVED, c);
    }
}

bool player::swimming() const
{
    return in_water() && can_swim();
}

bool player::submerged() const
{
    return false;
}

bool player::floundering() const
{
    return in_water() && !can_swim() && !extra_balanced();
}

bool player::extra_balanced() const
{
    const dungeon_feature_type grid = grd(pos());
    return species == SP_GREY_DRACONIAN
              || form == TRAN_TREE
              || grid == DNGN_SHALLOW_WATER
                  && (species == SP_NAGA // tails, not feet
                      || body_size(PSIZE_BODY) >= SIZE_LARGE)
                  && (form == TRAN_LICH || form == TRAN_STATUE
                      || form == TRAN_SHADOW
                      || !form_changed_physiology());
}

int player::get_hit_dice() const
{
    return experience_level;
}

int player::get_experience_level() const
{
    return experience_level;
}

bool player::can_pass_through_feat(dungeon_feature_type grid) const
{
    return !feat_is_solid(grid) && grid != DNGN_MALIGN_GATEWAY;
}

bool player::is_habitable_feat(dungeon_feature_type actual_grid) const
{
    if (!can_pass_through_feat(actual_grid))
        return false;

    if (airborne()
#if TAG_MAJOR_VERSION == 34
            || species == SP_DJINNI
#endif
            )
    {
        return true;
    }

    if (
#if TAG_MAJOR_VERSION == 34
        actual_grid == DNGN_LAVA && species != SP_LAVA_ORC ||
#endif
        actual_grid == DNGN_DEEP_WATER && !can_swim())
    {
        return false;
    }

    return true;
}

size_type player::body_size(size_part_type psize, bool base) const
{
    if (base)
        return species_size(species, psize);
    else
    {
        size_type tf_size = transform_size(form);
        return tf_size == SIZE_CHARACTER ? species_size(species, psize)
                                         : tf_size;
    }
}

int player::body_weight(bool base) const
{
    int weight = actor::body_weight(base);

    if (base)
        return weight;

    switch (form)
    {
    case TRAN_STATUE:
        weight *= 2;
        break;
    case TRAN_LICH:
    case TRAN_SHADOW:
        weight /= 2;
        break;
    default:
        break;
    }

    return weight;
}

int player::damage_type(int)
{
    if (const item_def* wp = weapon())
        return get_vorpal_type(*wp);
    else if (form == TRAN_BLADE_HANDS)
        return DVORP_SLICING;
    else if (has_usable_claws())
        return DVORP_CLAWING;
    else if (has_usable_tentacles())
        return DVORP_TENTACLE;

    return DVORP_CRUSHING;
}

/**
 * What weapon brand does the player attack with in melee?
 */
brand_type player::damage_brand(int)
{
    const int wpn = equip[EQ_WEAPON];
    if (wpn != -1 && !melded[EQ_WEAPON])
    {
        if (is_range_weapon(inv[wpn]))
            return SPWPN_NORMAL; // XXX: check !is_melee_weapon instead?
        return get_weapon_brand(inv[wpn]);
    }

    // unarmed

    if (duration[DUR_CONFUSING_TOUCH])
        return SPWPN_CONFUSE;

    return get_form()->get_uc_brand();
}


/**
 * Return the delay caused by attacking with the provided weapon & projectile.
 *
 * @param weap          The weapon to be used; may be null.
 * @param projectile    The projectile to be fired/thrown; may be null.
 * @param random        Whether to randomize delay, or provide a fixed value
 *                      for display.
 * @param scaled        Whether to apply special delay modifiers (finesse)
 * @return              The time taken by an attack with the given weapon &
 *                      projectile, in aut.
 */
random_var player::attack_delay(const item_def *weap,
                                const item_def *projectile, bool random,
                                bool scaled) const
{
    random_var attk_delay = constant(15);
    // a semi-arbitrary multiplier, to minimize loss of precision from integer
    // math.
    const int DELAY_SCALE = 20;
    const int armour_penalty = adjusted_body_armour_penalty(DELAY_SCALE);
    const int base_shield_penalty = adjusted_shield_penalty(DELAY_SCALE);

    bool check_weapon = (!projectile && !!weap)
                        || projectile
                            && (is_launched(this, weap, *projectile)
                                != LRET_THROWN);

    if (!check_weapon)
    {
        if (form_uses_xl())
        {
            attk_delay =
                constant(10)
                - div_rand_round(constant(you.experience_level * 10), 54);
        }
        else
        {
            // UC/throwing attacks are slowed by heavy armour (aevp)
            attk_delay = max(10, 7 + div_rand_round(armour_penalty,
                                                    DELAY_SCALE));

            // ...and sped up by skill (min delay (10 - 270/54) = 5)
            skill_type sk = projectile ? SK_THROWING : SK_UNARMED_COMBAT;
            attk_delay -= div_rand_round(constant(you.skill(sk, 10)), 54);

            // Bats are faster (for whatever good it does them).
            if (you.form == TRAN_BAT && !projectile)
                attk_delay = div_rand_round(attk_delay * constant(3), 5);
        }
    }
    else
    {
        if (weap && is_weapon(*weap)
            && (!projectile && !is_range_weapon(*weap)
                || projectile
                   && is_launched(this, weap, *projectile) == LRET_LAUNCHED))
        {
            const skill_type wpn_skill = item_attack_skill(*weap);
            attk_delay = constant(property(*weap, PWPN_SPEED));
            attk_delay -=
                div_rand_round(constant(you.skill(wpn_skill, 10)),
                               DELAY_SCALE);

            // apply minimum to weapon skill modification
            attk_delay = rv::max(attk_delay, weapon_min_delay(*weap));

            if (weap->base_type == OBJ_WEAPONS
                && get_weapon_brand(*weap) == SPWPN_SPEED)
            {
                attk_delay = div_rand_round(constant(2) * attk_delay, 3);
            }
        }
    }

    // At the moment it never gets this low anyway.
    attk_delay = rv::max(attk_delay, constant(3));

    // Calculate this separately to avoid overflowing the weights in
    // the random_var.
    random_var shield_penalty = constant(0);
    if (base_shield_penalty)
    {
        shield_penalty =
            div_rand_round(rv::min(rv::roll_dice(1, base_shield_penalty),
                                   rv::roll_dice(1, base_shield_penalty)),
                           DELAY_SCALE);
    }
    // Give unarmed shield-users a slight penalty always.
    if (!weap && player_wearing_slot(EQ_SHIELD))
        shield_penalty += rv::random2(2);

    int final_delay = random ? attk_delay.roll() + shield_penalty.roll()
                             : attk_delay.expected() + shield_penalty.expected();
    // Stop here if we just want the unmodified value.
    if (!scaled)
        return final_delay;

    const int scaling = finesse_adjust_delay(you.time_taken);
    return max(2, div_rand_round(scaling * final_delay, 10));
}

// Returns the item in the given equipment slot, NULL if the slot is empty.
// eq must be in [EQ_WEAPON, EQ_RING_AMULET], or bad things will happen.
item_def *player::slot_item(equipment_type eq, bool include_melded) const
{
    ASSERT_RANGE(eq, EQ_WEAPON, NUM_EQUIP);

    const int item = equip[eq];
    if (item == -1 || !include_melded && melded[eq])
        return NULL;
    return const_cast<item_def *>(&inv[item]);
}

// Returns the item in the player's weapon slot.
item_def *player::weapon(int /* which_attack */) const
{
    if (melded[EQ_WEAPON])
        return NULL;

    return slot_item(EQ_WEAPON, false);
}

// Give hands required to wield weapon.
hands_reqd_type player::hands_reqd(const item_def &item) const
{
    if (species == SP_FORMICID)
        return HANDS_ONE;
    else
        return actor::hands_reqd(item);
}

bool player::can_wield(const item_def& item, bool ignore_curse,
                       bool ignore_brand, bool ignore_shield,
                       bool ignore_transform) const
{
    if (equip[EQ_WEAPON] != -1 && !ignore_curse)
    {
        if (inv[equip[EQ_WEAPON]].cursed())
            return false;
    }

    // Unassigned means unarmed combat.
    const bool two_handed = item.base_type == OBJ_UNASSIGNED
                            || hands_reqd(item) == HANDS_TWO;

    if (two_handed && (
        (!ignore_shield && player_wearing_slot(EQ_SHIELD))
        || player_mutation_level(MUT_MISSING_HAND)))
    {
        return false;
    }

    return could_wield(item, ignore_brand, ignore_transform);
}

/**
 * Checks whether the player could ever wield the given weapon, regardless of
 * what they're currently wielding, transformed into, or any other state.
 *
 * @param item              The item to wield.
 * @param ignore_brand      Whether to disregard the weapon's brand.
 * @return                  Whether the player could potentially wield the
 *                          item.
 */
bool player::could_wield(const item_def &item, bool ignore_brand,
                         bool ignore_transform, bool quiet) const
{
    const size_type bsize = body_size(PSIZE_TORSO, ignore_transform);

    // Only ogres and trolls can wield large rocks (for sandblast).
    if (bsize < SIZE_LARGE
        && item.base_type == OBJ_MISSILES && item.sub_type == MI_LARGE_ROCK)
    {
        if (!quiet)
            mpr("That's too large and heavy for you to wield.");
        return false;
    }

    // Most non-weapon objects can be wielded, though there's rarely a point
    if (!is_weapon(item))
    {
        if (item.base_type == OBJ_ARMOUR || item.base_type == OBJ_JEWELLERY)
        {
            if (!quiet)
                mprf("You can't wield %s.", base_type_string(item).c_str());
            return false;
        }

        return true;
    }
    else if (species == SP_FELID)
    {
        if (!quiet)
            mpr("You can't use weapons.");
        return false;
    }

    // Small species wielding large weapons...
    if (!is_weapon_wieldable(item, bsize))
    {
        if (!quiet)
            mpr("That's too large for you to wield.");
        return false;
    }

    if (player_mutation_level(MUT_MISSING_HAND)
        && you.hands_reqd(item) == HANDS_TWO)
    {
        return false;
    }

    // don't let undead/demonspawn wield holy weapons/scrolls (out of spite)
    if (!ignore_brand && undead_or_demonic() && is_holy_item(item))
    {
        if (!quiet)
            mpr("This weapon is holy and will not allow you to wield it.");
        return false;
    }

    return true;
}

// Returns the shield the player is wearing, or NULL if none.
item_def *player::shield() const
{
    return slot_item(EQ_SHIELD, false);
}

void player::make_hungry(int hunger_increase, bool silent)
{
    if (hunger_increase > 0)
        ::make_hungry(hunger_increase, silent);
    else if (hunger_increase < 0)
        ::lessen_hunger(-hunger_increase, silent);
}

string player::name(description_level_type dt, bool, bool) const
{
    switch (dt)
    {
    case DESC_NONE:
        return "";
    case DESC_A: case DESC_THE:
    default:
        return "you";
    case DESC_YOUR:
    case DESC_ITS:
        return "your";
    }
}

string player::pronoun(pronoun_type pro, bool /*force_visible*/) const
{
    return decline_pronoun(GENDER_YOU, pro);
}

string player::conj_verb(const string &verb) const
{
    return conjugate_verb(verb, true);
}

/**
 * What's the singular form of a name for the player's current hands?
 *
 * @return A string describing the player's current hand or hand-equivalents.
 */
static string _hand_name_singular()
{
    if (!get_form()->hand_name.empty())
        return get_form()->hand_name;

    if (you.species == SP_FELID)
        return "paw";

    if (you.has_usable_claws())
        return "claw";

    if (you.has_usable_tentacles())
        return "tentacle";

    return "hand";
}

/**
 * What's the the name for the player's hands?
 *
 * @param plural                Whether to use the plural, if possible.
 * @param can_plural[in,out]    Whether this name can be pluralized.
 * @return A string describing the player's current hand or hand-equivalents.
 */
string player::hand_name(bool plural, bool *can_plural) const
{
    bool _can_plural;
    if (can_plural == NULL)
        can_plural = &_can_plural;
    *can_plural = !player_mutation_level(MUT_MISSING_HAND);

    const string singular = _hand_name_singular();
    if (plural && *can_plural)
        return pluralise(singular);

    return singular;
}

/**
 * What's the singular form of a name for the player's current feet?
 *
 * @param can_plural[in,out]    Whether this name can be pluralized.
 * @return A string describing the player's current feet or feet-equivalents.
 */
static string _foot_name_singular(bool *can_plural)
{
    if (!get_form()->foot_name.empty())
        return get_form()->foot_name;

    if (player_mutation_level(MUT_HOOVES) >= 3)
        return "hoof";

    if (you.has_usable_talons())
        return "talon";

    if (you.has_usable_tentacles())
    {
        *can_plural = false;
        return "tentacles";
    }

    if (you.species == SP_NAGA)
    {
        *can_plural = false;
        return "underbelly";
    }

    if (you.species == SP_FELID)
        return "paw";

    if (you.fishtail)
    {
        *can_plural = false;
        return "tail";
    }

    return "foot";
}

/**
 * What's the the name for the player's feet?
 *
 * @param plural                Whether to use the plural, if possible.
 * @param can_plural[in,out]    Whether this name can be pluralized.
 * @return A string describing the player's current feet or feet-equivalents.
 */
string player::foot_name(bool plural, bool *can_plural) const
{
    bool _can_plural;
    if (can_plural == NULL)
        can_plural = &_can_plural;
    *can_plural = true;

    const string singular = _foot_name_singular(can_plural);
    if (plural && *can_plural)
        return pluralise(singular);

    return singular;
}

string player::arm_name(bool plural, bool *can_plural) const
{
    if (form_changed_physiology())
        return hand_name(plural, can_plural);

    if (can_plural != NULL)
        *can_plural = true;

    string adj;
    string str = "arm";

    if (player_genus(GENPC_DRACONIAN) || species == SP_NAGA)
        adj = "scaled";
    else if (species == SP_TENGU)
        adj = "feathered";
    else if (species == SP_MUMMY)
        adj = "bandage-wrapped";
    else if (species == SP_OCTOPODE)
        str = "tentacle";

    if (form == TRAN_LICH)
        adj = "bony";
    else if (form == TRAN_SHADOW)
        adj = "shadowy";

    if (!adj.empty())
        str = adj + " " + str;

    if (plural)
        str = pluralise(str);

    return str;
}

/**
 * What name should be used for the player's means of unarmed attack?
 *
 * (E.g. for display in the top-right of the UI.)
 *
 * @return  A string describing the player's UC attack 'weapon'.
 */
string player::unarmed_attack_name() const
{
    string default_name = "Nothing wielded";

    if (has_usable_claws(true))
    {
        if (species == SP_FELID)
            default_name = "Teeth and claws";
        else
            default_name = "Claws";
    }
    else if (has_usable_tentacles(true))
        default_name = "Tentacles";

    return get_form()->get_uc_attack_name(default_name);
}

bool player::fumbles_attack()
{
    bool did_fumble = false;

    // Fumbling in shallow water.
    if (floundering() || liquefied_ground())
    {
        if (x_chance_in_y(4, dex()) || one_chance_in(5))
        {
            mpr("Your unstable footing causes you to fumble your attack.");
            did_fumble = true;
        }
        if (floundering())
            learned_something_new(HINT_FUMBLING_SHALLOW_WATER);
    }
    return did_fumble;
}

bool player::cannot_fight() const
{
    return false;
}

void player::attacking(actor *other, bool ranged)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!other)
        return;

    if (other->is_monster())
    {
        const monster* mon = other->as_monster();
        if (!mon->friendly() && !mon->neutral())
            pet_target = mon->mindex();
    }

    if (ranged || mons_is_firewood((monster*) other))
        return;

    const int chance = pow(3, player_mutation_level(MUT_BERSERK) - 1);
    if (player_mutation_level(MUT_BERSERK) && x_chance_in_y(chance, 100))
        go_berserk(false);
}

/**
 * Check to see if Chei slows down the berserking player.
 * @param intentional If true, this was initiated by the player, and additional
 *                    messages can be printed if we can't berserk.
 * @return            True if Chei will slow the player, false otherwise.
 */
static bool _chei_prevents_berserk_haste(bool intentional)
{
    if (!you_worship(GOD_CHEIBRIADOS))
        return false;

    // Chei makes berserk not speed you up.
    // Unintentional would be forgiven "just this once" every time.
    // Intentional could work as normal, but that would require storing
    // whether you transgressed to start it -- so we just consider this
    // a part of your penance.
    if (!intentional)
    {
        simple_god_message(" protects you from inadvertent hurry.");
        return true;
    }

    did_god_conduct(DID_HASTY, 8);
    // Let's see if you've lost your religion...
    if (!you_worship(GOD_CHEIBRIADOS))
        return false;

    simple_god_message(" forces you to slow down.");
    return true;
}

/**
 * Make the player go berserk!
 * @param intentional If true, this was initiated by the player, and additional
 *                    messages can be printed if we can't berserk.
 * @param potion      If true, this was caused by the player quaffing !berserk;
 *                    and we get the same additional messages as when
 *                    intentional is true.
 * @return            True if we went berserk, false otherwise.
 */
bool player::go_berserk(bool intentional, bool potion)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!you.can_go_berserk(intentional, potion))
        return false;

    if (stasis_blocks_effect(true,
                             "%s thrums violently and saps your rage.",
                             3,
                             "%s vibrates violently and saps your rage."))
    {
        return false;
    }

    if (crawl_state.game_is_hints())
        Hints.hints_berserk_counter++;

    mpr("A red film seems to cover your vision as you go berserk!");

    if (you.duration[DUR_FINESSE] > 0)
    {
        you.duration[DUR_FINESSE] = 0; // Totally incompatible.
        mpr("Your finesse ends abruptly.");
    }

    if (!_chei_prevents_berserk_haste(intentional))
        mpr("You feel yourself moving faster!");

    mpr("You feel mighty!");

    int berserk_duration = (20 + random2avg(19,2)) / 2;

    you.increase_duration(DUR_BERSERK, berserk_duration);

    calc_hp();
    set_hp(you.hp * 3 / 2);

    deflate_hp(you.hp_max, false);

    if (!you.duration[DUR_MIGHT])
        notify_stat_change(STAT_STR, 5, true);

    if (you.berserk_penalty != NO_BERSERK_PENALTY)
        you.berserk_penalty = 0;

    you.redraw_quiver = true; // Account for no firing.

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC)
    {
        mpr("You burn with rage!");
        // This will get sqrt'd later, so.
        you.temperature = TEMP_MAX;
    }
#endif

    if (player_equip_unrand(UNRAND_JIHAD))
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
            if (mi->friendly())
                mi->go_berserk(false);

    return true;
}

bool player::can_go_berserk() const
{
    return can_go_berserk(false);
}

bool player::can_go_berserk(bool intentional, bool potion, bool quiet) const
{
    const bool verbose = (intentional || potion) && !quiet;

    if (berserk())
    {
        if (verbose)
            mpr("You're already berserk!");
        // or else you won't notice -- no message here.
        return false;
    }

    if (duration[DUR_EXHAUSTED])
    {
        if (verbose)
            mpr("You're too exhausted to go berserk.");
        // or else they won't notice -- no message here
        return false;
    }

    if (duration[DUR_DEATHS_DOOR])
    {
        if (verbose)
            mpr("Your body is effectively dead; that's not a shape for a blood rage.");
        return false;
    }

    if (beheld() && !player_equip_unrand(UNRAND_DEMON_AXE))
    {
        if (verbose)
            mpr("You are too mesmerised to rage.");
        // or else they won't notice -- no message here
        return false;
    }

    if (afraid())
    {
        if (verbose)
            mpr("You are too terrified to rage.");

        return false;
    }

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
    {
        if (verbose)
            mpr("Only creatures of flesh and blood can berserk.");

        return false;
    }

#endif
    if (is_lifeless_undead())
    {
        if (verbose)
            mpr("You cannot raise a blood rage in your lifeless body.");

        return false;
    }

    // Stasis, but only for identified amulets; unided amulets will
    // trigger when the player attempts to activate berserk,
    // auto-iding at that point, but also killing the berserk and
    // wasting a turn.
    if (stasis(false))
    {
        if (verbose)
            mpr("You cannot go berserk while under stasis.");
        return false;
    }

    if (!intentional && !potion && clarity())
    {
        if (verbose)
            mpr("You're too calm and focused to rage.");
        return false;
    }

    COMPILE_CHECK(HUNGER_STARVING - 100 + BERSERK_NUTRITION < HUNGER_VERY_HUNGRY);
    if (hunger <= HUNGER_VERY_HUNGRY)
    {
        if (verbose)
            mpr("You're too hungry to go berserk.");
        return false;
    }

    return true;
}

bool player::berserk() const
{
    return duration[DUR_BERSERK];
}

bool player::can_cling_to_walls() const
{
    return false;
}

bool player::antimagic_susceptible() const
{
    // Maybe check for having non-zero (max) MP?
    return true;
}

bool player::is_web_immune() const
{
    // Spider form
    return form == TRAN_SPIDER;
}

bool player::shove(const char* feat_name)
{
    for (distance_iterator di(pos()); di; ++di)
        if (in_bounds(*di) && !actor_at(*di) && !is_feat_dangerous(grd(*di))
            && can_pass_through_feat(grd(*di)))
        {
            moveto(*di);
            if (*feat_name)
                mprf("You are pushed out of the %s.", feat_name);
            dprf("Moved to (%d, %d).", pos().x, pos().y);
            return true;
        }
    return false;
}

int player::constriction_damage() const
{
    return roll_dice(2, div_rand_round(strength(), 5));
}

int player::heads() const
{
    if (form != TRAN_HYDRA)
        return 1; // not actually always true
    ASSERT(props.exists(HYDRA_FORM_HEADS_KEY));
    return props[HYDRA_FORM_HEADS_KEY].get_int();
}
