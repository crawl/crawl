------------------------------------------------------------------------------
-- procedural_complex.lua: Complex procedural functions
--
-- These use base primitives to build up more complex shapes
------------------------------------------------------------------------------

require("dlua/layout/procedural.lua")

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
