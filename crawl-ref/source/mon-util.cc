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
#include "enum.h"
#include "mon-util.h"
#include "monstuff.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <sstream>

#include "externs.h"

#include "beam.h"
#include "debug.h"
#include "delay.h"
#include "dgnevent.h"
#include "fight.h"
#include "ghost.h"
#include "insult.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "Kills.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "mstuff2.h"
#include "mtransit.h"
#include "player.h"
#include "randart.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "tiles.h"
#include "traps.h"
#include "view.h"

//jmf: moved from inside function
static FixedVector < int, NUM_MONSTERS > mon_entry;

struct mon_spellbook
{
    mon_spellbook_type type;
    spell_type spells[NUM_MONSTER_SPELL_SLOTS];
};

mon_display monster_symbols[NUM_MONSTERS];

// really important extern -- screen redraws suck w/o it {dlb}
FixedVector < unsigned short, 1000 > mcolour;

static bool initialized_randmons = false;
static std::vector<monster_type> monsters_by_habitat[NUM_HABITATS];

static monsterentry mondata[] = {
#include "mon-data.h"
};

#define MONDATASIZE ARRAYSIZE(mondata)

static mon_spellbook mspell_list[] = {
#include "mon-spll.h"
};

static int mons_exp_mod(int mclass);

// macro that saves some typing, nothing more
#define smc get_monster_data(mc)

/* ******************** BEGIN PUBLIC FUNCTIONS ******************** */

habitat_type grid2habitat(dungeon_feature_type grid)
{
    if (grid_is_watery(grid))
        return (HT_WATER);

    if (grid_is_rock(grid) && !grid_is_permarock(grid))
        return (HT_ROCK);

    switch (grid)
    {
    case DNGN_LAVA:
        return (HT_LAVA);
    case DNGN_FLOOR:
    default:
        return (HT_LAND);
    }
}

dungeon_feature_type habitat2grid(habitat_type ht)
{
    switch (ht)
    {
    case HT_WATER:
        return (DNGN_DEEP_WATER);
    case HT_LAVA:
        return (DNGN_LAVA);
    case HT_ROCK:
        return (DNGN_ROCK_WALL);
    case HT_LAND:
    default:
        return (DNGN_FLOOR);
    }
}

static void initialize_randmons()
{
    for (int i = 0; i < NUM_HABITATS; ++i)
    {
        const dungeon_feature_type grid = habitat2grid( habitat_type(i) );

        for (int m = 0; m < NUM_MONSTERS; ++m)
        {
            if (invalid_monster_class(m))
                continue;
            if (monster_habitable_grid(m, grid))
                monsters_by_habitat[i].push_back(static_cast<monster_type>(m));
        }
    }
    initialized_randmons = true;
}

monster_type random_monster_at_grid(int x, int y)
{
    return (random_monster_at_grid(grd[x][y]));
}

monster_type random_monster_at_grid(dungeon_feature_type grid)
{
    if (!initialized_randmons)
        initialize_randmons();

    const habitat_type ht = grid2habitat(grid);
    const std::vector<monster_type> &valid_mons = monsters_by_habitat[ht];
    ASSERT(!valid_mons.empty());
    return valid_mons.empty()? MONS_PROGRAM_BUG
                 : valid_mons[ random2(valid_mons.size()) ];
}

typedef std::map<std::string, unsigned> mon_name_map;
static mon_name_map mon_name_cache;

void init_mon_name_cache()
{
    for (unsigned i = 0; i < sizeof(mondata) / sizeof(*mondata); ++i)
    {
        std::string name = mondata[i].name;
        lowercase(name);

        const int          mtype = mondata[i].mc;
        const monster_type mon   = monster_type(mtype);

        mon_name_cache[name] = mon;
    }
}

