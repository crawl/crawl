/**
 * @file
 * @brief Functions related to special abilities.
**/

#include "AppHdr.h"

#include "ability.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "abyss.h"
#include "act-iter.h"
#include "acquire.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "chardump.h"
#include "cleansing-flame-source-type.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "evoke.h"
#include "exercise.h"
#include "fight.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "level-state-type.h"
#include "libutil.h"
#include "macro.h"
#include "maps.h"
#include "menu.h"
#include "message.h"
#include "mgen-data.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "movement.h"
#include "mutation.h"
#include "nearby-danger.h" // i_feel_safe
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "rltiles/tiledef-icons.h"
#include "skills.h"
#include "species.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-monench.h"
#include "spl-miscast.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-zap.h"
#include "stairs.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "tilepick.h"
#include "transform.h"
#include "traps.h"
#include "uncancel.h"
#include "unicode.h"
#include "view.h"
#include "wiz-dgn.h"

struct generic_cost
{
    int base, add, rolls;

    generic_cost(int num)
        : base(num), add(num == 0 ? 0 : (num + 1) / 2 + 1), rolls(1)
    {
    }
    generic_cost(int num, int _add, int _rolls = 1)
        : base(num), add(_add), rolls(_rolls)
    {
    }
    static generic_cost fixed(int fixed)
    {
        return generic_cost(fixed, 0, 1);
    }
    static generic_cost range(int low, int high, int _rolls = 1)
    {
        return generic_cost(low, high - low + 1, _rolls);
    }

    int cost() const PURE;

    operator bool () const { return base > 0 || add > 0; }
};

struct scaling_cost
{
    int scaling_val;
    int fixed_val;

    scaling_cost(int permille, int fixed = 0)
        : scaling_val(permille), fixed_val(fixed) {}

    static scaling_cost fixed(int fixed)
    {
        return scaling_cost(0, fixed);
    }

    int cost(int max) const;

    operator bool () const { return fixed_val != 0 || scaling_val != 0; }
};

/// What affects the failure chance of the ability?
enum class fail_basis
{
    xl,
    evo,
    invo,
};

/**
 * What skill is used to determine the player's god's invocations' failure
 * chance?
 *
 * XXX: deduplicate this with the similar code for divine titles, etc
 * (skills.cc:skill_title_by_rank)
 *
 * IMPORTANT NOTE: functions that depend on this will be wrong if you aren't
 * currently worshipping a god that grants the given ability (e.g. in ?/A)!
 *
 * @return      The appropriate skill type; e.g. SK_INVOCATIONS.
 */
skill_type invo_skill(god_type god)
{
    switch (god)
    {
        case GOD_KIKUBAAQUDGHA:
            return SK_NECROMANCY;

#if TAG_MAJOR_VERSION == 34
        case GOD_PAKELLAS:
            return SK_EVOCATIONS;
#endif
        case GOD_ASHENZARI:
        case GOD_JIYVA:
        case GOD_GOZAG:
        case GOD_RU:
        case GOD_TROG:
        case GOD_WU_JIAN:
        case GOD_VEHUMET:
        case GOD_XOM:
        case GOD_IGNIS:
            return SK_NONE; // ugh
        default:
            return SK_INVOCATIONS;
    }
}

/// How to determine the odds of the ability failing?
struct failure_info
{
    /// what determines the variable portion of failure: e.g. xl, evo, invo
    fail_basis basis;
    /// base failure chance
    int base_chance;
    /// multiplier to skill/xl; subtracted from base fail chance
    int variable_fail_mult;
    /// denominator to piety; subtracted from base fail chance if invo
    int piety_fail_denom;

    /**
     * What's the chance of the ability failing if the player tries to use it
     * right now?
     *
     * See spl-cast.cc:_get_true_fail_rate() for details on what this 'chance'
     * actually means.
     *
     * @return  A failure chance; may be outside the 0-100 range.
     */
    int chance() const
    {
        switch (basis)
        {
        case fail_basis::xl:
            return base_chance - you.experience_level * variable_fail_mult;
        case fail_basis::evo:
            return base_chance - you.skill(SK_EVOCATIONS, variable_fail_mult);
        case fail_basis::invo:
        {
            const int sk_mod = invo_skill() == SK_NONE ? 0 :
                                 you.skill(invo_skill(), variable_fail_mult);
            const int piety_mod
                = piety_fail_denom ? you.piety / piety_fail_denom : 0;
            return base_chance - sk_mod - piety_mod;
        }
        default:
            die("unknown failure basis %d!", (int)basis);
        }
    }

    /// What skill governs the use of this ability, if any?
    skill_type skill() const
    {
        switch (basis)
        {
        case fail_basis::evo:
            return SK_EVOCATIONS;
        case fail_basis::invo:
            return invo_skill();
        case fail_basis::xl:
        default:
            return SK_NONE;
        }
    }
};

// Structure for representing an ability:
struct ability_def
{
    ability_type        ability;
    const char *        name;
    unsigned int        mp_cost;        // magic cost of ability
    scaling_cost        hp_cost;        // hit point cost of ability
    generic_cost        piety_cost;     // + random2((piety_cost + 1) / 2 + 1)
    int                 range;          // ability range
    failure_info        failure;        // calculator for failure odds
    ability_flags       flags;          // used for additional cost notices

    int get_mp_cost() const
    {
        if (you.has_mutation(MUT_HP_CASTING))
            return 0;
        return mp_cost;
    }

    int get_hp_cost() const
    {
        int cost = hp_cost.cost(you.hp_max);
        if (ability == ABIL_MAKHLEB_DESTRUCTION
            && you.has_mutation(MUT_MAKHLEB_MARK_ATROCITY)
            && you.duration[DUR_GROWING_DESTRUCTION])
        {
            const int stacks = makhleb_get_atrocity_stacks();
            cost = cost * (100 + (stacks * 11)) / 100 + (stacks * 4);
        }
        if (you.has_mutation(MUT_HP_CASTING))
            return cost + mp_cost;
        return cost;
    }

    int avg_piety_cost() const
    {
        if (!piety_cost)
            return 0;

        return piety_cost.base + piety_cost.add/2;
    }

    string piety_pips() const
    {
        const int pip_size = 5;
        const int n_pips = (avg_piety_cost() + pip_size - 1) / pip_size;
        string pips;
        for (int i = 0; i < n_pips; i++)
            pips += "-";
        return pips;
    }

    string piety_desc() const
    {
        if (ability == ABIL_IGNIS_FIERY_ARMOUR
            || ability == ABIL_IGNIS_FOXFIRE)
        {
            // It'd be misleading to describe these with a % of 'max piety',
            // since Ignis's max piety is Special.
            return "";
        }

        if (avg_piety_cost() <= 1)
            return " (less than 1% of your maximum possible piety)";

        // Round up
        const int perc = max((avg_piety_cost() * 100 + 199) / 200, 0);
        return make_stringf(" (about %d%% of your maximum possible piety)", perc);
    }
};

static int _lookup_ability_slot(ability_type abil);
static spret _do_ability(const ability_def& abil, bool fail, dist *target,
                         bolt beam);
static void _finalize_ability_costs(const ability_def& abil, int mp_cost, int hp_cost);

