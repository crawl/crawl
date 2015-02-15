/**
 * @file
 * @brief Report which species, jobs and combos are playable.
 */

#include "AppHdr.h"

#include <algorithm>
#include <vector>

#include "playable.h"
#include "jobs.h"
#include "newgame.h"
#include "ng-restr.h"
#include "species.h"

string combo_type::abbr() const
{
    return string(get_species_abbrev(species)) + get_job_abbrev(job);
}

template <typename Enum, typename Predicate>
auto filter_enum(Enum max, Predicate filter) -> vector<decltype(max)>
{
    vector<decltype(max)> things;
    for (int i = 0; i < max; ++i)
    {
        const decltype(max) thing(static_cast<decltype(max)>(i));
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
