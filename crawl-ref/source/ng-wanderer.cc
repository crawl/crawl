#include "AppHdr.h"

#include "ng-wanderer.h"

#include "item-prop.h"
#include "ng-setup.h"
#include "potion-type.h"
#include "randbook.h"
#include "random.h"
#include "skills.h"
#include "spl-book.h" // you_can_memorise
#include "spl-util.h"

static void _give_wanderer_weapon(skill_type wpn_skill, int plus)
{
    if (wpn_skill == SK_THROWING)
    {
        // Plus is set if we are getting a good item. In that case, we
        // get curare here.
        if (plus)
        {
            newgame_make_item(OBJ_MISSILES, MI_DART, 1 + random2(4),
                              0, SPMSL_CURARE);
        }
        // Otherwise, we just get some poisoned darts.
        else
        {
            newgame_make_item(OBJ_MISSILES, MI_DART, 5 + roll_dice(2, 5),
                              0, SPMSL_POISONED);
        }
    }

    weapon_type sub_type;

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

    case SK_BOWS:
        sub_type = WPN_SHORTBOW;
        break;

    case SK_CROSSBOWS:
        sub_type = WPN_HAND_CROSSBOW;
        break;

    default:
        sub_type = WPN_DAGGER;
        break;
    }

    newgame_make_item(OBJ_WEAPONS, sub_type, 1, plus);

    if (sub_type == WPN_SHORTBOW)
        newgame_make_item(OBJ_MISSILES, MI_ARROW, 15 + random2avg(21, 5));
    else if (sub_type == WPN_HAND_CROSSBOW)
        newgame_make_item(OBJ_MISSILES, MI_BOLT, 15 + random2avg(21, 5));
}

