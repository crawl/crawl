/**
 * @file
 * @brief Brand/ench/etc effects that might alter something in an
 *             unexpected way.
**/

#pragma once

#include "actor.h"
#include "beh-type.h"
#include "mgen-data.h"
#include "mon-util.h"
#include "monster.h"

struct bolt;

class final_effect
{
public:
    virtual ~final_effect() {}

    virtual bool mergeable(const final_effect &a) const = 0;
    virtual void merge(const final_effect &)
    {
    }

    virtual void fire() = 0;

protected:
    static void schedule(final_effect *eff);

    mid_t att, def;
    coord_def posn;
    actor *attacker() const { return actor_by_mid(att); }
    actor *defender() const { return actor_by_mid(def); }

    final_effect(const actor *attack, const actor *defend, const coord_def &pos)
        : att(attack ? attack->mid : 0),
          def(defend ? defend->mid : 0),
          posn(pos)
    {
    }
};

class mirror_damage_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void merge(const final_effect &a) override;
    void fire() override;

    static void schedule(const actor *attack, const actor *defend, int dam)
    {
        final_effect::schedule(new mirror_damage_fineff(attack, defend, dam));
    }
protected:
    mirror_damage_fineff(const actor *attack, const actor *defend, int dam)
        : final_effect(attack, defend, coord_def()), damage(dam)
    {
    }
    int damage;
};

class anguish_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void merge(const final_effect &a) override;
    void fire() override;

    static void schedule(const actor *attack, int dam)
    {
        final_effect::schedule(new anguish_fineff(attack, dam));
    }
protected:
    anguish_fineff(const actor *attack, int dam)
        : final_effect(attack, nullptr, coord_def()), damage(dam)
    {
    }
    int damage;
};

class ru_retribution_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void merge(const final_effect &a) override;
    void fire() override;

    static void schedule(const actor *attack, const actor *defend, int dam)
    {
        final_effect::schedule(new ru_retribution_fineff(attack, defend, dam));
    }
protected:
    ru_retribution_fineff(const actor *attack, const actor *defend, int dam)
        : final_effect(attack, defend, coord_def()), damage(dam)
    {
    }
    int damage;
};

class trample_follow_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void fire() override;

    static void schedule(const actor *attack, const coord_def &pos)
    {
        final_effect::schedule(new trample_follow_fineff(attack, pos));
    }
protected:
    trample_follow_fineff(const actor *attack, const coord_def &pos)
        : final_effect(attack, 0, pos)
    {
    }
};

class blink_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void fire() override;

    static void schedule(const actor *blinker, const actor *other = nullptr)
    {
        final_effect::schedule(new blink_fineff(blinker, other));
    }
protected:
    blink_fineff(const actor *blinker, const actor *o)
        : final_effect(o, blinker, coord_def())
    {
    }
};

class teleport_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void fire() override;

    static void schedule(const actor *defend)
    {
        final_effect::schedule(new teleport_fineff(defend));
    }
protected:
    teleport_fineff(const actor *defend)
        : final_effect(0, defend, coord_def())
    {
    }
};

class trj_spawn_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void merge(const final_effect &a) override;
    void fire() override;

    static void schedule(const actor *attack, const actor *defend,
                         const coord_def &pos, int dam)
    {
        final_effect::schedule(new trj_spawn_fineff(attack, defend, pos, dam));
    }
protected:
    trj_spawn_fineff(const actor *attack, const actor *defend,
                     const coord_def &pos, int dam)
        : final_effect(attack, defend, pos), damage(dam)
    {
    }
    int damage;
};

class blood_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void fire() override;
    void merge(const final_effect &a) override;

    static void schedule(const actor *defend, const coord_def &pos,
                         int blood_amount)
    {
        final_effect::schedule(new blood_fineff(defend, pos, blood_amount));
    }
protected:
    blood_fineff(const actor *defend, const coord_def &pos, int blood_amount)
        : final_effect(0, 0, pos), mtype(defend->type), blood(blood_amount)
    {
    }
    monster_type mtype;
    int blood;
};

class deferred_damage_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void merge(const final_effect &a) override;
    void fire() override;

    static void schedule(const actor *attack, const actor *defend,
                         int dam, bool attacker_effects, bool fatal = true)
    {
        final_effect::schedule(
            new deferred_damage_fineff(attack, defend, dam, attacker_effects,
                                       fatal));
    }
protected:
    deferred_damage_fineff(const actor *attack, const actor *defend,
                           int dam, bool _attacker_effects, bool _fatal)
        : final_effect(attack, defend, coord_def()),
          damage(dam), attacker_effects(_attacker_effects), fatal(_fatal)
    {
    }
    int damage;
    bool attacker_effects;
    bool fatal;
};

class starcursed_merge_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void fire() override;

    static void schedule(const actor *merger)
    {
        final_effect::schedule(new starcursed_merge_fineff(merger));
    }
