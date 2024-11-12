/**
 * @file
 * @brief Functions for handling player mutations.
**/

#include "AppHdr.h"

#include "mutation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "art-enum.h"
#include "ability.h"
#include "areas.h"
#include "cio.h"
#include "coordit.h"
#include "dactions.h"
#include "database.h" // getLongDescription
#include "delay.h"
#include "describe.h"
#include "english.h"
#include "env.h"
#include "god-abil.h"
#include "god-passive.h"
#include "hints.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "melee-attack.h" // mut_aux_attack_desc
#include "menu.h"
#include "message.h"
#include "mon-place.h"
#include "notes.h"
#include "output.h"
#include "player-stats.h"
#include "religion.h"
#include "skills.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "transform.h"
#include "unicode.h"
#include "view.h"
#include "xom.h"

using namespace ui;

static bool _delete_single_mutation_level(mutation_type mutat, const string &reason, bool transient);
static string _future_mutation_description(mutation_type mut, int levels);

struct body_facet_def
{
    equipment_type eq;
    mutation_type mut;
};

struct facet_def
{
    int tier;
    mutation_type muts[3];
    int when[3];
};

struct demon_mutation_info
{
    mutation_type mut;
    int when;
    int facet;

    demon_mutation_info(mutation_type m, int w, int f)
        : mut(m), when(w), facet(f) { }
};

enum class mutflag
{
    good    = 1 << 0, // used by benemut etc
    bad     = 1 << 1, // used by malmut etc
    jiyva   = 1 << 2, // jiyva-only muts
    qazlal  = 1 << 3, // qazlal wrath
    makhleb = 1 << 4, // makhleb capstone marks

    last    = makhleb
};
DEF_BITFIELD(mutflags, mutflag, 4);
COMPILE_CHECK(mutflags::exponent(mutflags::last_exponent) == mutflag::last);

#include "mutation-data.h"

static const body_facet_def _body_facets[] =
{
    { EQ_HELMET, MUT_HORNS },
    { EQ_HELMET, MUT_ANTENNAE },
    { EQ_HELMET, MUT_BEAK },
    { EQ_GLOVES, MUT_CLAWS },
    { EQ_GLOVES, MUT_DEMONIC_TOUCH },
    { EQ_BOOTS, MUT_HOOVES },
    { EQ_CLOAK, MUT_WEAKNESS_STINGER }
};

vector<mutation_type> get_removed_mutations()
{
    static vector<mutation_type> removed_mutations =
    {
#if TAG_MAJOR_VERSION == 34
        MUT_ROUGH_BLACK_SCALES,
        MUT_BREATHE_FLAMES,
        MUT_BREATHE_POISON,
        MUT_CARNIVOROUS,
        MUT_CLING,
        MUT_CONSERVE_POTIONS,
        MUT_CONSERVE_SCROLLS,
        MUT_EXOSKELETON,
        MUT_FAST_METABOLISM,
        MUT_FLEXIBLE_WEAK,
        MUT_FOOD_JELLY,
        MUT_FUMES,
        MUT_HERBIVOROUS,
        MUT_JUMP,
        MUT_SAPROVOROUS,
        MUT_SLOW_METABOLISM,
        MUT_STRONG_STIFF,
        MUT_SUSTAIN_ATTRIBUTES,
        MUT_TELEPORT_CONTROL,
        MUT_TRAMPLE_RESISTANCE,
        MUT_MUMMY_RESTORATION,
        MUT_NO_CHARM_MAGIC,
        MUT_MIASMA_IMMUNITY,
        MUT_BLURRY_VISION,
        MUT_BLINK,
        MUT_UNBREATHING,
        MUT_GOURMAND,
        MUT_AWKWARD_TONGUE,
        MUT_NOISE_DAMPENING,
#endif
    };

    return removed_mutations;
}

/**
 * Conflicting mutation pairs. Entries are symmetric (so if A conflicts
 * with B, B conflicts with A in the same way).
 *
 * The third value in each entry means:
 *   0: If the new mutation is forced, remove all levels of the old
 *      mutation. Either way, keep scanning for more conflicts and
 *      do what they say (accepting the mutation if there are no
 *      further conflicts).
 *
 *  -1: If the new mutation is forced, remove all levels of the old
 *      mutation and scan for more conflicts. If it is not forced,
 *      fail at giving the new mutation.
 *
 *   1: If the new mutation is temporary, just allow the conflict.
 *      Otherwise, trade off: delete one level of the old mutation,
 *      don't give the new mutation, and consider it a success.
 *
 * It makes sense to have two entries for the same pair, one with value 0
 * and one with 1: that would replace all levels of the old mutation if
 * forced, or a single level if not forced. However, the 0 entry must
 * precede the 1 entry; so if you re-order this list, keep all the 0s
 * before all the 1s.
 */
static const int conflict[][3] =
{
    { MUT_REGENERATION,        MUT_INHIBITED_REGENERATION,  0},
    { MUT_FAST,                MUT_SLOW,                    0},
    { MUT_STRONG,              MUT_WEAK,                    1},
    { MUT_CLEVER,              MUT_DOPEY,                   1},
    { MUT_AGILE,               MUT_CLUMSY,                  1},
    { MUT_ROBUST,              MUT_FRAIL,                   1},
    { MUT_HIGH_MAGIC,          MUT_LOW_MAGIC,               1},
    { MUT_WILD_MAGIC,          MUT_SUBDUED_MAGIC,           1},
    { MUT_REGENERATION,        MUT_INHIBITED_REGENERATION,  1},
    { MUT_BERSERK,             MUT_CLARITY,                 1},
    { MUT_FAST,                MUT_SLOW,                    1},
    { MUT_MUTATION_RESISTANCE, MUT_DEVOLUTION,              1},
    { MUT_EVOLUTION,           MUT_DEVOLUTION,              1},
    { MUT_MUTATION_RESISTANCE, MUT_EVOLUTION,              -1},
    { MUT_FANGS,               MUT_BEAK,                   -1},
    { MUT_ANTENNAE,            MUT_HORNS,                  -1}, // currently overridden by physiology_mutation_conflict
    { MUT_BEAK,                MUT_HORNS,                  -1},
    { MUT_BEAK,                MUT_ANTENNAE,               -1},
    { MUT_HOOVES,              MUT_TALONS,                 -1},
    { MUT_CLAWS,               MUT_DEMONIC_TOUCH,          -1},
    { MUT_TRANSLUCENT_SKIN,    MUT_CAMOUFLAGE,             -1},
    { MUT_ANTIMAGIC_BITE,      MUT_ACIDIC_BITE,            -1},
    { MUT_HEAT_RESISTANCE,     MUT_HEAT_VULNERABILITY,     -1},
    { MUT_COLD_RESISTANCE,     MUT_COLD_VULNERABILITY,     -1},
    { MUT_SHOCK_RESISTANCE,    MUT_SHOCK_VULNERABILITY,    -1},
    { MUT_STRONG_WILLED,       MUT_WEAK_WILLED,            -1},
#if TAG_MAJOR_VERSION == 34
    { MUT_NO_REGENERATION,     MUT_INHIBITED_REGENERATION, -1},
    { MUT_NO_REGENERATION,     MUT_REGENERATION,           -1},
#endif
    { MUT_HP_CASTING,          MUT_HIGH_MAGIC,             -1},
    { MUT_HP_CASTING,          MUT_LOW_MAGIC,              -1},
    { MUT_HP_CASTING,          MUT_EFFICIENT_MAGIC,        -1}
};

static bool _mut_has_use(const mutation_def &mut, mutflag use)
{
    return bool(mut.uses & use);
}

static int _mut_weight(const mutation_def &mut, mutflag use)
{
    switch (use)
    {
        case mutflag::jiyva:
        case mutflag::qazlal:
            return 1;
        case mutflag::good:
        case mutflag::bad:
        default:
            return mut.weight;
    }
}

static int mut_index[NUM_MUTATIONS];
static int category_mut_index[MUT_NON_MUTATION - CATEGORY_MUTATIONS];
static map<mutflag, int> total_weight;

void init_mut_index()
{
    total_weight.clear();
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        mut_index[i] = -1;

    for (unsigned int i = 0; i < ARRAYSZ(mut_data); ++i)
    {
        const mutation_type mut = mut_data[i].mutation;
        ASSERT_RANGE(mut, 0, NUM_MUTATIONS);
        ASSERT(mut_index[mut] == -1);
        mut_index[mut] = i;
        for (const auto flag : mutflags::range())
        {
            if (_mut_has_use(mut_data[i], flag))
                total_weight[flag] += _mut_weight(mut_data[i], flag);
        }
    }

    // this is all a bit silly but ok
    for (int i = 0; i < MUT_NON_MUTATION - CATEGORY_MUTATIONS; ++i)
        category_mut_index[i] = -1;

    for (unsigned int i = 0; i < ARRAYSZ(category_mut_data); ++i)
    {
        const mutation_type mut = category_mut_data[i].mutation;
        ASSERT_RANGE(mut, CATEGORY_MUTATIONS, MUT_NON_MUTATION);
        ASSERT(category_mut_index[mut-CATEGORY_MUTATIONS] == -1);
        category_mut_index[mut-CATEGORY_MUTATIONS] = i;
    }
}

static const mutation_def& _get_mutation_def(mutation_type mut)
{
    ASSERT_RANGE(mut, 0, NUM_MUTATIONS);
    ASSERT(mut_index[mut] != -1);
    return mut_data[mut_index[mut]];
}

/*
 * Get the max number of possible levels for mutation `mut`. This is typically 1 or 3.
 *
 * @return the mutation cap.
 */
int get_mutation_cap(mutation_type mut)
{
    return _get_mutation_def(mut).levels;
}

static const mutation_category_def& _get_category_mutation_def(mutation_type mut)
{
    ASSERT_RANGE(mut, CATEGORY_MUTATIONS, MUT_NON_MUTATION);
    ASSERT(category_mut_index[mut-CATEGORY_MUTATIONS] != -1);
    return category_mut_data[category_mut_index[mut-CATEGORY_MUTATIONS]];
}

static bool _is_valid_mutation(mutation_type mut)
{
    return mut >= 0 && mut < NUM_MUTATIONS && mut_index[mut] != -1;
}

bool is_bad_mutation(mutation_type mut)
{
    return _mut_has_use(mut_data[mut_index[mut]], mutflag::bad);
}

bool is_good_mutation(mutation_type mut)
{
    return _mut_has_use(mut_data[mut_index[mut]], mutflag::good);
}

static const mutation_type _ds_scales[] =
{
    MUT_DISTORTION_FIELD,           MUT_ICY_BLUE_SCALES,
    MUT_LARGE_BONE_PLATES,          MUT_MOLTEN_SCALES,
    MUT_RUGGED_BROWN_SCALES,        MUT_SLIMY_GREEN_SCALES,
    MUT_THIN_METALLIC_SCALES,       MUT_THIN_SKELETAL_STRUCTURE,
    MUT_YELLOW_SCALES,              MUT_STURDY_FRAME,
    MUT_SANGUINE_ARMOUR,            MUT_BIG_BRAIN,
    MUT_SHARP_SCALES,
};

static bool _is_demonspawn_scale(mutation_type mut)
{
    return find(begin(_ds_scales), end(_ds_scales), mut) != end(_ds_scales);
}

bool is_body_facet(mutation_type mut)
{
    return any_of(begin(_body_facets), end(_body_facets),
                  [=](const body_facet_def &facet)
                  { return facet.mut == mut; });
}

/*
 * The degree to which `mut` is suppressed by the current form.
 *
 * @param mut  the mutation to check.
 *
 * @return  mutation_activity_type::FULL: completely available.
 *          mutation_activity_type::PARTIAL: partially suppressed.
 *          mutation_activity_type::INACTIVE: completely suppressed.
 */
mutation_activity_type mutation_activity_level(mutation_type mut)
{
    // Makhleb mutations are active in all forms, but only while worshipping.
    const mutation_def& mdef = _get_mutation_def(mut);
    if (_mut_has_use(mdef, mutflag::makhleb))
    {
        if (you_worship(GOD_MAKHLEB))
            return mutation_activity_type::FULL;
        else
            return mutation_activity_type::INACTIVE;
    }

    // First make sure the player's form permits the mutation.
    if (!form_keeps_mutations())
    {
        if (you.form == transformation::dragon)
        {
            monster_type drag = species::dragon_form(you.species);
            if (mut == MUT_SHOCK_RESISTANCE && drag == MONS_STORM_DRAGON)
                return mutation_activity_type::FULL;
            if ((mut == MUT_ACIDIC_BITE || mut == MUT_ACID_RESISTANCE)
                && drag == MONS_ACID_DRAGON)
            {
                return mutation_activity_type::FULL;
            }
            if (mut == MUT_STINGER && drag == MONS_SWAMP_DRAGON)
                return mutation_activity_type::FULL;
            if (mut == MUT_STEAM_RESISTANCE && drag == MONS_STEAM_DRAGON)
                return mutation_activity_type::FULL;
        }
        // Vampire bats keep their fangs.
        if (you.form == transformation::bat
            && you.has_innate_mutation(MUT_VAMPIRISM)
            && mut == MUT_FANGS)
        {
            return mutation_activity_type::FULL;
        }
        // XXX EVIL HACK: we want to meld coglins' exoskeletons in 'full meld'
        // forms like serpent and dragon, but treeform keeps using weapons and
        // should really keep allowing dual wielding. I'm so sorry about this.
        if (you.form == transformation::tree
            && (mut == MUT_WIELD_OFFHAND
                || mut == MUT_SLOW_WIELD
                || mut == MUT_WARMUP_STRIKES))
        {
            return mutation_activity_type::FULL;
        }
        // Dex and HP changes are kept in all forms.
#if TAG_MAJOR_VERSION == 34
        if (mut == MUT_ROUGH_BLACK_SCALES)
            return mutation_activity_type::PARTIAL;
#endif
        if (mut == MUT_RUGGED_BROWN_SCALES)
            return mutation_activity_type::PARTIAL;
        else if (_get_mutation_def(mut).form_based)
            return mutation_activity_type::INACTIVE;
    }

    if (you.form == transformation::statue)
    {
        // Statues get all but the AC benefit from scales, but are not affected
        // by other changes in body material or speed.
        switch (mut)
        {
        case MUT_GELATINOUS_BODY:
        case MUT_TOUGH_SKIN:
        case MUT_SHAGGY_FUR:
        case MUT_FAST:
        case MUT_SLOW:
        case MUT_IRIDESCENT_SCALES:
            return mutation_activity_type::INACTIVE;
#if TAG_MAJOR_VERSION == 34
        case MUT_ROUGH_BLACK_SCALES:
#endif
        case MUT_RUGGED_BROWN_SCALES:
        case MUT_SHARP_SCALES:
            return mutation_activity_type::PARTIAL;
        case MUT_YELLOW_SCALES:
        case MUT_ICY_BLUE_SCALES:
        case MUT_MOLTEN_SCALES:
        case MUT_SLIMY_GREEN_SCALES:
        case MUT_THIN_METALLIC_SCALES:
            return you.get_base_mutation_level(mut) > 2 ? mutation_activity_type::PARTIAL :
                                                          mutation_activity_type::INACTIVE;
        default:
            break;
        }
    }

    if (you.form == transformation::blade_hands
        && (mut == MUT_PAWS || mut == MUT_CLAWS))
    {
        return mutation_activity_type::INACTIVE;
    }

    if (mut == MUT_TELEPORT
        && (you.no_tele() || player_in_branch(BRANCH_ABYSS)))
    {
        return mutation_activity_type::INACTIVE;
    }

    if (mut == MUT_BERSERK && you.is_lifeless_undead())
        return mutation_activity_type::INACTIVE;

    if (!form_has_blood(you.form) && mut == MUT_SANGUINE_ARMOUR)
        return mutation_activity_type::INACTIVE;

    if (mut == MUT_TIME_WARPED_BLOOD && (!form_has_blood(you.form)
            || have_passive(passive_t::no_haste)))
    {
        return mutation_activity_type::INACTIVE;
    }

    if (mut == MUT_DEMONIC_GUARDIAN && you.allies_forbidden())
        return mutation_activity_type::INACTIVE;

    if (mut == MUT_NIMBLE_SWIMMER)
    {
        if (feat_is_water(env.grid(you.pos())))
            return mutation_activity_type::FULL;
        else
            return mutation_activity_type::INACTIVE;
    }

    return mutation_activity_type::FULL;
}

