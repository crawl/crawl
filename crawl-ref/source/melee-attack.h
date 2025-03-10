#pragma once

#include <list>

#include "attack.h"
#include "fight.h"
#include "item-prop-enum.h" // vorpal_damage_type
#include "random-var.h"
#include "tag-version.h"

enum unarmed_attack_type
{
    UNAT_NO_ATTACK,                    //    0
    UNAT_CONSTRICT,  // put constriction first so octopodes will use it
    UNAT_KICK,
    UNAT_HEADBUTT,
    UNAT_PECK,
    UNAT_TAILSLAP,
    UNAT_TOUCH,
    UNAT_PUNCH,
    UNAT_BITE,
    UNAT_PSEUDOPODS,
    UNAT_TENTACLES,
    UNAT_MAW,
    UNAT_EXECUTIONER_BLADE,
    UNAT_FUNGAL_FISTICLOAK,
    UNAT_FIRST_ATTACK = UNAT_CONSTRICT,
    UNAT_LAST_ATTACK = UNAT_FUNGAL_FISTICLOAK,
    NUM_UNARMED_ATTACKS,
};

class melee_attack : public attack
{
public:
    // For monsters, attack_number indicates which of their attacks is used.
    // For players, attack_number is used when using off-hand weapons, and
    // indicates whether this is their primary attack or their follow-up.
    // Note that we randomize which hand is used for the primary attack.
    int       attack_number;
    int       effective_attack_number;

    // A tally of all direct weapon + brand damage inflicted by this attack
    // (including damage against cleave targets, both hits of quick blades,
    // and aux attacks).
    int       total_damage_done;

    list<actor*> cleave_targets;
    bool         cleaving;        // additional attack from cleaving
    bool         is_multihit;     // quick blade follow-up attack
    bool         is_riposte;      // fencers' retaliation attack
    bool         is_projected;    // projected weapon spell attack
    int          charge_pow;      // electric charge bonus damage
    bool         never_cleave;    // if this attack shouldn't trigger cleave
                                  // followups, but still do 100% damage
    int          dmg_mult;        // percentage multiplier to max damage roll
    int          flat_dmg_bonus;  // flat slaying to add to this attack
    bool         never_prompt;    // whether to skip prompting the player about
                                  // harming allies
    wu_jian_attack_type wu_jian_attack;
    int wu_jian_number_of_targets;
    coord_def attack_position;
    item_def *mutable_wpn;

public:
    melee_attack(actor *attacker, actor *defender,
                 int attack_num = 0, int effective_attack_num = 0);
    void set_weapon(item_def *weapon, bool offhand = false);

    bool launch_attack_set(bool allow_rev = true);
    bool attack();
    int calc_to_hit(bool random) override;
    int post_roll_to_hit_modifiers(int mhit, bool random) override;

    bool would_prompt_player();

    static void chaos_affect_actor(actor *victim);

    bool player_do_aux_attack(unarmed_attack_type atk);
    bool do_drag();

private:
    /* Attack phases */
    bool handle_phase_attempted() override;
    bool handle_phase_blocked() override;
    bool handle_phase_dodged() override;
    bool handle_phase_hit() override;
    bool handle_phase_damaged() override;
    bool handle_phase_aux(); // specific to melee attacks
    bool handle_phase_killed() override;
    bool handle_phase_end() override;

    /* Combat Calculations */
    bool using_weapon() const override;
    int weapon_damage() const override;
    int calc_mon_to_hit_base() override;
    int apply_damage_modifiers(int damage) override;
    int calc_damage() override;
    bool apply_damage_brand(const char *what = nullptr) override;

    /* Attack effects */
    void check_autoberserk();
    bool check_unrand_effects() override;

    void rot_defender(int amount);

    bool consider_decapitation(int damage_done);
    bool attack_chops_heads(int damage_done);
    void decapitate();

    bool run_attack_set();
    bool swing_with(item_def &weapon, bool offhand);
    void force_cleave(item_def &weapon, coord_def target);

    /* Axe cleaving */
    void cleave_setup();
    int cleave_damage_mod(int dam);

    /* Long blade riposte */
    void riposte();

    /* Wu Jian martial attacks*/
    int martial_damage_mod(int dam);

    /* Mutation Effects */
    void do_spines();
    void do_passive_freeze();
    void do_foul_flame();
    void emit_foul_stench();

    /* Divine Effect */
    void do_fiery_armour_burn();

    /* Retaliation Effects */
    void do_minotaur_retaliation();
    void maybe_riposte();

    /* Item Effects */
    void do_starlight();

    /* Brand / Attack Effects */
    bool do_knockback(bool slippery);

    /* Output methods */
    void set_attack_verb(int damage) override;
    void announce_hit() override;
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
    void apply_sign_of_ruin_effects();
    void do_ooze_engulf();
    void try_parry_disarm();
    void do_vampire_lifesteal();
private:
    // Player-attack specific stuff
    // Auxiliary unarmed attacks.
    bool player_do_aux_attacks();
    void player_aux_setup(unarmed_attack_type atk);
    bool player_aux_test_hit();
    bool player_aux_apply(unarmed_attack_type atk);

    int  player_apply_misc_modifiers(int damage) override;
    int  player_apply_final_multipliers(int damage, bool aux = false) override;
    int  player_apply_postac_multipliers(int damage) override;

    void player_exercise_combat_skills() override;
    bool player_monattk_hit_effects();
    void attacker_sustain_passive_damage();
    int  staff_damage(stave_type staff) const;
    string staff_message(stave_type staff, int damage) const;
    bool apply_staff_damage();
    void player_stab_check() override;
    bool player_good_stab() override;
    void player_announce_aux_hit();
    string charge_desc();
    string weapon_desc();
    void player_warn_miss();
    void player_weapon_upsets_god();
    bool bad_attempt();
    bool player_unrand_bad_attempt(const item_def *offhand,
                                   bool check_only = false);
    void _defender_die();
    void handle_spectral_brand();

    // Added in, were previously static methods of fight.cc
    bool _extra_aux_attack(unarmed_attack_type atk);

    bool can_reach(int dist);

    item_def *offhand_weapon() const;

    // XXX: set up a copy constructor instead?
    void copy_to(melee_attack &other);

    vorpal_damage_type damage_type;

    // Is a special stab against a sleeping monster by a Dithmenos player
    // shadow (affects messaging).
    bool is_shadow_stab;
};

string aux_attack_desc(unarmed_attack_type unat, int force_damage = -1);
string mut_aux_attack_desc(mutation_type mut);
vector<string> get_player_aux_names();

bool coglin_spellmotor_attack();
bool spellclaws_attack(int spell_level);
