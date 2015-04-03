------------------------------------------------------------------------------
-- hyper_paint.lua:
--
-- Functions for painting onto usage grids (for whole layouts and for
-- smaller code rooms).
--
-- TODO: Loads of this file is redundant with procedural primitives. Really
-- we just need some sensible mappings of procedurals to paint shapes and
-- it'll be a lot more flexible.
------------------------------------------------------------------------------

hyper.paint = {}

function hyper.paint.paint_grid(paint, options, usage_grid)

  for i,item in ipairs(paint) do
    local feature,feature_type
    if item.type == "floor" then
      feature_type = "floor"
    elseif item.type == "wall" then
      feature_type = "wall"
    elseif item.type == "space" or item.type == "proc" then
      feature_type = "space"
    elseif item.feature ~= nil then
      feature_type = item.feature
    end

    -- Check which shape to paint
    local shape_type = "quad"
    if item.shape ~= nil then shape_type = item.shape end

    -- Get information about the feature
    local feature,space,solid,wall = hyper.paint.feature_flags(feature_type,options)
    local feature_usage = item.usage

    -- These three flags are in addition to the feature and affect how the builder treats the squares during
    -- placement and subsequently.
    local open = (item.open ~= nil and item.open) -- Further rooms can be placed in this area. Be careful (for now) to leave a border around this so as not to block connections
    local exit = (item.exit ~= nil and item.exit) -- Same as using @ in vaults, allows this square to have connections adjacent, blocks any non-exit squares from being used as such

    if item.corner1 == nil then item.corner1 = { x = 0, y = 0 } end
    if item.corner2 == nil then item.corner2 = { x = usage_grid.width-1, y = usage_grid.height-1 } end

    -- Paint features onto grid
    -- PERF: Can slightly optimise this loop (which could be important since it gets called a lot)
    -- by deciding which function we're going to use first rather than have this unwieldy if statement
    if shape_type == "quad" or shape_type == "ellipse" or shape_type == "trapese" or item.type == "proc" then
      -- Set layout details in the painted area
      for x = item.corner1.x, item.corner2.x, 1 do
        for y = item.corner1.y, item.corner2.y, 1 do
          if item.type == "proc" or shape_type == "quad"
            or (shape_type == "ellipse" and hyper.paint.inside_oval(x,y,item))
            or (shape_type == "trapese" and hyper.paint.inside_trapese(x,y,item))
            or (type(shape_type) == "function" and hyper.paint.inside_custom(x,y,item)) then

            -- For procedural painting we get the feature cell-by-cell
            if item.type == "proc" then
              local mx,my = hyper.paint.map_to_unit(x,y,item)
              -- TODO: Allow proc paint to return an additional parameter containing other properties e.g. tile, monsters, clouds, items, stuff...
              --       (Although often we just want to return generic floor shapes and then tile and populate them later)
              feature_type,feature_usage = item.callback(x,y,mx,my)
              if feature_type ~= nil then
                feature,space,solid,wall = hyper.paint.feature_flags(feature_type,options)
              else
                feature = nil
              end
            end

            -- Work out where to actually paint
            if feature ~= nil then
              local ax,ay = x,y
              if item.wrap then
                ax = x % usage_grid.width
                ay = y % usage_grid.height
              end
              local usage = { solid = solid, feature = feature, space = space, open = open, wall = wall }
              if feature_usage ~= nil then
                hyper.merge_options(usage,feature_usage)
              end
              hyper.usage.set_usage(usage_grid,ax,ay, usage)
            end
          end
        end
      end
    elseif shape_type == "plot" then
      if item.points ~= nil then
        for i,pos in ipairs(item.points) do
          hyper.usage.set_usage(layout_grid, pos.x, pos.y,{ solid = solid, feature = feature, space = space, open = open, wall = wall })
        end
      else
        hyper.usage.set_usage(layout_grid, item.x, item.y, { solid = solid, feature = feature, space = space, open = open, wall = wall })
      end
    end
  end

end

