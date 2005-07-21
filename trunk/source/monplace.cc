/*
 *  File:       monplace.cc
 *  Summary:    Functions used when placing monsters in the dungeon.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "monplace.h"

#include "externs.h"
#include "dungeon.h"
#include "monstuff.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "misc.h"
#include "player.h"
#include "stuff.h"
#include "spells4.h"

// NEW place_monster -- note that power should be set to:
// 51 for abyss
// 52 for pandemonium
// x otherwise

// proximity is the same as for mons_place:
// 0 is no restrictions
// 1 attempts to place near player
// 2 attempts to avoid player LOS

#define BIG_BAND        20

static int band_member(int band, int power);
static int choose_band( int mon_type, int power, int &band_size );
static int place_monster_aux(int mon_type, char behaviour, int target,
    int px, int py, int power, int extra, bool first_band_member);

bool place_monster(int &id, int mon_type, int power, char behaviour,
    int target, bool summoned, int px, int py, bool allow_bands,
    int proximity, int extra)
{
    int band_size = 0;
    int band_monsters[BIG_BAND];        // band monster types
    int lev_mons = power;               // final 'power'
    int count;
    int i;

    // set initial id to -1  (unsuccessful create)
    id = -1;

    // (1) early out (summoned to occupied grid)
    if (summoned && mgrd[px][py] != NON_MONSTER)
        return (false);

    // (2) take care of random monsters
    if (mon_type == RANDOM_MONSTER
        && player_in_branch( BRANCH_HALL_OF_BLADES ))
    {
        mon_type = MONS_DANCING_WEAPON;
    }

    if (mon_type == RANDOM_MONSTER)
    {
        if (player_in_branch( BRANCH_MAIN_DUNGEON )
            && lev_mons != 51 && one_chance_in(4))
        {
            lev_mons = random2(power);
        }

        if (player_in_branch( BRANCH_MAIN_DUNGEON ) && lev_mons < 28)
        {
            // potentially nasty surprise, but very rare
            if (one_chance_in(5000))
                lev_mons = random2(27);

            // slightly out of depth monsters are more common:
            if (one_chance_in(50))
                lev_mons += random2(6);

            if (lev_mons > 27)
                lev_mons = 27;
        }

        /* Abyss or Pandemonium. Almost never called from Pan;
           probably only if a rand demon gets summon anything spell */
        if (lev_mons == 51
            || you.level_type == LEVEL_PANDEMONIUM
            || you.level_type == LEVEL_ABYSS)
        {
            do
            {
                count = 0;

                do
                {
                    // was: random2(400) {dlb}
                    mon_type = random2(NUM_MONSTERS);
                    count++;
                }
                while (mons_abyss(mon_type) == 0 && count < 2000);

                if (count == 2000)
                    return (false);
            }
            while (random2avg(100, 2) > mons_rare_abyss(mon_type)
                    && !one_chance_in(100));
        }
        else
        {
            int level, diff, chance;

            if (lev_mons > 30)
                lev_mons = 30;

            for (i = 0; i < 10000; i++)
            {
                int count = 0;

                do
                {
                    mon_type = random2(NUM_MONSTERS);
                    count++;
                }
                while (mons_rarity(mon_type) == 0 && count < 2000);

                if (count == 2000)
                    return (false);

                level  = mons_level( mon_type );
                diff   = level - lev_mons;
                chance = mons_rarity( mon_type ) - (diff * diff);

                if ((lev_mons >= level - 5 && lev_mons <= level + 5)
                    && random2avg(100, 2) <= chance)
                {
                    break;
                }
            }

            if (i == 10000)
                return (false);
        }
    }

    // (3) decide on banding (good lord!)
    band_size = 1;
    band_monsters[0] = mon_type;

    if (allow_bands)
    {
        int band = choose_band(mon_type, power, band_size);
        band_size ++;

        for (i = 1; i < band_size; i++)
            band_monsters[i] = band_member( band, power );
    }

    // Monsters that can't move shouldn't be taking the stairs -- bwr
    if (proximity == PROX_NEAR_STAIRS && mons_speed( mon_type ) == 0)
        proximity = PROX_AWAY_FROM_PLAYER;

    // (4) for first monster, choose location.  This is pretty intensive.
    bool proxOK;
    bool close_to_player;

    // player shoved out of the way?
    bool shoved = false;
    unsigned char stair_gfx = 0;
    int pval = 0;

    if (!summoned)
    {
        int tries = 0;
        // try to pick px, py that is
        // a) not occupied
        // b) compatible
        // c) in the 'correct' proximity to the player
        unsigned char grid_wanted = monster_habitat(mon_type);
        while(true)
        {
            tries ++;
            // give up on stair placement?
            if (proximity == PROX_NEAR_STAIRS)
            {
                if (tries > 320)
                {
                    proximity = PROX_AWAY_FROM_PLAYER;
                    tries = 0;
                }
            }
            else if (tries > 60) 
                return (false);

            px = 5 + random2(GXM - 10);
            py = 5 + random2(GYM - 10);

            // occupied?
            if (mgrd[px][py] != NON_MONSTER
                || (px == you.x_pos && py == you.y_pos))
            {
                continue;
            }

            // compatible - floor?
            if (grid_wanted == DNGN_FLOOR && grd[px][py] < DNGN_FLOOR)
                continue;

            // compatible - others (must match, except for deep water monsters
            // generated in shallow water)
            if ((grid_wanted != DNGN_FLOOR && grd[px][py] != grid_wanted)
                && (grid_wanted != DNGN_DEEP_WATER || grd[px][py] != DNGN_SHALLOW_WATER))
            {
                continue;
            }

            // don't generate monsters on top of teleport traps
            // (how did they get there?)
            int trap = trap_at_xy(px, py);
            if (trap >= 0)
            {
                if (env.trap[trap].type == TRAP_TELEPORT)
                    continue;
            }

            // check proximity to player
            proxOK = true;

            switch (proximity)
            {
                case PROX_ANYWHERE:
                    if (grid_distance( you.x_pos, you.y_pos, px, py ) < 2 + random2(3))
                    {
                        proxOK = false;
                    }
                    break;

                case PROX_CLOSE_TO_PLAYER:
                case PROX_AWAY_FROM_PLAYER:
                    close_to_player = (distance(you.x_pos, you.y_pos, px, py) < 64);

                    if ((proximity == PROX_CLOSE_TO_PLAYER && !close_to_player)
                        || (proximity == PROX_AWAY_FROM_PLAYER && close_to_player))
                    {
                        proxOK = false;
                    }
                    break;

                case PROX_NEAR_STAIRS:
                    pval = near_stairs(px, py, 1, stair_gfx);
                    if (pval == 2)
                    {
                        // 0 speed monsters can't shove player out of their way.
                        if (mons_speed(mon_type) == 0)
                        {
                            proxOK = false;
                            break;
                        }
                        // swap the monster and the player spots,  unless the
                        // monster was generated in lava or deep water.
                        if (grd[px][py] == DNGN_LAVA || grd[px][py] == DNGN_DEEP_WATER)
                        {
                            proxOK = false;
                            break;
                        }
                        shoved = true;
                        int tpx = px;
                        int tpy = py;
                        px = you.x_pos;
                        py = you.y_pos;
                        you.x_pos = tpx;
                        you.y_pos = tpy;
                    }

                    proxOK = (pval > 0);
                    break;
                default:
                    break;
            }

            if (!proxOK)
                continue;

            // cool.. passes all tests
            break;
        } // end while.. place first monster
    }

    id = place_monster_aux( mon_type, behaviour, target, px, py, lev_mons, 
                            extra, true );

    // now, forget about banding if the first placement failed,  or there's too
    // many monsters already,  or we successfully placed by stairs
    if (id < 0 || id+30 > MAX_MONSTERS)
        return false;

    // message to player from stairwell/gate appearance?
    if (see_grid(px, py) && proximity == PROX_NEAR_STAIRS)
    {
        info[0] = '\0';

        if (player_monster_visible( &menv[id] ))
            strcpy(info, ptr_monam( &menv[id], DESC_CAP_A ));
        else if (shoved)
            strcpy(info, "Something");

        if (shoved)
        {
            strcat(info, " shoves you out of the ");
            strcat(info, (stair_gfx == '>' || stair_gfx == '<') ? "stairwell!"
                                                                : "gateway!");
            mpr(info);
        }
        else if (info[0] != '\0')
        {
            strcat(info, (stair_gfx == '>') ? " comes up the stairs." :
                         (stair_gfx == '<') ? " comes down the stairs."
                                            : " comes through the gate.");
            mpr(info);
        }

        // special case: must update the view for monsters created in player LOS
        viewwindow(1, false);
    }

    // monsters placed by stairs don't bring the whole band up/down/through
    // with them
    if (proximity == PROX_NEAR_STAIRS)
        return (true);

    // (5) for each band monster, loop call to place_monster_aux().
    for(i = 1; i < band_size; i++)
    {
        place_monster_aux( band_monsters[i], behaviour, target, px, py, 
                           lev_mons, extra, false );
    }

    // placement of first monster, at least, was a success.
    return (true);
}

