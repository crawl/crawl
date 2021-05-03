#include "AppHdr.h"

#include "player-equip.h"

#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "delay.h"
#include "english.h" // conjugate_verb
#include "god-abil.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h"
#include "hints.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h" // command_to_string
#include "monster.h"
#include "message.h"
#include "nearby-danger.h"
#include "notes.h"
#include "player-stats.h"
#include "religion.h"
#include "shopping.h"
#include "spl-clouds.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-wpnench.h"
#include "stringutil.h"
#include "tag-version.h"
#include "xom.h"

static void _mark_unseen_monsters();

/**
 * Recalculate the player's max hp and set the current hp based on the %change
 * of max hp. This has resulted from our having equipped an artefact that
 * changes max hp.
 */
static void _calc_hp_artefact()
{
    calc_hp();
    if (you.hp_max <= 0) // Borgnjor's abusers...
        ouch(0, KILLED_BY_DRAINING);
}

static void _flight_equip()
{
    if (you.airborne()) // already aloft
        mpr("You feel rather light.");
    else
        float_player();
    you.attribute[ATTR_PERM_FLIGHT] = 1;
}

// Fill an empty equipment slot.
void equip_item(equipment_type slot, int item_slot, bool msg)
{
    ASSERT_RANGE(slot, EQ_FIRST_EQUIP, NUM_EQUIP);
    ASSERT(you.equip[slot] == -1);
    ASSERT(!you.melded[slot]);

    you.equip[slot] = item_slot;

    equip_effect(slot, item_slot, false, msg);
    you.gear_change = true;
}

