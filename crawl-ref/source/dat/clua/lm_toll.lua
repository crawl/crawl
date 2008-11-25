------------------------------------------------------------------------------
-- lm_toll.lua:
-- One-way toll-stair marker.
------------------------------------------------------------------------------

require("clua/lm_1way.lua")

TollStair = util.subclass(OneWayStair)

function TollStair:new(props)
  local toll = self.super.new(self, props)
  if not props.amount or props.amount < 1 then
    error("Bad toll amount: " .. props.amount)
  end
  return toll
end

function TollStair:activate(marker)
  self.super.activate(self, marker)

  dgn.register_listener(dgn.dgn_event_type("v_leave_level"),
                        marker, marker:pos())
end

function TollStair:event(marker, ev)
  if self.super.event(self, marker, ev) then
    return true
  end

  if ev:type() == dgn.dgn_event_type('v_leave_level') then
    -- Have we enough gold?
    local gold = you.gold()
    local needed = self.props.amount

    if gold < needed then
      crawl.mpr("This portal charges " .. needed .. " gold for entry; " ..
                "you have only " .. gold .. " gold.")
      return false
    end

    -- Ok, ask if the player wants to spend the $$$.
    if not crawl.yesno("This portal charges " .. needed ..
                       " gold for entry. Pay?", true, "n") then
      return false
    end

    -- Gold gold gold! Forget that gold!
    you.gold(you.gold() - needed)
    return true
  end
end

function TollStair:read(marker, th)
  TollStair.super.read(self, marker, th)
  setmetatable(self, TollStair)
  return self
end

function toll_stair(pars)
  return TollStair:new(pars)
end