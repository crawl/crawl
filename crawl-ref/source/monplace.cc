/*
 *  File:       monplace.cc
 *  Summary:    Functions used when placing monsters in the dungeon.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "monplace.h"

#include "branch.h"
#include "externs.h"
#include "lev-pand.h"
#include "makeitem.h"
#include "monstuff.h"
#include "mon-pick.h"
#include "mon-util.h"
#include "player.h"
#include "stuff.h"
#include "spells4.h"
#include "terrain.h"
#include "traps.h"
#include "view.h"

// NEW place_monster -- note that power should be set to:
// 51 for abyss
// 52 for pandemonium
// x otherwise

// proximity is the same as for mons_place:
// 0 is no restrictions
// 1 attempts to place near player
// 2 attempts to avoid player LOS

#define BIG_BAND        20

static int band_member(band_type band, int power);
static band_type choose_band( int mon_type, int power, int &band_size );
static int place_monster_aux(int mon_type, beh_type behaviour, int target,
    int px, int py, int power, int extra, bool first_band_member,
    int dur = 0);

// Returns whether actual_grid is compatible with grid_wanted for monster 
// movement (or for monster generation, if generation is true).
bool grid_compatible(int grid_wanted, int actual_grid, bool generation)
{
    // XXX What in Xom's name is DNGN_WATER_STUCK? It looks like an artificial
    // device to slow down fiery monsters flying over water.
    if (grid_wanted == DNGN_FLOOR)
        return actual_grid >= DNGN_FLOOR 
            || (!generation 
                    && actual_grid == DNGN_SHALLOW_WATER);

    return (grid_wanted == actual_grid
            || (grid_wanted == DNGN_DEEP_WATER
                && (actual_grid == DNGN_SHALLOW_WATER
                    || actual_grid == DNGN_BLUE_FOUNTAIN)));
}

// Can this monster survive on actual_grid?
//
// If you have an actual monster, use this instead of the overloaded function
// that uses only the monster class to make decisions.
bool monster_habitable_grid(const monsters *m, int actual_grid)
{
    return (monster_habitable_grid(m->type, actual_grid, mons_flies(m),
                                   m->paralysed()));
}

inline static bool mons_airborne(int mcls, int flies, bool paralysed)
{
    if (flies == -1)
        flies = mons_class_flies(mcls);
    return (paralysed? flies == 2 : flies != 0);
}

// Can monsters of class monster_class live happily on actual_grid? Use flies
// == true to pretend the monster can fly.
//
// [dshaligram] We're trying to harmonise the checks from various places into
// one check, so we no longer care if a water elemental springs into existence
// on dry land, because they're supposed to be able to move onto dry land 
// anyway.
bool monster_habitable_grid(int monster_class, int actual_grid, int flies,
                            bool paralysed)
{
    const int preferred_habitat = monster_habitat(monster_class);
    return (grid_compatible(preferred_habitat, actual_grid)
            // [dshaligram] Flying creatures are all DNGN_FLOOR, so we
            // only have to check for the additional valid grids of deep
            // water and lava.
            || (mons_airborne(monster_class, flies, paralysed)
                && (actual_grid == DNGN_LAVA 
                    || actual_grid == DNGN_DEEP_WATER))

            // Amphibious critters are happy in the water.
            || (mons_class_flag(monster_class, M_AMPHIBIOUS)
                && grid_compatible(DNGN_DEEP_WATER, actual_grid))

            // And water elementals are native to the water but happy on land
            // as well.
            || (monster_class == MONS_WATER_ELEMENTAL
                && grid_compatible(DNGN_FLOOR, actual_grid)));
}

// Returns true if the monster can submerge in the given grid
bool monster_can_submerge(int monster_class, int grid)
{
    switch (monster_class)
    {
    case MONS_BIG_FISH:
    case MONS_GIANT_GOLDFISH:
    case MONS_ELECTRICAL_EEL:
    case MONS_JELLYFISH:
    case MONS_WATER_ELEMENTAL:
    case MONS_SWAMP_WORM:
        return (grid == DNGN_DEEP_WATER || grid == DNGN_BLUE_FOUNTAIN);

    case MONS_LAVA_WORM:
    case MONS_LAVA_FISH:
    case MONS_LAVA_SNAKE:
    case MONS_SALAMANDER:
        return (grid == DNGN_LAVA);

    default:
        return (false);
    }
}

static bool need_super_ood(int lev_mons)
{
    return (env.turns_on_level > 1400 - lev_mons * 117
            && one_chance_in(5000));
}

/*
static bool need_moderate_ood(int lev_mons)
{
    return (env.turns_on_level > 700 - lev_mons * 117? one_chance_in(40) :
            one_chance_in(50));
}
*/

static int fuzz_mons_level(int level)
{
    int fuzz = random2avg(11, 8);
    if (fuzz > 5)
        level += fuzz - 5;
    return (level);
}

