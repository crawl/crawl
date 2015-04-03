------------------------------------------------------------------------------
-- rooms_primitive.lua: Primitive room generators
--
-- These generate code rooms and are used by "code" generators in
-- the options tables.

rooms = rooms or {}
rooms.primitive = {}

-- A square room made of floor
function rooms.primitive.floor(room,options)
  return {
    { type = "floor", corner1 = { x = 0, y = 0 }, corner2 = { x = room.size.x - 1, y = room.size.y - 1 } },
  }
end

-- A square room made of wall
function rooms.primitive.wall(room,options)
  return {
    { type = "wall", corner1 = { x = 0, y = 0 }, corner2 = { x = room.size.x - 1, y = room.size.y - 1 } },
  }
end

-- A circular room of floor
function rooms.primitive.ellipse(room,options,gen)
  return {
    { type = "floor", corner1 = { x = 0, y = 0}, corner2 = { x = room.size.x - 1, y = room.size.y - 1 }, shape = "ellipse" },
  }
end

function rooms.primitive.procedural(room,options,gen)
  return {
    { corner1 = { x = 0, y = 0}, corner2 = { x = room.size.x - 1, y = room.size.y - 1 }, shape = "proc",  },
  }
end
