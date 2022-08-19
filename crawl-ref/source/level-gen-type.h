#pragma once

// This list contains the possible values for Options.pregeneration
enum class level_gen_type
{
    incremental, // generate levels in a stable order, catching
                 // up as needed when entering a new level
    full,        // generate all levels in advance
    classic,     // generate levels when entered; breaks seeding
};