static void hell_spawn_random_monsters()
{
    const int speedup_turn = 1339;
    
    // Monster generation in the Vestibule starts ratcheting up quickly
    // after speedup_turn turns spent in the Vestibule.
    int genodds = (you.char_direction == GDT_DESCENDING) ? 240 : 8;
    if (env.turns_on_level > speedup_turn)
    {
        genodds -= (env.turns_on_level - speedup_turn) / 12;
        if (genodds < 3)
            genodds = 3;
    }

    if (one_chance_in(genodds))
    {
        int distance_odds = 10;
        if (env.turns_on_level > speedup_turn)
            distance_odds -= (env.turns_on_level - speedup_turn) / 100;

        if (distance_odds < 2)
            distance_odds = 2;

        proximity_type prox =
            (one_chance_in(distance_odds) ? PROX_NEAR_STAIRS 
             : PROX_AWAY_FROM_PLAYER);
        mons_place( WANDERING_MONSTER, BEH_HOSTILE, MHITNOT, false,
                    50, 50, LEVEL_DUNGEON, prox );
        viewwindow(true, false);
    }
}

void spawn_random_monsters()
{
    if (player_in_branch(BRANCH_VESTIBULE_OF_HELL))
    {
        hell_spawn_random_monsters();
        return;
    }
    
    // place normal dungeon monsters,  but not in player LOS
    if (you.level_type == LEVEL_DUNGEON
        && !player_in_branch( BRANCH_ECUMENICAL_TEMPLE )
        && one_chance_in((you.char_direction == GDT_DESCENDING) ? 240 : 8))
    {
        proximity_type prox = (one_chance_in(10) ? PROX_NEAR_STAIRS 
                                                 : PROX_AWAY_FROM_PLAYER);

        // The rules change once the player has picked up the Orb...
        if (you.char_direction == GDT_ASCENDING)
            prox = (one_chance_in(6) ? PROX_CLOSE_TO_PLAYER : PROX_ANYWHERE);

        mons_place( WANDERING_MONSTER, BEH_HOSTILE, MHITNOT, false,
                    50, 50, LEVEL_DUNGEON, prox );
        viewwindow(true, false);
    }

    // place Abyss monsters.
    if (you.level_type == LEVEL_ABYSS && one_chance_in(5))
    {
        mons_place( WANDERING_MONSTER, BEH_HOSTILE, MHITNOT, false,
                    50, 50, LEVEL_ABYSS, PROX_ANYWHERE );
        viewwindow(true, false);
    }

    // place Pandemonium monsters
    if (you.level_type == LEVEL_PANDEMONIUM && one_chance_in(50))
    {
        pandemonium_mons();
        viewwindow(true, false);
    }

    // No monsters in the Labyrinth, or the Ecumenical Temple, or in Bazaars.
}

monster_type pick_random_monster(const level_id &place,
                                 int power,
                                 int &lev_mons)
{
    monster_type mon_type = MONS_PROGRAM_BUG;
    
    lev_mons = power;

    if (place.branch == BRANCH_MAIN_DUNGEON
        && lev_mons != 51 && one_chance_in(4))
    {
        lev_mons = random2(power);
    }

    if (place.branch == BRANCH_MAIN_DUNGEON
        && lev_mons < 28)
    {
        if (you.your_level)
            lev_mons = fuzz_mons_level(lev_mons);

        // potentially nasty surprise, but very rare
        if (need_super_ood(lev_mons))
            lev_mons += random2(12);

        // slightly out of depth monsters are more common:
        // [ds] Replaced with a fuzz above for a more varied mix.
        //if (need_moderate_ood(lev_mons))
        //    lev_mons += random2(5);

        if (lev_mons > 27)
            lev_mons = 27;
    }

    /* Abyss or Pandemonium. Almost never called from Pan;
       probably only if a rand demon gets summon anything spell */
    if (lev_mons == 51
        || place.level_type == LEVEL_PANDEMONIUM
        || place.level_type == LEVEL_ABYSS)
    {
        do
        {
            int count = 0;

            do
            {
                // was: random2(400) {dlb}
                mon_type = static_cast<monster_type>( random2(NUM_MONSTERS) );
                count++;
            }
            while (mons_abyss(mon_type) == 0 && count < 2000);

            if (count == 2000)
                return (MONS_PROGRAM_BUG);
        }
        while (random2avg(100, 2) > mons_rare_abyss(mon_type)
               && !one_chance_in(100));
    }
    else
    {
        int level, diff, chance;

        if (lev_mons > 30)
            lev_mons = 30;

        int i;
        for (i = 0; i < 10000; i++)
        {
            int count = 0;

            do
            {
                mon_type = static_cast<monster_type>(random2(NUM_MONSTERS));
                count++;
            }
            while (mons_rarity(mon_type, place) == 0 && count < 2000);

            if (count == 2000)
                return (MONS_PROGRAM_BUG);

            level  = mons_level( mon_type, place );
            diff   = level - lev_mons;
            chance = mons_rarity( mon_type, place ) - (diff * diff);

            if ((lev_mons >= level - 5 && lev_mons <= level + 5)
                && random2avg(100, 2) <= chance)
            {
                break;
            }
        }

        if (i == 10000)
            return (MONS_PROGRAM_BUG);
    }
    return (mon_type);
}

