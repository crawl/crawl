#pragma once

class map_lines;

//
//  The basic idea here is as follows:
//    We make a box, but the sides go in and out instead
//    of straight. Then we add doors on the straight
//    parts.
//
//  In more detail, each side is divided into a number of
//    divisions, each of which is a certain distance out
//    from the center of the box. THe divisions are
//    connected at the corners, giving the box an irregular
//    shape.
//
//      +---+       +--+
//      |   +----+  |  +-+--+
//  +---+        +--+
//
//    ^   ^   ^   ^  ^  ^ ^
//         7 divisions
//
//  Some complexities:
//    1. The divisions must be space out enough that there
//       is at least once cell between them.
//    2. The box must be big enough to divide
//    3. The box must touch its specified outer bounds at
//       least once on each side
//    4. The corners of the box must not pass through each
//       other. That would create an unreachable location
//       and cause the level to veto.
//
//  Parameter(s):
//    <1> map: The map_lines to draw the room to
//    <2> x1
//    <3> y1
//    <4> x2
//    <5> y2: The maximum boundaries of the box
//    <6> div_x
//    <7> div_y: The maximum divisions along the x-/y-aligned
//               walls
//    <8> in_x
//    <9> in_x: The maximum distance the x-/y-aligned walls can
//              be from the "official" edge of the box
//    <10>floor_glyph: The glyph to fill the inside with
//    <11>wall_glyph: The glyph to use for make the walls
//    <12>door_glyph: The glyph to use for the doors
//    <13>door_count: The maximum number of doors
//
//  Preconditions(s):
//    <1> lines.width() > 0
//    <2> lines.height() > 0
//    <3> x1 >= 0
//    <4> x1 <= x2
//    <5> x2 < lines.width()
//    <6> y1 >= 0
//    <7> y1 <= y2
//    <8> y2 < lines.height()
//    <9> div_x >= 0
//    <10> div_y >= 0
//    <11> in_x >= 0
//    <12>in_y >= 0
//    <13>door_count >= 0
//
//  Some Notes:
//    <1> The sides of a box never go in past the middle, no
//        matter what you set the values to. Otherwise there
//        is self-intersection (very bad).
//

void make_irregular_box(map_lines& map, int x1, int y1, int x2, int y2,
                        int div_x = 1,    int div_y = 1,
                        int in_x = 10000, int in_y = 10000,
                        char floor_glyph = '.', char wall_glyph = 'x',
                        char door_glyph = '+',  int door_count = 1);
