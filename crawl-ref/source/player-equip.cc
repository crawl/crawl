#include "AppHdr.h"

#include "player-equip.h"

#include "areas.h"
#include "artefact.h"
#include "delay.h"
#include "describe.h"
#include "food.h"
#include "goditem.h"
#include "godpassive.h"
#include "hints.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "misc.h"
#include "notes.h"
#include "options.h"
#include "player.h"
#include "player-stats.h"
#include "religion.h"
#include "shopping.h"
#include "skills2.h"
#include "spl-cast.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "state.h"
#include "stuff.h"
#include "transform.h"
#include "xom.h"

#include <cmath>

static void _equip_effect(equipment_type slot, int item_slot, bool unmeld,
                          bool msg);
static void _unequip_effect(equipment_type slot, int item_slot, bool meld,
                            bool msg);

void calc_hp_artefact()
{
    // Rounding must be down or Deep Dwarves would abuse certain values.
    // We can reduce errors by a factor of 100 by using partial hp we have.
    int old_max = you.hp_max;
    int hp = you.hp * 100 + you.hit_points_regeneration;
    calc_hp();
    int new_max = you.hp_max;
    hp = hp * new_max / old_max;
    if (hp < 100)
        hp = 100;
    you.hp = min(hp / 100, you.hp_max);
    you.hit_points_regeneration = hp % 100;
    if (you.hp_max <= 0) // Borgnjor's abusers...
        ouch(0, NON_MONSTER, KILLED_BY_DRAINING);
}

// Fill an empty equipment slot.
void equip_item(equipment_type slot, int item_slot, bool msg)
{
    ASSERT_RANGE(slot, EQ_NONE + 1, NUM_EQUIP);
    ASSERT(you.equip[slot] == -1);
    ASSERT(!you.melded[slot]);

    // Maybe this prevent a carried item from allowing training.
    maybe_change_train(you.inv[item_slot], false);
    you.equip[slot] = item_slot;
    item_skills(you.inv[item_slot], you.start_train);

    _equip_effect(slot, item_slot, false, msg);
    ash_check_bondage();
    if (you.equip[slot] != -1 && you.inv[you.equip[slot]].cursed())
        auto_id_inventory();
}

