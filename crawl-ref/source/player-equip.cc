#include "AppHdr.h"

#include "player-equip.h"

#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "delay.h"
#include "english.h" // conjugate_verb
#include "evoke.h"
#include "food.h"
#include "god-abil.h"
#include "god-item.h"
#include "god-passive.h"
#include "hints.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h" // command_to_string
#include "message.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "player-stats.h"
#include "religion.h"
#include "shopping.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "spl-wpnench.h"
#include "xom.h"

static void _mark_unseen_monsters();

/**
 * Recalculate the player's max hp and set the current hp based on the %change
 * of max hp. This has resulted from our having equipped an artefact that
 * changes max hp.
 */
static void _calc_hp_artefact()
{
    recalc_and_scale_hp();
    if (you.hp_max <= 0) // Borgnjor's abusers...
        ouch(0, KILLED_BY_DRAINING);
}

// Fill an empty equipment slot.
void equip_item(equipment_type slot, int item_slot, bool msg)
{
    ASSERT_RANGE(slot, EQ_FIRST_EQUIP, NUM_EQUIP);
    ASSERT(you.equip[slot] == -1);
    ASSERT(!you.melded[slot]);

    you.equip[slot] = item_slot;

    equip_effect(slot, item_slot, false, msg);
    ash_check_bondage();
    if (you.equip[slot] != -1 && you.inv[you.equip[slot]].cursed())
        auto_id_inventory();
}

// Clear an equipment slot (possibly melded).
bool unequip_item(equipment_type slot, bool msg)
{
    ASSERT_RANGE(slot, EQ_FIRST_EQUIP, NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    const int item_slot = you.equip[slot];
    if (item_slot == -1)
        return false;
    else
    {
        you.equip[slot] = -1;

        if (!you.melded[slot])
            unequip_effect(slot, item_slot, false, msg);
        else
            you.melded.set(slot, false);
        ash_check_bondage();
        return true;
    }
}

// Meld a slot (if equipped).
// Does not handle unequip effects, since melding should be simultaneous (so
// you should call all unequip effects after all melding is done)
bool meld_slot(equipment_type slot, bool msg)
{
    ASSERT_RANGE(slot, EQ_FIRST_EQUIP, NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && !you.melded[slot])
    {
        you.melded.set(slot);
        return true;
    }
    return false;
}

// Does not handle equip effects, since unmelding should be simultaneous (so
// you should call all equip effects after all unmelding is done)
bool unmeld_slot(equipment_type slot, bool msg)
{
    ASSERT_RANGE(slot, EQ_FIRST_EQUIP, NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && you.melded[slot])
    {
        you.melded.set(slot, false);
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

static void _assert_valid_slot(equipment_type eq, equipment_type slot)
{
#ifdef ASSERTS
    if (eq == slot)
        return;
    ASSERT(eq == EQ_RINGS); // all other slots are unique
    equipment_type r1 = EQ_LEFT_RING, r2 = EQ_RIGHT_RING;
    if (you.species == SP_OCTOPODE)
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

    if (slot == EQ_WEAPON)
        _unequip_weapon_effect(item, msg, meld);
    else if (slot >= EQ_CLOAK && slot <= EQ_BODY_ARMOUR)
        _unequip_armour_effect(item, meld, slot);
    else if (slot >= EQ_FIRST_JEWELLERY && slot <= EQ_LAST_JEWELLERY)
        _unequip_jewellery_effect(item, msg, meld, slot);
}

///////////////////////////////////////////////////////////
// Actual equip and unequip effect implementation below
//

static void _equip_artefact_effect(item_def &item, bool *show_msgs, bool unmeld,
                                   equipment_type slot)
{
#define unknown_proprt(prop) (proprt[(prop)] && !known[(prop)])

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

    const bool alreadyknown = item_type_known(item);
    const bool dangerous    = player_in_a_dangerous_place();
    const bool msg          = !show_msgs || *show_msgs;

    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_properties(item, proprt, known);

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

    // Modify ability scores.
    notify_stat_change(STAT_STR, proprt[ARTP_STRENGTH],
                       !(msg && unknown_proprt(ARTP_STRENGTH)));
    notify_stat_change(STAT_INT, proprt[ARTP_INTELLIGENCE],
                       !(msg && unknown_proprt(ARTP_INTELLIGENCE)));
    notify_stat_change(STAT_DEX, proprt[ARTP_DEXTERITY],
                       !(msg && unknown_proprt(ARTP_DEXTERITY)));

    if (unknown_proprt(ARTP_CONTAM) && msg)
        mpr("You feel a build-up of mutagenic energy.");

    if (!unmeld && !item.cursed() && proprt[ARTP_CURSE])
        do_curse_item(item, !msg);

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you use an unknown random artefact and
        // there is a dangerous monster nearby...
        xom_is_stimulated(100);
    }

    if (proprt[ARTP_HP])
        _calc_hp_artefact();

    // Let's try this here instead of up there.
    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();

    if (!fully_identified(item))
    {
        set_ident_type(item, true);
        set_ident_flags(item, ISFLAG_IDENT_MASK);
    }
#undef unknown_proprt
}

static void _unequip_artefact_effect(item_def &item,
                                     bool *show_msgs, bool meld,
                                     equipment_type slot)
{
    ASSERT(is_artefact(item));

    artefact_properties_t proprt;
    artefact_known_props_t known;
    artefact_properties(item, proprt, known);
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

    if (proprt[ARTP_FLY] != 0 && you.cancellable_flight()
        && !you.evokable_flight())
    {
        you.duration[DUR_FLIGHT] = 0;
        land_player();
    }

    if (proprt[ARTP_INVISIBLE] != 0
        && you.duration[DUR_INVIS] > 1
        && !you.attribute[ATTR_INVIS_UNCANCELLABLE]
        && !you.evokable_invis())
    {
        you.duration[DUR_INVIS] = 1;
    }

    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();

    if (proprt[ARTP_CONTAM] && !meld)
    {
        mpr("Mutagenic energies flood into your body!");
        contaminate_player(7000, true);
    }

    if (proprt[ARTP_DRAIN] && !meld)
        drain_player(150, true, true);

    if (proprt[ARTP_SEE_INVISIBLE])
        _mark_unseen_monsters();

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.unrand_idx);

        if (entry->unequip_func)
            entry->unequip_func(&item, show_msgs);

        if (entry->world_reacts_func)
            you.unrand_reacts.set(slot, false);
    }

    // this must be last!
    if (proprt[ARTP_FRAGILE] && !meld)
    {
        mprf("%s crumbles to dust!", item.name(DESC_THE).c_str());
        dec_inv_item_quantity(item.link, 1);
    }
}

