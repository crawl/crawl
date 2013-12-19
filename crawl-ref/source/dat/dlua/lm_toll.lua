------------------------------------------------------------------------------
-- lm_toll.lua:
-- One-way toll-stair marker.
------------------------------------------------------------------------------

require("dlua/lm_1way.lua")

TollStair = util.subclass(OneWayStair)
TollStair.CLASS = "TollStair"

function TollStair:new(props)
  local toll = OneWayStair.new(self, props)
  if not props.amount or props.amount < 1 then
    error("Bad toll amount: " .. props.amount)
  end
  toll.seen = 0
  return toll
end

function TollStair:activate(marker)
  self.super.activate(self, marker)

  dgn.register_listener(dgn.dgn_event_type("v_leave_level"),
                        marker, marker:pos())
  dgn.register_listener(dgn.dgn_event_type("player_los"),
                        marker, marker:pos())
end

function TollStair:check_shopping_list(marker, first_time)
  local name = "Ziggurat"

  if you.shopping_list_has(name, marker) then
    return
  end

  local ask_name = name
  if first_time then
    ask_name = "" .. self.props.amount .. " gp " .. ask_name
  end

  crawl.redraw_screen()
  if crawl.yesno("Add " .. ask_name .. " to shopping list? (y/N)",
                 true, "n")
  then
    local verb = self.props.shop_verb or "enter"

    you.shopping_list_add(name, verb, self.props.amount, marker)

    crawl.mpr("You can access your shopping list by pressing <w>$</w>.")
  end
end

function TollStair:event(marker, ev)
  if ev:type() == dgn.dgn_event_type('player_los') then
    local x, y = ev:pos()

    -- Only prompt to add to shopping list the first time we are seen.
    if self.seen == 1 then
      return true
    end

    self.seen = 1

    if self.props.amount <= you.gold() then
      return true
    end

    return true
  end

  return self.super.event(self, marker, ev)
end

function TollStair:check_veto(marker, pname)
  -- Have we enough gold?
  local gold = you.gold()
  local needed = self.props.amount

  if gold < needed then
    crawl.mpr("This portal charges " .. needed .. " gold for entry; " ..
              "you have only " .. gold .. " gold.")
    if not you.in_branch("Pan") then
      self:check_shopping_list(marker)
    end
    return "veto"
  end

  if pname == "veto_stair" then
    -- Ok, ask if the player wants to spend the $$$.
    if not crawl.yesno("This portal charges " .. needed ..
                       " gold for entry. Pay?", true, "n") then
      if not you.in_branch("Pan") then
        self:check_shopping_list(marker)
      end
      return "veto"
    end
  elseif pname == "veto_level_change" then
    local name = "Ziggurat"
    if you.shopping_list_has(name, marker) then
      crawl.mpr("Removing " .. name .. " from shopping list.")
      you.shopping_list_del(name, marker)
    end

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

  if pname == 'feature_description_long' then
    return self:feature_description_long(marker)
  end

  return self.super.property(self, marker, pname)
end

function TollStair:feature_description_long(marker)
  return "The portal charges " .. self.props.amount .. " for entry.\n"
end

function TollStair:write(marker, th)
  TollStair.super.write(self, marker, th)
  file.marshall(th, self.seen)
end

function TollStair:read(marker, th)
  TollStair.super.read(self, marker, th)
  self.seen = file.unmarshall_number(th)

  setmetatable(self, TollStair)
  return self
end

function toll_stair(pars)
  return TollStair:new(pars)
end
