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
-- rectangle_iterator
------------------------------------------------------------------------------
iter.rectangle_iterator = {}

function iter.rectangle_iterator:_new ()
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function iter.rectangle_iterator:new (corner1, corner2, filter)
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
        point = dgn.point(self.cur_x, self.cur_y)
        if not self:check_filter(point) then
          point = -2
        end
      end
    else
      self.cur_x = self.cur_x + 1
      point = dgn.point(self.cur_x, self.cur_y)
      if not self:check_filter(point) then
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
      return true
    else
      return false
    end
  else
    return true
  end
end

function iter.rectangle_iterator:iter ()
  return function() return self:next() end, nil, nil
end

function iter.rect_iterator(top_corner, bottom_corner, filter)
  return iter.rectangle_iterator:new(top_corner, bottom_corner, filter)
end

-------------------------------------------------------------------------------
-- los_iterator
-------------------------------------------------------------------------------

function iter.los_iterator (ic, filter, center)
  local y_x, y_y = you.pos()

  if center ~= nil then
    y_x, y_y = center:xy()
  end

  local include_center = ic or false
  local top_corner = dgn.point(y_x - 8, y_y - 8)
  local bottom_corner = dgn.point(y_x + 8, y_y + 8)

  local function check_los (point)
    local _x, _y = point:xy()

    if filter ~= nil then
      if not filter(point) then
        return false
      end
    end

    if y_x == _x and y_y == y then
      return include_center
    end

    if not you.see_cell(_x, _y) then
      return false
    end

    return true
  end

  return iter.rect_iterator(top_corner, bottom_corner, check_los)
end

-------------------------------------------------------------------------------
-- adjacent_iterator
-------------------------------------------------------------------------------

function iter.adjacent_iterator (ic, filter, center)
  local y_x, y_y = you.pos()

  if center ~= nil then
    y_x, y_y = center:xy()
  end

  local top_corner = dgn.point(y_x - 1, y_y - 1)
  local bottom_corner = dgn.point(y_x + 1, y_y + 1)
  local include_center = ic or false

  local function check_adj (point)
    local _x, _y = point:xy()

    if filter ~= nil then
      if not filter(point) then
        return false
      end
    end

    if y_x == _x and y_y == y then
      return include_center
    end

    return true
  end

  return iter.rect_iterator(top_corner, bottom_corner, check_adj)
end

-------------------------------------------------------------------------------
-- circle_iterator
-------------------------------------------------------------------------------

function iter.circle_iterator (radius, ic, filter, center)
  if radius == nil then
    error("circle_iterator needs a radius")
  end

  local y_x, y_y = you.pos()

  if center ~= nil then
    y_x, y_y = center:xy()
  end

  local include_center = ic or false
  local top_corner = dgn.point(y_x - radius, y_y - radius)
  local bottom_corner = dgn.point(y_x + radius, y_y + radius)

  local function check_dist (point)
    local _x, _y = point:xy()
    local dist = dgn.distance(y_x, y_y, _x, _y)

    if filter ~= nil then
      if not filter(point) then
        return false
      end
    end

    if y_x == _x and y_y == y then
      return include_center
    end

    if dist >= (radius * radius) + 1 then
      return false
    end

    return true
  end

  return iter.rect_iterator(top_corner, bottom_corner, check_dist)
end
