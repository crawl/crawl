crawl_require('dlua/lm_trig.lua')

function dgn.activate_item_decay(data, triggerable, trigger, marker, ev)
  local items = dgn.items_at(marker:pos())
  for _, item in ipairs(items) do
    dgn.item_property_remove(item, CORPSE_NEVER_DECAYS)
  end
end

function dgn.delayed_decay_extra(e, key, itemdesc, more_items)
  -- Add never_decay to each sub-part of the itemdef
  local def = "never_decay " .. itemdesc:gsub("([,/])", "%1 never_decay ")
  if more_items then
    def = def .. ", " .. more_items
  end
  e.kitem(key .. " = " .. def)
  -- [ds] Use an anonymous function so that we create a new
  -- triggerable for each square instead of reusing one across all
  -- squares.
  e.lua_marker(key,
               function ()
                 return function_in_los("dgn.activate_item_decay", nil,
                                        false, nil)
               end)
end

function dgn.delayed_decay(e, key, itemdesc)
  dgn.delayed_decay_extra(e, key, itemdesc, nil)
end
