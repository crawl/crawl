------------------------------------------------------------------------------
-- dungeon.lua:
-- Dungeoneering functions.
------------------------------------------------------------------------------

crawl_require("dlua/point.lua")
crawl_require("dlua/fnwrap.lua")
crawl_require('dlua/util.lua')

-- Namespace for callbacks (just an aid to recognising callbacks, no magic)
util.namespace('callback')

dgn.GXM, dgn.GYM = dgn.max_bounds()
dgn.MAX_MONSTERS = dgn.max_monsters()

dgn.f_map = { }
dgn.MAP_GLOBAL_HOOKS = { }

dgn.MAP_HOOK_NAMES = {
  "pre_main", "post_main", "post_place",
  "pre_epilogue", "post_epilogue"
}

dgn.MAP_HOOK_ALIASES = {
  main = 'post_main',
  epilogue = 'post_epilogue',
  place = 'post_place'
}

local function dgn_hook_variable_name(hook_name)
  hook_name = dgn.MAP_HOOK_ALIASES[hook_name] or hook_name
  return "HOOK_" .. hook_name
end

local function dgn_init_hook_tables(container_table)
  -- Define hook tables for Lua hooks.
  for _, hook in ipairs(dgn.MAP_HOOK_NAMES) do
    container_table[dgn_hook_variable_name(hook)] = { }
  end
end

dgn_init_hook_tables(dgn.MAP_GLOBAL_HOOKS)

-- Table that will be saved in <foo>.sav.
dgn.persist = { }

function dgn_clear_data()
  dgn.persist = { }
end

function dgn_save_data(th)
  lmark.marshall_table(th, dgn.persist)
end

function dgn_load_data(th)
  dgn.persist = lmark.unmarshall_table(th) or { }
end

function dgn_set_persistent_var(var, val)
  dgn.persist[var] = val
end

function dgn.fnum(name)
  local fnum = dgn.f_map[name]
  if not fnum then
    fnum = dgn.feature_number(name)
    dgn.f_map[name] = fnum
  end
  return fnum
end

function dgn.map_environment(map)
  if not map then
    map = g_dgn_curr_map
  end
  assert(map, "Can't get map_environment: No active map")
  local mapname = dgn.name(map)
  return dgn.map_environment_by_name(mapname)
end

function dgn.map_environment_by_name(mapname)
  assert(dgn._map_envs, "Can't get map environment for " .. mapname ..
         ": no environments available")
  local mapenv = dgn._map_envs[mapname]
  return mapenv or { }
end

function dgn.hook_environment(hook_env, hook_name, fn)
  local hook_table = hook_env[dgn_hook_variable_name(hook_name)]
  assert(hook_table and type(hook_table) == 'table',
         "Bad hook table for hook " .. hook_name)
  table.insert(hook_table, fn)
end

-- Register a hook for the current map.
function dgn.hook(map, hook_name, fn)
  local map_environment = dgn.map_environment_by_name(dgn.name(map))
  dgn.hook_environment(map_environment, hook_name, fn)
end

-- Register a global hook that will be fired for all maps.
function dgn.global_hook(hook_name, fn)
  dgn.hook_environment(dgn.MAP_GLOBAL_HOOKS, hook_name, fn)
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

-- Given a hook function, returns a single-element table containing the
-- function, viz. { fn }. Given anything else, returns it unchanged.
local function dgn_fixup_hook(hook_function_or_table)
  if not hook_function_or_table then
    return
  end
  if type(hook_function_or_table) == 'function' then
    return { hook_function_or_table }
  end
  assert(type(hook_function_or_table) == 'table',
         "Bad hook value: expected table or function, got "
           .. type(hook_function_or_table))
  return hook_function_or_table
end

