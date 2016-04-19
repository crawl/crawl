------------------------------------------------------------------------------
-- iter.lua:
-- Iterationeering functions.
------------------------------------------------------------------------------
--
-- Example usage of adjacent_iterator:
--
-- function iter.rats()
--   local i
--   for i in iter.adjacent_iterator() do
--     dgn.create_monster(i.x, i.y, "rat")
--   end
-- end
--
-- The above function will surround the player with rats.
--
------------------------------------------------------------------------------
-- dgn.point iterators.
-- All functions expect dgn.point parameters, and return dgn.points.
------------------------------------------------------------------------------
iter = {}

------------------------------------------------------------------------------
-- generic filters for use with iterators
--   these always need to be used with 'rvi', and any filter that acts on them
--   is assumed to as well (but only return a point)
------------------------------------------------------------------------------

function iter.monster_filter (filter)
  local function filter_fn (point)
    if filter ~= nil then
      if filter(point) == nil then
        return nil
      else
        point = filter(point)
      end
    end

    if not dgn.in_bounds(point.x, point.y) then
      return nil
    end

    return dgn.mons_at(point.x, point.y)
  end
  return filter_fn
end

------------------------------------------------------------------------------
-- rectangle_iterator
------------------------------------------------------------------------------
iter.rectangle_iterator = {}

function iter.rectangle_iterator:_new ()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function iter.rectangle_iterator:new (corner1, corner2, filter, rvi)
  if corner1 == nil or corner2 == nil then
    error("need two corners to a rectangle")
  end

  if corner2.x < corner1.x then
    error("corner2's x less than corner1's; did you swap them?")
  elseif corner2.y < corner1.y then
    error("corner2's y less than corner1's; did you swap them?")
  end

  local mt = iter.rectangle_iterator:_new()
  mt.min_x = corner1.x
  mt.min_y = corner1.y
  mt.max_x = corner2.x
  mt.max_y = corner2.y
  mt.cur_x = corner1.x - 1
  mt.cur_y = corner1.y
  mt.filter = filter or nil
  mt.rvi = rvi or false

  return mt:iter()
end

function iter.rectangle_iterator:next()
  local point = nil
  repeat
    if self.cur_x >= self.max_x then
      self.cur_y = self.cur_y + 1
      self.cur_x = self.min_x
      if self.cur_y > self.max_y then
        point = -1
      else
        point = self:check_filter(dgn.point(self.cur_x, self.cur_y))
        if point == nil then
          point = -2
        end
      end
    else
      self.cur_x = self.cur_x + 1
      point = self:check_filter(dgn.point(self.cur_x, self.cur_y))
      if point == nil then
        point = -2
      end
    end
    if point == -1 then
      return nil
    elseif point == -2 then
      point = nil
    end
  until point

  return point
end

function iter.rectangle_iterator:check_filter(point)
  if self.filter ~= nil then
    if self.filter(point) then
      if self.rvi then
        return self.filter(point)
      else
        return point
      end
    else
      return nil
    end
  else
    return point
  end
end

function iter.rectangle_iterator:iter ()
  return function() return self:next() end, nil, nil
end

function iter.rect_iterator(top_corner, bottom_corner, filter, rvi)
  return iter.rectangle_iterator:new(top_corner, bottom_corner, filter, rvi)
end

function iter.rect_size_iterator(top_corner, size, filter, rvi)
  return iter.rect_iterator(top_corner, top_corner + size - dgn.point(1, 1),
                            filter, rvi)
end

function iter.mons_rect_iterator (top_corner, bottom_corner, filter)
  return iter.rect_iterator(top_corner, bottom_corner, iter.monster_filter(filter), true)
end

function iter.border_iterator(top_corner, bottom_corner, rvi)
  local function check_inner(point)
    if point.x == top_corner.x or point.x == bottom_corner.x or point.y == top_corner.y or point.y == bottom_corner.y then
      return point
    end
    return nil
  end
  return iter.rectangle_iterator:new(top_corner, bottom_corner, check_inner, rvi)
end

-------------------------------------------------------------------------------
-- los_iterator
-------------------------------------------------------------------------------

