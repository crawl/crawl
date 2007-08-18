------------------------------------------------------------------------------
-- lm_tmsg.lua:
-- Messaging for timed Lua markers.
------------------------------------------------------------------------------

TimedMessaging = { }
TimedMessaging.__index = TimedMessaging

function TimedMessaging._new()
  local m = { }
  setmetatable(m, TimedMessaging)
  return m
end

function TimedMessaging.new(pars)
  pars = pars or { }
  local m = TimedMessaging._new()
  m.noisemaker = pars.noisemaker
  m.verb       = pars.verb
  m.finalmsg   = pars.finalmsg
  m.ranges     = pars.ranges
  m.initmsg    = pars.initmsg or ''
  return m
end

function TimedMessaging:init(tmarker, cm, verbose)
  local lab = dgn.grid(cm:pos()) == dgn.feature_number('enter_labyrinth')
  if not self.noisemaker then
    self.noisemaker = lab and "an ancient clock" or "a massive bell"
  end

  self.verb = self.verb or (lab and 'ticking' or 'tolling')

  if not self.finalmsg then
    self.finalmsg = lab and "last, dying ticks of the clock"
                        or  "last, dying notes of the bell"
  end

  if not self.ranges then
    self.ranges = { { 5000, 'stately ' }, { 4000, '' },
                    { 2500, 'brisk ' },   { 1500, 'urgent ' },
                    { 0, 'frantic ' } }
  end

  self.check = self.check or tmarker.dur - 500
  if self.check < 50 then
    self.check = 50
  end

  if verbose and #self.initmsg > 0 and you.hear_pos(cm:pos()) then
    crawl.mpr(self.initmsg, "sound")
  end
end

function TimedMessaging:say_message(dur)
  self.sound_channel = self.sound_channel or crawl.msgch_num('sound')
  if dur <= 0 then
    crawl.mpr("You hear the " .. self.finalmsg .. ".", self.sound_channel)
    return
  end
  for _, chk in ipairs(self.ranges) do
    if dur > chk[1] then
      crawl.mpr("You hear the " .. chk[2] .. self.verb 
                .. " of " .. self.noisemaker .. ".",
                self.sound_channel)
      break
    end
  end
end

function TimedMessaging:event(luamark, cmarker, event)
  if luamark.dur < self.check or luamark.dur <= 0 then
    self.check = luamark.dur - 250
    if luamark.dur > 900 then
      self.check = self.check - 250
    end

    if you.hear_pos(cmarker:pos()) then
      self:say_message(luamark.dur)
    end
  end
end

function TimedMessaging:write(th)
  file.marshall(th, self.check)
  file.marshall(th, self.noisemaker)
  file.marshall(th, self.verb)
  file.marshall(th, self.initmsg)
  file.marshall(th, self.finalmsg)
  lmark.marshall_table(th, self.ranges)
end

function TimedMessaging.read(th)
  local tm = TimedMessaging._new()
  tm.check = file.unmarshall_number(th)
  tm.noisemaker = file.unmarshall_string(th)
  tm.verb = file.unmarshall_string(th)
  tm.initmsg = file.unmarshall_string(th)
  tm.finalmsg = file.unmarshall_string(th)
  tm.ranges = lmark.unmarshall_table(th)
  return tm
end

function bell_clock_msg(pars)
  return TimedMessaging.new(pars)
end
