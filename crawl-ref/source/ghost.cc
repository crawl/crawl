/*
 * File:    ghost.cc
 * Summary: Player ghost and random Pandemonium demon handling.
 *
 * Created for Dungeon Crawl Reference by dshaligram on
 * Thu Mar 15 20:10:20 2007 UTC.
 *
 * Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "ghost.h"

#include "externs.h"
#include "itemname.h"
#include "itemprop.h"
#include "newgame.h"
#include "randart.h"
#include "skills2.h"
#include "stuff.h"
#include "mtransit.h"
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
// 0
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_FIRE_STORM,
    SPELL_ICE_STORM,
    SPELL_BOLT_OF_DRAINING,
    SPELL_AGONY,
    SPELL_DISINTEGRATE,
    SPELL_LIGHTNING_BOLT,
    SPELL_STICKY_FLAME,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_BOLT_OF_MAGMA,
    SPELL_ICE_BOLT,
    SPELL_BOLT_OF_FIRE,
// 10
    SPELL_BOLT_OF_COLD,
    SPELL_FIREBALL,
    SPELL_DELAYED_FIREBALL,
    SPELL_VENOM_BOLT,
    SPELL_BOLT_OF_IRON,
    SPELL_STONE_ARROW,
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
    SPELL_PAIN,
    SPELL_STING,
// 20
    SPELL_SHOCK,
    SPELL_SANDBLAST,
    SPELL_MAGIC_DART,
    SPELL_SLEEP,
    SPELL_BACKLIGHT,
    SPELL_NO_SPELL,                        // end search
};

// Order for looking for summonings and self-enchants for the 3rd spell
// slot.
static spell_type search_order_third[] = {
// 0
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_SUMMON_GREATER_DEMON,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_SUMMON_WRAITHS,
    SPELL_SUMMON_DEMON,
    SPELL_DEMONIC_HORDE,
    SPELL_HASTE,
    SPELL_SUMMON_UGLY_THING,
    SPELL_SUMMON_ICE_BEAST,
    SPELL_ANIMATE_DEAD,
// 10
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
    SPELL_DIG,
    SPELL_BACKLIGHT,
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
    resists          = mon_resist_def();
    spellcaster      = false;
    cycle_colours    = false;
    fly              = FL_NONE;
}

void ghost_demon::init_random_demon()
{
    name = make_name(random_int(), false);

    // hp - could be defined below (as could ev, AC etc). Oh well, too late:
    max_hp = 100 + roll_dice( 3, 50 );

    ev = 5 + random2(20);
    ac = 5 + random2(20);

    see_invis = !one_chance_in(10);

    if (!one_chance_in(3))
        resists.fire = random_range(1, 2);
    else
    {
        resists.fire = 0;    // res_fire

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
    damage = 20 + roll_dice( 2, 20 );

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
    // and I expect named demons to typically have a trick or two -- bwr
    spellcaster = !one_chance_in(10);

    // Does demon fly?
    fly = (one_chance_in(3) ? FL_NONE :
           one_chance_in(5) ? FL_LEVITATE : FL_FLY);

    // hit dice:
    xl = 10 + roll_dice(2, 10);

    // Does demon cycle colours?
    cycle_colours = one_chance_in(10);

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
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
            spells[i] = translate_spell( spells[i] );

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
            spells[2] = SPELL_SMITING;
        if (one_chance_in(25))
            spells[2] = SPELL_HELLFIRE_BURST;
        if (one_chance_in(12))
            spells[2] = SPELL_SUMMON_GREATER_DEMON;
        if (one_chance_in(12))
            spells[2] = SPELL_SUMMON_DEMON;

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

// Returns the movement speed for a player ghost. Note that this is a real
// speed, not a movement cost, so higher is better.
static int _player_ghost_base_movement_speed()
{
    int speed = (you.species == SP_NAGA ? 8 : 10);

    if (player_mutation_level(MUT_FAST))
        speed += player_mutation_level(MUT_FAST) + 1;

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
    max_hp = ((you.hp_max >= MAX_GHOST_HP) ? MAX_GHOST_HP : you.hp_max);
    ev     = player_evasion();
    ac     = player_AC();

    if (ev > MAX_GHOST_EVASION)
        ev = MAX_GHOST_EVASION;

    see_invis      = player_see_invis();
    resists.fire   = player_res_fire();
    resists.cold   = player_res_cold();
    resists.elec   = player_res_electricity();
    speed          = _player_ghost_base_movement_speed();

    damage = 4;
    brand = SPWPN_NORMAL;

    if (you.weapon())
    {
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
            }
        }
    }
    else
    {
        // Unarmed combat.
        if (you.species == SP_TROLL)
            damage += you.experience_level;

        damage += you.skills[SK_UNARMED_COMBAT];
    }

    damage *= 30 + you.skills[SK_FIGHTING];
    damage /= 30;

    damage += you.strength / 4;

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

    add_spells();
}

static spell_type search_first_list(int ignore_spell)
{
    for (unsigned i = 0;
         i < sizeof(search_order_conj) / sizeof(*search_order_conj); i++)
     {
        if (search_order_conj[i] == SPELL_NO_SPELL)
            return (SPELL_NO_SPELL);

        if (search_order_conj[i] == ignore_spell)
            continue;

        if (player_has_spell(search_order_conj[i]))
            return (search_order_conj[i]);
    }

    return (SPELL_NO_SPELL);
}

static spell_type search_second_list(int ignore_spell)
{
    for (unsigned i = 0;
         i < sizeof(search_order_third) / sizeof(*search_order_third); i++)
    {
        if (search_order_third[i] == SPELL_NO_SPELL)
            return (SPELL_NO_SPELL);

        if (search_order_third[i] == ignore_spell)
            continue;

        if (player_has_spell(search_order_third[i]))
            return (search_order_third[i]);
    }

    return (SPELL_NO_SPELL);
}

static spell_type search_third_list(int ignore_spell)
{
    for (unsigned i = 0;
         i < sizeof(search_order_misc) / sizeof(*search_order_misc); i++)
    {
        if (search_order_misc[i] == SPELL_NO_SPELL)
            return (SPELL_NO_SPELL);

        if (search_order_misc[i] == ignore_spell)
            continue;

        if (player_has_spell(search_order_misc[i]))
            return (search_order_misc[i]);
    }

    return (SPELL_NO_SPELL);
}

// Used when creating ghosts: goes through and finds spells for the
// ghost to cast.  Death is a traumatic experience, so ghosts only
// remember a few spells.
void ghost_demon::add_spells()
{
    int i = 0;

    spells.init(SPELL_NO_SPELL);

    spells[ 0 ] = search_first_list(SPELL_NO_SPELL);
    spells[ 1 ] = search_first_list(spells[0]);
    spells[ 2 ] = search_second_list(SPELL_NO_SPELL);
    spells[ 3 ] = search_third_list(SPELL_DIG);

    if (spells[ 3 ] == SPELL_NO_SPELL)
        spells[ 3 ] = search_first_list(SPELL_NO_SPELL);

    spells[ 4 ] = search_third_list(spells[3]);

    if (spells[ 4 ] == SPELL_NO_SPELL)
        spells[ 4 ] = search_first_list(spells[3]);

    if (player_has_spell( SPELL_DIG ))
        spells[ 4 ] = SPELL_DIG;

    // Look for Blink or Teleport Self for the emergency slot.
    if (player_has_spell( SPELL_CONTROLLED_BLINK )
        || player_has_spell( SPELL_BLINK ))
    {
        spells[ 5 ] = SPELL_CONTROLLED_BLINK;
    }

    if (player_has_spell( SPELL_TELEPORT_SELF ))
        spells[ 5 ] = SPELL_TELEPORT_SELF;

    for (i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
        spells[i] = translate_spell( spells[i] );
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

    // Pick up any other ghosts that happen to be on the level if we have space.
    // If the player is undead, add one to the ghost quota for the level.
    find_extra_ghosts( gs, n_extra_ghosts() + 1 - gs.size() );

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
#if DEBUG_BONES | DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Saving ghost: %s", g.name.c_str());
#endif
}

void ghost_demon::find_extra_ghosts( std::vector<ghost_demon> &gs, int n )
{
    for (int i = 0; n > 0 && i < MAX_MONSTERS; ++i)
    {
        if (!menv[i].alive())
            continue;

        if (menv[i].type == MONS_PLAYER_GHOST && menv[i].ghost.get())
        {
            // Bingo!
            announce_ghost(*menv[i].ghost);
            gs.push_back( *menv[i].ghost );
            --n;
        }
    }

    // Check the transit list for the current level.
    find_transiting_ghosts(gs, n);
}

// Returns the number of extra ghosts allowed on the level.
int ghost_demon::n_extra_ghosts()
{
    const int lev = you.your_level + 1;
    const int subdepth = subdungeon_depth(you.where_are_you, you.your_level);

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
        // Values greater than the allowed maximum or less then the allowed
        // minimum signalize bugginess.
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
        if (ghost.species < SP_HUMAN || ghost.species >= NUM_SPECIES)
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

        // Only Pandemonium lords cycle colours.
        if (ghost.cycle_colours)
            return (false);

        // Name validation.
        if (!validate_player_name(ghost.name.c_str(), false))
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
