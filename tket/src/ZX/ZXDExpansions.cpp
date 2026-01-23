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

#include <tkassert/Assert.hpp>

#include "tket/Utils/GraphHeaders.hpp"
#include "tket/ZX/ZXDiagram.hpp"

namespace tket {

namespace zx {

enum class CPMDouble { Original, Conjugated };

struct CPMVert {
  ZXVert vert;
  CPMDouble conj;

  bool operator<(const CPMVert& other) const {
    if (this->vert == other.vert)
      return this->conj < other.conj;
    else
      return this->vert < other.vert;
  }
};

ZXDiagram ZXDiagram::to_doubled_diagram() const {
  ZXDiagram doubled;
  // map from vertex
  std::map<CPMVert, ZXVert> iso;

  BGL_FORALL_VERTICES(v, *this->graph, ZXGraph) {
    ZXGen_ptr op = this->get_vertex_ZXGen_ptr(v);
    std::optional<QuantumType> qtype = op->get_qtype();
    if (op->get_type() == ZXType::ZXBox) {
      const ZXBox& box = static_cast<const ZXBox&>(*op);
      ZXGen_ptr new_op = std::make_shared<const ZXBox>(
          box.get_diagram()->to_doubled_diagram());
      ZXVert added = doubled.add_vertex(new_op);
      iso.insert({{v, CPMDouble::Original}, added});
    } else {
      TKET_ASSERT(qtype.has_value());
      if (*qtype == QuantumType::Quantum) {
        ZXGen_ptr orig_op, conj_op;
        switch (op->get_type()) {
          case ZXType::Input:
          case ZXType::Output:
          case ZXType::Open: {
            orig_op = ZXGen::create_gen(op->get_type(), QuantumType::Classical);
            conj_op = orig_op;
            break;
          }
          case ZXType::ZSpider:
          case ZXType::XSpider:
          case ZXType::XY:
          case ZXType::YZ: {
            const PhasedGen& bg = static_cast<const PhasedGen&>(*op);
            orig_op = ZXGen::create_gen(
                op->get_type(), bg.get_param(), QuantumType::Classical);
            conj_op = ZXGen::create_gen(
                op->get_type(), -bg.get_param(), QuantumType::Classical);
            break;
          }
          case ZXType::XZ: {
            const PhasedGen& bg = static_cast<const PhasedGen&>(*op);
            orig_op = ZXGen::create_gen(
                op->get_type(), bg.get_param(), QuantumType::Classical);
            conj_op = orig_op;
            break;
          }
          case ZXType::PX:
          case ZXType::PZ: {
            const CliffordGen& bg = static_cast<const CliffordGen&>(*op);
            orig_op = ZXGen::create_gen(
                op->get_type(), bg.get_param(), QuantumType::Classical);
            conj_op = orig_op;
            break;
          }
          case ZXType::PY: {
            const CliffordGen& bg = static_cast<const CliffordGen&>(*op);
            orig_op = ZXGen::create_gen(
                op->get_type(), bg.get_param(), QuantumType::Classical);
            conj_op = ZXGen::create_gen(
                op->get_type(), !bg.get_param(), QuantumType::Classical);
            break;
          }
          case ZXType::Hbox: {
            const PhasedGen& bg = static_cast<const PhasedGen&>(*op);
            orig_op = ZXGen::create_gen(
                op->get_type(), bg.get_param(), QuantumType::Classical);
            conj_op = ZXGen::create_gen(
                op->get_type(), SymEngine::conjugate(bg.get_param()),
                QuantumType::Classical);
            break;
          }
          case ZXType::Triangle: {
            orig_op =
                ZXGen::create_gen(ZXType::Triangle, QuantumType::Classical);
            conj_op = orig_op;
            break;
          }
          default:
            throw ZXError("Unrecognised ZXType in to_doubled_diagram()");
        }
        ZXVert orig = doubled.add_vertex(orig_op);
        ZXVert conj = doubled.add_vertex(conj_op);
        iso.insert({{v, CPMDouble::Original}, orig});
        iso.insert({{v, CPMDouble::Conjugated}, conj});
      } else {
        ZXVert added = doubled.add_vertex(op);
        iso.insert({{v, CPMDouble::Original}, added});
      }
    }
  }

  BGL_FORALL_EDGES(w, *this->graph, ZXGraph) {
    WireProperties wp = this->get_wire_info(w);
    ZXVert s = source(w);
    ZXVert t = target(w);
    std::optional<unsigned> new_s_port, new_t_port;
    ZXGen_ptr sgen = get_vertex_ZXGen_ptr(s);
    ZXGen_ptr tgen = get_vertex_ZXGen_ptr(t);
    // Quantum ports on a ZXBox get mapped to two Classical ports
    if (sgen->get_type() == ZXType::ZXBox) {
      const ZXBox& box = static_cast<const ZXBox&>(*sgen);
      std::vector<QuantumType> sig = box.get_signature();
      unsigned p = 0;
      for (unsigned i = 0; i < *wp.source_port; ++i) {
        if (sig.at(i) == QuantumType::Quantum)
          p += 2;
        else
          p += 1;
      }
      new_s_port = p;
    }
    // Other generators just get duplicated
    else {
      new_s_port = wp.source_port;
    }
    // Do the same for the target
    if (tgen->get_type() == ZXType::ZXBox) {
      const ZXBox& box = static_cast<const ZXBox&>(*tgen);
      std::vector<QuantumType> sig = box.get_signature();
      unsigned p = 0;
      for (unsigned i = 0; i < *wp.target_port; ++i) {
        if (sig.at(i) == QuantumType::Quantum)
          p += 2;
        else
          p += 1;
      }
      new_t_port = p;
    } else {
      new_t_port = wp.target_port;
    }
    WireProperties new_wp{
        wp.type, QuantumType::Classical, new_s_port, new_t_port};
    ZXVert new_s = iso.at({s, CPMDouble::Original});
    ZXVert new_t = iso.at({t, CPMDouble::Original});
    doubled.add_wire(new_s, new_t, new_wp);
    // Handle the second wire for expanding Quantum wires
    if (wp.qtype == QuantumType::Quantum) {
      // For ZXBox, the conjugated port is the next one
      if (sgen->get_type() == ZXType::ZXBox) *new_wp.source_port += 1;
      // For other generators, the conjugate is a new vertex
      else if (sgen->get_qtype() == QuantumType::Quantum)
        new_s = iso.at({s, CPMDouble::Conjugated});
      // Do the same for the target
      if (tgen->get_type() == ZXType::ZXBox)
        *new_wp.target_port += 1;
      else if (tgen->get_qtype() == QuantumType::Quantum)
        new_t = iso.at({t, CPMDouble::Conjugated});
      doubled.add_wire(new_s, new_t, new_wp);
    }
  }

  for (const ZXVert& b : boundary) {
    doubled.boundary.push_back(iso.at({b, CPMDouble::Original}));
    if (get_qtype(b) == QuantumType::Quantum)
      doubled.boundary.push_back(iso.at({b, CPMDouble::Conjugated}));
  }

  return doubled;
}

ZXDiagram ZXDiagram::to_quantum_embedding() const {
  ZXDiagram embedding(*this);
  for (ZXVert& b : embedding.boundary) {
    if (embedding.get_qtype(b) == QuantumType::Classical) {
      ZXVert new_b =
          embedding.add_vertex(embedding.get_zxtype(b), QuantumType::Quantum);
      ZXGen_ptr id =
          ZXGen::create_gen(ZXType::ZSpider, Expr(0.), QuantumType::Classical);
      embedding.set_vertex_ZXGen_ptr(b, id);
      embedding.add_wire(new_b, b, ZXWireType::Basic, QuantumType::Quantum);
      b = new_b;
    }
  }

  return embedding;
}

}  // namespace zx

}  // namespace tket