// Clear an equipment slot (possibly melded).
bool unequip_item(equipment_type slot, bool msg, bool skip_effects)
{
    ASSERT_RANGE(slot, EQ_FIRST_EQUIP, NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    const int item_slot = you.equip[slot];
    if (item_slot == -1)
        return false;
    else
    {
        you.equip[slot] = -1;

        if (you.melded[slot])
            you.melded.set(slot, false);
        else if (!skip_effects)
            unequip_effect(slot, item_slot, false, msg);

        ash_check_bondage();
        you.last_unequip = item_slot;
        you.gear_change = true;
        return true;
    }
}

// Meld a slot (if equipped).
// Does not handle unequip effects, since melding should be simultaneous (so
// you should call all unequip effects after all melding is done)
bool meld_slot(equipment_type slot)
{
    ASSERT_RANGE(slot, EQ_FIRST_EQUIP, NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && !you.melded[slot])
    {
        you.melded.set(slot);
        you.gear_change = true;
        return true;
    }
    return false;
}

// Does not handle equip effects, since unmelding should be simultaneous (so
// you should call all equip effects after all unmelding is done)
bool unmeld_slot(equipment_type slot)
{
    ASSERT_RANGE(slot, EQ_FIRST_EQUIP, NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && you.melded[slot])
    {
        you.melded.set(slot, false);
        you.gear_change = true;
        return true;
    }
    return false;
}

static void _equip_weapon_effect(item_def& item, bool showMsgs, bool unmeld);
static void _unequip_weapon_effect(item_def& item, bool showMsgs, bool meld);
static void _equip_armour_effect(item_def& arm, bool unmeld,
                                 equipment_type slot);
static void _unequip_armour_effect(item_def& item, bool meld,
                                   equipment_type slot);
static void _equip_jewellery_effect(item_def &item, bool unmeld,
                                    equipment_type slot);
static void _unequip_jewellery_effect(item_def &item, bool mesg, bool meld,
                                      equipment_type slot);
static void _equip_use_warning(const item_def& item);
static void _equip_regeneration_item(const item_def& item);
static void _deactivate_regeneration_item(const item_def& item, bool meld);

static void _assert_valid_slot(equipment_type eq, equipment_type slot)
{
#ifdef ASSERTS
    if (eq == slot)
        return;
    ASSERT(eq == EQ_RINGS); // all other slots are unique
    equipment_type r1 = EQ_LEFT_RING, r2 = EQ_RIGHT_RING;
    if (species::arm_count(you.species) > 2)
        r1 = EQ_RING_ONE, r2 = EQ_RING_EIGHT;
    if (slot >= r1 && slot <= r2)
        return;
    if (const item_def* amu = you.slot_item(EQ_AMULET, true))
        if (is_unrandom_artefact(*amu, UNRAND_FINGER_AMULET) && slot == EQ_RING_AMULET)
            return;
    die("ring on invalid slot %d", slot);
#endif
}

void equip_effect(equipment_type slot, int item_slot, bool unmeld, bool msg)
{
    item_def& item = you.inv[item_slot];
    equipment_type eq = get_item_slot(item);

    if (slot == EQ_WEAPON && eq != EQ_WEAPON)
        return;

    _assert_valid_slot(eq, slot);

    if (msg)
        _equip_use_warning(item);

    const interrupt_block block_unmeld_interrupts(unmeld);

    identify_item(item);

    if (slot == EQ_WEAPON)
        _equip_weapon_effect(item, msg, unmeld);
    else if (slot >= EQ_CLOAK && slot <= EQ_BODY_ARMOUR)
        _equip_armour_effect(item, unmeld, slot);
    else if (slot >= EQ_FIRST_JEWELLERY && slot <= EQ_LAST_JEWELLERY)
        _equip_jewellery_effect(item, unmeld, slot);
}

void unequip_effect(equipment_type slot, int item_slot, bool meld, bool msg)
{
    item_def& item = you.inv[item_slot];
    equipment_type eq = get_item_slot(item);

    if (slot == EQ_WEAPON && eq != EQ_WEAPON)
        return;

    _assert_valid_slot(eq, slot);

    const interrupt_block block_meld_interrupts(meld);

    if (slot == EQ_WEAPON)
        _unequip_weapon_effect(item, msg, meld);
    else if (slot >= EQ_CLOAK && slot <= EQ_BODY_ARMOUR)
        _unequip_armour_effect(item, meld, slot);
    else if (slot >= EQ_FIRST_JEWELLERY && slot <= EQ_LAST_JEWELLERY)
        _unequip_jewellery_effect(item, msg, meld, slot);

    if (item.cursed() && !meld)
        destroy_item(item);
}

///////////////////////////////////////////////////////////
// Actual equip and unequip effect implementation below
//

static void _equip_artefact_effect(item_def &item, bool *show_msgs, bool unmeld,
                                   equipment_type slot)
{
    ASSERT(is_artefact(item));

    // Call unrandart equip function first, so that it can modify the
    // artefact's properties before they're applied.
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);

        if (entry->equip_func)
            entry->equip_func(&item, show_msgs, unmeld);

        if (entry->world_reacts_func)
            you.unrand_reacts.set(slot);
    }

    const bool msg          = !show_msgs || *show_msgs;

    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_properties(item, proprt);
    artefact_known_properties(item, known);

    if (proprt[ARTP_AC] || proprt[ARTP_SHIELDING])
        you.redraw_armour_class = true;

    if (proprt[ARTP_EVASION])
        you.redraw_evasion = true;

    if (proprt[ARTP_SEE_INVISIBLE])
        autotoggle_autopickup(false);

    if (proprt[ARTP_MAGICAL_POWER] && !known[ARTP_MAGICAL_POWER] && msg)
    {
        canned_msg(proprt[ARTP_MAGICAL_POWER] > 0 ? MSG_MANA_INCREASE
                                                  : MSG_MANA_DECREASE);
    }

    if (proprt[ARTP_REGENERATION]
        && !unmeld
        // If regen is an intrinsic property too, don't double print messages.
        && !item.is_type(OBJ_JEWELLERY, AMU_REGENERATION)
        && (item.base_type != OBJ_ARMOUR
            || !armour_type_prop(item.sub_type, ARMF_REGENERATION)))
    {
        _equip_regeneration_item(item);
    }

    // Modify ability scores.
    notify_stat_change(STAT_STR, proprt[ARTP_STRENGTH],
                       !(msg && proprt[ARTP_STRENGTH] && !unmeld));
    notify_stat_change(STAT_INT, proprt[ARTP_INTELLIGENCE],
                       !(msg && proprt[ARTP_INTELLIGENCE] && !unmeld));
    notify_stat_change(STAT_DEX, proprt[ARTP_DEXTERITY],
                       !(msg && proprt[ARTP_DEXTERITY] && !unmeld));

    if (proprt[ARTP_FLY])
        _flight_equip();

    if (proprt[ARTP_CONTAM] && msg && !unmeld)
        mpr("You feel a build-up of mutagenic energy.");

    if (proprt[ARTP_RAMPAGING] && msg && !unmeld)
        mpr("You feel ready to rampage towards enemies.");

    if (proprt[ARTP_HP])
        _calc_hp_artefact();

    // Let's try this here instead of up there.
    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();
}

static void _unequip_fragile_artefact(item_def& item, bool meld)
{
    ASSERT(is_artefact(item));

    if (artefact_property(item, ARTP_FRAGILE) && !meld)
    {
        mprf("%s crumbles to dust!", item.name(DESC_THE).c_str());
        dec_inv_item_quantity(item.link, 1);
    }
}