function dgn_map_copy_hooks_from(hook_source_mapname, hook_name)
  local source_environment = dgn.map_environment_by_name(hook_source_mapname)
  local hook_variable = dgn_hook_variable_name(hook_name)
  local source_hook = dgn_fixup_hook(source_environment[hook_variable])
  if not source_hook then
    return
  end

  local target_environment = dgn.map_environment()
  local target_hook = dgn_fixup_hook(target_environment[hook_variable])
  if not target_hook then
    target_hook = source_hook
  else
    target_hook = util.append(target_hook, source_hook)
  end
  target_environment[hook_variable] = target_hook
end

function dgn_run_hooks_in_environment(env, hook_name)
  local hook_variable_name = dgn_hook_variable_name(hook_name)
  local hook_functions_table = dgn_fixup_hook(env[hook_variable_name])
  if hook_functions_table then
    for _, hook_function in ipairs(hook_functions_table) do
      dgn_run_map(hook_function)
    end
  end
end

function dgn_map_run_hook(hook_name)
  -- There must be a "current" map that we can run hooks for.
  if not g_dgn_curr_map then
    return
  end

  -- Hooks, if any, will be defined in the map environment, so if there are
  -- no map environments, there's nothing to do.
  if not dgn._map_envs then
    return
  end

  local mapname = dgn.name(g_dgn_curr_map)
  local map_environment = dgn.map_environment_by_name(mapname)
  dgn_run_hooks_in_environment(map_environment, hook_name)
  dgn_run_hooks_in_environment(dgn.MAP_GLOBAL_HOOKS, hook_name)
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
      dgn_init_hook_tables(meta)
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

   -- Convenience global variable, e.g. mapgrd[x][y] = 'x'
   meta['mapgrd'] = dgn.mapgrd_table(map)

   meta['_G'] = meta
   meta.wrapped_instance = map
   return meta
end

-- Discards accumulated map environments.
function dgn_flush_map_environments()
  dgn._map_envs = nil
  dgn.MAP_GLOBAL_HOOKS = { }
  dgn_init_hook_tables(dgn.MAP_GLOBAL_HOOKS)
end

function dgn_flush_map_environment_for(mapname)
  if dgn._map_envs then
    dgn._map_envs[mapname] = nil
  end
end

function dgn_set_map(map)
  local old_map = g_dgn_curr_map
  g_dgn_curr_map = map
  return old_map
end

-- Given a list of map chunk functions, runs each one in order in that
-- map's environment (wrapped with setfenv) and returns the return
-- value of the last chunk. If the caller is interested in the return
-- values of all the chunks, this function may be called multiple
-- times, once for each chunk.
function dgn_run_map(...)
  local map_chunk_functions = { ... }
  if #map_chunk_functions > 0 then
    local ret
    if not g_dgn_curr_map then
      error("No current map?")
    end
    local env = dgn_map_meta_wrap(g_dgn_curr_map, dgn)
    for _, map_chunk_function in ipairs(map_chunk_functions) do
      if map_chunk_function then
        ret = setfenv(map_chunk_function, env)()
      end
    end
    return ret
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
  for k, v in pairs(map) do -- arbitrary iter order should be ok here
    fnmap[dgn.fnum(k)] = dgn.fnum(v)
  end
  return fnmap
end

-- Given a list of feature names, returns a dictionary mapping feature
-- numbers to true.
function dgn.feature_number_set(feature_names)
  local dict = { }
  for _, name in ipairs(feature_names) do
    dict[dgn.fnum(name)] = true
  end
  return dict
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

function dgn.feature_set_fn(...)
  local chosen_features = dgn.feature_number_set({ ... })
  return function (fnum)
           return chosen_features[fnum]
         end
end

-- Finds all points in the map satisfying the supplied predicate.
function dgn.find_points(predicate)
  local points = { }
  for x = 0, dgn.GXM - 1 do
    for y = 0, dgn.GYM - 1 do
      local p = dgn.point(x, y)
      if predicate(p) then
        table.insert(points, p)
      end
    end
  end
  return points
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

