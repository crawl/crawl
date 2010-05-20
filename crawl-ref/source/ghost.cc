/*
 * File:    ghost.cc
 * Summary: Player ghost and random Pandemonium demon handling.
 *
 * Created for Dungeon Crawl Reference by dshaligram on
 * Thu Mar 15 20:10:20 2007 UTC.
 */

#include "AppHdr.h"

#include "ghost.h"

#include "artefact.h"
#include "colour.h"
#include "env.h"
#include "externs.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "mon-iter.h"
#include "ng-input.h"
#include "random.h"
#include "skills2.h"
#include "spl-cast.h"
#include "mon-util.h"
#include "mon-transit.h"
#include "place.h"
#include "player.h"
#include "religion.h"

#include <vector>

#define MAX_GHOST_DAMAGE     50
#define MAX_GHOST_HP        400
#define MAX_GHOST_EVASION    60
#define MIN_GHOST_SPEED       6
#define MAX_GHOST_SPEED      13

std::vector<ghost_demon> ghosts;

// Order for looking for conjurations for the 1st & 2nd spell slots,
// when finding spells to be remembered by a player's ghost.
static spell_type search_order_conj[] = {
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_FIRE_STORM,
    SPELL_ICE_STORM,
    SPELL_CHAIN_LIGHTNING,
    SPELL_BOLT_OF_DRAINING,
    SPELL_AGONY,
    SPELL_DISINTEGRATE,
    SPELL_LIGHTNING_BOLT,
    SPELL_AIRSTRIKE,
    SPELL_STICKY_FLAME,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_IOOD,
    SPELL_BOLT_OF_MAGMA,
    SPELL_THROW_ICICLE,
    SPELL_BOLT_OF_FIRE,
    SPELL_BOLT_OF_COLD,
    SPELL_FIREBALL,
    SPELL_DELAYED_FIREBALL,
    SPELL_VENOM_BOLT,
    SPELL_IRON_SHOT,
    SPELL_STONE_ARROW,
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
    SPELL_PAIN,
    SPELL_STING,
    SPELL_SHOCK,
    SPELL_SANDBLAST,
    SPELL_MAGIC_DART,
    SPELL_HIBERNATION,
    SPELL_CORONA,
    SPELL_NO_SPELL,                        // end search
};

// Order for looking for summonings and self-enchants for the 3rd spell
// slot.
static spell_type search_order_third[] = {
// 0
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_SUMMON_GREATER_DEMON,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_HAUNT,
    SPELL_SUMMON_DEMON,
    SPELL_DEMONIC_HORDE,
    SPELL_HASTE,
    SPELL_SILENCE,
    SPELL_SUMMON_BUTTERFLIES,
    SPELL_SUMMON_SWARM,
// 10
    SPELL_SUMMON_UGLY_THING,
    SPELL_SWIFTNESS,
    SPELL_SUMMON_ICE_BEAST,
    SPELL_ANIMATE_DEAD,
    SPELL_INVISIBILITY,
    SPELL_SUMMON_SCORPIONS,
    SPELL_CALL_IMP,
    SPELL_SUMMON_SMALL_MAMMALS,
    SPELL_CONTROLLED_BLINK,
    SPELL_BLINK,
    SPELL_NO_SPELL,                        // end search
};

// Order for looking for enchants for the 4th & 5th spell slots.  If
// this fails, go through conjurations.  Note: Dig must be in misc2
// (5th) position to work.
static spell_type search_order_misc[] = {
// 0
    SPELL_AGONY,
    SPELL_BANISHMENT,
    SPELL_FREEZING_CLOUD,
    SPELL_DISPEL_UNDEAD,
    SPELL_PARALYSE,
    SPELL_CONFUSE,
    SPELL_MEPHITIC_CLOUD,
    SPELL_SLOW,
    SPELL_POLYMORPH_OTHER,
    SPELL_TELEPORT_OTHER,
// 10
    SPELL_EVAPORATE, // replaced with Mephitic Cloud, though at lower priority
    SPELL_DIG,
    SPELL_CORONA,
    SPELL_NO_SPELL,                        // end search
};

