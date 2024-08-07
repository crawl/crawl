#include "AppHdr.h"
#include <map>

#include "mpr.h"
#include "species.h"

#include "branch.h"
#include "item-prop.h"
#include "items.h"
#include "mutation.h"
#include "output.h"
#include "playable.h"
#include "player.h"
#include "player-stats.h"
#include "random.h"
#include "skills.h"
#include "stringutil.h"
#include "tag-version.h"
#include "tiledoll.h"
#include "travel.h"

#include "species-data.h"

/*
 * Get the species_def for the given species type. Asserts if the species_type
 * is not less than NUM_SPECIES.
 *
 * @param species The species type.
 * @returns The species_def of that species.
 */
const species_def& get_species_def(species_type species)
{
    if (species != SP_UNKNOWN)
        ASSERT_RANGE(species, 0, NUM_SPECIES);
    return species_data.at(species);
}

namespace species
{
    /**
     * Return the name of the given species.
     * @param speci       the species to be named.
     * @param spname_type the kind of name to get: adjectival, the genus, or plain.
     * @returns the requested name, which will just be plain if no adjective
     *          or genus is defined.
     */
    string name(species_type speci, name_type spname_type)
    {
        const species_def& def = get_species_def(speci);
        if (spname_type == SPNAME_GENUS && def.genus_name)
            return def.genus_name;
        else if (spname_type == SPNAME_ADJ && def.adj_name)
            return def.adj_name;
        return def.name;
    }

    /// Does exact case-sensitive lookup of a species name
    species_type from_str(const string &species)
    {
        species_type sp;
        if (species.empty())
            return SP_UNKNOWN;

        for (int i = 0; i < NUM_SPECIES; ++i)
        {
            sp = static_cast<species_type>(i);
            if (species == name(sp))
                return sp;
        }

        return SP_UNKNOWN;
    }

    /// Does loose, non-case-sensitive lookup of a species based on a string,
    /// prioritizing non-removed species
    species_type from_str_loose(const string &species, bool initial_only)
    {
        // XX consolidate with from_str?
        string spec = lowercase_string(species);

        species_type sp = SP_UNKNOWN;
        species_type removed = SP_UNKNOWN;

        for (int i = 0; i < NUM_SPECIES; ++i)
        {
            const species_type si = static_cast<species_type>(i);
            const string sp_name = lowercase_string(name(si));

            string::size_type pos = sp_name.find(spec);
            if (pos != string::npos)
            {
                if (is_removed(si))
                {
                    // first matching removed species
                    if (removed == SP_UNKNOWN && (!initial_only || pos == 0))
                        removed = si;
                    // otherwise, we already found a matching removed species
                }
                else if (pos == 0 || !initial_only)
                {
                    sp = si;
                    // We prefer prefixes over partial matches.
                    if (pos == 0)
                        break;
                }
            }
        }
        if (sp != SP_UNKNOWN)
            return sp;
        else
            return removed;
    }


    species_type from_abbrev(const char *abbrev)
    {
        if (lowercase_string(abbrev) == "dr")
            return SP_BASE_DRACONIAN;

        for (auto& entry : species_data)
            if (lowercase_string(abbrev) == lowercase_string(entry.second.abbrev))
                return entry.first;

        return SP_UNKNOWN;
    }

    const char *get_abbrev(species_type which_species)
    {
        return get_species_def(which_species).abbrev;
    }

    bool is_elven(species_type species)
    {
        return species == SP_DEEP_ELF;
    }

    bool is_undead(species_type species)
    {
        return undead_type(species) != US_ALIVE;
    }

    bool is_draconian(species_type species)
    {
        return bool(get_species_def(species).flags & SPF_DRACONIAN);
    }

    // A random non-base draconian colour appropriate for the player.
    species_type random_draconian_colour()
    {
        species_type species;
        do {
            species =
                static_cast<species_type>(random_range(0, NUM_SPECIES - 1));
        } while (!is_draconian(species)
               || is_removed(species)
               || species == SP_BASE_DRACONIAN);

        return species;
    }

    /**
     * Where does a given species fall on the Undead Spectrum?
     *
     * @param species   The species in question.
     * @return          What class of undead the given species falls on, if any.
     */
    undead_state_type undead_type(species_type species)
    {
        return get_species_def(species).undeadness;
    }