monster_type get_monster_by_name(std::string name, bool exact)
{
    lowercase(name);

    if (exact)
    {
        mon_name_map::iterator i = mon_name_cache.find(name);

        if (i != mon_name_cache.end())
            return static_cast<monster_type>(i->second);

        return MONS_PROGRAM_BUG;
    }

    monster_type mon = MONS_PROGRAM_BUG;
    for (unsigned i = 0; i < sizeof(mondata) / sizeof(*mondata); ++i)
    {
        std::string candidate = mondata[i].name;
        lowercase(candidate);

        const int mtype = mondata[i].mc;

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
    mon_entry.init(-1);

    // next, fill static array with location of entry in mondata[] {dlb}:
    for (x = 0; x < MONDATASIZE; x++)
        mon_entry[mondata[x].mc] = x;

    // finally, monsters yet with dummy entries point to TTTSNB(tm) {dlb}:
    for (x = 0; x < NUM_MONSTERS; x++)
        if (mon_entry[x] == -1)
            mon_entry[x] = mon_entry[MONS_PROGRAM_BUG];

    init_monster_symbols();
}                               // end mons_init()

void init_monster_symbols()
{
    for (int i = 0; i < NUM_MONSTERS; ++i)
    {
        mon_display &md = monster_symbols[i];
        const monsterentry *me = get_monster_data(i);
        if (me)
        {
            md.glyph = me->showchar;
            md.colour = me->colour;
        }
    }

    for (int i = 0, size = Options.mon_glyph_overrides.size();
         i < size; ++i)
    {
        const mon_display &md = Options.mon_glyph_overrides[i];
        if (md.type == MONS_PROGRAM_BUG)
            continue;

        if (md.glyph)
            monster_symbols[md.type].glyph = md.glyph;
        if (md.colour)
            monster_symbols[md.type].colour = md.colour;
    }
}

const mon_resist_def &get_mons_class_resists(int mc)
{
    const monsterentry *me = get_monster_data(mc);
    return (me? me->resists : get_monster_data(MONS_PROGRAM_BUG)->resists);
}

mon_resist_def get_mons_resists(const monsters *mon)
{
    if (mon->type == MONS_PLAYER_GHOST || mon->type == MONS_PANDEMONIUM_DEMON)
        return (mon->ghost->resists);
    
    mon_resist_def resists = get_mons_class_resists(mon->type);
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

monsters *monster_at(const coord_def &pos)
{
    const int mindex = mgrd(pos);
    return (mindex != NON_MONSTER? &menv[mindex] : NULL);
}

int mons_piety(const monsters *mon)
{
    if (mon->god == GOD_NO_GOD)
        return (0);

    // We're open to fine-tuning.
    return (mon->hit_dice * 14);
}

bool mons_class_flag(int mc, int bf)
{
    const monsterentry *me = smc;

    if (!me)
        return (false);

    return ((me->bitfields & bf) != 0);
}                               // end mons_class_flag()

static int scan_mon_inv_randarts( const monsters *mon,
                                  randart_prop_type ra_prop)
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

// Returns true if a given monster is an abomination
// created by Twisted Resurrection
static bool mons_your_abomination(const monsters *mon)
{
    if ( mon->type != MONS_ABOMINATION_SMALL
         && mon->type != MONS_ABOMINATION_LARGE )
    {
        return (false);
    }

    // Reusing the colour scheme - hacky! (jpeg)
    return (mon->number == BROWN || mon->number == RED
            || mon->number == LIGHTRED);
}

mon_holy_type mons_holiness(const monsters *mon)
{
    if (mons_your_abomination(mon))
        return (MH_UNDEAD);

    return (mons_class_holiness(mon->type));
}

mon_holy_type mons_class_holiness(int mc)
{
    return (smc->holiness);
}                               // end mons_holiness()

bool mons_class_is_stationary(int mc)
{
    return (mc == MONS_OKLOB_PLANT
                || mc == MONS_PLANT
                || mc == MONS_FUNGUS
                || mc == MONS_CURSE_SKULL
                || mons_is_statue(mc)
                || mons_is_mimic(mc));
}

bool mons_is_stationary(const monsters *mon)
{
    return (mons_class_is_stationary(mon->type));
}

bool mons_class_is_confusable(int mc)
{
    return (smc->resist_magic < MAG_IMMUNE
            && mons_intel(mc) > I_PLANT);
}

bool mons_class_is_slowable(int mc)
{
    return (smc->resist_magic < MAG_IMMUNE);
}

bool mons_is_wall_shielded(int mc)
{
    return (mons_habitat_by_type(mc) == HT_ROCK);
}

// returns whether a monster is non-solid
// and thus can't be affected by some traps
bool mons_is_insubstantial(int mc)
{
    return mons_class_flag(mc, M_INSUBSTANTIAL);
}

bool mons_behaviour_perceptible(const monsters *mon)
{
    return (!mons_class_flag(mon->type, M_NO_EXP_GAIN)
            && !mons_is_mimic(mon->type)
            && !mons_is_statue(mon->type)
            && mon->type != MONS_OKLOB_PLANT);
}

bool mons_is_icy(const monsters *mon)
{
    return (mons_is_icy(mon->type));
}

bool mons_is_icy(int mtype)
{
    return (mtype == MONS_ICE_BEAST
            || mtype == MONS_SIMULACRUM_SMALL
            || mtype == MONS_SIMULACRUM_LARGE
            || mtype == MONS_ICE_STATUE);
}

bool invalid_monster(const monsters *mon)
{
    return (!mon || mon->type == -1);
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
    return (mc == MONS_ORANGE_STATUE
            || mc == MONS_SILVER_STATUE
            || mc == MONS_ICE_STATUE);
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

    case 'm':   // merfolk
        if (mc == MONS_MERFOLK || mc == MONS_MERMAID)
            return (true);
        return (false);

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
    const monsterentry *me = get_monster_data(mc);
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

int get_shout_noise_level(const shout_type shout)
{
    switch (shout)
    {
    case S_SILENT:
        return 0;
    case S_HISS:
    case S_VERY_SOFT:
        return 4;
    case S_SOFT:
        return 6;
    case S_LOUD:
        return 10;
    case S_SHOUT2:
    case S_ROAR:
    case S_VERY_LOUD:
        return 12;

    default:
        return 8;
    }
}

shout_type mons_shouts(int mc)
{
    shout_type u = smc->shouts;

    if (u == S_RANDOM)
        u = static_cast<shout_type>(random2(NUM_SHOUTS));

    return (u);
}                               // end mons_shouts()

bool mons_is_unique( int mc )
{
    return (mons_class_flag(mc, M_UNIQUE));
}

bool mons_sense_invis(const monsters *mon)
{
    return (mons_class_flag(mon->type, M_SENSE_INVIS));
}

bool mons_see_invis(const monsters *mon)
{
    if (mon->type == MONS_PLAYER_GHOST || mon->type == MONS_PANDEMONIUM_DEMON)
        return (mon->ghost->see_invis);
    else if (mons_class_flag(mon->type, M_SEE_INVIS))
        return (true);
    else if (scan_mon_inv_randarts( mon, RAP_EYESIGHT ) > 0)
        return (true);

    return (false);
}                               // end mons_see_invis()


// This does NOT do line of sight!  It checks the targ's visibility 
// with respect to mon's perception, but doesn't do walls or range.
bool mons_monster_visible( const monsters *mon, const monsters *targ )
{
    if (targ->has_ench(ENCH_SUBMERGED)
        || (targ->invisible() && !mons_see_invis(mon)))
    {
        return (false);
    }

    return (true);
}

// This does NOT do line of sight!  It checks the player's visibility 
// with respect to mon's perception, but doesn't do walls or range.
bool mons_player_visible( const monsters *mon )
{
    if (you.invisible())
    {
        if (player_in_water())
            return (true);

        if (mons_see_invis( mon ) || mons_sense_invis(mon))
            return (true);

        return (false);
    }

    return (true);
}

unsigned mons_char(int mc)
{
    return monster_symbols[mc].glyph;
}

int mons_class_colour(int mc)
{
    return monster_symbols[mc].colour;
}

mon_itemuse_type mons_itemuse(int mc)
{
    return (smc->gmon_use);
}                               // end mons_itemuse()

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
            return mon_attack_def::attk(mons->ghost->damage);
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

bool mons_immune_magic(const monsters *mon)
{
    return get_monster_data(mon->type)->resist_magic == MAG_IMMUNE;
}

int mons_resist_magic( const monsters *mon )
{
    if ( mons_immune_magic(mon) )
        return MAG_IMMUNE;

    int u = (get_monster_data(mon->type))->resist_magic;

    // negative values get multiplied with mhd
    if (u < 0)
        u = mon->hit_dice * -u * 4 / 3;

    // randarts have a multiplicative effect
    u *= (scan_mon_inv_randarts( mon, RAP_MAGIC ) + 100);
    u /= 100;

    // ego armour resistance
    const int armour = mon->inv[MSLOT_ARMOUR];

    if (armour != NON_ITEM 
        && get_armour_ego_type( mitm[armour] ) == SPARM_MAGIC_RESISTANCE )
    {
        u += 30;
    }

    return (u);
}                               // end mon_resist_magic()

const char* mons_resist_string(const monsters *mon)
{
    if ( mons_immune_magic(mon) )
        return "is unaffected";
    else
        return "resists";
}

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
    mprf(MSGCH_DIAGNOSTICS,
         "Power: %d, monster's MR: %d, target: %d, roll: %d", 
         pow, mrs, mrchance, mrch2 );
#endif

    return (mrch2 < mrchance);
}                               // end check_mons_resist_magic()

int mons_res_elec( const monsters *mon )
{
    int mc = mon->type;

    /* this is a variable, not a player_xx() function, so can be above 1 */
    int u = 0;

    u += get_mons_resists(mon).elec;

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
                || get_mons_resists(mon).asphyx > 0);
}

int mons_res_acid( const monsters *mon )
{
    return get_mons_resists(mon).acid;
}

int mons_res_poison( const monsters *mon )
{
    int mc = mon->type;

    int u = get_mons_resists(mon).poison;

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

bool mons_res_sticky_flame( const monsters *mon )
{
    return (get_mons_resists(mon).sticky_flame
            || mon->has_equipped(EQ_BODY_ARMOUR, ARM_MOTTLED_DRAGON_ARMOUR));
}

int mons_res_steam( const monsters *mon )
{
    int res = get_mons_resists(mon).steam;
    if (mon->has_equipped(EQ_BODY_ARMOUR, ARM_STEAM_DRAGON_ARMOUR))
        res += 3;
    return (res + mons_res_fire(mon) / 2);
}

int mons_res_fire( const monsters *mon )
{
    int mc = mon->type;

    const mon_resist_def res = get_mons_resists(mon);
    
    int u = std::min(res.fire + res.hellfire * 3, 3);

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

    int u = get_mons_resists(mon).cold;

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

int mons_res_miasma( const monsters *mon )
{
    return (mons_holiness(mon) != MH_NATURAL 
            || mon->type == MONS_DEATH_DRAKE ? 3 : 0);
}

int mons_res_negative_energy( const monsters *mon )
{
    int mc = mon->type;

    if (mons_holiness(mon) == MH_UNDEAD 
        || mons_holiness(mon) == MH_DEMONIC
        || mons_holiness(mon) == MH_NONLIVING
        || mons_holiness(mon) == MH_PLANT
        || mon->type == MONS_SHADOW_DRAGON
        || mon->type == MONS_DEATH_DRAKE
           // TSO protects his warriors' life force
        || mon->type == MONS_DAEVA)
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

bool mons_skeleton(int mc)
{
    if (mons_zombie_size(mc) == Z_NOZOMBIE
        || mons_weight(mc) == 0 || (mons_class_flag(mc, M_NO_SKELETON)))
    {
        return false;
    }

    return true;
}                               // end mons_skeleton()

flight_type mons_class_flies(int mc)
{
    if (mons_class_flag(mc, M_FLIES))
        return (FL_FLY);

    if (mons_class_flag(mc, M_LEVITATE))
        return (FL_LEVITATE);

    return (FL_NONE);
}

flight_type mons_flies(const monsters *mon)
{
    if (mon->type == MONS_PANDEMONIUM_DEMON
        && mon->ghost->fly)
    {
        return (mon->ghost->fly);
    }

    const int type = mons_is_zombified(mon)? mons_zombie_base(mon) : mon->type;
    const flight_type ret = mons_class_flies( type );
    return (ret ? ret
            : (scan_mon_inv_randarts(mon, RAP_LEVITATE) > 0) ? FL_LEVITATE
            : FL_NONE);
}                               // end mons_flies()

bool mons_amphibious(int mc)
{
    return mons_class_flag(mc, M_AMPHIBIOUS);
}

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
    struct monsterentry *mon_class = get_monster_data( type );

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

    if (mons_is_statue(mclass))
        return (mHD * 15);

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
            case SPELL_PARALYSE:
            case SPELL_SMITING:
            case SPELL_HELLFIRE_BURST:
            case SPELL_HELLFIRE:
            case SPELL_SYMBOL_OF_TORMENT:
                diff += 25;
                break;

            case SPELL_LIGHTNING_BOLT:
            case SPELL_BOLT_OF_DRAINING:
            case SPELL_VENOM_BOLT:
            case SPELL_STICKY_FLAME:
            case SPELL_DISINTEGRATE:
            case SPELL_SUMMON_GREATER_DEMON:
            case SPELL_BANISHMENT:
            case SPELL_LEHUDIBS_CRYSTAL_SPEAR:
            case SPELL_BOLT_OF_IRON:
            case SPELL_TELEPORT_SELF:
            case SPELL_TELEPORT_OTHER:
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

void mons_load_spells( monsters *mon, mon_spellbook_type book )
{
    mon->load_spells(book);
}

void define_monster(int index)
{
    define_monster(menv[index]);
}

// generate a shiny new and unscarred monster
void define_monster(monsters &mons)
{
    int temp_rand = 0;          // probability determination {dlb}
    int mcls = mons.type;
    int hd, hp, hp_max, ac, ev, speed;
    int monnumber = mons.number;
    const monsterentry *m = get_monster_data(mcls);
    int col = mons_class_colour(mons.type);
    mon_spellbook_type spells = MST_NO_SPELLS;

    hd = m->hpdice[0];

    // misc
    ac = m->AC;
    ev = m->ev;

    speed = monsters::base_speed(mcls);

    mons.god = GOD_NO_GOD;

    switch (mcls)
    {
    case MONS_ABOMINATION_SMALL:
        hd = 4 + random2(4);
        ac = 3 + random2(7);
        ev = 7 + random2(6);

        if (monnumber == MONS_PROGRAM_BUG) // ??
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

        if (monnumber == MONS_PROGRAM_BUG)
            col = random_colour();
        break;

    case MONS_BEAST:
        hd = 4 + random2(4);
        ac = 2 + random2(5);
        ev = 7 + random2(5);
        break;

    case MONS_HYDRA:
        monnumber = random_range(4, 8);
        break;

    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_SOLDIER:
    case MONS_ORC_WIZARD:
        spells = static_cast<mon_spellbook_type>(MST_ORC_WIZARD_I+random2(3));
        break;

    case MONS_LICH:
    case MONS_ANCIENT_LICH:
        spells = static_cast<mon_spellbook_type>(MST_LICH_I + random2(4));
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
        spells = static_cast<mon_spellbook_type>(MST_WIZARD_I + random2(5));
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
        while (drac_colour_incompatible(mcls, monnumber));
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

    // so let it be written, so let it be done
    mons.hit_dice = hd;
    mons.hit_points = hp;
    mons.max_hit_points = hp_max;
    mons.ac = ac;
    mons.ev = ev;
    mons.speed = speed;
    mons.speed_increment = 70;

    if (mons.number == MONS_PROGRAM_BUG)
        mons.number = monnumber;
    
    mons.flags = 0L;
    mons.experience = 0L;
    mons.colour = col;
    
    mons_load_spells( &mons, spells );

    // reset monster enchantments
    mons.enchantments.clear();
    mons.ench_countdown = 0;
}                               // end define_monster()

static const char *drac_colour_names[] = {
    "black", "mottled", "yellow", "green", "purple",
    "red", "white", "pale"
};

std::string draconian_colour_name(monster_type mtype)
{
    ASSERT(ARRAYSIZE(drac_colour_names) ==
           MONS_PALE_DRACONIAN - MONS_DRACONIAN);
    if (mtype < MONS_BLACK_DRACONIAN || mtype > MONS_PALE_DRACONIAN)
        return "buggy";
    return (drac_colour_names[mtype - MONS_BLACK_DRACONIAN]);
}

monster_type draconian_colour_by_name(const std::string &name)
{
    ASSERT(ARRAYSIZE(drac_colour_names) ==
           MONS_PALE_DRACONIAN - MONS_DRACONIAN);
    for (unsigned i = 0; i < ARRAYSIZE(drac_colour_names); ++i)
        if (name == drac_colour_names[i])
            return static_cast<monster_type>(i + MONS_BLACK_DRACONIAN);
    return (MONS_PROGRAM_BUG);
}

static std::string str_monam(const monsters& mon, description_level_type desc,
                             bool force_seen)
{
    if (desc == DESC_NONE)
        return ("");
    
    // Handle non-visible case first
    if ( !force_seen && !player_monster_visible(&mon) )
    {       
        switch (desc)
        {
        case DESC_CAP_THE: case DESC_CAP_A:
            return "It";
        case DESC_NOCAP_THE: case DESC_NOCAP_A: case DESC_PLAIN:
            return "it";
        default:
            return "it (buggy)";
        }
    }

    // Assumed visible from now on

    // Various special cases:
    // non-gold mimics, dancing weapons, ghosts, Pan demons
    if ( mons_is_mimic(mon.type) && mon.type != MONS_GOLD_MIMIC )
    {
        item_def item;
        get_mimic_item( &mon, item );
        return item.name(desc);
    }

    if (mon.type == MONS_DANCING_WEAPON && mon.inv[MSLOT_WEAPON] != NON_ITEM)
    {
        item_def item = mitm[mon.inv[MSLOT_WEAPON]];
        unset_ident_flags( item, ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES );
        return item.name(desc);
    }

    if (mon.type == MONS_PLAYER_GHOST)
        return mon.ghost->name + "'s ghost";

    if (mon.type == MONS_PANDEMONIUM_DEMON)
        return mon.ghost->name;

    std::string result;

    // Start building the name string.

    // Start with the prefix.
    // (Uniques don't get this, because their names are proper nouns.)
    if ( !mons_is_unique(mon.type) )
    {
        switch (desc)
        {
        case DESC_CAP_THE:   result = "The "; break;
        case DESC_NOCAP_THE: result = "the "; break;
        case DESC_CAP_A:     result = "A ";   break;
        case DESC_NOCAP_A:   result = "a ";   break;
        case DESC_PLAIN: default:             break;
        }
    }

    // Some monsters might want the name of a different creature.
    int nametype = mon.type;

    // Tack on other prefixes.
    switch (mon.type)
    {
    case MONS_SPECTRAL_THING: 
        result += "spectral ";
        nametype = mon.number;
        break;

    case MONS_ZOMBIE_SMALL:     case MONS_ZOMBIE_LARGE:
    case MONS_SKELETON_SMALL:   case MONS_SKELETON_LARGE:
    case MONS_SIMULACRUM_SMALL: case MONS_SIMULACRUM_LARGE:
        nametype = mon.number;
        break;

    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_DRACONIAN_SCORCHER:
        result +=
            draconian_colour_name(
                static_cast<monster_type>( mon.number ) ) + " ";
        break;
        
    default:
        break;
    }

    // Add the base name.
    result += get_monster_data(nametype)->name;

    // Add suffixes.
    switch (mon.type)
    {
    case MONS_ZOMBIE_SMALL: case MONS_ZOMBIE_LARGE:
        result += " zombie"; break;
    case MONS_SKELETON_SMALL: case MONS_SKELETON_LARGE:
        result += " skeleton"; break;
    case MONS_SIMULACRUM_SMALL: case MONS_SIMULACRUM_LARGE:
        result += " simulacrum"; break;
    default: break;
    }

    // Vowel fix: Change 'a orc' to 'an orc'
    if ( result.length() >= 3 &&
         (result[0] == 'a' || result[0] == 'A') &&
         result[1] == ' ' &&
         is_vowel(result[2]) )
    {
        result.insert(1, "n");
    }

    // All done.
    return result;
}

std::string mons_type_name(int type, description_level_type desc )
{
    std::string result;
    if ( !mons_is_unique(type) )
    {
        switch (desc)
        {
        case DESC_CAP_THE:   result = "The "; break;
        case DESC_NOCAP_THE: result = "the "; break;
        case DESC_CAP_A:     result = "A ";   break;
        case DESC_NOCAP_A:   result = "a ";   break;
        case DESC_PLAIN: default:             break;
        }
    }

    result += get_monster_data(type)->name;

    // Vowel fix: Change 'a orc' to 'an orc'
    if ( result.length() >= 3 &&
         (result[0] == 'a' || result[0] == 'A') &&
         result[1] == ' ' &&
         is_vowel(result[2]) )
    {
        result.insert(1, "n");
    }
    return result;
}

/* ********************* END PUBLIC FUNCTIONS ********************* */

// see mons_init for initialization of mon_entry array.
monsterentry *get_monster_data(int p_monsterid)
{
    const int me =
        p_monsterid != -1 && p_monsterid < NUM_MONSTERS?
        mon_entry[p_monsterid] : -1;

    if (me >= 0)                // PARANOIA
        return (&mondata[me]);
    else
        return (NULL);
}                               // end get_monster_data()

static int mons_exp_mod(int mc)
{
    return (smc->exp_mod);
}                               // end mons_exp_mod()


int mons_speed(int mc)
{
    return (smc->speed);
}                               // end mons_speed()


mon_intel_type mons_intel(int mc)
{
    return (smc->intel);
}

habitat_type mons_habitat_by_type(int mc)
{
    const monsterentry *me = get_monster_data(mc);
    return (me? me->habitat : HT_LAND);
}

habitat_type mons_habitat(const monsters *m)
{
    return mons_habitat_by_type(
        mons_is_zombified(m)? mons_zombie_base(m) : m->type );
}

bool intelligent_ally(const monsters *monster)
{
    return (monster->attitude == ATT_FRIENDLY
            && mons_intel(monster->type) >= I_NORMAL);
}


int mons_power(int mc)
{
    // for now, just return monster hit dice.
    return (smc->hpdice[0]);
}

bool mons_aligned(int m1, int m2)
{
    mon_attitude_type fr1, fr2;
    struct monsters *mon1, *mon2;

    if (m1 == MHITNOT || m2 == MHITNOT)
        return (true);

    if (m1 == MHITYOU)
        fr1 = ATT_FRIENDLY;
    else
    {
        mon1 = &menv[m1];
        fr1 = mons_attitude(mon1);
    }

    if (m2 == MHITYOU)
        fr2 = ATT_FRIENDLY;
    else
    {
        mon2 = &menv[m2];
        fr2 = mons_attitude(mon2);
    }

    return (fr1 == fr2);
}

bool mons_wields_two_weapons(monster_type m)
{
    return mons_class_flag(m, M_TWOWEAPON);
}

bool mons_wields_two_weapons(const monsters *m)
{
    return (mons_wields_two_weapons(static_cast<monster_type>(m->type)));
}

bool mons_eats_corpses(const monsters *m)
{
    return (m->type == MONS_NECROPHAGE || m->type == MONS_GHOUL);
}

bool mons_is_summoned(const monsters *m)
{
    return (m->has_ench(ENCH_ABJ));
}

// Does not check whether the monster can dual-wield - that is the
// caller's responsibility.
int mons_offhand_weapon_index(const monsters *m)
{
    return (m->inv[MSLOT_ALT_WEAPON]);
}

int mons_base_damage_brand(const monsters *m)
{
    if (m->type == MONS_PLAYER_GHOST || m->type == MONS_PANDEMONIUM_DEMON)
        return m->ghost->brand;

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

bool mons_neutral(const monsters *m)
{
    return (m->attitude == ATT_NEUTRAL || m->has_ench(ENCH_NEUTRAL));
}

mon_attitude_type mons_attitude(const monsters *m)
{
    return (m->has_ench(ENCH_CHARM)? ATT_FRIENDLY : m->attitude);
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
    return (m->has_ench(ENCH_CONFUSION)
            && !mons_class_flag(m->type, M_CONFUSED));
}

bool mons_is_caught(const monsters *m)
{
    return (m->has_ench(ENCH_HELD));
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
    return (mons_behaviour_perceptible(m)
            && !mons_friendly(m)
            && (mons_is_sleeping(m)
                || mons_is_paralysed(m)));
}

bool mons_looks_distracted(const monsters *m)
{
    return (mons_behaviour_perceptible(m)
                && !mons_friendly(m)
                && ((m->foe != MHITYOU && !mons_is_batty(m))
                    || mons_is_confused(m)
                    || mons_is_fleeing(m)
                    || mons_is_caught(m)));
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

bool ms_requires_tracer(spell_type monspell)
{
    return (spell_needs_tracer(monspell));
}

// returns true if the spell is something you wouldn't want done if
// you had a friendly target..  only returns a meaningful value for
// non-beam spells

bool ms_direct_nasty(spell_type monspell)
{
    return (spell_needs_foe(monspell)
            && !spell_typematch(monspell, SPTYP_SUMMONING));
}

// Spells a monster may want to cast if fleeing from the player, and 
// the player is not in sight.
bool ms_useful_fleeing_out_of_sight( const monsters *mon, spell_type monspell )
{
    if (ms_waste_of_time( mon, monspell ))
        return (false);

    switch (monspell)
    {
    case SPELL_HASTE:
    case SPELL_INVISIBILITY:
    case SPELL_LESSER_HEALING:
    case SPELL_ANIMATE_DEAD:
        return (true);

    default:
        if (spell_typematch(monspell, SPTYP_SUMMONING) && one_chance_in(4))
            return (true);
        break;
    }

    return (false);
}

bool ms_low_hitpoint_cast( const monsters *mon, spell_type monspell )
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
    case SPELL_TELEPORT_SELF:
    case SPELL_TELEPORT_OTHER:
    case SPELL_LESSER_HEALING:
        ret = true;
        break;

    case SPELL_BLINK:
    case SPELL_BLINK_OTHER:
        if (targ_adj)
            ret = true;
        break;

    case SPELL_NO_SPELL:
        ret = false;
        break;

    default:
        if (!targ_adj && spell_typematch(monspell, SPTYP_SUMMONING))
            ret = true;
        break;
    }

    return (ret);
}

// spells for a quick get-away
// currently only used to get out of a net
bool ms_quick_get_away( const monsters *mon /*unused*/, spell_type monspell )
{
    switch (monspell)
    {
      case SPELL_TELEPORT_SELF:
      case SPELL_BLINK:
        return true;
      default:
        return false;
    }
}

// Checks to see if a particular spell is worth casting in the first place.
bool ms_waste_of_time( const monsters *mon, spell_type monspell )
{
    bool ret = false;
    int intel, est_magic_resist, power, diff;

    const actor *foe = mon->get_foe();

    // Keep friendly summoners from spamming summons constantly.
    if (mons_friendly(mon)
        && (!foe || foe == &you)
        && spell_typematch(monspell, SPTYP_SUMMONING))
    {
        return (true);
    }
    
    // Eventually, we'll probably want to be able to have monsters 
    // learn which of their elemental bolts were resisted and have those 
    // handled here as well. -- bwr
    switch (monspell)
    {
    case SPELL_BACKLIGHT:
    {
        ret = !foe || foe->backlit();
        break;
    }

    case SPELL_BERSERKER_RAGE:
        if (!mon->needs_berserk(false))
            ret = true;
        break;
        
    case SPELL_HASTE:
        if (mon->has_ench(ENCH_HASTE))
            ret = true;
        break;

    case SPELL_INVISIBILITY:
        if (mon->has_ench(ENCH_INVIS) ||
            (mons_friendly(mon) && !player_see_invis(false)))
            ret = true;
        break;

    case SPELL_LESSER_HEALING:
        if (mon->hit_points > mon->max_hit_points / 2)
            ret = true;
        break;

    case SPELL_TELEPORT_SELF:
        // Monsters aren't smart enough to know when to cancel teleport.
        if (mon->has_ench( ENCH_TP ))
            ret = true;
        break;

    case SPELL_TELEPORT_OTHER:
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

    case SPELL_SLOW:
    case SPELL_CONFUSE:
    case SPELL_PAIN:
    case SPELL_BANISHMENT:
    case SPELL_DISINTEGRATE:
    case SPELL_PARALYSE:
    case SPELL_SLEEP:
        if (monspell == SPELL_SLEEP && (!foe || foe->asleep()))
            return (true);
        
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
                est_magic_resist = mons_resist_magic(&menv[mon->foe]);

            // now randomize (normal intels less accurate than high):
            if (intel == I_NORMAL)
                est_magic_resist += random2(80) - 40;  
            else
                est_magic_resist += random2(30) - 15;  
        }

        power = 12 * mon->hit_dice * (monspell == SPELL_PAIN ? 2 : 1);
        power = stepdown_value( power, 30, 40, 100, 120 );

        // Determine the amount of chance allowed by the benefit from
        // the spell.  The estimated difficulty is the probability
        // of rolling over 100 + diff on 2d100. -- bwr
        diff = (monspell == SPELL_PAIN 
                || monspell == SPELL_SLOW 
                || monspell == SPELL_CONFUSE) ? 0 : 50;

        if (est_magic_resist - power > diff)
            ret = true;

        break;

    default:
        break;
    }

    return (ret);    
}

static bool ms_ranged_spell( spell_type monspell )
{
    switch (monspell)
    {
    case SPELL_HASTE:
    case SPELL_LESSER_HEALING:
    case SPELL_TELEPORT_SELF:
    case SPELL_INVISIBILITY:
    case SPELL_BLINK:
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
            if (mon->spells[i] != SPELL_NO_SPELL)
                return (true);
        }
    }
    return (false);
}

bool mons_has_ranged_spell( const monsters *mon )
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

bool mons_has_ranged_attack( const monsters *mon )
{
    // Ugh.
    monsters *mnc = const_cast<monsters*>(mon);
    const item_def *weapon  = mnc->launcher();
    const item_def *primary = mnc->mslot_item(MSLOT_WEAPON);
    const item_def *missile = mnc->missiles();

    if (!missile && weapon != primary && primary
        && get_weapon_brand(*primary) == SPWPN_RETURNING)
    {
        return (true);
    }

    if (!missile)
        return (false);
    
    return is_launched(mnc, weapon, *missile);
}


// use of variant:
// 0 : She is tap dancing.
// 1 : It seems she is tap dancing. (lower case pronoun)
// 2 : Her sword explodes!          (upper case possessive)
// 3 : It sticks to her sword!      (lower case possessive)
// ... as needed

const char *mons_pronoun(monster_type mon_type, pronoun_type variant)
{
    gender_type gender = GENDER_NEUTER;

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
            case MONS_MERMAID:
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
        if (hspell_pass[i] == SPELL_SYMBOL_OF_TORMENT
            || hspell_pass[i] == SPELL_SMITING
            || hspell_pass[i] == SPELL_HELLFIRE_BURST)
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
    const monsterentry *me = get_monster_data(m->type);
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
    const monsterentry *me1 = get_monster_data(m1->type),
                       *me2 = get_monster_data(m2->type);
    
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
      target_x(0), target_y(0), inv(), spells(), attitude(ATT_HOSTILE),
      behaviour(BEH_WANDER), foe(MHITYOU), enchantments(), flags(0L),
      experience(0), number(0), colour(BLACK), foe_memory(0), god(GOD_NO_GOD),
      ghost(), seen_context("")
{
}

// Empty destructor to keep auto_ptr happy with incomplete ghost_demon type.
monsters::~monsters()
{
}

monsters::monsters(const monsters &mon)
{
    init_with(mon);
}

monsters &monsters::operator = (const monsters &mon)
{
    if (this != &mon)
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
    experience        = mon.experience;
    number            = mon.number;
    colour            = mon.colour;
    foe_memory        = mon.foe_memory;
    god               = mon.god;
    
    if (mon.ghost.get())
        ghost.reset(new ghost_demon( *mon.ghost ));
    else
        ghost.reset(NULL);
}

coord_def monsters::pos() const
{
    return coord_def(x, y);
}

coord_def monsters::target_pos() const
{
    return coord_def(target_x, target_y);
}

bool monsters::swimming() const
{
    const dungeon_feature_type grid = grd[x][y];
    return (grid_is_watery(grid) && mons_habitat(this) == HT_WATER);
}

bool monsters::submerged() const
{
    return (mons_is_submerged(this));
}

bool monsters::floundering() const
{
    const dungeon_feature_type grid = grd[x][y];
    return (grid_is_water(grid)
            // Can't use monster_habitable_grid because that'll return true
            // for non-water monsters in shallow water.
            && mons_habitat(this) != HT_WATER
            && !mons_amphibious(type)
            && !mons_flies(this));
}

bool mons_class_can_pass(const int mclass, const dungeon_feature_type grid)
{
    if (mons_is_wall_shielded(mclass))
    {
        // Permanent walls can't be passed through.
        return (!grid_is_solid(grid) ||
                   (grid_is_rock(grid) && !grid_is_permarock(grid)));
    }

    return !grid_is_solid(grid);
}

bool monsters::can_pass_through_feat(dungeon_feature_type grid) const
{
    return mons_class_can_pass(type, grid);
}

bool monsters::can_drown() const
{
    // Mummies can fall apart in water; ghouls and demons can drown in
    // water/lava.
    return (!mons_res_asphyx(this)
            || mons_genus(type) == MONS_MUMMY
            || mons_genus(type) == MONS_GHOUL
            || holiness() == MH_DEMONIC);
}

size_type monsters::body_size(int /* psize */, bool /* base */) const
{
    const monsterentry *e = get_monster_data(type);
    return (e? e->size : SIZE_MEDIUM);
}

int monsters::body_weight() const
{
    int mclass = type;

    switch(mclass)
    {
    case MONS_SPECTRAL_THING:
    case MONS_SPECTRAL_WARRIOR:
    case MONS_ELECTRIC_GOLEM:
    case MONS_RAKSHASA_FAKE:
        return 0;

    case MONS_ZOMBIE_SMALL:
    case MONS_ZOMBIE_LARGE:
    case MONS_SKELETON_SMALL:
    case MONS_SKELETON_LARGE:
    case MONS_SIMULACRUM_SMALL:
    case MONS_SIMULACRUM_LARGE:
        mclass = number;
        break;
    default:
        break;
    }

    int weight = mons_weight(mclass);

    // Water elementals are "insubstantial", but still have weight.
    if (weight == 0 && type == MONS_WATER_ELEMENTAL)
        weight = 1500;

    // weight == 0 in the monster entry indicates "no corpse".  Can't
    // use CE_NOCORPSE, because the corpse-effect field is used for
    // corpseless monsters to indicate what happens if their blood
    // is sucked.  Grrrr.
    if (weight == 0 && !mons_is_insubstantial(type))
    {
        const monsterentry *entry = get_monster_data(mclass);
        switch(entry->size)
        {
        case SIZE_TINY:
            weight = 150;
            break;
        case SIZE_LITTLE:
            weight = 300;
            break;
        case SIZE_SMALL:
            weight = 425;
            break;
        case SIZE_MEDIUM:
            weight = 550;
            break;
        case SIZE_LARGE:
            weight = 1300;
            break;
        case SIZE_BIG:
            weight = 1500;
            break;
        case SIZE_GIANT:
            weight = 1800;
            break;
        case SIZE_HUGE:
            weight = 2200;
            break;
        default:
            mpr("ERROR: invalid monster body weight");
            perror("monsters::body_weight(): invalid monster body weight");
            end(0);
        }

        switch(mclass)
        {
        case MONS_IRON_DEVIL:
            weight += 550;
            break;

        case MONS_STONE_GOLEM:
        case MONS_EARTH_ELEMENTAL:
        case MONS_CRYSTAL_GOLEM:
            weight *= 2;
            break;

        case MONS_IRON_DRAGON:
        case MONS_IRON_GOLEM:
            weight *= 3;
            break;

        case MONS_QUICKSILVER_DRAGON:
        case MONS_SILVER_STATUE:
            weight *= 4;
            break;

        case MONS_WOOD_GOLEM:
            weight *= 2;
            weight /= 3;
            break;

        case MONS_FLYING_SKULL:
        case MONS_CURSE_SKULL:
        case MONS_SKELETAL_DRAGON:
        case MONS_SKELETAL_WARRIOR:
            weight /= 2;
            break;

        case MONS_SHADOW_FIEND:
        case MONS_SHADOW_IMP:
        case MONS_SHADOW_DEMON:
            weight /= 3;
            break;
        }

        switch(monster_symbols[mclass].glyph)
        {
        case 'L':
            weight /= 2;
            break;

        case 'p':
            weight = 0;
            break;
        }
    }

    if (type == MONS_SKELETON_SMALL || type == MONS_SKELETON_LARGE)
        weight /= 2;

    return (weight);
}

int monsters::total_weight() const
{
    int burden = 0;

    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
        if (inv[i] != NON_ITEM)
            burden += item_mass(mitm[inv[i]]) * mitm[inv[i]].quantity;

    return (body_weight() + burden);
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

item_def *monsters::missiles()
{
    return (inv[MSLOT_MISSILE] != NON_ITEM? &mitm[inv[MSLOT_MISSILE]] : NULL);
}

int monsters::missile_count()
{
    if (const item_def *missile = missiles())
        return (missile->quantity);
    return (0);
}

item_def *monsters::launcher()
{
    item_def *weap = mslot_item(MSLOT_WEAPON);
    if (weap && is_range_weapon(*weap))
        return (weap);
    weap = mslot_item(MSLOT_ALT_WEAPON);
    return (weap && is_range_weapon(*weap)? weap : NULL);
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


bool monsters::can_throw_rocks() const
{
    return (type == MONS_STONE_GIANT || type == MONS_CYCLOPS
            || type == MONS_OGRE || type == MONS_POLYPHEMUS);
}

bool monsters::has_spell_of_type(unsigned disciplines) const
{
    for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
    {
        if (spells[i] == SPELL_NO_SPELL)
            continue;

        if (spell_typematch(spells[i], disciplines))
            return (true);
    }
    return (false);
}

bool monsters::can_use_missile(const item_def &item) const
{
    // Pretty simplistic at the moment. We allow monsters to pick up
    // missiles without the corresponding launcher, assuming that sufficient
    // wandering may get them to stumble upon the launcher.

    // Prevent monsters that have conjurations / summonings from
    // grabbing missiles.
    if (has_spell_of_type(SPTYP_CONJURATION | SPTYP_SUMMONING))
        return (false);

    // Blademasters don't want to throw stuff.
    if (type == MONS_DEEP_ELF_BLADEMASTER)
        return (false);
    
    if (item.base_type == OBJ_WEAPONS)
        return (is_throwable(item));

    if (item.base_type != OBJ_MISSILES)
        return (false);

    if (item.sub_type == MI_THROWING_NET && body_size(PSIZE_BODY) < SIZE_MEDIUM)
        return (false);

    if (item.sub_type == MI_LARGE_ROCK && !can_throw_rocks())
        return (false);

    return (true);
}

void monsters::swap_slots(mon_inv_type a, mon_inv_type b)
{
    const int swap = inv[a];
    inv[a] = inv[b];
    inv[b] = swap;
}

void monsters::equip_weapon(item_def &item, int near)
{
    if (need_message(near))
        mprf("%s wields %s.", name(DESC_CAP_THE).c_str(),
             item.name(DESC_NOCAP_A, false, false, true,
                       false, ISFLAG_CURSED).c_str());

    const int brand = get_weapon_brand(item);
    if (brand == SPWPN_PROTECTION)
        ac += 5;

    if (brand != SPWPN_NORMAL && need_message(near))
    {
        bool message_given = true;
        switch (brand)
        {
        case SPWPN_FLAMING:
            mpr("It bursts into flame!");
            break;
        case SPWPN_FREEZING:
            mpr("It glows with a cold blue light!");
            break;
        case SPWPN_HOLY_WRATH:
            mpr("It softly glows with a divine radiance!");
            break;
        case SPWPN_ELECTROCUTION:
            mpr("You hear the crackle of electricity.");
            break;
        case SPWPN_VENOM:
            mpr("It begins to drip with poison!");
            break;
        case SPWPN_DRAINING:
            mpr("You sense an unholy aura.");
            break;
        case SPWPN_FLAME:
            mpr("It glows red for a moment.");
            break;
        case SPWPN_FROST:
            mpr("It is covered in frost.");
            break;
        case SPWPN_RETURNING:
            mpr("It wiggles slightly.");
            break;
        case SPWPN_DISTORTION:
            mpr("Its appearance distorts for a moment.");
            break;
        default:
            message_given = false;
        }
        if (message_given)
            set_ident_flags(item, ISFLAG_KNOW_TYPE);
    }    
}

void monsters::equip_armour(item_def &item, int near)
{
    if (need_message(near))
        mprf("%s wears %s.", name(DESC_CAP_THE).c_str(),
             item.name(DESC_NOCAP_A).c_str());

    const equipment_type eq = get_armour_slot(item);
    if (eq != EQ_SHIELD)
    {
        ac += property( item, PARM_AC );

        const int armour_plus = item.plus;
        ASSERT(abs(armour_plus) < 20);
        if (abs(armour_plus) < 20)
            ac += armour_plus;
    }

    // Shields can affect evasion.
    ev += property( item, PARM_EVASION ) / 2;
    if (ev < 1)
        ev = 1;   // This *shouldn't* happen.
}

void monsters::equip(item_def &item, int slot, int near)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        equip_weapon(item, near);
        break;
    case OBJ_ARMOUR:
        equip_armour(item, near);
        break;
    default:
        break;
    }
}

void monsters::unequip_weapon(item_def &item, int near)
{
    if (need_message(near))
        mprf("%s unwields %s.", name(DESC_CAP_THE).c_str(),
             item.name(DESC_NOCAP_A, false, false, true,
                       false, ISFLAG_CURSED).c_str());

    const int brand = get_weapon_brand(item);
    if (brand == SPWPN_PROTECTION)
        ac -= 5;

    if (brand != SPWPN_NORMAL && need_message(near))
    {
        bool message_given = true;
        switch (brand)
        {
        case SPWPN_FLAMING:
            mpr("It stops flaming.");
            break;

        case SPWPN_HOLY_WRATH:
            mpr("It stops glowing.");
            break;

        case SPWPN_ELECTROCUTION:
            mpr("It stops crackling.");
            break;

        case SPWPN_VENOM:
            mpr("It stops dripping with poison.");
            break;

        case SPWPN_DISTORTION:
            mpr("Its appearance distorts for a moment.");
            break;

        default:
            message_given = false;
        }
        if (message_given)
            set_ident_flags(item, ISFLAG_KNOW_TYPE);
    }    
}

void monsters::unequip_armour(item_def &item, int near)
{
    if (need_message(near))
        mprf("%s takes off %s.", name(DESC_CAP_THE).c_str(),
             item.name(DESC_NOCAP_A).c_str());

    const equipment_type eq = get_armour_slot(item);
    if (eq != EQ_SHIELD)
    {
        ac -= property( item, PARM_AC );

        const int armour_plus = item.plus;
        ASSERT(abs(armour_plus) < 20);
        if (abs(armour_plus) < 20) 
            ac -= armour_plus;
    }
    
    ev -= property( item, PARM_EVASION ) / 2;
    if (ev < 1)
        ev = 1;   // This *shouldn't* happen.
}

bool monsters::unequip(item_def &item, int slot, int near, bool force)
{
    if (item.cursed() && !force)
        return false;

    if (!force && mons_near(this) && player_monster_visible(this))
        set_ident_flags(item, ISFLAG_KNOW_CURSE);

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        unequip_weapon(item, near);
        break;

    // Armour swapping not implemented yet, but armour can be
    // removed if wizard forces monster to wear a new piece of
    // armour with the "give item" wizard command.
    case OBJ_ARMOUR:
        unequip_armour(item, near);
        break;

    default:
        break;
    }

    return true;
}

void monsters::lose_pickup_energy()
{
    if (const monsterentry* entry = find_monsterentry())
    {
        const int delta = speed * entry->energy_usage.pickup_percent / 100;
        if (speed_increment > 25 && delta < speed_increment)
            speed_increment -= delta;
    }
}

void monsters::pickup_message(const item_def &item, int near)
{
    if (need_message(near))
        mprf("%s picks up %s.",
             name(DESC_CAP_THE).c_str(),
             item.base_type == OBJ_GOLD? "some gold"
             : item.name(DESC_NOCAP_A).c_str());
}

bool monsters::pickup(item_def &item, int slot, int near, bool force_merge)
{
    if (inv[slot] != NON_ITEM)
    {
        if (items_stack(item, mitm[inv[slot]], force_merge))
        {
            dungeon_events.fire_position_event(
                dgn_event(DET_ITEM_PICKUP, pos(), 0, item.index(),
                          monster_index(this)),
                pos());

            pickup_message(item, near);
            inc_mitm_item_quantity( inv[slot], item.quantity );
            destroy_item(item.index());
            equip(item, slot, near);
            lose_pickup_energy();
            return (true);
        }
        return (false);
    }

    dungeon_events.fire_position_event(
        dgn_event(DET_ITEM_PICKUP, pos(), 0, item.index(),
                  monster_index(this)),
        pos());
    
    const int index = item.index();
    unlink_item(index);
    inv[slot] = index;

    pickup_message(item, near);
    equip(item, slot, near);
    lose_pickup_energy();
    return (true);
}

bool monsters::drop_item(int eslot, int near)
{
    if (eslot < 0 || eslot >= NUM_MONSTER_SLOTS)
        return (false);

    int index = inv[eslot];
    if (index == NON_ITEM)
        return (true);

    // Unequip equipped items before dropping them; unequip() prevents
    // cursed items from being removed.
    bool was_unequipped = false;
    if (eslot == MSLOT_WEAPON || eslot == MSLOT_ARMOUR
        || (eslot == MSLOT_ALT_WEAPON && mons_wields_two_weapons(this) ))
    {
        if (!unequip(mitm[index], eslot, near))
            return (false);
        was_unequipped = true;
    }

    const std::string iname = mitm[index].name(DESC_NOCAP_A);
    if (!move_item_to_grid(&index, x, y))
    {
        // Re-equip item if we somehow failed to drop it.
        if (was_unequipped)
            equip(mitm[index], eslot, near);

        return (false);
    }

    if (need_message(near))
        mprf("%s drops %s.", name(DESC_CAP_THE).c_str(), iname.c_str());

    inv[eslot] = NON_ITEM;
    return (true);
}

bool monsters::pickup_launcher(item_def &launch, int near)
{
    const int mdam_rating = mons_weapon_damage_rating(launch);
    const missile_type mt = fires_ammo_type(launch);
    int eslot = -1;
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        if (const item_def *elaunch = mslot_item(static_cast<mon_inv_type>(i)))
        {
            if (!is_range_weapon(*elaunch))
                continue;
            
            return (fires_ammo_type(*elaunch) == mt
                    && mons_weapon_damage_rating(*elaunch) < mdam_rating
                    && drop_item(i, near) && pickup(launch, i, near));
        }
        else
            eslot = i;
    }

    return (eslot == -1? false : pickup(launch, eslot, near));
}

bool monsters::pickup_melee_weapon(item_def &item, int near)
{
    if (mons_wields_two_weapons(this))
    {
        // If we have either weapon slot free, pick up the weapon.
        if (inv[MSLOT_WEAPON] == NON_ITEM)
            return pickup(item, MSLOT_WEAPON, near);
        else if (inv[MSLOT_ALT_WEAPON] == NON_ITEM)
            return pickup(item, MSLOT_ALT_WEAPON, near);
    }

    const int mdam_rating = mons_weapon_damage_rating(item);
    int eslot = -1;
    bool has_melee = false;
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        if (const item_def *weap = mslot_item(static_cast<mon_inv_type>(i)))
        {
            if (is_range_weapon(*weap))
                continue;
            has_melee = true;
            if (mons_weapon_damage_rating(*weap) < mdam_rating)
                return (drop_item(i, near) && pickup(item, i, near));
        }
        else
            eslot = i;
    }

    if (eslot == MSLOT_ALT_WEAPON && inv[MSLOT_WEAPON] == NON_ITEM)
        eslot = MSLOT_WEAPON;

    return (eslot == -1 || has_melee? false : pickup(item, eslot, near));
}

// Arbitrary damage adjustment for quantity of missiles. So sue me.
static int q_adj_damage(int damage, int qty)
{
    return (damage * std::min(qty, 8));
}
    
bool monsters::pickup_throwable_weapon(item_def &item, int near)
{
    if (mslot_item(MSLOT_MISSILE) && pickup(item, MSLOT_MISSILE, near, true))
        return (true);
    
    item_def *launch = NULL;
    const int exist_missile = mons_pick_best_missile(this, &launch, true);
    if (exist_missile == NON_ITEM
        || (q_adj_damage(mons_missile_damage(launch, &mitm[exist_missile]),
                          mitm[exist_missile].quantity)
            <
            q_adj_damage(mons_thrown_weapon_damage(&item), item.quantity)))
    {
        if (inv[MSLOT_MISSILE] != NON_ITEM && !drop_item(MSLOT_MISSILE, near))
            return (false);
        return pickup(item, MSLOT_MISSILE, near);
    }
    return (false);
}

bool monsters::wants_weapon(const item_def &weap) const
{
    if (is_fixed_artefact( weap ))
        return (false);

    // Blademasters and master archers like their starting weapon and
    // don't want another, thank you.
    if (type == MONS_DEEP_ELF_BLADEMASTER
        || type == MONS_DEEP_ELF_MASTER_ARCHER)
    {
        return (false);
    }
    
    // XXX: Make this check dependent on creature size.
    // wimpy monsters (Kob, gob) shouldn't pick up halberds etc
    // of course, this also block knives {dlb}:
    if ((::mons_species(type) == MONS_KOBOLD
         || ::mons_species(type) == MONS_GOBLIN)
        && property( weap, PWPN_HIT ) <= 0)
    {
        return (false);
    }

    // Nobody picks up giant clubs:
    if (weap.sub_type == WPN_GIANT_CLUB
        || weap.sub_type == WPN_GIANT_SPIKED_CLUB)
    {
        return (false);
    }

    const int brand = get_weapon_brand(weap);
    const int holy = holiness();

    if (brand == SPWPN_HOLY_WRATH
        && (holy == MH_DEMONIC || holy == MH_UNDEAD))
    {
        return (false);
    }

    return (true);    
}

bool monsters::wants_armour(const item_def &item) const
{
    // FIXME: Need monster body-size handling. For now, never attempt to
    // change armour.
    return (!mslot_item(MSLOT_ARMOUR));
}

static mon_inv_type equip_slot_to_mslot(equipment_type eq)
{
    switch (eq)
    {
    case EQ_WEAPON: return MSLOT_WEAPON;
    case EQ_BODY_ARMOUR: return MSLOT_ARMOUR;
    case EQ_SHIELD: return MSLOT_SHIELD;
    default: return (NUM_MONSTER_SLOTS);
    }
}

bool monsters::pickup_armour(item_def &item, int near, bool force)
{
    ASSERT(item.base_type == OBJ_ARMOUR);
    
    if (!force && !wants_armour(item))
        return (false);

    const equipment_type eq = get_armour_slot(item);
    // XXX: Monsters can only equip body armour and shields (as of 0.4).
    // They can still be forced to wear stuff - this is needed for bardings.
    if (!force && eq != EQ_BODY_ARMOUR && eq != EQ_SHIELD)
        return (false);

    const mon_inv_type mslot = equip_slot_to_mslot(eq);
    if (mslot == NUM_MONSTER_SLOTS)
        return false;

    // XXX: Very simplistic armour evaluation for the moment.
    if (const item_def *existing_armour = slot_item(eq))
    {
        if (!force && existing_armour->armour_rating() >= item.armour_rating())
            return (false);

        if (!drop_item(mslot, near))
            return (false);
    }

    return pickup(item, mslot, near);
}

bool monsters::pickup_weapon(item_def &item, int near, bool force)
{
    if (!force && !wants_weapon(item))
        return (false);
    
    // Weapon pickup involves:
    // - If we have no weapons, always pick this up.
    // - If this is a melee weapon and we already have a melee weapon, pick
    //   it up if it is superior to the one we're carrying (and drop the
    //   one we have).
    // - If it is a ranged weapon, and we already have a ranged weapon,
    //   pick it up if it is better than the one we have.
    // - If it is a throwable weapon, and we're carrying no missiles (or our
    //   missiles are the same type), pick it up.

    if (is_range_weapon(item))
        return (pickup_launcher(item, near));
    
    if (pickup_melee_weapon(item, near))
        return (true);

    return (can_use_missile(item) && pickup_throwable_weapon(item, near));
}

bool monsters::pickup_missile(item_def &item, int near, bool force)
{
    // XXX: Missile pickup could get a lot smarter if we allow monsters to
    // drop their existing missiles and pick up new stuff, but that's too
    // much work for now.
    
    const item_def *miss = missiles();
    
    // monster may not pick up trapping net
    if (mons_is_caught(this) && item.sub_type == MI_THROWING_NET
        && item_is_stationary(item))
    {
        return (false);
    }
    
    if (miss && items_stack(*miss, item))
        return (pickup(item, MSLOT_MISSILE, near));

    if (!force && !can_use_missile(item))
        return (false);

    return pickup(item, MSLOT_MISSILE, near);
}

bool monsters::pickup_wand(item_def &item, int near)
{
    // Only low-HD monsters bother with wands.
    return hit_dice < 14 && pickup(item, MSLOT_WAND, near);
}

bool monsters::pickup_scroll(item_def &item, int near)
{
    return pickup(item, MSLOT_SCROLL, near);
}

bool monsters::pickup_potion(item_def &item, int near)
{
    return pickup(item, MSLOT_POTION, near);
}

bool monsters::pickup_gold(item_def &item, int near)
{
    return pickup(item, MSLOT_GOLD, near);
}

bool monsters::eat_corpse(item_def &carrion, int near)
{
    if (!mons_eats_corpses(this))
        return (false);

    hit_points += 1 + random2(mons_weight(carrion.plus)) / 100;

    // limited growth factor here -- should 77 really be the cap? {dlb}:
    if (hit_points > 100)
        hit_points = 100;

    if (hit_points > max_hit_points)
        max_hit_points = hit_points;

    if (need_message(near))
        mprf("%s eats %s.", name(DESC_CAP_THE).c_str(),
             carrion.name(DESC_NOCAP_THE).c_str());

    destroy_item( carrion.index() );
    return (true);
}

bool monsters::pickup_misc(item_def &item, int near)
{
    // Never pick up runes.
    if (item.sub_type == MISC_RUNE_OF_ZOT)
        return (false);
    
    return pickup(item, MSLOT_MISCELLANY, near);
}

bool monsters::pickup_item(item_def &item, int near, bool force)
{
    // Never pick up stuff when we're in battle.
    if (!force && (behaviour != BEH_WANDER || attitude == ATT_NEUTRAL))
        return (false);
    
    // Jellies are not handled here.
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        return pickup_weapon(item, near, force);
    case OBJ_ARMOUR:
        return pickup_armour(item, near, force);
    case OBJ_MISSILES:
        return pickup_missile(item, near, force);
    case OBJ_WANDS:
        return pickup_wand(item, near);
    case OBJ_SCROLLS:
        return pickup_scroll(item, near);
    case OBJ_POTIONS:
        return pickup(item, MSLOT_POTION, near);
    case OBJ_CORPSES:
        return eat_corpse(item, near);
    case OBJ_MISCELLANY:
        return pickup_misc(item, near);
    case OBJ_GOLD:
        return pickup_gold(item, near);
    default:
        return (false);
    }
}

