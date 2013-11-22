/**
 * @file
 * @brief Implementations of some of the dungeon layouts.
**/

#include "AppHdr.h"

#include "dgn-layouts.h"

#include "coordit.h"
#include "dungeon.h"
#include "libutil.h"
#include "terrain.h"
#include "traps.h"

static bool _find_forbidden_in_area(dgn_region& area, unsigned int mask);
static int _count_antifeature_in_box(int x0, int y0, int x1, int y1,
                                     dungeon_feature_type feat);
static int _trail_random_dir(int pos, int bound, int margin);
static void _make_trail(int xs, int xr, int ys, int yr, int corrlength,
                        int intersect_chance, int no_corr,
                        coord_def& begin, coord_def& end);
static void _builder_extras(int level_number);
static bool _octa_room(dgn_region& region, int oblique_max,
                       dungeon_feature_type type_floor);
static dungeon_feature_type _random_wall(void);
static void _chequerboard(dgn_region& region, dungeon_feature_type target,
                          dungeon_feature_type floor1,
                          dungeon_feature_type floor2);
static int _box_room_door_spot(int x, int y);
static int  _box_room_doors(int bx1, int bx2, int by1, int by2, int new_doors);
static void _box_room(int bx1, int bx2, int by1, int by2,
                      dungeon_feature_type wall_type);
static bool _is_wall(int x, int y);
static void _big_room(int level_number);
static void _diamond_rooms(int level_number);
static int _good_door_spot(int x, int y);
static bool _make_room(int sx,int sy,int ex,int ey,int max_doors, int doorlevel);
static void _make_random_rooms(int num, int max_doors, int door_level,
                               int max_x, int max_y, int max_room_size);
static void _place_pool(dungeon_feature_type pool_type, uint8_t pool_x1,
                        uint8_t pool_y1, uint8_t pool_x2, uint8_t pool_y2);
static void _many_pools(dungeon_feature_type pool_type);
static bool _may_overwrite_pos(coord_def c);
static void _build_river(dungeon_feature_type river_type);
static void _build_lake(dungeon_feature_type lake_type);

void dgn_build_basic_level()
{
    int level_number = env.absdepth0;

    env.level_build_method += " basic";
    env.level_layout_types.insert("basic");

    int corrlength = 2 + random2(14);
    int no_corr = (one_chance_in(100) ? 500 + random2(500)
                                      : 30 + random2(200));
    int intersect_chance = (one_chance_in(20) ? 400 : random2(20));

    coord_def begin;
    coord_def end;

    vector<coord_def> upstairs;

    _make_trail(35, 30, 35, 20, corrlength, intersect_chance, no_corr,
                 begin, end);

    if (!begin.origin() && !end.origin())
    {
        grd(begin) = DNGN_STONE_STAIRS_DOWN_I;
        grd(end)   = DNGN_STONE_STAIRS_UP_I;
        upstairs.push_back(begin);
    }

    begin.reset(); end.reset();

    _make_trail(10, 15, 10, 15, corrlength, intersect_chance, no_corr,
                 begin, end);

    if (!begin.origin() && !end.origin())
    {
        grd(begin) = DNGN_STONE_STAIRS_DOWN_II;
        grd(end)   = DNGN_STONE_STAIRS_UP_II;
        upstairs.push_back(begin);
    }

    begin.reset(); end.reset();

    _make_trail(50, 20, 10, 15, corrlength, intersect_chance, no_corr,
                 begin, end);

    if (!begin.origin() && !end.origin())
    {
        grd(begin) = DNGN_STONE_STAIRS_DOWN_III;
        grd(end)   = DNGN_STONE_STAIRS_UP_III;
        upstairs.push_back(begin);
    }

    // Generate a random dead-end that /may/ have a shaft.  Good luck!
    if (is_valid_shaft_level() && !one_chance_in(4)) // 3/4 times
    {
        // This is kinda hack-ish.  We're still early in the dungeon
        // generation process, and we know that there will be no other
        // traps.  If we promise to make /just one/, we can get away
        // with making this trap the first trap.
        // If we aren't careful, we'll trigger an assert in _place_traps().

        begin.reset(); end.reset();

        _make_trail(50, 20, 40, 20, corrlength, intersect_chance, no_corr,
                     begin, end);

        dprf("Placing shaft trail...");
        if (!end.origin())
        {
            if (!begin.origin())
                upstairs.push_back(begin);
            if (!one_chance_in(3) && !map_masked(end, MMT_NO_TRAP)) // 2/3 chance it ends in a shaft
            {
                trap_def* ts = NULL;
                int i = 0;
                for (; i < MAX_TRAPS; i++)
                {
                    if (env.trap[i].type != TRAP_UNASSIGNED)
                        continue;

                    ts = &env.trap[i];
                    break;
                }
                if (i < MAX_TRAPS)
                {
                    ts->type = TRAP_SHAFT;
                    ts->pos = end;
                    grd(end) = DNGN_UNDISCOVERED_TRAP;
                    env.tgrid(end) = i;
                    if (shaft_known(level_number, false))
                        ts->reveal();
                    dprf("Trail ends in shaft.");
                }
                else
                {
                    grd(end) = DNGN_FLOOR;
                    dprf("Trail does not end in shaft.");
                }
            }
            else
            {
                grd(end) = DNGN_FLOOR;
                dprf("Trail does not end in shaft.");
            }
        }
    }

    for (vector<coord_def>::iterator pathstart = upstairs.begin();
         pathstart != upstairs.end(); pathstart++)
    {
        vector<coord_def>::iterator pathend = pathstart;
        pathend++;
        for (; pathend != upstairs.end(); pathend++)
            join_the_dots(*pathstart, *pathend, MMT_VAULT);
    }

    if (level_number > 1 && one_chance_in(16))
        _big_room(level_number);

    if (random2(level_number) > 6 && one_chance_in(3))
        _diamond_rooms(level_number);

    // Make some rooms:
    int doorlevel = random2(11);
    int roomsize  = 4 + random2(5) + random2(6);

    int no_rooms = random_choose_weighted(636, (5 + random2avg(29, 2)),
                                          49, 100,
                                          15, 1, 0);

    _make_random_rooms(no_rooms, 2 + random2(8), doorlevel, 50, 40, roomsize);

    _make_random_rooms(1 + random2(3), 1, doorlevel, 55, 45, 6);

    _builder_extras(level_number);
}

