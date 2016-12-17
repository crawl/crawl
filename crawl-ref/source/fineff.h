/**
 * @file
 * @brief Brand/ench/etc effects that might alter something in an
 *             unexpected way.
**/

#ifndef FINEFF_H
#define FINEFF_H

#include "mon-util.h"

class final_effect
{
public:
    virtual ~final_effect() {}

    virtual bool mergeable(const final_effect &a) const = 0;
    virtual void merge(const final_effect &a)
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

    static void schedule(const actor *blinker)
    {
        final_effect::schedule(new blink_fineff(blinker));
    }
protected:
    blink_fineff(const actor *blinker)
        : final_effect(0, blinker, coord_def())
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

class shock_serpent_discharge_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override;
    void merge(const final_effect &a) override;
    void fire() override;

    static void schedule(const actor *serpent, const actor &oppressor,
                         coord_def pos, int pow)
    {
        final_effect::schedule(new shock_serpent_discharge_fineff(serpent,
                                                                  oppressor,
                                                                  pos, pow));
    }
protected:
    shock_serpent_discharge_fineff(const actor *serpent, const actor &rudedude,
                                   coord_def pos, int pow)
        : final_effect(0, serpent, coord_def()), oppressor(rudedude),
          position(pos), power(pow),
        attitude(mons_attitude(*serpent->as_monster()))
    {
    }
    const actor &oppressor;
    coord_def position;
    int power;
    mon_attitude_type attitude;
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
    bool mergeable(const final_effect &a) const override { return false; }
    void fire() override;

    static void schedule(coord_def pos, int revives, beh_type attitude,
                         unsigned short foe)
    {
        final_effect::schedule(new bennu_revive_fineff(pos, revives, attitude, foe));
    }
protected:
    bennu_revive_fineff(coord_def pos, int _revives, beh_type _att,
                        unsigned short _foe)
        : final_effect(0, 0, pos), revives(_revives), attitude(_att), foe(_foe)
    {
    }
    int revives;
    beh_type attitude;
    unsigned short foe;
};

class infestation_death_fineff : public final_effect
{
public:
    bool mergeable(const final_effect &a) const override { return false; }
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

void fire_final_effects();

#endif