// Clear an equipment slot (possibly melded).
bool unequip_item(equipment_type slot, bool msg)
{
    ASSERT_RANGE(slot, EQ_NONE + 1, NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    const int item_slot = you.equip[slot];
    if (item_slot == -1)
        return false;
    else
    {
        item_skills(you.inv[item_slot], you.stop_train);
        you.equip[slot] = -1;
        // Maybe this allows training for a carried item.
        maybe_change_train(you.inv[item_slot], true);

        if (!you.melded[slot])
            _unequip_effect(slot, item_slot, false, msg);
        else
            you.melded.set(slot, false);
        ash_check_bondage();
        return true;
    }
}

// Meld a slot (if equipped).
bool meld_slot(equipment_type slot, bool msg)
{
    ASSERT_RANGE(slot, EQ_NONE + 1, NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && !you.melded[slot])
    {
        you.melded.set(slot);
        _unequip_effect(slot, you.equip[slot], true, msg);
        return true;
    }
    return false;
}

bool unmeld_slot(equipment_type slot, bool msg)
{
    ASSERT_RANGE(slot, EQ_NONE + 1, NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && you.melded[slot])
    {
        you.melded.set(slot, false);
        _equip_effect(slot, you.equip[slot], true, msg);
        return true;
    }
    return false;
}

static void _equip_weapon_effect(item_def& item, bool showMsgs, bool unmeld);
static void _unequip_weapon_effect(item_def& item, bool showMsgs, bool meld);
static void _equip_armour_effect(item_def& arm, bool unmeld);
static void _unequip_armour_effect(item_def& item, bool meld);
static void _equip_jewellery_effect(item_def &item, bool unmeld);
static void _unequip_jewellery_effect(item_def &item, bool mesg, bool meld);
static void _equip_use_warning(const item_def& item);

static void _equip_effect(equipment_type slot, int item_slot, bool unmeld,
                          bool msg)
{
    item_def& item = you.inv[item_slot];
    equipment_type eq = get_item_slot(item);

    if (slot == EQ_WEAPON && eq != EQ_WEAPON)
        return;

    ASSERT(slot == eq
           || eq == EQ_RINGS && (slot == EQ_LEFT_RING || slot == EQ_RIGHT_RING)
           || eq == EQ_RINGS && you.species == SP_OCTOPODE);

    if (msg)
        _equip_use_warning(item);

    if (slot == EQ_WEAPON)
        _equip_weapon_effect(item, msg, unmeld);
    else if (slot >= EQ_CLOAK && slot <= EQ_BODY_ARMOUR)
        _equip_armour_effect(item, unmeld);
    else if (slot >= EQ_LEFT_RING && slot < NUM_EQUIP)
        _equip_jewellery_effect(item, unmeld);
}

static void _unequip_effect(equipment_type slot, int item_slot, bool meld,
                            bool msg)
{
    item_def& item = you.inv[item_slot];
    equipment_type eq = get_item_slot(item);

    if (slot == EQ_WEAPON && eq != EQ_WEAPON)
        return;

    ASSERT(slot == eq
           || eq == EQ_RINGS && (slot == EQ_LEFT_RING || slot == EQ_RIGHT_RING)
           || eq == EQ_RINGS && you.species == SP_OCTOPODE);

    if (slot == EQ_WEAPON)
        _unequip_weapon_effect(item, msg, meld);
    else if (slot >= EQ_CLOAK && slot <= EQ_BODY_ARMOUR)
        _unequip_armour_effect(item, meld);
    else if (slot >= EQ_LEFT_RING && slot < NUM_EQUIP)
        _unequip_jewellery_effect(item, msg, meld);

    if (slot == EQ_SHIELD && !meld)
        you.stop_train.insert(SK_SHIELDS);
}

///////////////////////////////////////////////////////////
// Actual equip and unequip effect implementation below
//

static void _equip_artefact_effect(item_def &item, bool *show_msgs, bool unmeld)
{
#define unknown_proprt(prop) (proprt[(prop)] && !known[(prop)])

    ASSERT(is_artefact(item));

    // Call unrandart equip function first, so that it can modify the
    // artefact's properties before they're applied.
    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.special);

        if (entry->equip_func)
            entry->equip_func(&item, show_msgs, unmeld);

        if (entry->world_reacts_func)
        {
            equipment_type eq = get_item_slot(item.base_type, item.sub_type);
            you.unrand_reacts |= (1 << eq);
        }
    }

    const bool alreadyknown = item_type_known(item);
    const bool dangerous    = player_in_a_dangerous_place();
    const bool msg          = !show_msgs || *show_msgs;

    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_wpn_properties(item, proprt, known);

    // Only give property messages for previously unknown properties.
    if (proprt[ARTP_AC])
    {
        you.redraw_armour_class = true;
        if (!known[ARTP_AC])
        {
            if (msg)
            {
                mprf("You feel %s.", proprt[ARTP_AC] > 0?
                     "well-protected" : "more vulnerable");
            }
            artefact_wpn_learn_prop(item, ARTP_AC);
        }
    }

    if (proprt[ARTP_EVASION])
    {
        you.redraw_evasion = true;
        if (!known[ARTP_EVASION])
        {
            if (msg)
            {
                mprf("You feel somewhat %s.", proprt[ARTP_EVASION] > 0?
                     "nimbler" : "more awkward");
            }
            artefact_wpn_learn_prop(item, ARTP_EVASION);
        }
    }

    if (proprt[ARTP_EYESIGHT])
        autotoggle_autopickup(false);

    if (proprt[ARTP_MAGICAL_POWER] && !known[ARTP_MAGICAL_POWER])
    {
        if (msg)
        {
            canned_msg(proprt[ARTP_MAGICAL_POWER] > 0 ? MSG_MANA_INCREASE
                                                      : MSG_MANA_DECREASE);
        }
        artefact_wpn_learn_prop(item, ARTP_MAGICAL_POWER);
    }

    // Modify ability scores.
    // Output result even when identified (because of potential fatality).
    notify_stat_change(STAT_STR,     proprt[ARTP_STRENGTH], !msg, item);
    notify_stat_change(STAT_INT, proprt[ARTP_INTELLIGENCE], !msg, item);
    notify_stat_change(STAT_DEX,    proprt[ARTP_DEXTERITY], !msg, item);

    const artefact_prop_type stat_props[3] =
        {ARTP_STRENGTH, ARTP_INTELLIGENCE, ARTP_DEXTERITY};

    for (int i = 0; i < 3; i++)
        if (unknown_proprt(stat_props[i]))
            artefact_wpn_learn_prop(item, stat_props[i]);

    // For evokable stuff, check whether other equipped items yield
    // the same ability.  If not, and if the ability granted hasn't
    // already been discovered, give a message.
    if (unknown_proprt(ARTP_FLY)
        && !items_give_ability(item.link, ARTP_FLY))
    {
        if (msg)
        {
            if (you.airborne())
                mpr("You feel vaguely more buoyant than before.");
            else
                mpr("You feel buoyant.");
        }
        artefact_wpn_learn_prop(item, ARTP_FLY);
    }

    if (unknown_proprt(ARTP_INVISIBLE) && !you.duration[DUR_INVIS])
    {
        if (msg)
            mpr("You become transparent for a moment.");
        artefact_wpn_learn_prop(item, ARTP_INVISIBLE);
    }

    if (unknown_proprt(ARTP_BERSERK)
        && !items_give_ability(item.link, ARTP_BERSERK))
    {
        if (msg)
            mpr("You feel a brief urge to hack something to bits.");
        artefact_wpn_learn_prop(item, ARTP_BERSERK);
    }

    if (unknown_proprt(ARTP_BLINK)
        && !items_give_ability(item.link, ARTP_BLINK))
    {
        if (msg)
            mpr("You feel jittery for a moment.");
        artefact_wpn_learn_prop(item, ARTP_BLINK);
    }

    if (unknown_proprt(ARTP_MUTAGENIC))
    {
        if (msg)
            mpr("You feel a build-up of mutagenic energy.");
        artefact_wpn_learn_prop(item, ARTP_MUTAGENIC);
    }
    if (unknown_proprt(ARTP_JUMP)
        && !items_give_ability(item.link, ARTP_JUMP))
    {
        if (msg)
        {
            // Give message even if evoke jump isn't the active jump source
            if (player_evoke_jump_range() >
                player_mutation_level(MUT_JUMP) + 2)
                mpr("You feel more sure on your feet.");
            else
                mpr("Your feet feel light for a moment.");
        }
        artefact_wpn_learn_prop(item, ARTP_JUMP);
    }

    if (!unmeld && !item.cursed() && proprt[ARTP_CURSED] > 0
         && one_chance_in(proprt[ARTP_CURSED]))
    {
        do_curse_item(item, !msg);
        artefact_wpn_learn_prop(item, ARTP_CURSED);
    }

    if (proprt[ARTP_NOISES])
        you.attribute[ATTR_NOISES] = 1;

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you use an unknown random artefact and
        // there is a dangerous monster nearby...
        xom_is_stimulated(100);
    }

    if (proprt[ARTP_HP])
        calc_hp_artefact();

    // Let's try this here instead of up there.
    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();