static void _equip_use_warning(const item_def& item)
{
    if (is_holy_item(item) && you_worship(GOD_YREDELEMNUL))
        mpr("You really shouldn't be using a holy item like this.");
    else if (is_corpse_violating_item(item) && you_worship(GOD_FEDHAS))
        mpr("You really shouldn't be using a corpse-violating item like this.");
    else if (is_evil_item(item) && is_good_god(you.religion))
        mpr("You really shouldn't be using an evil item like this.");
    else if (is_unclean_item(item) && you_worship(GOD_ZIN))
        mpr("You really shouldn't be using an unclean item like this.");
    else if (is_chaotic_item(item) && you_worship(GOD_ZIN))
        mpr("You really shouldn't be using a chaotic item like this.");
    else if (is_hasty_item(item) && you_worship(GOD_CHEIBRIADOS))
        mpr("You really shouldn't be using a hasty item like this.");
    else if (is_fiery_item(item) && you_worship(GOD_DITHMENOS))
        mpr("You really shouldn't be using a fiery item like this.");
    else if (is_channeling_item(item) && you_worship(GOD_PAKELLAS))
        mpr("You really shouldn't be trying to channel magic like this.");
}

static void _wield_cursed(item_def& item, bool known_cursed, bool unmeld)
{
    if (!item.cursed() || unmeld)
        return;
    mprf("It sticks to your %s!", you.hand_name(false).c_str());
    int amusement = 16;
    if (!known_cursed)
    {
        amusement *= 2;
        if (origin_as_god_gift(item) == GOD_XOM)
            amusement *= 2;
    }
    const int wpn_skill = item_attack_skill(item.base_type, item.sub_type);
    if (wpn_skill != SK_FIGHTING && you.skills[wpn_skill] == 0)
        amusement *= 2;

    xom_is_stimulated(amusement);
}

