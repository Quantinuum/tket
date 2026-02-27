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
#include <memory>
#include <optional>
#include <stdexcept>
#include <unsupported/Eigen/MatrixFunctions>
#include <vector>

#include "../testutil.hpp"
#include "Ops/ClassicalOps.hpp"
#include "tket/Circuit/Boxes.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Gate/GatePtr.hpp"
#include "tket/Gate/SymTable.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Ops/OpPtr.hpp"
#include "tket/Predicates/PassLibrary.hpp"
#include "tket/Transformations/OptimisationPass.hpp"
#include "tket/Utils/Constants.hpp"
#include "tket/Utils/Expression.hpp"

namespace tket {
namespace test_Ops {

void clear_symbol_table() { SymTable::get_registered_symbols().clear(); }

SCENARIO("Check op retrieval overloads are working correctly.", "[ops]") {
  GIVEN("Transposes retrieval at the Op level") {
    const Op_ptr h = (get_op_ptr(OpType::H));
    CHECK(h->get_name() == "H");
    REQUIRE(*h->transpose() == *h);
    const Op_ptr x = (get_op_ptr(OpType::X));
    CHECK(x->get_name() == "X");
    REQUIRE(*x->transpose() == *x);
    const Op_ptr y = (get_op_ptr(OpType::Y));
    CHECK(y->get_name() == "Y");
    REQUIRE(y->transpose()->get_name() == "U3(3, 0.5, 0.5)");
    const Op_ptr z = (get_op_ptr(OpType::Z));
    CHECK(z->get_name() == "Z");
    REQUIRE(*z->transpose() == *z);
    const Op_ptr swap = (get_op_ptr(OpType::SWAP));
    CHECK(swap->get_name() == "SWAP");
    REQUIRE(*swap->transpose() == *swap);
    const Op_ptr ch = (get_op_ptr(OpType::CH));
    CHECK(ch->get_name() == "CH");
    REQUIRE(*ch->transpose() == *ch);
    const Op_ptr cx = (get_op_ptr(OpType::CX));
    CHECK(cx->get_name() == "CX");
    REQUIRE(*cx->transpose() == *cx);
    const Op_ptr cz = (get_op_ptr(OpType::CZ));
    CHECK(cz->get_name() == "CZ");
    REQUIRE(*cz->transpose() == *cz);
    const Op_ptr cv = (get_op_ptr(OpType::CV));
    CHECK(cv->get_name() == "CV");
    REQUIRE(*cv->transpose() == *cv);
    const Op_ptr cv_dg = (get_op_ptr(OpType::CVdg));
    CHECK(cv_dg->get_name() == "CVdg");
    REQUIRE(*cv_dg->transpose() == *cv_dg);
    const Op_ptr ccx = (get_op_ptr(OpType::CCX));
    CHECK(ccx->get_name() == "CCX");
    REQUIRE(*ccx->transpose() == *ccx);
    const Op_ptr noop = (get_op_ptr(OpType::noop));
    CHECK(noop->get_name() == "noop");
    REQUIRE(*noop->transpose() == *noop);
    const Op_ptr cswap = (get_op_ptr(OpType::CSWAP));
    CHECK(cswap->get_name() == "CSWAP");
    REQUIRE(*cswap->transpose() == *cswap);
    const Op_ptr cnx = (get_op_ptr(OpType::CnX));
    CHECK(cnx->get_name() == "CnX");
    REQUIRE(*cnx->transpose() == *cnx);
    const Op_ptr cnz = (get_op_ptr(OpType::CnZ));
    CHECK(cnz->get_name() == "CnZ");
    REQUIRE(*cnz->transpose() == *cnz);
    const Op_ptr bridge = (get_op_ptr(OpType::BRIDGE));
    CHECK(bridge->get_name() == "BRIDGE");
    REQUIRE(*bridge->transpose() == *bridge);
    const Op_ptr s = (get_op_ptr(OpType::S));
    CHECK(s->get_name() == "S");
    REQUIRE(*s->transpose() == *s);
    const Op_ptr t = (get_op_ptr(OpType::T));
    CHECK(t->get_name() == "T");
    REQUIRE(*t->transpose() == *t);
    const Op_ptr v = (get_op_ptr(OpType::V));
    CHECK(v->get_name() == "V");
    REQUIRE(*v->transpose() == *v);
    const Op_ptr sx = (get_op_ptr(OpType::SX));
    CHECK(sx->get_name() == "SX");
    REQUIRE(*sx->transpose() == *sx);
    const Op_ptr sx_dg = (get_op_ptr(OpType::SXdg));
    CHECK(sx_dg->get_name() == "SXdg");
    REQUIRE(*sx_dg->transpose() == *sx_dg);
    const Op_ptr crz = (get_op_ptr(OpType::CRz, 0.5));
    CHECK(crz->get_name() == "CRz(0.5)");
    REQUIRE(*crz->transpose() == *crz);
    const Op_ptr crx = (get_op_ptr(OpType::CRx, 0.5));
    CHECK(crx->get_name() == "CRx(0.5)");
    REQUIRE(*crx->transpose() == *crx);
    std::vector<Expr> rhs = {Expr(-0.5)};
    const Op_ptr cry = (get_op_ptr(OpType::CRy, 0.5));
    CHECK(cry->get_name() == "CRy(0.5)");
    CHECK(cry->get_params().size() == 1);
    REQUIRE(cry->transpose()->get_params() == rhs);
    const Op_ptr cu1 = (get_op_ptr(OpType::CU1, 0.5));
    CHECK(cu1->get_name() == "CU1(0.5)");
    REQUIRE(*cu1->transpose() == *cu1);
    const Op_ptr u1 = (get_op_ptr(OpType::U1, 0.5));
    CHECK(u1->get_name() == "U1(0.5)");
    REQUIRE(*u1->transpose() == *u1);
    const Op_ptr rz = (get_op_ptr(OpType::Rz, 0.5));
    CHECK(rz->get_name() == "Rz(0.5)");
    REQUIRE(*rz->transpose() == *rz);
    const Op_ptr rx = (get_op_ptr(OpType::Rx, 0.5));
    CHECK(rx->get_name() == "Rx(0.5)");
    REQUIRE(*rx->transpose() == *rx);
    const Op_ptr ry = (get_op_ptr(OpType::Ry, 0.5));
    CHECK(ry->get_name() == "Ry(0.5)");
    CHECK(ry->get_params().size() == 1);
    REQUIRE(ry->transpose()->get_params() == rhs);
    const Op_ptr cnry = (get_op_ptr(OpType::CnRy, 0.5));
    CHECK(cnry->get_name() == "CnRy(0.5)");
    CHECK(cnry->get_params().size() == 1);
    REQUIRE(cnry->transpose()->get_params() == rhs);
    const Op_ptr cnrx = (get_op_ptr(OpType::CnRx, 0.5));

    std::vector<Expr> rhs_same = {Expr(0.5)};
    CHECK(cnrx->get_name() == "CnRx(0.5)");
    CHECK(cnrx->get_params().size() == 1);
    REQUIRE(cnrx->transpose()->get_params() == rhs_same);
    const Op_ptr cnrz = (get_op_ptr(OpType::CnRz, 0.5));
    CHECK(cnrz->get_name() == "CnRz(0.5)");
    CHECK(cnrz->get_params().size() == 1);
    REQUIRE(cnrz->transpose()->get_params() == rhs_same);
    const Op_ptr xxphase = (get_op_ptr(OpType::XXPhase, 0.5));
    CHECK(xxphase->get_name() == "XXPhase(0.5)");
    REQUIRE(*xxphase->transpose() == *xxphase);
    const Op_ptr yyphase = (get_op_ptr(OpType::YYPhase, 0.5));
    CHECK(yyphase->get_name() == "YYPhase(0.5)");
    REQUIRE(*yyphase->transpose() == *yyphase);
    const Op_ptr zzphase = (get_op_ptr(OpType::ZZPhase, 0.5));
    CHECK(zzphase->get_name() == "ZZPhase(0.5)");
    REQUIRE(*zzphase->transpose() == *zzphase);
    const Op_ptr xxphase3 = (get_op_ptr(OpType::XXPhase3, 0.5));
    CHECK(xxphase3->get_name() == "XXPhase3(0.5)");
    REQUIRE(*xxphase3->transpose() == *xxphase3);
    const Op_ptr eswap = (get_op_ptr(OpType::ESWAP, 0.5));
    CHECK(eswap->get_name() == "ESWAP(0.5)");
    REQUIRE(*eswap->transpose() == *eswap);
    const Op_ptr fsim = (get_op_ptr(OpType::FSim, {0.5, 0.5}));
    CHECK(fsim->get_name() == "FSim(0.5, 0.5)");
    REQUIRE(*fsim->transpose() == *fsim);
    const Op_ptr u2 = (get_op_ptr(OpType::U2, {0.5, -0.5}));
    CHECK(u2->get_name() == "U2(0.5, 1.5)");
    CHECK(u2->get_params().size() == 2);
    std::vector<Expr> u2_params = {Expr(0.5), Expr{1.5}};
    REQUIRE(u2->transpose()->get_params() == u2_params);
    const Op_ptr u3 = (get_op_ptr(OpType::U3, {0.2, 0.5, -0.5}));
    CHECK(u3->get_name() == "U3(0.2, 0.5, 1.5)");
    CHECK(u3->get_params().size() == 3);
    std::vector<Expr> u3_params = {Expr{-0.2}, Expr(-0.5), Expr{0.5}};
    REQUIRE(u3->transpose()->get_params() == u3_params);
    const Op_ptr cu3 = (get_op_ptr(OpType::CU3, {0.2, 0.5, -0.5}));
    CHECK(cu3->get_name() == "CU3(0.2, 0.5, 1.5)");
    CHECK(cu3->get_params().size() == 3);
    std::vector<Expr> cu3_params = {Expr{-0.2}, Expr(-0.5), Expr{0.5}};
    REQUIRE(cu3->transpose()->get_params() == cu3_params);
    const Op_ptr TK1 = (get_op_ptr(OpType::TK1, {0.2, 0.5, -0.5}));
    CHECK(TK1->get_name() == "TK1(0.2, 0.5, 3.5)");
    CHECK(TK1->get_params().size() == 3);
    std::vector<Expr> tk1_params = {Expr{-0.5}, Expr(0.5), Expr{0.2}};
    REQUIRE(TK1->transpose()->get_params() == tk1_params);
    const Op_ptr phasedx = (get_op_ptr(OpType::PhasedX, {0.5, -0.5}));
    CHECK(phasedx->get_name() == "PhasedX(0.5, 1.5)");
    CHECK(phasedx->get_params().size() == 2);
    std::vector<Expr> phasedx_params = {Expr{0.5}, Expr(0.5)};
    REQUIRE(phasedx->transpose()->get_params() == phasedx_params);
    const Op_ptr nphasedx = (get_op_ptr(OpType::NPhasedX, {0.5, -0.5}));
    CHECK(nphasedx->get_name() == "NPhasedX(0.5, 1.5)");
    CHECK(nphasedx->get_params().size() == 2);
    CHECK(
        nphasedx->transpose()->get_params() ==
        std::vector<SymEngine::Expression>{0.5, 0.5});
    const Op_ptr twinphasedx = (get_op_ptr(OpType::TwinPhasedX, {0.5, -0.5}));
    CHECK(twinphasedx->get_name() == "TwinPhasedX(0.5, 1.5)");
    CHECK(twinphasedx->get_params().size() == 2);
    CHECK(
        twinphasedx->transpose()->get_params() ==
        std::vector<SymEngine::Expression>{0.5, 0.5});
    const Op_ptr phasedxx = (get_op_ptr(OpType::PhasedXX, {0.5, -0.5}));
    CHECK(phasedxx->get_name() == "PhasedXX(0.5, 0.5)");  // last param is mod 1
    CHECK(phasedxx->get_params().size() == 2);
    CHECK(
        phasedxx->transpose()->get_params() ==
        std::vector<SymEngine::Expression>{-0.5, 0.5});
    const Op_ptr phase = (get_op_ptr(OpType::Phase, 0.5));
    CHECK(phase->get_name() == "Phase(0.5)");
    REQUIRE(*phase->transpose() == *phase);
  }

  GIVEN("Check the transpose at the Box level") {
    // A 2x2 transposable unitary
    Eigen::Matrix2cd m;
    m(0, 0) = 0.;
    m(0, 1) = -1.;
    m(1, 0) = 1.;
    m(1, 1) = 0.;
    const Unitary1qBox u1qb(m);
    const Op_ptr u1qb_t_ptr = u1qb.transpose();
    // Casting the Unitary1qBox type
    std::shared_ptr<const Unitary1qBox> u1qb_t =
        std::dynamic_pointer_cast<const Unitary1qBox>(u1qb_t_ptr);
    CHECK(u1qb_t_ptr->get_name() == "Unitary1qBox");
    REQUIRE(matrices_are_equal(u1qb_t->get_matrix(), m.transpose()));

    // A 4x4 transposable unitary
    Eigen::Matrix4cd m2;
    m2(0, 0) = 1.;
    m2(0, 1) = 0.;
    m2(0, 2) = 0.;
    m2(0, 3) = 0.;
    m2(1, 0) = 0.;
    m2(1, 1) = 1.;
    m2(1, 2) = 0.;
    m2(1, 3) = 0.;
    m2(2, 0) = 0.;
    m2(2, 1) = 0.;
    m2(2, 2) = 0.;
    m2(2, 3) = -1.;
    m2(3, 0) = 0.;
    m2(3, 1) = 0.;
    m2(3, 2) = -1.;
    m2(3, 3) = 0.;
    const Unitary2qBox u2qb(m2);
    const Op_ptr u2qb_t_ptr = u2qb.transpose();
    // Casting the Unitary2qBox type
    std::shared_ptr<const Unitary2qBox> u2qb_t =
        std::dynamic_pointer_cast<const Unitary2qBox>(u2qb_t_ptr);
    CHECK(u2qb_t_ptr->get_name() == "Unitary2qBox");
    REQUIRE(matrices_are_equal(u2qb_t->get_matrix(), m2.transpose()));

    const ExpBox expbox(m2, -0.5);
    const Op_ptr expbox_t_ptr = expbox.transpose();
    // Casting the ExpBox type
    std::shared_ptr<const ExpBox> expbox_t =
        std::dynamic_pointer_cast<const ExpBox>(expbox_t_ptr);
    CHECK(expbox_t_ptr->get_name() == "ExpBox");
    // Get the (matrix, phase) pair
    auto pair = expbox_t->get_matrix_and_phase();
    REQUIRE(matrices_are_equal(pair.first, m2.transpose()));

    double t = 0.5;
    // Construct a Pauli box with an even number of Y-gates
    const PauliExpBox pbox_e(
        SymPauliTensor({Pauli::X, Pauli::Y, Pauli::Z, Pauli::Y, Pauli::X}, t));
    const Op_ptr pbox_t_ptr = pbox_e.transpose();
    // Casting the PauliExpBox type
    std::shared_ptr<const PauliExpBox> pbox_t =
        std::dynamic_pointer_cast<const PauliExpBox>(pbox_t_ptr);
    CHECK(pbox_t_ptr->get_name() == "PauliExpBox");
    REQUIRE(pbox_t->get_phase() == t);

    // Construct a Pauli box with an odd number of Y-gates
    const PauliExpBox pbox_o(
        SymPauliTensor({Pauli::X, Pauli::Y, Pauli::Z, Pauli::X}, t));
    const Op_ptr pbox_o_t_ptr = pbox_o.transpose();
    // Casting the PauliExpBox type
    std::shared_ptr<const PauliExpBox> pbox_o_t =
        std::dynamic_pointer_cast<const PauliExpBox>(pbox_o_t_ptr);
    CHECK(pbox_o_t_ptr->get_name() == "PauliExpBox");
    REQUIRE(pbox_o_t->get_phase() == -t);
  }

  GIVEN("A type based retrieval") {
    const Op_ptr h = (get_op_ptr(OpType::H));
    REQUIRE(h->get_name() == "H");
    REQUIRE(*h->dagger() == *h);
  }

  GIVEN("An input/output specified retrieval") {
    const Op_ptr h = (get_op_ptr(OpType::SWAP));
    REQUIRE(h->get_name() == "SWAP");
    REQUIRE(h->get_params().size() == 0);
  }

  GIVEN("An single parameter retrieval") {
    const Op_ptr h = (get_op_ptr(OpType::Rx, 5.2));
    REQUIRE(h->get_name() == "Rx(1.2)");
    REQUIRE(h->get_params().size() == 1);
    std::vector<Expr> rhs = {Expr(5.2)};
    REQUIRE(h->get_params() == rhs);
  }

  GIVEN("A multi parameter retrieval") {
    std::vector<Expr> rhs = {Expr(3.2), Expr(1.2)};
    const Op_ptr h = (get_op_ptr(OpType::U2, rhs));
    REQUIRE(h->get_name() == "U2(1.2, 1.2)");
    REQUIRE(h->get_desc().n_params() == 2);
    REQUIRE(h->get_params() == rhs);
  }

  GIVEN("Operations whose parameters have different domains") {
    Op_ptr op2 = get_op_ptr(OpType::U1, 6.4);
    Op_ptr op4 = get_op_ptr(OpType::CnRy, 6.4);
    double param2 = eval_expr(op2->get_params_reduced()[0]).value();
    double param4 = eval_expr(op4->get_params_reduced()[0]).value();
    REQUIRE(std::abs(param2 - 0.4) < ERR_EPS);
    REQUIRE(std::abs(param4 - 2.4) < ERR_EPS);
  }
}

SCENARIO("Examples for is_singleq_unitary") {
  GIVEN("Some true positives") {
    REQUIRE((get_op_ptr(OpType::Z))->get_desc().is_singleq_unitary());
    std::vector<Expr> params = {0.1, 0.2, 0.3};
    REQUIRE((get_op_ptr(OpType::U3, params))->get_desc().is_singleq_unitary());
  }
  GIVEN("Variable-qubit gates") {
    REQUIRE(!(get_op_ptr(OpType::CnRy, 0.2))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::CnRx, 0.2))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::CnRz, 0.2))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::PhaseGadget, Expr(0.4)))
                 ->get_desc()
                 .is_singleq_unitary());
  }
  GIVEN("Multi-qubit gates") {
    REQUIRE(!(get_op_ptr(OpType::CX))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::ZZPhase, Expr(0.5)))
                 ->get_desc()
                 .is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::CRz, 0.5))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::CRx, 0.5))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::CRy, 0.5))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::CV))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::CVdg))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::ECR))->get_desc().is_singleq_unitary());
  }
  GIVEN("Non-reversible gates") {
    REQUIRE(!(get_op_ptr(OpType::Measure))->get_desc().is_singleq_unitary());
    REQUIRE(!(get_op_ptr(OpType::Reset))->get_desc().is_singleq_unitary());
  }
}

