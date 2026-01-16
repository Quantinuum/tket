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
#include <tket/Transformations/Decomposition.hpp>

#include "CircuitsForTesting.hpp"
#include "testutil.hpp"
#include "tket/Circuit/Boxes.hpp"
#include "tket/Circuit/CircUtils.hpp"
#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Converters/Converters.hpp"
#include "tket/Diagonalisation/Diagonalisation.hpp"
#include "tket/Gate/SymTable.hpp"
#include "tket/PauliGraph/ConjugatePauliFunctions.hpp"
#include "tket/PauliGraph/PauliGraph.hpp"
#include "tket/Transformations/PauliOptimisation.hpp"
#include "tket/Transformations/Rebase.hpp"
#include "tket/Utils/UnitID.hpp"

namespace tket {
namespace test_PauliGraph {

SCENARIO("Correct creation of PauliGraphs") {
  GIVEN("A Clifford circuit") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::S, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::Vdg, {1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    circ.add_op<unsigned>(OpType::Phase, 0.8, {});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    UnitaryRevTableau correct_tab = circuit_to_unitary_rev_tableau(circ);
    REQUIRE(pg.get_clifford_ref() == correct_tab);
  }
  GIVEN("A 1qb circuit") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.6, {0});
    circ.add_op<unsigned>(OpType::Ry, 1.2, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 4);
  }
  GIVEN("A 2qb circuit with no interaction") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.6, {0});
    circ.add_op<unsigned>(OpType::Ry, 1.2, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.2, {1});
    circ.add_op<unsigned>(OpType::Rx, 1.6, {1});
    circ.add_op<unsigned>(OpType::Rz, 1.3, {1});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 7);
  }
  GIVEN("A 2qb circuit with some anti-commuting interaction") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.2, {1});
    circ.add_op<unsigned>(OpType::XXPhase, 1.1, {0, 1});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 3);
  }
  GIVEN("A 2qb circuit with some commuting interaction") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.2, {1});
    circ.add_op<unsigned>(OpType::ZZPhase, 1.1, {0, 1});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 3);
  }
  GIVEN("A 2qb circuit a Clifford-angled ZZPhase") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.2, {1});
    circ.add_op<unsigned>(OpType::ZZPhase, 0.5, {0, 1});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 2);
  }
  GIVEN("A 1qb circuit with stuff to merge") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 1.3, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.6, {0});
    circ.add_op<unsigned>(OpType::Rz, 1.1, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 3);
  }
  GIVEN("A 2qb circuit with stuff to merge") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.2, {1});
    circ.add_op<unsigned>(OpType::ZZPhase, 1.1, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.8, {0});
    circ.add_op<unsigned>(OpType::ZZPhase, 1.6, {1, 0});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 3);
  }
  GIVEN("A circuit with Cliffords and non-Cliffords") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.4, {0});
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::Rz, 1.1, {0});
    circ.add_op<unsigned>(OpType::Rz, 1.8, {1});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 3);
  }
  GIVEN("A dense example") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {2});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {3});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {1});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {2});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {3});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {2});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {3});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {1});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {2});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {3});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 16);
  }
  GIVEN("A more interesting example (tof_3)") {
    Circuit circ(5);
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::H, {4});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::Tdg, {4});
    circ.add_op<unsigned>(OpType::CX, {0, 4});
    circ.add_op<unsigned>(OpType::T, {4});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::Tdg, {4});
    circ.add_op<unsigned>(OpType::CX, {0, 4});
    circ.add_op<unsigned>(OpType::T, {4});
    circ.add_op<unsigned>(OpType::T, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::Tdg, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::H, {4});
    circ.add_op<unsigned>(OpType::H, {4});
    circ.add_op<unsigned>(OpType::H, {4});
    circ.add_op<unsigned>(OpType::CX, {4, 3});
    circ.add_op<unsigned>(OpType::Tdg, {3});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::T, {3});
    circ.add_op<unsigned>(OpType::CX, {4, 3});
    circ.add_op<unsigned>(OpType::Tdg, {3});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::T, {3});
    circ.add_op<unsigned>(OpType::T, {4});
    circ.add_op<unsigned>(OpType::CX, {2, 4});
    circ.add_op<unsigned>(OpType::T, {2});
    circ.add_op<unsigned>(OpType::Tdg, {4});
    circ.add_op<unsigned>(OpType::CX, {2, 4});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::H, {4});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::Tdg, {4});
    circ.add_op<unsigned>(OpType::CX, {0, 4});
    circ.add_op<unsigned>(OpType::T, {4});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::Tdg, {4});
    circ.add_op<unsigned>(OpType::CX, {0, 4});
    circ.add_op<unsigned>(OpType::T, {4});
    circ.add_op<unsigned>(OpType::T, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::Tdg, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::H, {4});
    circ.add_op<unsigned>(OpType::H, {4});
    circ.add_op<unsigned>(OpType::H, {4});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 15);
  }
  GIVEN("A circuit with a PauliExpBox") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::ZZPhase, 0.2, {0, 1});
    circ.add_op<unsigned>(OpType::Vdg, {0});
    circ.add_op<unsigned>(OpType::H, {1});
    PauliExpBox peb({{Pauli::Y, Pauli::X}, 0.333});
    circ.add_box(peb, {0, 1});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    REQUIRE(pg.n_vertices() == 1);
  }
}

