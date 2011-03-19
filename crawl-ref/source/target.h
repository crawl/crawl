#ifndef TARGET_H
#define TARGET_H

#include "beam.h"

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
                    int exp_min = 0, int exp_max = 0, bool wall_ok = false);
    bool set_aim(coord_def a);
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
private:
    int range2;
    // assumes exp_map is valid only if >0, so let's keep it private
    int exp_range_min, exp_range_max;
    explosion_map exp_map_min, exp_map_max;
    bool affects_walls;
};

class targetter_reach : public targetter
{
public:
    targetter_reach(const actor* act, reach_type ran = REACH_NONE);
    reach_type range;
    bool valid_aim(coord_def a);
    aff_type is_affected(coord_def loc);
};

#endif