function dgn.find_map(parameters)
  local function resolve_parameter(par)
    local partype = type(par)
    if partype == 'table' then
      return resolve_parameter(util.random_from(par))
    elseif partype == 'function' then
      return resolve_parameter(par())
    else
      return par
    end
  end
  if parameters.map then
    return resolve_parameter(parameters.map)
  end

  if parameters.tag then
    return dgn.map_by_tag(resolve_parameter(parameters.tag))
  end

  if parameters.name then
    return dgn.map_by_name(resolve_parameter(parameters.name))
  end

  if parameters.random_mini or parameters.random_vault then
    local mini = parameters.random_mini
    local depth = resolve_parameter(parameters.random_mini
                                    or parameters.random_vault)
    if type(depth) == 'boolean' then
      depth = dgn.level_id()
    end
    return dgn.map_in_depth(depth, mini)
  end
  error("dgn.find_map: no map selection criteria provided")
end

-- Places one or more maps matching the specified parameters. parameters
-- must be a Lua table; these parameter values are relevant:
--   map          - map object to place
--   name         - name of the map to place (not recommended; using a specific
--                  tag is usually better).
--   tag          - tag of the map to place
--   random_mini  - if logical true, picks a random minivault in depth;
--                  if set to a place name, uses that place name to choose
--                  the minivault.
--   random_vault - if logical true, picks a random vault in depth; if
--                  set to a place name, uses that place name to choose the
--                  vault.
--
--   count        - Number of times to repeat the map placement; defaults to 1.
--                  Note that the map must be marked allow_dup.
--   die_on_error - If set false, will *not* raise an error if a map cannot
--                  be placed. Defaults to false.
-- Returns true if the map(s) were placed successfully, false or nil otherwise.
--
-- map, name and tag are mutually exclusive. These parameters may be
-- single-valued, or may be a table. If the value is a table, a random
-- element of the table is used by calling util.random_from(table). If
-- the value is a function, the function is called to get the map.
function dgn.place_maps(parameters)
  local n_times = parameters.count or 1
  local n_placed = 0

  local function map_error(name)
    -- this prefix string is used in map_def::run_hook
    error("Failed to place map '" .. name .. "'")
  end

  for i = 1, n_times do
    local map = dgn.find_map(parameters)
    if map then
      local map_placed_ok = dgn.place_map(map, true, false, nil, nil,
                                          parameters.tag)
      if map_placed_ok then
        n_placed = n_placed + 1
      else
        if not parameters.die_on_error then
          return nil
        end
        map_error(dgn.name(map))
      end
    else
      if not parameters.die_on_error then
        return nil
      end
      error("Could not find map to place")
    end
  end
  return n_placed
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

-- Turn persistent data into a human readable string.
function persist_to_string()
    return table_to_string(dgn.persist)
end

-- List of useful scrolls, with some reasonable weights.
-- When changing the list or the weights, please keep the total weight at 1000.
dgn.good_scrolls = [[
    w:85  scroll of identify no_pickup /
    w:38  scroll of identify no_pickup q:2 /
    w:10  scroll of identify no_pickup q:3 /
    w:85  scroll of teleportation no_pickup /
    w:38  scroll of teleportation no_pickup q:2 /
    w:10  scroll of teleportation no_pickup q:3 /
    w:85  scroll of fog no_pickup /
    w:33  scroll of fog no_pickup q:2 /
    w:85  scroll of remove curse no_pickup /
    w:38  scroll of remove curse no_pickup q:2 /
    w:95  scroll of enchant weapon no_pickup /
    w:38  scroll of enchant weapon no_pickup q:2 /
    w:54  scroll of blinking no_pickup /
    w:22  scroll of blinking no_pickup q:2 /
    w:54  scroll of enchant armour no_pickup /
    w:22  scroll of enchant armour no_pickup q:2 /
    w:33  scroll of magic mapping no_pickup /
    w:11  scroll of magic mapping no_pickup q:2 /
    w:33  scroll of amnesia no_pickup /
    w:11  scroll of amnesia no_pickup q:2 /
    w:33  scroll of holy word no_pickup q:1 /
    w:11  scroll of holy word no_pickup q:2 /
    w:22  scroll of silence no_pickup q:1 /
    w:5   scroll of silence no_pickup q:2 /
    w:11  scroll of acquirement no_pickup q:1 /
    w:4   scroll of acquirement no_pickup q:2 /
    w:1   scroll of acquirement no_pickup q:3 /
    w:11  scroll of brand weapon no_pickup q:1 /
    w:11  scroll of torment no_pickup q:1 /
    w:11  scroll of vulnerability no_pickup
    ]]

