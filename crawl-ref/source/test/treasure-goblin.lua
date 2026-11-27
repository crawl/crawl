-- Treasure goblins never fight. One that knows of a foe should keep running
-- from it for as long as it can see it, and when its patience runs out it
-- should stand still to open a portal instead.

local you_pos = dgn.point(20, 20)
local foe_pos = dgn.point(22, 20)
local goblin_pos = dgn.point(24, 20)

debug.goto_place("D:12")
dgn.reset_level()
dgn.fill_grd_area(1, 1, dgn.GXM - 2, dgn.GYM - 2, 'floor')
you.moveto(you_pos.x, you_pos.y)
debug.los_changed()

local goblin = dgn.create_monster(goblin_pos.x, goblin_pos.y,
                                  "generate_awake treasure goblin")
assert(goblin, "Could not create a treasure goblin")

-- The goblin's coordinates are given relative to the player.
local function distance()
    return math.max(math.abs(goblin.x), math.abs(goblin.y))
end

local function act()
    goblin.add_energy(12)
    goblin.run_ai()
end

-- An unaware goblin is in no hurry to leave.
act()
test.eq(goblin.beh, "wander", "the goblin took fright at nothing")
assert(not goblin.has_prop("treasure_goblin_escape"),
       "The goblin's escape was timed before it noticed anything")

-- Give the goblin something to notice. No time passes for the test player,
-- so they are never noticed by themselves; but once the goblin has a foe of
-- any kind it may well decide that the player beyond it is the real danger.
-- Either way, it should run in the same direction.
assert(dgn.create_monster(foe_pos.x, foe_pos.y,
                          "generate_awake orc warrior att:friendly"),
       "Could not create something for the goblin to run from")
for i = 1, 100 do
    if goblin.beh == "flee" then
        break
    end
    goblin.handle_behaviour()
end
test.eq(goblin.beh, "flee", "the goblin did not take fright")

local start = distance()
for i = 1, 6 do
    act()
    test.eq(goblin.beh, "flee",
            "the goblin stopped fleeing after " .. i .. " moves")
end
assert(distance() >= start + 3,
       "The goblin did not run: it was " .. start .. " away and is now "
       .. distance() .. " away")

-- The player is shown how close it is to leaving.
goblin.set_prop("treasure_goblin_escape", you.time() + 1000)
goblin.set_prop("treasure_goblin_limit", you.time() + 2000)
assert(goblin.get_info():status("looking for a way out"),
       "The goblin's wish to leave is not shown")
goblin.set_prop("treasure_goblin_escape", you.time() + 30)
assert(goblin.get_info():status("about to escape"),
       "The goblin's imminent escape is not shown")

-- Once it has been left alone for long enough, it stands still to make its
-- portal rather than running any further.
goblin.set_prop("treasure_goblin_escape", 0)
goblin.set_prop("treasure_goblin_limit", 0)
act()
local stopped_at = distance()
assert(goblin.get_info():status("opening a portal"),
       "The goblin did not start opening a portal when its time was up")
for i = 1, 3 do
    act()
end
test.eq(distance(), stopped_at, "the goblin kept running with a portal open")

dgn.dismiss_monsters()