// Last slot (emergency) can only be Teleport Self or Blink.

ghost_demon::ghost_demon()
{
    reset();
}

void ghost_demon::reset()
{
    name.clear();
    species          = SP_UNKNOWN;
    job              = JOB_UNKNOWN;
    religion         = GOD_NO_GOD;
    best_skill       = SK_FIGHTING;
    best_skill_level = 0;
    xl               = 0;
    max_hp           = 0;
    ev               = 0;
    ac               = 0;
    damage           = 0;
    speed            = 10;
    see_invis        = false;
    brand            = SPWPN_NORMAL;
    att_type         = AT_HIT;
    att_flav         = AF_PLAIN;
    resists          = mon_resist_def();
    spellcaster      = false;
    cycle_colours    = false;
    colour           = BLACK;
    fly              = FL_NONE;
}

void ghost_demon::init_random_demon()
{
    name = make_name(random_int(), false);

    // hp - could be defined below (as could ev, AC, etc.). Oh well, too late:
    max_hp = 100 + roll_dice(3, 50);

    ev = 5 + random2(20);
    ac = 5 + random2(20);

    see_invis = !one_chance_in(10);

    if (!one_chance_in(3))
        resists.fire = random_range(1, 2);
    else
    {
        resists.fire = 0; // res_fire

        if (one_chance_in(10))
            resists.fire = -1;
    }

    if (!one_chance_in(3))
        resists.cold = random_range(1, 2);
    else
    {
        resists.cold = 0;
        if (one_chance_in(10))
            resists.cold = -1;
    }

    // Demons, like ghosts, automatically get poison res. and life prot.

    // resist electricity:
    resists.elec = one_chance_in(3);

    // HTH damage:
    damage = 20 + roll_dice(2, 20);

    // special attack type (uses weapon brand code):
    brand = SPWPN_NORMAL;

    if (!one_chance_in(3))
    {
        do
        {
            brand = static_cast<brand_type>(random2(MAX_PAN_LORD_BRANDS));
            // some brands inappropriate (e.g. holy wrath)
        }
        while (brand == SPWPN_HOLY_WRATH
               || (brand == SPWPN_ORC_SLAYING
                   && you.mons_species() != MONS_ORC)
               || (brand == SPWPN_DRAGON_SLAYING
                   && you.mons_species() != MONS_DRACONIAN)
               || brand == SPWPN_PROTECTION
               || brand == SPWPN_FLAME
               || brand == SPWPN_FROST);
    }

    // Is demon a spellcaster?
    // Upped from one_chance_in(3)... spellcasters are more interesting
    // and I expect named demons to typically have a trick or two. - bwr
    spellcaster = !one_chance_in(10);

    // Does demon fly?
    fly = (one_chance_in(3) ? FL_NONE :
           one_chance_in(5) ? FL_LEVITATE
                            : FL_FLY);

    // hit dice:
    xl = 10 + roll_dice(2, 10);

    // Does demon cycle colours?
    cycle_colours = one_chance_in(10);

    colour = random_colour();

    spells.init(SPELL_NO_SPELL);

    // This bit uses the list of player spells to find appropriate
    // spells for the demon, then converts those spells to the monster
    // spell indices.  Some special monster-only spells are at the end.
    if (spellcaster)
    {
        if (coinflip())
            spells[0] = RANDOM_ELEMENT(search_order_conj);

        // Might duplicate the first spell, but that isn't a problem.
        if (coinflip())
            spells[1] = RANDOM_ELEMENT(search_order_conj);

        if (!one_chance_in(4))
            spells[2] = RANDOM_ELEMENT(search_order_third);

        if (coinflip())
        {
            spells[3] = RANDOM_ELEMENT(search_order_misc);
            if (spells[3] == SPELL_DIG)
                spells[3] = SPELL_NO_SPELL;
        }

        if (coinflip())
            spells[4] = RANDOM_ELEMENT(search_order_misc);

        if (coinflip())
            spells[5] = SPELL_BLINK;
        if (coinflip())
            spells[5] = SPELL_TELEPORT_SELF;

        // Convert the player spell indices to monster spell ones.
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
            spells[i] = translate_spell(spells[i]);

        // Give demon a chance for some monster-only spells.
        // Demon-summoning should be fairly common.
        if (one_chance_in(25))
            spells[0] = SPELL_HELLFIRE_BURST;
        if (one_chance_in(25))
            spells[0] = SPELL_FIRE_STORM;
        if (one_chance_in(25))
            spells[0] = SPELL_ICE_STORM;
        if (one_chance_in(25))
            spells[0] = SPELL_METAL_SPLINTERS;
        if (one_chance_in(25))
            spells[0] = SPELL_ENERGY_BOLT;  // eye of devastation

        if (one_chance_in(25))
            spells[1] = SPELL_STEAM_BALL;
        if (one_chance_in(25))
            spells[1] = SPELL_ISKENDERUNS_MYSTIC_BLAST;
        if (one_chance_in(25))
            spells[1] = SPELL_HELLFIRE;
        if (one_chance_in(25))
            spells[1] = SPELL_IOOD;

        if (one_chance_in(25))
            spells[2] = SPELL_SMITING;
        if (one_chance_in(25))
            spells[2] = SPELL_HELLFIRE_BURST;
        if (one_chance_in(12))
            spells[2] = SPELL_SUMMON_GREATER_DEMON;
        if (one_chance_in(12))
            spells[2] = SPELL_SUMMON_DEMON;
        if (one_chance_in(10))
            spells[2] = SPELL_SUMMON_EYEBALLS;

        if (one_chance_in(20))
            spells[3] = SPELL_SUMMON_GREATER_DEMON;
        if (one_chance_in(20))
            spells[3] = SPELL_SUMMON_DEMON;

        // At least they can summon demons.
        if (spells[3] == SPELL_NO_SPELL)
            spells[3] = SPELL_SUMMON_DEMON;

        if (one_chance_in(15))
            spells[4] = SPELL_DIG;
    }
}

