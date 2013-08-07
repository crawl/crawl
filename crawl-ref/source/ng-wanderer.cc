#include "AppHdr.h"

#include "ng-wanderer.h"

#include "itemname.h"
#include "itemprop.h"
#include "ng-setup.h"
#include "player.h"
#include "random.h"
#include "skills2.h"
#include "spl-util.h"

// Returns true if a "good" weapon is given.
static bool _give_wanderer_weapon(int & slot, int wpn_skill, int plus)
{
    // Darts skill also gets you some needles.
    if (wpn_skill == SK_THROWING)
    {
        // Plus is set if we are getting a good item.  In that case, we
        // get curare here.
        if (plus)
        {
            newgame_make_item(slot, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1,
                               1 + random2(4));
            set_item_ego_type(you.inv[slot], OBJ_MISSILES, SPMSL_CURARE);
            slot++;
        }
        // Otherwise, we just get some poisoned needles.
        else
        {
            newgame_make_item(slot, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1,
                               5 + roll_dice(2, 5));
            set_item_ego_type(you.inv[slot], OBJ_MISSILES, SPMSL_POISONED);
            slot++;
        }

        autopickup_starting_ammo(MI_NEEDLE);
    }

    newgame_make_item(slot, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);

    // We'll also re-fill the template, all for later possible safe
    // reuse of code in the future.
    you.inv[slot].quantity  = 1;
    you.inv[slot].plus      = 0;
    you.inv[slot].plus2     = 0;
    you.inv[slot].special   = 0;

    // Now fill in the type according to the random wpn_skill.
    switch (wpn_skill)
    {
    case SK_SHORT_BLADES:
        you.inv[slot].sub_type = WPN_SHORT_SWORD;
        break;

    case SK_LONG_BLADES:
        you.inv[slot].sub_type = WPN_FALCHION;
        break;

    case SK_MACES_FLAILS:
        you.inv[slot].sub_type = WPN_MACE;
        break;

    case SK_AXES:
        you.inv[slot].sub_type = WPN_HAND_AXE;
        break;

    case SK_POLEARMS:
        you.inv[slot].sub_type = WPN_SPEAR;
        break;

    case SK_STAVES:
        you.inv[slot].sub_type = WPN_QUARTERSTAFF;
        break;

    case SK_THROWING:
        you.inv[slot].sub_type = WPN_BLOWGUN;
        break;

    case SK_BOWS:
        you.inv[slot].sub_type = WPN_BOW;
        break;

    case SK_CROSSBOWS:
        you.inv[slot].sub_type = WPN_CROSSBOW;
        break;
    }

    int offset = plus ? 1 : 0;
    you.inv[slot].plus  = random2(plus) + offset;
    you.inv[slot].plus2 = random2(plus) + offset;

    return true;
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

    switch ((int)selected_role)
    {
    case STAT_DEX:
        switch (random2(6))
        {
        case 0:
        case 1:
            selected_skill = SK_FIGHTING;
            break;
        case 2:
            selected_skill = SK_DODGING;
            break;
        case 3:
            selected_skill = SK_STEALTH;
            break;
        case 4:
        case 5:
            selected_skill = sk_1;
            break;
        }
        break;

    case STAT_STR:
    {
        int options = 3;
        if (!you_can_wear(EQ_BODY_ARMOUR))
            options--;

        switch (random2(options))
        {
        case 0:
            selected_skill = SK_FIGHTING;
            break;
        case 1:
            selected_skill = sk_1;
            break;
        case 2:
            selected_skill = SK_ARMOUR;
            break;
        }
        break;
    }

    case STAT_INT:
        switch (random2(3))
        {
        case 0:
            selected_skill = SK_SPELLCASTING;
            break;
        case 1:
            selected_skill = sk_1;
            break;
        case 2:
            selected_skill = sk_2;
            break;
        }
        break;
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
          SK_HEXES, SK_CHARMS, SK_FIRE_MAGIC, SK_ICE_MAGIC,
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
    skill_type weapon_type = NUM_SKILLS;
    skill_type spell2 = NUM_SKILLS;

    weapon_type = _wanderer_role_weapon_select(role);
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

static void _give_wanderer_book(skill_type skill, int & slot)
{
    int book_type = BOOK_MINOR_MAGIC;
    switch ((int)skill)
    {
    case SK_SPELLCASTING:
        book_type = BOOK_MINOR_MAGIC;
        break;

    case SK_CONJURATIONS:
        switch (random2(3))
        {
        case 0:
            book_type = BOOK_MINOR_MAGIC;
            break;
        case 1:
        case 2:
            book_type = BOOK_CONJURATIONS;
            break;
        }
        break;

    case SK_SUMMONINGS:
        switch (random2(2))
        {
        case 0:
            book_type = BOOK_MINOR_MAGIC;
            break;
        case 1:
            book_type = BOOK_CALLINGS;
            break;
        }
        break;

    case SK_NECROMANCY:
        book_type = BOOK_NECROMANCY;
        break;

    case SK_TRANSLOCATIONS:
        book_type = BOOK_SPATIAL_TRANSLOCATIONS;
        break;

    case SK_TRANSMUTATIONS:
        switch (random2(2))
        {
        case 0:
            book_type = BOOK_GEOMANCY;
            break;
        case 1:
            book_type = BOOK_CHANGES;
            break;
        }
        break;

    case SK_FIRE_MAGIC:
        book_type = BOOK_FLAMES;
        break;

    case SK_ICE_MAGIC:
        book_type = BOOK_FROST;
        break;

    case SK_AIR_MAGIC:
        book_type = BOOK_AIR;
        break;

    case SK_EARTH_MAGIC:
        book_type = BOOK_GEOMANCY;
        break;

    case SK_POISON_MAGIC:
        book_type = BOOK_YOUNG_POISONERS;
        break;

    case SK_HEXES:
        book_type = BOOK_MALEDICT;
        break;

    case SK_CHARMS:
        book_type = BOOK_BATTLE;
        break;
    }

    newgame_make_item(slot, EQ_NONE, OBJ_BOOKS, book_type);
}

// Players can get some consumables as a "good item".
static void _good_potion_or_scroll(int & slot)
{
    int base_rand = 5;
    // No potions for mummies.
    if (you.is_undead == US_UNDEAD)
        base_rand -= 3;
    // No berserk rage for ghouls.
    else if (you.is_undead && you.is_undead != US_SEMI_UNDEAD)
        base_rand--;

    you.inv[slot].quantity = 1;
    you.inv[slot].plus     = 0;
    you.inv[slot].plus2    = 0;

    switch (random2(base_rand))
    {
    case 0:
        you.inv[slot].base_type = OBJ_SCROLLS;
        you.inv[slot].sub_type  = SCR_FEAR;
        break;

    case 1:
        you.inv[slot].base_type = OBJ_SCROLLS;
        you.inv[slot].sub_type  = SCR_BLINKING;
        break;

    case 2:
        you.inv[slot].base_type = OBJ_POTIONS;
        you.inv[slot].sub_type  = POT_HEAL_WOUNDS;
        break;

    case 3:
        you.inv[slot].base_type = OBJ_POTIONS;
        you.inv[slot].sub_type  = POT_SPEED;
        break;

    case 4:
        you.inv[slot].base_type = OBJ_POTIONS;
        you.inv[slot].sub_type  = POT_BERSERK_RAGE;
        break;
    }

    slot++;
}

// Make n random attempts at identifying curing or teleportation.
static void _curing_or_teleport(int n)
{
    int temp_rand = 2;

    // No potions for mummies.
    if (you.is_undead == US_UNDEAD)
        temp_rand--;

    for (int i = 0; i < n; ++i)
    {
        switch (random2(temp_rand))
        {
        case 0:
            set_ident_type(OBJ_SCROLLS, SCR_TELEPORTATION, ID_KNOWN_TYPE);
            break;
        case 1:
            set_ident_type(OBJ_POTIONS, POT_CURING, ID_KNOWN_TYPE);
            break;
        }
    }
}

// Create a random wand in the inventory.
static void _wanderer_random_evokable(int & slot)
{
    wand_type selected_wand = WAND_ENSLAVEMENT;

    switch (random2(5))
    {
    case 0:
        selected_wand = WAND_ENSLAVEMENT;
        break;

    case 1:
        selected_wand = WAND_CONFUSION;
        break;

    case 2:
        selected_wand = WAND_MAGIC_DARTS;
        break;

    case 3:
        selected_wand = WAND_FROST;
        break;

    case 4:
        selected_wand = WAND_FLAME;
        break;

    default:
        break;
    }

    newgame_make_item(slot, EQ_NONE, OBJ_WANDS, selected_wand, -1, 1,
                       15);
    slot++;
}

static void _wanderer_good_equipment(skill_type & skill, int & slot)
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

    switch ((int)skill)
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
        _give_wanderer_weapon(slot, skill, 3);
        slot++;
        break;

    case SK_ARMOUR:
        // Deformed species aren't given armour skill, so there's no need
        // to worry about scale mail's not fitting.
        newgame_make_item(slot, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_SCALE_MAIL);
        slot++;
        break;

    case SK_SHIELDS:
        newgame_make_item(slot, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD,
                           ARM_BUCKLER);
        slot++;
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
    case SK_CHARMS:
        _give_wanderer_book(skill, slot);
        slot++;
        break;

    case SK_DODGING:
    case SK_STEALTH:
    case SK_UNARMED_COMBAT:
    case SK_INVOCATIONS:
    {
        // Random consumables: 2x attempts to ID curing/teleportation
        // 1 good potion/scroll.
        _curing_or_teleport(2);
        _good_potion_or_scroll(slot);
        break;
    }

    case SK_EVOCATIONS:
        // Random wand
        _wanderer_random_evokable(slot);
        break;
    }
}

