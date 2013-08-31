------------------------------------------------------------------------------
-- lm_trig.lua:
-- DgnTriggerers and triggerables:
--
-- This is similar to the observable/observer design pattern: a triggerable
-- class which does something and a triggerer which sets it off.  As an
-- example, the ChangeFlags class (dlua/lm_flags.lua), rather than having
-- three subclasses (for monster death, feature change and item pickup)
-- needs no subclasses, but is just one class which is given triggerers
-- which listen for different events.  Additionally, new types of triggerers
-- can be developed and used without have to update the ChangeFlags code.
--
-- Unlike with the observable/observer design pattern, each triggerer is
-- associated with a single triggerable, rather than there being one observable
-- and multiple observers, since each triggerer might have a data payload which
-- is meant to be different for each triggerable.
--
-- A triggerable class just needs to subclass Triggerable and define an
-- "on_trigger" method.
--
-- If a triggerable marker has the property "master_name" with the value
-- "FOO", then when triggered on_trigger() will be called at each marker
-- on the level which has the property "slaved_to" equal to "FOO".  A
-- master marker can be slaved to itself to cause on_trigger() to be called
-- at its location.  If the master marker has the property
-- "single_random_slave" set to anything but the empty string ("") then
-- on each triggering only a single, randomly chosen slave will have
-- on_trigger() called.
--
-- Ordinarily, a master marker which listens to position-dependent events will
-- only be triggered by events which happen at the master's position.  To make
-- the master marker also listen to events which happen at the locations of the
-- slave markers, set the property "listen_to_slaves" to anything but the empty
-- string true.  This will cause all of the slave markers to be triggered
-- whenever any of the slave markers are triggered.  To only trigger the slave
-- where the event happened, also set the property "only_at_slave" to anything
-- but the empty string.
--
-- on_trigger() shouldn't have to worry about the master/slave business,
-- and should have the same code regardless of whether or not it's a
-- master or just a plain triggerable.  If on_trigger() calls
-- self:remove() while it's acting on a slave marker then the slave marker
-- will be removed, and if called on the master while the master is slaved
-- to itself it will stop acting as a slave, with the master marker being
-- automatically removed when all slaves are gone.
------------------------------------------------------------------------------

-- XXX: Listeners and the whole master/slave business could be more
-- generally and more flexibly done by implementing a framework for
-- Lua defined/fired events, making Triggerable fire a Lua event every
-- time it's triggered, and then registering listeners for those Lua
-- events.  However, we don't need that much flexibility yet, so it's
-- all handled inside of the Triggerable class.

require('dlua/lm_mslav.lua')

Triggerable = { CLASS = "Triggerable" }
Triggerable.__index = Triggerable

function Triggerable:new(props)
  props = props or {}

  local tr = { }
  setmetatable(tr, self)
  self.__index = self

  tr.props             = props
  tr.triggerers        = { }
  tr.dgn_trigs_by_type = { }
  tr.listeners         = { }

  return tr
end

function Triggerable:unmangle(x)
  if x and util.callable(x) then
    return x(self)
  else
    return x
  end
end

function Triggerable:property(marker, pname)
  return self:unmangle(self.props[pname] or '')
end

function Triggerable:set_property(pname, pval)
  local old_val = self.props[pname]
  self.props[pname] = pval
  return old_val
end

function Triggerable:add_listener(listener)
  if type(listener) ~= "table" then
    error("listener isn't a table")
  end
  if not listener.func then
    error("listener has no func")
  end
  listener.func = global_function(listener.func, "triggerable listener")
  table.insert(self.listeners, listener)
end

