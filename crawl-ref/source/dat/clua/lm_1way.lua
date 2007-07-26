------------------------------------------------------------------------------
-- lm_1way.lua:
-- One-way stair marker.
------------------------------------------------------------------------------

OneWayStair = { }
OneWayStair.__index = OneWayStair

function OneWayStair.new()
  local ows = { }
  setmetatable(ows, OneWayStair)
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

function OneWayStair.read(marker, th)
  return OneWayStair.new()
end

function one_way_stair()
  return OneWayStair.new()
end
