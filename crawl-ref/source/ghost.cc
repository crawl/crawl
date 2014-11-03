/**
 * @file
 * @brief Player ghost and random Pandemonium demon handling.
**/

#include "AppHdr.h"

#include "ghost.h"

#include <vector>

#include "act-iter.h"
#include "colour.h"
#include "database.h"
#include "env.h"
#include "itemname.h"
#include "itemprop.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-transit.h"
#include "ng-input.h"
#include "skills.h"
#include "spl-util.h"
#include "stringutil.h"

#define MAX_GHOST_DAMAGE     50
#define MAX_GHOST_HP        400
#define MAX_GHOST_EVASION    60
#define MIN_GHOST_SPEED       6
#define MAX_GHOST_SPEED      13

vector<ghost_demon> ghosts;

// Pan lord conjuration spell list.
static spell_type search_order_conj[] =
{
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_FIRE_STORM,
    SPELL_GLACIATE,
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
    SPELL_VENOM_BOLT,
    SPELL_IRON_SHOT,
    SPELL_LRD,
    SPELL_STONE_ARROW,
    SPELL_FORCE_LANCE,
    SPELL_DISCHARGE,
    SPELL_DAZZLING_SPRAY,
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
    SPELL_FREEZE,
    SPELL_PAIN,
    SPELL_STING,
    SPELL_SHOCK,
    SPELL_SANDBLAST,
    SPELL_MAGIC_DART,
    SPELL_HIBERNATION,
    SPELL_FLAME_TONGUE,
    SPELL_CORONA,
    SPELL_NO_SPELL,                        // end search
};

// Pan lord self-enchantment / summoning spell list.
static spell_type search_order_selfench[] =
{
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_SUMMON_GREATER_DEMON,
    SPELL_SUMMON_DRAGON,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_HAUNT,
    SPELL_SUMMON_HYDRA,
    SPELL_SUMMON_DEMON,
    SPELL_HASTE,
    SPELL_SILENCE,
    SPELL_BATTLESPHERE,
    SPELL_SUMMON_BUTTERFLIES,
    SPELL_SUMMON_SWARM,
    SPELL_MONSTROUS_MENAGERIE,
    SPELL_SWIFTNESS,
    SPELL_SUMMON_ICE_BEAST,
    SPELL_ANIMATE_DEAD,
    SPELL_TWISTED_RESURRECTION,
    SPELL_INVISIBILITY,
    SPELL_CALL_IMP,
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_MALIGN_GATEWAY,
    SPELL_BLINK,
    SPELL_NO_SPELL,                        // end search
    // No Simulacrum: iffy for pghosts (picking up material components),
    // largely useless on Pan lords.
};

// Pan lord misc spell list.
static spell_type search_order_misc[] =
{
    SPELL_SHATTER,
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_BANISHMENT,
    SPELL_FREEZING_CLOUD,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_MASS_CONFUSION,
    SPELL_ENGLACIATION,
    SPELL_DISPEL_UNDEAD,
    SPELL_PARALYSE,
    SPELL_CONFUSE,
    SPELL_MEPHITIC_CLOUD,
    SPELL_SLOW,
    SPELL_PETRIFY,
    SPELL_POLYMORPH,
    SPELL_TELEPORT_OTHER,
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
    resists          = 0;
    cycle_colours    = false;
    colour           = BLACK;
    fly              = FL_NONE;
    acting_part      = MONS_0;
}

/**
 * Choose a random brand for a pandemonium lord's melee attacks.
 *
 * @return  A random valid brand type (not holy wrath, protection, etc)
 */
static brand_type _random_special_pan_lord_brand()
{
    return random_choose(SPWPN_FLAMING,
                         SPWPN_FREEZING,
                         SPWPN_ELECTROCUTION,
                         SPWPN_VENOM,
                         SPWPN_DRAINING,
                         SPWPN_SPEED,
                         SPWPN_VORPAL,
                         SPWPN_VAMPIRISM,
                         SPWPN_PAIN,
                         SPWPN_ANTIMAGIC,
                         SPWPN_DISTORTION,
                         SPWPN_CHAOS,
                         -1);
}

#define ADD_SPELL(which_spell) \
    { \
        slot.spell = which_spell; \
        if (slot.spell != SPELL_NO_SPELL) \
            spells.push_back(slot); \
    }

