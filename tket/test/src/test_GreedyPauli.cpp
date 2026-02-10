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

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>

#include "testutil.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Gate/SymTable.hpp"
#include "tket/Ops/ClassicalOps.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Predicates/CompilerPass.hpp"
#include "tket/Predicates/PassGenerators.hpp"
#include "tket/Predicates/PassLibrary.hpp"
#include "tket/Transformations/BasicOptimisation.hpp"
#include "tket/Transformations/GreedyPauliOptimisation.hpp"
#include "tket/Utils/Expression.hpp"

namespace tket {
namespace test_GreedyPauliSimp {

SCENARIO("Exception handling") {
  GIVEN("Invalid arguments") {
    Circuit circ(1);
    REQUIRE_THROWS_MATCHES(
        Transforms::greedy_pauli_optimisation(0.3, 0.3, 0, 10).apply(circ),
        Transforms::GreedyPauliSimp::GreedyPauliSimpError,
        MessageContains("max_lookahead must be greater than 0."));
    REQUIRE_THROWS_MATCHES(
        Transforms::greedy_pauli_optimisation(0.3, 0.3, 10, 0).apply(circ),
        Transforms::GreedyPauliSimp::GreedyPauliSimpError,
        MessageContains("max_tqe_candidates must be greater than 0."));
  }
}

SCENARIO("Clifford synthesis") {
  GIVEN("Empty circuit") {
    Circuit circ(3);
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("1Q Simple Clifford") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Sdg, {0});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("2Q Simple Clifford") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Y, {0});
    circ.add_op<unsigned>(OpType::Vdg, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("3Q Simple Clifford") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::Y, {0});
    circ.add_op<unsigned>(OpType::Sdg, {2});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::CZ, {0, 2});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("5Q Simple Clifford") {
    Circuit circ(5);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::S, {1});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::CZ, {1, 2});
    circ.add_op<unsigned>(OpType::V, {1});
    circ.add_op<unsigned>(OpType::X, {3});
    circ.add_op<unsigned>(OpType::CZ, {0, 4});
    circ.add_op<unsigned>(OpType::CY, {0, 1});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::Z, {2});
    circ.add_op<unsigned>(OpType::Y, {4});
    circ.add_op<unsigned>(OpType::CY, {3, 4});
    circ.add_op<unsigned>(OpType::CX, {2, 0});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("Clifford with swaps") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 3});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::CZ, {1, 3});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {2});
    circ.add_op<unsigned>(OpType::SWAP, {3, 1});
    circ.add_op<unsigned>(OpType::CY, {0, 2});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("Test search limits") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::CZ, {1, 3});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::CX, {3, 2});
    circ.add_op<unsigned>(OpType::Z, {2});
    circ.add_op<unsigned>(OpType::SWAP, {3, 1});
    circ.add_op<unsigned>(OpType::CY, {0, 2});
    Circuit d1(circ);
    Circuit d2(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation(0.7, 0.3, 2, 1).apply(d1));
    REQUIRE(Transforms::greedy_pauli_optimisation(0.7, 0.3, 20, 20).apply(d2));
    REQUIRE(test_unitary_comparison(circ, d1, true));
    REQUIRE(test_unitary_comparison(circ, d2, true));
    REQUIRE(d1 != d2);
  }
}
SCENARIO("Complete synthesis") {
  GIVEN("Qubit permutation circuit") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.replace_SWAPs();
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("1Q Simple Circuit") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Sdg, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.3, {0});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("Symbolic Circuit") {
    Circuit circ(2);
    auto a = SymTable::fresh_symbol("a");
    auto b = SymTable::fresh_symbol("b");
    auto ea = Expr(a);
    auto eb = Expr(b);
    circ.add_op<unsigned>(OpType::Sdg, {0});
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::Ry, eb, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rx, ea, {0});
    Circuit d(circ);
    symbol_map_t symbol_map;
    symbol_map[a] = Expr(0.5);
    symbol_map[b] = Expr(0.7);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    circ.symbol_substitution(symbol_map);
    d.symbol_substitution(symbol_map);
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("4Q PauliExp Circuit") {
    Circuit circ(4);
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::X}, 0.3)), {0, 1});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::Y}, -0.1)), {2, 3});
    circ.add_box(
        PauliExpPairBox(
            SymPauliTensor({Pauli::X, Pauli::Z}, 1.0),
            SymPauliTensor({Pauli::Z, Pauli::X}, 0.4)),
        {0, 2});
    circ.add_box(
        PauliExpCommutingSetBox({
            {{Pauli::I, Pauli::Y, Pauli::I}, -0.1},
            {{Pauli::X, Pauli::Y, Pauli::Z}, -1.2},
            {{Pauli::X, Pauli::Y, Pauli::Z}, 0.5},
        }),
        {1, 2, 3});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 3});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::CZ, {1, 3});
    Circuit d = Transforms::GreedyPauliSimp::greedy_pauli_graph_synthesis(circ);
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("Arbitrary Circuit") {
    Circuit circ(5);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.add_op<unsigned>(OpType::Rz, 0.1, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 3});
    circ.add_op<unsigned>(OpType::Ry, 0.2, {3});
    circ.add_op<unsigned>(OpType::Ry, 0.15, {2});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {4});
    circ.add_op<unsigned>(OpType::CZ, {1, 4});
    circ.add_op<unsigned>(OpType::ZZMax, {1, 2});
    circ.add_op<unsigned>(OpType::T, {4});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::ZZPhase, 0.7, {3, 2});
    circ.add_op<unsigned>(OpType::T, {3});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {2});
    circ.add_op<unsigned>(OpType::SWAP, {3, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::CY, {0, 2});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("5Q PauliExp Circuit with search limits") {
    Circuit circ(5);
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::X, Pauli::X}, 0.3)),
        {0, 1, 4});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::Z, Pauli::Y}, -0.1)),
        {2, 3, 0});
    circ.add_box(
        PauliExpPairBox(
            SymPauliTensor({Pauli::X, Pauli::Z, Pauli::Y}, 1.0),
            SymPauliTensor({Pauli::Z, Pauli::X, Pauli::Y}, 0.4)),
        {0, 2, 4});
    circ.add_box(
        PauliExpCommutingSetBox({
            {{
                 Pauli::I,
                 Pauli::Y,
                 Pauli::I,
             },
             -0.1},
            {{Pauli::X, Pauli::Y, Pauli::Z}, -1.2},
            {{Pauli::X, Pauli::Y, Pauli::Z}, 0.5},
        }),
        {1, 2, 3});
    circ.add_box(
        PauliExpCommutingSetBox({
            {{
                 Pauli::I,
                 Pauli::X,
                 Pauli::I,
             },
             -0.15},
            {{Pauli::X, Pauli::X, Pauli::Z}, -1.25},
            {{Pauli::X, Pauli::X, Pauli::Z}, 0.2},
        }),
        {0, 3, 4});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 3});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::CZ, {1, 3});
    Circuit d1(circ);
    Circuit d2(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation(0.7, 0.3, 3, 3).apply(d1));
    REQUIRE(Transforms::greedy_pauli_optimisation(0.7, 0.3, 30, 30).apply(d2));
    REQUIRE(test_unitary_comparison(circ, d1, true));
    REQUIRE(test_unitary_comparison(circ, d2, true));
    REQUIRE(d1 != d2);
  }
  GIVEN("Circuit with trivial Pauli exps") {
    Circuit circ(4);
    circ.add_box(PauliExpBox(SymPauliTensor({Pauli::X, Pauli::X}, 2)), {0, 1});
    circ.add_box(
        PauliExpPairBox(
            SymPauliTensor({Pauli::I, Pauli::I}, 1.2),
            SymPauliTensor({Pauli::Z, Pauli::X}, -2)),
        {0, 2});
    circ.add_box(
        PauliExpCommutingSetBox({
            {{Pauli::I, Pauli::Y, Pauli::I}, 0},
            {{Pauli::X, Pauli::Y, Pauli::Z}, 0},
            {{Pauli::I, Pauli::I, Pauli::I}, 0.5},
        }),
        {1, 2, 3});
    Circuit d = Transforms::GreedyPauliSimp::greedy_pauli_graph_synthesis(circ);
    REQUIRE(test_unitary_comparison(circ, d, true));
    REQUIRE(d.n_gates() == 0);
  }
  GIVEN("Circuit with non-default UnitIDs") {
    Circuit circ;
    register_t reg_a = circ.add_q_register("a", 2);
    register_t reg_b = circ.add_q_register("b", 2);
    circ.add_op<UnitID>(OpType::CX, {reg_a[0], reg_b[1]});
    circ.add_op<UnitID>(OpType::SWAP, {reg_b[0], reg_a[1]});
    circ.add_op<UnitID>(OpType::Rz, 0.3, {reg_a[1]});
    circ.add_op<UnitID>(OpType::CX, {reg_a[1], reg_b[1]});
    circ.add_op<UnitID>(OpType::Ry, 0.2, {reg_b[1]});
    circ.add_op<UnitID>(OpType::H, {reg_b[1]});
    circ.add_op<UnitID>(OpType::Rz, 0.3, {reg_a[0]});
    circ.add_op<UnitID>(OpType::CY, {reg_a[0], reg_a[1]});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("Circuit with measurements") {
    Circuit circ(4, 4);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.add_op<unsigned>(OpType::Rz, 0.1, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 3});
    circ.add_op<unsigned>(OpType::Ry, 0.2, {3});
    circ.add_op<unsigned>(OpType::Ry, 0.15, {2});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::ZZMax, {1, 2});
    // For circuit d, we add measurement after synthesis
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(d));
    REQUIRE(d.has_implicit_wireswaps());
    for (unsigned i = 0; i < 4; i++) {
      d.add_op<UnitID>(OpType::Measure, {Qubit(i), Bit(i)});
    }
    // For circuit g, we add measurement before synthesis
    Circuit g(circ);
    for (unsigned i = 0; i < 4; i++) {
      g.add_op<UnitID>(OpType::Measure, {Qubit(i), Bit(i)});
    }
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(g));
    REQUIRE(d == g);
  }
  GIVEN("Circuit with conditional gates") {
    Circuit circ(2, 2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.5}, {0}, {0}, 0);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Circuit d(2, 2);
    Op_ptr cond = std::make_shared<Conditional>(
        std::make_shared<PauliExpBox>(
            SymPauliTensor({Pauli::Z, Pauli::I}, 0.5)),
        1, 0);
    d.add_conditional_gate<unsigned>(OpType::Sdg, {}, {0}, {0}, 0);
    d.add_conditional_gate<unsigned>(OpType::Z, {}, {0}, {0}, 0);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(circ == d);
  }
  GIVEN("Circuit with conditional gates 2") {
    Circuit circ(2, 1);
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::Z}, 0.12)), {0, 1});
    Op_ptr cond = std::make_shared<Conditional>(
        std::make_shared<PauliExpBox>(
            SymPauliTensor({Pauli::Z, Pauli::Z}, 0.5)),
        1, 0);
    circ.add_op<unsigned>(cond, {0, 0, 1});
    // two boxes anti-commute hence simultaneous diagonalisation
    Circuit d(2, 1);
    d.add_op<unsigned>(OpType::CY, {1, 0});
    d.add_op<unsigned>(OpType::Rx, 0.12, {0});
    d.add_conditional_gate<unsigned>(OpType::Sdg, {}, {0}, {0}, 0);
    d.add_conditional_gate<unsigned>(OpType::Z, {}, {0}, {0}, 0);
    d.add_op<unsigned>(OpType::CY, {1, 0});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(circ == d);
  }

  GIVEN("Circuit with conditional gates 3") {
    Circuit c(2, 1);
    c.add_op<unsigned>(OpType::Rx, 1.0246, {0});
    c.add_conditional_gate<unsigned>(OpType::Ry, {-0.25}, {1}, {0}, 0);
    c.add_conditional_gate<unsigned>(OpType::CZ, {}, {0, 1}, {0}, 0);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c));

    Circuit comp(2, 1);
    comp.add_op<unsigned>(OpType::Rx, 1.0246, {0});
    comp.add_conditional_gate<unsigned>(OpType::Ry, {-0.25}, {1}, {0}, 0);
    comp.add_conditional_gate<unsigned>(OpType::CZ, {}, {1, 0}, {0}, 0);
    // Previously Conditional Ry was being commuted ahead of Rz and then CZ
    // merged in without checking commuting with Rx Returning the same circuit
    // is expected, except the CZ gate is "flipped"
    REQUIRE(comp == c);
  }
  GIVEN("Circuit with conditional gates and measures") {
    Circuit circ(2, 2);
    Op_ptr cond1 = std::make_shared<Conditional>(
        std::make_shared<PauliExpBox>(
            SymPauliTensor({Pauli::Z, Pauli::X}, 0.5)),
        1, 0);
    Op_ptr cond2 = std::make_shared<Conditional>(
        std::make_shared<PauliExpBox>(
            SymPauliTensor({Pauli::Z, Pauli::Y}, 0.12)),
        1, 0);
    circ.add_op<unsigned>(cond1, {0, 0, 1});
    circ.add_op<unsigned>(OpType::Measure, {0, 0});
    // can commute to the front
    circ.add_op<unsigned>(cond2, {1, 0, 1});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(circ.count_n_qubit_gates(2) == 3);
  }

  GIVEN("Conditionals merging") {
    Circuit circ(2, 2);
    Op_ptr cond1 = std::make_shared<Conditional>(
        std::make_shared<PauliExpBox>(
            SymPauliTensor({Pauli::Z, Pauli::X}, 0.25)),
        1, 0);
    Op_ptr cond2 = std::make_shared<Conditional>(
        std::make_shared<PauliExpBox>(
            SymPauliTensor({Pauli::Z, Pauli::X}, -0.25)),
        1, 0);
    circ.add_op<unsigned>(cond1, {0, 0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(cond2, {0, 0, 1});
    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 1}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 1}, {0}, 0);
    circ.add_op<unsigned>(OpType::Rz, -0.3, {0});
    // should all be canceled
    Circuit d(2, 2);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    Transforms::remove_redundancies().apply(circ);
    REQUIRE(circ == d);
  }
  GIVEN("Circuit with classical gates") {
    Circuit circ(1, 4);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(ClassicalX(), {1});
    circ.add_op<unsigned>(ClassicalCX(), {0, 1});
    circ.add_op<unsigned>(AndWithOp(), {2, 3});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(circ == d);
  }
  GIVEN("Circuit with WASMs") {
    std::string wasm_file = "string/with/path/to/wasm/file";
    std::string wasm_func = "stringNameOfWASMFunc";
    std::vector<unsigned> uv = {2, 1};
    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(6, 1, uv, uv, wasm_func, wasm_file);
    Circuit circ(1, 7);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<UnitID>(
        wop_ptr,
        {Bit(0), Bit(1), Bit(2), Bit(3), Bit(4), Bit(5), WasmState(0)});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(circ == d);
  }
  GIVEN("Circuit with mid-circuit measurements") {
    Circuit circ(2, 2);
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::Measure, {0, 0});
    circ.add_op<unsigned>(OpType::Tdg, {0});
    circ.add_op<unsigned>(OpType::Measure, {1, 1});
    Circuit d(2, 2);
    d.add_op<unsigned>(OpType::Measure, {0, 0});
    d.add_op<unsigned>(OpType::Measure, {1, 1});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(circ == d);
  }
  GIVEN("Circuit with mid-circuit measurements 2") {
    // -X
    Circuit c1(1, 1);
    c1.add_op<unsigned>(OpType::Z, {0});
    c1.add_op<unsigned>(OpType::H, {0});
    c1.add_op<unsigned>(OpType::Measure, {0, 0});
    c1.add_op<unsigned>(OpType::T, {0});
    Circuit d1(1, 1);
    d1.add_op<unsigned>(OpType::H, {0});
    d1.add_op<unsigned>(OpType::X, {0});
    d1.add_op<unsigned>(OpType::Measure, {0, 0});
    d1.add_op<unsigned>(OpType::X, {0});
    d1.add_op<unsigned>(OpType::H, {0});
    d1.add_op<unsigned>(OpType::Rx, 3.75, {0});
    d1.add_op<unsigned>(OpType::H, {0});
    d1.add_op<unsigned>(OpType::X, {0});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c1));
    REQUIRE(c1 == d1);
    // Y
    Circuit c2(1, 1);
    c2.add_op<unsigned>(OpType::V, {0});
    c2.add_op<unsigned>(OpType::Measure, {0, 0});
    c2.add_op<unsigned>(OpType::T, {0});
    Circuit d2(1, 1);
    d2.add_op<unsigned>(OpType::V, {0});
    d2.add_op<unsigned>(OpType::Measure, {0, 0});
    d2.add_op<unsigned>(OpType::Vdg, {0});
    d2.add_op<unsigned>(OpType::Ry, 0.25, {0});
    d2.add_op<unsigned>(OpType::V, {0});
    // Vdg;Ry(0.25);V = T
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c2));
    REQUIRE(c2 == d2);
    // -Y
    Circuit c3(1, 1);
    c3.add_op<unsigned>(OpType::Vdg, {0});
    c3.add_op<unsigned>(OpType::Measure, {0, 0});
    c3.add_op<unsigned>(OpType::T, {0});
    Circuit d3(1, 1);
    d3.add_op<unsigned>(OpType::Vdg, {0});
    d3.add_op<unsigned>(OpType::Measure, {0, 0});
    d3.add_op<unsigned>(OpType::V, {0});
    d3.add_op<unsigned>(OpType::Ry, 3.75, {0});
    d3.add_op<unsigned>(OpType::V, {0});
    d3.add_op<unsigned>(OpType::X, {0});
    // V;Ry(3.75);V;X = T
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c3));
    REQUIRE(c3 == d3);
    // -Z
    Circuit c4(1, 1);
    c4.add_op<unsigned>(OpType::X, {0});
    c4.add_op<unsigned>(OpType::Measure, {0, 0});
    c4.add_op<unsigned>(OpType::T, {0});
    Circuit d4(1, 1);
    d4.add_op<unsigned>(OpType::X, {0});
    d4.add_op<unsigned>(OpType::Measure, {0, 0});
    d4.add_op<unsigned>(OpType::X, {0});
    d4.add_op<unsigned>(OpType::Rz, 3.75, {0});
    d4.add_op<unsigned>(OpType::X, {0});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c4));
    REQUIRE(c4 == d4);
  }

  GIVEN("Circuit with resets") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Reset, {0});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(circ == d);
  }

  GIVEN("Circuit with resets 2") {
    // -X/Z
    Circuit c1(1);
    c1.add_op<unsigned>(OpType::Z, {0});
    c1.add_op<unsigned>(OpType::H, {0});
    c1.add_op<unsigned>(OpType::Reset, {0});
    Circuit d1(1);
    d1.add_op<unsigned>(OpType::H, {0});
    d1.add_op<unsigned>(OpType::X, {0});
    d1.add_op<unsigned>(OpType::Reset, {0});
    d1.add_op<unsigned>(OpType::X, {0});
    d1.add_op<unsigned>(OpType::H, {0});
    d1.add_op<unsigned>(OpType::H, {0});
    d1.add_op<unsigned>(OpType::X, {0});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c1));
    REQUIRE(c1 == d1);

    // X/-Z
    Circuit c2(1);
    c2.add_op<unsigned>(OpType::X, {0});
    c2.add_op<unsigned>(OpType::H, {0});
    c2.add_op<unsigned>(OpType::Reset, {0});
    Circuit d2(1);
    d2.add_op<unsigned>(OpType::H, {0});
    d2.add_op<unsigned>(OpType::Z, {0});
    d2.add_op<unsigned>(OpType::Reset, {0});
    d2.add_op<unsigned>(OpType::Z, {0});
    d2.add_op<unsigned>(OpType::H, {0});
    d2.add_op<unsigned>(OpType::H, {0});
    d2.add_op<unsigned>(OpType::Z, {0});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c2));
    REQUIRE(c2 == d2);
  }

  GIVEN("XY rest") {
    Circuit c(1);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::S, {0});
    c.add_op<unsigned>(OpType::Reset, {0});
    // c is equivalent to Rest(X/Y);H;S
    // GPS should implement Rest(X/Y) as H;S;Reset(Z/X);Sdg;H
    Circuit d(1);
    d.add_op<unsigned>(OpType::H, {0});
    d.add_op<unsigned>(OpType::S, {0});
    d.add_op<unsigned>(OpType::Reset, {0});
    d.add_op<unsigned>(OpType::Sdg, {0});
    d.add_op<unsigned>(OpType::H, {0});
    d.add_op<unsigned>(OpType::H, {0});
    d.add_op<unsigned>(OpType::S, {0});

    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c));
    REQUIRE(c == d);
  }

  GIVEN("XZ rest") {
    Circuit c(1);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::Reset, {0});
    // c is equivalent to Rest(X/Z);H
    // GPS should implement Rest(X/Z) as H;Reset(Z/X);H
    Circuit d(1);
    d.add_op<unsigned>(OpType::H, {0});
    d.add_op<unsigned>(OpType::Reset, {0});
    d.add_op<unsigned>(OpType::H, {0});
    d.add_op<unsigned>(OpType::H, {0});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c));
    REQUIRE(c == d);
  }

  GIVEN("YX rest") {
    Circuit c(1);
    c.add_op<unsigned>(OpType::V, {0});
    c.add_op<unsigned>(OpType::Reset, {0});
    // c is equivalent to Rest(Y/X);V
    // GPS should implement Rest(Y/X) as V;Reset(Z/X);Vdg
    Circuit d(1);
    d.add_op<unsigned>(OpType::V, {0});
    d.add_op<unsigned>(OpType::Reset, {0});
    d.add_op<unsigned>(OpType::Vdg, {0});
    d.add_op<unsigned>(OpType::V, {0});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c));
    REQUIRE(c == d);
  }

  GIVEN("YZ rest") {
    Circuit c(1);
    c.add_op<unsigned>(OpType::Sdg, {0});
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::Reset, {0});
    // c is equivalent to Rest(Y/Z);Sdg;H
    // GPS should implement Rest(Y/Z) as Sdg;H;Reset(Z/X);H;S
    Circuit d(1);
    d.add_op<unsigned>(OpType::Sdg, {0});
    d.add_op<unsigned>(OpType::H, {0});
    d.add_op<unsigned>(OpType::Reset, {0});
    d.add_op<unsigned>(OpType::H, {0});
    d.add_op<unsigned>(OpType::S, {0});
    d.add_op<unsigned>(OpType::Sdg, {0});
    d.add_op<unsigned>(OpType::H, {0});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c));
    REQUIRE(c == d);
  }

  GIVEN("ZY rest") {
    Circuit c(1);
    c.add_op<unsigned>(OpType::Sdg, {0});
    c.add_op<unsigned>(OpType::Reset, {0});
    // c is equivalent to Rest(Z/Y);Sdg
    // GPS should implement Rest(Z/Y) as Sdg;Reset(Z/X);S
    Circuit d(1);
    d.add_op<unsigned>(OpType::Sdg, {0});
    d.add_op<unsigned>(OpType::Reset, {0});
    d.add_op<unsigned>(OpType::S, {0});
    d.add_op<unsigned>(OpType::Sdg, {0});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(c));
    REQUIRE(c == d);
  }

  GIVEN("Circuit with measures, classicals, and resets") {
    Circuit circ(3, 1);
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::Z, Pauli::Z}, 0.3)),
        {0, 1, 2});
    circ.add_op<unsigned>(OpType::Measure, {0, 0});
    circ.add_op<unsigned>(ClassicalX(), {0});
    circ.add_op<unsigned>(OpType::Reset, {1});
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(circ.count_n_qubit_gates(2) == 4);
    REQUIRE(circ.count_gates(OpType::ClassicalTransform) == 1);
    REQUIRE(circ.count_gates(OpType::Measure) == 1);
    REQUIRE(circ.count_gates(OpType::Reset) == 1);
  }
  GIVEN("Compile to ZZPhase") {
    Circuit circ(2);
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::X}, 0.3)), {0, 1});
    Circuit d1(circ);
    Circuit d2(circ);
    REQUIRE(
        Transforms::greedy_pauli_optimisation(0.7, 0.3, 500, 500, 0, true)
            .apply(d1));
    REQUIRE(
        Transforms::greedy_pauli_optimisation(0.7, 0.3, 500, 500, 0, false)
            .apply(d2));
    REQUIRE(test_unitary_comparison(circ, d1, true));
    REQUIRE(test_unitary_comparison(circ, d2, true));
    REQUIRE(d1.count_n_qubit_gates(2) == 1);
    REQUIRE(d2.count_n_qubit_gates(2) == 2);
  }
  GIVEN("Multiple ZZPhases at once") {
    Circuit circ(6);
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::X}, 0.3)), {0, 1});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::X}, 0.1)), {2, 3});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::Y}, 0.2)), {4, 5});
    Circuit d(circ);
    REQUIRE(
        Transforms::greedy_pauli_optimisation(0.7, 0.3, 500, 500, 0, true)
            .apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
    REQUIRE(d.count_n_qubit_gates(2) == 3);
  }
  GIVEN("Large circuit with ZZPhase") {
    Circuit circ(6);
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::X}, 0.3)), {0, 1});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::Y, Pauli::X}, 0.2)),
        {0, 1, 2});
    circ.add_box(
        PauliExpCommutingSetBox({
            {{Pauli::I, Pauli::Y, Pauli::I, Pauli::Z}, 1.2},
            {{Pauli::X, Pauli::Y, Pauli::Z, Pauli::I}, 0.8},
            {{Pauli::I, Pauli::I, Pauli::I, Pauli::Z}, 1.25},
        }),
        {1, 2, 3, 4});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::Y, Pauli::X}, 0.1)), {2, 3});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::Y, Pauli::X}, 0.11)),
        {1, 3, 4});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::Y, Pauli::Y}, 0.2)), {4, 5});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::Z, Pauli::X}, 0.15)),
        {2, 4, 5});
    circ.add_box(
        PauliExpBox(
            SymPauliTensor({Pauli::X, Pauli::X, Pauli::X, Pauli::X}, 0.25)),
        {2, 4, 5, 0});
    circ.add_box(
        PauliExpBox(
            SymPauliTensor({Pauli::Y, Pauli::Z, Pauli::Z, Pauli::X}, 0.125)),
        {1, 3, 5, 0});

    circ.add_box(
        PauliExpBox(SymPauliTensor(
            {Pauli::X, Pauli::Z, Pauli::Y, Pauli::Y, Pauli::Z, Pauli::X},
            0.125)),
        {1, 3, 5, 0, 2, 4});

    circ.add_box(
        PauliExpBox(SymPauliTensor(
            {Pauli::Z, Pauli::Y, Pauli::Y, Pauli::Z, Pauli::Z, Pauli::X},
            0.125)),
        {0, 1, 2, 3, 4, 5});

    circ.add_box(
        PauliExpBox(SymPauliTensor(
            {Pauli::X, Pauli::Z, Pauli::Y, Pauli::Z, Pauli::Z, Pauli::X},
            0.125)),
        {5, 2, 4, 1, 3, 0});

    circ.add_box(
        PauliExpBox(SymPauliTensor(
            {Pauli::X, Pauli::Z, Pauli::Y, Pauli::Y, Pauli::Z, Pauli::X},
            0.125)),
        {0, 5, 1, 4, 3, 2});

    Circuit d(circ);
    REQUIRE(
        !Transforms::greedy_pauli_optimisation(0.7, 0.3, 500, 500, 0, true, 0)
             .apply(d));
    REQUIRE(
        Transforms::greedy_pauli_optimisation(0.7, 0.3, 500, 500, 0, true, 10)
            .apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  GIVEN("Select TQE over ZZPhase") {
    Circuit circ(3);
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::X, Pauli::Y}, 0.3)), {0, 1});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::Y, Pauli::Z}, 0.22)),
        {0, 1, 2});
    circ.add_box(
        PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::Y, Pauli::X}, 0.15)),
        {0, 1, 2});
    Circuit d(circ);
    REQUIRE(
        Transforms::greedy_pauli_optimisation(0.7, 0.3, 500, 500, 0, true)
            .apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
    // if the first XY was implemented using a ZZPhase
    // then 2 TQEs is needed to conjugate the remaining two strings to
    // weight 2
    // hence 5 2-qubit gates in total.
    REQUIRE(d.count_n_qubit_gates(2) == 4);
  }
}