SCENARIO("TopSortIterator") {
  GIVEN("An empty circuit") {
    Circuit circ(2);
    REQUIRE_NOTHROW(circuit_to_pauli_graph(circ));
  }
}

SCENARIO("Synthesising PauliGraphs") {
  GIVEN("A dense example") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {2});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {3});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {1});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {2});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {3});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {2});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {3});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {1});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {2});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {3});
    const Eigen::MatrixXcd circ_unitary = tket_sim::get_unitary(circ);
    PauliGraph pg = circuit_to_pauli_graph(circ);
    pg.sanity_check();
    WHEN("Synthesising individually") {
      Circuit synth = pauli_graph_to_pauli_exp_box_circuit_individually(pg);
      Transforms::decomp_boxes().apply(synth);
      pg.sanity_check();
      Eigen::MatrixXcd synth_unitary = tket_sim::get_unitary(synth);
      REQUIRE((synth_unitary - circ_unitary).cwiseAbs().sum() < ERR_EPS);
    }
    WHEN("Synthesising pairwise") {
      Circuit synth = pauli_graph_to_pauli_exp_box_circuit_pairwise(pg);
      Transforms::decomp_boxes().apply(synth);
      Eigen::MatrixXcd synth_unitary = tket_sim::get_unitary(synth);
      REQUIRE((synth_unitary - circ_unitary).cwiseAbs().sum() < ERR_EPS);
    }
  }
  GIVEN("A UCCSD example") {
    const auto& circ = CircuitsForTesting::get().uccsd;
    const auto circ_unitary = tket_sim::get_unitary(circ);
    const PauliGraph pg = circuit_to_pauli_graph(circ);
    WHEN("Synthesising individually") {
      Circuit synth = pauli_graph_to_pauli_exp_box_circuit_individually(pg);
      Transforms::decomp_boxes().apply(synth);
      const auto synth_unitary = tket_sim::get_unitary(synth);
      REQUIRE((synth_unitary - circ_unitary).cwiseAbs().sum() < ERR_EPS);
    }
    WHEN("Synthesising pairwise") {
      Circuit synth = pauli_graph_to_pauli_exp_box_circuit_pairwise(pg);
      Transforms::decomp_boxes().apply(synth);
      const auto synth_unitary = tket_sim::get_unitary(synth);
      REQUIRE((synth_unitary - circ_unitary).cwiseAbs().sum() < ERR_EPS);
    }
  }
}

// Probably no special significance, just something repeated several times
static void add_ops_to_prepend_1(Circuit& circ) {
  circ.add_op<unsigned>(OpType::Rx, 1.511, {2});
  circ.add_op<unsigned>(OpType::Rz, 0.745, {2});
}
static void add_ops_to_prepend_2(Circuit& circ) {
  circ.add_op<unsigned>(OpType::Rx, 0.849, {3});
  circ.add_op<unsigned>(OpType::Rz, 0.102, {3});
}

