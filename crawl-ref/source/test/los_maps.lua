-- check LOS according to pre-defined maps

local checks = 0

-- visible:
local rock_wall = dgn.find_feature_number("rock_wall")
local floor = dgn.find_feature_number("floor")
-- invisible (not needed):
local stone_wall = dgn.find_feature_number("stone_wall")
local water = dgn.find_feature_number("deep_water")

local function test_los_map(map)
  dgn.reset_level()
  -- choose random debug map; better choose all
  dgn.tags(map, "no_rotate no_vmirror no_hmirror no_pool_fixup")
  local name = dgn.name(map)
  -- local width, height = dgn.mapsize(map) -- returns (0,0)
  local function place_map()
    return dgn.place_map(map, true, true)
  end
  dgn.with_map_anchors(30, 30, place_map)
  you.moveto(30, 30)
  crawl.redraw_view()
  for x = 0, 9 do
    for y = 0, 9 do
      local xa = 30 + x
      local ya = 30 + y
      local p = dgn.point(x, y)
      local feat = dgn.grid(xa, ya)
      local should_see = ((x == 0 and y == 0) or
                          feat == rock_wall or
                          feat == floor)
      local should_not_see = (x ~= 0 or y ~= 0) and
                              (feat == stone_wall or feat == water)
      if (not (should_see or should_not_see)) then
        assert(false, "Illegal feature " .. dgn.feature_name(feat) .. " (" ..
                      feat .. ") at " .. p .. " in " .. name .. ".")
      end
      local can_see = you.see_cell(xa, ya)
      local err = can_see and (not should_see) or (not can_see) and should_see
      if err then
        local cans = can_see and "can" or "can't"
        local shoulds = should_see and "should" or "shouldn't"
        local errstr = "LOS error in " .. name .. ": " .. shoulds ..
                       " see " .. p .. " (" .. feat .. ") but " .. cans .. "."
        assert(false, errstr)
      end
    end
  end
end

local function test_los_maps()
  local map = dgn.map_by_tag("debug_los")
  assert(map, "Could not find debug-los maps (tag 'debug_los')")
  while map do
    test_los_map(map)
    checks = checks + 1
    map = dgn.map_by_tag("debug_los")
  end
end

test_los_maps()
