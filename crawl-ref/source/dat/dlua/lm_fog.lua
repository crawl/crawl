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
-- cloud_type: The name of the cloud type to use. Possible cloud types are:
--     flame, noxious fumes, freezing vapour, poison gas,
--     grey smoke, blue smoke, translocational energy, steam, rain,
--     foul pestilence, black smoke, mutagenic fog, thin mist (the default).
-- walk_dist: The distance to move over the course of one random walk.
--     defaults to 0.
-- pow_min: The minimum "power" (lifetime) of each cloud; defaults to 1.
-- pow_max: The maximum power of each cloud; must be provided.
-- pow_rolls: The number of rolls of [pow_min, pow_max], with the average
--     value uses; increasing the values makes the average value more likely
--     and extreme values less likely. Defaults to 1.
-- delay, delay_min and delay_max: The delay between laying down one cloud
--     and the next.  10 is equal to normal-speed player turn. Either
--     delay or delay_max and delay_min must be provided. Providing just
--     "delay" is equivalent to delay_min and delay_max being equal.
-- size, size_min and size_max: The number of grids each cloud will cover.
--     Either size or size_max and size_min must be provided. Providing
--     just "size" is equivalent to size_min and size_max being equal.
-- size_buildup_amnt, size_buildup_time: Increase the cloud size over time.
--     Adds (size_buildup_amnt / size_buildup_time * turns_since_made)
--     to size_min and size_max, maxing out at size_buildup_amnt.
-- spread_rate: The rate at which a cloud spreads. Must either be
--     -1 (default spread rate that varies by cloud type) or between
--     0 and 100 inclusive.
-- spread_rate_amnt, spread_rate_buildup: Similar to size_buildup_amnt
--     and size_buildup_time
-- start_clouds: The number of clouds to lay when the level containing
--     the cloud machine is entered. This is necessary since clouds
--     are cleared when the player leaves a level.
-- listener: A table with a function field called 'func'. Will be called
--     whenever the countdown is activated, and whenever the fog
--     machine is reset. It will be called with a reference to the table,
--     the fog machine, the Triggerer which was the cause, the marker,
--     and the vevent which was the cause.
-- excl_rad: A value determining how large an exclusion radius will be
--     placed around this cloud. A value of -1 means that no exclusion will be
--     placed, while a value of 0 means that only the cloud will be excluded.
--     Any other value is used as the size of the exclusion.
--
------------------------------------------------------------------------------

crawl_require('dlua/lm_trig.lua')

FogMachine       = util.subclass(Triggerable)
FogMachine.CLASS = "FogMachine"

function FogMachine:_new()
  local m = self.super.new(self)
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
  m.kill_cat     = pars.kill_cat     or "other"
  m.size_min     = pars.size_min     or pars.size or 1
  m.size_max     = pars.size_max     or pars.size
  m.spread_rate  = pars.spread_rate  or -1
  m.start_clouds = pars.start_clouds or 1
  m.excl_rad     = pars.excl_rad     or 1

  m.size_buildup_amnt   = pars.size_buildup_amnt   or 0
  m.size_buildup_time   = pars.size_buildup_time   or 1
  m.spread_buildup_amnt = pars.spread_buildup_amnt or 0
  m.spread_buildup_time = pars.spread_buildup_time or 1

  m.buildup_turns      = 0

  local tick_pars = {}
  tick_pars.delay_min = pars.delay_min or pars.delay or 1
  tick_pars.delay_max = pars.delay_max or pars.delay
  tick_pars.type      = "turn"

  m:add_triggerer( DgnTriggerer:new (tick_pars) )

  m:add_triggerer( DgnTriggerer:new { type = "entered_level" } )

  if pars.listener then
    m:add_listener(pars.listener)
  end

  return m
end

function FogMachine:apply_cloud(point, pow_min, pow_max, pow_rolls,
                                size, cloud_type, kill_cat, spread, excl_rad)
  dgn.apply_area_cloud(point.x, point.y, pow_min, pow_max, pow_rolls, size,
                       cloud_type, kill_cat, spread, excl_rad)
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
                   self.cloud_type, self.kill_cat, spread, self.excl_rad)
end

function FogMachine:do_trigger(triggerer, marker, ev)
  -- Override do_trigger for things that we want to do only once if
  -- there's multiple markers slaved to this one.
  if triggerer.type == "turn" then
    self.buildup_turns = self.buildup_turns + ev:ticks()

    if (self.buildup_turns > self.size_buildup_time) then
      self.buildup_turns = self.size_buildup_time
    end
  elseif triggerer.type == "entered_level" then
    local et = dgn.dgn_event_type("turn")
    for _, trig_idx in ipairs(self.dgn_trigs_by_type[et]) do
      local trig = self.triggerers[trig_idx]
      trig:reset_countdown()
    end
  end

  -- Turns passing without reaching the countdown shouldn't generate
  -- fog.
  if triggerer.type == "turn" and triggerer.sub_type ~= "countdown" then
    triggerer.listener_only = true
  else
    triggerer.listener_only = false
  end

  -- This will call on_trigger() for all the slaves.
  FogMachine.super.do_trigger(self, triggerer, marker, ev)
end

