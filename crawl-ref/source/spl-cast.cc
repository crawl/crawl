/*
 *  File:       spl-cast.cc
 *  Summary:    Spell casting and miscast functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <sstream>
#include <iomanip>

#include "spl-cast.h"

#include "externs.h"

#include "beam.h"
#include "cloud.h"
#include "describe.h"
#include "effects.h"
#include "fight.h"
#include "food.h"
#include "format.h"
#include "initfile.h"
#include "invent.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "Kills.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mstuff2.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "quiver.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "transfor.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

#ifdef DOS
#include <conio.h>
#endif

// This determines how likely it is that more powerful wild magic
// effects will occur.  Set to 100 for the old probabilities (although
// the individual effects have been made much nastier since then).
#define WILD_MAGIC_NASTINESS 150

static bool _surge_identify_boosters(spell_type spell)
{
    const unsigned int typeflags = get_spell_disciplines(spell);
    if ( (typeflags & SPTYP_FIRE) || (typeflags & SPTYP_ICE) )
    {
        // Must not be wielding an unIDed staff.
        // Note that robes of the Archmagi identify on wearing,
        // so that's less of an issue.
        const item_def* wpn = player_weapon();
        if ( wpn == NULL ||
             wpn->base_type != OBJ_STAVES ||
             item_ident(*wpn, ISFLAG_KNOW_PROPERTIES) )
        {
            int num_unknown = 0;
            for ( int i = EQ_LEFT_RING; i <= EQ_RIGHT_RING; ++i )
            {
                if (you.equip[i] != -1 &&
                    !item_ident(you.inv[you.equip[i]], ISFLAG_KNOW_PROPERTIES))
                {
                    ++num_unknown;
                }
            }

            // We can also identify cases with two unknown rings, both
            // of fire (or both of ice)...let's skip it.
            if ( num_unknown == 1 )
            {
                for ( int i = EQ_LEFT_RING; i <= EQ_RIGHT_RING; ++i )
                    if ( you.equip[i] != -1 )
                    {
                        item_def& ring = you.inv[you.equip[i]];
                        if (!item_ident(ring, ISFLAG_KNOW_PROPERTIES)
                            && (ring.sub_type == RING_FIRE
                                || ring.sub_type == RING_ICE))
                        {
                            set_ident_type( ring.base_type, ring.sub_type,
                                            ID_KNOWN_TYPE );
                            set_ident_flags(ring, ISFLAG_KNOW_PROPERTIES);
                            mprf("You are wearing: %s",
                                 ring.name(DESC_INVENTORY_EQUIP).c_str());
                        }
                    }

                return (true);
            }
        }
    }
    return (false);
}

static void _surge_power(spell_type spell)
{
    int enhanced = 0;

    _surge_identify_boosters(spell);
    enhanced += spell_enhancement(get_spell_disciplines(spell));

    if (enhanced)               // one way or the other {dlb}
    {
        mprf("You feel a%s %s",
             (enhanced < -2)  ? "n extraordinarily" :
             (enhanced == -2) ? "n extremely" :
             (enhanced == 2)  ? " strong" :
             (enhanced > 2)   ? " huge"
                              : "",
             (enhanced < 0) ? "numb sensation."
                            : "surge of power!");
    }
}

static std::string _spell_base_description(spell_type spell)
{
    std::ostringstream desc;

    desc << std::left;

    // spell name
    desc << std::setw(30) << spell_title(spell);

    // spell schools
    bool already = false;
    for ( int i = 0; i <= SPTYP_LAST_EXPONENT; ++i)
    {
        if (spell_typematch(spell, (1<<i)))
        {
            if (already)
                desc << '/';
            desc << spelltype_name(1 << i);
            already = true;
        }
    }

    const int so_far = desc.str().length();
    if ( so_far < 60 )
        desc << std::string(60 - so_far, ' ');

    // spell fail rate, level
    desc << std::setw(12) << failure_rate_to_string(spell_fail(spell))
         << spell_difficulty(spell);

    return desc.str();
}

static std::string _spell_extra_description(spell_type spell)
{
    std::ostringstream desc;

    desc << std::left;

    // spell name
    desc << std::setw(30) << spell_title(spell);

    // spell power, spell range, hunger level, level
    const std::string rangestring = spell_range_string(spell);

    desc << std::setw(14) << spell_power_string(spell)
         << std::setw(16 + tagged_string_tag_length(rangestring)) << rangestring
         << std::setw(12) << spell_hunger_string(spell)
         << spell_difficulty(spell);

    return desc.str();
}

int list_spells(bool toggle_with_I)
{
    ToggleableMenu spell_menu(MF_SINGLESELECT | MF_ANYPRINTABLE |
        MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING);
#ifdef USE_TILE
    {
        // [enne] - Hack.  Make title an item so that it's aligned.
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry(
                " Your Spells                      Type          "
                "                Success   Level",
                " Your Spells                      Power         "
                "Range           Hunger    Level",
                MEL_ITEM);
        me->colour = BLUE;
        spell_menu.add_entry(me);
    }
#else
    spell_menu.set_title(
        new ToggleableMenuEntry(
            " Your Spells                      Type          "
            "                Success   Level",
            " Your Spells                      Power         "
            "Range           Hunger    Level",
            MEL_TITLE));
#endif
    spell_menu.set_highlighter(NULL);
    spell_menu.add_toggle_key('!');
    if (toggle_with_I)
    {
        spell_menu.set_more(
            formatted_string("Press '!' or 'I' to toggle spell view."));
        spell_menu.add_toggle_key('I');
    }
    else
    {
        spell_menu.set_more(
            formatted_string("Press '!' to toggle spell view."));
    }

    spell_menu.set_tag("spell");

    for (int i = 0; i < 52; ++i)
    {
        const char letter = index_to_letter(i);
        const spell_type spell = get_spell_by_letter(letter);
        if (spell != SPELL_NO_SPELL)
        {
            ToggleableMenuEntry* me =
                new ToggleableMenuEntry(_spell_base_description(spell),
                                        _spell_extra_description(spell),
                                        MEL_ITEM, 1, letter);
            spell_menu.add_entry(me);
        }
    }

    std::vector<MenuEntry*> sel = spell_menu.show();
    redraw_screen();
    if ( sel.empty() )
    {
        return 0;
    }
    else
    {
        ASSERT(sel.size() == 1);
        ASSERT(sel[0]->hotkeys.size() == 1);
        return sel[0]->hotkeys[0];
    }
}

static int _apply_spellcasting_success_boosts(spell_type spell, int chance)
{
    int wizardry = player_mag_abil(false);
    int fail_reduce = 100;
    int wiz_factor = 87;

    if (you.religion == GOD_VEHUMET
        && !player_under_penance() && you.piety >= 50
        && (spell_typematch(spell, SPTYP_CONJURATION)
            || spell_typematch(spell, SPTYP_SUMMONING)))
    {
        // [dshaligram] Fail rate multiplier used to be .5, scaled
        // back to 67%.
        fail_reduce = fail_reduce * 67 / 100;
    }

    // [dshaligram] Apply wizardry factor here, rather than mixed into the
    // pre-scaling spell power.
    while (wizardry-- > 0)
    {
        fail_reduce  = fail_reduce * wiz_factor / 100;
        wiz_factor  += (100 - wiz_factor) / 3;
    }

    // Draconians get a boost to dragon-form.
    if (spell == SPELL_DRAGON_FORM && player_genus(GENPC_DRACONIAN))
        fail_reduce = fail_reduce * 70 / 100;

    // Hard cap on fail rate reduction.
    if (fail_reduce < 50)
        fail_reduce = 50;

    return (chance * fail_reduce / 100);
}

int spell_fail(spell_type spell)
{
    int chance = 60;
    int chance2 = 0, armour = 0;

    chance -= 6 * calc_spell_power( spell, false, true );
    chance -= (you.intel * 2);

    //chance -= (you.intel - 10) * abs(you.intel - 10);
    //chance += spell_difficulty(spell) * spell_difficulty(spell) * 3; //spell_difficulty(spell);

    if (you.equip[EQ_BODY_ARMOUR] != -1)
    {

        int ev_penalty = abs(property( you.inv[you.equip[EQ_BODY_ARMOUR]],
                                       PARM_EVASION ));

        // The minus 15 is to make the -1 light armours not so bad
        armour += (20 * ev_penalty) - 15;

        //jmf: armour skill now reduces failure due to armour
        //bwr: this was far too good, an armour skill of 5 was
        //     completely negating plate mail.  Plate mail should
        //     hardly be completely negated, it should still be
        //     an important consideration for even high level characters.
        //     Truth is, even a much worse penalty than the above can
        //     easily be overcome by gaining spell skills... and a lot
        //     faster than any reasonable rate of bonus here.
        int lim_str = (you.strength > 30) ? 30 :
                      (you.strength < 10) ? 10 : you.strength;

        armour -= ((you.skills[SK_ARMOUR] * lim_str) / 15);

        int race_arm = get_equip_race( you.inv[you.equip[EQ_BODY_ARMOUR]] );
        int racial_type = 0;

        if (player_genus(GENPC_DWARVEN))
            racial_type = ISFLAG_DWARVEN;
        else if (player_genus(GENPC_ELVEN))
            racial_type = ISFLAG_ELVEN;
        else if (you.species == SP_HILL_ORC)
            racial_type = ISFLAG_ORCISH;

        // Elven armour gives everyone some benefit to spellcasting,
        // Dwarven armour hinders everyone.
        switch (race_arm)
        {
        case ISFLAG_ELVEN:
            armour -= 20;
            break;
        case ISFLAG_DWARVEN:
            armour += 10;
            break;
        default:
            break;
        }

        // Armour of the same racial type reduces penalty.
        if (racial_type && race_arm == racial_type)
            armour -= 10;

        if (armour > 0)
            chance += armour;
    }

    if (you.weapon() && you.weapon()->base_type == OBJ_WEAPONS)
    {
        int wpn_penalty = (3 * (property(*you.weapon(), PWPN_SPEED) - 12))/2;

        if (wpn_penalty > 0)
            chance += wpn_penalty;
    }

    if (you.shield())
    {
        switch (you.shield()->sub_type)
        {
        case ARM_BUCKLER:
            chance += 5;
            break;

        case ARM_SHIELD:
            chance += 15;
            break;

        case ARM_LARGE_SHIELD:
            // *BCR* Large chars now get a lower penalty for large shields
            if (you.species == SP_OGRE || you.species == SP_TROLL
                || player_genus(GENPC_DRACONIAN))
            {
                chance += 20;
            }
            else
                chance += 30;
            break;
        }
    }

    switch (spell_difficulty(spell))
    {
    case  1: chance +=   3; break;
    case  2: chance +=  15; break;
    case  3: chance +=  35; break;
    case  4: chance +=  70; break;
    case  5: chance += 100; break;
    case  6: chance += 150; break;
    case  7: chance += 200; break;
    case  8: chance += 260; break;
    case  9: chance += 330; break;
    case 10: chance += 420; break;
    case 11: chance += 500; break;
    case 12: chance += 600; break;
    default: chance += 750; break;
    }

    chance2 = chance;

    const int chance_breaks[][2] = {
        {45, 45}, {42, 43}, {38, 41}, {35, 40}, {32, 38}, {28, 36},
        {22, 34}, {16, 32}, {10, 30}, {2, 28}, {-7, 26}, {-12, 24},
        {-18, 22}, {-24, 20}, {-30, 18}, {-38, 16}, {-46, 14},
        {-60, 12}, {-80, 10}, {-100, 8}, {-120, 6}, {-140, 4},
        {-160, 2}, {-180, 0}
    };

    for ( unsigned int i = 0; i < ARRAYSZ(chance_breaks); ++i )
        if ( chance < chance_breaks[i][0] )
            chance2 = chance_breaks[i][1];

    if (you.duration[DUR_TRANSFORMATION] > 0)
    {
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_BLADE_HANDS:
            chance2 += 20;
            break;

        case TRAN_SPIDER:
        case TRAN_BAT:
            chance2 += 10;
            break;
        }
    }

    // Apply the effects of Vehumet and items of wizardry.
    chance2 = _apply_spellcasting_success_boosts(spell, chance2);

    if (chance2 > 100)
        chance2 = 100;

    return (chance2);
}                               // end spell_fail()


int calc_spell_power(spell_type spell, bool apply_intel, bool fail_rate_check)
{
    // When checking failure rates, wizardry is handled after the various
    // stepping calulations.
    int power = (you.skills[SK_SPELLCASTING] / 2)
                 + (fail_rate_check? 0 : player_mag_abil(false));
    int enhanced = 0;

    unsigned int disciplines = get_spell_disciplines( spell );

    //jmf: evil evil evil -- exclude HOLY bit
    disciplines &= (~SPTYP_HOLY);

    int skillcount = count_bits( disciplines );
    if (skillcount)
    {
        for (int ndx = 0; ndx <= SPTYP_LAST_EXPONENT; ndx++)
        {
            unsigned int bit = (1 << ndx);
            if (disciplines & bit)
                power += (you.skills[spell_type2skill(bit)] * 2) / skillcount;
        }
    }

    if (apply_intel)
        power = (power * you.intel) / 10;

    // [dshaligram] Enhancers don't affect fail rates any more, only spell
    // power. Note that this does not affect Vehumet's boost in castability.
    if (!fail_rate_check)
        enhanced = spell_enhancement( disciplines );

    if (enhanced > 0)
    {
        for (int i = 0; i < enhanced; i++)
        {
            power *= 15;
            power /= 10;
        }
    }
    else if (enhanced < 0)
    {
        for (int i = enhanced; i < 0; i++)
            power /= 2;
    }

    power = stepdown_value( power, 50, 50, 150, 200 );

    const int cap = spell_power_cap(spell);
    if (cap > 0)
        power = std::min(power, cap);

    return (power);
}


int spell_enhancement( unsigned int typeflags )
{
    int enhanced = 0;

    if (typeflags & SPTYP_CONJURATION)
        enhanced += player_spec_conj();

    if (typeflags & SPTYP_ENCHANTMENT)
        enhanced += player_spec_ench();

    if (typeflags & SPTYP_SUMMONING)
        enhanced += player_spec_summ();

    if (typeflags & SPTYP_POISON)
        enhanced += player_spec_poison();

    if (typeflags & SPTYP_NECROMANCY)
        enhanced += player_spec_death() - player_spec_holy();

    if (typeflags & SPTYP_FIRE)
        enhanced += player_spec_fire() - player_spec_cold();

    if (typeflags & SPTYP_ICE)
        enhanced += player_spec_cold() - player_spec_fire();

    if (typeflags & SPTYP_EARTH)
        enhanced += player_spec_earth() - player_spec_air();

    if (typeflags & SPTYP_AIR)
        enhanced += player_spec_air() - player_spec_earth();

    if (you.special_wield == SPWLD_SHADOW)
        enhanced -= 2;

    // These are used in an exponential way, so we'll limit them a bit. -- bwr
    if (enhanced > 3)
        enhanced = 3;
    else if (enhanced < -3)
        enhanced = -3;

    return (enhanced);
}                               // end spell_enhancement()

void inspect_spells()
{
    if (!you.spell_no)
    {
        mpr("You don't know any spells.");
        return;
    }

    // Maybe we should honour auto_list here, but if you want the
    // description, you probably want the listing, too.
    int keyin = list_spells();
    if ( isalpha(keyin) )
    {
        describe_spell(get_spell_by_letter(keyin));
        redraw_screen();
    }
}

// returns false if spell failed, and true otherwise
bool cast_a_spell()
{
    if (!you.spell_no)
    {
        mpr("You don't know any spells.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    if (silenced(you.pos()))
    {
        mpr("You cannot cast spells when silenced!");
        crawl_state.zero_turns_taken();
        more();
        return (false);
    }

    int keyin = 0;              // silence stupid compilers

    while (true)
    {
        mpr( "Cast which spell ([?*] list)? ", MSGCH_PROMPT );

        keyin = get_ch();

        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells();
            if (!keyin)
                keyin = ESCAPE;

            redraw_screen();

            if ( isalpha(keyin) || keyin == ESCAPE )
                break;
            else
                mesclr();
        }
        else
        {
            break;
        }
    }

    if (keyin == ESCAPE)
    {
        canned_msg( MSG_OK );
        return (false);
    }

    if (!isalpha(keyin))
    {
        mpr("You don't know that spell.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    const spell_type spell = get_spell_by_letter( keyin );

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    if (spell_mana( spell ) > you.magic_points)
    {
        mpr("You don't have enough magic to cast that spell.");
        return (false);
    }

    if (you.is_undead != US_UNDEAD && you.species != SP_VAMPIRE
        && (you.hunger_state == HS_STARVING
            || you.hunger <= spell_hunger( spell )))
    {
        mpr("You don't have the energy to cast that spell.");
        return (false);
    }

    const bool staff_energy = player_energy();
    if (you.confused())
        random_uselessness();
    else
    {
        const spret_type cast_result = your_spells( spell );
        if (cast_result == SPRET_ABORT)
        {
            crawl_state.zero_turns_taken();
            return (false);
        }

        exercise_spell( spell, true, cast_result == SPRET_SUCCESS );
        did_god_conduct( DID_SPELL_CASTING, 1 + random2(5) );
    }

    dec_mp( spell_mana(spell) );

    if (!staff_energy && you.is_undead != US_UNDEAD)
    {
        const int spellh = calc_hunger( spell_hunger(spell) );
        if (spellh > 0)
        {
            make_hungry(spellh, true);
            learned_something_new(TUT_SPELL_HUNGER);
        }
    }

    you.turn_is_over = true;
    alert_nearby_monsters();

    return (true);
}                               // end cast_a_spell()

// "Utility" spells for the sake of simplicity are currently ones with
// enchantments, translocations, or divinations.
static bool _spell_is_utility_spell(spell_type spell_id)
{
    return (spell_typematch(spell_id,
                SPTYP_ENCHANTMENT | SPTYP_TRANSLOCATION | SPTYP_DIVINATION));
}

static bool _spell_is_unholy(spell_type spell_id)
{
    return (testbits(get_spell_flags(spell_id), SPFLAG_UNHOLY));
}

bool maybe_identify_staff(item_def &item, spell_type spell)
{
    if (item_type_known(item))
        return (true);

    int relevant_skill = 0;
    const bool chance = (spell != SPELL_NO_SPELL);

    switch (item.sub_type)
    {
        case STAFF_ENERGY:
            if (!chance) // The staff of energy only autoIDs by chance.
                return (false);
            // intentional fall-through
        case STAFF_WIZARDRY:
            relevant_skill = you.skills[SK_SPELLCASTING];
            break;

        case STAFF_FIRE:
            if (!chance || spell_typematch(spell, SPTYP_FIRE))
                relevant_skill = you.skills[SK_FIRE_MAGIC];
            else if (spell_typematch(spell, SPTYP_ICE))
                relevant_skill = you.skills[SK_ICE_MAGIC];
            break;

        case STAFF_COLD:
            if (!chance || spell_typematch(spell, SPTYP_ICE))
                relevant_skill = you.skills[SK_ICE_MAGIC];
            else if (spell_typematch(spell, SPTYP_FIRE))
                relevant_skill = you.skills[SK_FIRE_MAGIC];
            break;

        case STAFF_AIR:
            if (!chance || spell_typematch(spell, SPTYP_AIR))
                relevant_skill = you.skills[SK_AIR_MAGIC];
            else if (spell_typematch(spell, SPTYP_EARTH))
                relevant_skill = you.skills[SK_EARTH_MAGIC];
            break;

        case STAFF_EARTH:
            if (!chance || spell_typematch(spell, SPTYP_EARTH))
                relevant_skill = you.skills[SK_EARTH_MAGIC];
            else if (spell_typematch(spell, SPTYP_AIR))
                relevant_skill = you.skills[SK_AIR_MAGIC];
            break;

        case STAFF_POISON:
            if (!chance || spell_typematch(spell, SPTYP_POISON))
                relevant_skill = you.skills[SK_POISON_MAGIC];
            break;

        case STAFF_DEATH:
            if (!chance || spell_typematch(spell, SPTYP_NECROMANCY))
                relevant_skill = you.skills[SK_NECROMANCY];
            break;

        case STAFF_CONJURATION:
            if (!chance || spell_typematch(spell, SPTYP_CONJURATION))
                relevant_skill = you.skills[SK_CONJURATIONS];
            break;

        case STAFF_ENCHANTMENT:
            if (!chance || spell_typematch(spell, SPTYP_ENCHANTMENT))
                relevant_skill = you.skills[SK_ENCHANTMENTS];
            break;

        case STAFF_SUMMONING:
            if (!chance || spell_typematch(spell, SPTYP_SUMMONING))
                relevant_skill = you.skills[SK_SUMMONINGS];
            break;
    }

    bool id_staff = false;

    if (chance)
    {
        if (you.skills[SK_SPELLCASTING] > relevant_skill)
            relevant_skill = you.skills[SK_SPELLCASTING];

        if (x_chance_in_y(relevant_skill, 100))
            id_staff = true;
    }
    else if (relevant_skill >= 4)
        id_staff = true;

    if (id_staff)
    {
        item_def& wpn = *you.weapon();
        set_ident_type(wpn, ID_KNOWN_TYPE);
        set_ident_flags( wpn, ISFLAG_IDENT_MASK);
        mprf("You are wielding %s.", wpn.name(DESC_NOCAP_A).c_str());
        more();

        you.wield_change = true;
    }
    return (id_staff);
}

static void _spellcasting_side_effects(spell_type spell, bool idonly = false)
{
    if (you.weapon() && item_is_staff(*you.weapon()))
        maybe_identify_staff(*you.weapon(), spell);

    if (idonly)
        return;

    if (!_spell_is_utility_spell(spell))
        did_god_conduct(DID_SPELL_NONUTILITY, 10 + spell_difficulty(spell));

    if (spell_typematch(spell, SPTYP_HOLY))
        did_god_conduct(DID_HOLY, 10 + spell_difficulty(spell));

    // Self-banishment gets a special exemption - you're there to spread
    // light.
    if (_spell_is_unholy(spell) && !you.banished)
        did_god_conduct(DID_UNHOLY, 10 + spell_difficulty(spell));

    // Linley says: Condensation Shield needs some disadvantages to keep
    // it from being a no-brainer... this isn't much, but its a start -- bwr
    if (spell_typematch(spell, SPTYP_FIRE))
        expose_player_to_element(BEAM_FIRE, 0);

    if (spell_typematch(spell, SPTYP_NECROMANCY))
    {
        did_god_conduct(DID_NECROMANCY, 10 + spell_difficulty(spell));

        if (spell == SPELL_NECROMUTATION && is_good_god(you.religion))
            excommunication();
    }

    alert_nearby_monsters();
}

static bool _vampire_cannot_cast(spell_type spell)
{
    if (you.species != SP_VAMPIRE)
        return (false);

    if (you.hunger_state > HS_SATIATED)
        return (false);

    // Satiated or less
    switch (spell)
    {
    case SPELL_AIR_WALK:
    case SPELL_ALTER_SELF:
    case SPELL_BERSERKER_RAGE:
    case SPELL_BLADE_HANDS:
    case SPELL_CURE_POISON_II:
    case SPELL_DRAGON_FORM:
    case SPELL_ICE_FORM:
    case SPELL_RESIST_POISON:
    case SPELL_SPIDER_FORM:
    case SPELL_STATUE_FORM:
    case SPELL_STONESKIN:
    case SPELL_TAME_BEASTS:
        return (true);
    default:
        return (false);
    }
}

static bool _spell_is_uncastable(spell_type spell)
{
    if (player_is_unholy() && spell_typematch(spell, SPTYP_HOLY))
    {
        mpr("You can't use this type of magic!");
        return (true);
    }

    // Normally undead can't memorise these spells, so this check is
    // to catch those in Lich form.  As such, we allow the Lich form
    // to be extended here. -- bwr
    if (spell != SPELL_NECROMUTATION
        && undead_cannot_memorise( spell, you.is_undead ))
    {
        mpr( "You cannot cast that spell in your current form!" );
        return (true);
    }

    if (spell == SPELL_SYMBOL_OF_TORMENT && player_res_torment())
    {
        mpr("To torment others, one must first know what torment means. ");
        return (true);
    }

    if (_vampire_cannot_cast( spell ))
    {
        mpr("Your current blood level is not sufficient to cast that spell.");
        return (true);
    }

    return (false);
}

#ifdef WIZARD
static void _try_monster_cast(spell_type spell, int powc,
                              dist &spd, bolt &beam)
{
    if (mgrd(you.pos()) != NON_MONSTER)
    {
        mpr("Couldn't try casting monster spell because you're "
            "on top of a monster.");
        return;
    }

    int midx;

    for (midx = 0; midx < MAX_MONSTERS; midx++)
        if (menv[midx].type == -1)
            break;

    if (midx == MAX_MONSTERS)
    {
        mpr("Couldn't try casting monster spell because there is "
            "no empty monster slot.");
        return;
    }

    mpr("Invalid player spell, attempting to cast it as monster spell.");

    monsters* mon = &menv[midx];

    mon->mname      = "Dummy Monster";
    mon->type       = MONS_HUMAN;
    mon->behaviour  = BEH_SEEK;
    mon->attitude   = ATT_FRIENDLY;
    mon->flags      = (MF_CREATED_FRIENDLY | MF_JUST_SUMMONED | MF_SEEN
                       | MF_WAS_IN_VIEW | MF_HARD_RESET);
    mon->hit_points = you.hp;
    mon->hit_dice   = you.experience_level;
    mon->pos()      = you.pos();
    mon->target     = spd.target;

    if (!spd.isTarget)
        mon->foe = MHITNOT;
    else if (mgrd(spd.target) == NON_MONSTER)
    {
        if (spd.isMe)
            mon->foe = MHITYOU;
        else
            mon->foe = MHITNOT;
    }
    else
        mon->foe = mgrd(spd.target);

    mgrd(you.pos()) = midx;

    mons_cast(mon, beam, spell);

    mon->reset();
}
#endif // WIZARD

beam_type _spell_to_beam_type(spell_type spell)
{
    switch (spell)
    {
    case SPELL_BURN: return BEAM_FIRE;
    case SPELL_FREEZE: return BEAM_COLD;
    case SPELL_CRUSH: return BEAM_MISSILE;
    case SPELL_ARC: return BEAM_ELECTRICITY;
    default: break;
    }
    return BEAM_NONE;
}

int _setup_evaporate_cast()
{
    int rc = prompt_invent_item("Throw which potion?", MT_INVLIST, OBJ_POTIONS);

    if (prompt_failed(rc))
    {
        rc = -1;
    }
    else if (you.inv[rc].base_type != OBJ_POTIONS)
    {
        mpr("This spell works only on potions!");
        rc = -1;
    }
    else
    {
        mprf(MSGCH_PROMPT, "Where do you want to aim %s?",
             you.inv[rc].name(DESC_NOCAP_YOUR).c_str());
    }
    return rc;
}

static bool _can_cast_detect()
{
    if (!testbits(env.level_flags, LFLAG_NO_MAGIC_MAP))
        return true;

    mpr("You feel momentarily disoriented.");
    return false;
}

// Returns SPRET_SUCCESS if spell is successfully cast for purposes of
// exercising, SPRET_FAIL otherwise, or SPRET_ABORT if the player canceled
// the casting.
// Not all of these are actually real spells; invocations, decks, rods or misc.
// effects might also land us here.
// Others are currently unused or unimplemented.
spret_type your_spells(spell_type spell, int powc, bool allow_fail)
{
    ASSERT(!crawl_state.arena);

    const bool wiz_cast = (crawl_state.prev_cmd == CMD_WIZARD && !allow_fail);

    dist spd;
    bolt beam;

    // [dshaligram] Any action that depends on the spellcasting attempt to have
    // succeeded must be performed after the switch().
    if (!wiz_cast && _spell_is_uncastable(spell))
        return (SPRET_ABORT);

    const unsigned int flags = get_spell_flags(spell);

    ASSERT(wiz_cast || !(flags & SPFLAG_TESTING));
    if ((flags & SPFLAG_TESTING) && !wiz_cast)
    {
        mprf(MSGCH_ERROR, "Spell %s is a testing spell, but you didn't use "
                          "the &Z wizard command; please file a bug report.",
             spell_title(spell));
        return (SPRET_ABORT);
    }

    int potion = -1;

    // XXX: This handles only some of the cases where spells need targeting...
    // there are others that do their own that will be missed by this
    // (and thus will not properly ESC without cost because of it).
    // Hopefully, those will eventually be fixed. -- bwr
    if ((flags & SPFLAG_TARGETING_MASK) && spell != SPELL_PORTAL_PROJECTILE)
    {
        targ_mode_type targ =
            (testbits(flags, SPFLAG_HELPFUL) ? TARG_FRIEND : TARG_ENEMY);

        if (testbits(flags, SPFLAG_NEUTRAL))
            targ = TARG_ANY;

        targeting_type dir  =
            (testbits( flags, SPFLAG_TARGET ) ? DIR_TARGET :
             testbits( flags, SPFLAG_GRID )   ? DIR_TARGET :
             testbits( flags, SPFLAG_DIR )    ? DIR_DIR
                                              : DIR_NONE);

        const char *prompt = get_spell_target_prompt(spell);
        if (spell == SPELL_EVAPORATE)
        {
            potion = _setup_evaporate_cast();
            if (potion == -1)
                return (SPRET_ABORT);
        }
        else if (dir == DIR_DIR)
            mpr(prompt ? prompt : "Which direction?", MSGCH_PROMPT);

        const bool needs_path = (!testbits(flags, SPFLAG_GRID)
                                 && !testbits(flags, SPFLAG_TARGET));

        const bool dont_cancel_me = testbits(flags, SPFLAG_AREA);


        // FIXME: Code duplication (see similar line below).
        int range_power = (powc == 0 ? calc_spell_power(spell, true) : powc);

        const int range = spell_range(spell, range_power, false);

        if (!spell_direction(spd, beam, dir, targ, range,
                             needs_path, true, dont_cancel_me, prompt,
                             testbits(flags, SPFLAG_NOT_SELF)))
        {
            return (SPRET_ABORT);
        }

        beam.range = spell_range(spell, range_power, true);

        if (testbits(flags, SPFLAG_NOT_SELF) && spd.isMe)
        {
            if (spell == SPELL_TELEPORT_OTHER || spell == SPELL_HEAL_OTHER
                || spell == SPELL_POLYMORPH_OTHER || spell == SPELL_BANISHMENT)
            {
                mpr( "Sorry, this spell works on others only." );
            }
            else
                canned_msg(MSG_UNTHINKING_ACT);

            return (SPRET_ABORT);
        }
    }

    // Enhancers only matter for calc_spell_power() and spell_fail().
    // Not sure about this: is it flavour or misleading? (jpeg)
    if (powc == 0 || allow_fail)
        _surge_power(spell);

    // Added this so that the passed in powc can have meaning -- bwr
    // Remember that most holy spells don't yet use powc!
    if (powc == 0)
        powc = calc_spell_power( spell, true );

    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    const bool normal_cast = crawl_state.prev_cmd == CMD_CAST_SPELL
                          && god == GOD_NO_GOD;

    // Make some noise if it's actually the player casting.
    if (god == GOD_NO_GOD)
        noisy( spell_noise(spell), you.pos() );

    if (allow_fail)
    {
        int spfl = random2avg(100, 3);

        if (you.religion != GOD_SIF_MUNA
            && you.penance[GOD_SIF_MUNA] && one_chance_in(20))
        {
            god_speaks(GOD_SIF_MUNA, "You feel a surge of divine spite.");

            // This will cause failure and increase the miscast effect.
            spfl = -you.penance[GOD_SIF_MUNA];

            // Reduced penance reduction here because casting spells
            // is a player controllable act.  -- bwr
            if (one_chance_in(12))
                dec_penance(GOD_SIF_MUNA, 1);
        }

        const int spfail_chance = spell_fail(spell);
        // Divination mappings backfire in Labyrinths.
        if (you.level_type == LEVEL_LABYRINTH
            && testbits(flags, SPFLAG_MAPPING))
        {
            mprf(MSGCH_WARN,
                 "The warped magic of this place twists your "
                 "spell in on itself!");
            spfl = spfail_chance / 2 - 1;
        }

        if (spfl < spfail_chance)
        {
            _spellcasting_side_effects(spell, true);

            mpr( "You miscast the spell." );
            flush_input_buffer( FLUSH_ON_FAILURE );
            learned_something_new( TUT_SPELL_MISCAST );

            if (you.religion == GOD_SIF_MUNA
                && !player_under_penance()
                && you.piety >= 100 && x_chance_in_y(you.piety + 1, 150))
            {
                canned_msg(MSG_NOTHING_HAPPENS);
                return (SPRET_FAIL);
            }

            // all spell failures give a bit of magical radiation..
            // failure is a function of power squared multiplied
            // by how badly you missed the spell.  High power
            // spells can be quite nasty: 9 * 9 * 90 / 500 = 15
            // points of contamination!
            int nastiness = spell_mana(spell) * spell_mana(spell)
                                              * (spfail_chance - spfl) + 250;

            const int cont_points = div_rand_round(nastiness, 500);

            // miscasts are uncontrolled
            contaminate_player( cont_points );

            MiscastEffect( &you, NON_MONSTER, spell, spell_mana(spell),
                           spfail_chance - spfl );

            return (SPRET_FAIL);
        }
    }

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Spell #%d, power=%d", spell, powc);
#endif

    switch (spell)
    {
    // spells using burn_freeze()
    case SPELL_BURN:
    case SPELL_FREEZE:
    case SPELL_CRUSH:
    case SPELL_ARC:
        if (!burn_freeze(powc, _spell_to_beam_type(spell),
                         mgrd(you.pos() + spd.delta)))
        {
            return (SPRET_ABORT);
        }
        break;

    // direct beams/bolts
    case SPELL_MAGIC_DART:
        if (!zapping(ZAP_MAGIC_DARTS, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_STRIKING:
        if (!zapping(ZAP_STRIKING, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_THROW_FLAME:
        if (!zapping(ZAP_FLAME, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_THROW_FROST:
        if (!zapping(ZAP_FROST, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_PAIN:
        if (!zapping(ZAP_PAIN, powc, beam, true))
            return (SPRET_ABORT);
        dec_hp(1, false);
        break;

    case SPELL_FLAME_TONGUE:
        if (!zapping(ZAP_FLAME_TONGUE, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_SANDBLAST:
        if (!cast_sandblast(powc, beam))
            return (SPRET_ABORT);
        break;

    case SPELL_BONE_SHARDS:
        if (!cast_bone_shards(powc, beam))
            return (SPRET_ABORT);
        break;

    case SPELL_SHOCK:
        if (!zapping(ZAP_ELECTRICITY, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_STING:
        if (!zapping(ZAP_STING, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_VAMPIRIC_DRAINING:
        vampiric_drain(powc, spd);
        break;

    case SPELL_BOLT_OF_FIRE:
        if (!zapping(ZAP_FIRE, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_BOLT_OF_COLD:
        if (!zapping(ZAP_COLD, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_STONE_ARROW:
        if (!zapping(ZAP_STONE_ARROW, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_POISON_ARROW:
        if (!zapping(ZAP_POISON_ARROW, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_BOLT_OF_IRON:
        if (!zapping(ZAP_IRON_BOLT, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_LIGHTNING_BOLT:
        if (!zapping(ZAP_LIGHTNING, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_BOLT_OF_MAGMA:
        if (!zapping(ZAP_MAGMA, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_VENOM_BOLT:
        if (!zapping(ZAP_VENOM_BOLT, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_BOLT_OF_DRAINING:
        if (!zapping(ZAP_NEGATIVE_ENERGY, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:
        if (!zapping(ZAP_CRYSTAL_SPEAR, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_BOLT_OF_INACCURACY:
        if (!zapping(ZAP_BEAM_OF_ENERGY, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_STICKY_FLAME:
        if (!zapping(ZAP_STICKY_FLAME, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_DISPEL_UNDEAD:
        if (!zapping(ZAP_DISPEL_UNDEAD, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_ISKENDERUNS_MYSTIC_BLAST:
        if (!zapping(ZAP_MYSTIC_BLAST, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_THUNDERBOLT:
        if (!zapping(ZAP_LIGHTNING, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_AGONY:
        if (!zapping(ZAP_AGONY, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_DISRUPT:
        if (!zapping(ZAP_DISRUPTION, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_DISINTEGRATE:
        if (!zapping(ZAP_DISINTEGRATION, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_ICE_BOLT:
        if (!zapping(ZAP_ICE_BOLT, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_ORB_OF_FRAGMENTATION:
        if (!zapping(ZAP_ORB_OF_FRAGMENTATION, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_CIGOTUVIS_DEGENERATION:
        if (!zapping(ZAP_DEGENERATION, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_ORB_OF_ELECTROCUTION:
        if (!zapping(ZAP_ORB_OF_ELECTRICITY, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_FLAME_OF_CLEANSING:
        cleansing_flame(powc, CLEANSING_FLAME_SPELL, you.pos(), &you);
        break;

    case SPELL_HOLY_WORD:
        holy_word(100, HOLY_WORD_SPELL, you.pos(), true, &you);
        break;

    case SPELL_REPEL_UNDEAD:
        turn_undead(100);
        break;

    case SPELL_HELLFIRE:
        // Should only be available from
        // staff of Dispater & Sceptre of Asmodeus
        if (!zapping(ZAP_HELLFIRE, powc, beam, true))
            return (SPRET_ABORT);
        break;

    // Clouds and explosions.
    case SPELL_MEPHITIC_CLOUD:
        if (!stinking_cloud(powc, beam))
            return (SPRET_ABORT);
        break;

    case SPELL_EVAPORATE:
        if (!cast_evaporate(powc, beam, potion))
            return SPRET_ABORT;
        break;

    case SPELL_POISONOUS_CLOUD:
        cast_big_c(powc, CLOUD_POISON, KC_YOU, beam);
        break;

    case SPELL_FREEZING_CLOUD:
        cast_big_c(powc, CLOUD_COLD, KC_YOU, beam);
        break;

    case SPELL_FIRE_STORM:
        cast_fire_storm(powc, beam);
        break;

    case SPELL_ICE_STORM:
        if (!zapping(ZAP_ICE_STORM, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_FIREBALL:
        if (!fireball(powc, beam))
            return (SPRET_ABORT);
        break;

    case SPELL_DELAYED_FIREBALL:
        if (normal_cast)
            crawl_state.cant_cmd_repeat("You can't repeat delayed fireball.");
        // This spell has two main advantages over Fireball:
        //
        // (1) The release is instantaneous, so monsters will not
        //     get an action before the player... this allows the
        //     player to hit monsters with a double fireball (this
        //     is why we only allow one delayed fireball at a time,
        //     if you want to allow for more, then the release should
        //     take at least some amount of time).
        //
        //     The casting of this spell still costs a turn.  So
        //     casting Delayed Fireball and immediately releasing
        //     the fireball is only slightly different from casting
        //     a regular Fireball (monsters act in the middle instead
        //     of at the end).  This is why we allow for the spell
        //     level discount so that Fireball is free with this spell
        //     (so that it only costs 7 levels instead of 13 to have
        //     both).
        //
        // (2) When the fireball is released, it is guaranteed to
        //     go off... the spell only fails at this point.  This can
        //     be a large advantage for characters who have difficulty
        //     casting Fireball in their standard equipment.  However,
        //     the power level for the actual fireball is determined at
        //     release, so if you do swap out your enhancers you'll
        //     get a less powerful ball when its released. -- bwr
        //
        if (!you.attribute[ ATTR_DELAYED_FIREBALL ])
        {
            // okay, this message is weak but functional -- bwr
            mpr( "You feel magically charged." );
            you.attribute[ ATTR_DELAYED_FIREBALL ] = 1;
        }
        else
            canned_msg( MSG_NOTHING_HAPPENS );
        break;

    // LOS spells
    case SPELL_SMITING:
        if (!cast_smiting(powc, beam.target))
            return (SPRET_ABORT);
        break;

    case SPELL_TWIST:
        cast_twist(powc, beam.target);
        break;

    case SPELL_AIRSTRIKE:
        airstrike(powc, spd);
        break;

    case SPELL_FRAGMENTATION:
        if (!cast_fragmentation(powc, spd))
            return (SPRET_ABORT);
        break;

    case SPELL_FAR_STRIKE:
        cast_far_strike(powc);
        break;

    case SPELL_PORTAL_PROJECTILE:
        if (!cast_portal_projectile(powc))
            return SPRET_ABORT;
        break;

    // other effects
    case SPELL_DISCHARGE:
        cast_discharge(powc);
        break;

    case SPELL_CHAIN_LIGHTNING:
        cast_chain_lightning(powc);
        break;

    case SPELL_DISPERSAL:
        cast_dispersal(powc);
        break;

    case SPELL_SHATTER:
        cast_shatter(powc);
        break;

    case SPELL_BEND:
        cast_bend(powc);
        break;

    case SPELL_SYMBOL_OF_TORMENT:
        torment(TORMENT_SPELL, you.pos());
        break;

    case SPELL_OZOCUBUS_REFRIGERATION:
        cast_refrigeration(powc);
        break;

    case SPELL_IGNITE_POISON:
        cast_ignite_poison(powc);
        break;

    case SPELL_ROTTING:
        cast_rotting(powc);
        break;

    // Summoning spells, and other spells that create new monsters.
    // If a god is making you cast one of these spells, any monsters
    // produced will count as god gifts.
    case SPELL_SUMMON_BUTTERFLIES:
        cast_summon_butterflies(powc, god);
        break;

    case SPELL_SUMMON_SMALL_MAMMALS:
        cast_summon_small_mammals(powc, god);
        break;

    case SPELL_STICKS_TO_SNAKES:
        cast_sticks_to_snakes(powc, god);
        break;

    case SPELL_SUMMON_SCORPIONS:
        cast_summon_scorpions(powc, god);
        break;

    case SPELL_SUMMON_SWARM:
        cast_summon_swarm(powc, god);
        break;

    case SPELL_CALL_CANINE_FAMILIAR:
        cast_call_canine_familiar(powc, god);
        break;

    case SPELL_SUMMON_ELEMENTAL:
        if (!cast_summon_elemental(powc, god))
            return (SPRET_ABORT);
        break;

    case SPELL_SUMMON_ICE_BEAST:
        cast_summon_ice_beast(powc, god);
        break;

    case SPELL_SUMMON_UGLY_THING:
        cast_summon_ugly_thing(powc, god);
        break;

    case SPELL_SUMMON_DRAGON:
        cast_summon_dragon(powc, god);
        break;

    case SPELL_SUMMON_ANGEL:
        summon_holy_being_type(MONS_ANGEL, powc, god, (int)spell);
        break;

    case SPELL_SUMMON_DAEVA:
        summon_holy_being_type(MONS_DAEVA, powc, god, (int)spell);
        break;

    case SPELL_TUKIMAS_DANCE:
        // Temporarily turn a wielded weapon into a dancing weapon.
        if (normal_cast)
            crawl_state.cant_cmd_repeat("You can't repeat Tukima's Dance.");
        cast_tukimas_dance(powc, god);
        break;

    case SPELL_CONJURE_BALL_LIGHTNING:
        cast_conjure_ball_lightning(powc, god);
        break;

    case SPELL_CALL_IMP:
        cast_call_imp(powc, god);
        break;

    case SPELL_SUMMON_DEMON:
        cast_summon_demon(powc, god);
        break;

    case SPELL_DEMONIC_HORDE:
        cast_demonic_horde(powc, god);
        break;

    case SPELL_SUMMON_GREATER_DEMON:
        cast_summon_greater_demon(powc, god);
        break;

    case SPELL_SHADOW_CREATURES:
        cast_shadow_creatures(god);
        break;

    case SPELL_SUMMON_HORRIBLE_THINGS:
        cast_summon_horrible_things(powc, god);
        break;

    case SPELL_ANIMATE_SKELETON:
        mpr("You attempt to give life to the dead...");
        animate_remains(you.pos(), CORPSE_SKELETON, BEH_FRIENDLY,
                        you.pet_target, god);
        break;

    case SPELL_ANIMATE_DEAD:
        mpr("You call on the dead to walk for you.");
        animate_dead(&you, powc + 1, BEH_FRIENDLY, you.pet_target, god);
        break;

    case SPELL_SIMULACRUM:
        cast_simulacrum(powc, god);
        break;

    case SPELL_TWISTED_RESURRECTION:
        cast_twisted_resurrection(powc, god);
        break;

    case SPELL_SUMMON_WRAITHS:
        cast_summon_wraiths(powc, god);
        break;

    case SPELL_DEATH_CHANNEL:
        cast_death_channel(powc, god);
        break;

    // Enchantments.
    case SPELL_CONFUSING_TOUCH:
        cast_confusing_touch(powc);
        break;

    case SPELL_BACKLIGHT:
        if (!zapping(ZAP_BACKLIGHT, powc + 10, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_CAUSE_FEAR:
        mass_enchantment(ENCH_FEAR, powc, MHITYOU);
        break;

    case SPELL_SLOW:
        if (!zapping(ZAP_SLOWING, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_CONFUSE:
        if (!zapping(ZAP_CONFUSION, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_ENSLAVEMENT:
        if (!zapping(ZAP_ENSLAVEMENT, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_TAME_BEASTS:
        cast_tame_beasts(powc);
        break;

    case SPELL_SLEEP:
    {
        const int sleep_power =
            stepdown_value( powc * 9 / 10, 5, 35, 45, 50 );
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Sleep power stepdown: %d -> %d",
             powc, sleep_power);
#endif
        if (!zapping(ZAP_SLEEP, sleep_power, beam, true))
            return (SPRET_ABORT);
        break;
    }

    case SPELL_PARALYSE:
        if (!zapping(ZAP_PARALYSIS, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_PETRIFY:
        if (!zapping(ZAP_PETRIFY, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_POLYMORPH_OTHER:
        // Trying is already enough, even if it fails.
        did_god_conduct(DID_DELIBERATE_MUTATING, 10);

        if (!zapping(ZAP_POLYMORPH_OTHER, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_TELEPORT_OTHER:
        if (!zapping(ZAP_TELEPORTATION, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_INTOXICATE:
        cast_intoxicate(powc);
        break;

    case SPELL_MASS_CONFUSION:
        mass_enchantment(ENCH_CONFUSION, powc, MHITYOU);
        break;

    case SPELL_MASS_SLEEP:
        cast_mass_sleep(powc);
        break;

    case SPELL_CONTROL_UNDEAD:
        mass_enchantment(ENCH_CHARM, powc, MHITYOU);
        break;

    case SPELL_ABJURATION_I:
    case SPELL_ABJURATION_II:
        abjuration(powc);
        break;

    case SPELL_BANISHMENT:
        if (beam.target == you.pos())
        {
            mpr("You cannot banish yourself!");
            break;
        }
        if (!zapping(ZAP_BANISHMENT, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_OLGREBS_TOXIC_RADIANCE:
        cast_toxic_radiance();
        break;

    // beneficial enchantments
    case SPELL_HASTE:
        if (!zapping(ZAP_HASTING, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_INVISIBILITY:
        if (!zapping(ZAP_INVISIBILITY, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_LESSER_HEALING:
        if (!cast_healing(5))
            return (SPRET_ABORT);
        break;

    case SPELL_GREATER_HEALING:
        if (!cast_healing(25))
            return (SPRET_ABORT);
        break;

    case SPELL_HEAL_OTHER:
        zapping(ZAP_HEALING, powc, beam);
        break;

    // Self-enchantments. (Spells that can only affect the player.)
    // Resistances.
    case SPELL_INSULATION:
        cast_insulation(powc);
        break;

    case SPELL_RESIST_POISON:
        cast_resist_poison(powc);
        break;

    case SPELL_SEE_INVISIBLE:
        cast_see_invisible(powc);
        break;

    case SPELL_CONTROL_TELEPORT:
        cast_teleport_control(powc);
        break;

    // Healing.
    case SPELL_CURE_POISON_I:   // Ely version
    case SPELL_CURE_POISON_II:  // Poison magic version
        // both use same function
        cast_cure_poison(powc);
        break;

    case SPELL_PURIFICATION:
        purification();
        break;

    case SPELL_RESTORE_STRENGTH:
        restore_stat(STAT_STRENGTH, 0, false);
        break;

    case SPELL_RESTORE_INTELLIGENCE:
        restore_stat(STAT_INTELLIGENCE, 0, false);
        break;

    case SPELL_RESTORE_DEXTERITY:
        restore_stat(STAT_DEXTERITY, 0, false);
        break;

    // Weapon brands.
    case SPELL_SURE_BLADE:
        cast_sure_blade(powc);
        break;

    case SPELL_TUKIMAS_VORPAL_BLADE:
        if (!brand_weapon(SPWPN_VORPAL, powc))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_FIRE_BRAND:
        if (!brand_weapon(SPWPN_FLAMING, powc))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_FREEZING_AURA:
        if (!brand_weapon(SPWPN_FREEZING, powc))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_MAXWELLS_SILVER_HAMMER:
        if (!brand_weapon(SPWPN_DUMMY_CRUSHING, powc))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_POISON_WEAPON:
        if (!brand_weapon(SPWPN_VENOM, powc))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_EXCRUCIATING_WOUNDS:
        if (!brand_weapon(SPWPN_PAIN, powc))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_LETHAL_INFUSION:
        if (!brand_weapon(SPWPN_DRAINING, powc))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_WARP_BRAND:
        if (!brand_weapon(SPWPN_DISTORTION, powc))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_POISON_AMMUNITION:
        cast_poison_ammo();
        break;

    // Transformations.
    case SPELL_BLADE_HANDS:
        if (!transform(powc, TRAN_BLADE_HANDS))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_SPIDER_FORM:
        if (!transform(powc, TRAN_SPIDER))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_STATUE_FORM:
        if (!transform(powc, TRAN_STATUE))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_ICE_FORM:
        if (!transform(powc, TRAN_ICE_BEAST))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_DRAGON_FORM:
        if (!transform(powc, TRAN_DRAGON))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_NECROMUTATION:
        if (!transform(powc, TRAN_LICH))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_AIR_WALK:
        if (!transform(powc, TRAN_AIR))
            canned_msg(MSG_SPELL_FIZZLES);
        break;

    case SPELL_ALTER_SELF:
        // Trying is already enough, even if it fails.
        did_god_conduct(DID_DELIBERATE_MUTATING, 10);

        if (normal_cast)
            crawl_state.cant_cmd_repeat("You can't repeat Alter Self.");
        if (!enough_hp( you.hp_max / 2, true ))
        {
            mpr( "Your body is in too poor a condition "
                 "for this spell to function." );

            return (SPRET_FAIL);
        }

        mpr("Your body is suffused with transfigurative energy!");

        set_hp( 1 + random2(you.hp), false );

        if (!mutate(RANDOM_MUTATION, false))
            mpr("Odd... you don't feel any different.");
        break;

    // General enhancement.
    case SPELL_BERSERKER_RAGE:
        if (!berserk_check_wielded_weapon())
           return (SPRET_ABORT);

        cast_berserk();
        break;

    case SPELL_REGENERATION:
        cast_regen(powc);
        break;

    case SPELL_REPEL_MISSILES:
        missile_prot(powc);
        break;

    case SPELL_DEFLECT_MISSILES:
        deflection(powc);
        break;

    case SPELL_SWIFTNESS:
        cast_swiftness(powc);
        break;

    case SPELL_LEVITATION:
        potion_effect(POT_LEVITATION, powc);
        break;

    case SPELL_FLY:
        cast_fly(powc);
        break;

    case SPELL_STONESKIN:
        cast_stoneskin(powc);
        break;

    case SPELL_STONEMAIL:
        stone_scales(powc);
        break;

    case SPELL_CONDENSATION_SHIELD:
        cast_condensation_shield(powc);
        break;

    case SPELL_OZOCUBUS_ARMOUR:
        ice_armour(powc, false);
        break;

    case SPELL_FORESCRY:
        cast_forescry(powc);
        break;

    case SPELL_SILENCE:
        cast_silence(powc);
        break;

    // other
    case SPELL_SELECTIVE_AMNESIA:
        crawl_state.cant_cmd_repeat("You can't repeat selective amnesia.");

        // Sif Muna power calls with true
        if (!cast_selective_amnesia(false))
            return (SPRET_ABORT);
        break;

    case SPELL_EXTENSION:
        extension(powc);
        break;

    case SPELL_BORGNJORS_REVIVIFICATION:
        cast_revivification(powc);
        break;

    case SPELL_SUBLIMATION_OF_BLOOD:
        cast_sublimation_of_blood(powc);
        break;

    case SPELL_DEATHS_DOOR:
        cast_deaths_door(powc);
        break;

    case SPELL_RING_OF_FLAMES:
        cast_ring_of_flames(powc);
        break;

    // Escape spells.
    case SPELL_BLINK:
        random_blink(true);
        break;

    case SPELL_TELEPORT_SELF:
        you_teleport();
        break;

    case SPELL_SEMI_CONTROLLED_BLINK:
        //jmf: powc is ignored
        if (cast_semi_controlled_blink(powc) == -1)
            return (SPRET_ABORT);
        break;

    case SPELL_CONTROLLED_BLINK:
        if (blink(powc, true) == -1)
            return (SPRET_ABORT);
        break;

    // Utility spells.
    case SPELL_DETECT_CURSE:
        detect_curse(false);
        break;

    case SPELL_REMOVE_CURSE:
        remove_curse(false);
        break;

    case SPELL_IDENTIFY:
        identify(powc);
        break;

    case SPELL_DETECT_SECRET_DOORS:
        if (_can_cast_detect())
            cast_detect_secret_doors(powc);
        break;

    case SPELL_DETECT_TRAPS:
        if (_can_cast_detect())
            mprf("You detect %s", (detect_traps(powc) > 0) ? "traps!"
                                                           : "nothing.");
        break;

    case SPELL_DETECT_ITEMS:
        if (_can_cast_detect())
            mprf("You detect %s", (detect_items(powc) > 0) ? "items!"
                                                           : "nothing.");
        break;

    case SPELL_DETECT_CREATURES:
    {
        if (!_can_cast_detect())
            break;

        const int prev_detected = count_detected_mons();
        const int num_creatures = detect_creatures(powc);

        if (!num_creatures)
            mpr("You detect nothing.");
        else if (num_creatures == prev_detected)
        {
            // This is not strictly true. You could have cast
            // Detect Creatures with a big enough fuzz that the detected
            // glyph is still on the map when the original one has been
            // killed. Then another one is spawned, so the number is
            // the same as before. There's no way we can check this however.
            mpr("You detect no further creatures.");
        }
        else
            mpr("You detect creatures!");
        break;
    }

    case SPELL_MAGIC_MAPPING:
        if (you.level_type == LEVEL_PANDEMONIUM)
        {
            mpr("Your Earth magic cannot map Pandemonium.");
        }
        else
        {
            powc = stepdown_value( powc, 10, 10, 40, 45 );
            magic_mapping( 5 + powc, 50 + random2avg( powc * 2, 2 ), false );
        }
        break;

    case SPELL_CREATE_NOISE:  // Unused, the player can shout to do this. - bwr
        noisy(25, you.pos(), "You hear a voice calling your name!");
        break;

    case SPELL_PROJECTED_NOISE:
        project_noise();
        break;

    case SPELL_CONJURE_FLAME:
        if (!conjure_flame(powc))
            return (SPRET_ABORT);
        break;

    case SPELL_DIG:
        if (!zapping(ZAP_DIGGING, powc, beam, true))
            return (SPRET_ABORT);
        break;

    case SPELL_PASSWALL:
        cast_passwall(powc);
        break;

    case SPELL_TOMB_OF_DOROKLOHE:
        entomb(powc);
        break;

    case SPELL_APPORTATION:
        if (!cast_apportation(powc, beam.target))
            return (SPRET_ABORT);
        break;

    case SPELL_SWAP:
        if (normal_cast)
            crawl_state.cant_cmd_repeat("You can't swap.");
        cast_swap(powc);
        break;

    case SPELL_RECALL:
        recall(0);
        break;

    case SPELL_PORTAL:
        if (normal_cast)
            crawl_state.cant_cmd_repeat("You can't repeat create portal.");
        if (portal() == -1)
            return (SPRET_ABORT);
        break;

    case SPELL_CORPSE_ROT:
        corpse_rot();
        break;

    case SPELL_FULSOME_DISTILLATION:
        cast_fulsome_distillation(powc);
        break;

    case SPELL_DETECT_MAGIC:
        mpr("FIXME: implement!");
        break;

    case SPELL_DEBUGGING_RAY:
        if (!zapping(ZAP_DEBUGGING_RAY, powc, beam, true))
            return (SPRET_ABORT);
        break;

    default:
#ifdef WIZARD
        if (you.wizard && !allow_fail && is_valid_spell(spell)
            && (flags & SPFLAG_MONSTER))
        {
            _try_monster_cast(spell, powc, spd, beam);
            return (SPRET_SUCCESS);
        }
#endif

        if (is_valid_spell(spell))
            mprf(MSGCH_ERROR, "Spell '%s' is not a player castable spell.",
                 spell_title(spell));
        else
            mpr("Invalid spell!", MSGCH_ERROR);

        return (SPRET_ABORT);
        break;
    }                           // end switch

    _spellcasting_side_effects(spell);

    return (SPRET_SUCCESS);
}

void exercise_spell( spell_type spell, bool spc, bool success )
{
    // (!success) reduces skill increase for miscast spells
    int ndx = 0;
    int skill;
    int exer = 0;
    int exer_norm = 0;
    int workout = 0;

    // This is used as a reference level to normalise spell skill training
    // (for Sif Muna piety). Normalised skill training is worked out as:
    // norm = actual_amount_trained * species_aptitude / ref_skill. This was
    // set at 50 in stone_soup 0.1.1 (which is bad).
    const int ref_skill = 80;

    unsigned int disciplines = get_spell_disciplines(spell);

    //jmf: evil evil evil -- exclude HOLY bit
    disciplines &= (~SPTYP_HOLY);

    int skillcount = count_bits( disciplines );

    if (!success)
        skillcount += 4 + random2(10);

    const int diff = spell_difficulty(spell);
    for (ndx = 0; ndx <= SPTYP_LAST_EXPONENT; ndx++)
    {
        if (!spell_typematch( spell, 1 << ndx ))
            continue;

        skill = spell_type2skill( 1 << ndx );
        workout = (random2(1 + diff) / skillcount);

        if (!one_chance_in(5))
            workout++;       // most recently, this was an automatic add {dlb}

        const int exercise_amount = exercise( skill, workout );
        exer      += exercise_amount;
        exer_norm +=
            exercise_amount * species_skills(skill, you.species) / ref_skill;
    }

    /* ******************************************************************
       Other recent formulae for the above:

       * workout = random2(spell_difficulty(spell_ex)
       * (10 + (spell_difficulty(spell_ex) * 2 )) / 10 / spellsy + 1);

       * workout = spell_difficulty(spell_ex)
       * (15 + spell_difficulty(spell_ex)) / 15 / spellsy;

       spellcasting had also been generally exercised at the same time
       ****************************************************************** */

    if (spc)
    {
        const int exercise_amount =
            exercise(SK_SPELLCASTING, one_chance_in(3) ? 1
                            : random2(1 + random2(diff)));
        exer      += exercise_amount;
        exer_norm += exercise_amount *
            species_skills(SK_SPELLCASTING, you.species) / ref_skill;
    }

    if (exer_norm)
        did_god_conduct( DID_SPELL_PRACTISE, exer_norm );
}

