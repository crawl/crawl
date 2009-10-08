#ifndef MON_LOS_H
/*
 *  File:       mon-los.cc
 *  Summary:    Monster line-of-sight.
 */

#define MON_LOS_H

#define _monster_los_LSIZE (2 * LOS_RADIUS + 1)

// This class can be used to fill the entire surroundings (los_range)
// of a monster or other position with seen/unseen values, so as to be able
// to compare several positions within this range.
class monster_los
{
public:
    monster_los();
    virtual ~monster_los();

    // public methods
    void set_monster(monsters *mon);
    void set_los_centre(int x, int y);
    void set_los_centre(const coord_def& p) { this->set_los_centre(p.x, p.y); }
    void set_los_range(int r);
    void fill_los_field(void);
    bool in_sight(int x, int y);
    bool in_sight(const coord_def& p) { return this->in_sight(p.x, p.y); }

protected:
    // protected methods
    coord_def pos_to_index(coord_def &p);
    coord_def index_to_pos(coord_def &i);

    void set_los_value(int x, int y, bool blocked, bool override = false);
    int get_los_value(int x, int y);
    bool is_blocked(int x, int y);
    bool is_unknown(int x, int y);

    void check_los_beam(int dx, int dy);

    // The (fixed) size of the array.
    static const int LSIZE;

    static const int L_VISIBLE;
    static const int L_UNKNOWN;
    static const int L_BLOCKED;

    // The centre of our los field.
    int gridx, gridy;

    // Habitat checks etc. should be done for this monster.
    // Usually the monster whose LOS we're trying to calculate
    // (if mon->x == gridx, mon->y == gridy).
    // Else, any monster trying to move around within this los field.
    monsters *mons;

    // Range may never be greater than LOS_RADIUS!
    int range;

    // The array to store the LOS values.
    int los_field[_monster_los_LSIZE][_monster_los_LSIZE];
};

#endif
