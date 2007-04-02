/*
 *  File:       mon-util.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <2>    11/04/99        cdl     added a break to spell selection
 *                                  for kobold Summoners
 *                                  print just "[Ii]t" for invisible undead
 *                                  renamed params to monam()
 *   <1>     -/--/--        LRH     Created
 */

// $pellbinder: (c) D.G.S.E 1998
// some routines snatched from former monsstat.cc

#include "AppHdr.h"
#include "mon-util.h"
#include "monstuff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "externs.h"

#include "beam.h"
#include "debug.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "Kills.h"
#include "misc.h"
#include "monplace.h"
#include "mstuff2.h"
#include "mtransit.h"
#include "player.h"
#include "randart.h"
#include "stuff.h"
#include "view.h"

//jmf: moved from inside function
static FixedVector < int, NUM_MONSTERS > mon_entry;

// really important extern -- screen redraws suck w/o it {dlb}
FixedVector < unsigned short, 1000 > mcolour;

enum habitat_type
{
    // Flying monsters will appear in all categories
    HT_NORMAL,          // Normal critters
    HT_SHALLOW_WATER,   // Union of normal + water
    HT_DEEP_WATER,      // Water critters
    HT_LAVA,            // Lava critters

    NUM_HABITATS
};

static bool initialized_randmons = false;
static std::vector<int> monsters_by_habitat[NUM_HABITATS];

static struct monsterentry mondata[] = {
#include "mon-data.h"
};

#define MONDATASIZE (sizeof(mondata)/sizeof(struct monsterentry))

static int mspell_list[][7] = {
#include "mon-spll.h"
};

#if DEBUG_DIAGNOSTICS
static const char *monster_spell_name[] = {
    "Magic Missile",
    "Throw Flame",
    "Throw Frost",
    "Paralysis",
    "Slow",
    "Haste",
    "Confuse",
    "Venom Bolt",
    "Fire Bolt",
    "Cold Bolt",
    "Lightning Bolt", 
    "Invisibility",
    "Fireball",
    "Heal",
    "Teleport",
    "Teleport Other",
    "Blink",
    "Crystal Spear",
    "Dig",
    "Negative Bolt",
    "Hellfire Burst",
    "Vampire Summon",
    "Orb Energy",
    "Brain Feed",
    "Level Summon",
    "Fake Rakshasa Summon",
    "Steam Ball",
    "Summon Demon",
    "Animate Dead",
    "Pain",
    "Smite",
    "Sticky Flame",
    "Poison Blast",
    "Summon Demon Lesser",
    "Summon Ufetubus",
    "Purple Blast",
    "Summon Beast",
    "Energy Bolt",
    "Sting",
    "Iron Bolt",
    "Stone Arrow",
    "Poison Splash",
    "Summon Undead",
    "Mutation",
    "Cantrip",
    "Disintegrate", 
    "Marsh Gas",
    "Quicksilver Bolt",
    "Torment",
    "Hellfire",
    "Metal Splinters",
    "Summon Demon Greater",
    "Banishment",
    "Controlled Blink",
    "Control Undead",
    "Miasma",
    "Summon Drakes",
    "Blink Other",
    "Dispel Undead",
    "Hellfrost",
    "Poison Arrow",
    "Summon Small Mammals",
    "Summon Mushrooms",
    "Ice Bolt",
    "Magma",
    "Shock",
    "Berserk Rage",
    "Might"
};
#endif

static int mons_exp_mod(int mclass);
static monsterentry *seekmonster(int p_monsterid);

// macro that saves some typing, nothing more
#define smc seekmonster(mc)

/* ******************** BEGIN PUBLIC FUNCTIONS ******************** */

habitat_type grid2habitat(int grid)
{
    switch (grid)
    {
    case DNGN_DEEP_WATER:
        return (HT_DEEP_WATER);
    case DNGN_SHALLOW_WATER:
        return (HT_SHALLOW_WATER);
    case DNGN_LAVA:
        return (HT_LAVA);
    default:
        return (HT_NORMAL);
    }
}

int habitat2grid(habitat_type ht)
{
    switch (ht)
    {
    case HT_DEEP_WATER:
        return (DNGN_DEEP_WATER);
    case HT_SHALLOW_WATER:
        return (DNGN_SHALLOW_WATER);
    case HT_LAVA:
        return (DNGN_LAVA);
    case HT_NORMAL:
    default:
        return (DNGN_FLOOR);
    }
}

static void initialize_randmons()
{
    for (int i = 0; i < NUM_HABITATS; ++i)
    {
        int grid = habitat2grid( habitat_type(i) );

        for (int m = 0; m < NUM_MONSTERS; ++m)
        {
            if (invalid_monster_class(m))
                continue;
            if (monster_habitable_grid(m, grid))
                monsters_by_habitat[i].push_back(m);
        }
    }
    initialized_randmons = true;
}

monster_type random_monster_at_grid(int x, int y)
{
    return (random_monster_at_grid(grd[x][y]));
}

monster_type random_monster_at_grid(int grid)
{
    if (!initialized_randmons)
        initialize_randmons();

    const habitat_type ht = grid2habitat(grid);
    const std::vector<int> &valid_mons = monsters_by_habitat[ht];
    ASSERT(!valid_mons.empty());
    return valid_mons.empty()? MONS_PROGRAM_BUG
                 : monster_type(valid_mons[ random2(valid_mons.size()) ]);
}

monster_type get_monster_by_name(std::string name, bool exact)
{
    lowercase(name);

    monster_type mon = MONS_PROGRAM_BUG;
    for (unsigned i = 0; i < sizeof(mondata) / sizeof(*mondata); ++i)
    {
        std::string candidate = mondata[i].name;
        lowercase(candidate);

        const int mtype = mondata[i].mc;

        if (exact)
        {
            if (name == candidate)
                return monster_type(mtype);

            continue;
        }

        const std::string::size_type match = candidate.find(name);
        if (match == std::string::npos)
            continue;

        mon = monster_type(mtype);
        // we prefer prefixes over partial matches
        if (match == 0)
            break;
    }
    return (mon);
}

void init_monsters(FixedVector < unsigned short, 1000 > &colour)
{
    unsigned int x;             // must be unsigned to match size_t {dlb}

    for (x = 0; x < MONDATASIZE; x++)
        colour[mondata[x].mc] = mondata[x].colour;

    //unsigned int x = 0;    // must be unsigned to match size_t {dlb}

    // first, fill static array with dummy values {dlb};
    for (x = 0; x < NUM_MONSTERS; x++)
        mon_entry[x] = -1;

    // next, fill static array with location of entry in mondata[] {dlb}:
    for (x = 0; x < MONDATASIZE; x++)
        mon_entry[mondata[x].mc] = x;

    // finally, monsters yet with dummy entries point to TTTSNB(tm) {dlb}:
    for (x = 0; x < NUM_MONSTERS; x++)
    {
        if (mon_entry[x] == -1)
            mon_entry[x] = mon_entry[MONS_PROGRAM_BUG];
    }
    //return (monsterentry *) 0; // return value should not matter here {dlb}
}                               // end mons_init()

unsigned long get_mons_class_resists(int mc)
{
    return (smc->resists);
}

unsigned long get_mons_resists(const monsters *mon)
{
    unsigned long resists = get_mons_class_resists(mon->type);
    if ((mons_genus(mon->type) == MONS_DRACONIAN &&
         mon->type != MONS_DRACONIAN) ||
        mon->type == MONS_TIAMAT)
    {
        monster_type draco_species = draco_subspecies(mon);
        if (draco_species != mon->type)
            resists |= get_mons_class_resists(draco_species);
    }
    return (resists);
}

int mons_piety(const monsters *mon)
{
    if (mon->god == GOD_NO_GOD)
        return (0);

    // We're open to fine-tuning.
    return (mon->hit_dice * 14);
}

unsigned long mons_resist(const monsters *mon, unsigned long flags)
{
    return (get_mons_resists(mon) & flags);
}

bool mons_class_flag(int mc, int bf)
{
    const monsterentry *me = smc;

    if (!me)
        return (false);

    return ((me->bitfields & bf) != 0);
}                               // end mons_class_flag()

static int scan_mon_inv_randarts( const monsters *mon, int ra_prop )
{
    int ret = 0;

    if (mons_itemuse( mon->type ) >= MONUSE_STARTING_EQUIPMENT)
    {
        const int weapon = mon->inv[MSLOT_WEAPON];
        const int second = mon->inv[MSLOT_MISSILE]; // ettins/two-head oges
        const int armour = mon->inv[MSLOT_ARMOUR];

        if (weapon != NON_ITEM && mitm[weapon].base_type == OBJ_WEAPONS
            && is_random_artefact( mitm[weapon] ))
        {
            ret += randart_wpn_property( mitm[weapon], ra_prop );
        }

        if (second != NON_ITEM && mitm[second].base_type == OBJ_WEAPONS
            && is_random_artefact( mitm[second] ))
        {
            ret += randart_wpn_property( mitm[second], ra_prop );
        }

        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR
            && is_random_artefact( mitm[armour] ))
        {
            ret += randart_wpn_property( mitm[armour], ra_prop );
        }
    }

    return (ret);
}

mon_holy_type mons_holiness(const monsters *mon)
{
    return (mons_class_holiness(mon->type));
}

mon_holy_type mons_class_holiness(int mc)
{
    return (smc->holiness);
}                               // end mons_holiness()

bool mons_class_is_stationary(int type)
{
    return (type == MONS_OKLOB_PLANT
                || type == MONS_PLANT
                || type == MONS_FUNGUS
                || type == MONS_CURSE_SKULL
                || mons_is_statue(type)
                || mons_is_mimic(type));
}

bool mons_is_stationary(const monsters *mons)
{
    return (mons_class_is_stationary(mons->type));
}

bool invalid_monster(const monsters *mons)
{
    return (!mons || mons->type == -1);
}

bool invalid_monster_class(int mclass)
{
    return (mclass < 0 
            || mclass >= NUM_MONSTERS 
            || mon_entry[mclass] == -1
            || mon_entry[mclass] == MONS_PROGRAM_BUG);
}

bool mons_is_statue(int mc)
{
    return (mc == MONS_ORANGE_STATUE || mc == MONS_SILVER_STATUE);
}

bool mons_is_mimic( int mc )
{
    return (mons_species( mc ) == MONS_GOLD_MIMIC);
}

bool mons_is_demon( int mc )
{
    const int show_char = mons_char( mc );

    // Not every demonic monster is a demon (ie hell hog, hell hound)
    if (mons_class_holiness( mc ) == MH_DEMONIC
        && (isdigit( show_char ) || show_char == '&'))
    {
        return (true);
    }

    return (false);
}

// Used for elven Glamour ability. -- bwr
bool mons_is_humanoid( int mc )
{
    switch (mons_char( mc))
    {
    case 'o':   // orcs
    case 'e':   // elvens (deep)
    case 'c':   // centaurs
    case 'C':   // giants
    case 'O':   // ogres
    case 'K':   // kobolds
    case 'N':   // nagas
    case '@':   // adventuring humans
    case 'T':   // trolls
        return (true);

    case 'g':   // goblines, hobgoblins, gnolls, boggarts -- but not gargoyles
        if (mc != MONS_GARGOYLE
            && mc != MONS_METAL_GARGOYLE
            && mc != MONS_MOLTEN_GARGOYLE)
        {
            return (true);
        }

    default:
        break;
    }

    return (false);
}

int mons_zombie_size(int mc)
{
    return (smc->zombie_size);
}                               // end mons_zombie_size()


int mons_weight(int mc)
{
    return (smc->weight);
}                               // end mons_weight()

corpse_effect_type mons_corpse_effect(int mc)
{
    return (smc->corpse_thingy);
}                               // end mons_corpse_effect()


monster_type mons_species( int mc )
{
    const monsterentry *me = seekmonster(mc);
    return (me? me->species : MONS_PROGRAM_BUG);
}                               // end mons_species()

monster_type mons_genus( int mc )
{
    return (smc->genus);
}

