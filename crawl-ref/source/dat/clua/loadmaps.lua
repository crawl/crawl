------------------------------------------------------------------------------
-- loadmaps.lua:
--
-- Compiles and loads .des files that Crawl needs. This only includes the
-- base .des files. Optional .des files that the user requests in .crawlrc
-- are handled separately.
------------------------------------------------------------------------------

local des_files = {
   "entry.des", "elf.des", "float.des", "hells.des", "hive.des", "lab.des", 
   "lair.des", "large.des", "mini.des", "orc.des", "pan.des", "portal.des", 
   "temple.des", "vaults.des", "zot.des"
}

for _, file in ipairs(des_files) do
   dgn.load_des_file(file)
end