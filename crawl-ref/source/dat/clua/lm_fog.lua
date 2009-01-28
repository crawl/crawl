------------------------------------------------------------------------------
-- lm_tmsg.lua:
-- Fog machines.
--
-- There are three different "pure" ways to use a fog machine marker:
--
-- 1) Repeatedly lay down medium to large clouds on top of the marker
--    and let them pile up on one another.  (One of the cloud grids in
--    the first laid cloud has to decay away before this is this really
--    starts working.
--
-- 2) Perform random walks from the marker and place a single-grid cloud
--    at the destination of each walk.
--
-- 3) Place a single-grid cloud on the marker and let it spread out.
--
-- Combining these different methods, along with varying the different
-- parameters, can be used to achieve different effects.
--
-- Marker parameters:
--
-- cloud_type: The name of the cloud type to use.  Possible cloud types are:
--     flame, noxious fumes, freezing vapour, poison gases,
--     grey smoke, blue smoke, purple smoke, steam, 
--     foul pestilence, black smoke, thin mist (the default).
-- walk_dist: The distance to move over the course of one random walk.
--     defaults to 0.
-- pow_min: The minimum "power" (lifetime) of each cloud; defaults to 1.
-- pow_max: The maximum power of each cloud; must be provided.
-- pow_rolls: The number of rolls of [pow_min, pow_max], with the average
--     value uses; increasing the values makes the average value more likely
--     and extreme values less likely.  Defaults to 1.
-- delay, delay_min and delay_max: The delay between laying down one cloud
--     and the next.  10 is equal to normal-speed player turn.  Either
--     delay or delay_max and delay_min must be provided.  Providing just
--     "delay" is equivalent to delay_min and delay_max being equal.
-- size, size_min and size_max: The number of grids each cloud will cover.
--     Either size or size_max and size_min must be provided.  Providing
--     just "size" is equivalent to size_min and size_max being equal.
-- size_buildup_amnt, size_buildup_time: Increase the cloud size over time.
--     Adds (size_buildup_amnt / size_buildup_time * turns_since_made)
--     to size_min and size_max, maxing out at size_buildup_amnt.
-- spread_rate: The rate at which a cloud spreads.  Must either be
--     -1 (default spread rate that varies by cloud type) or between
--     0 and 100 inclusive.
-- spread_rate_amnt, spread_rate_buildup: Similar to size_buildup_amnt
--     and size_buildup_time
-- start_clouds: The number of clouds to lay when the level containing
--     the cloud machine is entered.  This is necessary since clouds
--     are cleared when the player leaves a level.
------------------------------------------------------------------------------

FogMachine = { CLASS = "FogMachine" }
FogMachine.__index = FogMachine

function FogMachine:_new()
  local m = { }
  setmetatable(m, self)
  self.__index = self
  return m
end

function FogMachine:new(pars)
  if not pars then
    error("No parameters provided")
  end

  if not pars.pow_max then
    error("No pow_max provided.")
  end

  if not (pars.delay or (pars.delay_min and pars.delay_max)) then
    error("Delay parameters not provided.")
  end

  if not (pars.size or (pars.size_min and pars.size_max)) then
    error("Size parameters not provided.")
  end

  local m = FogMachine:_new()
  m.cloud_type   = pars.cloud_type   or "thin mist"
  m.walk_dist    = pars.walk_dist    or 0
  m.pow_min      = pars.pow_min      or 1
  m.pow_max      = pars.pow_max
  m.pow_rolls    = pars.pow_rolls    or 3
  m.delay_min    = pars.delay_min    or pars.delay or 1
  m.delay_max    = pars.delay_max    or pars.delay
  m.kill_cat     = pars.kill_cat     or "other"
  m.size_min     = pars.size_min     or pars.size or 1
  m.size_max     = pars.size_max     or pars.size
  m.spread_rate  = pars.spread_rate  or -1
  m.start_clouds = pars.start_clouds or 1

  m.size_buildup_amnt   = pars.size_buildup_amnt   or 0
  m.size_buildup_time   = pars.size_buildup_time   or 1
  m.spread_buildup_amnt = pars.spread_buildup_amnt or 0
  m.spread_buildup_time = pars.spread_buildup_time or 1

  m.buildup_turns = 0
  m.countdown          = 0

  return m
end