SCENARIO("Test mutual diagonalisation of fully commuting sets") {
  GIVEN("A 2 qb Identity gadget") {
    Circuit circ(2);
    PauliExpBox peb({{Pauli::I, Pauli::I}, 0.333});
    circ.add_box(peb, {0, 1});
    const auto& prepend = CircuitsForTesting::get().prepend_2qb_circuit;
    Circuit test1 = prepend >> circ;

    PauliGraph pg = circuit_to_pauli_graph(circ);
    pg.sanity_check();
    Circuit out = pauli_graph_to_pauli_exp_box_circuit_sets(pg);
    Transforms::decomp_boxes().apply(out);
    pg.sanity_check();
    Circuit test2 = prepend >> out;
    REQUIRE(test_statevector_comparison(test1, test2));
  }
  GIVEN("2 qb 1 Pauli gadget circuit") {
    const auto& prepend = CircuitsForTesting::get().prepend_2qb_circuit;
    Circuit circ(2);
    PauliExpBox peb({{Pauli::Z, Pauli::X}, 0.333});
    circ.add_box(peb, {0, 1});
    Circuit test1 = prepend >> circ;

    PauliGraph pg = circuit_to_pauli_graph(circ);
    Circuit out = pauli_graph_to_pauli_exp_box_circuit_sets(pg);
    Transforms::decomp_boxes().apply(out);
    Circuit test2 = prepend >> out;
    REQUIRE(test_statevector_comparison(test1, test2));
  }
  GIVEN("2 qb 2 Pauli gadget circuit") {
    const auto& prepend = CircuitsForTesting::get().prepend_2qb_circuit;
    Circuit circ(2);
    PauliExpBox peb({{Pauli::Z, Pauli::X}, 0.333});
    circ.add_box(peb, {0, 1});
    PauliExpBox peb2({{Pauli::Y, Pauli::Y}, 0.174});
    circ.add_box(peb2, {0, 1});
    Circuit test1 = prepend >> circ;

    PauliGraph pg = circuit_to_pauli_graph(circ);
    Circuit out = pauli_graph_to_pauli_exp_box_circuit_sets(pg);
    Transforms::decomp_boxes().apply(out);
    Circuit test2 = prepend >> out;
    REQUIRE(test_statevector_comparison(test1, test2));
  }
  GIVEN("2 qb 3 Pauli gadget circuit with symbols") {
    const auto& prepend = CircuitsForTesting::get().prepend_2qb_circuit;
    Circuit circ(2);
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);
    Sym b = SymTable::fresh_symbol("b");
    Expr eb(b);
    Sym c = SymTable::fresh_symbol("c");
    Expr ec(c);
    std::map<Sym, double, SymEngine::RCPBasicKeyLess> symbol_map = {
        {a, 0.3112}, {b, 1.178}, {c, -0.911}};

    PauliExpBox peb({{Pauli::Z, Pauli::Z}, ea});
    circ.add_box(peb, {0, 1});
    PauliExpBox peb2({{Pauli::X, Pauli::X}, eb});
    circ.add_box(peb2, {0, 1});
    PauliExpBox peb3({{Pauli::Y, Pauli::Y}, ec});
    circ.add_box(peb3, {0, 1});
    Circuit test1 = prepend >> circ;
    test1.symbol_substitution(symbol_map);

    PauliGraph pg = circuit_to_pauli_graph(circ);
    Circuit out = pauli_graph_to_pauli_exp_box_circuit_sets(pg);
    Transforms::decomp_boxes().apply(out);
    Circuit test2 = prepend >> out;
    test2.symbol_substitution(symbol_map);
    REQUIRE(test_statevector_comparison(test1, test2));
  }
  GIVEN("2 qb 3 Pauli gadget circuit with symbols and Pauli::I present") {
    const auto& prepend = CircuitsForTesting::get().prepend_2qb_circuit;
    Circuit circ(2);
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);
    Sym b = SymTable::fresh_symbol("b");
    Expr eb(b);
    Sym c = SymTable::fresh_symbol("c");
    Expr ec(c);
    std::map<Sym, double, SymEngine::RCPBasicKeyLess> symbol_map = {
        {a, 0.3112}, {b, 1.178}, {c, -0.911}};

    PauliExpBox peb({{Pauli::Z, Pauli::Z}, ea});
    circ.add_box(peb, {0, 1});
    PauliExpBox peb2({{Pauli::I, Pauli::X}, eb});
    circ.add_box(peb2, {0, 1});
    PauliExpBox peb3({{Pauli::Y, Pauli::I}, ec});
    circ.add_box(peb3, {0, 1});
    Circuit test1 = prepend >> circ;
    REQUIRE(test1.is_symbolic());
    test1.symbol_substitution(symbol_map);
    REQUIRE(!test1.is_symbolic());

    PauliGraph pg = circuit_to_pauli_graph(circ);
    Circuit out = pauli_graph_to_pauli_exp_box_circuit_sets(pg);
    Transforms::decomp_boxes().apply(out);
    Circuit test2 = prepend >> out;
    test2.symbol_substitution(symbol_map);
    REQUIRE(test_statevector_comparison(test1, test2));
  }
  GIVEN("3 qb 2 Pauli Gadget circuit") {
    auto prepend = CircuitsForTesting::get_prepend_circuit(3);
    add_ops_to_prepend_1(prepend);

    Circuit circ(3);
    PauliExpBox peb({{Pauli::Z, Pauli::X, Pauli::Z}, 0.333});
    circ.add_box(peb, {0, 1, 2});
    PauliExpBox peb2({{Pauli::Y, Pauli::X, Pauli::X}, 0.174});
    circ.add_box(peb2, {0, 1, 2});
    Circuit test1 = prepend >> circ;
    PauliGraph pg = circuit_to_pauli_graph(circ);
    Circuit out = pauli_graph_to_pauli_exp_box_circuit_sets(pg);
    Transforms::decomp_boxes().apply(out);
    Circuit test2 = prepend >> out;
    REQUIRE(test_statevector_comparison(test1, test2));
  }
  GIVEN("4 qb 3 Pauli Gadget circuit") {
    auto prepend = CircuitsForTesting::get_prepend_circuit(3);
    add_ops_to_prepend_1(prepend);

    Circuit circ(4);
    PauliExpBox peb({{Pauli::Z, Pauli::Z, Pauli::Z, Pauli::Z}, 0.333});
    circ.add_box(peb, {0, 1, 2, 3});
    PauliExpBox peb2({{Pauli::X, Pauli::Z, Pauli::X, Pauli::I}, 0.233});
    circ.add_box(peb2, {0, 1, 2, 3});
    PauliExpBox peb3({{Pauli::X, Pauli::X, Pauli::X, Pauli::X}, 0.174});
    circ.add_box(peb3, {0, 1, 2, 3});
    Circuit test1 = prepend >> circ;

    WHEN("Using default CX-decomposition") {
      PauliGraph pg = circuit_to_pauli_graph(circ);
      Circuit out = pauli_graph_to_pauli_exp_box_circuit_sets(pg);
      Transforms::decomp_boxes().apply(out);
      Circuit test2 = prepend >> out;
      REQUIRE(test_statevector_comparison(test1, test2));
    }

    WHEN("Using XXPhase3-decomposition") {
      PauliGraph pg = circuit_to_pauli_graph(circ);
      Circuit out = pauli_graph_to_pauli_exp_box_circuit_sets(
          pg, CXConfigType::MultiQGate);
      Transforms::decomp_boxes().apply(out);
      REQUIRE(out.count_gates(OpType::XXPhase3) == 2);
      Circuit test2 = prepend >> out;
      REQUIRE(test_statevector_comparison(test1, test2));
    }
  }
  GIVEN("3 qb 6 Pauli Gadget circuit for different strats and configs") {
    // add some arbitrary rotations to get away from |00> state
    Circuit circ(3, 3);
    CircuitsForTesting::add_initial_prepend_ops(circ);
    add_ops_to_prepend_1(circ);

    PauliExpBox peb({{Pauli::Z, Pauli::Y, Pauli::X}, 0.333});
    circ.add_box(peb, {0, 1, 2});
    PauliExpBox peb2({{Pauli::Y, Pauli::Z, Pauli::X}, 0.174});
    circ.add_box(peb2, {0, 1, 2});
    PauliExpBox peb3({{Pauli::Y, Pauli::Z, Pauli::I}, 0.567});
    circ.add_box(peb3, {0, 1, 2});
    PauliExpBox peb4({{Pauli::Z, Pauli::Y, Pauli::I}, 1.849});
    circ.add_box(peb4, {0, 1, 2});
    PauliExpBox peb5({{Pauli::X, Pauli::X, Pauli::X}, 1.67});
    circ.add_box(peb5, {0, 1, 2});
    PauliExpBox peb6({{Pauli::X, Pauli::X, Pauli::I}, 0.83});
    circ.add_box(peb6, {0, 1, 2});
    Circuit test1 = circ;

    WHEN("Different strategies and configs") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Sets, CXConfigType::Star)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Different strategies and configs") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Individual, CXConfigType::Star)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Different strategies and configs") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Pairwise, CXConfigType::Star)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Different strategies and configs") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Sets, CXConfigType::Snake)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Different strategies and configs") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Individual, CXConfigType::Snake)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Different strategies and configs") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Pairwise, CXConfigType::Snake)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Different strategies and configs") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Sets, CXConfigType::Tree)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Different strategies and configs") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Individual, CXConfigType::Tree)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Different strategies and configs") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Pairwise, CXConfigType::Tree)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Pairwise strategy with CXConfigType::MultiQGate") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Pairwise, CXConfigType::MultiQGate)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Sets strategy with CXConfigType::MultiQGate") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Sets, CXConfigType::MultiQGate)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
    WHEN("Individual strategy with CXConfigType::MultiQGate") {
      Transforms::synthesise_pauli_graph(
          Transforms::PauliSynthStrat::Individual, CXConfigType::MultiQGate)
          .apply(circ);
      Transforms::decomp_boxes().apply(circ);
      REQUIRE(circ.count_gates(OpType::XXPhase3) == 6);
      REQUIRE(test_statevector_comparison(test1, circ));
    }
  }
  GIVEN(
      "4 qb 8 Pauli Gadget circuit (ie. double excitations for UCCSD "
      "circuits)") {
    // add some arbitrary rotations to get away from |00> state
    auto prepend = CircuitsForTesting::get_prepend_circuit(4);
    add_ops_to_prepend_1(prepend);
    add_ops_to_prepend_2(prepend);

    Circuit circ(4);
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);
    Sym b = SymTable::fresh_symbol("b");
    Expr eb(b);
    Sym c = SymTable::fresh_symbol("c");
    Expr ec(c);
    Sym d = SymTable::fresh_symbol("d");
    Expr ed(d);
    Sym e = SymTable::fresh_symbol("e");
    Expr ee(e);
    Sym f = SymTable::fresh_symbol("f");
    Expr ef(f);
    Sym g = SymTable::fresh_symbol("g");
    Expr eg(g);
    Sym h = SymTable::fresh_symbol("h");
    Expr eh(h);
    std::map<Sym, double, SymEngine::RCPBasicKeyLess> symbol_map = {
        {a, 0.3112}, {b, 1.178}, {c, -0.911}, {d, 0.7122},
        {e, 1.102},  {f, 0.151}, {g, 1.223},  {h, 1.666}};

    PauliExpBox peb0({{Pauli::X, Pauli::X, Pauli::X, Pauli::Y}, ea});
    circ.add_box(peb0, {0, 1, 2, 3});
    PauliExpBox peb1({{Pauli::X, Pauli::X, Pauli::Y, Pauli::X}, eb});
    circ.add_box(peb1, {0, 1, 2, 3});
    PauliExpBox peb2({{Pauli::X, Pauli::Y, Pauli::X, Pauli::X}, ec});
    circ.add_box(peb2, {0, 1, 2, 3});
    PauliExpBox peb3({{Pauli::X, Pauli::Y, Pauli::Y, Pauli::Y}, ed});
    circ.add_box(peb3, {0, 1, 2, 3});
    PauliExpBox peb4({{Pauli::Y, Pauli::X, Pauli::X, Pauli::X}, ee});
    circ.add_box(peb4, {0, 1, 2, 3});
    PauliExpBox peb5({{Pauli::Y, Pauli::X, Pauli::Y, Pauli::Y}, ef});
    circ.add_box(peb5, {0, 1, 2, 3});
    PauliExpBox peb6({{Pauli::Y, Pauli::Y, Pauli::X, Pauli::Y}, eg});
    circ.add_box(peb6, {0, 1, 2, 3});
    PauliExpBox peb7({{Pauli::Y, Pauli::Y, Pauli::Y, Pauli::X}, eh});
    circ.add_box(peb7, {0, 1, 2, 3});

    Circuit test1 = prepend >> circ;
    CircBox circbox(circ);
    Circuit major_circ(4);
    major_circ.add_box(circbox, {0, 1, 2, 3});
    Transforms::special_UCC_synthesis().apply(major_circ);
    Circuit test2 = prepend >> major_circ;
    test1.symbol_substitution(symbol_map);
    test2.symbol_substitution(symbol_map);
    REQUIRE(test_statevector_comparison(test1, test2));
  }
  GIVEN(
      "4 qb 4 Pauli Gadget circuit (some single excitations for UCCSD "
      "circuits)") {
    auto prepend = CircuitsForTesting::get_prepend_circuit(4);
    add_ops_to_prepend_1(prepend);
    add_ops_to_prepend_2(prepend);

    Circuit circ(4);
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);
    Sym b = SymTable::fresh_symbol("b");
    Expr eb(b);
    Sym c = SymTable::fresh_symbol("c");
    Expr ec(c);
    Sym d = SymTable::fresh_symbol("d");
    Expr ed(d);
    std::map<Sym, double, SymEngine::RCPBasicKeyLess> symbol_map = {
        {a, 0.3112}, {b, 1.178}, {c, -0.911}, {d, 0.7122}};

    PauliExpBox peb0({{Pauli::Y, Pauli::Z, Pauli::X, Pauli::I}, ea});
    circ.add_box(peb0, {0, 1, 2, 3});
    PauliExpBox peb1({{Pauli::X, Pauli::Z, Pauli::Y, Pauli::I}, eb});
    circ.add_box(peb1, {0, 1, 2, 3});
    PauliExpBox peb2({{Pauli::I, Pauli::Y, Pauli::Z, Pauli::X}, ec});
    circ.add_box(peb2, {0, 1, 2, 3});
    PauliExpBox peb3({{Pauli::I, Pauli::X, Pauli::Y, Pauli::Z}, ed});
    circ.add_box(peb3, {0, 1, 2, 3});

    Circuit test1 = prepend >> circ;
    CircBox circbox(circ);
    Circuit major_circ(4);
    major_circ.add_box(circbox, {0, 1, 2, 3});
    Transforms::special_UCC_synthesis().apply(major_circ);
    Circuit test2 = prepend >> major_circ;
    test1.symbol_substitution(symbol_map);
    test2.symbol_substitution(symbol_map);
    REQUIRE(test_statevector_comparison(test1, test2));
  }
  GIVEN(
      "5 qubit 7 Pauli Gadget circuit; example from compilation strategy "
      "paper") {
    // This circuit requires greedy diagonalisation
    auto prepend = CircuitsForTesting::get_prepend_circuit(5);
    add_ops_to_prepend_1(prepend);
    add_ops_to_prepend_2(prepend);
    prepend.add_op<unsigned>(OpType::Rx, 0.466, {4});
    prepend.add_op<unsigned>(OpType::Rz, 1.303, {4});

    Circuit circ(5);
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);
    Sym b = SymTable::fresh_symbol("b");
    Expr eb(b);
    Sym c = SymTable::fresh_symbol("c");
    Expr ec(c);
    Sym d = SymTable::fresh_symbol("d");
    Expr ed(d);
    Sym e = SymTable::fresh_symbol("e");
    Expr ee(e);
    Sym f = SymTable::fresh_symbol("f");
    Expr ef(f);
    Sym g = SymTable::fresh_symbol("g");
    Expr eg(g);
    std::map<Sym, double, SymEngine::RCPBasicKeyLess> symbol_map = {
        {a, 0.3112}, {b, 1.178}, {c, -0.911}, {d, 0.7122},
        {e, 1.102},  {f, 0.151}, {g, 1.223}};

    PauliExpBox peb0({{Pauli::I, Pauli::X, Pauli::Z, Pauli::I, Pauli::Z}, ea});
    circ.add_box(peb0, {0, 1, 2, 3, 4});
    PauliExpBox peb1({{Pauli::I, Pauli::Y, Pauli::I, Pauli::Z, Pauli::Y}, eb});
    circ.add_box(peb1, {0, 1, 2, 3, 4});
    PauliExpBox peb2({{Pauli::X, Pauli::X, Pauli::I, Pauli::Y, Pauli::I}, ec});
    circ.add_box(peb2, {0, 1, 2, 3, 4});
    PauliExpBox peb3({{Pauli::Y, Pauli::Y, Pauli::X, Pauli::I, Pauli::I}, ed});
    circ.add_box(peb3, {0, 1, 2, 3, 4});
    PauliExpBox peb4({{Pauli::Z, Pauli::I, Pauli::Y, Pauli::X, Pauli::X}, ee});
    circ.add_box(peb4, {0, 1, 2, 3, 4});
    PauliExpBox peb5({{Pauli::Z, Pauli::X, Pauli::I, Pauli::Z, Pauli::Z}, ef});
    circ.add_box(peb5, {0, 1, 2, 3, 4});
    PauliExpBox peb6({{Pauli::Z, Pauli::Y, Pauli::Z, Pauli::I, Pauli::Y}, eg});
    circ.add_box(peb6, {0, 1, 2, 3, 4});

    Circuit test1 = prepend >> circ;
    CircBox circbox(circ);
    Circuit major_circ(5);
    major_circ.add_box(circbox, {0, 1, 2, 3, 4});
    Transforms::special_UCC_synthesis().apply(major_circ);
    Circuit test2 = prepend >> major_circ;
    REQUIRE(test2.count_gates(OpType::CX) == 24);
    test1.symbol_substitution(symbol_map);
    test2.symbol_substitution(symbol_map);
    REQUIRE(test_statevector_comparison(test1, test2));
  }
  GIVEN("A circuit with a classical bit") {
    // https://github.com/quantinuum/tket/issues/1578
    Circuit c0(0, 1);
    CircBox cbox(c0);
    Circuit c(0, 1);
    c.add_box<CircBox, Bit>(cbox, {Bit("c", 0)});
    REQUIRE_NOTHROW(Transforms::special_UCC_synthesis().apply(c));
  }
  GIVEN(
      "Clifford merges requires removing from start line without segfault "
      "(Grover circuit)") {
    Circuit oracle(5);
    oracle.add_op<unsigned>(OpType::CCX, {0, 1, 4});
    oracle.add_op<unsigned>(OpType::H, {4});
    oracle.add_op<unsigned>(OpType::CCX, {2, 3, 4});
    oracle.add_op<unsigned>(OpType::H, {4});
    oracle.add_op<unsigned>(OpType::CCX, {0, 1, 4});

    Circuit reflect(2);
    add_1qb_gates(reflect, OpType::H, {0, 1});
    add_1qb_gates(reflect, OpType::X, {0, 1});
    reflect.add_op<unsigned>(OpType::CZ, {0, 1});
    add_1qb_gates(reflect, OpType::X, {0, 1});
    add_1qb_gates(reflect, OpType::H, {0, 1});

    Circuit circ(5, 4);
    add_1qb_gates(circ, OpType::H, {0, 1, 2, 3});

    circ.append(oracle);
    circ.append_qubits(reflect, {2, 3});
    circ.append(oracle);
    circ.append_qubits(reflect, {0, 1});
    circ.append(oracle);
    circ.append_qubits(reflect, {2, 3});

    add_2qb_gates(circ, OpType::Measure, {{0, 0}, {1, 1}, {2, 2}, {3, 3}});

    Transforms::rebase_pyzx().apply(circ);
    bool success = Transforms::synthesise_pauli_graph().apply(circ);
    REQUIRE(success);
  }
}

