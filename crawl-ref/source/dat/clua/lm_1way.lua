------------------------------------------------------------------------------
-- lm_1way.lua:
-- One-way stair marker.
------------------------------------------------------------------------------

OneWayStair = PortalDescriptor:new()
OneWayStair.__index = OneWayStair

function OneWayStair:new(props)
  local ows = PortalDescriptor.new(self, props)
  setmetatable(ows, self)
  return ows
end

function OneWayStair:activate(marker)
  local ev = dgn.dgn_event_type('player_climb')
  dgn.register_listener(ev, marker, marker:pos())
end

function OneWayStair:event(marker, ev)
  if ev:type() == dgn.dgn_event_type('player_climb') then
    local x, y = ev:pos()
    dgn.terrain_changed(x, y, 'floor', false)
    dgn.remove_listener(marker, ev:pos())
    dgn.remove_marker(marker)
  end
end

function OneWayStair:read(marker, th)
  PortalDescriptor.read(self, marker, th)
  setmetatable(self, OneWayStair)
  return self
end

function one_way_stair(pars)
  return OneWayStair:new(pars)
end
