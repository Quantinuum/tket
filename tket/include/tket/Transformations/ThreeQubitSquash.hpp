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
#include "tket/OpType/OpType.hpp"

#include "tket_export.h"

namespace tket {

namespace Transforms {

/**
 * Squash sequences of 3-qubit instructions into a canonical form.
 *
 * The circuit should comprise only 1- and 2-qubit gates, with the 2-qubit gates
 * being either all CX or all TK2; this is also the target gate. The transform
 * will only squash subcircuits that reduce the count of the relevant 2-qubit
 * gate.
 *
 * @param target_2qb_gate Target 2-qubit gate (either CX or TK2)
 * @return Transform implementing the squash
 */
TKET_EXPORT Transform three_qubit_squash(OpType target_2qb_gate = OpType::CX);

}  // namespace Transforms

}  // namespace tket
