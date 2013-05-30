------------------------------------------------------------------------------
-- theme.lua: Theme functions for different branches
--
------------------------------------------------------------------------------

theme = {}
theme.D = {}

-- Tilesets for cavelike D layouts. Optionally can use an emerald caverns
-- theme.
function theme.D.caves(e)
  if not you.in_branch("D") then return; end

  local variants = {
    { weight = 10, wall = 'wall_pebble_lightgray', floor = 'floor_pebble_brown' }, -- Classic
    { weight = 1, wall = 'wall_emerald', floor = 'floor_pebble_green', glyph = 'b' },      -- Classic crystal
    { weight = 10, wall = 'wall_pebble_darkgray', floor = 'floor_pebble_brown' },
    { weight = 10, wall = 'wall_pebble_lightgray', floor = 'floor_pebble_darkgray' },
    { weight = 10, wall = 'wall_pebble_brown', floor = 'floor_pebble_darkgray' },
    { weight = 10, wall = 'wall_pebble_brown', floor = 'floor_pebble_darkbrown' },
    { weight = 10, wall = 'wall_pebble_lightgray', floor = 'floor_pebble_darkbrown' },
    { weight = 10, wall = 'wall_pebble_white', floor = 'floor_pebble_darkbrown' },
    { weight = 10, wall = 'wall_pebble_darkbrown', floor = 'floor_pebble_darkgray' },
    { weight = 10, wall = 'wall_pebble_midbrown', floor = 'floor_pebble_darkgray' },
    { weight = 10, wall = 'wall_pebble_darkbrown', floor = 'floor_pebble_darkbrown' },
    { weight = 10, wall = 'wall_pebble_midbrown', floor = 'floor_pebble_darkbrown' },
  }

  local chosen = util.random_weighted_from("weight", variants)

  -- Apply the settings
  if chosen.wall ~= nil then
    if chosen.glyph ~= nil then
      e.subst('x = '..chosen.glyph)
      e.tile(chosen.glyph..' = '..chosen.wall)
    else
      e.lrocktile(chosen.wall)
    end
  end
  if chosen.floor ~= nil then
    e.lfloortile(chosen.floor)
  end
end

function theme.D.caves_classic(e)
  if not you.in_branch("D") then return; end
  e.lrocktile('wall_pebble_lightgray')
  e.lfloortile('floor_pebble_brown')

  if crawl.one_chance_in(5) then
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