    monster_type to_mons_species(species_type species)
    {
        return get_species_def(species).monster_species;
    }

    // XX non-draconians, unify with skin names?
    const char* scale_type(species_type species)
    {
        switch (species)
        {
            case SP_RED_DRACONIAN:
                return "fiery red";
            case SP_WHITE_DRACONIAN:
                return "icy white";
            case SP_GREEN_DRACONIAN:
                return "lurid green";
            case SP_YELLOW_DRACONIAN:
                return "golden yellow";
            case SP_GREY_DRACONIAN:
                return "dull iron-grey";
            case SP_BLACK_DRACONIAN:
                return "glossy black";
            case SP_PURPLE_DRACONIAN:
                return "rich purple";
            case SP_PALE_DRACONIAN:
                return "pale cyan-grey";
            case SP_BASE_DRACONIAN:
                return "plain brown";
            default:
                return "";
        }
    }

    monster_type dragon_form(species_type s)
    {
        switch (s)
        {
        case SP_WHITE_DRACONIAN:
            return MONS_ICE_DRAGON;
        case SP_GREEN_DRACONIAN:
            return MONS_SWAMP_DRAGON;
        case SP_YELLOW_DRACONIAN:
            return MONS_GOLDEN_DRAGON;
        case SP_GREY_DRACONIAN:
            return MONS_IRON_DRAGON;
        case SP_BLACK_DRACONIAN:
            return MONS_STORM_DRAGON;
        case SP_PURPLE_DRACONIAN:
            return MONS_QUICKSILVER_DRAGON;
        case SP_PALE_DRACONIAN:
            return MONS_STEAM_DRAGON;
        case SP_RED_DRACONIAN:
        default:
            return MONS_FIRE_DRAGON;
        }
    }

    ability_type draconian_breath(species_type species)
    {
        switch (species)
        {
        case SP_GREEN_DRACONIAN:   return ABIL_NOXIOUS_BREATH;
        case SP_RED_DRACONIAN:     return ABIL_COMBUSTION_BREATH;
        case SP_WHITE_DRACONIAN:   return ABIL_GLACIAL_BREATH;
        case SP_YELLOW_DRACONIAN:  return ABIL_CAUSTIC_BREATH;
        case SP_BLACK_DRACONIAN:   return ABIL_GALVANIC_BREATH;
        case SP_PURPLE_DRACONIAN:  return ABIL_NULLIFYING_BREATH;
        case SP_PALE_DRACONIAN:    return ABIL_STEAM_BREATH;
        case SP_GREY_DRACONIAN:    return ABIL_MUD_BREATH;
        case SP_BASE_DRACONIAN:
        default: return ABIL_NON_ABILITY;
        }
    }

    /// Does the species have (real) mutation `mut`? Not for demonspawn.
    /// @return the first xl at which the species gains the mutation, or 0 if it
    ///         does not ever gain it.
    int mutation_level(species_type species, mutation_type mut, int mut_level)
    {
        int total = 0;
        // relies on levels being in order -- I think this is safe?
        for (const auto& lum : get_species_def(species).level_up_mutations)
            if (mut == lum.mut)
            {
                total += lum.mut_level;
                if (total >= mut_level)
                    return lum.xp_level;
            }

        return 0;
    }

    const vector<string>& fake_mutations(species_type species, bool terse)
    {
        return terse ? get_species_def(species).terse_fake_mutations
                     : get_species_def(species).verbose_fake_mutations;
    }

    bool has_blood(species_type species)
    {
        return !bool(get_species_def(species).flags & SPF_NO_BLOOD);
    }

    bool has_hair(species_type species)
    {
        return !bool(get_species_def(species).flags & SPF_NO_HAIR);
    }

    bool has_bones(species_type species)
    {
        return !bool(get_species_def(species).flags & SPF_NO_BONES);
    }

    bool has_feet(species_type species)
    {
        return !bool(get_species_def(species).flags & SPF_NO_FEET);
    }

    bool has_ears(species_type species)
    {
        return !bool(get_species_def(species).flags & SPF_NO_EARS);
    }

    bool can_throw_large_rocks(species_type species)
    {
        return size(species) >= SIZE_LARGE;
    }

    bool wears_barding(species_type species)
    {
        return bool(get_species_def(species).flags & SPF_BARDING);
    }

