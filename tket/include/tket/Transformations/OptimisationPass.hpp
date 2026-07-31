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

#include "tket/OpType/OpType.hpp"
#include "tket/Transformations/Transform.hpp"

#include "tket_export.h"

namespace tket {

namespace Transforms {

//////////////////////////
// Full Optimisation Pass//
//////////////////////////

/*these Transform passes do not preserve connectivity*/

/**
 * only peephole optimisation, so no higher structure abstraction.
 * Two qubit Cartan, Clifford, synthesis
 * @param allow_swaps whether to allow introduction of implicit wire swaps
 * Expects: Any gates
 * Produces: CX, TK1
 *
 */
TKET_EXPORT Transform peephole_optimise_2q(bool allow_swaps = true);

/**
 * Peephole optimisation including resynthesis of three-qubit gate sequences.
 *
 * The allow_swaps parameter has no effect when the target gate is TK2.
 *
 * @param allow_swaps whether to allow introduction of implicit wire swaps
 * @param target_2qb_gate target 2-qubut gate (CX or TK2)
 *
 * Produces: (CX or TK2) and TK1.
 */
TKET_EXPORT Transform full_peephole_optimise(
    bool allow_swaps = true, OpType target_2qb_gate = OpType::CX);

/**
 * Simplify a circuit using Clifford rules.
 *
 * The resulting circuit will consist of the target two-qubit gate (which may be
 * either CX or TK2) and TK1 gates.
 *
 * @param allow_swaps whether to allow introduction of implicit wire swaps
 * @param target_2qb_gate target two-qubit gate
 */
TKET_EXPORT Transform clifford_simp(
    bool allow_swaps = true, OpType target_2qb_gate = OpType::CX);

//////////////////
// Synthesis Pass//
//////////////////

/*Synthesis passes preserve connectivity */

/**
 * Synthesise a circuit consisting of TK2 and TK1 gates only.
 */
TKET_EXPORT Transform synthesise_tk();

/**
 * Synthesise a circuit consisting of CX and TK1 gates only.
 */
TKET_EXPORT Transform synthesise_tket();

// converts a circuit into the UMD primitives (Rz, PhasedX, XXPhase) whilst
// optimising Expects: Any gate set Produces: XXPhase, PhasedX, Rz
TKET_EXPORT Transform synthesise_UMD();

/////////////////////////////
// Pauli Gadget Optimisation//
/////////////////////////////

/**
 * Depth-saving resynthesis of phase gadgets with alignment.
 *
 * Produces CX and TK1 gates.
 */
TKET_EXPORT Transform optimise_via_PhaseGadget(
    CXConfigType cx_config = CXConfigType::Snake);

}  // namespace Transforms

}  // namespace tket