static vector<ability_def> &_get_ability_list()
{
    // construct on initialization v2:
    // https://isocpp.org/wiki/faq/ctors#construct-on-first-use-v2

    // The description screen was way out of date with the actual costs.
    // This table puts all the information in one place... -- bwr
    //
    // The four numerical fields are: MP, HP, piety, and range.
    // Note:  piety_cost = val + random2((val + 1) / 2 + 1);
    //        hp cost is in per-mil of maxhp (i.e. 20 = 2% of hp, rounded up)
    static vector<ability_def> Ability_List =
    {
        // NON_ABILITY should always come first
        { ABIL_NON_ABILITY, "No ability", 0, 0, 0, -1, {}, abflag::none },
        // Innate abilities:
        { ABIL_SPIT_POISON, "Spit Poison",
            0, 0, 0, 5, {fail_basis::xl, 20, 1},
            abflag::breath | abflag::dir_or_target },
        { ABIL_BREATHE_FIRE, "Breathe Fire",
            0, 0, 0, 5, {fail_basis::xl, 30, 1},
            abflag::breath | abflag::dir_or_target },
        { ABIL_COMBUSTION_BREATH, "Combustion Breath",
            0, 0, 0, 5, {fail_basis::xl, 30, 1},
            abflag::drac_charges },
        { ABIL_GLACIAL_BREATH, "Glacial Breath",
            0, 0, 0, 5, {fail_basis::xl, 30, 1},
            abflag::drac_charges },
        { ABIL_BREATHE_POISON, "Breathe Poison Gas",
            0, 0, 0, 6, {fail_basis::xl, 30, 1},
            abflag::breath | abflag::dir_or_target },
        { ABIL_NOXIOUS_BREATH, "Noxious Breath",
            0, 0, 0, 6, {fail_basis::xl, 30, 1},
            abflag::drac_charges },
        { ABIL_GALVANIC_BREATH, "Galvanic Breath",
            0, 0, 0, LOS_MAX_RANGE, {fail_basis::xl, 30, 1},
            abflag::drac_charges },
        { ABIL_NULLIFYING_BREATH, "Nullifying Breath",
            0, 0, 0, LOS_MAX_RANGE, {fail_basis::xl, 30, 1},
            abflag::drac_charges },
        { ABIL_STEAM_BREATH, "Steam Breath",
            0, 0, 0, 6, {fail_basis::xl, 20, 1},
            abflag::drac_charges },
        { ABIL_CAUSTIC_BREATH, "Caustic Breath",
            0, 0, 0, 3, {fail_basis::xl, 30, 1},
            abflag::drac_charges },
        { ABIL_MUD_BREATH, "Mud Breath",
            0, 0, 0, 3, {fail_basis::xl, 30, 1},
            abflag::drac_charges },
        { ABIL_DAMNATION, "Hurl Damnation",
            0, 150, 0, 6, {fail_basis::xl, 50, 1}, abflag::none },
        { ABIL_WORD_OF_CHAOS, "Word of Chaos",
            6, 0, 0, -1, {fail_basis::xl, 50, 1}, abflag::max_hp_drain },
        { ABIL_DIG, "Dig",
            0, 0, 0, -1, {}, abflag::instant | abflag::max_hp_drain },
        { ABIL_SHAFT_SELF, "Shaft Self",
            0, 0, 0, -1, {}, abflag::delay },
        { ABIL_HOP, "Hop",
            0, 0, 0, -1, {}, abflag::none }, // range special-cased
        { ABIL_BLINKBOLT, "Blinkbolt",
            0, 0, 0, LOS_MAX_RANGE, {}, abflag::none },
        { ABIL_SIPHON_ESSENCE, "Siphon Essence",
            20, 0, 0, -1, {}, abflag::none },
#if TAG_MAJOR_VERSION == 34
        { ABIL_HEAL_WOUNDS, "Heal Wounds",
            0, 0, 0, -1, {fail_basis::xl, 45, 2}, abflag::none },
#endif
        { ABIL_IMBUE_SERVITOR, "Imbue Servitor",
            0, 0, 0, -1, {}, abflag::delay },
        { ABIL_IMPRINT_WEAPON, "Imprint Weapon",
            0, 0, 0, -1, {}, abflag::delay },
        { ABIL_END_TRANSFORMATION, "End Transformation",
            0, 0, 0, -1, {}, abflag::none },
        { ABIL_BEGIN_UNTRANSFORM, "Begin Untransformation",
            0, 0, 0, -1, {}, abflag::none },
        { ABIL_INVENT_GIZMO, "Invent Gizmo",
            0, 0, 0, -1, {}, abflag::none },

        { ABIL_CACOPHONY, "Cacophony",
            4, 0, 0, -1, {}, abflag::none },

        { ABIL_BAT_SWARM, "Bat Swarm",
            6, 0, 0, -1, {}, abflag::none },

        { ABIL_ENKINDLE, "Enkindle",
                0, 0, 0, -1, {}, abflag::instant },

        // EVOKE abilities use Evocations and come from items.
        { ABIL_EVOKE_BLINK, "Evoke Blink",
            0, 0, 0, -1, {fail_basis::evo, 40, 2}, abflag::none },
        { ABIL_EVOKE_TURN_INVISIBLE, "Evoke Invisibility",
            0, 0, 0, -1, {fail_basis::evo, 40, 2}, abflag::max_hp_drain },
        // TODO: any way to automatically derive these from the artefact name?
        { ABIL_EVOKE_DISPATER, "Evoke Damnation",
            4, 100, 0, 6, {}, abflag::none },
        { ABIL_EVOKE_OLGREB, "Evoke the Staff of Olgreb",
            4, 0, 0, -1, {}, abflag::none },

        // INVOCATIONS:
        // Zin
        { ABIL_ZIN_RECITE, "Recite",
            0, 0, 0, -1, {fail_basis::invo, 30, 6, 20}, abflag::none },
        { ABIL_ZIN_VITALISATION, "Vitalisation",
            2, 0, 1, -1, {fail_basis::invo, 40, 5, 20}, abflag::none },
        { ABIL_ZIN_IMPRISON, "Imprison",
            5, 0, 4, LOS_MAX_RANGE, {fail_basis::invo, 60, 5, 20},
            abflag::target | abflag::not_self },
        { ABIL_ZIN_SANCTUARY, "Sanctuary",
            7, 0, 15, -1, {fail_basis::invo, 80, 4, 25}, abflag::none },
        { ABIL_ZIN_DONATE_GOLD, "Donate Gold",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },

        // The Shining One
        { ABIL_TSO_DIVINE_SHIELD, "Divine Shield",
            3, 0, 3, -1, {fail_basis::invo, 35, 5, 20}, abflag::none },
        { ABIL_TSO_CLEANSING_FLAME, "Cleansing Flame",
            5, 0, 2, -1, {fail_basis::invo, 70, 4, 25}, abflag::none },
        { ABIL_TSO_SUMMON_DIVINE_WARRIOR, "Summon Divine Warrior",
            8, 0, 5, -1, {fail_basis::invo, 80, 4, 25}, abflag::none },
        { ABIL_TSO_BLESS_WEAPON, "Brand Weapon With Holy Wrath",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },

        // Kikubaaqudgha
        { ABIL_KIKU_UNEARTH_WRETCHES, "Unearth Wretches",
            3, 0, 5, -1, {fail_basis::invo, 40, 5, 20}, abflag::none },
        { ABIL_KIKU_SIGN_OF_RUIN, "Sign of Ruin",
            5, 0, 4, -1, {fail_basis::invo, 60, 5, 20}, abflag::target },
        { ABIL_KIKU_GIFT_CAPSTONE_SPELLS, "Receive Forbidden Knowledge",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_KIKU_BLESS_WEAPON, "Brand Weapon With Pain",
            0, 0, 0, -1, {fail_basis::invo}, abflag::torment },

        // Yredelemnul
        { ABIL_YRED_LIGHT_THE_TORCH, "Light the Black Torch",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_YRED_RECALL_UNDEAD_HARVEST, "Recall Undead Harvest",
            2, 0, 0, -1, {fail_basis::invo, 20, 4, 25}, abflag::none },
        { ABIL_YRED_HURL_TORCHLIGHT, "Hurl Torchlight",
            4, 0, 2, 4, {fail_basis::invo, 30, 5, 25}, abflag::torchlight },
        { ABIL_YRED_BIND_SOUL, "Bind Soul",
            6, 0, 8, LOS_MAX_RANGE, {fail_basis::invo, 50, 5, 25},
            abflag::target | abflag::injury },
        { ABIL_YRED_FATHOMLESS_SHACKLES, "Fathomless Shackles",
            8, 0, 20, -1, {fail_basis::invo, 80, 4, 25}, abflag::none },

        // Okawaru
        { ABIL_OKAWARU_HEROISM, "Heroism",
            2, 0, 3, -1, {fail_basis::invo, 30, 6, 20}, abflag::none },
        { ABIL_OKAWARU_FINESSE, "Finesse",
            5, 0, 5, -1, {fail_basis::invo, 60, 4, 25}, abflag::none },
        { ABIL_OKAWARU_DUEL, "Duel",
            7, 0, 5, LOS_MAX_RANGE, {fail_basis::invo, 80, 4, 20},
            abflag::target | abflag::not_self },
        { ABIL_OKAWARU_GIFT_WEAPON, "Receive Weapon",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_OKAWARU_GIFT_ARMOUR, "Receive Armour",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },


        // Makhleb
        { ABIL_MAKHLEB_DESTRUCTION, "Unleash Destruction",
            0, scaling_cost(65, 2), 0, LOS_MAX_RANGE, {fail_basis::invo, 20, 5, 20},
            abflag::dir_or_target | abflag::not_self },
        { ABIL_MAKHLEB_ANNIHILATION, "Globe of Annihilation",
            0, scaling_cost::fixed(6), 2, LOS_MAX_RANGE,
            {fail_basis::invo, 20, 5, 20}, abflag::dir_or_target },
        { ABIL_MAKHLEB_INFERNAL_SERVANT, "Infernal Servant",
            0, scaling_cost::fixed(8), 4, -1, {fail_basis::invo, 40, 5, 20}},
        { ABIL_MAKHLEB_INFERNAL_LEGION, "Infernal Legion",
            0, scaling_cost::fixed(10), 8, -1, {fail_basis::invo, 55, 5, 20}},
        { ABIL_MAKHLEB_BRAND_SELF_1, "Brand Self #1",
            0, 0, 0, -1, {fail_basis::invo}, abflag::injury },
        { ABIL_MAKHLEB_BRAND_SELF_2, "Brand Self #2",
            0, 0, 0, -1, {fail_basis::invo}, abflag::injury },
        { ABIL_MAKHLEB_BRAND_SELF_3, "Brand Self #3",
            0, 0, 0, -1, {fail_basis::invo}, abflag::injury },
        { ABIL_MAKHLEB_VESSEL_OF_SLAUGHTER, "Vessel of Slaughter",
            0, 0, 12, -1,
            {fail_basis::invo, 75, 5, 25}, abflag::none },

        // Sif Muna
        { ABIL_SIF_MUNA_CHANNEL_ENERGY, "Channel Magic",
            0, 0, 2, -1, {fail_basis::invo, 60, 4, 25}, abflag::none },
        { ABIL_SIF_MUNA_FORGET_SPELL, "Forget Spell",
            0, 0, 8, -1, {fail_basis::invo}, abflag::none },
        { ABIL_SIF_MUNA_DIVINE_EXEGESIS, "Divine Exegesis",
            0, 0, 12, -1, {fail_basis::invo, 80, 4, 25}, abflag::none },

        // Trog
        { ABIL_TROG_BERSERK, "Berserk",
            0, 0, 1, -1, {fail_basis::invo, 45, 0, 2}, abflag::none },
        { ABIL_TROG_HAND, "Trog's Hand",
            0, 0, 2, -1, {fail_basis::invo, piety_breakpoint(2), 0, 1},
            abflag::none },
        { ABIL_TROG_BROTHERS_IN_ARMS, "Brothers in Arms",
            0, 0, 5, -1, {fail_basis::invo, piety_breakpoint(5), 0, 1},
            abflag::none },

        // Elyvilon
        { ABIL_ELYVILON_PURIFICATION, "Purification",
            2, 0, 2, -1, {fail_basis::invo, 20, 5, 20}, abflag::conf_ok },
        { ABIL_ELYVILON_HEAL_OTHER, "Heal Other",
            2, 0, 2, -1, {fail_basis::invo, 40, 5, 20}, abflag::none },
        { ABIL_ELYVILON_HEAL_SELF, "Heal Self",
            2, 0, 3, -1, {fail_basis::invo, 40, 5, 20}, abflag::none },
        { ABIL_ELYVILON_DIVINE_VIGOUR, "Divine Vigour",
            0, 0, 6, -1, {fail_basis::invo, 80, 4, 25}, abflag::none },

        // Lugonu
        { ABIL_LUGONU_ABYSS_EXIT, "Depart the Abyss",
            1, 0, 10, -1, {fail_basis::invo, 30, 6, 20}, abflag::none },
        { ABIL_LUGONU_BANISH, "Banish",
            4, 0, generic_cost::range(3, 4), LOS_MAX_RANGE,
            {fail_basis::invo, 65, 7, 20}, abflag::none },
        { ABIL_LUGONU_CORRUPT, "Corrupt",
            7, scaling_cost::fixed(5), 10, -1, {fail_basis::invo, 70, 4, 25},
            abflag::none },
        { ABIL_LUGONU_ABYSS_ENTER, "Enter the Abyss",
            10, 0, 28, -1, {fail_basis::invo, 80, 4, 25}, abflag::injury },
        { ABIL_LUGONU_BLESS_WEAPON, "Brand Weapon With Distortion",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },

        // Nemelex
        { ABIL_NEMELEX_DRAW_DESTRUCTION, "Draw Destruction",
            0, 0, 0, -1, {fail_basis::invo}, abflag::card },
        { ABIL_NEMELEX_DRAW_ESCAPE, "Draw Escape",
            0, 0, 0, -1, {fail_basis::invo}, abflag::card },
        { ABIL_NEMELEX_DRAW_SUMMONING, "Draw Summoning",
            0, 0, 0, -1, {fail_basis::invo}, abflag::card },
        { ABIL_NEMELEX_DRAW_STACK, "Draw Stack",
            0, 0, 0, -1, {fail_basis::invo}, abflag::card },
        { ABIL_NEMELEX_TRIPLE_DRAW, "Triple Draw",
            2, 0, 6, -1, {fail_basis::invo, 60, 5, 20}, abflag::none },
        { ABIL_NEMELEX_DEAL_FOUR, "Deal Four",
            8, 0, 8, -1, {fail_basis::invo, -1}, // failure special-cased
            abflag::none },
        { ABIL_NEMELEX_STACK_FIVE, "Stack Five",
            5, 0, 10, -1, {fail_basis::invo, 80, 4, 25}, abflag::none },

        // Beogh
        { ABIL_BEOGH_DISMISS_APOSTLE_1, "Dismiss Apostle #1",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_BEOGH_DISMISS_APOSTLE_2, "Dismiss Apostle #2",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_BEOGH_DISMISS_APOSTLE_3, "Dismiss Apostle #3",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_BEOGH_SMITING, "Smiting",
            3, 0, 2, LOS_MAX_RANGE,
            {fail_basis::invo, 40, 5, 20}, abflag::none },
        { ABIL_BEOGH_RECALL_APOSTLES, "Recall Apostles",
            2, 0, 0, -1, {fail_basis::invo, 30, 6, 20}, abflag::none },
        { ABIL_BEOGH_RECRUIT_APOSTLE, "Recruit Apostle",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_BEOGH_BLOOD_FOR_BLOOD, "Blood for Blood",
            8, 0, 20, -1, {fail_basis::invo, 70, 4, 25}, abflag::none },

        // Jiyva
        { ABIL_JIYVA_OOZEMANCY, "Oozemancy",
            3, 0, 8, -1, {fail_basis::invo, 80, 0, 2}, abflag::none },
        { ABIL_JIYVA_SLIMIFY, "Slimify",
            5, 0, 10, -1, {fail_basis::invo, 90, 0, 2}, abflag::none },

        // Fedhas
        { ABIL_FEDHAS_WALL_OF_BRIARS, "Wall of Briars",
            3, 0, 2, -1, {fail_basis::invo, 30, 6, 20}, abflag::none },
        { ABIL_FEDHAS_GROW_BALLISTOMYCETE, "Grow Ballistomycete",
            4, 0, 4, 2, {fail_basis::invo, 60, 4, 25}, abflag::target },
        { ABIL_FEDHAS_OVERGROW, "Overgrow",
            8, 0, 12, LOS_MAX_RANGE, {fail_basis::invo, 70, 5, 20},
            abflag::none },
        { ABIL_FEDHAS_GROW_OKLOB, "Grow Oklob",
            6, 0, 6, 2, {fail_basis::invo, 80, 4, 25}, abflag::target },

        // Cheibriados
        { ABIL_CHEIBRIADOS_TIME_BEND, "Bend Time",
            3, 0, 1, -1, {fail_basis::invo, 40, 4, 20}, abflag::none },
        { ABIL_CHEIBRIADOS_DISTORTION, "Temporal Distortion",
            4, 0, 3, -1, {fail_basis::invo, 60, 5, 20}, abflag::instant },
        { ABIL_CHEIBRIADOS_SLOUCH, "Slouch",
            5, 0, 8, -1, {fail_basis::invo, 60, 4, 25}, abflag::none },
        { ABIL_CHEIBRIADOS_TIME_STEP, "Step From Time",
            10, 0, 12, -1, {fail_basis::invo, 80, 4, 25}, abflag::none },

        // Ashenzari
        { ABIL_ASHENZARI_CURSE, "Curse Item",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_ASHENZARI_UNCURSE, "Shatter the Chains",
            0, 0, 0, -1, {fail_basis::invo}, abflag::curse },

        // Dithmenos
        { ABIL_DITHMENOS_SHADOWSLIP, "Shadowslip",
            4, 60, 2, -1, {fail_basis::invo, 50, 6, 30}, abflag::instant },
        { ABIL_DITHMENOS_APHOTIC_MARIONETTE, "Aphotic Marionette",
            5, 0, 3, -1, {fail_basis::invo, 60, 4, 25}, abflag::target },
        { ABIL_DITHMENOS_PRIMORDIAL_NIGHTFALL, "Primordial Nightfall",
            8, 0, 13, -1, {fail_basis::invo, 80, 4, 25}, abflag::none },

        // Ru
        { ABIL_RU_DRAW_OUT_POWER, "Draw Out Power",
            0, 0, 0, -1, {fail_basis::invo},
            abflag::exhaustion | abflag::max_hp_drain | abflag::conf_ok },
        { ABIL_RU_POWER_LEAP, "Power Leap",
            5, 0, 0, 3, {fail_basis::invo}, abflag::exhaustion },
        { ABIL_RU_APOCALYPSE, "Apocalypse",
            8, 0, 0, -1, {fail_basis::invo},
            abflag::exhaustion | abflag::max_hp_drain },

        { ABIL_RU_SACRIFICE_PURITY, "Sacrifice Purity",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_WORDS, "Sacrifice Words",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_DRINK, "Sacrifice Drink",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_ESSENCE, "Sacrifice Essence",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_HEALTH, "Sacrifice Health",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_STEALTH, "Sacrifice Stealth",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_ARTIFICE, "Sacrifice Artifice",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_LOVE, "Sacrifice Love",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_COURAGE, "Sacrifice Courage",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_ARCANA, "Sacrifice Arcana",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_NIMBLENESS, "Sacrifice Nimbleness",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_DURABILITY, "Sacrifice Durability",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_HAND, "Sacrifice a Hand",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_EXPERIENCE, "Sacrifice Experience",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_SKILL, "Sacrifice Skill",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_EYE, "Sacrifice an Eye",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_RESISTANCE, "Sacrifice Resistance",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_SACRIFICE_FORMS, "Sacrifice Forms",
            0, 0, 0, -1, {fail_basis::invo}, abflag::sacrifice },
        { ABIL_RU_REJECT_SACRIFICES, "Reject Sacrifices",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },

        // Gozag
        { ABIL_GOZAG_POTION_PETITION, "Potion Petition",
            0, 0, 0, -1, {fail_basis::invo}, abflag::gold },
        { ABIL_GOZAG_CALL_MERCHANT, "Call Merchant",
            0, 0, 0, -1, {fail_basis::invo}, abflag::gold },
        { ABIL_GOZAG_BRIBE_BRANCH, "Bribe Branch",
            0, 0, 0, -1, {fail_basis::invo}, abflag::gold },

        // Qazlal
        { ABIL_QAZLAL_UPHEAVAL, "Upheaval",
            3, 0, generic_cost::range(3, 4), LOS_MAX_RANGE,
            {fail_basis::invo, 40, 5, 20},
            abflag::none },
        { ABIL_QAZLAL_ELEMENTAL_FORCE, "Elemental Force",
            5, 0, 6, -1, {fail_basis::invo, 60, 5, 20}, abflag::none },
        { ABIL_QAZLAL_DISASTER_AREA, "Disaster Area",
            8, 0, 10, -1, {fail_basis::invo, 70, 4, 25}, abflag::none },

        // Uskayaw
        { ABIL_USKAYAW_STOMP, "Stomp",
            3, 0, generic_cost::fixed(20), -1, {fail_basis::invo},
            abflag::none },
        { ABIL_USKAYAW_LINE_PASS, "Line Pass",
            4, 0, generic_cost::fixed(20), -1,
            {fail_basis::invo}, abflag::none },
        { ABIL_USKAYAW_GRAND_FINALE, "Grand Finale",
            8, 0, generic_cost::fixed(0), LOS_MAX_RANGE,
            {fail_basis::invo, 120 + piety_breakpoint(4), 5, 1}, abflag::none },

        // Hepliaklqana
        { ABIL_HEPLIAKLQANA_RECALL, "Recall Ancestor",
            2, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_HEPLIAKLQANA_TRANSFERENCE, "Transference",
            2, 0, 3, LOS_MAX_RANGE, {fail_basis::invo, 40, 5, 20},
            abflag::none },
        { ABIL_HEPLIAKLQANA_IDEALISE, "Idealise",
            4, 0, 4, -1, {fail_basis::invo, 60, 4, 25}, abflag::none },

        { ABIL_HEPLIAKLQANA_TYPE_KNIGHT, "Ancestor Life: Knight",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_HEPLIAKLQANA_TYPE_BATTLEMAGE, "Ancestor Life: Battlemage",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_HEPLIAKLQANA_TYPE_HEXER, "Ancestor Life: Hexer",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },

        { ABIL_HEPLIAKLQANA_IDENTITY, "Ancestor Identity",
            0, 0, 0, -1, {fail_basis::invo}, abflag::instant },

        // Wu Jian
        { ABIL_WU_JIAN_SERPENTS_LASH, "Serpent's Lash",
            0, 0, 2, -1, {fail_basis::invo},
            abflag::exhaustion | abflag::instant },
        { ABIL_WU_JIAN_HEAVENLY_STORM, "Heavenly Storm",
            0, 0, 20, -1, {fail_basis::invo, piety_breakpoint(5), 0, 1},
            abflag::none },
        // Lunge and Whirlwind abilities aren't menu abilities but currently
        // need to exist for action counting, hence need enums/entries.
        { ABIL_WU_JIAN_LUNGE, "Lunge",
            0, 0, 0, -1, {}, abflag::berserk_ok },
        { ABIL_WU_JIAN_WHIRLWIND, "Whirlwind",
            0, 0, 0, -1, {}, abflag::berserk_ok },
        { ABIL_WU_JIAN_WALLJUMP, "Wall Jump",
            0, 0, 0, -1, {}, abflag::berserk_ok },

        // Ignis
        { ABIL_IGNIS_FIERY_ARMOUR, "Fiery Armour",
            0, 0, 8, -1, {fail_basis::invo}, abflag::none },
        { ABIL_IGNIS_FOXFIRE, "Foxfire Swarm",
            0, 0, 12, -1, {fail_basis::invo}, abflag::quiet_fail },
        { ABIL_IGNIS_RISING_FLAME, "Rising Flame",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },

        { ABIL_RENOUNCE_RELIGION, "Renounce Religion",
            0, 0, 0, -1, {fail_basis::invo}, abflag::none },
        { ABIL_CONVERT_TO_BEOGH, "Convert to Beogh",
            0, 0, 0, -1, {fail_basis::invo}, abflag::conf_ok },
#ifdef WIZARD
        { ABIL_WIZ_BUILD_TERRAIN, "Build terrain",
            0, 0, 0, LOS_MAX_RANGE, {}, abflag::instant },
        { ABIL_WIZ_SET_TERRAIN, "Set terrain to build",
            0, 0, 0, -1, {}, abflag::instant },
        { ABIL_WIZ_CLEAR_TERRAIN, "Clear terrain to floor",
            0, 0, 0, LOS_MAX_RANGE, {}, abflag::instant },
#endif

    };
    return Ability_List;
}

static map<ability_type, spell_type> breath_to_spell =
    {
        { ABIL_COMBUSTION_BREATH, SPELL_COMBUSTION_BREATH },
        { ABIL_GLACIAL_BREATH, SPELL_GLACIAL_BREATH },
        { ABIL_NULLIFYING_BREATH, SPELL_NULLIFYING_BREATH },
        { ABIL_STEAM_BREATH, SPELL_STEAM_BREATH },
        { ABIL_NOXIOUS_BREATH, SPELL_NOXIOUS_BREATH },
        { ABIL_CAUSTIC_BREATH, SPELL_CAUSTIC_BREATH },
        { ABIL_MUD_BREATH, SPELL_MUD_BREATH },
        { ABIL_GALVANIC_BREATH, SPELL_GALVANIC_BREATH },
    };

static const ability_def& get_ability_def(ability_type abil)
{
    for (const ability_def &ab_def : _get_ability_list())
        if (ab_def.ability == abil)
            return ab_def;

    return _get_ability_list()[0];
}

vector<ability_type> get_defined_abilities()
{
    vector<ability_type> r;
    for (const ability_def &ab_def : _get_ability_list())
        r.push_back(ab_def.ability);
    return r;
}

unsigned int ability_mp_cost(ability_type abil)
{
    return get_ability_def(abil).get_mp_cost();
}

int ability_range(ability_type abil)
{
    int range = get_ability_def(abil).range;

    // Special-cased abilities with variable range.
    switch (abil)
    {
        case ABIL_HOP:
            range = frog_hop_range();
            break;
        default:
            break;
    }

    return min((int)you.current_vision, range);
}

static int _makhleb_destruction_power()
{
    if (you.has_mutation(MUT_MAKHLEB_MARK_ATROCITY))
    {
        const int stacks = makhleb_get_atrocity_stacks();
        return you.skill_rdiv(SK_INVOCATIONS, 8 + (3 * stacks), 6) + (stacks * 3);
    }
    else
        return you.skill_rdiv(SK_INVOCATIONS, 4, 3);
}

static int _makhleb_annihilation_power()
{
    return you.skill_rdiv(SK_INVOCATIONS, 9, 3);
}

static int _ability_zap_pow(ability_type abil)
{
    switch (abil)
    {
        case ABIL_SPIT_POISON:
            return 10 + you.experience_level;
        case ABIL_BREATHE_FIRE:
            return you.experience_level * 2;
        case ABIL_BREATHE_POISON:
            return you.experience_level;
        case ABIL_MAKHLEB_DESTRUCTION:
            return _makhleb_destruction_power();
        default:
            ASSERT(ability_to_zap(abil) == NUM_ZAPS);
            return 0;
    }
}

ability_flags get_ability_flags(ability_type ability)
{
    const ability_def& abil = get_ability_def(ability);
    return abil.flags;
}

/**
 * Is there a valid ability with a name matching that given?
 *
 * @param key   The name in question. (Not case sensitive.)
 * @return      true if such an ability exists; false if not.
 */
bool string_matches_ability_name(const string& key)
{
    return ability_by_name(key) != ABIL_NON_ABILITY;
}

static bool _invis_causes_drain()
{
    return !you.unrand_equipped(UNRAND_AMULET_INVISIBILITY)
               && !you.unrand_equipped(UNRAND_SCARF_INVISIBILITY);
}

/**
 * Find an ability whose name matches the given key.
 *
 * @param name      The name in question. (Not case sensitive.)
 * @return          The enum of the relevant ability, if there was one; else
 *                  ABIL_NON_ABILITY.
 */
ability_type ability_by_name(const string &key)
{
    for (const auto &abil : _get_ability_list())
    {
        if (abil.ability == ABIL_NON_ABILITY)
            continue;

        const string name = lowercase_string(ability_name(abil.ability));
        if (name == lowercase_string(key))
            return abil.ability;
    }

    return ABIL_NON_ABILITY;
}

string print_abilities()
{
    string text = "\n<w>a:</w> ";

    const vector<talent> talents = your_talents(false);

    if (talents.empty())
        text += "no special abilities";
    else
    {
        for (unsigned int i = 0; i < talents.size(); ++i)
        {
            if (i)
                text += ", ";
            text += ability_name(talents[i].which);
        }
    }

    return text;
}

int get_gold_cost(ability_type ability)
{
    switch (ability)
    {
    case ABIL_GOZAG_CALL_MERCHANT:
        return gozag_price_for_shop(true);
    case ABIL_GOZAG_POTION_PETITION:
        return GOZAG_POTION_PETITION_AMOUNT;
    case ABIL_GOZAG_BRIBE_BRANCH:
        return GOZAG_BRIBE_AMOUNT;
    default:
        return 0;
    }
}

string nemelex_card_text(ability_type ability)
{
    int cards = deck_cards(ability_deck(ability));

    if (ability == ABIL_NEMELEX_DRAW_STACK)
        return make_stringf("(next: %s)", stack_top().c_str());
    else
        return make_stringf("(%d in deck)", cards);
}

static string _ashenzari_curse_text()
{
    const CrawlVector& curses = you.props[CURSE_KNOWLEDGE_KEY].get_vector();
    return "(Boost: "
           + comma_separated_fn(curses.begin(), curses.end(),
                                curse_abbr, "/", "/")
           + ")";
}

const string make_cost_description(ability_type ability)
{
    const ability_def& abil = get_ability_def(ability);
    string ret;
    if (abil.get_mp_cost())
        ret += make_stringf(", %d MP", abil.get_mp_cost());

    if (abil.flags & abflag::variable_mp)
        ret += ", MP";

#if TAG_MAJOR_VERSION == 34
    if (ability == ABIL_HEAL_WOUNDS)
        ret += make_stringf(", Permanent MP (%d left)", get_real_mp(false));
#endif

    if (ability == ABIL_CACOPHONY)
        ret += ", Noise";

    if (ability == ABIL_ASHENZARI_CURSE
        && !you.props[CURSE_KNOWLEDGE_KEY].get_vector().empty())
    {
        ret += ", ";
        ret += _ashenzari_curse_text();
    }

    const int hp_cost = abil.get_hp_cost();
    if (hp_cost)
        ret += make_stringf(", %d HP", hp_cost);

    if (abil.piety_cost)
        ret += make_stringf(", Piety%s", abil.piety_pips().c_str());

    if (abil.flags & abflag::breath)
        ret += ", Breath";

    if (abil.flags & abflag::drac_charges)
    {
        ret += make_stringf(", %d/%d uses available",
                    draconian_breath_uses_available(),
                    MAX_DRACONIAN_BREATH);
    }

    if (abil.flags & abflag::delay)
        ret += ", Delay";

    if (abil.flags & abflag::torment)
        ret += ", Torment";

    if (abil.flags & abflag::injury)
        ret += ", Injury";

    if (abil.flags & abflag::exhaustion)
        ret += ", Exhaustion";

    if (abil.flags & abflag::instant)
        ret += ", Instant"; // not really a cost, more of a bonus - bwr

    if (abil.flags & abflag::max_hp_drain
        && (ability != ABIL_EVOKE_TURN_INVISIBLE || _invis_causes_drain()))
    {
        ret += ", Drain";
    }

    if (abil.flags & abflag::curse)
        ret += ", Cursed item";

    if (abil.flags & abflag::gold)
    {
        const int amount = get_gold_cost(ability);
        if (amount)
            ret += make_stringf(", %d Gold", amount);
        else if (ability == ABIL_GOZAG_POTION_PETITION)
            ret += ", Free";
        else
            ret += ", Gold";
    }

    if (abil.flags & abflag::sacrifice)
    {
        ret += ", ";
        const string prefix = "Sacrifice ";
        ret += string(ability_name(ability)).substr(prefix.size());
        ret += ru_sac_text(ability);
    }

    if (abil.flags & abflag::card)
    {
        ret += ", ";
        ret += "A Card ";
        ret += nemelex_card_text(ability);
    }

    if (abil.flags & abflag::torchlight)
        ret += ", Torchlight";

    // If we haven't output anything so far, then the effect has no cost
    if (ret.empty())
        return "None";

    ret.erase(0, 2);
    return ret;
}

