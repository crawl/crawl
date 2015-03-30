-- Counts monsters in a specified map
local niters = 10
local output_file = "monster-report.out"

local sort_type = "XP"

local function canonical_name(mons)
  local shapeshifter = mons.shapeshifter
  if shapeshifter then
    return shapeshifter
  end

  local dancing_weapon = mons.dancing_weapon
  if dancing_weapon then
    return dancing_weapon
  end

  local mname = mons.name

  if string.find(mname, 'very ugly thing$') then
    return "very ugly thing"
  end

  if string.find(mname, 'ugly thing$') then
    return "ugly thing"
  end

  local _, _, hydra_suffix = string.find(mname, '.*-headed (.*)')
  if hydra_suffix then
    mname = hydra_suffix
  end

  return mname
end

local function count_monsters_at(place, set)
  if place == nil then
    return nil
  end

  debug.goto_place("D:1")
  test.regenerate_level(nil, true)
  debug.dismiss_monsters()
  dgn.place_map(dgn.map_by_name(place), true)

  local monsters_here = set or {  }
  for mons in test.level_monster_iterator() do
    local unique = mons.unique
    local mname = canonical_name(mons)
    if unique and group_uniques then
      mname = "[UNIQUE]"
    end
    local mstat = monsters_here[mname] or { count = 0, exp = 0 }
    mstat.count = mstat.count + 1
    mstat.exp   = mstat.exp + mons.experience
    monsters_here[mname] = mstat
  end
  return monsters_here
end

local function report_monster_counts_at(place, mcount_map)
  local text = ''
  text = text .. "\n------------------------------------------------------------\n"
  text = text .. place .. " monsters, " .. niters .. " iteration\n"
  text = text .. "------------------------------------------------------------\n"

  local monster_counts = util.pairs(mcount_map)
  table.sort(monster_counts, function (a, b)
-- Further sort options can be added later if desired. Default is XP, -sort_count sorts by count/percentage instead.
    if sort_type == 'count' then
        return a[2].total > b[2].total
    else
        return a[2].etotal > b[2].etotal
    end
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

local function report_monster_counts(summary, mcounts)
  local places = util.keys(mcounts)
  table.sort(places)

  local text = ''

  if multiple_levels then
    text = text .. report_monster_counts_at(start_level .. " to " .. end_level,
                                            summary)
  end

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

-- Given a table mapping places to lists of niters sets of monster counts
-- and a list of places, returns one summary stats table.
local function calculate_summary_stats(place_totals, places)
  local function combine_iter_results(master, inst)
    for mons, stat in pairs(inst) do
      local mstat = master[mons] or { count = 0, exp = 0 }
      mstat.count = mstat.count + stat.count
      mstat.exp   = mstat.exp + stat.exp
      master[mons] = mstat
    end
  end

  local iters = { }
  for i = 1, niters do
    local iterstats = { }
    for _, place in ipairs(places) do
      combine_iter_results(iterstats, place_totals[place][i])
    end
    table.insert(iters, iterstats)
  end
  return calculate_monster_stats(iters)
end

local function count_monsters_in(map)
  local mcounts = { }
  local summary = { }
  local places = { }
  local place_totals = { }

  table.insert(places, map)

  local mset = { }

  local iter_mpops = { }
  for i = 1, niters do
    crawl.message("Counting monsters for " .. map .. ", depth: " ..
                  you.absdepth() ..
                  " (" ..
                  i .. "/" .. niters .. ")")
    local res = count_monsters_at(map)
    table.insert(iter_mpops, res)
  end

  mcounts[map] = calculate_monster_stats(iter_mpops)

  report_monster_counts(summary, mcounts)
end

local mplaces = script.simple_args()
if #mplaces == 0 then
  script.usage("Usage: genmap <map>")
end

if #mplaces == 4 then
  output_file = mplaces[2]
end

count_monsters_in(mplaces[1])