#define MAX_RECURSE 100

MiscastEffect::MiscastEffect(actor* _target, int _source, spell_type _spell,
                             int _pow, int _fail, std::string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, std::string _hand_str,
                             bool _can_plural) :
    target(_target), source(_source), cause(_cause), spell(_spell),
    school(SPTYP_NONE), pow(_pow), fail(_fail), level(-1), kc(KC_NCATEGORIES),
    kt(KILL_NONE), mon_target(NULL), mon_source(NULL),
    nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin),
    hand_str(_hand_str), can_plural_hand(_can_plural)
{
    ASSERT(is_valid_spell(_spell));
    unsigned int schools = get_spell_disciplines(_spell);
    ASSERT(schools != SPTYP_NONE);
    ASSERT(!(schools & SPTYP_HOLY));
    UNUSED(schools);

    init();
    do_miscast();
}

MiscastEffect::MiscastEffect(actor* _target, int _source,
                             spschool_flag_type _school, int _level,
                             std::string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, std::string _hand_str,
                             bool _can_plural) :
    target(_target), source(_source), cause(_cause), spell(SPELL_NO_SPELL),
    school(_school), pow(-1), fail(-1), level(_level), kc(KC_NCATEGORIES),
    kt(KILL_NONE), mon_target(NULL), mon_source(NULL),
    nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin),
    hand_str(_hand_str), can_plural_hand(_can_plural)
{
    ASSERT(!_cause.empty());
    ASSERT(count_bits(_school) == 1);
    ASSERT(_school < SPTYP_HOLY || _school == SPTYP_RANDOM);
    ASSERT(level >= 0 && level <= 3);

    init();
    do_miscast();
}

