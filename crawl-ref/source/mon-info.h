#ifndef MON_INFO_H
#define MON_INFO_H

#include "mon-util.h"

#define SPECIAL_WEAPON_KEY "special_weapon_name"

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
    MB_WEBBED,
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
#if TAG_MAJOR_VERSION == 34
    MB_PREP_RESURRECT,
#endif
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
#if TAG_MAJOR_VERSION == 34
    MB_FADING_AWAY,
    MB_MOSTLY_FADED,
#endif
    MB_FEAR_INSPIRING,
    MB_WITHDRAWN,
#if TAG_MAJOR_VERSION == 34
    MB_ATTACHED,
#endif
    MB_DAZED,
    MB_MUTE,
    MB_BLIND,
    MB_DUMB,
    MB_MAD,
    MB_CLINGING,
    MB_NAME_ZOMBIE,
    MB_PERM_SUMMON,
    MB_INNER_FLAME,
    MB_UMBRAED,
    MB_ROUSED,
    MB_BREATH_WEAPON,
    MB_DEATHS_DOOR,
    MB_FIREWOOD,
    MB_TWO_WEAPONS,
    MB_NO_REGEN,
#if TAG_MAJOR_VERSION == 34
    MB_SUPPRESSED,
#endif
    MB_ROLLING,
    MB_RANGED_ATTACK,
    MB_NO_NAME_TAG,
    MB_OZOCUBUS_ARMOUR,
    MB_STONESKIN,
    MB_WRETCHED,
    MB_SCREAMED,
    MB_WORD_OF_RECALL,
    MB_INJURY_BOND,
    MB_WATER_HOLD,
    MB_WATER_HOLD_DROWN,
    MB_FLAYED,
#if TAG_MAJOR_VERSION == 34
    MB_RETCHING,
#endif
    MB_WEAK,
    MB_DIMENSION_ANCHOR,
    MB_CONTROL_WINDS,
#if TAG_MAJOR_VERSION == 34
    MB_WIND_AIDED,
    MB_SUMMONED_NO_STAIRS, // Temp. summoned and capped monsters
#endif
    MB_SUMMONED_CAPPED,    // Expiring due to summons cap
    MB_TOXIC_RADIANCE,
    MB_GRASPING_ROOTS,
    MB_FIRE_VULN,
    MB_TORNADO,
    MB_TORNADO_COOLDOWN,
    MB_BARBS,
    MB_POISON_VULN,
    MB_ICEMAIL,
    MB_AGILE,
    MB_FROZEN,
    MB_BLACK_MARK,
    MB_SAP_MAGIC,
    MB_SHROUD,
    MB_CORROSION,
    MB_SPECTRALISED,
    MB_SLOW_MOVEMENT,
    MB_LIGHTLY_DRAINED,
    MB_HEAVILY_DRAINED,
    MB_REPEL_MSL,
#if TAG_MAJOR_VERSION == 34
    MB_NEGATIVE_VULN,
#endif
    MB_CONDENSATION_SHIELD,
    MB_RESISTANCE,
    MB_HEXED,
    MB_BONE_ARMOUR,
    NUM_MB_FLAGS
};

struct monster_info_base
{
    coord_def pos;
    FixedBitVector<NUM_MB_FLAGS> mb;
    string mname;
    monster_type type;
    monster_type base_type;
    monster_type draco_type;
    union
    {
        unsigned number; ///< General purpose number variable
        int num_heads;   ///< # of hydra heads
        int slime_size;  ///< # of slimes in this one
        bool is_active;  ///< Whether this ballisto is active or not
    };
    int _colour;
    mon_attitude_type attitude;
    mon_threat_level_type threat;
    mon_dam_level_type dam;
    // TODO: maybe we should store the position instead
    dungeon_feature_type fire_blocker;
    string description;
    string quote;
    mon_holy_type holi;
    mon_intel_type mintel;
    int ac;
    int ev;
    int base_ev;
    resists_t mresists;
    mon_itemuse_type mitemuse;
    int mbase_speed;
    mon_energy_usage menergy;
    flight_type fly;
    CrawlHashTable props;
    string constrictor_name;
    vector<string> constricting_name;
    monster_spells spells;
    mon_attack_def attack[MAX_NUM_ATTACKS];

