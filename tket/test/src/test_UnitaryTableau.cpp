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
#include <sstream>

#include "testutil.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Clifford/UnitaryTableau.hpp"
#include "tket/Converters/Converters.hpp"
#include "tket/Converters/UnitaryTableauBox.hpp"

namespace tket::test_UnitaryTableau {

static void add_ops_list_one_to_circuit(Circuit& circ) {
  circ.add_op<unsigned>(OpType::CX, {0, 1});
  circ.add_op<unsigned>(OpType::S, {1});
  circ.add_op<unsigned>(OpType::CX, {0, 1});
  circ.add_op<unsigned>(OpType::CX, {1, 2});
  circ.add_op<unsigned>(OpType::Vdg, {1});
  circ.add_op<unsigned>(OpType::CX, {1, 0});
}
static Circuit get_test_circ() {
  Circuit circ(3);
  add_ops_list_one_to_circuit(circ);
  return circ;
}
static void add_ops_list_two_to_circuit(
    Circuit& circ, OpType middle_op = OpType::S) {
  circ.add_op<unsigned>(OpType::H, {0});
  circ.add_op<unsigned>(OpType::V, {1});
  circ.add_op<unsigned>(OpType::CX, {0, 1});
  circ.add_op<unsigned>(OpType::CX, {1, 2});
  circ.add_op<unsigned>(middle_op, {2});
  circ.add_op<unsigned>(OpType::CX, {1, 2});
  circ.add_op<unsigned>(OpType::CX, {0, 1});
  circ.add_op<unsigned>(OpType::H, {0});
  circ.add_op<unsigned>(OpType::Vdg, {1});
}
static UnitaryTableau get_tableau_with_gates_applied_at_front() {
  UnitaryTableau tab(3);
  tab.apply_gate_at_front(OpType::CX, {Qubit(1), Qubit(0)});
  tab.apply_gate_at_front(OpType::Vdg, {Qubit(1)});
  tab.apply_gate_at_front(OpType::CX, {Qubit(1), Qubit(2)});
  tab.apply_gate_at_front(OpType::CX, {Qubit(0), Qubit(1)});
  tab.apply_gate_at_front(OpType::S, {Qubit(1)});
  tab.apply_gate_at_front(OpType::CX, {Qubit(0), Qubit(1)});
  return tab;
}
static UnitaryRevTableau get_rev_tableau_with_gates_applied_at_front() {
  UnitaryRevTableau tab(3);
  tab.apply_gate_at_front(OpType::CX, {Qubit(1), Qubit(0)});
  tab.apply_gate_at_front(OpType::Vdg, {Qubit(1)});
  tab.apply_gate_at_front(OpType::CX, {Qubit(1), Qubit(2)});
  tab.apply_gate_at_front(OpType::CX, {Qubit(0), Qubit(1)});
  tab.apply_gate_at_front(OpType::S, {Qubit(1)});
  tab.apply_gate_at_front(OpType::CX, {Qubit(0), Qubit(1)});
  return tab;
}
static SymplecticTableau get_initial_stab_destab_tab() {
  MatrixXb xmat(6, 3);
  MatrixXb zmat(6, 3);
  xmat << MatrixXb::Identity(3, 3), MatrixXb::Zero(3, 3);
  zmat << MatrixXb::Zero(3, 3), MatrixXb::Identity(3, 3);
  return SymplecticTableau(xmat, zmat, VectorXb::Zero(6));
}

SCENARIO("Correct updates of SymplecticTableau") {
  GIVEN("Check initial tableau") {
    SymplecticTableau tab = get_initial_stab_destab_tab();
    REQUIRE(tab.get_n_qubits() == 3);
    REQUIRE(tab.get_n_rows() == 6);
    REQUIRE(tab.rank() == 6);
    MatrixXb correct_anti_commuting(6, 6);
    correct_anti_commuting << MatrixXb::Zero(3, 3), MatrixXb::Identity(3, 3),
        MatrixXb::Identity(3, 3), MatrixXb::Zero(3, 3);
    CHECK(tab.anticommuting_rows() == correct_anti_commuting);
    tab.row_mult(0, 1);
    tab.row_mult(5, 3, -1.);
    std::stringstream tabstr;
    tabstr << tab;
    CHECK(
        tabstr.str() ==
        "1 0 0 0 0 0 0\n"
        "1 1 0 0 0 0 0\n"
        "0 0 1 0 0 0 0\n"
        "0 0 0 1 0 1 1\n"
        "0 0 0 0 1 0 0\n"
        "0 0 0 0 0 1 0\n");
    tab.gaussian_form();
    std::stringstream tabstr2;
    tabstr2 << tab;
    CHECK(
        tabstr2.str() ==
        "1 0 0 0 0 0 0\n"
        "0 0 0 1 0 0 1\n"
        "0 1 0 0 0 0 0\n"
        "0 0 0 0 1 0 0\n"
        "0 0 1 0 0 0 0\n"
        "0 0 0 0 0 1 0\n");
  }
  GIVEN("A single S gate") {
    SymplecticTableau tab0 = get_initial_stab_destab_tab();
    SymplecticTableau tab1 = get_initial_stab_destab_tab();
    SymplecticTableau tab2 = get_initial_stab_destab_tab();
    tab0.apply_S(0);
    tab1.apply_pauli_gadget(
        PauliStabiliser({Pauli::Z, Pauli::I, Pauli::I}, 0), 1);
    tab2.apply_gate(OpType::S, {0});
    std::stringstream tabstr;
    tabstr << tab0;
    // S is e^{-i Z pi/4}
    // Pauli reorder rules give e^{-i P pi/4} Q U = (-iPQ) e^{-i P pi/4} U
    // -iZX = +Y
    // So phase bit of updated row should be 0
    CHECK(
        tabstr.str() ==
        "1 0 0 1 0 0 0\n"
        "0 1 0 0 0 0 0\n"
        "0 0 1 0 0 0 0\n"
        "0 0 0 1 0 0 0\n"
        "0 0 0 0 1 0 0\n"
        "0 0 0 0 0 1 0\n");
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
  }
  GIVEN("A single V gate") {
    SymplecticTableau tab0 = get_initial_stab_destab_tab();
    SymplecticTableau tab1 = get_initial_stab_destab_tab();
    SymplecticTableau tab2 = get_initial_stab_destab_tab();
    tab0.apply_V(0);
    tab1.apply_pauli_gadget(
        PauliStabiliser({Pauli::X, Pauli::I, Pauli::I}, 0), 1);
    tab2.apply_gate(OpType::V, {0});
    std::stringstream tabstr;
    tabstr << tab0;
    // V is e^{-i X pi/4}
    // -iXZ = -Y
    // So phase bit of updated row should be 1
    CHECK(
        tabstr.str() ==
        "1 0 0 0 0 0 0\n"
        "0 1 0 0 0 0 0\n"
        "0 0 1 0 0 0 0\n"
        "1 0 0 1 0 0 1\n"
        "0 0 0 0 1 0 0\n"
        "0 0 0 0 0 1 0\n");
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
  }
  GIVEN("A single CX gate") {
    SymplecticTableau tab0 = get_initial_stab_destab_tab();
    SymplecticTableau tab1 = get_initial_stab_destab_tab();
    SymplecticTableau tab2 = get_initial_stab_destab_tab();
    SymplecticTableau tab3 = get_initial_stab_destab_tab();
    tab0.apply_CX(0, 1);
    tab1.apply_pauli_gadget(
        PauliStabiliser({Pauli::Z, Pauli::I, Pauli::I}, 0), 1);
    tab1.apply_pauli_gadget(
        PauliStabiliser({Pauli::I, Pauli::X, Pauli::I}, 0), 1);
    tab1.apply_pauli_gadget(
        PauliStabiliser({Pauli::Z, Pauli::X, Pauli::I}, 2), 1);
    tab2.apply_pauli_gadget(
        PauliStabiliser({Pauli::Z, Pauli::I, Pauli::I}, 0), 3);
    tab2.apply_pauli_gadget(
        PauliStabiliser({Pauli::I, Pauli::X, Pauli::I}, 0), 3);
    tab2.apply_pauli_gadget(
        PauliStabiliser({Pauli::Z, Pauli::X, Pauli::I}, 2), 3);
    tab3.apply_gate(OpType::CX, {0, 1});
    std::stringstream tabstr;
    tabstr << tab0;
    CHECK(
        tabstr.str() ==
        "1 1 0 0 0 0 0\n"
        "0 1 0 0 0 0 0\n"
        "0 0 1 0 0 0 0\n"
        "0 0 0 1 0 0 0\n"
        "0 0 0 1 1 0 0\n"
        "0 0 0 0 0 1 0\n");
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
    CHECK(tab0 == tab3);
  }
}

SCENARIO("Correct creation of UnitaryTableau") {
  GIVEN("An identity circuit") {
    UnitaryTableau tab(3);
    REQUIRE(tab.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z));
    REQUIRE(tab.get_zrow(Qubit(1)) == SpPauliStabiliser(Qubit(1), Pauli::Z));
    REQUIRE(tab.get_zrow(Qubit(2)) == SpPauliStabiliser(Qubit(2), Pauli::Z));
    REQUIRE(tab.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X));
    REQUIRE(tab.get_xrow(Qubit(1)) == SpPauliStabiliser(Qubit(1), Pauli::X));
    REQUIRE(tab.get_xrow(Qubit(2)) == SpPauliStabiliser(Qubit(2), Pauli::X));
  }
  GIVEN("A single S gate") {
    UnitaryTableau tab0(3);
    UnitaryTableau tab1(3);
    UnitaryTableau tab2(3);
    UnitaryTableau tab3(3);
    UnitaryTableau tab4(3);
    UnitaryTableau tab5(3);
    tab0.apply_S_at_front(Qubit(0));
    tab1.apply_S_at_end(Qubit(0));
    tab2.apply_gate_at_front(OpType::S, {Qubit(0)});
    tab3.apply_gate_at_end(OpType::S, {Qubit(0)});
    tab4.apply_pauli_at_front(SpPauliStabiliser(Qubit(0), Pauli::Z), 1);
    tab5.apply_pauli_at_end(SpPauliStabiliser(Qubit(0), Pauli::Z), 1);
    // Phases should match those in the tests for SymplecticTableau
    CHECK(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z));
    CHECK(tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Y));
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
    CHECK(tab0 == tab3);
    CHECK(tab0 == tab4);
    CHECK(tab0 == tab5);
  }
  GIVEN("A single V gate") {
    UnitaryTableau tab0(3);
    UnitaryTableau tab1(3);
    UnitaryTableau tab2(3);
    UnitaryTableau tab3(3);
    UnitaryTableau tab4(3);
    UnitaryTableau tab5(3);
    tab0.apply_V_at_front(Qubit(0));
    tab1.apply_V_at_end(Qubit(0));
    tab2.apply_gate_at_front(OpType::V, {Qubit(0)});
    tab3.apply_gate_at_end(OpType::V, {Qubit(0)});
    tab4.apply_pauli_at_front(SpPauliStabiliser(Qubit(0), Pauli::X), 1);
    tab5.apply_pauli_at_end(SpPauliStabiliser(Qubit(0), Pauli::X), 1);
    CHECK(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Y, 2));
    CHECK(tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X, 0));
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
    CHECK(tab0 == tab3);
    CHECK(tab0 == tab4);
    CHECK(tab0 == tab5);
  }
  GIVEN("A single Z gate") {
    UnitaryTableau tab0(3);
    UnitaryTableau tab1(3);
    UnitaryTableau tab2(3);
    UnitaryTableau tab3(3);
    UnitaryTableau tab4(3);
    UnitaryTableau tab5(3);
    tab0.apply_gate_at_front(OpType::Z, {Qubit(0)});
    tab1.apply_gate_at_end(OpType::Z, {Qubit(0)});
    tab2.apply_gate_at_front(OpType::S, {Qubit(0)});
    tab2.apply_gate_at_front(OpType::S, {Qubit(0)});
    tab3.apply_gate_at_end(OpType::Sdg, {Qubit(0)});
    tab3.apply_gate_at_end(OpType::Sdg, {Qubit(0)});
    tab4.apply_Z_at_front(Qubit(0));
    tab5.apply_Z_at_end(Qubit(0));
    CHECK(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z));
    CHECK(tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X, 2));
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
    CHECK(tab0 == tab3);
    CHECK(tab0 == tab4);
    CHECK(tab0 == tab5);
  }
  GIVEN("A single X gate") {
    UnitaryTableau tab0(3);
    UnitaryTableau tab1(3);
    UnitaryTableau tab2(3);
    UnitaryTableau tab3(3);
    UnitaryTableau tab4(3);
    UnitaryTableau tab5(3);
    tab0.apply_gate_at_front(OpType::X, {Qubit(0)});
    tab1.apply_gate_at_end(OpType::X, {Qubit(0)});
    tab2.apply_gate_at_front(OpType::V, {Qubit(0)});
    tab2.apply_gate_at_front(OpType::V, {Qubit(0)});
    tab3.apply_gate_at_end(OpType::Vdg, {Qubit(0)});
    tab3.apply_gate_at_end(OpType::Vdg, {Qubit(0)});
    tab4.apply_X_at_front(Qubit(0));
    tab5.apply_X_at_end(Qubit(0));
    CHECK(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z, 2));
    CHECK(tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X));
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
    CHECK(tab0 == tab3);
    CHECK(tab0 == tab4);
    CHECK(tab0 == tab5);
  }
  GIVEN("A single H gate") {
    UnitaryTableau tab0(3);
    UnitaryTableau tab1(3);
    UnitaryTableau tab2(3);
    UnitaryTableau tab3(3);
    UnitaryTableau tab4(3);
    UnitaryTableau tab5(3);
    tab0.apply_gate_at_front(OpType::H, {Qubit(0)});
    tab1.apply_gate_at_end(OpType::H, {Qubit(0)});
    tab2.apply_gate_at_front(OpType::S, {Qubit(0)});
    tab2.apply_gate_at_front(OpType::V, {Qubit(0)});
    tab2.apply_gate_at_front(OpType::S, {Qubit(0)});
    tab3.apply_gate_at_end(OpType::Vdg, {Qubit(0)});
    tab3.apply_gate_at_end(OpType::Sdg, {Qubit(0)});
    tab3.apply_gate_at_end(OpType::Vdg, {Qubit(0)});
    tab4.apply_H_at_front(Qubit(0));
    tab5.apply_H_at_end(Qubit(0));
    CHECK(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X));
    CHECK(tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z));
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
    CHECK(tab0 == tab3);
    CHECK(tab0 == tab4);
    CHECK(tab0 == tab5);
  }
  GIVEN("A single CX gate") {
    UnitaryTableau tab0(3);
    UnitaryTableau tab1(3);
    UnitaryTableau tab2(3);
    UnitaryTableau tab3(3);
    tab0.apply_CX_at_front(Qubit(0), Qubit(1));
    tab1.apply_CX_at_end(Qubit(0), Qubit(1));
    tab2.apply_pauli_at_front(SpPauliStabiliser(Qubit(0), Pauli::Z), 1);
    tab2.apply_pauli_at_front(SpPauliStabiliser(Qubit(1), Pauli::X), 1);
    tab2.apply_pauli_at_front(
        SpPauliStabiliser(DensePauliMap{Pauli::Z, Pauli::X}), 3);
    tab3.apply_pauli_at_end(SpPauliStabiliser(Qubit(0), Pauli::Z), 3);
    tab3.apply_pauli_at_end(SpPauliStabiliser(Qubit(1), Pauli::X), 3);
    tab3.apply_pauli_at_end(
        SpPauliStabiliser(DensePauliMap{Pauli::Z, Pauli::X}), 1);
    CHECK(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z));
    CHECK(tab0.get_xrow(Qubit(1)) == SpPauliStabiliser(Qubit(1), Pauli::X));
    CHECK(
        tab0.get_zrow(Qubit(1)) ==
        SpPauliStabiliser(DensePauliMap{Pauli::Z, Pauli::Z}));
    CHECK(
        tab0.get_xrow(Qubit(0)) ==
        SpPauliStabiliser(DensePauliMap{Pauli::X, Pauli::X}));
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
    CHECK(tab0 == tab3);
  }
  GIVEN("A Clifford circuit") {
    Circuit circ = get_test_circ();
    UnitaryTableau tab = circuit_to_unitary_tableau(circ);
    UnitaryTableau rev_tab = get_tableau_with_gates_applied_at_front();
    REQUIRE(tab == rev_tab);
    Eigen::MatrixXcd circ_u = tket_sim::get_unitary(circ);
    for (unsigned q = 0; q < 3; ++q) {
      CmplxSpMat xq = SpPauliString(Qubit(q), Pauli::X).to_sparse_matrix(3);
      Eigen::MatrixXcd xqd = xq;
      PauliStabiliser xrow = tab.get_xrow(Qubit(q));
      CmplxSpMat xrowmat = xrow.to_sparse_matrix(3);
      Eigen::MatrixXcd xrowmatd = xrowmat;
      CHECK((xrowmatd * circ_u * xqd).isApprox(circ_u));
      CmplxSpMat zq = SpPauliString(Qubit(q), Pauli::Z).to_sparse_matrix(3);
      Eigen::MatrixXcd zqd = zq;
      PauliStabiliser zrow = tab.get_zrow(Qubit(q));
      CmplxSpMat zrowmat = zrow.to_sparse_matrix(3);
      Eigen::MatrixXcd zrowmatd = zrowmat;
      CHECK((zrowmatd * circ_u * zqd).isApprox(circ_u));
    }
  }
  GIVEN("A PI/2 rotation") {
    Circuit circ = get_test_circ();
    UnitaryTableau tab = circuit_to_unitary_tableau(circ);
    SpPauliStabiliser pauli(DensePauliMap{Pauli::X, Pauli::Y, Pauli::Z});
    tab.apply_pauli_at_end(pauli, 3);

    add_ops_list_two_to_circuit(circ, OpType::Sdg);
    UnitaryTableau correct_tab = circuit_to_unitary_tableau(circ);
    REQUIRE(tab == correct_tab);
  }
  GIVEN("A PI/2 rotation at front") {
    UnitaryTableau tab = get_tableau_with_gates_applied_at_front();
    SpPauliStabiliser pauli(DensePauliMap{Pauli::X, Pauli::Y, Pauli::Z});
    tab.apply_pauli_at_front(pauli, 1);

    Circuit circ(3);
    add_ops_list_two_to_circuit(circ);
    add_ops_list_one_to_circuit(circ);
    UnitaryTableau correct_tab = circuit_to_unitary_tableau(circ);
    REQUIRE(tab == correct_tab);
  }
  GIVEN("Combining two circuits via tableau compose") {
    Circuit circ(3);
    add_ops_list_one_to_circuit(circ);
    UnitaryTableau first = circuit_to_unitary_tableau(circ);

    Circuit circ1(3);
    add_ops_list_two_to_circuit(circ1);
    UnitaryTableau second = circuit_to_unitary_tableau(circ1);
    UnitaryTableau correct = circuit_to_unitary_tableau(circ >> circ1);
    UnitaryTableau result = UnitaryTableau::compose(first, second);
    REQUIRE(result == correct);
  }
}

