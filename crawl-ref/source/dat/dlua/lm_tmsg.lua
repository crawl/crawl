------------------------------------------------------------------------------
-- lm_tmsg.lua:
-- Messaging for timed Lua markers.
------------------------------------------------------------------------------

TimedMessaging = { CLASS = "TimedMessaging" }
TimedMessaging.__index = TimedMessaging

function TimedMessaging:new(m, nocheck)
  m = m or { }
  setmetatable(m, self)
  self.__index = self

  if not nocheck then
    if not m.messages then
      assert(m.noisemaker, "No noisemaker specified")
      assert(m.verb, "No verb specified")
    end

    if m.visible and not m.messages then
      error("No messages set for timer messager")
    end
  end

  return m
end

function TimedMessaging:channel()
  if not self.sound_channel then
    self.sound_channel =
      crawl.msgch_num(self.visible and 'default' or 'timed_portal')
  end
  return self.sound_channel
end

function TimedMessaging:init(tmarker, cm, verbose)
  self.entity = self.entity or tmarker.props.entity or tmarker.props.desc

  if not self.ranges and not self.visible and not self.messages then
    self.ranges = {
      { 5000, 'stately ' }, { 4000, '' },
      { 2500, 'brisk ' },   { 1500, 'urgent ' },
      { 0, 'frantic ' }
    }
  end

  if not self.range_adjectives then
    self.range_adjectives = {
      { 28, 'very distant' },
      { 21, 'distant' },
      { 14, '$F nearby' },
      { 7,  '$F very nearby' },
      { 0,  '$F' }
    }
  end

  self.check = self.check or tmarker.dur - 500
  if self.check < 50 then
    self.check = 50
  end

  if self.messages then
    for _, message in ipairs(self.messages) do
      local mes = message[2]
      if string.find(mes, "$F") then
        mes = util.expand_entity(self.entity, mes)
      end
    end
  end

  self._have_entered_level = false
  if verbose and self.initmsg then
    self:emit_message(cm, self.initmsg)
  end
end

function TimedMessaging:perceptible(cm)
  if not cm then
    return true
  end

  if self.visible then
    return you.see_cell(cm:pos())
  else
    return you.hear_pos(cm:pos())
  end
end

function TimedMessaging:emit_message(cm, msg)
  if not msg or not self:perceptible(cm) then
    return
  end

  if type(msg) == 'table' then
    util.foreach(msg,
                 function (m)
                   self:emit_message(cm, m)
                 end)
  else
    if #msg < 1 then
      return
    end

    crawl.mpr(util.expand_entity(self.entity, msg), self:channel())
  end
end

function TimedMessaging:proc_ranges(ranges, dur, fn)
  if not ranges then
    return
  end
  for _, chk in ipairs(ranges) do
    if dur > chk[1] then
      fn(chk, dur)
      break
    end
  end
end

function TimedMessaging:player_distance(cm)
  local cx, cy = cm:pos()
  local x, y = you.pos()
  local dx, dy = cx - x, cy - y
  return math.max(math.abs(dx), math.abs(dy))
end

function TimedMessaging:choose_range_adjective(distance)
  for _, dadj in ipairs(self.range_adjectives) do
    if distance >= dadj[1] then
      return dadj[2]
    end
  end
end

function TimedMessaging:range_adjective(cm, thing)
  local adj = self:choose_range_adjective(self:player_distance(cm))
  if string.find(adj, '$F') then
    return util.expand_entity(self.noisemaker, adj)
  else
    return crawl.article_a(#adj == 0 and self.noisemaker
                           or adj .. ' ' .. self.noisemaker)
  end
end

function TimedMessaging:say_message(cm, dur)
  if dur <= 100 then
    self:emit_message(nil, self.finalmsg)
    return
  end

  local noisemaker =
    self.noisemaker and self:range_adjective(cm, self.noisemaker)

  self:proc_ranges(self.ranges, dur,
                   function (chk)
                     self:emit_message(nil,
                                       "You hear the " .. chk[2] .. self.verb
                                       .. " of " .. noisemaker .. ".")
                   end)

  self:proc_ranges(self.messages, dur,
                   function (chk)
                     self:emit_message(nil, chk[2])
                   end)
end

function TimedMessaging:event(luamark, cmarker, event)
  if event:type() == dgn.dgn_event_type('entered_level') then
    self._have_entered_level = true
  elseif not self._have_entered_level then
    return
  end
  if luamark.dur < self.check or luamark.dur <= 100 and self.check > -150 then
    self.check = luamark.dur - 250
    if luamark.dur > 900 then
      self.check = self.check - 250
    end

    if self:perceptible(cmarker) then
      self:say_message(cmarker, luamark.dur)
    end
  end
end

function TimedMessaging:write(th)
  lmark.marshall_table(th, self)
end

function TimedMessaging:read(marker, th)
  return TimedMessaging:new(lmark.unmarshall_table(th))
end

function timed_msg(pars)
  return TimedMessaging:new(pars)
end

-- Accepts pairs of turns and messages used for a timer. For instance
-- timer_interval_messages(500, 'You feel vaguely uneasy.',
--                         250, 'You feel extremely uneasy.',
--                         100, 'You feel a primal terror.')
-- Will produce the first message when the timer has > 500 turns, the
-- second message when the timer has <= 500 turns and >250 turns, and so on.
-- Note that any given interval message will be repeated, usually every 50
-- or 25 turns.
function timer_interval_messages(...)
  local breakpoints = util.partition({ ... }, 2)
  -- Expand turn breakpoints into tenths of a turn.
  util.foreach(breakpoints,
               function (brk)
                 brk[1] = brk[1] * 10
               end)
  return breakpoints
end

-- Accepts timer messages as with timer_interval_messages, but with no
-- explicit intervals. Instead, the total turn count is divided by the
-- number of messages to determine intervals.
function time_messages(total_turns, ...)
  local messages = { ... }

  local n = #messages

  -- Each interval is 1.2 * the previous (lower) interval.
  local inflate = 1.2

  local function power_sum(n)
    local sum = 1
    for i = 1, n - 1 do
      sum = sum + inflate ^ i
    end
    return sum
  end

  local base_interval = total_turns / power_sum(n)

  local res = { }
  for i = 1, n - 1 do
    local pow = n - i
    total_turns = total_turns - base_interval * inflate ^ pow
    table.insert(res, { math.floor(10 * total_turns), messages[i] } )
  end
  table.insert(res, { 0, messages[n] })
  return res
end
