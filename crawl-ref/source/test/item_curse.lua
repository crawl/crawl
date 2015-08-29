-- Test handling of not_cursed in item curse status generation.

local niters = 2500
local item_type = "short sword not_cursed"
local place = dgn.point(20, 20)
local curse_count = 0

local function test_item (place, item_type)
    dgn.create_item(place.x, place.y, item_type)
    local item = dgn.items_at(place.x, place.y)[1]
    if item.is_cursed then
      curse_count = curse_count + 1
    end
end

local function do_item_tests (niters, item_type, place)
    debug.goto_place("D:1")
    dgn.dismiss_monsters()
    dgn.grid(place.x, place.y, "floor")

    for i=1, niters do
      if #dgn.items_at(place.x, place.y) ~= 0 then
        iter.stack_destroy(place)
      end
      test_item(place, item_type)
    end

    if curse_count ~= 0 then
      error("Generated " .. curse_count .. " cursed '" .. item_type .. "' out of " .. niters .. ".")
    end
end

do_item_tests (niters, item_type, place)
