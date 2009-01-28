---------------------------------------------------------------------------
-- lm_flags.lua
-- Changing level flags and branch flags
--------------------------------------------------------------------------

--------------------------------------------------------------------------
-- There are three different types of pre-packaged "change level and
-- branch flags" marker types.  All three share the following parameters:
--
-- * level_flags: A space separated list of level flag names to set or unset.
--       To unset a flag, prefix the name with "!".
--
-- * branch_flags: Like level_flags, but for branch flags to set or unset.
--
-- * group: Different flag change markers on the same level can be put
--       into the same group by giving them the same group name.
--       Of all of the markers in the same group, only the last one
--       to have its conditions met will cause the flags to change.
--       This is useful if, for example, there are two magical fountains,
--       and you only want the flags to change when both dry up.
--       
-- * msg: A message to give the user when the flags are changed; suppress
--       any messages Crawl would normally give for the changes effected.
--       The message is not given if nothing is changed (i.e., if all
--       the flags to be set were already set and all the flags to be
--       unset were already unset).
--
-- The three different marker types can be created with the following
-- functions:
--
-- * mons_dies_change_flags(): Creates a marker which changes the flags
--        when a named monster dies.  Accepts the parameter "mon_name"
--        as the name of the monster to watch.  The marker can be
--        placed anywhere on the level, and doesn't have to be near the
--        monster when it dies.
--
-- * feat_change_change_flags(): Creates a marker which acts when the
--        feature of its grid changes.  Accepts the optional string
--        parameter "final_feat", which will cause the change to only
--        take place when the changed feature contains final_feat as
--        a substring.  For example, a sparkling fountain can dry up
--        either by turning directly into a dry_fountain_ii feature,
--        or by first turning into a blue_fountain feature, and then
--        into a dry_fountain_i feature.  Without final_feat, the
--        flags will change if the sparkling fountain changes into
--        a blue fountain.  However, if "final_feat" is set to
--        "dry_fountain", the marker will ignore the feature turning
--        into blue_fountain, and will only act when it changes into
--        dry_fountain_i or dry_fountain_ii
--
-- * item_pickup_change_flags(): Creates a marker which acts when
--        an item on its grid gets picked up.  Accepts the parameter
--        "item", which is the plain name of the item its watching
--        (i.e., "Orb of Zot" and "golden rune of Zot" rather than
--         "the Orb of Zot" or "a golden rune of Zot").
--------------------------------------------------------------------------
 
ChangeFlags = { CLASS = "ChangeFlags" }
ChangeFlags.__index = ChangeFlags

function ChangeFlags:_new()
  local cf = { }
  setmetatable(cf, self)
  self.__index = self

  return cf
end

function ChangeFlags:new(pars)
  pars = pars or { }

  pars.level_flags  = pars.level_flags  or ""
  pars.branch_flags = pars.branch_flags or ""
  pars.msg          = pars.msg          or ""

  if pars.level_flags == "" and pars.branch_flags == ""
    and not pars.trigger
  then
    error("Must provide at least one of level_flags, branch_flags, or trigger.")
  end

  local cf = self:_new()
  cf.level_flags  = pars.level_flags
  cf.branch_flags = pars.branch_flags
  cf.msg          = pars.msg
  cf.trigger      = pars.trigger
  cf.props        = { flag_group = pars.group }

  return cf
end

function ChangeFlags:do_change(marker)
  local did_change1 = false
  local did_change2 = false
  local silent      = self.msg and self.msg ~= ""

  if self.trigger then
    self.trigger(marker)
  end

  if self.props.flag_group and self.props.flag_group ~= "" then
    local num_markers = dgn.num_matching_markers("flag_group",
                                                 self.props.group)

    if num_markers > 1 then
      return false, false
    end
  end

  if self.level_flags and self.level_flags ~= "" then
    did_change1 = dgn.change_level_flags(self.level_flags, silent)
  end

  if self.branch_flags and self.branch_flags ~= "" then
    did_change2 = dgn.change_branch_flags(self.branch_flags, silent)
  end

  if did_change1 or did_change2 then
    if self.msg and self.msg ~= "" then
      crawl.mpr(self.smg)
    end

    return true, true
  end
  
  return true, false
end

function ChangeFlags:property(marker, pname)
  return self.props[pname] or ''
end

function ChangeFlags:write(marker, th)
  file.marshall(th, self.level_flags)
  file.marshall(th, self.branch_flags)
  file.marshall(th, self.msg)
  file.marshall_meta(th, self.trigger)
  lmark.marshall_table(th, self.props)
end

function ChangeFlags:read(marker, th)
  self.level_flags  = file.unmarshall_string(th)
  self.branch_flags = file.unmarshall_string(th)
  self.msg          = file.unmarshall_string(th)
  self.trigger      = file.unmarshall_meta(th)
  self.props        = lmark.unmarshall_table(th)
  setmetatable(self, ChangeFlags) 

  return self
end

--------------------------------------------------------------------------
--------------------------------------------------------------------------

MonDiesChangeFlags = ChangeFlags:_new()
MonDiesChangeFlags.__index = MonDiesChangeFlags

