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
#include <optional>
#include <string>

#include "tket/Converters/Converters.hpp"
#include "tket/Predicates/PassLibrary.hpp"
#include "tket/Transformations/Rebase.hpp"
#include "tket/ZX/Rewrite.hpp"

namespace tket {
namespace zx {
namespace test_ZXSimp {

SCENARIO("Testing graph state simplification") {
  /**
   * Diagram 1: testing simplification on graph states
   * This diagram follows from section A of:
   *  \ref https://arxiv.org/pdf/1902.03178.pdf
   **/
  ZXDiagram diag1(4, 4, 0, 0);
  ZXVertVec d1_in = diag1.get_boundary(ZXType::Input);
  ZXVertVec d1_out = diag1.get_boundary(ZXType::Output);

  ZXVert c11 = diag1.add_vertex(ZXType::ZSpider, 1.5);
  ZXVert c12 = diag1.add_vertex(ZXType::ZSpider, 0.5);
  ZXVert c13 = diag1.add_vertex(ZXType::ZSpider);
  ZXVert c14 = diag1.add_vertex(ZXType::XSpider);
  ZXVert c15 = diag1.add_vertex(ZXType::ZSpider, 0.25);
  ZXVert c21 = diag1.add_vertex(ZXType::ZSpider, 0.5);
  ZXVert c22 = diag1.add_vertex(ZXType::ZSpider);
  ZXVert c23 = diag1.add_vertex(ZXType::ZSpider);
  ZXVert c24 = diag1.add_vertex(ZXType::ZSpider, 0.25);
  ZXVert c25 = diag1.add_vertex(ZXType::ZSpider);
  ZXVert c31 = diag1.add_vertex(ZXType::XSpider);
  ZXVert c32 = diag1.add_vertex(ZXType::XSpider);
  ZXVert c33 = diag1.add_vertex(ZXType::ZSpider, 0.5);
  ZXVert c34 = diag1.add_vertex(ZXType::ZSpider, 0.5);
  ZXVert c35 = diag1.add_vertex(ZXType::XSpider);
  ZXVert c41 = diag1.add_vertex(ZXType::ZSpider);
  ZXVert c42 = diag1.add_vertex(ZXType::ZSpider);
  ZXVert c43 = diag1.add_vertex(ZXType::ZSpider, 1.5);
  ZXVert c44 = diag1.add_vertex(ZXType::XSpider, 1.0);
  ZXVert c45 = diag1.add_vertex(ZXType::ZSpider, 0.5);
  ZXVert c46 = diag1.add_vertex(ZXType::XSpider, 1.0);

  diag1.add_wire(d1_in[0], c11, ZXWireType::Basic);
  diag1.add_wire(c11, c12, ZXWireType::H);
  diag1.add_wire(c12, c13, ZXWireType::Basic);
  diag1.add_wire(c13, c41, ZXWireType::H);
  diag1.add_wire(c13, c14, ZXWireType::Basic);
  diag1.add_wire(c14, c42, ZXWireType::Basic);
  diag1.add_wire(c14, c15, ZXWireType::H);
  diag1.add_wire(c15, d1_out[0], ZXWireType::H);

  diag1.add_wire(d1_in[1], c21, ZXWireType::Basic);
  diag1.add_wire(c21, c22, ZXWireType::Basic);
  diag1.add_wire(c22, c31, ZXWireType::Basic);
  diag1.add_wire(c22, c23, ZXWireType::H);
  diag1.add_wire(c23, c32, ZXWireType::Basic);
  diag1.add_wire(c23, c24, ZXWireType::Basic);
  diag1.add_wire(c24, c25, ZXWireType::H);
  diag1.add_wire(c25, c35, ZXWireType::Basic);
  diag1.add_wire(d1_out[1], c25, ZXWireType::Basic);

  diag1.add_wire(d1_in[2], c31, ZXWireType::Basic);
  diag1.add_wire(c31, c32, ZXWireType::Basic);
  diag1.add_wire(c32, c33, ZXWireType::Basic);
  diag1.add_wire(c33, c34, ZXWireType::H);
  diag1.add_wire(c34, c35, ZXWireType::Basic);
  diag1.add_wire(c35, d1_out[2], ZXWireType::Basic);

  diag1.add_wire(d1_in[3], c41, ZXWireType::H);
  diag1.add_wire(c41, c42, ZXWireType::Basic);
  diag1.add_wire(c42, c43, ZXWireType::H);
  diag1.add_wire(c43, c44, ZXWireType::Basic);
  diag1.add_wire(c44, c45, ZXWireType::Basic);
  diag1.add_wire(c45, c46, ZXWireType::Basic);
  diag1.add_wire(c46, d1_out[3], ZXWireType::Basic);

  REQUIRE_NOTHROW(diag1.check_validity());

  /** Apply rewrites to diagram 1 to turn into a graph-like form **/

  Rewrite::red_to_green().apply(diag1);
  Rewrite::spider_fusion().apply(diag1);
  Rewrite::parallel_h_removal().apply(diag1);
  Rewrite::io_extension().apply(diag1);
  Rewrite::separate_boundaries().apply(diag1);

  /**
   * Graph simplification via Pauli & Clifford removal
   * We perform the full simpliciation procedure described in theorem 5.4
   * of \ref https://arxiv.org/pdf/1902.03178.pdf
   **/

  CHECK(Rewrite::remove_interior_cliffords().apply(diag1));
  CHECK_FALSE(
      Rewrite::extend_at_boundary_paulis().apply(
          diag1));  // If remove_interior_cliffords is exhaustive, this should
                    // not need to be applied
  CHECK(Rewrite::remove_interior_paulis().apply(diag1));
  // This example will have no gadgets to gadgetise
  CHECK_FALSE(Rewrite::gadgetise_interior_paulis().apply(diag1));

  CHECK_FALSE(Rewrite::parallel_h_removal().apply(diag1));
}

SCENARIO("Simplification of a paper example") {
  /**
   * This circuit is taken from Figure 1 of:
   *  \ref https://arxiv.org/pdf/1903.10477.pdf
   **/
  Circuit circ(5);
  circ.add_op<unsigned>(OpType::CCX, {0, 1, 4});
  circ.add_op<unsigned>(OpType::CCX, {2, 4, 3});
  circ.add_op<unsigned>(OpType::CCX, {0, 1, 4});
  Transforms::rebase_quil().apply(circ);
  ZXDiagram diag;
  boost::bimap<ZXVert, Vertex> bmap;
  std::tie(diag, bmap) = circuit_to_zx(circ);

  REQUIRE_NOTHROW(diag.check_validity());

  /** Obtain a graph-like form **/
  Rewrite::red_to_green().apply(diag);
  Rewrite::spider_fusion().apply(diag);
  Rewrite::parallel_h_removal().apply(diag);
  Rewrite::io_extension().apply(diag);
  Rewrite::separate_boundaries().apply(diag);

  /** Graph simplification via Pauli & Clifford removal **/
  CHECK(Rewrite::remove_interior_cliffords().apply(diag));
  CHECK(Rewrite::extend_at_boundary_paulis().apply(diag));
  CHECK(Rewrite::remove_interior_paulis().apply(diag));
  CHECK(Rewrite::gadgetise_interior_paulis().apply(diag));

  CHECK_FALSE(Rewrite::parallel_h_removal().apply(diag));
}

SCENARIO("Testing cases for internalising gadgets in MBQC") {
  // Semantic preservation tested in pytket (zx_diagram_test.py
  // test_internalise_gadgets)
  std::list<ZXType> axis_types = {ZXType::XY, ZXType::PX, ZXType::PY};
  std::list<ZXType> gadget_types = {ZXType::XY, ZXType::XZ, ZXType::YZ,
                                    ZXType::PX, ZXType::PY, ZXType::PZ};
  for (const ZXType& axis_basis : axis_types) {
    for (const ZXType& gadget_basis : gadget_types) {
      ZXDiagram diag(1, 1, 0, 0);
      ZXVert in = diag.get_boundary(ZXType::Input).at(0);
      ZXVert out = diag.get_boundary(ZXType::Output).at(0);
      ZXVert in_v = diag.add_clifford_vertex(ZXType::PX, false);
      ZXVert out_v = diag.add_clifford_vertex(ZXType::PX, false);
      ZXVert axis = is_Clifford_gen_type(axis_basis)
                        ? diag.add_clifford_vertex(axis_basis, false)
                        : diag.add_vertex(axis_basis, 0.25);
      ZXVert gadget = is_Clifford_gen_type(gadget_basis)
                          ? diag.add_clifford_vertex(gadget_basis, false)
                          : diag.add_vertex(gadget_basis, 0.25);
      diag.add_wire(in, in_v);
      diag.add_wire(in_v, axis, ZXWireType::H);
      diag.add_wire(axis, out_v, ZXWireType::H);
      diag.add_wire(out_v, out);
      diag.add_wire(axis, gadget, ZXWireType::H);
      bool changed = Rewrite::internalise_gadgets().apply(diag);
      CHECK(
          (axis_basis == ZXType::XY &&
           (gadget_basis == ZXType::XY || gadget_basis == ZXType::XZ)) ^
          changed);
    }
  }
}

SCENARIO("Testing for vertex reuse bug") {
  /**
   * The following circuits were shown to hit errors non-deterministically, with
   * the error likelihood varying with the device.
   *
   * At some point during extraction, a vertex would be removed from the graph,
   * but some pointers to it lingered. Later, if an input extension occurred,
   * new vertices would be added which may (depending on how boost was feeling
   * with memory) reuse the same memory locations, so the old pointers now
   * worked, meaning lookups in sets/functions would give false positives.
   */
  GIVEN("Offender 1") {
    Circuit circ(3);
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::CX, {2, 1});
    circ.add_op<unsigned>(OpType::Rz, 1.1, {1});
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CX, {2, 1});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::H, {2});
    circ.add_op<unsigned>(OpType::Rz, 0.970644, {1});
    circ.add_op<unsigned>(OpType::Rz, 0.9, {2});
    circ.add_op<unsigned>(OpType::H, {1});
    circ.add_op<unsigned>(OpType::H, {2});

