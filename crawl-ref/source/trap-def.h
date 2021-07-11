#pragma once

#include "trap-type.h"

struct trap_def
{
    /// The position of the trap on the map
    coord_def pos;
    /// The type of trap
    trap_type type;
    /// The amount of ammo remaining. For web traps, if 1, destroy on exit.
    short     ammo_qty;

    dungeon_feature_type feature() const;
    string name(description_level_type desc = DESC_PLAIN) const;
    bool is_bad_for_player() const;
    bool is_safe(actor* act = 0) const;
    void trigger(actor& triggerer);
    void destroy(bool known = false);
    void hide();
    void reveal();
    void prepare_ammo(int charges = 0);
    bool active() const;
    bool defined() const { return active(); }
};