// Returns the movement speed for a player ghost.  Note that this is a
// real speed, not a movement cost, so higher is better.
static int _player_ghost_base_movement_speed()
{
    int speed = 10;

    if (player_mutation_level(MUT_FAST))
        speed += player_mutation_level(MUT_FAST) + 1;
    if (player_mutation_level(MUT_SLOW))
        speed -= player_mutation_level(MUT_SLOW) + 1;

    if (player_equip_ego_type(EQ_BOOTS, SPARM_RUNNING))
        speed += 2;

    // Cap speeds.
    if (speed < MIN_GHOST_SPEED)
        speed = MIN_GHOST_SPEED;
    else if (speed > MAX_GHOST_SPEED)
        speed = MAX_GHOST_SPEED;

    return (speed);
}

void ghost_demon::init_player_ghost()
{
    name   = you.your_name;
    max_hp = ((get_real_hp(false) >= MAX_GHOST_HP)
             ? MAX_GHOST_HP
             : get_real_hp(false));
    ev     = player_evasion();
    ac     = you.armour_class();

    if (ev > MAX_GHOST_EVASION)
        ev = MAX_GHOST_EVASION;

    see_invis      = you.can_see_invisible();
    resists.fire   = player_res_fire();
    resists.cold   = player_res_cold();
    resists.elec   = player_res_electricity();
    speed          = _player_ghost_base_movement_speed();

    damage = 4;
    brand = SPWPN_NORMAL;

    if (you.weapon())
    {
        // This includes ranged weapons, but they're treated as melee.

        const item_def& weapon = *you.weapon();
        if (weapon.base_type == OBJ_WEAPONS || weapon.base_type == OBJ_STAVES)
        {
            damage = property(weapon, PWPN_DAMAGE);

            damage *= 25 + you.skills[weapon_skill(weapon)];
            damage /= 25;

            if (weapon.base_type == OBJ_WEAPONS)
            {
                brand = static_cast<brand_type>(get_weapon_brand(weapon));

                // Ghosts can't get holy wrath, but they get to keep
                // the weapon.
                if (brand == SPWPN_HOLY_WRATH)
                    brand = SPWPN_NORMAL;

                // Don't copy ranged-only brands from launchers (reaping etc.).
                if (brand > MAX_PAN_LORD_BRANDS)
                    brand = SPWPN_NORMAL;
            }
        }
    }
    else
    {
        // Unarmed combat.
        if (you.demon_pow[MUT_CLAWS])
            damage += you.experience_level;

        damage += you.skills[SK_UNARMED_COMBAT];
    }

    damage *= 30 + you.skills[SK_FIGHTING];
    damage /= 30;

    damage += you.strength() / 4;

    if (damage > MAX_GHOST_DAMAGE)
        damage = MAX_GHOST_DAMAGE;

    species = you.species;
    job = you.char_class;

    // Ghosts can't worship good gods.
    if (!is_good_god(you.religion))
        religion = you.religion;

    best_skill = ::best_skill(SK_FIGHTING, (NUM_SKILLS - 1), 99);
    best_skill_level = you.skills[best_skill];
    xl = you.experience_level;

    // These are the same as in mon-data.h.
    colour = WHITE;
    fly = FL_LEVITATE;

    add_spells();
}