static const string _detailed_cost_description(ability_type ability)
{
    const ability_def& abil = get_ability_def(ability);
    ostringstream ret;

    bool have_cost = false;
    ret << "This ability costs: ";

    if (abil.get_mp_cost())
    {
        have_cost = true;
        ret << "\nMP     : ";
        ret << abil.get_mp_cost();
    }
    if (abil.get_hp_cost())
    {
        have_cost = true;
        ret << "\nHP     : ";
        ret << abil.get_hp_cost();
    }

    if (abil.piety_cost)
    {
        have_cost = true;
        ret << "\nPiety  : ";
        ret << abil.piety_pips() << abil.piety_desc();
    }

    if (abil.flags & abflag::gold)
    {
        have_cost = true;
        ret << "\nGold   : ";
        int gold_amount = get_gold_cost(ability);
        if (gold_amount)
            ret << gold_amount;
        else if (ability == ABIL_GOZAG_POTION_PETITION)
            ret << "free";
        else
            ret << "variable";
    }

    if (abil.flags & abflag::curse)
    {
        have_cost = true;
        ret << "\nOne cursed item";
    }

    if (abil.flags & abflag::torchlight)
    {
        have_cost = true;
        ret << "\nTorchlight";
    }
    if (!have_cost)
        ret << "nothing.";

    if (abil.flags & abflag::breath)
        ret << "\nYou must catch your breath between uses of this ability.";

    if (abil.flags & abflag::delay)
        ret << "\nThis ability takes some time before being effective.";

    if (abil.flags & abflag::injury)
        ret << "\nUsing this ability will hurt you for a large fraction of your current HP.";

    if (abil.flags & abflag::torment)
        ret << "\nUsing this ability invokes torment.";

    if (abil.flags & abflag::exhaustion)
        ret << "\nThis ability causes exhaustion, and cannot be used when exhausted.";

    if (abil.flags & abflag::instant)
        ret << "\nThis ability is instantaneous.";

    if (abil.flags & abflag::conf_ok)
        ret << "\nYou can use this ability even if confused.";

    if (abil.flags & abflag::max_hp_drain
        && (ability != ABIL_EVOKE_TURN_INVISIBLE || _invis_causes_drain()))
    {
        ret << "\nThis ability will temporarily drain your maximum health when used";
        if (ability == ABIL_EVOKE_TURN_INVISIBLE)
            ret << ", even unsuccessfully";
        ret << ".";
    }

    if (abil.flags & abflag::drac_charges)
        ret << "\nGaining experience will replenish charges of this ability.";

#if TAG_MAJOR_VERSION == 34
    if (abil.ability == ABIL_HEAL_WOUNDS)
    {
        ASSERT(!have_cost); // validate just in case this ever changes
        return "This ability has a chance of reducing your maximum magic "
               "capacity when used.";
    }
#endif

    return ret.str();
}

// TODO: consolidate with player_has_ability?
ability_type fixup_ability(ability_type ability)
{
    switch (ability)
    {
    case ABIL_TROG_BERSERK:
        if (you.is_lifeless_undead() || you.stasis())
            return ABIL_NON_ABILITY;
        return ability;

    case ABIL_EVOKE_BLINK:
        if (you.stasis())
            return ABIL_NON_ABILITY;
        else
            return ability;

    case ABIL_LUGONU_ABYSS_EXIT:
    case ABIL_LUGONU_ABYSS_ENTER:
        if (brdepth[BRANCH_ABYSS] == -1)
            return ABIL_NON_ABILITY;
        else
            return ability;

    case ABIL_OKAWARU_GIFT_ARMOUR:
        if (you.props.exists(OKAWARU_ARMOUR_GIFTED_KEY)
            || !player_can_use_armour())
        {
            return ABIL_NON_ABILITY;
        }
        else
            return ability;

    case ABIL_OKAWARU_GIFT_WEAPON:
        if (you.props.exists(OKAWARU_WEAPON_GIFTED_KEY))
            return ABIL_NON_ABILITY;
        // fall through
    case ABIL_TSO_BLESS_WEAPON:
    case ABIL_KIKU_BLESS_WEAPON:
    case ABIL_LUGONU_BLESS_WEAPON:
        if (you.has_mutation(MUT_NO_GRASPING))
            return ABIL_NON_ABILITY;
        else
            return ability;

    case ABIL_ELYVILON_HEAL_OTHER:
    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
    case ABIL_TROG_BROTHERS_IN_ARMS:
    case ABIL_GOZAG_BRIBE_BRANCH:
    case ABIL_QAZLAL_ELEMENTAL_FORCE:
        if (you.allies_forbidden())
            return ABIL_NON_ABILITY;
        else
            return ability;

    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
        if (you.get_mutation_level(MUT_HP_CASTING))
            return ABIL_NON_ABILITY;
        return ability;

    case ABIL_SIF_MUNA_FORGET_SPELL:
        if (you.get_mutation_level(MUT_INNATE_CASTER))
            return ABIL_NON_ABILITY;
        return ability;

    case ABIL_BEOGH_RECRUIT_APOSTLE:
        if (!you.duration[DUR_BEOGH_CAN_RECRUIT])
            return ABIL_NON_ABILITY;
        return ability;

    case ABIL_BEOGH_DISMISS_APOSTLE_1:
        if (get_num_apostles() < 1)
            return ABIL_NON_ABILITY;
        return ability;

    case ABIL_BEOGH_DISMISS_APOSTLE_2:
        if (get_num_apostles() < 2)
            return ABIL_NON_ABILITY;
        return ability;

    case ABIL_BEOGH_DISMISS_APOSTLE_3:
        if (get_num_apostles() < 3)
            return ABIL_NON_ABILITY;
        return ability;

    case ABIL_BEOGH_RECALL_APOSTLES:
        if (get_num_apostles() < 1)
            return ABIL_NON_ABILITY;
        return ability;

    case ABIL_MAKHLEB_INFERNAL_SERVANT:
        if (you.has_mutation(MUT_MAKHLEB_MARK_ANNIHILATION))
            return ABIL_MAKHLEB_ANNIHILATION;
        else if (you.allies_forbidden())
            return ABIL_NON_ABILITY;
        else if (you.has_mutation(MUT_MAKHLEB_MARK_LEGION))
            return ABIL_MAKHLEB_INFERNAL_LEGION;
        return ability;

    case ABIL_MAKHLEB_VESSEL_OF_SLAUGHTER:
        if (you.has_mutation(MUT_MAKHLEB_MARK_FANATIC))
            return ability;
        return ABIL_NON_ABILITY;

    default:
        return ability;
    }
}

/// Handle special cases for ability failure chances.
static int _adjusted_failure_chance(ability_type ability, int base_chance)
{
    switch (ability)
    {
    case ABIL_BREATHE_FIRE:
    case ABIL_GLACIAL_BREATH:
    case ABIL_CAUSTIC_BREATH:
    case ABIL_GALVANIC_BREATH:
    case ABIL_NULLIFYING_BREATH:
    case ABIL_NOXIOUS_BREATH:
    case ABIL_STEAM_BREATH:
    case ABIL_MUD_BREATH:
        if (you.form == transformation::dragon)
            return base_chance - 20;
        return base_chance;

    case ABIL_NEMELEX_DEAL_FOUR:
        return 70 - (you.piety * 2 / 45) - you.skill(SK_INVOCATIONS, 9) / 2;

    default:
        return base_chance;
    }
}

talent get_talent(ability_type ability, bool check_confused)
{
    ASSERT(ability != ABIL_NON_ABILITY);

    // Placeholder handling, part 1: The ability we have might be a
    // placeholder, so convert it into its corresponding ability before
    // doing anything else, so that we'll handle its flags properly.
    talent result { fixup_ability(ability), 0, 0, false };
    const ability_def &abil = get_ability_def(result.which);

    if (check_confused && you.confused()
        && !testbits(abil.flags, abflag::conf_ok))
    {
        result.which = ABIL_NON_ABILITY;
        return result;
    }

    // Look through the table to see if there's a preference, else find
    // a new empty slot for this ability. - bwr
    const int index = find_ability_slot(abil.ability);
    result.hotkey = index >= 0 ? index_to_letter(index) : 0;

    const int base_chance = abil.failure.chance();
    const int failure = _adjusted_failure_chance(ability, base_chance);
    result.fail = max(0, min(100, failure));

    result.is_invocation = abil.failure.basis == fail_basis::invo;

    return result;
}

mutation_type makhleb_ability_to_mutation(ability_type abil)
{
    // XXX: The list of marks the player will be offered is generated as soon
    //      as the player first joins Makhleb, but internally their ability
    //      keybinds are assigned before god-specific code is run, and it will
    //      try to pull the names of mutations that aren't assigned yet. Use a
    //      placeholder to stop a crash on conversion.
    if (!you.props.exists(MAKHLEB_OFFERED_MARKS_KEY))
        return MUT_NON_MUTATION;

    return (mutation_type)you.props[MAKHLEB_OFFERED_MARKS_KEY]
                            .get_vector()[abil - ABIL_MAKHLEB_BRAND_SELF_1].get_int();
}


string ability_name(ability_type ability, bool dbname)
{
    // Special-case some dynamic names
    switch (ability)
    {
        case ABIL_BEOGH_RECRUIT_APOSTLE:
            if (dbname)
                return "Recruit Apostle";
            else
                return "Recruit " + get_apostle_name(0);

        case ABIL_BEOGH_DISMISS_APOSTLE_1:
            if (dbname)
                return "Dismiss Apostle";
            else
                return "Dismiss " + get_apostle_name(1, true);

        case ABIL_BEOGH_DISMISS_APOSTLE_2:
            if (dbname)
                return "Dismiss Apostle";
            else
                return "Dismiss " + get_apostle_name(2, true);

        case ABIL_BEOGH_DISMISS_APOSTLE_3:
            if (dbname)
                return "Dismiss Apostle";
            else
                return "Dismiss " + get_apostle_name(3, true);

        case ABIL_MAKHLEB_BRAND_SELF_1:
        case ABIL_MAKHLEB_BRAND_SELF_2:
        case ABIL_MAKHLEB_BRAND_SELF_3:
            if (dbname)
                return "Brand Self";
            else
            {
                return make_stringf("Accept %s",
                                    mutation_name(makhleb_ability_to_mutation(ability)));
            }

        default:
            return get_ability_def(ability).name;
    }
}

vector<string> get_ability_names()
{
    vector<string> result;
    for (const talent &tal : your_talents(false))
        result.push_back(ability_name(tal.which));
    return result;
}

static string _curse_desc()
{
    if (!you.props.exists(CURSE_KNOWLEDGE_KEY))
        return "";

    const CrawlVector& curses = you.props[CURSE_KNOWLEDGE_KEY].get_vector();

    if (curses.empty())
        return "";

    return "\nIf you bind an item with this curse Ashenzari will enhance "
           "the following skills:\n"
           + comma_separated_fn(curses.begin(), curses.end(), desc_curse_skills,
                                ".\n", ".\n") + ".";
}

static string _desc_sac_mut(const CrawlStoreValue &mut_store)
{
    return mut_upgrade_summary(static_cast<mutation_type>(mut_store.get_int()));
}

static string _sacrifice_desc(const ability_type ability)
{
    const string boilerplate =
        "\nIf you make this sacrifice, your powers granted by Ru "
        "will become stronger in proportion to the value of the "
        "sacrifice, and you may gain new powers as well.\n\n"
        "Sacrifices cannot be taken back.\n";
    const string piety_info = ru_sacrifice_description(ability);
    const string desc = boilerplate + piety_info;

    if (!you_worship(GOD_RU))
        return desc;

    const string sac_vec_key = ru_sacrifice_vector(ability);
    if (sac_vec_key.empty())
        return desc;

    ASSERT(you.props.exists(sac_vec_key));
    const CrawlVector &sacrifice_muts = you.props[sac_vec_key].get_vector();
    return "\nAfter this sacrifice, you will find that "
            + comma_separated_fn(sacrifice_muts.begin(), sacrifice_muts.end(),
                                 _desc_sac_mut)
            + ".\n" + desc;
}

static string _nemelex_desc(ability_type ability)
{
    ostringstream desc;
    deck_type deck = ability_deck(ability);

    desc << "Draw a card from " << (deck == DECK_STACK ? "your " : "the ");
    desc << deck_name(deck) << "; " << lowercase_first(deck_description(deck));

    return desc.str();
}

static int _tso_cleansing_flame_power(bool allow_random = true)
{
    return allow_random ? 10 + you.skill_rdiv(SK_INVOCATIONS, 7, 6)
                        : 10 + you.skill(SK_INVOCATIONS, 7) / 6;
}

static int _yred_hurl_torchlight_power()
{
    return 12 + you.skill(SK_INVOCATIONS, 4);
}

static int _beogh_smiting_power(bool allow_random = true)
{
    return 12 + skill_bump(SK_INVOCATIONS, 6, allow_random);
}

static int _hurl_damnation_power()
{
    return 40 + you.experience_level * 6;
}

static int _blinkbolt_power()
{
    return get_form()->get_level(200) / 27;
}

static int _orb_of_dispater_power();

// XXX This is a mess, caused by abilities doing damage via various
// different in-house and outsourced methods.
static string _ability_damage_string(ability_type ability)
{
    // TODO The following abilities have effects that would be nice to show
    //      but which don't fit neatly into the "Damage: dice" format.
    // Chei: temporal distortion, step from time
    // Elyvilon: heal other, heal self, divine vigour
    // Fedhas: ballistomycete, oklob
    // Hep: idealise
    // Ignis: fiery armour
    // Kiku: unearth wretches
    // Ru: draw out power
    // Yred: fathomless shackles
    // Zin: vitalisation
    // TSO: divine shield

    dice_def dam;

    switch (ability)
    {
        case ABIL_MAKHLEB_DESTRUCTION:
            return spell_damage_string(SPELL_UNLEASH_DESTRUCTION, false,
                                       _makhleb_destruction_power());

        case ABIL_MAKHLEB_ANNIHILATION:
            return spell_damage_string(SPELL_UNLEASH_DESTRUCTION, false,
                                       _makhleb_annihilation_power());

        case ABIL_YRED_HURL_TORCHLIGHT:
            return spell_damage_string(SPELL_HURL_TORCHLIGHT, false,
                                       _yred_hurl_torchlight_power());
        case ABIL_BEOGH_SMITING:
            dam = beogh_smiting_dice(_beogh_smiting_power(false), false);
            return make_stringf("6 + %dd%d", dam.num, dam.size);
        case ABIL_TSO_CLEANSING_FLAME:
            return make_stringf("2d%d*", _tso_cleansing_flame_power(false));
        case ABIL_CHEIBRIADOS_SLOUCH:
            return make_stringf("%dd3 / 2 (against normal-speed enemies)",
                                slouch_damage_for_speed());
        case ABIL_IGNIS_FOXFIRE:
            return "1d8/foxfire"; // constant
        case ABIL_QAZLAL_UPHEAVAL:
            dam = qazlal_upheaval_damage(false);
            break;
        case ABIL_QAZLAL_DISASTER_AREA:
            dam = qazlal_upheaval_damage(false);
            return make_stringf("%dd%d/upheaval", dam.num, dam.size);
        case ABIL_RU_POWER_LEAP:
            dam = ru_power_leap_damage(false);
            break;
        case ABIL_RU_APOCALYPSE:
            // Apocalypse uses 4 dice, 6 dice or 8 dice depending on the effect.
            return make_stringf("10 + (4-8)d%d", apocalypse_die_size(false));
        case ABIL_JIYVA_OOZEMANCY:
            // per turn damage is 2d(3*#walls)
            return "2d(3-24)";
        case ABIL_USKAYAW_STOMP:
            dam = uskayaw_stomp_extra_damage(false);
            return make_stringf("%dd%d + 1/6 of current HP",
                                dam.num, dam.size);
        case ABIL_USKAYAW_GRAND_FINALE: // infinity
            return Options.char_set == CSET_ASCII ? "death" : "\u221e";
        case ABIL_DAMNATION:
            return spell_damage_string(SPELL_HURL_DAMNATION, false,
                                       _hurl_damnation_power());
        case ABIL_EVOKE_DISPATER:
            return spell_damage_string(SPELL_HURL_DAMNATION, true,
                                       _orb_of_dispater_power());
        case ABIL_SPIT_POISON:
        case ABIL_BREATHE_POISON:
        case ABIL_BREATHE_FIRE:
            dam = zap_damage(ability_to_zap(ability), _ability_zap_pow(ability),
                             false, false);
            break;
        case ABIL_BLINKBOLT:
            return spell_damage_string(SPELL_BLINKBOLT, false,
                                       _blinkbolt_power());
        case ABIL_COMBUSTION_BREATH:
            dam = combustion_breath_damage(you.form == transformation::dragon
                                            ? you.experience_level * 2
                                            : you.experience_level, false);
            break;
        case ABIL_MUD_BREATH:
        case ABIL_GALVANIC_BREATH:
        case ABIL_STEAM_BREATH:
        case ABIL_GLACIAL_BREATH:
        case ABIL_NULLIFYING_BREATH:
        case ABIL_NOXIOUS_BREATH:
        case ABIL_CAUSTIC_BREATH:
            return spell_damage_string(breath_to_spell[ability], false,
                                       you.form == transformation::dragon
                                        ? you.experience_level * 2
                                        : you.experience_level);
        default:
            return "";
    }

    return make_stringf("%dd%d", dam.num, dam.size);
}

// XXX: should this be in describe.cc?
string get_ability_desc(const ability_type ability, bool need_title)
{
    const string& name = ability_name(ability, true);

    string lookup;

    if (testbits(get_ability_def(ability).flags, abflag::card))
        lookup = _nemelex_desc(ability);
    else
        lookup = getLongDescription(name + " ability");

    if (lookup.empty()) // Nothing found?
        lookup = "No description found.\n";

    switch (ability)
    {
        case ABIL_ASHENZARI_CURSE:
            lookup += _curse_desc();
            break;

        case ABIL_BEOGH_DISMISS_APOSTLE_1:
        case ABIL_BEOGH_DISMISS_APOSTLE_2:
        case ABIL_BEOGH_DISMISS_APOSTLE_3:
        {
            const int index = ability - ABIL_BEOGH_DISMISS_APOSTLE_1 + 1;
            lookup += "\n" + apostle_short_description(index) + "\n";
        }
        break;

        case ABIL_MAKHLEB_BRAND_SELF_1:
        case ABIL_MAKHLEB_BRAND_SELF_2:
        case ABIL_MAKHLEB_BRAND_SELF_3:
        {
            const mutation_type mut = makhleb_ability_to_mutation(ability);
            lookup += "\n" + get_mutation_desc(mut);
        }
        break;

        default:
        break;
    }

    if (testbits(get_ability_def(ability).flags, abflag::sacrifice))
        lookup += _sacrifice_desc(ability);

    const string damage_str = _ability_damage_string(ability);

    const string range_str = range_string(ability_range(ability));

    lookup += "\n";

    if (damage_str != "")
        lookup += make_stringf("Damage: %s\n ", damage_str.c_str());
    lookup += make_stringf("Range: %s\n", range_str.c_str());

    ostringstream res;
    if (need_title)
        res << name << "\n\n";
    res << lookup << "\n" << _detailed_cost_description(ability);

    const string quote = getQuoteString(name + " ability");
    if (!quote.empty())
        res << "\n_________________\n\n<darkgrey>" << quote << "</darkgrey>";

    return res.str();
}

