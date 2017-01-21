local place = dgn.point(20, 20)

debug.goto_place("D:2")
dgn.reset_level()

local function itemname(item_spec, name_type)
  dgn.reset_level()
  dgn.fill_grd_area(1, 1, dgn.GXM - 2, dgn.GYM - 2, 'floor')
  local items = test.place_items_at(place, item_spec)
  assert(#items == 1,
         "Could not create item: '" .. item_spec .. "'")
  local top_item = items[1]
  return top_item.name(name_type)
end

local function check_monster_name(mspec, monster_name_checks,
                                  corpse_name_checks)

  dgn.reset_level()
  dgn.fill_grd_area(1, 1, dgn.GXM - 2, dgn.GYM - 2, 'floor')

  local function check_names(thing, name_checks, namefn)
    if type(name_checks) == 'string' then
      test.eq(namefn(thing), name_checks, mspec)
    else
      local desc_type = name_checks[1]
      local expected_name = name_checks[2]
      if type(desc_type) == 'string' then
        test.eq(namefn(thing, desc_type), expected_name, mspec)
      else
        for i = 1, #desc_type do
          test.eq(namefn(thing, desc_type[i]), expected_name[i], mspec)
        end
      end
    end
  end

  local function check_monster_names(name_checks)
    local mons = dgn.create_monster(place.x, place.y, mspec)
    assert(mons, "Could not create monster from spec: " .. mspec)
    check_names(mons, name_checks,
                function (mons, desc)
                  return mons.mfull_name(desc)
                end)
  end

  local function check_monster_corpse_names(name_checks)
    local corpse_spec = mspec .. " corpse"
    local items = test.place_items_at(place, corpse_spec)
    assert(#items == 1,
           "Could not create corpse: " .. corpse_spec)
    local corpse = items[1]
    check_names(corpse, name_checks,
                function (corpse, desc)
                  return corpse.name(desc)
                end)
  end

  check_monster_names(monster_name_checks)
  if corpse_name_checks then
    check_monster_corpse_names(corpse_name_checks)
  end
end

local function check_names(list)
  for _, check in ipairs(list) do
    check_monster_name(check[1], check[2], check[3])
  end
end

-- Each line must have two or three entries:
-- 1. A valid monster spec to generate the monster
-- 2. The expected monster name (string) for DESC_PLAIN OR
--    a table as { desc_type, expected_monster_name } where
--    desc_type is a description type string ("a", "The", etc.) or a table
--    of description type strings. expected_monster_name is an expected
--    monster name string or a table of strings (if the description type is
--    also a table.
-- 3. Optionally, the expected item name for DESC_PLAIN or a table of
--    item description type and expected item name.
local name_checks = {
  { "hippogriff", { "the", "the hippogriff" }, { "a", "a hippogriff corpse" } },
  { "kobold name:ugly name_adjective",
    { "a", "an ugly kobold" } },
  { "kobold name:ugly name_adjective",
    { "the", "the ugly kobold" } },
  { "kobold name:ugly n_adj n_spe", "ugly kobold", "ugly kobold corpse" },
  { "kobold name:Durwent",
    { "a", "Durwent the kobold" },
    { "a", "the kobold corpse of Durwent" } },
  { "kobold name:wearing_mittens name_suffix",
    { "a", "a kobold wearing mittens" },
    "kobold corpse" },
  { "gnoll name:gnoll_lieutenant name_replace name_descriptor name_species",
    { "a", "a gnoll lieutenant" },
    { "a", "a gnoll lieutenant corpse" } },
  { "gnoll name:gnoll_lieutenant name_replace name_descriptor",
    { "a", "a gnoll lieutenant" },
    -- [ds] FIXME: this should probably be just "a gnoll corpse"
    { "a", "a gnoll corpse of gnoll lieutenant" } },
}
check_names(name_checks)

test.eq(itemname("hydra chunk q:10"), "10 inedible chunks of flesh")
