------------------------------------------------------------------------------
-- dungeon.lua:
-- Dungeoneering functions.
------------------------------------------------------------------------------

dgn.GXM, dgn.GYM = dgn.max_bounds()

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