SCENARIO("Test GreedyPauliSimp for individual gates") {
  Circuit circ(1);
  circ.add_op<unsigned>(OpType::Z, {0});
  std::vector<Op_ptr> ops_0q = {
      get_op_ptr(OpType::Phase, 0.25),
  };
  std::vector<Op_ptr> ops_1q = {
      get_op_ptr(OpType::noop),
      get_op_ptr(OpType::Z),
      get_op_ptr(OpType::X),
      get_op_ptr(OpType::Y),
      get_op_ptr(OpType::S),
      get_op_ptr(OpType::V),
      get_op_ptr(OpType::Sdg),
      get_op_ptr(OpType::Vdg),
      get_op_ptr(OpType::H),
      get_op_ptr(OpType::Rz, 0.25),
      get_op_ptr(OpType::Rz, 0.5),
      get_op_ptr(OpType::Rx, 1),
      get_op_ptr(OpType::Rx, 0.15),
      get_op_ptr(OpType::Ry, 0.25),
      get_op_ptr(OpType::Ry, -0.5),
      get_op_ptr(OpType::PhasedX, {0.15, 0.2}),
      get_op_ptr(OpType::PhasedX, {0.5, -0.5}),
      get_op_ptr(OpType::PhasedX, {0.2, 1}),
      get_op_ptr(OpType::TK1, {0.25, 0.5, 0.75}),
      get_op_ptr(OpType::TK1, {0.012, 0.78, 0.1249}),
      get_op_ptr(OpType::TK1, {-0.25, 0.5, 0.5}),
      get_op_ptr(OpType::TK1, {0.12385, 0.281, 0.595}),
      get_op_ptr(OpType::T),
      get_op_ptr(OpType::Tdg),
  };
  std::vector<Op_ptr> ops_2q = {
      get_op_ptr(OpType::SWAP),
      get_op_ptr(OpType::CX),
      get_op_ptr(OpType::CY),
      get_op_ptr(OpType::CZ),
      get_op_ptr(OpType::ZZMax),
      get_op_ptr(OpType::ZZPhase, 0.25),
      get_op_ptr(OpType::ZZPhase, 0.5),
      get_op_ptr(OpType::PhaseGadget, 0.5, 2),
      get_op_ptr(OpType::XXPhase, 0.25),
      get_op_ptr(OpType::XXPhase, 0.5),
      get_op_ptr(OpType::YYPhase, 0.25),
      get_op_ptr(OpType::YYPhase, 1),
  };
  for (Op_ptr op : ops_0q) {
    Circuit circ(1);
    circ.add_op<unsigned>(op, {});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  for (Op_ptr op : ops_1q) {
    Circuit circ(1);
    circ.add_op<unsigned>(op, {0});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
  for (Op_ptr op : ops_2q) {
    Circuit circ(2);
    circ.add_op<unsigned>(op, {0, 1});
    Circuit d(circ);
    REQUIRE(Transforms::greedy_pauli_optimisation().apply(circ));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
}

SCENARIO("Test GreedyPauliSimp pass construction") {
  // test pass construction
  GIVEN("A circuit") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::Rz, 0.5, {1});
    CompilationUnit cu(c);
    CHECK(gen_greedy_pauli_simp(0.3, 0.5)->apply(cu));
    REQUIRE(test_unitary_comparison(c, cu.get_circ_ref(), true));
  }
  GIVEN("Preserving a circuit with only_reduce set to true.") {
    Circuit c(4);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::CX, {2, 3});
    CompilationUnit cu(c);
    REQUIRE(!gen_greedy_pauli_simp(0.3, 0.5, 500, 500, 0, false, 100, true)
                 ->apply(cu));
  }
  GIVEN("A circuit with implicit Clifford merging") {
    Circuit c(1);
    c.add_op<unsigned>(OpType::Rz, 1.005, {0});
    c.add_op<unsigned>(OpType::PhasedX, {1.50, 1.505}, {0});
    CompilationUnit cu(c);
    CHECK(gen_greedy_pauli_simp(0.3, 0.5)->apply(cu));
    REQUIRE(test_unitary_comparison(c, cu.get_circ_ref(), true));
  }
}

