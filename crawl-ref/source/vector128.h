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

// Wrappers for platform-specific 128-bit vectors.
#ifndef VECTOR128_H_
#define VECTOR128_H_

#include <stdint.h>     // uint64_t

#if defined(__SSE2__) && defined(__AES__)

#define RANDEN_AESNI 1
#include <wmmintrin.h>

#elif defined(__powerpc__) && defined(__VSX__)

#define RANDEN_PPC 1
#define RANDEN_BIG_ENDIAN 1
#include <altivec.h>

#elif defined(__ARM_NEON) && defined(__ARM_FEATURE_CRYPTO)

#define RANDEN_ARM 1
#include <arm_neon.h>

#else
#error "Port"
#endif

#if defined(__clang__) || defined(__GNUC__)
#define RANDEN_INLINE inline __attribute__((always_inline))
#define RANDEN_RESTRICT __restrict__
#else
#define RANDEN_INLINE
#define RANDEN_RESTRICT
#endif

namespace randen {

#ifdef RANDEN_AESNI

class V {
 public:
  RANDEN_INLINE V() {}  // Leaves v_ uninitialized.
  RANDEN_INLINE V& operator=(const V other) {
    raw_ = other.raw_;
    return *this;
  }

  // Convert from/to intrinsics.
  RANDEN_INLINE explicit V(const __m128i raw) : raw_(raw) {}
  __m128i raw() const { return raw_; }

  RANDEN_INLINE V& operator^=(const V other) {
    raw_ = _mm_xor_si128(raw_, other.raw_);
    return *this;
  }

 private:
  // Note: this wrapper is faster than using __m128i directly.
  __m128i raw_;
};

#elif defined(RANDEN_PPC)

// Already provides operator^=.
using V = vector unsigned long long;

#elif defined(RANDEN_ARM)

// Already provides operator^=.
using V = uint8x16_t;

#else
#error "Port"
#endif

constexpr int kLanes = sizeof(V) / sizeof(uint64_t);

// On big-endian platforms, byte-swap constants (e.g. round keys) to ensure
// results match little-endian platforms.
#ifdef RANDEN_BIG_ENDIAN
#define RANDEN_LE(a, b) __builtin_bswap64(b), __builtin_bswap64(a)
#else
#define RANDEN_LE(a, b) a, b
#endif

#ifdef RANDEN_BIG_ENDIAN
static RANDEN_INLINE V ReverseBytes(const V v) {
  // Reverses the bytes of the vector.
  const vector unsigned char perm = {15, 14, 13, 12, 11, 10, 9, 8,
                                     7,  6,  5,  4,  3,  2,  1, 0};
  return vec_perm(v, v, perm);
}
#endif

// WARNING: these load/store in native byte order. It is OK to load and then
// store an unchanged vector, but interpreting the bits as a number or input
// to AES will have platform-dependent results. Call ReverseBytes after load
// and/or before store #ifdef RANDEN_BIG_ENDIAN.

static RANDEN_INLINE V Load(const uint64_t* RANDEN_RESTRICT lanes,
                            const int block) {
#ifdef RANDEN_AESNI
  const uint64_t* RANDEN_RESTRICT from = lanes + block * kLanes;
  return V(_mm_load_si128(reinterpret_cast<const __m128i*>(from)));
#elif defined(RANDEN_PPC)
  const V* RANDEN_RESTRICT from =
      reinterpret_cast<const V*>(lanes + block * kLanes);
  return vec_vsx_ld(0, from);
#elif defined(RANDEN_ARM)
  const uint8_t* RANDEN_RESTRICT from =
      reinterpret_cast<const uint8_t*>(lanes + block * kLanes);
  return vld1q_u8(from);
#else
#error "Port"
#endif
}

static RANDEN_INLINE void Store(const V v, uint64_t* RANDEN_RESTRICT lanes,
                                const int block) {
#ifdef RANDEN_AESNI
  uint64_t* RANDEN_RESTRICT to = lanes + block * kLanes;
  _mm_store_si128(reinterpret_cast<__m128i * RANDEN_RESTRICT>(to), v.raw());
#elif defined(RANDEN_PPC)
  V* RANDEN_RESTRICT to = reinterpret_cast<V*>(lanes + block * kLanes);
  vec_vsx_st(v, 0, to);
#elif defined(RANDEN_ARM)
  uint8_t* RANDEN_RESTRICT to =
      reinterpret_cast<uint8_t*>(lanes + block * kLanes);
  vst1q_u8(to, v);
#else
#error "Port"
#endif
}

// One round of AES. "round_key" is a public constant for breaking the
// symmetry of AES (ensures previously equal columns differ afterwards).
static RANDEN_INLINE V AES(const V state, const V round_key) {
#ifdef RANDEN_AESNI
  // It is important to always use the full round function - omitting the
  // final MixColumns reduces security [https://eprint.iacr.org/2010/041.pdf]
  // and does not help because we never decrypt.
  return V(_mm_aesenc_si128(state.raw(), round_key.raw()));
#elif defined(RANDEN_PPC)
  return V(__builtin_crypto_vcipher(state, round_key));
#elif defined(RANDEN_ARM)
  return vaesmcq_u8(vaeseq_u8(state, round_key));
#else
#error "Port"
#endif
}

}  // namespace randen

#endif  // VECTOR128_H_
