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

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <tkrng/RNG.hpp>
#include <vector>

#include "testutil.hpp"
#include "tket/Circuit/CircPool.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/Command.hpp"
#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Gate/SymTable.hpp"
#include "tket/Mapping/LexiLabelling.hpp"
#include "tket/Mapping/LexiRouteRoutingMethod.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/OpType/OpTypeFunctions.hpp"
#include "tket/Ops/ClassicalOps.hpp"
#include "tket/Placement/Placement.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Predicates/CompilerPass.hpp"
#include "tket/Predicates/PassGenerators.hpp"
#include "tket/Predicates/PassLibrary.hpp"
#include "tket/Transformations/MeasurePass.hpp"
#include "tket/Transformations/OptimisationPass.hpp"
#include "tket/Transformations/PauliOptimisation.hpp"
#include "tket/Utils/Expression.hpp"
#include "tket/Utils/UnitID.hpp"
namespace tket {
namespace test_CompilerPass {

SCENARIO("Run some basic Compiler Passes") {
  GIVEN(
      "A Compilation Unit that has unsatisfied Predicates at the end of "
      "the Compiler Pass") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    OpTypeSet ots = {OpType::CX};
    PredicatePtr gsp = std::make_shared<GateSetPredicate>(ots);
    PredicatePtrMap ppm{CompilationUnit::make_type_pair(gsp)};

    OpTypeSet ots2 = {OpType::CY};
    PredicatePtr gsp2 = std::make_shared<GateSetPredicate>(ots2);
    PredicatePtrMap ppm2{CompilationUnit::make_type_pair(gsp2)};

    CompilationUnit cu(circ, ppm);
    // safety mode off
    PostConditions pc{ppm2, {}, Guarantee::Preserve};
    PassPtr compass = std::make_shared<StandardPass>(
        ppm, Transforms::id, pc, nlohmann::json{});
    WHEN("Run a basic pass") { REQUIRE(!compass->apply(cu)); }
    // switch safety mode on
    PassPtr compass2 = std::make_shared<StandardPass>(
        ppm, Transforms::id, pc, nlohmann::json{});
    WHEN("Run something with an unsatisfied predicate") {
      REQUIRE_THROWS_AS(
          compass2->apply(cu, SafetyMode::Audit), UnsatisfiedPredicate);
    }
    WHEN("Compose 2 compatible Compiler Passes") {
      PostConditions pc3{ppm2, {}, Guarantee::Preserve};
      PassPtr compass3 = std::make_shared<StandardPass>(
          ppm2, Transforms::id, pc3, nlohmann::json{});
      PassPtr combination = compass >> compass3;

      // safety mode off
      REQUIRE(!combination->apply(cu, SafetyMode::Default));

      PassPtr combination2 = compass2 >> compass3;
      // safety mode on
      REQUIRE_THROWS_AS(
          combination2->apply(cu, SafetyMode::Audit), UnsatisfiedPredicate);
    }
    WHEN("Compose 2 incompatible Compiler Passes") {
      REQUIRE_THROWS_AS(compass2 >> compass, IncompatibleCompilerPasses);
    }
    WHEN("Add a class guarantee that invalidates the GateSetPredicate") {
      PredicateClassGuarantees pcg{
          {CompilationUnit::make_type_pair(gsp2).first, Guarantee::Clear}};
      PostConditions pc_clear{{}, pcg, Guarantee::Preserve};
      PassPtr compass_clear = std::make_shared<StandardPass>(
          ppm2, Transforms::id, pc_clear, nlohmann::json{});
      Circuit circ2(2);
      circ2.add_op<unsigned>(OpType::CY, {0, 1});
      CompilationUnit cu2(circ2, ppm);
      REQUIRE(!compass_clear->apply(cu2));
      REQUIRE(!cu2.check_all_predicates());
    }
  }
}

SCENARIO("Test that qubits added via add_qubit are tracked.") {
  GIVEN("Adding qubit via custom Pass.") {
    Circuit circ(2, 1);
    Qubit weird_qb("weird_q", 3);
    Qubit weird_qb2("weird_q", 5);
    Qubit weird_qb3("weird_qb", 7);
    Bit weird_cb("weird_c", 3, 1);
    circ.add_qubit(weird_qb);
    circ.add_qubit(weird_qb2);
    circ.add_bit(weird_cb);

    CompilationUnit cu(circ);

    // circuit bimaps property wont be changed, nor will compilation unit
    circ.add_qubit(weird_qb3);
    unit_bimap_t cu_initial = cu.get_initial_map_ref();

    auto it = cu_initial.left.find(weird_qb3);
    REQUIRE(it == cu_initial.left.end());

    // Instead add transform for running it
    Transform t =
        Transform([](Circuit& circ, std::shared_ptr<unit_bimaps_t> maps) {
          Qubit weird_qb4("weird_qb", 9);
          circ.add_qubit(weird_qb4);
          if (maps) {
            maps->initial.left.insert({weird_qb4, weird_qb4});
            maps->final.left.insert({weird_qb4, weird_qb4});
          }
          return true;
        });

    // convert to pass
    PredicatePtrMap s_ps;
    PostConditions postcon;
    auto pass =
        std::make_shared<StandardPass>(s_ps, t, postcon, nlohmann::json{});

    // Comparison qubit
    Qubit weird_qb4("weird_qb", 9);
    pass->apply(cu);
    cu_initial = cu.get_initial_map_ref();
    // check all maps to show weird_qb4 is mapped to self in both initial and
    // final
    it = cu_initial.left.find(weird_qb4);
    REQUIRE(it != cu_initial.left.end());
    REQUIRE(it->second == weird_qb4);
    auto cu_final = cu.get_final_map_ref();
    it = cu_final.left.find(weird_qb4);
    REQUIRE(it != cu_final.left.end());
    REQUIRE(it->second == weird_qb4);
  }
}
SCENARIO("Test making (mostly routing) passes using PassGenerators") {
  GIVEN("Correct pass for Predicate") {
    SquareGrid grid(1, 5);

    PassPtr cp_route = gen_default_mapping_pass(grid, false);
    Circuit circ(5);
    add_2qb_gates(circ, OpType::CX, {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {3, 4}});

    PredicatePtr routed_correctly =
        std::make_shared<ConnectivityPredicate>(grid);
    PredicatePtrMap preds{CompilationUnit::make_type_pair(routed_correctly)};
    CompilationUnit cu(circ, preds);

    WHEN("Ran in unsafe mode") {
      REQUIRE(cp_route->apply(cu, SafetyMode::Default));
      REQUIRE(cu.check_all_predicates());
    }
    WHEN("Ran in safe mode") {
      REQUIRE(cp_route->apply(cu, SafetyMode::Audit));
      REQUIRE(cu.check_all_predicates());
    }
  }
  GIVEN("Incorrect pass for Predicate logs a warning") {
    SquareGrid grid(2, 3);

    PassPtr cp_route = gen_default_mapping_pass(grid, false);
    Circuit circ(6);
    add_2qb_gates(circ, OpType::CX, {{0, 1}, {0, 5}, {0, 3}, {1, 2}, {3, 4}});

    SquareGrid grid2(1, 6);
    PredicatePtr routed_correctly =
        std::make_shared<ConnectivityPredicate>(grid2);
    PredicatePtrMap preds{CompilationUnit::make_type_pair(routed_correctly)};
    CompilationUnit cu(circ, preds);

    WHEN("Ran in unsafe mode") {
      REQUIRE(cp_route->apply(cu));  // warning should be logged
      REQUIRE(!cu.check_all_predicates());
    }
    WHEN("Ran in safe mode") {
      REQUIRE(
          cp_route->apply(cu, SafetyMode::Audit));  // warning should be logged
      REQUIRE(!cu.check_all_predicates());
    }
  }
  GIVEN("Correct sequence of Synthesis, Routing and Rebasing") {
    Circuit circ(6);
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 5});
    circ.add_op<unsigned>(OpType::CZ, {0, 3});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::Z, {3});
    circ.add_op<unsigned>(OpType::CY, {3, 4});

    SquareGrid grid(2, 3);

    PredicatePtr routed_correctly =
        std::make_shared<ConnectivityPredicate>(grid);
    OpTypeSet ots = {OpType::CX, OpType::PhasedX, OpType::Rz};
    PredicatePtr gsp = std::make_shared<GateSetPredicate>(ots);

    PredicatePtrMap preds = {
        CompilationUnit::make_type_pair(routed_correctly),
        CompilationUnit::make_type_pair(gsp)};
    CompilationUnit cu(circ, preds);

    PassPtr cp_route = gen_default_mapping_pass(grid, false);

    Circuit cx(2);
    cx.add_op<unsigned>(OpType::CX, {0, 1});
    PassPtr pz_rebase = gen_rebase_pass(
        {OpType::CX, OpType::PhasedX, OpType::Rz}, cx,
        CircPool::tk1_to_PhasedXRz);
    PassPtr all_passes = SynthesiseTK() >> cp_route >> pz_rebase;

    REQUIRE(all_passes->apply(cu));
    REQUIRE(cu.check_all_predicates());
    WHEN("Ran in safe mode") {
      REQUIRE(all_passes->apply(cu, SafetyMode::Audit));
      REQUIRE(cu.check_all_predicates());
    }
    WHEN("Make incorrect sequence") {
      PassPtr bad_pass = cp_route >> SynthesiseTK();
      bad_pass->apply(cu);
      REQUIRE(!cu.check_all_predicates());
    }
  }
  GIVEN("Synthesise Passes in a row then routing") {
    Circuit circ(5);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::CH, {0, 2});
    circ.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::X, {4});
    OpTypeSet ots = {OpType::TK2, OpType::TK1, OpType::SWAP};
    PredicatePtr gsp = std::make_shared<GateSetPredicate>(ots);
    SquareGrid grid(2, 3);

    PredicatePtr routed_correctly =
        std::make_shared<ConnectivityPredicate>(grid);
    PredicatePtrMap preds = {
        CompilationUnit::make_type_pair(routed_correctly),
        CompilationUnit::make_type_pair(gsp)};

    CompilationUnit cu(circ, preds);

    Placement::Ptr pp = std::make_shared<GraphPlacement>(grid);
    PassPtr cp_route = gen_full_mapping_pass(
        grid, pp,
        {std::make_shared<LexiLabellingMethod>(),
         std::make_shared<LexiRouteRoutingMethod>()});

    PassPtr all_passes = SynthesiseTket() >> SynthesiseTK() >> cp_route;
    REQUIRE(all_passes->apply(cu));
    REQUIRE(cu.check_all_predicates());
  }
  GIVEN("A gen_euler_pass test with strict decomposition") {
    PassPtr squash = gen_euler_pass(OpType::Rz, OpType::Rx, true);
    Circuit circ(1);
    for (unsigned i = 0; i < 9; ++i) {
      circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
      circ.add_op<unsigned>(OpType::Rx, 0.5, {0});
    }
    CompilationUnit cu(circ);
    squash->apply(cu);
    const Circuit& c = cu.get_circ_ref();
    c.assert_valid();
    REQUIRE(c.n_gates() == 3);
  }
  GIVEN("A gen_euler_pass test") {
    PassPtr squash = gen_euler_pass(OpType::Rz, OpType::Rx, false);
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    for (unsigned i = 0; i < 9; ++i) {
      circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
      circ.add_op<unsigned>(OpType::Rx, 0.5, {0});
      circ.add_op<unsigned>(OpType::Rx, 0.3, {1});
      circ.add_op<unsigned>(OpType::Rz, 0.5, {1});
    }
    CompilationUnit cu(circ);
    squash->apply(cu);
    const Circuit& c = cu.get_circ_ref();
    c.assert_valid();
    REQUIRE(c.n_gates() == 3 + 3 + 1);
    auto cmds = c.get_commands();
    std::vector<OpType> expected_optypes{
        OpType::Rz, OpType::Rx,              // before CX
        OpType::CX, OpType::Rx, OpType::Rz,  // qubit 0 after CX
        OpType::Rz, OpType::Rx               // qubit 1 after CX
    };
    for (unsigned i = 0; i < expected_optypes.size(); ++i) {
      REQUIRE(cmds[i].get_op_ptr()->get_type() == expected_optypes[i]);
    }
  }
  GIVEN("A gen_euler_pass test with 2CX") {
    PassPtr squash = gen_euler_pass(OpType::Rz, OpType::Rx, false);
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    for (unsigned i = 0; i < 9; ++i) {
      circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
      circ.add_op<unsigned>(OpType::Rx, 0.5, {0});
      circ.add_op<unsigned>(OpType::Rx, 0.3, {1});
      circ.add_op<unsigned>(OpType::Rz, 0.5, {1});
    }
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    for (unsigned i = 0; i < 9; ++i) {
      circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
      circ.add_op<unsigned>(OpType::Rx, 0.5, {0});
      circ.add_op<unsigned>(OpType::Rx, 0.3, {1});
      circ.add_op<unsigned>(OpType::Rz, 0.5, {1});
    }
    CompilationUnit cu(circ);
    squash->apply(cu);
    const Circuit& c = cu.get_circ_ref();
    c.assert_valid();
    REQUIRE(c.n_gates() == 4 * 2 + 1 + 1 + 2);
    auto cmds = c.get_commands();
    std::vector<OpType> expected_optypes{
        OpType::Rx,                          // qubit 0 before CXs
        OpType::Rz,                          // qubit 1 before CXs
        OpType::CX, OpType::Rz, OpType::Rx,  // qubit 0 between CXs
        OpType::Rx, OpType::Rz,              // qubit 1 between CXs
        OpType::CX, OpType::Rx, OpType::Rz,  // qubit 0 after CXs
        OpType::Rz, OpType::Rx,              // qubit 1 after CXs
    };
    for (unsigned i = 0; i < expected_optypes.size(); ++i) {
      REQUIRE(cmds[i].get_op_ptr()->get_type() == expected_optypes[i]);
    }
  }

  GIVEN("gen_euler_pass commuting conditionals through CX") {
    PassPtr squash = gen_euler_pass(OpType::Rz, OpType::Rx);
    Circuit circ(2, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.143}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rx, {0.528}, {1}, {0}, 0);
    circ.add_conditional_barrier({0, 1}, {}, {0}, 1, "");
    CompilationUnit cu(circ);
    squash->apply(cu);
    const Circuit& c = cu.get_circ_ref();
    c.assert_valid();
    REQUIRE(c.n_gates() == 4);
    std::vector<OpType> expected_optypes{
        OpType::Conditional,  // qubit 0 before CX
        OpType::Conditional,  // qubit 1 before CX
        OpType::CX, OpType::Conditional};
    check_command_types(c, expected_optypes);

    auto cmds = c.get_commands();
    Op_ptr op0 =
        static_cast<const Conditional&>(*cmds[0].get_op_ptr()).get_op();
    Op_ptr op1 =
        static_cast<const Conditional&>(*cmds[1].get_op_ptr()).get_op();

    REQUIRE(op0->get_type() == OpType::Rz);
    REQUIRE(op0->get_params() == std::vector<Expr>{0.285});
    REQUIRE(op1->get_type() == OpType::Rx);
    REQUIRE(op1->get_params() == std::vector<Expr>{0.528});
  }

  GIVEN("Repeat synthesis passes") {
    OpTypeSet ots = {OpType::H};
    PredicatePtr gsp = std::make_shared<GateSetPredicate>(ots);
    PredicatePtrMap ppm{CompilationUnit::make_type_pair(gsp)};
    PostConditions pc{ppm, {}, Guarantee::Preserve};
    PassPtr compass = std::make_shared<StandardPass>(
        ppm, Transforms::id, pc, nlohmann::json{});
    PassPtr rep = std::make_shared<RepeatPass>(compass);
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::H, {0});
    CompilationUnit cu(circ);
    REQUIRE_NOTHROW(rep->apply(cu));
    cu.get_circ_ref().assert_valid();
  }
  GIVEN("Full compilation sequence") {
    SquareGrid grid(1, 5);
    std::vector<PassPtr> passes = {
        DecomposeBoxes(), RebaseTket(), gen_default_mapping_pass(grid, true)};
    REQUIRE_NOTHROW(SequencePass(passes));
  }
  GIVEN("TK1 and TK2 replacement functions") {
    auto tk1_replacement = [](const Expr& a, const Expr& b, const Expr& c) {
      Circuit circ(1);
      circ.add_op<unsigned>(OpType::Rz, c, {0});
      circ.add_op<unsigned>(OpType::Rx, b, {0});
      circ.add_op<unsigned>(OpType::Rz, a, {0});
      return circ;
    };
    auto tk2_replacement = [](const Expr& a, const Expr& b, const Expr& c) {
      Circuit circ(2);
      circ.add_op<unsigned>(OpType::ZZPhase, c, {0, 1});
      circ.add_op<unsigned>(OpType::YYPhase, b, {0, 1});
      circ.add_op<unsigned>(OpType::XXPhase, a, {0, 1});
      return circ;
    };
    OpTypeSet allowed_gates = {OpType::Rx,      OpType::Ry,
                               OpType::Rz,      OpType::XXPhase,
                               OpType::YYPhase, OpType::ZZPhase};

    Circuit circ(2);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    PassPtr pp = gen_rebase_pass_via_tk2(
        allowed_gates, tk2_replacement, tk1_replacement);

    CompilationUnit cu(circ);
    CHECK(pp->apply(cu));
    CHECK(cu.get_circ_ref().count_gates(OpType::XXPhase) == 1);
    CHECK(cu.get_circ_ref().count_gates(OpType::YYPhase) == 0);
    CHECK(cu.get_circ_ref().count_gates(OpType::ZZPhase) == 0);
  }
}

