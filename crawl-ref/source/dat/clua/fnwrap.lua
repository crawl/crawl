-------------------------------------------------------------------------
-- Wraps a global function in a table that serializes by name
-- and delegates to the supplied global
-------------------------------------------------------------------------

require('clua/util.lua')
util.defclass("FunctionWrapper")

local global_function_map = { }

function FunctionWrapper.__call(fnwrapper, ...)
  local fn = fnwrapper.fn
  return fn(...)
end

local function resolve_function(global_name)
  local pieces = crawl.split(global_name, ".")
  local resolved = _G
  for _, piece in ipairs(pieces) do
    if not resolved or type(resolved) ~= 'table' then
      break
    end
    resolved = resolved[piece]
  end
  assert(resolved and type(resolved) == 'function',
         "Could not resolve '" .. global_name .. "' to a function")
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

function global_function(fn_name)
  return FunctionWrapper:new(fn_name)
end