    CompilationUnit cu(circ);
    ZXGraphlikeOptimisation()->apply(cu);
  }
  GIVEN("Offender 2") {
    Circuit circ(6);
    circ.add_op<unsigned>(OpType::Rx, -0.78, {3});
    circ.add_op<unsigned>(OpType::Rx, 1.069, {5});
    circ.add_op<unsigned>(OpType::CX, {0, 3});
    circ.add_op<unsigned>(OpType::CX, {3, 5});
    circ.add_op<unsigned>(OpType::CX, {0, 5});
    circ.add_op<unsigned>(OpType::Rx, 1.069, {3});
    circ.add_op<unsigned>(OpType::Rx, -5.958, {4});
    circ.add_op<unsigned>(OpType::CX, {3, 4});
    circ.add_op<unsigned>(OpType::CX, {3, 5});
    circ.add_op<unsigned>(OpType::CX, {0, 3});

    CompilationUnit cu(circ);
    ZXGraphlikeOptimisation()->apply(cu);
  }
}

SCENARIO("ZX resynthesis with implicit permutation removed") {
  {
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    CompilationUnit cu(circ);
    ZXGraphlikeOptimisation(false)->apply(cu);
    const auto& resynth = cu.get_circ_ref();
    CHECK(!resynth.has_implicit_wireswaps());
  }
}

SCENARIO("ZXGraphlikeOptimisation") {
  GIVEN("Circuit with name") {
    // https://github.com/quantinuum/tket/issues/1821
    Circuit circ(2);
    circ.add_op<unsigned>(OpType::H, {0});
    circ.add_op<unsigned>(OpType::CX, {0, 1});
    circ.set_name("Fred");
    CompilationUnit cu(circ);
    CHECK(ZXGraphlikeOptimisation(false)->apply(cu));
    Circuit circ1 = cu.get_circ_ref();
    std::optional<std::string> name = circ1.get_name();
    REQUIRE(name.has_value());
    REQUIRE(*name == "Fred");
  }
}

}  // namespace test_ZXSimp
}  // namespace zx
}  // namespace tket
