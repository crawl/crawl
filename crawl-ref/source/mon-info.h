#pragma once

#include <functional>
#include <vector>

#include "enchant-type.h"
#include "mon-util.h"
#include "options.h"
#include "tag-version.h"

using std::vector;

#define SPECIAL_WEAPON_KEY "special_weapon_name"
#define CLOUD_IMMUNE_MB_KEY "cloud_immune"
#define PRIEST_KEY "priest"
#define ACTUAL_SPELLCASTER_KEY "actual_spellcaster"
#define NECROMANCER_KEY "necromancer"

enum monster_info_flags
{
    MB_STABBABLE,
    MB_MAYBE_STABBABLE,
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
    MB_MORE_POISONED,
    MB_MAX_POISONED,
#if TAG_MAJOR_VERSION == 34
    MB_ROTTING,
#endif
    MB_SUMMONED,
    MB_HALOED,
    MB_GLOWING,
    MB_CHARMED,
    MB_BURNING,
    MB_PARALYSED,
    MB_SICK,
    MB_CAUGHT,
    MB_WEBBED,
#if TAG_MAJOR_VERSION == 34
    MB_OLD_FRENZIED,
#endif
    MB_PETRIFYING,
    MB_PETRIFIED,
    MB_LOWERED_WL,
    MB_POSSESSABLE,
#if TAG_MAJOR_VERSION == 34
    MB_OLD_ENSLAVED,
#endif
    MB_SWIFT,
    MB_FRENZIED,
    MB_SILENCING,
    MB_MESMERIZING,
#if TAG_MAJOR_VERSION == 34
    MB_EVIL_ATTACK,
#endif
    MB_SHAPESHIFTER,
    MB_CHAOTIC,
#if TAG_MAJOR_VERSION == 34
    MB_SUBMERGED,
    MB_BLEEDING,
#endif
#if TAG_MAJOR_VERSION == 34
    MB_PREP_RESURRECT,
#endif
    MB_REGENERATION,
    MB_STRONG_WILLED,
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
#if TAG_MAJOR_VERSION == 34
    MB_WITHDRAWN,
    MB_ATTACHED,
#endif
    MB_DAZED,
    MB_MUTE,
    MB_BLIND,
    MB_DUMB,
    MB_MAD,
#if TAG_MAJOR_VERSION == 34
    MB_CLINGING,
#endif
    MB_NAME_ZOMBIE,
#if TAG_MAJOR_VERSION == 34
    MB_PERM_SUMMON,
#endif
    MB_INNER_FLAME,
    MB_UMBRAED,
#if TAG_MAJOR_VERSION == 34
    MB_OLD_ROUSED,
#endif
    MB_BREATH_WEAPON,
#if TAG_MAJOR_VERSION == 34
    MB_DEATHS_DOOR,
#endif
    MB_FIREWOOD,
    MB_TWO_WEAPONS,
    MB_NO_REGEN,
#if TAG_MAJOR_VERSION == 34
    MB_SUPPRESSED,
#endif
    MB_ROLLING,
    MB_RANGED_ATTACK,
    MB_NO_NAME_TAG,
#if TAG_MAJOR_VERSION == 34
    MB_MAGIC_ARMOUR,
#endif
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
#if TAG_MAJOR_VERSION == 34
    MB_CONTROL_WINDS,
    MB_WIND_AIDED,
    MB_SUMMONED_NO_STAIRS, // Temp. summoned and capped monsters
    MB_SUMMONED_CAPPED,    // Expiring due to summons cap
#endif
    MB_TOXIC_RADIANCE,
    MB_GRASPING_ROOTS,
    MB_FIRE_VULN,
    MB_VORTEX,
    MB_VORTEX_COOLDOWN,
    MB_BARBS,
    MB_POISON_VULN,
    MB_AGILE,
    MB_FROZEN,
    MB_SIGN_OF_RUIN,
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
    MB_CONDENSATION_SHIELD,
#endif
    MB_RESISTANCE,
    MB_HEXED,
#if TAG_MAJOR_VERSION == 34
    MB_BONE_ARMOUR,
    MB_CHANT_FIRE_STORM,
    MB_CHANT_WORD_OF_ENTROPY,
#endif
    MB_AIRBORNE,
#if TAG_MAJOR_VERSION == 34
    MB_BRILLIANCE_AURA,
#endif
    MB_EMPOWERED_SPELLS,
    MB_READY_TO_HOWL,
    MB_PARTIALLY_CHARGED,
    MB_FULLY_CHARGED,
    MB_GOZAG_INCITED,
    MB_PAIN_BOND,
    MB_IDEALISED,
    MB_BOUND_SOUL,
    MB_INFESTATION,
    MB_NO_REWARD,
    MB_STILL_WINDS,
    MB_SLOWLY_DYING,
#if TAG_MAJOR_VERSION == 34
    MB_PINNED,
#endif
    MB_VILE_CLUTCH,
    MB_WATERLOGGED,
    MB_CLOUD_RING_THUNDER,
    MB_CLOUD_RING_FLAMES,
    MB_CLOUD_RING_CHAOS,
    MB_CLOUD_RING_MUTATION,
    MB_CLOUD_RING_FOG,
    MB_CLOUD_RING_ICE,
    MB_CLOUD_RING_MISERY,
    MB_CLOUD_RING_ACID,
    MB_CLOUD_RING_MIASMA,
    MB_WITHERING,
    MB_CRUMBLING,
    MB_ALLY_TARGET,
    MB_CANT_DRAIN,
    MB_CONCENTRATE_VENOM,
    MB_FIRE_CHAMPION,
    MB_SILENCE_IMMUNE,
    MB_ANTIMAGIC,
    MB_NO_ATTACKS,
    MB_RES_DROWN,
    MB_ANGUISH,
    MB_CLARITY,
    MB_DISTRACTED,
    MB_CANT_SEE_YOU,
    MB_UNBLINDABLE,
    MB_SIMULACRUM,
    MB_REFLECTING,
    MB_TELEPORTING,
    MB_CONTAM_LIGHT,
    MB_CONTAM_HEAVY,
#if TAG_MAJOR_VERSION == 34
    MB_PURSUING,
#endif
    MB_BOUND,
    MB_BULLSEYE_TARGET,
    MB_VITRIFIED,
    MB_CURSE_OF_AGONY,
    MB_RETREATING,
    MB_TOUCH_OF_BEOGH,
    MB_AWAITING_RECRUITMENT,
    MB_VENGEANCE_TARGET,
    MB_MAGNETISED,
    MB_RIMEBLIGHT,
    MB_ARMED,
    MB_SHADOWLESS,
    MB_PLAYER_SERVITOR,
    MB_FROZEN_IN_TERROR,
    MB_SOUL_SPLINTERED,
    MB_ENGULFING_PLAYER,
    MB_DOUBLED_HEALTH,
    MB_ABJURABLE,
    MB_UNREWARDING,
    MB_MINION,
    MB_KINETIC_GRAPNEL,
    MB_TEMPERED,
    MB_HATCHING,
    MB_BLINKITIS,
    MB_NO_TELE,
    MB_CHAOS_LACE,
    MB_VEXED,
    MB_VAMPIRE_THRALL,
    MB_PYRRHIC_RECOLLECTION,
    NUM_MB_FLAGS
};

