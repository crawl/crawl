/*
 *  File:       monplace.h
 *  Summary:    Functions used when placing monsters in the dungeon.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MONPLACE_H
#define MONPLACE_H

#include "enum.h"
#include "dungeon.h"
#include "FixVec.h"

enum band_type
{
    BAND_NO_BAND                = 0,
    BAND_KOBOLDS,
    BAND_ORCS,
    BAND_ORC_WARRIOR,
    BAND_ORC_KNIGHT,
    BAND_KILLER_BEES,           // 5
    BAND_FLYING_SKULLS,
    BAND_SLIME_CREATURES,
    BAND_YAKS,
    BAND_UGLY_THINGS,
    BAND_HELL_HOUNDS,           // 10
    BAND_JACKALS,
    BAND_HELL_KNIGHTS,
    BAND_ORC_HIGH_PRIEST,
    BAND_GNOLLS,                // 14
    // 15
    BAND_BUMBLEBEES             = 16,
    BAND_CENTAURS,
    BAND_YAKTAURS,
    BAND_INSUBSTANTIAL_WISPS,
    BAND_OGRE_MAGE,             // 20
    BAND_DEATH_YAKS,
    BAND_NECROMANCER,
    BAND_BALRUG,
    BAND_CACODEMON,
    BAND_EXECUTIONER,           // 25
    BAND_HELLWING,
    BAND_DEEP_ELF_FIGHTER,
    BAND_DEEP_ELF_KNIGHT,
    BAND_DEEP_ELF_HIGH_PRIEST,
    BAND_KOBOLD_DEMONOLOGIST,   // 30
    BAND_NAGAS,
    BAND_WAR_DOGS,
    BAND_GREY_RATS,
    BAND_GREEN_RATS,
    BAND_ORANGE_RATS,           // 35
    BAND_SHEEP,
    BAND_GHOULS,
    BAND_DEEP_TROLLS,
    BAND_HOGS,
    BAND_HELL_HOGS,             // 40
    BAND_GIANT_MOSQUITOES,
    BAND_BOGGARTS,
    BAND_BLINK_FROGS,
    BAND_SKELETAL_WARRIORS,
    BAND_DRACONIAN,             // 45
    BAND_PANDEMONIUM_DEMON,     // 46
    NUM_BANDS                   // always last
};

enum demon_class_type
{
    DEMON_LESSER,                      //    0: Class V
    DEMON_COMMON,                      //    1: Class II-IV
    DEMON_GREATER,                     //    2: Class I
    DEMON_RANDOM                       //    any of the above
};

enum holy_being_class_type
{
    HOLY_BEING_ZIN,                    //    0: Angel
    HOLY_BEING_TSO,                    //    1: Daeva
    HOLY_BEING_RANDOM                  //    any of the above
};

enum dragon_class_type
{
    DRAGON_LIZARD,
    DRAGON_DRACONIAN,
    DRAGON_DRAGON
};

enum proximity_type   // proximity to player to create monster
{
    PROX_ANYWHERE,
    PROX_CLOSE_TO_PLAYER,
    PROX_AWAY_FROM_PLAYER,
    PROX_NEAR_STAIRS
};

enum mgen_flag_type
{
    MG_PERMIT_BANDS = 0x01,
    MG_FORCE_PLACE  = 0x02,
    MG_FORCE_BEH    = 0x04,
    MG_PLAYER_MADE  = 0x08,
    MG_PATROLLING   = 0x10
};

// A structure with all the data needed to whip up a new monster.
struct mgen_data
{
    // Monster type.
    monster_type    cls;

    // If the monster is zombie-like, or a specialised draconian, this
    // is the base monster that the monster is based on - should be
    // set to MONS_PROGRAM_BUG when not used.
    monster_type    base_type;

    // Determines the behaviour of the monster after it is generated. This
    // behaviour is an unholy combination of monster attitude
    // (friendly, hostile) and monster initial state (asleep, wandering).
    // XXX: Could use splitting up these aspects.
    beh_type        behaviour;

    // For summoned monsters, this is a measure of how long the summon will
    // hang around, on a scale of 1-6, 6 being longest. Use 0 for monsters
    // that aren't summoned.
    int             abjuration_duration;

    // Where the monster will be created.
    coord_def       pos;

    // The monster's foe, i.e. which monster it will want to attack. foe
    // may be an index into the monster array (0 - (MAX_MONSTERS-1)), or
    // it may be MHITYOU to indicate that the monster wants to attack the
    // player, or MHITNOT, to indicate that the monster has no foe and is
    // just wandering around.
    unsigned short  foe;

    // Generation flags from mgen_flag_type.
    unsigned        flags;

    // What god the monster worships, if any.  This is currently only
    // used for monsters that are god gifts, to indicate which god sent
    // them.
    god_type        god;

    // The number of hydra heads or manticore attack volleys. Note:
    // in older version this field was used for both this and for base_type.
    int             number;

    // The colour of the monster.
    int             colour;

    // A measure of how powerful the generated monster should be (for
    // randomly chosen monsters), usually equal to the absolute depth
    // that the player is in the dungeon.
    int             power;

    // How close to or far from the player the monster should be created.
    // Is usually used only when the initial position (pos) is unspecified.
    proximity_type  proximity;

    // What place we're in, or pretending to be in, usually the place
    // the player is actually in.
    level_area_type level_type;

    // Some predefined vaults (aka maps) include flags to suppress random
    // generation of monsters. When generating monsters, this is a mask of
    // map flags to honour (such as MMT_NO_MONS to specify that we shouldn't
    // randomly generate a monster inside a map that doesn't want it). These
    // map flags are usually respected only when a dungeon level is being
    // constructed, since at future points vault information may no longer
    // be available (vault metadata is not preserved across game saves).
    unsigned        map_mask;

    mgen_data(monster_type mt = RANDOM_MONSTER,
              beh_type beh = BEH_HOSTILE,
              int abj = 0,
              const coord_def &p = coord_def(-1, -1),
              unsigned short mfoe = MHITNOT,
              unsigned monflags = 0,
              god_type which_god = GOD_NO_GOD,
              monster_type base = MONS_PROGRAM_BUG,
              int monnumber = 0,
              int moncolour = BLACK,
              int monpower = you.your_level,
              proximity_type prox = PROX_ANYWHERE,
              level_area_type ltype = you.level_type)

        : cls(mt), base_type(base), behaviour(beh),
          abjuration_duration(abj), pos(p), foe(mfoe), flags(monflags),
          god(which_god), number(monnumber), colour(moncolour),
          power(monpower), proximity(prox), level_type(ltype), map_mask(0)
    {
    }

    bool permit_bands() const { return (flags & MG_PERMIT_BANDS); }
    bool force_place() const { return (flags & MG_FORCE_PLACE); }
    bool needs_patrol_point() const { return (flags & MG_PATROLLING); }

    // Is there a valid position set on this struct that we want to use
    // when placing the monster?
    bool use_position() const { return in_bounds(pos); }

    bool summoned() const { return (abjuration_duration > 0); }

    static mgen_data sleeper_at(monster_type what,
                                const coord_def &where,
                                unsigned flags = 0)
    {
        return mgen_data(what, BEH_SLEEP, 0, where, MHITNOT, flags);
    }

    static mgen_data hostile_at(monster_type what,
                                const coord_def &where,
                                int abj_deg = 0,
                                unsigned flags = 0,
                                bool alert = false,
                                god_type god = GOD_NO_GOD)
    {
        return mgen_data(what, BEH_HOSTILE, abj_deg, where,
                         alert ? MHITYOU : MHITNOT,
                         flags, god);
    }
};

/* ***********************************************************************
 * Creates a monster near the place specified in the mgen_data, producing
 * a "puff of smoke" message if the monster cannot be placed. This is usually
 * used for summons and other monsters that want to appear near a given
 * position like a summon.
 * *********************************************************************** */
