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

#include "debug.h"
#include "itemname.h"
#include "itemprop.h"
#include "monplace.h"
#include "mstuff2.h"
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
    if (mons_genus(mon->type) == MONS_DRACONIAN 
            && mon->type != MONS_DRACONIAN)
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
        return (ghost.values[ GVAL_SEE_INVIS ]);
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
    if (mons_has_ench(targ, ENCH_SUBMERGED)
        || (mons_has_ench(targ, ENCH_INVIS) && !mons_see_invis(mon)))
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

int mons_damage(int mc, int rt)
{
    ASSERT(rt >= 0);
    ASSERT(rt <= 3);

    if (rt < 0 || rt > 3)       // make it fool-proof
        return (0);

    if (rt == 0 && (mc == MONS_PLAYER_GHOST || mc == MONS_PANDEMONIUM_DEMON))
        return (ghost.values[ GVAL_DAMAGE ]);

    return (smc->damage[rt]);
}                               // end mons_damage()

int mons_resist_magic( const monsters *mon )
{
    int u = (seekmonster(mon->type))->resist_magic;

    // negative values get multiplied with mhd
    if (u < 0)
        u = mon->hit_dice * (-u * 2);

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

    if (mrs == 5000)
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

    return ((mrch2 < mrchance) ? true : false);
}                               // end check_mons_resist_magic()


int mons_res_elec( const monsters *mon )
{
    int mc = mon->type;

    if (mc == MONS_PLAYER_GHOST || mc == MONS_PANDEMONIUM_DEMON)
        return (ghost.values[ GVAL_RES_ELEC ]);

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
        return (ghost.values[ GVAL_RES_FIRE ]);

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
        return (ghost.values[ GVAL_RES_COLD ]);

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
             // && !mons_has_ench( mon, ENCH_PETRIFY ));
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
    if (mc == MONS_PANDEMONIUM_DEMON)
        return (ghost.values[ GVAL_DEMONLORD_FLY ]);

    int f = smc->bitfields;

    if (f & M_FLIES)
        return (1);

    if (f & M_LEVITATE)
        return (2);

    return (0);
}

int mons_flies( const monsters *mon )
{
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
    int x, y;

    if (book == MST_NO_SPELLS)
    {
        for (y = 0; y < NUM_MONSTER_SPELL_SLOTS; y++)
            mon->spells[y] = MS_NO_SPELL;
        return;
    }

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS, "%s: loading spellbook #%d", 
            ptr_monam( mon, DESC_PLAIN ), book );
#endif

    for (x = 0; x < 6; x++)
        mon->spells[x] = MS_NO_SPELL;

    if (book == MST_GHOST)
    {
        for (y = 0; y < NUM_MONSTER_SPELL_SLOTS; y++)
        {
            mon->spells[y] = ghost.values[ GVAL_SPELL_1 + y ];
#if DEBUG_DIAGNOSTICS
            mprf( MSGCH_DIAGNOSTICS, "spell #%d: %d", y, mon->spells[y] );
#endif
        }
    }
    else 
    {
        // this needs to be rewritten a la the monsterseek rewrite {dlb}:
        for (x = 0; x < NUM_MSTYPES; x++)
        {
            if (mspell_list[x][0] == book)
                break;
        }

        if (x < NUM_MSTYPES)
        {
            for (y = 0; y < 6; y++)
                mon->spells[y] = mspell_list[x][y + 1];
        }
    }
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

        if (hd >= 7)
        {
            mons.type = MONS_BLACK_DRACONIAN + random2(8);
            col = mons_class_colour( mons.type );
        }
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
    mons.armour_class = ac;
    mons.evasion = ev;
    mons.speed = speed;
    mons.speed_increment = 70;
    mons.number = monnumber;
    mons.flags = 0L;
    mons.colour = col;
    mons_load_spells( &mons, spells );

    // reset monster enchantments
    for (int i = 0; i < NUM_MON_ENCHANTS; i++)
        mons.enchantment[i] = ENCH_NONE;
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

    return (monam( mon->number, mon->type,
                   force_seen || player_monster_visible( mon ), 
                   desc, mon->inv[MSLOT_WEAPON] ));
}

