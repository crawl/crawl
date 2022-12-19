-- Monster placement tests.

local place = dgn.point(20, 20)

local function place_monster_on(x, y, monster, feature)
  dgn.grid(place.x, place.y, feature)
  return dgn.create_monster(x, y, monster)
end

local function assert_place_monster_on(monster, feature)
  feature = feature or 'floor'
  dgn.dismiss_monsters()
  crawl.message("Placing " .. monster .. " on " .. feature)
  assert(place_monster_on(place.x, place.y, monster, feature),
         "Could not place monster " .. monster .. " on " .. feature)
end

assert_place_monster_on("quokka", "floor")
assert_place_monster_on("necrophage", "altar_zin")
assert_place_monster_on("rat", "shallow water")

-- [ds] One wonders why due has this morbid fetish involving flying
-- skulls and lava...
assert_place_monster_on("flying skull", "lava")
assert_place_monster_on("dryad", "tree")
assert_place_monster_on("cyan ugly thing")
assert_place_monster_on("purple very ugly thing")

dgn.dismiss_monsters()
