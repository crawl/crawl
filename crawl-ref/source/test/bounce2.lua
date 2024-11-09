-- Check bouncing of lightning bolts around in a box with ragged
-- edges. If there's anything wrong the C/C++ code will assert.

local checks = 0
local iters  = 5
local beams  = 10

local function test_bounce_iter(map)
  dgn.reset_level()
  -- local width, height = dgn.mapsize(map) -- returns (0,0)
  local function place_map()
    return dgn.place_map(map, true, true, 30, 30)
  end
  dgn.with_map_anchors(30, 30, place_map)

  if dgn.grid(31, 31) ~= dgn.find_feature_number("floor") then
    assert(false, "Map not placed properly")
  end

  -- Move player out of the way
  you.moveto(2, 2)

  -- Fire beams to random targets, using a ray as returned by
  -- find_ray if available.
  for i = 1, beams do
    local dx = crawl.random2(9)
    local dy = crawl.random2(9)
    debug.bouncy_beam(30, 30, 30+dx, 30+dy, 1000, true)
  end
end

local function test_bounces()
  debug.reset_player_data()
  local map = dgn.map_by_tag("bounce_test")
  assert(map, "Could not find bounce_test map (tag 'bounce_test')")
  -- The ragedness of the map edge is randomized on placement,
  -- so place the same map multiple times.
  for i = 1, iters do
    test_bounce_iter(map)
  end

  checks = 1
end

test_bounces()