static string _suppressedmut(string desc, bool terse=false)
{
    return terse ? "(" + desc + ")" : "<darkgrey>((" + desc + "))</darkgrey>";
}

static string _innatemut(string desc, bool terse=false)
{
    return terse ? desc : "<lightblue>" + desc + "</lightblue>";
}

static string _formmut(string desc, bool terse=false)
{
    return terse ? desc : "<green>" + desc + "</green>";
}

static string _badmut(string desc, bool terse=false)
{
    return terse ? desc : "<lightred>" + desc + "</lightred>";
}

static string _annotate_form_based(string desc, bool suppressed, bool terse=false)
{
    if (suppressed)
        return _suppressedmut(desc, terse);
    return _innatemut(desc, terse);
}

static string _dragon_abil(string desc, bool terse=false)
{
    const bool supp = form_changes_physiology()
                            && you.form != transformation::dragon;
    return _annotate_form_based(desc, supp, terse);
}

tileidx_t get_mutation_tile(mutation_type mut)
{
    return _get_mutation_def(mut).tile;
}

/*
 * Does the player have mutation `mut` with at least one temporary level?
 *
 * Reminder: temporary mutations can coexist with innate or normal mutations.
 */
bool player::has_temporary_mutation(mutation_type mut) const
{
    return you.temp_mutation[mut] > 0;
}

/*
 * Does the player have mutation `mut` with at least one innate level?
 *
 * Reminder: innate mutations can coexist with temporary or normal mutations.
 */
bool player::has_innate_mutation(mutation_type mut) const
{
    return you.innate_mutation[mut] > 0;
}

/*
 * How much of mutation `mut` does the player have? This ignores form changes.
 * If all three bool arguments are false, this should always return 0.
 *
 * @param temp   include temporary mutation levels. defaults to true.
 * @param innate include innate mutation levels. defaults to true.
 * @param normal include normal (non-temp, non-innate) mutation levels. defaults to true.
 *
 * @return the total levels of the mutation.
 */
int player::get_base_mutation_level(mutation_type mut, bool innate, bool temp, bool normal) const
{
    ASSERT_RANGE(mut, 0, NUM_MUTATIONS);
    // you.mutation stores the total levels of all mutations
    int level = you.mutation[mut];
    if (!temp)
        level -= you.temp_mutation[mut];
    if (!innate)
        level -= you.innate_mutation[mut];
    if (!normal)
        level -= (you.mutation[mut] - (you.temp_mutation[mut] + you.innate_mutation[mut]));
    ASSERT(level >= 0);
    return level;
}

/*
 * How much of mutation `mut` does the player have innately?
 *
 */
int player::get_innate_mutation_level(mutation_type mut) const
{
    ASSERT_RANGE(mut, 0, NUM_MUTATIONS);
    return you.innate_mutation[mut];
}

/*
 * How much of mutation `mut` does the player have temporarily?
 *
 */
int player::get_temp_mutation_level(mutation_type mut) const
{
    ASSERT_RANGE(mut, 0, NUM_MUTATIONS);
    return you.temp_mutation[mut];
}

/*
 * Get the current player mutation level for `mut`, possibly incorporating information about forms.
 * See the other version of this function for the canonical usage of `minact`; some forms such as scale mutations
 * have different thresholds depending on the purpose and form and so will call this directly (e.g. ac
 * but not resistances are suppressed in statueform.)
 *
 * @param mut           the mutation to check
 * @param minact        the minimum activity level needed for the mutation to count as non-suppressed.
 *
 * @return a mutation level, 0 if the mutation doesn't exist or is suppressed.
 */
int player::get_mutation_level(mutation_type mut, mutation_activity_type minact) const
{
    ASSERT_RANGE(mut, 0, NUM_MUTATIONS);
    if (mutation_activity_level(mut) < minact)
        return 0;
    return get_base_mutation_level(mut, true, true);
}

/*
 * Get the current player mutation level for `mut`, possibly incorporating information about forms.
 *
 * @param mut           the mutation to check
 * @param check_form    whether to incorporate suppression from forms. Defaults to true.
 *
 * @return a mutation level, 0 if the mutation doesn't exist or is suppressed.
 */
int player::get_mutation_level(mutation_type mut, bool check_form) const
{
    return get_mutation_level(mut, check_form ? mutation_activity_type::PARTIAL :
                                                            mutation_activity_type::INACTIVE);
}

/*
 * Does the player have mutation `mut` in some form?
 */
bool player::has_mutation(mutation_type mut, bool check_form) const
{
    return get_mutation_level(mut, check_form) > 0;
}

/*
 * Test the validity of the player mutation structures, using ASSERTs.
 * Will crash on a failure.
 *
 * @debug_msg whether to output diagnostic `dprf`s in the process.
 */
void validate_mutations(bool debug_msg)
{
    if (debug_msg)
        dprf("Validating player mutations");
    int total_temp = 0;

    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        if (debug_msg && you.mutation[mut] > 0)
        {
            dprf("mutation %s: total %d innate %d temp %d",
                mutation_name(mut), you.mutation[mut],
                you.innate_mutation[mut], you.temp_mutation[mut]);
        }
        ASSERT(you.get_base_mutation_level(mut) == you.mutation[mut]);
        ASSERT(you.mutation[i] >= you.innate_mutation[mut] + you.temp_mutation[mut]);
        total_temp += you.temp_mutation[mut];

        const mutation_def& mdef = _get_mutation_def(mut);
        ASSERT(you.mutation[mut] <= mdef.levels);

        // reconstruct what the innate mutations should be based on Ds mutation schedule
        // TODO generalize to all innate muts
        if (you.species == SP_DEMONSPAWN)
        {
            bool is_trait = false;
            int trait_level = 0;
            // If the player has sacrificed xp, use the pre-sac xl; sac xp
            // doesn't remove Ds mutations.
            // You can still trick wizmode into crashing here.
            const int check_xl = (you.get_mutation_level(MUT_INEXPERIENCED)
                            && you.max_level <= you.get_experience_level() + 2)
                                ? you.max_level
                                : you.get_experience_level();
            for (player::demon_trait trait : you.demonic_traits)
            {
                if (trait.mutation == mut)
                {
                    is_trait = true;
                    if (check_xl >= trait.level_gained)
                        trait_level += 1;
                }
            }

            if (debug_msg && is_trait)
            {
                dprf("scheduled innate for %s: %d, actual %d", mutation_name(mut),
                     trait_level, you.innate_mutation[mut]);
            }
            if (is_trait)
                ASSERT(you.innate_mutation[mut] == trait_level);
        }
    }
    ASSERT(total_temp == you.attribute[ATTR_TEMP_MUTATIONS]);

// #define DEBUG_MUTATIONS
#ifdef DEBUG_MUTATIONS
    if (debug_msg)
    {
        auto removed_v = get_removed_mutations();
        unordered_set<mutation_type, hash<int>> removed(removed_v.begin(), removed_v.end());
        vector<string> no_desc;

        for (int i = 0; i < NUM_MUTATIONS; i++)
        {
            mutation_type mut = static_cast<mutation_type>(i);
            if (removed.count(mut))
                continue;
            const string name = mutation_name(mut);
            string lookup = getLongDescription(name + " mutation");
            if (lookup.empty())
                no_desc.push_back(name);
        }
        dprf("Mutations with no description: %s",
                    join_strings(no_desc.begin(), no_desc.end(), ", ").c_str());
    }
#endif
}

static string _terse_mut_name(mutation_type mut)
{
    const int current_level = you.get_mutation_level(mut);
    const int base_level = you.get_base_mutation_level(mut);
    const bool lowered = current_level < base_level;
    const int temp_levels = you.get_base_mutation_level(mut, false, true, false); // only temp levels
    const int ordinary_levels = you.get_base_mutation_level(mut, true, false, true); // excluding temp levels

    const int max_levels = mutation_max_levels(mut);

    string current = mutation_name(mut);

    if (max_levels > 1)
    {
        // add on any numeric levels
        ostringstream ostr;
        ostr << " ";
        if (ordinary_levels == 0) // only temporary levels are present
            ostr << temp_levels;
        else
        {
            // at least some non-temporary levels
            ostr << ordinary_levels;
            if (temp_levels)
                ostr << "[+" << temp_levels << "]";
        }
        current += ostr.str();
    }

    // bracket the whole thing
    if (ordinary_levels == 0)
        current = "[" + current + "]";

    if (!current.empty())
    {
        if (current_level == 0) // suppressed by form
            current = "(" + current + ")";
        if (lowered)
            current = "<darkgrey>" + current + "</darkgrey>";
    }
    return current;
}

static vector<string> _get_form_fakemuts(bool terse)
{
    vector<string> result;
    const auto *form = get_form(you.form);
    ASSERT(form);
    // we could add form->get_long_name() here for `terse`, but the line in
    // % is shown right below a line which includes the form name.
    if (!terse)
        result.push_back(_formmut(form->get_description()));

    for (const auto &p : form->get_fakemuts(terse))
        if (!p.empty())
            result.push_back(_formmut(p, terse));

    if (you.form == transformation::dragon)
    {
        if (!species::is_draconian(you.species)
            || you.species == SP_BASE_DRACONIAN) // ugh
        {
            result.push_back(terse
                ? "breathe fire" : _formmut("You can breathe fire."));
        }
        else if (!terse
            && species::draconian_breath(you.species) != ABIL_NON_ABILITY)
        {
            result.push_back(
                _formmut("Your breath weapon is enhanced in this form."));
        }
    }

    // form-based flying can't be stopped, so don't print amphibiousness
    if (form->player_can_fly())
        result.push_back(terse ? "flying" : _formmut("You are flying."));
    // n.b. this could cause issues for non-dragon giant forms if they exist
    else if (form->player_can_swim() && !species::can_swim(you.species))
        result.push_back(terse ? "amphibious" : _formmut("You are amphibious."));

    const int hp_mod = form->mult_hp(10);
    if (hp_mod > 10)
    {
        result.push_back(terse ? "boosted hp"
            : _formmut(make_stringf("Your maximum health is %sincreased.",
                hp_mod < 13 ? "" : "greatly ")));
    } // see badmuts section below for max health reduction

    // Form resistances
    // We don't want to display the resistance on the A screen if the player
    // already has a mutation/innate property that supersedes this resistance.
    // Wisp form has a general text which displays all resistances except
    // poison immunity.

    if (player_res_poison(false, true, false, true) == 3
        && player_res_poison(false, false, false, false) != 3)
    {
        result.push_back(terse ? "poison immunity"
                               : _formmut("You are immune to poison."));
    }
    else if (form->res_pois() > 0 && !you.has_mutation(MUT_POISON_RESISTANCE))
    {
        result.push_back(terse ? "poison resistance"
                           : _formmut("You are resistant to poisons. (rPois)"));
    }

    if (you.get_mutation_level(MUT_NEGATIVE_ENERGY_RESISTANCE) != 3
        && you.form != transformation::wisp)
    {
        if (form->res_neg() == 3)
        {
            // Gargoyles have innate rN+ so let's not have
            // "negative energy resistance" appear twice in their terse lists.
            result.push_back(terse ? "negative energy immunity"
                      : _formmut("You are immune to negative energy. (rN+++)"));
        }
        else if (form->res_neg() == 1) // statue form
        {
            result.push_back(terse ? "statue negative energy resistance"
                  : _formmut("Your stone body resists negative energy. (rN+)"));
        }
    }

    if (form->res_elec()
        && !you.has_mutation(MUT_SHOCK_RESISTANCE)
        && you.form != transformation::wisp)
    {
        result.push_back(terse ? "electricity resistance"
                   : _formmut("You are resistant to electric shocks. (rElec)"));
    }

    if (you.form == transformation::dragon)
    {
        // Dragon form (except for some draconians) suppresses other resistance
        // mutations so we don't worry about overlapping other mutations.
        switch (species::dragon_form(you.species))
        {
            case MONS_FIRE_DRAGON:
                result.push_back(terse ? "fire resistance 2"
                             : _formmut("You are very heat resistant. (rF++)"));
                result.push_back(terse ? "cold vulnerability 1"
                                : _badmut("You are vulnerable to cold. (rC-)"));
                break;
            case MONS_ICE_DRAGON:
                result.push_back(terse ? "cold resistance 2"
                          : _formmut("You are very resistant to cold. (rC++)"));
                result.push_back(terse ? "heat vulnerability 1"
                                : _badmut("You are vulnerable to heat. (rF-)"));
                break;
            default:
                // all other draconian colours keep their innate resistance
                // and simply add rPois (dealt with further up)
                break;
        }
    }
    else if (you.form != transformation::wisp
             && form->res_cold() // death form only
             && you.get_mutation_level(MUT_COLD_RESISTANCE) != 3)
    {
        result.push_back(terse ? "undead cold resistance"
                    : _formmut("Your undead body is resistant to cold. (rC+)"));
    }

    // Bad effects
    // Try to put all `_badmut`s together.

    if (hp_mod < 10)
    {
        result.push_back(terse ? "reduced hp"
            : _badmut(make_stringf("Your maximum health is decreased%s.",
                form->underskilled() ? ", since you lack skill for your form"
                    : "")));
    }

    for (const auto &p : form->get_bad_fakemuts(terse))
        if (!p.empty())
            result.push_back(_badmut(p, terse));

    // Note: serpent form suppresses any innate cold-bloodedness
    if (you.form == transformation::serpent)
    {
        // XXX Hacky suppression with rC+
        if (you.res_cold())
        {
            result.push_back(terse ? "(cold-blooded)"
                : "<darkgray>((You are cold-blooded and may be slowed by cold attacks.))</darkgray>");
        }
        else
        {
            result.push_back(terse ? "cold-blooded"
                : _badmut("You are cold-blooded and may be slowed by cold attacks."));
        }
    }

    if (you.form == transformation::blade_hands
        && you_can_wear(EQ_BODY_ARMOUR, false) != false)
    {
        const int penalty_percent = form->get_base_ac_penalty(100);
        if (penalty_percent)
        {
            result.push_back(terse ? "blade armour"
                : _badmut(make_stringf("Your body armour is %s at protecting you.",
                          penalty_percent == 100 ? "completely ineffective"
                        : penalty_percent >=  70 ? "much less effective"
                        : penalty_percent >=  30 ? "less effective"
                                                 : "slightly less effective"
            )));
        }
    }

    if (!terse && !form->can_wield() && !you.has_mutation(MUT_NO_GRASPING))
    {
        // same as MUT_NO_GRASPING
        result.push_back(_badmut(
            "You are incapable of wielding weapons or throwing items."));
    }

    // XX say something about AC? Best would be to compare it to AC without
    // the form, but I'm not sure if that's possible
    // AC is currently dealt with via the `A!` "form properties" screen.

    // XX better synchronizing with various base armour/eq possibilities
    if (!terse && !you.has_mutation(MUT_NO_ARMOUR))
    {
        const string melding_desc = form->melding_description();
        if (!melding_desc.empty())
            result.push_back(_badmut(melding_desc));
    }

    return result;
}

