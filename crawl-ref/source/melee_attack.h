#ifndef MELEE_ATTACK_H
#define MELEE_ATTACK_H

#include <list>

#include "attack.h"
#include "fight.h"
#include "random-var.h"

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
    UNAT_LAST_ATTACK = UNAT_TENTACLES,
    NUM_UNARMED_ATTACKS,
};

class melee_attack : public attack
{
public:
    // mon_attack_def stuff
    int       attack_number;
    int       effective_attack_number;

    list<actor*> cleave_targets;
    bool         cleaving;        // additional attack from cleaving
    bool         is_riposte;            // long blade retaliation attack
    bool         is_ieoh_jian_martial;  // Ieoh Jian auxiliary attacks
    coord_def attack_position;

public:
    melee_attack(actor *attacker, actor *defender,
                 int attack_num = -1, int effective_attack_num = -1,
                 bool is_cleaving = false,
                 coord_def attack_pos = coord_def(0, 0));

    // Applies attack damage and other effects.
    bool attack();

    // To-hit is a function of attacker/defender, inherited from attack
    int calc_to_hit(bool random = true) override;

    static void chaos_affect_actor(actor *victim);

private:
    /* Attack phases */
    bool handle_phase_attempted() override;
    bool handle_phase_dodged() override;
    bool handle_phase_hit() override;
    bool handle_phase_damaged() override;
    bool handle_phase_aux(); // specific to melee attacks
    bool handle_phase_killed() override;
    bool handle_phase_end() override;

    /* Combat Calculations */
    bool using_weapon() override;
    int weapon_damage() override;
    int calc_mon_to_hit_base() override;
    int apply_damage_modifiers(int damage, int damage_max) override;
    int calc_damage() override;

    /* Attack effects */
    void check_autoberserk();
    bool check_unrand_effects() override;

    void rot_defender(int amount);

    bool consider_decapitation(int damage_done, int damage_type = -1);
    bool attack_chops_heads(int damage_done, int damage_type, int wpn_brand);
    void decapitate(int dam_type);

    /* Axe cleaving */
    void cleave_setup();
    int cleave_damage_mod(int dam);

    /* Long blade riposte */
    void riposte();

    /* Ieoh Jian martial attacks*/
    int martial_damage_mod(int dam);

    /* Mutation Effects */
    void do_spines();
    void do_passive_freeze();
#if TAG_MAJOR_VERSION == 34
    void do_passive_heat();
#endif
    void emit_foul_stench();
    /* Race Effects */
    void do_minotaur_retaliation();

    /* Brand / Attack Effects */
    bool do_knockback(bool trample = true);

    /* Output methods */
    void set_attack_verb(int damage) override;
    void announce_hit() override;

    /* Misc methods */
    void handle_noise(const coord_def & pos);
private:
    // Monster-attack specific stuff
    bool mons_attack_effects() override;
    void mons_apply_attack_flavour();
    string mons_attack_verb();
    string mons_attack_desc();
    // TODO: Unify do_poison and poison_monster
    bool mons_do_poison();
    void mons_do_napalm();
    void mons_do_eyeball_confusion();
    void mons_do_tendril_disarm();
    void apply_black_mark_effects();
private:
    // Player-attack specific stuff
    // Auxiliary unarmed attacks.
    bool player_aux_unarmed();
    bool player_gets_aux_punch();
    void player_aux_setup(unarmed_attack_type atk);
    bool player_aux_test_hit();
    bool player_aux_apply(unarmed_attack_type atk);

    int  player_apply_misc_modifiers(int damage) override;
    int  player_apply_final_multipliers(int damage) override;

    void player_exercise_combat_skills() override;
    bool player_monattk_hit_effects();
    void attacker_sustain_passive_damage();
    int  staff_damage(skill_type skill);
    void apply_staff_damage();
    void player_stab_check() override;
    bool player_good_stab() override;
    void player_announce_aux_hit();
    string player_why_missed();
    void player_warn_miss();
    void player_weapon_upsets_god();
    void _defender_die();

    // Added in, were previously static methods of fight.cc
    bool _extra_aux_attack(unarmed_attack_type atk);
    int calc_your_to_hit_unarmed(int uattack = UNAT_NO_ATTACK);
    bool _player_vampire_draws_blood(const monster* mon, const int damage,
                                     bool needs_bite_msg = false);
    bool _vamp_wants_blood_from_monster(const monster* mon);

    bool can_reach();
};

#endif
