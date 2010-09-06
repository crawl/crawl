/*
 *  File:       los.h
 *  Summary:    Line-of-sight algorithm.
 */

#ifndef LOS_H
#define LOS_H

#include "coord-circle.h"
#include "losparam.h"

class circle_def;
struct ray_def;

#define EPSILON_VALUE 0.00001

bool double_is_zero(const double x);

void set_los_radius(int r);
int get_los_radius_sq(); // XXX

// Default bounds that tracks global LOS radius.
#define BDS_DEFAULT (circle_def())
// Default bounds for find_ray, used for beams.
#define BDS_CONSTANT (circle_def(LOS_MAX_RANGE, C_ROUND))

bool find_ray(const coord_def& source, const coord_def& target,
              ray_def& ray, const opacity_func &opc = opc_solid,
              const circle_def &bds = BDS_CONSTANT, bool cycle = false);
bool exists_ray(const coord_def& source, const coord_def& target,
                const opacity_func &opc = opc_solid,
                const circle_def &bds = BDS_CONSTANT);
dungeon_feature_type ray_blocker(const coord_def& source, const coord_def& target);

void fallback_ray(const coord_def& source, const coord_def& target,
                  ray_def& ray);

int num_feats_between(const coord_def& source, const coord_def& target,
                      dungeon_feature_type min_feat,
                      dungeon_feature_type max_feat,
                      bool exclude_endpoints = true,
                      bool just_check = false);
bool cell_see_cell(const coord_def& p1, const coord_def& p2);

typedef SquareArray<bool, LOS_MAX_RANGE> los_grid;

void clear_rays_on_exit();
void losight(los_grid& sh, const coord_def& center,
             const opacity_func &opc = opc_default,
             const circle_def &bds = BDS_DEFAULT);
void losight(los_grid& sh, const los_param& param);

void los_actor_moved(const actor* act, const coord_def& oldpos);
void los_monster_died(const monster* mon);
void los_terrain_changed(const coord_def& p);
void los_cloud_changed(const coord_def& p);
void los_changed();
#endif
