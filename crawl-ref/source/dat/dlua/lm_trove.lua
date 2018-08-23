------------------------------------------------------------------------------
-- lm_trove.lua:
-- Custom tolled (item) map marker.
--
-- Current only accepts the following:
--  any armour
--  any jewellery
--  any weapon
--  any book
--  any scroll
--  any potion
--  any wand
--  runes
--  the horn of Geryon
--  your piety!
------------------------------------------------------------------------------

crawl_require('dlua/lm_1way.lua')

TroveMarker = util.subclass(OneWayStair)
TroveMarker.CLASS = "TroveMarker"

function TroveMarker:new(props)
  props = props or { }

  local tmarker = self.super.new(self, props)

  if fnum == dgn.feature_number('unseen') then
    error("Bad feature name: " .. feat)
  end

  if not props.toll or props.toll == nil then
    error("Need a toll.")
  end

  if props.toll.item == nil and props.toll.nopiety == nil then
    error("Toll doesn't have any contents!")
  end

  init_item(props.toll.item)

  tmarker.toll = props.toll

  return tmarker
end

function init_item(item)
  if item == nil then
    return
  end

  if item.quantity == nil then
    error("Item needs a quantity.")
  end
  if item.base_type == nil then
    error("Item needs a base type.")
  end
  if item.sub_type == nil then
    error("Item needs a sub type.")
  end
  if item.ego_type == nil or item.ego_type == "" then
    item.ego_type = false
  end
  if item.plus1 == nil then
    item.plus1 = false
  end
  if item.plus2 == nil then
    item.plus2 = false -- #if TAG_MAJOR_VERSION == 34
  end
  if item.artefact_name == nil then
    item.artefact_name = false
  end

  return tmarker
end

function TroveMarker:activate(marker, verbose)
  self.super.activate(self, marker, verbose)
end

function TroveMarker:fdesc_long (marker)
  local state
  local toll = get_toll(self.props)

  if toll.item then
    if self:showing_item() then
      state = "This portal requires the presence of " ..
              self:item_name() .. " to function."
    else
      state = "This portal needs " ..
              self:item_name() .. " to function."
    end
  elseif toll.nopiety then
    state = "The portal is engraved with runes symbolizing the primacy of the "
            .. "material over the divine. Those who enter it may find riches, "
            .. "but when they return, they will find that whatever gods they "
            .. "held dear have forgotten them in their absence."
  else
    error("\nThis portal is very buggy.")
  end
  state = "\n" .. state .. "\n"
  return state
end

function TroveMarker:showing_item (marker)
  local toll = get_toll(self.props)
  if toll.item then
    if toll.item.sub_type == "rune of Zot" then
      return true
    end
  end
  return false
end

function TroveMarker:overview_note (marker)
  local toll = get_toll(self.props)
  if toll.item then
    if self:showing_item() then
      return "show " .. self:item_name(false)
    else
      return "give " .. self:item_name(false)
    end
  elseif toll.nopiety then
    return "lose all piety"
  else
    return "be buggy"
  end
end

function TroveMarker:debug_portal (marker)
  local _x, _y = marker:pos()
  local toll = get_toll(self)
  if crawl.yesno("Try making this Trove use an item (or a different item)?", true, "n") then
    local _x, _y = marker:pos()
    if #dgn.items_at(_x, _y) ~= 0 then
      new_item = dgn.items_at(_x, _y)[1]
      if crawl.yesno("Switch Trove to " .. new_item.name() .. " instead?",
                     true, "n") then
        self.toll = trove.get_trove_toll_with_item(
            trove.get_trove_item(nil, 0, new_item))
        init_item(self.toll.item)
        self.props.toll = self.toll
        crawl.mpr("Switched to a new item.")
      else
        crawl.mpr("Ignoring " .. new_item.name())
      end
    else
      crawl.mpr("To change the item the trove wants, first drop it "
                .. "on the trove tile.")
    end
  elseif crawl.yesno("Make this portal take your piety?", true, "n") then
    self.toll = trove.get_toll_nopiety()
    self.props.toll = self.toll
  else
    self:debug_diag(marker)
  end
end

