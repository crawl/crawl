-- Vet LOS for symmetry.

local FAILMAP = 'losfail.map'
local checks = 0

local function test_los()
  -- Send the player to a random spot on the level.
  you.random_teleport()

  -- Forcibly redo LOS.
  you.losight()

  -- And draw the view to keep the watcher entertained.
  crawl.redraw_view()

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

local function run_los_tests(depth, nlevels, tests_per_level)
  local place = "D:" .. depth
  crawl.mpr("Running LOS tests on " .. place)
  dgn.dbg_goto_place(place)

  for lev_i = 1, nlevels do
    dgn.dbg_flush_map_memory()
    dgn.dbg_generate_level()
    for t_i = 1, tests_per_level do
      test_los()
    end
  end
end

for depth = 1, 27 do
  run_los_tests(depth, 10, 100)
end