MiscastEffect::MiscastEffect(actor* _target, int _source,
                             spschool_flag_type _school, int _pow, int _fail,
                             std::string _cause,
                             nothing_happens_when_type _nothing_happens,
                             int _lethality_margin, std::string _hand_str,
                             bool _can_plural) :
    target(_target), source(_source), cause(_cause), spell(SPELL_NO_SPELL),
    school(_school), pow(_pow), fail(_fail), level(-1), kc(KC_NCATEGORIES),
    kt(KILL_NONE), mon_target(NULL), mon_source(NULL),
    nothing_happens_when(_nothing_happens),
    lethality_margin(_lethality_margin),
    hand_str(_hand_str), can_plural_hand(_can_plural)
{
    ASSERT(!_cause.empty());
    ASSERT(count_bits(_school) == 1);
    ASSERT(_school < SPTYP_HOLY || _school == SPTYP_RANDOM);

    init();
    do_miscast();
}

MiscastEffect::~MiscastEffect()
{
    ASSERT(recursion_depth == 0);
}

void MiscastEffect::init()
{
    ASSERT(spell != SPELL_NO_SPELL && school == SPTYP_NONE
           || spell == SPELL_NO_SPELL && school != SPTYP_NONE);
    ASSERT(pow != -1 && fail != -1 && level == -1
           || pow == -1 && fail == -1 && level >= 0 && level <= 3);

    ASSERT(target != NULL);
    ASSERT(target->alive());

    ASSERT(lethality_margin == 0 || target->atype() == ACT_PLAYER);

    recursion_depth = 0;

    source_known = target_known = false;

    mon_target = mon_source = NULL;
    act_source = NULL;

    const bool death_curse = (cause.find("death curse") != std::string::npos);

    if (target->atype() == ACT_MONSTER)
    {
        mon_target   = dynamic_cast<monsters*>(target);
        target_known = you.can_see(mon_target);
    }
    else
        target_known = true;

    kill_source = source;
    if (source == WIELD_MISCAST || source == MELEE_MISCAST)
    {
        if (target->atype() == ACT_MONSTER)
        {
            mon_source  = dynamic_cast<monsters*>(target);
            kill_source = monster_index(mon_source);
        }
        else
            kill_source = NON_MONSTER;
    }

    if (kill_source == NON_MONSTER)
    {
        kc           = KC_YOU;
        kt           = KILL_YOU_MISSILE;
        act_source   = &you;
        source_known = true;
    }
    else if (!invalid_monster_index(kill_source))
    {
        mon_source = &menv[kill_source];
        act_source = mon_source;
        ASSERT(mon_source->type != -1);

        if (death_curse && target->atype() == ACT_MONSTER
            && mon_target->confused_by_you())
        {
            kt = KILL_YOU_CONF;
        }
        else if (!death_curse && mon_source->confused_by_you()
                 && !mons_friendly(mon_source))
        {
            kt = KILL_YOU_CONF;
        }
        else
            kt = KILL_MON_MISSILE;

        if (mons_friendly(mon_source))
            kc = KC_FRIENDLY;
        else
            kc = KC_OTHER;

        source_known = you.can_see(mon_source);

        if (target_known && death_curse)
            source_known = true;
    }
    else
    {
        ASSERT(source == ZOT_TRAP_MISCAST
               || source == MISC_KNOWN_MISCAST
               || source == MISC_UNKNOWN_MISCAST
               || (source < 0 && -source < NUM_GODS));

        act_source = target;

        kc = KC_OTHER;
        kt = KILL_MISC;

        if (source == ZOT_TRAP_MISCAST)
        {
            source_known = target_known;

            if (target->atype() == ACT_MONSTER
                && mon_target->confused_by_you())
            {
                kt = KILL_YOU_CONF;
            }
        }
        else if (source == MISC_KNOWN_MISCAST)
            source_known = true;
        else if (source == MISC_UNKNOWN_MISCAST)
            source_known = false;
        else
            source_known = true;
    }

    ASSERT(kc != KC_NCATEGORIES && kt != KILL_NONE);

    if (death_curse && !invalid_monster_index(kill_source))
    {
        if (starts_with(cause, "a "))
            cause.replace(cause.begin(), cause.begin() + 1, "an indirect");
        else if (starts_with(cause, "an "))
            cause.replace(cause.begin(), cause.begin() + 2, "an indirect");
        else
            cause = replace_all(cause, "death curse", "indirect death curse");
   }

    // source_known = false for MELEE_MISCAST so that melee miscasts
    // won't give a "nothing happens" message.
    if (source == MELEE_MISCAST)
        source_known = false;

    if (hand_str.empty())
        target->hand_name(true, &can_plural_hand);

    // Explosion stuff.
    beam.is_explosion = true;

    if (cause.empty())
        cause = get_default_cause();
    beam.aux_source  = cause;
    beam.beam_source = kill_source;
    beam.thrower     = kt;
}