static void _print_talent_description(const talent& tal)
{
    describe_ability(tal.which);
}

void no_ability_msg()
{
    mpr("Sorry, you're not good enough to have a special ability.");
}

// Prompts the user for an ability to use, first checking the lua hook
// c_choose_ability
bool activate_ability()
{
    vector<talent> talents = your_talents(false);

    if (talents.empty())
    {
        no_ability_msg();
        crawl_state.zero_turns_taken();
        return false;
    }

    int selected = -1;

    string luachoice;

    if (!clua.callfn("c_choose_ability", ">s", &luachoice))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (!luachoice.empty())
    {
        bool valid = false;
        // Sanity check
        for (unsigned int i = 0; i < talents.size(); ++i)
        {
            if (talents[i].hotkey == luachoice[0])
            {
                selected = static_cast<int>(i);
                valid = true;
                break;
            }
        }

        // Lua gave us garbage, defer to the user
        if (!valid)
            selected = -1;
    }

    if (Options.ability_menu && selected == -1)
    {
        selected = choose_ability_menu(talents);
        if (selected == -1)
        {
            canned_msg(MSG_OK);
            crawl_state.zero_turns_taken();
            return false;
        }
    }
    else
    {
        while (selected < 0)
        {
            msg::streams(MSGCH_PROMPT) << "Use which ability? (? or * to list) "
                                       << endl;

            const int keyin = get_ch();

            if (keyin == '?' || keyin == '*')
            {
                selected = choose_ability_menu(talents);
                if (selected == -1)
                {
                    canned_msg(MSG_OK);
                    crawl_state.zero_turns_taken();
                    return false;
                }
            }
            else if (key_is_escape(keyin) || keyin == ' ' || keyin == '\r'
                     || keyin == '\n')
            {
                canned_msg(MSG_OK);
                crawl_state.zero_turns_taken();
                return false;
            }
            else if (isaalpha(keyin))
            {
                // Try to find the hotkey.
                for (unsigned int i = 0; i < talents.size(); ++i)
                {
                    if (talents[i].hotkey == keyin)
                    {
                        selected = static_cast<int>(i);
                        break;
                    }
                }

                // If we can't, cancel out.
                if (selected < 0)
                {
                    mpr("You can't do that.");
                    crawl_state.zero_turns_taken();
                    return false;
                }
            }
        }
    }
    return activate_talent(talents[selected]);
}

static bool _can_movement_ability(bool quiet)
{
    const string reason = movement_impossible_reason();
    if (reason.empty())
        return true;
    if (!quiet)
        mpr(reason);
    return false;
}

static bool _can_hop(bool quiet)
{
    if (you.duration[DUR_NO_HOP])
    {
        if (!quiet)
            mpr("Your legs are too worn out to hop.");
        return false;
    }
    return _can_movement_ability(quiet);
}

static bool _can_blinkbolt(bool quiet)
{
    if (you.duration[DUR_BLINKBOLT_COOLDOWN])
    {
        if (!quiet)
            canned_msg(MSG_CANNOT_DO_YET);
        return false;
    }
    return true;
}

static bool _can_rising_flame(bool quiet)
{
    ASSERT(can_do_capstone_ability(GOD_IGNIS));
    if (you.duration[DUR_RISING_FLAME])
    {
        if (!quiet)
            mpr("You're already rising!");
        return false;
    }
    if (you.where_are_you == BRANCH_DUNGEON && you.depth == 1)
    {
        if (player_has_orb())
            return true;
        else
        {
            if (!quiet)
                mpr("You can't rise from this level without the Orb!");
            return false;
        }
    }
    if (!level_above().is_valid())
    {
        if (!quiet)
            mpr("You can't rise from this level!");
        return false;
    }
    return true;
}