// The overall role choice for wanderers is a weighted chance based on
// stats.
static stat_type _wanderer_choose_role()
{
    int total_stats = 0;
    for (int i = 0; i < NUM_STATS; ++i)
        total_stats += you.stat(static_cast<stat_type>(i));

    int target = random2(total_stats);

    stat_type role;

    if (target < you.strength())
        role = STAT_STR;
    else if (target < (you.dex() + you.strength()))
        role = STAT_DEX;
    else
        role = STAT_INT;

    return role;
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

static skill_type _wanderer_role_skill_select(stat_type selected_role,
                                              skill_type sk_1,
                                              skill_type sk_2)
{
    skill_type selected_skill = SK_NONE;

    switch (selected_role)
    {
    case STAT_DEX:
        // Duplicates are intentional.
        selected_skill = random_choose(SK_FIGHTING, SK_FIGHTING,
                                       SK_DODGING,
                                       SK_STEALTH,
                                       sk_1, sk_1);
        break;

    case STAT_STR:
        do
        {
            selected_skill = random_choose(SK_FIGHTING, sk_1, SK_ARMOUR);
        }
        while (is_useless_skill(selected_skill));
        break;

    case STAT_INT:
        selected_skill = random_choose(SK_SPELLCASTING, sk_1, sk_2);
        break;

    default:
        die("bad skill_type %d", selected_role);
    }

    if (selected_skill == NUM_SKILLS)
    {
        ASSERT(you.species == SP_FELID);
        selected_skill = SK_UNARMED_COMBAT;
    }

    return selected_skill;
}

static skill_type _wanderer_role_weapon_select(stat_type role)
{
    skill_type skill = NUM_SKILLS;
    const skill_type str_weapons[] =
        { SK_AXES, SK_MACES_FLAILS, SK_BOWS, SK_CROSSBOWS };

    int str_size = ARRAYSZ(str_weapons);

    const skill_type dex_weapons[] =
        { SK_SHORT_BLADES, SK_LONG_BLADES, SK_STAVES, SK_UNARMED_COMBAT,
          SK_POLEARMS };

    int dex_size = ARRAYSZ(dex_weapons);

    const skill_type casting_schools[] =
        { SK_SUMMONINGS, SK_NECROMANCY, SK_TRANSLOCATIONS,
          SK_TRANSMUTATIONS, SK_POISON_MAGIC, SK_CONJURATIONS,
          SK_HEXES, SK_FIRE_MAGIC, SK_ICE_MAGIC,
          SK_AIR_MAGIC, SK_EARTH_MAGIC };

    int casting_size = ARRAYSZ(casting_schools);

    switch ((int)role)
    {
    case STAT_STR:
        skill = _apt_weighted_choice(str_weapons, str_size);
        break;

    case STAT_DEX:
        skill = _apt_weighted_choice(dex_weapons, dex_size);
        break;

    case STAT_INT:
        skill = _apt_weighted_choice(casting_schools, casting_size);
        break;
    }

    return skill;
}

static void _wanderer_role_skill(stat_type role, int levels)
{
    skill_type weapon_type = _wanderer_role_weapon_select(role);
    skill_type spell2 = NUM_SKILLS;

    if (role == STAT_INT)
       spell2 = _wanderer_role_weapon_select(role);

    skill_type selected_skill = NUM_SKILLS;
    for (int i = 0; i < levels; ++i)
    {
        selected_skill = _wanderer_role_skill_select(role, weapon_type,
                                                     spell2);
        you.skills[selected_skill]++;
    }
}

// Select a random skill from all skills we have at least 1 level in.
static skill_type _weighted_skill_roll()
{
    int total_skill = 0;

    for (unsigned i = 0; i < NUM_SKILLS; ++i)
        total_skill += you.skills[i];

    int probe = random2(total_skill);
    int covered_region = 0;

    for (unsigned i = 0; i < NUM_SKILLS; ++i)
    {
        covered_region += you.skills[i];
        if (probe < covered_region)
            return skill_type(i);
    }

    return NUM_SKILLS;
}

static void _give_wanderer_book(skill_type skill)
{
    book_type book;
    switch (skill)
    {
    default:
    case SK_SPELLCASTING:
        book = BOOK_MINOR_MAGIC;
        break;

    case SK_CONJURATIONS:
        // minor magic should have only half the likelihood of conj
        book = random_choose(BOOK_MINOR_MAGIC,
                             BOOK_CONJURATIONS, BOOK_CONJURATIONS);
        break;

    case SK_SUMMONINGS:
        book = random_choose(BOOK_MINOR_MAGIC, BOOK_CALLINGS);
        break;

    case SK_NECROMANCY:
        book = BOOK_NECROMANCY;
        break;

    case SK_TRANSLOCATIONS:
        book = BOOK_SPATIAL_TRANSLOCATIONS;
        break;

    case SK_TRANSMUTATIONS:
        book = random_choose(BOOK_GEOMANCY, BOOK_CHANGES);
        break;

    case SK_FIRE_MAGIC:
        book = BOOK_FLAMES;
        break;

    case SK_ICE_MAGIC:
        book = BOOK_FROST;
        break;

    case SK_AIR_MAGIC:
        book = BOOK_AIR;
        break;

    case SK_EARTH_MAGIC:
        book = BOOK_GEOMANCY;
        break;

    case SK_POISON_MAGIC:
        book = BOOK_YOUNG_POISONERS;
        break;

    case SK_HEXES:
        book = BOOK_MALEDICT;
        break;
    }

    newgame_make_item(OBJ_BOOKS, book);
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

// Give the wanderer a randart book containing two spells of total level 4.
// The theme of the book is the spell school of the chosen skill.
static void _give_wanderer_minor_book(skill_type skill)
{
    // Doing a rejection loop for this because I am lazy.
    while (skill == SK_SPELLCASTING)
    {
        int value = SK_LAST_MAGIC - SK_FIRST_MAGIC_SCHOOL + 1;
        skill = skill_type(SK_FIRST_MAGIC_SCHOOL + random2(value));
    }

    spschool school = skill2spell_type(skill);

    item_def* item = newgame_make_item(OBJ_BOOKS, BOOK_RANDART_THEME);
    if (!item)
        return;

    build_themed_book(*item, exact_level_spell_filter,
                      forced_book_theme(school), 2);
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
    const vector<pair<pair<object_class_type, int>, int>> options = {
        { { OBJ_SCROLLS, SCR_FEAR }, 1 },
        { { OBJ_SCROLLS, SCR_BLINKING },
            you.species == SP_FORMICID ? 0 : 1 },
        { { OBJ_POTIONS, POT_HEAL_WOUNDS },
            (you.species == SP_MUMMY
             || you.species == SP_VINE_STALKER) ? 0 : 1 },
        { { OBJ_POTIONS, POT_HASTE },
            (you.species == SP_MUMMY
             || you.species == SP_FORMICID) ? 0 : 1 },
        { { OBJ_POTIONS, POT_BERSERK_RAGE },
            (you.species == SP_FORMICID
             || you.is_lifeless_undead(false)) ? 0 : 1},
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
            you.species == SP_FORMICID ? 0 : 1 },
        { { OBJ_POTIONS, POT_CURING },
            you.species == SP_MUMMY ? 0 : 1 },
        { { OBJ_POTIONS, POT_LIGNIFY },
            you.is_lifeless_undead(false) ? 0 : 1 },
    };

    const pair<object_class_type, int> *option
        = random_choose_weighted(options);
    ASSERT(option);
    newgame_make_item(option->first, option->second);
}

// Create a random wand in the inventory.
static void _wanderer_random_evokable()
{
    if (one_chance_in(3))
    {
        int selected_evoker =
              random_choose(MISC_BOX_OF_BEASTS, MISC_PHIAL_OF_FLOODS,
                            MISC_PHANTOM_MIRROR, MISC_CONDENSER_VANE,
                            MISC_TIN_OF_TREMORSTONES, MISC_LIGHTNING_ROD);

        newgame_make_item(OBJ_MISCELLANY, selected_evoker, 1);
    }
    else
    {
        wand_type selected_wand =
              random_choose(WAND_ENSLAVEMENT, WAND_PARALYSIS, WAND_FLAME);
        newgame_make_item(OBJ_WANDS, selected_wand, 1, 15);
    }
}

static void _wanderer_good_equipment(skill_type & skill)
{

    const skill_type combined_weapon_skills[] =
        { SK_AXES, SK_MACES_FLAILS, SK_BOWS, SK_CROSSBOWS,
          SK_SHORT_BLADES, SK_LONG_BLADES, SK_STAVES, SK_UNARMED_COMBAT,
          SK_POLEARMS };

    int total_weapons = ARRAYSZ(combined_weapon_skills);

    // Normalise the input type.
    if (skill == SK_FIGHTING)
    {
        int max_sklev = 0;
        skill_type max_skill = SK_NONE;

        for (int i = 0; i < total_weapons; ++i)
        {
            if (you.skills[combined_weapon_skills[i]] >= max_sklev)
            {
                max_skill = combined_weapon_skills[i];
                max_sklev = you.skills[max_skill];
            }
        }
        skill = max_skill;
    }

    switch (skill)
    {
    case SK_MACES_FLAILS:
    case SK_AXES:
    case SK_POLEARMS:
    case SK_THROWING:
    case SK_SHORT_BLADES:
    case SK_LONG_BLADES:
    case SK_BOWS:
    case SK_STAVES:
    case SK_CROSSBOWS:
        _give_wanderer_weapon(skill, 2);
        break;

    case SK_ARMOUR:
        newgame_make_item(OBJ_ARMOUR, ARM_SCALE_MAIL, 1, 2);
        break;

    case SK_DODGING:
        // +2 leather armour or +0 leather armour and also 2-4 nets
        if (coinflip())
            newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR, 1, 2);
        else
        {
            newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR);
            newgame_make_item(OBJ_MISSILES, MI_THROWING_NET, 2 + random2(3));
        }
        break;

    case SK_STEALTH:
        // +2 dagger and a good consumable
        newgame_make_item(OBJ_WEAPONS, WPN_DAGGER, 1, 2);
        _good_potion_or_scroll();
        break;


    case SK_SHIELDS:
        newgame_make_item(OBJ_ARMOUR, ARM_KITE_SHIELD);
        break;

    case SK_SPELLCASTING:
    case SK_CONJURATIONS:
    case SK_SUMMONINGS:
    case SK_NECROMANCY:
    case SK_TRANSLOCATIONS:
    case SK_TRANSMUTATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_POISON_MAGIC:
    case SK_HEXES:
        _give_wanderer_book(skill);
        break;

    case SK_UNARMED_COMBAT:
    {
        // 2 random good potions/scrolls
        _good_potion_or_scroll();
        _good_potion_or_scroll();
        break;
    }

    case SK_EVOCATIONS:
        // Random wand
        _wanderer_random_evokable();
        break;
    default:
        break;
    }
}