#undef unknown_proprt
}

static void _unequip_artefact_effect(item_def &item,
                                     bool *show_msgs, bool meld)
{
    ASSERT(is_artefact(item));

    artefact_properties_t proprt;
    artefact_known_props_t known;
    artefact_wpn_properties(item, proprt, known);
    const bool msg = !show_msgs || *show_msgs;

    if (proprt[ARTP_AC])
    {
        you.redraw_armour_class = true;
        if (!known[ARTP_AC] && msg)
        {
            mprf("You feel less %s.",
                 proprt[ARTP_AC] > 0? "well-protected" : "vulnerable");
        }
    }

    if (proprt[ARTP_EVASION])
    {
        you.redraw_evasion = true;
        if (!known[ARTP_EVASION] && msg)
        {
            mprf("You feel less %s.",
                 proprt[ARTP_EVASION] > 0? "nimble" : "awkward");
        }
    }

    if (proprt[ARTP_HP])
        calc_hp_artefact();

    if (proprt[ARTP_MAGICAL_POWER] && !known[ARTP_MAGICAL_POWER] && msg)
    {
        canned_msg(proprt[ARTP_MAGICAL_POWER] > 0 ? MSG_MANA_DECREASE
                                                  : MSG_MANA_INCREASE);
    }

    // Modify ability scores; always output messages.
    notify_stat_change(STAT_STR, -proprt[ARTP_STRENGTH],     !msg, item,
                       true);
    notify_stat_change(STAT_INT, -proprt[ARTP_INTELLIGENCE], !msg, item,
                       true);
    notify_stat_change(STAT_DEX, -proprt[ARTP_DEXTERITY],    !msg, item,
                       true);

    if (proprt[ARTP_NOISES] != 0)
        you.attribute[ATTR_NOISES] = 0;

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

    if (proprt[ARTP_MUTAGENIC] && !meld)
    {
        mpr("Mutagenic energies flood into your body!");
        contaminate_player(7, true);
    }

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.special);

        if (entry->unequip_func)
            entry->unequip_func(&item, show_msgs);

        if (entry->world_reacts_func)
        {
            equipment_type eq = get_item_slot(item.base_type, item.sub_type);
            you.unrand_reacts &= ~(1 << eq);
        }
    }
}

