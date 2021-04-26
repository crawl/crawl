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

#include "ability.h"
#include "areas.h"
#include "cio.h"
#include "coordit.h"
#include "dactions.h"
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
#include "xom.h"

using namespace ui;

static bool _delete_single_mutation_level(mutation_type mutat, const string &reason, bool transient);

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
    xom     = 1 << 4, // xom being xom

    last    = xom
};
DEF_BITFIELD(mutflags, mutflag, 4);
COMPILE_CHECK(mutflags::exponent(mutflags::last_exponent) == mutflag::last);

#include "mutation-data.h"

static const body_facet_def _body_facets[] =
{
    //{ EQ_NONE, MUT_FANGS },
    { EQ_HELMET, MUT_HORNS },
    { EQ_HELMET, MUT_ANTENNAE },
    //{ EQ_HELMET, MUT_BEAK },
    { EQ_GLOVES, MUT_CLAWS },
    { EQ_BOOTS, MUT_HOOVES },
    { EQ_BOOTS, MUT_TALONS }
};

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
#endif
    };

vector<mutation_type> get_removed_mutations()
{
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
#if TAG_MAJOR_VERSION == 34
    { MUT_REGENERATION,        MUT_SLOW_METABOLISM,         0},
#endif
    { MUT_REGENERATION,        MUT_INHIBITED_REGENERATION,  0},
    { MUT_FAST,                MUT_SLOW,                    0},
#if TAG_MAJOR_VERSION == 34
    { MUT_STRONG_STIFF,        MUT_FLEXIBLE_WEAK,           1},
#endif
    { MUT_STRONG,              MUT_WEAK,                    1},
    { MUT_CLEVER,              MUT_DOPEY,                   1},
    { MUT_AGILE,               MUT_CLUMSY,                  1},
    { MUT_ROBUST,              MUT_FRAIL,                   1},
    { MUT_HIGH_MAGIC,          MUT_LOW_MAGIC,               1},
    { MUT_WILD_MAGIC,          MUT_SUBDUED_MAGIC,           1},
#if TAG_MAJOR_VERSION == 34
    { MUT_CARNIVOROUS,         MUT_HERBIVOROUS,             1},
    { MUT_SLOW_METABOLISM,     MUT_FAST_METABOLISM,         1},
#endif
    { MUT_REGENERATION,        MUT_INHIBITED_REGENERATION,  1},
    { MUT_BERSERK,             MUT_CLARITY,                 1},
    { MUT_FAST,                MUT_SLOW,                    1},
    { MUT_FANGS,               MUT_BEAK,                   -1},
    { MUT_ANTENNAE,            MUT_HORNS,                  -1}, // currently overridden by physiology_mutation_conflict
    { MUT_HOOVES,              MUT_TALONS,                 -1}, // currently overridden by physiology_mutation_conflict
    { MUT_TRANSLUCENT_SKIN,    MUT_CAMOUFLAGE,             -1},
    { MUT_MUTATION_RESISTANCE, MUT_EVOLUTION,              -1},
    { MUT_ANTIMAGIC_BITE,      MUT_ACIDIC_BITE,            -1},
    { MUT_HEAT_RESISTANCE,     MUT_HEAT_VULNERABILITY,     -1},
    { MUT_COLD_RESISTANCE,     MUT_COLD_VULNERABILITY,     -1},
    { MUT_SHOCK_RESISTANCE,    MUT_SHOCK_VULNERABILITY,    -1},
    { MUT_STRONG_WILLED,       MUT_WEAK_WILLED,            -1},
    { MUT_NO_REGENERATION,     MUT_INHIBITED_REGENERATION, -1},
    { MUT_NO_REGENERATION,     MUT_REGENERATION,           -1},
    { MUT_HP_CASTING,          MUT_HIGH_MAGIC,             -1},
    { MUT_HP_CASTING,          MUT_LOW_MAGIC,              -1},
};