static int place_monster_aux( int mon_type, char behaviour, int target,
                              int px, int py, int power, int extra,
                              bool first_band_member )
{
    int id, i;
    char grid_wanted;
    int fx=0, fy=0;     // final x,y

    // gotta be able to pick an ID
    for (id = 0; id < MAX_MONSTERS; id++)
    {
        if (menv[id].type == -1)
            break;
    }

    if (id == MAX_MONSTERS)
        return -1;

    // scrap monster inventory
    for (i = 0; i < NUM_MONSTER_SLOTS; i++)
        menv[id].inv[i] = NON_ITEM;

    // scrap monster enchantments
    for (i = 0; i < NUM_MON_ENCHANTS; i++)
        menv[id].enchantment[i] = ENCH_NONE;

    // setup habitat and placement
    if (first_band_member)
    {
        fx = px;
        fy = py;
    }
    else
    {
        grid_wanted = monster_habitat(mon_type);

        // we'll try 1000 times for a good spot
        for (i = 0; i < 1000; i++)
        {
            fx = px + random2(7) - 3;
            fy = py + random2(7) - 3;

            // occupied?
            if (mgrd[fx][fy] != NON_MONSTER)
                continue;

            // compatible - floor?
            if (grid_wanted == DNGN_FLOOR && grd[fx][fy] < DNGN_FLOOR)
                continue;

            // compatible - others (must match, except for deep water monsters
            // generated in shallow water)
            if ((grid_wanted != DNGN_FLOOR && grd[fx][fy] != grid_wanted)
                && (grid_wanted != DNGN_DEEP_WATER || grd[fx][fy] != DNGN_SHALLOW_WATER))
                continue;

            // don't generate monsters on top of teleport traps
            // (how do they get there?)
            int trap = trap_at_xy(fx, fy);
            if (trap >= 0)
                if (env.trap[trap].type == TRAP_TELEPORT)
                    continue;

            // cool.. passes all tests
            break;
        }

        // did we really try 1000 times?
        if (i == 1000)
            return (-1);
    }

    // now, actually create the monster (wheeee!)
    menv[id].type = mon_type;
    menv[id].number = 250;

    menv[id].x = fx;
    menv[id].y = fy;

    // link monster into monster grid
    mgrd[fx][fy] = id;

    // generate a brand shiny new monster, or zombie
    if (mon_type == MONS_ZOMBIE_SMALL
        || mon_type == MONS_ZOMBIE_LARGE
        || mon_type == MONS_SIMULACRUM_SMALL
        || mon_type == MONS_SIMULACRUM_LARGE
        || mon_type == MONS_SKELETON_SMALL
        || mon_type == MONS_SKELETON_LARGE
        || mon_type == MONS_SPECTRAL_THING)
    {
        define_zombie( id, extra, mon_type, power );
    }
    else
    {
        define_monster(id);
    }

    // The return of Boris is now handled in monster_die()...
    // not setting this for Boris here allows for multiple Borises
    // in the dungeon at the same time.  -- bwr
    if (mon_type >= MONS_TERENCE && mon_type <= MONS_BORIS)
        you.unique_creatures[mon_type - 280] = 1;

    if (extra != 250)
        menv[id].number = extra;

    if (mons_flag(mon_type, M_INVIS))
        mons_add_ench(&menv[id], ENCH_INVIS);

    if (mons_flag(mon_type, M_CONFUSED))
        mons_add_ench(&menv[id], ENCH_CONFUSION);

    if (mon_type == MONS_SHAPESHIFTER)
        mons_add_ench(&menv[id], ENCH_SHAPESHIFTER);

    if (mon_type == MONS_GLOWING_SHAPESHIFTER)
        mons_add_ench(&menv[id], ENCH_GLOWING_SHAPESHIFTER);

    if (mon_type == MONS_GIANT_BAT || mon_type == MONS_UNSEEN_HORROR
        || mon_type == MONS_GIANT_BLOWFLY)
    {
        menv[id].flags |= MF_BATTY;
    }

    if ((mon_type == MONS_BIG_FISH
            || mon_type == MONS_GIANT_GOLDFISH
            || mon_type == MONS_ELECTRICAL_EEL
            || mon_type == MONS_JELLYFISH
            || mon_type == MONS_WATER_ELEMENTAL
            || mon_type == MONS_SWAMP_WORM)
        && grd[fx][fy] == DNGN_DEEP_WATER
        && !one_chance_in(5))
    {
        mons_add_ench( &menv[id], ENCH_SUBMERGED );
    }

    if ((mon_type == MONS_LAVA_WORM
            || mon_type == MONS_LAVA_FISH
            || mon_type == MONS_LAVA_SNAKE
            || mon_type == MONS_SALAMANDER)
        && grd[fx][fy] == DNGN_LAVA
        && !one_chance_in(5))
    {
        mons_add_ench( &menv[id], ENCH_SUBMERGED );
    }

    menv[id].flags |= MF_JUST_SUMMONED;

    if (mon_type == MONS_DANCING_WEAPON && extra != 1) // ie not from spell
    {
        give_item( id, power );

        if (menv[id].inv[MSLOT_WEAPON] != NON_ITEM)
            menv[id].number = mitm[ menv[id].inv[MSLOT_WEAPON] ].colour;
    }
    else if (mons_itemuse(mon_type) >= MONUSE_STARTING_EQUIPMENT)
    {
        give_item( id, power );

        // Give these monsters a second weapon -- bwr
        if (mon_type == MONS_TWO_HEADED_OGRE || mon_type == MONS_ETTIN)
        {
            give_item( id, power );
        }
    }


    // give manticores 8 to 16 spike volleys.
    // they're not spellcasters so this doesn't screw anything up.
    if (mon_type == MONS_MANTICORE)
        menv[id].number = 8 + random2(9);

    // set attitude, behaviour and target
    menv[id].attitude = ATT_HOSTILE;
    menv[id].behaviour = behaviour;
    menv[id].foe_memory = 0;

    // setting attitude will always make the
    // monster wander.. if you want sleeping
    // hostiles,  use BEH_SLEEP since the default
    // attitude is hostile.
    if (behaviour > NUM_BEHAVIOURS)
    {
        if (behaviour == BEH_FRIENDLY || behaviour == BEH_GOD_GIFT)
            menv[id].attitude = ATT_FRIENDLY;

        menv[id].behaviour = BEH_WANDER;
    }

    menv[id].foe = target;

    return (id);
}                               // end place_monster_aux()


