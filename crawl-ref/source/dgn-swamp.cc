#include "AppHdr.h"

#include "dgn-swamp.h"

#include "coordit.h"
#include "dungeon.h"
#include "dgn-height.h"
#include "random.h"

static void _swamp_slushy_patches(int depth_multiplier)
{
    const int margin = 11;
    const int yinterval = 4;
    const int xinterval = 4;
    const int fuzz = 9;
    const coord_def grdc(GXM / 2, GYM / 2);
    for (int y = margin; y < GYM - margin - 1; y += yinterval)
    {
        for (int x = margin; x < GXM - margin - 1; x += xinterval)
        {
            const coord_def p(x, y);
            const coord_def c(dgn_random_point_from(p, fuzz, margin));
            const coord_def center_delta(grdc - c);
            const int depth = 1600 - center_delta.abs();
            const int depth_offset = depth * 10 / 1600;
            dgn_island_centred_at(c, random_range(5, 25 - depth_multiplier / 2),
                                  random_range(2, 6),
                                  int_range(8 + depth_offset,
                                            17 + depth_offset),
                                            margin);
        }
    }
}

static dungeon_feature_type _swamp_feature_for_height(int height)
{
    return height >= 18 ? DNGN_TREE :
        height > 8 ? DNGN_SHALLOW_WATER :
        height > 0 ? DNGN_FLOOR :
        height > -6 ? DNGN_SHALLOW_WATER :
        DNGN_TREE;
}

static void _swamp_apply_features(int margin)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        const coord_def c(*ri);
        if (!map_masked(c, MMT_VAULT))
        {
            if (c.x < margin || c.y < margin || c.x >= GXM - margin
                || c.y >= GYM - margin)
            {
                grd(c) = DNGN_TREE;
            }
            else
            {
                // This gets done in two passes.
                // First, mangroves are placed, then the heightmap is smoothed,
                // then the rest of the features are placed.
                // That's why this conditional exists.
                if (grd(c) != DNGN_TREE)
                    grd(c) = _swamp_feature_for_height(dgn_height_at(c));
            }
        }
    }
}

void dgn_build_swamp_level()
{
    env.level_build_method += " swamp";
    env.level_layout_types.insert("swamp");

    dgn_initialise_heightmap(-19);
    _swamp_slushy_patches(3);
    dgn_smooth_heights(2, 1);
    _swamp_apply_features(2);
    dgn_smooth_heights(1, 2);
    _swamp_apply_features(2);
    env.heightmap.reset(NULL);
}
