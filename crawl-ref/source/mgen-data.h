#pragma once

#include "beh-type.h"
#include "coord.h"
#include "god-type.h"
#include "mgen-enum.h"
#include "mon-flags.h"

// Hash key for passing a weapon to be given to
// a dancing weapon.
#define TUKIMA_WEAPON "tukima-weapon"
#define TUKIMA_POWER "tukima-power"

#define MGEN_NUM_HEADS "num_heads"
#define MGEN_BLOB_SIZE "blob_size"
#define MGEN_TENTACLE_CONNECT "tentacle_connect"
/// doesn't automatically perish over time (for pillars of salt, blocks of ice)
#define MGEN_NO_AUTO_CRUMBLE "no_auto_crumble"

// A structure with all the data needed to whip up a new monster.
struct mgen_data
{
    // Monster type.
    monster_type    cls;

    // Determines the behaviour of the monster after it is generated. This
    // behaviour is an unholy combination of monster attitude
    // (friendly, hostile) and monster initial state (asleep, wandering).
    // XXX: Could use splitting up these aspects.
    beh_type        behaviour;

    // Who summoned this monster?  Important to know for death accounting
    // and the summon cap, if and when it goes in. nullptr is no summoner.
    const actor*    summoner;

    // For summoned monsters, this is a measure of how long the summon will
    // hang around, on a scale of 1-6, 6 being longest. Use 0 for monsters
    // that aren't summoned.
    int             abjuration_duration;

    // For summoned monsters this is their type of summoning, either the
    // spell which summoned them or one of the values of the enumeration
    // mon_summon_type in mon-enum.h.
    int             summon_type;

    // Where the monster will be created.
    coord_def       pos;

    // The monster's foe, i.e. which monster it will want to attack. foe
    // may be an index into the monster array (0 - (MAX_MONSTERS-1)), or
    // it may be MHITYOU to indicate that the monster wants to attack the
    // player, or MHITNOT, to indicate that the monster has no foe and is
    // just wandering around.
    unsigned short  foe;

    // Generation flags.
    mgen_flags      flags;

    // What god the monster worships, if any. Used for monsters that
    // are god gifts, to indicate which god sent them, and by priest
    // monsters, to indicate whose priest they are.
    god_type        god;

    // If the monster is zombie-like, or a specialised draconian, this
    // is the base monster that the monster is based on - should be
    // set to MONS_NO_MONSTER when not used.
    monster_type    base_type;

    // The colour of the monster, or COLOUR_UNDEF for col:any
    int             colour;

    // How close to or far from the player the monster should be created.
    // Is usually used only when the initial position (pos) is unspecified.
    proximity_type  proximity;

    // What place we're in, or pretending to be in, usually the place
    // the player is actually in.
    level_id        place;

    int             hd;
    int             hp;

    // These flags will be appended to the monster's flags after placement.
    monster_flags_t extra_flags;

    string          mname;

    // This is used to account for non-actor summoners. Blasted by an Ice
    // Fiend ... summoned by the effects of Hell.
    string          non_actor_summoner;

    // This simply stores the initial shape-shifter type.
    monster_type    initial_shifter;

    // A grid feature to prefer when finding a place to create monsters.
    // For instance, using DNGN_FLOOR when placing flying monsters or
    // merfolk in the Shoals will force them to appear on land.
    // preferred_grid_feature will be ignored if it is incompatible with
    // the monster's native habitat (for instance, if trying to place
    // a electric eel with preferred_grid_feature DNGN_FLOOR).
    dungeon_feature_type preferred_grid_feature = DNGN_UNSEEN;

    // Some predefined vaults (aka maps) include flags to suppress random
    // generation of monsters. When generating monsters, this is a mask of
    // map flags to honour (such as MMT_NO_MONS to specify that we shouldn't
    // randomly generate a monster inside a map that doesn't want it). These
    // map flags are usually respected only when a dungeon level is being
    // constructed, since at future points vault information may no longer
    // be available (vault metadata is not preserved across game saves).
    unsigned        map_mask = 0;

    // This can eventually be used to store relevant information.
    CrawlHashTable  props;

    // Was this creature spawned after level generation.
    bool            is_spawn;

    mgen_data(monster_type mt = RANDOM_MONSTER,
              beh_type beh = BEH_HOSTILE,
              const coord_def &p = coord_def(-1, -1),
              unsigned short mfoe = MHITNOT,
              mgen_flags genflags = MG_NONE,
              god_type which_god = GOD_NO_GOD)

        : cls(mt), behaviour(beh), summoner(nullptr), abjuration_duration(0),
          summon_type(0), pos(p), foe(mfoe), flags(genflags), god(which_god),
          base_type(MONS_NO_MONSTER), colour(COLOUR_INHERIT),
          proximity(PROX_ANYWHERE), place(level_id::current()), hd(0), hp(0),
          extra_flags(MF_NO_FLAGS), mname(""), non_actor_summoner(""),
          initial_shifter(RANDOM_MONSTER), is_spawn(false)
    { }

    mgen_data &set_non_actor_summoner(string nas)
    {
        non_actor_summoner = nas;
        return *this;
    }

    mgen_data &set_place(level_id _place)
    {
        place = _place;
        return *this;
    }

    mgen_data &set_prox(proximity_type prox)
    {
        proximity = prox;
        return *this;
    }

    mgen_data &set_col(int col)
    {
        colour = col;
        return *this;
    }

    mgen_data &set_base(monster_type base)
    {
        base_type = base;
        return *this;
    }

    mgen_data &set_summoned(const actor* _summoner, int abjuration_dur,
                            int _summon_type, god_type _god = GOD_NO_GOD)
    {
        summoner = _summoner;
        abjuration_duration = abjuration_dur;
        summon_type = _summon_type;
        if (_god != GOD_NO_GOD)
            god = _god;

        ASSERT(summon_type == 0 || abjuration_dur >= 1 && abjuration_dur <= 6
               || cls == MONS_BALL_LIGHTNING || cls == MONS_ORB_OF_DESTRUCTION
               || cls == MONS_BATTLESPHERE
               || summon_type == SPELL_STICKS_TO_SNAKES
               || summon_type == SPELL_DEATH_CHANNEL
               || summon_type == SPELL_BIND_SOULS
               || summon_type == SPELL_SIMULACRUM
               || summon_type == SPELL_AWAKEN_VINES
               || summon_type == SPELL_FULMINANT_PRISM
               || summon_type == SPELL_INFESTATION);
        return *this;
    }

    bool permit_bands() const
    {
        // The permit flag is set but the forbid flag is not.
        return (flags & (MG_PERMIT_BANDS|MG_FORBID_BANDS)) == MG_PERMIT_BANDS;
    }

    bool force_place() const        { return bool(flags & MG_FORCE_PLACE); }
    bool needs_patrol_point() const { return bool(flags & MG_PATROLLING); }

    // Is there a valid position set on this struct that we want to use
    // when placing the monster?
    bool use_position() const { return in_bounds(pos); };

    bool summoned() const { return abjuration_duration > 0; }

    static mgen_data sleeper_at(monster_type what,
                                const coord_def &where,
                                mgen_flags genflags = MG_NONE)
    {
        return mgen_data(what, BEH_SLEEP, where, MHITNOT, genflags);
    }

    static mgen_data hostile_at(monster_type mt,
                                bool alert = false,
                                const coord_def &p = coord_def(-1, -1))

    {
        return mgen_data(mt, BEH_HOSTILE, p, alert ? MHITYOU : MHITNOT);
    }
};