bool monsters::need_message(int &near) const
{
    return near != -1? near : (near = visible());
}

void monsters::swap_weapons(int near)
{
    item_def *weap = mslot_item(MSLOT_WEAPON);
    item_def *alt  = mslot_item(MSLOT_ALT_WEAPON);
    
    if (weap)
        if(!unequip(*weap, MSLOT_WEAPON, near))
            // Item was cursed
            return;

    swap_slots(MSLOT_WEAPON, MSLOT_ALT_WEAPON);

    if (alt)
        equip(*alt, MSLOT_WEAPON, near);

    // Monsters can swap weapons really fast. :-)
    if ((weap || alt) && speed_increment >= 2)
    {
        if (const monsterentry *entry = find_monsterentry())
            speed_increment -= div_rand_round(entry->energy_usage.attack, 5);
    }
}

void monsters::wield_melee_weapon(int near)
{
    const item_def *weap = mslot_item(MSLOT_WEAPON);
    if (!weap || (!weap->cursed() && is_range_weapon(*weap)))
    {
        const item_def *alt = mslot_item(MSLOT_ALT_WEAPON);
        if (alt && (!weap || !is_range_weapon(*alt)))
            swap_weapons(near);
    }
}

item_def *monsters::slot_item(equipment_type eq)
{
    return mslot_item(equip_slot_to_mslot(eq));
}

