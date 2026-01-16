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

#include "Simulation/ComparisonFunctions.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Predicates/CompilerPass.hpp"
#include "tket/Predicates/PassGenerators.hpp"
#include "tket/Transformations/ContextualReduction.hpp"
#include "tket/Transformations/Transform.hpp"

namespace tket {
namespace test_ContextOpt {

SCENARIO("Create and Discard operations") {
  GIVEN("An ordinary circuit") {
    Circuit c(3, 2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::CX, {1, 0});
    c.add_op<unsigned>(OpType::H, {2});
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Measure, {2, 1});
    qubit_vector_t qubits = c.all_qubits();
    c.qubit_create(qubits[0]);
    c.qubit_create(qubits[1]);
    c.qubit_discard(qubits[1]);
    c.qubit_discard(qubits[2]);
    V_iterator i, end;
    std::map<OpType, unsigned> counts = c.op_counts();
    REQUIRE(counts[OpType::Input] == 1);
    REQUIRE(counts[OpType::Create] == 2);
    REQUIRE(counts[OpType::Output] == 1);
    REQUIRE(counts[OpType::Discard] == 2);
    REQUIRE(counts[OpType::H] == 3);
    REQUIRE(counts[OpType::CX] == 2);
    REQUIRE(counts[OpType::Measure] == 2);
    REQUIRE(c.get_commands().size() == 7);
    REQUIRE(c.is_created(qubits[0]));
    REQUIRE(!c.is_discarded(qubits[0]));
    REQUIRE(c.is_created(qubits[1]));
    REQUIRE(c.is_discarded(qubits[1]));
    REQUIRE(!c.is_created(qubits[2]));
    REQUIRE(c.is_discarded(qubits[2]));
  }
  GIVEN("Composition of circuits") {
    Circuit c0(4);
    c0.add_op<unsigned>(OpType::H, {0});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    c0.add_op<unsigned>(OpType::H, {2});
    c0.add_op<unsigned>(OpType::CX, {2, 3});
    c0.add_op<unsigned>(OpType::CCX, {1, 2, 3});
    Circuit c1 = c0;
    c1.qubit_create(Qubit(1));
    c0.qubit_discard(Qubit(3));
    c1.qubit_create(Qubit(3));
    Circuit c2 = c0;
    c0.append(c1);
    V_iterator i, end;
    std::map<OpType, unsigned> counts = c0.op_counts();
    REQUIRE(counts[OpType::Input] == 4);
    REQUIRE(counts[OpType::Output] == 4);
    REQUIRE(counts[OpType::Reset] == 2);
    REQUIRE(counts[OpType::H] == 4);
    REQUIRE(counts[OpType::CX] == 4);
    REQUIRE(counts[OpType::CCX] == 2);
    c2.qubit_discard(Qubit(2));
    REQUIRE_THROWS_AS(c2.append(c1), CircuitInvalidity);
  }
  GIVEN("Unmeasurable gates") {
    Circuit c(4, 1);
    qubit_vector_t q = c.all_qubits();
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::CX, {2, 3});
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::H, {2});
    c.add_op<unsigned>(OpType::Measure, {3, 0});
    c.qubit_discard(q[0]);
    c.qubit_discard(q[1]);
    REQUIRE(c.count_gates(OpType::CX) == 3);
    REQUIRE(c.count_gates(OpType::H) == 2);
    REQUIRE(Transforms::remove_discarded_ops().apply(c));
    REQUIRE(c.count_gates(OpType::CX) == 2);
    // The H on qubit 2 should remain because it hasn't been discarded.
    REQUIRE(c.count_gates(OpType::H) == 1);
  }
  GIVEN("Initial classical maps") {
    Circuit c(4);
    c.qubit_create(Qubit(1));
    c.qubit_create(Qubit(2));
    c.qubit_create(Qubit(3));
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::X, {0});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::Y, {3});
    c.add_op<unsigned>(OpType::Z, {3});
    c.add_op<unsigned>(OpType::CX, {2, 3});
    c.add_op<unsigned>(OpType::CZ, {1, 2});
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::H, {2});
    c.add_op<unsigned>(OpType::H, {3});
    c.add_op<unsigned>(OpType::Reset, {2});
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::Y, {2});
    c.add_op<unsigned>(OpType::SWAP, {1, 2});
    REQUIRE(Transforms::simplify_initial().apply(c));
    REQUIRE(c.count_gates(OpType::H) == 3);
    REQUIRE(c.count_gates(OpType::X) == 7);
    REQUIRE(c.count_gates(OpType::Y) == 0);
    REQUIRE(c.count_gates(OpType::Reset) == 1);
    REQUIRE(c.count_gates(OpType::CX) == 0);
    REQUIRE(c.count_gates(OpType::CZ) == 0);
    REQUIRE(c.count_gates(OpType::SWAP) == 0);
  }
  GIVEN("State vector after removing initial classical maps") {
    Circuit c(4);
    c.qubit_create(Qubit(1));
    c.qubit_create(Qubit(2));
    c.qubit_create(Qubit(3));
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::X, {0});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::Y, {3});
    c.add_op<unsigned>(OpType::Z, {3});
    c.add_op<unsigned>(OpType::CX, {2, 3});
    c.add_op<unsigned>(OpType::CZ, {1, 2});
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::H, {2});
    c.add_op<unsigned>(OpType::H, {3});
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::Y, {2});
    c.add_op<unsigned>(OpType::SWAP, {1, 2});
    const auto s = tket_sim::get_statevector(c);
    REQUIRE(Transforms::simplify_initial().apply(c));
    const auto s1 = tket_sim::get_statevector(c);
    REQUIRE(
        tket_sim::compare_statevectors_or_unitaries(
            s, s1, tket_sim::MatrixEquivalence::EQUAL_UP_TO_GLOBAL_PHASE));
  }
  GIVEN("Circuit with zero-preserving ops") {
    Circuit c(3);
    c.add_op<unsigned>(OpType::CH, {0, 1});
    c.add_op<unsigned>(OpType::CX, {2, 1});
    c.add_op<unsigned>(OpType::CH, {1, 2});
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::Y, {1});
    c.add_op<unsigned>(OpType::Z, {2});
    const auto s = tket_sim::get_statevector(c);
    REQUIRE(
        Transforms::simplify_initial(
            Transforms::AllowClassical::Yes, Transforms::CreateAllQubits::Yes)
            .apply(c));
    REQUIRE(c.count_gates(OpType::CH) == 0);
    REQUIRE(c.count_gates(OpType::CX) == 0);
    REQUIRE(c.count_gates(OpType::H) == 1);
    REQUIRE(c.count_gates(OpType::Y) == 0);
    REQUIRE(c.count_gates(OpType::Z) == 0);
    REQUIRE(c.count_gates(OpType::X) == 1);
    const auto s1 = tket_sim::get_statevector(c);
    REQUIRE(
        tket_sim::compare_statevectors_or_unitaries(
            s, s1, tket_sim::MatrixEquivalence::EQUAL_UP_TO_GLOBAL_PHASE));
  }
  GIVEN("Circuit tracking known computational-basis states") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::H, {1});
    c.add_op<unsigned>(OpType::Reset, {0});
    c.add_op<unsigned>(OpType::Reset, {1});
    c.add_op<unsigned>(OpType::X, {0});
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::ESWAP, 0.25, {0, 1});
    REQUIRE(Transforms::simplify_initial().apply(c));
    REQUIRE(c.count_gates(OpType::H) == 2);
    REQUIRE(c.count_gates(OpType::Reset) == 2);
    REQUIRE(c.count_gates(OpType::X) == 2);
    REQUIRE(c.count_gates(OpType::ESWAP) == 0);
  }
  GIVEN("Permutation of computational basis defined by a Unitary2qBox") {
    Eigen::Matrix4cd m;
    m << 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0;
    Unitary2qBox ubox(m);
    Circuit c(2);
    c.qubit_create_all();
    c.add_op<unsigned>(OpType::X, {1});
    c.add_box(ubox, {0, 1});
    REQUIRE(Transforms::simplify_initial().apply(c));
    REQUIRE(c.count_gates(OpType::X) == 2);
    REQUIRE(c.count_gates(OpType::Unitary2qBox) == 0);
  }
  GIVEN("A general unitary that maps 01 to 11") {
    // A random 3x3 unitary generated using scipy.stats.unitary_group.rvs:
    Eigen::Matrix3cd X = Eigen::Matrix3cd::Zero();
    X(0, 0) = Complex(-0.1257627612858676, 0.5339680535044199);
    X(0, 1) = Complex(0.13303378515004494, -0.039425619949907315);
    X(0, 2) = Complex(-0.5848560922806899, -0.5811650622093808);
    X(1, 0) = Complex(0.2902966360126237, -0.14739450774914262);
    X(1, 1) = Complex(-0.18937235300569172, -0.8620304955353689);
    X(1, 2) = Complex(0.12953569222179317, -0.3134721093333459);
    X(2, 0) = Complex(0.041297990158390696, -0.7689987281683078);
    X(2, 1) = Complex(0.44396399176500223, 0.06844810589370819);
    X(2, 2) = Complex(-0.3958183865180194, -0.22016827154320687);
    CHECK(is_unitary(X));
    Eigen::Matrix4cd m;
    // clang-format off
    m << X(0, 0),       0, X(0, 1), X(0, 2),
         X(1, 0),       0, X(1, 1), X(1, 2),
         X(2, 0),       0, X(2, 1), X(2, 2),
               0,       1,       0,       0;
    // clang-format on
    Unitary2qBox ubox(m);
    Circuit c(2);
    c.add_op<unsigned>(OpType::X, {1});
    c.add_box(ubox, {0, 1});
    REQUIRE(
        Transforms::simplify_initial(
            Transforms::AllowClassical::Yes, Transforms::CreateAllQubits::Yes)
            .apply(c));
    REQUIRE(c.count_gates(OpType::X) == 2);
    REQUIRE(c.count_gates(OpType::Unitary2qBox) == 0);
  }
  GIVEN("A classical map before a Measure") {
    Circuit c(1, 1);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::X, {0});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.qubit_discard_all();
    REQUIRE(Transforms::simplify_measured().apply(c));
    REQUIRE(c.count_gates(OpType::H) == 1);
    REQUIRE(c.count_gates(OpType::X) == 0);
    REQUIRE(c.count_gates(OpType::Measure) == 1);
    REQUIRE(c.count_gates(OpType::ClassicalTransform) == 1);
  }
  GIVEN("A Bell circuit where both qubits are discarded") {
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Measure, {1, 1});
    c.qubit_discard_all();
    REQUIRE(Transforms::simplify_measured().apply(c));
    REQUIRE(c.count_gates(OpType::H) == 1);
    REQUIRE(c.count_gates(OpType::CX) == 0);
    REQUIRE(c.count_gates(OpType::Measure) == 2);
    REQUIRE(c.count_gates(OpType::ClassicalTransform) == 1);
  }
  GIVEN("A Bell circuit where only one qubit is discarded") {
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Measure, {1, 1});
    c.qubit_discard(Qubit(0));
    REQUIRE(!Transforms::simplify_measured().apply(c));
  }
  GIVEN("A circuit with a measurement on a known basis state") {
    Circuit c(2, 1);
    c.qubit_create(Qubit(0));
    c.add_op<unsigned>(OpType::X, {0});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_conditional_gate<unsigned>(OpType::H, {}, {1}, {0}, 1);
    REQUIRE(Transforms::simplify_initial().apply(c));
    REQUIRE(c.count_gates(OpType::X) == 1);
    REQUIRE(c.count_gates(OpType::Measure) == 0);
    REQUIRE(c.count_gates(OpType::SetBits) == 1);
    REQUIRE(c.count_gates(OpType::Conditional) == 1);
  }
}