SCENARIO("Check exceptions in basic Op methods", "[ops]") {
  GIVEN("A non-single-qubit gate for get_tk1_angles") {
    const Op_ptr o = get_op_ptr(OpType::CX);
    REQUIRE_THROWS_AS(as_gate_ptr(o)->get_tk1_angles(), BadOpType);
  }
  GIVEN("An invalid port number for commuting_basis") {
    const Op_ptr o = get_op_ptr(OpType::Z);
    REQUIRE_THROWS_AS((o)->commuting_basis(1), std::domain_error);
  }
  GIVEN("A parameterised gate in get_op_ptr(OpType)") {
    REQUIRE_THROWS_AS(get_op_ptr(OpType::U1), InvalidParameterCount);
  }
}

SCENARIO("Commutation relations") {
  GIVEN("An arbitrary TK1") {
    Op_ptr op = get_op_ptr(OpType::TK1, std::vector<Expr>{0.2, 0.4, 0.4});
    REQUIRE(op->commuting_basis(0) == std::nullopt);
  }
  GIVEN("An X-commuting TK1") {
    Op_ptr op = get_op_ptr(OpType::TK1, std::vector<Expr>{0., 0.4, 0.});
    REQUIRE(op->commuting_basis(0) == Pauli::X);
  }
  GIVEN("A Z-commuting TK1") {
    Op_ptr op = get_op_ptr(OpType::TK1, std::vector<Expr>{0.3, 2., 0.2});
    REQUIRE(op->commuting_basis(0) == Pauli::Z);
  }
  GIVEN("An I-commuting TK1") {
    Op_ptr op = get_op_ptr(OpType::TK1, std::vector<Expr>{0.3, 0., -0.3});
    REQUIRE(op->commuting_basis(0) == Pauli::I);
  }
  GIVEN("A noop TK1") {
    Op_ptr op = get_op_ptr(OpType::TK1, std::vector<Expr>{0., 0., 0.});
    REQUIRE(op->commuting_basis(0) == Pauli::I);
  }
  GIVEN("A noop") {
    Op_ptr op = get_op_ptr(OpType::noop);
    REQUIRE(op->commuting_basis(0) == Pauli::I);
  }
  GIVEN("A PhasedX") {
    Op_ptr op = get_op_ptr(OpType::PhasedX, std::vector<Expr>{0.2, 0.3});
    REQUIRE(op->commuting_basis(0) == std::nullopt);
  }
  GIVEN("A PhasedX that is a Rx") {
    Op_ptr op = get_op_ptr(OpType::PhasedX, std::vector<Expr>{0.2, 0.});
    REQUIRE(op->commuting_basis(0) == Pauli::X);
  }
  GIVEN("A PhasedX that is a noop") {
    Op_ptr op = get_op_ptr(OpType::PhasedX, std::vector<Expr>{0., 0.4});
    REQUIRE(op->commuting_basis(0) == Pauli::I);
  }
  GIVEN("A Rz that is a noop") {
    Op_ptr op = get_op_ptr(OpType::Rz, 0.);
    REQUIRE(op->commuting_basis(0) == Pauli::I);
  }
  GIVEN("A Rx that is a global phase") {
    Op_ptr op = get_op_ptr(OpType::Rx, 2.);
    REQUIRE(op->commuting_basis(0) == Pauli::I);
  }
  GIVEN("A Rx") {
    Op_ptr op = get_op_ptr(OpType::Rx, 2.32);
    REQUIRE(op->commuting_basis(0) == Pauli::X);
  }
  GIVEN("A PhasedXX") {
    Op_ptr op = get_op_ptr(OpType::PhasedXX, std::vector<Expr>{0.2, 0.3});
    REQUIRE(op->commuting_basis(0) == std::nullopt);
  }
  GIVEN("A PhasedXX that is an XXPhase") {
    Op_ptr op = get_op_ptr(OpType::PhasedXX, std::vector<Expr>{0.2, 0.});
    REQUIRE(op->commuting_basis(0) == Pauli::X);
  }
  GIVEN("A PhasedXX that is a noop") {
    Op_ptr op = get_op_ptr(OpType::PhasedXX, std::vector<Expr>{0., 0.1});
    REQUIRE(op->commuting_basis(0) == Pauli::I);
  }
  GIVEN("A nested conditional") {
    Circuit c0(1, 1);
    c0.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0}, 1);
    Op_ptr cbox_op = std::make_shared<CircBox>(c0);
    Op_ptr cond_cond_x = std::make_shared<Conditional>(cbox_op, 1, 1);
    Circuit circ(1, 2);
    circ.add_op<UnitID>(cond_cond_x, {Bit(0), Qubit(0), Bit(1)});
    CompilationUnit cu(circ);
    DecomposeBoxes()->apply(cu);
    Command nested_cond_X = cu.get_circ_ref().get_commands().at(0);
    REQUIRE(nested_cond_X.get_op_ptr()->commuting_basis(2) == Pauli::X);
    CHECK(nested_cond_X.get_op_ptr()->commutes_with_basis(Pauli::I, 2));
    CHECK(nested_cond_X.get_op_ptr()->commutes_with_basis(Pauli::X, 2));
    CHECK_FALSE(nested_cond_X.get_op_ptr()->commutes_with_basis(Pauli::Y, 2));
    CHECK_FALSE(nested_cond_X.get_op_ptr()->commutes_with_basis(Pauli::Z, 2));
  }
}

