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

#include "tket/Utils/GraphHeaders.hpp"
#include "tket/ZX/Rewrite.hpp"

namespace tket {

namespace zx {

bool Rewrite::extend_for_PX_outputs_fun(ZXDiagram& diag) {
  bool modified = false;
  for (const ZXVert& o : diag.get_boundary(ZXType::Output)) {
    ZXVert n = diag.neighbours(o).front();
    if (diag.get_zxtype(n) != ZXType::Input &&
        !(diag.get_zxtype(n) == ZXType::PX &&
          !diag.get_vertex_ZXGen<CliffordGen>(n).get_param())) {
      // Extend output
      ZXGen_ptr px = ZXGen::create_gen(ZXType::PX, false);
      ZXVert n1 = diag.add_vertex(px);
      ZXVert n2 = diag.add_vertex(px);
      diag.remove_wire(diag.adj_wires(o).front());
      diag.add_wire(n, n1, ZXWireType::H);
      diag.add_wire(n1, n2, ZXWireType::H);
      diag.add_wire(n2, o);
      modified = true;
    }
  }
  return modified;
}

Rewrite Rewrite::extend_for_PX_outputs() {
  return Rewrite(extend_for_PX_outputs_fun);
}

bool Rewrite::internalise_gadgets_fun(ZXDiagram& diag) {
  std::set<ZXVert> out_vs;
  for (const ZXVert& o : diag.get_boundary(ZXType::Output))
    out_vs.insert(diag.other_end(diag.adj_wires(o).front(), o));
  std::list<ZXVert> to_remove;
  BGL_FORALL_VERTICES(v, *diag.graph, ZXGraph) {
    if (diag.degree(v) == 1 && is_MBQC_type(diag.get_zxtype(v))) {
      ZXVert axis = diag.neighbours(v).front();
      if (out_vs.find(axis) != out_vs.end()) continue;
      std::optional<unsigned> axis_clifford = std::nullopt;
      Expr axis_XY_angle;
      switch (diag.get_zxtype(axis)) {
        case ZXType::XY: {
          axis_XY_angle = diag.get_vertex_ZXGen<PhasedGen>(axis).get_param();
          axis_clifford = equiv_Clifford(axis_XY_angle);
          break;
        }
        case ZXType::PX: {
          bool axis_param =
              diag.get_vertex_ZXGen<CliffordGen>(axis).get_param();
          axis_clifford = axis_param ? 2 : 0;
          axis_XY_angle = axis_param ? 1. : 0.;
          break;
        }
        case ZXType::PY: {
          bool axis_param =
              diag.get_vertex_ZXGen<CliffordGen>(axis).get_param();
          axis_clifford = axis_param ? 3 : 1;
          axis_XY_angle = axis_param ? 1.5 : 0.5;
          break;
        }
        default:
          continue;
      }
      switch (diag.get_zxtype(v)) {
        case ZXType::XY: {
          Expr current_param = diag.get_vertex_ZXGen<PhasedGen>(v).get_param();
          if (!axis_clifford)
            continue;
          else if (*axis_clifford % 2 == 0) {
            Expr new_param =
                (*axis_clifford == 0) ? -current_param : current_param;
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(ZXType::YZ, new_param));
          } else {
            Expr new_param =
                (*axis_clifford == 1) ? current_param : -current_param;
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(ZXType::XZ, new_param));
          }
          to_remove.push_back(v);
          break;
        }
        case ZXType::YZ: {
          Expr current_param = diag.get_vertex_ZXGen<PhasedGen>(v).get_param();
          Expr new_param = axis_XY_angle - current_param;
          diag.set_vertex_ZXGen_ptr(
              axis, ZXGen::create_gen(ZXType::XY, new_param));
          to_remove.push_back(v);
          break;
        }
        case ZXType::XZ: {
          Expr current_param = diag.get_vertex_ZXGen<PhasedGen>(v).get_param();
          if (!axis_clifford)
            continue;
          else if (*axis_clifford % 2 == 0) {
            Expr new_param = (*axis_clifford == 0) ? 0.5 - current_param
                                                   : current_param - 0.5;
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(ZXType::XZ, new_param));
          } else {
            Expr new_param = (*axis_clifford == 1) ? 0.5 - current_param
                                                   : current_param - 0.5;
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(ZXType::YZ, new_param));
          }
          to_remove.push_back(v);
          break;
        }
        case ZXType::PX: {
          bool current_param =
              diag.get_vertex_ZXGen<CliffordGen>(v).get_param();
          diag.set_vertex_ZXGen_ptr(
              axis, ZXGen::create_gen(ZXType::PZ, current_param));
          to_remove.push_back(v);
          break;
        }
        case ZXType::PY: {
          bool current_param =
              diag.get_vertex_ZXGen<CliffordGen>(v).get_param();
          if (!axis_clifford) {
            Expr new_param =
                current_param ? axis_XY_angle + 0.5 : axis_XY_angle - 0.5;
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(ZXType::XY, new_param));
          } else if (*axis_clifford % 2 == 0) {
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(
                          ZXType::PY, current_param ^ (*axis_clifford == 0)));
          } else {
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(
                          ZXType::PX, !current_param ^ (*axis_clifford == 1)));
          }
          to_remove.push_back(v);
          break;
        }
        case ZXType::PZ: {
          bool current_param =
              diag.get_vertex_ZXGen<CliffordGen>(v).get_param();
          if (!axis_clifford) {
            Expr new_param = current_param ? axis_XY_angle + 1. : axis_XY_angle;
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(ZXType::XY, new_param));
          } else if (*axis_clifford % 2 == 0) {
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(
                          ZXType::PX, !current_param ^ (*axis_clifford == 0)));
          } else {
            diag.set_vertex_ZXGen_ptr(
                axis, ZXGen::create_gen(
                          ZXType::PY, !current_param ^ (*axis_clifford == 1)));
          }
          to_remove.push_back(v);
          break;
        }
        default:
          break;
      }
    }
  }
  for (const ZXVert& v : to_remove) diag.remove_vertex(v);
  return !to_remove.empty();
}

Rewrite Rewrite::internalise_gadgets() {
  return Rewrite(internalise_gadgets_fun);
}

}  // namespace zx

}  // namespace tket
