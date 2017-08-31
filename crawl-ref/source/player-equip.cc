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
#include "item-status-flag-type.h"
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
        mpr("당신의 몸에 변이를 유발하는 에너지가 축적되는 것이 느껴진다.");

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

/**
 * If player removes evocable invis and we need to clean things up, set
 * remaining Invis duration to 1 AUT and give the player contam equal to the
 * amount the player would receive if they waited the invis out.
 */
static void _unequip_invis()
{
    if (you.duration[DUR_INVIS] > 1
        && you.evokable_invis() == 0
        && !you.attribute[ATTR_INVIS_UNCANCELLABLE])
    {

        // scale up contam by 120% just to ensure that ending invis early is
        // worse than just resting it off.
        mpr("You absorb a burst of magical contamination as your invisibility "
             "abruptly ends!");
        const int invis_duration_left = you.duration[DUR_INVIS] * 120 / 100;
        const int remaining_contam = div_rand_round(
            invis_duration_left * INVIS_CONTAM_PER_TURN, BASELINE_DELAY
        );
        contaminate_player(remaining_contam, true);
        you.duration[DUR_INVIS] = 0;
    }
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

    if (proprt[ARTP_INVISIBLE] != 0)
        _unequip_invis();

    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();

    if (proprt[ARTP_CONTAM] && !meld)
    {
        mpr("변이를 유발하는 에너지가 당신의 몸으로 흘러들어온다!");
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
        mprf("%s은(는) 먼지처럼 바스라졌다!", item.name(DESC_PLAIN).c_str());
        dec_inv_item_quantity(item.link, 1);
    }
}

static void _equip_use_warning(const item_def& item)
{
    if (is_holy_item(item) && you_worship(GOD_YREDELEMNUL))
        mpr("당신은 절대 이런 신성한 물건을 사용해서는 안 된다.");
    else if (is_corpse_violating_item(item) && you_worship(GOD_FEDHAS))
        mpr("당신은 절대 이런 시체를 해치는 물건을 사용해서는 안 된다.");
    else if (is_evil_item(item) && is_good_god(you.religion))
        mpr("당신은 절대 이런 사악한 물건을 사용해서는 안 된다.");
    else if (is_unclean_item(item) && you_worship(GOD_ZIN))
        mpr("당신은 절대 이런 부정한 물건을 사용해서는 안 된다.");
    else if (is_chaotic_item(item) && you_worship(GOD_ZIN))
        mpr("당신은 절대 이런 혼돈의 물건을 사용해서는 안 된다.");
    else if (is_hasty_item(item) && you_worship(GOD_CHEIBRIADOS))
        mpr("당신은 절대 이런 신속한 물건을 사용해서는 안 된다.");
    else if (is_fiery_item(item) && you_worship(GOD_DITHMENOS))
        mpr("당신은 절대 이런 불타는 물건을 사용해서는 안 된다.");
    else if (is_channeling_item(item) && you_worship(GOD_PAKELLAS))
        mpr("당신은 절대 이런 식으로 마력을 충전해서는 안 된다.");
}

