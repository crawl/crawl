#pragma once

#include "artefact.h"
#include "item-prop-enum.h"
#include "item-status-flag-type.h"
#include "mon-enum.h"
#include "kill-method-type.h"
#include "pronoun-type.h"
#include "spl-util.h" // spschool type definition

// Used throughout inheriting classes, define them here for universal access
const int HIT_WEAK   = 7;
const int HIT_MED    = 18;
const int HIT_STRONG = 36;

const int BACKLIGHT_TO_HIT_BONUS = 5;
const int UMBRA_TO_HIT_MALUS = -3;
const int CONFUSION_TO_HIT_MALUS = -5;
const int TRANSLUCENT_SKIN_TO_HIT_MALUS = -2;

const int BULLSEYE_TO_HIT_DIV = 6;

const int REPEL_MISSILES_EV_BONUS = 15;

class attack
{
// Public Properties
public:
    // General attack properties, set on instantiation or through a normal
    // thread of execution
    actor   *attacker, *defender;
    // Who is morally responsible for the attack? Could be YOU_FAULTLESS
    // or a monster if this is a reflected ranged attack.
    actor *responsible;

    bool    attack_occurred;
    bool    cancel_attack;
    bool    did_hit;
    bool    needs_message;
    bool    attacker_visible, defender_visible;
    bool    perceived_attack, obvious_effect;


    int     to_hit;
    int     damage_done;
    int     special_damage; // TODO: We'll see if we can remove this
    int     aux_damage;     // TODO: And this too

    beam_type special_damage_flavour;

    bool    stab_attempt;
    int     stab_bonus;

    // Fetched/Calculated from the attacker, stored to save execution time
    int             ev_margin;

    // TODO: Expand the implementation of attack_type/attack_flavour to be used
    // by players as well as monsters. It could be a good middle ground for
    // brands and attack types (presently, players don't really have an attack
    // type)
    attack_type     attk_type;
    attack_flavour  attk_flavour;
    int             attk_damage;

    const item_def  *weapon;
    brand_type      damage_brand;
    skill_type      wpn_skill;

    // If weapon is an artefact, its properties.
    artefact_properties_t art_props;

    // If a weapon is an unrandart, its unrandart entry.
    const unrandart_entry *unrand_entry;

    int     attacker_to_hit_penalty;

    // Attack messages
    string     attack_verb, verb_degree;
    string     no_damage_message;
    string     special_damage_message;
    string     aux_attack, aux_verb;

    item_def        *defender_shield;

    bool simu;

// Public Methods
public:
    attack(actor *attk, actor *defn, actor *blame = 0);

    // To-hit is a function of attacker/defender, defined in sub-classes
    virtual int calc_to_hit(bool random);
    int calc_pre_roll_to_hit(bool random);
    virtual int post_roll_to_hit_modifiers(int mhit, bool random);

    // Exact copies of their melee_attack predecessors
    string actor_name(const actor *a, description_level_type desc,
                      bool actor_visible);
    string actor_pronoun(const actor *a, pronoun_type ptyp, bool actor_visible);
    string anon_name(description_level_type desc);
    string anon_pronoun(pronoun_type ptyp);

    // TODO: Definitely want to get rid of this, which we can't really do
    // until we refactor the whole pronoun / desc usage from these lowly
    // classes all the way up to monster/player (and actor) classes.
    string defender_name(bool allow_reflexive);

// Private Properties
    string aux_source;
    kill_method_type kill_type;

// Private Methods
protected:
    virtual void init_attack(skill_type unarmed_skill, int attack_number);

    /* Attack Phases */
    virtual bool handle_phase_attempted();
    virtual bool handle_phase_dodged() = 0;
    virtual bool handle_phase_blocked();
    virtual bool handle_phase_hit() = 0;
    virtual bool handle_phase_damaged();
    virtual bool handle_phase_killed();
    virtual bool handle_phase_end();

    /* Combat Calculations */
    virtual bool using_weapon() const = 0;
    virtual int weapon_damage() const = 0;
    int adjusted_weapon_damage() const;
    virtual int get_weapon_plus();
    virtual int calc_base_unarmed_damage() const;
    virtual int calc_mon_to_hit_base() = 0;
    virtual int apply_damage_modifiers(int damage) = 0;
    int apply_rev_penalty(int damage) const;
    virtual int calc_damage();
    int lighting_effects();
    int test_hit(int to_hit, int ev, bool randomise_ev);
    int apply_defender_ac(int damage, int damage_max = 0,
                          ac_type ac_rule = ac_type::normal) const;
    // Determine if we're blocking (partially or entirely)
    virtual bool attack_shield_blocked(bool verbose);
    virtual bool ignores_shield(bool verbose)
    {
        UNUSED(verbose);
        return false;
    }
    virtual bool apply_damage_brand(const char *what = nullptr);
    void calc_elemental_brand_damage(beam_type flavour,
                                     const char *verb,
                                     const char *what = nullptr);

    /* Weapon Effects */
    virtual bool check_unrand_effects() = 0;

    /* Attack Effects */
    virtual bool mons_attack_effects() = 0;
    void alert_defender();
    bool distortion_affects_defender();
    void antimagic_affects_defender(int pow);
    void pain_affects_defender();
    brand_type random_chaos_brand();
    void drain_defender();
    void drain_defender_speed();
    void maybe_trigger_jinxbite();
    void maybe_trigger_fugue_wail(const coord_def pos);
    void maybe_trigger_autodazzler();

    bool paragon_defends_player();

    virtual int inflict_damage(int dam, beam_type flavour = NUM_BEAMS,
                               bool clean = false);

    /* Output */
    string debug_damage_number();
    string evasion_margin_adverb();

    virtual void set_attack_verb(int damage) = 0;
    virtual void announce_hit() = 0;

    void stab_message();

    void handle_noise(const coord_def & pos);

    string atk_name(description_level_type desc);
    string def_name(description_level_type desc);
    string wep_name(description_level_type desc = DESC_YOUR,
                    iflags_t ignore_flags = ISFLAG_KNOW_PLUSES);

    attack_flavour random_chaos_attack_flavour();
    bool apply_poison_damage_brand();

    virtual int  player_apply_misc_modifiers(int damage);
    virtual int  player_apply_slaying_bonuses(int damage, bool aux);
    virtual int  player_apply_final_multipliers(int damage,
                                                bool /*aux*/ = false);
    virtual int player_apply_postac_multipliers(int damage);

    virtual void player_exercise_combat_skills();

    virtual bool player_good_stab() = 0;
    virtual int  player_stab_weapon_bonus(int damage);
    virtual int  player_stab(int damage);
    virtual void player_stab_check();

private:
    actor &stat_source() const;
};

string attack_strength_punctuation(int dmg);
