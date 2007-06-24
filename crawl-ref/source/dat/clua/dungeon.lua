------------------------------------------------------------------------------
-- dungeon.lua:
-- Dungeoneering functions.
------------------------------------------------------------------------------

-- Given an object and a table (dgn), returns a table with functions
-- that translate into method calls on the object. This table is
-- suitable for setfenv() on a function that expects to directly
-- address a map object.
function dgn_map_meta_wrap(obj, tab)
   local meta = { }
   for fn, val in pairs(tab) do
      meta[fn] = function (...)
                    return crawl.err_trace(val, obj, ...)
                 end
   end
   meta.wrapped_instance = obj

   local meta_meta = { __index = _G }
   setmetatable(meta, meta_meta)
   return meta
end

function dgn_set_map(map)
   g_dgn_curr_map = map
end

function dgn_run_map(prelude, main)
   if prelude or main then
      env = dgn_map_meta_wrap(g_dgn_curr_map, dgn)
      if prelude then
         setfenv(prelude, env)()
      end
      if main then
         setfenv(main, env)()
      end
      -- Return the environment in case we want to chain further
      -- calls.
      return env
   end
end