static vector<string> _get_fakemuts(bool terse)
{
    vector<string> result;

    // XX sort good and bad non-permanent mutations better? Comes up mostly for
    // vampires

    // non-permanent and form-based stuff

    if (you.form != transformation::none)
    {
        vector<string> form_fakemuts = _get_form_fakemuts(terse);
        result.insert(result.end(), form_fakemuts.begin(), form_fakemuts.end());
    }

    // divine effects

    if (you.can_water_walk())
    {
        if (terse)
            result.push_back("walk on water");
        else
        {
            if (have_passive(passive_t::water_walk))
                result.push_back(_formmut("You can walk on water."));
            else
                result.push_back(_formmut("You can walk on water until reaching land."));
        }
    }

    if (you.props.exists(ORCIFICATION_LEVEL_KEY))
    {
        if (!terse)
        {
            if (you.props[ORCIFICATION_LEVEL_KEY].get_int() == 1)
                result.push_back(_formmut("Your facial features look somewhat orcish."));
            else
                result.push_back(_formmut("Your facial features are unmistakably orcish."));
        }
    }

    if (have_passive(passive_t::frail)
        || player_under_penance(GOD_HEPLIAKLQANA))
    {
        if (terse)
            result.push_back("reduced essence");
        else
        {
            // XX message is probably wrong for penance?
            result.push_back(_badmut(
                "Your life essence is reduced to manifest your ancestor. (-10% HP)"));
        }
    }

    // Innate abilities which haven't been implemented as mutations yet.
    for (const string& str : species::fake_mutations(you.species, terse))
    {
        if (species::is_draconian(you.species))
            result.push_back(_dragon_abil(str, terse));
        else
            result.push_back(_innatemut(str, terse));
    }

    if (you.racial_ac(false) > 0)
    {
        const int ac = you.racial_ac(false) / 100;
        if (terse)
            result.push_back("AC +" + to_string(ac));
        else
        {
            // XX generalize this code somehow?
            const string scale_clause = string(species::scale_type(you.species))
                  + " scales are hard";

            result.push_back(_annotate_form_based(
                        make_stringf("Your %s. (AC +%d)", you.species == SP_NAGA
                                            ? "serpentine skin is tough"
                                            : you.species == SP_GARGOYLE
                                            ? "stone body is resilient"
                                            : scale_clause.c_str(),
                           ac),
                        player_is_shapechanged()
                        && !(species::is_draconian(you.species)
                             && you.form == transformation::dragon)));
        }
    }

    // player::can_swim includes other cases, e.g. extra-balanced species that
    // are not truly amphibious. Mertail has its own description that implies
    // amphibiousness.
    if (species::can_swim(you.species) && !you.has_innate_mutation(MUT_MERTAIL))
    {
        result.push_back(_annotate_form_based(
                    terse ? "amphibious" : "You are amphibious.",
                    !form_can_swim(), terse));
    }

    if (species::arm_count(you.species) > 2)
    {
        const bool rings_melded = !get_form()->slot_available(EQ_RING_EIGHT);
        const int arms = you.arm_count();
        if (terse)
        {
            result.push_back(_annotate_form_based(
                make_stringf("%d rings", arms), rings_melded, true));
        }
        else
        {
            result.push_back(_annotate_form_based(
                make_stringf("You can wear up to %s rings at the same time.",
                         number_in_words(arms).c_str()), rings_melded));
        }
    }

    // in the terse list, this adj + a minimal size-derived desc covers the
    // same ground as the detailed size-derived desc; so no need for the size
    // itself in the long form.
    if (terse)
    {
        const char* size_adjective = get_size_adj(you.body_size(PSIZE_BODY), true);
        if (size_adjective)
            result.emplace_back(size_adjective);
    }

    // XX is there a cleaner approach?
    string armour_mut;
    string weapon_mut;

    switch (you.body_size(PSIZE_TORSO, true))
    {
    case SIZE_LITTLE:
        armour_mut = terse ? "unfitting armour"
            : "You are too small for most types of armour.";
        weapon_mut = terse ? "no large weapons"
            : "You are very small and have problems with some larger weapons.";
        break;
    case SIZE_SMALL:
        weapon_mut = terse ? "no large weapons"
            : "You are small and have problems with some larger weapons.";
        break;
    case SIZE_LARGE:
        armour_mut = terse ? "unfitting armour"
            : "You are too large for most types of armour.";
        break;
    default: // no giant species
        break;
    }
    // Could move this into species-data, but then the hack that assumes
    // _dragon_abil should get called on all draconian fake muts would break.
    if (species::is_draconian(you.species))
    {
        armour_mut = terse ? "unfitting armour"
            : "You cannot fit into any form of body armour.";
    }
    if (!weapon_mut.empty() && !you.has_mutation(MUT_NO_GRASPING))
        result.push_back(_innatemut(weapon_mut, terse));
    if (!armour_mut.empty() && !you.has_mutation(MUT_NO_ARMOUR))
        result.push_back(_innatemut(armour_mut, terse));

    if (you.has_mutation(MUT_VAMPIRISM))
    {
        if (you.vampire_alive)
        {
            result.push_back(terse ? "alive" :
                _formmut("Your natural rate of healing is slightly increased."));
        }
        else if (terse)
            result.push_back("bloodless");
        else
        {
            result.push_back(
                _formmut("You do not regenerate when monsters are visible."));
            result.push_back(
                _formmut("You are frail without blood (-20% HP)."));
            result.push_back(
                _formmut("You can heal yourself when you bite living creatures."));
            // XX automatically color this green somehow? Handled below more
            // generally for non-vampires
            result.push_back(_formmut("You are immune to poison."));
        }
    }
    else if (!terse && player_res_poison(false, false, false, false) == 3)
        result.push_back(_innatemut("You are immune to poison."));

    return result;
}

static vector<mutation_type> _get_ordered_mutations()
{
    vector<mutation_type> muts;

    // First add (non-removable) inborn abilities and demon powers.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        if (you.has_innate_mutation(mut))
            muts.push_back(mut);
    }

    // Now add removable mutations.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        if (you.get_base_mutation_level(mut, false, false, true) > 0
            && !you.has_innate_mutation(mut)
            && !you.has_temporary_mutation(mut))
        {
            muts.push_back(mut);
        }
    }

    //Finally, temporary mutations.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        if (you.has_temporary_mutation(mut))
            muts.push_back(mut);
    }

    return muts;
}


static vector<string> _get_mutations_descs(bool terse)
{
    vector<string> result = _get_fakemuts(terse);
    for (mutation_type mut : _get_ordered_mutations())
    {
        result.push_back(terse ? _terse_mut_name(mut)
                               : mutation_desc(mut, -1, true,
                                               you.sacrifices[mut] != 0));
    }

    return result;
}

string terse_mutation_list()
{
    const vector<string> mutations = _get_mutations_descs(true);

    if (mutations.empty())
        return "no striking features";
    else
    {
        return comma_separated_line(mutations.begin(), mutations.end(),
                                     ", ", ", ");
    }
}

string describe_mutations(bool drop_title)
{
#ifdef DEBUG
    validate_mutations(true);
#endif
    string result;

    if (!drop_title)
    {
        result += "<white>";
        result += "Innate Abilities, Weirdness & Mutations";
        result += "</white>\n\n";
    }

    const vector<string> mutations = _get_mutations_descs(false);

    if (mutations.empty())
        result += "You are rather mundane.\n";
    else
        result += join_strings(mutations.begin(), mutations.end(), "\n");

    return result;
}

static bool _has_partially_suppressed_muts()
{
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        if (!you.get_base_mutation_level(mut))
            continue;
        if (mutation_activity_level(mut) == mutation_activity_type::PARTIAL)
            return true;
    }
    return false;
}

static bool _has_fully_suppressed_muts()
{
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        if (!you.get_base_mutation_level(mut))
            continue;
        if (mutation_activity_level(mut) == mutation_activity_type::INACTIVE)
            return true;
    }
    return false;
}

static bool _has_transient_muts()
{
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        if (you.has_temporary_mutation(static_cast<mutation_type>(i)))
            return true;
    return false;
}

enum mut_menu_mode {
    MENU_MUTS,
    MENU_VAMP,
    MENU_FORM,
    NUM_MENU_MODES
};

const char *menu_mode_labels[] = {
    "Mutations",
    "Blood properties",
    "Form properties"
};
COMPILE_CHECK(ARRAYSZ(menu_mode_labels) == NUM_MENU_MODES);

class MutationMenu : public Menu
{
private:
    vector<string> fakemuts;
    vector<mutation_type> muts;
    mut_menu_mode mode;
    bool has_future_muts;
public:
    MutationMenu()
        : Menu(MF_SINGLESELECT | MF_ANYPRINTABLE | MF_ALLOW_FORMATTING
            | MF_ARROWS_SELECT),
          fakemuts(_get_fakemuts(false)),
          muts( _get_ordered_mutations()),
          mode(MENU_MUTS)
    {
        set_highlighter(nullptr);
        set_title(new MenuEntry("Innate Abilities, Weirdness & Mutations",
                                MEL_TITLE));
        menu_action = ACT_EXAMINE;
        update_entries();
        update_more();
    }

private:
    void update_entries()
    {
        clear();
        switch (mode)
        {
        case MENU_MUTS:
            update_muts();
            break;
        case MENU_VAMP:
            update_blood();
            break;
        case MENU_FORM:
            update_form();
            break;
        default:
            break;
        }
    }

    void update_form()
    {
        if (you.active_talisman.defined())
        {
            const string tal_name = you.active_talisman.name(DESC_PLAIN, false, false, false);
            const string head = make_stringf("<w>%s</w>:",
                                             uppercase_first(tal_name).c_str());
            add_entry(new MenuEntry(head, MEL_ITEM, 1, 0));

            if (is_artefact(you.active_talisman))
            {
                vector<string> artps;
                desc_randart_props(you.active_talisman, artps);
                for (string desc : artps)
                    add_entry(new MenuEntry(desc, MEL_ITEM, 1, 0));
            }
        }


        talisman_form_desc tfd;
        describe_talisman_form(you.default_form, tfd, true);
        add_entry(new MenuEntry("", MEL_ITEM, 1, 0)); // XXX spacing kludge?
        if (you.active_talisman.defined() && is_artefact(you.active_talisman))
        {
            add_entry(new MenuEntry("<w>Skill:</w>", MEL_ITEM, 1, 0));
            add_entry(new MenuEntry("", MEL_ITEM, 1, 0)); // XXX spacing kludge?
        }
        for (auto skinfo : tfd.skills)
        {
            const string label = make_stringf("%s: %s\n", skinfo.first.c_str(), skinfo.second.c_str());
            add_entry(new MenuEntry(label, MEL_ITEM, 1, 0));
        }
        if (!tfd.defenses.empty())
        {
            add_entry(new MenuEntry("<w>Defence:</w>", MEL_ITEM, 1, 0));
            add_entry(new MenuEntry("", MEL_ITEM, 1, 0)); // XXX  spacing kludge?
        }
        for (auto skinfo : tfd.defenses)
        {
            const string label = make_stringf("%s: %s\n", skinfo.first.c_str(), skinfo.second.c_str());
            add_entry(new MenuEntry(label, MEL_ITEM, 1, 0));
        }
        if (!tfd.offenses.empty())
        {
            add_entry(new MenuEntry("<w>Offence:</w>", MEL_ITEM, 1, 0));
            add_entry(new MenuEntry("", MEL_ITEM, 1, 0)); // XXX  spacing kludge?
        }
        for (auto skinfo : tfd.offenses)
        {
            const string label = make_stringf("%s: %s\n", skinfo.first.c_str(), skinfo.second.c_str());
            add_entry(new MenuEntry(label, MEL_ITEM, 1, 0));
        }
    }

