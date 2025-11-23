#pragma once

enum seen_context_type
{
    SC_NONE,
    SC_NEWLY_SEEN,       // Had its initial encounter message already printed this turn
    SC_ALREADY_IN_VIEW,  // Was already in sight when a see_monster interrupt was fired
    SC_ABYSS,            // Normal Abyss monster creation
    SC_ORBRUN,           // Orbrun spawns
};