static unsigned char _ugly_thing_assign_colour(unsigned char force_colour,
                                               unsigned char force_not_colour)
{
    unsigned char colour;

    if (force_colour != BLACK)
        colour = force_colour;
    else
    {
        do
            colour = ugly_thing_random_colour();
        while (force_not_colour != BLACK && colour == force_not_colour);
    }

    return (colour);
}

static mon_attack_flavour _ugly_thing_colour_to_flavour(unsigned char u_colour)
{
    mon_attack_flavour u_att_flav = AF_PLAIN;

    switch (u_colour)
    {
    case RED:
        u_att_flav = AF_FIRE;
        break;

    case BROWN:
        u_att_flav = AF_ACID;
        break;

    case GREEN:
        u_att_flav = AF_POISON_NASTY;
        break;

    case CYAN:
        u_att_flav = AF_ELEC;
        break;

    case MAGENTA:
        u_att_flav = AF_DISEASE;
        break;

    case LIGHTGREY:
        u_att_flav = AF_COLD;
        break;

    default:
        break;
    }

    return (u_att_flav);
}

static mon_attack_flavour _very_ugly_thing_flavour_upgrade(mon_attack_flavour u_att_flav)
{
    switch (u_att_flav)
    {
    case AF_FIRE:
        u_att_flav = AF_NAPALM;
        break;

    case AF_POISON_NASTY:
        u_att_flav = AF_POISON_MEDIUM;
        break;

    case AF_DISEASE:
        u_att_flav = AF_ROT;
        break;

    default:
        break;
    }

    return (u_att_flav);
}

void ghost_demon::init_ugly_thing(bool very_ugly, bool only_mutate,
                                  unsigned char force_colour)
{
    // Movement speed: 11, the same as in mon-data.h.
    speed = 11;

    // Midpoint: 10, as in mon-data.h.
    ev = 9 + random2(3);

    // Midpoint: 3, as in mon-data.h.
    ac = 2 + random2(3);

    // Midpoint: 12, as in mon-data.h.
    damage = 11 + random2(3);

    // If we're mutating an ugly thing, leave its experience level, hit
    // dice and maximum hit points as they are.
    if (!only_mutate)
    {
        // Experience level: 8, the same as in mon-data.h.
        xl = 8;

        // Hit dice: {8, 3, 5, 0}, the same as in mon-data.h.
        max_hp = hit_points(xl, 3, 5);
    }

    const mon_attack_type att_types[] =
    {
        AT_BITE, AT_STING, AT_ENGULF, AT_CLAW, AT_PECK, AT_HEADBUTT, AT_PUNCH,
        AT_KICK, AT_TENTACLE_SLAP, AT_TAIL_SLAP, AT_GORE, AT_CONSTRICT
    };

    att_type = RANDOM_ELEMENT(att_types);

    // An ugly thing always gets a low-intensity colour.  If we're
    // mutating it, it always gets a different colour from what it had
    // before.
    colour = _ugly_thing_assign_colour(make_low_colour(force_colour),
                                       only_mutate ? make_low_colour(colour)
                                                   : BLACK);

    // Pick a compatible attack flavour for this colour.
    att_flav = _ugly_thing_colour_to_flavour(colour);

    // Pick a compatible resistance for this attack flavour.
    ugly_thing_add_resistance(false, att_flav);

    // If this is a very ugly thing, upgrade it properly.
    if (very_ugly)
        ugly_thing_to_very_ugly_thing();
}