// Provide a function for handling initial wielding of 'special'
// weapons, or those whose function is annoying to reproduce in
// other places *cough* auto-butchering *cough*.    {gdl}
static void _equip_weapon_effect(item_def& item, bool showMsgs, bool unmeld)
{
    int special = 0;

    const bool artefact     = is_artefact(item);
    const bool known_cursed = item_known_cursed(item);

    // And here we finally get to the special effects of wielding. {dlb}
    switch (item.base_type)
    {
    case OBJ_STAVES:
    {
        set_ident_flags(item, ISFLAG_IDENT_MASK);
        set_ident_type(OBJ_STAVES, item.sub_type, true);

        if (item.sub_type == STAFF_POWER)
        {
            canned_msg(MSG_MANA_INCREASE);
            calc_mp();
        }

        _wield_cursed(item, known_cursed, unmeld);
        break;
    }

    case OBJ_WEAPONS:
    {
        // Note that if the unrand equip prints a message, it will
        // generally set showMsgs to false.
        if (artefact)
            _equip_artefact_effect(item, &showMsgs, unmeld, EQ_WEAPON);

        const bool was_known      = item_type_known(item);
              bool known_recurser = false;

        set_ident_flags(item, ISFLAG_IDENT_MASK);

        special = item.brand;

        if (artefact)
        {
            special = artefact_property(item, ARTP_BRAND);

            if (!was_known && !(item.flags & ISFLAG_NOTED_ID))
            {
                item.flags |= ISFLAG_NOTED_ID;

                // Make a note of it.
                take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_A),
                               origin_desc(item)));
            }
            else
                known_recurser = artefact_known_property(item, ARTP_CURSE);
        }

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
                    if (you.species == SP_VAMPIRE)
                        mpr("You feel a bloodthirsty glee!");
                    else if (you.undead_state() == US_ALIVE && !you_foodless())
                        mpr("You feel a dreadful hunger.");
                    else
                        mpr("You feel an empty sense of dread.");
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

                default:
                    break;
                }
            }

            // effect second
            switch (special)
            {
            case SPWPN_VAMPIRISM:
                if (you.species != SP_VAMPIRE
                    && you.undead_state() == US_ALIVE
                    && !you_foodless()
                    && !unmeld)
                {
                    make_hungry(4500, false, false);
                }
                break;

            case SPWPN_DISTORTION:
                if (!was_known)
                {
                    // Xom loves it when you ID a distortion weapon this way,
                    // and even more so if he gifted the weapon himself.
                    if (origin_as_god_gift(item) == GOD_XOM)
                        xom_is_stimulated(200);
                    else
                        xom_is_stimulated(100);
                }
                break;

            case SPWPN_ANTIMAGIC:
                calc_mp();
                break;

            default:
                break;
            }
        }

        _wield_cursed(item, known_cursed || known_recurser, unmeld);
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
    you.m_quiver.on_weapon_changed();

    // Fragile artefacts may be destroyed, so make a copy
    item_def item = real_item;

    // Call this first, so that the unrandart func can set showMsgs to
    // false if it does its own message handling.
    if (is_artefact(item))
        _unequip_artefact_effect(real_item, &showMsgs, meld, EQ_WEAPON);

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
                    if (you.species == SP_VAMPIRE)
                        mpr("You feel your glee subside.");
                    else
                        mpr("You feel the dreadful sensation subside.");
                }
                break;

            case SPWPN_DISTORTION:
                // Removing the translocations skill reduction of effect,
                // it might seem sensible, but this brand is supposed
                // to be dangerous because it does large bonus damage,
                // as well as free teleport other side effects, and
                // even with the miscast effects you can rely on the
                // occasional spatial bonus to mow down some opponents.
                // It's far too powerful without a real risk, especially
                // if it's to be allowed as a player spell. -- bwr

                // int effect = 9 -
                //        random2avg(you.skills[SK_TRANSLOCATIONS] * 2, 2);

                if (!meld)
                {
                    if (have_passive(passive_t::safe_distortion))
                    {
                        simple_god_message(" absorbs the residual spatial "
                                           "distortion as you unwield your "
                                           "weapon.");
                        break;
                    }
                    // Makes no sense to discourage unwielding a temporarily
                    // branded weapon since you can wait it out. This also
                    // fixes problems with unwield prompts (mantis #793).
                    MiscastEffect(&you, nullptr, WIELD_MISCAST,
                                  SPTYP_TRANSLOCATION, 9, 90,
                                  "a distortion unwield");
                }
                break;

            case SPWPN_ANTIMAGIC:
                calc_mp();
                mpr("You feel magic returning to you.");
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
    else if (item.is_type(OBJ_STAVES, STAFF_POWER))
    {
        calc_mp();
        canned_msg(MSG_MANA_DECREASE);
    }

    // Unwielding dismisses an active spectral weapon
    monster *spectral_weapon = find_spectral_weapon(&you);
    if (spectral_weapon)
    {
        mprf("Your spectral weapon disappears as %s.",
             meld ? "your weapon melds" : "you unwield");
        end_spectral_weapon(spectral_weapon, false, true);
    }
}

