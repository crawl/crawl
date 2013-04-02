------------------------------------------------------------------------------
-- profiler.lua
-- A very simple Lua profiler
------------------------------------------------------------------------------

profiler = {}

function profiler.print_time(label,millis)
  print(label .. ": " .. (millis/1000) .. "secs")
end

-- Begins profiling
function profiler.start()
  profiler.stack = {}
  profiler.done = {}
  profiler.start_time = crawl.millis()
  print("Started Profiling: " .. profiler.start_time)
end

function profiler.push(name,data)
  local now = crawl.millis()
  local op = { name = name, data = data, start = now }
  table.insert(profiler.stack,op)
end

function profiler.pop()
  local op = profiler.stack[#(profiler.stack)]
  table.remove(profiler.stack,#(profiler.stack))
  table.insert(profiler.done,op)
  op.stop = crawl.millis()
  profiler.print_time(op.name,op.stop-op.start)
end

function profiler.stop()
  profiler.stop_time = crawl.millis()
  print("Stopped Profiling: " .. profiler.start_time)
  profiler.print_time("Total",profiler.stop_time-profiler.start_time)
end
