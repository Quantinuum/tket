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
#include "tket_export.h"

namespace tket {

namespace Transforms {

// multi-qubit patterns that decrease the CX count
// inserting swaps can sometimes cause errors elsewhere (e.g. routing), so
// they can be turned off
TKET_EXPORT Transform multiq_clifford_replacement(bool allow_swaps = false);

// copies Z through the target of a CX and X through the control
TKET_EXPORT Transform copy_pi_through_CX();

/**
 * Apply some of the Clifford rules in the paper "Optimising Clifford Circuits
 * with Quantomatic".
 */
TKET_EXPORT Transform singleq_clifford_sweep();

// In some cases, 2-qubit gates can be saved by:
// 1) representing end-of-circuit measurements as a collection of single Z
// operators 2) transforming each fo these single Z operators to a new Pauli
// operator by acting on them with an end-of-circuit Clifford subcircuit 3)
// finding a Clifford that mutually diagonalises these Pauli operators 4)
// replacing the end-of-circuit Clifford subcircuit with the mutual
// diagonalisation circuit and adding classical logic to map the diagonalised
// Pauli operators back to the desired single Z operators
TKET_EXPORT Transform push_cliffords_through_measures();

}  // namespace Transforms

}  // namespace tket
