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
#include <numeric>
#include <optional>
#include <stdexcept>
#include <tket/Circuit/Circuit.hpp>

#include "CircuitsForTesting.hpp"
#include "Simulation/ComparisonFunctions.hpp"
#include "testutil.hpp"
#include "tket/Circuit/CircPool.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Gate/Rotation.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/OpType/OpTypeFunctions.hpp"
#include "tket/Ops/BarrierOp.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Predicates/CompilerPass.hpp"
#include "tket/Predicates/PassLibrary.hpp"
#include "tket/Predicates/Predicates.hpp"
#include "tket/Transformations/BasicOptimisation.hpp"
#include "tket/Transformations/CliffordOptimisation.hpp"
#include "tket/Transformations/Combinator.hpp"
#include "tket/Transformations/Decomposition.hpp"
#include "tket/Transformations/OptimisationPass.hpp"
#include "tket/Transformations/PauliOptimisation.hpp"
#include "tket/Transformations/Rebase.hpp"
#include "tket/Transformations/Replacement.hpp"
#include "tket/Transformations/RzPhasedXSquash.hpp"
#include "tket/Transformations/Transform.hpp"
#include "tket/Utils/Expression.hpp"
#include "tket/Utils/UnitID.hpp"

/* This test file covers decomposition, basic optimisation and synthesis passes.
It does not cover Rebasing, Clifford optimisation, Phase Gadgets,
Multi-controlled Decomp, CZ Optimisation, PauliString optimisation, getting
matrices from Circuits etc*/

// TODO: Split this up more

namespace tket {
namespace test_Synthesis {

SCENARIO("Check commutation through multiqubit ops") {
  GIVEN("An empty circuit") {
    Circuit circ(1);
    REQUIRE(!Transforms::commute_through_multis().apply(circ));
    WHEN("A single qubit gate is added") {
      circ.add_op<unsigned>(OpType::Z, {0});
      // circ.add_op<unsigned>(OpType::Y, {1});
      // circ.add_op<unsigned>(OpType::Z, {2});
      Circuit single = circ;
      THEN("No commutation is performed") {
        REQUIRE(!Transforms::commute_through_multis().apply(circ));
        REQUIRE(circ == single);
      }
      AND_WHEN("A two qubit gate is added at the end") {
        circ.add_blank_wires(1);
        circ.add_op<unsigned>(OpType::CZ, {0, 1});
        Circuit two_none = circ;
        THEN("No commutation is performed") {
          REQUIRE(!Transforms::commute_through_multis().apply(circ));
          REQUIRE(circ == two_none);
        }
        AND_WHEN("Single qubit gate is added to the end") {
          std::vector<Expr> vec{0.5};
          const Op_ptr op_z = get_op_ptr(OpType::Rz, vec);
          const Op_ptr op_y = get_op_ptr(OpType::Ry, vec);

          circ.add_op<unsigned>(op_z, {0});
          circ.add_op<unsigned>(OpType::Z, {0});

          circ.add_op<unsigned>(op_y, {1});

          Circuit correct(2);
          correct.add_op<unsigned>(OpType::Z, {0});
          correct.add_op<unsigned>(op_z, {0});
          correct.add_op<unsigned>(OpType::Z, {0});
          correct.add_op<unsigned>(OpType::CZ, {0, 1});
          correct.add_op<unsigned>(op_y, {1});
          THEN("Only the correct single qubit gate is commuted") {
            REQUIRE(Transforms::commute_through_multis().apply(circ));
            REQUIRE(correct == circ);
          }
        }
      }
    }
  }

  GIVEN("A complicated multi qubit circuit") {
    const Op_ptr op_z = get_op_ptr(OpType::Rz, 0.2);
    const Op_ptr op_xxphase = get_op_ptr(OpType::XXPhase, 0.2);
    const Op_ptr op_xxphase3 = get_op_ptr(OpType::XXPhase3, 0.3);
    Circuit circ(4, 1);
    circ.add_op<unsigned>(OpType::Z, {0});
    circ.add_conditional_gate<unsigned>(OpType::BRIDGE, {}, {1, 2, 3}, {0}, 1);
    circ.add_op<unsigned>(OpType::CCX, {1, 2, 3});

    circ.add_op<unsigned>(OpType::noop, {2});
    circ.add_op<unsigned>(op_z, {2});
    circ.add_op<unsigned>(OpType::X, {3});

    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::ZZMax, {1, 3});

    add_1qb_gates(circ, OpType::Z, {1, 3, 1});
    circ.add_op<unsigned>(op_xxphase, {0, 2});

    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::Y, {2});

    circ.add_op<unsigned>(op_xxphase3, {0, 2, 3});

    circ.add_op<unsigned>(OpType::X, {2});
    circ.add_op<unsigned>(OpType::Z, {3});

    WHEN("Commutation is performed") {
      REQUIRE(Transforms::commute_through_multis().apply(circ));

      THEN("The correct final circuit is produced") {
        Circuit correct(4, 1);
        correct.add_op<unsigned>(OpType::Z, {0});
        correct.add_op<unsigned>(OpType::Z, {1});

        correct.add_op<unsigned>(OpType::X, {0});
        correct.add_op<unsigned>(OpType::Z, {1});
        correct.add_op<unsigned>(OpType::noop, {2});

        correct.add_op<unsigned>(op_z, {2});
        correct.add_op<unsigned>(OpType::X, {3});

        correct.add_conditional_gate<unsigned>(
            OpType::BRIDGE, {}, {1, 2, 3}, {0}, 1);
        correct.add_op<unsigned>(OpType::CCX, {1, 2, 3});

        correct.add_op<unsigned>(OpType::H, {2});
        correct.add_op<unsigned>(OpType::Z, {3});

        correct.add_op<unsigned>(OpType::ZZMax, {1, 3});

        correct.add_op<unsigned>(op_xxphase, {0, 2});

        correct.add_op<unsigned>(OpType::Y, {2});
        correct.add_op<unsigned>(OpType::X, {2});

        correct.add_op<unsigned>(op_xxphase3, {0, 2, 3});

        correct.add_op<unsigned>(OpType::Z, {3});

        REQUIRE(circ == correct);
      }
    }
  }
  GIVEN("A circuit with classical control") {
    Circuit circ(2, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 1);

    circ.add_barrier({0, 1});

    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 1}, {0}, 1);
    circ.add_op<unsigned>(OpType::Rz, 0.142, {0});

    circ.add_barrier({0, 1});

    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {1, 0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {1, 0}, {0}, 1);
    circ.add_op<unsigned>(OpType::X, {0});

    REQUIRE(Transforms::commute_through_multis().apply(circ));

    Circuit solution(2, 1);
    solution.add_op<unsigned>(OpType::CX, {0, 1});
    solution.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 1);

    solution.add_barrier({0, 1});

    solution.add_op<unsigned>(OpType::Rz, 0.142, {0});
    solution.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 1}, {0}, 1);

    solution.add_barrier({0, 1});

    solution.add_op<unsigned>(OpType::X, {0});
    solution.add_conditional_gate<unsigned>(OpType::CX, {}, {1, 0}, {0}, 0);
    solution.add_conditional_gate<unsigned>(OpType::CX, {}, {1, 0}, {0}, 1);

    REQUIRE(circ == solution);
  }
  GIVEN("A circuit with classical control (2)") {
    Circuit circ(3, 3);
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_measure(0, 0);
    circ.add_measure(1, 1);
    circ.add_conditional_gate<unsigned>(OpType::X, {}, {2}, {0, 1}, 1);

    Circuit old_circ = circ;
    REQUIRE(!Transforms::commute_through_multis().apply(circ));
    REQUIRE(old_circ == circ);
  }
  GIVEN("A circuit with classical control (3)") {
    Circuit circ(3, 3);
    circ.add_measure(0, 0);
    circ.add_measure(1, 1);
    circ.add_conditional_gate<unsigned>(OpType::ZZMax, {}, {0, 2}, {0, 1}, 1);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Z, {2});

    Circuit solution(3, 3);
    solution.add_measure(0, 0);
    solution.add_measure(1, 1);
    solution.add_op<unsigned>(OpType::Rz, 0.3, {0});
    solution.add_op<unsigned>(OpType::Z, {2});
    solution.add_conditional_gate<unsigned>(
        OpType::ZZMax, {}, {0, 2}, {0, 1}, 1);

    REQUIRE(Transforms::commute_through_multis().apply(circ));
    REQUIRE(solution == circ);
  }
  GIVEN("A bridge") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::BRIDGE, {1, 2, 0});
    REQUIRE_FALSE(Transforms::commute_through_multis().apply(circ));
  }
  GIVEN("A circuit with a conditional measure") {
    Circuit circ(2, 3);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_conditional_gate<unsigned>(OpType::Measure, {}, {0, 0}, {1}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Measure, {}, {1, 0}, {2}, 1);
    Circuit orig = circ;

    REQUIRE(!Transforms::commute_through_multis().apply(circ));
    REQUIRE(orig == circ);
  }
}

SCENARIO(
    "Generating Circuits and performing decomposition, basic optimisation "
    "and synthesis") {
  GIVEN("A circuit made of non-IBM ops") {
    Circuit circ(3);
    Vertex v1 = circ.add_op<unsigned>(OpType::CCX, {0, 1, 2});
    unsigned N =
        CX_circ_from_multiq(circ.get_Op_ptr_from_Vertex(v1)).n_vertices();
    Transforms::decompose_multi_qubits_CX().apply(circ);
    REQUIRE(circ.n_vertices() == N);
  }
  GIVEN("Circuits of phase gadgets") {
    Circuit circ(8);
    circ.add_op<unsigned>(OpType::PhaseGadget, 0.3, {0, 1, 2, 3, 4, 5, 6, 7});
    circ.add_op<unsigned>(OpType::PhaseGadget, 0.5, {0});
    circ.add_op<unsigned>(OpType::PhaseGadget, 1., {1, 2, 3, 4, 5});
    Transforms::decompose_multi_qubits_CX().apply(circ);
    Transforms::decompose_single_qubits_TK1().apply(circ);
    REQUIRE(circ.get_slices().size() == 23);
  }
  GIVEN("Circuits of symbolic phase gadgets") {
    Circuit circ(8);
    Sym a = SymEngine::symbol("alpha");
    Expr alpha(a);
    Sym b = SymEngine::symbol("beta");
    Expr beta(b);
    Sym c = SymEngine::symbol("gamma");
    Expr gamma(c);
    circ.add_op<unsigned>(OpType::PhaseGadget, alpha, {0, 1, 2, 3, 4, 5, 6, 7});
    circ.add_op<unsigned>(OpType::PhaseGadget, beta, {0});
    circ.add_op<unsigned>(OpType::PhaseGadget, gamma, {1, 2, 3, 4, 5});
    Transforms::decompose_multi_qubits_CX().apply(circ);
    Transforms::decompose_single_qubits_TK1().apply(circ);
    symbol_map_t symbol_map;
    symbol_map[a] = Expr(0.3);
    symbol_map[b] = Expr(0.5);
    symbol_map[c] = Expr(1.);
    circ.symbol_substitution(symbol_map);
    REQUIRE(circ.get_slices().size() == 23);
    REQUIRE(circ.count_gates(OpType::TK1) == 3);
  }
  GIVEN("Commute Rz through CX") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.333, {0});
    Transforms::commute_through_multis().apply(circ);
    SliceVec slices = circ.get_slices();
    REQUIRE(circ.get_OpType_from_Vertex(*slices[0].begin()) == OpType::Rz);
    REQUIRE(circ.get_OpType_from_Vertex(*slices[1].begin()) == OpType::CX);
  }

  GIVEN("A series of one-qubit gates and CZs") {
    Circuit test1(4);
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::CZ, {0, 1});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::CZ, {0, 1});
    test1.add_op<unsigned>(OpType::Z, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::Z, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    add_2qb_gates(test1, OpType::CZ, {{1, 2}, {1, 2}, {1, 2}, {1, 2}});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::Y, {3});

    WHEN("Synthesis is performed") {
      Transforms::synthesise_tket().apply(test1);
      BGL_FORALL_VERTICES(v, test1.dag, DAG) {
        OpType optype = test1.get_OpType_from_Vertex(v);
        bool finished_synth =
            ((test1.detect_boundary_Op(v)) || (optype == OpType::TK1) ||
             (optype == OpType::CX));
        REQUIRE(finished_synth);
      }
      REQUIRE_NOTHROW(test1.get_slices());
      test1.assert_valid();
    }
  }
  GIVEN("Circuit containing only two CXs in a row with matching edge ports") {
    Circuit circ;
    circ.add_blank_wires(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    WHEN("Circuit is synthesised to TK1") {
      Transforms::synthesise_tket().apply(circ);
      THEN(
          "Resulting circuit is empty (apart from input/output "
          "vertices") {
        REQUIRE(circ.n_vertices() == 4);
        BGL_FORALL_VERTICES(v, circ.dag, DAG) {
          REQUIRE(circ.detect_boundary_Op(v));
        }
        circ.get_slices();
        circ.assert_valid();
      }
    }
  }
  GIVEN(
      "Circuit containing only two CXs in a row but with non-matching edge "
      "ports") {
    Circuit circ;
    circ.add_blank_wires(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 0});
    WHEN("Circuit is synthesised") {
      Transforms::synthesise_tket().apply(circ);
      THEN("Resulting circuit still contains the CXs") {
        REQUIRE(circ.n_vertices() == 6);
        circ.get_slices();
        circ.assert_valid();
      }
    }
  }
  GIVEN("Circuit with only blank wires") {
    Circuit circ;
    int width = 6;
    circ.add_blank_wires(width);
    Transforms::synthesise_tket().apply(circ);
    circ.assert_valid();
    SliceVec slices = circ.get_slices();
    REQUIRE(slices.size() == 0);
  }
  GIVEN("A UCCSD example") {
    auto circ = CircuitsForTesting::get().uccsd;
    const StateVector s0 = tket_sim::get_statevector(circ);
    REQUIRE(circ.count_gates(OpType::TK1) == 0);
    REQUIRE(circ.count_gates(OpType::CX) == 12);
    Transforms::squash_1qb_to_tk1().apply(circ);
    REQUIRE(circ.count_gates(OpType::TK1) == 12);
    REQUIRE(circ.count_gates(OpType::CX) == 12);
    const StateVector s1 = tket_sim::get_statevector(circ);
    REQUIRE(tket_sim::compare_statevectors_or_unitaries(s0, s1));
  }
  GIVEN("A controlled phase") {
    // https://github.com/quantinuum/tket/issues/576
    Circuit circ(1, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {2.}, {0}, {0}, 1);
    Transforms::squash_1qb_to_pqp(OpType::Rz, OpType::Ry).apply(circ);
    Circuit circ1(1, 1);
    circ1.add_conditional_gate<unsigned>(OpType::Phase, {1.}, {}, {0}, 1);
    REQUIRE(circ == circ1);
  }
}

