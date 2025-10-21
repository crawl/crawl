// These are in the order of the TextureID enum in tiles.h
import floorTiles from "../../../game_data/static/tileinfo-floor";
import wallTiles from "../../../game_data/static/tileinfo-wall";
import featTiles from "../../../game_data/static/tileinfo-feat";
import playerTiles from "../../../game_data/static/tileinfo-player";
import mainTiles from "../../../game_data/static/tileinfo-main";
import guiTiles from "../../../game_data/static/tileinfo-gui";
import iconsTiles from "../../../game_data/static/tileinfo-icons";

const tiles = [
  floorTiles,
  wallTiles,
  featTiles,
  playerTiles,
  mainTiles,
  guiTiles,
  iconsTiles,
];

export default function (tex_idx) {
  return tiles[tex_idx];
}