SCENARIO("Construct sequence pass") {
  std::vector<PassPtr> passes = {CommuteThroughMultis(), KAKDecomposition()};
  PassPtr sequence = std::make_shared<SequencePass>(passes);

  WHEN("Apply to valid CompilationUnit") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(circ);
    REQUIRE_NOTHROW(sequence->apply(cu));
  }
}

SCENARIO("Construct sequence pass that is invalid in strict mode") {
  std::vector<PassPtr> invalid_pass_to_combo{SynthesiseTket(), SynthesiseTK()};
  for (const PassPtr& pass : invalid_pass_to_combo) {
    std::vector<PassPtr> passes = {pass};
    OpTypeSet ots = {OpType::CX};
    PredicatePtr gsp = std::make_shared<GateSetPredicate>(ots);
    PredicatePtrMap ppm{CompilationUnit::make_type_pair(gsp)};
    PostConditions pc{{}, {}, Guarantee::Preserve};
    PassPtr compass = std::make_shared<StandardPass>(
        ppm, Transforms::id, pc, nlohmann::json{});
    passes.push_back(compass);
    REQUIRE_THROWS_AS((void)SequencePass(passes), IncompatibleCompilerPasses);
    GIVEN("A circuit compilable with non-strict SequencePass") {
      PassPtr sequence = std::make_shared<SequencePass>(passes, false);
      Circuit circ(2);
      CompilationUnit cu(circ);
      REQUIRE_NOTHROW(sequence->apply(cu));
    }
  }
}

SCENARIO("Construct invalid sequence of loops") {
  PredicatePtr pp1 = std::make_shared<NoClassicalControlPredicate>();
  PredicatePtrMap ppm{CompilationUnit::make_type_pair(pp1)};
  PostConditions pc{{}, {}, Guarantee::Preserve};
  PassPtr pass1 =
      std::make_shared<StandardPass>(ppm, Transforms::id, pc, nlohmann::json{});
  PassPtr loop1 = std::make_shared<RepeatPass>(pass1);
  PostConditions pc2{{}, {}, Guarantee::Clear};
  PredicatePtrMap empty_ppm{};
  PassPtr pass2 = std::make_shared<StandardPass>(
      empty_ppm, Transforms::id, pc2, nlohmann::json{});
  PassPtr loop2 = std::make_shared<RepeatPass>(pass2);
  std::vector<PassPtr> good_passes{loop1, loop2};
  std::vector<PassPtr> bad_passes{loop2, loop1};
  REQUIRE_NOTHROW((void)SequencePass(good_passes));
  REQUIRE_THROWS_AS((void)SequencePass(bad_passes), IncompatibleCompilerPasses);
}

SCENARIO("RepeatPass with strict checking") {
  Circuit circ(1);
  circ.add_op<unsigned>(OpType::PhasedX, {0.3, 0.2}, {0});
  PassPtr pp = SquashRzPhasedX();
  PassPtr rep_pp = std::make_shared<RepeatPass>(pp, true);
  CompilationUnit cu(circ);
  bool rv = rep_pp->apply(cu);
  CHECK_FALSE(rv);
  CHECK(cu.get_circ_ref() == circ);
  circ.add_op<unsigned>(OpType::Rz, 0.0, {0});
  CompilationUnit cu1(circ);
  bool rv1 = rep_pp->apply(cu1);
  CHECK(rv1);
  CHECK(cu1.get_circ_ref() != circ);
}

SCENARIO("Test RepeatWithMetricPass") {
  GIVEN("Monotonically decreasing pass") {
    PassPtr seq_p = RemoveRedundancies() >> CommuteThroughMultis();
    Transform::Metric met = [](const Circuit& circ) {
      return circ.n_vertices();
    };
    PassPtr rwm_p = std::make_shared<RepeatWithMetricPass>(seq_p, met);
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(circ);
    rwm_p->apply(cu);
    REQUIRE(cu.get_circ_ref().n_gates() == 1);
  }
}

SCENARIO("Track initial and final maps throughout compilation") {
  GIVEN("SynthesiseTK should not affect them") {
    Circuit circ(5);
    add_2qb_gates(circ, OpType::CY, {{0, 3}, {1, 4}, {1, 0}, {2, 1}});
    circ.add_op<unsigned>(OpType::SWAP, {3, 4});
    circ.add_op<unsigned>(OpType::Z, {4});
    REQUIRE(circ.replace_SWAPs());
    CompilationUnit cu(circ);
    SynthesiseTK()->apply(cu);
    for (auto pair : cu.get_initial_map_ref().left) {
      REQUIRE(pair.first == pair.second);
    }
    for (auto pair : cu.get_final_map_ref().left) {
      REQUIRE(pair.first == pair.second);
    }
  }
  GIVEN("Routing should modify them") {
    Circuit circ(5);
    add_2qb_gates(circ, OpType::CY, {{0, 3}, {1, 4}, {1, 0}, {2, 1}});
    circ.add_op<unsigned>(OpType::SWAP, {3, 4});
    circ.add_op<unsigned>(OpType::Z, {4});
    REQUIRE(circ.replace_SWAPs());
    unit_map_t rename_map = {
        {Qubit(0), Qubit("qa")},
        {Qubit(1), Qubit("qb")},
        {Qubit(2), Qubit("qc")},
        {Qubit(3), Qubit("qd")},
        {Qubit(4), Qubit("qe")}};
    circ.rename_units(rename_map);
    CompilationUnit cu(circ);

    SquareGrid grid(2, 3);
    PassPtr cp_route = gen_default_mapping_pass(grid, false);
    cp_route->apply(cu);
    bool ids_updated = true;
    for (auto pair : cu.get_initial_map_ref().left) {
      ids_updated &= grid.node_exists(Node(pair.second));
    }
    for (auto pair : cu.get_final_map_ref().left) {
      ids_updated &= grid.node_exists(Node(pair.second));
    }
    REQUIRE(ids_updated);
    Circuit res(cu.get_circ_ref());
    Vertex x = res.add_op<Qubit>(
        OpType::X, {Qubit(cu.get_final_map_ref().left.at(Qubit("qe")))});
    Vertex pred = res.get_predecessors(x).front();
    REQUIRE(res.get_OpType_from_Vertex(pred) == OpType::Z);
  }
}

SCENARIO("FlattenRegisters pass") {
  GIVEN("A simple circuit") {
    Circuit circ(3, 2);
    CompilationUnit cu(circ);
    REQUIRE_FALSE(FlattenRegisters()->apply(cu));
  }
  GIVEN("A non-simple circuit") {
    Circuit circ(2, 1);
    Qubit weird_qb("weird_q", 3);
    Qubit weird_qb2("weird_q", 5);
    Bit weird_cb("weird_c", 3, 1);
    circ.add_qubit(weird_qb);
    circ.add_qubit(weird_qb2);
    circ.add_bit(weird_cb);
    CompilationUnit cu(circ);
    REQUIRE(FlattenRegisters()->apply(cu));
    REQUIRE(cu.get_circ_ref().is_simple());
    unit_bimap_t map = cu.get_initial_map_ref();
    REQUIRE(map.left.at(weird_qb) == Qubit(2));
    REQUIRE(map.left.at(weird_qb2) == Qubit(3));
    REQUIRE(map.left.at(weird_cb) == Bit(1));
  }
}