static bool _mut_has_use(const mutation_def &mut, mutflag use)
{
    return bool(mut.uses & use);
}

#define MUT_BAD(mut) _mut_has_use((mut), mutflag::bad)
#define MUT_GOOD(mut) _mut_has_use((mut), mutflag::good)

static int _mut_weight(const mutation_def &mut, mutflag use)
{
    switch (use)
    {
        case mutflag::jiyva:
        case mutflag::qazlal:
        case mutflag::xom:
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
    // First make sure the player's form permits the mutation.
    if (!form_keeps_mutations())
    {
        if (you.form == transformation::dragon)
        {
            monster_type drag = species::dragon_form(you.species);
            if (mut == MUT_SHOCK_RESISTANCE && drag == MONS_STORM_DRAGON)
                return mutation_activity_type::FULL;
            if (mut == MUT_UNBREATHING && drag == MONS_IRON_DRAGON)
                return mutation_activity_type::FULL;
            if ((mut == MUT_ACIDIC_BITE || mut == MUT_ACID_RESISTANCE)
                && drag == MONS_GOLDEN_DRAGON)
            {
                return mutation_activity_type::FULL;
            }
            if (mut == MUT_STINGER && drag == MONS_SWAMP_DRAGON)
                return mutation_activity_type::FULL;
        }
        // Vampire bats keep their fangs.
        if (you.form == transformation::bat
            && you.has_innate_mutation(MUT_VAMPIRISM)
            && mut == MUT_FANGS)
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

    //XXX: Should this make claws inactive too?
    if (you.form == transformation::blade_hands && mut == MUT_PAWS)
        return mutation_activity_type::INACTIVE;

    if (you.form == transformation::tree && mut == MUT_TELEPORT)
        return mutation_activity_type::INACTIVE;
#if TAG_MAJOR_VERSION == 34
    if ((you_worship(GOD_PAKELLAS) || player_under_penance(GOD_PAKELLAS))
         && (mut == MUT_MANA_LINK || mut == MUT_MANA_REGENERATION))
    {
        return mutation_activity_type::INACTIVE;
    }
#endif

    if (mut == MUT_BERSERK && you.is_lifeless_undead())
        return mutation_activity_type::INACTIVE;

    if (!form_can_bleed(you.form) && mut == MUT_SANGUINE_ARMOUR)
        return mutation_activity_type::INACTIVE;

    if (mut == MUT_DEMONIC_GUARDIAN && you.get_mutation_level(MUT_NO_LOVE))
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

// Counts of various statuses/types of mutations from the current/most
// recent call to describe_mutations. TODO: eliminate
static int _num_full_suppressed = 0;
static int _num_part_suppressed = 0;
static int _num_transient = 0;

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
    {
        return _suppressedmut(desc, terse);
        ++_num_full_suppressed;
    }
    else
        return _innatemut(desc, terse);
}

static string _dragon_abil(string desc, bool terse=false)
{
    const bool supp = form_changed_physiology()
                            && you.form != transformation::dragon;
    return _annotate_form_based(desc, supp, terse);
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

// TODO: reimplement other form quirks as mutations, generalize this idea?
static bool _is_appendage_mutation(mutation_type mut)
{
    for (auto app : you.props[APPENDAGE_KEY].get_vector())
        if (mut == static_cast<mutation_type>(app.get_int()))
            return true;
    return false;
}

static vector<string> _get_mutations(bool terse)
{
    vector<string> result;

    bool pois_printed = false;

    // XX sort good and bad non-permanent mutations better? Comes up mostly for
    // vampires

    // non-permanent and form-based stuff

    if (you.form != transformation::none)
    {
        const auto *form = get_form(you.form);
        ASSERT(form);
        // we could add form->get_long_name() here for `terse`, but the line in
        // % is shown right below a line which includes the form name.
        if (!terse)
            result.push_back(_formmut(form->get_description()));
        else if (you.form == transformation::appendage)
        {
            // terse mode: these mutations are skipped later, so add the short
            // forms here. The appendage description covers the long form case.
            for (auto app : you.props[APPENDAGE_KEY].get_vector())
            {
                result.push_back(_terse_mut_name(
                                static_cast<mutation_type>(app.get_int())));
            }
        }

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

        if (form_base_movespeed(you.form) < 10)
            result.push_back(terse ? "fast" : _formmut("You move quickly."));

        // form-based flying can't be stopped, so don't print amphibiousness
        if (form->player_can_fly())
            result.push_back(terse ? "flying" : _formmut("You are flying."));
        else if (form->player_can_swim() && !you.can_swim(true)) // n.b. this could cause issues for non-dragon giant forms if they exist
            result.push_back(terse ? "amphibious" : _formmut("You are amphibious."));

        if (form->hp_mod > 10)
        {
            result.push_back(terse ? "boosted hp"
                : _formmut(make_stringf("Your maximum health is %sincreased.",
                    form->hp_mod < 13 ? "" : "greatly ")));
        }
        else if (form->hp_mod < 10)
            result.push_back(terse ? "reduced hp" : _badmut("Your maximum health is decreased."));

        // immunity comes from form
        if (!terse && player_res_poison(false, true, false) == 3
                    && !player_res_poison(false, false, false))
        {
            pois_printed = true;
            // wispform has a fakemut that prints something more general
            if (you.form != transformation::wisp)
                result.push_back(_formmut("You are immune to poison."));
        }

        // bad stuff
        if (!terse
            && (form->spellcasting_penalty > 0
                || you.form == transformation::shadow)) // hard-coded effect
        {
            result.push_back(_badmut("Your spellcasting is less reliable in this form."));
        }

        // XX say something about AC? Best would be to compare it to AC without
        // the form, but I'm not sure if that's possible

        // XX better synchronizing with various base armour/eq possibilities
        if (!terse && !you.has_mutation(MUT_NO_ARMOUR))
        {
            const string melding_desc = form->melding_description();
            if (!melding_desc.empty())
                result.push_back(_badmut(melding_desc));
        }
        if (!terse && !form->can_wield() && !you.has_mutation(MUT_NO_GRASPING))
        {
            // same as MUT_NO_GRASPING
            result.push_back(_badmut(
                "You are incapable of wielding weapons or throwing items."));
        }

        if (!form->can_cast)
            result.push_back(terse ? "no casting" : _badmut("You cannot cast spells."));

    }

    // This gets DUR_NO_POTIONS as well as necromutation, is that good?
    if (!you.can_drink(true) && you.can_drink(false))
        result.push_back(terse ? "no potions" : _badmut("You cannot drink.")); // same as MUT_NO_DRINK

    //pseudo-forms that come from species

    if (you.has_mutation(MUT_VAMPIRISM))
    {
        if (you.vampire_alive)
        {
            result.push_back(terse ? "alive" :
                _formmut("Your natural rate of healing is accelerated."));
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
            if (!pois_printed)
                result.push_back(_formmut("You are immune to poison."));
            pois_printed = true;
        }
    }

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
                  + " scales are "
                  + (you.species == SP_GREY_DRACONIAN ? "very " : "") + "hard";

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
                    !form_likes_water(), terse));
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

    if (!terse && species::get_stat_gain_multiplier(you.species) > 1)
        result.push_back(_innatemut("Your attributes grow dramatically as you level up."));

    // vampire, form cases handled above
    if (!terse && player_res_poison(false, false, false) == 3 && !pois_printed)
        result.push_back(_innatemut("You are immune to poison."));

    // First add (non-removable) inborn abilities and demon powers.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        mutation_type mut_type = static_cast<mutation_type>(i);
        if (_is_appendage_mutation(mut_type))
            continue;
        if (you.has_innate_mutation(mut_type))
        {
            result.push_back(terse ? _terse_mut_name(mut_type)
                                   : mutation_desc(mut_type, -1, true,
                                    ((you.sacrifices[i] != 0) ? true : false)));
        }
    }

    // Now add removable mutations.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        mutation_type mut_type = static_cast<mutation_type>(i);
        if (_is_appendage_mutation(mut_type))
            continue;
        if (you.get_base_mutation_level(mut_type, false, false, true) > 0
            && !you.has_innate_mutation(mut_type)
            && !you.has_temporary_mutation(mut_type))
        {
            result.push_back(terse ? _terse_mut_name(mut_type)
                                   : mutation_desc(mut_type, -1, true));
        }
    }

    //Finally, temporary mutations.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        mutation_type mut_type = static_cast<mutation_type>(i);
        if (you.has_temporary_mutation(mut_type))
        {
            result.push_back(terse ? _terse_mut_name(mut_type)
                                   : mutation_desc(mut_type, -1, true));
        }
    }

    return result;
}