-- Some scroll and potions with weights that are used as nice loot. These lists
-- emphasize tactical consumables and permanent character/equipment
-- upgrades--things that are relevant the entire game and for most characters.
-- These lists should be used to supplement the more variable loot types % and
-- * to help ensure that a loot pile has usable items. Each list should total
-- 100 weight.
dgn.loot_scrolls = [[
    w:15  scroll of teleportation /
    w:15  scroll of fog /
    w:15  scroll of fear /
    w:10  scroll of blinking /
    w:10  scroll of summoning /
    w:8   scroll of magic mapping /
    w:10  scroll of enchant weapon /
    w:10  scroll of enchant armour /
    w:5   scroll of brand weapon /
    w:2   scroll of acquirement
    ]]

dgn.loot_potions = [[
    w:15  potion of haste /
    w:15  potion of heal wounds /
    w:10  potion of might /
    w:10  potion of invisibility /
    w:10  potion of stabbing /
    w:10  potion of magic /
    w:10  potion of mutation /
    w:8   potion of cancellation /
    w:5   potion of brilliance /
    w:5   potion of resistance /
    w:2   potion of experience
    ]]

-- Some definitions for all types of auxiliary armour.
dgn.aux_armour = "cloak / scarf / helmet / hat / pair of gloves " ..
    "/ pair of boots"

-- Scarves not influenced by good_item.
dgn.good_aux_armour = "cloak good_item / scarf / helmet good_item " ..
    "/ hat good_item / pair of gloves good_item / pair of boots good_item"

-- Scarves excluded since they can't be randart.
dgn.randart_aux_armour = "cloak randart / helmet randart / hat randart " ..
    "/ pair of gloves randart / pair of boots randart"

-- Returns true if point1 is inside radius(X, point2).
function dgn.point_in_radius(point1, point2, radius)
  return dgn.distance(point1.x, point1.y, point2.x, point2.y) <=
    (radius*radius)+1
end

-- Returns a random and uniform colour for use in vaultioneering
function dgn.random_colour (colours)
  if colours == nil or #colours == 0 then
    colours = {"blue", "green", "cyan", "red", "magenta", "brown", "lightgray",
               "darkgray", "lightblue", "lightgreen", "lightcyan", "lightmagenta",
               "yellow", "white"}
  end

  return util.random_from(colours)
end

function dgn.flood(map, x, y, distance, func)
    local queue = {}
    local queued = {}

    table.insert(queue, { x = x, y = y, distance = distance })

    while #queue > 0 do
        local coord = table.remove(queue, 1)

        local new_distance = func(coord.x, coord.y, coord.distance)

        if new_distance > 0 then
            for dx = -1, 1 do
                for dy = -1, 1 do
                    local new_x, new_y = coord.x + dx, coord.y + dy

                    queued[new_x] = queued[new_x] or {}

                    if (dx ~= 0 or dy ~= 0)
                      and dgn.is_valid_coord(map, { x = new_x, y = new_y })
                      and not queued[new_x][new_y]
                    then
                        table.insert(queue, {
                            x = new_x,
                            y = new_y,
                            distance = new_distance
                        })

                        queued[new_x][new_y] = true
                    end
                end
            end
        end
    end
end