struct monster_info_base
{
    coord_def pos;
    FixedBitVector<NUM_MB_FLAGS> mb;
    string mname;
    monster_type type;
    monster_type base_type;
    union
    {
        // These must all be the same size!
        unsigned number; ///< General purpose number variable
        int num_heads;   ///< # of hydra heads
        int slime_size;  ///< # of slimes in this one
        int is_active;   ///< Whether this ballisto is active or not
    };
    int _colour;
    int ghost_colour;
    mon_attitude_type attitude;
    mon_threat_level_type threat;
    mon_dam_level_type dam;
    // TODO: maybe we should store the position instead
    dungeon_feature_type fire_blocker;
    string description;
    string quote;
    mon_holy_type holi;
    mon_intel_type mintel;
    int hd;
    int ac;
    int ev;
    int base_ev;
    int sh;
    int mr;
    resists_t mresists;
    bool can_see_invis;
    mon_itemuse_type mitemuse;
    int mbase_speed;
    mon_energy_usage menergy;
    CrawlHashTable props;
    string constrictor_name;
    vector<string> constricting_name;
    monster_spells spells;
    mon_attack_def attack[MAX_NUM_ATTACKS];
    bool can_go_frenzy;
    bool can_feel_fear;
    bool sleepwalking;
    bool backlit;
    bool umbraed;

