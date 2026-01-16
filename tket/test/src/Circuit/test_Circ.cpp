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

#include <boost/graph/graph_traits.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <sstream>
#include <unsupported/Eigen/MatrixFunctions>
#include <vector>

#include "../testutil.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/DAGDefs.hpp"
#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Circuit/Slices.hpp"
#include "tket/Gate/GatePtr.hpp"
#include "tket/Gate/OpPtrFunctions.hpp"
#include "tket/OpType/EdgeType.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/OpType/OpTypeFunctions.hpp"
#include "tket/Ops/ClassicalOps.hpp"
#include "tket/Ops/OpPtr.hpp"
#include "tket/Transformations/Decomposition.hpp"
#include "tket/Transformations/OptimisationPass.hpp"
#include "tket/Transformations/Replacement.hpp"
#include "tket/Transformations/Transform.hpp"
#include "tket/Utils/UnitID.hpp"

namespace tket {
namespace test_Circ {

static std::pair<Op_ptr, Expr> op_to_tk1(const Op_ptr& op) {
  std::vector<Expr> angles = as_gate_ptr(op)->get_tk1_angles();
  return {
      get_op_ptr(OpType::TK1, {angles[0], angles[1], angles[2]}), angles[3]};
}

SCENARIO(
    "Check that a Circuit with no edges can be constructed", "[edgeless]") {
  GIVEN("n vertices") {
    Circuit no_edges;
    unsigned n = 3;
    for (unsigned i = 0; i < n; ++i) {
      no_edges.add_vertex(OpType::H);
    }
    REQUIRE(no_edges.n_vertices() == n);
  }
}

SCENARIO("Check that a Circuit cannot have 2 registers with the same name") {
  GIVEN("Duplication") {
    Circuit circ;
    circ.add_q_register("duplicate", 4);
    REQUIRE_THROWS_AS(circ.add_q_register("duplicate", 4), CircuitInvalidity);
  }
  GIVEN("Duplication - classic") {
    Circuit circ;
    circ.add_c_register("duplicate", 4);
    REQUIRE_THROWS_AS(circ.add_c_register("duplicate", 4), CircuitInvalidity);
  }
  GIVEN("Duplication - q - c") {
    Circuit circ;
    circ.add_q_register("duplicate", 4);
    REQUIRE_THROWS_AS(circ.add_c_register("duplicate", 4), CircuitInvalidity);
  }
  GIVEN("Duplication - c - q") {
    Circuit circ;
    circ.add_c_register("duplicate", 4);
    REQUIRE_THROWS_AS(circ.add_q_register("duplicate", 4), CircuitInvalidity);
  }
  GIVEN("Check default registers do nothing weird") {
    Circuit circ(1);
    circ.add_blank_wires(3);
    REQUIRE(circ.default_regs_ok());
    REQUIRE(circ.is_simple());
    REQUIRE(circ.boundary.size() == 4);
    WHEN("Add default c reg name as q reg") {
      REQUIRE_NOTHROW(circ.add_q_register("c", 3));
    }
  }
}

SCENARIO(
    "Check that simple 1-qubit Circuits can be constructed properly using "
    "add_vertex etc",
    "[1-qubit],[add_circ]") {
  GIVEN("A sequence of X,Y,Z-gates") {
    Circuit simple;
    Vertex a = simple.add_vertex(OpType::Input);
    Vertex b = simple.add_vertex(OpType::X);
    Vertex c = simple.add_vertex(OpType::Z);
    Vertex d = simple.add_vertex(OpType::Z);
    Vertex e = simple.add_vertex(OpType::Output);
    simple.boundary.insert({Qubit(q_default_reg()), a, e});
    simple.add_edge({a, 0}, {b, 0}, EdgeType::Quantum);
    simple.add_edge({b, 0}, {c, 0}, EdgeType::Quantum);
    simple.add_edge({c, 0}, {d, 0}, EdgeType::Quantum);
    simple.add_edge({d, 0}, {e, 0}, EdgeType::Quantum);
    GIVEN("Get slices from circuit") {
      SliceVec slices = simple.get_slices();
      REQUIRE(slices.size() == 3);
      for (SliceVec::iterator i = slices.begin(); i != slices.end(); ++i) {
        REQUIRE((*i).size() == 1);
      }
    }
  }
  GIVEN("A circuit of In->Out edges") {
    Circuit new_circ(4);
    SliceVec slices = new_circ.get_slices();
    REQUIRE(slices.size() == 0);
  }
  GIVEN("A badly-formed vertex") {
    Circuit circ(2);
    REQUIRE_THROWS(circ.add_op<unsigned>(OpType::H, {}));
    REQUIRE_THROWS(circ.add_op<unsigned>(OpType::H, {0, 1}));
  }
  GIVEN("A badly-formed vertex - wasm") {
    Circuit circ(2);
    REQUIRE_THROWS(circ.add_op<unsigned>(OpType::WASM, {}));
    REQUIRE_THROWS(circ.add_op<unsigned>(OpType::WASM, {0, 1}));
  }
}

SCENARIO(
    "Check that simple 1-qubit Circuits can be constructed properly",
    "[1-qubit]") {
  GIVEN("A sequence of 3 H-gates") {
    Circuit test(1);
    add_1qb_gates(test, OpType::H, {0, 0, 0});
    REQUIRE(test.depth() == 3);
    REQUIRE(test.n_vertices() == 5);
    GIVEN("The addition of another gate using add_op") {
      std::vector<unsigned> qubs = {0};
      test.add_op<unsigned>(OpType::H, qubs);
      REQUIRE(test.depth() == 4);
      REQUIRE(test.n_vertices() == 6);
    }
    GIVEN("Get slices from the circuit") {
      SliceVec slices = test.get_slices();
      REQUIRE(slices.size() == 3);
      for (SliceVec::iterator i = slices.begin(); i != slices.end(); ++i) {
        REQUIRE((*i).size() == 1);
      }
    }
  }
  GIVEN("A Circuit to put an invalid command onto") {
    Circuit test(1);
    REQUIRE_THROWS(test.add_op<unsigned>(OpType::CX, {0, 0}));
  }
}

SCENARIO(
    "Check that a more complicated Circuit can be constructed properly",
    "[n-qubit]") {
  GIVEN("A series of H and CNOT gates") {
    Circuit test1(3);
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::CX, {0, 1});
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::H, {1});
    test1.add_op<unsigned>(OpType::CX, {0, 2});
    test1.add_op<unsigned>(OpType::CX, {2, 1});
    REQUIRE(test1.count_gates(OpType::CX) == 3);
    REQUIRE(!test1.is_symbolic());
  }
}

SCENARIO("test conditional count") {
  GIVEN("conditional circ") {
    Circuit circ;
    register_t qreg = circ.add_q_register("qb", 2);
    register_t creg = circ.add_c_register("b", 2);
    circ.add_conditional_gate<UnitID>(OpType::H, {}, {qreg[1]}, {creg[0]}, 1);
    circ.add_conditional_gate<UnitID>(OpType::H, {}, {qreg[1]}, {creg[0]}, 1);
    circ.add_conditional_gate<UnitID>(OpType::H, {}, {qreg[1]}, {creg[0]}, 1);
    circ.add_op<Qubit>(OpType::H, {Qubit(qreg[0])});
    circ.add_op<Qubit>(OpType::H, {Qubit(qreg[0])});
    REQUIRE(circ.n_qubits() == 2);
    REQUIRE(circ.n_bits() == 2);
    REQUIRE(circ.count_gates(OpType::CX, false) == 0);
    REQUIRE(circ.count_gates(OpType::H, false) == 2);
    REQUIRE(circ.count_gates(OpType::CX, true) == 0);
    REQUIRE(circ.count_gates(OpType::H, true) == 5);
  }
  GIVEN("A circuit with nested conditionals") {
    Circuit c0(1, 1);
    c0.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0}, 1);
    c0.add_op<unsigned>(OpType::X, {0});
    Op_ptr cbox_op = std::make_shared<CircBox>(c0);
    Op_ptr cond_cond_x = std::make_shared<Conditional>(cbox_op, 1, 1);
    Circuit circ(1, 2);
    circ.add_op<UnitID>(cond_cond_x, {Bit(0), Qubit(0), Bit(1)});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0}, 0);
    Transforms::decomp_boxes().apply(circ);
    REQUIRE(circ.n_qubits() == 1);
    REQUIRE(circ.n_bits() == 2);
    REQUIRE(circ.count_gates(OpType::X, false) == 1);
    REQUIRE(circ.count_gates(OpType::X, true) == 4);
  }
}

SCENARIO("Creating gates via Qubits and Registers") {
  GIVEN("A purely quantum circuit") {
    Circuit circ;
    register_t qreg = circ.add_q_register("a", 2);
    circ.add_op<Qubit>(OpType::H, {Qubit(qreg[0])});
    circ.add_op<Qubit>(OpType::CX, {Qubit(qreg[0]), Qubit(qreg[1])});
    register_t qreg2 = circ.add_q_register("b", 2);
    circ.add_op<Qubit>(OpType::CX, {Qubit(qreg[1]), Qubit(qreg2[1])});
    REQUIRE(circ.n_qubits() == 4);
    REQUIRE(circ.count_gates(OpType::CX) == 2);
    REQUIRE(circ.depth() == 3);
  }
  GIVEN("A mixed circuit") {
    Circuit circ;
    register_t qreg = circ.add_q_register("qb", 2);
    register_t creg = circ.add_c_register("b", 2);
    Vertex h = circ.add_conditional_gate<UnitID>(
        OpType::H, {}, {qreg[0]}, {creg[0]}, 1);
    Vertex m = circ.add_measure(Qubit(qreg[0]), Bit(creg[0]));
    circ.add_conditional_gate<UnitID>(OpType::Y, {}, {qreg[1]}, {creg[0]}, 1);
    Vertex m2 = circ.add_conditional_gate<UnitID>(
        OpType::Measure, {}, {qreg[1], creg[0]}, {creg[0], creg[1]}, 3);
    REQUIRE(circ.n_qubits() == 2);
    REQUIRE(circ.n_bits() == 2);
    REQUIRE(circ.depth() == 4);
    REQUIRE(circ.n_in_edges_of_type(h, EdgeType::Boolean) == 1);
    REQUIRE(circ.n_in_edges_of_type(m2, EdgeType::Boolean) == 2);
    REQUIRE(circ.n_out_edges_of_type(m, EdgeType::Boolean) == 2);
    REQUIRE(circ.n_in_edges_of_type(m, EdgeType::Classical) == 1);
    REQUIRE(circ.n_out_edges_of_type(m, EdgeType::Classical) == 1);
    REQUIRE(
        circ.n_in_edges_of_type(circ.get_out(creg[0]), EdgeType::Classical) ==
        1);
  }

  GIVEN("A new circuit - wasm") {
    Circuit circ;

    std::string funcname = "wasm_func_name";
    std::string wasm_file_uid = "wasm_file_hash";
    std::vector<unsigned> width_i_parameter = {1};
    std::vector<unsigned> width_o_parameter = {1};
    std::vector<unsigned> args = {0, 1};

    unsigned n_args = args.size();
    const std::shared_ptr<WASMOp> op = std::make_shared<WASMOp>(
        n_args, 1, width_i_parameter, width_o_parameter, funcname,
        wasm_file_uid);
  }
}

SCENARIO("Exception handling in get_(in/out)_edges") {
  GIVEN("A circuit with an unconnected input") {
    Circuit circ(2);
    Vertex cx = circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.remove_vertex(
        circ.q_inputs()[0], Circuit::GraphRewiring::No,
        Circuit::VertexDeletion::No);
    WHEN("Testing get_in_edges throws an exception") {
      REQUIRE_THROWS_AS(circ.get_in_edges(cx), CircuitInvalidity);
    }
    WHEN("Testing get_q_in_edges throws an exception") {
      REQUIRE_THROWS_AS(
          circ.get_in_edges_of_type(cx, EdgeType::Quantum), CircuitInvalidity);
    }
  }
  GIVEN("A circuit with too many inputs to a vertex") {
    Circuit circ(2);
    Vertex x = circ.add_op<unsigned>(OpType::X, {0});
    circ.add_edge({circ.q_inputs()[1], 0}, {x, 0}, EdgeType::Quantum);
    WHEN("Testing get_in_edges throws an exception") {
      REQUIRE_THROWS_AS(circ.get_in_edges(x), CircuitInvalidity);
    }
    WHEN("Testing get_q_in_edges throws an exception") {
      REQUIRE_THROWS_AS(
          circ.get_in_edges_of_type(x, EdgeType::Quantum), CircuitInvalidity);
    }
  }
  GIVEN("A circuit with too many outputs from a vertex") {
    Circuit circ(2);
    Vertex x = circ.add_op<unsigned>(OpType::X, {0});
    circ.add_edge({x, 0}, {circ.q_outputs()[1], 0}, EdgeType::Quantum);
    WHEN("Testing get_out_edges throws an exception") {
      REQUIRE_THROWS_AS(circ.get_all_out_edges(x), CircuitInvalidity);
    }
    WHEN("Testing get_q_out_edges throws an exception") {
      REQUIRE_THROWS_AS(
          circ.get_out_edges_of_type(x, EdgeType::Quantum), CircuitInvalidity);
    }
  }
}

SCENARIO("Verifying rearrange_qubit/classical_registers") {
  GIVEN("A simple circuit") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Y, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Qubit qb0(0);
    Qubit qb1(1);
    WHEN("Renaming entire register") {
      Qubit a0("a", 0);
      Qubit a1("a", 1);
      unit_map_t qubit_map = {{qb0, a0}, {qb1, a1}};
      circ.rename_units(qubit_map);
      REQUIRE(!circ.is_simple());
      REQUIRE(circ.boundary.size() == 2);
      REQUIRE_NOTHROW(circ.get_in(a0));
      REQUIRE_THROWS_AS(circ.get_in(qb0), CircuitInvalidity);
      qubit_vector_t correct = {a0, a1};
      REQUIRE(circ.all_qubits() == correct);
    }
    WHEN("Reordering register") {
      Vertex in0 = circ.get_in(qb0);
      unit_map_t qubit_map = {{qb0, qb1}, {qb1, qb0}};
      circ.rename_units(qubit_map);
      REQUIRE(circ.is_simple());
      REQUIRE(circ.boundary.size() == 2);
      REQUIRE(circ.get_in(qb1) == in0);
      qubit_vector_t correct = {qb0, qb1};
      REQUIRE(circ.all_qubits() == correct);
    }
    WHEN("Breaking register into two") {
      Qubit a("a");
      Qubit b("b");
      unit_map_t qubit_map = {{qb0, a}, {qb1, b}};
      circ.rename_units(qubit_map);
      REQUIRE(!circ.is_simple());
      REQUIRE(circ.boundary.size() == 2);
      qubit_vector_t correct = {a, b};
      REQUIRE(circ.all_qubits() == correct);
      REQUIRE_THROWS_AS(circ.get_in(qb0), CircuitInvalidity);
      THEN("Recombining ports") {
        qubit_map = {{a, qb0}, {b, qb1}};
        circ.rename_units(qubit_map);
        correct = {qb0, qb1};
        REQUIRE(circ.is_simple());
        REQUIRE(circ.all_qubits() == correct);
      }
    }
  }
  GIVEN("Same, but for classical registers") {
    Circuit circ(2, 2);
    circ.add_conditional_gate<unsigned>(OpType::Y, {}, {0}, {0, 1}, 0);
    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 1}, {1}, 1);
    Bit b0(0);
    Bit b1(1);
    WHEN("Renaming entire register") {
      Bit a0("a", 0);
      Bit a1("a", 1);
      unit_map_t bit_map = {{b0, a0}, {b1, a1}};
      circ.rename_units(bit_map);
      REQUIRE(!circ.is_simple());
      REQUIRE(circ.boundary.size() == 4);
      REQUIRE_NOTHROW(circ.get_in(a0));
      REQUIRE_THROWS_AS(circ.get_in(b0), CircuitInvalidity);
      bit_vector_t correct = {a0, a1};
      REQUIRE(circ.all_bits() == correct);
    }
    WHEN("Reordering register") {
      Vertex in0 = circ.get_in(b0);
      unit_map_t bit_map = {{b0, b1}, {b1, b0}};
      circ.rename_units(bit_map);
      REQUIRE(circ.is_simple());
      REQUIRE(circ.boundary.size() == 4);
      REQUIRE(circ.get_in(b1) == in0);
      bit_vector_t correct = {b0, b1};
      REQUIRE(circ.all_bits() == correct);
    }
    WHEN("Breaking register into two") {
      Bit a("a");
      Bit b("b");
      unit_map_t bit_map = {{b0, a}, {b1, b}};
      circ.rename_units(bit_map);
      REQUIRE(!circ.is_simple());
      REQUIRE(circ.boundary.size() == 4);
      bit_vector_t correct = {a, b};
      REQUIRE(circ.all_bits() == correct);
      REQUIRE_THROWS_AS(circ.get_in(b0), CircuitInvalidity);
      THEN("Recombining ports") {
        bit_map = {{a, b0}, {b, b1}};
        circ.rename_units(bit_map);
        correct = {b0, b1};
        REQUIRE(circ.is_simple());
        REQUIRE(circ.all_bits() == correct);
      }
    }
  }
}