string terse_mutation_list()
{
    const vector<string> mutations = _get_mutations(true);

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

    _num_full_suppressed = _num_part_suppressed = 0;
    _num_transient = 0;

    if (!drop_title)
    {
        result += "<white>";
        result += "Innate Abilities, Weirdness & Mutations";
        result += "</white>\n\n";
    }

    const vector<string> mutations = _get_mutations(false);

    if (mutations.empty())
        result += "You are rather mundane.\n";
    else
        result += join_strings(mutations.begin(), mutations.end(), "\n");

    return result;
}

static formatted_string _vampire_Ascreen_footer(bool first_page)
{
    const char *text = first_page ? "<w>Mutations</w>|Blood properties"
                                  : "Mutations|<w>Blood properties</w>";
    const string fmt = make_stringf("[<w>!</w>/<w>^</w>"
#ifdef USE_TILE_LOCAL
            "|<w>Right-click</w>"
#endif
            "]: %s", text);
    return formatted_string::parse_string(fmt);
}

static string _display_vampire_attributes()
{
    ASSERT(you.has_mutation(MUT_VAMPIRISM));

    string result;

    const int lines = 13;
    string column[lines][3] =
    {
        {"                     ", "<green>Alive</green>      ", "<lightred>Bloodless</lightred>"},
                                 //Full       Bloodless
        {"Regeneration         ", "fast       ", "none with monsters in sight"},

        {"HP modifier          ", "none       ", "-20%"},

        {"Stealth boost        ", "none       ", "major "},

        {"Heal on bite         ", "no         ", "yes "},

        {"\n<w>Resistances</w>\n"
         "Poison resistance    ", "           ", "immune"},

        {"Cold resistance      ", "           ", "++    "},

        {"Negative resistance  ", "           ", "+++   "},

        {"Miasma resistance    ", "           ", "immune"},

        {"Torment resistance   ", "           ", "immune"},

        {"\n<w>Transformations</w>\n"
         "Bat form (XL 3+)     ", "no         ", "yes   "},

        {"Other forms          ", "yes        ", "no    "},

        {"Berserk              ", "yes        ", "no    "}
    };

    const int highlight_col = you.vampire_alive ? 1 : 2;

    for (int y = 0; y < lines; y++)  // lines   (properties)
    {
        for (int x = 0; x < 3; x++)  // columns (states)
        {
            if (y > 0 && x == highlight_col)
                result += "<w>";
            result += column[y][x];
            if (y > 0 && x == highlight_col)
                result += "</w>";
        }
        result += "\n";
    }

    trim_string_right(result);
    return result;
}