static void _unequip_artefact_effect(item_def &item,
                                     bool *show_msgs, bool meld,
                                     equipment_type slot,
                                     bool weapon)
{
    ASSERT(is_artefact(item));

    artefact_properties_t proprt;
    artefact_known_props_t known;
    artefact_properties(item, proprt);
    artefact_known_properties(item, known);
    const bool msg = !show_msgs || *show_msgs;

    if (proprt[ARTP_AC] || proprt[ARTP_SHIELDING])
        you.redraw_armour_class = true;

    if (proprt[ARTP_EVASION])
        you.redraw_evasion = true;

    if (proprt[ARTP_HP])
        _calc_hp_artefact();

    if (proprt[ARTP_MAGICAL_POWER] && !known[ARTP_MAGICAL_POWER] && msg)
    {
        canned_msg(proprt[ARTP_MAGICAL_POWER] > 0 ? MSG_MANA_DECREASE
                                                  : MSG_MANA_INCREASE);
    }

    notify_stat_change(STAT_STR, -proprt[ARTP_STRENGTH],     true);
    notify_stat_change(STAT_INT, -proprt[ARTP_INTELLIGENCE], true);
    notify_stat_change(STAT_DEX, -proprt[ARTP_DEXTERITY],    true);

    if (proprt[ARTP_FLY] != 0)
        land_player();

    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();

    if (proprt[ARTP_CONTAM] && !meld)
    {
        mpr("Mutagenic energies flood into your body!");
        contaminate_player(7000, true);
    }

    if (proprt[ARTP_RAMPAGING] && !you.rampaging() && msg && !meld)
        mpr("You no longer feel able to rampage towards enemies.");

    if (proprt[ARTP_DRAIN] && !meld)
        drain_player(150, true, true);

    if (proprt[ARTP_SEE_INVISIBLE])
        _mark_unseen_monsters();

    if (proprt[ARTP_REGENERATION])
        _deactivate_regeneration_item(item, meld);

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);

        if (entry->unequip_func)
            entry->unequip_func(&item, show_msgs);

        if (entry->world_reacts_func)
            you.unrand_reacts.set(slot, false);
    }

    // If the item is a weapon, then we call it from unequip_weapon_effect
    // separately, to make sure the message order makes sense.
    if (!weapon)
        _unequip_fragile_artefact(item, meld);
}

static void _equip_use_warning(const item_def& item)
{
    if (is_holy_item(item) && you_worship(GOD_YREDELEMNUL))
        mpr("You really shouldn't be using a holy item like this.");
    else if (is_evil_item(item) && is_good_god(you.religion))
        mpr("You really shouldn't be using an evil item like this.");
    else if (is_unclean_item(item) && you_worship(GOD_ZIN))
        mpr("You really shouldn't be using an unclean item like this.");
    else if (is_chaotic_item(item) && you_worship(GOD_ZIN))
        mpr("You really shouldn't be using a chaotic item like this.");
    else if (is_hasty_item(item) && you_worship(GOD_CHEIBRIADOS))
        mpr("You really shouldn't be using a hasty item like this.");
    else if (is_wizardly_item(item) && you_worship(GOD_TROG))
        mpr("You really shouldn't be using a wizardly item like this.");
}