function iter.los_iterator (ic, filter, center, rvi)
  local y_x, y_y = you.pos()

  if center ~= nil then
    y_x, y_y = center:xy()
  end

  local include_center = ic
  local top_corner = dgn.point(y_x - 8, y_y - 8)
  local bottom_corner = dgn.point(y_x + 8, y_y + 8)

  local function check_los (point)
    local _x, _y = point:xy()
    local npoint = true

    if filter ~= nil then
      if rvi then
        npoint = filter(point)
      end

      if not filter(point) then
        return nil
      end
    end

    if y_x == _x and y_y == _y then
      if rvi and include_center then
        return npoint
      else
        return include_center
      end
    end

    if not you.see_cell(_x, _y) then
      return nil
    end

    if rvi then
      return npoint
    else
      return true
    end
  end

  return iter.rect_iterator(top_corner, bottom_corner, check_los, rvi)
end

function iter.mons_los_iterator (ic, filter, center)
  return iter.los_iterator(ic, iter.monster_filter(filter), center, true)
end

-------------------------------------------------------------------------------
-- adjacent_iterator
-------------------------------------------------------------------------------

function iter.adjacent_iterator (ic, filter, center, rvi)
  local y_x, y_y = you.pos()

  if center ~= nil then
    y_x, y_y = center:xy()
  end

  local top_corner = dgn.point(y_x - 1, y_y - 1)
  local bottom_corner = dgn.point(y_x + 1, y_y + 1)
  local include_center = ic

  local function check_adj (point)
    local _x, _y = point:xy()
    local npoint = point

    if filter ~= nil then
      if rvi then
        npoint = filter(point)
      end

      if not filter(point) then
        return nil
      end
    end

    if y_x == _x and y_y == _y then
      if rvi and include_center then
        return npoint
      else
        return include_center
      end
    end

    if rvi then
      return npoint
    else
      return true
    end
  end

  return iter.rect_iterator(top_corner, bottom_corner, check_adj, rvi)
end

function iter.mons_adjacent_iterator (ic, filter, center)
  return iter.adjacent_iterator(ic, iter.monster_filter(filter), center, true)
end

function iter.adjacent_iterator_to(center, include_center, filter)
  return iter.adjacent_iterator(include_center, filter, center, true)
end

-------------------------------------------------------------------------------
-- Circle_iterator
-------------------------------------------------------------------------------

function iter.circle_iterator (radius, ic, filter, center, rvi)
  if radius == nil then
    error("circle_iterator needs a radius")
  end

  local y_x, y_y = you.pos()

  if center ~= nil then
    y_x, y_y = center:xy()
  end

  local include_center = ic
  local top_corner = dgn.point(y_x - radius, y_y - radius)
  local bottom_corner = dgn.point(y_x + radius, y_y + radius)

  local function check_dist (point)
    local _x, _y = point:xy()
    local dist = dgn.distance(y_x, y_y, _x, _y)
    local npoint = nil

    if filter ~= nil then
      if rvi then
        npoint = filter(point)
      end

      if not filter(point) then
        return nil
      end
    end

    if y_x == _x and y_y == _y then
      if rvi and include_center then
        return npoint
      else
        return include_center
      end
    end

    if dist >= (radius * radius) + 1 then
      return nil
    end

    if rvi then
      return npoint
    else
      return true
    end
  end

  return iter.rect_iterator(top_corner, bottom_corner, check_dist, rvi)
end

function iter.mons_circle_iterator (radius, ic, filter, center)
  return iter.circle_iterator(radius, ic, iter.monster_filter(filter), center, true)
end

-------------------------------------------------------------------------------
-- item stack-related functions
--   not necessarily iterators
-------------------------------------------------------------------------------

function iter.stack_search (coord, term, extra)
  local _x, _y
  if extra ~= nil then
    _x = coord
    _y = term
    term = extra
  elseif coord == nil then
    error("stack_search requires a location")
  else
    _x, _y = coord:xy()
  end

  if term == nil then
    error("stack_search requires a search term")
  end

  local stack = dgn.items_at(_x, _y)
  if #stack == 0 then
    return nil
  end

  for _, item in ipairs(stack) do
    if string.find(item.name(), (term)) then
      return item
    end
  end

  return nil
end

