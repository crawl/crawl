------------------------------------------------------------------------------
-- lm_fog.lua:
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
-- Fog machines can be "chained" together (activated at the same time) by using
-- the convenience function "chained_fog_machine" with the normal paramaters 
-- which are accepted by "fog_machine".
--
-- Marker parameters:
--
-- cloud_type: The name of the cloud type to use.  Possible cloud types are:
--     flame, noxious fumes, freezing vapour, poison gases,
--     grey smoke, blue smoke, purple smoke, steam, rain,
--     foul pestilence, black smoke, mutagenic fog, thin mist (the default).
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
-- listener: A FunctionMachine listener marker. Will be called
--     whenever the countdown is activated, and whenever the fog
--     machine is reset. It will be called with the FogMachine's
--     marker, a string containing the event ("decrement", "trigger"),
--     the actual event object, and a copy of the FogMachine itself.
--     See the section "Messages for fog machines" at the end of the
--     file.
--
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
  m.listener     = pars.listener     or nil

  m.size_buildup_amnt   = pars.size_buildup_amnt   or 0
  m.size_buildup_time   = pars.size_buildup_time   or 1
  m.spread_buildup_amnt = pars.spread_buildup_amnt or 0
  m.spread_buildup_time = pars.spread_buildup_time or 1

  m.buildup_turns      = 0
  m.countdown          = 0

  return m
end

function FogMachine:apply_cloud(point, pow_min, pow_max, pow_rolls,
                                size, cloud_type, kill_cat, spread)
  dgn.apply_area_cloud(point.x, point.y, pow_min, pow_max, pow_rolls, size,
                       cloud_type, kill_cat, spread)
end

function FogMachine:do_fog(point)
  local p = point
  if self.walk_dist > 0 then
    p = dgn.point(dgn.random_walk(p.x, p.y, self.walk_dist))
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

  self:apply_cloud(p, self.pow_min, self.pow_max, self.pow_rolls,
                   crawl.random_range(size_min, size_max, 1),
                   self.cloud_type, self.kill_cat, spread)
end

function FogMachine:activate(marker, verbose)
  local _x, _y = marker:pos()
  dgn.register_listener(dgn.dgn_event_type('turn'), marker)
  dgn.register_listener(dgn.dgn_event_type('entered_level'), marker)
end

function FogMachine:notify_listener(point, event, evobj)
  if self.listener then
    return self.listener:do_function(point, event, ev, self)
  end
end

function FogMachine:event(marker, ev)
  local _x, _y = marker:pos()
  if ev:type() == dgn.dgn_event_type('turn') then
    self.countdown = self.countdown - ev:ticks()

    self.buildup_turns = self.buildup_turns + ev:ticks()

    if (self.buildup_turns > self.size_buildup_time) then
      self.buildup_turns = self.size_buildup_time
    end

    if self.countdown > 0 then
      self:notify_listener(dgn.point(marker:pos()), "decrement", ev)
    elseif self.countdown <= 0 then
      self:notify_listener(dgn.point(marker:pos()), "trigger", ev)
    end

    while self.countdown <= 0 do
      self:do_fog(dgn.point(marker:pos()))
      self.countdown = self.countdown +
        crawl.random_range(self.delay_min, self.delay_max, 1)
    end
  elseif ev:type() == dgn.dgn_event_type('entered_level') then
    for i = 1, self.start_clouds do
      self:do_fog(dgn.point(marker:pos()))
      self:notify_listener(dgn.point(marker:pos()), "trigger", ev)
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
  if self.listener then
    file.marshall_meta(th, true)
    self.listener:write(marker, th)
  else
    file.marshall_meta(th, false)
  end
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
  got_listener               = file.unmarshall_meta(th)
  if got_listener == true then
    self.listener = function_machine {
      marker_type = "listener",
      func = (function() end)
    }
    self.listener:read(marker, th)
  else
    self.listener = nil
  end

  setmetatable(self, FogMachine)

  return self
end

function fog_machine(pars)
  return FogMachine:new(pars)
end

function chained_fog_machine(pars)
  return lmark.synchronized_markers(FogMachine:new(pars), "do_fog")
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