SCENARIO(
    "Check that annihilation function works on a basic circuit",
    "[transform][annihilation][optimise]") {
  GIVEN("A contrived circuit that should annihilate entirely") {
    Circuit test(2);
    add_1qb_gates(test, OpType::H, {0, 1});
    add_2qb_gates(test, OpType::CZ, {{0, 1}, {0, 1}});
    add_1qb_gates(test, OpType::H, {0, 1});
    WHEN("Redundancy removal is performed") {
      Transforms::remove_redundancies().apply(test);
      REQUIRE(test.n_vertices() == 4);
      BGL_FORALL_VERTICES(v, test.dag, DAG) {
        bool no_ops = test.detect_boundary_Op(v);
        REQUIRE(no_ops);
      }
      test.assert_valid();
    }
  }
  GIVEN("A circuit with noop gates") {
    Circuit test(2);
    test.add_op<unsigned>(OpType::noop, {0});
    test.add_op<unsigned>(OpType::CZ, {0, 1});
    test.add_op<unsigned>(OpType::noop, {1});
    test.add_op<unsigned>(OpType::noop, {1});
    REQUIRE(Transforms::remove_redundancies().apply(test));
    REQUIRE(test.n_gates() == 1);
    REQUIRE(
        test.get_OpType_from_Vertex(*test.get_slices()[0].begin()) ==
        OpType::CZ);
  }
  GIVEN("A 4-qubit circuit with some annihilation") {
    Circuit test1(4);
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::CZ, {0, 1});
    test1.add_op<unsigned>(OpType::noop, {0});
    test1.add_op<unsigned>(OpType::CZ, {0, 1});
    test1.add_op<unsigned>(OpType::Z, {0});
    test1.add_op<unsigned>(OpType::noop, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::Z, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::Rz, 2. / 3., {0});
    test1.add_op<unsigned>(OpType::Rz, 2. / 3., {0});
    test1.add_op<unsigned>(OpType::Rz, 2. / 3., {0});
    test1.add_op<unsigned>(OpType::noop, {0});
    test1.add_op<unsigned>(OpType::CX, {0, 1});
    test1.add_op<unsigned>(OpType::CX, {0, 1});
    test1.add_op<unsigned>(OpType::Y, {0});

    WHEN("Annihilation is performed") {
      REQUIRE(Transforms::remove_redundancies().apply(test1));
      REQUIRE(test1.n_vertices() == 9);
      test1.assert_valid();
    }
  }
  GIVEN("A similar 4-qubit circuit but with some port swapping") {
    Circuit test1(4);
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::CZ, {0, 1});
    test1.add_op<unsigned>(OpType::CZ, {0, 1});
    test1.add_op<unsigned>(OpType::Z, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::Z, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::CX, {1, 2});
    test1.add_op<unsigned>(OpType::CX, {2, 1});
    test1.add_op<unsigned>(OpType::Y, {3});
    WHEN("Annihilation Transformation is performed") {
      REQUIRE(Transforms::remove_redundancies().apply(test1));
      REQUIRE(test1.n_vertices() == 11);
      test1.assert_valid();
    }
  }
  GIVEN("A circuit which could be merged or have identity removed") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 0.4, {0});
    circ.add_op<unsigned>(OpType::Rz, 0., {0});
    REQUIRE_NOTHROW(Transforms::remove_redundancies().apply(circ));
  }

  GIVEN("A circuit with Z basis operations at the end") {
    Circuit test1(4, 4);
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::X, {1});
    test1.add_op<unsigned>(OpType::Y, {2});
    test1.add_op<unsigned>(OpType::Z, {3});
    // test1.add_op<unsigned>(OpType::CZ, {0, 1});
    test1.add_op<unsigned>(OpType::CX, {0, 1});
    test1.add_op<unsigned>(OpType::CZ, {2, 3});
    test1.add_op<unsigned>(OpType::X, {2});
    test1.add_op<unsigned>(OpType::CZ, {0, 1});
    test1.add_op<unsigned>(OpType::Z, {0});

    CHECK_FALSE(Transforms::remove_redundancies().apply(test1));
    WHEN("Measurements are added") {
      test1.add_measure(0, 0);
      test1.add_measure(1, 1);
      test1.add_measure(2, 2);
      THEN("Redundant gates before a measurement are removed.") {
        REQUIRE(Transforms::remove_redundancies().apply(test1));
        CHECK(test1.n_gates() == 10);
      }
    }
    WHEN("The measure is classically controlled instead") {
      test1.add_conditional_gate<unsigned>(OpType::Measure, {}, {0, 0}, {1}, 1);
      test1.add_measure(1, 1);
      test1.add_measure(2, 2);
      THEN("Redundancies are not removed at that measure") {
        REQUIRE_FALSE(Transforms::remove_redundancies().apply(test1));
        CHECK(test1.n_gates() == 12);
      }
    }
  }
}

