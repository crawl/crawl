require('dlua/lm_trig.lua')

function dgn.activate_item_decay(data, triggerable, trigger, marker, ev)
  local items = dgn.items_at(marker:pos())
  for _, item in ipairs(items) do
    dgn.item_property_remove(item, CORPSE_NEVER_DECAYS)
  end
end

function dgn.delayed_decay(e, key, itemdesc)
  e.kitem(key .. " = never_decay " .. itemdesc)
  -- [ds] Use an anonymous function so that we create a new
  -- triggerable for each square instead of reusing one across all
  -- squares.
  e.lua_marker(key,
               function ()
                 return function_in_los("dgn.activate_item_decay", nil,
                                        false, nil)
               end)
end