// Check prerequisites for a number of abilities.
// Abort any attempt if these cannot be met, without losing the turn.
// TODO: Many more cases need to be added!
static bool _check_ability_possible(const ability_def& abil, bool quiet = false)
{
#ifdef WIZARD
    if (abil.ability >= ABIL_FIRST_WIZ)
        return you.wizard;
#endif
    if (you.berserk() && !testbits(abil.flags, abflag::berserk_ok))
    {
        if (!quiet)
            canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (abil.ability == ABIL_TROG_BERSERK
        && !you.can_go_berserk(true, false, quiet))
    {
        return false;
    }

    if (you.confused() && !testbits(abil.flags, abflag::conf_ok))
    {
        if (!quiet)
            canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    // Silence and water elementals
    if (silenced(you.pos())
        || you.duration[DUR_WATER_HOLD] && !you.res_water_drowning())
    {
        talent tal = get_talent(abil.ability, false);
        if (tal.is_invocation && abil.ability != ABIL_RENOUNCE_RELIGION)
        {
            if (!quiet)
            {
                mprf("You cannot call out to %s while %s.",
                     god_name(you.religion).c_str(),
                     you.duration[DUR_WATER_HOLD] ? "unable to breathe"
                                                  : "silenced");
            }
            return false;
        }
        if (abil.ability == ABIL_WORD_OF_CHAOS)
        {
            if (!quiet)
            {
                mprf("You cannot speak a word of chaos while %s.",
                     you.duration[DUR_WATER_HOLD] ? "unable to breathe"
                                                  : "silenced");
            }
            return false;
        }
    }

    const god_power* god_power = god_power_from_ability(abil.ability);
    if (god_power && !god_power_usable(*god_power))
    {
        if (!quiet)
            canned_msg(MSG_GOD_DECLINES);
        return false;
    }

    if (testbits(abil.flags, abflag::card) && !deck_cards(ability_deck(abil.ability)))
    {
        if (!quiet)
            mpr("That deck is empty!");
        return false;
    }

    if (!quiet)
    {
        vector<text_pattern> &actions = Options.confirm_action;
        if (!actions.empty())
        {
            string name = ability_name(abil.ability);
            for (const text_pattern &action : actions)
            {
                if (action.matches(name))
                {
                    string prompt = "Really use " + name + "?";
                    if (!yesno(prompt.c_str(), false, 'n'))
                    {
                        canned_msg(MSG_OK);
                        return false;
                    }
                    break;
                }
            }
        }
    }

    // Check that we can afford to pay the costs.
    // Note that mutation shenanigans might leave us with negative MP,
    // so don't fail in that case if there's no MP cost.
    if (abil.get_mp_cost() > 0 && !enough_mp(abil.get_mp_cost(), quiet, true))
        return false;

    const int hpcost = abil.get_hp_cost();
    if (hpcost > 0 && !enough_hp(hpcost, quiet))
        return false;

    switch (abil.ability)
    {
    case ABIL_ZIN_RECITE:
    {
        if (!zin_check_able_to_recite(quiet))
            return false;

        int result = zin_check_recite_to_monsters(quiet);
        if (result != 1)
        {
            if (!quiet)
            {
                if (result == 0)
                    mpr("There's no appreciative audience!");
                else if (result == -1)
                    mpr("You are not zealous enough to affect this audience!");
            }
            return false;
        }
        return true;
    }

    case ABIL_ZIN_SANCTUARY:
        if (env.sanctuary_time)
        {
            if (!quiet)
                mpr("There's already a sanctuary in place on this level.");
            return false;
        }
        return true;

    case ABIL_ZIN_DONATE_GOLD:
        if (!you.gold)
        {
            if (!quiet)
                mpr("You have nothing to donate!");
            return false;
        }
        return true;

    case ABIL_OKAWARU_DUEL:
        if (okawaru_duel_active() || player_in_branch(BRANCH_ARENA))
        {
            if (!quiet)
                mpr("You are already engaged in single combat!");
            return false;
        }
        return true;

    case ABIL_OKAWARU_GIFT_WEAPON:
    case ABIL_OKAWARU_GIFT_ARMOUR:
        if (feat_eliminates_items(env.grid(you.pos())))
        {
            if (!quiet)
                mpr("Any gift you received here would fall and be lost!");
            return false;
        }
        return true;

    case ABIL_ELYVILON_HEAL_SELF:
        if (you.hp == you.hp_max)
        {
            if (!quiet)
                canned_msg(MSG_FULL_HEALTH);
            return false;
        }
        if (you.duration[DUR_DEATHS_DOOR])
        {
            if (!quiet)
                mpr("You can't heal while in death's door.");
            return false;
        }
        return true;

    case ABIL_ELYVILON_PURIFICATION:
        if (!you.duration[DUR_SICKNESS]
            && !you.duration[DUR_POISONING]
            && !you.duration[DUR_CONF] && !you.duration[DUR_SLOW]
            && !you.petrifying()
            && !player_drained()
            && !you.duration[DUR_WEAK])
        {
            if (!quiet)
                mpr("Nothing ails you!");
            return false;
        }
        return true;

    case ABIL_ELYVILON_DIVINE_VIGOUR:
        if (you.duration[DUR_DIVINE_VIGOUR])
        {
            if (!quiet)
                mpr("You have already been granted divine vigour!");
            return false;
        }
        return true;

    case ABIL_JIYVA_OOZEMANCY:
        if (you.duration[DUR_OOZEMANCY])
        {
            if (!quiet)
                mpr("You are already calling forth ooze!");
            return false;
        }
        return true;

    case ABIL_LUGONU_ABYSS_EXIT:
        if (!player_in_branch(BRANCH_ABYSS))
        {
            if (!quiet)
                mpr("You aren't in the Abyss!");
            return false;
        }
        return true;

    case ABIL_LUGONU_CORRUPT:
        return !is_level_incorruptible(quiet);

    case ABIL_LUGONU_ABYSS_ENTER:
        if (player_in_branch(BRANCH_ABYSS))
        {
            if (!quiet)
                mpr("You're already here!");
            return false;
        }
        return true;

    case ABIL_SIF_MUNA_FORGET_SPELL:
        if (you.spell_no == 0)
        {
            if (!quiet)
                canned_msg(MSG_NO_SPELLS);
            return false;
        }
        return true;

    case ABIL_SIF_MUNA_DIVINE_EXEGESIS:
        return can_cast_spells(quiet);

    case ABIL_FEDHAS_WALL_OF_BRIARS:
    {
        vector<coord_def> spaces = find_briar_spaces(true);
        if (spaces.empty())
        {
            if (!quiet)
                mpr("There isn't enough space to grow briars here.");
            return false;
        }
        return true;
    }

    case ABIL_RU_DRAW_OUT_POWER:
        if (you.duration[DUR_EXHAUSTED])
        {
            if (!quiet)
                mpr("You're too exhausted to draw out your power.");
            return false;
        }
        if (you.hp == you.hp_max && you.magic_points == you.max_magic_points
            && !you.duration[DUR_CONF]
            && !you.duration[DUR_SLOW]
            && !you.attribute[ATTR_HELD]
            && !you.petrifying()
            && !you.is_constricted())
        {
            if (!quiet)
                mpr("You have no need to draw out power.");
            return false;
        }
        return true;

    case ABIL_RU_POWER_LEAP:
        if (you.duration[DUR_EXHAUSTED])
        {
            if (!quiet)
                mpr("You're too exhausted to power leap.");
            return false;
        }
        return true;

    case ABIL_RU_APOCALYPSE:
        if (you.duration[DUR_EXHAUSTED])
        {
            if (!quiet)
                mpr("You're too exhausted to unleash your apocalyptic power.");
            return false;
        }
        return true;

    case ABIL_QAZLAL_ELEMENTAL_FORCE:
    {
        vector<coord_def> clouds = find_elemental_targets();
        if (clouds.empty())
        {
            if (!quiet)
                mpr("You can't see any clouds you can empower.");
            return false;
        }
        return true;
    }

    case ABIL_COMBUSTION_BREATH:
    case ABIL_GLACIAL_BREATH:
    case ABIL_GALVANIC_BREATH:
    case ABIL_CAUSTIC_BREATH:
    case ABIL_NULLIFYING_BREATH:
    case ABIL_STEAM_BREATH:
    case ABIL_NOXIOUS_BREATH:
    case ABIL_MUD_BREATH:
        if (draconian_breath_uses_available() <= 0)
        {
            if (!quiet)
                mpr("You have exhausted your breath weapon. Slay more foes!");
            return false;
        }
        return true;

    case ABIL_BREATHE_FIRE:
    case ABIL_SPIT_POISON:
    case ABIL_BREATHE_POISON:
        if (you.duration[DUR_BREATH_WEAPON])
        {
            if (!quiet)
                canned_msg(MSG_CANNOT_DO_YET);
            return false;
        }
        return true;

#if TAG_MAJOR_VERSION == 34
    case ABIL_HEAL_WOUNDS:
        if (you.hp == you.hp_max)
        {
            if (!quiet)
                canned_msg(MSG_FULL_HEALTH);
            return false;
        }
        if (you.duration[DUR_DEATHS_DOOR])
        {
            if (!quiet)
                mpr("You can't heal while in death's door.");
            return false;
        }
        if (get_real_mp(false) < 1)
        {
            if (!quiet)
                mpr("You don't have enough innate magic capacity.");
            return false;
        }
        return true;
#endif

    case ABIL_SHAFT_SELF:
        return you.can_do_shaft_ability(quiet);

    case ABIL_HOP:
        return _can_hop(quiet);

    case ABIL_INVENT_GIZMO:
    {
        if (you.experience_level < COGLIN_GIZMO_XL)
        {
            if (!quiet)
            {
                mprf("In %d experience levels, you will have learned enough to "
                     "assemble a masterpiece.", (COGLIN_GIZMO_XL - you.experience_level));
            }
            return false;
        }

        return true;
    }

    case ABIL_CACOPHONY:
        // In the very unlikely case that the player has regained enough XP to
        // use this ability again before it ends. Maybe in Sprint?
        if (you.duration[DUR_CACOPHONY])
        {
            if (!quiet)
                mpr("You are already making a cacophony!");
            return false;
        }
        else if (you.props.exists(CACOPHONY_XP_KEY))
        {
            if (!quiet)
                mpr("You must recover your energy before unleashing another cacophony.");
            return false;
        }
        else if (!you.equipment.get_first_slot_item(SLOT_HAUNTED_AUX))
        {
            if (!quiet)
                mpr("You aren't haunting any armour at the moment!");
            return false;
        }

        return true;

    case ABIL_BAT_SWARM:
    {
        if (you.props.exists(BATFORM_XP_KEY))
        {
            if (!quiet)
                mpr("You must recover your energy before scattering into bats again.");
            return false;
        }
        const string reason = cant_transform_reason(transformation::bat_swarm);
        if (!reason.empty())
        {
            if (!quiet)
                mpr(reason);
            return false;
        }
        return true;
    }

    case ABIL_ENKINDLE:
    {
        if (you.duration[DUR_ENKINDLED])
        {
            if (!quiet)
                mpr("You are already burning your memories away!");

            return false;
        }
        else if (you.props[ENKINDLE_CHARGES_KEY].get_int() == 0)
        {
            if (!quiet)
                mpr("You don't have any memories left to burn.");

            return false;
        }

        return true;
    }

    case ABIL_WORD_OF_CHAOS:
        if (you.duration[DUR_WORD_OF_CHAOS_COOLDOWN])
        {
            if (!quiet)
                canned_msg(MSG_CANNOT_DO_YET);
            return false;
        }
        return true;

    case ABIL_EVOKE_BLINK:
        if (you.duration[DUR_BLINK_COOLDOWN])
        {
            if (!quiet)
                canned_msg(MSG_CANNOT_DO_YET);
            return false;
        }
        // fallthrough
    case ABIL_BLINKBOLT:
    {
        const string no_tele_reason = you.no_tele_reason(true);
        if (no_tele_reason.empty())
            return true;

        if (!quiet)
             mpr(no_tele_reason);
        return false;
    }

    case ABIL_TROG_BERSERK:
        return you.can_go_berserk(true, false, true)
               && (quiet || berserk_check_wielded_weapon());

    case ABIL_EVOKE_TURN_INVISIBLE:
        if (you.duration[DUR_INVIS])
        {
            if (!quiet)
                mpr("You are already invisible!");
            return false;
        }
        return true;

    case ABIL_GOZAG_POTION_PETITION:
        return gozag_setup_potion_petition(quiet);

    case ABIL_GOZAG_CALL_MERCHANT:
        return gozag_setup_call_merchant(quiet);

    case ABIL_GOZAG_BRIBE_BRANCH:
        return gozag_check_bribe_branch(quiet);

    case ABIL_RU_SACRIFICE_EXPERIENCE:
        if (you.experience_level <= RU_SAC_XP_LEVELS)
        {
            if (!quiet)
                mpr("You don't have enough experience to sacrifice.");
            return false;
        }
        return true;

        // only available while your ancestor is alive.
    case ABIL_HEPLIAKLQANA_IDEALISE:
    case ABIL_HEPLIAKLQANA_RECALL:
    case ABIL_HEPLIAKLQANA_TRANSFERENCE:
        if (hepliaklqana_ancestor() == MID_NOBODY)
        {
            if (!quiet)
            {
                mprf("%s is still trapped in memory!",
                     hepliaklqana_ally_name().c_str());
            }
            return false;
        }
        return true;

    case ABIL_WU_JIAN_SERPENTS_LASH:
        if (you.attribute[ATTR_SERPENTS_LASH])
        {
            if (!quiet)
                mpr("You are already lashing out.");
            return false;
        }
        if (you.duration[DUR_EXHAUSTED])
        {
            if (!quiet)
                mpr("You are too exhausted to lash out.");
            return false;
        }
        return true;

    case ABIL_WU_JIAN_HEAVENLY_STORM:
        if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
        {
            if (!quiet)
                mpr("You are already engulfed in a heavenly storm!");
            return false;
        }
        return true;

    case ABIL_WU_JIAN_WALLJUMP:
    {
        // TODO: Add check for whether there is any valid landing spot
        if (you.is_nervous())
        {
            if (!quiet)
                mpr("You are too terrified to wall jump!");
            return false;
        }
        if (you.attribute[ATTR_HELD])
        {
            if (!quiet)
            {
                mprf("You cannot wall jump while caught in a %s.",
                     get_trapping_net(you.pos()) == NON_ITEM ? "web" : "net");
            }
            return false;
        }
        // Is there a valid place to wall jump?
        bool has_targets = false;
        for (adjacent_iterator ai(you.pos()); ai; ++ai)
            if (feat_can_wall_jump_against(env.grid(*ai)))
            {
                has_targets = true;
                break;
            }

        if (!has_targets)
        {
            if (!quiet)
                mpr("There is nothing to wall jump against here.");
            return false;
        }
        return true;
    }

    case ABIL_IGNIS_RISING_FLAME:
        return _can_rising_flame(quiet);

    case ABIL_DIG:
        return form_keeps_mutations();

    case ABIL_SIPHON_ESSENCE:
        if (you.duration[DUR_SIPHON_COOLDOWN])
        {
            if (!quiet)
                canned_msg(MSG_CANNOT_DO_YET);
            return false;
        }
        return true;

    case ABIL_YRED_LIGHT_THE_TORCH:
    {
        if (yred_torch_is_raised())
        {
            if (!quiet)
                mpr("The black torch is already ablaze!");
            return false;
        }
        const string reason = yred_cannot_light_torch_reason();
        if (!reason.empty())
        {
            if (!quiet)
                mpr(reason.c_str());
            return false;
        }
        return true;
    }

    case ABIL_YRED_HURL_TORCHLIGHT:
        if (!yred_torch_is_raised())
        {
            if (!quiet)
                mpr("The black torch is unlit!");
            return false;
        }
        if (yred_get_torch_power() < 1)
        {
            if (!quiet)
                mpr("You must stoke the torch's fire more first.");
            return false;
        }
        return true;

    case ABIL_YRED_FATHOMLESS_SHACKLES:
        if (you.duration[DUR_FATHOMLESS_SHACKLES])
        {
            if (!quiet)
                mpr("You are already invoking Yredulemnul's grip!");
            return false;
        }
        return true;

    case ABIL_BEOGH_RECRUIT_APOSTLE:
        if (get_num_apostles() == 3)
        {
            if (!quiet)
                mpr("You already have the maximum number of followers. Dismiss one first.");
            return false;
        }
        return true;

    case ABIL_BEOGH_BLOOD_FOR_BLOOD:
        if (you.duration[DUR_BLOOD_FOR_BLOOD])
        {
            if (!quiet)
                mpr("You have already called for blood!");
            return false;
        }
        else if (!you.props.exists(BEOGH_RES_PIETY_NEEDED_KEY))
        {
            if (!quiet)
                mpr("Your apostles are all alive!");
            return false;
        }
        else if (!tile_has_valid_bfb_corpse(you.pos()))
        {
            if (!quiet)
                mpr("You must be standing atop the corpse of a fallen apostle!");
            return false;
        }
        return true;

    case ABIL_DITHMENOS_SHADOWSLIP:
    {
        const string reason = dithmenos_cannot_shadowslip_reason();
        if (!reason.empty())
        {
            if (!quiet)
                mpr(reason.c_str());
            return false;
        }
        return true;
    }

    case ABIL_DITHMENOS_APHOTIC_MARIONETTE:
    {
        const string reason = dithmenos_cannot_marionette_reason();
        if (!reason.empty())
        {
            if (!quiet)
                mpr(reason.c_str());
            return false;
        }
        return true;
    }

    case ABIL_DITHMENOS_PRIMORDIAL_NIGHTFALL:
        if (you.duration[DUR_PRIMORDIAL_NIGHTFALL])
        {
            if (!quiet)
                mpr("Night has already fallen.");
            return false;
        }
        return true;

    case ABIL_MAKHLEB_VESSEL_OF_SLAUGHTER:
        if (player_in_branch(BRANCH_CRUCIBLE))
        {
            if (!quiet)
                mpr("Mahkleb denies you. Endure the Crucible first!");
            return false;
        }
        else if (you.form == transformation::slaughter)
        {
            if (!quiet)
                mpr("You are already a vessel of slaughter!");
            return false;
        }
        return true;

    default:
        return true;
    }
}

bool check_ability_possible(const ability_type ability, bool quiet)
{
    return _check_ability_possible(get_ability_def(ability), quiet);
}

class ability_targeting_behaviour : public targeting_behaviour
{
public:
    ability_targeting_behaviour(ability_type _abil)
        : targeting_behaviour(false), abil(_abil)
    {
    }

    bool targeted() override
    {
        return !!(get_ability_flags(abil) & abflag::targeting_mask);
    }
private:
    ability_type abil;
};

static vector<string> _desc_slouch_damage(const monster_info& mi)
{
    if (!monster_at(mi.pos) || !you.can_see(*monster_at(mi.pos)))
        return vector<string>{};
    else if (!is_slouchable(mi.pos))
        return vector<string>{make_stringf("not susceptible")};
    else
        return vector<string>{make_stringf("damage: %dd3 / 2", slouch_damage(monster_at(mi.pos)))};
}

static vector<string> _desc_bind_soul_hp(const monster_info& mi)
{
    if (!monster_at(mi.pos) || !yred_can_bind_soul(monster_at(mi.pos)))
        return vector<string>{};
    return vector<string>{make_stringf("hp as a bound soul: ~%d", yred_get_bound_soul_hp(mi.type, true))};
}

static vector<string> _desc_marionette_spells(const monster_info& mi)
{
    if (mi.is(MB_SHADOWLESS) || mi.is(MB_SUMMONED) || mi.attitude != ATT_HOSTILE)
        return vector<string>();

    vector<mon_spell_slot> spells = get_unique_spells(mi);
    int num_spells = spells.size();
    int num_usable_spells = monster_at(mi.pos)->props[DITHMENOS_MARIONETTE_SPELLS_KEY].get_int();

    return vector<string>{make_stringf("%d/%d spells usable", num_usable_spells, num_spells)};
}

static vector<coord_def> _find_shadowslip_affected()
{
    vector<coord_def> targs;

    monster* shadow = dithmenos_get_player_shadow();

    // XXX: It is possible for this code to be called by attempting to use a
    //      quivvered Shadowslip with no shadow active. The targets would never
    //      be displayed in that case anyway, so return an empty list.
    if (!shadow || !shadow->alive())
        return targs;

    // All monsters in LoS of both the player and their shadow, and which are
    // currently focused on the player.
    for (monster_near_iterator mi(shadow->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (*mi != shadow && !mons_aligned(*mi, shadow)
            && you.see_cell_no_trans(mi->pos())
            && (mi->foe == MHITYOU && mi->behaviour == BEH_SEEK))
        {
            targs.push_back(mi->pos());
        }
    }

    return targs;
}

static vector<coord_def> _find_carnage_servant_targets()
{
    vector<coord_def> targs;

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        if (!mi->wont_attack() && !mi->is_firewood() && you.can_see(**mi))
            targs.push_back(mi->pos());

    return targs;
}

unique_ptr<targeter> find_ability_targeter(ability_type ability)
{
    switch (ability)
    {
    // Limited radius:
    case ABIL_ZIN_SANCTUARY:
    case ABIL_YRED_FATHOMLESS_SHACKLES:
        return make_unique<targeter_radius>(&you, LOS_DEFAULT, 4);
    case ABIL_TSO_CLEANSING_FLAME:
    case ABIL_WU_JIAN_HEAVENLY_STORM:
        return make_unique<targeter_radius>(&you, LOS_SOLID, 2);
    case ABIL_CHEIBRIADOS_TIME_BEND:
    case ABIL_USKAYAW_STOMP:
        return make_unique<targeter_maybe_radius>(&you, LOS_NO_TRANS, 1, 0, 1);

    // Multiposition:
    case ABIL_EVOKE_BLINK:
        return make_unique<targeter_multiposition>(&you, find_blink_targets());
    case ABIL_WORD_OF_CHAOS:
        return make_unique<targeter_multiposition>(&you, find_chaos_targets(true));
    case ABIL_ZIN_RECITE:
        return make_unique<targeter_multiposition>(&you, find_recite_targets());
    case ABIL_FEDHAS_WALL_OF_BRIARS:
        return make_unique<targeter_multiposition>(&you, find_briar_spaces(true), AFF_YES);
    case ABIL_QAZLAL_ELEMENTAL_FORCE:
        return make_unique<targeter_multiposition>(&you, find_elemental_targets());
    case ABIL_JIYVA_OOZEMANCY:
        return make_unique<targeter_walls>(&you, find_slimeable_walls());
    case ABIL_SIPHON_ESSENCE:
        return make_unique<targeter_siphon_essence>();
    case ABIL_DITHMENOS_SHADOWSLIP:
        return make_unique<targeter_multiposition>(&you, _find_shadowslip_affected(), AFF_YES);

    // Full LOS:
    case ABIL_QAZLAL_DISASTER_AREA: // Doesn't account for explosions hitting
                                    // areas behind glass.
    case ABIL_RU_APOCALYPSE:
        return make_unique<targeter_maybe_radius>(&you, LOS_NO_TRANS, LOS_RADIUS);
     case ABIL_LUGONU_CORRUPT:
        return make_unique<targeter_maybe_radius>(&you, LOS_DEFAULT, LOS_RADIUS);

    // Summons:
    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
    case ABIL_TROG_BROTHERS_IN_ARMS:
    case ABIL_KIKU_UNEARTH_WRETCHES:
        return make_unique<targeter_maybe_radius>(&you, LOS_NO_TRANS, 2, 0, 1);
    case ABIL_IGNIS_FOXFIRE:
        return make_unique<targeter_radius>(&you, LOS_NO_TRANS, 2, 0, 1);
    case ABIL_MAKHLEB_INFERNAL_SERVANT:
        if (you.has_mutation(MUT_MAKHLEB_MARK_CARNAGE))
            return make_unique<targeter_multiposition>(&you, _find_carnage_servant_targets(), AFF_MAYBE);
        else
            return make_unique<targeter_maybe_radius>(&you, LOS_NO_TRANS, 2, 0, 1);

    // Self-targeted:
    case ABIL_SHAFT_SELF:
#if TAG_MAJOR_VERSION == 34
    case ABIL_HEAL_WOUNDS:
#endif
    case ABIL_EVOKE_TURN_INVISIBLE:
    case ABIL_END_TRANSFORMATION:
    case ABIL_BEGIN_UNTRANSFORM:
    case ABIL_ZIN_VITALISATION:
    case ABIL_TSO_DIVINE_SHIELD:
    case ABIL_YRED_RECALL_UNDEAD_HARVEST:
    case ABIL_OKAWARU_HEROISM:
    case ABIL_OKAWARU_FINESSE:
    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
    case ABIL_TROG_BERSERK:
    case ABIL_TROG_HAND:
    case ABIL_ELYVILON_PURIFICATION:
    case ABIL_ELYVILON_HEAL_SELF:
    case ABIL_ELYVILON_DIVINE_VIGOUR:
    case ABIL_LUGONU_ABYSS_EXIT:
    case ABIL_LUGONU_ABYSS_ENTER:
    case ABIL_NEMELEX_DRAW_DESTRUCTION: // Sometimes targeted, but not always.
    case ABIL_NEMELEX_DRAW_ESCAPE:
    case ABIL_NEMELEX_DRAW_SUMMONING:
    case ABIL_NEMELEX_DRAW_STACK:
    case ABIL_NEMELEX_STACK_FIVE:
    case ABIL_BEOGH_RECALL_APOSTLES:
    case ABIL_JIYVA_SLIMIFY:
    case ABIL_CHEIBRIADOS_TIME_STEP:
    case ABIL_CHEIBRIADOS_DISTORTION:
    case ABIL_RU_DRAW_OUT_POWER:
    case ABIL_GOZAG_POTION_PETITION:
    case ABIL_GOZAG_CALL_MERCHANT:
    case ABIL_HEPLIAKLQANA_RECALL:
    case ABIL_WU_JIAN_SERPENTS_LASH:
    case ABIL_IGNIS_FIERY_ARMOUR:
    case ABIL_IGNIS_RISING_FLAME:
        return make_unique<targeter_radius>(&you, LOS_SOLID_SEE, 0);

    case ABIL_HEPLIAKLQANA_IDEALISE:
    {
        monster *ancestor = hepliaklqana_ancestor_mon();
        if (ancestor && you.can_see(*ancestor))
            return make_unique<targeter_radius>(ancestor, LOS_SOLID_SEE, 0);
        else
            return make_unique<targeter_radius>(&you, LOS_SOLID_SEE, 0);
    }

    case ABIL_YRED_BIND_SOUL:
        return make_unique<targeter_bind_soul>();

    case ABIL_CHEIBRIADOS_SLOUCH:
        return make_unique<targeter_slouch>();

    case ABIL_DITHMENOS_APHOTIC_MARIONETTE:
        return make_unique<targeter_marionette>();

    case ABIL_KIKU_SIGN_OF_RUIN:
        return make_unique<targeter_smite>(&you, LOS_RADIUS, 2, 2);

    default:
        break;
    }

    if (ability_to_zap(ability) != NUM_ZAPS)
    {
        return make_unique<targeter_beam>(&you, ability_range(ability),
                                          ability_to_zap(ability),
                                          _ability_zap_pow(ability), 0, 0);
    }

    return nullptr;
}

bool ability_has_targeter(ability_type abil)
{
    return bool(find_ability_targeter(abil));
}

bool activate_talent(const talent& tal, dist *target)
{
    const ability_def& abil = get_ability_def(tal.which);

    bolt beam;
    dist target_local;
    if (!target)
        target = &target_local;

    if (!_check_ability_possible(abil))
    {
        crawl_state.zero_turns_taken();
        return false;
    }

    const int range = ability_range(abil.ability);
    beam.range = range;

    bool is_targeted = !!(abil.flags & abflag::targeting_mask);
    unique_ptr<targeter> hitfunc = find_ability_targeter(abil.ability);

    if (is_targeted
        || hitfunc
           && (target->fire_context
               || Options.always_use_static_ability_targeters
               || Options.force_ability_targeter.count(abil.ability) > 0))
    {
        ability_targeting_behaviour beh(abil.ability);
        direction_chooser_args args;

        args.hitfunc = hitfunc.get();
        args.restricts = testbits(abil.flags, abflag::target) ? DIR_TARGET
                                                              : DIR_NONE;
        args.mode = TARG_HOSTILE;
        args.range = range;
        args.needs_path = !testbits(abil.flags, abflag::target);
        args.top_prompt = make_stringf("%s: <w>%s</w>",
                                       is_targeted ? "Aiming" : "Activating",
                                       ability_name(abil.ability).c_str());
        targeter_beam* beamfunc = dynamic_cast<targeter_beam*>(hitfunc.get());
        if (beamfunc && beamfunc->beam.hit > 0 && !beamfunc->beam.is_explosion)
            args.get_desc_func = bind(desc_beam_hit_chance, placeholders::_1, hitfunc.get());
        else if (abil.ability == ABIL_YRED_BIND_SOUL)
            args.get_desc_func = bind(_desc_bind_soul_hp, placeholders::_1);
        else if (abil.ability == ABIL_CHEIBRIADOS_SLOUCH)
            args.get_desc_func = bind(_desc_slouch_damage, placeholders::_1);
        else if (abil.ability == ABIL_DITHMENOS_APHOTIC_MARIONETTE)
        {
            args.get_desc_func = bind(_desc_marionette_spells, placeholders::_1);
            // Calculate and cache what spells are usable by each target in
            // screen so that this doesn't get recalculated numerous times as
            // the player interacts with the targeter.
            dithmenos_cache_marionette_viability();
        }

        if (abil.failure.base_chance)
        {
            args.top_prompt +=
                make_stringf(" <lightgrey>(%s risk of failure)</lightgrey>",
                             failure_rate_to_string(tal.fail).c_str());
        }
        args.behaviour = &beh;
        if (!is_targeted)
            args.default_place = you.pos();
        if (hitfunc && hitfunc->can_affect_walls())
        {
            args.show_floor_desc = true;
            args.show_boring_feats = false;
        }
        args.self = testbits(abil.flags, abflag::not_self) ?
            confirm_prompt_type::cancel : confirm_prompt_type::none;

        if (!spell_direction(*target, beam, &args))
        {
            crawl_state.zero_turns_taken();
            return false;
        }

        if (testbits(abil.flags, abflag::not_self) && target->isMe())
        {
            canned_msg(MSG_UNTHINKING_ACT);
            crawl_state.zero_turns_taken();
            return false;
        }
    }

    bool fail = random2avg(100, 3) < tal.fail;

    // Pay HP/MP costs first, so that abilities which heal the player (or kill
    // things which then cause the player to get healed) can properly cover
    // their own costs. (Also, so that Mark of Atrocity can calculate its cost
    // properly.) We will refund this later, if the ability fails or is
    // cancelled.
    const int hp_cost = abil.get_hp_cost();
    const int mp_cost = abil.get_mp_cost();

    if (mp_cost)
        pay_mp(mp_cost);

    if (hp_cost)
        pay_hp(hp_cost);

    const spret ability_result = _do_ability(abil, fail, target, beam);
    switch (ability_result)
    {
        case spret::success:
        {
            ASSERT(!fail);
            practise_using_ability(abil.ability);
            _finalize_ability_costs(abil, mp_cost, hp_cost);

            // Ephemeral Shield activates on any invocation with a cost,
            // even if that's just a cooldown or small amounts of HP.
            // No rapidly wall-jumping or renaming your ancestor, alas.
            if (is_religious_ability(abil.ability)
                && (abil.piety_cost || (abil.flags & abflag::exhaustion)
                    || (abil.flags & abflag::max_hp_drain)
                    || (abil.ability == ABIL_ZIN_RECITE)
                    || (abil.flags & abflag::card) || (abil.flags & abflag::gold)
                    || (abil.flags & abflag::sacrifice)
                    || (abil.flags & abflag::torment)
                    || (abil.flags & abflag::injury) || abil.get_hp_cost() > 0
                    || abil.get_mp_cost() > 0)
                && you.has_mutation(MUT_EPHEMERAL_SHIELD))
            {
                you.set_duration(DUR_EPHEMERAL_SHIELD, random_range(3, 5));
                you.redraw_armour_class = true;
            }

            // XXX: Merge Dismiss Apostle #1/2/3 into a single count
            ability_type log_type = abil.ability;
            if (log_type == ABIL_BEOGH_DISMISS_APOSTLE_2
                || log_type == ABIL_BEOGH_DISMISS_APOSTLE_3)
            {
                log_type = ABIL_BEOGH_DISMISS_APOSTLE_1;
            }

            count_action(tal.is_invocation ? CACT_INVOKE : CACT_ABIL, log_type);
            return true;
        }
        case spret::fail:
            if (!testbits(abil.flags, abflag::quiet_fail))
                mpr("You fail to use your ability.");
            you.turn_is_over = true;
            if (mp_cost)
                refund_mp(mp_cost);
            if (hp_cost)
                refund_hp(hp_cost);
            return false;
        case spret::abort:
            crawl_state.zero_turns_taken();
            if (mp_cost)
                refund_mp(mp_cost);
            if (hp_cost)
                refund_hp(hp_cost);
            return false;
        case spret::none:
        default:
            die("Weird ability return type");
            return false;
    }
}

/// If the player is stationary, print 'You cannot move.' and return true.
static bool _abort_if_stationary()
{
    if (you.is_motile())
        return false;

    canned_msg(MSG_CANNOT_MOVE);
    return true;
}

static bool _cleansing_flame_affects(const actor *act)
{
    return act->res_holy_energy() < 3
           && !never_harm_monster(&you, act->as_monster());
}

static int _orb_of_dispater_power()
{
    return you.skill(SK_EVOCATIONS, 8);
}

static bool _evoke_orb_of_dispater(dist *target)
{
    int power = _orb_of_dispater_power();

    if (your_spells(SPELL_HURL_DAMNATION, power, false, nullptr, target)
        == spret::abort)
    {
        return false;
    }
    mpr("You feel the orb feeding on your energy!");
    return true;
}

static bool _evoke_staff_of_olgreb(dist *target)
{
    int power = div_rand_round(20 + you.skill(SK_EVOCATIONS, 20), 4);

    if (your_spells(SPELL_OLGREBS_TOXIC_RADIANCE, power, false, nullptr, target)
        == spret::abort)
    {
        return false;
    }
    did_god_conduct(DID_WIZARDLY_ITEM, 10);
    return true;
}

static vector<monster*> _get_siphon_victims(bool known)
{
    vector<monster*> victims;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (grid_distance(you.pos(), mi->pos()) <= siphon_essence_range()
            && siphon_essence_affects(**mi)
            && (you.can_see(**mi) || !known))
        {
            victims.push_back(*mi);
        }
    }
    return victims;
}

static spret _siphon_essence(bool fail)
{
    if (_get_siphon_victims(true).empty()
        && !yesno("There are no victims visible. Siphon anyway?", true, 'n'))
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }

    fail_check();

    int damage = 0;
    bool seen = false;
    for (monster* mon : _get_siphon_victims(false))
    {
        const int dam = mon->hit_points / 2;
        mon->hurt(&you, dam, BEAM_TORMENT_DAMAGE /*dubious*/);
        damage += dam;
        if (damage && mon->observable())
        {
            simple_monster_message(*mon, " convulses!");
            behaviour_event(mon, ME_ANNOY);
            seen = true;
        }
    }

    you.increase_duration(DUR_SIPHON_COOLDOWN, 12 + random2(5));

    if (!damage)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::success;
    }

    if (seen)
        mpr("You feel stolen life flooding into you!");
    else
        mpr("You feel stolen life flooding into you from an unseen source!");

    if (you.hp == you.hp_max)
        return spret::success;

    // no death's door check because death form is incompatible with doors
    // TODO: move this into transform.cc, use proper scaling and scale meaningfully
    const int skillcap = 19 + get_form()->get_level(3);
    const int healing = div_rand_round(min(damage, skillcap) * 2, 3); // max 67
    inc_hp(healing);
    canned_msg(MSG_GAIN_HEALTH);
    return spret::success;
}