const char *monam( int mons_num, int mons, bool vis, char desc, int mons_wpn )
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
        strcpy(gmo_n, ghost.name);
        strcat(gmo_n, "'s ghost");
        break;

    case MONS_PANDEMONIUM_DEMON:
        strcpy(gmo_n, ghost.name);
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
        }

        strcpy(gmon_name, glog);
        return (glog);
    }

    if (!mons_is_unique( mons_num )) 
    {
        switch (descrip)
        {
        case DESC_CAP_THE:
            strcpy(glog, "The ");
            break;
        case DESC_NOCAP_THE:
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
        fr1 = (mon1->attitude == ATT_FRIENDLY) || mons_has_ench(mon1, ENCH_CHARM);
    }

    if (m2 == MHITYOU)
        fr2 = true;
    else
    {
        mon2 = &menv[m2];
        fr2 = (mon2->attitude == ATT_FRIENDLY) || mons_has_ench(mon2, ENCH_CHARM);
    }

    return (fr1 == fr2);
}

bool mons_wields_two_weapons(const monsters *m)
{
    return (m->type == MONS_TWO_HEADED_OGRE || m->type == MONS_ETTIN);
}

// Does not check whether the monster can dual-wield - that is the
// caller's responsibility.
int mons_offhand_weapon_index(const monsters *m)
{
    return (m->inv[1]);
}

int mons_weapon_index(const monsters *m)
{
    // This randomly picks one of the wielded weapons for monsters that can use
    // two weapons. Not ideal, but better than nothing. fight.cc does it right,
    // for various values of right.
    int weap = m->inv[MSLOT_WEAPON];

    if (mons_wields_two_weapons(m))
    {
        const int offhand = mons_offhand_weapon_index(m);
        if (offhand != NON_ITEM && (weap == NON_ITEM || coinflip()))
            weap = offhand;
    }

    return (weap);
}

int mons_base_damage_type(const monsters *m)
{
    return (mons_class_flag(m->type, M_CLAWS)? DVORP_CLAWING : DVORP_CRUSHING);
}

int mons_damage_type(const monsters *m)
{
    const int mweap = mons_weapon_index(m);

    if (mweap == NON_ITEM)
        return (mons_base_damage_type(m));

    return (get_vorpal_type(mitm[mweap]));
}

int mons_damage_brand(const monsters *m)
{
    const int mweap = mons_weapon_index(m);

    if (mweap == NON_ITEM)
        return (SPWPN_NORMAL);

    const item_def &weap = mitm[mweap];
    return (!is_range_weapon(weap)? get_weapon_brand(weap) : SPWPN_NORMAL);
}

bool mons_friendly(const monsters *m)
{
    return (m->attitude == ATT_FRIENDLY || mons_has_ench(m, ENCH_CHARM));
}

bool mons_is_submerged( const monsters *mon )
{
    // FIXME, switch to 4.1's MF_SUBMERGED system which is much cleaner.
    return (mons_has_ench( mon, ENCH_SUBMERGED ));
}

bool mons_is_paralysed(const monsters *m)
{
    // maybe this should be 70
    return (m->speed_increment <= 60);
}