bool place_monster(int &id, int mon_type, int power, beh_type behaviour,
                   int target, bool summoned, int px, int py, bool allow_bands,
                   proximity_type proximity, int extra, int dur,
                   unsigned mmask)
{
    int band_size = 0;
    int band_monsters[BIG_BAND];        // band monster types
    int lev_mons = power;               // final 'power'
    int i;
    
    unsigned char stair_gfx = 0;
    int tries = 0;
    int pval = 0;
    level_id place = level_id::current();

    // set initial id to -1  (unsuccessful create)
    id = -1;

    // (1) early out (summoned to occupied grid)
    if (summoned && mgrd[px][py] != NON_MONSTER)
        return (false);

    // (2) take care of random monsters
    if (mon_type == RANDOM_MONSTER)
    {
        // respect destination level for staircases
        if (proximity == PROX_NEAR_STAIRS)
        {
            while(++tries <= 320)
            {
                px = 5 + random2(GXM - 10);
                py = 5 + random2(GYM - 10);

                if (mgrd[px][py] != NON_MONSTER
                    || (px == you.x_pos && py == you.y_pos))
                {
                    continue;
                }

                // Is the grid verboten?
                if (!unforbidden( coord_def(px, py), mmask ))
                    continue;

                // don't generate monsters on top of teleport traps
                int trap = trap_at_xy(px, py);
                if (trap >= 0)
                {
                    if (env.trap[trap].type == TRAP_TELEPORT)
                       continue;
                }

                // check whether there's a stair
                // and whether it leads to another branch
                pval = near_stairs(px, py, 1, stair_gfx, place.branch);

                // no monsters spawned in the Temple
                if (branches[place.branch].id == BRANCH_ECUMENICAL_TEMPLE)
                    continue;

                // found a position near the stairs!
                if (pval > 0)
                    break;
            }
            if (tries > 320)
            {   // give up and try somewhere else
                proximity = PROX_AWAY_FROM_PLAYER;
            }
            else
            {
                if ( stair_gfx == '>' ) // deeper level
                    lev_mons++;
                else if (stair_gfx == '<') // higher level
                {
                    // monsters don't come from outside the dungeon
                    if (lev_mons <= 0)
                    {
                        proximity = PROX_AWAY_FROM_PLAYER;
                        // in that case lev_mons stays as it is
                    }
                    else
                        lev_mons--;
                }
//                mprf("branch := %d, level := %d",
//                     branches[place.branch].id, lev_mons+1);

            }
        } // end proximity check

        if (branches[place.branch].id == BRANCH_HALL_OF_BLADES)
            mon_type = MONS_DANCING_WEAPON;
        else
        {
            // now pick a monster of the given branch and level
            mon_type = pick_random_monster(place, lev_mons,
                                       lev_mons);

            if (mon_type == MONS_PROGRAM_BUG)
                return (false);
        }
    }

    // (3) decide on banding (good lord!)
    band_size = 1;
    band_monsters[0] = mon_type;

    if (allow_bands)
    {
        const band_type band = choose_band(mon_type, power, band_size);
        band_size ++;

        for (i = 1; i < band_size; i++)
            band_monsters[i] = band_member( band, power );
    }

    if (proximity == PROX_NEAR_STAIRS)
    {
        // for some cases disallow monsters on stairs
        if (mons_class_is_stationary( mon_type )
            || pval == 2 && (mons_speed(mon_type) == 0
               || grd[px][py] == DNGN_LAVA || grd[px][py] == DNGN_DEEP_WATER))
        {
            proximity = PROX_AWAY_FROM_PLAYER;
        }
    }

    // (4) for first monster, choose location.  This is pretty intensive.
    bool proxOK;
    bool close_to_player;

    // player shoved out of the way?
    bool shoved = false;

    if (!summoned)
    {
        tries = 0;
        
        // try to pick px, py that is
        // a) not occupied
        // b) compatible
        // c) in the 'correct' proximity to the player
        unsigned char grid_wanted = monster_habitat(mon_type);
        while(true)
        {
            // handled above, won't change anymore
            if (proximity == PROX_NEAR_STAIRS && tries)
            {
                proximity = PROX_AWAY_FROM_PLAYER;
                tries = 0;
            }
            // Dropped number of tries from 60.
            else if (tries >= 45)
                return (false);

            tries ++;
            
            // placement already decided for PROX_NEAR_STAIRS
            if (proximity != PROX_NEAR_STAIRS)
            {
                px = 5 + random2(GXM - 10);
                py = 5 + random2(GYM - 10);
            }
            
            // Let's recheck these even for PROX_NEAR_STAIRS, just in case
            
            // occupied?
            if (mgrd[px][py] != NON_MONSTER
                || (px == you.x_pos && py == you.y_pos))
            {
                continue;
            }

            // Is the monster happy where we want to put it?
            if (!grid_compatible(grid_wanted, grd[px][py], true))
                continue;

            // Is the grid verboten?
            if (!unforbidden( coord_def(px, py), mmask ))
                continue;

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
                    if (pval == 2) // player on stairs
                    {
                        // 0 speed monsters can't shove player out of their way.
                        if (mons_speed(mon_type) == 0)
                        {
                            proxOK = false;
                            break;
                        }
                        // swap the monster and the player spots, unless the
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
                        you.moveto(tpx, tpy);
                    }

                    proxOK = (pval > 0);
                    break;
            }

            if (!proxOK)
                continue;

            // cool.. passes all tests
            break;
        } // end while.. place first monster
    }

    id = place_monster_aux( mon_type, behaviour, target, px, py, lev_mons, 
                            extra, true);

    // now, forget about banding if the first placement failed, or there's too
    // many monsters already, or we successfully placed by stairs
    if (id < 0 || id+30 > MAX_MONSTERS)
        return false;

    // message to player from stairwell/gate appearance?
    if (see_grid(px, py) && proximity == PROX_NEAR_STAIRS)
    {
        std::string msg;

        if (player_monster_visible( &menv[id] ))
            msg = menv[id].name(DESC_CAP_A);
        else if (shoved)
            msg = "Something";

        if (shoved)
        {
            msg += " shoves you out of the ";
            if (stair_gfx == '>' || stair_gfx == '<')
                msg += "stairwell!";
            else
                msg += "gateway!";
            mpr(msg.c_str());
        }
        else if (!msg.empty())
        {
            if ( stair_gfx == '>' )
                msg += " comes up the stairs.";
            else if (stair_gfx == '<')
                msg += " comes down the stairs.";
            else
                msg += " comes through the gate.";
            mpr(msg.c_str());
        }

        // special case: must update the view for monsters created in player LOS
        viewwindow(1, false);
    }

    // monsters placed by stairs don't bring the whole band up/down/through
    // with them
    if (proximity == PROX_NEAR_STAIRS)
        return (true);

    // Not PROX_NEAR_STAIRS, so will it will be part of a band, if there
    // is any.
    if (band_size > 1)
        menv[id].flags |= MF_BAND_MEMBER;

    // (5) for each band monster, loop call to place_monster_aux().
    for(i = 1; i < band_size; i++)
    {
        id = place_monster_aux( band_monsters[i], behaviour, target, px, py,
                                lev_mons, extra, false, dur);
        menv[id].flags |= MF_BAND_MEMBER;
    }

    // placement of first monster, at least, was a success.
    return (true);
}