void dgn_build_bigger_room_level(void)
{
    env.level_build_method += " bigger_room";
    env.level_layout_types.insert("open");

    for (rectangle_iterator ri(10); ri; ++ri)
        if (grd(*ri) == DNGN_ROCK_WALL
            && !map_masked(*ri, MMT_VAULT))
        {
            grd(*ri) = DNGN_FLOOR;
        }

    dungeon_feature_type pool_type = DNGN_DEEP_WATER;

    if (one_chance_in(15))
        pool_type = DNGN_TREE;

    _many_pools(pool_type);

    if (one_chance_in(3))
    {
        if (coinflip())
            _build_river(DNGN_DEEP_WATER);
        else
            _build_lake(DNGN_DEEP_WATER);
    }

    dgn_place_stone_stairs(true);
}

// A more chaotic version of city level.
void dgn_build_chaotic_city_level(dungeon_feature_type force_wall)
{
    env.level_build_method += make_stringf(" chaotic_city [%s]",
        force_wall == NUM_FEATURES ? "any" : dungeon_feature_name(force_wall));
    env.level_layout_types.insert("city");

    int number_boxes = 5000;
    dungeon_feature_type drawing = DNGN_ROCK_WALL;
    uint8_t b1x, b1y, b2x, b2y;
    int i;

    number_boxes = random_choose_weighted(32, 4000,
                                          24, 3000,
                                          16, 5000,
                                          8, 2000,
                                          1, 1000, 0);

    if (force_wall != NUM_FEATURES)
        drawing = force_wall;
    else
    {
        drawing = random_choose_weighted(10, DNGN_ROCK_WALL,
                                         5, DNGN_STONE_WALL,
                                         3, DNGN_METAL_WALL, 0);
    }

    dgn_replace_area(10, 10, (GXM - 10), (GYM - 10), DNGN_ROCK_WALL,
                     DNGN_FLOOR, MMT_VAULT);

    // replace_area can also be used to fill in:
    for (i = 0; i < number_boxes; i++)
    {
        int room_width = 3 + random2(7) + random2(5);
        int room_height = 3 + random2(7) + random2(5);

        b1x = 11 + random2(GXM - 21 - room_width);
        b1y = 11 + random2(GYM - 21 - room_height);

        b2x = b1x + room_width;
        b2y = b1y + room_height;

        dgn_region box = dgn_region::absolute(b1x, b1y, b2x, b2y);
        if (_find_forbidden_in_area(box, MMT_VAULT))
            continue;

        if (_count_antifeature_in_box(b1x-1, b1y-1, b2x+2, b2y+2, DNGN_FLOOR))
            continue;

        if (force_wall == NUM_FEATURES && one_chance_in(3))
        {
            drawing = random_choose_weighted(261, DNGN_ROCK_WALL,
                                             116, DNGN_STONE_WALL,
                                             40, DNGN_METAL_WALL, 0);
        }

        if (one_chance_in(3))
            _box_room(b1x, b2x, b1y, b2y, drawing);
        else
        {
            dgn_replace_area(b1x, b1y, b2x, b2y, DNGN_FLOOR, drawing,
                             MMT_VAULT);
        }
    }

    dgn_region room = dgn_region::absolute(25, 25, 55, 45);

    // A market square.
    if (!_find_forbidden_in_area(room, MMT_VAULT) && one_chance_in(4))
    {
        int oblique_max = 0;
        if (!one_chance_in(4))
            oblique_max = 5 + random2(20);      // used elsewhere {dlb}

        dungeon_feature_type feature = DNGN_FLOOR;
        if (one_chance_in(10))
            feature = coinflip() ? DNGN_DEEP_WATER : DNGN_LAVA;

        _octa_room(room, oblique_max, feature);
    }
}

