-- Counts monsters in a specify level, or level range.
local niters = 150
local output_file = "monster-report.out"

local excluded_things = util.set({ "plant", "fungus", "bush" })

local function count_monsters_at(place, set)
  debug.goto_place(place)
  test.regenerate_level()

  local monster_set = set or { }
  for mons in test.level_monster_iterator() do
    local mname = mons.name
    if not excluded_things[mname] then
      local count = monster_set[mname] or 0
      monster_set[mname] = count + 1
    end
  end
  return monster_set
end

local function report_monster_counts_at(place, mcount_map)
  local text = ''
  text = text .. "\n-------------------------------------------\n"
  text = text .. place .. " monsters per-level\n"
  text = text .. "-------------------------------------------\n"

  local monster_counts = util.pairs(mcount_map)
  table.sort(monster_counts, function (a, b)
                               return a[2] > b[2]
                             end)

  local total = 0
  for _, monster_pop in ipairs(monster_counts) do
    total = total + monster_pop[2]
  end

  for _, monster_pop in ipairs(monster_counts) do
    text = text .. string.format("%6.2f (%6.2f%%)   %s\n",
                                 monster_pop[2] * 1.0 / niters,
                                 monster_pop[2] * 100.0 / total,
                                 monster_pop[1])
  end
  return text
end

local function report_monster_counts(mcounts)
  local places = util.keys(mcounts)
  table.sort(places)

  local text = ''
  for _, place in ipairs(places) do
    text = text .. report_monster_counts_at(place, mcounts[place])
  end
  file.writefile(output_file, text)
  crawl.mpr("Dumped monster stats to " .. output_file)
end

local function count_monsters_from(start_place, end_place)
  local place = start_place
  local mcounts = { }
  while place do
    local mset = { }
    for i = 1, niters do
      crawl.mesclr()
      crawl.mpr("Counting monsters at " .. place .. " (" ..
                i .. "/" .. niters .. ")")
      count_monsters_at(place, mset)
    end
    mcounts[place] = mset

    if place == end_place then
      break
    end

    place = test.deeper_place_from(place)
  end

  report_monster_counts(mcounts)
end

local function parse_resets(resets)
  local pieces = crawl.split(resets, ",")
  local resets = { }
  for _, p in ipairs(pieces) do
    local _, _, place, depth = string.find(p, "^(.+)=(%d+)$")
    table.insert(resets, { place, tonumber(depth) })
  end
  return resets
end

local function branch_resets()
  local args = crawl.script_args()
  local resets = { }
  for _, arg in ipairs(args) do
    local _, _, rawresets = string.find(arg, "^-reset=(.*)")
    if rawresets then
      util.append(resets, parse_resets(rawresets))
    end
  end
  return resets
end

local function set_branch_depths()
  for _, reset in ipairs(branch_resets()) do
    debug.goto_place(reset[1], reset[2])
  end
end

local function start_end_levels()
  local args = script.simple_args()
  if #args == 0 then
    script.usage([[
Usage: place-population <start> [<end>]
For instance: place-population Shoal:1 Shoal:5
              place-population Lair:3

You may optionally force branches to have entrances at specific places
with:
              place-population -reset=Lair:1=8,Snake:1=3 Snake:5

With the general form:

                 -reset=<place>=<depth>[,<place>=<depth>,...]

where <place> is a valid place name as used in .des files, and <depth> is the
depth of the branch's entrance in its parent branch. Thus -reset=Snake:1=3
implies that the entrance of the Snake Pit is assumed to be on Lair:3.
]])
  end
  return args[1], args[2] or args[1]
end

set_branch_depths()
local lstart, lend = start_end_levels()
count_monsters_from(lstart, lend)
