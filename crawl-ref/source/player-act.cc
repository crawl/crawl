/**
 * @file
 * @brief Implementing the actor interface for player.
**/

#include "AppHdr.h"

#include "player.h"

#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h" // is_unrandom_artefact (woodcutter)
#include "coordit.h"
#include "dgn-event.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "god-abil.h" // RU_SAC_XP_LEVELS
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::no_haste
#include "hints.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-use.h"
#include "message.h"
#include "movement.h"
#include "player-stats.h"
#include "religion.h"
#include "spl-damage.h"
#include "spl-monench.h"
#include "state.h"
#include "teleport.h"
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
    if (_duration != nullptr)
        *_duration = -1;
    if (summon_type != nullptr)
        *summon_type = 0;

    return false;
}

// n.b. it might be better to use this as player::moveto's function signature
// itself (or something more flexible), but that involves annoying refactoring
// because of the actor/monster signature.
static void _player_moveto(const coord_def &c, bool real_movement, bool clear_net)
{
    if (c != you.pos())
    {
        if (clear_net)
            clear_trapping_net();

        // we need to do this even for fake movement -- otherwise nothing ends
        // the dur for temporal distortion. (I'm not actually sure why?)
        end_wait_spells();
        if (real_movement)
        {
            // Remove spells that break upon movement
            remove_ice_movement();
        }
    }

    crawl_view.set_player_at(c);
    you.set_position(c);

    // clear invalid constrictions even with fake movement
    you.clear_invalid_constrictions();
    you.clear_far_engulf();
}

player_vanishes::player_vanishes(bool _movement)
    : source(you.pos()), movement(_movement)
{
    _player_moveto(coord_def(0,0), movement, true);
}

player_vanishes::~player_vanishes()
{
    if (monster *mon = monster_at(source))
    {
        mon->props[FAKE_BLINK_KEY].get_bool() = true;
        mon->blink();
        mon->props.erase(FAKE_BLINK_KEY);
        if (monster *stubborn = monster_at(source))
            monster_teleport(stubborn, true, true);
    }

    _player_moveto(source, movement, true);
}

void player::moveto(const coord_def &c, bool clear_net)
{
    _player_moveto(c, true, clear_net);
}

bool player::move_to_pos(const coord_def &c, bool clear_net, bool /*force*/)
{
    if (actor_at(c))
        return false;
    moveto(c, clear_net);
    return true;
}

void player::apply_location_effects(const coord_def &oldpos,
                                    killer_type /*killer*/,
                                    int /*killernum*/)
{
    moveto_location_effects(env.grid(oldpos));
}

void player::did_deliberate_movement()
{
    player_did_deliberate_movement();
}

