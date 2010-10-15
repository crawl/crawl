/*
 *  File:       wiz-you.h
 *  Summary:    Player related wizard functions.
 *  Written by: Linley Henzell and Jesse Jones
 */

#ifndef WIZYOU_H
#define WIZYOU_H

void wizard_cast_spec_spell(void);
void wizard_cast_spec_spell_name(void);
void wizard_heal(bool super_heal);
void wizard_set_hunger_state();
void wizard_set_piety();
void wizard_exercise_skill(void);
void wizard_set_skill_level(void);
void wizard_set_all_skills(void);
void wizard_change_species(void);
void wizard_set_xl();
bool wizard_add_mutation();
void wizard_get_religion(void);
void wizard_set_stats(void);
void wizard_edit_durations(void);
void wizard_get_god_gift ();
void wizard_toggle_xray_vision();
void wizard_god_wrath();

#endif
