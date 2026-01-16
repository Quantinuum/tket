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
#include <tket/Circuit/Circuit.hpp>
#include <tket/Ops/ClassicalOps.hpp>
#include <tket/Transformations/BasicOptimisation.hpp>
#include <tket/Transformations/CliffordOptimisation.hpp>
#include <tket/Transformations/OptimisationPass.hpp>
#include <tket/Transformations/PQPSquash.hpp>
#include <vector>

#include "symengine/eval_double.h"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/OpType/OpType.hpp"

namespace tket {
namespace test_Symbolic {

// Check (by substituting a selection of values) equivalence of two single-qubit
// circuits containing (at most) a single symbol "a".
static void check_equiv(const Circuit &circ, const Circuit &circ1) {
  static const std::vector<double> as = {0.,  0.4, 0.8, 1.2, 1.6, 2.0,
                                         2.4, 2.8, 3.2, 3.6, 4.0};
  Sym asym = SymEngine::symbol("a");
  for (double a : as) {
    INFO("circ:\n" << circ << "circ1:\n" << circ1 << "a = " << a);
    symbol_map_t smap = {{asym, a}};
    Circuit c = circ;
    c.symbol_substitution(smap);
    Eigen::MatrixXcd u = tket_sim::get_unitary(c);
    Circuit c1 = circ1;
    c1.symbol_substitution(smap);
    Eigen::MatrixXcd u1 = tket_sim::get_unitary(c1);
    CHECK(u.isApprox(u1));
  }
}

SCENARIO("Symbolic squashing, correctness") {
  Sym asym = SymEngine::symbol("a");
  Expr alpha(asym);
  Sym bsym = SymEngine::symbol("b");
  Expr beta(bsym);
  Sym csym = SymEngine::symbol("c");
  Expr gamma(csym);

  GIVEN("squash_1qb_to_pqp") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Ry, 0.5, {0});
    circ.add_op<unsigned>(OpType::Rz, alpha, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.5, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.5, {0});
    circ.add_op<unsigned>(OpType::Rz, 1, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.5, {0});

    Circuit circ1 = circ;
    Transforms::squash_1qb_to_pqp(OpType::Ry, OpType::Rz, true).apply(circ1);
    check_equiv(circ, circ1);
  }

