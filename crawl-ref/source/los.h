/*
 *  File:       los.h
 *  Summary:    Line-of-sight algorithm.
 */

#ifndef LOS_H
#define LOS_H

#include "los_def.h"
#include "losparam.h"

#define EPSILON_VALUE 0.00001

bool double_is_zero(const double x);

void set_los_radius(int r);
int get_los_radius_sq(); // XXX

struct ray_def;
bool find_ray(const coord_def& source, const coord_def& target,
              ray_def& ray, const opacity_func &opc = opc_solid,
              const bounds_func &bds = bds_default, bool cycle = false);
bool exists_ray(const coord_def& source, const coord_def& target,
                const opacity_func &opc = opc_solid,
                const bounds_func &bds = bds_default);
dungeon_feature_type ray_blocker(const coord_def& source, const coord_def& target);

void fallback_ray(const coord_def& source, const coord_def& target,
                  ray_def& ray);

int num_feats_between(const coord_def& source, const coord_def& target,
                      dungeon_feature_type min_feat,
                      dungeon_feature_type max_feat,
                      bool exclude_endpoints = true,
                      bool just_check = false);
bool cell_see_cell(const coord_def& p1, const coord_def& p2);

void clear_rays_on_exit();
void losight(env_show_grid& sh, const coord_def& center,
             const opacity_func &opc = opc_default,
             const bounds_func &bds = bds_default);
void losight(env_show_grid& sh, const los_param& param);

void calc_show_los();
bool see_cell(const env_show_grid &show,
              const coord_def &c,
              const coord_def &pos );
bool observe_cell(const coord_def &p);
bool see_cell_no_trans( const coord_def &p );
bool trans_wall_blocking( const coord_def &p );

#endif
