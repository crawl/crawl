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

#include "ghost.h"

#include "externs.h"
#include "itemname.h"
#include "itemprop.h"
#include "randart.h"
#include "skills2.h"
#include "stuff.h"
#include "mtransit.h"
#include "place.h"
#include "player.h"
#include <vector>

std::vector<ghost_demon> ghosts;

/*
   Order for looking for conjurations for the 1st & 2nd spell slots,
   when finding spells to be remembered by a player's ghost:
 */
static spell_type search_order_conj[] = {
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_BOLT_OF_DRAINING,
    SPELL_AGONY,
    SPELL_DISINTEGRATE,
    SPELL_LIGHTNING_BOLT,
    SPELL_STICKY_FLAME,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_BOLT_OF_MAGMA,
    SPELL_ICE_BOLT,
    SPELL_BOLT_OF_FIRE,
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
    SPELL_SHOCK,
    SPELL_MAGIC_DART,
    SPELL_SLEEP,
    SPELL_BACKLIGHT,
    SPELL_NO_SPELL,                        // end search
};

/*
   Order for looking for summonings and self-enchants for the 3rd spell slot:
 */
static spell_type search_order_third[] = {
/* 0 */
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_SUMMON_GREATER_DEMON,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_SUMMON_WRAITHS,
    SPELL_SUMMON_DEMON,
    SPELL_DEMONIC_HORDE,
    SPELL_HASTE,
    SPELL_ANIMATE_DEAD,
    SPELL_INVISIBILITY,
    SPELL_CALL_IMP,
    SPELL_SUMMON_SMALL_MAMMAL,
/* 10 */
    SPELL_CONTROLLED_BLINK,
    SPELL_BLINK,
    SPELL_NO_SPELL,                        // end search
};

/*
   Order for looking for enchants for the 4th + 5th spell slot. If fails, will
   go through conjs.
   Note: Dig must be in misc2 (5th) position to work.
 */
static spell_type search_order_misc[] = {
/* 0 */
    SPELL_AGONY,
    SPELL_BANISHMENT,
    SPELL_FREEZING_CLOUD,    
    SPELL_PARALYSE,
    SPELL_CONFUSE,
    SPELL_MEPHITIC_CLOUD,
    SPELL_SLOW,
    SPELL_POLYMORPH_OTHER,
    SPELL_TELEPORT_OTHER,
    SPELL_DIG,
    SPELL_BACKLIGHT,
    SPELL_NO_SPELL,                        // end search
};

/* Last slot (emergency) can only be teleport self or blink. */

ghost_demon::ghost_demon()
{
    reset();
}