static void _equip_use_warning(const item_def& item)
{
    if (is_holy_item(item) && you_worship(GOD_YREDELEMNUL))
        mpr("You really shouldn't be using a holy item like this.");
    else if (is_unholy_item(item) && is_good_god(you.religion))
        mpr("You really shouldn't be using an unholy item like this.");
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
    else if (is_poisoned_item(item) && you_worship(GOD_SHINING_ONE))
        mpr("You really shouldn't be using a poisoned item like this.");
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
        god_type god;
        if (origin_is_god_gift(item, &god) && god == GOD_XOM)
            amusement *= 2;
    }
    const int wpn_skill = weapon_skill(item.base_type, item.sub_type);
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
    case OBJ_MISCELLANY:
    {
        if (item.sub_type == MISC_LANTERN_OF_SHADOWS)
        {
            if (showMsgs)
                mpr("The area is filled with flickering shadows.");

            you.attribute[ATTR_SHADOWS] = 1;
            update_vision_range();
        }
        break;
    }

    case OBJ_STAVES:
    {
        set_ident_flags(item, ISFLAG_IDENT_MASK);
        set_ident_type(OBJ_STAVES, item.sub_type, ID_KNOWN_TYPE);

        if (item.sub_type == STAFF_POWER)
        {
            int mp = item.special - you.elapsed_time / POWER_DECAY;

            if (mp > 0)
                if (you.species == SP_DJINNI)
                    you.hp += mp;
                else
                    you.magic_points += mp;

            if (get_real_mp(true) >= 50)
                mpr("You feel your magic capacity is already quite full.");
            else
                canned_msg(MSG_MANA_INCREASE);

            calc_mp();
        }

        _wield_cursed(item, known_cursed, unmeld);
        break;
    }

    case OBJ_RODS:
    {
        set_ident_flags(item, ISFLAG_IDENT_MASK);
        _wield_cursed(item, known_cursed, unmeld);
        break;
    }

    case OBJ_WEAPONS:
    {
        // Call unrandart equip func before item is identified.
        if (artefact)
            _equip_artefact_effect(item, &showMsgs, unmeld);

        const bool was_known      = item_type_known(item);
              bool known_recurser = false;

        set_ident_flags(item, ISFLAG_EQ_WEAPON_MASK);

        special = item.special;

        if (artefact)
        {
            special = artefact_wpn_property(item, ARTP_BRAND);

            if (!was_known)
            {
                item.flags |= ISFLAG_NOTED_ID;

                // Make a note of it.
                take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_A).c_str(),
                               origin_desc(item).c_str()));
            }
            else
                known_recurser = artefact_known_wpn_property(item,
                                                             ARTP_CURSED);
        }

        if (special != SPWPN_NORMAL)
        {
            // message first
            if (showMsgs)
            {
                switch (special)
                {
                case SPWPN_FLAMING:
                    mpr("It bursts into flame!");
                    break;

                case SPWPN_FREEZING:
                    mpr("It glows with a cold blue light!");
                    break;

                case SPWPN_HOLY_WRATH:
                    mpr("It softly glows with a divine radiance!");
                    break;

                case SPWPN_ELECTROCUTION:
                    if (!silenced(you.pos()))
                    {
                        mpr("You hear the crackle of electricity.",
                            MSGCH_SOUND);
                    }
                    else
                        mpr("You see sparks fly.");
                    break;

                case SPWPN_DRAGON_SLAYING:
                    mpr(player_genus(GENPC_DRACONIAN)
                        || you.form == TRAN_DRAGON
                            ? "You feel a sudden desire to commit suicide."
                            : "You feel a sudden desire to slay dragons!");
                    break;

                case SPWPN_VENOM:
                    mpr("It begins to drip with poison!");
                    break;

                case SPWPN_PROTECTION:
                    mpr("You feel protected!");
                    break;

                case SPWPN_EVASION:
                    mpr("You feel nimbler!");
                    break;

                case SPWPN_DRAINING:
                    mpr("You sense an unholy aura.");
                    break;

                case SPWPN_SPEED:
                    mprf("Your %s tingle!",
                         you.hand_name(true).c_str());
                    break;

                case SPWPN_FLAME:
                    mpr("It bursts into flame!");
                    break;

                case SPWPN_FROST:
                    mpr("It is covered in frost.");
                    break;

                case SPWPN_VAMPIRICISM:
                    if (you.species == SP_VAMPIRE)
                    {
                        mpr("You feel a bloodthirsty glee!");
                        break;
                    }

                    if (you.is_undead == US_ALIVE && !you_foodless())
                    {
                        mpr("You feel a dreadful hunger.");
                        // takes player from Full to Hungry
                        if (!unmeld)
                            make_hungry(4500, false, false);
                    }
                    else
                        mpr("You feel an empty sense of dread.");
                    break;

                case SPWPN_RETURNING:
                    mpr("It wiggles slightly.");
                    break;

                case SPWPN_PAIN:
                {
                    const char* your_arm = you.arm_name(false).c_str();
                    if (you.skill(SK_NECROMANCY) == 0)
                        mpr("You have a feeling of ineptitude.");
                    else if (you.skill(SK_NECROMANCY) <= 6)
                        mprf("Pain shudders through your %s!", your_arm);
                    else
                        mprf("A searing pain shoots up your %s!", your_arm);
                    break;
                }

                case SPWPN_CHAOS:
                    mpr("It is briefly surrounded by a scintillating aura "
                        "of random colours.");
                    break;

                case SPWPN_PENETRATION:
                    mprf("Your %s briefly pass through it before you manage "
                         "to get a firm grip on it.",
                         you.hand_name(true).c_str());
                    break;

                case SPWPN_REAPING:
                    mpr("It is briefly surrounded by shifting shadows.");
                    break;

                case SPWPN_ANTIMAGIC:
                    // Even if your maxmp is 0.
                    mpr("You feel magic leave you.");
                    break;

                default:
                    break;
                }
            }

            // effect second
            switch (special)
            {
            case SPWPN_PROTECTION:
                you.redraw_armour_class = true;
                break;

            case SPWPN_EVASION:
                you.redraw_evasion = true;
                break;

            case SPWPN_DISTORTION:
                mpr("Space warps around you for a moment!");

                if (!was_known)
                {
                    // Xom loves it when you ID a distortion weapon this way,
                    // and even more so if he gifted the weapon himself.
                    god_type god;
                    if (origin_is_god_gift(item, &god) && god == GOD_XOM)
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
        maybe_id_weapon(item);
        break;
    }
    default:
        break;
    }

    if (showMsgs)
        warn_shield_penalties();

    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
}

