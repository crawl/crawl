-- persist.lua
-- Functions to persist the lua table c_persist

-- We want to load this file immediately before calling c_save_persist() to
-- ensure that the functions haven't been overwritten by player versions.

function stringify(x)
    local t = type(x)
    if t == "nil" then
        return "nil"
    elseif t == "number" then
        return tostring(x)
    elseif t == "string" then
        return string.format("%q", x)
    elseif t == "boolean" then
        return x and "true" or "false"
    else
        error("Cannot stringify objects of type " .. t .. debug.traceback())
    end
end

function stringify_table(tab,indent_level)
  if not indent_level then
    indent_level = 0
  end
  local spaces = ""
  for i = 1,2 * indent_level + 1 do
    spaces = spaces .. " "
  end
  local res = spaces .. "{\n"
  for key,val in pairs(tab) do
    res = res .. spaces .. " [" .. stringify(key) .. "] ="
    if type(val) ~= "table" then
      res = res .. " " .. stringify(val) .. ",\n"
    elseif next(val) == nil then -- table is empty
      res = res .. " { },\n"
    else
      res = res .. "\n" .. stringify_table(val,indent_level + 1) .. ",\n"
    end
  end
  res = res .. spaces .. "}"
  return res
end

function c_save_persist()
    if next(c_persist) == nil then -- table is empty
      return ""
    end
    str = stringify_table(c_persist)
    return "c_persist =\n" .. str
end
