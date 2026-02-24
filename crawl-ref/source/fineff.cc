/**
 * @file
 * @brief Brand/ench/etc effects that might alter something in an
 *             unexpected way.
**/

#include "AppHdr.h"

#include "fineff.h"

#include "act-iter.h"
#include "actor.h"
#include "attitude-change.h"
#include "beam.h"
#include "bloodspatter.h"
#include "coordit.h"
#include "dactions.h"
#include "death-curse.h"
#include "delay.h" // stop_delay
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "fight.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-wrath.h" // lucy_check_meddling
#include "libutil.h"
#include "losglobal.h"
#include "melee-attack.h"
#include "message.h"
#include "mgen-data.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-util.h"
#include "monster.h"
#include "movement.h"
#include "ouch.h"
#include "religion.h"
#include "spl-damage.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"

class final_effect
{
public:
    virtual ~final_effect() {}

    virtual bool mergeable(const final_effect& a) const = 0;
    virtual void merge(const final_effect&)
    {
    }

    virtual void fire() = 0;

protected:
    mid_t att, def;
    coord_def posn;
    actor* attacker() const { return actor_by_mid(att); }
    actor* defender() const { return actor_by_mid(def); }

    final_effect(const actor* attack, const actor* defend, const coord_def& pos)
        : att(attack ? attack->mid : 0),
        def(defend ? defend->mid : 0),
        posn(pos)
    {
    }
};

class mirror_damage_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void merge(const final_effect& a) override;
    void fire() override;

    mirror_damage_fineff(const actor* attack, const actor* defend, int dam)
        : final_effect(attack, defend, coord_def()), damage(dam)
    {
    }
protected:
    int damage;
};

class anguish_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void merge(const final_effect& a) override;
    void fire() override;

    anguish_fineff(const actor* attack, int dam)
        : final_effect(attack, nullptr, coord_def()), damage(dam)
    {
    }
protected:
    int damage;
};

class ru_retribution_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void merge(const final_effect& a) override;
    void fire() override;

    ru_retribution_fineff(const actor* attack, const actor* defend, int dam)
        : final_effect(attack, defend, coord_def()), damage(dam)
    {
    }
protected:
    int damage;
};

class trample_follow_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void fire() override;

    trample_follow_fineff(const actor* attack, const coord_def& pos)
        : final_effect(attack, 0, pos)
    {
    }
};

class blink_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void fire() override;

    blink_fineff(const actor* blinker, const actor* o)
        : final_effect(o, blinker, coord_def())
    {
    }
};

class teleport_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void fire() override;

    teleport_fineff(const actor* defend)
        : final_effect(0, defend, coord_def())
    {
    }
};

class trj_spawn_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void merge(const final_effect& a) override;
    void fire() override;

    trj_spawn_fineff(const actor* attack, const actor* defend,
        const coord_def& pos, int dam)
        : final_effect(attack, defend, pos), damage(dam)
    {
    }
protected:
    int damage;
};

class blood_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void fire() override;
    void merge(const final_effect& a) override;

    blood_fineff(const actor* defend, const coord_def& pos, int blood_amount)
        : final_effect(0, 0, pos), mtype(defend->type), blood(blood_amount)
    {
    }
protected:
    monster_type mtype;
    int blood;
};

class deferred_damage_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void merge(const final_effect& a) override;
    void fire() override;

    deferred_damage_fineff(const actor* attack, const actor* defend,
        int dam, bool _attacker_effects, bool _fatal)
        : final_effect(attack, defend, coord_def()),
        damage(dam), attacker_effects(_attacker_effects), fatal(_fatal)
    {
    }
protected:
    int damage;
    bool attacker_effects;
    bool fatal;
};

class starcursed_merge_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void fire() override;

    starcursed_merge_fineff(const actor* merger)
        : final_effect(0, merger, coord_def())
    {
    }
};

class shock_discharge_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void merge(const final_effect& a) override;
    void fire() override;

    shock_discharge_fineff(const actor* discharger, actor& rudedude,
        coord_def pos, int pow, string shock_src)
        : final_effect(0, discharger, coord_def()), oppressor(rudedude),
        position(pos), power(pow), shock_source(shock_src)
    {
    }
protected:
    actor& oppressor;
    coord_def position;
    int power;
    string shock_source;
};

class explosion_fineff : public final_effect
{
public:
    // One explosion at a time, please.
    bool mergeable(const final_effect&) const override { return false; }
    void fire() override;

    explosion_fineff(const bolt& beem, string boom, string sanct,
        explosion_fineff_type _typ, const actor* agent,
        string poof)
        : final_effect(0, 0, coord_def()), beam(beem),
        boom_message(boom), sanctuary_message(sanct),
        typ(_typ), flame_agent(agent), poof_message(poof)
    {
    }
protected:
    bolt beam;
    string boom_message;
    string sanctuary_message;
    explosion_fineff_type typ;
    const actor* flame_agent;
    string poof_message;
};

class splinterfrost_fragment_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&) const override { return false; }
    void fire() override;

    splinterfrost_fragment_fineff(bolt beem, string _msg)
        : final_effect(0, 0, coord_def()), beam(beem), msg(_msg)
    {
    }
protected:
    bolt beam;
    string msg;
};

// A fineff that triggers a daction; otherwise the daction
// occurs immediately (and then later) -- this might actually
// be too soon in some cases.
class delayed_action_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override { return false; };
    virtual void fire() override;

    delayed_action_fineff(daction_type _action, const string& _final_msg)
        : final_effect(0, 0, coord_def()),
        action(_action), final_msg(_final_msg)
    {
    }
protected:
    daction_type action;
    string final_msg;
};

class kirke_death_fineff : public delayed_action_fineff
{
public:
    void fire() override;

    kirke_death_fineff(const string& _final_msg)
        : delayed_action_fineff(DACT_KIRKE_HOGS, _final_msg)
    {
    }
};

class rakshasa_clone_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void fire() override;

    rakshasa_clone_fineff(const actor* defend, const coord_def& pos)
        : final_effect(0, defend, pos)
    {
    }
protected:
    int damage;
};