SCENARIO("Exception testing for rearrange_qubit_registers") {
  GIVEN("A basic two qubit circuit") {
    Circuit circ(2, 2);
    circ.add_op<unsigned>(OpType::Y, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Qubit qb0(0);
    Qubit qb1(1);
    WHEN("Map uses a qubit multiple times") {
      Qubit a("a");
      unit_map_t qubit_map = {{qb0, a}, {qb1, a}};
      REQUIRE_THROWS_AS(circ.rename_units(qubit_map), CircuitInvalidity);
    }
    WHEN("Mapping unit to one that already exists") {
      unit_map_t qubit_map = {{qb0, qb1}};
      REQUIRE_THROWS_AS(circ.rename_units(qubit_map), CircuitInvalidity);
    }
    WHEN("Incompatible type with existing registers") {
      Qubit c0(c_default_reg(), 0);
      unit_map_t qubit_map = {{qb0, c0}};
      REQUIRE_THROWS_AS(circ.rename_units(qubit_map), CircuitInvalidity);
    }
    WHEN("Incompatible dimension with existing registers") {
      Qubit qb00(q_default_reg(), 0, 0);
      unit_map_t qubit_map = {{qb0, qb00}};
      REQUIRE_THROWS_AS(circ.rename_units(qubit_map), CircuitInvalidity);
    }
  }
}

SCENARIO("Verifying qubits_from_q_frontier") {
  Circuit test(2);
  Vertex h = test.add_op<unsigned>(OpType::H, {0});
  Vertex x = test.add_op<unsigned>(OpType::X, {1});
  Qubit q0(0);
  Qubit q1(1);
  GIVEN("A full frontier") {
    auto frontier = std::make_shared<unit_frontier_t>();
    frontier->insert({q0, test.get_nth_in_edge(test.get_out(q0), 0)});
    frontier->insert({q1, test.get_nth_in_edge(test.get_out(q1), 0)});
    unit_vector_t correct = {q0};
    REQUIRE(test.args_from_frontier(h, frontier, {}) == correct);
    correct = {q1};
    REQUIRE(test.args_from_frontier(x, frontier, {}) == correct);
  }
  GIVEN("A frontier without the specific vertex") {
    auto frontier = std::make_shared<unit_frontier_t>();
    frontier->insert({q1, test.get_nth_in_edge(test.get_out(q1), 0)});
    REQUIRE_THROWS_AS(
        test.args_from_frontier(h, frontier, {}), CircuitInvalidity);
  }
  GIVEN("An empty frontier") {
    auto empty = std::make_shared<unit_frontier_t>();
    REQUIRE_THROWS_AS(test.args_from_frontier(h, empty, {}), CircuitInvalidity);
  }
}

SCENARIO("Verifying controls_from_c_frontier") {
  Circuit test(2, 2);
  Vertex h =
      test.add_conditional_gate<unsigned>(OpType::H, {}, uvec{0}, {0}, 0);
  Vertex x = test.add_conditional_gate<unsigned>(OpType::X, {}, {1}, {0, 1}, 2);
  Qubit q0(0);
  Qubit q1(1);
  Bit b0(0);
  Bit b1(1);
  GIVEN("A full frontier") {
    auto frontier = std::make_shared<unit_frontier_t>();
    frontier->insert({q0, test.get_nth_in_edge(test.get_out(q0), 0)});
    frontier->insert({q1, test.get_nth_in_edge(test.get_out(q1), 0)});
    auto cfrontier = std::make_shared<b_frontier_t>();
    cfrontier->insert(
        {b0, test.get_out_edges_of_type(test.get_in(b0), EdgeType::Boolean)});
    cfrontier->insert(
        {b1, test.get_out_edges_of_type(test.get_in(b1), EdgeType::Boolean)});
    unit_vector_t correct = {b0, q0};
    REQUIRE(test.args_from_frontier(h, frontier, cfrontier) == correct);
    correct = {b0, b1, q1};
    REQUIRE(test.args_from_frontier(x, frontier, cfrontier) == correct);
  }
}

SCENARIO("Verifying bits_from_c_frontier") {
  Circuit test(2, 2);
  Vertex m = test.add_op<unsigned>(OpType::Measure, {0, 0});
  test.add_conditional_gate<unsigned>(OpType::H, {}, uvec{1}, {0}, 1);
  Qubit q0(0);
  Qubit q1(1);
  Bit b0(0);
  Bit b1(1);
  GIVEN("A full frontier") {
    auto frontier = std::make_shared<unit_frontier_t>();
    frontier->insert({q0, test.get_nth_in_edge(test.get_out(q0), 0)});
    frontier->insert({q1, test.get_nth_in_edge(test.get_out(q1), 0)});
    frontier->insert({b0, test.get_nth_out_edge(m, 1)});
    frontier->insert({b1, test.get_nth_out_edge(test.get_in(b1), 0)});
    unit_vector_t correct = {q0, b0};
    REQUIRE(test.args_from_frontier(m, frontier, {}) == correct);
  }
}

SCENARIO("Testing command_from_vertex") {
  Circuit test(2);
  Vertex cx = test.add_op<unsigned>(OpType::CX, {1, 0});
  Vertex h = test.add_op<unsigned>(OpType::H, {0});
  Vertex x = test.add_op<unsigned>(OpType::X, {1});
  Qubit q0(0);
  Qubit q1(1);
  WHEN("Calling with full frontier") {
    auto frontier = std::make_shared<unit_frontier_t>();
    frontier->insert({q0, test.get_nth_in_edge(test.get_out(q0), 0)});
    frontier->insert({q1, test.get_nth_in_edge(test.get_out(q1), 0)});
    Command com =
        test.command_from_vertex(h, frontier, std::make_shared<b_frontier_t>());
    REQUIRE(*com.get_op_ptr() == *get_op_ptr(OpType::H));
    unit_vector_t correct = {q0};
    REQUIRE(com.get_args() == correct);
    com =
        test.command_from_vertex(x, frontier, std::make_shared<b_frontier_t>());
    REQUIRE(*com.get_op_ptr() == *get_op_ptr(OpType::X));
    correct = {q1};
    REQUIRE(com.get_args() == correct);
  }
  WHEN("Checking multi-qubit gate") {
    auto frontier = std::make_shared<unit_frontier_t>();
    frontier->insert({q0, test.get_nth_out_edge(cx, 1)});
    frontier->insert({q1, test.get_nth_out_edge(cx, 0)});
    Command com = test.command_from_vertex(
        cx, frontier, std::make_shared<b_frontier_t>());
    REQUIRE(*com.get_op_ptr() == *get_op_ptr(OpType::CX));
    unit_vector_t correct = {q1, q0};
    REQUIRE(com.get_args() == correct);
  }
}

SCENARIO("Testing command_from_vertex with mixed circuits") {
  Circuit test(2, 2);
  Vertex h =
      test.add_conditional_gate<unsigned>(OpType::H, {}, uvec{0}, {0}, 0);
  Vertex m = test.add_measure(0, 0);
  Vertex x = test.add_conditional_gate<unsigned>(OpType::X, {}, {1}, {0, 1}, 3);
  Qubit q0(0);
  Qubit q1(1);
  Bit b0(0);
  Bit b1(1);
  WHEN("Checking a single control") {
    auto qf = std::make_shared<unit_frontier_t>();
    auto prev_cf = std::make_shared<b_frontier_t>();
    qf->insert({q0, test.get_nth_out_edge(h, 1)});
    qf->insert({q1, test.get_nth_in_edge(x, 2)});
    prev_cf->insert({b0, test.get_nth_b_out_bundle(test.get_in(b0), 0)});
    prev_cf->insert({b1, test.get_nth_b_out_bundle(test.get_in(b1), 0)});
    qf->insert({b0, test.get_nth_out_edge(test.get_in(b0), 0)});
    qf->insert({b1, test.get_nth_out_edge(test.get_in(b1), 0)});
    Command com = test.command_from_vertex(h, qf, prev_cf);
    unit_vector_t correct = {b0, q0};
    REQUIRE(com.get_args() == correct);
  }
  WHEN("Checking multiple controls") {
    auto qf = std::make_shared<unit_frontier_t>();
    auto prev_cf = std::make_shared<b_frontier_t>();
    qf->insert({q0, test.get_nth_out_edge(m, 0)});
    qf->insert({q1, test.get_nth_out_edge(x, 2)});
    prev_cf->insert({b0, test.get_nth_b_out_bundle(m, 1)});
    prev_cf->insert({b1, test.get_nth_b_out_bundle(test.get_in(b1), 0)});
    qf->insert({b0, test.get_nth_out_edge(m, 1)});
    qf->insert({b1, test.get_nth_out_edge(test.get_in(b1), 0)});
    Command com = test.command_from_vertex(x, qf, prev_cf);
    unit_vector_t correct = {b0, b1, q1};
    REQUIRE(com.get_args() == correct);
  }
  WHEN("Checking classical destinations") {
    auto qf = std::make_shared<unit_frontier_t>();
    auto prev_cf = std::make_shared<b_frontier_t>();
    qf->insert({q0, test.get_nth_out_edge(m, 0)});
    qf->insert({q1, test.get_nth_in_edge(x, 2)});
    prev_cf->insert({b0, test.get_nth_b_out_bundle(test.get_in(b0), 0)});
    prev_cf->insert({b1, test.get_nth_b_out_bundle(test.get_in(b1), 0)});
    qf->insert({b0, test.get_nth_out_edge(m, 1)});
    qf->insert({b1, test.get_nth_out_edge(test.get_in(b1), 0)});
    Command com = test.command_from_vertex(m, qf, prev_cf);
    unit_vector_t correct = {q0, b0};
    REQUIRE(com.get_args() == correct);
  }
}

SCENARIO("Testing successors and predecessors on valid circuits") {
  Circuit circ(4);
  Vertex ccx = circ.add_op<unsigned>(OpType::CCX, {1, 2, 0});
  Vertex h = circ.add_op<unsigned>(OpType::H, {1});
  Vertex pg = circ.add_op<unsigned>(OpType::PhaseGadget, 0.2, {0, 1, 3, 2});
  VertexVec correct = {h, pg};
  REQUIRE(circ.get_successors(ccx) == correct);
  correct = {ccx, h, circ.q_inputs()[3]};
  REQUIRE(circ.get_predecessors(pg) == correct);
}

SCENARIO("Testing getter methods for created and discarded qubits") {
  Circuit circ(2);
  circ.qubit_create(Qubit(0));
  circ.qubit_discard(Qubit(0));
  circ.qubit_create(Qubit(1));
  qubit_vector_t created = {Qubit(0), Qubit(1)};
  qubit_vector_t discarded = {Qubit(0)};
  REQUIRE(circ.created_qubits() == created);
  REQUIRE(circ.discarded_qubits() == discarded);
}

SCENARIO("Exception handling in get_(next/last)_q_edge") {
  Circuit circ(2);
  Vertex cx = circ.add_op<unsigned>(OpType::CX, {0, 1});
  WHEN("get_next_q_edge is not given an in edge") {
    Edge e = circ.get_nth_in_edge(circ.q_outputs()[0], 0);
    REQUIRE_THROWS_AS(circ.get_next_edge(cx, e), CircuitInvalidity);
  }
  WHEN("get_last_q_edge is not given an out edge") {
    Edge e = circ.get_nth_out_edge(circ.q_inputs()[0], 0);
    REQUIRE_THROWS_AS(circ.get_last_edge(cx, e), CircuitInvalidity);
  }
}

SCENARIO("Exception handling in get_(next/prev)_q_pair") {
  Circuit circ(1);
  Vertex pg = circ.add_op<unsigned>(OpType::PhaseGadget, 0.3, {0});
  Edge loop_edge = circ.add_edge({pg, 1}, {pg, 1}, EdgeType::Quantum);
  WHEN("get_next_q_pair is given a looping edge") {
    REQUIRE_THROWS_AS(circ.get_next_pair(pg, loop_edge), CircuitInvalidity);
  }
  WHEN("get_prev_q_pair is given a looping edge") {
    REQUIRE_THROWS_AS(circ.get_prev_pair(pg, loop_edge), CircuitInvalidity);
  }
}

SCENARIO("Checking reverse slicing gives expected results") {
  GIVEN("A circuit with no slicing freedom") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {0});
    SliceVec forwards = circ.get_slices();
    SliceVec backwards = circ.get_reverse_slices();
    SliceVec::reverse_iterator r = backwards.rbegin();
    for (const Slice& s : forwards) {
      CHECK(s == *r);
      r++;
    }
    REQUIRE(r == backwards.rend());
  }
  GIVEN("A circuit with some slicing freedom") {
    Circuit circ(2);
    Vertex x = circ.add_op<unsigned>(OpType::X, {0});
    Vertex y = circ.add_op<unsigned>(OpType::Y, {0});
    Vertex z = circ.add_op<unsigned>(OpType::Z, {1});
    SliceVec backwards = circ.get_reverse_slices();
    REQUIRE(backwards.size() == 2);
    Slice correct = {y, z};
    REQUIRE(backwards[0] == correct);
    correct = {x};
    REQUIRE(backwards[1] == correct);
  }
}

SCENARIO(
    "Check that a multiqubit Circuit can generate correct QCommands",
    "[n-qubit],[QCommands]") {
  GIVEN("A well defined 4 qubit circuit with CCX, CZ") {
    Circuit test1(4, 1);
    test1.add_op<unsigned>(OpType::H, {0});
    test1.add_op<unsigned>(OpType::CCX, {0, 2, 1});
    test1.add_op<unsigned>(OpType::CZ, {2, 0});
    test1.add_op<unsigned>(OpType::CZ, {2, 3});
    test1.add_op<unsigned>(OpType::Z, {3});
    test1.add_op<unsigned>(OpType::Measure, {3, 0});

    std::vector<Command> qcoms = test1.get_commands();
    REQUIRE(qcoms.size() == 6);
    REQUIRE((qcoms[1].get_op_ptr())->get_type() == OpType::CCX);
    unit_vector_t test_qbs = {Qubit(0), Qubit(2), Qubit(1)};
    REQUIRE(qcoms[1].get_args() == test_qbs);
    test_qbs = {Qubit(2), Qubit(3)};
    REQUIRE(qcoms[3].get_args() == test_qbs);
    REQUIRE(qcoms[5].get_qubits() == qubit_vector_t{Qubit(3)});
    REQUIRE(qcoms[5].get_bits() == bit_vector_t{Bit(0)});

    std::list<Command> qcoms_cz = test1.get_commands_of_type(OpType::CZ);
    REQUIRE(qcoms_cz.size() == 2);
    std::list<Command> qcoms_m = test1.get_commands_of_type(OpType::Measure);
    REQUIRE(qcoms_m.size() == 1);
  }
}

SCENARIO(
    "Check that copying a simple 1-qubit Circuit into another works",
    "[1-qubit],[add_circ],[copy_graph]") {
  GIVEN("Viable small circuits of single-qubit gates") {
    Circuit test(1);
    test.add_op<unsigned>(OpType::H, {0});
    unsigned num_ins_1 = test.n_units();
    unsigned depth1 = test.depth();
    Circuit test2(1);
    test2.add_op<unsigned>(OpType::X, {0});
    test2.add_op<unsigned>(OpType::Y, {0});
    test2.rename_units<Qubit, Qubit>({{Qubit(0), Qubit("a")}});
    unsigned num_ins_2 = test2.n_units();
    unsigned depth2 = test2.depth();
    test.copy_graph(test2);
    REQUIRE(
        test.n_units() ==
        num_ins_1 + num_ins_2);  // test no. of inputs/outputs is conserved
    unsigned max_depth = (depth1 > depth2) ? depth1 : depth2;
    REQUIRE(
        test.depth() ==
        max_depth);  // test depth function works for combined circuit
  }
}

SCENARIO(
    "Check that using * copying a larger n-qubit Circuit into another "
    "works",
    "[n-qubit],[add_circ],[copy_graph],[parallel]") {
  GIVEN("Viable circuits of n-qubit gates") {
    // circuit 1
    Circuit test(2);
    test.add_op<unsigned>(OpType::H, {0});
    test.add_op<unsigned>(OpType::CZ, {1, 0});
    test.add_op<unsigned>(OpType::CRz, 0.5, {1, 0});

    unsigned num_ins_1 = test.n_units();
    unsigned depth1 = test.depth();
    // circuit 2
    Circuit test2(4);
    test2.add_op<unsigned>(OpType::X, {0});
    test2.add_op<unsigned>(OpType::Rz, 0.25, {2});
    add_2qb_gates(test2, OpType::CX, {{0, 1}, {1, 0}, {0, 1}});
    test2.add_op<unsigned>(OpType::X, {0});
    test2.add_op<unsigned>(OpType::SWAP, {1, 3});
    test2.rename_units<Qubit, Qubit>(
        {{Qubit(0), Qubit("a", 0)},
         {Qubit(1), Qubit("a", 1)},
         {Qubit(2), Qubit("a", 2)},
         {Qubit(3), Qubit("a", 3)}});

    unsigned num_ins_2 = test2.n_units();
    unsigned depth2 = test2.depth();
    unsigned max_depth = (depth1 > depth2) ? depth1 : depth2;
    Circuit test3 = test * test2;
    REQUIRE(test3.n_units() == num_ins_1 + num_ins_2);
    REQUIRE(test3.depth() == max_depth);
    test.copy_graph(test2);
    REQUIRE(test.n_units() == num_ins_1 + num_ins_2);
    REQUIRE(test.depth() == max_depth);
  }
}

SCENARIO("Edge cases for all_qubit_paths") {
  GIVEN("An empty circuit") {
    Circuit test;
    REQUIRE(test.all_qubit_paths() == std::vector<QPathDetailed>());
    REQUIRE(test.implicit_qubit_permutation() == qubit_map_t());
  }
}