item_def *monsters::mslot_item(mon_inv_type mslot) const
{
    const int mindex = mslot == NUM_MONSTER_SLOTS? NON_ITEM : inv[mslot];
    return (mindex == NON_ITEM? NULL: &mitm[mindex]);
}

item_def *monsters::shield()
{
    return (mslot_item(MSLOT_SHIELD));
}

std::string monsters::name(description_level_type desc) const
{
    return this->name(desc, false);
}

std::string monsters::name(description_level_type desc, bool force_vis) const
{
    const bool possessive =
        (desc == DESC_NOCAP_YOUR || desc == DESC_NOCAP_ITS);

    if (possessive)
        desc = DESC_NOCAP_THE;

    std::string mname = str_monam(*this, desc, force_vis);
    return (possessive? apostrophise(mname) : mname);
}

std::string monsters::pronoun(pronoun_type pro) const
{
    return (mons_pronoun(static_cast<monster_type>(type), pro));
}

std::string monsters::conj_verb(const std::string &verb) const
{
    if (!verb.empty() && verb[0] == '!')
        return (verb.substr(1));
    
    if (verb == "are")
        return ("is");
    
    return (pluralise(verb));
}

int monsters::id() const
{
    return (type);
}

int monsters::get_experience_level() const
{
    return (hit_dice);
}

