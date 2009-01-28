------------------------------------------------------------------------------
-- lm_timed.lua:
-- Lua timed map feature markers.
------------------------------------------------------------------------------

require('clua/lm_tmsg.lua')
require('clua/lm_1way.lua')

TimedMarker = util.subclass(OneWayStair)
TimedMarker.CLASS = "TimedMarker"

function TimedMarker:new(props)
  props = props or { }

  local tmarker = self.super.new(self, props)

  if not props.msg then
    error("No messaging object provided (msg = nil)")
  end

  props.high = props.high or props.low or props.turns or 1
  props.low  = props.low or props.high or props.turns or 1

  local dur = crawl.random_range(props.low, props.high, props.navg or 1)
  if fnum == dgn.feature_number('unseen') then
    error("Bad feature name: " .. feat)
  end

  tmarker.dur = dur * 10
  tmarker.msg = props.msg

  props.msg = nil

  return tmarker
end

function TimedMarker:activate(marker, verbose)
  self.super.activate(self, marker, verbose)
  self.msg:init(self, marker, verbose)
  dgn.register_listener(dgn.dgn_event_type('turn'), marker)
end

function TimedMarker:disappear(marker, affect_player, x, y)
  dgn.remove_listener(marker)
  self.super.disappear(self, marker, affect_player, x, y)
end

function TimedMarker:timeout(marker, verbose, affect_player)
  local x, y = marker:pos()

  if you.pos() == marker:pos() and you.taking_stairs() then
    if verbose then
      crawl.mpr( dgn.feature_desc_at(x, y, "The") .. " vanishes " ..
                "just as you enter it!")
      return
    end
  end

  if verbose then
    if you.see_grid(marker:pos()) then
      crawl.mpr( util.expand_entity(self.props.entity, self.props.disappear) or
                 dgn.feature_desc_at(x, y, "The") .. " disappears!")
    else
      crawl.mpr("The walls and floor vibrate strangely for a moment.")
    end
  end

  self:disappear(marker, affect_player, x, y)
end

function TimedMarker:event(marker, ev)
  if self.super.event(self, marker, ev) then
    return true
  end

  self.ticktype = self.ticktype or dgn.dgn_event_type('turn')

  if ev:type() ~= self.ticktype then
    return
  end

  self.dur = self.dur - ev:ticks()
  self.msg:event(self, marker, ev)
  if self.dur <= 0 then
    self:timeout(marker, true, true)
    return true
  end
end

function TimedMarker:describe(marker)
  local feat = self.props.floor or 'floor'
  return feat .. "/" .. tostring(self.dur)
end

function TimedMarker:read(marker, th)
  TimedMarker.super.read(self, marker, th)
  self.dur = file.unmarshall_number(th)
  self.msg  = file.unmarshall_fn(th)(th)
  setmetatable(self, TimedMarker)
  return self
end

function TimedMarker:write(marker, th)
  TimedMarker.super.write(self, marker, th)
  file.marshall(th, self.dur)
  file.marshall(th, self.msg.read)
  self.msg:write(th)
end

function timed_marker(pars)
  return TimedMarker:new(pars)
end
