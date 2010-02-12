------------------------------------------------------------------------------
-- lm_trove.lua:
-- Custom (temporarily) timed, and tolled (gold or item) map marker.
--  Basically just a combination of TimedMarker and TollStair, with added veto
--  checks against items.
------------------------------------------------------------------------------

require('clua/lm_tmsg.lua')
require('clua/lm_1way.lua')

TroveMarker = util.subclass(OneWayStair)
TroveMarker.CLASS = "TroveMarker"

function TroveMarker:new(props)
  -- On top of the normal Toll stair (the 'amount' value), this also accepts
  -- secondary_amount and toll_item.
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

  if not props.amount or props.amount < 1 then
    error("Bad toll amount: " .. props.amount)
  end

  if not props.secondary_amount or props.secondary_amount < 1 then
    error("Bad second toll amount: " .. props.secondary_amount)
  end

  if not props.toll_item or props.toll_item == nil then
    error("Need a toll item.")
  end

  tmarker.seen = false
  tmarker.no_timeout = false
  tmarker.toll_item = props.toll_item

  props.msg = nil

  return tmarker
end

function TroveMarker:activate(marker, verbose)
  self.super.activate(self, marker, verbose)
  self.msg:init(self, marker, verbose)
  dgn.register_listener(dgn.dgn_event_type('turn'), marker)
  dgn.register_listener(dgn.dgn_event_type("v_leave_level"),
                        marker, marker:pos())
  dgn.register_listener(dgn.dgn_event_type("player_los"),
                        marker, marker:pos())
end

function TroveMarker:property(marker, pname)
  if pname == 'veto_stair' or pname == 'veto_level_change' then
    return self:check_veto(marker, pname)
  end

  if pname == 'feature_description_long' then
    return self:feature_description_long(marker)
  end

  return self.super.property(self, marker, pname)
end

function TroveMarker:disappear(marker, affect_player, x, y)
  dgn.remove_listener(marker)
  self.super.disappear(self, marker, affect_player, x, y)
end

function TroveMarker:timeout(marker, verbose, affect_player)
  local x, y = marker:pos()

  if you.pos() == marker:pos() and you.taking_stairs() then
    if verbose then
      crawl.mpr( dgn.feature_desc_at(x, y, "The") .. " vanishes " ..
                "just as you enter it!")
      return
    end
  end

  if verbose then
    if you.see_cell(marker:pos()) then
      crawl.mpr( util.expand_entity(self.props.entity, self.props.disappear) or
                 dgn.feature_desc_at(x, y, "The") .. " disappears!")
    else
      crawl.mpr("The walls and floor vibrate strangely for a moment.")
    end
  end

  self:disappear(marker, affect_player, x, y)
end

function TroveMarker:event(marker, ev)
  local x, y = ev:pos()
  if ev:type() == dgn.dgn_event_type('player_los') and self.seen ~= true then
    self.seen = true
  end

  if self.super.event(self, marker, ev) then
    return true
  end

  self.ticktype = self.ticktype or dgn.dgn_event_type('turn')

  if ev:type() ~= self.ticktype then
    return
  end

  if self.no_timeout then
    return
  end

  self.dur = self.dur - ev:ticks()
  self.msg:event(self, marker, ev)
  if self.dur <= 0 then
    self:timeout(marker, true, true)
    return true
  end
end

function TroveMarker:describe(marker)
  local desc = self:unmangle(self.props.desc_long)
  if desc then
    desc = desc .. "\n\n"
  else
    desc = ""
  end
  return desc .. "The portal charges " .. self.props.amount .. " for entry.\n"
end

function TroveMarker:read(marker, th)
  TroveMarker.super.read(self, marker, th)
  self.seen = file.unmarshall_meta(th)
  self.no_timeout = file.unmarshall_meta(th)
  self.toll_item = lmark.unmarshall_table(th)
  self.dur  = file.unmarshall_number(th)
  self.msg  = file.unmarshall_fn(th)(th)

  setmetatable(self, TroveMarker)
  return self
end

function TroveMarker:write(marker, th)
  TroveMarker.super.write(self, marker, th)
  file.marshall_meta(th, self.seen)
  file.marshall_meta(th, self.no_timeout)
  lmark.marshall_table(th, self.toll_item)
  file.marshall(th, self.dur)
  file.marshall(th, self.msg.read)
  self.msg:write(th)
end

function TroveMarker:item_name()
  local item = self.toll_item
  local s = ""
  if item.quantity > 1 then
    s = s .. item.quantity
  end

  if item.artefact_name ~= false then
    if string.find(item.artefact_name, "'s") then
      return item.artefact_name
    else
      return "the " .. item.artefact_name
    end
  end

  if item.base_type == "weapon" or item.base_type == "armour" then
    if item.plus1 ~= false then
      s = s .. " "
      if item.plus1 > -1 then
        s = s .. "+"
      end
      s = s .. item.plus1
    end

    if item.plus2 ~= false then
      if item.plus1 ~= item.plus2 and item.base_type ~= "armour" then
        s = s .. ", +"
        if item.plus2 > -1 then
          s = s .. item.plus2
        end
      end
    end
  end

  if item.base_type == "potion" or item.base_type == "scroll" then
    s = s .. " " .. item.base_type .. "s of"
  elseif item.base_type == "book" then
    s = s .. " book of"
  end

  s = s .. " " .. item.sub_type

  if item.base_type == "armour" or item.base_type == "weapon" then
    if item.ego_type then
      s = s .. " of " .. item.ego_type
    end
  end

  s = util.trim(s)

  if string.find(s, "^%d+") then
    return s
  else
    return crawl.grammar(s, "a")
  end