SCENARIO("Error handling in UnitaryTableau generation") {
  GIVEN("Add a non-clifford gate at end") {
    UnitaryTableau tab(2);
    REQUIRE_THROWS_AS(tab.apply_gate_at_end(OpType::T, {Qubit(0)}), BadOpType);
  }
  GIVEN("Add a non-clifford gate at front") {
    UnitaryTableau tab(2);
    REQUIRE_THROWS_AS(
        tab.apply_gate_at_front(OpType::Tdg, {Qubit(0)}), BadOpType);
  }
  GIVEN("Tableau from a non-Clifford circuit") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CH, {1, 0});
    REQUIRE_THROWS_AS(circuit_to_unitary_tableau(circ), BadOpType);
  }
}

SCENARIO("Synthesis of circuits from UnitaryTableau") {
  GIVEN("Transform a circuit in a round trip") {
    Circuit circ(3);
    add_ops_list_one_to_circuit(circ);
    UnitaryTableau tab = circuit_to_unitary_tableau(circ);
    Circuit res = unitary_tableau_to_circuit(tab);
    UnitaryTableau res_tab = circuit_to_unitary_tableau(res);
    REQUIRE(res_tab == tab);
  }
  GIVEN("Additional gate coverage, check unitary") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::ZZMax, {0, 1});
    circ.add_op<unsigned>(OpType::ECR, {2, 3});
    circ.add_op<unsigned>(OpType::ISWAPMax, {1, 2});
    circ.add_op<unsigned>(OpType::noop, {0});
    UnitaryTableau tab = circuit_to_unitary_tableau(circ);
    UnitaryTableau rev_tab(4);
    rev_tab.apply_gate_at_front(OpType::noop, {Qubit(0)});
    rev_tab.apply_gate_at_front(OpType::ISWAPMax, {Qubit(1), Qubit(2)});
    rev_tab.apply_gate_at_front(OpType::ECR, {Qubit(2), Qubit(3)});
    rev_tab.apply_gate_at_front(OpType::ZZMax, {Qubit(0), Qubit(1)});
    REQUIRE(tab == rev_tab);
    Circuit res = unitary_tableau_to_circuit(tab);
    REQUIRE(test_unitary_comparison(circ, res, true));
  }
}