SCENARIO("Check that a simple SWAP removal can be performed", "[SWAP]") {
  GIVEN("A basic circuit containing a SWAP") {
    Circuit test2;
    Vertex b1 = test2.add_vertex(OpType::Input);
    Vertex b2 = test2.add_vertex(OpType::Input);
    Vertex b3 = test2.add_vertex(OpType::SWAP);
    Vertex b5 = test2.add_vertex(OpType::Output);
    Vertex b6 = test2.add_vertex(OpType::Output);
    Qubit qb0(0);
    Qubit qb1(1);
    test2.boundary.insert({qb0, b1, b5});
    test2.boundary.insert({qb1, b2, b6});

    test2.add_edge({b1, 0}, {b3, 0}, EdgeType::Quantum);
    test2.add_edge({b2, 0}, {b3, 1}, EdgeType::Quantum);
    test2.add_edge({b3, 0}, {b5, 0}, EdgeType::Quantum);
    test2.add_edge({b3, 1}, {b6, 0}, EdgeType::Quantum);
    VertexVec old_path_0 = test2.qubit_path_vertices(qb0);
    REQUIRE(old_path_0[0] == b1);
    REQUIRE(old_path_0[1] == b3);
    REQUIRE(old_path_0[2] == b5);
    VertexVec old_path_1 = test2.qubit_path_vertices(qb1);
    REQUIRE(old_path_1[0] == b2);
    REQUIRE(old_path_1[1] == b3);
    REQUIRE(old_path_1[2] == b6);
    std::vector<Command> coms = test2.get_commands();
    REQUIRE(coms.size() == 1);
    unit_vector_t qbs = {qb0, qb1};
    Command test_command(get_op_ptr(OpType::SWAP), qbs);
    REQUIRE(coms[0] == test_command);
    REQUIRE(test2.replace_SWAPs());
    // test2.print_graph();
    VertexVec new_path_0 = test2.qubit_path_vertices(qb0);
    test2.assert_valid();
  }
}

SCENARIO(
    "Check that the circuit copy constructor is working",
    "[copy_graph],[constructor]") {
  GIVEN("A basic circuit") {
    Circuit test(1);
    test.add_op<unsigned>(OpType::H, {0});
    test.add_op<unsigned>(OpType::X, {0});

    Circuit copied(test);
    // vertex pointers are different now. use vertex map or summat
    // REQUIRE(test.get_slices()==copied.get_slices());
    REQUIRE(test.depth() == copied.depth());

    std::vector<Command> coms = copied.get_commands();
    REQUIRE(coms.size() == 2);
  }
}

SCENARIO(
    "Check that circuits can be copy-pasted and then operations performed "
    "successfully",
    "[routing],[copy_graph]") {
  GIVEN("Two simple circuits") {
    Circuit test_i(1);
    test_i.add_op<unsigned>(OpType::Rx, 0.75, {0});

    Circuit comTest(2);
    comTest.add_op<unsigned>(OpType::X, {0});
    comTest.add_op<unsigned>(OpType::CX, {0, 1});

    Circuit test2(2);
    add_1qb_gates(test2, OpType::X, {0, 1});
    test2.add_op<unsigned>(OpType::SWAP, {0, 1});
    add_1qb_gates(test2, OpType::X, {0, 1});

    WHEN("A graph is copied") {
      test2.rename_units<Qubit, Qubit>(
          {{Qubit(0), Qubit("a")}, {Qubit(1), Qubit("b")}});
      test2.copy_graph(test_i);
      THEN("Slices/routing grids are retrieved") {
        SliceVec someslices = test2.get_slices();
        REQUIRE(someslices.size() == 3);
      }
    }

    WHEN("SWAPs are removed") {
      REQUIRE(test2.replace_SWAPs());
      test2.assert_valid();
      REQUIRE(test2.get_commands().size() == 4);
    }
    WHEN(">> tested") {
      // Circuit another(test2);
      Circuit bigNew = test2 >> test2;

      REQUIRE(bigNew.n_qubits() == test2.n_qubits());
      bigNew.assert_valid();

      Circuit resultant(2);
      add_1qb_gates(resultant, OpType::X, {0, 1});
      resultant.add_op<unsigned>(OpType::SWAP, {0, 1});
      add_1qb_gates(resultant, OpType::X, {0, 1, 0, 1});
      resultant.add_op<unsigned>(OpType::SWAP, {0, 1});
      add_1qb_gates(resultant, OpType::X, {0, 1});

      REQUIRE(resultant.get_commands().size() == 10);
      REQUIRE(resultant == bigNew);
      THEN("Can do so recursively") {
        for (unsigned i = 0; i < 4; ++i) {
          test2 = test2 >> test2;
        }
        for (unsigned i = 0; i < 3; ++i) {
          resultant = resultant >> resultant;
        }
        REQUIRE(resultant == test2);
      }
      // bigNew.print_graph();
    }
  }
}

SCENARIO(
    "Test that substituting a basic circuit into another works properly",
    "[sub],[copy_graph]") {
  GIVEN("Two basic circuits") {
    Circuit test(2);
    Vertex h1 = test.add_op<unsigned>(OpType::H, {0});
    Vertex h2 = test.add_op<unsigned>(OpType::H, {1});

    Circuit test2(2);
    Vertex x1 = test2.add_op<unsigned>(OpType::X, {0});
    Vertex x2 = test2.add_op<unsigned>(OpType::X, {1});
    test2.add_op<unsigned>(OpType::SWAP, {0, 1});
    add_1qb_gates(test2, OpType::X, {0, 1});
    unsigned depth_before = test2.depth();
    WHEN("The substitution is accurately performed") {
      Edge e1 = test2.get_nth_in_edge(x1, 0);
      Edge e2 = test2.get_nth_in_edge(x2, 0);
      Edge e3 = test2.get_nth_out_edge(x1, 0);
      Edge e4 = test2.get_nth_out_edge(x2, 0);
      Subcircuit sub = {{e1, e2}, {e3, e4}, {}, {x1, x2}};
      test2.substitute(test, sub);
      REQUIRE(test2.get_slices().size() == depth_before);
      test2.assert_valid();
    }
    WHEN("The reverse substitution is performed") {
      Edge f1 = test.get_nth_in_edge(h1, 0);
      Edge f2 = test.get_nth_in_edge(h2, 0);
      Edge f3 = test.get_nth_out_edge(h1, 0);
      Edge f4 = test.get_nth_out_edge(h2, 0);
      Subcircuit sub = {{f1, f2}, {f3, f4}, {}, {h1, h2}};
      test.substitute(test2, sub, Circuit::VertexDeletion::Yes);
      REQUIRE(test.get_slices().size() == depth_before);
      test.assert_valid();
    }
    WHEN("The circuit to insert is not simple") {
      unit_map_t qmap;
      Edge e1 = test2.get_nth_in_edge(x1, 0);
      Edge e2 = test2.get_nth_in_edge(x2, 0);
      Edge e3 = test2.get_nth_out_edge(x1, 0);
      Edge e4 = test2.get_nth_out_edge(x2, 0);
      Subcircuit sub = {{e1, e2}, {e3, e4}, {}, {x1, x2}};
      test2.substitute(test, sub);
      qmap.insert({Qubit(0), Qubit("a", 1)});
      qmap.insert({Qubit(1), Qubit("b", 0)});
      test.rename_units(qmap);
      REQUIRE_THROWS_AS(test2.substitute(test, sub), SimpleOnly);
    }
  }
  GIVEN("Circuits with Classical effects") {
    Circuit circ(2, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Vertex meas = circ.add_op<unsigned>(OpType::Measure, {0, 0});
    Vertex condz =
        circ.add_conditional_gate<unsigned>(OpType::Z, {}, {0}, {0}, 1);
    Vertex condcx =
        circ.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 1}, {0}, 1);
    Subcircuit sub = circ.singleton_subcircuit(condz);
    Circuit rep(1, 1);
    rep.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0}, 0);
    circ.substitute(rep, sub, Circuit::VertexDeletion::Yes);
    REQUIRE(
        circ.get_commands()[2].get_op_ptr()->get_type() == OpType::Conditional);
    Vertex source_of_condition = circ.source(circ.get_nth_in_edge(condcx, 0));
    REQUIRE(source_of_condition == meas);
  }
  GIVEN("Circuits with WASM") {
    Circuit circ(1, 2);
    Op_ptr wop_ptr = std::make_shared<WASMOp>(
        3, 2, std::vector<unsigned>{1, 1}, std::vector<unsigned>{1}, "wasmfunc",
        "path/to/file");
    Vertex wv = circ.add_op<UnitID>(
        wop_ptr, {Bit(0), Bit(1), Bit(0), WasmState(0), WasmState(1)});
    circ.assert_valid();
    Subcircuit sub = circ.singleton_subcircuit(wv);

    Circuit rep(0, 3);
    rep.add_wasm_register(2);
    Op_ptr wop_ptr2 = std::make_shared<WASMOp>(
        2, 1, std::vector<unsigned>{1}, std::vector<unsigned>{1}, "smallerfunc",
        "path/to/file2");
    rep.add_op<UnitID>(wop_ptr2, {Bit(0), Bit(2), WasmState(1)});
    circ.substitute(rep, sub, Circuit::VertexDeletion::Yes);
    circ.assert_valid();
    REQUIRE(circ.n_vertices() == 11);
    REQUIRE(circ.get_commands().at(0).to_str() == "WASM c[0], c[0], _w[1];");
  }
}

SCENARIO("Test that substituting edge cases works correctly") {
  GIVEN("A circuit with 1 op acting on all qubits") {
    Circuit test(2);
    Vertex cx = test.add_op<unsigned>(OpType::CX, {0, 1});
    Subcircuit sub = test.singleton_subcircuit(cx);
    Qubit qb0(0);
    Qubit qb1(1);
    WHEN("A cross-wire is substituted") {
      Circuit test2;
      Vertex b1 = test2.add_vertex(OpType::Input);
      Vertex b2 = test2.add_vertex(OpType::Input);
      Vertex b3 = test2.add_vertex(OpType::Output);
      Vertex b4 = test2.add_vertex(OpType::Output);
      test2.boundary.insert({qb0, b1, b3});
      test2.boundary.insert({qb1, b2, b4});
      test2.add_edge({b1, 0}, {b4, 0}, EdgeType::Quantum);
      test2.add_edge({b2, 0}, {b3, 0}, EdgeType::Quantum);

      test.substitute(test2, sub);
      REQUIRE(test.get_successors(test.get_in(qb0))[0] == test.get_out(qb1));
      REQUIRE(test.get_successors(test.get_in(qb1))[0] == test.get_out(qb0));
    }

    WHEN("Parallel wires are substituted") {
      Circuit test2(2);
      test.substitute(test2, sub);
      REQUIRE(test.get_successors(test.get_in(qb0))[0] == test.get_out(qb0));
      REQUIRE(test.get_successors(test.get_in(qb1))[0] == test.get_out(qb1));
    }
  }
  GIVEN("A complex n-qubit circuit") {
    Circuit test2(4);
    Vertex x1 = test2.add_op<unsigned>(OpType::X, {0});
    Vertex rx = test2.add_op<unsigned>(OpType::Rx, 0.25, {2});
    Vertex cx1 = test2.add_op<unsigned>(OpType::CX, {0, 1});
    Vertex cx2 = test2.add_op<unsigned>(OpType::CX, {1, 0});
    Vertex cx3 = test2.add_op<unsigned>(OpType::CX, {0, 1});
    Vertex x2 = test2.add_op<unsigned>(OpType::X, {0});
    Vertex swap = test2.add_op<unsigned>(OpType::SWAP, {1, 3});
    EdgeVec ins;
    for (const Vertex& i : test2.q_inputs()) {
      ins.push_back(test2.get_nth_out_edge(i, 0));
    }
    std::vector<std::optional<Edge>> outs;
    for (const Vertex& o : test2.q_outputs()) {
      outs.push_back(test2.get_nth_in_edge(o, 0));
    }
    Subcircuit sub = {ins, outs, {}, {x1, rx, cx1, cx2, cx3, x2, swap}};
    WHEN("A single vertex is substituted") {
      Circuit test3(4);
      test3.add_barrier({0, 1, 2, 3});

      test2.substitute(test3, sub);
      REQUIRE(test2.depth() == 0);
      REQUIRE(test2.get_slices().size() == 1);
      test2.assert_valid();
    }
  }
  GIVEN(
      "Test that subcircuit substitution works when given in/outedges "
      "which are the same") {
    Circuit to_sub(2);
    to_sub.add_op<unsigned>(OpType::CZ, {0, 1});
    to_sub.add_op<unsigned>(OpType::H, {0});
    Circuit circ(2);
    Vertex cz = circ.add_op<unsigned>(OpType::CZ, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Subcircuit subcirc;
    subcirc.in_hole = circ.get_all_out_edges(cz);
    subcirc.out_hole = circ.get_linear_out_edges(cz);
    circ.substitute(to_sub, subcirc, Circuit::VertexDeletion::Yes);
    REQUIRE(circ.n_gates() == 4);
    REQUIRE(circ.count_gates(OpType::CZ) == 2);
    REQUIRE(circ.count_gates(OpType::CX) == 1);
    REQUIRE(circ.count_gates(OpType::H) == 1);
  }
}

SCENARIO(
    "Test a circuit with blank wires can have the blank wires removed",
    "[blank_wires]") {
  Circuit test(2);
  test.add_op<unsigned>(OpType::CX, {0, 1});
  test.add_op<unsigned>(OpType::Z, {0});

  WHEN("Check Commands work correctly") {
    std::vector<Command> coms = test.get_commands();
    REQUIRE(*coms[0].get_op_ptr() == *get_op_ptr(OpType::CX));
    REQUIRE(*coms[1].get_op_ptr() == *get_op_ptr(OpType::Z));
  }

  test.add_blank_wires(8);
  REQUIRE(test.n_bits() == 0);
  REQUIRE(test.n_qubits() == 10);
  test.remove_blank_wires();
  REQUIRE(test.n_bits() == 0);
  REQUIRE(test.n_qubits() == 2);
  test.assert_valid();
}

SCENARIO(
    "Test a circuit with blank wires can have the blank wires removed keeping "
    "classical",
    "[blank_wires]") {
  Circuit test(4, 2);
  test.add_op<unsigned>(OpType::CX, {0, 1});
  test.add_op<unsigned>(OpType::Z, {0});

  WHEN("Check Commands work correctly") {
    std::vector<Command> coms = test.get_commands();
    REQUIRE(*coms[0].get_op_ptr() == *get_op_ptr(OpType::CX));
    REQUIRE(*coms[1].get_op_ptr() == *get_op_ptr(OpType::Z));
  }

  test.add_blank_wires(8);
  REQUIRE(test.n_bits() == 2);
  REQUIRE(test.n_qubits() == 12);
  test.remove_blank_wires(true);
  REQUIRE(test.n_bits() == 2);
  REQUIRE(test.n_qubits() == 2);
  test.assert_valid();
}

SCENARIO(
    "Test that the copy constructor and copy assignment operator work "
    "correctly",
    "[copy]") {
  GIVEN("An initial circuit") {
    Circuit circ(6);
    circ.add_op<unsigned>(OpType::Z, {0});
    Vertex xgate = circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::Y, {2});
    circ.add_barrier({3, 4});
    VertexSet bin = {xgate};
    circ.remove_vertices(
        bin, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::Yes);
    circ.assert_valid();
    unsigned N = circ.n_vertices();

    WHEN("Copy constructor used") {
      Circuit circ2(circ);
      circ2.add_op<unsigned>(OpType::CZ, {0, 1});
      circ2.add_blank_wires(1);
      circ2.assert_valid();
      REQUIRE(circ2.n_vertices() == N + 3);
    }

    WHEN("Copy assignment operator used") {
      Circuit circ3 = circ;
      circ3.remove_blank_wires();
      circ3.assert_valid();
      REQUIRE(circ3.n_vertices() == N - 4);
    }
  }
}

SCENARIO("circuit equality ", "[equality]") {
  GIVEN("Two large, equal circuits") {
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
    add_1qb_gates(test1, OpType::X, {0, 0});
    test1.add_op<unsigned>(OpType::CX, {3, 2});
    test1.add_op<unsigned>(OpType::Y, {3});
    Circuit test2(test1);
    REQUIRE(test1 == test2);
  }
  GIVEN("Circuits with equivalent parameter expressions") {
    Circuit test1(2);
    test1.add_op<unsigned>(OpType::CX, {0, 1});
    test1.add_op<unsigned>(OpType::Rx, Expr(1 / sqrt(2.)), {0});
    Circuit test2(2);
    test2.add_op<unsigned>(OpType::CX, {0, 1});
    test2.add_op<unsigned>(OpType::Rx, SymEngine::cos(Expr("pi") / 4), {0});
    REQUIRE(test1 == test2);
  }
  GIVEN("Circuits with known mismatches") {
    Circuit test1(2);
    Circuit test2(2);
    CHECK(test1 == test2);

    test1.set_name("test");

    CHECK_FALSE(test1 == test2);
    CHECK_THROWS_AS(test1.circuit_equality(test2), CircuitInequality);

    test2.set_name("test");
    CHECK(test1 == test2);

    test1.add_phase(0.3);

    CHECK_FALSE(test1 == test2);
    CHECK_THROWS_AS(test1.circuit_equality(test2), CircuitInequality);
    test2.add_phase(0.3);

    Circuit cliff_simp(test1);
    add_2qb_gates(cliff_simp, OpType::CX, {{0, 1}, {1, 0}});
    Transforms::clifford_simp().apply(cliff_simp);

    test1.add_op<unsigned>(OpType::CX, {1, 0});

    CHECK_FALSE(test1 == cliff_simp);
    CHECK_THROWS_AS(test1.circuit_equality(cliff_simp), CircuitInequality);

    test1.permute_boundary_output(cliff_simp.implicit_qubit_permutation());
    CHECK(test1 == cliff_simp);

    test2.add_op<unsigned>(OpType::CX, {1, 0});
    test2.permute_boundary_output(cliff_simp.implicit_qubit_permutation());
    CHECK(test1 == test2);

    test2.add_op<unsigned>(OpType::CX, {1, 0});
    CHECK_FALSE(test1 == test2);

    test1.add_op<unsigned>(OpType::CX, {1, 0});

    test1.add_bit(Bit(0));

    CHECK_FALSE(test1 == test2);
    CHECK_THROWS_AS(test1.circuit_equality(test2), CircuitInequality);

    test2.add_bit(Bit(0));

    test2.add_qubit(Qubit(3));

    CHECK_FALSE(test1 == test2);
    CHECK_THROWS_AS(test1.circuit_equality(test2), CircuitInequality);

    test1.add_qubit(Qubit(3));

    REQUIRE(test1 == test2);

    REQUIRE_THROWS(test1.add_qubit(Qubit(3), true));
    test1.add_qubit(Qubit(3), false);
    REQUIRE_THROWS(test1.add_bit(Bit(0), true));
    test1.add_bit(Bit(0), false);
  }
  GIVEN("Circuits with mismatched created qubits") {
    Circuit test1(2);
    Circuit test2(2);
    test1.qubit_create(Qubit(0));
    REQUIRE(test1 != test2);
    REQUIRE_THROWS_MATCHES(
        test1.circuit_equality(test2), CircuitInequality,
        MessageContains("Circuit created qubits do not match."));
  }
  GIVEN("Circuit with duplicate qubit/bit register names") {
    Circuit circ;
    Qubit qa0("a", 0);
    Bit bb0("b", 0);
    Qubit qb1("b", 1);
    Bit ba1("a", 1);
    circ.add_qubit(qa0);
    circ.add_bit(bb0);
    REQUIRE_THROWS(circ.add_qubit(qb1));
    REQUIRE_THROWS(circ.add_bit(ba1));
  }
  GIVEN("Circuits with mismatched discarded qubits") {
    Circuit test1(2);
    Circuit test2(2);
    test1.qubit_discard(Qubit(0));
    REQUIRE(test1 != test2);
    REQUIRE_THROWS_MATCHES(
        test1.circuit_equality(test2), CircuitInequality,
        MessageContains("Circuit discarded qubits do not match."));
  }
  GIVEN("Circuits with matched qubit boundary types") {
    Circuit test1(2);
    Circuit test2(2);
    test1.qubit_create(Qubit(0));
    test1.qubit_discard(Qubit(0));
    test1.qubit_create(Qubit(1));
    test2.qubit_create(Qubit(0));
    test2.qubit_discard(Qubit(0));
    test2.qubit_create(Qubit(1));
    REQUIRE(test1 == test2);
  }
}

