------------------------------------------------------------------------------
-- ziggurat.lua:
--
-- Code for ziggurats.
--
-- Important notes:
-- ----------------
-- Functions that are attached to Lua markers' onclimb properties
-- cannot be closures, because Lua markers must be saved and closure
-- upvalues cannot (yet) be saved.
------------------------------------------------------------------------------

require("clua/lm_toll.lua")

-- Deepest you can go in a ziggurat - at this point it's beyond
-- obvious that we're not challenging the player, and one could hope
-- she has enough loot by now.
ZIGGURAT_MAX = 81

function zig()
  if not dgn.persist.ziggurat or not dgn.persist.ziggurat.depth then
    dgn.persist.ziggurat = { }
    -- Initialise here to handle ziggurats accessed directly by &P.
    initialise_ziggurat(dgn.persist.ziggurat)
  end
  return dgn.persist.ziggurat
end

function cleanup_ziggurat()
  return one_way_stair {
    onclimb = function(...)
                dgn.persist.ziggurat = { }
              end,
    dstplace = zig().origin_level
  }
end

local wall_colours = {
  "blue", "red", "lightblue", "magenta", "green", "white"
}

function ziggurat_wall_colour()
  return util.random_from(wall_colours)
end

function initialise_ziggurat(z)
  z.depth = 1

  -- Any given ziggurat will use the same builder for all its levels.
  z.builder = ziggurat_choose_builder()

  z.colour = ziggurat_wall_colour()
  z.level  = { }

  z.origin_level = dgn.level_name(dgn.level_id())
end

function ziggurat_initialiser()
  -- First ziggurat will be initialised twice.
  initialise_ziggurat(zig())
end

local function random_floor_colour()
  return ziggurat_wall_colour()
end

-- Increments the depth in the ziggurat when the player takes a
-- downstair in the ziggurat.
function zig_depth_increment()
  zig().depth = zig().depth + 1
  zig().level = { }
end

-- Returns the current depth in the ziggurat.
local function zig_depth()
  return zig().depth or 0
end

-- Common setup for ziggurat entry vaults.
function ziggurat_portal(e)
  local d = crawl.roll_dice
  local entry_fee =
    10 * math.floor(100 + d(3,200) / 3 + d(10) * d(10) * d(10))

  local function stair()
    return toll_stair {
      amount = entry_fee,
      toll_desc = "to enter a ziggurat",
      desc = "gateway to a ziggurat",
      overmap = "Ziggurat",
      overmap_note = "" .. entry_fee .. " gp",
      dst = "ziggurat",
      dstname = "Ziggurat:1",
      dstname_abbrev = "Zig:1",
      dstorigin = "on level 1 of a ziggurat",
      floor = "stone_arch",
      onclimb = ziggurat_initialiser
    }
  end

  e.lua_marker("O", stair)
  e.kfeat("O = enter_portal_vault")
end

-- Common setup for ziggurat levels.
function ziggurat_level(e)
  e.tags("ziggurat")
  e.tags("allow_dup")
  e.orient("encompass")

  if crawl.game_started() then
    ziggurat_build_level(e)
  end
end

-----------------------------------------------------------------------------
-- Ziggurat level builders.

function ziggurat_build_level(e)
  local builder = zig().builder
  if builder then
    return ziggurat_builder_map[builder](e)
  end
end

local zigstair = dgn.gridmark

-- Creates a Lua marker table that increments ziggurat depth.
local function zig_go_deeper()
  local newdepth = zig().depth + 1
  return one_way_stair {
    onclimb = zig_depth_increment,
    dstname = "Ziggurat:" .. newdepth,
    dstname_abbrev = "Zig:" .. newdepth,
    dstorigin = "on level " .. newdepth .. " of a ziggurat"
  }
end

local function map_area()
  local base_area = 5 + 2 * zig_depth()
  return 4 * base_area + crawl.random2(base_area)
end

local function clamp_in(val, low, high)
  if val < low then
    return low
  elseif val > high then
    return high
  else
    return val
  end
end

local function clamp_in_bounds(x, y)
  return clamp_in(x, 1, dgn.GXM - 2), clamp_in(y, 1, dgn.GYM - 2)
end

