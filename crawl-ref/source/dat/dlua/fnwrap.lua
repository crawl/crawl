-------------------------------------------------------------------------
-- Wraps a global function in a table that serializes by name
-- and delegates to the supplied global
-------------------------------------------------------------------------

crawl_require('dlua/util.lua')
util.defclass("FunctionWrapper")

local global_function_name_cache = { }

function FunctionWrapper.__call(fnwrapper, ...)
  local fn = fnwrapper.fn
  return fn(...)
end

local function resolve_function(global_name)
  local cached_lookup = global_function_name_cache[global_name]
  if cached_lookup then
    return cached_lookup
  end

  local resolved = _G
  local pieces = crawl.split(global_name, ".")
  for _, piece in ipairs(pieces) do
    if not resolved or type(resolved) ~= 'table' then
      break
    end
    resolved = resolved[piece]
  end
  assert(resolved and type(resolved) == 'function',
         "Could not resolve '" .. global_name .. "' to a function")
  global_function_name_cache[global_name] = resolved
  return resolved
end

function FunctionWrapper:new(fn_name)
  local fwrap = util.newinstance(self)
  fwrap.name = fn_name
  fwrap.fn = resolve_function(fn_name)
  return fwrap
end

function FunctionWrapper:marshall(th)
  file.marshall_meta(th, self.name)
end

function FunctionWrapper:unmarshall(th)
  local name = file.unmarshall_meta(th)
  return self:new(name)
end

-- Given the name of a globally visible function, returns a
-- FunctionWrapper object that proxies to that function and that can
-- be saved and reloaded by name.
--
-- If given a FunctionWrapper object directly, global_function()
-- returns the FunctionWrapper unchanged. This makes it a convenient
-- passthrough if you're unsure if the parameter you have is wrapped
-- or not.
--
-- If given a description parameter, global_function() uses the
-- description when producing errors for invalid (non-function,
-- non-FunctionWrapper input).
--
function global_function(fn_name, description)
  if getmetatable(fn_name) == FunctionWrapper then
    return fn_name
  elseif type(fn_name) == 'string' then
    return FunctionWrapper:new(fn_name)
  else
    if description then
      error("Expected global function name for " .. description ..
            ", got " .. type(fn_name) .. ":" .. debug.traceback())
    else
      error("Expected global function name, got " .. type(fn_name)
            .. ":" .. debug.traceback())
    end
  end
end