    void update_blood()
    {
        ASSERT(you.has_mutation(MUT_VAMPIRISM));

        string result;

        const int lines = 17;
        string columns[lines][3] =
        {
            {"                     ", "<green>Alive</green>      ", "<lightred>Bloodless</lightred>"},
                                     //Full       Bloodless
            {"Regeneration         ", "fast       ", "none with monsters in sight"},

            {"HP modifier          ", "none       ", "-20%"},

            {"Stealth boost        ", "none       ", "major "},

            {"Heal on bite         ", "no         ", "yes "},

            {"", "", ""},
            {"<w>Resistances</w>", "", ""},
            {"Poison resistance    ", "           ", "immune"},

            {"Cold resistance      ", "           ", "++    "},

            {"Negative resistance  ", "           ", "+++   "},

            {"Miasma resistance    ", "           ", "immune"},

            {"Torment resistance   ", "           ", "immune"},

            {"", "", ""},
            {"<w>Transformations</w>", "", ""},
            {"Bat form (XL 3+)     ", "no         ", "yes   "},

            {"Other forms          ", "yes        ", "no    "},

            {"Berserk              ", "yes        ", "no    "}
        };

        const int highlight_col = you.vampire_alive ? 1 : 2;

        for (int y = 0; y < lines; y++)  // lines   (properties)
        {
            string label = "";
            for (int x = 0; x < 3; x++)  // columns (states)
            {
                string col = columns[y][x];
                if (x == highlight_col)
                    col = make_stringf("<w>%s</w>", col.c_str());
                label += col;
            }
            add_entry(new MenuEntry(label, MEL_ITEM, 1, 0));
        }
    }

    void update_muts()
    {
        for (const auto &fakemut : fakemuts)
        {
            MenuEntry* me = new MenuEntry(fakemut, MEL_ITEM, 1, 0);
            me->indent_no_hotkeys = !muts.empty();
            add_entry(me);
        }

        menu_letter hotkey;
        for (mutation_type &mut : muts)
        {
            const string desc = mutation_desc(mut, -1, true,
                                              you.sacrifices[mut] != 0);
            MenuEntry* me = new MenuEntry(desc, MEL_ITEM, 1, hotkey);
            ++hotkey;
            me->data = &mut;
#ifdef USE_TILE_WEB
            // This is a horrible hack. There's a bug where webtiles will
            // carry over mutation icons from the main mutation menu to the
            // vampirism menu. Rather than fix it, I've turned it off here.
            // I'm very sorry.
            if (!you.has_mutation(MUT_VAMPIRISM))
#endif
#ifdef USE_TILE
            {
                const tileidx_t tile = get_mutation_tile(mut);
                if (tile != 0)
                    me->add_tile(tile_def(tile + you.get_mutation_level(mut, false) - 1));
            }
#endif
            add_entry(me);
        }

        const vector<level_up_mutation> &xl_muts = get_species_def(you.species).level_up_mutations;
        has_future_muts = false;
        if (!xl_muts.empty())
        {
            vector<pair<mutation_type, int>> gained_muts;
            for (auto& mut : get_species_def(you.species).level_up_mutations)
            {
                if (you.experience_level < mut.xp_level)
                {
                    // Tally how many levels of this mutation we will have by the
                    // time we gain this instance of it (so that mutations
                    // scheduled to be gotten progressively will name each step
                    // correctly).
                    gained_muts.emplace_back(mut.mut, mut.mut_level);
                    int mut_lv = 0;
                    for (auto& gained_mut : gained_muts)
                        if (gained_mut.first == mut.mut)
                            mut_lv += gained_mut.second;

                    string mut_desc = _future_mutation_description(mut.mut, mut_lv);
#ifndef USE_TILE
                    chop_string(mut_desc, crawl_view.termsz.x - 15, false);
#endif

                    const string desc = make_stringf("<darkgrey>[%s]</darkgrey> XL %d",
                                                        mut_desc.c_str(),
                                                        mut.xp_level);
                    MenuEntry* me = new MenuEntry(desc, MEL_ITEM, 1, hotkey);
                    ++hotkey;
                    // XXX: Ugh...
                    me->data = (void*)&mut.mut;
                    add_entry(me);

                    has_future_muts = true;
                }
            }
        }

        if (items.empty())
        {
            add_entry(new MenuEntry("You are rather mundane.",
                                    MEL_ITEM, 1, 0));
        }
    }

    void update_more()
    {
        string extra = "";
        if (mode == MENU_MUTS)
        {
            if (_has_partially_suppressed_muts())
                extra += "<brown>()</brown>  : Partially suppressed.\n";
            // TODO: also handle suppressed fakemuts
            if (_has_fully_suppressed_muts())
                extra += "<darkgrey>(())</darkgrey>: Completely suppressed.\n";
            if (_has_transient_muts())
                extra += "<magenta>[]</magenta>   : Transient mutations.\n";
            if (has_future_muts)
                extra += "<darkgrey>[]</darkgrey>: Gained at a future XL.\n";
        }
        extra += picker_footer();
        set_more(extra);
    }

    string picker_footer()
    {
        auto modes = valid_modes();
        if (modes.size() <= 1)
            return "";
        string t = "";
        for (auto m : modes)
        {
            if (t.size() > 0)
                t += "|";
            const char* label = menu_mode_labels[m];
            if (m == mode)
                t += make_stringf("<w>%s</w>", label);
            else
                t += string(label);
        }
        return make_stringf("[<w>!</w>/<w>^</w>"
#ifdef USE_TILE_LOCAL
                "|<w>Right-click</w>"
#endif
                "]: %s", t.c_str());
    }

    vector<mut_menu_mode> valid_modes()
    {
        vector<mut_menu_mode> modes = {MENU_MUTS};
        if (you.has_mutation(MUT_VAMPIRISM))
            modes.push_back(MENU_VAMP);
        if (you.default_form != transformation::none)
            modes.push_back(MENU_FORM);
        return modes;
    }

    virtual bool process_key(int keyin) override
    {
        switch (keyin)
        {
        case '!':
        case '^':
        case CK_MOUSE_CMD:
        {
            auto modes = valid_modes();
            if (modes.size() > 1)
            {
                cycle_mut_mode(modes);
                update_entries();
                update_more();
                update_menu(true);
            }
        }
            return true;
        default:
            return Menu::process_key(keyin);
        }
    }

    void cycle_mut_mode(const vector<mut_menu_mode> &modes)
    {
        for (int i = 0; i < (int)modes.size(); i++)
        {
            if (modes[i] == mode)
            {
                mode = modes[(i + 1) % modes.size()];
                return;
            }
        }
        ASSERT(false);
    }

    bool examine_index(int i) override
    {
        ASSERT(i >= 0 && i < static_cast<int>(items.size()));
        if (items[i]->data)
        {
            // XX don't use C casts
            const mutation_type mut = *((mutation_type*)(items[i]->data));
            describe_mutation(mut);
        }
        return true;
    }
};

void display_mutations()
{
#ifdef DEBUG
    validate_mutations(true);
#endif

    MutationMenu mut_menu;
    mut_menu.show();
}

static int _calc_mutation_amusement_value(mutation_type which_mutation)
{
    int amusement = 12 * (11 - _get_mutation_def(which_mutation).weight);

    if (is_good_mutation(which_mutation))
        amusement /= 2;
    else if (is_bad_mutation(which_mutation))
        amusement *= 2;
    // currently is only ever one of these, but maybe that'll change?

    return amusement;
}

static bool _accept_mutation(mutation_type mutat, bool temp, bool ignore_weight)
{
    if (!_is_valid_mutation(mutat))
        return false;

    if (physiology_mutation_conflict(mutat))
        return false;

    // Devolution gives out permanent badmuts, so we don't want to give it as
    // a temporary mut.
    if (temp && mutat == MUT_DEVOLUTION)
        return false;

    const mutation_def& mdef = _get_mutation_def(mutat);

    if (you.get_base_mutation_level(mutat) >= mdef.levels)
        return false;

    // don't let random good mutations cause stat 0. Note: various code paths,
    // including jiyva-specific muts, and innate muts, don't include this check!
    if (_mut_has_use(mdef, mutflag::good) && mutation_causes_stat_zero(mutat))
        return false;

    if (ignore_weight)
        return true;

    if (mdef.weight == 0)
        return false;

    // bias towards adding (non-innate) levels to existing innate mutations.
    const int weight = mdef.weight + you.get_innate_mutation_level(mutat);

    // Low weight means unlikely to choose it.
    return x_chance_in_y(weight, 10);
}

static mutation_type _get_mut_with_use(mutflag mt)
{
    const int tweight = lookup(total_weight, mt, 0);
    ASSERT(tweight);

    int cweight = random2(tweight);
    for (const mutation_def &mutdef : mut_data)
    {
        if (!_mut_has_use(mutdef, mt))
            continue;

        cweight -= _mut_weight(mutdef, mt);
        if (cweight >= 0)
            continue;

        return mutdef.mutation;
    }

    die("Error while selecting mutations");
}

static mutation_type _get_random_slime_mutation()
{
    return _get_mut_with_use(mutflag::jiyva);
}

bool is_slime_mutation(mutation_type mut)
{
    return _mut_has_use(mut_data[mut_index[mut]], mutflag::jiyva);
}

bool is_makhleb_mark(mutation_type mut)
{
    return _mut_has_use(mut_data[mut_index[mut]], mutflag::makhleb);
}

static mutation_type _get_random_qazlal_mutation()
{
    return _get_mut_with_use(mutflag::qazlal);
}

static mutation_type _get_random_mutation(mutation_type mutclass,
                                          mutation_permanence_class perm)
{
    mutflag mt;
    switch (mutclass)
    {
        case RANDOM_MUTATION:
            // maintain an arbitrary ratio of good to bad muts to allow easier
            // weight changes within categories - 60% good seems to be about
            // where things are right now
            mt = x_chance_in_y(3, 5) ? mutflag::good : mutflag::bad;
            break;
        case RANDOM_XOM_MUTATION:
            // similar to random mutation, but slightly less likely to be good!
            mt = coinflip() ? mutflag::good : mutflag::bad;
            break;
        case RANDOM_BAD_MUTATION:
        case RANDOM_CORRUPT_MUTATION:
            mt = mutflag::bad;
            break;
        case RANDOM_GOOD_MUTATION:
            mt = mutflag::good;
            break;
        default:
            die("invalid mutation class: %d", mutclass);
    }

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        mutation_type mut = _get_mut_with_use(mt);
        if (_accept_mutation(mut, perm == MUTCLASS_TEMPORARY, true))
            return mut;
    }

    return NUM_MUTATIONS;
}

/**
 * Does the player have a mutation that conflicts with the given mutation?
 *
 * @param mut           A mutation. (E.g. MUT_INHIBITED_REGENERATION, ...)
 * @param innate_only   Whether to only check innate mutations (from e.g. race)
 * @return              The level of the conflicting mutation.
 *                      E.g., if MUT_INHIBITED_REGENERATION is passed in and the
 *                      player has 2 levels of MUT_REGENERATION, 2 will be
 *                      returned.
 *
 *                      No guarantee is offered on ordering if there are
 *                      multiple conflicting mutations with different levels.
 */
int mut_check_conflict(mutation_type mut, bool innate_only)
{
    for (const int (&confl)[3] : conflict)
    {
        if (confl[0] != mut && confl[1] != mut)
            continue;

        const mutation_type confl_mut
           = static_cast<mutation_type>(confl[0] == mut ? confl[1] : confl[0]);

        const int level = you.get_base_mutation_level(confl_mut, true, !innate_only, !innate_only);
        if (level)
            return level;
    }

    return 0;
}

/// Does the given mut at the given level block use of the given item? If so, why?
static string _mut_blocks_item_reason(const item_def &item, mutation_type mut, int level)
{
    if (level <= 0) return "";

    if (is_unrandom_artefact(item, UNRAND_LEAR))
    {
        switch (mut)
        {
        case MUT_CLAWS:
        case MUT_DEMONIC_TOUCH:
            if (level < 3)
                return "";
            // XXX: instead say demonic touch would destroy the hauberk?
            return make_stringf("The hauberk won't fit your %s.",
                                you.hand_name(true).c_str());
        case MUT_HORNS:
        case MUT_ANTENNAE:
            if (level < 3)
                return "";
            return "The hauberk won't fit your head.";
        default:
            return "";
        }
    }
    switch (get_armour_slot(item))
    {
    case EQ_GLOVES:
        if (level < 3)
            break;
        if (mut == MUT_CLAWS)
        {
            return make_stringf("You can't wear gloves with your huge claw%s!",
                                you.arm_count() == 1 ? "" : "s");
        }
        if (mut == MUT_DEMONIC_TOUCH)
            return "Your demonic touch would destroy the gloves!";
        break;

    case EQ_BOOTS:
        if (mut == MUT_FLOAT)
            return "You have no feet!"; // or legs
        if (level < 3 || item.sub_type == ARM_BARDING)
            break;
        if (mut == MUT_HOOVES)
            return "You can't wear boots with hooves!";
        if (mut == MUT_TALONS)
            return "Boots don't fit your talons!";
        break;

    case EQ_HELMET:
        if (mut == MUT_HORNS && level >= 3)
            return "You can't wear any headgear with your large horns!";
        if (mut == MUT_ANTENNAE && level >= 3)
            return "You can't wear any headgear with your large antennae!";
        // Soft helmets (caps and wizard hats) always fit, otherwise.
        // Caps and wizard hats haven't existed for many years, but I find this
        // comment quaint and wish to preserve it. -- pf
        if (!is_hard_helmet(item))
            return "";
        if (mut == MUT_HORNS)
            return "You can't wear that with your horns!";
        if (mut == MUT_BEAK)
            return "You can't wear that with your beak!";
        if (mut == MUT_ANTENNAE)
            return "You can't wear that with your antennae!";
        break;

    case EQ_CLOAK:
        if (mut == MUT_WEAKNESS_STINGER && level == 3)
            return "You can't wear that with your sharp stinger!";
        break;

    default:
        break;
    }
    return "";
}

/**
 * Does the player have a mutation that blocks equipping the given item?
 *
 * @param temp Whether to consider your current form, probably.
 * @return A reason why the item can't be worn, or the empty string if it's fine.
 */
string mut_blocks_item_reason(const item_def &item, bool temp)
{
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        const auto mut = (mutation_type)i;
        const int level = you.get_mutation_level(mut, temp);
        const string reason = _mut_blocks_item_reason(item, mut, level);
        if (!reason.empty())
            return reason;
    }
    return "";
}

static void _maybe_remove_armour(mutation_type mut, int level)
{
    for (int i = EQ_MIN_ARMOUR; i <= EQ_BODY_ARMOUR; ++i)
    {
        if (you.melded[i])
            continue;
        const int slot = you.equip[i];
        if (slot == -1)
            continue;
        if (_mut_blocks_item_reason(you.inv[slot], mut, level).empty())
            continue;
        remove_one_equip((equipment_type)i, false, true);
    }
}