bool monsters::fumbles_attack(bool verbose)
{
    if (floundering() && one_chance_in(4))
    {
        if (verbose)
        {
            if (you.can_see(this))
                mprf("%s splashes around in the water.",
                     this->name(DESC_CAP_THE).c_str());
            else if (!silenced(you.x_pos, you.y_pos) && !silenced(x, y))
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
    if ( !this->can_go_berserk() )
        return;

    if (has_ench(ENCH_SLOW))
    {
        del_ench(ENCH_SLOW, true); // give no additional message
        simple_monster_message(
            this,
            make_stringf(" shakes off %s lethargy.",
                         pronoun(PRONOUN_NOCAP_POSSESSIVE).c_str()).c_str());
    }
    del_ench(ENCH_HASTE);
    del_ench(ENCH_FATIGUE, true); // give no additional message

    const int duration = 16 + random2avg(13, 2);
    add_ench(mon_enchant(ENCH_BERSERK, 0, KC_OTHER, duration * 10));
    add_ench(mon_enchant(ENCH_HASTE, 0, KC_OTHER, duration * 10));
    simple_monster_message( this, " goes berserk!" );
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

bool monsters::has_spell(spell_type spell) const
{
    for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
    {
        if (spells[i] == spell)
            return (true);
    }
    return (false);
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

bool monsters::backlit() const
{
    return (has_ench(ENCH_BACKLIGHT));
}

bool monsters::caught() const
{
    return (mons_is_caught(this));
}

int monsters::shield_bonus() const
{
    const item_def *shld = const_cast<monsters*>(this)->shield();
    if (shld)
    {
        // Note that 0 is not quite no-blocking.
        if (incapacitated())
            return (0);
        
        const int shld_c = property(*shld, PARM_AC);
        return (random2(1 + shld_c) + random2(hit_dice / 2));
    }
    return (-100);
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
    else if (caught())
        evasion /= (body_size(PSIZE_BODY) + 2);
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

mon_holy_type monsters::holiness() const
{
    return (mons_holiness(this));
}

int monsters::res_fire() const
{
    return (mons_res_fire(this));
}

int monsters::res_steam() const
{
    return (mons_res_steam(this));
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

flight_type monsters::flight_mode() const
{
    return (mons_flies(this));
}

bool monsters::is_levitating() const
{
    // Checking class flags is not enough - see mons_flies.
    return (flight_mode() == FL_LEVITATE);
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

void monsters::hurt(const actor *agent, int amount)
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
                        monster_index( dynamic_cast<const monsters*>(agent) ));
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
        add_ench( mon_enchant(ENCH_ROT, std::min(rotlevel, 4),
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
#ifdef USE_TILE
    TileGhostInit(g);
#endif    
    ghost.reset( new ghost_demon(g) );
}

void monsters::pandemon_init()
{
    hit_dice = ghost->xl;
    hit_points = ghost->max_hp;
    max_hit_points = ghost->max_hp;
    ac = ghost->ac;
    ev = ghost->ev;
    speed = (one_chance_in(3) ? 10 : 8 + roll_dice(2, 9));
    speed_increment = 70;
    if (you.char_direction == GDT_ASCENDING && you.level_type == LEVEL_DUNGEON)
        colour = LIGHTRED;
    else
        colour = random_colour();        // demon's colour
    load_spells(MST_GHOST);
}

void monsters::ghost_init()
{
    type = MONS_PLAYER_GHOST;
    hit_dice = ghost->xl;
    hit_points = ghost->max_hp;
    max_hit_points = ghost->max_hp;
    ac = ghost->ac;
    ev = ghost->ev;
    speed = ghost->speed;
    speed_increment = 70;
    attitude = ATT_HOSTILE;
    behaviour = BEH_WANDER;
    flags = 0;
    foe = MHITNOT;
    foe_memory = 0;
    colour = mons_class_colour(MONS_PLAYER_GHOST);
    number = MONS_PROGRAM_BUG;
    load_spells(MST_GHOST);

    inv.init(NON_ITEM);
    enchantments.clear();
    ench_countdown = 0;

    find_place_to_live();
}

bool monsters::check_set_valid_home(const coord_def &place,
                                    coord_def &chosen,
                                    int &nvalid) const
{
    if (!in_bounds(place))
        return (false);

    if (place == you.pos())
        return (false);

    if (mgrd(place) != NON_MONSTER || grd(place) < DNGN_FLOOR)
        return (false);

    if (one_chance_in(++nvalid))
        chosen = place;

    return (true);
}

bool monsters::find_home_around(const coord_def &c, int radius)
{
    coord_def place(-1, -1);
    int nvalid = 0;
    for (int yi = -radius; yi <= radius; ++yi)
    {
        const coord_def c1(c.x - radius, c.y + yi);
        const coord_def c2(c.x + radius, c.y + yi);
        check_set_valid_home(c1, place, nvalid);
        check_set_valid_home(c2, place, nvalid);
    }

    for (int xi = -radius + 1; xi < radius; ++xi)
    {
        const coord_def c1(c.x + xi, c.y - radius);
        const coord_def c2(c.x + xi, c.y + radius);
        check_set_valid_home(c1, place, nvalid);
        check_set_valid_home(c2, place, nvalid);
    }

    if (nvalid)
    {
        x = place.x;
        y = place.y;
        return (true);
    }
    return (false);
}

bool monsters::find_place_near_player()
{
    for (int radius = 1; radius < 7; ++radius)
        if (find_home_around(you.pos(), radius))
            return (true);
    return (false);
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
    ench_countdown = 0;
    inv.init(NON_ITEM);

    flags = 0;
    experience = 0L;
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

void monsters::set_transit(const level_id &dest)
{
    add_monster_to_transit(dest, *this);
}

void monsters::load_spells(mon_spellbook_type book)
{
    spells.init(SPELL_NO_SPELL);
    if (book == MST_NO_SPELLS || (book == MST_GHOST && !ghost.get()))
        return;

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS, "%s: loading spellbook #%d", 
          name(DESC_PLAIN).c_str(), static_cast<int>(book) );
#endif

    if (book == MST_GHOST)
        spells = ghost->spells;
    else 
    {
        for (unsigned int i = 0; i < ARRAYSIZE(mspell_list); ++i)
        {
            if (mspell_list[i].type == book)
            {
                for (int j = 0; j < NUM_MONSTER_SPELL_SLOTS; ++j)
                    spells[j] = mspell_list[i].spells[j];
                break;
            }
        }
    }
#if DEBUG_DIAGNOSTICS
    // Only for ghosts, too spammy to use for all monsters.
    if (book == MST_GHOST)
    {
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; i++)
            mprf( MSGCH_DIAGNOSTICS, "Spell #%d: %d (%s)",
                  i, spells[i], spell_title(spells[i]) );
    }
#endif
}

bool monsters::has_ench(enchant_type ench) const
{
    return (enchantments.find(ench) != enchantments.end());
}

bool monsters::has_ench(enchant_type ench, enchant_type ench2) const
{
    if (ench2 == ENCH_NONE)
        ench2 = ench;

    for (int i = ench; i <= ench2; ++i)
    {
        if (has_ench(static_cast<enchant_type>(i)))
            return (true);
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
            return (i->second);
    }

    return mon_enchant();
}

void monsters::update_ench(const mon_enchant &ench)
{
    if (ench.ench != ENCH_NONE)
    {
        mon_enchant_list::iterator i = enchantments.find(ench.ench);
        if (i != enchantments.end())
            i->second = ench;
    }
}

bool monsters::add_ench(const mon_enchant &ench)
{
    // silliness
    if (ench.ench == ENCH_NONE)
        return (false);

    mon_enchant_list::iterator i = enchantments.find(ench.ench);
    bool new_enchantment = false;
    mon_enchant *added = NULL;
    if (i == enchantments.end())
    {
        new_enchantment = true;
        added = &(enchantments[ench.ench] = ench);
    }
    else
    {
        i->second += ench;
        added = &i->second;
    }

    // If the duration is not set, we must calculate it (depending on the
    // enchantment).
    if (!ench.duration)
        added->set_duration(this, new_enchantment? NULL : &ench);

    if (new_enchantment)
        add_enchantment_effect(ench);

    return (true);
}

void monsters::add_enchantment_effect(const mon_enchant &ench, bool quiet)
{
    // check for slow/haste
    switch (ench.ench)
    {
    case ENCH_BERSERK:
        // Inflate hp.
        scale_hp(3, 2);
        break;
    
    case ENCH_HASTE:
        if (speed >= 100)
            speed = 100 + ((speed - 100) * 2);
        else
            speed *= 2;

        break;

    case ENCH_SLOW:
        if (speed >= 100)
            speed = 100 + ((speed - 100) / 2);
        else
            speed /= 2;

        break;

    case ENCH_SUBMERGED:
        if (type == MONS_AIR_ELEMENTAL && mons_near(this) && !quiet)
            mprf("%s merges itself into the air.",
                 name(DESC_CAP_A, true).c_str() );
        break;
        
    case ENCH_CHARM:
        behaviour = BEH_SEEK;
        target_x = you.x_pos;
        target_y = you.y_pos;
        foe = MHITYOU;
        break;

    default:
        break;
    }
}

bool monsters::del_ench(enchant_type ench, bool quiet)
{
    mon_enchant_list::iterator i = enchantments.find(ench);
    if (i == enchantments.end())
        return (false);

    const mon_enchant me = i->second;
    const enchant_type et = i->first;
    enchantments.erase(et);
    remove_enchantment_effect(me, quiet);
    return (true);
}

void monsters::remove_enchantment_effect(const mon_enchant &me, bool quiet)
{
    switch (me.ench)
    {
    case ENCH_BERSERK:
        scale_hp(2, 3);
        break;

    case ENCH_HASTE:
        if (speed >= 100)
            speed = 100 + ((speed - 100) / 2);
        else
            speed /= 2;

        break;

    case ENCH_SLOW:
        if (!quiet)
            simple_monster_message(this, " is no longer moving slowly.");
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
        snprintf( info, INFO_SIZE, " seems to regain %s courage.",
                  mons_pronoun(static_cast<monster_type>(this->type),
                               PRONOUN_NOCAP_POSSESSIVE));
                  
        if (!quiet)
            simple_monster_message(this, info);

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
                mprf("%s appears!", name(DESC_CAP_A, true).c_str() );
            
            seen_monster(this);
        }
        break;

    case ENCH_CHARM:
        if (!quiet)
            simple_monster_message(this, " is no longer charmed.");

        if (mons_near(this) && player_monster_visible(this))
        {
            // and fire activity interrupts
            interrupt_activity(AI_SEE_MONSTER,
                               activity_interrupt_data(this, "uncharm"));
        }

        // reevaluate behaviour
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_BACKLIGHT:
        if (!quiet)
        {
            if (player_monster_visible(this))
                simple_monster_message(this, " stops glowing.");
            else if (has_ench(ENCH_INVIS) && mons_near(this))
                mprf("%s stops glowing and disappears.",
                     name(DESC_CAP_THE, true).c_str());
        }
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

    case ENCH_HELD:
    {
        int net = get_trapping_net(x,y);
        if (net != NON_ITEM)
            remove_item_stationary(mitm[net]);
            
        if (!quiet)
            simple_monster_message(this, " breaks free.");
        break;
    }
    case ENCH_ABJ:
    case ENCH_SHORT_LIVED:
        add_ench( mon_enchant(ENCH_ABJ) );
 
        // just for flavour
        if ((this->flags & MF_GOD_GIFT) && this->has_ench(ENCH_BERSERK))
            simple_monster_message(this, " is no longer berserk.");

        monster_die( this, quiet? KILL_DISMISSED : KILL_RESET, 0 );
        break;

    case ENCH_SUBMERGED:
        if (you.can_see(this))
        {
            if (!mons_is_safe( static_cast<const monsters*>(this))
                && is_run_delay(current_delay_action()))
            {
                // Already set somewhere else.
                if (seen_context != "")
                    return;

                if (type == MONS_AIR_ELEMENTAL)
                    seen_context = "thin air";
                else if (monster_habitable_grid(this, DNGN_FLOOR))
                    seen_context = "bursts forth";
                else
                    seen_context = "surfaces";
            }
            else if (!quiet)
                if (type == MONS_AIR_ELEMENTAL)
                    mprf("%s forms itself from the air!",
                         name(DESC_CAP_A, true).c_str() );
        }
        else if (mons_near(this) && monster_habitable_grid(this, DNGN_FLOOR))
        {
            mpr("Something invisble bursts forth from the water.");
            interrupt_activity( AI_FORCE_INTERRUPT );
        }

        break;

    default:
        break;
    }
}

bool monsters::lose_ench_levels(const mon_enchant &e, int lev)
{
    if (!lev)
        return (false);

    if (e.degree <= lev)
    {
        del_ench(e.ench);
        return (true);
    }
    else
    {
        mon_enchant newe(e);
        newe.degree -= lev;
        update_ench(newe);
        return (false);
    }
}

bool monsters::lose_ench_duration(const mon_enchant &e, int dur)
{
    if (!dur)
        return (false);

    if (e.duration <= dur)
    {
        del_ench(e.ench);
        return (true);
    }
    else
    {
        mon_enchant newe(e);
        newe.duration -= dur;
        update_ench(newe);
        return (false);
    }
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
    if (enchantments.empty())
        return;
    
    const mon_enchant_list ec = enchantments;
    for (mon_enchant_list::const_iterator i = ec.begin();
         i != ec.end(); ++i)
    {
        switch (i->first)
        {
        case ENCH_POISON: case ENCH_ROT: case ENCH_BACKLIGHT:
        case ENCH_STICKY_FLAME: case ENCH_ABJ: case ENCH_SHORT_LIVED:
        case ENCH_SLOW: case ENCH_HASTE: case ENCH_FEAR:
        case ENCH_INVIS: case ENCH_CHARM: case ENCH_SLEEP_WARY:
        case ENCH_SICK: case ENCH_SLEEPY: case ENCH_PARALYSIS:
        case ENCH_BATTLE_FRENZY: case ENCH_NEUTRAL:
            lose_ench_levels(i->second, levels);
            break;

        case ENCH_BERSERK:
            del_ench(i->first);
            del_ench(ENCH_HASTE);
            break;

        case ENCH_FATIGUE:
            del_ench(i->first);
            del_ench(ENCH_SLOW);
            break;

        case ENCH_TP:
            del_ench(i->first);
            teleport(true);
            break;

        case ENCH_CONFUSION:
            if (!mons_class_flag(type, M_CONFUSED))
            {
                del_ench(i->first);
            }
            blink();
            break;

        case ENCH_HELD:
            del_ench(i->first);
            break;

        default:
            break;
        }

        if (!alive())
            break;
    }
}

std::string monsters::describe_enchantments() const
{
    std::ostringstream oss;
    for (mon_enchant_list::const_iterator i = enchantments.begin();
         i != enchantments.end(); ++i)
    {
        if (i != enchantments.begin())
            oss << ", ";
        oss << std::string(i->second);
    }
    return (oss.str());
}

// used to adjust time durations in calc_duration() for monster speed
static inline int mod_speed( int val, int speed )
{
    if (!speed)
        speed = 10;
    const int modded = (speed ? (val * 10) / speed : val);
    return (modded? modded : 1);
}

bool monsters::decay_enchantment(const mon_enchant &me, bool decay_degree)
{
    // Faster monsters can wiggle out of the net more quickly.
    const int spd = (me.ench == ENCH_HELD) ? speed :
                                             10;
    const int actdur = speed_to_duration(spd);
    if (lose_ench_duration(me, actdur))
        return (true);

    if (!decay_degree)
        return (false);
    
    // Decay degree so that higher degrees decay faster than lower
    // degrees, and a degree of 1 does not decay (it expires when the
    // duration runs out).
    const int level = me.degree;
    if (level <= 1)
        return (false);

    const int decay_factor = level * (level + 1) / 2;
    if (me.duration < me.maxduration * (decay_factor - 1) / decay_factor)
    {
        mon_enchant newme = me;
        --newme.degree;
        newme.maxduration = newme.duration;

        if (newme.degree <= 0)
        {
            del_ench(me.ench);
            return (true);
        }
        else
            update_ench(newme);
    }
    return (false);
}

void monsters::apply_enchantment(const mon_enchant &me)
{
    const int spd = 10;
    switch (me.ench)
    {
    case ENCH_BERSERK:
        if (decay_enchantment(me))
        {
            simple_monster_message(this, " is no longer berserk.");
            del_ench(ENCH_HASTE);
            const int duration = random_range(70, 130);
            add_ench(mon_enchant(ENCH_FATIGUE, 0, KC_OTHER, duration));
            add_ench(mon_enchant(ENCH_SLOW, 0, KC_OTHER, duration));
        }
        break;

    case ENCH_FATIGUE:
        if (decay_enchantment(me))
        {
            simple_monster_message(this, " looks more energetic.");
            del_ench(ENCH_SLOW, true);
        }
        break;

    case ENCH_SLOW:
    case ENCH_HASTE:
    case ENCH_FEAR:
    case ENCH_PARALYSIS:
    case ENCH_SICK:
    case ENCH_BACKLIGHT:
    case ENCH_ABJ:
    case ENCH_CHARM:
    case ENCH_SLEEP_WARY:
        decay_enchantment(me);
        break;
            
    case ENCH_BATTLE_FRENZY:
        decay_enchantment(me, false);
        break;

    case ENCH_HELD:
    {
        if (mons_is_stationary(this) || mons_is_paralysed(this)
            || this->behaviour == BEH_SLEEP)
        {
            break;
        }
        
        int net = get_trapping_net(x,y,true);

        if (net == NON_ITEM) // really shouldn't happen!
        {
            del_ench(ENCH_HELD);
            break;
        }

        // handled in handle_pickup
        if (mons_itemuse(type) == MONUSE_EATS_ITEMS)
            break;
        
        // the enchantment doubles as the durability of a net
        // the more corroded it gets, the more easily it will break
        const int hold = mitm[net].plus; // this will usually be negative
        const int mon_size = body_size(PSIZE_BODY);
            
        // smaller monsters can escape more quickly
        if (mon_size < random2(SIZE_BIG)  // BIG = 5
            && !has_ench(ENCH_BERSERK) && type != MONS_DANCING_WEAPON)
        {
            if (mons_near(this) && !player_monster_visible(this))
                mpr("Something wriggles in the net.");
            else
                simple_monster_message(this, " struggles to escape the net.");

            // confused monsters have trouble finding the exit
            if (has_ench(ENCH_CONFUSION) && !one_chance_in(5))
                break;

            decay_enchantment(me, 2*(NUM_SIZE_LEVELS - mon_size) - hold);
            
            // frayed nets are easier to escape
            if (mon_size <= -(hold-1)/2)
                decay_enchantment(me, (NUM_SIZE_LEVELS - mon_size));
        }
        else // large (and above) monsters always thrash the net and destroy it
        {    // e.g. ogre, large zombie (large); centaur, nage, hydra (big)
        
            if (mons_near(this) && !player_monster_visible(this))
                mpr("Something wriggles in the net.");
            else
                simple_monster_message(this, " struggles against the net.");

            // confused monsters more likely to struggle without result
            if (has_ench(ENCH_CONFUSION) && one_chance_in(3))
                break;

            // nets get destroyed more quickly for larger monsters
            // and if already strongly frayed
            int damage = 0;
            
            // tiny: 1/6, little: 2/5, small: 3/4, medium and above: always
            if (random2(SIZE_GIANT - mon_size) <= mon_size)
                damage++;

            // handled specially to make up for its small size
            if (type == MONS_DANCING_WEAPON)
            {
                damage += one_chance_in(3);

                if (can_cut_meat(mitm[inv[MSLOT_WEAPON]]))
                    damage++;
            }
            
            
            // extra damage for large (50%) and big (always)
            if (mon_size == SIZE_BIG || mon_size == SIZE_LARGE && coinflip())
                damage++;

            // overall damage per struggle:
            // tiny   -> 1/6
            // little -> 2/5
            // small  -> 3/4
            // medium -> 1
            // large  -> 1,5
            // big    -> 2

            // extra damage if already damaged
            if (random2(body_size(PSIZE_BODY) - hold + 1) >= 4)
                damage++;

            // berserking doubles damage dealt
            if (has_ench(ENCH_BERSERK))
                damage *= 2;

            // Faster monsters can damage the net more often per
            // time period.
            if (speed != 0)
                damage = div_rand_round(damage * speed, spd);

            mitm[net].plus -= damage;
            
            if (mitm[net].plus < -7)
            {
                if (mons_near(this))
                {
                    if (player_monster_visible(this))
                    {
                        mprf("The net rips apart, and %s comes free!",
                             name(DESC_NOCAP_THE).c_str());
                    }
                    else
                    {
                        mpr("All of a sudden the net rips apart!");
                    }
                }
                destroy_item(net);

                del_ench(ENCH_HELD, true);
            }

        }
        break;
    }
    case ENCH_CONFUSION:
        if (!mons_class_flag(type, M_CONFUSED))
            decay_enchantment(me);
        break;

    case ENCH_INVIS:
        if (!mons_class_flag( type, M_INVIS ))
            decay_enchantment(me);
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
        const dungeon_feature_type grid = grd[x][y];

        // Badly injured monsters prefer to stay submerged...
        // electrical eels and lava snakes have ranged attacks
        // and are more likely to surface.  -- bwr
        if (!monster_can_submerge(this, grid))
            del_ench(ENCH_SUBMERGED); // forced to surface
        else if (hit_points <= max_hit_points / 2)
            break;
        else if (((type == MONS_ELECTRICAL_EEL || type == MONS_LAVA_SNAKE)
                  && (one_chance_in(50) || (mons_near(this) 
                                            && hit_points == max_hit_points
                                            && !one_chance_in(10))))
                 || one_chance_in(200)
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
        const int poisonval = me.degree;
        int dam = (poisonval >= 4) ? 1 : 0;

        if (coinflip())
            dam += roll_dice( 1, poisonval + 1 );

        if (mons_res_poison(this) < 0)
            dam += roll_dice( 2, poisonval ) - 1;

        if (dam > 0)
        {
            hurt_monster( this, dam );

#if DEBUG_DIAGNOSTICS
            // for debugging, we don't have this silent.
            simple_monster_message( this, " takes poison damage.", 
                                    MSGCH_DIAGNOSTICS );
            mprf(MSGCH_DIAGNOSTICS, "poison damage: %d", dam );
#endif

            if (hit_points < 1)
            {
                monster_die(this, me.killer(), me.kill_agent());
                break;
            }
        }

        decay_enchantment(me, true);
        break;
    }
    case ENCH_ROT:
    {
        if (hit_points > 1 && one_chance_in(3))
        {
            hurt_monster(this, 1);
            if (hit_points < max_hit_points && coinflip())
                --max_hit_points;
        }

        decay_enchantment(me, true);
        break;
    }

    // assumption: mons_res_fire has already been checked
    case ENCH_STICKY_FLAME:
    {
        int dam =
            resist_adjust_damage(this, BEAM_FIRE, res_fire(),
                                 roll_dice( 2, 4 ) - 1);

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

        decay_enchantment(me, true);
        break;
    }

    case ENCH_SHORT_LIVED:
        // This should only be used for ball lightning -- bwr
        if (decay_enchantment(me))
            hit_points = -1;
        break;

    case ENCH_GLOWING_SHAPESHIFTER:     // this ench never runs out
        // number of actions is fine for shapeshifters
        if (type == MONS_GLOWING_SHAPESHIFTER || one_chance_in(4))
            monster_polymorph(this, RANDOM_MONSTER, PPT_SAME);
        break;

    case ENCH_SHAPESHIFTER:     // this ench never runs out
        if (type == MONS_SHAPESHIFTER 
            || random2(1000) < ( 1000 / ((15 * hit_dice) / 5)))
        {
            monster_polymorph(this, RANDOM_MONSTER, PPT_SAME);
        }
        break;

    case ENCH_TP:
        if (decay_enchantment(me, true))
            monster_teleport( this, true );
        break;

    case ENCH_SLEEPY:
        del_ench(ENCH_SLEEPY);
        break;

    default:
        break;
    }
}

void monsters::mark_summoned(int longevity, bool mark_items)
{
    add_ench( mon_enchant(ENCH_ABJ, longevity) );
    if (mark_items)
    {
        for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        {
            const int item = inv[i];
            if (item != NON_ITEM)
                mitm[item].flags |= ISFLAG_SUMMONED;
        }
    }
}

void monsters::apply_enchantments()
{
    if (enchantments.empty())
        return;
    
    const mon_enchant_list ec = enchantments;
    for (mon_enchant_list::const_iterator i = ec.begin(); i != ec.end(); ++i)
    {
        apply_enchantment(i->second);
        if (!alive())
            break;
    }
}

void monsters::scale_hp(int num, int den)
{
    hit_points     = hit_points * num / den;
    max_hit_points = max_hit_points * num / den;
    
    if (hit_points < 1)
        hit_points = 1;
    if (max_hit_points < 1)
        max_hit_points = 1;
    if (hit_points > max_hit_points)
        hit_points = max_hit_points;
}

kill_category monsters::kill_alignment() const
{
    return (mons_friendly(this)? KC_FRIENDLY : KC_OTHER);
}

bool monsters::sicken(int amount)
{
    if (holiness() != MH_NATURAL || (amount /= 2) < 1)
        return (false);

    if (!has_ench(ENCH_SICK)
        && mons_near(this) && player_monster_visible(this))
    {
        // Yes, could be confused with poisoning.
        mprf("%s looks sick.", name(DESC_CAP_THE).c_str());
    }

    add_ench(mon_enchant(ENCH_SICK, 0, KC_OTHER, amount * 10));

    return (true);
}

int monsters::base_speed(int mcls)
{
    const monsterentry *m = get_monster_data(mcls);
    if (!m)
        return (10);

    int speed = m->speed;

    switch (mcls)
    {
    case MONS_ABOMINATION_SMALL:
        speed = 7 + random2avg(9, 2);
        break;
    case MONS_ABOMINATION_LARGE:
        speed = 6 + random2avg(7, 2);
        break;
    case MONS_BEAST:
        speed = 8 + random2(5);
        break;
    }

    return (speed);
}

// Recalculate movement speed.
void monsters::fix_speed()
{
    if (mons_is_zombified(this))
        speed = base_speed(number) - 2;
    else
        speed = base_speed(type);

    if (has_ench(ENCH_HASTE))
        speed *= 2;
    else if (has_ench(ENCH_SLOW))
        speed /= 2;
}

// Check speed and speed_increment sanity.
void monsters::check_speed()
{
    // FIXME: If speed is borked, recalculate. Need to figure out how speed
    // is getting borked.
    if (speed < 0 || speed > 130)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "Bad speed: %s, spd: %d, spi: %d, hd: %d, ench: %s",
             name(DESC_PLAIN).c_str(),
             speed, speed_increment, hit_dice,
             describe_enchantments().c_str());
#endif
        
        fix_speed();
        
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Fixed speed for %s to %d",
             name(DESC_PLAIN).c_str(), speed);
#endif
    }

    if (speed_increment < 0)
        speed_increment = 0;

    if (speed_increment > 200)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "Clamping speed increment on %s: %d",
             name(DESC_PLAIN).c_str(), speed_increment);
#endif
        speed_increment = 140;
    }
}