static void _wield_cursed(item_def& item, bool known_cursed, bool unmeld)
{
    if (!item.cursed() || unmeld)
        return;
    mprf("그것은 당신의 %s에 달라붙어 떨어지지 않는다!", you.hand_name(false).c_str());
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
                    mprf("%s이(가) 화염에 휩싸인다!", item_name.c_str());
                    break;

                case SPWPN_FREEZING:
                   mprf("%s이(가) %s", item_name.c_str(),
                        is_range_weapon(item) ?
                            "서리에 뒤덮인다." :
                            "차가운 파란색으로 빛난다!");
                    break;

                case SPWPN_HOLY_WRATH:
                    mprf("%s이(가) 신성한 광휘로 부드럽게 빛난다!",
                         item_name.c_str());
                    break;

                case SPWPN_ELECTROCUTION:
                    if (!silenced(you.pos()))
                    {
                        mprf(MSGCH_SOUND,
                             "전기의 폭음이 들려왔다.");
                    }
                    else
                        mpr("불꽃이 튀는 것이 보인다.");
                    break;

                case SPWPN_VENOM:
                    mprf("%s이(가) 독을 뚝뚝 흘리기 시작했다!", item_name.c_str());
                    break;

                case SPWPN_PROTECTION:
                    mprf("%s에 잠든 힘이 퍼져나온다!", item_name.c_str());
                    break;

                case SPWPN_DRAINING:
                    mpr("부정한 오라가 느껴진다.");
                    break;

                case SPWPN_SPEED:
                    mpr(you.hands_act("따끔", "!"));
                    break;

                case SPWPN_VAMPIRISM:
                    if (you.species == SP_VAMPIRE)
                        mpr("피에 굶주린 환희가 느껴진다.");
                    else if (you.undead_state() == US_ALIVE && !you_foodless())
                        mpr("무시무시한 허기가 느껴진다.");
                    else
                        mpr("공포가 사라진 것이 느껴진다.");
                    break;

                case SPWPN_PAIN:
                {
                    const string your_arm = you.arm_name(false);
                    if (you.skill(SK_NECROMANCY) == 0)
                        mpr("미숙한 기분이 든다.");
                    else if (you.skill(SK_NECROMANCY) <= 6)
                    {
                        mprf("강렬한 고통이 %s을(를) 타고 올라왔다!",
                             your_arm.c_str());
                    }
                    else
                    {
                        mprf("타는 듯한 고통이 %s을(를) 쑤시기 시작했다!",
                             your_arm.c_str());
                    }
                    break;
                }

                case SPWPN_CHAOS:
                    mprf("%s이(가) 순간적으로 반짝이는 무작위적 색깔의 "
                         "오라에 휩싸였다.", item_name.c_str());
                    break;

                case SPWPN_PENETRATION:
                {
                    // FIXME: make hands_act take a pre-verb adverb so we can
                    // use it here.
                    bool plural = true;
                    string hand = you.hand_name(true, &plural);

                    mprf("당신의 %s을 꽉 잡아 쥐기 전에 손이 그것을 잠깐 %s했다.",
                         hand.c_str(), conjugate_verb("통과", plural).c_str());
                    break;
                }

                case SPWPN_REAPING:
                    mprf("%s이(가) 순간적으로 변화무쌍한 그림자에 휩싸였다.",
                         item_name.c_str());
                    break;

                case SPWPN_ANTIMAGIC:
                    // Even if your maxmp is 0.
                    mpr("마력이 당신을 떠나간다.");
                    break;

                case SPWPN_DISTORTION:
                    mpr("순간적으로 당신 주위의 공간이 왜곡되었다!");
                    break;

                case SPWPN_ACID:
                    mprf("%s이(가) 부식성 점액을 흘리기 시작했다!", item_name.c_str());
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
                    mprf("%s의 불길이 사라졌다.", msg.c_str());
                break;

            case SPWPN_FREEZING:
            case SPWPN_HOLY_WRATH:
                if (showMsgs)
                    mprf("%s의 빛이 사라졌다.", msg.c_str());
                break;

            case SPWPN_ELECTROCUTION:
                if (showMsgs)
                    mprf("%s의 파열음이 멈추었다.", msg.c_str());
                break;

            case SPWPN_VENOM:
                if (showMsgs)
                    mprf("%s에서 독액이 흘러 나오는 것이 멈추었다.", msg.c_str());
                break;

            case SPWPN_PROTECTION:
                if (showMsgs)
                    mprf("%s이(가) 잠잠해졌다.", msg.c_str());
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
                        mpr("당신의 환희가 잦아든다.");
                    else
                        mpr("공포스러운 감각이 잦아드는 것이 느껴진다.");
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
                        simple_god_message("가 당신이 무기를 해제할 때 "
                                           "잔류한 공간 왜곡을 "
                                           "흡수했다.");
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
                mpr("마력이 당신에게로 돌아오는 것이 느껴진다.");
                break;

                // NOTE: When more are added here, *must* duplicate unwielding
                // effect in brand weapon scroll effect in read_scroll.

            case SPWPN_ACID:
                mprf("%s이(가) 더 이상 부식성 점액을 흘리지 않는다.", msg.c_str());
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
        mprf("%s 당신의 유령 무기가 사라졌다.",
             meld ? "당신의 무기가 녹아들자" : "당신이 장비를 놓자");
        end_spectral_weapon(spectral_weapon, false, true);
    }
}

