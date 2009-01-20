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
syn match desOtherLuaBlock /^\(prelude\|lua\|validate\)\?\s*{{/ contained
syn match desLuaBlockEnd /}}/ contained
"syn match desColonLine /^\s*:/ contained

syn cluster desLuaGroup contains=desLuaBlock,desOtherLuaBlock,desLuaBlockEnd

syn region desLua start=/^\s*\(lua\)\?\s*{{/ end=/}}\s*$/ contains=@desLuaGroup,@desLua keepend
syn region desLuaCol start=/^\s*:/ end=/$/ contains=@desLuaGroup,@desLua keepend
syn region desVal start=/^\s*validate\?\s*{{/ end=/}}\s*$/ contains=@desLuaGroup,@desLua keepend
syn region desPre start=/^\s*prelude\?\s*{{/ end=/}}\s*$/ contains=@desLuaGroup,@desLua keepend

setlocal iskeyword+=:
setlocal iskeyword+=-

syn keyword desSubstDec SUBST: contained
syn keyword desNsubstDec NSUBST: contained
syn keyword desShuffleDec SHUFFLE: contained

syn region desSubst start=/^SUBST:\s*/ end=/$/ contains=desSubstDec,desSubstArg,desSubstSep,@desMapElements keepend

syn region desNsubst start=/^NSUBST:\s*/ end=/$/ contains=desNsubstDec,desSubstArg,desSubstSep,@desMapElements keepend

syn region desShuffle start=/^SHUFFLE:\s*/ end=/$/ contains=desShuffleDec,desMapFrag keepend

syn keyword desDeclarator NAME: ORIENT: DEPTH: PLACE: MONS: FLAGS: default-depth: TAGS: CHANCE: WEIGHT: ITEM: KFEAT: KMONS: KITEM: COLOUR: KMASK: MARKER: LFLAGS: BFLAGS: LROCKCOL: LFLOORCOL: LFLOORTILE: LROCKTILE: FTILE: RTILE:
syn keyword desOrientation encompass north south east west northeast northwest southeast southwest float
syn keyword desOrientation no_hmirror no_vmirror no_rotate
syn keyword desOrientation entry pan lab bazaar allow_dup dummy mini_float minotaur
syn keyword desOrientation no_pool_fixup no_wall_fixup no_monster_gen generate_awake no_item_gen no_tele_control not_mappable no_magic_map no_secret_doors generate_loot
syn keyword desOrientation Temple Orc Elf Lair Swamp Shoal Slime Snake Hive Vault Blade Crypt Tomb Hell Dis Geh Coc Tar
syn keyword desOrientation D: contained

syn match desComment "^\s*#.*$"

syn match desEntry "\<\w*_entry\>"
" 'transparent' is a Vim syntax keyword??? 
syn match desTransparent "transparent"
syn match desRange "\d*-\d*"
syn match desNumber "\s\d*"
syn match desWeight "w:\d*"
syn match desSlash "/"

syn keyword desMapBookend MAP ENDMAP contained
syn match desMapFloor /\./ contained
syn match desMapWall  /x/ contained
syn match desMapDoor  /[+=]/ contained
syn match desMapStoneWall /c/ contained
syn match desMapCrystalWall /b/ contained
syn match desMapMetalWall /v/ contained
syn match desMapWaxWall /a/ contained
syn match desMapMonst /[0-9]/ contained
syn match desMapGold /\$/ contained
syn match desMapLava /l/ contained
syn match desMapWater /w/ contained
syn match desMapShallow /W/ contained
syn match desMapEntry /@/ contained
syn match desMapTrap  /\^/ contained

syn match desMapValuable /[R%*|]/ contained
syn match desMapRune /[PO]/ contained
syn match desMapOrb /Z/ contained

syn cluster desMapElements contains=desMapBookend,desMapWall,desMapFloor
syn cluster desMapElements add=desMapMonst,desMapCrystalWall,desMapGold
syn cluster desMapElements add=desMapLava,desMapMetalWall,desMapDoor
syn cluster desMapElements add=desMapStoneWall,desMapWater,desMapShallow
syn cluster desMapElements add=desMapTrap,desMapEntry,desMapWaxWall

syn cluster desMapElements add=desMapRune,desMapOrb,desMapValuable

syn match desSubstArg /\S/ contained nextgroup=desSubstSep skipwhite
syn match desSubstSep /[:=]/ contained nextgroup=desMapFrag skipwhite
syn region desMapFrag start=/./ end=/$/ contains=@desMapElements contained

syn region desMap start=/^\s*\<MAP\>\s*$/ end=/^\s*\<ENDMAP\>\s*$/ contains=@desMapElements keepend

hi link desDeclarator Statement
hi link desSubstDec   Statement
hi link desNsubstDec  Statement
hi link desShuffleDec Statement
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
hi link desTransparent Type

" It would be really nice if this worked for people who switch bg
" post-loading, like "normal" highlights do.  Does someone know how?
if &bg == "dark"
  hi desMapWall guifg=darkgray term=bold gui=bold ctermfg=white
  hi desMapCrystalWall guifg=#009040 term=bold gui=bold ctermfg=green
  hi desMapStoneWall guifg=black gui=bold ctermfg=gray
  hi desMapMetalWall guifg=#004090 term=bold gui=bold ctermfg=lightblue
  hi desMapWaxWall guifg=#a0a000 gui=bold ctermfg=yellow
  hi desMapFloor guifg=#008000 ctermfg=darkgray
  hi desMapMonst guifg=red ctermfg=red
  hi desMapLava guifg=red gui=bold ctermfg=darkred
  hi desMapTrap guifg=red gui=bold ctermfg=darkred
  hi desMapWater guifg=lightblue ctermfg=darkblue
  hi desMapShallow guifg=lightcyan ctermfg=darkcyan
  hi desMapGold guifg=#c09000 ctermfg=yellow
  hi desMapDoor guifg=brown gui=bold ctermfg=white
  hi desMapEntry guifg=black guibg=white gui=bold ctermfg=white ctermbg=black

  hi desMapValuable guifg=darkgreen gui=bold ctermfg=yellow
  hi desMapRune     guifg=orange gui=bold ctermfg=magenta
  hi desMapOrb      guibg=gold guifg=black ctermfg=magenta
else
  hi desMapWall guifg=darkgray term=bold gui=bold ctermfg=brown
  hi desMapCrystalWall guifg=#009040 term=bold gui=bold ctermfg=green
  hi desMapStoneWall guifg=black gui=bold ctermfg=darkgray
  hi desMapMetalWall guifg=#004090 term=bold gui=bold ctermfg=blue
  hi desMapWaxWall guifg=#a0a000 gui=bold ctermfg=yellow
  hi desMapFloor guifg=#008000 ctermfg=lightgray
  hi desMapMonst guifg=red ctermfg=darkred
  hi desMapLava guifg=red gui=bold ctermfg=red
  hi desMapTrap guifg=red gui=bold ctermfg=red
  hi desMapWater guifg=lightblue ctermfg=darkblue
  hi desMapShallow guifg=lightcyan ctermfg=darkcyan
  hi desMapGold guifg=#c09000 ctermfg=yellow
  hi desMapDoor guifg=brown gui=bold ctermfg=black ctermbg=brown
  hi desMapEntry guifg=black guibg=white gui=bold ctermfg=white ctermbg=black

  hi desMapValuable guifg=darkgreen gui=bold ctermfg=lightgreen
  hi desMapRune     guifg=orange gui=bold ctermfg=white
  hi desMapOrb      guibg=gold guifg=black ctermfg=white
endif

syn sync minlines=45

let b:current_syntax="levdes"
