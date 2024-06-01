#include "AppHdr.h"

#include "ng-wanderer.h"

#include "item-prop.h"
#include "item-use.h"
#include "items.h"
#include "jobs.h"
#include "newgame.h"
#include "ng-setup.h"
#include "potion-type.h"
#include "randbook.h"
#include "random.h"
#include "skills.h"
#include "spl-book.h" // you_can_memorise
#include "spl-util.h"

static void _give_wanderer_weapon(skill_type wpn_skill, bool good_item)
{
    if (wpn_skill == SK_THROWING)
    {
        // good_item always assigns 7 ammo: 1-4 curare, the rest javelins
        if (good_item)
        {
            const int num_curare = 1 + random2(4);
            newgame_make_item(OBJ_MISSILES, MI_DART, num_curare,
                              0, SPMSL_CURARE);

            // Gives large rocks for large species, 2x boomerangs for small
            const int num_javelins = 7 - num_curare;
            give_throwing_ammo(num_javelins);
        }
        // Otherwise, we get 7-15 poisoned darts or 6-10 boomerangs.
        else
        {
            if (one_chance_in(3))
            {
                newgame_make_item(OBJ_MISSILES, MI_BOOMERANG,
                              4 + roll_dice(2, 3), 0, SPMSL_NORMAL);
            }
            else
            {
                newgame_make_item(OBJ_MISSILES, MI_DART, 5 + roll_dice(2, 5),
                              0, SPMSL_POISONED);
            }
        }
        return; // don't give a dagger as well
    }

    weapon_type sub_type;
    int plus = 0;
    bool upgrade_base = good_item && one_chance_in(3);
    int ego = SPWPN_NORMAL;

    // Now fill in the type according to the random wpn_skill.
    switch (wpn_skill)
    {
    case SK_SHORT_BLADES:
        sub_type = WPN_SHORT_SWORD;
        break;

    case SK_LONG_BLADES:
        sub_type = WPN_FALCHION;
        break;

    case SK_MACES_FLAILS:
        sub_type = WPN_MACE;
        break;

    case SK_AXES:
        sub_type = WPN_HAND_AXE;
        break;

    case SK_POLEARMS:
        sub_type = WPN_SPEAR;
        break;

    case SK_STAVES:
        sub_type = WPN_QUARTERSTAFF;
        break;

    case SK_RANGED_WEAPONS:
        sub_type = WPN_SLING;
        break;

    default:
        sub_type = WPN_DAGGER;
        break;
    }

    if (upgrade_base)
    {
        const weapon_type old_type = sub_type;
        sub_type = starting_weapon_upgrade(sub_type, you.char_class,
                                            you.species);
        // If we can't upgrade the type, give it a little plus.
        if (sub_type == old_type)
            plus = 2;
    }
    else if (good_item)
        plus = 2;


    newgame_make_item(OBJ_WEAPONS, sub_type, 1, plus, ego);
}

static void _assign_wanderer_stats(skill_type sk1, skill_type sk2,
                                    skill_type sk3)
{
    skill_type skills[] = {sk1, sk2, sk3};
    int str_count = 0;
    int dex_count = 0;
    int int_count = 0;

    for (int i = 0; i < (int)ARRAYSZ(skills); i++)
    {
        skill_type sk = skills[i];
        switch (sk)
        {
            case SK_AXES:
            case SK_MACES_FLAILS:
            case SK_ARMOUR:
                str_count++;
                break;

            case SK_SHORT_BLADES:
            case SK_LONG_BLADES:
            case SK_RANGED_WEAPONS:
            case SK_STAVES:
            case SK_DODGING:
            case SK_SHIELDS:
            case SK_STEALTH:
                dex_count++;
                break;

            case SK_POLEARMS:
            case SK_UNARMED_COMBAT:
            case SK_FIGHTING:
            case SK_EVOCATIONS:
            case SK_THROWING:
            case SK_SHAPESHIFTING:
                if (coinflip())
                    str_count++;
                else
                    dex_count++;
                break;

            case SK_SPELLCASTING:
            case SK_SUMMONINGS:
            case SK_NECROMANCY:
            case SK_TRANSLOCATIONS:
            case SK_ALCHEMY:
            case SK_CONJURATIONS:
            case SK_HEXES:
            case SK_FIRE_MAGIC:
            case SK_ICE_MAGIC:
            case SK_AIR_MAGIC:
            case SK_EARTH_MAGIC:
                int_count++;
                break;

            default:
                break;
        }
    }

    for (int i = 0; i < 12; i++)
    {
        const auto stat = random_choose_weighted(
                you.base_stats[STAT_STR] > 17 ? 1 : 2 + 2*str_count, STAT_STR,
                you.base_stats[STAT_INT] > 17 ? 1 : 2 + 2*int_count, STAT_INT,
                you.base_stats[STAT_DEX] > 17 ? 1 : 2 + 2*dex_count, STAT_DEX);
            you.base_stats[stat]++;
    }
}

