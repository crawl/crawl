var tile_cache = {};
var tile_cache_dx = 0;
var tile_cache_dy = 0;

function clear_tile_cache()
{
    tile_cache = {};
    tile_cache_dx = 0x8FFF;
    tile_cache_dy = 0x8FFF;
}

function set_tile_cache(x, y, cell)
{
    x += tile_cache_dx;
    y += tile_cache_dy;
    tile_cache[x + (y << 16)] = cell;
}

function get_tile_cache(x, y)
{
    x += tile_cache_dx;
    y += tile_cache_dy;
    return tile_cache[x + (y << 16)];
}

function shift_tile_cache(dx, dy)
{
    tile_cache_dx += dx;
    tile_cache_dy += dy;
}
