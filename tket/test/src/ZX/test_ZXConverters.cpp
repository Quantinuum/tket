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

#include <boost/graph/adjacency_list.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../testutil.hpp"
#include "tket/Converters/Converters.hpp"
#include "tket/Gate/SymTable.hpp"
namespace tket {
namespace zx {
namespace test_ZXConverters {
SCENARIO("Check converting gates to spiders") {
  GIVEN("X") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::X, {0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec boundary = zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert input = boundary[0];
    ZXVert x = zx.neighbours(input)[0];
    REQUIRE(zx.get_zxtype(x) == ZXType::XSpider);
    PhasedGen x_gen = zx.get_vertex_ZXGen<PhasedGen>(x);
    REQUIRE(x_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(x_gen.get_param() == 1.);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE(zx.n_vertices() == 3);
    REQUIRE(zx.n_wires() == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Rx") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rx, 0.3, {0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec boundary = zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert input = boundary[0];
    ZXVert x = zx.neighbours(input)[0];
    REQUIRE(zx.get_zxtype(x) == ZXType::XSpider);
    PhasedGen x_gen = zx.get_vertex_ZXGen<PhasedGen>(x);
    REQUIRE(x_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(x_gen.get_param() == 0.3);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE(zx.n_vertices() == 3);
    REQUIRE(zx.n_wires() == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Z") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Z, {0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec boundary = zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert input = boundary[0];
    ZXVert z = zx.neighbours(input)[0];
    REQUIRE(zx.get_zxtype(z) == ZXType::ZSpider);
    PhasedGen z_gen = zx.get_vertex_ZXGen<PhasedGen>(z);
    REQUIRE(z_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(z_gen.get_param() == 1.);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE(zx.n_vertices() == 3);
    REQUIRE(zx.n_wires() == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Rz") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, 0.4, {0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec boundary = zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert input = boundary[0];
    ZXVert z = zx.neighbours(input)[0];
    REQUIRE(zx.get_zxtype(z) == ZXType::ZSpider);
    PhasedGen z_gen = zx.get_vertex_ZXGen<PhasedGen>(z);
    REQUIRE(z_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(z_gen.get_param() == 0.4);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE(zx.n_vertices() == 3);
    REQUIRE(zx.n_wires() == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("H") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::H, {0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec boundary = zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert input = boundary[0];
    ZXVert h = zx.neighbours(input)[0];
    ZXGen_ptr h_ptr = zx.get_vertex_ZXGen_ptr(h);
    REQUIRE(h_ptr->get_type() == ZXType::Hbox);
    REQUIRE(h_ptr->get_qtype() == QuantumType::Quantum);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 0.5));
    REQUIRE(zx.n_vertices() == 3);
    REQUIRE(zx.n_wires() == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("CX") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec boundary = zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert input0 = boundary[0];
    ZXVert input1 = boundary[1];
    ZXVert ctr = zx.neighbours(input0)[0];
    ZXVert targ = zx.neighbours(input1)[0];
    std::optional<Wire> w = zx.wire_between(ctr, targ);
    REQUIRE(zx.get_zxtype(ctr) == ZXType::ZSpider);
    REQUIRE(zx.get_zxtype(targ) == ZXType::XSpider);
    PhasedGen ctr_gen = zx.get_vertex_ZXGen<PhasedGen>(ctr);
    PhasedGen targ_gen = zx.get_vertex_ZXGen<PhasedGen>(targ);
    REQUIRE(ctr_gen.get_param() == 0.);
    REQUIRE(targ_gen.get_param() == 0.);
    REQUIRE(ctr_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(targ_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 2));
    REQUIRE(zx.n_vertices() == 6);
    REQUIRE(zx.n_wires() == 5);
    REQUIRE(zx.get_qtype(w.value()) == QuantumType::Quantum);
    REQUIRE(zx.get_wire_type(w.value()) == ZXWireType::Basic);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("CZ") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::CZ, {0, 1});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec boundary = zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert input0 = boundary[0];
    ZXVert input1 = boundary[1];
    ZXVert ctr = zx.neighbours(input0)[0];
    ZXVert targ = zx.neighbours(input1)[0];
    std::optional<Wire> w = zx.wire_between(ctr, targ);
    REQUIRE(zx.get_zxtype(ctr) == ZXType::ZSpider);
    REQUIRE(zx.get_zxtype(targ) == ZXType::ZSpider);
    PhasedGen ctr_gen = zx.get_vertex_ZXGen<PhasedGen>(ctr);
    PhasedGen targ_gen = zx.get_vertex_ZXGen<PhasedGen>(targ);
    REQUIRE(ctr_gen.get_param() == 0.);
    REQUIRE(targ_gen.get_param() == 0.);
    REQUIRE(ctr_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(targ_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE(zx.n_vertices() == 6);
    REQUIRE(zx.n_wires() == 5);
    REQUIRE(zx.get_qtype(w.value()) == QuantumType::Quantum);
    REQUIRE(zx.get_wire_type(w.value()) == ZXWireType::H);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Measure") {
    Circuit circ(1, 1);
    circ.add_op<unsigned>(OpType::Measure, {0, 0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec q_boundary = zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVertVec c_boundary =
        zx.get_boundary(ZXType::Input, QuantumType::Classical);
    ZXVert input0 = q_boundary[0];
    ZXVert input1 = c_boundary[0];
    ZXVert vert0 = zx.neighbours(input0)[0];
    ZXVert vert1 = zx.neighbours(input1)[0];
    REQUIRE(zx.get_zxtype(vert0) == ZXType::ZSpider);
    REQUIRE(zx.get_zxtype(vert1) == ZXType::ZSpider);
    PhasedGen gen0 = zx.get_vertex_ZXGen<PhasedGen>(vert0);
    PhasedGen gen1 = zx.get_vertex_ZXGen<PhasedGen>(vert1);
    REQUIRE(zx.degree(vert0) == 3);
    REQUIRE(zx.degree(vert1) == 1);
    REQUIRE(gen0.get_param() == 0.);
    REQUIRE(gen1.get_param() == 0.);
    REQUIRE(gen0.get_qtype() == QuantumType::Classical);
    REQUIRE(gen1.get_qtype() == QuantumType::Classical);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE(zx.n_vertices() == 6);
    REQUIRE(zx.n_wires() == 4);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Reset") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Reset, {0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec in_boundary =
        zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVertVec out_boundary =
        zx.get_boundary(ZXType::Output, QuantumType::Quantum);
    ZXVert input = in_boundary[0];
    ZXVert output = out_boundary[0];
    ZXVert discard = zx.neighbours(input)[0];
    ZXVert init = zx.neighbours(output)[0];
    REQUIRE(zx.get_zxtype(discard) == ZXType::ZSpider);
    REQUIRE(zx.get_zxtype(init) == ZXType::XSpider);
    PhasedGen gen0 = zx.get_vertex_ZXGen<PhasedGen>(discard);
    PhasedGen gen1 = zx.get_vertex_ZXGen<PhasedGen>(init);
    REQUIRE(zx.degree(discard) == 1);
    REQUIRE(zx.degree(init) == 1);
    REQUIRE(gen0.get_param() == 0.);
    REQUIRE(gen1.get_param() == 0.);
    REQUIRE(gen0.get_qtype() == QuantumType::Classical);
    REQUIRE(gen1.get_qtype() == QuantumType::Quantum);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 0.5));
    REQUIRE(zx.n_vertices() == 4);
    REQUIRE(zx.n_wires() == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Collapse") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Collapse, {0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec boundary = zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert input = boundary[0];
    ZXVert z = zx.neighbours(input)[0];
    REQUIRE(zx.get_zxtype(z) == ZXType::ZSpider);
    PhasedGen z_gen = zx.get_vertex_ZXGen<PhasedGen>(z);
    REQUIRE(z_gen.get_qtype() == QuantumType::Classical);
    REQUIRE(z_gen.get_param() == 0.);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE(zx.n_vertices() == 3);
    REQUIRE(zx.n_wires() == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Barrier") {
    Circuit circ(1, 1);
    circ.add_barrier(std::vector<unsigned>{0}, std::vector<unsigned>{0});
    circ.add_barrier(std::vector<unsigned>{}, std::vector<unsigned>{0});
    circ.add_barrier(std::vector<unsigned>{0}, std::vector<unsigned>{});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec q_in_boundary =
        zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVertVec c_in_boundary =
        zx.get_boundary(ZXType::Input, QuantumType::Classical);
    ZXVert q_input = q_in_boundary[0];
    ZXVert q_next = zx.neighbours(q_input)[0];
    ZXVert c_input = c_in_boundary[0];
    ZXVert c_next = zx.neighbours(c_input)[0];
    ZXVertVec q_out_boundary =
        zx.get_boundary(ZXType::Output, QuantumType::Quantum);
    ZXVertVec c_out_boundary =
        zx.get_boundary(ZXType::Output, QuantumType::Classical);
    REQUIRE(q_out_boundary[0] == q_next);
    REQUIRE(c_out_boundary[0] == c_next);
    REQUIRE(zx.n_vertices() == 4);
    REQUIRE(zx.n_wires() == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Conditional Barrier") {
    Circuit circ(2, 2);
    circ.add_conditional_barrier(
        std::vector<unsigned>{0, 1}, std::vector<unsigned>{0},
        std::vector<unsigned>{1}, 1, "");
    REQUIRE_THROWS_AS(circuit_to_zx(circ), Unsupported);
  }
  GIVEN("noop") {
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::noop, {0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec q_in_boundary =
        zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert q_input = q_in_boundary[0];
    ZXVert q_next = zx.neighbours(q_input)[0];
    ZXVertVec q_out_boundary =
        zx.get_boundary(ZXType::Output, QuantumType::Quantum);
    REQUIRE(q_out_boundary[0] == q_next);
    REQUIRE(zx.n_vertices() == 2);
    REQUIRE(zx.n_wires() == 1);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Simple SWAP") {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec q_in_boundary =
        zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    Vertex q0_in = circ.get_in(Qubit(0));
    Vertex q1_in = circ.get_in(Qubit(1));
    Vertex q0_out = circ.get_out(Qubit(0));
    Vertex q1_out = circ.get_out(Qubit(1));
    auto q0_zx_in = bmap.right.find(q0_in)->second;
    auto q1_zx_in = bmap.right.find(q1_in)->second;
    auto q0_zx_out = bmap.right.find(q0_out)->second;
    auto q1_zx_out = bmap.right.find(q1_out)->second;
    REQUIRE(zx.neighbours(q0_zx_in)[0] == q1_zx_out);
    REQUIRE(zx.neighbours(q1_zx_in)[0] == q0_zx_out);
    REQUIRE(zx.n_vertices() == 4);
    REQUIRE(zx.n_wires() == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Multiple SWAPs") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::SWAP, {1, 2});
    circ.add_op<unsigned>(OpType::Rz, 0.1, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.2, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.3, {2});

    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);

    ZXVertVec q_in_boundary =
        zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVert q_input_0 = q_in_boundary[0];
    ZXVert q_next_0 = zx.neighbours(q_input_0)[0];
    ZXVert q_input_1 = q_in_boundary[1];
    ZXVert q_next_1 = zx.neighbours(q_input_1)[0];
    ZXVert q_input_2 = q_in_boundary[2];
    ZXVert q_next_2 = zx.neighbours(q_input_2)[0];

    REQUIRE(zx.get_zxtype(q_next_0) == ZXType::ZSpider);
    REQUIRE(zx.get_zxtype(q_next_1) == ZXType::ZSpider);
    REQUIRE(zx.get_zxtype(q_next_2) == ZXType::ZSpider);

    PhasedGen z0_gen = zx.get_vertex_ZXGen<PhasedGen>(q_next_0);
    PhasedGen z1_gen = zx.get_vertex_ZXGen<PhasedGen>(q_next_1);
    PhasedGen z2_gen = zx.get_vertex_ZXGen<PhasedGen>(q_next_2);

    REQUIRE(z0_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(z0_gen.get_param() == 0.3);

    REQUIRE(z1_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(z1_gen.get_param() == 0.1);

    REQUIRE(z2_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(z2_gen.get_param() == 0.2);

    REQUIRE(zx.n_vertices() == 9);
    REQUIRE(zx.n_wires() == 6);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Create") {
    Circuit circ(1);
    circ.qubit_create(Qubit(0));
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec input_boundary =
        zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    REQUIRE(input_boundary.size() == 0);
    ZXVertVec output_boundary =
        zx.get_boundary(ZXType::Output, QuantumType::Quantum);
    ZXVert output = output_boundary[0];
    ZXVert x = zx.neighbours(output)[0];
    REQUIRE(zx.get_zxtype(x) == ZXType::XSpider);
    PhasedGen x_gen = zx.get_vertex_ZXGen<PhasedGen>(x);
    REQUIRE(x_gen.get_qtype() == QuantumType::Quantum);
    REQUIRE(x_gen.get_param() == 0.);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 0.5));
    REQUIRE(zx.n_vertices() == 2);
    REQUIRE(zx.n_wires() == 1);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Discard") {
    Circuit circ(1);
    circ.qubit_discard(Qubit(0));
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    ZXVertVec input_boundary =
        zx.get_boundary(ZXType::Input, QuantumType::Quantum);
    ZXVertVec output_boundary =
        zx.get_boundary(ZXType::Output, QuantumType::Quantum);
    REQUIRE(output_boundary.size() == 0);
    ZXVert input = input_boundary[0];
    ZXVert z = zx.neighbours(input)[0];
    REQUIRE(zx.get_zxtype(z) == ZXType::ZSpider);
    PhasedGen z_gen = zx.get_vertex_ZXGen<PhasedGen>(z);
    REQUIRE(z_gen.get_qtype() == QuantumType::Classical);
    REQUIRE(z_gen.get_param() == 0.);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE(zx.n_vertices() == 2);
    REQUIRE(zx.n_wires() == 1);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Empty CircBox") {
    Circuit circ(3);
    Circuit inner(2);
    CircBox inner_box(inner);
    circ.add_box(inner_box, {1, 2});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 3);
    REQUIRE(zx.n_vertices() == 6);
    REQUIRE(zx.n_wires() == 3);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Nested CircBox") {
    Circuit circ(5);
    Circuit inner(4);
    Circuit inner_most(2);
    inner.add_op<unsigned>(OpType::Rx, 0.5, {0});
    inner.add_op<unsigned>(OpType::CX, {1, 2});
    inner_most.add_op<unsigned>(OpType::H, {1});
    CircBox inner_most_box(inner_most);
    inner.add_box(inner_most_box, {2, 3});
    CircBox inner_box(inner);
    circ.add_box(inner_box, {0, 1, 2, 3});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {4});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    for (unsigned i = 0; i < 5; i++) {
      Vertex q_in = circ.get_in(Qubit(i));
      Vertex q_out = circ.get_out(Qubit(i));
      auto q_zx_in = bmap.right.find(q_in)->second;
      auto q_zx_out = bmap.right.find(q_out)->second;
      ZXVert mid_v = zx.neighbours(q_zx_in)[0];
      ZXVertVec mid_v_nbs = zx.neighbours(mid_v);
      REQUIRE(
          std::find(mid_v_nbs.begin(), mid_v_nbs.end(), q_zx_out) !=
          mid_v_nbs.end());
      // Check type before cast to avoid casting errors
      REQUIRE(is_phase_type(zx.get_zxtype(mid_v)));
      PhasedGen mid_gen = zx.get_vertex_ZXGen<PhasedGen>(mid_v);
      switch (i) {
        case 0:
          REQUIRE(zx.degree(mid_v) == 2);
          REQUIRE(mid_gen.get_type() == ZXType::XSpider);
          REQUIRE(mid_gen.get_qtype() == QuantumType::Quantum);
          REQUIRE(mid_gen.get_param() == 0.5);
          break;
        case 1:
          REQUIRE(zx.degree(mid_v) == 3);
          REQUIRE(mid_gen.get_type() == ZXType::ZSpider);
          REQUIRE(mid_gen.get_qtype() == QuantumType::Quantum);
          REQUIRE(mid_gen.get_param() == 0.);
          break;
        case 2:
          REQUIRE(zx.degree(mid_v) == 3);
          REQUIRE(mid_gen.get_type() == ZXType::XSpider);
          REQUIRE(mid_gen.get_qtype() == QuantumType::Quantum);
          REQUIRE(mid_gen.get_param() == 0.);
          break;
        case 3:
          REQUIRE(zx.degree(mid_v) == 2);
          REQUIRE(mid_gen.get_type() == ZXType::Hbox);
          REQUIRE(mid_gen.get_qtype() == QuantumType::Quantum);
          break;
        case 4:
          REQUIRE(zx.degree(mid_v) == 2);
          REQUIRE(mid_gen.get_type() == ZXType::ZSpider);
          REQUIRE(mid_gen.get_qtype() == QuantumType::Quantum);
          REQUIRE(mid_gen.get_param() == 0.5);
          break;
      }
    }
    REQUIRE(zx.n_vertices() == 15);
    REQUIRE(zx.n_wires() == 11);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 5);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 5);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Quantum) == 2);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Quantum) == 2);
    REQUIRE(zx.count_vertices(ZXType::Hbox, QuantumType::Quantum) == 1);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Conditional gate") {
    Circuit circ(1, 1);
    circ.add_conditional_gate<unsigned>(OpType::H, {}, {0}, {0}, 1);
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 21);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Quantum) == 6);
    REQUIRE(zx.count_vertices(ZXType::Triangle, QuantumType::Quantum) == 4);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Classical) == 0);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Classical) == 3);
    REQUIRE(zx.count_vertices(ZXType::Hbox, QuantumType::Quantum) == 1);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Conditional measure") {
    Circuit circ(1, 1);
    circ.add_conditional_gate<unsigned>(OpType::Measure, {}, {0, 0}, {0}, 0);
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 37);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Quantum) == 6);
    REQUIRE(zx.count_vertices(ZXType::Triangle, QuantumType::Quantum) == 4);
    REQUIRE(zx.count_vertices(ZXType::Triangle, QuantumType::Classical) == 4);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Classical) == 7);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Classical) == 9);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Conditional CircBox") {
    Circuit circ(2, 2);
    Circuit inner(2);
    inner.add_op<unsigned>(OpType::H, {0});
    inner.add_op<unsigned>(OpType::H, {1});
    CircBox inner_box(inner);
    Op_ptr inner_op = std::make_shared<CircBox>(inner_box);
    Op_ptr con_op = std::make_shared<Conditional>(inner_op, 2, 3);
    circ.add_op<UnitID>(
        con_op, {Bit(0), Bit(1), Qubit(0), Qubit(1)}, std::nullopt);
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 47);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 2);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Classical) == 2);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 2);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Classical) == 2);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Quantum) == 12);
    REQUIRE(zx.count_vertices(ZXType::Triangle, QuantumType::Quantum) == 8);
    REQUIRE(zx.count_vertices(ZXType::Triangle, QuantumType::Classical) == 3);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Classical) == 0);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Quantum) == 6);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Classical) == 8);
    REQUIRE(zx.count_vertices(ZXType::Hbox, QuantumType::Quantum) == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Nested conditional gate") {
    Circuit circ(1, 3);
    Op_ptr cond = std::make_shared<Conditional>(get_op_ptr(OpType::H), 1, 1);
    Op_ptr condcond = std::make_shared<Conditional>(cond, 2, 1);
    circ.add_op<UnitID>(
        condcond, {Bit(0), Bit(1), Bit(2), Qubit(0)}, std::nullopt);
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 35);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Classical) == 3);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Classical) == 3);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Quantum) == 6);
    REQUIRE(zx.count_vertices(ZXType::Triangle, QuantumType::Quantum) == 4);
    REQUIRE(zx.count_vertices(ZXType::Triangle, QuantumType::Classical) == 4);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Classical) == 8);
    REQUIRE(zx.count_vertices(ZXType::Hbox, QuantumType::Quantum) == 1);
    REQUIRE_NOTHROW(zx.check_validity());
  }
  GIVEN("Consecutive conditional gates") {
    // Test that only one copy spider should be connected to the classical input
    Circuit circ(1, 1);
    circ.add_conditional_gate<unsigned>(OpType::H, {}, {0}, {0}, 1);
    circ.add_conditional_gate<unsigned>(OpType::H, {}, {0}, {0}, 1);
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 37);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Quantum) == 12);
    REQUIRE(zx.count_vertices(ZXType::Triangle, QuantumType::Quantum) == 8);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Classical) == 0);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Quantum) == 6);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Classical) == 5);
    REQUIRE(zx.count_vertices(ZXType::Hbox, QuantumType::Quantum) == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }
}