static skill_type _apt_weighted_choice(const skill_type * skill_array,
                                       unsigned arr_size)
{
    int total_apt = 0;

    for (unsigned i = 0; i < arr_size; ++i)
    {
        int reciprocal_apt = 100 / species_apt_factor(skill_array[i]);
        total_apt += reciprocal_apt;
    }

    unsigned probe = random2(total_apt);
    unsigned region_covered = 0;

    for (unsigned i = 0; i < arr_size; ++i)
    {
        int reciprocal_apt = 100 / species_apt_factor(skill_array[i]);
        region_covered += reciprocal_apt;

        if (probe < region_covered)
            return skill_array[i];
    }

    return NUM_SKILLS;
}

static skill_type _wanderer_role_skill_select(bool defense)
{
    skill_type skill = NUM_SKILLS;
    const skill_type offense_skills[] =
        { SK_AXES, SK_MACES_FLAILS, SK_RANGED_WEAPONS, SK_POLEARMS,
          SK_SHORT_BLADES, SK_LONG_BLADES, SK_STAVES, SK_UNARMED_COMBAT,
          SK_SUMMONINGS, SK_NECROMANCY, SK_TRANSLOCATIONS,
          SK_ALCHEMY, SK_CONJURATIONS,
          SK_HEXES, SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_SPELLCASTING,
          SK_AIR_MAGIC, SK_EARTH_MAGIC, SK_SHAPESHIFTING, SK_FIGHTING };

    int offense_size = ARRAYSZ(offense_skills);

    const skill_type physical_skills[] =
        { SK_AXES, SK_MACES_FLAILS, SK_RANGED_WEAPONS, SK_POLEARMS,
          SK_SHORT_BLADES, SK_LONG_BLADES, SK_STAVES, SK_UNARMED_COMBAT,
          SK_SHAPESHIFTING, SK_FIGHTING };

    int physical_size = ARRAYSZ(physical_skills);

    const skill_type defense_skills[] =
        { SK_FIGHTING, SK_DODGING, SK_ARMOUR, SK_SHIELDS, SK_EVOCATIONS,
          SK_STEALTH, SK_THROWING };

    int defense_size = ARRAYSZ(defense_skills);

    if (defense)
        skill = _apt_weighted_choice(defense_skills, defense_size);
    // reduce the chance of a spell felid a bit
    else if (you.has_mutation(MUT_NO_GRASPING) && one_chance_in(3))
        skill = _apt_weighted_choice(physical_skills, physical_size);
    else
        skill = _apt_weighted_choice(offense_skills, offense_size);

    return skill;
}

static void _setup_starting_skills(skill_type sk1, skill_type sk2,
                                   skill_type sk3, int levels)
{
    ASSERT(levels > 4);

    int martial = 0;
    int magical = 0;
    set<skill_type> skills = {sk1, sk2, sk3};


    // give a baseline of our points away to our "role" skills, if they weren't
    // blanked by fallback, and decide the weight of martial and magical skills
    you.skills[sk1] = 1;
    levels -= 1;
    for (auto sk : skills)
    {
        if (sk <= SK_LAST_MUNDANE)
            martial++;
        else if (sk > SK_LAST_MUNDANE && sk <= SK_LAST_MAGIC)
            magical++;
        if (sk != SK_NONE)
        {
            you.skills[sk]++;
            levels--;
        }
    }

    skill_type selected = SK_NONE;

    do
    {
        selected = random_choose_weighted(8, sk1,
                                          5, sk2,
                                          5, sk3,
                                          1 + 2 * martial, SK_FIGHTING,
                                          1 + martial, SK_ARMOUR,
                                          2 + magical / 2, SK_DODGING,
                                          1, SK_THROWING,
                                          2, SK_STEALTH,
                                          1 + magical, SK_SPELLCASTING,
                                          2, SK_EVOCATIONS,
                                          1, SK_INVOCATIONS,
                                          2, SK_SHAPESHIFTING);

        if (!is_useless_skill(selected) && selected != SK_NONE
            && you.skills[selected] < 5)
        {
            you.skills[selected]++;
            levels--;
        }
    }
    while (levels > 0);
}

