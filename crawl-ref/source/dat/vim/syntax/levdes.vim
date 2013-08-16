" Vim syntax file
" Language:	Dungeon Crawl level design (.des) files.
" Maintainer:	Darshan Shaligram <scintilla@gmail.com>
" Last Change:	2007 Jun 28
" Remark:	Basic Vim syntax highlighting for Dungeon Crawl Stone Soup
"               level design (.des) files.
"
" How to use this:
" * Put levdes.vim (this file) under ~/.vim/syntax (or similar directory for
"   your system - usually C:\Program Files\Vim\vimfiles\syntax on Windows).
" * In your .vimrc, add this line:
"     au BufRead,BufNewFile *.des set syntax=levdes
" Thereafter, any .des files you edit in (g)vim will use syntax highlighting.

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

if !exists("main_syntax")
  let main_syntax = 'des'
endif

syn include @desLua syntax/lua.vim

syn case match

syn match desLuaBlock /\(lua\)\?\s\+{{/ contained
syn match desOtherLuaBlock /^\(prelude\|lua\|validate\|epilogue\|veto\)\?\s*{{/ contained
syn match desLuaBlockEnd /}}/ contained
"syn match desColonLine /^\s*:/ contained

syn cluster desLuaGroup contains=desLuaBlock,desOtherLuaBlock,desLuaBlockEnd

syn region desLua start=/^\s*\(lua\)\?\s*{{/ end=/}}\s*$/ contains=@desLuaGroup,@desLua keepend
syn region desLuaCol start=/^\s*:/ end=/$/ contains=@desLuaGroup,@desLua keepend
syn region desVal start=/^\s*validate\?\s*{{/ end=/}}\s*$/ contains=@desLuaGroup,@desLua keepend
syn region desPre start=/^\s*prelude\?\s*{{/ end=/}}\s*$/ contains=@desLuaGroup,@desLua keepend
syn region desEpi start=/^\s*epilogue\?\s*{{/ end=/}}\s*$/ contains=@desLuagroup,@deslua keepend
syn region desVet start=/^\s*veto\?\s*{{/ end=/}}\s*$/ contains=@desLuagroup,@deslua keepend

setlocal iskeyword+=:
setlocal iskeyword+=-

syn keyword desSubstDec SUBST: contained
syn keyword desNsubstDec NSUBST: contained
syn keyword desShuffleDec SHUFFLE: contained
syn keyword desClearDec CLEAR: contained

syn region desSubst start=/^SUBST:\s*/ end=/$/ contains=desSubstDec,desSubstArg,desSubstSep,@desMapElements keepend
syn region desNsubst start=/^NSUBST:\s*/ end=/$/ contains=desNsubstDec,desSubstArg,desSubstSep,@desMapElements keepend
syn region desShuffle start=/^SHUFFLE:\s*/ end=/$/ contains=desShuffleDec,desMapFrag keepend
syn region desClear start=/^CLEAR:\s*/ end=/$/ contains=desClearDec,desSubstArg keepend

syn match desGod /ashenzari\|beogh\|cheibriados\|elyvilon\|fedhas\|jiyva\|kikubaaqudgha\|lugonu\|makhleb/ contained
syn match desGod /nemelex_xobeh\|okawaru\|sif_muna\|trog\|vehumet\|xom\|yredelemnul\|zin\|the_shining_one/ contained

syn keyword desDeclarator NAME: ORIENT: DEPTH: PLACE: MONS: FLAGS: default-depth: TAGS: CHANCE: WEIGHT:
syn keyword desDeclarator ITEM: KFEAT: KMONS: KITEM: COLOUR: KMASK: KPROP: MARKER: WELCOME: LFLAGS: BFLAGS:
syn keyword desDeclarator LROCKCOL: LFLOORCOL: LFLOORTILE: LROCKTILE: FTILE: RTILE: TILE: SUBVAULT: FHEIGHT: DESC:

" keywords
" ORIENT
syn keyword desOrientation north south east west northwest northeast southwest southeast encompass float centre

" DEPTH | PLACE
syn keyword desOrientation Temple Orc Elf Lair Swamp Shoals Snake Spider Slime Vaults Blade Crypt Tomb
syn keyword desOrientation Hell Dis Geh Coc Tar Zot Forest Abyss Pan Zig Lab Bazaar Trove Sewer Ossuary
syn keyword desOrientation Bailey IceCv Volcano WizLab
syn keyword desOrientation D: contained

" TAGS
" in decks.cc and dgn-labyrinth.cc (without `minotaur` because monster)
syn keyword desOrientation trowel_portal lab generate_loot
" map building in dungeon.cc (`transparent` is handled later)
syn keyword desOrientation dummy entry mini_float extra ruin layout pan decor
syn keyword desOrientation allow_dup uniq luniq
syn keyword desOrientation no_hmirror no_vmirror no_rotate
syn keyword desOrientation no_dump
" vault placement in maps.cc
syn keyword desOrientation unrand place_unique special_room tutorial
syn keyword desOrientation water_ok can_overwrite replace_portal

" LFLAGS (in l_dgn.cc)
syn keyword desOrientation no_tele_control not_mappable no_magic_map

" ITEM | KITEM (in mapdef.cc, without `random`)
syn keyword desOrientation randbook any good_item star_item superb_item gold nothing
syn keyword desOrientation acquire mundane damaged cursed randart not_cursed useful unobtainable
syn keyword desOrientation mimic no_mimic no_pickup no_uniq allow_uniq
syn keyword desOrientation corpse chunk skeleton never_decay rotting

" MONS | KMONS (in mapdef.cc)
syn keyword desOrientation fix_slot priest_spells actual_spells god_gift
syn keyword desOrientation generate_awake patrolling band
syn keyword desOrientation hostile friendly good_neutral fellow_slime strict_neutral neutral
syn keyword desOrientation zombie skeleton simulacrum spectre chimera
syn keyword desOrientation seen always_corpse never_corpse
syn keyword desOrientation base nonbase
syn keyword desOrientation n_suf        n_adj           n_rpl         n_the
syn keyword desOrientation name_suffix  name_adjective  name_replace  name_definite
syn keyword desOrientation n_des            n_spe         n_zom        n_noc
syn keyword desOrientation name_descriptor  name_species  name_zombie  name_nocorpse

" TILE
syn keyword desOrientation no_random

" KFEAT
syn keyword desOrientation known mimic

" abyss TAGS in mapdef.cc
syn keyword desOrientation abyss abyss_rune
syn keyword desOrientation overwritable
" KMASK (in mapdef.cc)
syn keyword desOrientation vault no_item_gen no_monster_gen no_pool_fixup no_wall_fixup opaque no_trap_gen

" KPROP
syn keyword desOrientation bloody highlight mold no_cloud_gen no_rtele_into no_ctele_into no_tele_into no_tide no_submerge no_jiyva

syn match desComment "^\s*#.*$"

syn match desEntry "\<\w*_entry\>"
syn match desEntry "\<serial_\w*\>"
syn match desEntry "\<layout_\w*\>"
syn match desEntry "\<uniq_\w*\>"
syn match desEntry "\<chance_\w*\>"
syn match desEntry "\<altar_\w*\>"           contains=desGod
syn match desEntry "\<uniq_altar_\w*\w*\>"   contains=desGod
syn match desEntry "\<temple_overflow_\w*\>" contains=desGod
syn match desEntry "\<overflow_altar_\w*\>"  contains=desGod
syn match desEntry "\<enter_\c\w*\>"
syn match desEntry "\<exit_\c\w*\>"
syn match desEntry "\<ruin_\c\w*\>"
syn match desEntry "\<return_from_\c\w*\>"
"
" 'transparent' is a Vim syntax keyword
syn match desTransparent "transparent"
syn match desRange "\d*-\d*"
syn match desNumber "\s\d*"
syn match desWeight "w\(eight\)\=:\d*"
syn match desSlash "/"

syn keyword desMapBookend MAP ENDMAP contained
syn match desMapWall /x/ contained
syn match desMapPermaWall /X/ contained
syn match desMapStoneWall /c/ contained
syn match desMapGlassWall /[mno]/ contained
syn match desMapMetalWall /v/ contained
syn match desMapCrystalWall /b/ contained
syn match desMapTree /t/ contained

syn match desMapFloor /\./ contained
syn match desMapDoor /[+=]/ contained

syn match desMapShallow /W/ contained
syn match desMapWater /w/ contained
syn match desMapLava /l/ contained

syn match desMapEntry /@/ contained
syn match desMapStairs /[}{)(\]\[]/ contained
syn match desMapTrap /[\^~]/ contained

syn match desMapGold /\$/ contained
syn match desMapValuable /[%*|]/ contained

syn match desMapMonst /[0-9]/ contained

syn cluster desMapElements contains=desMapBookend
syn cluster desMapElements add=desMapWall,desMapPermaWall,desMapStoneWall,desMapGlassWall,desMapCrystalWall,desMapMetalWall,desMapTree
syn cluster desMapElements add=desMapFloor,desMapDoor
syn cluster desMapElements add=desMapShallow,desMapWater,desMapLava
syn cluster desMapElements add=desMapEntry,desMapStairs,desMapTrap
syn cluster desMapElements add=desMapGold,desMapValuable
syn cluster desMapElements add=desMapMons

syn match desSubstArg /\S/ contained nextgroup=desSubstSep skipwhite
syn match desSubstSep /[:=]/ contained nextgroup=desMapFrag skipwhite
syn region desMapFrag start=/./ end=/$/ contains=@desMapElements contained

syn region desMap start=/^\s*\<MAP\>\s*$/ end=/^\s*\<ENDMAP\>\s*$/ contains=@desMapElements keepend

hi link desDeclarator Statement
hi link desSubstDec   Statement
hi link desNsubstDec  Statement
hi link desShuffleDec Statement
hi link desClearDec   Statement
hi link desMapBookend Statement
hi link desLuaBlock   Statement
hi link desOtherLuaBlock Statement
hi link desLuaBlockEnd Statement
"hi link desColonLine  Statement
hi link desComment    Comment
hi link desMap        String
hi link desSubstArg   String
hi link desRange      String
hi link desEntry      Type
hi link desNumber     String
hi link desWeight     String
hi link desSlash      Comment

hi link desSubstSep    Type
hi link desOrientation Type
hi link desGod         Special
hi link desTransparent Type

" It would be really nice if this worked for people who switch bg
" post-loading, like "normal" highlights do. Does someone know how?
if &bg == "dark"
  hi desMapWall guifg=darkgray term=bold gui=bold ctermfg=white
  hi desMapPermaWall guifg=#a0a000 gui=bold ctermfg=yellow
  hi desMapStoneWall guifg=black gui=bold ctermfg=gray
  hi desMapGlassWall guifg=lightcyan ctermfg=lightcyan
  hi desMapMetalWall guifg=#004090 term=bold gui=bold ctermfg=lightblue
  hi desMapCrystalWall guifg=#009040 term=bold gui=bold ctermfg=green
  hi desMapTree guifg=#00aa00 ctermfg=darkgreen
  hi desMapFloor guifg=#008000 ctermfg=darkgray
  hi desMapDoor guifg=brown gui=bold ctermfg=white
  hi desMapShallow guifg=lightcyan ctermfg=darkcyan
  hi desMapWater guifg=lightblue ctermfg=darkblue
  hi desMapLava guifg=red gui=bold ctermfg=darkred

  hi desMapEntry guifg=black guibg=white gui=bold ctermfg=white ctermbg=black
  hi desMapStairs guifg=orange gui=bold ctermfg=magenta
  hi desMapTrap guifg=red gui=bold ctermfg=darkred

  hi desMapGold guifg=#c09000 ctermfg=yellow
  hi desMapValuable guifg=darkgreen gui=bold ctermfg=yellow
  hi desMapMonst guifg=red ctermfg=red
else
  hi desMapWall guifg=darkgray term=bold gui=bold ctermfg=brown
  hi desMapPermaWall guifg=#a0a000 gui=bold ctermfg=yellow
  hi desMapStoneWall guifg=black gui=bold ctermfg=darkgray
  hi desMapGlassWall guifg=lightcyan ctermfg=lightcyan
  hi desMapMetalWall guifg=#004090 term=bold gui=bold ctermfg=blue
  hi desMapCrystalWall guifg=#009040 term=bold gui=bold ctermfg=green
  hi desMapTree guifg=#00aa00 ctermfg=darkgreen
  hi desMapFloor guifg=#008000 ctermfg=lightgray
  hi desMapDoor guifg=brown gui=bold ctermfg=black ctermbg=brown
  hi desMapShallow guifg=lightcyan ctermfg=darkcyan
  hi desMapWater guifg=lightblue ctermfg=darkblue
  hi desMapLava guifg=red gui=bold ctermfg=red

  hi desMapEntry guifg=black guibg=white gui=bold ctermfg=white ctermbg=black
  hi desMapStairs guifg=orange gui=bold ctermfg=white
  hi desMapTrap guifg=red gui=bold ctermfg=red

  hi desMapGold guifg=#c09000 ctermfg=yellow
  hi desMapValuable guifg=darkgreen gui=bold ctermfg=lightgreen
  hi desMapMonst guifg=red ctermfg=darkred
endif

syn sync minlines=45

let b:current_syntax="levdes"
