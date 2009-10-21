------------------------------------------------------------------------------
-- loadmaps.lua:
--
-- Compiles and loads .des files that Crawl needs. This only includes the
-- base .des files. Optional .des files that the user requests in .crawlrc
-- are handled separately.
------------------------------------------------------------------------------

local des_files = {
  -- The dummy vaults that define global vault generation odds.
  "dummy.des",

  -- Example vaults, included here so that Crawl will syntax-check them
  "didact.des",

  "arena.des",

  "altar.des", "bazaar.des", "bailey.des", "entry.des", "elf.des", 
  "float.des", "hells.des", "hive.des", "icecave.des", "lab.des", 
  "lair.des", "large.des", "layout.des", "mini.des", "ossuary.des", 
  "orc.des", "pan.des", "sewer.des", "shoals.des", "temple.des",
  "trove.des", "shrine.des",
  "vaults.des", "crypt.des", "ziggurat.des", "zot.des", "rooms.des"
}

for _, file in ipairs(des_files) do
   dgn.load_des_file(file)
end
