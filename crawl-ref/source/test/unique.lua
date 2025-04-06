-- Test correct handling of you.unique_creatures.

local function test_uniques()
  debug.save_uniques()
  debug.generate_level()
  assert(debug.check_uniques(),
         "you.uniques out of sync with actually placed uniques")
end

local function test_uniques_branch(branch, depth)
  crawl.message("Running unique placement tests in branch " .. branch)
  debug.reset_player_data()
  for d = 1, depth do
    debug.goto_place(branch .. ":" .. d)
    test_uniques()
  end
end

local function test_uniques_blank(branch, depth, nlevels)
  local place = branch .. ":" .. depth
  crawl.message("Running blanked placement test at " .. place)
  debug.goto_place(place)
  for lev_i = 1, nlevels do
    debug.reset_player_data()
    test_uniques()
  end
end

local function test_uniques_random(branch, depth, nlevels)
  local place = branch .. ":" .. depth
  crawl.message("Running randomized placement test at " .. place)
  debug.goto_place(place)
  for lev_i = 1, nlevels do
    debug.reset_player_data()
    debug.randomize_uniques()
    test_uniques()
  end
end

local function run_unique_tests()
  test_uniques_branch("D", 15)
  test_uniques_branch("Depths", 4)
  test_uniques_branch("Dis", 7)

  for depth = 1, 15 do
    test_uniques_blank("D", depth, 1)
    test_uniques_random("D", depth, 1)
  end
  for depth = 1, 4 do
    test_uniques_blank("Depths", depth, 1)
    test_uniques_random("Depths", depth, 1)
  end

  for depth = 1, 7 do
    test_uniques_blank("Dis", depth, 1)
    if depth < 7 then
      -- otherwise we get failures for randomly placed Dispater
      -- getting placed again
      test_uniques_random("Dis", depth, 1)
    end
  end

  -- why does this go backwards??
  for depth = 4, 1, -1 do
    test_uniques_blank("Swamp", depth, 1)
    if depth < 4 then
      -- otherwise problem with lernaean, like Dispater above
      test_uniques_random("Swamp", depth, 1)
    end
  end
end

run_unique_tests()
