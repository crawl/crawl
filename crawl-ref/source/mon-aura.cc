#include "AppHdr.h"

#include "mon-aura.h"

#include "act-iter.h"
#include "coord.h"
#include "fprop.h"
#include "losglobal.h"
#include "monster.h"
#include "mon-ench.h"

struct aura_data
{
    enchant_type ench_type;
    int base_duration;
    bool is_hostile;
    duration_type dur_type;
    string player_key;
    function<bool(const actor& targ, mid_t source)> valid_target;
    function<void(const monster* source)> player_msg;
    bool adjacent_only;

    aura_data(enchant_type _ench_type,
              int _base_duration, bool _is_hostile,
              duration_type _dur_type = NUM_DURATIONS,
              string _player_key = "",
              function<bool(const actor& targ, mid_t source)> _valid_target = nullptr,
              function<void(const monster* source)> _player_msg = nullptr,
              bool _adjacent_only = false):
              ench_type(_ench_type), base_duration(_base_duration),
              is_hostile(_is_hostile),
              dur_type(_dur_type), player_key(_player_key),
              valid_target(_valid_target), player_msg(_player_msg),
              adjacent_only(_adjacent_only)
              {}
};

static const vector<pair<monster_type, aura_data>> mon_auras =
{
    {MONS_TORPOR_SNAIL,
        {ENCH_SLOW, 1, true, DUR_SLOW, TORPOR_SLOWED_KEY,
         [](const actor& targ, mid_t) { return !targ.stasis();},
         [](const monster* source)
            {  mprf("Being near %s leaves you feeling lethargic.",
                        source->name(DESC_THE).c_str());
            }
        }
    },

    {MONS_OPHAN,
        {ENCH_NONE, 1, true, DUR_SENTINEL_MARK, OPHAN_MARK_KEY,
         nullptr,
         [](const monster* source)
            {  mprf("%s gaze reveals you to all!",
                        source->name(DESC_ITS).c_str());
            }
        },
    },

    {MONS_MARTYRED_SHADE,
        {ENCH_INJURY_BOND, 30, false}},

    {MONS_POLTERGUARDIAN,
        {ENCH_DEFLECT_MISSILES, 1, false}},

    {MONS_GLOWING_ORANGE_BRAIN,
        {ENCH_EMPOWERED_SPELLS, 1, false,
         NUM_DURATIONS, "",
         [](const actor& targ, mid_t) { return targ.antimagic_susceptible() ;}
        }
    },

    {MONS_APIS,
        {ENCH_DOUBLED_VIGOUR, 1, false,
         NUM_DURATIONS, "",
         [](const actor& targ, mid_t) { return targ.type != MONS_APIS ;}
        }
    },

    {MONS_IRONBOUND_BEASTMASTER,
        {ENCH_HASTE, 1, false,
         NUM_DURATIONS, "",
         [](const actor& targ, mid_t) { return (targ.holiness() & MH_NATURAL)
         && targ.is_monster() && (mons_intel(*targ.as_monster()) != I_HUMAN) ;}
        }
    },

    {MONS_PHALANX_BEETLE,
        {ENCH_PHALANX_BARRIER, 1, false, DUR_PHALANX_BARRIER, PHALANX_BARRIER_KEY,
         [](const actor& targ, mid_t source)
            { return targ.mid == monster_by_mid(source)->summoner;},
        nullptr, true
        }
    },
};

static const aura_data& _get_aura_for(const monster& mon)
{
    const monster_type mtype = (mon.type == MONS_BOUND_SOUL ? mon.base_monster
                                                            : mon.type);

    for (const auto& entry : mon_auras)
    {
        if (entry.first == mtype)
            return entry.second;
    }

    die("no aura found for %s", mon.name(DESC_THE).c_str());
}

static const aura_data& _get_aura_from_key(string player_key)
{
    for (const auto& entry : mon_auras)
    {
        if (entry.second.player_key == player_key)
            return entry.second;
    }

    die("aura %s not found", player_key.c_str());
}

bool mons_has_aura(const monster& mon)
{
    if (mon.type == MONS_BOUND_SOUL)
        return mons_class_flag(mon.base_monster, M_HAS_AURA);
    else
    {
        return mons_class_flag(mon.type, M_HAS_AURA)
               && !mon.props.exists(KIKU_WRETCH_KEY);
    }
}

bool mons_has_aura_of_type(const monster& mon, enchant_type type)
{
    if (!mons_has_aura(mon))
        return false;

    const monster_type mtype = (mon.type == MONS_BOUND_SOUL ? mon.base_monster
                                                            : mon.type);

    for (const auto& entry : mon_auras)
    {
        if (entry.second.ench_type == type && entry.first == mtype)
            return true;
    }

    return false;
}

bool mons_has_aura_of_type(const monster& mon, duration_type type)
{
    if (!mons_has_aura(mon))
        return false;

    const monster_type mtype = (mon.type == MONS_BOUND_SOUL ? mon.base_monster
                                                            : mon.type);

    for (const auto& entry : mon_auras)
    {
        if (entry.second.dur_type == type && entry.first == mtype)
            return true;
    }

    return false;
}