static int choose_band( int mon_type, int power, int &band_size )
{
    // init
    band_size = 0;
    int band = BAND_NO_BAND;

    switch (mon_type)
    {
    case MONS_ORC:
        if (coinflip())
            break;
        // intentional fall-through {dlb}
    case MONS_ORC_WARRIOR:
        band = BAND_ORCS;       // orcs
        band_size = 2 + random2(3);
        break;

    case MONS_BIG_KOBOLD:
        if (power > 3)
        {
            band = BAND_KOBOLDS;
            band_size = 2 + random2(6);
        }
        break;

    case MONS_ORC_WARLORD:
        band_size = 5 + random2(5);   // warlords have large bands
        // intentional fall through
    case MONS_ORC_KNIGHT:
        band = BAND_ORC_KNIGHT;       // orcs + knight
        band_size += 3 + random2(4);
        break;

    case MONS_KILLER_BEE:
        band = BAND_KILLER_BEES;       // killer bees
        band_size = 2 + random2(4);
        break;

    case MONS_FLYING_SKULL:
        band = BAND_FLYING_SKULLS;       // flying skulls
        band_size = 2 + random2(4);
        break;
    case MONS_SLIME_CREATURE:
        band = BAND_SLIME_CREATURES;       // slime creatures
        band_size = 2 + random2(4);
        break;
    case MONS_YAK:
        band = BAND_YAKS;       // yaks
        band_size = 2 + random2(4);
        break;
    case MONS_UGLY_THING:
        band = BAND_UGLY_THINGS;       // ugly things
        band_size = 2 + random2(4);
        break;
    case MONS_HELL_HOUND:
        band = BAND_HELL_HOUNDS;       // hell hound
        band_size = 2 + random2(3);
        break;
    case MONS_JACKAL:
        band = BAND_JACKALS;      // jackal
        band_size = 1 + random2(3);
        break;
    case MONS_HELL_KNIGHT:
    case MONS_MARGERY:
        band = BAND_HELL_KNIGHTS;      // hell knight
        band_size = 4 + random2(4);
        break;
    case MONS_JOSEPHINE:
    case MONS_NECROMANCER:
    case MONS_VAMPIRE_MAGE:
        band = BAND_NECROMANCER;      // necromancer
        band_size = 4 + random2(4);
        break;
    case MONS_ORC_HIGH_PRIEST:
        band = BAND_ORC_HIGH_PRIEST;      // orc high priest
        band_size = 4 + random2(4);
        break;
    case MONS_GNOLL:
        band = BAND_GNOLLS;      // gnoll
        band_size = ((coinflip())? 3 : 2);
        break;
    case MONS_BUMBLEBEE:
        band = BAND_BUMBLEBEES;      // bumble bees
        band_size = 2 + random2(4);
        break;
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        if (power > 9 && one_chance_in(3))
        {
            band = BAND_CENTAURS;  // centaurs
            band_size = 2 + random2(4);
        }
        break;

    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        if (coinflip())
        {
            band = BAND_YAKTAURS;  // yaktaurs
            band_size = 2 + random2(3);
        }
        break;

    case MONS_DEATH_YAK:
        band = BAND_DEATH_YAKS;      // death yaks
        band_size = 2 + random2(4);
        break;
    case MONS_INSUBSTANTIAL_WISP:
        band = BAND_INSUBSTANTIAL_WISPS;      // wisps
        band_size = 4 + random2(5);
        break;
    case MONS_OGRE_MAGE:
        band = BAND_OGRE_MAGE;      // ogre mage
        band_size = 4 + random2(4);
        break;
    case MONS_BALRUG:
        band = BAND_BALRUG;      // RED gr demon
        band_size = 2 + random2(3);
        break;
    case MONS_CACODEMON:
        band = BAND_CACODEMON;      // BROWN gr demon
        band_size = 1 + random2(3);
        break;

    case MONS_EXECUTIONER:
        if (coinflip())
        {
            band = BAND_EXECUTIONER;  // DARKGREY gr demon
            band_size = 1 + random2(3);
        }
        break;

    case MONS_HELLWING:
        if (coinflip())
        {
            band = BAND_HELLWING;  // LIGHTGREY gr demon
            band_size = 1 + random2(4);
        }
        break;

    case MONS_DEEP_ELF_FIGHTER:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_FIGHTER;  // deep elf warrior
            band_size = 3 + random2(4);
        }
        break;

    case MONS_DEEP_ELF_KNIGHT:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_KNIGHT;  // deep elf knight
            band_size = 3 + random2(4);
        }
        break;

    case MONS_DEEP_ELF_HIGH_PRIEST:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_HIGH_PRIEST;  // deep elf high priest
            band_size = 3 + random2(4);
        }
        break;

    case MONS_KOBOLD_DEMONOLOGIST:
        if (coinflip())
        {
            band = BAND_KOBOLD_DEMONOLOGIST;  // kobold demonologist
            band_size = 3 + random2(6);
        }
        break;

    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
        band = BAND_NAGAS;      // Nagas
        band_size = 3 + random2(4);
        break;
    case MONS_WAR_DOG:
        band = BAND_WAR_DOGS;      // war dogs
        band_size = 2 + random2(4);
        break;
    case MONS_GREY_RAT:
        band = BAND_GREY_RATS;      // grey rats
        band_size = 4 + random2(6);
        break;
    case MONS_GREEN_RAT:
        band = BAND_GREEN_RATS;      // green rats
        band_size = 4 + random2(6);
        break;
    case MONS_ORANGE_RAT:
        band = BAND_ORANGE_RATS;      // orange rats
        band_size = 3 + random2(4);
        break;
    case MONS_SHEEP:
        band = BAND_SHEEP;      // sheep
        band_size = 3 + random2(5);
        break;
    case MONS_GHOUL:
        band = BAND_GHOULS;      // ghoul
        band_size = 2 + random2(3);
        break;
    case MONS_HOG:
        band = BAND_HOGS;      // hog
        band_size = 1 + random2(3);
        break;
    case MONS_GIANT_MOSQUITO:
        band = BAND_GIANT_MOSQUITOES;      // mosquito
        band_size = 1 + random2(3);
        break;
    case MONS_DEEP_TROLL:
        band = BAND_DEEP_TROLLS;      // deep troll
        band_size = 3 + random2(3);
        break;
    case MONS_HELL_HOG:
        band = BAND_HELL_HOGS;      // hell-hog
        band_size = 1 + random2(3);
        break;
    case MONS_BOGGART:
        band = BAND_BOGGARTS;      // boggart
        band_size = 2 + random2(3);
        break;
    case MONS_BLINK_FROG:
        band = BAND_BLINK_FROGS;      // blink frog
        band_size = 2 + random2(3);
        break;
    case MONS_SKELETAL_WARRIOR:
        band = BAND_SKELETAL_WARRIORS;      // skeletal warrior
        band_size = 2 + random2(3);
        break;
    } // end switch

    if (band != BAND_NO_BAND && band_size == 0)
        band = BAND_NO_BAND;

    if (band_size >= BIG_BAND)
        band_size = BIG_BAND-1;

    return (band);
}