    bool has_claws(species_type species)
    {
        return mutation_level(species, MUT_CLAWS) == 1;
    }

    bool is_nonliving(species_type species)
    {
        // XXX: move to data?
        return species == SP_GARGOYLE || species == SP_DJINNI;
    }

    bool can_swim(species_type species)
    {
        return get_species_def(species).habitat == HT_WATER;
    }

    bool likes_water(species_type species)
    {
        return can_swim(species)
               || get_species_def(species).habitat == HT_AMPHIBIOUS;
    }

    size_type size(species_type species, size_part_type psize)
    {
        const size_type size = get_species_def(species).size;
        if (psize == PSIZE_TORSO
            && bool(get_species_def(species).flags & SPF_SMALL_TORSO))
        {
            return static_cast<size_type>(static_cast<int>(size) - 1);
        }
        return size;
    }


    /** What walking-like thing does this species do?
     *
     *  @param sp what kind of species to look at
     *  @returns a "word" to which "-er" or "-ing" can be appended.
     */
    string walking_verb(species_type sp)
    {
        auto verb = get_species_def(sp).walking_verb;
        return verb ? verb : "Walk";
    }

    /** For purposes of skill/god titles, what walking-like thing does this
     *  species do?
     *
     *  @param sp what kind of species to look at
     *  @returns a "word" to which "-er" or "-ing" can be appended.
     */
    string walking_title(species_type sp)
    {
        if (sp == SP_ARMATAUR)
            return "Roll";
        return walking_verb(sp);
    }

    /**
     * What is an appropriate name for children of this species?
     *
     *  @param sp what kind of species to look at
     *  @returns something like 'kitten' or 'child'.
     */
    string child_name(species_type sp)
    {
        auto verb = get_species_def(sp).child_name;
        return verb ? verb : "Child";
    }

    /**
     * What is an appropriate name for orcs of this species?
     *
     *  @param sp what kind of species to look at
     *  @returns something with 'orc' in it.
     */
    string orc_name(species_type sp)
    {
        auto verb = get_species_def(sp).orc_name;
        return verb ? verb : "Orc";
    }

    /**
     * What message should be printed when a character of the specified species
     * prays at an altar, if not in some form?
     * To be inserted into "You %s the altar of foo."
     *
     * @param species   The species in question.
     * @return          An action to be printed when the player prays at an altar.
     *                  E.g., "coil in front of", "kneel at", etc.
     */
    string prayer_action(species_type species)
    {
      auto action = get_species_def(species).altar_action;
      return action ? action : "kneel at";
    }

    static const string shout_verbs[] = {"shout", "yell", "scream"};
    static const string felid_shout_verbs[] = {"meow", "yowl", "caterwaul"};
    static const string frog_shout_verbs[] = {"croak", "ribbit", "bellow"};
    static const string dog_shout_verbs[] = {"bark", "howl", "screech"};

    /**
     * What verb should be used to describe the species' shouting?
     * @param sp a species
     * @param screaminess a loudness level; in range [0,2]
     * @param directed with this is to be directed at another actor
     * @return A shouty kind of verb
     */
    string shout_verb(species_type sp, int screaminess, bool directed)
    {
        screaminess = max(min(screaminess,
                                static_cast<int>(sizeof(shout_verbs) - 1)), 0);
        switch (sp)
        {
        case SP_GNOLL:
            if (screaminess == 0 && directed && coinflip())
                return "growl";
            return dog_shout_verbs[screaminess];
        case SP_BARACHI:
            return frog_shout_verbs[screaminess];
        case SP_FELID:
            if (screaminess == 0 && directed)
                return "hiss"; // hiss at, not meow at
            return felid_shout_verbs[screaminess];
        default:
            return shout_verbs[screaminess];
        }
    }

    /**
     * Return an adjective or noun for the species' skin.
     * @param adj whether to provide an adjective (if true), or a noun (if false).
     * @return a non-empty string. Nouns will be pluralised if they are count nouns.
     *         Right now, plurality can be determined by `ends_with(noun, "s")`.
     */
    string skin_name(species_type species, bool adj)
    {
        // Aside from direct uses, some flavor stuff checks the strings
        // here. TODO: should some of these be species flags a la hair?
        // Also, some skin mutations should have a way of overriding these perhaps
        if (is_draconian(species) || species == SP_NAGA)
            return adj ? "scaled" : "scales";
        else if (species == SP_TENGU)
            return adj ? "feathered" : "feathers";
        else if (species == SP_FELID)
            return adj ? "furry" : "fur";
        else if (species == SP_MUMMY)
            return adj ? "bandage-wrapped" : "bandages";
        else if (species == SP_GARGOYLE)
            return adj ? "stony" : "stone";
        else
            return adj ? "fleshy" : "skin";
    }