function FogMachine:do_fog(marker)
  local x, y = marker:pos()
  if self.walk_dist > 0 then
    x, y = dgn.random_walk(x, y, self.walk_dist)
  end

  local buildup_turns = self.buildup_turns

  -- Size buildup
  if buildup_turns > self.size_buildup_time then
    buildup_turns = self.size_buildup_time
  end

  local size_buildup = self.size_buildup_amnt * buildup_turns /
    self.size_buildup_time

  local size_min = self.size_min + size_buildup
  local size_max = self.size_max + size_buildup

  if (size_min < 0) then
    size_min = 0
  end

  -- Spread buildup
  buildup_turns = self.buildup_turns

  if buildup_turns > self.spread_buildup_time then
    buildup_turns = self.spread_buildup_time
  end

  local spread = self.spread_rate + (self.spread_buildup_amnt * buildup_turns /
                                     self.spread_buildup_time)

  dgn.apply_area_cloud(x, y, self.pow_min, self.pow_max, self.pow_rolls,
                       crawl.random_range(size_min, size_max, 1),
                       self.cloud_type, self.kill_cat, spread)
end

function FogMachine:activate(marker, verbose)
  local _x, _y = marker:pos()
  dgn.register_listener(dgn.dgn_event_type('turn'), marker)
  dgn.register_listener(dgn.dgn_event_type('entered_level'), marker)
end

function FogMachine:event(marker, ev)
  local _x, _y = marker:pos()
  if ev:type() == dgn.dgn_event_type('turn') then
    self.countdown = self.countdown - ev:ticks()

    self.buildup_turns = self.buildup_turns + ev:ticks()

    if (self.buildup_turns > self.size_buildup_time) then
      self.buildup_turns = self.size_buildup_time
    end

    while self.countdown <= 0 do
      self:do_fog(marker)
      self.countdown = self.countdown +
        crawl.random_range(self.delay_min, self.delay_max, 1)
    end
  elseif ev:type() == dgn.dgn_event_type('entered_level') then
    for i = 1, self.start_clouds do
      self:do_fog(marker)
      self.countdown = crawl.random_range(self.delay_min, self.delay_max, 1)
      self.buildup_turns = 0
    end
  end
end

function FogMachine:write(marker, th)
  file.marshall(th, self.cloud_type)
  file.marshall(th, self.walk_dist)
  file.marshall(th, self.pow_min)
  file.marshall(th, self.pow_max)
  file.marshall(th, self.pow_rolls)
  file.marshall(th, self.delay_min)
  file.marshall(th, self.delay_max)
  file.marshall(th, self.kill_cat)
  file.marshall(th, self.size_min)
  file.marshall(th, self.size_max)
  file.marshall(th, self.spread_rate)
  file.marshall(th, self.start_clouds)
  file.marshall(th, self.size_buildup_amnt)
  file.marshall(th, self.size_buildup_time)
  file.marshall(th, self.spread_buildup_amnt)
  file.marshall(th, self.spread_buildup_time)
  file.marshall(th, self.buildup_turns)
  file.marshall(th, self.countdown)
end

function FogMachine:read(marker, th)
  self.cloud_type          = file.unmarshall_string(th)
  self.walk_dist           = file.unmarshall_number(th)
  self.pow_min             = file.unmarshall_number(th)
  self.pow_max             = file.unmarshall_number(th)
  self.pow_rolls           = file.unmarshall_number(th)
  self.delay_min           = file.unmarshall_number(th)
  self.delay_max           = file.unmarshall_number(th)
  self.kill_cat            = file.unmarshall_string(th)
  self.size_min            = file.unmarshall_number(th)
  self.size_max            = file.unmarshall_number(th)
  self.spread_rate         = file.unmarshall_number(th)
  self.start_clouds        = file.unmarshall_number(th)
  self.size_buildup_amnt   = file.unmarshall_number(th)
  self.size_buildup_time   = file.unmarshall_number(th)
  self.spread_buildup_amnt = file.unmarshall_number(th)
  self.spread_buildup_time = file.unmarshall_number(th)
  self.buildup_turns       = file.unmarshall_number(th)
  self.countdown           = file.unmarshall_number(th)

  setmetatable(self, FogMachine)

  return self
end

function fog_machine(pars)
  return FogMachine:new(pars)
end

function fog_machine_geyser(cloud_type, size, power, buildup_amnt,
                            buildup_time)
  return FogMachine:new {
    cloud_type = cloud_type, pow_max = power, size = size,
    delay_min = power , delay_max = power * 2,
    size_buildup_amnt = buildup_amnt or 0,
    size_buildup_time = buildup_time or 1
  }
end

function fog_machine_spread(cloud_type, size, power, buildup_amnt,
                            buildup_time)
  return FogMachine:new {
    cloud_type = cloud_type, pow_max = power, spread_rate = size,
    size = 1, delay_min = 5, delay_max = 15,
    spread_buildup_amnt = buildup_amnt or 0,
    spread_buildup_time = buildup_time or 1
  }
end

function fog_machine_brownian(cloud_type, size, power)
  return FogMachine:new {
    cloud_type = cloud_type, size = 1, pow_max = power,
    walk_dist = size, delay_min = 1, delay_max = power / size
  }
end
