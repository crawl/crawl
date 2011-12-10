#ifndef TARGET_H
#define TARGET_H

#include "beam.h"
#include "mon-info.h"

enum aff_type // sign and non-zeroness matters
{
    AFF_TRACER = -1,
    AFF_NO      = 0,
    AFF_MAYBE   = 1, // can possibly affect
    AFF_YES,         // intended/likely to affect
    // If you want to extend this to pass the probability somehow, feel free to,
    // just keep AFF_YES the minimal "bright" value.
};

class targetter
{
public:
    virtual ~targetter() {};

    coord_def origin;
    coord_def aim;
    const actor* agent;
    std::string why_not;

    virtual bool set_aim(coord_def a);
    virtual bool valid_aim(coord_def a) = 0;

    virtual aff_type is_affected(coord_def loc) = 0;
};

class targetter_beam : public targetter
{
public:
    targetter_beam(const actor *act, int range, beam_type flavour, bool stop);
    bolt beam;
    bool set_aim(coord_def a);
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
};

class targetter_view : public targetter
{
public:
    targetter_view();
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
};

class targetter_smite : public targetter
{
public:
    targetter_smite(const actor *act, int range = LOS_RADIUS,
                    int exp_min = 0, int exp_max = 0, bool wall_ok = false,
                    bool (*affects_pos_func)(const coord_def &) = 0);
    bool set_aim(coord_def a);
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
private:
    int range2;
    // assumes exp_map is valid only if >0, so let's keep it private
    int exp_range_min, exp_range_max;
    explosion_map exp_map_min, exp_map_max;
    bool affects_walls;
    bool (*affects_pos)(const coord_def &);
};

class targetter_reach : public targetter
{
public:
    targetter_reach(const actor* act, reach_type ran = REACH_NONE);
    reach_type range;
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
};

class targetter_cloud : public targetter
{
public:
    targetter_cloud(const actor* act, int range = LOS_RADIUS,
                    int count_min = 8, int count_max = 10);
    bool set_aim(coord_def a);
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
    int range2;
    int cnt_min, cnt_max;
    std::map<coord_def, aff_type> seen;
    std::vector<std::vector<coord_def> > queue;
};

class targetter_splash : public targetter
{
public:
    targetter_splash(const actor *act);
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
private:
    bool anyone_there(coord_def loc);
};

class targetter_los : public targetter
{
public:
    targetter_los(const actor *act, los_type los = LOS_DEFAULT,
                  int range = LOS_RADIUS, int range_max = 0);
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
private:
    los_type los;
    int range2, range_max2;
};
#endif
