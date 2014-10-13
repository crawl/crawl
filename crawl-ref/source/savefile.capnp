@0xba3b5128ca47fcc0;

enum Branch {
  dungeon @0;
  temple @1;
  orc @2;
  elf @3;
  dwarf @4;
  lair @5;
  swamp @6;
  shoals @7;
  snake @8;
  spider @9;
  slime @10;
  vaults @11;
  blade @12;
  crypt @13;
  tomb @14;
  depths @15;
  vestibule @16;
  dis @17;
  gehenna @18;
  cocytus @19;
  tartarus @20;
  zot @21;
  forest @22;
  abyss @23;
  pandemonium @24;
  ziggurat @25;
  labyrinth @26;
  bazaar @27;
  trove @28;
  sewer @29;
  ossuary @30;
  bailey @31;
  iceCave @32;
  volcano @33;
  wizlab @34;
}

struct LevelId {
  depth  @0 :UInt8;
  branch @1 :Branch;
}

struct Position {
  x @0 :Int16;
  y @1 :Int16;
}

struct Item {
  type           @0  :UInt8;
  subtype        @1  :UInt8;
  plus           @2  :Int8;
  plus2          @3  :Int8;  # Used for nets
  special        @4  :Int32;
  quantity       @5  :Int16;
  colour         @6  :UInt8;
  random         @7  :UInt8; # Tile selection
  position       @8  :Position;
  link           @9  :UInt16;
  slot           @10 :UInt8;
  originalPlace  @11 :LevelId;
  originalMonNum @12 :Int16;
  inscription    @13 :Text;
}
