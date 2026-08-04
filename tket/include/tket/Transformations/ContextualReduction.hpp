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
#include "tket/Circuit/Circuit.hpp"
#include "tket_export.h"

namespace tket {

namespace Transforms {

/** Allow insertion of classical operations when simplifying? */
enum class AllowClassical { Yes, No };

/** Automatically create all qubits in zero state when simplifying? */
enum class CreateAllQubits { Yes, No };

/**
 * Truncate the circuit so that the last operation on every (non-empty)
 * classical wire is a Measure, and return a classical postprocessing circuit as
 * well as the truncated circuit.
 *
 * The sequential composition of the two circuits returned is equivalent to the
 * original circuit.
 *
 * @param circ circuit to separate
 *
 * @return a pair of circuits (C0, C1) where C0 has no operations following the
 * final Measure on every classical wire; C1 consists of purely classical
 * operations on the same classical units; and C0 followed by C1 is equivalent
 * to the original circuit.
 */
TKET_EXPORT std::pair<Circuit, Circuit> separate_classical(const Circuit &circ);

/**
 * Remove all operations that have no @ref OpType::Output or
 * @ref OpType::ClOutput in their causal future.
 */
Transform remove_discarded_ops();

/**
 * Simplify the circuit where it acts on known basis states.
 *
 * Whenever a gate transforms a known basis state to another known basis
 * state, remove it, inserting X gates where necessary to achieve the same
 * state. Beginning with Create and Reset vertices, move forward through the
 * circuit as far as we can in this way.
 *
 * Global phase is ignored.
 *
 * @param allow_classical if allowed, insert SetBits operations in place of
 *  Measure operations that act on known basis states
 * @param create_all_qubits if enabled, annotate all qubits as initialized
 *  to zero as part of the transform, before applying simplification
 * @param xcirc 1-qubit circuit implementing an X gate (if null, an X gate
 *  is used)
 */
Transform simplify_initial(
    AllowClassical allow_classical = AllowClassical::Yes,
    CreateAllQubits create_all_qubits = CreateAllQubits::No,
    std::shared_ptr<const Circuit> xcirc = 0);

/**
 * Commute classical maps through measurements.
 *
 * A "classical map" is a pure quantum operation that acts as a permutation
 * of the computational basis states composed with a diagonal operator.
 *
 * A "measured classical map" is a classical map whose succeeding vertices
 * in the DAG are all @ref OpType::Measure.
 *
 * This transform replaces all measured classical maps that are followed by
 * Measure operations whose quantum output is discarded with classical
 * operations following the Measure.
 *
 * The process is repeated until no such replacements are possible.
 *
 * Global phase is not preserved.
 */
Transform simplify_measured();

}  // namespace Transforms

}  // namespace tket
