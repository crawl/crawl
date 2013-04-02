------------------------------------------------------------------------------
-- v_paint.lua:
--
-- Functions for painting the Vaults layouts and managing
-- walls and areas that rooms can be applied to.
--
-- We will (in the simplest case) paint rectangles of floor onto the dungeon
-- and then sweep across the whole grid to determine which squares can be
-- used for what.
--
-- Whilst placing rooms we'll update the usage grid as areas become available
-- or new walls become eligible (but that's a much more predictable case than
-- initial level generation).
------------------------------------------------------------------------------

------------------------------------------------------------------------------
-- Initialize and empty usage grid
local function new_layout(width, height)
  layout_grid = { width = width, height = height }

  for y = 0, (height - 1), 1 do
    layout_grid[y] = { }
    for x = 0, (width - 1), 1 do
      layout_grid[y][x] = { solid = true } -- 1 means wall
    end
  end
  return layout_grid
end
function vaults_new_layout(width, height)
  return new_layout(width,height)
end
local function new_usage(width, height)
  usage_grid = { eligibles = { }, width = width, height = height }

  for y = 0, (height-1), 1 do
    usage_grid[y] = { }
    for x = 0, (width-1), 1 do
      usage_grid[y][x] = { usage = "none" }
    end
  end
  return usage_grid
end

local function get_usage(usage_grid,x,y)
  -- Handle out of bounds
  if usage_grid[y] == nil then
    return nil
  end
  if usage_grid[y][x] == nil then
    return nil
  end

  return usage_grid[y][x]
end

-- Global version
function vaults_get_usage(usage_grid,x,y)
  return get_usage(usage_grid,x,y)
end

local function set_usage(usage_grid,x,y,usage)
  if usage_grid[y] == nil or usage_grid[y][x] == nil then return false end
  -- Check existing usage, remove it from eligibles if it's there
  local current = usage_grid[y][x]
  if current.eligibles_index ~= nil then
    table.remove(usage_grid.eligibles,current.eligibles_index)
  end
  -- Add to the eligibles list if it's eligible
  if usage.usage == "eligible" or usage.usage == "eligible_open" or usage.usage == "open" then
    usage.spot = { x = x, y = y } -- Store x,y in the usage object otherwise when we look it up in the list we don't know where it came from!
    table.insert(usage_grid.eligibles,usage)
    usage.eligibles_index = #(usage_grid.eligibles)  -- Store index of the new item so we can remove it when it's overwritten
  end
  -- Store usage in grid
  usage_grid[y][x] = usage
end

-- Global version
function vaults_set_usage(usage_grid,x,y,usage)
  return set_usage(usage_grid,x,y,usage)
end

local function get_layout(layout_grid,x,y)
  -- Handle out of bounds
  if layout_grid[y] == nil then
    return { space = true, solid = true, exit = false, feature = "permarock_wall" }
  end
  if layout_grid[y][x] == nil then
    return { space = true, solid = true, exit = false, feature = "permarock_wall" }
  end

  return layout_grid[y][x]
end

function vaults_get_layout(layout_grid,x,y)
  return get_layout(layout_grid,x,y)
end

local function set_layout(layout_grid,x,y,value)
  if layout_grid[y] == nil or layout_grid[y][x] == nil then
  else
    layout_grid[y][x] = value
  end
end
function vaults_set_layout(layout_grid,x,y,value)
  return set_layout(layout_grid,x,y,value)
end