SCENARIO("Correct creation of UnitaryRevTableau") {
  GIVEN("An identity circuit") {
    UnitaryRevTableau tab(3);
    REQUIRE(tab.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z));
    REQUIRE(tab.get_zrow(Qubit(1)) == SpPauliStabiliser(Qubit(1), Pauli::Z));
    REQUIRE(tab.get_zrow(Qubit(2)) == SpPauliStabiliser(Qubit(2), Pauli::Z));
    REQUIRE(tab.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X));
    REQUIRE(tab.get_xrow(Qubit(1)) == SpPauliStabiliser(Qubit(1), Pauli::X));
    REQUIRE(tab.get_xrow(Qubit(2)) == SpPauliStabiliser(Qubit(2), Pauli::X));
  }
  GIVEN("A single S gate") {
    UnitaryRevTableau tab0(3);
    UnitaryRevTableau tab1(3);
    UnitaryRevTableau tab2(3);
    UnitaryRevTableau tab3(3);
    UnitaryRevTableau tab4(3);
    UnitaryRevTableau tab5(3);
    tab0.apply_S_at_end(Qubit(0));
    tab1.apply_S_at_front(Qubit(0));
    tab2.apply_gate_at_end(OpType::S, {Qubit(0)});
    tab3.apply_gate_at_front(OpType::S, {Qubit(0)});
    tab4.apply_pauli_at_end(SpPauliStabiliser(Qubit(0), Pauli::Z), 1);
    tab5.apply_pauli_at_front(SpPauliStabiliser(Qubit(0), Pauli::Z), 1);
    // Reading the stabilizers in the reverse direction changes how we apply the
    // Pauli reorder rules to determine the correct phase:
    // U Q e^{-i P pi/4} = U e^{-i P pi/4}.
    // (iPQ) iZX = -Y
    CHECK(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z, 0));
    CHECK(tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Y, 2));
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
    CHECK(tab0 == tab3);
    CHECK(tab0 == tab4);
    CHECK(tab0 == tab5);
  }
  GIVEN("A single V gate") {
    UnitaryRevTableau tab0(3);
    UnitaryRevTableau tab1(3);
    UnitaryRevTableau tab2(3);
    UnitaryRevTableau tab3(3);
    UnitaryRevTableau tab4(3);
    UnitaryRevTableau tab5(3);
    tab0.apply_V_at_end(Qubit(0));
    tab1.apply_V_at_front(Qubit(0));
    tab2.apply_gate_at_end(OpType::V, {Qubit(0)});
    tab3.apply_gate_at_front(OpType::V, {Qubit(0)});
    tab4.apply_pauli_at_end(SpPauliStabiliser(Qubit(0), Pauli::X), 1);
    tab5.apply_pauli_at_front(SpPauliStabiliser(Qubit(0), Pauli::X), 1);
    // iXZ = +Y
    CHECK(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Y));
    CHECK(tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X));
    CHECK(tab0 == tab1);
    CHECK(tab0 == tab2);
    CHECK(tab0 == tab3);
    CHECK(tab0 == tab4);
    CHECK(tab0 == tab5);
  }
  GIVEN("A single Z gate") {
    UnitaryRevTableau tab0(3);
    UnitaryRevTableau tab1(3);
    UnitaryRevTableau tab2(3);
    UnitaryRevTableau tab3(3);
    tab0.apply_gate_at_end(OpType::Z, {Qubit(0)});
    tab1.apply_gate_at_front(OpType::Z, {Qubit(0)});
    tab2.apply_Z_at_end(Qubit(0));
    tab3.apply_Z_at_front(Qubit(0));
    REQUIRE(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z));
    REQUIRE(
        tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X, 2));
    REQUIRE(tab0 == tab1);
    REQUIRE(tab0 == tab2);
    REQUIRE(tab0 == tab3);
  }
  GIVEN("A single X gate") {
    UnitaryRevTableau tab0(3);
    UnitaryRevTableau tab1(3);
    UnitaryRevTableau tab2(3);
    UnitaryRevTableau tab3(3);
    tab0.apply_gate_at_end(OpType::X, {Qubit(0)});
    tab1.apply_gate_at_front(OpType::X, {Qubit(0)});
    tab2.apply_X_at_end(Qubit(0));
    tab3.apply_X_at_front(Qubit(0));
    REQUIRE(
        tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z, 2));
    REQUIRE(tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X));
    REQUIRE(tab0 == tab1);
    REQUIRE(tab0 == tab2);
    REQUIRE(tab0 == tab3);
  }
  GIVEN("A single H gate") {
    UnitaryRevTableau tab0(3);
    UnitaryRevTableau tab1(3);
    UnitaryRevTableau tab2(3);
    UnitaryRevTableau tab3(3);
    tab0.apply_gate_at_end(OpType::H, {Qubit(0)});
    tab1.apply_gate_at_front(OpType::H, {Qubit(0)});
    tab2.apply_H_at_end(Qubit(0));
    tab3.apply_H_at_front(Qubit(0));
    REQUIRE(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::X));
    REQUIRE(tab0.get_xrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z));
    REQUIRE(tab0 == tab1);
    REQUIRE(tab0 == tab2);
    REQUIRE(tab0 == tab3);
  }
  GIVEN("A single CX gate") {
    UnitaryRevTableau tab0(3);
    UnitaryRevTableau tab1(3);
    tab0.apply_CX_at_end(Qubit(0), Qubit(1));
    tab1.apply_CX_at_front(Qubit(0), Qubit(1));
    REQUIRE(tab0.get_zrow(Qubit(0)) == SpPauliStabiliser(Qubit(0), Pauli::Z));
    REQUIRE(tab0.get_xrow(Qubit(1)) == SpPauliStabiliser(Qubit(1), Pauli::X));
    REQUIRE(
        tab0.get_zrow(Qubit(1)) ==
        SpPauliStabiliser(DensePauliMap{Pauli::Z, Pauli::Z}));
    REQUIRE(
        tab0.get_xrow(Qubit(0)) ==
        SpPauliStabiliser(DensePauliMap{Pauli::X, Pauli::X}));
    REQUIRE(tab0 == tab1);
    std::stringstream tabstr;
    tabstr << tab0;
    CHECK(
        tabstr.str() ==
        "1 1 0   0 0 0   0\t->\tX@q[0]\n"
        "0 1 0   0 0 0   0\t->\tX@q[1]\n"
        "0 0 1   0 0 0   0\t->\tX@q[2]\n"
        "--\n"
        "0 0 0   1 0 0   0\t->\tZ@q[0]\n"
        "0 0 0   1 1 0   0\t->\tZ@q[1]\n"
        "0 0 0   0 0 1   0\t->\tZ@q[2]\n");
  }
  GIVEN("A Clifford circuit") {
    Circuit circ = get_test_circ();
    UnitaryRevTableau tab = circuit_to_unitary_rev_tableau(circ);
    UnitaryRevTableau rev_tab = get_rev_tableau_with_gates_applied_at_front();
    REQUIRE(tab == rev_tab);
  }
  GIVEN("A PI/2 rotation") {
    Circuit circ = get_test_circ();
    UnitaryRevTableau tab = circuit_to_unitary_rev_tableau(circ);
    SpPauliStabiliser pauli(DensePauliMap{Pauli::X, Pauli::Y, Pauli::Z});
    tab.apply_pauli_at_end(pauli, 3);

    add_ops_list_two_to_circuit(circ, OpType::Sdg);
    UnitaryRevTableau correct_tab = circuit_to_unitary_rev_tableau(circ);
    REQUIRE(tab == correct_tab);
  }
  GIVEN("A PI/2 rotation at front") {
    UnitaryRevTableau tab = get_rev_tableau_with_gates_applied_at_front();
    SpPauliStabiliser pauli(DensePauliMap{Pauli::X, Pauli::Y, Pauli::Z});
    tab.apply_pauli_at_front(pauli, 1);

    Circuit circ(3);
    add_ops_list_two_to_circuit(circ);
    add_ops_list_one_to_circuit(circ);
    UnitaryRevTableau correct_tab = circuit_to_unitary_rev_tableau(circ);
    REQUIRE(tab == correct_tab);
  }
  GIVEN("Combining two circuits via tableau compose") {
    Circuit circ(3);
    add_ops_list_one_to_circuit(circ);
    UnitaryRevTableau first = circuit_to_unitary_rev_tableau(circ);

    Circuit circ1(3);
    add_ops_list_two_to_circuit(circ1);
    UnitaryRevTableau second = circuit_to_unitary_rev_tableau(circ1);
    UnitaryRevTableau correct = circuit_to_unitary_rev_tableau(circ >> circ1);
    UnitaryRevTableau result = UnitaryRevTableau::compose(first, second);
    REQUIRE(result == correct);
  }
}

