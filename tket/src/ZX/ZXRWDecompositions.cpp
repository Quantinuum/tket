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

#include "tket/Utils/Constants.hpp"
#include "tket/Utils/GraphHeaders.hpp"
#include "tket/ZX/Rewrite.hpp"

namespace tket {

namespace zx {

bool Rewrite::decompose_boxes_fun(ZXDiagram& diag) {
  bool success = false;
  std::list<ZXVert> to_decompose;
  BGL_FORALL_VERTICES(v, *diag.graph, ZXGraph) {
    if (diag.get_zxtype(v) == ZXType::ZXBox) to_decompose.push_back(v);
  }
  for (const ZXVert& box : to_decompose) {
    const ZXBox& zxb = diag.get_vertex_ZXGen<ZXBox>(box);
    ZXDiagram inner(*zxb.get_diagram());
    decompose_boxes_fun(inner);
    std::vector<std::pair<Wire, WireEnd>> sub_boundary(zxb.n_ports());
    for (const Wire& w : diag.adj_wires(box)) {
      if (diag.source(w) == box)
        sub_boundary.at(*diag.source_port(w)) = {w, WireEnd::Source};
      if (diag.target(w) == box)
        sub_boundary.at(*diag.target_port(w)) = {w, WireEnd::Target};
    }
    ZXDiagram::Subdiagram sub{sub_boundary, {box}};
    diag.substitute(inner, sub);
    success = true;
  }
  return success;
}

Rewrite Rewrite::decompose_boxes() { return Rewrite(decompose_boxes_fun); }

bool Rewrite::basic_wires_fun(ZXDiagram& diag) {
  ZXGen_ptr qhad =
      ZXGen::create_gen(ZXType::Hbox, Expr(-1), QuantumType::Quantum);
  ZXGen_ptr chad =
      ZXGen::create_gen(ZXType::Hbox, Expr(-1), QuantumType::Classical);
  WireVec targets;
  BGL_FORALL_EDGES(w, *diag.graph, ZXGraph) {
    if (diag.get_wire_type(w) == ZXWireType::H) targets.push_back(w);
  }
  for (const Wire& w : targets) {
    WireProperties wp = diag.get_wire_info(w);
    ZXGen_ptr had = (wp.qtype == QuantumType::Quantum) ? qhad : chad;
    ZXVert h = diag.add_vertex(had);
    ZXVert s = diag.source(w);
    ZXVert t = diag.target(w);
    wp.type = ZXWireType::Basic;
    WireProperties wp2 = wp;
    wp.target_port = std::nullopt;
    wp2.source_port = std::nullopt;
    diag.add_wire(s, h, wp);
    diag.add_wire(h, t, wp);
    diag.remove_wire(w);
  }
  return !targets.empty();
}

Rewrite Rewrite::basic_wires() { return Rewrite(basic_wires_fun); }

bool Rewrite::rebase_to_zx_fun(ZXDiagram& diag) {
  ZXVertVec verts;
  BGL_FORALL_VERTICES(v, *diag.graph, ZXGraph) {
    ZXType t = diag.get_zxtype(v);
    if (!is_boundary_type(t) && !is_spider_type(t)) verts.push_back(v);
  }
  for (const ZXVert& v : verts) {
    switch (diag.get_zxtype(v)) {
      case ZXType::Hbox: {
        // Use a combination of the equations in doi:10.1145/3209108.3209128 and
        // doi:10.4204/EPTCS.340.16 Iteratively decompose based on the phase, so
        // cannot be applied to symbolic phases
        const PhasedGen& vg = diag.get_vertex_ZXGen<PhasedGen>(v);
        std::optional<Complex> opt_ph = eval_expr_c(vg.get_param());
        if (!opt_ph)
          throw ZXError(
              "Hbox with symbolic phase cannot be decomposed into ZX "
              "generators");
        QuantumType qt = *vg.get_qtype();
        unsigned deg = diag.degree(v);
        WireVec v_ws = diag.adj_wires(v);
        std::vector<std::pair<Wire, WireEnd>> v_bounds;

        ZXDiagram rep(0, 0, 0, 0);
        double r = std::abs(*opt_ph - 1.);
        double ph = std::arg(*opt_ph - 1.) / PI;
        // Core is an algebraic Zbox surrounded by triangles
        ZXVert zph = rep.add_vertex(ZXType::ZSpider, Expr(ph), qt);
        // Not every will may have the same QuantumType in general
        bool all_same_qt = true;
        for (const Wire& w : diag.adj_wires(v)) {
          QuantumType wqt = diag.get_qtype(w);
          if (qt != wqt) all_same_qt = false;
          if (diag.source(w) == v) {
            v_bounds.push_back({w, WireEnd::Source});
            ZXVert bound = rep.add_vertex(ZXType::Open, wqt);
            rep.boundary.push_back(bound);
            ZXVert tri = rep.add_vertex(ZXType::Triangle, wqt);
            rep.add_wire(bound, tri, ZXWireType::Basic, wqt, std::nullopt, 0);
            rep.add_wire(tri, zph, ZXWireType::Basic, wqt, 1, std::nullopt);
          }
          if (diag.target(w) == v) {
            v_bounds.push_back({w, WireEnd::Target});
            ZXVert bound = rep.add_vertex(ZXType::Open, wqt);
            rep.boundary.push_back(bound);
            ZXVert tri = rep.add_vertex(ZXType::Triangle, wqt);
            rep.add_wire(bound, tri, ZXWireType::Basic, wqt, std::nullopt, 0);
            rep.add_wire(tri, zph, ZXWireType::Basic, wqt, 1, std::nullopt);
          }
        }
        // Special cases for degree 2
        if (deg == 2 && all_same_qt && approx_eq(opt_ph->real(), -1.) &&
            approx_0(opt_ph->imag())) {
          WireVec ws = diag.adj_wires(v);
          if (ws.size() == 1) {
            // self-loop
            diag.multiply_scalar(0.);
          } else {
            // Replace with a Hadamard edge
            ZXVert s = diag.other_end(ws.at(0), v);
            ZXVert t = diag.other_end(ws.at(1), v);
            unsigned n_hs = 0;
            if (diag.get_wire_type(ws.at(0)) == ZXWireType::H) ++n_hs;
            if (diag.get_wire_type(ws.at(1)) == ZXWireType::H) ++n_hs;
            diag.add_wire(
                s, t, (n_hs == 1) ? ZXWireType::Basic : ZXWireType::H, qt);
          }
          diag.remove_vertex(v);
          break;
        }
        // Using the algebraic fusion rule, can break off 2-boxes (0-phase
        // ZSpider and a triangle) to half the coefficient.
        // r is always positive as it is the result of an abs
        for (; r > 2.; r *= 0.5) {
          ZXVert tri = rep.add_vertex(ZXType::Triangle, qt);
          ZXVert one = rep.add_vertex(ZXType::ZSpider, qt);
          rep.add_wire(zph, tri, ZXWireType::Basic, qt, std::nullopt, 0);
          rep.add_wire(tri, one, ZXWireType::Basic, qt, 1, std::nullopt);
        }
        // Identify alpha s.t. r = e^{i*pi*alpha} + e^{-i*pi*alpha} =
        // 2*cos(alpha) and implement the algebraic Zbox
        double alpha = std::acos(r / 2.) / PI;
        ZXVert tri = rep.add_vertex(ZXType::Triangle, qt);
        ZXVert negal = rep.add_vertex(ZXType::ZSpider, Expr(-alpha), qt);
        ZXVert xmerge = rep.add_vertex(ZXType::XSpider, qt);
        ZXVert al = rep.add_vertex(ZXType::ZSpider, Expr(alpha), qt);
        rep.add_wire(zph, tri, ZXWireType::Basic, qt, std::nullopt, 0);
        rep.add_wire(tri, negal, ZXWireType::Basic, qt, 1, std::nullopt);
        rep.add_wire(zph, xmerge, ZXWireType::Basic, qt);
        rep.add_wire(negal, xmerge, ZXWireType::Basic, qt);
        rep.add_wire(xmerge, al, ZXWireType::Basic, qt);
        rep.multiply_scalar(
            (qt == QuantumType::Quantum) ? Expr(2.)
                                         : Expr(SymEngine::sqrt(Expr(2.))));

        // Decompose triangles
        rebase_to_zx_fun(rep);

        // Substitute
        ZXDiagram::Subdiagram sub{v_bounds, {v}};
        diag.substitute(rep, sub);
        break;
      }
      case ZXType::XY: {
        ZXGen_ptr vgen = diag.get_vertex_ZXGen_ptr(v);
        const PhasedGen& vg = static_cast<const PhasedGen&>(*vgen);
        ZXGen_ptr new_gen = ZXGen::create_gen(
            ZXType::ZSpider, -vg.get_param(), *vg.get_qtype());
        diag.set_vertex_ZXGen_ptr(v, new_gen);
        break;
      }
      case ZXType::XZ: {
        ZXGen_ptr vgen = diag.get_vertex_ZXGen_ptr(v);
        const PhasedGen& vg = static_cast<const PhasedGen&>(*vgen);
        ZXVert phase_v =
            diag.add_vertex(ZXType::XSpider, vg.get_param(), *vg.get_qtype());
        ZXGen_ptr new_gen =
            ZXGen::create_gen(ZXType::ZSpider, Expr(0.5), *vg.get_qtype());
        diag.set_vertex_ZXGen_ptr(v, new_gen);
        diag.add_wire(v, phase_v, ZXWireType::Basic, *vg.get_qtype());
        break;
      }
      case ZXType::YZ: {
        ZXGen_ptr vgen = diag.get_vertex_ZXGen_ptr(v);
        const PhasedGen& vg = static_cast<const PhasedGen&>(*vgen);
        ZXVert phase_v =
            diag.add_vertex(ZXType::XSpider, vg.get_param(), *vg.get_qtype());
        ZXGen_ptr new_gen = ZXGen::create_gen(ZXType::ZSpider, *vg.get_qtype());
        diag.set_vertex_ZXGen_ptr(v, new_gen);
        diag.add_wire(v, phase_v, ZXWireType::Basic, *vg.get_qtype());
        break;
      }
      case ZXType::PX: {
        ZXGen_ptr vgen = diag.get_vertex_ZXGen_ptr(v);
        const CliffordGen& vg = static_cast<const CliffordGen&>(*vgen);
        ZXGen_ptr new_gen = ZXGen::create_gen(
            ZXType::ZSpider, vg.get_param() ? Expr(1.) : Expr(0.),
            *vg.get_qtype());
        diag.set_vertex_ZXGen_ptr(v, new_gen);
        break;
      }
      case ZXType::PY: {
        ZXGen_ptr vgen = diag.get_vertex_ZXGen_ptr(v);
        const CliffordGen& vg = static_cast<const CliffordGen&>(*vgen);
        ZXGen_ptr new_gen = ZXGen::create_gen(
            ZXType::ZSpider, vg.get_param() ? Expr(0.5) : Expr(-0.5),
            *vg.get_qtype());
        diag.set_vertex_ZXGen_ptr(v, new_gen);
        break;
      }
      case ZXType::PZ: {
        ZXGen_ptr vgen = diag.get_vertex_ZXGen_ptr(v);
        const CliffordGen& vg = static_cast<const CliffordGen&>(*vgen);
        ZXVert phase_v = diag.add_vertex(
            ZXType::XSpider, vg.get_param() ? Expr(1.) : Expr(0.),
            *vg.get_qtype());
        ZXGen_ptr new_gen = ZXGen::create_gen(ZXType::ZSpider, *vg.get_qtype());
        diag.set_vertex_ZXGen_ptr(v, new_gen);
        diag.add_wire(v, phase_v, ZXWireType::Basic, *vg.get_qtype());
        break;
      }
      case ZXType::Triangle: {
        // Decomposition of the triangle given in Lemma 3.3, "Completeness of
        // the ZX-calculus for Pure Qubit Clifford+T Quantum Mechanics", K. Feng
        // Ng, Q. Wang, 2018
        ZXDiagram tri(0, 0, 0, 0);
        QuantumType qt = *diag.get_qtype(v);
        ZXVert in = tri.add_vertex(ZXType::Input, qt);
        tri.boundary.push_back(in);
        ZXVert out = tri.add_vertex(ZXType::Output, qt);
        tri.boundary.push_back(out);
        tri.multiply_scalar(
            (qt == QuantumType::Quantum) ? Expr(2.)
                                         : Expr(SymEngine::sqrt(Expr(2.))));
        ZXVert split = tri.add_vertex(ZXType::XSpider, qt);
        ZXVert lrz = tri.add_vertex(ZXType::ZSpider, -0.25, qt);
        ZXVert rrz = tri.add_vertex(ZXType::ZSpider, 0.25, qt);
        ZXVert laxis = tri.add_vertex(ZXType::XSpider, qt);
        ZXVert raxis = tri.add_vertex(ZXType::XSpider, qt);
        ZXVert lph = tri.add_vertex(ZXType::ZSpider, -0.25, qt);
        ZXVert rph = tri.add_vertex(ZXType::ZSpider, 0.25, qt);
        ZXVert merge = tri.add_vertex(ZXType::ZSpider, qt);
        // Flat of the triangle is the output/port 1
        tri.add_wire(out, split, ZXWireType::Basic, qt);
        tri.add_wire(split, lrz, ZXWireType::Basic, qt);
        tri.add_wire(split, rrz, ZXWireType::Basic, qt);
        tri.add_wire(lrz, laxis, ZXWireType::Basic, qt);
        tri.add_wire(rrz, raxis, ZXWireType::Basic, qt);
        tri.add_wire(laxis, lph, ZXWireType::Basic, qt);
        tri.add_wire(raxis, rph, ZXWireType::Basic, qt);
        tri.add_wire(laxis, merge, ZXWireType::Basic, qt);
        tri.add_wire(raxis, merge, ZXWireType::Basic, qt);
        // Point of the triangle is the input/port 0
        tri.add_wire(merge, in, ZXWireType::Basic, qt);

        Wire w0 = diag.wire_at_port(v, 0);
        Wire w1 = diag.wire_at_port(v, 1);
        WireEnd we0, we1;
        if (w0 == w1) {
          if (diag.source_port(w0) == 0) {
            we0 = WireEnd::Source;
            we1 = WireEnd::Target;
          } else {
            we0 = WireEnd::Target;
            we1 = WireEnd::Source;
          }
        } else {
          we0 = diag.end_of(w0, v);
          we1 = diag.end_of(w1, v);
        }
        ZXDiagram::Subdiagram sub{{{w0, we0}, {w1, we1}}, {v}};
        diag.substitute(tri, sub);
        break;
      }
      default:
        break;
    }
  }
  return !verts.empty();
}

Rewrite Rewrite::rebase_to_zx() { return Rewrite(rebase_to_zx_fun); }

bool Rewrite::rebase_to_mbqc_fun(ZXDiagram& diag) {
  ZXVertVec verts;
  BGL_FORALL_VERTICES(v, *diag.graph, ZXGraph) {
    ZXType t = diag.get_zxtype(v);
    if (!is_boundary_type(t) && !is_MBQC_type(t)) verts.push_back(v);
  }
  for (const ZXVert& v : verts) {
    switch (diag.get_zxtype(v)) {
      case ZXType::ZSpider: {
        ZXGen_ptr vgen = diag.get_vertex_ZXGen_ptr(v);
        const PhasedGen& vg = static_cast<const PhasedGen&>(*vgen);
        std::optional<unsigned> pi2_mult = equiv_Clifford(vg.get_param());
        ZXGen_ptr new_gen;
        if (pi2_mult) {
          if (*pi2_mult % 2 == 0)
            new_gen = ZXGen::create_gen(
                ZXType::PX, (*pi2_mult % 4 == 2), *vg.get_qtype());
          else
            new_gen = ZXGen::create_gen(
                ZXType::PY, (*pi2_mult % 4 == 1), *vg.get_qtype());
        } else
          new_gen =
              ZXGen::create_gen(ZXType::XY, -vg.get_param(), *vg.get_qtype());
        diag.set_vertex_ZXGen_ptr(v, new_gen);
        break;
      }
      case ZXType::XSpider: {
        ZXGen_ptr vgen = diag.get_vertex_ZXGen_ptr(v);
        const PhasedGen& vg = static_cast<const PhasedGen&>(*vgen);
        std::optional<unsigned> pi2_mult = equiv_Clifford(vg.get_param());
        ZXGen_ptr new_gen;
        if (pi2_mult) {
          if (*pi2_mult % 2 == 0)
            new_gen = ZXGen::create_gen(
                ZXType::PX, (*pi2_mult % 4 == 2), *vg.get_qtype());
          else
            new_gen = ZXGen::create_gen(
                ZXType::PY, (*pi2_mult % 4 == 1), *vg.get_qtype());
        } else
          new_gen =
              ZXGen::create_gen(ZXType::XY, -vg.get_param(), *vg.get_qtype());
        diag.set_vertex_ZXGen_ptr(v, new_gen);
        for (const Wire& w : diag.adj_wires(v)) {
          ZXWireType wt = diag.get_wire_type(w);
          diag.set_wire_type(
              w, (wt == ZXWireType::Basic) ? ZXWireType::H : ZXWireType::Basic);
        }
        break;
      }
      case ZXType::Hbox: {
        WireVec v_ws = diag.adj_wires(v);
        std::vector<std::pair<Wire, WireEnd>> v_bounds;
        for (const Wire& w : v_ws) {
          if (diag.source(w) == v) v_bounds.push_back({w, WireEnd::Source});
          if (diag.target(w) == v) v_bounds.push_back({w, WireEnd::Target});
        }
        ZXDiagram::Subdiagram sub{v_bounds, {v}};
        ZXDiagram h = sub.to_diagram(diag);
        rebase_to_zx_fun(h);
        rebase_to_mbqc_fun(h);
        diag.substitute(h, sub);
        break;
      }
      case ZXType::Triangle: {
        Wire w0 = diag.wire_at_port(v, 0);
        Wire w1 = diag.wire_at_port(v, 1);
        WireEnd we0, we1;
        if (w0 == w1) {
          if (diag.source_port(w0) == 0) {
            we0 = WireEnd::Source;
            we1 = WireEnd::Target;
          } else {
            we0 = WireEnd::Target;
            we1 = WireEnd::Source;
          }
        } else {
          we0 = diag.end_of(w0, v);
          we1 = diag.end_of(w1, v);
        }
        ZXDiagram::Subdiagram sub{{{w0, we0}, {w1, we1}}, {v}};
        ZXDiagram tri = sub.to_diagram(diag);
        rebase_to_zx_fun(tri);
        rebase_to_mbqc_fun(tri);
        diag.substitute(tri, sub);
        break;
      }
      default:
        break;
    }
  }
  return !verts.empty();
}

Rewrite Rewrite::rebase_to_mbqc() { return Rewrite(rebase_to_mbqc_fun); }

}  // namespace zx

}  // namespace tket
