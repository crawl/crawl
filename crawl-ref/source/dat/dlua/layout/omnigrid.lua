------------------------------------------------------------------------------
-- omnigrid.lua: Create flexible layout grids by subdivision
--
------------------------------------------------------------------------------

omnigrid = {}

omnigrid.grid_defaults = {
  subdivide_initial_chance = 100, -- % chance of subdividing at first level, if < 100 then we might just get chaotic city
  subdivide_level_multiplier = 0.80,   -- Multiply subdivide chance by this amount with each level
  -- Don't ever create areas smaller than this
  minimum_size_x = 15,
  minimum_size_y = 15,
  guaranteed_divides = 1,
  choose_axis = function(width,height,depth)
    return crawl.coinflip() and "y" or "x"
  end,
}

omnigrid.paint_defaults = {
  fill_chance = 64, -- % chance of filling an area vs. leaving it open
  corridor_width = 4,  -- Padding around a fill area, effectively this is half the corridor width
  jitter_min = -3,  -- Negative jitter shrinks the room in so the corridors get bigger (this can be arbitrarily high)
  jitter_max = 1,  -- Positive jitter grows the room / shrinks the corrdidor, should be _less_ than half your corridor width
  jitter = false,
  outer_corridor = true,
  feature_type = "floor"
}

-- Builds the paint array for omnigrid
function omnigrid.paint(grid,options,paint)
  -- Inherit default options
  if options == nil then options = {} end
  setmetatable(options, { __index = omnigrid.paint_defaults })

  if paint == nil then paint = {} end

  for i, area in ipairs(grid) do
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
      if corner2.x==options.size.x-1 then
        if options.outer_corridor then corner2.x = corner2.x - options.corridor_width end
      else corner2.x = corner2.x - off2 end
      if corner2.y==options.size.y-1 then
        if options.outer_corridor then corner2.y = corner2.y - options.corridor_width end
      else corner2.y = corner2.y - off2 end
      -- Perform jitter
      if options.jitter then
        corner1.x = corner1.x - crawl.random_range(options.jitter_min,options.jitter_max)
        corner1.y = corner1.y - crawl.random_range(options.jitter_min,options.jitter_max)
        corner2.x = corner2.x + crawl.random_range(options.jitter_min,options.jitter_max)
        corner2.y = corner2.y + crawl.random_range(options.jitter_min,options.jitter_max)
      end
      if corner1.x<=corner2.x and corner1.y<=corner2.y then
        if options.paint_func ~= nil then
          util.append(paint, options.paint_func(corner1,corner2,{ feature = "space" }))
        else
          table.insert(paint,{ type = "space", corner1 = corner1, corner2 = corner2 } )
        end
      end
    end
  end

  return paint

end

-- TODO: Consolidate the structure this produces with the zonifier zonemap
function omnigrid.subdivide(x1,y1,x2,y2,options)
  -- Inherit default options
  if options == nil then options = {} end
  setmetatable(options, { __index = omnigrid.grid_defaults })
  -- Set minimum sizes for both axes if we have a single minimum size
  if options.minimum_size ~= nil then
    options.minimum_size_x,options.minimum_size_y = options.minimum_size,options.minimum_size
  end
  return omnigrid.subdivide_recursive(x1,y1,x2,y2,options,
                                      options.subdivide_initial_chance,0)
end

