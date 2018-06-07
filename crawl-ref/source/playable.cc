/**
 * @file
 * @brief Report which species, jobs and combos are playable.
 */

#include "AppHdr.h"

#include <algorithm>
#include <vector>

#include "json.h"
#include "json-wrapper.h"

#include "playable.h"
#include "jobs.h"
#include "newgame.h"
#include "ng-restr.h"
#include "species.h"
#include "skills.h"
#include "stringutil.h" // to_string on Cygwin

string combo_type::abbr() const
{
    return string(get_species_abbrev(species)) + get_job_abbrev(job);
}

template <typename Enum, typename Predicate>
vector<Enum> filter_enum(Enum max, Predicate filter)
{
    vector<Enum> things;
    for (int i = 0; i < max; ++i)
    {
        const Enum thing(static_cast<Enum>(i));
        if (filter(thing))
            things.push_back(thing);
    }
    return things;
}

vector<job_type> playable_jobs()
{
    return filter_enum(NUM_JOBS, is_starting_job);
}

vector<species_type> playable_species()
{
    return filter_enum(NUM_SPECIES, is_starting_species);
}

vector<combo_type> playable_combos()
{
    vector<combo_type> combos;
    for (int sp = 0; sp < NUM_SPECIES; ++sp)
    {
        if (!is_starting_species(species_type(sp)))
            continue;

        for (int job = 0; job < NUM_JOBS; ++job)
        {
            if (job_allowed(species_type(sp), job_type(job)))
                combos.push_back(combo_type{species_type(sp), job_type(job)});
        }
    }
    return combos;
}

template <typename Coll, typename S>
vector<string> stringify(const Coll &c, S stringify)
{
    vector<string> result;
    std::transform(c.begin(), c.end(), std::back_inserter(result), stringify);
    return result;
}

vector<string> playable_job_names()
{
    return stringify(playable_jobs(), get_job_name);
}

vector<string> playable_species_names()
{
    return stringify(playable_species(),
                     [](species_type sp) { return species_name(sp); });
}

vector<string> playable_combo_names()
{
    return stringify(playable_combos(),
                     [](combo_type c) { return c.abbr(); });
}

// JSON output:

static JsonNode *_species_apts(species_type sp)
{
    JsonNode *apts(json_mkobject());
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        const skill_type sk(static_cast<skill_type>(i));
        const int apt(species_apt(sk, sp));
        if (apt != UNUSABLE_SKILL)
            json_append_member(apts, skill_name(sk), json_mknumber(apt));
    }
    return apts;
}

static JsonNode *_species_modifiers(species_type sp)
{
    JsonNode *modifiers(json_mkobject());
    json_append_member(modifiers, "xp", json_mknumber(species_exp_modifier(sp)));
    json_append_member(modifiers, "hp", json_mknumber(species_hp_modifier(sp)));
    json_append_member(modifiers, "mp", json_mknumber(species_mp_modifier(sp)));
    json_append_member(modifiers, "mr", json_mknumber(species_mr_modifier(sp)));
    return modifiers;
}

static JsonNode *_species_metadata(species_type sp,
                                   species_type derives = SP_UNKNOWN)
{
    JsonNode *species(json_mkobject());
    json_append_member(species, "name", json_mkstring(species_name(sp).c_str()));
    json_append_member(species, "abbr", json_mkstring(get_species_abbrev(sp)));
    if (derives != SP_UNKNOWN)
    {
        json_append_member(species, "derives",
                           json_mkstring(species_name(derives).c_str()));
    }
    json_append_member(species, "apts", _species_apts(sp));
    json_append_member(species, "modifiers", _species_modifiers(sp));
    return species;
}

static JsonNode *_species_metadata_array()
{
    JsonNode *species(json_mkarray());
    for (species_type sp : playable_species())
    {
        json_append_element(species, _species_metadata(sp));
        if (sp == SP_BASE_DRACONIAN)
        {
            for (int drac = SP_FIRST_NONBASE_DRACONIAN;
                 drac <= SP_LAST_NONBASE_DRACONIAN; ++drac)
            {
                json_append_element(species,
                                    _species_metadata(species_type(drac),
                                                      SP_BASE_DRACONIAN));
            }
        }
    }
    return species;
}

static JsonNode *_job_metadata(job_type job)
{
    JsonNode *job_json(json_mkobject());
    json_append_member(job_json, "name", json_mkstring(get_job_name(job)));
    json_append_member(job_json, "abbr", json_mkstring(get_job_abbrev(job)));
    return job_json;
}

static JsonNode *_job_metadata_array()
{
    JsonNode *array(json_mkarray());
    for (job_type job : playable_jobs())
        json_append_element(array, _job_metadata(job));
    return array;
}

static JsonNode *_combo_array()
{
    JsonNode *array(json_mkarray());
    for (const string &combo_abbr : playable_combo_names())
        json_append_element(array, json_mkstring(combo_abbr.c_str()));
    return array;
}

/*! @brief Returns a JSON object (encoded as a string) of the form
 *         @code
 *           { "species": [...], "jobs": [...], "combos": [...] }
 *         @endcode
 *
 * Species are objects of the form:
 * @code
 *   { "name": "Vine Stalker", "abbr": "VS", "apts": {...}, "modifiers": {...} }
 * @endcode
 *
 * Jobs are objects of the form:
 * @code
 *   { "name": "Fire Elementalist", "abbr": "FE" }
 * @endcode
 *
 * Combos is an array of combo abbreviation strings.
 *
 * Aptitudes (apts) is an object of the form:
 * @code
 *   { "<skill-name>": <aptitude> }
 * @endcode
 *
 * Modifiers is an object of the form:
 * @code
 *   { "<modifier-name>": <modifier> }
 * @endcode
 * where <modifier-name>: hp, mp, exp, mr.
 */
string playable_metadata_json()
{
    JsonWrapper json(json_mkobject());
    json_append_member(json.node, "species", _species_metadata_array());
    json_append_member(json.node, "jobs", _job_metadata_array());
    json_append_member(json.node, "combos", _combo_array());
    return json.to_string();
}