/* Helper functions */

static bool _find_forbidden_in_area(dgn_region& area, unsigned int mask)
{
    for (rectangle_iterator ri(area.pos, area.end()); ri; ++ri)
        if (map_masked(*ri, mask))
            return true;

    return false;
}

static int _count_antifeature_in_box(int x0, int y0, int x1, int y1,
                                     dungeon_feature_type feat)
{
    return (x1-x0)*(y1-y0) - count_feature_in_box(x0, y0, x1, y1, feat);
}

static int _trail_random_dir(int pos, int bound, int margin)
{
    int dir = 0;

    if (pos < margin)
        dir = 1;
    else if (pos > bound - margin)
        dir = -1;

    if (dir == 0 || x_chance_in_y(2, 5))
        dir = coinflip() ? -1 : 1;

    return dir;
}

static void _make_trail(int xs, int xr, int ys, int yr, int corrlength,
                        int intersect_chance, int no_corr,
                        coord_def& begin, coord_def& end)
{
    int finish = 0;
    int length = 0;
    int tries = 200;

    coord_def pos;
    coord_def dir(0, 0);

    do
    {
        pos.x = xs + random2(xr);
        pos.y = ys + random2(yr);
    }
    while (grd(pos) != DNGN_ROCK_WALL && grd(pos) != DNGN_FLOOR
           || map_masked(pos, MMT_VAULT) && tries-- > 0);

    if (tries < 0)
        return;

    tries = 200;
    // assign begin position
    begin = pos;

    // wander
    while (finish < no_corr)
    {
        if (!(tries--)) // give up after 200 tries
            return;

        dir.reset();

        // Put something in to make it go to parts of map it isn't in now.
        if (coinflip())
            dir.x = _trail_random_dir(pos.x, GXM, 15);
        else
            dir.y = _trail_random_dir(pos.y, GYM, 15);

        if (dir.x == 0 && dir.y == 0
            || map_masked(pos + dir, MMT_VAULT))
            continue;

        // Corridor length... change only when going vertical?
        if (dir.x == 0 || length == 0)
            length = random2(corrlength) + 2;

        for (int bi = 0; bi < length; bi++)
        {
            if (pos.x < X_BOUND_1 + 4)
                dir.set(1, 0);

            if (pos.x > (X_BOUND_2 - 4))
                dir.set(-1, 0);

            if (pos.y < Y_BOUND_1 + 4)
                dir.set(0, 1);

            if (pos.y > (Y_BOUND_2 - 4))
                dir.set(0, -1);

            if (map_masked(pos + dir, MMT_VAULT))
                break;

            // See if we stop due to intersection with another corridor/room.
            if (grd(pos + dir * 2) == DNGN_FLOOR
                && !one_chance_in(intersect_chance))
            {
                break;
            }

            pos += dir;

            if (grd(pos) == DNGN_ROCK_WALL)
                grd(pos) = DNGN_FLOOR;
        }

        if (finish == no_corr - 1 && grd(pos) != DNGN_FLOOR)
            finish -= 2;

        finish++;
    }

    // assign end position
    end = pos;
}

static void _builder_extras(int level_number)
{
    if (level_number > 6 && one_chance_in(10))
    {
        dungeon_feature_type pool_type = (level_number < 11
                                          || coinflip()) ? DNGN_DEEP_WATER
                                                         : DNGN_LAVA;
        if (one_chance_in(15))
            pool_type = DNGN_TREE;

        _many_pools(pool_type);
        return;
    }

    //mv: It's better to be here so other dungeon features are not overridden
    //    by water.
    dungeon_feature_type river_type
        = (one_chance_in(5 + level_number) ? DNGN_SHALLOW_WATER
                                             : DNGN_DEEP_WATER);

    if (level_number > 11
        && (one_chance_in(5) || (level_number > 15 && !one_chance_in(5))))
    {
        river_type = DNGN_LAVA;
    }

    if (player_in_branch(BRANCH_GEHENNA))
    {
        river_type = DNGN_LAVA;

        if (coinflip())
            _build_river(river_type);
        else
            _build_lake(river_type);
    }
    else if (player_in_branch(BRANCH_COCYTUS))
    {
        river_type = DNGN_DEEP_WATER;

        if (coinflip())
            _build_river(river_type);
        else
            _build_lake(river_type);
    }

    if (level_number > 8 && one_chance_in(16))
        _build_river(river_type);
    else if (level_number > 8 && one_chance_in(12))
    {
        _build_lake((river_type != DNGN_SHALLOW_WATER) ? river_type
                                                       : DNGN_DEEP_WATER);
    }
}

