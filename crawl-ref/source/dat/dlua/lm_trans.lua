-- lm_trans.lua: Between-vault transporter markers. See the syntax guide for
-- details on usage.

crawl_require('dlua/util.lua')

-- A subclass of PortalDescriptor that sets the _transporter_name property with
-- a unique name.
Transporter = util.subclass(PortalDescriptor)
Transporter.CLASS = "Transporter"

function Transporter:new(name)
  if not name then
    error("No transporter name provided")
  end

  local t = self.super.new(self)
  t.props = {_transporter_name = name}

  return t
end

function transp_loc(name)
  return Transporter:new(name)
end

-- A subclass of PortalDescriptor that sets the _transporter_dest_name property
-- with a unique name.
TransporterDestination = util.subclass(PortalDescriptor)
TransporterDestination.CLASS = "TransporterDestination"

function TransporterDestination:new(name)
  if not name then
    error("No transporter destination name provided")
  end

  local t = self.super.new(self)
  t.props = {_transporter_dest_name = name}

  return t
end

function transp_dest_loc(name)
  return TransporterDestination:new(name)
end
