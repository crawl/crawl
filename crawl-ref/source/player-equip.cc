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
#include "spl-mis.h"
#include "state.h"
#include "stuff.h"
#include "transform.h"
#include "xom.h"

#include <cmath>

static void _equip_effect(equipment_type slot, int item_slot, bool unmeld, bool msg);
static void _unequip_effect(equipment_type slot, int item_slot, bool msg);

// Fill an empty equipment slot.
void equip_item(equipment_type slot, int item_slot, bool msg)
{
    ASSERT(slot > EQ_NONE && slot < NUM_EQUIP);
    ASSERT(you.equip[slot] == -1);
    ASSERT(!you.melded[slot]);

    you.equip[slot] = item_slot;

    _equip_effect(slot, item_slot, false, msg);
}

// Clear an equipment slot (possibly melded).
bool unequip_item(equipment_type slot, bool msg)
{
    ASSERT(slot > EQ_NONE && slot < NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    const int item_slot = you.equip[slot];
    if (item_slot == -1)
        return (false);
    else
    {
        you.equip[slot] = -1;
        if (!you.melded[slot])
            _unequip_effect(slot, item_slot, msg);
        else
            you.melded[slot] = false;
        return (true);
    }
}

// Meld a slot (if equipped).
bool meld_slot(equipment_type slot, bool msg)
{
    ASSERT(slot > EQ_NONE && slot < NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && !you.melded[slot])
    {
        you.melded[slot] = true;
        _unequip_effect(slot, you.equip[slot], msg);
        return (true);
    }
    return (false);
}

bool unmeld_slot(equipment_type slot, bool msg)
{
    ASSERT(slot > EQ_NONE && slot < NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && you.melded[slot])
    {
        you.melded[slot] = false;
        _equip_effect(slot, you.equip[slot], true, msg);
        return (true);
    }
    return (false);
}

static void _equip_weapon_effect(item_def& item, bool showMsgs);
static void _unequip_weapon_effect(item_def& item, bool showMsgs);
static void _equip_armour_effect(item_def& arm, bool unmeld);
static void _unequip_armour_effect(item_def& item);
static void _equip_jewellery_effect(item_def &item);
static void _unequip_jewellery_effect(item_def &item, bool mesg);

static void _equip_effect(equipment_type slot, int item_slot, bool unmeld, bool msg)
{
    item_def& item = you.inv[item_slot];
    equipment_type eq = get_item_slot(item);

    if (slot == EQ_WEAPON && eq != EQ_WEAPON)
        return;

    ASSERT(slot == eq
           || eq == EQ_RINGS
              && (slot == EQ_LEFT_RING || slot == EQ_RIGHT_RING));

    if (slot == EQ_WEAPON)
        _equip_weapon_effect(item, msg);
    else if (slot >= EQ_CLOAK && slot <= EQ_BODY_ARMOUR)
        _equip_armour_effect(item, unmeld);
    else if (slot >= EQ_LEFT_RING && slot <= EQ_AMULET)
        _equip_jewellery_effect(item);
}

static void _unequip_effect(equipment_type slot, int item_slot, bool msg)
{
    item_def& item = you.inv[item_slot];
    equipment_type eq = get_item_slot(item);

    if (slot == EQ_WEAPON && eq != EQ_WEAPON)
        return;

    ASSERT(slot == eq
           || eq == EQ_RINGS
              && (slot == EQ_LEFT_RING || slot == EQ_RIGHT_RING));

    if (slot == EQ_WEAPON)
        _unequip_weapon_effect(item, msg);
    else if (slot >= EQ_CLOAK && slot <= EQ_BODY_ARMOUR)
        _unequip_armour_effect(item);
    else if (slot >= EQ_LEFT_RING && slot <= EQ_AMULET)
        _unequip_jewellery_effect(item, msg);
}


///////////////////////////////////////////////////////////
// Actual equip and unequip effect implementation below
//

static void _equip_artefact_effect(item_def &item, bool *show_msgs=NULL,
                                   bool unmeld=false)
{
#define unknown_proprt(prop) (proprt[(prop)] && !known[(prop)])

    ASSERT( is_artefact( item ) );

    // Call unrandart equip function first, so that it can modify the
    // artefact's properties before they're applied.
    if (is_unrandom_artefact( item ))
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

    artefact_properties_t  proprt;
    artefact_known_props_t known;
    artefact_wpn_properties( item, proprt, known );

    // Only give property messages for previously unknown properties.
    if (proprt[ARTP_AC])
    {
        you.redraw_armour_class = true;
        if (!known[ARTP_AC])
        {
            mprf("You feel %s.", proprt[ARTP_AC] > 0?
                 "well-protected" : "more vulnerable");
            artefact_wpn_learn_prop(item, ARTP_AC);
        }
    }

    if (proprt[ARTP_EVASION])
    {
        you.redraw_evasion = true;
        if (!known[ARTP_EVASION])
        {
            mprf("You feel somewhat %s.", proprt[ARTP_EVASION] > 0?
                 "nimbler" : "more awkward");
            artefact_wpn_learn_prop(item, ARTP_EVASION);
        }
    }

    if (proprt[ARTP_PONDEROUS])
    {
        mpr("You feel rather ponderous.");
        che_handle_change(CB_PONDEROUS, 1);
    }

    if (proprt[ARTP_MAGICAL_POWER] && !known[ARTP_MAGICAL_POWER])
    {
        canned_msg(proprt[ARTP_MAGICAL_POWER] > 0 ? MSG_MANA_INCREASE
                                                  : MSG_MANA_DECREASE);
        artefact_wpn_learn_prop(item, ARTP_MAGICAL_POWER);
    }

    // Modify ability scores.
    // Output result even when identified (because of potential fatality).
    notify_stat_change( STAT_STR,     proprt[ARTP_STRENGTH],     false, item );
    notify_stat_change( STAT_INT, proprt[ARTP_INTELLIGENCE], false, item );
    notify_stat_change( STAT_DEX,    proprt[ARTP_DEXTERITY],    false, item );

    const artefact_prop_type stat_props[3] =
        {ARTP_STRENGTH, ARTP_INTELLIGENCE, ARTP_DEXTERITY};

    for (int i = 0; i < 3; i++)
        if (unknown_proprt(stat_props[i]))
            artefact_wpn_learn_prop(item, stat_props[i]);

    // For evokable stuff, check whether other equipped items yield
    // the same ability.  If not, and if the ability granted hasn't
    // already been discovered, give a message.
    if (unknown_proprt(ARTP_LEVITATE)
        && !items_give_ability(item.link, ARTP_LEVITATE))
    {
        if (you.airborne())
            mpr("You feel vaguely more buoyant than before.");
        else
            mpr("You feel buoyant.");
        artefact_wpn_learn_prop(item, ARTP_LEVITATE);
    }

    if (unknown_proprt(ARTP_INVISIBLE) && !you.duration[DUR_INVIS])
    {
        mpr("You become transparent for a moment.");
        artefact_wpn_learn_prop(item, ARTP_INVISIBLE);
    }

    if (unknown_proprt(ARTP_BERSERK)
        && !items_give_ability(item.link, ARTP_BERSERK))
    {
        mpr("You feel a brief urge to hack something to bits.");
        artefact_wpn_learn_prop(item, ARTP_BERSERK);
    }

    if (!unmeld && !item.cursed() && proprt[ARTP_CURSED] > 0
         && one_chance_in(proprt[ARTP_CURSED]))
    {
        do_curse_item( item, false );
        artefact_wpn_learn_prop(item, ARTP_CURSED);
    }

    if (proprt[ARTP_NOISES])
        you.attribute[ATTR_NOISES] = 1;

    if (!alreadyknown && Options.autoinscribe_artefacts)
        add_autoinscription(item, artefact_auto_inscription(item));

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you use an unknown random artefact and
        // there is a dangerous monster nearby...
        xom_is_stimulated(128);
    }

    // Let's try this here instead of up there.
    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();
#undef unknown_proprt
}

