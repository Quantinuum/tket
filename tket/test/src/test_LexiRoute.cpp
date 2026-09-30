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
#include <fstream>

#include "testutil.hpp"
#include "tket/Mapping/LexiLabelling.hpp"
#include "tket/Mapping/LexiRoute.hpp"
#include "tket/Mapping/LexiRouteRoutingMethod.hpp"
#include "tket/Mapping/MappingManager.hpp"
#include "tket/Mapping/Verification.hpp"
#include "tket/Ops/ClExpr.hpp"
#include "tket/Ops/ClassicalOps.hpp"
#include "tket/Placement/Placement.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Predicates/CompilerPass.hpp"
#include "tket/Predicates/PassGenerators.hpp"
#include "tket/Transformations/Decomposition.hpp"

namespace tket {

// Checks if the initial/final maps are correct by walking through the circuit
bool check_permutation(
    const Circuit& circ, const std::shared_ptr<unit_bimaps_t>& bimaps) {
  // qubits |-> nodes
  // qubits get moved with swap gates
  unit_bimap_t qubit_map;
  for (auto q : circ.all_qubits()) {
    qubit_map.left.insert({bimaps->initial.right.find(q)->second, q});
  }
  for (const Command& cmd : circ.get_commands()) {
    Op_ptr op = cmd.get_op_ptr();
    if (op->get_type() == OpType::SWAP) {
      unit_vector_t units = cmd.get_args();
      // swap qubits in qubit_map
      auto it0 = qubit_map.right.find(units[0]);
      auto it1 = qubit_map.right.find(units[1]);
      UnitID q0 = it0->second;
      UnitID q1 = it1->second;
      qubit_map.right.erase(it1);
      qubit_map.right.erase(it0);
      qubit_map.left.insert({q1, units[0]});
      qubit_map.left.insert({q0, units[1]});
    }
  }
  // Check this agrees with the final map
  for (auto it = qubit_map.left.begin(); it != qubit_map.left.end(); ++it) {
    auto final_it = bimaps->final.left.find(it->first);
    if (final_it == bimaps->final.left.end() ||
        final_it->second != it->second) {
      return false;
    }
  }
  return true;
}

// Checks if the results matches the initial circ after resolving the
// permutations
bool check_permutation_unitary(
    Circuit& initial_circ, Circuit& circ,
    const std::shared_ptr<unit_bimaps_t>& maps) {
  for (auto me : maps->initial) {
    if (me.left != me.right) return false;
  }

  // bool found_permutations = true;
  while (true) {
    bool found_permutations = false;
    for (auto me : maps->final) {
      if (me.left != me.right) found_permutations = true;
    }
    if (found_permutations) {
      for (auto me : maps->final) {
        if (me.left != me.right) {
          circ.add_op<UnitID>(OpType::SWAP, {Qubit(me.left), Qubit(me.right)});
          break;
        }
      }
    } else {
      return test_unitary_comparison(initial_circ, circ);
    }
  }

  return true;
}

void add_swap_tests(
    Circuit& circ, std::vector<Node>& node_vec, unsigned u0, unsigned u1) {
  std::vector<Qubit> qubits_renamed = circ.all_qubits();
  circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[u0], qubits_renamed[u1]});

  Node no = node_vec[u0];
  node_vec[u0] = node_vec[u1];
  node_vec[u1] = no;
}

SCENARIO("Test LexiRoute::solve and LexiRoute::solve_labelling") {
  std::vector<Node> nodes = {Node("test_node", 0), Node("test_node", 1),
                             Node("test_node", 2), Node("node_test", 3),
                             Node("node_test", 4), Node("node_test", 5),
                             Node("test_node", 6), Node("node_test", 7)};
  // n0 -- n1 -- n2 -- n3 -- n4
  //             |     |
  //             n5    n7
  //             |
  //             n6
  Architecture architecture(
      {{nodes[0], nodes[1]},
       {nodes[1], nodes[2]},
       {nodes[2], nodes[3]},
       {nodes[3], nodes[4]},
       {nodes[2], nodes[5]},
       {nodes[5], nodes[6]},
       {nodes[3], nodes[7]}});
  ArchitecturePtr shared_arc = std::make_shared<Architecture>(architecture);
  GIVEN("Single best solution, all qubits labelled.") {
    Circuit circ(6);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});
    circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[3]});
    circ.add_op<UnitID>(OpType::CX, {qubits[4], qubits[5]});
    // n0 -- n1 -- n2 -- n3 -- n4
    //             |     |
    //             n5    n7
    //             |
    //             n6
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]}, {qubits[2], nodes[2]},
        {qubits[3], nodes[3]}, {qubits[4], nodes[6]}, {qubits[5], nodes[5]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiRoute lr(shared_arc, mf);

    lr.solve(4);
    std::vector<Command> commands = mf->circuit_.get_commands();
    REQUIRE(commands.size() == 4);
    Command swap_c = commands[1];
    unit_vector_t uids = {nodes[1], nodes[2]};
    REQUIRE(swap_c.get_args() == uids);
    REQUIRE(*swap_c.get_op_ptr() == *get_op_ptr(OpType::SWAP));
  }
  GIVEN("Single best solution, one qubit unlabelled.") {
    Circuit circ(6);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});
    circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[3]});
    circ.add_op<UnitID>(OpType::CX, {qubits[4], qubits[5]});
    // n0 -- n1 -- n2 -- n3 -- n4
    //             |     |
    //             n5    n7
    //             |
    //             n6
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]},
        {qubits[1], nodes[1]},
        {qubits[2], nodes[2]},
        {qubits[3], nodes[3]},
        {qubits[5], nodes[5]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf0 = std::make_shared<MappingFrontier>(circ);
    LexiRoute lr(shared_arc, mf0);
    lr.solve_labelling();

    REQUIRE(mf0->circuit_.n_gates() == 3);

    rename_map = {{qubits[4], nodes[6]}};
    mf0->circuit_.rename_units(rename_map);

    MappingFrontier_ptr mf1 = std::make_shared<MappingFrontier>(circ);
    LexiRoute lr1(shared_arc, mf1);
    lr1.solve(4);

    std::vector<Command> commands = mf1->circuit_.get_commands();
    Command swap_c = commands[1];
    unit_vector_t uids = {nodes[1], nodes[2]};
    REQUIRE(swap_c.get_args() == uids);
    REQUIRE(*swap_c.get_op_ptr() == *get_op_ptr(OpType::SWAP));
  }
  GIVEN("Single best solution, one stage of look-ahead required.") {
    Circuit circ(8);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[4]});
    circ.add_op<UnitID>(OpType::CX, {qubits[6], qubits[7]});
    circ.add_op<UnitID>(OpType::CX, {qubits[2], qubits[7]});
    //                   n7
    //                   |
    // n0 -- n1 -- n2 -- n3 -- n4
    //             |
    //             n5
    //             |
    //             n6
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]}, {qubits[2], nodes[2]},
        {qubits[3], nodes[3]}, {qubits[4], nodes[4]}, {qubits[5], nodes[5]},
        {qubits[6], nodes[6]}, {qubits[7], nodes[7]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiRoute lr(shared_arc, mf);

    lr.solve(4);
    std::vector<Command> commands = mf->circuit_.get_commands();
    REQUIRE(commands.size() == 4);
    Command swap_c = commands[0];
    unit_vector_t uids = {nodes[7], nodes[3]};
    REQUIRE(swap_c.get_args() == uids);
    REQUIRE(*swap_c.get_op_ptr() == *get_op_ptr(OpType::SWAP));

    Command changed_c = commands[3];
    uids = {nodes[2], nodes[3]};
    REQUIRE(changed_c.get_args() == uids);
  }
  GIVEN("All unlabelled, labelling can give complete solution.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[3]});
    circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[4]});

    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiRoute lr0(shared_arc, mf);
    lr0.solve_labelling();
    std::vector<Command> commands = mf->circuit_.get_commands();
    REQUIRE(commands.size() == 4);
    Command c = commands[0];
    unit_vector_t uids = {nodes[2], nodes[1]};
    REQUIRE(c.get_args() == uids);
    mf->advance_frontier_boundary(shared_arc);

    LexiRoute lr1(shared_arc, mf);
    lr1.solve_labelling();
    uids = {nodes[2], nodes[3]};
    REQUIRE(mf->circuit_.get_commands()[1].get_args() == uids);
    mf->advance_frontier_boundary(shared_arc);

    LexiRoute lr2(shared_arc, mf);
    lr2.solve_labelling();
    uids = {nodes[2], nodes[5]};
    REQUIRE(mf->circuit_.get_commands()[2].get_args() == uids);
    mf->advance_frontier_boundary(shared_arc);

    LexiRoute lr3(shared_arc, mf);
    lr3.solve_labelling();
    uids = {nodes[5], nodes[6]};
    REQUIRE(mf->circuit_.get_commands()[3].get_args() == uids);
  }
  GIVEN("Bridge preferred, CX.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});
    circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[1]});
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[1]},
        {qubits[1], nodes[3]},
        {qubits[2], nodes[0]},
        {qubits[3], nodes[7]},
        {qubits[4], nodes[2]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);

    mf->advance_frontier_boundary(shared_arc);
    LexiRoute lr(shared_arc, mf);
    lr.solve(4);
    Command bridge_c = mf->circuit_.get_commands()[0];
    unit_vector_t uids = {nodes[1], nodes[2], nodes[3]};
    REQUIRE(bridge_c.get_args() == uids);
    REQUIRE(*bridge_c.get_op_ptr() == *get_op_ptr(OpType::BRIDGE));
  }
  GIVEN("Bridge preferred, CZ.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CZ, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});
    circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[1]});
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[1]},
        {qubits[1], nodes[3]},
        {qubits[2], nodes[0]},
        {qubits[3], nodes[7]},
        {qubits[4], nodes[2]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);

    mf->advance_frontier_boundary(shared_arc);
    LexiRoute lr(shared_arc, mf);
    lr.solve(4);
    REQUIRE(mf->circuit_.get_commands().size() == 4);
  }
  GIVEN("Bridge preferred, conditional CX.") {
    Circuit circ(5, 1);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 1}, {0}, 1);
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});
    circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[1]});
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[1]},
        {qubits[1], nodes[3]},
        {qubits[2], nodes[0]},
        {qubits[3], nodes[7]},
        {qubits[4], nodes[2]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);

    mf->advance_frontier_boundary(shared_arc);
    LexiRoute lr(shared_arc, mf);
    lr.solve(4);
    REQUIRE(mf->circuit_.get_commands().size() == 4);
  }
  GIVEN("Bridge preferred, conditional CZ.") {
    Circuit circ(5, 1);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_conditional_gate<unsigned>(OpType::CZ, {}, {0, 1}, {0}, 1);
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});
    circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[1]});
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[1]},
        {qubits[1], nodes[3]},
        {qubits[2], nodes[0]},
        {qubits[3], nodes[7]},
        {qubits[4], nodes[2]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);

    mf->advance_frontier_boundary(shared_arc);
    LexiRoute lr(shared_arc, mf);
    lr.solve(4);
    REQUIRE(mf->circuit_.get_commands().size() == 4);
  }

  GIVEN("Ancilla assignment, one valid node.") {
    Circuit circ(3);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CZ, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});

    std::vector<Node> nodes = {
        Node("test_node", 0), Node("test_node", 1), Node("test_node", 2),
        Node("node_test", 3), Node("node_test", 4)};
    // just a ring

    Architecture architecture(
        {{nodes[0], nodes[1]},
         {nodes[1], nodes[2]},
         {nodes[2], nodes[3]},
         {nodes[3], nodes[4]},
         {nodes[4], nodes[0]}});
    ArchitecturePtr shared_arc = std::make_shared<Architecture>(architecture);

    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[2]}, {qubits[1], nodes[4]}};
    circ.rename_units(rename_map);

    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    mf->advance_frontier_boundary(shared_arc);
    LexiRoute lr0(shared_arc, mf);
    lr0.solve(20);
    REQUIRE(circ.all_qubits()[1] == nodes[4]);

    mf->advance_frontier_boundary(shared_arc);
    LexiRoute lr1(shared_arc, mf);
    lr1.solve_labelling();
    REQUIRE(circ.all_qubits()[0] == nodes[3]);
  }

  GIVEN("Ancilla assignment, multiple valid Node.") {
    Circuit circ(3);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CZ, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});

    std::vector<Node> nodes = {Node("test_node", 0), Node("test_node", 1),
                               Node("test_node", 2), Node("node_test", 3),
                               Node("node_test", 4), Node("node_test", 5),
                               Node("node_test", 6)};
    // A ring, but with two identical length paths where ancilla could be
    // assigned
    Architecture architecture(
        {{nodes[0], nodes[1]},
         {nodes[1], nodes[2]},
         {nodes[2], nodes[3]},
         {nodes[2], nodes[5]},
         {nodes[3], nodes[6]},
         {nodes[5], nodes[6]},
         {nodes[3], nodes[4]},
         {nodes[5], nodes[4]},
         {nodes[4], nodes[0]}});
    ArchitecturePtr shared_arc = std::make_shared<Architecture>(architecture);

    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[2]}, {qubits[1], nodes[4]}};
    circ.rename_units(rename_map);

    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    mf->advance_frontier_boundary(shared_arc);
    LexiRoute lr0(shared_arc, mf);
    lr0.solve_labelling();

    mf->advance_frontier_boundary(shared_arc);
    LexiRoute lr1(shared_arc, mf);
    lr1.solve(20);

    REQUIRE(circ.all_qubits()[1] == nodes[5]);
  }
  GIVEN("Ancilla assignment, one valid Node, with merge.") {
    Circuit circ(4);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CZ, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[2]});
    circ.add_op<UnitID>(OpType::H, {qubits[3]});

    std::vector<Node> nodes = {
        Node("test_node", 0), Node("test_node", 1), Node("test_node", 2),
        Node("node_test", 3), Node("node_test", 4)};
    // just a ring

    Architecture architecture(
        {{nodes[0], nodes[1]},
         {nodes[1], nodes[2]},
         {nodes[2], nodes[3]},
         {nodes[3], nodes[4]},
         {nodes[4], nodes[0]}});
    ArchitecturePtr shared_arc = std::make_shared<Architecture>(architecture);

    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[2]}, {qubits[1], nodes[4]}, {qubits[3], nodes[3]}};
    circ.rename_units(rename_map);

    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    mf->ancilla_nodes_.insert(nodes[3]);
    mf->advance_frontier_boundary(shared_arc);

    LexiRoute lr0(shared_arc, mf);
    lr0.solve_labelling();

    REQUIRE(circ.all_qubits()[1] == nodes[4]);
    REQUIRE(circ.all_qubits()[0] == nodes[3]);
  }
  GIVEN(
      "Single best solution, with measurements and classically controlled "
      "gates.") {
    Circuit circ(6, 1);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 2}, {0}, 1);
    circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[3]});
    circ.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0}, 1);
    circ.add_op<UnitID>(OpType::Measure, {qubits[1], Bit(0)});
    circ.add_op<UnitID>(OpType::CX, {qubits[4], qubits[5]});
    circ.add_op<UnitID>(OpType::Measure, {qubits[3], Bit(0)});
    circ.add_conditional_barrier({0, 1, 2}, {}, {0}, 1, "");
    // n0 -- n1 -- n2 -- n3 -- n4
    //             |     |
    //             n5    n7
    //             |
    //             n6
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]}, {qubits[2], nodes[2]},
        {qubits[3], nodes[3]}, {qubits[4], nodes[6]}, {qubits[5], nodes[5]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiRoute lr(shared_arc, mf);

    lr.solve(4);
    std::vector<Command> commands = mf->circuit_.get_commands();
    REQUIRE(commands.size() == 8);
    Command swap_c = commands[1];
    unit_vector_t uids = {nodes[1], nodes[2]};
    REQUIRE(swap_c.get_args() == uids);
    REQUIRE(*swap_c.get_op_ptr() == *get_op_ptr(OpType::SWAP));
  }
  GIVEN(
      "Labelling is required, but there are no free remaining qubits, for"
      " one updated label, order 0.") {
    Circuit circ(9);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[8]});
    // n0 -- n1 -- n2 -- n3 -- n4
    //             |     |
    //             n5    n7
    //             |
    //             n6
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]}, {qubits[2], nodes[2]},
        {qubits[3], nodes[3]}, {qubits[4], nodes[4]}, {qubits[5], nodes[5]},
        {qubits[6], nodes[6]}, {qubits[7], nodes[7]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiRoute lr(shared_arc, mf);

    REQUIRE_THROWS_AS(lr.solve_labelling(), LexiRouteError);
  }
  GIVEN(
      "Labelling is required, but there are no free remaining qubits, for "
      "one updated label, order 1.") {
    Circuit circ(9);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[8]});
    // n0 -- n1 -- n2 -- n3 -- n4
    //             |     |
    //             n5    n7
    //             |
    //             n6
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[8], nodes[1]}, {qubits[2], nodes[2]},
        {qubits[3], nodes[3]}, {qubits[4], nodes[4]}, {qubits[5], nodes[5]},
        {qubits[6], nodes[6]}, {qubits[7], nodes[7]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiRoute lr(shared_arc, mf);
    REQUIRE_THROWS_AS(lr.solve_labelling(), LexiRouteError);
  }
  GIVEN(
      "Labelling is required, but there are no free remaining qubits, for"
      "two updated labels.") {
    Circuit circ(10);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[9], qubits[8]});
    // n0 -- n1 -- n2 -- n3 -- n4
    //             |     |
    //             n5    n7
    //             |
    //             n6
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]}, {qubits[2], nodes[2]},
        {qubits[3], nodes[3]}, {qubits[4], nodes[4]}, {qubits[5], nodes[5]},
        {qubits[6], nodes[6]}, {qubits[7], nodes[7]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiRoute lr(shared_arc, mf);
    REQUIRE_THROWS_AS(lr.solve_labelling(), LexiRouteError);
  }
}