// Provide a function for handling initial wielding of 'special'
// weapons, or those whose function is annoying to reproduce in
// other places *cough* auto-butchering *cough*.    {gdl}
static void _equip_weapon_effect(item_def& item, bool showMsgs, bool unmeld)
{
    you.wield_change = true;
    quiver::on_weapon_changed();
    int special = 0;

    const bool artefact     = is_artefact(item);

    // And here we finally get to the special effects of wielding. {dlb}
    switch (item.base_type)
    {
    case OBJ_STAVES:
    {
        break;
    }

    case OBJ_WEAPONS:
    {
        // Note that if the unrand equip prints a message, it will
        // generally set showMsgs to false.
        if (artefact)
            _equip_artefact_effect(item, &showMsgs, unmeld, EQ_WEAPON);

        special = item.brand;

        if (artefact)
            special = artefact_property(item, ARTP_BRAND);

        if (special != SPWPN_NORMAL)
        {
            // message first
            if (showMsgs)
            {
                const string item_name = item.name(DESC_YOUR);
                switch (special)
                {
                case SPWPN_FLAMING:
                    mprf("%s bursts into flame!", item_name.c_str());
                    break;

                case SPWPN_FREEZING:
                   mprf("%s %s", item_name.c_str(),
                        is_range_weapon(item) ?
                            "is covered in frost." :
                            "glows with a cold blue light!");
                    break;

                case SPWPN_HOLY_WRATH:
                    mprf("%s softly glows with a divine radiance!",
                         item_name.c_str());
                    break;

                case SPWPN_ELECTROCUTION:
                    if (!silenced(you.pos()))
                    {
                        mprf(MSGCH_SOUND,
                             "You hear the crackle of electricity.");
                    }
                    else
                        mpr("You see sparks fly.");
                    break;

                case SPWPN_VENOM:
                    mprf("%s begins to drip with poison!", item_name.c_str());
                    break;

                case SPWPN_PROTECTION:
                    mprf("%s hums with potential!", item_name.c_str());
                    break;

                case SPWPN_DRAINING:
                    mpr("You sense an unholy aura.");
                    break;

                case SPWPN_SPEED:
                    mpr(you.hands_act("tingle", "!"));
                    break;

                case SPWPN_VAMPIRISM:
                    if (you.has_mutation(MUT_VAMPIRISM))
                        mpr("You feel a bloodthirsty glee!");
                    else
                        mpr("You feel a sense of dread.");
                    break;

                case SPWPN_PAIN:
                {
                    const string your_arm = you.arm_name(false);
                    if (you.skill(SK_NECROMANCY) == 0)
                        mpr("You have a feeling of ineptitude.");
                    else if (you.skill(SK_NECROMANCY) <= 6)
                    {
                        mprf("Pain shudders through your %s!",
                             your_arm.c_str());
                    }
                    else
                    {
                        mprf("A searing pain shoots up your %s!",
                             your_arm.c_str());
                    }
                    break;
                }

                case SPWPN_CHAOS:
                    mprf("%s is briefly surrounded by a scintillating aura of "
                         "random colours.", item_name.c_str());
                    break;

                case SPWPN_PENETRATION:
                {
                    // FIXME: make hands_act take a pre-verb adverb so we can
                    // use it here.
                    bool plural = true;
                    string hand = you.hand_name(true, &plural);

                    mprf("Your %s briefly %s through it before you manage "
                         "to get a firm grip on it.",
                         hand.c_str(), conjugate_verb("pass", plural).c_str());
                    break;
                }

                case SPWPN_REAPING:
                    mprf("%s is briefly surrounded by shifting shadows.",
                         item_name.c_str());
                    break;

                case SPWPN_ANTIMAGIC:
                    // Even if your maxmp is 0.
                    mpr("You feel magic leave you.");
                    break;

                case SPWPN_DISTORTION:
                    mpr("Space warps around you for a moment!");
                    break;

                case SPWPN_ACID:
                    mprf("%s begins to ooze corrosive slime!", item_name.c_str());
                    break;

                case SPWPN_SPECTRAL:
                    mprf("You feel a bond with your %s.", item_name.c_str());
                    break;

                default:
                    break;
                }
            }

            if (special == SPWPN_ANTIMAGIC)
                calc_mp();
        }

        break;
    }
    default:
        break;
    }
}

static void _unequip_weapon_effect(item_def& real_item, bool showMsgs,
                                   bool meld)
{
    you.wield_change = true;
    quiver::on_weapon_changed();

    // Fragile artefacts may be destroyed, so make a copy
    item_def item = real_item;

    // Call this first, so that the unrandart func can set showMsgs to
    // false if it does its own message handling.
    if (is_artefact(item))
    {
        _unequip_artefact_effect(real_item, &showMsgs, meld, EQ_WEAPON,
                                 true);
    }

    if (item.base_type == OBJ_WEAPONS)
    {
        const int brand = get_weapon_brand(item);

        if (brand != SPWPN_NORMAL)
        {
            const string msg = item.name(DESC_YOUR);

            switch (brand)
            {
            case SPWPN_FLAMING:
                if (showMsgs)
                    mprf("%s stops flaming.", msg.c_str());
                break;

            case SPWPN_FREEZING:
            case SPWPN_HOLY_WRATH:
                if (showMsgs)
                    mprf("%s stops glowing.", msg.c_str());
                break;

            case SPWPN_ELECTROCUTION:
                if (showMsgs)
                    mprf("%s stops crackling.", msg.c_str());
                break;

            case SPWPN_VENOM:
                if (showMsgs)
                    mprf("%s stops dripping with poison.", msg.c_str());
                break;

            case SPWPN_PROTECTION:
                if (showMsgs)
                    mprf("%s goes still.", msg.c_str());
                if (you.duration[DUR_SPWPN_PROTECTION])
                {
                    you.duration[DUR_SPWPN_PROTECTION] = 0;
                    you.redraw_armour_class = true;
                }
                break;

            case SPWPN_VAMPIRISM:
                if (showMsgs)
                {
                    if (you.has_mutation(MUT_VAMPIRISM))
                        mpr("You feel your glee subside.");
                    else
                        mpr("You feel the dreadful sensation subside.");
                }
                break;

            case SPWPN_DISTORTION:
                if (!meld)
                    unwield_distortion();

                break;

            case SPWPN_ANTIMAGIC:
                calc_mp();
                mpr("You feel magic returning to you.");
                break;

            case SPWPN_SPECTRAL:
                {
                    monster *spectral_weapon = find_spectral_weapon(&you);
                    if (spectral_weapon)
                    {
                        mprf("Your spectral weapon disappears as %s.",
                             meld ? "your weapon melds" : "you unwield");
                        end_spectral_weapon(spectral_weapon, false, true);
                    }
                }
                break;

                // NOTE: When more are added here, *must* duplicate unwielding
                // effect in brand weapon scroll effect in read_scroll.

            case SPWPN_ACID:
                mprf("%s stops oozing corrosive slime.", msg.c_str());
                break;
            }

            if (you.duration[DUR_EXCRUCIATING_WOUNDS])
            {
                ASSERT(real_item.defined());
                end_weapon_brand(real_item, true);
            }
        }
    }

    if (is_artefact(item))
        _unequip_fragile_artefact(item, meld);
}