    mid_t client_id;
    mid_t summoner_id;
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
    : monster_info_base(mi), i_ghost(mi.i_ghost)
    {
        for (unsigned i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
        {
            if (mi.inv[i])
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
                   bool fullname = true, const char *adjective = nullptr,
                   bool verbose = true) const;

    /* only real equipment is visible, miscellany is for mimic items */
    unique_ptr<item_def> inv[MSLOT_LAST_VISIBLE_SLOT + 1];

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
    } i_ghost;

    inline bool is(unsigned mbflag) const
    {
        return mb[mbflag];
    }

    inline string damage_desc() const
    {
        return get_damage_level_string(holi, dam);
    }
    string get_max_hp_desc() const;
    int get_known_max_hp() const;
    int regen_rate(int scale) const;

    inline bool neutral() const
    {
        return attitude == ATT_NEUTRAL || attitude == ATT_GOOD_NEUTRAL;
    }

    string db_name() const;
    bool has_proper_name() const;
    string pluralised_name(bool fullname = true) const;
    string common_name(description_level_type desc = DESC_PLAIN) const;
    string proper_name(description_level_type desc = DESC_PLAIN) const;
    string full_name(description_level_type desc = DESC_PLAIN) const;

    vector<string> attributes() const;

    const char *pronoun(pronoun_type variant) const;
    bool pronoun_plurality() const;

    string wounds_description_sentence() const;
    string wounds_description(bool colour = false) const;

    string constriction_description() const;

    monster_type draconian_subspecies() const;

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
    bool nightvision() const;
    int willpower() const;
    int lighting_modifiers() const;

    int base_speed() const
    {
        return mbase_speed;
    }

    string speed_description() const;

    bool wields_two_weapons() const;
    bool can_regenerate() const;
    reach_type reach_range(bool items = true) const;

    size_type body_size() const;
    bool net_immune() const;

    // These should be kept in sync with the actor equivalents
    // (Maybe unify somehow?)
    // Note: actor version is now actor::cannot_act.
    bool cannot_move() const;
    bool asleep() const;
    bool incapacitated() const;
    bool airborne() const;
    bool ground_level() const;

    bool is_named() const
    {
        return !mname.empty() || mons_is_unique(type);
    }

    bool is_actual_spellcaster() const
    {
        return props.exists(ACTUAL_SPELLCASTER_KEY);
    }

    bool is_priest() const
    {
        return props.exists(PRIEST_KEY);
    }

    bool has_necromancy_spell() const
    {
        return props.exists(NECROMANCER_KEY);
    }

    bool fellow_slime() const;

    vector<string> get_unusual_items() const;
    bool has_unusual_items() const;

    bool has_spells() const;
    bool antimagic_susceptible() const;
    int spell_hd(spell_type spell = SPELL_NO_SPELL) const;
    unsigned colour(bool base_colour = false) const;
    void set_colour(int colour);

    bool has_trivial_ench(enchant_type ench) const;
    bool unravellable() const;

    monster* get_known_summoner() const;

protected:
    string _core_name() const;
    string _base_name() const;
    string _apply_adjusted_description(description_level_type desc, const string& s) const;
};

// Colour should be between -1 and 15 inclusive!
bool set_monster_list_colour(monster_list_colour_type, int colour);
void clear_monster_list_colours();

void get_monster_info(vector<monster_info>& mons);

void mons_to_string_pane(string& desc, int& desc_colour, bool fullname,
                           const vector<monster_info>& mi, int start,
                           int count);
void mons_conditions_string(string& desc, const vector<monster_info>& mi,
                            int start, int count, bool equipment);

string description_for_ench(enchant_type type);
