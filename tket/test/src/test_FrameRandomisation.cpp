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

#include "testutil.hpp"
#include "tket/Characterisation/FrameRandomisation.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Predicates/PassLibrary.hpp"
#include "tket/Transformations/Rebase.hpp"
#include "tket/Transformations/Transform.hpp"

namespace tket {

std::vector<Cycle> FrameRandomisationTester::get_cycles(const Circuit& circ) {
  return fr->get_cycles(circ);
}

OpTypeVector FrameRandomisationTester::get_out_frame(
    const OpTypeVector& in_frame, const Cycle& cycle_ops) {
  return fr->get_out_frame(in_frame, cycle_ops).first;
}

namespace test_FrameRandomisation {

SCENARIO(
    "Test whether get_cycles returns the expected number of cycles with "
    "the correct operations.") {
  FrameRandomisation fr({OpType::CX, OpType::H}, {}, {{}});
  FrameRandomisationTester fr_tester(&fr);

  const auto add_fixed_sequence_of_ops = [](Circuit& circ) {
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::Y, {1});
    circ.add_op<unsigned>(OpType::Z, {2});
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
  };
  const auto get_comparison = [](Edge& e) {
    return Cycle(
        {{e, e}, {e, e}, {e, e}}, {
                                      {OpType::Input, {}, {}},
                                      {OpType::H, {0}, {}},
                                      {OpType::Input, {}, {}},
                                      {OpType::H, {1}, {}},
                                      {OpType::CX, {0, 1}, {}},
                                      {OpType::Input, {}, {}},
                                      {OpType::H, {2}, {}},
                                      {OpType::CX, {0, 2}, {}},
                                  });
  };
  GIVEN("A circuit with one expected cycle.") {
    Circuit circ(3);
    add_fixed_sequence_of_ops(circ);

    Edge e;
    const auto comparison = get_comparison(e);
    std::vector<Cycle> cycles = fr_tester.get_cycles(circ);
    REQUIRE(cycles.size() == 1);
    REQUIRE(cycles[0] == comparison);
  }
  GIVEN("A circuit with two expected cycle.") {
    Circuit circ(3);
    add_fixed_sequence_of_ops(circ);
    add_fixed_sequence_of_ops(circ);

    Edge e;
    const auto comparison_0 = get_comparison(e);
    const Cycle comparison_1(
        {{e, e}, {e, e}, {e, e}}, {
                                      {OpType::H, {0}, {}},
                                      {OpType::H, {1}, {}},
                                      {OpType::CX, {1, 0}, {}},
                                      {OpType::H, {2}, {}},
                                      {OpType::CX, {1, 2}, {}},
                                  });

    std::vector<Cycle> cycles = fr_tester.get_cycles(circ);
    REQUIRE(cycles.size() == 2);
    REQUIRE(cycles[0] == comparison_0);
    REQUIRE(cycles[1] == comparison_1);
  }
  GIVEN("A circuit with 50 cycles.") {
    Circuit circ(3);
    for (unsigned i = 0; i < 50; i++) {
      add_fixed_sequence_of_ops(circ);
    }
    std::vector<Cycle> cycles = fr_tester.get_cycles(circ);
    REQUIRE(cycles.size() == 50);
  }
}

SCENARIO("Test that get_out_frame returns the expected result.") {
  Circuit circ(2);
  circ.add_op<unsigned>(OpType::CX, {0, 1});
  std::map<OpType, std::map<OpTypeVector, OpTypeVector>> init;
  std::map<OpTypeVector, OpTypeVector> entry;
  entry[{OpType::X, OpType::X}] = {OpType::Y, OpType::Y};
  entry[{OpType::Y, OpType::Y}] = {OpType::X, OpType::X};
  init[OpType::CX] = entry;
  FrameRandomisation fr({OpType::CX, OpType::H}, {OpType::X}, init);
  FrameRandomisationTester fr_tester(&fr);

  Cycle cycle = fr_tester.get_cycles(circ)[0];
  OpTypeVector out_frame =
      fr_tester.get_out_frame({OpType::X, OpType::X}, cycle);
  REQUIRE(out_frame[0] == OpType::Y);
  REQUIRE(out_frame[1] == OpType::Y);
  out_frame = fr_tester.get_out_frame({OpType::Y, OpType::Y}, cycle);
  REQUIRE(out_frame[0] == OpType::X);
  REQUIRE(out_frame[1] == OpType::X);
}

// KEY: op type
// VALUE: list of command indices with that type.
typedef std::map<OpType, std::vector<unsigned>> CircResult;

// KEY: index in "all_circuits", a list of circuits
// VALUE: result for that circuit.
typedef std::map<unsigned, CircResult> AllCircuitsResult;

// KEY: index in "all_circuits"
// VALUE: the expected parameter value in command[n]
//          for that circuit (n is constant over all circuits
//          for which this applies).
typedef std::map<unsigned, double> ParameterValues;

static void test_parameter_value(
    unsigned circuit_index, const ParameterValues& values,
    const std::vector<Command>& commands,
    unsigned index_for_command_with_parameter) {
  if (values.empty()) {
    return;
  }
  const double expected_parameter_value = values.at(circuit_index);
  const auto params =
      commands[index_for_command_with_parameter].get_op_ptr()->get_params();
  REQUIRE(params.size() == 1);
  // Normally it's very naughty to do doubles equality.
  // But here, no calculations other than possibly inserting
  // a minus sign are performed, so no roundoff errors occur.
  REQUIRE(params[0] == expected_parameter_value);
}

static void test_command_types(
    const AllCircuitsResult& result, const std::vector<Circuit>& all_circuits,
    unsigned number_of_commands, const ParameterValues& values = {},
    OpType op_with_parameter = OpType::noop,
    unsigned index_for_command_with_parameter = 9999) {
  for (const auto& outer_entry : result) {
    const auto& circuit_index = outer_entry.first;
    const auto commands = all_circuits[circuit_index].get_commands();
    REQUIRE(commands.size() == number_of_commands);
    test_parameter_value(
        circuit_index, values, commands, index_for_command_with_parameter);

    CircResult op_types_for_this_circ = outer_entry.second;
    if (!values.empty()) {
      op_types_for_this_circ[op_with_parameter].push_back(
          index_for_command_with_parameter);
    }
    for (const auto& inner_entry : op_types_for_this_circ) {
      const OpType& type = inner_entry.first;
      auto command_indices = inner_entry.second;
      for (unsigned ii : command_indices) {
        REQUIRE(commands[ii].get_op_ptr()->get_type() == type);
      }
    }
  }
}

SCENARIO("Test that get_all_circuits returns all frames for all cycles.") {
  std::map<OpType, std::map<OpTypeVector, OpTypeVector>> init;
  std::map<OpTypeVector, OpTypeVector> entry_cx;
  std::map<OpTypeVector, OpTypeVector> entry_h;
  entry_cx[{OpType::X, OpType::X}] = {OpType::Y, OpType::Y};
  entry_cx[{OpType::Y, OpType::Y}] = {OpType::X, OpType::X};
  entry_cx[{OpType::X, OpType::Y}] = {OpType::Y, OpType::X};
  entry_cx[{OpType::Y, OpType::X}] = {OpType::X, OpType::Y};
  entry_h[{OpType::X}] = {OpType::Y};
  entry_h[{OpType::Y}] = {OpType::X};
  init[OpType::CX] = entry_cx;
  init[OpType::H] = entry_h;
  FrameRandomisation fr({OpType::CX, OpType::H}, {OpType::X, OpType::Y}, init);

  GIVEN("A two-qubit circuit with one CX gate.") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    const std::vector<Circuit> two_circuits =
        fr.sample_randomisation_circuits(circ, 2);
    REQUIRE(two_circuits.size() == 2);
    const std::vector<Circuit> all_circuits = fr.get_all_circuits(circ);
    REQUIRE(all_circuits.size() == 4);
    REQUIRE(
        std::find(all_circuits.begin(), all_circuits.end(), two_circuits[0]) !=
        all_circuits.end());
    for (const Circuit& circ : all_circuits) {
      const auto coms = circ.get_commands();
      REQUIRE(coms.size() == 7);
      REQUIRE(
          coms[0].get_op_ptr()->get_type() != coms[5].get_op_ptr()->get_type());
      REQUIRE(
          coms[1].get_op_ptr()->get_type() != coms[6].get_op_ptr()->get_type());
    }
  }
  GIVEN("A two-qubit circuit with three cycles.") {
    Circuit circ(2);
    const auto add_four_ops = [&circ]() {
      circ.add_op<unsigned>(OpType::CX, {0, 1});
      circ.add_op<unsigned>(OpType::H, {1});
      circ.add_op<unsigned>(OpType::S, {0});
      circ.add_op<unsigned>(OpType::S, {1});
    };
    circ.add_op<unsigned>(OpType::S, {0});
    add_four_ops();
    circ.add_op<unsigned>(OpType::H, {0});
    add_four_ops();
    add_four_ops();
    const std::vector<Circuit> all_circuits = fr.get_all_circuits(circ);
    REQUIRE(all_circuits.size() == 64);
    const std::vector<unsigned> indices_a{1, 2, 8, 11, 12, 18, 19, 22, 23, 29};
    const std::vector<unsigned> indices_b{7, 28};

    const AllCircuitsResult expected_result{
        {0, {{OpType::X, indices_a}, {OpType::Y, indices_b}}},
        {63, {{OpType::X, indices_b}, {OpType::Y, indices_a}}},
    };
    test_command_types(expected_result, all_circuits, 32);
  }
}