/**
 * Can we include the given spell in our themed spellbook?
 *
 * Guarantees exactly two spells of total spell level 4.
 * (I.e., 2+2 or 1+3)
 *
 * XXX: strongly consider caching this - currently we're n^2 over all spells,
 * which seems excessive.
 *
 * @param discipline_1      The first spellschool of the book.
 * @param discipline_2      The second spellschool of the book.
 * @param agent             The entity creating the book; possibly a god.
 * @param prev              A list of spells already chosen for the book.
 * @param spell             The spell to be filtered.
 * @return                  Whether the spell can be included.
 */
static bool exact_level_spell_filter(spschool discipline_1,
                                     spschool discipline_2,
                                     int agent,
                                     const vector<spell_type> &prev,
                                     spell_type spell)
{
    if (!basic_themed_spell_filter(discipline_1, discipline_2, agent, prev,
                                   spell))
    {
        return false;
    }

    if (!you_can_memorise(spell))
        return false;

    static const int TOTAL_LEVELS = 4;

    const int spell_level = spell_difficulty(spell);
    if (prev.size())
        return TOTAL_LEVELS == spell_level + spell_difficulty(prev[0]);

    // we need to check to see there is some possible second spell; otherwise
    // we could be walking into a trap, if we select e.g. a level 2 spell when
    // there's only one player-castable level 2 spell in the school.
    const vector<spell_type> incl_spell = {spell};
    for (int s = 0; s < NUM_SPELLS; ++s)
    {
        const spell_type second_spell = static_cast<spell_type>(s);
        if (exact_level_spell_filter(discipline_1, discipline_2,
                                     agent, incl_spell, second_spell))
        {
            return true;
        }
    }

    return false;
}

static vector<spell_type> _give_wanderer_major_spells(skill_type skill,
                                                      int num, int max_level)
{
    spschool school = skill2spell_type(skill);
    vector<spell_type> spells;
    set<spell_type> spellset;

    for (int i = 0; i < num; i++)
    {
        spell_type next_spell = SPELL_NO_SPELL;
        int seen = 0;
        for (int s = 0; s < NUM_SPELLS; ++s)
        {
            const spell_type spell = static_cast<spell_type>(s);

            if (!is_player_book_spell(spell)
                || spell_is_useless(spell, false)
                || spellset.find(spell) != spellset.end()
                || (school != spschool::none
                    && !(get_spell_disciplines(spell) & school)))
            {
                continue;
            }

            const int limit = i == 0 ? 1 : max_level;
            if (spell_difficulty(spell) > limit)
                continue;

            if (one_chance_in(++seen))
                next_spell = spell;
        }

        // this could happen if a spell school has no level 1 spell, or has
        // less than 4 learnable spells for the player's species
        if (next_spell != SPELL_NO_SPELL)
        {
            spellset.insert(next_spell);
            spells.push_back(next_spell);
        }
    }

    library_add_spells(spells, true);
    return spells;
}

// Give the wanderer two spells of total level 4.
// The theme of these spells is the school of the chosen skill.
static vector<spell_type> _give_wanderer_minor_spells(skill_type skill)
{
    // Doing a rejection loop for this because I am lazy.
    while (skill == SK_SPELLCASTING)
    {
        int value = SK_LAST_MAGIC - SK_FIRST_MAGIC_SCHOOL + 1;
        skill = skill_type(SK_FIRST_MAGIC_SCHOOL + random2(value));
    }

    spschool school = skill2spell_type(skill);
    spschool discipline_1 = forced_book_theme(school)();
    spschool discipline_2 = forced_book_theme(school)();
    vector<spell_type> spells;
    theme_book_spells(discipline_1, discipline_2,
                      exact_level_spell_filter,
                      IT_SRC_NONE, 2, spells);
    library_add_spells(spells, true);
    return spells;
}

/**
 * Create a consumable as a "good item".
 *
 * Shouldn't ever create an useless consumable for the player's species.
 * e.g., potions for Mu, heal wounds for VS, blinking for Fo.
 */
