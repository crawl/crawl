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

function procedural.transform.inverse_polar(source,radius)
  local frad = procedural.radial{}

  return function(x,y)

    local a = frad(x,y)
    local r = math.sqrt(x^2+y^2)/radius
    return source(a,r)

  end

end