// The "decent" spell type item puts a spell in the player's memory.
static void _give_wanderer_spell(skill_type skill)
{
    spell_type spell = SPELL_NO_SPELL;

    // Doing a rejection loop for this because I am lazy.
    while (skill == SK_SPELLCASTING || skill == SK_CHARMS)
    {
        int value = SK_LAST_MAGIC - SK_FIRST_MAGIC_SCHOOL + 1;
        skill = skill_type(SK_FIRST_MAGIC_SCHOOL + random2(value));
    }

    switch ((int)skill)
    {
    case SK_CONJURATIONS:
        spell = SPELL_MAGIC_DART;
        break;

    case SK_SUMMONINGS:
        spell = SPELL_SUMMON_SMALL_MAMMAL;
        break;

    case SK_NECROMANCY:
        spell = SPELL_PAIN;
        break;

    case SK_TRANSLOCATIONS:
        spell = SPELL_APPORTATION;
        break;

    case SK_TRANSMUTATIONS:
        spell = SPELL_BEASTLY_APPENDAGE;
        break;

    case SK_FIRE_MAGIC:
        spell = SPELL_FLAME_TONGUE;
        break;

    case SK_ICE_MAGIC:
        spell = SPELL_FREEZE;
        break;

    case SK_AIR_MAGIC:
        spell = SPELL_SHOCK;
        break;

    case SK_EARTH_MAGIC:
        spell = SPELL_SANDBLAST;
        break;

    case SK_POISON_MAGIC:
        spell = SPELL_STING;
        break;

    case SK_HEXES:
        spell = SPELL_CORONA;
        break;
    }

    add_spell_to_memory(spell);
}