SCENARIO("RemoveBarriers pass") {
  GIVEN("A circuit without a Barrier") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    CompilationUnit cu(circ);
    REQUIRE_FALSE(RemoveBarriers()->apply(cu));
    const Circuit& circ1 = cu.get_circ_ref();
    REQUIRE(circ1 == circ);
  }
  GIVEN("A circuit with a Barrier") {
    Circuit circ(3);
    add_1qb_gates(circ, OpType::H, {0, 1});
    circ.add_barrier({1, 2});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    CompilationUnit cu(circ);
    REQUIRE(RemoveBarriers()->apply(cu));
    const Circuit& circ1 = cu.get_circ_ref();
    REQUIRE(circ1.n_vertices() < circ.n_vertices());
  }
}

SCENARIO("RemovePhaseOps pass") {
  GIVEN("A circuit containing Phase ops") {
    Circuit circ(1, 1), circ0(1, 1);
    circ.add_op<unsigned>(OpType::Phase, 0.1, {});
    circ.add_op<unsigned>(OpType::H, {0});
    circ0.add_op<unsigned>(OpType::H, {0});
    circ.add_measure(0, 0);
    circ0.add_measure(0, 0);
    circ.add_conditional_gate<unsigned>(OpType::Phase, {0.2}, {}, {0}, 1);
    circ.add_op<unsigned>(OpType::Phase, 0.3, {});
    CompilationUnit cu(circ);
    REQUIRE(RemovePhaseOps()->apply(cu));
    const Circuit& circ1 = cu.get_circ_ref();
    REQUIRE(circ1 == circ0);
    CompilationUnit cu0(circ0);
    REQUIRE_FALSE(RemovePhaseOps()->apply(cu0));
  }
}

SCENARIO("gen_placement_pass test") {
  GIVEN("A simple circuit and device and base Placement.") {
    Circuit circ(4);
    add_2qb_gates(circ, OpType::CX, {{0, 1}, {2, 1}, {2, 3}});
    Architecture arc(
        {{Node(0), Node(1)}, {Node(1), Node(2)}, {Node(3), Node(2)}});
    Placement::Ptr plptr = std::make_shared<Placement>(arc);
    PassPtr pp_place = gen_placement_pass(plptr);
    CompilationUnit cu(circ);
    pp_place->apply(cu);
    Circuit res(cu.get_circ_ref());
    qubit_vector_t all_res_qbs = res.all_qubits();
    REQUIRE(all_res_qbs[0] == Node(0));
    REQUIRE(all_res_qbs[1] == Node(1));
    REQUIRE(all_res_qbs[2] == Node(2));
    REQUIRE(all_res_qbs[3] == Node(3));
  }
  GIVEN("A simple circuit and device and GraphPlacement.") {
    Circuit circ(4);
    add_2qb_gates(circ, OpType::CX, {{0, 1}, {2, 1}, {2, 3}});
    Architecture arc(
        {{Node(0), Node(1)}, {Node(1), Node(2)}, {Node(3), Node(2)}});
    Placement::Ptr plptr = std::make_shared<GraphPlacement>(arc);
    PassPtr pp_place = gen_placement_pass(plptr);
    CompilationUnit cu(circ);
    pp_place->apply(cu);
    Circuit res(cu.get_circ_ref());
    qubit_vector_t all_res_qbs = res.all_qubits();
    for (unsigned nn = 0; nn <= 3; ++nn) {
      REQUIRE(all_res_qbs[nn] == Node(nn));
    }
  }

  GIVEN("A large circuit and a large architecture.") {
    unsigned N = 150;
    Circuit circ(N);
    for (unsigned i = 0; i < N - 3; ++i) {
      circ.add_op<unsigned>(OpType::CX, {i, i + 1});
      circ.add_op<unsigned>(OpType::CX, {i, i + 2});
      circ.add_op<unsigned>(OpType::CX, {i, i + 3});
    }
    // Generate a line architecture
    std::vector<std::pair<unsigned, unsigned>> edges;
    for (unsigned i = 0; i < N - 1; i++) {
      edges.push_back({i, i + 1});
    }
    Architecture line_arc(edges);
    // Get a graph placement
    PassPtr graph_place = gen_placement_pass(
        std::make_shared<GraphPlacement>(line_arc, 100, 100000));
    CompilationUnit graph_cu((Circuit(circ)));
    graph_place->apply(graph_cu);
    // Get a noise - aware placement
    avg_node_errors_t empty_node_errors = {};
    avg_readout_errors_t empty_readout_errors = {};
    avg_link_errors_t empty_link_errors = {};
    PassPtr noise_place = gen_placement_pass(
        std::make_shared<NoiseAwarePlacement>(
            line_arc, empty_node_errors, empty_link_errors,
            empty_readout_errors, 10, 1000000));
    CompilationUnit noise_cu((Circuit(circ)));
    noise_place->apply(noise_cu);
    // Get a line placement
    PassPtr line_place =
        gen_placement_pass(std::make_shared<LinePlacement>(line_arc));
    CompilationUnit line_cu((Circuit(circ)));
    line_place->apply(line_cu);
    // Get a fall back placement from a graph placement
    PassPtr graph_fall_back_place = gen_placement_pass(
        std::make_shared<GraphPlacement>(line_arc, 1000000, 0));
    CompilationUnit graph_fall_back_cu((Circuit(circ)));
    graph_fall_back_place->apply(graph_fall_back_cu);
    // Get a fall back placement from a noise -
    // aware placement
    PassPtr noise_fall_back_place = gen_placement_pass(
        std::make_shared<NoiseAwarePlacement>(
            line_arc, empty_node_errors, empty_link_errors,
            empty_readout_errors, 1000000, 0));
    CompilationUnit noise_fall_back_cu((Circuit(circ)));
    noise_fall_back_place->apply(noise_fall_back_cu);

    REQUIRE(graph_cu.get_final_map_ref() != line_cu.get_final_map_ref());
    REQUIRE(noise_cu.get_final_map_ref() != line_cu.get_final_map_ref());
    REQUIRE(
        graph_fall_back_cu.get_final_map_ref() == line_cu.get_final_map_ref());
    REQUIRE(
        noise_fall_back_cu.get_final_map_ref() == line_cu.get_final_map_ref());
  }
}

SCENARIO("gen_rename_qubits_pass test") {
  GIVEN("A circuit and a mapping") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    std::map<Qubit, Qubit> qm;
    Qubit newq0("newq0", 0);
    Qubit newq1("newq1", 1);
    Qubit newq2("newq2", 0);
    qm[Qubit(0)] = newq0;
    qm[Qubit(1)] = newq1;
    qm[Qubit(2)] = newq2;
    PassPtr pp = gen_rename_qubits_pass(qm);
    CompilationUnit cu(circ);
    REQUIRE(pp->apply(cu));
    const Circuit& newcirc = cu.get_circ_ref();
    Command cmd = newcirc.get_commands()[0];
    REQUIRE(cmd.get_args()[0] == newq0);
    REQUIRE(cmd.get_args()[1] == newq1);
  }
}

SCENARIO("PeepholeOptimise2Q and FullPeepholeOptimise") {
  GIVEN("A circuit with a Reset operation.") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::ZZMax, {1, 0});
    circ.add_op<unsigned>(OpType::CH, {0, 1});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    circ.add_op<unsigned>(OpType::Reset, {1});
    CompilationUnit cu(circ);
    REQUIRE(PeepholeOptimise2Q()->apply(cu));
    Circuit circ1 = circ;
    CompilationUnit cu1(circ1);
    REQUIRE(FullPeepholeOptimise()->apply(cu1));
  }
  GIVEN("A circuit with two CXs.") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    CompilationUnit cu(circ);
    REQUIRE(!PeepholeOptimise2Q(false)->apply(cu));
    qubit_map_t perm = cu.get_circ_ref().implicit_qubit_permutation();
    for (const std::pair<const Qubit, Qubit>& pair : perm) {
      REQUIRE(pair.first == pair.second);
    }
  }
  GIVEN("A circuit with classical operations.") {
    Circuit circ(2, 1);
    circ.add_op<unsigned>(OpType::ZZMax, {1, 0});
    circ.add_op<unsigned>(OpType::Reset, {1});
    circ.add_conditional_gate<unsigned>(OpType::Z, {}, {0}, {0}, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::U1, 0.2, {0});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    circ.add_op<unsigned>(ClassicalX(), {0});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 1}, {0}, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::V, {0});
    circ.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0}, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::U1, 0.4, {1});
    circ.add_op<unsigned>(ClassicalX(), {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(circ);
    REQUIRE(FullPeepholeOptimise()->apply(cu, SafetyMode::Audit));
  }
  GIVEN("A symbolic circuit") {
    Sym a = SymEngine::symbol("alpha");
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::ZZMax, {1, 0});
    circ.add_op<unsigned>(OpType::CH, {0, 1});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    circ.add_op<unsigned>(OpType::Ry, 2 * Expr(a), {1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    CompilationUnit cu(circ);
    REQUIRE(PeepholeOptimise2Q()->apply(cu));
    Circuit circ1 = circ;
    CompilationUnit cu1(circ1);
    REQUIRE(FullPeepholeOptimise()->apply(cu1));
  }
  GIVEN("Symbolic circuit, FullPeepholeOptimise TK2") {
    // https://github.com/quantinuum/tket/issues/963
    Sym a = SymEngine::symbol("a");
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, Expr(a), {0});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    CompilationUnit cu(circ);
    REQUIRE(FullPeepholeOptimise(true, OpType::TK2)->apply(cu));
  }
  GIVEN("YYPhase") {
    // TKET-1302
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::YYPhase, 1.00378, {0, 1});
    circ.add_op<unsigned>(OpType::CV, {0, 1});
    circ.add_op<unsigned>(OpType::CSX, {1, 0});
    CompilationUnit cu(circ);
    REQUIRE(PeepholeOptimise2Q()->apply(cu));
    REQUIRE(test_unitary_comparison(circ, cu.get_circ_ref()));
    Circuit circ1 = circ;
    CompilationUnit cu1(circ1);
    REQUIRE(FullPeepholeOptimise()->apply(cu1));
    REQUIRE(test_unitary_comparison(circ, cu.get_circ_ref()));
  }
  GIVEN("An X + BRIDGE circuit") {
    // https://github.com/quantinuum/tket/issues/9
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::BRIDGE, {0, 1, 2});
    CompilationUnit cu(circ);
    REQUIRE(FullPeepholeOptimise()->apply(cu));
    REQUIRE(test_unitary_comparison(circ, cu.get_circ_ref()));
  }
  GIVEN("A circuit targeting TK2.") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.2, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(circ);
    REQUIRE(FullPeepholeOptimise(true, OpType::TK2)->apply(cu));

    circ = cu.get_circ_ref();

    REQUIRE(circ.count_gates(OpType::TK2) == 1);
  }
  GIVEN("A circuit containing a Unitary1qBox") {
    // https://github.com/quantinuum/tket/issues/1750
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    Eigen::Matrix2cd u;
    u << 1.0, 0.0, 0.0, 1.0;
    Unitary1qBox ubox(u);
    circ.add_box(ubox, {2});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    CompilationUnit cu(circ);
    REQUIRE(FullPeepholeOptimise()->apply(cu));
  }
}