static int place_monster_aux( int mon_type, beh_type behaviour, int target,
                              int px, int py, int power, int extra,
                              bool first_band_member, int dur )
{
    int id, i;
    unsigned char grid_wanted;
    int fx=0, fy=0;     // final x,y

    // gotta be able to pick an ID
    for (id = 0; id < MAX_MONSTERS; id++)
    {
        if (menv[id].type == -1)
            break;
    }

    if (id == MAX_MONSTERS)
        return -1;

    menv[id].inv.init(NON_ITEM);
    // scrap monster enchantments
    menv[id].enchantments.clear();

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

            if (!grid_compatible(grid_wanted, grd[fx][fy], true))
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
    if (mons_is_unique(mon_type))
        you.unique_creatures[mon_type] = true;

    if (extra != 250)
        menv[id].number = extra;

    if (mons_class_flag(mon_type, M_INVIS))
        menv[id].add_ench(ENCH_INVIS);

    if (mons_class_flag(mon_type, M_CONFUSED))
        menv[id].add_ench(ENCH_CONFUSION);

    if (mon_type == MONS_SHAPESHIFTER)
        menv[id].add_ench(ENCH_SHAPESHIFTER);

    if (mon_type == MONS_GLOWING_SHAPESHIFTER)
        menv[id].add_ench(ENCH_GLOWING_SHAPESHIFTER);

    if (mon_type == MONS_GIANT_BAT || mon_type == MONS_UNSEEN_HORROR
        || mon_type == MONS_GIANT_BLOWFLY)
    {
        menv[id].flags |= MF_BATTY;
    }

    if (monster_can_submerge(mon_type, grd[fx][fy])
            && !one_chance_in(5))
        menv[id].add_ench(ENCH_SUBMERGED);
    
    menv[id].flags |= MF_JUST_SUMMONED;

    if (mon_type == MONS_DANCING_WEAPON && extra != 1) // ie not from spell
    {
        give_item( id, power );

        if (menv[id].inv[MSLOT_WEAPON] != NON_ITEM)
            menv[id].colour = mitm[ menv[id].inv[MSLOT_WEAPON] ].colour;
    }
    else if (mons_itemuse(mon_type) >= MONUSE_STARTING_EQUIPMENT)
    {
        give_item( id, power );
        // Give these monsters a second weapon -- bwr
        if (mons_wields_two_weapons(static_cast<monster_type>(mon_type)))
            give_item( id, power );

        unwind_var<int> save_speedinc(menv[id].speed_increment);
        menv[id].wield_melee_weapon(false);
    }

    // give manticores 8 to 16 spike volleys.
    // they're not spellcasters so this doesn't screw anything up.
    if (mon_type == MONS_MANTICORE)
        menv[id].number = 8 + random2(9);

    // set attitude, behaviour and target
    menv[id].attitude = ATT_HOSTILE;
    menv[id].behaviour = behaviour;

    if (mons_is_statue(mon_type))
        menv[id].behaviour = BEH_WANDER;

    menv[id].foe_memory = 0;

    // setting attitude will always make the
    // monster wander.. if you want sleeping
    // hostiles, use BEH_SLEEP since the default
    // attitude is hostile.
    if (behaviour > NUM_BEHAVIOURS)
    {
        if (behaviour == BEH_FRIENDLY || behaviour == BEH_GOD_GIFT)
            menv[id].attitude = ATT_FRIENDLY;

        if (behaviour == BEH_NEUTRAL)
            menv[id].attitude = ATT_NEUTRAL;

        menv[id].behaviour = BEH_WANDER;
    }

    // dur should always be 1-6 for monsters that can be abjured.
    if (dur >= 1 && dur <= 6)
        menv[id].add_ench( mon_enchant(ENCH_ABJ, dur) );

    menv[id].foe = target;

    // Initialise pandemonium demons
    if (menv[id].type == MONS_PANDEMONIUM_DEMON)
    {
        ghost_demon ghost;
        ghost.init_random_demon();
        menv[id].set_ghost(ghost);
        menv[id].pandemon_init();
    }

    mark_interesting_monst(&menv[id], behaviour);
    
    if (player_monster_visible(&menv[id]) && mons_near(&menv[id]))
        seen_monster(&menv[id]);

    // if summoned onto traps, directly affect monster
    menv[id].apply_location_effects();

    return (id);
}                               // end place_monster_aux()