class bennu_revive_fineff : public final_effect
{
public:
    // Each trigger is from the death of a different bennu---no merging.
    bool mergeable(const final_effect&) const override { return false; }
    void fire() override;

    bennu_revive_fineff(const monster* bennu)
        : final_effect(bennu, 0, bennu->pos())
    {
        env.final_effect_monster_cache.push_back(*bennu);
    }
};

class avoided_death_fineff : public final_effect
{
public:
    // Each trigger is from the death of a different monster---no merging.
    bool mergeable(const final_effect&) const override { return false; }
    void fire() override;

    avoided_death_fineff(const actor* _def, int _hp)
        : final_effect(0, _def, coord_def()), hp(_hp)
    {
    }
protected:
    int hp;
};

class infestation_death_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&) const override { return false; }
    void fire() override;

    infestation_death_fineff(coord_def pos, const string& _name)
        : final_effect(0, 0, pos), name(_name)
    {
    }
protected:
    string name;
};

class make_derived_undead_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&) const override { return false; }
    void fire() override;

    make_derived_undead_fineff(coord_def pos, mgen_data _mg, int _xl,
        const string& _agent, const string& _msg,
        bool _act_immediately)
        : final_effect(0, 0, pos), mg(_mg), experience_level(_xl),
        agent(_agent), message(_msg), act_immediately(_act_immediately)
    {
    }
protected:
    mgen_data mg;
    int experience_level;
    string agent;
    string message;
    bool act_immediately;
};

class mummy_death_curse_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&) const override { return false; }
    void fire() override;

    mummy_death_curse_fineff(const actor* attack, const monster* source, killer_type _killer, int _pow)
        : final_effect(fixup_attacker(attack), 0, coord_def()),
        killer(_killer), pow(_pow)
    {
        // Cache the dying mummy so morgues can look up the monster source if it kills us.
        env.final_effect_monster_cache.push_back(*source);
        dead_mummy = source->mid;
    }
protected:
    const actor* fixup_attacker(const actor* a);

    killer_type killer;
    int pow;
    mid_t dead_mummy;
};

class summon_dismissal_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& fe) const override;
    void merge(const final_effect&) override;
    void fire() override;

    summon_dismissal_fineff(const actor* _defender)
        : final_effect(0, _defender, coord_def())
    {
    }
};

class spectral_weapon_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&) const override { return false; };
    void fire() override;

    spectral_weapon_fineff(const actor& attack, const actor& defend,
        item_def* wpn)
        : final_effect(&attack, &defend, coord_def()), weapon(wpn)
    {
    }
protected:
    item_def* weapon;
};

class lugonu_meddle_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override
    {
        return typeid(*this) == typeid(a);
    };
    void fire() override;

    lugonu_meddle_fineff() : final_effect(nullptr, nullptr, coord_def()) {}
};

class jinxbite_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&/*a*/) const override { return false; };
    void fire() override;

    jinxbite_fineff(const actor* defend)
        : final_effect(nullptr, defend, coord_def())
    {
    }
};

class beogh_resurrection_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override;
    void fire() override;

    beogh_resurrection_fineff(bool end_ostracism_only)
        : final_effect(nullptr, nullptr, coord_def()), ostracism_only(end_ostracism_only)
    {
    }
protected:
    const bool ostracism_only;
};

class dismiss_divine_allies_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&) const override { return false; }
    void fire() override;

    dismiss_divine_allies_fineff(const god_type _god)
        : final_effect(0, 0, coord_def()), god(_god)
    {
    }
protected:
    const god_type god;
};

class death_spawn_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&) const override { return false; }
    void fire() override;

    death_spawn_fineff(mgen_data _mg)
        : final_effect(0, 0, _mg.pos), mg(_mg)
    {
    }
protected:
    const mgen_data mg;
};

class detonation_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&/*a*/) const override { return false; };
    void fire() override;

    detonation_fineff(const coord_def& pos, const item_def* wpn)
        : final_effect(&you, nullptr, pos), weapon(wpn)
    {
    }
protected:
    const item_def* weapon;
};

class stardust_fineff : public final_effect
{
public:
    bool mergeable(const final_effect&/*a*/) const override { return false; };
    void fire() override;

    stardust_fineff(actor* agent, int _power, int _max, bool _is_star_jelly)
        : final_effect(agent, nullptr, you.pos()), power(_power), max_stars(_max),
                                                   is_star_jelly(_is_star_jelly)
    {
        // If this is a star jelly, cache it (even if it's not dead yet; since
        // it may die to further damage events within the same attack action.)
        if (is_star_jelly)
            env.final_effect_monster_cache.push_back(*agent->as_monster());
    }
protected:
    int power;
    int max_stars;
    bool is_star_jelly;
};

class pyromania_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override
    {
        return typeid(*this) == typeid(a);
    };
    void fire() override;

    pyromania_fineff()
        : final_effect(&you, nullptr, you.pos())
    {
    }
};

class celebrant_bloodrite_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override
    {
        return typeid(*this) == typeid(a);
    }
    void fire() override;

    celebrant_bloodrite_fineff()
        : final_effect(&you, nullptr, you.pos())
    {
    }
};

class eeljolt_fineff : public final_effect
{
public:
    bool mergeable(const final_effect& a) const override
    {
        return typeid(*this) == typeid(a);
    }
    void fire() override;

    eeljolt_fineff()
        : final_effect(&you, nullptr, you.pos())
    {
    }
};

// Things to happen when the current attack/etc finishes.
static vector<final_effect*> _final_effects;

static void _schedule_final_effect(final_effect *eff)
{
    for (auto fe : _final_effects)
    {
        if (fe->mergeable(*eff))
        {
            fe->merge(*eff);
            delete eff;
            return;
        }
    }
    _final_effects.push_back(eff);
}

void schedule_mirror_damage_fineff(const actor* attack, const actor* defend,
                                   int dam)
{
    _schedule_final_effect(new mirror_damage_fineff(attack, defend, dam));
}

void schedule_anguish_fineff(const actor* attack, int dam)
{
    _schedule_final_effect(new anguish_fineff(attack, dam));
}

