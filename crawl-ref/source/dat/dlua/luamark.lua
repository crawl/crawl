------------------------------------------------------------------------------
-- luamark.lua:
-- Lua map marker handling.
------------------------------------------------------------------------------

require('dlua/fnwrap.lua')

-- Every marker class must register its class name keyed to its reader
-- function in this table.
MARKER_UNMARSHALL_TABLE = { }

function dlua_register_marker_table(table, reader)
  if not reader then
    reader = table.read
  end
  MARKER_UNMARSHALL_TABLE[table.CLASS] = reader
end

require('dlua/lm_trig.lua')
require('dlua/lm_pdesc.lua')
require('dlua/lm_1way.lua')
require('dlua/lm_timed.lua')
require('dlua/lm_toll.lua') -- remove upon TAG_MAJOR_VERSION bump
require('dlua/lm_fog.lua')
require('dlua/lm_props.lua')
require('dlua/lm_mon_prop.lua')
require('dlua/lm_monst.lua')
require('dlua/lm_trove.lua')
require('dlua/lm_door.lua')
require('dlua/lm_items.lua')
require('dlua/lm_named_hatch.lua')
require('dlua/fnwrap.lua')
require('dlua/lm_trans.lua')

function dlua_marker_reader_name(table)
  -- Check that the reader is actually registered for this table.
  dlua_marker_reader_fn(table)
  return table.CLASS
end

function dlua_marker_function(table, name)
  return table[name]
end

function dlua_marker_method(table, name, marker, ...)
  if table[name] then
    return table[name](table, marker, ...)
  end
end

function dlua_marker_reader_fn(table)
  assert(table.CLASS, "Marker table has no CLASS property")
  local reader = MARKER_UNMARSHALL_TABLE[table.CLASS]
  if not reader then
    assert(table.read, "Marker table (" .. table.CLASS .. ") has no " ..
           "registered reader and no .read method.")
    return table.read
  end
  return reader
end

function dlua_marker_read(marker_class_name, marker_userdata, th)
  local reader_fn =
    MARKER_UNMARSHALL_TABLE[marker_class_name] or _G[marker_class_name].read
  return reader_fn({ }, marker_userdata, th)
end

util.namespace('lmark')

local FNWRAP_TABLE_KEY = -2

function lmark.marshall_marker(th, marker)
  assert(marker.CLASS, "Marker does not have CLASS attribute!")
  file.marshall_meta(th, marker.CLASS)
  marker:write(th)
end

function lmark.unmarshall_marker(th)
  local marker_class = file.unmarshall_meta(th)
  return dlua_marker_read(marker_class, nil, th)
end

-- Marshalls a table comprising of keys that are strings or numbers only,
-- and values that are strings, numbers, functions, or tables only. The table
-- cannot have cycles, and the table's metatable is not preserved.
function lmark.marshall_table(th, table)
  if not table then
    file.marshall(th, -1)
    return
  end

  if getmetatable(table) == FunctionWrapper then
    file.marshall(th, FNWRAP_TABLE_KEY)
    table:marshall(th)
    return
  end

  -- Count the number of elements first (ugh)
  local nsize = 0
  local tsize = 0
  for _, v in pairs(table) do
    if type(v) == 'table' then
      tsize = tsize + 1
    else
      nsize = nsize + 1
    end
  end

  file.marshall(th, nsize)
  for key, value in pairs(table) do
    if type(value) ~= 'table' then
      if type(value) == 'function' then
        error("Cannot marshall function in key: " .. key ..
              ":" .. debug.traceback())
      end
      file.marshall_meta(th, key)
      file.marshall_meta(th, value)
    end
  end

  file.marshall(th, tsize)
  for key, value in pairs(table) do
    if type(value) == 'table' then
      file.marshall_meta(th, key)
      lmark.marshall_table(th, value)
    end
  end
end

-- Unmarshals a table marshaled by marshall_table.
function lmark.unmarshall_table(th)
  local nsize = file.unmarshall_number(th)

  if nsize == -1 then
    return nil
  end

  if nsize == FNWRAP_TABLE_KEY then
    return FunctionWrapper:unmarshall(th)
  end

  local ret = { }
  for i = 1, nsize do
    local key = file.unmarshall_meta(th)
    local val = file.unmarshall_meta(th)
    ret[key] = val
  end

  local tsize = file.unmarshall_number(th)
  for i = 1, tsize do
    local key = file.unmarshall_meta(th)
    local val = lmark.unmarshall_table(th)
    ret[key] = val
  end
  return ret
end
