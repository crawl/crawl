------------------------------------------------------------------------------
-- lm_timed.lua:
-- Lua timed map feature markers.
------------------------------------------------------------------------------

dofile('clua/lm_tmsg.lua')

TimedMarker = PortalDescriptor:new()
TimedMarker.__index = TimedMarker

function TimedMarker:_new()
  local marker = { }
  setmetatable(marker, self)
  self.__index = self
  return marker
end

function TimedMarker:new(pars)
  pars = pars or { }
  if not pars.msg then
    error("No messaging object provided (msg = nil)")
  end
  pars.high = pars.high or pars.low or pars.turns or 1
  pars.low  = pars.low or pars.high or pars.turns or 1
  local dur = crawl.random_range(pars.low, pars.high, pars.navg or 1)
  local feat = pars.feat or 'floor'
  local fnum = dgn.feature_number(feat)
  if fnum == dgn.feature_number('unseen') then
    error("Bad feature name: " .. feat)
  end

  local tmarker     = self:_new()
  tmarker.dur       = dur * 10
  tmarker.fnum      = fnum
  tmarker.feat      = feat
  tmarker.msg       = pars.msg
  tmarker.disappear = pars.disappear

  if pars.props then
    tmarker.props = pars.props
  end
  return tmarker
end

function TimedMarker:activate(marker)
  self.msg:init(self, marker)

  dgn.register_listener(dgn.dgn_event_type('turn'), marker)
  dgn.register_listener(dgn.dgn_event_type('player_climb'), 
                        marker, marker:pos())
end

function TimedMarker:timeout(marker, verbose, affect_player)
  local x, y = marker:pos()
  if verbose then
    if you.see_grid(marker:pos()) then
      crawl.mpr( self.disappear or 
                 dgn.feature_desc_at(x, y, "The") .. " disappears!")
    else
      crawl.mpr("The walls and floor vibrate strangely for a moment.")
    end
  end
  dgn.terrain_changed(x, y, self.fnum, affect_player)
  dgn.remove_listener(marker)
  dgn.remove_listener(marker, marker:pos())
  dgn.remove_marker(marker)
end

function TimedMarker:event(marker, ev)
  self.ticktype = self.ticktype or dgn.dgn_event_type('turn')
  self.stairtype = self.stairtype or dgn.dgn_event_type('player_climb')

  if ev:type() == self.stairtype then
    local mx, my = marker:pos()
    local ex, ey = ev:pos()
    if mx == ex and my == ey then
      self:timeout(marker, false, false)
    end
    return
  end

  if ev:type() ~= self.ticktype then
    return
  end
  self.dur = self.dur - ev:ticks()
  self.msg:event(self, marker, ev)
  if self.dur <= 0 then
    self:timeout(marker, true, true)
  end
end

function TimedMarker:describe(marker)
  return self.feat .. "/" .. tostring(self.dur)
end

function TimedMarker:read(marker, th)
  PortalDescriptor.read(self, marker, th)
  self.dur = file.unmarshall_number(th)
  self.fnum = file.unmarshall_number(th)
  self.feat = file.unmarshall_string(th)
  self.disappear = file.unmarshall_meta(th)
  self.msg  = file.unmarshall_fn(th)(th)
  setmetatable(self, TimedMarker)
  return self 
end

function TimedMarker:write(marker, th)
  PortalDescriptor.write(self, marker, th)
  file.marshall(th, self.dur)
  file.marshall(th, self.fnum)
  file.marshall(th, self.feat)
  file.marshall_meta(th, self.disappear)
  file.marshall(th, self.msg.read)
  self.msg:write(th)
end

function timed_marker(pars)
  return TimedMarker:new(pars)
end
