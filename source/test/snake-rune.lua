-- Walks down the dungeon to Snake:$ and tests for the existence of the rune.
-- This is a more exhaustive test than rune-gen.lua since it generates all
-- intermediate levels and activates the dungeon connectivity code.

-- XXX randomly segfaulting at the moment... reenable it once that's fixed (so
-- that it doesn't mess up other test results in the meantime
--[[
local niters = 10
local current_iter = 0

local branch_entrance_feats = {
  { dgn.fnum("enter_lair"), "Lair:1" },
  { dgn.fnum("enter_snake_pit"), "Snake:1" }
}

local junk_feat_fn = dgn.feature_set_fn("rock_wall", "floor", "stone_wall")

local function thing_exists_fn(thing)
  return function()
           return test.level_contains_item(thing)
         end
end

local function visit_branch_end_from(start, stair_places, final_predicate)
  while true do
    crawl.message("Visiting (" .. current_iter .. ") " .. start)
    debug.goto_place(start)

    dgn.reset_level()
    debug.generate_level()

    local downstairs = { }
    for y = 1, dgn.GYM - 2 do
      for x = 1, dgn.GXM - 2 do
        local dfeat = dgn.grid(x, y)
        if not junk_feat_fn(dfeat) then
          for _, place in ipairs(stair_places) do
            if dfeat == place[1] then
              return visit_branch_end_from(place[2], stair_places,
                                           final_predicate)
            end
          end

          if test.is_down_stair(dfeat) then
            table.insert(downstairs, dgn.point(x, y))
          end
        end
      end
    end

    if #downstairs > 0 then
      local _, _, branch, depth = string.find(start, "(%w+):(%d+)")
      start = branch .. ":" .. (tonumber(depth) + 1)
    else
      test.map_assert(final_predicate(),
                      "Place " .. start .. " does not satisfy predicate")
      return
    end
  end
end

for i = 1, niters do
  crawl.message("Visiting Snake:$ the hard way")
  current_iter = i
  debug.flush_map_memory()
  entry = crawl.random_range(3, 7)
  debug.goto_place("Snake:1", entry)
  visit_branch_end_from("D:1",
                        branch_entrance_feats,
                        thing_exists_fn("serpentine rune"))
end
]]