int create_monster( mgen_data mg, bool fail_msg = true );

/* ***********************************************************************
 * Primary function to create monsters. See mgen_data for details on monster
 * placement.
 * *********************************************************************** */
int mons_place( mgen_data mg );

/* ***********************************************************************
 * This isn't really meant to be a public function.  It is a low level
 * monster placement function used by dungeon building routines and
 * mons_place().  If you need to put a monster somewhere, use mons_place().
 * Summoned creatures can be created with create_monster().
 * *********************************************************************** */
int place_monster( mgen_data mg, bool force_pos = false );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - debug - decks - effects - fight - it_use3 - item_use -
 *              items - monstuff - mstuff2 - religion - spell - spells -
 *              spells2 - spells3 - spells4
 * *********************************************************************** */

class level_id;
monster_type pick_random_monster(const level_id &place,
                                 int power,
                                 int &lev_mons);

bool player_will_anger_monster(monster_type type, bool *holy = NULL,
                               bool *unholy = NULL, bool *lawful = NULL,
                               bool *antimagical = NULL);

bool player_will_anger_monster(monsters *mon, bool *holy = NULL,
                               bool *unholy = NULL, bool *lawful = NULL,
                               bool *antimagical = NULL);

bool player_angers_monster(monsters *mon);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: misc - monplace - spells3
 * *********************************************************************** */
