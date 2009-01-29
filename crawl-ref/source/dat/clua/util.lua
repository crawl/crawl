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

function util.identity(x)
  return x
end

-- Returns the sublist of elements at indices [istart, iend) of the
-- supplied list.
function util.slice(list, istart, iend)
  if not iend then
    iend = #list + 1
  end

  local res = { }
  for i = istart, iend - 1 do
    table.insert(res, list[i])
  end
  return res
end

function util.partition(list, slice, increment)
  local res = { }
  for i = 1, #list, increment or slice do
    table.insert(res, util.slice(list, i, i + slice))
  end
  return res
end

function util.curry(fn, ...)
  local params = { ... }
  if #params == 1 then
    return function (...)
             return fn(params[1], ...)
           end
  else
    return function (...)
             return fn(unpack(util.catlist(params, ...)))
           end
  end
end

-- Returns a list of the keys in the given map.
function util.keys(map)
  local keys = { }
  for key, _ in pairs(ziggurat_builder_map) do
    table.insert(keys, key)
  end
  return keys
end

-- Returns a list of the values in the given map.
function util.values(map)
  local values = { }
  for _, value in pairs(ziggurat_builder_map) do
    table.insert(values, value)
  end
  return values
end

-- Creates a string of the elements in list joined by separator.
function util.join(sep, list)
  return table.concat(list, sep)
end

-- Creates a set (a map of keys to true) from the list supplied.
function util.set(list)
  local set = { }
  for _, value in ipairs(list) do
    set[value] = true
  end
  return set
end

-- Appends the elements in any number of additional tables to the first table.
function util.append(table, ...)
  local res = table
  local tables = { ... }
  if #tables == 0 then
    return res
  elseif #tables == 1 and #res == 0 then
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

function util.catlist(...)
  return util.append({ }, ...)
end

function util.cathash(...)
  local res = { }
  local tables = { ... }
  if #tables == 1 then
    return tables[1]
  else
    for _, tab in ipairs(tables) do
      for key, val in pairs(tab) do
        res[key] = val
      end
    end
  end
  return res
end

function util.foreach(list, fn)
  for _, val in ipairs(list) do
    fn(val)
  end
end

-- Classic map, but discards nil values (table.insert doesn't like nil).
function util.map(fn, ...)
  local lists = { ... }
  local res = { }
  if #lists == 0 then
    return res
  elseif #lists == 1 then
    for _, val in ipairs(lists[1]) do
      local nval = fn(val)
      if nval ~= nil then
        table.insert(res, nval)
      end
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
      local nval = fn(unpack(args))
      if nval ~= nil then
        table.insert(res, nval)
      end
    end
  end
  return res
end

function util.filter(fn, list)
  local res = { }
  for _, val in ipairs(list) do
    if fn(val) then
      table.insert(res, val)
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

function util.random_weighted_from(weightfn, list)
  if type(weightfn) ~= "function" then
    weightfn = function (table)
                 return table[weightfn]
               end
  end
  local cweight = 0
  local chosen = nil
  util.foreach(list,
               function (e)
                 local wt = weightfn(e) or 10
                 cweight = cweight + wt
                 if crawl.random2(cweight) < wt then
                   chosen = e
                 end
               end)
  return chosen
end

function util.expand_entity(entity, msg)
  if not entity or not msg then
    return msg
  end

  local msg_a =  string.gsub(msg, "$F%{(%w+)%}",
                             function (desc)
                               return crawl.grammar(entity, desc)
                             end)
  return string.gsub(msg_a, "$F",
                     function ()
                       return crawl.grammar(entity, 'a')
                     end)
end

----------------------------------------------------------

util.Timer = { CLASS = "Timer" }
util.Timer.__index = util.Timer

function util.Timer:new(pars)
  self.__index = self
  local timer = pars or { }
  setmetatable(timer, self)
  timer:init()
  return timer
end

function util.Timer:init()
  self.epoch = crawl.millis()
end

function util.Timer:mark(what)
  local last = self.last or self.epoch
  local now = crawl.millis()
  crawl.mpr(what .. ": " .. (now - last) .. " ms")
  self.last = now
end

-- Turn contents of a table into a human readable string
function table_to_string(table, depth)
  depth = depth or 0

  local indent = string.rep(" ", depth * 4)

  if type(table) ~= "table" then
    return indent .. "['" .. type(table) .. "', not a table]"
  end

  local str = ""

  local meta = getmetatable(table)

  if meta and meta.CLASS then
    str = str .. indent .. "CLASS: "
    if type (meta.CLASS) == "string" then
      str = str .. meta.CLASS .. "\n"
    else
      str = str .. "[type " .. type(meta.CLASS) .. "]\n"
    end
  end

  for key, value in pairs(table) do
    local typ = type(key)
    if typ == "string" or typ == "number" then
      str = str .. indent .. key .. ": "
    else
      str = str .. indent .. "[type " .. typ .. "]: "
    end

    typ = type(value)
    if typ == "table" then
      str = str .. "\n" .. table_to_string(value, depth + 1)
    elseif typ == "number" or typ == "string" or typ == "boolen" then
      str = str .. value
    else
      str = str .. "[type " .. typ .. "]"
    end
    str = str .. "\n"
  end

  return str
end