function Triggerable:add_triggerer(triggerer)
  if not triggerer.type then
    error("triggerer has no type")
  end

  table.insert(self.triggerers, triggerer)

  if (triggerer.method == "dgn_event") then
    local et = dgn.dgn_event_type(triggerer.type)
    if not self.dgn_trigs_by_type[et] then
      self.dgn_trigs_by_type[et] = {}
    end

    table.insert(self.dgn_trigs_by_type[et], #self.triggerers)
  else
    local method = triggerer.method or "(nil)"

    local class
    local meta = getmetatable(triggerer)
    if not meta then
      class = "(no meta table)"
    elseif not meta.CLASS then
      class = "(no class name)"
    else
      class = meta.CLASS
    end

    error("Unknown triggerer method '" .. method .. "' for trigger class '"
          .. class .. "'")
  end

  triggerer:added(self)
end

function Triggerable:move(marker, dest, y)
  local was_activated = self.activated

  self:remove_all_triggerers(marker)

  -- XXX: Are coordinated passed around as single objects?
  if y then
    marker:move(dest, y)
  else
    marker:move(dest)
  end

  if was_activated then
    self.activated = false
    self:activate(marker)
  end
end

function Triggerable:remove(marker)
  if self.removed then
    crawl.mpr("ERROR: Triggerable already removed", "error")
    return
  end

  if self.calling_slaves then
    self.want_remove = true
    return
  end

  self:remove_all_triggerers(marker)
  dgn.remove_marker(marker)

  self.removed = true
end

function Triggerable:remove_all_triggerers(marker)
  for _, trig in ipairs(self.triggerers) do
    trig:remove(self, marker)
  end
end

function Triggerable:activate(marker)
  if self.removed then
    error("Can't activate, triggerable removed")
  end

  if #self.triggerers == 0 then
    error("Triggerable has no triggerers")
  end

  if self.activating then
    error("Triggerable already activating")
  end

  -- We're not aborting if already activated, since we may
  -- need to reregister listeners after other code has
  -- reset dungeon events. Lugonu's corruption does this,
  -- for example. If any triggerable code wants to avoid
  -- being activated twice, that specific code could
  -- check against self.activated.

  self.activating = true

  -- NOTE: The master marker can be slaved to itself.
  local listen_to_slaves = self:property(marker, "listen_to_slaves")
  local master_name      = self:property(marker, "master_name")

  local slaves
  if listen_to_slaves ~= "" and master_name ~= ""  then
    slaves = dgn.find_markers_by_prop("slaved_to", master_name)
    if #slaves == 0 then
      error("Triggerable has no slaves to listen to")
    end
  else
    slaves = { marker }
  end

  for _, trig in ipairs(self.triggerers) do
    local et = dgn.dgn_event_type(trig.type)

    if (dgn.dgn_event_is_position(et)) then
      for _, slave_marker in ipairs(slaves) do
        trig:activate(self, marker, slave_marker:pos())
      end
    else
      trig:activate(self, marker)
    end
  end
  self.activating = false
  self.activated  = true
end

function Triggerable:event(marker, ev)
  local et = ev:type()

  local trig_list = self.dgn_trigs_by_type[et]

  if not trig_list then
    local class  = getmetatable(self).CLASS
    local x, y   = marker:pos()
    local e_type = dgn.dgn_event_type(et)

    error("Triggerable type " .. class .. " at (" ..x .. ", " .. y .. ") " ..
           "has no triggerers for dgn_event " .. e_type )
  end

  for _, trig_idx in ipairs(trig_list) do
    self.triggerers[trig_idx]:event(self, marker, ev)

    if self.removed then
      return
    end
  end
end

function Triggerable:do_trigger(triggerer, marker, ev)
  for _, listener in ipairs(self.listeners) do
    listener:func(triggerable, triggerer, marker, ev)
  end

  if triggerer.listener_only then
    return
  end

  local master_name = self:property(marker, "master_name")

  if master_name == "" then
    self:on_trigger(triggerer, marker, ev)
    return
  end

  -- NOTE: The master marker can be slaved to itself.
  local slaves

  if self:property(marker, "only_at_slave") ~= '' then
    local slave_marker = dgn.marker_at_pos(ev:pos())
    if not slave_marker then
      error("No slave marker at event position")
    end

    slaves = { slave_marker }
  else
    slaves = dgn.find_markers_by_prop("slaved_to", master_name)
  end

  -- If all slaves are gone, we're done.
  if #slaves == 0 then
    self:remove(marker)
    return
  end

  local master_pos = dgn.point(marker:pos())
  local num_slaves = #slaves

  if self:property(marker, "single_random_slave") ~= '' then
    slaves = { slaves[ crawl.random2(#slaves) + 1 ] }
  end

  local num_want_remove = 0
  self.calling_slaves = true
  for _, slave_marker in ipairs(slaves) do
    self.want_remove = false

    self:on_trigger(triggerer, slave_marker, ev)

    if self.want_remove then
      num_want_remove = num_want_remove + 1

      if dgn.point(slave_marker:pos()) == master_pos then
        -- The master marker shouldn't be removed until the end, so
        -- simply stop being slaved to itself.
        self.props.slaved_to = nil
        if self:property("listen_to_slaves") ~= "" then
          local et = dgn.dgn_event_type(triggerer.type)

          if (dgn.dgn_event_is_position(et)) then
            triggerer:remove(self, slave_marker)
          end
        end
      else
        triggerer:remove(self, slave_marker)
        dgn.remove_marker(slave_marker)
      end
    end
  end
  self.calling_slaves = false

  if num_want_remove >= num_slaves then
    -- Make sure they're really all gone.
    slaves =
      dgn.find_markers_by_prop("slaved_to", master_name)

    if #slaves == 0 then
      self:remove(marker)
    end
  end
end

function Triggerable:write(marker, th)
  file.marshall(th, #self.triggerers)
  for _, trig in ipairs(self.triggerers) do
    -- We'll be handling the de-serialization of the triggerer, so we need to
    -- save the class name.
    file.marshall(th, getmetatable(trig).CLASS)
    trig:write(marker, th)
  end

  lmark.marshall_table(th, self.dgn_trigs_by_type)
  lmark.marshall_table(th, self.props)
  lmark.marshall_table(th, self.listeners)
end

function Triggerable:read(marker, th)
  self.triggerers = {}

  local num_trigs = file.unmarshall_number(th)
  for i = 1, num_trigs do
    -- Hack to let triggerer classes de-serialize themselves.
    local trig_class    = file.unmarshall_string(th)

    -- _G is the global symbol table, and the class name of the triggerer is
    -- the name of that class' class object
    local trig_table = _G[trig_class].read(nil, marker, th)
    table.insert(self.triggerers, trig_table)
  end

  self.dgn_trigs_by_type = lmark.unmarshall_table(th)
  self.props             = lmark.unmarshall_table(th)
  self.listeners         = lmark.unmarshall_table(th)

  setmetatable(self, Triggerable)
  return self
end

--------------------------

-- Convenience functions, similar to the lmark ones in lm_mslav.lua

Triggerable.slave_cookie = 0

function Triggerable.next_slave_id()
  local slave_id = "marker_slave" .. Triggerable.slave_cookie
  Triggerable.slave_cookie = Triggerable.slave_cookie + 1
  return slave_id
end

function Triggerable.make_master(lmarker, slave_id)
  -- NOTE: The master marker is slaved to itself.
  lmarker:set_property("master_name", slave_id)
  lmarker:set_property("slaved_to",   slave_id)

  return lmarker
end

function Triggerable.make_slave(slave_id)
  return props_marker { slaved_to = slave_id }
end

function Triggerable.synchronized_markers(master)
  local first = true
  local slave_id = lmark.next_slave_id()
  return function ()
           if first then
             first = false
             return Triggerable.make_master(master, slave_id)
           else
             return Triggerable.make_slave(slave_id)
           end
         end
end

--------------------------

-- A simple class to invoke an arbitrary Lua function.  Should be split out
-- into own file if/when it becomes more complex.

util.subclass(Triggerable, "TriggerableFunction")

function TriggerableFunction:new(pars)
  pars = pars or { }

  local tf = self.super.new(self)

  if not pars.func then
    error("Must provide func to TriggerableFunction")
  end

  tf.func     = global_function(pars.func, "triggerable function")
  tf.repeated = pars.repeated
  tf.data     = pars.data or {}
  tf.props    = pars.props or {}

  return tf
end

function TriggerableFunction:on_trigger(triggerer, marker, ev)
  self.func(self.data, self, triggerer, marker, ev)

  if not self.repeated then
    self:remove(marker)
  end
end

function TriggerableFunction:write(marker, th)
  TriggerableFunction.super.write(self, marker, th)

  lmark.marshall_table(th, self.func)
  file.marshall_meta(th, self.repeated)
  lmark.marshall_table(th, self.data)
end

function TriggerableFunction:read(marker, th)
  TriggerableFunction.super.read(self, marker, th)

  self.func     = lmark.unmarshall_table(th)
  self.repeated = file.unmarshall_meta(th)
  self.data     = lmark.unmarshall_table(th)

  setmetatable(self, TriggerableFunction)

  return self
end

function triggerable_function_constructor(trigger_type)
  return function (func, data, repeated, props)
           local tf = TriggerableFunction:new {
             func = func,
             data = data,
             repeated = repeated,
             props = props
           }
           tf:add_triggerer( DgnTriggerer:new { type = trigger_type } )
           return tf
         end
end

function_at_spot = triggerable_function_constructor('player_move')
function_in_los  = triggerable_function_constructor('player_los')

--------------------------

-- A simple class to give out messages.  Should be split out into own
-- file if/when it becomes more complex.

TriggerableMessage       = util.subclass(Triggerable)
TriggerableMessage.CLASS = "TriggerableMessage"

function TriggerableMessage:new(pars)
  pars = pars or { }

  local tm = self.super.new(self)

  if not pars.msg then
    error("Must provide msg to TriggerableMessage")
  elseif type(pars.msg) ~= "string" then
    error("TriggerableMessage msg must be string, not " .. type(pars.msg))
  end

  pars.channel  = pars.channel or "plain"
  pars.repeated = pars.repeated

  tm.msg      = pars.msg
  tm.channel  = pars.channel
  tm.repeated = pars.repeated
  tm.props    = pars.props

  return tm
end

function TriggerableMessage:on_trigger(triggerer, marker, ev)
  crawl.mpr(self.msg, self.channel)

  if not self.repeated then
    self:remove(marker)
  end
end

function TriggerableMessage:write(marker, th)
  TriggerableMessage.super.write(self, marker, th)

  file.marshall(th, self.msg)
  file.marshall(th, self.channel)
  file.marshall_meta(th, self.repeated)
end

function TriggerableMessage:read(marker, th)
  TriggerableMessage.super.read(self, marker, th)

  self.msg      = file.unmarshall_string(th)
  self.channel  = file.unmarshall_string(th)
  self.repeated = file.unmarshall_meta(th)

  setmetatable(self, TriggerableMessage)

  return self
end

function message_at_spot(msg, channel, repeated, props)
  local tm = TriggerableMessage:new
      { msg = msg, channel = channel, repeated = repeated, props = props }

  tm:add_triggerer( DgnTriggerer:new { type   = "player_move" } )

  return tm
end

-------------------------------------------------------------------------------
-- NOTE: The CLASS string of a triggerer class *MUST* be exactly the same as
-- the triggerer class name, or it won't be able to deserialize properly.
--
-- NOTE: A triggerer shouldn't store a reference to the triggerable it
-- belongs to, and if it does then it must not save/restore that reference.
-------------------------------------------------------------------------------

-- DgnTriggerer listens for dungeon events of these types:
--
-- * monster_dies: Waits for a monster to die.  Needs the parameter
--       "target", who's value is the name of the monster who's death
--       we're wating for, or "any" for any monster.  Doesn't matter where
--       the triggerable/marker is placed.
--
-- * feat_change: Waits for a cell's feature to change.  Accepts the
--       optional parameter "target", which if set delays the trigger
--       until the feature the cell turns into contains the target as a
--       substring.  The triggerable/marker must be placed on top of the
--       cell who's feature you wish to monitor.
--
-- * item_moved: Wait for an item to move from one cell to another.
--      Needs the parameter "target", who's value is the name of the
--      item that is being tracked, or which can be "auto", in which
--      case it will pick the item placed by the vault.  The
--      triggerable/marker must be placed on top of the cell containing
--      the item.
--
-- * item_pickup: Wait for an item to be picked up.  Needs the parameter
--      "target", who's value is the name of the item that is being tracked,
--      or which can be "auto", in which case it will pick the item placed
--      by the vault.  The triggerable/marker must be placed on top of the
--      cell containing the item.  Automatically takes care of the item
--      moving from one square to another without being picked up.
--
-- * player_move: Wait for the player to move to a cell.  The
--      triggerable/marker must be placed on top of cell in question.
--
-- * player_los: Wait for the player to come into LOS of a cell. The
--      triggerable/marker must be placed on top of cell in question.
--
-- * turn: Called once for each player turn that passes.
--
-- * entered_level: Called when player enters the level, after all level
--      setup code has completed.
--
-- * door_opened, door_closed: Called whenever doors are opened and closed by
--      the player, or whenever they are opened by monsters (monsters do not
--      close doors).
--
-- * hp_warning: Called whenever a HP warning is triggered.
--
-- * pressure_plate: Called when someone (player or a monster) steps on a
--      pressure plate trap.

util.defclass("DgnTriggerer")

function DgnTriggerer:new(pars)
  if not pars then
    error("No parameters provided")
  end

  if not pars.type then
    error("DgnTriggerer must have a type")
  end

  -- Check for method name identical to event name.
  if not DgnTriggerer[pars.type] then
    error("DgnTriggerer can't handle event type '" .. pars.type .. "'")
  end

  if pars.type == "monster_dies" or pars.type == "item_moved"
     or pars.type == "item_pickup"
  then
    if not pars.target then
      error(pars.type .. " DgnTriggerer must have parameter 'target'")
    end
  end

  local tr = util.copy_table(pars)
  setmetatable(tr, self)
  self.__index = self

  tr:setup()

  if tr.type == "turn" and (tr.delay or (tr.delay_min and tr.delay_max)) then
    tr.delay_min = tr.delay_min or tr.delay or 1
    tr.delay_max = tr.delay_max or tr.delay

    tr.buildup_turns = 0
    tr.countdown     = 0
  end

  return tr
end

function DgnTriggerer:setup()
  self.method = "dgn_event"
end

function DgnTriggerer:added(triggerable)
  if self.type == "item_pickup" then
    -- Automatically move the triggerable if the item we're watching is moved
    -- before it it's picked up.
    local mover = util.copy_table(self)

    mover.type         = "item_moved"
    mover.marker_mover = true

    triggerable:add_triggerer( DgnTriggerer:new(mover) )
  elseif self.type == "turn" then
    if self.countdown then
      self:reset_countdown()
    end
  end
end

function DgnTriggerer:activate(triggerable, marker, x, y)
  if not (triggerable.activated or triggerable.activating) then
    error("DgnTriggerer:activate(): triggerable is not activated")
  end

  if self.type == "item_moved" or self.type == "item_pickup" then
    if self.target == "auto" then
      local items = dgn.items_at(marker:pos())

      if #items == 0 then
        error("No vault item for " .. self.type)
      elseif #items > 1 then
        error("Too many vault items for " .. self.type)
      end
      self.target = items[1].name()
    end
  end

  local et = dgn.dgn_event_type(self.type)

  if (dgn.dgn_event_is_position(et)) then
    if not x or not y then
      x, y = marker:pos()
    end

    dgn.register_listener(et, marker, x, y)
  else
    dgn.register_listener(et, marker)
  end
end

function DgnTriggerer:remove(triggerable, marker)
  if not triggerable.activated then
    return
  end

  local et = dgn.dgn_event_type(self.type)

  if (dgn.dgn_event_is_position(et)) then
    dgn.remove_listener(marker, marker:pos())
  else
    dgn.remove_listener(marker)
  end
end

function DgnTriggerer:event(triggerable, marker, ev)
  -- Use a method-dispatch type mechanism, rather than a series
  -- of "elseif"s.
  DgnTriggerer[self.type](self, triggerable, marker, ev)
end

function DgnTriggerer:monster_dies(triggerable, marker, ev)
  local mid = ev:arg1()
  local mons = dgn.mons_from_mid(mid)

  if not mons then
    error("DgnTriggerer:monster_dies() didn't get a valid mid")
  end

  if self.target == "any" or mons.full_name == self.target or
      (mons.has_prop(self.target) and mons.get_prop(self.target) == self.target) then
    triggerable:do_trigger(self, marker, ev)
  end
end

function DgnTriggerer:feat_change(triggerable, marker, ev)
  if self.target and self.target ~= "" then
    local feat = dgn.feature_name(dgn.grid(ev:pos()))
    if not string.find(feat, self.target) then
      return
    end
  end
  triggerable:do_trigger(self, marker, ev)
end

function DgnTriggerer:item_moved(triggerable, marker, ev)
  local obj_idx = ev:arg1()
  local it      = dgn.item_from_index(obj_idx)

  if not it then
    error("DgnTriggerer:item_moved() didn't get a valid item index")
  end

  if it.name() == self.target then
    if self.marker_mover then
      -- We only exist to move the triggerable if the item moves
      triggerable:move(marker, ev:dest())
    else
      triggerable:do_trigger(self, marker, ev)
    end
  end
end

function DgnTriggerer:item_pickup(triggerable, marker, ev)
  local obj_idx = ev:arg1()
  local it      = dgn.item_from_index(obj_idx)

  if not it then
    error("DgnTriggerer:item_pickup() didn't get a valid item index")
  end

  if it.name() == self.target then
    triggerable:do_trigger(self, marker, ev)
  end
end

function DgnTriggerer:player_move(triggerable, marker, ev)
  triggerable:do_trigger(self, marker, ev)
end

function DgnTriggerer:player_los(triggerable, marker, ev)
  triggerable:do_trigger(self, marker, ev)
end

function DgnTriggerer:door_opened(triggerable, marker, ev)
  triggerable:do_trigger(self, marker, ev)
end

function DgnTriggerer:door_closed(triggerable, marker, ev)
  triggerable:do_trigger(self, marker, ev)
end

function DgnTriggerer:hp_warning(triggerable, marker, ev)
  triggerable:do_trigger(self, marker, ev)
end

function DgnTriggerer:pressure_plate(triggerable, marker, ev)
  triggerable:do_trigger(self, marker, ev)
end

function DgnTriggerer:turn(triggerable, marker, ev)
  if not self.countdown then
    triggerable:do_trigger(self, marker, ev)
    return
  end

  self.countdown = self.countdown - ev:ticks()

  if self.countdown > 0 then
    self.sub_type = "tick"
    triggerable:do_trigger(self, marker, ev)
    return
  end

  self.sub_type = "countdown"

  local countdown_limit = self.countdown_limit or 200
  while self.countdown <= 0 and countdown_limit > 0 do
    countdown_limit = countdown_limit - 1
    triggerable:do_trigger(self, marker, ev)
    self.countdown = self.countdown +
        crawl.random_range(self.delay_min, self.delay_max, 1)
  end

  if self.countdown < 0 then
    self:reset_countdown()
  end
end

function DgnTriggerer:reset_countdown()
  assert(self.type == "turn")

  self.countdown = crawl.random_range(self.delay_min, self.delay_max, 1)
end

function DgnTriggerer:entered_level(triggerable, marker, ev)
  triggerable:do_trigger(self, marker, ev)
end

-------------------

function DgnTriggerer:write(marker, th)
  -- Will always be "dgn_event", so we don't need to save it.
  self.method = nil
  lmark.marshall_table(th, self)
end

function DgnTriggerer:read(marker, th)
  local tr = lmark.unmarshall_table(th)
  setmetatable(tr, DgnTriggerer)
  tr:setup()
  return tr
end