SCENARIO("Test LexiLabellingMethod") {
  std::vector<Node> nodes = {
      Node("test_node", 0), Node("test_node", 1), Node("test_node", 2),
      Node("node_test", 3), Node("node_test", 4)};

  // straight line
  Architecture architecture(
      {{nodes[0], nodes[1]},
       {nodes[1], nodes[2]},
       {nodes[2], nodes[3]},
       {nodes[3], nodes[4]}});
  ArchitecturePtr shared_arc = std::make_shared<Architecture>(architecture);
  GIVEN("No qubit to label, empty frontier, routing_method false.") {
    Circuit circ(5);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiLabellingMethod lrm;
    REQUIRE(!lrm.routing_method(mf, shared_arc).first);
  }
  GIVEN("No qubit to label, partially filled frontier, routing_method false.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[4]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[2]});
    circ.add_op<UnitID>(OpType::ZZPhase, 0.3, {qubits[3], qubits[0]});
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]},
        {qubits[1], nodes[1]},
        {qubits[2], nodes[2]},
        {qubits[3], nodes[3]},
        {qubits[4], nodes[4]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiLabellingMethod lrm;
    REQUIRE(!lrm.routing_method(mf, shared_arc).first);
  }
  GIVEN("Qubit to label, but casually restricted, routing_method false.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[4]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[2]});
    circ.add_op<UnitID>(OpType::ZZPhase, 0.3, {qubits[3], qubits[0]});
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]},
        {qubits[1], nodes[1]},
        {qubits[2], nodes[2]},
        {qubits[4], nodes[4]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiLabellingMethod lrm;
    REQUIRE(!lrm.routing_method(mf, shared_arc).first);
  }
  GIVEN(
      "Two Qubit to label in future slice, causally restricted, "
      "routing_method false.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[2]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[3]});
    circ.add_op<UnitID>(OpType::ZZPhase, 0.3, {qubits[3], qubits[4]});
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]}, {qubits[2], nodes[2]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiLabellingMethod lrm;
    REQUIRE(!lrm.routing_method(mf, shared_arc).first);
  }
  GIVEN("Three Qubit Gate, all labelled, first slice, routing_method false.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[4]});
    circ.add_op<UnitID>(OpType::CCX, {qubits[1], qubits[2], qubits[3]});
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]},
        {qubits[1], nodes[1]},
        {qubits[2], nodes[2]},
        {qubits[3], nodes[3]},
        {qubits[4], nodes[4]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiLabellingMethod lrm;
    REQUIRE(!lrm.routing_method(mf, shared_arc).first);
  }
  GIVEN("One unlabelled qubit, one slice, check and route.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::CX, {qubits[2], qubits[3]});
    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]}, {qubits[2], nodes[2]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    VertPort pre_label =
        mf->linear_boundary->get<TagKey>().find(qubits[3])->second;
    LexiLabellingMethod lrm;
    std::pair<bool, unit_map_t> out = lrm.routing_method(mf, shared_arc);
    REQUIRE(out.first);
    REQUIRE(
        mf->linear_boundary->get<TagKey>().find(qubits[3]) ==
        mf->linear_boundary->get<TagKey>().end());
    VertPort post_label =
        mf->linear_boundary->get<TagKey>().find(nodes[3])->second;
    REQUIRE(pre_label == post_label);
  }
  GIVEN(
      "One unlabelled qubit, two slices, lookahead for better solution,check"
      " and route.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::ZZPhase, 0.8, {qubits[2], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[0]});

    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]}, {qubits[3], nodes[3]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    VertPort pre_label =
        mf->linear_boundary->get<TagKey>().find(qubits[2])->second;
    LexiLabellingMethod lrm;

    std::pair<bool, unit_map_t> out = lrm.routing_method(mf, shared_arc);
    REQUIRE(out.first);
    REQUIRE(
        mf->linear_boundary->get<TagKey>().find(qubits[2]) ==
        mf->linear_boundary->get<TagKey>().end());
    VertPort post_label =
        mf->linear_boundary->get<TagKey>().find(nodes[2])->second;
    REQUIRE(pre_label == post_label);
  }
  GIVEN("Two unlabelled qubits, one slice, check and route.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[1]});
    circ.add_op<UnitID>(OpType::ZZPhase, 0.8, {qubits[2], qubits[3]});

    std::map<UnitID, UnitID> rename_map = {
        {qubits[2], nodes[2]}, {qubits[1], nodes[1]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    VertPort pre_label_0 =
        mf->linear_boundary->get<TagKey>().find(qubits[0])->second;
    VertPort pre_label_3 =
        mf->linear_boundary->get<TagKey>().find(qubits[3])->second;
    LexiLabellingMethod lrm;
    std::pair<bool, unit_map_t> out = lrm.routing_method(mf, shared_arc);
    REQUIRE(out.first);
    REQUIRE(
        mf->linear_boundary->get<TagKey>().find(qubits[0]) ==
        mf->linear_boundary->get<TagKey>().end());
    REQUIRE(
        mf->linear_boundary->get<TagKey>().find(qubits[3]) ==
        mf->linear_boundary->get<TagKey>().end());
    VertPort post_label_0 =
        mf->linear_boundary->get<TagKey>().find(nodes[0])->second;
    REQUIRE(pre_label_0 == post_label_0);
    VertPort post_label_3 =
        mf->linear_boundary->get<TagKey>().find(nodes[3])->second;
    REQUIRE(pre_label_3 == post_label_3);
  }
  GIVEN("Two unlabelled qubits, two slices, lookahead, check and route.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[2], qubits[1]});
    circ.add_op<UnitID>(OpType::ZZPhase, 0.8, {qubits[4], qubits[3]});
    circ.add_op<UnitID>(OpType::CX, {qubits[2], qubits[4]});

    std::map<UnitID, UnitID> rename_map = {
        {qubits[4], nodes[4]}, {qubits[1], nodes[1]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    VertPort pre_label_0 =
        mf->linear_boundary->get<TagKey>().find(qubits[2])->second;
    VertPort pre_label_3 =
        mf->linear_boundary->get<TagKey>().find(qubits[3])->second;
    LexiLabellingMethod lrm;
    std::pair<bool, unit_map_t> out = lrm.routing_method(mf, shared_arc);
    REQUIRE(out.first);
    REQUIRE(
        mf->linear_boundary->get<TagKey>().find(qubits[2]) ==
        mf->linear_boundary->get<TagKey>().end());
    REQUIRE(
        mf->linear_boundary->get<TagKey>().find(qubits[3]) ==
        mf->linear_boundary->get<TagKey>().end());
    VertPort post_label_0 =
        mf->linear_boundary->get<TagKey>().find(nodes[0])->second;
    REQUIRE(pre_label_0 == post_label_0);
    VertPort post_label_3 =
        mf->linear_boundary->get<TagKey>().find(nodes[3])->second;
    REQUIRE(pre_label_3 == post_label_3);
  }
  GIVEN(
      "Two unlabelled qubits, two slices, lookahead unrouted, check and "
      "route.") {
    Circuit circ(5);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[2], qubits[1]});
    circ.add_op<UnitID>(OpType::ZZPhase, 0.8, {qubits[4], qubits[3]});
    circ.add_op<UnitID>(OpType::CX, {qubits[2], qubits[0]});

    std::map<UnitID, UnitID> rename_map = {
        {qubits[4], nodes[4]}, {qubits[1], nodes[1]}};
    circ.rename_units(rename_map);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    VertPort pre_label_0 =
        mf->linear_boundary->get<TagKey>().find(qubits[2])->second;
    VertPort pre_label_3 =
        mf->linear_boundary->get<TagKey>().find(qubits[3])->second;
    LexiLabellingMethod lrm;
    std::pair<bool, unit_map_t> out = lrm.routing_method(mf, shared_arc);
    REQUIRE(out.first);
    REQUIRE(
        mf->linear_boundary->get<TagKey>().find(qubits[2]) ==
        mf->linear_boundary->get<TagKey>().end());
    REQUIRE(
        mf->linear_boundary->get<TagKey>().find(qubits[3]) ==
        mf->linear_boundary->get<TagKey>().end());
    VertPort post_label_0 =
        mf->linear_boundary->get<TagKey>().find(nodes[0])->second;
    REQUIRE(pre_label_0 == post_label_0);
    VertPort post_label_3 =
        mf->linear_boundary->get<TagKey>().find(nodes[3])->second;
    REQUIRE(pre_label_3 == post_label_3);
  }
}
SCENARIO("Test LexiRouteRoutingMethod") {
  std::vector<Node> nodes = {
      Node("test_node", 0), Node("test_node", 1), Node("test_node", 2),
      Node("node_test", 3), Node("node_test", 4), Node("node_test", 5),
      Node("test_node", 6), Node("node_test", 7), Node("node_test", 8),
      Node("node_test", 9), Node("node_test", 10)};
  //       n9 -- n8 -- n10
  //             |     |
  // n0 -- n1 -- n2 -- n3 -- n4
  //             |     |
  //             n5    n7
  //             |
  //             n6
  Architecture architecture(
      {{nodes[0], nodes[1]},
       {nodes[1], nodes[2]},
       {nodes[2], nodes[3]},
       {nodes[3], nodes[4]},
       {nodes[2], nodes[5]},
       {nodes[5], nodes[6]},
       {nodes[3], nodes[7]},
       {nodes[2], nodes[8]},
       {nodes[8], nodes[9]},
       {nodes[8], nodes[10]},
       {nodes[3], nodes[10]}});
  ArchitecturePtr shared_arc = std::make_shared<Architecture>(architecture);
  GIVEN("Circuit with all qubits, labelled, stage 0.") {
    Circuit circ(11);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[4]});
    circ.add_op<UnitID>(OpType::CX, {qubits[6], qubits[7]});
    circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[10]});
    circ.add_op<UnitID>(OpType::CX, {qubits[8], qubits[5]});
    circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[9]});

    circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[9]});
    circ.add_op<UnitID>(OpType::CX, {qubits[10], qubits[0]});
    circ.add_op<UnitID>(OpType::CX, {qubits[6], qubits[0]});

    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]},  {qubits[2], nodes[2]},
        {qubits[3], nodes[3]}, {qubits[4], nodes[4]},  {qubits[5], nodes[5]},
        {qubits[6], nodes[6]}, {qubits[7], nodes[7]},  {qubits[8], nodes[8]},
        {qubits[9], nodes[9]}, {qubits[10], nodes[10]}};
    circ.rename_units(rename_map);

    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiRouteRoutingMethod lrrm(100);
    std::pair<bool, unit_map_t> bool_init_map =
        lrrm.routing_method(mf, shared_arc);
    REQUIRE(bool_init_map.first);
    REQUIRE(bool_init_map.second.size() == 0);

    std::vector<Command> commands = mf->circuit_.get_commands();
    REQUIRE(commands.size() == 9);
    Command bridge_c = commands[2];
    unit_vector_t uids = {nodes[8], nodes[2], nodes[5]};
    REQUIRE(bridge_c.get_args() == uids);
    REQUIRE(*bridge_c.get_op_ptr() == *get_op_ptr(OpType::BRIDGE));
  }
  GIVEN("Circuit with all qubits, labelled, stage 1.") {
    Circuit circ(11);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[4]});
    circ.add_op<UnitID>(OpType::CX, {qubits[6], qubits[7]});
    circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[10]});
    circ.add_op<UnitID>(OpType::CX, {qubits[8], qubits[5]});
    circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[9]});
    //       n9 -- n8 -- n3
    //             |     |
    // n0 -- n1 -- n2 -- n10 -- n4
    //             |     |
    //             n6    n7
    //             |
    //             n5
    circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[9]});
    circ.add_op<UnitID>(OpType::CX, {qubits[10], qubits[0]});
    circ.add_op<UnitID>(OpType::CX, {qubits[6], qubits[0]});

    std::map<UnitID, UnitID> rename_map = {
        {qubits[0], nodes[0]}, {qubits[1], nodes[1]},  {qubits[2], nodes[2]},
        {qubits[3], nodes[3]}, {qubits[4], nodes[4]},  {qubits[5], nodes[6]},
        {qubits[6], nodes[5]}, {qubits[7], nodes[7]},  {qubits[8], nodes[8]},
        {qubits[9], nodes[9]}, {qubits[10], nodes[10]}};
    circ.rename_units(rename_map);

    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(circ);
    LexiRouteRoutingMethod lrrm(100);
    std::pair<bool, unit_map_t> bool_init_map =
        lrrm.routing_method(mf, shared_arc);
    REQUIRE(bool_init_map.first);
    REQUIRE(bool_init_map.second.size() == 0);
    std::vector<Command> commands = mf->circuit_.get_commands();
    REQUIRE(commands.size() == 10);
    Command swap_c = commands[0];
    unit_vector_t uids = {nodes[3], nodes[10]};
    REQUIRE(swap_c.get_args() == uids);
    REQUIRE(*swap_c.get_op_ptr() == *get_op_ptr(OpType::SWAP));
  }
}
SCENARIO("Test MappingManager with LexiRouteRoutingMethod and LexiLabelling") {
  GIVEN("11 Node Architecture, 11 Qubit circuit, multiple SWAP required.") {
    std::vector<Node> nodes = {
        Node("test_node", 0), Node("test_node", 1), Node("test_node", 2),
        Node("node_test", 3), Node("node_test", 4), Node("node_test", 5),
        Node("test_node", 6), Node("node_test", 7), Node("node_test", 8),
        Node("node_test", 9), Node("node_test", 10)};
    //       n9 -- n8 -- n10
    //             |     |
    // n0 -- n1 -- n2 -- n3 -- n4
    //             |     |
    //             n5    n7
    //             |
    //             n6
    Architecture architecture(
        {{nodes[0], nodes[1]},
         {nodes[1], nodes[2]},
         {nodes[2], nodes[3]},
         {nodes[3], nodes[4]},
         {nodes[2], nodes[5]},
         {nodes[5], nodes[6]},
         {nodes[3], nodes[7]},
         {nodes[2], nodes[8]},
         {nodes[8], nodes[9]},
         {nodes[8], nodes[10]},
         {nodes[3], nodes[10]}});
    ArchitecturePtr shared_arc = std::make_shared<Architecture>(architecture);
    Circuit circ(11);
    std::vector<Qubit> qubits = circ.all_qubits();
    for (unsigned i = 0; i < 11; i++) {
      circ.add_op<UnitID>(OpType::CX, {qubits[0], qubits[4]});
      circ.add_op<UnitID>(OpType::CX, {qubits[6], qubits[7]});
      circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[10]});
      circ.add_op<UnitID>(OpType::CX, {qubits[8], qubits[5]});
      circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[9]});
      circ.add_op<UnitID>(OpType::CX, {qubits[2], qubits[8]});

      circ.add_op<UnitID>(OpType::CX, {qubits[1], qubits[5]});
      circ.add_op<UnitID>(OpType::CX, {qubits[3], qubits[9]});
      circ.add_op<UnitID>(OpType::CX, {qubits[10], qubits[0]});
      circ.add_op<UnitID>(OpType::CX, {qubits[6], qubits[0]});
    }

    Circuit copy_circ(circ);
    // transform stuff
    PassPtr dec = gen_decompose_routing_gates_to_cxs_pass(architecture, false);

    MappingManager mm(shared_arc);
    MappingFrontier_ptr mf = std::make_shared<MappingFrontier>(copy_circ);

    LexiLabellingMethod lrm;
    std::vector<RoutingMethodPtr> vrm = {
        std::make_shared<LexiLabellingMethod>(lrm),
        std::make_shared<LexiRouteRoutingMethod>()};
    // Contains initial and final map
    std::shared_ptr<unit_bimaps_t> maps = std::make_shared<unit_bimaps_t>();
    // Initialise the maps by the same way it's done with CompilationUnit
    for (const UnitID& u : circ.all_units()) {
      maps->initial.insert({u, u});
      maps->final.insert({u, u});
    }

    bool res = mm.route_circuit_with_maps(circ, vrm, maps);

    PredicatePtr routed_correctly =
        std::make_shared<ConnectivityPredicate>(architecture);
    PredicatePtrMap preds{CompilationUnit::make_type_pair(routed_correctly)};
    CompilationUnit cu0(circ, preds);
    dec->apply(cu0);
    REQUIRE(res);
    REQUIRE(cu0.check_all_predicates());
    REQUIRE(check_permutation(circ, maps));
  }
  GIVEN("Square Grid Architecture, large number of gates.") {
    SquareGrid sg(5, 10);
    ArchitecturePtr shared_arc = std::make_shared<Architecture>(sg);
    Circuit circ(35);
    std::vector<Qubit> qubits = circ.all_qubits();
    for (unsigned i = 0; i < qubits.size() - 1; i++) {
      circ.add_op<UnitID>(OpType::CX, {qubits[i], qubits[i + 1]});
    }
    for (unsigned i = 0; i < qubits.size() - 2; i++) {
      circ.add_op<UnitID>(OpType::CZ, {qubits[i], qubits[i + 2]});
    }
    // transform stuff
    PassPtr dec = gen_decompose_routing_gates_to_cxs_pass(sg, false);

    MappingManager mm(shared_arc);
    LexiLabellingMethod lrm;
    std::vector<RoutingMethodPtr> vrm = {
        std::make_shared<LexiLabellingMethod>(lrm),
        std::make_shared<LexiRouteRoutingMethod>()};
    bool res = mm.route_circuit(circ, vrm);

    PredicatePtr routed_correctly = std::make_shared<ConnectivityPredicate>(sg);
    PredicatePtrMap preds{CompilationUnit::make_type_pair(routed_correctly)};
    CompilationUnit cu(circ, preds);
    dec->apply(cu);
    REQUIRE(res);
    REQUIRE(cu.check_all_predicates());
    REQUIRE(circ.n_gates() == 88);
  }
}

