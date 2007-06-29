------------------------------------------------------------------------------
-- loadmaps.lua:
--
-- Compiles and loads .des files that Crawl needs. This only includes the
-- base .des files. Optional .des files that the user requests in .crawlrc
-- are handled separately.
------------------------------------------------------------------------------

local des_files = {
   "entry.des", "splev.des", "ebranch.des", "vaults.des"
}

for _, file in ipairs(des_files) do
   dgn.load_des_file(file)
end