static void _spirit_shield_message(bool unmeld)
{
    if (!unmeld && you.spirit_shield() < 2 && !you.has_mutation(MUT_HP_CASTING))
    {
        mpr("You feel your power drawn to a protective spirit.");
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_DEEP_DWARF
            && !(have_passive(passive_t::no_mp_regen)
                 || player_under_penance(GOD_PAKELLAS)))
        {
            drain_mp(you.magic_points);
            mpr("Now linked to your health, your magic stops regenerating.");
        }
#endif
    }
    else if (!unmeld && (you.get_mutation_level(MUT_MANA_SHIELD)
                         || you.has_mutation(MUT_HP_CASTING)))
    {
        mpr("You feel the presence of a powerless spirit.");
    }
    else if (!you.get_mutation_level(MUT_MANA_SHIELD))
        mpr("You feel spirits watching over you.");
}

static void _equip_armour_effect(item_def& arm, bool unmeld,
                                 equipment_type slot)
{
    int ego = get_armour_ego_type(arm);
    if (ego != SPARM_NORMAL)
    {
        switch (ego)
        {
        case SPARM_FIRE_RESISTANCE:
            mpr("You feel resistant to fire.");
            break;

        case SPARM_COLD_RESISTANCE:
            mpr("You feel resistant to cold.");
            break;

        case SPARM_POISON_RESISTANCE:
            if (player_res_poison(false, false, false) < 3)
                mpr("You feel resistant to poison.");
            break;

        case SPARM_SEE_INVISIBLE:
            mpr("You feel perceptive.");
            autotoggle_autopickup(false);
            break;

        case SPARM_INVISIBILITY:
            if (!you.duration[DUR_INVIS])
                mpr("You become transparent for a moment.");
            break;

        case SPARM_STRENGTH:
            notify_stat_change(STAT_STR, 3, false);
            break;

        case SPARM_DEXTERITY:
            notify_stat_change(STAT_DEX, 3, false);
            break;

        case SPARM_INTELLIGENCE:
            notify_stat_change(STAT_INT, 3, false);
            break;

        case SPARM_PONDEROUSNESS:
            mpr("You feel rather ponderous.");
            break;

        case SPARM_FLYING:
            _flight_equip();
            break;

        case SPARM_WILLPOWER:
            mpr("You feel strong-willed.");
            break;

        case SPARM_PROTECTION:
            mpr("You feel protected.");
            break;

        case SPARM_STEALTH:
            if (!you.get_mutation_level(MUT_NO_STEALTH))
                mpr("You feel stealthy.");
            break;

        case SPARM_RESISTANCE:
            mpr("You feel resistant to extremes of temperature.");
            break;

        case SPARM_POSITIVE_ENERGY:
            mpr("You feel more protected from negative energy.");
            break;

        case SPARM_ARCHMAGI:
            if (!you.skill(SK_SPELLCASTING))
                mpr("You feel strangely lacking in power.");
            else
                mpr("You feel powerful.");
            break;

        case SPARM_SPIRIT_SHIELD:
            _spirit_shield_message(unmeld);
            break;

        case SPARM_ARCHERY:
            mpr("You feel that your aim is more steady.");
            break;

        case SPARM_REPULSION:
            mpr("You are surrounded by a repulsion field.");
            break;

        case SPARM_SHADOWS:
            mpr("It gets dark.");
            update_vision_range();
            break;

        case SPARM_RAMPAGING:
            mpr("You feel ready to rampage towards enemies.");
            break;
        }
    }

    if (armour_type_prop(arm.sub_type, ARMF_REGENERATION) && !unmeld)
        _equip_regeneration_item(arm);

    if (is_artefact(arm))
    {
        bool show_msgs = true;
        _equip_artefact_effect(arm, &show_msgs, unmeld, slot);
    }

    you.redraw_armour_class = true;
    you.redraw_evasion = true;
}

