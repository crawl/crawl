#ifndef PLACE_INFO_H
#define PLACE_INFO_H

class PlaceInfo
{
public:
    int level_type;     // enum level_area_type
    int branch;         // enum branch_type if LEVEL_DUNGEON; otherwise -1

    unsigned int num_visits;
    unsigned int levels_seen;

    unsigned int mon_kill_exp;
    unsigned int mon_kill_exp_avail;
    unsigned int mon_kill_num[KC_NCATEGORIES];

    int turns_total;
    int turns_explore;
    int turns_travel;
    int turns_interlevel;
    int turns_resting;
    int turns_other;

    int elapsed_total;
    int elapsed_explore;
    int elapsed_travel;
    int elapsed_interlevel;
    int elapsed_resting;
    int elapsed_other;

public:
    PlaceInfo();

    bool is_global() const;
    void make_global();

    void assert_validity() const;

    const std::string short_name() const;

    const PlaceInfo &operator += (const PlaceInfo &other);
    const PlaceInfo &operator -= (const PlaceInfo &other);
    PlaceInfo operator + (const PlaceInfo &other) const;
    PlaceInfo operator - (const PlaceInfo &other) const;
};

#endif