void ghost_demon::ugly_thing_to_very_ugly_thing()
{
    // Midpoint when added to an ugly thing: 4, as in mon-data.h.
    ac++;

    // Midpoint when added to an ugly thing: 17, as in mon-data.h.
    damage += 5;

    // Experience level when added to an ugly thing: 12, the same as in
    // mon-data.h.
    xl += 4;

    // Hit dice when added to an ugly thing: {12, 3, 5, 0}, the same as
    // in mon-data.h.
    max_hp += hit_points(4, 3, 5);

    // A very ugly thing always gets a high-intensity colour.
    colour = make_high_colour(colour);

    // A very ugly thing sometimes gets an upgraded attack flavour.
    att_flav = _very_ugly_thing_flavour_upgrade(att_flav);

    // Pick a compatible resistance for this attack flavour.
    ugly_thing_add_resistance(true, att_flav);
}

void ghost_demon::ugly_thing_add_resistance(bool very_ugly,
                                            mon_attack_flavour u_att_flav)
{
    resists.elec = 0;
    resists.poison = 0;
    resists.fire = 0;
    resists.sticky_flame = false;
    resists.cold = 0;
    resists.acid = 0;
    resists.rotting = false;

    switch (u_att_flav)
    {
    case AF_FIRE:
    case AF_NAPALM:
        resists.fire = (very_ugly ? 2 : 1);
        resists.sticky_flame = true;
        break;

    case AF_ACID:
        resists.acid = (very_ugly ? 2 : 1);
        break;

    case AF_POISON_NASTY:
    case AF_POISON_MEDIUM:
        resists.poison = (very_ugly ? 2 : 1);
        break;

    case AF_ELEC:
        resists.elec = (very_ugly ? 2 : 1);
        break;

    case AF_DISEASE:
    case AF_ROT:
        resists.rotting = true;
        break;

    case AF_COLD:
        resists.cold = (very_ugly ? 2 : 1);
        break;

    default:
        break;
    }
}