static void _unequip_armour_effect(item_def& item, bool meld,
                                   equipment_type slot)
{
    you.redraw_armour_class = true;
    you.redraw_evasion = true;

    switch (get_armour_ego_type(item))
    {
    case SPARM_FIRE_RESISTANCE:
        mpr("You feel less resistant to fire.");
        break;

    case SPARM_COLD_RESISTANCE:
        mpr("You feel less resistant to cold.");
        break;

    case SPARM_POISON_RESISTANCE:
        if (player_res_poison() <= 0)
            mpr("You no longer feel resistant to poison.");
        break;

    case SPARM_SEE_INVISIBLE:
        if (!you.can_see_invisible())
        {
            mpr("You feel less perceptive.");
            _mark_unseen_monsters();
        }
        break;

    case SPARM_STRENGTH:
        notify_stat_change(STAT_STR, -3, false);
        break;

    case SPARM_DEXTERITY:
        notify_stat_change(STAT_DEX, -3, false);
        break;

    case SPARM_INTELLIGENCE:
        notify_stat_change(STAT_INT, -3, false);
        break;

    case SPARM_PONDEROUSNESS:
    {
        // XX can the noun here be derived from the species walking verb?
        const string noun = you.species == SP_NAGA ? "slither" : "step";
        mprf("That put a bit of spring back into your %s.", noun.c_str());
        break;
    }

    case SPARM_FLYING:
        land_player();
        break;

    case SPARM_WILLPOWER:
        mpr("You feel less strong-willed.");
        break;

    case SPARM_PROTECTION:
        mpr("You feel less protected.");
        break;

    case SPARM_STEALTH:
        if (!you.get_mutation_level(MUT_NO_STEALTH))
            mpr("You feel less stealthy.");
        break;

    case SPARM_RESISTANCE:
        mpr("You feel hot and cold all over.");
        break;

    case SPARM_POSITIVE_ENERGY:
        mpr("You feel less protected from negative energy.");
        break;

    case SPARM_ARCHMAGI:
        mpr("You feel strangely numb.");
        break;

    case SPARM_SPIRIT_SHIELD:
        if (!you.spirit_shield())
        {
            mpr("You feel strangely alone.");
            if (you.species == SP_DEEP_DWARF)
                mpr("Your magic begins regenerating once more.");
        }
        break;

    case SPARM_ARCHERY:
        mpr("Your aim is not that steady anymore.");
        break;

    case SPARM_REPULSION:
        mpr("The haze of the repulsion field disappears.");
        break;

    case SPARM_SHADOWS:
        mpr("The dungeon's light returns to normal.");
        update_vision_range();
        break;

    case SPARM_RAMPAGING:
        if (!you.rampaging())
            mpr("You no longer feel able to rampage towards enemies.");
        break;

    default:
        break;
    }

    if (armour_type_prop(item.sub_type, ARMF_REGENERATION))
        _deactivate_regeneration_item(item, meld);

    if (is_artefact(item))
        _unequip_artefact_effect(item, nullptr, meld, slot, false);
}

static void _remove_amulet_of_faith(item_def &item)
{
#ifndef DEBUG_DIAGNOSTICS
    UNUSED(item);
#endif
    if (you_worship(GOD_RU))
    {
        // next sacrifice is going to be delaaaayed.
        if (you.piety < piety_breakpoint(5))
        {
#ifdef DEBUG_DIAGNOSTICS
            const int cur_delay = you.props[RU_SACRIFICE_DELAY_KEY].get_int();
#endif
            ru_reject_sacrifices(true);
            dprf("prev delay %d, new delay %d", cur_delay,
                 you.props[RU_SACRIFICE_DELAY_KEY].get_int());
        }
    }
    else if (!you_worship(GOD_NO_GOD)
             && !you_worship(GOD_XOM)
             && !you_worship(GOD_GOZAG))
    {
        simple_god_message(" seems less interested in you.");

        const int piety_loss = div_rand_round(you.piety, 3);
        // Piety penalty for removing the Amulet of Faith.
        if (you.piety - piety_loss > 10)
        {
            mprf(MSGCH_GOD, "You feel less pious.");
            dprf("%s: piety drain: %d",
                 item.name(DESC_PLAIN).c_str(), piety_loss);
            lose_piety(piety_loss);
        }
    }
}

