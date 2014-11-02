#include <assert.h>
#include <iostream>

#include "wang.h"
#include "wang_data.h"

using namespace wang;
using namespace std;

void test_symmetry(DominoSet& dominoes) {
  const uint8_t domino_count = dominoes.size();
  for (uint8_t i = 0; i < domino_count; ++i) {
    Domino* d = dominoes.get(i);
    for (uint8_t j = 0; j < domino_count; ++j) {
      Domino* other = dominoes.get(j);
      for (int dir_idx = FIRST_DIRECTION; dir_idx <= LAST_DIRECTION; ++dir_idx) {
        Direction dir = static_cast<Direction>(dir_idx);
        if (d->matches(other, dir)) {
          assert(other->matches(d, reverse(dir)));
        } else {
          assert(!other->matches(d, reverse(dir)));
        }
      }
    }
  }
}

int main(int argc, char** argv) {
  DominoSet dominoes(wang::periodic_set, 16);
  DominoSet aperiodic_dominoes(wang::aperiodic_set, 44);
  DominoSet cohen_dominoes(wang::cohen_set, 8);
  test_symmetry(dominoes);
  test_symmetry(aperiodic_dominoes);
  vector<uint8_t> output;
  aperiodic_dominoes.Generate(5, 4, output);
  cohen_dominoes.Generate(20, 15, output);
  return 0;
}
