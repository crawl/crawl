------------------------------------------------------------------------------
-- luamark.lua:
-- Lua map marker handling.
------------------------------------------------------------------------------

require('clua/lm_trig.lua')

require('clua/lm_pdesc.lua')
require('clua/lm_1way.lua')
require('clua/lm_timed.lua')
require('clua/lm_flags.lua')
require('clua/lm_fog.lua')
require('clua/lm_props.lua')
require('clua/lm_monst.lua')

function dlua_marker_function(table, name)
  return table[name]
end

function dlua_marker_method(table, name, marker, ...)
  if table[name] then
    return table[name](table, marker, ...)
  end
end

function dlua_marker_read(fn, marker, th)
  return fn({ }, marker, th)
end

lmark = { }

-- Marshalls a table comprising of keys that are strings or numbers only,
-- and values that are strings, numbers, functions, or tables only. The table
-- cannot have cycles, and the table's metatable is not preserved.
function lmark.marshall_table(th, table)
  if not table then
    file.marshall(th, -1)
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