SCENARIO("Testing general 1qb squash") {
  GIVEN("A series of 0 param gates") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rx, 0., {0});
    circ.add_op<unsigned>(OpType::Ry, 0., {0});
    circ.add_op<unsigned>(OpType::Rx, 0., {0});
    REQUIRE(Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Ry).apply(circ));
    REQUIRE(circ.n_vertices() == 2);
  }
  GIVEN("Repetitions of a single gate to merge") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rx, 0.5, {0});
    circ.add_op<unsigned>(OpType::Rx, 1.2, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.2, {0});
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);
    REQUIRE(circ.n_vertices() == 3);
  }

  GIVEN("A single QPQ triple to convert to PQP") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rx, 0.5, {0});
    circ.add_op<unsigned>(OpType::Rz, 1.2, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.2, {0});
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);
    REQUIRE(circ.count_gates(OpType::Rz) == 2);
    REQUIRE(circ.count_gates(OpType::Rx) == 1);
  }

  GIVEN("A circuit that reduces to an identity") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 1.2, {0});
    circ.add_op<unsigned>(OpType::Rx, 1.0, {0});
    circ.add_op<unsigned>(OpType::Rz, 1.2, {0});
    circ.add_op<unsigned>(OpType::Rx, 1.0, {0});
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);
    REQUIRE(circ.n_vertices() == 2);
  }

  GIVEN("Many long merges") {
    Circuit circ(1);
    for (int i = 0; i < 100; i++) {
      circ.add_op<unsigned>(OpType::Rz, 0.035, {0});
    }
    for (int i = 0; i < 100; i++) {
      circ.add_op<unsigned>(OpType::Rx, 0.012, {0});
    }
    for (int i = 0; i < 100; i++) {
      circ.add_op<unsigned>(OpType::Rz, 0.004, {0});
    }
    for (int i = 0; i < 100; i++) {
      circ.add_op<unsigned>(OpType::Rx, 0.026, {0});
    }
    for (int i = 0; i < 100; i++) {
      circ.add_op<unsigned>(OpType::Rz, 0.017, {0});
    }
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);
    REQUIRE(circ.count_gates(OpType::Rz) == 2);
    REQUIRE(circ.count_gates(OpType::Rx) == 1);
  }

  GIVEN("Multiple regions to apply") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rx, 0.5, {0});
    circ.add_op<unsigned>(OpType::Rz, 1.2, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.2, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rx, 0.5, {0});
    circ.add_op<unsigned>(OpType::Rz, 1.2, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.2, {0});
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);
    REQUIRE(circ.count_gates(OpType::Rz) == 3);
    REQUIRE(circ.count_gates(OpType::Rx) == 2);
  }

  GIVEN("Circuit already in desired form") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Ry, 0.5, {0});
    circ.add_op<unsigned>(OpType::Rx, 1.2, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.2, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rx, 0.5, {1});
    circ.add_op<unsigned>(OpType::Ry, 1.2, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rx, 0.2, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.2, {1});
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Ry, true).apply(circ);
    REQUIRE(!success);
    REQUIRE(circ.depth() == 8);
  }

  GIVEN("Circuit has few rotations, but still not optimal") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, 1., {0});
    circ.add_op<unsigned>(OpType::Ry, .5, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {0});
    circ.add_op<unsigned>(OpType::Ry, 1., {0});
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rz, OpType::Ry).apply(circ);
    REQUIRE(success);
  }
  GIVEN("Circuit has few rotations and is optimal") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Ry, 3.5, {0});
    circ.add_op<unsigned>(OpType::Rx, 1, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Ry, 1., {0});
    circ.add_op<unsigned>(OpType::Rx, 1., {0});
    auto u0 = tket_sim::get_unitary(circ);
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Ry).apply(circ);
    auto u1 = tket_sim::get_unitary(circ);
    REQUIRE(u0.isApprox(u1));
    REQUIRE(!success);
  }

  GIVEN("Circuit with first angle pi") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 1., {0});
    circ.add_op<unsigned>(OpType::Rx, 0.528, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.694, {0});
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);
    REQUIRE(circ.n_vertices() == 4);
    VertexVec vertices = circ.vertices_in_order();
    Op_ptr op1 = circ.get_Op_ptr_from_Vertex(vertices[1]);
    Op_ptr op2 = circ.get_Op_ptr_from_Vertex(vertices[2]);
    REQUIRE(op1->get_type() == OpType::Rx);
    REQUIRE(test_equiv_val(op1->get_params()[0], -0.528));
    REQUIRE(op2->get_type() == OpType::Rz);
    REQUIRE(test_equiv_val(op2->get_params()[0], 1.694));
  }

  GIVEN("Circuit with second angle pi") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 0.142, {0});
    circ.add_op<unsigned>(OpType::Rx, 1., {0});
    circ.add_op<unsigned>(OpType::Rz, 0.694, {0});
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);
    REQUIRE(circ.n_vertices() == 4);
    VertexVec vertices = circ.vertices_in_order();
    Op_ptr op1 = circ.get_Op_ptr_from_Vertex(vertices[1]);
    Op_ptr op2 = circ.get_Op_ptr_from_Vertex(vertices[2]);
    REQUIRE(op1->get_type() == OpType::Rz);
    REQUIRE(test_equiv_val(op1->get_params()[0], 0.142 - 0.694));
    REQUIRE(op2->get_type() == OpType::Rx);
    REQUIRE(test_equiv_val(op2->get_params()[0], 1.));
  }

  GIVEN("Circuit with third angle pi") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 0.142, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.528, {0});
    circ.add_op<unsigned>(OpType::Rz, 1., {0});
    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);
    REQUIRE(circ.n_vertices() == 4);
    VertexVec vertices = circ.vertices_in_order();
    Op_ptr op1 = circ.get_Op_ptr_from_Vertex(vertices[1]);
    Op_ptr op2 = circ.get_Op_ptr_from_Vertex(vertices[2]);
    REQUIRE(op1->get_type() == OpType::Rz);
    REQUIRE(test_equiv_val(op1->get_params()[0], 1.142));
    REQUIRE(op2->get_type() == OpType::Rx);
    REQUIRE(test_equiv_val(op2->get_params()[0], -0.528));
  }

  GIVEN("commuting non-compatible conditionals") {
    Circuit circ(2, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.143}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rx, {0.528}, {1}, {0}, 0);

    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);

    REQUIRE(circ.n_gates() == 4);
    std::vector<OpType> expected_optypes{
        OpType::Conditional,  // qubit 0 before CX
        OpType::Conditional,  // qubit 1 before CX
        OpType::CX, OpType::Conditional};
    check_command_types(circ, expected_optypes);

    auto cmds = circ.get_commands();
    expected_optypes = {OpType::Rz, OpType::Rx, OpType::CX, OpType::Rz};
    std::vector<std::vector<Expr>> exp_params{{0.142}, {0.528}, {}, {0.143}};
    for (unsigned i = 0; i < cmds.size(); ++i) {
      Op_ptr op = cmds[i].get_op_ptr();
      if (op->get_type() == OpType::Conditional) {
        op = static_cast<const Conditional &>(*op).get_op();
      }
      REQUIRE(op->get_type() == expected_optypes[i]);
      REQUIRE(op->get_params() == exp_params[i]);
    }

    // as a bonus: check that you can commute another Rz gate through
    success = Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);
    success = Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(!success);
  }

  GIVEN("squashing non-compatible conditionals") {
    Circuit circ(1, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rx, {0.143}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rx, {0.143}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 1);

    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rx, {0.143}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rx, {0.143}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 0);

    Circuit circ_no_cond(1, 1);
    circ_no_cond.add_op<unsigned>(OpType::Rz, 0.142, {0});
    circ_no_cond.add_op<unsigned>(OpType::Rx, 0.143, {0});
    circ_no_cond.add_op<unsigned>(OpType::Rz, 0.142, {0});
    circ_no_cond.add_op<unsigned>(OpType::Rx, 0.143, {0});
    circ_no_cond.add_op<unsigned>(OpType::Rz, 0.142, {0});

    bool success =
        Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ);
    REQUIRE(success);

    Transforms::squash_1qb_to_pqp(OpType::Rx, OpType::Rz).apply(circ_no_cond);

    REQUIRE(circ.n_gates() == 6);
    REQUIRE(circ_no_cond.n_gates() == 3);

    auto cmds = circ.get_commands();
    auto cmds_no_cond = circ_no_cond.get_commands();
    for (unsigned i = 0; i < 3; ++i) {
      const Conditional &cond1 =
          static_cast<const Conditional &>(*cmds[i].get_op_ptr());
      Op_ptr op = cond1.get_op();
      REQUIRE(cond1.get_value() == 1);
      REQUIRE(*op == *cmds_no_cond[i].get_op_ptr());
      const Conditional &cond2 =
          static_cast<const Conditional &>(*cmds[i + 3].get_op_ptr());
      op = cond2.get_op();
      REQUIRE(cond2.get_value() == 0);
      REQUIRE(*op == *cmds_no_cond[i].get_op_ptr());
    }
  }

  GIVEN("Squashing in a choice of gate set") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 0.142, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.528, {0});
    circ.add_op<unsigned>(OpType::Rz, 1., {0});
    circ.add_op<unsigned>(OpType::Rx, 0.482, {0});
    Circuit copy = circ;
    auto xzx = [](const Expr &a, const Expr &b, const Expr &c) {
      Rotation r(OpType::Rz, c);
      r.apply(Rotation(OpType::Rx, b));
      r.apply(Rotation(OpType::Rz, a));
      auto [a1, b1, c1] = r.to_pqp(OpType::Rx, OpType::Rz);
      Circuit ci(1);
      ci.add_op<unsigned>(OpType::Rx, a1, {0});
      ci.add_op<unsigned>(OpType::Rz, b1, {0});
      ci.add_op<unsigned>(OpType::Rx, c1, {0});
      return ci;
    };
    OpTypeSet singleqs = {OpType::Rz, OpType::Rx};
    bool success = Transforms::squash_factory(singleqs, xzx).apply(circ);
    REQUIRE(success);
    check_command_types(circ, {OpType::Rx, OpType::Rz, OpType::Rx});
    REQUIRE(test_unitary_comparison(circ, copy));
    success = Transforms::squash_factory(singleqs, xzx).apply(circ);
    REQUIRE_FALSE(success);
  }
  GIVEN("Squashing with PhasedX") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 0.142, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.528, {0});
    circ.add_op<unsigned>(OpType::Rz, 1., {0});
    Circuit copy = circ;
    OpTypeSet singleqs = {OpType::Rz, OpType::PhasedX};
    bool success =
        Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
            .apply(circ);
    REQUIRE_FALSE(success);
    singleqs.insert(OpType::Rx);
    success = Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
                  .apply(circ);
    REQUIRE(success);
    check_command_types(circ, {OpType::Rz, OpType::PhasedX});
    REQUIRE(test_unitary_comparison(circ, copy));
    success = Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
                  .apply(circ);
    REQUIRE_FALSE(success);
  }
  GIVEN("Squashing 2xPhasedX") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::PhasedX, {0.5, 0.5}, {0});
    circ.add_op<unsigned>(OpType::PhasedX, {0.5, 0.5}, {0});
    Circuit copy = circ;
    OpTypeSet singleqs = {OpType::Rz, OpType::PhasedX};
    bool success =
        Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
            .apply(circ);
    REQUIRE(success);
    check_command_types(circ, {OpType::PhasedX});
    REQUIRE(test_unitary_comparison(circ, copy));
    success = Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
                  .apply(circ);
    REQUIRE_FALSE(success);
  }
  GIVEN("Squashing 2xPhasedX that make a Rz") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::PhasedX, {0.5, 0.5}, {0});
    circ.add_op<unsigned>(OpType::PhasedX, {1.5, 0.5}, {0});
    circ.add_op<unsigned>(OpType::Rz, 1.2, {0});
    Circuit copy = circ;
    OpTypeSet singleqs = {OpType::Rz, OpType::PhasedX};
    bool success =
        Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
            .apply(circ);
    REQUIRE(success);
    check_command_types(circ, {OpType::Rz});
    REQUIRE(test_unitary_comparison(circ, copy));
    success = Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
                  .apply(circ);
    REQUIRE_FALSE(success);
  }
  GIVEN("Squashing alongside rebasing") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, 0.43, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Circuit copy = circ;
    bool success = Transforms::rebase_factory(
                       {OpType::ZZMax, OpType::PhasedX, OpType::Rz},
                       CircPool::CX_using_ZZMax(), CircPool::tk1_to_PhasedXRz)
                       .apply(circ);
    REQUIRE(success);
    OpTypeSet singleqs = {OpType::Rz, OpType::PhasedX};
    success = Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
                  .apply(circ);
    REQUIRE(success);
    success = Transforms::remove_redundancies().apply(circ);
    REQUIRE(success);
    check_command_types(
        circ, {OpType::Rz, OpType::PhasedX, OpType::ZZMax, OpType::Rz,
               OpType::PhasedX});
    success = Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
                  .apply(circ);
    REQUIRE_FALSE(success);
  }
  GIVEN("Squashing conditionals with PhasedX") {
    Circuit inner(1, 1);
    inner.add_conditional_gate<unsigned>(OpType::Rz, {0.849}, {0}, {0}, 0);
    inner.add_conditional_gate<unsigned>(OpType::Rx, {0.234}, {0}, {0}, 0);
    inner.add_conditional_gate<unsigned>(OpType::Rz, {0.014}, {0}, {0}, 0);
    inner.add_conditional_gate<unsigned>(OpType::Rx, {0.547}, {0}, {0}, 0);
    inner.add_conditional_gate<unsigned>(OpType::Rz, {0.832}, {0}, {0}, 0);
    Op_ptr cbox_op = std::make_shared<CircBox>(inner);
    Op_ptr cond_cbox_op = std::make_shared<Conditional>(cbox_op, 1, 1);
    Circuit circ(1, 2);
    circ.add_op<unsigned>(OpType::Rz, 0.142, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.528, {0});
    circ.add_op<unsigned>(OpType::Rz, 1., {0});
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rx, {0.528}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {1.}, {0}, {0}, 0);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rx, {0.528}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {1.}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {0.142}, {0}, {0, 1}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rx, {0.528}, {0}, {0, 1}, 1);
    circ.add_conditional_gate<unsigned>(OpType::Rz, {1.}, {0}, {0, 1}, 1);
    circ.add_op<unsigned>(OpType::Rz, 0.142, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.528, {0});
    circ.add_op<unsigned>(OpType::Rz, 1., {0});
    circ.add_op<UnitID>(cond_cbox_op, {Bit(0), Qubit(0), Bit(1)});
    Transforms::decomp_boxes().apply(circ);
    OpTypeSet singleqs = {OpType::Rz, OpType::Rx, OpType::PhasedX};
    bool success =
        Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
            .apply(circ);
    REQUIRE(success);
    check_command_types(
        circ, {OpType::Rz, OpType::PhasedX, OpType::Conditional,
               OpType::Conditional, OpType::Conditional, OpType::Conditional,
               OpType::Conditional, OpType::Conditional, OpType::Rz,
               OpType::PhasedX, OpType::Conditional, OpType::Conditional});
    success = Transforms::squash_factory(singleqs, CircPool::tk1_to_PhasedXRz)
                  .apply(circ);
    REQUIRE_FALSE(success);
  }
}

SCENARIO("Decomposing TK1 into Rx, Ry") {
  Circuit circ(1);
  circ.add_op<unsigned>(OpType::TK1, {0.2, 0.2, 0.3}, {0});
  Transforms::decompose_XY().apply(circ);
  REQUIRE(circ.count_gates(OpType::Rx) == 2);
  REQUIRE(circ.count_gates(OpType::Ry) == 3);
}

SCENARIO("Squishing a circuit into U3 and CNOTs") {
  GIVEN("A series of one-qubit gates and CNOTs") {
    Circuit test1(4);
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::CX, {0, 1});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::CX, {0, 1});
    test1.add_op<unsigned>(OpType::Z, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::Rz, 0.2, {0});
    test1.add_op<unsigned>(OpType::Rz, -0.2, {0});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::Z, {0});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::CX, {1, 2});
    test1.add_op<unsigned>(OpType::CX, {2, 1});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::X, {0});
    test1.add_op<unsigned>(OpType::Y, {3});
    test1.add_op<unsigned>(OpType::Rx, 0.33, {3});
    test1.add_op<unsigned>(OpType::Rx, 1.67, {3});
    unsigned num_vertices = test1.n_vertices();
    unsigned num_of_pairs = 3;
    WHEN("Annihilation,conversion and squashing is done") {
      Transforms::remove_redundancies().apply(test1);
      REQUIRE(test1.n_vertices() == num_vertices - 2 * num_of_pairs);
      Transforms::decompose_single_qubits_TK1().apply(test1);
      Transforms::squash_1qb_to_tk1().apply(test1);
      test1.assert_valid();
      THEN("Circuit is shrunk to the correct depth") {
        REQUIRE(test1.depth() == 6);
      }
    }
  }
  GIVEN("A circuit which cannot be squished") {
    Circuit test1(1);
    test1.add_op<unsigned>(OpType::X, {0});
    WHEN("A squish is attempted") {
      REQUIRE(Transforms::decompose_single_qubits_TK1().apply(test1));
      THEN("Nothing happens to the circuit except an op label change") {
        REQUIRE(test1.depth() == 1);
        REQUIRE(test1.count_gates(OpType::TK1) == 1);
      }
    }
  }
  GIVEN("A circuit with 0 parameter ops") {
    Circuit test(1);
    test.add_op<unsigned>(OpType::Rx, 0., {0});
    test.add_op<unsigned>(OpType::Rx, 0.67, {0});
    test.add_op<unsigned>(OpType::Rx, 1.33, {0});
    test.add_op<unsigned>(OpType::Rz, 1.5, {0});
    test.add_op<unsigned>(OpType::Rz, 0.5, {0});
    test.add_op<unsigned>(OpType::H, {0});
    test.add_op<unsigned>(OpType::X, {0});
    test.add_op<unsigned>(OpType::X, {0});
    test.add_op<unsigned>(OpType::Y, {0});
    test.add_op<unsigned>(OpType::H, {0});
    test.add_op<unsigned>(OpType::Z, {0});
    test.add_op<unsigned>(OpType::Z, {0});

    REQUIRE(Transforms::remove_redundancies().apply(test));
    auto slices = test.get_slices();
    REQUIRE(slices.size() == 3);
    REQUIRE(test.get_OpType_from_Vertex(*slices[0].begin()) == OpType::H);
    REQUIRE(test.get_OpType_from_Vertex(*slices[1].begin()) == OpType::Y);
    REQUIRE(test.get_OpType_from_Vertex(*slices[2].begin()) == OpType::H);
  }
}