monster_type draco_subspecies( const monsters *mon )
{
    ASSERT( mons_genus( mon->type ) == MONS_DRACONIAN );

    if ( mon->type == MONS_TIAMAT )
    {
        switch ( mon->colour )
        {
        case RED:
            return MONS_RED_DRACONIAN;
        case WHITE:
            return MONS_WHITE_DRACONIAN;
        case DARKGREY:          // black
            return MONS_BLACK_DRACONIAN;
        case GREEN:
            return MONS_GREEN_DRACONIAN;
        case MAGENTA:
            return MONS_PURPLE_DRACONIAN;
        default:
            break;
        }
    }

    monster_type ret = mons_species( mon->type );

    if (ret == MONS_DRACONIAN && mon->type != MONS_DRACONIAN)
        ret = static_cast<monster_type>( mon->number );

    return (ret);
}

int mons_shouts(int mc)
{
    int u = smc->shouts;

    if (u == -1)
        u = random2(12);

    return (u);
}                               // end mons_shouts()

bool mons_is_unique( int mc )
{
    return (mons_class_flag(mc, M_UNIQUE));
}

char mons_see_invis( struct monsters *mon )
{
    if (mon->type == MONS_PLAYER_GHOST || mon->type == MONS_PANDEMONIUM_DEMON)
        return (mon->ghost->values[ GVAL_SEE_INVIS ]);
    else if (((seekmonster(mon->type))->bitfields & M_SEE_INVIS) != 0)
        return (1);
    else if (scan_mon_inv_randarts( mon, RAP_EYESIGHT ) > 0)
        return (1);

    return (0);
}                               // end mons_see_invis()


// This does NOT do line of sight!  It checks the targ's visibility 
// with respect to mon's perception, but doesn't do walls or range.
bool mons_monster_visible( struct monsters *mon, struct monsters *targ )
{
    if (targ->has_ench(ENCH_SUBMERGED)
        || (targ->has_ench(ENCH_INVIS) && !mons_see_invis(mon)))
    {
        return (false);
    }

    return (true);
}

// This does NOT do line of sight!  It checks the player's visibility 
// with respect to mon's perception, but doesn't do walls or range.
bool mons_player_visible( struct monsters *mon )
{
    if (you.invis)
    {
        if (player_in_water())
            return (true);

        if (mons_see_invis( mon ))
            return (true);

        return (false);
    }

    return (true);
}

unsigned char mons_char(int mc)
{
    return (unsigned char) smc->showchar;
}                               // end mons_char()


char mons_itemuse(int mc)
{
    return (smc->gmon_use);
}                               // end mons_itemuse()

int mons_class_colour(int mc)
{
    const monsterentry *m = smc;
    if (!m)
        return (BLACK);

    const int class_colour = m->colour;
    return (class_colour);
}                               // end mons_colour()

int mons_colour(const monsters *monster)
{
    return (monster->colour);
}

int mons_zombie_base(const monsters *monster)
{
    return (monster->number);
}

bool mons_is_zombified(const monsters *monster)
{
    switch (monster->type)
    {
    case MONS_ZOMBIE_SMALL: case MONS_ZOMBIE_LARGE:
    case MONS_SKELETON_SMALL: case MONS_SKELETON_LARGE:
    case MONS_SIMULACRUM_SMALL: case MONS_SIMULACRUM_LARGE:
    case MONS_SPECTRAL_THING:
        return (true);
    default:
        return (false);
    }
}

int downscale_zombie_damage(int damage)
{
    // these are cumulative, of course: {dlb}
    if (damage > 1)
        damage--;
    if (damage > 4)
        damage--;
    if (damage > 11)
        damage--;
    if (damage > 14)
        damage--;

    return (damage);
}

mon_attack_def downscale_zombie_attack(const monsters *mons,
                                       mon_attack_def attk)
{
    switch (attk.type)
    {
    case AT_STING: case AT_SPORE:
    case AT_TOUCH: case AT_ENGULF:
        attk.type = AT_HIT;
        break;
    default:
        break;
    }

    if (mons->type == MONS_SIMULACRUM_LARGE
        || mons->type == MONS_SIMULACRUM_SMALL)
    {
        attk.flavour = AF_COLD;
    }
    else
    {
        attk.flavour =  AF_PLAIN;
    }
    
    attk.damage = downscale_zombie_damage(attk.damage);

    return (attk);
}

mon_attack_def mons_attack_spec(const monsters *mons, int attk_number)
{
    int mc = mons->type;
    const bool zombified = mons_is_zombified(mons);
    
    if ((attk_number < 0 || attk_number > 3) || mc == MONS_HYDRA)
        attk_number = 0;

    if (mc == MONS_PLAYER_GHOST || mc == MONS_PANDEMONIUM_DEMON)
    {
        if (attk_number == 0)
            return mon_attack_def::attk(mons->ghost->values[GVAL_DAMAGE]);
        else
            return mon_attack_def::attk(0, AT_NONE);
    }

    if (zombified)
        mc = mons_zombie_base(mons);

    mon_attack_def attk = smc->attack[attk_number];
    if (attk.flavour == AF_KLOWN)
    {
        switch (random2(6))
        {
        case 0: attk.flavour = AF_POISON_NASTY; break;
        case 1: attk.flavour = AF_ROT; break;
        case 2: attk.flavour = AF_DRAIN_XP; break;
        case 3: attk.flavour = AF_FIRE; break;
        case 4: attk.flavour = AF_COLD; break;
        case 5: attk.flavour = AF_BLINK; break;
        }
    }
    
    return (zombified? downscale_zombie_attack(mons, attk) : attk);
}

int mons_damage(int mc, int rt)
{
    if (rt < 0 || rt > 3)
        rt = 0;
    return smc->attack[rt].damage;
}                               // end mons_damage()

int mons_resist_magic( const monsters *mon )
{
    int u = (seekmonster(mon->type))->resist_magic;

    // negative values get multiplied with mhd
    if (u < 0)
        u = mon->hit_dice * -u;

    u += scan_mon_inv_randarts( mon, RAP_MAGIC );

    // ego armour resistance
    const int armour = mon->inv[MSLOT_ARMOUR];

    if (armour != NON_ITEM 
        && get_armour_ego_type( mitm[armour] ) == SPARM_MAGIC_RESISTANCE )
    {
        u += 30;
    }

    return (u);
}                               // end mon_resist_magic()


// Returns true if the monster made its save against hostile
// enchantments/some other magics.
bool check_mons_resist_magic( const monsters *monster, int pow )
{
    int mrs = mons_resist_magic(monster);

    if (mrs == MAG_IMMUNE)
        return (true);

    // Evil, evil hack to make weak one hd monsters easier for first
    // level characters who have resistable 1st level spells. Six is
    // a very special value because mrs = hd * 2 * 3 for most monsters,
    // and the weak, low level monsters have been adjusted so that the
    // "3" is typically a 1.  There are some notable one hd monsters
    // that shouldn't fall under this, so we do < 6, instead of <= 6...
    // or checking monster->hit_dice.  The goal here is to make the
    // first level easier for these classes and give them a better
    // shot at getting to level two or three and spells that can help
    // them out (or building a level or two of their base skill so they
    // aren't resisted as often). -- bwr
    if (mrs < 6 && coinflip())
        return (false);

    pow = stepdown_value( pow, 30, 40, 100, 120 );

    const int mrchance = (100 + mrs) - pow;
    const int mrch2 = random2(100) + random2(101);

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, 
              "Power: %d, monster's MR: %d, target: %d, roll: %d", 
              pow, mrs, mrchance, mrch2 );

    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    return (mrch2 < mrchance);
}                               // end check_mons_resist_magic()

int mons_res_elec( const monsters *mon )
{
    int mc = mon->type;

    if (mc == MONS_PLAYER_GHOST || mc == MONS_PANDEMONIUM_DEMON)
        return (mon->ghost->values[ GVAL_RES_ELEC ]);

    /* this is a variable, not a player_xx() function, so can be above 1 */
    int u = 0;

    if (mons_resist(mon, MR_RES_ELEC))
        u++;

    // don't bother checking equipment if the monster can't use it
    if (mons_itemuse(mc) >= MONUSE_STARTING_EQUIPMENT)
    {
        u += scan_mon_inv_randarts( mon, RAP_ELECTRICITY );

        // no ego armour, but storm dragon.
        const int armour = mon->inv[MSLOT_ARMOUR];
        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR
            && mitm[armour].sub_type == ARM_STORM_DRAGON_ARMOUR)
        {
            u += 1;
        }
    }

    return (u);
}                               // end mons_res_elec()

bool mons_res_asphyx( const monsters *mon )
{
    const int holiness = mons_holiness( mon );
    return (holiness == MH_UNDEAD 
                || holiness == MH_DEMONIC
                || holiness == MH_NONLIVING
                || holiness == MH_PLANT
                || mons_resist(mon, MR_RES_ASPHYX));
}

int mons_res_poison( const monsters *mon )
{
    int mc = mon->type;

    int u = 0, f = get_mons_resists(mon);

    if (f & MR_RES_POISON)
        u++;

    if (f & MR_VUL_POISON)
        u--;

    if (mons_itemuse(mc) >= MONUSE_STARTING_EQUIPMENT)
    {
        u += scan_mon_inv_randarts( mon, RAP_POISON );

        const int armour = mon->inv[MSLOT_ARMOUR];
        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR)
        {
            // intrinsic armour abilities
            switch (mitm[armour].sub_type)
            {
            case ARM_SWAMP_DRAGON_ARMOUR: u += 1; break;
            case ARM_GOLD_DRAGON_ARMOUR:  u += 1; break;
            default:                              break;
            }

            // ego armour resistance
            if (get_armour_ego_type( mitm[armour] ) == SPARM_POISON_RESISTANCE)
                u += 1;
        }
    }

    return (u);
}                               // end mons_res_poison()

int mons_res_fire( const monsters *mon )
{
    int mc = mon->type;

    if (mc == MONS_PLAYER_GHOST || mc == MONS_PANDEMONIUM_DEMON)
        return (mon->ghost->values[ GVAL_RES_FIRE ]);

    int u = 0, f = get_mons_resists(mon);

    // no Big Prize (tm) here either if you set all three flags. It's a pity uh?
    //
    // Note that natural monster resistance is two levels, this is duplicate
    // the fact that having this flag used to be a lot better than armour
    // for monsters (it used to make them immune in a lot of cases) -- bwr
    if (f & MR_RES_HELLFIRE)
        u += 3;
    else if (f & MR_RES_FIRE)
        u += 2;
    else if (f & MR_VUL_FIRE)
        u--;

    if (mons_itemuse(mc) >= MONUSE_STARTING_EQUIPMENT)
    {
        u += scan_mon_inv_randarts( mon, RAP_FIRE );

        const int armour = mon->inv[MSLOT_ARMOUR];
        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR)
        {
            // intrinsic armour abilities
            switch (mitm[armour].sub_type)
            {
            case ARM_DRAGON_ARMOUR:      u += 2; break;
            case ARM_GOLD_DRAGON_ARMOUR: u += 1; break;
            case ARM_ICE_DRAGON_ARMOUR:  u -= 1; break;
            default:                             break;
            }

            // check ego resistance
            const int ego = get_armour_ego_type( mitm[armour] );
            if (ego == SPARM_FIRE_RESISTANCE || ego == SPARM_RESISTANCE)
                u += 1;
        }
    }

    return (u);
}                               // end mons_res_fire()


int mons_res_cold( const monsters *mon )
{
    int mc = mon->type;

    if (mc == MONS_PLAYER_GHOST || mc == MONS_PANDEMONIUM_DEMON)
        return (mon->ghost->values[ GVAL_RES_COLD ]);

    int u = 0, f = get_mons_resists(mon);

    // Note that natural monster resistance is two levels, this is duplicate
    // the fact that having this flag used to be a lot better than armour
    // for monsters (it used to make them immune in a lot of cases) -- bwr
    if (f & MR_RES_COLD)
        u += 2;
    else if (f & MR_VUL_COLD)
        u--;

    if (mons_itemuse(mc) >= MONUSE_STARTING_EQUIPMENT)
    {
        u += scan_mon_inv_randarts( mon, RAP_COLD );

        const int armour = mon->inv[MSLOT_ARMOUR];
        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR)
        {
            // intrinsic armour abilities
            switch (mitm[armour].sub_type)
            {
            case ARM_ICE_DRAGON_ARMOUR:  u += 2; break;
            case ARM_GOLD_DRAGON_ARMOUR: u += 1; break;
            case ARM_DRAGON_ARMOUR:      u -= 1; break;
            default:                             break;
            }

            // check ego resistance
            const int ego = get_armour_ego_type( mitm[armour] );
            if (ego == SPARM_COLD_RESISTANCE || ego == SPARM_RESISTANCE)
                u += 1;
        }
    }

    return (u);
}                               // end mons_res_cold()