actor *monsters::get_foe() const
{
    if (foe == MHITNOT)
        return (NULL);
    else if (foe == MHITYOU)
        return (&you);

    // must be a monster
    monsters *my_foe = &menv[foe];
    return (my_foe->alive()? my_foe : NULL);
}

int monsters::foe_distance() const
{
    const actor *afoe = get_foe();
    return (afoe? pos().distance_from(afoe->pos()) : INFINITE_DISTANCE);
}

bool monsters::can_go_berserk() const
{
    if (holiness() != MH_NATURAL)
        return (false);
    
    if (has_ench(ENCH_FATIGUE) || has_ench(ENCH_BERSERK))
        return (false);

    // If we have no melee attack, going berserk is pointless.
    const mon_attack_def attk = mons_attack_spec(this, 0);
    if (attk.type == AT_NONE || attk.damage == 0)
        return (false);
    
    return (true);
}

bool monsters::needs_berserk(bool check_spells) const
{
    if (!can_go_berserk())
        return (false);
    
    if (has_ench(ENCH_HASTE) || has_ench(ENCH_TP))
        return (false);

    if (foe_distance() > 3)
        return (false);

    if (check_spells)
    {
        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
        {
            const int spell = spells[i];
            if (spell != SPELL_NO_SPELL && spell != SPELL_BERSERKER_RAGE)
                return (false);
        }
    }

    return (true);
}