std::string MiscastEffect::get_default_cause()
{
    // This is only for true miscasts, which means both a spell and that
    // the source of the miscast is the same as the target of the miscast.
    ASSERT(source >= 0 && source <= NON_MONSTER);
    ASSERT(spell != SPELL_NO_SPELL);
    ASSERT(school == SPTYP_NONE);

    if (source == NON_MONSTER)
    {
        ASSERT(target->atype() == ACT_PLAYER);
        std::string str = "your miscasting ";
        str += spell_title(spell);
        return str;
    }

    ASSERT(mon_source != NULL);
    ASSERT(mon_source == mon_target);

    if (you.can_see(mon_source))
    {
        return apostrophise(mon_source->base_name(DESC_PLAIN))
            + " spell miscasting";
    }
    else
        return "something's spell miscasting";
}

bool MiscastEffect::neither_end_silenced()
{
    return (!silenced(you.pos()) && !silenced(target->pos()));
}

void MiscastEffect::do_miscast()
{
    ASSERT(recursion_depth >= 0 && recursion_depth < MAX_RECURSE);

    if (recursion_depth == 0)
        did_msg = false;

    unwind_var<int> unwind_depth(recursion_depth);
    recursion_depth++;

    // Repeated calls to do_miscast() on a single object instance have
    // killed a target which was alive when the object was created.
    if (!target->alive())
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Miscast target '%s' already dead",
             target->name(DESC_PLAIN, true).c_str());
