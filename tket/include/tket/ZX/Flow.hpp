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

#pragma once

#include <boost/bimap.hpp>

#include "tket/ZX/ZXDiagram.hpp"
#include "tket_export.h"

namespace tket {

// Forward declare converter for friend access
class Circuit;
Circuit zx_to_circuit(const zx::ZXDiagram& diag);

namespace zx {

/**
 * Data structure for flow in qubit MBQC.
 * Different classes of flow exist based on the types of measurements and
 * correction sets accepted, but the contents of the flow are the same. Causal <
 * XY gflow < 3Plane gflow < Pauli flow
 *
 * `c` defines the correction set for each measured vertex.
 * `d` approximates the partial order by giving the depth of the measurement
 * from the output, i.e. d(u) < d(v) => v is measured before u.
 */
class TKET_EXPORT Flow {
 public:
  Flow(
      const std::map<ZXVert, ZXVertSeqSet>& c,
      const std::map<ZXVert, unsigned>& d);

  // Returns the correction set for a given measured vertex (those vertices
  // receiving an X correction) Will fail with a map.at error if v is not in the
  // flow
  ZXVertSeqSet c(const ZXVert& v) const;
  // Returns the odd neighbourhood of the correction set for a given measured
  // vertex (those vertices receiving a Z correction) Will fail with a map.at
  // error if v is not in the flow
  ZXVertSeqSet odd(const ZXVert& v, const ZXDiagram& diag) const;
  // Returns the depth from the outputs in the ordering of the flow
  // e.g. an output vertex will have depth 0, the last measured vertex has depth
  // 1
  unsigned d(const ZXVert& v) const;

  // Verify that a flow is well-formed according to the Pauli flow conditions
  // Throws a ZXError if any condition is violated
  void verify(const ZXDiagram& diag) const;

  // Focuses a flow according to Lemma B.5, Simmons "Relating Measurement
  // Patterns to Circuits via Pauli Flow" https://arxiv.org/pdf/2109.05654.pdf
  void focus(const ZXDiagram& diag);

  // Attempts to identify a causal flow for a diagram
  // Follows Algorithm 1 from Mhalla & Perdrix "Finding Optimal Flows
  // Efficiently" https://arxiv.org/pdf/0709.2670.pdf O(n^2 log n) for n
  // vertices
  static Flow identify_causal_flow(const ZXDiagram& diag);
  // Attempts to identify a Pauli flow for a diagram
  // Follows Algorithm 1 from Simmons "Relating Measurement Patterns to Circuits
  // via Pauli Flow" https://arxiv.org/pdf/2109.05654.pdf O(n^4) for n vertices
  static Flow identify_pauli_flow(const ZXDiagram& diag);

  // Attempts to identify focussed sets according to Lemma B.10, Simmons
  // "Relating Measurement Patterns to Circuits via Pauli Flow"
  // https://arxiv.org/pdf/2109.05654.pdf
  static std::set<ZXVertSeqSet> identify_focussed_sets(const ZXDiagram& diag);

  friend Circuit tket::zx_to_circuit(const zx::ZXDiagram& diag);

  // Internal state is public so binders can expose maps for easy iteration
  // Correction sets
  std::map<ZXVert, ZXVertSeqSet> c_;
  // Approximate the partial order by recording the depth from outputs
  std::map<ZXVert, unsigned> d_;

 private:
  // Solve for corrections using Gaussian elimination and back substitution
  // Used within identify_pauli_flow
  // correctors are those vertices which may be included in the correction sets
  // preserve are those vertices which may not be included in the odd
  // neighbourhood (unless being corrected) to_solve are those vertices that are
  // yet to find corrections ys are all vertices with ZXType::PY The maps
  // convert between row/column indices in the matrix and vertices in the
  // diagram
  static std::map<ZXVert, ZXVertSeqSet> gauss_solve_correctors(
      const ZXDiagram& diag, const boost::bimap<ZXVert, unsigned>& correctors,
      const boost::bimap<ZXVert, unsigned>& preserve, const ZXVertVec& to_solve,
      const boost::bimap<ZXVert, unsigned>& ys);
};

}  // namespace zx

}  // namespace tket
