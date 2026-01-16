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

#include <Eigen/Core>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <memory>
#include <tket/Circuit/ToffoliBox.hpp>
#include <tket/OpType/OpType.hpp>
#include <tket/Ops/Op.hpp>
#include <tket/Ops/OpPtr.hpp>
#include <tket/Utils/Expression.hpp>

#include "../testutil.hpp"
#include "tket/Circuit/Boxes.hpp"
#include "tket/Circuit/CircUtils.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/ConjugationBox.hpp"
#include "tket/Circuit/DiagonalBox.hpp"
#include "tket/Circuit/Multiplexor.hpp"
#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Circuit/StatePreparation.hpp"
#include "tket/Circuit/ToffoliBox.hpp"
#include "tket/Converters/PhasePoly.hpp"
#include "tket/Gate/SymTable.hpp"

using Catch::Matchers::StartsWith;

namespace tket {
namespace test_Boxes {

SCENARIO("CircBox in-place symbol substitution") {
  Sym asym = SymEngine::symbol("a");
  Expr alpha(asym);
  Sym bsym = SymEngine::symbol("b");
  Expr beta(bsym);
  Circuit test_circuit(1);
  test_circuit.add_op<unsigned>(OpType::Rx, {alpha}, {0});
  test_circuit.add_op<unsigned>(OpType::Ry, {beta}, {0});
  auto circ_box = CircBox(test_circuit);
  SymSet sym_set0 = circ_box.free_symbols();
  CHECK(sym_set0.size() == 2);
  double x = 0.125;
  double y = 0.250;
  symbol_map_t map = {{asym, x}, {bsym, y}};
  circ_box.symbol_substitution_in_place(map);
  SymSet sym_set1 = circ_box.free_symbols();
  CHECK(sym_set1.empty());
  REQUIRE(!circ_box.to_circuit()->is_symbolic());
}

SCENARIO("Using Boxes", "[boxes]") {
  GIVEN("CircBox manipulation") {
    // Empty box
    CircBox cb;
    Circuit empty;
    REQUIRE(*(cb.to_circuit()) == empty);
    // Small box
    Circuit u(2);
    u.add_op<unsigned>(OpType::Ry, -0.75, {0});
    u.add_op<unsigned>(OpType::CX, {0, 1});
    const CircBox ubox(u);
    Circuit v(2);
    v.add_box(ubox, {0, 1});
    {
      const auto raw_u_unitary = tket_sim::get_unitary(u);
      const auto v_unitary = tket_sim::get_unitary(v);
      CHECK(raw_u_unitary.isApprox(v_unitary));
    }
    Circuit c0(3);
    c0.add_op<unsigned>(OpType::Rx, 0.5, {0});
    c0.add_op<unsigned>(OpType::Ry, 1.5, {1});
    c0.add_op<unsigned>(OpType::Rz, 0.75, {2});
    c0.add_box(ubox, {1, 0});
    c0.add_op<unsigned>(OpType::CX, {1, 2});
    REQUIRE(c0.n_gates() == 5);
    CircBox c0box(c0);
    // Basic utility methods
    REQUIRE(c0box.n_qubits() == 3);
    REQUIRE(c0box.n_boolean() == 0);
    REQUIRE(c0box.n_classical() == 0);
    // Put them in a bigger circuit
    Circuit d(4, 3);
    d.add_box(c0box, {1, 2, 0});
    d.add_op<unsigned>(OpType::CX, {0, 3});
    REQUIRE(d.n_gates() == 2);
    d.add_box(c0box, {3, 2, 1});
    REQUIRE(d.n_gates() == 3);
    d.add_box(c0box, {2, 3, 1});
    REQUIRE(d.n_gates() == 4);
    // Box up the bigger circuit
    CircBox dbox(d);
    Circuit e(4, 3);
    e.add_box(dbox, {/*qbs*/ 0, 1, 2, 3, /*cbs*/ 0, 1, 2});
    e.add_box(dbox, {/*qbs*/ 1, 2, 3, 0, /*cbs*/ 1, 2, 0});
    REQUIRE(e.n_gates() == 2);
    REQUIRE(!e.is_symbolic());
    // A circuit equivalent to c0 without boxes
    Circuit c0a(3);
    c0a.add_op<unsigned>(OpType::Rx, 0.5, {0});
    c0a.add_op<unsigned>(OpType::Ry, 1.5, {1});
    c0a.add_op<unsigned>(OpType::Rz, 0.75, {2});
    c0a.add_op<unsigned>(OpType::Ry, -0.75, {1});
    c0a.add_op<unsigned>(OpType::CX, {1, 0});
    c0a.add_op<unsigned>(OpType::CX, {1, 2});
    // Check c0 and c0a are equivalent
    Eigen::MatrixXcd uc0 = tket_sim::get_unitary(c0);
    Eigen::MatrixXcd uc0a = tket_sim::get_unitary(c0a);
    REQUIRE((uc0 - uc0a).cwiseAbs().sum() < ERR_EPS);
    // Test circuit name access
    CircBox cbox(Circuit(2));
    cbox.set_circuit_name("test_box");
    REQUIRE(cbox.get_circuit_name() == "test_box");
  }
  GIVEN("CircBox with non-default units") {
    Circuit c0;
    c0.add_q_register("a", 2);
    c0.add_q_register("b", 1);
    c0.add_op<Qubit>(
        OpType::CCX, {Qubit("a", 1), Qubit("a", 0), Qubit("b", 0)});
    CircBox cbox(c0);
    Circuit c(3);
    c.add_box(cbox, {0, 2, 1});
    REQUIRE(!test_unitary_comparison(c0, c));
    Circuit c1(3);
    c1.add_box(cbox, {0, 1, 2});
    REQUIRE(test_unitary_comparison(c0, c1));
  }
  GIVEN("Unitary1qBox manipulation") {
    // random 1qb gate
    Circuit setup(1);
    setup.add_op<unsigned>(OpType::TK1, {0.2374, 1.0353, 0.5372}, {0});
    Eigen::Matrix2cd m = get_matrix_from_circ(setup);
    Unitary1qBox mbox(m);
    Circuit c(1);
    c.add_box(mbox, qubit_vector_t{Qubit("q", 0)});
    REQUIRE(c.n_gates() == 1);
    // extract its circuit
    std::shared_ptr<Circuit> excirc = mbox.to_circuit();
    // check we extract the same circuit from the deserialized box
    VertexSet vset = c.get_gates_of_type(OpType::Unitary1qBox);
    REQUIRE(vset.size() == 1);
    Vertex v = *vset.begin();
    Op_ptr op = c.get_Op_ptr_from_Vertex(v);
    std::shared_ptr<const Unitary1qBox> b =
        std::dynamic_pointer_cast<const Unitary1qBox>(op);
    std::shared_ptr<Circuit> excirc1 = b->to_circuit();
    REQUIRE(*excirc1 == *excirc);
    // compose with inverse of box
    c.append(c.dagger());
    Eigen::MatrixXcd c1m = tket_sim::get_unitary(c);
    // check it's the identity
    REQUIRE((c1m - Eigen::Matrix2cd::Identity()).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("Unitary2qBox manipulation") {
    // permutation matrix
    Eigen::Matrix4cd m;
    m << 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0;
    Unitary2qBox mbox(m);
    Circuit c(2);
    c.add_box(mbox, {0, 1});
    REQUIRE(c.n_gates() == 1);
    // make a more complicated 2-qubit circuit
    Circuit d(2);
    d.add_op<unsigned>(OpType::Rx, 0.2, {0});
    d.add_op<unsigned>(OpType::Ry, 1.2, {1});
    d.add_op<unsigned>(OpType::CX, {0, 1});
    d.add_op<unsigned>(OpType::Rz, 0.4, {1});
    d.add_op<unsigned>(OpType::H, {0});
    d.add_op<unsigned>(OpType::CX, {1, 0});
    // get its unitary
    Eigen::Matrix4cd dm = get_matrix_from_2qb_circ(d);
    // make a box out of this
    Unitary2qBox dbox(dm);
    // make this into a new circuit
    Circuit d1(2);
    d1.add_box(dbox, {0, 1});
    // compose with inverse of d
    d1.append(d.dagger());
    Eigen::MatrixXcd d1m = tket_sim::get_unitary(d1);
    // check it's the identity
    REQUIRE((d1m - Eigen::Matrix4cd::Identity()).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("Unitary Box Identity constructors") {
    REQUIRE(Unitary1qBox().to_circuit());
    REQUIRE(Unitary1qBox().get_unitary() == Unitary1qBox().get_matrix());
    REQUIRE(
        Unitary1qBox().dagger()->get_unitary() == Unitary1qBox().get_unitary());
    REQUIRE(Unitary2qBox().to_circuit());
    REQUIRE(Unitary2qBox().get_unitary() == Unitary2qBox().get_matrix());
    REQUIRE(
        Unitary2qBox().dagger()->get_unitary() == Unitary2qBox().get_unitary());
    REQUIRE(Unitary3qBox().to_circuit());
    REQUIRE(Unitary3qBox().get_unitary() == Unitary3qBox().get_matrix());
    REQUIRE(
        Unitary3qBox().dagger()->get_unitary() == Unitary3qBox().get_unitary());
  }
  GIVEN("little-endian representation") {
    Eigen::Matrix4cd m0;
    m0 << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0;
    Unitary2qBox m0box(m0);
    Circuit c0(2);
    c0.add_box(m0box, {0, 1});
    Circuit c1(2);
    c1.add_op<unsigned>(OpType::CX, {0, 1});
    Eigen::Matrix4cd m1 = get_matrix_from_2qb_circ(c1);
    REQUIRE((m0 - m1).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("ExpBox manipulation") {
    // empty
    Circuit empty(2);
    empty.add_op<unsigned>(OpType::TK1, {0, 0, 0}, {0});
    empty.add_op<unsigned>(OpType::TK1, {0, 0, 0}, {1});
    empty.add_op<unsigned>(OpType::TK2, {0, 0, 0}, {0, 1});
    empty.add_op<unsigned>(OpType::TK1, {0, 0, 0}, {0});
    empty.add_op<unsigned>(OpType::TK1, {0, 0, 0}, {1});
    REQUIRE(*(ExpBox().to_circuit()) == empty);
    // random hermitian matrix
    Eigen::Matrix4cd A;
    A << 0., 1., 2., 3., 1., 2., 3. * i_, 4., 2., -3. * i_, 3, 2. - 3. * i_, 3.,
        4., 2. + 3. * i_, 5.;
    ExpBox ebox(A, -0.5);
    Circuit c(2);
    c.add_box(ebox, {0, 1});
    Eigen::Matrix4cd U = (+0.5 * i_ * A).exp();  // should be the inverse
    Unitary2qBox ubox(U);
    c.add_box(ubox, {0, 1});  // should act as the identity
    Eigen::MatrixXcd uc = tket_sim::get_unitary(c);
    REQUIRE((uc - Eigen::Matrix4cd::Identity()).cwiseAbs().sum() < ERR_EPS);
  }
}

SCENARIO("box daggers", "[boxes]") {
  GIVEN("a circuit made of various boxes") {
    // CircuitBox
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::Ry, -0.75, {0});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    CircBox cbox(c0);
    // Unitary2qBox
    Eigen::Matrix4cd m;
    m << 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0;
    Unitary2qBox ubox(m);
    // ExpBox
    Eigen::Matrix4cd A;
    A << 0., 1., 2., 3., 1., 2., 3. * i_, 4., 2., -3. * i_, 3, 2. - 3. * i_, 3.,
        4., 2. + 3. * i_, 5.;
    ExpBox ebox(A, -0.5);
    // PauliExpBox
    PauliExpBox pbox(SymPauliTensor({Pauli::X, Pauli::Y, Pauli::Z}, 0.8));

    // Put all these boxes into a circuit
    Circuit w(3);
    w.add_op<unsigned>(OpType::Rx, 0.5, {0});
    w.add_op<unsigned>(OpType::CX, {0, 1});
    w.add_box(cbox, {1, 2});
    w.add_box(ubox, {1, 0});
    w.add_box(ebox, {2, 1});
    w.add_box(pbox, {1, 2, 0});

    // Compute the dagger
    Circuit wdag = w.dagger();

    // Check dagger is correct
    w.append(wdag);
    Eigen::MatrixXcd u = tket_sim::get_unitary(w);
    REQUIRE((u - Eigen::MatrixXcd::Identity(8, 8)).cwiseAbs().sum() < ERR_EPS);
  }
}

SCENARIO("QControlBox", "[boxes]") {
  GIVEN("controlled X") {
    Op_ptr op = get_op_ptr(OpType::X);
    QControlBox qcbox(op);
    REQUIRE(qcbox.get_op() == op);
    REQUIRE(qcbox.get_n_controls() == 1);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    Circuit expected(2);
    expected.add_op<unsigned>(OpType::CX, {0, 1});
    REQUIRE(*c == expected);
  }
  GIVEN("controlled CX") {
    Op_ptr op = get_op_ptr(OpType::CX);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    Circuit expected(3);
    expected.add_op<unsigned>(OpType::CCX, {0, 1, 2});
    REQUIRE(*c == expected);
  }
  GIVEN("controlled CCX") {
    Op_ptr op = get_op_ptr(OpType::CCX);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    Circuit expected(4);
    expected.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3});
    REQUIRE(*c == expected);
  }
  GIVEN("controlled CnX") {
    Circuit c0(4);
    c0.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3});
    Op_ptr op = c0.get_commands()[0].get_op_ptr();
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    Circuit expected(5);
    expected.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4});
    REQUIRE(*c == expected);
  }
  GIVEN("controlled CnY") {
    Circuit c0(4);
    c0.add_op<unsigned>(OpType::CnY, {0, 1, 2, 3});
    Op_ptr op = c0.get_commands()[0].get_op_ptr();
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    Circuit expected(5);
    expected.add_op<unsigned>(OpType::CnY, {0, 1, 2, 3, 4});
    REQUIRE(*c == expected);
  }
  GIVEN("controlled CnZ") {
    Circuit c0(4);
    c0.add_op<unsigned>(OpType::CnZ, {0, 1, 2, 3});
    Op_ptr op = c0.get_commands()[0].get_op_ptr();
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    Circuit expected(5);
    expected.add_op<unsigned>(OpType::CnZ, {0, 1, 2, 3, 4});
    REQUIRE(*c == expected);
  }
  GIVEN("controlled Rz") {
    double a = 0.125;
    Circuit c0(1);
    c0.add_op<unsigned>(OpType::Rz, a, {0});
    Op_ptr op = c0.get_commands()[0].get_op_ptr();
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(4, 4);
    V(2, 2) = exp(-0.5 * i_ * PI * a);
    V(3, 3) = exp(0.5 * i_ * PI * a);
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled Rx") {
    double a = 0.125;
    Circuit c0(1);
    c0.add_op<unsigned>(OpType::Rx, a, {0});
    Op_ptr op = c0.get_commands()[0].get_op_ptr();
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(4, 4);
    V(2, 2) = std::cos(0.5 * PI * a);
    V(2, 3) = i_ * std::sin(-0.5 * PI * a);
    V(3, 2) = i_ * std::sin(-0.5 * PI * a);
    V(3, 3) = std::cos(0.5 * PI * a);
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled Ry") {
    double a = 0.125;
    Circuit c0(1);
    c0.add_op<unsigned>(OpType::Ry, a, {0});
    Op_ptr op = c0.get_commands()[0].get_op_ptr();
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(4, 4);
    V(2, 2) = std::cos(0.5 * PI * a);
    V(2, 3) = std::sin(-0.5 * PI * a);
    V(3, 2) = std::sin(0.5 * PI * a);
    V(3, 3) = std::cos(0.5 * PI * a);
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled SU2") {
    Op_ptr op = get_op_ptr(OpType::TK1, {0.92, 1.23, 3.34}, 1);
    QControlBox qcbox(op, 10);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    // Make sure the CnSU2 decomp method is used
    unsigned n_cx = c->count_gates(OpType::CX);
    unsigned n_cry = c->count_gates(OpType::CRy);
    unsigned n_crz = c->count_gates(OpType::CRz);
    unsigned n_2q = c->count_n_qubit_gates(2);
    REQUIRE(n_2q == n_cx + n_cry + n_crz);
    REQUIRE(n_cry <= 2);
    REQUIRE(n_crz <= 3);
    REQUIRE(n_2q <= 317);
  }
  GIVEN("controlled S") {
    Op_ptr op = get_op_ptr(OpType::S);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(4, 4);
    V(2, 2) = 1;
    V(3, 3) = i_;
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled V") {
    const double sq = 1 / std::sqrt(2.);
    Op_ptr op = get_op_ptr(OpType::V);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(4, 4);
    V(2, 2) = sq;
    V(2, 3) = sq * -i_;
    V(3, 2) = sq * -i_;
    V(3, 3) = sq;
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled SX") {
    Op_ptr op = get_op_ptr(OpType::SX);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(4, 4);
    V(2, 2) = 0.5 * (1. + i_);
    V(2, 3) = 0.5 * (1. - i_);
    V(3, 2) = 0.5 * (1. - i_);
    V(3, 3) = 0.5 * (1. + i_);
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled Sycamore") {
    Op_ptr op = get_op_ptr(OpType::Sycamore);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(8, 8);
    V(5, 5) = V(6, 6) = 0;
    V(5, 6) = V(6, 5) = -i_;
    V(7, 7) = exp(-i_ * PI / 6.);
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled TK2") {
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::TK2, {0.3, 0.4, 0.8}, {0, 1});
    Op_ptr op = c0.get_commands()[0].get_op_ptr();
    const Eigen::MatrixXcd U0 = as_gate_ptr(op)->get_unitary();
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(8, 8);
    for (unsigned i = 0; i < 4; i++) {
      for (unsigned j = 0; j < 4; j++) {
        V(4 + i, 4 + j) = U0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
  }
  GIVEN("2-controlled X") {
    Op_ptr op = get_op_ptr(OpType::X);
    QControlBox qcbox(op, 2);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    Circuit expected(3);
    expected.add_op<unsigned>(OpType::CCX, {0, 1, 2});
    REQUIRE(*c == expected);
  }
  GIVEN("controlled CircBox") {
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::H, {0});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    const Eigen::MatrixXcd U0 = tket_sim::get_unitary(c0);
    CircBox cbox(c0);
    Op_ptr op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(8, 8);
    for (unsigned i = 0; i < 4; i++) {
      for (unsigned j = 0; j < 4; j++) {
        V(4 + i, 4 + j) = U0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
  }

  GIVEN("controlled empty CircBox") {
    Circuit c0(2);
    c0.add_phase(0.3);
    const Eigen::MatrixXcd U0 = tket_sim::get_unitary(c0);
    CircBox cbox(c0);
    Op_ptr op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(8, 8);
    for (unsigned i = 0; i < 4; i++) {
      for (unsigned j = 0; j < 4; j++) {
        V(4 + i, 4 + j) = U0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
  }

  GIVEN("2-controlled CircBox") {
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::H, {0});
    c0.add_op<unsigned>(OpType::Rz, 0.5, {0});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    c0.add_op<unsigned>(OpType::Rz, 0.3, {0});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    // Should be reduced to a single 1-q unitary
    const Eigen::MatrixXcd U0 = tket_sim::get_unitary(c0);
    CircBox cbox(c0);
    Op_ptr op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(op, 2);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    // The 2-controlled 1-q unitary should be decomposed into 8 gates
    REQUIRE(c->n_gates() == 8);
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(16, 16);
    for (unsigned i = 0; i < 4; i++) {
      for (unsigned j = 0; j < 4; j++) {
        V(12 + i, 12 + j) = U0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled CircBox with gates merged") {
    Circuit c0(3);
    c0.add_op<unsigned>(OpType::X, {0});
    c0.add_op<unsigned>(OpType::CU1, 0.33, {0, 1});
    c0.add_op<unsigned>(OpType::T, {0});
    c0.add_op<unsigned>(OpType::CCX, {0, 1, 2});
    c0.add_op<unsigned>(OpType::CU1, -0.33, {1, 0});
    // This circuit can be reduced to XT[0] and CCX[0,1,2]
    const Eigen::MatrixXcd U0 = tket_sim::get_unitary(c0);
    CircBox cbox(c0);
    Op_ptr op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    // C(XT) should be translated into a U1 gate and a CU3 gate
    // CCX should become C3X
    std::vector<OpType> expected_optypes{OpType::U1, OpType::CU3, OpType::CnX};
    auto cmds = c->get_commands();
    REQUIRE(cmds.size() == 3);
    for (unsigned i = 0; i < expected_optypes.size(); ++i) {
      REQUIRE(cmds[i].get_op_ptr()->get_type() == expected_optypes[i]);
    }
    REQUIRE(equiv_0(c->get_phase()));
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(16, 16);
    for (unsigned i = 0; i < 8; i++) {
      for (unsigned j = 0; j < 8; j++) {
        V(8 + i, 8 + j) = U0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled CircBox with large n=6") {
    Circuit c0(3);
    c0.add_op<unsigned>(OpType::TK1, {0.55, 0.22, 0.98}, {0});
    c0.add_op<unsigned>(OpType::CZ, {0, 1});
    c0.add_op<unsigned>(OpType::X, {0});
    c0.add_op<unsigned>(OpType::CX, {1, 0});
    c0.add_op<unsigned>(OpType::Rx, 0.7, {0});
    const Eigen::MatrixXcd U0 = tket_sim::get_unitary(c0);
    CircBox cbox(c0);
    Op_ptr op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(op, 6);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(512, 512);
    for (unsigned i = 0; i < 8; i++) {
      for (unsigned j = 0; j < 8; j++) {
        V(504 + i, 504 + j) = U0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled CircBox with gates merged to identity") {
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::Z, {0});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    c0.add_op<unsigned>(OpType::X, {1});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    c0.add_op<unsigned>(OpType::X, {1});
    c0.add_op<unsigned>(OpType::CZ, {0, 1});
    c0.add_op<unsigned>(OpType::Z, {0});
    c0.add_op<unsigned>(OpType::CZ, {0, 1});
    const Eigen::MatrixXcd U0 = tket_sim::get_unitary(c0);
    REQUIRE(U0.isApprox(Eigen::Matrix4cd::Identity(), ERR_EPS));
    CircBox cbox(c0);
    Op_ptr op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    REQUIRE(c->n_gates() == 0);
    REQUIRE(equiv_0(c->get_phase()));
  }

  GIVEN("controlled Unitary1qBox") {
    Circuit c0(1);
    c0.add_op<unsigned>(OpType::TK1, {0.6, 0.7, 0.8}, {0});
    c0.add_phase(0.9);
    Eigen::Matrix2cd m0 = get_matrix_from_circ(c0);
    Unitary1qBox mbox(m0);
    Op_ptr op = std::make_shared<Unitary1qBox>(mbox);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(4, 4);
    for (unsigned i = 0; i < 2; i++) {
      for (unsigned j = 0; j < 2; j++) {
        V(2 + i, 2 + j) = m0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled Unitary2qBox") {
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::Rx, 0.2, {0});
    c0.add_op<unsigned>(OpType::Ry, 1.2, {1});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    c0.add_op<unsigned>(OpType::Rz, 0.4, {1});
    c0.add_op<unsigned>(OpType::H, {0});
    c0.add_op<unsigned>(OpType::CX, {1, 0});
    Eigen::Matrix4cd m0 = get_matrix_from_2qb_circ(c0);
    Unitary2qBox ubox(m0);
    Op_ptr op = std::make_shared<Unitary2qBox>(ubox);
    QControlBox qcbox(op);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(8, 8);
    for (unsigned i = 0; i < 4; i++) {
      for (unsigned j = 0; j < 4; j++) {
        V(4 + i, 4 + j) = m0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
  }
  GIVEN("2-controlled Unitary2qBox") {
    // https://cqc.atlassian.net/browse/TKET-1651
    Eigen::Matrix4cd M{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, -1}};
    Unitary2qBox ubox(M);
    Op_ptr op = std::make_shared<Unitary2qBox>(ubox);
    QControlBox qcbox(op, 2);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(16, 16);
    V(15, 15) = -1;
    REQUIRE(U.isApprox(V));
  }
  GIVEN("controlled symbolic operation") {
    Sym s = SymEngine::symbol("a");
    Expr a = Expr(s);
    Op_ptr op = get_op_ptr(OpType::Rx, a);
    QControlBox qcbox(op);
    Circuit c(*qcbox.to_circuit());
    double v = 0.125;
    double x = std::cos(0.5 * PI * v), y = std::sin(0.5 * PI * v);
    symbol_map_t map = {{s, v}};
    c.symbol_substitution(map);
    const Eigen::MatrixXcd U = tket_sim::get_unitary(c);
    Eigen::Matrix4cd V = Eigen::Matrix4cd::Identity();
    V(2, 2) = V(3, 3) = x;
    V(2, 3) = V(3, 2) = -i_ * y;
    REQUIRE(U.isApprox(V));
  }
  GIVEN("nested QControlBox") {
    Op_ptr op = get_op_ptr(OpType::S);
    QControlBox qcbox(op);
    Circuit c(2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_box(qcbox, {0, 1});
    const Eigen::MatrixXcd U = tket_sim::get_unitary(c);
    CircBox cbox(c);
    Op_ptr op1 = std::make_shared<CircBox>(cbox);
    QControlBox qcbox1(op1);
    std::shared_ptr<Circuit> c1 = qcbox1.to_circuit();
    const Eigen::MatrixXcd U1 = tket_sim::get_unitary(*c1);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(8, 8);
    for (unsigned i = 0; i < 4; i++) {
      for (unsigned j = 0; j < 4; j++) {
        V(4 + i, 4 + j) = U(i, j);
      }
    }
    REQUIRE(U1.isApprox(V));
  }
  GIVEN("controlled phase") {
    Op_ptr op = get_op_ptr(OpType::Phase, 0.25);
    QControlBox qcbox(op);
    Circuit c(1);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_box(qcbox, {0});
    std::shared_ptr<Circuit> c1 = qcbox.to_circuit();
    Eigen::MatrixXcd U1 = tket_sim::get_unitary(*c1);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(2, 2);
    V(1, 1) = std::exp(i_ * PI * 0.25);
    REQUIRE(U1.isApprox(V));
  }

  GIVEN("controlled CircBox with wire swaps") {
    Circuit c0(4);
    Sym s = SymEngine::symbol("a");
    Expr a = Expr(s);
    c0.add_op<unsigned>(OpType::TK1, {0.55, 0.22, a}, {0});
    c0.add_op<unsigned>(OpType::CZ, {0, 1});
    c0.add_op<unsigned>(OpType::X, {0});
    c0.add_op<unsigned>(OpType::CX, {1, 3});
    c0.add_op<unsigned>(OpType::Rx, 0.7, {0});
    c0.add_op<unsigned>(OpType::SWAP, {0, 1});
    c0.add_op<unsigned>(OpType::SWAP, {1, 2});
    REQUIRE(c0.replace_SWAPs());
    REQUIRE(c0.has_implicit_wireswaps());
    Circuit c0_numerical(c0);
    symbol_map_t map = {{s, 0.125}};
    c0_numerical.symbol_substitution(map);
    const Eigen::MatrixXcd U0 = tket_sim::get_unitary(c0_numerical);
    // Test symbolic decomp
    CircBox cbox(c0);
    Op_ptr op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(op, 1);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    c->symbol_substitution(map);
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(32, 32);
    for (unsigned i = 0; i < 16; i++) {
      for (unsigned j = 0; j < 16; j++) {
        V(16 + i, 16 + j) = U0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
    // Test numerical decomp
    CircBox cbox_numerical(c0_numerical);
    Op_ptr op2 = std::make_shared<CircBox>(cbox_numerical);
    QControlBox qcbox_numerical(op2, 1);
    std::shared_ptr<Circuit> c_numerical = qcbox_numerical.to_circuit();
    const Eigen::MatrixXcd U2 = tket_sim::get_unitary(*c_numerical);
    REQUIRE(U2.isApprox(V));
  }
  GIVEN("controlled CircBox with identity gates") {
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::TK1, {0., 0., 0.}, {0});
    c0.add_op<unsigned>(OpType::Rx, 0., {0});
    c0.add_op<unsigned>(OpType::CRx, 4., {0, 1});
    c0.add_op<unsigned>(OpType::noop, {0});
    CircBox cbox(c0);
    Op_ptr op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(op, 1);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    REQUIRE(c->n_gates() == 0);
    REQUIRE(equiv_0(c->get_phase()));
  }
  GIVEN("controlled gate that is identity up to a phase") {
    // phase = 1.
    Op_ptr op = get_op_ptr(OpType::U3, {2., 0.5, -0.5});
    QControlBox qcbox(op, 1);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    // Check the second qubit is empty
    Vertex q1_in = c->get_in(Qubit(1));
    EdgeVec q1_out_es = c->get_all_out_edges(q1_in);
    REQUIRE(q1_out_es.size() == 1);
    REQUIRE(c->target(q1_out_es[0]) == c->get_out(Qubit(1)));
    Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(4, 4);
    V(2, 2) = std::exp(i_ * PI);
    V(3, 3) = std::exp(i_ * PI);
    REQUIRE(U.isApprox(V));
  }

  GIVEN("symbolic circuit with barriers") {
    Sym s = SymEngine::symbol("a");
    Expr a = Expr(s);
    Circuit inner_c(1);
    inner_c.add_op<unsigned>(OpType::X, {0});
    inner_c.add_barrier(std::vector<unsigned>{0});
    inner_c.add_op<unsigned>(OpType::Ry, a, {0});
    CircBox cbox(inner_c);
    Op_ptr cbox_op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(cbox_op, 2);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    auto cmds = c->get_commands();
    REQUIRE(cmds.size() == 3);
    REQUIRE(cmds[0].get_op_ptr()->get_type() == OpType::CCX);
    REQUIRE(cmds[1].get_op_ptr()->get_type() == OpType::Barrier);
    unit_vector_t barrier_args{Qubit(2)};
    REQUIRE(cmds[1].get_args() == barrier_args);
    REQUIRE(cmds[2].get_op_ptr()->get_type() == OpType::CnRy);
  }

  GIVEN("numerical circuit with barriers") {
    Circuit inner_c(2);
    inner_c.add_op<unsigned>(OpType::X, {0});
    inner_c.add_barrier({0, 1});
    inner_c.add_op<unsigned>(OpType::Y, {0});
    inner_c.add_barrier(std::vector<unsigned>{1});
    inner_c.add_op<unsigned>(OpType::Z, {0});

    CircBox cbox(inner_c);
    Op_ptr cbox_op = std::make_shared<CircBox>(cbox);
    QControlBox qcbox(cbox_op, 2);
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    auto cmds = c->get_commands();
    // the circuit should contain a ccx
    // a barrier at {q[2], q[3]}, a barrier at q[3]
    // merged CC(Z*Y) decomposed (6 gates)
    REQUIRE(cmds.size() == 9);
    REQUIRE(cmds[0].get_op_ptr()->get_type() == OpType::CCX);
    REQUIRE(cmds[1].get_op_ptr()->get_type() == OpType::Barrier);
    unit_vector_t barrier_args{Qubit(2), Qubit(3)};
    REQUIRE(cmds[1].get_args() == barrier_args);
    auto barrier_cmds = c->get_commands_of_type(OpType::Barrier);
    REQUIRE(barrier_cmds.size() == 2);
    auto it = barrier_cmds.begin();
    it++;
    unit_vector_t barrier_args2{Qubit(3)};
    REQUIRE(it->get_args() == barrier_args2);
  }
  GIVEN("Unitary2qBox controlled by state") {
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::Rx, 0.2, {0});
    c0.add_op<unsigned>(OpType::Ry, 1.2, {1});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    c0.add_op<unsigned>(OpType::Rz, 0.4, {1});
    c0.add_op<unsigned>(OpType::H, {0});
    c0.add_op<unsigned>(OpType::CX, {1, 0});
    Eigen::Matrix4cd m0 = get_matrix_from_2qb_circ(c0);
    Unitary2qBox ubox(m0);
    Op_ptr op = std::make_shared<Unitary2qBox>(ubox);
    QControlBox qcbox(op, 2, {0, 1});
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd U = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(16, 16);
    for (unsigned i = 0; i < 4; i++) {
      for (unsigned j = 0; j < 4; j++) {
        V(4 + i, 4 + j) = m0(i, j);
      }
    }
    REQUIRE(U.isApprox(V));
    // check get_box_unitary is correct
    std::optional<Eigen::MatrixXcd> box_u = qcbox.get_box_unitary();
    REQUIRE(V.isApprox(box_u.value()));
  }
  GIVEN("Random unitary box controlled by state") {
    Eigen::MatrixXcd u = random_unitary(8, 1);
    Unitary3qBox ubox(u);
    Op_ptr op = std::make_shared<Unitary3qBox>(ubox);
    QControlBox qcbox(op, 3, {1, 0, 0});
    std::shared_ptr<Circuit> c = qcbox.to_circuit();
    const Eigen::MatrixXcd circ_u = tket_sim::get_unitary(*c);
    Eigen::MatrixXcd V = Eigen::MatrixXcd::Identity(64, 64);
    for (unsigned i = 0; i < 8; i++) {
      for (unsigned j = 0; j < 8; j++) {
        V(32 + i, 32 + j) = u(i, j);
      }
    }
    REQUIRE(circ_u.isApprox(V));
    // check get_box_unitary is correct
    std::optional<Eigen::MatrixXcd> box_u = qcbox.get_box_unitary();
    REQUIRE(V.isApprox(box_u.value()));
  }
  GIVEN("numerical circuit with a ConjugationBox") {
    Circuit compute(2);
    Circuit action(2);
    compute.add_op<unsigned>(OpType::CX, {1, 0});
    action.add_op<unsigned>(OpType::Z, {0});
    Op_ptr compute_op = std::make_shared<CircBox>(compute);
    Op_ptr action_op = std::make_shared<CircBox>(action);
    Op_ptr cj_op =
        std::make_shared<ConjugationBox>(ConjugationBox(compute_op, action_op));
    WHEN("Simple") {
      QControlBox qbox(cj_op);
      std::shared_ptr<Circuit> c = qbox.to_circuit();
      Circuit d(3);
      d.add_op<unsigned>(OpType::CX, {2, 1});
      d.add_op<unsigned>(OpType::CZ, {0, 1});
      d.add_op<unsigned>(OpType::CX, {2, 1});
      REQUIRE(*c == d);
    }
    WHEN("Wrapped in a CircBox") {
      Circuit inner_circ(4);
      inner_circ.add_op<unsigned>(cj_op, {1, 3});
      Op_ptr circbox_op = std::make_shared<CircBox>(inner_circ);
      QControlBox qbox(circbox_op);
      std::shared_ptr<Circuit> c = qbox.to_circuit();
      Circuit d(5);
      d.add_op<unsigned>(OpType::CX, {4, 2});
      d.add_op<unsigned>(OpType::CZ, {0, 2});
      d.add_op<unsigned>(OpType::CX, {4, 2});
      REQUIRE(*c == d);
    }
    WHEN("Nested") {
      Circuit compute_outer(3);
      compute_outer.add_op<unsigned>(OpType::CX, {2, 1});
      Circuit action_outer(3);
      action_outer.add_op<unsigned>(cj_op, {1, 0});
      Op_ptr compute_outer_op = std::make_shared<CircBox>(compute_outer);
      Op_ptr action_outer_op = std::make_shared<CircBox>(action_outer);
      Op_ptr cj_op_2 = std::make_shared<ConjugationBox>(
          ConjugationBox(compute_outer_op, action_outer_op));
      QControlBox qbox(cj_op_2);
      std::shared_ptr<Circuit> c = qbox.to_circuit();
      Circuit d(4);
      d.add_op<unsigned>(OpType::CX, {3, 2});
      d.add_op<unsigned>(OpType::CX, {1, 2});
      d.add_op<unsigned>(OpType::CZ, {0, 2});
      d.add_op<unsigned>(OpType::CX, {1, 2});
      d.add_op<unsigned>(OpType::CX, {3, 2});
      REQUIRE(*c == d);
    }
  }
  GIVEN("symbolic circuit with a ConjugationBox") {
    Sym s = SymEngine::symbol("a");
    Expr a = Expr(s);
    Sym s1 = SymEngine::symbol("b");
    Expr b = Expr(s1);
    Circuit compute(1);
    Circuit action(1);
    Circuit uncompute(1);
    compute.add_op<unsigned>(OpType::Rx, a, {0});
    action.add_op<unsigned>(OpType::Z, {0});
    uncompute.add_op<unsigned>(OpType::Rx, b, {0});
    Op_ptr compute_op = std::make_shared<CircBox>(compute);
    Op_ptr action_op = std::make_shared<CircBox>(action);
    Op_ptr uncompute_op = std::make_shared<CircBox>(uncompute);
    Op_ptr cj_op = std::make_shared<ConjugationBox>(
        ConjugationBox(compute_op, action_op, uncompute_op));
    QControlBox qbox(cj_op);
    std::shared_ptr<Circuit> c = qbox.to_circuit();
    Circuit d(2);
    d.add_op<unsigned>(OpType::Rx, a, {1});
    d.add_op<unsigned>(OpType::CZ, {0, 1});
    d.add_op<unsigned>(OpType::Rx, b, {1});
    REQUIRE(*c == d);
  }

  GIVEN("controlled phase_gadget, numerical") {
    Expr a;
    a = 0.3;
    QControlBox qbox(get_op_ptr(OpType::PhaseGadget, {a}, 2));
    std::shared_ptr<Circuit> c = qbox.to_circuit();
    REQUIRE(c->count_gates(OpType::CX) == 4);
  }
  GIVEN("controlled phase_gadget, symbolic") {
    Expr a;
    Sym s = SymEngine::symbol("a");
    a = Expr(s);
    QControlBox qbox(get_op_ptr(OpType::PhaseGadget, {a}, 2));
    std::shared_ptr<Circuit> c = qbox.to_circuit();
    REQUIRE(c->count_gates(OpType::CX) == 2);
    REQUIRE(c->count_gates(OpType::CRz) == 1);
  }
  GIVEN("controlled PauliExpBox") {
    // https://github.com/quantinuum/tket/issues/1109
    PauliExpBox pbox(
        SymPauliTensor({Pauli::I, Pauli::Z, Pauli::I, Pauli::I}, 0.7));
    Op_ptr op = std::make_shared<PauliExpBox>(pbox);
    QControlBox qbox(op);
    std::shared_ptr<Circuit> c = qbox.to_circuit();
    // construct the expected circuit
    Circuit correct_inner(4);
    correct_inner.add_op<unsigned>(OpType::Rz, 0.7, {1});
    QControlBox correct_qbox(std::make_shared<CircBox>(correct_inner));
    std::shared_ptr<Circuit> d = correct_qbox.to_circuit();
    REQUIRE(*c == *d);
  }
}

SCENARIO("Unitary3qBox", "[boxes]") {
  GIVEN("An 8x8 permutation matrix") {
    Eigen::MatrixXcd U = Eigen::MatrixXcd::Zero(8, 8);
    U(0, 3) = 1;
    U(1, 1) = 1;
    U(2, 7) = 1;
    U(3, 5) = 1;
    U(4, 0) = 1;
    U(5, 4) = 1;
    U(6, 2) = 1;
    U(7, 6) = 1;
    Unitary3qBox ubox(U);
    std::shared_ptr<const Circuit> c = ubox.to_circuit();
    REQUIRE(c->count_gates(OpType::CX) <= 24);
    Eigen::MatrixXcd U1 = tket_sim::get_unitary(*c);
    REQUIRE(U1.isApprox(U));
  }
}

SCENARIO("Checking equality", "[boxes]") {
  GIVEN("Some different types") {
    Circuit u(2);
    u.add_op<unsigned>(OpType::Rz, -0.75, {0});
    u.add_op<unsigned>(OpType::CX, {0, 1});
    const CircBox ubox(u);
    Eigen::Matrix4cd m = tket_sim::get_unitary(u);
    Unitary2qBox mbox(m);
    REQUIRE(ubox != mbox);

    Op_ptr op1 = get_op_ptr(OpType::X);
    Op_ptr op2 = get_op_ptr(OpType::Z);
    REQUIRE(op1 == op1);
    REQUIRE(op1 != op2);

    Eigen::Matrix4cd A;
    A << 0., 1., 2., 3., 1., 2., 3. * i_, 4., 2., -3. * i_, 3, 2. - 3. * i_, 3.,
        4., 2. + 3. * i_, 5.;
    ExpBox ebox(A, -0.5);
    REQUIRE(ebox != mbox);

    PhasePolyBox ppbox(u);
    REQUIRE(ppbox != mbox);
  }
  GIVEN("CircBoxes") {
    Circuit u(2);
    u.add_op<unsigned>(OpType::Ry, -0.75, {0});
    u.add_op<unsigned>(OpType::CX, {0, 1});
    const CircBox ubox(u);

    Circuit u2(2);
    u2.add_op<unsigned>(OpType::Ry, -0.35, {0});
    u2.add_op<unsigned>(OpType::CX, {0, 1});
    const CircBox ubox2(u2);

    WHEN("both arguments are equal") { REQUIRE(ubox == ubox); }
    WHEN("different ids but equivalent inner circuits") {
      REQUIRE(ubox == CircBox(u));
    }
    WHEN("different inner circuits") { REQUIRE(ubox != ubox2); }
  }
  GIVEN("Unitary1qBox") {
    Circuit setup(1);
    setup.add_op<unsigned>(OpType::TK1, {0.2374, 1.0353, 0.5372}, {0});
    Eigen::Matrix2cd m = tket_sim::get_unitary(setup);
    Unitary1qBox mbox(m);

    WHEN("both arguments are equal") { REQUIRE(mbox == mbox); }
    WHEN("different ids but matrices are equal") {
      Eigen::Matrix2cd m2 = tket_sim::get_unitary(setup);
      Unitary1qBox mbox2(m2);
      REQUIRE(mbox == mbox2);
    }
    WHEN("both arguments are different") {
      setup.add_op<unsigned>(OpType::TK1, {0.2374, 1.0353, 0.5372}, {0});
      Eigen::Matrix2cd m3 = tket_sim::get_unitary(setup);
      Unitary1qBox mbox3(m3);
      REQUIRE(mbox != mbox3);
    }
  }
  GIVEN("Unitary2qBox") {
    Circuit setup(2);
    setup.add_op<unsigned>(OpType::TK1, {0.2374, 1.0353, 0.5372}, {0});
    setup.add_op<unsigned>(OpType::CX, {0, 1});
    Eigen::Matrix4cd m = tket_sim::get_unitary(setup);
    Unitary2qBox mbox(m);

    WHEN("both arguments are equal") { REQUIRE(mbox == mbox); }
    WHEN("different ids but matrices are equal") {
      Eigen::Matrix4cd m2 = tket_sim::get_unitary(setup);
      Unitary2qBox mbox2(m2);
      REQUIRE(mbox == mbox2);
    }
    WHEN("both arguments are different") {
      setup.add_op<unsigned>(OpType::CX, {1, 0});
      Eigen::Matrix4cd m3 = tket_sim::get_unitary(setup);
      Unitary2qBox mbox3(m3);
      REQUIRE(mbox != mbox3);
    }
  }
  GIVEN("Unitary3qBox") {
    Circuit setup(3);
    setup.add_op<unsigned>(OpType::TK1, {0.2374, 1.0353, 0.5372}, {0});
    setup.add_op<unsigned>(OpType::CX, {0, 1});
    setup.add_op<unsigned>(OpType::CX, {1, 2});
    Eigen::MatrixXcd m = tket_sim::get_unitary(setup);
    Unitary3qBox mbox(m);

    WHEN("both arguments are equal") { REQUIRE(mbox == mbox); }
    WHEN("different ids but matrices are equal") {
      Eigen::MatrixXcd m2 = tket_sim::get_unitary(setup);
      Unitary3qBox mbox2(m2);
      REQUIRE(mbox == mbox2);
    }
    WHEN("both arguments are different") {
      setup.add_op<unsigned>(OpType::CX, {0, 2});
      Eigen::MatrixXcd m3 = tket_sim::get_unitary(setup);
      Unitary3qBox mbox3(m3);
      REQUIRE(mbox != mbox3);
    }
  }
  GIVEN("ExpBox") {
    // random hermitian matrix
    Eigen::Matrix4cd A;
    A << 0., 1., 2., 3., 1., 2., 3. * i_, 4., 2., -3. * i_, 3, 2. - 3. * i_, 3.,
        4., 2. + 3. * i_, 5.;
    ExpBox ebox(A, -0.5);
    WHEN("both arguments are equal") { REQUIRE(ebox == ebox); }
    WHEN("different ids but matrices are equal") {
      ExpBox ebox2(A, -0.5);
      REQUIRE(ebox == ebox2);
    }
    WHEN("both arguments are different") {
      ExpBox ebox3(A, -0.2);
      REQUIRE(ebox != ebox3);
    }
  }
  GIVEN("Pauli gadgets") {
    double t = 1.687029013593215;
    PauliExpBox pbox(SymPauliTensor({Pauli::X}, t));
    WHEN("both arguments are equal") { REQUIRE(pbox == pbox); }
    WHEN("both arguments are different") {
      PauliExpBox pbox2(SymPauliTensor({Pauli::Y}, t));
      REQUIRE(pbox != pbox2);
    }
  }
  GIVEN("QControlBox") {
    Circuit u(2);
    u.add_op<unsigned>(OpType::CX, {0, 1});
    Op_ptr op = std::make_shared<CircBox>(u);
    QControlBox qcbox(op, 3, {1, 0, 1});
    WHEN("both arguments are equal") { REQUIRE(qcbox == qcbox); }
    WHEN("different ids but equivalent ops") {
      Circuit u2(2);
      u2.add_op<unsigned>(OpType::CX, {0, 1});
      Op_ptr op2 = std::make_shared<CircBox>(u2);
      QControlBox qcbox2(op2, 3, {1, 0, 1});
      REQUIRE(qcbox == qcbox2);
    }
    WHEN("different ids, equivalent ops, but different types") {
      Op_ptr op3 = get_op_ptr(OpType::CX);
      REQUIRE(qcbox != QControlBox(op3, 3, {1, 0, 1}));
    }
    WHEN("different control states") {
      REQUIRE(qcbox != QControlBox(op, 3, {0, 0, 1}));
    }
    WHEN("equivalent control states") {
      REQUIRE(QControlBox(op, 3) == QControlBox(op, 3, {1, 1, 1}));
    }
    WHEN("all arguments are different") {
      Op_ptr op4 = get_op_ptr(OpType::Y);
      QControlBox qcbox4(op4);
      REQUIRE(qcbox != qcbox4);
    }
  }
  GIVEN("PhasePolyBox") {
    Circuit u(2);
    u.add_op<unsigned>(OpType::Rz, -0.75, {0});
    u.add_op<unsigned>(OpType::CX, {0, 1});
    PhasePolyBox ppbox(u);
    WHEN("both arguments are equal") { REQUIRE(ppbox == ppbox); }
    WHEN("both arguments are different") {
      u.add_op<unsigned>(OpType::CX, {1, 0});
      PhasePolyBox ppbox2(u);
      REQUIRE(ppbox != ppbox2);
    }
  }
  GIVEN("CustomGate") {
    Circuit setup(1);
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);

    // "random" 1qb gate.
    const double param1 = 1.23323;
    const double param2 = 0.42323;
    const double param3 = 0.34212;
    const std::string name1{"gate name1"};
    const std::string name2{"gate name2"};
    setup.add_op<unsigned>(OpType::TK1, {ea, param1, param2}, {0});

    composite_def_ptr_t def1 = CompositeGateDef::define_gate(name1, setup, {a});
    composite_def_ptr_t def2 = CompositeGateDef::define_gate(name2, setup, {a});
    const CustomGate g1(def1, {param3});
    const CustomGate g1_repeated(def1, {param3});
    const CustomGate g1_wrong(def1, {param1});
    const CustomGate g2(def2, {param3});

    // Check that all IDs are different.
    const std::set<boost::uuids::uuid> ids{
        g1.get_id(), g1_repeated.get_id(), g1_wrong.get_id(), g2.get_id()};
    CHECK(ids.size() == 4);
    CHECK(g1 == g1);
    CHECK(g1 == g1_repeated);
    CHECK(g1 != g2);
    CHECK(g1 != g1_wrong);
    CHECK(g1_repeated != g1_wrong);
    CHECK_THROWS_AS(CustomGate(nullptr, {param3}), std::runtime_error);
  }
  GIVEN("ProjectorAssertionBox") {
    Eigen::MatrixXcd bell(4, 4);
    bell << 0.5, 0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, 0.5, 0, 0, 0.5;
    ProjectorAssertionBox box(bell);
    WHEN("both arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but equivalent projectors") {
      REQUIRE(box == ProjectorAssertionBox(bell));
    }
    WHEN("different projectors") {
      Eigen::MatrixXcd p(4, 4);
      p << 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;
      REQUIRE(box != ProjectorAssertionBox(p));
    }
  }
  GIVEN("StabiliserAssertionBox") {
    PauliStabiliser p1 = {{Pauli::X, Pauli::X}, 0};
    PauliStabiliser p2 = {{Pauli::Z, Pauli::Z}, 0};
    PauliStabiliser p3 = {{Pauli::Z, Pauli::Z}, 2};
    StabiliserAssertionBox box({p1, p2});
    WHEN("both arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but equivalent stabilisers") {
      REQUIRE(box == StabiliserAssertionBox({p1, p2}));
    }
    WHEN("different stabilisers") {
      REQUIRE(box != StabiliserAssertionBox({p1, p3}));
    }
  }
  GIVEN("DiagonalBox") {
    Eigen::Vector2cd diag(i_, 1);
    DiagonalBox box(diag);
    WHEN("all arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but other args are equal") {
      DiagonalBox box2(diag);
      REQUIRE(box == box2);
    }
    WHEN("arguments are different") {
      DiagonalBox box3(diag, false);
      REQUIRE(box != box3);
    }
  }
  GIVEN("MultiplexorBox") {
    ctrl_op_map_t op_map = {{{1}, get_op_ptr(OpType::H)}};
    MultiplexorBox box(op_map);
    WHEN("all arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but other args are equal") {
      ctrl_op_map_t op_map2 = {{{1}, get_op_ptr(OpType::H)}};
      MultiplexorBox box2(op_map2);
      REQUIRE(box == box2);
    }
    WHEN("arguments are different") {
      ctrl_op_map_t op_map2 = {{{0}, get_op_ptr(OpType::H)}};
      MultiplexorBox box3(op_map2);
      REQUIRE(box != box3);
    }
  }
  GIVEN("MultiplexedRotationBox") {
    ctrl_op_map_t op_map = {{{1}, get_op_ptr(OpType::Rz, 0.7)}};
    MultiplexedRotationBox box(op_map);
    WHEN("all arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but other args are equal") {
      ctrl_op_map_t op_map2 = {{{1}, get_op_ptr(OpType::Rz, 0.7)}};
      MultiplexedRotationBox box2(op_map2);
      REQUIRE(box == box2);
    }
    WHEN("arguments are different") {
      ctrl_op_map_t op_map2 = {{{0}, get_op_ptr(OpType::Rz, 0.7)}};
      MultiplexedRotationBox box3(op_map2);
      REQUIRE(box != box3);
    }
  }
  GIVEN("MultiplexedU2Box") {
    ctrl_op_map_t op_map = {{{1}, get_op_ptr(OpType::H)}};
    MultiplexedU2Box box(op_map);
    WHEN("all arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but other args are equal") {
      ctrl_op_map_t op_map2 = {{{1}, get_op_ptr(OpType::H)}};
      MultiplexedU2Box box2(op_map2);
      REQUIRE(box == box2);
    }
    WHEN("arguments are different") {
      MultiplexedU2Box box3(op_map, false);
      REQUIRE(box != box3);
    }
  }
  GIVEN("MultiplexedTensoredU2Box") {
    ctrl_tensored_op_map_t op_map;
    op_map.insert({{0, 0}, {get_op_ptr(OpType::X), get_op_ptr(OpType::X)}});
    MultiplexedTensoredU2Box box(op_map);
    WHEN("all arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but other args are equal") {
      ctrl_tensored_op_map_t op_map2;
      op_map2.insert({{0, 0}, {get_op_ptr(OpType::X), get_op_ptr(OpType::X)}});
      MultiplexedTensoredU2Box box2(op_map2);
      REQUIRE(box == box2);
    }
    WHEN("arguments are different") {
      op_map.insert({{0, 1}, {get_op_ptr(OpType::H), get_op_ptr(OpType::X)}});
      MultiplexedTensoredU2Box box3(op_map);
      REQUIRE(box != box3);
    }
  }
  GIVEN("StatePreparationBox") {
    Eigen::Vector2cd diag(0, 1);
    StatePreparationBox box(diag);
    WHEN("all arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but other args are equal") {
      StatePreparationBox box2(diag);
      REQUIRE(box == box2);
    }
    WHEN("arguments are different") {
      StatePreparationBox box3(diag, false, true);
      REQUIRE(box != box3);
    }
  }
  GIVEN("ToffoliBox") {
    state_perm_t perm;
    perm[{0, 1}] = {1, 0};
    perm[{1, 0}] = {0, 1};
    ToffoliBox box(perm);
    WHEN("all arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but other args are equal") {
      ToffoliBox box2(perm);
      REQUIRE(box == box2);
    }
    WHEN("permutations are equivalent") {
      state_perm_t perm2;
      perm2[{0, 1}] = {1, 0};
      perm2[{1, 0}] = {0, 1};
      perm2[{1, 1}] = {1, 1};
      ToffoliBox box2(perm2);
      REQUIRE(box == box2);
    }
    WHEN("permutations are not equivalent") {
      state_perm_t perm2;
      perm2[{0, 1}] = {1, 0};
      perm2[{1, 0}] = {0, 1};
      perm2[{1, 1}] = {0, 0};
      perm2[{0, 0}] = {1, 1};
      ToffoliBox box2(perm2);
      REQUIRE(box != box2);
    }
    WHEN("permutations are not equivalent case 2") {
      state_perm_t perm2;
      perm2[{1, 1}] = {0, 0};
      perm2[{0, 0}] = {1, 1};
      ToffoliBox box2(perm2);
      REQUIRE(box != box2);
    }
    WHEN("arguments are different") {
      ToffoliBox box2(perm, ToffoliBoxSynthStrat::Cycle);
      REQUIRE(box != box2);
    }
  }
  GIVEN("PauliExpBox") {
    PauliExpBox pbox(SymPauliTensor({Pauli::X, Pauli::Y, Pauli::Z}, 0.8));
    WHEN("both arguments are equal") { REQUIRE(pbox == pbox); }
    WHEN("different ids but same arguments") {
      REQUIRE(
          pbox ==
          PauliExpBox(SymPauliTensor({Pauli::X, Pauli::Y, Pauli::Z}, 0.8)));
    }
    WHEN("different ids, equivalent angle") {
      REQUIRE(
          pbox ==
          PauliExpBox(SymPauliTensor({Pauli::X, Pauli::Y, Pauli::Z}, 4.8)));
    }
    WHEN("different arguments") {
      REQUIRE(
          pbox !=
          PauliExpBox(SymPauliTensor({Pauli::X, Pauli::Y, Pauli::Z}, 0.9)));
    }
  }
  GIVEN("PauliExpPairBox") {
    PauliExpPairBox pbox(
        SymPauliTensor({Pauli::X}, 1.0), SymPauliTensor({Pauli::I}, 0.0));
    WHEN("both arguments are equal") { REQUIRE(pbox == pbox); }
    WHEN("different ids but same arguments") {
      REQUIRE(
          pbox == PauliExpPairBox(
                      SymPauliTensor({Pauli::X}, 1.0),
                      SymPauliTensor({Pauli::I}, 0.0)));
    }
    WHEN("different ids, equivalent angle") {
      REQUIRE(
          pbox == PauliExpPairBox(
                      SymPauliTensor({Pauli::X}, 1.0),
                      SymPauliTensor({Pauli::I}, 4.0)));
    }
    WHEN("different arguments") {
      REQUIRE(
          pbox != PauliExpPairBox(
                      SymPauliTensor({Pauli::X}, -1.0),
                      SymPauliTensor({Pauli::I}, 0.0)));
    }
  }
  GIVEN("PauliExpCommutingSetBox") {
    PauliExpCommutingSetBox pbox(
        {SymPauliTensor({Pauli::X}, 1.0), SymPauliTensor({Pauli::I}, 1.2),
         SymPauliTensor({Pauli::I}, -0.5)});
    WHEN("both arguments are equal") { REQUIRE(pbox == pbox); }
    WHEN("different ids but same arguments") {
      REQUIRE(
          pbox ==
          PauliExpCommutingSetBox(
              {SymPauliTensor({Pauli::X}, 1.0), SymPauliTensor({Pauli::I}, 1.2),
               SymPauliTensor({Pauli::I}, -0.5)}));
    }
    WHEN("different ids, equivalent angles") {
      REQUIRE(
          pbox == PauliExpCommutingSetBox(
                      {SymPauliTensor({Pauli::X}, -3.0),
                       SymPauliTensor({Pauli::I}, 5.2),
                       SymPauliTensor({Pauli::I}, -0.5)}));
    }
    WHEN("different arguments") {
      REQUIRE(
          pbox !=
          PauliExpCommutingSetBox(
              {SymPauliTensor({Pauli::Y}, 1.0), SymPauliTensor({Pauli::I}, 1.2),
               SymPauliTensor({Pauli::I}, -0.5)}));
    }
  }
  GIVEN("TermSequenceBox") {
    TermSequenceBox tbox(
        {SymPauliTensor({Pauli::X}, 1.0), SymPauliTensor({Pauli::I}, 1.2),
         SymPauliTensor({Pauli::I}, -0.5)});
    WHEN("both arguments are equal") { REQUIRE(tbox == tbox); }
    WHEN("different ids but same arguments") {
      REQUIRE(
          tbox ==
          TermSequenceBox(
              {SymPauliTensor({Pauli::X}, 1.0), SymPauliTensor({Pauli::I}, 1.2),
               SymPauliTensor({Pauli::I}, -0.5)}));
    }
    WHEN("different ids, equivalent angles") {
      REQUIRE(
          tbox == TermSequenceBox(
                      {SymPauliTensor({Pauli::X}, -3.0),
                       SymPauliTensor({Pauli::I}, 5.2),
                       SymPauliTensor({Pauli::I}, -0.5)}));
    }
    WHEN("different arguments") {
      REQUIRE(
          tbox !=
          TermSequenceBox(
              {SymPauliTensor({Pauli::Y}, 1.0), SymPauliTensor({Pauli::I}, 1.2),
               SymPauliTensor({Pauli::I}, -0.5)}));
    }
  }
  GIVEN("ConjugationBox") {
    Circuit compute(2);
    compute.add_op<unsigned>(OpType::CRx, 0.5, {1, 0});
    Op_ptr compute_op = std::make_shared<CircBox>(compute);
    Circuit action(2);
    action.add_op<unsigned>(OpType::H, {0});
    Op_ptr action_op = std::make_shared<CircBox>(action);
    ConjugationBox box(compute_op, action_op);
    WHEN("all arguments are equal") { REQUIRE(box == box); }
    WHEN("different ids but equivalent ops") {
      REQUIRE(box == ConjugationBox(compute_op, action_op));
    }
    WHEN("different uncompute") {
      Circuit uncompute(2);
      uncompute.add_op<unsigned>(OpType::CZ, {0, 1});
      Op_ptr uncompute_op = std::make_shared<CircBox>(uncompute);
      REQUIRE(box != ConjugationBox(compute_op, action_op, uncompute_op));
    }
    WHEN("equivalent uncompute") {
      REQUIRE(
          box == ConjugationBox(compute_op, action_op, compute_op->dagger()));
      REQUIRE(
          ConjugationBox(compute_op, action_op, compute_op->dagger()) ==
          ConjugationBox(compute_op, action_op));
    }
    WHEN("different args") {
      Circuit compute_2(2);
      compute_2.add_op<unsigned>(OpType::CZ, {0, 1});
      Op_ptr compute_2_op = std::make_shared<CircBox>(compute_2);
      REQUIRE(box != ConjugationBox(compute_2_op, action_op));
    }
  }
  GIVEN("ConjugationBox with symbols") {
    Sym asym = SymTable::fresh_symbol("a");
    Expr a(asym);
    Sym bsym = SymTable::fresh_symbol("b");
    Expr b(bsym);
    Sym csym = SymTable::fresh_symbol("c");
    Expr c(csym);
    Op_ptr compute = get_op_ptr(OpType::Rx, {a});
    Op_ptr action = get_op_ptr(OpType::Rz, {b});
    Op_ptr uncompute = get_op_ptr(OpType::Rx, {c});
    ConjugationBox box0(compute, action);
    ConjugationBox box1(compute, action, uncompute);
    SymSet sym_set0 = box0.free_symbols();
    CHECK(sym_set0.size() == 2);
    SymSet sym_set1 = box1.free_symbols();
    CHECK(sym_set1.size() == 3);
    SymEngine::map_basic_basic sym_map;
    sym_map[asym] = Expr(0.25);
    sym_map[bsym] = Expr(0.75);
    sym_map[csym] = Expr(-0.25);
    Op_ptr sub_box0 = box0.symbol_substitution(sym_map);
    CHECK(sub_box0->free_symbols().empty());
    Op_ptr sub_box1 = box1.symbol_substitution(sym_map);
    CHECK(sub_box1->free_symbols().empty());
  }
}

SCENARIO("Checking box names", "[boxes]") {
  GIVEN("CustomGate without parameters") {
    Circuit setup(1);
    setup.add_op<unsigned>(OpType::TK1, {0.3333, 1.111, 0.5555}, {0});
    const std::string name("gate without params");
    composite_def_ptr_t def = CompositeGateDef::define_gate(name, setup, {});
    CustomGate g(def, {});
    CHECK(g.get_name() == name);
  }
  GIVEN("CustomGate with 1 parameter") {
    Circuit setup(1);
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);
    setup.add_op<unsigned>(OpType::TK1, {ea, 0.3333, 1.111}, {0});
    const std::string prefix("gate with params");
    composite_def_ptr_t def = CompositeGateDef::define_gate(prefix, setup, {a});
    CustomGate g(def, {0.4444});

    // Of course, 0.4444 is NOT exactly represented by a double,
    // so it might print something like 0.4443999... or 0.4440000...1.
    // This test will still pass even if so.
    CHECK_THAT(g.get_name(), StartsWith(prefix + "(0.444"));
  }
  GIVEN("CustomGate with 3 parameters") {
    Circuit setup(1);
    Sym a = SymTable::fresh_symbol("a");
    Sym b = SymTable::fresh_symbol("b");
    Sym c = SymTable::fresh_symbol("c");
    Expr ea(a);
    Expr eb(b);
    Expr ec(c);
    setup.add_op<unsigned>(OpType::TK1, {ea, eb, ec}, {0});
    const std::string prefix("gate with 3 params");
    composite_def_ptr_t def =
        CompositeGateDef::define_gate(prefix, setup, {a, b, c});
    CustomGate g(def, {0.1111, 0.2222, 0.4444});
    const std::string name = g.get_name();
    CHECK(name == "gate with 3 params(0.1111,0.2222,0.4444)");
  }
}

SCENARIO("Unitaries") {
  GIVEN("Unitary1qBox") {
    Eigen::Matrix2cd u = random_unitary(2, 1);
    Unitary1qBox ubox(u);
    Circuit c(1);
    c.add_box(ubox, {0});
    Eigen::MatrixXcd u1 = tket_sim::get_unitary(c);
    CHECK(u1.isApprox(u));
  }

  GIVEN("Unitary2qBox") {
    Eigen::Matrix4cd u = random_unitary(4, 1);
    Unitary2qBox ubox(u);
    Circuit c(2);
    c.add_box(ubox, {0, 1});
    Eigen::MatrixXcd u1 = tket_sim::get_unitary(c);
    CHECK(u1.isApprox(u));
  }

  GIVEN("Unitary3qBox") {
    Eigen::MatrixXcd u = random_unitary(8, 1);
    Unitary3qBox ubox(u);
    Circuit c(3);
    c.add_box(ubox, {0, 1, 2});
    Eigen::MatrixXcd u1 = tket_sim::get_unitary(c);
    CHECK(u1.isApprox(u));
  }

  GIVEN("CircBox") {
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::H, {0});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    Eigen::MatrixXcd u = tket_sim::get_unitary(c0);
    CircBox cbox(c0);
    Circuit c(2);
    c.add_box(cbox, {0, 1});
    Eigen::MatrixXcd u1 = tket_sim::get_unitary(c);
    CHECK(u1.isApprox(u));
  }

  GIVEN("ExpBox") {
    Eigen::Matrix4cd A;
    A << 0., 1., 2., 3., 1., 2., 3. * i_, 4., 2., -3. * i_, 3, 2. - 3. * i_, 3.,
        4., 2. + 3. * i_, 5.;
    const double t = 0.7;
    Eigen::MatrixXcd u = (i_ * t * A).exp();
    ExpBox ebox(A, t);
    Circuit c(2);
    c.add_box(ebox, {0, 1});
    Eigen::MatrixXcd u1 = tket_sim::get_unitary(c);
    CHECK(u1.isApprox(u));
  }

  GIVEN("QControlBox") {
    Op_ptr op = get_op_ptr(OpType::H);
    QControlBox qcbox(op, 2);
    Circuit c(3);
    c.add_box(qcbox, {0, 1, 2});
    Eigen::MatrixXcd u = tket_sim::get_unitary(c);
    CHECK(u.topLeftCorner(6, 6).isApprox(Eigen::MatrixXcd::Identity(6, 6)));
    CHECK(u.bottomRightCorner(2, 2).isApprox(op->get_unitary()));
  }

  GIVEN("ToffoliBox") {
    state_perm_t p;
    p[{0, 1, 1}] = {0, 0, 1};
    p[{0, 0, 1}] = {1, 1, 0};
    p[{1, 1, 0}] = {0, 1, 1};
    ToffoliBox tbox(p);
    Circuit c(3);
    c.add_box(tbox, {0, 1, 2});
    Eigen::MatrixXcd u = tket_sim::get_unitary(c);
    CHECK(std::abs(u(0, 0) - 1.) < ERR_EPS);
    CHECK(std::abs(u(1, 3) - 1.) < ERR_EPS);
    CHECK(std::abs(u(2, 2) - 1.) < ERR_EPS);
    CHECK(std::abs(u(3, 6) - 1.) < ERR_EPS);
    CHECK(std::abs(u(4, 4) - 1.) < ERR_EPS);
    CHECK(std::abs(u(5, 5) - 1.) < ERR_EPS);
    CHECK(std::abs(u(6, 1) - 1.) < ERR_EPS);
    CHECK(std::abs(u(7, 7) - 1.) < ERR_EPS);
  }
}

}  // namespace test_Boxes
}  // namespace tket