function MonDiesChangeFlags:_new(pars)
  local mdcf

  if pars then
    mdcf = ChangeFlags:new(pars)
  else
    mdcf = ChangeFlags:_new()
  end
    
  setmetatable(mdcf, self)
  self.__index = self

  return mdcf
end

function MonDiesChangeFlags:new(pars)
  pars = pars or { }

  if not pars.mon_name then
    error("No monster name provided.")
  end

  local mdcf = self:_new(pars)
  mdcf.mon_name = pars.mon_name

  return mdcf
end

function MonDiesChangeFlags:activate(marker)
  dgn.register_listener(dgn.dgn_event_type('monster_dies'), marker)
end

function MonDiesChangeFlags:event(marker, ev)
  local midx = ev:arg1()
  local mons = dgn.mons_from_index(midx)

  if not mons then
    error("MonDiesChangeFlags:event() didn't get a valid monster index")
  end

  if mons.name == self.mon_name then
    ChangeFlags.do_change(self, marker)
    dgn.remove_listener(marker)
    dgn.remove_marker(marker)
  end
end

function MonDiesChangeFlags:write(marker, th)
  ChangeFlags.write(self, marker, th)
  file.marshall(th, self.mon_name)
end

function MonDiesChangeFlags:read(marker, th)
  ChangeFlags.read(self, marker, th)
  self.mon_name  = file.unmarshall_string(th)
  setmetatable(self, MonDiesChangeFlags) 

  return self
end

function mons_dies_change_flags(pars)
  return MonDiesChangeFlags:new(pars)
end

-----------------------------------------------------------------------------
-----------------------------------------------------------------------------
FeatChangeChangeFlags = ChangeFlags:_new()
FeatChangeChangeFlags.__index = FeatChangeChangeFlags

function FeatChangeChangeFlags:_new(pars)
  local fccf

  if pars then
    fccf = ChangeFlags:new(pars)
  else
    fccf = ChangeFlags:_new()
  end
    
  setmetatable(fccf, self)
  self.__index = self

  return fccf
end

function FeatChangeChangeFlags:new(pars)
  pars = pars or { }

  local fccf = self:_new(pars)

  fccf.final_feat = pars.final_feat

  return fccf
end

function FeatChangeChangeFlags:activate(marker)
  dgn.register_listener(dgn.dgn_event_type('feat_change'), marker,
                        marker:pos())
end

function FeatChangeChangeFlags:event(marker, ev)
  if self.final_feat and self.final_feat ~= "" then
    local feat = dgn.feature_name(dgn.grid(ev:pos()))
    if not string.find(feat, self.final_feat) then
      return
    end
  end

  ChangeFlags.do_change(self, marker)
  dgn.remove_listener(marker, marker:pos())
  dgn.remove_marker(marker)
end

function FeatChangeChangeFlags:write(marker, th)
  ChangeFlags.write(self, marker, th)
  file.marshall(th, self.final_feat)
end

function FeatChangeChangeFlags:read(marker, th)
  ChangeFlags.read(self, marker, th)
  self.final_feat = file.unmarshall_string(th)
  setmetatable(self, FeatChangeChangeFlags) 

  return self
end

function feat_change_change_flags(pars)
  return FeatChangeChangeFlags:new(pars)
end

--------------------------------------------------------------------------
--------------------------------------------------------------------------

ItemPickupChangeFlags = ChangeFlags:_new()
ItemPickupChangeFlags.__index = ItemPickupChangeFlags

function ItemPickupChangeFlags:_new(pars)
  local ipcf

  if pars then
    ipcf = ChangeFlags:new(pars)
  else
    ipcf = ChangeFlags:_new()
  end
    
  setmetatable(ipcf, self)
  self.__index = self

  return ipcf
end

function ItemPickupChangeFlags:new(pars)
  pars = pars or { }

  if not pars.item then
    error("No item name provided.")
  end

  local ipcf = self:_new(pars)
  ipcf.item = pars.item

  return ipcf
end

function ItemPickupChangeFlags:activate(marker)
  dgn.register_listener(dgn.dgn_event_type('item_pickup'), marker,
                        marker:pos())
  dgn.register_listener(dgn.dgn_event_type('item_moved'), marker,
                        marker:pos())
end

function ItemPickupChangeFlags:event(marker, ev)
  local obj_idx  = ev:arg1()
  local it       = dgn.item_from_index(obj_idx)

  if not it then
    error("ItemPickupChangeFlags:event() didn't get a valid item index")
  end

  if item.name(it) == self.item then
    local picked_up = ev:type() == dgn.dgn_event_type('item_pickup')
    if picked_up then
      ChangeFlags.do_change(self, marker)
      dgn.remove_listener(marker, marker:pos())
      dgn.remove_marker(marker)
    else
      dgn.remove_listener(marker, marker:pos())
      marker:move(ev:dest())
      self:activate(marker)
    end
  end
end

function ItemPickupChangeFlags:write(marker, th)
  ChangeFlags.write(self, marker, th)
  file.marshall(th, self.item)
end

function ItemPickupChangeFlags:read(marker, th)
  ChangeFlags.read(self, marker, th)
  self.item  = file.unmarshall_string(th)
  setmetatable(self, ItemPickupChangeFlags) 

  return self
end

function item_pickup_change_flags(pars)
  return ItemPickupChangeFlags:new(pars)
end
