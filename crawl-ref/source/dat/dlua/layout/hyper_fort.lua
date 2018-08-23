------------------------------------------------------------------------------
-- hyper_fort.lua:
--
-- Decided to keep this separate as well; it might be used from a lot of
-- places.
--
-- Builds interesting fort structures with crennelations and internal
-- architecture.
------------------------------------------------------------------------------

crawl_require("dlua/hyper_city.lua")

hyper.fort = {}

-- Forts with crennelations. This vault can be as small as 5x5 but will look much better at larger sizes.
-- TODO: * Figure out how to create lava/deep water moats and paint drawbridges from the doors.
--       * Or, fill the whole level with lava/deep water and ensure the forts are connected up
--       * Make forts connect up with cloisters
--       * Another option should be to fill the fort with solid wall and allow rooms to be carved within
--       * Add interesting decor spots in the center of crennelations if they're large enough
-- First versions of forts were built by a drunken architect but they looked awesome in a different way, e.g.:
-- TODO: Integrate slightly broken designs into shape_fort and enable on a parameter occasionally.
--       * Corner crennelations were half size so they were offset / only on certain sizes
--       * One corner was missing (could randomly omit corners)
--       * Side crennelations sometimes bigger on one side than the other and missing from one axis
--       * Side crennelations being clustered in the middle, not going all the way along the wall (started with pos too big) - could also randomly omit them
-- Other ideas for ruination:
--       * Randomly scatter sections of wall to look like they've been smashed down in a battle or crumbled over time
function hyper.fort.shape_fort(params)
  -- Figure out how size of corner crennelations
  local shortest_axis = math.min(params.size.x,params.size.y)
  local corners_min = math.max(2, crawl.div_rand_round(shortest_axis, 8))
  local corners_max = math.max(corners_min, crawl.div_rand_round(shortest_axis, 5))
  local corners = crawl.random_range(corners_min,corners_max)
  -- Pad the map by half the corner size
  local padding = crawl.div_rand_round(corners,2)

  -- Paint floor with padding around the edge
  local paint = {
    -- Generate a border of exit and fill the middle with floor. The exits will get painted over by the crennelations. This ensures
    -- doors and attachments only happen in between the crennelations.
    -- TODO: Something seems broken with exit analysis
    { type = "floor", corner1 = { x = padding, y = padding }, corner2 = { x = room.size.x - 1 - padding, y = room.size.y - 1 - padding }, exit = true },
    -- Finally leave a border and paint an open area in the middle for inner geometry placement
    { type = "floor", corner1 = { x = padding + 1, y = padding + 1 }, corner2 = { x = room.size.x - 2 - padding, y = room.size.y - 2 - padding }, open = true },
    -- { type = "floor", corner1 = { x = padding + 2, y = padding + 2 }, corner2 = { x = room.size.x - 3 - padding, y = room.size.y - 3 - padding }, open = true },
  }

  -- Paint corner crennelations
  for i, n in ipairs(vector.diagonals) do
    local cx = ((n.x + 1) / 2) * (params.size.x - corners)
    local cy = ((n.y + 1) / 2) * (params.size.y - corners)
    table.insert(paint, { type = "floor", corner1 = { x = cx, y = cy }, corner2 = { x = cx + corners - 1, y = cy + corners - 1 } })
  end

  -- Size of remaining crennelations
  -- TODO: It's looking pretty good already but we could add more variation in this randomisation. Also vary the sides more (as long as opposite
  --       sides are symmetrical (apart from 'drunk' version) which the algorithm nicely supports)
  local cren = math.min(crawl.random_range(crawl.div_rand_round(padding,2), padding), math.max(1,crawl.div_rand_round(shortest_axis - 2 * corners, 5)))
  if cren == 0 then return paint end

  local cren_depth = math.max(1,crawl.div_rand_round(cren,2))
  local cren_gap = crawl.random_range(math.max(1,crawl.div_rand_round(cren_depth,2)),math.min(cren_depth*3,math.floor((shortest_axis - 2 * corners) / 3)))

  -- Deal with each side separately. Calculate how many cells we have remaining on each side.
  local sides = {
    { x = 1, y = 0, rem = room.size.x - 2 * corners - (crawl.random_range(0,3)) },
    { x = 0, y = 1, rem = room.size.y - 2 * corners - (crawl.random_range(0,3)) },  -- The random(0,3) will cause a gap to be left in the middle of up to 3 squares
  }

  for i,side in ipairs(sides) do
    local rem = side.rem
    local pos = corners
    -- Work from the outside and see how many crennelations we can add
    while (rem > 0) do
      -- Subtract gap on both sides
      rem = rem - 2 * cren_gap
      pos = pos + cren_gap
      if rem >= (2 * cren) then
        -- Draw symmetrical crennelations on each side (we'll end up drawing the center ones twice over but it doesn't hugely matter)
        for n = 0, 1, 1 do
          for m = 0, 1, 1 do
            -- Don't worry about the math here, it all makes sense. Actually it would have been simpler to just insert four paint lines but oh well.
            local spos = (1-m) * pos + m * ( (side.x * room.size.x) + (side.y * room.size.y) - pos - cren )
            local npos = (1-n) * (padding - cren_depth) + n * ( (side.x * room.size.y) + (side.y * room.size.x) - padding )
            c1 = {
              x = side.x * (spos) + (1 - side.x) * (npos),
              y = side.y * (spos) + (1 - side.y) * (npos)
            }
            table.insert(paint, { type = "floor", corner1 = c1,
              corner2 = { x = c1.x + (side.x * cren) + (side.y * cren_depth) - 1, y = c1.y + (side.y * cren) + (side.x * cren_depth) - 1 } })
          end
        end
      end
      rem = rem - 2 * cren
      pos = pos + cren
    end
  end

  -- This is new. Since we know exactly where we should build internal stuff right now, let's do it. This is more controlled than letting
  -- random placement do the job later.
  -- TODO: There should be a consistent mechanism to achieve this across different rooms - from outside being able to set subdivision
  -- params / methods, placement strategies, as well as generator weights. And not to mention that any child rooms could themselves
  -- spawn a child build process.

  -- So -- rather than even trying to do this here, instead we start off with an area

  -- local zones = hyper.city.omnigrid_
  -- table.insert(paint, { type = "build", passes = passes })

  return paint

end

-- Room function for the fort.
function hyper.fort.room_fort(room,options,gen)
  -- TODO: Need more options. Map gen.fort_options onto the params table.
  return hyper.fort.shape_fort{
    size = room.size
    -- entrances = gen.entrances
  }
end