SCENARIO("Check some daggers work correctly", "[ops]") {
  WHEN("Check U2 gets daggered correctly") {
    Expr e(0.33);
    Expr e2(1.33);
    std::vector<Expr> exprs{e, e2};
    const Op_ptr op = get_op_ptr(OpType::U2, exprs);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::U3);
    std::vector<Expr> exprs2 = (daggered)->get_params();
    std::vector<double> vals;
    for (auto x : exprs2) {
      std::optional<double> eval = eval_expr_mod(x);
      CHECK(eval);
      vals.push_back(eval.value());
    }
    std::vector<double> correct = {1.5, 2 - 1.33, 2 - 0.33};
    REQUIRE(vals == correct);
  }
  WHEN("Check U3 gets daggered correctly") {
    Expr e1(1.43);
    Expr e2(0.15);
    Expr e3(1.58);
    std::vector<Expr> exprs{e1, e2, e3};
    const Op_ptr op = get_op_ptr(OpType::U3, exprs);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::U3);
    std::vector<Expr> exprs2 = (daggered)->get_params();
    std::vector<double> vals;
    for (auto x : exprs2) {
      std::optional<double> eval = eval_expr_mod(x);
      CHECK(eval);
      vals.push_back(eval.value());
    }
    std::vector<double> correct = {2 - 1.43, 2 - 1.58, 2 - 0.15};
    REQUIRE(vals == correct);
  }
  WHEN("Check CU3 gets daggered correctly") {
    Expr e1(1.43);
    Expr e2(0.15);
    Expr e3(1.58);
    std::vector<Expr> exprs{e1, e2, e3};
    const Op_ptr op = get_op_ptr(OpType::CU3, exprs);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::CU3);
    std::vector<Expr> exprs2 = (daggered)->get_params();
    std::vector<double> vals;
    for (auto x : exprs2) {
      std::optional<double> eval = eval_expr_mod(x);
      CHECK(eval);
      vals.push_back(eval.value());
    }
    std::vector<double> correct = {2 - 1.43, 2 - 1.58, 2 - 0.15};
    REQUIRE(vals == correct);
  }
  WHEN("Check HQS_1q gets daggered correctly") {
    Expr e1(0.03);
    Expr e2(1.95);
    std::vector<Expr> exprs{e1, e2};
    const Op_ptr op = get_op_ptr(OpType::PhasedX, exprs);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::PhasedX);
    std::vector<Expr> exprs2 = (daggered)->get_params();
    std::vector<double> vals;
    for (auto x : exprs2) {
      std::optional<double> eval = eval_expr_mod(x);
      CHECK(eval);
      vals.push_back(eval.value());
    }
    std::vector<double> correct = {2 - 0.03, 1.95};
    REQUIRE(vals == correct);
  }
  WHEN("Check ZZMax gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::ZZMax);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::ZZPhase);
    REQUIRE(test_equiv_val(daggered->get_params()[0], -0.5));
  }
  WHEN("Check CRz gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::CRz, 0.5);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::CRz);
    REQUIRE(test_equiv_val(daggered->get_params()[0], -0.5));
  }
  WHEN("Check CRx gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::CRx, 0.5);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::CRx);
    REQUIRE(test_equiv_val(daggered->get_params()[0], -0.5));
  }
  WHEN("Check CRy gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::CRy, 0.5);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::CRy);
    REQUIRE(test_equiv_val(daggered->get_params()[0], -0.5));
  }
  WHEN("Check CV gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::CV);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::CVdg);
  }
  WHEN("Check CVdg gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::CVdg);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::CV);
  }
  WHEN("Check ECR gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::ECR);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::ECR);
  }
  WHEN("Check TwinPhasedX gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::TwinPhasedX, {0.5, 0.5});
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::TwinPhasedX);
    REQUIRE(test_equiv_val(daggered->get_params()[0], -0.5));
    REQUIRE(test_equiv_val(daggered->get_params()[1], 0.5));
  }
  WHEN("Check PhasedXX gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::PhasedXX, {0.5, 0.5});
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::PhasedXX);
    REQUIRE(test_equiv_val(daggered->get_params()[0], -0.5));
    REQUIRE(test_equiv_val(daggered->get_params()[1], 0.5));
  }
  WHEN("Check Phase gets daggered correctly") {
    const Op_ptr op = get_op_ptr(OpType::Phase, 0.5);
    const Op_ptr daggered = (op)->dagger();
    REQUIRE(daggered->get_type() == OpType::Phase);
    REQUIRE(test_equiv_val(daggered->get_params()[0], -0.5));
  }
}