void ghost_demon::init_random_demon()
{
    mon_spell_slot slot;
    slot.flags = MON_SPELL_DEMONIC;

    do name = make_name(random_int(), false);
        while (!getLongDescription(name).empty());

    // hp - could be defined below (as could ev, AC, etc.). Oh well, too late:
    max_hp = 100 + roll_dice(3, 50);

    ev = 5 + random2(20);
    ac = 5 + random2(20);

    see_invis = true;

    resists = 0;

    if (!one_chance_in(3))
        resists |= MR_RES_FIRE * random_range(1, 2);
    else if (one_chance_in(10))
        resists |= MR_VUL_FIRE;

    if (!one_chance_in(3))
        resists |= MR_RES_COLD * random_range(1, 2);
    else
        resists |= MR_VUL_COLD;

    // Demons, like ghosts, automatically get poison res. and life prot.

    // resist electricity:
    if (one_chance_in(3))
        resists |= MR_RES_ELEC; // no rElec++ for Pan lords, because of witches

    // HTH damage:
    damage = 20 + roll_dice(2, 20);

    // Does demon fly?
    fly = (one_chance_in(3) ? FL_NONE :
           one_chance_in(5) ? FL_LEVITATE
                            : FL_WINGED);

    // hit dice:
    xl = 10 + roll_dice(2, 10);

    // Is demon a spellcaster?
    // Non-spellcasters always have branded melee and are faster instead.
    const bool spellcaster = x_chance_in_y(3,4);

    if (one_chance_in(3) || !spellcaster)
        brand = _random_special_pan_lord_brand();
    else
        brand = SPWPN_NORMAL;

    // Non-caster demons are fast, casters may get haste.
    if (!spellcaster)
        speed = 11 + roll_dice(2,4);
    else if (one_chance_in(3))
        speed = 10;
    else
        speed = 8 + roll_dice(2,5);

    spells.clear();

    if (spellcaster)
    {
        // This bit uses the list of player spells to find appropriate
        // spells for the demon, then converts those spells to the monster
        // spell indices.  Some special monster-only spells are at the end.

        if (coinflip())
            ADD_SPELL(RANDOM_ELEMENT(search_order_conj));

        // Might duplicate the first spell, but that isn't a problem.
        if (coinflip())
            ADD_SPELL(RANDOM_ELEMENT(search_order_conj));

        ADD_SPELL(one_chance_in(4) ? SPELL_SUMMON_DEMON
                                   : RANDOM_ELEMENT(search_order_selfench));

        if (coinflip())
        {
            spell_type spell = RANDOM_ELEMENT(search_order_misc);
            if (spell != SPELL_DIG)
                ADD_SPELL(spell);
        }

        if (coinflip())
            ADD_SPELL(RANDOM_ELEMENT(search_order_misc));

        ADD_SPELL(random_choose_weighted(2, SPELL_TELEPORT_SELF,
                                         1, SPELL_BLINK,
                                         1, SPELL_NO_SPELL,
                                         0));

        // Give demon a chance for some monster-only spells.
        // Demon-summoning should be fairly common.
        if (one_chance_in(4))
        {
            ADD_SPELL(random_choose(SPELL_HELLFIRE_BURST,
                                    SPELL_FIRE_STORM,
                                    SPELL_GLACIATE,
                                    SPELL_METAL_SPLINTERS,
           /* eye of devastation */ SPELL_ENERGY_BOLT,
                                    SPELL_ORB_OF_ELECTRICITY,
                                    -1));
        }

        if (one_chance_in(25))
            ADD_SPELL(SPELL_STEAM_BALL);
        if (one_chance_in(25))
            ADD_SPELL(SPELL_QUICKSILVER_BOLT);
        if (one_chance_in(25))
            ADD_SPELL(SPELL_HELLFIRE);
        if (one_chance_in(25))
            ADD_SPELL(SPELL_IOOD);

        if (one_chance_in(25))
            ADD_SPELL(SPELL_SMITING);
        if (one_chance_in(25))
            ADD_SPELL(SPELL_HELLFIRE_BURST);
        if (one_chance_in(22))
            ADD_SPELL(SPELL_SUMMON_HYDRA);
        if (one_chance_in(20))
            ADD_SPELL(SPELL_SUMMON_DRAGON);
        if (one_chance_in(12))
            ADD_SPELL(SPELL_SUMMON_GREATER_DEMON);
        if (one_chance_in(12))
            ADD_SPELL(SPELL_SUMMON_DEMON);
        if (one_chance_in(10))
            ADD_SPELL(SPELL_SUMMON_EYEBALLS);

        if (one_chance_in(20))
            ADD_SPELL(SPELL_SUMMON_GREATER_DEMON);
        if (one_chance_in(20))
            ADD_SPELL(SPELL_SUMMON_DEMON);
        if (one_chance_in(20))
            ADD_SPELL(SPELL_MALIGN_GATEWAY);

        if (one_chance_in(15))
            ADD_SPELL(SPELL_DIG);

        fixup_spells(spells, xl, true, false);
    }

    // Does demon cycle colours?
    cycle_colours = one_chance_in(10);

    colour = random_colour();

}

