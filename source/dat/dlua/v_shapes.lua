------------------------------------------------------------------------------
-- v_shapes.lua:
--
-- Creates shapes for layout paint arrays

hypervaults.shapes = {}
hypervaults.floors = {}

function floor_vault_paint_callback(room,options)
  return {
    { type = "floor", corner1 = { x = 0, y = 0 }, corner2 = { x = room.size.x - 1, y = room.size.y - 1 }, empty = true },
  }
end

-- This version of floor vault adds a staircase somewhere randomly in the middle
-- In other layouts we don't want to mess with the stairs but in V this will ensure the stairs are in a room, not the corridors
function hypervaults.shapes.floor_vault(room, options, gen)

  local paint = floor_vault_paint_callback(room,options,gen)
  if options.stair_chance ~= nil and crawl.x_chance_in_y(options.stair_chance,100) then

    -- Place the stair
    table.insert(paint, { shape = "plot", feature = "stone_stairs_down_i",
      x = crawl.random2(math.floor(room.size.x/2)) + crawl.div_rand_round(room.size.x,4),
      y = crawl.random2(math.floor(room.size.y/2)) + crawl.div_rand_round(room.size.y,4),
    })

  end
  return paint

end

function junction_vault_paint_callback(room,options,gen)

  local floor = floor_vault_paint_callback(room,options,gen)

  local max_corridor = math.min(gen.max_corridor, math.floor(math.min(room.size.x,room.size.y)/2))
  local corridor_width = crawl.random_range(gen.min_corridor,max_corridor)

  -- Use cunning grid wrapping to generate all kinds of corners and intersections
  local corner1 = { x = crawl.random_range(corridor_width,room.size.x), y = crawl.random_range(corridor_width,room.size.y) }
  table.insert(floor, { type = "space", corner1 = corner1, corner2 = { x = corner1.x + room.size.x - 1 - corridor_width, y = corner1.y + room.size.y - 1 - corridor_width }, wrap = true })
  return floor
end

function bubble_vault_paint_callback(room,options,gen)
  return {
    { type = "floor", corner1 = { x = 0, y = 0 }, corner2 = { x = room.size.x - 1, y = room.size.y - 1 }, shape = "ellipse" },
  }
end

function hypervaults.floors.full_size_room()
  local gxm, gym = dgn.max_bounds()

  local paint = {
    { type = "floor", corner1 = { x = 1, y = 1 }, corner2 = { x = gxm-2, y = gym-2 } }
  }
  return paint
end

function hypervaults.floors.three_quarters_size_room()
  local gxm, gym = dgn.max_bounds()

  local paint = {
    { type = "floor", corner1 = { x = 11, y = 11 }, corner2 = { x = gxm-12, y = gym-12 } }
  }
  return paint
end

function hypervaults.floors.small_central_room()
  local gxm, gym = dgn.max_bounds()
  -- Put a single empty room somewhere roughly central. All rooms will be built off from each other following this
  local x1 = crawl.random_range(30, gxm-70)
  local y1 = crawl.random_range(30, gym-70)

  local paint = {
    { type = "floor", corner1 = { x = x1, y = y1 }, corner2 = { x = x1 + crawl.random_range(4,6), y = y1 + crawl.random_range(4,6) } }
  }
  return paint
end

function hypervaults.floors.small_edge_room()
  local gxm, gym = dgn.max_bounds()
  local normal = hypervaults.normals[crawl.random2(4)+1]
  local size = { x = crawl.random_range(4,8), y = crawl.random_range(4,8) }

  local x1 = math.floor((normal.x + 1)*(gxm - 10 - size.x)/2 + 5)
  local y1 = math.floor((normal.y + 1)*(gym - 10 - size.y)/2 + 5)

  local paint = {
    { type = "floor", corner1 = { x = x1, y = y1 }, corner2 = { x = x1 + size.x - 1, y = y1 + size.y - 1 } }
  }
  return paint
end