static int band_member(int band, int power)
{
    int mon_type = -1;
    int temp_rand;

    if (band == BAND_NO_BAND)
        return -1;

    switch (band)
    {
    case BAND_KOBOLDS:
        mon_type = MONS_KOBOLD;
        break;

    case BAND_ORC_KNIGHT:
    case BAND_ORC_HIGH_PRIEST:
        temp_rand = random2(30);
        mon_type = ((temp_rand > 17) ? MONS_ORC :          // 12 in 30
                    (temp_rand >  8) ? MONS_ORC_WARRIOR :  //  9 in 30
                    (temp_rand >  6) ? MONS_WARG :         //  2 in 30
                    (temp_rand >  4) ? MONS_ORC_WIZARD :   //  2 in 30
                    (temp_rand >  2) ? MONS_ORC_PRIEST :   //  2 in 30
                    (temp_rand >  1) ? MONS_OGRE :         //  1 in 30
                    (temp_rand >  0) ? MONS_TROLL          //  1 in 30
                                     : MONS_ORC_SORCERER); //  1 in 30
        break;

    case BAND_KILLER_BEES:
        mon_type = MONS_KILLER_BEE;
        break;

    case BAND_FLYING_SKULLS:
        mon_type = MONS_FLYING_SKULL;
        break;

    case BAND_SLIME_CREATURES:
        mon_type = MONS_SLIME_CREATURE;
        break;

    case BAND_YAKS:
        mon_type = MONS_YAK;
        break;

    case BAND_UGLY_THINGS:
        mon_type = ((power > 21 && one_chance_in(4)) ?
                                MONS_VERY_UGLY_THING : MONS_UGLY_THING);
        break;

    case BAND_HELL_HOUNDS:
        mon_type = MONS_HELL_HOUND;
        break;

    case BAND_JACKALS:
        mon_type = MONS_JACKAL;
        break;

    case BAND_GNOLLS:
        mon_type = MONS_GNOLL;
        break;

    case BAND_BUMBLEBEES:
        mon_type = MONS_BUMBLEBEE;
        break;

    case BAND_CENTAURS:
        mon_type = MONS_CENTAUR;
        break;

    case BAND_YAKTAURS:
        mon_type = MONS_YAKTAUR;
        break;

    case BAND_INSUBSTANTIAL_WISPS:
        mon_type = MONS_INSUBSTANTIAL_WISP;
        break;

    case BAND_DEATH_YAKS:
        mon_type = MONS_DEATH_YAK;
        break;

    case BAND_NECROMANCER:                // necromancer
        temp_rand = random2(13);
        mon_type = ((temp_rand > 9) ? MONS_ZOMBIE_SMALL :   // 3 in 13
                    (temp_rand > 6) ? MONS_ZOMBIE_LARGE :   // 3 in 13
                    (temp_rand > 3) ? MONS_SKELETON_SMALL : // 3 in 13
                    (temp_rand > 0) ? MONS_SKELETON_LARGE   // 3 in 13
                                    : MONS_NECROPHAGE);     // 1 in 13
        break;

    case BAND_BALRUG:
        mon_type = (coinflip()? MONS_NEQOXEC : MONS_ORANGE_DEMON);
        break;

    case BAND_CACODEMON:
        mon_type = MONS_LEMURE;
        break;

    case BAND_EXECUTIONER:
        mon_type = (coinflip() ? MONS_ABOMINATION_SMALL : MONS_ABOMINATION_LARGE);
        break;

    case BAND_HELLWING:
        mon_type = (coinflip() ? MONS_HELLWING : MONS_SMOKE_DEMON);
        break;

    case BAND_DEEP_ELF_FIGHTER:    // deep elf fighter
        temp_rand = random2(11);
        mon_type = ((temp_rand >  4) ? MONS_DEEP_ELF_SOLDIER : // 6 in 11
                    (temp_rand == 4) ? MONS_DEEP_ELF_FIGHTER : // 1 in 11
                    (temp_rand == 3) ? MONS_DEEP_ELF_KNIGHT :  // 1 in 11
                    (temp_rand == 2) ? MONS_DEEP_ELF_CONJURER :// 1 in 11
                    (temp_rand == 1) ? MONS_DEEP_ELF_MAGE      // 1 in 11
                                     : MONS_DEEP_ELF_PRIEST);  // 1 in 11
        break;

    case BAND_ORCS:
        mon_type = MONS_ORC;
        if (one_chance_in(5))
            mon_type = MONS_ORC_WIZARD;
        if (one_chance_in(7))
            mon_type = MONS_ORC_PRIEST;
        break;

    case BAND_HELL_KNIGHTS:
        mon_type = MONS_HELL_KNIGHT;
        if (one_chance_in(4))
            mon_type = MONS_NECROMANCER;
        break;

    //case 12 is orc high priest

    case BAND_OGRE_MAGE:
        mon_type = MONS_OGRE;
        if (one_chance_in(3))
            mon_type = MONS_TWO_HEADED_OGRE;
        break;                  // ogre mage

        // comment does not match value (30, TWO_HEADED_OGRE) prior to
        // enum replacement [!!!] 14jan2000 {dlb}

    case BAND_DEEP_ELF_KNIGHT:                    // deep elf knight
        temp_rand = random2(208);
        mon_type =
                ((temp_rand > 159) ? MONS_DEEP_ELF_SOLDIER :    // 23.08%
                 (temp_rand > 111) ? MONS_DEEP_ELF_FIGHTER :    // 23.08%
                 (temp_rand >  79) ? MONS_DEEP_ELF_KNIGHT :     // 15.38%
                 (temp_rand >  51) ? MONS_DEEP_ELF_MAGE :       // 13.46%
                 (temp_rand >  35) ? MONS_DEEP_ELF_PRIEST :     //  7.69%
                 (temp_rand >  19) ? MONS_DEEP_ELF_CONJURER :   //  7.69%
                 (temp_rand >   3) ? MONS_DEEP_ELF_SUMMONER :    // 7.69%
                 (temp_rand ==  3) ? MONS_DEEP_ELF_DEMONOLOGIST :// 0.48%
                 (temp_rand ==  2) ? MONS_DEEP_ELF_ANNIHILATOR : // 0.48%
                 (temp_rand ==  1) ? MONS_DEEP_ELF_SORCERER      // 0.48%
                                   : MONS_DEEP_ELF_DEATH_MAGE);  // 0.48%
        break;

    case BAND_DEEP_ELF_HIGH_PRIEST:                // deep elf high priest
        temp_rand = random2(16);
        mon_type =
                ((temp_rand > 12) ? MONS_DEEP_ELF_SOLDIER :     // 3 in 16
                 (temp_rand >  9) ? MONS_DEEP_ELF_FIGHTER :     // 3 in 16
                 (temp_rand >  6) ? MONS_DEEP_ELF_PRIEST :      // 3 in 16
                 (temp_rand == 6) ? MONS_DEEP_ELF_MAGE :        // 1 in 16
                 (temp_rand == 5) ? MONS_DEEP_ELF_SUMMONER :    // 1 in 16
                 (temp_rand == 4) ? MONS_DEEP_ELF_CONJURER :    // 1 in 16
                 (temp_rand == 3) ? MONS_DEEP_ELF_DEMONOLOGIST :// 1 in 16
                 (temp_rand == 2) ? MONS_DEEP_ELF_ANNIHILATOR : // 1 in 16
                 (temp_rand == 1) ? MONS_DEEP_ELF_SORCERER      // 1 in 16
                                  : MONS_DEEP_ELF_DEATH_MAGE);  // 1 in 16
        break;

    case BAND_KOBOLD_DEMONOLOGIST:
        temp_rand = random2(13);
        mon_type = ((temp_rand > 4) ? MONS_KOBOLD :             // 8 in 13
                    (temp_rand > 0) ? MONS_BIG_KOBOLD           // 4 in 13
                                    : MONS_KOBOLD_DEMONOLOGIST);// 1 in 13
        break;

    case BAND_NAGAS:
        mon_type = MONS_NAGA;
        break;
    case BAND_WAR_DOGS:
        mon_type = MONS_WAR_DOG;
        break;
    case BAND_GREY_RATS:
        mon_type = MONS_GREY_RAT;
        break;
    case BAND_GREEN_RATS:
        mon_type = MONS_GREEN_RAT;
        break;
    case BAND_ORANGE_RATS:
        mon_type = MONS_ORANGE_RAT;
        break;
    case BAND_SHEEP:
        mon_type = MONS_SHEEP;
        break;
    case BAND_GHOULS:
        mon_type = (coinflip() ? MONS_GHOUL : MONS_NECROPHAGE);
        break;
    case BAND_DEEP_TROLLS:
        mon_type = MONS_DEEP_TROLL;
        break;
    case BAND_HOGS:
        mon_type = MONS_HOG;
        break;
    case BAND_HELL_HOGS:
        mon_type = MONS_HELL_HOG;
        break;
    case BAND_GIANT_MOSQUITOES:
        mon_type = MONS_GIANT_MOSQUITO;
        break;
    case BAND_BOGGARTS:
        mon_type = MONS_BOGGART;
        break;
    case BAND_BLINK_FROGS:
        mon_type = MONS_BLINK_FROG;
        break;
    case BAND_SKELETAL_WARRIORS:
        mon_type = MONS_SKELETAL_WARRIOR;
        break;
    }

    return (mon_type);
}