SCENARIO("Test that subcircuits are correctly generated") {
  GIVEN("A circuit with an interesting subgraph") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CX, {2, 0});
    Vertex cx = circ.add_op<unsigned>(OpType::CX, {0, 1});
    Vertex z = circ.add_op<unsigned>(OpType::Z, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    Subcircuit s = {
        circ.get_in_edges(cx),
        {circ.get_nth_out_edge(z, 0), circ.get_nth_out_edge(cx, 1)},
        {},
        {cx, z}};
    Circuit sub = circ.subcircuit(s);
    bool test =
        (sub.get_OpType_from_Vertex(
             sub.get_successors(sub.get_in(Qubit(0)))[0]) == OpType::CX &&
         sub.get_OpType_from_Vertex(
             sub.get_predecessors(sub.get_out(Qubit(0)))[0]) == OpType::Z);
    REQUIRE(test);
    circ.substitute(sub, s, Circuit::VertexDeletion::Yes);
    check_command_types(circ, {OpType::CX, OpType::CX, OpType::Z, OpType::CX});
  }
  GIVEN("A subcircuit with conditional gates") {
    Circuit circ(2, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Measure, {0, 0});
    Vertex condz =
        circ.add_conditional_gate<unsigned>(OpType::Z, {}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::CX, {}, {0, 1}, {0}, 1);
    Subcircuit s = circ.singleton_subcircuit(condz);
    Circuit sub = circ.subcircuit(s);
    REQUIRE(
        sub.get_commands()[0].get_op_ptr()->get_type() == OpType::Conditional);
  }
  GIVEN("A subcircuit of multiple gates, including all kinds of edges") {
    Circuit circ(2, 2);
    circ.add_conditional_gate<unsigned>(OpType::Z, {}, {0}, {0}, 1);
    Vertex cx = circ.add_op<unsigned>(OpType::CX, {0, 1});
    Vertex cond_y =
        circ.add_conditional_gate<unsigned>(OpType::Y, {}, {0}, {0}, 1);
    Op_ptr wop_ptr = std::make_shared<WASMOp>(
        3, 2, std::vector<unsigned>{1, 1}, std::vector<unsigned>{1}, "wasmfunc",
        "path/to/file");
    Vertex wv = circ.add_op<UnitID>(
        wop_ptr, {Bit(0), Bit(1), Bit(0), WasmState(0), WasmState(1)});
    circ.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0}, 1);
    Subcircuit s = circ.make_subcircuit({cx, cond_y, wv});
    Circuit sub = circ.subcircuit(s);
    sub.assert_valid();
    REQUIRE(sub.get_commands().size() == 3);
    CHECK(sub.get_commands()[0].get_op_ptr()->get_type() == OpType::WASM);
    CHECK(sub.get_commands()[1].get_op_ptr()->get_type() == OpType::CX);
    CHECK(
        sub.get_commands()[2].get_op_ptr()->get_type() == OpType::Conditional);
    THEN("Substitute with a blank circuit to check it wires up correctly") {
      // Each Boolean input to the subcircuit is treated as a separate classical
      // wire
      Circuit blank(2, 4);
      blank.add_wasm_register(2);
      circ.substitute(blank, s, Circuit::VertexDeletion::Yes);
      REQUIRE(circ.get_commands().size() == 2);
      CHECK(circ.get_commands()[0].to_str() == "IF ([c[0]] == 1) THEN Z q[0];");
      CHECK(circ.get_commands()[1].to_str() == "IF ([c[0]] == 1) THEN X q[0];");
    }
  }
}

SCENARIO("Functions with symbolic ops") {
  GIVEN("A simple circuit with symbolics to instantiate") {
    Circuit circ(2);
    Sym a = SymEngine::symbol("alpha");
    Expr alpha(a);
    Sym b = SymEngine::symbol("beta");
    Expr e = -2 * Expr(b);
    circ.add_op<unsigned>(OpType::Rz, alpha, {0});
    circ.add_op<unsigned>(OpType::PhaseGadget, e, {0, 1});
    REQUIRE(circ.is_symbolic());
    SymSet symbols = circ.free_symbols();
    REQUIRE(symbols.size() == 2);
    REQUIRE(symbols.find(a) != symbols.end());
    symbol_map_t symbol_map;
    symbol_map[a] = Expr(0.5);
    symbol_map[b] = Expr(0.7);
    circ.symbol_substitution(symbol_map);
    VertexVec vertices = circ.vertices_in_order();
    Op_ptr op2 = circ.get_Op_ptr_from_Vertex(vertices[2]);
    Op_ptr op3 = circ.get_Op_ptr_from_Vertex(vertices[3]);
    REQUIRE(op2->get_type() == OpType::Rz);
    REQUIRE(test_equiv_val(op2->get_params()[0], 0.5));
    REQUIRE(op3->get_type() == OpType::PhaseGadget);
    REQUIRE(test_equiv_val(op3->get_params()[0], 0.6));
  }
  GIVEN("A simple circuit with symbolics to instantiate, and a Barrier gate.") {
    Circuit circ(2);
    Sym a = SymEngine::symbol("alpha");
    Expr alpha(a);
    circ.add_op<unsigned>(OpType::Rx, alpha, {0});
    circ.add_barrier({0, 1});
    REQUIRE(circ.is_symbolic());
    SymSet symbols = circ.free_symbols();
    REQUIRE(symbols.size() == 1);
    REQUIRE(symbols.find(a) != symbols.end());
    symbol_map_t symbol_map;
    symbol_map[a] = Expr(0.2);
    circ.symbol_substitution(symbol_map);
    VertexVec vertices = circ.vertices_in_order();
    Op_ptr op2 = circ.get_Op_ptr_from_Vertex(vertices[2]);
    Op_ptr op3 = circ.get_Op_ptr_from_Vertex(vertices[3]);
    REQUIRE(op2->get_type() == OpType::Rx);
    REQUIRE(test_equiv_val(op2->get_params()[0], 0.2));
    REQUIRE(op3->get_type() == OpType::Barrier);
  }
  GIVEN("A circuit with symbolic gates and boxes that belong to opgroups.") {
    Sym asym = SymEngine::symbol("a");
    Expr alpha(asym);
    Sym bsym = SymEngine::symbol("b");
    Expr beta(bsym);
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Rx, alpha, {0}, "Rx");
    Circuit inner_circ(2);
    inner_circ.add_op<unsigned>(OpType::Rx, {alpha}, {0});
    inner_circ.add_op<unsigned>(OpType::Ry, {beta}, {0});
    auto cbox = CircBox(inner_circ);
    circ.add_box(cbox, {0, 1}, "cbox");
    DensePauliMap paulis0{Pauli::X, Pauli::X};
    DensePauliMap paulis1{Pauli::Z, Pauli::X};
    auto ppbox = PauliExpPairBox(
        SymPauliTensor(paulis0, alpha), SymPauliTensor(paulis1, beta));
    circ.add_box(ppbox, {0, 1}, "ppbox");
    symbol_map_t symbol_map;
    symbol_map[asym] = Expr(0.2);
    symbol_map[bsym] = Expr(0.3);
    circ.symbol_substitution(symbol_map);
    REQUIRE(!circ.is_symbolic());
    std::unordered_set<std::string> opgroups({"Rx", "cbox", "ppbox"});
    REQUIRE(circ.get_opgroups() == opgroups);
    std::vector<Command> cmds = circ.get_commands();
    REQUIRE(cmds.size() == 3);
    REQUIRE(cmds[0].get_opgroup().value() == "Rx");
    REQUIRE(cmds[1].get_opgroup().value() == "cbox");
    REQUIRE(cmds[2].get_opgroup().value() == "ppbox");
  }
}

SCENARIO("Test depth_by_type method") {
  GIVEN("A trivial CX circuit") {
    for (unsigned N = 0; N < 10; ++N) {
      Circuit circ(2);
      for (unsigned i = 0; i < N; ++i) {
        circ.add_op<unsigned>(OpType::CX, {0, 1});
      }
      REQUIRE(circ.depth_by_type(OpType::CX) == N);
      REQUIRE(circ.depth() == circ.depth_by_type(OpType::CX));
    }
  }
  GIVEN("A non-trivial circuit") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {2, 1});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::CX, {3, 1});
    REQUIRE(circ.depth_by_type(OpType::CX) == 3);
    REQUIRE(circ.dagger().n_vertices() == circ.n_vertices());
  }
  GIVEN("A circuit with other causal links") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_barrier({0, 2});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    REQUIRE(circ.depth_by_type(OpType::CX) == 2);
  }
  GIVEN("A Clifford+T circuit and getting T-depth") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::T, {1});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::T, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::T, {1});
    REQUIRE(circ.depth_by_type(OpType::T) == 3);
  }
  GIVEN("A T-depth circuit") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::T, {3});
    circ.add_op<unsigned>(OpType::CCX, {1, 2, 3});
    circ.add_op<unsigned>(OpType::T, {2});
    circ.add_op<unsigned>(OpType::CCX, {0, 1, 2});
    circ.add_op<unsigned>(OpType::T, {1});
    REQUIRE(circ.depth_by_type(OpType::T) == 3);
  }
  GIVEN("A CNOT-depth circuit") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {2});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    REQUIRE(circ.depth_by_type(OpType::CX) == 1);
  }
  GIVEN("Product state with classical wires") {
    Circuit circ(4, 2);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_measure(1, 1);
    circ.add_op<unsigned>(OpType::Y, {2});
    circ.add_op<unsigned>(OpType::S, {2});
    circ.add_op<unsigned>(OpType::T, {2});
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    REQUIRE(circ.depth_by_type(OpType::CX) == 1);
  }
  GIVEN("Interacting via classical effects") {
    Circuit circ(4, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_measure(1, 0);
    circ.add_conditional_gate<unsigned>(OpType::X, {}, uvec{2}, {0}, 1);
    circ.add_op<unsigned>(OpType::CX, {2, 3});
    REQUIRE(circ.depth_by_type(OpType::CX) == 2);
  }
  GIVEN("Multiple OpTypes") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {1});
    circ.add_op<unsigned>(OpType::CY, {2, 1});
    REQUIRE(circ.depth_by_types({OpType::CX, OpType::CY}) == 2);
  }
}
SCENARIO("Test depth_2q method") {
  GIVEN("2q OpTypes") {
    Circuit circ(3, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {0});
    circ.add_op<unsigned>(OpType::Z, {1});
    circ.add_op<unsigned>(OpType::CZ, {1, 0});
    circ.add_conditional_gate<unsigned>(OpType::CY, {}, {1, 2}, {0}, 0);
    REQUIRE(circ.depth_2q() == 3);
  }
  GIVEN("Boxes") {
    Circuit circ(3);
    Circuit inner(2);
    inner.add_op<unsigned>(OpType::CX, {0, 1});
    CircBox cbox(inner);
    circ.add_box(cbox, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {1});
    REQUIRE(circ.depth_2q() == 1);
  }
  GIVEN("Multi-q gates") {
    Circuit circ(5, 2);
    circ.add_op<unsigned>(OpType::CnX, {0, 1, 2, 4, 3});
    circ.add_op<unsigned>(OpType::CnX, {0, 2});
    circ.add_op<unsigned>(OpType::CnX, {2, 4, 3});
    circ.add_op<unsigned>(OpType::CnX, {3, 4});
    REQUIRE(circ.depth_2q() == 2);
  }
  GIVEN("A circuit with other causal links") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_barrier({0, 2});
    circ.add_op<unsigned>(OpType::CZ, {2, 3});
    REQUIRE(circ.depth_2q() == 2);
  }
}

SCENARIO("Test extracting slice segments") {
  GIVEN("A simple circuit") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CZ, {1, 2});
    circ.add_op<unsigned>(OpType::CY, {2, 0});
    circ.add_op<unsigned>(OpType::CH, {0, 1});
    circ.add_op<unsigned>(OpType::CX, {1, 2});
    circ.add_op<unsigned>(OpType::CX, {2, 0});
    circ.extract_slice_segment(2, 4);
    REQUIRE(circ.n_vertices() == 9);
    std::set<OpType> optypes;
    for (const Command& cmd : circ) {
      optypes.insert(cmd.get_op_ptr()->get_type());
    }
    std::set<OpType> expected = {OpType::CZ, OpType::CY, OpType::CH};
    REQUIRE(optypes == expected);
  }
}

SCENARIO("Test next slice") {
  GIVEN("A simple circuit") {
    Circuit circ(4);
    Vertex v1 = circ.add_op<unsigned>(OpType::X, {0});
    Vertex v8 = circ.add_op<unsigned>(OpType::S, {3});
    circ.add_op<unsigned>(OpType::T, {3});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::CY, {2, 3});
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CZ, {0, 2});
    circ.add_op<unsigned>(OpType::Y, {0});
    circ.add_op<unsigned>(OpType::CX, {3, 1});

    auto frontier = std::make_shared<unit_frontier_t>();
    for (const Qubit& q : circ.all_qubits()) {
      Vertex in = circ.get_in(q);
      frontier->insert({q, circ.get_nth_out_edge(in, 0)});
    }
    CutFrontier slice_front =
        circ.next_cut(frontier, std::make_shared<b_frontier_t>());
    Slice sl = *slice_front.slice;
    WHEN("The frontier is calculated from inputs") {
      THEN("The first slice is recovered accurately.") {
        REQUIRE(sl.size() == 2);
        REQUIRE(sl[0] == v1);
        REQUIRE(sl[1] == v8);
      }
    }
  }
}

SCENARIO("Test next quantum slice") {
  GIVEN("A simple circuit") {
    Circuit circ(3, 1);
    Vertex v1 = circ.add_op<unsigned>(OpType::X, {0});
    Vertex v2 =
        circ.add_conditional_gate<unsigned>(OpType::Rx, {0.6}, {1}, {0}, 1);
    Vertex v3 =
        circ.add_conditional_gate<unsigned>(OpType::Ry, {0.6}, {2}, {0}, 1);
    circ.add_op<unsigned>(OpType::S, {2});
    circ.add_op<unsigned>(OpType::T, {1});

    auto frontier = std::make_shared<unit_frontier_t>();
    for (const Qubit& q : circ.all_qubits()) {
      Vertex in = circ.get_in(q);
      frontier->insert({q, circ.get_nth_out_edge(in, 0)});
    }
    CutFrontier slice_front = circ.next_q_cut(frontier);
    Slice sl = *slice_front.slice;
    WHEN("The frontier is calculated from inputs") {
      THEN("The first slice is recovered accurately.") {
        REQUIRE(sl.size() == 3);
        REQUIRE(sl[0] == v1);
        REQUIRE(sl[1] == v2);
        REQUIRE(sl[2] == v3);
      }
    }
  }
}

SCENARIO("Test circuit.transpose() method") {
  GIVEN("Simple circuit") {
    Circuit circ(2);
    // Unitary2qBox
    Eigen::Matrix4cd m;
    m << 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0;
    Unitary2qBox ubox(m);
    circ.add_box(ubox, {1, 0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});

    // Compute the transpose
    Circuit circ_t = circ.transpose();

    // Retrieve the underlying OpTypes
    std::vector<Command> coms = circ_t.get_commands();
    const Op_ptr ubox_t_ptr = coms[1].get_op_ptr();
    const Op_ptr cx_t_ptr = coms[0].get_op_ptr();
    // Casting the Unitary2qBox type
    std::shared_ptr<const Unitary2qBox> ubox_t =
        std::dynamic_pointer_cast<const Unitary2qBox>(ubox_t_ptr);
    CHECK(ubox_t_ptr->get_name() == "Unitary2qBox");
    CHECK(cx_t_ptr->get_name() == "CX");
    REQUIRE(matrices_are_equal(ubox_t->get_matrix(), m.transpose()));
    REQUIRE(*cx_t_ptr == *get_op_ptr(OpType::CX));
  }
  GIVEN("Circuit with barriers") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Y, {0});
    circ.add_barrier({0, 1}, {}, "comment");
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Circuit correct_transposed(2);
    correct_transposed.add_op<unsigned>(OpType::CX, {0, 1});
    correct_transposed.add_barrier({0, 1}, {}, "comment");
    correct_transposed.add_op<unsigned>(OpType::U3, {3, 0.5, 0.5}, {0});
    Circuit transposed = circ.transpose();
    REQUIRE(transposed == correct_transposed);
    transposed.assert_valid();
  }
}