local function rectangle_dimensions()
  local area = map_area()

  local cx, cy = dgn.GXM / 2, dgn.GYM / 2

  local function rectangle_eccentricity()
  -- exc is the excentrity for the two rectangle, it grows with depth as
  -- 0, 0-1, 1, 1-2, 2, ...
    local exc = math.floor((zig().depth-1) / 2)
    if ((zig().depth-1) % 2) ~= 0 and crawl.coinflip() then
      exc = exc + 1
    end
    return exc
  end

  local exc = rectangle_eccentricity()
  local b = math.floor(math.sqrt(area+12*exc*exc)) - 4*exc
  local a = math.floor((area + b - 1) / b)

  local a2 = math.floor(a / 2) + (a % 2)
  local b2 = math.floor(b / 2) + (b % 2)
  local x1, y1 = clamp_in_bounds(cx - a2, cy - b2)
  local x2, y2 = clamp_in_bounds(cx + a2, cy + b2)
  return x1, y1, x2, y2
end

local function set_floor_colour(colour)
  if not zig().level.floor_colour then
    zig().level.floor_colour = colour
    dgn.change_floor_colour(colour, false)
  end
end

local function set_random_floor_colour()
  set_floor_colour( random_floor_colour() )
end

local function with_props(spec, props)
  return util.cathash({ spec = spec }, props)
end

local function depth_if(spec, fn)
  return { spec = spec, cond = fn }
end

local function depth_ge(lev, spec)
  return depth_if(spec, function ()
                          return zig().depth >= lev
                        end)
end

local function depth_lt(lev, spec)
  return depth_if(spec, function ()
                          return zig().depth < lev
                        end)
end

local function monster_creator_fn(arg)
  local atyp = type(arg)
  if atyp == "string" then
    local _, _, branch = string.find(arg, "^place:(%w+):")
    local function mspec(x, y, nth)
      if branch then
        set_floor_colour(dgn.br_floorcol(branch))
      end

      return dgn.create_monster(x, y, arg)
    end
    return { fn = mspec }
  elseif atyp == "table" then
    if not arg.cond or arg.cond() then
      return util.cathash(monster_creator_fn(arg.spec), arg)
    end
  elseif atyp == "function" then
    return { fn = arg }
  end
end

local mons_populations = {
  -- Dress up monster sets a bit.
  "place:Elf:$ w:300 / deep elf blademaster / deep elf master archer / " ..
    "deep elf annihilator / deep elf sorcerer / deep elf demonologist",
  "place:Orc:$ w:120 / orc warlord / orc knight / stone giant",
  "place:Vault:$",
  with_props("place:Slime:$", { jelly_protect = true }),
  "place:Snake:$",
  "place:Lair:$",
  "place:Tomb:$",
  "place:Crypt:$",
  "place:Abyss",
  "place:Shoal:$",
  depth_ge(6, "place:Pan w:400 / w:15 pandemonium lord"),
  depth_lt(6, "place:Pan")
}

local function mons_random_gen(x, y, nth)
  set_random_floor_colour()
  local mgen = nil
  while not mgen do
    mgen = monster_creator_fn(util.random_from(mons_populations))
    if mgen then
      mgen = mgen.fn
    end
  end
  return mgen(x, y, nth)
end

local function mons_drac_gen(x, y, nth)
  set_random_floor_colour()
  return dgn.create_monster(x, y, "random draconian")
end

local function mons_panlord_gen(x, y, nth)
  set_random_floor_colour()
  if nth == 1 then
    return dgn.create_monster(x, y, "pandemonium lord")
  else
    return dgn.create_monster(x, y, "place:Pan")
  end
end

local mons_generators = {
  mons_random_gen,
  depth_ge(6, mons_drac_gen),
  depth_ge(8, mons_panlord_gen)
}

function ziggurat_monster_creators()
  return util.map(monster_creator_fn,
                  util.catlist(mons_populations, mons_generators))
end

local function ziggurat_vet_monster(fmap)
  local fn = fmap.fn
  fmap.fn = function (x, y, nth, hdmax)
              for i = 1, 100 do
                local mons = fn(x, y, nth)
                if mons then
                  -- Discard zero-exp monsters, and monsters that explode
                  -- the HD limit.
                  if mons.experience == 0 or mons.hd > hdmax * 1.3 then
                    mons.dismiss()
                  else
                    if mons.muse == "eats_items" then
                      zig().level.jelly_protect = true
                    end
                    -- Monster is ok!
                    return mons
                  end
                end
              end
              -- Give up.
              return nil
            end
  return fmap