void schedule_ru_retribution_fineff(const actor* attack, const actor* defend,
                                    int dam)
{
    _schedule_final_effect(new ru_retribution_fineff(attack, defend, dam));
}

void schedule_trample_follow_fineff(const actor* attack, const coord_def& pos)
{
    _schedule_final_effect(new trample_follow_fineff(attack, pos));
}

void schedule_blink_fineff(const actor* blinker, const actor* other)
{
    _schedule_final_effect(new blink_fineff(blinker, other));
}

void schedule_teleport_fineff(const actor* defend)
{
    _schedule_final_effect(new teleport_fineff(defend));
}

void schedule_trj_spawn_fineff(const actor* attack, const actor* defend,
                               const coord_def& pos, int dam)
{
    _schedule_final_effect(new trj_spawn_fineff(attack, defend, pos, dam));
}

void schedule_blood_fineff(const actor* defend, const coord_def& pos,
                           int blood_amount)
{
    _schedule_final_effect(new blood_fineff(defend, pos, blood_amount));
}

void schedule_deferred_damage_fineff(const actor* attack, const actor* defend,
                                     int dam, bool attacker_effects,
                                     bool fatal)
{
    _schedule_final_effect(new deferred_damage_fineff(attack, defend, dam,
                                                      attacker_effects,
                                                      fatal));
}

void schedule_starcursed_merge_fineff(const actor* merger)
{
    _schedule_final_effect(new starcursed_merge_fineff(merger));
}

void schedule_shock_discharge_fineff(const actor* discharger, actor& oppressor,
                                     coord_def pos, int pow,
                                     string shock_source)
{
    _schedule_final_effect(new shock_discharge_fineff(discharger,
                                                      oppressor, pos, pow,
                                                      shock_source));
}

void schedule_explosion_fineff(bolt& beam, string boom, string sanct,
                               explosion_fineff_type typ,
                               const actor* flame_agent,
                               string poof)
{
    _schedule_final_effect(new explosion_fineff(beam, boom, sanct,
                                                typ, flame_agent, poof));
}

void schedule_splinterfrost_fragment_fineff(bolt& beam, string msg)
{
    _schedule_final_effect(new splinterfrost_fragment_fineff(beam, msg));
}

void schedule_delayed_action_fineff(daction_type action,
                                    const string& final_msg)
{
    _schedule_final_effect(new delayed_action_fineff(action, final_msg));
}

void schedule_kirke_death_fineff(const string& final_msg)
{
    _schedule_final_effect(new kirke_death_fineff(final_msg));
}

void schedule_rakshasa_clone_fineff(const actor* defend, const coord_def& pos)
{
    _schedule_final_effect(new rakshasa_clone_fineff(defend, pos));
}

void schedule_bennu_revive_fineff(const monster* bennu)
{
    _schedule_final_effect(new bennu_revive_fineff(bennu));
}

void schedule_avoided_death_fineff(monster* mons)
{
    // pretend to be dead until our revival, to prevent
    // sequencing errors from inadvertently making us change alignment
    const int realhp = mons->hit_points;
    mons->hit_points = -realhp;
    mons->flags |= MF_PENDING_REVIVAL;
    mons->props.erase(ATTACK_KILL_KEY);
    _schedule_final_effect(new avoided_death_fineff(mons, realhp));
}

void schedule_infestation_death_fineff(coord_def pos, const string& name)
{
    _schedule_final_effect(new infestation_death_fineff(pos, name));
}

void schedule_make_derived_undead_fineff(coord_def pos, mgen_data mg, int xl,
                                         const string& agent,
                                         const string& msg,
                                         bool act_immediately)
{
    _schedule_final_effect(new make_derived_undead_fineff(pos, mg, xl, agent,
                                                          msg,
                                                          act_immediately));
}

void schedule_mummy_death_curse_fineff(const actor* attack,
                                       const monster* dead_mummy,
                                       killer_type killer, int pow)
{
    _schedule_final_effect(new mummy_death_curse_fineff(attack, dead_mummy,
                                                        killer, pow));
}

void schedule_summon_dismissal_fineff(const actor* _defender)
{
    _schedule_final_effect(new summon_dismissal_fineff(_defender));
}

void schedule_spectral_weapon_fineff(const actor& attack, const actor& defend,
                                     item_def* weapon)
{
    _schedule_final_effect(new spectral_weapon_fineff(attack, defend, weapon));
}

void schedule_lugonu_meddle_fineff()
{
    _schedule_final_effect(new lugonu_meddle_fineff());
}

void schedule_jinxbite_fineff(const actor* defend)
{
    _schedule_final_effect(new jinxbite_fineff(defend));
}

void schedule_beogh_resurrection_fineff(bool end_ostracism_only)
{
    _schedule_final_effect(new beogh_resurrection_fineff(end_ostracism_only));
}

void schedule_dismiss_divine_allies_fineff(const god_type god)
{
    _schedule_final_effect(new dismiss_divine_allies_fineff(god));
}

void schedule_death_spawn_fineff(monster_type mon_type, coord_def pos, int dur,
                                 int summon_type)
{
    mgen_data _mg = mgen_data(mon_type, BEH_HOSTILE, pos,
                              MHITNOT, MG_FORCE_PLACE);
    _mg.set_summoned(nullptr, summon_type, dur, false, false);
    _schedule_final_effect(new death_spawn_fineff(_mg));
}

void schedule_death_spawn_fineff(mgen_data mg)
{
    _schedule_final_effect(new death_spawn_fineff(mg));
}

void schedule_detonation_fineff(const coord_def& pos, const item_def* wpn)
{
    _schedule_final_effect(new detonation_fineff(pos, wpn));
}

void schedule_stardust_fineff(actor* agent, int power, int max_stars, bool force_max)
{
    _schedule_final_effect(new stardust_fineff(agent, power, max_stars, force_max));
}

void schedule_pyromania_fineff()
{
    _schedule_final_effect(new pyromania_fineff());
}

void schedule_celebrant_bloodrite_fineff()
{
    _schedule_final_effect(new celebrant_bloodrite_fineff());
}

void schedule_eeljolt_fineff()
{
    _schedule_final_effect(new eeljolt_fineff());
}