function hyper.paint.feature_flags(feature_type,options)
  local feature
  if feature_type == "floor" then
    feature = options.layout_floor_type
  elseif feature_type == "wall" then
    feature = options.layout_wall_type or "rock_wall"
  elseif feature_type == "space" then
    feature = "space"
  end

  local space = (feature == "space")
  local solid = not space and not (feat.has_solid_floor(feature) or feat.is_door(feature))
  local wall = feat.is_wall(feature)
  return feature,space,solid,wall
end

-- Maps draw positions onto a unit square <0,0> to <1,1>
-- TODO: Something is going wrong with this algorithm because a 4x4 circle shouldn't look
--       like a square, but I don't see anything wrong with the math.
function hyper.paint.map_to_unit(x,y,item)

  local sx,sy = item.corner2.x - item.corner1.x,item.corner2.y - item.corner1.y

  -- Slightly scale down the input grid to add 0.5 squares padding on all sides. This should solve accuracy errors
  -- we otherwise get at the edges of shapes from attempting to draw a continuous geometric shape onto a discrete grid.
  local rx,ry = (x - item.corner1.x) * (sx-1)/sx + 0.5,(y - item.corner1.y) * (sy-1)/sy + 0.5

  -- Finally map onto the unit square
  return rx/sx,ry/sy
end

-- Determine if a point is inside an oval
-- TODO: http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
--       http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
--       Using a variant of that algorithm and adapt to ovals might give more reliable results and be quicker
function hyper.paint.inside_oval(x,y,item)
  -- Circle test
  local ax,ay = hyper.paint.map_to_unit(x,y,item)
  return (math.pow(ax * 2 - 1,2) + math.pow(ay * 2 - 1,2)) <= 1
end

-- Determine if a point is inside a simple trapeze
-- TODO: Handle rotation and allow offsetting the top and bottom on the x axis for more complex shapes
function hyper.paint.inside_trapese(x,y,item)

  local width1 = item.width1
  local width2 = item.width2
  if width1 == nil then width1 = 0 end
  if width2 == nil then width2 = 1 end

  local ax,ay = hyper.paint.map_to_unit(x,y,item)
  local expected_width = ay * width2 + (1-ay) * width1
  return math.abs(ax-0.5) < (expected_width / 2)

end

function hyper.paint.inside_custom(x,y,item)
  local ax,ay = hyper.paint.map_to_unit(x,y,item)
  return item.shape_type(x,y,ax,ay,item)
end