static band_type choose_band( int mon_type, int power, int &band_size )
{
    // init
    band_size = 0;
    band_type band = BAND_NO_BAND;

    switch (mon_type)
    {
    case MONS_ORC:
        if (coinflip())
            break;
        // intentional fall-through {dlb}
    case MONS_ORC_WARRIOR:
        band = BAND_ORCS;
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
        band = BAND_KILLER_BEES;
        band_size = 2 + random2(4);
        break;

    case MONS_FLYING_SKULL:
        band = BAND_FLYING_SKULLS;
        band_size = 2 + random2(4);
        break;
    case MONS_SLIME_CREATURE:
        band = BAND_SLIME_CREATURES;
        band_size = 2 + random2(4);
        break;
    case MONS_YAK:
        band = BAND_YAKS;
        band_size = 2 + random2(4);
        break;
    case MONS_UGLY_THING:
        band = BAND_UGLY_THINGS;
        band_size = 2 + random2(4);
        break;
    case MONS_HELL_HOUND:
        band = BAND_HELL_HOUNDS;
        band_size = 2 + random2(3);
        break;
    case MONS_JACKAL:
        band = BAND_JACKALS;
        band_size = 1 + random2(3);
        break;
    case MONS_HELL_KNIGHT:
    case MONS_MARGERY:
        band = BAND_HELL_KNIGHTS;
        band_size = 4 + random2(4);
        break;
    case MONS_JOSEPHINE:
    case MONS_NECROMANCER:
    case MONS_VAMPIRE_MAGE:
        band = BAND_NECROMANCER;
        band_size = 4 + random2(4);
        break;
    case MONS_ORC_HIGH_PRIEST:
        band = BAND_ORC_HIGH_PRIEST;
        band_size = 4 + random2(4);
        break;
    case MONS_GNOLL:
        band = BAND_GNOLLS;
        band_size = ((coinflip())? 3 : 2);
        break;
    case MONS_BUMBLEBEE:
        band = BAND_BUMBLEBEES;
        band_size = 2 + random2(4);
        break;
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        if (power > 9 && one_chance_in(3))
        {
            band = BAND_CENTAURS;
            band_size = 2 + random2(4);
        }
        break;

    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        if (coinflip())
        {
            band = BAND_YAKTAURS;
            band_size = 2 + random2(3);
        }
        break;

    case MONS_DEATH_YAK:
        band = BAND_DEATH_YAKS;
        band_size = 2 + random2(4);
        break;
    case MONS_INSUBSTANTIAL_WISP:
        band = BAND_INSUBSTANTIAL_WISPS;
        band_size = 4 + random2(5);
        break;
    case MONS_OGRE_MAGE:
        band = BAND_OGRE_MAGE;
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

    case MONS_PANDEMONIUM_DEMON:
        band = BAND_PANDEMONIUM_DEMON;
        band_size = random_range(1, 3);
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
            band = BAND_DEEP_ELF_FIGHTER;
            band_size = 3 + random2(4);
        }
        break;

    case MONS_DEEP_ELF_KNIGHT:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_KNIGHT;
            band_size = 3 + random2(4);
        }
        break;

    case MONS_DEEP_ELF_HIGH_PRIEST:
        if (coinflip())
        {
            band = BAND_DEEP_ELF_HIGH_PRIEST;
            band_size = 3 + random2(4);
        }
        break;

    case MONS_KOBOLD_DEMONOLOGIST:
        if (coinflip())
        {
            band = BAND_KOBOLD_DEMONOLOGIST;
            band_size = 3 + random2(6);
        }
        break;

    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
        band = BAND_NAGAS;
        band_size = 3 + random2(4);
        break;
    case MONS_WAR_DOG:
        band = BAND_WAR_DOGS;
        band_size = 2 + random2(4);
        break;
    case MONS_GREY_RAT:
        band = BAND_GREY_RATS;
        band_size = 4 + random2(6);
        break;
    case MONS_GREEN_RAT:
        band = BAND_GREEN_RATS;
        band_size = 4 + random2(6);
        break;
    case MONS_ORANGE_RAT:
        band = BAND_ORANGE_RATS;
        band_size = 3 + random2(4);
        break;
    case MONS_SHEEP:
        band = BAND_SHEEP;
        band_size = 3 + random2(5);
        break;
    case MONS_GHOUL:
        band = BAND_GHOULS;
        band_size = 2 + random2(3);
        break;
    case MONS_HOG:
        band = BAND_HOGS;
        band_size = 1 + random2(3);
        break;
    case MONS_GIANT_MOSQUITO:
        band = BAND_GIANT_MOSQUITOES;
        band_size = 1 + random2(3);
        break;
    case MONS_DEEP_TROLL:
        band = BAND_DEEP_TROLLS;
        band_size = 3 + random2(3);
        break;
    case MONS_HELL_HOG:
        band = BAND_HELL_HOGS;
        band_size = 1 + random2(3);
        break;
    case MONS_BOGGART:
        band = BAND_BOGGARTS;
        band_size = 2 + random2(3);
        break;
    case MONS_BLINK_FROG:
        band = BAND_BLINK_FROGS;
        band_size = 2 + random2(3);
        break;
    case MONS_SKELETAL_WARRIOR:
        band = BAND_SKELETAL_WARRIORS;
        band_size = 2 + random2(3);
        break;
    case MONS_CYCLOPS:
        if ( one_chance_in(5) || player_in_branch(BRANCH_SHOALS) )
        {
            band = BAND_SHEEP;  // Odyssey reference
            band_size = 2 + random2(3);
        }
        break;
    case MONS_POLYPHEMUS:
        band = BAND_DEATH_YAKS;
        band_size = 3 + random2(3);
        break;

    // Journey -- Added Draconian Packs  
    case MONS_WHITE_DRACONIAN:
    case MONS_RED_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_PALE_DRACONIAN:
        if (power > 18 && one_chance_in(3) && you.level_type == LEVEL_DUNGEON) 
        {
            band = BAND_DRACONIAN;
            band_size = random_range(2, 4);
        }
        break;
    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_SHIFTER:
        if (power > 20 && you.level_type == LEVEL_DUNGEON)
        {
            band = BAND_DRACONIAN;
            band_size = random_range(3, 6);
        }
        break;
    case MONS_TIAMAT:
        band = BAND_DRACONIAN;
        // yup, scary
        band_size = random_range(3,6) + random_range(3,6) + 2;
        break;
    } // end switch

    if (band != BAND_NO_BAND && band_size == 0)
        band = BAND_NO_BAND;

    if (band_size >= BIG_BAND)
        band_size = BIG_BAND-1;

    return (band);
}

