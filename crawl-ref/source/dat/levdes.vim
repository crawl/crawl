" levdes.vim: 
"
" Basic Vim syntax highlighting for Dungeon Crawl Stone Soup level design
" (.des) files.

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn case match

setlocal iskeyword+=:
setlocal iskeyword+=-

syn keyword desDeclarator NAME: ORIENT: DEPTH: PLACE: MONS: FLAGS: SYMBOL: default-depth: TAGS: CHANCE:
syn keyword desOrientation encompass north south east west northeast northwest southeast southwest north_dis

syn match desComment "^\s*#.*$"

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
syn match desMapEntry /@/ contained
syn match desMapTrap  /\^/ contained

syn match desMapValuable /[R%*|]/ contained
syn match desMapRune /[PO]/ contained
syn match desMapOrb /Z/ contained

syn cluster desMapElements contains=desMapBookend,desMapWall,desMapFloor
syn cluster desMapElements add=desMapMonst,desMapCrystalWall,desMapGold
syn cluster desMapElements add=desMapLava,desMapMetalWall,desMapDoor
syn cluster desMapElements add=desMapStoneWall,desMapWater,desMapTrap
syn cluster desMapElements add=desMapEntry,desMapWaxWall

syn cluster desMapElements add=desMapRune,desMapOrb,desMapValuable

syn region desMap start=/^\s*\<MAP\>\s*$/ end=/^\s*\<ENDMAP\>\s*$/ contains=@desMapElements keepend

hi link desDeclarator Statement
hi link desMapBookend Statement
hi link desComment    Comment
hi link desMap        String
hi link desOrientation Type

hi desMapWall guifg=darkgrey term=bold gui=bold
hi desMapCrystalWall guifg=#009040 term=bold gui=bold
hi desMapStoneWall guifg=black gui=bold
hi desMapMetalWall guifg=#004090 term=bold gui=bold
hi desMapWaxWall guifg=#a0a000 gui=bold
hi desMapFloor guifg=#008000
hi desMapMonst guifg=red
hi desMapLava guifg=red gui=bold
hi desMapTrap guifg=red gui=bold
hi desMapWater guifg=lightblue
hi desMapGold guifg=#c09000
hi desMapDoor guifg=brown gui=bold
hi desMapEntry guifg=black guibg=white gui=bold

hi desMapValuable guifg=darkgreen gui=bold
hi desMapRune     guifg=orange gui=bold
hi desMapOrb      guibg=gold guifg=black

syn sync minlines=45