SCENARIO(
    "Test that get_all_circuits returns correct frames for all cycles "
    "using PauliFrameRandomisation.") {
  PauliFrameRandomisation pfr;
  GIVEN("A two-qubit circuit with one CX gate.") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    const std::vector<Circuit> all_circuits = pfr.get_all_circuits(circ);
    REQUIRE(all_circuits.size() == 16);

    const AllCircuitsResult expected_result{
        {0, {{OpType::Z, {0, 1, 6}}, {OpType::noop, {5}}}},
        {3,
         {
             {OpType::Z, {0, 5}},
             {OpType::noop, {1, 6}},
         }},
        {7, {{OpType::X, {0, 5, 6}}, {OpType::noop, {1}}}},
        {11,
         {
             {OpType::Y, {0, 5}},
             {OpType::noop, {1}},
             {OpType::X, {6}},
         }},
        {15,
         {
             {OpType::noop, {0, 1, 5, 6}},
         }},
    };
    test_command_types(expected_result, all_circuits, 7);
  }
}

SCENARIO(
    "Test that get_all_circuits returns correct frames and circuits for "
    "all cycles using UniversalFrameRandomisation.") {
  UniversalFrameRandomisation ufr;
  GIVEN("A two-qubit circuit with one CX and Rz(0.2) gate.") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rz, 0.2, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});

    const std::vector<Circuit> all_circuits = ufr.get_all_circuits(circ);
    REQUIRE(all_circuits.size() == 16);

    const AllCircuitsResult expected_result{
        {0,
         {
             {OpType::Z, {0, 1, 7}},
             {OpType::noop, {6}},
         }},
        {3,
         {
             {OpType::Z, {0, 6}},
             {OpType::noop, {1, 7}},
         }},
        {7,
         {
             {OpType::X, {0, 6, 7}},
             {OpType::noop, {1}},
         }},
        {11,
         {
             {OpType::X, {7}},
             {OpType::Y, {0, 6}},
             {OpType::noop, {1}},
         }},
        {15,
         {
             {OpType::noop, {0, 1, 6, 7}},
         }},
    };
    const ParameterValues param_values{
        {0, 0.2}, {3, 0.2}, {7, -0.2}, {11, -0.2}, {15, 0.2},
    };
    test_command_types(
        expected_result, all_circuits, 8, param_values, OpType::Rz, 3);
  }
  GIVEN("A circuit that gets rebased ready for UFR") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Ry, 0.2, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    std::vector<Circuit> all_circuits = ufr.get_all_circuits(circ);
    REQUIRE(all_circuits.size() == 256);
    Transforms::rebase_UFR().apply(circ);
    all_circuits = ufr.get_all_circuits(circ);
    REQUIRE(all_circuits.size() == 16);
  }
}

