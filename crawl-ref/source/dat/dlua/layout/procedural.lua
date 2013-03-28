------------------------------------------------------------------------------
-- procedural.lua: Procedural layout functions
--
-- These generators accept a params object which configures the final
-- function produced. The resultant function will take an x,y coord and
-- return a result typically in the range 0..1. These functions should
-- be considered primitives that can be combined together other functions
-- to produce more complex patterns which can serve as the blueprints
-- for actual layouts.
------------------------------------------------------------------------------

procedural = {}

------------------------------------------------------------------------------
-- Aggregate functions
--
-- These factories return a function that aggregates several functions or
-- values together with a mathematical operation. Each one takes a parameter
-- list which can consist of any mixture of functions and numerical values.
-- The returned function performs the aggregate of those functions and values
-- at each x,y coordinate.

-- Add the output of several functions together
function procedural.add(...)
  local vars = { ... }
  return procedural.aggregate(vars,function(t,v) return t+v end)
end

-- Takes the value of the first function,
-- subtracts the values of the rest from it
function procedural.sub(...)
  local vars = { ... }
  return procedural.aggregate(vars,function(t,v) return t-v end)
end

-- Multiplies together several functions or values
function procedural.mul(...)
  local vars = { ... }
  return procedural.aggregate(vars,function(t,v) return t*v end)
end

function procedural.div(...)
  local vars = { ... }
  return procedural.aggregate(vars,function(t,v) return t/v end)
end

-- Creates a function that returns the minimum value of several functions
function procedural.min(...)
  local vars = { ... }
  return procedural.aggregate(vars,function(t,v) return math.min(t,v) end)
end

-- Creates a function that returns the maximum value of several functions
function procedural.max(...)
  local vars = { ... }
  return procedural.aggregate(vars,function(t,v) return math.max(t,v) end)
end

function procedural.aggregate(list,op)

  return function(x,y)
    local val = 0
    local r
    for i,f in ipairs(list) do
      if type(f) == "function" then
        r = f(x,y)
      else
        r = f
      end
      if i==1 then
        val = r
      else
        val = op(val,r)
      end
    end
    return val
  end

end

-- Some non-aggregate function transforms
function procedural.abs(func)
  return function(x,y)
    return math.abs(func(x,y))
  end
end

-- A function that returns 1 around the border edge, and fades to 0 over
-- a specified number of padding squares.
function procedural.border(params)
  local x1,y1 = 0,0
  local x2,y2 = dgn.max_bounds()
  x2 = x2 - 1
  y2 = y2 - 1
  if params.x1 ~= nil then x1 = params.x1 end
  if params.x2 ~= nil then x2 = params.x2 end
  if params.y1 ~= nil then y1 = params.y1 end
  if params.y2 ~= nil then y2 = params.y2 end
  local padding = params.padding == nil and 10 or params.padding
  if params.additive then
    return function(x,y)
      local nearx = math.min(x-x1,x2-x)
      local neary = math.min(y-y1,y2-y)
      return math.min(1, 2 - procedural.minmax_map(nearx, 0, padding + 1)
                           - procedural.minmax_map(neary, 0, padding + 1))
    end
  else
    return function(x,y)
      local near = math.min(x - x1, y - y1, x2 - x, y2 - y)
      return 1 - procedural.minmax_map(near, 0, padding + 1)
    end
  end
end

-- Calculates nearest distance to edge of a box. Returns 0 in the
-- box center, 1 at the box edge.
-- TODO: Border is basically this with a numerical map applied,
-- we can get rid of the border implementation and replace it with that
function procedural.box(params)

  local x1,y1 = 0,0
  local x2,y2 = dgn.max_bounds()
  x2 = x2 - 1
  y2 = y2 - 1
  if params.x1 ~= nil then x1 = params.x1 end
  if params.x2 ~= nil then x2 = params.x2 end
  if params.y1 ~= nil then y1 = params.y1 end
  if params.y2 ~= nil then y2 = params.y2 end

  -- TODO: Mathematically speaking this is correct but in reality
  -- we might have to adjust this a little for the grid
  local minhalf = math.min((x2-x1)/2,(y2-y1)/2)

  return function(x,y)
    local near = math.min(x-x1,y-y1,x2-x,y2-y)
    return 1 - near/minhalf
  end

end

-- Returns distance from a point, useful for circles and ellipses
function procedural.distance(params)
  local xo,yo = params.origin.x,params.origin.y
  local radiusx,radiusy = params.radius,params.radius
  if params.radiusx ~= nil then radiusx = params.radiusx end
  if params.radiusy ~= nil then radiusy = params.radiusy end

  return function(x,y)
    return math.sqrt(math.pow((xo - x) / radiusx, 2)
                   + math.pow((yo - y) / radiusy, 2))
  end
end

-- Gives the radial value from a point, i.e. the arctangent of the line
-- from the origin to the point
function procedural.radial(params)

  local xo,yo = params.origin.x,params.origin.y

  return function(x,y)
    local xd,yd = xo-x,yo-y
    local r
    if yd == 0 then r = (xd>0) and 270 or 90
    else
      r = math.atan(xd/yd) / math.pi * 180
      if yd>0 then r = r+180 end
    end
    if params.phase ~= nil then
      r = (r + params.phase*360)
    end
    return (r % 360)/360
  end

end

