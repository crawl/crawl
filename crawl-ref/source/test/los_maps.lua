-- check LOS according to pre-defined maps

local checks = 0

-- visible:
local rock_wall = dgn.find_feature_number("rock_wall")
local floor = dgn.find_feature_number("floor")
-- invisible (not needed):
local stone_wall = dgn.find_feature_number("stone_wall")
local water = dgn.find_feature_number("deep_water")

local function test_los_map(map)
  crawl.mesclr()
  -- choose random debug map; better choose all
  dgn.tags(map, "no_rotate no_vmirror no_hmirror no_pool_fixup")
  local name = dgn.name(map)
  -- local width, height = dgn.mapsize(map) -- returns (0,0)
  local function place_map()
    return dgn.place_map(map, true, true)
  end
  dgn.with_map_anchors(30, 30, place_map)
  you.moveto(30, 30)
  you.losight()
  crawl.redraw_view()
  for x = 0, 9 do
    for y = 0, 9 do
      local xa = 30 + x
      local ya = 30 + y
      local p = dgn.point(x, y)
      local should_see = ((x == 0 and y == 0) or
                          dgn.grid(xa, ya) == rock_wall or
                          dgn.grid(xa, ya) == floor)
      local can_see = you.see_grid(xa, ya)
      if can_see and (not should_see) then
        assert(false, "los error in " .. name .."(iter #" .. checks ..
                      "): can see " .. p .. " but shouldn't.")
      elseif (not can_see) and should_see then
        assert(false, "los error in " .. name .. "(iter #" .. checks ..
                      "): should see " .. p .. " but can't.")
      end
    end
  end
end

local function test_los_maps()
  dgn.load_des_file("debug-los.des")
  local map = dgn.map_by_tag("debug_los")
  while map do
    test_los_map(map)
    checks = checks + 1
    map = dgn.map_by_tag("debug_los")
  end
end

test_los_maps()