static void _spirit_shield_message(bool unmeld)
{
    if (!unmeld && you.spirit_shield() < 2)
    {
        dec_mp(you.magic_points);
        mpr("You feel your power drawn to a protective spirit.");
        if (you.species == SP_DEEP_DWARF
            && !(have_passive(passive_t::no_mp_regen)
                 || player_under_penance(GOD_PAKELLAS)))
        {
            mpr("Now linked to your health, your magic stops regenerating.");
        }
    }
    else if (!unmeld && player_mutation_level(MUT_MANA_SHIELD))
        mpr("You feel the presence of a powerless spirit.");
    else // unmeld or already spirit-shielded
        mpr("You feel spirits watching over you.");
}

static void _equip_armour_effect(item_def& arm, bool unmeld,
                                 equipment_type slot)
{
    const bool known_cursed = item_known_cursed(arm);
    int ego = get_armour_ego_type(arm);
    if (ego != SPARM_NORMAL)
    {
        switch (ego)
        {
        case SPARM_RUNNING:
            if (!you.fishtail)
                mpr("You feel quick.");
            break;

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
            // If you weren't flying when you took off the boots, don't restart.
            if (you.attribute[ATTR_LAST_FLIGHT_STATUS]
                || player_mutation_level(MUT_NO_ARTIFICE))
            {
                if (you.airborne())
                {
                    you.attribute[ATTR_PERM_FLIGHT] = 1;
                    mpr("You feel rather light.");
                }
                else
                {
                    you.attribute[ATTR_PERM_FLIGHT] = 1;
                    float_player();
                }
            }
            if (!unmeld && !player_mutation_level(MUT_NO_ARTIFICE))
            {
                if (player_mutation_level(MUT_NO_ARTIFICE))
                    mpr("Take it off to stop flying.");
                else
                {
                mprf("(use the <w>%s</w>bility menu to %s flying)",
                     command_to_string(CMD_USE_ABILITY).c_str(),
                     you.attribute[ATTR_LAST_FLIGHT_STATUS]
                         ? "stop or start" : "start or stop");
                }
            }

            break;

        case SPARM_MAGIC_RESISTANCE:
            mpr("You feel resistant to hostile enchantments.");
            break;

        case SPARM_PROTECTION:
            mpr("You feel protected.");
            break;

        case SPARM_STEALTH:
            if (!player_mutation_level(MUT_NO_STEALTH))
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

        case SPARM_REPULSION:
            mpr("You are surrounded by a repulsion field.");
            break;
        }
    }

    if (is_artefact(arm))
    {
        bool show_msgs = true;
        _equip_artefact_effect(arm, &show_msgs, unmeld, slot);
    }

    if (arm.cursed() && !unmeld)
    {
        mpr("Oops, that feels deathly cold.");
        learned_something_new(HINT_YOU_CURSED);

        if (!known_cursed)
        {
            int amusement = 64;

            if (origin_as_god_gift(arm) == GOD_XOM)
                amusement *= 2;

            xom_is_stimulated(amusement);
        }
    }

    you.redraw_armour_class = true;
    you.redraw_evasion = true;
}

/**
 * The player lost a source of permafly. End their flight if there was
 * no other source, evoking a ring of flight "for free" if possible.
 */