// Returns the movement speed for a player ghost.  Note that this is a
// real speed, not a movement cost, so higher is better.
static int _player_ghost_base_movement_speed()
{
    int speed = 10;

    if (int fast = player_mutation_level(MUT_FAST, false))
        speed += fast + 1;
    if (int slow = player_mutation_level(MUT_SLOW, false))
        speed -= slow + 1;

    if (you.wearing_ego(EQ_BOOTS, SPARM_RUNNING))
        speed += 1;

    // Cap speeds.
    if (speed < MIN_GHOST_SPEED)
        speed = MIN_GHOST_SPEED;
    else if (speed > MAX_GHOST_SPEED)
        speed = MAX_GHOST_SPEED;

    return speed;
}

void ghost_demon::init_player_ghost()
{
    name   = you.your_name;
    max_hp = ((get_real_hp(false) >= MAX_GHOST_HP)
             ? MAX_GHOST_HP
             : get_real_hp(false));
    ev     = player_evasion(EV_IGNORE_HELPLESS);
    ac     = you.armour_class();

    if (ev > MAX_GHOST_EVASION)
        ev = MAX_GHOST_EVASION;

    see_invis      = you.can_see_invisible();
    resists        = 0;
    set_resist(resists, MR_RES_FIRE, player_res_fire());
    set_resist(resists, MR_RES_COLD, player_res_cold());
    set_resist(resists, MR_RES_ELEC, player_res_electricity());
    // clones might lack innate rPois, copy it.  pghosts don't care.
    set_resist(resists, MR_RES_POISON, player_res_poison());
    set_resist(resists, MR_RES_NEG, you.res_negative_energy());
    set_resist(resists, MR_RES_ACID, player_res_acid());
    // multi-level for players, boolean as an innate monster resistance
    set_resist(resists, MR_RES_STEAM, player_res_steam() ? 1 : 0);
    set_resist(resists, MR_RES_STICKY_FLAME, player_res_sticky_flame());
    set_resist(resists, MR_RES_ASPHYX, you.res_asphyx());
    set_resist(resists, MR_RES_ROTTING, you.res_rotting());
    set_resist(resists, MR_RES_PETRIFY, you.res_petrify());
    speed          = _player_ghost_base_movement_speed();

    damage = 4;
    brand = SPWPN_NORMAL;

    if (you.weapon())
    {
        // This includes ranged weapons, but they're treated as melee.

        const item_def& weapon = *you.weapon();
        if (is_weapon(weapon))
        {
            damage = property(weapon, PWPN_DAMAGE);

            // Bows skill doesn't make bow-bashing better.
            skill_type sk = is_range_weapon(weapon) ? SK_FIGHTING
                                                    : item_attack_skill(weapon);
            damage *= 25 + you.skills[sk];
            damage /= 25;

            if (weapon.base_type == OBJ_WEAPONS)
            {
                brand = static_cast<brand_type>(get_weapon_brand(weapon));

                // Ghosts can't get holy wrath, but they get to keep
                // the weapon.
                if (brand == SPWPN_HOLY_WRATH)
                    brand = SPWPN_NORMAL;

                // Don't copy ranged- or artefact-only brands (reaping etc.).
                if (brand > MAX_GHOST_BRAND)
                    brand = SPWPN_NORMAL;
            }
            else if (weapon.base_type == OBJ_STAVES)
            {
                switch (static_cast<stave_type>(weapon.sub_type))
                {
                // very bad approximations
                case STAFF_FIRE: brand = SPWPN_FLAMING; break;
                case STAFF_COLD: brand = SPWPN_FREEZING; break;
                case STAFF_POISON: brand = SPWPN_VENOM; break;
                case STAFF_DEATH: brand = SPWPN_PAIN; break;
                case STAFF_AIR: brand = SPWPN_ELECTROCUTION; break;
                case STAFF_EARTH: brand = SPWPN_VORPAL; break;
                default: ;
                }
            }
        }
    }
    else
    {
        // Unarmed combat.
        if (you.innate_mutation[MUT_CLAWS])
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

    religion = you.religion;

    best_skill = ::best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    best_skill_level = you.skills[best_skill];
    xl = you.experience_level;

    // These are the same as in mon-data.h.
    colour = WHITE;
    fly = FL_LEVITATE;

    add_spells();
}

static colour_t _ugly_thing_assign_colour(colour_t force_colour,
                                          colour_t force_not_colour)
{
    colour_t colour;

    if (force_colour != BLACK)
        colour = force_colour;
    else
    {
        do
            colour = ugly_thing_random_colour();
        while (force_not_colour != BLACK && colour == force_not_colour);
    }

    return colour;
}

static attack_flavour _very_ugly_thing_flavour_upgrade(attack_flavour u_att_flav)
{
    switch (u_att_flav)
    {
    case AF_FIRE:
        u_att_flav = AF_STICKY_FLAME;
        break;

    case AF_POISON:
        u_att_flav = AF_POISON_STRONG;
        break;

    default:
        break;
    }

    return u_att_flav;
}

static attack_flavour _ugly_thing_colour_to_flavour(colour_t u_colour)
{
    attack_flavour u_att_flav = AF_PLAIN;

    switch (make_low_colour(u_colour))
    {
    case RED:
        u_att_flav = AF_FIRE;
        break;

    case BROWN:
        u_att_flav = AF_ACID;
        break;

    case GREEN:
        u_att_flav = AF_POISON;
        break;

    case CYAN:
        u_att_flav = AF_ELEC;
        break;

    case LIGHTGREY:
        u_att_flav = AF_COLD;
        break;

    default:
        break;
    }

    if (is_high_colour(u_colour))
        u_att_flav = _very_ugly_thing_flavour_upgrade(u_att_flav);

    return u_att_flav;
}

/**
 * Init a ghost demon object corresponding to an ugly thing monster.
 *
 * @param very_ugly     Whether the ugly thing is a very ugly thing.
 * @param only_mutate   Whether to mutate the ugly thing's colour away from its
 *                      old colour (the force_colour).
 * @param force_colour  The ugly thing's colour. (Default BLACK = random)
 */
void ghost_demon::init_ugly_thing(bool very_ugly, bool only_mutate,
                                  colour_t force_colour)
{
    const monster_type type = very_ugly ? MONS_VERY_UGLY_THING
                                        : MONS_UGLY_THING;
    const monsterentry* stats = get_monster_data(type);

    speed = stats->speed;
    ev = stats->ev;
    ac = stats->AC;
    damage = stats->attack[0].damage;

    // If we're mutating an ugly thing, leave its experience level, hit
    // dice and maximum hit points as they are.
    if (!only_mutate)
    {
        xl = stats->hpdice[0];
        max_hp = hit_points(xl, stats->hpdice[1], stats->hpdice[2]);
    }

    const attack_type att_types[] =
    {
        AT_BITE, AT_STING, AT_ENGULF, AT_CLAW, AT_PECK, AT_HEADBUTT, AT_PUNCH,
        AT_KICK, AT_TENTACLE_SLAP, AT_TAIL_SLAP, AT_GORE, AT_TRUNK_SLAP
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
    if (colour == MAGENTA)
        damage = damage * 4 / 3; // +5 for uglies, +9 for v uglies

    // Pick a compatible resistance for this attack flavour.
    ugly_thing_add_resistance(false, att_flav);

    // If this is a very ugly thing, upgrade it properly.
    if (very_ugly)
        ugly_thing_to_very_ugly_thing();
}

void ghost_demon::ugly_thing_to_very_ugly_thing()
{
    // A very ugly thing always gets a high-intensity colour.
    colour = make_high_colour(colour);

    // A very ugly thing sometimes gets an upgraded attack flavour.
    att_flav = _very_ugly_thing_flavour_upgrade(att_flav);

    // Pick a compatible resistance for this attack flavour.
    ugly_thing_add_resistance(true, att_flav);
}

static resists_t _ugly_thing_resists(bool very_ugly, attack_flavour u_att_flav)
{
    switch (u_att_flav)
    {
    case AF_FIRE:
    case AF_STICKY_FLAME:
        return MR_RES_FIRE * (very_ugly ? 2 : 1) | MR_RES_STICKY_FLAME;

    case AF_ACID:
        return MR_RES_ACID;

    case AF_POISON:
    case AF_POISON_STRONG:
        return MR_RES_POISON * (very_ugly ? 2 : 1);

    case AF_ELEC:
        return MR_RES_ELEC * (very_ugly ? 2 : 1);

    case AF_COLD:
        return MR_RES_COLD * (very_ugly ? 2 : 1);

    default:
        return 0;
    }
}

void ghost_demon::ugly_thing_add_resistance(bool very_ugly,
                                            attack_flavour u_att_flav)
{
    resists = _ugly_thing_resists(very_ugly, u_att_flav);
}

void ghost_demon::init_dancing_weapon(const item_def& weapon, int power)
{
    int delay = property(weapon, PWPN_SPEED);
    int damg  = property(weapon, PWPN_DAMAGE);

    if (power > 100)
        power = 100;

    colour = weapon.get_colour();
    fly = FL_LEVITATE;

    // We want Tukima to reward characters who invest heavily in
    // Hexes skill. Therefore, weapons benefit from very high skill.

    // First set up what the monsters will look like with 100 power.
    // Daggers are weak here! In the table, "44+22" means d44+d22 with
    // d22 being base damage and d44 coming from power.
    // Giant spiked club: speed 12, 44+22 damage, 22 AC, 36 HP, 16 EV
    // Bardiche:          speed 10, 40+20 damage, 18 AC, 40 HP, 15 EV
    // Dagger:            speed 20,  8+ 4 damage,  4 AC, 20 HP, 20 EV
    // Quick blade:       speed 23, 10+ 5 damage,  5 AC, 14 HP, 22 EV
    // Rapier:            speed 18, 14+ 7 damage,  7 AC, 24 HP, 19 EV

    xl = 15;

    speed   = 30 - delay;
    ev      = 25 - delay / 2;
    ac      = damg;
    damage  = 2 * damg;
    max_hp  = delay * 2;

    // Don't allow the speed to become too low.
    speed = max(3, (speed / 2) * (1 + power / 100));

    ev    = max(3, ev * power / 100);
    ac = ac * power / 100;
    max_hp = max(5, max_hp * power / 100);
    damage = max(1, damage * power / 100);
}

void ghost_demon::init_spectral_weapon(const item_def& weapon,
                                       int power, int wpn_skill)
{
    int damg  = property(weapon, PWPN_DAMAGE);

    if (power > 100)
        power = 100;

    // skill is on a 10 scale
    if (wpn_skill > 270)
        wpn_skill = 270;

    colour = weapon.get_colour();
    fly = FL_LEVITATE;

    // Hit dice (to hit) scales with weapon skill alone.
    // Damage scales with weapon skill, but how well depends on spell power.
    // Defenses scale with spell power alone.
    // Appropriate investment is rewarded with a stronger spectral weapon.

    xl = max(wpn_skill / 10, 1);

    // At 0 power, weapon skill is 1/3 as effective as on the player
    // At max power, weapon skill is as effective as on the player.
    // Power has a linear effect between those endpoints.
    // It's possible this ends up too strong,
    // but 100 power on Hexes/Charms will take significant investment
    // most players wouldn't otherwise get.
    //
    // Damage multiplier table:
    //     |            weapon skill
    // pow |   3       9       15      21      27
    // --- |   -----   ----    ----    ----    ----
    // 0   |   1.04    1.12    1.20    1.28    1.36
    // 10  |   1.05    1.14    1.24    1.34    1.43
    // 20  |   1.06    1.17    1.28    1.39    1.50
    // 30  |   1.06    1.19    1.32    1.45    1.58
    // 40  |   1.07    1.22    1.36    1.50    1.65
    // 50  |   1.08    1.24    1.40    1.56    1.72
    // 60  |   1.09    1.26    1.44    1.62    1.79
    // 70  |   1.10    1.29    1.48    1.67    1.87
    // 80  |   1.10    1.31    1.52    1.73    1.94
    // 90  |   1.11    1.34    1.56    1.79    2.01
    // 100 |   1.12    1.36    1.60    1.84    2.08
    damage  = damg;
    int scale = 250 * 150 / (50 + power);
    damage *= scale + wpn_skill;
    damage /= scale;

    speed   = 30;
    ev      = 10 + div_rand_round(power,10);
    ac      = 2 + div_rand_round(power,10);
    max_hp  = 10 + div_rand_round(power,3);
}

// Used when creating ghosts: goes through and finds spells for the
// ghost to cast.  Death is a traumatic experience, so ghosts only
// remember a few spells.
void ghost_demon::add_spells()
{
    spells.clear();
    mon_spell_slot slot;
    slot.flags = MON_SPELL_WIZARD;

    for (int i = 0; i < you.spell_no; i++)
    {
        const int chance = max(0, 50 - spell_fail(you.spells[i]));
        const spell_type spell = translate_spell(you.spells[i]);
        if (spell != SPELL_NO_SPELL
            && !(get_spell_flags(spell) & SPFLAG_NO_GHOST)
            && is_valid_mon_spell(spell)
            && x_chance_in_y(chance*chance, 50*50))
        {
            slot.spell = spell;
            spells.push_back(slot);
        }
    }

    fixup_spells(spells, xl, true, false);

    if (species_genus(species) == GENPC_DRACONIAN
        && species != SP_BASE_DRACONIAN
        && species != SP_GREY_DRACONIAN)
    {
        slot.spell = SPELL_BOLT_OF_DRAINING;
        slot.freq  = 33; // Not too common
        slot.flags = MON_SPELL_NATURAL | MON_SPELL_BREATH;
        spells.push_back(slot);
    }
}

bool ghost_demon::has_spells() const
{
    return spells.size() > 0;
}

// When passed the number for a player spell, returns the equivalent
// monster spell.  Returns SPELL_NO_SPELL on failure (no equivalent).
spell_type ghost_demon::translate_spell(spell_type spell) const
{
    switch (spell)
    {
    case SPELL_CONTROLLED_BLINK:
        return SPELL_BLINK;        // approximate
    case SPELL_DELAYED_FIREBALL:
        return SPELL_FIREBALL;
    case SPELL_DRAGON_CALL:
        return SPELL_SUMMON_DRAGON;
    default:
        break;
    }

    return spell;
}

vector<ghost_demon> ghost_demon::find_ghosts()
{
    vector<ghost_demon> gs;

    if (you.undead_state(false) == US_ALIVE)
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

    return gs;
}

void ghost_demon::find_transiting_ghosts(
    vector<ghost_demon> &gs, int n)
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
                const monster& m = i->mons;
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

void ghost_demon::find_extra_ghosts(vector<ghost_demon> &gs, int n)
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
    if (env.absdepth0 < 10)
        return 0;

    return MAX_GHOSTS - 1;
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
            return false;
        if (ghost.max_hp < 1 || ghost.max_hp > MAX_GHOST_HP)
            return false;
        if (ghost.xl < 1 || ghost.xl > 27)
            return false;
        if (ghost.ev > MAX_GHOST_EVASION)
            return false;
        if (ghost.speed < MIN_GHOST_SPEED || ghost.speed > MAX_GHOST_SPEED)
            return false;
        if (get_resist(ghost.resists, MR_RES_ELEC) < 0)
            return false;
        if (ghost.brand < SPWPN_NORMAL || ghost.brand > MAX_GHOST_BRAND)
            return false;
        if (ghost.species < 0 || ghost.species >= NUM_SPECIES)
            return false;
        if (ghost.job < JOB_FIGHTER || ghost.job >= NUM_JOBS)
            return false;
        if (ghost.best_skill < SK_FIGHTING || ghost.best_skill >= NUM_SKILLS)
            return false;
        if (ghost.best_skill_level < 0 || ghost.best_skill_level > 27)
            return false;
        if (ghost.religion < GOD_NO_GOD || ghost.religion >= NUM_GODS)
            return false;

        if (ghost.brand == SPWPN_HOLY_WRATH)
            return false;

        // Only (very) ugly things get non-plain attack types and
        // flavours.
        if (ghost.att_type != AT_HIT || ghost.att_flav != AF_PLAIN)
            return false;

        // Only Pandemonium lords cycle colours.
        if (ghost.cycle_colours)
            return false;

        // Name validation.
        if (!validate_player_name(ghost.name, false))
            return false;
        // Many combining characters can come per every letter, but if there's
        // that much, it's probably a maliciously forged ghost of some kind.
        if (ghost.name.length() > kNameLen * 10 || ghost.name.empty())
            return false;
        if (ghost.name != trimmed_string(ghost.name))
            return false;

        // Check for non-existing spells.
        for (unsigned int sp = 0; sp < ghost.spells.size(); ++sp)
            if (ghost.spells[sp].spell < 0 || ghost.spells[sp].spell >= NUM_SPELLS)
                return false;
    }
    return true;
}

