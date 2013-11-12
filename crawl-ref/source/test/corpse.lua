-----------------------------------------------------------------------
-- Tests for corpse gen.
-----------------------------------------------------------------------

local p = dgn.point(20, 20)

debug.goto_place("Depths:2")

local function ok(corpse, pattern)
  dgn.reset_level()
  dgn.fill_grd_area(1, 1, dgn.GXM - 2, dgn.GYM - 2, 'floor')
  dgn.create_item(p.x, p.y, corpse)

  local items = dgn.items_at(p.x, p.y)
  assert(#items > 0, "Could not create item for '" .. corpse .. "'")
  assert(#items == 1, "Unexpected item count for '" .. corpse .. "'")

  local item = items[1]
  local name = item.name('')

  pattern = pattern or corpse
  assert(string.find(name, pattern, 1, true),
         "Unexpected item name: " .. name .. ", expected: " .. pattern)
end

local function fail(corpse, pattern)
  assert(not pcall(function ()
                     ok(corpse, pattern)
                   end),
         "Expected item '" .. corpse .. "' to fail, but it did not")
end

-- All set up!
ok("hydra corpse")
ok("hippogriff skeleton")
for i = 1,100 do
  ok("any corpse", "corpse")
end
ok("rat chunk", "chunk of rat")
fail("zombie chunk")
fail("giant eyeball corpse")
ok("orc warrior corpse", "orc corpse")
