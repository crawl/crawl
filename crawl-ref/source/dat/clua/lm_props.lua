------------------------------------------------------------------------------
-- lm_props.lua:
-- Property markers.
------------------------------------------------------------------------------

PropertiesDescriptor = { CLASS = "PropertiesDescriptor" }
PropertiesDescriptor.__index = PropertiesDescriptor

function PropertiesDescriptor:new(properties)
  local pd = { }
  setmetatable(pd, self)
  pd.props = properties
  return pd
end

function PropertiesDescriptor:write(marker, th)
  lmark.marshall_table(th, self.props)
end

function PropertiesDescriptor:read(marker, th)
  self.props = lmark.unmarshall_table(th)
  setmetatable(self, PropertiesDescriptor)
  return self
end

function PropertiesDescriptor:property(marker, pname)
  return self.props and self.props[pname] or ''
end

function props_marker(props)
  return PropertiesDescriptor:new(props)
end
