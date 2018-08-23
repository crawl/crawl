-------------------------------------------------------------------------------
-- lm_door.lua
--  Markers for doors.
-------------------------------------------------------------------------------

-------------------------------------------------------------------------------
-- This marker can be applied to a door to prevent monsters from opening it,
-- unless the door has already previously been opened by the player.
-------------------------------------------------------------------------------

crawl_require('dlua/lm_trig.lua')

RestrictDoor       = util.subclass(Triggerable)
RestrictDoor.CLASS = "RestrictDoor"

function RestrictDoor:new(props)
  props = props or { }

  local rd = self.super.new(self)

  props.door_restrict = "veto"
  rd.props = props

  return rd
end

function RestrictDoor:write(marker, th)
  RestrictDoor.super.write(self, marker, th)
  lmark.marshall_table(th, self.props)
end

function RestrictDoor:read(marker, th)
  RestrictDoor.super.read(self, marker, th)
  self.props          = lmark.unmarshall_table(th)
  setmetatable(self, RestrictDoor)
  return self
end

function RestrictDoor:on_trigger(triggerer, marker, ev)
  self.props.door_restrict = ""
end

function restrict_door(props)
  local rd = RestrictDoor:new(props)
  rd:add_triggerer(DgnTriggerer:new{type = "door_opened"})
  return rd
end

-------------------------------------------------------------------------------
-- This marker can be applied to a door to prevent the player from opening   --
-- it. By default, it will ask for three runes; this can be modified by the  --
-- "veto_func" parameter.                                                    --
-------------------------------------------------------------------------------

crawl_require("dlua/lm_pdesc.lua")

LockDoor       = util.subclass(PortalDescriptor)
LockDoor.CLASS = "LockDoor"

function LockDoor:new(props)
  props = props or { }
  local ld = self.super.new(self)
  ld.props = props
  return ld
end

function LockDoor:property (marker, pname)
  if pname == "veto_open" then
     return self:check_veto(marker, pname)
  end
  return self.super.property(self, marker, pname)
end

function LockDoor:check_veto (marker, pname, dry_run)
  local rune_count = you.num_runes()

  if dry_run ~= nil then crawl.mpr("Got " .. rune_count .. " runes") end
  if rune_count < 3 then
    return "veto"
  else
    if self.props.opened_message ~= nil then
      crawl.mpr(self.props.opened_message)
    end

    if self.props.opened_function ~= nil then
      self.props.opened_function()
    end

    -- set always_lock to permanently lock (if you close it and drop your runes
    -- it will be locked.
    if self.props.always_lock == nil then
      dgn.remove_marker(marker)
    end

    return ""
  end
end

function LockDoor:debug_portal (marker)
  self:check_veto (marker, "", true)
end

function LockDoor:write(marker, th)
  LockDoor.super.write(self, marker, th)

  lmark.marshall_table(th, self.props)
end

function LockDoor:read(marker, th)
  LockDoor.super.read(self, marker, th)
  self.props          = lmark.unmarshall_table(th)
  setmetatable(self, LockDoor)
  return self
end

function lock_door(props)
  return LockDoor:new(props)
end