static int band_member(band_type band, int power)
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
        mon_type = (coinflip() ? MONS_ABOMINATION_SMALL
                               : MONS_ABOMINATION_LARGE);
        break;

    case BAND_PANDEMONIUM_DEMON:
        if (one_chance_in(7))
            mon_type = random_choose_weighted(50, MONS_LICH,
                                              10, MONS_ANCIENT_LICH,
                                              0);
        else if (one_chance_in(6))
            mon_type = random_choose_weighted(50, MONS_ABOMINATION_SMALL,
                                              40, MONS_ABOMINATION_LARGE,
                                              10, MONS_TENTACLED_MONSTROSITY,
                                              0);
        else
            mon_type =
                summon_any_demon(
                    static_cast<demon_class_type>(
                        random_choose_weighted(50, DEMON_COMMON,
                                               20, DEMON_GREATER,
                                               10, DEMON_RANDOM,
                                               0)));
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
    case BAND_DRACONIAN:
    {
        temp_rand = random2( (power < 24) ? 24 : 37 );
        mon_type =                  
                ((temp_rand > 35) ? MONS_DRACONIAN_CALLER :     // 1 in 34
                 (temp_rand > 33) ? MONS_DRACONIAN_KNIGHT :     // 2 in 34
                 (temp_rand > 31) ? MONS_DRACONIAN_MONK :       // 2 in 34
                 (temp_rand > 29) ? MONS_DRACONIAN_SHIFTER :    // 2 in 34
                 (temp_rand > 27) ? MONS_DRACONIAN_ANNIHILATOR :// 2 in 34
                 (temp_rand > 25) ? MONS_DRACONIAN_SCORCHER :   // 2 in 34
                 (temp_rand > 23) ? MONS_DRACONIAN_ZEALOT :     // 2 in 34
                 (temp_rand > 20) ? MONS_YELLOW_DRACONIAN :     // 3 in 34
                 (temp_rand > 17) ? MONS_GREEN_DRACONIAN :      // 3 in 34
                 (temp_rand > 14) ? MONS_BLACK_DRACONIAN :      // 3 in 34
                 (temp_rand > 11) ? MONS_WHITE_DRACONIAN :      // 3 in 34
                 (temp_rand >  8) ? MONS_PALE_DRACONIAN :       // 3 in 34
                 (temp_rand >  5) ? MONS_PURPLE_DRACONIAN :     // 3 in 34
                 (temp_rand >  2) ? MONS_MOTTLED_DRACONIAN :    // 3 in 34
                                    MONS_RED_DRACONIAN );       // 3 in 34
        break;
    }
    default:
        break;
    }

    return (mon_type);
}