bool mons_is_confused(const monsters *m)
{
    return (mons_has_ench(m, ENCH_CONFUSION) &&
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

int mons_has_ench(const monsters *mon, unsigned int ench, unsigned int ench2)
{
    // silliness
    if (ench == ENCH_NONE)
        return (ench);

    if (ench2 == ENCH_NONE)
        ench2 = ench;

    for (int p = 0; p < NUM_MON_ENCHANTS; p++)
    {
        if (mon->enchantment[p] >= ench && mon->enchantment[p] <= ench2)
            return (mon->enchantment[p]);
    }

    return (ENCH_NONE);
}

// Returning the deleted enchantment is important!  See abjuration. -- bwr
int mons_del_ench( struct monsters *mon, unsigned int ench, unsigned int ench2,
                   bool quiet )
{
    unsigned int p;
    int ret_val = ENCH_NONE;

    // silliness
    if (ench == ENCH_NONE)
        return (ENCH_NONE);

    if (ench2 == ENCH_NONE)
        ench2 = ench;

    for (p = 0; p < NUM_MON_ENCHANTS; p++)
    {
        if (mon->enchantment[p] >= ench && mon->enchantment[p] <= ench2)
            break;
    }

    if (p == NUM_MON_ENCHANTS)
        return (ENCH_NONE);

    ret_val = mon->enchantment[p];
    mon->enchantment[p] = ENCH_NONE;

    // check for slow/haste
    if (ench == ENCH_HASTE)
    {
        if (mon->speed >= 100)
            mon->speed = 100 + ((mon->speed - 100) / 2);
        else
            mon->speed /= 2;
    }

    if (ench == ENCH_SLOW)
    {
        if (mon->speed >= 100)
            mon->speed = 100 + ((mon->speed - 100) * 2);
        else
            mon->speed *= 2;
    }

    if (ench == ENCH_FEAR)
    {
        if (!quiet)
            simple_monster_message(mon, " seems to regain its courage.");

        // reevaluate behaviour
        behaviour_event(mon, ME_EVAL);
    }

    if (ench == ENCH_CONFUSION)
    {
        if (!quiet)
            simple_monster_message(mon, " seems less confused.");

        // reevaluate behaviour
        behaviour_event(mon, ME_EVAL);
    }

    if (ench == ENCH_INVIS)
    {
        // invisible monsters stay invisible
        if (mons_class_flag(mon->type, M_INVIS))
        {
            mon->enchantment[p] = ENCH_INVIS;
        }
        else if (mons_near(mon) && !player_see_invis() 
                    && !mons_has_ench( mon, ENCH_SUBMERGED ))
        {
            if (!quiet)
            {
                strcpy( info, ptr_monam( mon, DESC_CAP_A ) );
                strcat( info, " appears!" );
                mpr( info );
            }
            seen_monster(mon);
        }
    }

    if (ench == ENCH_CHARM)
    {
        if (!quiet)
            simple_monster_message(mon, " is no longer charmed.");

        // reevaluate behaviour
        behaviour_event(mon, ME_EVAL);
    }

    if (ench == ENCH_BACKLIGHT_I)
    {
        if (!quiet)
            simple_monster_message(mon, " stops glowing.");
    }

    if (ench == ENCH_STICKY_FLAME_I || ench == ENCH_YOUR_STICKY_FLAME_I)
    {
        if (!quiet)
            simple_monster_message(mon, " stops burning.");
    }

    if (ench == ENCH_POISON_I || ench == ENCH_YOUR_POISON_I)
    {
        if (!quiet)
            simple_monster_message(mon, " looks more healthy.");
    }

    if (ench == ENCH_YOUR_ROT_I)
    {
        if (!quiet)
            simple_monster_message(mon, " is no longer rotting.");
    }

    return (ret_val);
}

bool mons_add_ench(struct monsters *mon, unsigned int ench)
{
    // silliness
    if (ench == ENCH_NONE)
        return (false);

    int newspot = -1;

    // don't double-add
    for (int p = 0; p < NUM_MON_ENCHANTS; p++)
    {
        if (mon->enchantment[p] == ench)
            return (true);

        if (mon->enchantment[p] == ENCH_NONE && newspot < 0)
            newspot = p;
    }

    if (newspot < 0)
        return (false);

    mon->enchantment[newspot] = ench;
//    if ench == ENCH_FEAR //mv: withou this fear & repel undead spell doesn't work


    // check for slow/haste
    if (ench == ENCH_HASTE)
    {
        if (mon->speed >= 100)
            mon->speed = 100 + ((mon->speed - 100) * 2);
        else
            mon->speed *= 2;
    }

    if (ench == ENCH_SLOW)
    {
        if (mon->speed >= 100)
            mon->speed = 100 + ((mon->speed - 100) / 2);
        else
            mon->speed /= 2;
    }

    return (true);
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
        if (mons_has_ench( mon, ENCH_HASTE ))
            ret = true;
        break;

    case MS_INVIS:
        if (mons_has_ench( mon, ENCH_INVIS ) ||
            (mons_friendly(mon) && !player_see_invis(false)))
            ret = true;
        break;

    case MS_HEAL:
        if (mon->hit_points > mon->max_hit_points / 2)
            ret = true;
        break;

    case MS_TELEPORT:
        // Monsters aren't smart enough to know when to cancel teleport.
        if (mons_has_ench( mon, ENCH_TP_I, ENCH_TP_IV ))
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
            if (mons_has_ench( &menv[mon->foe], ENCH_TP_I, ENCH_TP_IV))
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

    if (mons_is_unique( mon_type ))
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
