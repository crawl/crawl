#include "AppHdr.h"

#include "coordit.h"
#include "dungeon.h"
#include "dgn-height.h"
#include "random.h"

static void _swamp_slushy_patches(int depth_multiplier)
{
    const int margin = 6;
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
    return height >= 14? DNGN_DEEP_WATER :
        height > 0? DNGN_SHALLOW_WATER :
        height > -9 ? DNGN_FLOOR :
        height > -13 ? DNGN_SHALLOW_WATER :
        height > -17 && coinflip() ? DNGN_SHALLOW_WATER :
        DNGN_TREES;
}

static void _swamp_apply_features(int margin)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        const coord_def c(*ri);
        if (unforbidden(c, MMT_VAULT))
        {
            if (c.x < margin || c.y < margin || c.x >= GXM - margin
                || c.y >= GYM - margin)
                grd(c) = DNGN_TREES;
            else
               grd(c) = _swamp_feature_for_height(dgn_height_at(c));
        }
    }
}

void dgn_build_swamp_level(int level)
{
    dgn_Build_Method += " swamp";
    dgn_Layout_Type   = "swamp";

    const int swamp_depth = level_id::current().depth - 1;
    dgn_initialise_heightmap(-17);
    _swamp_slushy_patches(swamp_depth * 3);
    dgn_smooth_heights();
    _swamp_apply_features(2);
    env.heightmap.reset(NULL);

    dgn_place_stone_stairs();
    process_disconnected_zones(0, 0, GXM - 1, GYM - 1, true, DNGN_TREES);
}


#if OLD_SWAMP_LAYOUT
void dgn_prepare_swamp()
{
    dgn_Build_Method += " swamp";
    dgn_Layout_Type   = "swamp";

    const int margin = 5;

    for (int i = margin; i < (GXM - margin); i++)
        for (int j = margin; j < (GYM - margin); j++)
        {
            // Don't apply Swamp prep in vaults.
            if (!unforbidden(coord_def(i, j), MMT_VAULT))
                continue;

            // doors -> floors {dlb}
            if (grd[i][j] == DNGN_CLOSED_DOOR || grd[i][j] == DNGN_SECRET_DOOR)
                grd[i][j] = DNGN_FLOOR;

            // floors -> shallow water 1 in 3 times {dlb}
            if (grd[i][j] == DNGN_FLOOR && one_chance_in(3))
                grd[i][j] = DNGN_SHALLOW_WATER;

            // walls -> deep/shallow water or remain unchanged {dlb}
            if (grd[i][j] == DNGN_ROCK_WALL)
            {
                const int temp_rand = random2(6);

                if (temp_rand > 0)      // 17% chance unchanged {dlb}
                {
                    grd[i][j] = ((temp_rand > 2) ? DNGN_SHALLOW_WATER // 50%
                                                 : DNGN_DEEP_WATER);  // 33%
                }
            }
        }
}
#endif