SCENARIO("FullPeepholeOptimise with various options") {
  GIVEN("Large 'random' circuit") {
    Circuit circ(4);
    RNG rng;
    for (unsigned i = 0; i < 100; i++) {
      unsigned a = rng.get_size_t(3);
      unsigned b = rng.get_size_t(3);
      unsigned c = rng.get_size_t(3);
      unsigned d = rng.get_size_t(3);
      circ.add_op<unsigned>(OpType::H, {a});
      circ.add_op<unsigned>(OpType::T, {b});
      if (c != d) {
        circ.add_op<unsigned>(OpType::CZ, {c, d});
      }
    }

    CompilationUnit cu_swaps_cx(circ);
    CompilationUnit cu_swaps_tk2(circ);
    CompilationUnit cu_noswaps_cx(circ);
    CompilationUnit cu_noswaps_tk2(circ);
    FullPeepholeOptimise(true, OpType::CX)->apply(cu_swaps_cx);
    FullPeepholeOptimise(true, OpType::TK2)->apply(cu_swaps_tk2);
    FullPeepholeOptimise(false, OpType::CX)->apply(cu_noswaps_cx);
    FullPeepholeOptimise(false, OpType::TK2)->apply(cu_noswaps_tk2);
    Circuit compiled_circ_swaps_cx = cu_swaps_cx.get_circ_ref();
    Circuit compiled_circ_swaps_tk2 = cu_swaps_tk2.get_circ_ref();
    Circuit compiled_circ_noswaps_cx = cu_noswaps_cx.get_circ_ref();
    Circuit compiled_circ_noswaps_tk2 = cu_noswaps_tk2.get_circ_ref();
    unsigned n_gates_swaps_cx = compiled_circ_swaps_cx.n_gates();
    unsigned n_cx_swaps_cx = compiled_circ_swaps_cx.count_gates(OpType::CX);
    unsigned n_tk1_swaps_cx = compiled_circ_swaps_cx.count_gates(OpType::TK1);
    unsigned n_gates_swaps_tk2 = compiled_circ_swaps_tk2.n_gates();
    unsigned n_tk2_swaps_tk2 = compiled_circ_swaps_tk2.count_gates(OpType::TK2);
    unsigned n_tk1_swaps_tk2 = compiled_circ_swaps_tk2.count_gates(OpType::TK1);
    unsigned n_gates_noswaps_cx = compiled_circ_noswaps_cx.n_gates();
    unsigned n_cx_noswaps_cx = compiled_circ_noswaps_cx.count_gates(OpType::CX);
    unsigned n_tk1_noswaps_cx =
        compiled_circ_noswaps_cx.count_gates(OpType::TK1);
    unsigned n_gates_noswaps_tk2 = compiled_circ_noswaps_tk2.n_gates();
    unsigned n_tk2_noswaps_tk2 =
        compiled_circ_noswaps_tk2.count_gates(OpType::TK2);
    unsigned n_tk1_noswaps_tk2 =
        compiled_circ_noswaps_tk2.count_gates(OpType::TK1);

    CHECK(n_gates_swaps_cx == n_cx_swaps_cx + n_tk1_swaps_cx);
    CHECK(n_gates_swaps_tk2 == n_tk2_swaps_tk2 + n_tk1_swaps_tk2);
    CHECK(n_gates_noswaps_cx == n_cx_noswaps_cx + n_tk1_noswaps_cx);
    CHECK(n_gates_noswaps_tk2 == n_tk2_noswaps_tk2 + n_tk1_noswaps_tk2);
  }
}

SCENARIO("rebase and decompose PhasePolyBox test") {
  GIVEN("rebase and decompose I") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::Y, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});

    CompilationUnit cu(circ);
    REQUIRE(ComposePhasePolyBoxes()->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(test_unitary_comparison(circ, result));
  }
  GIVEN("rebase and compose with custom registers") {
    Circuit circ;
    auto a_reg = circ.add_q_register("a", 2);
    auto b_reg = circ.add_q_register("b", 1);
    circ.add_op<UnitID>(OpType::CX, {a_reg[0], b_reg[0]});
    circ.add_op<UnitID>(OpType::CX, {a_reg[1], a_reg[0]});

    CompilationUnit cu(circ);
    REQUIRE(ComposePhasePolyBoxes()->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(test_unitary_comparison(circ, result));
  }
  GIVEN("rebase and decompose II") {
    Circuit circ(2, 2);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::Measure, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rx, 0.3, {1});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::Measure, {0, 0});
    CompilationUnit cu(circ);
    REQUIRE(ComposePhasePolyBoxes()->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(result.count_gates(OpType::CX) == 0);
    REQUIRE(result.count_gates(OpType::Rz) == 0);
    REQUIRE(result.count_gates(OpType::X) == 0);
    REQUIRE(result.count_gates(OpType::H) == 4);
    REQUIRE(result.count_gates(OpType::Measure) == 2);
    REQUIRE(result.count_gates(OpType::PhasePolyBox) == 4);
  }
  GIVEN("rebase and decompose III") {
    Circuit circ(8);
    circ.add_op<unsigned>(OpType::Rz, 0.5, {1});
    circ.add_op<unsigned>(OpType::Rx, 1.5, {2});
    circ.add_op<unsigned>(OpType::CX, {1, 7});
    circ.add_op<unsigned>(OpType::CX, {2, 4});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {1});
    circ.add_op<unsigned>(OpType::Rx, 0.5, {2});
    circ.add_op<unsigned>(OpType::CX, {1, 3});
    circ.add_op<unsigned>(OpType::X, {3});
    circ.add_op<unsigned>(OpType::X, {4});
    circ.add_op<unsigned>(OpType::CX, {5, 6});
    circ.add_op<unsigned>(OpType::CX, {6, 7});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {2});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    circ.add_op<unsigned>(OpType::Rz, 1.5, {0});
    circ.add_op<unsigned>(OpType::CX, {1, 3});
    circ.add_op<unsigned>(OpType::CX, {5, 0});
    circ.add_op<unsigned>(OpType::H, {1});
    add_2qb_gates(circ, OpType::CX, {{1, 4}, {2, 4}, {4, 7}, {3, 0}});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {0});
    circ.add_op<unsigned>(OpType::CX, {6, 3});
    circ.add_op<unsigned>(OpType::Rz, 1.5, {0});
    circ.add_op<unsigned>(OpType::CX, {4, 0});

    CompilationUnit cu(circ);
    REQUIRE(ComposePhasePolyBoxes()->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(test_unitary_comparison(circ, result));
  }
  GIVEN("Unsatisfied NoClassicalControlPredicate") {
    Circuit c(1, 1);
    c.add_conditional_gate<unsigned>(OpType::H, {}, {0}, {0}, 1);
    CompilationUnit cu(c);
    REQUIRE_THROWS_AS(ComposePhasePolyBoxes()->apply(cu), UnsatisfiedPredicate);
  }
  GIVEN("NoWireSwapsPredicate for ComposePhasePolyBoxes") {
    Circuit circ(5);
    add_2qb_gates(circ, OpType::CX, {{0, 3}, {1, 4}});
    circ.add_op<unsigned>(OpType::SWAP, {3, 4});
    circ.add_op<unsigned>(OpType::Z, {3});

    REQUIRE(NoWireSwapsPredicate().verify(circ));
    REQUIRE(circ.replace_SWAPs());
    REQUIRE(!NoWireSwapsPredicate().verify(circ));

    CompilationUnit cu(circ);
    REQUIRE(ComposePhasePolyBoxes()->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(test_unitary_comparison(result, circ));
    REQUIRE(NoWireSwapsPredicate().verify(result));
  }
  GIVEN("NoWireSwapsPredicate for ComposePhasePolyBoxes II") {
    Circuit circ(5);
    add_2qb_gates(circ, OpType::CX, {{0, 3}, {1, 4}});
    circ.add_op<unsigned>(OpType::SWAP, {3, 4});
    circ.add_op<unsigned>(OpType::Z, {3});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {1});
    circ.add_op<unsigned>(OpType::Z, {4});

    REQUIRE(NoWireSwapsPredicate().verify(circ));
    REQUIRE(circ.replace_SWAPs());
    REQUIRE(!NoWireSwapsPredicate().verify(circ));

    CompilationUnit cu(circ);
    REQUIRE(ComposePhasePolyBoxes()->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(test_unitary_comparison(result, circ));
    REQUIRE(NoWireSwapsPredicate().verify(result));
  }
  GIVEN("NoWireSwapsPredicate for ComposePhasePolyBoxes III") {
    Circuit circ(5);
    add_2qb_gates(circ, OpType::CX, {{0, 3}, {1, 4}});
    circ.add_op<unsigned>(OpType::SWAP, {3, 4});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::Z, {3});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::Z, {4});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 3});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::CX, {2, 3});

    REQUIRE(NoWireSwapsPredicate().verify(circ));
    REQUIRE(circ.replace_SWAPs());
    REQUIRE(!NoWireSwapsPredicate().verify(circ));

    CompilationUnit cu(circ);
    REQUIRE(ComposePhasePolyBoxes()->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(test_unitary_comparison(result, circ));
    REQUIRE(NoWireSwapsPredicate().verify(result));
  }
  GIVEN("NoWireSwapsPredicate for ComposePhasePolyBoxes min size") {
    Circuit circ(5);
    add_2qb_gates(circ, OpType::CX, {{0, 3}, {1, 4}});
    circ.add_op<unsigned>(OpType::SWAP, {3, 4});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::Z, {3});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::Z, {4});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 3});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::CX, {2, 3});

    REQUIRE(NoWireSwapsPredicate().verify(circ));
    REQUIRE(circ.replace_SWAPs());
    REQUIRE(!NoWireSwapsPredicate().verify(circ));

    CompilationUnit cu(circ);
    REQUIRE(ComposePhasePolyBoxes(5)->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(result.count_gates(OpType::H) == 3);
    REQUIRE(result.count_gates(OpType::CX) == 2);
    REQUIRE(result.count_gates(OpType::SWAP) == 0);
    REQUIRE(result.count_gates(OpType::Z) == 0);
    REQUIRE(result.count_gates(OpType::PhasePolyBox) == 2);

    REQUIRE(test_unitary_comparison(result, circ));
    REQUIRE(NoWireSwapsPredicate().verify(result));
  }
  GIVEN("NoWireSwapsPredicate for ComposePhasePolyBoxes min size ii") {
    Circuit circ(5);
    add_2qb_gates(circ, OpType::CX, {{0, 3}, {1, 4}});
    circ.add_op<unsigned>(OpType::SWAP, {3, 4});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::Z, {3});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::Z, {4});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 3});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::CX, {2, 3});

    REQUIRE(NoWireSwapsPredicate().verify(circ));
    REQUIRE(circ.replace_SWAPs());
    REQUIRE(!NoWireSwapsPredicate().verify(circ));

    CompilationUnit cu(circ);
    REQUIRE(ComposePhasePolyBoxes(6)->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(result.count_gates(OpType::H) == 3);
    REQUIRE(result.count_gates(OpType::CX) == 7);
    REQUIRE(result.count_gates(OpType::SWAP) == 0);
    REQUIRE(result.count_gates(OpType::Z) == 0);
    REQUIRE(result.count_gates(OpType::PhasePolyBox) == 1);

    REQUIRE(test_unitary_comparison(result, circ));
    REQUIRE(NoWireSwapsPredicate().verify(result));
  }
  GIVEN("NoWireSwapsPredicate for aas I") {
    std::vector<Node> nodes = {Node(0), Node(1), Node(2), Node(3), Node(4)};
    Architecture architecture(
        {{nodes[0], nodes[1]},
         {nodes[1], nodes[2]},
         {nodes[2], nodes[3]},
         {nodes[3], nodes[4]}});

    Circuit circ(5);
    add_2qb_gates(circ, OpType::CX, {{0, 3}, {1, 4}});
    circ.add_op<unsigned>(OpType::SWAP, {3, 4});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::Z, {3});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::Z, {4});
    circ.add_op<unsigned>(OpType::CX, {1, 4});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 3});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::CX, {2, 3});

    REQUIRE(NoWireSwapsPredicate().verify(circ));
    REQUIRE(circ.replace_SWAPs());
    REQUIRE(!NoWireSwapsPredicate().verify(circ));

    CompilationUnit cu(circ);

    REQUIRE(gen_full_mapping_pass_phase_poly(
                architecture, 1, aas::CNotSynthType::Rec)
                ->apply(cu));
    Circuit result = cu.get_circ_ref();

    REQUIRE(test_unitary_comparison(result, circ));
    REQUIRE(NoWireSwapsPredicate().verify(result));
  }
  GIVEN("A circuit with some global phase to be preserved") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::ZZMax, {2, 1});
    circ.add_op<unsigned>(OpType::Phase, 0.974579, std::vector<unsigned>{});
    circ.add_op<unsigned>(OpType::CCX, {0, 1, 2});
    circ.add_phase(0.23428);
    CompilationUnit cu(circ);
    ComposePhasePolyBoxes()->apply(cu);
    REQUIRE(test_unitary_comparison(circ, cu.get_circ_ref()));
  }
}