static void _wanderer_decent_equipment(skill_type & skill,
                                       set<skill_type> & gift_skills,
                                       int & slot)
{
    const skill_type combined_weapon_skills[] =
        { SK_AXES, SK_MACES_FLAILS, SK_BOWS, SK_CROSSBOWS,
          SK_SHORT_BLADES, SK_LONG_BLADES, SK_STAVES, SK_UNARMED_COMBAT,
          SK_POLEARMS };

    int total_weapons = ARRAYSZ(combined_weapon_skills);

    // If we already gave an item for this type, just give the player
    // a consumable.
    if ((skill == SK_DODGING || skill == SK_STEALTH)
        && gift_skills.find(SK_ARMOUR) != gift_skills.end())
    {
        skill = SK_NONE;
    }

    // Give the player knowledge of only one spell.
    if (skill >= SK_SPELLCASTING && skill <= SK_LAST_MAGIC)
    {
        for (unsigned i = 0; i < you.spells.size(); ++i)
        {
            if (you.spells[i] != SPELL_NO_SPELL)
            {
                skill = SK_NONE;
                break;
            }
        }
    }

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
    // a curing potion/teleportation scroll.
    if (gift_skills.find(skill) != gift_skills.end())
        skill = SK_NONE;

    switch ((int)skill)
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
        _give_wanderer_weapon(slot, skill, 0);
        slot++;
        break;

    case SK_ARMOUR:
        newgame_make_item(slot, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_RING_MAIL);
        slot++;
        break;

    case SK_SHIELDS:
        newgame_make_item(slot, EQ_SHIELD, OBJ_ARMOUR, ARM_BUCKLER,
                           ARM_SHIELD);
        slot++;
        break;

    case SK_DODGING:
    case SK_STEALTH:
        newgame_make_item(slot, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        slot++;
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
        _give_wanderer_spell(skill);
        break;

    case SK_UNARMED_COMBAT:
    case SK_INVOCATIONS:
    case SK_EVOCATIONS:
    case SK_NONE:
        _curing_or_teleport(1);
        break;
    }
}