void lose_permafly_source()
{
    const bool had_perm_flight = you.attribute[ATTR_PERM_FLIGHT];

    if (had_perm_flight
        && !you.wearing_ego(EQ_ALL_ARMOUR, SPARM_FLYING)
        && !you.racial_permanent_flight())
    {
        you.attribute[ATTR_PERM_FLIGHT] = 0;
        if (you.evokable_flight())
        {
            fly_player(
                player_adjust_evoc_power(you.skill(SK_EVOCATIONS, 2) + 30),
                true);
        }
    }

    // since a permflight item can keep tempflight evocations going
    // we should check tempflight here too
    if (you.cancellable_flight() && !you.evokable_flight())
        you.duration[DUR_FLIGHT] = 0;

    if (had_perm_flight)
        land_player(); // land_player() has a check for airborne()
}

static void _unequip_armour_effect(item_def& item, bool meld,
                                   equipment_type slot)
{
    you.redraw_armour_class = true;
    you.redraw_evasion = true;

    switch (get_armour_ego_type(item))
    {
    case SPARM_RUNNING:
        if (!you.fishtail)
            mpr("You feel rather sluggish.");
        break;

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

    case SPARM_INVISIBILITY:
        if (you.duration[DUR_INVIS]
            && !you.attribute[ATTR_INVIS_UNCANCELLABLE]
            && !you.evokable_invis())
        {
            you.duration[DUR_INVIS] = 1;
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
        mpr("That put a bit of spring back into your step.");
        break;

    case SPARM_FLYING:
        // Save current flight status so we can restore it on reequip
        you.attribute[ATTR_LAST_FLIGHT_STATUS] =
            you.attribute[ATTR_PERM_FLIGHT];

        lose_permafly_source();
        break;

    case SPARM_MAGIC_RESISTANCE:
        mpr("You feel less resistant to hostile enchantments.");
        break;

    case SPARM_PROTECTION:
        mpr("You feel less protected.");
        break;

    case SPARM_STEALTH:
        if (!player_mutation_level(MUT_NO_STEALTH))
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

    case SPARM_REPULSION:
        mpr("The haze of the repulsion field disappears.");
        break;

    default:
        break;
    }

    if (is_artefact(item))
        _unequip_artefact_effect(item, nullptr, meld, slot);
}

static void _remove_amulet_of_faith(item_def &item)
{
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

static void _remove_amulet_of_harm()
{
    if (you.undead_state() == US_ALIVE)
        mpr("The amulet drains your life force as you remove it!");
    else
        mpr("The amulet drains your animating force as you remove it!");

    drain_player(150, false, true);
}

static void _equip_amulet_of_regeneration()
{
    if (player_mutation_level(MUT_SLOW_REGENERATION) == 3)
        mpr("The amulet feels cold and inert.");
    else if (you.hp == you.hp_max)
    {
        you.props[REGEN_AMULET_ACTIVE] = 1;
        mpr("The amulet throbs as it attunes itself to your uninjured body.");
    }
    else
    {
        mpr("You sense that the amulet cannot attune itself to your injured"
            " body.");
        you.props[REGEN_AMULET_ACTIVE] = 0;
    }
}

static void _equip_amulet_of_mana_regeneration()
{
    if (!player_regenerates_mp())
        mpr("The amulet feels cold and inert.");
    else if (you.magic_points == you.max_magic_points)
    {
        you.props[MANA_REGEN_AMULET_ACTIVE] = 1;
        mpr("The amulet hums as it attunes itself to your energized body.");
    }
    else
    {
        mpr("You sense that the amulet cannot attune itself to your exhausted"
            " body.");
        you.props[MANA_REGEN_AMULET_ACTIVE] = 0;
    }
}

static void _equip_jewellery_effect(item_def &item, bool unmeld,
                                    equipment_type slot)
{
    const bool artefact     = is_artefact(item);
    const bool known_cursed = item_known_cursed(item);
    const bool known_bad    = (item_type_known(item)
                               && item_value(item) <= 2);

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

    case RING_PROTECTION:
    case AMU_REFLECTION:
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
        canned_msg(MSG_MANA_INCREASE);
        calc_mp();
        break;

    case RING_TELEPORTATION:
        if (you.no_tele())
            mpr("You feel a slight, muted jump rush through you.");
        else
            // keep in sync with player_teleport
            mprf("You feel slightly %sjumpy.",
                 (player_teleport(false) > 8) ? "more " : "");
        break;

    case AMU_FAITH:
        if (you.species == SP_DEMIGOD)
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

    case AMU_THE_GOURMAND:
        // What's this supposed to achieve? (jpeg)
        you.duration[DUR_GOURMAND] = 0;
        mpr("You feel a craving for the dungeon's cuisine.");
        break;

    case AMU_REGENERATION:
        if (!unmeld)
            _equip_amulet_of_regeneration();
        break;

    case AMU_MANA_REGENERATION:
        if (!unmeld)
            _equip_amulet_of_mana_regeneration();
        break;

    case AMU_GUARDIAN_SPIRIT:
        _spirit_shield_message(unmeld);
        break;
    }

    bool new_ident = false;
    // Artefacts have completely different appearance than base types
    // so we don't allow them to make the base types known.
    if (artefact)
    {
        bool show_msgs = true;
        _equip_artefact_effect(item, &show_msgs, unmeld, slot);

        set_ident_flags(item, ISFLAG_KNOW_PROPERTIES);
    }
    else
    {
        new_ident = set_ident_type(item, true);
        set_ident_flags(item, ISFLAG_IDENT_MASK);
    }

    if (item.cursed() && !unmeld)
    {
        mprf("Oops, that %s feels deathly cold.",
             jewellery_is_amulet(item)? "amulet" : "ring");
        learned_something_new(HINT_YOU_CURSED);

        int amusement = 32;
        if (!known_cursed && !known_bad)
        {
            amusement *= 2;

            if (origin_as_god_gift(item) == GOD_XOM)
                amusement *= 2;
        }
        xom_is_stimulated(amusement);
    }

    // Cursed or not, we know that since we've put the ring on.
    set_ident_flags(item, ISFLAG_KNOW_CURSE);

    if (!unmeld)
        mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
    if (new_ident)
        auto_assign_item_slot(item);
}

static void _unequip_jewellery_effect(item_def &item, bool mesg, bool meld,
                                      equipment_type slot)
{
    // The ring/amulet must already be removed from you.equip at this point.
    switch (item.sub_type)
    {
    case RING_FIRE:
    case RING_LOUDNESS:
    case RING_ICE:
    case RING_LIFE_PROTECTION:
    case RING_POISON_RESISTANCE:
    case RING_PROTECTION_FROM_COLD:
    case RING_PROTECTION_FROM_FIRE:
    case RING_PROTECTION_FROM_MAGIC:
    case RING_SLAYING:
    case RING_STEALTH:
    case RING_TELEPORTATION:
    case RING_WIZARDRY:
    case AMU_REGENERATION:
        break;

    case RING_SEE_INVISIBLE:
        _mark_unseen_monsters();
        break;

    case RING_PROTECTION:
    case AMU_REFLECTION:
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
        if (you.cancellable_flight() && !you.evokable_flight())
        {
            you.duration[DUR_FLIGHT] = 0;
            land_player();
        }
        break;

    case RING_MAGICAL_POWER:
        canned_msg(MSG_MANA_DECREASE);
        break;

    case AMU_THE_GOURMAND:
        you.duration[DUR_GOURMAND] = 0;
        break;

    case AMU_FAITH:
        if (!meld)
            _remove_amulet_of_faith(item);
        break;

    case AMU_HARM:
        if (!meld)
            _remove_amulet_of_harm();
        break;

    case AMU_GUARDIAN_SPIRIT:
        if (you.species == SP_DEEP_DWARF && player_regenerates_mp())
            mpr("Your magic begins regenerating once more.");
        break;
    }

    if (is_artefact(item))
        _unequip_artefact_effect(item, &mesg, meld, slot);

    // Must occur after ring is removed. -- bwr
    calc_mp();
}

bool unwield_item(bool showMsgs)
{
    if (!you.weapon())
        return false;

    if (you.berserk())
    {
        if (showMsgs)
            canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    item_def& item = *you.weapon();

    const bool is_weapon = get_item_slot(item) == EQ_WEAPON;

    if (is_weapon && !safe_to_remove(item))
        return false;

    unequip_item(EQ_WEAPON, showMsgs);

    you.wield_change     = true;
    you.redraw_quiver    = true;

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
