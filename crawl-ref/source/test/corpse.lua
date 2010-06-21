-----------------------------------------------------------------------
-- Tests for corpse gen.
-----------------------------------------------------------------------

local p = dgn.point(20, 20)

debug.goto_place("D:20")

local function ok(corpse, pattern)
  dgn.reset_level()
  dgn.grid(p.x, p.y, 'floor')
  dgn.create_item(p.x, p.y, corpse)

  local items = dgn.items_at(p.x, p.y)
  assert(#items > 0, "Could not create item for '" .. corpse .. "'")
  assert(#items == 1, "Unexpected item count for '" .. corpse .. "'")

  local item = items[1]
  local name = item.name(item, '')

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
ok("any corpse", "corpse")
ok("rat chunk", "chunk of rat")
fail("zombie chunk")