SCENARIO("Check copying of expressions between circuits", "[ops]") {
  WHEN("Calling circuit copy constructor on symbolic circuit") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::Rx, 0.5, {0});
    Expr const_val(1.5);
    c.add_op<unsigned>(OpType::Rx, const_val, {1});
    Sym a = SymEngine::symbol("alpha");
    Sym b = SymEngine::symbol("beta");
    Expr sym_a(a);
    c.add_op<unsigned>(OpType::Rz, sym_a, {0});
    Expr sym_b(b);
    c.add_op<unsigned>(OpType::Rz, sym_b, {1});
    symbol_map_t smap;
    smap[a] = Expr(1.7);
    c.symbol_substitution(smap);
    REQUIRE(c.is_symbolic());
    Circuit copy = c;
    std::list<Command> rz_cmds = copy.get_commands_of_type(OpType::Rz);
    REQUIRE(rz_cmds.size() == 2);
    Op_ptr rz0 = rz_cmds.front().get_op_ptr();
    Op_ptr rz1 = rz_cmds.back().get_op_ptr();
    if (test_equiv_val(rz0->get_params()[0], 1.7)) {
      REQUIRE(!eval_expr_mod(rz1->get_params()[0]));
    } else {
      REQUIRE(!eval_expr_mod(rz0->get_params()[0]));
      REQUIRE(test_equiv_val(rz1->get_params()[0], 1.7));
    }
  }
}