static void _wanderer_decent_equipment(skill_type & skill,
                                       set<skill_type> & gift_skills)
{
    const skill_type combined_weapon_skills[] =
        { SK_AXES, SK_MACES_FLAILS, SK_BOWS, SK_CROSSBOWS,
          SK_SHORT_BLADES, SK_LONG_BLADES, SK_STAVES, SK_UNARMED_COMBAT,
          SK_POLEARMS };

    int total_weapons = ARRAYSZ(combined_weapon_skills);

    // If fighting comes up, give something from the highest weapon
    // skill.
    if (skill == SK_FIGHTING)
    {
        int max_sklev = 0;
        skill_type max_skill = SK_NONE;

        for (int i = 0; i < total_weapons; ++i)
        {
            if (you.skills[combined_weapon_skills[i]] >= max_sklev)
            {
                max_skill = combined_weapon_skills[i];
                max_sklev = you.skills[max_skill];
            }
        }

        skill = max_skill;
    }

    // Don't give a gift from the same skill twice; just default to
    // a decent consumable
    if (gift_skills.count(skill))
        skill = SK_NONE;

    switch (skill)
    {
    case SK_MACES_FLAILS:
    case SK_AXES:
    case SK_POLEARMS:
    case SK_BOWS:
    case SK_CROSSBOWS:
    case SK_THROWING:
    case SK_STAVES:
    case SK_SHORT_BLADES:
    case SK_LONG_BLADES:
        _give_wanderer_weapon(skill, 0);
        break;

    case SK_ARMOUR:
        newgame_make_item(OBJ_ARMOUR, ARM_RING_MAIL);
        break;

    case SK_SHIELDS:
        newgame_make_item(OBJ_ARMOUR, ARM_BUCKLER);
        break;

    case SK_SPELLCASTING:
    case SK_CONJURATIONS:
    case SK_SUMMONINGS:
    case SK_NECROMANCY:
    case SK_TRANSLOCATIONS:
    case SK_TRANSMUTATIONS:
    case SK_FIRE_MAGIC:
    case SK_ICE_MAGIC:
    case SK_AIR_MAGIC:
    case SK_EARTH_MAGIC:
    case SK_POISON_MAGIC:
        _give_wanderer_minor_book(skill);
        break;

    case SK_EVOCATIONS:
        newgame_make_item(OBJ_WANDS, WAND_RANDOM_EFFECTS, 1, 15);
        break;

    case SK_DODGING:
    case SK_STEALTH:
    case SK_UNARMED_COMBAT:
    case SK_NONE:
        _decent_potion_or_scroll();
        break;

    default:
        break;
    }
}

