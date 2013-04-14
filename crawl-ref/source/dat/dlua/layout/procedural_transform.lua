------------------------------------------------------------------------------
-- procedural_transform.lua: Complex transforms
--
------------------------------------------------------------------------------

require("dlua/layout/procedural.lua")

procedural.transform = {}

function procedural.transform.wrapped_cylinder(source,period,radius,zscale,zoff)

  if zscale == nil then zscale = 1 end
  if zoff == nil then zoff = 0 end

  return function(x,y)

    -- Convert x to rads
    local a = x * 2 * math.pi / period
    -- Extrude a circle along the y axis of the original function
    return source(math.sin(a) * radius, y, math.cos(a) * radius * zscale + x * zoff)

  end

end

-- Transforms a cartesian domain into a polar one. In other words, it takes
-- a rectangular shape and transforms it into a circular one, where y becomes
-- the radius, and x becomes the angle about the origin.
function procedural.transform.polar(source,radius,sx,sy)
  local frad = procedural.radial{}
  if sx == nil then sx = 1 end
  if sy == nil then sy = 1 end

  return function(x,y)

    local a = frad(x,y)
    local r = math.sqrt(x^2+y^2)/radius
    return source(a*sx,r*sy)

  end

end

-- Inverse of the polar transform
function procedural.transform.polar_inverse(source,radius,sx,sy)
  local frad = procedural.radial{}
  if sx == nil then sx = 1 end
  if sy == nil then sy = 1 end

  return function(x,y)

    local a = x / sx
    local r = radius * y / sy

    -- TODO: Check that this is properly the inverse of polar; it might have
    -- been flipped or rotated; although this doesn't matter slightly for current layouts
    return source(math.sin(a*2*math.pi)*r, math.cos(a*2*math.pi)*r)
  end

end
