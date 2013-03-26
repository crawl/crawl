------------------------------------------------------------------------------
-- omnigrid.lua: Create flexible layout grids by subdivision
--
------------------------------------------------------------------------------

hyper.layout = {}

function hyper.layout.omnigrid_defaults()
  local gxm,gym = dgn.max_bounds()
  local options = {
    subdivide_initial_chance = 100, -- % chance of subdividing at first level, if < 100 then we might just get chaotic city
    subdivide_level_multiplier = 0.80,   -- Multiply subdivide chance by this amount with each level
    minimum_size = 15,  -- Don't ever create areas smaller than this
    fill_chance = 64, -- % chance of filling an area vs. leaving it open
    corridor_width = 4,  -- Padding around a fill area, effectively this is half the corridor width
    jitter_min = -3,  -- Negative jitter shrinks the room in so the corridors get bigger (this can be arbitrarily high)
    jitter_max = 1,  -- Positive jitter grows the room / shrinks the corrdidor, should be _less_ than half your corridor width
    jitter = crawl.coinflip(),
    guaranteed_divides = 1,
    outer_corridor = crawl.coinflip(),
    feature_type = "floor",
    size = { x = gxm-2, y = gym-2 }
  }
  return options
end

-- Builds the paint array for omnigrid
function hyper.layout.omnigrid_primitive(params)
  local options = hyper.layout.omnigrid_defaults()
  if params ~= nil then hyper.merge_options(options,params) end

  local gxm,gym = options.size.x,options.size.y
  local results = hyper.layout.omnigrid_subdivide(0,0,gxm-1,gym-1,options)
  local paint = {}
  if params.floor_func ~= nil then
    util.append(paint,params.floor_func({ x = 0, y = 0},{ x = gxm-1, y = gym-1 },corner2,options.feature_type))
  else
    table.insert(paint,{ type = options.feature_type, corner1 = { x = 0, y = 0}, corner2 = { x = gxm-1, y = gym-1 }, usage = options.usage_params })
  end

  for i, area in ipairs(results) do
    -- Fill the area?
    if crawl.random2(100) < options.fill_chance then
      local off1 = math.floor(options.corridor_width/2)
      local off2 = math.ceil(options.corridor_width/2)
      local corner1 = { x = area.x1, y = area.y1 }
      if corner1.x==0 then
        if options.outer_corridor then corner1.x = corner1.x + options.corridor_width end
      else corner1.x = corner1.x + off1 end
      if corner1.y==0 then
        if options.outer_corridor then corner1.y = corner1.y + options.corridor_width end
      else corner1.y = corner1.y + off1 end
      local corner2 = { x = area.x2, y = area.y2 }
      if corner2.x==gxm-1 then
        if options.outer_corridor then corner2.x = corner2.x - options.corridor_width end
      else corner2.x = corner2.x - off2 end
      if corner2.y==gym-1 then
        if options.outer_corridor then corner2.y = corner2.y - options.corridor_width end
      else corner2.y = corner2.y - off2 end
      -- Perform jitter
      if options.jitter then
        corner1.x = corner1.x - crawl.random_range(options.jitter_min,options.jitter_max)
        corner1.y = corner1.y - crawl.random_range(options.jitter_min,options.jitter_max)
        corner2.x = corner2.x + crawl.random_range(options.jitter_min,options.jitter_max)
        corner2.y = corner2.y + crawl.random_range(options.jitter_min,options.jitter_max)
      end
      if corner1.x<corner2.x and corner1.y<corner2.y then
        if params.paint_func ~= nil then
          util.append(paint, params.paint_func(corner1,corner2,{ feature = "space" }))
        else
          table.insert(paint,{ type = "space", corner1 = corner1, corner2 = corner2 } )
        end
      end
    end
  end
  return paint
end

function hyper.layout.omnigrid_subdivide(x1,y1,x2,y2,options,results,chance,depth)
  -- Default parameters
  if results == nil then results = { } end
  if chance == nil then chance = options.subdivide_initial_chance end
  if depth == nil then depth = 0 end

  local subdiv_x, subdiv_y, subdivide = true,true,true
  local width,height = x2-x1+1,y2-y1+1

  -- Check which if any axes can be subdivided
  if width < 2 * options.minimum_size then
    subdiv_x = false
  end
  if height < 2 * options.minimum_size then
    subdiv_y = false
  end
  if not subdiv_x and not subdiv_y then
    subdivide = false
  end
  if (options.guaranteed_divides == nil or depth >= options.guaranteed_divides) and (crawl.random2(100) >= chance) then
    subdivide = false
  end

  if not subdivide then
    -- End of subdivision; add an area
    table.insert(results, { x1=x1,y1=y1,x2=x2,y2=y2 })
  else
    -- Choose axis? (Remember some might already be too small)
    local which = "x"
    if not subdiv_x then which = "y"
    elseif subdiv_y then
      if crawl.coinflip() then which = "y" end
    end

    local new_chance = chance * options.subdivide_level_multiplier
    local new_depth = depth + 1
    -- Could probably avoid this duplication but it's not that bad
    if which == "x" then
      local pos = crawl.random_range(options.minimum_size,width-options.minimum_size)
      -- Create the two new areas
      hyper.layout.omnigrid_subdivide(x1,y1,x1 + pos - 1,y2,options,results,new_chance,new_depth)
      hyper.layout.omnigrid_subdivide(x1 + pos,y1,x2,y2,options,results,new_chance,new_depth)
    else
      local pos = crawl.random_range(options.minimum_size,height-options.minimum_size)
      -- Create the two new areas
      hyper.layout.omnigrid_subdivide(x1,y1,x2,y1 + pos - 1,options,results,new_chance,new_depth)
      hyper.layout.omnigrid_subdivide(x1,y1 + pos,x2,y2,options,results,new_chance,new_depth)
    end
  end

  return results

end