  GIVEN("singleq_clifford_sweep (1)") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::U3, {2 * alpha, 0, 1.5}, {0});
    circ.add_op<unsigned>(OpType::Z, {0});
    circ.add_op<unsigned>(OpType::X, {0});

    Circuit circ1 = circ;
    Transforms::singleq_clifford_sweep().apply(circ1);
    check_equiv(circ, circ1);
  }

  GIVEN("singleq_clifford_sweep (2)") {
    Circuit circ(3);

    circ.add_op<unsigned>(OpType::U3, {alpha, 0, 0.5}, {2});
    circ.add_op<unsigned>(OpType::Vdg, {0});
    circ.add_op<unsigned>(OpType::Sdg, {2});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::S, {2});
    circ.add_op<unsigned>(OpType::S, {0});
    circ.add_op<unsigned>(OpType::V, {2});
    circ.add_op<unsigned>(OpType::V, {0});
    circ.add_op<unsigned>(OpType::U3, {0.5, 0, 0}, {2});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::U3, {0.5, 1.5, 1}, {2});
    circ.add_op<unsigned>(OpType::Sdg, {2});
    circ.add_op<unsigned>(OpType::X, {2});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::V, {1});
    circ.add_op<unsigned>(OpType::Z, {2});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::X, {2});
    circ.add_op<unsigned>(OpType::Sdg, {0});
    circ.add_op<unsigned>(OpType::S, {1});
    circ.add_op<unsigned>(OpType::V, {2});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::Sdg, {1});
    circ.add_op<unsigned>(OpType::Vdg, {2});
    circ.add_op<unsigned>(OpType::V, {0});
    circ.add_op<unsigned>(OpType::S, {1});
    circ.add_op<unsigned>(OpType::Vdg, {2});
    circ.add_op<unsigned>(OpType::S, {0});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::S, {2});
    circ.add_op<unsigned>(OpType::V, {2});
    circ.add_op<unsigned>(OpType::U3, {0.5, 0, 0}, {1});
    circ.add_op<unsigned>(OpType::Z, {2});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::Z, {2});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {2});
    circ.add_op<unsigned>(OpType::CX, {2, 1});
    circ.add_op<unsigned>(OpType::U3, {0.5, 1.5, 1}, {1});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::Sdg, {2});
    circ.add_op<unsigned>(OpType::Z, {1});
    circ.add_op<unsigned>(OpType::Vdg, {2});
    circ.add_op<unsigned>(OpType::Vdg, {1});
    circ.add_op<unsigned>(OpType::S, {2});
    circ.add_op<unsigned>(OpType::Sdg, {1});
    circ.add_op<unsigned>(OpType::TK1, {1, 0.5, 3}, {2});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::S, {1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    circ.add_op<unsigned>(OpType::Z, {0});

    Circuit circ1 = circ;
    Transforms::singleq_clifford_sweep().apply(circ1);
    check_equiv(circ, circ1);
  }

  GIVEN("Edge case where symengine atan2 returns nan (1)") {
    // https://github.com/quantinuum/tket/issues/304
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rx, {alpha}, {0});
    circ.add_op<unsigned>(OpType::Ry, {beta}, {0});
    Transforms::synthesise_tket().apply(circ);
    symbol_map_t smap = {{asym, 0}, {bsym, 0}};
    circ.symbol_substitution(smap);
    std::vector<Command> cmds = circ.get_commands();
    CHECK(cmds.size() == 4);
    Op_ptr op = cmds[1].get_op_ptr();
    CHECK(op->get_type() == OpType::TK1);
    std::vector<Expr> params = op->get_params();
    CHECK_NOTHROW(SymEngine::eval_double(params[0]));
    CHECK_NOTHROW(SymEngine::eval_double(params[1]));
    CHECK_NOTHROW(SymEngine::eval_double(params[2]));
    CHECK(approx_0(params[1]));
    CHECK(approx_0(params[0] + params[2]));
  }

  GIVEN("Edge case where symengine atan2 returns nan (2)") {
    // https://github.com/quantinuum/tket/issues/304
    // and
    // https://github.com/symengine/symengine/issues/1875
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rx, {alpha}, {0});
    circ.add_op<unsigned>(OpType::Ry, {alpha}, {0});
    Transforms::synthesise_tket().apply(circ);
    symbol_map_t smap = {{asym, 0}};
    circ.symbol_substitution(smap);
    std::vector<Command> cmds = circ.get_commands();
    CHECK(cmds.size() == 4);
    Op_ptr op = cmds[1].get_op_ptr();
    CHECK(op->get_type() == OpType::TK1);
    std::vector<Expr> params = op->get_params();
    CHECK_NOTHROW(SymEngine::eval_double(params[0]));
    CHECK_NOTHROW(SymEngine::eval_double(params[1]));
    CHECK_NOTHROW(SymEngine::eval_double(params[2]));
    CHECK(approx_0(params[1]));
    CHECK(approx_0(params[0] + params[2]));
  }

  GIVEN("Squashing where expressions are allowed to expand") {
    auto squash_circuit = [](Circuit &c, bool always_squash_symbols) {
      auto squasher =
          std::make_unique<Transforms::PQPSquasher>(OpType::Ry, OpType::Rz);
      return SingleQubitSquash(
                 std::move(squasher), c, false, always_squash_symbols)
          .squash();
    };
    Circuit circ0(1);
    circ0.add_op<unsigned>(OpType::Rz, 0.5, {0});
    circ0.add_op<unsigned>(OpType::Ry, 0.5, {0});
    circ0.add_op<unsigned>(OpType::Rz, {alpha}, {0});
    circ0.add_op<unsigned>(OpType::Ry, {beta}, {0});
    circ0.add_op<unsigned>(OpType::Rz, {gamma}, {0});
    Circuit circ1 = circ0;
    CHECK_FALSE(squash_circuit(circ0, false));
    CHECK(squash_circuit(circ1, true));
    symbol_map_t smap = {{bsym, 0.3}, {csym, 0.4}};
    circ0.symbol_substitution(smap);
    circ1.symbol_substitution(smap);
    check_equiv(circ0, circ1);
  }
}

