-- Arena monster sets used for the first vault choice. The monster entries are
-- in the keys 'first', 'second', and 'third' in decreasing difficulty with
-- each entry - giving the monster to place on the glyphs '1', '2', and '3',
-- respectively.
--
-- Each entry must contain a 'mons' key with the definition, and 'min' and 'max
-- keys. The number of monsters that place for that glyph will then be random
-- uniform on [min, max].
--
-- Set the weight for the entry by adding a 'weight' key. The default weight is
-- 10. Set a a key of 'loot' with a valid item def to define a custom item.
-- Exactly one instance of this item will always place as loot.
tier1_gauntlet_arenas = {
  {
    first   = {mons = "spiny frog simulacrum / wyvern simulacrum " ..
                      "/ hornet simulacrum", min = 1, max = 1},
    second  = {mons = "simulacrum place:D:12", min = 2, max = 4},
  },
  {
    second  = {mons = "raiju", min = 1, max = 1},
    third   = {mons = "steam dragon", min = 1, max = 2},
  },
  {
    second  = {mons = "jelly w:5 / slime creature", min = 3, max = 6},
  },
  {
    first   = {mons = "death ooze", min = 1, max = 1},
  },
  {
    second  = {mons = "sixfirhy", min = 1, max = 2},
  },
  {
    first   = {mons = "ice devil", min = 1, max = 1},
    second  = {mons = "white imp", min = 2, max = 4},
  },
  {
    first   = {mons = "soul eater", min = 1, max = 1},
    second  = {mons = "shadow imp", min = 2, max = 4},
  },
  {
    first   = {mons = "ynoxinul", min = 1, max = 1},
    second  = {mons = "rust devil", min = 0, max = 1},
    third   = {mons = "iron imp", min = 2, max = 4},
  },
  {
    first   = {mons = "smoke demon", min = 1, max = 1},
    second  = {mons = "red devil", min = 0, max = 1},
    second  = {mons = "crimson imp", min = 2, max = 4},
  },
  {
    first   = {mons = "neqoxec", min = 1, max = 1},
    second  = {mons = "chaos spawn", min = 1, max = 3},
  },
  {
    second  = {mons = "shadow", min = 2, max = 4},
  },
  {
    first   = {mons = "catoblepas", min = 1, max = 1},
    second  = {mons = "gargoyle", min = 1, max = 2},
  },
  {
    first   = {mons = "torpor snail", min = 1, max = 1},
    second  = {mons = "yak band / wolf band / elephant band", min = 1,
               max = 1},
  },
  {
    first   = {mons = "fire crab", min = 1, max = 1},
    second  = {mons = "fire bat", min = 2, max = 4},
  },
  {
    second  = {mons = "shapeshifter", min = 1, max = 3},
  },
  {
    first   = {mons = "flying skull band", min = 1, max = 1},
  },
  {
    first   = {mons = "queen ant", min = 1, max = 1},
    second  = {mons = "soldier ant", min = 2, max = 4},
  },
  {
    first   = {mons = "hornet", min = 1, max = 1},
    second  = {mons = "killer bee", min = 2, max = 4},
  },
  {
    first   = {mons = "queen bee", min = 1, max = 1},
    second  = {mons = "killer bee", min = 2, max = 4},
  },
  {
    first   = {mons = "moth of wrath", min = 1, max = 1},
    second  = {mons = "wolf", min = 1, max = 3},
  },
  {
    first   = {mons = "worldbinder", min = 1, max = 1},
    second  = {mons = "boggart", min = 1, max = 3},
  },
  {
    first   = {mons = "death scarab", min = 1, max = 1},
    second  = {mons = "spectral thing place:Lair:6", min = 1, max = 1},
  },
  {
    second  = {mons = "small abomination", min = 2, max = 4},
  },
  {
    second  = {mons = "floating eye", min = 1, max = 2},
    third   = {mons = "shining eye / eye of devastation", min = 1, max = 2},
  },
  {
    first   = {mons = "tengu warrior", min = 1, max = 1},
    second  = {mons = "tengu conjurer", min = 1, max = 2},
  },
  {
    first   = {mons = "wizard", min = 1, max = 1},
    second  = {mons = "crimson imp / white imp / shadow imp", min = 1, max = 3},
  },
}