void player::set_position(const coord_def &c)
{
    ASSERT(!crawl_state.game_is_arena());

    const bool real_move = (c != pos());
    actor::set_position(c);

    if (real_move)
    {
        prev_grd_targ.reset();
        if (duration[DUR_QUAD_DAMAGE])
            invalidate_agrid(true);

        if (player_has_orb() || player_equip_unrand(UNRAND_CHARLATANS_ORB))
        {
            if (player_has_orb())
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

bool player::floundering() const
{
    return in_water() && !can_swim() && !extra_balanced();
}

/**
 * Does the player get a balance bonus in their current terrain? If so, they
 * will not count as floundering, even if they can't swim.
 */
bool player::extra_balanced() const
{
    // trees are balanced everywhere they can inhabit.
    return form == transformation::tree
        // Species or forms with large bodies (e.g. nagas) are ok in water.
        // (N.b. all large form sizes can swim anyways, and also giant sized
        // creatures can automatically swim, so the form part is a bit
        // academic at the moment.)
        || body_size(PSIZE_BODY) >= SIZE_LARGE;
}

int player::get_hit_dice() const
{
    return experience_level;
}

int player::get_experience_level() const
{
    return experience_level;
}

int player::get_max_xl() const
{
    return 27 - get_mutation_level(MUT_INEXPERIENCED) * RU_SAC_XP_LEVELS;
}

bool player::can_pass_through_feat(dungeon_feature_type grid) const
{
    return !feat_is_solid(grid) && grid != DNGN_MALIGN_GATEWAY;
}

bool player::is_habitable_feat(dungeon_feature_type actual_grid) const
{
    return can_pass_through_feat(actual_grid)
           && !is_feat_dangerous(actual_grid);
}

size_type player::body_size(size_part_type psize, bool base) const
{
    const auto charsize = species::size(species, psize);
    if (base)
        return charsize;
    else
    {
        size_type tf_size = get_form()->size;
        return tf_size == SIZE_CHARACTER ? charsize : tf_size;
    }
}

vorpal_damage_type player::damage_type(int)
{
    if (const item_def* wp = weapon())
        return get_vorpal_type(*wp);
    if (form == transformation::blade_hands)
        return DAMV_PIERCING;
    if (has_usable_claws())
        return DVORP_CLAWING;
    if (has_usable_tentacles())
        return DVORP_TENTACLE;
    return DVORP_CRUSHING;
}

/**
 * What weapon brand does the player attack with in melee?
 */
brand_type player::damage_brand(int)
{
    // confusing touch always overrides
    if (duration[DUR_CONFUSING_TOUCH])
        return SPWPN_CONFUSE;

    const int wpn = equip[EQ_WEAPON];
    if (wpn != -1 && !melded[EQ_WEAPON])
    {
        if (is_range_weapon(inv[wpn]))
            return SPWPN_NORMAL; // XXX: check !is_melee_weapon instead?
        return get_weapon_brand(inv[wpn]);
    }

    // unarmed

    return get_form()->get_uc_brand();
}


/**
 * Return the delay caused by attacking with your weapon or this projectile.
 *
 * @param projectile  The projectile to be thrown, if any.
 * @param rescale         Whether to re-scale the time to account for the fact that
 *                   finesse doesn't stack with haste.
 * @return           A random_var representing the range of possible values of
 *                   attack delay. It can be casted to an int, in which case
 *                   its value is determined by the appropriate rolls.
 */
random_var player::attack_delay(const item_def *projectile, bool rescale) const
{
    const item_def *primary = weapon();
    const random_var primary_delay = attack_delay_with(projectile, rescale, primary);
    if (projectile && !is_launcher_ammo(*projectile))
        return primary_delay; // throwing doesn't use the offhand

    const item_def *offhand = you.offhand_weapon();
    if (!offhand
        || is_melee_weapon(*offhand) && projectile
        || is_range_weapon(*offhand) && !projectile)
    {
        return primary_delay;
    }

    // re-use of projectile is very dubious here
    const random_var offhand_delay = attack_delay_with(projectile, rescale, offhand);
    return div_rand_round(primary_delay + offhand_delay, 2);
}

random_var player::attack_delay_with(const item_def *projectile, bool rescale,
                                     const item_def *weap) const
{
    // The delay for swinging non-weapons and tossing non-missiles.
    random_var attk_delay(15);
    // a semi-arbitrary multiplier, to minimize loss of precision from integer
    // math.
    const int DELAY_SCALE = 20;

    const bool throwing = projectile && is_throwable(this, *projectile);
    const bool unarmed_attack = !weap && !projectile;
    const bool melee_weapon_attack = !projectile
                                     && weap
                                     && is_melee_weapon(*weap);
    const bool ranged_weapon_attack = projectile
                                      && is_launcher_ammo(*projectile);
    if (throwing)
    {
        // Thrown weapons use 10 + projectile damage to determine base delay.
        const skill_type wpn_skill = SK_THROWING;
        const int projectile_delay = 10 + property(*projectile, PWPN_DAMAGE) / 2;
        attk_delay = random_var(projectile_delay);
        attk_delay -= div_rand_round(random_var(you.skill(wpn_skill, 10)),
                                     DELAY_SCALE);

        // apply minimum to weapon skill modification
        attk_delay = rv::max(attk_delay,
                random_var(FASTEST_PLAYER_THROWING_SPEED));
    }
    else if (unarmed_attack)
    {
        int sk = form_uses_xl() ? experience_level * 10 :
                                  skill(SK_UNARMED_COMBAT, 10);
        attk_delay = random_var(10) - div_rand_round(random_var(sk), 27*2);
    }
    else if (melee_weapon_attack || ranged_weapon_attack)
    {
        const skill_type wpn_skill = item_attack_skill(*weap);
        // Cap skill contribution to mindelay skill, so that rounding
        // doesn't make speed brand benefit from higher skill.
        const int wpn_sklev = min(you.skill(wpn_skill, 10),
                                  10 * weapon_min_delay_skill(*weap));

        attk_delay = random_var(property(*weap, PWPN_SPEED));
        if (is_unrandom_artefact(*weap, UNRAND_WOODCUTTERS_AXE))
            return attk_delay;

        attk_delay -= div_rand_round(random_var(wpn_sklev), DELAY_SCALE);
        // we should really use weapon_adjust_delay here,
        // but we'd need to support random_var
        const brand_type brand = get_weapon_brand(*weap);
        if (brand == SPWPN_SPEED)
            attk_delay = div_rand_round(attk_delay * 2, 3);
        else if (brand == SPWPN_HEAVY)
            attk_delay = div_rand_round(attk_delay * 3, 2);
    }

    // At the moment it never gets this low anyway.
    attk_delay = rv::max(attk_delay, random_var(3));

    attk_delay +=
        div_rand_round(random_var(adjusted_shield_penalty(DELAY_SCALE)),
                       DELAY_SCALE);

    // Slow attacks with ranged weapons, but not clumsy bashes.
    // Don't slow throwing attacks while holding a ranged weapon.
    // Don't slow tossing.
    if (ranged_weapon_attack && is_slowed_by_armour(weap))
    {
        const int aevp = you.adjusted_body_armour_penalty(DELAY_SCALE);
        attk_delay += div_rand_round(random_var(aevp), DELAY_SCALE);
    }

    if (you.duration[DUR_FINESSE])
    {
        ASSERT(!you.duration[DUR_BERSERK]);
        // Finesse shouldn't stack with Haste, so we make this attack take
        // longer so when Haste speeds it up, only Finesse will apply.
        if (you.duration[DUR_HASTE] && rescale)
            attk_delay = haste_mul(attk_delay);
        attk_delay = div_rand_round(attk_delay, 2);
    }

    return rv::max(div_rand_round(attk_delay * player_speed(), BASELINE_DELAY),
                   random_var(1));
}

// Returns the item in the given equipment slot, nullptr if the slot is empty.
// eq must be in [EQ_WEAPON, EQ_RING_AMULET], or bad things will happen.
item_def *player::slot_item(equipment_type eq, bool include_melded) const
{
    ASSERT_RANGE(eq, EQ_FIRST_EQUIP, NUM_EQUIP);

    const int item = equip[eq];
    if (item == -1 || !include_melded && melded[eq])
        return nullptr;
    return const_cast<item_def *>(&inv[item]);
}

// Returns the item in the player's weapon slot.
item_def *player::weapon(int /* which_attack */) const
{
    if (melded[EQ_WEAPON])
        return nullptr;

    return slot_item(EQ_WEAPON, false);
}

// Give hands required to wield weapon.
hands_reqd_type player::hands_reqd(const item_def &item, bool base) const
{
    if (you.has_mutation(MUT_QUADRUMANOUS)
        && (!is_weapon(item) || is_weapon_wieldable(item, SIZE_MEDIUM)))
    {
        return HANDS_ONE;
    }
    return actor::hands_reqd(item, base);
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
        (!ignore_shield && shield())
        || get_mutation_level(MUT_MISSING_HAND)))
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
 * @return                  Whether the player could potentially wield the
 *                          item.
 */
bool player::could_wield(const item_def &item, bool /*ignore_brand*/,
                         bool ignore_transform, bool quiet) const
{
    // Some lingering flavor from the days where sandblast ammo was wielded.
    // harmless.
    if (!can_throw_large_rocks()
        && item.is_type(OBJ_MISSILES, MI_LARGE_ROCK))
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
                mprf("You can't wield %s.", base_type_string(item));
            return false;
        }

        return true;
    }
    else if (you.has_mutation(MUT_NO_GRASPING))
    {
        if (!quiet)
            mpr("You can't use weapons.");
        return false;
    }
    else if (!ignore_transform && !form_can_wield())
    {
        if (!quiet)
            mpr("You can't use weapons in this form.");
        return false;
    }

    const size_type bsize = body_size(PSIZE_TORSO, ignore_transform);
    // Small species wielding large weapons...
    if (!is_weapon_wieldable(item, bsize)
        && !you.has_mutation(MUT_QUADRUMANOUS))
    {
        if (!quiet)
            mpr("That's too large for you to wield.");
        return false;
    }

    if (get_mutation_level(MUT_MISSING_HAND)
        && you.hands_reqd(item) == HANDS_TWO)
    {
        return false;
    }

    return true;
}