void ghost_demon::init_dancing_weapon(const item_def& weapon, int power)
{
    int mass  = item_mass(weapon);
    int delay = property(weapon, PWPN_SPEED);
    int damg  = property(weapon, PWPN_DAMAGE);

    if (power > 150)
        power = 150;

    resists.poison = 1;
    resists.fire = 1;
    resists.cold = 1;
    resists.elec = 1;

    colour = weapon.colour;
    fly = FL_LEVITATE;

    // We want Tukima to reward characters who invest heavily in both
    // carrying capacity and enchantments skill.  Therefore, heavy
    // weapons are a bit stronger when animated, and benefit much more
    // from very high skill.

    // First set up what the monsters will look like at very high skill.
    // Daggers are weak here! In the table, "44+22" means d44+d22 with
    // d22 being base damage and d44 coming from power.

    // Giant spiked club: speed 12, 44+22 damage, 35 AC, 70 HP, 16 EV
    // Bardiche:          speed 10, 40+20 damage, 20 AC, 40 HP, 15 EV
    // Katana:            speed 17, 26+13 damage, 16 AC, 32 HP, 18 EV
    // Dagger:            speed 20,  8+ 4 damage,  2 AC,  4 HP, 20 EV
    // Quick blade:       speed 23, 10+ 5 damage,  5 AC, 10 HP, 22 EV
    // Sabre:             speed 18, 14+ 7 damage,  9 AC, 18 HP, 19 EV

    xl = 15;

    speed   = 30 - delay;
    ev      = 25 - delay / 2;
    ac      = mass / 10;
    damage  = 2 * damg;
    max_hp  = mass / 5;

    // If you aren't an awesome spellcaster, nerf the weapons.  Do it in
    // a way that lays most of the penalty on heavy weapons.

    speed = std::max(3, speed - (10 - power / 15));
    ev    = std::max(3, ev    - (10 - power / 15));

    ac = ac * power / 200;
    max_hp = std::max(5, max_hp * power / 150);
    damage = std::max(1, damage * power / 150);

    // For a spellpower 75 character (typical late midgame mage with no Ench
    // focus), we have:

    // Giant spiked club: speed 7,  22+22 damage, 17 AC, 35 HP, 11 EV
    // Bardiche:          speed 5,  20+20 damage, 10 AC, 20 HP, 10 EV
    // Katana:            speed 12, 13+13 damage,  8 AC, 16 HP, 13 EV
    // Dagger:            speed 15,  4+4  damage,  1 AC,  5 HP, 15 EV
    // Quick blade:       speed 18,  5+5  damage,  2 AC,  5 HP, 17 EV
    // Sabre:             speed 13,  7+7  damage,  4 AC,  9 HP, 14 EV

    // At spellpower 37 (early game character with focus on Ench):

    // Giant spiked club: speed 5, 11+22 damage, 8 AC, 17 HP,  9 EV
    // Bardiche:          speed 3, 10+20 damage, 5 AC, 10 HP,  8 EV
    // Katana:            speed 10, 6+13 damage, 4 AC,  8 HP, 11 EV
    // Dagger:            speed 13, 2+4  damage, 0 AC,  5 HP, 13 EV
    // Quick blade:       speed 16, 2+5  damage, 1 AC,  5 HP, 15 EV
    // Sabre:             speed 11, 3+7  damage, 2 AC,  5 HP, 12 EV
}

static bool _know_spell(spell_type spell)
{
    return (you.has_spell(spell) && spell_fail(spell) < 50);
}

static spell_type search_first_list(int ignore_spell)
{
    for (unsigned i = 0;
         i < sizeof(search_order_conj) / sizeof(*search_order_conj); ++i)
     {
        if (search_order_conj[i] == SPELL_NO_SPELL)
            return (SPELL_NO_SPELL);

        if (search_order_conj[i] == ignore_spell)
            continue;

        if (_know_spell(search_order_conj[i]))
            return (search_order_conj[i]);
    }

    return (SPELL_NO_SPELL);
}

static spell_type search_second_list(int ignore_spell)
{
    for (unsigned i = 0;
         i < sizeof(search_order_third) / sizeof(*search_order_third); ++i)
    {
        if (search_order_third[i] == SPELL_NO_SPELL)
            return (SPELL_NO_SPELL);

        if (search_order_third[i] == ignore_spell)
            continue;

        if (_know_spell(search_order_third[i]))
            return (search_order_third[i]);
    }

    return (SPELL_NO_SPELL);
}

static spell_type search_third_list(int ignore_spell)
{
    for (unsigned i = 0;
         i < sizeof(search_order_misc) / sizeof(*search_order_misc); ++i)
    {
        if (search_order_misc[i] == SPELL_NO_SPELL)
            return (SPELL_NO_SPELL);

        if (search_order_misc[i] == ignore_spell)
            continue;

        if (_know_spell(search_order_misc[i]))
            return (search_order_misc[i]);
    }

    return (SPELL_NO_SPELL);
}

