------------------------------------------------------------------------------
-- procedural_primitive.lua: Primitive procedural functions
--
-- These procedurals can be used as a base to build up more complex objects
------------------------------------------------------------------------------

-- crawl_require("dlua/layout/procedural.lua")

primitive = {}

function primitive.linear_x(x,y)
  return x
end

function primitive.linear_y(x,y)
  return y
end

function primitive.linear_xy(x,y)
  return x+y
end

function primitive.linear_yx(x,y)
  return x-y
end

function primitive.box(x,y)
  return math.max(math.abs(x),math.abs(y))
end

-- Returns distance from the origin, useful for circles and ellipses
function primitive.distance(x,y)
  return math.sqrt(math.pow(x, 2) + math.pow(y, 2))
end

-- Gives the radial value from a point, i.e. the arctangent of the line
-- from the origin to the point
function primitive.radial(x,y)
  local r
  if y == 0 then r = (x>0) and 270 or 90
  else
    r = math.atan(x/y) / math.pi * 180
    if y>0 then r = r+180 end
  end
  return (r % 360)/360
end

------------------------------------------------------------------------------
-- Slightly more complex functions
--
-- But still primitive enough to count

function primitive.cross()
  return procedural.min(procedural.abs(primitive.linear_x),procedural.abs(primitive.linear_y))
end

function primitive.ring(radius)
  return procedural.abs(procedural.sub(primitive.distance,radius))
end

function primitive.ringify(func,radius)
  return procedural.abs(procedural.sub(func,radius))
end

-- A large X-shape
function primitive.ex()
  return procedural.min(procedural.abs(primitive.linear_xy), procedural.abs(primitive.linear_yx))
end

function primitive.diamond()
  return procedural.max(procedural.abs(primitive.linear_xy), procedural.abs(primitive.linear_yx))
end

function primitive.octagon(edge)
  return procedural.max(primitive.box,procedural.sub(primitive.diamond(),edge))
end

function primitive.hexagon(edge)
  -- Use a domain transform to warp an octagon into a hexagon
  -- There must be a less hackish way to do this but I can't think of it today...
  local foct = primitive.octagon(edge)
  return function(x,y)
    return foct(x,math.abs(y)+edge)
  end
end

function primitive.triangle()
  -- This isn't a very good primitive because it's a hacked-together triangle.
  -- Should just come up with a proper algorithm at some point.
  local tri = procedural.max(primitive.linear_y,procedural.neg(primitive.linear_xy),primitive.linear_yx)
  if crawl.coinflip() then return procedural.flip_y(tri) end
  return tri
end

function primitive.spiral(phase)

end
