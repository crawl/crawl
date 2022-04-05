/**
 * @file
 * @brief Spell miscast class.
**/

#pragma once

#include "enum.h"
#include "monster.h"
#include "mpr.h"
#include "spl-util.h"

enum class nothing_happens
{
    DEFAULT,
    NEVER,
    ALWAYS,
};

enum class miscast_source
{
    zot_trap,
    hell_effect,
    wield,
    melee,
    spell,
    ability,   // Not currently used
    wizard,
    mummy,
    deck,
    god
};

class actor;
// class monster;

struct miscast_source_info
{
    // The source of the miscast
    miscast_source source;
    // If source == miscast_source::god, the god the miscast was created by
    god_type god;
};

class MiscastEffect
{
public:
    MiscastEffect(actor* _target, actor* _act_source,
                  miscast_source_info _source, spell_type _spell, int _pow,
                  int _fail, string _cause = "",
                  nothing_happens _nothing_happens = nothing_happens::DEFAULT,
                  int _lethality_margin = 0,
                  string _hand_str = "", bool _can_plural_hand = true);
    MiscastEffect(actor* _target, actor* _act_source,
                  miscast_source_info _source, spschool _school,
                  int _level, string _cause,
                  nothing_happens _nothing_happens = nothing_happens::DEFAULT,
                  int _lethality_margin = 0,
                  string _hand_str = "", bool _can_plural_hand = true);
    MiscastEffect(actor* _target, actor* _act_source,
                  miscast_source_info _source, spschool _school,
                  int _pow, int _fail, string _cause,
                  nothing_happens _nothing_happens = nothing_happens::DEFAULT,
                  int _lethality_margin = 0,
                  string _hand_str = "", bool _can_plural_hand = true);

    ~MiscastEffect();

    void do_miscast();

private:
    actor* target;
    // May be nullptr.
    actor* act_source;
    // Struct containing the source of the miscast and if applicable, its god
    miscast_source_info special_source;

    string cause;

    spell_type         spell;
    spschool school;

    int pow;
    int fail;
    int level;

private:
    // init() sets this to a proper value
    killer_type kt = KILL_NONE;

    nothing_happens nothing_happens_when;

    int lethality_margin;

    string hand_str;
    bool        can_plural_hand;

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

    bool neither_end_silenced();

    void do_msg(bool suppress_nothing_happens = false);
    bool _ouch(int dam, beam_type flavour = BEAM_NONE);
    bool _explosion();
    bool _big_cloud(cloud_type cl_type, int cloud_pow, int size,
                    int spread_rate = -1);
    bool _paralyse(int dur);
    bool _sleep(int dur);
    bool _create_monster(monster_type what, int abj_deg, bool alert = false);
    bool _send_to_abyss();
    bool _malign_gateway(bool hostile = true);
    void _do_poison(int amount);
    void _malmutate();

    bool avoid_lethal(int dam);

    void _conjuration(int severity);
    void _hexes(int severity);
    void _charms(int severity);
    void _translocation(int severity);
    void _summoning(int severity);
    void _necromancy(int severity);
    void _transmutation(int severity);
    void _fire(int severity);
    void _ice(int severity);
    void _earth(int severity);
    void _air(int severity);
    void _poison(int severity);

    void _zot();
};