SCENARIO(
    "Check that an already solved routing problem will not add unnecessary "
    "swaps") {
  GIVEN("A solved problem") {
    Circuit test_circuit;
    test_circuit.add_blank_wires(4);
    add_2qb_gates(test_circuit, OpType::CX, {{0, 1}, {1, 2}, {2, 3}, {3, 0}});

    // Ring of size 4
    RingArch arc(4);
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        test_circuit, {std::make_shared<LexiLabellingMethod>(),
                       std::make_shared<LexiRouteRoutingMethod>()}));
    REQUIRE(test_circuit.n_gates() == 4);
  }
  GIVEN("A solved problem supplied with map and custom architecture") {
    Circuit test_circuit;
    test_circuit.add_blank_wires(4);
    add_2qb_gates(test_circuit, OpType::CX, {{0, 1}, {1, 2}, {2, 3}, {3, 0}});

    Architecture test_arc(
        {{Node(0), Node(1)},
         {Node(1), Node(2)},
         {Node(2), Node(3)},
         {Node(3), Node(0)}});
    Placement test_p(test_arc);

    std::map<Qubit, Node> map_;
    for (unsigned nn = 0; nn <= 3; ++nn) {
      map_[Qubit(nn)] = Node(nn);
    }
    test_p.place_with_map(test_circuit, map_);
    qubit_vector_t all_qs_post_place = test_circuit.all_qubits();

    MappingManager mm(std::make_shared<Architecture>(test_arc));
    REQUIRE(!mm.route_circuit(
        test_circuit, {std::make_shared<LexiLabellingMethod>(),
                       std::make_shared<LexiRouteRoutingMethod>()}));

    qubit_vector_t all_qs_post_solve = test_circuit.all_qubits();
    REQUIRE(all_qs_post_place == all_qs_post_solve);
    REQUIRE(test_circuit.n_gates() == 4);
  }
}

SCENARIO("Empty Circuit test") {
  GIVEN("An Empty Circuit") {
    Circuit circ;
    circ.add_blank_wires(4);
    Architecture arc(
        {{Node(0), Node(1)}, {Node(1), Node(2)}, {Node(2), Node(3)}});
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        circ, {
                  std::make_shared<LexiLabellingMethod>(),
                  std::make_shared<LexiRouteRoutingMethod>(),
              }));
    REQUIRE(circ.n_gates() == 0);
  }
}

SCENARIO("Routing on circuit with no multi-qubit gates") {
  GIVEN("A circuit with no multi-qubit gates") {
    Circuit circ;
    circ.add_blank_wires(4);
    add_1qb_gates(circ, OpType::X, {0, 2});
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::Y, {1});

    unsigned orig_vertices = circ.n_vertices();
    Architecture arc(
        {{Node(0), Node(1)}, {Node(1), Node(2)}, {Node(2), Node(3)}});
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        circ, {
                  std::make_shared<LexiLabellingMethod>(),
                  std::make_shared<LexiRouteRoutingMethod>(),
              }));
    REQUIRE(orig_vertices - 8 == circ.n_gates());
  }
}

SCENARIO("Test routing on a directed architecture with bidirectional edges") {
  GIVEN("A simple two-qubit circuit") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Architecture arc({{Node(0), Node(1)}, {Node(1), Node(0)}});
    Architecture arc2({std::pair<Node, Node>{Node(0), Node(1)}});

    // routing ignored bi directional edge and solves correctly
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        circ, {std::make_shared<LexiLabellingMethod>(),
               std::make_shared<LexiRouteRoutingMethod>()}));
    REQUIRE(circ.n_gates() == 2);
    CHECK(respects_connectivity_constraints(circ, arc, false));
  }
}

SCENARIO(
    "Test routing on a directed architecture doesn't throw an error if "
    "non-cx optype is presented") {
  GIVEN(
      "A simple two-qubit circuit with non-cx multi-qubit gates and a "
      "directed architecture") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CU1, 0.5, {1, 0});
    circ.add_op<unsigned>(OpType::CU1, 0.5, {0, 1});
    circ.add_op<unsigned>(OpType::CY, {1, 0});
    circ.add_op<unsigned>(OpType::CY, {0, 1});
    circ.add_op<unsigned>(OpType::CZ, {1, 0});
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::CRz, 0.5, {1, 0});
    circ.add_op<unsigned>(OpType::CRz, 0.5, {0, 1});

    Architecture arc({std::pair<Node, Node>{Node(0), Node(1)}});
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        circ, {std::make_shared<LexiLabellingMethod>(),
               std::make_shared<LexiRouteRoutingMethod>()}));
    REQUIRE(circ.n_gates() == 8);
  }
}

