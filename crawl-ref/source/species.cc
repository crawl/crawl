#include "AppHdr.h"
#include <map>

#include "species.h"

#include "mon-enum.h"
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

int species_has_claws(species_type species, bool mut_level)
{
    if (species == SP_TROLL)
        return 3;

    if (species == SP_GHOUL)
        return 1;

    // Felid claws don't count as a claws mutation.  The claws mutation
    // does only hands, not paws.
    if (species == SP_FELID && !mut_level)
        return 1;

    return 0;
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

bool species_is_unbreathing(species_type species)
{
    return species == SP_GREY_DRACONIAN || species == SP_GARGOYLE
           || species_is_undead(species);
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

monster_type player_species_to_mons_species(species_type species)
{
    return _species_def(species).monster_species;
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

int species_stealth_modifier(species_type species)
{
    return _species_def(species).stealth_mod;
}

int species_mr_modifier(species_type species)
{
    return _species_def(species).mr_mod;
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
    if (you.experience_level % sd.how_often != 0)
        return;

    vector<stat_type> avail_stats;
    if (sd.gain_s)
        avail_stats.push_back(STAT_STR);
    if (sd.gain_i)
        avail_stats.push_back(STAT_INT);
    if (sd.gain_d)
        avail_stats.push_back(STAT_DEX);
    modify_stat(*random_iterator(avail_stats), 1, false, "level gain");
}