// Tries to give you the mutation by deleting a conflicting
// one, or clears out conflicting mutations if we should give
// you the mutation anyway.
// Return:
//  1 if we should stop processing (success);
//  0 if we should continue processing;
// -1 if we should stop processing (failure).
static int _handle_conflicting_mutations(mutation_type mutation,
                                         bool override,
                                         const string &reason,
                                         bool temp = false)
{
    // If we have one of the pair, delete all levels of the other,
    // and continue processing.
    for (const int (&confl)[3] : conflict)
    {
        for (int j = 0; j <= 1; ++j)
        {
            const mutation_type a = (mutation_type)confl[j];
            const mutation_type b = (mutation_type)confl[1-j];

            if (mutation == a && you.get_base_mutation_level(b) > 0)
            {
                // can never delete innate mutations. For case -1 and 0, fail if there are any, otherwise,
                // make sure there is a non-innate instance to delete.
                if (you.has_innate_mutation(b) &&
                    (confl[2] != 1
                     || you.get_base_mutation_level(b, true, false, false) == you.get_base_mutation_level(b)))
                {
                    dprf("Delete mutation failed: have innate mutation %d at level %d, you.mutation at level %d", b,
                        you.get_innate_mutation_level(b), you.get_base_mutation_level(b));
                    return -1;
                }

                // at least one level of this mutation is temporary
                const bool temp_b = you.has_temporary_mutation(b);

                // confl[2] indicates how the mutation resolution should proceed (see `conflict` a the beginning of this file):
                switch (confl[2])
                {
                case -1:
                    // Fail if not forced, otherwise override.
                    if (!override)
                        return -1;
                case 0:
                    // Ignore if not forced, otherwise override.
                    // All cases but regen:slowmeta will currently trade off.
                    if (override)
                    {
                        while (_delete_single_mutation_level(b, reason, true))
                            ;
                    }
                    break;
                case 1:
                    // If we have one of the pair, delete a level of the
                    // other, and that's it.
                    //
                    // Temporary mutations can co-exist with things they would
                    // ordinarily conflict with. But if both a and b are temporary,
                    // mark b for deletion.
                    if ((temp || temp_b) && !(temp && temp_b))
                        return 0;       // Allow conflicting transient mutations
                    else
                    {
                        _delete_single_mutation_level(b, reason, true);
                        return 1;     // Nothing more to do.
                    }

                default:
                    die("bad mutation conflict resolution");
                }
            }
        }
    }

    return 0;
}

static equipment_type _eq_type_for_mut(mutation_type mutat)
{
    if (!is_body_facet(mutat))
        return EQ_NONE;
    for (const body_facet_def &facet : _body_facets)
        if (mutat == facet.mut)
            return facet.eq;
    return EQ_NONE;
}

// Make Ashenzari suppress mutations that would shatter your cursed item.
static bool _ashenzari_blocks(mutation_type mutat)
{
    if (GOD_ASHENZARI != you.religion)
        return false;

    const equipment_type eq_type = _eq_type_for_mut(mutat);
    if (eq_type == EQ_NONE || you.equip[eq_type] == -1)
        return false;

    const item_def &it = you.inv[you.equip[eq_type]];
    if (!it.cursed())
        return false;

    if (_mut_blocks_item_reason(it, mutat, you.get_mutation_level(mutat) + 1).empty())
        return false;

    const string msg = make_stringf(" prevents a mutation which would have shattered %s.",
                                    it.name(DESC_YOUR).c_str());
    simple_god_message(msg.c_str());
    return true;
}

/// Do you have an existing mutation in the same body slot? (E.g., gloves, helmet...)
static bool _body_facet_blocks(mutation_type mutat)
{
    const equipment_type eq_type = _eq_type_for_mut(mutat);
    if (eq_type == EQ_NONE)
        return false;

    for (const body_facet_def &facet : _body_facets)
    {
        if (eq_type == facet.eq
            && mutat != facet.mut
            && you.get_base_mutation_level(facet.mut))
        {
            return true;
        }
    }
    return false;
}

static bool _exoskeleton_incompatible(mutation_type mutat)
{
    // Coglins attack with and wear aux armour on their exoskeleton-limbs,
    // not their fleshy, mutation-prone hands. Disable mutations that would
    // make no sense in this scheme.
    switch (mutat)
    {
    case MUT_HOOVES:
    case MUT_CLAWS:
    case MUT_TALONS:
        return true;
    default:
        return false;
    }
}

bool physiology_mutation_conflict(mutation_type mutat)
{
    if (mutat == MUT_IRIDESCENT_SCALES)
    {
        // No extra scales for most demonspawn, but monstrous demonspawn who
        // wouldn't usually get scales can get regular ones randomly.
        if (you.species == SP_DEMONSPAWN)
        {
            return any_of(begin(you.demonic_traits), end(you.demonic_traits),
                          [=](const player::demon_trait &t) {
                              return _is_demonspawn_scale(t.mutation);});
        }

        // No extra scales for draconians.
        if (species::is_draconian(you.species))
            return true;
    }

    // Only species that already have tails can get this one. A felid
    // tail does nothing in combat, so ignore it. For merfolk it would
    // only work in the water, so skip it. Demonspawn tails come with a
    // stinger already.
    if ((!you.has_tail(false)
         || you.species == SP_FELID
         || you.has_innate_mutation(MUT_MERTAIL)
         || you.has_mutation(MUT_WEAKNESS_STINGER))
        && mutat == MUT_STINGER)
    {
        return true;
    }

    // Need tentacles to grow something on them.
    if (!you.has_innate_mutation(MUT_TENTACLE_ARMS)
        && mutat == MUT_TENTACLE_SPIKE)
    {
        return true;
    }

    // No bones for thin skeletal structure or horns.
    if (!you.has_bones(false)
        && (mutat == MUT_THIN_SKELETAL_STRUCTURE || mutat == MUT_HORNS))
    {
        return true;
    }

    // No feet.
    if (!player_has_feet(false, false)
        && (mutat == MUT_HOOVES || mutat == MUT_TALONS))
    {
        return true;
    }

    // To get upgraded spit poison, you must have it innately
    if (!you.has_innate_mutation(MUT_SPIT_POISON) && mutat == MUT_SPIT_POISON)
        return true;

    // Only Draconians (and gargoyles) can get wings.
    if (!species::is_draconian(you.species) && you.species != SP_GARGOYLE
        && mutat == MUT_BIG_WINGS)
    {
        return true;
    }

    // Vampires' healing rates depend on their blood level.
    if (you.has_mutation(MUT_VAMPIRISM)
        && (mutat == MUT_REGENERATION || mutat == MUT_INHIBITED_REGENERATION))
    {
        return true;
    }

    // Today's guru wisdom: octopodes have no hands.
    if (you.has_innate_mutation(MUT_TENTACLE_ARMS) && mutat == MUT_CLAWS)
        return true;

    // Merfolk have no feet in the natural form, and we never allow mutations
    // that show up only in a certain transformation.
    if (you.has_innate_mutation(MUT_MERTAIL)
        && (mutat == MUT_TALONS || mutat == MUT_HOOVES))
    {
        return true;
    }

    // Formicids have stasis and so prevent mutations that would do nothing.
    if (you.stasis() && (mutat == MUT_BERSERK || mutat == MUT_TELEPORT))
        return true;

    if (you.innate_sinv() && mutat == MUT_ACUTE_VISION)
        return true;

    // Already immune.
    if (you.is_nonliving(false, false) && mutat == MUT_POISON_RESISTANCE)
        return true;

    // We can't use is_useless_skill() here, since species that can still wear
    // body armour can sacrifice armour skill with Ru.
    if (species_apt(SK_ARMOUR) == UNUSABLE_SKILL
        && (mutat == MUT_DEFORMED || mutat == MUT_STURDY_FRAME))
    {
        return true;
    }

    // Mutations of the same slot conflict
    if (_body_facet_blocks(mutat))
        return true;

    if (you.species == SP_COGLIN && _exoskeleton_incompatible(mutat))
        return true;

    return false;
}

static const char* _stat_mut_desc(mutation_type mut, bool gain)
{
    stat_type stat = STAT_STR;
    bool positive = gain;
    switch (mut)
    {
    case MUT_WEAK:
        positive = !positive;
    case MUT_STRONG:
        stat = STAT_STR;
        break;

    case MUT_DOPEY:
        positive = !positive;
    case MUT_CLEVER:
        stat = STAT_INT;
        break;

    case MUT_CLUMSY:
        positive = !positive;
    case MUT_AGILE:
        stat = STAT_DEX;
        break;

    default:
        die("invalid stat mutation: %d", mut);
    }
    return stat_desc(stat, positive ? SD_INCREASE : SD_DECREASE);
}

/**
 * Do a resistance check for the given mutation permanence class.
 * Does not include divine intervention!
 *
 * @param mutclass The type of mutation that is checking resistance
 * @param beneficial Is the mutation beneficial?
 *
 * @return True if a mutation is successfully resisted, false otherwise.
**/
static bool _resist_mutation(mutation_permanence_class mutclass,
                             bool beneficial)
{
    if (you.get_mutation_level(MUT_MUTATION_RESISTANCE) == 3)
        return true;

    const int mut_resist_chance = mutclass == MUTCLASS_TEMPORARY ? 2 : 3;
    if (you.get_mutation_level(MUT_MUTATION_RESISTANCE)
        && !one_chance_in(mut_resist_chance))
    {
        return true;
    }

    // To be nice, beneficial mutations go through removable sources of rMut.
    if (you.rmut_from_item() && !beneficial
        && !one_chance_in(mut_resist_chance))
    {
        return true;
    }

    return false;
}

/*
 * Try to mutate the player, along with associated bookkeeping. This accepts mutation categories as well as particular mutations.
 *
 * In many cases this will produce only 1 level of mutation at a time, but it may mutate more than one level if the mutation category is corrupt or qazlal.
 *
 * If the player is at the mutation cap, this may fail.
 *   1. If mutclass is innate, this will attempt to replace temporary and normal mutations (in that order) and will fail if this isn't possible (e.g. there are only innate levels).
 *   2. Otherwise, this will fail. This means that a temporary mutation can block a permanent mutation of the same type in some circumstances.
 *
 * If the mutation conflicts with an existing one it may fail. See `_handle_conflicting_mutations`.
 *
 * If the player is undead, this may stat drain instead. Stat draincounts as
 * success.
 *
 * @param which_mutation    the mutation to use.
 * @param reason            the explanation for how the player got mutated.
 * @param failMsg           whether to do any messaging if this fails.
 * @param force_mutation    whether to override mutation protection and the like.
 * @param god_gift          is this a god gift? Entails overriding mutation resistance if not forced.
 * @param mutclass          is the mutation temporary, regular, or permanent (innate)? permanent entails force_mutation.
 *
 * @return whether the mutation succeeded.
 */