// PUBLIC FUNCTION -- mons_place().

int mons_place( int mon_type, char behaviour, int target, bool summoned,
                int px, int py, int level_type, int proximity, int extra )
{
    int mon_count = 0;
    int temp_rand;          // probabilty determination {dlb}
    bool permit_bands = false;

    for (int il = 0; il < MAX_MONSTERS; il++)
    {
        if (menv[il].type != -1)
            mon_count++;
    }

    if (mon_type == WANDERING_MONSTER)
    {
        if (mon_count > 150)
            return (-1);

        mon_type = RANDOM_MONSTER;
    }

    // all monsters have been assigned? {dlb}
    if (mon_count > MAX_MONSTERS - 2)
        return (-1);

    // this gives a slight challenge to the player as they ascend the
    // dungeon with the Orb
    if (you.char_direction == DIR_ASCENDING && mon_type == RANDOM_MONSTER
        && you.level_type == LEVEL_DUNGEON)
    {
        temp_rand = random2(276);

        mon_type = ((temp_rand > 184) ? MONS_WHITE_IMP + random2(15) :  // 33.33%
                 (temp_rand > 104) ? MONS_HELLION + random2(10) :    // 28.99%
                 (temp_rand > 78)  ? MONS_HELL_HOUND :               //  9.06%
                 (temp_rand > 54)  ? MONS_ABOMINATION_LARGE :        //  8.70%
                 (temp_rand > 33)  ? MONS_ABOMINATION_SMALL :        //  7.61%
                 (temp_rand > 13)  ? MONS_RED_DEVIL                  //  7.25%
                                   : MONS_PIT_FIEND);                //  5.07%
    }

    if (mon_type == RANDOM_MONSTER || level_type == LEVEL_PANDEMONIUM)
        permit_bands = true;

    int mid = -1;

    // translate level_type
    int power;

    switch (level_type)
    {
        case LEVEL_PANDEMONIUM:
            power = 52;     // sigh..
            break;
        case LEVEL_ABYSS:
            power = 51;
            break;
        case LEVEL_DUNGEON: // intentional fallthrough
        default:
            power = you.your_level;
            break;
    }

    if (place_monster( mid, mon_type, power, behaviour, target, summoned,
                       px, py, permit_bands, proximity, extra ) == false)
    {
        return (-1);
    }

    if (mid != -1)
    {
        struct monsters *const creation = &menv[mid];

        // look at special cases: CHARMED, FRIENDLY, HOSTILE, GOD_GIFT
        // alert summoned being to player's presence
        if (behaviour > NUM_BEHAVIOURS)
        {
            if (behaviour == BEH_FRIENDLY || behaviour == BEH_GOD_GIFT)
                creation->flags |= MF_CREATED_FRIENDLY;

            if (behaviour == BEH_GOD_GIFT)
                creation->flags |= MF_GOD_GIFT;

            if (behaviour == BEH_CHARMED)
            {
                creation->attitude = ATT_HOSTILE;
                mons_add_ench(creation, ENCH_CHARM);
            }

            // make summoned being aware of player's presence
            behaviour_event(creation, ME_ALERT, MHITYOU);
        }
    }

    return (mid);
}                               // end mons_place()