static void _spirit_shield_message(bool unmeld)
{
    if (!unmeld && you.spirit_shield() < 2)
    {
        dec_mp(you.magic_points);
        mpr("당신의 힘이 수호 정령에게로 빠져나간다.");
        if (you.species == SP_DEEP_DWARF
            && !(have_passive(passive_t::no_mp_regen)
                 || player_under_penance(GOD_PAKELLAS)))
        {
            mpr("당신의 마력이 체력과 연결됨에 따라 더 이상 재생하지 않는다.");
        }
    }
    else if (!unmeld && you.get_mutation_level(MUT_MANA_SHIELD))
        mpr("무력한 정령의 존재가 느껴진다.");
    else // unmeld or already spirit-shielded
        mpr("정령이 당신을 지켜보는 것이 느껴진다.");
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
                mpr("빨라진 느낌이 든다.");
            break;

        case SPARM_FIRE_RESISTANCE:
            mpr("불에 대한 저항력이 느껴진다.");
            break;

        case SPARM_COLD_RESISTANCE:
            mpr("냉기에 대한 저항력이 느껴진다.");
            break;

        case SPARM_POISON_RESISTANCE:
            if (player_res_poison(false, false, false) < 3)
                mpr("독에 대한 저항력이 느껴진다.");
            break;

        case SPARM_SEE_INVISIBLE:
            mpr("시야 너머가 보인다.");
            autotoggle_autopickup(false);
            break;

        case SPARM_INVISIBILITY:
            if (!you.duration[DUR_INVIS])
                mpr("당신은 순간적으로 투명해졌다.");
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
            mpr("꽤 묵직해진 느낌이 든다.");
            break;

        case SPARM_FLYING:
            // If you weren't flying when you took off the boots, don't restart.
            if (you.attribute[ATTR_LAST_FLIGHT_STATUS]
                || you.has_mutation(MUT_NO_ARTIFICE))
            {
                if (you.airborne())
                {
                    you.attribute[ATTR_PERM_FLIGHT] = 1;
                    mpr("꽤 가벼워진 느낌이 든다.");
                }
                else
                {
                    you.attribute[ATTR_PERM_FLIGHT] = 1;
                    float_player();
                }
            }
            if (!unmeld && !you.has_mutation(MUT_NO_ARTIFICE))
            {
                if (you.has_mutation(MUT_NO_ARTIFICE))
                    mpr("비행을 멈추려면 장비를 해제하십시오.");
                else
                {
                mprf("(<w>%s</w>능력창에서 사용가능 : 비행을 %s하기)",
                     command_to_string(CMD_USE_ABILITY).c_str(),
                     you.attribute[ATTR_LAST_FLIGHT_STATUS]
                         ? "정지 또는 시작" : "시작 또는 정지");
                }
            }

            break;

        case SPARM_MAGIC_RESISTANCE:
            mpr("적대적 주술에 대한 저항력이 느껴진다.");
            break;

        case SPARM_PROTECTION:
            mpr("보호받는 느낌이 든다.");
            break;

        case SPARM_STEALTH:
            if (!you.get_mutation_level(MUT_NO_STEALTH))
                mpr("은밀해진 느낌이 든다.");
            break;

        case SPARM_RESISTANCE:
            mpr("온도의 양 극단에 대한 저항력이 느껴진다.");
            break;

        case SPARM_POSITIVE_ENERGY:
            mpr("음에너지로부터 더욱 보호받는 느낌이 든다.");
            break;

        case SPARM_ARCHMAGI:
            if (!you.skill(SK_SPELLCASTING))
                mpr("힘이 기묘하게 부족한 느낌이 든다.");
            else
                mpr("힘이 넘치는 느낌이 든다.");
            break;

        case SPARM_SPIRIT_SHIELD:
            _spirit_shield_message(unmeld);
            break;

        case SPARM_ARCHERY:
            mpr("당신의 조준이 더욱 안정적으로 느껴진다.");
            break;

        case SPARM_REPULSION:
            mpr("반발 역장에 둘러싸였다.");
            break;

        case SPARM_CLOUD_IMMUNE:
            mpr("구름의 효과에 면역이 된 것이 느껴진다.");
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
        mpr("윽, 시체처럼 차갑다.");
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
            mpr("더욱 달팽이처럼 느려진 것 같다.");
        break;

    case SPARM_FIRE_RESISTANCE:
        mpr("불에 대한 저항력이 약해진 것 같다.");
        break;

    case SPARM_COLD_RESISTANCE:
        mpr("냉기에 대한 저항력이 약해진 것 같다.");
        break;

    case SPARM_POISON_RESISTANCE:
        if (player_res_poison() <= 0)
            mpr("더 이상 독에 대한 저항력이 느껴지지 않는다.");
        break;

    case SPARM_SEE_INVISIBLE:
        if (!you.can_see_invisible())
        {
            mpr("시야 너머가 닫힌다.");
            _mark_unseen_monsters();
        }
        break;

    case SPARM_INVISIBILITY:
        _unequip_invis();
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
        mpr("그것이 당신의 발걸음을 더욱 가볍게 만들었다.");
        break;

    case SPARM_FLYING:
        // Save current flight status so we can restore it on reequip
        you.attribute[ATTR_LAST_FLIGHT_STATUS] =
            you.attribute[ATTR_PERM_FLIGHT];

        lose_permafly_source();
        break;

    case SPARM_MAGIC_RESISTANCE:
        mpr("적대적 주술에 취약해진 느낌이 든다.");
        break;

    case SPARM_PROTECTION:
        mpr("덜 보호받는 느낌이 든다.");
        break;

    case SPARM_STEALTH:
        if (!you.get_mutation_level(MUT_NO_STEALTH))
            mpr("덜 은밀해진 느낌이 든다.");
        break;

    case SPARM_RESISTANCE:
        mpr("전체적으로 뜨겁고 차가워졌다.");
        break;

    case SPARM_POSITIVE_ENERGY:
        mpr("음에너지로부터 덜 보호받는 느낌이 든다.");
        break;

    case SPARM_ARCHMAGI:
        mpr("기묘하게 무기력한 느낌이 든다.");
        break;

    case SPARM_SPIRIT_SHIELD:
        if (!you.spirit_shield())
        {
            mpr("묘하게 외로운 느낌이 든다.");
            if (you.species == SP_DEEP_DWARF)
                mpr("당신의 마력이 더욱 빠르게 재생한다.");
        }
        break;

    case SPARM_ARCHERY:
        mpr("당신의 조준은 더 이상 안정적이지 않다.");
        break;

    case SPARM_REPULSION:
        mpr("반발 역장의 안개가 사라졌다.");
        break;

    case SPARM_CLOUD_IMMUNE:
        mpr("구름에 취약해진 느낌이 든다.");
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
        simple_god_message("은(는) 당신에 대해 무관심해진 것 같다.");

        const int piety_loss = div_rand_round(you.piety, 3);
        // Piety penalty for removing the Amulet of Faith.
        if (you.piety - piety_loss > 10)
        {
            mprf(MSGCH_GOD, "당신은 덜 경건한 기분이 든다.");
            dprf("%s: piety drain: %d",
                 item.name(DESC_PLAIN).c_str(), piety_loss);
            lose_piety(piety_loss);
        }
    }
}