void display_mutations()
{
    string mutation_s = describe_mutations(true);

    string extra = "";
    if (_num_part_suppressed)
        extra += "<brown>()</brown>  : Partially suppressed.\n";
    if (_num_full_suppressed)
        extra += "<darkgrey>(())</darkgrey>: Completely suppressed.\n";
    if (_num_transient)
        extra += "<magenta>[]</magenta>   : Transient mutations.";

    if (!extra.empty())
    {
        mutation_s += "\n\n\n\n";
        mutation_s += extra;
    }
    trim_string_right(mutation_s);

    auto vbox = make_shared<Box>(Widget::VERT);
    vbox->set_cross_alignment(Widget::STRETCH);

    const char *title_text = "Innate Abilities, Weirdness & Mutations";
    auto title = make_shared<Text>(formatted_string(title_text, WHITE));
    auto title_hbox = make_shared<Box>(Widget::HORZ);
    title_hbox->add_child(move(title));
    title_hbox->set_main_alignment(Widget::CENTER);
    vbox->add_child(move(title_hbox));

    auto switcher = make_shared<Switcher>();

    const string vamp_s = you.has_mutation(MUT_VAMPIRISM)
                                        ?_display_vampire_attributes()
                                        : "N/A";
    const string descs[3] =  { mutation_s, vamp_s };
    for (int i = 0; i < 2; i++)
    {
        auto scroller = make_shared<Scroller>();
        auto text = make_shared<Text>(formatted_string::parse_string(
                descs[static_cast<int>(i)]));
        text->set_wrap_text(true);
        scroller->set_child(text);
        switcher->add_child(move(scroller));
    }

    switcher->current() = 0;
    switcher->set_margin_for_sdl(20, 0, 0, 0);
    switcher->set_margin_for_crt(1, 0, 0, 0);
    switcher->expand_h = false;
    switcher->align_x = Widget::STRETCH;
#ifdef USE_TILE_LOCAL
    switcher->max_size().width = tiles.get_crt_font()->char_width()*80;
#endif
    vbox->add_child(switcher);

    auto bottom = make_shared<Text>(_vampire_Ascreen_footer(true));
    bottom->set_margin_for_sdl(20, 0, 0, 0);
    bottom->set_margin_for_crt(1, 0, 0, 0);
    if (you.has_mutation(MUT_VAMPIRISM))
        vbox->add_child(bottom);

    auto popup = make_shared<ui::Popup>(vbox);

    bool done = false;
    int lastch;
    popup->on_keydown_event([&](const KeyEvent& ev) {
        lastch = ev.key();
        if (you.has_mutation(MUT_VAMPIRISM)
            && (lastch == '!' || lastch == CK_MOUSE_CMD || lastch == '^'))
        {
            int& c = switcher->current();

            bottom->set_text(_vampire_Ascreen_footer(c));

            c = 1 - c;
#ifdef USE_TILE_WEB
            tiles.json_open_object();
            tiles.json_write_int("pane", c);
            tiles.ui_state_change("mutations", 0);
#endif
        }
        else
            done = !switcher->current_widget()->on_event(ev);
        return true;
    });

#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("mutations", mutation_s);
    if (you.has_mutation(MUT_VAMPIRISM))
        tiles.json_write_bool("vampire_alive", you.vampire_alive);
    tiles.push_ui_layout("mutations", 1);
    popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif

    ui::run_layout(move(popup), done);
}