static spret _do_draconian_breath(const ability_def& abil, dist *target, bool fail)
{
    spret result = spret::abort;

    const int pow = you.form == transformation::dragon ? you.experience_level * 2
                                                       : you.experience_level;

    result = your_spells(breath_to_spell[abil.ability], pow,
                            false, nullptr, target, fail);

    if (result == spret::success)
        you.props[DRACONIAN_BREATH_USES_KEY].get_int() -= 1;

    return result;
}

static spret _do_cacophony()
{
    vector<item_def*> eq = you.equipment.get_slot_items(SLOT_HAUNTED_AUX);

    bool did_something = false;
    for (item_def* item : eq)
    {
        int mitm_slot = get_mitm_slot(10);

        // This should be practically impossible, but if we can't make any
        // item for our animated armour, abort in the case we've done nothing
        // at all. If somehow we made at least one, just keep going.
        if (mitm_slot == NON_ITEM)
        {
            if (!did_something)
            {
                canned_msg(MSG_NOTHING_HAPPENS);
                return spret::abort;
            }
        }

        mgen_data mg(MONS_HAUNTED_ARMOUR, BEH_FRIENDLY, you.pos(), MHITYOU,
                      MG_FORCE_BEH | MG_AUTOFOE);
        mg.set_summoned(&you, MON_SUMM_CACOPHONY, 0, false);
        mg.set_range(1, 3);
        mg.hd = you.experience_level * 2 / 3 + (item->plus * 2);
        monster* armour = create_monster(mg);
        if (!armour)
            continue;

        did_something = true;

        item_def &fake_armour = env.item[mitm_slot];
        fake_armour.clear();
        fake_armour = *item;
        fake_armour.flags |= ISFLAG_SUMMONED | ISFLAG_IDENTIFIED;
        armour->pickup_item(fake_armour, false, true);
        armour->speed_increment = 80;
    }

    if (!did_something)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::abort;
    }

    you.duration[DUR_CACOPHONY] = random_range(300, 400);
    you.props[CACOPHONY_XP_KEY] = 150;
    mpr("You send your armour flying and begin to make an unholy cacophony!");

    return spret::success;
}

/*
 * Use an ability.
 *
 * @param abil The actual ability used.
 * @param fail If true, the ability is doomed to fail, and spret::fail will
 * be returned if the ability is not spret::aborted.
 * @returns Whether the spell succeeded (spret::success), failed (spret::fail),
 *  or was canceled (spret::abort). Never returns spret::none.
 */
static spret _do_ability(const ability_def& abil, bool fail, dist *target,
                         bolt beam)
{
    // Note: the costs will not be applied until after this switch
    // statement... it's assumed that only failures have returned! - bwr
    switch (abil.ability)
    {
#if TAG_MAJOR_VERSION == 34
    case ABIL_HEAL_WOUNDS:
        fail_check();
        if (one_chance_in(4))
        {
            mpr("Your magical essence is drained by the effort!");
            rot_mp(1);
        }
        potionlike_effect(POT_HEAL_WOUNDS, 40);
        break;
#endif

    case ABIL_DIG:
        if (!you.digging)
        {
            you.digging = true;
            mpr("You extend your mandibles.");
        }
        else
        {
            you.digging = false;
            mpr("You retract your mandibles.");
        }
        break;

    case ABIL_SHAFT_SELF:
        if (you.can_do_shaft_ability(false))
        {
            if (cancel_harmful_move())
                return spret::abort;

            if (beogh_cancel_leaving_floor())
                return spret::abort;

            if (yesno("Are you sure you want to shaft yourself?", true, 'n'))
                start_delay<ShaftSelfDelay>(1);
            else
            {
                canned_msg(MSG_OK);
                return spret::abort;
            }
        }
        else
            return spret::abort;
        break;

    case ABIL_HOP:
        if (_can_hop(false))
            return frog_hop(fail, target);
        else
            return spret::abort;

    case ABIL_BLINKBOLT:
        if (!_can_blinkbolt(false))
            return spret::abort;
        {
            const int power = get_form()->get_level(200) / 27;
            return your_spells(SPELL_BLINKBOLT, power, false, nullptr, target);
        }

    case ABIL_SIPHON_ESSENCE:
        return _siphon_essence(fail);

    case ABIL_SPIT_POISON:      // Naga poison spit
    {
        int power = 10 + you.experience_level;

        if (!player_tracer(ZAP_SPIT_POISON, power, beam))
            return spret::abort;
        else
        {
            fail_check();
            zapping(ZAP_SPIT_POISON, power, beam);
            you.set_duration(DUR_BREATH_WEAPON, 3 + random2(5));
        }
        break;
    }

    case ABIL_IMBUE_SERVITOR:
        return imbue_servitor();

    case ABIL_IMPRINT_WEAPON:
        {
            item_def *wpn = nullptr;
            auto success = use_an_item_menu(wpn, OPER_ANY, OSEL_ARTEFACT_WEAPON,
                                "Select an artefact weapon to imprint upon your Paragon.",
                                [=](){return true;});

            if (success == OPER_NONE)
                return spret::abort;

            if (god_hates_item(*wpn))
            {
                mprf(MSGCH_WARN, "%s forbids using such a weapon!",
                     god_name(you.religion).c_str());
                return spret::abort;
            }

            static const vector<int> forbidden_unrands =
            {
                UNRAND_POWER,
            };

            // Currently excluding the same weapons that Manifold Assault does
            // (because they work weirdly for projected attacks), though a
            // larger number than that are *unsafe*. Is it okay if the player
            // knowingly imprints something dangerous-but-fun? Maybe?
            for (int urand : forbidden_unrands)
            {
                if (is_unrandom_artefact(*wpn, urand))
                {
                    mprf(MSGCH_WARN, "That weapon cannot be imprinted.");
                    return spret::abort;
                }
            }

            if (monster* paragon = find_player_paragon())
                paragon->del_ench(ENCH_SUMMON_TIMER);

            item_def replica = item_def(*wpn);
            start_delay<ImprintDelay>(5, replica);
        }
        break;

    case ABIL_COMBUSTION_BREATH:
    case ABIL_GLACIAL_BREATH:
    case ABIL_NULLIFYING_BREATH:
    case ABIL_STEAM_BREATH:
    case ABIL_NOXIOUS_BREATH:
    case ABIL_CAUSTIC_BREATH:
    case ABIL_GALVANIC_BREATH:
    case ABIL_MUD_BREATH:
        return _do_draconian_breath(abil, target, fail);

    case ABIL_BREATHE_FIRE:
    case ABIL_BREATHE_POISON:
    {
        spret result = spret::abort;
        int cooldown = 3 + random2(10) + random2(30 - you.experience_level);

        static map<ability_type, string> breath_message =
        {
            { ABIL_BREATHE_FIRE, "You breathe a blast of fire." },
            { ABIL_BREATHE_POISON, "You exhale a blast of poison gas." },
        };

        result = zapping(ability_to_zap(abil.ability),
                         _ability_zap_pow(abil.ability), beam, true,
                         breath_message[abil.ability].c_str(), fail);

        if (result == spret::success)
            you.increase_duration(DUR_BREATH_WEAPON, cooldown);

        return result;
    }

    case ABIL_EVOKE_BLINK:      // randarts
        return cast_blink(min(50, 1 + you.skill(SK_EVOCATIONS, 3)), fail);

    case ABIL_EVOKE_DISPATER:
        if (!_evoke_orb_of_dispater(target))
            return spret::abort;
        break;

    case ABIL_EVOKE_OLGREB:
        if (!_evoke_staff_of_olgreb(target))
            return spret::abort;
        break;

    // DEMONIC POWERS:
    case ABIL_DAMNATION:
        return your_spells(SPELL_HURL_DAMNATION, _hurl_damnation_power(),
                           false, nullptr, target, fail);

    case ABIL_WORD_OF_CHAOS:
        return word_of_chaos(40 + you.experience_level * 6, fail);

    case ABIL_EVOKE_TURN_INVISIBLE:     // cloaks, randarts
        if (!invis_allowed())
            return spret::abort;
        if (_invis_causes_drain())
            drain_player(40, false, true); // yes, before the fail check!
        fail_check();
        potionlike_effect(POT_INVISIBILITY, you.skill(SK_EVOCATIONS, 2) + 5);
        contaminate_player(1000 + random2(500), true);
        break;

    case ABIL_END_TRANSFORMATION:
        if (feat_dangerous_for_form(you.default_form, env.grid(you.pos())))
        {
            mprf("Turning back right now would cause you to %s!",
                    env.grid(you.pos()) == DNGN_LAVA ? "burn" : "drown");
            return spret::abort;
        }
        return_to_default_form();
        break;

    case ABIL_BEGIN_UNTRANSFORM:
        if (feat_dangerous_for_form(transformation::none, env.grid(you.pos())))
        {
            mprf("Turning back right now would cause you to %s!",
                    env.grid(you.pos()) == DNGN_LAVA ? "burn" : "drown");
            return spret::abort;
        }
        if (!i_feel_safe(true) && !yesno("Still begin untransforming?", true, 'n'))
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
        start_delay<TransformDelay>(transformation::none, nullptr);
        break;

    case ABIL_INVENT_GIZMO:
        if (!coglin_invent_gizmo())
            return spret::abort;
        break;

    case ABIL_CACOPHONY:
        return _do_cacophony();

    case ABIL_BAT_SWARM:
    {
        mpr("You scatter into a swarm of vampire bats!");
        transform(random2(12), transformation::bat_swarm);
        you.transform_uncancellable = true;
        const int pow = get_form()->get_level(10);
        big_cloud(CLOUD_BATS, &you, you.pos(), 18 + pow / 20, 8 + pow / 15, 1);
        you.props[BATFORM_XP_KEY] = 90;
        break;
    }

    case ABIL_ENKINDLE:
    {
        draw_ring_animation(you.pos(), 3, LIGHTCYAN, CYAN, true);
        mprf(MSGCH_DURATION, "Your flames flare with remembrance!");
        you.duration[DUR_ENKINDLED] = (random_range(12, 20)
                                       + (you.props[ENKINDLE_CHARGES_KEY].get_int() * 3))
                                            * BASELINE_DELAY;
        you.props.erase(ENKINDLE_PROGRESS_KEY);
        break;
    }

    // INVOCATIONS:
    case ABIL_ZIN_RECITE:
    {
        if (zin_check_recite_to_monsters() == 1)
        {
            fail_check();
            you.attribute[ATTR_RECITE_TYPE] =
                (recite_type) random2(NUM_RECITE_TYPES); // This is just flavor
            you.attribute[ATTR_RECITE_SEED] = random2(2187); // 3^7
            you.duration[DUR_RECITE] = 3 * BASELINE_DELAY;
            mpr("You clear your throat and prepare to recite.");
        }
        else
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
        break;
    }
    case ABIL_ZIN_VITALISATION:
        fail_check();
        zin_vitalisation();
        break;

    case ABIL_ZIN_IMPRISON:
        return zin_imprison(beam.target, fail);

    case ABIL_ZIN_SANCTUARY:
        fail_check();
        zin_sanctuary();
        break;

    case ABIL_ZIN_DONATE_GOLD:
        if (!zin_donate_gold())
            return spret::abort;
        break;

    case ABIL_TSO_DIVINE_SHIELD:
        fail_check();
        tso_divine_shield();
        break;

    case ABIL_TSO_CLEANSING_FLAME:
    {
        targeter_radius hitfunc(&you, LOS_SOLID, 2);
        {
            if (stop_attack_prompt(hitfunc, "invoke Cleansing Flame",
                                   _cleansing_flame_affects))
            {
                return spret::abort;
            }
        }
        fail_check();
        cleansing_flame(_tso_cleansing_flame_power(),
                        cleansing_flame_source::invocation, you.pos(), &you);
        break;
    }

    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
        if (stop_summoning_prompt(MR_RES_POISON, M_FLIES))
            return spret::abort;
        fail_check();
        summon_holy_warrior(you.skill(SK_INVOCATIONS, 4), false);
        break;

    case ABIL_TSO_BLESS_WEAPON:
        simple_god_message(" will bless one of your weapons.");
        // included in default force_more_message
        if (!bless_weapon(GOD_SHINING_ONE, SPWPN_HOLY_WRATH, YELLOW))
            return spret::abort;
        break;

    case ABIL_KIKU_UNEARTH_WRETCHES:
        fail_check();
        kiku_unearth_wretches();
        break;

    case ABIL_KIKU_SIGN_OF_RUIN:
        fail_check();
        mpr("You invoke the name of Kikubaaqudgha!");
        cast_sign_of_ruin(you, beam.target,
                          (4 + you.skill_rdiv(SK_NECROMANCY, 4, 7)) * BASELINE_DELAY);
        break;

    case ABIL_KIKU_BLESS_WEAPON:
        simple_god_message(" will bloody one of your weapons with pain.");
        // included in default force_more_message
        if (!bless_weapon(GOD_KIKUBAAQUDGHA, SPWPN_PAIN, RED))
            return spret::abort;
        break;

    case ABIL_KIKU_GIFT_CAPSTONE_SPELLS:
        if (!kiku_gift_capstone_spells())
            return spret::abort;
        break;

    case ABIL_YRED_RECALL_UNDEAD_HARVEST:
        fail_check();
        do_player_recall(recall_t::yred);
        break;

    case ABIL_YRED_LIGHT_THE_TORCH:
        yred_light_the_torch();
        break;

    case ABIL_YRED_HURL_TORCHLIGHT:
    {
        spret result = your_spells(SPELL_HURL_TORCHLIGHT,
                                   _yred_hurl_torchlight_power(),
                                   false, nullptr, target, fail);

        if (result == spret::success)
            you.props[YRED_TORCH_POWER_KEY].get_int() -= 1;

        return result;
    }

    case ABIL_YRED_FATHOMLESS_SHACKLES:
        fail_check();
        mprf(MSGCH_DURATION, "You call down Yredelemnul's inexorable grip.");
        // XXX: Some invo formula
        you.duration[DUR_FATHOMLESS_SHACKLES] = random_range(15, 25) * BASELINE_DELAY;
        yred_make_blasphemy();
        invalidate_agrid(true);
        break;

    case ABIL_YRED_BIND_SOUL:
    {
        monster* mons = monster_at(beam.target);

        if (mons && you.can_see(*mons) && mons->is_illusion())
        {
            fail_check();
            mprf("You attempt to bind %s soul, but %s is merely a clone!",
                 mons->name(DESC_ITS).c_str(),
                 mons->pronoun(PRONOUN_SUBJECTIVE).c_str());
            // Still costs a turn to gain the information.
            return spret::success;
        }

        fail_check();

        int pain = you.hp / 3;
        dec_hp(pain, false);
        mons->add_ench(mon_enchant(ENCH_SOUL_RIPE, pain, &you,
                                   INFINITE_DURATION));
        mprf("You wrap your dark will around %s soul!",
              mons->name(DESC_ITS).c_str());
        break;
    }

    case ABIL_OKAWARU_HEROISM:
        fail_check();
        mprf(MSGCH_DURATION, you.duration[DUR_HEROISM]
             ? "You feel more confident with your borrowed prowess."
             : "You gain the combat prowess of a mighty hero.");

        you.increase_duration(DUR_HEROISM,
                              10 + random2avg(you.skill(SK_INVOCATIONS, 6), 2),
                              100);
        you.redraw_evasion      = true;
        you.redraw_armour_class = true;
        break;

    case ABIL_OKAWARU_FINESSE:
        fail_check();
        if (you.duration[DUR_FINESSE])
        {
            // "Your [hand(s)] get{s} new energy."
            mprf(MSGCH_DURATION, "%s",
                 you.hands_act("get", "new energy.").c_str());
        }
        else
            mprf(MSGCH_DURATION, "You can now deal lightning-fast blows.");

        you.increase_duration(DUR_FINESSE,
                              10 + random2avg(you.skill(SK_INVOCATIONS, 6), 2),
                              100);

        did_god_conduct(DID_HASTY, 8); // Currently irrelevant.
        break;

    case ABIL_OKAWARU_DUEL:
        return okawaru_duel(beam.target, fail);

    case ABIL_OKAWARU_GIFT_WEAPON:
        if (!okawaru_gift_weapon())
            return spret::abort;
        break;

    case ABIL_OKAWARU_GIFT_ARMOUR:
        if (!okawaru_gift_armour())
            return spret::abort;
        break;

    case ABIL_MAKHLEB_ANNIHILATION:
        return cast_iood(&you, _makhleb_annihilation_power(), &beam, 0, 0, MHITNOT,
                         fail, false, MONS_GLOBE_OF_ANNIHILATION);

    case ABIL_MAKHLEB_DESTRUCTION:
        return makhleb_unleash_destruction(_makhleb_destruction_power(), beam, fail);

    case ABIL_MAKHLEB_INFERNAL_SERVANT:
        if (you.has_mutation(MUT_MAKHLEB_MARK_CARNAGE)
            && get_dist_to_nearest_monster() > you.current_vision)
        {
            mprf("You can't see any nearby targets.");
            return spret::abort;
        }

        fail_check();
        makhleb_infernal_servant();
        break;

    case ABIL_MAKHLEB_INFERNAL_LEGION:
        return makhleb_infernal_legion(fail);

    case ABIL_MAKHLEB_VESSEL_OF_SLAUGHTER:
        fail_check();
        makhleb_vessel_of_slaughter();
        break;

    case ABIL_MAKHLEB_BRAND_SELF_1:
    case ABIL_MAKHLEB_BRAND_SELF_2:
    case ABIL_MAKHLEB_BRAND_SELF_3:
        makhleb_inscribe_mark(makhleb_ability_to_mutation(abil.ability));
        break;

    case ABIL_TROG_BERSERK:
        fail_check();
        // Trog abilities don't use or train invocations.
        you.go_berserk(true);
        break;

    case ABIL_TROG_HAND:
        fail_check();
        // Trog abilities don't use or train invocations.
        trog_do_trogs_hand(you.piety / 2);
        break;

    case ABIL_TROG_BROTHERS_IN_ARMS:
        fail_check();
        // Trog abilities don't use or train invocations.
        summon_berserker(you.piety +
                         random2(you.piety/4) - random2(you.piety/4),
                         &you);
        break;

    case ABIL_SIF_MUNA_FORGET_SPELL:
        if (cast_selective_amnesia() <= 0)
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
        break;

    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
        fail_check();
        mpr("You channel some magical energy.");
        you.increase_duration(DUR_CHANNEL_ENERGY,
            4 + random2avg(you.skill_rdiv(SK_INVOCATIONS, 2, 3), 2), 100);
        break;

    case ABIL_SIF_MUNA_DIVINE_EXEGESIS:
        return divine_exegesis(fail);

    case ABIL_ELYVILON_HEAL_SELF:
    {
        fail_check();
        const int pow = min(50, 10 + you.skill_rdiv(SK_INVOCATIONS, 1, 3));
        const int healed = pow + roll_dice(2, pow) - 2;
        mpr("You are healed.");
        inc_hp(healed);
        break;
    }

    case ABIL_ELYVILON_PURIFICATION:
        fail_check();
        elyvilon_purification();
        break;

    case ABIL_ELYVILON_HEAL_OTHER:
    {
        int pow = 30 + you.skill(SK_INVOCATIONS, 1);
        return cast_healing(pow, fail);
    }

    case ABIL_ELYVILON_DIVINE_VIGOUR:
        fail_check();
        elyvilon_divine_vigour();
        break;

    case ABIL_LUGONU_ABYSS_EXIT:
        if (cancel_harmful_move(false))
            return spret::abort;
        fail_check();
        down_stairs(DNGN_EXIT_ABYSS);
        break;

    case ABIL_LUGONU_BANISH:
    {
        beam.range = you.current_vision;
        const int pow = 68 + you.skill(SK_INVOCATIONS, 3);

        direction_chooser_args args;
        args.mode = TARG_HOSTILE;
        args.get_desc_func = bind(desc_wl_success_chance, placeholders::_1,
                                  zap_ench_power(ZAP_BANISHMENT, pow, false),
                                  nullptr);
        if (!spell_direction(*target, beam, &args))
            return spret::abort;

        if (beam.target == you.pos())
        {
            mpr("You cannot banish yourself!");
            return spret::abort;
        }

        fail_check();

        return zapping(ZAP_BANISHMENT, pow, beam, true, nullptr, fail);
    }

    case ABIL_LUGONU_CORRUPT:
        fail_check();
        lugonu_corrupt_level(300 + you.skill(SK_INVOCATIONS, 15));
        break;

    case ABIL_LUGONU_ABYSS_ENTER:
    {
        if (cancel_harmful_move(false))
            return spret::abort;
        fail_check();
        // Deflate HP.
        dec_hp(random2avg(you.hp, 2), false);

        no_notes nx; // This banishment shouldn't be noted.
        banished();
        break;
    }

    case ABIL_LUGONU_BLESS_WEAPON:
        simple_god_message(" will brand one of your weapons with the "
                           "corruption of the Abyss.");
        // included in default force_more_message
        if (!bless_weapon(GOD_LUGONU, SPWPN_DISTORTION, MAGENTA))
            return spret::abort;
        break;

    case ABIL_NEMELEX_DRAW_DESTRUCTION:
        if (!deck_draw(DECK_OF_DESTRUCTION))
            return spret::abort;
        break;

    case ABIL_NEMELEX_DRAW_ESCAPE:
        if (!deck_draw(DECK_OF_ESCAPE))
            return spret::abort;
        break;

    case ABIL_NEMELEX_DRAW_SUMMONING:
        if (!deck_draw(DECK_OF_SUMMONING))
            return spret::abort;
        break;

    case ABIL_NEMELEX_DRAW_STACK:
        if (!deck_draw(DECK_STACK))
            return spret::abort;
        break;

    case ABIL_NEMELEX_TRIPLE_DRAW:
        return deck_triple_draw(fail);

    case ABIL_NEMELEX_DEAL_FOUR:
        return deck_deal(fail);

    case ABIL_NEMELEX_STACK_FIVE:
        return deck_stack(fail);

    case ABIL_BEOGH_SMITING:
        return your_spells(SPELL_SMITING, _beogh_smiting_power(),
                           false, nullptr, target, fail);

    case ABIL_BEOGH_RECALL_APOSTLES:
        fail_check();
        do_player_recall(recall_t::beogh);
        break;

    case ABIL_BEOGH_RECRUIT_APOSTLE:
        beogh_recruit_apostle();
        break;

    case ABIL_BEOGH_DISMISS_APOSTLE_1:
        beogh_dismiss_apostle(1);
        break;

    case ABIL_BEOGH_DISMISS_APOSTLE_2:
        beogh_dismiss_apostle(2);
        break;

    case ABIL_BEOGH_DISMISS_APOSTLE_3:
        beogh_dismiss_apostle(3);
        break;

    case ABIL_BEOGH_BLOOD_FOR_BLOOD:
        fail_check();
        beogh_blood_for_blood();
        break;

    case ABIL_FEDHAS_WALL_OF_BRIARS:
        fail_check();
        fedhas_wall_of_briars();
        break;

    case ABIL_FEDHAS_GROW_BALLISTOMYCETE:
        return fedhas_grow_ballistomycete(beam.target, fail);

    case ABIL_FEDHAS_OVERGROW:
        return fedhas_overgrow(fail);

    case ABIL_FEDHAS_GROW_OKLOB:
        return fedhas_grow_oklob(beam.target, fail);

    case ABIL_JIYVA_SLIMIFY:
    {
        fail_check();
        const item_def* const weapon = you.weapon();
        const string msg = weapon ? weapon->name(DESC_YOUR)
                                  : ("your " + you.hand_name(true));
        mprf(MSGCH_DURATION, "A thick mucus forms on %s.", msg.c_str());
        you.increase_duration(DUR_SLIMIFY,
                              random2avg(you.piety / 4, 2) + 3, 100);
        break;
    }

    case ABIL_JIYVA_OOZEMANCY:
        return jiyva_oozemancy(fail);

    case ABIL_CHEIBRIADOS_TIME_STEP:
        fail_check();
        cheibriados_time_step(you.skill(SK_INVOCATIONS, 10));
        break;

    case ABIL_CHEIBRIADOS_TIME_BEND:
        fail_check();
        cheibriados_time_bend(16 + you.skill(SK_INVOCATIONS, 8));
        break;

    case ABIL_CHEIBRIADOS_DISTORTION:
        fail_check();
        cheibriados_temporal_distortion();
        break;

    case ABIL_CHEIBRIADOS_SLOUCH:
        return cheibriados_slouch(fail);

    case ABIL_ASHENZARI_CURSE:
        if (!ashenzari_curse_item())
            return spret::abort;
        break;

    case ABIL_ASHENZARI_UNCURSE:
        if (!ashenzari_uncurse_item())
            return spret::abort;
        break;

    case ABIL_DITHMENOS_SHADOWSLIP:
        return dithmenos_shadowslip(fail);

    case ABIL_DITHMENOS_APHOTIC_MARIONETTE:
        return dithmenos_marionette(*monster_at(beam.target), fail);

    case ABIL_DITHMENOS_PRIMORDIAL_NIGHTFALL:
        return dithmenos_nightfall(fail);

    case ABIL_GOZAG_POTION_PETITION:
        run_uncancel(UNC_POTION_PETITION, 0);
        break;

    case ABIL_GOZAG_CALL_MERCHANT:
        run_uncancel(UNC_CALL_MERCHANT, 0);
        break;

    case ABIL_GOZAG_BRIBE_BRANCH:
        if (!gozag_bribe_branch())
            return spret::abort;
        break;

    case ABIL_QAZLAL_UPHEAVAL:
        return qazlal_upheaval(coord_def(), false, fail, target);

    case ABIL_QAZLAL_ELEMENTAL_FORCE:
        return qazlal_elemental_force(fail);

    case ABIL_QAZLAL_DISASTER_AREA:
        return qazlal_disaster_area(fail);

    case ABIL_RU_SACRIFICE_PURITY:
    case ABIL_RU_SACRIFICE_WORDS:
    case ABIL_RU_SACRIFICE_DRINK:
    case ABIL_RU_SACRIFICE_ESSENCE:
    case ABIL_RU_SACRIFICE_HEALTH:
    case ABIL_RU_SACRIFICE_STEALTH:
    case ABIL_RU_SACRIFICE_ARTIFICE:
    case ABIL_RU_SACRIFICE_LOVE:
    case ABIL_RU_SACRIFICE_COURAGE:
    case ABIL_RU_SACRIFICE_ARCANA:
    case ABIL_RU_SACRIFICE_NIMBLENESS:
    case ABIL_RU_SACRIFICE_DURABILITY:
    case ABIL_RU_SACRIFICE_HAND:
    case ABIL_RU_SACRIFICE_EXPERIENCE:
    case ABIL_RU_SACRIFICE_SKILL:
    case ABIL_RU_SACRIFICE_EYE:
    case ABIL_RU_SACRIFICE_RESISTANCE:
    case ABIL_RU_SACRIFICE_FORMS:
        if (!ru_do_sacrifice(abil.ability))
            return spret::abort;
        break;

    case ABIL_RU_REJECT_SACRIFICES:
        if (!ru_reject_sacrifices())
            return spret::abort;
        break;

    case ABIL_RU_DRAW_OUT_POWER:
        ru_draw_out_power();
        you.increase_duration(DUR_EXHAUSTED, 12 + random2(5));
        break;

    case ABIL_RU_POWER_LEAP:
        if (_abort_if_stationary() || cancel_harmful_move())
            return spret::abort;
        if (!ru_power_leap()) // TODO dist arg
            return spret::abort;
        you.increase_duration(DUR_EXHAUSTED, 18 + random2(8));
        break;

    case ABIL_RU_APOCALYPSE:
        if (!ru_apocalypse())
            return spret::abort;
        you.increase_duration(DUR_EXHAUSTED, 30 + random2(20));
        break;

    case ABIL_USKAYAW_STOMP:
        if (!uskayaw_stomp())
            return spret::abort;
        break;

    case ABIL_USKAYAW_LINE_PASS:
        if (_abort_if_stationary() || cancel_harmful_move())
            return spret::abort;
        if (!uskayaw_line_pass()) // TODO dist arg
            return spret::abort;
        break;

    case ABIL_USKAYAW_GRAND_FINALE:
        if (cancel_harmful_move(false))
            return spret::abort;
        return uskayaw_grand_finale(fail); // TODO dist arg

    case ABIL_HEPLIAKLQANA_IDEALISE:
        return hepliaklqana_idealise(fail);

    case ABIL_HEPLIAKLQANA_RECALL:
        if (try_recall(hepliaklqana_ancestor()))
            upgrade_hepliaklqana_ancestor(true);
        break;

    case ABIL_HEPLIAKLQANA_TRANSFERENCE:
        return hepliaklqana_transference(fail); // TODO: dist arg

    case ABIL_HEPLIAKLQANA_TYPE_KNIGHT:
    case ABIL_HEPLIAKLQANA_TYPE_BATTLEMAGE:
    case ABIL_HEPLIAKLQANA_TYPE_HEXER:
        if (!hepliaklqana_choose_ancestor_type(abil.ability))
            return spret::abort;
        break;

    case ABIL_HEPLIAKLQANA_IDENTITY:
        hepliaklqana_choose_identity();
        break;

    case ABIL_WU_JIAN_SERPENTS_LASH:
        mprf(MSGCH_GOD, "Your muscles tense, ready for explosive movement...");
        you.attribute[ATTR_SERPENTS_LASH] = 2;
        you.redraw_status_lights = true;
        return spret::success;

    case ABIL_WU_JIAN_HEAVENLY_STORM:
        fail_check();
        wu_jian_heavenly_storm();
        break;

    case ABIL_WU_JIAN_WALLJUMP:
        return wu_jian_wall_jump_ability();

    case ABIL_IGNIS_FIERY_ARMOUR:
        fiery_armour();
        return spret::success;

    case ABIL_IGNIS_FOXFIRE:
        return foxfire_swarm();

    case ABIL_IGNIS_RISING_FLAME:
        if (!_can_rising_flame(false))
            return spret::abort;
        mpr("You begin to rise into the air.");
        // slightly faster than teleport
        you.set_duration(DUR_RISING_FLAME, 2 + random2(3));
        you.one_time_ability_used.set(GOD_IGNIS);
        return spret::success;

    case ABIL_RENOUNCE_RELIGION:
        if (yesno("Really renounce your faith, foregoing its fabulous benefits?",
                  false, 'n')
            && yesno("Are you sure?", false, 'n'))
        {
            excommunication(true);
        }
        else
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
        break;

    case ABIL_CONVERT_TO_BEOGH:
        god_pitch(GOD_BEOGH);
        if (you_worship(GOD_BEOGH))
        {
            spare_beogh_convert();
            break;
        }
        return spret::abort;

#ifdef WIZARD
    case ABIL_WIZ_BUILD_TERRAIN:
    {
        int &last_feat = you.props[WIZ_LAST_FEATURE_TYPE_PROP].get_int();
        if (!wizard_create_feature(*target,
                        static_cast<dungeon_feature_type>(last_feat), false))
        {
            return spret::abort;
        }
        break;
    }
    case ABIL_WIZ_CLEAR_TERRAIN:
        if (!wizard_create_feature(*target, DNGN_FLOOR, false))
            return spret::abort;
        break;
    case ABIL_WIZ_SET_TERRAIN:
    {
        auto feat = wizard_select_feature(false);
        if (feat == DNGN_UNSEEN)
            return spret::abort;
        you.props[WIZ_LAST_FEATURE_TYPE_PROP] = static_cast<int>(feat);
        mprf("Now building '%s'", dungeon_feature_name(feat));
        break;
    }
#endif
    case ABIL_NON_ABILITY:
        mpr("Sorry, you can't do that.");
        break;

    default:
        die("invalid ability");
    }

    return spret::success;
}