SCENARIO("Test circuit.dagger() method") {
  GIVEN("Simple circuit") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Sdg, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::V, {1});
    circ.add_op<unsigned>(OpType::Vdg, {1});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::S, {0});
    Circuit daggered = circ.dagger();
    REQUIRE(daggered == circ);
    SliceVec slices1, slices2;
    for (SliceIterator sliceit = daggered.slice_begin();
         sliceit != daggered.slice_end(); sliceit++) {
      slices1.push_back(*sliceit);
    }
    slices2 = daggered.get_slices();
    REQUIRE(slices1 == slices2);
    daggered.assert_valid();
  }
  GIVEN("A circuit with complex gates") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CnRy, 0.2, {0, 1});
    Eigen::Matrix4cd mat;
    mat << 1, 0, 0, 0, 0, i_, 0, 0, 0, 0, 0, -i_, 0, 0, i_, 0;
    circ.add_box(Unitary2qBox(mat), {1, 2});
    circ.add_op<unsigned>(OpType::TK1, {0.3, 0.7, 0.8}, {1});
    Circuit daggered = circ.dagger();
    daggered.assert_valid();

    const Eigen::MatrixXcd u = tket_sim::get_unitary(circ);
    const Eigen::MatrixXcd udag = tket_sim::get_unitary(daggered);
    REQUIRE(u.adjoint().isApprox(udag, ERR_EPS));
  }
  GIVEN("Circuit with barriers") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::Sdg, {0});
    circ.add_barrier({0, 1}, {}, "comment");
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    Circuit correct_daggered(2);
    correct_daggered.add_op<unsigned>(OpType::CX, {0, 1});
    correct_daggered.add_barrier({0, 1}, {}, "comment");
    correct_daggered.add_op<unsigned>(OpType::S, {0});
    Circuit daggered = circ.dagger();
    REQUIRE(daggered == correct_daggered);
    daggered.assert_valid();
  }
}

SCENARIO("Test conditional_circuit method") {
  GIVEN("A circuit with wireswaps") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    REQUIRE(circ.replace_SWAPs());
    REQUIRE_THROWS_AS(
        circ.conditional_circuit({Bit(0), Bit(1)}, 3), CircuitInvalidity);
  }
  GIVEN("A circuit that writes to one of the condition bits") {
    Circuit circ(2, 2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::Measure, {1, 1});
    REQUIRE_THROWS_AS(
        circ.conditional_circuit({Bit(0), Bit(1)}, 3), CircuitInvalidity);
  }
  GIVEN("A basic, valid circuit") {
    Circuit circ(2, 2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::Measure, {1, 1});
    Circuit cond_circ = circ.conditional_circuit({Bit(0), Bit(2)}, 3);
    std::vector<Command> coms = cond_circ.get_commands();
    REQUIRE(coms.size() == 3);
    for (const Command& com : coms) {
      CHECK(com.get_op_ptr()->get_type() == OpType::Conditional);
    }
    unit_vector_t args = {Bit(0), Bit(2), Qubit(0), Qubit(1)};
    CHECK(coms[0].get_args() == args);
    args = {Bit(0), Bit(2), Qubit(1)};
    CHECK(coms[1].get_args() == args);
    args = {Bit(0), Bit(2), Qubit(1), Bit(1)};
    CHECK(coms[2].get_args() == args);
  }
}

SCENARIO("Test append method") {
  Circuit test2(2);
  add_1qb_gates(test2, OpType::X, {0, 1});
  test2.add_op<unsigned>(OpType::SWAP, {0, 1});
  add_1qb_gates(test2, OpType::X, {0, 1});

  Circuit test3(test2);
  test2.append(test3);

  REQUIRE(test2.is_simple());
  REQUIRE(test2.n_qubits() == test3.n_qubits());
  test2.assert_valid();

  Circuit resultant(2);
  add_1qb_gates(resultant, OpType::X, {0, 1});
  resultant.add_op<unsigned>(OpType::SWAP, {0, 1});
  add_1qb_gates(resultant, OpType::X, {0, 1, 0, 1});
  resultant.add_op<unsigned>(OpType::SWAP, {0, 1});
  add_1qb_gates(resultant, OpType::X, {0, 1});

  REQUIRE(resultant == test2);
  SliceVec slices1, slices2;
  for (auto slice = resultant.slice_begin(); slice != resultant.slice_end();
       slice++) {
    slices1.push_back(*slice);
  }
  slices2 = resultant.get_slices();
  REQUIRE(slices1 == slices2);
}

SCENARIO("Test Command Iterator") {
  GIVEN("A single qubit circuit") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::X, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Command com{op, {Qubit(0)}};
    Circuit::CommandIterator cit = circ.begin();
    REQUIRE(*cit == com);
    cit++;
    REQUIRE(cit == circ.end());
  }

  GIVEN("A 2qubit circuit") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Z, {1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    const Op_ptr op2 = get_op_ptr(OpType::CX);
    const Op_ptr op3 = get_op_ptr(OpType::Z);
    Qubit qb0(0);
    Qubit qb1(1);
    Command com{op, {qb0}};
    Command com2{op2, {qb0, qb1}};
    Command com3{op3, {qb1}};
    Circuit::CommandIterator cit = circ.begin();
    REQUIRE(*cit == com);
    cit++;
    REQUIRE(*cit == com2);
    cit++;
    REQUIRE(*cit == com3);
  }

  GIVEN("A 3-qb circuit") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CX, {0, 2});
    circ.add_op<unsigned>(OpType::CZ, {1, 2});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {1});
    circ.add_op<unsigned>(OpType::S, {0});
    circ.add_op<unsigned>(OpType::Tdg, {2});
    OpTypeSet allowed_ops = {OpType::CX, OpType::CZ,  OpType::Rz,
                             OpType::S,  OpType::Tdg, OpType::Output};
    std::vector<Command> comvec;
    for (Command x : circ) {
      bool found =
          (allowed_ops.find(x.get_op_ptr()->get_type()) != allowed_ops.end());
      REQUIRE(found);
      comvec.push_back(x);
    }
    REQUIRE(comvec.size() == circ.n_gates());
  }
}

SCENARIO("Test substitute_all") {
  Circuit circ(3, 1);
  circ.add_op<unsigned>(OpType::Rx, 0.6, {0});
  circ.add_op<unsigned>(OpType::H, {0});
  circ.add_op<unsigned>(OpType::CX, {1, 0});
  circ.add_op<unsigned>(OpType::CZ, {0, 2});
  circ.add_op<unsigned>(OpType::X, {0});
  circ.add_op<unsigned>(OpType::Y, {2});
  circ.add_op<unsigned>(OpType::CRz, 0.3, {0, 1});
  circ.add_op<unsigned>(OpType::Rz, 0.4, {2});
  circ.add_conditional_gate<unsigned>(OpType::Rx, {0.6}, {2}, {0}, 1);
  circ.add_op<unsigned>(OpType::Rx, 0.6, {1});

  WHEN("Replace with a 1qb valid circuit") {
    Op_ptr op = get_op_ptr(OpType::Rx, 0.6);
    Circuit sub(1);
    sub.add_op<unsigned>(OpType::X, {0});
    sub.add_op<unsigned>(OpType::Rx, 1.6, {0});

    REQUIRE_NOTHROW(circ.substitute_all(sub, op));
    REQUIRE(circ.n_vertices() == 21);

    Circuit correct_circ(3, 1);
    correct_circ.add_op<unsigned>(OpType::X, {0});
    correct_circ.add_op<unsigned>(OpType::Rx, 1.6, {0});
    correct_circ.add_op<unsigned>(OpType::H, {0});
    correct_circ.add_op<unsigned>(OpType::CX, {1, 0});
    correct_circ.add_op<unsigned>(OpType::CZ, {0, 2});
    correct_circ.add_op<unsigned>(OpType::X, {0});
    correct_circ.add_op<unsigned>(OpType::Y, {2});
    correct_circ.add_op<unsigned>(OpType::CRz, 0.3, {0, 1});
    correct_circ.add_op<unsigned>(OpType::Rz, 0.4, {2});
    correct_circ.add_conditional_gate<unsigned>(OpType::X, {}, {2}, {0}, 1);
    correct_circ.add_conditional_gate<unsigned>(OpType::Rx, {1.6}, {2}, {0}, 1);
    correct_circ.add_op<unsigned>(OpType::X, {1});
    correct_circ.add_op<unsigned>(OpType::Rx, 1.6, {1});

    REQUIRE(circ == correct_circ);
  }
  WHEN("Replace with a different, 2qb valid circuit") {
    Op_ptr op = get_op_ptr(OpType::CRz, 0.3);
    Circuit sub(2);
    sub.add_op<unsigned>(OpType::CX, {0, 1});
    REQUIRE_NOTHROW(circ.substitute_all(sub, op));

    Circuit correct_circ(3, 1);
    correct_circ.add_op<unsigned>(OpType::Rx, 0.6, {0});
    correct_circ.add_op<unsigned>(OpType::H, {0});
    correct_circ.add_op<unsigned>(OpType::CX, {1, 0});
    correct_circ.add_op<unsigned>(OpType::CZ, {0, 2});
    correct_circ.add_op<unsigned>(OpType::X, {0});
    correct_circ.add_op<unsigned>(OpType::Y, {2});
    correct_circ.add_op<unsigned>(OpType::CX, {0, 1});
    correct_circ.add_op<unsigned>(OpType::Rz, 0.4, {2});
    correct_circ.add_conditional_gate<unsigned>(OpType::Rx, {0.6}, {2}, {0}, 1);
    correct_circ.add_op<unsigned>(OpType::Rx, 0.6, {1});

    REQUIRE(circ == correct_circ);
  }

  WHEN("Try to replace with an invalid circuit") {
    Op_ptr op = get_op_ptr(OpType::CRz, 0.3);
    Circuit sub(3);
    sub.add_op<unsigned>(OpType::CCX, {0, 1, 2});
    REQUIRE_THROWS(circ.substitute_all(sub, op));
  }

  WHEN("Substituting a conditional op") {
    Circuit circ(2, 1);
    circ.add_conditional_gate<unsigned>(OpType::SWAP, {}, {0, 1}, {0}, 1);
    REQUIRE(circ.n_gates() == 1);
    Circuit newswap(2);
    add_2qb_gates(newswap, OpType::CX, {{0, 1}, {1, 0}, {0, 1}});
    REQUIRE(Transforms::decompose_SWAP(newswap).apply(circ));
    REQUIRE(circ.n_gates() == 3);
  }
}