SCENARIO("Symbolic GPI, GPI2, AAMS") {
  Sym asym = SymEngine::symbol("a");
  Expr a(asym);
  Sym bsym = SymEngine::symbol("b");
  Expr b(bsym);
  Sym csym = SymEngine::symbol("c");
  Expr c(csym);

  Circuit gpi_orig(1);
  gpi_orig.add_op<unsigned>(OpType::GPI, a, {0});
  Circuit gpi2_orig(1);
  gpi2_orig.add_op<unsigned>(OpType::GPI2, a, {0});
  Circuit aams_orig(2);
  aams_orig.add_op<unsigned>(OpType::AAMS, {a, b, c}, {0, 1});

  std::vector<double> rands{0.1231, 2.3124, 34.23, 2.23, 3.15, 1.2, 0.93};
  for (unsigned i = 0; i < rands.size(); ++i) {
    Circuit gpi_orig_sub = gpi_orig;
    Circuit gpi2_orig_sub = gpi2_orig;
    double an = rands[i];
    symbol_map_t symmap;
    symmap[asym] = an;
    gpi_orig_sub.symbol_substitution(symmap);
    gpi2_orig_sub.symbol_substitution(symmap);
    auto u_gpi_orig = tket_sim::get_unitary(gpi_orig_sub);
    auto u_gpi2_orig = tket_sim::get_unitary(gpi2_orig_sub);
    Circuit gpi_res(1);
    Circuit gpi2_res(1);
    gpi_res.add_op<unsigned>(OpType::GPI, an, {0});
    gpi2_res.add_op<unsigned>(OpType::GPI2, an, {0});
    auto u_gpi_res = tket_sim::get_unitary(gpi_res);
    auto u_gpi2_res = tket_sim::get_unitary(gpi2_res);
    REQUIRE(u_gpi_res.isApprox(u_gpi_orig));
    REQUIRE(u_gpi2_res.isApprox(u_gpi2_orig));
    for (unsigned j = 0; j < rands.size(); ++j) {
      double bn = rands[j];
      for (unsigned k = 0; k < rands.size(); ++k) {
        double cn = rands[k];
        symmap[bsym] = bn;
        symmap[csym] = cn;
        Circuit aams_orig_sub = aams_orig;
        aams_orig_sub.symbol_substitution(symmap);
        auto u_aams_orig = tket_sim::get_unitary(aams_orig_sub);
        Circuit aams_res(2);
        aams_res.add_op<unsigned>(OpType::AAMS, {an, bn, cn}, {0, 1});
        auto u_aams_res = tket_sim::get_unitary(aams_res);
        REQUIRE(u_aams_res.isApprox(u_aams_orig));
      }
    }
  }
}

SCENARIO("Symbolic substitution for classical operations") {
  std::vector<uint64_t> and_table = {0, 1, 2, 7, 0, 1, 2, 7};
  std::shared_ptr<ClassicalTransformOp> and_ttop =
      std::make_shared<ClassicalTransformOp>(3, and_table);
  std::shared_ptr<RangePredicateOp> rpop =
      std::make_shared<RangePredicateOp>(3, 2, 6);
  Circuit circ(1, 4);
  circ.add_op<unsigned>(OpType::H, {0});
  circ.add_op<unsigned>(and_ttop, {0, 1, 2});
  circ.add_op<unsigned>(and_ttop, {1, 2, 3});
  circ.add_op<unsigned>(rpop, {0, 1, 2, 3});
  circ.add_op<unsigned>(AndOp(), {2, 3, 0});
  circ.add_op<unsigned>(OrOp(), {0, 1, 2});
  circ.add_op<unsigned>(NotOp(), {2, 3});
  circ.add_op<unsigned>(ClassicalX(), {1});
  circ.add_op<unsigned>(ClassicalCX(), {0, 1});
  circ.add_op<unsigned>(AndWithOp(), {2, 3});
  Circuit circ1(circ);
  symbol_map_t symmap;
  circ1.symbol_substitution(symmap);
  REQUIRE(circ == circ1);
}

}  // namespace test_Symbolic
}  // namespace tket