    string arm_name(species_type species)
    {
        if (mutation_level(species, MUT_TENTACLE_ARMS))
            return "tentacle";
        else if (species == SP_FELID)
            return "leg";
        else
            return "arm";
    }

    string hand_name(species_type species)
    {
        // see also player::hand_name
        if (mutation_level(species, MUT_PAWS))
            return "paw";
        else if (mutation_level(species, MUT_TENTACLE_ARMS))
            return "tentacle";
        else if (mutation_level(species, MUT_CLAWS))
            return "claw"; // overridden for felids by first check
        else if (species == SP_COGLIN)
            return "grasper";
        else
            return "hand";
    }

    int arm_count(species_type species)
    {
        return species == SP_OCTOPODE ? 8 : 2;
    }

    equipment_type sacrificial_arm(species_type species)
    {
        // this is a bit special-case-y because the sac slot doesn't follow
        // from the enum; for 2-armed species it is the left ring (which is first),
        // but for 8-armed species it is ring 8 (which is last).
        // XX maybe swap the targeted sac hand? But this requires some painful
        // save compat
        return arm_count(species) == 2 ? EQ_LEFT_RING : EQ_RING_EIGHT;
    }

    /**
     *  Checks some species-level equipment slot constraints. Anything hard-coded
     *  per species, but not handled by a mutation should be here. See also
     *  player.cc::you_can_wear and item-use.cc::can_wear_armour for the full
     *  division of labor. This function is guaranteed to handle species ring
     *  slots.
     *
     *  @param species the species type to check
     *  @param eq the equipment slot to check
     *  @return true if the equipment slot is not used by the species; false
     *          indicates only that nothing in this check bans the slot. For
     *          example, this function does not check felid mutations.
     */
    bool bans_eq(species_type species, equipment_type eq)
    {
        const int arms = arm_count(species);
        // only handles 2 or 8
        switch (eq)
        {
        case EQ_LEFT_RING:
        case EQ_RIGHT_RING:
            return arms > 2;
        case EQ_RING_ONE:
        case EQ_RING_TWO:
        case EQ_RING_THREE:
        case EQ_RING_FOUR:
        case EQ_RING_FIVE:
        case EQ_RING_SIX:
        case EQ_RING_SEVEN:
        case EQ_RING_EIGHT:
            return arms <= 2;
        // not banned by any species
        case EQ_AMULET:
        case EQ_RING_AMULET:
        // not handled here:
        case EQ_WEAPON:
        case EQ_STAFF:
        case EQ_RINGS:
        case EQ_RINGS_PLUS: // what is this stuff
        case EQ_ALL_ARMOUR:
            return false;
        default:
            break;
        }
        // remaining should be armour only
        if (species == SP_OCTOPODE && eq != EQ_HELMET && eq != EQ_OFFHAND)
            return true;

        if (is_draconian(species) && eq == EQ_BODY_ARMOUR)
            return true;

        // for everything else that is handled by mutations, including felid
        // restrictions, see item-use.cc::can_wear_armour. (TODO: move more of the
        // code here to mutations?)
        return false;
    }

    /**
     * Get ring slots available to a species.
     * @param species the species to check
     * @param missing_hand if true, removes a designated hand from the result
     */
    vector<equipment_type> ring_slots(species_type species, bool missing_hand)
    {
        vector<equipment_type> result;
        if (you.has_mutation(MUT_NO_JEWELLERY))
            return result;

        const equipment_type missing = missing_hand
                            ? sacrificial_arm(species) : EQ_NONE;

        for (int i = EQ_FIRST_JEWELLERY; i <= EQ_LAST_JEWELLERY; i++)
        {
            const auto eq = static_cast<equipment_type>(i);
            if (eq != EQ_AMULET
                && eq != EQ_RING_AMULET
                && eq != missing
                && !bans_eq(species, eq))
            {
                result.push_back(eq);
            }
        }
        return result;
    }