SCENARIO("Check that fresh_symbol actually gives a unique symbol") {
  clear_symbol_table();
  GIVEN("Manually obtained fresh_symbols") {
    Sym alpha = SymTable::fresh_symbol("a");
    Sym alpha2 = SymTable::fresh_symbol("a_2");
    Sym alpha1 = SymTable::fresh_symbol("a");
    Sym alpha3 = SymTable::fresh_symbol("a");
    REQUIRE(alpha->get_name() == "a");
    REQUIRE(alpha1->get_name() == "a_1");
    REQUIRE(alpha2->get_name() == "a_2");
    REQUIRE(alpha3->get_name() == "a_3");
  }
  GIVEN("Symbols introduced by ops") {
    get_op_ptr(OpType::Rx, Expr("2x+y"));
    Sym x1 = SymTable::fresh_symbol("x");
    REQUIRE(x1->get_name() == "x_1");
  }
}

SCENARIO("Custom Gates") {
  GIVEN("Basic manipulation") {
    // random 1qb gate
    Circuit setup(1);
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);
    setup.add_op<unsigned>(OpType::TK1, {ea, 1.0353, 0.5372}, {0});
    composite_def_ptr_t def = CompositeGateDef::define_gate("g", setup, {a});
    CustomGate g(def, {0.2374});
    Circuit c(1);
    c.add_box(g, qubit_vector_t{Qubit("q", 0)});
    REQUIRE(c.n_gates() == 1);
    // expand definition
    Circuit expanded = setup;
    symbol_map_t map = {{a, 0.2374}};
    expanded.symbol_substitution(map);
    REQUIRE(*g.to_circuit() == expanded);
  }
  GIVEN("Multiple from the same definition") {
    Circuit setup(2);
    Sym a = SymTable::fresh_symbol("a");
    Expr b(SymTable::fresh_symbol("b"));
    setup.add_op<unsigned>(OpType::CX, {0, 1});
    setup.add_op<unsigned>(OpType::Ry, {a}, {0});
    composite_def_ptr_t def = CompositeGateDef::define_gate("g", setup, {a});
    CustomGate g0(def, {0.2374});
    CustomGate g1(def, {b});
    REQUIRE(!(g0 == g1));
    REQUIRE(!(*g0.to_circuit() == *g1.to_circuit()));
  }
}

