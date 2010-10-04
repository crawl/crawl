#ifndef MGEN_DATA_H
#define MGEN_DATA_H

#include "mgen_enum.h"
#include "player.h"

// Hash key for passing a weapon to be given to
// a dancing weapon.
const std::string TUKIMA_WEAPON = "tukima-weapon";

// A structure with all the data needed to whip up a new monster.
struct mgen_data
{
    // Monster type.
    monster_type    cls;

    // If the monster is zombie-like, or a specialised draconian, this
    // is the base monster that the monster is based on - should be
    // set to MONS_NO_MONSTER when not used.
    monster_type    base_type;

    // Determines the behaviour of the monster after it is generated. This
    // behaviour is an unholy combination of monster attitude
    // (friendly, hostile) and monster initial state (asleep, wandering).
    // XXX: Could use splitting up these aspects.
    beh_type        behaviour;

    // Who summoned this monster?  Important to know for death accounting
    // and the summon cap, if and when it goes in.  NULL is no summoner.
    actor*          summoner;

    // For summoned monsters, this is a measure of how long the summon will
    // hang around, on a scale of 1-6, 6 being longest. Use 0 for monsters
    // that aren't summoned.
    int             abjuration_duration;

    // For summoned monsters this is their type of summoning, either the
    // spell which summoned them or one of the values of the enumeration
    // mon_summon_type in mon-util.h.
    int             summon_type;

    // Where the monster will be created.
    coord_def       pos;

    // A grid feature to prefer when finding a place to create monsters.
    // For instance, using DNGN_FLOOR when placing flying monsters or
    // merfolk in the Shoals will force them to appear on land.
    // preferred_grid_feature will be ignored if it is incompatible with
    // the monster's native habitat (for instance, if trying to place
    // a big fish with preferred_grid_feature DNGN_FLOOR).
    dungeon_feature_type preferred_grid_feature;

    // The monster's foe, i.e. which monster it will want to attack. foe
    // may be an index into the monster array (0 - (MAX_MONSTERS-1)), or
    // it may be MHITYOU to indicate that the monster wants to attack the
    // player, or MHITNOT, to indicate that the monster has no foe and is
    // just wandering around.
    unsigned short  foe;

    // Generation flags from mgen_flag_type.
    unsigned        flags;

    // What god the monster worships, if any.  Used for monsters that
    // are god gifts, to indicate which god sent them, and by priest
    // monsters, to indicate whose priest they are.
    god_type        god;

    // The number of hydra heads, the number of manticore attack volleys,
    // or the number of merged slime creatures.
    //
    // Note: in older versions this field was used for both this and for
    // base_type.
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

    // XXX: Also rather hackish.
    int             hd;
    int             hp;

    // These flags will be appended to the monster's flags after placement.
    // These flags are MF_XXX, rather than MG_XXX flags.
    uint64_t extra_flags;

    // XXX: Rather hackish.
    std::string     mname;

    // This is used to account for non-actor summoners.  Blasted by an Ice
    // Fiend ... summoned by the effects of Hell.
    std::string     non_actor_summoner;

    // This can eventually be used to store relevant information.
    CrawlHashTable  props;

    mgen_data(monster_type mt = RANDOM_MONSTER,
              beh_type beh = BEH_HOSTILE,
              actor* sner = 0,
              int abj = 0,
              int st = 0,
              const coord_def &p = coord_def(-1, -1),
              unsigned short mfoe = MHITNOT,
              unsigned genflags = 0,
              god_type which_god = GOD_NO_GOD,
              monster_type base = MONS_NO_MONSTER,
              int monnumber = 0,
              int moncolour = BLACK,
              int monpower = you.absdepth0,
              proximity_type prox = PROX_ANYWHERE,
              level_area_type ltype = you.level_type,
              int mhd = 0, int mhp = 0,
              uint64_t extflags = 0,
              std::string monname = "",
              std::string nas = "")

        : cls(mt), base_type(base), behaviour(beh), summoner(sner),
          abjuration_duration(abj), summon_type(st), pos(p),
          preferred_grid_feature(DNGN_UNSEEN), foe(mfoe), flags(genflags),
          god(which_god), number(monnumber), colour(moncolour),
          power(monpower), proximity(prox), level_type(ltype), map_mask(0),
          hd(mhd), hp(mhp), extra_flags(extflags), mname(monname),
          non_actor_summoner(nas), props()
    {
        ASSERT(summon_type == 0 || (abj >= 1 && abj <= 6)
               || mt == MONS_BALL_LIGHTNING || mt == MONS_ORB_OF_DESTRUCTION);
    }

    bool permit_bands() const { return (flags & MG_PERMIT_BANDS); }
    bool force_place() const { return (flags & MG_FORCE_PLACE); }
    bool needs_patrol_point() const { return (flags & MG_PATROLLING); }

    // Is there a valid position set on this struct that we want to use
    // when placing the monster?
    bool use_position() const;

    bool summoned() const { return (abjuration_duration > 0); }

    static mgen_data sleeper_at(monster_type what,
                                const coord_def &where,
                                unsigned genflags = 0)
    {
        return mgen_data(what, BEH_SLEEP, 0, 0, 0, where, MHITNOT, genflags);
    }

    static mgen_data hostile_at(monster_type mt,
                                std::string nsummoner,
                                bool alert = false,
                                int abj = 0,
                                int st = 0,
                                const coord_def &p = coord_def(-1, -1),
                                unsigned genflags = 0,
                                god_type ngod = GOD_NO_GOD,
                                monster_type base = MONS_NO_MONSTER)

    {
        return mgen_data(mt, BEH_HOSTILE, 0, abj, st, p,
                         alert ? MHITYOU : MHITNOT,
                         genflags, ngod, base, 0, BLACK, you.absdepth0,
                         PROX_ANYWHERE, you.level_type, 0, 0, 0, "", nsummoner);
    }
};

#endif