end

local function choose_monster_set()
  return ziggurat_vet_monster(util.random_from(ziggurat_monster_creators()))
end

-- Function to find travel-safe squares, excluding closed doors.
local dgn_passable = dgn.passable_excluding("closed_door")

local function ziggurat_create_monsters(p, mfn)
  local depth = zig_depth()
  local hd_pool = depth * (depth + 8)

  local function mons_place_p(point)
    return not dgn.mons_at(point.x, point.y)
  end

  local nth = 1

  while hd_pool > 0 do
    local place = dgn.find_adjacent_point(p, mons_place_p, dgn_passable)
    local mons = mfn(place.x, place.y, nth, hd_pool)

    if mons then
      nth = nth + 1
      hd_pool = hd_pool - mons.hd
    else
      -- Can't find any suitable monster for the HD we have left.
      break
    end
  end
end

local function flip_rectangle(x1, y1, x2, y2)
  local cx = math.floor((x1 + x2) / 2)
  local cy = math.floor((y1 + y2) / 2)
  local nx1 = cx + y1 - cy
  local nx2 = cx + y2 - cy
  local ny1 = cy + x1 - cx
  local ny2 = cy + x2 - cx
  return { nx1, ny1, nx2, ny2 }
end

local function ziggurat_create_loot_at(c)
  local nloot = zig_depth()
  local depth = zig_depth()

  local function free_space_threshold(max)
    local function is_free_space(p)
      return dgn.grid(p.x, p.y) == dgn.fnum("floor") and
        #dgn.items_at(p.x, p.y) <= max
    end
    return is_free_space
  end

  local function free_space_do(fn)
    for i = 0, 20 do
      local p =
        dgn.find_adjacent_point(c, free_space_threshold(i), dgn_passable)
      if p then
        fn(p)
        break
      end
    end
  end

  local loot_depth = 20
  if you.absdepth() > loot_depth then
    loot_depth = you.absdepth() - 1
  end

  local function place_loot(what)
    free_space_do(function (p)
                    dgn.create_item(p.x, p.y, what, loot_depth)
                  end)
  end

  for i = 1, nloot do
    if crawl.one_chance_in(depth) then
      for j = 1, 4 do
        place_loot("*")
      end
    else
      place_loot("|")
    end
  end
end

-- Suitable for use in loot vaults.
function ziggurat_loot_spot(e, key)
  e.lua_marker(key, portal_desc { ziggurat_loot = "X" })
  e.kfeat(key .. " = .")
  e.marker("@ = feat: permarock_wall")
  e.kfeat("@ = +")
end

local function ziggurat_create_loot_vault(entry, exit)
  local inc = (exit - entry):sgn()

  local function find_door_spot(p)
    while not dgn.is_wall(dgn.grid(p.x, p.y)) do
      p = p + inc
    end
    return p
  end

  local connect_point = exit - inc * 3

  local map = dgn.map_by_tag("ziggurat_loot_chamber", false)

  local function loot_fallback()
    crawl.mpr("Failed to create loot vault, giving up", "diagnostic")
    -- FAIL, generate loot the usual way.
    ziggurat_create_loot_at(exit)
  end

  if not map then
    return loot_fallback()
  end

  local function place_loot_chamber()
    local res = dgn.place_map(map, false, true)
    if res then
      zig().level.loot_chamber = true
    end
    return res
  end

  local function bad_loot_bounds(map, px, py, xs, ys)
    local vc = dgn.point(px + math.floor(xs / 2),
                         py + math.floor(ys / 2))


    local function safe_area()
      local p = dgn.point(px, py)
      local sz = dgn.point(xs, ys)
      local floor = dgn.fnum("floor")
      return dgn.rectangle_forall(p, p + sz - 1,
                                  function (c)
                                    return dgn.grid(c.x, c.y) == floor
                                  end)
    end

    local linc = (exit - vc):sgn()
    -- The map's positions should be at the same increment to the exit
    -- as the exit is to the entrance, else reject the place.
    return not (inc == linc) or not safe_area()
  end

  local function connect_loot_chamber()
    return dgn.with_map_bounds_fn(bad_loot_bounds, place_loot_chamber)
  end

  local res = dgn.with_map_anchors(connect_point.x, connect_point.y,
                                   connect_loot_chamber)
  if not res then
    loot_fallback()
  else
    -- Find the square to drop the loot.
    local lootx, looty = dgn.find_marker_prop("ziggurat_loot")

    if lootx and looty then
      ziggurat_create_loot_at(dgn.point(lootx, looty))
    else
      loot_fallback()
    end
  end
