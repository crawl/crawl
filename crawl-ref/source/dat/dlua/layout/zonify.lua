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
  zonify.walk(x1,y1,zonemap,fgrid,fcell,fgroup)
  return zonemap
end

-- Recursively walks cells (like a flood fill) to discover all
-- cells in the defined zone
function zonify.walk(x,y,zonemap,fgrid,fcell,fgroup,from,zone)
  local grid = fgrid(x,y)
  if grid ~= nil then return end
  local cell = fcell(x,y)
  if cell == nil then return end
  cell.x,cell.y = x, y

  -- This is a currently unexplored cell
  local group = fgroup(cell)
  if group == nil then return end
  local newzone = false

  if zone == nil then
    -- Create a new zone
    zone = { group = group, borders = {}, cells = {} }
    if zonemap[group] == nil then zonemap[group] = {} end
    table.insert(zonemap[group],zone)
    newzone = true
  end

  if group == zone.group then
    table.insert(zone.cells, cell)
    fgrid(x,y,zone)
    -- Now recursively check all directions for more zones
    for i,dir in ipairs(vector.directions) do
      local nx,ny = x + dir.x,y + dir.y
      zonify.walk(nx,ny,zonemap,fgrid,fcell,fgroup,cell,zone)
    end
  else
    table.insert(zone.borders,{ x = x, y = y }) -- Don't need this info (yet): from_cell = from, cell = cell, group = group } )
  end
  if newzone then
    zone.done = true
    -- Recurse with a new zone for each bordering square
    for i,b in ipairs(zone.borders) do
      zonify.walk(b.x,b.y,zonemap,fgrid,fcell,fgroup,nil,nil)
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
            ffill(cell.x,cell.y,cell)
          end
        end
      end
    end
  end

end

-- Zonifies the current map based on solidity
function zonify.map_map(e)

  -- TODO: Are there mapgrid function to check solidity after SUBST etc.?
  --       There is dgn.inspect_map but it requires a vault_placement rather than a map...
  -- local floor = ".W+({[)}]<>_"
  local wall = "wlxcvbtg"

  -- TODO: Can we check size of current map after extend_map?
  local gxm,gym = dgn.max_bounds()
  return zonify.map(
    { x1 = 1, y1 = 1, x2 = gxm-2, y2 = gym-2 },
    function(x,y)
      return dgn.in_bounds(x,y) and { glyph = e.mapgrd[x][y] } or nil
    end,
    function(val)
      return string.find(wall,val.glyph,1,true) and "wall" or "floor"
    end
  )

end

function zonify.map_fill_zones(e, num_to_keep, glyph)
  if glyph == nil then glyph = 'x' end

  local zonemap = zonify.map_map(e)
  zonify.fill_smallest_zones(zonemap,1,"floor",function(x,y,cell) e.mapgrd[x][y] = glyph end)
end

function zonify.map_fill_lava_zones(e, num_to_keep, glyph)
  if glyph == nil then glyph = 'x' end

  local wall = "wxcvbtg"

  -- TODO: Can we check size of current map after extend_map?
  local gxm,gym = dgn.max_bounds()
  local zonemap = zonify.map(
    { x1 = 1, y1 = 1, x2 = gxm-2, y2 = gym-2 },
    function(x,y)
      return dgn.in_bounds(x,y) and { glyph = e.mapgrd[x][y] } or nil
    end,
    function(val)
      return string.find(wall,val.glyph,1,true) and "wall" or "floor"
    end
  )
  zonify.fill_smallest_zones(zonemap,1,"floor",function(x,y,cell) e.mapgrd[x][y] = glyph end)
end

-- Zonifies the current dungeon grid
-- TODO: Implement for usage grid also (and as a hyper pass)
function zonify.grid_map(e)
  -- TODO: Allow analysing within arbitrary bounds
  local gxm,gym = dgn.max_bounds()
  return zonify.map(
    { x1 = 1, y1 = 1, x2 = gxm-2, y2 = gym-2 },
    zonify.grid_analyse_coord,zonify.grid_group_feature)
end

function zonify.grid_analyse_coord(x,y)
  -- Out of bounds
  if not dgn.in_bounds(x,y) then return nil end
  return {
    feat = dgn.feature_name(dgn.grid(x,y)),
    vault = dgn.in_vault(x,y),
    passable = dgn.is_passable(x,y)
  }
end

function zonify.grid_group_feature(val)
  if val.vault then return "vault" end
  return val.passable and "floor" or "wall"
end

function zonify.grid_fill_zones(num_to_keep,feature)
  if feature == nil then feature = 'rock_wall' end
  local zonemap = zonify.grid_map()
  zonify.fill_smallest_zones(zonemap,1,"floor",function(x,y,cell) dgn.grid(x,y,feature) end)
end

-- This is a little brutish but for some layouts (e.g. swamp) a different pass
-- is needed to fill in deep water zones and stop flyers/swimmers getting trapped.
-- TODO: Need to simplify into a single pass that understands connectivity issues
-- between different zone groups and can handle everything more elegantly.
function zonify.grid_fill_water_zones(num_to_keep,feature)
  if feature == nil then feature = 'rock_wall' end
  local gxm,gym = dgn.max_bounds()
  local zonemap = zonify.map(
    { x1 = 1, y1 = 1, x2 = gxm-2, y2 = gym-2 },
    zonify.grid_analyse_coord,
    -- This grouping function treats deep water as floor so it'll get filled
    -- in when surrounded by wall
    function(val)
      if val.vault then return "vault" end
      return (val.passable or val.feat == "deep_water") and "floor" or "wall"
    end)
  zonify.fill_smallest_zones(zonemap,1,"floor",function(x,y,cell) dgn.grid(x,y,feature) end)
end