int ghost_level_to_rank(const int xl)
{
    if (xl <  4) return 0;
    if (xl <  7) return 1;
    if (xl < 11) return 2;
    if (xl < 16) return 3;
    if (xl < 22) return 4;
    if (xl < 26) return 5;
    if (xl < 27) return 6;
    return 7;
}

// Approximate inverse, in the middle of the range
int ghost_rank_to_level(const int rank)
{
    switch (rank)
    {
    case 0:
        return 2;
    case 1:
        return 5;
    case 2:
        return 9;
    case 3:
        return 13;
    case 4:
        return 19;
    case 5:
        return 24;
    case 6:
        return 26;
    case 7:
        return 27;
    default:
        die("Bad ghost rank %d", rank);
    }
}

static spell_type servitor_spells[] =
{
    // primary spells
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_IOOD,
    SPELL_IRON_SHOT,
    SPELL_BOLT_OF_FIRE,
    SPELL_BOLT_OF_COLD,
    SPELL_POISON_ARROW,
    SPELL_LIGHTNING_BOLT,
    SPELL_BOLT_OF_MAGMA,
    SPELL_BOLT_OF_DRAINING,
    SPELL_VENOM_BOLT,
    SPELL_THROW_ICICLE,
    SPELL_STONE_ARROW,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    // secondary spells
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_FIREBALL,
    SPELL_AIRSTRIKE,
    SPELL_LRD,
    SPELL_FREEZING_CLOUD,
    SPELL_POISONOUS_CLOUD,
    SPELL_FORCE_LANCE,
    SPELL_DAZZLING_SPRAY,
    SPELL_MEPHITIC_CLOUD,
    // fallback spells
    SPELL_STICKY_FLAME,
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
    SPELL_FREEZE,
    SPELL_FLAME_TONGUE,
    SPELL_STING,
    SPELL_SANDBLAST,
    SPELL_MAGIC_DART,
    // end search
    SPELL_NO_SPELL,
};

void ghost_demon::init_spellforged_servitor(actor* caster)
{
    mon_spell_slot slot;
    slot.flags = MON_SPELL_WIZARD;
    monster* mon = caster->is_monster() ? caster->as_monster() : NULL;

    int pow = mon ? 12 * mon->spell_hd(SPELL_SPELLFORGED_SERVITOR)
                  : calc_spell_power(SPELL_SPELLFORGED_SERVITOR, true);

    colour = LIGHTMAGENTA; // cf. mon-data.h
    speed = 10;
    ev = 10;
    ac = 10;
    xl = 9 + div_rand_round(pow, 14);
    max_hp = 80;
    damage = 0;
    att_type = AT_NONE;

    int i = 0;
    spell_type spell = SPELL_NO_SPELL;
    while ((spell = servitor_spells[i++]) != SPELL_NO_SPELL)
    {
        if (mon && mon->has_spell(spell)
            || !mon && you.has_spell(spell) && spell_fail(spell) < 50)
        {
            slot.spell = spell;
            spells.push_back(slot);
        }
    }

    fixup_spells(spells, 150, true, false);
}