SCENARIO("DecomposeArbitrarilyControlledGates test") {
  GIVEN("A CCX gate") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CCX, {2, 0, 1});
    CompilationUnit cu(circ);
    REQUIRE(DecomposeArbitrarilyControlledGates()->apply(cu));
  }
}

SCENARIO("Precomposed passes successfully compose") {
  GIVEN("gen_directed_cx_routing_pass") {
    RingArch arc(6);
    REQUIRE_NOTHROW(gen_directed_cx_routing_pass(
        arc, {std::make_shared<LexiLabellingMethod>(),
              std::make_shared<LexiRouteRoutingMethod>()}));
  }
}

SCENARIO("Test Pauli Graph Synthesis Pass") {
  PassPtr graph_synth = gen_synthesise_pauli_graph(
      Transforms::PauliSynthStrat::Sets, CXConfigType::Star);
  GIVEN("Two PauliExpBoxes") {
    Circuit circ(3, "test");
    PauliExpBox peb({{Pauli::Z, Pauli::X, Pauli::Z}, 0.333});
    circ.add_box(peb, {0, 1, 2});
    PauliExpBox peb2({{Pauli::Y, Pauli::X, Pauli::X}, 0.174});
    circ.add_box(peb2, {0, 1, 2});

    CompilationUnit cu(circ);
    graph_synth->apply(cu);
    const Circuit& circ1 = cu.get_circ_ref();

    REQUIRE(test_unitary_comparison(circ, circ1));
    REQUIRE(circ1.get_name() == "test");
  }
  GIVEN("Lots of different gates") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::Z, {0});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::Y, {2});
    circ.add_op<unsigned>(OpType::S, {0});
    circ.add_op<unsigned>(OpType::Sdg, {1});
    circ.add_op<unsigned>(OpType::V, {2});
    circ.add_op<unsigned>(OpType::Vdg, {0});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::CX, {2, 0});
    circ.add_op<unsigned>(OpType::CY, {0, 1});
    circ.add_op<unsigned>(OpType::CZ, {1, 2});
    circ.add_op<unsigned>(OpType::SWAP, {2, 0});
    circ.add_op<unsigned>(OpType::Rz, 0.25, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.25, {1});
    circ.add_op<unsigned>(OpType::Ry, 0.25, {2});
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::Tdg, {1});
    circ.add_op<unsigned>(OpType::ZZMax, {2, 0});
    circ.add_op<unsigned>(OpType::ZZPhase, 0.25, {0, 1});
    circ.add_op<unsigned>(OpType::PhaseGadget, 0.25, {0, 1, 2});
    circ.add_op<unsigned>(OpType::XXPhase, 0.25, {1, 2});
    circ.add_op<unsigned>(OpType::YYPhase, 0.25, {2, 0});
    circ.add_op<unsigned>(OpType::PhasedX, {0.25, 1.75}, {0});
    // ... and some with Clifford angles...
    circ.add_op<unsigned>(OpType::Rz, 0.5, {0});
    circ.add_op<unsigned>(OpType::Rx, 1.0, {1});
    circ.add_op<unsigned>(OpType::Ry, 1.5, {2});
    circ.add_op<unsigned>(OpType::ZZPhase, 0.5, {0, 1});
    circ.add_op<unsigned>(OpType::PhaseGadget, 1.0, {0, 1, 2});
    circ.add_op<unsigned>(OpType::XXPhase, 1.5, {1, 2});
    circ.add_op<unsigned>(OpType::YYPhase, 2.5, {2, 0});
    circ.add_op<unsigned>(OpType::PhasedX, {3.5, 0.5}, {0});
    circ.add_op<unsigned>(OpType::SX, {1});
    circ.add_op<unsigned>(OpType::SXdg, {2});

    CompilationUnit cu(circ);
    graph_synth->apply(cu);

    REQUIRE(test_unitary_comparison(circ, cu.get_circ_ref(), true));
  }
  GIVEN("Implicit qubit permutation") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    Transforms::clifford_simp().apply(circ);
    REQUIRE(circ.has_implicit_wireswaps());

    CompilationUnit cu(circ);
    graph_synth->apply(cu);

    REQUIRE(test_unitary_comparison(circ, cu.get_circ_ref(), true));
  }
}

SCENARIO("Compose Pauli Graph synthesis Passes") {
  RingArch arc(10);
  PassPtr dir_pass = gen_directed_cx_routing_pass(
      arc, {std::make_shared<LexiLabellingMethod>(),
            std::make_shared<LexiRouteRoutingMethod>()});
  GIVEN("Special UCC Synthesis") {
    PassPtr spec_ucc = gen_special_UCC_synthesis();
    REQUIRE_NOTHROW(spec_ucc >> dir_pass);
  }
  GIVEN("Pauli Graph synthesis") {
    PassPtr graph_synth = gen_synthesise_pauli_graph(
        Transforms::PauliSynthStrat::Sets, CXConfigType::Star);
    REQUIRE_NOTHROW(graph_synth >> dir_pass);
  }
  GIVEN("Pairwise Synthesis") {
    PassPtr pairwise = gen_pairwise_pauli_gadgets(CXConfigType::Tree);
    REQUIRE_NOTHROW(pairwise >> dir_pass);
  }
}

SCENARIO("Commute measurements to the end of a circuit") {
  PassPtr delay_pass = DelayMeasures();
  PassPtr try_delay_pass = DelayMeasures(true);
  PredicatePtr mid_meas_pred = std::make_shared<NoMidMeasurePredicate>();
  GIVEN("Measurements already at end") {
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::Z, {0});
    c.add_op<unsigned>(OpType::CX, {1, 0});
    c.add_op<unsigned>(OpType::Rx, 0.3, {1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Measure, {1, 1});
    CompilationUnit cu(c);
    REQUIRE_FALSE(delay_pass->apply(cu));
    REQUIRE(mid_meas_pred->verify(cu.get_circ_ref()));
  }
  GIVEN("Gates after measure") {
    Circuit inner(2);
    inner.add_op<unsigned>(OpType::ZZMax, {0, 1});
    inner.add_op<unsigned>(OpType::Y, {1});
    Op_ptr box_op = std::make_shared<CircBox>(inner);
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::Measure, {0, 1});
    c.add_op<unsigned>(OpType::Z, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::Rx, 0.3, {1});
    c.add_conditional_gate<unsigned>(OpType::CZ, {}, {0, 1}, {0}, 1);
    c.add_op<UnitID>(box_op, {Qubit(0), Qubit(1)});
    c.add_op<unsigned>(OpType::SWAP, {0, 1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    CompilationUnit cu(c);
    REQUIRE(delay_pass->apply(cu));
    REQUIRE(mid_meas_pred->verify(cu.get_circ_ref()));
    Circuit expected(2, 2);
    expected.add_op<unsigned>(OpType::Z, {0});
    expected.add_op<unsigned>(OpType::CX, {0, 1});
    expected.add_op<unsigned>(OpType::Rx, 0.3, {1});
    expected.add_conditional_gate<unsigned>(OpType::CZ, {}, {0, 1}, {0}, 1);
    expected.add_op<UnitID>(box_op, {Qubit(0), Qubit(1)});
    expected.add_op<unsigned>(OpType::SWAP, {0, 1});
    expected.add_op<unsigned>(OpType::Measure, {0, 0});
    expected.add_op<unsigned>(OpType::Measure, {1, 1});
    REQUIRE(cu.get_circ_ref() == expected);
  }
  GIVEN("Measure blocked by quantum gate") {
    Circuit c(1, 1);
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Rx, 0.3, {0});
    CompilationUnit cu(c);
    REQUIRE_THROWS_AS(delay_pass->apply(cu), UnsatisfiedPredicate);
  }
  GIVEN("Measure blocked by quantum gate, using a partial delay pass") {
    Circuit c(1, 1);
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Rx, 0.3, {0});
    CompilationUnit cu(c);
    REQUIRE_FALSE(try_delay_pass->apply(cu));
  }
  GIVEN("Measure blocked by classical operation") {
    Circuit c(2, 1);
    add_2qb_gates(c, OpType::Measure, {{0, 0}, {1, 0}});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Measure, {1, 0});
    CompilationUnit cu(c);
    REQUIRE_THROWS_AS(delay_pass->apply(cu), UnsatisfiedPredicate);
  }
  GIVEN("Measure blocked by classical operation, using a partial delay pass") {
    Circuit c(2, 1);
    add_2qb_gates(c, OpType::Measure, {{0, 0}, {1, 0}});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Measure, {1, 0});
    CompilationUnit cu(c);
    REQUIRE_FALSE(try_delay_pass->apply(cu));
  }
  GIVEN("Measure blocked by conditional operation") {
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::CZ, {0, 1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_conditional_gate<unsigned>(OpType::Z, {}, {1}, {0}, 1);
    CompilationUnit cu(c);
    REQUIRE_THROWS_AS(delay_pass->apply(cu), UnsatisfiedPredicate);
  }
  GIVEN(
      "Measure partially blocked by conditional operation, using a partial "
      "delay pass") {
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::CZ, {0, 1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Z, {0});
    c.add_conditional_gate<unsigned>(OpType::Z, {}, {1}, {0}, 1);
    CompilationUnit cu(c);
    REQUIRE(try_delay_pass->apply(cu));
  }
  GIVEN("Measure blocked by conditional SWAP") {
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::CZ, {0, 1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_conditional_gate<unsigned>(OpType::SWAP, {}, {0, 1}, {1}, 1);
    CompilationUnit cu(c);
    REQUIRE_THROWS_AS(delay_pass->apply(cu), UnsatisfiedPredicate);
  }
  GIVEN("Measure partially blocked by conditional SWAP") {
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::CZ, {0, 1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_op<unsigned>(OpType::Z, {0});
    c.add_conditional_gate<unsigned>(OpType::SWAP, {}, {0, 1}, {1}, 1);
    CompilationUnit cu(c);
    REQUIRE(try_delay_pass->apply(cu));
  }
  GIVEN("Call on invalid circuit without checking the predicate throws") {
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::CZ, {0, 1});
    c.add_op<unsigned>(OpType::Measure, {0, 0});
    c.add_conditional_gate<unsigned>(OpType::Z, {}, {1}, {0}, 1);
    REQUIRE_THROWS_AS(Transforms::delay_measures().apply(c), CircuitInvalidity);
  }
  GIVEN(
      "Call on invalid nested circuit without checking the predicate throws") {
    Circuit inner1(1, 2);
    inner1.add_conditional_gate<unsigned>(OpType::Measure, {}, {0, 0}, {1}, 1);
    CircBox cbox1(inner1);

    Circuit inner2(1, 2);
    inner2.add_box(cbox1, {0, 0, 1});
    CircBox cbox2(inner2);

    Circuit c(1, 2);
    c.add_box(cbox2, {0, 0, 1});
    c.add_op<unsigned>(OpType::X, {0});
    REQUIRE_THROWS_AS(Transforms::delay_measures().apply(c), CircuitInvalidity);
  }
  GIVEN("Combined with routing") {
    Circuit test(3, 1);
    add_2qb_gates(test, OpType::CX, {{0, 1}, {1, 2}});
    add_1qb_gates(test, OpType::X, {0, 0});
    test.add_measure(1, 0);
    add_1qb_gates(test, OpType::X, {2, 2});
    test.add_op<unsigned>(OpType::CX, {0, 2});

    Architecture line(
        {{Node(0), Node(1)}, {Node(1), Node(2)}, {Node(2), Node(3)}});
    Placement::Ptr pp = std::make_shared<Placement>(line);
    PassPtr route_pass = gen_full_mapping_pass(
        line, pp,
        {std::make_shared<LexiLabellingMethod>(),
         std::make_shared<LexiRouteRoutingMethod>()});
    CompilationUnit cu(test);
    route_pass->apply(cu);
    REQUIRE(delay_pass->apply(cu));
    Command final_command = cu.get_circ_ref().get_commands()[7];
    OpType type = final_command.get_op_ptr()->get_type();
    REQUIRE(type == OpType::Measure);
    // REQUIRE(final_command.get_args().front() == Node(3));
  }
  GIVEN("Measures targeting the same bit") {
    // https://github.com/quantinuum/tket/issues/1305
    Circuit c(4, 1);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_measure(3, 0);
    c.add_measure(1, 0);
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::CCX, {1, 3, 2});
    CompilationUnit cu(c);
    CHECK_FALSE(try_delay_pass->apply(cu));
  }
}

