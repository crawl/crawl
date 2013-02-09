------------------------------------------------------------------------------
-- v_shapes.lua:
--
-- Creates shapes for layout paint arrays

hypervaults.shapes = {}
hypervaults.floors = {}

function floor_vault_paint_callback(room,options)
  return {
    { type = "floor", corner1 = { x = 0, y = 0 }, corner2 = { x = room.size.x - 1, y = room.size.y - 1 }},
  }
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

function n3v_paint_small_central_room()
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

  local x1 = (normal.x + 1)*(gxm-4-size.x)/2 + 2
  local y1 = (normal.y + 1)*(gxm-4-size.y)/2 + 2

  local paint = {
    { type = "floor", corner1 = { x = x1, y = y1 }, corner2 = { x = x1 + size.x, y = y1 + size.y } }
  }
  return paint
end

function hypervaults.shapes.sword(room,options,gen)
  local hilt_pos = math.floor(room.size.y * 3/4)
  local hilt_width = 3
  local hilt_width_half = math.floor(hilt_width/2)
  local blade_width = 5
  local blade_width_half = math.floor(blade_width/2)
  local center = math.floor(room.size.x / 2 + 0.5) - 1
  local paint = {
    -- Blade
    { type = "floor", corner1 = { x = center - blade_width_half, y = 0}, corner2 = { x = center + blade_width_half, y = hilt_pos - hilt_width_half - 1}, shape = "trapese" },
    -- Hilt
    { type = "floor", corner1 = { x = 0, y = hilt_pos - hilt_width_half }, corner2 = { x = room.size.x - 1, y = hilt_pos + hilt_width_half } },
    { type = "floor", corner1 = { x = center - hilt_width_half, y = hilt_pos + hilt_width_half + 1}, corner2 = { x = center + hilt_width_half, y = room.size.y + 1 }, width1 = 1, width2 = 0, shape = "trapese" }
  }
  return paint
end