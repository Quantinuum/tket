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

#include "tket/Architecture/Architecture.hpp"
#include "tket/Circuit/Circuit.hpp"

#include "tket_export.h"

namespace tket {
/**
 * Check that the circuit respects architectural constraints
 *
 * @param circ circuit to check
 * @param arch architecture
 * @param directed if true, disallow two-qubit gates except for CX
 * @param bridge_allowed whether 3-qubit \ref OpType::BRIDGE operations are
 * allowed
 */
TKET_EXPORT bool respects_connectivity_constraints(
    const Circuit& circ, const Architecture& arch, bool directed,
    bool bridge_allowed = false);

}  // namespace tket
