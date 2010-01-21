-- Counts monsters in a specify level, or level range.
local niters = 150
local output_file = "monster-report.out"

local excluded_things = util.set({}) -- "plant", "fungus", "bush" })
local use_random_maps = true

local function count_monsters_at(place, set)
  debug.goto_place(place)
  test.regenerate_level(nil, use_random_maps)

  local monsters_here = set or {  }
  for mons in test.level_monster_iterator() do
    local mname = mons.name
    if not excluded_things[mname] then
      local mstat = monsters_here[mname] or { count = 0, exp = 0 }
      mstat.count = mstat.count + 1
      mstat.exp   = mstat.exp + mons.experience
      monsters_here[mname] = mstat
    end
  end
  return monsters_here
end

local function report_monster_counts_at(place, mcount_map)
  local text = ''
  text = text .. "\n-------------------------------------------\n"
  text = text .. place .. " monsters per-level ("
  text = text .. (use_random_maps and "using random maps"
                  or "NO random maps") .. ")\n"
  text = text .. "-------------------------------------------\n"

  local monster_counts = util.pairs(mcount_map)
  table.sort(monster_counts, function (a, b)
                               return a[2].total > b[2].total
                             end)

  local total = 0
  for _, monster_pop in ipairs(monster_counts) do
    if monster_pop[1] ~= 'TOTAL' then
      total = total + monster_pop[2].total
    end
  end

  text = text .. "Count  | XPAv  | XPMin | XPMax | XPSigma | CountMin | CountMax | CountSigma |    %    | Monster\n"
  for _, monster_pop in ipairs(monster_counts) do
    text = text .. string.format("%6.2f | %5d | %5d | %5d | %7d | %8d | %8d | %10.2f | %6.2f%% | %s\n",
                                 monster_pop[2].mean,
                                 monster_pop[2].emean,
                                 monster_pop[2].emin,
                                 monster_pop[2].emax,
                                 monster_pop[2].esigma,
                                 monster_pop[2].min,
                                 monster_pop[2].max,
                                 monster_pop[2].sigma,
                                 monster_pop[2].total * 100.0 / total,
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

local function calculate_monster_stats(iter_mpops)
  local function blank_stats()
    return {
      total = 0, max = 0, min = 10 ^ 10,
      etotal = 0, emax = 0, emin = 10 ^ 10, eiters = { },
      pops = { } }
  end

  local function update_mons_stats(stat, instance_stat)
    local function update_total_max_min(value, total, max, min)
      stat[total] = stat[total] + value
      if value > stat[max] then
        stat[max] = value
      end
      if value < stat[min] then
        stat[min] = value
      end
    end
    update_total_max_min(instance_stat.count, 'total', 'max', 'min')
    update_total_max_min(instance_stat.exp, 'etotal', 'emax', 'emin')
    table.insert(stat.pops, instance_stat.count)
    table.insert(stat.eiters, instance_stat.exp)
    return stat
  end

  local function sum(list)
    local total = 0
    for _, num in ipairs(list) do
      total = total + num
    end
    return total
  end

  local function combine_stats(target, src)
    local total = sum(src.pops)
    local etotal = sum(src.eiters)
    table.insert(target.pops, total)
    table.insert(target.eiters, etotal)
    target.total = target.total + total
    target.etotal = target.etotal + etotal
    target.max = math.max(target.max, total)
    target.min = math.min(target.min, total)
    target.emax = math.max(target.emax, etotal)
    target.emin = math.min(target.emin, etotal)
  end

  local final_stats = { TOTAL = blank_stats() }
  local n = #iter_mpops
  for _, mpop in ipairs(iter_mpops) do
    local global_stat = blank_stats()
    for mons, istat in pairs(mpop) do
      local mstats = final_stats[mons] or blank_stats()
      final_stats[mons] = update_mons_stats(mstats, istat)
      update_mons_stats(global_stat, istat)
    end
    combine_stats(final_stats.TOTAL, global_stat)
  end

  local function calc_mean_sigma(total, instances)
    local mean = total / n
    local totaldelta2 = 0
    for _, count in ipairs(instances) do
      local delta = count - mean
      totaldelta2 = totaldelta2 + delta * delta
    end
    -- There will be no instances with count 0, handle those:
    for extra = 1, (n - #instances) do
      totaldelta2 = totaldelta2 + mean * mean
    end
    local sigma = math.sqrt(totaldelta2 / n)
    return mean, sigma
  end

  for mons, stat in pairs(final_stats) do
    stat.mean, stat.sigma = calc_mean_sigma(stat.total, stat.pops)
    stat.emean, stat.esigma = calc_mean_sigma(stat.etotal, stat.eiters)
  end
  return final_stats
end

local function count_monsters_from(start_place, end_place)
  local place = start_place
  local mcounts = { }
  while place do
    local mset = { }

    local iter_mpops = { }
    for i = 1, niters do
      crawl.message("Counting monsters at " .. place .. " (" ..
                    i .. "/" .. niters .. ")")
      local res = count_monsters_at(place)
      table.insert(iter_mpops, res)
    end

    mcounts[place] = calculate_monster_stats(iter_mpops)

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
    if arg == '-nomaps' then
      use_random_maps = false
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

You can also disable the use of random maps during level generation with:
               -nomaps
]])
  end
  return args[1], args[2] or args[1]
end

set_branch_depths()
local lstart, lend = start_end_levels()
count_monsters_from(lstart, lend)
