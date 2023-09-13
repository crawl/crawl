#pragma once

// this enum needs to align with the option values for enemy_hp_colour, see
// game_options::update_enemy_hp_colour
enum mon_dam_level_type
{
    MDAM_OKAY,
    MDAM_LIGHTLY_DAMAGED,
    MDAM_MODERATELY_DAMAGED,
    MDAM_HEAVILY_DAMAGED,
    MDAM_SEVERELY_DAMAGED,
    MDAM_ALMOST_DEAD,
    MDAM_DEAD,
};
