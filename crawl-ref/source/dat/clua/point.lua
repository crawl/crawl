--------------------------------------------------------------------------
-- point.lua
--------------------------------------------------------------------------

local point_metatable = { }

function dgn.point(x, y)
  local pt = { x = x, y = y }
  setmetatable(pt, point_metatable)
  return pt
end

point_metatable.__add = function (a, b)
                          if type(b) == "number" then
                            return dgn.point(a.x + b, a.y + b)
                          else
                            return dgn.point(a.x + b.x, a.y + b.y)
                          end
                        end

point_metatable.__sub = function (a, b)
                          if type(b) == "number" then
                            return dgn.point(a.x - b, a.y - b)
                          else
                            return dgn.point(a.x - b.x, a.y - b.y)
                          end
                        end

point_metatable.__div = function (a, b)
                          if type(b) ~= "number" then
                            error("Can only divide by numbers.")
                          end
                          return dgn.point(math.floor(a.x / b),
                                           math.floor(a.y / b))
                        end

point_metatable.__mul = function (a, b)
                          if type(b) ~= "number" then
                            error("Can only multiply by numbers.")
                          end
                          return dgn.point(a.x * b, a.y * b)
                        end

point_metatable.__unm = function (a)
                          return dgn.point(-a.x, -a.y)
                        end

point_metatable.__concat = function (pre, post)
                             local function pstr(p)
                               return "(" .. p.x .. "," .. p.y .. ")"
                             end
                             if getmetatable(pre) == point_metatable then
                               return pstr(pre) .. post
                             else
                               return pre .. pstr(post)
                             end
                           end