// Used when creating ghosts: goes through and finds spells for the
// ghost to cast.  Death is a traumatic experience, so ghosts only
// remember a few spells.
void ghost_demon::add_spells()
{
    spells.init(SPELL_NO_SPELL);

    spells[0] = search_first_list(SPELL_NO_SPELL);
    spells[1] = search_first_list(spells[0]);
    spells[2] = search_second_list(SPELL_NO_SPELL);
    spells[3] = search_third_list(SPELL_DIG);

    if (spells[3] == SPELL_NO_SPELL)
        spells[3] = search_first_list(SPELL_NO_SPELL);

    spells[4] = search_third_list(spells[3]);

    if (spells[4] == SPELL_NO_SPELL)
        spells[4] = search_first_list(spells[3]);

    if (_know_spell(SPELL_DIG))
        spells[4] = SPELL_DIG;

    // Look for Blink or Teleport Self for the emergency slot.
    if (_know_spell(SPELL_CONTROLLED_BLINK)
        || _know_spell(SPELL_BLINK))
    {
        spells[5] = SPELL_CONTROLLED_BLINK;
    }

    if (_know_spell(SPELL_TELEPORT_SELF))
        spells[5] = SPELL_TELEPORT_SELF;

    for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
        spells[i] = translate_spell(spells[i]);
}

// When passed the number for a player spell, returns the equivalent
// monster spell.  Returns SPELL_NO_SPELL on failure (no equivalent).
spell_type ghost_demon::translate_spell(spell_type spel) const
{
    switch (spel)
    {
    case SPELL_CONTROLLED_BLINK:
        return (SPELL_BLINK);        // approximate
    case SPELL_DEMONIC_HORDE:
        return (SPELL_CALL_IMP);
    case SPELL_AGONY:
    case SPELL_SYMBOL_OF_TORMENT:
        // Too powerful to give ghosts Torment for Agony?  Nah.
        return (SPELL_SYMBOL_OF_TORMENT);
    case SPELL_DELAYED_FIREBALL:
        return (SPELL_FIREBALL);
    case SPELL_PETRIFY:
        return (SPELL_PARALYSE);
    case SPELL_EVAPORATE:
        return (SPELL_MEPHITIC_CLOUD);
    default:
        break;
    }

    return (spel);
}

std::vector<ghost_demon> ghost_demon::find_ghosts()
{
    std::vector<ghost_demon> gs;

    if (!you.is_undead)
    {
        ghost_demon player;
        player.init_player_ghost();
        announce_ghost(player);
        gs.push_back(player);
    }

    // Pick up any other ghosts that happen to be on the level if we
    // have space.  If the player is undead, add one to the ghost quota
    // for the level.
    find_extra_ghosts(gs, n_extra_ghosts() + 1 - gs.size());

    return (gs);
}

void ghost_demon::find_transiting_ghosts(
    std::vector<ghost_demon> &gs, int n)
{
    if (n <= 0)
        return;

    const m_transit_list *mt = get_transit_list(level_id::current());
    if (mt)
    {
        for (m_transit_list::const_iterator i = mt->begin();
             i != mt->end() && n > 0; ++i)
        {
            if (i->mons.type == MONS_PLAYER_GHOST)
            {
                const monsters &m = i->mons;
                if (m.ghost.get())
                {
                    announce_ghost(*m.ghost);
                    gs.push_back(*m.ghost);
                    --n;
                }
            }
        }
    }
}

void ghost_demon::announce_ghost(const ghost_demon &g)
{
#if defined(DEBUG_BONES) || defined(DEBUG_DIAGNOSTICS)
    mprf(MSGCH_DIAGNOSTICS, "Saving ghost: %s", g.name.c_str());
#endif
}

void ghost_demon::find_extra_ghosts( std::vector<ghost_demon> &gs, int n )
{
    for (monster_iterator mi; mi && n > 0; ++mi)
    {
        if (mi->type == MONS_PLAYER_GHOST && mi->ghost.get())
        {
            // Bingo!
            announce_ghost(*(mi->ghost));
            gs.push_back(*(mi->ghost));
            --n;
        }
    }

    // Check the transit list for the current level.
    find_transiting_ghosts(gs, n);
}