SCENARIO("Test commutation through CXsw", "[transform]") {
  GIVEN("Circuit with several instances of CX-Z") {
    Circuit circ;
    circ.add_blank_wires(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {0});
    WHEN("Pattern match performed") {
      // std::vector<Subcircuit> patterns = circ.pattern_match_CX_Rz();
      // REQUIRE(patterns.size()==2);
      THEN("Circuit is replaced with pattern") {
        Transform seq = Transforms::commute_through_multis() >>
                        Transforms::remove_redundancies();
        Transform repeat = Transforms::repeat_with_metric(
            seq, [](const Circuit &circ) { return circ.depth(); });
        repeat.apply(circ);
        REQUIRE(circ.n_vertices() == 5);
      }
    }
  }

  GIVEN("Circuit with no instances") {
    Circuit circ;
    circ.add_blank_wires(3);
    for (int i = 0; i < 3; ++i) {
      circ.add_op<unsigned>(OpType::CX, {0, 1});
    }
    Circuit new_circ = circ;
    Transforms::commute_through_multis().apply(circ);
    REQUIRE(circ.n_vertices() == new_circ.n_vertices());
    REQUIRE(circ.n_edges() == new_circ.n_edges());

    // method to verify two circuits are identical (in vertex ordering, not just
    // an isomorphism)
    SliceVec circslice = circ.get_slices();
    SliceVec newcircslice = new_circ.get_slices();
    for (unsigned i = 0; i < circslice.size(); ++i) {
      Slice::iterator k = newcircslice[i].begin();
      for (Slice::iterator j = circslice[i].begin(); j != circslice[i].end();
           ++j) {
        REQUIRE(
            circ.get_Op_ptr_from_Vertex(*k) ==
            new_circ.get_Op_ptr_from_Vertex(*j));
        ++k;
      }
    }
  }

  GIVEN("A UCCSD example") {
    auto circ = CircuitsForTesting::get().uccsd;
    const StateVector s0 = tket_sim::get_statevector(circ);
    REQUIRE(circ.count_gates(OpType::Rx) == 12);
    REQUIRE(circ.count_gates(OpType::Rz) == 2);
    REQUIRE(circ.count_gates(OpType::CX) == 12);
    REQUIRE(circ.count_gates(OpType::TK1) == 0);
    Transforms::commute_through_multis().apply(circ);
    REQUIRE(circ.count_gates(OpType::Rx) == 12);
    REQUIRE(circ.count_gates(OpType::Rz) == 2);
    REQUIRE(circ.count_gates(OpType::CX) == 12);
    REQUIRE(circ.count_gates(OpType::TK1) == 0);
    Transforms::squash_1qb_to_tk1().apply(circ);
    REQUIRE(circ.count_gates(OpType::Rx) == 0);
    REQUIRE(circ.count_gates(OpType::Rz) == 0);
    REQUIRE(circ.count_gates(OpType::CX) == 12);
    REQUIRE(circ.count_gates(OpType::TK1) == 12);
    const StateVector s1 = tket_sim::get_statevector(circ);
    REQUIRE(tket_sim::compare_statevectors_or_unitaries(s0, s1));
  }
}

SCENARIO(
    "Test that multi qubit conversion for IBM spits out message if no "
    "conversion can be done",
    "[transform][multi_qubit]") {
  Circuit circ(3);
  Transforms::decompose_multi_qubits_CX().apply(circ);
}

SCENARIO(
    "Test that annihilate works with new functionality",
    "[transform][annihilate]") {
  GIVEN("A circuit with some conjugate ops") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::S, {0});
    circ.add_op<unsigned>(OpType::Sdg, {0});
    circ.add_op<unsigned>(OpType::Tdg, {0});
    circ.add_op<unsigned>(OpType::T, {1});
    circ.add_op<unsigned>(OpType::Rx, 0., {1});
    circ.add_op<unsigned>(OpType::Rz, 0., {0});
    WHEN("Annihilate is performed") {
      REQUIRE(Transforms::remove_redundancies().apply(circ));
      REQUIRE(circ.n_vertices() == 5);
    }
  }
  GIVEN("A large circuit with lots of CXs that should all annihilate") {
    unsigned N = 1000;
    Circuit circ(N + 1);
    for (unsigned i = 0; i < N; ++i) {
      circ.add_op<unsigned>(OpType::CX, {i, i + 1});
    }
    for (unsigned i = 0; i < N; ++i) {
      unsigned a = N - i;
      circ.add_op<unsigned>(OpType::CX, {a - 1, a});
    }
    REQUIRE(Transforms::remove_redundancies().apply(circ));
    REQUIRE(circ.n_vertices() == (2 * N + 2));
    REQUIRE(circ.count_gates(OpType::CX) == 0);
  }
  GIVEN(
      "A large circuit with lots of CXs that should not annihilate (ports "
      "dont match") {
    unsigned N = 50;
    Circuit circ(N + 1);
    for (unsigned i = 0; i < N; ++i) {
      circ.add_op<unsigned>(OpType::CX, {i, i + 1});
    }
    for (unsigned i = 0; i < N; ++i) {
      unsigned a = N - i;
      circ.add_op<unsigned>(OpType::CX, {a, a - 1});
    }
    REQUIRE(!Transforms::remove_redundancies().apply(circ));
    REQUIRE(circ.n_vertices() == (4 * N + 2));
    REQUIRE(circ.count_gates(OpType::CX) == 2 * N);
  }
  GIVEN("A UCCSD example, with added gates") {
    auto circ = CircuitsForTesting::get().uccsd;
    REQUIRE(circ.count_gates(OpType::Rx) == 12);
    REQUIRE(circ.count_gates(OpType::Rz) == 2);
    REQUIRE(circ.count_gates(OpType::CX) == 12);

    // Extra gates not part of the common UCCSD example!
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0., {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});

    REQUIRE(circ.count_gates(OpType::Rx) == 12);
    REQUIRE(circ.count_gates(OpType::Rz) == 3);
    REQUIRE(circ.count_gates(OpType::CX) == 14);
    const StateVector s0 = tket_sim::get_statevector(circ);
    Transforms::remove_redundancies().apply(circ);
    REQUIRE(circ.count_gates(OpType::Rx) == 8);
    REQUIRE(circ.count_gates(OpType::Rz) == 2);
    REQUIRE(circ.count_gates(OpType::CX) == 12);
    const StateVector s1 = tket_sim::get_statevector(circ);
    REQUIRE(tket_sim::compare_statevectors_or_unitaries(s0, s1));
  }
}

SCENARIO("Molmer-Sorensen gate conversions") {
  GIVEN("A single MS gate") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::XXPhase, 0.4, {0, 1});
    bool success = Transforms::decompose_multi_qubits_CX().apply(circ);
    REQUIRE(success);
    success = Transforms::decompose_MolmerSorensen().apply(circ);
    REQUIRE(success);
    Transforms::squash_1qb_to_tk1().apply(circ);
    REQUIRE(circ.n_vertices() == 5);
  }
  GIVEN("A single CX gate") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    bool success = Transforms::decompose_MolmerSorensen().apply(circ);
    REQUIRE(success);
    success = Transforms::decompose_multi_qubits_CX().apply(circ);
    REQUIRE(success);
    Transforms::clifford_simp().apply(circ);
    REQUIRE(circ.count_gates(OpType::CX) == 1);
  }
  GIVEN("A CX and reset") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Reset, {0});
    bool success = Transforms::decompose_MolmerSorensen().apply(circ);
    REQUIRE(success);
    success = Transforms::decompose_multi_qubits_CX().apply(circ);
    REQUIRE(success);
    Transforms::clifford_simp().apply(circ);
    REQUIRE(circ.count_gates(OpType::CX) == 1);
  }
}

SCENARIO("Decomposition of multi-qubit gates") {
  GIVEN("A single CU1 gate") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CU1, 0.3, {0, 1});
    bool success = Transforms::rebase_tket().apply(circ);
    REQUIRE(success);
    REQUIRE(circ.n_vertices() > 7);
  }

  GIVEN("Failed qft circuit") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::X, {2});
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CU1, 0.5, {1, 0});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::CU1, 0.25, {2, 0});
    circ.add_op<unsigned>(OpType::CU1, 0.5, {2, 1});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::CU1, 0.125, {3, 0});
    circ.add_op<unsigned>(OpType::CU1, 0.25, {3, 1});
    circ.add_op<unsigned>(OpType::CU1, 0.5, {3, 2});
    circ.add_op<unsigned>(OpType::H, {3});
    circ.add_op<unsigned>(OpType::Collapse, {0});
    circ.add_op<unsigned>(OpType::Collapse, {1});
    circ.add_op<unsigned>(OpType::Collapse, {2});
    circ.add_op<unsigned>(OpType::Collapse, {3});
    bool success = Transforms::rebase_tket().apply(circ);
    REQUIRE(success);
    REQUIRE(circ.count_gates(OpType::CU1) == 0);
  }

  GIVEN("TK2 gate") {
    Circuit circ(2);
    double a = 0.3, b = 0.4, c = 1.85;
    circ.add_op<unsigned>(OpType::TK2, {a, b, c}, {0, 1});
    Eigen::MatrixXcd u = tket_sim::get_unitary(circ);
    Transforms::decompose_multi_qubits_CX().apply(circ);
    Eigen::MatrixXcd u1 = tket_sim::get_unitary(circ);
    REQUIRE(u1.isApprox(u));
  }
}

SCENARIO("Test synthesise_UMD") {
  GIVEN("3 expressions which =0") {
    Expr a = 0.;
    Expr b = 0.;
    Expr c = 0.;
    Circuit circ = CircPool::tk1_to_PhasedXRz(a, b, c);
    Transforms::remove_redundancies().apply(circ);
    REQUIRE(circ.n_gates() == 0);
  }
  GIVEN("An Rz in disguise") {
    Expr a = 0.3;
    Expr b = 0.;
    Expr c = 1.3;
    Circuit circ = CircPool::tk1_to_PhasedXRz(a, b, c);
    REQUIRE(circ.n_gates() == 1);
  }
  GIVEN("Y-gate") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Y, {0});
    const StateVector sv1 = tket_sim::get_statevector(circ);
    REQUIRE(Transforms::synthesise_UMD().apply(circ));
    const StateVector sv2 = tket_sim::get_statevector(circ);
    REQUIRE(tket_sim::compare_statevectors_or_unitaries(sv1, sv2));
    REQUIRE(circ.n_gates() == 1);
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(circ.get_slices()[0][0]);
    Expr p1 = (op)->get_params()[0];
    Expr p2 = (op)->get_params()[1];
    REQUIRE(test_equiv_val(p1, 1.0));
    REQUIRE(test_equiv_val(p2, 0.5));
  }
  GIVEN("Small 1qb circuit") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::Rx, 1.33, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.17, {0});
    StateVector sv1 = tket_sim::get_statevector(circ);

    REQUIRE(Transforms::synthesise_UMD().apply(circ));
    REQUIRE(Transforms::synthesise_tket().apply(circ));
    StateVector sv2 = tket_sim::get_statevector(circ);

    REQUIRE(tket_sim::compare_statevectors_or_unitaries(sv1, sv2));
  }
  GIVEN("CX circuit") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    StateVector sv1 = tket_sim::get_statevector(circ);

    REQUIRE(Transforms::synthesise_UMD().apply(circ));
    REQUIRE(circ.n_gates() == 5);
    REQUIRE(circ.count_gates(OpType::PhasedX) == 3);
    REQUIRE(circ.count_gates(OpType::Rz) == 1);
    REQUIRE(circ.count_gates(OpType::XXPhase) == 1);

    REQUIRE(Transforms::synthesise_tket().apply(circ));
    StateVector sv2 = tket_sim::get_statevector(circ);

    REQUIRE(tket_sim::compare_statevectors_or_unitaries(sv1, sv2));
  }
  GIVEN("Phase gadget") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    StateVector sv1 = tket_sim::get_statevector(circ);

    REQUIRE(Transforms::synthesise_UMD().apply(circ));
    REQUIRE(Transforms::synthesise_tket().apply(circ));
    StateVector sv2 = tket_sim::get_statevector(circ);

    REQUIRE(tket_sim::compare_statevectors_or_unitaries(sv1, sv2));
  }
}

