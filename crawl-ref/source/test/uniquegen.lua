-- Test that randomly generated monsters aren't uniques.

local place = dgn.point(20, 20)
local place2 = dgn.point(50, 50) -- out-of-view of place, to avoid message spam
local count = 100

local function test_place_random_monster()
  dgn.dismiss_monsters()
  dgn.grid(place.x, place.y, "floor")
  -- Try several times, because MONS_NO_MONSTER may be on the random list.
  for attempt = 1, 100 do
    m = dgn.create_monster(place.x, place.y, "random")
    if m then
      break
    end
  end
  assert(m,
         "could not place random monster")
  assert(not m.unique,
         "random monster is unique " .. m.name)
end

local function test_random_unique(branch, depth)
  crawl.message("Running random monster unique tests in branch " .. branch
                .. " through depth " .. depth)
  debug.flush_map_memory()
  for d = 1, depth do
    debug.goto_place(branch .. ":" .. d)
    dgn.grid(place2.x, place2.y, "floor")
    you.moveto(place2.x, place2.y)
    for i = 1, count do
      test_place_random_monster()
    end
  end
end

local function run_random_unique_tests()
  for depth = 1, 15 do
    test_random_unique("D", depth, 3)
  end

  for depth = 1, 5 do
    test_random_unique("Depths", depth, 3)
  end

  for depth = 1, 4 do
    test_random_unique("Dis", depth, 3)
  end

  for depth = 1, 4 do
    test_random_unique("Swamp", depth, 5)
  end
end

run_random_unique_tests()
