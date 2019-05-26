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

#include "randen.h"

#include <string.h>     // memcpy

#include "vector128.h"

namespace randen {
namespace {

// High-level summary:
// 1) Reverie (see "A Robust and Sponge-Like PRNG with Improved Efficiency") is
//    a sponge-like random generator that requires a cryptographic permutation.
//    It improves upon "Provably Robust Sponge-Based PRNGs and KDFs" by
//    achieving backtracking resistance with only one Permute() per buffer.
//
// 2) "Simpira v2: A Family of Efficient Permutations Using the AES Round
//    Function" constructs up to 1024-bit permutations using an improved
//    Generalized Feistel network with 2-round AES-128 functions. This Feistel
//    block shuffle achieves diffusion faster and is less vulnerable to
//    sliced-biclique attacks than the Type-2 cyclic shuffle.
//
// 3) "Improving the Generalized Feistel" and "New criterion for diffusion
//    property" extends the same kind of improved Feistel block shuffle to 16
//    branches, which enables a 2048-bit permutation.
//
// We combine these three ideas and also change Simpira's subround keys from
// structured/low-entropy counters to digits of Pi.

// Largest size for which security proofs are known.
constexpr int kFeistelBlocks = 16;

// Type-2 generalized Feistel => one round function for every two blocks.
constexpr int kFeistelFunctions = kFeistelBlocks / 2;  // = 8

// Ensures SPRP security and two full subblock diffusions.
constexpr int kFeistelRounds = 16 + 1;  // > 4 * log2(kFeistelBlocks)

// Independent keys (272 = 2.1 KiB) for the first AES subround of each function.
constexpr int kKeys = kFeistelRounds * kFeistelFunctions;

const uint64_t* RANDEN_RESTRICT Keys() {
  // "Nothing up my sleeve" numbers from the first hex digits of Pi, obtained
  // from http://hexpi.sourceforge.net/. Native byte order.
  alignas(32) static constexpr uint64_t pi_digits[kKeys * kLanes] = {
      RANDEN_LE(0x243F6A8885A308D3ull, 0x13198A2E03707344ull),
      RANDEN_LE(0xA4093822299F31D0ull, 0x082EFA98EC4E6C89ull),
      RANDEN_LE(0x452821E638D01377ull, 0xBE5466CF34E90C6Cull),
      RANDEN_LE(0xC0AC29B7C97C50DDull, 0x3F84D5B5B5470917ull),
      RANDEN_LE(0x9216D5D98979FB1Bull, 0xD1310BA698DFB5ACull),
      RANDEN_LE(0x2FFD72DBD01ADFB7ull, 0xB8E1AFED6A267E96ull),
      RANDEN_LE(0xBA7C9045F12C7F99ull, 0x24A19947B3916CF7ull),
      RANDEN_LE(0x0801F2E2858EFC16ull, 0x636920D871574E69ull),
      RANDEN_LE(0xA458FEA3F4933D7Eull, 0x0D95748F728EB658ull),
      RANDEN_LE(0x718BCD5882154AEEull, 0x7B54A41DC25A59B5ull),
      RANDEN_LE(0x9C30D5392AF26013ull, 0xC5D1B023286085F0ull),
      RANDEN_LE(0xCA417918B8DB38EFull, 0x8E79DCB0603A180Eull),
      RANDEN_LE(0x6C9E0E8BB01E8A3Eull, 0xD71577C1BD314B27ull),
      RANDEN_LE(0x78AF2FDA55605C60ull, 0xE65525F3AA55AB94ull),
      RANDEN_LE(0x5748986263E81440ull, 0x55CA396A2AAB10B6ull),
      RANDEN_LE(0xB4CC5C341141E8CEull, 0xA15486AF7C72E993ull),
      RANDEN_LE(0xB3EE1411636FBC2Aull, 0x2BA9C55D741831F6ull),
      RANDEN_LE(0xCE5C3E169B87931Eull, 0xAFD6BA336C24CF5Cull),
      RANDEN_LE(0x7A32538128958677ull, 0x3B8F48986B4BB9AFull),
      RANDEN_LE(0xC4BFE81B66282193ull, 0x61D809CCFB21A991ull),
      RANDEN_LE(0x487CAC605DEC8032ull, 0xEF845D5DE98575B1ull),
      RANDEN_LE(0xDC262302EB651B88ull, 0x23893E81D396ACC5ull),
      RANDEN_LE(0x0F6D6FF383F44239ull, 0x2E0B4482A4842004ull),
      RANDEN_LE(0x69C8F04A9E1F9B5Eull, 0x21C66842F6E96C9Aull),
      RANDEN_LE(0x670C9C61ABD388F0ull, 0x6A51A0D2D8542F68ull),
      RANDEN_LE(0x960FA728AB5133A3ull, 0x6EEF0B6C137A3BE4ull),
      RANDEN_LE(0xBA3BF0507EFB2A98ull, 0xA1F1651D39AF0176ull),
      RANDEN_LE(0x66CA593E82430E88ull, 0x8CEE8619456F9FB4ull),
      RANDEN_LE(0x7D84A5C33B8B5EBEull, 0xE06F75D885C12073ull),
      RANDEN_LE(0x401A449F56C16AA6ull, 0x4ED3AA62363F7706ull),
      RANDEN_LE(0x1BFEDF72429B023Dull, 0x37D0D724D00A1248ull),
      RANDEN_LE(0xDB0FEAD349F1C09Bull, 0x075372C980991B7Bull),
      RANDEN_LE(0x25D479D8F6E8DEF7ull, 0xE3FE501AB6794C3Bull),
      RANDEN_LE(0x976CE0BD04C006BAull, 0xC1A94FB6409F60C4ull),
      RANDEN_LE(0x5E5C9EC2196A2463ull, 0x68FB6FAF3E6C53B5ull),
      RANDEN_LE(0x1339B2EB3B52EC6Full, 0x6DFC511F9B30952Cull),
      RANDEN_LE(0xCC814544AF5EBD09ull, 0xBEE3D004DE334AFDull),
      RANDEN_LE(0x660F2807192E4BB3ull, 0xC0CBA85745C8740Full),
      RANDEN_LE(0xD20B5F39B9D3FBDBull, 0x5579C0BD1A60320Aull),
      RANDEN_LE(0xD6A100C6402C7279ull, 0x679F25FEFB1FA3CCull),
      RANDEN_LE(0x8EA5E9F8DB3222F8ull, 0x3C7516DFFD616B15ull),
      RANDEN_LE(0x2F501EC8AD0552ABull, 0x323DB5FAFD238760ull),
      RANDEN_LE(0x53317B483E00DF82ull, 0x9E5C57BBCA6F8CA0ull),
      RANDEN_LE(0x1A87562EDF1769DBull, 0xD542A8F6287EFFC3ull),
      RANDEN_LE(0xAC6732C68C4F5573ull, 0x695B27B0BBCA58C8ull),
      RANDEN_LE(0xE1FFA35DB8F011A0ull, 0x10FA3D98FD2183B8ull),
      RANDEN_LE(0x4AFCB56C2DD1D35Bull, 0x9A53E479B6F84565ull),
      RANDEN_LE(0xD28E49BC4BFB9790ull, 0xE1DDF2DAA4CB7E33ull),
      RANDEN_LE(0x62FB1341CEE4C6E8ull, 0xEF20CADA36774C01ull),
      RANDEN_LE(0xD07E9EFE2BF11FB4ull, 0x95DBDA4DAE909198ull),
      RANDEN_LE(0xEAAD8E716B93D5A0ull, 0xD08ED1D0AFC725E0ull),
      RANDEN_LE(0x8E3C5B2F8E7594B7ull, 0x8FF6E2FBF2122B64ull),
      RANDEN_LE(0x8888B812900DF01Cull, 0x4FAD5EA0688FC31Cull),
      RANDEN_LE(0xD1CFF191B3A8C1ADull, 0x2F2F2218BE0E1777ull),
      RANDEN_LE(0xEA752DFE8B021FA1ull, 0xE5A0CC0FB56F74E8ull),
      RANDEN_LE(0x18ACF3D6CE89E299ull, 0xB4A84FE0FD13E0B7ull),
      RANDEN_LE(0x7CC43B81D2ADA8D9ull, 0x165FA26680957705ull),
      RANDEN_LE(0x93CC7314211A1477ull, 0xE6AD206577B5FA86ull),
      RANDEN_LE(0xC75442F5FB9D35CFull, 0xEBCDAF0C7B3E89A0ull),
      RANDEN_LE(0xD6411BD3AE1E7E49ull, 0x00250E2D2071B35Eull),
      RANDEN_LE(0x226800BB57B8E0AFull, 0x2464369BF009B91Eull),
      RANDEN_LE(0x5563911D59DFA6AAull, 0x78C14389D95A537Full),
      RANDEN_LE(0x207D5BA202E5B9C5ull, 0x832603766295CFA9ull),
      RANDEN_LE(0x11C819684E734A41ull, 0xB3472DCA7B14A94Aull),
      RANDEN_LE(0x1B5100529A532915ull, 0xD60F573FBC9BC6E4ull),
      RANDEN_LE(0x2B60A47681E67400ull, 0x08BA6FB5571BE91Full),
      RANDEN_LE(0xF296EC6B2A0DD915ull, 0xB6636521E7B9F9B6ull),
      RANDEN_LE(0xFF34052EC5855664ull, 0x53B02D5DA99F8FA1ull),
      RANDEN_LE(0x08BA47996E85076Aull, 0x4B7A70E9B5B32944ull),
      RANDEN_LE(0xDB75092EC4192623ull, 0xAD6EA6B049A7DF7Dull),
      RANDEN_LE(0x9CEE60B88FEDB266ull, 0xECAA8C71699A18FFull),
      RANDEN_LE(0x5664526CC2B19EE1ull, 0x193602A575094C29ull),
      RANDEN_LE(0xA0591340E4183A3Eull, 0x3F54989A5B429D65ull),
      RANDEN_LE(0x6B8FE4D699F73FD6ull, 0xA1D29C07EFE830F5ull),
      RANDEN_LE(0x4D2D38E6F0255DC1ull, 0x4CDD20868470EB26ull),
      RANDEN_LE(0x6382E9C6021ECC5Eull, 0x09686B3F3EBAEFC9ull),
      RANDEN_LE(0x3C9718146B6A70A1ull, 0x687F358452A0E286ull),
      RANDEN_LE(0xB79C5305AA500737ull, 0x3E07841C7FDEAE5Cull),
      RANDEN_LE(0x8E7D44EC5716F2B8ull, 0xB03ADA37F0500C0Dull),
      RANDEN_LE(0xF01C1F040200B3FFull, 0xAE0CF51A3CB574B2ull),
      RANDEN_LE(0x25837A58DC0921BDull, 0xD19113F97CA92FF6ull),
      RANDEN_LE(0x9432477322F54701ull, 0x3AE5E58137C2DADCull),
      RANDEN_LE(0xC8B576349AF3DDA7ull, 0xA94461460FD0030Eull),
      RANDEN_LE(0xECC8C73EA4751E41ull, 0xE238CD993BEA0E2Full),
      RANDEN_LE(0x3280BBA1183EB331ull, 0x4E548B384F6DB908ull),
      RANDEN_LE(0x6F420D03F60A04BFull, 0x2CB8129024977C79ull),
      RANDEN_LE(0x5679B072BCAF89AFull, 0xDE9A771FD9930810ull),
      RANDEN_LE(0xB38BAE12DCCF3F2Eull, 0x5512721F2E6B7124ull),
      RANDEN_LE(0x501ADDE69F84CD87ull, 0x7A5847187408DA17ull),
      RANDEN_LE(0xBC9F9ABCE94B7D8Cull, 0xEC7AEC3ADB851DFAull),
      RANDEN_LE(0x63094366C464C3D2ull, 0xEF1C18473215D808ull),
      RANDEN_LE(0xDD433B3724C2BA16ull, 0x12A14D432A65C451ull),
      RANDEN_LE(0x50940002133AE4DDull, 0x71DFF89E10314E55ull),
      RANDEN_LE(0x81AC77D65F11199Bull, 0x043556F1D7A3C76Bull),
      RANDEN_LE(0x3C11183B5924A509ull, 0xF28FE6ED97F1FBFAull),
      RANDEN_LE(0x9EBABF2C1E153C6Eull, 0x86E34570EAE96FB1ull),
      RANDEN_LE(0x860E5E0A5A3E2AB3ull, 0x771FE71C4E3D06FAull),
      RANDEN_LE(0x2965DCB999E71D0Full, 0x803E89D65266C825ull),
      RANDEN_LE(0x2E4CC9789C10B36Aull, 0xC6150EBA94E2EA78ull),
      RANDEN_LE(0xA6FC3C531E0A2DF4ull, 0xF2F74EA7361D2B3Dull),
      RANDEN_LE(0x1939260F19C27960ull, 0x5223A708F71312B6ull),
      RANDEN_LE(0xEBADFE6EEAC31F66ull, 0xE3BC4595A67BC883ull),
      RANDEN_LE(0xB17F37D1018CFF28ull, 0xC332DDEFBE6C5AA5ull),
      RANDEN_LE(0x6558218568AB9702ull, 0xEECEA50FDB2F953Bull),
      RANDEN_LE(0x2AEF7DAD5B6E2F84ull, 0x1521B62829076170ull),
      RANDEN_LE(0xECDD4775619F1510ull, 0x13CCA830EB61BD96ull),
      RANDEN_LE(0x0334FE1EAA0363CFull, 0xB5735C904C70A239ull),
      RANDEN_LE(0xD59E9E0BCBAADE14ull, 0xEECC86BC60622CA7ull),
      RANDEN_LE(0x9CAB5CABB2F3846Eull, 0x648B1EAF19BDF0CAull),
      RANDEN_LE(0xA02369B9655ABB50ull, 0x40685A323C2AB4B3ull),
      RANDEN_LE(0x319EE9D5C021B8F7ull, 0x9B540B19875FA099ull),
      RANDEN_LE(0x95F7997E623D7DA8ull, 0xF837889A97E32D77ull),
      RANDEN_LE(0x11ED935F16681281ull, 0x0E358829C7E61FD6ull),
      RANDEN_LE(0x96DEDFA17858BA99ull, 0x57F584A51B227263ull),
      RANDEN_LE(0x9B83C3FF1AC24696ull, 0xCDB30AEB532E3054ull),
      RANDEN_LE(0x8FD948E46DBC3128ull, 0x58EBF2EF34C6FFEAull),
      RANDEN_LE(0xFE28ED61EE7C3C73ull, 0x5D4A14D9E864B7E3ull),
      RANDEN_LE(0x42105D14203E13E0ull, 0x45EEE2B6A3AAABEAull),
      RANDEN_LE(0xDB6C4F15FACB4FD0ull, 0xC742F442EF6ABBB5ull),
      RANDEN_LE(0x654F3B1D41CD2105ull, 0xD81E799E86854DC7ull),
      RANDEN_LE(0xE44B476A3D816250ull, 0xCF62A1F25B8D2646ull),
      RANDEN_LE(0xFC8883A0C1C7B6A3ull, 0x7F1524C369CB7492ull),
      RANDEN_LE(0x47848A0B5692B285ull, 0x095BBF00AD19489Dull),
      RANDEN_LE(0x1462B17423820D00ull, 0x58428D2A0C55F5EAull),
      RANDEN_LE(0x1DADF43E233F7061ull, 0x3372F0928D937E41ull),
      RANDEN_LE(0xD65FECF16C223BDBull, 0x7CDE3759CBEE7460ull),
      RANDEN_LE(0x4085F2A7CE77326Eull, 0xA607808419F8509Eull),
      RANDEN_LE(0xE8EFD85561D99735ull, 0xA969A7AAC50C06C2ull),
      RANDEN_LE(0x5A04ABFC800BCADCull, 0x9E447A2EC3453484ull),
      RANDEN_LE(0xFDD567050E1E9EC9ull, 0xDB73DBD3105588CDull),
      RANDEN_LE(0x675FDA79E3674340ull, 0xC5C43465713E38D8ull),
      RANDEN_LE(0x3D28F89EF16DFF20ull, 0x153E21E78FB03D4Aull),
      RANDEN_LE(0xE6E39F2BDB83ADF7ull, 0xE93D5A68948140F7ull),
      RANDEN_LE(0xF64C261C94692934ull, 0x411520F77602D4F7ull),
      RANDEN_LE(0xBCF46B2ED4A10068ull, 0xD40824713320F46Aull),
      RANDEN_LE(0x43B7D4B7500061AFull, 0x1E39F62E97244546ull)};
  static_assert(pi_digits[kKeys * kLanes - 1] != 0, "Too few initializers");
  return pi_digits;
}

// Improved odd-even shuffle from "New criterion for diffusion property".
RANDEN_INLINE void BlockShuffle(uint64_t* RANDEN_RESTRICT state) {
  // First make a copy (optimized out).
  uint64_t source[kFeistelBlocks * kLanes];
  memcpy(source, state, sizeof(source));

  constexpr int shuffle[kFeistelBlocks] = {7,  2, 13, 4,  11, 8,  3, 6,
                                           15, 0, 9,  10, 1,  14, 5, 12};
  for (int branch = 0; branch < kFeistelBlocks; ++branch) {
    const V v = Load(source, shuffle[branch]);
    Store(v, state, branch);
  }
}

// Cryptographic permutation based via type-2 Generalized Feistel Network.
// Indistinguishable from ideal by chosen-ciphertext adversaries using less than
// 2^64 queries if the round function is a PRF. This is similar to the b=8 case
// of Simpira v2, but more efficient than its generic construction for b=16.
RANDEN_INLINE void Permute(uint64_t* RANDEN_RESTRICT state) {
  // Round keys for one AES per Feistel round and branch: first digits of Pi.
  const uint64_t* RANDEN_RESTRICT keys = Keys();

  // (Successfully unrolled; the first iteration jumps into the second half)
#ifdef __clang__
#pragma clang loop unroll_count(2)
#endif
  for (int round = 0; round < kFeistelRounds; ++round) {
    for (int branch = 0; branch < kFeistelBlocks; branch += 2) {
      const V even = Load(state, branch);
      const V odd = Load(state, branch + 1);
      // Feistel round function using two AES subrounds. Very similar to F()
      // from Simpira v2, but with independent subround keys. Uses 17 AES rounds
      // per 16 bytes (vs. 10 for AES-CTR). Computing eight round functions in
      // parallel hides the 7-cycle AESNI latency on HSW. Note that the Feistel
      // XORs are 'free' (included in the second AES instruction).
      const V f1 = AES(even, Load(keys, 0));
      keys += kLanes;
      const V f2 = AES(f1, odd);
      Store(f2, state, branch + 1);
    }

    BlockShuffle(state);
  }
}

// Enables native loads in the round loop by pre-swapping.
RANDEN_INLINE void SwapIfBigEndian(uint64_t* RANDEN_RESTRICT state) {
#ifdef RANDEN_BIG_ENDIAN
  for (int branch = 0; branch < kFeistelBlocks; ++branch) {
    const V v = ReverseBytes(Load(state, branch));
    Store(v, state, branch);
  }
#endif
}

}  // namespace

void Internal::Absorb(const void* seed_void, void* state_void) {
  uint64_t* RANDEN_RESTRICT state = reinterpret_cast<uint64_t*>(state_void);
  const uint64_t* RANDEN_RESTRICT seed =
      reinterpret_cast<const uint64_t*>(seed_void);

  constexpr int kCapacityBlocks = kCapacityBytes / sizeof(V);
  static_assert(kCapacityBlocks * sizeof(V) == kCapacityBytes, "Not i*V");
  for (size_t i = kCapacityBlocks; i < kStateBytes / sizeof(V); ++i) {
    V block = Load(state, i);
    block ^= Load(seed, i - kCapacityBlocks);
    Store(block, state, i);
  }
}

void Internal::Generate(void* state_void) {
  uint64_t* RANDEN_RESTRICT state = reinterpret_cast<uint64_t*>(state_void);

  static_assert(kCapacityBytes == sizeof(V), "Capacity mismatch");
  const V prev_inner = Load(state, 0);

  SwapIfBigEndian(state);

  Permute(state);

  SwapIfBigEndian(state);

  // Ensure backtracking resistance.
  V inner = Load(state, 0);
  inner ^= prev_inner;
  Store(inner, state, 0);
}

}  // namespace randen