int mons_res_negative_energy( const monsters *mon )
{
    int mc = mon->type;

    if (mons_holiness(mon) == MH_UNDEAD 
        || mons_holiness(mon) == MH_DEMONIC
        || mons_holiness(mon) == MH_NONLIVING
        || mons_holiness(mon) == MH_PLANT
        || mon->type == MONS_SHADOW_DRAGON
        || mon->type == MONS_DEATH_DRAKE)
    {
        return (3);  // to match the value for players
    }

    int u = 0;

    if (mons_itemuse(mc) >= MONUSE_STARTING_EQUIPMENT)
    {
        u += scan_mon_inv_randarts( mon, RAP_NEGATIVE_ENERGY );

        const int armour = mon->inv[MSLOT_ARMOUR];
        if (armour != NON_ITEM && mitm[armour].base_type == OBJ_ARMOUR)
        {
            // check for ego resistance
            if (get_armour_ego_type( mitm[armour] ) == SPARM_POSITIVE_ENERGY)
                u += 1;
        }
    }

    return (u);
}                               // end mons_res_negative_energy()

bool mons_is_evil( const monsters *mon )
{
    return (mons_class_flag( mon->type, M_EVIL ));
}

bool mons_is_unholy( const monsters *mon )
{
    const mon_holy_type holy = mons_holiness( mon );

    return (holy == MH_UNDEAD || holy == MH_DEMONIC);
}

bool mons_has_lifeforce( const monsters *mon )
{
    const int holy = mons_holiness( mon );

    return (holy == MH_NATURAL || holy == MH_PLANT);
             // && !mon->has_ench(ENCH_PETRIFY));
}

int mons_skeleton(int mc)
{
    if (mons_zombie_size(mc) == 0
        || mons_weight(mc) == 0 || ((smc->bitfields & M_NO_SKELETON) != 0))
    {
        return (0);
    }

    return (1);
}                               // end mons_skeleton()

int mons_class_flies(int mc)
{
    int f = smc->bitfields;

    if (f & M_FLIES)
        return (1);

    if (f & M_LEVITATE)
        return (2);

    return (0);
}

int mons_flies( const monsters *mon )
{
    if (mon->type == MONS_PANDEMONIUM_DEMON
        && mon->ghost->values[ GVAL_DEMONLORD_FLY ])
    {
        return (1);
    }
    
    int ret = mons_class_flies( mon->type );
    return (ret ? ret : (scan_mon_inv_randarts(mon, RAP_LEVITATE) > 0) ? 2 : 0);
}                               // end mons_flies()


// this nice routine we keep in exactly the way it was
int hit_points(int hit_dice, int min_hp, int rand_hp)
{
    int hrolled = 0;

    for (int hroll = 0; hroll < hit_dice; hroll++)
    {
        hrolled += random2(1 + rand_hp);
        hrolled += min_hp;
    }

    return (hrolled);
}                               // end hit_points()

// This function returns the standard number of hit dice for a type 
// of monster, not a pacticular monsters current hit dice. -- bwr
int mons_type_hit_dice( int type )
{
    struct monsterentry *mon_class = seekmonster( type );

    if (mon_class)
        return (mon_class->hpdice[0]);

    return (0);
}


int exper_value( const struct monsters *monster )
{
    long x_val = 0;

    // these three are the original arguments:
    const int  mclass      = monster->type;
    const int  mHD         = monster->hit_dice;
    const int  maxhp       = monster->max_hit_points;

    // these are some values we care about:
    const int  speed       = mons_speed(mclass);
    const int  modifier    = mons_exp_mod(mclass);
    const int  item_usage  = mons_itemuse(mclass);

    // XXX: shapeshifters can qualify here, even though they can't cast:
    const bool spellcaster = mons_class_flag( mclass, M_SPELLCASTER );

    // early out for no XP monsters
    if (mons_class_flag(mclass, M_NO_EXP_GAIN))
        return (0);

    // no experience for destroying furniture, even if the furniture started
    // the fight.
    if (mons_is_statue(mclass))
        return (0);

    // These undead take damage to maxhp, so we use only HD. -- bwr
    if (mclass == MONS_ZOMBIE_SMALL
        || mclass == MONS_ZOMBIE_LARGE
        || mclass == MONS_SIMULACRUM_SMALL
        || mclass == MONS_SIMULACRUM_LARGE
        || mclass == MONS_SKELETON_SMALL
        || mclass == MONS_SKELETON_LARGE)
    {
        x_val = (16 + mHD * 4) * (mHD * mHD) / 10;
    }
    else
    {
        x_val = (16 + maxhp) * (mHD * mHD) / 10;
    }


    // Let's calculate a simple difficulty modifier -- bwr
    int diff = 0;

    // Let's look for big spells:
    if (spellcaster)
    {
        const monster_spells &hspell_pass = monster->spells;

        for (int i = 0; i < 6; i++)
        {
            switch (hspell_pass[i])
            {
            case MS_PARALYSIS:
            case MS_SMITE:
            case MS_HELLFIRE_BURST:
            case MS_HELLFIRE:
            case MS_TORMENT:
                diff += 25;
                break;

            case MS_LIGHTNING_BOLT:
            case MS_NEGATIVE_BOLT:
            case MS_VENOM_BOLT:
            case MS_STICKY_FLAME:
            case MS_DISINTEGRATE:
            case MS_SUMMON_DEMON_GREATER:
            case MS_BANISHMENT:
            case MS_CRYSTAL_SPEAR:
            case MS_IRON_BOLT:
            case MS_TELEPORT:
            case MS_TELEPORT_OTHER:
                diff += 10;
                break;

            default:
                break;
            }
        }
    }

    // let's look at regeneration
    if (monster_descriptor( mclass, MDSC_REGENERATES ))
        diff += 15;

    // Monsters at normal or fast speed with big melee damage
    if (speed >= 10)
    {
        int max_melee = 0;
        for (int i = 0; i < 4; i++)
            max_melee += mons_damage( mclass, i );

        if (max_melee > 30)
            diff += (max_melee / ((speed == 10) ? 2 : 1));
    }

    // Monsters who can use equipment (even if only the equipment
    // they are given) can be considerably enhanced because of 
    // the way weapons work for monsters. -- bwr
    if (item_usage == MONUSE_STARTING_EQUIPMENT 
        || item_usage == MONUSE_WEAPONS_ARMOUR)
    {
        diff += 30;
    }

    // Set a reasonable range on the difficulty modifier... 
    // Currently 70% - 200% -- bwr
    if (diff > 100)
        diff = 100;
    else if (diff < -30)
        diff = -30;

    // Apply difficulty
    x_val *= (100 + diff);
    x_val /= 100;

    // Basic speed modification
    if (speed > 0)
    {
        x_val *= speed;
        x_val /= 10;
    }

    // Slow monsters without spells and items often have big HD which
    // cause the experience value to be overly large... this tries
    // to reduce the inappropriate amount of XP that results. -- bwr
    if (speed < 10 && !spellcaster && item_usage < MONUSE_STARTING_EQUIPMENT)
    {
        x_val /= 2;
    }

    // Apply the modifier in the monster's definition
    if (modifier > 0)
    {
        x_val *= modifier;
        x_val /= 10;
    }

    // Reductions for big values. -- bwr
    if (x_val > 100)
        x_val = 100 + ((x_val - 100) * 3) / 4;
    if (x_val > 1000)
        x_val = 1000 + (x_val - 1000) / 2;

    // guarantee the value is within limits
    if (x_val <= 0)
        x_val = 1;
    else if (x_val > 15000)
        x_val = 15000;

    return (x_val);
}                               // end exper_value()

void mons_load_spells( monsters *mon, int book )
{
    mon->load_spells(book);
}

#if DEBUG_DIAGNOSTICS
const char *mons_spell_name( int spell )
{
    if (spell == MS_NO_SPELL || spell >= NUM_MONSTER_SPELLS || spell < 0)
        return ("No spell");

    return (monster_spell_name[ spell ]);
}
#endif

// generate a shiny new and unscarred monster
void define_monster(int index)
{
    monsters &mons = menv[index];

    int temp_rand = 0;          // probability determination {dlb}
    int mcls = mons.type;
    int hd, hp, hp_max, ac, ev, speed;
    int monnumber = mons.number;
    const monsterentry *m = seekmonster(mcls);
    int col = mons_class_colour(mons.type);
    int spells = MST_NO_SPELLS;

    hd = m->hpdice[0];

    // misc
    ac = m->AC;
    ev = m->ev;

    speed = m->speed;

    mons.god = GOD_NO_GOD;

    switch (mcls)
    {
    case MONS_ABOMINATION_SMALL:
        hd = 4 + random2(4);
        ac = 3 + random2(7);
        ev = 7 + random2(6);
        speed = 7 + random2avg(9, 2);

        if (monnumber == 250)
            col = random_colour();
        break;

    case MONS_ZOMBIE_SMALL:
        hd = (coinflip() ? 1 : 2);
        break;

    case MONS_ZOMBIE_LARGE:
        hd = 3 + random2(5);
        break;

    case MONS_ABOMINATION_LARGE:
        hd = 8 + random2(4);
        ac = 5 + random2avg(9, 2);
        ev = 3 + random2(5);
        speed = 6 + random2avg(7, 2);

        if (monnumber == 250)
            col = random_colour();
        break;

    case MONS_BEAST:
        hd = 4 + random2(4);
        ac = 2 + random2(5);
        ev = 7 + random2(5);
        speed = 8 + random2(5);
        break;

    case MONS_HYDRA:
        monnumber = random_range(4, 8);
        break;

    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_SOLDIER:
    case MONS_ORC_WIZARD:
        spells = MST_ORC_WIZARD_I + random2(3);
        break;

    case MONS_LICH:
    case MONS_ANCIENT_LICH:
        spells = MST_LICH_I + random2(4);
        break;

    case MONS_HELL_KNIGHT:
        spells = (coinflip() ? MST_HELL_KNIGHT_I : MST_HELL_KNIGHT_II);
        break;

    case MONS_NECROMANCER:
        spells = (coinflip() ? MST_NECROMANCER_I : MST_NECROMANCER_II);
        break;

    case MONS_WIZARD:
    case MONS_OGRE_MAGE:
    case MONS_EROLCHA:
    case MONS_DEEP_ELF_MAGE:
        spells = MST_WIZARD_I + random2(5);
        break;

    case MONS_DEEP_ELF_CONJURER:
        spells = 
            (coinflip()? MST_DEEP_ELF_CONJURER_I : MST_DEEP_ELF_CONJURER_II);
        break;

    case MONS_BUTTERFLY:
    case MONS_SPATIAL_VORTEX:
    case MONS_KILLER_KLOWN:
        col = random_colour();
        break;

    case MONS_GILA_MONSTER:
        temp_rand = random2(7);

        col = (temp_rand >= 5 ? LIGHTRED :                   // 2/7
               temp_rand >= 3 ? LIGHTMAGENTA :               // 2/7
               temp_rand >= 1 ? MAGENTA                      // 1/7
                              : YELLOW);                     // 1/7
        break;

    case MONS_DRACONIAN:
        // these are supposed to only be created by polymorph
        hd += random2(10);
        ac += random2(5);
        ev += random2(5);
        break;

    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_SCORCHER:
    {
        // Professional draconians still have a base draconian type.
        // White draconians will never be draconian scorchers, but
        // apart from that, anything goes.
        do
            monnumber = MONS_BLACK_DRACONIAN + random2(8);
        while (monnumber == MONS_WHITE_DRACONIAN
                && mcls == MONS_DRACONIAN_SCORCHER);
        break;
    }
    case MONS_DRACONIAN_KNIGHT:
    {
        temp_rand = random2(10);
        // hell knight, death knight, chaos knight...
        if (temp_rand < 6)
            spells = (coinflip() ? MST_HELL_KNIGHT_I : MST_HELL_KNIGHT_II);
        else if (temp_rand < 9)
            spells = (coinflip() ? MST_NECROMANCER_I : MST_NECROMANCER_II);
        else 
            spells = (coinflip() ? MST_DEEP_ELF_CONJURER_I 
                                 : MST_DEEP_ELF_CONJURER_II);
        
        monnumber = MONS_BLACK_DRACONIAN + random2(8);
        break;
    }
    case MONS_HUMAN:
    case MONS_ELF:
        // these are supposed to only be created by polymorph 
        hd += random2(10);
        ac += random2(5);
        ev += random2(5);
        break;

    default:
        if (mons_is_mimic( mcls ))
            col = get_mimic_colour( &mons );
        break;
    }

    if (spells == MST_NO_SPELLS && mons_class_flag(mons.type, M_SPELLCASTER))
        spells = m->sec;

    // some calculations
    hp = hit_points(hd, m->hpdice[1], m->hpdice[2]);
    hp += m->hpdice[3];
    hp_max = hp;

    if (monnumber == 250)
        monnumber = m->sec;

    // so let it be written, so let it be done
    mons.hit_dice = hd;
    mons.hit_points = hp;
    mons.max_hit_points = hp_max;
    mons.ac = ac;
    mons.ev = ev;
    mons.speed = speed;
    mons.speed_increment = 70;
    mons.number = monnumber;
    mons.flags = 0L;
    mons.colour = col;
    mons_load_spells( &mons, spells );

    // reset monster enchantments
    mons.enchantments.clear();
}                               // end define_monster()