SCENARIO(
    "Test that sample_cycles returns correct number of cycles using "
    "PowerCycle.") {
  const auto test_sample_cycles_from_power_cycle =
      [](unsigned multiplier, const Circuit& circ, unsigned number_of_tests) {
        PowerCycle pc;
        for (unsigned nn = 1; nn <= number_of_tests; ++nn) {
          const auto sample_cycles = pc.sample_cycles(circ, nn, 1);
          REQUIRE(sample_cycles.size() == 1);
          const Circuit& cycle_circ = sample_cycles[0];
          REQUIRE(cycle_circ.n_gates() == nn * multiplier);
        }
      };
  GIVEN("A one-qubit circuit with one H gate.") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::H, {0});
    test_sample_cycles_from_power_cycle(5, circ, 4);
  }
  GIVEN("A 5-qubit circuit.") {
    Circuit circ(5);
    add_2qb_gates(circ, OpType::CX, {{0, 1}, {2, 1}, {3, 2}, {3, 4}, {0, 1}});
    add_1qb_gates(circ, OpType::S, {3, 2, 4});
    test_sample_cycles_from_power_cycle(20, circ, 4);
  }
}

SCENARIO("Frame Randomisation Segmentation Fault") {
  // https://github.com/quantinuum/tket/issues/1015
  PauliFrameRandomisation pfr;
  Circuit c(4);
  c.add_op<unsigned>(OpType::CX, {0, 2});
  c.add_op<unsigned>(OpType::Z, {0});
  c.add_op<unsigned>(OpType::CX, {0, 3});
  c.add_op<unsigned>(OpType::Z, {0});
  c.add_op<unsigned>(OpType::CX, {0, 3});
  c.add_op<unsigned>(OpType::CX, {0, 1});
  std::vector<Circuit> circs = pfr.sample_randomisation_circuits(c, 1);
  CHECK(circs.size() == 1);
}

