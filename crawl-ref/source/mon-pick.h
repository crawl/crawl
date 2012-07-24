/**
 * @file
 * @brief Functions used to help determine which monsters should appear.
**/


#ifndef MONPICK_H
#define MONPICK_H

#include "externs.h"

#define DEPTH_NOWHERE 999

int mons_rarity(monster_type mcls, const level_id &place = level_id::current());
int mons_level(monster_type mcls, const level_id &place = level_id::current());

void debug_monpick();

int mons_null_level(monster_type mcls);
int mons_null_rare(monster_type mcls);

int mons_abyss_level(monster_type mcls);
int mons_abyss_rare(monster_type mcls);
int mons_pan_level(monster_type mcls);
int mons_pan_rare(monster_type mcls);

int mons_cocytus_level(monster_type mcls);
int mons_cocytus_rare(monster_type mcls);
int mons_crypt_level(monster_type mcls);
int mons_crypt_rare(monster_type mcls);
int mons_dis_level(monster_type mcls);
int mons_dis_rare(monster_type mcls);
int mons_gehenna_level(monster_type mcls);
int mons_gehenna_rare(monster_type mcls);
int mons_hallblade_level(monster_type mcls);
int mons_hallblade_rare(monster_type mcls);
int mons_hallelf_level(monster_type mcls);
int mons_hallelf_rare(monster_type mcls);
int mons_hallzot_level(monster_type mcls);
int mons_hallzot_rare(monster_type mcls);
int mons_lair_level(monster_type mcls);
int mons_lair_rare(monster_type mcls);
int mons_dwarf_level(monster_type mcls);
int mons_dwarf_rare(monster_type mcls);
int mons_mineorc_level(monster_type mcls);
int mons_mineorc_rare(monster_type mcls);
int mons_pitslime_level(monster_type mcls);
int mons_pitslime_rare(monster_type mcls);
int mons_pitsnake_level(monster_type mcls);
int mons_pitsnake_rare(monster_type mcls);
int mons_dungeon_level(monster_type mcls);
int mons_dungeon_rare(monster_type mcls);
int mons_swamp_level(monster_type mcls);
int mons_swamp_rare(monster_type mcls);
int mons_shoals_level(monster_type mcls);
int mons_shoals_rare(monster_type mcls);
int mons_spidernest_level(monster_type mcls);
int mons_spidernest_rare(monster_type mcls);
int mons_forest_level(monster_type mcls);
int mons_forest_rare(monster_type mcls);
int mons_tartarus_level(monster_type mcls);
int mons_tartarus_rare(monster_type mcls);
int mons_tomb_level(monster_type mcls);
int mons_tomb_rare(monster_type mcls);
int mons_vaults_level(monster_type mcls);
int mons_vaults_rare(monster_type mcls);
int mons_vestibule_level(monster_type mcls);
int mons_vestibule_rare(monster_type mcls);
int mons_sewer_level(monster_type mcls);
int mons_sewer_rare(monster_type mcls);
int mons_volcano_level(monster_type mcls);
int mons_volcano_rare(monster_type mcls);
int mons_icecave_level(monster_type mcls);
int mons_icecave_rare(monster_type mcls);
int mons_bailey_level(monster_type mcls);
int mons_bailey_rare(monster_type mcls);
int mons_ossuary_level(monster_type mcls);
int mons_ossuary_rare(monster_type mcls);

#endif
