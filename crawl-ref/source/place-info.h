#ifndef PLACE_INFO_H
#define PLACE_INFO_H

class PlaceInfo
{
public:
    int level_type;     // enum level_area_type
    int branch;         // enum branch_type if LEVEL_DUNGEON; otherwise -1

    unsigned long num_visits;
    unsigned long levels_seen;

    unsigned long mon_kill_exp;
    unsigned long mon_kill_exp_avail;
    unsigned long mon_kill_num[KC_NCATEGORIES];

    long turns_total;
    long turns_explore;
    long turns_travel;
    long turns_interlevel;
    long turns_resting;
    long turns_other;

    long elapsed_total;
    long elapsed_explore;
    long elapsed_travel;
    long elapsed_interlevel;
    long elapsed_resting;
    long elapsed_other;
    
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