function TroveMarker:debug_diag (marker)
  local toll = get_toll(self)
  if toll.nopiety then
    crawl.mpr("It's a nopiety trove.")
    return
  end

  if crawl.yesno("Run diagnostic on inventory?", true, "n") then
    self:search_for_rune(marker, nil, true)
    self:search_for_item(marker, nil, items.inventory(), true)
  end

  if crawl.yesno("Run diagonstic on floor?", true, "n") then
    local _x, _y = marker:pos()
    self:search_for_item(marker, nil, dgn.items_at(_x, _y), true)
  end

  local item = toll.item
  if item == nil then
    return "Failed to get item???"
  end

  if item.quantity == nil then item.quantity = false end
  if item.base_type == nil then item.base_type = false end
  if item.sub_type == nil then item.sub_type = false end
  if item.ego_type == nil or item.ego_type == "" then item.ego_type = false end
  if item.plus1 == nil then item.plus1 = false end
  if item.artefact_name == nil then item.artefact_name = false end

  if item.quantity ~= false then
    crawl.mpr("Wanted quantity: " .. item.quantity)
  else
    crawl.mpr("Wanted quantity: nil.")
  end

  if item.base_type ~= false then
    crawl.mpr("Wanted base type: " .. item.base_type)
  else
    crawl.mpr("Wanted base type: nil")
  end

  if item.sub_type ~= false then
    crawl.mpr("Wanted sub type: " .. item.sub_type)
  else
    crawl.mpr("Wanted sub type: nil")
  end

  if item.ego_type ~= false then
    crawl.mpr("Wanted ego type: " .. item.ego_type)
  else
    crawl.mpr("Wanted ego type: nil")
  end

  if item.plus1 ~= false then
    crawl.mpr("Wanted plus1: " .. item.plus1)
  else
    crawl.mpr("Unwanted plus1.")
  end

  if item.artefact_name ~= false then
    crawl.mpr("Wanted artefact name: " .. item.artefact_name)
  else
    crawl.mpr("Unwanted artefact.")
  end

  return "Done. Item name is " .. self:item_name() .. "!"
end

function TroveMarker:property(marker, pname)
  if pname == 'veto_stair' then
    return self:check_veto(marker, pname)
  end

  if pname == "feature_description_long" then
    return self:fdesc_long (marker)
  elseif pname == "overview_note" then
    return self:overview_note (marker)
  end

  if pname == "portal_debug" then
    return self:debug_portal (marker)
  end

  return self.super.property(self, marker, pname)
end

function TroveMarker:describe(marker)
  local toll = get_toll(self)
  if toll.item then
    return "The portal requires " .. self:item_name() .. " for entry.\n"
  elseif toll.nopiety then
    return "Entrants to this portal lose all standing with their god.\n"
  else
    return "This portal is very buggy."
  end
end

function TroveMarker:read(marker, th)
  TroveMarker.super.read(self, marker, th)
  -- self.toll_item never used, but I'm not messing with this marshalling code.
  -- blackcustard May 26 2013.
  toll = lmark.unmarshall_table(th)
  if toll.base_type then
    -- It's a toll_item!
    -- TODO(nrook): Remove when > major version 34
    self.toll = {item = toll}
  else
    self.toll = toll
  end

  setmetatable(self, TroveMarker)
  return self
end

function TroveMarker:write(marker, th)
  TroveMarker.super.write(self, marker, th)
  lmark.marshall_table(th, self.toll)
end

