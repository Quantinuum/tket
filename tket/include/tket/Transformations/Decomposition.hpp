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

#include "Transform.hpp"
#include "tket/Architecture/Architecture.hpp"

#include "tket_export.h"

namespace tket {

namespace Transforms {

/**
 * Decomposes all multi-qubit unitary gates into TK2 and single-qubit gates.
 *
 * Ignores boxes.
 *
 * Expects: any gates
 * Produces: TK2 and any single-qubit gates
 */
TKET_EXPORT Transform decompose_multi_qubits_TK2();

/**
 * Decomposes all multi-qubit unitary gates into CX and single-qubit gates.
 *
 * Ignores boxes.
 *
 * Expects: any gates
 * Produces: CX and any single-qubit gates
 */
TKET_EXPORT Transform decompose_multi_qubits_CX();

/**
 * Decomposes all single-qubit unitary gates into TK1 gates. Ignores boxes.
 */
TKET_EXPORT Transform decompose_single_qubits_TK1();

/**
 * Starting with Rz, Ry and multi-qubit gates, replace all singles with TK1.
 */
TKET_EXPORT Transform decompose_ZYZ_to_TK1();

/**
 * Starting with Rz, Rx and multi-qubit gates, replace all singles with TK1.
 */
TKET_EXPORT Transform decompose_ZXZ_to_TK1();

// converts all single-qubit gates into Rz and Rx gates
// Expects: any gates
// Produces: Rz, Rx and any multi-qubit gates
TKET_EXPORT Transform decompose_ZX();

// converts all single-qubit gates into Rz and Ry gates
// Expects: any gates
// Produces: Rz, Ry and any multi-qubit gates
TKET_EXPORT Transform decompose_ZY();

// converts all single-qubit gates into Rx and Ry gates
// Expects: any gates
// Produces: Rx, Ry and any multi-qubit gates
TKET_EXPORT Transform decompose_XY();

// converts all TK1-gates into rzrx gates
// Expects: TK1-gates and any multi-qubit gates
// Produces: Rz, Rx and any multi-qubit gates
TKET_EXPORT Transform decompose_tk1_to_rzrx();

// replaces CXs with ZZMax
// Expects: CX and any single-qubit gates
// Produces: ZZMax and any single-qubit gates
TKET_EXPORT Transform decompose_CX_to_HQS2();

// replaces Rz-Rx-Rz triples with Rz-PhasedX pairs
// Expects: Rz, Rx, and any multi-qubit gates
// Produces: Rz, PhasedX, and any multi-qubit gates
TKET_EXPORT Transform decompose_ZX_to_HQS1();

// converts all CX gates into Molmer-Sorensen gates by recognising exp(-i XX *
// angle * pi/2) and converting rest to exp(-i XX * pi/4) Expects: CX, Rx, and
// other single-qubit gates Produces: Molmer-Sorensen, U2, and other
// single-qubit gates
TKET_EXPORT Transform decompose_MolmerSorensen();

/**
 * @brief Decomposes each TK2 gate into two-qubit gates.
 *
 * We currently support CX, ZZMax and ZZPhase.
 *
 * Decompose each TK2 gate into two-qubit gates in a noise-aware way. Supported
 * two-qubit gate fidelities will be used to return the optimal decomposition of
 * each TK2 gate, taking noise into consideration.
 *
 * Using the `allow_swaps=true` (default) option, qubits will be swapped when
 * convenient to reduce the two-qubit gate count of the decomposed TK2.
 *
 * If no fidelities are provided, the decomposition will be exact, using CX
 * gates. For equal fidelities, ZZPhase will be preferred over ZZMax and CX if
 * it requires fewer gates.
 *
 * If the TK2 angles are symbolic values, the decomposition will be exact
 * (i.e. not noise-aware). It is not possible in general to obtain optimal
 * decompositions for arbitrary symbolic parameters, so consider substituting
 * for concrete values if possible.
 *
 * All TK2 angles must be normalised to the Weyl chamber. This can be achieved
 * using the \ref normalise_TK2 transform.
 *
 * @param fid The two-qubit gate fidelities (optional).
 * @param allow_swaps Allow implicit swaps (default = true).
 * @return Transform
 */
TKET_EXPORT Transform decompose_TK2(const TwoQbFidelities& fid, bool allow_swaps = true);
TKET_EXPORT Transform decompose_TK2(bool allow_swaps = true);

/**
 * @brief Synthesise ZZPhase gates from CX and Rz, as well as XX/YYPhase.
 *
 * Expects: CX, Rz, XXPhase, YYPhase Produces: ZZPhase, CX, Rz.
 */
TKET_EXPORT Transform decompose_ZZPhase();

/**
 * Decompose single-qubit Clifford gates to a standard Cliffford gate set.
 *
 * Replaces all single-qubit unitary gates outside the set {Z, X, S, V} that are
 * recognized as Clifford operations with an equivalent sequence of gates from
 * that set.
 *
 * @param tk2_to_cx whether to rebase TK2 gates to CX and standard Cliffords
 */
TKET_EXPORT Transform decompose_cliffords_std(bool tk2_to_cx = true);

TKET_EXPORT Transform decompose_ZX_to_cliffords();

// converts all valid CX-Rz-CX strings and CX-Rx-CX strings to 2qb
// PhaseGadgets Expects: CX, Rz, Rx and any other single-qubit gates Produces:
// PhaseGadgets, and any other gates
TKET_EXPORT Transform decompose_PhaseGadgets();

/**
 * Recursively replaces all boxes by their decomposition using Box::to_circuit
 * Expects: any gateset
 * @param excluded_types box types excluded from decomposition
 * @param excluded_opgroups opgroups excluded from decomposition
 * @param included_types optional, only decompose these box types
 * @param included_opgroups optional, only decompose these opgroups
 * returns potentially all gates
 */
TKET_EXPORT Transform decomp_boxes(
    const std::unordered_set<OpType>& excluded_types = {},
    const std::unordered_set<std::string>& excluded_opgroups = {},
    const std::optional<std::unordered_set<OpType>>& included_types =
        std::nullopt,
    const std::optional<std::unordered_set<std::string>>& included_opgroups =
        std::nullopt);

/**
 * Replaces all CX+Rz sub circuits by PhasePolyBox
 * Expects: only CX + Rz + H (and measure + reset + collapse + barrier)
 * @param min_size value for the minimal number of CX in each box, groups with
 * less than min_size CX gates are not converted to a PhasePolyBox, default
 * value is 0
 * @return Transformation to perform the conversion
 */
TKET_EXPORT Transform compose_phase_poly_boxes(unsigned min_size = 0);

// converts all SWAP gates to given replacement circuit (not checked to
// preserve unitary) Expects: SWAP gates, replacement circuit Produces:
// Instances of the replacement circuit,
TKET_EXPORT Transform decompose_SWAP(const Circuit& replacement_circuit);

// converts all SWAP gates to 3 CX gates
// providing an Architecture will prefer an orientation that reduces H
// redirection cost -X-    -C-X-C-   -X-C-X-
//  |   =  | | |  =  | | |
// -X-    -X-C-X-   -C-X-C-
// Expects: SWAP gates
// Produces: CX gates
TKET_EXPORT Transform decompose_SWAP_to_CX(const Architecture& arc = Architecture());

// converts all BRIDGE (distance 2 CX) gates to 4 CX gates
// -B-   -C-   -C---C---   ---C---C-
//  R     |     |   |         |   |
// -I- = --- = -X-C-X-C- = -C-X-C-X-
//  D     |       |   |     |   |
// -G-   -X-   ---X---X-   -X---X---
//  E
// Expects: BRIDGE gates
// Produces: CX gates
TKET_EXPORT Transform decompose_BRIDGE_to_CX();

// converts -C- gates to -H-X-H- depending on direction of Architecture edges
//           |              |
//          -X-          -H-C-H-
// Expects: CX gates
// Produces CX and H gates
TKET_EXPORT Transform decompose_CX_directed(const Architecture& arc);

/**
 * @brief Decompose NPhasedX gates into single-qubit PhasedX gates.
 */
TKET_EXPORT Transform decompose_NPhasedX();

// does not use ancillae
// Expects: CCX + any other gates
// returns CX, H, T, Tdg + any previous gates
TKET_EXPORT Transform decomp_CCX();

// converts arbitrarily controlled Ry gates. It does not use ancillae, so it
// is not very depth-efficient Expects: CRys and any other gates returns Ry,
// CX, H, T, Tdg + whatever other gates were there before
TKET_EXPORT Transform decomp_controlled_Rys();

// does not use ancillae
// Expects: CCX, CnX, CnY, CnZ, CnRy, CnRx, CnRz, and any other gates
// returns CX and single-qubit gate + any previous gates
TKET_EXPORT Transform decomp_arbitrary_controlled_gates();

// For every two CnX gates, we try to reorder their control qubits
// and adjust the direction of their decomposition (i.e. CnX = CnX.dagger)
// to improve the chance of gate cancellation. This method will not improve
// the decomposition when the CnX gates are scattered; but it works the best
// when CnX gates are very close to each other.
TKET_EXPORT Transform cnx_pairwise_decomposition();

}  // namespace Transforms

}  // namespace tket