static int ood_limit() {
    return Options.ood_interesting;
}

void mark_interesting_monst(struct monsters* monster, beh_type behaviour)
{
    bool interesting = false;
    
    // Unique monsters are always intersting
    if ( mons_is_unique(monster->type) )
        interesting = true;
    // If it's never going to attack us, then not interesting
    else if (behaviour == BEH_FRIENDLY || behaviour == BEH_GOD_GIFT)
        interesting = false;
    // Don't waste time on moname() if user isn't using this option
    else if ( Options.note_monsters.size() > 0 )
    {        
        const std::string iname = mons_type_name(monster->type, DESC_NOCAP_A);
        for (unsigned i = 0; i < Options.note_monsters.size(); ++i)
        {
            if (Options.note_monsters[i].matches(iname))
            {
                interesting = true;
                break;
            }    
        }
    }
    else if ( you.where_are_you == BRANCH_MAIN_DUNGEON &&
              you.level_type == LEVEL_DUNGEON &&
              mons_level(monster->type) >= you.your_level + ood_limit() &&
              mons_level(monster->type) < 99 &&
              !(monster->type >= MONS_EARTH_ELEMENTAL &&
                monster->type <= MONS_AIR_ELEMENTAL)
              && (!Options.safe_zero_exp ||
                  !mons_class_flag( monster->type, M_NO_EXP_GAIN )))
        interesting = true;

    if ( interesting )
        monster->flags |= MF_INTERESTING;
}

// PUBLIC FUNCTION -- mons_place().

static int pick_zot_exit_defender()
{
    if (one_chance_in(11))
        return (MONS_PANDEMONIUM_DEMON);
    
    const int temp_rand = random2(276);
    const int mon_type =
        ((temp_rand > 184) ? MONS_WHITE_IMP + random2(15) :  // 33.33%
         (temp_rand > 104) ? MONS_HELLION + random2(10) :    // 28.99%
         (temp_rand > 78)  ? MONS_HELL_HOUND :               //  9.06%
         (temp_rand > 54)  ? MONS_ABOMINATION_LARGE :        //  8.70%
         (temp_rand > 33)  ? MONS_ABOMINATION_SMALL :        //  7.61%
         (temp_rand > 13)  ? MONS_RED_DEVIL                  //  7.25%
                           : MONS_PIT_FIEND);                //  5.07%

    return (mon_type);
}

int mons_place( int mon_type, beh_type behaviour, int target, bool summoned,
                int px, int py, int level_type, proximity_type proximity,
                int extra, int dur, bool permit_bands )
{
    int mon_count = 0;
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
    if (you.char_direction == GDT_ASCENDING && mon_type == RANDOM_MONSTER
        && you.level_type == LEVEL_DUNGEON)
    {
        mon_type = pick_zot_exit_defender();
        permit_bands = true;
    }

    if (permit_bands 
            || mon_type == RANDOM_MONSTER 
            || level_type == LEVEL_PANDEMONIUM)
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
                       px, py, permit_bands, proximity, extra, dur ) == false)
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
                creation->add_ench(ENCH_CHARM);
            }

            // make summoned being aware of player's presence
            behaviour_event(creation, ME_ALERT, MHITYOU);
        }
    }

    return (mid);
}                               // end mons_place()

coord_def find_newmons_square(int mons_class, int x, int y)
{
    FixedVector < char, 2 > empty;
    coord_def pos(-1, -1);

    empty[0] = 0;
    empty[1] = 0;

    if (mons_class == WANDERING_MONSTER)
        mons_class = RANDOM_MONSTER;

    int spcw = ((mons_class == RANDOM_MONSTER)? DNGN_FLOOR 
                                              : monster_habitat( mons_class ));

    // Might be better if we chose a space and tried to match the monster
    // to it in the case of RANDOM_MONSTER, that way if the target square
    // is surrounded by water of lava this function would work.  -- bwr 
    if (empty_surrounds( x, y, spcw, 2, true, empty ))
    {
        pos.x = empty[0];
        pos.y = empty[1];
    }
    
    return (pos);
}

bool player_angers_monster(monsters *creation)
{
    // get the drawbacks, not the benefits...
    // (to prevent demon-scumming)
    if ( (you.religion == GOD_ZIN ||
          you.religion == GOD_SHINING_ONE ||
          you.religion == GOD_ELYVILON) &&
         mons_is_unholy(creation) )
    {
        if ( creation->attitude != ATT_HOSTILE )
        {
            creation->attitude = ATT_HOSTILE;
            if ( see_grid(creation->x, creation->y)
                 && player_monster_visible(creation) )
            {
                mprf("%s is enraged by your holy aura!",
                     creation->name(DESC_CAP_THE).c_str());
            }
        }
        return (true);
    }
    return (false);
}