function omnigrid.subdivide_recursive(x1,y1,x2,y2,options,chance,depth)

  local subdiv_x, subdiv_y, subdivide = true,true,true
  local width,height = x2-x1+1,y2-y1+1

  -- Check which if any axes can be subdivided
  if width < 2 * options.minimum_size_x then
    subdiv_x = false
  end
  if height < 2 * options.minimum_size_y then
    subdiv_y = false
  end
  if not subdiv_x and not subdiv_y then
    subdivide = false
  end

  if (options.guaranteed_divides == nil or depth >= options.guaranteed_divides)
     and (crawl.random2(100) >= chance) then
    subdivide = false
  end

  if not subdivide then
    -- End of subdivision; add an area
    return { { x1=x1,y1=y1,x2=x2,y2=y2,depth=depth,borders={} } }
  else
    local results = {}

    -- Choose axis? (Force it if one is too small)
    local which = "x"
    if not subdiv_x then which = "y"
    elseif subdiv_y then
      which = options.choose_axis(width,height,depth)
    end

    local new_chance = chance * options.subdivide_level_multiplier
    local new_depth = depth + 1
    -- Could probably avoid this duplication but it's not that bad
    if which == "x" then
      local pos = crawl.random_range(options.minimum_size_x,width-options.minimum_size_x)
      -- Create the two new areas
      local resa = omnigrid.subdivide_recursive(x1,y1,x1 + pos - 1,y2,options,new_chance,new_depth)
      local resb = omnigrid.subdivide_recursive(x1 + pos,y1,x2,y2,options,new_chance,new_depth)
      -- Compare the two sets of results to create borders
      for ia,a in ipairs(resa) do
        table.insert(results,a)
        if a.x2 == (x1+pos-1) then
          for ib,b in ipairs(resb) do
            if b.x1 == x1 + pos then
              -- These two are bordering. The border is the horizontal intersection of the two lines.
              local by1 = math.max(a.y1,b.y1)
              local by2 = math.min(a.y2,b.y2)
              -- If there's an actual intersection
              if (by1<=by2) then
                local b1 = { of = a, with = b, dir = 3, x1 = a.x2, x2 = a.x2, y1 = by1, y2 = by2, len = by2-by1+1 }
                local b2 = { of = b, with = a, dir = 1, x1 = b.x1, x2 = b.x1, y1 = by1, y2 = by2, len = by2-by1+1 }
                b1.inverse,b2.inverse = b2,b1
                table.insert(a.borders,b1)
                table.insert(b.borders,b2)
              end
            end
          end
        end
      end
      for ib,b in ipairs(resb) do
        table.insert(results,b)
      end
    else
      local pos = crawl.random_range(options.minimum_size_y,height-options.minimum_size_y)
      -- Create the two new areas
      local resa = omnigrid.subdivide_recursive(x1,y1,x2,y1 + pos - 1,options,new_chance,new_depth)
      local resb = omnigrid.subdivide_recursive(x1,y1 + pos,x2,y2,options,new_chance,new_depth)
      -- Compare the two sets of results to create borders
      for ia,a in ipairs(resa) do
        table.insert(results,a)
        if a.y2 == (y1+pos-1) then
          for ib,b in ipairs(resb) do
            if b.y1 == y1 + pos then
              -- These two are bordering. The border is the horizontal intersection of the two lines.
              local bx1 = math.max(a.x1,b.x1)
              local bx2 = math.min(a.x2,b.x2)
              -- If there's an actual intersection
              if (bx1<=bx2) then
                local b1 = { of = a, with = b, dir = 2, x1 = bx1, x2 = bx2, y1 = a.y2, y2 = a.y2, len = bx2-bx1+1 }
                local b2 = { of = b, with = a, dir = 0, x1 = bx1, x2 = bx2, y1 = b.y1, y2 = b.y1, len = bx2-bx1+1 }
                b1.inverse,b2.inverse = b2,b1
                table.insert(a.borders,b1)
                table.insert(b.borders,b2)
              end
            end
          end
        end
      end
      for ib,b in ipairs(resb) do
        table.insert(results,b)
      end
    end
    return results
  end
end

function omnigrid.connect(options)

  local grid = options.grid
  local groups = options.groups

  local count = 0
  if groups == nil then
    groups = { }
    for i,cell in ipairs(grid) do
      local group = { cell }
      cell.group = group
      cell.borders_left = #(cell.borders)
      groups[group] = group
    end
    count = #grid
  else
    -- Slow way to count a previously-created groups list; Lua doesn't
    -- have a function for counting number of hash keys
    for k,v in pairs(groups) do
      if v ~= nil then count = count + 1 end
    end
  end

  local function mergegroup(a,b)
    if a == b then return; end
    for i,cell in ipairs(b) do
      cell.group = a
      table.insert(a,cell)
    end
    groups[b] = nil
    count = count - 1
  end

  local bail = false
  local iters = 0
  local minbord = options.min_border_length or 2
  local maxiters = options.max_iterations or 500
  local mingroups = options.min_groups or 1

  -- Setup default callbacks
  local fbail = options.bail_func or function(groups,count) return count <= mingroups end
  local fstyle = options.style_func or function() return options.default_style or "open" end
  local wgroup = options.group_weight or function(k,v) return v ~= nil and math.ceil(1000/#(v)) or 0 end
  local wcell = options.cell_weight or function(c) return c.borders_left end
  -- Ensure border hasn't already been connected (given a style)
  -- and also that it's of enough length to actually create a path
  -- between the rooms
  local wbord = options.border_weight or function(b) return b.style == nil and b.len >= minbord and b.with.group ~= b.of.group and 10 or 0 end

  while not bail do
    iters = iters + 1

    -- Randomly pick a group
    local group = util.random_weighted_keys(wgroup, groups)
    -- Pick a cell from the group
    local cell = group and util.random_weighted_from(wcell, group)
    local border = cell and util.random_weighted_from(wbord, cell.borders)
    if border then
      local style = fstyle(border,cell,groups,count)
      local new = (border.style == nil)
      border.style = style
      border.inverse.style = style
      if new then
        cell.borders_left = cell.borders_left - 1
        border.with.borders_left = border.with.borders_left - 1
      end
      mergegroup(group,border.with.group)
    end

    -- Main exit condition; once we've reduced to a single group
    if fbail(groups,count) or iters > maxiters or group == nil then bail = true end
  end

  return groups
end
