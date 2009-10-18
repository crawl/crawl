-- Monster placement tests.

local place = dgn.point(20, 20)

local function place_monster_on(x, y, monster, feature)
  dgn.grid(place.x, place.y, feature)
  return dgn.create_monster(x, y, monster)
end

local function assert_place_monster_on(monster, feature)
  dgn.dismiss_monsters()
  crawl.mpr("Placing " .. monster .. " on " .. feature)
  assert(place_monster_on(place.x, place.y, monster, feature),
         "Could not place monster " .. monster .. " on " .. feature)
end

assert_place_monster_on("quokka", "floor")
assert_place_monster_on("necrophage", "altar_zin")
assert_place_monster_on("rat", "shallow water")

-- [ds] One wonders why due has this morbid fetish involving flying
-- skulls and lava...
assert_place_monster_on("flying skull", "lava")
assert_place_monster_on("rock worm", "rock_wall")