bool monsters::can_see_invisible() const
{
    return (mons_see_invis(this));
}

bool monsters::invisible() const
{
    return (has_ench(ENCH_INVIS) && !backlit());
}

bool monsters::visible_to(const actor *looker) const
{
    if (this == looker)
        return (!invisible() || can_see_invisible());

    if (looker->atype() == ACT_PLAYER)
        return player_monster_visible(this);
    else
    {
        const monsters* mon = dynamic_cast<const monsters*>(looker);
        return mons_monster_visible(mon, this);
    }
}

bool monsters::can_see(const actor *target) const
{
    if (this == target)
        return visible_to(target);

    if (!target->visible_to(this))
        return false;

    if (target->atype() == ACT_PLAYER)
        return mons_near(this);

    const monsters* mon = dynamic_cast<const monsters*>(target);
    int       tx  = mon->x;
    int       ty  = mon->y;

    if (distance(x, y, tx, ty) > LOS_RADIUS * LOS_RADIUS)
        return false;

    // Ignoring clouds for now.
    return (num_feats_between(x, y, tx, ty, DNGN_UNSEEN, DNGN_MAXOPAQUE) == 0);
}

void monsters::mutate()
{
    if (holiness() != MH_NATURAL)
        return;

    monster_polymorph(this, RANDOM_MONSTER);
}

bool monsters::is_icy() const
{
    return (mons_is_icy(type));
}

bool monsters::has_action_energy() const
{
    return (speed_increment >= 80);
}

void monsters::check_redraw(const coord_def &old) const
{
    const bool see_new = see_grid(pos());
    const bool see_old = see_grid(old);
    if ((see_new || see_old) && !view_update())
    {
        if (see_new)
            view_update_at(pos());
        if (see_old)
            view_update_at(old);
        update_screen();
    }
}

void monsters::apply_location_effects()
{
    dungeon_events.fire_position_event(DET_MONSTER_MOVED, pos());
    
    // monsters stepping on traps:
    mons_trap(this);

    if (alive())
        mons_check_pool(this);

    if (alive() && has_ench(ENCH_SUBMERGED)
        && !monster_can_submerge(this, grd[x][y]))
    {
        del_ench(ENCH_SUBMERGED);
    }
}

// returns true if the trap should be revealed to the player
bool monsters::do_shaft()
{
    if (!is_valid_shaft_level())
        return (false);

    // Handle instances of do_shaft() being invoked magically when
    // the monster isn't standing over a shaft.
    if (trap_type_at_xy(x, y) != TRAP_SHAFT)
    {
        switch(grd[x][y])
        {
        case DNGN_FLOOR:
        case DNGN_OPEN_DOOR:
        case DNGN_TRAP_MECHANICAL:
        case DNGN_TRAP_MAGICAL:
        case DNGN_TRAP_NATURAL:
        case DNGN_UNDISCOVERED_TRAP:
        case DNGN_ENTER_SHOP:
            break;

        default:
            return (false);
        }

        if (airborne() || total_weight() == 0)
        {
            if (mons_near(this))
            {
                if (player_monster_visible(this))
                    mprf("A shaft briefly opens up underneath %s!",
                         name(DESC_NOCAP_THE).c_str());
                else
                    mpr("A shaft briefly opens up in the floor!");
            }

            handle_items_on_shaft(this->x, this->y, false);
            return (false);
        }
    }

    level_id lev = shaft_dest();

    if (lev == level_id::current())
        return (false);

    set_transit(lev);
    bool reveal = false;
    if (simple_monster_message(this, " falls through a shaft!"))
        reveal = true;;

    handle_items_on_shaft(this->x, this->y, false);

    // Monster is no longer on this level
    destroy_inventory();
    monster_cleanup(this);

    return (reveal);
}

void monsters::put_to_sleep(int)
{
    if (has_ench(ENCH_BERSERK))
        return;
    behaviour = BEH_SLEEP;
    add_ench(ENCH_SLEEPY);
    add_ench(ENCH_SLEEP_WARY);
}

void monsters::check_awaken(int)
{
    // XXX
}

const monsterentry *monsters::find_monsterentry() const
{
    return (type == -1 || type == MONS_PROGRAM_BUG) ? NULL
        : get_monster_data(type);
}

int monsters::action_energy(energy_use_type et) const
{
    if (const monsterentry *me = find_monsterentry())
    {
        const mon_energy_usage &mu = me->energy_usage;
        switch (et)
        {
        case EUT_MOVE:    return mu.move;
        case EUT_SWIM:    return mu.swim;
        case EUT_MISSILE: return mu.missile;
        case EUT_ITEM:    return mu.item;
        case EUT_SPECIAL: return mu.special;
        case EUT_SPELL:   return mu.spell;
        case EUT_ATTACK:  return mu.attack;
        case EUT_PICKUP:  return mu.pickup_percent;
        }
    }
    return 10;
}

void monsters::lose_energy(energy_use_type et)
{
    speed_increment -= action_energy(et);
}

/////////////////////////////////////////////////////////////////////////
// mon_enchant

static const char *enchant_names[] = 
{
    "none", "slow", "haste", "fear", "conf", "inv", "pois", "bers",
    "rot", "summon", "abj", "backlit", "charm", "fire",
    "gloshifter", "shifter", "tp", "wary", "submerged",
    "short lived", "paralysis", "sick", "sleep", "fatigue", "held",
    "blood-lust", "neutral", "bug"
};

const char *mons_enchantment_name(enchant_type ench)
{
    ASSERT(NUM_ENCHANTMENTS ==
           (sizeof(enchant_names) / sizeof(*enchant_names)) - 1);
    
    if (ench > NUM_ENCHANTMENTS)
        ench = NUM_ENCHANTMENTS;

    return (enchant_names[ench]);
}

mon_enchant::mon_enchant(enchant_type e, int deg, kill_category whose,
                         int dur)
    : ench(e), degree(deg), duration(dur), maxduration(0), who(whose)
{
}

mon_enchant::operator std::string () const
{
    return make_stringf("%s (%d:%d%s)",
                        mons_enchantment_name(ench),
                        degree,
                        duration,
                        kill_category_desc(who));
}

const char *mon_enchant::kill_category_desc(kill_category k) const
{
    return (k == KC_YOU?      " you" :
            k == KC_FRIENDLY? " pet" : "");
}

void mon_enchant::merge_killer(kill_category k)
{
    who = who < k? who : k;
}