static void _equip_regeneration_item(const item_def &item)
{
    equipment_type eq_slot = item_equip_slot(item);
    // currently regen is only on amulets and armour
    bool plural = eq_slot == EQ_GLOVES || eq_slot == EQ_BOOTS;
    string item_name = is_artefact(item) ? get_artefact_name(item)
                                         : eq_slot == EQ_AMULET
                                         ? "amulet"
                                         : eq_slot == EQ_BODY_ARMOUR
                                         ? "armour"
                                         : item_slot_name(eq_slot);

    if (you.get_mutation_level(MUT_NO_REGENERATION))
    {
        mprf("The %s feel%s cold and inert.", item_name.c_str(),
             plural ? "" : "s");
        return;
    }
    if (you.hp == you.hp_max)
    {
        mprf("The %s throb%s to your uninjured body.", item_name.c_str(),
             plural ? " as they attune themselves" : "s as it attunes itself");
        you.activated.set(eq_slot);
        return;
    }
    mprf("The %s cannot attune %s to your injured body.", item_name.c_str(),
         plural ? "themselves" : "itself");
    you.activated.set(eq_slot, false);
    return;
}

bool acrobat_boost_active()
{
    return you.wearing(EQ_AMULET, AMU_ACROBAT)
           && you.duration[DUR_ACROBAT]
           && (!you.caught())
           && (!you.is_constricted());
}

static void _equip_amulet_of_mana_regeneration()
{
    if (!player_regenerates_mp())
        mpr("The amulet feels cold and inert.");
    else if (you.magic_points == you.max_magic_points)
        you.props[MANA_REGEN_AMULET_ACTIVE] = 1;
    else
    {
        mpr("The amulet cannot attune itself to your exhausted body.");
        you.props[MANA_REGEN_AMULET_ACTIVE] = 0;
    }
}

static void _equip_amulet_of_reflection()
{
    you.redraw_armour_class = true;
    mpr("You feel a shielding aura gather around you.");
}

static void _equip_jewellery_effect(item_def &item, bool unmeld,
                                    equipment_type slot)
{
    const bool artefact     = is_artefact(item);

    switch (item.sub_type)
    {
    case RING_FIRE:
        mpr("You feel more attuned to fire.");
        break;

    case RING_ICE:
        mpr("You feel more attuned to ice.");
        break;

    case RING_SEE_INVISIBLE:
        if (item_type_known(item))
            autotoggle_autopickup(false);
        break;

    case RING_FLIGHT:
        _flight_equip();
        break;

    case RING_PROTECTION:
        you.redraw_armour_class = true;
        break;

    case RING_EVASION:
        you.redraw_evasion = true;
        break;

    case RING_STRENGTH:
        notify_stat_change(STAT_STR, item.plus, false);
        break;

    case RING_DEXTERITY:
        notify_stat_change(STAT_DEX, item.plus, false);
        break;

    case RING_INTELLIGENCE:
        notify_stat_change(STAT_INT, item.plus, false);
        break;

    case RING_MAGICAL_POWER:
        if (you.has_mutation(MUT_HP_CASTING))
        {
            mpr("You repel a surge of foreign magic.");
            break;
        }
        canned_msg(MSG_MANA_INCREASE);
        calc_mp();
        break;

    case AMU_FAITH:
        if (you.has_mutation(MUT_FORLORN))
            mpr("You feel a surge of self-confidence.");
        else if (you_worship(GOD_RU) && you.piety >= piety_breakpoint(5))
        {
            simple_god_message(" says: An ascetic of your devotion"
                               " has no use for such trinkets.");
        }
        else if (you_worship(GOD_GOZAG))
            simple_god_message(" cares for nothing but gold!");
        else
        {
            mprf(MSGCH_GOD, "You feel a %ssurge of divine interest.",
                            you_worship(GOD_NO_GOD) ? "strange " : "");
        }

        break;

    case AMU_REGENERATION:
        if (!unmeld)
            _equip_regeneration_item(item);
        break;

    case AMU_ACROBAT:
        if (!unmeld)
            mpr("You feel ready to tumble and roll out of harm's way.");
        break;

    case AMU_MANA_REGENERATION:
        if (!unmeld)
            _equip_amulet_of_mana_regeneration();
        break;

    case AMU_REFLECTION:
        if (!unmeld)
            _equip_amulet_of_reflection();
        break;

    case AMU_GUARDIAN_SPIRIT:
        _spirit_shield_message(unmeld);
        break;
    }

    if (artefact)
    {
        bool show_msgs = true;
        _equip_artefact_effect(item, &show_msgs, unmeld, slot);
    }

    if (!unmeld)
        mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
}

