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

#include "CompilerPass.hpp"

#include "tket_export.h"

namespace tket {

TKET_EXPORT const PassPtr &SynthesiseTK();
TKET_EXPORT const PassPtr &SynthesiseTket();

TKET_EXPORT const PassPtr &RemoveRedundancies();
TKET_EXPORT const PassPtr &CommuteThroughMultis();
TKET_EXPORT const PassPtr &DecomposeArbitrarilyControlledGates();
// Expects: CX and any single-qubit gates,
// but does not break if it encounters others
TKET_EXPORT const PassPtr &DecomposeMultiQubitsCX();
TKET_EXPORT const PassPtr &DecomposeSingleQubitsTK1();

/**
 * Recursively replaces all boxes by their decomposition using Box::to_circuit
 * @param excluded_types box types excluded from decomposition
 * @param excluded_opgroups opgroups excluded from decomposition
 * @param included_types optional, only decompose these box types
 * @param included_opgroups optional, only decompose these opgroups
 */
TKET_EXPORT PassPtr DecomposeBoxes(
    const std::unordered_set<OpType> &excluded_types = {},
    const std::unordered_set<std::string> &excluded_opgroups = {},
    const std::optional<std::unordered_set<OpType>> &included_types =
        std::nullopt,
    const std::optional<std::unordered_set<std::string>> &included_opgroups =
        std::nullopt);

/**
 * converts a circuit containing all possible gates to a circuit containing only
 * phase poly boxes + H gates (and measure + reset + collapse + barrier)
 * @param min_size value for the minimal number of CX in each box, groups with
 * less than min_size CX gates are not converted to a PhasePolyBox, default
 * value is 0
 * @return PassPtr to perform the conversion
 */
TKET_EXPORT PassPtr ComposePhasePolyBoxes(unsigned min_size = 0);

/** Squash sequences of single-qubit gates to TK1 gates. */
TKET_EXPORT const PassPtr &SquashTK1();

/**
 * @brief Squash single qubit gates into PhasedX and Rz gates.
 * Commute Rzs to the back if possible.
 */
TKET_EXPORT const PassPtr &SquashRzPhasedX();

TKET_EXPORT const PassPtr &RebaseTket();
TKET_EXPORT const PassPtr &RebaseUFR();

/**
 * @brief Replace all SX in the circuit with Rx(1/2) and all SXdg with Rx(-1/2).
 */
TKET_EXPORT const PassPtr &RxFromSX();

TKET_EXPORT const PassPtr &DecomposeBridges();
TKET_EXPORT const PassPtr &FlattenRegisters();

/** Remove all& \ref OpType::Barrier from the circuit. */
TKET_EXPORT const PassPtr &RemoveBarriers();

/** Commutes measurements to the end of the circuit.
 * @param allow_partial Whether to allow measurements that cannot be commuted to
 * the end, and delay them as much as possible instead. If false, the pass
 * includes a @ref CommutableMeasuresPredicate precondition.
 */
TKET_EXPORT const PassPtr &DelayMeasures(bool allow_partial = false);

/**
 * Remove all operations that have no @ref OpType::Output or
 * @ref OpType::ClOutput in their causal future.
 */
TKET_EXPORT const PassPtr &RemoveDiscarded();

/**
 * Replace all measured classical maps that are followed by Measure operations
 * whose quantum output is discarded with classical operations following the
 * Measure.
 */
TKET_EXPORT const PassPtr &SimplifyMeasured();

/**
 * @brief Normalises all TK2 gates.
 *
 * TK2 gates have three angles in the interval [0, 4], but these can always
 * be normalised to be within the so-called Weyl chamber by adding single-qubit
 * gates.
 *
 * More precisely, the three angles a, b, c of TK2(a, b, c) are normalised
 * exactly when the two following conditions are met:
 *  - numerical values must be in the Weyl chamber, ie 1/2 >= a >= b >= |c|,
 *  - symbolic values must come before any numerical value in the array.
 *
 * After this pass, all TK2 angles will be normalised and the circuit will
 * satisfy `NormalisedTK2Predicate`.
 *
 * @return compilation pass to perform this transformation
 */
TKET_EXPORT const PassPtr &NormaliseTK2();

/**
 * @brief Converts ZZPhase with angle 1 or -1 to two Rz(1) gates.
 * @return compilation pass to perform this transformation
 */
TKET_EXPORT const PassPtr &ZZPhaseToRz();

/**
 * @brief Decompose CnX gates to 2-qubit gates and single qubit gates.
 *
 * For every two CnX gates, reorder their control qubits to improve
 * the chance of gate cancellation
 *
 * @return compilation pass to perform this transformation
 */
TKET_EXPORT const PassPtr &CnXPairwiseDecomposition();

/**
 * @brief Remove any implicit qubit permutation by appending SWAP gates.
 *
 * Note that if the circuit contains measurements, they may become mid-circuit
 * measurements in the transformed circuit.
 *
 * @return compilation pass to perform this transformation
 */
TKET_EXPORT const PassPtr &RemoveImplicitQubitPermutation();

/**
 * Attempt to optimise the circuit by simplifying in ZX calculus and extracting
 * a circuit back out.
 *
 * Due to limitations in extraction, will not work if the circuit contains
 * created or discarded qubits.
 *
 * As a resynthesis pass, this will ignore almost all optimisations achieved
 * beforehand and may increase the cost of the circuit.
 *
 * @param allow_swaps Determines whether the extracted circuit may have
 * implicit wire swaps. If set to false, implicit wire swaps will be removed
 * by adding SWAP gates at the end of the circuit.
 *
 * @return compilation pass to perform this transformation
 */
TKET_EXPORT const PassPtr &ZXGraphlikeOptimisation(bool allow_swaps = true);

/** Remove all \ref OpType::Phase (including conditionals) from the circuit. */
TKET_EXPORT const PassPtr &RemovePhaseOps();

}  // namespace tket
