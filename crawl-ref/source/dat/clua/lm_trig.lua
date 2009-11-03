------------------------------------------------------------------------------
-- lm_trig.lua:
-- DgnTriggerers and triggerables:
--
-- This is similar to the overvable/observer design pattern: a triggerable
-- class which does something and a triggerrer which sets it off.  As an
-- example, the ChangeFlags class (clua/lm_flags.lua), rather than having
-- three subclasses (for monster death, feature change and item pickup)
-- needs no subclasses, but is just one class which is given triggerers
-- which listen for different events.  Additionally, new types of triggerers
-- can be developed and used without have to update the ChangeFlags code.
--
-- Unlike with the overvable/observer design pattern, each triggerer is
-- associated with a signle triggerable, rather than there being one observable
-- and multiple observers, since each triggerer might have a data payload which
-- is meant to be different for each triggerable.
--
-- A triggerable class just needs to subclass Triggerable and define an
-- "on_trigger" method.
------------------------------------------------------------------------------

Triggerable = { CLASS = "Triggerable" }
Triggerable.__index = Triggerable

function Triggerable:new()
  local tr = { }
  setmetatable(tr, self)
  self.__index = self

  tr.triggerers        = { }
  tr.dgn_trigs_by_type = { }

  return tr
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
    error("Trigerrable already removed")
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
    error("Can't activate, trigerrable removed")
  end

  if self.activating then
    error("Triggerable already activating")
  end

  if self.activated then
    error("Triggerable already activated")
  end

  self.activating = true
  for _, trig in ipairs(self.triggerers) do
    trig:activate(self, marker)
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

function Triggerable:write(marker, th)
  file.marshall(th, #self.triggerers)
  for _, trig in ipairs(self.triggerers) do
    -- We'll be handling the de-serialization of the triggerer, so we need to
    -- save the class name.
    file.marshall(th, getmetatable(trig).CLASS)
    trig:write(marker, th)
  end

  lmark.marshall_table(th, self.dgn_trigs_by_type)
end

function Triggerable:read(marker, th)
  self.triggerers = {}

  local num_trigs = file.unmarshall_number(th)
  for i = 1, num_trigs do
    -- Hack to let triggerer classes de-serialize themselves.
    local trig_class    = file.unmarshall_string(th)

    -- _G is the global symbol table, and the class name of the triggerer is
    -- the name of that class's class object
    local trig_table = _G[trig_class].read(nil, marker, th)
    table.insert(self.triggerers, trig_table)
  end

  self.dgn_trigs_by_type = lmark.unmarshall_table(th)

  setmetatable(self, Triggerable)
  return self
end

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

function message_at_spot(msg, channel, repeated)
  local tm = TriggerableMessage:new 
      { msg = msg, channel = channel, repeated = repeated }

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
--       we're wating for.  Doesn't matter where the triggerable/marker
--       is placed.
--
-- * feat_change: Waits for a cell's feature to change.  Accepts the
--       optional parameter "target", which if set delays the trigger
--       until the feature the cell turns into contains the target as a
--       substring.  The triggerable/marker must be placed on top of the
--       cell who's feature you wish to monitor.
--
-- * item_moved: Wait for an item to move from one cell to another.
--      Needs the parameter "target", who's value is the name of the
--      item that is being tracked.  The triggerable/marker must be placed
--      on top of the cell containing the item.
--
-- * item_pickup: Wait for an item to be picked up.  Needs the parameter
--      "target", who's value is the name of the item that is being tracked.
--      The triggerable/marker must be placed on top of the cell containing
--      the item.  Automatically takes care of the item moving from one
--      square to another without being picked up.
--
-- * player_move: Wait for the player to move to a cell.  The
--      triggerable/marker must be placed on top of cell in question.
--
-- * player_los: Wait for the player to come into LOS of a cell, which
--      must contain a notable feature..  The triggerable/marker must be
--      placed on top of cell in question.

DgnTriggerer = { CLASS = "DgnTriggerer" }
DgnTriggerer.__index = DgnTriggerer

function DgnTriggerer:new(pars)
  pars = pars or {}

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
  end
end

function DgnTriggerer:activate(triggerable, marker)
  if not (triggerable.activated or triggerable.activating) then
    error("DgnTriggerer:activate(): triggerable is not activated")
  end

  local et = dgn.dgn_event_type(self.type)

  if (dgn.dgn_event_is_position(et)) then
    dgn.register_listener(et, marker, marker:pos())
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
  local midx = ev:arg1()
  local mons = dgn.mons_from_index(midx)

  if not mons then
    error("DgnTriggerer:monster_dies() didn't get a valid monster index")
  end

  if mons.name == self.target then
    triggerable:on_trigger(self, marker, ev)
  end
end

function DgnTriggerer:feat_change(triggerable, marker, ev)
  if self.target and self.target ~= "" then
    local feat = dgn.feature_name(dgn.grid(ev:pos()))
    if not string.find(feat, self.target) then
      return
    end
  end
  triggerable:on_trigger(self, marker, ev)
end

function DgnTriggerer:item_moved(triggerable, marker, ev)
  local obj_idx = ev:arg1()
  local it      = dgn.item_from_index(obj_idx)

  if not it then
    error("DgnTriggerer:item_moved() didn't get a valid item index")
  end

  if item.name(it) == self.target then
    if self.marker_mover then
      -- We only exist to move the triggerable if the item moves
      triggerable:move(marker, ev:dest())
    else
      triggerable:on_trigger(self, marker, ev)
    end
  end
end

function DgnTriggerer:item_pickup(triggerable, marker, ev)
  local obj_idx = ev:arg1()
  local it      = dgn.item_from_index(obj_idx)

  if not it then
    error("DgnTriggerer:item_pickup() didn't get a valid item index")
  end

  if item.name(it) == self.target then
    triggerable:on_trigger(self, marker, ev)
  end
end

function DgnTriggerer:player_move(triggerable, marker, ev)
  triggerable:on_trigger(self, marker, ev)
end

function DgnTriggerer:player_los(triggerable, marker, ev)
  triggerable:on_trigger(self, marker, ev)
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
