-- Monster casting tests.
-- this is mostly implemented on the c++ side (see debug_check_moncasts), but
-- we use lua to more easily set up the dungeon scenario.
dgn.dismiss_monsters()

local place1 = dgn.point(20, 20)
local place2 = dgn.point(20, 21)


dgn.create_monster(place1.x, place1.y, "generate_awake test statue")
-- first test without a foe. This provides a check on whether targeting flags
-- are set correctly.
debug.check_moncasts(place1.x, place1.y, place2.x, place2.y)

-- then check with a foe
dgn.create_monster(place2.x, place2.y, "generate_awake test statue")
debug.check_moncasts(place1.x, place1.y, place2.x, place2.y)

-- clean up
dgn.dismiss_monsters()