SCENARIO("Conjugating Cliffords through Pauli tensors") {
  GIVEN("A 3qb XYZ pauli tensor") {
    SpPauliStabiliser qpt(DensePauliMap{Pauli::X, Pauli::Y, Pauli::Z});
    WHEN("Commuting a Hadamard through qb0") {
      Qubit qb0(0);
      conjugate_PauliTensor(qpt, OpType::H, qb0);
      THEN("X becomes Z") {
        REQUIRE(qpt.get(qb0) == Pauli::Z);
        REQUIRE(qpt.is_real_negative() == false);
      }
    }
    WHEN("Commuting a X through qb0") {
      Qubit qb0(0);
      conjugate_PauliTensor(qpt, OpType::X, qb0);
      THEN("X remains X") {
        REQUIRE(qpt.get(qb0) == Pauli::X);
        REQUIRE(qpt.is_real_negative() == false);
      }
    }
    WHEN("Commuting a X through qb1") {
      Qubit qb1(1);
      conjugate_PauliTensor(qpt, OpType::X, qb1);
      THEN("Y becomes -Y") {
        REQUIRE(qpt.get(qb1) == Pauli::Y);
        REQUIRE(qpt.is_real_negative() == true);
      }
    }
    WHEN("Commuting a CX through qb0-qb1") {
      Qubit qb0(0), qb1(1);
      conjugate_PauliTensor(qpt, OpType::CX, qb0, qb1);
      THEN("XY becomes YZ") {
        REQUIRE(qpt.get(qb0) == Pauli::Y);
        REQUIRE(qpt.get(qb1) == Pauli::Z);
        REQUIRE(qpt.is_real_negative() == false);
      }
    }
    WHEN("Commuting an XXPhase3 through qb0-qb1-qb2") {
      Qubit qb0(0), qb1(1), qb2(2);
      conjugate_PauliTensor(qpt, OpType::XXPhase3, qb0, qb1, qb2);
      THEN("XYZ becomes -XZY") {
        REQUIRE(qpt.get(qb0) == Pauli::X);
        REQUIRE(qpt.get(qb1) == Pauli::Z);
        REQUIRE(qpt.get(qb2) == Pauli::Y);
        REQUIRE(qpt.is_real_negative() == true);
      }
    }
  }
  GIVEN("A 3qb XXX pauli tensor") {
    SpPauliStabiliser qpt(DensePauliMap{Pauli::X, Pauli::X, Pauli::X});
    WHEN("Commuting an XXPhase3 through qb0-qb1-qb2") {
      Qubit qb0(0), qb1(1), qb2(2);
      THEN("XXX remains XXX") {
        REQUIRE(qpt.get(qb0) == Pauli::X);
        REQUIRE(qpt.get(qb1) == Pauli::X);
        REQUIRE(qpt.get(qb2) == Pauli::X);
        REQUIRE(qpt.is_real_negative() == false);
      }
    }
  }
}

