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

function dump_usage_grid_v3(usage_grid)
  local gxm,gym = usage_grid.width,usage_grid.height

  for i = 0, gym-1, 1 do
    local maprow = ""
    for j = 0, gxm-1, 1 do
      local cell = usage_grid[i][j]
      if cell.buffer then maprow = maprow .. ":"
      elseif cell.space then maprow = maprow .. " "
      elseif cell.vault then maprow = maprow .. "v"
      elseif cell.exit then maprow = maprow .. "@"
      elseif cell.carvable then maprow = maprow .. "+"
      elseif cell.restricted then maprow = maprow .. "!"
      elseif cell.wall then maprow = maprow .. "X"
      elseif cell.solid then maprow = maprow .. "x"
      else maprow = maprow .. "." end
    end
    print (maprow)
  end
end

function dump_usage_grid(usage_grid)

  local gxm,gym = usage_grid.width,usage_grid.height

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

function dump_usage_grid_pretty(usage_grid)

  local gxm,gym = usage_grid.width,usage_grid.height

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
