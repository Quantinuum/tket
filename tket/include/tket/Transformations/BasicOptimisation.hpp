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
#include "tket/Characterisation/ErrorTypes.hpp"

namespace tket {

namespace Transforms {

// removes gate-inverse pairs, merges rotations, removes identity rotations,
// and removes redundant gates before measure Expects: Any gates Produces: The
// same gate set
bool redundancy_removal(Circuit& circuit);
Transform remove_redundancies();

/**
 * Squash all single-qubit gates to TK1.
 */
Transform squash_1qb_to_tk1();

// moves single qubit operations past multiqubit operations they commute with,
// towards front of circuit (hardcoded)
// Expects: Any gates
// Produces: Any gates
Transform commute_through_multis();

// squashes each sequence of two-qubit instructions into their canonical 3-CX
// form (Cartan/KAK decomposition) The optional CX gate fidelity cx_fidelity
// is used to produce circuit decompositions that maximise expected circuit
// fidelity, assuming perfect local operations (i.e. with fidelity 1.).
// Expects: CX and any single qubit gates
// Produces: CX and single TK1 qubit gates

/**
 * @brief Squash sequences of two-qubit operations into minimal form.
 *
 * Squash together sequences of single- and two-qubit gates
 * into minimal form. Can decompose to TK2 or CX gates.
 *
 * Two-qubit operations can always be expressed in a minimal form of
 * maximum three CXs, or as a single TK2 gate (a result also known
 * as the KAK or Cartan decomposition).
 *
 * It is in general recommended to squash to TK2 gates, and to then use the
 * `DecomposeTK2` pass for noise-aware decompositions to other gatesets.
 * For backward compatibility, decompositions to CX are also supported. In this
 * case, `cx_fidelity` can be provided to perform approximate decompositions to
 * CX.
 *
 * When decomposing to TK2 gates, any sequence of two or more two-qubit gates
 * on the same set of qubits are replaced by a single TK2 gate. When decomposing
 * to CX, the substitution is only performed if it results in a reduction of the
 * number of CX gates, or if at least one of the two-qubit gates is not a CX.
 *
 * Using the `allow_swaps=true` (default) option, qubits may be swapped when
 * convenient to further reduce the two-qubit gate count.
 *
 * @param target_2qb_gate OpType to decompose to. Either TK2 or CX.
 * @param cx_fidelity Estimated CX gate fidelity, used when target_2qb_gate=CX.
 * @param allow_swaps Whether to allow implicit wire swaps.
 * @return Transform
 */
Transform two_qubit_squash(
    OpType target_2qb_gate = OpType::CX, double cx_fidelity = 1.,
    bool allow_swaps = true);
Transform two_qubit_squash(bool allow_swaps);

/**
 * general u_squash by converting any chains of p, q gates (p, q in
 * {Rx,Ry,Rz}) to triples -p-q-p- or -q-p-q-
 *
 * if strict = false (default), then the chains will be squashed to either
 * -p-q-p- or -q-p-q-, and the third rotation will be commuted through the
 * next gate whenever possible if strict = true, then all chains will be
 * squashed to -p-q-p-
 *
 * Expects: p, q, and any multi-qubit gates
 * Produces: p, q, and any multi-qubit gates
 */
Transform squash_1qb_to_pqp(
    const OpType& q, const OpType& p, bool strict = false);

// identifies single-qubit chains and squashes them in the target gate set
// Expects: any gates
// Produces: singleqs and any multi-qubit gates
Transform squash_factory(
    const OpTypeSet& singleqs,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk1_replacement,
    bool always_squash_symbols = false);

// commutes single qubit gates through SWAP gates, leaving them on the
// PhysicalQubit with best fidelity for the given OP Expects: any single qubit
// gates, SWAP gates Produces: any single qubit gates, SWAP gates
Transform commute_SQ_gates_through_SWAPS(const avg_node_errors_t& node_errors);
Transform commute_SQ_gates_through_SWAPS(const op_node_errors_t& node_errors);

/**
 * @brief Absorb Rz gates into NPhasedX where possible
 *
 * For any gate NPhasedX(𝛼, 𝛽) in the circuit, try to change 𝛽 to reduce the
 * number of surrounding Rz gates.
 */
Transform absorb_Rz_NPhasedX();

/**
 * @brief Converts any ZZPhase with angle in (-1, 1) to two pi Rz gates.
 */
Transform ZZPhase_to_Rz();

/**
 * @brief Normalises all TK2 gates so that `NormalisedTK2Predicate` is
 * satisfied.
 */
Transform normalise_TK2();

/**
 * @brief Squash single qubit gates into PhasedX and Rz gates.
 *
 * Commute Rzs to the back if possible.
 */
Transform squash_1qb_to_Rz_PhasedX();

/**
 * Generate a transform that rounds all angles to the nearest \f$ \pi / 2^n \f$.
 *
 * @param n precision to retain in angles
 * @param only_zeros only set angles smaller than \f$ \pi / 2^{n+1} \f$ to zero
 *
 * @pre n < 32
 *
 * @return Transform to perform rounding
 */
Transform round_angles(unsigned n, bool only_zeros = false);

}  // namespace Transforms

}  // namespace tket