std::string str_monam(const monsters *mon, description_level_type desc,
                      bool force_seen)
{
    return (ptr_monam(mon, desc, force_seen));
}

/* ------------------------- monam/moname ------------------------- */
const char *ptr_monam( const monsters *mon, char desc, bool force_seen )
{
    // We give an item type description for mimics now, note that 
    // since gold mimics only have one description (to match the
    // examine code in direct.cc), we won't bother going through
    // this for them. -- bwr
    if (mons_is_mimic( mon->type ) && mon->type != MONS_GOLD_MIMIC)
    {
        static char mimic_name_buff[ ITEMNAME_SIZE ];

        item_def item;
        get_mimic_item( mon, item );
        item_name( item, desc, mimic_name_buff );

        return (mimic_name_buff);
    }

    return (monam( mon, mon->number, mon->type,
                   force_seen || player_monster_visible( mon ), 
                   desc, mon->inv[MSLOT_WEAPON] ));
}

const char *monam( const monsters *mon,
                   int mons_num, int mons, bool vis, char desc, int mons_wpn )
{
    static char gmo_n[ ITEMNAME_SIZE ];
    char gmo_n2[ ITEMNAME_SIZE ] = "";

    gmo_n[0] = 0;

    // If you can't see the critter, let moname() print [Ii]t.
    if (!vis)
    {
        moname( mons, vis, desc, gmo_n );
        return (gmo_n);
    }

    // These need their description level handled here instead of
    // in monam().
    if (mons == MONS_SPECTRAL_THING || mons_genus(mons) == MONS_DRACONIAN)
    {
        switch (desc)
        {
        case DESC_CAP_THE:
            strcpy(gmo_n, "The");
            break;
        case DESC_NOCAP_THE:
            strcpy(gmo_n, "the");
            break;
        case DESC_CAP_A:
            strcpy(gmo_n, "A");
            break;
        case DESC_NOCAP_A:
            strcpy(gmo_n, "a");
            break;
        case DESC_PLAIN:         /* do nothing */ ;
            break;
            //default: DEBUGSTR("bad desc flag");
        }
    }

    switch (mons)
    {
    case MONS_ZOMBIE_SMALL: 
    case MONS_ZOMBIE_LARGE:
        moname(mons_num, vis, desc, gmo_n);
        strcat(gmo_n, " zombie");
        break;

    case MONS_SKELETON_SMALL: 
    case MONS_SKELETON_LARGE:
        moname(mons_num, vis, desc, gmo_n);
        strcat(gmo_n, " skeleton");
        break;

    case MONS_SIMULACRUM_SMALL: 
    case MONS_SIMULACRUM_LARGE:
        moname(mons_num, vis, desc, gmo_n);
        strcat(gmo_n, " simulacrum");
        break;

    case MONS_SPECTRAL_THING: 
        strcat(gmo_n, " spectral ");
        moname(mons_num, vis, DESC_PLAIN, gmo_n2);
        strcat(gmo_n, gmo_n2);
        break;

    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_DRACONIAN_SCORCHER:
        if (desc != DESC_PLAIN)
            strcat( gmo_n, " " );

        switch (mons_num)
        {
        default: break;
        case MONS_BLACK_DRACONIAN:   strcat(gmo_n, "black ");   break;
        case MONS_MOTTLED_DRACONIAN: strcat(gmo_n, "mottled "); break;
        case MONS_YELLOW_DRACONIAN:  strcat(gmo_n, "yellow ");  break;
        case MONS_GREEN_DRACONIAN:   strcat(gmo_n, "green ");   break;
        case MONS_PURPLE_DRACONIAN:  strcat(gmo_n, "purple ");  break;
        case MONS_RED_DRACONIAN:     strcat(gmo_n, "red ");     break;
        case MONS_WHITE_DRACONIAN:   strcat(gmo_n, "white ");   break;
        case MONS_PALE_DRACONIAN:    strcat(gmo_n, "pale ");    break;
        }

        moname( mons, vis, DESC_PLAIN, gmo_n2 );
        strcat( gmo_n, gmo_n2 );
        break;

    case MONS_DANCING_WEAPON:
        // safety check -- if we don't have/know the weapon use default name
        if (mons_wpn == NON_ITEM)
            moname( mons, vis, desc, gmo_n );
        else 
        {
            item_def item = mitm[mons_wpn];
            unset_ident_flags( item, ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES );
            item_name( item, desc, gmo_n );
        }
        break;

    case MONS_PLAYER_GHOST:
        strcpy(gmo_n, mon->ghost->name.c_str());
        strcat(gmo_n, "'s ghost");
        break;

    case MONS_PANDEMONIUM_DEMON:
        strcpy(gmo_n, mon->ghost->name.c_str());
        break;

    default:
        moname(mons, vis, desc, gmo_n);
        break;
    }

    return (gmo_n);
}                               // end monam()

const char *moname(int mons_num, bool vis, char descrip, char glog[ ITEMNAME_SIZE ])
{
    glog[0] = 0;

    char gmon_name[ ITEMNAME_SIZE ] = "";
    strcpy( gmon_name, seekmonster( mons_num )->name );

    if (!vis)
    {
        switch (descrip)
        {
        case DESC_CAP_THE:
        case DESC_CAP_A:
            strcpy(glog, "It");
            break;
        case DESC_NOCAP_THE:
        case DESC_NOCAP_A:
        case DESC_PLAIN:
            strcpy(glog, "it");
            break;
        case DESC_NOCAP_YOUR:
            strcpy(glog, "its");
            break;
        case DESC_CAP_YOUR:
            strcpy(glog, "Its");
            break;
        }

        strcpy(gmon_name, glog);
        return (glog);
    }

    if (!mons_is_unique( mons_num )) 
    {
        switch (descrip)
        {
        case DESC_CAP_THE:
        case DESC_CAP_YOUR:
            strcpy(glog, "The ");
            break;
        case DESC_NOCAP_THE:
        case DESC_NOCAP_YOUR:
            strcpy(glog, "the ");
            break;
        case DESC_CAP_A:
            strcpy(glog, "A");
            break;
        case DESC_NOCAP_A:
            strcpy(glog, "a");
            break;
        case DESC_PLAIN:
            break;
        // default: DEBUGSTR("bad monster descrip flag");
        }

        if (descrip == DESC_CAP_A || descrip == DESC_NOCAP_A)
        {
            switch (toupper(gmon_name[0]))
            {
            case 'A':
            case 'E':
            case 'I':
            case 'O':
            case 'U':
                strcat(glog, "n ");
                break;

            default:
                strcat(glog, " ");
                break;
            }
        }
    }

    strcat(glog, gmon_name);

    if ((descrip == DESC_CAP_YOUR || descrip == DESC_NOCAP_YOUR) && *glog)
    {
        const int lastch = glog[ strlen(glog) - 1 ];
        if (lastch == 's' || lastch == 'x')
            strcat(glog, "'");
        else
            strcat(glog, "'s");
    }

    return (glog);
}                               // end moname()

/* ********************* END PUBLIC FUNCTIONS ********************* */

// see mons_init for initialization of mon_entry array.
static monsterentry *seekmonster(int p_monsterid)
{
    int me = p_monsterid != -1? mon_entry[p_monsterid] : -1;

    if (me >= 0)                // PARANOIA
        return (&mondata[me]);
    else
        return (NULL);
}                               // end seekmonster()

static int mons_exp_mod(int mc)
{
    return (smc->exp_mod);
}                               // end mons_exp_mod()


int mons_speed(int mc)
{
    return (smc->speed);
}                               // end mons_speed()


int mons_intel(int mc)          //jmf: "fixed" to work with new I_ types
{
    switch (smc->intel)
    {
    case I_PLANT:
        return (I_PLANT);
    case I_INSECT:
    case I_REPTILE:
        return (I_INSECT);
    case I_ANIMAL:
    case I_ANIMAL_LIKE:
        return (I_ANIMAL);
    case I_NORMAL:
        return (I_NORMAL);
    case I_HIGH:
        return (I_HIGH);
    default:
        return (I_NORMAL);
    }
}                               // ens mons_intel()


int mons_intel_type(int mc)     //jmf: new, used by my spells
{
    return (smc->intel);
}                               // end mons_intel_type()

int mons_power(int mc)
{
    // for now, just return monster hit dice.
    return (smc->hpdice[0]);
}

bool mons_aligned(int m1, int m2)
{
    bool fr1, fr2;
    struct monsters *mon1, *mon2;

    if (m1 == MHITNOT || m2 == MHITNOT)
        return (true);

    if (m1 == MHITYOU)
        fr1 = true;
    else
    {
        mon1 = &menv[m1];
        fr1 = (mon1->attitude == ATT_FRIENDLY) || mon1->has_ench(ENCH_CHARM);
    }

    if (m2 == MHITYOU)
        fr2 = true;
    else
    {
        mon2 = &menv[m2];
        fr2 = (mon2->attitude == ATT_FRIENDLY) || mon2->has_ench(ENCH_CHARM);
    }

    return (fr1 == fr2);
}

bool mons_wields_two_weapons(const monsters *m)
{
    return (m->type == MONS_TWO_HEADED_OGRE || m->type == MONS_ETTIN);
}

bool mons_is_summoned(const monsters *m)
{
    return (m->has_ench(ENCH_ABJ));
}

// Does not check whether the monster can dual-wield - that is the
// caller's responsibility.
int mons_offhand_weapon_index(const monsters *m)
{
    return (m->inv[1]);
}

int mons_base_damage_brand(const monsters *m)
{
    if (m->type == MONS_PLAYER_GHOST || m->type == MONS_PANDEMONIUM_DEMON)
        return m->ghost->values[ GVAL_BRAND ];

    return (SPWPN_NORMAL);
}

int mons_size(const monsters *m)
{
    return m->body_size();
}

bool mons_friendly(const monsters *m)
{
    return (m->attitude == ATT_FRIENDLY || m->has_ench(ENCH_CHARM));
}

bool mons_is_submerged( const monsters *mon )
{
    // FIXME, switch to 4.1's MF_SUBMERGED system which is much cleaner.
    return (mon->has_ench(ENCH_SUBMERGED));
}

bool mons_is_paralysed(const monsters *m)
{
    return (m->has_ench(ENCH_PARALYSIS));
}

bool mons_is_confused(const monsters *m)
{
    return (m->has_ench(ENCH_CONFUSION) &&
                            !mons_class_flag(m->type, M_CONFUSED));
}

bool mons_is_fleeing(const monsters *m)
{
    return (m->behaviour == BEH_FLEE);
}

bool mons_is_sleeping(const monsters *m)
{
    return (m->behaviour == BEH_SLEEP);
}

bool mons_is_batty(const monsters *m)
{
    return testbits(m->flags, MF_BATTY);
}

bool mons_was_seen(const monsters *m)
{
    return testbits(m->flags, MF_SEEN);
}

bool mons_is_known_mimic(const monsters *m)
{
    return mons_is_mimic(m->type) && testbits(m->flags, MF_KNOWN_MIMIC);
}

bool mons_looks_stabbable(const monsters *m)
{
    // Make sure oklob plants are never highlighted. That'll defeat the
    // point of making them look like normal plants.
    return (!mons_class_flag(m->type, M_NO_EXP_GAIN)
                && m->type != MONS_OKLOB_PLANT
                && !mons_is_mimic(m->type)
                && !mons_is_statue(m->type)
                && !mons_friendly(m)
                && mons_is_sleeping(m));
}

