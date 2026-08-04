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

// decomposes multiq gates not in the gate set to CXs, then replaces CXs with
// the replacement (if CX is not allowed) then converts singleq gates no in
// the gate set to U3 and replaces them using provided function Expects: any
// gates Produces: gates in allowed_gates
Transform rebase_factory(
    const OpTypeSet& allowed_gates, const Circuit& cx_replacement,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk1_replacement);

Transform rebase_factory_via_tk2(
    const OpTypeSet& allowed_gates,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk1_replacement,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk2_replacement);

// Multiqs: CX
// Singleqs: TK1
TKET_EXPORT Transform rebase_tket();

// Multiqs: CZ
// Singleqs: PhasedX, Rz
TKET_EXPORT Transform rebase_cirq();

// Multiqs: CZ
// Singleqs: Rx, Rz
TKET_EXPORT Transform rebase_quil();

// Multiqs: SWAP, CX, CZ
// Singleqs: H, X, Z, S, T, Rx, Rz
TKET_EXPORT Transform rebase_pyzx();

// Multiqs: SWAP, CRz, CX, CZ
// Singleqs: H, X, Y, Z, S, T, V, Rx, Ry, Rz
TKET_EXPORT Transform rebase_projectq();

// Used for UniversalFrameRandomisation
// Multiqs: CX
// Singleqs: Rz, H
TKET_EXPORT Transform rebase_UFR();

// Multiqs: ZZMax
// Singleqs: Rz, PhasedX
TKET_EXPORT Transform rebase_HQS();

// Multiqs: TK2
// Singleqs: TK1
TKET_EXPORT Transform rebase_TK();

// Multiqs: XXPhase
// Singleqs: Rz, PhasedX
TKET_EXPORT Transform rebase_UMD();

// Multiqs: AAMS
// Singleqs: GPI, GPI2
TKET_EXPORT Transform rebase_ionq();

}  // namespace Transforms

}  // namespace tket