static bool _octa_room(dgn_region& region, int oblique_max,
                       dungeon_feature_type type_floor)
{
    env.level_build_method += make_stringf(" octa_room [%d %s]", oblique_max,
                                           dungeon_feature_name(type_floor));

    int x,y;

    coord_def& tl = region.pos;
    coord_def br = region.end();

    // Hack - avoid lava in the crypt {gdl}
    if ((player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_TOMB)
         || player_in_branch(BRANCH_COCYTUS))
         && type_floor == DNGN_LAVA)
    {
        type_floor = DNGN_SHALLOW_WATER;
    }

    int oblique = oblique_max;

    for (x = tl.x; x < br.x; x++)
    {
        if (x > tl.x - oblique_max)
            oblique += 2;

        if (oblique > 0)
            oblique--;
    }

    oblique = oblique_max;


    for (x = tl.x; x < br.x; x++)
    {
        for (y = tl.y + oblique; y < br.y - oblique; y++)
        {
            if (map_masked(coord_def(x, y), MMT_VAULT))
                continue;

            if (_is_wall(x, y))
                grd[x][y] = type_floor;

            if (grd[x][y] == DNGN_FLOOR && type_floor == DNGN_SHALLOW_WATER)
                grd[x][y] = DNGN_SHALLOW_WATER;

            if (grd[x][y] == DNGN_CLOSED_DOOR && !feat_is_solid(type_floor))
                grd[x][y] = DNGN_FLOOR;       // ick
        }

        if (x > br.x - oblique_max)
            oblique += 2;

        if (oblique > 0)
            oblique--;
    }

    return true;
}

static dungeon_feature_type _random_wall()
{
    const dungeon_feature_type min_rand = DNGN_METAL_WALL;
    const dungeon_feature_type max_rand = DNGN_STONE_WALL;
    dungeon_feature_type wall;
    do
    {
        wall = static_cast<dungeon_feature_type>(
                   random_range(min_rand, max_rand));
    }
    while (wall == DNGN_SLIMY_WALL);

    return wall;
}

// Helper function for chequerboard rooms.
// Note that box boundaries are INclusive.
static void _chequerboard(dgn_region& region, dungeon_feature_type target,
                           dungeon_feature_type floor1,
                           dungeon_feature_type floor2)
{
    for (rectangle_iterator ri(region.pos, region.end()); ri; ++ri)
        if (grd(*ri) == target && !map_masked(*ri, MMT_VAULT))
            grd(*ri) = ((ri->x + ri->y) % 2) ? floor2 : floor1;
}

static int _box_room_door_spot(int x, int y)
{
    // If there is a door near us embedded in rock, we have to be a door too.
    if (grd[x-1][y] == DNGN_CLOSED_DOOR
            && _is_wall(x-1,y-1) && _is_wall(x-1,y+1)
        || grd[x+1][y] == DNGN_CLOSED_DOOR
            && _is_wall(x+1,y-1) && _is_wall(x+1,y+1)
        || grd[x][y-1] == DNGN_CLOSED_DOOR
            && _is_wall(x-1,y-1) && _is_wall(x+1,y-1)
        || grd[x][y+1] == DNGN_CLOSED_DOOR
            && _is_wall(x-1,y+1) && _is_wall(x+1,y+1))
    {
        grd[x][y] = DNGN_CLOSED_DOOR;
        return 2;
    }

    // To be a good spot for a door, we need non-wall on two sides and
    // wall on two sides.
    bool nor = _is_wall(x, y-1);
    bool sou = _is_wall(x, y+1);
    bool eas = _is_wall(x-1, y);
    bool wes = _is_wall(x+1, y);

    if (nor == sou && eas == wes && nor != eas)
        return 1;

    return 0;
}

