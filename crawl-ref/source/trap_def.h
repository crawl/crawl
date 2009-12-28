#ifndef TRAP_DEF_H
#define TRAP_DEF_H

struct trap_def
{
    coord_def pos;
    trap_type type;
    int       ammo_qty;

    dungeon_feature_type category() const;
    std::string name(description_level_type desc = DESC_PLAIN) const;
    bool is_known(const actor* act = 0) const;
    void trigger(actor& triggerer, bool flat_footed = false);
    void disarm();
    void destroy();
    void hide();
    void reveal();
    void prepare_ammo();
    bool type_has_ammo() const;
    bool active() const;
    int max_damage(const actor& act);

private:
    void message_trap_entry();
    void shoot_ammo(actor& act, bool was_known);
    item_def generate_trap_item();
    int shot_damage(actor& act);
};

#endif