-- We need to implement our own version of item_name because
-- self.props.toll_item is not really an item. It is a table that contains
-- all the important information about what the toll item should be.
function TroveMarker:item_name(do_grammar)
  local item = get_toll(self).item
  if item == nil then
    error("item_name called on a toll without an item", 2)
  end

  local s = ""
  if item.quantity > 1 then
    s = s .. item.quantity
  end

  if item.sub_type == "rune of Zot" then
    if do_grammar == false then
      -- See trove.des where the name of the rune is stuffed into this variable.
      -- The format is "xxx rune of Zot". Not just "xxx".
      -- Where "xxx" is like "slimy".
      return item.ego_type
    else
      return crawl.grammar(item.ego_type, "the")
    end
  end

  if item.sub_type == "horn of Geryon" then
    if do_grammar == false then
      return "horn of Geryon"
    else
      return "the horn of Geryon"
    end
  end

  if item.artefact_name ~= false then
    if string.find(item.artefact_name, "'s") or do_grammar == false then
      return item.artefact_name
    else
      return "the " .. item.artefact_name
    end
  end

  if item.base_type == "weapon" or item.base_type == "armour" then
    if item.plus1 ~= false and item.plus1 ~= nil then
      s = s .. " "
      if item.plus1 > -1 then
        s = s .. "+"
      end
      s = s .. item.plus1
    end
  end

  local jwith_pluses = {"ring of protection", "ring of evasion",
                        "ring of strength", "ring of intelligence",
                        "ring of dexterity", "ring of slaying",
                        "amulet of reflection"}
  if item.base_type == "jewellery" and
     util.contains(jwith_pluses, item.sub_type) then
    s = s .. " +" .. item.plus1
  end

  if item.base_type == "potion" or item.base_type == "scroll" then
    if item.quantity > 1 then
      s = s .. " " .. item.base_type .. "s of"
    else
      s = s .. " " .. item.base_type .. " of"
    end
  elseif item.base_type == "book" then
    books = {"Necronomicon", "tome of Destruction",
             "Young Poisoner's Handbook", "Grand Grimoire"}
    if util.contains(books, item.sub_type) then
      if do_grammar == false then
        return item.sub_type
      else
        return "a " .. item.sub_type
      end
    end
  elseif item.base_type == "wand" then
    s = s .. " wand of"
  end

  s = s .. " " .. item.sub_type

  if item.base_type == "wand" then
    s = s .. " (" .. item.plus1 .. ")"
  end

  if item.base_type == "armour" or item.base_type == "weapon" then
    if item.ego_type then
      s = s .. " of " .. item.ego_type
    end
  end

  s = util.trim(s)

  if string.find(s, "^%d+")
     or string.find(item.sub_type, "dragon scales")
     or do_grammar == false then
    return s
  else
    return crawl.grammar(s, "a")
  end
end

function TroveMarker:search_for_rune(marker, pname, dry_run)
  item = get_toll(self).item
  if (item.base_type ~= "miscellaneous"
      or item.sub_type ~= "rune of Zot") then
    return false
  elseif you.have_rune(item.plus1) then
    if dry_run ~= nil then crawl.mpr("Accepting " .. item.ego_type .. ".") end
    return true
  else
    if dry_run ~= nil then crawl.mpr("No " .. item.ego_type .. ".") end
    return false
  end
end