bool mons_looks_distracted(const monsters *m)
{
    return (!mons_class_flag(m->type, M_NO_EXP_GAIN)
                && m->type != MONS_OKLOB_PLANT
                && !mons_is_mimic(m->type)
                && !mons_is_statue(m->type)
                && !mons_friendly(m)
                && ((m->foe != MHITYOU && !mons_is_batty(m))
                    || mons_is_confused(m)
                    || mons_is_fleeing(m)));
}

bool mons_should_fire(struct bolt &beam)
{
    // use of foeRatio:
    // the higher this number, the more monsters
    // will _avoid_ collateral damage to their friends.
    // setting this to zero will in fact have all
    // monsters ignore their friends when considering
    // collateral damage.

    // quick check - did we in fact get any foes?
    if (beam.foe_count == 0)
        return (false);

    // if we either hit no friends, or monster too dumb to care
    if (beam.fr_count == 0 || !beam.smart_monster)
        return (true);

    // only fire if they do acceptably low collateral damage
    // the default for this is 50%;  in other words, don't
    // hit a foe unless you hit 2 or fewer friends.
    if (beam.foe_power >= (beam.foe_ratio * beam.fr_power) / 100)
        return (true);

    return (false);
}

// used to determine whether or not a monster should always
// fire this spell if selected.  If not, we should use a
// tracer.

// note - this function assumes that the monster is "nearby"
// its target!

bool ms_requires_tracer(int monspell)
{
    bool requires = false;

    switch(monspell)
    {
        case MS_BANISHMENT:
        case MS_COLD_BOLT:
        case MS_ICE_BOLT:
        case MS_SHOCK:
        case MS_MAGMA:
        case MS_CONFUSE:
        case MS_CRYSTAL_SPEAR:
        case MS_DISINTEGRATE:
        case MS_ENERGY_BOLT:
        case MS_FIRE_BOLT:
        case MS_FIREBALL:
        case MS_FLAME:
        case MS_FROST:
        case MS_HELLFIRE:
        case MS_IRON_BOLT:
        case MS_LIGHTNING_BOLT:
        case MS_MARSH_GAS:
        case MS_MIASMA:
        case MS_METAL_SPLINTERS:
        case MS_MMISSILE:
        case MS_NEGATIVE_BOLT:
        case MS_ORB_ENERGY:
        case MS_PAIN:
        case MS_PARALYSIS:
        case MS_POISON_BLAST:
        case MS_POISON_ARROW:
        case MS_POISON_SPLASH:
        case MS_QUICKSILVER_BOLT:
        case MS_SLOW:
        case MS_STEAM_BALL:
        case MS_STICKY_FLAME:
        case MS_STING:
        case MS_STONE_ARROW:
        case MS_TELEPORT_OTHER:
        case MS_VENOM_BOLT:
            requires = true;
            break;

        // self-niceties and direct effects
        case MS_ANIMATE_DEAD:
        case MS_BLINK:
        case MS_BRAIN_FEED:
        case MS_DIG:
        case MS_FAKE_RAKSHASA_SUMMON:
        case MS_HASTE:
        case MS_HEAL:
        case MS_HELLFIRE_BURST:
        case MS_INVIS:
        case MS_LEVEL_SUMMON:
        case MS_MUTATION:
        case MS_SMITE:
        case MS_SUMMON_BEAST:
        case MS_SUMMON_DEMON_LESSER:
        case MS_SUMMON_DEMON:
        case MS_SUMMON_DEMON_GREATER:
        case MS_SUMMON_UFETUBUS:
        case MS_TELEPORT:
        case MS_TORMENT:
        case MS_SUMMON_SMALL_MAMMALS:
        case MS_VAMPIRE_SUMMON:
        case MS_CANTRIP:

        // meaningless, but sure, why not?
        case MS_NO_SPELL:
            break;

        default:
            break;

    }

    return (requires);
}

// returns true if the spell is something you wouldn't want done if
// you had a friendly target..  only returns a meaningful value for
// non-beam spells

bool ms_direct_nasty(int monspell)
{
    bool nasty = true;

    switch(monspell)
    {
        // self-niceties/summonings
        case MS_ANIMATE_DEAD:
        case MS_BLINK:
        case MS_DIG:
        case MS_FAKE_RAKSHASA_SUMMON:
        case MS_HASTE:
        case MS_HEAL:
        case MS_INVIS:
        case MS_LEVEL_SUMMON:
        case MS_SUMMON_BEAST:
        case MS_SUMMON_DEMON_LESSER:
        case MS_SUMMON_DEMON:
        case MS_SUMMON_DEMON_GREATER:
        case MS_SUMMON_UFETUBUS:
        case MS_TELEPORT:
        case MS_SUMMON_SMALL_MAMMALS:
        case MS_VAMPIRE_SUMMON:
            nasty = false;
            break;

        case MS_BRAIN_FEED:
        case MS_HELLFIRE_BURST:
        case MS_MUTATION:
        case MS_SMITE:
        case MS_TORMENT:

        // meaningless, but sure, why not?
        case MS_NO_SPELL:
            break;

        default:
            break;

    }

    return (nasty);
}

// Spells a monster may want to cast if fleeing from the player, and 
// the player is not in sight.
bool ms_useful_fleeing_out_of_sight( struct monsters *mon, int monspell )
{
    if (ms_waste_of_time( mon, monspell ))
        return (false);

    switch (monspell)
    {
    case MS_HASTE:
    case MS_INVIS:
    case MS_HEAL:
    case MS_ANIMATE_DEAD:
        return (true);

    case MS_SUMMON_SMALL_MAMMALS:
    case MS_VAMPIRE_SUMMON:
    case MS_SUMMON_UFETUBUS:
    case MS_FAKE_RAKSHASA_SUMMON:
    case MS_LEVEL_SUMMON:
    case MS_SUMMON_DEMON:
    case MS_SUMMON_DEMON_LESSER:
    case MS_SUMMON_BEAST:
    case MS_SUMMON_UNDEAD:
    case MS_SUMMON_MUSHROOMS:
    case MS_SUMMON_DEMON_GREATER:
        if (one_chance_in(10))    // only summon friends some of the time
            return (true);
        break;

    default:
        break;
    }

    return (false);
}

bool ms_low_hitpoint_cast( struct monsters *mon, int monspell )
{
    bool ret = false;

    bool targ_adj = false;

    if (mon->foe == MHITYOU || mon->foe == MHITNOT) 
    {
        if (adjacent(you.x_pos, you.y_pos, mon->x, mon->y))
            targ_adj = true;
    }
    else if (adjacent( menv[mon->foe].x, menv[mon->foe].y, mon->x, mon->y ))
    {
        targ_adj = true;
    }

    switch (monspell)
    {
    case MS_TELEPORT:
    case MS_TELEPORT_OTHER:
    case MS_HEAL:
        ret = true;
        break;

    case MS_BLINK:
        if (targ_adj)
            ret = true;
        break;

    case MS_SUMMON_SMALL_MAMMALS:
    case MS_VAMPIRE_SUMMON:
    case MS_SUMMON_UFETUBUS:
    case MS_FAKE_RAKSHASA_SUMMON:
        if (!targ_adj)
            ret = true;
        break;
    }

    return (ret);
}

// Checks to see if a particular spell is worth casting in the first place.
bool ms_waste_of_time( struct monsters *mon, int monspell )
{
    bool  ret = false;
    int intel, est_magic_resist, power, diff;
    struct monsters *targ;


    // Eventually, we'll probably want to be able to have monsters 
    // learn which of their elemental bolts were resisted and have those 
    // handled here as well. -- bwr
    switch (monspell)
    {
    case MS_HASTE:
        if (mon->has_ench(ENCH_HASTE))
            ret = true;
        break;

    case MS_INVIS:
        if (mon->has_ench(ENCH_INVIS) ||
            (mons_friendly(mon) && !player_see_invis(false)))
            ret = true;
        break;

    case MS_HEAL:
        if (mon->hit_points > mon->max_hit_points / 2)
            ret = true;
        break;

    case MS_TELEPORT:
        // Monsters aren't smart enough to know when to cancel teleport.
        if (mon->has_ench( ENCH_TP ))
            ret = true;
        break;

    case MS_TELEPORT_OTHER:
        // Monsters aren't smart enough to know when to cancel teleport.
        if (mon->foe == MHITYOU)
        {
            if (you.duration[DUR_TELEPORT])
                return (true);
        }
        else if (mon->foe != MHITNOT)
        {
            if (menv[mon->foe].has_ench(ENCH_TP))
                return (true);
        }
        // intentional fall-through

    case MS_SLOW:
    case MS_CONFUSE:
    case MS_PAIN:
    case MS_BANISHMENT:
    case MS_DISINTEGRATE:
    case MS_PARALYSIS:
        // occasionally we don't estimate... just fire and see:
        if (one_chance_in(5))
            return (false);

        // Only intelligent monsters estimate.
        intel = mons_intel( mon->type );
        if (intel != I_NORMAL && intel != I_HIGH)
            return (false);

        // We'll estimate the target's resistance to magic, by first getting
        // the actual value and then randomizing it.
        est_magic_resist = (mon->foe == MHITNOT) ? 10000 : 0;

        if (mon->foe != MHITNOT)
        {
            if (mon->foe == MHITYOU)
                est_magic_resist = player_res_magic();
            else
            {
                targ = &menv[ mon->foe ];
                est_magic_resist = mons_resist_magic( targ );
            }

            // now randomize (normal intels less accurate than high):
            if (intel == I_NORMAL)
                est_magic_resist += random2(80) - 40;  
            else
                est_magic_resist += random2(30) - 15;  
        }

        power = 12 * mon->hit_dice * (monspell == MS_PAIN ? 2 : 1);
        power = stepdown_value( power, 30, 40, 100, 120 );

        // Determine the amount of chance allowed by the benefit from
        // the spell.  The estimated difficulty is the probability
        // of rolling over 100 + diff on 2d100. -- bwr
        diff = (monspell == MS_PAIN 
                || monspell == MS_SLOW 
                || monspell == MS_CONFUSE) ? 0 : 50;

        if (est_magic_resist - power > diff)
            ret = true;

        break;

    default:
        break;
    }

    return (ret);    
}

static bool ms_ranged_spell( int monspell )
{
    switch (monspell)
    {
    case MS_HASTE:
    case MS_HEAL:
    case MS_TELEPORT:
    case MS_INVIS:
    case MS_BLINK:
        return (false);

    default:
        break;
    }

    return (true);
}

bool mons_is_magic_user( const monsters *mon )
{
    if (mons_class_flag(mon->type, M_ACTUAL_SPELLS))
    {
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
        {
            if (mon->spells[i] != MS_NO_SPELL)
                return (true);
        }
    }
    return (false);
}

bool mons_has_ranged_spell( struct monsters *mon )
{
    const int  mclass = mon->type;

    if (mons_class_flag( mclass, M_SPELLCASTER ))
    {
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
        {
            if (ms_ranged_spell( mon->spells[i] ))
                return (true);
        }
    }

    return (false);
}

bool mons_has_ranged_attack( struct monsters *mon )
{
    const int weapon = mon->inv[MSLOT_WEAPON];
    const int ammo = mon->inv[MSLOT_MISSILE];

    const int lnchClass = (weapon != NON_ITEM) ? mitm[weapon].base_type : -1;
    const int lnchType  = (weapon != NON_ITEM) ? mitm[weapon].sub_type  :  0;

    const int ammoClass = (ammo != NON_ITEM) ? mitm[ammo].base_type : -1;
    const int ammoType  = (ammo != NON_ITEM) ? mitm[ammo].sub_type  :  0;

    bool launched = false;
    bool thrown = false;

    throw_type( lnchClass, lnchType, ammoClass, ammoType, launched, thrown );

    if (launched || thrown)
        return (true);

    return (false);
}


// use of variant:
// 0 : She is tap dancing.
// 1 : It seems she is tap dancing. (lower case pronoun)
// 2 : Her sword explodes!          (upper case possessive)
// 3 : It sticks to her sword!      (lower case possessive)
// ... as needed