function hyper.paint.determine_usage_from_layout(layout_grid,options)

  usage_restricted_count = 0
  usage_open_count = 0
  usage_eligible_count = 0
  usage_none_count = 0

  local gxm, gym = layout_grid.width,layout_grid.height
  local usage_grid = hyper.usage.new_usage(gxm,gym)

  for x = 0, gxm-1, 1 do
    for y = 0, gym-1, 1 do

      -- We need to know the local layout grid around this square
      local local_grid = { }
      -- This flag will track if there is only floor in the area
      local only_floor = true
      for yl = -options.min_distance_from_wall, options.min_distance_from_wall, 1 do
        local_grid[yl] = { }
        for xl = -options.min_distance_from_wall, options.min_distance_from_wall, 1 do
          local cell = get_layout(layout_grid,x + xl,y + yl)
          local_grid[yl][xl] = cell
          if cell.solid then only_floor = false end
        end
      end

      -- Completely open floor so we could place a room here
      if only_floor == true then
        set_usage(usage_grid,x,y, { usage = "open" })
      else

        -- Are we dealing with floor or wall?
        if local_grid[0][0].solid then -- Wall

          local function gridsum(grid,list)
            local sum = 0
            for i,pos in ipairs(list) do
              if grid[pos.y][pos.x].solid then sum = sum + 1 end
            end
            return sum
          end

          -- A wall can either be usage "none" meaning parts of a room could later be built over it;
          -- or it can be "eligible" meaning it can be used as a connecting wall/door to a room;
          -- or it can be "restricted" meaning its geometry is not suited for a connecting wall or it has already been used

          -- We don't need to cover all cases of complex geometry since for now the layouts will be
          -- making simple large blocks. If complex geometry ever gets used it doesn't matter if we flag some squares
          -- as eligible when they don't really work because room placement will still vetoe if it can't find a clear area of
          -- open or none. It's more important to find all squares that *could* be eligible.

          -- Sum the adjacent squares
          local adjacent_sum = gridsum(local_grid, { {x=-1,y=0},{x=1,y=0},{x=0,y=-1},{x=0,y=1} })
          -- Eligible squares have floor on only one side
          -- This ignores diagonals (which is where complex geometry will produce some eligible squares that aren't
          -- really eligible). But it's complicated because we're after diagonals only on the side where the floor is.

          -- What we need to know in the usage grid is the normal, i.e. the direction in which the user will be entering
          -- the room.
          if adjacent_sum == 3 then
           -- Floor to the north
            if not local_grid[-1][0].solid then
              set_usage(usage_grid,x,y, { usage = "eligible", normal = { x = 0, y = 1 }, depth = 1})
            end
            -- Floor to the south
            if not local_grid[1][0].solid then
              set_usage(usage_grid,x,y, { usage = "eligible", normal = { x = 0, y = -1 }, depth = 1})
            end
            -- Floor to the west
            if not local_grid[0][-1].solid then
              set_usage(usage_grid,x,y, { usage = "eligible", normal = { x = 1, y = 0 }, depth = 1})
            end
            -- Floor to the east
            if not local_grid[0][1].solid then
              set_usage(usage_grid,x,y, { usage = "eligible", normal = { x = -1, y = 0 }, depth = 1})
            end
          else
            -- Wall all around?
            if adjacent_sum == 4 then

              local diagonal_sum = gridsum(local_grid, { {x=-1,y=-1},{x=1,y=-1},{x=-1,y=1},{x=1,y=1} })

              -- Wall mostly all around? (We allow one missing corner otherwise rooms can't overlap corners
              -- and logically it's fine for any wall to be placed there, other missing holes will fail the placement
              -- anyway)
              if diagonal_sum >= 3 then
                -- Should have been set this way at initialization but let's check anyway
                set_usage(usage_grid,x,y, { usage = "none" })
              else
                -- There are some diagonal holes so we can't use this square
                set_usage(usage_grid,x,y, { usage = "restricted" })
              end

            end
          end
        else -- Floor

          -- We already know there is a wall nearby, so this square is restricted
          set_usage(usage_grid,x,y, { usage = "restricted", reason = "border" })

        end
      end

    end
  end
  return usage_grid
end

function hyper.paint.paint_vaults_layout(paint, options, layout_grid)

  -- Default options
  if options == nil then options = vaults_default_options() end

  -- Pick wall type from spread in config
  local wall_type = "stone_wall"
  if options.wall_type ~= nil then wall_type = options.wall_type end
  if options.layout_wall_weights ~= nil then
    local chosen = util.random_weighted_from("weight", options.layout_wall_weights)
    wall_type = chosen.feature
  end
  -- Store it in options so it can be used for room surrounds also
  options.layout_wall_type = wall_type

  local gxm, gym = dgn.max_bounds()
  layout_grid = new_layout(gxm,gym) -- Will contain data about how each square is used and therefore how rooms can be applied
  paint_grid(paint,options,layout_grid) -- Paint fills onto the layout grid

  local usage_grid = hyper.paint.determine_usage_from_layout(layout_grid,options) -- Analyse the layout to determine usage

  -- Apply to the actual dungeon grid
  for x = 0, gxm-1, 1 do
    for y = 0, gym-1, 1 do
      local cell = get_layout(layout_grid,x,y)
      if cell.feature ~= nil and cell.feature ~= "space" then
        dgn.grid(x,y,cell.feature)
      elseif cell.feature == nil then
        -- Make sure we set the right type of wall in unpainted grids
        dgn.grid(x,y,wall_type)
      end
    end
  end

  return usage_grid

end
