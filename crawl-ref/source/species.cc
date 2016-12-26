#include "AppHdr.h"
#include <map>

#include "species.h"

#include "itemprop.h"
#include "mon-enum.h"
#include "mutation.h"
#include "player.h"
#include "player-stats.h"
#include "random.h"
#include "stringutil.h"

#include "species-data.h"

static const species_def& _species_def(species_type species)
{
    if (species != SP_UNKNOWN)
        ASSERT_RANGE(species, 0, NUM_SPECIES);
    return species_data.at(species);
}

const char *get_species_abbrev(species_type which_species)
{
    return _species_def(which_species).abbrev;
}

// Needed for debug.cc and hiscores.cc.
species_type get_species_by_abbrev(const char *abbrev)
{
    if (lowercase_string(abbrev) == "dr")
        return SP_BASE_DRACONIAN;

    for (auto& entry : species_data)
        if (lowercase_string(abbrev) == lowercase_string(entry.second.abbrev))
            return entry.first;

    return SP_UNKNOWN;
}

// Does a case-sensitive lookup of the species name supplied.
species_type str_to_species(const string &species)
{
    species_type sp;
    if (species.empty())
        return SP_UNKNOWN;

    for (int i = 0; i < NUM_SPECIES; ++i)
    {
        sp = static_cast<species_type>(i);
        if (species == species_name(sp))
            return sp;
    }

    return SP_UNKNOWN;
}

/**
 * Return the name of the given species.
 * @param speci       the species to be named.
 * @param spname_type the kind of name to get: adjectival, the genus, or plain.
 * @returns the requested name, which will just be plain if no adjective
 *          or genus is defined.
 */
string species_name(species_type speci, species_name_type spname_type)
{
    const species_def& def = _species_def(speci);
    if (spname_type == SPNAME_GENUS && def.genus_name)
        return def.genus_name;
    else if (spname_type == SPNAME_ADJ && def.adj_name)
        return def.adj_name;
    return def.name;
}

/** What walking-like thing does this species do?
 *
 *  @param sp what kind of species to look at
 *  @returns a "word" to which "-er" or "-ing" can be appended.
 */
string species_walking_verb(species_type sp)
{
    switch (sp)
    {
    case SP_NAGA:
        return "Slid";
    case SP_TENGU:
        return "Glid";
    case SP_OCTOPODE:
        return "Wriggl";
    case SP_VINE_STALKER:
        return "Stalk";
    default:
        return "Walk";
    }
}

/**
 * Where does a given species fall on the Undead Spectrum?
 *
 * @param species   The species in question.
 * @return          What class of undead the given species falls on, if any.
 */
undead_state_type species_undead_type(species_type species)
{
    return _species_def(species).undeadness;
}

/**
 * Is a given species undead?
 *
 * @param species   The species in question.
 * @return          Whether that species is undead.
 */
bool species_is_undead(species_type species)
{
    return species_undead_type(species) != US_ALIVE;
}

bool species_can_swim(species_type species)
{
    return _species_def(species).habitat == HT_WATER;
}

bool species_likes_water(species_type species)
{
    return species_can_swim(species)
           || _species_def(species).habitat == HT_AMPHIBIOUS;
}

bool species_likes_lava(species_type species)
{
    return _species_def(species).habitat == HT_AMPHIBIOUS_LAVA;
}

bool species_can_throw_large_rocks(species_type species)
{
    return species_size(species) >= SIZE_LARGE;
}

bool species_is_elven(species_type species)
{
    return bool(_species_def(species).flags & SPF_ELVEN);
}

bool species_is_draconian(species_type species)
{
    return bool(_species_def(species).flags & SPF_DRACONIAN);
}

bool species_is_orcish(species_type species)
{
    return bool(_species_def(species).flags & SPF_ORCISH);
}

bool species_has_hair(species_type species)
{
    return !bool(_species_def(species).flags & (SPF_NO_HAIR | SPF_DRACONIAN));
}

size_type species_size(species_type species, size_part_type psize)
{
    const size_type size = _species_def(species).size;
    if (psize == PSIZE_TORSO
        && bool(_species_def(species).flags & SPF_SMALL_TORSO))
    {
        return static_cast<size_type>(static_cast<int>(size) - 1);
    }
    return size;
}

bool species_recommends_job(species_type species, job_type job)
{
    return find(_species_def(species).recommended_jobs.begin(),
                _species_def(species).recommended_jobs.end(),
                job) != _species_def(species).recommended_jobs.end();
}