SCENARIO("RemoveRedundancies and phase") {
  GIVEN("A trivial circuit with nonzero phase") {
    // TKET-679
    Circuit c(1);
    c.add_op<unsigned>(OpType::TK1, {1., 0., 1.}, {0});
    CompilationUnit cu(c);
    REQUIRE(RemoveRedundancies()->apply(cu));
    const Circuit& c1 = cu.get_circ_ref();
    REQUIRE(c1.get_commands().size() == 0);
    REQUIRE(equiv_val(c1.get_phase(), 1.));
  }
  GIVEN("A circuit with a trivial TK2 gate and nonzero phase") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::TK2, {0., 2., 4.}, {0, 1});
    CompilationUnit cu(c);
    REQUIRE(RemoveRedundancies()->apply(cu));
    const Circuit& c1 = cu.get_circ_ref();
    REQUIRE(c1.get_commands().size() == 0);
    REQUIRE(equiv_val(c1.get_phase(), 1.));
  }
  GIVEN("A circuit with a trivial TK2 gate and nonzero phase") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::TK2, {0., 2., 4.}, {0, 1});
    CompilationUnit cu(c);
    REQUIRE(RemoveRedundancies()->apply(cu));
    const Circuit& c1 = cu.get_circ_ref();
    REQUIRE(c1.get_commands().size() == 0);
    REQUIRE(equiv_val(c1.get_phase(), 1.));
  }
  GIVEN("A circuit with a Phase gate") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::Phase, 0.25, {});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(c);
    REQUIRE(RemoveRedundancies()->apply(cu));
    REQUIRE(cu.get_circ_ref().n_gates() == 2);
  }
}

// Check whether a circuit maps all basis states to basis states.
// All compiler passes should preserve this property.
static bool is_classical_map(const Circuit& c) {
  const Eigen::MatrixXcd u = tket_sim::get_unitary(c);
  return std::all_of(
      u.data(), u.data() + u.size(), [](const std::complex<double>& x) {
        double r = std::abs(x);
        return r < ERR_EPS || r > 1 - ERR_EPS;
      });
}

SCENARIO("CX mapping pass") {
  // TKET-1045
  GIVEN("A device with a linear architecture") {
    Architecture line(
        {{Node(0), Node(1)},
         {Node(1), Node(2)},
         {Node(2), Node(3)},
         {Node(3), Node(4)}});

    // Noise-aware placement and rebase
    Placement::Ptr placer = std::make_shared<GraphPlacement>(line);
    Circuit cx(2);
    cx.add_op<unsigned>(OpType::CX, {0, 1});
    OpTypeSet gateset = all_single_qubit_types();
    gateset.insert(OpType::CX);
    PassPtr rebase = gen_rebase_pass(gateset, cx, CircPool::tk1_to_tk1);

    // Circuit mapping basis states to basis states
    Circuit c(3);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::CCX, {2, 1, 0});
    c.add_op<unsigned>(OpType::CY, {1, 0});
    c.add_op<unsigned>(OpType::CY, {2, 1});
    REQUIRE(is_classical_map(c));

    // Rebase
    CompilationUnit cu_rebase(c);
    REQUIRE(rebase->apply(cu_rebase));
    const Circuit& c_rebased = cu_rebase.get_circ_ref();
    REQUIRE(is_classical_map(c_rebased));

    // Place
    CompilationUnit cu_place(c_rebased);
    gen_placement_pass(placer)->apply(cu_place);
    const Circuit& c_placed = cu_place.get_circ_ref();
    REQUIRE(is_classical_map(c_placed));

    // Route
    LexiRouteRoutingMethod lrrm(50);
    RoutingMethodPtr rmw = std::make_shared<LexiRouteRoutingMethod>(lrrm);
    CompilationUnit cu_route(c_placed);
    gen_routing_pass(
        line, {std::make_shared<LexiLabellingMethod>(),
               std::make_shared<LexiRouteRoutingMethod>()})
        ->apply(cu_route);
    const Circuit& c_routed = cu_route.get_circ_ref();

    // Rebase again
    CompilationUnit cu(c_routed);
    rebase->apply(cu);
    const Circuit& c1 = cu.get_circ_ref();
    c1.assert_valid();
    REQUIRE(is_classical_map(c1));
  }
  // SEE TKET ISSUE 475
  GIVEN("A circuit with a barrier and an Ancilla that needs relabelling.") {
    Circuit circ(25);
    add_2qb_gates(
        circ, OpType::CX,
        {{2, 1},
         {3, 7},
         {0, 3},
         {6, 9},
         {7, 15},
         {16, 6},
         {18, 12},
         {7, 19},
         {4, 21},
         {18, 4},
         {23, 11},
         {17, 24},
         {8, 13}});
    circ.add_barrier({0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                      13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24});
    add_2qb_gates(circ, OpType::CX, {{2, 1}, {23, 19}, {23, 11}});

    std::vector<std::pair<unsigned, unsigned>> edges = {
        {0, 1},   {0, 5},   {0, 6},   {1, 0},   {1, 2},   {1, 5},   {1, 6},
        {1, 7},   {2, 1},   {2, 3},   {2, 6},   {2, 7},   {2, 8},   {3, 2},
        {3, 4},   {3, 7},   {3, 8},   {3, 9},   {4, 3},   {4, 8},   {4, 9},
        {5, 0},   {5, 1},   {5, 6},   {5, 10},  {5, 11},  {6, 0},   {6, 1},
        {6, 2},   {6, 5},   {6, 7},   {6, 10},  {6, 11},  {6, 12},  {7, 1},
        {7, 2},   {7, 3},   {7, 6},   {7, 8},   {7, 11},  {7, 12},  {7, 13},
        {8, 2},   {8, 3},   {8, 4},   {8, 7},   {8, 9},   {8, 12},  {8, 13},
        {8, 14},  {9, 3},   {9, 4},   {9, 8},   {9, 13},  {9, 14},  {10, 5},
        {10, 6},  {10, 11}, {10, 15}, {10, 16}, {11, 5},  {11, 6},  {11, 7},
        {11, 10}, {11, 12}, {11, 15}, {11, 16}, {11, 17}, {12, 6},  {12, 7},
        {12, 8},  {12, 11}, {12, 13}, {12, 16}, {12, 17}, {12, 18}, {13, 7},
        {13, 8},  {13, 9},  {13, 12}, {13, 14}, {13, 17}, {13, 18}, {13, 19},
        {14, 8},  {14, 9},  {14, 13}, {14, 18}, {14, 19}, {15, 10}, {15, 11},
        {15, 16}, {15, 20}, {15, 21}, {16, 10}, {16, 11}, {16, 12}, {16, 15},
        {16, 17}, {16, 20}, {16, 21}, {16, 22}, {17, 11}, {17, 12}, {17, 13},
        {17, 16}, {17, 18}, {17, 21}, {17, 22}, {17, 23}, {18, 12}, {18, 13},
        {18, 14}, {18, 17}, {18, 19}, {18, 22}, {18, 23}, {18, 24}, {19, 13},
        {19, 14}, {19, 18}, {19, 23}, {19, 24}, {20, 15}, {20, 16}, {20, 21},
        {21, 15}, {21, 16}, {21, 17}, {21, 20}, {21, 22}, {22, 16}, {22, 17},
        {22, 18}, {22, 21}, {22, 23}, {23, 17}, {23, 18}, {23, 19}, {23, 22},
        {23, 24}, {24, 18}, {24, 19}, {24, 23},
    };
    Architecture arc(edges);
    PassPtr r_p = gen_routing_pass(
        arc, {std::make_shared<LexiLabellingMethod>(),
              std::make_shared<LexiRouteRoutingMethod>()});
    CompilationUnit cu(circ);
    r_p->apply(cu);
    // In case where this failed, the IR had a cycle so get_commands()
    // would produced a segmentation fault
    cu.get_circ_ref().get_commands();
    // Therefore this REQUIRE confirms that is not happening
    REQUIRE(true);
  }
  GIVEN("A circuit with a barrier and internal measurements.") {
    Circuit circ(2);
    Bit id(0);
    circ.add_bit(id, false);
    circ.add_measure(Qubit(0), id);
    circ.add_barrier({0, 1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    std::vector<std::pair<unsigned, unsigned>> edges = {{0, 1}};
    Architecture arc(edges);
    Placement::Ptr plptr = std::make_shared<Placement>(arc);
    std::vector<RoutingMethodPtr> config = {
        std::make_shared<LexiRouteRoutingMethod>()};
    THEN("Mapping with delay_measurements fails on the predicate.") {
      CompilationUnit cu(circ);
      PassPtr pass = gen_cx_mapping_pass(arc, plptr, config, false, true);
      REQUIRE_THROWS_AS(pass->apply(cu), UnsatisfiedPredicate);
    }
  }
  GIVEN("A circuit with measurements inside boxes.") {
    Circuit inner1(1, 2);
    inner1.add_conditional_gate<unsigned>(OpType::Measure, {}, {0, 0}, {1}, 1);
    CircBox cbox1(inner1);

    Circuit inner2(1, 2);
    inner2.add_box(cbox1, {0, 0, 1});
    CircBox cbox2(inner2);

    Circuit circ(1, 2);
    circ.add_box(cbox2, {0, 0, 1});
    circ.add_op<unsigned>(OpType::X, {0});

    std::vector<std::pair<unsigned, unsigned>> edges = {};
    Architecture arc(edges);
    Placement::Ptr plptr = std::make_shared<Placement>(arc);
    std::vector<RoutingMethodPtr> config = {
        std::make_shared<LexiRouteRoutingMethod>()};
    THEN("Mapping with delay_measurements fails on the predicate.") {
      CompilationUnit cu(circ);
      PassPtr pass = gen_cx_mapping_pass(arc, plptr, config, false, true);
      REQUIRE_THROWS_AS(pass->apply(cu), UnsatisfiedPredicate);
    }
  }
  GIVEN("CXMappingPass preconditions") {
    // https://github.com/quantinuum/tket/issues/1597
    // Construct a circuit with a barrier and an implicit wire swap; check that
    // we can apply CXMappingPass.
    Circuit c(2, 2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::CX, {1, 0});
    c.add_barrier({0, 1});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_measure(0, 0);
    c.add_measure(1, 1);
    Transforms::clifford_simp().apply(c);
    CHECK(c.has_implicit_wireswaps());
    std::vector<std::pair<unsigned, unsigned>> edges = {{0, 1}};
    Architecture arc(edges);
    Placement::Ptr plptr = std::make_shared<Placement>(arc);
    std::vector<RoutingMethodPtr> config = {
        std::make_shared<LexiRouteRoutingMethod>()};
    PassPtr p = gen_cx_mapping_pass(arc, plptr, config, true, false);
    CompilationUnit cu(c);
    REQUIRE_NOTHROW(p->apply(cu));
  }
}

SCENARIO("ThreeQubitSquah") {
  GIVEN("A 3-qubit circuit that can be squashed") {
    Circuit c(3);
    for (unsigned i = 0; i < 21; i++) {
      c.add_op<unsigned>(OpType::H, {i % 3});
      c.add_op<unsigned>(OpType::CX, {i % 3, (i + 1) % 3});
      c.add_op<unsigned>(OpType::Rz, 0.25, {(i + 1) % 3});
    }
    CompilationUnit cu(c);
    REQUIRE(ThreeQubitSquash()->apply(cu));
    const Circuit& c1 = cu.get_circ_ref();
    REQUIRE(c1.count_gates(OpType::CX) <= 19);
    REQUIRE(test_statevector_comparison(c, c1));
  }
  GIVEN("Unsatisfied gateset") {
    Circuit c(3);
    c.add_op<unsigned>(OpType::CH, {0, 1});
    CompilationUnit cu(c);
    REQUIRE_THROWS_AS(ThreeQubitSquash()->apply(cu), UnsatisfiedPredicate);
  }
  GIVEN("A 3-qubit circuit that is non-trivially the identity") {
    Circuit c(3);
    c.add_op<unsigned>(OpType::U3, {1.5, 0, 1.5}, {0});
    c.add_op<unsigned>(OpType::U3, {0.5, 0.75, 1.25}, {1});
    c.add_op<unsigned>(OpType::U3, {0.5, 0, 1}, {2});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::U3, {0.25, 0.25, 1.75}, {1});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::U3, {0.5, 0, 0.25}, {1});
    c.add_op<unsigned>(OpType::U3, {3.5, 1.75, 0}, {2});
    c.add_op<unsigned>(OpType::CX, {2, 0});
    c.add_op<unsigned>(OpType::U3, {0.5, 1.75, 0}, {0});
    c.add_op<unsigned>(OpType::CX, {1, 0});
    c.add_op<unsigned>(OpType::U1, 0.25, {0});
    c.add_op<unsigned>(OpType::CX, {2, 0});
    c.add_op<unsigned>(OpType::U1, 0.25, {0});
    c.add_op<unsigned>(OpType::CX, {1, 0});
    c.add_op<unsigned>(OpType::U3, {1.5, 1.5, 1.75}, {0});
    c.add_op<unsigned>(OpType::U3, {0.5, 0.75, 1.25}, {1});
    c.add_op<unsigned>(OpType::CX, {2, 0});
    c.add_op<unsigned>(OpType::U1, 0.5, {0});
    c.add_op<unsigned>(OpType::U3, {0.5, 0, 0.5}, {2});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::U3, {0.25, 0.25, 1.75}, {1});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::U3, {0.5, 0, 0.25}, {1});
    c.add_op<unsigned>(OpType::U3, {3.5, 0.25, 0}, {2});
    c.add_op<unsigned>(OpType::CX, {2, 0});
    c.add_op<unsigned>(OpType::U3, {0.5, 1.75, 0}, {0});
    c.add_op<unsigned>(OpType::CX, {1, 0});
    c.add_op<unsigned>(OpType::U1, 0.25, {0});
    c.add_op<unsigned>(OpType::CX, {2, 0});
    c.add_op<unsigned>(OpType::U1, 0.25, {0});
    c.add_op<unsigned>(OpType::CX, {1, 0});
    c.add_op<unsigned>(OpType::U1, 0.25, {0});
    c.add_op<unsigned>(OpType::CX, {2, 0});
    c.add_phase(0.25);

    CompilationUnit cu(c);
    REQUIRE(ThreeQubitSquash()->apply(cu));
    const Circuit& c1 = cu.get_circ_ref();
    REQUIRE(c1.get_commands().empty());
  }
}

