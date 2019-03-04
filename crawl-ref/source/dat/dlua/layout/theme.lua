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

    return util.random_choose_weighted {
        {wall_type, 216},
        {'c', 9},
        {'x', 9},
        {'v', 5}
    }

end

-- Get a random weighted material for rooms in room layouts
function theme.add_gehenna_buildings(e)

    local depth_fraction = you.depth_fraction()
    local border = 4
    local gxm, gym = dgn.max_bounds()
    local right  = gxm - 1 - border
    local bottom = gym - 1 - border
    local size_max = 6 + 4 * depth_fraction
    local separation_min = crawl.random_range(1, border - depth_fraction*2)

    local room_count_min = (150 + depth_fraction *  850) / size_max
    local room_count_max = (300 + depth_fraction * 1700) / size_max
    for i = 1, crawl.random_range(room_count_min, room_count_max) do
        local size = crawl.random_range(4, size_max)
        local min_x = crawl.random_range(border, right  - size)
        local min_y = crawl.random_range(border, bottom - size)
        local max_x = min_x + size
        local max_y = min_y + size

        local cell_count = size * size
        local wall_count = e.count_feature_in_box {x1 = min_x, x2 = max_x,
                                                   y1 = min_y, y2 = max_y,
                                                   feat = "xv" }
        local blocked = e.find_in_area {x1 = min_x - separation_min,
                                        x2 = max_x + separation_min,
                                        y1 = min_y - separation_min,
                                        y2 = max_y + separation_min,
                                        find = "c", find_vault = true }
        if not blocked and wall_count < cell_count
           and wall_count >= cell_count * 0.5 then
            local door_count = crawl.random_range(1, 3)
            e.make_round_box {x1 = min_x, x2 = max_x,
                              y1 = min_y, y2 = max_y,
                              floor = '.', wall = 'c',
                              door_count = door_count, veto_gates=true,
                              veto_if_no_doors=true }
        end
    end
end