#endif
        return;
    }

    spschool_flag_type sp_type;
    int                severity;

    if (spell != SPELL_NO_SPELL)
    {
        std::vector<int> school_list;
        for (int i = 0; i < SPTYP_LAST_EXPONENT; i++)
            if (spell_typematch(spell, 1 << i))
                school_list.push_back(i);

        unsigned int _school = school_list[random2(school_list.size())];
        sp_type = static_cast<spschool_flag_type>(1 << _school);
    }
    else
    {
        sp_type = school;
        if (sp_type == SPTYP_RANDOM)
        {
            int exp = (random2(SPTYP_LAST_EXPONENT));
            sp_type = (spschool_flag_type) (1 << exp);
        }
    }

    if (level != -1)
        severity = level;
    else
    {
        severity = (pow * fail * (10 + pow) / 7 * WILD_MAGIC_NASTINESS) / 100;

#if DEBUG_DIAGNOSTICS || DEBUG_MISCAST
        mprf(MSGCH_DIAGNOSTICS, "'%s' miscast power: %d",
             spell != SPELL_NO_SPELL ? spell_title(spell)
                                     : spelltype_short_name(sp_type),
             severity);
#endif

        if (random2(40) > severity && random2(40) > severity)
        {
            if (target->atype() == ACT_PLAYER)
                canned_msg(MSG_NOTHING_HAPPENS);
            return;
        }

        severity /= 100;
        severity = random2(severity);
        if (severity > 3)
            severity = 3;
        else if (severity < 0)
            severity = 0;
    }

#if DEBUG_DIAGNOSTICS || DEBUG_MISCAST
    mprf(MSGCH_DIAGNOSTICS, "Sptype: %s, severity: %d",
         spelltype_short_name(sp_type), severity );
#endif

    beam.ex_size            = 1;
    beam.name               = "";
    beam.damage             = dice_def(0, 0);
    beam.flavour            = BEAM_NONE;
    beam.msg_generated      = false;
    beam.in_explosion_phase = false;

    // Do this here since multiple miscasts (wizmode testing) might move
    // the target around.
    beam.source = target->pos();
    beam.target = target->pos();
    beam.use_target_as_pos = true;

    all_msg = you_msg = mon_msg = mon_msg_seen = mon_msg_unseen = "";
    msg_ch  = MSGCH_PLAIN;

    switch (sp_type)
    {
    case SPTYP_CONJURATION:    _conjuration(severity);    break;
    case SPTYP_ENCHANTMENT:    _enchantment(severity);    break;
    case SPTYP_TRANSLOCATION:  _translocation(severity);  break;
    case SPTYP_SUMMONING:      _summoning(severity);      break;
    case SPTYP_NECROMANCY:     _necromancy(severity);     break;
    case SPTYP_TRANSMUTATION:  _transmutation(severity); break;
    case SPTYP_FIRE:           _fire(severity);           break;
    case SPTYP_ICE:            _ice(severity);            break;
    case SPTYP_EARTH:          _earth(severity);          break;
    case SPTYP_AIR:            _air(severity);            break;
    case SPTYP_POISON:         _poison(severity);         break;
    case SPTYP_DIVINATION:
        // Divination miscasts have nothing in common between the player
        // and monsters.
        if (target->atype() == ACT_PLAYER)
            _divination_you(severity);
        else
            _divination_mon(severity);
        break;

    default:
        DEBUGSTR("Invalid miscast spell discipline.");
    }

    if (target->atype() == ACT_PLAYER)
        xom_is_stimulated(severity * 50);
}

void MiscastEffect::do_msg(bool suppress_nothing_happnes)
{
    ASSERT(!did_msg);

    if (mon_target != NULL && !mons_near(mon_target))
        return;

    did_msg = true;

    std::string msg;

    if (!all_msg.empty())
        msg = all_msg;
    else if (target->atype() == ACT_PLAYER)
        msg = you_msg;
    else if (!mon_msg.empty())
    {
        msg = mon_msg;
        // Monster might be unseen with hands that can't be seen.
        ASSERT(msg.find("@hand") == std::string::npos);
    }
    else
    {
        if (you.can_see(target))
            msg = mon_msg_seen;
        else
        {
            msg = mon_msg_unseen;
            // Can't see the hands of invisible monsters.
            ASSERT(msg.find("@hand") == std::string::npos);
        }
    }

    if (msg.empty())
    {
        if (!suppress_nothing_happnes
            && (nothing_happens_when == NH_ALWAYS
                || (nothing_happens_when == NH_DEFAULT && source_known
                    && target_known)))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
        }

        return;
    }

    bool plural;

    if (hand_str.empty())
    {
        msg = replace_all(msg, "@hand@",  target->hand_name(false, &plural));
        msg = replace_all(msg, "@hands@", target->hand_name(true));
    }
    else
    {
        plural = can_plural_hand;
        msg = replace_all(msg, "@hand@",  hand_str);
        if (can_plural_hand)
            msg = replace_all(msg, "@hands@", pluralise(hand_str));
        else
            msg = replace_all(msg, "@hands@", hand_str);
    }

    if (plural)
        msg = replace_all(msg, "@hand_conj@", "");
    else
        msg = replace_all(msg, "@hand_conj@", "s");

    if (target->atype() == ACT_MONSTER)
        msg = do_mon_str_replacements(msg, mon_target, S_SILENT);

    mpr(msg.c_str(), msg_ch);
}

bool MiscastEffect::_ouch(int dam, beam_type flavour)
{
    // Delay do_msg() until after avoid_lethal().
    if (target->atype() == ACT_MONSTER)
    {
        do_msg(true);

        bolt beem;

        beem.flavour = flavour;
        dam = mons_adjust_flavoured(mon_target, beem, dam, true);
        mon_target->hurt(NULL, dam, BEAM_MISSILE, false);

        if (!mon_target->alive())
            monster_die(mon_target, kt, kill_source);
    }
    else
    {
        dam = check_your_resists(dam, flavour);

        if (avoid_lethal(dam))
            return (false);

        do_msg(true);

        kill_method_type method;

        if (source == NON_MONSTER && spell != SPELL_NO_SPELL)
            method = KILLED_BY_WILD_MAGIC;
        else if (source == ZOT_TRAP_MISCAST)
            method = KILLED_BY_TRAP;
        else if (source < 0 && -source < NUM_GODS)
        {
            god_type god = static_cast<god_type>(-source);

            if (god == GOD_XOM && you.penance[GOD_XOM] == 0)
                method = KILLED_BY_XOM;
            else
                method = KILLED_BY_DIVINE_WRATH;
        }
        else if (!invalid_monster_index(source))
            method = KILLED_BY_MONSTER;
        else
            method = KILLED_BY_SOMETHING;

        bool see_source = mon_source ? you.can_see(mon_source) : false;
        ouch(dam, kill_source, method, cause.c_str(), see_source);
    }

    return (true);
}

bool MiscastEffect::_explosion()
{
    ASSERT(!beam.name.empty());
    ASSERT(beam.damage.num != 0 && beam.damage.size != 0);
    ASSERT(beam.flavour != BEAM_NONE);

    int max_dam = beam.damage.num * beam.damage.size;
    max_dam = check_your_resists(max_dam, beam.flavour);
    if (avoid_lethal(max_dam))
        return (false);

    do_msg(true);
    beam.explode();

    return (true);
}

bool MiscastEffect::_lose_stat(unsigned char which_stat,
                               unsigned char stat_loss)
{
    if (lethality_margin <= 0)
        return lose_stat(which_stat, stat_loss, false, cause);

    if (which_stat == STAT_RANDOM)
    {
        const int might = you.duration[DUR_MIGHT] ? 5 : 0;

        std::vector<unsigned char> stat_types;
        if ((you.strength - might - stat_loss) > 0)
            stat_types.push_back(STAT_STRENGTH);
        if ((you.intel - stat_loss) > 0)
            stat_types.push_back(STAT_INTELLIGENCE);
        if ((you.dex - stat_loss) > 0)
            stat_types.push_back(STAT_DEXTERITY);

        if (stat_types.size() == 0)
        {
            if (avoid_lethal(you.hp))
                return (false);
            else
                return lose_stat(which_stat, stat_loss, false, cause);
        }

        which_stat = stat_types[random2(stat_types.size())];
    }

    int val;

    switch(which_stat)
    {
        case STAT_STRENGTH: val = you.strength; break;
        case STAT_INTELLIGENCE: val = you.intel; break;
        case STAT_DEXTERITY: val = you.dex; break;

        default: DEBUGSTR("Invalid stat type."); return (false);
    }

    if ((val - stat_loss) <= 0)
    {
        if (avoid_lethal(you.hp))
            return (false);
    }

    return lose_stat(which_stat, stat_loss, false, cause);
}

void MiscastEffect::_potion_effect(int pot_eff, int pot_pow)
{
    if (target->atype() == ACT_PLAYER)
    {
        potion_effect(static_cast<potion_type>(pot_eff), pot_pow, false);
        return;
    }

    switch (pot_eff)
    {
        case POT_LEVITATION:
            // There's no levitation enchantment for monsters, and,
            // anyway, it's not nearly as inconvenient for monsters as
            // for the player, so backlight them instead.
            mon_target->add_ench(mon_enchant(ENCH_BACKLIGHT, pot_pow, kc));
            break;
        case POT_BERSERK_RAGE:
            if (target->can_go_berserk())
            {
                target->go_berserk(false);
                break;
            }
            // Intentional fallthrough if that didn't work.
        case POT_SLOWING:
            target->slow_down(act_source, pot_pow);
            break;
        case POT_PARALYSIS:
            target->paralyse(act_source, pot_pow);
            break;
        case POT_CONFUSION:
            target->confuse(act_source, pot_pow);
            break;

        default:
            ASSERT(false);
    }
}

void MiscastEffect::send_abyss()
{
    if (you.level_type == LEVEL_ABYSS)
    {
        you_msg        = "The world appears momentarily distorted.";
        mon_msg_seen   = "@The_monster@ wobbles for a moment.";
        mon_msg_unseen = "An empty piece of space momentarily distorts.";
        do_msg();
        return;
    }

    target->banish(cause);
}

bool MiscastEffect::avoid_lethal(int dam)
{
    if (lethality_margin <= 0 || (you.hp - dam) > lethality_margin)
        return (false);

    if (recursion_depth == MAX_RECURSE)
    {
#if DEBUG_DIAGNOSTICS || DEBUG_MISCAST
        mpr("Couldn't avoid lethal miscast: too much recursion.",
            MSGCH_ERROR);
#endif
        return (false);
    }

    if (did_msg)
    {
#if DEBUG_DIAGNOSTICS || DEBUG_MISCAST
        mpr("Couldn't avoid lethal miscast: already printed message for this "
            "miscast.", MSGCH_ERROR);
#endif
        return (false);
    }

#if DEBUG_DIAGNOSTICS || DEBUG_MISCAST
    mpr("Avoided lethal miscast.", MSGCH_DIAGNOSTICS);
#endif

    do_miscast();

    return (true);
}

bool MiscastEffect::_create_monster(monster_type what, int abj_deg,
                                    bool alert)
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    mgen_data data = mgen_data::hostile_at(what, target->pos(),
                                           abj_deg, 0, alert, god);

    // hostile_at() assumes the monster is hostile to the player,
    // but should be hostile to the target monster unless the miscast
    // is a result of either divine wrath or a Zot trap.
    if (target->atype() == ACT_MONSTER && you.penance[god] == 0
        && source != ZOT_TRAP_MISCAST)
    {
        switch (mon_target->temp_attitude())
        {
            case ATT_FRIENDLY:     data.behaviour = BEH_HOSTILE; break;
            case ATT_HOSTILE:      data.behaviour = BEH_FRIENDLY; break;
            case ATT_GOOD_NEUTRAL:
            case ATT_NEUTRAL:
                data.behaviour = BEH_NEUTRAL;
            break;
        }

        if (alert)
            data.foe = monster_index(mon_target);

        // No permanent allies from miscasts.
        if (data.behaviour == BEH_FRIENDLY && abj_deg == 0)
            data.abjuration_duration = 6;
    }

    // If data.abjuration_duration == 0, then data.summon_type will
    // simply be ignored.
    if (data.abjuration_duration != 0)
    {
        if (you.penance[god] > 0)
            data.summon_type = MON_SUMM_WRATH;
        else if (source == ZOT_TRAP_MISCAST)
            data.summon_type = MON_SUMM_ZOT;
        else
            data.summon_type = MON_SUMM_MISCAST;
    }

    return (create_monster(data) != -1);
}

static bool _has_hair(actor* target)
{
    // Don't bother for monsters.
    if (target->atype() == ACT_MONSTER)
        return (false);

    return (!transform_changed_physiology() && you.species != SP_GHOUL
            && you.species != SP_KENKU && !player_genus(GENPC_DRACONIAN));
}

static std::string _hair_str(actor* target, bool &plural)
{
    ASSERT(target->atype() == ACT_PLAYER);

    if (you.species == SP_MUMMY)
    {
        plural = true;
        return "bandages";
    }
    else
    {
        plural = false;
        return "hair";
    }
}