SCENARIO("Frame Randomisation non-real DAG") {
  // https://github.com/quantinuum/tket/issues/1015
  GIVEN("CnX gate with 3 controls") {
    PauliFrameRandomisation pfr;
    Circuit c(4);
    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3});
    CompilationUnit cu(c);
    RebaseTket()->apply(cu);
    std::vector<Circuit> circs =
        pfr.sample_randomisation_circuits(cu.get_circ_ref(), 1);
    CHECK(circs.size() == 1);
    CHECK(circs[0].get_commands().size() == 107);
  }
  GIVEN("CnX gate with 4 controls") {
    PauliFrameRandomisation pfr;
    Circuit c(5);
    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4});
    CompilationUnit cu(c);
    RebaseTket()->apply(cu);
    std::vector<Circuit> circs =
        pfr.sample_randomisation_circuits(cu.get_circ_ref(), 1);
    CHECK(circs.size() == 1);
    CHECK(circs[0].get_commands().size() == 293);
  }
  GIVEN("CnX gate with 5 controls") {
    PauliFrameRandomisation pfr;
    Circuit c(6);
    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4, 5});
    CompilationUnit cu(c);
    RebaseTket()->apply(cu);
    std::vector<Circuit> circs =
        pfr.sample_randomisation_circuits(cu.get_circ_ref(), 1);
    CHECK(circs.size() == 1);
    CHECK(circs[0].get_commands().size() == 742);
  }
  GIVEN("CnX gate with 6 controls") {
    PauliFrameRandomisation pfr;
    Circuit c(7);
    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4, 5, 6});
    CompilationUnit cu(c);
    RebaseTket()->apply(cu);
    std::vector<Circuit> circs =
        pfr.sample_randomisation_circuits(cu.get_circ_ref(), 1);
    CHECK(circs.size() == 1);
    CHECK(circs[0].get_commands().size() == 1118);
  }
  GIVEN("CnX gate with 7 controls") {
    PauliFrameRandomisation pfr;
    Circuit c(8);
    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4, 5, 6, 7});
    CompilationUnit cu(c);
    RebaseTket()->apply(cu);
    std::vector<Circuit> circs =
        pfr.sample_randomisation_circuits(cu.get_circ_ref(), 1);
    CHECK(circs.size() == 1);
    CHECK(circs[0].get_commands().size() == 1570);
  }
}

}  // namespace test_FrameRandomisation
}  // namespace tket
