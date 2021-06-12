#pragma once

// Be sure to change activity_interrupt_names in delay.cc to match!
enum class activity_interrupt
{
    force = 0,          // Forcibly kills any activity that can be
                        // interrupted.
    keypress,           // Not currently used
    full_hp,            // Player is fully healed
    full_mp,            // Player has recovered all mp
    ancestor_hp,        // Player's ancestor is fully healed
    message,            // Message was displayed
    hp_loss,
    stat_change,
    see_monster,
    monster_attacks,
    teleport,
    hit_monster,        // Player hit invis monster during travel/explore.
    sense_monster,
    mimic,

    // Always the last.
    COUNT
};
constexpr int NUM_ACTIVITY_INTERRUPTS
    = static_cast<int>(activity_interrupt::COUNT);
