------------------------------------------------------------------------------
-- dungeon.lua:
-- Dungeoneering functions.
------------------------------------------------------------------------------

require("clua/point.lua")

dgn.GXM, dgn.GYM = dgn.max_bounds()
dgn.MAX_MONSTERS = dgn.max_monsters()

dgn.f_map = { }

-- Table that will be saved in <foo>.sav.
dgn.persist = { }

function dgn_save_data(th)
  lmark.marshall_table(th, dgn.persist)
end

function dgn_load_data(th)
  dgn.persist = lmark.unmarshall_table(th) or { }
end

function dgn.fnum(name)
  local fnum = dgn.f_map[name]
  if not fnum then
    fnum = dgn.feature_number(name)
    dgn.f_map[name] = fnum
  end
  return fnum
end

function dgn.monster_fn(spec)
  local mspec = dgn.monster_spec(spec)
  return function (x, y)
           return dgn.create_monster(x, y, mspec)
         end
end

-- Given a feature name or number, returns a feature number.
function dgn.find_feature_number(name_num)
  if type(name_num) == "number" then
    return name_num
  else
    return dgn.fnum(name_num)
  end
end

-- Wraps a map_def into a Lua environment (a table) such that
-- functions run in the environment (with setfenv) can directly
-- address the map with function calls such as name(), tags(), etc.
--
-- This function caches the environments it creates, so that successive runs
-- of Lua chunks from the same map will use the same environment.
function dgn_map_meta_wrap(map, tab)
   if not dgn._map_envs then
      dgn._map_envs = { }
   end

   local name = dgn.name(map)
   local meta = dgn._map_envs[name]

   if not meta then
      meta = { }

      local meta_meta = { __index = _G }
      setmetatable(meta, meta_meta)
      dgn._map_envs[name] = meta
   end

   -- We must set this each time - the map may have the same name, but
   -- be a different C++ object.
   for fn, val in pairs(tab) do
      meta[fn] = function (...)
                    return crawl.err_trace(val, map, ...)
                 end
   end
   meta['_G'] = meta
   meta.wrapped_instance = map
   return meta
end

-- Discards accumulated map environments.
function dgn_flush_map_environments()
   dgn._map_envs = nil
end

function dgn_set_map(map)
   g_dgn_curr_map = map
end

function dgn_run_map(prelude, main)
   if prelude or main then
      local env = dgn_map_meta_wrap(g_dgn_curr_map, dgn)
      if prelude then
         local fn = setfenv(prelude, env)
         if not main then
            return fn()
         end
         fn()
      end
      if main then
         return setfenv(main, env)()
      end
   end
end

--------------------------------------------------------------------

function dgn.places_connected(map, map_glyph, test_connect, ...)
   local points = { }
   for _, glyph in ipairs( { ... } ) do
      local x, y = map_glyph(map, glyph)
      if x and y then
         table.insert(points, x)
         table.insert(points, y)
      else
         error("Can't find coords for '" .. glyph .. "'")
      end
   end
   return test_connect(map, unpack(points))
end

function dgn.any_glyph_connected(map, ...)
   return dgn.places_connected(map, dgn.gly_points,
                              dgn.any_point_connected, ...)
end

function dgn.has_exit_from_glyph(map, glyph)
   return dgn.places_connected(map, dgn.gly_point, dgn.has_exit_from, glyph)
end

function dgn.glyphs_connected(map, ...)
   return dgn.places_connected(map, dgn.gly_point, dgn.points_connected, ...)
end

function dgn.orig_glyphs_connected(map, ...)
   return dgn.places_connected(map, dgn.orig_gly_point,
                               dgn.points_connected, ...)
end

function dgn.orig_fn(map, fnx, ...)
   local original = dgn.original_map(map)
   if not original then
      error("Can't find original map for map '" .. dgn.name(map) .. "'")
   end

   return fnx(original, ...)
end

function dgn.orig_gly_point(map, glyph)
   return dgn.orig_fn(map, dgn.gly_point, glyph)
end

function dgn.orig_gly_points(map, glyph)
   return dgn.orig_fn(map, dgn.gly_points, glyph)
end

function dgn.fnum(feat)
  if type(feat) == 'string' then
    return dgn.feature_number(feat)
  else
    return feat
  end
end

function dgn.fnum_map(map)
  local fnmap = { }
  for k, v in pairs(map) do
    fnmap[dgn.fnum(k)] = dgn.fnum(v)
  end
  return fnmap
end

-- Replaces all features matching
function dgn.replace_feat(rmap)
  local cmap = dgn.fnum_map(rmap)

  for x = 0, dgn.GXM - 1 do
    for y = 0, dgn.GYM - 1 do
      local grid = dgn.grid(x, y)
      local repl = cmap[grid]
      if repl then
        dgn.terrain_changed(x, y, repl)
      end
    end
  end
