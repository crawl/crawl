/**
 * @file
 * @brief Spell miscast class.
**/

#ifndef SPL_MISCAST_H
#define SPL_MISCAST_H

#include "enum.h"

#include "monster.h"
#include "mpr.h"
#include "spl-util.h"

enum nothing_happens_when_type
{
    NH_DEFAULT,
    NH_NEVER,
    NH_ALWAYS,
};

class actor;
// class monster;

class MiscastEffect
{
public:
    MiscastEffect(actor* _target, int _source, spell_type _spell, int _pow,
                  int _fail, string _cause = "",
                  nothing_happens_when_type _nothing_happens = NH_DEFAULT,
                  int _lethality_margin = 0,
                  string _hand_str = "", bool _can_plural_hand = true);
    MiscastEffect(actor* _target, int _source, spschool_flag_type _school,
                  int _level, string _cause,
                  nothing_happens_when_type _nothing_happens = NH_DEFAULT,
                  int _lethality_margin = 0,
                  string _hand_str = "", bool _can_plural_hand = true);
    MiscastEffect(actor* _target, int _source, spschool_flag_type _school,
                  int _pow, int _fail, string _cause,
                  nothing_happens_when_type _nothing_happens = NH_DEFAULT,
                  int _lethality_margin = 0,
                  string _hand_str = "", bool _can_plural_hand = true);

    ~MiscastEffect();

    void do_miscast();

private:
    actor* target;
    int    source;

    string cause;

    spell_type         spell;
    spschool_flag_type school;

    int pow;
    int fail;
    int level;

private:
    kill_category kc;
    killer_type   kt;

    nothing_happens_when_type nothing_happens_when;

    int lethality_margin;

    string hand_str;
    bool        can_plural_hand;

    int    kill_source;
    actor* act_source;
    actor* guilty;

    bool source_known;
    bool target_known;

    bolt beam;

    string all_msg;
    string you_msg;
    string mon_msg;
    string mon_msg_seen;
    string mon_msg_unseen;

    msg_channel_type msg_ch;

    int  sound_loudness;

    int  recursion_depth;
    bool did_msg;

private:
    void init();
    string get_default_cause(bool attribute_to_user) const;

    monster* target_as_monster()
    {
        return dynamic_cast<monster* >(target);
    }

    monster* source_as_monster()
    {
        return dynamic_cast<monster* >(act_source);
    }

    bool neither_end_silenced();

    void do_msg(bool suppress_nothing_happens = false);
    bool _ouch(int dam, beam_type flavour = BEAM_NONE);
    bool _explosion();
    bool _big_cloud(cloud_type cl_type, int cloud_pow, int size,
                    int spread_rate = -1);
    bool _lose_stat(stat_type which_stat, int8_t stat_loss);
    void _potion_effect(potion_type pot_eff, int pot_pow);
    bool _create_monster(monster_type what, int abj_deg, bool alert = false);
    bool _send_to_abyss();
    bool _malign_gateway();
    void _do_poison(int amount);

    bool avoid_lethal(int dam);

    void _conjuration(int severity);
    void _enchantment(int severity);
    void _translocation(int severity);
    void _summoning(int severity);
    void _divination_you(int severity);
    void _divination_mon(int severity);
    void _necromancy(int severity);
    void _transmutation(int severity);
    void _fire(int severity);
    void _ice(int severity);
    void _earth(int severity);
    void _air(int severity);
    void _poison(int severity);

    void _zot();
};

#endif