static int _box_room_doors(int bx1, int bx2, int by1, int by2, int new_doors)
{
    int good_doors[200];        // 1 == good spot, 2 == door placed!
    int spot;
    int i, j;
    int doors_placed = new_doors;

    // sanity
    if (2 * (bx2-bx1 + by2-by1) > 200)
        return 0;

    // Go through, building list of good door spots, and replacing wall
    // with door if we're about to block off another door.
    int spot_count = 0;

    // top & bottom
    for (i = bx1 + 1; i < bx2; i++)
    {
        good_doors[spot_count ++] = _box_room_door_spot(i, by1);
        good_doors[spot_count ++] = _box_room_door_spot(i, by2);
    }
    // left & right
    for (i = by1+1; i < by2; i++)
    {
        good_doors[spot_count ++] = _box_room_door_spot(bx1, i);
        good_doors[spot_count ++] = _box_room_door_spot(bx2, i);
    }

    if (new_doors == 0)
    {
        // Count # of doors we HAD to place.
        for (i = 0; i < spot_count; i++)
            if (good_doors[i] == 2)
                doors_placed++;

        return doors_placed;
    }

    // Avoid an infinite loop if there are not enough good spots. --KON
    j = 0;
    for (i = 0; i < spot_count; i++)
        if (good_doors[i] == 1)
            j++;

    if (new_doors > j)
        new_doors = j;

    while (new_doors > 0 && spot_count > 0)
    {
        spot = random2(spot_count);
        if (good_doors[spot] != 1)
            continue;

        j = 0;
        for (i = bx1 + 1; i < bx2; i++)
        {
            if (spot == j++)
            {
                grd[i][by1] = DNGN_CLOSED_DOOR;
                break;
            }
            if (spot == j++)
            {
                grd[i][by2] = DNGN_CLOSED_DOOR;
                break;
            }
        }

        for (i = by1 + 1; i < by2; i++)
        {
            if (spot == j++)
            {
                grd[bx1][i] = DNGN_CLOSED_DOOR;
                break;
            }
            if (spot == j++)
            {
                grd[bx2][i] = DNGN_CLOSED_DOOR;
                break;
            }
        }

        // Try not to put a door in the same place twice.
        good_doors[spot] = 2;
        new_doors --;
    }

    return doors_placed;
}

static void _box_room(int bx1, int bx2, int by1, int by2,
                      dungeon_feature_type wall_type)
{
    // Hack -- avoid lava in the crypt. {gdl}
    if ((player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_TOMB))
         && wall_type == DNGN_LAVA)
    {
        wall_type = DNGN_SHALLOW_WATER;
    }

    int new_doors, doors_placed;

    // Do top & bottom walls.
    dgn_replace_area(bx1, by1, bx2, by1, DNGN_FLOOR, wall_type, MMT_VAULT);
    dgn_replace_area(bx1, by2, bx2, by2, DNGN_FLOOR, wall_type, MMT_VAULT);

    // Do left & right walls.
    dgn_replace_area(bx1, by1+1, bx1, by2-1, DNGN_FLOOR, wall_type, MMT_VAULT);
    dgn_replace_area(bx2, by1+1, bx2, by2-1, DNGN_FLOOR, wall_type, MMT_VAULT);

    // Sometimes we have to place doors, or else we shut in other
    // buildings' doors.
    doors_placed = _box_room_doors(bx1, bx2, by1, by2, 0);

    new_doors = random_choose_weighted(54, 2,
                                       23, 1,
                                       23, 3, 0);

    // Small rooms don't have as many doors.
    if ((bx2-bx1)*(by2-by1) < 36 && new_doors > 1)
        new_doors--;

    new_doors -= doors_placed;
    if (new_doors > 0)
        _box_room_doors(bx1, bx2, by1, by2, new_doors);
}

static bool _is_wall(int x, int y)
{
    return feat_is_wall(grd[x][y]);
}

