-- Sanity checks that should be run just before the game starts.

local function assert_place_has_map(place)
  assert(dgn.map_by_place(place), "No map found for " .. place)
end

local function sanity_checks()
  local places = {
    "Zot:$", "Snake:$", "Swamp:$", "Spider:$", "Slime:$", "Elf:$",
    "Vaults:$", "Tomb:$", "Coc:$", "Tar:$", "Dis:$", "Geh:$"
  }
  for _, place in ipairs(places) do
    assert_place_has_map(place)
  end
end

sanity_checks()