static void _remove_amulet_of_harm()
{
    if (you.undead_state() == US_ALIVE)
        mpr("목걸이를 벗자 당신의 생명력이 목걸이로 빨려나갔다!");
    else
        mpr("목걸이를 벗자 당신을 움직이는 힘이 목걸이로 빨려나갔다!");

    drain_player(150, false, true);
}

static void _equip_amulet_of_regeneration()
{
    if (you.get_mutation_level(MUT_NO_REGENERATION) > 0)
        mpr("이 목걸이는 차갑고 기능이 멈춘 것 같다.");
    else if (you.hp == you.hp_max)
    {
        you.props[REGEN_AMULET_ACTIVE] = 1;
        mpr("목걸이가 당신의 온전한 몸과 동화되며 고동치기 시작했다.");
    }
    else
    {
        mpr("목걸이가 당신의 상처입은 몸과 동화되지 못하고 있다");
        you.props[REGEN_AMULET_ACTIVE] = 0;
    }
}

static void _equip_amulet_of_mana_regeneration()
{
    if (!player_regenerates_mp())
        mpr("이 목걸이는 차갑고 기능이 멈춘 것 같다.");
    else if (you.magic_points == you.max_magic_points)
    {
        you.props[MANA_REGEN_AMULET_ACTIVE] = 1;
        mpr("목걸이가 당신의 활발한 육체와 동화되며 윙윙거리기 시작했다.");
    }
    else
    {
        mpr("목걸이가 당신의 지친 몸과 동화되지 못하는 것이 느껴진다");
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
        mpr("불과 친숙해진 느낌이 든다.");
        break;

    case RING_ICE:
        mpr("냉기와 친숙해진 느낌이 든다.");
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
            mpr("작고 조용한 도약이 당신에게로 쏟아진다.");
        else
            // keep in sync with player_teleport
            mprf("약간의 불안정함을 %s느꼈다.",
                 (player_teleport(false) > 8) ? " " : "");
        break;

    case AMU_FAITH:
        if (you.species == SP_DEMIGOD)
            mpr("자신감이 솟구친다.");
        else if (you_worship(GOD_RU) && you.piety >= piety_breakpoint(5))
        {
            simple_god_message("가 말했다: 네 고행에 대한 헌신은 "
                               "그런 장식품 따위가 없어도 알고 있다.");
        }
        else if (you_worship(GOD_GOZAG))
            simple_god_message("는 오직 금화만을 원한다!");
        else
        {
            mprf(MSGCH_GOD, "신의 %s관심이 느껴진다.",
                            you_worship(GOD_NO_GOD) ? "낯선 " : "");
        }

        break;

    case AMU_THE_GOURMAND:
        // What's this supposed to achieve? (jpeg)
        you.duration[DUR_GOURMAND] = 0;
        mpr("던전의 음식에 대한 열망이 피어오른다.");
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
        mprf("헉, 이 %s는 끔찍하게 차갑다.",
             jewellery_is_amulet(item)? "목걸이" : "반지");
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
            mpr("당신의 마력이 더 빠르게 재생하기 시작했다.");
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