SCENARIO("Check converting circuits to diagrams") {
  GIVEN("A empty circuit") {
    Circuit circ;
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 0);
    REQUIRE(zx.n_wires() == 0);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE_NOTHROW(zx.check_validity());
  }

  GIVEN("A circuit with no gates") {
    Circuit circ(3, 1);
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);
    circ.add_phase(ea);
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 8);
    REQUIRE(zx.n_wires() == 4);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Classical) == 1);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE_NOTHROW(zx.check_validity());
  }

  GIVEN("A circuit with spiderless ops") {
    Circuit circ(3, 2);
    circ.add_op<unsigned>(OpType::X, {2});
    circ.add_barrier(std::vector<unsigned>{0, 2}, std::vector<unsigned>{0, 1});
    circ.add_op<unsigned>(OpType::noop, {0});
    circ.add_op<unsigned>(OpType::noop, {0});
    circ.add_barrier({0, 1});
    circ.add_op<unsigned>(OpType::noop, {1});
    circ.add_barrier(std::vector<unsigned>{}, std::vector<unsigned>{0});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::SWAP, {0, 1});
    circ.add_op<unsigned>(OpType::X, {1});
    circ.add_op<unsigned>(OpType::Measure, {0, 0});
    circ.add_barrier(std::vector<unsigned>{1}, std::vector<unsigned>{0, 1});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 14);
    REQUIRE(zx.n_wires() == 9);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Classical) == 2);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Classical) == 2);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Quantum) == 2);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Classical) == 2);
    REQUIRE_NOTHROW(zx.check_validity());
  }

  GIVEN("A simple circuit") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::X, {0});
    circ.add_op<unsigned>(OpType::Rz, 0.5, {1});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::CZ, {1, 2});
    circ.add_op<unsigned>(OpType::CZ, {1, 0});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 15);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Classical) == 0);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Classical) == 0);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Quantum) == 2);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Quantum) == 6);
    REQUIRE(zx.count_vertices(ZXType::Hbox, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_wires(ZXWireType::H) == 2);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE_NOTHROW(zx.check_validity());
  }

  GIVEN("A simple symbolic circuit") {
    Sym a = SymTable::fresh_symbol("a");
    Expr ea(a);
    Sym b = SymTable::fresh_symbol("b");
    Expr eb(b);
    Circuit circ(1);
    circ.add_op<unsigned>(OpType::Rz, ea, {0});
    circ.add_phase(eb);
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 3);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Quantum) == 1);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE_NOTHROW(zx.check_validity());
  }

  GIVEN("A simple circuit with projective operations") {
    Circuit circ(3, 1);
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.add_op<unsigned>(OpType::Measure, {0, 0});
    circ.add_op<unsigned>(OpType::X, {2});
    circ.add_op<unsigned>(OpType::Reset, {2});
    circ.add_op<unsigned>(OpType::Collapse, {1});
    ZXDiagram zx;
    boost::bimap<ZXVert, Vertex> bmap;
    std::tie(zx, bmap) = circuit_to_zx(circ);
    REQUIRE(zx.n_vertices() == 16);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::Input, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::Output, QuantumType::Classical) == 1);
    REQUIRE(zx.count_vertices(ZXType::XSpider, QuantumType::Quantum) == 3);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Quantum) == 1);
    REQUIRE(zx.count_vertices(ZXType::ZSpider, QuantumType::Classical) == 4);
    REQUIRE(test_equiv_expr_c(zx.get_scalar(), 1));
    REQUIRE_NOTHROW(zx.check_validity());
  }
}
}  // namespace test_ZXConverters
}  // namespace zx
}  // namespace tket