// We don't actually want to send adventurers wandering naked into the
// dungeon.
static void _wanderer_cover_equip_holes(int & slot)
{
    // We are going to cover any glaring holes (no armour/no weapon) that
    // occurred during equipment generation.
    if (you.equip[EQ_BODY_ARMOUR] == -1)
    {
        newgame_make_item(slot, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        slot++;
    }

    if (you.equip[EQ_WEAPON] == -1)
    {
        weapon_type weapon = WPN_CLUB;
        if (you.dex() > you.strength())
            weapon = WPN_DAGGER;

        newgame_make_item(slot, EQ_WEAPON, OBJ_WEAPONS, weapon);
        slot++;
    }

    // Give a dagger if you have stealth skill.  Maybe this is
    // unnecessary?
    if (you.skills[SK_STEALTH] > 1)
    {
        bool has_dagger = false;

        for (int i = 0; i < slot; ++i)
        {
            if (you.inv[i].base_type == OBJ_WEAPONS
                && you.inv[i].sub_type == WPN_DAGGER)
            {
                has_dagger = true;
                break;
            }
        }

        if (!has_dagger)
        {
            newgame_make_item(slot, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);
            slot++;
        }
    }

    // The player needs a stack of bolts if they have a crossbow.
    bool need_bolts = false;

    for (int i = 0; i < slot; ++i)
    {
        if (you.inv[i].base_type == OBJ_WEAPONS
            && you.inv[i].sub_type == WPN_CROSSBOW)
        {
            need_bolts = true;
            break;
        }
    }

    if (need_bolts)
    {
        newgame_make_item(slot, EQ_NONE, OBJ_MISSILES, MI_BOLT, -1,
                           15 + random2avg(21, 5));
        slot++;
        autopickup_starting_ammo(MI_BOLT);
    }

    // And the player needs arrows if they have a bow.
    bool needs_arrows = false;

    for (int i = 0; i < slot; ++i)
    {
        if (you.inv[i].base_type == OBJ_WEAPONS
            && you.inv[i].sub_type == WPN_BOW)
        {
            needs_arrows = true;
            break;
        }
    }

    if (needs_arrows)
    {
        newgame_make_item(slot, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1,
                           15 + random2avg(21, 5));
        slot++;
        autopickup_starting_ammo(MI_ARROW);
    }
}

// New style wanderers are supposed to be decent in terms of skill
// levels/equipment, but pretty randomised.
void create_wanderer(void)
{
    // Decide what our character roles are.
    stat_type primary_role   = _wanderer_choose_role();
    stat_type secondary_role = _wanderer_choose_role();

    // Regardless of roles, players get a couple levels in these skills.
    const skill_type util_skills[] =
    { SK_THROWING, SK_STEALTH, SK_SHIELDS, SK_EVOCATIONS, SK_INVOCATIONS };

    int util_size = ARRAYSZ(util_skills);

    // No Invocations for demigods.
    if (you.species == SP_DEMIGOD)
        util_size--;

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

    // Caster types maybe need more MP?
    int mp_adjust = 0;
    if (primary_role == STAT_INT)
        mp_adjust++;
    if (secondary_role == STAT_INT)
        mp_adjust++;
    you.mp_max_perm += mp_adjust;

    // Keep track of what skills we got items from, mostly to prevent
    // giving a good and then a normal version of the same weapon.
    set<skill_type> gift_skills;

    // Wanderers get 1 good thing, a couple average things, and then
    // 1 last stage to fill any glaring equipment holes (no clothes,
    // etc.).
    skill_type good_equipment = _weighted_skill_roll();

    // The first of these goes through the whole role/aptitude weighting
    // thing again.  It's quite possible that this will give something
    // we have no skill in.
    stat_type selected_role = one_chance_in(3) ? secondary_role : primary_role;
    skill_type sk_1 = SK_NONE;
    skill_type sk_2 = SK_NONE;

    sk_1 = _wanderer_role_weapon_select(selected_role);
    if (selected_role == STAT_INT)
        sk_2 = _wanderer_role_weapon_select(selected_role);

    skill_type decent_1 = _wanderer_role_skill_select(selected_role,
                                                      sk_1, sk_2);
    skill_type decent_2 = _weighted_skill_roll();

    // Not even trying to put things in the same slot from game to game.
    int equip_slot = 0;

    _wanderer_good_equipment(good_equipment, equip_slot);
    gift_skills.insert(good_equipment);

    _wanderer_decent_equipment(decent_1, gift_skills, equip_slot);
    gift_skills.insert(decent_1);
    _wanderer_decent_equipment(decent_2, gift_skills, equip_slot);
    gift_skills.insert(decent_2);

    _wanderer_cover_equip_holes(equip_slot);
}