const char *mons_pronoun(int mon_type, int variant)
{
    int gender = GENDER_NEUTER;         

    if (mons_is_unique( mon_type ) && mon_type != MONS_PLAYER_GHOST)
    {
        switch(mon_type)
        {
            case MONS_JESSICA:
            case MONS_PSYCHE:
            case MONS_JOSEPHINE:
            case MONS_AGNES:
            case MONS_MAUD:
            case MONS_LOUISE:
            case MONS_FRANCES:
            case MONS_MARGERY:
            case MONS_EROLCHA:
            case MONS_ERICA:
            case MONS_TIAMAT:
                gender = GENDER_FEMALE;
                break;
            default:
                gender = GENDER_MALE;
                break;
        }
    }

    switch(variant)
    {
        case PRONOUN_CAP:
            return ((gender == 0) ? "It" :
                    (gender == 1) ? "He" : "She");

        case PRONOUN_NOCAP:
            return ((gender == 0) ? "it" :
                    (gender == 1) ? "he" : "she");

        case PRONOUN_CAP_POSSESSIVE:
            return ((gender == 0) ? "Its" :
                    (gender == 1) ? "His" : "Her");

        case PRONOUN_NOCAP_POSSESSIVE:
            return ((gender == 0) ? "its" :
                    (gender == 1) ? "his" : "her");

        case PRONOUN_REFLEXIVE:  // awkward at start of sentence, always lower
            return ((gender == 0) ? "itself"  :
                    (gender == 1) ? "himself" : "herself");
    }

    return ("");
}

/*
 * Checks if the monster can use smiting/torment to attack without unimpeded
 * LOS to the player.
 */
static bool mons_can_smite(const monsters *monster)
{
    if (monster->type == MONS_FIEND)
        return (true);

    const monster_spells &hspell_pass = monster->spells;
    for (unsigned i = 0; i < hspell_pass.size(); ++i)
        if (hspell_pass[i] == MS_TORMENT || hspell_pass[i] == MS_SMITE)
            return (true);

    return (false);
}

/*
 * Determines if a monster is smart and pushy enough to displace other monsters.
 * A shover should not cause damage to the shovee by displacing it, so monsters
 * that trail clouds of badness are ineligible. The shover should also benefit
 * from shoving, so monsters that can smite/torment are ineligible.
 *
 * (Smiters would be eligible for shoving when fleeing if the AI allowed for 
 * smart monsters to flee.)
 */
bool monster_shover(const monsters *m)
{
    const monsterentry *me = seekmonster(m->type);
    if (!me)
        return (false);

    // Efreet and fire elementals are disqualified because they leave behind
    // clouds of flame. Rotting devils trail clouds of miasma.
    if (m->type == MONS_EFREET || m->type == MONS_FIRE_ELEMENTAL
            || m->type == MONS_ROTTING_DEVIL
            || m->type == MONS_CURSE_TOE)
        return (false);

    // Smiters profit from staying back and smiting.
    if (mons_can_smite(m))
        return (false);
    
    int mchar = me->showchar;
    // Somewhat arbitrary: giants and dragons are too big to get past anything,
    // beetles are too dumb (arguable), dancing weapons can't communicate, eyes
    // aren't pushers and shovers, zombies are zombies. Worms and elementals
    // are on the list because all 'w' are currently unrelated.
    return (mchar != 'C' && mchar != 'B' && mchar != '(' && mchar != 'D'
            && mchar != 'G' && mchar != 'Z' && mchar != 'z'
            && mchar != 'w' && mchar != '#');
}

// Returns true if m1 and m2 are related, and m1 is higher up the totem pole
// than m2. The criteria for being related are somewhat loose, as you can see
// below.
bool monster_senior(const monsters *m1, const monsters *m2)
{
    const monsterentry *me1 = seekmonster(m1->type),
                       *me2 = seekmonster(m2->type);
    
    if (!me1 || !me2)
        return (false);

    int mchar1 = me1->showchar,
        mchar2 = me2->showchar;

    // If both are demons, the smaller number is the nastier demon.
    if (isdigit(mchar1) && isdigit(mchar2))
        return (mchar1 < mchar2);

    // &s are the evillest demons of all, well apart from Geryon, who really
    // profits from *not* pushing past beasts.
    if (mchar1 == '&' && isdigit(mchar2) && m1->type != MONS_GERYON)
        return (m1->hit_dice > m2->hit_dice);

    // Skeletal warriors can push past zombies large and small.
    if (m1->type == MONS_SKELETAL_WARRIOR && (mchar2 == 'z' || mchar2 == 'Z'))
        return (m1->hit_dice > m2->hit_dice);

    if (m1->type == MONS_QUEEN_BEE 
            && (m2->type == MONS_KILLER_BEE 
                    || m2->type == MONS_KILLER_BEE_LARVA))
        return (true);

    if (m1->type == MONS_KILLER_BEE && m2->type == MONS_KILLER_BEE_LARVA)
        return (true);

    // Special-case gnolls so they can't get past (hob)goblins
    if (m1->type == MONS_GNOLL && m2->type != MONS_GNOLL)
        return (false);

    return (mchar1 == mchar2 && m1->hit_dice > m2->hit_dice);
}


///////////////////////////////////////////////////////////////////////////////
// monsters methods

monsters::monsters()
    : type(-1), hit_points(0), max_hit_points(0), hit_dice(0),
      ac(0), ev(0), speed(0), speed_increment(0), x(0), y(0),
      target_x(0), target_y(0), inv(), spells(), attitude(ATT_NEUTRAL),
      behaviour(BEH_WANDER), foe(MHITYOU), enchantments(), flags(0L),
      number(0), colour(BLACK), foe_memory(0), god(GOD_NO_GOD),
      ghost()
{
}

monsters::monsters(const monsters &mon)
{
    init_with(mon);
}

monsters &monsters::operator = (const monsters &mon)
{
    init_with(mon);
    return (*this);
}

void monsters::init_with(const monsters &mon)
{
    type              = mon.type;
    hit_points        = mon.hit_points;
    max_hit_points    = mon.max_hit_points;
    hit_dice          = mon.hit_dice;
    ac                = mon.ac;
    ev                = mon.ev;
    speed             = mon.speed;
    speed_increment   = mon.speed_increment;
    x                 = mon.x;
    y                 = mon.y;
    target_x          = mon.target_x;
    target_y          = mon.target_y;
    inv               = mon.inv;
    spells            = mon.spells;
    attitude          = mon.attitude;
    behaviour         = mon.behaviour;
    foe               = mon.foe;
    enchantments      = mon.enchantments;
    flags             = mon.flags;
    number            = mon.number;
    colour            = mon.colour;
    foe_memory        = mon.foe_memory;
    god               = mon.god;
    
    if (mon.ghost.get())
        ghost.reset( new ghost_demon( *mon.ghost ) );
}

coord_def monsters::pos() const
{
    return coord_def(x, y);
}

bool monsters::swimming() const
{
    const int grid = grd[x][y];
    return (grid_is_watery(grid) && monster_habitat(type) == DNGN_DEEP_WATER);
}

bool monsters::floundering() const
{
    const int grid = grd[x][y];
    return (grid_is_water(grid)
            // Can't use monster_habitable_grid because that'll return true
            // for non-water monsters in shallow water.
            && monster_habitat(type) != DNGN_DEEP_WATER
            && !mons_class_flag(type, M_AMPHIBIOUS)
            && !mons_flies(this));
}

size_type monsters::body_size(int /* psize */, bool /* base */) const
{
    const monsterentry *e = seekmonster(type);
    return (e? e->size : SIZE_MEDIUM);
}

int monsters::damage_type(int which_attack)
{
    const item_def *mweap = weapon(which_attack);

    if (!mweap)
    {
        const mon_attack_def atk = mons_attack_spec(this, which_attack);
        return (atk.type == AT_CLAW? DVORP_CLAWING : DVORP_CRUSHING);
    }
 
    return (get_vorpal_type(*mweap));
}

int monsters::damage_brand(int which_attack)
{
    const item_def *mweap = weapon(which_attack);

    if (!mweap)
        return (SPWPN_NORMAL);
    
    return (!is_range_weapon(*mweap)? get_weapon_brand(*mweap) : SPWPN_NORMAL);
}

item_def *monsters::weapon(int which_attack)
{
    if (which_attack > 1)
        which_attack &= 1;
    
    // This randomly picks one of the wielded weapons for monsters that can use
    // two weapons. Not ideal, but better than nothing. fight.cc does it right,
    // for various values of right.
    int weap = inv[MSLOT_WEAPON];

    if (which_attack && mons_wields_two_weapons(this))
    {
        const int offhand = mons_offhand_weapon_index(this);
        if (offhand != NON_ITEM
            && (weap == NON_ITEM || which_attack == 1 || coinflip()))
        {
            weap = offhand;
        }
    }

    return (weap == NON_ITEM? NULL : &mitm[weap]);
}

static int equip_slot_to_mslot(equipment_type eq)
{
    switch (eq)
    {
    case EQ_WEAPON: return MSLOT_WEAPON;
    case EQ_BODY_ARMOUR: return MSLOT_ARMOUR;
    default: return (-1);
    }
}

item_def *monsters::slot_item(equipment_type eq)
{
    int mslot = equip_slot_to_mslot(eq);
    int mindex = mslot == -1? NON_ITEM : inv[mslot];
    return (mindex == NON_ITEM? NULL: &mitm[mindex]);
}

item_def *monsters::shield()
{
    return (NULL);
}

std::string monsters::name(description_level_type desc) const
{
    return (ptr_monam(this, desc));
}

std::string monsters::name(description_level_type desc, bool force_vis) const
{
    if (!force_vis || !has_ench(ENCH_INVIS))
        return (name(desc));

    monsters m = *this;
    m.del_ench(ENCH_INVIS, true);
    return (m.name(desc));
}

std::string monsters::pronoun(pronoun_type pro) const
{
    return (mons_pronoun(type, pro));
}

std::string monsters::conj_verb(const std::string &verb) const
{
    if (!verb.empty() && verb[0] == '!')
        return (verb.substr(1));
    
    if (verb == "are")
        return ("is");
    
    return (pluralize(verb));
}

int monsters::id() const
{
    return (type);
}

bool monsters::fumbles_attack(bool verbose)
{
    if (floundering() && one_chance_in(4))
    {
        if (verbose && !silenced(you.x_pos, you.y_pos)
            && !silenced(x, y))
        {
            mprf(MSGCH_SOUND, "You hear a splashing noise.");
        }
        return (true);
    }

    if (mons_is_submerged(this))
        return (true);
    
    return (false);
}

bool monsters::cannot_fight() const
{
    return mons_class_flag(type, M_NO_EXP_GAIN)
        || mons_is_statue(type);
}

void monsters::attacking(actor * /* other */)
{
}

void monsters::go_berserk(bool /* intentional */)
{
}

void monsters::expose_to_element(beam_type, int)
{
}

void monsters::banish(const std::string &)
{
    monster_die(this, KILL_RESET, 0);
}

int monsters::holy_aura() const
{
    return ((type == MONS_DAEVA || type == MONS_ANGEL)? hit_dice : 0);
}

bool monsters::visible() const
{
    return (mons_near(this) && player_monster_visible(this));
}

bool monsters::confused() const
{
    return (mons_is_confused(this));
}

bool monsters::paralysed() const
{
    return (mons_is_paralysed(this));
}

bool monsters::asleep() const
{
    return (mons_is_sleeping(this));
}

int monsters::shield_bonus() const
{
    // XXX: Monsters don't actually get shields yet.
    const item_def *shld = const_cast<monsters*>(this)->shield();
    if (shld)
    {
        const int shld_c = property(*shld, PARM_AC);
        return (random2(shld_c + random2(hit_dice / 2)) / 2);
    }
    return (0);
}

int monsters::shield_block_penalty() const
{
    return (0);
}

int monsters::shield_bypass_ability(int) const
{
    return (15 + hit_dice * 2 / 3);
}

int monsters::armour_class() const
{
    return (ac);
}

int monsters::melee_evasion(const actor *act) const
{
    int evasion = ev;
    if (paralysed() || asleep() || one_chance_in(20))
        evasion = 0;
    else if (confused())
        evasion /= 2;
    return (evasion);
}

void monsters::heal(int amount, bool max_too)
{
    hit_points += amount;
    if (max_too)
        max_hit_points += amount;
    if (hit_points > max_hit_points)
        hit_points = max_hit_points;
}

int monsters::holiness() const
{
    return (mons_holiness(this));
}

int monsters::res_fire() const
{
    return (mons_res_fire(this));
}

int monsters::res_cold() const
{
    return (mons_res_cold(this));
}