bool mutate(mutation_type which_mutation, const string &reason, bool failMsg,
            bool force_mutation, bool god_gift, bool beneficial,
            mutation_permanence_class mutclass)
{
    if (which_mutation == RANDOM_BAD_MUTATION
        && mutclass == MUTCLASS_NORMAL
        && crawl_state.disables[DIS_AFFLICTIONS])
    {
        return true; // no fallbacks
    }

    god_gift |= crawl_state.is_god_acting();

    if (mutclass == MUTCLASS_INNATE)
        force_mutation = true;

    mutation_type mutat = which_mutation;

    if (!force_mutation)
    {
        // God gifts override all sources of mutation resistance other
        // than divine protection.
        if (!god_gift && _resist_mutation(mutclass, beneficial))
        {
            if (failMsg)
                mprf(MSGCH_MUTATION, "You feel odd for a moment.");
            return false;
        }

        // Zin's protection.
        if (have_passive(passive_t::resist_mutation)
            && x_chance_in_y(you.piety, piety_breakpoint(5)))
        {
            simple_god_message(" protects your body from mutation!");
            return false;
        }
    }

    // Undead bodies don't mutate, they fall apart. -- bwr
    if (!you.can_safely_mutate())
    {
        switch (mutclass)
        {
        case MUTCLASS_TEMPORARY:
            if (coinflip())
                return false;
            // fallthrough to normal mut
        case MUTCLASS_NORMAL:
            mprf(MSGCH_MUTATION, "Your body decomposes!");
            lose_stat(STAT_RANDOM, 1);
            return true;
        case MUTCLASS_INNATE:
            // You can't miss out on innate mutations just because you're
            // temporarily undead.
            break;
        default:
            die("bad fall through");
            return false;
        }
    }

    if (mutclass == MUTCLASS_NORMAL
        && (which_mutation == RANDOM_MUTATION
            || which_mutation == RANDOM_XOM_MUTATION)
        && x_chance_in_y(you.how_mutated(false, true), 15))
    {
        // God gifts override mutation loss due to being heavily
        // mutated.
        if (!one_chance_in(3) && !god_gift && !force_mutation)
            return false;
        else
            return delete_mutation(RANDOM_MUTATION, reason, failMsg,
                                   force_mutation, false);
    }

    mutat = concretize_mut(which_mutation, mutclass);
    if (!_is_valid_mutation(mutat))
        return false;

    // [Cha] don't allow teleportitis in sprint
    if (mutat == MUT_TELEPORT && crawl_state.game_is_sprint())
        return false;

    if (physiology_mutation_conflict(mutat))
        return false;

    if (mutclass != MUTCLASS_INNATE && _ashenzari_blocks(mutat))
        return false;

    const mutation_def& mdef = _get_mutation_def(mutat);

    bool gain_msg = true;

    if (mutclass == MUTCLASS_INNATE)
    {
        // are there any non-innate instances to replace?  Prioritize temporary mutations over normal.
        // Temporarily decrement the mutation value so it can be silently regained in the while loop below.
        if (you.mutation[mutat] > you.innate_mutation[mutat])
        {
            if (you.temp_mutation[mutat] > 0)
            {
                you.temp_mutation[mutat]--;
                you.attribute[ATTR_TEMP_MUTATIONS]--;
                if (you.attribute[ATTR_TEMP_MUTATIONS] == 0)
                    you.attribute[ATTR_TEMP_MUT_XP] = 0;
            }
            you.mutation[mutat]--;
            mprf(MSGCH_MUTATION, "Your mutations feel more permanent.");
            take_note(Note(NOTE_PERM_MUTATION, mutat,
                    you.get_base_mutation_level(mutat), reason.c_str()));
            gain_msg = false;
        }
    }
    if (you.mutation[mutat] >= mdef.levels)
        return false;

    // God gifts and forced mutations clear away conflicting mutations.
    int rc = _handle_conflicting_mutations(mutat, god_gift || force_mutation,
                                           reason,
                                           mutclass == MUTCLASS_TEMPORARY);
    if (rc == 1)
        return true;
    if (rc == -1)
        return false;

    ASSERT(rc == 0);

#ifdef USE_TILE_LOCAL
    const unsigned int old_talents = your_talents(false).size();
#endif

    const int levels = (which_mutation == RANDOM_CORRUPT_MUTATION
                         || which_mutation == RANDOM_QAZLAL_MUTATION)
                       ? min(2, mdef.levels - you.get_base_mutation_level(mutat))
                       : 1;
    ASSERT(levels > 0); //TODO: is > too strong?

    int count = levels;

    while (count-- > 0)
    {
        // no fail condition past this point, so it is safe to do bookkeeping
        you.mutation[mutat]++;
        if (mutclass == MUTCLASS_TEMPORARY)
        {
            // do book-keeping for temporary mutations
            you.temp_mutation[mutat]++;
            you.attribute[ATTR_TEMP_MUTATIONS]++;
        }
        else if (mutclass == MUTCLASS_INNATE)
            you.innate_mutation[mutat]++;

        const int cur_base_level = you.get_base_mutation_level(mutat);

        // More than three messages, need to give them by hand.
        switch (mutat)
        {
        case MUT_STRONG: case MUT_AGILE:  case MUT_CLEVER:
        case MUT_WEAK:   case MUT_CLUMSY: case MUT_DOPEY:
            mprf(MSGCH_MUTATION, "You feel %s.", _stat_mut_desc(mutat, true));
            gain_msg = false;
            break;

        case MUT_LARGE_BONE_PLATES:
            {
                const string arms = pluralise(species::arm_name(you.species));
                mprf(MSGCH_MUTATION, "%s",
                     replace_all(mdef.gain[cur_base_level - 1], "arms",
                                 arms).c_str());
                gain_msg = false;
            }
            break;

        case MUT_MISSING_HAND:
            {
                // n.b. we cannot use the built in pluralisation, because at
                // this point the mut has already applied, and hand_name takes
                // it into account.
                const string hands = pluralise(you.hand_name(false));
                mprf(MSGCH_MUTATION, "%s",
                     replace_all(mdef.gain[cur_base_level - 1], "hands",
                                 hands).c_str());
                gain_msg = false;
            }
            break;

        case MUT_SPIT_POISON:
            // Breathe poison replaces spit poison (so it takes the slot).
            if (cur_base_level >= 2)
                for (int i = 0; i < 52; ++i)
                    if (you.ability_letter_table[i] == ABIL_SPIT_POISON)
                        you.ability_letter_table[i] = ABIL_BREATHE_POISON;
            break;

        default:
            break;
        }

        // For all those scale mutations.
        you.redraw_armour_class = true;

        notify_stat_change();

        if (gain_msg)
            mprf(MSGCH_MUTATION, "%s", mdef.gain[cur_base_level - 1]);

        // Do post-mutation effects.
        switch (mutat)
        {
        case MUT_FRAIL:
        case MUT_ROBUST:
        case MUT_RUGGED_BROWN_SCALES:
            calc_hp();
            break;

        case MUT_LOW_MAGIC:
        case MUT_HIGH_MAGIC:
            calc_mp();
            break;

        case MUT_PASSIVE_MAPPING:
            add_daction(DACT_REAUTOMAP);
            break;

        case MUT_ACUTE_VISION:
            // We might have to turn autopickup back on again.
            autotoggle_autopickup(false);
            break;

        case MUT_NIGHTSTALKER:
            update_vision_range();
            break;

        case MUT_BIG_WINGS:
#ifdef USE_TILE
            init_player_doll();
#endif
            break;

        case MUT_SILENCE_AURA:
        case MUT_FOUL_SHADOW:
            invalidate_agrid(true);
            break;

        case MUT_EVOLUTION:
        case MUT_DEVOLUTION:
            if (cur_base_level == 1)
            {
                you.props[EVOLUTION_MUTS_KEY] = 0;
                set_evolution_mut_xp(mutat == MUT_DEVOLUTION);
            }
            break;

        default:
            break;
        }

        _maybe_remove_armour(mutat, cur_base_level);
        ash_check_bondage();

        xom_is_stimulated(_calc_mutation_amusement_value(mutat));

        if (mutclass != MUTCLASS_TEMPORARY)
        {
            take_note(Note(NOTE_GET_MUTATION, mutat, cur_base_level,
                           reason.c_str()));
        }
        else
        {
            // only do this once regardless of how many levels got added
            you.attribute[ATTR_TEMP_MUT_XP] = temp_mutation_roll();
        }

        if (you.hp <= 0)
        {
            ouch(0, KILLED_BY_FRAILTY, MID_NOBODY,
                 make_stringf("gaining the %s mutation",
                              mutation_name(mutat)).c_str());
        }
    }

#ifdef USE_TILE_LOCAL
    if (your_talents(false).size() != old_talents)
    {
        tiles.layout_statcol();
        redraw_screen();
        update_screen();
    }
#endif
#ifdef DEBUG
    if (mutclass != MUTCLASS_INNATE) // taken care of in perma_mutate. Skipping this here avoids validation issues in doing repairs.
        validate_mutations(false);
#endif
    return true;
}

/**
 * If the given mutation is a 'random' type (e.g. RANDOM_GOOD_MUTATION),
 * turn it into a specific appropriate mut (e.g. MUT_HORNS). Otherwise, return
 * the given mut as-is.
 */
mutation_type concretize_mut(mutation_type mut,
                             mutation_permanence_class mutclass)
{
    switch (mut)
    {
    case RANDOM_MUTATION:
    case RANDOM_GOOD_MUTATION:
    case RANDOM_BAD_MUTATION:
    case RANDOM_CORRUPT_MUTATION:
    case RANDOM_XOM_MUTATION:
        return _get_random_mutation(mut, mutclass);
    case RANDOM_SLIME_MUTATION:
        return _get_random_slime_mutation();
    case RANDOM_QAZLAL_MUTATION:
        return _get_random_qazlal_mutation();
    default:
        return mut;
    }
}

/**
 * Delete a single mutation level of fixed type `mutat`.
 * If `transient` is set, allow deleting temporary mutations, and prioritize them.
 * Note that if `transient` is true and there are no temporary mutations, this can delete non-temp mutations.
 * If `transient` is false, and there are only temp mutations, this will fail; otherwise it will delete a non-temp mutation.
 *
 * @mutat     the mutation to delete
 * @reason    why is it being deleted
 * @transient whether to allow (and prioritize) deletion of temporary mutations
 *
 * @return whether a mutation was deleted.
 */
static bool _delete_single_mutation_level(mutation_type mutat,
                                          const string &reason,
                                          bool transient)
{
    // are there some non-innate mutations to delete?
    if (you.get_base_mutation_level(mutat, false, true, true) == 0)
        return false;

    bool was_transient = false;
    if (you.has_temporary_mutation(mutat))
    {
        if (transient)
            was_transient = true;
        else if (you.get_base_mutation_level(mutat, false, false, true) == 0) // there are only temporary mutations to delete
            return false;

        // fall through: there is a non-temporary mutation level that can be deleted.
    }

    const mutation_def& mdef = _get_mutation_def(mutat);

    bool lose_msg = true;

    you.mutation[mutat]--;

    switch (mutat)
    {
    case MUT_STRONG: case MUT_AGILE:  case MUT_CLEVER:
    case MUT_WEAK:   case MUT_CLUMSY: case MUT_DOPEY:
        mprf(MSGCH_MUTATION, "You feel %s.", _stat_mut_desc(mutat, false));
        lose_msg = false;
        break;

    case MUT_SPIT_POISON:
        // Breathe poison replaces spit poison (so it takes the slot).
        if (you.mutation[mutat] < 2)
            for (int i = 0; i < 52; ++i)
                if (you.ability_letter_table[i] == ABIL_SPIT_POISON)
                    you.ability_letter_table[i] = ABIL_BREATHE_POISON;
        break;

    case MUT_NIGHTSTALKER:
        update_vision_range();
        break;

    case MUT_BIG_WINGS:
        land_player();
        break;

    case MUT_HORNS:
    case MUT_ANTENNAE:
    case MUT_BEAK:
    case MUT_CLAWS:
    case MUT_HOOVES:
    case MUT_TALONS:
        // Recheck Ashenzari bondage in case our available slots changed.
        ash_check_bondage();
        break;

    case MUT_SILENCE_AURA:
    case MUT_FOUL_SHADOW:
        invalidate_agrid(true);
        break;

    case MUT_EVOLUTION:
    case MUT_DEVOLUTION:
        if (!you.mutation[mutat])
            you.props[EVOLUTION_MUTS_KEY] = 0;
        break;

    default:
        break;
    }

    // For all those scale mutations.
    you.redraw_armour_class = true;

    notify_stat_change();

    if (lose_msg)
        mprf(MSGCH_MUTATION, "%s", mdef.lose[you.mutation[mutat]]);

    // Do post-mutation effects.
    if (mutat == MUT_FRAIL || mutat == MUT_ROBUST
        || mutat == MUT_RUGGED_BROWN_SCALES)
    {
        calc_hp();
    }
    if (mutat == MUT_LOW_MAGIC || mutat == MUT_HIGH_MAGIC)
        calc_mp();

    if (was_transient)
    {
        --you.temp_mutation[mutat];
        --you.attribute[ATTR_TEMP_MUTATIONS];
    }
    else
        take_note(Note(NOTE_LOSE_MUTATION, mutat, you.mutation[mutat], reason));

    if (you.hp <= 0)
    {
        ouch(0, KILLED_BY_FRAILTY, MID_NOBODY,
             make_stringf("losing the %s mutation", mutation_name(mutat)).c_str());
    }

    return true;
}

/// Returns the mutflag corresponding to a given class of random mutations, or 0.
static mutflag _mutflag_for_random_type(mutation_type mut_type)
{
    switch (mut_type)
    {
    case RANDOM_GOOD_MUTATION:
        return mutflag::good;
    case RANDOM_BAD_MUTATION:
    case RANDOM_CORRUPT_MUTATION:
        return mutflag::bad;
    case RANDOM_SLIME_MUTATION:
        return mutflag::jiyva;
    case RANDOM_QAZLAL_MUTATION:
        return mutflag::qazlal;
    case RANDOM_MUTATION:
    case RANDOM_XOM_MUTATION:
    default:
        return (mutflag)0;
    }
}

/**
 * Given a type of 'random mutation' (eg RANDOM_XOM_MUTATION), return a mutation of that type that
 * we can delete from the player, or else NUM_MUTATIONS.
 */
static mutation_type _concretize_mut_deletion(mutation_type mut_type)
{
    switch (mut_type)
    {
        case RANDOM_MUTATION:
        case RANDOM_GOOD_MUTATION:
        case RANDOM_BAD_MUTATION:
        case RANDOM_CORRUPT_MUTATION:
        case RANDOM_XOM_MUTATION:
        case RANDOM_SLIME_MUTATION:
        case RANDOM_QAZLAL_MUTATION:
            break;
        default:
            return mut_type;
    }

    const mutflag mf = _mutflag_for_random_type(mut_type);
    int seen = 0;
    mutation_type chosen = NUM_MUTATIONS;
    for (const mutation_def &mutdef : mut_data)
    {
        if (mf != (mutflag)0 && !_mut_has_use(mutdef, mf))
            continue;
        // Check whether we have a non-innate, permanent level of this mut
        if (you.get_base_mutation_level(mutdef.mutation, false, false) == 0)
            continue;

        ++seen;
        if (one_chance_in(seen))
            chosen = mutdef.mutation;
    }
    return chosen;
}

/*
 * Delete a mutation level, accepting random mutation types and checking mutation resistance.
 * This will not delete temporary or innate mutations.
 *
 * @param which_mutation    a mutation, including random
 * @param reason            the reason for deletion
 * @param failMsg           whether to message the player on failure
 * @param force_mutation    whether to try to override mut resistance & undeadness
 * @param god_gift          is the mutation a god gift & thus ignores mut resistance?
 * @return true iff a mutation was deleted.
 */
bool delete_mutation(mutation_type which_mutation, const string &reason,
                     bool failMsg,
                     bool force_mutation, bool god_gift)
{
    god_gift |= crawl_state.is_god_acting();

    if (!force_mutation)
    {
        if (!god_gift)
        {
            if (you.get_mutation_level(MUT_MUTATION_RESISTANCE) > 1
                && (you.get_mutation_level(MUT_MUTATION_RESISTANCE) == 3
                    || coinflip()))
            {
                if (failMsg)
                    mprf(MSGCH_MUTATION, "You feel rather odd for a moment.");
                return false;
            }
        }

        if (!you.can_safely_mutate())
            return false;
    }

    const mutation_type mutat = _concretize_mut_deletion(which_mutation);
    if (mutat == NUM_MUTATIONS)
        return false;
    return _delete_single_mutation_level(mutat, reason, false); // won't delete temp mutations
}

/*
 * Delete all (non-innate) mutations.
 *
 * If you really need to delete innate mutations as well, have a look at `change_species_to` in species.cc.
 * Changing species to human, for example, is a safe way to clear innate mutations entirely. For a
 * demonspawn, you could also use wizmode code to set the level to 1.
 *
 * @return  Whether the function found mutations to delete.
 */
bool delete_all_mutations(const string &reason)
{
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        while (_delete_single_mutation_level(static_cast<mutation_type>(i), reason, true))
            ;
    }
    ASSERT(you.attribute[ATTR_TEMP_MUTATIONS] == 0);
    ASSERT(you.how_mutated(false, true, false) == 0);
    you.attribute[ATTR_TEMP_MUT_XP] = 0;

    return !you.how_mutated();
}

/*
 * Delete all temporary mutations.
 *
 * @return  Whether the function found mutations to delete.
 */
bool delete_all_temp_mutations(const string &reason)
{
    bool found = false;
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        while (you.has_temporary_mutation(static_cast<mutation_type>(i)))
            if (_delete_single_mutation_level(static_cast<mutation_type>(i), reason, true))
                found = true;
    }
    // the rest of the bookkeeping is handled in _delete_single_mutation_level
    you.attribute[ATTR_TEMP_MUT_XP] = 0;
    return found;
}

/*
 * Delete a single level of a random temporary mutation.
 * This function does not itself do XP-related bookkeeping; see `temp_mutation_wanes()`.
 *
 * @return          Whether the function found a mutation to delete.
 */
bool delete_temp_mutation()
{
    if (you.attribute[ATTR_TEMP_MUTATIONS] > 0)
    {
        mutation_type mutat = NUM_MUTATIONS;

        int count = 0;
        for (int i = 0; i < NUM_MUTATIONS; i++)
            if (you.has_temporary_mutation(static_cast<mutation_type>(i)) && one_chance_in(++count))
                mutat = static_cast<mutation_type>(i);

#if TAG_MAJOR_VERSION == 34
        // We had a brief period (between 0.14-a0-1589-g48c4fed and
        // 0.14-a0-1604-g40af2d8) where we corrupted attributes in transferred
        // games.
        if (mutat == NUM_MUTATIONS)
        {
            mprf(MSGCH_ERROR, "Found no temp mutations, clearing.");
            you.attribute[ATTR_TEMP_MUTATIONS] = 0;
            return false;
        }
#else
        ASSERTM(mutat != NUM_MUTATIONS, "Found no temp mutations, expected %d",
                                        you.attribute[ATTR_TEMP_MUTATIONS]);
#endif

        if (_delete_single_mutation_level(mutat, "temp mutation expiry", true))
            return true;
    }

    return false;
}