// Returns the shield the player is wearing, or nullptr if none.
item_def *player::shield() const
{
    item_def *offhand_item = slot_item(EQ_OFFHAND, false);
    if (!offhand_item || offhand_item->base_type != OBJ_ARMOUR)
        return nullptr;
    return offhand_item;
}

item_def *player::offhand_weapon() const
{
    if (!you.has_mutation(MUT_WIELD_OFFHAND))
        return nullptr;
    item_def *offhand_item = slot_item(EQ_OFFHAND, false);
    if (!offhand_item || !is_weapon(*offhand_item))
        return nullptr;
    // XXX: sanity check for 2hs..?
    return offhand_item;
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
static string _hand_name_singular(bool temp)
{
    // first handle potentially transient hand names
    if (temp && !get_form()->hand_name.empty())
        return get_form()->hand_name;

    if (you.has_mutation(MUT_PAWS, temp))
        return "paw"; // XX redundant with species

    if (you.has_usable_claws())
        return "claw";

    // Storm Form inactivates tentacle constriction, but an octopode's
    // electric body still maintains similar anatomy.
    if (you.has_usable_tentacles(you.form != transformation::storm))
        return "tentacle";

    // Storm Form inactivates the paws mutation, but graphically, a Felid's
    // electric body still maintains similar anatomy.
    if (temp && you.form == transformation::storm
        && you.species == SP_FELID)
    {
        return "paw";
    }

    // For flavor reasons, use "fists" instead of "hands" in various places,
    // but if the creature does have a custom hand name, let the above code
    // preempt it.
    if (temp && (you.form == transformation::statue
                 || you.form == transformation::storm))
    {
        return "fist";
    }

    // player has no usable claws, but has the mutation -- they are suppressed
    // by something. (The species names will give the wrong answer for this
    // case, except for felids, where we want "blade paws".)
    if (you.has_mutation(MUT_CLAWS, false))
        return "hand";

    // then fall back on the species name
    return species::hand_name(you.species);
}

// XX: this is distinct from hand_name because of the actor api
string player::base_hand_name(bool plural, bool temp, bool *can_plural) const
{
    bool _can_plural;
    if (can_plural == nullptr)
        can_plural = &_can_plural;
    // note: octopodes have four primary tentacles, two pairs that are each used
    // like a hand. So even with MUT_MISSING_HAND, they should flavorwise still
    // use plurals when counting arms.
    *can_plural = you.arm_count() > 1;

    string singular;
    // For flavor reasons we use "blade X" in a bunch of places, not just the
    // UC weapon display, where X is the custom hand name. For that reason, we
    // need to do the calculation here.
    if (temp && form == transformation::blade_hands)
        singular += "blade ";
    singular += _hand_name_singular(temp);
    if (plural && *can_plural)
        return pluralise(singular);

    return singular;
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
    return base_hand_name(plural, true, can_plural);
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

    if (you.get_mutation_level(MUT_HOOVES) >= 3)
        return "hoof";

    if (you.has_usable_talons())
        return "talon";

    if (you.has_usable_tentacles())
    {
        *can_plural = false;
        return "tentacles";
    }

    if (you.species == SP_NAGA
        || you.species == SP_DJINNI)
    {
        *can_plural = false;
        return "underbelly";
    }

    if (you.has_mutation(MUT_PAWS))
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
    if (can_plural == nullptr)
        can_plural = &_can_plural;
    *can_plural = true;

    const string singular = _foot_name_singular(can_plural);
    if (plural && *can_plural)
        return pluralise(singular);

    return singular;
}

string player::arm_name(bool plural, bool *can_plural) const
{
    if (form_changes_physiology())
        return hand_name(plural, can_plural);

    if (can_plural != nullptr)
        *can_plural = true;

    string str = species::arm_name(species);

    string adj;
    if (form == transformation::death)
        adj = "fossilised";
    else
        adj = species::skin_name(species, true);

    if (adj != "fleshy")
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
string player::unarmed_attack_name(string default_name) const
{
    if (has_usable_claws(true))
    {
        if (you.has_mutation(MUT_FANGS))
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
    if (floundering()
        || liquefied_ground() && you.duration[DUR_LIQUEFYING] == 0)
    {
        if (x_chance_in_y(3, 8))
        {
            mpr("Your unstable footing causes you to fumble your attack.");
            did_fumble = true;
        }
        if (floundering())
            learned_something_new(HINT_FUMBLING_SHALLOW_WATER);
    }
    return did_fumble;
}

void player::attacking(actor *other)
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
}

/**
 * Check to see if Chei slows down the berserking player.
 * @param intentional If true, this was initiated by the player, and additional
 *                    messages can be printed if we can't berserk.
 * @return            True if Chei will slow the player, false otherwise.
 */
static bool _god_prevents_berserk_haste(bool intentional)
{
    if (!have_passive(passive_t::no_haste))
        return false;

    if (intentional)
        simple_god_message(" forces you to slow down.");
    else
        simple_god_message(" protects you from inadvertent hurry.");

    return true;
}

/**
 * Make the player go berserk!
 * @param intentional If true, this was initiated by the player, so god conduts
 *                    about anger apply.
 * @param potion      If true, this was caused by the player quaffing !berserk;
 *                    and we get additional messages if goingn berserk isn't
 *                    possible.
 * @return            True if we went berserk, false otherwise.
 */
bool player::go_berserk(bool intentional, bool potion)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!you.can_go_berserk(intentional, potion, !potion))
        return false;

    if (crawl_state.game_is_hints())
        Hints.hints_berserk_counter++;

    mpr("A red film seems to cover your vision as you go berserk!");

    if (you.duration[DUR_FINESSE] > 0)
    {
        you.duration[DUR_FINESSE] = 0; // Totally incompatible.
        mpr("Your finesse ends abruptly.");
    }

    if (!_god_prevents_berserk_haste(intentional))
        mpr("You feel yourself moving faster!");

    mpr("You feel mighty!");

    int dur = (20 + random2avg(19,2)) / 2;
    you.increase_duration(DUR_BERSERK, dur);

    // Apply Berserk's +50% Current/Max HP.
    calc_hp(true);

    you.berserk_penalty = 0;

    quiver::set_needs_redraw();

    if (player_equip_unrand(UNRAND_ZEALOT_SWORD))
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
            if (mi->friendly())
                mi->go_berserk(false);

    return true;
}

