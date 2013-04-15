------------------------------------------------------------------------------
-- hyper_caves.lua: Cave drawing functions
--

hyper.caves = {}

-- A really heavily distorted small room.
function hyper.caves.cavern_room(room,options,gen)

  local fborder = procedural.border{ x1 = -1, y1 = -1, x2 = room.size.x, y2 = room.size.y, padding = 2 }
  local fwall = procedural.worley_diff { scale = 0.5 }

  return {
    { type = "proc", callback = function(x,y,mx,my)
      local combi = (1-fborder(x,y)) * (0.1+fwall(x,y))
      return combi < 0.1 and "space" or "floor"
    end }
  }
end

function hyper.caves.cave_walls(room,options,gen)
  return hyper.caves.cave_paint({x = 0, y = 0},{ x = room.size.x-1,y = room.size.y-1})
end

function hyper.caves.cave_room(room,options,gen)
  return hyper.caves.cave_paint({x = 0, y = 0},{ x = room.size.x-1,y = room.size.y-1}, {
    feature = "floor",
    wall = "wall",
    outer_break = 2, -- 1
    wall_break = .8,
    noise_scale = 8,
    padding = 3
    })
end

function hyper.caves.cave_room_walled(room,options,gen)
  return hyper.caves.cave_paint({x = 0, y = 0},{ x = room.size.x-1,y = room.size.y-1}, {
    feature = "floor",
    outer_break = 1,
    noise_scale = 4,
    padding = 4
    })
end

-- A nice crinkly cavernous border usually with a small number of decent-sized internal structures
function hyper.caves.cavern_walls(room,options,gen)
  local fborder = procedural.border{ x1 = 0, y1 = 0, x2 = room.size.x-1, y2 = room.size.y-1, padding = 10 }
  local fwall = procedural.worley_diff { scale = 0.25 }
  local wdiff = util.random_range_real(0.7,0.9)
  return {
    { type = "proc", c1 = { x = 0, y = 0 }, c2 = { x = room.size.x-1, y = room.size.y-1 }, callback = function(x,y,mx,my)
      local combi = fwall(x,y)+fborder(x,y)
      return combi >= wdiff and "space" or "floor"
    end }
  }
end

-- For cave city rooms we want to pick a particular type
-- of cave and build the whole level with that. So this picks
-- a random room function to be used every time.
function hyper.caves.random_cave_room()

  if crawl.one_chance_in(4) then return hyper.caves.cavern_room,{} end

  local params = {
    feature = "floor",
    outer_break = 1,
    noise_scale = util.random_range_real(3,5),
    padding = util.random_range_real(0.5,2)
  }
  if (false and crawl.one_chance_in(3)) then
    -- Add thicker walls
    params.wall = true
    params.wall_feature = "wall"
    params.wall_break = 0.8 -- util.random_range_real(0.7,0.8)
    params.outer_break = 1.5
    params.padding = 2
  end

  return function(room,options,gen)
    return hyper.caves.cave_paint({x = 0, y = 0},{ x = room.size.x-1,y = room.size.y-1},params)
  end, params

end

function hyper.caves.cave_paint(corner1,corner2,params)
  local defaults = {
      feature = "floor",
      wall_feature = nil,
      outer_break = 1.0,
      wall_break = 0.8,
      noise_scale = 2
    }
  params = params ~= nil and hyper.merge_options(defaults,params) or defaults
  local x1,y1,x2,y2 = corner1.x,corner1.y,corner2.x,corner2.y
  local padding = params.padding or math.max(1,crawl.div_rand_round(math.min(x2-x1, y2-y1),8))
  local fborder = procedural.border{ x1 = x1-1, y1 = y1-1, x2 = x2+1, y2 = y2+1, padding = padding, additive = true }
  local fwall = procedural.simplex3d { scale = params.noise_scale }

  local fexit = procedural.radial { origin = { x=(x2-x1)/2,y=(y2-y1)/2 }, phase = util.random_range_real(0,1) }
  local exit_size = util.random_range_real(.2,.4)
  return {
    { type = "proc", corner1 = corner1, corner2 = corner2, callback = function(x,y,mx,my)
      -- Standard algorithm looked pretty good but some internal junk:
      local combi = fborder(x,y) * 2 + fwall(x,y)
      if combi > params.outer_break then return nil
      elseif params.wall_feature and combi > params.wall_break and fexit(x,y) > exit_size then
        return params.wall_feature, { carvable = true }
      else return params.feature end
    end }
  }
end
