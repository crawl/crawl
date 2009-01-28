------------------------------------------------------------------------------
-- lm_1way.lua:
-- One-way stair marker.
------------------------------------------------------------------------------

OneWayStair = util.subclass(PortalDescriptor)
OneWayStair.CLASS = "OneWayStair"

function OneWayStair:activate(marker)
  local ev = dgn.dgn_event_type('player_climb')
  dgn.register_listener(ev, marker, marker:pos())
end

function OneWayStair:disappear(marker, affect_player, x, y)
  dgn.terrain_changed(x, y, self.props.floor or 'floor', affect_player, false)
  dgn.remove_listener(marker, x, y)
  dgn.remove_marker(marker)
end

function OneWayStair:event(marker, ev)
  if ev:type() == dgn.dgn_event_type('player_climb') then
    local x, y = ev:pos()

    -- If there's an onclimb function, call it before we do our thing.
    if self.props.onclimb then
      self.props.onclimb(self, marker, ev)
    end

    self:disappear(marker, false, x, y)
    return true
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
