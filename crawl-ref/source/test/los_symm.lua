-- Vet LOS for symmetry.

local FAILMAP = 'losfail.map'
local checks = 0

local function test_losight_symmetry()
  -- Send the player to a random spot on the level.
  you.random_teleport()

  checks = checks + 1
  local you_x, you_y = you.pos()

  local visible_spots = { }
  for y = -8, 8 do
    for x = -8, 8 do
      if x ~= 0 or y ~= 0 then
        local px, py = x + you_x, y + you_y
        -- It makes sense to check visibility of a cell on the border, but
        -- we'd be unable to test the reverse.
        if dgn.in_bounds(px, py) and you.see_cell(px, py) then
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
    if not you.see_cell(you_x, you_y) then
      -- Draw the view to show the problem.
      crawl.redraw_view()
      local this_p = dgn.point(x, y)
      local you_p = dgn.point(you_x, you_y)
      dgn.fprop_changed(you_x, you_y, "highlight")
      debug.dump_map(FAILMAP)
      assert(false,
             "LOS asymmetry detected (iter #" .. checks .. "): " .. you_p ..
               " sees " .. this_p .. ", but not vice versa." ..
               " Map saved to " .. FAILMAP)
    end
  end
end

local function run_los_tests(depth, nlevels, tests_per_level)
  local place = "D:" .. depth
  crawl.message("Running LOS tests on " .. place)
  debug.goto_place(place)

  for lev_i = 1, nlevels do
    debug.flush_map_memory()
    debug.generate_level()
    for t_i = 1, tests_per_level do
      test_losight_symmetry()
    end
  end
end

for depth = 1, 16 do
  run_los_tests(depth, 1, 3)
end
