------------------------------------------------------------------------------
-- theme.lua: Theme functions for different branches
--
------------------------------------------------------------------------------

theme = {}
theme.D = {}

-- Tileset for cavelike D layouts. Optionally can use an emerald caverns
-- theme.
function theme.D.caves(e, allow_crystal)
  if not you.in_branch("D") then return; end
  e.lrocktile('wall_pebble_lightgray')
  e.lfloortile('floor_pebble_brown')

  if (allow_crystal and crawl.one_chance_in(5)) then
    e.subst('x = b')
    e.tile('b = wall_emerald')
    e.lfloortile('floor_pebble_green')
  end
end

-- Maybe convert rock to a random material across the whole level
function theme.level_material(e)
  if not you.in_branch("Lair") and not you.in_branch("Zot")
    and not you.in_branch("Dis")
    and you.absdepth() >= 4 and crawl.one_chance_in(20) then
    e.subst('x : ccv')
  end
end

-- Get a random weighted material for rooms in room layouts
function theme.room_material(wall_type)

    wall_type = wall_type or 'x'

    if you.branch() == "Dis" then
        return 'x'  -- Will be converted to metal
    elseif crawl.one_chance_in(250) then
        return 'b'
    end

    return crawl.random_element {
        [wall_type] = 216,
        ['c'] = 9,
        ['x'] = 9,
        ['v'] = 5
    }

end