SCENARIO("Error handling in UnitaryRevTableau generation") {
  GIVEN("Add a non-clifford gate at front") {
    UnitaryRevTableau tab(2);
    REQUIRE_THROWS_AS(
        tab.apply_gate_at_front(OpType::T, {Qubit(0)}), BadOpType);
  }
  GIVEN("Add a non-clifford gate at end") {
    UnitaryRevTableau tab(2);
    REQUIRE_THROWS_AS(
        tab.apply_gate_at_end(OpType::Tdg, {Qubit(0)}), BadOpType);
  }
  GIVEN("Tableau from a non-Clifford circuit") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CH, {1, 0});
    REQUIRE_THROWS_AS(circuit_to_unitary_rev_tableau(circ), BadOpType);
  }
}

SCENARIO("Synthesis of circuits from UnitaryRevTableau") {
  GIVEN("Transform a circuit in a round trip") {
    Circuit circ(3);
    add_ops_list_one_to_circuit(circ);
    UnitaryRevTableau tab = circuit_to_unitary_rev_tableau(circ);
    Circuit res = unitary_rev_tableau_to_circuit(tab);
    UnitaryRevTableau res_tab = circuit_to_unitary_rev_tableau(res);
    REQUIRE(res_tab == tab);
  }
  GIVEN("Another circuit containing more OpTypes") {
    // https://github.com/quantinuum/tket/issues/1268
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::SX, {0});
    circ.add_op<unsigned>(OpType::SXdg, {1});
    circ.add_op<unsigned>(OpType::ZZMax, {0, 1});
    circ.add_op<unsigned>(OpType::SX, {0});
    circ.add_op<unsigned>(OpType::SXdg, {1});
    circ.add_op<unsigned>(OpType::ECR, {0, 1});
    circ.add_op<unsigned>(OpType::SX, {0});
    circ.add_op<unsigned>(OpType::SXdg, {1});
    circ.add_op<unsigned>(OpType::ISWAPMax, {0, 1});
    UnitaryRevTableau tab = circuit_to_unitary_rev_tableau(circ);
    Circuit res = unitary_rev_tableau_to_circuit(tab);
    REQUIRE(test_unitary_comparison(circ, res, true));
    UnitaryRevTableau res_tab = circuit_to_unitary_rev_tableau(res);
    REQUIRE(res_tab == tab);
    // Manually call apply_gate methods
    UnitaryRevTableau tab_front(2);
    tab_front.apply_gate_at_front(OpType::ISWAPMax, {Qubit(0), Qubit(1)});
    tab_front.apply_gate_at_front(OpType::SXdg, {Qubit(1)});
    tab_front.apply_gate_at_front(OpType::SX, {Qubit(0)});
    tab_front.apply_gate_at_front(OpType::ECR, {Qubit(0), Qubit(1)});
    tab_front.apply_gate_at_front(OpType::SXdg, {Qubit(1)});
    tab_front.apply_gate_at_front(OpType::SX, {Qubit(0)});
    tab_front.apply_gate_at_front(OpType::ZZMax, {Qubit(0), Qubit(1)});
    tab_front.apply_gate_at_front(OpType::SXdg, {Qubit(1)});
    tab_front.apply_gate_at_front(OpType::SX, {Qubit(0)});
    REQUIRE(tab == tab_front);
    UnitaryRevTableau tab_end(2);
    tab_end.apply_gate_at_end(OpType::SX, {Qubit(0)});
    tab_end.apply_gate_at_end(OpType::SXdg, {Qubit(1)});
    tab_end.apply_gate_at_end(OpType::ZZMax, {Qubit(0), Qubit(1)});
    tab_end.apply_gate_at_end(OpType::SX, {Qubit(0)});
    tab_end.apply_gate_at_end(OpType::SXdg, {Qubit(1)});
    tab_end.apply_gate_at_end(OpType::ECR, {Qubit(0), Qubit(1)});
    tab_end.apply_gate_at_end(OpType::SX, {Qubit(0)});
    tab_end.apply_gate_at_end(OpType::SXdg, {Qubit(1)});
    tab_end.apply_gate_at_end(OpType::ISWAPMax, {Qubit(0), Qubit(1)});
    REQUIRE(tab == tab_end);
  }
}