    int get_exp_modifier(species_type species)
    {
        return get_species_def(species).xp_mod;
    }

    int get_hp_modifier(species_type species)
    {
        return get_species_def(species).hp_mod;
    }

    int get_mp_modifier(species_type species)
    {
        return get_species_def(species).mp_mod;
    }

    int get_wl_modifier(species_type species)
    {
        return get_species_def(species).wl_mod;
    }

    /**
     *  Does this species have (relatively) low strength?
     *  Used to generate the title for UC ghosts.
     *
     *  @param species the speciecs to check.
     *  @returns whether the starting str is lower than the starting dex.
     */
    bool has_low_str(species_type species)
    {
        return get_species_def(species).d >= get_species_def(species).s;
    }


    bool recommends_job(species_type species, job_type job)
    {
        return find(get_species_def(species).recommended_jobs.begin(),
                    get_species_def(species).recommended_jobs.end(),
                    job) != get_species_def(species).recommended_jobs.end();
    }

    bool recommends_weapon(species_type species, weapon_type wpn)
    {
        const skill_type sk =
              wpn == WPN_UNARMED ? SK_UNARMED_COMBAT :
                                   item_attack_skill(OBJ_WEAPONS, wpn);

        return find(get_species_def(species).recommended_weapons.begin(),
                    get_species_def(species).recommended_weapons.end(),
                    sk) != get_species_def(species).recommended_weapons.end();
    }

    bool is_valid(species_type species)
    {
        return 0 <= species && species < NUM_SPECIES;
    }


    // Ensure the species isn't SP_RANDOM/SP_VIABLE and it has recommended jobs
    // (old disabled species have none).
    bool is_starting_species(species_type species)
    {
        return is_valid(species)
            && !get_species_def(species).recommended_jobs.empty();
    }

    // A random valid (selectable on the new game screen) species.
    species_type random_starting_species()
    {
        const auto species = playable_species();
        return species[random2(species.size())];
    }

    bool is_removed(species_type species)
    {
    #if TAG_MAJOR_VERSION == 34
        if (species == SP_MOTTLED_DRACONIAN)
            return true;
    #endif
        // all other derived Dr are ok and don't have recommended jobs
        if (is_draconian(species))
            return false;
        if (get_species_def(species).recommended_jobs.empty())
            return true;
        return false;
    }


    /** All non-removed species, including base and derived species */
    vector<species_type> get_all_species()
    {
        vector<species_type> species;
        for (int i = 0; i < NUM_SPECIES; ++i)
        {
            const auto sp = static_cast<species_type>(i);
            if (!is_removed(sp))
                species.push_back(sp);
        }
        return species;
    }
}

int draconian_breath_uses_available()
{
    if (!species::is_draconian(you.species))
        return 0;

    if (!you.props.exists(DRACONIAN_BREATH_USES_KEY))
        return 0;

    return you.props[DRACONIAN_BREATH_USES_KEY].get_int();
}

// Attempts to gain num uses of our draconian breath weapon.
// Returns true if any were gained (ie: we were not already capped)
bool gain_draconian_breath_uses(int num)
{
    int cur = draconian_breath_uses_available();

    if (cur >= MAX_DRACONIAN_BREATH)
        return false;

    you.props[DRACONIAN_BREATH_USES_KEY] = min(MAX_DRACONIAN_BREATH, cur + num);
    return true;
}

void give_basic_mutations(species_type species)
{
    // Don't perma_mutate since that gives messages.
    for (const auto& lum : get_species_def(species).level_up_mutations)
        if (lum.xp_level == 1)
            you.mutation[lum.mut] = you.innate_mutation[lum.mut] = lum.mut_level;
}

void give_level_mutations(species_type species, int xp_level)
{
    for (const auto& lum : get_species_def(species).level_up_mutations)
        if (lum.xp_level == xp_level)
        {
            perma_mutate(lum.mut, lum.mut_level,
                         species::name(species) + " growth");
        }
}

void species_stat_init(species_type species)
{
    you.base_stats[STAT_STR] = get_species_def(species).s;
    you.base_stats[STAT_INT] = get_species_def(species).i;
    you.base_stats[STAT_DEX] = get_species_def(species).d;
}

void species_stat_gain(species_type species)
{
    const species_def& sd = get_species_def(species);
    if (sd.level_stats.size() > 0 && you.experience_level % sd.how_often == 0)
        modify_stat(*random_iterator(sd.level_stats), 1, false);
}