SCENARIO("Test greedy diagonalisation explicitly") {
  auto is_diagonal = [](const SpSymPauliTensor& qpt) {
    for (auto [qb, p] : qpt.string) {
      if (p != Pauli::I && p != Pauli::Z) {
        return false;
      }
    }
    return true;
  };

  auto apply_strategy = [](std::list<SpSymPauliTensor>& gadgets,
                           std::set<Qubit>& qubits, Circuit& cliff_circ,
                           const CXConfigType config) {
    while (!qubits.empty()) {
      Conjugations conjugations;
      greedy_diagonalise(gadgets, qubits, conjugations, cliff_circ, config);
      for (auto& g : gadgets) {
        apply_conjugations(g, conjugations);
      }
      check_easy_diagonalise(gadgets, qubits, cliff_circ);
    }
  };
  GIVEN("A large-ish set of PauliTensor") {
    unsigned n_qbs = 6;
    std::set<Qubit> qbs;
    for (unsigned i = 0; i < n_qbs; ++i) qbs.insert(Qubit(i));

    std::list<SpSymPauliTensor> gadgets;
    Conjugations conjugations;
    Circuit cliff_circ(n_qbs);

    // commuting set
    gadgets.push_back(SpSymPauliTensor(
        {Pauli::Z, Pauli::Z, Pauli::Z, Pauli::X, Pauli::X, Pauli::X}, 1.13));
    gadgets.push_back(SpSymPauliTensor(
        {Pauli::Z, Pauli::X, Pauli::Y, Pauli::Z, Pauli::Z, Pauli::X}, 0.226));
    gadgets.push_back(SpSymPauliTensor(
        {Pauli::Z, Pauli::Y, Pauli::X, Pauli::Z, Pauli::Z, Pauli::X}, 0.013));
    gadgets.push_back(SpSymPauliTensor(
        {Pauli::Z, Pauli::Y, Pauli::X, Pauli::Y, Pauli::Y, Pauli::X}, 0.952));
    gadgets.push_back(SpSymPauliTensor(
        {Pauli::X, Pauli::Z, Pauli::Z, Pauli::Y, Pauli::Y, Pauli::Y}, 1.88));

    WHEN("a single run with Snake configuration") {
      CXConfigType cx_config = CXConfigType::Snake;
      greedy_diagonalise(gadgets, qbs, conjugations, cliff_circ, cx_config);
      THEN("5x CX are used") {
        REQUIRE(cliff_circ.depth_by_type(OpType::CX) == 5);
        REQUIRE(cliff_circ.count_gates(OpType::CX) == 5);
      }
    }
    WHEN("repeated runs with Snake configuration") {
      CXConfigType cx_config = CXConfigType::Snake;
      apply_strategy(gadgets, qbs, cliff_circ, cx_config);
      THEN("gadgets are diagonal") {
        for (const auto& g : gadgets) {
          REQUIRE(is_diagonal(g));
        }
      }
    }
    WHEN("a single run with Star configuration") {
      CXConfigType cx_config = CXConfigType::Star;
      greedy_diagonalise(gadgets, qbs, conjugations, cliff_circ, cx_config);
      THEN("5x CX are used") {
        REQUIRE(cliff_circ.depth_by_type(OpType::CX) == 5);
        REQUIRE(cliff_circ.count_gates(OpType::CX) == 5);
      }
    }
    WHEN("repeated runs with Star configuration") {
      CXConfigType cx_config = CXConfigType::Star;
      apply_strategy(gadgets, qbs, cliff_circ, cx_config);
      THEN("gadgets are diagonal") {
        for (const auto& g : gadgets) {
          REQUIRE(is_diagonal(g));
        }
      }
    }
    WHEN("a single run with Tree configuration") {
      CXConfigType cx_config = CXConfigType::Tree;
      greedy_diagonalise(gadgets, qbs, conjugations, cliff_circ, cx_config);
      THEN("5x CX are used, depth 3") {
        REQUIRE(cliff_circ.depth_by_type(OpType::CX) == 3);
        REQUIRE(cliff_circ.count_gates(OpType::CX) == 5);
      }
    }
    WHEN("repeated runs with Tree configuration") {
      CXConfigType cx_config = CXConfigType::Tree;
      apply_strategy(gadgets, qbs, cliff_circ, cx_config);
      THEN("gadgets are diagonal") {
        for (const auto& g : gadgets) {
          REQUIRE(is_diagonal(g));
        }
      }
    }
    WHEN("a single run with MultiQGate configuration") {
      CXConfigType cx_config = CXConfigType::MultiQGate;
      greedy_diagonalise(gadgets, qbs, conjugations, cliff_circ, cx_config);
      THEN("2x XXPhase3 are used") {
        REQUIRE(cliff_circ.depth_by_type(OpType::XXPhase3) == 2);
        REQUIRE(cliff_circ.depth_by_type(OpType::CX) == 1);
      }
    }
    WHEN("repeated runs with MultiQGate configuration") {
      CXConfigType cx_config = CXConfigType::MultiQGate;
      apply_strategy(gadgets, qbs, cliff_circ, cx_config);
      THEN("gadgets are diagonal") {
        for (const auto& g : gadgets) {
          REQUIRE(is_diagonal(g));
        }
      }
    }
  }
}