end

-- Returns a function that returns true if the point specified is
-- travel-passable and is not one of the features specified.
function dgn.passable_excluding(...)
  local forbidden_features =
    util.set(util.map(dgn.find_feature_number, { ... }))
  return function (p)
           local x, y = p.x, p.y
           return dgn.is_passable(x, y) and
             not forbidden_features[dgn.grid(x, y)]
         end
end

-- Returns all adjacent points of c which are in_bounds (i.e. excluding the
-- outermost layer of wall) and for which faccept returns true.
function dgn.adjacent_points(c, faccept)
  local plist = { }

  local compass = {
    { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
    { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 }
  }

  for _, cp in ipairs(compass) do
    local p = dgn.point(c.x + cp[1], c.y + cp[2])
    if dgn.in_bounds(p.x, p.y) and faccept(p) then
      table.insert(plist, p)
    end
  end
  return plist
end

-- Returns the closest point p starting from c (including c) for which
-- fcondition(p) is true, without passing through squares for which
-- fpassable(p) is false. If no suitable square is found, returns nil.
--
-- All points are instances of dgn.point, i.e. a Lua table with x and y keys.
--
-- fpassable may be omitted, in which case it defaults to dgn.is_passable.
function dgn.find_adjacent_point(c, fcondition, fpassable)
  -- Just in case c isn't a real dgn.point - canonicalise.
  c = dgn.point(c.x, c.y)

  local mapped_points = { }

  local function seen(p)
    return mapped_points[p:str()]
  end

  local function record(p, val)
    mapped_points[p:str()] = val or 1
  end

  if not fpassable then
    fpassable = function (p)
                  return dgn.is_passable(p.x, p.y)
                end
  end

  local iter_points = { { }, { } }
  local iter = 1
  table.insert(iter_points[iter], c)

  local function next_iter()
    if iter == 1 then
      return 2
    else
      return 1
    end
  end

  local distance = 1
  record(c, distance)

  while #iter_points[iter] > 0 do
    for _, p in ipairs(iter_points[iter]) do
      if fcondition(p) then
        return p
      end

      -- Add adjacent points to queue.
      for _, np in ipairs(dgn.adjacent_points(p, fpassable)) do
        if not seen(np) then
          table.insert(iter_points[next_iter()], np)
          record(np, distance + 1)
        end
      end
    end
    iter_points[iter] = { }
    iter = next_iter()
    distance = distance + 1
  end

  -- No suitable point.
  return nil
end

-- Scans the entire map, colouring squares for which fselect(p)
-- returns true with colour.
function dgn.colour_map(fselect, colour)
  for x = 0, dgn.GXM - 1 do
    for y = 0, dgn.GYM - 1 do
      if fselect(dgn.point(x, y)) then
        dgn.colour_at(x, y, colour)
      end
    end
  end
end

-- Returns true if fpred returns true for all squares in the rectangle
-- whose top-left corner is tl and bottom right corner is br (br.x >=
-- tl.x and br.y >= tl.y).
function dgn.rectangle_forall(tl, br, fpred)
  for x = tl.x, br.x do
    for y = tl.y, br.y do
      if not fpred(dgn.point(x, y)) then
        return false
      end
    end
  end
  return true
end

-- Sets the grid feature at (x,y) to grid. Optionally, if a marker value
-- is provided, creates a suitable marker at (x,y).
--
-- marker may be
-- 1) a function, in which case a Lua marker is created that
--    will call the function to create a marker table.
-- 2) a table, in which case a Lua marker is created that uses the table
--    directly as a marker table.
-- 3) anything else, which is assumed to be a feature name or number, and
--    is used to create a feature marker.
--
function dgn.gridmark(x, y, grid, marker)
  dgn.grid(x, y, grid)
  if marker then
    local t = type(marker)
    if t == "function" or t == "table" then
      dgn.register_lua_marker(x, y, marker)
    else
      dgn.register_feature_marker(x, y, marker)
    end
  end
end

----------------------------------------------------------------------
-- Convenience functions for vaults.

-- Returns a marker table that sets the destination tag for that square,
-- usually used to set a destination tag for a portal or a stair in a
-- portal vault.
function portal_stair_dst(dst)
  return one_way_stair { dst = dst }
end

-- Common initialisation for portal vaults.
function portal_vault(e, tag)
  if tag then
    e.tags(tag)
  end
  e.orient("encompass")
end

-- Set all stone downstairs in this vault to go to the tag 'next'.
function portal_next(e, next)
  for _, feat in ipairs({ "]", ")", "}" }) do
    e.lua_marker(feat, portal_stair_dst(next))
  end
end

-- Turn persistant data into a human readable string
function persist_to_string()
    return table_to_string(dgn.persist)
end