static void _swap_equip(equipment_type a, equipment_type b)
{
    swap(you.equip[a], you.equip[b]);
    bool tmp = you.melded[a];
    you.melded.set(a, you.melded[b]);
    you.melded.set(b, tmp);
}

/**
 * Change the player's species to something else.
 *
 * This is used primarily in wizmode, but is also used for extreme
 * cases of save compatibility (see `files.cc:_convert_obsolete_species`).
 * This does *not* check for obsoleteness -- as long as it's in
 * species_data it'll do something.
 *
 * @param sp the new species.
 */
void change_species_to(species_type sp)
{
    ASSERT(sp != SP_UNKNOWN);

    // Re-scale skill-points.
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        you.skill_points[sk] *= species_apt_factor(sk, sp)
                                / species_apt_factor(sk);
    }

    species_type old_sp = you.species;
    you.species = sp;
    you.chr_species_name = species::name(sp);
    dprf("Species change: %s -> %s", species::name(old_sp).c_str(),
        you.chr_species_name.c_str());

    // Change permanent mutations, but preserve non-permanent ones.
    uint8_t prev_muts[NUM_MUTATIONS];

    // remove all innate mutations
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (you.has_innate_mutation(static_cast<mutation_type>(i)))
        {
            you.mutation[i] -= you.innate_mutation[i] - you.sacrifices[i];
            you.innate_mutation[i] = you.sacrifices[i];
        }
        prev_muts[i] = you.mutation[i];
    }
    // add the appropriate innate mutations for the new species and xl
    give_basic_mutations(sp);
    for (int i = 2; i <= you.experience_level; ++i)
        give_level_mutations(sp, i);

    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        // TODO: why do previous non-innate mutations override innate ones?  Shouldn't this be the other way around?
        if (prev_muts[i] > you.innate_mutation[i])
            you.innate_mutation[i] = 0;
        else
            you.innate_mutation[i] -= prev_muts[i];
    }

    if (sp == SP_DEMONSPAWN)
    {
        roll_demonspawn_mutations();
        for (int i = 0; i < int(you.demonic_traits.size()); ++i)
        {
            mutation_type m = you.demonic_traits[i].mutation;

            if (you.demonic_traits[i].level_gained > you.experience_level)
                continue;

            ++you.mutation[m];
            ++you.innate_mutation[m];
        }
    }

    update_vision_range(); // for Ba, and for Ko

    // XX not general if there are ever any other options
    if ((old_sp == SP_OCTOPODE) != (sp == SP_OCTOPODE))
    {
        _swap_equip(EQ_LEFT_RING, EQ_RING_ONE);
        _swap_equip(EQ_RIGHT_RING, EQ_RING_TWO);
        // All species allow exactly one amulet.
    }

    // FIXME: this checks only for valid slots, not for suitability of the
    // item in question. This is enough to make assertions happy, though.
    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
    {
        if (you.equip[i] != -1)
        {
            if (i == EQ_WEAPON && you.inv[you.equip[i]].base_type == OBJ_WEAPONS
                && sp == SP_COGLIN)
            {
                // Coglins' wielded non-offhand weapons need names. Without
                // them, wielding a non-offhand weapon, changing species to
                // coglin, and then trying to unwield that weapon will assert.
                maybe_name_weapon(you.inv[you.equip[i]], true);
            }

            if (bool(!you_can_wear(static_cast<equipment_type>(i)))
                // XXX: This can't be caught by you_can_wear as everyone has offhand slots
                || (i == EQ_OFFHAND && you.inv[you.equip[i]].base_type == OBJ_WEAPONS
                    && !you.has_mutation(MUT_WIELD_OFFHAND)))
            {
                mprf("%s fall%s away.",
                     you.inv[you.equip[i]].name(DESC_YOUR).c_str(),
                     you.inv[you.equip[i]].quantity > 1 ? "" : "s");
                // Unwear items without the usual processing.
                you.equip[i] = -1;
                you.melded.set(i, false);
            }
        }
    }

    // Sanitize skills.
    fixup_skills();

    calc_hp();
    calc_mp();

    // The player symbol depends on species.
    update_player_symbol();
#ifdef USE_TILE
    init_player_doll();
#endif
    redraw_screen();
    update_screen();
}