protected:
    starcursed_merge_fineff(const actor *merger)
        : final_effect(0, merger, coord_def())
    {
    }
};

class shock_discharge_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void merge(const final_effect &a) override;
    void fire() override;

    static void schedule(const actor *discharger, actor &oppressor,
                         coord_def pos, int pow, string shock_source)
    {
        final_effect::schedule(new shock_discharge_fineff(discharger,
                                                          oppressor, pos, pow,
                                                          shock_source));
    }
protected:
    shock_discharge_fineff(const actor *discharger, actor &rudedude,
                           coord_def pos, int pow, string shock_src)
        : final_effect(0, discharger, coord_def()), oppressor(rudedude),
          position(pos), power(pow), shock_source(shock_src)
    {
    }
    actor &oppressor;
    coord_def position;
    int power;
    string shock_source;
};

enum explosion_fineff_type : int {
    EXPLOSION_FINEFF_GENERIC,
    EXPLOSION_FINEFF_INNER_FLAME,
    EXPLOSION_FINEFF_CONCUSSION,
};

class explosion_fineff : public final_effect
{
public:
    // One explosion at a time, please.
    bool mergeable(const final_effect &) const override { return false; }
    void fire() override;

    static void schedule(bolt &beam, string boom, string sanct,
                         explosion_fineff_type typ, const actor* flame_agent,
                         string poof)
    {
        final_effect::schedule(new explosion_fineff(beam, boom, sanct,
                                                    typ, flame_agent, poof));
    }
protected:
    explosion_fineff(const bolt &beem, string boom, string sanct,
                     explosion_fineff_type _typ, const actor* agent,
                     string poof)
        : final_effect(0, 0, coord_def()), beam(beem),
          boom_message(boom), sanctuary_message(sanct),
          typ(_typ), flame_agent(agent), poof_message(poof)
    {
    }
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
    bool mergeable(const final_effect &) const override { return false; }
    void fire() override;

    static void schedule(bolt &beam, string msg)
    {
        final_effect::schedule(new splinterfrost_fragment_fineff(beam, msg));
    }
protected:
    splinterfrost_fragment_fineff(bolt beem, string _msg)
        : final_effect(0, 0, coord_def()), beam(beem), msg(_msg)
    {
    }
    bolt beam;
    string msg;
};

// A fineff that triggers a daction; otherwise the daction
// occurs immediately (and then later) -- this might actually
// be too soon in some cases.
class delayed_action_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    virtual void fire() override;

    static void schedule(daction_type action, const string &final_msg)
    {
        final_effect::schedule(new delayed_action_fineff(action, final_msg));
    }
protected:
    delayed_action_fineff(daction_type _action, const string &_final_msg)
        : final_effect(0, 0, coord_def()),
          action(_action), final_msg(_final_msg)
    {
    }
    daction_type action;
    string final_msg;
};

class kirke_death_fineff : public delayed_action_fineff
{
public:
    void fire() override;

    static void schedule(const string &final_msg)
    {
        final_effect::schedule(new kirke_death_fineff(final_msg));
    }
protected:
    kirke_death_fineff(const string & _final_msg)
        : delayed_action_fineff(DACT_KIRKE_HOGS, _final_msg)
    {
    }
};

class rakshasa_clone_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void fire() override;

    static void schedule(const actor *defend, const coord_def &pos)
    {
        final_effect::schedule(new rakshasa_clone_fineff(defend, pos));
    }
protected:
    rakshasa_clone_fineff(const actor *defend, const coord_def &pos)
        : final_effect(0, defend, pos)
    {
    }
    int damage;
};

class bennu_revive_fineff : public final_effect
{
public:
    // Each trigger is from the death of a different bennu---no merging.
    bool mergeable(const final_effect &) const override { return false; }
    void fire() override;

    static void schedule(coord_def pos, int revives, beh_type attitude,
                         unsigned short foe, bool duel, mon_enchant gozag_bribe)
    {
        final_effect::schedule(new bennu_revive_fineff(pos, revives, attitude,
                                                       foe, duel, gozag_bribe));
    }
protected:
    bennu_revive_fineff(coord_def pos, int _revives, beh_type _att,
                        unsigned short _foe, bool _duel,
                        mon_enchant _gozag_bribe)
        : final_effect(0, 0, pos), revives(_revives), attitude(_att), foe(_foe),
          duel(_duel), gozag_bribe(_gozag_bribe)
    {
    }
    int revives;
    beh_type attitude;
    unsigned short foe;
    bool duel;
    mon_enchant gozag_bribe;
};

class avoided_death_fineff : public final_effect
{
public:
    // Each trigger is from the death of a different monster---no merging.
    bool mergeable(const final_effect &) const override { return false; }
    void fire() override;