static void _good_potion_or_scroll()
{
    // vector of weighted {object_class_type, subtype} pairs
    // xxx: could we use is_useless_item here? (not without dummy items...?)
    const int ally_scr_type = item_for_set(ITEM_SET_ALLY_SCROLLS);
    const vector<pair<pair<object_class_type, int>, int>> options = {
        { { OBJ_SCROLLS, SCR_FEAR }, 4 },
        { { OBJ_SCROLLS, ally_scr_type }, 1 },
        { { OBJ_SCROLLS, SCR_BLINKING },
            you.stasis() ? 0 : 4 },
        { { OBJ_POTIONS, POT_HEAL_WOUNDS },
            (you.has_mutation(MUT_NO_DRINK)
             || you.get_mutation_level(MUT_NO_POTION_HEAL) >= 2) ? 0 : 2 },
        { { OBJ_POTIONS, POT_HASTE },
            (you.has_mutation(MUT_NO_DRINK)
             || you.stasis()) ? 0 : 2 },
        { { OBJ_POTIONS, POT_BERSERK_RAGE },
            (you.stasis()
             || you.is_lifeless_undead(false)) ? 0 : 2 },
        { { OBJ_POTIONS, POT_MIGHT },
            you.has_mutation(MUT_NO_DRINK) ? 0 : 1 },
        { { OBJ_POTIONS, POT_BRILLIANCE },
            you.has_mutation(MUT_NO_DRINK) ? 0 : 1 },
        { { OBJ_POTIONS, POT_INVISIBILITY },
            you.has_mutation(MUT_NO_DRINK) ? 0 : 1 },
    };

    const pair<object_class_type, int> *option
        = random_choose_weighted(options);
    ASSERT(option);
    newgame_make_item(option->first, option->second);
}

/**
 * Make a 'decent' consumable for a wanderer to start with.
 *
 * Shouldn't ever create a completely useless item for the player's species.
 */
static void _decent_potion_or_scroll()
{
    // vector of weighted {object_class_type, subtype} pairs
    // xxx: could we use is_useless_item here? (not without dummy items...?)
    const vector<pair<pair<object_class_type, int>, int>> options = {
        { { OBJ_SCROLLS, SCR_TELEPORTATION },
            you.stasis() ? 0 : 6 },
        { { OBJ_SCROLLS, SCR_FOG }, 6 },
        { { OBJ_SCROLLS, SCR_VULNERABILITY }, 2 },
        { { OBJ_SCROLLS, SCR_SILENCE }, 2 },
        { { OBJ_POTIONS, POT_CURING },
            you.has_mutation(MUT_NO_DRINK) ? 0 : 5 },
        { { OBJ_POTIONS, POT_LIGNIFY },
            you.is_lifeless_undead(false) ? 0 : 5 },
        { { OBJ_POTIONS, POT_ATTRACTION },
            you.has_mutation(MUT_NO_DRINK) ? 0 : 5 },
        { { OBJ_POTIONS, POT_MUTATION },
            you.is_lifeless_undead(false) ? 0 : 1 },
    };

    const pair<object_class_type, int> *option
        = random_choose_weighted(options);
    ASSERT(option);
    newgame_make_item(option->first, option->second);
}

// Create a random wand or misc evoker in the inventory.
static void _wanderer_random_evokable()
{
    if (one_chance_in(3))
    {
        const auto area_evoker_type =
            (misc_item_type)item_for_set(ITEM_SET_AREA_MISCELLANY);
        const auto ally_evoker_type =
            (misc_item_type)item_for_set(ITEM_SET_ALLY_MISCELLANY);
        const auto control_evoker_type =
            (misc_item_type)item_for_set(ITEM_SET_CONTROL_MISCELLANY);
        misc_item_type selected_evoker =
              random_choose(ally_evoker_type, control_evoker_type,
                            MISC_PHANTOM_MIRROR, area_evoker_type,
                            MISC_LIGHTNING_ROD);

        newgame_make_item(OBJ_MISCELLANY, selected_evoker, 1);
    }
    else
    {
        const auto hex_wand_type = (wand_type)item_for_set(ITEM_SET_HEX_WANDS);
        const auto beam_wand_type = (wand_type)item_for_set(ITEM_SET_BEAM_WANDS);
        const auto blast_wand_type = (wand_type)item_for_set(ITEM_SET_BLAST_WANDS);
        wand_type selected_wand =
              random_choose(hex_wand_type, WAND_MINDBURST, WAND_POLYMORPH,
                            WAND_FLAME, blast_wand_type, beam_wand_type);
        int charges;
        switch (selected_wand)
        {
        // completely nuts
        case WAND_ACID:
        case WAND_LIGHT:
        case WAND_QUICKSILVER:
        case WAND_ICEBLAST:
        case WAND_ROOTS:
        case WAND_WARPING:
            charges = 2 + random2(3);
        break;

        // extremely strong
        case WAND_CHARMING:
        case WAND_PARALYSIS:
        case WAND_MINDBURST:
            charges = 5 + random2avg(16, 3);
            break;

        // a little less strong
        case WAND_FLAME:
        case WAND_POLYMORPH:
            charges = 8 + random2avg(16, 3);
            break;

        default:
            charges = 15;
            break;
        }

        newgame_make_item(OBJ_WANDS, selected_wand, 1, charges);
    }
}