// Accounting for source attitude and other aura properties, could this aura
// affect a given actor?
static bool _aura_could_affect(const aura_data& aura,
                               const coord_def& source_pos,
                               mid_t source_mid,
                               mon_attitude_type source_attitude,
                               const actor& victim)
{
    if (aura.adjacent_only && !adjacent(source_pos, victim.pos()))
        return false;

    // Auras do not apply to self
    if (victim.is_monster() && victim.mid == source_mid)
        return false;

    // Is the victim the right alignment?
    if (mons_atts_aligned(source_attitude, victim.temp_attitude()) == aura.is_hostile)
        return false;

    // Is the aura something that should affect firewood?
    // (Guarding firewood may sound silly, but is quite relevant for Fedhas
    // + Martyr's Knell)
    if (victim.is_firewood() && aura.ench_type != ENCH_INJURY_BOND)
        return false;

    // If the aura suppressed by sanctuary?
    if (aura.is_hostile && (is_sanctuary(source_pos) || is_sanctuary(victim.pos())))
        return false;

    // Does the aura have more specific conditions that are invalid?
    if (aura.valid_target && !aura.valid_target(victim, source_mid))
        return false;

    // Looks good!
    return true;
}

static bool _aura_could_affect(const aura_data& aura,
                               const monster& mon_source,
                               const actor& victim)
{
    return _aura_could_affect(aura, mon_source.pos(), mon_source.mid,
                              mon_source.temp_attitude(), victim);
}

static void _update_aura(const aura_data& aura, const coord_def& source_pos,
                         mid_t source_mid, mon_attitude_type source_attitude)
{
    // Hostile auras are suppressed by the source being in a sanctuary
    if (aura.is_hostile && is_sanctuary(source_pos))
        return;

    if (aura.ench_type != ENCH_NONE)
    {
        for (monster_near_iterator mi(source_pos, LOS_NO_TRANS); mi; ++mi)
        {
            if (_aura_could_affect(aura, source_pos, source_mid, source_attitude, **mi))
            {
                // Check if the target has a pre-existing version of this enchantment
                // with a longer duration.
                if (mi->has_ench(aura.ench_type))
                {
                    mon_enchant en = mi->get_ench(aura.ench_type);
                    if (en.duration > aura.base_duration)
                        continue;
                }

                mon_enchant new_ench(aura.ench_type, monster_by_mid(source_mid),
                                     aura.base_duration, 0,
                                     aura.is_hostile ? AURA_HOSTILE : AURA_FRIENDLY);

                // Override an existing enchant rather than just add to it
                // (which would stack durations).
                if (mi->has_ench(aura.ench_type))
                    mi->update_ench(new_ench);
                else
                    mi->add_ench(new_ench);
            }
        }
    }

    // End here if this doesn't also affect the player
    if (aura.dur_type == NUM_DURATIONS
        || !cell_see_cell(source_pos, you.pos(), LOS_NO_TRANS)
        || !_aura_could_affect(aura, source_pos, source_mid, source_attitude, you))
    {
        return;
    }

    if (you.duration[aura.dur_type] > aura.base_duration)
        return;

    if (!you.duration[aura.dur_type])
    {
        if (aura.player_msg)
            aura.player_msg(monster_by_mid(source_mid));
        if (aura.dur_type == DUR_PHALANX_BARRIER)
            you.redraw_armour_class = true;
    }

    you.duration[aura.dur_type] = aura.base_duration;
    you.props[aura.player_key].get_bool() = true;
}

void mons_update_aura(const monster& mon)
{
    if (!mons_has_aura(mon))
        return;

    const aura_data aura = _get_aura_for(mon);

    _update_aura(aura, mon.pos(), mon.mid, mon.temp_attitude());
}

static const aura_data AURA_OF_VIGOR = {ENCH_DOUBLED_VIGOUR, 1, false};

void player_update_auras()
{
    if (you.duration[DUR_DIVINE_VIGOUR])
        _update_aura(AURA_OF_VIGOR, you.pos(), MID_PLAYER, ATT_FRIENDLY);
}

bool aura_is_active(const monster& affected, enchant_type type)
{
    if (type == ENCH_DOUBLED_VIGOUR && you.duration[DUR_DIVINE_VIGOUR]
        && _aura_could_affect(AURA_OF_VIGOR, you.pos(), MID_PLAYER, ATT_FRIENDLY, affected))
    {
        return true;
    }

    for (monster_near_iterator mi(affected.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_has_aura_of_type(**mi, type))
            continue;

        if (!_aura_could_affect(_get_aura_for(**mi), **mi, affected))
            continue;

        return true;
    }

    return false;
}

bool aura_is_active_on_player(string player_key)
{
    if (!you.props.exists(player_key))
        return false;

    const aura_data aura = _get_aura_from_key(player_key);
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_has_aura_of_type(**mi, aura.dur_type))
            continue;

        if (!_aura_could_affect(aura, **mi, you))
            continue;

        return true;
    }

    // We didn't find the aura, so delete the key at the same time
    you.props.erase(player_key);

    return false;
}