SCENARIO("UnitaryTableauBoxes in Circuits") {
  GIVEN("A tableau from another circuit") {
    Circuit inner(3);
    add_ops_list_one_to_circuit(inner);
    UnitaryTableau tab = circuit_to_unitary_tableau(inner);
    Circuit circ(4);
    Op_ptr box = std::make_shared<const UnitaryTableauBox>(tab);
    circ.add_op<unsigned>(OpType::CZ, uvec({1, 2}));
    circ.add_op<unsigned>(box, uvec({0, 1, 3}));
    circ.add_op<unsigned>(OpType::CX, uvec({0, 2}));
    Circuit correct(4);
    correct.add_op<unsigned>(OpType::CZ, uvec({1, 2}));
    correct.add_op<unsigned>(OpType::SWAP, uvec({2, 3}));
    add_ops_list_one_to_circuit(correct);
    correct.add_op<unsigned>(OpType::SWAP, uvec({2, 3}));
    correct.add_op<unsigned>(OpType::CX, uvec({0, 2}));
    REQUIRE(test_unitary_comparison(circ, correct, true));
  }
}

SCENARIO("Unitary inversions") {
  GIVEN("Some unitary tableau") {
    Circuit inner(3);
    add_ops_list_one_to_circuit(inner);
    UnitaryTableau tab = circuit_to_unitary_tableau(inner);
    Op_ptr box = std::make_shared<const UnitaryTableauBox>(tab);
    WHEN("Dagger") {
      Op_ptr box_dagger = box->dagger();
      Circuit circ(3);
      circ.add_op<unsigned>(box_dagger, uvec{0, 1, 2});
      REQUIRE(test_unitary_comparison(circ, inner.dagger(), true));
    }
    WHEN("Transpose") {
      Op_ptr box_transpose = box->transpose();
      Circuit circ(3);
      circ.add_op<unsigned>(box_transpose, uvec{0, 1, 2});
      REQUIRE(test_unitary_comparison(circ, inner.transpose(), true));
    }
  }
  GIVEN("Same with UnitaryRevTableau") {
    Circuit circ(3);
    add_ops_list_one_to_circuit(circ);
    UnitaryRevTableau tab = circuit_to_unitary_rev_tableau(circ);
    WHEN("Dagger") {
      UnitaryRevTableau dag_tab = tab.dagger();
      Circuit dag_circ = unitary_rev_tableau_to_circuit(dag_tab);
      REQUIRE(test_unitary_comparison(dag_circ, circ.dagger(), true));
    }
    WHEN("Transpose") {
      UnitaryRevTableau tp_tab = tab.transpose();
      Circuit tp_circ = unitary_rev_tableau_to_circuit(tp_tab);
      REQUIRE(test_unitary_comparison(tp_circ, circ.transpose(), true));
    }
    WHEN("Conjugate") {
      UnitaryRevTableau con_tab = tab.conjugate();
      Circuit con_circ = unitary_rev_tableau_to_circuit(con_tab);
      REQUIRE(
          test_unitary_comparison(con_circ, circ.dagger().transpose(), true));
    }
  }
}

