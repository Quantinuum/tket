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

#include <catch2/catch_test_macros.hpp>

#include "tket/Diagonalisation/PauliPartition.hpp"

namespace tket {
namespace test_Partition {

SCENARIO("Small sets of Gadgets are partitioned correctly") {
  const std::list<PauliPartitionStrat> strats{
      PauliPartitionStrat::NonConflictingSets,
      PauliPartitionStrat::CommutingSets};

  // NOTE: all methods seem to give the same results,
  // which is not surprising for small sets.
  // It would be good for a subject expert to add
  // more extensive tests with larger sets.
  const std::vector<GraphColourMethod> colouring_methods{
      GraphColourMethod::LargestFirst, GraphColourMethod::Exhaustive,
      GraphColourMethod::Lazy};

// https://github.com/Quantinuum/tket/issues/2199
#ifdef NDEBUG
  GIVEN("No gadgets") {
    for (auto colouring_method : colouring_methods) {
      for (PauliPartitionStrat strat : strats) {
        std::list<SpPauliString> tensors;
        std::list<std::list<SpPauliString>> void_terms =
            term_sequence(tensors, strat, colouring_method);
        REQUIRE(void_terms.empty());
      }
    }
  }
#endif

  GIVEN("Two anti-commuting gadgets") {
    /* We know the correct order, as QubitOperator orders
    lexicographically */
    Qubit q0(0);
    Qubit q1(1);
    Qubit q2(2);
    SpPauliString qp_map0({{q0, Pauli::I}, {q1, Pauli::X}, {q2, Pauli::Y}});
    SpPauliString qp_map1({{q0, Pauli::Z}, {q1, Pauli::Z}, {q2, Pauli::Y}});
    std::list<SpPauliString> tensors{qp_map0, qp_map1};

    for (auto colouring_method : colouring_methods) {
      for (PauliPartitionStrat strat : strats) {
        std::list<std::list<SpPauliString>> terms =
            term_sequence(tensors, strat, colouring_method);

        REQUIRE(terms.size() == 2);
        REQUIRE(terms.begin()->size() == 1);
        REQUIRE(*(terms.begin()->begin()) == qp_map0);
        REQUIRE((++terms.begin())->size() == 1);
        REQUIRE(*((++terms.begin())->begin()) == qp_map1);
      }
    }
  }
  GIVEN("Three partitions of four gadgets") {
    SpPauliString qp_map0(Qubit(0), Pauli::I);
    SpPauliString qp_map1(Qubit(0), Pauli::X);
    SpPauliString qp_map2(Qubit(0), Pauli::Y);
    SpPauliString qp_map3(Qubit(0), Pauli::Z);
    std::list<SpPauliString> tensors{qp_map0, qp_map1, qp_map2, qp_map3};

    for (auto colouring_method : colouring_methods) {
      for (PauliPartitionStrat strat : strats) {
        std::list<std::list<SpPauliString>> terms =
            term_sequence(tensors, strat, colouring_method);

        REQUIRE(terms.size() == 3);
        unsigned total_terms = 0;
        for (const std::list<SpPauliString>& g_map : terms) {
          REQUIRE((g_map.size() == 1 || g_map.size() == 2));
          total_terms += g_map.size();
        }
        REQUIRE(total_terms == 4);
      }
    }
  }
}

}  // namespace test_Partition
}  // namespace tket