/// Attempt to look up a description of this mutation in the database.
/// Not to be confused with mutation_desc().
string get_mutation_desc(mutation_type mut)
{
    const char* const name = mutation_name(mut);
    const string key = make_stringf("%s mutation", name);
    string lookup = getLongDescription(key);
    hint_replace_cmds(lookup);

    ostringstream desc;
    desc << lookup;
    if (lookup.empty()) // Nothing found?
        desc << mutation_desc(mut, -1, false) << "\n";

    desc << mut_aux_attack_desc(mut);

    // TODO: consider adding other fun facts here
        // _get_mutation_def(mut).form_based
        // type of mutation (species, etc)
        // if we tracked it: source (per level, I guess?)
        // gain/loss messages (to clarify to players when they get a
        //                     confusing message)

    const string quote = getQuoteString(key);
    if (!quote.empty())
        desc << "\n_________________\n\n<darkgrey>" << quote << "</darkgrey>";
    return desc.str();
}

const char* mutation_name(mutation_type mut, bool allow_category)
{
    if (allow_category && mut >= CATEGORY_MUTATIONS && mut < MUT_NON_MUTATION)
        return _get_category_mutation_def(mut).short_desc;

    // note -- this can produce crashes if fed invalid mutations, e.g. if allow_category is false and mut is a category mutation
    if (!_is_valid_mutation(mut))
        return nullptr;

    return _get_mutation_def(mut).short_desc;
}

const char* category_mutation_name(mutation_type mut)
{
    if (mut < CATEGORY_MUTATIONS || mut >= MUT_NON_MUTATION)
        return nullptr;
    return _get_category_mutation_def(mut).short_desc;
}

/*
 * Given some name, return a mutation type. Tries to match the short description as found in `mutation-data.h`.
 * If `partial_matches` is set, it will fill the vector with any partial matches it finds. If there is exactly one,
 * will return this mutation, otherwise, will fail.
 *
 * @param allow_category    whether to include category mutation types (e.g. RANDOM_GOOD)
 * @param partial_matches   an optional pointer to a vector, in case the consumer wants to do something
 *                          with the partial match results (e.g. show them to the user). If this is `nullptr`,
 *                          will accept only exact matches.
 *
 * @return the mutation type if successful, otherwise NUM_MUTATIONS if it can't find a single match.
 */
mutation_type mutation_from_name(string name, bool allow_category, vector<mutation_type> *partial_matches)
{
    mutation_type mutat = NUM_MUTATIONS;

    string spec = lowercase_string(name);

    if (allow_category)
    {
        for (int i = CATEGORY_MUTATIONS; i < MUT_NON_MUTATION; ++i)
        {
            mutation_type mut = static_cast<mutation_type>(i);
            const char* mut_name_c = category_mutation_name(mut);
            if (!mut_name_c)
                continue;
            const string mut_name = lowercase_string(mut_name_c);

            if (spec == mut_name)
                return mut; // note, won't fully populate partial_matches

            if (partial_matches && strstr(mut_name.c_str(), spec.c_str()))
                partial_matches->push_back(mut);
        }
    }

    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        const char *mut_name_c = mutation_name(mut);
        if (!mut_name_c)
            continue;
        const string mut_name = lowercase_string(mut_name_c);

        if (spec == mut_name)
        {
            mutat = mut;
            break;
        }

        if (partial_matches && strstr(mut_name.c_str(), spec.c_str()))
            partial_matches->push_back(mut);
    }

    // If only one matching mutation, use that.
    if (partial_matches && mutat == NUM_MUTATIONS && partial_matches->size() == 1)
        mutat = (*partial_matches)[0];
    return mutat;
}

/**
 * A summary of what the next level of a mutation does.
 *
 * @param mut   The mutation_type in question; e.g. MUT_FRAIL.
 * @return      The mutation's description, helpfully trimmed.
 *              e.g. "you are frail (-10% HP)".
 */
string mut_upgrade_summary(mutation_type mut)
{
    if (!_is_valid_mutation(mut))
        return nullptr;

    string mut_desc =
        lowercase_first(mutation_desc(mut, you.mutation[mut] + 1));
    strip_suffix(mut_desc, ".");
    return mut_desc;
}

int mutation_max_levels(mutation_type mut)
{
    if (!_is_valid_mutation(mut))
        return 0;

    return _get_mutation_def(mut).levels;
}

/// Return a string describing the mutation.
/// If colour is true, also add the colour annotation.
/// Not to be confused with get_mutation_desc().
string mutation_desc(mutation_type mut, int level, bool colour,
        bool is_sacrifice)
{
    // Ignore the player's forms, etc.
    const bool ignore_player = (level != -1);

    const mutation_activity_type active = mutation_activity_level(mut);
    const bool partially_active = (active == mutation_activity_type::PARTIAL);
    const bool fully_inactive = (active == mutation_activity_type::INACTIVE);

    const bool temporary = you.has_temporary_mutation(mut);

    // level == -1 means default action of current level
    if (level == -1)
    {
        if (!fully_inactive)
            level = you.get_mutation_level(mut);
        else // give description of fully active mutation
            level = you.get_base_mutation_level(mut);
    }

    string result;

    const mutation_def& mdef = _get_mutation_def(mut);

    if (mut == MUT_STRONG || mut == MUT_CLEVER
        || mut == MUT_AGILE || mut == MUT_WEAK
        || mut == MUT_DOPEY || mut == MUT_CLUMSY)
    {
        level = min(level, 2);
    }
    if (mut == MUT_ICEMAIL)
    {
        ostringstream ostr;
        ostr << mdef.have[0] << player_icemail_armour_class() << ")";
        result = ostr.str();
    }
    else if (mut == MUT_CONDENSATION_SHIELD)
    {
        ostringstream ostr;
        ostr << mdef.have[0] << player_condensation_shield_class() << ")";
        result = ostr.str();
    }
    else if (mut == MUT_SANGUINE_ARMOUR)
    {
        ostringstream ostr;
        ostr << mdef.have[level - 1] << sanguine_armour_bonus() / 100 << ")";
        result = ostr.str();
    }
    else if (mut == MUT_MP_WANDS && you.has_mutation(MUT_HP_CASTING))
        result = "You expend health (3 HP) to strengthen your wands.";
    else if (!ignore_player && mut == MUT_TENTACLE_ARMS)
    {
        const string num_tentacles = number_in_words(you.has_tentacles(false));
        result = make_stringf(
            "You have tentacles for arms and can constrict up to %s enemies at once.",
            num_tentacles.c_str());
    }
    else if (!ignore_player && you.has_innate_mutation(MUT_PAWS) && mut == MUT_CLAWS)
        result = "You have sharp claws."; // XX ugly override
    else if (result.empty() && level > 0)
        result = mdef.have[level - 1];

    if (!ignore_player)
    {
        if (fully_inactive)
            result = "((" + result + "))";
        else if (partially_active)
            result = "(" + result + ")";
    }

    if (temporary)
        result = "[" + result + "]";

    if (colour)
    {
        const char* colourname = (is_bad_mutation(mut) ? "red" : "lightgrey");
        const bool permanent   = you.has_innate_mutation(mut);

        if (permanent)
        {
            const bool demonspawn = (you.species == SP_DEMONSPAWN);
            const bool extra = you.get_base_mutation_level(mut, false, true, true) > 0;

            if (fully_inactive
                || (mut == MUT_COLD_BLOODED && player_res_cold(false) > 0))
            {
                colourname = "darkgrey";
            }
            else if (is_sacrifice)
                colourname = "lightred";
            else if (partially_active)
                colourname = demonspawn ? "yellow"    : "blue";
            else if (extra)
                colourname = demonspawn ? "lightcyan" : "cyan";
            else
                colourname = demonspawn ? "cyan"      : "lightblue";
        }
        else if (fully_inactive)
            colourname = "darkgrey";
        else if (partially_active)
            colourname = "brown";
        else if (is_slime_mutation(mut))
            colourname = "lightgreen";
        else if (temporary)
            colourname = (you.get_base_mutation_level(mut, true, false, true) > 0) ?
                         "lightmagenta" : "magenta";

        // Build the result
        ostringstream ostr;
        ostr << '<' << colourname << '>' << result
             << "</" << colourname << '>';
        result = ostr.str();
    }

    return result;
}

// Get a description for a mutation the player will gain at a future XL,
// reworded slightly to sound like they do not currently have it.
static string _future_mutation_description(mutation_type mut_type, int levels)
{
    levels += you.innate_mutation[mut_type];
    string mut_desc = mutation_desc(mut_type, levels);

    // If we have a custom message defined for this future mutation, use it.
    const char* const* future_desc = _get_mutation_def(mut_type).will_gain;
    if (future_desc[levels - 1] != NULL)
        return string(future_desc[levels - 1]);

    // Otherwise do some simple string replacements to cover common cases.
    mut_desc = replace_all(mut_desc, " can ", " will be able to ");
    mut_desc = replace_all(mut_desc, " have ", " will have ");
    mut_desc = replace_all(mut_desc, " are ", " will be ");
    mut_desc = replace_all(mut_desc, " is ", " will be ");

    return mut_desc;
}

// The "when" numbers indicate the range of times in which the mutation tries
// to place itself; it will be approximately placed between when% and
// (when + 100)% of the way through the mutations. For example, you should
// usually get all your body slot mutations in the first 2/3 of your
// mutations and you should usually only start your tier 3 facet in the second
// half of your mutations. See _order_ds_mutations() for details.
static const facet_def _demon_facets[] =
{
    // Body Slot facets
    { 0, { MUT_CLAWS, MUT_CLAWS, MUT_CLAWS },
      { -33, -33, -33 } },
    { 0, { MUT_HORNS, MUT_HORNS, MUT_HORNS },
      { -33, -33, -33 } },
    { 0, { MUT_ANTENNAE, MUT_ANTENNAE, MUT_ANTENNAE },
      { -33, -33, -33 } },
    { 0, { MUT_HOOVES, MUT_HOOVES, MUT_HOOVES },
      { -33, -33, -33 } },
    { 0, { MUT_WEAKNESS_STINGER, MUT_WEAKNESS_STINGER, MUT_WEAKNESS_STINGER },
      { -33, -33, -33 } },
    { 0, { MUT_DEMONIC_TOUCH, MUT_DEMONIC_TOUCH, MUT_DEMONIC_TOUCH },
      { -33, -33, -33 } },
    // Scale mutations
    { 1, { MUT_DISTORTION_FIELD, MUT_DISTORTION_FIELD, MUT_DISTORTION_FIELD },
      { -33, -33, 0 } },
    { 1, { MUT_ICY_BLUE_SCALES, MUT_ICY_BLUE_SCALES, MUT_ICY_BLUE_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_LARGE_BONE_PLATES, MUT_LARGE_BONE_PLATES, MUT_LARGE_BONE_PLATES },
      { -33, -33, 0 } },
    { 1, { MUT_MOLTEN_SCALES, MUT_MOLTEN_SCALES, MUT_MOLTEN_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_RUGGED_BROWN_SCALES, MUT_RUGGED_BROWN_SCALES,
           MUT_RUGGED_BROWN_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_SLIMY_GREEN_SCALES, MUT_SLIMY_GREEN_SCALES, MUT_SLIMY_GREEN_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_THIN_METALLIC_SCALES, MUT_THIN_METALLIC_SCALES,
        MUT_THIN_METALLIC_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_THIN_SKELETAL_STRUCTURE, MUT_THIN_SKELETAL_STRUCTURE,
           MUT_THIN_SKELETAL_STRUCTURE },
      { -33, -33, 0 } },
    { 1, { MUT_YELLOW_SCALES, MUT_YELLOW_SCALES, MUT_YELLOW_SCALES },
      { -33, -33, 0 } },
    { 1, { MUT_STURDY_FRAME, MUT_STURDY_FRAME, MUT_STURDY_FRAME },
      { -33, -33, 0 } },
    { 1, { MUT_SANGUINE_ARMOUR, MUT_SANGUINE_ARMOUR, MUT_SANGUINE_ARMOUR },
      { -33, -33, 0 } },
    { 1, { MUT_BIG_BRAIN, MUT_BIG_BRAIN, MUT_BIG_BRAIN },
      { -33, -33, 0 } },
    { 1, { MUT_SHARP_SCALES, MUT_SHARP_SCALES, MUT_SHARP_SCALES },
      { -33, -33, 0 } },
    // Tier 2 facets
    { 2, { MUT_IGNITE_BLOOD, MUT_IGNITE_BLOOD, MUT_IGNITE_BLOOD },
      { -33, 0, 0 } },
    { 2, { MUT_CONDENSATION_SHIELD, MUT_ICEMAIL, MUT_ICEMAIL },
      { -33, 0, 0 } },
    { 2, { MUT_DEMONIC_MAGIC, MUT_DEMONIC_MAGIC, MUT_DEMONIC_MAGIC },
      { -33, 0, 0 } },
    { 2, { MUT_POWERED_BY_DEATH, MUT_POWERED_BY_DEATH, MUT_POWERED_BY_DEATH },
      { -33, 0, 0 } },
    { 2, { MUT_DEMONIC_GUARDIAN, MUT_DEMONIC_GUARDIAN, MUT_DEMONIC_GUARDIAN },
      { -33, 0, 0 } },
    { 2, { MUT_SPINY, MUT_SPINY, MUT_SPINY },
      { -33, 0, 0 } },
    { 2, { MUT_POWERED_BY_PAIN, MUT_POWERED_BY_PAIN, MUT_POWERED_BY_PAIN },
      { -33, 0, 0 } },
    { 2, { MUT_FOUL_STENCH, MUT_FOUL_STENCH, MUT_FOUL_STENCH },
      { -33, 0, 0 } },
    { 2, { MUT_MANA_REGENERATION, MUT_MANA_SHIELD, MUT_MANA_LINK },
      { -33, 0, 0 } },
    { 2, { MUT_FOUL_SHADOW, MUT_FOUL_SHADOW, MUT_FOUL_SHADOW },
      { -33, 0, 0 } },
    // Tier 3 facets
    { 3, { MUT_DEMONIC_WILL, MUT_TORMENT_RESISTANCE, MUT_HURL_DAMNATION },
      { 50, 50, 50 } },
    { 3, { MUT_ROBUST, MUT_ROBUST, MUT_ROBUST },
      { 50, 50, 50 } },
    { 3, { MUT_HEX_ENHANCER, MUT_BLACK_MARK, MUT_SILENCE_AURA },
      { 50, 50, 50 } },
    { 3, { MUT_AUGMENTATION, MUT_AUGMENTATION, MUT_AUGMENTATION },
      { 50, 50, 50 } },
    { 3, { MUT_CORRUPTING_PRESENCE, MUT_CORRUPTING_PRESENCE, MUT_WORD_OF_CHAOS },
      { 50, 50, 50 } },
};

