------------------------------------------------------------------------------
-- util.lua
-- Lua utilities.
------------------------------------------------------------------------------

util = { }

function util.defclass(name)
  local cls = { CLASS = name }
  cls.__index = cls
  _G[name] = cls
  return cls
end

function util.subclass(parent, subclassname)
  -- parent should have no-arg constructor.
  local subclass = parent:new()
  subclass.super = parent
  -- Not strictly necessary - parent constructor should do this.
  subclass.__index = subclass
  if subclassname then
    subclass.CLASS = subclassname
    _G[subclassname] = subclass
  end
  return subclass
end

function util.newinstance(class)
  local instance = { }
  setmetatable(instance, class)
  return instance
end

function util.callable(x)
  return type(x) == 'function' or (type(x) == 'table'
                                   and getmetatable(x)
                                   and getmetatable(x).__call)
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
  for key, _ in pairs(map) do
    table.insert(keys, key)
  end
  return keys
end

-- Returns a list of the values in the given map.
function util.values(map)
  local values = { }
  for _, value in pairs(map) do
    table.insert(values, value)
  end
  return values
end

-- Returns a list of lists built from the given map, each sublist being
-- in the form { key, value } for each key-value pair in the map.
function util.pairs(map)
  local mappairs = { }
  for key, value in pairs(map) do
    table.insert(mappairs, { key, value })
  end
  return mappairs
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
function util.append(first, ...)
  local res = first
  local tables = { ... }
  for _, tab in ipairs(tables) do
    for _, val in ipairs(tab) do
      table.insert(res, val)
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

function util.indexof(list, item)
  for _, value in ipairs(list) do
    if value == item then return _ end
  end
  return -1
end

function util.remove(list, item)
  local index = util.indexof(list,item)
  if index>-1 then
    table.remove(list,index)
  end
end

function util.contains(haystack, needle)
  for _, value in ipairs(haystack) do
    if value == needle then
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
    local weightkey = weightfn
    weightfn = function (table)
                 return table[weightkey]
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

function util.random_weighted_keys(weightfn, list, order)
  if type(weightfn) ~= "function" then
    local weightkey = weightfn
    weightfn = function (table)
                 return table[weightkey]
               end
  end
  local cweight = 0
  local chosen = nil
  local keys = {}
  for k in pairs(list) do
    keys[#keys+1] = k
  end
  if order then
    table.sort(keys, order)
  else
    table.sort(keys)
  end
  for i,k in ipairs(keys) do
    v = list[k]
    local wt = weightfn(k,v) or 10
    cweight = cweight + wt
    if crawl.random2(cweight) < wt then
      chosen = v
    end
  end
  return chosen
end

-- Given a table of elements, choose a subset of size n without replacement.
function util.random_subset(set, n)
   local result = {}
   local cmap = {}
   for i=1,n,1 do
       local vals = {}
       local indices = {}
       local count = 0
       for j,v in ipairs(set) do
           if not cmap[j] then
               indices[count + 1] = j
               vals[count + 1] = v
               count = count + 1
           end
       end
       if count == 0 then
           break
       end
       local ind = crawl.random2(count) + 1
       table.insert(result, vals[ind])
       cmap[indices[ind]] = true
   end
   return result
end

-- Uses a predefined weight function allowing list items
-- to be weighted according to branch
function util.random_branch_weighted(list)
  local branch = you.branch()
  local weightfn = function(item)
    -- Check for "branch" element to limit this item to a given branch
    if item.branch ~= nil and item.branch ~= branch then return 0 end;
    -- Check for an entry with the name of the branch and use that if exists
    if item[branch] ~= nil then return item[branch] end
    -- Otherwise default 'weight' element
    return item.weight == nil and 10 or item.weight
  end
  return util.random_weighted_from(weightfn,list)
end

function util.random_range_real(lower,upper)
  return lower + crawl.random_real() * (upper-lower)
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

function util.range(start, stop)
  local rt
  for i = start, stop do
    table.insert(rt, i)
  end
  return rt
end

-- From http://lua-users.org/wiki/CommonFunctions
function util.trim(s)
  return (s:gsub("^%s*(.-)%s*$", "%1"))
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

-- Turn contents of a table into a human readable string.
-- Used (indirectly) by dbg-asrt.cc:do_crash_dump().
-- TODO: Maybe replace this with http://www.hpelbers.org/lua/print_r ?
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
    elseif typ == "number" or typ == "string" then
      str = str .. value
    elseif typ == "boolean" then
      str = str .. tostring(value)
    else
      str = str .. "[type " .. typ .. "]"
    end
    str = str .. "\n"
  end

  return str
end

-- Copied from http://lua-users.org/wiki/CopyTable
function util.copy_table(object)
  local lookup_table = {}
  local function _copy(object)
    if type(object) ~= "table" then
      return object
    elseif lookup_table[object] then
      return lookup_table[object]
    end
    local new_table = {}
    lookup_table[object] = new_table
    for index, value in pairs(object) do
      new_table[_copy(index)] = _copy(value)
    end
    return setmetatable(new_table, getmetatable(object))
  end
  return _copy(object)
end

-- Initialises a namespace that has functions spread across multiple files.
-- If the namespace table does not exist, it is created. If it already exists,
-- it is not modified.
function util.namespace(table_name)
  if _G[table_name] == nil then
    _G[table_name] = { }
  end
end