static void _unequip_artefact_effect(const item_def &item, bool *show_msgs=NULL)
{
    ASSERT( is_artefact( item ) );

    artefact_properties_t proprt;
    artefact_known_props_t known;
    artefact_wpn_properties( item, proprt, known );

    if (proprt[ARTP_AC])
    {
        you.redraw_armour_class = true;
        if (!known[ARTP_AC])
        {
            mprf("You feel less %s.",
                 proprt[ARTP_AC] > 0? "well-protected" : "vulnerable");
        }
    }

    if (proprt[ARTP_EVASION])
    {
        you.redraw_evasion = true;
        if (!known[ARTP_EVASION])
        {
            mprf("You feel less %s.",
                 proprt[ARTP_EVASION] > 0? "nimble" : "awkward");
        }
    }

    if (proprt[ARTP_PONDEROUS])
    {
        mpr("That put a bit of spring back into your step.");
        che_handle_change(CB_PONDEROUS, -1);
    }

    if (proprt[ARTP_MAGICAL_POWER] && !known[ARTP_MAGICAL_POWER])
    {
        canned_msg(proprt[ARTP_MAGICAL_POWER] > 0 ? MSG_MANA_DECREASE
                                                  : MSG_MANA_INCREASE);
    }

    // Modify ability scores; always output messages.
    notify_stat_change(STAT_STR, -proprt[ARTP_STRENGTH],     false, item,
                       true);
    notify_stat_change(STAT_INT, -proprt[ARTP_INTELLIGENCE], false, item,
                       true);
    notify_stat_change(STAT_DEX, -proprt[ARTP_DEXTERITY],    false, item,
                       true);

    if (proprt[ARTP_NOISES] != 0)
        you.attribute[ATTR_NOISES] = 0;

    if (proprt[ARTP_LEVITATE] != 0
        && you.duration[DUR_LEVITATION] > 2
        && !you.attribute[ATTR_LEV_UNCANCELLABLE]
        && !you.permanent_levitation())
    {
        you.duration[DUR_LEVITATION] = 1;
    }

    if (proprt[ARTP_INVISIBLE] != 0 && you.duration[DUR_INVIS] > 1)
        you.duration[DUR_INVIS] = 1;

    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();

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

// Provide a function for handling initial wielding of 'special'
// weapons, or those whose function is annoying to reproduce in
// other places *cough* auto-butchering *cough*.    {gdl}
static void _equip_weapon_effect(item_def& item, bool showMsgs)
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

            you.current_vision -= 2;
            set_los_radius(you.current_vision);
            you.attribute[ATTR_SHADOWS] = 1;
        }
        else if (item.sub_type == MISC_HORN_OF_GERYON)
            set_ident_flags(item, ISFLAG_IDENT_MASK);
        break;
    }

    case OBJ_STAVES:
    {
        if (item.sub_type == STAFF_POWER)
        {
            int mp = item.special - you.elapsed_time / POWER_DECAY;

            if (mp > 0)
                you.magic_points += mp;

            if ((you.max_magic_points + 13) *
                (1.0+player_mutation_level(MUT_HIGH_MAGIC)/10.0) > 50)
                mpr("You feel your mana capacity is already quite full.");
            else
                canned_msg(MSG_MANA_INCREASE);

            calc_mp();
            set_ident_type(item, ID_KNOWN_TYPE);
            set_ident_flags(item, ISFLAG_EQ_WEAPON_MASK);
        }
        else if (!maybe_identify_staff(item))
        {
            // Give curse status when wielded.
            // Right now that's always "uncursed". -- bwr
            set_ident_flags(item, ISFLAG_KNOW_CURSE);
        }
        // Automatically identify rods; you can do this by wielding and then
        // evoking them, so do it automatically instead. We don't need to give
        // a message either, as the game will do that automatically. {due}
        if (item_is_rod(item))
        {
            if (!item_type_known(item))
            {
                set_ident_type( OBJ_STAVES, item.sub_type, ID_KNOWN_TYPE );
                set_ident_flags( item, ISFLAG_KNOW_TYPE );
            }
            if (!item_ident( item, ISFLAG_KNOW_PLUSES))
                set_ident_flags( item, ISFLAG_KNOW_PLUSES );
        }
        break;
    }

    case OBJ_WEAPONS:
    {
        if (showMsgs)
        {
            if (is_holy_item(item) && you.religion == GOD_YREDELEMNUL)
                mpr("You really shouldn't be using a holy item like this.");
            else if (is_unholy_item(item) && is_good_god(you.religion))
                mpr("You really shouldn't be using an unholy item like this.");
            else if (is_evil_item(item) && is_good_god(you.religion))
                mpr("You really shouldn't be using an evil item like this.");
            else if (is_chaotic_item(item) && you.religion == GOD_ZIN)
                mpr("You really shouldn't be using a chaotic item like this.");
            else if (is_hasty_item(item) && you.religion == GOD_CHEIBRIADOS)
                mpr("You really shouldn't be using a fast item like this.");
        }

        // Call unrandrt equip func before item is identified.
        if (artefact)
            _equip_artefact_effect(item, &showMsgs);

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

                if (Options.autoinscribe_artefacts)
                    add_autoinscription(item, artefact_auto_inscription(item));

                // Make a note of it.
                take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_NOCAP_A).c_str(),
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

                case SPWPN_ORC_SLAYING:
                    mpr((you.species == SP_HILL_ORC)
                            ? "You feel a sudden desire to commit suicide."
                            : "You feel a sudden desire to kill orcs!");
                    break;

                case SPWPN_DRAGON_SLAYING:
                    mpr(player_genus(GENPC_DRACONIAN)
                        || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
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

                    if (you.is_undead == US_ALIVE)
                    {
                        mpr("You feel a dreadful hunger.");
                        // takes player from Full to Hungry
                        make_hungry(4500, false, false);
                    }
                    else
                        mpr("You feel an empty sense of dread.");
                    break;

                case SPWPN_RETURNING:
                    mpr("It wiggles slightly.");
                    break;

                case SPWPN_PAIN:
                    if (you.skills[SK_NECROMANCY] == 0)
                        mpr("You have a feeling of ineptitude.");
                    else if (you.skills[SK_NECROMANCY] <= 4)
                        mpr("Pain shudders through your arm!");
                    else
                        mpr("A searing pain shoots up your arm!");
                    break;

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
                        xom_is_stimulated(255);
                    else
                        xom_is_stimulated(128);
                }
                break;

            default:
                break;
            }
        }

        if (item.cursed())
        {
            mpr("It sticks to your hand!");
            int amusement = 16;
            if (!known_cursed && !known_recurser)
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

        break;
    }
    default:
        break;
    }

    if (showMsgs)
        warn_shield_penalties();

    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
}