void MiscastEffect::_conjuration(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (_has_hair(target) ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            you_msg      = "Sparks fly from your @hands@!";
            mon_msg_seen = "Sparks fly from @the_monster@'s @hands@!";
            break;
        case 1:
            you_msg      = "The air around you crackles with energy!";
            mon_msg_seen = "The air around @the_monster@ crackles "
                           "with energy!";
            break;
        case 2:
            you_msg      = "Wisps of smoke drift from your @hands@.";
            mon_msg_seen = "Wisps of smoke drift from @the_monster@'s "
                           "@hands@.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "You are momentarily dazzled by a flash of light!";
            mon_msg_seen = "@The_monster@ emits a flash of light!";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@ glows momentarily.";
            // A small glow isn't going to make it past invisibility.
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (player_can_smell())
                all_msg = "You smell something strange.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 10:
        {
            // Player only (for now).
            bool plural;
            std::string hair = _hair_str(target, plural);
            you_msg = make_stringf("Your %s stand%s on end.", hair.c_str(),
                                   plural ? "" : "s");
        }
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "Smoke pours from your @hands@!";
            mon_msg_seen   = "Smoke pours from @the_monster@'s @hands@!";
            mon_msg_unseen = "Smoke appears from out of nowhere!";

            do_msg();
            big_cloud(CLOUD_GREY_SMOKE, kc, kt, target->pos(), 20,
                      7 + random2(7));
            break;
        case 1:
            you_msg      = "A wave of violent energy washes through your body!";
            mon_msg_seen = "@The_monster@ lurches violently!";
            _ouch(6 + random2avg(7, 2));
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg      = "Energy rips through your body!";
            mon_msg_seen = "@The_monster@ jerks violently!";
            _ouch(9 + random2avg(17, 2));
            break;
        case 1:
            you_msg        = "You are caught in a violent explosion!";
            mon_msg_seen   = "@The_monster@ is caught in a violent explosion!";
            mon_msg_unseen = "A violent explosion happens from out of thin "
                             "air!";

            beam.flavour = BEAM_MISSILE;
            beam.damage  = dice_def(3, 12);
            beam.name    = "explosion";
            beam.colour  = random_colour();

            _explosion();
            break;
        }
        break;

    case 3:         // considerably less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg      = "You are blasted with magical energy!";
            mon_msg_seen = "@The_monster@ is blasted with magical energy!";
            // No message for invis monsters?
            _ouch(12 + random2avg(29, 2));
            break;
        case 1:
            all_msg = "There is a sudden explosion of magical energy!";

            beam.flavour = BEAM_MISSILE;
            beam.type    = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 20);
            beam.name    = "explosion";
            beam.colour  = random_colour();
            beam.ex_size = coinflip() ? 1 : 2;

            _explosion();
            break;
        }
    }
}

static void _your_hands_glow(actor* target, std::string& you_msg,
                             std::string& mon_msg_seen, bool pluralize)
{
    you_msg      = "Your @hands@ ";
    mon_msg_seen = "@The_monster@'s @hands@ ";
    // No message for invisible monsters.

    if (pluralize)
    {
       you_msg      += "glow";
       mon_msg_seen += "glow";
    }
    else
    {
        you_msg      += "glows";
        mon_msg_seen += "glows";
    }
    you_msg      += " momentarily.";
    mon_msg_seen += " momentarily.";
}

void MiscastEffect::_enchantment(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            _your_hands_glow(target, you_msg, mon_msg_seen, can_plural_hand);
            break;
        case 1:
            you_msg      = "The air around you crackles with energy!";
            mon_msg_seen = "The air around @the_monster@ crackles with"
                           " energy!";
            break;
        case 2:
            you_msg        = "Multicoloured lights dance before your eyes!";
            mon_msg_seen   = "Multicoloured lights dance around @the_monster@!";
            mon_msg_unseen = "Multicoloured lights dance in the air!";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "Waves of light ripple over your body.";
            mon_msg_seen = "Waves of light ripple over @the_monster@'s body.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@'s body glows momentarily.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                // XXX: Should use noisy().
                all_msg = "You hear something strange.";
                msg_ch  = MSGCH_SOUND;
                return;
            }
            else if (target->atype() == ACT_PLAYER
                     && you.attribute[ATTR_TRANSFORMATION] != TRAN_AIR)
            {
                you_msg = "Your skull vibrates slightly.";
            }
            break;
        }
        do_msg();
        break;

    case 1:         // slightly annoying
        switch (random2(crawl_state.arena ? 1 : 2))
        {
        case 0:
            _potion_effect(POT_LEVITATION, 20);
            break;
        case 1:
            // XXX: Something else for monsters?
            random_uselessness();
            break;
        }
        break;

    case 2:         // much more annoying
        switch (random2(7))
        {
        case 0:
        case 1:
        case 2:
            if (target->atype() == ACT_PLAYER)
            {
                mpr("You sense a malignant aura.");
                curse_an_item(false);
                break;
            }
            // Intentional fall-through for monsters.
        case 3:
        case 4:
        case 5:
            _potion_effect(POT_SLOWING, 10);
            break;
        case 6:
            _potion_effect(POT_BERSERK_RAGE, 10);
            break;
        }
        break;

    case 3:         // potentially lethal
        // Only use first two cases for monsters.
        switch (random2(target->atype() == ACT_PLAYER ? 4 : 2))
        {
        case 0:
            _potion_effect(POT_PARALYSIS, 10);
            break;
        case 1:
            _potion_effect(POT_CONFUSION, 10);
            break;
        case 2:
            mpr("You feel saturated with unharnessed energies!");
            you.magic_contamination += random2avg(19, 3);
            break;
        case 3:
            do
                curse_an_item(false);
            while (!one_chance_in(3));
            mpr("You sense an overwhelmingly malignant aura!");
            break;
        }
        break;
    }
}

void MiscastEffect::_translocation(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            you_msg      = "Space warps around you.";
            mon_msg_seen = "Space warps around @the_monster@.";
            break;
        case 1:
            you_msg      = "The air around you crackles with energy!";
            mon_msg_seen = "The air around @the_monster@ crackles with "
                           "energy!";
            break;
        case 2:
            you_msg      = "You feel a wrenching sensation.";
            mon_msg_seen = "@The monster@ jerks violently for a moment.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "You spin around.";
            mon_msg_seen = "@The_monster@ spins around.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "The world appears momentarily distorted!";
            mon_msg_seen = "@The_monster@ appears momentarily distorted!";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            you_msg = "You feel uncomfortable.";
            mon_msg_seen = "@The_monster@ grimaces.";
            break;
        }
        do_msg();
        break;

    case 1:         // mostly harmless
        switch (random2(6))
        {
        case 0:
        case 1:
        case 2:
            you_msg        = "You are caught in a localised field of spatial "
                             "distortion.";
            mon_msg_seen   = "@The_monster@ is caught in a localised field of "
                             "spatial distortion.";
            mon_msg_unseen = "A piece of empty space twists and distorts.";
            _ouch(4 + random2avg(9, 2));
            break;
        case 3:
        case 4:
            you_msg        = "Space bends around you!";
            mon_msg_seen   = "Space bends around @the_monster@!";
            mon_msg_unseen = "A piece of empty space twists and distorts.";
            if (_ouch(4 + random2avg(7, 2)) && target->alive())
                target->blink(false);
            break;
        case 5:
            if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                all_msg = "Space twists in upon itself!";
            do_msg();
            break;
        }
        break;

    case 2:         // less harmless
        switch (random2(7))
        {
        case 0:
        case 1:
        case 2:
            you_msg        = "You are caught in a strong localised spatial "
                             "distortion.";
            mon_msg_seen   = "@The_monster@ is caught in a strong localised "
                             "spatial distortion.";
            mon_msg_unseen = "A piece of empty space twists and writhes.";
            _ouch(9 + random2avg(23, 2));
            break;
        case 3:
        case 4:
            you_msg        = "Space warps around you!";
            mon_msg_seen   = "Space warps around @the_monster!";
            mon_msg_unseen = "A piece of empty space twists and writhes.";
            if (_ouch(5 + random2avg(9, 2)) && target->alive())
            {
                if (one_chance_in(3))
                    target->teleport(true);
                else
                    target->blink(false);
                _potion_effect(POT_CONFUSION, 40);
            }
            break;
        case 5:
        {
            bool success = false;

            for (int i = 1 + random2(3); i >= 0; --i)
            {
                if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                    success = true;
            }

            if (success)
                all_msg = "Space twists in upon itself!";
            break;
        }
        case 6:
            send_abyss();
            break;
        }
        break;

    case 3:         // much less harmless
        // Don't use the last case for monsters.
        switch (random2(target->atype() == ACT_PLAYER ? 4 : 3))
        {
        case 0:
            you_msg        = "You are caught in an extremely strong localised "
                             "spatial distortion!";
            mon_msg_seen   = "@The_monster@ is caught in an extremely strong "
                             "localised spatial distortion!";
            mon_msg_unseen = "A rift temporarily opens in the fabric of space!";
            _ouch(15 + random2avg(29, 2));
            break;
        case 1:
            you_msg        = "Space warps crazily around you!";
            mon_msg_seen   = "Space warps crazily around @the_monster@!";
            mon_msg_unseen = "A rift temporarily opens in the fabric of space!";
            if (_ouch(9 + random2avg(17, 2)) && target->alive())
            {
                target->teleport(true);
                _potion_effect(POT_CONFUSION, 60);
            }
            break;
        case 2:
            send_abyss();
            break;
        case 3:
            mpr("You feel saturated with unharnessed energies!");
            you.magic_contamination += random2avg(19, 3);
            break;
        }
        break;
    }
}

void MiscastEffect::_summoning(int severity)
{
    switch (severity)
    {
    case 0:         // harmless messages only
        switch (random2(10))
        {
        case 0:
            you_msg      = "Shadowy shapes form in the air around you, "
                           "then vanish.";
            mon_msg_seen = "Shadowy shapes form in the air around "
                           "@the_monster@, then vanish.";
            break;
        case 1:
            if (neither_end_silenced())
            {
                all_msg = "You hear strange voices.";
                msg_ch  = MSGCH_SOUND;
            }
            else if (target->atype() == ACT_PLAYER)
                you_msg = "You feel momentarily dizzy.";
            break;
        case 2:
            you_msg = "Your head hurts.";
            // Monster messages needed.
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg = "Your brain hurts!";
            // Monster messages needed.
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "The world appears momentarily distorted.";
            mon_msg_seen = "@The_monster@ appears momentarily distorted.";
            break;
        case 7:
            you_msg      = "Space warps around you.";
            mon_msg_seen = "Space warps around @the_monster@.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                you_msg      = "Distant voices call out to you!";
                mon_msg_seen = "Distant voices call out to @the_monster@!";
                msg_ch       = MSGCH_SOUND;
            }
            else if (target->atype() == ACT_PLAYER)
                you_msg = "You feel watched.";
            break;
        }
        do_msg();
        break;

    case 1:         // a little bad
        switch (random2(6))
        {
        case 0:             // identical to translocation
        case 1:
        case 2:
            you_msg        = "You are caught in a localised spatial "
                             "distortion.";
            mon_msg_seen   = "@The_monster@ is caught in a localised spatial "
                             "distortion.";
            mon_msg_unseen = "An empty piece of space distorts and twists.";
            _ouch(5 + random2avg(9, 2));
            break;
        case 3:
            if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                all_msg = "Space twists in upon itself!";
            do_msg();
            break;
        case 4:
        case 5:
            if (_create_monster(summon_any_demon(DEMON_LESSER), 5, true))
                all_msg = "Something appears in a flash of light!";
            do_msg();
            break;
        }
        break;

    case 2:         // more bad
        switch (random2(6))
        {
        case 0:
        {
            bool success = false;

            for (int i = 1 + random2(3); i >= 0; --i)
            {
                if (_create_monster(MONS_SPATIAL_VORTEX, 3))
                    success = true;
            }

            if (success)
                all_msg = "Space twists in upon itself!";
            do_msg();
            break;
        }

        case 1:
        case 2:
            if (_create_monster(summon_any_demon(DEMON_COMMON), 5, true))
                all_msg = "Something forms from out of thin air!";
            do_msg();
            break;

        case 3:
        case 4:
        case 5:
        {
            bool success = false;

            for (int i = 1 + random2(2); i >= 0; --i)
            {
                if (_create_monster(summon_any_demon(DEMON_LESSER), 5, true))
                    success = true;
            }

            if (success && neither_end_silenced())
            {
                you_msg = "A chorus of chattering voices calls out to you!";
                mon_msg = "A chorus of chattering voices calls out!";
                msg_ch  = MSGCH_SOUND;
            }
            do_msg();
            break;
        }
        }
        break;

    case 3:         // more bad
        switch (random2(4))
        {
        case 0:
            if (_create_monster(MONS_ABOMINATION_SMALL, 0, true))
                all_msg = "Something forms from out of thin air.";
            do_msg();
            break;

        case 1:
            if (_create_monster(summon_any_demon(DEMON_GREATER), 0, true))
                all_msg = "You sense a hostile presence.";
            do_msg();
            break;

        case 2:
        {
            bool success = false;

            for (int i = 1 + random2(2); i >= 0; --i)
            {
                if (_create_monster(summon_any_demon(DEMON_COMMON), 3, true))
                    success = true;
            }

            if (success)
            {
                you_msg = "Something turns its malign attention towards "
                          "you...";
                mon_msg = "You sense a malign presence.";
            }
            do_msg();
            break;
        }

        case 3:
            send_abyss();
            break;
        }
        break;
    }
}

void MiscastEffect::_divination_you(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        switch (random2(10))
        {
        case 0:
            mpr("Weird images run through your mind.");
            break;
        case 1:
            if (!silenced(you.pos()))
                mpr("You hear strange voices.", MSGCH_SOUND);
            else
                mpr("Your nose twitches.");
            break;
        case 2:
            mpr("Your head hurts.");
            break;
        case 3:
            mpr("You feel a strange surge of energy!");
            break;
        case 4:
            mpr("Your brain hurts!");
            break;
        case 5:
            mpr("Strange energies run through your body.");
            break;
        case 6:
            mpr("Everything looks hazy for a moment.");
            break;
        case 7:
            mpr("You seem to have forgotten something, but you can't remember what it was!");
            break;
        case 8:
            canned_msg(MSG_NOTHING_HAPPENS);
            break;
        case 9:
            mpr("You feel uncomfortable.");
            break;
        }
        break;

    case 1:         // more annoying things
        switch (random2(2))
        {
        case 0:
            mpr("You feel slightly disoriented.");
            forget_map(10 + random2(10));
            break;
        case 1:
            potion_effect(POT_CONFUSION, 10);
            break;
        }
        break;

    case 2:         // even more annoying things
        switch (random2(2))
        {
        case 0:
            if (you.is_undead)
                mpr("You suddenly recall your previous life!");
            else if (_lose_stat(STAT_INTELLIGENCE, 1 + random2(3)))
            {
                mpr("You have damaged your brain!");
            }
            else if (!did_msg)
                mpr("You have a terrible headache.");
            break;
        case 1:
            mpr("You feel lost.");
            forget_map(40 + random2(40));
            break;
        }

        potion_effect(POT_CONFUSION, 1);  // common to all cases here {dlb}
        break;

    case 3:         // nasty
        switch (random2(3))
        {
        case 0:
            mpr(forget_spell() ? "You have forgotten a spell!"
                               : "You get a splitting headache.");
            break;
        case 1:
            mpr("You feel completely lost.");
            forget_map(100);
            break;
        case 2:
            if (you.is_undead)
                mpr("You suddenly recall your previous life.");
            else if (_lose_stat(STAT_INTELLIGENCE, 3 + random2(3)))
            {
                mpr("You have damaged your brain!");
            }
            else if (!did_msg)
                mpr("You have a terrible headache.");
            break;
        }

        potion_effect(POT_CONFUSION, 1);  // common to all cases here {dlb}
        break;
    }
}

// XXX: Monster divination miscasts.
void MiscastEffect::_divination_mon(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        mon_msg_seen = "@The_monster@ looks momentarily confused.";
        break;

    case 1:         // more annoying things
        switch (random2(2))
        {
        case 0:
            mon_msg_seen = "@The_monster@ looks slightly disoriented.";
            break;
        case 1:
            mon_target->confuse(
                act_source,
                1 + random2(3 + act_source->get_experience_level()));
            break;
        }
        break;

    case 2:         // even more annoying things
        mon_msg_seen = "@The_monster@ shudders.";
        mon_target->confuse(
            act_source,
            5 + random2(3 + act_source->get_experience_level()));
        break;

    case 3:         // nasty
        mon_msg_seen = "@The_monster@ reels.";
        if (one_chance_in(7))
            mon_target->forget_random_spell();
        mon_target->confuse(
            act_source,
            8 + random2(3 + act_source->get_experience_level()));
        break;
    }
}