SCENARIO("Compare SymplecticTableau and UnitaryTableau") {
  GIVEN("The same sequence of gates, compare string representations") {
    SymplecticTableau stab(
        PauliStabiliserVec{
            {DensePauliMap{Pauli::X, Pauli::I, Pauli::I}},
            {DensePauliMap{Pauli::I, Pauli::X, Pauli::I}},
            {DensePauliMap{Pauli::I, Pauli::I, Pauli::X}},
            {DensePauliMap{Pauli::Z, Pauli::I, Pauli::I}},
            {DensePauliMap{Pauli::I, Pauli::Z, Pauli::I}},
            {DensePauliMap{Pauli::I, Pauli::I, Pauli::Z}},
        });
    // Paulis cancel with subsequent gadget
    stab.apply_gate(OpType::X, uvec{0});
    stab.apply_gate(OpType::Y, uvec{1});
    stab.apply_gate(OpType::Z, uvec{2});
    stab.apply_pauli_gadget({DensePauliMap{Pauli::X, Pauli::Y, Pauli::Z}}, 2);
    // CY and CZ combine to Sdg(0), CX(0, 1)
    stab.apply_gate(OpType::CY, uvec{0, 1});
    stab.apply_gate(OpType::CZ, uvec{0, 1});
    // SWAP that will remain
    stab.apply_gate(OpType::SWAP, uvec{1, 2});
    // BRIDGE cancels CX from CY+CZ
    stab.apply_gate(OpType::BRIDGE, uvec{0, 1, 2});
    std::stringstream stabstr;
    stabstr << stab;
    CHECK(
        stabstr.str() ==
        "1 0 0 1 0 0 1\n"
        "0 0 1 0 0 0 0\n"
        "0 1 0 0 0 0 0\n"
        "0 0 0 1 0 0 0\n"
        "0 0 0 0 0 1 0\n"
        "0 0 0 0 1 0 0\n");
    UnitaryTableau utab(3);
    // Same sequence, but appended to the front instead of the end
    utab.apply_gate_at_front(OpType::BRIDGE, {Qubit(0), Qubit(1), Qubit(2)});
    utab.apply_gate_at_front(OpType::SWAP, {Qubit(1), Qubit(2)});
    utab.apply_gate_at_front(OpType::CZ, {Qubit(0), Qubit(1)});
    utab.apply_gate_at_front(OpType::CY, {Qubit(0), Qubit(1)});
    utab.apply_pauli_at_front({DensePauliMap{Pauli::X, Pauli::Y, Pauli::Z}}, 2);
    utab.apply_gate_at_front(OpType::X, {Qubit(0)});
    utab.apply_gate_at_front(OpType::Y, {Qubit(1)});
    utab.apply_gate_at_front(OpType::Z, {Qubit(2)});
    std::stringstream utabstr;
    utabstr << utab;
    CHECK(
        utabstr.str() ==
        "X@q[0]\t->\t1 0 0   1 0 0   1\n"
        "X@q[1]\t->\t0 0 1   0 0 0   0\n"
        "X@q[2]\t->\t0 1 0   0 0 0   0\n"
        "--\n"
        "Z@q[0]\t->\t0 0 0   1 0 0   0\n"
        "Z@q[1]\t->\t0 0 0   0 0 1   0\n"
        "Z@q[2]\t->\t0 0 0   0 1 0   0\n");
  }
}

