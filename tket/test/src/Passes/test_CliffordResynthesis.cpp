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
#include <vector>

#include "../testutil.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Predicates/PassGenerators.hpp"

namespace tket {
namespace test_CliffordResynthesis {

void check_clifford_resynthesis(const Circuit &c) {
  Circuit c0 = c;
  CompilationUnit cu(c0);
  gen_clifford_resynthesis_pass()->apply(cu);
  Circuit c1 = cu.get_circ_ref();

  REQUIRE(test_unitary_comparison(c0, c1, true));
}

SCENARIO("SynthesiseCliffordResynthesis correctness") {
  GIVEN("A small Clifford circuit") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    check_clifford_resynthesis(c);
  }
  GIVEN("A Clifford and a non-Clifford operation") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::T, {0});
    check_clifford_resynthesis(c);
  }
  GIVEN("A circuit with two Clifford subcircuits") {
    Circuit c(4);
    c.add_op<unsigned>(OpType::T, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});  // (0)
    c.add_op<unsigned>(OpType::CY, {1, 2});  // (0)
    c.add_op<unsigned>(OpType::CZ, {0, 1});  // (0)
    c.add_op<unsigned>(OpType::TK2, {0.1, 0.2, 0.3}, {2, 3});
    c.add_op<unsigned>(OpType::T, {1});
    c.add_op<unsigned>(OpType::CX, {1, 2});  // (1)
    c.add_op<unsigned>(OpType::H, {2});      // (1)
    c.add_op<unsigned>(OpType::CY, {2, 3});  // (1)
    c.add_op<unsigned>(OpType::TK2, {0.1, 0.2, 0.3}, {1, 2});
    check_clifford_resynthesis(c);
  }
  GIVEN("A more complex example") {
    Circuit c(5);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::T, {2});
    c.add_op<unsigned>(OpType::Y, {3});
    c.add_op<unsigned>(OpType::Z, {4});
    c.add_op<unsigned>(OpType::CX, {2, 3});
    c.add_op<unsigned>(OpType::T, {2});
    c.add_op<unsigned>(OpType::S, {3});
    c.add_op<unsigned>(OpType::V, {1});
    c.add_op<unsigned>(OpType::V, {2});
    c.add_op<unsigned>(OpType::Vdg, {3});
    c.add_op<unsigned>(OpType::CY, {1, 3});
    c.add_op<unsigned>(OpType::CZ, {3, 4});
    c.add_op<unsigned>(OpType::SWAP, {2, 3});
    c.add_op<unsigned>(OpType::Sdg, {4});
    c.add_op<unsigned>(OpType::CX, {3, 0});
    c.add_op<unsigned>(OpType::V, {0});
    check_clifford_resynthesis(c);
  }
  GIVEN("A circuit with no Clifford gates") {
    Circuit c(1);
    c.add_op<unsigned>(OpType::T, {0});
    check_clifford_resynthesis(c);
  }
  GIVEN("An Rz(0) gate and a ZZMax gate") {
    // Test workaround for https://github.com/quantinuum/tket/issues/1268
    Circuit c(2);
    c.add_op<unsigned>(OpType::Rz, 0., {0});
    c.add_op<unsigned>(OpType::ZZMax, {0, 1});
    check_clifford_resynthesis(c);
  }
  GIVEN("A troublesome circuit (1)") {
    // https://github.com/quantinuum/tket/issues/1279
    Circuit c(3);
    c.add_op<unsigned>(OpType::ECR, {1, 2});
    c.add_op<unsigned>(OpType::CnRy, 0., {0, 1});
    c.add_op<unsigned>(OpType::Rz, 0., {1});
    c.add_op<unsigned>(OpType::ZZMax, {2, 1});
    check_clifford_resynthesis(c);
  }
  GIVEN("A troublesome circuit (2)") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::Z, {0});
    c.add_op<unsigned>(OpType::T, {0});
    c.add_op<unsigned>(OpType::CY, {1, 0});
    check_clifford_resynthesis(c);
  }
  GIVEN("A Clifford-angle NPhasedX on 1 qubit") {
    // https://github.com/quantinuum/tket/issues/1408
    Circuit c(1);
    c.add_op<unsigned>(OpType::NPhasedX, {0.5, 0.0}, {0});
    check_clifford_resynthesis(c);
  }
  GIVEN("A Clifford-angle NPhasedX on 2 qubits") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::NPhasedX, {0.5, 0.0}, {0, 1});
    check_clifford_resynthesis(c);
  }

  GIVEN("A troublesome circuit (3)") {
    // https://github.com/quantinuum/tket/issues/1468
    Circuit c0(6);
    c0.add_op<unsigned>(OpType::XXPhase, 0.5, {1, 4});
    c0.add_op<unsigned>(OpType::XXPhase, 1.5, {2, 3});
    c0.add_op<unsigned>(OpType::XXPhase, 2.5, {1, 3});
    c0.add_op<unsigned>(OpType::YYPhase, 0.5, {4, 5});
    c0.add_op<unsigned>(OpType::YYPhase, 1.5, {4, 2});
    c0.add_op<unsigned>(OpType::YYPhase, 2.5, {3, 1});
    c0.add_op<unsigned>(OpType::ZZPhase, 0.5, {0, 3});
    c0.add_op<unsigned>(OpType::ZZPhase, 1.5, {4, 1});
    c0.add_op<unsigned>(OpType::ZZPhase, 2.5, {0, 5});
    CompilationUnit cu(c0);
    gen_clifford_resynthesis_pass()->apply(cu);
    Circuit c1 = cu.get_circ_ref();
    std::vector<Command> cmds = c1.get_commands();
    CHECK(std::all_of(cmds.begin(), cmds.end(), [](const Command &cmd) {
      return cmd.get_op_ptr()->is_clifford();
    }));
    CHECK(c1.count_n_qubit_gates(2) <= c0.count_n_qubit_gates(2));
  }
}

}  // namespace test_CliffordResynthesis
}  // namespace tket