void ghost_demon::reset()
{
    name.clear();
    species = SP_UNKNOWN;
    job = JOB_UNKNOWN;
    best_skill = SK_FIGHTING;
    best_skill_level = 0;
    xl = 0;
    max_hp = 0;
    ev = 0;
    ac = 0;
    damage = 0;
    speed = 10;
    see_invis = false;
    brand = SPWPN_NORMAL;
    resists = mon_resist_def();
    spellcaster = false;
    cycle_colours = false;
    fly = FL_NONE;
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
        resists.fire = 0;    /* res_fire */

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

    // demons, like ghosts, automatically get poison res. and life prot.

    // resist electricity:
    resists.elec = one_chance_in(3);

    // HTH damage:
    damage = 20 + roll_dice( 2, 20 );

    // special attack type (uses weapon brand code):
    brand = SPWPN_NORMAL;

    if (!one_chance_in(3))
    {
        do {
            brand = static_cast<brand_type>( random2(MAX_PAN_LORD_BRANDS) );
            /* some brands inappropriate (e.g. holy wrath) */
        } while (brand == SPWPN_HOLY_WRATH 
                 || (brand == SPWPN_ORC_SLAYING
                     && you.mons_species() != MONS_ORC)
                 || (brand == SPWPN_DRAGON_SLAYING
                     && you.mons_species() != MONS_DRACONIAN)
                 || brand == SPWPN_PROTECTION 
                 || brand == SPWPN_FLAME 
                 || brand == SPWPN_FROST);
    }

    // is demon a spellcaster?
    // upped from one_chance_in(3)... spellcasters are more interesting
    // and I expect named demons to typically have a trick or two -- bwr
    spellcaster = !one_chance_in(10);

    // does demon fly?
    fly = (one_chance_in(3)? FL_NONE :
           one_chance_in(5)? FL_LEVITATE : FL_FLY);
    
    // hit dice:
    xl = 10 + roll_dice(2, 10);

    // does demon cycle colours?
    cycle_colours = one_chance_in(10);

    spells.init(SPELL_NO_SPELL);

    /* This bit uses the list of player spells to find appropriate spells
       for the demon, then converts those spells to the monster spell indices.
       Some special monster-only spells are at the end. */
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
            if ( spells[3] == SPELL_DIG )
                spells[3] = SPELL_NO_SPELL;
        } 

        if (coinflip())
            spells[4] = RANDOM_ELEMENT(search_order_misc);

        if (coinflip())
            spells[5] = SPELL_BLINK;
        if (coinflip())
            spells[5] = SPELL_TELEPORT_SELF;

        /* Converts the player spell indices to monster spell ones */
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
            spells[i] = translate_spell( spells[i] );

        /* give demon a chance for some monster-only spells: */
        /* and demon-summoning should be fairly common: */
        if (one_chance_in(25))
            spells[0] = SPELL_HELLFIRE_BURST;
        if (one_chance_in(25))
            spells[0] = SPELL_METAL_SPLINTERS;
        if (one_chance_in(25))
            spells[0] = SPELL_ENERGY_BOLT;  /* eye of devas */

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

        /* at least they can summon demons */
        if (spells[3] == SPELL_NO_SPELL)
            spells[3] = SPELL_SUMMON_DEMON;

        if (one_chance_in(15))
            spells[4] = SPELL_DIG;
    }
}

void ghost_demon::init_player_ghost()
{
    name = you.your_name;
    max_hp = ((you.hp_max >= 400) ? 400 : you.hp_max);
    ev     = player_evasion();
    ac     = player_AC();

    if (ev > 60)
        ev = 60;

    see_invis = player_see_invis();
    resists.fire = player_res_fire();
    resists.cold = player_res_cold();
    resists.elec = player_res_electricity();
    speed = player_ghost_base_movement_speed();

    int d = 4;
    int e = SPWPN_NORMAL;
    const int wpn = you.equip[EQ_WEAPON];

    if (wpn != -1)
    {
        if (you.inv[wpn].base_type == OBJ_WEAPONS
            || you.inv[wpn].base_type == OBJ_STAVES)
        {
            d = property( you.inv[wpn], PWPN_DAMAGE );

            d *= 25 + you.skills[weapon_skill( you.inv[wpn].base_type,
                                               you.inv[wpn].sub_type )];
            d /= 25;

            if (you.inv[wpn].base_type == OBJ_WEAPONS)
            {
                if (is_random_artefact( you.inv[wpn] ))
                    e = randart_wpn_property( you.inv[wpn], RAP_BRAND );
                else
                    e = you.inv[wpn].special;
            }
        }
    }
    else
    {
        /* Unarmed combat */
        if (you.species == SP_TROLL)
            d += you.experience_level;

        d += you.skills[SK_UNARMED_COMBAT];
    }

    d *= 30 + you.skills[SK_FIGHTING];
    d /= 30;

    d += you.strength / 4;

    if (d > 50)
        d = 50;

    damage = d;
    brand  = static_cast<brand_type>( e );
    species = you.species;
    best_skill = ::best_skill(SK_FIGHTING, (NUM_SKILLS - 1), 99);
    best_skill_level = you.skills[best_skill];
    xl = you.experience_level;
    job = you.char_class;

    add_spells();
}