-- Draws a horizontal or vertical bar at the specified position. The bar has
-- value 1 at the position and tails off to 0 at a distance of width/2 to
-- either side
-- TODO: Implement rotation transforms to eliminate this v/h logic
function procedural.bar(params)

  local horizontal = params.horizontal == nil and true or params.horizontal
  local width = params.width or 1
  local position = params.position or 1
  local positiona,positionb = position,position
  if params.inner ~= nil then
    positiona,positionb = position - params.inner/2, position + params.inner/2
  end
  local start = math.floor(position - width/2)
  local finish = start + width + 1
  if horizontal then
    return function(x,y) return procedural.boundary_map(y,start,positiona,positionb,finish) end
  end
  return function(x,y) return procedural.boundary_map(x,start,positiona,positionb,finish) end

end

-- This is more of a complex than a primitive, should move to some sort of
-- library file if it's going to be used more than once
function procedural.river(params)

  local gxm,gym = dgn.max_bounds()

  local horizontal = params.horizontal == nil and true or params.horizontal
  local width = params.width or 15
  local position = params.position or math.floor(gym/2)
  local turbulence = params.turbulence == nil and 10 or params.turbulence
  local turbulence_scale = params.turbulence_scale or 1

  local fbase = procedural.bar{horizontal = horizontal, width = width, position = position}
  if not turbulence then return fbase end
  return procedural.distort{ source = fbase, scale = turbulence,
    offsetx = (not horizontal) and procedural.simplex3d{ scale = turbulence_scale } or nil,
    offsety = horizontal and procedural.simplex3d{ scale = turbulence_scale } or nil }

end

-- Provides the full data from a Worley call, should not be directly used
-- e.g.  as a transform

function procedural.worley(params)
  local final_scale_x = params.scale * 0.8
  local final_scale_y = params.scale * 0.8
  local major_offset_x = crawl.random2(1000000)
  local major_offset_y = crawl.random2(1000000)
  local major_offset_z = crawl.random2(1000000)
  return function(x,y)
    local d1,d2,id1,id2,pos1x,pos1y,pos1z,pos2x,pos2y,pos2z =
        crawl.worley(x * final_scale_x + major_offset_x,
                     y * final_scale_y + major_offset_y, major_offset_z)
    return {
      d = { d1,d2 },

      -- Transform the ids into a 0..1 range so this gives us a random
      -- number unique to the whole node.

      -- TODO: It'll often be more useful to split the id up into bytes or
      -- even bits so we have more random numbers and flags available, but
      -- for optimisation purposes we only want them if necessary...
      id = { id1/4294967295.0,id2/4294967295.0 },
      pos = { { x = pos1x, y = pos1y, z = pos1z },
              { x = pos2x, y = pos2y, z = pos2z } }
    }
  end
end

-- Returns a function that computes the difference between distance[0] and
-- distance[1] for a given Worley function
function procedural.worley_diff(params)
  local worley = procedural.worley(params)
  return function(x,y)
    local data = worley(x,y)
    return data.d[2]-data.d[1], data.id[1]
  end
end

function procedural.simplex3d(params)
  local final_scale_x = params.scale / 10
  local final_scale_y = params.scale / 10
  local major_offset_x = crawl.random2(1000000)
  local major_offset_y = crawl.random2(1000000)
  local major_offset_z = crawl.random2(1000000)

  return function(x,y)
    local result = crawl.simplex(x * final_scale_x + major_offset_x,
                   y * final_scale_y + major_offset_y, major_offset_z)
    if params.unit == nil or params.unit then result = result / 2 + 0.5 end
    return result
  end
end

function procedural.simplex4d(params)
  local final_scale_x = params.scale / 10
  local final_scale_y = params.scale / 10
  local major_offset_x = crawl.random2(1000000)
  local major_offset_y = crawl.random2(1000000)
  local major_offset_z = crawl.random2(1000000)
  local major_offset_w = crawl.random2(1000000)
  return function(x,y)
    return crawl.simplex(x * final_scale_x + major_offset_x,
                         y * final_scale_y + major_offset_y,
                         major_offset_z, major_offset_w) / 2 + 0.5
  end
end

-- Creates a domain distortion function to offset one function using two others
-- for the x,y offsets
function procedural.distort(params)
  local source = params.source
  local offsetx = params.offsetx or function(x,y) return 0 end
  local offsety = params.offsety or function(x,y) return 0 end
  local scale = params.scale or 1
  return function(x,y)
    return source(x+offsetx(x,y) * scale,y+offsety(x,y) * scale)
  end
end

-- Utility functions. These don't return a function themselves, just a value.

-- Maps a value onto a 0..1..0 scale. Outside the boundaries it is 0, and
-- when between b2 and b3 it is 1.  Between b1 and b2, and b3 and b4, it
-- smoothly scales between 1 and 0.
function procedural.boundary_map(val, b1,b2,b3,b4)
  if val<=b1 or val>=b4 then return 0
  elseif b2<=val and val<=b3 then return 1
  elseif val < b2 then return (val-b1)/(b2-b1)
  else return (b4-val)/(b4-b3) end
end

-- Similar but maps just onto 0..1 on min/max boundaries.
function procedural.minmax_map(val,min,max)
  if val<min then return 0
  elseif val>max then return 1
  else return (val-min)/(max-min) end
end

-- Render functions
function procedural.render_map(e, fval, fresult)

  local gxm,gym = dgn.max_bounds()
  e.extend_map { width = gxm, height = gym, fill = 'x' }
  for x = 1,gxm-2,1 do
    for y = 1,gym-2,1 do
      local val = fval(x,y)
      local r = fresult(val)
      if r ~= nil then e.mapgrd[x][y] = r end
    end
  end

end