static void _unequip_weapon_effect(item_def& item, bool showMsgs, bool meld)
{
    you.wield_change = true;
    you.m_quiver->on_weapon_changed();

    // Call this first, so that the unrandart func can set showMsgs to
    // false if it does its own message handling.
    if (is_artefact(item))
        _unequip_artefact_effect(item, &showMsgs, meld);

    if (item.base_type == OBJ_MISCELLANY
        && item.sub_type == MISC_LANTERN_OF_SHADOWS)
    {
        you.attribute[ATTR_SHADOWS] = 0;
        update_vision_range();
    }
    else if (item.base_type == OBJ_WEAPONS)
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
                    mpr("You feel less protected.");
                you.redraw_armour_class = true;
                break;

            case SPWPN_EVASION:
                if (showMsgs)
                    mpr("You feel like more of a target.");
                you.redraw_evasion = true;
                break;

            case SPWPN_VAMPIRICISM:
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

                if (you.duration[DUR_WEAPON_BRAND] == 0 && !meld)
                {
                    // Makes no sense to discourage unwielding a temporarily
                    // branded weapon since you can wait it out. This also
                    // fixes problems with unwield prompts (mantis #793).
                    MiscastEffect(&you, WIELD_MISCAST, SPTYP_TRANSLOCATION,
                                  9, 90, "a distortion unwield");
                }
                break;

            case SPWPN_ANTIMAGIC:
                calc_mp();
                mpr("You feel magic returning to you.");
                break;

                // NOTE: When more are added here, *must* duplicate unwielding
                // effect in vorpalise weapon scroll effect in read_scoll.
            }

            if (you.duration[DUR_WEAPON_BRAND])
            {
                you.duration[DUR_WEAPON_BRAND] = 0;
                set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);

                // We're letting this through even if hiding messages.
                mpr("Your branding evaporates.");
            }
        }
    }
    else if (item.base_type == OBJ_STAVES && item.sub_type == STAFF_POWER)
    {
        int mp = you.magic_points;
        if (you.species == SP_DJINNI)
        {
            mp = you.hp;
            calc_hp();
            mp -= you.hp;
        }
        else
        {
            calc_mp();
            mp -= you.magic_points;
        }

        // Store the MP in case you'll re-wield quickly.
        item.special = mp + you.elapsed_time / POWER_DECAY;

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

static void _equip_armour_effect(item_def& arm, bool unmeld)
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
            mpr("You feel healthy.");
            break;

        case SPARM_SEE_INVISIBLE:
            mpr("You feel perceptive.");
            autotoggle_autopickup(false);
            break;

        case SPARM_DARKNESS:
            if (!you.duration[DUR_INVIS])
                mpr("You become transparent for a moment.");
            break;

        case SPARM_STRENGTH:
            notify_stat_change(STAT_STR, 3, false, arm);
            break;

        case SPARM_DEXTERITY:
            notify_stat_change(STAT_DEX, 3, false, arm);
            break;

        case SPARM_INTELLIGENCE:
            notify_stat_change(STAT_INT, 3, false, arm);
            break;

        case SPARM_PONDEROUSNESS:
            mpr("You feel rather ponderous.");
            break;

        case SPARM_FLYING:
            mpr("You feel rather light.");
            break;

        case SPARM_JUMPING:
            if (you.evokable_jump())
                mpr("You feel more sure on your feet.");
            break;

        case SPARM_MAGIC_RESISTANCE:
            mpr("You feel resistant to hostile enchantments.");
            break;

        case SPARM_PROTECTION:
            mpr("You feel protected.");
            break;

        case SPARM_STEALTH:
            mpr("You feel stealthy.");
            break;

        case SPARM_RESISTANCE:
            mpr("You feel resistant to extremes of temperature.");
            break;

        case SPARM_POSITIVE_ENERGY:
            mpr("Your life force is being protected.");
            break;

        case SPARM_ARCHMAGI:
            if (!you.skill(SK_SPELLCASTING))
                mpr("You feel strangely lacking in power.");
            else
                mpr("You feel powerful.");
            break;

        case SPARM_SPIRIT_SHIELD:
            if (!unmeld && you.spirit_shield() < 2)
            {
                dec_mp(you.magic_points);
                if (you.species == SP_DJINNI)
                    mpr("You feel the presence of a powerless spirit.");
                else
                    mpr("You feel your power drawn to a protective spirit.");
                if (you.species == SP_DEEP_DWARF)
                    mpr("Now linked to your health, your magic stops regenerating.");
            }
            else
                mpr("You feel spirits watching over you.");
            break;

        case SPARM_ARCHERY:
            mpr("You feel that your aim is more steady.");
            break;
        }
    }

    if (is_artefact(arm))
    {
        bool show_msgs = true;
        _equip_artefact_effect(arm, &show_msgs, unmeld);
    }

    if (arm.cursed() && !unmeld)
    {
        mpr("Oops, that feels deathly cold.");
        learned_something_new(HINT_YOU_CURSED);

        if (!known_cursed)
        {
            int amusement = 64;

            // Cursed cloaks prevent you from removing body armour.
            // Cursed gloves prevent switching of rings.
            if (get_armour_slot(arm) == EQ_CLOAK
                || get_armour_slot(arm) == EQ_GLOVES)
            {
                amusement *= 2;
            }

            god_type god;
            if (origin_is_god_gift(arm, &god) && god == GOD_XOM)
                amusement *= 2;

            xom_is_stimulated(amusement);
        }
    }

    if (get_item_slot(arm) == EQ_SHIELD)
        warn_shield_penalties();

    you.redraw_armour_class = true;
    you.redraw_evasion = true;
}