static void _big_room(int level_number)
{
    dungeon_feature_type type_floor = DNGN_FLOOR;
    dungeon_feature_type type_2 = DNGN_FLOOR;

    dgn_region region;

    int overlap_tries = 200;

    if (one_chance_in(4))
    {
        int oblique = 5 + random2(20);

        do
        {
            region = dgn_region(8 + random2(30), 8 + random2(22),
                                21 + random2(10), 21 + random2(8));
        }
        while (_find_forbidden_in_area(region, MMT_VAULT)
               && overlap_tries-- > 0);

        if (overlap_tries < 0)
            return;

        // Usually floor, except at higher levels.
        if (!one_chance_in(5) || level_number < 8 + random2(8))
        {
            _octa_room(region, oblique, DNGN_FLOOR);
            return;
        }

        // Default is lava.
        type_floor = DNGN_LAVA;

        if (level_number > 7)
        {
            type_floor = (x_chance_in_y(14, level_number) ? DNGN_DEEP_WATER
                                                          : DNGN_LAVA);
        }

        _octa_room(region, oblique, type_floor);
    }

    overlap_tries = 200;

    // What now?
    do
    {
        region = dgn_region(8 + random2(30), 8 + random2(22),
                            21 + random2(10), 21 + random2(8));
    }
    while (_find_forbidden_in_area(region, MMT_VAULT) && overlap_tries-- > 0);

    if (overlap_tries < 0)
        return;

    if (level_number > 7 && one_chance_in(4))
    {
        type_floor = (x_chance_in_y(14, level_number) ? DNGN_DEEP_WATER
                                                      : DNGN_LAVA);
    }

    // Make the big room.
    dgn_replace_area(region.pos, region.end(), DNGN_ROCK_WALL, type_floor,
                     MMT_VAULT);
    dgn_replace_area(region.pos, region.end(), DNGN_CLOSED_DOOR, type_floor,
                     MMT_VAULT);

    if (type_floor == DNGN_FLOOR)
        type_2 = _random_wall();

    // No lava in the Crypt or Tomb, thanks!
    if (player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_TOMB))
    {
        if (type_floor == DNGN_LAVA)
            type_floor = DNGN_SHALLOW_WATER;

        if (type_2 == DNGN_LAVA)
            type_2 = DNGN_SHALLOW_WATER;
    }

    // Sometimes make it a chequerboard.
    if (one_chance_in(4))
        _chequerboard(region, type_floor, type_floor, type_2);
    // Sometimes make an inside room w/ stone wall.
    else if (one_chance_in(6))
    {
        int i = region.pos.x;
        int j = region.pos.y;
        int k = region.end().x;
        int l = region.end().y;

        do
        {
            i += 2 + random2(3);
            j += 2 + random2(3);
            k -= 2 + random2(3);
            l -= 2 + random2(3);
            // check for too small
            if (i >= k - 3)
                break;
            if (j >= l - 3)
                break;

            _box_room(i, k, j, l, DNGN_STONE_WALL);
        }
        while (level_number < 1500);       // ie forever
    }
}

static void _diamond_rooms(int level_number)
{
    int numb_diam = 1 + random2(10);
    dungeon_feature_type type_floor = DNGN_DEEP_WATER;
    int runthru = 0;
    int i, oblique_max;

    // I guess no diamond rooms in either of these places. {dlb}
    if (player_in_branch(BRANCH_DIS) || player_in_branch(BRANCH_TARTARUS))
        return;

    if (level_number > 5 + random2(5) && coinflip())
        type_floor = DNGN_SHALLOW_WATER;

    if (level_number > 10 + random2(5) && coinflip())
        type_floor = DNGN_DEEP_WATER;

    if (level_number > 17 && coinflip())
        type_floor = DNGN_LAVA;

    if (level_number > 10 && one_chance_in(15))
        type_floor = (coinflip() ? DNGN_STONE_WALL : DNGN_ROCK_WALL);

    if (level_number > 12 && one_chance_in(20))
        type_floor = DNGN_METAL_WALL;

    if (player_in_branch(BRANCH_GEHENNA))
        type_floor = DNGN_LAVA;
    else if (player_in_branch(BRANCH_COCYTUS))
        type_floor = DNGN_DEEP_WATER;

    for (i = 0; i < numb_diam; i++)
    {
        int overlap_tries = 200;
        dgn_region room;
        do
        {
            room = dgn_region(8 + random2(43), 8 + random2(35),
                              6 + random2(15), 6 + random2(10));
        }
        while (_find_forbidden_in_area(room, MMT_VAULT)
               && overlap_tries-- > 0);

        if (overlap_tries < 0)
            return;

        oblique_max = room.size.x / 2;

        if (!_octa_room(room, oblique_max, type_floor))
        {
            runthru++;
            if (runthru > 9)
                runthru = 0;
            else
            {
                i--;
                continue;
            }
        }
    }
}

static int _good_door_spot(int x, int y)
{
    if (!feat_is_solid(grd[x][y]) && grd[x][y] < DNGN_ENTER_PANDEMONIUM
        || feat_is_closed_door(grd[x][y]))
    {
        return 1;
    }

    return 0;
}

