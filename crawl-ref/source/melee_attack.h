#ifndef MELEE_ATTACK_H
#define MELEE_ATTACK_H

#include "artefact.h"
#include "attack.h"
#include "fight.h"
#include "mon-enum.h"
#include "random-var.h"
#include "random.h"

enum unarmed_attack_type
{
    UNAT_NO_ATTACK,                    //    0
    UNAT_KICK,
    UNAT_HEADBUTT,
    UNAT_TAILSLAP,
    UNAT_PUNCH,
    UNAT_BITE,
    UNAT_PSEUDOPODS,
    UNAT_FIRST_ATTACK = UNAT_KICK,
    UNAT_LAST_ATTACK = UNAT_PSEUDOPODS,
};

class melee_attack : public attack
{
public:
    bool      perceived_attack, obvious_effect;

    int       attack_number;

    int       extra_noise;

    bool      skip_chaos_message;

    beam_type special_damage_flavour;

    // TODO: Remove entirely OR move it into attack
    item_def  *shield;

    // Armour penalties?
    // Adjusted EV penalty for body armour and shields.
    // TODO: Replaced with attack methods
    int       player_body_armour_penalty;
    int       player_shield_penalty;

    // Combined to-hit penalty from armour and shield.
    int       player_armour_tohit_penalty;
    int       player_shield_tohit_penalty;

    // TODO: Replaced with attack::unarmed_capable
    bool      can_do_unarmed;

    // Miscast to cause after special damage is done.  If miscast_level == 0
    // the miscast is discarded if special_damage_message isn't empty.
    int    miscast_level;
    int    miscast_type;
    actor* miscast_target;

public:
    melee_attack(actor *attacker, actor *defender,
                 bool allow_unarmed = true, int attack_num = -1);

    // Applies attack damage and other effects.
    bool attack();

    // TODO: move base calc to actor/player/monster, use attack::adjust_to_hit
    int  calc_to_hit(bool random = true);
    random_var player_calc_attack_delay();

    static void chaos_affect_actor(actor *victim);

private:
    void init_attack();
    bool is_banished(const actor *) const;
    void check_autoberserk();
    bool check_unrand_effects();
    void emit_nodmg_hit_message();
    void identify_mimic(actor *mon);

    bool attack_shield_blocked(bool verbose);
    bool apply_damage_brand();
    int fire_res_apply_cerebov_downgrade(int res);
    void drain_defender();
    void rot_defender(int amount, int immediate = 0);
    void splash_defender_with_acid(int strength);
    void splash_monster_with_acid(int strength);
    bool decapitate_hydra(int damage_done, int damage_type = -1);
    bool chop_hydra_head(int damage_done,
                          int dam_type,
                          int wpn_brand);

    // Returns true if the defender is banished.
    bool distortion_affects_defender();

    void antimagic_affects_defender();
    void pain_affects_defender();
    void chaos_affects_defender();
    void chaos_affects_attacker();
    void chaos_killed_defender(monster* def_copy);
    int  random_chaos_brand();
    void do_miscast();

    void handle_noise(const coord_def & pos);

    // Handle specific attack phases (mons and player)
    void respond_to_attack_phase(phase);

    // Added from fight.cc, were static, should be removed
    int _modify_blood_amount(const int damage, const int dam_type);
    bool _move_stairs();
private:
    // Monster-attack specific stuff
    bool mons_attack_you();
    bool mons_attack_mons();
    int  mons_to_hit();
    bool mons_self_destructs();
    bool mons_attack_warded_off();
    int mons_attk_delay();
    int mons_calc_damage(const mon_attack_def &attk);
    bool do_trample();
    void mons_apply_attack_flavour(const mon_attack_def &attk);
    int mons_apply_defender_ac(int damage, int damage_max);
    bool mons_perform_attack();
    void mons_perform_attack_rounds();
    void mons_check_attack_perceived();
    std::string mons_attack_verb(const mon_attack_def &attk);
    std::string mons_attack_desc(const mon_attack_def &attk);
    void mons_announce_hit(const mon_attack_def &attk);
    void mons_announce_dud_hit(const mon_attack_def &attk);
    void mons_set_weapon(const mon_attack_def &attk);
    void mons_do_poison(const mon_attack_def &attk);
    void mons_do_napalm();
    std::string mons_defender_name();
    void wasp_paralyse_defender();
    void mons_do_passive_freeze();
    void mons_do_spines();
    void mons_do_eyeball_confusion();

    mon_attack_flavour random_chaos_attack_flavour();

    // Added in, were previously static functions in fight.cc, most should
    // be removed and placed in other classes (monster of player, mostly)
    void _find_remains(monster* mon, int &corpse_class, int &corpse_index,
                       item_def &fake_corpse, int &last_item,
                       std::vector<int> items);
    bool _make_zombie(monster* mon, int corpse_class, int corpse_index,
                      item_def &fake_corpse, int last_item);
    int test_melee_hit(int to_hit, int ev, defer_rand& r);
    void mons_lose_attack_energy(monster* attacker, int wpn_speed,
                                 int which_attack, int effective_attack);
private:
    // Player-attack specific stuff
    bool player_attack();

    // Auxiliary unarmed attacks.
    bool player_aux_unarmed();
    unarmed_attack_type player_aux_choose_baseattack();
    void player_aux_setup(unarmed_attack_type atk);
    bool player_aux_test_hit();
    bool player_aux_apply(unarmed_attack_type atk);

    int  player_stat_modify_damage(int damage);
    int  player_aux_stat_modify_damage(int damage);
    int  player_to_hit(bool random_factor);
    void calc_player_armour_shield_tohit_penalty(bool random_factor);
    void player_apply_attack_delay();
    int  player_apply_weapon_bonuses(int damage);
    int  player_apply_weapon_skill(int damage);
    int  player_apply_fighting_skill(int damage, bool aux);
    int  player_apply_misc_modifiers(int damage);
    int  player_apply_monster_ac(int damage);
    void player_weapon_auto_id();
    int  player_stab_weapon_bonus(int damage);
    int  player_stab(int damage);
    int  player_weapon_type_modify(int damage);

    int  player_hits_monster();
    int  player_calc_base_weapon_damage();
    int  player_calc_base_unarmed_damage();
    void player_exercise_combat_skills();
    bool player_monattk_hit_effects();
    void player_sustain_passive_damage();
    int  player_staff_damage(skill_type skill);
    void player_apply_staff_damage();
    bool player_check_monster_died();
    void player_calc_hit_damage();
    void player_stab_check();
    random_var player_weapon_speed();
    random_var player_unarmed_speed();
    void player_announce_aux_hit();
    void player_announce_hit();
    std::string player_why_missed();
    void player_warn_miss();
    void player_check_weapon_effects();
    void _monster_die(monster* mons, killer_type killer, int killer_index);

    // Output methods
    void stab_message();

    // Added in, were previously static methods of fight.cc
    bool _tran_forbid_aux_attack(unarmed_attack_type atk);
    bool _extra_aux_attack(unarmed_attack_type atk);
    bool player_fights_well_unarmed(int heavy_armour_penalty);
    int calc_your_to_hit_unarmed(int uattack = UNAT_NO_ATTACK,
                                 bool vampiric = false);
    int calc_stat_to_hit_base();
    void _steal_item_from_player(monster* mon);
    bool _player_vampire_draws_blood(const monster* mon, const int damage,
                                     bool needs_bite_msg = false,
                                     int reduction = 1);
    int calc_stat_to_dam_base();
    bool _vamp_wants_blood_from_monster(const monster* mon);
};

#endif