// Where should this test go?
// It could be argued that it belongs more in
// Circuit/test_Boxes.cpp, or Circuit/Simulation/test_TketSim.cpp.
// Or maybe even test_Synthesis.cpp, since they all use synthesise_tket
SCENARIO("Two-qubit entangling gates") {
  GIVEN("ESWAP") {
    // ESWAP(a) =exp(-½iπa*SWAP)
    // Verify using ExpBox
    Eigen::Matrix4cd swap;
    swap << 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1;
    const double a = 0.9890097497602238;
    ExpBox ebox(swap, -0.5 * PI * a);
    Circuit c0(2);
    c0.add_box(ebox, {0, 1});
    Circuit c1(2);
    c1.add_op<unsigned>(OpType::ESWAP, a, {0, 1});
    REQUIRE(test_unitary_comparison(c0, c1));
    // Rebase
    CompilationUnit cu(c1);
    SynthesiseTK()->apply(cu);
    REQUIRE(test_unitary_comparison(c0, cu.get_circ_ref()));
  }
  GIVEN("FSim") {
    // Check the unitary
    const double a = 0.5482604236674578;
    const double b = 0.3843021673091409;
    double ca = std::cos(PI * a);
    double sa = std::sin(PI * a);
    double cb = std::cos(PI * b);
    double sb = std::sin(PI * b);
    Eigen::Matrix4cd m;
    m << 1, 0, 0, 0, 0, ca, -i_ * sa, 0, 0, -i_ * sa, ca, 0, 0, 0, 0,
        cb - i_ * sb;
    Unitary2qBox ubox(m);
    Circuit c0(2);
    c0.add_box(ubox, {0, 1});
    Circuit c1(2);
    c1.add_op<unsigned>(OpType::FSim, {a, b}, {0, 1});
    REQUIRE(test_unitary_comparison(c0, c1));
    // Rebase
    CompilationUnit cu(c1);
    SynthesiseTK()->apply(cu);
    REQUIRE(test_unitary_comparison(c0, cu.get_circ_ref()));
  }
  GIVEN("Sycamore") {
    Eigen::Matrix4cd m;
    m << 1, 0, 0, 0, 0, 0, -i_, 0, 0, -i_, 0, 0, 0, 0, 0,
        std::cos(PI / 6) - i_ * std::sin(PI / 6);
    Unitary2qBox ubox(m);
    Circuit c0(2);
    c0.add_box(ubox, {0, 1});
    Circuit c1(2);
    c1.add_op<unsigned>(OpType::Sycamore, {0, 1});
    Transforms::synthesise_tket().apply(c1);
    REQUIRE(test_unitary_comparison(c0, c1));
  }
  GIVEN("ISWAPMax") {
    Eigen::Matrix4cd m;
    m << 1, 0, 0, 0, 0, 0, i_, 0, 0, i_, 0, 0, 0, 0, 0, 1;
    Unitary2qBox ubox(m);
    Circuit c0(2);
    c0.add_box(ubox, {0, 1});
    Circuit c1(2);
    c1.add_op<unsigned>(OpType::ISWAPMax, {0, 1});
    Transforms::synthesise_tket().apply(c1);
    REQUIRE(test_unitary_comparison(c0, c1));
  }
  GIVEN("PhasedISWAP") {
    const double p = 0.6;
    const double t = 0.7;
    double c = std::cos(0.5 * PI * t);
    double s = std::sin(0.5 * PI * t);
    Complex f = exp(2 * PI * i_ * p);
    Eigen::Matrix4cd m;
    m << 1, 0, 0, 0, 0, c, i_ * s * f, 0, 0, i_ * s * std::conj(f), c, 0, 0, 0,
        0, 1;
    Unitary2qBox ubox(m);
    Circuit c0(2);
    c0.add_box(ubox, {0, 1});
    Circuit c1(2);
    c1.add_op<unsigned>(OpType::PhasedISWAP, {p, t}, {0, 1});
    Transforms::synthesise_tket().apply(c1);
    REQUIRE(test_unitary_comparison(c0, c1));
  }
}

