-------------------------------------------------------------------------------
-- lm_door.lua
-- Disallow monsters form using certain doors.
-------------------------------------------------------------------------------

-------------------------------------------------------------------------------
-- This marker can be applied to a door to prevent monsters from opening it,
-- unless the door has already previously been opened by the player.
-------------------------------------------------------------------------------

require('clua/lm_trig.lua')

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