void mon_enchant::cap_degree()
{
    // Sickness is not capped.
    if (ench == ENCH_SICK)
        return;
    
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
        degree   += other.degree;
        cap_degree();
        duration += other.duration;
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

killer_type mon_enchant::killer() const
{
    return (who == KC_YOU?       KILL_YOU :
            who == KC_FRIENDLY?  KILL_MON :
            KILL_MISC);
}

int mon_enchant::kill_agent() const
{
    return (who == KC_FRIENDLY? ANON_FRIENDLY_MONSTER : 0);
}

int mon_enchant::modded_speed(const monsters *mons, int hdplus) const
{
    return (mod_speed(mons->hit_dice + hdplus, mons->speed));
}

int mon_enchant::calc_duration(const monsters *mons,
                               const mon_enchant *added) const
{
    int cturn = 0;
    
    const int newdegree = added? added->degree : degree;
    const int deg = newdegree? newdegree : 1;

    // Beneficial enchantments (like Haste) should not be throttled by
    // monster HD!
    switch (ench)
    {
    case ENCH_HASTE:
        cturn = 1000 / mod_speed(25, mons->speed);
        break;
    case ENCH_INVIS:
        cturn = 1000 / mod_speed(25, mons->speed);
        break;
    case ENCH_SLOW:
        cturn = 250 / (1 + modded_speed(mons, 10));
        break;
    case ENCH_FEAR:
        cturn = 150 / (1 + modded_speed(mons, 5));
        break;
    case ENCH_PARALYSIS:
        cturn = std::max(90 / modded_speed(mons, 5), 3);
        break;
    case ENCH_CONFUSION:
        cturn = std::max(100 / modded_speed(mons, 5), 3);
        break;
    case ENCH_HELD:
        cturn = 120 / mod_speed(25, mons->speed);
        break;
    case ENCH_POISON:
        cturn = 1000 * deg / mod_speed(125, mons->speed);
        break;
    case ENCH_STICKY_FLAME:
        cturn = 1000 * deg / mod_speed(200, mons->speed);
        break;
    case ENCH_ROT:
        if (deg > 1)
            cturn = 1000 * (deg - 1) / mod_speed(333, mons->speed);
        cturn += 1000 / mod_speed(250, mons->speed);
        break;
    case ENCH_BACKLIGHT:
        if (deg > 1)
            cturn = 1000 * (deg - 1) / mod_speed(200, mons->speed);
        cturn += 1000 / mod_speed(100, mons->speed);
        break;
    case ENCH_SHORT_LIVED:
        cturn = 1000 / mod_speed(200, mons->speed);
        break;
    case ENCH_ABJ:
        if (deg >= 6)
            cturn = 1000 / mod_speed(10, mons->speed);
        if (deg >= 5)
            cturn += 1000 / mod_speed(20, mons->speed);
        cturn += 1000 * std::min(4, deg) / mod_speed(100, mons->speed);
        break;
    case ENCH_CHARM:
        cturn = 500 / modded_speed(mons, 10);
        break;
    case ENCH_TP:
        cturn = 1000 * deg / mod_speed(1000, mons->speed);
        break;
    case ENCH_SLEEP_WARY:
        cturn = 1000 / mod_speed(50, mons->speed);
        break;
    default:
        break;
    }
    
    if (cturn < 2)
        cturn = 2;

    int raw_duration = (cturn * speed_to_duration(mons->speed));
    raw_duration = fuzz_value(raw_duration, 60, 40);
    if (raw_duration < 15)
        raw_duration = 15;

    return (raw_duration);
}

// Calculate the effective duration (in terms of normal player time - 10
// duration units being one normal player action) of this enchantment.
void mon_enchant::set_duration(const monsters *mons, const mon_enchant *added)
{
    if (duration && !added)
        return;

    if (added && added->duration)
        duration += added->duration;
    else
        duration += calc_duration(mons, added);

    if (duration > maxduration)
        maxduration = duration;
}

// Replaces the "@foo@" strings in monster shout and monster speak
// definitions.
std::string do_mon_str_replacements(const std::string &in_msg,
                                    const monsters* monster)
{
    std::string msg = in_msg;
    description_level_type nocap, cap;

    if (monster->attitude == ATT_FRIENDLY && !mons_is_unique(monster->type)
        && player_monster_visible(monster))
    {
        nocap = DESC_PLAIN;
        cap   = DESC_PLAIN;

        msg = replace_all(msg, "@the_something@", "your @the_something@");
        msg = replace_all(msg, "@The_something@", "Your @The_something@");
        msg = replace_all(msg, "@the_monster@",   "your @the_monster@");
        msg = replace_all(msg, "@The_monster@",   "Your @the_monster@");
    }
    else
    {
        nocap = DESC_NOCAP_THE;
        cap   = DESC_CAP_THE;
    }

    if (see_grid(monster->x, monster->y))
    {
        dungeon_feature_type feat = grd[monster->x][monster->y];
        if (feat < DNGN_MINMOVE || feat >= NUM_REAL_FEATURES)
            msg = replace_all(msg, "@surface@", "buggy surface");
        else if (feat == DNGN_LAVA)
            msg = replace_all(msg, "@surface@", "lava");
        else if (feat == DNGN_DEEP_WATER || feat == DNGN_SHALLOW_WATER)
            msg = replace_all(msg, "@surface@", "water");
        else if (feat >= DNGN_ALTAR_ZIN && feat < DNGN_BLUE_FOUNTAIN)
            msg = replace_all(msg, "@surface@", "altar");
        else
            msg = replace_all(msg, "@surface@", "ground");

        msg = replace_all(msg, "@feature@", raw_feature_description(feat));
    }
    else
    {
        msg = replace_all(msg, "@surface@", "buggy unseen surface");
        msg = replace_all(msg, "@feature@", "buggy unseen feature");
    }

    msg = replace_all(msg, "@player_name@", you.your_name);

    if (player_monster_visible(monster))
    {
        std::string something = monster->name(DESC_PLAIN);
        msg = replace_all(msg, "@something@",   something);
        msg = replace_all(msg, "@a_something@", monster->name(DESC_NOCAP_A));
        msg = replace_all(msg, "@the_something@", monster->name(nocap));

        something[0] = toupper(something[0]);
        msg = replace_all(msg, "@Something@",   something);
        msg = replace_all(msg, "@A_something@", monster->name(DESC_CAP_A));
        msg = replace_all(msg, "@The_something@", monster->name(cap));
    }
    else
    {
        msg = replace_all(msg, "@something@",     "something");
        msg = replace_all(msg, "@a_something@",   "something");
        msg = replace_all(msg, "@the_something@", "something");

        msg = replace_all(msg, "@Something@",     "Something");
        msg = replace_all(msg, "@A_something@",   "Something");
        msg = replace_all(msg, "@The_something@", "Something");
    }

    std::string plain = monster->name(DESC_PLAIN);
    msg = replace_all(msg, "@monster@",     plain);
    msg = replace_all(msg, "@a_monster@",   monster->name(DESC_NOCAP_A));
    msg = replace_all(msg, "@the_monster@", monster->name(nocap));

    plain[0] = toupper(plain[0]);
    msg = replace_all(msg, "@Monster@",     plain);
    msg = replace_all(msg, "@A_monster@",   monster->name(DESC_CAP_A));
    msg = replace_all(msg, "@The_monster@", monster->name(cap));

    msg = replace_all(msg, "@Pronoun@",
                      monster->pronoun(PRONOUN_CAP));
    msg = replace_all(msg, "@pronoun@",
                      monster->pronoun(PRONOUN_NOCAP));
    msg = replace_all(msg, "@Possessive@",
                      monster->pronoun(PRONOUN_CAP_POSSESSIVE));
    msg = replace_all(msg, "@possessive@",
                      monster->pronoun(PRONOUN_NOCAP_POSSESSIVE));

    msg = replace_all(msg, "@imp_taunt@",   imp_taunt_str());
    msg = replace_all(msg, "@demon_taunt@", demon_taunt_str());

    static const char * sound_list[] = 
    {
        "says",         // actually S_SILENT
        "shouts", 
        "barks", 
        "shouts", 
        "roars", 
        "screams", 
        "bellows", 
        "screeches", 
        "buzzes", 
        "moans", 
        "whines", 
        "croaks", 
        "growls",
        "hisses",
        "breathes", // S_VERY_SOFT
        "whispers", // S_SOFT
        "says",     // S_NORMAL
        "shouts",   // S_LOUD
        "screams"   // S_VERY_LOUD
    };

    if (mons_shouts(monster->type) >= NUM_SHOUTS)
    {
        mpr("Invalid @says@ type.", MSGCH_DIAGNOSTICS);
        msg = replace_all(msg, "@says@", "buggily says");
    }
    else
        msg = replace_all(msg, "@says@",
                          sound_list[mons_shouts(monster->type)]);

    // The proper possessive for a word ending in an "s" is to
    // put an apostrophe after the "s": "Chris" -> "Chris'",
    // not "Chris" -> "Chris's".  Stupid English language...
    msg = replace_all(msg, "s's", "s'");

    return msg;
}

static mon_body_shape get_ghost_shape(const monsters *mon)
{
    const ghost_demon &ghost = *(mon->ghost);

    switch (ghost.species)
    {
    case SP_NAGA:
        return (MON_SHAPE_NAGA);

    case SP_CENTAUR:
        return (MON_SHAPE_CENTAUR);

    case SP_KENKU:
        return (MON_SHAPE_HUMANOID_WINGED);

    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_GOLDEN_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_UNK0_DRACONIAN:
    case SP_UNK1_DRACONIAN:
    case SP_BASE_DRACONIAN:
        return (MON_SHAPE_HUMANOID_TAILED);

    default:
        return (MON_SHAPE_HUMANOID);
    }
}

mon_body_shape get_mon_shape(const monsters *mon)
{
    if (mon->type == MONS_PLAYER_GHOST)
        return get_ghost_shape(mon);
    else if (mons_is_zombified(mon))
        return get_mon_shape(mon->number);
    else
        return get_mon_shape(mon->type);
}

mon_body_shape get_mon_shape(const int type)
{
    switch(mons_char(type))
    {
    case 'a': // ants and cockroaches
        return(MON_SHAPE_INSECT);
    case 'b':  // bats and butterflys
        if (type == MONS_BUTTERFLY)
            return(MON_SHAPE_INSECT_WINGED);
        else
            return(MON_SHAPE_BAT);
    case 'c': // centaurs
        return(MON_SHAPE_CENTAUR);
    case 'd': // draconions and drakes
        if (mons_genus(type) == MONS_DRACONIAN ||
            mons_class_flag(type, M_HUMANOID))
        {
            if (mons_class_flag(type, M_FLIES))
                return(MON_SHAPE_HUMANOID_WINGED_TAILED);
            else
                return(MON_SHAPE_HUMANOID_TAILED);
        }
        else if (mons_class_flag(type, M_FLIES))
            return(MON_SHAPE_QUADRUPED_WINGED);
        else
            return(MON_SHAPE_QUADRUPED);
    case 'e': // elves
        return(MON_SHAPE_HUMANOID);
    case 'f': // fungi
        return(MON_SHAPE_FUNGUS);
    case 'g': // gargoyles, gnolls, goblins and hobgoblins
        if (type == MONS_GARGOYLE)
            return(MON_SHAPE_HUMANOID_WINGED_TAILED);
        else
            return(MON_SHAPE_HUMANOID);
    case 'h': // hounds
    case 'j': // jackals
        return(MON_SHAPE_QUADRUPED);
    case 'k': // killer bees
        return(MON_SHAPE_INSECT_WINGED);
    case 'l': // lizards
        return(MON_SHAPE_QUADRUPED);
    case 'm': // minotaurs, manticores, and snails/slugs/etc
        if (type == MONS_MINOTAUR)
            return(MON_SHAPE_HUMANOID);
        else if (type == MONS_MANTICORE)
            return(MON_SHAPE_QUADRUPED);
        else
            return(MON_SHAPE_SNAIL);
    case 'n': // necrophages and ghouls
        return(MON_SHAPE_HUMANOID);
    case 'o': // orcs
        return(MON_SHAPE_HUMANOID);
    case 'p': // ghosts
        if (type != MONS_INSUBSTANTIAL_WISP &&
            type != MONS_PLAYER_GHOST)
            return(MON_SHAPE_HUMANOID);
    case 'q': // quasists
        return(MON_SHAPE_HUMANOID_TAILED);
    case 'r': // rodents
        return(MON_SHAPE_QUADRUPED);
    case 's': // arachnids and centidpeds
        if (type == MONS_GIANT_CENTIPEDE)
            return(MON_SHAPE_CENTIPEDE);
        else
            return(MON_SHAPE_ARACHNID);
    case 'u': // ugly things are humanoid???
        return(MON_SHAPE_HUMANOID);
    case 'v': // vortices and elementals
        return(MON_SHAPE_MISC);
    case 'w': // worms
        return(MON_SHAPE_SNAKE);
    case 'x': // small abominations
        return(MON_SHAPE_MISC);
    case 'y': // winged insects
        return(MON_SHAPE_INSECT_WINGED);
    case 'z': // small skeletons
        if (type == MONS_SKELETAL_WARRIOR)
            return(MON_SHAPE_HUMANOID);
        else
            // constructed type, not enough info to determine shape
            return(MON_SHAPE_MISC);
    case 'A': // angelic beings
        return(MON_SHAPE_HUMANOID_WINGED);
    case 'B': // beetles
        return(MON_SHAPE_INSECT);
    case 'C': // giants
        return(MON_SHAPE_HUMANOID);
    case 'D': // dragons
        if (mons_class_flag(type, M_FLIES))
            return(MON_SHAPE_QUADRUPED_WINGED);
        else
            return(MON_SHAPE_QUADRUPED);
    case 'E': // effreets
        return(MON_SHAPE_HUMANOID);
    case 'F': // frogs
        return(MON_SHAPE_QUADRUPED_TAILLESS);
    case 'G': // floating eyeballs and orbs
        return(MON_SHAPE_ORB);
    case 'H': // hippogriffs and griffns
        return(MON_SHAPE_QUADRUPED_WINGED);
    case 'I': // ice beasts
        return(MON_SHAPE_QUADRUPED);
    case 'J': // jellies and jellyfish
        return(MON_SHAPE_BLOB);
    case 'K': // kobolds
        return(MON_SHAPE_HUMANOID);
    case 'L': // liches
        return(MON_SHAPE_HUMANOID);
    case 'M': // mummies
        return(MON_SHAPE_HUMANOID);
    case 'N': // nagas
        return(MON_SHAPE_NAGA);
    case 'O': // ogres
        return(MON_SHAPE_HUMANOID);
    case 'P': // plants
        return(MON_SHAPE_PLANT);
    case 'Q': // queen insects
        if (type == MONS_QUEEN_BEE)
            return(MON_SHAPE_INSECT_WINGED);
        else
            return(MON_SHAPE_INSECT);
    case 'R': // rakshasa; humanoid?
        return(MON_SHAPE_HUMANOID);
    case 'S': // snakes
        return(MON_SHAPE_SNAKE);
    case 'T': // trolls
        return(MON_SHAPE_HUMANOID);
    case 'U': // bears
        return(MON_SHAPE_QUADRUPED_TAILLESS);
    case 'V': // vampires
        return(MON_SHAPE_HUMANOID);
    case 'W': // wraiths, humanoid if not a spectral thing
        if (type == MONS_SPECTRAL_THING)
            // constructed type, not enough info to determine shape
            return(MON_SHAPE_MISC);
        else
            return(MON_SHAPE_HUMANOID);
    case 'X': // large abominations
        return(MON_SHAPE_MISC);
    case 'Y': // yaks and sheep
        if (type == MONS_SHEEP)
            return(MON_SHAPE_QUADRUPED_TAILLESS);
        else
            return(MON_SHAPE_QUADRUPED);
    case 'Z': // constructed type, not enough info to determine shape
        return(MON_SHAPE_MISC);
    case ';': // Fish and eels
        if (type == MONS_ELECTRICAL_EEL)
            return(MON_SHAPE_SNAKE);
        else
            return (MON_SHAPE_FISH);

    // The various demons, plus some golems and statues.  And humanoids.
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '&':
    case '8':
    case '@':
        // Assume demon has wings if it can fly.
        bool flies = mons_class_flag(type, M_FLIES);

        // Assume demon has a tail if it has a sting attack or a
        // tail slap attack.
        monsterentry *mon_data = get_monster_data(type);
        bool tailed = false;
        for (int i = 0; i < 4; i++)
            if (mon_data->attack[i].type == AT_STING ||
                mon_data->attack[i].type == AT_TAIL_SLAP)
            {
                tailed = true;
                break;
            }

        if (flies && tailed)
            return(MON_SHAPE_HUMANOID_WINGED_TAILED);
        else if (flies && !tailed)
            return(MON_SHAPE_HUMANOID_WINGED);
        else if (!flies && tailed)
            return(MON_SHAPE_HUMANOID_TAILED);
        else
            return(MON_SHAPE_HUMANOID);
    }

    return(MON_SHAPE_MISC);
}

std::string get_mon_shape_str(const monsters *mon)
{
    return get_mon_shape_str(get_mon_shape(mon));
}

std::string get_mon_shape_str(const int type)
{
    return get_mon_shape_str(get_mon_shape(type));
}

std::string get_mon_shape_str(const mon_body_shape shape)
{
    ASSERT(shape >= MON_SHAPE_HUMANOID && shape <= MON_SHAPE_MISC);

    if (shape < MON_SHAPE_HUMANOID || shape > MON_SHAPE_MISC)
        return("buggy shape");

    static const char *shape_names[] =
    {
        "humanoid", "winged humanoid", "tailed humanoid",
        "winged tailed humanoid", "centaur", "naga",
        "quadruped", "tailless quadruped", "winged quadruped",
        "bat", "snake", "fish",  "insect", "winged insect",
        "arachnid", "centipede", "snail", "plant", "fungus", "orb",
        "blob", "misc"
    };

    return (shape_names[shape]);
}

/////////////////////////////////////////////////////////////////////////
// mon_resist_def

mon_resist_def::mon_resist_def()
    : elec(0), poison(0), fire(0), steam(0), cold(0), hellfire(0),
      asphyx(0), acid(0), sticky_flame(false), pierce(0),
      slice(0), bludgeon(0)
{
}

short mon_resist_def::get_default_res_level(int resist, short level) const
{
    if (level != -100)
        return level;
    switch (resist)
    {
    case MR_RES_STEAM:
    case MR_RES_ACID:
        return 3;
    case MR_RES_ELEC:
        return 2;
    default:
        return 1;
    }
}

mon_resist_def::mon_resist_def(int flags, short level)
    : elec(0), poison(0), fire(0), steam(0), cold(0), hellfire(0),
      asphyx(0), acid(0), sticky_flame(false), pierce(0),
      slice(0), bludgeon(0)
{
    for (int i = 0; i < 32; ++i)
    {
        const short nl = get_default_res_level(1 << i, level);
        switch (flags & (1 << i))
        {
        case MR_RES_STEAM:   steam = 3; break; 
        case MR_RES_ELEC:    elec = nl; break;
        case MR_RES_POISON:  poison = nl; break;
        case MR_RES_FIRE:    fire = nl; break;
        case MR_RES_HELLFIRE: hellfire = nl; break;
        case MR_RES_COLD:    cold = nl; break;
        case MR_RES_ASPHYX:  asphyx = nl; break;
        case MR_RES_ACID:    acid = nl; break;
        case MR_VUL_ELEC:    elec = -nl; break;
        case MR_VUL_POISON:  poison = -nl; break;
        case MR_VUL_FIRE:    fire = -nl; break;
        case MR_VUL_COLD:    cold = -nl; break;
        
        case MR_RES_PIERCE:  pierce = nl; break;
        case MR_RES_SLICE:   slice = nl; break;
        case MR_RES_BLUDGEON: bludgeon = nl; break;
        case MR_VUL_PIERCE:  pierce = -nl; break;
        case MR_VUL_SLICE:   slice = -nl; break;
        case MR_VUL_BLUDGEON: bludgeon = -nl; break;

        case MR_RES_STICKY_FLAME: sticky_flame = true; break;

        default: break;
        }
    }
}

const mon_resist_def &mon_resist_def::operator |= (const mon_resist_def &o)
{
    elec += o.elec;
    poison += o.poison;
    fire += o.fire;
    cold += o.cold;
    hellfire += o.hellfire;
    asphyx += o.asphyx;
    acid += o.acid;
    pierce += o.pierce;
    slice += o.slice;
    bludgeon += o.bludgeon;
    sticky_flame = sticky_flame || o.sticky_flame;
    return (*this);
}

mon_resist_def mon_resist_def::operator | (const mon_resist_def &o) const
{
    mon_resist_def c(*this);
    return (c |= o);
}
