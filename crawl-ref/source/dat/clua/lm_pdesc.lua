------------------------------------------------------------------------------
-- lm_desc.lua:
-- Portal descriptor markers.
------------------------------------------------------------------------------

PortalDescriptor = { }
PortalDescriptor.__index = PortalDescriptor

function PortalDescriptor:new(properties)
  local pd = { }
  setmetatable(pd, self)
  pd.props = properties
  return pd
end

function PortalDescriptor:write(marker, th)
  file.marshall(th, self.desc or '')
  lmark.marshall_table(th, self.props)
end

function PortalDescriptor:read(marker, th)
  self.desc  = file.unmarshall_string(th)
  self.props = lmark.unmarshall_table(th)
  setmetatable(self, PortalDescriptor)
  return self
end

function PortalDescriptor:feature_description(marker)
  return self.props.desc
end

function PortalDescriptor:property(marker, pname)
  return self.props and self.props[pname] or ''
end

function portal_desc(desc, props)
  return PortalDescriptor:new(desc, props)
end
