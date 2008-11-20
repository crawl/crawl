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

function zig()
  if not dgn.persist.ziggurat then
    dgn.persist.ziggurat = { }
  end
  return dgn.persist.ziggurat
end

-- Returns a function that changes the depth in the ziggurat to the depth
-- specified.
local function zig_depth_increment()
  return function (...)
           zig().depth = zig().depth + 1
         end
end

function ziggurat_initializer()
  local z = zig()
  z.depth = 1
  z.builder = ziggurat_choose_builder()
end

-- Returns the current depth in the ziggurat.
local function zig_depth()
  return zig().depth or 0
end

-- Common setup for ziggurat entry vaults.
function ziggurat_portal(e)
  local function stair()
    return one_way_stair {
      desc = "gateway to a ziggurat",
      dst = "ziggurat",
      floor = "stone_arch",
      onclimb = ziggurat_initializer
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
  ziggurat_build_level(e)
end

-----------------------------------------------------------------------------
-- Ziggurat level builders.

function ziggurat_build_level(e)
  local builder = zig().builder
  if builder then
    return ziggurat_builder_map[builder](e)
  end
end

local function zigstair(x, y, stair, marker)
  dgn.grid(x, y, stair)
  if marker then
    local t = type(marker)
    if t == "function" or t == "table" then
      dgn.register_lua_marker(x, y, marker)
    else
      dgn.register_feature_marker(x, y, marker)
    end
  end
end

-- Creates a Lua marker table that increments ziggurat depth.
local function zig_go_deeper()
  return one_way_stair {
    onclimb = zig_depth_increment()
  }
end

local function ziggurat_rectangle_builder(e)
  local grid = dgn.grid

  -- FIXME: Cool code goes here.
  local x1, y1 = 20, 20
  local x2, y2 = dgn.GXM - 20, dgn.GYM - 20
  dgn.fill_area(x1, y1, x2, y2, "floor")

  local my = math.floor((y1 + y2) / 2)
  zigstair(x1, my, "stone_arch", "stone_stairs_up_i")
  zigstair(x2, my, "stone_stairs_down_i", zig_go_deeper)
  grid(x2, my + 1, "exit_portal_vault")

  crawl.mpr("Ziggurat depth is now " .. zig_depth())
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
  return ziggurat_builders[crawl.random_range(1, #ziggurat_builders)]
end