    static void schedule(monster * mons)
    {
        // pretend to be dead until our revival, to prevent
        // sequencing errors from inadvertently making us change alignment
        const int realhp = mons->hit_points;
        mons->hit_points = -realhp;
        mons->flags |= MF_PENDING_REVIVAL;
        final_effect::schedule(new avoided_death_fineff(mons, realhp));
    }
protected:
    avoided_death_fineff(const actor * _def, int _hp)
        : final_effect(0, _def, coord_def()), hp(_hp)
    {
    }
    int hp;
};

class infestation_death_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &) const override { return false; }
    void fire() override;

    static void schedule(coord_def pos, const string &name)
    {
        final_effect::schedule(new infestation_death_fineff(pos, name));
    }
protected:
    infestation_death_fineff(coord_def pos, const string &_name)
        : final_effect(0, 0, pos), name(_name)
    {
    }
    string name;
};

class make_derived_undead_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &) const override { return false; }
    void fire() override;

    static void schedule(coord_def pos, mgen_data mg, int xl,
                         const string &agent, const string &msg)
    {
        final_effect::schedule(new make_derived_undead_fineff(pos, mg, xl, agent, msg));
    }
protected:
    make_derived_undead_fineff(coord_def pos, mgen_data _mg, int _xl,
                               const string &_agent, const string &_msg)
        : final_effect(0, 0, pos), mg(_mg), experience_level(_xl),
          agent(_agent), message(_msg)
    {
    }
    mgen_data mg;
    int experience_level;
    string agent;
    string message;
};

class mummy_death_curse_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &) const override { return false; }
    void fire() override;

    static void schedule(const actor * attack, string name, killer_type killer, int pow)
    {
        final_effect::schedule(new mummy_death_curse_fineff(attack, name, killer, pow));
    }
protected:
    mummy_death_curse_fineff(const actor * attack, string _name, killer_type _killer, int _pow)
        : final_effect(fixup_attacker(attack), 0, coord_def()), name(_name),
          killer(_killer), pow(_pow)
    {
    }
    const actor *fixup_attacker(const actor *a);

    string name;
    killer_type killer;
    int pow;
};

class summon_dismissal_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &fe) const override;
    void merge(const final_effect &) override;
    void fire() override;

    static void schedule(const actor * _defender)
    {
        final_effect::schedule(new summon_dismissal_fineff(_defender));
    }
protected:
    summon_dismissal_fineff(const actor * _defender)
        : final_effect(0, _defender, coord_def())
    {
    }
};

class spectral_weapon_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &) const override { return false; };
    void fire() override;

    static void schedule(const actor &attack, const actor &defend,
                         const item_def *weapon)
    {
        final_effect::schedule(new spectral_weapon_fineff(attack, defend, weapon));
    }
protected:
    spectral_weapon_fineff(const actor &attack, const actor &defend,
                           const item_def *wpn)
        : final_effect(&attack, &defend, coord_def()), weapon(wpn)
    {
    }

    const item_def *weapon;
};

class lugonu_meddle_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &) const override { return true; };
    void fire() override;

    static void schedule() {
        final_effect::schedule(new lugonu_meddle_fineff());
    }
protected:
    lugonu_meddle_fineff() : final_effect(nullptr, nullptr, coord_def()) { }
};

class jinxbite_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &/*a*/) const override { return false; };
    void fire() override;

    static void schedule(const actor *defend)
    {
        final_effect::schedule(new jinxbite_fineff(defend));
    }
protected:
    jinxbite_fineff(const actor *defend)
        : final_effect(nullptr, defend, coord_def())
    {
    }
};

class beogh_resurrection_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void fire() override;

    static void schedule(bool end_ostracism_only = false)
    {
        final_effect::schedule(new beogh_resurrection_fineff(end_ostracism_only));
    }
protected:
    beogh_resurrection_fineff(bool end_ostracism_only)
        : final_effect(nullptr, nullptr, coord_def()), ostracism_only(end_ostracism_only)
    {
    }
    const bool ostracism_only;
};

class dismiss_divine_allies_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &) const override { return false; }
    void fire() override;

    static void schedule(const god_type god)
    {
        final_effect::schedule(new dismiss_divine_allies_fineff(god));
    }
protected:
    dismiss_divine_allies_fineff(const god_type _god)
        : final_effect(0, 0, coord_def()), god(_god)
    {
    }
    const god_type god;
};

class death_spawn_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &) const override { return false; }
    void fire() override;

    static void schedule(monster_type mon_type, coord_def pos, int dur,
                         int summon_type = SPELL_NO_SPELL)
    {
        mgen_data _mg = mgen_data(mon_type, BEH_HOSTILE, pos,
                                    MHITNOT, MG_FORCE_PLACE);
        _mg.set_summoned(nullptr, summon_type, dur, false, false);
        final_effect::schedule(new death_spawn_fineff(_mg));
    }

    static void schedule(mgen_data mg)
    {
        final_effect::schedule(new death_spawn_fineff(mg));
    }
protected:
    death_spawn_fineff(mgen_data _mg)
        : final_effect(0, 0, _mg.pos), mg(_mg)
    {
    }
    const mgen_data mg;
};

void fire_final_effects();
