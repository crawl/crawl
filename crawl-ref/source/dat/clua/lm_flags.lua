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
--
-- ChangeFlags is a Triggerable subclass, and the above three functions
-- are just convenience functions which add a single triggerer.  Other
-- triggerers (or more than one) can be added.
--------------------------------------------------------------------------

require('clua/lm_trig.lua')

ChangeFlags       = util.subclass(Triggerable)
ChangeFlags.CLASS = "ChangeFlags"

function ChangeFlags:new(pars)
  pars = pars or { }

  local cf = self.super.new(self)

  pars.level_flags  = pars.level_flags  or ""
  pars.branch_flags = pars.branch_flags or ""
  pars.msg          = pars.msg          or ""

  if pars.level_flags == "" and pars.branch_flags == "" then
    error("Must provide at least one of level_flags or branch_flags.")
  end

  cf.level_flags  = pars.level_flags
  cf.branch_flags = pars.branch_flags
  cf.msg          = pars.msg
  cf.props        = { flag_group = pars.group }

  return cf
end

function ChangeFlags:on_trigger(triggerer, marker, ev)
  local did_change1 = false
  local did_change2 = false
  local silent      = self.msg and self.msg ~= ""

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

  self:remove(marker)

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
  ChangeFlags.super.write(self, marker, th)

  file.marshall(th, self.level_flags)
  file.marshall(th, self.branch_flags)
  file.marshall(th, self.msg)
  lmark.marshall_table(th, self.props)
end

function ChangeFlags:read(marker, th)
  ChangeFlags.super.read(self, marker, th)

  self.level_flags  = file.unmarshall_string(th)
  self.branch_flags = file.unmarshall_string(th)
  self.msg          = file.unmarshall_string(th)
  self.props        = lmark.unmarshall_table(th)

  setmetatable(self, ChangeFlags) 

  return self
end

--------------------------------------------------------------------------
--------------------------------------------------------------------------

function mons_dies_change_flags(pars)
  local mon_name = pars.mon_name or pars.target

  pars.mon_name = nil
  pars.target   = nil

  local cf = ChangeFlags:new(pars)

  cf:add_triggerer(
                    DgnTriggerer:new {
                      type   = "monster_dies",
                      target = mon_name
                    }
                  )

  return cf
end

function feat_change_change_flags(pars)
  local final_feat = pars.final_feat or pars.target

  pars.final_feat = nil
  pars.target     = nil

  local cf = ChangeFlags:new(pars)

  cf:add_triggerer(
                    DgnTriggerer:new {
                      type   = "feat_change",
                      target = final_feat
                    }
                  )

  return cf
end

function item_pickup_change_flags(pars)
  local item = pars.item or pars.target

  pars.item   = nil
  pars.target = nil

  local cf = ChangeFlags:new(pars)

  cf:add_triggerer(
                    DgnTriggerer:new {
                      type   = "item_pickup",
                      target = item
                    }
                  )

  return cf
end
