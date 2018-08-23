--[[
lm_named_hatch.lua: Hatches with fixed destinations on their target level.

Set a lua marker on an escape hatch using hatch_name(name), and set another
corresponding marker on the hatch destination with hatch_dest_name(name), using
the same string `name' in both markers. When the player takes this hatch, the
chosen destination will that of destination marker instead of a random
destination.
--]]

crawl_require('dlua/util.lua')

-- A subclass of PortalDescriptor that sets the _hatch_name property
-- with a unique name.
HatchName = util.subclass(PortalDescriptor)
HatchName.CLASS = "HatchName"

function HatchName:new(name)
  if not name then
    error("No hatch name provided")
  end

  local t = self.super.new(self)
  t.props = {_hatch_name = name}

  return t
end

function hatch_name(name)
  return HatchName:new(name)
end

-- A subclass of PortalDescriptor that sets the _hatch_dest_name property
-- with a unique name.
HatchDestinationName = util.subclass(PortalDescriptor)
HatchDestinationName.CLASS = "HatchDestinationName"

function HatchDestinationName:new(name)
  if not name then
    error("No hatch destination name provided")
  end

  local t = self.super.new(self)
  t.props = {_hatch_dest_name = name}

  return t
end

function hatch_dest_name(name)
  return HatchDestinationName:new(name)
end
