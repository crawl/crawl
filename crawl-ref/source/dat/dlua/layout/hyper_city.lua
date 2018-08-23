------------------------------------------------------------------------------
-- hyper_city.lua:
--
-- City layouts get their own fle now. There are two main variants:
--  * Hyper City. Builds an initial space - could be an open level, could
--    have some geometry, maybe a street lyout, could even be an existing layout.
--    Random buildings are then placed wherever they'll fit in the space or are
--    carved into walls or joined onto each other.
--  * Grid City. Creates a zones grid (by one of two methods of subdivision)
--    and then places buildings specifically within those zones.
------------------------------------------------------------------------------

crawl_require("dlua/v_main.lua")

hyper.city = {}
hyper.city.default_strategy = hyper.place.strategy_inner_or_carve

function hyper.city.room_types(pass,options)

  return {
    { weight = 10, paint_callback = floor_vault, room_transform = hyper.rooms.add_walls,size_padding = 3,min_size = 3 },
    -- Filler to be used with very tiny rooms and occasionally with bigger areas
    { weight = 1, paint_callback = wall_vault, room_transform = hyper.rooms.add_walls,size_padding = 1 },
    { weight = 10, paint_callback = wall_vault, room_transform = hyper.rooms.add_walls,size_padding = 1 },
    { weight = 1, paint_callback = wall_vault, room_transform = hyper.rooms.add_walls,size_padding = 1 },
    { weight = 1, paint_callback = wall_vault, room_transform = hyper.rooms.add_walls,size_padding = 1 },
    { weight = 1, paint_callback = wall_vault, room_transform = hyper.rooms.add_walls,size_padding = 1 },

  }

end

function hyper.city.grid_rooms(room,options,gen)

  local grid = gen.grid_callback(room,options,gen)

  local paint = {
    { type = "floor", corner1 = { x = 0, y = 0 }, corner2 = { x = room.size.x-1, y = room.size.y-1 } }
  }
  local passes = {}
  for i,zone in ipairs(grid.zones) do
    table.insert(passes,{ pass = "Zone"..i, strategy = hyper.place.strategy_primary, max_rooms = 1, bounds = zone.bounds, generators = gen.room_generators })
  end
  table.insert(paint,{ type = "build", passes = passes })
end


-- Creates a house by subdividing the rooms and then connecting them up
function hyper.city.house_vault(room,options,gen)
  local params = hyper.merge_options(hyper.layout.omnigrid_defaults(),{
    jitter = false,
    minimum_size = 3,
    subdivide_level_multiplier = 0.90,
    guaranteed_divides = 3
  })

  local zonemap = hyper.layout.omnigrid_subdivide(0,0,room.size.x-1,room.size.y-1,params)

  -- Fill with wall to start with
  local paint = {
    { type = "wall", corner1 = { x = 0, y = 0 }, corner2 = { x = room.size.x-1, y = room.size.y-1 } }
  }

  -- Paint the zones with floor
  util.append(paint,hyper.layout.zonemap_paint(zonemap,{
    feature = "floor",
    outer_padding = false,
    corridor_width = 1
  }))

  -- TODO: Genericise the following process more. We should be able to supply custom decorators
  -- to draw the connection (and this would allow for e.g. spotty connector for zones that don't
  -- already overlap).
  -- In fact, this could entirely replace the existing way that carvable walls are decorated.

  -- Run two passes of "connect" - the first joins zones together with floor to create bigger room shapes
  local plots = util.append(hyper.layout.zonemap_connect(zonemap,{
    min_zones = 4,
    connect_borders = function(borders)
                        return function(wall,border)
                          -- TODO: Can make a bit of internal architecture by patterning the connections
                          return "floor"
                        end
                      end
  }),
  -- The second connects all the remaining zones together with doors until we only have 1 zone
  hyper.layout.zonemap_connect(zonemap,{
    max_zones = 1,
    connect_borders = function(borders)
                        -- How many doors to make? (Usually 1)
                        local numconnects = 1
                        if crawl.coinflip() then
                          numconnects = numconnects + crawl.random2(3)
                        end
                        local bcons = {}
                        for n=1,numconnects,1 do
                          local id = crawl.random2(#borders)+1
                          if bcons[id] == nil then bcons[id] = {} end
                          local wallnum = crawl.random2(#borders.walls)+1
                          bcons[id][wallnum] = true
                        end
                        return function(wall,border)
                          if bcons[border.id] ~= nil and bcons[border.id][wall.id] == true then
                            return "closed_door"
                          else
                            return nil
                          end
                        end
                      end
  }))
  table.insert(paint, { type = "plot", coords = plots })
  room.zonemap = zonemap -- TODO: Probably isn't useful to us in this state
  return paint
end

function hyper.layout.zonemap_connect(zonemap,params)
  -- TODO: zone connection needs to be in a separate function
  local bail = false
  local function mergezones(a,b)
    -- Always take the lower id, it'll simplify table manipulation
    if b.id<a.id then a,b=b,a end
    local plots = {}
    -- Update foreign borders
    local connected = {}
    for i,bor in ipairs(b.borders) do
      if bor.b == a then
        -- Lose borders with the one we're merging to (but remember them for paint func)
        table.insert(connected,bor)
        util.remove(a.borders,bor.inverse) -- TODO: slow
      else
        -- Update other borders to point to us
        bor.a = a
        bor.inverse.b = a
        -- TODO: It's possible that the border needs to merge with an existing one if they are congruent lines
        table.insert(a.borders,bor)
      end
    end
    local fdeco = params.connect_borders(connected)
    -- TODO: Passing in the callback probably obfuscates things here. Should just return a list of connected
    -- internals with the result then handle them any way we wanted? Or are there other uses for having it here?
    for i,bor in ipairs(connected) do
      for w,wall in ipairs(bor.walls) do
        local feat = fdeco(wall,bor)
        table.insert(plots, { feature = feat, x = wall.x, y = wall.y })
      end
    end

    util.append(a.connects,b.connects)
    util.append(a.borders,b.borders)
    a.size = a.size + b.size
    -- Remove the old zone
    table.remove(zonemap.zones,b.id)
    -- Adjust ids of zones to the right
    for n = b.id, #(zonemap.zones), 1 do
      zonemap.zones[n].id = n
    end
    -- Adjust count
    zonemap.count = zonemap.count - 1
    return plots
  end
  -- Weight function for picking the next zone to join up
  -- TODO: Not really sure how these
  local function fweightpick(zone)
    return math.ceil(100 / zone.size)
  end
  -- Weight function for picking which zone to connect it to
  local function fweightconnect(border)
    return math.ceil(100 / zone.to.size)
  end
  local plots = {}
  while (zonemap.count > 1 and not bail) do
    -- Pick which zone to link up
    local picked = util.random_weighted_from(fweightpick,zonemap.zones)
    local connect = util.random_weighted_from(fweightconnect,picked.borders)

    util.append(plots,mergezones(picked,connect))

    if zonemap.count == params.min_zones then
      bail = true
    elseif zonemap.count <= params.max_zones
      -- TODO: Not sure of a good way to handle this proability. Could always just keep going until min_zones is met and let
      -- the caller randomise this. Or could get increasingly likely the closer we are to min_zones.
      and crawl.random2(101) < 10 then
    end
  end

  return plots
end
