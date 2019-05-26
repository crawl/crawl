// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// 'Strong' (indistinguishable from random, backtracking-resistant) random
// generator, faster in some benchmarks than std::mt19937_64 and pcg64_c32.
// Accompanying paper: https://arxiv.org/abs/1810.02227

#ifndef RANDEN_H_
#define RANDEN_H_

#include <stdint.h>
#include <string.h>  // memcpy
#include <algorithm>
#include <ios>
#include <istream>
#include <iterator>
#include <limits>
#include <ostream>
#include <type_traits>

// RANDen = RANDom generator or beetroots in Swiss German.
namespace randen {

struct Internal {
  static void Absorb(const void* seed, void* state);
  static void Generate(void* state);

  static constexpr int kStateBytes = 256;  // 2048-bit

  // Size of the 'inner' (inaccessible) part of the sponge. Larger values would
  // require more frequent calls to Generate.
  static constexpr int kCapacityBytes = 16;  // 128-bit
};

// Deterministic pseudorandom byte generator with backtracking resistance
// (leaking the state does not compromise prior outputs). Based on Reverie
// (see "A Robust and Sponge-Like PRNG with Improved Efficiency") instantiated
// with an improved Simpira-like permutation.
// Returns values of type "T" (must be a built-in unsigned integer type).
template <typename T>
class alignas(32) Randen {
  static_assert(std::is_unsigned<T>::value,
                "Randen must be parameterized by a built-in unsigned integer");

 public:
  // C++11 URBG interface:
  using result_type = T;

  static constexpr result_type min() {
    return std::numeric_limits<result_type>::min();
  }

  static constexpr result_type max() {
    return std::numeric_limits<result_type>::max();
  }

  explicit Randen(result_type seed_value = 0) { seed(seed_value); }

  template <class SeedSequence,
            typename = typename std::enable_if<
                !std::is_same<SeedSequence, Randen>::value>::type>
  explicit Randen(SeedSequence&& seq) {
    seed(seq);
  }

  // Default copy and move operators.
  Randen(const Randen&) = default;
  Randen& operator=(const Randen&) = default;

  Randen(Randen&&) = default;
  Randen& operator=(Randen&&) = default;

  // Returns random bits from the buffer in units of T.
  result_type operator()() {
    // (Local copy ensures compiler knows this is not aliased.)
    size_t next = next_;

    // Refill the buffer if needed (unlikely).
    if (next >= kStateT) {
      Internal::Generate(state_);
      next = kCapacityT;
    }

    const result_type ret = state_[next];
    next_ = next + 1;
    return ret;
  }

  template <class SeedSequence>
  typename std::enable_if<
      !std::is_convertible<SeedSequence, result_type>::value, void>::type
  seed(SeedSequence& seq) {
    seed();
    reseed(seq);
  }

  void seed(result_type seed_value = 0) {
    next_ = kStateT;
    std::fill(std::begin(state_), std::begin(state_) + kCapacityT, 0);
    std::fill(std::begin(state_) + kCapacityT, std::end(state_), seed_value);
  }

  // Inserts entropy into (part of) the state. Calling this periodically with
  // sufficient entropy ensures prediction resistance (attackers cannot predict
  // future outputs even if state is compromised).
  template <class SeedSequence>
  void reseed(SeedSequence& seq) {
    using U32 = typename SeedSequence::result_type;
    constexpr int kRate32 =
        (Internal::kStateBytes - Internal::kCapacityBytes) / sizeof(U32);
    U32 buffer[kRate32];
    seq.generate(buffer, buffer + kRate32);
    Internal::Absorb(buffer, state_);
    next_ = kStateT;  // Generate will be called by operator()
  }

  void discard(unsigned long long count) {
    using ull_t = unsigned long long;
    const ull_t remaining = kStateT - next_;
    if (count <= remaining) {
      next_ += count;
      return;
    }
    count -= remaining;

    const ull_t kRateT = kStateT - kCapacityT;
    while (count > kRateT) {
      Internal::Generate(state_);
      next_ = kCapacityT;
      count -= kRateT;
    }

    if (count != 0) {
      Internal::Generate(state_);
      next_ = kCapacityT + count;
    }
  }

  bool operator==(const Randen& other) const {
    return next_ == other.next_ &&
           std::equal(std::begin(state_), std::end(state_),
                      std::begin(other.state_));
  }

  bool operator!=(const Randen& other) const { return !(*this == other); }

  template <class CharT, class Traits>
  friend std::basic_ostream<CharT, Traits>& operator<<(
      std::basic_ostream<CharT, Traits>& os,  // NOLINT(runtime/references)
      const Randen<T>& engine) {              // NOLINT(runtime/references)
    const auto flags = os.flags(std::ios_base::dec | std::ios_base::left);
    const auto fill = os.fill(os.widen(' '));

    for (auto x : engine.state_) {
      os << x << os.fill();
    }
    os << engine.next_;

    os.flags(flags);
    os.fill(fill);
    return os;
  }

  template <class CharT, class Traits>
  friend std::basic_istream<CharT, Traits>& operator>>(
      std::basic_istream<CharT, Traits>& is,  // NOLINT(runtime/references)
      Randen<T>& engine) {                    // NOLINT(runtime/references)
    const auto flags = is.flags(std::ios_base::dec | std::ios_base::skipws);
    const auto fill = is.fill(is.widen(' '));

    T state[kStateT];
    size_t next;
    for (auto& x : state) {
      is >> x;
    }
    is >> next;
    if (!is.fail()) {
      memcpy(engine.state_, state, sizeof(engine.state_));
      engine.next_ = next;
    }
    is.flags(flags);
    is.fill(fill);
    return is;
  }

 private:
  static constexpr size_t kStateT = Internal::kStateBytes / sizeof(T);
  static constexpr size_t kCapacityT = Internal::kCapacityBytes / sizeof(T);

  // First kCapacityT are `inner', the others are accessible random bits.
  alignas(32) result_type state_[kStateT];
  size_t next_;  // index within state_
};

}  // namespace randen

#endif  // RANDEN_H_

