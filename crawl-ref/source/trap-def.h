#ifndef TRAP_DEF_H
#define TRAP_DEF_H

struct trap_def
{
    /// The position of the trap on the map
    coord_def pos;
    /// The type of trap
    trap_type type;
    /// The amount of ammo remaining. For web traps, if 1, destroy on exit.
    short     ammo_qty;
    /// How difficult the trap is to detect
    uint8_t   skill_rnd;

    dungeon_feature_type category() const;
    string name(description_level_type desc = DESC_PLAIN) const;
    bool is_known(const actor* act = 0) const;
    bool is_safe(actor* act = 0) const;
    void trigger(actor& triggerer);
    void destroy(bool known = false);
    void hide();
    void reveal();
    void prepare_ammo(int charges = 0);
    bool type_has_ammo() const;
    bool active() const;
    bool defined() const { return active(); }
    int max_damage(const actor& act);
    int to_hit_bonus();

private:
    void shoot_ammo(actor& act, bool was_known);
    item_def generate_trap_item();
    int shot_damage(actor& act);
};

#endif