bool mirror_damage_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const mirror_damage_fineff& o =
        static_cast<const mirror_damage_fineff&>(fe);
    return att == o.att && def == o.def;
}

bool anguish_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const anguish_fineff& o = static_cast<const anguish_fineff&>(fe);
    return att == o.att;
}

bool ru_retribution_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const ru_retribution_fineff& o =
        static_cast<const ru_retribution_fineff&>(fe);
    return att == o.att && def == o.def;
}

bool trample_follow_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const trample_follow_fineff& o =
        static_cast<const trample_follow_fineff&>(fe);
    return att == o.att;
}

bool blink_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const blink_fineff& o = static_cast<const blink_fineff&>(fe);
    return def == o.def && att == o.att;
}

bool teleport_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const teleport_fineff& o = static_cast<const teleport_fineff&>(fe);
    return def == o.def;
}

bool trj_spawn_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const trj_spawn_fineff& o = static_cast<const trj_spawn_fineff&>(fe);
    return att == o.att && def == o.def && posn == o.posn;
}

bool blood_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const blood_fineff& o = static_cast<const blood_fineff&>(fe);
    return posn == o.posn && mtype == o.mtype;
}

bool deferred_damage_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const deferred_damage_fineff& o =
        static_cast<const deferred_damage_fineff&>(fe);
    return att == o.att && def == o.def
           && attacker_effects == o.attacker_effects && fatal == o.fatal;
}

bool starcursed_merge_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const starcursed_merge_fineff& o =
        static_cast<const starcursed_merge_fineff&>(fe);
    return def == o.def;
}

bool shock_discharge_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const shock_discharge_fineff& o =
        static_cast<const shock_discharge_fineff&>(fe);
    return def == o.def;
}

bool rakshasa_clone_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const rakshasa_clone_fineff& o =
        static_cast<const rakshasa_clone_fineff&>(fe);
    return att == o.att && def == o.def && posn == o.posn;
}

bool summon_dismissal_fineff::mergeable(const final_effect &fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const summon_dismissal_fineff& o =
        static_cast<const summon_dismissal_fineff&>(fe);
    return def == o.def;
}

bool beogh_resurrection_fineff::mergeable(const final_effect& fe) const
{
    if (typeid(*this) != typeid(fe))
        return false;
    const beogh_resurrection_fineff& o =
        static_cast<const beogh_resurrection_fineff&>(fe);
    return ostracism_only == o.ostracism_only;
}

void mirror_damage_fineff::merge(const final_effect &fe)
{
    const mirror_damage_fineff *mdfe =
        dynamic_cast<const mirror_damage_fineff *>(&fe);
    ASSERT(mdfe);
    ASSERT(mergeable(*mdfe));
    damage += mdfe->damage;
}

void anguish_fineff::merge(const final_effect &fe)
{
    const anguish_fineff *afe =
        dynamic_cast<const anguish_fineff *>(&fe);
    ASSERT(afe);
    ASSERT(mergeable(*afe));
    damage += afe->damage;
}

void ru_retribution_fineff::merge(const final_effect &fe)
{
    const ru_retribution_fineff *mdfe =
        dynamic_cast<const ru_retribution_fineff *>(&fe);
    ASSERT(mdfe);
    ASSERT(mergeable(*mdfe));
}

void trj_spawn_fineff::merge(const final_effect &fe)
{
    const trj_spawn_fineff *trjfe =
        dynamic_cast<const trj_spawn_fineff *>(&fe);
    ASSERT(trjfe);
    ASSERT(mergeable(*trjfe));
    damage += trjfe->damage;
}

void blood_fineff::merge(const final_effect &fe)
{
    const blood_fineff *bfe = dynamic_cast<const blood_fineff *>(&fe);
    ASSERT(bfe);
    ASSERT(mergeable(*bfe));
    blood += bfe->blood;
}

void deferred_damage_fineff::merge(const final_effect &fe)
{
    const deferred_damage_fineff *ddamfe =
        dynamic_cast<const deferred_damage_fineff *>(&fe);
    ASSERT(ddamfe);
    ASSERT(mergeable(*ddamfe));
    damage += ddamfe->damage;
}

void shock_discharge_fineff::merge(const final_effect &fe)
{
    const shock_discharge_fineff *ssdfe =
        dynamic_cast<const shock_discharge_fineff *>(&fe);
    power += ssdfe->power;
}

void summon_dismissal_fineff::merge(const final_effect &)
{
    // no damage to accumulate, but no need to fire this more than once
    return;
}

void mirror_damage_fineff::fire()
{
    actor *attack = attacker();
    if (!attack || attack == defender() || !attack->alive())
        return;
    // defender being dead is ok, if we killed them we still suffer

    god_acting gdact(GOD_YREDELEMNUL); // XXX: remove?

    if (att == MID_PLAYER)
    {
        const monster* reflector = defender() ?
                                   defender()->as_monster() : nullptr;
        if (reflector)
        {
            mprf("%s reflects your damage back at you!",
                 reflector->name(DESC_THE).c_str());
        }
        else
            mpr("Your damage is reflected back at you!");
        ouch(damage, KILLED_BY_MIRROR_DAMAGE);
    }
    else
    {
        simple_monster_message(*monster_by_mid(att), " suffers a backlash!");
        attack->hurt(defender(), damage);
    }
}

void anguish_fineff::fire()
{
    actor *attack = attacker();
    if (!attack || !attack->alive())
        return;

    const string punct = attack_strength_punctuation(damage);
    const string msg = make_stringf(" is wracked by anguish%s", punct.c_str());
    simple_monster_message(*monster_by_mid(att), msg.c_str());
    attack->hurt(monster_by_mid(MID_YOU_FAULTLESS), damage);
}

void ru_retribution_fineff::fire()
{
    actor *attack = attacker();
    if (!attack || attack == defender() || !attack->alive())
        return;
    if (def == MID_PLAYER)
        ru_do_retribution(monster_by_mid(att), damage);
}

void trample_follow_fineff::fire()
{
    actor *attack = attacker();
    if (attack
        && attack->pos() != posn
        && adjacent(attack->pos(), posn)
        && attack->is_habitable(posn)
        && !monster_at(posn))
    {
        attack->move_to(posn, MV_DELIBERATE);
    }
}