SCENARIO("Decomposing a multi-qubit operation into CXs") {
  const double sq = 1 / std::sqrt(2.);
  GIVEN("Trivial (single-qubit) case") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::X, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const Eigen::MatrixXcd u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 0, 1, 1, 0;
    REQUIRE(u.isApprox(correct));
  }
  GIVEN("A CZ gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CZ, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const Eigen::MatrixXcd u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    correct << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CY gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CY, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const Eigen::MatrixXcd u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0, -i_,
            0, 0, i_, 0;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CH gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CH, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, sq, sq,
            0, 0, sq, -sq;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CCX gate") {
    Circuit circ(3);
    Vertex v = circ.add_op<unsigned>(OpType::CCX, {0, 1, 2});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(8, 8);
    // clang-format off
        correct << 1, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 1,
            0, 0, 0, 0, 0, 0, 1, 0;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CRz gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CRz, 0.5, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, sq - sq * i_, 0,
            0, 0, 0, sq + sq * i_;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
    REQUIRE(rep.count_gates(OpType::CX) == 2);
  }
  GIVEN("A CRz(+-pi) gate") {
    Circuit circ(2);
    Vertex v;
    WHEN("CRz(+pi)") { v = circ.add_op<unsigned>(OpType::CRz, 1., {0, 1}); }
    WHEN("CRz(-pi)") { v = circ.add_op<unsigned>(OpType::CRz, -1., {0, 1}); }
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    rep = CX_circ_from_multiq(op);

    REQUIRE(rep.count_gates(OpType::CX) == 1);

    const auto u = tket_sim::get_unitary(rep);
    const auto u_correct = tket_sim::get_unitary(circ);
    REQUIRE((u - u_correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CRx gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CRx, 0.5, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, sq, -sq * i_,
            0, 0, -sq* i_, sq;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
    REQUIRE(rep.count_gates(OpType::CX) == 2);
  }
  GIVEN("A CRx(+-pi) gate") {
    Circuit circ(2);
    Vertex v;
    WHEN("CRx(+pi)") { v = circ.add_op<unsigned>(OpType::CRx, 1., {0, 1}); }
    WHEN("CRx(-pi)") { v = circ.add_op<unsigned>(OpType::CRx, -1., {0, 1}); }
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    rep = CX_circ_from_multiq(op);

    REQUIRE(rep.count_gates(OpType::CX) == 1);

    const auto u = tket_sim::get_unitary(rep);
    const auto u_correct = tket_sim::get_unitary(circ);
    REQUIRE((u - u_correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CRy gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CRy, 0.5, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, sq, -sq,
            0, 0, sq, sq;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
    REQUIRE(rep.count_gates(OpType::CX) == 2);
  }
  GIVEN("A CRy(+-pi) gate") {
    Circuit circ(2);
    Vertex v;
    WHEN("CRy(+pi)") { v = circ.add_op<unsigned>(OpType::CRy, 1., {0, 1}); }
    WHEN("CRy(-pi)") { v = circ.add_op<unsigned>(OpType::CRy, -1., {0, 1}); }
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    rep = CX_circ_from_multiq(op);

    REQUIRE(rep.count_gates(OpType::CX) == 1);

    const auto u = tket_sim::get_unitary(rep);
    const auto u_correct = tket_sim::get_unitary(circ);
    REQUIRE((u - u_correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CV gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CV, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, sq, sq * -i_,
            0, 0, sq * -i_, sq;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CVdg gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CVdg, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, sq, sq * i_,
            0, 0, sq * i_, sq;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }

  GIVEN("A CSX gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CSX, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0.5 * (1. + i_), 0.5 * (1. - i_),
            0, 0, 0.5 * (1. - i_), 0.5 * (1. + i_);
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CSXdg gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CSXdg, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0.5 * (1. - i_), 0.5 * (1. + i_),
            0, 0, 0.5 * (1. + i_), 0.5 * (1. - i_);
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }

  GIVEN("A CS gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CS, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, i_;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CSdg gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CSdg, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, -i_;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }

  GIVEN("A CU1 gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::CU1, 0.5, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, i_;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CU3 gate") {
    Circuit circ(2);
    std::vector<Expr> p = {0.5, 0.5, 1.};
    Vertex v = circ.add_op<unsigned>(OpType::CU3, p, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, sq, sq,
            0, 0, sq * i_, -sq * i_;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A PhaseGadget") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::PhaseGadget, 0.3, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    const PauliString pauliop({Pauli::Z, Pauli::Z});
    Eigen::MatrixXcd exponent = -pauliop.to_sparse_matrix(2) * 0.15 * PI * i_;
    Eigen::MatrixXcd correct = exponent.exp();
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("An ISWAP gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::ISWAP, 0.5, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(4, 4);
    // clang-format off
        correct << 1, 0, 0, 0,
            0, sq, sq * i_, 0,
            0, sq * i_, sq, 0,
            0, 0, 0, 1;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A CSWAP gate") {
    Circuit circ(3);
    Vertex v = circ.add_op<unsigned>(OpType::CSWAP, {0, 1, 2});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(8, 8);
    // clang-format off
        correct << 1, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 1;
    // clang-format on
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A Molmer-Sorensen gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::XXPhase, 0.5, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    const PauliString pauliop({Pauli::X, Pauli::X});
    Eigen::MatrixXcd exponent = -pauliop.to_sparse_matrix(2) * 0.25 * PI * i_;
    Eigen::MatrixXcd correct = exponent.exp();
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A 3-qubit Molmer-Sorensen gate") {
    Circuit circ(3);
    Vertex v = circ.add_op<unsigned>(OpType::XXPhase3, 0.5, {0, 1, 2});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("tket-sim XXPhase3 unitary") { rep = circ; }
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    const PauliString pauliop01({Pauli::X, Pauli::X, Pauli::I});
    const PauliString pauliop12({Pauli::I, Pauli::X, Pauli::X});
    const PauliString pauliop02({Pauli::X, Pauli::I, Pauli::X});
    Eigen::MatrixXcd exponent =
        -(pauliop01.to_sparse_matrix(3) + pauliop12.to_sparse_matrix(3) +
          pauliop02.to_sparse_matrix(3)) *
        0.25 * PI * i_;

    Eigen::MatrixXcd correct = exponent.exp();

    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A ZZMax gate") {
    Circuit circ(2);
    Vertex v = circ.add_op<unsigned>(OpType::ZZMax, {0, 1});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    const PauliString pauliop({Pauli::Z, Pauli::Z});
    Eigen::MatrixXcd exponent = -pauliop.to_sparse_matrix(2) * 0.25 * PI * i_;
    Eigen::MatrixXcd correct = exponent.exp();
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("An NPhasedX gate") {
    Circuit circ(3);
    Vertex v = circ.add_op<unsigned>(OpType::NPhasedX, {0.5, 1.5}, {0, 1, 2});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("tket-sim NPhasedX unitary") { rep = circ; }
    WHEN("Default circuit replacement") { rep = CX_circ_from_multiq(op); }

    const auto u = tket_sim::get_unitary(rep);
    Circuit phasedx(1);
    phasedx.add_op<unsigned>(OpType::PhasedX, {0.5, 1.5}, {0});
    const auto phasedx_u = tket_sim::get_unitary(phasedx);
    Eigen::MatrixXcd correct = Eigen::kroneckerProduct(
        phasedx_u, Eigen::kroneckerProduct(phasedx_u, phasedx_u));
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A gate with no defined decomposition") {
    Circuit circ(1);
    Vertex box = circ.add_barrier(uvec{0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(box);
    REQUIRE_THROWS_AS(CX_circ_from_multiq(op), BadOpType);
    REQUIRE_THROWS_AS(CX_ZX_circ_from_op(op), BadOpType);
  }
}

SCENARIO("Decomposing a single qubit gate") {
  const double sq = 1 / std::sqrt(2.);
  GIVEN("A Z gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::Z, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1, 0, 0, -1;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("An X gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::X, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 0, 1, 1, 0;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A Y gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::Y, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 0, -i_, i_, 0;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("An S gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::S, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1, 0, 0, i_;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("An Sdg gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::Sdg, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1, 0, 0, -i_;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A T gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::T, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1, 0, 0, sq * (1. + i_);
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A Tdg gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::Tdg, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1, 0, 0, sq * (1. - i_);
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A V gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::V, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1, -i_, -i_, 1;
    correct *= sq;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A Vdg gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::Vdg, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1, i_, i_, 1;
    correct *= sq;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A SX gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::SX, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1. + i_, 1. - i_, 1. - i_, 1. + i_;
    correct *= 0.5;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A SXdg gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::SXdg, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1. - i_, 1. + i_, 1. + i_, 1. - i_;
    correct *= 0.5;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A H gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::H, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    Eigen::MatrixXcd correct(2, 2);
    correct << 1, 1, 1, -1;
    correct *= sq;
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("An Rx gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::Rx, 0.3, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    const PauliString pauliop({Pauli::X});
    Eigen::MatrixXcd exponent = -pauliop.to_sparse_matrix(1) * 0.15 * PI * i_;
    Eigen::MatrixXcd correct = exponent.exp();
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("An Ry gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::Ry, 0.4, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    const PauliString pauliop({Pauli::Y});
    Eigen::MatrixXcd exponent = -pauliop.to_sparse_matrix(1) * 0.2 * PI * i_;
    Eigen::MatrixXcd correct = exponent.exp();
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("An Rz gate") {
    Circuit circ(1);
    Vertex v = circ.add_op<unsigned>(OpType::Rz, 0.7, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    const PauliString pauliop({Pauli::Z});
    Eigen::MatrixXcd exponent = -pauliop.to_sparse_matrix(1) * 0.35 * PI * i_;
    Eigen::MatrixXcd correct = exponent.exp();
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("U gates") {
    Circuit circ(1);
    Vertex v;
    WHEN("Taking a U3 gate") {
      std::vector<Expr> params = {0.1, 0.8, 1.4};
      v = circ.add_op<unsigned>(OpType::U3, params, {0});
    }
    WHEN("Taking a U2 gate") {
      std::vector<Expr> params = {0.8, 1.4};
      v = circ.add_op<unsigned>(OpType::U2, params, {0});
    }
    WHEN("Taking a U1 gate") {
      v = circ.add_op<unsigned>(OpType::U1, 1.4, {0});
    }
    const Circuit rep = CX_ZX_circ_from_op(circ.get_Op_ptr_from_Vertex(v));
    const auto u = tket_sim::get_unitary(rep);
    const auto correct = tket_sim::get_unitary(circ);
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A PhasedX gate") {
    Circuit circ(1);
    std::vector<Expr> params = {0.6, 1.3};
    Vertex v = circ.add_op<unsigned>(OpType::PhasedX, params, {0});
    const Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Circuit rep;
    WHEN("Default circuit replacement") {
      std::pair<Op_ptr, Expr> rep_op = op_to_tk1(op);
      rep.add_blank_wires(1);
      rep.add_op<unsigned>(rep_op.first, {0});
      rep.add_phase(rep_op.second);
    }
    WHEN("ZX circuit replacement") { rep = CX_ZX_circ_from_op(op); }

    const auto u = tket_sim::get_unitary(rep);
    const PauliString pauliop({Pauli::Z});
    Eigen::MatrixXcd exponent = -pauliop.to_sparse_matrix(1) * 0.65 * PI * i_;
    Eigen::MatrixXcd phaser = exponent.exp();
    exponent = -PauliString({Pauli::X}).to_sparse_matrix(1) * 0.3 * PI * i_;
    Eigen::MatrixXcd correct = phaser * exponent.exp() * phaser.adjoint();
    REQUIRE((u - correct).cwiseAbs().sum() < ERR_EPS);
  }
  GIVEN("A gate with no defined decomposition") {
    Circuit circ(1);
    Vertex box = circ.add_barrier(uvec{0});
    const Op_ptr g = (circ.get_Op_ptr_from_Vertex(box));
    REQUIRE_THROWS_AS(op_to_tk1(g), BadOpType);
  }
}

SCENARIO("Attempt to append using qubit map") {
  GIVEN("Circuits 1") {
    Circuit circ(5);
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::Z, {1});
    circ.add_op<unsigned>(OpType::Y, {2});
    circ.add_op<unsigned>(OpType::H, {3});
    add_2qb_gates(circ, OpType::CX, {{2, 3}, {1, 3}, {0, 2}});
    qubit_vector_t qr = {Qubit(0), Qubit(1), Qubit(2), Qubit(3), Qubit(4)};

    WHEN("Append circuit 1") {
      Circuit circ2(2);
      circ2.add_op<unsigned>(OpType::Rz, 0.3, {0});
      circ2.add_op<unsigned>(OpType::Ry, 0.4, {1});
      unit_map_t qm;
      qm.insert({qr[0], qr[3]});
      qm.insert({qr[1], qr[0]});

      circ.append_with_map(circ2, qm);
      REQUIRE(circ.n_vertices() == 19);
      REQUIRE(circ.depth() == 4);

      REQUIRE(circ.n_qubits() == 5);
    }
    WHEN("Append circuit 2") {
      Circuit circ3(5);
      circ3.add_op<unsigned>(OpType::Rz, 0.1, {0});
      circ3.add_op<unsigned>(OpType::Rz, 0.2, {1});
      circ3.add_op<unsigned>(OpType::Rz, 0.3, {2});
      circ3.add_op<unsigned>(OpType::Rz, 0.4, {3});
      circ3.add_op<unsigned>(OpType::Rz, 0.5, {4});
      unit_map_t qm;
      qm.insert({qr[0], qr[4]});
      qm.insert({qr[1], qr[3]});
      qm.insert({qr[2], qr[2]});
      qm.insert({qr[3], qr[1]});
      qm.insert({qr[4], qr[0]});
      circ.append_with_map(circ3, qm);

      REQUIRE(circ.n_vertices() == 10 + 12);
      REQUIRE(circ.depth() == 4);
      REQUIRE(circ.n_qubits() == 5);

      VertexSet encountered;
      for (const Qubit& qb : qr) {
        const Vertex& v = circ.get_out(qb);
        REQUIRE(v != boost::graph_traits<DAG>::null_vertex());
        REQUIRE(encountered.find(v) == encountered.end());
        encountered.insert(v);
      }
    }
  }
}

SCENARIO("Attempt to append using qubit vector") {
  GIVEN("Pre-existing 4qb circuit") {
    Circuit circ(4);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::U1, 0.3, {0});
    circ.add_op<unsigned>(OpType::CZ, {3, 2});
    circ.add_op<unsigned>(OpType::CX, {1, 2});

    WHEN("Try adding new circuit") {
      Circuit circ2(2);
      circ2.add_op<unsigned>(OpType::Rz, 0.3, {0});
      circ2.add_op<unsigned>(OpType::CRz, 0.7, {0, 1});

      std::vector<unsigned> qbs = {2, 3};
      circ.append_qubits(circ2, qbs);

      Circuit compare(4);
      compare.add_op<unsigned>(OpType::CX, {0, 1});
      compare.add_op<unsigned>(OpType::U1, 0.3, {0});
      compare.add_op<unsigned>(OpType::CZ, {3, 2});
      compare.add_op<unsigned>(OpType::CX, {1, 2});
      compare.add_op<unsigned>(OpType::Rz, 0.3, {2});
      compare.add_op<unsigned>(OpType::CRz, 0.7, {2, 3});
      REQUIRE(compare == circ);
    }

    WHEN("Try adding a new circuit incorrectly") {
      Circuit circ2(5);
      add_2qb_gates(
          circ2, OpType::CX, {{0, 1}, {0, 2}, {0, 3}, {0, 4}, {4, 1}});

      std::vector<unsigned> qbs = {0, 1, 3, 4};
      REQUIRE_THROWS(circ.append_qubits(circ2, qbs));
    }
  }
}

SCENARIO("Attempt to append multiple circuits sequentially") {
  Circuit d(2);
  d.add_op<unsigned>(OpType::CX, {0, 1});

  Circuit circ(8);
  for (const auto& pair : std::vector<std::vector<unsigned>>{
           {3, 7}, {1, 2}, {5, 7}, {0, 1}, {2, 3}, {4, 5}, {6, 7}}) {
    circ.append_qubits(d, pair);
  }
  qubit_vector_t qr = {Qubit(0), Qubit(1), Qubit(2), Qubit(3),
                       Qubit(4), Qubit(5), Qubit(6), Qubit(7)};
  REQUIRE(circ.n_vertices() == 23);
  REQUIRE(circ.n_gates() == 7);
  std::vector<Command> coms = circ.get_commands();
  std::vector<unit_vector_t> correct_qubits = {
      {qr[1], qr[2]}, {qr[3], qr[7]}, {qr[0], qr[1]}, {qr[2], qr[3]},
      {qr[5], qr[7]}, {qr[4], qr[5]}, {qr[6], qr[7]}};
  REQUIRE(coms.size() == correct_qubits.size());
  for (unsigned i = 0; i < coms.size(); ++i) {
    REQUIRE(coms[i].get_args() == correct_qubits[i]);
  }
  // Test the boundary has no duplicates or missing vertices
  VertexSet encountered;
  for (const Qubit& qb : qr) {
    const Vertex& v = circ.get_out(qb);
    REQUIRE(v != boost::graph_traits<DAG>::null_vertex());
    REQUIRE(encountered.find(v) == encountered.end());
    encountered.insert(v);
  }
}

SCENARIO("Represent symbolic operations correctly") {
  Circuit c(1);
  Sym a = SymEngine::symbol("alpha");
  Expr alpha(a);
  c.add_op<unsigned>(OpType::Rz, 0.5, {0});
  c.add_op<unsigned>(OpType::Rz, 0.5 * alpha, {0});
  REQUIRE(c.is_symbolic());
  const SymSet symbols = c.free_symbols();
  REQUIRE(symbols.size() == 1);
  REQUIRE(symbols.find(a) != symbols.end());
  std::stringstream cmd_0, cmd_1;
  cmd_0 << c.get_commands()[0];
  cmd_1 << c.get_commands()[1];
  std::string expected_0 = "Rz(0.5) q[0];";
  std::string expected_1 = "Rz(0.5*alpha) q[0];";
  REQUIRE(cmd_0.str() == expected_0);
  REQUIRE(cmd_1.str() == expected_1);
}

SCENARIO("Confirm that LaTeX output compiles", "[latex][.long]") {
  Circuit c(5, 2);
  c.add_conditional_gate<unsigned>(OpType::Z, {}, uvec{0}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::U1, {0.3}, uvec{1}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CZ, {}, {0, 1}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::YYPhase, {0.6}, {4, 3}, {}, 0);
  c.add_measure(0, 0);
  c.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0, 1}, 2);
  c.add_conditional_gate<unsigned>(OpType::CX, {}, {1, 0}, {1}, 1);
  c.add_conditional_gate<unsigned>(OpType::SWAP, {}, {1, 0}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CCX, {}, {2, 4, 3}, {0}, 0);
  c.add_conditional_gate<unsigned>(OpType::CSWAP, {}, {3, 4, 2}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CnX, {}, {0, 1, 2, 4, 3}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CnY, {}, {0, 1, 2, 4, 3}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CnZ, {}, {0, 1, 2, 4, 3}, {}, 0);
  c.add_conditional_gate<unsigned>(
      OpType::CnRy, {-0.57}, {0, 3, 2, 4, 1}, {}, 0);
  c.add_conditional_gate<unsigned>(
      OpType::CnRx, {-0.57}, {0, 3, 2, 4, 1}, {}, 0);
  c.add_conditional_gate<unsigned>(
      OpType::CnRz, {-0.57}, {0, 3, 2, 4, 1}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CH, {}, {1, 0}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CY, {}, {2, 3}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CRz, {1.42}, {0, 2}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CRx, {1.42}, {0, 2}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CRy, {1.42}, {0, 2}, {}, 0);
  c.add_conditional_gate<unsigned>(OpType::CU1, {0.02}, {4, 3}, {}, 0);
  c.add_conditional_gate<unsigned>(
      OpType::CU3, {1.04, 0.36, -0.36}, {0, 4}, {}, 0);

  // https://github.com/quantinuum/tket/issues/1363
  Qubit q1("q_1", 0);
  Bit c1("c_1", 0);
  c.add_qubit(q1);
  c.add_bit(c1);
  c.add_measure(q1, c1);

  c.to_latex_file("circ.tex");
  int response = std::system("latexmk -pdf circ.tex -quiet");
  REQUIRE(response == 0);
  response = std::system("latexmk -C");  // clean up generated files
  REQUIRE(response == 0);
  remove("circ.tex");
}

SCENARIO("Vertex info maps") {
  GIVEN("A mixed circuit with wire swaps") {
    Circuit c;
    register_t qbs = c.add_q_register(q_default_reg(), 4);
    register_t bs = c.add_c_register(c_default_reg(), 1);
    Vertex z = c.add_conditional_gate<unsigned>(OpType::Z, {}, uvec{3}, {}, 0);
    Vertex cx = c.add_conditional_gate<unsigned>(OpType::CX, {}, {2, 1}, {}, 0);
    Vertex cz =
        c.add_conditional_gate<unsigned>(OpType::CZ, {}, {2, 0}, {0}, 1);
    Vertex m = c.add_measure(0, 0);
    Vertex x = c.add_conditional_gate<unsigned>(OpType::X, {}, uvec{1}, {0}, 1);
    c.add_op<unsigned>(OpType::SWAP, {0, 1});
    Vertex cy =
        c.add_conditional_gate<unsigned>(OpType::CY, {}, {1, 2}, {0}, 1);
    REQUIRE(c.replace_SWAPs());
    THEN("Check vertex_unit_map") {
      std::map<Vertex, unit_set_t> vmap = c.vertex_unit_map();
      unit_set_t correct = {qbs[1], qbs[2]};
      REQUIRE(vmap.at(cx) == correct);
      correct = {qbs[0], bs[0]};
      REQUIRE(vmap.at(m) == correct);
      correct = {qbs[1]};
      REQUIRE(vmap.at(x) == correct);
      correct = {qbs[0], qbs[2]};
      REQUIRE(vmap.at(cy) == correct);
      correct = {bs[0]};
      REQUIRE(vmap.at(c.get_in(bs[0])) == correct);
      correct = {qbs[1]};
      REQUIRE(vmap.at(c.get_out(qbs[0])) == correct);
    }
    THEN("Check vertex_depth_map") {
      std::map<Vertex, unsigned> dmap = c.vertex_depth_map();
      REQUIRE(dmap.at(z) == 0);
      REQUIRE(dmap.at(cx) == 0);
      REQUIRE(dmap.at(cz) == 1);
      REQUIRE(dmap.at(m) == 2);
      REQUIRE(dmap.at(x) == 3);
      REQUIRE(dmap.at(cy) == 3);
      REQUIRE(dmap.at(c.get_in(qbs[0])) == 0);
      REQUIRE(dmap.at(c.get_out(bs[0])) == 4);
    }
  }
  GIVEN("A mixed circuit with no classical control") {
    Circuit c;
    register_t qbs = c.add_q_register(q_default_reg(), 4);
    register_t bs = c.add_c_register(c_default_reg(), 1);
    Vertex z = c.add_conditional_gate<unsigned>(OpType::Z, {}, uvec{3}, {}, 0);
    Vertex cx = c.add_conditional_gate<unsigned>(OpType::CX, {}, {2, 1}, {}, 0);
    Vertex cz = c.add_conditional_gate<unsigned>(OpType::CZ, {}, {2, 0}, {}, 0);
    Vertex m = c.add_measure(0, 0);
    Vertex x = c.add_conditional_gate<unsigned>(OpType::X, {}, uvec{1}, {}, 0);
    c.add_op<unsigned>(OpType::SWAP, {0, 1});
    Vertex cy = c.add_conditional_gate<unsigned>(OpType::CY, {}, {1, 2}, {}, 0);
    REQUIRE(c.replace_SWAPs());
    THEN("Check vertex_rev_depth_map") {
      std::map<Vertex, unsigned> dmap = c.vertex_rev_depth_map();
      REQUIRE(dmap.at(z) == 0);
      REQUIRE(dmap.at(cx) == 3);
      REQUIRE(dmap.at(cz) == 2);
      REQUIRE(dmap.at(m) == 1);
      REQUIRE(dmap.at(x) == 0);
      REQUIRE(dmap.at(cy) == 0);
      REQUIRE(dmap.at(c.get_in(qbs[0])) == 4);
      REQUIRE(dmap.at(c.get_out(bs[0])) == 0);
    }
  }
}

SCENARIO("(qu)bit_readout/mapping for a circuit") {
  Circuit c;
  register_t qreg = c.add_q_register("q", 4);
  register_t creg = c.add_c_register("c", 3);
  register_t dreg = c.add_c_register("d", 1);
  c.add_measure(Qubit(qreg[0]), Bit(creg[0]));
  c.add_measure(Qubit(qreg[1]), Bit(creg[2]));
  c.add_measure(Qubit(qreg[2]), Bit(creg[2]));
  c.add_measure(Qubit(qreg[3]), Bit(creg[1]));
  c.add_op<Qubit>(OpType::X, {Qubit(qreg[3])});
  // barriers should have no effect
  c.add_barrier({qreg[2]});
  c.add_barrier({qreg[2]});
  c.add_barrier({creg[0], creg[1]});
  std::map<Bit, unsigned> readout = c.bit_readout();
  REQUIRE(readout.size() == 4);
  REQUIRE(readout.at(Bit(creg[0])) == 0);
  REQUIRE(readout.at(Bit(creg[1])) == 1);
  REQUIRE(readout.at(Bit(creg[2])) == 2);
  REQUIRE(readout.at(Bit(dreg[0])) == 3);
  std::map<Qubit, unsigned> q_readout = c.qubit_readout();
  REQUIRE(q_readout.size() == 2);
  REQUIRE(q_readout.at(Qubit(qreg[0])) == 0);
  REQUIRE(q_readout.at(Qubit(qreg[2])) == 2);
  std::map<Qubit, Bit> qb_map = c.qubit_to_bit_map();
  REQUIRE(qb_map.size() == 2);
  REQUIRE(qb_map.at(Qubit(qreg[0])) == creg[0]);
  REQUIRE(qb_map.at(Qubit(qreg[2])) == creg[2]);
}

SCENARIO("Invalid Measure operations") {
  GIVEN("Trying to add a Measure gate with no classical output") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    REQUIRE_THROWS_AS(
        circ.add_op<unsigned>(OpType::Measure, {0}), CircuitInvalidity);
  }
  GIVEN("Trying to add a Measure on non-existent wires") {
    // https://github.com/quantinuum/tket/issues/979
    Circuit circ;
    REQUIRE_THROWS(circ.add_measure(0, 0));
    std::vector<Command> cmds = circ.get_commands();
    REQUIRE(cmds.empty());
  }
}

SCENARIO("Testing add_op with Barrier type and add_barrier") {
  // TKET-377
  GIVEN("An attempt to add a Barrier with Qubit arguments") {
    Circuit c(1);
    std::vector<Expr> params;
    qubit_vector_t qubits = c.all_qubits();
    REQUIRE_THROWS_AS(
        c.add_op(OpType::Barrier, params, qubits), CircuitInvalidity);
  }
  GIVEN("An attempt to add a Barrier with unsigned arguments") {
    Circuit c(1);
    std::vector<Expr> params;
    uvec unsigneds{0};
    REQUIRE_THROWS_AS(
        c.add_op(OpType::Barrier, params, unsigneds), CircuitInvalidity);
  }
  GIVEN("An attempt to add a Barrier with no params") {
    Circuit c(1);
    uvec unsigneds{0};
    REQUIRE_THROWS_AS(c.add_op(OpType::Barrier, unsigneds), CircuitInvalidity);
  }
  GIVEN("An attempt to add a Barrier to Qubits via get_op_ptr") {
    Circuit c(1);
    qubit_vector_t qubits = c.all_qubits();
    Op_ptr barrier = get_op_ptr(OpType::Barrier);
    REQUIRE_THROWS_AS(c.add_op(barrier, qubits), CircuitInvalidity);
  }
  GIVEN("An attempt to add a Barrier to unsigneds via get_op_ptr") {
    Circuit c(1);
    uvec unsigneds{0};
    Op_ptr barrier = get_op_ptr(OpType::Barrier);
    REQUIRE_THROWS_AS(c.add_op(barrier, unsigneds), CircuitInvalidity);
  }
  GIVEN("An attempt to add barriers of different signatures") {
    Circuit c(3, 3);
    c.add_barrier({0, 1}, {});
    REQUIRE_NOTHROW(c.add_barrier({0, 1}, {0}));
  }
}

SCENARIO("Named operation groups") {
  GIVEN("A circuit with named operation groups") {
    Circuit c(3);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {1, 2});
    c.add_op<unsigned>(OpType::CX, {0, 1}, "group2");
    c.add_op<unsigned>(OpType::H, {0}, "group1");
    c.add_op<unsigned>(OpType::H, {1}, "group1");
    c.add_op<unsigned>(OpType::S, {2}, "group1");
    c.add_op<unsigned>(OpType::CX, {1, 0}, "group2");
    c.add_op<unsigned>(OpType::CX, {1, 2}, "group2");

    Op_ptr x_op = get_op_ptr(OpType::X);
    REQUIRE(c.substitute_named(x_op, "group1"));

    std::unordered_set<std::string> opgroups({"group1", "group2"});
    REQUIRE(c.get_opgroups() == opgroups);

    Circuit c2(2);
    c2.add_op<unsigned>(OpType::T, {0});
    c2.add_op<unsigned>(OpType::CRx, 0.1, {0, 1}, "group2a");
    REQUIRE(c.substitute_named(c2, "group2"));

    std::unordered_set<std::string> opgroups2({"group1", "group2a"});
    REQUIRE(c.get_opgroups() == opgroups2);

    REQUIRE(c.count_gates(OpType::H) == 1);
    REQUIRE(c.count_gates(OpType::S) == 0);
    REQUIRE(c.count_gates(OpType::X) == 3);
    REQUIRE(c.count_gates(OpType::CX) == 1);
    REQUIRE(c.count_gates(OpType::T) == 3);
    REQUIRE(c.count_gates(OpType::CRx) == 3);

    Op_ptr y_op = get_op_ptr(OpType::Y);
    REQUIRE(c.substitute_named(y_op, "group1"));

    REQUIRE(c.count_gates(OpType::X) == 0);
    REQUIRE(c.count_gates(OpType::Y) == 3);

    REQUIRE(!c.substitute_named(x_op, "group0"));

    REQUIRE(c.count_gates(OpType::X) == 0);
    REQUIRE(c.count_gates(OpType::Y) == 3);

    Eigen::Matrix4cd m;
    // clang-format off
        m << 0, 1, 0, 0,
             0, 0, 0, 1,
             0, 0, 1, 0,
             1, 0, 0, 0;
    // clang-format on
    Unitary2qBox ubox(m);
    REQUIRE(c.substitute_named(ubox, "group2a"));

    REQUIRE(c.count_gates(OpType::CRx) == 0);
    REQUIRE(c.count_gates(OpType::Unitary2qBox) == 3);

    Circuit c1 = c;
    REQUIRE(c == c1);
    REQUIRE(c.get_opgroups() == opgroups2);
    REQUIRE(c1.get_opgroups() == opgroups2);
  }
  GIVEN("Negative tests for operation groups") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::H, {0}, "group1");
    // Try adding op with incompatible signature to same group:
    REQUIRE_THROWS_AS(
        c.add_op<unsigned>(OpType::CX, {0, 1}, "group1"), CircuitInvalidity);
    c.add_op<unsigned>(OpType::X, {1}, "group1");
    Op_ptr cx_op = get_op_ptr(OpType::CX);
    // Try substituting op of incompatible signature:
    REQUIRE_THROWS_AS(c.substitute_named(cx_op, "group1"), CircuitInvalidity);
    // Try substituting a circuit with a name collision:
    Circuit c1(1);
    c1.add_op<unsigned>(OpType::Rx, 0.125, {0});
    c1.add_op<unsigned>(OpType::Z, {0}, "group1");
    REQUIRE_THROWS_AS(c.substitute_named(c1, "group1"), CircuitInvalidity);
    // Try substituting a circuit with the wrong signature:
    Circuit c2(2);
    REQUIRE_THROWS_AS(c.substitute_named(c2, "group1"), CircuitInvalidity);
    // Check circuit is unchanged:
    Circuit c3(2);
    c3.add_op<unsigned>(OpType::H, {0}, "group1");
    c3.add_op<unsigned>(OpType::X, {1}, "group1");
    REQUIRE(c == c3);
    // Check equality fails if groups are different:
    Circuit c4(2);
    c4.add_op<unsigned>(OpType::H, {0}, "group1");
    c4.add_op<unsigned>(OpType::X, {1}, "group2");
    REQUIRE(!(c == c4));
  }
}

SCENARIO("Test count_n_qubit_gates") {
  GIVEN("A Circuit with a range of different sized gates") {
    Circuit c(10, 1);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::T, {1});
    c.add_op<unsigned>(OpType::S, {2});
    c.add_op<unsigned>(OpType::X, {3});
    c.add_op<unsigned>(OpType::Y, {4});
    c.add_op<unsigned>(OpType::Z, {5});
    c.add_op<unsigned>(OpType::S, {6});
    c.add_op<unsigned>(OpType::Z, {7});
    c.add_op<unsigned>(OpType::V, {8});
    c.add_op<unsigned>(OpType::H, {9});

    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::CZ, {1, 2});
    c.add_op<unsigned>(OpType::CY, {2, 3});
    c.add_op<unsigned>(OpType::CX, {3, 4});
    c.add_op<unsigned>(OpType::CZ, {4, 5});
    c.add_op<unsigned>(OpType::CY, {5, 6});
    c.add_op<unsigned>(OpType::ZZMax, {6, 7});
    c.add_op<unsigned>(OpType::CX, {7, 8});
    c.add_op<unsigned>(OpType::CZ, {8, 9});

    c.add_op<unsigned>(OpType::CCX, {0, 1, 2});
    c.add_op<unsigned>(OpType::CCX, {1, 2, 3});
    c.add_op<unsigned>(OpType::CCX, {2, 3, 4});
    c.add_op<unsigned>(OpType::CCX, {3, 4, 5});
    c.add_op<unsigned>(OpType::CCX, {4, 5, 6});
    c.add_op<unsigned>(OpType::CCX, {5, 6, 7});
    c.add_op<unsigned>(OpType::CCX, {6, 7, 8});
    c.add_op<unsigned>(OpType::CCX, {7, 8, 9});

    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3});
    c.add_op<unsigned>(OpType::CnX, {1, 2, 3, 4});
    c.add_op<unsigned>(OpType::CnX, {2, 3, 4, 5});
    c.add_op<unsigned>(OpType::CnX, {3, 4, 5, 6});
    c.add_op<unsigned>(OpType::CnX, {4, 5, 6, 7});
    c.add_op<unsigned>(OpType::CnX, {5, 6, 7, 8});
    c.add_op<unsigned>(OpType::CnX, {6, 7, 8, 9});

    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4});
    c.add_op<unsigned>(OpType::CnX, {1, 2, 3, 4, 5});
    c.add_op<unsigned>(OpType::CnX, {2, 3, 4, 5, 6});
    c.add_op<unsigned>(OpType::CnX, {3, 4, 5, 6, 7});
    c.add_op<unsigned>(OpType::CnX, {4, 5, 6, 7, 8});
    c.add_op<unsigned>(OpType::CnX, {5, 6, 7, 8, 9});

    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4, 5});
    c.add_op<unsigned>(OpType::CnX, {1, 2, 3, 4, 5, 6});
    c.add_op<unsigned>(OpType::CnX, {2, 3, 4, 5, 6, 7});
    c.add_op<unsigned>(OpType::CnX, {3, 4, 5, 6, 7, 8});
    c.add_op<unsigned>(OpType::CnX, {4, 5, 6, 7, 8, 9});

    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4, 5, 6});
    c.add_op<unsigned>(OpType::CnX, {1, 2, 3, 4, 5, 6, 7});
    c.add_op<unsigned>(OpType::CnX, {2, 3, 4, 5, 6, 7, 8});
    c.add_op<unsigned>(OpType::CnX, {3, 4, 5, 6, 7, 8, 9});

    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4, 5, 6, 7});
    c.add_op<unsigned>(OpType::CnX, {1, 2, 3, 4, 5, 6, 7, 8});
    c.add_op<unsigned>(OpType::CnX, {2, 3, 4, 5, 6, 7, 8, 9});

    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4, 5, 6, 7, 8});
    c.add_op<unsigned>(OpType::CnX, {1, 2, 3, 4, 5, 6, 7, 8, 9});

    c.add_op<unsigned>(OpType::CnX, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    c.add_barrier({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    c.add_measure(Qubit(0), Bit(0));
    c.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0}, 1);
    REQUIRE(c.count_n_qubit_gates(0) == 0);
    REQUIRE(c.count_n_qubit_gates(1) == 11);
    REQUIRE(c.count_n_qubit_gates(2) == 9);
    REQUIRE(c.count_n_qubit_gates(3) == 8);
    REQUIRE(c.count_n_qubit_gates(4) == 7);
    REQUIRE(c.count_n_qubit_gates(5) == 6);
    REQUIRE(c.count_n_qubit_gates(6) == 5);
    REQUIRE(c.count_n_qubit_gates(7) == 4);
    REQUIRE(c.count_n_qubit_gates(8) == 3);
    REQUIRE(c.count_n_qubit_gates(9) == 2);
    REQUIRE(c.count_n_qubit_gates(10) == 1);
  }
}
SCENARIO("Vertices in order") {
  GIVEN("A circuit with 3 qubits and 6 operations") {
    Circuit c(3);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_op<unsigned>(OpType::T, {0});
    c.add_op<unsigned>(OpType::CY, {1, 2});
    c.add_op<unsigned>(OpType::S, {2});
    c.add_op<unsigned>(OpType::CZ, {0, 1});
    VertexVec vertices = c.vertices_in_order();
    unsigned n_vertices = vertices.size();
    CHECK(n_vertices == 3 + 6 + 3);
    unsigned h_pos = 0, cx_pos = 0, t_pos = 0, cy_pos = 0, s_pos = 0,
             cz_pos = 0;
    unsigned n_inp = 0, n_out = 0;
    for (unsigned i = 0; i < n_vertices; i++) {
      OpType optype = c.get_OpType_from_Vertex(vertices[i]);
      switch (optype) {
        case OpType::H:
          h_pos = i;
          break;
        case OpType::CX:
          cx_pos = i;
          break;
        case OpType::T:
          t_pos = i;
          break;
        case OpType::CY:
          cy_pos = i;
          break;
        case OpType::S:
          s_pos = i;
          break;
        case OpType::CZ:
          cz_pos = i;
          break;
        case OpType::Input:
          n_inp++;
          break;
        case OpType::Output:
          n_out++;
          break;
        default:
          CHECK(!"Unexpected operation");
      }
    }
    CHECK(n_inp == 3);
    CHECK(n_out == 3);
    CHECK(h_pos < cx_pos);
    CHECK(cx_pos < t_pos);
    CHECK(cx_pos < cy_pos);
    CHECK(t_pos < cz_pos);
    CHECK(cy_pos < s_pos);
    CHECK(cy_pos < cz_pos);
  }
}