// Returns TRUE if a room was made successfully.
static bool _make_room(int sx,int sy,int ex,int ey,int max_doors, int doorlevel)
{
    int find_door = 0;
    int diag_door = 0;
    int rx, ry;

    // Check top & bottom for possible doors.
    for (rx = sx; rx <= ex; rx++)
    {
        find_door += _good_door_spot(rx,sy);
        find_door += _good_door_spot(rx,ey);
    }

    // Check left and right for possible doors.
    for (ry = sy + 1; ry < ey; ry++)
    {
        find_door += _good_door_spot(sx,ry);
        find_door += _good_door_spot(ex,ry);
    }

    diag_door += _good_door_spot(sx,sy);
    diag_door += _good_door_spot(ex,sy);
    diag_door += _good_door_spot(sx,ey);
    diag_door += _good_door_spot(ex,ey);

    if ((diag_door + find_door) > 1 && max_doors == 1)
        return false;

    if (find_door == 0 || find_door > max_doors)
        return false;

    // Convert the area to floor.
    for (rx = sx; rx <= ex; rx++)
        for (ry = sy; ry <= ey; ry++)
        {
            if (grd[rx][ry] <= DNGN_FLOOR)
                grd[rx][ry] = DNGN_FLOOR;
        }

    // Put some doors on the sides (but not in corners),
    // where it makes sense to do so.
    for (ry = sy + 1; ry < ey; ry++)
    {
        // left side
        if (grd[sx-1][ry] == DNGN_FLOOR
            && feat_is_solid(grd[sx-1][ry-1])
            && feat_is_solid(grd[sx-1][ry+1]))
        {
            if (x_chance_in_y(doorlevel, 10))
                grd[sx-1][ry] = DNGN_CLOSED_DOOR;
        }

        // right side
        if (grd[ex+1][ry] == DNGN_FLOOR
            && feat_is_solid(grd[ex+1][ry-1])
            && feat_is_solid(grd[ex+1][ry+1]))
        {
            if (x_chance_in_y(doorlevel, 10))
                grd[ex+1][ry] = DNGN_CLOSED_DOOR;
        }
    }

    // Put some doors on the top & bottom.
    for (rx = sx + 1; rx < ex; rx++)
    {
        // top
        if (grd[rx][sy-1] == DNGN_FLOOR
            && feat_is_solid(grd[rx-1][sy-1])
            && feat_is_solid(grd[rx+1][sy-1]))
        {
            if (x_chance_in_y(doorlevel, 10))
                grd[rx][sy-1] = DNGN_CLOSED_DOOR;
        }

        // bottom
        if (grd[rx][ey+1] == DNGN_FLOOR
            && feat_is_solid(grd[rx-1][ey+1])
            && feat_is_solid(grd[rx+1][ey+1]))
        {
            if (x_chance_in_y(doorlevel, 10))
                grd[rx][ey+1] = DNGN_CLOSED_DOOR;
        }
    }

    return true;
}

static void _make_random_rooms(int num, int max_doors, int door_level,
                               int max_x, int max_y, int max_room_size)
{
    int i, sx, sy, ex, ey, time_run = 0;
    dgn_region room;

    for (i = 0; i < num; i++)
    {
        int overlap_tries = 200;
        do
        {
            sx = 8 + random2(max_x);
            sy = 8 + random2(max_y);
            ex = sx + 2 + random2(max_room_size);
            ey = sy + 2 + random2(max_room_size);
            room = dgn_region::absolute(sx, sy, ex, ey);
        }
        while (_find_forbidden_in_area(room, MMT_VAULT)
               && overlap_tries-- > 0);

        if (overlap_tries < 0)
            return;

        if (!_make_room(sx, sy, ex, ey, max_doors, door_level))
        {
            time_run++;
            i--;
        }

        if (time_run > 30)
        {
            time_run = 0;
            i++;
        }
    }
}

static void _place_pool(dungeon_feature_type pool_type, uint8_t pool_x1,
                        uint8_t pool_y1, uint8_t pool_x2,
                        uint8_t pool_y2)
{
    int i, j;
    uint8_t left_edge, right_edge;

    // Don't place LAVA pools in crypt... use shallow water instead.
    if (pool_type == DNGN_LAVA
        && (player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_TOMB)))
    {
        pool_type = DNGN_SHALLOW_WATER;
    }

    if (pool_x1 >= pool_x2 - 4 || pool_y1 >= pool_y2 - 4)
        return;

    left_edge  = pool_x1 + 2 + random2(pool_x2 - pool_x1);
    right_edge = pool_x2 - 2 - random2(pool_x2 - pool_x1);

    for (j = pool_y1 + 1; j < pool_y2 - 1; j++)
    {
        for (i = pool_x1 + 1; i < pool_x2 - 1; i++)
        {
            if (i >= left_edge && i <= right_edge && grd[i][j] == DNGN_FLOOR)
                grd[i][j] = pool_type;
        }

        if (j - pool_y1 < (pool_y2 - pool_y1) / 2 || one_chance_in(4))
        {
            if (left_edge > pool_x1 + 1)
                left_edge -= random2(3);

            if (right_edge < pool_x2 - 1)
                right_edge += random2(3);
        }

        if (left_edge < pool_x2 - 1
            && (j - pool_y1 >= (pool_y2 - pool_y1) / 2
                || left_edge <= pool_x1 + 2 || one_chance_in(4)))
        {
            left_edge += random2(3);
        }

        if (right_edge > pool_x1 + 1
            && (j - pool_y1 >= (pool_y2 - pool_y1) / 2
                || right_edge >= pool_x2 - 2 || one_chance_in(4)))
        {
            right_edge -= random2(3);
        }
    }
}

