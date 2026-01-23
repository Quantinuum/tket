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
#include <catch2/matchers/catch_matchers.hpp>
#include <fstream>

#include "tket/ZX/ZXDiagram.hpp"
#include "tket/ZX/ZXGenerator.hpp"

namespace tket {
namespace zx {
namespace test_ZXDiagram {

SCENARIO("Testing generator creation") {
  BoundaryGen input(ZXType::Input, QuantumType::Quantum);
  CHECK(input.get_name() == "Q-Input");
  CHECK(input.get_type() == ZXType::Input);
  CHECK(input.get_qtype() == QuantumType::Quantum);
  CHECK(input.valid_edge(std::nullopt, QuantumType::Quantum));
  CHECK_FALSE(input.valid_edge(0, QuantumType::Quantum));
  CHECK_FALSE(input.valid_edge(std::nullopt, QuantumType::Classical));

  PhasedGen zSpider(ZXType::ZSpider, 0.3, QuantumType::Classical);
  CHECK(zSpider.get_name() == "C-Z(0.3)");
  CHECK(zSpider.get_type() == ZXType::ZSpider);
  CHECK(zSpider.get_qtype() == QuantumType::Classical);
  CHECK(zSpider.free_symbols().empty());
  CHECK(zSpider.valid_edge(std::nullopt, QuantumType::Quantum));
  CHECK(zSpider.valid_edge(std::nullopt, QuantumType::Classical));
  CHECK_FALSE(zSpider.valid_edge(0, QuantumType::Quantum));

  PhasedGen xSpider(ZXType::XSpider, Expr("2*a"), QuantumType::Quantum);
  CHECK(xSpider.get_name() == "Q-X(2*a)");
  CHECK(xSpider.get_type() == ZXType::XSpider);
  CHECK(xSpider.get_qtype() == QuantumType::Quantum);
  CHECK(xSpider.free_symbols().size() == 1);
  CHECK(xSpider.valid_edge(std::nullopt, QuantumType::Quantum));
  CHECK_FALSE(xSpider.valid_edge(std::nullopt, QuantumType::Classical));
  SymEngine::map_basic_basic sub_map;
  Sym a = SymEngine::symbol("a");
  sub_map[a] = Expr(0.8);
  CHECK(xSpider.symbol_substitution(sub_map)->get_name() == "Q-X(1.6)");
  CHECK(
      *xSpider.symbol_substitution(sub_map) ==
      PhasedGen(ZXType::XSpider, 1.6, QuantumType::Quantum));

  CliffordGen px(ZXType::PX, true, QuantumType::Classical);
  CHECK(px.get_name() == "C-X(1)");
  CHECK(px.get_type() == ZXType::PX);
  CHECK(px.get_param() == true);
  CHECK(px.free_symbols().empty());
  CHECK(!(px == CliffordGen(ZXType::PX, false, QuantumType::Quantum)));
  CHECK(px == CliffordGen(ZXType::PX, true, QuantumType::Classical));

  // Should throw an error: type Triangle is not a BasicGen type
  REQUIRE_THROWS_AS(PhasedGen(ZXType::Triangle, 0.3), ZXError);

  DirectedGen tri(ZXType::Triangle, QuantumType::Classical);
  CHECK(tri.get_name() == "C-Tri");
  CHECK_FALSE(tri.valid_edge(std::nullopt, QuantumType::Classical));
  CHECK_FALSE(tri.valid_edge(2, QuantumType::Classical));
  CHECK(tri.valid_edge(0, QuantumType::Classical));
  CHECK_FALSE(tri.valid_edge(1, QuantumType::Quantum));
}

SCENARIO("Testing diagram creation & vertex/edge additions") {
  ZXDiagram diag(1, 1, 0, 0);
  CHECK(diag.get_scalar() == Expr(1.));
  CHECK_FALSE(diag.is_symbolic());
  diag.multiply_scalar(0.4);
  diag.multiply_scalar(Expr("2*a"));
  CHECK(diag.get_scalar() == Expr("0.8*a"));
  CHECK(diag.free_symbols().size() == 1);

  ZXVert zSpid_v = diag.add_vertex(ZXType::ZSpider, 0.1);
  ZXVert xSpid_v = diag.add_vertex(ZXType::XSpider, 3.4);
  ZXVert hbox_v = diag.add_vertex(
      ZXType::Hbox, 6.7 * Expr("b") + 3 * Expr(i_), QuantumType::Classical);

  REQUIRE_THROWS_AS(diag.add_vertex(ZXType::ZXBox, 3.), ZXError);

  REQUIRE_THROWS_WITH(
      diag.check_validity(), "Boundary vertex does not have degree 1");

  diag.add_wire(diag.get_boundary().at(0), zSpid_v);
  diag.add_wire(diag.get_boundary().at(1), xSpid_v);
  diag.add_wire(zSpid_v, xSpid_v);
  diag.add_wire(xSpid_v, zSpid_v, ZXWireType::H);
  Wire extra = diag.add_wire(diag.get_boundary().at(1), zSpid_v);
  REQUIRE_THROWS_WITH(
      diag.check_validity(), "Boundary vertex does not have degree 1");

  diag.remove_wire(extra);
  REQUIRE_NOTHROW(diag.check_validity());

  Wire wrong_port = diag.add_wire(
      hbox_v, xSpid_v, ZXWireType::Basic, QuantumType::Quantum, 0);
  REQUIRE_THROWS_WITH(
      diag.check_validity(), "Wire at a named port of an undirected vertex");
  diag.remove_wire(wrong_port);

  ZXVert tri_v = diag.add_vertex(ZXType::Triangle);
  diag.add_wire(tri_v, zSpid_v, ZXWireType::Basic, QuantumType::Quantum, 0);
  REQUIRE_THROWS_WITH(
      diag.check_validity(),
      "Not all ports of a directed vertex have wires connected");

  diag.add_wire(zSpid_v, tri_v);
  REQUIRE_THROWS_WITH(
      diag.check_validity(), "Wire at an unnamed port of a directed vertex");

  CHECK(diag.remove_wire(
      tri_v, zSpid_v,
      {ZXWireType::Basic, QuantumType::Quantum, std::nullopt, std::nullopt}));
  diag.add_wire(
      zSpid_v, tri_v, ZXWireType::Basic, QuantumType::Quantum, std::nullopt, 1);
  REQUIRE_NOTHROW(diag.check_validity());

  Wire extra_port =
      diag.add_wire(tri_v, zSpid_v, ZXWireType::Basic, QuantumType::Quantum, 1);
  REQUIRE_THROWS_WITH(
      diag.check_validity(), "Multiple wires on the same port of a vertex");
  diag.remove_wire(extra_port);

  ZXDiagram inner(1, 2, 1, 0);
  ZXVert inner_spid =
      inner.add_vertex(ZXType::ZSpider, 0.6, QuantumType::Classical);
  inner.add_wire(inner_spid, inner.get_boundary().at(0));
  inner.add_wire(inner_spid, inner.get_boundary().at(1));
  inner.add_wire(inner_spid, inner.get_boundary().at(2), ZXWireType::H);
  inner.add_wire(
      inner_spid, inner.get_boundary().at(3), ZXWireType::Basic,
      QuantumType::Classical);
  ZXGen_ptr box = std::make_shared<const ZXBox>(inner);

  ZXVert box_v = diag.add_vertex(box);
  diag.add_wire(box_v, hbox_v, ZXWireType::Basic, QuantumType::Quantum, 0);
  diag.add_wire(box_v, hbox_v, ZXWireType::Basic, QuantumType::Quantum, 1);
  diag.add_wire(box_v, xSpid_v, ZXWireType::Basic, QuantumType::Quantum, 2);
  Wire wrong_qtype =
      diag.add_wire(box_v, hbox_v, ZXWireType::Basic, QuantumType::Quantum, 3);
  REQUIRE_THROWS_WITH(
      diag.check_validity(),
      "QuantumType of wire is incompatible with the given port");

  diag.set_wire_qtype(wrong_qtype, QuantumType::Classical);
  REQUIRE_NOTHROW(diag.check_validity());

  CHECK(diag.free_symbols().size() == 2);
  SymEngine::map_basic_basic sub_map;
  Sym a = SymEngine::symbol("a");
  Sym b = SymEngine::symbol("b");
  sub_map[a] = Expr(0.8);
  diag.symbol_substitution(sub_map);
  CHECK(diag.free_symbols().size() == 1);
  sub_map[b] = Expr(0.4);
  diag.symbol_substitution(sub_map);
  CHECK(diag.free_symbols().size() == 0);
  CHECK(diag.get_name(hbox_v) == "C-H(2.68 + 3.0*I)");
  REQUIRE_NOTHROW(diag.check_validity());

  THEN("Print diagram to file") {
    std::ofstream dot_file("zxdiag.dot");
    dot_file << diag.to_graphviz_str();
    dot_file.close();
    remove("zxdiag.dot");
  }
}

SCENARIO("Test move constructors") {
  GIVEN("A single spider") {
    ZXDiagram diag(1, 2, 0, 0);
    ZXVertVec ins = diag.get_boundary(ZXType::Input);
    ZXVertVec outs = diag.get_boundary(ZXType::Output);
    ZXVert z = diag.add_vertex(ZXType::ZSpider, 0.3);
    diag.add_wire(ins[0], z);
    diag.add_wire(outs[0], z);
    diag.add_wire(outs[1], z);

    WHEN("Move constructor") {
      ZXDiagram d2(std::move(diag));
      REQUIRE_NOTHROW(d2.check_validity());
      CHECK(d2.n_vertices() == 4);
      CHECK(d2.n_wires() == 3);
    }
    WHEN("Move assignment") {
      ZXDiagram d2(4, 4, 1, 3);
      d2 = std::move(diag);
      REQUIRE_NOTHROW(d2.check_validity());
      CHECK(d2.n_vertices() == 4);
      CHECK(d2.n_wires() == 3);
    }
  }
}

SCENARIO("Check that diagram conversions achieve the correct form") {
  GIVEN("A mixed circuit") {
    ZXDiagram diag(2, 2, 1, 1);
    ZXVertVec ins = diag.get_boundary(ZXType::Input);
    ZXVertVec outs = diag.get_boundary(ZXType::Output);
    ZXVert qz = diag.add_vertex(ZXType::ZSpider, 0.3);
    ZXVert qx = diag.add_vertex(ZXType::XSpider);
    ZXVert cz = diag.add_vertex(ZXType::ZSpider, QuantumType::Classical);
    ZXDiagram inner(1, 1, 0, 0);
    inner.add_vertex(ZXType::Hbox, Expr(i_));
    inner.add_vertex(ZXType::Triangle);
    ZXGen_ptr box = std::make_shared<const ZXBox>(inner);
    ZXVert b = diag.add_vertex(box);
    diag.add_wire(ins[0], qz);
    diag.add_wire(qz, outs[0]);
    diag.add_wire(qx, outs[1], ZXWireType::H);
    diag.add_wire(ins[1], cz, ZXWireType::Basic, QuantumType::Quantum);
    diag.add_wire(ins[2], cz, ZXWireType::H, QuantumType::Classical);
    diag.add_wire(outs[2], cz, ZXWireType::Basic, QuantumType::Classical);
    diag.add_wire(b, qz, ZXWireType::Basic, QuantumType::Quantum, 0);
    diag.add_wire(
        qx, b, ZXWireType::Basic, QuantumType::Quantum, std::nullopt, 1);
    THEN("Expand quantum vertices/edges into pairs of classical ones") {
      ZXDiagram doubled = diag.to_doubled_diagram();
      REQUIRE_NOTHROW(doubled.check_validity());
      CHECK(doubled.n_vertices() == 16);
      CHECK(doubled.n_wires() == 14);
      for (const ZXVert &b : doubled.get_boundary()) {
        CHECK(doubled.get_qtype(b) == QuantumType::Classical);
        CHECK(
            doubled.get_qtype(doubled.adj_wires(b)[0]) ==
            QuantumType::Classical);
        CHECK(
            doubled.get_qtype(doubled.neighbours(b)[0]) ==
            QuantumType::Classical);
      }
      ZXVertVec ins = doubled.get_boundary(ZXType::Input);
      CHECK(doubled.get_name(doubled.neighbours(ins[0])[0]) == "C-Z(0.3)");
      CHECK(doubled.get_name(doubled.neighbours(ins[1])[0]) == "C-Z(1.7)");
    }
    GIVEN("Embedding classical boundaries into quantum states") {
      ZXDiagram embedded = diag.to_quantum_embedding();
      REQUIRE_NOTHROW(embedded.check_validity());
      CHECK(embedded.n_vertices() == 12);
      CHECK(embedded.n_wires() == 10);
      for (const ZXVert &b : embedded.get_boundary()) {
        CHECK(embedded.get_qtype(b) == QuantumType::Quantum);
      }
    }
  }
}

SCENARIO("Subdiagram substitutions") {
  GIVEN("Euler exchange") {
    ZXDiagram diag(1, 1, 0, 0);
    ZXVert in = diag.get_boundary(ZXType::Input).at(0);
    ZXVert out = diag.get_boundary(ZXType::Output).at(0);
    ZXVert za = diag.add_vertex(ZXType::ZSpider, 0.5);
    ZXVert x = diag.add_vertex(ZXType::XSpider, 0.5);
    ZXVert zb = diag.add_vertex(ZXType::ZSpider, 0.5);
    Wire iw = diag.add_wire(in, za);
    diag.add_wire(za, x);
    diag.add_wire(x, zb);
    Wire ow = diag.add_wire(zb, out, ZXWireType::H);

    ZXDiagram to_insert(1, 1, 0, 0);
    ZXVert in_in = to_insert.get_boundary(ZXType::Input).at(0);
    ZXVert in_out = to_insert.get_boundary(ZXType::Output).at(0);
    ZXVert xa = to_insert.add_vertex(ZXType::XSpider, 1.5);
    ZXVert z = to_insert.add_vertex(ZXType::ZSpider, 1.5);
    ZXVert xb = to_insert.add_vertex(ZXType::XSpider, 1.5);
    to_insert.add_wire(in_in, xa, ZXWireType::H);
    to_insert.add_wire(xa, z);
    to_insert.add_wire(z, xb);
    to_insert.add_wire(xb, in_out, ZXWireType::H);

    ZXDiagram::Subdiagram sub{
        {{iw, WireEnd::Target}, {ow, WireEnd::Source}}, {za, x, zb}};
    REQUIRE_NOTHROW(sub.check_validity(diag));
    REQUIRE_NOTHROW(sub.to_diagram(diag).check_validity());
    REQUIRE_NOTHROW(diag.substitute(to_insert, sub));
    REQUIRE_NOTHROW(diag.check_validity());
    CHECK(diag.n_vertices() == 5);
    CHECK(diag.n_wires() == 4);
    iw = diag.adj_wires(in).at(0);
    CHECK(diag.get_wire_type(iw) == ZXWireType::H);
    CHECK(diag.get_zxtype(diag.other_end(iw, in)) == ZXType::XSpider);
    ow = diag.adj_wires(out).at(0);
    CHECK(diag.get_wire_type(ow) == ZXWireType::Basic);
    CHECK(diag.get_zxtype(diag.other_end(ow, out)) == ZXType::XSpider);
  }
  GIVEN("A subdiagram with a self-edge") {
    ZXDiagram diag(1, 1, 0, 1);
    ZXVert x = diag.add_vertex(ZXType::XSpider, 1., QuantumType::Classical);
    Wire wi =
        diag.add_wire(diag.get_boundary(ZXType::Input).at(0), x, ZXWireType::H);
    Wire woq = diag.add_wire(x, diag.get_boundary(ZXType::Output).at(0));
    Wire woc = diag.add_wire(
        x, diag.get_boundary(ZXType::Output).at(1), ZXWireType::Basic,
        QuantumType::Classical);
    Wire wloop = diag.add_wire(x, x, ZXWireType::Basic, QuantumType::Classical);

    ZXDiagram to_insert(1, 1, 1, 2);
    ZXVert z_inner =
        to_insert.add_vertex(ZXType::ZSpider, 1., QuantumType::Classical);
    to_insert.add_wire(to_insert.get_boundary(ZXType::Input).at(0), z_inner);
    to_insert.add_wire(
        to_insert.get_boundary(ZXType::Input).at(1), z_inner, ZXWireType::Basic,
        QuantumType::Classical);
    to_insert.add_wire(to_insert.get_boundary(ZXType::Output).at(0), z_inner);
    to_insert.add_wire(
        to_insert.get_boundary(ZXType::Output).at(1), z_inner,
        ZXWireType::Basic, QuantumType::Classical);
    to_insert.add_wire(
        to_insert.get_boundary(ZXType::Output).at(2), z_inner, ZXWireType::H,
        QuantumType::Classical);

    ZXDiagram::Subdiagram sub{
        {{wi, WireEnd::Target},
         {woq, WireEnd::Source},
         {wloop, WireEnd::Source},
         {wloop, WireEnd::Target},
         {woc, WireEnd::Source}},
        {x}};
    REQUIRE_NOTHROW(sub.check_validity(diag));
    REQUIRE_NOTHROW(diag.substitute(to_insert, sub));
    REQUIRE_NOTHROW(diag.check_validity());
    CHECK(diag.n_vertices() == 4);
    CHECK(diag.n_wires() == 4);
    woc = diag.adj_wires(diag.get_boundary(ZXType::Output).at(1)).at(0);
    CHECK(diag.get_wire_type(woc) == ZXWireType::H);
    ZXVert z = diag.other_end(woc, diag.get_boundary(ZXType::Output).at(1));
    CHECK(diag.get_zxtype(z) == ZXType::ZSpider);
    wloop = diag.wires_between(z, z).at(0);
    CHECK(diag.get_wire_type(wloop) == ZXWireType::Basic);
    CHECK(diag.get_qtype(wloop) == QuantumType::Classical);
  }
  GIVEN("Replacing a ZXBox") {
    ZXDiagram inner(1, 2, 0, 0);
    ZXVert inner_z = inner.add_vertex(ZXType::ZSpider, 0.3);
    inner.add_wire(inner.get_boundary().at(0), inner_z);
    inner.add_wire(inner.get_boundary().at(1), inner_z, ZXWireType::H);
    inner.add_wire(inner.get_boundary().at(2), inner_z);

    ZXDiagram diag(1, 1, 0, 0);
    ZXVert outer_z = diag.add_vertex(ZXType::ZSpider, 0.7);
    ZXVert box = diag.add_vertex(std::make_shared<const ZXBox>(inner));
    Wire wi = diag.add_wire(
        diag.get_boundary(ZXType::Input).at(0), box, ZXWireType::Basic,
        QuantumType::Quantum, std::nullopt, 0);
    Wire wo = diag.add_wire(
        diag.get_boundary(ZXType::Output).at(0), box, ZXWireType::Basic,
        QuantumType::Quantum, std::nullopt, 1);
    Wire wz = diag.add_wire(
        outer_z, box, ZXWireType::H, QuantumType::Quantum, std::nullopt, 2);

    ZXDiagram::Subdiagram sub{
        {{wi, WireEnd::Target}, {wo, WireEnd::Target}, {wz, WireEnd::Target}},
        {box}};
    REQUIRE_NOTHROW(sub.check_validity(diag));
    REQUIRE_NOTHROW(diag.substitute(inner, sub));
    REQUIRE_NOTHROW(diag.check_validity());
    CHECK(
        diag.get_wire_type(
            diag.adj_wires(diag.get_boundary(ZXType::Output).at(0)).at(0)) ==
        ZXWireType::H);
    CHECK(diag.get_wire_type(diag.adj_wires(outer_z).at(0)) == ZXWireType::H);
  }
  GIVEN("Substitution yields a wireloop") {
    ZXDiagram identity(1, 1, 0, 0);
    identity.add_wire(
        identity.get_boundary(ZXType::Input).at(0),
        identity.get_boundary(ZXType::Output).at(0));

    ZXDiagram loop;
    ZXVert z = loop.add_vertex(ZXType::ZSpider);
    Wire wloop = loop.add_wire(z, z);

    ZXDiagram::Subdiagram sub{
        {{wloop, WireEnd::Source}, {wloop, WireEnd::Target}}, {z}};
    REQUIRE_NOTHROW(sub.check_validity(loop));
    REQUIRE_NOTHROW(loop.substitute(identity, sub));
    REQUIRE_NOTHROW(loop.check_validity());
    CHECK(loop.n_vertices() == 0);
    CHECK(loop.n_wires() == 0);
    CHECK(loop.get_scalar() == 4.);
  }
}

}  // namespace test_ZXDiagram
}  // namespace zx
}  // namespace tket