SCENARIO("Opaque classical ops") {
  GIVEN("RNG op serialization") {
    std::shared_ptr<OpaqueClassicalOp> op =
        std::make_shared<OpaqueClassicalOp>(OpType::RNGSeed);
    nlohmann::json j = op->serialize();
    Op_ptr op1 = OpaqueClassicalOp::deserialize(j);
    REQUIRE(op->is_equal(*op1));
  }
  GIVEN("RNG circuit serialization") {
    Circuit c(0, 64);

    REQUIRE(c.r_inputs().empty());
    REQUIRE(c.r_outputs().empty());

    std::vector<UnitID> args64;
    for (unsigned i = 0; i < 64; i++) {
      args64.push_back(Bit(i));
    }
    args64.push_back(RngState(0));
    std::vector<UnitID> args32;
    for (unsigned i = 0; i < 32; i++) {
      args32.push_back(Bit(i));
    }
    args32.push_back(RngState(0));

    c.add_op(std::make_shared<OpaqueClassicalOp>(OpType::RNGSeed), args64);
    std::vector<bool> values{true, true};
    std::shared_ptr<SetBitsOp> sb = std::make_shared<SetBitsOp>(values);
    c.add_op<Bit>(sb, {Bit(0), Bit(1)});
    c.add_op(std::make_shared<OpaqueClassicalOp>(OpType::RNGBound), args32);
    c.add_op(std::make_shared<OpaqueClassicalOp>(OpType::RNGIndex), args32);
    c.add_op(std::make_shared<OpaqueClassicalOp>(OpType::RNGNum), args32);

    REQUIRE(c.r_inputs().size() == 1);
    REQUIRE(c.r_outputs().size() == 1);
    c.assert_valid();

    nlohmann::json j = c;
    Circuit c1 = j.get<Circuit>();
    REQUIRE(c == c1);
  }
  GIVEN("Shot-number serialization") {
    Circuit c(0, 32);
    c.add_op(
        std::make_shared<OpaqueClassicalOp>(OpType::JobShotNum), c.all_bits());
    nlohmann::json j = c;
    Circuit c1 = j.get<Circuit>();
    REQUIRE(c == c1);
  }
  GIVEN("RNG circuit substitution") {
    Circuit c(1, 32);
    c.add_op<unsigned>(OpType::H, {0});
    std::vector<UnitID> args32;
    for (unsigned i = 0; i < 32; i++) {
      args32.push_back(Bit(i));
    }
    args32.push_back(RngState(0));
    Vertex v =
        c.add_op(std::make_shared<OpaqueClassicalOp>(OpType::RNGBound), args32);
    // Replace the RNGBound operation with RNGIndex.
    Circuit c1(0, 32);
    c1.add_op(std::make_shared<OpaqueClassicalOp>(OpType::RNGIndex), args32);
    c.substitute(c1, v);
    std::map<OpType, unsigned> opcounts = c.op_counts();
    REQUIRE(!opcounts.contains(OpType::RNGBound));
    REQUIRE(opcounts.at(OpType::RNGIndex) == 1);
  }
}

SCENARIO("Clifford checks") {
  GIVEN("PhasedX") {
    const Op_ptr op0 = get_op_ptr(OpType::PhasedX, {1.0, 0.25});
    const Op_ptr op1 = get_op_ptr(OpType::PhasedX, {1.5, 0.5});
    const Op_ptr op2 = get_op_ptr(OpType::PhasedX, {1.5, 0.25});
    const Op_ptr op3 = get_op_ptr(OpType::PhasedX, {1.0, 0.125});
    CHECK(op0->is_clifford());
    CHECK(op1->is_clifford());
    CHECK(!op2->is_clifford());
    CHECK(!op3->is_clifford());
  }
}

}  // namespace test_Ops
}  // namespace tket