static spell_type search_first_list(int ignore_spell)
{
    for (unsigned i = 0;
         i < sizeof(search_order_conj) / sizeof(*search_order_conj); i++)
     {
        if (search_order_conj[i] == SPELL_NO_SPELL)
            return SPELL_NO_SPELL;

        if (search_order_conj[i] == ignore_spell)
            continue;

        if (player_has_spell(search_order_conj[i]))
            return search_order_conj[i];
    }

    return SPELL_NO_SPELL;
}                               // end search_first_list()

static spell_type search_second_list(int ignore_spell)
{
    for (unsigned i = 0;
         i < sizeof(search_order_third) / sizeof(*search_order_third); i++)
    {
        if (search_order_third[i] == SPELL_NO_SPELL)
            return SPELL_NO_SPELL;

        if (search_order_third[i] == ignore_spell)
            continue;

        if (player_has_spell(search_order_third[i]))
            return search_order_third[i];
    }

    return SPELL_NO_SPELL;
}                               // end search_second_list()

static spell_type search_third_list(int ignore_spell)
{
    for (unsigned i = 0;
         i < sizeof(search_order_misc) / sizeof(*search_order_misc); i++)
    {
        if (search_order_misc[i] == SPELL_NO_SPELL)
            return SPELL_NO_SPELL;

        if (search_order_misc[i] == ignore_spell)
            continue;

        if (player_has_spell(search_order_misc[i]))
            return search_order_misc[i];
    }

    return SPELL_NO_SPELL;
}                               // end search_third_list()

/*
   Used when creating ghosts: goes through and finds spells for the ghost to
   cast. Death is a traumatic experience, so ghosts only remember a few spells.
 */
void ghost_demon::add_spells( )
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

    /* Looks for blink/tport for emergency slot */
    if (player_has_spell( SPELL_CONTROLLED_BLINK ) 
        || player_has_spell( SPELL_BLINK ))
    {
        spells[ 5 ] = SPELL_CONTROLLED_BLINK;
    }

    if (player_has_spell( SPELL_TELEPORT_SELF ))
        spells[ 5 ] = SPELL_TELEPORT_SELF;

    for (i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
        spells[i] = translate_spell( spells[i] );
}                               // end add_spells()

/*
   When passed the number for a player spell, returns the equivalent monster
   spell. Returns SPELL_NO_SPELL on failure (no equiv).
 */
spell_type ghost_demon::translate_spell(spell_type spel) const
{
    switch (spel)
    {
    case SPELL_CONTROLLED_BLINK:
        return (SPELL_BLINK);        /* approximate */
    case SPELL_DEMONIC_HORDE:
        return (SPELL_CALL_IMP);
    case SPELL_AGONY:
    case SPELL_SYMBOL_OF_TORMENT:
        /* Too powerful to give ghosts Torment for Agony? Nah. */
        return (SPELL_SYMBOL_OF_TORMENT);
    case SPELL_DELAYED_FIREBALL:
        return (SPELL_FIREBALL);
    default:
        break;
    }

    return (spel);
}

std::vector<ghost_demon> ghost_demon::find_ghosts()
{
    std::vector<ghost_demon> gs;

    ghost_demon player;
    player.init_player_ghost();
    announce_ghost(player);
    gs.push_back(player);

    find_extra_ghosts( gs, n_extra_ghosts() );

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
#ifdef DEBUG_DIAGNOSTICS
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
    
    // No multiple ghosts until level 9 of the Main Dungeon.
    if ((lev < 9 && you.where_are_you == BRANCH_MAIN_DUNGEON)
        || (subdepth < 2 && you.where_are_you == BRANCH_LAIR)
        || (subdepth < 2 && you.where_are_you == BRANCH_ORCISH_MINES))
        return (0);

    if (you.where_are_you == BRANCH_LAIR
        || you.where_are_you == BRANCH_ORCISH_MINES
        || (you.where_are_you == BRANCH_MAIN_DUNGEON && lev < 15))
        return (1);

    return 1 + (random2(20) < lev) + (random2(40) < lev);
}
