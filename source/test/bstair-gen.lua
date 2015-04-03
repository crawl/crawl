local niters = 5

local function test_branch_stair_level(place, stair, nlevels)
  debug.goto_place(place)
  for i = 1, nlevels do
    crawl.message(place .. " branch stair test " .. i .. " of " .. nlevels)
    crawl.delay(0)
    test.regenerate_level()
    test.map_assert(test.find_feature(stair),
                    "No " .. stair .. " created at " .. place)
  end
end

local function test_branch_stair_places(nlevels, level_stairs)
  for _, place in ipairs(level_stairs) do
    test_branch_stair_level(place[1], place[2], nlevels)
  end
end

test_branch_stair_places(niters,
                 { { "Depths:$", "enter_zot" },
                   { "Depths:3", "enter_pandemonium" },
                   { "Depths:2", "enter_hell" } })
