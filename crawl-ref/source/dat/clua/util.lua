------------------------------------------------------------------------------
-- util.lua
-- Lua utilities.
------------------------------------------------------------------------------

util = { }

function util.subclass(parent)
  -- parent should have no-arg constructor.
  local subclass = parent:new()
  subclass.super = parent
  -- Not strictly necessary - parent constructor should do this.
  subclass.__index = subclass
  return subclass
end

function util.catlist(...)
  local res = { }
  local tables = { ... }
  if #tables == 1 then
    return tables[1]
  else
    for _, tab in ipairs(tables) do
      for _, val in ipairs(tab) do
        table.insert(res, val)
      end
    end
  end
  return res
end

function util.cathash(...)
  local res = { }
  local tables = { ... }
  if #tables == 1 then
    return tables[1]
  else
    for _, tab in ipairs(tables) do
      for key, val in ipairs(tab) do
        res[key] = val
      end
    end
  end
  return res
end

function util.map(fn, ...)
  local lists = { ... }
  local res = { }
  if #lists == 0 then
    return res
  elseif #lists == 1 then
    for _, val in ipairs(lists[1]) do
      table.insert(res, fn(val))
    end
  else
    for i = 1, #lists[1] do
      local args = { }
      for _, list in ipairs(lists) do
        if not list[i] then
          break
        end
        table.insert(args, list[i])
      end
      if #args < #lists then
        break
      end
      table.insert(res, fn(unpack(args)))
    end
  end
  return res
end

function util.forall(list, pred)
  for _, value in ipairs(list) do
    if not pred(value) then
      return false
    end
  end
  return true
end

function util.exists(list, pred)
  for _, value in ipairs(list) do
    if pred(value) then
      return true
    end
  end
  return false
end

function util.random_from(list)
  return list[ crawl.random2(#list) + 1 ]
end