void MiscastEffect::_necromancy(int severity)
{
    if (target->atype() == ACT_PLAYER && you.religion == GOD_KIKUBAAQUDGHA
        && !player_under_penance() && you.piety >= piety_breakpoint(1)
        && x_chance_in_y(you.piety, 150))
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    switch (severity)
    {
    case 0:
        switch (random2(10))
        {
        case 0:
            if (player_can_smell())
                all_msg = "You smell decay.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 1:
            if (neither_end_silenced())
            {
                all_msg = "You hear strange and distant voices.";
                msg_ch  =  MSGCH_SOUND;
            }
            else if (target->atype() == ACT_PLAYER)
                you_msg = "You feel homesick.";
            break;
        case 2:
            you_msg = "Pain shoots through your body.";
            // Monster messages needed.
            break;
        case 3:
            you_msg = "Your bones ache.";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "The world around you seems to dim momentarily.";
            mon_msg_seen = "@The_monster@ seems to dim momentarily.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "You shiver with cold.";
            mon_msg_seen = "@The_monster@ shivers with cold.";
            break;
        case 7:
            you_msg = "You sense a malignant aura.";
            // Monster messages needed.
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            you_msg      = "You feel very uncomfortable.";
            mon_msg_seen = "@The_monster@ grimaces horribly.";
            break;
        }
        do_msg();
        break;

    case 1:         // a bit nasty
        switch (random2(3))
        {
        case 0:
            // Monster messages needed.
            if (target->res_torment())
                you_msg = "You feel weird for a moment.";
            else
            {
                you_msg = "Pain shoots through your body!";
                _ouch(5 + random2avg(15, 2));
            }
            break;
        case 1:
            you_msg      = "You feel horribly lethargic.";
            mon_msg_seen = "@The_monster@ looks incredibly listless.";
            _potion_effect(POT_SLOWING, 15);
            break;
        case 2:
            if (target->res_rotting() == 0)
            {
                if (player_can_smell())
                {
                    // identical to a harmless message
                    all_msg = "You smell decay.";
                }

                if (target->atype() == ACT_PLAYER)
                    you.rotting++;
                else
                    mon_target->add_ench(mon_enchant(ENCH_ROT, 1, kc));
            }
            else if (you.species == SP_MUMMY)
            {
                // Monster messages needed.
                you_msg = "Your bandages flutter.";
            }
            break;
        }
        if (!did_msg)
            do_msg();
        break;

    case 2:         // much nastier
        switch (random2(3))
        {
        case 0:
        {
            bool success = false;

            for (int i = random2(3); i >= 0; --i)
            {
                if (_create_monster(MONS_SHADOW, 2, true))
                    success = true;
            }

            if (success)
            {
                you_msg        = "Flickering shadows surround you.";
                mon_msg_seen   = "Flickering shadows surround @the_monster@.";
                mon_msg_unseen = "Shadows flicker in the thin air.";
            }
            do_msg();
            break;
        }

        case 1:
            you_msg      = "You are engulfed in negative energy!";
            mon_msg_seen = "@The_monster@ is engulfed in negative energy!";

            if (lethality_margin == 0 || you.experience > 0
                || !avoid_lethal(you.hp))
            {
                if (one_chance_in(3))
                {
                    target->drain_exp(act_source);
                    break;
                }
            }

            // If draining failed, just flow through...

        case 2:
            // Monster messages needed.
            if (target->res_torment())
            {
                you_msg = "You feel weird for a moment.";
                do_msg();
            }
            else
            {
                you_msg = "You convulse helplessly as pain tears through "
                          "your body!";
                _ouch(15 + random2avg(23, 2));
            }
            break;
        }
        break;

    case 3:         // even nastier
        // Don't use last case for monsters.
        switch (random2(target->atype() == ACT_PLAYER ? 6 : 5))
        {
        case 0:
            if (target->holiness() == MH_UNDEAD)
            {
                // Monster messages needed.
                you_msg = "Something just walked over your grave. No, really!";
                do_msg();
            }
            else
                torment_monsters(target->pos(), 0, TORMENT_GENERIC);
            break;

        case 1:
            target->rot(act_source, random2avg(7, 2) + 1);
            break;

        case 2:
            if (_create_monster(MONS_SOUL_EATER, 4, true))
            {
                you_msg        = "Something reaches out for you...";
                mon_msg_seen   = "Something reaches out for @the_monster@...";
                mon_msg_unseen = "Something reaches out from thin air...";
            }
            do_msg();
            break;

        case 3:
            if (_create_monster(MONS_REAPER, 4, true))
            {
                you_msg        = "Death has come for you...";
                mon_msg_seen   = "Death has come for @the_monster@...";
                mon_msg_unseen = "Death appears from thin air...";
            }
            do_msg();
            break;

        case 4:
            you_msg      = "You are engulfed in negative energy!";
            mon_msg_seen = "@The_monster@ is engulfed in negative energy!";

            if (lethality_margin == 0 || you.experience > 0
                || !avoid_lethal(you.hp))
            {
                target->drain_exp(act_source);
                break;
            }

            // If draining failed, just flow through if it's the player...
            if (target->atype() == ACT_MONSTER)
                break;

        case 5:
            _lose_stat(STAT_RANDOM, 1 + random2avg(7, 2));
            break;
        }
        break;
    }
}

void MiscastEffect::_transmutation(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (_has_hair(target) ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            _your_hands_glow(target, you_msg, mon_msg_seen, can_plural_hand);
            break;
        case 1:
            you_msg        = "The air around you crackles with energy!";
            mon_msg_seen   = "The air around @the_monster@ crackles with "
                             "energy!";
            mon_msg_unseen = "The thin air crackles with energy!";
            break;
        case 2:
            you_msg        = "Multicoloured lights dance before your eyes!";
            mon_msg_seen   = "Multicoloured lights dance around @the_monster@!";
            mon_msg_unseen = "Multicoloured lights dance in the air!";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg        = "Waves of light ripple over your body.";
            mon_msg_seen   = "Waves of light ripple over @the_monster@'s body.";
            mon_msg_unseen = "Waves of light ripple in the air.";
            break;
        case 5:
            you_msg      = "Strange energies run through your body.";
            mon_msg_seen = "@The_monster@ glows " + weird_glowing_colour() +
                           " for a moment.";
            break;
        case 6:
            you_msg      = "Your skin tingles.";
            mon_msg_seen = "@The_monster@ twitches.";
            break;
        case 7:
            you_msg      = "Your skin glows momentarily.";
            mon_msg_seen = "@The_monster@'s body glows momentarily.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (player_can_smell())
                all_msg = "You smell something strange.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 10:
        {
            // Player only (for now).
            bool plural;
            std::string hair = _hair_str(target, plural);
            you_msg = make_stringf("Your %s momentarily turn%s into snakes!",
                                   hair.c_str(), plural ? "" : "s");
        }
        }
        do_msg();
        break;

    case 1:         // slightly annoying
        switch (random2(crawl_state.arena ? 1 : 2))
        {
        case 0:
            you_msg      = "Your body is twisted painfully.";
            mon_msg_seen = "@The_monster@'s body twists unnaturally.";
            _ouch(1 + random2avg(11, 2));
            break;
        case 1:
            random_uselessness();
            break;
        }
        break;

    case 2:         // much more annoying
        // Last case for players only.
        switch (random2(target->atype() == ACT_PLAYER ? 4 : 3))
        {
        case 0:
            you_msg      = "Your body is twisted very painfully!";
            mon_msg_seen = "@The_monster@'s body twists and writhes.";
            _ouch(3 + random2avg(23, 2));
            break;
        case 1:
            _potion_effect(POT_PARALYSIS, 10);
            break;
        case 2:
            _potion_effect(POT_CONFUSION, 10);
            break;
        case 3:
            mpr("You feel saturated with unharnessed energies!");
            you.magic_contamination += random2avg(19, 3);
            break;
        }
        break;

    case 3:         // even nastier
        if (target->atype() == ACT_MONSTER)
            target->mutate(); // Polymorph the monster, if possible.

        switch (random2(3))
        {
        case 0:
            you_msg = "Your body is flooded with distortional energies!";
            mon_msg = "@The_monster@'s body is flooded with distortional "
                      "energies!";
            if (_ouch(3 + random2avg(18, 2)) && target->alive())
            {
                if (target->atype() == ACT_PLAYER)
                    you.magic_contamination += random2avg(35, 3);
            }
            break;

        case 1:
            // HACK: Avoid lethality before deleting mutation, since
            // afterwards a message would already have been given.
            if (lethality_margin > 0
                && (you.hp - lethality_margin) <= 27
                && avoid_lethal(you.hp))
            {
                return;
            }

            if (target->atype() == ACT_PLAYER)
            {
                you_msg = "You feel very strange.";
                delete_mutation(RANDOM_MUTATION, true, false,
                                lethality_margin > 0);
            }
            _ouch(5 + random2avg(23, 2));
            break;

        case 2:
            // HACK: Avoid lethality before giving mutation, since
            // afterwards a message would already have been given.
            if (lethality_margin > 0
                && (you.hp - lethality_margin) <= 27
                && avoid_lethal(you.hp))
            {
                return;
            }

            if (target->atype() == ACT_PLAYER)
            {
                you_msg = "Your body is distorted in a weirdly horrible way!";
                const bool failMsg = !give_bad_mutation(true, false,
                                                        lethality_margin > 0);
                if (coinflip())
                    give_bad_mutation(failMsg, false, lethality_margin > 0);
            }
            _ouch(5 + random2avg(23, 2));
            break;
        }
        break;
    }
}

void MiscastEffect::_fire(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        switch (random2(10))
        {
        case 0:
            you_msg      = "Sparks fly from your @hands@!";
            mon_msg_seen = "Sparks fly from @the_monster@'s @hands@!";
            break;
        case 1:
            you_msg      = "The air around you burns with energy!";
            mon_msg_seen = "The air around @the_monster@ burns with energy!";
            break;
        case 2:
            you_msg      = "Wisps of smoke drift from your @hands@.";
            mon_msg_seen = "Wisps of smoke drift from @the_monster@'s @hands@.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            if (player_can_smell())
                all_msg = "You smell smoke.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 5:
            you_msg = "Heat runs through your body.";
            // Monster messages needed.
            break;
        case 6:
            you_msg = "You feel uncomfortably hot.";
            // Monster messages needed.
            break;
        case 7:
            you_msg      = "Lukewarm flames ripple over your body.";
            mon_msg_seen = "Dim flames ripple over @the_monster@'s body.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg = "You hear a sizzling sound.";
                msg_ch  = MSGCH_SOUND;
            }
            else if (target->atype() == ACT_PLAYER)
                you_msg = "You feel like you have heartburn.";
            break;
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "Smoke pours from your @hands@!";
            mon_msg_seen   = "Smoke pours from @the_monster@'s @hands@!";
            mon_msg_unseen = "Smoke appears out of nowhere!";

            do_msg();
            big_cloud(random_smoke_type(), kc, kt, target->pos(), 20,
                      7 + random2(7));
            break;

        case 1:
            you_msg      = "Flames sear your flesh.";
            mon_msg_seen = "Flames sear @the_monster@.";
            if (target->res_fire() < 0)
            {
                if (!_ouch(2 + random2avg(13, 2)))
                    return;
            }
            else
                do_msg();
            if (target->alive())
                target->expose_to_element(BEAM_FIRE, 3);
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "You are blasted with fire.";
            mon_msg_seen   = "@The_monster@ is blasted with fire.";
            mon_msg_unseen = "A flame briefly burns in thin air.";
            if (_ouch(5 + random2avg(29, 2), BEAM_FIRE) && target->alive())
                target->expose_to_element(BEAM_FIRE, 5);
            break;

        case 1:
            you_msg        = "You are caught in a fiery explosion!";
            mon_msg_seen   = "@The_monster@ is caught in a fiery explosion!";
            mon_msg_unseen = "Fire explodes from out of thin air!";

            beam.flavour = BEAM_FIRE;
            beam.type    = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 14);
            beam.name    = "explosion";
            beam.colour  = RED;

            _explosion();
            break;
        }
        break;

    case 3:         // considerably less harmless stuff
        switch (random2(3))
        {
        case 0:
            you_msg        = "You are blasted with searing flames!";
            mon_msg_seen   = "@The_monster@ is blasted with searing flames!";
            mon_msg_unseen = "A large flame burns hotly for a moment in the "
                             "thin air.";
            if (_ouch(9 + random2avg(33, 2), BEAM_FIRE) && target->alive())
                target->expose_to_element(BEAM_FIRE, 10);
            break;
        case 1:
            all_msg = "There is a sudden and violent explosion of flames!";

            beam.flavour = BEAM_FIRE;
            beam.type    = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 20);
            beam.name    = "fireball";
            beam.colour  = RED;
            beam.ex_size = coinflip() ? 1 : 2;

            _explosion();
            break;

        case 2:
        {
            you_msg      = "You are covered in liquid flames!";
            mon_msg_seen = "@The_monster@ is covered in liquid flames!";
            do_msg();

            if (target->atype() == ACT_PLAYER)
                you.duration[DUR_LIQUID_FLAMES] += random2avg(7, 3) + 1;
            else
            {
                mon_target->add_ench(mon_enchant(ENCH_STICKY_FLAME,
                    std::min(4, 1 + random2(mon_target->hit_dice) / 2), kc));
            }
            break;
        }
        }
        break;
    }
}

void MiscastEffect::_ice(int severity)
{
    const dungeon_feature_type feat = grd(target->pos());

    const bool frostable_feat =
        (feat == DNGN_FLOOR || grid_altar_god(feat) != GOD_NO_GOD
         || grid_is_staircase(feat) || grid_is_water(feat));

    const std::string feat_name = (feat == DNGN_FLOOR ? "the " : "") +
        feature_description(target->pos(), false, DESC_NOCAP_THE);

    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (frostable_feat ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            you_msg      = "You shiver with cold.";
            mon_msg_seen = "@The_monster@ shivers with cold.";
            break;
        case 1:
            you_msg = "A chill runs through your body.";
            // Monster messages needed.
            break;
        case 2:
            you_msg        = "Wisps of condensation drift from your @hands@.";
            mon_msg_seen   = "Wisps of condensation drift from @the_monster@'s "
                             "@hands@.";
            mon_msg_unseen = "Wisps of condensation drift in the air.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg      = "Your @hands@ feel@hand_conj@ numb with cold.";
            // Monster messages needed.
            break;
        case 5:
            you_msg = "A chill runs through your body.";
            // Monster messages needed.
            break;
        case 6:
            you_msg = "You feel uncomfortably cold.";
            // Monster messages needed.
            break;
        case 7:
            you_msg      = "Frost covers your body.";
            mon_msg_seen = "Frost covers @the_monster@'s body.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg = "You hear a crackling sound.";
                msg_ch  = MSGCH_SOUND;
            }
            else if (target->atype() == ACT_PLAYER)
                you_msg = "A snowflake lands on your nose.";
            break;
        case 10:
            if (grid_is_water(feat))
                all_msg  = "A thin layer of ice forms on " + feat_name;
            else
                all_msg  = "Frost spreads across " + feat_name;
            break;
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            mpr("You feel extremely cold.");
            // Monster messages needed.
            break;
        case 1:
            you_msg      = "You are covered in a thin layer of ice.";
            mon_msg_seen = "@The_monster@ is covered in a thin layer of ice.";
            if (target->res_cold() < 0)
            {
                if (!_ouch(4 + random2avg(5, 2)))
                    return;
            }
            else
                do_msg();
            if (target->alive())
                target->expose_to_element(BEAM_COLD, 2);
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg = "Heat is drained from your body.";
            // Monster messages needed.
            if (_ouch(5 + random2(6) + random2(7), BEAM_COLD) && target->alive())
                target->expose_to_element(BEAM_COLD, 4);
            break;

        case 1:
            you_msg        = "You are caught in an explosion of ice and frost!";
            mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "ice and frost!";
            mon_msg_unseen = "Ice and frost explode from out of thin air!";

            beam.flavour = BEAM_COLD;
            beam.type    = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 11);
            beam.name    = "explosion";
            beam.colour  = WHITE;

            _explosion();
            break;
        }
        break;

    case 3:         // less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg      = "You are blasted with ice!";
            mon_msg_seen = "@The_monster@ is blasted with ice!";
            if (_ouch(9 + random2avg(23, 2), BEAM_ICE) && target->alive())
                target->expose_to_element(BEAM_COLD, 9);
            break;
        case 1:
            you_msg        = "Freezing gasses pour from your @hands@!";
            mon_msg_seen   = "Freezing gasses pour from @the_monster@'s "
                             "@hands@!";

            do_msg();
            big_cloud(CLOUD_COLD, kc, kt, target->pos(), 20, 8 + random2(4));
            break;
        }
        break;
    }
}