SCENARIO("Copying Z and X through a CX") {
  GIVEN("A CX followed by a Z") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {1});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    REQUIRE(Transforms::copy_pi_through_CX().apply(circ));
    REQUIRE(circ.count_gates(OpType::Z) == 2);
    REQUIRE(circ.count_gates(OpType::CX) == 1);
  }
  GIVEN("A CX followed by a Z") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    REQUIRE(Transforms::copy_pi_through_CX().apply(circ));
    REQUIRE(circ.count_gates(OpType::X) == 2);
    REQUIRE(circ.count_gates(OpType::CX) == 1);
  }
  GIVEN("A Z on the commuting side") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {0});
    REQUIRE(!Transforms::copy_pi_through_CX().apply(circ));
  }
  GIVEN("A X on the commuting side") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::X, {1});
    REQUIRE(!Transforms::copy_pi_through_CX().apply(circ));
  }
  GIVEN("Two CXs to commute through - previously broke by yielding a cycle") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::X, {0});
    Transforms::copy_pi_through_CX().apply(circ);
    REQUIRE_NOTHROW(circ.depth_by_type(OpType::CX));
  }
}

SCENARIO("Test barrier blocks transforms successfully") {
  GIVEN("Small circuit with barrier") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::U1, 0.5, {0});
    circ.add_barrier(uvec{0});
    circ.add_op<unsigned>(OpType::U1, 0.5, {0});
    REQUIRE(!Transforms::remove_redundancies().apply(circ));
    REQUIRE_THROWS_AS(
        Transforms::pairwise_pauli_gadgets().apply(circ), BadOpType);
  }
  GIVEN("Bigger circuit with barrier") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CZ, {1, 2});
    circ.add_op<unsigned>(OpType::CZ, {1, 2});
    circ.add_barrier({0, 1, 2});
    REQUIRE(verify_n_qubits_for_ops(circ));
    REQUIRE(Transforms::remove_redundancies().apply(circ));
    REQUIRE(verify_n_qubits_for_ops(circ));
    REQUIRE(circ.depth() == 1);
    REQUIRE(circ.depth_by_type(OpType::Barrier) == 1);
  }
  GIVEN("Controlled gates with barrier") {
    Circuit circ(8);
    circ.add_op<unsigned>(OpType::CnRy, 0.4, {0, 1, 2, 3, 4, 5, 6, 7});
    circ.add_op<unsigned>(OpType::CnRx, 0.4, {0, 1, 2, 3, 4, 5, 6, 7});
    circ.add_op<unsigned>(OpType::CnRz, 0.4, {0, 1, 2, 3, 4, 5, 6, 7});
    circ.add_op<unsigned>(OpType::CX, {6, 7});
    circ.add_barrier({0, 1, 2, 3});
    circ.add_op<unsigned>(OpType::CX, {6, 7});
    circ.add_op<unsigned>(OpType::CnRz, -0.4, {0, 1, 2, 3, 4, 5, 6, 7});
    circ.add_op<unsigned>(OpType::CnRx, -0.4, {0, 1, 2, 3, 4, 5, 6, 7});
    circ.add_op<unsigned>(OpType::CnRy, -0.4, {0, 1, 2, 3, 4, 5, 6, 7});
    REQUIRE(verify_n_qubits_for_ops(circ));
    REQUIRE(circ.n_gates() == 9);
    REQUIRE(Transforms::remove_redundancies().apply(circ));
    REQUIRE(verify_n_qubits_for_ops(circ));
    REQUIRE(circ.depth_by_type(OpType::Barrier) == 1);
    REQUIRE(circ.n_gates() == 7);  // both CXs removed
    Circuit rep(4);
    const Op_ptr bar =
        std::make_shared<BarrierOp>(op_signature_t(4, EdgeType::Quantum));
    REQUIRE(circ.substitute_all(rep, bar));
    REQUIRE(Transforms::remove_redundancies().apply(circ));
    REQUIRE(verify_n_qubits_for_ops(circ));
    REQUIRE(circ.n_gates() == 0);
  }
  GIVEN("Barrier blocking some but not all single-qubit optimisations") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.6, {0});
    circ.add_barrier(uvec{0});
    circ.add_op<unsigned>(OpType::Rx, 0.8, {0});
    REQUIRE(Transforms::synthesise_tket().apply(circ));
    REQUIRE(circ.depth() == 2);
    REQUIRE(circ.depth_by_type(OpType::Barrier) == 1);
  }
}

SCENARIO("Check the identification of ZZPhase gates works correctly") {
  GIVEN("A circuit with no ZZPhase gates") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    REQUIRE(!Transforms::decompose_ZZPhase().apply(circ));
  }
  GIVEN("A circuit with 2 ZZPhase gates") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rx, 0.6, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    REQUIRE(Transforms::decompose_ZZPhase().apply(circ));
    REQUIRE(circ.count_gates(OpType::ZZPhase) == 2);
  }
  GIVEN("A circuit with a larger PhaseGadget structure but only 1 ZZ") {
    Circuit circ(4);
    add_2qb_gates(circ, OpType::CX, {{3, 2}, {2, 0}, {0, 1}});
    circ.add_op<unsigned>(OpType::Rx, 0.3, {0});
    add_2qb_gates(circ, OpType::CX, {{0, 1}, {2, 0}, {3, 2}});
    REQUIRE(Transforms::decompose_ZZPhase().apply(circ));
    REQUIRE(circ.count_gates(OpType::ZZPhase) == 1);
    REQUIRE(circ.count_gates(OpType::CX) == 4);
  }
}

SCENARIO("Decomposition of XXPhase and YYPhase into ZZPhase") {
  unsigned exp_n_zzphase;
  Circuit c;
  GIVEN("A circuit with a single XXPhase") {
    c = Circuit(2);
    c.add_op<unsigned>(OpType::XXPhase, 0.3, {0, 1});
    exp_n_zzphase = 1;
  }
  GIVEN("A circuit with a single YYPhase") {
    c = Circuit(2);
    c.add_op<unsigned>(OpType::YYPhase, 0.3, {0, 1});
    exp_n_zzphase = 1;
  }
  GIVEN("A circuit with 1x XXPhase, 2x YYPhase and 1xZZPhase") {
    c = Circuit(3);
    c.add_op<unsigned>(OpType::XXPhase, 0.3, {0, 1});
    c.add_op<unsigned>(OpType::YYPhase, 0.7, {1, 2});
    c.add_op<unsigned>(OpType::ZZPhase, 0.88, {0, 2});
    c.add_op<unsigned>(OpType::YYPhase, 0.38, {0, 2});
    exp_n_zzphase = 4;
  }
  GIVEN("A XXPhase(a) symbolic circ") {
    c = Circuit(2);
    Sym a = SymEngine::symbol("alpha");
    Expr alpha(a);
    c.add_op<unsigned>(OpType::XXPhase, alpha, {0, 1});
    exp_n_zzphase = 1;
  }
  REQUIRE(Transforms::decompose_ZZPhase().apply(c));
  REQUIRE(c.count_gates(OpType::ZZPhase) == exp_n_zzphase);
  REQUIRE(!Transforms::decompose_ZZPhase().apply(c));
}

SCENARIO("Test TK1 gate decomp for some gates") {
  std::vector<Expr> pars = {
      0.3, 0.7, 0.8};  // no ops required >3 params currently
  std::set<OpType> cant_do = {
      OpType::Input,        OpType::Output,       OpType::ClInput,
      OpType::ClOutput,     OpType::WASMInput,    OpType::WASMOutput,
      OpType::noop,         OpType::Reset,        OpType::BRIDGE,
      OpType::Unitary1qBox, OpType::Unitary2qBox, OpType::Unitary3qBox,
      OpType::ExpBox,       OpType::PauliExpBox,  OpType::CustomGate,
      OpType::Collapse,     OpType::Measure,      OpType::Label,
      OpType::Branch,       OpType::Goto,         OpType::Stop,
      OpType::Create,       OpType::Discard,      OpType::RNGInput,
      OpType::RNGOutput,    OpType::RNGSeed,      OpType::RNGBound,
      OpType::RNGIndex,     OpType::RNGNum,       OpType::JobShotNum};
  for (const std::pair<const OpType, OpTypeInfo> &map_pair : optypeinfo()) {
    OpTypeInfo oti = map_pair.second;
    if (!oti.signature) continue;
    if (cant_do.find(map_pair.first) != cant_do.end()) continue;
    unsigned n_qbs = oti.signature->size();
    Circuit circ(n_qbs);
    std::vector<Expr> params(pars.begin(), pars.begin() + oti.n_params());
    std::vector<unsigned> qbs(n_qbs);
    std::iota(qbs.begin(), qbs.end(), 0);
    circ.add_op<unsigned>(map_pair.first, params, qbs);
    Transforms::rebase_tket().apply(circ);
    Circuit circ2 = circ;
    Transforms::decompose_ZX().apply(circ2);
    const StateVector sv2 = tket_sim::get_statevector(circ2);
    Transforms::decompose_tk1_to_rzrx().apply(circ);
    const StateVector sv = tket_sim::get_statevector(circ);
    REQUIRE(tket_sim::compare_statevectors_or_unitaries(sv, sv2));
  }
}

SCENARIO("Testing in_weyl_chamber") {
  GIVEN("Normalised angles (1)") { REQUIRE(in_weyl_chamber({0.5, 0.5, 0})); }
  GIVEN("Normalised angles (2)") { REQUIRE(in_weyl_chamber({0.5, 0.3, 0})); }
  GIVEN("Normalised angles (2)") { REQUIRE(in_weyl_chamber({0.3, 0.3, -0.2})); }
  GIVEN("Non normalised angles (1)") {
    REQUIRE_FALSE(in_weyl_chamber({0.3, 0.3, -0.31}));
  }
  GIVEN("Non normalised angles (2)") {
    REQUIRE_FALSE(in_weyl_chamber({0.2, 0.3, 0}));
  }
  GIVEN("Non normalised angles (3)") {
    REQUIRE_FALSE(in_weyl_chamber({1, 0, 0}));
  }
  GIVEN("Non normalised angles (4)") {
    REQUIRE_FALSE(in_weyl_chamber({0, 0, 0.1}));
  }
  GIVEN("A close to invalid TK2") {
    Circuit c = CircPool::TK2_using_normalised_TK2(
        3.48828125, 0.51171875000000022, 0.48828124999999983);
    Vertex tk2 = *c.get_gates_of_type(OpType::TK2).begin();
    Op_ptr op = c.get_Op_ptr_from_Vertex(tk2);
    auto params = op->get_params();
    REQUIRE(in_weyl_chamber({params[0], params[1], params[2]}));
  }
}