int create_monster( int cls, int dur, int beha, int cr_x, int cr_y,
                    int hitting, int zsec )
{
    int summd = -1;
    FixedVector < char, 2 > empty;

    unsigned char spcw = ((cls == RANDOM_MONSTER) ? DNGN_FLOOR 
                                                  : monster_habitat( cls ));

    empty[0] = 0;
    empty[1] = 0;

    // Might be better if we chose a space and tried to match the monster
    // to it in the case of RANDOM_MONSTER, that way if the target square
    // is surrounded by water of lava this function would work.  -- bwr 
    if (empty_surrounds( cr_x, cr_y, spcw, true, empty ))
    {
        summd = mons_place( cls, beha, hitting, true, empty[0], empty[1],
                            you.level_type, 0, zsec );
    }

    // determine whether creating a monster is successful (summd != -1) {dlb}:
    // then handle the outcome {dlb}:
    if (summd == -1)
    {
        if (see_grid( cr_x, cr_y ))
            mpr("You see a puff of smoke.");
    }
    else
    {
        struct monsters *const creation = &menv[summd];

        // dur should always be ENCH_ABJ_xx
        if (dur >= ENCH_ABJ_I && dur <= ENCH_ABJ_VI)
            mons_add_ench(creation, dur );

        // look at special cases: CHARMED, FRIENDLY, HOSTILE, GOD_GIFT
        // alert summoned being to player's presence
        if (beha > NUM_BEHAVIOURS)
        {
            if (beha == BEH_FRIENDLY || beha == BEH_GOD_GIFT)
                creation->flags |= MF_CREATED_FRIENDLY;

            if (beha == BEH_GOD_GIFT)
                creation->flags |= MF_GOD_GIFT;

            if (beha == BEH_CHARMED)
            {
                creation->attitude = ATT_HOSTILE;
                mons_add_ench(creation, ENCH_CHARM);
            }

            // make summoned being aware of player's presence
            behaviour_event(creation, ME_ALERT, MHITYOU);
        }

        if (creation->type == MONS_RAKSHASA_FAKE && !one_chance_in(3))
            mons_add_ench(creation, ENCH_INVIS);
    }

    // the return value is either -1 (failure of some sort)
    // or the index of the monster placed (if I read things right) {dlb}
    return (summd);
}                               // end create_monster()


