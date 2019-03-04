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

function MonPropsMarker:init(marker)
  -- use init, not activate, so that the properties can get transferred to the
  -- monster immediately on level generation, rather than requiring the player
  -- to enter the level.
  local mon = dgn.mons_at(marker:pos())

  if not mon then
    crawl.mpr("No monster for MonPropsMarker:activate()")
    dgn.remove_marker(marker)
    return
  end

  for k, v in pairs(self.props) do
    mon.set_prop(k, v)
  end

  -- NOTE: do *not* call dgn.remove_marker() right now; removing a marker
  -- while it's being activated causes memory problems. We'll be
  -- removed after activation is done with the "post_activate_remove"
  -- property.
  self.want_remove = true
end

function MonPropsMarker:property(marker, pname)
  if self.want_remove and pname == "post_init_remove" then
    return "true"
  end

  return ""
end

function MonPropsMarker:write(marker, th)
  error("MonPropsMarker should never be saved")
end

function MonPropsMarker:read(marker, th)
  error("MonPropsMarker should never be in save file")
end
