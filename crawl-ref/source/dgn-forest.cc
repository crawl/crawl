#include "AppHdr.h"

#include "coordit.h"
#include "dungeon.h"
#include "dgn-height.h"
#include "mgen_data.h"
#include "mon-place.h"
#include "random.h"
#include "tiledef-dngn.h"
#include "tileview.h"

static void _forest_clearings(int depth_multiplier)
{
    const int margin = 9;
    const int yinterval = 18;
    const int xinterval = 18;
    const int fuzz = 9;
    const int rowsize = (GXM-1)/xinterval;
    const int colsize = (GYM-1)/yinterval;
    const int max_clearings = rowsize*colsize;
    coord_def clearings[max_clearings];
    int i = 0, j = 0;
    for (int y = margin; y < GYM - margin - 1; y += yinterval)
    {
        for (int x = margin; x < GXM - margin - 1; x += xinterval)
        {
            const coord_def p(x, y);
            const coord_def c(dgn_random_point_from(p, fuzz, margin));
            /*const coord_def center_delta(grdc - c);*/
            /*const int depth = 1600 - center_delta.abs();*/
            /*const int depth_offset = depth * 10 / 1600;*/
            clearings[i] = c;
            dgn_island_centred_at(c,
                                  random_range(60, 
                                      (90 - 4*depth_multiplier)),
                                  random_range(1, 4 - depth_multiplier/2),
                                  int_range(20, 25), margin);
            i++;
        }
    }
    
    dgn_smooth_heights();

    for (i = 0; i < max_clearings; i++)
    {
        for (j = i+1; j < max_clearings; j++)
        {
            if ((!((abs(i - j) == 1) && ((j % rowsize) != 0))) &&
                (abs(i - j) != rowsize)) continue; // connect only adjacent
                                                   // clearings

            const int newheight = 20;
            int dx, dy;
            coord_def path_pos(clearings[i]), extra_pos;
            while (path_pos != clearings[j]) {
                dgn_height_at(path_pos) += newheight;
                extra_pos = dgn_random_point_from(path_pos, 1);
                dgn_height_at(extra_pos) += newheight;
                dx = clearings[j].x - path_pos.x;
                dy = clearings[j].y - path_pos.y;
                if (abs(dx) > abs(dy))
                {
                    path_pos.x += (dx / abs(dx));
                    path_pos.y += random_range(-1, 1);
                    if (path_pos.y < 0) path_pos.y = 0;
                    if (path_pos.y > GYM-1) path_pos.y = GYM-1;
                }
                else
                {
                    path_pos.y += (dy / abs(dy));
                    path_pos.x += random_range(-1, 1);
                    if (path_pos.x < 0) path_pos.x = 0;
                    if (path_pos.x > GXM-1) path_pos.x = GXM-1;
                }
            }
        }
    }
}

static dungeon_feature_type _forest_feature_for_height(int height)
{
    return ((height >= 1) ||
            ((height >= -5) && x_chance_in_y(1 - height, 6)))
             ? DNGN_FLOOR : DNGN_TREE;
}

static void _forest_apply_features(int margin)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        const coord_def c(*ri);
        if (!map_masked(c, MMT_VAULT))
        {
            int height = dgn_height_at(c);

            if (c.x < margin || c.y < margin || c.x >= GXM - margin
                || c.y >= GYM - margin)
                grd(c) = DNGN_TREE;
            else
               grd(c) = _forest_feature_for_height(height);

            if ((height > 1) &&
                (x_chance_in_y(28 - height, 27)))
                mons_place(mgen_data::hostile_at(
                    coinflip() ? MONS_PLANT : MONS_BUSH, "", false, 0, 0, c));
            else if ((height >= 30) ||
                     ((height > 24) &&
                      x_chance_in_y(height - 24, 6)))
            {
                dgn_set_grid_colour_at(c, BROWN);
                env.tile_flv(c).floor = TILE_FLOOR_DIRT + random2(3);
            }
        }
    }
}

void dgn_build_forest_level()
{
    env.level_build_method += " forest";
    env.level_layout_types.insert("forest");

    const int forest_depth = you.depth - 1;
    dgn_initialise_heightmap(-17);
    _forest_clearings(forest_depth);
    _forest_apply_features(2);
    env.heightmap.reset(NULL);

    dgn_place_stone_stairs();
    process_disconnected_zones(0, 0, GXM - 1, GYM - 1, true, DNGN_TREE);
}
