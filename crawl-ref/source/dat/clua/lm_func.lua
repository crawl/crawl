------------------------------------------------------------------------------
-- lm_func.lua:
-- Function execution.
--
-- Calls a specific Lua function when activated; activation of the
-- function can occur when certain conditions are met. See the
-- marker_type parameter as listed below.
--
-- Marker parameters:
--  func: The function to be called.
--  marker_type: Can be one of the following:
--    * "random": Call the function randomly, takes the following parameters
--       "turns_min", "turns_max", and will call every random(min, max) turns.
--    * "turns": Call the function regularly, takes the parameter "turns", and
--       will call the function every "turns".
--    * "in_los": Calls the function whenever the marker is in the
--       player's line of sight. Takes the parameter "repeated", which
--       is "false" by default, meaning that the function is only
--       called once. Passing true means that the function is called
--       every turn that the marker is in LOS.
--    * "player_at": Calls the function whenever the player is at the
--       same position as the marker. Takes the same "repeated"
--       parameter as "in_los".
--    * "listener": A function machine that can be linked into other lua
--       markers and machines. It is not triggered independently, but
--       called by the "parent" marker, though always with the same
--       marker_table parameter as other machines. May take further
--       parameters, see the parent's documentation.
--
--  marker_table: Table to be passed to the function when called.
--  Defaults to {}.
--
-- Specific markers take specific parameters, as listed under marker_type.
--
-- The function will be called with the "marker" object and the table
-- marker_params.
--
------------------------------------------------------------------------------

FunctionMachine = { CLASS = "FunctionMachine" }
FunctionMachine.__index = FunctionMachine

function FunctionMachine:_new()
  local m = { }
  setmetatable(m, self)
  self.__index = self
  return m
end

function FunctionMachine:new(pars)
  if not pars then
    error("No parameters provided")
  end

  if not pars.func then
    error("No function provided")
  end

  local f = function() end
  if type(pars.func) ~= type(f) then
    error("Need an actual function")
  end

  if not pars.marker_type then
    error("No marker_type specified")
  end

  if (pars.marker_type == "random"
      and not (pars.turns_min and pars.turns_max)) then
    error("marker_type:random, but no turns_min or turns_max specified")
  end

  if pars.marker_type == "turns" and not pars.turns then
    error("marker_type:turns, but no turns specified")
  end

  local m = FunctionMachine:_new()

  m.func            = pars.func
  m.marker_type     = pars.marker_type
  m.activated       = false
  m.countdown       = 0

  -- Setting turns is basically the same as setting turns_min and
  -- turns_max to the same value; there's no point duplicating
  -- functionality later.
  m.turns_min       = pars.turns_min        or pars.turns or 0
  m.turns_max       = pars.turns_max        or pars.turns or 0
  m.repeated        = pars.repeated         or false
  m.marker_params   = pars.marker_params    or {}

  -- Some arithmetic to make these make sense in terms of actual turns
  -- versus ticks.
  m.turns_min = m.turns_min * 10
  m.turns_max = m.turns_max * 10

  return m
end

function FunctionMachine:do_function(position, ...)
  local largs = { ... }
  if #largs == 0 then
    self.func(position, self.marker_params)
  else
    self.func(position, self.marker_params, unpack(largs))
  end
end

function FunctionMachine:activate(marker, verbose)
  local _x, _y = marker:pos()
  if (self.marker_type == "listener") then
    return
  else
    dgn.register_listener(dgn.dgn_event_type('turn'), marker)
  end
end

function FunctionMachine:event(marker, ev)
  local x, y = marker:pos()
  if ev:type() == dgn.dgn_event_type('turn') then
    if self.marker_type == "random" or self.marker_type == "turns" then
      self.countdown = self.countdown - ev:ticks()

      while self.countdown <= 0 do
        self:do_function(dgn.point(marker:pos()))
        self.activated = true
        self.countdown = self.countdown +
          crawl.random_range(self.turns_min, self.turns_max, 1)
      end
    elseif self.marker_type == "in_los" then
      if you.see_cell(x, y) then
        if not self.activated or self.repeated then
          self:do_function(dgn.point(marker:pos()))
          self.activated = true
        end
      end
    elseif self.marker_type == "player_at" then
      you_x, you_y = you.pos()
      if you_x == x and you_y == y then
        if not self.activated or self.repeated then
          self:do_function(dgn.point(marker:pos()))
          self.activated = true
        end
      end
    end
  end
end

function FunctionMachine:write(marker, th)
  file.marshall_meta(th, self.func)
  file.marshall_meta(th, self.marker_type)
  file.marshall_meta(th, self.activated)
  file.marshall_meta(th, self.countdown)
  file.marshall_meta(th, self.turns_min)
  file.marshall_meta(th, self.turns_max)
  file.marshall_meta(th, self.repeated)
  lmark.marshall_table(th, self.marker_params)
end

function FunctionMachine:read(marker, th)
  self.func                = file.unmarshall_meta(th)
  self.marker_type         = file.unmarshall_meta(th)
  self.activated           = file.unmarshall_meta(th)
  self.countdown           = file.unmarshall_meta(th)
  self.turns_min           = file.unmarshall_meta(th)
  self.turns_max           = file.unmarshall_meta(th)
  self.repeated            = file.unmarshall_meta(th)
  self.marker_params       = lmark.unmarshall_table(th)

  setmetatable(self, FunctionMachine)

  return self
end

function function_machine(pars)
  return FunctionMachine:new(pars)
end

function message_machine (pars)
  local channel = pars.channel or false
  local mtable = {message = pars.message, channel = pars.channel}
  pars.func = function (position, mtable)
                crawl.mpr(mtable.message, mtable.channel)
              end
  pars.marker_params = mtable
  return FunctionMachine:new(pars)
end