SCENARIO("CustomPass") {
  GIVEN("Identity transform") {
    auto transform = [](const Circuit& c) {
      Circuit c1 = c;
      return c1;
    };
    PassPtr pp = CustomPass(transform);
    Circuit c(2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(c);
    REQUIRE_FALSE(pp->apply(cu));
    REQUIRE(cu.get_circ_ref() == c);
  }
  GIVEN("Custom pass that ignores ops with small params") {
    auto transform = [](const Circuit& c) {
      Circuit c1;
      for (const Qubit& qb : c.all_qubits()) {
        c1.add_qubit(qb);
      }
      for (const Bit& cb : c.all_bits()) {
        c1.add_bit(cb);
      }
      for (const Command& cmd : c.get_commands()) {
        Op_ptr op = cmd.get_op_ptr();
        unit_vector_t args = cmd.get_args();
        std::vector<Expr> params = op->get_params();
        if (params.empty() ||
            std::any_of(params.begin(), params.end(), [](const Expr& e) {
              return !approx_0(e, 0.01);
            })) {
          c1.add_op<UnitID>(op, args);
        }
      }
      return c1;
    };
    PassPtr pp = CustomPass(transform);
    THEN("The pass eliminates small-angle rotations") {
      Circuit c(2);
      c.add_op<unsigned>(OpType::Rx, 0.001, {0});
      c.add_op<unsigned>(OpType::CZ, {0, 1});
      CompilationUnit cu(c);
      REQUIRE(pp->apply(cu));
      REQUIRE(cu.get_circ_ref().n_gates() == 1);
    }
    AND_WHEN("The pass is followed by RemoveRedundancies") {
      SequencePass seq({pp, RemoveRedundancies()});
      THEN("It can reduce circuits even further") {
        Circuit c(1);
        c.add_op<unsigned>(OpType::Rx, 0.25, {0});
        c.add_op<unsigned>(OpType::Rz, 0.001, {0});
        c.add_op<unsigned>(OpType::Rx, 0.25, {0});
        CompilationUnit cu(c);
        REQUIRE(seq.apply(cu));
        REQUIRE(cu.get_circ_ref().n_gates() == 1);
      }
    }
  }
  GIVEN("CustomPassMap with function that relabels qubits.") {
    auto t = [](const Circuit& circ) {
      Circuit copy = circ;
      unit_map_t initial_m = {{Qubit(0), Qubit(1)}, {Qubit(1), Qubit(0)}};
      unit_map_t final_m = {{Qubit(0), Qubit(1)}, {Qubit(1), Qubit(0)}};
      std::pair<Circuit, std::pair<unit_map_t, unit_map_t>> ret = {
          copy, {initial_m, final_m}};
      return ret;
    };
    PassPtr pp = CustomPassMap(t);

    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(c);
    pp->apply(cu);

    unit_bimap_t cu_initial = cu.get_initial_map_ref();
    unit_bimap_t cu_final = cu.get_final_map_ref();

    TKET_ASSERT(cu_initial.left.find(Qubit(0))->second == Qubit(1));
    TKET_ASSERT(cu_initial.left.find(Qubit(1))->second == Qubit(0));
    TKET_ASSERT(cu_final.left.find(Qubit(0))->second == Qubit(1));
    TKET_ASSERT(cu_final.left.find(Qubit(1))->second == Qubit(0));
  }
}

SCENARIO("Flatten and relabel registers") {
  GIVEN("No empty wires, qubits from same register.") {
    Circuit c(3);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::Z, {2});
    CompilationUnit cu(c);

    PassPtr pp = gen_flatten_relabel_registers_pass("a");
    REQUIRE(pp->apply(cu));

    unit_bimap_t cu_initial = cu.get_initial_map_ref();
    unit_bimap_t cu_final = cu.get_final_map_ref();

    REQUIRE(cu_initial.left.find(Qubit(0))->second == Qubit("a", 0));
    REQUIRE(cu_initial.left.find(Qubit(1))->second == Qubit("a", 1));
    REQUIRE(cu_final.left.find(Qubit(0))->second == Qubit("a", 0));
    REQUIRE(cu_final.left.find(Qubit(1))->second == Qubit("a", 1));
  }
  GIVEN("Two empty wires, qubits from different registers.") {
    Circuit c(5);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::Z, {4});

    std::map<Qubit, Qubit> rename_map = {
        {Qubit(0), Qubit("a", 1)},
        {Qubit(1), Qubit("c", 0)},
        {Qubit(2), Qubit("e", 1)},
        {Qubit(3), Qubit("a", 5)},
        {Qubit(4), Qubit("s", 7)}};
    c.rename_units(rename_map);

    CompilationUnit cu(c);

    PassPtr pp = gen_flatten_relabel_registers_pass("a");
    REQUIRE(pp->apply(cu));

    unit_bimap_t cu_initial = cu.get_initial_map_ref();
    unit_bimap_t cu_final = cu.get_final_map_ref();

    REQUIRE(cu_initial.left.find(Qubit("a", 1))->second == Qubit("a", 0));
    REQUIRE(cu_initial.left.find(Qubit("c", 0))->second == Qubit("a", 1));
    REQUIRE(cu_initial.left.find(Qubit("e", 1))->second == Qubit("e", 1));
    REQUIRE(cu_initial.left.find(Qubit("a", 5))->second == Qubit("a", 5));
    REQUIRE(cu_initial.left.find(Qubit("s", 7))->second == Qubit("a", 2));

    REQUIRE(cu_final.left.find(Qubit("a", 1))->second == Qubit("a", 0));
    REQUIRE(cu_final.left.find(Qubit("c", 0))->second == Qubit("a", 1));
    REQUIRE(cu_final.left.find(Qubit("e", 1))->second == Qubit("e", 1));
    REQUIRE(cu_final.left.find(Qubit("a", 5))->second == Qubit("a", 5));
    REQUIRE(cu_final.left.find(Qubit("s", 7))->second == Qubit("a", 2));
  }
}

SCENARIO("Custom rebase pass with implicit wire swaps.") {
  OpTypeSet allowed_gates_cx = {OpType::PhasedX, OpType::Rz, OpType::CX};
  PassPtr pp_rebase_cx = gen_rebase_pass_via_tk2(
      allowed_gates_cx, CircPool::TK2_using_CX_and_swap,
      CircPool::tk1_to_PhasedXRz);
  OpTypeSet allowed_gates_zzmax = {OpType::PhasedX, OpType::Rz, OpType::ZZMax};
  PassPtr pp_rebase_zzmax = gen_rebase_pass_via_tk2(
      allowed_gates_zzmax, CircPool::TK2_using_ZZMax_and_swap,
      CircPool::tk1_to_PhasedXRz);
  OpTypeSet allowed_gates_zzphase = {
      OpType::PhasedX, OpType::Rz, OpType::ZZPhase};
  PassPtr pp_rebase_zzphase = gen_rebase_pass_via_tk2(
      allowed_gates_zzphase, CircPool::TK2_using_ZZPhase_and_swap,
      CircPool::tk1_to_PhasedXRz);
  GIVEN("Targeting CX gates, ISWAPMax gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ISWAPMax, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_cx->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::CX) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting CX gates, Sycamore gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::Sycamore, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_cx->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::CX) == 2);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting CX gates, ISWAP gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ISWAP, 0.3, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_cx->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::CX) == 2);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting CX gates, SWAP gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::SWAP, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_cx->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::CX) == 0);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting CX gates, CX gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(c);
    CHECK(!pp_rebase_cx->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::CX) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting CX gates, ZZMAX gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ZZMax, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_cx->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::CX) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting CX gates, ZZPhasegate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ZZPhase, 0.3, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_cx->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::CX) == 2);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZMax gates, ISWAPMax gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ISWAPMax, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzmax->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZMax) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZMax gates, ISWAP gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ISWAP, 0.3, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzmax->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZMax) == 2);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZMax gates, Sycamore gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::Sycamore, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzmax->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZMax) == 2);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZMax gates, SWAP gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::SWAP, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzmax->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZMax) == 0);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZMax gates, CX gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzmax->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZMax) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZMax gates, ZZMAX gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ZZMax, {0, 1});
    CompilationUnit cu(c);
    CHECK(!pp_rebase_zzmax->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZMax) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZMax gates, ZZPhasegate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ZZPhase, 0.3, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzmax->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZMax) == 2);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZPhase gates, ISWAPMax gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ISWAPMax, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzphase->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZPhase) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZPhase gates, ISWAP gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ISWAP, 0.3, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzphase->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZPhase) == 2);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZPhase gates, Sycamore gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::Sycamore, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzphase->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZPhase) == 2);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZPhase gates, SWAP gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::SWAP, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzphase->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZPhase) == 0);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZPhase gates, CX gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzphase->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZPhase) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZPhase gates, ZZMax gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ZZMax, {0, 1});
    CompilationUnit cu(c);
    CHECK(pp_rebase_zzphase->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZPhase) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting ZZPhase gates, ZZPhasegate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ZZPhase, 0.3, {0, 1});
    CompilationUnit cu(c);
    CHECK(!pp_rebase_zzphase->apply(cu));
    REQUIRE(cu.get_circ_ref().count_gates(OpType::ZZPhase) == 1);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting TK2 gates, SWAP gate.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::SWAP, {0, 1});
    CompilationUnit cu(c);
    CHECK(gen_rebase_pass_via_tk2(
              {OpType::PhasedX, OpType::Rz, OpType::TK2},
              CircPool::TK2_using_TK2_or_swap, CircPool::tk1_to_PhasedXRz)
              ->apply(cu));
    REQUIRE(cu.get_circ_ref().n_gates() == 0);
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
  GIVEN("Targeting CZ and PhasedX gates.") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::TK1, {0.1, 0.2, 0.3}, {0});
    c.add_op<unsigned>(OpType::TK1, {0.7, 0.6, 0.8}, {1});
    CompilationUnit cu(c);
    CHECK(gen_rebase_pass_via_tk2(
              {OpType::PhasedX, OpType::CZ}, CircPool::TK2_using_TK2_or_swap,
              CircPool::tk1_to_PhasedX)
              ->apply(cu));
    auto u1 = tket_sim::get_unitary(c);
    auto u2 = tket_sim::get_unitary(cu.get_circ_ref());
    REQUIRE(u1.isApprox(u2));
  }
}

