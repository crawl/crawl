/*
 * File:    ghost.cc
 * Summary: Player ghost and random Pandemonium demon handling.
 *
 * Created for Dungeon Crawl Reference by $Author:dshaligram $ on
 * $Date: 2007-03-15 $.
 */

#include "AppHdr.h"

#include "externs.h"
#include "itemname.h"
#include "itemprop.h"
#include "randart.h"
#include "skills2.h"
#include "stuff.h"
#include "misc.h"
#include "mtransit.h"
#include "player.h"
#include <vector>

std::vector<ghost_demon> ghosts;

/*
   Order for looking for conjurations for the 1st & 2nd spell slots,
   when finding spells to be remembered by a player's ghost:
 */
static int search_order_conj[] = {
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
    SPELL_BACKLIGHT,
    SPELL_NO_SPELL,                        // end search
};

/*
   Order for looking for summonings and self-enchants for the 3rd spell slot:
 */
static int search_order_third[] = {
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
static int search_order_misc[] = {
/* 0 */
    SPELL_AGONY,
    SPELL_BANISHMENT,
    SPELL_PARALYSE,
    SPELL_CONFUSE,
    SPELL_SLOW,
    SPELL_POLYMORPH_OTHER,
    SPELL_TELEPORT_OTHER,
    SPELL_DIG,
    SPELL_BACKLIGHT,
    SPELL_NO_SPELL,                        // end search
};

/* Last slot (emergency) can only be teleport self or blink. */

ghost_demon::ghost_demon() : name(), values()
{
    reset();
}

void ghost_demon::reset()
{
    name.clear();
    values.init(0);
    values[GVAL_SPEED] = 10;
}

void ghost_demon::init_random_demon()
{
    char st_p[ITEMNAME_SIZE];

    make_name(random_int(), false, st_p);
    name = st_p;

    // hp - could be defined below (as could ev, AC etc). Oh well, too late:
    values[ GVAL_MAX_HP ] = 100 + roll_dice( 3, 50 );

    values[ GVAL_EV ] = 5 + random2(20);
    values[ GVAL_AC ] = 5 + random2(20);

    values[ GVAL_SEE_INVIS ] = (one_chance_in(10) ? 0 : 1);

    if (!one_chance_in(3))
        values[ GVAL_RES_FIRE ] = (coinflip() ? 2 : 3);
    else
    {
        values[ GVAL_RES_FIRE ] = 0;    /* res_fire */

        if (one_chance_in(10))
            values[ GVAL_RES_FIRE ] = -1;
    }

    if (!one_chance_in(3))
        values[ GVAL_RES_COLD ] = 2;
    else
    {
        values[ GVAL_RES_COLD ] = 0;    /* res_cold */

        if (one_chance_in(10))
            values[ GVAL_RES_COLD ] = -1;
    }

    // demons, like ghosts, automatically get poison res. and life prot.

    // resist electricity:
    values[ GVAL_RES_ELEC ] = (!one_chance_in(3) ? 1 : 0);

    // HTH damage:
    values[ GVAL_DAMAGE ] = 20 + roll_dice( 2, 20 );

    // special attack type (uses weapon brand code):
    values[ GVAL_BRAND ] = SPWPN_NORMAL;

    if (!one_chance_in(3))
    {
        do {
            values[ GVAL_BRAND ] = random2(17);
            /* some brands inappropriate (eg holy wrath) */
        } while (values[ GVAL_BRAND ] == SPWPN_HOLY_WRATH 
                 || values[ GVAL_BRAND ] == SPWPN_ORC_SLAYING
                 || values[ GVAL_BRAND ] == SPWPN_PROTECTION 
                 || values[ GVAL_BRAND ] == SPWPN_FLAME 
                 || values[ GVAL_BRAND ] == SPWPN_FROST 
                 || values[ GVAL_BRAND ] == SPWPN_DISRUPTION);
    }

    // is demon a spellcaster?
    // upped from one_chance_in(3)... spellcasters are more interesting
    // and I expect named demons to typically have a trick or two -- bwr
    values[GVAL_DEMONLORD_SPELLCASTER] = (one_chance_in(10) ? 0 : 1);

    // does demon fly? (0 = no, 1 = fly, 2 = levitate)
    values[GVAL_DEMONLORD_FLY] = (one_chance_in(3) ? 0 : 
                                        one_chance_in(5) ? 2 : 1);

    // vacant <ghost best skill level>:
    values[GVAL_DEMONLORD_UNUSED] = 0;

    // hit dice:
    values[GVAL_DEMONLORD_HIT_DICE] = 10 + roll_dice(2, 10);

    // does demon cycle colours?
    values[GVAL_DEMONLORD_CYCLE_COLOUR] = (one_chance_in(10) ? 1 : 0);

    for (int i = GVAL_SPELL_1; i <= GVAL_SPELL_6; i++)
        values[i] = SPELL_NO_SPELL;

    /* This bit uses the list of player spells to find appropriate spells
       for the demon, then converts those spells to the monster spell indices.
       Some special monster-only spells are at the end. */
    if (values[ GVAL_DEMONLORD_SPELLCASTER ] == 1)
    {
#define RANDOM_ARRAY_ELEMENT(x) x[random2(sizeof(x) / sizeof(x[0]))]
        
        if (coinflip())
            values[GVAL_SPELL_1]=RANDOM_ARRAY_ELEMENT(search_order_conj);

        // Might duplicate the first spell, but that isn't a problem.
        if (coinflip())
            values[GVAL_SPELL_2]=RANDOM_ARRAY_ELEMENT(search_order_conj);

        if (!one_chance_in(4))
            values[GVAL_SPELL_3]=RANDOM_ARRAY_ELEMENT(search_order_third);

        if (coinflip())
            values[GVAL_SPELL_4]=RANDOM_ARRAY_ELEMENT(search_order_misc);

        if (coinflip())
            values[GVAL_SPELL_5]=RANDOM_ARRAY_ELEMENT(search_order_misc);

#undef RANDOM_ARRAY_ELEMENT

        if (coinflip())
            values[ GVAL_SPELL_6 ] = SPELL_BLINK;
        if (coinflip())
            values[ GVAL_SPELL_6 ] = SPELL_TELEPORT_SELF;

        /* Converts the player spell indices to monster spell ones */
        for (int i = GVAL_SPELL_1; i <= GVAL_SPELL_6; i++)
            values[i] = translate_spell( values[i] );

        /* give demon a chance for some monster-only spells: */
        /* and demon-summoning should be fairly common: */
        if (one_chance_in(25))
            values[GVAL_SPELL_1] = SPELL_HELLFIRE_BURST;
        if (one_chance_in(25))
            values[GVAL_SPELL_1] = SPELL_METAL_SPLINTERS;
        if (one_chance_in(25))
            values[GVAL_SPELL_1] = SPELL_ENERGY_BOLT;  /* eye of devas */

        if (one_chance_in(25))
            values[GVAL_SPELL_2] = SPELL_STEAM_BALL;
        if (one_chance_in(25))
            values[GVAL_SPELL_2] = SPELL_ISKENDERUNS_MYSTIC_BLAST;
        if (one_chance_in(25))
            values[GVAL_SPELL_2] = SPELL_HELLFIRE;

        if (one_chance_in(25))
            values[GVAL_SPELL_3] = SPELL_SMITING;
        if (one_chance_in(25))
            values[GVAL_SPELL_3] = SPELL_HELLFIRE_BURST;
        if (one_chance_in(12))
            values[GVAL_SPELL_3] = SPELL_SUMMON_GREATER_DEMON;
        if (one_chance_in(12))
            values[GVAL_SPELL_3] = SPELL_SUMMON_DEMON;

        if (one_chance_in(20))
            values[GVAL_SPELL_4] = SPELL_SUMMON_GREATER_DEMON;
        if (one_chance_in(20))
            values[GVAL_SPELL_4] = SPELL_SUMMON_DEMON;

        /* at least they can summon demons */
        if (values[GVAL_SPELL_4] == SPELL_NO_SPELL)
            values[GVAL_SPELL_4] = SPELL_SUMMON_DEMON;

        if (one_chance_in(15))
            values[GVAL_SPELL_5] = SPELL_DIG;
    }
}

void ghost_demon::init_player_ghost()
{
    name = you.your_name;
    values[ GVAL_MAX_HP ]    = ((you.hp_max >= 400) ? 400 : you.hp_max);
    values[ GVAL_EV ]        = player_evasion();
    values[ GVAL_AC ]        = player_AC();

    if (values[GVAL_EV] > 40)
        values[GVAL_EV] = 40;
    
    values[ GVAL_SEE_INVIS ] = player_see_invis();
    values[ GVAL_RES_FIRE ]  = player_res_fire();
    values[ GVAL_RES_COLD ]  = player_res_cold();
    values[ GVAL_RES_ELEC ]  = player_res_electricity();
    values[ GVAL_SPEED ]     = player_ghost_base_movement_speed();

    int d = 4;
    int e = 0;
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

    values[ GVAL_DAMAGE ] = d;
    values[ GVAL_BRAND ]  = e;
    values[ GVAL_SPECIES ] = you.species;
    values[ GVAL_BEST_SKILL ] = best_skill(SK_FIGHTING, (NUM_SKILLS - 1), 99);
    values[ GVAL_SKILL_LEVEL ] =
        you.skills[best_skill(SK_FIGHTING, (NUM_SKILLS - 1), 99)];
    values[ GVAL_EXP_LEVEL ] = you.experience_level;
    values[ GVAL_CLASS ] = you.char_class;

    add_spells();
}

static int search_first_list(int ignore_spell)
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

static int search_second_list(int ignore_spell)
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

static int search_third_list(int ignore_spell)
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

    for (i = GVAL_SPELL_1; i <= GVAL_SPELL_6; i++)
        values[i] = SPELL_NO_SPELL;

    values[ GVAL_SPELL_1 ] = search_first_list(SPELL_NO_SPELL);
    values[ GVAL_SPELL_2 ] = search_first_list(values[GVAL_SPELL_1]);
    values[ GVAL_SPELL_3 ] = search_second_list(SPELL_NO_SPELL);
    values[ GVAL_SPELL_4 ] = search_third_list(SPELL_NO_SPELL);

    if (values[ GVAL_SPELL_4 ] == SPELL_NO_SPELL)
        values[ GVAL_SPELL_4 ] = search_first_list(SPELL_NO_SPELL);

    values[ GVAL_SPELL_5 ] = search_first_list(values[GVAL_SPELL_4]);

    if (values[ GVAL_SPELL_5 ] == SPELL_NO_SPELL)
        values[ GVAL_SPELL_5 ] = search_first_list(values[GVAL_SPELL_4]);

    if (player_has_spell( SPELL_DIG ))
        values[ GVAL_SPELL_5 ] = SPELL_DIG;

    /* Looks for blink/tport for emergency slot */
    if (player_has_spell( SPELL_CONTROLLED_BLINK ) 
        || player_has_spell( SPELL_BLINK ))
    {
        values[ GVAL_SPELL_6 ] = SPELL_CONTROLLED_BLINK;
    }

    if (player_has_spell( SPELL_TELEPORT_SELF ))
        values[ GVAL_SPELL_6 ] = SPELL_TELEPORT_SELF;

    for (i = GVAL_SPELL_1; i <= GVAL_SPELL_6; i++)
        values[i] = translate_spell( values[i] );
}                               // end add_spells()

/*
   When passed the number for a player spell, returns the equivalent monster
   spell. Returns SPELL_NO_SPELL on failure (no equiv).
 */
int ghost_demon::translate_spell(int spel) const
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
                    gs.push_back(*m.ghost);
                    --n;
                }
            }
        }
    }
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
