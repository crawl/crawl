/**
 * @file
 * @brief Look-up functions for player tiles.
**/

#pragma once

#ifdef USE_TILE

#include "rltiles/tiledef_defines.h"

struct dolls_data;
struct item_def;

// Player equipment lookup
tileidx_t tilep_equ_weapon(const item_def &item);
tileidx_t tilep_equ_shield(const item_def &item);
tileidx_t tilep_equ_armour(const item_def &item);
tileidx_t tilep_equ_cloak(const item_def &item);
tileidx_t tilep_equ_helm(const item_def &item);
tileidx_t tilep_equ_gloves(const item_def &item);
tileidx_t tilep_equ_boots(const item_def &item);

tileidx_t tileidx_player();
bool is_player_tile(tileidx_t tile, tileidx_t base_tile);

tileidx_t tilep_species_to_base_tile(int sp, int level);

void tilep_draconian_init(int sp, int level, tileidx_t *base,
                          tileidx_t *head, tileidx_t *wing);
void tilep_race_default(int sp, int level, dolls_data *doll);
void tilep_job_default(int job, dolls_data *doll);
void tilep_calc_flags(const dolls_data &data, int flag[]);

void tilep_scan_parts(char *fbuf, dolls_data &doll, int species, int level);
void tilep_print_parts(char *fbuf, const dolls_data &doll);

#endif