SCENARIO("Diagonalise a pair of gadgets") {
  unsigned n_qbs = 6;
  std::set<Qubit> qbs;
  for (unsigned i = 0; i < n_qbs; ++i) qbs.insert(Qubit(i));

  Conjugations conjugations;
  Circuit circ(n_qbs);

  // commuting set
  std::vector<SpSymPauliTensor> gadgets;
  gadgets.push_back(SpSymPauliTensor(
      {Pauli::Z, Pauli::Z, Pauli::X, Pauli::I, Pauli::I, Pauli::X}, 1.13));
  gadgets.push_back(SpSymPauliTensor(
      {Pauli::Z, Pauli::Z, Pauli::X, Pauli::Z, Pauli::Z, Pauli::I}, 0.226));

  Circuit correct;
  for (unsigned i = 0; i < 2; ++i) {
    correct.append(pauli_gadget(gadgets.at(i)));
  }
  auto u_correct = tket_sim::get_unitary(correct);

  GIVEN("Snake configuration") {
    CXConfigType config = CXConfigType::Snake;
    circ.append(pauli_gadget_pair(gadgets.at(0), gadgets.at(1), config));
    THEN("Unitary is correct") {
      auto u_res = tket_sim::get_unitary(circ);
      REQUIRE((u_correct - u_res).cwiseAbs().sum() < ERR_EPS);
    }
  }
  GIVEN("Star configuration") {
    CXConfigType config = CXConfigType::Star;
    circ.append(pauli_gadget_pair(gadgets.at(0), gadgets.at(1), config));
    THEN("Unitary is correct") {
      auto u_res = tket_sim::get_unitary(circ);
      REQUIRE((u_correct - u_res).cwiseAbs().sum() < ERR_EPS);
    }
  }
  GIVEN("Tree configuration") {
    CXConfigType config = CXConfigType::Tree;
    circ.append(pauli_gadget_pair(gadgets.at(0), gadgets.at(1), config));
    THEN("Unitary is correct") {
      auto u_res = tket_sim::get_unitary(circ);
      REQUIRE((u_correct - u_res).cwiseAbs().sum() < ERR_EPS);
    }
  }
  GIVEN("MultiQGate configuration") {
    CXConfigType config = CXConfigType::MultiQGate;
    circ.append(pauli_gadget_pair(gadgets.at(0), gadgets.at(1), config));
    circ.decompose_boxes_recursively();
    THEN("XXPhase3 were used") {
      REQUIRE(circ.count_gates(OpType::XXPhase3) == 2);
    }
    THEN("Unitary is correct") {
      auto u_res = tket_sim::get_unitary(circ);
      REQUIRE((u_correct - u_res).cwiseAbs().sum() < ERR_EPS);
    }
  }
}