static void _unequip_weapon_effect(item_def& item, bool showMsgs)
{
    you.m_quiver->on_weapon_changed();

    // Call this first, so that the unrandart func can set showMsgs to
    // false if it does its own message handling.
    if (is_artefact(item))
        _unequip_artefact_effect(item, &showMsgs);

    if (item.base_type == OBJ_MISCELLANY
        && item.sub_type == MISC_LANTERN_OF_SHADOWS )
    {
        you.current_vision += 2;
        set_los_radius(you.current_vision);
        you.attribute[ATTR_SHADOWS] = 0;
    }
    else if (item.base_type == OBJ_WEAPONS)
    {
        const int brand = get_weapon_brand( item );

        if (brand != SPWPN_NORMAL)
        {
            const std::string msg = item.name(DESC_CAP_YOUR);

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

                if (you.duration[DUR_WEAPON_BRAND] == 0)
                {
                    // Makes no sense to discourage unwielding a temporarily
                    // branded weapon since you can wait it out. This also
                    // fixes problems with unwield prompts (mantis #793).
                    MiscastEffect(&you, WIELD_MISCAST, SPTYP_TRANSLOCATION,
                                  9, 90, "distortion unwield");
                }
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
        calc_mp();
        mp -= you.magic_points;

        // Store the MP in case you'll re-wield quickly.
        item.special = mp + you.elapsed_time / POWER_DECAY;

        canned_msg(MSG_MANA_DECREASE);
    }
}

static void _equip_armour_effect(item_def& arm, bool unmeld)
{
    const bool known_cursed = item_known_cursed(arm);
    int ego = get_armour_ego_type( arm );
    if (ego != SPARM_NORMAL)
    {
        switch (ego)
        {
        case SPARM_RUNNING:
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
            che_handle_change(CB_PONDEROUS, 1);
            you.redraw_evasion = true;
            break;

        case SPARM_LEVITATION:
            mpr("You feel rather light.");
            break;

        case SPARM_MAGIC_RESISTANCE:
            mpr("You feel resistant to magic.");
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
            if (!you.skills[SK_SPELLCASTING])
                mpr("You feel strangely numb.");
            else
                mpr("You feel extremely powerful.");
            break;

        case SPARM_SPIRIT_SHIELD:
            if (player_spirit_shield() < 2)
            {
                set_mp(0, false);
                mpr("You feel spirits watching over you.");
                if (you.species == SP_DEEP_DWARF)
                    mpr("Now linked to your health, your magic stops regenerating.");
            }
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

static void _unequip_armour_effect(item_def& item)
{
    you.redraw_armour_class = true;
    you.redraw_evasion = true;

    switch (get_armour_ego_type(item))
    {
    case SPARM_RUNNING:
        mpr("You feel rather sluggish.");
        break;

    case SPARM_FIRE_RESISTANCE:
        mpr("\"Was it this warm in here before?\"");
        break;

    case SPARM_COLD_RESISTANCE:
        mpr("You catch a bit of a chill.");
        break;

    case SPARM_POISON_RESISTANCE:
        if (!player_res_poison())
            mpr("You feel less healthy.");
        break;

    case SPARM_SEE_INVISIBLE:
        if (!you.can_see_invisible())
            mpr("You feel less perceptive.");
        break;

    case SPARM_DARKNESS:        // I do not understand this {dlb}
        if (you.duration[DUR_INVIS])
            you.duration[DUR_INVIS] = 1;
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
        che_handle_change(CB_PONDEROUS, -1);
        break;

    case SPARM_LEVITATION:
        if (you.duration[DUR_LEVITATION] && !you.attribute[ATTR_LEV_UNCANCELLABLE])
            you.duration[DUR_LEVITATION] = 1;
        break;

    case SPARM_MAGIC_RESISTANCE:
        mpr("You feel less resistant to magic.");
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
        if (!player_spirit_shield())
        {
            mpr("You feel strangely alone.");
            if (you.species == SP_DEEP_DWARF)
                mpr("Your magic begins regenerating once more.");
        }
        else if (player_equip(EQ_AMULET, AMU_GUARDIAN_SPIRIT, true))
        {
            item_def& amu(you.inv[you.equip[EQ_AMULET]]);
            if (!item_type_known(amu))
            {
                set_ident_type(amu.base_type, amu.sub_type, ID_KNOWN_TYPE);
                set_ident_flags(amu, ISFLAG_KNOW_PROPERTIES);
                mprf("You are wearing: %s",
                     amu.name(DESC_INVENTORY_EQUIP).c_str());
            }
        }
        break;

    case SPARM_ARCHERY:
        mpr("Your aim is not that steady anymore.");
        break;

    default:
        break;
    }

    if (is_artefact(item))
        _unequip_artefact_effect(item);
}

static void _remove_amulet_of_faith(item_def &item)
{
    if (you.religion != GOD_NO_GOD
        && you.religion != GOD_XOM)
    {
        simple_god_message(" seems less interested in you.");

        const int piety_loss = div_rand_round(you.piety, 3);
        // Piety penalty for removing the Amulet of Faith.
        if (you.piety - piety_loss > 10)
        {
            mprf(MSGCH_GOD,
                 "%s leaches power out of you as you remove it.",
                 item.name(DESC_CAP_YOUR).c_str());
            dprf("%s: piety leach: %d",
                 item.name(DESC_PLAIN).c_str(), piety_loss);
            lose_piety(piety_loss);
        }
    }
}

static void _equip_jewellery_effect(item_def &item)
{
    item_type_id_state_type ident        = ID_TRIED_TYPE;
    artefact_prop_type      fake_rap     = ARTP_NUM_PROPERTIES;
    bool                    learn_pluses = false;

    // Randart jewellery shouldn't auto-ID just because the base type
    // is known. Somehow the player should still be told, preferably
    // by message. (jpeg)
    const bool artefact     = is_artefact(item);
    const bool known_pluses = item_ident(item, ISFLAG_KNOW_PLUSES);
    const bool known_cursed = item_known_cursed(item);
    const bool known_bad    = (item_type_known(item)
                               && item_value(item) <= 2);

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
    case RING_SUSTAIN_ABILITIES:
    case RING_SUSTENANCE:
    case RING_SLAYING:
    case RING_WIZARDRY:
    case RING_TELEPORT_CONTROL:
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
            if (!artefact)
                ident = ID_KNOWN_TYPE;
            else if (!known_pluses)
            {
                mprf("You feel %s.", item.plus > 0 ?
                     "well-protected" : "more vulnerable");
            }
            learn_pluses = true;
        }
        break;

    case RING_INVISIBILITY:
        if (!you.duration[DUR_INVIS])
        {
            mpr("You become transparent for a moment.");
            if (artefact)
                fake_rap = ARTP_INVISIBLE;
            else
                ident = ID_KNOWN_TYPE;
        }
        break;

    case RING_EVASION:
        you.redraw_evasion = true;
        if (item.plus != 0)
        {
            if (!artefact)
                ident = ID_KNOWN_TYPE;
            else if (!known_pluses)
                mprf("You feel %s.", item.plus > 0? "nimbler" : "more awkward");
            learn_pluses = true;
        }
        break;

    case RING_STRENGTH:
        if (item.plus)
        {
            notify_stat_change(STAT_STR, item.plus, false, item);

            if (artefact)
                fake_rap = ARTP_STRENGTH;
            else
                ident = ID_KNOWN_TYPE;

           learn_pluses = true;
        }
        break;

    case RING_DEXTERITY:
        if (item.plus)
        {
            notify_stat_change(STAT_DEX, item.plus, false, item);

            if (artefact)
                fake_rap = ARTP_DEXTERITY;
            else
                ident = ID_KNOWN_TYPE;

           learn_pluses = true;
        }
        break;

    case RING_INTELLIGENCE:
        if (item.plus)
        {
            notify_stat_change(STAT_INT, item.plus, false, item);

            if (artefact)
                fake_rap = ARTP_INTELLIGENCE;
            else
                ident = ID_KNOWN_TYPE;

           learn_pluses = true;
        }
        break;

    case RING_MAGICAL_POWER:
        if ((you.max_magic_points + 9) *
            (1.0+player_mutation_level(MUT_HIGH_MAGIC)/10.0) > 50)
            mpr("You feel your mana capacity is already quite full.");
        else
            canned_msg(MSG_MANA_INCREASE);

        calc_mp();
        if (artefact)
            fake_rap = ARTP_MAGICAL_POWER;
        else
            ident = ID_KNOWN_TYPE;
        break;

    case RING_LEVITATION:
        if (!scan_artefacts(ARTP_LEVITATE))
        {
            if (you.airborne())
                mpr("You feel vaguely more buoyant than before.");
            else
                mpr("You feel buoyant.");
            if (artefact)
                fake_rap = ARTP_LEVITATE;
            else
                ident = ID_KNOWN_TYPE;
        }
        break;

    case RING_TELEPORTATION:
        if (crawl_state.game_is_sprint())
            mpr("You feel a slight, muted jump rush through you.");
        else
            mpr("You feel slightly jumpy.");
        if (artefact)
            fake_rap = ARTP_CAUSE_TELEPORTATION;
        else
            ident = ID_KNOWN_TYPE;
        break;

    case AMU_RAGE:
        if (!scan_artefacts(ARTP_BERSERK))
        {
            mpr("You feel a brief urge to hack something to bits.");
            if (artefact)
                fake_rap = ARTP_BERSERK;
            else
                ident = ID_KNOWN_TYPE;
        }
        break;

    case AMU_FAITH:
        if (you.religion != GOD_NO_GOD)
        {
            mpr("You feel a surge of divine interest.", MSGCH_GOD);
            ident = ID_KNOWN_TYPE;
        }
        break;

    case AMU_THE_GOURMAND:
        // What's this supposed to achieve? (jpeg)
        you.duration[DUR_GOURMAND] = 0;
        break;

    case AMU_CONTROLLED_FLIGHT:
        if (you.duration[DUR_LEVITATION]
            && !extrinsic_amulet_effect(AMU_CONTROLLED_FLIGHT))
        {
            ident = ID_KNOWN_TYPE;
        }
        break;

    case AMU_GUARDIAN_SPIRIT:
        if (player_spirit_shield() < 2)
        {
            set_mp(0, false);
            mpr("You feel your power drawn to a protective spirit.");
            if (you.species == SP_DEEP_DWARF)
                mpr("Now linked to your health, your magic stops regenerating.");
            ident = ID_KNOWN_TYPE;
        }
        break;

    case RING_REGENERATION:
        // To be exact, bloodless vampires should get the id only after they
        // drink anything.  Not worth complicating the code, IMHO. [1KB]
        if (player_mutation_level(MUT_SLOW_HEALING) < 3)
            ident = ID_KNOWN_TYPE;
        break;

    case AMU_STASIS:
        int amount = you.duration[DUR_HASTE] + you.duration[DUR_SLOW];
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
            you.duration[DUR_HASTE] = 0;
            you.duration[DUR_SLOW] = 0;
            you.duration[DUR_TELEPORT] = 0;
        }
        break;

    }

    // Artefacts have completely different appearance than base types
    // so we don't allow them to make the base types known.
    if (artefact)
    {
        bool show_msgs = true;
        _equip_artefact_effect(item, &show_msgs);

        if (learn_pluses && (item.plus != 0 || item.plus2 != 0))
            set_ident_flags(item, ISFLAG_KNOW_PLUSES);

        if (fake_rap != ARTP_NUM_PROPERTIES)
            artefact_wpn_learn_prop(item, fake_rap);

        if (!item.props.exists("jewellery_tried")
            || !item.props["jewellery_tried"].get_bool())
        {
            item.props["jewellery_tried"].get_bool() = true;
        }
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

    mpr(item.name(DESC_INVENTORY_EQUIP).c_str());
}

static void _unequip_jewellery_effect(item_def &item, bool mesg)
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

    case RING_LEVITATION:
        if (you.duration[DUR_LEVITATION] && !you.permanent_levitation()
            && you.attribute[ATTR_LEV_UNCANCELLABLE])
        {
            you.duration[DUR_LEVITATION] = 1;
        }
        break;

    case RING_INVISIBILITY:
        if (you.duration[DUR_INVIS])
            you.duration[DUR_INVIS] = 1;
        break;

    case RING_MAGICAL_POWER:
        canned_msg(MSG_MANA_DECREASE);
        break;

    case AMU_THE_GOURMAND:
        you.duration[DUR_GOURMAND] = 0;
        break;

    case AMU_FAITH:
        _remove_amulet_of_faith(item);
        break;

    case AMU_GUARDIAN_SPIRIT:
        if (you.species == SP_DEEP_DWARF)
            mpr("Your magic begins regenerating once more.");
        break;
    }

    if (is_artefact(item))
        _unequip_artefact_effect(item, &mesg);

    // Must occur after ring is removed. -- bwr
    calc_mp();
}