bool species_recommends_weapon(species_type species, weapon_type wpn)
{
    const skill_type sk =
          wpn == WPN_THROWN  ? SK_THROWING :
          wpn == WPN_UNARMED ? SK_UNARMED_COMBAT :
                               item_attack_skill(OBJ_WEAPONS, wpn);

    return find(_species_def(species).recommended_weapons.begin(),
                _species_def(species).recommended_weapons.end(),
                sk) != _species_def(species).recommended_weapons.end();
}

monster_type player_species_to_mons_species(species_type species)
{
    return _species_def(species).monster_species;
}

const vector<string>& fake_mutations(species_type species, bool terse)
{
    return terse ? _species_def(species).terse_fake_mutations
                 : _species_def(species).verbose_fake_mutations;
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
string species_prayer_action(species_type species)
{
    switch (species)
    {
        case SP_NAGA:
            return "coil in front of";
        case SP_OCTOPODE:
            return "curl up in front of";
        case SP_FELID:
            // < TGWi> you curl up on the altar and go to sleep
            return "sit before";
        default:
            return "kneel at";
    }
}

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

monster_type dragon_form_dragon_type()
{
    switch (you.species)
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
    ASSERT(species_is_draconian(species));
    switch (species)
    {
    case SP_GREEN_DRACONIAN:   return ABIL_BREATHE_MEPHITIC;
    case SP_RED_DRACONIAN:     return ABIL_BREATHE_FIRE;
    case SP_WHITE_DRACONIAN:   return ABIL_BREATHE_FROST;
    case SP_YELLOW_DRACONIAN:  return ABIL_BREATHE_ACID;
    case SP_BLACK_DRACONIAN:   return ABIL_BREATHE_LIGHTNING;
    case SP_PURPLE_DRACONIAN:  return ABIL_BREATHE_POWER;
    case SP_PALE_DRACONIAN:    return ABIL_BREATHE_STEAM;
    case SP_BASE_DRACONIAN: case SP_GREY_DRACONIAN:
    default: return ABIL_NON_ABILITY;
    }
}

bool species_is_unbreathing(species_type species)
{
    return any_of(_species_def(species).level_up_mutations.begin(),
                  _species_def(species).level_up_mutations.end(),
                  [](level_up_mutation lum)
                    { return lum.mut == MUT_UNBREATHING;});
}

bool species_has_claws(species_type species)
{
    return any_of(_species_def(species).level_up_mutations.begin(),
                  _species_def(species).level_up_mutations.end(),
                  [](level_up_mutation lum) { return lum.mut == MUT_CLAWS
                                                     && lum.xp_level == 1; });
}

void give_basic_mutations(species_type species)
{
    // Don't perma_mutate since that gives messages.
    for (const auto& lum : _species_def(species).level_up_mutations)
        if (lum.xp_level == 1)
            you.mutation[lum.mut] = you.innate_mutation[lum.mut] = lum.mut_level;
}

void give_level_mutations(species_type species, int xp_level)
{
    for (const auto& lum : _species_def(species).level_up_mutations)
        if (lum.xp_level == xp_level)
        {
            perma_mutate(lum.mut, lum.mut_level,
                         species_name(species) + " growth");
        }
}

int species_exp_modifier(species_type species)
{
    return _species_def(species).xp_mod;
}

int species_hp_modifier(species_type species)
{
    return _species_def(species).hp_mod;
}

int species_mp_modifier(species_type species)
{
    return _species_def(species).mp_mod;
}

int species_mr_modifier(species_type species)
{
    return _species_def(species).mr_mod;
}

/**
 *  Does this species have (relatively) low strength?
 *  Used to generate the title for UC ghosts.
 *
 *  @param species the speciecs to check.
 *  @returns whether the starting str is lower than the starting dex.
 */
bool species_has_low_str(species_type species)
{
    return _species_def(species).d >= _species_def(species).s;
}

void species_stat_init(species_type species)
{
    you.base_stats[STAT_STR] = _species_def(species).s;
    you.base_stats[STAT_INT] = _species_def(species).i;
    you.base_stats[STAT_DEX] = _species_def(species).d;
}

void species_stat_gain(species_type species)
{
    const species_def& sd = _species_def(species);
    if (you.experience_level % sd.how_often == 0)
        modify_stat(*random_iterator(sd.level_stats), 1, false);
}