void blink_fineff::fire()
{
    actor *defend = defender();
    if (!defend || !defend->alive() || defend->no_tele())
        return;

    // if we're doing 'blink with', only blink if we have a partner
    actor *pal = attacker();
    if (pal && (!pal->alive() || pal->no_tele()))
        return;

    defend->blink();
    if (!defend->alive())
        return;

    // Is something else also getting blinked?
    if (!pal || !pal->alive() || pal->no_tele())
        return;

    for (fair_adjacent_iterator ai(defend->pos()); ai; ++ai)
    {
        // No blinking into teleport closets.
        if (testbits(env.pgrid(*ai), FPROP_NO_TELE_INTO))
            continue;
        // XXX: allow fedhasites to be blinked into plants?
        if (actor_at(*ai) || !pal->is_habitable(*ai))
            continue;

        pal->blink_to(*ai);
        break;
    }
}

void teleport_fineff::fire()
{
    actor *defend = defender();
    if (defend && defend->alive() && !defend->no_tele())
        defend->teleport(true);
}

void trj_spawn_fineff::fire()
{
    const actor *attack = attacker();
    actor *trj = defender();

    int tospawn = div_rand_round(damage, 12);

    if (tospawn <= 0)
        return;

    dprf("Trying to spawn %d jellies.", tospawn);

    unsigned short foe = attack && attack->alive() ? attack->mindex() : MHITNOT;
    // may be ANON_FRIENDLY_MONSTER
    if (invalid_monster_index(foe) && foe != MHITYOU)
        foe = MHITNOT;

    // Give spawns the same attitude as TRJ; if TRJ is now dead, make them
    // hostile.
    const beh_type spawn_beh = trj
        ? attitude_creation_behavior(trj->as_monster()->attitude)
        : BEH_HOSTILE;

    // No permanent friendly jellies from a charmed TRJ.
    if (spawn_beh == BEH_FRIENDLY && !crawl_state.game_is_arena())
        return;

    int spawned = 0;
    for (int i = 0; i < tospawn; ++i)
    {
        const monster_type jelly = royal_jelly_ejectable_monster();
        if (monster *mons = create_monster(
                              mgen_data(jelly, spawn_beh, posn, foe,
                                        MG_DONT_COME, GOD_JIYVA)
                              .set_summoned(trj, 0)
                              .set_range(1, LOS_RADIUS)
                              .copy_from_parent(trj)))
        {
            // Don't allow milking the Royal Jelly.
            mons->flags |= MF_NO_REWARD | MF_HARD_RESET;
            spawned++;
        }
    }

    if (!spawned || !you.see_cell(posn))
        return;

    if (trj)
    {
        const string monnam = trj->name(DESC_THE);
        mprf("%s shudders%s.", monnam.c_str(),
             spawned >= 5 ? " alarmingly" :
             spawned >= 3 ? " violently" :
             spawned > 1 ? " vigorously" : "");

        if (spawned == 1)
            mprf("%s spits out another jelly.", monnam.c_str());
        else
        {
            mprf("%s spits out %s more jellies.",
                 monnam.c_str(),
                 number_in_words(spawned).c_str());
        }
    }
    else if (spawned == 1)
        mpr("One of the Royal Jelly's fragments survives.");
    else
    {
        mprf("%s of the Royal Jelly's fragments survive.",
             number_in_words(spawned).c_str());
    }
}

void blood_fineff::fire()
{
    bleed_onto_floor(posn, mtype, blood, true);
}

void deferred_damage_fineff::fire()
{
    actor *df = defender();
    if (!df)
        return;

    // Once we actually apply damage to tentacle monsters,
    // remove their protection from further damage.
    if (df->props.exists(TENTACLE_LORD_HITS))
        df->props.erase(TENTACLE_LORD_HITS);

    if (!fatal)
    {
        // Cap non-fatal damage by the defender's hit points
        // FIXME: Consider adding a 'fatal' parameter to ::hurt
        //        to better interact with damage reduction/boosts
        //        which may be applied later.
        int df_hp = df->is_player() ? you.hp
                                    : df->as_monster()->hit_points;
        damage = min(damage, df_hp - 1);
    }

    df->hurt(attacker(), damage, BEAM_MISSILE, KILLED_BY_MONSTER, "", "",
             true, attacker_effects);
}

static void _do_merge_masses(monster* initial_mass, monster* merge_to)
{
    // Combine enchantment durations.
    merge_ench_durations(*initial_mass, *merge_to);

    merge_to->blob_size += initial_mass->blob_size;
    merge_to->max_hit_points += initial_mass->max_hit_points;
    merge_to->hit_points += initial_mass->hit_points;

    // Merge monster flags (mostly so that MF_CREATED_NEUTRAL, etc. are
    // passed on if the merged slime subsequently splits. Hopefully
    // this won't do anything weird.
    merge_to->flags |= initial_mass->flags;

    // Overwrite the state of the slime getting merged into, because it
    // might have been resting or something.
    merge_to->behaviour = initial_mass->behaviour;
    merge_to->foe = initial_mass->foe;

    behaviour_event(merge_to, ME_EVAL);

    // Have to 'kill' the slime doing the merging.
    monster_die(*initial_mass, KILL_RESET, NON_MONSTER, true);
}

