------------------------------------------------------------------------------
-- lm_toll.lua:
-- One-way toll-stair marker.
-- This file is necessary for TAG_MAJOR_VERSION == 34.
-- If TAG_MAJOR_VERSION > 34 this file should be removed.
------------------------------------------------------------------------------

crawl_require("dlua/lm_1way.lua")

TollStair = util.subclass(OneWayStair)
TollStair.CLASS = "TollStair"

function TollStair:new(props)
  return OneWayStair.new(self, props)
end

function TollStair:activate(marker)
  self.super.activate(self, marker)
end

function TollStair:event(marker, ev)
  return self.super.event(self, marker, ev)
end

function TollStair:check_veto(marker, pname)
end

function TollStair:property(marker, pname)
  return self.super.property(self, marker, pname)
end

function TollStair:feature_description_long(marker)
end

function TollStair:write(marker, th)
  TollStair.super.write(self, marker, th)
  file.marshall(th, self.seen)
end

function TollStair:read(marker, th)
  TollStair.super.read(self, marker, th)
  self.seen = file.unmarshall_number(th)

  setmetatable(self, TollStair)
  return self
end

function toll_stair(pars)
  return TollStair:new(pars)
end
