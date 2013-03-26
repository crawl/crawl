------------------------------------------------------------------------------
-- zonify.lua: Dungeon zoning
--
--
------------------------------------------------------------------------------

require("dlua/layout/vector.lua")

zonify = {}

function zonify.map(bounds,fcell,fgroup)
  local x1,x2,y1,y2 = bounds.x1,bounds.x2,bounds.y1,bounds.y2
  local zonemap = {}
  local zonegrid = {}
  local function fgrid(x,y,val)
    if zonegrid[y] == nil then zonegrid[y] = {} end
    if val ~= nil then
      zonegrid[y][x] = val
    end
    return zonegrid[y][x]
  end
  for y = y1,y2,1 do
    for x = x1,x2,1 do
      local cell = fcell(x,y)
      if cell ~= nil then
        local grid = fgrid(x,y)
        if grid == nil then
          local group = fgroup(cell)
          if group ~= nil then
            if zonemap[group] == nil then zonemap[group] = {} end
            -- Recursively walk and populate the zone
            local zone = { group = group, borders = {}, cells = {} }
            zonify.walk({x=x,y=y},cell,zone,fgrid,fcell,fgroup)
            table.insert(zonemap[group],zone)
          end
        end
      end
    end
  end
  return zonemap
end

-- Recursively walks cells (like a flood fill) to discover all
-- cells in the defined zone
function zonify.walk(pos,cell,zone,fgrid,fcell,fgroup)
  table.insert(zone.cells, { pos = pos, cell = cell })
  fgrid(pos.x,pos.y,zone)
  for i,dir in ipairs(vector.directions) do
    local x,y = pos.x+dir.x,pos.y+dir.y
    ncell = fcell(x,y)
    if ncell ~= nil then
      local grid = fgrid(x,y)
      if grid == nil then
        local group = fgroup(ncell)
        if group ~= nil then
          if group == zone.group then
            zonify.walk({x=x,y=y},ncell,zone,fgrid,fcell,fgroup)
          else
            -- TODO: Could recurse further at this point and explore the specified gruop
            table.insert(zone.borders,{ pos = {x=x,y=y}, anchor = { pos = pos, cell = cell }, cell = cell, group = group } )
          end
        end
      end
    end
  end
end

-- Fills all the zones except the largest num_to_keep
function zonify.fill_smallest_zones(zonemap,num_to_keep,fgroup,ffill)

  if num_to_keep == nil then num_to_keep = 1 end

  local largest = nil
  local count = nil
  for name,group in pairs(zonemap) do
    if type(fgroup) == "function" and fgroup(group) or name==fgroup then
      for i,zone in ipairs(group) do
        zcount = #(zone.cells)
        if count == nil or zcount > count then
          count = zcount
          largest = zone
        end
      end
    end
  end

  if largest == nil then return false end

  for name,group in pairs(zonemap) do
    if type(fgroup) == "function" and fgroup(group) or name==fgroup then
      for i,zone in ipairs(group) do
        if zone ~= largest then
          for c,cell in ipairs(zone.cells) do
            ffill(cell.pos.x,cell.pos.y,cell)
          end
        end
      end
    end
  end

end

-- Zonifies the current map based on solidity
function zonify.map_map(e)

  -- TODO: Are there mapgrid function to check solidity after SUBST etc.?
  local floor = ".W+"
  local wall = "wlxcv"

  -- TODO: Can we check size of current map after extend_map?
  local gxm,gym = dgn.max_bounds()
  return zonify.map(
    { x1 = 0, y1 = 0, x2 = gxm-1, y2 = gym-1 },
    function(x,y)
      return dgn.in_bounds(x,y) and e.mapgrd[x][y] or nil
    end,
    function(val)
      return string.find(wall,val,1,true) and "wall" or "floor"
    end
  )

end

function zonify.map_fill_zones(e,num_to_keep,glyph)

  if glyph == nil then glyph = 'x' end

  local zonemap = zonify.map_map(e)
  zonify.fill_smallest_zones(zonemap,1,"floor",function(x,y,cell) e.mapgrd[x][y] = glyph end)

end

-- Zonifies the current dungeon grid
-- TODO: Implement for usage grid also (and as a hyper pass)
function zonify.grid_map(e)
  -- TODO: Allow analysing within arbitrary bounds
  local gxm,gym = dgn.max_bounds()
  return zonify.map(
    { x1 = 0, y1 = 0, x2 = gxm-1, y2 = gym-1 },
    function(x,y)
      return dgn.in_bounds(x,y) and dgn.grid(x,y) or nil
    end,
    function(val)
      return feat.has_solid_floor(val) and "floor" or "wall"
    end
  )
end

function zonify.grid_fill_zones(num_to_keep,feature)
  if glyph == nil then glyph = 'x' end
  local zonemap = zonify.grid_map()
  zonify.fill_smallest_zones(zonemap,1,"floor",function(x,y,cell) dgn.grid(x,y,feature) end)
end