void starcursed_merge_fineff::fire()
{
    actor *defend = defender();
    if (!defend || !defend->alive())
        return;

    monster *mon = defend->as_monster();

    // Find a random adjacent starcursed mass and merge with it.
    for (fair_adjacent_iterator ai(mon->pos()); ai; ++ai)
    {
        monster* mergee = monster_at(*ai);
        if (mergee && mergee->alive() && mergee->type == MONS_STARCURSED_MASS)
        {
            simple_monster_message(*mon,
                    " shudders and is absorbed by its neighbour.");
            _do_merge_masses(mon, mergee);
            return;
        }
    }

    // If there was nothing adjacent to merge with, at least try to move toward
    // another starcursed mass
    for (distance_iterator di(mon->pos(), true, true, LOS_RADIUS); di; ++di)
    {
        monster* ally = monster_at(*di);
        if (ally && ally->alive() && ally->type == MONS_STARCURSED_MASS
            && mon->can_see(*ally))
        {
            bool moved = false;

            coord_def sgn = (*di - mon->pos()).sgn();
            if (mon_can_move_to_pos(mon, sgn))
            {
                mon->move_to(mon->pos()+sgn, MV_DELIBERATE, true);
                moved = true;
            }
            else if (abs(sgn.x) != 0)
            {
                coord_def dx(sgn.x, 0);
                if (mon_can_move_to_pos(mon, dx))
                {
                    mon->move_to(mon->pos()+dx, MV_DELIBERATE, true);
                    moved = true;
                }
            }
            else if (abs(sgn.y) != 0)
            {
                coord_def dy(0, sgn.y);
                if (mon_can_move_to_pos(mon, dy))
                {
                    mon->move_to(mon->pos()+dy, MV_DELIBERATE, true);
                    moved = true;
                }
            }

            if (moved)
            {
                simple_monster_message(*mon, " shudders and withdraws towards its neighbour.");
                mon->speed_increment -= 10;
                mon->finalise_movement();
            }
        }
    }
}

void shock_discharge_fineff::fire()
{
    if (!oppressor.alive())
        return;

    const int max_range = 3; // v0v
    if (grid_distance(oppressor.pos(), position) > max_range
        || !cell_see_cell(position, oppressor.pos(), LOS_SOLID_SEE))
    {
        return;
    }

    const int amount = roll_dice(3, 4 + power * 3 / 2);
    int final_dmg = resist_adjust_damage(&oppressor, BEAM_ELECTRICITY, amount);
    final_dmg = oppressor.apply_ac(final_dmg, 0, ac_type::half);

    const actor *serpent = defender();
    if (serpent && you.can_see(*serpent))
    {
        mprf("%s %s discharges%s, shocking %s%s",
             serpent->name(DESC_ITS).c_str(),
             shock_source.c_str(),
             power < 4 ? "" : " violently",
             oppressor.name(DESC_THE).c_str(),
             attack_strength_punctuation(final_dmg).c_str());
    }
    else if (you.can_see(oppressor))
    {
        mprf("The air sparks with electricity, shocking %s%s",
             oppressor.name(DESC_THE).c_str(),
             attack_strength_punctuation(final_dmg).c_str());
    }

    bolt beam;
    beam.flavour = BEAM_ELECTRICITY;
    const string name = serpent && serpent->alive_or_reviving() ?
                        serpent->name(DESC_A, true) :
                        "a shock serpent"; // dubious
    oppressor.hurt(serpent, final_dmg, beam.flavour, KILLED_BY_BEAM,
                   name.c_str(), shock_source.c_str());

    // Do resist messaging
    if (oppressor.alive())
    {
        oppressor.beam_resists(beam, amount, true);
        if (final_dmg)
            oppressor.expose_to_element(beam.flavour, final_dmg);
    }
}

void explosion_fineff::fire()
{
    if (is_sanctuary(beam.target))
    {
        if (you.see_cell(beam.target))
            mprf(MSGCH_GOD, "%s", sanctuary_message.c_str());
        return;
    }

    if (you.see_cell(beam.target))
    {
        if (typ == EXPLOSION_FINEFF_CONCUSSION || typ == EXPLOSION_FINEFF_PYROMANIA)
            mpr(boom_message);
        else
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s", boom_message.c_str());
    }

    if (typ == EXPLOSION_FINEFF_INNER_FLAME)
        for (adjacent_iterator ai(beam.target, false); ai; ++ai)
            if (!one_chance_in(5))
                place_cloud(CLOUD_FIRE, *ai, 10 + random2(10), flame_agent);

    beam.explode(true, typ == EXPLOSION_FINEFF_PYROMANIA);

    if (typ == EXPLOSION_FINEFF_CONCUSSION)
    {
        for (fair_adjacent_iterator ai(beam.target); ai; ++ai)
        {
            actor *act = actor_at(*ai);
            if (!act
                || act->is_stationary()
                || !could_harm(&you, act))
            {
                continue;
            }

            const coord_def newpos = (*ai - beam.target) + *ai;
            if (newpos == *ai || actor_at(newpos) || !act->is_habitable(newpos))
                continue;

            act->move_to(newpos, MV_DEFAULT, true);
            if (you.can_see(*act))
            {
                mprf("%s %s knocked back by the blast.",
                     act->name(DESC_THE).c_str(),
                     act->conj_verb("are").c_str());
            }
            act->finalise_movement();
        }
    }

    if (you.see_cell(beam.target) && !poof_message.empty())
        mprf(MSGCH_MONSTER_TIMEOUT, "%s", poof_message.c_str());
}

void delayed_action_fineff::fire()
{
    if (!final_msg.empty())
        mpr(final_msg);
    add_daction(action);
}

void kirke_death_fineff::fire()
{
    delayed_action_fineff::fire();

    // Revert the player last
    if (you.form == transformation::pig)
        return_to_default_form();
}

void rakshasa_clone_fineff::fire()
{
    actor *defend = defender();
    if (!defend)
        return;

    monster *rakshasa = defend->as_monster();
    ASSERT(rakshasa);

    // Using SPELL_NO_SPELL to prevent overwriting normal clones
    cast_phantom_mirror(rakshasa, rakshasa, 50, SPELL_NO_SPELL);
    cast_phantom_mirror(rakshasa, rakshasa, 50, SPELL_NO_SPELL);
    rakshasa->lose_energy(EUT_SPELL);

    if (you.can_see(*rakshasa))
    {
        mprf(MSGCH_MONSTER_SPELL,
             "The injured %s weaves a defensive illusion!",
             rakshasa->name(DESC_PLAIN).c_str());
    }
}