// We don't actually want to send adventurers wandering naked into the
// dungeon.
static void _wanderer_cover_equip_holes()
{
    if (you.equip[EQ_BODY_ARMOUR] == -1)
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);

    if (you.equip[EQ_WEAPON] == -1)
    {
        newgame_make_item(OBJ_WEAPONS,
                          you.dex() > you.strength() ? WPN_DAGGER : WPN_CLUB);
    }

    // Give a dagger if you have stealth skill. Maybe this is
    // unnecessary?
    if (you.skills[SK_STEALTH] > 1)
    {
        bool has_dagger = false;

        for (const item_def& i : you.inv)
        {
            if (i.is_type(OBJ_WEAPONS, WPN_DAGGER))
            {
                has_dagger = true;
                break;
            }
        }

        if (!has_dagger)
            newgame_make_item(OBJ_WEAPONS, WPN_DAGGER);
    }
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

    // Decide what our character roles are.
    stat_type primary_role   = _wanderer_choose_role();
    stat_type secondary_role = _wanderer_choose_role();

    // Regardless of roles, players get a couple levels in these skills.
    const skill_type util_skills[] =
    { SK_THROWING, SK_STEALTH, SK_SHIELDS, SK_EVOCATIONS };

    int util_size = ARRAYSZ(util_skills);

    // Maybe too many skill levels, given the level 1 floor on skill
    // levels for wanderers?
    int primary_skill_levels   = 5;
    int secondary_skill_levels = 3;

    // Allocate main skill levels.
    _wanderer_role_skill(primary_role, primary_skill_levels);
    _wanderer_role_skill(secondary_role, secondary_skill_levels);

    skill_type util_skill1 = _apt_weighted_choice(util_skills, util_size);
    skill_type util_skill2 = _apt_weighted_choice(util_skills, util_size);

    // And a couple levels of utility skills.
    you.skills[util_skill1]++;
    you.skills[util_skill2]++;

    // Keep track of what skills we got items from, mostly to prevent
    // giving a good and then a normal version of the same weapon.
    set<skill_type> gift_skills;

    // Wanderers get 1 good thing, a couple average things, and then
    // 1 last stage to fill any glaring equipment holes (no clothes,
    // etc.).
    skill_type good_equipment = _weighted_skill_roll();

    // The first of these goes through the whole role/aptitude weighting
    // thing again. It's quite possible that this will give something
    // we have no skill in.
    const stat_type selected_role = one_chance_in(3) ? secondary_role
                                                     : primary_role;
    const skill_type sk_1 = _wanderer_role_weapon_select(selected_role);
    skill_type sk_2 = SK_NONE;

    if (selected_role == STAT_INT)
        sk_2 = _wanderer_role_weapon_select(selected_role);

    skill_type decent_1 = _wanderer_role_skill_select(selected_role,
                                                      sk_1, sk_2);
    skill_type decent_2 = _weighted_skill_roll();

    _wanderer_good_equipment(good_equipment);
    gift_skills.insert(good_equipment);

    _wanderer_decent_equipment(decent_1, gift_skills);
    gift_skills.insert(decent_1);
    _wanderer_decent_equipment(decent_2, gift_skills);
    gift_skills.insert(decent_2);

    _wanderer_cover_equip_holes();
}

void memorise_wanderer_spell()
{
    // If the player got only one level 1 spell, memorise it. Otherwise, let the
    // player choose which spell(s) to memorise and don't memorise any.
    auto const available_spells = get_sorted_spell_list(true, true);
    if (available_spells.size())
    {
        int num_level_one_spells = 0;
        spell_type which_spell;
        for (spell_type spell : available_spells)
            if (spell_difficulty(spell) == 1)
            {
                num_level_one_spells += 1;
                which_spell = spell;
            }

        if (num_level_one_spells == 1)
            add_spell_to_memory(which_spell);
    }
}