SCENARIO("Dense CX circuits route successfully") {
  GIVEN(
      "Complex CX circuits for large directed architecture based off "
      "IBMTokyo") {
    Circuit circ(17);
    for (unsigned x = 0; x < 17; ++x) {
      for (unsigned y = 0; y + 1 < x; ++y) {
        if (x % 2) {  // swap the way directed chain runs each time
          add_2qb_gates(circ, OpType::CX, {{x, y}, {y + 1, y}});
        } else {
          add_2qb_gates(circ, OpType::CX, {{y, x}, {y, y + 1}});
        }
      }
    }
    Architecture arc(
        std::vector<std::pair<unsigned, unsigned>>{
            {0, 1},   {1, 2},   {2, 3},   {3, 4},   {0, 5},   {1, 6},
            {1, 7},   {2, 6},   {2, 7},   {3, 8},   {3, 9},   {4, 8},
            {4, 9},   {5, 6},   {5, 10},  {5, 11},  {6, 10},  {6, 11},
            {6, 7},   {7, 12},  {7, 13},  {7, 8},   {8, 12},  {8, 13},
            {8, 9},   {10, 11}, {11, 16}, {11, 17}, {11, 12}, {12, 16},
            {12, 17}, {12, 13}, {13, 18}, {13, 19}, {13, 14}, {14, 18},
            {14, 19}, {15, 16}, {16, 17}, {17, 18}, {18, 19}});
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        circ, {std::make_shared<LexiLabellingMethod>(),
               std::make_shared<LexiRouteRoutingMethod>()}));
    (Transforms::decompose_SWAP_to_CX() >> Transforms::decompose_BRIDGE_to_CX())
        .apply(circ);

    Transforms::decompose_CX_directed(arc).apply(circ);
    REQUIRE(respects_connectivity_constraints(circ, arc, true));
  }
}

SCENARIO(
    "Dense CX circuits route successfully on undirected Ring with "
    "placement.",
    "[.long]") {
  GIVEN("Complex CX circuits, big ring") {
    Circuit circ(29);
    for (unsigned x = 0; x < 29; ++x) {
      for (unsigned y = 0; y + 1 < x; ++y) {
        if (x % 2) {
          add_2qb_gates(circ, OpType::CX, {{x, y}, {y + 1, y}});
        } else {
          add_2qb_gates(circ, OpType::CX, {{y, x}, {y, y + 1}});
        }
      }
    }
    RingArch arc(29);
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        circ, {std::make_shared<LexiLabellingMethod>(),
               std::make_shared<LexiRouteRoutingMethod>()}));
    Transforms::decompose_SWAP_to_CX().apply(circ);
    REQUIRE(respects_connectivity_constraints(circ, arc, false, true));
  }
}

SCENARIO(
    "Dense CX circuits route successfully on smart placement unfriendly "
    "architecture.") {
  GIVEN("Complex CX circuits, big ring") {
    Circuit circ(13);
    for (unsigned x = 0; x < 13; ++x) {
      for (unsigned y = 0; y + 1 < x; ++y) {
        if (x % 2) {
          add_2qb_gates(circ, OpType::CX, {{x, y}, {y + 1, y}});
        } else {
          add_2qb_gates(circ, OpType::CX, {{y, x}, {y, y + 1}});
        }
      }
    }
    Architecture arc(
        std::vector<std::pair<unsigned, unsigned>>{
            {0, 1},
            {2, 0},
            {2, 4},
            {6, 4},
            {8, 6},
            {8, 10},
            {12, 10},
            {3, 1},
            {3, 5},
            {7, 5},
            {7, 9},
            {11, 9},
            {11, 13},
            {12, 13},
            {6, 7}});
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        circ, {std::make_shared<LexiLabellingMethod>(),
               std::make_shared<LexiRouteRoutingMethod>()}));
    REQUIRE(respects_connectivity_constraints(circ, arc, false, true));
  }
}

SCENARIO("Empty circuits, with and without blank wires") {
  GIVEN("An empty circuit with some qubits") {
    Circuit circ(6);
    RingArch arc(6);
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        circ, {
                  std::make_shared<LexiLabellingMethod>(),
                  std::make_shared<LexiRouteRoutingMethod>(),
              }));
    REQUIRE(circ.depth() == 0);
    REQUIRE(circ.n_gates() == 0);
    REQUIRE(circ.n_qubits() == 6);
    REQUIRE(respects_connectivity_constraints(circ, arc, true));
  }
  GIVEN("An empty circuit with some qubits with labelling") {
    Circuit circ(6);
    RingArch arc(6);
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(mm.route_circuit(
        circ, {std::make_shared<LexiLabellingMethod>(),
               std::make_shared<LexiRouteRoutingMethod>()}));
    REQUIRE(circ.depth() == 0);
    REQUIRE(circ.n_gates() == 0);
    REQUIRE(circ.n_qubits() == 6);
    REQUIRE(respects_connectivity_constraints(circ, arc, true));
  }
  GIVEN("An empty circuit with no qubits") {
    Circuit circ(0);
    RingArch arc(6);
    MappingManager mm(std::make_shared<Architecture>(arc));
    REQUIRE(!mm.route_circuit(
        circ, {std::make_shared<LexiLabellingMethod>(),
               std::make_shared<LexiRouteRoutingMethod>()}));
    REQUIRE(circ.depth() == 0);
    REQUIRE(circ.n_gates() == 0);
    REQUIRE(circ.n_qubits() == 0);
  }
}

SCENARIO("Initial map should contain all data qubits") {
  GIVEN("An example circuit") {
    Circuit circ(10);
    std::vector<Qubit> qubits = circ.all_qubits();
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[4]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[3], qubits[2]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[7], qubits[6]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[0]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[8], qubits[7]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[9], qubits[4]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[6], qubits[5]});
    SquareGrid sg(4, 4);
    // Contains initial and final map
    std::shared_ptr<unit_bimaps_t> maps = std::make_shared<unit_bimaps_t>();
    // Initialise the maps by the same way it's done with CompilationUnit
    for (const UnitID& u : circ.all_units()) {
      maps->initial.insert({u, u});
      maps->final.insert({u, u});
    }

    MappingManager mm(std::make_shared<Architecture>(sg));
    mm.route_circuit_with_maps(
        circ,
        {std::make_shared<LexiLabellingMethod>(),
         std::make_shared<LexiRouteRoutingMethod>()},
        maps);
    for (auto q : qubits) {
      REQUIRE(maps->initial.left.find(q) != maps->initial.left.end());
      REQUIRE(maps->final.left.find(q) != maps->final.left.end());
    }
    REQUIRE(check_permutation(circ, maps));
  }
  GIVEN("An example circuit with remap") {
    Circuit circ(10);
    SquareGrid sg(4, 4);
    std::vector<Node> nodes = sg.get_all_nodes_vec();
    std::vector<Qubit> qubits = circ.all_qubits();

    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[4]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[3], qubits[2]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[7], qubits[6]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[0]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[8], qubits[7]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[9], qubits[4]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[6], qubits[5]});

    std::map<UnitID, UnitID> rename_map;

    for (unsigned i = 0; i < 10; ++i) {
      rename_map.insert({qubits[i], nodes[i]});
    }

    circ.rename_units(rename_map);

    Circuit initial_circ = Circuit(circ);

    // Contains initial and final map
    std::shared_ptr<unit_bimaps_t> maps = std::make_shared<unit_bimaps_t>();
    // Initialise the maps by the same way it's done with CompilationUnit
    for (const UnitID& u : circ.all_units()) {
      maps->initial.insert({u, u});
      maps->final.insert({u, u});
    }

    MappingManager mm(std::make_shared<Architecture>(sg));
    mm.route_circuit_with_maps(
        circ,
        {std::make_shared<LexiLabellingMethod>(),
         std::make_shared<LexiRouteRoutingMethod>()},
        maps);
    for (auto q : circ.all_qubits()) {
      // circuit is labeled with physical nodes
      REQUIRE(maps->initial.right.find(q) != maps->initial.right.end());
      REQUIRE(maps->final.right.find(q) != maps->final.right.end());
    }
    REQUIRE(check_permutation(circ, maps));
  }
  GIVEN("An example circuit with remap II") {
    Circuit circ(6);
    SquareGrid sg(3, 3);
    std::vector<Node> nodes = sg.get_all_nodes_vec();
    std::vector<Qubit> qubits = circ.all_qubits();

    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[3], qubits[1]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[0]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});

    std::map<UnitID, UnitID> rename_map;

    for (unsigned i = 0; i < 6; ++i) {
      rename_map.insert({qubits[i], nodes[i]});
    }

    circ.rename_units(rename_map);

    Circuit initial_circ = Circuit(circ);

    // Contains initial and final map
    std::shared_ptr<unit_bimaps_t> maps = std::make_shared<unit_bimaps_t>();
    // Initialise the maps by the same way it's done with CompilationUnit
    for (const UnitID& u : circ.all_units()) {
      maps->initial.insert({u, u});
      maps->final.insert({u, u});
    }

    MappingManager mm(std::make_shared<Architecture>(sg));
    mm.route_circuit_with_maps(
        circ,
        {std::make_shared<LexiLabellingMethod>(),
         std::make_shared<LexiRouteRoutingMethod>()},
        maps);
    for (auto q : circ.all_qubits()) {
      REQUIRE(maps->initial.right.find(q) != maps->initial.right.end());
      REQUIRE(maps->final.right.find(q) != maps->final.right.end());
    }
    REQUIRE(check_permutation(circ, maps));

    std::vector<Qubit> qubits_renamed = circ.all_qubits();

    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[1], qubits_renamed[4]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[3], qubits_renamed[4]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[1], qubits_renamed[2]});

    REQUIRE(test_unitary_comparison(initial_circ, circ));
  }
  GIVEN("An example circuit with remap III") {
    Circuit circ(6);
    SquareGrid sg(3, 3);
    std::vector<Node> nodes = sg.get_all_nodes_vec();
    std::vector<Qubit> qubits = circ.all_qubits();

    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[3], qubits[1]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[0]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[3], qubits[1]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[0]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[3], qubits[1]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[0]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[5]});

    std::map<UnitID, UnitID> rename_map;

    for (unsigned i = 0; i < 6; ++i) {
      rename_map.insert({qubits[i], nodes[i]});
    }

    circ.rename_units(rename_map);

    Circuit initial_circ = Circuit(circ);

    // Contains initial and final map
    std::shared_ptr<unit_bimaps_t> maps = std::make_shared<unit_bimaps_t>();
    // Initialise the maps by the same way it's done with CompilationUnit
    for (const UnitID& u : circ.all_units()) {
      maps->initial.insert({u, u});
      maps->final.insert({u, u});
    }

    MappingManager mm(std::make_shared<Architecture>(sg));
    mm.route_circuit_with_maps(
        circ,
        {std::make_shared<LexiLabellingMethod>(),
         std::make_shared<LexiRouteRoutingMethod>()},
        maps);
    for (auto q : circ.all_qubits()) {
      REQUIRE(maps->initial.right.find(q) != maps->initial.right.end());
      REQUIRE(maps->final.right.find(q) != maps->final.right.end());
    }
    REQUIRE(check_permutation(circ, maps));

    std::vector<Qubit> qubits_renamed = circ.all_qubits();

    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[2], qubits_renamed[5]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[3], qubits_renamed[4]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[1], qubits_renamed[2]});

    REQUIRE(test_unitary_comparison(initial_circ, circ));
  }
  GIVEN("An example circuit with remap IV") {
    Circuit circ(6);
    SquareGrid sg(3, 3);
    std::vector<Node> nodes = sg.get_all_nodes_vec();
    std::vector<Qubit> qubits = circ.all_qubits();

    circ.add_op<UnitID>(OpType::H, {qubits[0]});
    circ.add_op<UnitID>(OpType::H, {qubits[1]});
    circ.add_op<UnitID>(OpType::H, {qubits[2]});
    circ.add_op<UnitID>(OpType::H, {qubits[3]});
    circ.add_op<UnitID>(OpType::H, {qubits[4]});
    circ.add_op<UnitID>(OpType::H, {qubits[5]});

    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[3], qubits[1]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[0]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[3]});

    circ.add_op<UnitID>(OpType::Y, {qubits[0]});
    circ.add_op<UnitID>(OpType::Y, {qubits[1]});
    circ.add_op<UnitID>(OpType::Y, {qubits[2]});
    circ.add_op<UnitID>(OpType::Y, {qubits[3]});
    circ.add_op<UnitID>(OpType::Y, {qubits[4]});
    circ.add_op<UnitID>(OpType::Y, {qubits[5]});

    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[3], qubits[1]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[0]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[3]});

    circ.add_op<UnitID>(OpType::Y, {qubits[0]});
    circ.add_op<UnitID>(OpType::Y, {qubits[1]});
    circ.add_op<UnitID>(OpType::Y, {qubits[2]});
    circ.add_op<UnitID>(OpType::Y, {qubits[3]});
    circ.add_op<UnitID>(OpType::Y, {qubits[4]});
    circ.add_op<UnitID>(OpType::Y, {qubits[5]});

    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[3], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[1]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[2], qubits[0]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[1], qubits[5]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[4], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[0], qubits[3]});
    circ.add_op<UnitID>(OpType::CZ, {qubits[0], qubits[5]});

    std::map<UnitID, UnitID> rename_map;

    for (unsigned i = 0; i < 6; ++i) {
      rename_map.insert({qubits[i], nodes[i]});
    }

    circ.rename_units(rename_map);

    Circuit initial_circ = Circuit(circ);

    // Contains initial and final map
    std::shared_ptr<unit_bimaps_t> maps = std::make_shared<unit_bimaps_t>();
    // Initialise the maps by the same way it's done with CompilationUnit
    for (const UnitID& u : circ.all_units()) {
      maps->initial.insert({u, u});
      maps->final.insert({u, u});
    }

    MappingManager mm(std::make_shared<Architecture>(sg));
    mm.route_circuit_with_maps(
        circ,
        {std::make_shared<LexiLabellingMethod>(),
         std::make_shared<LexiRouteRoutingMethod>()},
        maps);

    for (auto q : circ.all_qubits()) {
      REQUIRE(maps->initial.right.find(q) != maps->initial.right.end());
      REQUIRE(maps->final.right.find(q) != maps->final.right.end());
    }
    REQUIRE(check_permutation(circ, maps));

    std::vector<Qubit> qubits_renamed = circ.all_qubits();

    // add swaps to resolve permutation
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[1], qubits_renamed[2]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[4], qubits_renamed[5]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[1], qubits_renamed[4]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[1], qubits_renamed[3]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[2], qubits_renamed[5]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[1], qubits_renamed[2]});
    circ.add_op<UnitID>(OpType::SWAP, {qubits_renamed[3], qubits_renamed[4]});

    REQUIRE(test_unitary_comparison(initial_circ, circ));
  }
}
SCENARIO("Unlabelled qubits should be assigned to ancilla qubits.") {
  Architecture arc(
      {{Node(0), Node(1)},
       {Node(1), Node(2)},
       {Node(2), Node(3)},
       {Node(3), Node(0)}});
  Circuit c(4);
  c.add_op<unsigned>(OpType::CZ, {0, 3});
  c.add_op<unsigned>(OpType::CZ, {1, 0});
  c.add_op<unsigned>(OpType::CZ, {3, 1});
  c.add_op<unsigned>(OpType::H, {2});

  std::shared_ptr<unit_bimaps_t> maps = std::make_shared<unit_bimaps_t>();
  // Initialise the maps by the same way it's done with CompilationUnit
  for (const UnitID& u : c.all_units()) {
    maps->initial.insert({u, u});
    maps->final.insert({u, u});
  }

  MappingManager mm(std::make_shared<Architecture>(arc));
  mm.route_circuit_with_maps(
      c,
      {std::make_shared<LexiLabellingMethod>(),
       std::make_shared<LexiRouteRoutingMethod>()},
      maps);
  REQUIRE(maps->initial.left.find(Qubit(0))->second == Node(1));
  REQUIRE(maps->initial.left.find(Qubit(1))->second == Node(2));
  REQUIRE(maps->initial.left.find(Qubit(2))->second == Node(3));
  REQUIRE(maps->initial.left.find(Qubit(3))->second == Node(0));
  REQUIRE(maps->final.left.find(Qubit(0))->second == Node(1));
  REQUIRE(maps->final.left.find(Qubit(1))->second == Node(3));
  REQUIRE(maps->final.left.find(Qubit(2))->second == Node(2));
  REQUIRE(maps->final.left.find(Qubit(3))->second == Node(0));
}
SCENARIO("Lexi relabel with partially mapped circuit") {
  GIVEN("With an unplaced qubit") {
    Architecture arc({{Node(0), Node(1)}, {Node(1), Node(2)}});
    Circuit c(3);
    c.add_op<unsigned>(OpType::CZ, {0, 1}, "cz0,1");
    c.add_op<unsigned>(OpType::CZ, {1, 2}, "cz1,2");
    std::shared_ptr<unit_bimaps_t> maps = std::make_shared<unit_bimaps_t>();
    // Initialise the maps by the same way it's done with CompilationUnit
    for (const UnitID& u : c.all_units()) {
      maps->initial.insert({u, u});
      maps->final.insert({u, u});
    }
    Placement pl(arc);
    std::map<Qubit, Node> partial_map;
    partial_map.insert({Qubit(0), Node(0)});
    partial_map.insert({Qubit(1), Node(1)});
    pl.place_with_map(c, partial_map, maps);

    MappingManager mm(std::make_shared<Architecture>(arc));
    mm.route_circuit_with_maps(
        c,
        {std::make_shared<LexiLabellingMethod>(),
         std::make_shared<LexiRouteRoutingMethod>()},
        maps);
    REQUIRE(check_permutation(c, maps));
  }
  GIVEN("With an unplaced qubit merged to an ancilla") {
    Circuit c(4);
    c.add_op<unsigned>(OpType::CZ, {3, 0}, "cz3,0");
    c.add_op<unsigned>(OpType::CZ, {1, 0}, "cz1,0");
    c.add_op<unsigned>(OpType::CZ, {1, 3}, "cz1,3");
    c.add_op<unsigned>(OpType::CZ, {3, 2}, "cz3,2");

    Architecture arc(
        {{Node(0), Node(1)},
         {Node(0), Node(2)},
         {Node(0), Node(3)},
         {Node(4), Node(1)},
         {Node(4), Node(2)}});
    PassPtr plac_p = gen_placement_pass(std::make_shared<GraphPlacement>(arc));
    CompilationUnit cu(c);
    REQUIRE(plac_p->apply(cu));
    const unit_bimap_t& initial_map = cu.get_initial_map_ref();
    const unit_bimap_t& final_map = cu.get_final_map_ref();

    PassPtr r_p = gen_routing_pass(
        arc, {std::make_shared<LexiLabellingMethod>(),
              std::make_shared<LexiRouteRoutingMethod>()});
    REQUIRE(r_p->apply(cu));

    for (const Qubit& q : c.all_qubits()) {
      REQUIRE(initial_map.left.find(q) != initial_map.left.end());
      REQUIRE(final_map.left.find(q) != final_map.left.end());
    }
    for (const Qubit& q : cu.get_circ_ref().all_qubits()) {
      REQUIRE(initial_map.right.find(q) != initial_map.right.end());
      REQUIRE(final_map.right.find(q) != final_map.right.end());
    }
  }
}