static void _unequip_armour_effect(item_def& item, bool meld)
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
        mpr("\"Was it this warm in here before?\"");
        break;

    case SPARM_COLD_RESISTANCE:
        mpr("You catch a bit of a chill.");
        break;

    case SPARM_POISON_RESISTANCE:
        if (player_res_poison() <= 0)
            mpr("You feel less healthy.");
        break;

    case SPARM_SEE_INVISIBLE:
        if (!you.can_see_invisible())
            mpr("You feel less perceptive.");
        break;

    case SPARM_DARKNESS:
        if (you.duration[DUR_INVIS]
            && !you.attribute[ATTR_INVIS_UNCANCELLABLE]
            && !you.evokable_invis())
        {
            you.duration[DUR_INVIS] = 1;
        }
        break;

    case SPARM_STRENGTH:
        notify_stat_change(STAT_STR, -3, false, item, true);
        break;

    case SPARM_DEXTERITY:
        notify_stat_change(STAT_DEX, -3, false, item, true);
        break;

    case SPARM_INTELLIGENCE:
        notify_stat_change(STAT_INT, -3, false, item, true);
        break;

    case SPARM_PONDEROUSNESS:
        mpr("That put a bit of spring back into your step.");
        break;

    case SPARM_FLYING:
        if (you.attribute[ATTR_PERM_FLIGHT]
            && !you.wearing_ego(EQ_ALL_ARMOUR, SPARM_FLYING)
            && !you.racial_permanent_flight())
        {
            you.attribute[ATTR_PERM_FLIGHT] = 0;
            if (you.evokable_flight())
                fly_player(you.skill(SK_EVOCATIONS, 2) + 30, true);
        }

        // since a permflight item can keep tempflight evocations going
        // we should check tempflight here too
        if (you.cancellable_flight() && !you.evokable_flight())
        {
            you.duration[DUR_FLIGHT] = 0;
            land_player();
        }
        break;

    case SPARM_JUMPING:
        if (!you.evokable_jump()
            && player_mutation_level(MUT_JUMP) < player_evoke_jump_range())
            mpr("You feel less sure on your feet.");
        break;
    case SPARM_MAGIC_RESISTANCE:
        mpr("You feel less resistant to hostile enchantments.");
        break;

    case SPARM_PROTECTION:
        mpr("You feel less protected.");
        break;

    case SPARM_STEALTH:
        mpr("You feel less stealthy.");
        break;

    case SPARM_RESISTANCE:
        mpr("You feel hot and cold all over.");
        break;

    case SPARM_POSITIVE_ENERGY:
        mpr("You feel vulnerable.");
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
        else if (you.wearing(EQ_AMULET, AMU_GUARDIAN_SPIRIT, true))
        {
            item_def& amu(you.inv[you.equip[EQ_AMULET]]);
            wear_id_type(amu);
        }
        break;

    case SPARM_ARCHERY:
        mpr("Your aim is not that steady anymore.");
        break;

    default:
        break;
    }

    if (is_artefact(item))
        _unequip_artefact_effect(item, NULL, meld);
}

static void _remove_amulet_of_faith(item_def &item)
{
    if (!you_worship(GOD_NO_GOD)
        && !you_worship(GOD_XOM))
    {
        simple_god_message(" seems less interested in you.");

        const int piety_loss = div_rand_round(you.piety, 3);
        // Piety penalty for removing the Amulet of Faith.
        if (you.piety - piety_loss > 10)
        {
            mprf(MSGCH_GOD,
                 "%s leaches power out of you as you remove it.",
                 item.name(DESC_YOUR).c_str());
            dprf("%s: piety leach: %d",
                 item.name(DESC_PLAIN).c_str(), piety_loss);
            lose_piety(piety_loss);
        }
    }
}