SCENARIO("Test GreedyPauliSimp with multiple trials and threads") {
  GIVEN("Large circuit with ZZPhase") {
    Circuit circ(6);
    for (unsigned i = 0; i < 1000; i++) {
      circ.add_box(
          PauliExpBox(SymPauliTensor({Pauli::X, Pauli::X}, 0.3)), {0, 1});
      circ.add_box(
          PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::Y, Pauli::X}, 0.2)),
          {0, 1, 2});
      circ.add_box(
          PauliExpCommutingSetBox({
              {{Pauli::I, Pauli::Y, Pauli::I, Pauli::Z}, 1.2},
              {{Pauli::X, Pauli::Y, Pauli::Z, Pauli::I}, 0.8},
              {{Pauli::I, Pauli::I, Pauli::I, Pauli::Z}, 1.25},
          }),
          {1, 2, 3, 4});
      circ.add_box(
          PauliExpBox(SymPauliTensor({Pauli::Y, Pauli::X}, 0.1)), {2, 3});
      circ.add_box(
          PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::Y, Pauli::X}, 0.11)),
          {1, 3, 4});
      circ.add_box(
          PauliExpBox(SymPauliTensor({Pauli::Y, Pauli::Y}, 0.2)), {4, 5});
      circ.add_box(
          PauliExpBox(SymPauliTensor({Pauli::Z, Pauli::Z, Pauli::X}, 0.15)),
          {2, 4, 5});
      circ.add_box(
          PauliExpBox(
              SymPauliTensor({Pauli::X, Pauli::X, Pauli::X, Pauli::X}, 0.25)),
          {2, 4, 5, 0});
      circ.add_box(
          PauliExpBox(
              SymPauliTensor({Pauli::Y, Pauli::Z, Pauli::Z, Pauli::X}, 0.125)),
          {1, 3, 5, 0});

      circ.add_box(
          PauliExpBox(SymPauliTensor(
              {Pauli::X, Pauli::Z, Pauli::Y, Pauli::Y, Pauli::Z, Pauli::X},
              0.125)),
          {1, 3, 5, 0, 2, 4});

      circ.add_box(
          PauliExpBox(SymPauliTensor(
              {Pauli::Z, Pauli::Y, Pauli::Y, Pauli::Z, Pauli::Z, Pauli::X},
              0.125)),
          {0, 1, 2, 3, 4, 5});

      circ.add_box(
          PauliExpBox(SymPauliTensor(
              {Pauli::X, Pauli::Z, Pauli::Y, Pauli::Z, Pauli::Z, Pauli::X},
              0.125)),
          {5, 2, 4, 1, 3, 0});

      circ.add_box(
          PauliExpBox(SymPauliTensor(
              {Pauli::X, Pauli::Z, Pauli::Y, Pauli::Y, Pauli::Z, Pauli::X},
              0.125)),
          {0, 5, 1, 4, 3, 2});
    }
    Circuit d(circ);
    REQUIRE(!Transforms::greedy_pauli_optimisation(
                 0.7, 0.3, 500, 500, 0, true, 0, 10)
                 .apply(d));
    REQUIRE(
        Transforms::greedy_pauli_optimisation(
            0.7, 0.3, 500, 500, 0, true, 100, 10)
            .apply(d));
    REQUIRE(test_unitary_comparison(circ, d, true));
  }
}
SCENARIO("Following a rebase pass") {
  PassPtr rebase =
      gen_auto_rebase_pass({OpType::ZZPhase, OpType::PhasedX, OpType::Rz});
  PassPtr paulisynth = gen_greedy_pauli_simp(0.3, 0.5);
  std::vector<PassPtr> passes = {rebase, paulisynth};
  PassPtr seq = std::make_shared<SequencePass>(passes);
  Circuit circ(1);
  circ.add_op<unsigned>(OpType::H, {0});
  CompilationUnit cu(circ);
  CHECK(seq->apply(cu));
}

}  // namespace test_GreedyPauliSimp
}  // namespace tket
