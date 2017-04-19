/**
 * @file
 * @brief Player related wizard functions.
**/

#pragma once

#include "enum.h"
#include "job-type.h"
#include "species-type.h"

#ifdef WIZARD
#define FREEZE_TIME_KEY "freeze_time"

void wizard_cast_spec_spell();
void wizard_memorise_spec_spell();
void wizard_heal(bool super_heal);
void wizard_set_hunger_state();
void wizard_set_gold();
void wizard_set_piety();
void wizard_set_piety_to(int new_piety, bool force = false);
void wizard_exercise_skill();
void wizard_set_abyss();
void wizard_set_skill_level(skill_type skill = SK_NONE);
void wizard_set_all_skills();
void wizard_change_species();
void wizard_set_xl(bool change_skills = false);
void set_xl(const int newxl, const bool train, const bool silent=true);
bool wizard_add_mutation();
void wizard_set_stats();
void wizard_edit_durations();
void wizard_list_props();
void wizard_get_god_gift();
void wizard_toggle_xray_vision();
void wizard_freeze_time();
void wizard_god_wrath();
void wizard_god_mollify();
void wizard_transform();
void wizard_join_religion();
species_type find_species_from_string(const string &species_str);
job_type find_job_from_string(const string &job_str);
void wizard_change_job_to(job_type job);
void wizard_xom_acts();
#endif