function FogMachine:on_trigger(triggerer, marker, ev)
  if triggerer.type == 'turn' then
    self:do_fog(dgn.point(marker:pos()))
  elseif triggerer.type == 'entered_level' then
    for i = 1, self.start_clouds do
      self:do_fog(dgn.point(marker:pos()))
    end
  end
end

function FogMachine:write(marker, th)
  FogMachine.super.write(self, marker, th)

  file.marshall(th, self.cloud_type)
  file.marshall(th, self.walk_dist)
  file.marshall(th, self.pow_min)
  file.marshall(th, self.pow_max)
  file.marshall(th, self.pow_rolls)
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
  file.marshall(th, self.excl_rad)
end

function FogMachine:read(marker, th)
  FogMachine.super.read(self, marker, th)

  self.cloud_type          = file.unmarshall_string(th)
  self.walk_dist           = file.unmarshall_number(th)
  self.pow_min             = file.unmarshall_number(th)
  self.pow_max             = file.unmarshall_number(th)
  self.pow_rolls           = file.unmarshall_number(th)
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
  if tags.major_version() == 34 and file.minor_version(th) < tags.TAG_MINOR_DECUSTOM_CLOUDS then
                             file.unmarshall_string(th)
                             file.unmarshall_string(th)
                             file.unmarshall_string(th)
  end
  self.excl_rad            = file.unmarshall_number(th)

  setmetatable(self, FogMachine)

  return self
end

function fog_machine(pars)
  return FogMachine:new(pars)
end

function chained_fog_machine(pars)
  return Triggerable.synchronized_markers(FogMachine:new(pars))
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
  if not see_func then
    see_func = global_function("you.see_cell")
  end
  pars = {
    see_message = see_mesg,
    cantsee_message = cantsee_mesg,
    turns = trns * 10,
    warning_done = false,
    see_function = see_func
  }
  pars.func = global_function("warning_machine_warning_func")
  return pars
end

function warning_machine_warning_func(self, fog_machine, triggerer, marker, ev)
  local countdown = triggerer.countdown
  local point     = dgn.point(marker:pos())
  if triggerer.type == "turn" and triggerer.sub_type == "tick"
    and countdown <= self.turns
  then
    if not self.warning_done then
      if self.see_message and self.see_function(point.x, point.y) then
        crawl.mpr(self.see_message, "warning")
      elseif self.cantsee_message then
        crawl.mpr(self.cantsee_message, "warning")
      end
      self.warning_done = true
    end
  elseif event_name == "trigger" then
    self.warning_done = false
  end
end


function trigger_machine (cantsee_mesg, see_mesg, chan, see_func)
  if see_mesg == nil and cantsee_mesg == nil then
    error("Triggermachine requires a message!")
  end
  if not see_func then
    see_func = global_function("you.see_cell")
  end
  pars = {
    channel = chan or nil,
    see_message = see_mesg,
    cantsee_message = cantsee_mesg,
    see_function = see_func
  }
  pars.func = global_function("trigger_machine_trigger_func")
  return pars
end

function trigger_machine_trigger_func(self, fog_machine, triggerer, marker, ev)
  local point     = dgn.point(marker:pos())
  if triggerer.type == "turn" and triggerer.sub_type == "countdown" then
    channel = self.channel or ""
    if self.see_message ~= nil and self.see_function(point.x, point.y) then
      crawl.mpr(self.see_message, channel)
    elseif self.cantsee_message ~= nil then
      crawl.mpr(self.cantsee_message, channel)
    end
  end
end

function tw_machine (warn_turns, warn_cantsee_message,
                     trig_cantsee_message, trig_channel,
                     trig_see_message, warn_see_message,
                     see_func)
  if (not warn_turns or (not warn_see_message and not warn_cantsee_message)
      or (not trig_see_message and not trig_cantsee_message)) then
    error("TWMachine needs warning turns, warning message and "
          .. "triggering message.")
  end

  see_func = global_function(see_func or "you.see_cell")

  pars = {
    warning_see_message = warn_see_message,
    warning_cantsee_message = warn_cantsee_message,
    warning_turns = warn_turns * 10,
    warning_done = false,
    trigger_see_message = trig_see_message,
    trigger_cantsee_message = trig_cantsee_message,
    trigger_channel = trig_channel or nil,
    see_function = see_func
  }
  pars.func = global_function("tw_machine_tw_func")

  return pars
end

function tw_machine_tw_func(self, fog_machine, triggerer, marker, ev)
  local point     = dgn.point(marker:pos())
  local countdown = triggerer.countdown
  if triggerer.type == "turn" and triggerer.sub_type == "tick"
    and countdown <= self.warning_turns
  then
    if self.warning_done ~= true then
      if self.warning_see_message and self.see_function(point.x, point.y) then
        crawl.mpr(self.warning_see_message, "warning")
      elseif self.warning_cantsee_message ~= nil then
        crawl.mpr(self.warning_cantsee_message, "warning")
      end
      self.warning_done = true
    end
  elseif triggerer.type == "turn" and triggerer.sub_type == "countdown" then
    self.warning_done = false
    channel = self.trigger_channel or ""
    if self.trigger_see_message and self.see_function(point.x, point.y) then
      crawl.mpr(self.trigger_see_message, channel)
    elseif self.trigger_cantsee_message ~= nil then
      crawl.mpr(self.trigger_cantsee_message, channel)
    end
  end
end