function iter.stack_destroy(coord, extra)
  local _x, _y
  if extra ~= nil then
    _x = coord
    _y = extra
  elseif coord == nil then
    error("stack_search requires a location")
  else
    _x, _y = coord:xy()
  end

  local stack = dgn.items_at(_x, _y)

  while #stack ~= 0 do
    local name = stack[1].name()
    if stack[1].destroy() then
      if #stack <= #dgn.items_at(_x, _y) then
        error("destroyed an item ('" .. name .. "'), but the "
              .. "stack size is the same")
        return
      end
      stack = dgn.items_at(_x, _y)
    else
      error("couldn't destroy item '" .. name .. "'")
      return false
    end
  end
end

-------------------------------------------------------------------------------
-- subvault_iterator
-- Iterates through all map locations in a subvault that will get written
-- back to a parent.
-------------------------------------------------------------------------------
function iter.subvault_iterator (e, filter)

  if e == nil then
    error("subvault_iterator requires the env to be passed, e.g. _G")
  end

  local top_corner = dgn.point(0, 0)
  local bottom_corner

  local w,h = e.subvault_size()
  if w == 0 or h == 0 then
    -- Construct a valid rectangle.  subvault_cell_valid will always fail.
    bottom_corner = top_corner
  else
    bottom_corner = dgn.point(w-1, h-1)
  end


  local function check_mask (point)
    if filter ~= nil then
      if not filter(point) then
        return false
      end
    end

    return e.subvault_cell_valid(point.x, point.y)
  end

  return iter.rect_iterator(top_corner, bottom_corner, check_mask)
end

-------------------------------------------------------------------------------
-- Marker iterators
--   firstly, a point iterator. It's really just a fancy ipairs(), but stateful
--   and with the ability to filter points.
-------------------------------------------------------------------------------

iter.point_iterator = {}

function iter.point_iterator:_new ()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function iter.point_iterator:new (ptable, filter, rv_instead)
  if ptable == nil then
    error("ptable cannot be nil for point_iterator")
  end

  local mt  = iter.point_iterator:_new()
  mt.cur_p  = 0
  mt.table  = ptable
  mt.rvi    = rv_instead or false
  mt.filter = filter or nil

  return mt:iter()
end

function iter.point_iterator:next()
  local point = nil
  local q = 0
  repeat
    q = q + 1
    self.cur_p = self.cur_p + 1
    point = self:check_filter(self.table[self.cur_p])
  until point or q == 10

  return point
end

function iter.point_iterator:check_filter(point)
  if self.filter ~= nil then
    if self.filter(point) then
      if self.rvi then
        return self.filter(point)
      else
        return point
      end
    else
      return nil
    end
  else
    return point
  end
end

function iter.point_iterator:iter ()
  return function() return self:next() end, nil, nil
end

-- An easier and more posh way of interfacing with find_marker_positions_by_prop.
function iter.slave_iterator (prop, value)
  local ptable = dgn.find_marker_positions_by_prop(prop, value)
  if #ptable == 0 then
    error("Didn't find any props for " .. prop .. "=" .. value)
  else
    return iter.point_iterator:new(ptable)
  end
end

-------------------------------------------------------------------------------
-- Inventory iterator
-------------------------------------------------------------------------------

iter.invent_iterator = {}

function iter.invent_iterator:_new ()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function iter.invent_iterator:new (itable, filter, rv_instead)
  if itable == nil then
    error("itable cannot be nil for invent_iterator")
  end

  local mt  = iter.invent_iterator:_new()
  mt.cur_p  = 0
  mt.table  = itable
  mt.rvi    = rv_instead or false
  mt.filter = filter or nil

  return mt:iter()
end

function iter.invent_iterator:next()
  local point = nil
  local q = 0
  repeat
    q = q + 1
    self.cur_p = self.cur_p + 1
    point = self:check_filter(self.table[self.cur_p])
  until point or q == 10

  return point
end

function iter.invent_iterator:check_filter(item)
  if self.filter ~= nil then
    if self.filter(item) then
      if self.rvi then
        return self.filter(item)
      else
        return item
      end
    else
      return nil
    end
  else
    return item
  end
end

function iter.invent_iterator:iter ()
  return function() return self:next() end, nil, nil
end

-- An easier and more posh way of interfacing with inventory.
function iter.inventory_iterator ()
  return iter.invent_iterator:new(items.inventory())
end

function iter.stash_iterator (point, y)
  if y == nil then
    return iter.invent_iterator:new(dgn.items_at(point.x, point.y))
  else
    return iter.invent_iterator:new(dgn.items_at(point, y))
  end
end