SCENARIO("Testing decompose_TK2") {
  GIVEN("Parameterless decompose_TK2") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::TK2, {0.3, 0.1, 0.}, {0, 1});
    REQUIRE(Transforms::decompose_TK2().apply(c));
    REQUIRE(c.count_gates(OpType::CX) == 2);
    REQUIRE(c.count_gates(OpType::TK2) == 0);
    REQUIRE(!Transforms::decompose_TK2().apply(c));
  }
  GIVEN("Prioritise ZZPhase over ZZMax for equal fidelity (1)") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::TK2, {0.3, 0., 0.}, {0, 1});
    Transforms::TwoQbFidelities fid;
    fid.ZZPhase_fidelity = [](double) { return 1.; };
    fid.ZZMax_fidelity = 1.;
    REQUIRE(Transforms::decompose_TK2(fid).apply(c));
    REQUIRE(c.count_gates(OpType::ZZPhase) == 1);
    REQUIRE(c.count_gates(OpType::TK2) == 0);
    REQUIRE(c.count_gates(OpType::ZZMax) == 0);
    REQUIRE(!Transforms::decompose_TK2().apply(c));
  }
  GIVEN("Prioritise ZZPhase over ZZMax for equal fidelity (2)") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::TK2, {0.3, 0., 0.}, {0, 1});
    Transforms::TwoQbFidelities fid;
    fid.ZZPhase_fidelity = [](double) { return .9; };
    fid.ZZMax_fidelity = .9;
    REQUIRE(Transforms::decompose_TK2(fid).apply(c));
    REQUIRE(c.count_gates(OpType::ZZPhase) == 1);
    REQUIRE(c.count_gates(OpType::TK2) == 0);
    REQUIRE(c.count_gates(OpType::ZZMax) == 0);
    REQUIRE(!Transforms::decompose_TK2().apply(c));
  }

  // some useful symbolics
  Sym a = SymEngine::symbol("alpha");
  Expr alpha(a);
  Sym b = SymEngine::symbol("beta");
  Expr beta(b);
  Sym c = SymEngine::symbol("gamma");
  Expr gamma(c);

  GIVEN("Not in Weyl chamber error") {
    std::vector<std::vector<Expr>> params{
        {0.1, 0.3, 0.}, {0.6, 0, 0}, {0.4, 0.1, -0.2}, {0.2, alpha, 0}};
    for (auto angles : params) {
      Circuit c(2);
      c.add_op<unsigned>(OpType::TK2, angles, {0, 1});
      REQUIRE_THROWS_AS(
          Transforms::decompose_TK2().apply(c), std::domain_error);
    }
  }

  std::vector<std::vector<Expr>> params;
  std::vector<unsigned> exp_n_cx(4, 0);
  std::vector<unsigned> exp_n_zzmax(4, 0);
  std::vector<unsigned> exp_n_zzphase(4, 0);
  Transforms::TwoQbFidelities fid;
  bool is_symbolic = false;
  double eps = ERR_EPS;

  GIVEN("A bunch of TK2 gates, no fidelities") {
    params = {{.5, 0., 0.}, {.4, 0., 0.}, {.2, .2, 0.}, {0.2, 0.1, 0.08}};
    exp_n_cx = {1, 2, 2, 3};
  }
  GIVEN("A bunch of TK2 gates, perfect ZZMax fidelities") {
    fid.ZZMax_fidelity = 1.;
    params = {
        {0., 0., 0.},
        {.5, 0., 0.},
        {.4, 0., 0.},
        {.2, .2, 0.},
        {0.2, 0.1, 0.1}};
    exp_n_zzmax = {0, 1, 2, 2, 3};
    exp_n_zzphase = std::vector<unsigned>(exp_n_zzmax.size(), 0);
    exp_n_cx = std::vector<unsigned>(exp_n_zzmax.size(), 0);
  }
  GIVEN("A bunch of TK2 gates, ZZMax vs ZZPhase fidelities") {
    fid.ZZMax_fidelity = .99;
    fid.ZZPhase_fidelity = [](double angle) { return 1 - angle / 10.; };
    params = {
        {.5, 0., 0.},      // use single ZZMax
        {.48, 0., 0.},     // use single ZZMax (approx)
        {.4, 0., 0.},      // use two ZZMax
        {0.4, 0.1, 0.},    // use two ZZMax
        {0.4, 0.1, 0.01},  // use two ZZMax (approx)
        {.4, 0.3, 0.2},    // use three ZZMax
        {.1, 0., 0.},      // use single ZZPhase
        {0.05, 0.01, 0},   // use identity (approx)
        {0.1, 0.01, 0},    // use single ZZPhase (approx)
        {0.3, 0.01, 0},    // use two ZZMax
        {0.49, 0.01, 0},   // use single ZZMax (approx)
        {0.1, 0.1, 0},     // use two ZZMax
    };
    exp_n_zzmax = {1, 1, 2, 2, 2, 3, 0, 0, 0, 2, 1, 2};
    exp_n_zzphase = {0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0};
    exp_n_cx = std::vector<unsigned>(exp_n_zzmax.size(), 0);
    eps = 0.98;
  }
  GIVEN("Force use ZZPhase") {
    fid.ZZPhase_fidelity = [](double) { return 1.; };
    params = {{0., 0., 0.}, {0.3, 0., 0.}, {0.4, 0.3, 0.}, {0.4, 0.4, -0.3}};
    exp_n_zzphase = {0, 1, 2, 3};
  }
  GIVEN("Symbolic cases") {
    is_symbolic = true;
    params = {
        {alpha, 0., 0.},
        {alpha, beta, gamma},
        {alpha, 0.2, 0},
        {alpha, 0.1, 0.05}};
    GIVEN("Using default") { exp_n_cx = {2, 3, 2, 3}; }
    GIVEN("Using CX") {
      fid.CX_fidelity = 1.;
      exp_n_cx = {2, 3, 2, 3};
    }
    GIVEN("Using ZZMax") {
      fid.ZZMax_fidelity = 1.;
      exp_n_zzmax = {2, 3, 2, 3};
    }
    GIVEN("Force use ZZPhase") {
      fid.ZZPhase_fidelity = [](double) { return 1.; };
      exp_n_zzphase = {1, 3, 2, 3};
    }
    GIVEN("Either ZZMax or ZZPhase") {
      fid.ZZPhase_fidelity = [](double) { return 1.; };
      fid.ZZMax_fidelity = 1.;
      exp_n_zzphase = {1, 0, 0, 0};
      exp_n_zzmax = {0, 3, 2, 3};
    }
  }

  Eigen::MatrixXcd u1, u2;
  for (unsigned i = 0; i < params.size(); ++i) {
    Circuit c(2);
    c.add_op<unsigned>(OpType::TK2, params[i], {0, 1});

    Circuit c1 = c;
    REQUIRE(Transforms::decompose_TK2(fid).apply(c));
    Circuit c2 = c;

    if (is_symbolic) {
      // Substitute "arbitrary" values for symbols in both circuits
      const SymSet symbols = c.free_symbols();
      symbol_map_t smap;
      unsigned i = 0;
      for (const Sym &s : symbols) {
        smap[s] = PI * (i + 1) / ((i + 2) * (i + 3));
        i++;
      }
      c1.symbol_substitution(smap);
      c2.symbol_substitution(smap);
    }

    u1 = tket_sim::get_unitary(c1);
    u2 = tket_sim::get_unitary(c2);

    REQUIRE(u1.isApprox(u2, eps));
    REQUIRE(c.count_gates(OpType::CX) == exp_n_cx[i]);
    REQUIRE(c.count_gates(OpType::ZZMax) == exp_n_zzmax[i]);
    REQUIRE(c.count_gates(OpType::ZZPhase) == exp_n_zzphase[i]);
    REQUIRE(!Transforms::decompose_TK2().apply(c));
  }
  GIVEN("ZZPhase circuit with, single, small angles.") {
    fid.ZZPhase_fidelity = [](double) { return 1.; };
    Circuit circ_5(2);
    circ_5.add_op<unsigned>(tket::OpType::TK2, {0, 0, 1e-5}, {0, 1});
    Transforms::normalise_TK2().apply(circ_5);
    Transforms::decompose_TK2(fid).apply(circ_5);
    REQUIRE(circ_5.count_gates(OpType::ZZPhase) == 1);

    Circuit circ_6(2);
    circ_6.add_op<unsigned>(tket::OpType::TK2, {0, 0, 1e-6}, {0, 1});
    Transforms::normalise_TK2().apply(circ_6);
    Transforms::decompose_TK2(fid).apply(circ_6);
    REQUIRE(circ_6.count_gates(OpType::ZZPhase) == 0);
  }
}

SCENARIO("DecomposeTK2, implicit swaps") {
  Circuit c(2);
  unsigned n_noswap = 0, n_swap = 0;
  GIVEN("A swap") {
    c.add_op<unsigned>(OpType::SWAP, {0, 1});
    n_noswap = 3;
    n_swap = 0;
  }
  GIVEN("A 3-CX swap") {
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::CX, {1, 0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    n_noswap = 3;
    n_swap = 0;
  }
  GIVEN("2-CX") {
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::CX, {1, 0});
    n_noswap = 2;
    n_swap = 1;
  }
  GIVEN("TK2(0.5, 0.5, 0)") {
    c.add_op<unsigned>(OpType::TK2, {0.5, 0.5, 0}, {0, 1});
    n_noswap = 2;
    n_swap = 1;
  }
  GIVEN("TK2(0.5, 0.5, 3.91667)") {
    c.add_op<unsigned>(OpType::TK2, {0.5, 0.5, 3.91667}, {0, 1});
    n_noswap = 3;
    n_swap = 2;
  }

  (Transforms::synthesise_tk() >> Transforms::two_qubit_squash(OpType::TK2))
      .apply(c);

  Circuit c_res = c;
  StateVector s0 = tket_sim::get_statevector(c_res);
  Transforms::decompose_TK2(false).apply(c_res);
  StateVector s1 = tket_sim::get_statevector(c_res);
  REQUIRE(c_res.count_gates(OpType::CX) == n_noswap);
  REQUIRE(tket_sim::compare_statevectors_or_unitaries(s0, s1));

  c_res = c;
  Transforms::TwoQbFidelities fid;
  fid.ZZMax_fidelity = 1.;
  s0 = tket_sim::get_statevector(c_res);
  Transforms::decompose_TK2(fid, false).apply(c_res);
  s1 = tket_sim::get_statevector(c_res);
  REQUIRE(c_res.count_gates(OpType::ZZMax) == n_noswap);
  REQUIRE(c_res.count_gates(OpType::CX) == 0);
  REQUIRE(tket_sim::compare_statevectors_or_unitaries(s0, s1));

  c_res = c;
  s0 = tket_sim::get_statevector(c_res);
  Transforms::decompose_TK2(true).apply(c_res);
  s1 = tket_sim::get_statevector(c_res);
  REQUIRE(c_res.count_gates(OpType::CX) == n_swap);
  REQUIRE(tket_sim::compare_statevectors_or_unitaries(s0, s1));

  c_res = c;
  s0 = tket_sim::get_statevector(c_res);
  Transforms::decompose_TK2(fid, true).apply(c_res);
  s1 = tket_sim::get_statevector(c_res);
  REQUIRE(c_res.count_gates(OpType::ZZMax) == n_swap);
  REQUIRE(c_res.count_gates(OpType::CX) == 0);
  REQUIRE(tket_sim::compare_statevectors_or_unitaries(s0, s1));
}