bool player::can_go_berserk() const
{
    return can_go_berserk(false);
}

bool player::can_go_berserk(bool intentional, bool potion, bool quiet,
                            string *reason, bool temp) const
{
    const bool verbose = (intentional || potion) && !quiet;
    string msg;
    bool success = false;

    if (berserk() && temp)
        msg = "You're already berserk!";
    else if (duration[DUR_BERSERK_COOLDOWN] && temp)
        msg = "You're still recovering from your berserk rage.";
    else if (duration[DUR_DEATHS_DOOR] && temp)
        msg = "You can't enter a blood rage from death's door.";
    else if (beheld() && !player_equip_unrand(UNRAND_DEMON_AXE) && temp)
        msg = "You are too mesmerised to rage.";
    else if (afraid() && temp)
        msg = "You are too terrified to rage.";
    else if (!intentional && !potion && clarity() && temp)
        msg = "You're too calm and focused to rage.";
    else if (is_lifeless_undead(temp))
        msg = "You cannot raise a blood rage in your lifeless body.";
    else if (stasis())
        msg = "Your stasis prevents you from going berserk.";
    else
        success = true;

    if (!success)
    {
        if (verbose)
            mpr(msg);
        if (reason)
            *reason = msg;
    }
    return success;
}

bool player::berserk() const
{
    return duration[DUR_BERSERK];
}