-------------------------------------------------------------------------------
-- Messages for fog machines.
--
-- * warning_machine: Takes three parameters: turns, cantsee_message,
--      and, optionally, see_message. Turns is the value of player
--      turns before to trigger the message before the fog machine is
--      fired. If only see_message is provided, the message will only
--      be printed if the player can see the fog machine. If only
--      cantsee_mesage is provided, the message will be displayed
--      regardless. In combination, the message will be different
--      depending on whether or not the player can see the marker. By
--      default, the message will be displaying using the "warning"
--      channel.
--
-- * trigger_machine: Takes three parameters: cantsee_message,
--      see_message, and, optionally, channel. The functionality is
--      identical to a warning_machine, only the message is instead
--      displayed (or not displayed) when the fog machine is
--      triggered. The message channel can be provided.
--
-- * tw_machine: Combines the above two message machines, providing
--      warning messages as well as messages when triggered. Takes the
--      parameters: warn_turns, warning_cantsee_message,
--      trigger_cantsee_message, trigger_channel, trigger_see_message,
--      warning_see_message. Parameters work as described above.
--
-- In all instances, the "cantsee" form of the message parameter
-- cannot be null, and for warning and dual trigger/warning machines,
-- the turns parameter cannot be null. All other parameters are
-- considered optional.

function warning_machine (trns, cantsee_mesg, see_mesg, see_func)
  if trns == nil or (see_mesg == nil and cantsee_mesg == nil) then
    error("WarningMachine requires turns and message!")
  end
  local function warning_func (point, mtable, event_name, event, fm)
    local countdown = fm.countdown
    if event_name == "decrement" and countdown <= mtable.turns then
      if not mtable.warning_done then
        if mtable.see_message and mtable.see_function(point.x, point.y) then
          crawl.mpr(mtable.see_message, "warning")
        elseif mtable.cantsee_message then
          crawl.mpr(mtable.cantsee_message, "warning")
        end
        mtable.warning_done = true
      end
    elseif event_name == "trigger" then
      mtable.warning_done = false
    end
  end
  pars = {marker_type = "listener"}
  if not see_func then
    see_func = you.see_cell
  end
  pars.marker_params = {
    see_message = see_mesg,
    cantsee_message = cantsee_mesg,
    turns = trns * 10,
    warning_done = false,
    see_function = see_func
  }
  pars.func = warning_func
  return FunctionMachine:new(pars)
end

function trigger_machine (cantsee_mesg, see_mesg, chan, see_func)
  if see_mesg == nil and cantsee_mesg == nil then
    error("Triggermachine requires a message!")
  end
  local function trigger_func (point, mtable, event_name, event, fm)
    local countdown = fm.countdown
    if event_name == "trigger" then
      channel = mtable.channel or ""
      if mtable.see_message ~= nil and mtable.see_function(point.x, point.y) then
        crawl.mpr(mtable.see_message, channel)
      elseif mtable.cantsee_message ~= nil then
        crawl.mpr(mtable.cantsee_message, channel)
      end
    end
  end
  pars = {marker_type = "listener"}
  if not see_func then
    see_func = you.see_cell
  end
  pars.marker_params = {
    channel = chan or nil,
    see_message = see_mesg,
    cantsee_message = cantsee_mesg,
    see_function = see_func
  }
  pars.func = trigger_func
  return FunctionMachine:new(pars)
end

function tw_machine (warn_turns, warn_cantsee_message,
                     trig_cantsee_message, trig_channel,
                     trig_see_message, warn_see_message, see_func)
  if (not warn_turns or (not warn_see_message and not warn_cantsee_message)
      or (not trig_see_message and not trig_cantsee_message)) then
    error("TWMachine needs warning turns, warning message and "
          .. "triggering message.")
  end
  local function tw_func (point, mtable, event_name, event, fm)
    local countdown = fm.countdown
    if event_name == "decrement" and countdown <= mtable.warning_turns then
      if mtable.warning_done ~= true then
        if mtable.warning_see_message and mtable.see_function(point.x, point.y) then
          crawl.mpr(mtable.warning_see_message, "warning")
        elseif mtable.warning_cantsee_message ~= nil then
          crawl.mpr(mtable.warning_cantsee_message, "warning")
        end
        mtable.warning_done = true
      end
    elseif event_name == "trigger" then
      mtable.warning_done = false
      channel = mtable.trigger_channel or ""
      if mtable.trigger_see_message and mtable.see_function(point.x, point.y) then
        crawl.mpr(mtable.trigger_see_message, channel)
      elseif mtable.trigger_cantsee_message ~= nil then
        crawl.mpr(mtable.trigger_cantsee_message, channel)
      end
    end
  end
  if not see_func then
    see_func = you.see_cell
  end
  pars = {marker_type = "listener"}
  pars.marker_params = {
    warning_see_message = warn_see_message,
    warning_cantsee_message = warn_cantsee_message,
    warning_turns = warn_turns * 10,
    warning_done = false,
    trigger_see_message = trig_see_message,
    trigger_cantsee_message = trig_cantsee_message,
    trigger_channel = trig_channel or nil,
    see_function = see_func
  }
  pars.func = tw_func
  return FunctionMachine:new(pars)
end