SCENARIO("Measure handling in PauliGraph") {
  GIVEN("A circuit with end-of-circuit measurements") {
    Circuit circ(2, 2);
    circ.add_op<unsigned>(OpType::S, {0});
    circ.add_op<unsigned>(OpType::V, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.2, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {1});
    circ.add_op<unsigned>(OpType::Measure, {0, 1});
    circ.add_op<unsigned>(OpType::Measure, {1, 0});
    PauliGraph pg = circuit_to_pauli_graph(circ);
    pg.sanity_check();
    std::map<Qubit, unsigned> correct_readout = {{Qubit(0), 1}, {Qubit(1), 0}};
    WHEN("Synthesise individually") {
      Circuit circ2 = pauli_graph_to_pauli_exp_box_circuit_individually(pg);
      Transforms::decomp_boxes().apply(circ2);
      REQUIRE(circ2.qubit_readout() == correct_readout);
    }
    WHEN("Synthesise pairwise") {
      Circuit circ2 = pauli_graph_to_pauli_exp_box_circuit_pairwise(pg);
      Transforms::decomp_boxes().apply(circ2);
      pg.sanity_check();
      REQUIRE(circ2.qubit_readout() == correct_readout);
    }
    WHEN("Synthesise in sets") {
      Circuit circ2 = pauli_graph_to_pauli_exp_box_circuit_sets(pg);
      Transforms::decomp_boxes().apply(circ2);
      REQUIRE(circ2.qubit_readout() == correct_readout);
    }
  }
  GIVEN("A circuit with mid-circuit measurements") {
    Circuit circ(2, 2);
    circ.add_op<unsigned>(OpType::S, {0});
    circ.add_op<unsigned>(OpType::V, {1});
    circ.add_op<unsigned>(OpType::Measure, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.2, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {1});
    circ.add_op<unsigned>(OpType::Measure, {1, 0});
    REQUIRE_THROWS_AS(
        circuit_to_pauli_graph(circ), MidCircuitMeasurementNotAllowed);
  }
}

}  // namespace test_PauliGraph
}  // namespace tket