int monsters::res_elec() const
{
    return (mons_res_elec(this));
}

int monsters::res_poison() const
{
    return (mons_res_poison(this));
}

int monsters::res_negative_energy() const
{
    return (mons_res_negative_energy(this));
}

bool monsters::levitates() const
{
    return (mons_flies(this));
}

int monsters::mons_species() const
{
    return ::mons_species(type);
}

void monsters::poison(actor *agent, int amount)
{
    if (amount <= 0)
        return;

    // Scale poison down for monsters.
    if (!(amount /= 2))
        amount = 1;

    poison_monster(this, agent->kill_alignment(), amount);
}

int monsters::skill(skill_type sk, bool) const
{
    switch (sk)
    {
    case SK_NECROMANCY:
        return (holiness() == MH_UNDEAD? hit_dice / 2 : hit_dice / 3);
        
    default:
        return (0);
    }
}

void monsters::blink()
{
    monster_blink(this);
}

void monsters::teleport(bool now, bool)
{
    monster_teleport(this, now, false);
}

bool monsters::alive() const
{
    return (hit_points > 0 && type != -1);
}

god_type monsters::deity() const
{
    return (god);
}

void monsters::hurt(actor *agent, int amount)
{
    if (amount <= 0)
        return;
    
    hit_points -= amount;
    if ((hit_points < 1 || hit_dice < 1) && type != -1)
    {
        if (agent->atype() == ACT_PLAYER)
            monster_die(this, KILL_YOU, 0);
        else
            monster_die(this, KILL_MON,
                        monster_index( dynamic_cast<monsters*>(agent) ));
    }
}

void monsters::rot(actor *agent, int rotlevel, int immed_rot)
{
    if (mons_holiness(this) != MH_NATURAL)
        return;
    
    // Apply immediate damage because we can't handle rotting for monsters yet.
    const int damage = immed_rot;
    if (damage > 0)
    {
        if (mons_near(this) && player_monster_visible(this))
            mprf("%s %s!",
                 name(DESC_CAP_THE).c_str(),
                 rotlevel == 0? "looks less resilient" : "rots");
        hurt(agent, damage);
        if (alive())
        {
            max_hit_points -= immed_rot * 2;
            if (hit_points > max_hit_points)
                hit_points = max_hit_points;
        }
    }

    if (rotlevel > 0)
        add_ench( mon_enchant(ENCH_ROT, cap_int(rotlevel, 4),
                              agent->kill_alignment()) );
}

void monsters::confuse(int strength)
{
    bolt beam_temp;
    beam_temp.flavour = BEAM_CONFUSION;
    mons_ench_f2( this, beam_temp );
}

void monsters::paralyse(int strength)
{
    bolt paralysis;
    paralysis.flavour = BEAM_PARALYSIS;
    mons_ench_f2(this, paralysis);
}

void monsters::slow_down(int strength)
{
    bolt slow;
    slow.flavour = BEAM_SLOW;
    mons_ench_f2(this, slow);
}

void monsters::set_ghost(const ghost_demon &g)
{
    ghost.reset( new ghost_demon(g) );
}

void monsters::pandemon_init()
{
    hit_dice = ghost->values[ GVAL_DEMONLORD_HIT_DICE ];
    hit_points = ghost->values[ GVAL_MAX_HP ];
    max_hit_points = ghost->values[ GVAL_MAX_HP ];
    ac = ghost->values[ GVAL_AC ];
    ev = ghost->values[ GVAL_EV ];
    speed = (one_chance_in(3) ? 10 : 6 + roll_dice(2, 9));
    speed_increment = 70;
    colour = random_colour();        // demon's colour
    load_spells(MST_GHOST);
}

void monsters::ghost_init()
{
    type = MONS_PLAYER_GHOST;
    hit_dice = ghost->values[ GVAL_EXP_LEVEL ];
    hit_points = ghost->values[ GVAL_MAX_HP ];
    max_hit_points = ghost->values[ GVAL_MAX_HP ];
    ac = ghost->values[ GVAL_AC];
    ev = ghost->values[ GVAL_EV ];
    speed = ghost->values[ GVAL_SPEED ];
    speed_increment = 70;
    attitude = ATT_HOSTILE;
    behaviour = BEH_WANDER;
    flags = 0;
    foe = MHITNOT;
    foe_memory = 0;
    colour = mons_class_colour(MONS_PLAYER_GHOST);
    number = 250;
    load_spells(MST_GHOST);

    inv.init(NON_ITEM);
    enchantments.clear();

    find_place_to_live();
}

bool monsters::find_home_in(coord_def s, coord_def e)
{
    for (int iy = s.y; iy <= e.y; ++iy)
    {
        for (int ix = s.x; ix <= e.x; ++ix)
        {
            if (!in_bounds(ix, iy))
                continue;

            if (ix == you.x_pos && iy == you.y_pos)
                continue;

            if (mgrd[ix][iy] != NON_MONSTER || grd[ix][iy] < DNGN_FLOOR)
                continue;

            x = ix;
            y = iy;
            return (true);
        }
    }

    return (false);
}

bool monsters::find_place_near_player()
{
    return (find_home_in( you.pos() - coord_def(1, 1),
                          you.pos() + coord_def(1, 1) )
            || find_home_in( you.pos() - coord_def(6, 6),
                             you.pos() + coord_def(6, 6) ));
}

bool monsters::find_home_anywhere()
{
    int tries = 600;
    do
    {
        x = random_range(6, GXM - 7);
        y = random_range(6, GYM - 7);
    }
    while ((grd[x][y] != DNGN_FLOOR
           || mgrd[x][y] != NON_MONSTER)
           && tries-- > 0);

    return (tries >= 0);
}

bool monsters::find_place_to_live(bool near_player)
{
    if ((near_player && find_place_near_player())
        || find_home_anywhere())
    {
        mgrd[x][y] = monster_index(this);
        return (true);
    }

    return (false);
}

void monsters::destroy_inventory()
{
    for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
    {
        if (inv[j] != NON_ITEM)
        {
            destroy_item( inv[j] );
            inv[j] = NON_ITEM;
        }
    }
}

void monsters::reset()
{
    destroy_inventory();
    
    enchantments.clear();
    inv.init(NON_ITEM);

    flags = 0;
    type = -1;
    hit_points = 0;
    max_hit_points = 0;
    hit_dice = 0;
    ac = 0;
    ev = 0;
    speed_increment = 0;
    attitude = ATT_HOSTILE;
    behaviour = BEH_SLEEP;
    foe = MHITNOT;
    number = 0;

    if (in_bounds(x, y))
        mgrd[x][y] = NON_MONSTER;

    x = y = 0;
    ghost.reset(NULL);
}

bool monsters::needs_transit() const
{
    return ((mons_is_unique(type)
             || (flags & MF_BANISHED)
             || (you.level_type == LEVEL_DUNGEON && hit_dice > 8 + random2(25)))
            && !mons_is_summoned(this));
}

void monsters::set_transit(level_id dest)
{
    add_monster_to_transit(dest, *this);
}

void monsters::load_spells(int book)
{
    spells.init(MS_NO_SPELL);
    if (book == MST_NO_SPELLS || (book == MST_GHOST && !ghost.get()))
        return;

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS, "%s: loading spellbook #%d", 
          name(DESC_PLAIN).c_str(), book );
#endif

    if (book == MST_GHOST)
    {
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
        {
            spells[i] = ghost->values[ GVAL_SPELL_1 + i ];
#if DEBUG_DIAGNOSTICS
            mprf( MSGCH_DIAGNOSTICS, "spell #%d: %d", i, spells[i] );
#endif
        }
    }
    else 
    {
        int i = 0;
        // this needs to be rewritten a la the monsterseek rewrite {dlb}:
        for (; i < NUM_MSTYPES; i++)
        {
            if (mspell_list[i][0] == book)
                break;
        }

        if (i < NUM_MSTYPES)
        {
            for (int z = 0; z < NUM_MONSTER_SPELL_SLOTS; z++)
                spells[z] = mspell_list[i][z + 1];
        }
    }
}

bool monsters::has_ench(enchant_type ench,
                        enchant_type ench2) const
{
    if (ench2 == ENCH_NONE)
        ench2 = ench;

    for (int i = ench; i <= ench2; ++i)
    {
        if (enchantments.find( static_cast<enchant_type>(i) ) !=
            enchantments.end())
        {
            return (true);
        }
    }
    return (false);
}

mon_enchant monsters::get_ench(enchant_type ench1,
                               enchant_type ench2) const
{
    if (ench2 == ENCH_NONE)
        ench2 = ench1;

    for (int e = ench1; e <= ench2; ++e)
    {
        mon_enchant_list::const_iterator i =
            enchantments.find(static_cast<enchant_type>(e));
        if (i != enchantments.end())
            return (*i);
    }

    return mon_enchant();
}

void monsters::update_ench(const mon_enchant &ench)
{
    if (ench.ench != ENCH_NONE)
    {
        mon_enchant_list::iterator i = enchantments.find(ench);
        if (i != enchantments.end())
            enchantments.erase(i);
        enchantments.insert(ench);
    }
}

bool monsters::add_ench(const mon_enchant &ench)
{
    // silliness
    if (ench.ench == ENCH_NONE)
        return (false);

    mon_enchant_list::iterator i = enchantments.find(ench);
    if (i == enchantments.end())
        enchantments.insert(ench);
    else
    {
        mon_enchant new_ench = *i + ench;
        enchantments.erase(i);
        enchantments.insert(new_ench);
    }

    // check for slow/haste
    if (ench.ench == ENCH_HASTE)
    {
        if (speed >= 100)
            speed = 100 + ((speed - 100) * 2);
        else
            speed *= 2;
    }

    if (ench.ench == ENCH_SLOW)
    {
        if (speed >= 100)
            speed = 100 + ((speed - 100) / 2);
        else
            speed /= 2;
    }

    return (true);
}

bool monsters::del_ench(enchant_type ench, bool quiet)
{
    mon_enchant_list::iterator i = enchantments.find(ench);
    if (i == enchantments.end())
        return (false);

    mon_enchant me = *i;
    enchantments.erase(i);

    remove_enchantment_effect(me, quiet);
    return (true);
}

void monsters::remove_enchantment_effect(const mon_enchant &me, bool quiet)
{
    switch (me.ench)
    {
    case ENCH_HASTE:
        if (speed >= 100)
            speed = 100 + ((speed - 100) / 2);
        else
            speed /= 2;
        break;

    case ENCH_SLOW:
        if (speed >= 100)
            speed = 100 + ((speed - 100) * 2);
        else
            speed *= 2;
        break;

    case ENCH_PARALYSIS:
        if (!quiet)
            simple_monster_message(this, " is no longer paralysed.");

        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_FEAR:
        if (!quiet)
            simple_monster_message(this, " seems to regain its courage.");

        // reevaluate behaviour
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_CONFUSION:
        if (!quiet)
            simple_monster_message(this, " seems less confused.");

        // reevaluate behaviour
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_INVIS:
        // invisible monsters stay invisible
        if (mons_class_flag(type, M_INVIS))
            add_ench( mon_enchant(ENCH_INVIS) );
        else if (mons_near(this) && !player_see_invis() 
                    && !has_ench( ENCH_SUBMERGED ))
        {
            if (!quiet)
                mprf("%s appears!", name(DESC_CAP_A).c_str() );
            
            seen_monster(this);
        }
        break;

    case ENCH_CHARM:
        if (!quiet)
            simple_monster_message(this, " is no longer charmed.");

        // reevaluate behaviour
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_BACKLIGHT:
        if (!quiet)
            simple_monster_message(this, " stops glowing.");
        break;

    case ENCH_STICKY_FLAME:
        if (!quiet)
            simple_monster_message(this, " stops burning.");
        break;

    case ENCH_POISON:
        if (!quiet)
            simple_monster_message(this, " looks more healthy.");
        break;

    case ENCH_ROT:
        if (!quiet)
            simple_monster_message(this, " is no longer rotting.");
        break;

    case ENCH_ABJ:
    case ENCH_SHORT_LIVED:
        add_ench( mon_enchant(ENCH_ABJ) );
        monster_die( this, quiet? KILL_DISMISSED : KILL_RESET, 0 );
        break;

    default:
        break;
    }
}

void monsters::lose_ench_levels(const mon_enchant &e, int levels)
{
    if (!levels)
        return;

    mon_enchant me(e);
    me.degree -= levels;

    if (me.degree < 1)
        del_ench(e.ench);
    else
        update_ench(me);
}

