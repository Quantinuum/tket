// Copyright Quantinuum
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

#pragma once

#include <boost/range/adaptor/transformed.hpp>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

#include "tket_export.h"

namespace tket {

typedef std::vector<std::deque<bool>> GrayCode;
/**
 * Construct a GrayCode over `n` bits
 * @param n bits
 */
GrayCode gen_graycode(unsigned n);

/**
 * Check equality of each element of first and second
 * when first and second can provide begin, end iterators
 **/
template <typename T>
bool check_iterators_equality(const T& first, const T& second) {
  auto it1 = first.begin();
  auto f_end = first.end();

  auto it2 = second.begin();
  auto s_end = second.end();

  for (; it1 != f_end; ++it1) {
    if (it2 == s_end || !(*it1 == *it2)) {
      return false;
    }
    ++it2;
  }

  // check there were the same number of elements
  return (it2 == s_end);
}

/*
 * Convert a map-like object (e.g. <bimap>.right/<bimap>.left) to std::map
 *
 * This is particularly useful for bimaps. Converting a bimap to a map is
 * slightly annoying, see
 * https://stackoverflow.com/questions/20667187/convert-boostbimap-to-stdmap
 */
template <typename MapT>
static std::map<
    std::remove_const_t<typename MapT::key_type>,
    std::remove_const_t<typename MapT::mapped_type>>
bimap_to_map(MapT& bm) {
  using retT = std::map<
      std::remove_const_t<typename MapT::key_type>,
      std::remove_const_t<typename MapT::mapped_type>>;
  using funcT = std::function<typename retT::value_type(
      const typename MapT::value_type&)>;
  funcT trans = [](const typename MapT::value_type& val) {
    return std::make_pair(val.first, val.second);
  };
  auto it = bm | boost::adaptors::transformed(trans);
  return retT(it.begin(), it.end());
}

/**
 * Reverse bits 0,1,...,w-1 of the number v, assuming v < 2^w and w <= 64.
 */
uint64_t reverse_bits(uint64_t v, unsigned w);

/**
 * @brief
 * convert an unsigned to its binary representation
 * big-endian
 */
TKET_EXPORT std::vector<bool> dec_to_bin(
    unsigned long long dec, unsigned width);

/**
 * @brief
 * convert an bit vector to its decimal representation
 * big-endian
 */
TKET_EXPORT unsigned long long bin_to_dec(const std::vector<bool>& bin);
}  // namespace tket