bool empty_surrounds(int emx, int emy, unsigned char spc_wanted,
                     bool allow_centre, FixedVector < char, 2 > &empty)
{
    bool success;
    // assume all player summoning originates from player x,y
    bool playerSummon = (emx == you.x_pos && emy == you.y_pos);

    int xpos[25];   // good x pos
    int ypos[25];   // good y pos
    int good_count = 0;
    int count_x, count_y;

    char minx = -2;
    char maxx = 2;
    char miny = -2;
    char maxy = 2;

    for (count_x = minx; count_x <= maxx; count_x++)
    {
        for (count_y = miny; count_y <= maxy; count_y++)
        {
            success = false;

            if (!allow_centre && count_x == 0 && count_y == 0)
                continue;

            int tx = emx + count_x;
            int ty = emy + count_y;

            if (tx == you.x_pos && ty == you.y_pos)
                continue;

            if (mgrd[tx][ty] != NON_MONSTER)
                continue;

            // players won't summon out of LOS
            if (!see_grid(tx, ty) && playerSummon)
                continue;

            if (grd[tx][ty] == spc_wanted)
                success = true;

            // those seeking ground can stand in shallow water or better
            if (spc_wanted == DNGN_FLOOR && grd[tx][ty] >= DNGN_SHALLOW_WATER)
                success = true;

            // water monsters can be created in shallow as well as deep water
            if (spc_wanted == DNGN_DEEP_WATER && grd[tx][ty] == DNGN_SHALLOW_WATER)
                success = true;

            if (success)
            {
                // add point to list of good points
                xpos[good_count] = tx;
                ypos[good_count] = ty;
                good_count ++;
            }
        }                       // end "for count_y"
    }                           // end "for count_x"

    if (good_count > 0)
    {
        int pick = random2(good_count);
        empty[0] = xpos[pick];
        empty[1] = ypos[pick];
    }

    return (good_count > 0);
}                               // end empty_surrounds()

