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

function test.regenerate_level(place, use_random_maps)
  if place then
    debug.goto_place(place)
  end
  debug.flush_map_memory()
  dgn.reset_level()
  debug.generate_level(use_random_maps)
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

function test.level_monster_iterator(filter)
  return iter.mons_rect_iterator(dgn.point(1, 1),
                                 dgn.point(dgn.GXM - 2, dgn.GYM - 2),
                                 filter)
end

test.is_down_stair = dgn.feature_set_fn("stone_stairs_down_i",
                                        "stone_stairs_down_ii",
                                        "stone_stairs_down_iii")

function test.level_has_down_stair()
  for y = 1, dgn.GYM - 2 do
    for x = 1, dgn.GXM - 2 do
      local dfeat = dgn.grid(x, y)
      if test.is_down_stair(dfeat) then
        return true
      end
    end
  end
  return false
end

function test.deeper_place_from(place)
  if test.level_has_down_stair() then
    local _, _, branch, depth = string.find(place, "(%w+):(%d+)")
    return branch .. ":" .. (tonumber(depth) + 1)
  end
  return nil
end

util.namespace('script')

function script.simple_args()
  local args = crawl.script_args()
  return util.filter(function (arg)
                       return string.find(arg, '-') ~= 1
                     end,
                     args)
end

function script.usage(ustr)
  ustr = string.gsub(string.gsub(ustr, "^%s+", ""), "%s+$", "")
  error("\n" .. ustr)
end