void bennu_revive_fineff::fire()
{
    bool res_visible = you.see_cell(posn);

    const monster* orig = cached_monster_copy_by_mid(att);

    monster *newmons = create_monster(mgen_data(MONS_BENNU, BEH_HOSTILE, posn, orig->foe,
                                                res_visible ? MG_DONT_COME
                                                            : MG_NONE).copy_from_parent(orig));

    if (!newmons)
        return;

    const int old_revives = orig->props.exists(BENNU_REVIVES_KEY) ? orig->props[BENNU_REVIVES_KEY].get_byte() : 0;
    newmons->props[BENNU_REVIVES_KEY].get_byte() = old_revives + 1;

    // If we were dueling the original bennu, the duel continues.
    if (orig->props.exists(OKAWARU_DUEL_TARGET_KEY))
    {
        newmons->props[OKAWARU_DUEL_TARGET_KEY] = true;
        newmons->props[OKAWARU_DUEL_CURRENT_KEY] = true;
    }
}

void avoided_death_fineff::fire()
{
    actor* defend = defender();
    ASSERT(defend && defend->is_monster());
    monster* mons = defend->as_monster();
    mons->hit_points = hp;
    mons->flags &= ~MF_PENDING_REVIVAL;
}

void infestation_death_fineff::fire()
{
    if (monster *scarab = create_monster(mgen_data(MONS_DEATH_SCARAB,
                                                   BEH_FRIENDLY, posn,
                                                   MHITYOU, MG_AUTOFOE)
                                         .set_summoned(&you, SPELL_INFESTATION,
                                                       summ_dur(5), false),
                                         false))
    {
        if (you.see_cell(posn) || you.can_see(*scarab))
        {
            mprf("%s bursts from %s!", scarab->name(DESC_A, true).c_str(),
                                       name.c_str());
        }
    }
}

void make_derived_undead_fineff::fire()
{
    monster *undead = create_monster(mg);
    if (!undead)
        return;

    if (!message.empty() && you.can_see(*undead))
        mpr(message);

    // If the original monster has been levelled up, its HD might be
    // different from its class HD, in which case its HP should be
    // rerolled to match.
    if (undead->get_experience_level() != experience_level)
    {
        undead->set_hit_dice(max(experience_level, 1));
        roll_zombie_hp(undead);
    }

    if (!agent.empty())
        mons_add_blame(undead, "animated by " + agent);

    if (act_immediately)
    {
        undead->flags &= ~MF_JUST_SUMMONED;
        undead->speed_increment = 80;
        queue_monster_for_action(undead);
    }
}

const actor *mummy_death_curse_fineff::fixup_attacker(const actor *a)
{
    if (a && a->is_monster() && a->as_monster()->friendly()
        && !crawl_state.game_is_arena())
    {
        // Mummies are smart enough not to waste curses on summons or allies.
        return &you;
    }
    return a;
}

void mummy_death_curse_fineff::fire()
{
    if (pow <= 0)
        return;

    switch (killer)
    {
        // Mummy killed by trap or something other than the player or
        // another monster, so no curse.
        case KILL_NON_ACTOR:
        case KILL_RESET:
        case KILL_RESET_KEEP_ITEMS:
        // Mummy sent to the Abyss wasn't actually killed, so no curse.
        case KILL_BANISHED:
            return;

        default:
            break;
    }

    actor* victim;

    if (YOU_KILL(killer))
        victim = &you;
    // Killed by a Zot trap, a god, etc, or suicide.
    else if (!attacker() || attacker() == defender())
        return;
    else
        victim = attacker();

    // Mummy was killed by a ballistomycete spore or ball lightning?
    if (!victim->alive())
        return;

    // Stepped from time?
    if (!in_bounds(victim->pos()))
        return;

    if (victim->is_player())
        mprf(MSGCH_MONSTER_SPELL, "You feel extremely nervous for a moment...");
    else if (you.can_see(*victim))
    {
        mprf(MSGCH_MONSTER_SPELL, "A malignant aura surrounds %s.",
             victim->name(DESC_THE).c_str());
    }
    // The real mummy is dead, but we pass along a cached copy save at the time
    // they died (for morgue purposes)
    death_curse(*victim, cached_monster_copy_by_mid(dead_mummy), "", pow);
}

void summon_dismissal_fineff::fire()
{
    if (defender() && defender()->alive())
        monster_die(*(defender()->as_monster()), KILL_TIMEOUT, NON_MONSTER);
}

void spectral_weapon_fineff::fire()
{
    actor *atkr = attacker();
    actor *defend = defender();
    if (!defend || !atkr || !defend->alive() || !atkr->alive())
        return;

    if (!weapon || !weapon->defined())
        return;

    const coord_def target = defend->pos();

    // Do we already have a spectral weapon?
    monster* sw = find_spectral_weapon(*weapon);
    if (sw)
    {
        if (sw == defend)
            return; // don't attack yourself. too silly.
        // Is it already in range?
        const int sw_range = sw->reach_range();
        if (sw_range > 1
            && can_reach_attack_between(sw->pos(), target, sw_range)
            || adjacent(sw->pos(), target))
        {
            // Just attack.
            melee_attack melee_attk(sw, defend);
            melee_attk.attack();
            return;
        }
    }

    // Can we find a nearby space to attack from?
    const int atk_range = atkr->reach_range();
    int seen_valid = 0;
    coord_def chosen_pos;
    // Try only spaces adjacent to the attacker.
    for (adjacent_iterator ai(atkr->pos()); ai; ++ai)
    {
        if (actor_at(*ai)
            || !monster_habitable_grid(MONS_SPECTRAL_WEAPON, *ai))
        {
            continue;
        }
        // ... and only spaces the weapon could attack the defender from.
        if (grid_distance(*ai, target) > 1
            && (atk_range <= 1
                || !can_reach_attack_between(*ai, target, atk_range)))
        {
            continue;
        }
        // Reservoir sampling.
        seen_valid++;
        if (one_chance_in(seen_valid))
            chosen_pos = *ai;
    }
    if (!seen_valid)
        return;

    monster *mons = create_spectral_weapon(*atkr, chosen_pos, *weapon);
    if (!mons)
        return;

    melee_attack melee_attk(mons, defend);
    melee_attk.attack();
}

void lugonu_meddle_fineff::fire() {
    lucy_check_meddling();
}

void jinxbite_fineff::fire()
{
    actor* defend = defender();
    if (defend && defend->alive())
        attempt_jinxbite_hit(*defend);
}

