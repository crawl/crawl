------------------------------------------------------------------------------
-- profiler.lua
-- A very simple Lua profiler
------------------------------------------------------------------------------

profiler = {}

function profiler.print_time(label,millis,data)
  local time = label .. ": " .. (millis/1000) .. "secs"
  if data ~= nil then
    time = time .. " ( "
    local first = true
    for k,v in pairs(data) do
      if not first then time = time .. ", " end
      time = time .. k .. " = '" .. v .. "'"
      first = false
    end
    time = time .. " )"
  end
  print(time)
end

-- Begins profiling
function profiler.start(name)
  profiler.name = name
  profiler.stack = {}
  profiler.done = {}
  profiler.top = profiler.done
  profiler.start_time = crawl.millis()
  profiler.level = 0
end

function profiler.push(name,data)
  local now = crawl.millis()
  local op = { name = name, data = data, start = now, level = profiler.level, children = {} }
  table.insert(profiler.stack,op)
  profiler.top = op.children
  profiler.level = profiler.level + 1
end

function profiler.pop()
  local op = profiler.stack[#(profiler.stack)]
  table.remove(profiler.stack,#(profiler.stack))
  if #(profiler.stack) > 0 then
    profiler.top = profiler.stack[#(profiler.stack)].children
  else
    profiler.top = profiler.done
  end
  table.insert(profiler.top,op)
  op.stop = crawl.millis()
  profiler.level = profiler.level - 1
end

-- End the profiling job and print some output nicely
function profiler.stop()
  profiler.stop_time = crawl.millis()
  if profiler.name == nil then profiler.name = "Profile" end
  local under = ""
  for n = 1,#name,1 do
    under = under .. "="
  end
  print("")
  print(under)
  print(profiler.name)
  print(under)
  print("")
  print("Profiler Start: " .. profiler.start_time)
  print("Profiler Stop: " .. profiler.stop_time)
  profiler.print_time("Total",profiler.stop_time-profiler.start_time)
  print("")
  print("All Operations:")
  for i,op in ipairs(profiler.done) do
    profiler.render(op)
  end
end

function profiler.render(op)
  local name = op.name
  -- Pad the name with some spacing depending on the level of the operation
  if op.level > 0 then
    for n = 1,op.level,1 do
      name = "  " .. name
    end
  end
  profiler.print_time(name,op.stop-op.start,op.data)
  for i,op in ipairs(op.children) do
    profiler.render(op)
  end
end
