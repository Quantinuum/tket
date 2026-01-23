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
#include "tket/ZX/ZXDiagramImpl.hpp"

namespace tket {

namespace zx {

bool Rewrite::red_to_green_fun(ZXDiagram& diag) {
  bool success = false;
  BGL_FORALL_VERTICES(v, *diag.graph, ZXGraph) {
    if (diag.get_zxtype(v) != ZXType::XSpider) continue;
    // Found a match
    success = true;
    // Apply hadamards all around the spider
    for (Wire& w : diag.adj_wires(v)) {
      (*diag.graph)[w].type = ((*diag.graph)[w].type == ZXWireType::H)
                                  ? ZXWireType::Basic
                                  : ZXWireType::H;
    }
    // Replace X spider with Z spider
    const PhasedGen& x = diag.get_vertex_ZXGen<PhasedGen>(v);
    ZXGen_ptr z =
        ZXGen::create_gen(ZXType::ZSpider, x.get_param(), *x.get_qtype());
    diag.set_vertex_ZXGen_ptr(v, z);
  }
  return success;
}

Rewrite Rewrite::red_to_green() { return Rewrite(red_to_green_fun); }

bool Rewrite::spider_fusion_fun(ZXDiagram& diag) {
  bool success = false;
  std::set<ZXVert> bin;
  BGL_FORALL_VERTICES(v, *diag.graph, ZXGraph) {
    if (bin.contains(v)) continue;
    ZXType vtype = diag.get_zxtype(v);
    if (!is_spider_type(vtype)) continue;
    /**
     * Go through neighbours and find candidates for merging.
     * A merge candidate is either of the same colour and connected by
     * a normal edge or of different colour and connected by a Hadamard
     * edge
     **/
    WireVec adj_vec = diag.adj_wires(v);
    std::list<Wire> adj_list{adj_vec.begin(), adj_vec.end()};
    while (!adj_list.empty()) {
      Wire w = adj_list.front();
      adj_list.pop_front();
      ZXWireType wtype = diag.get_wire_type(w);
      ZXVert u = diag.other_end(w, v);
      if (bin.contains(u)) continue;
      ZXType utype = diag.get_zxtype(u);
      bool same_colour = vtype == utype;
      if (!is_spider_type(utype) || u == v ||
          (wtype == ZXWireType::Basic) != same_colour)
        continue;
      // The spiders `u` and `v` can be fused together
      // We merge into `v` and remove `u` so that we can efficiently continue to
      // search the neighbours
      const PhasedGen& vspid = diag.get_vertex_ZXGen<PhasedGen>(v);
      const PhasedGen& uspid = diag.get_vertex_ZXGen<PhasedGen>(u);
      ZXGen_ptr new_spid = ZXGen::create_gen(
          vtype, vspid.get_param() + uspid.get_param(),
          (vspid.get_qtype() == QuantumType::Classical ||
           uspid.get_qtype() == QuantumType::Classical)
              ? QuantumType::Classical
              : QuantumType::Quantum);
      diag.set_vertex_ZXGen_ptr(v, new_spid);
      for (const Wire& uw : diag.adj_wires(u)) {
        WireEnd u_end = diag.end_of(uw, u);
        ZXVert other = diag.other_end(uw, u);
        WireProperties uwp = diag.get_wire_info(uw);
        // Wires may need flipping type to match colours
        if (!same_colour)
          uwp.type = (uwp.type == ZXWireType::Basic) ? ZXWireType::H
                                                     : ZXWireType::Basic;
        /**
         * Basic edges between `(u, v)` will be ignored (these will be
         * contracted); H edges will become self loops on `v`
         **/
        if (other == v && uwp.type == ZXWireType::Basic) continue;
        // Self loops on `u` needs to become self loops on `v`
        if (other == u) other = v;
        // Connect edge to `v` instead with the same properties.
        Wire new_w;
        if (u_end == WireEnd::Source)
          new_w = diag.add_wire(v, other, uwp);
        else
          new_w = diag.add_wire(other, v, uwp);
        // Iteratively fuse along new wire if possible
        adj_list.push_back(new_w);
      }
      // Remove `u`
      bin.insert(u);
      success = true;
    }
  }
  for (ZXVert u : bin) {
    diag.remove_vertex(u);
  }
  return success;
}

Rewrite Rewrite::spider_fusion() { return Rewrite(spider_fusion_fun); }

bool Rewrite::self_loop_removal_fun(ZXDiagram& diag) {
  bool success = false;
  BGL_FORALL_VERTICES(v, *diag.graph, ZXGraph) {
    ZXType vtype = diag.get_zxtype(v);
    if (!is_spider_type(vtype)) continue;
    unsigned n_pis = 0;
    QuantumType vqtype = *diag.get_qtype(v);
    for (const Wire& w : diag.adj_wires(v)) {
      if (diag.other_end(w, v) != v) continue;
      // Found a self-loop
      ZXWireType wtype = diag.get_wire_type(w);
      QuantumType wqtype = diag.get_qtype(w);
      /**
       * Consider each case of quantumness:
       * - vqtype is Quantum
       *  + wqtype must be Quantum so Hadamards add phase
       * - vqtype is Classical
       *  + wqtype is Classical so each loop is 1 Hadamard
       *  + wqtype is Quantum so each loop is 2 Hadamards, cancelling out
       **/
      if ((vqtype == QuantumType::Quantum ||
           wqtype == QuantumType::Classical) &&
          wtype == ZXWireType::H)
        ++n_pis;
      diag.remove_wire(w);
      success = true;
    }
    if ((n_pis % 2) == 1) {
      const PhasedGen& spid = diag.get_vertex_ZXGen<PhasedGen>(v);
      ZXGen_ptr new_spid =
          ZXGen::create_gen(vtype, spid.get_param() + 1., vqtype);
      diag.set_vertex_ZXGen_ptr(v, new_spid);
    }
  }
  return success;
}

Rewrite Rewrite::self_loop_removal() { return Rewrite(self_loop_removal_fun); }

bool Rewrite::parallel_h_removal_fun(ZXDiagram& diag) {
  bool success = false;
  BGL_FORALL_VERTICES(v, *diag.graph, ZXGraph) {
    ZXType vtype = diag.get_zxtype(v);
    if (!is_spider_type(vtype)) continue;
    QuantumType vqtype = *diag.get_qtype(v);
    std::map<ZXVert, Wire> h_wires;
    for (const Wire& w : diag.adj_wires(v)) {
      ZXWireType wtype = diag.get_wire_type(w);
      ZXVert u = diag.other_end(w, v);
      ZXType utype = diag.get_zxtype(u);
      if (!is_spider_type(utype)) continue;
      if ((wtype == ZXWireType::H) != (utype == vtype)) continue;
      // This is (effectively) a Hadamard edge
      QuantumType uqtype = *diag.get_qtype(u);
      QuantumType wqtype = diag.get_qtype(w);
      if (vqtype == QuantumType::Classical &&
          uqtype == QuantumType::Classical && wqtype == QuantumType::Quantum) {
        // Doubled wire forms a pair
        diag.remove_wire(w);
        success = true;
        continue;
      }
      // Look for another wire to pair it with
      auto added = h_wires.insert({u, w});
      if (!added.second) {
        // Already found the other of the pair, so remove both
        Wire other_w = added.first->second;
        h_wires.erase(added.first);
        diag.remove_wire(w);
        diag.remove_wire(other_w);
        success = true;
      }
    }
  }
  return success;
}

Rewrite Rewrite::parallel_h_removal() {
  return Rewrite(parallel_h_removal_fun);
}

}  // namespace zx

}  // namespace tket
