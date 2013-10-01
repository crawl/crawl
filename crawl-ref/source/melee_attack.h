#ifndef MELEE_ATTACK_H
#define MELEE_ATTACK_H

#include <list>

#include "attack.h"
#include "fight.h"
#include "random-var.h"

// Forward declaration of the struct
struct mon_attack_def;

enum unarmed_attack_type
{
    UNAT_NO_ATTACK,                    //    0
    UNAT_CONSTRICT,  // put constriction first so octopodes will use it
    UNAT_KICK,
    UNAT_HEADBUTT,
    UNAT_PECK,
    UNAT_TAILSLAP,
    UNAT_PUNCH,
    UNAT_BITE,
    UNAT_PSEUDOPODS,
    UNAT_TENTACLES,
    UNAT_FIRST_ATTACK = UNAT_CONSTRICT,
    UNAT_LAST_ATTACK = UNAT_TENTACLES
};

class melee_attack : public attack
{
public:
    bool      perceived_attack, obvious_effect;

    // mon_attack_def stuff
    int       attack_number;
    int       effective_attack_number;

    bool      skip_chaos_message;

    beam_type special_damage_flavour;

    bool    stab_attempt;
    int     stab_bonus;

    bool         can_cleave;
    list<actor*> cleave_targets;
    bool         cleaving;        // additional attack from cleaving

    // Miscast to cause after special damage is done. If miscast_level == 0
    // the miscast is discarded if special_damage_message isn't empty.
    int    miscast_level;
    int    miscast_type;
    actor* miscast_target;

    bool simu;

public:
    melee_attack(actor *attacker, actor *defender,
                 int attack_num = -1, int effective_attack_num = -1,
                 bool is_cleaving = false);

    // Applies attack damage and other effects.
    bool attack();

    // To-hit is a function of attacker/defender, inherited from attack
    int calc_to_hit(bool random = true);
    int calc_attack_delay(bool random = true, bool scaled = true);

    static void chaos_affect_actor(actor *victim);

private:
    /* Attack phases */
    bool handle_phase_attempted();
    bool handle_phase_dodged();
    bool handle_phase_blocked();
    bool handle_phase_hit();
    bool handle_phase_damaged();
    bool handle_phase_killed();
    bool handle_phase_aux();
    bool handle_phase_end();

    /* Combat Calculations */
    int test_hit(int to_hit, int ev, bool randomise_ev);
    int calc_base_unarmed_damage();
    int calc_base_weapon_damage();
    int calc_damage();
    int calc_stat_to_hit_base();
    int calc_stat_to_dam_base();
    int apply_defender_ac(int damage, int damage_max = 0, bool half_ac = false);

    /* Attack effects */
    void check_autoberserk();
    bool check_unrand_effects();

    bool attack_shield_blocked(bool verbose);
    bool apply_damage_brand();

    void rot_defender(int amount, int immediate = 0);
    void splash_defender_with_acid(int strength);
    bool decapitate_hydra(int damage_done, int damage_type = -1);
    bool chop_hydra_head(int damage_done,
                         int dam_type,
                         brand_type wpn_brand);

    /* Axe cleaving */
    void cleave_setup();
    int cleave_damage_mod(int dam);

    /* Mutation Effects */
    void do_spines();
    void do_passive_freeze();
    void do_passive_heat();
    void emit_foul_stench();
    void tendril_disarm();
    /* Race Effects */
    void do_minotaur_retaliation();

    int inflict_damage(int dam, beam_type flavour = NUM_BEAMS, bool clean = false);

    /* Brand / Attack Effects */
    // Returns true if the defender is banished.
    bool distortion_affects_defender();
    void antimagic_affects_defender();
    void pain_affects_defender();
    void chaos_affects_defender();
    void chaos_affects_attacker();
    brand_type random_chaos_brand();
    void do_miscast();
    bool do_knockback(bool trample = true);
    bool attack_warded_off();
    void drain_defender();

    /* Output methods */
    void adjust_noise();
    void set_attack_verb();
    void announce_hit();

    /* Misc methods */
    void handle_noise(const coord_def & pos);
private:
    // Monster-attack specific stuff
    void mons_apply_attack_flavour();
    string mons_attack_verb();
    string mons_attack_desc();
    // TODO: Unify do_poison and poison_monster
    void mons_do_poison();
    void mons_do_napalm();
    void mons_do_eyeball_confusion();

    attack_flavour random_chaos_attack_flavour();
private:
    // Player-attack specific stuff
    // Auxiliary unarmed attacks.
    bool player_aux_unarmed();
    unarmed_attack_type player_aux_choose_uc_attack();
    void player_aux_setup(unarmed_attack_type atk);
    bool player_aux_test_hit();
    bool player_aux_apply(unarmed_attack_type atk);

    int  player_stat_modify_damage(int damage);
    int  player_aux_stat_modify_damage(int damage);
    int  player_apply_weapon_bonuses(int damage);
    int  player_apply_weapon_skill(int damage);
    int  player_apply_fighting_skill(int damage, bool aux);
    int  player_apply_misc_modifiers(int damage);
    int  player_apply_slaying_bonuses(int damage, bool aux);
    int  player_apply_final_multipliers(int damage);
    int  player_stab_weapon_bonus(int damage);
    int  player_stab(int damage);

    void player_exercise_combat_skills();
    bool player_monattk_hit_effects();
    void attacker_sustain_passive_damage();
    int  staff_damage(skill_type skill);
    void apply_staff_damage();
    void player_stab_check();
    random_var player_weapon_speed();
    random_var player_unarmed_speed();
    void player_announce_aux_hit();
    string player_why_missed();
    void player_warn_miss();
    void player_weapon_upsets_god();
    void _defender_die();

    // Output methods
    void stab_message();

    // Added in, were previously static methods of fight.cc
    bool _tran_forbid_aux_attack(unarmed_attack_type atk);
    bool _extra_aux_attack(unarmed_attack_type atk, bool is_uc);
    int calc_your_to_hit_unarmed(int uattack = UNAT_NO_ATTACK,
                                 bool vampiric = false);
    bool _player_vampire_draws_blood(const monster* mon, const int damage,
                                     bool needs_bite_msg = false,
                                     int reduction = 1);
    bool _vamp_wants_blood_from_monster(const monster* mon);

    bool can_reach();
};

#endif
