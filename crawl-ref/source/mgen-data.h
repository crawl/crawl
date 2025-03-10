#pragma once

#include "beh-type.h"
#include "coord.h"
#include "god-type.h"
#include "mgen-enum.h"
#include "mon-enum.h"
#include "mon-flags.h"
#include "mon-util.h"
#include "player.h"
#include "xp-tracking-type.h"

// Hash key for passing a weapon to be given to
// a dancing weapon.
#define TUKIMA_WEAPON "tukima-weapon"
#define TUKIMA_POWER "tukima-power"

#define MGEN_NUM_HEADS "num_heads"
#define MGEN_BLOB_SIZE "blob_size"
#define MGEN_TENTACLE_CONNECT "tentacle_connect"

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
    // and the summon cap. nullptr is no summoner.
    const actor*    summoner;

    // For summoned monsters, this is a measure of how long the summon will
    // hang around, measured in aut. Use 0 for monsters that aren't summoned
    // or those which are summoned but do not expire on their.
    int             summon_duration;

    // For summoned monsters this is their type of summoning, either the
    // spell which summoned them or one of the values of the enumeration
    // mon_summon_type in mon-enum.h.
    int             summon_type;

    // The center point around which the monster will be created.
    coord_def       pos;

    // Used to control where the monster will be randomly placed, relative to
    // the center point. At first, place_monster() will attempt to find a valid
    // spot within range_preferred tiles of pos. If none can be found, it will
    // expand its search up to range_max tiles of pos before giving up. If
    // range_min is non-negative, it will exclude tiles which are within that
    // distance of pos.
    int range_preferred = 2;
    int range_max = 0;
    int range_min = -1;

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

    // What class of XP is this for LevelXPInfo tracking purposes.
    xp_tracking_type xp_tracking;

    mgen_data(monster_type mt = RANDOM_MONSTER,
              beh_type beh = BEH_HOSTILE,
              const coord_def &p = coord_def(-1, -1),
              unsigned short mfoe = MHITNOT,
              mgen_flags genflags = MG_NONE,
              god_type which_god = GOD_NO_GOD)

        : cls(mt), behaviour(beh), summoner(nullptr), summon_duration(0),
          summon_type(0), pos(p), foe(mfoe), flags(genflags), god(which_god),
          base_type(MONS_NO_MONSTER), colour(COLOUR_INHERIT),
          proximity(PROX_ANYWHERE), place(level_id::current()), hd(0), hp(0),
          extra_flags(MF_NO_FLAGS), mname(""), non_actor_summoner(""),
          initial_shifter(RANDOM_MONSTER), xp_tracking(XP_NON_VAULT)
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

    mgen_data &set_range(int preferred_range, int max_range = 0, int min_range = -1)
    {
        range_preferred = preferred_range;
        if (max_range > 0)
            range_max = max_range;
        if (min_range >= 0)
            range_min = min_range;
        return *this;
    }

    mgen_data &set_summoned(const actor* _summoner, int _summon_type,
                            int duration = 0, bool abjurable = true,
                            bool dependent = true)
    {
        // Hijack any summons created by player shadows or marionettes to belong
        // to the player (your shadow is too emphemeral to keep them from
        // poofing and marionette is a one-shot effect)
        if (_summoner && _summoner->is_monster()
            && (mons_is_player_shadow(*_summoner->as_monster())
                || _summoner->real_attitude() == ATT_MARIONETTE))
        {
            // Summons that would appear around a marionette caster appear
            // around the player instead. (All bets are off it any more custom
            // placement is used.)
            if (pos == _summoner->pos()
                && _summoner->real_attitude() == ATT_MARIONETTE)
            {
                pos = you.pos();
            }

            summoner = &you;
            behaviour = BEH_FRIENDLY;

        }
        else
            summoner = _summoner;
        summon_duration = duration;
        summon_type = _summon_type;

        // It doesn't make sense to have an abjurable summon with no duration.
        if (duration == 0)
            abjurable = false;

        if (abjurable)
            extra_flags |= MF_ACTUAL_SUMMON;
        else
            extra_flags &= ~MF_ACTUAL_SUMMON;

        if (!dependent)
            extra_flags |= MF_PERSISTS;
        else
            extra_flags &= ~MF_PERSISTS;

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

    // XXX: The summoner field is used in normal band placement to temporarily
    //      designate a band member's leader, so we need to rule that out.
    bool is_summoned() const { return summon_type != SPELL_NO_SPELL
                                      || (summoner != nullptr && !(flags & MG_BAND_MINION)); }

    static mgen_data sleeper_at(monster_type what,
                                const coord_def &where,
                                mgen_flags genflags = MG_NONE)
    {
        return mgen_data(what, BEH_SLEEP, where, MHITNOT, genflags);
    }

    static mgen_data hostile_at(monster_type mt,
                                bool alert = false,
                                const coord_def &p = coord_def(-1, -1),
                                god_type god = GOD_NO_GOD)

    {
        return mgen_data(mt, BEH_HOSTILE, p, alert ? MHITYOU : MHITNOT,
                         MG_NONE, god);
    }
};
