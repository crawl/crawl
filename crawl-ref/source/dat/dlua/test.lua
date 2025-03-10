-- Support code used primarily for tests. This is loaded only when running
-- tests, not during normal Crawl execution.

util.namespace('test')

test.FAILMAP = 'level-fail.map'

function test.eq(actual_value, expected_value, extra)
  if extra then
    assert(actual_value == expected_value,
           "Expected '" .. expected_value .. "', " ..
             "but got '" .. actual_value .. "': " .. extra)
  else
    assert(actual_value == expected_value,
           "Expected '" .. expected_value .. "', " ..
             "but got '" .. actual_value .. "'")
  end
end

function test.map_assert(condition, message)
  if not condition then
    debug.dump_map(test.FAILMAP)
    assert(false, message .. " (map dumped to " .. test.FAILMAP .. ")")
  end
  return condition
end

function test.place_items_at(point, item_spec)
  dgn.create_item(point.x, point.y, item_spec)
  return dgn.items_at(point.x, point.y)
end

function test.find_monsters(mname)
  local monsters = { }
  for mons in test.level_monster_iterator() do
    if mons.name == mname then
      table.insert(monsters, mons)
    end
  end
  return monsters
end

function test.regenerate_level(place, use_random_maps)
  if place then
    debug.goto_place(place)
  end
  debug.reset_player_data() -- TODO: this is overkill for a single level
  dgn.reset_level()
  debug.generate_level(use_random_maps)
end

function test.find_feature(feature)
  local feat = dgn.fnum(feature)
  local function find_feature(p)
    return dgn.grid(p.x, p.y) == feat
  end

  local feature_points = dgn.find_points(find_feature)
  return #feature_points > 0 and feature_points[1] or nil
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