bool empty_surrounds( int emx, int emy, dungeon_feature_type spc_wanted,
                      int radius,
                      bool allow_centre, FixedVector<char, 2>& empty );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - items - maps - mstuff2 - spell - spells
 * *********************************************************************** */
monster_type summon_any_demon(demon_class_type dct);

monster_type summon_any_holy_being(holy_being_class_type hbct);

monster_type summon_any_dragon(dragon_class_type dct);

bool drac_colour_incompatible(int drac, int colour);

/* ***********************************************************************
 * called from: monplace monstuff
 * *********************************************************************** */
void mark_interesting_monst(monsters* monster,
                            beh_type behaviour = BEH_SLEEP);

bool grid_compatible(dungeon_feature_type grid_wanted,
                     dungeon_feature_type actual_grid,
                     bool generation = false);
bool monster_habitable_grid(const monsters *m,
                            dungeon_feature_type actual_grid);
bool monster_habitable_grid(int monster_class,
                            dungeon_feature_type actual_grid,
                            int flies = -1,
                            bool paralysed = false);
bool monster_can_submerge(const monsters *mons, dungeon_feature_type grid);
coord_def find_newmons_square(int mons_class, const coord_def &p);
coord_def find_newmons_square_contiguous(monster_type mons_class,
                                         const coord_def &start,
                                         int maxdistance = 3);

void spawn_random_monsters();

class monster_pathfind
{
public:
    monster_pathfind();
    virtual ~monster_pathfind();

    // public methods
    void set_range(int r);
    bool start_pathfind(monsters *mon, coord_def dest, bool msg = false);
    std::vector<coord_def> backtrack(void);
    std::vector<coord_def> calc_waypoints(void);

protected:
    // protected methods
    bool calc_path_to_neighbours(void);
    bool traversable(coord_def p);
    int  travel_cost(coord_def npos);
    int  estimated_cost(coord_def npos);
    void add_new_pos(coord_def pos, int total);
    void update_pos(coord_def pos, int total);
    bool get_best_position(void);


    // The monster trying to find a path.
    monsters *mons;

    // Our destination, and the current position we're looking at.
    coord_def start, target, pos;

    // Maximum range to search between start and target. None, if zero.
    int range;

    // Currently shortest and longest possible total length of the path.
    int min_length;
    int max_length;

    // The array of distances from start to any already tried point.
    int dist[GXM][GYM];
    // An array to store where we came from on a given shortest path.
    int prev[GXM][GYM];

    FixedVector<std::vector<coord_def>, GXM * GYM> hash;
};

int retarded_branch_counting_function(int branchnumber);

#endif  // MONPLACE_H