static int _calc_mutation_amusement_value(mutation_type which_mutation)
{
    int amusement = 12 * (11 - _get_mutation_def(which_mutation).weight);

    if (MUT_GOOD(mut_data[which_mutation]))
        amusement /= 2;
    else if (MUT_BAD(mut_data[which_mutation]))
        amusement *= 2;
    // currently is only ever one of these, but maybe that'll change?

    return amusement;
}

static bool _accept_mutation(mutation_type mutat, bool ignore_weight = false)
{
    if (!_is_valid_mutation(mutat))
        return false;

    if (physiology_mutation_conflict(mutat))
        return false;

    const mutation_def& mdef = _get_mutation_def(mutat);

    if (you.get_base_mutation_level(mutat) >= mdef.levels)
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

static mutation_type _delete_random_slime_mutation()
{
    mutation_type mutat;

    while (true)
    {
        mutat = _get_random_slime_mutation();

        if (you.get_base_mutation_level(mutat) > 0)
            break;

        if (one_chance_in(500))
        {
            mutat = NUM_MUTATIONS;
            break;
        }
    }

    return mutat;
}

bool is_slime_mutation(mutation_type mut)
{
    return _mut_has_use(mut_data[mut_index[mut]], mutflag::jiyva);
}

static mutation_type _get_random_xom_mutation()
{
    mutation_type mutat = NUM_MUTATIONS;

    do
    {
        mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));

        if (one_chance_in(1000))
            return NUM_MUTATIONS;
        else if (one_chance_in(5))
            mutat = _get_mut_with_use(mutflag::xom);
    }
    while (!_accept_mutation(mutat, false));

    return mutat;
}

