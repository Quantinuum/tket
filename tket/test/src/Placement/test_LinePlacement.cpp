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

#include "../testutil.hpp"
#include "tket/Placement/Placement.hpp"

namespace tket {
SCENARIO("LinePlacement class") {
  GIVEN("Empty Circuit, Empty Architecture, LinePlacement::Place.") {
    Architecture architecture;
    REQUIRE_THROWS_AS(LinePlacement(architecture), std::logic_error);
  }

// https://github.com/Quantinuum/tket/issues/2199
#ifdef NDEBUG
  GIVEN("Empty circuit, two qubit Architecture, LinePlacement::Place.") {
    std::vector<std::pair<unsigned, unsigned>> edges = {{0, 1}};
    Architecture architecture(edges);
    Circuit circuit;
    LinePlacement placement(architecture);
    placement.place(circuit);
    REQUIRE(circuit.n_qubits() == 0);
  }
#endif

  GIVEN("Single qubit circuit, two qubit Architecture, LinePlacement::Place") {
    std::vector<std::pair<unsigned, unsigned>> edges = {{0, 1}};
    Architecture architecture(edges);
    Circuit circuit(1);
    LinePlacement placement(architecture);
    placement.place(circuit);
    REQUIRE(circuit.all_qubits()[0] == Qubit(Placement::unplaced_reg(), 0));
  }
  GIVEN(
      "Two qubit unconnected circuit, two qubit Architecture, "
      "LinePlacement::Place") {
    std::vector<std::pair<unsigned, unsigned>> edges = {{0, 1}};
    Architecture architecture(edges);
    Circuit circuit(2);
    LinePlacement placement(architecture);
    placement.place(circuit);
    REQUIRE(circuit.all_qubits()[0] == Qubit(Placement::unplaced_reg(), 0));
    REQUIRE(circuit.all_qubits()[1] == Qubit(Placement::unplaced_reg(), 1));
  }
  GIVEN(
      "Three qubit unconnected circuit, two qubit Architecture, "
      "LinePlacement::Place") {
    std::vector<std::pair<unsigned, unsigned>> edges = {{0, 1}};
    Architecture architecture(edges);
    Circuit circuit(3);
    LinePlacement placement(architecture);
    REQUIRE_THROWS_AS(placement.place(circuit), std::invalid_argument);
  }
  GIVEN(
      "Two qubit connected circuit, three qubit Architecture, "
      "LinePlacement::get_placement_map") {
    std::vector<std::pair<unsigned, unsigned>> edges = {{0, 1}, {1, 2}};
    Architecture architecture(edges);
    Circuit circuit(2);
    circuit.add_op<unsigned>(OpType::CX, {1, 0});
    LinePlacement placement(architecture);
    std::map<Qubit, Node> map = placement.get_placement_map(circuit);
    REQUIRE(map[Qubit(0)] == Node(2));
    REQUIRE(map[Qubit(1)] == Node(1));
  }
  GIVEN(
      "Five qubit connected CX circuit, five qubit Architecture, "
      "LinePlacement::get_placement_map") {
    /**
     * Architecture graph:
     *      4
     *      |
     * 2 -- 0 -- 1
     *      |
     *      3
     */
    std::vector<std::pair<unsigned, unsigned>> edges = {
        {0, 1}, {0, 2}, {0, 3}, {0, 4}};
    Architecture architecture(edges);
    /**
     * Qubit interaction graph:
     *      4
     *      |
     * 2 -- 0 -- 1
     *      |
     *      3
     */
    Circuit circuit(5);
    add_2qb_gates(circuit, OpType::CX, {{0, 1}, {0, 2}, {0, 3}, {0, 4}});
    LinePlacement placement(architecture);
    std::map<Qubit, Node> map = placement.get_placement_map(circuit);
    REQUIRE(map[Qubit(0)] == Node(0));
    REQUIRE(map[Qubit(1)] == Node(3));
    REQUIRE(map[Qubit(2)] == Node(4));
    REQUIRE(map[Qubit(3)] == Qubit(Placement::unplaced_reg(), 0));
    REQUIRE(map[Qubit(4)] == Qubit(Placement::unplaced_reg(), 1));
  }
  GIVEN(
      "A four qubit circuit, four qubit architecture, "
      "LinePlacement::get_placement_map.") {
    /**
     * Architecture graph:
     * 0 -- 1 -- 2 -- 3
     */
    std::vector<std::pair<unsigned, unsigned>> edges = {{0, 1}, {1, 2}, {2, 3}};
    Architecture architecture(edges);
    /**
     * Qubit interaction graph:
     * 0 -- 1 -- 2
     *      |
     *      3
     */
    Circuit circuit(4);
    add_2qb_gates(circuit, OpType::CX, {{0, 1}, {2, 1}, {3, 1}});
    LinePlacement placement(architecture);
    std::map<Qubit, Node> map = placement.get_placement_map(circuit);
    REQUIRE(map[Qubit(0)] == Node(1));
    REQUIRE(map[Qubit(1)] == Node(2));
    REQUIRE(map[Qubit(2)] == Node(3));
    REQUIRE(map[Qubit(3)] == Qubit(Placement::unplaced_reg(), 0));
  }
}
}  // namespace tket
