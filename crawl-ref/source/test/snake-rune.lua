-- Walks down the dungeon to Snake:$ and tests for the existence of the rune.
-- This is a more exhaustive test than rune-gen.lua since it generates all
-- intermediate levels and activates the dungeon connectivity code.

local niters = 10
local current_iter = 0

local branch_entrance_feats = {
  { dgn.fnum("enter_lair"), "Lair:1" },
  { dgn.fnum("enter_snake_pit"), "Snake:1" }
}

local junk_feat_fn = dgn.feature_set_fn("rock_wall", "floor", "stone_wall")

local function thing_exists_fn(thing)
  crawl.message('z')
  return function()
           crawl.message('y')
           return test.level_contains_item(thing)
         end
end

local function visit_branch_end_from(start, stair_places, final_predicate)
  crawl.message('0')
  while true do
    crawl.message("Visiting (" .. current_iter .. ") " .. start)
    debug.goto_place(start)
    crawl.message('1')

    dgn.reset_level()
    crawl.message('2')
    debug.generate_level()
    crawl.message('3')

    local downstairs = { }
    crawl.message('4')
    for y = 1, dgn.GYM - 2 do
      for x = 1, dgn.GXM - 2 do
        local dfeat = dgn.grid(x, y)
        if not junk_feat_fn(dfeat) then
          crawl.message('8')
          for _, place in ipairs(stair_places) do
            if dfeat == place[1] then
              crawl.message('10')
              return visit_branch_end_from(place[2], stair_places,
                                           final_predicate)
            end
          end

          if test.is_down_stair(dfeat) then
            crawl.message('14')
            table.insert(downstairs, dgn.point(x, y))
            crawl.message('15')
          end
        end
      end
    end
    crawl.message('19')

    if #downstairs > 0 then
      crawl.message('20')
      local _, _, branch, depth = string.find(start, "(%w+):(%d+)")
      crawl.message('21')
      start = branch .. ":" .. (tonumber(depth) + 1)
      crawl.message('22')
    else
      crawl.message('23')
      test.map_assert(final_predicate(),
                      "Place " .. start .. " does not satisfy predicate")
      crawl.message('24')
      return
    end
    crawl.message('26')
  end
  crawl.message('27')
end

crawl.message('a')
for i = 1, niters do
  crawl.message("Visiting Snake:$ the hard way")
  current_iter = i
  crawl.message('b')
  debug.flush_map_memory()
  crawl.message('c')
  entry = crawl.random_range(3, 7)
  crawl.message('d')
  debug.goto_place("Snake:1", entry)
  crawl.message('e')
  visit_branch_end_from("D:1",
                        branch_entrance_feats,
                        thing_exists_fn("serpentine rune"))
  crawl.message('f')
end
crawl.message('g')
