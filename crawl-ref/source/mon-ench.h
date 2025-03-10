#pragma once

#include "enchant-type.h"
#include "externs.h"
#include "kill-category.h"
#include "mon-aura.h"

#define INFINITE_DURATION  30000
#define MAX_ENCH_DEGREE_DEFAULT  4
#define MAX_ENCH_DEGREE_ABJURATION  6

#define SIMULACRUM_POWER_KEY "simulacrum power"

class actor;
class monster;

void update_mons_cloud_ring(monster* mons);

class mon_enchant
{
public:
    enchant_type  ench;
    int           degree;   // The higher the degree, the faster the degree
                            // decays, but degrees of 1 do not decay -- they
                            // just run out when the duration does.
    int           duration, maxduration;
    kill_category who;      // Source's alignment.
    mid_t         source;   // Who set this enchantment?

    ench_aura_type  ench_is_aura; // Whether the source is a passive aura
                                  // that needs regular checking.

public:
    mon_enchant(enchant_type e = ENCH_NONE, int deg = 0,
                const actor *whose = 0,
                int dur = 0, ench_aura_type ench_is_aura = AURA_NO);

    killer_type killer() const;
    int kill_agent() const;
    actor* agent() const;

    operator string () const;
    const char *kill_category_desc(kill_category) const;
    void merge_killer(kill_category who, mid_t whos);
    void cap_degree();

    void set_duration(const monster* mons, const mon_enchant *exist);

    bool operator < (const mon_enchant &other) const
    {
        return ench < other.ench;
    }

    bool operator == (const mon_enchant &other) const
    {
        // NOTE: This does *not* check who/degree.
        return ench == other.ench;
    }

    mon_enchant &operator += (const mon_enchant &other);
    mon_enchant operator + (const mon_enchant &other) const;

private:
    int modded_speed(const monster* mons, int hdplus) const;
    int calc_duration(const monster* mons, const mon_enchant *added) const;
};

enchant_type name_to_ench(const char *name);
int summ_dur(int degree);