// Pay piety and time costs, and flush UI for HP/MP costs which have already
// been paid.
static void _finalize_ability_costs(const ability_def& abil, int mp_cost, int hp_cost)
{
    // wall jump handles its own timing, because it can be instant if
    // serpent's lash is activated.
    if (abil.flags & abflag::instant)
    {
        you.turn_is_over = false;
        you.elapsed_time_at_last_input = you.elapsed_time;
        update_turn_count();
    }
    else if (abil.ability != ABIL_WU_JIAN_WALLJUMP)
        you.turn_is_over = true;

    const int piety_cost = abil.piety_cost.cost();

    dprf("Cost: mp=%d; hp=%d; piety=%d",
         mp_cost, hp_cost, piety_cost);

    if (piety_cost)
        lose_piety(piety_cost);

    if (mp_cost || hp_cost)
        finalize_mp_cost(hp_cost);

    // This should trigger off using invocations that cost HP
    if (hp_cost)
        makhleb_celebrant_bloodrite();
}

int choose_ability_menu(const vector<talent>& talents)
{
    // TODO: refactor this into a proper subclass
    ToggleableMenu abil_menu(MF_SINGLESELECT | MF_ANYPRINTABLE
            | MF_NO_WRAP_ROWS | MF_TOGGLE_ACTION | MF_ARROWS_SELECT
            | MF_INIT_HOVER);

    abil_menu.set_highlighter(nullptr);
#ifdef USE_TILE_LOCAL
    {
        // Hack like the one in spl-cast.cc:list_spells() to align the title.
        ToggleableMenuEntry* me =
            new ToggleableMenuEntry("Ability - do what?                  "
                                    "Cost                            Failure",
                                    "Ability - describe what?            "
                                    "Cost                            Failure",
                                    MEL_ITEM);
        me->colour = BLUE;
        abil_menu.set_title(me, true, true);
    }
#else
    abil_menu.set_title(
        new ToggleableMenuEntry("Ability - do what?                  "
                                "Cost                            Failure",
                                "Ability - describe what?            "
                                "Cost                            Failure",
                                MEL_TITLE), true, true);
#endif
    abil_menu.set_tag("ability");
    abil_menu.add_toggle_from_command(CMD_MENU_CYCLE_MODE);
    abil_menu.add_toggle_from_command(CMD_MENU_CYCLE_MODE_REVERSE);
    // help here amounts to describing abilities:
    abil_menu.add_toggle_from_command(CMD_MENU_HELP);
    abil_menu.menu_action = Menu::ACT_EXECUTE;

    if (crawl_state.game_is_hints())
    {
        // XXX: This could be buggy if you manage to pick up lots and
        // lots of abilities during hints mode.
        abil_menu.set_more(hints_abilities_info());
    }
    else
    {
        abil_menu.set_more(formatted_string::parse_string(
            menu_keyhelp_cmd(CMD_MENU_HELP) + " toggle "
                       "between ability selection and description."));
    }

    int numbers[52];
    for (int i = 0; i < 52; ++i)
        numbers[i] = i;

    bool found_invocations = false;

    // First add all non-invocation abilities.
    for (unsigned int i = 0; i < talents.size(); ++i)
    {
        if (talents[i].is_invocation)
            found_invocations = true;
        else
        {
            ToggleableMenuEntry* me =
                new ToggleableMenuEntry(describe_talent(talents[i]),
                                        describe_talent(talents[i]),
                                        MEL_ITEM, 1, talents[i].hotkey);
            me->data = &numbers[i];
            me->add_tile(tile_def(tileidx_ability(talents[i].which)));
            if (!check_ability_possible(talents[i].which, true))
            {
                me->colour = COL_INAPPLICABLE;
                me->add_tile(tile_def(TILEI_MESH));
            }
            abil_menu.add_entry(me);
        }
    }

    if (found_invocations)
    {
#ifdef USE_TILE_LOCAL
        MenuEntry* subtitle = new MenuEntry(" Invocations -    ", MEL_ITEM);
        subtitle->colour = BLUE;
        abil_menu.add_entry(subtitle);
#else
        abil_menu.add_entry(new MenuEntry(" Invocations -    ", MEL_SUBTITLE));
#endif
        for (unsigned int i = 0; i < talents.size(); ++i)
        {
            if (talents[i].is_invocation)
            {
                ToggleableMenuEntry* me =
                    new ToggleableMenuEntry(describe_talent(talents[i]),
                                            describe_talent(talents[i]),
                                            MEL_ITEM, 1, talents[i].hotkey);
                me->data = &numbers[i];
                me->add_tile(tile_def(tileidx_ability(talents[i].which)));
                if (!check_ability_possible(talents[i].which, true))
                {
                    me->colour = COL_INAPPLICABLE;
                    me->add_tile(tile_def(TILEI_MESH));
                }
                abil_menu.add_entry(me);
            }
        }
    }

    int ret = -1;
    abil_menu.on_examine = [&talents](const MenuEntry& sel)
    {
        ASSERT(sel.hotkeys.size() == 1);
        int selected = *(static_cast<int*>(sel.data));

        _print_talent_description(talents[selected]);
        return true;
    };
    abil_menu.on_single_selection = [&ret](const MenuEntry& sel)
    {
        ASSERT(sel.hotkeys.size() == 1);
        ret = *(static_cast<int*>(sel.data));
        return false;
    };
    abil_menu.show(false);
    if (!crawl_state.doing_prev_cmd_again)
    {
        redraw_screen();
        update_screen();
    }
    return ret;
}


