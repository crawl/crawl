-------------------
-- Functions for Sprint
-------------------

function callback.acq_on_sight_trig(data, triggerable, triggerer, marker, ev)
  if data.triggered == true then
    return
 end

  local m = dgn.find_marker_positions_by_prop("slave_name", data.slave_name)[1]
  if m ~= nil then
    local x, y = m:xy()

    if not you.see_cell(x, y) then
      return
    end

    data.triggered = true
    if type(data.acq_type) == "string" then
      dgn.create_item(x, y, "acquire " .. data.acq_type)
    else
      for i,_ in ipairs(data.acq_type) do
        dgn.create_item(x, y, "acquire " .. _)
      end
    end
  else
    crawl.mpr("marker ["..data.slave_name.."] not found")
  end
end

acq_on_sight_count = 0

function acq_on_sight(e, glyph, type)
  acq_on_sight_count = acq_on_sight_count + 1
  local m_name = "acq_on_sight_" .. acq_on_sight_count
  local tm = TriggerableFunction:new{
    func="callback.acq_on_sight_trig",
    repeated=true,
    data={triggered=false, acq_type=type, slave_name=m_name} }
  tm:add_triggerer(DgnTriggerer:new{type="player_los"})
  e.lua_marker(glyph, tm)
  e.lua_marker(glyph, function()
    return portal_desc {slave_name=m_name}
  end)
end

-- usage:
-- : acq_on_sight(_G, '_', "weapon")
-- : acq_on_sight(_G, '_', { "weapon", "weapon", "armour" })
-- You can't have multiple squares with the given glyph ('_' here).