SCENARIO("Tableau serialisation") {
  GIVEN("A circuit containing a tableau") {
    MatrixXb xx(3, 3);
    MatrixXb xz(3, 3);
    VectorXb xph(3);
    MatrixXb zx(3, 3);
    MatrixXb zz(3, 3);
    VectorXb zph(3);
    xx << 1, 1, 0, 0, 1, 0, 0, 0, 1;
    xz << 0, 0, 0, 0, 0, 0, 0, 0, 1;
    xph << 0, 0, 1;
    zx << 0, 0, 0, 0, 1, 0, 0, 0, 0;
    zz << 1, 0, 0, 1, 1, 0, 0, 0, 1;
    zph << 1, 0, 1;
    Op_ptr box =
        std::make_shared<const UnitaryTableauBox>(xx, xz, xph, zx, zz, zph);
    REQUIRE(box->is_clifford());
    Circuit circ(3);
    circ.add_op<unsigned>(box, uvec{0, 1, 2});

    nlohmann::json j_circ = circ;
    Circuit circ2 = j_circ.get<Circuit>();
    REQUIRE(circ2 == circ);
  }
  GIVEN("Serialising a UnitaryRevTableau") {
    Circuit circ = get_test_circ();
    UnitaryRevTableau tab = circuit_to_unitary_rev_tableau(circ);
    nlohmann::json j_tab = tab;
    UnitaryRevTableau tab2 = j_tab.get<UnitaryRevTableau>();
    REQUIRE(tab == tab2);
  }
}

}  // namespace tket::test_UnitaryTableau