static mutation_type _get_random_qazlal_mutation()
{
    return _get_mut_with_use(mutflag::qazlal);
}

static mutation_type _get_random_mutation(mutation_type mutclass)
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
        if (_accept_mutation(mut, true))
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
                    die("bad mutation conflict resulution");
                }
            }
        }
    }

    return 0;
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

    // Only species that already have tails can get this one. For merfolk it
    // would only work in the water, so skip it.
    if ((!you.has_tail(false) || you.has_innate_mutation(MUT_MERTAIL))
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
    if (!species::has_bones(you.species)
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

    // Only Palentonga can go on a roll.
    if (!you.has_innate_mutation(MUT_ROLL) && mutat == MUT_ROLL)
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

    // Felid paws cap MUT_CLAWS at level 1. And octopodes have no hands.
    if ((you.has_innate_mutation(MUT_PAWS)
         || you.has_innate_mutation(MUT_TENTACLE_ARMS))
        && mutat == MUT_CLAWS)
    {
        return true;
    }

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
    if (you.is_nonliving(false) && mutat == MUT_POISON_RESISTANCE)
        return true;

    // We can't use is_useless_skill() here, since species that can still wear
    // body armour can sacrifice armour skill with Ru.
    if (species_apt(SK_ARMOUR) == UNUSABLE_SKILL
        && (mutat == MUT_DEFORMED || mutat == MUT_STURDY_FRAME))
    {
        return true;
    }

    equipment_type eq_type = EQ_NONE;

    // Mutations of the same slot conflict
    if (is_body_facet(mutat))
    {
        // Find equipment slot of attempted mutation
        for (const body_facet_def &facet : _body_facets)
            if (mutat == facet.mut)
                eq_type = facet.eq;

        if (eq_type != EQ_NONE)
        {
            for (const body_facet_def &facet : _body_facets)
            {
                if (eq_type == facet.eq
                    && mutat != facet.mut
                    && you.get_base_mutation_level(facet.mut))
                {
                    return true;
                }
            }
        }
    }

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
 * Does the player rot instead of mutating?
 * Right now this is coextensive with whether the player is unable to mutate.
 * For most undead, they will never mutate and always rot instead; vampires always mutate and never rot.
 *
 * @return true if so.
 */
bool undead_mutation_rot()
{
    return !you.can_safely_mutate();
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
    if (undead_mutation_rot())
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

    switch (which_mutation)
    {
    case RANDOM_MUTATION:
    case RANDOM_GOOD_MUTATION:
    case RANDOM_BAD_MUTATION:
    case RANDOM_CORRUPT_MUTATION:
        mutat = _get_random_mutation(which_mutation);
        break;
    case RANDOM_XOM_MUTATION:
        mutat = _get_random_xom_mutation();
        break;
    case RANDOM_SLIME_MUTATION:
        mutat = _get_random_slime_mutation();
        break;
    case RANDOM_QAZLAL_MUTATION:
        mutat = _get_random_qazlal_mutation();
        break;
    default:
        break;
    }


    if (!_is_valid_mutation(mutat))
        return false;

    // [Cha] don't allow teleportitis in sprint
    if (mutat == MUT_TELEPORT && crawl_state.game_is_sprint())
        return false;

    if (physiology_mutation_conflict(mutat))
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

    const unsigned int old_talents = your_talents(false).size();

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

        case MUT_HOOVES:
        case MUT_TALONS:
            // Hooves and talons force boots off at 3.
            if (cur_base_level >= 3 && !you.melded[EQ_BOOTS]
                && !you.wear_barding())
            {
                remove_one_equip(EQ_BOOTS, false, true);
            }
            // Recheck Ashenzari bondage in case our available slots changed.
            ash_check_bondage();
            break;

        case MUT_CLAWS:
            // Claws force gloves off at 3.
            if (cur_base_level >= 3 && !you.melded[EQ_GLOVES])
                remove_one_equip(EQ_GLOVES, false, true);
            // Recheck Ashenzari bondage in case our available slots changed.
            ash_check_bondage();
            break;

        case MUT_HORNS:
        case MUT_ANTENNAE:
            // Horns & Antennae 3 removes all headgear. Same algorithm as with
            // glove removal.

            if (cur_base_level >= 3 && !you.melded[EQ_HELMET])
                remove_one_equip(EQ_HELMET, false, true);
            // Intentional fall-through
        case MUT_BEAK:
            // Horns, beaks, and antennae force hard helmets off.
            if (you.equip[EQ_HELMET] != -1
                && is_hard_helmet(you.inv[you.equip[EQ_HELMET]])
                && !you.melded[EQ_HELMET])
            {
                remove_one_equip(EQ_HELMET, false, true);
            }
            // Recheck Ashenzari bondage in case our available slots changed.
            ash_check_bondage();
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
            invalidate_agrid(true);
            break;

        default:
            break;
        }

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
    if (crawl_state.game_is_hints()
        && your_talents(false).size() > old_talents)
    {
        learned_something_new(HINT_NEW_ABILITY_MUT);
    }
#ifdef DEBUG
    if (mutclass != MUTCLASS_INNATE) // taken care of in perma_mutate. Skipping this here avoids validation issues in doing repairs.
        validate_mutations(false);
#endif
    return true;
}

/*
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
        invalidate_agrid(true);
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

/*
 * Delete a mutation level, accepting random mutation types and checking mutation resistance.
 * This will not delete temporary or innate mutations.
 *
 * @param which_mutation    a mutation, including random
 * @param reason            the reason for deletion
 * @param failMsg           whether to message the player on failure
 * @param force_mutation    whether to try to override certain cases where the mutation would otherwise fail
 * @param god_gift          is the mutation a god gift?  Will also override certain cases.
 * @param disallow_mismatch for random mutations, do we override good/bad designations in `which_mutation`? (??)
 *
 * @return true iff a mutation was applied.
 */
bool delete_mutation(mutation_type which_mutation, const string &reason,
                     bool failMsg,
                     bool force_mutation, bool god_gift,
                     bool disallow_mismatch)
{
    god_gift |= crawl_state.is_god_acting();

    mutation_type mutat = which_mutation;

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

        if (undead_mutation_rot())
            return false;
    }

    if (which_mutation == RANDOM_MUTATION
        || which_mutation == RANDOM_XOM_MUTATION
        || which_mutation == RANDOM_GOOD_MUTATION
        || which_mutation == RANDOM_BAD_MUTATION
        || which_mutation == RANDOM_NON_SLIME_MUTATION
        || which_mutation == RANDOM_CORRUPT_MUTATION
        || which_mutation == RANDOM_QAZLAL_MUTATION)
    {
        while (true)
        {
            if (one_chance_in(1000))
                return false;

            mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));

            if (you.mutation[mutat] == 0
                && mutat != MUT_STRONG
                && mutat != MUT_CLEVER
                && mutat != MUT_AGILE
                && mutat != MUT_WEAK
                && mutat != MUT_DOPEY
                && mutat != MUT_CLUMSY)
            {
                continue;
            }

            if (which_mutation == RANDOM_NON_SLIME_MUTATION
                && is_slime_mutation(mutat))
            {
                continue;
            }

            // Check whether there is a non-innate level of `mutat` to delete
            if (you.get_base_mutation_level(mutat, false, true, true) == 0)
                continue;

            // MUT_ANTENNAE is 0, and you.attribute[] is initialized to 0.
            if (mutat && _is_appendage_mutation(mutat))
                continue;

            const mutation_def& mdef = _get_mutation_def(mutat);

            if (random2(10) >= mdef.weight && !is_slime_mutation(mutat))
                continue;

            const bool mismatch =
                (which_mutation == RANDOM_GOOD_MUTATION
                 && MUT_BAD(mdef))
                    || (which_mutation == RANDOM_BAD_MUTATION
                        && MUT_GOOD(mdef));

            if (mismatch && (disallow_mismatch || !one_chance_in(10)))
                continue;

            if (you.get_base_mutation_level(mutat, true, false, true) == 0)
                continue; // No non-transient mutations in this category to cure

            break;
        }
    }
    else if (which_mutation == RANDOM_SLIME_MUTATION)
    {
        mutat = _delete_random_slime_mutation();

        if (mutat == NUM_MUTATIONS)
            return false;
    }

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
 * @return the mutation type if succesful, otherwise NUM_MUTATIONS if it can't find a single match.
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

