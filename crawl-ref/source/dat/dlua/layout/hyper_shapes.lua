------------------------------------------------------------------------------
-- hyper_shapes.lua:
--
-- Creates shapes for layout paint arrays
--
-- TODO: Largely this file is redundant in favour of better placement
-- strategies. The interesting things here are size functions; cave rooms
-- (which should either go into a caves include or just layout_caves.des);
-- and a few misc functions and of course floor vault.

crawl_require("dlua/layout/procedural.lua")

hyper.shapes = {}
hyper.floors = {}

---------------------------------------------------------------------
-- Size callbacks
--
-- To be used in room generators to produce different types of sizings

-- TODO: There are two important factors - the distribution (i.e. do we tend towards
--       average, the lower end, or an even distribution between the min/max?) -
--       and the type of shape produced - square, any shape, narrow, near-square, etc.
--       Could think about separating these two factors for easier control
--       without having to repeat so much of the same logic each time.

-- Crudely produces a size tending towards the lower end of the min/max settings
function hyper.rooms.size_default(generator,options)
  -- Use min sizes from available config
  local min_size_x, max_size_x, min_size_y, max_size_y = options.min_room_size,options.max_room_size,options.min_room_size,options.max_room_size
  if generator.min_size ~= nil then min_size_x,min_size_y = generator.min_size,generator.min_size end
  if generator.max_size ~= nil then max_size_x,max_size_y = generator.max_size,generator.max_size end
  if generator.min_size_x ~= nil then min_size_x = generator.min_size_x end
  if generator.max_size_x ~= nil then max_size_x = generator.max_size_x end
  if generator.min_size_y ~= nil then min_size_y = generator.min_size_y end
  if generator.max_size_y ~= nil then max_size_y = generator.max_size_y end

  local diffx,diffy = max_size_x - min_size_x + 1, max_size_y - min_size_y + 1
  return { x = min_size_x + crawl.random2(crawl.random2(diffx)), y = min_size_y + crawl.random2(crawl.random2(diffy)) }
end

-- Produces only square sizes. Even distribution.
function hyper.rooms.size_square(chosen,options)
  local min_size, max_size = options.min_room_size,options.max_room_size
  if chosen.min_size ~= nil then min_size = chosen.min_size end
  if chosen.max_size ~= nil then max_size = chosen.max_size end

  local size = crawl.random_range(min_size,max_size)
  return { x = size, y = size }
end

function hyper.rooms.size_square_lower(chosen,options)
  -- Use the default distribution
  local size = hyper.rooms.size_default(chosen,options)
  -- Just take x sizes
  return { x = size.x, y = size.x }
end

---------------------------------------------------------------------
-- Util functions

-- Merge a set of properties onto every item in a paint array
function hyper.shapes.merge_props(paint, props)
  if props == nil then return paint end
  for i,p in ipairs(paint) do
    for k,prop in pairs(props) do
      p[k] = prop
    end
  end
end

---------------------------------------------------------------------
-- Draw functions
--
-- These take input parameters and return paint arrays (which should
-- be combined with util.append when using multiple draw functions).
-- These will be used by other room functions.
--
-- TODO: For any of these we should support an additional parameter
--       for specifying arbitrary properties of the returned paint, *or*
--       some sort of filter util which does the same on a whole batch afterward.
function hyper.shapes.draw_box(corner1, corner2)
  return {
    { type = "floor", corner1 = corner1, corner2 = corner2, open = true },
  }
end

function hyper.rooms.junction_vault(room,options,gen)

  local floor = rooms.primitive.floor(room,options,gen)

  local max_corridor = math.min(gen.max_corridor, math.floor(math.min(room.size.x,room.size.y)/2))
  local corridor_width = crawl.random_range(gen.min_corridor,max_corridor)

  -- Use cunning grid wrapping to generate all kinds of corners and intersections
  local corner1 = { x = crawl.random_range(corridor_width,room.size.x), y = crawl.random_range(corridor_width,room.size.y) }
  table.insert(floor, { type = "space", corner1 = corner1, corner2 = { x = corner1.x + room.size.x - 1 - corridor_width, y = corner1.y + room.size.y - 1 - corridor_width }, wrap = true })
  return floor
end
