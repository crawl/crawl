-- Generates maps for the supplied place names.

local places = script.simple_args()
if #places == 0 then
  script.usage("Usage: genlevel <place> [<place2> ...]")
end

local function map_dump_name_for_place(place)
  return "dump-" .. string.gsub(place, ":", "-") .. ".map"
end

for _, place in ipairs(places) do
  debug.goto_place(place)
  test.regenerate_level()

  local filename = map_dump_name_for_place(place)
  crawl.mpr("Dumping map of " .. place .. " to " .. filename)
  debug.dump_map(filename)
end