static bool _works_at_tier(const facet_def& facet, int tier)
{
    return facet.tier == tier;
}

typedef decltype(facet_def().muts) mut_array_t;
static bool _slot_is_unique(const mut_array_t &mut,
                            set<const facet_def *> facets_used)
{
    set<equipment_type> eq;

    // find the equipment slot(s) used by mut
    for (const body_facet_def &facet : _body_facets)
    {
        for (mutation_type slotmut : mut)
            if (facet.mut == slotmut)
                eq.insert(facet.eq);
    }

    if (eq.empty())
        return true;

    for (const facet_def *used : facets_used)
    {
        for (const body_facet_def &facet : _body_facets)
            if (facet.mut == used->muts[0] && eq.count(facet.eq))
                return false;
    }

    return true;
}

static vector<demon_mutation_info> _select_ds_mutations()
{
    int ct_of_tier[] = { 1, 1, 2, 1 };
    // 1 in 10 chance to create a monstrous set
    if (one_chance_in(10))
    {
        ct_of_tier[0] = 3;
        ct_of_tier[1] = 0;
    }

try_again:
    vector<demon_mutation_info> ret;

    ret.clear();
    int absfacet = 0;
    int elemental = 0;
    int cloud_producing = 0;
    int retaliation = 0;

    set<const facet_def *> facets_used;

    for (int tier = ARRAYSZ(ct_of_tier) - 1; tier >= 0; --tier)
    {
        for (int nfacet = 0; nfacet < ct_of_tier[tier]; ++nfacet)
        {
            const facet_def* next_facet;

            do
            {
                next_facet = &RANDOM_ELEMENT(_demon_facets);
            }
            while (!_works_at_tier(*next_facet, tier)
                   || facets_used.count(next_facet)
                   || !_slot_is_unique(next_facet->muts, facets_used));

            facets_used.insert(next_facet);

            for (int i = 0; i < 3; ++i)
            {
                mutation_type m = next_facet->muts[i];

                ret.emplace_back(m, next_facet->when[i], absfacet);

                if (i==0)
                {
                    if (m == MUT_CONDENSATION_SHIELD || m == MUT_IGNITE_BLOOD)
                        elemental++;

                    if (m == MUT_FOUL_STENCH || m == MUT_IGNITE_BLOOD)
                        cloud_producing++;

                    if (m == MUT_SPINY
                        || m == MUT_FOUL_STENCH
                        || m == MUT_FOUL_SHADOW)
                    {
                        retaliation++;
                    }
                }
            }

            ++absfacet;
        }
    }

    if (elemental > 1)
        goto try_again;

    if (cloud_producing > 1)
        goto try_again;

    if (retaliation > 1)
        goto try_again;

    return ret;
}

static vector<mutation_type>
_order_ds_mutations(vector<demon_mutation_info> muts)
{
    vector<mutation_type> out;
    vector<int> times;
    FixedVector<int, 1000> time_slots;
    time_slots.init(-1);
    for (unsigned int i = 0; i < muts.size(); i++)
    {
        int first = max(0, muts[i].when);
        int last = min(100, muts[i].when + 100);
        int k;
        do
        {
            k = 10 * first + random2(10 * (last - first));
        }
        while (time_slots[k] >= 0);
        time_slots[k] = i;
        times.push_back(k);

        // Don't reorder mutations within a facet.
        for (unsigned int j = i; j > 0; j--)
        {
            if (muts[j].facet == muts[j-1].facet && times[j] < times[j-1])
            {
                int earlier = times[j];
                int later = times[j-1];
                time_slots[earlier] = j-1;
                time_slots[later] = j;
                times[j-1] = earlier;
                times[j] = later;
            }
            else
                break;
        }
    }

    for (int time = 0; time < 1000; time++)
        if (time_slots[time] >= 0)
            out.push_back(muts[time_slots[time]].mut);

    return out;
}

static vector<player::demon_trait>
_schedule_ds_mutations(vector<mutation_type> muts)
{
    list<mutation_type> muts_left(muts.begin(), muts.end());

    list<int> slots_left;

    vector<player::demon_trait> out;

    for (int level = 2; level <= 27; ++level)
        slots_left.push_back(level);

    while (!muts_left.empty())
    {
        if (out.empty() // always give a mutation at XL 2
            || x_chance_in_y(muts_left.size(), slots_left.size()))
        {
            player::demon_trait dt;

            dt.level_gained = slots_left.front();
            dt.mutation     = muts_left.front();

            dprf("Demonspawn will gain %s at level %d",
                    _get_mutation_def(dt.mutation).short_desc, dt.level_gained);

            out.push_back(dt);

            muts_left.pop_front();
        }
        slots_left.pop_front();
    }

    return out;
}

void roll_demonspawn_mutations()
{
    // intentionally create the subgenerator either way, so that this has the
    // same impact on the current main rng for all chars.
    rng::subgenerator ds_rng;

    if (you.species != SP_DEMONSPAWN)
        return;
    you.demonic_traits = _schedule_ds_mutations(
                         _order_ds_mutations(
                         _select_ds_mutations()));
}

bool perma_mutate(mutation_type which_mut, int how_much, const string &reason)
{
    ASSERT(_is_valid_mutation(which_mut));
    ASSERT(!mut_check_conflict(which_mut, true));

    int cap = get_mutation_cap(which_mut);
    how_much = min(how_much, cap);

    int rc = 1;
    // clear out conflicting mutations
    int count = 0;
    while (rc == 1 && ++count < 100)
        rc = _handle_conflicting_mutations(which_mut, true, reason);
    ASSERT(rc == 0);

    int levels = 0;
    while (how_much-- > 0)
    {
        dprf("Perma Mutate %s: cap %d, total %d, innate %d", mutation_name(which_mut), cap,
            you.get_base_mutation_level(which_mut), you.get_innate_mutation_level(which_mut));
        if (you.get_base_mutation_level(which_mut, true, false, false) < cap
            && !mutate(which_mut, reason, false, true, false, false, MUTCLASS_INNATE))
        {
            dprf("Innate mutation failed.");
            break;
        }
        levels++;
    }

#ifdef DEBUG
    // don't validate permamutate directly on level regain; this is so that wizmode level change
    // functions can work correctly.
    if (you.experience_level >= you.max_level)
        validate_mutations(false);
#endif
    return levels > 0;
}

bool temp_mutate(mutation_type which_mut, const string &reason)
{
    return mutate(which_mut, reason, false, false, false, false, MUTCLASS_TEMPORARY);
}

int temp_mutation_roll()
{
    return min(you.experience_level, 17) * (500 + roll_dice(5, 500)) / 17;
}

bool temp_mutation_wanes()
{
    const int starting_tmuts = you.attribute[ATTR_TEMP_MUTATIONS];
    if (starting_tmuts == 0)
        return false;

    int num_remove = min(starting_tmuts,
        max(starting_tmuts * 5 / 12 - random2(3),
        1 + random2(3)));

    mprf(MSGCH_DURATION, "You feel the corruption within you wane %s.",
        (num_remove >= starting_tmuts ? "completely" : "somewhat"));

    for (int i = 0; i < num_remove; ++i)
        delete_temp_mutation(); // chooses randomly

    if (you.attribute[ATTR_TEMP_MUTATIONS] > 0)
        you.attribute[ATTR_TEMP_MUT_XP] += temp_mutation_roll();
    else
        you.attribute[ATTR_TEMP_MUT_XP] = 0;
    ASSERT(you.attribute[ATTR_TEMP_MUTATIONS] < starting_tmuts);
    return true;
}

/**
 * How mutated is the player?
 *
 * @param innate Whether to count innate mutations (default false).
 * @param levels Whether to add up mutation levels, as opposed to just counting number of mutations (default false).
 * @param temp Whether to count temporary mutations (default true).
 * @return Either the number of matching mutations, or the sum of their
 *         levels, depending on \c levels
 */
int player::how_mutated(bool innate, bool levels, bool temp) const
{
    int result = 0;

    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (you.mutation[i])
        {
            // Infernal Marks should count for silver vulnerability despite
            // being permanent mutations.
            const bool check_innate = innate || is_makhleb_mark(static_cast<mutation_type>(i));
            const int mut_level = get_base_mutation_level(static_cast<mutation_type>(i),
                                                          check_innate, temp);

            if (levels)
                result += mut_level;
            else if (mut_level > 0)
                result++;
        }
        if (you.species == SP_DEMONSPAWN
            && you.props.exists(NUM_SACRIFICES_KEY))
        {
            result -= you.props[NUM_SACRIFICES_KEY].get_int();
        }
    }

    return result;
}

// Primary function to handle demonic guardians.
// Guardian tier is partially based on player experience level. This should
// allow players to get the mutation early without it going totally out of
// control.
void check_demonic_guardian()
{
    // Players hated by all monsters don't get guardians, so that they aren't
    // swarmed by hostile executioners whenever things get rough.
    if (you.allies_forbidden())
        return;

    const int mutlevel = you.get_mutation_level(MUT_DEMONIC_GUARDIAN);

    if (you.duration[DUR_DEMONIC_GUARDIAN] == 0)
    {
        monster_type mt;
        int guardian_str = mutlevel + div_rand_round(you.experience_level - 9, 9);

        switch (guardian_str)
        {
        case 1:
            mt = random_choose(MONS_QUASIT, MONS_WHITE_IMP, MONS_UFETUBUS,
                               MONS_IRON_IMP, MONS_SHADOW_IMP);
            break;
        case 2:
            mt = random_choose(MONS_ORANGE_DEMON, MONS_ICE_DEVIL,
                               MONS_RUST_DEVIL, MONS_HELLWING);
            break;
        case 3:
            mt = random_choose(MONS_SOUL_EATER, MONS_SMOKE_DEMON,
                               MONS_SIXFIRHY, MONS_SUN_DEMON);
            break;
        case 4:
            mt = random_choose(MONS_BALRUG, MONS_CACODEMON, MONS_SIN_BEAST);
            break;
        case 5:
            mt = random_choose(MONS_EXECUTIONER, MONS_HELL_SENTINEL,
                               MONS_BRIMSTONE_FIEND);
            break;
        default:
            die("Invalid demonic guardian level: %d", mutlevel);
        }

        monster *guardian = create_monster(
            mgen_data(mt, BEH_FRIENDLY, you.pos(), MHITYOU,
                      MG_FORCE_BEH | MG_AUTOFOE).set_summoned(&you, 0, summ_dur(2)));

        if (!guardian)
            return;

        guardian->flags |= MF_NO_REWARD;
        guardian->flags |= MF_DEMONIC_GUARDIAN;

        guardian->add_ench(ENCH_LIFE_TIMER);

        // no more guardians for mutlevel+1 to mutlevel+20 turns
        you.duration[DUR_DEMONIC_GUARDIAN] = 10*(mutlevel + random2(20));

        mpr("A demonic guardian appears!");
    }
}

/**
 * Update the map knowledge based on any monster detection sources the player
 * has.
 */
void check_monster_detect()
{
    int radius = player_monster_detect_radius();
    if (radius <= 0)
        return;

    for (radius_iterator ri(you.pos(), radius, C_SQUARE); ri; ++ri)
    {
        monster* mon = monster_at(*ri);
        map_cell& cell = env.map_knowledge(*ri);
        if (!mon)
        {
            if (cell.detected_monster())
                cell.clear_monster();
            continue;
        }
        if (mon->is_firewood())
            continue;

        // [ds] If the PC remembers the correct monster at this
        // square, don't trample it with MONS_SENSED. Forgetting
        // legitimate monster memory affects travel, which can
        // path around mimics correctly only if it can actually
        // *see* them in monster memory -- overwriting the mimic
        // with MONS_SENSED causes travel to bounce back and
        // forth, since every time it leaves LOS of the mimic, the
        // mimic is forgotten (replaced by MONS_SENSED).
        // XXX: since mimics were changed, is this safe to remove now?
        const monster_type remembered_monster = cell.monster();
        if (remembered_monster == mon->type)
            continue;

        const monster_type mc = mon->friendly() ? MONS_SENSED_FRIENDLY
            : have_passive(passive_t::detect_montier)
            ? ash_monster_tier(mon)
            : MONS_SENSED;

        env.map_knowledge(*ri).set_detected_monster(mc);

        // Don't bother warning the player (or interrupting autoexplore) about
        // friendly monsters or those known to be easy, or those recently
        // warned about
        if (mc == MONS_SENSED_TRIVIAL || mc == MONS_SENSED_EASY
            || mc == MONS_SENSED_FRIENDLY || mon->wont_attack()
            || testbits(mon->flags, MF_SENSED))
        {
            continue;
        }

        for (radius_iterator ri2(mon->pos(), 2, C_SQUARE); ri2; ++ri2)
        {
            if (you.see_cell(*ri2))
            {
                mon->flags |= MF_SENSED;
                interrupt_activity(activity_interrupt::sense_monster);
                break;
            }
        }
    }
}

int augmentation_amount()
{
    int amount = 0;
    const int level = you.get_mutation_level(MUT_AUGMENTATION);

    for (int i = 0; i < level; ++i)
    {
        if (you.hp >= ((i + level) * you.hp_max) / (2 * level))
            amount++;
    }

    return amount;
}

void reset_powered_by_death_duration()
{
    const int pbd_dur = random_range(2, 5);
    you.set_duration(DUR_POWERED_BY_DEATH, pbd_dur);
}

/// How much XP is required for your next [d]evolution mutation?
static int _evolution_mut_xp(bool malignant)
{
    int xp = exp_needed(you.experience_level + 1)
             - exp_needed(you.experience_level);
    if (malignant)
        return xp / 4;
    return xp;
}

/// Update you.attribute[ATTR_EVOL_XP].
void set_evolution_mut_xp(bool malignant)
{
    // Intentionally erase any 'excess' XP to avoid this triggering
    // too quickly in the early game after big XP gains.
    you.attribute[ATTR_EVOL_XP] = _evolution_mut_xp(malignant);
    dprf("setting evol XP to %d", you.attribute[ATTR_EVOL_XP]);
}