//---------------------------------------------------------------
//
// timeout_enchantments
//
// Update a monster's enchantments when the player returns
// to the level.
//
// Management for enchantments... problems with this are the oddities 
// (monster dying from poison several thousands of turns later), and 
// game balance.  
//
// Consider: Poison/Sticky Flame a monster at range and leave, monster 
// dies but can't leave level to get to player (implied game balance of 
// the delayed damage is that the monster could be a danger before 
// it dies).  This could be fixed by keeping some monsters active 
// off level and allowing them to take stairs (a very serious change).
//
// Compare this to the current abuse where the player gets 
// effectively extended duration of these effects (although only 
// the actual effects only occur on level, the player can leave 
// and heal up without having the effect disappear).  
//
// This is a simple compromise between the two... the enchantments
// go away, but the effects don't happen off level.  -- bwr
//
//---------------------------------------------------------------
void monsters::timeout_enchantments(int levels)
{
    for (mon_enchant_list::iterator i = enchantments.begin();
         i != enchantments.end(); )
    {
        mon_enchant_list::iterator cur = i++;

        switch (cur->ench)
        {
        case ENCH_POISON: case ENCH_ROT: case ENCH_BACKLIGHT:
        case ENCH_STICKY_FLAME: case ENCH_ABJ: case ENCH_SHORT_LIVED:
        case ENCH_SLOW: case ENCH_HASTE: case ENCH_FEAR:
        case ENCH_INVIS: case ENCH_CHARM: case ENCH_SLEEP_WARY:
            lose_ench_levels(*cur, levels);
            break;

        case ENCH_TP:
            teleport(true);
            break;

        case ENCH_CONFUSION:
            blink();
            break;

        default:
            break;
        }
    }
}

// used to adjust time durations in handle_enchantment() for monster speed
static inline int mod_speed( int val, int speed )
{
    return (speed ? (val * 10) / speed : val);
}

void monsters::apply_enchantment(mon_enchant me, int spd)
{
    switch (me.ench)
    {
    case ENCH_SLOW:
        if (random2(250) <= mod_speed( hit_dice + 10, spd ))
            del_ench(me.ench);
        break;

    case ENCH_HASTE:
        if (random2(1000) < mod_speed( 25, spd ))
            del_ench(ENCH_HASTE);
        break;

    case ENCH_FEAR:
        if (random2(150) <= mod_speed( hit_dice + 5, spd ))
            del_ench(ENCH_FEAR);
        break;

    case ENCH_PARALYSIS:
        if (random2(120) < mod_speed( hit_dice + 5, spd ))
            del_ench(ENCH_PARALYSIS);
        break;

    case ENCH_CONFUSION:
        if (random2(120) < mod_speed( hit_dice + 5, spd ))
        {
            // don't delete perma-confusion
            if (!mons_class_flag(type, M_CONFUSED))
                del_ench(ENCH_CONFUSION);
        }
        break;

    case ENCH_INVIS:
        if (random2(1000) < mod_speed( 25, spd ))
        {
            // don't delete perma-invis
            if (!mons_class_flag( type, M_INVIS ))
                del_ench(ENCH_INVIS);
        }
        break;

    case ENCH_SUBMERGED:
    {
        // not even air elementals unsubmerge into clouds
        if (env.cgrid[x][y] != EMPTY_CLOUD)
            break;

        // Air elementals are a special case, as their
        // submerging in air isn't up to choice. -- bwr
        if (type == MONS_AIR_ELEMENTAL) 
        {
            heal_monster( this, 1, one_chance_in(5) );

            if (one_chance_in(5))
                del_ench(ENCH_SUBMERGED);

            break;
        }

        // Now we handle the others:
        int grid = grd[x][y];

        // Badly injured monsters prefer to stay submerged...
        // electrical eels and lava snakes have ranged attacks
        // and are more likely to surface.  -- bwr
        if (!monster_can_submerge(type, grid))
            del_ench(ENCH_SUBMERGED); // forced to surface
        else if (hit_points <= max_hit_points / 2)
            break;
        else if (((type == MONS_ELECTRICAL_EEL
                   || type == MONS_LAVA_SNAKE)
                  && (random2(1000) < mod_speed( 20, spd )
                      || (mons_near(this) 
                          && hit_points == max_hit_points
                          && !one_chance_in(10))))
                 || random2(2000) < mod_speed(10, spd)
                 || (mons_near(this)
                     && hit_points == max_hit_points
                     && !one_chance_in(5)))
        {
            del_ench(ENCH_SUBMERGED);
        }
        break;
    }
    case ENCH_POISON:
    {
        int poisonval = me.degree;
        int dam = (poisonval >= 4) ? 1 : 0;

        if (coinflip())
            dam += roll_dice( 1, poisonval + 1 );

        if (mons_res_poison(this) < 0)
            dam += roll_dice( 2, poisonval ) - 1;

        // We adjust damage for monster speed (since this is applied 
        // only when the monster moves), and we handle the factional
        // part as well (so that speed 30 creatures will take damage).
        dam *= 10;
        dam = (dam / spd) + ((random2(spd) < (dam % spd)) ? 1 : 0);

        if (dam > 0)
        {
            hurt_monster( this, dam );

#if DEBUG_DIAGNOSTICS
            // for debugging, we don't have this silent.
            simple_monster_message( this, " takes poison damage.", 
                                    MSGCH_DIAGNOSTICS );
            snprintf( info, INFO_SIZE, "poison damage: %d", dam );
            mpr( info, MSGCH_DIAGNOSTICS );
#endif

            if (hit_points < 1)
            {
                monster_die(this, me.killer(), me.kill_agent());
                break;
            }
        }

        // chance to get over poison (1 in 8, modified for speed)
        if (random2(1000) < mod_speed( 125, spd ))
            lose_ench_levels(me, 1);
        break;
    }
    case ENCH_ROT:
        if (hit_points > 1 
                 && random2(1000) < mod_speed( 333, spd ))
        {
            hurt_monster(this, 1);
            if (hit_points < max_hit_points && coinflip())
                --max_hit_points;
        }

        if (random2(1000) < mod_speed( me.degree == 1? 250 : 333, spd ))
            lose_ench_levels(me, 1);
        
        break;

    case ENCH_BACKLIGHT:
        if (random2(1000) < mod_speed( me.degree == 1? 100 : 200, spd ))
            lose_ench_levels(me, 1);
        break;

        // assumption: mons_res_fire has already been checked
    case ENCH_STICKY_FLAME:
    {
        int dam = roll_dice( 2, 4 ) - 1;

        if (mons_res_fire( this ) < 0)
            dam += roll_dice( 2, 5 ) - 1;

        // We adjust damage for monster speed (since this is applied 
        // only when the monster moves), and we handle the factional
        // part as well (so that speed 30 creatures will take damage).
        dam *= 10;
        dam = (dam / spd) + ((random2(spd) < (dam % spd)) ? 1 : 0);

        if (dam > 0)
        {
            hurt_monster( this, dam );
            simple_monster_message(this, " burns!");

#if DEBUG_DIAGNOSTICS
            mprf( MSGCH_DIAGNOSTICS, "sticky flame damage: %d", dam );
#endif

            if (hit_points < 1)
            {
                monster_die(this, me.killer(), me.kill_agent());
                break;
            }
        }

        // chance to get over sticky flame (1 in 5, modified for speed)
        if (random2(1000) < mod_speed( 200, spd ))
            lose_ench_levels(me, 1);
        break;
    }

    case ENCH_SHORT_LIVED:
        // This should only be used for ball lightning -- bwr
        if (random2(1000) < mod_speed( 200, spd ))
            hit_points = -1;
        break;

        // 19 is taken by summoning:
        // If these are changed, must also change abjuration
    case ENCH_ABJ:
    {
        const int mspd =
            me.degree == 6? 10 :
            me.degree == 5? 20 : 100;
        
        if (random2(1000) < mod_speed( mspd, spd ))
            lose_ench_levels(me, 1);
        break;
    }

    case ENCH_CHARM:
        if (random2(500) <= mod_speed( hit_dice + 10, spd ))
            del_ench(ENCH_CHARM);
        break;

    case ENCH_GLOWING_SHAPESHIFTER:     // this ench never runs out
        // number of actions is fine for shapeshifters
        if (type == MONS_GLOWING_SHAPESHIFTER 
            || random2(1000) < mod_speed( 250, spd ))
        {
            monster_polymorph(this, RANDOM_MONSTER, 0);
        }
        break;

    case ENCH_SHAPESHIFTER:     // this ench never runs out
        if (type == MONS_SHAPESHIFTER 
            || random2(1000) < mod_speed( 1000 / ((15 * hit_dice) / 5), spd ))
        {
            monster_polymorph(this, RANDOM_MONSTER, 0);
        }
        break;

    case ENCH_TP:
        if (me.degree <= 1)
        {
            del_ench(ENCH_TP);
            monster_teleport( this, true );
        }
        else
        {
            int tmp = mod_speed( 1000, spd );

            if (tmp < 1000 && random2(1000) < tmp)
                lose_ench_levels(me, 1);
            else if (me.degree - tmp / 1000 >= 1)
            {
                lose_ench_levels(me, tmp / 1000);
                tmp %= 1000;

                if (random2(1000) < tmp)
                {
                    if (me.degree > 1)
                        lose_ench_levels(me, 1);
                    else
                    {
                        del_ench( ENCH_TP );
                        monster_teleport( this, true );
                    }
                }
            }
            else
            {
                del_ench( ENCH_TP );
                monster_teleport( this, true );
            }
        }
        break;

    case ENCH_SLEEP_WARY:
        if (random2(1000) < mod_speed( 50, spd ))
            del_ench(ENCH_SLEEP_WARY);
        break;

    default:
        break;
    }
}

void monsters::apply_enchantments(int spd)
{
    for (mon_enchant_list::iterator i = enchantments.begin();
         i != enchantments.end(); )
    {
        mon_enchant_list::iterator cur = i++;
        apply_enchantment(*cur, spd);

        // If the monster died, the iterator will be invalid!
        if (!alive())
            break;
    }
}

kill_category monsters::kill_alignment() const
{
    return (attitude == ATT_FRIENDLY? KC_FRIENDLY : KC_OTHER);
}

/////////////////////////////////////////////////////////////////////////
// mon_enchant

static const char *enchant_names[] = 
{
    "none", "slow", "haste", "fear", "conf", "inv", "pois", "bers",
    "rot", "summon", "abj", "backlit", "charm", "fire",
    "gloshifter", "shifter", "tp", "wary", "submerged",
    "short lived", "paralysis", "bug"
};

const char *mons_enchantment_name(enchant_type ench)
{
    ASSERT(NUM_ENCHANTMENTS ==
           (sizeof(enchant_names) / sizeof(*enchant_names)) - 1);
    
    if (ench > NUM_ENCHANTMENTS)
        ench = NUM_ENCHANTMENTS;

    return (enchant_names[ench]);
}

mon_enchant::operator std::string () const
{
    return make_stringf("%s (%d%s)",
                        mons_enchantment_name(ench),
                        degree,
                        kill_category_desc(who));
}

const char *mon_enchant::kill_category_desc(kill_category k) const
{
    return (k == KC_YOU?      "you" :
            k == KC_FRIENDLY? "pet" : "");
}

void mon_enchant::merge_killer(kill_category k)
{
    who = who < k? who : k;
}

void mon_enchant::cap_degree()
{
    // Hard cap to simulate old enum behaviour, we should really throw this
    // out entirely.
    const int max = ench == ENCH_ABJ? 6 : 4;
    if (degree > max)
        degree = max;
}

mon_enchant &mon_enchant::operator += (const mon_enchant &other)
{
    if (ench == other.ench)
    {
        degree += other.degree;
        cap_degree();
        merge_killer(other.who);
    }
    return (*this);
}

mon_enchant mon_enchant::operator + (const mon_enchant &other) const
{
    mon_enchant tmp(*this);
    tmp += other;
    return (tmp);
}

int mon_enchant::killer() const
{
    return (who == KC_YOU?       KILL_YOU :
            who == KC_FRIENDLY?  KILL_MON :
            KILL_MISC);
}

int mon_enchant::kill_agent() const
{
    return (who == KC_FRIENDLY? ANON_FRIENDLY_MONSTER : 0);
}
