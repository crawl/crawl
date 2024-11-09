-- Vet LOS for symmetry.

local FAILMAP = 'losfail.map'
local checks = 0

local function test_cellseecell_symmetry()
  -- Send the player to a random spot on the level.
  you.random_teleport()
  crawl.redraw_view()

  checks = checks + 1
  local you_x, you_y = you.pos()

  for y = -9, 9 do
    for x = -9, 9 do
      local px, py = x + you_x, y + you_y
      if (x ~= 0 or y ~= 0) and dgn.in_bounds(px, py) then
        local forward = los.cell_see_cell(you_x, you_y, px, py)
        local backward = los.cell_see_cell(px, py, you_x, you_y)
        this_p = dgn.point(you_x, you_y)
        other_p = dgn.point(px, py)
        if not forward then
          you.moveto(px, py)
          local temp = this_p
          this_p = other_p
          other_p = temp
        end
        if (forward and not backward) or (not forward and backward) then
          dgn.fprop_changed(other_p.x, other_p.y, "highlight")
          debug.dump_map(FAILMAP)
          assert(false,
                 "cell_see_cell asymmetry detected (iter #" .. checks .. "): "
                   .. this_p .. " sees " .. other_p .. ", but not vice versa."
                   .. " Map saved to " .. FAILMAP)
        end
      end
    end
  end
end

local function run_los_tests(depth, nlevels, tests_per_level)
  local place = "D:" .. depth
  crawl.message("Running LOS tests on " .. place)
  debug.goto_place(place)

  for lev_i = 1, nlevels do
    debug.reset_player_data()
    debug.generate_level()
    for t_i = 1, tests_per_level do
      test_cellseecell_symmetry()
    end
  end
end

for depth = 1, 15 do
  run_los_tests(depth, 1, 1)
end
