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

#include "tket/ZX/Rewrite.hpp"

namespace tket {

namespace zx {

bool Rewrite::separate_boundaries_fun(ZXDiagram& diag) {
  bool success = false;
  for (const ZXVert& b : diag.get_boundary()) {
    // Boundaries in valid diagrams have degree 1
    Wire w = diag.adj_wires(b).at(0);
    // Since degree is 1, o is distinct from b
    ZXVert o = diag.other_end(w, b);
    // Other end of the wire needs to either be a boundary type or connected to
    // another boundary
    if (!is_boundary_type(diag.get_zxtype(o))) {
      bool o_shared = false;
      for (const ZXVert& n : diag.neighbours(o)) {
        if (n != b && is_boundary_type(diag.get_zxtype(n))) {
          o_shared = true;
          break;
        }
      }
      if (!o_shared) continue;
    }
    // New wires will inherit `w`'s `qtype`
    QuantumType wq = diag.get_qtype(w);
    ZXGen_ptr id = ZXGen::create_gen(ZXType::ZSpider, Expr(0.), wq);
    ZXVert z_at_b = diag.add_vertex(id);
    diag.add_wire(b, z_at_b, ZXWireType::Basic, wq);
    ZXVert z_at_o;
    if (diag.get_zxtype(o) == ZXType::ZSpider) {
      z_at_o = o;
    } else {
      z_at_o = diag.add_vertex(id);
      diag.add_wire(o, z_at_o, ZXWireType::Basic, wq);
    }
    if (diag.get_wire_type(w) == ZXWireType::Basic) {
      // Basic requires two Hadamard edges
      ZXVert middle = diag.add_vertex(id);
      diag.add_wire(z_at_b, middle, ZXWireType::H, wq);
      diag.add_wire(z_at_o, middle, ZXWireType::H, wq);
    } else {
      // Hadamard edge
      diag.add_wire(z_at_b, z_at_o, ZXWireType::H, wq);
    }
    diag.remove_wire(w);
    success = true;
  }
  return success;
}

Rewrite Rewrite::separate_boundaries() {
  return Rewrite(separate_boundaries_fun);
}

bool Rewrite::io_extension_fun(ZXDiagram& diag) {
  bool success = false;
  for (const ZXVert& b : diag.get_boundary()) {
    // Boundaries in valid diagrams have degree 1
    Wire w = diag.adj_wires(b).at(0);
    WireProperties wp = diag.get_wire_info(w);
    if (wp.type == ZXWireType::Basic) continue;
    // Extend by an identity spider
    ZXVert u = diag.other_end(w, b);
    ZXVert z = diag.add_vertex(ZXType::ZSpider, 0., wp.qtype);
    // `u` might be directed, so preserve ports by preserving direction
    if (diag.end_of(w, u) == WireEnd::Source)
      diag.add_wire(u, z, wp);
    else
      diag.add_wire(z, u, wp);
    diag.add_wire(b, z, ZXWireType::Basic, wp.qtype);
    diag.remove_wire(w);
    success = true;
  }
  return success;
}

Rewrite Rewrite::io_extension() { return Rewrite(io_extension_fun); }

}  // namespace zx

}  // namespace tket