SCENARIO(
    "Test FullPeepholeOptimise for short sequences of YYPhase, XXPhase and "
    "ZZPhase.") {
  GIVEN("YYPhase(0.3)") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::YYPhase, 0.3, {0, 1});
    CompilationUnit cu(c);
    CHECK(SynthesiseTK()->apply(cu));
    REQUIRE(cu.get_circ_ref().n_gates() == 1);
  }
  GIVEN("XXPhase(0.3)") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::XXPhase, 0.3, {0, 1});
    CompilationUnit cu(c);
    CHECK(SynthesiseTK()->apply(cu));
    REQUIRE(cu.get_circ_ref().n_gates() == 1);
  }

  GIVEN("ZZPhase(0.3)") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::ZZPhase, 0.3, {0, 1});
    CompilationUnit cu(c);
    CHECK(SynthesiseTK()->apply(cu));
    REQUIRE(cu.get_circ_ref().n_gates() == 1);
  }
}

SCENARIO("PauliExponentials") {
  GIVEN("A PhasedX gate") {
    // https://github.com/quantinuum/tket/issues/1244
    Circuit c(1);
    c.add_op<unsigned>(OpType::PhasedX, {0.5, 0.6}, {0});
    c.add_op<unsigned>(OpType::PhasedX, {0.6, 0.5}, {0});
    CompilationUnit cu(c);
    CHECK(gen_pauli_exponentials(Transforms::PauliSynthStrat::Individual)
              ->apply(cu));
    REQUIRE(test_unitary_comparison(c, cu.get_circ_ref(), true));
  }
}

SCENARIO("GPI, GPI2 and AAMS operations") {
  GIVEN("A circuit") {
    Circuit c(3);
    c.add_op<unsigned>(OpType::GPI, 0.1, {0});
    c.add_op<unsigned>(OpType::GPI2, 0.2, {1});
    c.add_op<unsigned>(OpType::AAMS, {0.3, 0.4, 0.5}, {0, 1});
    c.add_op<unsigned>(OpType::AAMS, {0.6, 0.7, 0.8}, {1, 2});
    c.add_op<unsigned>(OpType::GPI, 0.1, {1});
    c.add_op<unsigned>(OpType::GPI2, 0.2, {2});
    Circuit c_d = c.dagger();
    Circuit c2 = c;
    c2.append(c_d);
    Eigen::MatrixXcd u = tket_sim::get_unitary(c2);
    REQUIRE(u.isApprox(Eigen::MatrixXcd::Identity(8, 8)));
    Circuit c_t = c.transpose();
    CHECK(c_t.n_gates() == c.n_gates());
    CompilationUnit cu(c);
    CHECK(FullPeepholeOptimise(true, OpType::CX)->apply(cu));
    REQUIRE(test_unitary_comparison(c, cu.get_circ_ref()));
    CompilationUnit cu1(c);
    CHECK(gen_rebase_pass_via_tk2(
              {OpType::PhasedX, OpType::Rz, OpType::CX},
              CircPool::TK2_using_CX_and_swap, CircPool::tk1_to_PhasedXRz)
              ->apply(cu1));
    REQUIRE(test_unitary_comparison(c, cu1.get_circ_ref()));
  }
  GIVEN("Rebasing a symbolic AAAS gate") {
    Circuit circ(2);
    auto a = SymTable::fresh_symbol("a");
    auto b = SymTable::fresh_symbol("b");
    auto c = SymTable::fresh_symbol("c");
    auto ea = Expr(a);
    auto eb = Expr(b);
    auto ec = Expr(c);
    SymEngine::map_basic_basic sub_map{
        std::make_pair(a, Expr(0.1)), std::make_pair(b, Expr(0.2)),
        std::make_pair(c, Expr(0.3))};
    circ.add_op<unsigned>(OpType::AAMS, {Expr(a), Expr(b), Expr(c)}, {0, 1});
    CompilationUnit cu(circ);
    CHECK(gen_rebase_pass_via_tk2(
              {OpType::PhasedX, OpType::Rz, OpType::CX},
              CircPool::TK2_using_CX_and_swap, CircPool::tk1_to_PhasedXRz)
              ->apply(cu));
    Circuit circ1 = cu.get_circ_ref();
    circ1.symbol_substitution(sub_map);
    Circuit circ2(2);
    circ2.add_op<unsigned>(OpType::AAMS, {0.1, 0.2, 0.3}, {0, 1});
    REQUIRE(test_unitary_comparison(circ1, circ2));
  }
  GIVEN("A circuit made of Clifford gates") {
    Circuit c(3, 1);
    c.add_op<unsigned>(OpType::GPI, 0.25, {0});
    c.add_op<unsigned>(OpType::GPI2, 0.5, {1});
    c.add_op<unsigned>(OpType::AAMS, {0, 0.1, 0.2}, {0, 1});
    c.add_op<unsigned>(OpType::AAMS, {1., 0.25, 0.75}, {1, 2});
    c.add_op<unsigned>(OpType::AAMS, {0.5, 0.5, 1.5}, {0, 1});
    c.add_measure(0, 0);
    REQUIRE(CliffordCircuitPredicate().verify(c));
  }
  GIVEN("A circuit with a non-Clifford AAMS gate") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::AAMS, {1., 0, 0.1}, {0, 1});
    REQUIRE_FALSE(CliffordCircuitPredicate().verify(c));
  }
}

SCENARIO("AutoRebase") {
  GIVEN("Test rebase") {
    std::vector<std::pair<std::unordered_set<OpType>, bool>> params = {
        {{OpType::CX, OpType::TK1}, false},
        {{OpType::CX, OpType::U3}, false},
        {{OpType::ZZMax, OpType::X, OpType::SX, OpType::Rz}, false},
        {{OpType::XXPhase, OpType::PhasedX, OpType::Rz}, false},
        {{OpType::ECR, OpType::Rx, OpType::Rz}, false},
        {{OpType::CZ, OpType::Rx, OpType::Ry}, false},
        {{OpType::AAMS, OpType::Rz, OpType::H}, false},
        {{OpType::TK2, OpType::Rz, OpType::SX}, false},
        {{OpType::TK2, OpType::GPI, OpType::GPI2}, true},
        {{OpType::TK2, OpType::CX, OpType::X, OpType::TK1}, true}};
    Circuit tk2_circ(2);
    tk2_circ.add_op<unsigned>(OpType::TK1, {0.1, 0.2, 0.3}, {0});
    tk2_circ.add_op<unsigned>(OpType::TK2, {0.1, 0.2, 0.3}, {0, 1});
    CompilationUnit tk2_cu(tk2_circ);
    Circuit cx_circ(2);
    cx_circ.add_op<unsigned>(OpType::TK1, {0.1, 0.2, 0.3}, {0});
    cx_circ.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cx_cu(cx_circ);
    for (const auto& pair : params) {
      gen_auto_rebase_pass(pair.first, pair.second)
          ->apply(tk2_cu, SafetyMode::Audit);
      gen_auto_rebase_pass(pair.first, pair.second)
          ->apply(cx_cu, SafetyMode::Audit);
      REQUIRE(test_unitary_comparison(tk2_circ, tk2_cu.get_circ_ref()));
      REQUIRE(test_unitary_comparison(cx_circ, cx_cu.get_circ_ref()));
    }
  }
  GIVEN("Test exception") {
    REQUIRE_THROWS_AS(
        gen_auto_rebase_pass({OpType::CX, OpType::Rz}, false), Unsupported);
    REQUIRE_THROWS_AS(
        gen_auto_rebase_pass({OpType::CRz, OpType::TK1}, false), Unsupported);
    REQUIRE_THROWS_AS(gen_auto_rebase_pass({}, false), Unsupported);
  }
  GIVEN("Rebasing to ISWAPMax") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    Circuit circ1 = circ;
    PassPtr p_noswaps =
        gen_auto_rebase_pass({OpType::ISWAPMax, OpType::TK1}, false);
    PassPtr p_swapsok =
        gen_auto_rebase_pass({OpType::ISWAPMax, OpType::TK1}, true);
    CompilationUnit cu(circ);
    CompilationUnit cu1(circ1);
    CHECK(p_noswaps->apply(cu));
    CHECK(p_swapsok->apply(cu1));
    REQUIRE(test_unitary_comparison(circ, cu.get_circ_ref()));
    REQUIRE(test_unitary_comparison(circ1, cu1.get_circ_ref()));
  }
}

SCENARIO("AutoSquash") {
  GIVEN("Test squash") {
    Circuit c1(1);
    c1.add_op<unsigned>(OpType::Rx, 0.2, {0});
    c1.add_op<unsigned>(OpType::Rz, 0.4, {0});
    CompilationUnit cu1(c1);
    REQUIRE(gen_auto_squash_pass({OpType::TK1, OpType::Rz, OpType::Rx})
                ->apply(cu1));
    REQUIRE(cu1.get_circ_ref().n_gates() == 1);
    REQUIRE(cu1.get_circ_ref().count_gates(OpType::TK1) == 1);
    REQUIRE(test_unitary_comparison(c1, cu1.get_circ_ref()));
    // RzPhasedX squash
    Circuit c2(1);
    c2.add_op<unsigned>(OpType::TK1, {0.1, 0.2, 0.3}, {0});
    CompilationUnit cu2(c2);
    REQUIRE(gen_auto_squash_pass({OpType::PhasedX, OpType::Rz})->apply(cu2));
    REQUIRE(cu2.get_circ_ref().n_gates() == 2);
    REQUIRE(cu2.get_circ_ref().count_gates(OpType::PhasedX) == 1);
    REQUIRE(cu2.get_circ_ref().count_gates(OpType::Rz) == 1);
    REQUIRE(test_unitary_comparison(c2, cu2.get_circ_ref()));
  }
  GIVEN("Test exception") {
    REQUIRE_THROWS_AS(gen_auto_squash_pass({OpType::Rx}), Unsupported);
    REQUIRE_THROWS_AS(gen_auto_squash_pass({}), Unsupported);
  }
}

SCENARIO("Efficient TK2 synthesis") {
  // https://github.com/quantinuum/tket/issues/1738
  GIVEN("A single YYPhase") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::YYPhase, 0.3, {0, 1});

    Circuit c0(2);
    c0.add_op<unsigned>(OpType::TK2, {0.0, 0.3, 0.0}, {0, 1});

    CompilationUnit cu(c);
    CHECK(FullPeepholeOptimise(true, OpType::TK2)->apply(cu));
    Circuit c1 = cu.get_circ_ref();
    REQUIRE(c1 == c0);
  }
  GIVEN("An unnormalized TK2") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::TK2, {0.1, 0.2, 0.3}, {0, 1});

    CompilationUnit cu(c);
    FullPeepholeOptimise(true, OpType::TK2)->apply(cu);
    Circuit c1 = cu.get_circ_ref();
    REQUIRE(c1 == c);
  }
}

SCENARIO("RxFromSX") {
  GIVEN("A circuit containing SX and SXdg gates") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::SX, {0});
    c.add_op<unsigned>(OpType::SX, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::SXdg, {1});
    CompilationUnit cu(c);
    RxFromSX()->apply(cu);
    Circuit c1 = cu.get_circ_ref();
    Circuit c0(2);
    c0.add_op<unsigned>(OpType::Rx, 0.5, {0});
    c0.add_op<unsigned>(OpType::Rx, 0.5, {0});
    c0.add_op<unsigned>(OpType::CX, {0, 1});
    c0.add_op<unsigned>(OpType::Rx, -0.5, {1});
    c0.add_phase(0.25);
    REQUIRE(c1 == c0);
  }
}

}  // namespace test_CompilerPass
}  // namespace tket
