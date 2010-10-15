#ifndef MON_INFO_H
#define MON_INFO_H

#include "mon-stuff.h"
#include "mon-enum.h"
#include "mon-util.h"

enum monster_info_flags
{
    MB_STABBABLE,
    MB_DISTRACTED,
    MB_BERSERK,
    MB_DORMANT,
    MB_SLEEPING,
    MB_UNAWARE,
    MB_WANDERING,
    MB_HASTED,
    MB_STRONG,
    MB_SLOWED,
    MB_FLEEING,
    MB_CONFUSED,
    MB_INVISIBLE,
    MB_POISONED,
    MB_ROTTING,
    MB_SUMMONED,
    MB_HALOED,
    MB_GLOWING,
    MB_CHARMED,
    MB_BURNING,
    MB_PARALYSED,
    MB_SICK,
    MB_CAUGHT,
    MB_FRENZIED,
    MB_PETRIFYING,
    MB_PETRIFIED,
    MB_VULN_MAGIC,
    MB_POSSESSABLE,
    MB_ENSLAVED,
    MB_SWIFT,
    MB_INSANE,
    MB_SILENCING,
    MB_MESMERIZING,
    MB_EVIL_ATTACK,
    MB_SHAPESHIFTER,
    MB_CHAOTIC,
    MB_SUBMERGED,
    MB_BLEEDING,
    MB_DEFLECT_MSL,
    MB_PREP_RESURRECT,
    MB_REGENERATION,
    MB_RAISED_MR,
    MB_MIRROR_DAMAGE,
    MB_SAFE,
    MB_UNSAFE,
    MB_NAME_SUFFIX, // [art] rat foo does...
    MB_NAME_ADJECTIVE, // [art] foo rat does...
    MB_NAME_REPLACE, // [art] foo does
    MB_NAME_UNQUALIFIED, // Foo does...
    MB_NAME_THE, // The foo does....
    MB_FADING_AWAY,
    MB_MOSTLY_FADED,
    MB_FEAR_INSPIRING,
};

struct monster_info_base
{
    coord_def pos;
    uint64_t mb;
    std::string mname;
    monster_type type;
    monster_type base_type;
    unsigned number;
    unsigned colour;
    mon_attitude_type attitude;
    mon_dam_level_type dam;
    dungeon_feature_type fire_blocker; // TODO: maybe we should store the position instead
    std::string description;
    std::string quote;
    mon_holy_type holi;
    mon_intel_type mintel;
    mon_resist_def mresists;
    mon_itemuse_type mitemuse;
    int mbase_speed;
    flight_type fly;
    bool two_weapons;
    bool no_regen;
};

// Monster info used by the pane; precomputes some data
// to help with sorting and rendering.
struct monster_info : public monster_info_base
{
    static bool less_than(const monster_info& m1,
                          const monster_info& m2, bool zombified = true, bool fullname = true);

    static bool less_than_wrapper(const monster_info& m1,
                                  const monster_info& m2);

#define MILEV_ALL 0
#define MILEV_SKIP_SAFE -1
#define MILEV_NAME -2
    monster_info() {}
    monster_info(const monster* m, int level = MILEV_ALL);
    monster_info(monster_type p_type, monster_type p_base_type = MONS_NO_MONSTER);

    monster_info(const monster_info& mi)
    : monster_info_base(mi)
    {
        u = mi.u;
        for (unsigned i = 0; i < 6; ++i)
        {
            if (mi.inv[i].get())
                inv[i].reset(new item_def(*mi.inv[i]));
        }
    }

    monster_info& operator=(const monster_info& p)
    {
        this->~monster_info();
        new (this) monster_info(p);
        return *this;
    }

    void to_string(int count, std::string& desc, int& desc_color, bool fullname = true) const;

    // TODO: remove this
    monster* mon() const;

    /* only real equipment is visible, miscellany is for mimic items */
    std::auto_ptr<item_def> inv[6];

    union
    {
        struct
        {
            species_type species;
            job_type job;
            god_type religion;
            skill_type best_skill;
            short best_skill_rank;
            short xl_rank;
        } ghost;
    } u;

    inline bool is(unsigned mbflag) const
    {
        return !!(mb & ((uint64_t)1 << mbflag));
    }

    inline std::string damage_desc() const
    {
        return get_damage_level_string(holi, dam);
    }

    inline bool neutral() const
    {
        return attitude == ATT_NEUTRAL || attitude == ATT_GOOD_NEUTRAL || attitude == ATT_STRICT_NEUTRAL;
    }

    std::string db_name() const;
    bool has_proper_name() const;
    std::string common_name(description_level_type desc = DESC_PLAIN) const;
    std::string proper_name(description_level_type desc = DESC_PLAIN) const;
    std::string full_name(description_level_type desc = DESC_PLAIN, bool use_comma = false) const;

    std::vector<std::string> attributes() const;

    const char *pronoun(pronoun_type variant) const
    {
        return (mons_pronoun(static_cast<monster_type>(type), variant, true));
    }

    std::string wounds_description_sentence() const;
    std::string wounds_description(bool colour = false) const;

    monster_type draco_subspecies() const
    {
        return (draco_subspecies());
    }

    mon_intel_type intel() const
    {
        return (mintel);
    }

    mon_resist_def resists() const
    {
        return (mresists);
    }

    mon_itemuse_type itemuse() const
    {
        return (mitemuse);
    }

    int randarts(artefact_prop_type ra_prop) const;
    int res_magic() const;

    int base_speed() const
    {
        return (mbase_speed);
    }

    bool can_regenerate() const
    {
        return (!no_regen);
    }

    size_type body_size() const;

protected:
    std::string _core_name() const;
    std::string _apply_adjusted_description(description_level_type desc, const std::string& s) const;
};

void get_monster_info(std::vector<monster_info>& mons);

#endif
