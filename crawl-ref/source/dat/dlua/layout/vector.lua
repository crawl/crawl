------------------------------------------------------------------------------
-- vector.lua:
--
-- Functions relating to vector math
------------------------------------------------------------------------------

vector = {}

-- The four directions and their associated vector normal and name.
-- This helps us manage orientation of rooms.
vector.normals = {
  { x = 0, y = -1, dir = 0, name="n" },
  { x = -1, y = 0, dir = 1, name="w" },
  { x = 0, y = 1, dir = 2, name="s" },
  { x = 1, y = 0, dir = 3, name="e" }
}

-- Sometimes it's useful to have a list of diagonal vectors also
vector.diagonals = {
  { x = -1, y = -1, dir = 0.5, name="nw" },
  { x = -1, y = 1, dir = 1.5, name="sw" },
  { x = 1, y = 1, dir = 2.5, name="se" },
  { x = 1, y = -1, dir = 3.5, name="ne" }
}

-- All 8 directions (although not in logical order)
vector.directions = util.append({},vector.normals,vector.diagonals)

-- Moves from a start point along a movement vector mapped to actual vectors xVector/yVector
-- This allows us to deal with coords independently of how they are rotated to the dungeon grid
function vector.mapped_vector_add(start, move, xVector, yVector)

  return {
    x = start.x + (move.x * xVector.x) + (move.y * yVector.x),
    y = start.y + (move.x * xVector.y) + (move.y * yVector.y)
  }

end

-- Rotates a vector anticlockwise by 90 degree increments * count
-- Highly rudimentary, not actually used for anything, keeping around for now in case.
function vector.rotate(vec, count)
  if count > 0 then
    local rotated = { x = -vec.y, y = vec.x }
    count = count - 1
    if count <= 0 then return rotated end
    return vector_rotate(rotated,count)
  end
  if count < 0 then
    local rotated = { x = vec.y, y = -vec.x }
    count = count + 1
    if count >= 0 then return rotated end
    return vector_rotate(rotated,count)
  end
  return vec
end