// Return a string describing the mutation.
// If colour is true, also add the colour annotation.
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
    else if (have_passive(passive_t::no_mp_regen) && mut == MUT_ANTIMAGIC_BITE)
        result = "Your bite disrupts the magic of your enemies.";
    else if (result.empty() && level > 0)
        result = mdef.have[level - 1];

    if (!ignore_player)
    {
        if (fully_inactive)
        {
            result = "((" + result + "))";
            ++_num_full_suppressed;
        }
        else if (partially_active)
        {
            result = "(" + result + ")";
            ++_num_part_suppressed;
        }
    }

    if (temporary)
    {
        result = "[" + result + "]";
        ++_num_transient;
    }

    if (colour)
    {
        const char* colourname = (MUT_BAD(mdef) ? "red" : "lightgrey");
        const bool permanent   = you.has_innate_mutation(mut);

        if (permanent)
        {
            const bool demonspawn = (you.species == SP_DEMONSPAWN);
            const bool extra = you.get_base_mutation_level(mut, false, true, true) > 0;

            if (fully_inactive || (mut == MUT_COLD_BLOODED && player_res_cold(false) > 0))
                colourname = "darkgrey";
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
        else if (_is_appendage_mutation(mut) && you.form == transformation::appendage)
            colourname = "green";
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
    { 0, { MUT_TALONS, MUT_TALONS, MUT_TALONS },
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
                }
            }

            ++absfacet;
        }
    }

    if (elemental > 1)
        goto try_again;

    if (cloud_producing > 1)
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
            const int mut_level = get_base_mutation_level(static_cast<mutation_type>(i), innate, temp);

            if (levels)
                result += mut_level;
            else if (mut_level > 0)
                result++;
        }
        if (you.species == SP_DEMONSPAWN
            && you.props.exists("num_sacrifice_muts"))
        {
            result -= you.props["num_sacrifice_muts"].get_int();
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
    if (you.get_mutation_level(MUT_NO_LOVE))
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
            mt = random_choose(MONS_BALRUG, MONS_REAPER,
                               MONS_LOROCYPROCA, MONS_CACODEMON,
                               MONS_HELL_BEAST);
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
                      MG_FORCE_BEH | MG_AUTOFOE).set_summoned(&you, 2, 0));

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
        if (mons_is_firewood(*mon))
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