SCENARIO("Checking circuit graphviz output") {
  GIVEN("A simple circuit") {
    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});

    auto out = c.to_graphviz_str();
    std::string exp_out =
        "digraph G {\n"
        "{ rank = same\n"
        "0 2 }\n"
        "{ rank = same\n"
        "1 3 }\n"
        "0 [label = \"Input, 0\"];\n"
        "1 [label = \"Output, 1\"];\n"
        "2 [label = \"Input, 2\"];\n"
        "3 [label = \"Output, 3\"];\n"
        "4 [label = \"CX, 4\"];\n"
        "0 -> 4 [label =  \"0, 0\"];\n"
        "4 -> 1 [label =  \"0, 0\"];\n"
        "2 -> 4 [label =  \"0, 1\"];\n"
        "4 -> 3 [label =  \"1, 0\"];\n"
        "}";
    REQUIRE(out == exp_out);
  }
}

SCENARIO("Testing get_linear_edge") {
  Circuit circ(1, 1);
  Vertex v =
      circ.add_conditional_gate<unsigned>(OpType::Rx, {0.2}, {0}, {0}, 1);
  Edge boolean_edge = circ.get_nth_in_edge(v, 0);
  Vertex c_out = circ.c_outputs()[0];
  Edge classical_edge = circ.get_nth_in_edge(c_out, 0);
  Edge quantum_edge = circ.get_nth_in_edge(v, 1);

  REQUIRE(circ.get_linear_edge(boolean_edge) == classical_edge);
  REQUIRE(circ.get_linear_edge(classical_edge) == classical_edge);
  REQUIRE(circ.get_linear_edge(quantum_edge) == quantum_edge);
}

SCENARIO("Test replacing wire swaps") {
  Circuit circ(7);
  std::vector<Qubit> qreg = circ.all_qubits();
  // Add SWAP gates
  circ.add_op<unsigned>(OpType::SWAP, {0, 1});
  circ.add_op<unsigned>(OpType::SWAP, {1, 3});
  circ.add_op<unsigned>(OpType::SWAP, {3, 4});
  circ.add_op<unsigned>(OpType::SWAP, {2, 5});

  qubit_map_t correct_perm = {{qreg[0], qreg[4]}, {qreg[1], qreg[0]},
                              {qreg[2], qreg[5]}, {qreg[3], qreg[1]},
                              {qreg[4], qreg[3]}, {qreg[5], qreg[2]},
                              {qreg[6], qreg[6]}};
  const Eigen::MatrixXcd u = tket_sim::get_unitary(circ);
  // Replace SWAPS
  REQUIRE(circ.replace_SWAPs());
  REQUIRE(circ.n_gates() == 0);
  REQUIRE(circ.implicit_qubit_permutation() == correct_perm);
  const Eigen::MatrixXcd v = tket_sim::get_unitary(circ);
  REQUIRE(u.isApprox(v, ERR_EPS));
  // Replace wire swaps
  // perm in cycle notation (0, 4, 3, 1), (2, 5), (6), hence need 4 swaps
  circ.replace_all_implicit_wire_swaps();
  REQUIRE(!circ.has_implicit_wireswaps());
  REQUIRE(circ.n_gates() == 4);
  const Eigen::MatrixXcd w = tket_sim::get_unitary(circ);
  REQUIRE(u.isApprox(w, ERR_EPS));
}