static bool _on_floor(actor* target)
{
    return (!target->airborne() && grd(target->pos()) == DNGN_FLOOR);
}

void MiscastEffect::_earth(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
    case 1:
        num = 11 + (_on_floor(target) ? 2 : 0);
        switch (random2(num))
        {
        case 0:
            you_msg = "You feel earthy.";
            // Monster messages needed.
            break;
        case 1:
            you_msg        = "You are showered with tiny particles of grit.";
            mon_msg_seen   = "@The_monster@ is showered with tiny particles "
                             "of grit.";
            mon_msg_unseen = "Tiny particles of grit hang in the air for a "
                             "moment.";
            break;
        case 2:
            you_msg        = "Sand pours from your @hands@.";
            mon_msg_seen   = "Sand pours from @the_monster@'s @hands@.";
            mon_msg_unseen = "Sand pours from out of thin air.";
            break;
        case 3:
            you_msg = "You feel a surge of energy from the ground.";
            // Monster messages needed.
            break;
        case 4:
            if (neither_end_silenced())
            {
                all_msg = "You hear a distant rumble.";
                msg_ch  = MSGCH_SOUND;
            }
            else if (target->atype() == ACT_PLAYER)
                you_msg = "You sympathise with the stones.";
            break;
        case 5:
            you_msg = "You feel gritty.";
            // Monster messages needed.
            break;
        case 6:
            you_msg      = "You feel momentarily lethargic.";
            mon_msg_seen = "@The_monster@ looks momentarily listless.";
            break;
        case 7:
            you_msg        = "Motes of dust swirl before your eyes.";
            mon_msg_seen   = "Motes of dust swirl around @the_monster@.";
            mon_msg_unseen = "Motes of dust swirl around in the air.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
        {
            bool               pluralized = true;
            std::string        feet       = you.foot_name(true, &pluralized);
            std::ostringstream str;

            str << "Your " << feet << (pluralized ? " feel" : " feels")
                << " warm.";

            you_msg = str.str();
            // Monster messages needed.
            break;
        }
        case 10:
            if (target->cannot_move())
            {
                you_msg      = "You briefly vibrate.";
                mon_msg_seen = "@The_monster@ briefly vibrates.";
            }
            else
            {
                you_msg      = "You momentarily stiffen.";
                mon_msg_seen = "@The_monster@ momentarily stiffens.";
            }
            break;
        case 11:
            all_msg = "The floor vibrates.";
            break;
        case 12:
            all_msg = "The floor shifts beneath you alarmingly!";
            break;
        }
        do_msg();
        break;

    case 2:         // slightly less harmless stuff
        switch (random2(1))
        {
        case 0:
            switch (random2(3))
            {
            case 0:
                you_msg        = "You are hit by flying rocks!";
                mon_msg_seen   = "@The_monster@ is hit by flying rocks!";
                mon_msg_unseen = "Flying rocks appear out of thin air!";
                break;
            case 1:
                you_msg        = "You are blasted with sand!";
                mon_msg_seen   = "@The_monster@ is blasted with sand!";
                mon_msg_unseen = "A miniature sandstorm briefly appears!";
                break;
            case 2:
                you_msg        = "Rocks fall onto you out of nowhere!";
                mon_msg_seen   = "Rocks fall onto @the_monster@ out of "
                                 "nowhere!";
                mon_msg_unseen = "Rocks fall out of nowhere!";
                break;
            }
            _ouch(random2avg(13, 2) + 10 - random2(1 + target->armour_class()));
            break;
        }
        break;

    case 3:         // less harmless stuff
        switch (random2(1))
        {
        case 0:
            you_msg        = "You are caught in an explosion of flying "
                             "shrapnel!";
            mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "flying shrapnel!";
            mon_msg_unseen = "Flying shrapnel explodes from thin air!";

            beam.flavour = BEAM_FRAG;
            beam.type    = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 15);
            beam.name    = "explosion";
            beam.colour  = CYAN;

            if (one_chance_in(5))
                beam.colour = BROWN;
            if (one_chance_in(5))
                beam.colour = LIGHTCYAN;

            _explosion();
            break;
        }
        break;
    }
}

void MiscastEffect::_air(int severity)
{
    int num;
    switch (severity)
    {
    case 0:         // just a harmless message
        num = 10 + (_has_hair(target) ? 1 : 0);
        switch (random2(num))
        {
        case 0:
            you_msg = "Ouch! You gave yourself an electric shock.";
            mon_msg = "Ouch! You gave yourself an electric shock.";
            break;
        case 1:
            you_msg      = "You feel momentarily weightless.";
            mon_msg_seen = "@The_monster@ bobs in the air for a moment.";
            break;
        case 2:
            you_msg      = "Wisps of vapour drift from your @hands@.";
            mon_msg_seen = "Wisps of vapour drift from @the_monster@'s "
                           "@hands@.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg = "You feel electric!";
            // Monster messages needed.
            break;
        case 5:
        {
            bool pluralized = true;
            if (!hand_str.empty())
                pluralized = can_plural_hand;
            else
                target->hand_name(true, &pluralized);

            if (pluralized)
            {
                you_msg      = "Sparks of electricity dance between your "
                               "@hands@.";
                mon_msg_seen = "Sparks of electricity dance between "
                               "@the_monster@'s @hands@.";
            }
            else
            {
                you_msg      = "Sparks of electricity dance over your "
                               "@hand@.";
                mon_msg_seen = "Sparks of electricity dance over "
                               "@the_monster@'s @hand@.";
            }
            break;
        }
        case 6:
            you_msg      = "You are blasted with air!";
            mon_msg_seen = "@The_monster@ is blasted with air!";
            break;
        case 7:
            if (neither_end_silenced())
            {
                all_msg = "You hear a whooshing sound.";
                msg_ch  = MSGCH_SOUND;
            }
            else if (player_can_smell())
                all_msg = "You smell ozone.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg = "You hear a crackling sound.";
                msg_ch  = MSGCH_SOUND;
            }
            else if (player_can_smell())
                all_msg = "You smell something musty.";
            else if (you.species == SP_MUMMY)
                you_msg = "Your bandages flutter.";
            break;
        case 10:
        {
            // Player only (for now).
            bool plural;
            std::string hair = _hair_str(target, plural);
            you_msg = make_stringf("Your %s stand%s on end.", hair.c_str(),
                                   plural ? "" : "s");
            break;
        }
        }
        do_msg();
        break;
    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg      = "There is a short, sharp shower of sparks.";
            mon_msg_seen = "@The_monster@ is briefly showered in sparks.";
            break;
        case 1:
            if (silenced(you.pos()))
            {
               you_msg        = "The wind whips around you!";
               mon_msg_seen   = "The wind whips around @the_monster@!";
               mon_msg_unseen = "The wind whips!";
            }
            else
            {
               you_msg        = "The wind howls around you!";
               mon_msg_seen   = "The wind howls around @the_monster@!";
               mon_msg_unseen = "The wind howls!";
            }
            break;
        }
        do_msg();
        break;

    case 2:         // rather less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg = "Electricity courses through your body.";
            // Monster messages needed.
            _ouch(4 + random2avg(9, 2), BEAM_ELECTRICITY);
            break;
        case 1:
            you_msg        = "Noxious gasses pour from your @hands@!";
            mon_msg_seen   = "Noxious gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Noxious gasses appear from out of thin air!";

            do_msg();
            big_cloud(CLOUD_STINK, kc, kt, target->pos(), 20, 9 + random2(4));
            break;
        }
        break;

    case 3:         // musch less harmless stuff
        switch (random2(2))
        {
        case 0:
            you_msg        = "You are caught in an explosion of electrical "
                             "discharges!";
            mon_msg_seen   = "@The_monster@ is caught in an explosion of "
                             "electrical discharges!";
            mon_msg_unseen = "Electrical discharges explode from out of "
                             "thin air!";

            beam.flavour = BEAM_ELECTRICITY;
            beam.type    = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage  = dice_def(3, 8);
            beam.name    = "explosion";
            beam.colour  = LIGHTBLUE;
            beam.ex_size = one_chance_in(4) ? 1 : 2;

            _explosion();
            break;
        case 1:
            you_msg        = "Venomous gasses pour from your @hands@!";
            mon_msg_seen   = "Venomous gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Venomous gasses pour forth from the thin air!";

            do_msg();
            big_cloud(CLOUD_POISON, kc, kt, target->pos(), 20, 8 + random2(5));
            break;
        }
    }
}

// XXX MATT
void MiscastEffect::_poison(int severity)
{
    switch (severity)
    {
    case 0:         // just a harmless message
        switch (random2(10))
        {
        case 0:
            you_msg = "You feel mildly nauseous.";
            // Monster messages needed.
            break;
        case 1:
            you_msg = "You feel slightly ill.";
            // Monster messages needed.
            break;
        case 2:
            you_msg      = "Wisps of poison gas drift from your @hands@.";
            mon_msg_seen = "Wisps of poison gas drift from @the_monster@'s "
                           "@hands@.";
            break;
        case 3:
            you_msg = "You feel a strange surge of energy!";
            // Monster messages needed.
            break;
        case 4:
            you_msg = "You feel faint for a moment.";
            // Monster messages needed.
            break;
        case 5:
            you_msg = "You feel sick.";
            // Monster messages needed.
            break;
        case 6:
            you_msg = "You feel odd.";
            // Monster messages needed.
            break;
        case 7:
            you_msg = "You feel weak for a moment.";
            // Monster messages needed.
            break;
        case 8:
            // Set nothing; canned_msg(MSG_NOTHING_HAPPENS) will be taken
            // care of elsewhere.
            break;
        case 9:
            if (neither_end_silenced())
            {
                all_msg = "You hear a slurping sound.";
                msg_ch  = MSGCH_SOUND;
            }
            else if (you.species != SP_MUMMY)
                you_msg = "You taste almonds.";
            break;
        }
        do_msg();
        break;

    case 1:         // a bit less harmless stuff
        switch (random2(2))
        {
        case 0:
            if (target->res_poison() <= 0)
            {
                you_msg = "You feel sick.";
                // Monster messages needed.
                target->poison(act_source, 2 + random2(3));
            }
            do_msg();
            break;

        case 1:
            you_msg        = "Noxious gasses pour from your @hands@!";
            mon_msg_seen   = "Noxious gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Noxious gasses pour forth from the thin air!";
            place_cloud(CLOUD_STINK, target->pos(), 2 + random2(4), kc, kt );
            break;
        }
        break;

    case 2:         // rather less harmless stuff
        // Don't use last case for monsters.
        switch (random2(target->atype() == ACT_PLAYER ? 3 : 2))
        {
        case 0:
            if (target->res_poison() <= 0)
            {
                you_msg = "You feel very sick.";
                // Monster messages needed.
                target->poison(act_source, 3 + random2avg(9, 2));
            }
            do_msg();
            break;

        case 1:
            you_msg        = "Noxious gasses pour from your @hands@!";
            mon_msg_seen   = "Noxious gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Noxious gasses pour forth from the thin air!";

            do_msg();
            big_cloud(CLOUD_STINK, kc, kt, target->pos(), 20, 8 + random2(5));
            break;

        case 2:
            if (player_res_poison())
                canned_msg(MSG_NOTHING_HAPPENS);
            else
                _lose_stat(STAT_RANDOM, 1);
            break;
        }
        break;

    case 3:         // less harmless stuff
        // Don't use last case for monsters.
        switch (random2(target->atype() == ACT_PLAYER ? 3 : 2))
        {
        case 0:
            if (target->res_poison() <= 0)
            {
                you_msg = "You feel incredibly sick.";
                // Monster messages needed.
                target->poison(act_source, 10 + random2avg(19, 2));
            }
            do_msg();
            break;
        case 1:
            you_msg        = "Venomous gasses pour from your @hands@!";
            mon_msg_seen   = "Venomous gasses pour from @the_monster@'s "
                             "@hands@!";
            mon_msg_unseen = "Venomous gasses pour forth from the thin air!";

            do_msg();
            big_cloud(CLOUD_POISON, kc, kt, target->pos(), 20, 7 + random2(7));
            break;
        case 2:
            if (player_res_poison())
                canned_msg(MSG_NOTHING_HAPPENS);
            else
                _lose_stat(STAT_RANDOM, 1 + random2avg(5, 2));
            break;
        }
        break;
    }
}

const char* failure_rate_to_string( int fail )
{
    return
        (fail == 100) ? "Useless" : // 0% success chance
        (fail > 77) ? "Terrible"  : // 0-5%
        (fail > 71) ? "Cruddy"    : // 5-10%
        (fail > 64) ? "Bad"       : // 10-20%
        (fail > 59) ? "Very Poor" : // 20-30%
        (fail > 50) ? "Poor"      : // 30-50%
        (fail > 40) ? "Fair"      : // 50-70%
        (fail > 35) ? "Good"      : // 70-80%
        (fail > 28) ? "Very Good" : // 80-90%
        (fail > 22) ? "Great"     : // 90-95%
        (fail >  0) ? "Excellent" : // 95-100%
        "Perfect";                  // 100%
}

static unsigned int _breakpoint_rank(int val, const int breakpoints[],
                            unsigned int num_breakpoints)
{
    unsigned int result = 0;
    while (result < num_breakpoints && val >= breakpoints[result])
        ++result;
    return result;
}

const char* spell_hunger_string( spell_type spell )
{
    if ( you.is_undead == US_UNDEAD )
        return "N/A";

    const int hunger = spell_hunger(spell);
    const char* hunger_descriptions[] = {
        "None", "Grape", "Apple", "Choko", "Ration"
    };
    const int breakpoints[] = { 1, 25, 150, 500 };
    return (hunger_descriptions[_breakpoint_rank(hunger, breakpoints,
                                                 ARRAYSZ(breakpoints))]);
}

int spell_power_colour(spell_type spell)
{
    const int powercap = spell_power_cap(spell);
    if ( powercap == 0 )
        return DARKGREY;
    const int power = calc_spell_power(spell, true);
    if ( power >= powercap )
        return WHITE;
    if ( power * 3 < powercap )
        return RED;
    if ( power * 3 < powercap * 2 )
        return YELLOW;
    return GREEN;
}

static int _power_to_barcount( int power )
{
    if (power == -1)
        return -1;

    const int breakpoints[] = { 5, 10, 15, 25, 35, 50, 75, 100, 150 };
    return (_breakpoint_rank(power, breakpoints, ARRAYSZ(breakpoints)) + 1);
}

int spell_power_bars( spell_type spell )
{
    const int cap = spell_power_cap(spell);
    if ( cap == 0 )
        return -1;
    const int power = std::min(calc_spell_power(spell, true), cap);
    return _power_to_barcount(power);
}

std::string spell_power_string(spell_type spell)
{
    const int numbars = spell_power_bars(spell);
    const int capbars = _power_to_barcount(spell_power_cap(spell));
    ASSERT( numbars <= capbars );
    if ( numbars < 0 )
        return "N/A";
    else
        return std::string(numbars, '#') + std::string(capbars - numbars, '.');
}

std::string spell_range_string(spell_type spell)
{
    const int cap = spell_power_cap(spell);
    const int power = calc_spell_power(spell, true);
    const int range = spell_range(spell, power, false);
    const int maxrange = spell_range(spell, cap, false);
    if (range < 0)
        return "N/A";
    else
        return std::string("@") + std::string(range, '.')
            + "<darkgrey>" + std::string(maxrange - range, '.')
            + "</darkgrey>";
}