static void _equip_jewellery_effect(item_def &item, bool unmeld)
{
    item_type_id_state_type ident        = ID_TRIED_TYPE;
    artefact_prop_type      fake_rap     = ARTP_NUM_PROPERTIES;
    bool                    learn_pluses = false;

    // FIXME:
    // Randart jewellery shouldn't auto-ID just because the base type
    // is known. Somehow the player should still be told, preferably
    // by message. (jpeg)

    // XXX has to match artefact.cc:_artefact_desc_properties(), sort-of (SamB)

    const bool artefact     = is_artefact(item);
    const bool known_cursed = item_known_cursed(item);
    const bool known_bad    = (item_type_known(item)
                               && item_value(item) <= 2);

    switch (item.sub_type)
    {
    case RING_HUNGER:
    case RING_LIFE_PROTECTION:
    case RING_POISON_RESISTANCE:
    case RING_PROTECTION_FROM_COLD:
    case RING_PROTECTION_FROM_FIRE:
    case RING_PROTECTION_FROM_MAGIC:
    case RING_SUSTAIN_ABILITIES:
    case RING_SUSTENANCE:
    case RING_SLAYING:
        break;

    case RING_FIRE:
        mpr("You feel more attuned to fire.");
        ident = ID_KNOWN_TYPE;
        break;

    case RING_ICE:
        mpr("You feel more attuned to ice.");
        ident = ID_KNOWN_TYPE;
        break;

    case RING_WIZARDRY:
        ident = ID_KNOWN_TYPE;
        break;

    case RING_SEE_INVISIBLE:
        // We might have to turn autopickup back on again.
        // TODO: Check all monsters in LOS. If any of them are invisible
        //       (and thus become visible once the ring is worn), the ring
        //       should be autoidentified.
        if (item_type_known(item))
            autotoggle_autopickup(false);
        break;

    case RING_PROTECTION:
        you.redraw_armour_class = true;
        if (item.plus != 0)
        {
            fake_rap = ARTP_AC;
            ident = ID_KNOWN_TYPE;

            learn_pluses = true;
        }
        break;

    case RING_INVISIBILITY:
        mprf("You become %stransparent for a moment.",
             you.duration[DUR_INVIS] ? "more " : "");
        fake_rap = ARTP_INVISIBLE;
        ident = ID_KNOWN_TYPE;
        break;

    case RING_EVASION:
        you.redraw_evasion = true;
        if (item.plus != 0)
        {
            fake_rap = ARTP_EVASION;
            ident = ID_KNOWN_TYPE;

            learn_pluses = true;
        }
        break;

    case RING_STRENGTH:
        if (item.plus)
        {
            notify_stat_change(STAT_STR, item.plus, false, item);

            fake_rap = ARTP_STRENGTH;
            ident = ID_KNOWN_TYPE;

           learn_pluses = true;
        }
        break;

    case RING_DEXTERITY:
        if (item.plus)
        {
            notify_stat_change(STAT_DEX, item.plus, false, item);

            fake_rap = ARTP_DEXTERITY;
            ident = ID_KNOWN_TYPE;

           learn_pluses = true;
        }
        break;

    case RING_INTELLIGENCE:
        if (item.plus)
        {
            notify_stat_change(STAT_INT, item.plus, false, item);

            fake_rap = ARTP_INTELLIGENCE;
            ident = ID_KNOWN_TYPE;

           learn_pluses = true;
        }
        break;

    case RING_MAGICAL_POWER:
        if ((you.max_magic_points + 9) *
            (1.0+player_mutation_level(MUT_HIGH_MAGIC)/10.0) > 50)
        {
            mpr("You feel your magic capacity is already quite full.");
        }
        else
            canned_msg(MSG_MANA_INCREASE);

        calc_mp();

        fake_rap = ARTP_MAGICAL_POWER;
        ident = ID_KNOWN_TYPE;
        break;

    case RING_FLIGHT:
        mprf("You feel %sbuoyant.", you.airborne() ? "more " : "");
        fake_rap = ARTP_FLY;
        ident = ID_KNOWN_TYPE;
        break;

    case RING_TELEPORTATION:
        if (crawl_state.game_is_sprint())
            mpr("You feel a slight, muted jump rush through you.");
        else
            // keep in sync with player_teleport
            mprf("You feel slightly %sjumpy.",
                 (player_teleport(false) > 8) ? "more " : "");
        ident = ID_KNOWN_TYPE;
        break;

    case RING_TELEPORT_CONTROL:
        mprf("You feel %scontrolled for a moment.",
              you.duration[DUR_CONTROL_TELEPORT] ? "more " : "");
        ident = ID_KNOWN_TYPE;
        break;

    case AMU_RAGE:
        mpr("You feel a brief urge to hack something to bits.");
        fake_rap = ARTP_BERSERK;
        ident = ID_KNOWN_TYPE;
        break;

    case AMU_FAITH:
        if (!you_worship(GOD_NO_GOD))
        {
            mpr("You feel a surge of divine interest.", MSGCH_GOD);
            ident = ID_KNOWN_TYPE;
        }
        break;

    case AMU_THE_GOURMAND:
        // What's this supposed to achieve? (jpeg)
        you.duration[DUR_GOURMAND] = 0;

        if (you.species != SP_MUMMY
            && you.species != SP_VAMPIRE
            && player_mutation_level(MUT_HERBIVOROUS) < 3)
        {
            mpr("You feel a craving for the dungeon's cuisine.");
            ident = ID_KNOWN_TYPE;
        }
        break;

#if TAG_MAJOR_VERSION == 34
    case AMU_CONTROLLED_FLIGHT:
        ident = ID_KNOWN_TYPE;
        break;
#endif

    case AMU_GUARDIAN_SPIRIT:
        if (you.spirit_shield() < 2 && !unmeld)
        {
            dec_mp(you.magic_points);
            if (you.species == SP_DJINNI)
                mpr("You feel the presence of a powerless spirit.");
            else
                mpr("You feel your power drawn to a protective spirit.");
            if (you.species == SP_DEEP_DWARF)
                mpr("Now linked to your health, your magic stops regenerating.");
        }
        else
            mpr("You feel spirits watching over you.");
        ident = ID_KNOWN_TYPE;
        break;

    case RING_REGENERATION:
        // To be exact, bloodless vampires should get the id only after they
        // drink anything.  Not worth complicating the code, IMHO. [1KB]
        if (player_mutation_level(MUT_SLOW_HEALING) < 3)
            ident = ID_KNOWN_TYPE;
        break;

    case AMU_STASIS:
        // Berserk is possible with a Battlelust card or with a moth of wrath
        // that affects you while donning the amulet.
        int amount = you.duration[DUR_HASTE] + you.duration[DUR_SLOW]
                     + you.duration[DUR_BERSERK] + you.duration[DUR_FINESSE];
        if (you.duration[DUR_TELEPORT])
            amount += 30 + random2(150);
        if (amount)
        {
            mprf("The amulet engulfs you in a%s magical discharge!",
                 (amount > 250) ? " massive" :
                 (amount >  50) ? " violent" :
                                  "");
            ident = ID_KNOWN_TYPE;

            contaminate_player(pow(amount, 0.333), item_type_known(item));

            int dir = 0;
            if (you.duration[DUR_HASTE])
                dir++;
            if (you.duration[DUR_SLOW])
                dir--;
            if (dir > 0)
                mpr("You abruptly slow down.", MSGCH_DURATION);
            else if (dir < 0)
                mpr("Your slowness suddenly goes away.", MSGCH_DURATION);
            if (you.duration[DUR_TELEPORT])
                mpr("You feel strangely stable.", MSGCH_DURATION);
            if (you.duration[DUR_BERSERK])
                mpr("You violently calm down.", MSGCH_DURATION);
            // my thesaurus says this usage is correct
            if (you.duration[DUR_FINESSE])
                mpr("Your hands get arrested.", MSGCH_DURATION);
            you.duration[DUR_HASTE] = 0;
            you.duration[DUR_SLOW] = 0;
            you.duration[DUR_TELEPORT] = 0;
            you.duration[DUR_BERSERK] = 0;
            you.duration[DUR_FINESSE] = 0;
        }
        break;

    // When making a jewel type auto-id, please update Ashenzari's list
    // in godpassive.cc as well.
    }

    // Artefacts have completely different appearance than base types
    // so we don't allow them to make the base types known.
    if (artefact)
    {
        bool show_msgs = true;
        _equip_artefact_effect(item, &show_msgs, unmeld);

        if (ident == ID_KNOWN_TYPE)
            set_ident_flags(item, ISFLAG_KNOW_TYPE);

        if (learn_pluses)
            set_ident_flags(item, ISFLAG_KNOW_PLUSES);

        if (fake_rap != ARTP_NUM_PROPERTIES)
            artefact_wpn_learn_prop(item, fake_rap);

        item.flags |= ISFLAG_TRIED;
    }
    else
    {
        set_ident_type(item, ident);

        if (ident == ID_KNOWN_TYPE)
            set_ident_flags(item, ISFLAG_EQ_JEWELLERY_MASK);
    }

    if (item.cursed())
    {
        mprf("Oops, that %s feels deathly cold.",
             jewellery_is_amulet(item)? "amulet" : "ring");
        learned_something_new(HINT_YOU_CURSED);

        int amusement = 32;
        if (!known_cursed && !known_bad)
        {
            amusement *= 2;

            god_type god;
            if (origin_is_god_gift(item, &god) && god == GOD_XOM)
                amusement *= 2;
        }
        xom_is_stimulated(amusement);
    }

    // Cursed or not, we know that since we've put the ring on.
    set_ident_flags(item, ISFLAG_KNOW_CURSE);

    mpr_nocap(item.name(DESC_INVENTORY_EQUIP).c_str());
}

