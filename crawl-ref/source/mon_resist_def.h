#ifndef MON_RESIST_DEF_H
#define MON_RESIST_DEF_H

#include "mon-enum.h"

struct mon_resist_def
{
    // All values are actually saved as single-bytes, so practical
    // range is -128 - 127, and the game only distinguishes values in
    // the range -1 to 3.

    short elec;
    short poison;
    short fire;
    short steam;
    short cold;
    short hellfire;
    short acid;

    bool asphyx;
    bool sticky_flame;
    bool rotting;

    // Physical damage resists (currently unused)
    short pierce;
    short slice;
    short bludgeon;

    mon_resist_def();
    mon_resist_def(int flags, short level = -100);

    mon_resist_def operator | (const mon_resist_def &other) const;
    const mon_resist_def &operator |= (const mon_resist_def &other);

    short get_resist_level(mon_resist_flags res_type) const;

private:
    short get_default_res_level(int resist, short level) const;
};

inline mon_resist_def operator | (int a, const mon_resist_def &b)
{
    return (mon_resist_def(a) | b);
}

typedef mon_resist_def mrd;

#endif