function TroveMarker:search_for_item(marker, pname, iter_table, dry_run)
  -- Don't early out if our toll is a rune. The debug code can call this
  -- function on the floor, and a wizmode player may have left the rune we
  -- want on the portal.
  if #iter_table == 0 then
    if dry_run ~= nil then crawl.mpr("No items.") end
    return {}
  end

  local acceptable_items = {}

  local item = get_toll(self).item

  for it in iter.invent_iterator:new(iter_table) do
    local iplus1 = it.pluses()
    local this_item = true
    -- For misc items we check plus1 and nothing else.
    if it.base_type == "miscellaneous" then
      if iplus1 ~= false and item.plus1 ~= false and iplus1 ~= item.plus1 then
        -- If this weren't Lua this line could be replaced with "continue".
        this_item = false
      end
    else -- Most of the loop's body is in this block.
    if dry_run ~= nil then crawl.mpr("Checking item: " .. it.name()) end

    if not it.identified("type properties pluses") then
      if dry_run ~= nil then crawl.mpr("Item not identified.") end
      this_item = false
    end

    local jwith_pluses = {"ring of protection", "ring of evasion",
                          "ring of strength", "ring of intelligence",
                          "ring of dexterity", "ring of slaying",
                          "amulet of reflection"}
    if it.base_type == "jewellery" then
      if not util.contains(jwith_pluses, it.sub_type) then
         iplus1 = false
      end
    end

    if iplus1 == false then
      iplus1 = nil
    end

    if iplus1 == nil then
      if item.plus1 ~= false then
        if dry_run ~= nil then crawl.mpr("Nil plus1 when we want plus1.") end
        this_item = false
      end
    end

    -- And don't do anything if the pluses aren't correct, either.
    -- but accept items that are plus'd higher than spec'd.
    if iplus1 ~= nil and item.plus1 ~= false and iplus1 < item.plus1 then
      if dry_run ~= nil then crawl.mpr("Pluses do not match: " .. item.plus1 .. " versus " .. iplus1) end
      this_item = false
    end

    if it.quantity == nil then
      if dry_run ~= nil then crawl.mpr("Quantity is nil.") end
      this_item = false
    elseif item.quantity > 1 and it.quantity < item.quantity then
      if dry_run ~= nil then crawl.mpr("Quantity does not match, want " .. item.quantity .. ", got " .. it.quantity) end
      this_item = false
    end

    if item.artefact_name then
      if item.artefact_name ~= it.artefact_name then
        if dry_run ~= nil then crawl.mpr("Artefact name does not match, want " .. item.artefact_name .. ", got " .. it.artefact_name) end
        this_item = false
      end
    end

    if item.ego_type then
      if item.ego_type ~= it.ego_type then
        if dry_run ~= nil then crawl.mpr("Ego type does not match, want " .. item.ego_type .. ", got " .. it.ego_type) end
        this_item = false
      end
    end
    end -- End of the else block from above.
    -- Now all we need to do is to make sure that the item
    -- is the one we're looking for
    if this_item and item.sub_type == it.sub_type
       and item.base_type == it.base_type then
      if dry_run ~= nil then crawl.mpr("Accepting " .. it.name() .. " and added to queue.") end
      table.insert(acceptable_items, it)
    elseif this_item then
      if dry_run ~= nil then crawl.mpr("Sub and base types do not match.") end
      if dry_run ~= nil then crawl.mpr("Wanted base type " .. item.base_type .. ", got " .. it.base_type) end
      if dry_run ~= nil then crawl.mpr("Wanted sub type " .. item.sub_type .. ", got " .. it.sub_type) end
    end
  end
  return acceptable_items
end