static void _unequip_jewellery_effect(item_def &item, bool mesg, bool meld)
{
    // The ring/amulet must already be removed from you.equip at this point.

    // Turn off show_uncursed before getting the item name, because this item
    // was just removed, and the player knows it's uncursed.
    const bool old_showuncursed = Options.show_uncursed;
    Options.show_uncursed = false;

    Options.show_uncursed = old_showuncursed;

    switch (item.sub_type)
    {
    case RING_FIRE:
    case RING_HUNGER:
    case RING_ICE:
    case RING_LIFE_PROTECTION:
    case RING_POISON_RESISTANCE:
    case RING_PROTECTION_FROM_COLD:
    case RING_PROTECTION_FROM_FIRE:
    case RING_PROTECTION_FROM_MAGIC:
    case RING_REGENERATION:
    case RING_SEE_INVISIBLE:
    case RING_SLAYING:
    case RING_SUSTAIN_ABILITIES:
    case RING_SUSTENANCE:
    case RING_TELEPORTATION:
    case RING_WIZARDRY:
    case RING_TELEPORT_CONTROL:
        break;

    case RING_PROTECTION:
        you.redraw_armour_class = true;
        break;

    case RING_EVASION:
        you.redraw_evasion = true;
        break;

    case RING_STRENGTH:
        notify_stat_change(STAT_STR, -item.plus, false, item, true);
        break;

    case RING_DEXTERITY:
        notify_stat_change(STAT_DEX, -item.plus, false, item, true);
        break;

    case RING_INTELLIGENCE:
        notify_stat_change(STAT_INT, -item.plus, false, item, true);
        break;

    case RING_FLIGHT:
        if (you.cancellable_flight() && !you.evokable_flight())
        {
            you.duration[DUR_FLIGHT] = 0;
            land_player();
        }
        break;

    case RING_INVISIBILITY:
        if (you.duration[DUR_INVIS]
            && !you.attribute[ATTR_INVIS_UNCANCELLABLE]
            && !you.evokable_invis())
        {
            you.duration[DUR_INVIS] = 1;
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

    case AMU_GUARDIAN_SPIRIT:
        if (you.species == SP_DEEP_DWARF)
            mpr("Your magic begins regenerating once more.");
        break;
    }

    if (is_artefact(item))
        _unequip_artefact_effect(item, &mesg, meld);

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
    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;

    return true;
}