SCENARIO("Test replacing TK2 swaps") {
  Circuit circ(2);
  circ.add_op<unsigned>(OpType::TK2, {0.1, 0.2, 0.3}, {0, 1});
  GIVEN("case 1") {
    circ.add_op<unsigned>(OpType::TK2, {0.5, 0.5, 0.5}, {0, 1});
  }
  GIVEN("case 2") {
    circ.add_op<unsigned>(OpType::TK2, {1.5, 1.5, 1.5}, {0, 1});
  }
  GIVEN("case 3") {
    circ.add_op<unsigned>(OpType::TK2, {2.5, 2.5, 2.5}, {0, 1});
  }
  GIVEN("case 4") {
    circ.add_op<unsigned>(OpType::TK2, {3.5, 3.5, 3.5}, {0, 1});
  }
  const Eigen::MatrixXcd u = tket_sim::get_unitary(circ);
  circ.replace_SWAPs(false);
  REQUIRE(circ.n_gates() == 2);
  circ.replace_SWAPs(true);
  REQUIRE(circ.n_gates() == 1);
  const Eigen::MatrixXcd v = tket_sim::get_unitary(circ);
  REQUIRE(u.isApprox(v, ERR_EPS));
}

SCENARIO("check edge type in rewire function") {
  GIVEN("valid edge type") {
    Circuit c(2);
    Vertex v = c.add_op<unsigned>(OpType::CX, {0, 1});

    EdgeVec ins;
    for (const Vertex& i : c.q_inputs()) {
      ins.push_back(c.get_nth_out_edge(i, 0));
    }

    op_signature_t types = {EdgeType::Quantum, EdgeType::Quantum};

    c.rewire(v, ins, types);
  }

  GIVEN("invalid edge type") {
    Circuit c(2);
    Vertex v = c.add_op<unsigned>(OpType::CX, {0, 1});

    EdgeVec ins;
    for (const Vertex& i : c.q_inputs()) {
      ins.push_back(c.get_nth_out_edge(i, 0));
    }

    op_signature_t types = {EdgeType::Classical, EdgeType::Classical};

    REQUIRE_THROWS_AS(c.rewire(v, ins, types), CircuitInvalidity);
  }
}
void check_conditional_circuit(Circuit& c) {
  // Confirm that the DAG is constructed appropriately by checking
  // in and out edges of all vertices
  std::vector<Vertex> vertices = c.all_vertices();
  // First check Quantum and Classical input and output vertices
  // have expected number of out edges in interesting cases
  REQUIRE(c.n_out_edges_of_type(vertices[8], EdgeType::Classical) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[8], EdgeType::Boolean) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[10], EdgeType::Classical) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[10], EdgeType::Boolean) == 2);
  REQUIRE(c.n_out_edges_of_type(vertices[12], EdgeType::Classical) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[12], EdgeType::Boolean) == 2);
  REQUIRE(c.n_out_edges_of_type(vertices[14], EdgeType::Classical) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[14], EdgeType::Boolean) == 2);

  // Conditional gate 0
  REQUIRE(c.n_in_edges_of_type(vertices[18], EdgeType::Quantum) == 1);
  REQUIRE(c.n_in_edges_of_type(vertices[18], EdgeType::Classical) == 0);
  REQUIRE(c.n_in_edges_of_type(vertices[18], EdgeType::Boolean) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[18], EdgeType::Quantum) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[18], EdgeType::Classical) == 0);
  REQUIRE(c.n_out_edges_of_type(vertices[18], EdgeType::Boolean) == 0);
  Op_ptr op_0 = c.get_Op_ptr_from_Vertex(vertices[18]);
  const Conditional& con_0 = static_cast<const Conditional&>(*op_0);
  REQUIRE(con_0.get_type() == OpType::Conditional);
  Op_ptr barrier_0 = con_0.get_op();
  REQUIRE(barrier_0->get_type() == OpType::Barrier);
  op_signature_t sig_0 = {EdgeType::Quantum};
  REQUIRE(barrier_0->get_signature() == sig_0);

  // Conditional gate 1
  REQUIRE(c.n_in_edges_of_type(vertices[19], EdgeType::Quantum) == 2);
  REQUIRE(c.n_in_edges_of_type(vertices[19], EdgeType::Classical) == 1);
  REQUIRE(c.n_in_edges_of_type(vertices[19], EdgeType::Boolean) == 2);
  REQUIRE(c.n_out_edges_of_type(vertices[19], EdgeType::Quantum) == 2);
  REQUIRE(c.n_out_edges_of_type(vertices[19], EdgeType::Classical) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[19], EdgeType::Boolean) == 0);
  Op_ptr op_1 = c.get_Op_ptr_from_Vertex(vertices[19]);
  const Conditional& con_1 = static_cast<const Conditional&>(*op_1);
  REQUIRE(con_1.get_type() == OpType::Conditional);
  Op_ptr barrier_1 = con_1.get_op();
  REQUIRE(barrier_1->get_type() == OpType::Barrier);
  op_signature_t sig_1 = {
      EdgeType::Quantum, EdgeType::Quantum, EdgeType::Classical};
  REQUIRE(barrier_1->get_signature() == sig_1);

  // Conditional gate 2
  REQUIRE(c.n_in_edges_of_type(vertices[20], EdgeType::Quantum) == 1);
  REQUIRE(c.n_in_edges_of_type(vertices[20], EdgeType::Classical) == 1);
  REQUIRE(c.n_in_edges_of_type(vertices[20], EdgeType::Boolean) == 2);
  REQUIRE(c.n_out_edges_of_type(vertices[20], EdgeType::Quantum) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[20], EdgeType::Classical) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[20], EdgeType::Boolean) == 0);
  Op_ptr op_2 = c.get_Op_ptr_from_Vertex(vertices[20]);
  const Conditional& con_2 = static_cast<const Conditional&>(*op_2);
  REQUIRE(con_2.get_type() == OpType::Conditional);
  Op_ptr barrier_2 = con_2.get_op();
  REQUIRE(barrier_2->get_type() == OpType::Barrier);
  op_signature_t sig_2 = {EdgeType::Quantum, EdgeType::Classical};
  REQUIRE(barrier_2->get_signature() == sig_2);

  // Conditional gate 3
  REQUIRE(c.n_in_edges_of_type(vertices[21], EdgeType::Quantum) == 2);
  REQUIRE(c.n_in_edges_of_type(vertices[21], EdgeType::Classical) == 3);
  REQUIRE(c.n_in_edges_of_type(vertices[21], EdgeType::Boolean) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[21], EdgeType::Quantum) == 2);
  REQUIRE(c.n_out_edges_of_type(vertices[21], EdgeType::Classical) == 3);
  REQUIRE(c.n_out_edges_of_type(vertices[21], EdgeType::Boolean) == 2);
  Op_ptr op_3 = c.get_Op_ptr_from_Vertex(vertices[21]);
  const Conditional& con_3 = static_cast<const Conditional&>(*op_3);
  REQUIRE(con_3.get_type() == OpType::Conditional);
  Op_ptr barrier_3 = con_3.get_op();
  REQUIRE(barrier_3->get_type() == OpType::Barrier);
  op_signature_t sig_3 = {
      EdgeType::Quantum, EdgeType::Quantum, EdgeType::Classical,
      EdgeType::Classical, EdgeType::Classical};
  REQUIRE(barrier_3->get_signature() == sig_3);

  // Conditional gate 4
  REQUIRE(c.n_in_edges_of_type(vertices[22], EdgeType::Quantum) == 2);
  REQUIRE(c.n_in_edges_of_type(vertices[22], EdgeType::Classical) == 1);
  REQUIRE(c.n_in_edges_of_type(vertices[22], EdgeType::Boolean) == 2);
  REQUIRE(c.n_out_edges_of_type(vertices[22], EdgeType::Quantum) == 2);
  REQUIRE(c.n_out_edges_of_type(vertices[22], EdgeType::Classical) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[22], EdgeType::Boolean) == 0);
  Op_ptr op_4 = c.get_Op_ptr_from_Vertex(vertices[22]);
  const Conditional& con_4 = static_cast<const Conditional&>(*op_4);
  REQUIRE(con_4.get_type() == OpType::Conditional);
  Op_ptr barrier_4 = con_4.get_op();
  REQUIRE(barrier_4->get_type() == OpType::Barrier);
  op_signature_t sig_4 = {
      EdgeType::Quantum, EdgeType::Quantum, EdgeType::Classical};
  REQUIRE(barrier_4->get_signature() == sig_4);

  // Conditional gate 5
  REQUIRE(c.n_in_edges_of_type(vertices[23], EdgeType::Quantum) == 4);
  REQUIRE(c.n_in_edges_of_type(vertices[23], EdgeType::Classical) == 3);
  REQUIRE(c.n_in_edges_of_type(vertices[23], EdgeType::Boolean) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[23], EdgeType::Quantum) == 4);
  REQUIRE(c.n_out_edges_of_type(vertices[23], EdgeType::Classical) == 3);
  REQUIRE(c.n_out_edges_of_type(vertices[23], EdgeType::Boolean) == 0);
  Op_ptr op_5 = c.get_Op_ptr_from_Vertex(vertices[23]);
  const Conditional& con_5 = static_cast<const Conditional&>(*op_5);
  REQUIRE(con_5.get_type() == OpType::Conditional);
  Op_ptr barrier_5 = con_5.get_op();
  REQUIRE(barrier_5->get_type() == OpType::Barrier);
  op_signature_t sig_5 = {EdgeType::Quantum,   EdgeType::Quantum,
                          EdgeType::Quantum,   EdgeType::Quantum,
                          EdgeType::Classical, EdgeType::Classical,
                          EdgeType::Classical};
  REQUIRE(barrier_5->get_signature() == sig_5);

  // Conditional gate 6
  REQUIRE(c.n_in_edges_of_type(vertices[25], EdgeType::Quantum) == 4);
  REQUIRE(c.n_in_edges_of_type(vertices[25], EdgeType::Classical) == 3);
  REQUIRE(c.n_in_edges_of_type(vertices[25], EdgeType::Boolean) == 1);
  REQUIRE(c.n_out_edges_of_type(vertices[25], EdgeType::Quantum) == 4);
  REQUIRE(c.n_out_edges_of_type(vertices[25], EdgeType::Classical) == 3);
  REQUIRE(c.n_out_edges_of_type(vertices[25], EdgeType::Boolean) == 0);
  Op_ptr op_6 = c.get_Op_ptr_from_Vertex(vertices[25]);
  const Conditional& con_6 = static_cast<const Conditional&>(*op_6);
  REQUIRE(con_6.get_type() == OpType::Conditional);
  Op_ptr barrier_6 = con_6.get_op();
  REQUIRE(barrier_6->get_type() == OpType::Barrier);
  op_signature_t sig_6 = {EdgeType::Quantum,   EdgeType::Quantum,
                          EdgeType::Quantum,   EdgeType::Quantum,
                          EdgeType::Classical, EdgeType::Classical,
                          EdgeType::Classical};
  REQUIRE(barrier_6->get_signature() == sig_6);
}

SCENARIO("Check Circuit::add_conditional_barrier.") {
  GIVEN(
      "Add various forms of valid conditional barrier using the unsigned "
      "constructor.") {
    Circuit c(4, 5);
    std::vector<unsigned> c_empty = {};
    std::vector<unsigned> c_0 = {0};
    std::vector<unsigned> c_3 = {3};
    std::vector<unsigned> c_4 = {4};
    std::vector<unsigned> c_01 = {0, 1};
    std::vector<unsigned> c_02 = {0, 2};
    std::vector<unsigned> c_12 = {1, 2};
    std::vector<unsigned> c_13 = {1, 3};
    std::vector<unsigned> c_012 = {0, 1, 2};
    std::vector<unsigned> c_0123 = {0, 1, 2, 3};

    c.add_conditional_barrier(c_0, c_empty, c_0, 0, "");
    c.add_conditional_barrier(c_01, c_0, c_12, 1, "");
    c.add_conditional_barrier(c_0, c_0, c_12, 1, "");
    c.add_conditional_barrier(c_13, c_012, c_3, 0, "test");
    c.add_conditional_barrier(c_02, c_0, c_12, 1, "test1");
    c.add_conditional_barrier(c_0123, c_012, c_3, 0, "test1");
    c.add_measure(3, 4);
    c.add_conditional_barrier(c_0123, c_012, c_4, 0, "test2");
    check_conditional_circuit(c);
  }
  GIVEN(
      "Add various forms of valid conditional barrier using the  unitid "
      "constructor.") {
    Circuit c(4, 5);
    c.add_conditional_barrier({Qubit(0)}, {Bit(0)}, 0, "");
    c.add_conditional_barrier(
        {Qubit(0), Qubit(1), Bit(0)}, {Bit(1), Bit(2)}, 1, "");
    c.add_conditional_barrier({Qubit(0), Bit(0)}, {Bit(1), Bit(2)}, 1, "");
    c.add_conditional_barrier(
        {Qubit(1), Qubit(3), Bit(0), Bit(1), Bit(2)}, {Bit(3)}, 0, "test");
    c.add_conditional_barrier(
        {Qubit(0), Qubit(2), Bit(0)}, {Bit(1), Bit(2)}, 1, "test1");
    c.add_conditional_barrier(
        {Qubit(0), Qubit(1), Qubit(2), Qubit(3), Bit(0), Bit(1), Bit(2)},
        {Bit(3)}, 0, "test1");
    c.add_measure(Qubit(3), Bit(4));
    c.add_conditional_barrier(
        {Qubit(0), Qubit(1), Qubit(2), Qubit(3), Bit(0), Bit(1), Bit(2)},
        {Bit(4)}, 0, "test2");
    check_conditional_circuit(c);
  }
}

SCENARIO("Check decompose_boxes_recursively") {
  Circuit circ(2);
  Circuit c0(1);
  Eigen::Matrix2cd m;
  m << 0, -1, 1, 0;
  Unitary1qBox u1box(m);
  c0.add_box(u1box, {0});
  CircBox cbox(c0);
  circ.add_box(cbox, {0});
  circ.add_box(u1box, {0});
  Op_ptr op = get_op_ptr(OpType::X);
  QControlBox qcbox(op);
  circ.add_box(qcbox, {0, 1}, "opgroup1");
  circ.add_box(qcbox, {0, 1}, "opgroup2");
  circ.decompose_boxes_recursively({OpType::Unitary1qBox}, {"opgroup1"});
  std::vector<Command> cmds = circ.get_commands();
  REQUIRE(cmds.size() == 4);
  REQUIRE(cmds[0].get_op_ptr()->get_type() == OpType::Unitary1qBox);
  REQUIRE(cmds[1].get_op_ptr()->get_type() == OpType::Unitary1qBox);
  REQUIRE(cmds[2].get_op_ptr()->get_type() == OpType::QControlBox);
  REQUIRE(cmds[3].get_op_ptr()->get_type() == OpType::CX);
}

SCENARIO("Finding subcircuits") {
  GIVEN("A circuit with two Clifford subcircuits") {
    Circuit c(4);
    c.add_op<unsigned>(OpType::T, {0});
    Vertex cx0 = c.add_op<unsigned>(OpType::CX, {0, 1});  // (0)
    Vertex cy0 = c.add_op<unsigned>(OpType::CY, {1, 2});  // (0)
    Vertex cz0 = c.add_op<unsigned>(OpType::CZ, {0, 1});  // (0)
    c.add_op<unsigned>(OpType::TK2, {0.1, 0.2, 0.3}, {2, 3});
    c.add_op<unsigned>(OpType::T, {1});
    Vertex cx1 = c.add_op<unsigned>(OpType::CX, {1, 2});  // (1)
    Vertex h1 = c.add_op<unsigned>(OpType::H, {2});       // (1)
    Vertex cy1 = c.add_op<unsigned>(OpType::CY, {2, 3});  // (1)
    c.add_op<unsigned>(OpType::TK2, {0.1, 0.2, 0.3}, {1, 2});

    VertexSet expected0{cx0, cy0, cz0};
    VertexSet expected1{cx1, h1, cy1};

    std::vector<VertexSet> subcircuits =
        c.get_subcircuits([](Op_ptr op) { return op->is_clifford(); });
    REQUIRE(subcircuits.size() == 2);
    CHECK(subcircuits[0] == expected0);
    CHECK(subcircuits[1] == expected1);
  }
  GIVEN("A more complex example with more than one solution") {
    Circuit c(5);
    c.add_op<unsigned>(OpType::H, {0});
    c.add_op<unsigned>(OpType::X, {1});
    c.add_op<unsigned>(OpType::T, {2});
    c.add_op<unsigned>(OpType::Y, {3});
    c.add_op<unsigned>(OpType::Z, {4});
    c.add_op<unsigned>(OpType::CX, {2, 3});
    c.add_op<unsigned>(OpType::T, {2});
    c.add_op<unsigned>(OpType::S, {3});
    c.add_op<unsigned>(OpType::SX, {1});
    c.add_op<unsigned>(OpType::V, {2});
    c.add_op<unsigned>(OpType::Vdg, {3});
    c.add_op<unsigned>(OpType::CY, {1, 3});
    c.add_op<unsigned>(OpType::CZ, {3, 4});
    c.add_op<unsigned>(OpType::SWAP, {2, 3});
    c.add_op<unsigned>(OpType::Sdg, {4});
    c.add_op<unsigned>(OpType::ZZMax, {3, 0});
    c.add_op<unsigned>(OpType::SXdg, {0});
    std::vector<VertexSet> subcircuits =
        c.get_subcircuits([](Op_ptr op) { return op->is_clifford(); });
    REQUIRE(subcircuits.size() == 2);
    CHECK(subcircuits[0].size() + subcircuits[1].size() == 15);
  }
}

SCENARIO("Filter conditional commands") {
  // https://github.com/quantinuum/tket/issues/1726
  GIVEN("Circuit with a conditional after a measure") {
    Circuit c(1, 1);
    c.add_measure(0, 0);
    c.add_conditional_gate<unsigned>(OpType::X, {}, {0}, {0}, 1);
    std::list<Command> cmds = c.get_commands_of_type(OpType::Conditional);
    REQUIRE(cmds.size() == 1);
  }
}

}  // namespace test_Circ
}  // namespace tket