bool player::antimagic_susceptible() const
{
    // Maybe check for having non-zero (max) MP?
    return true;
}

bool player::is_web_immune() const
{
    return is_insubstantial()
        || player_equip_unrand(UNRAND_SLICK_SLIPPERS);
}

bool player::is_binding_sigil_immune() const
{
    return player_equip_unrand(UNRAND_SLICK_SLIPPERS);
}

bool player::shove(const char* feat_name)
{
    for (distance_iterator di(pos()); di; ++di)
        if (in_bounds(*di) && !actor_at(*di) && !is_feat_dangerous(env.grid(*di))
            && can_pass_through_feat(env.grid(*di)))
        {
            moveto(*di);
            if (*feat_name)
                mprf("You are pushed out of the %s.", feat_name);
            dprf("Moved to (%d, %d).", pos().x, pos().y);
            return true;
        }
    return false;
}

/*
 * Calculate base constriction damage.
 *
 * @param typ   The type of constriction the player is doing -
 *              direct (ala Naga/Octopode), BVC, etc.
 * @returns The base damage.
 */
int player::constriction_damage(constrict_type typ) const
{
    switch (typ)
    {
    case CONSTRICT_BVC:
        return roll_dice(2, div_rand_round(80 +
                   you.props[VILE_CLUTCH_POWER_KEY].get_int(), 20));
    case CONSTRICT_ROOTS:
        // Assume we're using the wand.
        // Min power 2d5, max power ~2d19
        return roll_dice(2, div_rand_round(25 +
                    you.props[FASTROOT_POWER_KEY].get_int(), 7));
    default:
        return roll_dice(2, div_rand_round(5 * (22 + 5 * you.experience_level), 81));
    }

}

bool player::is_dragonkind() const
{
    if (actor::is_dragonkind())
        return true;
    return you.form == transformation::dragon;
}