SCENARIO("Testing absorb_Rz_NPhasedX") {
  Circuit circ(3);
  Expr exp_beta;
  unsigned exp_n_rz;
  Vertex nphasedx;
  GIVEN("All Rz can be absorbed") {
    for (unsigned i = 0; i < circ.n_qubits(); ++i) {
      circ.add_op<unsigned>(OpType::Rz, 0.3, {i});
    }
    nphasedx = circ.add_op<unsigned>(OpType::NPhasedX, {0.5, 0.}, {0, 1, 2});
    for (unsigned i = 0; i < circ.n_qubits(); ++i) {
      circ.add_op<unsigned>(OpType::Rz, -0.3, {i});
    }
    exp_beta = -0.3;
    exp_n_rz = 0;
  }
  GIVEN("All Rz can be absorbed, add to existing beta") {
    for (unsigned i = 0; i < circ.n_qubits(); ++i) {
      circ.add_op<unsigned>(OpType::Rz, 0.3, {i});
    }
    nphasedx = circ.add_op<unsigned>(OpType::NPhasedX, {0.5, 0.2}, {0, 1, 2});
    for (unsigned i = 0; i < circ.n_qubits(); ++i) {
      circ.add_op<unsigned>(OpType::Rz, -0.3, {i});
    }
    exp_beta = 0.2 - 0.3;
    exp_n_rz = 0;
  }
  GIVEN("3 Rz can be absorbed") {
    for (unsigned i = 0; i < circ.n_qubits(); ++i) {
      circ.add_op<unsigned>(OpType::Rz, 0.3, {i});
    }
    nphasedx = circ.add_op<unsigned>(OpType::NPhasedX, {0.5, 0.2}, {0, 1, 2});
    for (unsigned i = 0; i < circ.n_qubits(); ++i) {
      circ.add_op<unsigned>(OpType::Rz, i * 0.2, {i});
    }
    exp_beta = 0.2 - 0.3;
    exp_n_rz = 3;
  }
  GIVEN("Only act on a subset") {
    for (unsigned i = 0; i < circ.n_qubits(); ++i) {
      circ.add_op<unsigned>(OpType::Rz, 0.3, {i});
    }
    circ.add_op<unsigned>(OpType::Rz, 0.4, {2});
    nphasedx = circ.add_op<unsigned>(OpType::NPhasedX, {0.5, 0.2}, {0, 1});
    for (unsigned i = 0; i < circ.n_qubits(); ++i) {
      circ.add_op<unsigned>(OpType::Rz, i * 0.2, {i});
    }
    exp_beta = 0.2 - 0.3;
    exp_n_rz = 3;
  }
  GIVEN("3 Rz can be absorbed, 3 must be created") {
    for (unsigned i = 0; i < circ.n_qubits(); ++i) {
      circ.add_op<unsigned>(OpType::Rz, 0.3, {i});
    }
    nphasedx = circ.add_op<unsigned>(OpType::NPhasedX, {0.5, 0.2}, {0, 1, 2});
    exp_beta = 0.2 - 0.3;
    exp_n_rz = 3;
  }
  GIVEN("A more random configuration") {
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.6, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.899, {2});
    nphasedx =
        circ.add_op<unsigned>(OpType::NPhasedX, {0.213, 0.212231}, {0, 1, 2});
    circ.add_op<unsigned>(OpType::Rz, -0.6, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.1244, {2});
    exp_beta = 0.212231 - 0.6;
    exp_n_rz = 4;
  }
  GIVEN("beta should be zero") {
    circ.add_op<unsigned>(OpType::Rz, 0.6, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.6, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.899, {2});
    nphasedx =
        circ.add_op<unsigned>(OpType::NPhasedX, {0.213, 0.212231}, {0, 1, 2});
    circ.add_op<unsigned>(OpType::Rz, 0.6, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.1244, {2});
    exp_beta = 0.212231;
    exp_n_rz = 4;
  }

  if (circ.get_commands().size()) {
    auto orig_u = tket_sim::get_unitary(circ);
    REQUIRE(Transforms::absorb_Rz_NPhasedX().apply(circ));
    auto new_u = tket_sim::get_unitary(circ);

    REQUIRE(!Transforms::absorb_Rz_NPhasedX().apply(circ));
    REQUIRE(circ.count_gates(OpType::NPhasedX) == 1);
    REQUIRE(circ.count_gates(OpType::Rz) == exp_n_rz);
    Expr beta = circ.get_Op_ptr_from_Vertex(nphasedx)->get_params().at(1);
    REQUIRE(equiv_expr(beta, exp_beta, 4));
    REQUIRE(new_u.isApprox(orig_u));
  }

  GIVEN("A circuit with multiple NPhasedX gates") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.6, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.899, {2});
    circ.add_op<unsigned>(OpType::NPhasedX, {0.213, 0.212231}, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, -0.3, {0});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::Rz, -0.3, {1});
    circ.add_op<unsigned>(OpType::NPhasedX, {0.323, 0.231}, {1, 2});
    circ.add_op<unsigned>(OpType::Rz, 0.298, {2});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::Rz, 0.198, {1});
    circ.add_op<unsigned>(OpType::NPhasedX, {0.123, 0.345}, {0, 1, 2});
    circ.add_op<unsigned>(OpType::CX, {1, 0});

    auto orig_u = tket_sim::get_unitary(circ);
    REQUIRE(Transforms::absorb_Rz_NPhasedX().apply(circ));
    auto new_u = tket_sim::get_unitary(circ);

    REQUIRE(circ.count_gates(OpType::NPhasedX) == 3);
    REQUIRE(new_u.isApprox(orig_u));
  }
  GIVEN("A circuit with nothing to do") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::Rz, 0.3, {0});
    circ.add_op<unsigned>(OpType::NPhasedX, {0.213, 0.212231}, {0, 1, 2});
    circ.add_op<unsigned>(OpType::Rz, -0.3, {0});
    REQUIRE(!Transforms::absorb_Rz_NPhasedX().apply(circ));
  }
  GIVEN("A circuit with symbolics") {
    Sym asym = SymEngine::symbol("a");
    Sym bsym = SymEngine::symbol("b");
    Expr a(asym), b(bsym);

    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, -a, {0});
    circ.add_op<unsigned>(OpType::Rz, -a, {1});
    Vertex nphasedx =
        circ.add_op<unsigned>(OpType::NPhasedX, {0.213, b}, {0, 1});
    circ.add_op<unsigned>(OpType::Rz, a, {0});

    REQUIRE(Transforms::absorb_Rz_NPhasedX().apply(circ));
    Expr beta = circ.get_Op_ptr_from_Vertex(nphasedx)->get_params().at(1);
    REQUIRE(equiv_expr(beta, a + b));
  }
}

// Verify preconditions and postconditions for pass applied to circuit
static void check_conditions(PassPtr pp, const Circuit &c) {
  CompilationUnit cu(c);
  pp->apply(cu);
  const Circuit c1 = cu.get_circ_ref();
  PassConditions pcons = pp->get_conditions();
  PredicatePtrMap precons = pcons.first;
  PredicatePtrMap postcons = pcons.second.specific_postcons_;
  for (auto precon : precons) {
    PredicatePtr pred = precon.second;
    CHECK(pred->verify(c));
  }
  for (auto postcon : postcons) {
    PredicatePtr pred = postcon.second;
    CHECK(pred->verify(c1));
  }
}

SCENARIO("Synthesis with conditional gates") {
  GIVEN("Circuit with conditional U1") {
    // https://github.com/quantinuum/tket/issues/394
    Circuit c(3);
    c.add_c_register("c", 3);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_measure(0, 0);
    c.add_measure(1, 1);
    c.add_conditional_gate<unsigned>(OpType::U1, {0.25}, {1}, {0}, 1);
    c.add_conditional_gate<unsigned>(
        OpType::CnRy, {0.25}, {0, 1, 2}, {0, 1}, 0);
    c.add_conditional_gate<unsigned>(
        OpType::CnRx, {0.25}, {0, 1, 2}, {0, 1}, 0);
    c.add_conditional_gate<unsigned>(
        OpType::CnRz, {0.25}, {0, 1, 2}, {0, 1}, 0);
    c.add_measure(2, 2);
    check_conditions(SynthesiseTK(), c);
    check_conditions(SynthesiseTket(), c);
  }

  GIVEN("SynthesiseTK with conditional 2-qubit gates") {
    // https://github.com/quantinuum/tket/issues/1708
    Circuit c(2, 1);
    c.add_conditional_gate<unsigned>(OpType::ZZPhase, {0.5}, {0, 1}, {0}, 1);
    CompilationUnit cu(c);
    SynthesiseTK()->apply(cu);
    CHECK(cu.get_circ_ref().count_n_qubit_gates(2) == 1);
  }
}

SCENARIO("Restricting ZZPhase gate angles.") {
  Circuit c(2);
  c.add_op<unsigned>(OpType::ZZPhase, 0.5, {0, 1});
  c.add_op<unsigned>(OpType::ZZPhase, 1.4, {0, 1});
  c.add_op<unsigned>(OpType::ZZPhase, 1.0, {1, 0});
  c.add_op<unsigned>(OpType::ZZPhase, -0.5, {0, 1});
  c.add_op<unsigned>(OpType::ZZPhase, -1.3, {0, 1});
  c.add_op<unsigned>(OpType::ZZPhase, -1.0, {0, 1});

  REQUIRE(Transforms::ZZPhase_to_Rz().apply(c));
  check_conditions(ZZPhaseToRz(), c);

  Circuit comparison(2);
  comparison.add_op<unsigned>(OpType::ZZPhase, 0.5, {0, 1});
  comparison.add_op<unsigned>(OpType::ZZPhase, 1.4, {0, 1});
  comparison.add_op<unsigned>(OpType::Rz, 1.0, {0});
  comparison.add_op<unsigned>(OpType::Rz, 1.0, {1});
  comparison.add_op<unsigned>(OpType::ZZPhase, -0.5, {0, 1});
  comparison.add_op<unsigned>(OpType::ZZPhase, -1.3, {0, 1});
  comparison.add_op<unsigned>(OpType::Rz, 1.0, {0});
  comparison.add_op<unsigned>(OpType::Rz, 1.0, {1});

  REQUIRE(comparison == c);
}

SCENARIO("ZZPhase_to_Rz with symbolic angles") {
  // https://github.com/quantinuum/tket/issues/1051
  Sym asym = SymEngine::symbol("a");
  Expr a(asym);
  Circuit c(2);
  c.add_op<unsigned>(OpType::ZZPhase, a, {0, 1});
  CHECK_FALSE(Transforms::ZZPhase_to_Rz().apply(c));
}

