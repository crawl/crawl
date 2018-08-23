------------------------------------------------------------------------------
-- procedural_complex.lua: Complex procedural functions
--
-- These use base primitives to build up more complex shapes
------------------------------------------------------------------------------

crawl_require("dlua/layout/procedural.lua")

complex = {}

-- Creates a cog shape in the -1,-1 -> 1,1 domain
function complex.cog(teeth,inner,gap,offset)
  local fr = procedural.phase(primitive.radial, teeth, offset)
  return function(x,y)
    local dp = primitive.distance(x,y)
    local d = dp/inner
    local r = fr(x,y)
    if d < 1 or dp > 1 then return d end
    if r < gap then return 0.99 end
    return d
  end
end

-- Creates a number of rays around the origin. Offset is 0..1 to denote
-- rotation. If converge is non-nil then the rays will be even-sized
-- all the way along instead of diverging in size; they will converge
-- at the radius given by converge.
function complex.rays(num,offset,converge,diverge)
  local frad = procedural.phase(primitive.radial, num, offset)
  if even == false then return frad end
  if diverge == nil then diverge = 0 end
  return function(x,y)
    local a = frad(x,y)
    local r = primitive.distance(x,y)
    -- TODO: Work out divergence. It means (r/converge) has to tend
    -- more towards 1 for higher values of r.
    return a * r / converge
  end
end
