#pragma once

// Be sure to change activity_interrupt_names in delay.cc to match!
enum class activity_interrupt
{
    force = 0,          // Forcibly kills any activity that can be
                        // interrupted. Delays that can have gameplay
                        // implications should explicitly avoid this in some
                        // fashion.
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
    sense_monster,      // Any non-hit event (detection as well as things like
                        // door opening) that reveals the presence of an unseen
                        // monster.
    mimic,
    ally_attacked,      // A permanent player ally was attacked by something
                        // out of the player's LoS

    // Always the last.
    COUNT
};
constexpr int NUM_ACTIVITY_INTERRUPTS
    = static_cast<int>(activity_interrupt::COUNT);