end

-- Returns true if the item was found and "used", false
-- if we didn't find anything.
function TroveMarker:check_item(marker, pname, position)
  local iter_table

  if position == "inventory" then
    iter_table = items.inventory()
  else
    iter_table = dgn.items_at(position.x, position.y)
  end

  if #iter_table == 0 then
    return false
  end

  local item = self.toll_item

  for it in iter.invent_iterator:new(iter_table) do
    local iplus1, iplus2 = it.pluses()
    local this_item = true

    if not it.identified() then
      this_item = false
    end

    if it.artefact == true and item.artefact_name == false then
      this_item = false
    end

    if iplus1 == false then
      iplus1 = nil
    end

    if iplus2 == false then
      iplus2 = nil
    end

    if iplus1 == nil or iplus2 == nil then
      if item.plus1 ~= false or item.plus2 ~= false then
        this_item = false
      end
    end

    -- And don't do anything if the pluses aren't correct, either.
    -- but accept items that are plus'd higher than spec'd.
    if iplus1 ~= nil and item.plus1 ~= false and iplus1 < item.plus1 then
      this_item = false
    elseif iplus2 ~= nil and item.plus2 ~= false and iplus2 < item.plus2 then
      this_item = false
    end

    if it.quantity == nil then
      this_item = false
    elseif item.quantity > 1 and it.quantity < item.quantity then
      this_item = false
    end

    if item.artefact_name then
      if item.artefact_name ~= it.artefact_name then
        this_item = false
      end
    end

    if item.ego_type then
      if item.ego_type ~= it.ego_type then
        this_item = false
      end
    end

    -- Now all we need to do is to make sure that the item is the one we're looking for
    if this_item and item.sub_type == it.sub_type and item.base_type == it.base_type then
      crawl.mpr("The portal accepts the item" .. self:plural() .. " and buzzes to life!")
      it.dec_quantity(item.quantity)
      return true
    end
  end
end

function TroveMarker:plural ()
  if self.toll_item.quantity > 1 then
    return "s"
  else
    return ""
  end
end

function TroveMarker:check_veto(marker, pname)
  -- Have we enough gold?
  local gold = you.gold()
  local needed = self.props.amount
  local needed2 = self.props.secondary_amount
  local quantity = false
  local plus = 0

  if gold < needed2 and self.no_timeout == false then
    crawl.mpr("This portal charges a minimum of " .. needed2 .. " gold for entry; " ..
              "you have only " .. gold .. " gold.")
    return "veto"
  end

  local _x, _y = marker:pos()

  if pname == "veto_stair" then
    -- Let's see if they want to pay the smaller amount
    if self.no_timeout == true then
      -- Let's check to see if there's an item at our current position
      if crawl.yesno("This trove needs " .. self:item_name() .. " to function. Give it the item" .. self:plural() .. "?", true, "n") then
        if self:check_item(marker, pname, dgn.point(_x, _y)) == true then
          return
        else
          if self:check_item(marker, pname, "inventory") == true then
            self:note_payed(self:item_name())
            return
          else
            crawl.mpr("You don't have the item" .. self:plural() .. " to give!")
            return "veto"
          end
        end
      end
      return "veto"
    else
      crawl.mpr("There is a foul-smelling imp just inside the portal. It says, " ..
                "\"I can lock this portal here for " .. needed2 .."gp!\".")
      if crawl.yesno("Pay? (you can refuse and instead pay the full fee to enter now)", true, "n") then
        self.no_timeout = true
        you.gold(you.gold() - needed2)
        self:note_payed(needed2 .. "gp", true)
        crawl.mpr("The imp fiddles with the portal, frowns, says, \"I think I broke it!\" and then disappears in a puff of smoke.")
        return "veto"
      else
        crawl.mpr("The foul-smelling imp curses your mother!")
      end

      -- Ok, ask if the player wants to spend the $$$.
      if crawl.yesno("You can still enter the portal, but it charges " .. needed ..
                         "gp for entry. Pay?", true, "n") then
        you.gold(you.gold() - needed)
        self:note_payed(needed .. "gp")
      else
        return "veto"
      end
    end
  end
end

function TroveMarker:note_payed(amount, lock)
  local toll_desc
  if self.props.toll_desc then
      toll_desc = self.props.toll_desc
  else
      toll_desc = "at " .. crawl.article_a(self.props.desc)
  end

  local tdesc = "toll"

  if lock ~= nil then
    tdesc = "fee"
    if self.props.lock_desc then
      toll_desc = self.props.lock_desc
    end
  end


  crawl.take_note("You paid a " .. tdesc .. " of " .. amount .. " " ..
                  toll_desc .. ".")
end

function trove_marker(pars)
  return TroveMarker:new(pars)
end
