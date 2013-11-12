-- Check basic find_ray functionality.

local FAILMAP = 'rayfail.map'
local checks = 0

local function test_findray()
  -- Send the player to a random spot on the level.
  you.random_teleport()

  local you_x, you_y = you.pos()
  local you_p = dgn.point(you_x, you_y)

  local visible_spots = { }
  for y = -8, 8 do
    for x = -8, 8 do
      if x ~= 0 or y ~= 0 then
        local px, py = x + you_x, y + you_y
        if you.see_cell_no_trans(px, py) then
          table.insert(visible_spots, { px, py })
        end
      end
    end
  end

  -- For each position in LOS, shoot a ray there
  -- and make sure it doesn't go through an opaque cell.
  for _, spot in ipairs(visible_spots) do
    checks = checks + 1
    local x, y = unpack(spot)
    local p = dgn.point(x, y)
    local ray = los.findray(you_x, you_y, x, y)
    if not ray then
      dgn.fprop_changed(x, y, "highlight")
      debug.dump_map(FAILMAP)
      assert(false, "Can't find ray to " .. p ..
                    " although it's in unobstructed view. (#" ..
                    checks .. ")")
    end
    local rx, ry = ray:pos()
    local ray_p = dgn.point(rx, ry)
    assert(ray_p == you_p,
           "Ray doesn't start at player position " .. you_p ..
           " but " .. ray_p .. ".")
    ray:advance()
    rx, ry = ray:pos()
    ray_p = dgn.point(rx, ry)
    while (ray_p ~= p) do
      if feat.is_opaque(rx, ry) then
        dgn.fprop_changed(x, y, "highlight")
        debug.dump_map(FAILMAP)
        assert(false,
               "Ray from " .. you_p .. " to " .. p ..
               " passes through opaque cell " .. ray_p
               .. ".")
      end
      ray:advance()
      rx, ry = ray:pos()
      ray_p = dgn.point(rx, ry)
    end
  end
end

local function run_findray_tests(depth, nlevels, tests_per_level)
  local place = "D:" .. depth
  crawl.message("Running find_ray tests on " .. place)
  debug.goto_place(place)

  for lev_i = 1, nlevels do
    debug.flush_map_memory()
    debug.generate_level()
    for t_i = 1, tests_per_level do
      test_findray()
    end
  end
end

for depth = 1, 16 do
  run_findray_tests(depth, 1, 10)
end