    uint32_t client_id;
};

// Monster info used by the pane; precomputes some data
// to help with sorting and rendering.
struct monster_info : public monster_info_base
{
    static bool less_than(const monster_info& m1,
                          const monster_info& m2, bool zombified = true,
                          bool fullname = true);

    static bool less_than_wrapper(const monster_info& m1,
                                  const monster_info& m2);

#define MILEV_ALL 0
#define MILEV_SKIP_SAFE -1
#define MILEV_NAME -2
    monster_info() { client_id = 0; }
    explicit monster_info(const monster* m, int level = MILEV_ALL);
    explicit monster_info(monster_type p_type,
                          monster_type p_base_type = MONS_NO_MONSTER);

    monster_info(const monster_info& mi)
    : monster_info_base(mi)
    {
        u = mi.u;
        for (unsigned i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
        {
            if (mi.inv[i].get())
                inv[i].reset(new item_def(*mi.inv[i]));
        }
    }

    monster_info& operator=(const monster_info& p)
    {
        if (this != &p)
        {
            this->~monster_info();
            new (this) monster_info(p);
        }
        return *this;
    }

    void to_string(int count, string& desc, int& desc_colour,
                   bool fullname = true, const char *adjective = nullptr) const;

    /* only real equipment is visible, miscellany is for mimic items */
    unique_ptr<item_def> inv[MSLOT_LAST_VISIBLE_SLOT + 1];

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
            short damage;
            short ac;
            monster_type acting_part;
            bool can_sinv;
        } ghost;
    } u;

    inline bool is(unsigned mbflag) const
    {
        return mb[mbflag];
    }

    inline string damage_desc() const
    {
        return get_damage_level_string(holi, dam);
    }

    inline bool neutral() const
    {
        return attitude == ATT_NEUTRAL || attitude == ATT_GOOD_NEUTRAL || attitude == ATT_STRICT_NEUTRAL;
    }

    string db_name() const;
    bool has_proper_name() const;
    string pluralised_name(bool fullname = true) const;
    string common_name(description_level_type desc = DESC_PLAIN) const;
    string proper_name(description_level_type desc = DESC_PLAIN) const;
    string full_name(description_level_type desc = DESC_PLAIN, bool use_comma = false) const;
    string chimera_part_names() const;

    vector<string> attributes() const;

    const char *pronoun(pronoun_type variant) const
    {
        return mons_pronoun(type, variant, true);
    }

    string wounds_description_sentence() const;
    string wounds_description(bool colour = false) const;

    string constriction_description() const;

    monster_type draco_or_demonspawn_subspecies() const
    {
        return draco_type;
    }

    mon_intel_type intel() const
    {
        return mintel;
    }

    resists_t resists() const
    {
        return mresists;
    }

    mon_itemuse_type itemuse() const
    {
        return mitemuse;
    }

    int randarts(artefact_prop_type ra_prop) const;
    bool can_see_invisible() const;
    int res_magic() const;

    int base_speed() const
    {
        return mbase_speed;
    }

    string speed_description() const;

    bool wields_two_weapons() const;
    bool can_regenerate() const;
    reach_type reach_range() const;

    size_type body_size() const;

    // These should be kept in sync with the actor equivalents
    // (Maybe unify somehow?)
    bool cannot_move() const;
    bool airborne() const;
    bool ground_level() const;

    bool is_named() const
    {
        return !mname.empty() || mons_is_unique(type);
    }

    bool is_actual_spellcaster() const
    {
        return props.exists("actual_spellcaster");
    }

    bool is_priest() const
    {
        return props.exists("priest");
    }

    bool has_spells() const;
    unsigned colour(bool base_colour = false) const;
    void set_colour(int colour);

protected:
    string _core_name() const;
    string _base_name() const;
    string _apply_adjusted_description(description_level_type desc, const string& s) const;
};

// Colour should be between -1 and 15 inclusive!
bool set_monster_list_colour(string key, int colour);
void clear_monster_list_colours();

void get_monster_info(vector<monster_info>& mons);

typedef function<vector<string> (const monster_info& mi)> (desc_filter);
#endif