// Returns the number of extra ghosts allowed on the level.
int ghost_demon::n_extra_ghosts()
{
    const int lev = you.absdepth0 + 1;
    const int subdepth = subdungeon_depth(you.where_are_you, you.absdepth0);

    if (you.level_type == LEVEL_PANDEMONIUM
        || you.level_type == LEVEL_ABYSS
        || (you.level_type == LEVEL_DUNGEON
            && (you.where_are_you == BRANCH_CRYPT
                || you.where_are_you == BRANCH_TOMB
                || you.where_are_you == BRANCH_HALL_OF_ZOT
                || player_in_hell()))
        || lev > 22)
    {
        return (MAX_GHOSTS - 1);
    }

    if (you.where_are_you == BRANCH_ECUMENICAL_TEMPLE)
        return (0);

    // No multiple ghosts until level 9 of the main dungeon.
    if (lev < 9 && you.where_are_you == BRANCH_MAIN_DUNGEON
        || subdepth < 2 && you.where_are_you == BRANCH_LAIR
        || subdepth < 2 && you.where_are_you == BRANCH_ORCISH_MINES)
    {
        return (0);
    }

    if (you.where_are_you == BRANCH_LAIR
        || you.where_are_you == BRANCH_ORCISH_MINES
        || you.where_are_you == BRANCH_MAIN_DUNGEON && lev < 15)
    {
        return (1);
    }

    return (1 + x_chance_in_y(lev, 20) + x_chance_in_y(lev, 40));
}

// Sanity checks for some ghost values.
bool debug_check_ghosts()
{
    for (unsigned int k = 0; k < ghosts.size(); ++k)
    {
        ghost_demon ghost = ghosts[k];
        // Values greater than the allowed maximum or less then the
        // allowed minimum signalise bugginess.
        if (ghost.damage < 0 || ghost.damage > MAX_GHOST_DAMAGE)
            return (false);
        if (ghost.max_hp < 1 || ghost.max_hp > MAX_GHOST_HP)
            return (false);
        if (ghost.xl < 1 || ghost.xl > 27)
            return (false);
        if (ghost.ev > MAX_GHOST_EVASION)
            return (false);
        if (ghost.speed < MIN_GHOST_SPEED || ghost.speed > MAX_GHOST_SPEED)
            return (false);
        if (ghost.resists.fire < -3 || ghost.resists.fire > 3)
            return (false);
        if (ghost.resists.cold < -3 || ghost.resists.cold > 3)
            return (false);
        if (ghost.resists.elec < 0)
            return (false);
        if (ghost.brand < SPWPN_NORMAL || ghost.brand > MAX_PAN_LORD_BRANDS)
            return (false);
        if (ghost.species < 0 || ghost.species >= NUM_SPECIES)
            return (false);
        if (ghost.job < JOB_FIGHTER || ghost.job >= NUM_JOBS)
            return (false);
        if (ghost.best_skill < SK_FIGHTING || ghost.best_skill >= NUM_SKILLS)
            return (false);
        if (ghost.best_skill_level < 0 || ghost.best_skill_level > 27)
            return (false);
        if (ghost.religion < GOD_NO_GOD || ghost.religion >= NUM_GODS)
            return (false);

        if (ghost.brand == SPWPN_HOLY_WRATH || is_good_god(ghost.religion))
            return (false);

        // Only (very) ugly things get non-plain attack types and
        // flavours.
        if (ghost.att_type != AT_HIT || ghost.att_flav != AF_PLAIN)
            return (false);

        // Only Pandemonium lords cycle colours.
        if (ghost.cycle_colours)
            return (false);

        // Name validation.
        if (!validate_player_name(ghost.name, false))
            return (false);
        if (ghost.name.length() > (kNameLen - 1) || ghost.name.length() == 0)
            return (false);
        if (ghost.name != trimmed_string(ghost.name))
            return (false);

        // Check for non-existing spells.
        for (int sp = 0; sp < NUM_MONSTER_SPELL_SLOTS; ++sp)
            if (ghost.spells[sp] < 0 || ghost.spells[sp] >= NUM_SPELLS)
                return (false);
    }
    return (true);
}