static void _many_pools(dungeon_feature_type pool_type)
{
    if (player_in_branch(BRANCH_COCYTUS))
        pool_type = DNGN_DEEP_WATER;
    else if (player_in_branch(BRANCH_GEHENNA))
        pool_type = DNGN_LAVA;
    else if (player_in_branch(BRANCH_CRYPT))
        return;

    const int num_pools = 20 + random2avg(9, 2);
    int pools = 0;

    env.level_build_method += make_stringf(" many_pools [%s %d]",
        dungeon_feature_name(pool_type), num_pools);

    for (int timeout = 0; pools < num_pools && timeout < 30000; ++timeout)
    {
        const int i = random_range(X_BOUND_1 + 1, X_BOUND_2 - 21);
        const int j = random_range(Y_BOUND_1 + 1, Y_BOUND_2 - 21);
        const int k = i + 2 + roll_dice(2, 9);
        const int l = j + 2 + roll_dice(2, 9);
        dgn_region room = dgn_region::absolute(i, j, k, l);

        if (_count_antifeature_in_box(i, j, k, l, DNGN_FLOOR) == 0
            && !_find_forbidden_in_area(room, MMT_VAULT))
        {
            _place_pool(pool_type, i, j, k, l);
            pools++;
        }
    }
}

// Used for placement of rivers/lakes.
static bool _may_overwrite_pos(coord_def c)
{
    if (map_masked(c, MMT_VAULT))
        return false;

    const dungeon_feature_type grid = grd(c);

    // Don't overwrite any stairs or branch entrances.
    if (grid >= DNGN_ENTER_SHOP && grid <= DNGN_EXIT_PORTAL_VAULT
        || grid == DNGN_EXIT_HELL)
    {
        return false;
    }

    // Don't overwrite feature if there's a monster or item there.
    // Otherwise, items/monsters might end up stuck in deep water.
    return !monster_at(c) && igrd(c) == NON_ITEM;
}

static void _build_river(dungeon_feature_type river_type) //mv
{
    int i,j;
    int y, width;

    if (player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_TOMB))
        return;

    env.level_build_method += make_stringf(" river [%s]",
                                           dungeon_feature_name(river_type));

    // Made rivers less wide... min width five rivers were too annoying. -- bwr
    width = 3 + random2(4);
    y = 10 - width + random2avg(GYM-10, 3);

    for (i = 5; i < (GXM - 5); i++)
    {
        if (one_chance_in(3))   y++;
        if (one_chance_in(3))   y--;
        if (coinflip())         width++;
        if (coinflip())         width--;

        if (width < 2) width = 2;
        if (width > 6) width = 6;

        for (j = y; j < y+width ; j++)
            if (j >= 5 && j <= GYM - 5)
            {
                // Note that vaults might have been created in this area!
                // So we'll avoid the silliness of orcs/royal jelly on
                // lava and deep water grids. -- bwr
                if (!one_chance_in(200) && _may_overwrite_pos(coord_def(i, j)))
                {
                    if (width == 2 && river_type == DNGN_DEEP_WATER
                        && coinflip())
                    {
                        grd[i][j] = DNGN_SHALLOW_WATER;
                    }
                    else
                        grd[i][j] = river_type;

                    // Override existing markers.
                    env.markers.remove_markers_at(coord_def(i, j), MAT_ANY);
                }
            }
    }
}

static void _build_lake(dungeon_feature_type lake_type) //mv
{
    int i, j;
    int x1, y1, x2, y2;

    if (player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_TOMB))
        return;

    env.level_build_method += make_stringf(" lake [%s]",
                                           dungeon_feature_name(lake_type));

    x1 = 5 + random2(GXM - 30);
    y1 = 5 + random2(GYM - 30);
    x2 = x1 + 4 + random2(16);
    y2 = y1 + 8 + random2(12);

    for (j = y1; j < y2; j++)
    {
        if (coinflip())  x1 += random2(3);
        if (coinflip())  x1 -= random2(3);
        if (coinflip())  x2 += random2(3);
        if (coinflip())  x2 -= random2(3);

        if (j - y1 < (y2 - y1) / 2)
        {
            x2 += random2(3);
            x1 -= random2(3);
        }
        else
        {
            x2 -= random2(3);
            x1 += random2(3);
        }

        for (i = x1; i < x2 ; i++)
            if (j >= 5 && j <= GYM - 5 && i >= 5 && i <= GXM - 5)
            {
                // Note that vaults might have been created in this area!
                // So we'll avoid the silliness of monsters and items
                // on lava and deep water grids. -- bwr
                if (!one_chance_in(200) && _may_overwrite_pos(coord_def(i, j)))
                {
                    grd[i][j] = lake_type;

                    // Override markers. (No underwater portals, please.)
                    env.markers.remove_markers_at(coord_def(i, j), MAT_ANY);
                }
            }
    }
}
