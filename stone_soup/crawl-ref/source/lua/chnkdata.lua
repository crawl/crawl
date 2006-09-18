-- SPOILER WARNING
--
-- This file contains spoiler information. Do not read or use this file if you
-- don't want to be spoiled. Further, the Lua code in this file may use this
-- spoily information to take actions on your behalf. If you don't want
-- automatic exploitation of spoilers, don't use this.
-- 
---------------------------------------------------------------------------
-- chnkdata.lua:
-- Raw data on chunks of meat, auto-extracted from mon-data.h.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/chnkdata.lua
--
-- This file has no directly usable functions, but is needed by gourmand.lua
-- and enables auto_eat_chunks in eat.lua.
---------------------------------------------------------------------------

sc_cont = {"program bug","ettin","goblin","jackal","manticore","orc","ugly thing","two-headed ogre","hobgoblin","ogre","troll","orc warrior","orc wizard","orc knight","minotaur","beast","iron devil","orc sorcerer","","","hell knight","necromancer","wizard","orc priest","orc high priest","human","gnoll","earth elemental","fire elemental","air elemental","Ice Fiend","Shadow Fiend","spectral warrior","pulsating lump","rock troll","stone giant","flayed ghost","insubstantial wisp","vapour","ogre-mage","dancing weapon","elf","war dog","grey rat","giant mosquito","fire giant","frost giant","firedrake","deep troll","giant blowfly","swamp dragon","swamp drake","hill giant","giant cockroach","white imp","lemure","ufetubus","manes","midge","neqoxec","hellwing","smoke demon","ynoxinul","Executioner","Blue Death","Balrug","Cacodemon","demonic crawler","sun demon","shadow imp","shadow wraith","Mnoleg","Lom Lobon","Cerebov","Gloorx Vloq","orc warlord","deep elf soldier","deep elf fighter","deep elf knight","deep elf mage","deep elf summoner","deep elf conjurer","deep elf priest","deep elf high priest","deep elf demonologist","deep elf annihilator","deep elf sorcerer","deep elf death mage","Terence","Jessica","Ijyb","Sigmund","Blork the orc","Edmund","Psyche","Erolcha","Donald","Urug","Michael","Joseph","Snorg","Erica","Josephine","Harold","Norbert","Jozef","Agnes","Maud","Louise","Francis","Frances","Rupert","Wayne","Duane","Xtahua","Norris","Adolf","Margery","Boris","Geryon","Dispater","Asmodeus","Antaeus","Ereshkigal","vault guard","Killer Klown","ball lightning","orb of fire","boggart","quicksilver dragon","iron dragon","skeletal warrior","","&","warg","","","komodo dragon"}
sc_pois = {"killer bee","killer bee larva","quasit","scorpion","yellow wasp","giant beetle","kobold","queen bee","kobold demonologist","big kobold","wolf spider","brain worm","boulder beetle","giant mite","hydra","mottled dragon","brown snake","death yak","bumblebee","redback","spiny worm","golden dragon","elephant slug","green rat","orange rat","black snake","giant centipede","iron troll","naga","yellow snake","red wasp","soldier ant","queen ant","ant larva","spiny frog","orange demon","Green Death","giant amoeba","giant slug","giant snail","boring beetle","naga mage","naga warrior","brown ooze","azure jelly","death ooze","acid blob","ooze","shining eye","greater naga","eye of devastation","gila monster"}
sc_hcl  = {"necrophage","ghoul"}
sc_mut  = {"guardian naga","eye of draining","giant orange brain","great orb of eyes","glowing shapeshifter","shapeshifter","very ugly thing"}

function sc_to_hash(list)
    local hash = { }
    for _, i in ipairs(list) do
        hash[i] = true
    end
    return hash
end

sc_cont = sc_to_hash( sc_cont )
sc_pois = sc_to_hash( sc_pois )
sc_hcl  = sc_to_hash( sc_hcl )
sc_mut  = sc_to_hash( sc_mut )
