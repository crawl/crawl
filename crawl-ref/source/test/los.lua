-- Vet LOS for symmetry.

local FAILMAP = 'losfail.map'
local checks = 0

local function test_losight_symmetry()
  -- Clear messages to prevent them accumulating and forcing a --more--
  crawl.mesclr()
  -- Send the player to a random spot on the level.
  you.random_teleport()

  -- Forcibly redo LOS.
  you.losight()

  checks = checks + 1
  local you_x, you_y = you.pos()

  local visible_spots = { }
  for y = -8, 8 do
    for x = -8, 8 do
      if x ~= 0 or y ~= 0 then
        local px, py = x + you_x, y + you_y
        if you.see_grid(px, py) then
          table.insert(visible_spots, { px, py })
        end
      end
    end
  end

  -- For each position in LOS, jump to that position and make sure we
  -- can see the original spot.
  for _, spot in ipairs(visible_spots) do
    local x, y = unpack(spot)
    you.moveto(x, y)
    you.losight()
    if not you.see_grid(you_x, you_y) then
      -- Draw the view to show the problem.
      crawl.redraw_view()
      local this_p = dgn.point(x, y)
      local you_p = dgn.point(you_x, you_y)
      dgn.grid(you_x, you_y, "floor_special")
      dgn.dbg_dump_map(FAILMAP)
      assert(false,
             "LOS asymmetry detected (iter #" .. checks .. "): " .. you_p ..
               " sees " .. this_p .. ", but not vice versa." ..
               " Map saved to " .. FAILMAP)
    end
  end
end

local function test_gridseegrid_symmetry()
  -- Clear messages to prevent them accumulating and forcing a --more--
  crawl.mesclr()
  -- Send the player to a random spot on the level.
  you.random_teleport()

  checks = checks + 1
  local you_x, you_y = you.pos()

  for y = -9, 9 do
    for x = -9, 9 do
      local px, py = x + you_x, y + you_y
      if (x ~= 0 or y ~= 0) and dgn.in_bounds(px, py) then
        local foreward = dgn.grid_see_grid(you_x, you_y, px, py)
        local backward = dgn.grid_see_grid(px, py, you_x, you_y)
        this_p = dgn.point(you_x, you_y)
        other_p = dgn.point(px, py)
        if not forward then
          you.moveto(px, py)
          local temp = this_p
          this_p = other_p
          other_p = temp
        end
        if forward ~= backward then
          dgn.grid(other_p.x, other_p.y, "floor_special")
          dgn.dbg_dump_map(FAILMAP)
          assert(false,
                 "grid_see_grid asymmetry detected (iter #" .. checks .. "): "
                   .. this_p .. " sees " .. other_p .. ", but not vice versa."
                   .. " Map saved to " .. FAILMAP)
        end
      end
    end
  end
end

local function run_los_tests(depth, nlevels, tests_per_level)
  local place = "D:" .. depth
  crawl.mpr("Running LOS tests on " .. place)
  dgn.dbg_goto_place(place)

  for lev_i = 1, nlevels do
    dgn.dbg_flush_map_memory()
    dgn.dbg_generate_level()
    for t_i = 1, tests_per_level do
      test_losight_symmetry()
    end
  end

  for lev_i = 1, nlevels do
    dgn.dbg_flush_map_memory()
    dgn.dbg_generate_level()
    for t_i = 1, tests_per_level do
      test_gridseegrid_symmetry()
    end
  end
end

for depth = 1, 27 do
  run_los_tests(depth, 1, 10)
end