local function determine_usage_from_layout(layout_grid,options)

  usage_restricted_count = 0
  usage_open_count = 0
  usage_eligible_count = 0
  usage_none_count = 0

  local gxm, gym = layout_grid.width,layout_grid.height
  local usage_grid = new_usage(gxm,gym)

  local function gridsum(grid,list)
    local sum = 0
    for i,pos in ipairs(list) do
      if grid[pos.y][pos.x].solid then sum = sum + 1 end
    end
    return sum
  end

  local local_grid = { }
  for yl = -options.min_distance_from_wall, options.min_distance_from_wall, 1 do
    local_grid[yl] = { }
  end

  local adjacent_vectors = { {x=-1,y=0},{x=1,y=0},{x=0,y=-1},{x=0,y=1} }
  local diagonal_vectors = { {x=-1,y=-1},{x=1,y=-1},{x=-1,y=1},{x=1,y=1} }

  for x = 0, gxm-1, 1 do
    for y = 0, gym-1, 1 do
      -- We need to know the local layout grid around this square
      -- This flag will track if there is only floor in the area
      local only_floor = true
      for yl = -options.min_distance_from_wall, options.min_distance_from_wall, 1 do
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

          -- A wall can either be usage "none" meaning parts of a room could later be built over it;
          -- or it can be "eligible" meaning it can be used as a connecting wall/door to a room;
          -- or it can be "restricted" meaning its geometry is not suited for a connecting wall or it has already been used

          -- We don't need to cover all cases of complex geometry since for now the layouts will be
          -- making simple large blocks. If complex geometry ever gets used it doesn't matter if we flag some squares
          -- as eligible when they don't really work because room placement will still vetoe if it can't find a clear area of
          -- open or none. It's more important to find all squares that *could* be eligible.

          -- Sum the adjacent squares
          local adjacent_sum = gridsum(local_grid, adjacent_vectors)
          -- Eligible squares have floor on only one side
          -- This ignores diagonals (which is where complex geometry will produce some eligible squares that aren't
          -- really eligible). But it's complicated because we're after diagonals only on the side where the floor is.

          -- What we need to know in the usage grid is the normal, i.e. the direction in which the user will be entering
          -- the room.
          if adjacent_sum == 3 then
           -- Floor to the north
            if not local_grid[-1][0].solid then
              set_usage(usage_grid,x,y, { usage = "eligible", normal = adjacent_vectors[4], depth = 1})
            end
            -- Floor to the south
            if not local_grid[1][0].solid then
              set_usage(usage_grid,x,y, { usage = "eligible", normal = adjacent_vectors[3], depth = 1})
            end
            -- Floor to the west
            if not local_grid[0][-1].solid then
              set_usage(usage_grid,x,y, { usage = "eligible", normal = adjacent_vectors[2], depth = 1})
            end
            -- Floor to the east
            if not local_grid[0][1].solid then
              set_usage(usage_grid,x,y, { usage = "eligible", normal = adjacent_vectors[1], depth = 1})
            end
          else
            -- Wall all around?
            if adjacent_sum == 4 then

              local diagonal_sum = gridsum(local_grid, diagonal_vectors)

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

-- Determine if a point is inside an oval
-- TODO: There's something wrong with this equation, ovals are coming out with flattened sides and sometimes completely square.
--       Needs somehow adjusting for the fact that we're dealing with grid squares, if that's indeed the problem ...
local function inside_oval(x,y,item)
  -- Radii
  local rx = (item.corner2.x - item.corner1.x)/2
  local ry = (item.corner2.y - item.corner1.y)/2
  -- Center point
  local h = (item.corner2.x + item.corner1.x)/2
  local k = (item.corner2.y + item.corner1.y)/2
  -- Test
  return ((math.pow(x-h,2)/math.pow(rx,2) + math.pow(y-k,2)/math.pow(ry,2))<=1)
end

local function inside_trapese(x,y,item)

  local width1 = item.width1
  local width2 = item.width2
  if width1 == nil then width1 = 0 end
  if width2 == nil then width2 = 1 end

  return true

end

function paint_grid(paint, options, grid)

  for i,item in ipairs(paint) do
    local feature
    if item.type == "floor" then
      feature = options.layout_floor_type
    elseif item.type == "wall" then
      feature = options.layout_wall_type
    elseif item.type == "space" then
      feature = "space"
    elseif item.feature ~= nil then
      feature = item.feature
    end

    -- Check which shape to paint
    local shape_type = "quad"
    if item.shape ~= nil then shape_type = item.shape end

    -- Discover whether the feature type is solid
    local space = (feature == "space")
    local solid = not space and not feat.has_solid_floor(feature)
    local empty = (item.empty ~= nil and item.empty)
    -- Paint features onto grid
    if shape_type == "quad" or shape_type == "ellipse" or shape_type == "trapese" then
      -- TODO: shape types are begging to be modularised
      -- Set layout details in the painted area
      for x = item.corner1.x, item.corner2.x, 1 do
        for y = item.corner1.y, item.corner2.y, 1 do
          if shape_type == "quad" or (shape_type == "ellipse" and inside_oval(x,y,item)) or (shape_type == "trapese" and inside_trapese(x,y,item)) then
            local ax,ay = x,y
            if item.wrap then
              ax = x % grid.width
              ay = y % grid.height
            end

            set_layout(layout_grid,ax,ay, { solid = solid, feature = feature, space = space, empty = empty })
          end
        end
      end
    elseif shape_type == "plot" then
      if item.points ~= nil then
        for i,pos in ipairs(item.points) do
          set_layout(layout_grid, pos.x, pos.y, { solid = solid, feature = feature, empty = empty })
        end
      else
        set_layout(layout_grid, item.x, item.y, { solid = solid, feature = feature, empty = empty })
      end
    end

  end

end

function paint_vaults_layout(paint, options, layout_grid)

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

  if _VAULTS_DEBUG then dump_layout_grid(layout_grid) end

  local usage_grid = determine_usage_from_layout(layout_grid,options) -- Analyse the layout to determine usage

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