SCENARIO("Test failing case") {
  std::ifstream circuit_file("lexiroute_circuit.json");
  nlohmann::json j = nlohmann::json::parse(circuit_file);
  auto c = j.get<Circuit>();
  Architecture arc(
      std::vector<std::pair<unsigned, unsigned>>{
          {0, 1},   {1, 2},   {2, 3},   {3, 5},   {4, 1},   {4, 7},
          {5, 8},   {6, 7},   {7, 10},  {8, 9},   {8, 11},  {10, 12},
          {11, 14}, {12, 13}, {14, 13}, {14, 16}, {12, 15}, {15, 18},
          {17, 18}, {16, 19}, {19, 20}, {18, 21}, {21, 23}, {19, 22},
          {22, 25}, {23, 24}, {24, 25}, {25, 26}});

  CompilationUnit cu(c);
  PassPtr r_p = gen_routing_pass(
      arc, {std::make_shared<LexiLabellingMethod>(),
            std::make_shared<LexiRouteRoutingMethod>()});
  REQUIRE(r_p->apply(cu));
}
SCENARIO("Test RangePredicate operations with LexiRoute.") {
  std::vector<uint64_t> and_table = {0, 1, 2, 7, 0, 1, 2, 7};
  std::shared_ptr<ClassicalTransformOp> and_ttop =
      std::make_shared<ClassicalTransformOp>(3, and_table);
  for (unsigned i = 0; i < 2; i++) {
    for (unsigned j = 0; j < 2; j++) {
      for (unsigned k = 0; k < 2; k++) {
        std::vector<bool> y = and_ttop->eval({(bool)i, (bool)j, (bool)k});
        REQUIRE(y[0] == i);
        REQUIRE(y[1] == j);
        REQUIRE(y[2] == (i & j));
      }
    }
  }

  uint64_t a = 2, b = 6;
  std::shared_ptr<RangePredicateOp> rpop =
      std::make_shared<RangePredicateOp>(3, a, b);
  for (uint64_t x = 0; x < 8; x++) {
    REQUIRE(
        rpop->eval(
            {(bool)(x & 1), (bool)((x >> 1) & 1), (bool)((x >> 2) & 1)})[0] ==
        (x >= a && x <= b));
  }

  Circuit circ(3, 4);
  circ.add_op<unsigned>(OpType::H, {0});
  circ.add_op<unsigned>(and_ttop, {0, 1, 2});
  circ.add_op<unsigned>(and_ttop, {1, 2, 3});
  circ.add_op<unsigned>(rpop, {0, 1, 2, 3});
  circ.add_op<unsigned>(AndOp(), {2, 3, 0});
  circ.add_op<unsigned>(OrOp(), {0, 1, 2});
  circ.add_op<unsigned>(NotOp(), {2, 3});
  circ.add_op<unsigned>(OpType::CX, {0, 1});
  circ.add_op<unsigned>(ClassicalX(), {1});
  circ.add_conditional_gate<unsigned>(OpType::CZ, {}, {0, 1}, {0}, 1);
  circ.add_op<unsigned>(ClassicalCX(), {0, 1});
  circ.add_op<unsigned>(AndWithOp(), {2, 3});
  circ.add_conditional_gate<unsigned>(OpType::CX, {}, {1, 0}, {0, 1, 2}, 1);
  circ.add_op<unsigned>(OrWithOp(), {1, 0});
  circ.add_op<unsigned>(OpType::CX, {2, 0});
  circ.add_op<unsigned>(OpType::CX, {2, 1});
  circ.add_op<unsigned>(OpType::H, {0});
  circ.add_op<unsigned>(OpType::H, {1});
  circ.add_op<unsigned>(OpType::H, {2});
  circ.add_op<unsigned>(AndOp(), {2, 3, 0});
  circ.add_op<unsigned>(OrOp(), {0, 1, 2});
  circ.add_op<unsigned>(NotOp(), {2, 3});
  circ.add_op<unsigned>(OpType::CX, {0, 1});
  circ.add_op<unsigned>(ClassicalX(), {1});
  circ.add_conditional_gate<unsigned>(OpType::CZ, {}, {0, 1}, {0}, 1);
  circ.add_op<unsigned>(ClassicalCX(), {0, 1});
  circ.add_op<unsigned>(AndWithOp(), {2, 3});
  circ.add_conditional_gate<unsigned>(OpType::CX, {}, {1, 0}, {0, 1, 2}, 1);
  circ.add_conditional_gate<unsigned>(OpType::CX, {}, {1, 0}, {0, 1}, 1);
  circ.add_conditional_gate<unsigned>(OpType::CX, {}, {1, 0}, {0}, 1);
  circ.add_conditional_gate<unsigned>(OpType::CZ, {}, {1, 2}, {0, 1, 2}, 1);
  circ.add_conditional_gate<unsigned>(OpType::CZ, {}, {1, 2}, {0, 1}, 1);
  circ.add_conditional_gate<unsigned>(OpType::CZ, {}, {1, 2}, {0}, 1);
  circ.add_op<unsigned>(OpType::CX, {2, 0});
  circ.add_op<unsigned>(OpType::CX, {2, 1});
  circ.add_op<unsigned>(OpType::H, {0});
  circ.add_op<unsigned>(OpType::H, {1});
  circ.add_op<unsigned>(OpType::H, {2});
  circ.add_op<unsigned>(AndOp(), {2, 3, 0});
  circ.add_op<unsigned>(OrOp(), {0, 1, 2});
  circ.add_op<unsigned>(NotOp(), {2, 3});
  RingArch arc(3);
  CompilationUnit cu(circ);
  PassPtr r_p = gen_routing_pass(
      arc, {std::make_shared<LexiLabellingMethod>(),
            std::make_shared<LexiRouteRoutingMethod>()});
  REQUIRE(r_p->apply(cu));
}

SCENARIO(
    "Test adding ancilla Node, using as end of path swaps and then merging "
    "with unplaced Qubit.") {
  Node unplaced = Node("unplaced", 0);
  std::vector<Node> placed = {
      Node("opposite", 0), Node("opposite", 1), Node("opposite", 2),
      Node("opposite", 3), Node("opposite", 4)};
  std::vector<std::pair<Node, Node>> coupling = {
      {placed[0], placed[1]},
      {placed[1], placed[2]},
      {placed[2], placed[3]},
      {placed[3], placed[4]}};
  std::shared_ptr<Architecture> architecture =
      std::make_shared<Architecture>(coupling);
  Circuit circuit(4);
  std::vector<Qubit> qubits = {Qubit(0), Qubit(1), Qubit(2), Qubit(3)};

  circuit.add_op<unsigned>(OpType::CX, {3, 1});
  circuit.add_op<unsigned>(OpType::CX, {2, 0});
  circuit.add_op<unsigned>(OpType::CX, {2, 1});
  circuit.add_op<unsigned>(OpType::CX, {3, 0});
  circuit.add_op<unsigned>(OpType::CX, {3, 2});

  std::map<Qubit, Node> p_map = {
      {qubits[0], placed[0]},
      {qubits[1], placed[1]},
      {qubits[2], placed[2]},
      {qubits[3], unplaced}};
  Placement::place_with_map(circuit, p_map);

  MappingFrontier mapping_frontier(circuit);
  mapping_frontier.advance_frontier_boundary(architecture);
  // adds "placed[3]" as ancilla
  REQUIRE(mapping_frontier.add_swap(placed[2], placed[3]));
  // provokes path swap
  REQUIRE(!mapping_frontier.add_swap(placed[2], placed[3]));
  // merge into unassigned
  mapping_frontier.merge_ancilla(unplaced, placed[2]);
  mapping_frontier.circuit_.get_commands();
  REQUIRE(true);
}