-- Monster sets used for the first vault choice, same structure as for
-- tier1_gauntlet_arenas.
tier2_gauntlet_arenas = {
  {
    first   = {mons = "hydra simulacrum / harpy simulacrum " ..
                      "/ ice dragon simulacrum", min = 1, max = 1},
    second  = {mons = "simulacrum place:D:15", min = 2, max = 4},
  },
  {
    first  = {mons = "dire elephant", min = 1, max = 1},
    second = {mons = "elephant", min = 1, max = 3},
  },
  {
    first  = {mons = "catoblepas", min = 1, max = 1},
    second = {mons = "molten gargoyle", min = 1, max = 2},
    third  = {mons = "gargoyle", min = 1, max = 2},
  },
  {
    second = {mons = "shadow", min = 3, max = 6},
  },
  {
    first  = {mons = "fire crab", min = 1, max = 1},
    second = {mons = "hell hound", min = 1, max = 3},
  },
  {
    first  = {mons = "efreet", min = 1, max = 1},
    second = {mons = "hell hound / fire elemental", min = 1, max = 3},
  },
  {
    first  = {mons = "hell hog", min = 1, max = 1},
    second = {mons = "fire bat / hell hound", min = 0, max = 3},
  },
  {
    second = {mons = "death ooze", min = 1, max = 1},
    third  = {mons = "jelly w:5 / slime creature", min = 2, max = 4},
  },
  {
    second = {mons = "sixfirhy", min = 2, max = 4},
  },
  {
    first  = {mons = "hell beast", min = 1, max = 1},
    second = {mons = "hell hound / crimson imp / iron imp", min = 0, max = 3},
  },
  {
    first  = {mons = "green death", min = 1, max = 1},
    second = {mons = "orange demon", min = 0, max = 3},
  },
  {
    first  = {mons = "hellion", min = 1, max = 1},
    second = {mons = "smoke demon / red devil", min = 1, max = 1},
    third  = {mons = "crimson imp", min = 0, max = 3},
    weight = 5
  },
  {
    first  = {mons = "balrug", min = 1, max = 1},
    third  = {mons = "crimson imp / hell hound", min = 0, max = 3},
    weight = 5
  },
  {
    first  = {mons = "cacodemon", min = 1, max = 1},
    second = {mons = "neqoxec", min = 1, max = 3},
    weight = 2
  },
  {
    first  = {mons = "blizzard demon", min = 1, max = 1},
    second = {mons = "white imp / ice devil", min = 1, max = 3},
  },
  {
    first  = {mons = "reaper", min = 1, max = 1},
    second = {mons = "hellwing / shadow imp", min = 1, max = 3},
  },
  {
    second = {mons = "shapeshifter", min = 2, max = 4},
  },
  {
    first  = {mons = "apocalypse crab", min = 1, max = 1},
  },
  {
    second = {mons = "flying skull band", min = 2, max = 3},
  },
  {
    first  = {mons = "death drake", min = 1, max = 1},
    second = {mons = "zombie place:D:15 / spectre place:D:15", min = 2,
              max = 4},
  },
  {
    first  = {mons = "ghost crab", min = 1, max = 1},
  },
  {
    first  = {mons = "torpor snail", min = 1, max = 1},
    second = {mons = "death yak / hydra", min = 1, max = 2},
  },
  {
    first  = {mons = "entropy weaver", min = 1, max = 1},
    second = {mons = "redback / wolf spider / jumping spider", min = 1,
              max = 2},
  },
  {
    first  = {mons = "moth of wrath", min = 1, max = 1},
    second = {mons = "redback / soldier ant", min = 2, max = 4},
  },
  {
    second = {mons = "thrashing horror", min = 1, max = 2},
    third  = {mons = "small abomination", min = 2, max = 4},
  },
  {
    first  = {mons = "great orb of eyes", min = 1, max = 1},
    second = {mons = "ugly thing", min = 1, max = 3},
  },
  {
    second = {mons = "glowing orange brain", min = 1, max = 2},
  },
  {
    second = {mons = "large abomination", min = 1, max = 2},
    third  = {mons = "small abomination", min = 2, max = 4},
  },
  {
    first  = {mons = "wretched star", min = 1, max = 1},
    second = {mons = "neqoxec / shapeshifter", min = 1, max = 3},
    loot   = "potion of mutation ident:type",
  },
  {
    second = {mons = "wizard", min = 2, max = 3},
  },
  {
    second = {mons = "death scarab", min = 1, max = 3},
    third  = {mons = "spectral thing place:Spider:4", min = 1, max = 3},
  },
  {
    first  = {mons = "orange crystal statue", min = 1, max = 1},
  },
  {
    first  = {mons = "obsidian statue", min = 1, max = 1},
  },
  {
    -- Guarantee that one places in the central location.
    first  = {mons = "lightning spire", min = 1, max = 1},
    second = {mons = "lightning spire", min = 0, max = 1},
    third  = {mons = "raiju", min = 1, max = 2},
  },
  -- Following sets will place in special vaults
  {
    second  = {mons = "centaur warrior / yaktaur", min = 2, max = 4},
    weight  = 0
  },
  {
    -- not sure if this can work or is really necessary; would use deep
    -- water/lava.
    second  = {mons = "swamp worm", min = 2, max = 3},
    weight  = 0
  },
}
