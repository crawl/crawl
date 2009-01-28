------------------------------------------------------------------------------
-- lm_toll.lua:
-- One-way toll-stair marker.
------------------------------------------------------------------------------

require("clua/lm_1way.lua")

TollStair = util.subclass(OneWayStair)
TollStair.CLASS = "TollStair"

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

function TollStair:check_veto(marker, pname)
  -- Have we enough gold?
  local gold = you.gold()
  local needed = self.props.amount

  if gold < needed then
    crawl.mpr("This portal charges " .. needed .. " gold for entry; " ..
              "you have only " .. gold .. " gold.")
    return "veto"
  end

  if pname == "veto_stair" then
    -- Ok, ask if the player wants to spend the $$$.
    if not crawl.yesno("This portal charges " .. needed ..
                       " gold for entry. Pay?", true, "n") then
      return "veto"
    end
  elseif pname == "veto_level_change" then
    -- Gold gold gold! Forget that gold!
    you.gold(you.gold() - needed)

    local toll_desc
    if self.props.toll_desc then
        toll_desc = self.props.toll_desc
    else
        toll_desc = "at " .. crawl.article_a(self.props.desc)
    end
    crawl.take_note("You paid a toll of " .. needed .. " gold " ..
                    toll_desc .. ".")
    return
  end
end

function TollStair:property(marker, pname)
  if pname == 'veto_stair' or pname == 'veto_level_change' then
    return self:check_veto(marker, pname)
  end

  return self.super.property(self, marker, pname)
end

function TollStair:read(marker, th)
  TollStair.super.read(self, marker, th)
  setmetatable(self, TollStair)
  return self
end

function toll_stair(pars)
  return TollStair:new(pars)
end
