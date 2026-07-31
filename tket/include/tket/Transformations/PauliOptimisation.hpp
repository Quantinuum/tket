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

/* Dictates whether synthesis of a PauliGraph should
    be done on the Paulis individually, making use of the pairwise
    interactions or collecting into mutually commuting sets.
    The additional greedy strategy applies two-qubit Clifford gates
    in a greedy fashion and delays the un-computation to the very end.
*/
enum class PauliSynthStrat { Individual, Pairwise, Sets, Greedy };

NLOHMANN_JSON_SERIALIZE_ENUM(
    PauliSynthStrat, {{PauliSynthStrat::Individual, "Individual"},
                      {PauliSynthStrat::Pairwise, "Pairwise"},
                      {PauliSynthStrat::Sets, "Sets"},
                      {PauliSynthStrat::Greedy, "Greedy"}});

TKET_EXPORT Transform pairwise_pauli_gadgets(CXConfigType cx_config = CXConfigType::Snake);

// always returns true, as it leaves Circuit data structure
TKET_EXPORT Transform synthesise_pauli_graph(
    PauliSynthStrat strat = PauliSynthStrat::Sets,
    CXConfigType cx_config = CXConfigType::Snake);

// Assumes incoming circuit is composed of `CircBox`es with
// `PauliExpBox`es inside
TKET_EXPORT Transform special_UCC_synthesis(
    PauliSynthStrat strat = PauliSynthStrat::Sets,
    CXConfigType cx_config = CXConfigType::Snake);

}  // namespace Transforms

}  // namespace tket