string describe_talent(const talent& tal)
{
    ASSERT(tal.which != ABIL_NON_ABILITY);

    const string failure = failure_rate_to_string(tal.fail);

    ostringstream desc;
    desc << left
         << chop_string(ability_name(tal.which), 32)
         << chop_string(make_cost_description(tal.which), 32)
         << chop_string(failure, 12);
    return trimmed_string(desc.str());
}

static void _add_talent(vector<talent>& vec, const ability_type ability,
                        bool check_confused)
{
    const talent t = get_talent(ability, check_confused);
    if (t.which != ABIL_NON_ABILITY)
        vec.push_back(t);
}

bool is_religious_ability(ability_type abil)
{
    // ignores abandon religion / convert to beogh
    return abil >= ABIL_FIRST_RELIGIOUS_ABILITY
        && abil <= ABIL_LAST_RELIGIOUS_ABILITY;
}

bool is_card_ability(ability_type abil)
{
    return testbits(get_ability_def(abil).flags, abflag::card);
}

bool player_has_ability(ability_type abil, bool include_unusable)
{
#ifdef WIZARD
    if (abil >= ABIL_FIRST_WIZ)
        return you.wizard;
#endif

    // TODO: consolidate fixup checks into here?
    abil = fixup_ability(abil);
    if (abil == ABIL_NON_ABILITY || abil == NUM_ABILITIES)
        return false;

    if (is_religious_ability(abil))
    {
        // TODO: something less dumb than this?
        auto god_abils = get_god_abilities(include_unusable, false,
                                               include_unusable);
        return count(god_abils.begin(), god_abils.end(), abil);
    }

    if (species::draconian_breath(you.species) == abil)
        return draconian_dragon_exception();

    switch (abil)
    {
#if TAG_MAJOR_VERSION == 34
    case ABIL_HEAL_WOUNDS:
        return you.species == SP_DEEP_DWARF;
#endif
    case ABIL_SHAFT_SELF:
        if (crawl_state.game_is_sprint() || brdepth[you.where_are_you] == 1)
            return false;
        // fallthrough
    case ABIL_DIG:
        return you.can_burrow()
               && (form_keeps_mutations() || include_unusable);
    case ABIL_HOP:
        return you.get_mutation_level(MUT_HOP);
    case ABIL_BREATHE_POISON:
        return you.get_mutation_level(MUT_SPIT_POISON) >= 2;
    case ABIL_SPIT_POISON:
        return you.get_mutation_level(MUT_SPIT_POISON) == 1;
    case ABIL_BREATHE_FIRE:
        return you.form == transformation::dragon
               && !species::is_draconian(you.species);
    case ABIL_BLINKBOLT:
        return you.form == transformation::storm;
    case ABIL_SIPHON_ESSENCE:
        return you.form == transformation::death;
    case ABIL_INVENT_GIZMO:
        return you.species == SP_COGLIN
        && !you.props.exists(INVENT_GIZMO_USED_KEY);
    case ABIL_CACOPHONY:
        return you.get_mutation_level(MUT_FORMLESS) == 2;
    case ABIL_BAT_SWARM:
        return you.form == transformation::vampire;
    case ABIL_ENKINDLE:
        return you.has_mutation(MUT_MNEMOPHAGE);
    case ABIL_IMBUE_SERVITOR:
        return you.has_spell(SPELL_SPELLSPARK_SERVITOR);
    case ABIL_IMPRINT_WEAPON:
        return you.has_spell(SPELL_PLATINUM_PARAGON);
    // mutations
    case ABIL_DAMNATION:
        return you.get_mutation_level(MUT_HURL_DAMNATION);
    case ABIL_WORD_OF_CHAOS:
        return you.get_mutation_level(MUT_WORD_OF_CHAOS)
               && (!silenced(you.pos()) || include_unusable);
    case ABIL_END_TRANSFORMATION:
        return you.form != you.default_form && !you.transform_uncancellable;
    case ABIL_BEGIN_UNTRANSFORM:
        return you.form == you.default_form
               && you.default_form != transformation::none;
    // TODO: other god abilities
    case ABIL_RENOUNCE_RELIGION:
        return !you_worship(GOD_NO_GOD);
    case ABIL_CONVERT_TO_BEOGH:
        return env.level_state & LSTATE_BEOGH && can_convert_to_beogh();
    // pseudo-evocations from equipped items
    case ABIL_EVOKE_BLINK:
        return you.scan_artefacts(ARTP_BLINK)
               && !you.get_mutation_level(MUT_NO_ARTIFICE);
    case ABIL_EVOKE_TURN_INVISIBLE:
        return you.evokable_invis()
               && !you.get_mutation_level(MUT_NO_ARTIFICE);
    case ABIL_EVOKE_DISPATER:
        return you.unrand_equipped(UNRAND_DISPATER)
               && !you.has_mutation(MUT_NO_ARTIFICE);
    case ABIL_EVOKE_OLGREB:
        return you.unrand_equipped(UNRAND_OLGREB)
               && !you.has_mutation(MUT_NO_ARTIFICE);
    default:
        // removed abilities handled here
        return false;
    }
}

/**
 * Return all relevant talents that the player has.
 *
 * Currently the only abilities that are affected by include_unusable are god
 * abilities (affect by e.g. penance or silence).
 * @param check_confused If true, abilities that don't work when confused will
 *                       be excluded.
 * @param include_unusable If true, abilities that are currently unusable will
 *                         be excluded.
 * @return  A vector of talent structs.
 */
vector<talent> your_talents(bool check_confused, bool include_unusable, bool ignore_piety)
{
    vector<talent> talents;

    // TODO: can we just iterate over ability_type?
    vector<ability_type> check_order =
        {
#if TAG_MAJOR_VERSION == 34
            ABIL_HEAL_WOUNDS,
#endif
            ABIL_DIG,
            ABIL_SHAFT_SELF,
            ABIL_HOP,
            ABIL_SPIT_POISON,
            ABIL_BREATHE_FIRE,
            ABIL_COMBUSTION_BREATH,
            ABIL_GLACIAL_BREATH,
            ABIL_BREATHE_POISON,
            ABIL_GALVANIC_BREATH,
            ABIL_NULLIFYING_BREATH,
            ABIL_STEAM_BREATH,
            ABIL_NOXIOUS_BREATH,
            ABIL_CAUSTIC_BREATH,
            ABIL_MUD_BREATH,
            ABIL_DAMNATION,
            ABIL_WORD_OF_CHAOS,
            ABIL_INVENT_GIZMO,
            ABIL_CACOPHONY,
            ABIL_BAT_SWARM,
            ABIL_ENKINDLE,
            ABIL_BLINKBOLT,
            ABIL_SIPHON_ESSENCE,
            ABIL_IMBUE_SERVITOR,
            ABIL_IMPRINT_WEAPON,
            ABIL_END_TRANSFORMATION,
            ABIL_BEGIN_UNTRANSFORM,
            ABIL_RENOUNCE_RELIGION,
            ABIL_CONVERT_TO_BEOGH,
            ABIL_EVOKE_BLINK,
            ABIL_EVOKE_TURN_INVISIBLE,
            ABIL_EVOKE_DISPATER,
            ABIL_EVOKE_OLGREB,
#ifdef WIZARD
            ABIL_WIZ_BUILD_TERRAIN,
            ABIL_WIZ_SET_TERRAIN,
            ABIL_WIZ_CLEAR_TERRAIN,
#endif
        };

    for (auto a : check_order)
        if (player_has_ability(a, include_unusable))
            _add_talent(talents, a, check_confused);


    // player_has_ability will just brute force these anyways (TODO)
    for (ability_type abil : get_god_abilities(include_unusable, ignore_piety,
                                               include_unusable))
    {
        _add_talent(talents, abil, check_confused);
    }

    // Side effect alert!
    // Find hotkeys for the non-hotkeyed talents. (XX: how does this relate
    // to the hotkey code in find_ability_slot??)
    for (talent &tal : talents)
    {
        const int index = _lookup_ability_slot(tal.which);
        if (index > -1)
        {
            tal.hotkey = index_to_letter(index);
            continue;
        }

        // Try to find a free hotkey for i, starting from Z.
        for (int k = 51; k >= 0; --k)
        {
            const int kkey = index_to_letter(k);
            bool good_key = true;

            // Check that it doesn't conflict with other hotkeys.
            for (const talent &other : talents)
                if (other.hotkey == kkey)
                {
                    good_key = false;
                    break;
                }

            if (good_key)
            {
                tal.hotkey = kkey;
                you.ability_letter_table[k] = tal.which;
                break;
            }
        }
        // In theory, we could be left with an unreachable ability
        // here (if you have 53 or more abilities simultaneously).
    }

    return talents;
}

/**
 * Maybe move an ability to the slot given by the ability_slot option.
 *
 * @param[in] slot current slot of the ability
 * @returns the new slot of the ability; may still be slot, if the ability
 *          was not reassigned.
 */
int auto_assign_ability_slot(int slot)
{
    const ability_type abil_type = you.ability_letter_table[slot];
    const string abilname = lowercase_string(ability_name(abil_type));
    bool overwrite = false;
    // check to see whether we've chosen an automatic label:
    for (auto& mapping : Options.auto_ability_letters)
    {
        // `matches` has a validity check
        if (!mapping.first.matches(abilname))
            continue;
        for (char i : mapping.second)
        {
            if (i == '+')
                overwrite = true;
            else if (i == '-')
                overwrite = false;
            else if (isaalpha(i))
            {
                const int index = letter_to_index(i);
                ability_type existing_ability = you.ability_letter_table[index];

                if (existing_ability == ABIL_NON_ABILITY
                    || existing_ability == abil_type)
                {
                    // Unassigned or already assigned to this ability.
                    you.ability_letter_table[index] = abil_type;
                    if (slot != index)
                        you.ability_letter_table[slot] = ABIL_NON_ABILITY;
                    return index;
                }
                else if (overwrite)
                {
                    const string str = lowercase_string(ability_name(existing_ability));
                    // Don't overwrite an ability matched by the same rule.
                    if (mapping.first.matches(str))
                        continue;
                    you.ability_letter_table[slot] = abil_type;
                    swap_ability_slots(slot, index, true);
                    return index;
                }
                // else occupied, continue to the next mapping.
            }
        }
    }
    return slot;
}

// Returns an index (0-51) if already assigned, -1 if not.
static int _lookup_ability_slot(const ability_type abil)
{
    // Placeholder handling, part 2: The ability we have might
    // correspond to a placeholder, in which case the ability letter
    // table will contain that placeholder. Convert the latter to
    // its corresponding ability before comparing the two, so that
    // we'll find the placeholder's index properly.
    for (int slot = 0; slot < 52; slot++)
        if (fixup_ability(you.ability_letter_table[slot]) == abil)
            return slot;
    return -1;
}

// Assign a new ability slot if necessary. Returns an index (0-51) if
// successful, -1 if you should just use the next one.
int find_ability_slot(const ability_type abil, char firstletter)
{
    // If we were already assigned a slot, use it.
    int had_slot = _lookup_ability_slot(abil);
    if (had_slot > -1)
        return had_slot;

    // No requested slot, find new one and make it preferred.

    // firstletter defaults to 'f', because a-e is for invocations
    int first_slot = letter_to_index(firstletter);

    // Reserve the first non-god ability slot (f) for Draconian breath
    if (you.species == SP_BASE_DRACONIAN && first_slot >= letter_to_index('f'))
        first_slot += 1;

    ASSERT(first_slot < 52);

    switch (abil)
    {
    case ABIL_KIKU_GIFT_CAPSTONE_SPELLS:
        first_slot = letter_to_index('N');
        break;
    case ABIL_TSO_BLESS_WEAPON:
    case ABIL_KIKU_BLESS_WEAPON:
    case ABIL_LUGONU_BLESS_WEAPON:
    case ABIL_OKAWARU_GIFT_WEAPON:
        first_slot = letter_to_index('W');
        break;
    case ABIL_OKAWARU_GIFT_ARMOUR:
        first_slot = letter_to_index('E');
        break;
    case ABIL_CONVERT_TO_BEOGH:
        first_slot = letter_to_index('Y');
        break;
    case ABIL_RU_SACRIFICE_PURITY:
    case ABIL_RU_SACRIFICE_WORDS:
    case ABIL_RU_SACRIFICE_DRINK:
    case ABIL_RU_SACRIFICE_ESSENCE:
    case ABIL_RU_SACRIFICE_HEALTH:
    case ABIL_RU_SACRIFICE_STEALTH:
    case ABIL_RU_SACRIFICE_ARTIFICE:
    case ABIL_RU_SACRIFICE_LOVE:
    case ABIL_RU_SACRIFICE_COURAGE:
    case ABIL_RU_SACRIFICE_ARCANA:
    case ABIL_RU_SACRIFICE_NIMBLENESS:
    case ABIL_RU_SACRIFICE_DURABILITY:
    case ABIL_RU_SACRIFICE_HAND:
    case ABIL_RU_SACRIFICE_EXPERIENCE:
    case ABIL_RU_SACRIFICE_SKILL:
    case ABIL_RU_SACRIFICE_EYE:
    case ABIL_RU_SACRIFICE_RESISTANCE:
    case ABIL_RU_SACRIFICE_FORMS:
    case ABIL_RU_REJECT_SACRIFICES:
    case ABIL_HEPLIAKLQANA_TYPE_KNIGHT:
    case ABIL_HEPLIAKLQANA_TYPE_BATTLEMAGE:
    case ABIL_HEPLIAKLQANA_TYPE_HEXER:
    case ABIL_HEPLIAKLQANA_IDENTITY: // move this?
    case ABIL_ASHENZARI_CURSE:
    case ABIL_ASHENZARI_UNCURSE:
    case ABIL_MAKHLEB_BRAND_SELF_1:
    case ABIL_MAKHLEB_BRAND_SELF_2:
    case ABIL_MAKHLEB_BRAND_SELF_3:
        first_slot = letter_to_index('G');
        break;

    case ABIL_BEOGH_DISMISS_APOSTLE_1:
        first_slot = letter_to_index('A');
        break;

    case ABIL_BEOGH_DISMISS_APOSTLE_2:
        first_slot = letter_to_index('B');
        break;

    case ABIL_BEOGH_DISMISS_APOSTLE_3:
        first_slot = letter_to_index('C');
        break;

    case ABIL_BEOGH_RECRUIT_APOSTLE:
        first_slot = letter_to_index('r');
        break;

    case ABIL_IMPRINT_WEAPON:
        first_slot = letter_to_index('w');
        break;

    case ABIL_RENOUNCE_RELIGION:
        first_slot = letter_to_index('X');
        break;

#ifdef WIZARD
    case ABIL_WIZ_BUILD_TERRAIN:
    case ABIL_WIZ_SET_TERRAIN:
    case ABIL_WIZ_CLEAR_TERRAIN:
        first_slot = letter_to_index('O'); // somewhat arbitrary, late in the alphabet
#endif
    default:
        break;
    }

    for (int slot = first_slot; slot < 52; ++slot)
    {
        if (you.ability_letter_table[slot] == ABIL_NON_ABILITY)
        {
            you.ability_letter_table[slot] = abil;
            return auto_assign_ability_slot(slot);
        }
    }

    // If we can't find anything else, try a-e.
    for (int slot = first_slot - 1; slot >= 0; --slot)
    {
        if (you.ability_letter_table[slot] == ABIL_NON_ABILITY)
        {
            you.ability_letter_table[slot] = abil;
            return auto_assign_ability_slot(slot);
        }
    }

    // All letters are assigned.
    return -1;
}


vector<ability_type> get_god_abilities(bool ignore_silence, bool ignore_piety,
                                       bool ignore_penance)
{
    vector<ability_type> abilities;
    if (you_worship(GOD_RU) && you.props.exists(AVAILABLE_SAC_KEY))
    {
        bool any_sacrifices = false;
        for (const auto& store : you.props[AVAILABLE_SAC_KEY].get_vector())
        {
            any_sacrifices = true;
            abilities.push_back(static_cast<ability_type>(store.get_int()));
        }
        if (any_sacrifices)
            abilities.push_back(ABIL_RU_REJECT_SACRIFICES);
    }
    if (you_worship(GOD_ASHENZARI))
    {
        if (you.props.exists(AVAILABLE_CURSE_KEY))
            abilities.push_back(ABIL_ASHENZARI_CURSE);
        if (ignore_piety || you.piety > ASHENZARI_BASE_PIETY )
            abilities.push_back(ABIL_ASHENZARI_UNCURSE);
    }
    // XXX: should we check ignore_piety?
    if (you_worship(GOD_HEPLIAKLQANA)
        && piety_rank() >= 2 && !you.props.exists(HEPLIAKLQANA_ALLY_TYPE_KEY))
    {
        for (int anc_type = ABIL_HEPLIAKLQANA_FIRST_TYPE;
             anc_type <= ABIL_HEPLIAKLQANA_LAST_TYPE;
             ++anc_type)
        {
            abilities.push_back(static_cast<ability_type>(anc_type));
        }
    }

    if (!ignore_silence && silenced(you.pos()))
    {
        if (have_passive(passive_t::wu_jian_wall_jump))
            abilities.push_back(ABIL_WU_JIAN_WALLJUMP);
        return abilities;
    }

    // Remaining abilities are unusable if silenced.
    if (you_worship(GOD_NEMELEX_XOBEH))
    {
        for (int deck = ABIL_NEMELEX_FIRST_DECK;
             deck <= ABIL_NEMELEX_LAST_DECK;
             ++deck)
        {
            abilities.push_back(static_cast<ability_type>(deck));
        }
        if (!you.props[NEMELEX_STACK_KEY].get_vector().empty())
            abilities.push_back(ABIL_NEMELEX_DRAW_STACK);
    }

    for (const auto& power : get_god_powers(you.religion))
    {
        if (god_power_usable(power, ignore_piety, ignore_penance))
        {
            const ability_type abil = fixup_ability(power.abil);
            ASSERT(abil != ABIL_NON_ABILITY);
            abilities.push_back(abil);
        }
    }

    return abilities;
}

void swap_ability_slots(int index1, int index2, bool silent)
{
    // Swap references in the letter table.
    ability_type tmp = you.ability_letter_table[index2];
    you.ability_letter_table[index2] = you.ability_letter_table[index1];
    you.ability_letter_table[index1] = tmp;

    if (!silent)
    {
        mprf_nocap("%c - %s", index_to_letter(index2),
                   ability_name(you.ability_letter_table[index2]).c_str());
    }

}

/**
 * What skill affects the success chance/power of a given skill, if any?
 *
 * @param ability       The ability in question.
 * @return              The skill that governs the ability, or SK_NONE.
 */
skill_type abil_skill(ability_type ability)
{
    ASSERT(ability != ABIL_NON_ABILITY);
    return get_ability_def(ability).failure.skill();
}

/**
 * How valuable is it to train the skill that governs this ability? (What
 * 'magnitude' does the ability have?)
 *
 * @param ability       The ability in question.
 * @return              A 'magnitude' for the ability, probably < 10.
 */
int abil_skill_weight(ability_type ability)
{
    ASSERT(ability != ABIL_NON_ABILITY);
    // This is very loosely modelled on a legacy model; fairly arbitrary.
    const int base_fail = get_ability_def(ability).failure.base_chance;
    const int floor = base_fail ? 1 : 0;
    return max(floor, div_rand_round(base_fail, 8) - 3);
}


////////////////////////////////////////////////////////////////////////
// generic_cost

int generic_cost::cost() const
{
    return base + (add > 0 ? random2avg(add, rolls) : 0);
}

int scaling_cost::cost(int max) const
{
    return fixed_val + ((scaling_val * max + 500) / 1000);
}