SCENARIO("Contextual optimization") {
  GIVEN("A circuit") {
    Circuit c(3, 3);
    c.add_op<unsigned>(OpType::X, {0});
    c.add_op<unsigned>(OpType::H, {1});
    c.add_op<unsigned>(OpType::H, {2});
    c.add_op<unsigned>(OpType::CY, {1, 2});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Measure, {1, 1});
    c.add_op<unsigned>(OpType::Measure, {2, 2});
    // Without any Create or Discard ...
    CompilationUnit cu0(c);
    PassPtr pp = gen_contextual_pass();
    REQUIRE(!pp->apply(cu0));
    // With Create and Discard ...
    c.qubit_create_all();
    c.qubit_discard_all();
    CompilationUnit cu1(c);
    REQUIRE(pp->apply(cu1));
    const Circuit &c1 = cu1.get_circ_ref();
    REQUIRE(c1.count_gates(OpType::X) == 0);
    REQUIRE(c1.count_gates(OpType::H) == 2);
    REQUIRE(c1.count_gates(OpType::CY) == 0);
    REQUIRE(c1.count_gates(OpType::Measure) == 2);
    REQUIRE(c1.count_gates(OpType::SetBits) == 1);
    REQUIRE(c1.count_gates(OpType::ClassicalTransform) == 2);
    auto [c0, ppc] = Transforms::separate_classical(c1);
    REQUIRE(c0.count_gates(OpType::H) == 2);
    REQUIRE(c0.count_gates(OpType::Measure) == 2);
    REQUIRE(ppc.count_gates(OpType::SetBits) == 1);
    REQUIRE(ppc.count_gates(OpType::ClassicalTransform) == 2);
  }
  GIVEN("Classical circuit evaluation") {
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Measure, {1, 1});
    c.qubit_create_all();
    c.qubit_discard_all();
    CompilationUnit cu(c);
    REQUIRE(gen_contextual_pass()->apply(cu));
    auto [c0, ppc] = Transforms::separate_classical(cu.get_circ_ref());
    // ppc should set Bit(1) to the value of Bit(0)
    std::map<Bit, bool> values;
    for (unsigned n = 0; n < 4; n++) {
      values[Bit(0)] = n & 1;
      values[Bit(1)] = (n >> 1) & 1;
      std::map<Bit, bool> new_values = ppc.classical_eval(values);
      REQUIRE(new_values[Bit(0)] == values[Bit(0)]);
      REQUIRE(new_values[Bit(1)] == new_values[Bit(0)]);
    }
  }
  GIVEN("Circuit with quantum op following final measurement") {
    // https://github.com/quantinuum/tket/issues/1794
    Circuit c(1, 1);
    c.add_op<unsigned>(OpType::X, {0});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::X, {0});
    auto [c0, ppc] = Transforms::separate_classical(c);
    CHECK(c0 == c);
    CHECK(ppc == Circuit(0, 1));
  }
}

}  // namespace test_ContextOpt
}  // namespace tket