end

local function ziggurat_create_loot(entrance, exit)
  if zig().level.jelly_protect then
    ziggurat_create_loot_vault(entrance, exit)
  else
    ziggurat_create_loot_at(exit)
  end
end

local function ziggurat_place_pillars(c)
  local range = crawl.random_range
  local floor = dgn.fnum("floor")

  local map, vplace = dgn.resolve_map(dgn.map_by_tag("ziggurat_pillar"))

  if not map then
    return
  end

  local name = dgn.name(map)

  local size = dgn.point(dgn.mapsize(map))

  -- Does the pillar want to be centered?
  local centered = string.find(dgn.tags(map), " centered ")

  local function good_place(p)
    local function good_square(where)
      return dgn.grid(where.x, where.y) == floor
    end
    return dgn.rectangle_forall(p, p + size - 1, good_square)
  end

  local function place_pillar()
    if centered then
      if good_place(c) then
        return dgn.place_map(map, false, true, c.x, c.y)
      end
    else
      for i = 1, 100 do
        local offset = range(-15, -size.x)
        local offsets = {
          dgn.point(offset, offset) - size + 1,
          dgn.point(offset - size.x + 1, -offset),
          dgn.point(-offset, -offset),
          dgn.point(-offset, offset - size.y + 1)
        }

        offsets = util.map(function (o)
                             return o + c
                           end, offsets)

        if util.forall(offsets, good_place) then
          local function replace(at, hflip, vflip)
            dgn.reuse_map(vplace, at.x, at.y, hflip, vflip)
          end

          replace(offsets[1], false, false)
          replace(offsets[2], false, true)
          replace(offsets[3], true, false)
          replace(offsets[4], false, true)
          return true
        end
      end
    end
  end

  for i = 1, 5 do
    if place_pillar() then
      break
    end
  end
end

local function ziggurat_rectangle_builder(e)
  local grid = dgn.grid

  dgn.fill_area(0, 0, dgn.GXM - 1, dgn.GYM - 1, "stone_wall")

  local x1, y1, x2, y2 = rectangle_dimensions()
  dgn.fill_area(x1, y1, x2, y2, "floor")

  dgn.fill_area(unpack( util.catlist(flip_rectangle(x1, y1, x2, y2),
                                     { "floor" }) ) )

  local c = dgn.point(x1 + x2, y1 + y2) / 2

  local entry = dgn.point(x1, c.y)
  local exit = dgn.point(x2, c.y)

  if zig_depth() % 2 == 0 then
    entry, exit = exit, entry
  end

  zigstair(entry.x, entry.y, "stone_arch", "stone_stairs_up_i")

  if zig().depth < ZIGGURAT_MAX then
    zigstair(exit.x, exit.y, "stone_stairs_down_i", zig_go_deeper)
  end

  zigstair(exit.x, exit.y + 1, "exit_portal_vault", cleanup_ziggurat())
  zigstair(exit.x, exit.y - 1, "exit_portal_vault", cleanup_ziggurat())

  local monster_generation = choose_monster_set()

  -- If we're going to spawn jellies, do our loot protection thing.
  if monster_generation.jelly_protect then
    zig().level.jelly_protect = true
  end

  ziggurat_create_loot(entry, exit)

  if not zig().level.loot_chamber then
    -- Place pillars if we did not create a loot chamber.
    ziggurat_place_pillars(c)
  end

  ziggurat_create_monsters(exit, monster_generation.fn)

  local function needs_colour(p)
    return not dgn.in_vault(p.x, p.y)
      and dgn.grid(p.x, p.y) == dgn.fnum("stone_wall")
  end

  dgn.colour_map(needs_colour, zig().colour)
end

----------------------------------------------------------------------

ziggurat_builder_map = {
  rectangle = ziggurat_rectangle_builder
}

local ziggurat_builders = { }
for key, val in pairs(ziggurat_builder_map) do
  table.insert(ziggurat_builders, key)
end

function ziggurat_choose_builder()
  return util.random_from(ziggurat_builders)
end