static void _deactivate_regeneration_item(const item_def &item, bool meld)
{
    if (!meld)
        you.activated.set(get_item_slot(item), false);
}

static void _unequip_jewellery_effect(item_def &item, bool mesg, bool meld,
                                      equipment_type slot)
{
    // The ring/amulet must already be removed from you.equip at this point.
    switch (item.sub_type)
    {
    case RING_FIRE:
    case RING_ICE:
    case RING_LIFE_PROTECTION:
    case RING_POISON_RESISTANCE:
    case RING_PROTECTION_FROM_COLD:
    case RING_PROTECTION_FROM_FIRE:
    case RING_WILLPOWER:
    case RING_SLAYING:
    case RING_STEALTH:
    case RING_WIZARDRY:
        break;

    case AMU_REGENERATION:
        _deactivate_regeneration_item(item, meld);
        break;

    case AMU_ACROBAT:
        if (!meld)
            you.activated.set(EQ_AMULET, false);
        break;

    case RING_SEE_INVISIBLE:
        _mark_unseen_monsters();
        break;

    case RING_PROTECTION:
        you.redraw_armour_class = true;
        break;

    case AMU_REFLECTION:
        if (!meld)
            you.activated.set(EQ_AMULET, false);
        you.redraw_armour_class = true;
        break;

    case RING_EVASION:
        you.redraw_evasion = true;
        break;

    case RING_STRENGTH:
        notify_stat_change(STAT_STR, -item.plus, false);
        break;

    case RING_DEXTERITY:
        notify_stat_change(STAT_DEX, -item.plus, false);
        break;

    case RING_INTELLIGENCE:
        notify_stat_change(STAT_INT, -item.plus, false);
        break;

    case RING_FLIGHT:
        land_player();
        break;

    case RING_MAGICAL_POWER:
        if (!you.has_mutation(MUT_HP_CASTING))
            canned_msg(MSG_MANA_DECREASE);
        break;

    case AMU_FAITH:
        if (!meld)
            _remove_amulet_of_faith(item);
        break;

    case AMU_GUARDIAN_SPIRIT:
        if (you.species == SP_DEEP_DWARF && player_regenerates_mp())
            mpr("Your magic begins regenerating once more.");
        break;
    }

    if (is_artefact(item))
        _unequip_artefact_effect(item, &mesg, meld, slot, false);

    // Must occur after ring is removed. -- bwr
    calc_mp();
}

bool unwield_item(bool showMsgs)
{
    if (!you.weapon())
        return false;

    item_def& item = *you.weapon();

    const bool is_weapon = get_item_slot(item) == EQ_WEAPON;

    if (is_weapon && !safe_to_remove(item))
        return false;

    unequip_item(EQ_WEAPON, showMsgs);

    you.wield_change     = true;
    quiver::set_needs_redraw();

    return true;
}

static void _mark_unseen_monsters()
{

    for (monster_iterator mi; mi; ++mi)
    {
        if (testbits((*mi)->flags, MF_WAS_IN_VIEW) && !you.can_see(**mi))
        {
            (*mi)->went_unseen_this_turn = true;
            (*mi)->unseen_pos = (*mi)->pos();
        }

    }
}

// This brand is supposed to be dangerous because it does large
// bonus damage, as well as banishment and other side effects,
// and you can rely on the occasional spatial bonus to mow down
// some opponents. It's far too powerful without a real risk.
// -- bwr [ed: ebering]
void unwield_distortion(bool brand)
{
    if (have_passive(passive_t::safe_distortion))
    {
        simple_god_message(make_stringf(" absorbs the residual spatial "
                           "distortion as you %s your "
                           "weapon.", brand ? "rebrand" : "unwield").c_str());
        return;
    }
    // Makes no sense to discourage unwielding a temporarily
    // branded weapon since you can wait it out. This also
    // fixes problems with unwield prompts (mantis #793).
    if (coinflip())
        you_teleport_now(false, true, "Space warps around you!");
    else if (coinflip())
    {
        you.banish(nullptr,
                   make_stringf("%sing a weapon of distortion",
                                brand ? "rebrand" : "unwield").c_str(),
                   you.get_experience_level(), true);
    }
    else
    {
        mpr("Space warps into you!");
        contaminate_player(random2avg(18000, 3), true);
    }
}