void beogh_resurrection_fineff::fire()
{
    beogh_resurrect_followers(ostracism_only);
}

void dismiss_divine_allies_fineff::fire()
{
    if (god == GOD_BEOGH)
        beogh_do_ostracism();
    else
        dismiss_god_summons(god);
}

void death_spawn_fineff::fire()
{
    create_monster(mg);
}

void splinterfrost_fragment_fineff::fire()
{
    if (!msg.empty())
        mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s", msg.c_str());

    beam.fire();
}

void detonation_fineff::fire()
{
    do_catalyst_explosion(posn, weapon);
}

void stardust_fineff::fire()
{
    actor* agent = actor_by_mid(att);

    // In case the agent is dead, check for a cached copy.
    if (!agent)
        agent = cached_monster_copy_by_mid(att);
    if (!agent)
        return;

    if (agent->is_player() && is_sanctuary(you.pos()))
        return;

    int count = 0;
    for (actor_near_iterator ai(agent->pos(), LOS_NO_TRANS); ai; ++ai)
        if (!ai->is_firewood() && !mons_aligned(agent, *ai))
            ++count;

    // Don't activate or go on cooldown if there's nothing to shoot at.
    if (count == 0)
        return;

    if (is_star_jelly)
        mprf("A flurry of magic pours from %s injured body!", agent->name(DESC_ITS).c_str());
    else
        mprf("%s orb unleashes a flurry of shooting stars!", agent->name(DESC_ITS).c_str());

    count = is_star_jelly ? max_stars : min(max_stars, count + 1);
    const int foe = agent->is_player() ? int{MHITYOU} : agent->as_monster()->foe;
    for (int i = 0; i < count; ++i)
    {
        mgen_data mg(MONS_SHOOTING_STAR, BEH_COPY, agent->pos(),
                     foe, MG_FORCE_BEH | MG_FORCE_PLACE | MG_AUTOFOE);

        mg.set_summoned(agent, MON_SUMM_STARDUST, 200, false, false);
        mg.set_range(1, 3);
        mg.hd = power;
        mg.hp = 100;
        if (monster* mon = create_monster(mg))
            mon->steps_remaining = 12;
    }

    if (agent->is_player())
        you.duration[DUR_STARDUST_COOLDOWN] = random_range(40, 70);
    else if (!is_star_jelly)
        agent->as_monster()->add_ench(mon_enchant(ENCH_ORB_COOLDOWN, agent, random_range(300, 500)));
}

void pyromania_fineff::fire()
{
    // Test if there's at least *something* worth hitting before exploding
    // (primarily to avoid wasting the player's time). Note: we don't check the
    // the player *wants* to hit what is there, just that there is something
    // there to hit. Burning down random plants is thematic here.
    bool found = false;
    for (radius_iterator ri(you.pos(), 3, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            if (could_harm(&you, mon))
            {
                found = true;
                break;
            }
        }
    }

    if (!found)
        return;

    bolt exp;
    zappy(ZAP_FIREBALL, 50, false, exp);
    exp.damage = pyromania_damage();
    exp.set_agent(&you);
    exp.target = you.pos();
    exp.source = you.pos();
    exp.ex_size = 3;

    mpr("Your orb flickers with a hungry flame!");
    exp.explode(true, true);
}

constexpr int BLOODRITE_MIN_SHOTS = 8;

void celebrant_bloodrite_fineff::fire()
{
    vector<coord_def> targs;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mi->wont_attack() && !mi->is_firewood())
            targs.push_back(mi->pos());
    }

    // Don't activate or go on cooldown if there's nothing to shoot at.
    if (targs.empty())
        return;

    mpr("You consecrate your suffering and invoke the rites of blood!");

    // Set cooldown before firing, in case we recieve damage during the volley
    // (eg: via reflected projectiles) that would trigger this again.
    you.duration[DUR_CELEBRANT_COOLDOWN] = 1;

    shuffle_array(targs);

    int shots_fired = 0;
    int repeats = 0;

    bolt beam;
    beam.range        = you.current_vision;
    beam.source       = you.pos();
    beam.source_id    = MID_PLAYER;
    beam.attitude     = ATT_FRIENDLY;
    beam.thrower      = KILL_YOU;
    zappy(ZAP_BLOOD_ARROW, 15 + you.skill(SK_INVOCATIONS, 2), false, beam);

    beam.draw_delay   = 10;

    // Fire once at every visible target. If that doesn't hit the minimum number
    // of shots, fire at most a second time at each of these targets.
    while (shots_fired < BLOODRITE_MIN_SHOTS && repeats < 2)
    {
        for (size_t i = 0; i < targs.size()
                && (repeats == 0 || shots_fired < BLOODRITE_MIN_SHOTS); ++i)
        {
            bolt shot = beam;
            shot.target = targs[i];
            shot.fire();
            view_clear_overlays();
            ++shots_fired;
        }
        ++repeats;
        shuffle_array(targs);
    }

    // If firing twice at every target still didn't hit the minimum number of
    // shots, fire the rest of them completely at random.
    while (shots_fired < BLOODRITE_MIN_SHOTS)
    {
        coord_def targ = you.pos();
        targ.x += random_range(-LOS_RADIUS, LOS_RADIUS);
        targ.y += random_range(-LOS_RADIUS, LOS_RADIUS);

        if (targ == you.pos())
            continue;

        bolt shot = beam;
        shot.target = targ;
        shot.fire();
        view_clear_overlays();
        ++shots_fired;
    }
}

void eeljolt_fineff::fire()
{
    do_eel_arcjolt();
}

// Effects that occur after all other effects, even if the monster is dead.
// For example, explosions that would hit other creatures, but we want
// to deal with only one creature at a time, so that's handled last.
void fire_final_effects()
{
    while (!_final_effects.empty())
    {
        // Remove it first so nothing can merge with it.
        unique_ptr<final_effect> eff(_final_effects.back());
        _final_effects.pop_back();
        eff->fire();
    }

    // Clear all cached monster copies
    env.final_effect_monster_cache.clear();
}

void clear_final_effects()
{
    deleteAll(_final_effects);
}