// Create a random low-level talisman in the inventory.
static void _wanderer_random_talisman()
{
    talisman_type selected_talisman =
        coinflip() ? TALISMAN_BEAST : TALISMAN_FLUX;

    newgame_make_item(OBJ_TALISMANS, selected_talisman);
}

static void _give_wanderer_aux_armour(int plus = 0)
{
    vector<armour_type> auxs = { ARM_HAT, ARM_GLOVES, ARM_BOOTS, ARM_CLOAK };

    armour_type choice = NUM_ARMOURS;
    int seen = 0;
    item_def dummy;
    dummy.base_type = OBJ_ARMOUR;

    for (armour_type aux : auxs)
    {
        dummy.sub_type = aux;
        if (!can_wear_armour(dummy, false, true))
            continue;

        if (one_chance_in(++seen))
            choice = aux;
    }

    // newgame make item will fix this up for us, but we needed our resivoir
    // sample to work for hat users too
    if (choice == ARM_HAT)
        choice = ARM_HELMET;

    if (choice != NUM_ARMOURS)
        newgame_make_item(OBJ_ARMOUR, choice, 1, plus);
}

static vector<spell_type> _wanderer_good_equipment(skill_type & skill)
{
    const skill_type combined_weapon_skills[] =
        { SK_AXES, SK_MACES_FLAILS, SK_RANGED_WEAPONS, SK_POLEARMS,
          SK_SHORT_BLADES, SK_LONG_BLADES, SK_STAVES, SK_UNARMED_COMBAT };

    int total_weapons = ARRAYSZ(combined_weapon_skills);

    // Normalise the input type.
    if (skill == SK_FIGHTING)
        skill =  _apt_weighted_choice(combined_weapon_skills, total_weapons);

    switch (skill)
    {
    case SK_MACES_FLAILS:
    case SK_AXES:
    case SK_POLEARMS:
    case SK_THROWING:
    case SK_SHORT_BLADES:
    case SK_LONG_BLADES:
    case SK_RANGED_WEAPONS:
    case SK_STAVES:
        _give_wanderer_weapon(skill, true);
        break;

    case SK_ARMOUR:
        if (you_can_wear(EQ_BODY_ARMOUR) != true)
            newgame_make_item(OBJ_ARMOUR, ARM_ACID_DRAGON_ARMOUR);
        else if (coinflip())
            newgame_make_item(OBJ_ARMOUR, ARM_SCALE_MAIL, 1, 2);
        else
            newgame_make_item(OBJ_ARMOUR, ARM_CHAIN_MAIL, 1, 0);
        break;

    case SK_DODGING:
        // +2 leather armour or +0 leather armour and also 2-4 nets
        if (coinflip())
        {
            if (!newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR, 1, 2))
                _give_wanderer_aux_armour(2);
        }
        else
        {
            if (!newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR))
                _give_wanderer_aux_armour();
            newgame_make_item(OBJ_MISSILES, MI_THROWING_NET, 2 + random2(3));
        }
        break;

    case SK_STEALTH:
        // +2 dagger and a good consumable or a +0 dagger and fancy darts
        if (coinflip())
        {
            newgame_make_item(OBJ_WEAPONS, WPN_DAGGER, 1, 2);
            _good_potion_or_scroll();
        }
        else
        {
            newgame_make_item(OBJ_WEAPONS, WPN_DAGGER, 1, 0);
            newgame_make_item(OBJ_MISSILES, MI_DART, 1 + coinflip(), 0,
                              SPMSL_BLINDING);
            newgame_make_item(OBJ_MISSILES, MI_DART, 1 + coinflip(), 0,
                              SPMSL_FRENZY);
        }
        break;


    case SK_SHIELDS:
        if (one_chance_in(3))
            newgame_make_item(OBJ_ARMOUR, ARM_BUCKLER, 1, 0, SPARM_PROTECTION);
        else
            newgame_make_item(OBJ_ARMOUR, ARM_BUCKLER, 1, 2);
        break;

    case SK_CONJURATIONS:
    case SK_SUMMONINGS:
    case SK_NECROMANCY:
    case SK_TRANSLOCATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_ALCHEMY:
    case SK_HEXES:
        return _give_wanderer_major_spells(skill, 3, 4);

    case SK_SPELLCASTING:
        return _give_wanderer_major_spells(skill, 4, 3);

    case SK_UNARMED_COMBAT:
    {
        // 2 random good potions/scrolls
        _good_potion_or_scroll();
        _good_potion_or_scroll();
        break;
    }

    case SK_EVOCATIONS:
        // Random wand or xp evoker
        _wanderer_random_evokable();
        break;

    case SK_SHAPESHIFTING:
        // Random low-level talisman
        _wanderer_random_talisman();
        break;

    default:
        break;
    }

    return vector<spell_type>{};
}