SCENARIO(
    "Test case fails if the linear boundary in merge_ancilla replaces the "
    "ancilla entry before erasing the merge entry.") {
  SquareGrid architecture(5, 5);
  std::vector<Node> nodes = architecture.get_all_nodes_vec();
  Circuit circuit(10);
  circuit.add_op<unsigned>(OpType::CX, {0, 1});
  circuit.add_op<unsigned>(OpType::CX, {0, 3});
  circuit.add_op<unsigned>(OpType::CX, {1, 2});
  circuit.add_barrier({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
  circuit.add_op<unsigned>(OpType::CX, {6, 7});
  std::map<Qubit, Node> p_map = {
      // mapping for qbs with 2qb gates
      {Qubit(0), nodes[0]},  {Qubit(1), nodes[4]},  {Qubit(2), nodes[20]},
      {Qubit(3), nodes[24]}, {Qubit(4), nodes[11]}, {Qubit(5), nodes[17]},
  };
  Placement::place_with_map(circuit, p_map);
  CompilationUnit cu(circuit);
  PassPtr r_p = gen_routing_pass(
      architecture, {std::make_shared<LexiLabellingMethod>(),
                     std::make_shared<LexiRouteRoutingMethod>()});
  REQUIRE(r_p->apply(cu));
}

SCENARIO(
    "Test relabelling a Circuit UnitID that is an Architecture Node but "
    "reassignable to an Ancilla Node.") {
  /**
   * If a non-labelled Qubit in a Circuit being mapped has no Quantum gates with
   * physical constraints (i.e. mostly multi-qubit gates) then before mapping we
   * assign it some "bad" Architecture Node (typically something on the edge of
   * the coupling graph with low out-degree) Any non-labelled Qubit in a Circuit
   * with Quantum gates with physical constraints are left unlabelled During
   * mapping, if a multi-qubit gate with a non-labelled Qubit is encountered we
   * need to allocate it to some _best_ Architecture Node In some cases, this
   * _best_ Node may end up being a "bad" Node we've used to assign an
   * "unimportant" (without connectivity graph related physical constraints)
   * Qubit too. If this is the case we relabel the unlabelled Qubit to this
   * _best_ Node and find a new Node to assign the "unimportant" Qubit to
   * Ideally we just find a spare Architecture Node that hasn't previously been
   * assigned to and reassign the "unimportant" Qubit to it. However in some
   * cases there can be no spare Architecture Node as they have been used as
   * ancilla Node for SWAP/BRIDGE gates In this case we take an Ancilla Node and
   * wire its output to the input of the "unimportant" Qubit Path, essentially
   * reassigning it.
   *
   * All these tests should call "reassign_to_any_ancilla_node"
   */
  GIVEN("Line Architecture, one reassignment.") {
    std::vector<std::pair<unsigned, unsigned>> coupling_map = {
        {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 8}};
    Architecture architecture(coupling_map);
    std::vector<Node> nodes = architecture.get_all_nodes_vec();
    Circuit circuit(9);
    circuit.add_op<unsigned>(OpType::CX, {0, 1});
    for (unsigned i = 0; i < 9; i++) {
      circuit.add_op<unsigned>(OpType::H, {i});
    }
    circuit.add_barrier({0, 1, 2, 3, 4, 5, 6, 7});
    for (unsigned i = 0; i < 8; i++) {
      circuit.add_op<unsigned>(OpType::H, {i});
    }
    circuit.add_op<unsigned>(OpType::CX, {0, 3});
    circuit.add_op<unsigned>(OpType::CX, {0, 2});
    circuit.add_op<unsigned>(OpType::CX, {0, 4});

    std::map<Qubit, Node> p_map = {
        // mapping for qbs with 2qb gates
        {Qubit(0), nodes[0]},
        {Qubit(1), nodes[8]}};
    Placement::place_with_map(circuit, p_map);
    CompilationUnit cu(circuit);
    PassPtr r_p = gen_routing_pass(
        architecture, {std::make_shared<LexiLabellingMethod>(),
                       std::make_shared<LexiRouteRoutingMethod>()});
    REQUIRE(r_p->apply(cu));

    // these exact maps should imply "reassign_to_any_ancilla_node" has been
    // called
    auto init_map = cu.get_initial_map_ref();
    auto final_map = cu.get_final_map_ref();

    REQUIRE(init_map.left.find(Node(0))->second == Node(0));
    REQUIRE(init_map.left.find(Node(8))->second == Node(8));
    REQUIRE(init_map.left.find(Qubit(2))->second == Node(6));
    REQUIRE(init_map.left.find(Qubit(3))->second == Node(5));
    REQUIRE(init_map.left.find(Qubit(4))->second == Node(7));
    REQUIRE(init_map.left.find(Qubit(5))->second == Node(1));
    REQUIRE(init_map.left.find(Qubit(6))->second == Node(2));
    REQUIRE(init_map.left.find(Qubit(7))->second == Node(3));
    REQUIRE(init_map.left.find(Qubit(8))->second == Node(4));

    REQUIRE(final_map.left.find(Node(0))->second == Node(0));
    REQUIRE(final_map.left.find(Node(8))->second == Node(4));
    REQUIRE(final_map.left.find(Qubit(2))->second == Node(2));
    REQUIRE(final_map.left.find(Qubit(3))->second == Node(3));
    REQUIRE(final_map.left.find(Qubit(4))->second == Node(1));
    REQUIRE(final_map.left.find(Qubit(5))->second == Node(5));
    REQUIRE(final_map.left.find(Qubit(6))->second == Node(6));
    REQUIRE(final_map.left.find(Qubit(7))->second == Node(7));
    REQUIRE(final_map.left.find(Qubit(8))->second == Node(8));
  }

  GIVEN("Line Architecture, two reassignments.") {
    unsigned n_nodes = 20;
    std::vector<std::pair<unsigned, unsigned>> coupling_map;
    for (unsigned i = 0; i < n_nodes; i++) {
      coupling_map.push_back({i, i + 1});
    }
    Architecture architecture(coupling_map);
    std::vector<Node> nodes = architecture.get_all_nodes_vec();

    Circuit circuit(n_nodes);
    circuit.add_op<unsigned>(OpType::CX, {0, 2});
    circuit.add_op<unsigned>(OpType::CX, {2, 1});
    circuit.add_op<unsigned>(OpType::CX, {2, 3});
    for (unsigned i = 0; i < n_nodes; i++) {
      circuit.add_op<unsigned>(OpType::H, {i});
    }
    std::vector<unsigned> barrier_arg(n_nodes);
    std::iota(barrier_arg.begin(), barrier_arg.end(), 0);
    circuit.add_barrier(barrier_arg);
    for (unsigned i = 0; i < n_nodes; i++) {
      circuit.add_op<unsigned>(OpType::H, {i});
    }
    circuit.add_op<unsigned>(OpType::CX, {2, 3});
    circuit.add_op<unsigned>(OpType::CX, {2, 4});
    std::map<Qubit, Node> p_map = {
        // mapping for qbs with 2qb gates
        {Qubit(0), nodes[6]},
        {Qubit(1), nodes[n_nodes - 1]}};

    Placement::place_with_map(circuit, p_map);
    CompilationUnit cu(circuit);
    PassPtr r_p = gen_routing_pass(
        architecture, {std::make_shared<LexiLabellingMethod>(),
                       std::make_shared<LexiRouteRoutingMethod>()});
    REQUIRE(r_p->apply(cu));

    auto init_map = cu.get_initial_map_ref();
    auto final_map = cu.get_final_map_ref();

    REQUIRE(init_map.left.find(Node(6))->second == Node(6));
    REQUIRE(init_map.left.find(Node(19))->second == Node(19));
    REQUIRE(init_map.left.find(Qubit(2))->second == Node(5));
    REQUIRE(init_map.left.find(Qubit(3))->second == Node(4));
    REQUIRE(init_map.left.find(Qubit(4))->second == Node(3));
    REQUIRE(init_map.left.find(Qubit(5))->second == Node(0));
    REQUIRE(init_map.left.find(Qubit(6))->second == Node(1));
    REQUIRE(init_map.left.find(Qubit(7))->second == Node(2));
    REQUIRE(init_map.left.find(Qubit(8))->second == Node(17));
    REQUIRE(init_map.left.find(Qubit(9))->second == Node(16));
    REQUIRE(init_map.left.find(Qubit(10))->second == Node(15));
    REQUIRE(init_map.left.find(Qubit(11))->second == Node(7));
    REQUIRE(init_map.left.find(Qubit(12))->second == Node(8));
    REQUIRE(init_map.left.find(Qubit(13))->second == Node(9));
    REQUIRE(init_map.left.find(Qubit(14))->second == Node(10));
    REQUIRE(init_map.left.find(Qubit(15))->second == Node(11));
    REQUIRE(init_map.left.find(Qubit(16))->second == Node(12));
    REQUIRE(init_map.left.find(Qubit(17))->second == Node(13));
    REQUIRE(init_map.left.find(Qubit(18))->second == Node(14));
    REQUIRE(init_map.left.find(Qubit(19))->second == Node(20));
    REQUIRE(
        init_map.left.find(Qubit(q_routing_ancilla_reg(), 0))->second ==
        Node(18));

    REQUIRE(final_map.left.find(Node(6))->second == Node(7));
    REQUIRE(final_map.left.find(Node(19))->second == Node(6));
    REQUIRE(final_map.left.find(Qubit(2))->second == Node(4));
    REQUIRE(final_map.left.find(Qubit(3))->second == Node(5));
    REQUIRE(final_map.left.find(Qubit(4))->second == Node(3));
    REQUIRE(final_map.left.find(Qubit(5))->second == Node(0));
    REQUIRE(final_map.left.find(Qubit(6))->second == Node(1));
    REQUIRE(final_map.left.find(Qubit(7))->second == Node(2));
    REQUIRE(final_map.left.find(Qubit(8))->second == Node(18));
    REQUIRE(final_map.left.find(Qubit(9))->second == Node(17));
    REQUIRE(final_map.left.find(Qubit(10))->second == Node(16));
    REQUIRE(final_map.left.find(Qubit(11))->second == Node(8));
    REQUIRE(final_map.left.find(Qubit(12))->second == Node(9));
    REQUIRE(final_map.left.find(Qubit(13))->second == Node(10));
    REQUIRE(final_map.left.find(Qubit(14))->second == Node(11));
    REQUIRE(final_map.left.find(Qubit(15))->second == Node(12));
    REQUIRE(final_map.left.find(Qubit(16))->second == Node(13));
    REQUIRE(final_map.left.find(Qubit(17))->second == Node(14));
    REQUIRE(final_map.left.find(Qubit(18))->second == Node(15));
    REQUIRE(final_map.left.find(Qubit(19))->second == Node(20));
    REQUIRE(
        final_map.left.find(Qubit(q_routing_ancilla_reg(), 0))->second ==
        Node(19));
    unit_bimaps_t maps = {init_map, final_map};
    REQUIRE(check_permutation(
        cu.get_circ_ref(), std::make_shared<unit_bimaps_t>(maps)));
  }
  GIVEN("Line Architecture, two reassignments, more gates.") {
    unsigned n_nodes = 30;
    std::vector<std::pair<unsigned, unsigned>> coupling_map;
    for (unsigned i = 0; i < n_nodes; i++) {
      coupling_map.push_back({i, i + 1});
    }
    Architecture architecture(coupling_map);
    std::vector<Node> nodes = architecture.get_all_nodes_vec();

    Circuit circuit(n_nodes);
    circuit.add_op<unsigned>(OpType::CZ, {3, 4});
    circuit.add_op<unsigned>(OpType::CZ, {4, 10});
    circuit.add_op<unsigned>(OpType::CZ, {4, 5});
    circuit.add_op<unsigned>(OpType::CZ, {5, 6});
    for (unsigned i = 0; i < n_nodes; i++) {
      circuit.add_op<unsigned>(OpType::H, {i});
    }
    std::vector<unsigned> barrier_arg(n_nodes);
    std::iota(barrier_arg.begin(), barrier_arg.end(), 0);
    circuit.add_barrier(barrier_arg);
    for (unsigned i = 0; i < n_nodes; i++) {
      circuit.add_op<unsigned>(OpType::H, {i});
    }
    circuit.add_op<unsigned>(OpType::CX, {5, 3});
    circuit.add_op<unsigned>(OpType::CX, {5, 4});
    circuit.add_barrier(barrier_arg);
    for (unsigned i = 0; i < n_nodes; i++) {
      circuit.add_op<unsigned>(OpType::H, {i});
    }
    circuit.add_op<unsigned>(OpType::CZ, {3, 4});
    circuit.add_op<unsigned>(OpType::CZ, {4, 10});

    std::map<Qubit, Node> p_map = {
        // mapping for qbs with 2qb gates
        {Qubit(3), nodes[14]},
        {Qubit(10), nodes[n_nodes - 1]}};

    Placement::place_with_map(circuit, p_map);
    CompilationUnit cu(circuit);
    PassPtr r_p = gen_routing_pass(
        architecture, {std::make_shared<LexiLabellingMethod>(),
                       std::make_shared<LexiRouteRoutingMethod>()});

    REQUIRE(r_p->apply(cu));

    auto init_map = cu.get_initial_map_ref();
    auto final_map = cu.get_final_map_ref();

    REQUIRE(init_map.left.find(Node(14))->second == Node(14));
    REQUIRE(init_map.left.find(Node(29))->second == Node(29));
    REQUIRE(init_map.left.find(Qubit(0))->second == Node(0));
    REQUIRE(init_map.left.find(Qubit(1))->second == Node(1));
    REQUIRE(init_map.left.find(Qubit(2))->second == Node(2));
    REQUIRE(init_map.left.find(Qubit(4))->second == Node(13));
    REQUIRE(init_map.left.find(Qubit(5))->second == Node(12));
    REQUIRE(init_map.left.find(Qubit(6))->second == Node(11));
    REQUIRE(init_map.left.find(Qubit(7))->second == Node(3));
    REQUIRE(init_map.left.find(Qubit(8))->second == Node(4));
    REQUIRE(init_map.left.find(Qubit(9))->second == Node(5));
    REQUIRE(init_map.left.find(Qubit(11))->second == Node(6));
    REQUIRE(init_map.left.find(Qubit(12))->second == Node(7));
    REQUIRE(init_map.left.find(Qubit(13))->second == Node(8));
    REQUIRE(init_map.left.find(Qubit(14))->second == Node(9));
    REQUIRE(init_map.left.find(Qubit(15))->second == Node(10));
    REQUIRE(init_map.left.find(Qubit(16))->second == Node(27));
    REQUIRE(init_map.left.find(Qubit(17))->second == Node(26));
    REQUIRE(init_map.left.find(Qubit(18))->second == Node(25));
    REQUIRE(init_map.left.find(Qubit(19))->second == Node(15));
    REQUIRE(init_map.left.find(Qubit(20))->second == Node(16));
    REQUIRE(init_map.left.find(Qubit(21))->second == Node(17));
    REQUIRE(init_map.left.find(Qubit(22))->second == Node(18));
    REQUIRE(init_map.left.find(Qubit(23))->second == Node(19));
    REQUIRE(init_map.left.find(Qubit(24))->second == Node(20));
    REQUIRE(init_map.left.find(Qubit(25))->second == Node(21));
    REQUIRE(init_map.left.find(Qubit(26))->second == Node(22));
    REQUIRE(init_map.left.find(Qubit(27))->second == Node(23));
    REQUIRE(init_map.left.find(Qubit(28))->second == Node(24));
    REQUIRE(init_map.left.find(Qubit(29))->second == Node(30));
    REQUIRE(
        init_map.left.find(Qubit(q_routing_ancilla_reg(), 0))->second ==
        Node(28));

    REQUIRE(final_map.left.find(Node(14))->second == Node(15));
    REQUIRE(final_map.left.find(Node(29))->second == Node(14));
    REQUIRE(final_map.left.find(Qubit(0))->second == Node(0));
    REQUIRE(final_map.left.find(Qubit(1))->second == Node(1));
    REQUIRE(final_map.left.find(Qubit(2))->second == Node(2));
    REQUIRE(final_map.left.find(Qubit(4))->second == Node(13));
    REQUIRE(final_map.left.find(Qubit(5))->second == Node(12));
    REQUIRE(final_map.left.find(Qubit(6))->second == Node(11));
    REQUIRE(final_map.left.find(Qubit(7))->second == Node(3));
    REQUIRE(final_map.left.find(Qubit(8))->second == Node(4));
    REQUIRE(final_map.left.find(Qubit(9))->second == Node(5));
    REQUIRE(final_map.left.find(Qubit(11))->second == Node(6));
    REQUIRE(final_map.left.find(Qubit(12))->second == Node(7));
    REQUIRE(final_map.left.find(Qubit(13))->second == Node(8));
    REQUIRE(final_map.left.find(Qubit(14))->second == Node(9));
    REQUIRE(final_map.left.find(Qubit(15))->second == Node(10));
    REQUIRE(final_map.left.find(Qubit(16))->second == Node(28));
    REQUIRE(final_map.left.find(Qubit(17))->second == Node(27));
    REQUIRE(final_map.left.find(Qubit(18))->second == Node(26));
    REQUIRE(final_map.left.find(Qubit(19))->second == Node(16));
    REQUIRE(final_map.left.find(Qubit(20))->second == Node(17));
    REQUIRE(final_map.left.find(Qubit(21))->second == Node(18));
    REQUIRE(final_map.left.find(Qubit(22))->second == Node(19));
    REQUIRE(final_map.left.find(Qubit(23))->second == Node(20));
    REQUIRE(final_map.left.find(Qubit(24))->second == Node(21));
    REQUIRE(final_map.left.find(Qubit(25))->second == Node(22));
    REQUIRE(final_map.left.find(Qubit(26))->second == Node(23));
    REQUIRE(final_map.left.find(Qubit(27))->second == Node(24));
    REQUIRE(final_map.left.find(Qubit(28))->second == Node(25));
    REQUIRE(final_map.left.find(Qubit(29))->second == Node(30));
    REQUIRE(
        final_map.left.find(Qubit(q_routing_ancilla_reg(), 0))->second ==
        Node(29));
    unit_bimaps_t maps = {init_map, final_map};
    REQUIRE(check_permutation(
        cu.get_circ_ref(), std::make_shared<unit_bimaps_t>(maps)));
  }

  GIVEN(
      "Line architecture, reassigned Nodes both at end of Circuit so extra "
      "logic required.") {
    std::vector<std::pair<unsigned, unsigned>> coupling_map;
    for (unsigned i = 0; i < 15; i++) {
      coupling_map.push_back({i, i + 1});
    }
    // coupling_map.push_back({15,0});
    Architecture architecture(coupling_map);
    std::vector<Node> nodes = architecture.get_all_nodes_vec();
    Circuit circuit(15);
    circuit.add_op<unsigned>(OpType::CX, {0, 1});
    circuit.add_op<unsigned>(OpType::CX, {0, 2});
    circuit.add_op<unsigned>(OpType::CX, {0, 3});
    circuit.add_op<unsigned>(OpType::CX, {0, 4});
    circuit.add_op<unsigned>(OpType::CX, {0, 5});
    circuit.add_op<unsigned>(OpType::CX, {1, 2});
    circuit.add_op<unsigned>(OpType::CX, {1, 3});
    circuit.add_op<unsigned>(OpType::CX, {1, 4});
    circuit.add_op<unsigned>(OpType::CX, {1, 5});
    circuit.add_op<unsigned>(OpType::CX, {2, 3});
    circuit.add_op<unsigned>(OpType::CX, {2, 4});
    circuit.add_op<unsigned>(OpType::CX, {2, 5});
    circuit.add_op<unsigned>(OpType::CX, {3, 4});
    circuit.add_op<unsigned>(OpType::CX, {3, 5});
    circuit.add_op<unsigned>(OpType::CX, {4, 5});
    for (unsigned i = 0; i < 15; i++) {
      circuit.add_op<unsigned>(OpType::H, {i});
    }
    std::vector<unsigned> barrier_indices(15);
    std::iota(barrier_indices.begin(), barrier_indices.end(), 0);
    circuit.add_barrier(barrier_indices);
    for (unsigned i = 0; i < 15; i++) {
      circuit.add_op<unsigned>(OpType::H, {i});
    }
    circuit.add_op<unsigned>(OpType::CX, {6, 7});
    circuit.add_op<unsigned>(OpType::CX, {6, 2});
    circuit.add_op<unsigned>(OpType::CX, {6, 5});
    circuit.add_op<unsigned>(OpType::CX, {7, 0});
    circuit.add_op<unsigned>(OpType::CX, {7, 1});
    circuit.add_op<unsigned>(OpType::CX, {7, 4});
    circuit.add_op<unsigned>(OpType::CX, {7, 5});
    std::map<Qubit, Node> p_map = {
        // mapping for qbs with 2qb gates
        {Qubit(0), nodes[1]},  {Qubit(1), nodes[7]}, {Qubit(2), nodes[13]},
        {Qubit(3), nodes[15]}, {Qubit(4), nodes[8]}, {Qubit(5), nodes[10]},
        // // mapping for 1qb qubits
    };
    Placement::place_with_map(circuit, p_map);
    CompilationUnit cu(circuit);
    PassPtr r_p = gen_routing_pass(
        architecture, {std::make_shared<LexiLabellingMethod>(),
                       std::make_shared<LexiRouteRoutingMethod>()});
    REQUIRE(r_p->apply(cu));

    auto init_map = cu.get_initial_map_ref();
    auto final_map = cu.get_final_map_ref();

    REQUIRE(init_map.left.find(Node(1))->second == Node(1));
    REQUIRE(init_map.left.find(Node(7))->second == Node(7));
    REQUIRE(init_map.left.find(Node(8))->second == Node(8));
    REQUIRE(init_map.left.find(Node(10))->second == Node(10));
    REQUIRE(init_map.left.find(Node(13))->second == Node(13));
    REQUIRE(init_map.left.find(Node(15))->second == Node(15));
    REQUIRE(init_map.left.find(Qubit(6))->second == Node(0));
    REQUIRE(init_map.left.find(Qubit(7))->second == Node(5));
    REQUIRE(init_map.left.find(Qubit(8))->second == Node(4));
    REQUIRE(init_map.left.find(Qubit(9))->second == Node(2));
    REQUIRE(init_map.left.find(Qubit(10))->second == Node(3));
    REQUIRE(init_map.left.find(Qubit(11))->second == Node(9));
    REQUIRE(init_map.left.find(Qubit(12))->second == Node(11));
    REQUIRE(init_map.left.find(Qubit(13))->second == Node(12));
    REQUIRE(init_map.left.find(Qubit(14))->second == Node(14));
    REQUIRE(
        init_map.left.find(Qubit(q_routing_ancilla_reg(), 2))->second ==
        Node(6));

    REQUIRE(final_map.left.find(Node(1))->second == Node(5));
    REQUIRE(final_map.left.find(Node(7))->second == Node(7));
    REQUIRE(final_map.left.find(Node(8))->second == Node(10));
    REQUIRE(final_map.left.find(Node(10))->second == Node(9));
    REQUIRE(final_map.left.find(Node(13))->second == Node(6));
    REQUIRE(final_map.left.find(Node(15))->second == Node(12));
    REQUIRE(final_map.left.find(Qubit(6))->second == Node(11));
    REQUIRE(final_map.left.find(Qubit(7))->second == Node(8));
    REQUIRE(final_map.left.find(Qubit(8))->second == Node(2));
    REQUIRE(final_map.left.find(Qubit(9))->second == Node(0));
    REQUIRE(final_map.left.find(Qubit(10))->second == Node(1));
    REQUIRE(final_map.left.find(Qubit(11))->second == Node(4));
    REQUIRE(final_map.left.find(Qubit(12))->second == Node(13));
    REQUIRE(final_map.left.find(Qubit(13))->second == Node(14));
    REQUIRE(final_map.left.find(Qubit(14))->second == Node(15));
    REQUIRE(
        final_map.left.find(Qubit(q_routing_ancilla_reg(), 2))->second ==
        Node(3));
    unit_bimaps_t maps = {init_map, final_map};
    REQUIRE(check_permutation(
        cu.get_circ_ref(), std::make_shared<unit_bimaps_t>(maps)));
  }
  GIVEN("Known failing case from json file, 14 qubit architecture.") {
    std::vector<std::pair<unsigned, unsigned>> coupling_map = {
        {1, 0},  {1, 2},   {2, 3},   {4, 3},  {4, 10}, {5, 4},
        {5, 6},  {5, 9},   {6, 8},   {7, 8},  {9, 8},  {9, 10},
        {11, 3}, {11, 10}, {11, 12}, {12, 2}, {13, 1}, {13, 12}};
    Architecture architecture(coupling_map);
    std::ifstream circuit_file("lexiroute_circuit_relabel_to_ancilla.json");
    nlohmann::json j = nlohmann::json::parse(circuit_file);
    auto c = j.get<Circuit>();
    std::map<Qubit, Node> p_map = {
        {Qubit(0), Node("unplaced", 0)},
        {Qubit(1), Node("unplaced", 1)},
        {Qubit(2), Node("unplaced", 2)},
        {Qubit(3), Node(10)},
        {Qubit(4), Node(4)},
        {Qubit(5), Node(3)},
        {Qubit(6), Node("unplaced", 3)},
        {Qubit(7), Node("unplaced", 4)},
        {Qubit(8), Node("unplaced", 5)},
        {Qubit(9), Node("unplaced", 6)},
        {Qubit(10), Node(11)},
        {Qubit(11), Node("unplaced", 7)},
        {Qubit(12), Node("unplaced", 8)},
        {Qubit(13), Node("unplaced", 9)}};

    Placement::place_with_map(c, p_map);
    CompilationUnit cu(c);
    PassPtr r_p = gen_routing_pass(
        architecture, {std::make_shared<LexiLabellingMethod>(),
                       std::make_shared<LexiRouteRoutingMethod>()});
    REQUIRE(r_p->apply(cu));

    auto init_map = cu.get_initial_map_ref();
    auto final_map = cu.get_final_map_ref();
    REQUIRE(init_map.left.find(Node("c0", 0))->second == Node("c0", 0));
    REQUIRE(init_map.left.find(Node("c0", 1))->second == Node("c0", 1));
    REQUIRE(init_map.left.find(Node("c0", 2))->second == Node("c0", 2));
    REQUIRE(init_map.left.find(Node(3))->second == Node(3));
    REQUIRE(init_map.left.find(Node(4))->second == Node(4));
    REQUIRE(init_map.left.find(Node(10))->second == Node(10));
    REQUIRE(init_map.left.find(Node(11))->second == Node(11));
    REQUIRE(init_map.left.find(Node("unplaced", 0))->second == Node(0));
    REQUIRE(init_map.left.find(Node("unplaced", 1))->second == Node(1));
    REQUIRE(init_map.left.find(Node("unplaced", 2))->second == Node(5));
    REQUIRE(init_map.left.find(Node("unplaced", 3))->second == Node(2));
    REQUIRE(init_map.left.find(Node("unplaced", 4))->second == Node(6));
    REQUIRE(init_map.left.find(Node("unplaced", 5))->second == Node(7));
    REQUIRE(init_map.left.find(Node("unplaced", 6))->second == Node(8));
    REQUIRE(init_map.left.find(Node("unplaced", 7))->second == Node(12));
    REQUIRE(init_map.left.find(Node("unplaced", 8))->second == Node(9));
    REQUIRE(init_map.left.find(Node("unplaced", 9))->second == Node(13));

    REQUIRE(final_map.left.find(Node("c0", 0))->second == Node("c0", 0));
    REQUIRE(final_map.left.find(Node("c0", 1))->second == Node("c0", 1));
    REQUIRE(final_map.left.find(Node("c0", 2))->second == Node("c0", 2));
    REQUIRE(final_map.left.find(Node(3))->second == Node(10));
    REQUIRE(final_map.left.find(Node(4))->second == Node(11));
    REQUIRE(final_map.left.find(Node(10))->second == Node(12));
    REQUIRE(final_map.left.find(Node(11))->second == Node(3));
    REQUIRE(final_map.left.find(Node("unplaced", 0))->second == Node(0));
    REQUIRE(final_map.left.find(Node("unplaced", 1))->second == Node(1));
    REQUIRE(final_map.left.find(Node("unplaced", 2))->second == Node(9));
    REQUIRE(final_map.left.find(Node("unplaced", 3))->second == Node(2));
    REQUIRE(final_map.left.find(Node("unplaced", 4))->second == Node(6));
    REQUIRE(final_map.left.find(Node("unplaced", 5))->second == Node(7));
    REQUIRE(final_map.left.find(Node("unplaced", 6))->second == Node(8));
    REQUIRE(final_map.left.find(Node("unplaced", 7))->second == Node(4));
    REQUIRE(final_map.left.find(Node("unplaced", 8))->second == Node(5));
    REQUIRE(final_map.left.find(Node("unplaced", 9))->second == Node(13));
    unit_bimaps_t maps = {init_map, final_map};
    REQUIRE(check_permutation(
        cu.get_circ_ref(), std::make_shared<unit_bimaps_t>(maps)));
  }
  GIVEN("Twenty qubit Circuit, Twenty Seven Qubit architecture.") {
    Circuit circuit(20);
    for (unsigned i = 0; i < 20; i++) {
      circuit.add_op<unsigned>(OpType::H, {0});
    }
    circuit.add_op<unsigned>(OpType::CZ, {0, 1});
    circuit.add_op<unsigned>(OpType::CZ, {2, 3});
    circuit.add_op<unsigned>(OpType::CZ, {4, 5});
    circuit.add_op<unsigned>(OpType::CZ, {6, 7});
    circuit.add_op<unsigned>(OpType::CZ, {8, 9});
    circuit.add_op<unsigned>(OpType::CZ, {0, 16});
    circuit.add_op<unsigned>(OpType::CZ, {1, 18});
    circuit.add_op<unsigned>(OpType::CZ, {2, 12});
    circuit.add_op<unsigned>(OpType::CZ, {3, 10});
    circuit.add_op<unsigned>(OpType::CZ, {4, 19});
    circuit.add_op<unsigned>(OpType::CZ, {5, 13});
    circuit.add_op<unsigned>(OpType::CZ, {6, 15});
    circuit.add_op<unsigned>(OpType::CZ, {7, 11});
    circuit.add_op<unsigned>(OpType::CZ, {8, 16});
    circuit.add_op<unsigned>(OpType::CZ, {9, 10});
    circuit.add_op<unsigned>(OpType::CZ, {11, 15});
    circuit.add_op<unsigned>(OpType::CZ, {12, 14});
    circuit.add_op<unsigned>(OpType::CZ, {17, 18});
    circuit.add_op<unsigned>(OpType::CZ, {13, 14});
    circuit.add_op<unsigned>(OpType::CZ, {17, 19});

    std::vector<std::pair<unsigned, unsigned>> coupling_map = {
        {0, 1},   {1, 0},   {1, 2},   {1, 4},   {2, 1},   {2, 3},   {3, 2},
        {3, 5},   {4, 1},   {4, 7},   {5, 3},   {5, 8},   {6, 7},   {7, 4},
        {7, 6},   {7, 10},  {8, 5},   {8, 9},   {8, 11},  {9, 8},   {10, 7},
        {10, 12}, {11, 8},  {11, 14}, {12, 10}, {12, 13}, {12, 15}, {13, 12},
        {13, 14}, {14, 11}, {14, 13}, {14, 16}, {15, 12}, {15, 18}, {16, 14},
        {16, 19}, {17, 18}, {18, 15}, {18, 17}, {18, 21}, {19, 16}, {19, 20},
        {19, 22}, {20, 19}, {21, 18}, {21, 23}, {22, 19}, {22, 25}, {23, 21},
        {23, 24}, {24, 23}, {24, 25}, {25, 22}, {25, 24}, {25, 26}, {26, 25}};
    Architecture architecture(coupling_map);

    CompilationUnit cu(circuit);
    PassPtr r_p = gen_routing_pass(
        architecture, {std::make_shared<LexiLabellingMethod>(),
                       std::make_shared<LexiRouteRoutingMethod>()});
    REQUIRE(r_p->apply(cu));

    auto init_map = cu.get_initial_map_ref();
    auto final_map = cu.get_final_map_ref();

    REQUIRE(init_map.left.find(Qubit(0))->second == Node(1));
    REQUIRE(init_map.left.find(Qubit(1))->second == Node(0));
    REQUIRE(init_map.left.find(Qubit(2))->second == Node(2));
    REQUIRE(init_map.left.find(Qubit(3))->second == Node(3));
    REQUIRE(init_map.left.find(Qubit(4))->second == Node(4));
    REQUIRE(init_map.left.find(Qubit(5))->second == Node(7));
    REQUIRE(init_map.left.find(Qubit(6))->second == Node(5));
    REQUIRE(init_map.left.find(Qubit(7))->second == Node(8));
    REQUIRE(init_map.left.find(Qubit(8))->second == Node(10));
    REQUIRE(init_map.left.find(Qubit(9))->second == Node(12));
    REQUIRE(init_map.left.find(Qubit(10))->second == Node(14));
    REQUIRE(init_map.left.find(Qubit(11))->second == Node(19));
    REQUIRE(init_map.left.find(Qubit(12))->second == Node(9));
    REQUIRE(init_map.left.find(Qubit(13))->second == Node(15));
    REQUIRE(init_map.left.find(Qubit(14))->second == Node(22));
    REQUIRE(init_map.left.find(Qubit(15))->second == Node(16));
    REQUIRE(init_map.left.find(Qubit(16))->second == Node(11));
    REQUIRE(init_map.left.find(Qubit(17))->second == Node(18));
    REQUIRE(init_map.left.find(Qubit(18))->second == Node(6));
    REQUIRE(init_map.left.find(Qubit(19))->second == Node(13));

    REQUIRE(final_map.left.find(Qubit(0))->second == Node(0));
    REQUIRE(final_map.left.find(Qubit(1))->second == Node(4));
    REQUIRE(final_map.left.find(Qubit(2))->second == Node(3));
    REQUIRE(final_map.left.find(Qubit(3))->second == Node(5));
    REQUIRE(final_map.left.find(Qubit(4))->second == Node(6));
    REQUIRE(final_map.left.find(Qubit(5))->second == Node(18));
    REQUIRE(final_map.left.find(Qubit(6))->second == Node(9));
    REQUIRE(final_map.left.find(Qubit(7))->second == Node(22));
    REQUIRE(final_map.left.find(Qubit(8))->second == Node(1));
    REQUIRE(final_map.left.find(Qubit(9))->second == Node(16));
    REQUIRE(final_map.left.find(Qubit(10))->second == Node(8));
    REQUIRE(final_map.left.find(Qubit(11))->second == Node(19));
    REQUIRE(final_map.left.find(Qubit(12))->second == Node(11));
    REQUIRE(final_map.left.find(Qubit(13))->second == Node(15));
    REQUIRE(final_map.left.find(Qubit(14))->second == Node(13));
    REQUIRE(final_map.left.find(Qubit(15))->second == Node(14));
    REQUIRE(final_map.left.find(Qubit(16))->second == Node(2));
    REQUIRE(final_map.left.find(Qubit(17))->second == Node(10));
    REQUIRE(final_map.left.find(Qubit(18))->second == Node(7));
    REQUIRE(final_map.left.find(Qubit(19))->second == Node(12));
    unit_bimaps_t maps = {init_map, final_map};
    REQUIRE(check_permutation(
        cu.get_circ_ref(), std::make_shared<unit_bimaps_t>(maps)));
  }
}
SCENARIO("Lexi route produces incorrect bimaps") {
  // segfault Github #777
  std::ifstream arch_file("ibm_montreal.json");
  nlohmann::json j_arch = nlohmann::json::parse(arch_file);
  auto arch = j_arch.get<Architecture>();
  std::ifstream circ_file("bug777_circuit.json");
  nlohmann::json j_circ = nlohmann::json::parse(circ_file);
  auto circ = j_circ.get<Circuit>();
  std::map<Qubit, Node> p_map = {
      {Node(0), Node(5)},
      {Node(1), Node(8)},
      {Node(2), Node("unplaced", 0)},
      {Node(3), Node(16)},
      {Node(4), Node(3)},
      {Node(5), Node("unplaced", 1)},
      {Node(6), Node("unplaced", 2)},
      {Node(7), Node("unplaced", 3)},
      {Node(8), Node("unplaced", 4)},
      {Node(9), Node("unplaced", 5)},
      {Node(10), Node("unplaced", 6)},
      {Node(11), Node(25)},
      {Node(12), Node("unplaced", 7)},
      {Node(13), Node(14)},
      {Node(14), Node("unplaced", 8)},
      {Node(15), Node(19)},
      {Node(16), Node(24)},
      {Node(17), Node("unplaced", 9)},
      {Node(18), Node("unplaced", 10)},
      {Node(19), Node(2)},
      {Node(20), Node(1)},
      {Node(21), Node(22)},
      {Node(22), Node(11)},
      {Node(23), Node("unplaced", 11)},
      {Node(24), Node("unplaced", 12)},
      {Node(25), Node("unplaced", 13)},
      {Node(26), Node("unplaced", 14)}};
  MappingManager mm(std::make_shared<Architecture>(arch));
  std::shared_ptr<unit_bimaps_t> maps = std::make_shared<unit_bimaps_t>();
  for (auto it : p_map) {
    maps->initial.insert({it.first, it.second});
    maps->final.insert({it.first, it.second});
  }
  std::vector<RoutingMethodPtr> config = {
      std::make_shared<LexiLabellingMethod>(),
      std::make_shared<LexiRouteRoutingMethod>()};
  REQUIRE(mm.route_circuit_with_maps(circ, config, maps));
  REQUIRE(check_permutation(circ, maps));
}
SCENARIO("Lexi route with ClExprOps in the lookahead slice") {
  // segfault Github #2180: advance_next_2qb_slice read the quantum in-edge of
  // a purely classical vertex, which has none.
  Circuit circ(3, 3);
  circ.add_op<unsigned>(OpType::CX, {0, 1});
  circ.add_op<unsigned>(OpType::CX, {2, 1});
  circ.add_conditional_gate<unsigned>(OpType::H, {}, {2}, {2}, 1);
  ClExpr bit_or(ClOp::BitOr, {ClBitVar{0}, ClBitVar{1}});
  circ.add_op<unsigned>(
      std::make_shared<const ClExprOp>(
          WiredClExpr(bit_or, {{0, 0}, {1, 1}}, {}, {2})),
      {1, 2, 0});
  ClExpr bit_and(ClOp::BitAnd, {ClBitVar{0}, ClBitVar{1}});
  circ.add_op<unsigned>(
      std::make_shared<const ClExprOp>(
          WiredClExpr(bit_and, {{0, 0}, {1, 1}}, {}, {2})),
      {1, 0, 2});
  circ.add_op<unsigned>(OpType::CX, {0, 2});
  circ.add_op<unsigned>(OpType::CX, {0, 2});
  Architecture arc({{Node(0), Node(1)}, {Node(1), Node(2)}});
  MappingManager mm(std::make_shared<Architecture>(arc));
  std::vector<RoutingMethodPtr> config = {
      std::make_shared<LexiLabellingMethod>(),
      std::make_shared<LexiRouteRoutingMethod>()};
  REQUIRE(mm.route_circuit(circ, config));
  REQUIRE(respects_connectivity_constraints(circ, arc, false));
}
}  // namespace tket