SCENARIO("Test squash Rz PhasedX") {
  GIVEN("A simple circuit") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::CZ, {0, 1});
    c.add_op<unsigned>(OpType::Rz, 0.8, {0});
    c.add_op<unsigned>(OpType::Rz, 0.7, {1});
    c.add_op<unsigned>(OpType::CZ, {0, 1});
    c.add_op<unsigned>(OpType::Ry, 0.4, {0});
    c.add_op<unsigned>(OpType::Rz, 0.3, {0});
    c.add_op<unsigned>(OpType::Rx, 0.11, {0});
    c.add_op<unsigned>(OpType::CZ, {0, 1});
    c.add_op<unsigned>(OpType::Rz, 0.5, {0});
    c.add_op<unsigned>(OpType::Rz, 0.5, {1});
    const Eigen::MatrixXcd u = tket_sim::get_unitary(c);

    WHEN("Squash forwards") {
      AND_WHEN("Use squash_1qb_to_Rz_PhasedX") {
        bool reverse = false;
        auto squasher =
            std::make_unique<Transforms::RzPhasedXSquasher>(reverse);
        Transforms::decompose_ZX().apply(c);
        SingleQubitSquash(std::move(squasher), c, reverse).squash();
      }
      AND_WHEN("Use squash_1qb_to_Rz_PhasedX") {
        Transforms::squash_1qb_to_Rz_PhasedX().apply(c);
      }
      AND_WHEN("Use SquashRzPhasedX") {
        CompilationUnit cu(c);
        SquashRzPhasedX()->apply(cu);
        c = cu.get_circ_ref();
      }
      const Eigen::MatrixXcd v = tket_sim::get_unitary(c);
      REQUIRE(u.isApprox(v, ERR_EPS));
      std::vector<VertPort> q0_path = c.unit_path(Qubit(0));
      std::vector<VertPort> q1_path = c.unit_path(Qubit(1));
      REQUIRE(
          c.get_OpType_from_Vertex(q0_path[q0_path.size() - 2].first) ==
          OpType::Rz);
      REQUIRE(c.get_OpType_from_Vertex(q0_path[4].first) == OpType::PhasedX);
      REQUIRE(
          c.get_OpType_from_Vertex(q1_path[q1_path.size() - 2].first) ==
          OpType::Rz);
      REQUIRE(c.count_gates(OpType::Rz) == 2);
      REQUIRE(c.count_gates(OpType::PhasedX) == 1);
      REQUIRE(c.count_gates(OpType::CZ) == 3);
      REQUIRE(c.count_gates(OpType::CX) == 1);
      REQUIRE(c.n_gates() == 7);
    }

    WHEN("Squash backwards") {
      bool reverse = true;
      auto squasher = std::make_unique<Transforms::RzPhasedXSquasher>(reverse);
      Transforms::decompose_ZX().apply(c);
      SingleQubitSquash(std::move(squasher), c, reverse).squash();
      const Eigen::MatrixXcd v = tket_sim::get_unitary(c);
      REQUIRE(u.isApprox(v, ERR_EPS));
      std::vector<VertPort> q0_path = c.unit_path(Qubit(0));
      std::vector<VertPort> q1_path = c.unit_path(Qubit(1));
      REQUIRE(c.get_OpType_from_Vertex(q0_path[1].first) == OpType::Rz);
      REQUIRE(c.get_OpType_from_Vertex(q0_path[5].first) == OpType::PhasedX);
      REQUIRE(c.get_OpType_from_Vertex(q1_path[2].first) == OpType::Rz);
      REQUIRE(c.count_gates(OpType::Rz) == 2);
      REQUIRE(c.count_gates(OpType::PhasedX) == 1);
      REQUIRE(c.count_gates(OpType::CZ) == 3);
      REQUIRE(c.count_gates(OpType::CX) == 1);
      REQUIRE(c.n_gates() == 7);
    }
  }
  GIVEN("Special case: a Rx gate") {
    // Test there is no leftover Rz
    Circuit c(1);
    c.add_op<unsigned>(OpType::Rx, 0.77, {0});
    const Eigen::MatrixXcd u = tket_sim::get_unitary(c);
    Transforms::squash_1qb_to_Rz_PhasedX().apply(c);
    REQUIRE(c.count_gates(OpType::PhasedX) == 1);
    REQUIRE(c.n_gates() == 1);
    const Eigen::MatrixXcd v = tket_sim::get_unitary(c);
    REQUIRE(u.isApprox(v, ERR_EPS));
  }
  GIVEN("Special case: a decomposed phased X") {
    // Test there is no leftover Rz
    Circuit c(1);
    c.add_op<unsigned>(OpType::Rz, -0.6, {0});
    c.add_op<unsigned>(OpType::Rx, 1.3, {0});
    c.add_op<unsigned>(OpType::Rz, 0.6, {0});
    const Eigen::MatrixXcd u = tket_sim::get_unitary(c);
    Transforms::squash_1qb_to_Rz_PhasedX().apply(c);
    REQUIRE(c.count_gates(OpType::PhasedX) == 1);
    REQUIRE(c.n_gates() == 1);
    const Eigen::MatrixXcd v = tket_sim::get_unitary(c);
    REQUIRE(u.isApprox(v, ERR_EPS));
  }
  GIVEN("Special case: a Rz gate") {
    // Test there is no PhasedX
    Circuit c(1);
    c.add_op<unsigned>(OpType::Rz, 0.77, {0});
    const Eigen::MatrixXcd u = tket_sim::get_unitary(c);
    Transforms::squash_1qb_to_Rz_PhasedX().apply(c);
    REQUIRE(c.count_gates(OpType::Rz) == 1);
    REQUIRE(c.n_gates() == 1);
    const Eigen::MatrixXcd v = tket_sim::get_unitary(c);
    REQUIRE(u.isApprox(v, ERR_EPS));
  }

  GIVEN("A symbolic circuit") {
    Sym a = SymEngine::symbol("alpha");
    Expr alpha(a);
    Sym b = SymEngine::symbol("beta");
    Expr beta(b);
    Sym c = SymEngine::symbol("gamma");
    Expr gamma(c);

    Circuit circ(2);
    circ.add_op<unsigned>(OpType::PhaseGadget, beta, {0});
    circ.add_op<unsigned>(OpType::Rz, alpha, {0});
    circ.add_op<unsigned>(OpType::PhasedX, {gamma, beta}, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});

    Circuit circ2(circ);
    Transforms::squash_1qb_to_Rz_PhasedX().apply(circ2);
    auto cmds = circ2.get_commands();
    REQUIRE(cmds.size() == 3);
    REQUIRE(cmds[0].get_op_ptr()->get_type() == OpType::PhasedX);
    REQUIRE(cmds[1].get_op_ptr()->get_type() == OpType::CX);
    REQUIRE(cmds[2].get_op_ptr()->get_type() == OpType::Rz);

    symbol_map_t symbol_map;
    symbol_map[a] = Expr(0.3);
    symbol_map[b] = Expr(0.5);
    symbol_map[c] = Expr(1.);
    circ.symbol_substitution(symbol_map);
    circ2.symbol_substitution(symbol_map);
    const Eigen::MatrixXcd u = tket_sim::get_unitary(circ);
    const Eigen::MatrixXcd v = tket_sim::get_unitary(circ2);
    REQUIRE(u.isApprox(v, ERR_EPS));
  }

  GIVEN("Another symbolic circuit") {
    // https://github.com/quantinuum/tket/issues/1052
    Sym a = SymEngine::symbol("alpha");
    Expr alpha(a);
    Sym b = SymEngine::symbol("beta");
    Expr beta(b);

    Circuit circ(1);
    circ.add_op<unsigned>(OpType::PhasedX, {alpha, beta}, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {0});
    circ.add_op<unsigned>(OpType::PhasedX, {0.5, 0.5}, {0});
    Transforms::squash_1qb_to_Rz_PhasedX(true).apply(circ);
    OpTypeSet allowed = {OpType::Rz, OpType::PhasedX};
    for (const Command &cmd : circ) {
      OpType optype = cmd.get_op_ptr()->get_type();
      CHECK(allowed.contains(optype));
    }
  }

  GIVEN("A circuit with classical control (1)") {
    // https://github.com/quantinuum/tket/issues/1324
    Circuit circ(3, 1);
    circ.add_op<unsigned>(OpType::CY, {0, 1});
    circ.add_conditional_gate<unsigned>(OpType::Rz, {1.0}, {1}, {0}, 1);
    circ.add_measure(Qubit(2), Bit(0));
    circ.add_op<unsigned>(OpType::CX, {2, 0});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    std::unique_ptr<AbstractSquasher> squasher =
        std::make_unique<Transforms::RzPhasedXSquasher>();
    SingleQubitSquash sqs(std::move(squasher), circ);
    VertexVec inputs = circ.q_inputs();
    VertexVec outputs = circ.q_outputs();
    Edge in = circ.get_nth_out_edge(inputs[1], 0);
    Edge out = circ.get_nth_in_edge(outputs[1], 0);
    // The Rz should not be commuted through the CZ, since if it were the source
    // of its conditional wire would not be "live" at the time of application.
    REQUIRE_FALSE(sqs.squash_between(in, out));
  }

  GIVEN("A circuit with classical control (2)") {
    // https://github.com/quantinuum/tket/issues/1324
    Circuit circ(3, 1);
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {2, 0});
    circ.add_measure(Qubit(2), Bit(0));
    circ.add_conditional_gate<unsigned>(OpType::Rz, {1.0}, {1}, {0}, 1);
    circ.add_op<unsigned>(OpType::CY, {0, 1});
    std::unique_ptr<AbstractSquasher> squasher =
        std::make_unique<Transforms::RzPhasedXSquasher>();
    SingleQubitSquash sqs(std::move(squasher), circ, true);
    VertexVec inputs = circ.q_inputs();
    VertexVec outputs = circ.q_outputs();
    Edge in = circ.get_nth_out_edge(inputs[1], 0);
    Edge out = circ.get_nth_in_edge(outputs[1], 0);
    // The Rz should not be commuted through the CZ, since if it were a cycle
    // (CZ->CX->Measure->Rz->CZ) would be introduced.
    REQUIRE_FALSE(sqs.squash_between(out, in));
  }

  GIVEN("Chains of identically-controlled Rz and PhasedX") {
    // https://github.com/quantinuum/tket/issues/1723
    Circuit circ(1, 2);
    for (int i = 0; i < 10; i++) {
      circ.add_conditional_gate<unsigned>(OpType::Rz, {0.67 * i}, {0}, {0}, 1);
      circ.add_conditional_gate<unsigned>(
          OpType::PhasedX, {0.76 * i, 0.77 * i}, {0}, {0}, 1);
    }
    circ.add_measure(Qubit(0), Bit(1));
    Transforms::squash_1qb_to_Rz_PhasedX().apply(circ);
    CHECK(circ.count_gates(OpType::Rz, true) == 1);
    CHECK(circ.count_gates(OpType::PhasedX, true) == 1);
  }
}

// https://github.com/quantinuum/tket/issues/535
SCENARIO("squash_1qb_to_Rz_PhasedX should preserve phase") {
  Circuit circ(1);
  circ.add_op<unsigned>(OpType::H, {0});
  circ.add_op<unsigned>(OpType::H, {0});
  const Eigen::MatrixXcd u = tket_sim::get_unitary(circ);
  Transforms::squash_1qb_to_Rz_PhasedX().apply(circ);
  const Eigen::MatrixXcd v = tket_sim::get_unitary(circ);
  REQUIRE(u.isApprox(v, ERR_EPS));
  REQUIRE(equiv_0(circ.get_phase()));
}

SCENARIO("Test decompose_ZXZ_to_TK1") {
  Circuit circ;
  unsigned tk1_count, total_count;
  GIVEN("A simple ZZ circuit") {
    circ = Circuit(1);
    circ.add_op<unsigned>(OpType::Rz, 0.234, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.434, {0});
    tk1_count = 1;
    total_count = 1;
  }
  GIVEN("A simple XX circuit") {
    circ = Circuit(1);
    circ.add_op<unsigned>(OpType::Rx, 0.234, {0});
    circ.add_op<unsigned>(OpType::Rx, 0.434, {0});
    tk1_count = 2;
    total_count = 2;
  }
  GIVEN("A simple ZXZ circuit") {
    circ = Circuit(1);
    circ.add_op<unsigned>(OpType::Rz, 0.234, {0});
    circ.add_op<unsigned>(OpType::Rx, 1.334, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.434, {0});
    tk1_count = 1;
    total_count = 1;
  }
  GIVEN("A simple ZXZ circuit with global phases") {
    circ = Circuit(1);
    circ.add_op<unsigned>(OpType::Rz, 2.234, {0});
    circ.add_op<unsigned>(OpType::Rx, 3.334, {0});
    circ.add_op<unsigned>(OpType::Rz, 2.434, {0});
    tk1_count = 1;
    total_count = 1;
  }
  GIVEN("A circuit with irreducible gates") {
    circ = Circuit(2);
    circ.add_op<unsigned>(OpType::Rz, 2.234, {0});
    circ.add_op<unsigned>(OpType::Rx, 3.334, {0});
    circ.add_op<unsigned>(OpType::V, {0});
    circ.add_op<unsigned>(OpType::Rz, 2.434, {0});
    circ.add_op<unsigned>(OpType::Rz, 12.23, {1});
    circ.add_op<unsigned>(OpType::Sdg, {1});
    circ.add_op<unsigned>(OpType::Rx, 22.22, {1});
    tk1_count = 4;
    total_count = 6;
  }
  GIVEN("A circuit with irreducible gates and blocking multiqb gates") {
    circ = Circuit(2);
    circ.add_op<unsigned>(OpType::Rz, 2.234, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Rx, 3.334, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.123, {0});
    circ.add_op<unsigned>(OpType::V, {0});
    circ.add_op<unsigned>(OpType::Rz, 2.434, {0});
    circ.add_op<unsigned>(OpType::Rz, 12.23, {1});
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::T, {1});
    circ.add_op<unsigned>(OpType::Sdg, {1});
    circ.add_op<unsigned>(OpType::Rz, 2.434, {0});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::Rx, 22.22, {1});
    circ.add_op<unsigned>(OpType::Rx, 3.334, {0});
    tk1_count = 7;
    total_count = 13;
  }

  auto u0 = tket_sim::get_unitary(circ);
  REQUIRE(Transforms::decompose_ZXZ_to_TK1().apply(circ));
  auto u1 = tket_sim::get_unitary(circ);

  REQUIRE(circ.count_gates(OpType::TK1) == tk1_count);
  REQUIRE(circ.n_gates() == total_count);

  REQUIRE(u1.isApprox(u1));
}

SCENARIO("Test decompose_ZYZ_to_TK1") {
  Circuit circ;
  unsigned tk1_count, total_count;
  GIVEN("A simple YY circuit") {
    circ = Circuit(1);
    circ.add_op<unsigned>(OpType::Ry, 0.234, {0});
    circ.add_op<unsigned>(OpType::Ry, 0.434, {0});
    tk1_count = 2;
    total_count = 2;
  }
  GIVEN("A simple ZYZ circuit") {
    circ = Circuit(1);
    circ.add_op<unsigned>(OpType::Rz, 0.234, {0});
    circ.add_op<unsigned>(OpType::Ry, 1.334, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.434, {0});
    tk1_count = 1;
    total_count = 1;
  }
  GIVEN("A simple ZYZ circuit with global phases") {
    circ = Circuit(1);
    circ.add_op<unsigned>(OpType::Rz, 2.234, {0});
    circ.add_op<unsigned>(OpType::Ry, 3.334, {0});
    circ.add_op<unsigned>(OpType::Rz, 2.434, {0});
    tk1_count = 1;
    total_count = 1;
  }
  GIVEN("A circuit with irreducible gates") {
    circ = Circuit(2);
    circ.add_op<unsigned>(OpType::Rz, 2.234, {0});
    circ.add_op<unsigned>(OpType::Ry, 3.334, {0});
    circ.add_op<unsigned>(OpType::V, {0});
    circ.add_op<unsigned>(OpType::Rz, 2.434, {0});
    circ.add_op<unsigned>(OpType::Rz, 12.23, {1});
    circ.add_op<unsigned>(OpType::Sdg, {1});
    circ.add_op<unsigned>(OpType::Ry, 22.22, {1});
    tk1_count = 4;
    total_count = 6;
  }
  GIVEN("A circuit with irreducible gates and blocking multiqb gates") {
    circ = Circuit(2);
    circ.add_op<unsigned>(OpType::Rz, 2.234, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Ry, 3.334, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.123, {0});
    circ.add_op<unsigned>(OpType::V, {0});
    circ.add_op<unsigned>(OpType::Rz, 2.434, {0});
    circ.add_op<unsigned>(OpType::Rz, 12.23, {1});
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::T, {1});
    circ.add_op<unsigned>(OpType::Sdg, {1});
    circ.add_op<unsigned>(OpType::Rz, 2.434, {0});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::Ry, 22.22, {1});
    circ.add_op<unsigned>(OpType::Ry, 3.334, {0});
    tk1_count = 7;
    total_count = 13;
  }

  auto u0 = tket_sim::get_unitary(circ);
  Transforms::decompose_ZYZ_to_TK1().apply(circ);
  auto u1 = tket_sim::get_unitary(circ);

  REQUIRE(circ.count_gates(OpType::TK1) == tk1_count);
  REQUIRE(circ.n_gates() == total_count);

  REQUIRE(u1.isApprox(u1));
}

}  // namespace test_Synthesis
}  // namespace tket