function TroveMarker:item_with_lowest_pluses(marker, pname, dry_run, items)
  local titem = items[1]
  local titem_p1 = titem.pluses()
  --crawl.mpr("Picking " .. titem.name() .. " to start with.")
  --crawl.mpr("This item p1: " .. titem_p1 .. ")

  item = get_toll(self).item

  for _, it in ipairs(items) do
    local this_p1 = it.pluses()
    --crawl.mpr("Looking at " .. it.name())
    if this_p1 < titem_p1 then
      titem = it
      titem_p1 = titem.pluses()
      --crawl.mpr("Picking " .. titem.name() ..
      --          " instead (lesser pluses instad)")
      --crawl.mpr("This item p1: " .. titem_p1 .. ")
    elseif this_p1 == item.plus1 then
      titem = it
      titem_p1 = titem.pluses()
      --crawl.mpr("Picking " .. titem.name() ..
      --          " instead (matches wanted pluses)")
      --crawl.mpr("This item p1: " .. titem_p1 .. ")
    end
  end
  return titem
end

function TroveMarker:plural ()
  item = get_toll(self.props).item
  if get_toll(self).item == nil then
    error("Called plural() on toll without item")
  end

  if item.quantity > 1 then
    return "s"
  else
    return ""
  end
end

function TroveMarker:check_item_veto(marker, pname)
  local quantity = false
  local plus = 0

  local item = get_toll(self.props).item
  -- The message is slightly different for items that aren't actually taken by
  -- the trove (runes, currently).
  if self:showing_item() then
    if not crawl.yesno("This trove requires the presence of "
                       .. self:item_name() .. " to function. Show it the item"
                       .. self:plural() .. "?", true, "n") then
      crawl.mpr("Okay, then.", "prompt")
      return "veto"
    end
  else
    if not crawl.yesno("This trove needs " .. self:item_name() ..
                       " to function. Give it the item" ..
                       self:plural() .. "?", true, "n") then
      crawl.mpr("Okay, then.", "prompt")
      return "veto"
    end
  end

  if item.sub_type == "rune of Zot" then
    -- Begin by checking for runes.
    if self:search_for_rune() then
      crawl.mpr("The portal draws power from the presence of the item"
                .. self:plural() .. " and buzzes to life!")
      self:note_payed("rune", false, item.ego_type)
      return
    else
      crawl.mpr("You don't have " .. self:item_name() .. " with you.")
      return "veto"
    end
  end

  -- Then check for items.
  local acceptable_items = self:search_for_item(marker, pname, items.inventory())

  if #acceptable_items == 0 then
    -- Give a different message for items that are not taken away.
    if self:showing_item() then
      crawl.mpr("You don't have " .. self:item_name() .. " with you.")
    else
      crawl.mpr("You don't have the item" .. self:plural() ..
                " to give! Perhaps you haven't completely identified the item yet?")
    end
    return "veto"
  end

  --crawl.mpr("Total of " .. #acceptable_items .. " acceptable items.")

  -- If there are no pluses but multiple acceptable items, or there is only one
  -- acceptable item, take the first in the list.
  local titem = nil
  if #acceptable_items == 1 or
      (item.plus1 == false) then
    titem = acceptable_items[1]
  else
    titem = self:item_with_lowest_pluses(marker, pname, false, acceptable_items)
  end

  -- Open the portal and maybe consume the item.
  if self:showing_item() then
    crawl.mpr("The portal draws power from the presence of the item" ..
              self:plural() .. " and buzzes to life!")
    self:note_payed(titem, false)
  else
    -- We should not try to take equipped items, there are too many weird edge
    -- cases like distortion.
    if titem.equipped then
      crawl.mpr("You must unequip the item" .. self:plural() .. " first!")
      return "veto"
    end
    self:note_payed(titem, true)
    crawl.mpr("The portal accepts the item" .. self:plural() ..
              " and buzzes to life!")
    titem.dec_quantity(item.quantity)
  end
  return
end

function get_toll(obj)
  -- TODO(nrook): Replace get_toll(self) with self.toll when major version > 34
  if obj.toll then
    return obj.toll
  else
    return {item = obj.toll_item}
  end
end

function TroveMarker:check_veto(marker, pname)
  local toll = get_toll(self.props)
  if toll.item then
    return self:check_item_veto(marker, pname)
  elseif toll.nopiety then
    local yesno_message = (
      "This portal proclaims the superiority of the material over the divine; "
      .. "those who enter it will find they have lost all favour with their "
      .. "chosen deity. Enter anyway?")
    if crawl.yesno(yesno_message, true, "n") then
      self:accept_nopiety()
    else
      return "veto"
    end
  else
    crawl.mpr("This trove is too buggy to enter!")
    return "veto"
  end
end

function TroveMarker:accept_nopiety ()
  if you.god() ~= "No God" then
    local piety_loss = you.piety() - 15
    if piety_loss > 0 then
      crawl.mpr("You feel utterly alone.")
      you.lose_piety(piety_loss)
    end
  end
end

function TroveMarker:note_payed(toll_item, item_taken, rune_name)
  local toll_desc
  if self.props.toll_desc then
    toll_desc = self.props.toll_desc
  else
    toll_desc = "at " .. crawl.article_a(self.props.desc)
  end

  local prefix
  if item_taken then
    prefix = "You paid a toll of "
  else
    prefix = "You showed "
  end

  -- Ugly special case. At this point in the code there is no rune item, so we
  -- can not rely on any of the normal item naming code.
  if toll_item == "rune" then
    crawl.take_note(prefix .. "the " .. rune_name .. " " .. toll_desc)
    return
  end

  -- XXX: The name of self.props.toll_item--the item the trove wants--is wrong,
  -- because the pluses might be wrong. But the name of toll_item--the item the
  -- player gave us--is also wrong, because the quantity might be wrong! So we
  -- need to use a little of both.

  local requested_item = get_toll(self).item
  local target_plus1 = requested_item.plus1
  local real_plus1 = toll_item.pluses()
  requested_item.plus1 = real_plus1

  crawl.take_note(prefix .. self:item_name() .. " " .. toll_desc)

  requested_item.plus1 = target_plus1
end

function trove_marker(pars)
  return TroveMarker:new(pars)
end
