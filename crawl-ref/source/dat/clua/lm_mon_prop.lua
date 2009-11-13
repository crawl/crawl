------------------------------------------------------------------------------
-- lm_mon_prop.lua:
--
-- This marker takes whatever monster is standing on it, sets properties in
-- the CrawlHashTable member of that monster, then disappears.
------------------------------------------------------------------------------

MonPropsMarker = { CLASS = "MonPropsMarker" }
MonPropsMarker.__index = MonPropsMarker

function MonPropsMarker:new(props)
  if not props then
    error("MonPropsMarker:new() needs props")
  end
  if type(props) ~= "table" then
    error("MonPropsMarker:new() needs type(props) == table")
  end

  local mp = { }
  setmetatable(mp, self)
  self.__index = self

  mp.props = props

  return mp
end

function MonPropsMarker:activate(marker)
  local mon = dgn.mons_at(marker:pos())

  if not mon then
    crawl.mpr("No monster for MonPropsMarker:activate()")
    dgn.remove_marker(marker)
    return
  end

  for k, v in pairs(self.props) do
    mon.set_prop(k, v)
  end

  dgn.remove_marker(marker)
end

function MonPropsMarker:write(marker, th)
  error("MonPropsMarker should never be saved")
end

function MonPropsMarker:read(marker, th)
  error("MonPropsMarker should never be in save file")
end
