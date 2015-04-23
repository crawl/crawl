/**
 * @file
 * @brief Report which species, jobs and combos are playable.
 */

#include "AppHdr.h"

#include <algorithm>
#include <vector>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/writer.h>

#include "playable.h"
#include "jobs.h"
#include "newgame.h"
#include "ng-restr.h"
#include "species.h"
#include "skills.h"

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

        string species_abbr = get_species_abbrev(species_type(sp));
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

typedef rapidjson::Writer<rapidjson::StringBuffer> json_writer;

static void _species_apts(json_writer &w, species_type sp)
{
    w.StartObject();
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        const skill_type sk(static_cast<skill_type>(i));
        const int apt(species_apt(sk, sp));
        if (apt != UNUSABLE_SKILL)
        {
            w.Key(skill_name(sk));
            w.Int(species_apt(sk, sp));
        }
    }
    w.EndObject();
}

static void _species_modifiers(json_writer &w, species_type sp)
{
    w.StartObject();
    w.Key("xp");
    w.Int(species_exp_modifier(sp));
    w.Key("hp");
    w.Int(species_hp_modifier(sp));
    w.Key("mp");
    w.Int(species_mp_modifier(sp));
    w.Key("stealth");
    w.Int(species_stealth_modifier(sp));
    w.Key("mr");
    w.Int(species_mr_modifier(sp));
    w.EndObject();
}

static void _species_metadata(json_writer &w, species_type sp)
{
    w.StartObject();
    w.Key("name");
    w.String(species_name(sp));
    w.Key("abbr");
    w.String(get_species_abbrev(sp));
    w.Key("apts");
    _species_apts(w, sp);
    w.Key("modifiers");
    _species_modifiers(w, sp);
    w.EndObject();
}

static void _species_metadata_array(json_writer &w)
{
    w.StartArray();
    for (species_type sp : playable_species())
        _species_metadata(w, sp);
    w.EndArray();
}

static void _job_metadata(json_writer &w, job_type job)
{
    w.StartObject();
    w.Key("name");
    w.String(get_job_name(job));
    w.Key("abbr");
    w.String(get_job_abbrev(job));
    w.EndObject();
}

static void _job_metadata_array(json_writer &w)
{
    w.StartArray();
    for (job_type job : playable_jobs())
        _job_metadata(w, job);
    w.EndArray();
}

static void _combo_array(json_writer &w)
{
    w.StartArray();
    for (const string &combo_abbr : playable_combo_names())
        w.String(combo_abbr);
    w.EndArray();
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
 * where <modifier-name>: hp, mp, exp, stealth, mr.
 */
string playable_metadata_json()
{
    rapidjson::StringBuffer sb_metadata;
    json_writer writer(sb_metadata);
    writer.StartObject();
    writer.Key("species");
    _species_metadata_array(writer);
    writer.Key("jobs");
    _job_metadata_array(writer);
    writer.Key("combos");
    _combo_array(writer);
    writer.EndObject();
    return sb_metadata.GetString();
}
