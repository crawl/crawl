#include "AppHdr.h"

#include "mon_resist_def.h"

#include "mon-enum.h"

/////////////////////////////////////////////////////////////////////////
// mon_resist_def

mon_resist_def::mon_resist_def()
    : elec(0), poison(0), fire(0), steam(0), cold(0), hellfire(0), acid(0),
      asphyx(false), sticky_flame(false), rotting(false), pierce(0), slice(0),
      bludgeon(0)
{
}

short mon_resist_def::get_default_res_level(int resist, short level) const
{
    if (level != -100)
        return level;
    switch (resist)
    {
    case MR_RES_STEAM:
    case MR_RES_ACID:
        return 3;
    case MR_RES_ELEC:
        return 2;
    default:
        return 1;
    }
}

mon_resist_def::mon_resist_def(int flags, short level)
    : elec(0), poison(0), fire(0), steam(0), cold(0), hellfire(0), acid(0),
      asphyx(false), sticky_flame(false), rotting(false), pierce(0), slice(0),
      bludgeon(0)
{
    for (int i = 0; i < 32; ++i)
    {
        const short nl = get_default_res_level(1 << i, level);
        switch (flags & (1 << i))
        {
        // resistances
        case MR_RES_STEAM:    steam      =    3; break;
        case MR_RES_ELEC:     elec       =   nl; break;
        case MR_RES_POISON:   poison     =   nl; break;
        case MR_RES_FIRE:     fire       =   nl; break;
        case MR_RES_HELLFIRE: hellfire   =   nl; break;
        case MR_RES_COLD:     cold       =   nl; break;
        case MR_RES_ASPHYX:   asphyx     = true; break;
        case MR_RES_ACID:     acid       =   nl; break;

        // vulnerabilities
        case MR_VUL_ELEC:     elec       = -nl;  break;
        case MR_VUL_POISON:   poison     = -nl;  break;
        case MR_VUL_FIRE:     fire       = -nl;  break;
        case MR_VUL_COLD:     cold       = -nl;  break;

        // resistance to certain damage types
        case MR_RES_PIERCE:   pierce     = nl;   break;
        case MR_RES_SLICE:    slice      = nl;   break;
        case MR_RES_BLUDGEON: bludgeon   = nl;   break;

        // vulnerability to certain damage types
        case MR_VUL_PIERCE:   pierce     = -nl;  break;
        case MR_VUL_SLICE:    slice      = -nl;  break;
        case MR_VUL_BLUDGEON: bludgeon   = -nl;  break;

        case MR_RES_STICKY_FLAME: sticky_flame = true; break;
        case MR_RES_ROTTING:      rotting      = true; break;

        default: break;
        }
    }
}

const mon_resist_def &mon_resist_def::operator |= (const mon_resist_def &o)
{
    elec        += o.elec;
    poison      += o.poison;
    fire        += o.fire;
    cold        += o.cold;
    hellfire    += o.hellfire;
    asphyx       = asphyx       || o.asphyx;
    acid        += o.acid;
    pierce      += o.pierce;
    slice       += o.slice;
    bludgeon    += o.bludgeon;
    sticky_flame = sticky_flame || o.sticky_flame;
    rotting      = rotting      || o.rotting;
    return (*this);
}

mon_resist_def mon_resist_def::operator | (const mon_resist_def &o) const
{
    mon_resist_def c(*this);
    return (c |= o);
}

short mon_resist_def::get_resist_level(mon_resist_flags res_type) const
{
    switch (res_type)
    {
    case MR_RES_ELEC:    return elec;
    case MR_RES_POISON:  return poison;
    case MR_RES_FIRE:    return fire;
    case MR_RES_STEAM:   return steam;
    case MR_RES_COLD:    return cold;
    case MR_RES_ACID:    return acid;
    case MR_RES_ROTTING: return rotting;
    default:             return (0);
    }
}