static vector<spell_type> _wanderer_decent_equipment(skill_type & skill,
                                                     set<skill_type> & gift_skills)
{
    // don't give a shield if we filled our hands already
    // or if we got a sling (likely to upgrade to a bow later)
    // (except for formicids, obviously)
    if (!you.has_mutation(MUT_QUADRUMANOUS)
        && skill == SK_SHIELDS
        && (!you.has_usable_offhand() || gift_skills.count(SK_RANGED_WEAPONS)))
    {
        skill = random_choose(SK_DODGING, SK_ARMOUR);
    }

    // or two handers if we have a shield (getting a 2h and a bow is ok)
    if (!you.has_mutation(MUT_QUADRUMANOUS)
        && gift_skills.count(SK_SHIELDS)
        && (skill == SK_STAVES || skill == SK_RANGED_WEAPONS))
    {
        skill = SK_FIGHTING;
    }

    // Don't give a gift from the same skill twice; just default to
    // a decent consumable
    if (gift_skills.count(skill))
        skill = SK_NONE;

    // don't give the player a second piece of armour
    if (gift_skills.count(SK_ARMOUR) && (skill == SK_DODGING
                                         || skill == SK_STEALTH)
        || (gift_skills.count(SK_DODGING) && (skill == SK_ARMOUR
                                              || skill == SK_STEALTH)))
    {
        skill = SK_NONE;
    }

    switch (skill)
    {
    case SK_MACES_FLAILS:
    case SK_AXES:
    case SK_POLEARMS:
    case SK_RANGED_WEAPONS:
    case SK_THROWING:
    case SK_STAVES:
    case SK_SHORT_BLADES:
    case SK_LONG_BLADES:
        _give_wanderer_weapon(skill, false);
        break;

    case SK_ARMOUR:
        // Dragon scales/tla is too good for "decent" quality
        // Just give an aux piece to On/Tr/Sp. Note: armour skill will later be
        // converted to dodging skill by reassess_starting_skills in this case.
        if (you_can_wear(EQ_BODY_ARMOUR) != true)
            _give_wanderer_aux_armour();
        else if (coinflip())
            newgame_make_item(OBJ_ARMOUR, ARM_RING_MAIL);
        else
            newgame_make_item(OBJ_ARMOUR, ARM_SCALE_MAIL);
        break;

    case SK_SHIELDS:
        newgame_make_item(OBJ_ARMOUR, ARM_BUCKLER);
        break;

    case SK_SPELLCASTING:
    case SK_CONJURATIONS:
    case SK_SUMMONINGS:
    case SK_NECROMANCY:
    case SK_TRANSLOCATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_ALCHEMY:
    case SK_HEXES:
        return _give_wanderer_minor_spells(skill);

    case SK_EVOCATIONS:
        newgame_make_item(OBJ_WANDS, coinflip() ? WAND_FLAME : WAND_POLYMORPH,
                          1, 3 + random2(5));
        break;

    case SK_SHAPESHIFTING:
        newgame_make_item(OBJ_TALISMANS, TALISMAN_BEAST);
        break;

    case SK_STEALTH:
    case SK_DODGING:
        _give_wanderer_aux_armour();
        break;

    case SK_FIGHTING:
        if (you.weapon())
        {
            you.weapon()->plus++;
            break;
        }
    // intentional fallthrough
    case SK_UNARMED_COMBAT:
    case SK_NONE:
        _decent_potion_or_scroll();
        break;

    default:
        break;
    }

    return vector<spell_type>{};
}