int summon_any_demon(char demon_class)
{
    int summoned;    // error trapping {dlb}
    int temp_rand;          // probability determination {dlb}

    switch (demon_class)
    {
    case DEMON_LESSER:
        temp_rand = random2(60);
        summoned = ((temp_rand > 49) ? MONS_IMP :        // 10 in 60
                    (temp_rand > 40) ? MONS_WHITE_IMP :  //  9 in 60
                    (temp_rand > 31) ? MONS_LEMURE :     //  9 in 60
                    (temp_rand > 22) ? MONS_UFETUBUS :   //  9 in 60
                    (temp_rand > 13) ? MONS_MANES :      //  9 in 60
                    (temp_rand > 4)  ? MONS_MIDGE        //  9 in 60
                                     : MONS_SHADOW_IMP); //  5 in 60
        break;

    case DEMON_COMMON:
        temp_rand = random2(3948);
        summoned = ((temp_rand > 3367) ? MONS_NEQOXEC :         // 14.69%
                    (temp_rand > 2787) ? MONS_ORANGE_DEMON :    // 14.69%
                    (temp_rand > 2207) ? MONS_HELLWING :        // 14.69%
                    (temp_rand > 1627) ? MONS_SMOKE_DEMON :     // 14.69%
                    (temp_rand > 1047) ? MONS_YNOXINUL :        // 14.69%
                    (temp_rand > 889)  ? MONS_RED_DEVIL :       //  4.00%
                    (temp_rand > 810)  ? MONS_HELLION :         //  2.00%
                    (temp_rand > 731)  ? MONS_ROTTING_DEVIL :   //  2.00%
                    (temp_rand > 652)  ? MONS_TORMENTOR :       //  2.00%
                    (temp_rand > 573)  ? MONS_REAPER :          //  2.00%
                    (temp_rand > 494)  ? MONS_SOUL_EATER :      //  2.00%
                    (temp_rand > 415)  ? MONS_HAIRY_DEVIL :     //  2.00%
                    (temp_rand > 336)  ? MONS_ICE_DEVIL :       //  2.00%
                    (temp_rand > 257)  ? MONS_BLUE_DEVIL :      //  2.00%
                    (temp_rand > 178)  ? MONS_BEAST :           //  2.00%
                    (temp_rand > 99)   ? MONS_IRON_DEVIL :      //  2.00%
                    (temp_rand > 49)   ? MONS_SUN_DEMON         //  1.26%
                                       : MONS_SHADOW_IMP);      //  1.26%
        break;

    case DEMON_GREATER:
        temp_rand = random2(1000);
        summoned = ((temp_rand > 868) ? MONS_CACODEMON :        // 13.1%
                    (temp_rand > 737) ? MONS_BALRUG :           // 13.1%
                    (temp_rand > 606) ? MONS_BLUE_DEATH :       // 13.1%
                    (temp_rand > 475) ? MONS_GREEN_DEATH :      // 13.1%
                    (temp_rand > 344) ? MONS_EXECUTIONER :      // 13.1%
                    (temp_rand > 244) ? MONS_FIEND :            // 10.0%
                    (temp_rand > 154) ? MONS_ICE_FIEND :        //  9.0%
                    (temp_rand > 73)  ? MONS_SHADOW_FIEND       //  8.1%
                                      : MONS_PIT_FIEND);        //  7.4%
        break;

    default:
        summoned = MONS_GIANT_ANT;      // this was the original behaviour {dlb}
        break;
    }

    return summoned;
}                               // end summon_any_demon()
