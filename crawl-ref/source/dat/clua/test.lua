-- Support code used primarily for tests. This is loaded only when running
-- tests, not during normal Crawl execution.

util.namespace('test')

test.FAILMAP = 'level-fail.map'

function test.map_assert(condition, message)
  if not condition then
    debug.dump_map(test.FAILMAP)
    assert(false, message .. " (map dumped to " .. test.FAILMAP .. ")")
  end
  return condition
end

function test.regenerate_level(place)
  if place then
    debug.goto_place(place)
  end
  debug.flush_map_memory()
  dgn.reset_level()
  debug.generate_level()
end

function test.level_contains_item(item)
  for y = 1, dgn.GYM - 2 do
    for x = 1, dgn.GXM - 2 do
      if iter.stack_search(x, y, item) then
        return true
      end
    end
  end
  return false
end