// We don't actually want to send adventurers wandering naked into the
// dungeon.
static void _wanderer_cover_equip_holes()
{
    if (you.equip[EQ_BODY_ARMOUR] == -1)
    {
        newgame_make_item(OBJ_ARMOUR,
                          you.strength() > you.intel() ? ARM_LEATHER_ARMOUR : ARM_ROBE);
    }

    // Give weapon, unless we started with some unarmed skill
    if (you.equip[EQ_WEAPON] == -1 && you.skills[SK_UNARMED_COMBAT] == 0)
    {
        newgame_make_item(OBJ_WEAPONS,
                          you.dex() > you.strength() ? WPN_DAGGER : WPN_CLUB);
    }
}

static void _add_spells(set<spell_type> &all_spells,
                        const vector<spell_type> &new_spells)
{
    all_spells.insert(new_spells.begin(), new_spells.end());
}

static void _handle_start_spells(const set<spell_type> &spells)
{
    if (you.has_mutation(MUT_INNATE_CASTER))
    {
        for (spell_type s : spells)
            if (you.spell_no < MAX_DJINN_SPELLS)
                add_spell_to_memory(s);
        return;
    }

    int lvl_1s = 0;
    spell_type to_memorise = SPELL_NO_SPELL;
    for (spell_type s : spells)
    {
        if (spell_difficulty(s) == 1)
        {
            ++lvl_1s;
            to_memorise = s;
        }
    }
    if (lvl_1s == 1 && !spell_is_useless(to_memorise, false, true))
        add_spell_to_memory(to_memorise);
}

// New style wanderers are supposed to be decent in terms of skill
// levels/equipment, but pretty randomised.
void create_wanderer()
{
    // intentionally create the subgenerator either way, so that this has the
    // same impact on the current main rng for all chars.
    rng::subgenerator wn_rng;
    if (you.char_class != JOB_WANDERER)
        return;
    // Keep track of what skills we got items from, mostly to prevent
    // giving a good and then a normal version of the same weapon.
    set<skill_type> gift_skills;

    // always give at least one "offense skill" and one "defence skill"
    skill_type gift_skill_1 = _wanderer_role_skill_select(one_chance_in(3));
    skill_type gift_skill_2 = _wanderer_role_skill_select(false);
    skill_type gift_skill_3 = _wanderer_role_skill_select(true);

    // assign remaining wanderer stat points according to gift skills
    _assign_wanderer_stats(gift_skill_1, gift_skill_2, gift_skill_3);

    // Wanderers get 1 good thing, a couple average things, and then
    // 1 last stage to fill any glaring equipment holes (no clothes,
    // etc.).

    set<spell_type> spells;
    _add_spells(spells, _wanderer_good_equipment(gift_skill_1));
    gift_skills.insert(gift_skill_1);

    _add_spells(spells, _wanderer_decent_equipment(gift_skill_2, gift_skills));
    gift_skills.insert(gift_skill_2);
    _add_spells(spells, _wanderer_decent_equipment(gift_skill_3, gift_skills));
    gift_skills.insert(gift_skill_3);

    // Give out an extra consumable, to ensure we have some kind of early game
    // tactical option.
    coinflip() ? _good_potion_or_scroll() : _decent_potion_or_scroll();

    // guarantee some spell levels if we get spells
    if (!spells.empty())
    {
        you.skills[SK_SPELLCASTING]++;
        _setup_starting_skills(gift_skill_1, gift_skill_2, gift_skill_3, 9);
    }
    else
        _setup_starting_skills(gift_skill_1, gift_skill_2, gift_skill_3, 10);

    _handle_start_spells(spells);

    _wanderer_cover_equip_holes();
}
