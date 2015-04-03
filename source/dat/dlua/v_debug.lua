------------------------------------------------------------------------------
-- v_debug.lua:
--
-- When things go wrong ...
------------------------------------------------------------------------------
function print_vector(caption,v)
  if v == nil then
   print(caption .. ": nil")
  else
    print(caption .. ": " .. v.x .. ", " .. v.y)
  end
end

function dump_layout_grid(layout_grid)
    local gxm,gym = layout_grid.width,layout_grid.height

    if gxm==nil or gym==nil then gxm,gym = dgn.max_bounds() end

  for i = 0, gym-1, 1 do
    local maprow = ""
    for j = 0, gxm-1, 1 do
      local cell = layout_grid[i][j]
      if cell.space then maprow = maprow .. " "
      elseif cell.solid == false then maprow = maprow .. "."
      elseif cell.solid == true then maprow = maprow .. "x"
      else maprow = maprow .. "?" end
    end
    print (maprow)
  end
end

function dump_usage_grid(usage_grid)

    local gxm,gym = dgn.max_bounds()

  for i = 0, gym-1, 1 do
    local maprow = ""
    for j = 0, gxm-1, 1 do
      local cell = usage_grid[i][j]
      if cell.usage == "none" then maprow = maprow .. " "
      elseif cell.usage == "restricted" then
        if cell.reason == "vault" then maprow = maprow .. ","
        elseif cell.reason == "door" then maprow = maprow .. "="
        else maprow = maprow .. "!" end
      elseif cell.usage == "empty" then maprow = maprow .. "."
      elseif cell.usage == "eligible" or cell.usage == "eligible_open" then
        if cell.normal == nil then maprow = maprow .. "!"
        elseif cell.normal.x == -1 then maprow = maprow .. "<"
        elseif cell.normal.x == 1 then maprow = maprow .. ">"
        elseif cell.normal.y == -1 then maprow = maprow .. "^"
        elseif cell.normal.y == 1 then maprow = maprow .. "v" end
      elseif cell.usage == "open" then maprow = maprow .. "_"  -- Easier to see the starting room
      else maprow = maprow .. "?" end
    end
    print (maprow)
  end

end

function dump_mask_grid(room)

  for y = -1, room.size.y, 1 do
    maprow = ""
    for x = -1, room.size.x, 1 do
      cell = room.mask_grid[y][x]
      local char = " "
      if cell.wall then
        if cell.dir == 0 then char = "^"
        elseif cell.dir == 1 then char = "<"
        elseif cell.dir == 2 then char = "v"
        elseif cell.dir == 3 then char = ">"
        elseif cell.dir == nil then char = "x"
        else char = "?" end
      end
      maprow = maprow .. char
    end
    print(maprow)
  end
end

function dump_usage_grid_pretty(usage_grid)

    local gxm,gym = dgn.max_bounds()

  for i = 0, gym-1, 1 do
    local maprow = ""
    for j = 0, gxm-1, 1 do
      local cell = usage_grid[i][j]
      if cell.usage == "none" then maprow = maprow .. " "
      elseif cell.usage == "restricted" then
        if cell.reason == "vault" then maprow = maprow .. ","
        elseif cell.reason == "door" then maprow = maprow .. "+"
        else maprow = maprow .. "." end
      elseif cell.usage == "empty" then maprow = maprow .. "."
      elseif cell.usage == "eligible" or cell.usage == "eligible_open" then
        if cell.normal == nil then maprow = maprow .. "!"
        elseif cell.normal.x == 0 then maprow = maprow .. "-"
        elseif cell.normal.y == 0 then maprow = maprow .. "|" end
      elseif cell.usage == "open" then maprow = maprow .. "."
      else maprow = maprow .. "?" end
    end
    print (maprow)
  end

end