int create_monster( int cls, int dur, beh_type beha, int cr_x, int cr_y,
                    int hitting, int zsec, bool permit_bands,
                    bool force_place, bool force_behaviour )
{
    int summd = -1;
    coord_def pos = find_newmons_square(cls, cr_x, cr_y);
    if (force_place && !grid_is_solid(grd[cr_x][cr_y])
        && mgrd[cr_x][cr_y] == NON_MONSTER)
    {
        pos.x = cr_x;
        pos.y = cr_y;
    }

    if (pos.x != -1 && pos.y != -1)
    {
        summd = mons_place( cls, beha, hitting, true, pos.x, pos.y,
                            you.level_type, PROX_ANYWHERE, zsec,
                            dur, permit_bands );
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
        if (dur >= 1 && dur <= 6)
            creation->add_ench( mon_enchant(ENCH_ABJ, dur) );

        // look at special cases: CHARMED, FRIENDLY, HOSTILE, GOD_GIFT
        // alert summoned being to player's presence
        if (beha > NUM_BEHAVIOURS)
        {
            if (beha == BEH_FRIENDLY || beha == BEH_GOD_GIFT)
                creation->flags |= MF_CREATED_FRIENDLY;

            if (beha == BEH_GOD_GIFT)
                creation->flags |= MF_GOD_GIFT;

            if (!force_behaviour && player_angers_monster(creation))
                beha = BEH_HOSTILE;
            
            if (beha == BEH_CHARMED)
            {
                creation->attitude = ATT_HOSTILE;
                creation->add_ench(ENCH_CHARM);
            }

            // make summoned being aware of player's presence
            behaviour_event(creation, ME_ALERT, MHITYOU);
        }

        if (creation->type == MONS_RAKSHASA_FAKE && !one_chance_in(3))
            creation->add_ench(ENCH_INVIS);
    }

    // the return value is either -1 (failure of some sort)
    // or the index of the monster placed (if I read things right) {dlb}
    return (summd);
}                               // end create_monster()


bool empty_surrounds(int emx, int emy, unsigned char spc_wanted,
                     int radius, bool allow_centre, 
                     FixedVector < char, 2 > &empty)
{
    bool success;
    // assume all player summoning originates from player x,y
    bool playerSummon = (emx == you.x_pos && emy == you.y_pos);

    int good_count = 0;
    int count_x, count_y;

    for (count_x = -radius; count_x <= radius; count_x++)
    {
        for (count_y = -radius; count_y <= radius; count_y++)
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

            // players won't summon out of LOS, or past transparent
            // walls.
            if (!see_grid_no_trans(tx, ty) && playerSummon)
                continue;

            if (grd[tx][ty] == spc_wanted)
                success = true;

            if (grid_compatible(spc_wanted, grd[tx][ty]))
                success = true;

            if (success && one_chance_in(++good_count))
            {
                // add point to list of good points
                empty[0] = tx;
                empty[1] = ty;
            }
        }                       // end "for count_y"
    }                           // end "for count_x"

    return (good_count > 0);
}                               // end empty_surrounds()

int summon_any_demon(demon_class_type demon_class)
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

monster_type rand_dragon( dragon_class_type type )
{
    monster_type  summoned = MONS_PROGRAM_BUG;
    int temp_rand;

    switch (type)
    {
    case DRAGON_LIZARD:
        temp_rand = random2(100);
        summoned = ((temp_rand > 80) ? MONS_SWAMP_DRAKE :
                    (temp_rand > 59) ? MONS_KOMODO_DRAGON : 
                    (temp_rand > 34) ? MONS_FIREDRAKE :
                    (temp_rand > 11) ? MONS_DEATH_DRAKE :
                                       MONS_DRAGON);
        break;

    case DRAGON_DRACONIAN:
        temp_rand = random2(70);
        summoned = ((temp_rand > 60) ? MONS_YELLOW_DRACONIAN :
                    (temp_rand > 50) ? MONS_BLACK_DRACONIAN :
                    (temp_rand > 40) ? MONS_PALE_DRACONIAN :
                    (temp_rand > 30) ? MONS_GREEN_DRACONIAN :
                    (temp_rand > 20) ? MONS_PURPLE_DRACONIAN :
                    (temp_rand > 10) ? MONS_RED_DRACONIAN
                                     : MONS_WHITE_DRACONIAN);
        break;

    case DRAGON_DRAGON:
        temp_rand = random2(90);
        summoned = ((temp_rand > 80) ? MONS_MOTTLED_DRAGON :
                    (temp_rand > 70) ? MONS_LINDWURM :
                    (temp_rand > 60) ? MONS_STORM_DRAGON :
                    (temp_rand > 50) ? MONS_MOTTLED_DRAGON :
                    (temp_rand > 40) ? MONS_STEAM_DRAGON :
                    (temp_rand > 30) ? MONS_DRAGON :
                    (temp_rand > 20) ? MONS_ICE_DRAGON :
                    (temp_rand > 10) ? MONS_SWAMP_DRAGON
                                     : MONS_SHADOW_DRAGON);
        break;

    default:
        break;
    }

    return (summoned);
}
